/****************************************************************************
*																			*
*					  cryptlib PKCS #11 Item Read Routines					*
*						Copyright Peter Gutmann 1998-2009					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "device.h"
  #include "pkcs11_api.h"
  #include "asn1.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "device/device.h"
  #include "device/pkcs11_api.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

/* In some rare situations only incomplete PKCS #11 support is available in 
   the underlying device, for example with a token that's been initialised 
   via CryptoAPI (which doesn't populate all the PKCS #11 fields) but which 
   is now being accessed through a PKCS #11 driver.  Define the following
   to have cryptlib perform an object search by enumerating every object of 
   the required type in the token and matching by the requested ID 
   information */

#define PKCS11_FIND_VIA_CRYPTLIB

#ifdef USE_PKCS11

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Get a PKCS #11 attribute value.  If the passed-in buffer is large enough
   to contain the value (it almost always is) then we use that, otherwise
   we dynamically allocate the storage for it, which is why the caller has
   to call an explicit cleanup function when they're finished with the 
   data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5, 6 ) ) \
static int getAttributeValue( INOUT PKCS11_INFO *pkcs11Info, 
							  const CK_OBJECT_HANDLE hObject, 
							  const CK_ATTRIBUTE_TYPE type,
							  OUT_BUFFER_ALLOC_OPT( *valueLength ) void **value, 
							  OUT_LENGTH_SHORT_Z int *valueLength,
							  INOUT_BUFFER( valueBufLength, *valueLength ) \
									void *valueBuffer,
							  IN_LENGTH_SHORT_MIN( 16 ) const int valueBufLength )
	{
	CK_ATTRIBUTE valueTemplate = \
		{ type, NULL_PTR, 0 };
	CK_RV status;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( value, sizeof( void * ) ) );
	assert( isWritePtr( valueLength, sizeof( int ) ) );
	assert( isWritePtr( valueBuffer, valueBufLength ) );

	REQUIRES( valueBufLength >= 16 && valueBufLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*value = NULL;
	*valueLength = 0;

	/* Find out how big the attribute value is */
	status = C_GetAttributeValue( pkcs11Info->hSession, hObject,
								  &valueTemplate, 1 );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_NOTFOUND ) );

	/* If it's larger than the supplied buffer, allocate the storage 
	   dynamically */
	if( valueTemplate.ulValueLen > valueBufLength )
		{
		if( valueTemplate.ulValueLen >= MAX_INTLENGTH_SHORT )
			return( CRYPT_ERROR_OVERFLOW );
		if( ( valueTemplate.pValue = clAlloc( "getAttributeValue", \
					( size_t ) ( valueTemplate.ulValueLen ) ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		}
	else
		valueTemplate.pValue = valueBuffer;

	/* Get the attribute value */
	status = C_GetAttributeValue( pkcs11Info->hSession, hObject,
								  &valueTemplate, 1 );
	if( status != CKR_OK )
		{
		if( valueTemplate.pValue != valueBuffer )
			clFree( "getAttributeValue", valueTemplate.pValue );
		return( pkcs11MapError( status, CRYPT_ERROR_NOTFOUND ) );
		}
	*value = valueTemplate.pValue;
	*valueLength = valueTemplate.ulValueLen;

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void getAttributeValueEnd( IN void *value, 
								  const void *valueBuffer )
	{
	assert( isReadPtr( value, 16 ) );
	assert( isReadPtr( valueBuffer, 16 ) );

	if( value != valueBuffer )
		clFree( "getAttributeValueEnd", value );
	}

/* Get the label for an object, truncating overly long labels if required.  
   We can't use a dynBuf for this because it's a PKCS #11 attribute rather 
   than a cryptlib attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int getObjectLabel( INOUT PKCS11_INFO *pkcs11Info, 
						   const CK_OBJECT_HANDLE hObject, 
						   OUT_BUFFER( maxLabelSize, *labelLength ) char *label, 
						   IN_LENGTH_SHORT_MIN( 16 ) const int maxLabelSize, 
						   OUT_LENGTH_SHORT_Z int *labelLength )
	{
	char labelBuffer[ CRYPT_MAX_TEXTSIZE + 8 ], *localLabel;
	int localLabelLength, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( label, maxLabelSize ) );
	assert( isWritePtr( labelLength, sizeof( int ) ) );

	REQUIRES( maxLabelSize >= 16 && maxLabelSize < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( label, 0, maxLabelSize );
	*labelLength = 0;

	cryptStatus = getAttributeValue( pkcs11Info, hObject, CKA_LABEL, 
									 &localLabel, &localLabelLength, 
									 labelBuffer, CRYPT_MAX_TEXTSIZE );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	*labelLength = min( localLabelLength, maxLabelSize );
	REQUIRES( *labelLength > 0 && *labelLength < MAX_INTLENGTH_SHORT );
	memcpy( label, localLabel, *labelLength );
	getAttributeValueEnd( localLabel, labelBuffer );

	return( CRYPT_OK );
	}

/* Read a flag for an object.  An absent value is treated as FALSE */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN readFlag( const PKCS11_INFO *pkcs11Info, 
						 const CK_OBJECT_HANDLE hObject,
						 const CK_ATTRIBUTE_TYPE flagType )
	{
	CK_BBOOL bFlag = FALSE;
	CK_ATTRIBUTE flagTemplate = { flagType, &bFlag, sizeof( CK_BBOOL ) };

	assert( isReadPtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );

	/* Some buggy implementations return CKR_OK but forget to set the
	   data value in the template (!!!) so we have to initialise bFlag
	   to a default of FALSE to handle this */
	return( ( C_GetAttributeValue( pkcs11Info->hSession, hObject,
								   &flagTemplate, 1 ) == CKR_OK && bFlag ) ? \
			TRUE : FALSE );
	}

/* Get the permitted-action flags for an object */

CHECK_RETVAL_RANGE( ACTION_PERM_FLAG_NONE, ACTION_PERM_FLAG_MAX ) STDC_NONNULL_ARG( ( 1 ) ) \
int getActionFlags( INOUT PKCS11_INFO *pkcs11Info,
					const CK_OBJECT_HANDLE hObject,
					IN_ENUM( KEYMGMT_ITEM ) const KEYMGMT_ITEM_TYPE itemType,
					IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	const BOOLEAN checkSign = ( isSigAlgo( cryptAlgo ) || \
								isMacAlgo( cryptAlgo ) ) ? \
							  TRUE : FALSE;
	const BOOLEAN checkCrypt = ( isCryptAlgo( cryptAlgo ) || \
								 isConvAlgo( cryptAlgo ) ) ? \
							   TRUE : FALSE;
	const BOOLEAN checkWrap = isCryptAlgo( cryptAlgo );
	BOOLEAN cryptAllowed = FALSE, sigAllowed = FALSE;
	int actionFlags = 0;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );

	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY || \
			  itemType == KEYMGMT_ITEM_SECRETKEY );
	REQUIRES( cryptAlgo >= CRYPT_ALGO_FIRST_CONVENTIONAL && \
			  cryptAlgo <= CRYPT_ALGO_LAST_MAC ); 

	/* Get the permitted actions for the object.  Some devices report bogus 
	   capabilities (for example encrypt for a MAC object) so we restrict 
	   the actions that we check for to try and weed out false positives.  
	   The kernel won't allow the setting of an invalid action anyway, but 
	   it's better to be safe here.
	   
	   We also have to provide special translation for the sign and sig-
	   check action flags, PKCS #11 treats the MAC operation as a member
	   of the signature family while cryptlib treats it as a member of the
	   hash family so if we get a sign/sigcheck permitted action for a MAC 
	   object we map it to a hash permitted action */
	if( ( checkCrypt && readFlag( pkcs11Info, hObject, CKA_ENCRYPT ) ) || \
		( checkWrap && readFlag( pkcs11Info, hObject, CKA_WRAP ) ) )
		{
		actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_ALL );
		cryptAllowed = TRUE;
		}
	if( ( checkCrypt && itemType != KEYMGMT_ITEM_PUBLICKEY && \
		  readFlag( pkcs11Info, hObject, CKA_DECRYPT ) ) || \
		( checkWrap && itemType == KEYMGMT_ITEM_PRIVATEKEY && \
		  readFlag( pkcs11Info, hObject, CKA_UNWRAP ) ) )
		{
		actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_ALL );
		cryptAllowed = TRUE;
		}
	if( checkSign && itemType != KEYMGMT_ITEM_PUBLICKEY && \
		readFlag( pkcs11Info, hObject, CKA_SIGN ) )
		{
		if( isMacAlgo( cryptAlgo ) )
			actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_HASH, ACTION_PERM_ALL );
		else
			actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGN, ACTION_PERM_ALL );
		sigAllowed = TRUE;
		}
	if( checkSign && readFlag( pkcs11Info, hObject, CKA_VERIFY ) )
		{
		if( isMacAlgo( cryptAlgo ) )
			actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_HASH, ACTION_PERM_ALL );
		else
			actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_ALL );
		sigAllowed = TRUE;
		}
	if( cryptAlgo == CRYPT_ALGO_RSA )
		{
		/* If there are any restrictions on the key usage then we have to 
		   make it internal-only because of RSA's signature/encryption 
		   duality */
		if( !( cryptAllowed && sigAllowed ) )
			actionFlags = MK_ACTION_PERM_NONE_EXTERNAL( actionFlags );
		}
	else
		{
		if( isDlpAlgo( cryptAlgo ) || isEccAlgo( cryptAlgo ) )
			{
			/* Because of the special-case data formatting requirements for 
			   DLP/ECDLP algorithms we make the usage internal-only */
			actionFlags = MK_ACTION_PERM_NONE_EXTERNAL( actionFlags );
			}
		}

	return( actionFlags );
	}

/* Get cryptlib algorithm and capability information for a PKCS #11 object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5, 6 ) ) \
static int getMechanismInfo( const PKCS11_INFO *pkcs11Info, 
							 const CK_OBJECT_HANDLE hObject,
							 const void *capabilityInfoList, 
							 const BOOLEAN isPKC,
							 const CAPABILITY_INFO **capabilityInfoPtrPtr,
							 OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo )
	{
	CK_KEY_TYPE keyType = DUMMY_INIT;
	CK_ATTRIBUTE keyTypeTemplate = \
		{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &keyType, sizeof( CK_KEY_TYPE ) };
	CK_RV status;
	const CAPABILITY_INFO *capabilityInfoPtr;
	const PKCS11_MECHANISM_INFO *mechanismInfoPtr;
	int mechanismInfoSize, i;

	assert( isReadPtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isReadPtr( capabilityInfoPtrPtr, sizeof( CAPABILITY_INFO ) ) );
	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );

	REQUIRES( capabilityInfoList != NULL );

	/* Clear return values */
	*capabilityInfoPtrPtr = NULL;
	*cryptAlgo = CRYPT_ALGO_NONE;

	/* Get the key type (equivalent to the cryptlib algoID) for this 
	   object */
	status = C_GetAttributeValue( pkcs11Info->hSession, hObject, 
								  &keyTypeTemplate, 1 );
	if( status != CKR_OK )
		{
		DEBUG_DIAG(( "Couldn't read CKA_KEY_TYPE" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_FAILED );
		}

	/* Hack for PKCS #11's broken HMAC "support", PKCS #11 has no HMAC 
	   object types so if we find a generic secret key object we assume that 
	   it's an HMAC-SHA1 object, the most common type */
	if( keyType == CKK_GENERIC_SECRET )
		{
		*cryptAlgo = CRYPT_ALGO_HMAC_SHA1;
		capabilityInfoPtr = findCapabilityInfo( capabilityInfoList, 
												*cryptAlgo );
		if( capabilityInfoPtr == NULL )
			return( CRYPT_ERROR_NOTAVAIL );
		*capabilityInfoPtrPtr = capabilityInfoPtr;

		return( CRYPT_OK );
		}

	/* Get the equivalent cryptlib algorithm type and use that to get the
	   capability information for the algorithm */
	if( isPKC )
		mechanismInfoPtr = getMechanismInfoPKC( &mechanismInfoSize );
	else
		mechanismInfoPtr = getMechanismInfoConv( &mechanismInfoSize );
	for( i = 0; i < mechanismInfoSize && \
				mechanismInfoPtr[ i ].keyType != keyType; i++ );
	ENSURES( i < mechanismInfoSize );
	mechanismInfoPtr = &mechanismInfoPtr[ i ];
	*cryptAlgo = mechanismInfoPtr->cryptAlgo;
	capabilityInfoPtr = findCapabilityInfo( capabilityInfoList, *cryptAlgo );
	if( capabilityInfoPtr == NULL )
		return( CRYPT_ERROR_NOTAVAIL );
	*capabilityInfoPtrPtr = capabilityInfoPtr;
	
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 Template Manipulation Routines						*
*																			*
****************************************************************************/

/* Add the components of an issuerAndSerialnumber to a certificate 
   template */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readIAndS( INOUT STREAM *stream,
					  INOUT CK_ATTRIBUTE *certTemplateI, 
					  INOUT CK_ATTRIBUTE *certTemplateS )
	{
	void *dataPtr = DUMMY_INIT_PTR;
	int length, cryptStatus;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( certTemplateI, sizeof( CK_ATTRIBUTE ) ) );
	assert( isWritePtr( certTemplateS, sizeof( CK_ATTRIBUTE ) ) );

	/* We don't clear the return values since these have already been 
	   partially initialised with attribute information by the caller */

	/* Read the wrapper tag */
	cryptStatus = readSequence( stream, NULL );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Read the issuer DN and add it to the template */
	cryptStatus = getStreamObjectLength( stream, &length );
	if( cryptStatusOK( cryptStatus ) )
		{
		certTemplateI->ulValueLen = length;
		cryptStatus = sMemGetDataBlock( stream, &dataPtr, length );
		}
	if( cryptStatusOK( cryptStatus ) )
		{
		certTemplateI->pValue = dataPtr;
		cryptStatus = sSkip( stream, length );
		}
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Read the serial number and add it to the template */
	cryptStatus = getStreamObjectLength( stream, &length );
	if( cryptStatusOK( cryptStatus ) )
		{
		certTemplateS->ulValueLen = length;
		cryptStatus = sMemGetDataBlock( stream, &dataPtr, length );
		}
	if( cryptStatusOK( cryptStatus ) )
		{
		certTemplateS->pValue = dataPtr;
		cryptStatus = sSkip( stream, length );
		}
	return( cryptStatus );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int addIAndSToTemplate( INOUT_ARRAY_C( 2 ) CK_ATTRIBUTE *certTemplate, 
						IN_BUFFER( iAndSLength ) const void *iAndSPtr, 
						IN_LENGTH_SHORT const int iAndSLength )
	{
	STREAM stream;
	int cryptStatus;

	assert( isWritePtr( certTemplate, sizeof( CK_ATTRIBUTE ) * 2 ) );
	assert( isReadPtr( iAndSPtr, iAndSLength ) );

	REQUIRES( iAndSLength > 0 && iAndSLength < MAX_INTLENGTH_SHORT );

	/* We don't clear the return value since this has already been 
	   partially initialised with attribute information by the caller */

	/* Parse the iAndS data and add it to the template */
	sMemConnect( &stream, iAndSPtr, iAndSLength );
	cryptStatus = readIAndS( &stream, &certTemplate[ 0 ], 
							 &certTemplate[ 1 ] );
	sMemDisconnect( &stream );
	return( cryptStatus );
	}

/* Set up a search template for an issuerAndSerialNumber.  Because Netscape 
   incorrectly used the raw serial number instead of the DER-encoded form 
   and other applications copied this, we also set up an alternative 
   template with the serial number in this alternative form that we fall 
   back to if a search using the correct form fails */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int initIAndSTemplate( OUT_ARRAY_C( 4 ) CK_ATTRIBUTE *iAndSTemplate,
							  OUT_ARRAY_C( 4 ) CK_ATTRIBUTE *iAndSTemplateAlt,
							  IN_BUFFER( keyIDlength ) const void *keyID, 
							  IN_LENGTH_KEYID const int keyIDlength )
	{
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	static const CK_ATTRIBUTE initTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		{ CKA_ISSUER, NULL_PTR, 0 },
		{ CKA_SERIAL_NUMBER, NULL_PTR, 0 }
		};
	STREAM stream;
	int offset = DUMMY_INIT, length, cryptStatus;

	assert( isWritePtr( iAndSTemplate, 4 * sizeof( CK_ATTRIBUTE ) ) );
	assert( isWritePtr( iAndSTemplateAlt, 4 * sizeof( CK_ATTRIBUTE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	/* Set up the issuerAndSerialNumber template */
	memcpy( iAndSTemplate, initTemplate, 4 * sizeof( CK_ATTRIBUTE ) );
	cryptStatus = addIAndSToTemplate( &iAndSTemplate[ 2 ], keyID, 
									  keyIDlength );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Set up the alternate template to the same as the main template, but 
	   with the tag and length stripped from the serial number */
	memcpy( iAndSTemplateAlt, iAndSTemplate, 4 * sizeof( CK_ATTRIBUTE ) );
	sMemConnect( &stream, iAndSTemplateAlt[ 3 ].pValue, 
				 iAndSTemplateAlt[ 3 ].ulValueLen );
	cryptStatus = readGenericHole( &stream, &length, 1, BER_INTEGER );
	if( cryptStatusOK( cryptStatus ) )
		offset = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	iAndSTemplateAlt[ 3 ].pValue = \
				( BYTE * ) iAndSTemplateAlt[ 3 ].pValue + offset;
	iAndSTemplateAlt[ 3 ].ulValueLen = length;
	
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Certificate Import Routines						*
*																			*
****************************************************************************/

/* Instantiate a certificate object from a PKCS #11 object handle */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int instantiateCert( INOUT PKCS11_INFO *pkcs11Info, 
							const CK_OBJECT_HANDLE hCertificate, 
							OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
							const BOOLEAN createContext )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	BYTE buffer[ MAX_BUFFER_SIZE + 8 ], *bufPtr;
	int length, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) );

	/* Clear return value */
	*iCryptCert = CRYPT_ERROR;

	/* Fetch the certificate data into local memory.  We can't use a dynBuf 
	   for this because it's a PKCS #11 attribute rather than a cryptlib 
	   attribute */
	cryptStatus = getAttributeValue( pkcs11Info, hCertificate, CKA_VALUE, 
									 &bufPtr, &length, buffer, 
									 MAX_BUFFER_SIZE );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Import the certificate as a cryptlib object */
	setMessageCreateObjectIndirectInfo( &createInfo, bufPtr, length,
										createContext ? \
											CRYPT_CERTTYPE_CERTIFICATE : \
											CRYPT_ICERTTYPE_DATAONLY );
	cryptStatus = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								   IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
								   &createInfo, OBJECT_TYPE_CERTIFICATE );
	getAttributeValueEnd( bufPtr, buffer );
	if( cryptStatusOK( cryptStatus ) )
		*iCryptCert = createInfo.cryptHandle;
	return( cryptStatus );
	}

/* Get a certificate chain from a device */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int getCertChain( INOUT PKCS11_INFO *pkcs11Info, 
						 IN_HANDLE const CRYPT_DEVICE iCertSource, 
						 const CK_OBJECT_HANDLE hCertificate, 
						 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert, 
						 const BOOLEAN createContext )
	{
	CK_ATTRIBUTE idTemplate = \
		{ CKA_ID, NULL_PTR, 0 };
	CK_RV status;
	BYTE keyID[ MAX_BUFFER_SIZE + 8 ];

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( isHandleRangeValid( iCertSource ) );

	/* Clear return value */
	*iCryptCert = CRYPT_ERROR;

	/* Find the ID for this certificate */
	status = C_GetAttributeValue( pkcs11Info->hSession, hCertificate, 
								  &idTemplate, 1 );
	if( status == CKR_OK && idTemplate.ulValueLen <= MAX_BUFFER_SIZE )
		{
		idTemplate.pValue = keyID;
		status = C_GetAttributeValue( pkcs11Info->hSession, hCertificate,
									  &idTemplate, 1 );
		}
	if( status != CKR_OK || idTemplate.ulValueLen > MAX_BUFFER_SIZE )
		{
		/* We couldn't get the ID to build the chain or it's too large to be
		   usable, we can at least still return the individual certificate */
		return( instantiateCert( pkcs11Info, hCertificate, iCryptCert, 
								 createContext ) );
		}

	/* Create the certificate chain via an indirect import */
	return( iCryptImportCertIndirect( iCryptCert, iCertSource, 
							CRYPT_IKEYID_KEYID, keyID, idTemplate.ulValueLen, 
							createContext ? KEYMGMT_FLAG_DATAONLY_CERT : 0 ) );
	}

/****************************************************************************
*																			*
*						 		Find-Item Routines							*
*																			*
****************************************************************************/

/* Find an object based on a given template.  There are two variations of 
   this, one that finds one and only one object and the other that returns 
   the first object that it finds without treating the presence of multiple 
   objects as an error.
   
   The way in which this call works has special significance, there are PKCS
   #11 implementations that don't allow any other calls during the init/find/
   final sequence so the code is structured to always call them one after 
   the other without any intervening calls.  In addition some drivers are
   confused over whether they're 1.x or 2.x and may or may not implement
   C_FindObjectsFinal().  Because of this we call it if it exists, if it 
   doesn't then we assume that the driver can handle cleanup itself (this 
   situation shouldn't occur because we've checked for 1.x drivers earlier, 
   but there are one or two drivers where it does happen) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int findDeviceObjects( INOUT PKCS11_INFO *pkcs11Info, 
							  OUT CK_OBJECT_HANDLE *hObject,
							  IN_ARRAY( templateCount ) \
								const CK_ATTRIBUTE *objectTemplate,
							  IN_RANGE( 1, 64 ) const CK_ULONG templateCount,
							  const BOOLEAN onlyOne )
	{
	CK_OBJECT_HANDLE hObjectArray[ 2 + 8 ];
	CK_ULONG ulObjectCount;
	CK_RV status;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( hObject, sizeof( CK_OBJECT_HANDLE  ) ) );
	assert( isReadPtr( objectTemplate, \
					   sizeof( CK_ATTRIBUTE ) * templateCount ) );
	
	REQUIRES( templateCount >= 1 && templateCount <= 64 );

	/* Clear return value */
	*hObject = CK_OBJECT_NONE;

	status = C_FindObjectsInit( pkcs11Info->hSession,
								( CK_ATTRIBUTE_PTR ) objectTemplate,
								templateCount );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_NOTFOUND ) );
	status = C_FindObjects( pkcs11Info->hSession, hObjectArray, 2, 
							&ulObjectCount );
	if( C_FindObjectsFinal != NULL )
		C_FindObjectsFinal( pkcs11Info->hSession );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_NOTFOUND ) );
	if( ulObjectCount <= 0 )
		return( CRYPT_ERROR_NOTFOUND );
	if( ulObjectCount > 1 && onlyOne )
		return( CRYPT_ERROR_DUPLICATE );
	*hObject = hObjectArray[ 0 ];

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int findObject( INOUT PKCS11_INFO *pkcs11Info, 
				OUT CK_OBJECT_HANDLE *hObject,
				IN_ARRAY( templateCount ) \
					const CK_ATTRIBUTE *objectTemplate,
				IN_RANGE( 1, 64 ) const CK_ULONG templateCount )
	{
	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( hObject, sizeof( CK_OBJECT_HANDLE  ) ) );
	assert( isReadPtr( objectTemplate, \
					   sizeof( CK_ATTRIBUTE ) * templateCount ) );

	REQUIRES( templateCount >= 1 && templateCount <= 64 );

	return( findDeviceObjects( pkcs11Info, hObject, 
							   objectTemplate, templateCount, TRUE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int findObjectEx( INOUT PKCS11_INFO *pkcs11Info, 
				  OUT CK_OBJECT_HANDLE *hObject,
				  IN_ARRAY( templateCount ) \
					const CK_ATTRIBUTE *objectTemplate,
				  IN_RANGE( 1, 64 ) const CK_ULONG templateCount )
	{
	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( hObject, sizeof( CK_OBJECT_HANDLE  ) ) );
	assert( isReadPtr( objectTemplate, \
					   sizeof( CK_ATTRIBUTE ) * templateCount ) );

	REQUIRES( templateCount >= 1 && templateCount <= 64 );

	return( findDeviceObjects( pkcs11Info, hObject, 
							   objectTemplate, templateCount, FALSE ) );
	}

/* Find an object from a source object by matching IDs.  This is used to
   find a key matching a certificate, a public key matching a private key, 
   or other objects with similar relationships */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int findObjectFromObject( INOUT PKCS11_INFO *pkcs11Info,
						  const CK_OBJECT_HANDLE hSourceObject, 
						  const CK_OBJECT_CLASS objectClass,
						  OUT CK_OBJECT_HANDLE *hObject )
	{
	CK_ATTRIBUTE keyTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &objectClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_ID, NULL_PTR, 0 }
		};
	BYTE buffer[ MAX_BUFFER_SIZE + 8 ], *bufPtr;
	int length, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( hObject, sizeof( CK_OBJECT_HANDLE ) ) );

	/* Clear return value */
	*hObject = CK_OBJECT_NONE;

	/* We're looking for a key whose ID matches that of the source object, 
	   read its certificate ID.  We can't use a dynBuf for this because it's 
	   a PKCS #11 attribute rather than a cryptlib attribute */
	cryptStatus = getAttributeValue( pkcs11Info, hSourceObject, CKA_ID, 
									 &bufPtr, &length, buffer, 
									 MAX_BUFFER_SIZE );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Find the key object with the given ID */
	keyTemplate[ 1 ].pValue = bufPtr;
	keyTemplate[ 1 ].ulValueLen = length;
	cryptStatus = findObject( pkcs11Info, hObject, keyTemplate, 2 );
	getAttributeValueEnd( bufPtr, buffer );

	return( cryptStatus );
	}

/* Find a certificate object based on various search criteria:
   
	- Find a certificate matching a supplied template - certFromTemplate()
	- Find a certificate matching a given label - certFromLabel()
	- Find a certificate matching a given ID - certFromID()
	- Find a certificate matching the ID of an object hObject - certFromObject()
	- Find any X.509 certificate - certFromLabel(), no label supplied.

  These are general-purpose functions whose behaviour can be modified through
  the following action codes */

typedef enum {
	FINDCERT_NONE,			/* No find-cert flag */
	FINDCERT_NORMAL,		/* Instantiate standard a certificate+context */
	FINDCERT_DATAONLY,		/* Instantiate data-only certificate */
	FINDCERT_P11OBJECT,		/* Return handle to PKCS #11 object */
	FINDCERT_LAST			/* Maximum possible flag value */
	} FINDCERT_ACTION_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int findCertFromTemplate( INOUT PKCS11_INFO *pkcs11Info,
								 IN_HANDLE const CRYPT_DEVICE iCertSource, 
								 IN_ARRAY( templateCount ) \
									const CK_ATTRIBUTE *findTemplate,
								 IN_RANGE( 1, 64 ) const int templateCount,
								 OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
								 OUT_OPT CK_OBJECT_HANDLE *hCertificate,
								 IN_ENUM( FINDCERT_ACTION ) \
									const FINDCERT_ACTION_TYPE findAction )
	{
	CK_OBJECT_HANDLE hFindCertificate;
	int cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isReadPtr( findTemplate, \
					   sizeof( CK_ATTRIBUTE ) * templateCount ) );
	assert( ( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) && \
			  hCertificate == NULL ) || \
			( iCryptCert == NULL && \
			  isWritePtr( hCertificate, sizeof( CK_OBJECT_HANDLE ) ) ) );

	REQUIRES( isHandleRangeValid( iCertSource ) );
	REQUIRES( templateCount >= 1 && templateCount <= 64 );
	REQUIRES( ( iCryptCert != NULL && hCertificate == NULL ) || \
			  ( iCryptCert == NULL && hCertificate != NULL ) );
	REQUIRES( findAction > FINDCERT_NONE && findAction < FINDCERT_LAST );

	/* Clear return values */
	if( iCryptCert != NULL )
		*iCryptCert = CRYPT_ERROR;
	if( hCertificate != NULL )
		*hCertificate = CK_OBJECT_NONE;

	/* Try and find the certificate from the given template */
	cryptStatus = findObject( pkcs11Info, &hFindCertificate, findTemplate, 
							  templateCount );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	if( findAction == FINDCERT_P11OBJECT )
		{
		*hCertificate = hFindCertificate;
		return( CRYPT_OK );
		}

	return( getCertChain( pkcs11Info, iCertSource, hFindCertificate, iCryptCert, 
						  ( findAction == FINDCERT_NORMAL ) ? TRUE : FALSE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int findCertFromLabel( INOUT PKCS11_INFO *pkcs11Info,
							  IN_HANDLE const CRYPT_DEVICE iCertSource, 
							  const CK_ATTRIBUTE_TYPE labelType,
							  IN_BUFFER_OPT( labelLength ) const char *label, 
							  IN_LENGTH_SHORT_Z const int labelLength,
							  OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
							  OUT_OPT CK_OBJECT_HANDLE *hCertificate,
							  IN_ENUM( FINDCERT_ACTION ) \
									const FINDCERT_ACTION_TYPE findAction )
	{
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	CK_ATTRIBUTE certTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		{ CKA_LABEL, NULL, 0 }
		};

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( ( label == NULL && labelLength == 0 ) || \
			isReadPtr( label, labelLength ) );
	assert( ( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) && \
			  hCertificate == NULL ) || \
			( iCryptCert == NULL && \
			  isWritePtr( hCertificate, sizeof( CK_OBJECT_HANDLE ) ) ) );

	REQUIRES( isHandleRangeValid( iCertSource ) );
	REQUIRES( ( label == NULL && labelLength == 0 ) || \
			  ( label != NULL && \
				labelLength > 0 && labelLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( iCryptCert != NULL && hCertificate == NULL ) || \
			  ( iCryptCert == NULL && hCertificate != NULL ) );
	REQUIRES( findAction > FINDCERT_NONE && findAction < FINDCERT_LAST );

	/* Clear return values */
	if( iCryptCert != NULL )
		*iCryptCert = CRYPT_ERROR;
	if( hCertificate != NULL )
		*hCertificate = CK_OBJECT_NONE;

	/* Try and find the certificate with the given label if there's one
	   supplied.  Usually this is the CKA_LABEL but it can also be something 
	   like a CKA_URL */
	if( label != NULL )
		{
		certTemplate[ 2 ].type = labelType;
		certTemplate[ 2 ].pValue = ( CK_VOID_PTR ) label;
		certTemplate[ 2 ].ulValueLen = labelLength;
		}
	return( findCertFromTemplate( pkcs11Info, iCertSource, certTemplate, 
								  ( label == NULL ) ? 2 : 3, iCryptCert, 
								  hCertificate, findAction ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int findCertFromID( INOUT PKCS11_INFO *pkcs11Info,
						   IN_HANDLE const CRYPT_DEVICE iCertSource, 
						   IN_BUFFER( certIDlength ) const void *certID, 
						   IN_LENGTH_SHORT const int certIDlength,
						   OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
						   OUT_OPT CK_OBJECT_HANDLE *hCertificate,
						   IN_ENUM( FINDCERT_ACTION ) \
								const FINDCERT_ACTION_TYPE findAction )
	{
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	CK_ATTRIBUTE certTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		{ CKA_ID, ( CK_VOID_PTR ) certID, certIDlength }
		};

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isReadPtr( certID, certIDlength ) );
	assert( ( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) && \
			  hCertificate == NULL ) || \
			( iCryptCert == NULL && \
			  isWritePtr( hCertificate, sizeof( CK_OBJECT_HANDLE ) ) ) );

	REQUIRES( isHandleRangeValid( iCertSource ) );
	REQUIRES( certIDlength > 0 && certIDlength < MAX_INTLENGTH_SHORT );
	REQUIRES( ( iCryptCert != NULL && hCertificate == NULL ) || \
			  ( iCryptCert == NULL && hCertificate != NULL ) );
	REQUIRES( findAction > FINDCERT_NONE && findAction < FINDCERT_LAST );

	/* Clear return values */
	if( iCryptCert != NULL )
		*iCryptCert = CRYPT_ERROR;
	if( hCertificate != NULL )
		*hCertificate = CK_OBJECT_NONE;

	/* Try and find the certificate with the given ID */
	return( findCertFromTemplate( pkcs11Info, iCertSource, certTemplate, 3, 
								  iCryptCert, hCertificate, findAction ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int findCertFromObject( INOUT PKCS11_INFO *pkcs11Info,
							   IN_HANDLE const CRYPT_DEVICE iCertSource, 
							   const CK_OBJECT_HANDLE hObject, 
							   OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
							   IN_ENUM( FINDCERT_ACTION ) \
									const FINDCERT_ACTION_TYPE findAction )
	{
	BYTE buffer[ MAX_BUFFER_SIZE + 8 ], *bufPtr;
	int length, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( isHandleRangeValid( iCertSource ) );
	REQUIRES( findAction > FINDCERT_NONE && findAction < FINDCERT_LAST );

	/* Clear return value */
	*iCryptCert = CRYPT_ERROR;

	/* We're looking for a certificate whose ID matches the object, read the 
	   key ID from the device.  We can't use a dynBuf for this because it's a 
	   PKCS #11 attribute rather than a cryptlib attribute */
	cryptStatus = getAttributeValue( pkcs11Info, hObject, CKA_ID, &bufPtr, 
									 &length, buffer, MAX_BUFFER_SIZE );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Look for a certificate with the same ID as the key */
	cryptStatus = findCertFromID( pkcs11Info, iCertSource, bufPtr, length, 
								  iCryptCert, NULL, findAction );
	getAttributeValueEnd( bufPtr, buffer );
	return( cryptStatus );
	}

/* Find an object using a non-PKCS #11 ID.  This is a special-case function
   that's used in rare situations where only incomplete PKCS #11 format
   support is available in the underlying device, for example with a token
   that's been initialised via CryptoAPI (which doesn't populate all of the 
   PKCS #11 fields) but which is now being accessed through a PKCS #11 
   driver.  It works by enumerating every object (of the required type, 
   which in this case is only certificates) in the token, creating a 
   cryptlib native object from them, and matching by the ID information
   provided by cryptlib.
   
   Since this complex search function is used by both findCert() (which 
   always tries to return a complete certificate chain) and getFirst()/
   getNext() (which are called indirectly by findCert() and which only 
   require a PKCS #11 certificate object handle to turn into a single
   standalone certificate) we have to provide a dual-purpose interface, 
   one that returns a certificate chain when called from findCert() and a 
   second one that returns a PKCS #11 certificate object handle when called 
   from getFirst()/getNext() as called by findCert() */

#ifdef PKCS11_FIND_VIA_CRYPTLIB

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int matchID( IN_HANDLE CRYPT_CERTIFICATE iCryptCert,
					IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
					IN_BUFFER( keyIDlength ) const void *keyID, 
					IN_LENGTH_KEYID const int keyIDlength )
	{
	MESSAGE_DATA msgData;
	char buffer[ MAX_ATTRIBUTE_SIZE + 8 ];
	int cryptStatus;

	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( isHandleRangeValid( iCryptCert ) );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER || \
			  keyIDtype == CRYPT_KEYID_LAST );
			  /* _LAST is by subject DN, i.e. child.issuerDN */
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	switch( keyIDtype )
		{
		case CRYPT_KEYID_NAME:
			/* This identifier is usually an arbitrary user-defined label
			   attached to the object, in the absence of this we use the 
			   certificate CN (or equivalent) as the item to match on.  This
			   is consistent with the use in updateCertificate(), which sets 
			   the holder name as the PKCS #11 label when a new certificate 
			   is added (only explicitly-created public- or private-key 
			   objects have user-defined labels) */
			setMessageData( &msgData, buffer, MAX_ATTRIBUTE_SIZE );
			cryptStatus = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S,
										   &msgData, 
										   CRYPT_IATTRIBUTE_HOLDERNAME );
			if( cryptStatusError( cryptStatus ) )
				return( cryptStatus );
			if( msgData.length != keyIDlength || \
				strCompare( msgData.data, keyID, keyIDlength ) )
				return( CRYPT_ERROR_NOTFOUND );
			return( CRYPT_OK );

		case CRYPT_KEYID_URI:
			setMessageData( &msgData, buffer, MAX_ATTRIBUTE_SIZE );
			cryptStatus = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S,
										   &msgData, 
										   CRYPT_IATTRIBUTE_HOLDERURI );
			if( cryptStatusError( cryptStatus ) )
				return( cryptStatus );
			if( msgData.length != keyIDlength || \
				strCompare( msgData.data, keyID, keyIDlength ) )
				return( CRYPT_ERROR_NOTFOUND );
			return( CRYPT_OK );

		case CRYPT_IKEYID_KEYID:
			setMessageData( &msgData, ( void * ) keyID, keyIDlength );
			return( krnlSendMessage( iCryptCert, IMESSAGE_COMPARE, &msgData, 
									 MESSAGE_COMPARE_KEYID ) );

		case CRYPT_IKEYID_ISSUERANDSERIALNUMBER:
			setMessageData( &msgData, ( void * ) keyID, keyIDlength );
			return( krnlSendMessage( iCryptCert, IMESSAGE_COMPARE, &msgData, 
									 MESSAGE_COMPARE_ISSUERANDSERIALNUMBER ) );

		case CRYPT_KEYID_LAST:
			/* This is a special-case code to used denote a match of the 
			   subject name, specifically the issuer DN of the child 
			   certificate (which is the next certificate's subject DN) when 
			   chain building */
			setMessageData( &msgData, ( void * ) keyID, keyIDlength );
			return( krnlSendMessage( iCryptCert, IMESSAGE_COMPARE, &msgData, 
									 MESSAGE_COMPARE_SUBJECT ) );

		default:
			retIntError();
		}
	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
static int searchDeviceObjects( INOUT PKCS11_INFO *pkcs11Info,
								OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
								OUT_OPT CK_OBJECT_HANDLE *hObject,
								IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
								IN_BUFFER( keyIDlength ) const void *keyID, 
								IN_LENGTH_KEYID const int keyIDlength,
								const BOOLEAN onlyOne )
	{
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	CK_ATTRIBUTE certTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		};
	CK_OBJECT_HANDLE hMatchedObject = CK_OBJECT_NONE;
	CK_ULONG ulObjectCount;
	CK_RV status;
	CRYPT_CERTIFICATE iMatchedCert = CRYPT_ERROR;
	BOOLEAN foundMatch = FALSE;
	int i, cryptStatus = CRYPT_ERROR_NOTFOUND;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( ( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) && \
			  hObject == NULL ) || \
			( iCryptCert == NULL && \
			  isWritePtr( hObject, sizeof( CK_OBJECT_HANDLE ) ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( ( iCryptCert != NULL && hObject == NULL ) || \
			  ( iCryptCert == NULL && hObject != NULL ) );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER || \
			  keyIDtype == CRYPT_KEYID_LAST );
			  /* _LAST is by subject DN, i.e. child.issuerDN */
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	/* Clear return values */
	if( iCryptCert != NULL )
		*iCryptCert = CRYPT_ERROR;
	if( hObject != NULL )
		*hObject = CK_OBJECT_NONE;

	status = C_FindObjectsInit( pkcs11Info->hSession, 
								( CK_ATTRIBUTE_PTR ) &certTemplate, 2 );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_NOTFOUND ) );
	for( i = 0; i < FAILSAFE_ITERATIONS_LARGE; i++ )
		{
		CK_OBJECT_HANDLE hMatchObject;
		CRYPT_CERTIFICATE iMatchCert;

		/* Try and get the next object of the requested type */
		status = C_FindObjects( pkcs11Info->hSession, &hMatchObject, 1, 
								&ulObjectCount );
		if( status != CKR_OK )
			{
			cryptStatus = pkcs11MapError( status, CRYPT_ERROR_NOTFOUND );
			break;
			}
		if( ulObjectCount <= 0 )
			{
			cryptStatus = CRYPT_ERROR_NOTFOUND;
			break;
			}

		/* Check whether this certificate meets the match criteria.  If 
		   we're matching on keyID we have to create the context associated
		   with the certificate in order to calculate the keyID, otherwise
		   we use a data-only certificate */
		cryptStatus = instantiateCert( pkcs11Info, hMatchObject, &iMatchCert, 
									   ( keyIDtype == CRYPT_IKEYID_KEYID ) ? \
											TRUE : FALSE );
		if( cryptStatusError( cryptStatus ) )
			{
			/* We couldn't create a certificate from the stored data, this
			   is a non-fatal error since there may be more objects present
			   so we move on to those */
			continue;
			}
		cryptStatus = matchID( iMatchCert, keyIDtype, keyID, keyIDlength );
		if( cryptStatusOK( cryptStatus ) && !foundMatch )
			{
			/* We've found a matching certificate, remember it for later */
			hMatchedObject = hMatchObject;
			iMatchedCert = iMatchCert;
			}
		else
			krnlSendNotifier( iMatchCert, IMESSAGE_DECREFCOUNT );
		if( cryptStatusError( cryptStatus ) )
			continue;

		/* We've found a matching certificate, if we're looking for a first-
		   match then we're done */
		if( !onlyOne )
			break;

		/* We're only looking for one matching certificate, if we've found a 
		   second match then this is an error */
		if( foundMatch )
			{
			krnlSendNotifier( iMatchedCert, IMESSAGE_DECREFCOUNT );
			iMatchedCert = CRYPT_ERROR;
			hMatchedObject = CK_OBJECT_NONE;
			cryptStatus = CRYPT_ERROR_DUPLICATE;
			break;
			}
		foundMatch = TRUE;
		}
	ENSURES( i < FAILSAFE_ITERATIONS_LARGE );
	if( C_FindObjectsFinal != NULL )
		C_FindObjectsFinal( pkcs11Info->hSession );

	/* If we found a matching certificate, return it to the caller.  We 
	   can't directly return the status code since the last one encountered 
	   may have been a CRYPT_ERROR_NOTFOUND if we're checking for the 
	   existence of only a single certificate by enumerating all 
	   certificates present */
	if( iMatchedCert != CRYPT_ERROR )
		{
		if( iCryptCert != NULL )
			*iCryptCert = iMatchedCert;
		else
			{
			krnlSendNotifier( iMatchedCert, IMESSAGE_DECREFCOUNT );
			*hObject = hMatchedObject;
			}
		return( CRYPT_OK );
		}
	return( cryptStatus );
	}
#endif /* PKCS11_FIND_VIA_CRYPTLIB */

/* Umbrella find-a-certificate function */
		
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int findCert( INOUT PKCS11_INFO *pkcs11Info,
					 IN_HANDLE const CRYPT_DEVICE iCertSource, 
					 IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
					 IN_BUFFER( keyIDlength ) const void *keyID, 
					 IN_LENGTH_KEYID const int keyIDlength,
					 OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
					 OUT_OPT CK_OBJECT_HANDLE *hCertificate )
	{
	CK_ATTRIBUTE iAndSTemplate[ 4 + 8 ], iAndSTemplateAlt[ 4 + 8 ];
#ifdef PKCS11_FIND_VIA_CRYPTLIB
	CK_OBJECT_HANDLE hFindCertificate;
#endif /* PKCS11_FIND_VIA_CRYPTLIB */
	const FINDCERT_ACTION_TYPE findAction = \
			( hCertificate != NULL ) ? FINDCERT_P11OBJECT : FINDCERT_NORMAL;
	int cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( ( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) && \
			  hCertificate == NULL ) || \
			( iCryptCert == NULL && \
			  isWritePtr( hCertificate, sizeof( CK_OBJECT_HANDLE ) ) ) );

	REQUIRES( isHandleRangeValid( iCertSource ) );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER || \
			  keyIDtype == CRYPT_KEYID_LAST );
			  /* _LAST is by subject DN, i.e. child.issuerDN */
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( ( iCryptCert != NULL && hCertificate == NULL ) || \
			  ( iCryptCert == NULL && hCertificate != NULL ) );

	/* Clear return values */
	if( iCryptCert != NULL )
		*iCryptCert = CRYPT_ERROR;
	if( hCertificate != NULL )
		*hCertificate = CK_OBJECT_NONE;

	switch( keyIDtype )
		{
		case CRYPT_IKEYID_ISSUERANDSERIALNUMBER:
			cryptStatus = initIAndSTemplate( iAndSTemplate, 
											 iAndSTemplateAlt,
											 keyID, keyIDlength );
			if( cryptStatusError( cryptStatus ) )
				return( cryptStatus );
			cryptStatus = findCertFromTemplate( pkcs11Info, iCertSource, 
												iAndSTemplate, 4, iCryptCert, 
												NULL, findAction );
			if( cryptStatus == CRYPT_ERROR_NOTFOUND )
				cryptStatus = findCertFromTemplate( pkcs11Info, iCertSource, 
													iAndSTemplateAlt, 4, iCryptCert, 
													NULL, findAction );
			if( cryptStatusOK( cryptStatus ) )
				return( cryptStatus );
			break;

		case CRYPT_IKEYID_KEYID:
			cryptStatus = findCertFromID( pkcs11Info, iCertSource, 
										  keyID, keyIDlength, iCryptCert, 
										  hCertificate, findAction );
			if( cryptStatusOK( cryptStatus ) )
				return( cryptStatus );
			break;

		case CRYPT_KEYID_NAME:
		case CRYPT_KEYID_URI:
			cryptStatus = findCertFromLabel( pkcs11Info, iCertSource, 
											 ( keyIDtype == CRYPT_KEYID_NAME ) ? \
												CKA_LABEL : CKA_URL,
											 keyID, keyIDlength, iCryptCert, 
											 hCertificate, findAction );
			if( cryptStatus == CRYPT_ERROR_NOTFOUND )
				{
				/* Some devices use the iD in place of the label, if a 
				   search by label fails we try again with the label as the 
				   iD */
				cryptStatus = findCertFromID( pkcs11Info, iCertSource, 
											  keyID, keyIDlength, iCryptCert, 
											  hCertificate, findAction );
				}
			if( cryptStatusOK( cryptStatus ) )
				return( cryptStatus );
			break;

		default:
			retIntError();
		}

	/* A standard search has failed, this may be because the necessary PKCS 
	   #11 ID information isn't present in the token, in which case we 
	   optionally try again by performing the search ourselves */
#ifdef PKCS11_FIND_VIA_CRYPTLIB
	cryptStatus = searchDeviceObjects( pkcs11Info, NULL, &hFindCertificate, 
									   keyIDtype, keyID, keyIDlength, 
									   TRUE );
	if( cryptStatusOK( cryptStatus ) )
		{
		return( getCertChain( pkcs11Info, iCertSource, hFindCertificate, 
							  iCryptCert, 
							  ( findAction == FINDCERT_NORMAL ) ? \
									TRUE : FALSE ) );
		}
#endif /* PKCS11_FIND_VIA_CRYPTLIB */

	return( cryptStatus );
	}

/* Find a public- or private-key object.  Alongside the standard cryptlib 
   status code we return an extended information code to handle the 
   convoluted search strategy required in getItemFunction(), see the 
   comments inline for the reason for the extra codes */

typedef enum {
	FIND_PUBPRIV_NONE,		/* No find-cert flag */
	FIND_PUBPRIV_CERTVIAKEY,/* Key found via certificate */
	FIND_PUBPRIV_KEYVIACERT,/* Certificate found via key */
	FIND_PUBPRIV_LAST		/* Maximum possible flag value */
	} FIND_PUBPRIV_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 6 ) ) \
static int findPubPrivKey( INOUT PKCS11_INFO *pkcs11Info,
						   OUT CK_OBJECT_HANDLE *hObject,
						   OUT CK_OBJECT_HANDLE *hCertificate,
						   OUT_ENUM_OPT( FIND_PUBPRIV ) \
								FIND_PUBPRIV_TYPE *findType,
						   IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
						   IN_BUFFER_OPT( keyIDlength ) const void *keyID, 
						   IN_LENGTH_KEYID_Z const int keyIDlength,
						   const BOOLEAN isPublicKey )
	{
	static const CK_OBJECT_CLASS pubkeyClass = CKO_PUBLIC_KEY;
	static const CK_OBJECT_CLASS privkeyClass = CKO_PRIVATE_KEY;
	CK_ATTRIBUTE keyTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) \
					 ( isPublicKey ? &pubkeyClass : &privkeyClass ), 
										sizeof( CK_OBJECT_CLASS ) },
		{ CKA_LABEL, ( CK_VOID_PTR ) keyID, keyIDlength }
		};
	int cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( hObject, sizeof( CK_OBJECT_HANDLE ) ) );
	assert( isWritePtr( hCertificate, sizeof( CK_OBJECT_HANDLE ) ) );
	assert( isWritePtr( findType, sizeof( FIND_PUBPRIV_TYPE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	/* Clear return values */
	*hObject = CK_OBJECT_NONE;
	*hCertificate = CK_OBJECT_NONE;
	*findType = FIND_PUBPRIV_NONE;

	/* Try and find the object with the given label/ID, or the first object 
	   of the given class if no ID is given */
	if( keyIDtype == CRYPT_IKEYID_KEYID )
		keyTemplate[ 1 ].type = CKA_ID;
	cryptStatus = findObject( pkcs11Info, hObject, keyTemplate, 2 );
	if( cryptStatus != CRYPT_ERROR_NOTFOUND )
		return( cryptStatus );

	/* Some devices use the iD in place of the label, if we're doing a 
	   search by label and it fails then we try again with the label as 
	   the iD */
	if( keyIDtype == CRYPT_KEYID_NAME )
		{
		keyTemplate[ 1 ].type = CKA_ID;
		cryptStatus = findObject( pkcs11Info, hObject, keyTemplate, 2 );
		keyTemplate[ 1 ].type = CKA_LABEL;
		if( cryptStatus != CRYPT_ERROR_NOTFOUND )
			return( cryptStatus );
		}

	/* Some devices may only contain private key objects with associated 
	   certificates that can't be picked out of the other cruft that's 
	   present without going via the private key, so if we're looking for a 
	   public key and don't find one we try again for a private key whose 
	   sole function is to point to an associated certificate */
	if( isPublicKey )
		{
		keyTemplate[ 0 ].pValue = ( CK_VOID_PTR ) &privkeyClass;
		cryptStatus = findObject( pkcs11Info, hObject, keyTemplate, 2 );
		if( cryptStatusError( cryptStatus ) )
			return( cryptStatus );
		
		/* Tell the caller that although we've got a private key object we 
		   only need it to find the associated certificate and not finding 
		   an associated certificate is an error */
		*findType = FIND_PUBPRIV_CERTVIAKEY;

		return( CRYPT_OK );
		}

	/* A standard search has failed, this may be because the necessary PKCS 
	   #11 ID information isn't present in the token, in which case we 
	   optionally try again by performing the search ourselves */
#ifdef PKCS11_FIND_VIA_CRYPTLIB
	cryptStatus = searchDeviceObjects( pkcs11Info, NULL, hCertificate, 
									   keyIDtype, keyID, keyIDlength, TRUE );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* We've found the identified certificate, use it to find the 
	   corresponding private key */
	cryptStatus = findObjectFromObject( pkcs11Info, *hCertificate, 
										CKO_PRIVATE_KEY, hObject );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	
	/* Tell the caller that we've already got a certificate to attach to 
	   the private key */
	*findType = FIND_PUBPRIV_KEYVIACERT;

	return( CRYPT_OK );
#else
	return( CRYPT_ERROR_NOTFOUND );
#endif /* PKCS11_FIND_VIA_CRYPTLIB */
	}

/****************************************************************************
*																			*
*						 	Read an Item from a Device						*
*																			*
****************************************************************************/

/* Instantiate an object in a device.  This works like the create context
   function but instantiates a cryptlib object using data already contained
   in the device (for example public-key components stored with a private 
   key or a stored certificate) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int rsaSetPublicComponents( INOUT PKCS11_INFO *pkcs11Info,
							IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							const CK_OBJECT_HANDLE hRsaKey,
							const BOOLEAN nativeContext );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int dsaSetPublicComponents( INOUT PKCS11_INFO *pkcs11Info,
							IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							const CK_OBJECT_HANDLE hDsaKey );

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int createNativeObject( INOUT PKCS11_INFO *pkcs11Info,
							   OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
							   const CK_OBJECT_HANDLE hObject,
							   IN_ENUM( KEYMGMT_ITEM ) \
									const KEYMGMT_ITEM_TYPE itemType,
							   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	CRYPT_CONTEXT iLocalContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int actionFlags, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY || \
			  itemType == KEYMGMT_ITEM_SECRETKEY );
	REQUIRES( cryptAlgo >= CRYPT_ALGO_FIRST_CONVENTIONAL && \
			  cryptAlgo <= CRYPT_ALGO_LAST_MAC ); 

	/* Clear return value */
	*iCryptContext = CRYPT_ERROR;

	/* Get the permitted-action flags for the object.  If no usage is 
	   allowed then we can't do anything with the object so we don't even 
	   try and create it */
	actionFlags = getActionFlags( pkcs11Info, hObject, itemType, cryptAlgo );
	if( actionFlags <= 0 )
		return( CRYPT_ERROR_PERMISSION );

	/* We're creating a public-key context, make it a native context instead 
	   of a device one.  This solves a variety of problems including the 
	   fact that some devices (which function purely as key stores coupled 
	   to modexp accelerators) only support private-key operations, that 
	   performing public-key operations natively is much, much faster than 
	   on any token, and finally that if we do it ourselves we can defend 
	   against a variety of RSA padding and timing attacks that have come up 
	   since the device firmware was done */
	setMessageCreateObjectInfo( &createInfo, cryptAlgo );
	cryptStatus = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								   IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
								   OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	iLocalContext = createInfo.cryptHandle;

	/* Send the keying information to the context and set the action flags */
	if( cryptAlgo == CRYPT_ALGO_RSA )
		cryptStatus = rsaSetPublicComponents( pkcs11Info, iLocalContext, 
											  hObject, TRUE );
	else
		cryptStatus = dsaSetPublicComponents( pkcs11Info, iLocalContext, 
											  hObject );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE, 
									   &actionFlags, 
									   CRYPT_IATTRIBUTE_ACTIONPERMS );
	if( cryptStatusError( cryptStatus ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		return( cryptStatus );
		}
	*iCryptContext = iLocalContext;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 7 ) ) \
static int createDeviceObject( INOUT PKCS11_INFO *pkcs11Info,
							   OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
							   const CK_OBJECT_HANDLE hObject,
							   IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptCert,
							   IN_HANDLE const CRYPT_USER iOwnerHandle,
							   IN_HANDLE const CRYPT_DEVICE iDeviceHandle,
							   const CAPABILITY_INFO *capabilityInfoPtr,
							   IN_ENUM( KEYMGMT_ITEM ) \
									const KEYMGMT_ITEM_TYPE itemType,
							   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
							   IN_LENGTH_KEY const int keySize )
	{
	CRYPT_CONTEXT iLocalContext;
	MESSAGE_DATA msgData;
	char label[ CRYPT_MAX_TEXTSIZE + 8 ];
	int createFlags = CREATEOBJECT_FLAG_DUMMY;
	int actionFlags, labelLength, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );

	REQUIRES( ( iCryptCert == CRYPT_UNUSED ) || \
			  isHandleRangeValid( iCryptCert ) );
	REQUIRES( iOwnerHandle == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iOwnerHandle ) );
	REQUIRES( isHandleRangeValid( iDeviceHandle ) );
	REQUIRES( itemType == KEYMGMT_ITEM_PRIVATEKEY || \
			  itemType == KEYMGMT_ITEM_SECRETKEY );
	REQUIRES( cryptAlgo >= CRYPT_ALGO_FIRST_CONVENTIONAL && \
			  cryptAlgo <= CRYPT_ALGO_LAST_MAC );
	REQUIRES( keySize >= MIN_KEYSIZE && keySize <= CRYPT_MAX_PKCSIZE );

	/* Clear return value */
	*iCryptContext = CRYPT_ERROR;

	/* Check whether this is a persistent object */
	if( readFlag( pkcs11Info, hObject, CKA_TOKEN ) )
		createFlags |= CREATEOBJECT_FLAG_PERSISTENT;

	/* Get the permitted-action flags for the object */
	actionFlags = getActionFlags( pkcs11Info, hObject, itemType, cryptAlgo );
	if( actionFlags <= 0 )
		{
		/* If no usage is allowed then we can't do anything with the object 
		   so we don't even try to create it */
		return( CRYPT_ERROR_PERMISSION );
		}

	/* Create a dummy context for the key and remember the device that it's 
	   contained in */
	cryptStatus = getObjectLabel( pkcs11Info, hObject, label, 
								  CRYPT_MAX_TEXTSIZE, &labelLength );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	if( labelLength <= 0 )
		{
		/* If there's no label present, use a dummy value */
		strlcpy_s( label, CRYPT_MAX_TEXTSIZE, "Label-less PKCS #11 key" );
		labelLength = 23;
		}
	cryptStatus = createContextFromCapability( &iLocalContext, 
							iOwnerHandle, capabilityInfoPtr, 
							createFlags | CREATEOBJECT_FLAG_PERSISTENT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = krnlSendMessage( iLocalContext, IMESSAGE_SETDEPENDENT,
								   ( MESSAGE_CAST ) &iDeviceHandle, 
								   SETDEP_OPTION_INCREF );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE, 
									   &actionFlags, 
									   CRYPT_IATTRIBUTE_ACTIONPERMS );
	if( cryptStatusError( cryptStatus ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		return( cryptStatus );
		}

	/* Set the object's label, record the handle for the device-internal 
	   key, and mark it as initialised (i.e. with a key loaded).  Setting 
	   the label requires special care because the label that we're setting 
	   matches that of an existing object so trying to set it as a standard 
	   CRYPT_CTXINFO_LABEL will return a CRYPT_ERROR_DUPLICATE error when 
	   the context code checks for the existence of an existing label.  To 
	   handle this we use the attribute CRYPT_IATTRIBUTE_EXISTINGLABEL to 
	   indicate that we're setting a label that matches an existing object 
	   in the device */
	setMessageData( &msgData, label, min( labelLength, CRYPT_MAX_TEXTSIZE ) );
	cryptStatus = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE_S,
								   &msgData, CRYPT_IATTRIBUTE_EXISTINGLABEL );
	if( cryptStatusOK( cryptStatus ) )
		{
		/* Send the keying information to the context.  For non-PKC contexts 
		   we only need to set the key length to let the user query the key 
		   size, for PKC contexts we also have to set the key components so
		   that they can be written into certificates.  Unfortunately we 
		   can't do this for DLP private keys since we can't read y from a 
		   DLP private key object (see the comments in the DSA code for more 
		   on this), however the only time that this is necessary is when a 
		   certificate is being generated for a key that was pre-generated 
		   in the device by someone else, which is typically done in Europe 
		   where DSA isn't used so this shouldn't be a problem */
		if( cryptAlgo == CRYPT_ALGO_RSA )
			{
			cryptStatus = rsaSetPublicComponents( pkcs11Info, iLocalContext, 
												  hObject, FALSE );
			}
		else
			{
			cryptStatus = krnlSendMessage( iLocalContext, 
										   IMESSAGE_SETATTRIBUTE, 
										   ( MESSAGE_CAST ) &keySize, 
										   CRYPT_IATTRIBUTE_KEYSIZE );
			}
		}
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE, 
									   ( MESSAGE_CAST ) &hObject, 
									   CRYPT_IATTRIBUTE_DEVICEOBJECT );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE,
									   MESSAGE_VALUE_UNUSED, 
									   CRYPT_IATTRIBUTE_INITIALISED );
	if( cryptStatusOK( cryptStatus ) && ( iCryptCert != CRYPT_UNUSED ) )
		{
		/* If it's a public key and there's a certificate present attach it 
		   to the context.  The certificate is an internal object used only 
		   by the context so we tell the kernel to mark it as owned by the 
		   context only */
		cryptStatus = krnlSendMessage( iLocalContext, IMESSAGE_SETDEPENDENT, 
									   ( MESSAGE_CAST ) &iCryptCert, 
									   SETDEP_OPTION_NOINCREF );
		}
	if( cryptStatusError( cryptStatus ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		return( cryptStatus );
		}
	*iCryptContext = iLocalContext;

	return( CRYPT_OK );
	}

/* Get an item from a device and instantiate either a native or a device 
   object from it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
static int getSecretKey( INOUT DEVICE_INFO *deviceInfo,
						 INOUT PKCS11_INFO *pkcs11Info,
						 OUT_HANDLE_OPT CRYPT_HANDLE *iCryptHandle,
						 IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
						 IN_BUFFER( keyIDlength ) const void *keyID, 
						 IN_LENGTH_KEYID const int keyIDlength,
						 IN_FLAGS_Z( KEYMGMT ) const int flags )
	{
	static const CK_OBJECT_CLASS secKeyClass = CKO_SECRET_KEY;
	const CAPABILITY_INFO *capabilityInfoPtr;
	CK_ULONG secKeySize = DUMMY_INIT;
	CK_ATTRIBUTE keyTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &secKeyClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_LABEL, NULL_PTR, 0 }
		};
	CK_ATTRIBUTE secKeySizeTemplate = \
		{ CKA_VALUE_LEN, &secKeySize, sizeof( CK_ULONG ) };
	CK_OBJECT_HANDLE hObject = CK_OBJECT_NONE;
	CRYPT_ALGO_TYPE cryptAlgo;
	int status, cryptStatus;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( iCryptHandle, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_IKEYID_KEYID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( flags >= KEYMGMT_FLAG_NONE && flags < KEYMGMT_FLAG_MAX );

	/* Clear return value */
	*iCryptHandle = CRYPT_ERROR;

	/* Try and find the object with the given label/ID */
	if( keyIDtype == CRYPT_IKEYID_KEYID )
		keyTemplate[ 1 ].type = CKA_ID;
	keyTemplate[ 1 ].pValue = ( CK_VOID_PTR ) keyID;
	keyTemplate[ 1 ].ulValueLen = keyIDlength;
	cryptStatus = findObject( pkcs11Info, &hObject, keyTemplate, 2 );
	if( cryptStatus == CRYPT_ERROR_NOTFOUND )
		{
		/* Some devices use the iD in place of the label, if a search by 
		   label fails we try again with the label as the iD */
		keyTemplate[ 1 ].type = CKA_ID;
		cryptStatus = findObject( pkcs11Info, &hObject, keyTemplate, 2 );
		}
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* If it's just an existence check, return now */
	if( flags & KEYMGMT_FLAG_CHECK_ONLY )
		return( CRYPT_OK );

	/* We found something, map the key type to a cryptlib algorithm ID and 
	   find its capabilities */
	cryptStatus = getMechanismInfo( pkcs11Info, hObject, 
									deviceInfo->capabilityInfoList,
									FALSE, &capabilityInfoPtr,
									&cryptAlgo );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	status = C_GetAttributeValue( pkcs11Info->hSession, hObject, 
								  &secKeySizeTemplate, 1 );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_NOTINITED ) );

	/* Create the object as a device object */
	return( createDeviceObject( pkcs11Info, iCryptHandle, hObject, 
								CRYPT_UNUSED, deviceInfo->ownerHandle, 
							    deviceInfo->objectHandle, capabilityInfoPtr,
							    KEYMGMT_ITEM_SECRETKEY, cryptAlgo, 
								secKeySize ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5, 8 ) ) \
static int getItemFunction( INOUT DEVICE_INFO *deviceInfo,
							OUT_HANDLE_OPT CRYPT_HANDLE *iCryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							IN_BUFFER( keyIDlength ) const void *keyID, 
							IN_LENGTH_KEYID const int keyIDlength,
							IN_OPT void *auxInfo, 
							INOUT_LENGTH_SHORT_Z int *auxInfoLength,
							IN_FLAGS_Z( KEYMGMT ) const int flags )
	{
	static const MAP_TABLE keySizeMapTbl[] = {
		{ CRYPT_ALGO_RSA, CKA_MODULUS },
		{ CRYPT_ALGO_DSA, CKA_PRIME },
		{ CRYPT_ALGO_DH, CKA_PRIME },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	const CAPABILITY_INFO *capabilityInfoPtr;
	CK_ATTRIBUTE keySizeTemplate = { 0, NULL, 0 };
	CK_OBJECT_HANDLE hObject = CK_OBJECT_NONE, hCertificate = CK_OBJECT_NONE;
	CK_RV status;
	CRYPT_CERTIFICATE iCryptCert = DUMMY_INIT;
	CRYPT_ALGO_TYPE cryptAlgo;
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;
	BOOLEAN certViaPrivateKey = FALSE, privateKeyViaCert = FALSE;
	BOOLEAN certPresent = FALSE;
	int keySize, cryptStatus;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( iCryptHandle, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );
	assert( ( auxInfo == NULL && *auxInfoLength == 0 ) || \
			isReadPtr( auxInfo, *auxInfoLength ) );

	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY || \
			  itemType == KEYMGMT_ITEM_SECRETKEY );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( ( auxInfo == NULL && *auxInfoLength == 0 ) || \
			  ( auxInfo != NULL && \
				*auxInfoLength > 0 && \
				*auxInfoLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( flags >= KEYMGMT_FLAG_NONE && flags < KEYMGMT_FLAG_MAX );

	/* Clear return value */
	*iCryptHandle = CRYPT_ERROR;

	/* If we're looking for a secret key then it's fairly straightforward */
	if( itemType == KEYMGMT_ITEM_SECRETKEY )
		return( getSecretKey( deviceInfo, pkcs11Info, iCryptHandle, 
							  keyIDtype, keyID, keyIDlength, flags ) );

	/* If we're looking for a public key, try for a certificate first.  Some 
	   non-crypto-capable devices don't have an explicit CKO_PUBLIC_KEY but 
	   only a CKO_CERTIFICATE and some apps delete the public key since it's
	   redundant, so we try to create a certificate object before we try 
	   anything else.  If the keyID type is an ID or label then this won't 
	   necessarily locate the certificate since it could be unlabelled or 
	   have a different label/ID, so if this fails we try again by going via 
	   the private key with the given label/ID */
	if( itemType == KEYMGMT_ITEM_PUBLICKEY )
		{
		CK_OBJECT_HANDLE hCertificateLabelObject = DUMMY_INIT;

		if( flags & ( KEYMGMT_FLAG_CHECK_ONLY | KEYMGMT_FLAG_LABEL_ONLY ) )
			{
			cryptStatus = findCert( pkcs11Info, deviceInfo->objectHandle,
									keyIDtype, keyID, keyIDlength, NULL, 
									&hCertificateLabelObject );
			}
		else
			{
			cryptStatus = findCert( pkcs11Info, deviceInfo->objectHandle,
									keyIDtype, keyID, keyIDlength, 
									&iCryptCert, NULL );
			}
		if( cryptStatusOK( cryptStatus ) )
			{
			/* If we're just checking whether an object exists, return now.  
			   If all we want is the key label, copy it back to the caller 
			   and exit */
			if( flags & KEYMGMT_FLAG_CHECK_ONLY )
				return( CRYPT_OK );
			if( flags & KEYMGMT_FLAG_LABEL_ONLY )
				{
				return( getObjectLabel( pkcs11Info, hCertificateLabelObject, 
										auxInfo, *auxInfoLength, 
										auxInfoLength ) );
				}
			*iCryptHandle = iCryptCert;

			return( CRYPT_OK );
			}

		/* If we're looking for a specific match on a certificate (rather 
		   than just a general public key) and we don't find anything, exit 
		   now */
		if( keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER )
			return( cryptStatus );
		}

	/* Either there were no certificates found or we're looking for a 
	   private key (or, somewhat unusually, a raw public key).  At this 
	   point we can approach the problem from one of two sides, if we've 
	   got an issuerAndSerialNumber we have to find the matching certificate 
	   and get the key from that, otherwise we find the key and get the 
	   certificate from that */
	if( keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER )
		{
		CK_ATTRIBUTE iAndSTemplate[ 4 + 8 ], iAndSTemplateAlt[ 4 + 8 ];

		/* Try and find the certificate from the given template.  Note that
		   we can't use findCert() for this because it returns a cryptlib
		   certificate and not a PKCS #11 object handle */
		cryptStatus = initIAndSTemplate( iAndSTemplate, iAndSTemplateAlt,
										 keyID, keyIDlength );
		if( cryptStatusError( cryptStatus ) )
			return( cryptStatus );
		cryptStatus = findObject( pkcs11Info, &hCertificate, 
								  iAndSTemplate, 4 );
		if( cryptStatus == CRYPT_ERROR_NOTFOUND )
			cryptStatus = findObject( pkcs11Info, &hCertificate, 
									  iAndSTemplateAlt, 4 );
#ifdef PKCS11_FIND_VIA_CRYPTLIB
		if( cryptStatus == CRYPT_ERROR_NOTFOUND )
			{
			cryptStatus = searchDeviceObjects( pkcs11Info, NULL, 
											   &hCertificate, keyIDtype, 
											   keyID, keyIDlength, TRUE );
			}
#endif /* PKCS11_FIND_VIA_CRYPTLIB */
		if( cryptStatusOK( cryptStatus ) )
			{
			/* We've found the identified certificate, use it to find the 
			   corresponding private key */
			cryptStatus = findObjectFromObject( pkcs11Info, hCertificate, 
												CKO_PRIVATE_KEY, &hObject );
			if( cryptStatusError( cryptStatus ) )
				return( cryptStatus );
	
			/* Remember that we've already got a certificate to attach to 
			   the private key */
			privateKeyViaCert = TRUE;
			}
		else
			{
			/* If we didn't find anything it may be because whoever set up 
			   the token didn't set the iAndS rather than because there's no
			   key there so we only bail out if we got some unexpected type 
			   of error */
			if( cryptStatus != CRYPT_ERROR_NOTFOUND )
				return( cryptStatus );
			}
		}
	else
		{
		FIND_PUBPRIV_TYPE findType;

		cryptStatus = findPubPrivKey( pkcs11Info, &hObject, &hCertificate, 
								&findType, keyIDtype, keyID, keyIDlength,
								( itemType == KEYMGMT_ITEM_PUBLICKEY ) ? \
									TRUE : FALSE );
		if( cryptStatusError( cryptStatus ) )
			return( cryptStatus );
		switch( findType )
			{
			case FIND_PUBPRIV_NONE:
				/* No special handling */
				break;

			case FIND_PUBPRIV_CERTVIAKEY:
				/* Remember that although we've got a private key object, we 
				   only need it to find the associated certificate and not 
				   finding an associated certificate is an error */
				certViaPrivateKey = TRUE;
				break;

			case FIND_PUBPRIV_KEYVIACERT:
				/* Remember that we've already got a certificate to attach 
				   to the private key */
				privateKeyViaCert = TRUE;
				break;

			default:
				retIntError();
			}
		}

	/* If we're looking for any kind of private key and we either have an
	   explicit certificate ID but couldn't find a certificate for it or we 
	   don't have a proper ID to search on and a generic search found more 
	   than one matching object, chances are that we're after a generic 
	   decrypt key.  The former only occurs in misconfigured or limited-
	   memory tokens, the latter only in rare tokens that store more than 
	   one private key, typically one for signing and one for verification.  
	   
	   If either of these cases occur then we try again looking specifically 
	   for a decryption key.  Even this doesn't always work, there are some
	   >1-key tokens that mark a signing key as a decryption key so we still 
	   get a CRYPT_ERROR_DUPLICATE error.
	   
	   Finally, if we can't find a decryption key either, we look for an
	   unwrapping key.  This may or may not work depending on whether we 
	   have a decryption key marked as valid for unwrapping but not 
	   decryption or a key that's genuinely only valid for unwrapping, but
	   at this point we're ready to try anything */
	if( itemType == KEYMGMT_ITEM_PRIVATEKEY && \
		( keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER && \
		  cryptStatus == CRYPT_ERROR_NOTFOUND ) || \
		( cryptStatus == CRYPT_ERROR_DUPLICATE ) )
		{
		static const CK_OBJECT_CLASS privkeyClass = CKO_PRIVATE_KEY;
		static const CK_BBOOL bTrue = TRUE;
		CK_ATTRIBUTE decryptKeyTemplate[] = {
			{ CKA_CLASS, ( CK_VOID_PTR ) &privkeyClass, sizeof( CK_OBJECT_CLASS ) },
			{ CKA_DECRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) }
			};

		cryptStatus = findObject( pkcs11Info, &hObject, 
								  decryptKeyTemplate, 2 );
		if( cryptStatusError( cryptStatus ) && \
			cryptStatus != CRYPT_ERROR_DUPLICATE )
			{
			decryptKeyTemplate[ 1 ].type = CKA_UNWRAP;
			cryptStatus = findObject( pkcs11Info, &hObject, 
									  decryptKeyTemplate, 2 );
			}
		}
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Sanity check that we actually found something */
	ENSURES( hObject != CK_OBJECT_NONE );

	/* If we're just checking whether an object exists, return now.  If all 
	   we want is the key label, copy it back to the caller and exit */
	if( flags & KEYMGMT_FLAG_CHECK_ONLY )
		return( CRYPT_OK );
	if( flags & KEYMGMT_FLAG_LABEL_ONLY )
		return( getObjectLabel( pkcs11Info, hObject, auxInfo, *auxInfoLength,
								auxInfoLength ) );

	/* We found something, map the key type to a cryptlib algorithm ID,
	   determine the key size, and find its capabilities */
	cryptStatus = getMechanismInfo( pkcs11Info, hObject, 
									deviceInfo->capabilityInfoList,
									TRUE, &capabilityInfoPtr, &cryptAlgo );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = mapValue( cryptAlgo, &keySize, keySizeMapTbl, 
							FAILSAFE_ARRAYSIZE( keySizeMapTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( cryptStatus ) );
	keySizeTemplate.type = keySize;		/* For int vs. enum */
	status = C_GetAttributeValue( pkcs11Info->hSession, hObject, 
								  &keySizeTemplate, 1 );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
	keySize = keySizeTemplate.ulValueLen;

	/* Try and find a certificate that matches the key.  The process is as
	   follows:

		if certificate object found in issuerAndSerialNumber search
			-- Implies key == private key
			create native data-only certificate object
			attach certificate object to key
		else
			{
			if public key read
				{
				if certificate
					create native certificate (+context) object
				else
					create context object
				}
			else
				{
				create device privkey object, mark as "key loaded"
				if certificate
					create native data-only certificate object
					attach certificate object to key
				}
			}

	   The reason for doing things this way is given in the comments earlier
	   on in this function */
	if( privateKeyViaCert )
		{
		/* Sanity check that we actually found a certificate */
		REQUIRES( hCertificate != CK_OBJECT_NONE );

		/* We've already got the certificate object handle, instantiate a 
		   native data-only certificate from it */
		cryptStatus = getCertChain( pkcs11Info, deviceInfo->objectHandle, 
									hCertificate, &iCryptCert, FALSE );
		if( cryptStatusError( cryptStatus ) )
			return( cryptStatus );
		certPresent = TRUE;
		}
	else
		{
		cryptStatus = findCertFromObject( pkcs11Info, deviceInfo->objectHandle, 
										  hObject, &iCryptCert, 
										  ( itemType == KEYMGMT_ITEM_PUBLICKEY ) ? \
										  FINDCERT_NORMAL : FINDCERT_DATAONLY );
		if( cryptStatusError( cryptStatus ) )
			{
			/* If we get a CRYPT_ERROR_NOTFOUND this is OK since it means 
			   that there's no certificate present, however anything else is 
			   an error.  In addition if we've got a private key whose only 
			   function is to point to an associated certificate then not 
			   finding anything is also an error */
			if( cryptStatus != CRYPT_ERROR_NOTFOUND || certViaPrivateKey )
				return( cryptStatus );
			}
		else
			{
			/* We got the certificate, if we're being asked for a public key 
			   then we've created a native object to contain it so we return 
			   that */
			certPresent = TRUE;
			if( itemType == KEYMGMT_ITEM_PUBLICKEY )
				{
				*iCryptHandle = iCryptCert;
				return( CRYPT_OK );
				}
			}
		}

	/* Create the object.  If it's a public-key object we create a native
	   object for the reasons given in createNativeObject(), otherwise we
	   create a device object */
	if( itemType == KEYMGMT_ITEM_PUBLICKEY )
		{
		return( createNativeObject( pkcs11Info, iCryptHandle, hObject,
									KEYMGMT_ITEM_PUBLICKEY, cryptAlgo ) );
		}
	cryptStatus = createDeviceObject( pkcs11Info, iCryptHandle, hObject, 
									  certPresent ? iCryptCert : CRYPT_UNUSED, 
									  deviceInfo->ownerHandle, 
									  deviceInfo->objectHandle, 
									  capabilityInfoPtr, 
									  KEYMGMT_ITEM_PRIVATEKEY, cryptAlgo, 
									  keySize );
	if( cryptStatusError( cryptStatus ) && certPresent )
		krnlSendNotifier( iCryptCert, IMESSAGE_DECREFCOUNT );
	return( cryptStatus );
	}

/* Get the sequence of certificates in a chain from a device */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
static int getFirstItemFunction( INOUT DEVICE_INFO *deviceInfo, 
								 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
								 OUT int *stateInfo,
								 IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
								 IN_BUFFER( keyIDlength ) const void *keyID, 
								 IN_LENGTH_KEYID const int keyIDlength,
								 IN_ENUM( KEYMGMT_ITEM ) \
									const KEYMGMT_ITEM_TYPE itemType,
								 IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	CK_ATTRIBUTE certTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		{ CKA_ID, ( CK_VOID_PTR ) keyID, keyIDlength }
		};
	CK_OBJECT_HANDLE hCertificate;
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;
	int cryptStatus;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );
	assert( isWritePtr( stateInfo, sizeof( int ) ) );

	REQUIRES( keyIDtype == CRYPT_IKEYID_KEYID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && \
			  options < KEYMGMT_FLAG_MAX );

	/* Clear return values */
	*iCertificate = CRYPT_ERROR;
	*stateInfo = CRYPT_ERROR;

	/* Try and find the certificate with the given ID.  This should work 
	   because we've just read the ID for the indirect-import that lead to 
	   the getFirst() call.  Note that we can't use findCert() for this 
	   because it uses getCertChain() to build the full chain of 
	   certificates from the leaf, which would end up calling back to 
	   here */
	cryptStatus = findObject( pkcs11Info, &hCertificate, certTemplate, 3 );
	ENSURES( cryptStatusOK( cryptStatus ) );

	/* Instantiate the certificate from the device */
	cryptStatus = instantiateCert( pkcs11Info, hCertificate, iCertificate, 
								   ( options & KEYMGMT_FLAG_DATAONLY_CERT ) ? \
									TRUE : FALSE );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	*stateInfo = *iCertificate;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int getNextItemFunction( INOUT DEVICE_INFO *deviceInfo, 
								OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
								INOUT int *stateInfo, 
								IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	CK_ATTRIBUTE certTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		{ CKA_SUBJECT, NULL, 0 }
		};
	CK_OBJECT_HANDLE hCertificate;
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;
	DYNBUF subjectDB;
	int cryptStatus;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isWritePtr( stateInfo, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( *stateInfo ) || *stateInfo == CRYPT_ERROR );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && options < KEYMGMT_FLAG_MAX );

	/* Clear return value */
	*iCertificate = CRYPT_ERROR;

	/* If the previous certificate was the last one, there's nothing left to 
	   fetch */
	if( *stateInfo == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTFOUND );

	/* Get the issuerName of the previous certificate, which is the 
	   subjectName of the certificate that we want */
	cryptStatus = dynCreate( &subjectDB, *stateInfo, 
							 CRYPT_IATTRIBUTE_ISSUER );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	certTemplate[ 2 ].pValue = dynData( subjectDB );
	certTemplate[ 2 ].ulValueLen = dynLength( subjectDB );

	/* Get the certificate with the subject's issuer DN.  Note that we
	   can't use findCert() for this because it uses getCertChain() to
	   build the full chain of certificates from the leaf, which would end
	   up calling back to here */
	cryptStatus = findObject( pkcs11Info, &hCertificate, certTemplate, 3 );
	if( cryptStatusOK( cryptStatus ) )
		{
		cryptStatus = instantiateCert( pkcs11Info, hCertificate, iCertificate, 
									   ( options & KEYMGMT_FLAG_DATAONLY_CERT ) ? \
										 TRUE : FALSE );
		}
#ifdef PKCS11_FIND_VIA_CRYPTLIB
	else
		{
		cryptStatus = searchDeviceObjects( pkcs11Info, iCertificate, NULL,
										   CRYPT_KEYID_LAST,
										   dynData( subjectDB ), 
										   dynLength( subjectDB ), TRUE );
		}
#endif /* PKCS11_FIND_VIA_CRYPTLIB */
	dynDestroy( &subjectDB );
	if( cryptStatusError( cryptStatus ) )
		{
		*stateInfo = CRYPT_ERROR;
		return( cryptStatus );
		}
	*stateInfo = *iCertificate;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Device Access Routines							*
*																			*
****************************************************************************/

/* Set up the function pointers to the read methods */

STDC_NONNULL_ARG( ( 1 ) ) \
void initPKCS11Read( INOUT DEVICE_INFO *deviceInfo )
	{
	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	deviceInfo->getItemFunction = getItemFunction;
	deviceInfo->getFirstItemFunction = getFirstItemFunction;
	deviceInfo->getNextItemFunction = getNextItemFunction;
	}
#endif /* USE_PKCS11 */
