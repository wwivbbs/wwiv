/****************************************************************************
*																			*
*					   cryptlib PKCS #15 Get-item Routines					*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "keyset.h"
  #include "pkcs15.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs15.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS15

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Set any optional attributes that may be associated with a key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int setKeyAttributes( IN_HANDLE const CRYPT_HANDLE iCryptHandle,
							 INOUT const PKCS15_INFO *pkcs15infoPtr,
							 IN_FLAGS_Z( ACTION_PERM ) const int actionFlags )
	{
	int status;

	assert( isReadPtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( actionFlags >= ACTION_PERM_FLAG_NONE && \
			  actionFlags < ACTION_PERM_FLAG_MAX );
			  /* This check is a bit odd because normally an action flag 
			     value of ACTION_PERM_FLAG_NONE would indicate that the 
				 object can't be used, but setting it to CRYPT_UNUSED would 
				 create a value outside the normal allowed range so we use 
				 _NONE to indicate a don't-care value */

	if( actionFlags != 0 )
		{
		status = krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &actionFlags,
								  CRYPT_IATTRIBUTE_ACTIONPERMS );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( pkcs15infoPtr->openPGPKeyIDlength > 0 )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, ( MESSAGE_CAST ) pkcs15infoPtr->openPGPKeyID,
						pkcs15infoPtr->openPGPKeyIDlength );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID_OPENPGP );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( pkcs15infoPtr->validFrom > MIN_TIME_VALUE )
		{
		MESSAGE_DATA msgData;

		/* This isn't really used for anything but is required to generate
		   the OpenPGP keyID, which includes the key creation time in the
		   ID-generation process */
		setMessageData( &msgData, ( MESSAGE_CAST ) &pkcs15infoPtr->validFrom,
						sizeof( time_t ) );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_PGPVALIDITY );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

/* Get an encoded trusted certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int getTrustedCert( IN_ARRAY( noPkcs15objects ) \
							const PKCS15_INFO *pkcs15info,
						   IN_LENGTH_SHORT const int noPkcs15objects,
						   OUT_BUFFER( dataMaxLength, *dataLength ) \
								void *data, 
						   IN_LENGTH_SHORT_MIN( 16 ) \
								const int dataMaxLength, 
						   OUT_LENGTH_BOUNDED_Z( dataMaxLength ) \
								int *dataLength, 
						   const BOOLEAN resetCertIndex )
	{
	static int trustedCertIndex;
	const PKCS15_INFO *pkcs15infoPtr;
	int certStartOffset, certDataTotalSize;

	assert( isReadPtr( pkcs15info, \
					   sizeof( PKCS15_INFO ) * noPkcs15objects ) );
	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( noPkcs15objects > 0 && noPkcs15objects < MAX_INTLENGTH_SHORT );
	REQUIRES( dataMaxLength >= 16 && dataMaxLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	/* If this is the first certificate, reset the index value.  This is 
	   pretty ugly since this sort of state-information value should be 
	   stored with the caller, however there's no way to pass this back and 
	   forth in a MESSAGE_DATA without resorting to an even uglier hack and 
	   it's safe since this attribute is only ever read by the init thread 
	   when it reads the configuration data at startup */
	if( resetCertIndex )
		trustedCertIndex = 0;
	else
		{
		/* Move on to the next certificate */
		if( trustedCertIndex >= noPkcs15objects - 1 )
			return( CRYPT_ERROR_NOTFOUND );
		trustedCertIndex++;	
		}

	/* Find the next trusted certificate */
	for( pkcs15infoPtr = NULL; 
		 trustedCertIndex < noPkcs15objects && \
			trustedCertIndex < FAILSAFE_ITERATIONS_MED; trustedCertIndex++ )
		{
		if( pkcs15info[ trustedCertIndex ].implicitTrust )
			{
			pkcs15infoPtr = &pkcs15info[ trustedCertIndex ];
			break;
			}
		}
	ENSURES( trustedCertIndex < FAILSAFE_ITERATIONS_MED );
	if( pkcs15infoPtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	certStartOffset = pkcs15infoPtr->certOffset;
	certDataTotalSize = pkcs15infoPtr->certDataSize;

	/* Return the data to the caller */
	REQUIRES( rangeCheck( certStartOffset, 
						  certDataTotalSize - certStartOffset,
						  certDataTotalSize ) );
	return( attributeCopyParams( data, dataMaxLength, dataLength,
					( BYTE * ) pkcs15infoPtr->certData + certStartOffset,
					certDataTotalSize - certStartOffset ) );
	}

/* Get an encoded configuration item */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int getConfigItem( IN_ARRAY( noPkcs15objects ) \
							const PKCS15_INFO *pkcs15info,
						  IN_LENGTH_SHORT const int noPkcs15objects,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE dataType,
						  OUT_BUFFER_OPT( dataMaxLength, *dataLength ) \
								void *data, 
						  IN_LENGTH_SHORT_Z const int dataMaxLength, 
						  OUT_LENGTH_BOUNDED_Z( dataMaxLength ) \
								int *dataLength )
	{
	const PKCS15_INFO *pkcs15infoPtr;
	int dataStartOffset, dataTotalSize, i;

	assert( isReadPtr( pkcs15info, \
					   sizeof( PKCS15_INFO ) * noPkcs15objects ) );
	assert( ( data == NULL && dataMaxLength == 0 ) || \
			isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( noPkcs15objects > 0 && noPkcs15objects < MAX_INTLENGTH_SHORT );
	REQUIRES( dataType == CRYPT_IATTRIBUTE_CONFIGDATA || \
			  dataType == CRYPT_IATTRIBUTE_USERINDEX || \
			  dataType == CRYPT_IATTRIBUTE_USERINFO );
	REQUIRES( ( data == NULL && dataMaxLength == 0 ) || \
			  ( data != NULL && \
				dataMaxLength >= 16 && \
				dataMaxLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return values */
	*dataLength = 0;
	if( data != NULL )
		memset( data, 0, min( 16, dataMaxLength ) );

	/* Find the particular data type that we're looking for */
	for( pkcs15infoPtr = NULL, i = 0; 
		 i < noPkcs15objects && i < FAILSAFE_ITERATIONS_MED; i++ )
		{
		if( ( pkcs15info[ i ].type == PKCS15_SUBTYPE_DATA && \
			  pkcs15info[ i ].dataType == dataType ) )
			{
			pkcs15infoPtr = &pkcs15info[ i ];
			break;
			}
		}
	ENSURES( i < FAILSAFE_ITERATIONS_MED );
	if( pkcs15infoPtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	dataStartOffset = pkcs15infoPtr->dataOffset;
	dataTotalSize = pkcs15infoPtr->dataDataSize;

	/* If it's just a length check, we're done */
	if( data == NULL )
		{
		*dataLength = dataTotalSize - dataStartOffset;
		return( CRYPT_OK );
		}

	/* Return the data to the caller */
	REQUIRES( rangeCheck( dataStartOffset, 
						  dataTotalSize - dataStartOffset,
						  dataTotalSize ) );
	return( attributeCopyParams( data, dataMaxLength, dataLength,
					( BYTE * ) pkcs15infoPtr->dataData + dataStartOffset,
					dataTotalSize - dataStartOffset ) );
	}

/****************************************************************************
*																			*
*									Get a Key								*
*																			*
****************************************************************************/

/* Read key data from a PKCS #15 collection */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
static int getItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							OUT_HANDLE_OPT CRYPT_HANDLE *iCryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							IN_BUFFER( keyIDlength ) const void *keyID, 
							IN_LENGTH_KEYID const int keyIDlength,
							IN_OPT void *auxInfo, 
							INOUT_OPT int *auxInfoLength,
							IN_FLAGS_Z( KEYMGMT ) const int flags )
	{
	CRYPT_CERTIFICATE iDataCert = CRYPT_ERROR;
	CRYPT_CONTEXT iCryptContext;
	const PKCS15_INFO *pkcs15infoPtr;
	MESSAGE_DATA msgData;
	const BOOLEAN publicComponentsOnly = \
					( itemType != KEYMGMT_ITEM_PRIVATEKEY ) ? TRUE : FALSE;
	const BOOLEAN isStorageObject = \
			( keysetInfoPtr->keysetFile->iHardwareDevice != CRYPT_UNUSED ) ? \
			TRUE : FALSE;
	const int auxInfoMaxLength = *auxInfoLength;
	int pubkeyActionFlags = 0, privkeyActionFlags = 0, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCryptHandle, sizeof( CRYPT_HANDLE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );
	assert( ( auxInfo == NULL && auxInfoMaxLength == 0 ) || \
			isReadPtr( auxInfo, auxInfoMaxLength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_PGPKEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( ( auxInfo == NULL && *auxInfoLength == 0 ) || \
			  ( auxInfo != NULL && \
				*auxInfoLength > 0 && \
				*auxInfoLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( flags >= KEYMGMT_FLAG_NONE && flags < KEYMGMT_FLAG_MAX );

	/* Clear return values */
	*iCryptHandle = CRYPT_ERROR;

	/* Locate the appropriate object in the PKCS #15 collection and make 
	   sure that the components that we need are present: Either a public 
	   key or a certificate for any type of read, and a private key as well 
	   for a private-key read */
	if( keyIDlength == 6 && !strCompare( keyID, "[none]", 6 ) )
		{
		/* It's a wildcard read, locate the first private-key object */
		pkcs15infoPtr = findEntry( keysetInfoPtr->keyData, 
								   keysetInfoPtr->keyDataNoObjects, 
								   keyIDtype, NULL, 0, flags, TRUE );
		}
	else
		{
		pkcs15infoPtr = findEntry( keysetInfoPtr->keyData, 
								   keysetInfoPtr->keyDataNoObjects, 
								   keyIDtype, keyID, keyIDlength, flags, FALSE );
		}
	if( pkcs15infoPtr == NULL )
		{
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "No information present for this ID" ) );
		}
	if( pkcs15infoPtr->pubKeyData == NULL && \
		pkcs15infoPtr->certData == NULL )
		{
		/* There's not enough information present to get a public key or the
		   public portions of a private key */
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "No public key or certificate data present for this ID" ) );
		}
	if( !publicComponentsOnly && pkcs15infoPtr->privKeyData == NULL )
		{
		/* There's not enough information present to get a private key */
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "No private key data present for this ID" ) );
		}

	/* If we're just checking whether an object exists, return now.  If all
	   that we want is the key label, copy it back to the caller and exit */
	if( flags & KEYMGMT_FLAG_CHECK_ONLY )
		return( CRYPT_OK );
	if( flags & KEYMGMT_FLAG_LABEL_ONLY )
		{
		return( attributeCopyParams( auxInfo, auxInfoMaxLength, 
									 auxInfoLength, pkcs15infoPtr->label, 
									 pkcs15infoPtr->labelLength ) );
		}

	/* If we're reading the private key and this isn't a PKCS #15 object 
	   store (which only contains metadata for private keys), make sure that 
	   the user has supplied a password.  This is checked by the kernel but 
	   we perform another check here just to be safe*/
	if( !publicComponentsOnly && !isStorageObject && auxInfo == NULL )
		return( CRYPT_ERROR_WRONGKEY );

	/* Read the public components */
	status = readPublicKeyComponents( pkcs15infoPtr, keysetInfoPtr->objectHandle,
									  keyIDtype, keyID, keyIDlength, 
									  publicComponentsOnly,
									  isStorageObject ? \
										keysetInfoPtr->keysetFile->iHardwareDevice : \
										SYSTEM_OBJECT_HANDLE,
									  &iCryptContext, &iDataCert,
									  &pubkeyActionFlags, 
									  &privkeyActionFlags, KEYSET_ERRINFO );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're only interested in the public components, set the key
	   permissions and exit */
	if( publicComponentsOnly )
		{
		status = setKeyAttributes( iCryptContext, pkcs15infoPtr,
								   ( pkcs15infoPtr->pubKeyData != NULL ) ? \
									 pubkeyActionFlags : 0 );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
			retExt( status, 
					( status, KEYSET_ERRINFO, 
					  "Couldn't set PKCS #15 permitted action flags for "
					  "the key" ) );
			}
		*iCryptHandle = iCryptContext;

		return( CRYPT_OK );
		}

	REQUIRES( ( pkcs15infoPtr->pubKeyData != NULL || \
				pkcs15infoPtr->certData != NULL ) && \
			  pkcs15infoPtr->privKeyData != NULL );

	/* Set the key label and read the private key components.  We have to 
	   set the label before we load the key or the key load will be blocked 
	   by the kernel.  In addition if this keyset is a device object store
	   we have to set the label as a CRYPT_IATTRIBUTE_EXISTINGLABEL 
	   otherwise the existing item's label will be erroneously reported as
	   a duplicate */
	if( pkcs15infoPtr->labelLength > 0 )
		{ setMessageData( &msgData, ( MESSAGE_CAST ) pkcs15infoPtr->label,
						  min( pkcs15infoPtr->labelLength, \
							   CRYPT_MAX_TEXTSIZE ) ); }
	else
		{ setMessageData( &msgData, ( MESSAGE_CAST ) "Dummy label", 11 ); }
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, isStorageObject ? \
								CRYPT_IATTRIBUTE_EXISTINGLABEL : \
								CRYPT_CTXINFO_LABEL );
	if( cryptStatusOK( status ) )
		{
		status = readPrivateKeyComponents( pkcs15infoPtr, iCryptContext, 
								auxInfo, *auxInfoLength, isStorageObject,
								KEYSET_ERRINFO );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		if( iDataCert != CRYPT_ERROR )
			krnlSendNotifier( iDataCert, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Connect the data-only certificate object to the context if it exists.
	   This is an internal object used only by the context so we tell the
	   kernel to mark it as owned by the context only */
	if( iDataCert != CRYPT_ERROR )
		{
		status = krnlSendMessage( iCryptContext, IMESSAGE_SETDEPENDENT, 
								  &iDataCert, SETDEP_OPTION_NOINCREF );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
			krnlSendNotifier( iDataCert, IMESSAGE_DECREFCOUNT );
			retExt( status, 
					( status, KEYSET_ERRINFO, 
					  "Couldn't attach certificate to key" ) );
			}
		}

	/* Set the permitted action flags */
	status = setKeyAttributes( iCryptContext, pkcs15infoPtr,
							   privkeyActionFlags );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, MESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, KEYSET_ERRINFO, 
				  "Couldn't set PKCS #15 permitted action flags for the "
				  "key" ) );
		}

	*iCryptHandle = iCryptContext;
	return( CRYPT_OK );
	}

/* Read special data from a PKCS #15 collection */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
static int getSpecialItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
								   IN_ATTRIBUTE \
										const CRYPT_ATTRIBUTE_TYPE dataType,
								   OUT_BUFFER_OPT( dataMaxLength, *dataLength ) \
										void *data,
								   IN_LENGTH_SHORT_Z const int dataMaxLength,
								   OUT_LENGTH_BOUNDED_Z( dataMaxLength ) \
										int *dataLength )
	{
	assert( ( data == NULL && dataMaxLength == 0 ) || \
			isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( dataType == CRYPT_IATTRIBUTE_CONFIGDATA || \
			  dataType == CRYPT_IATTRIBUTE_USERINDEX || \
			  dataType == CRYPT_IATTRIBUTE_USERINFO || \
			  dataType == CRYPT_IATTRIBUTE_TRUSTEDCERT || \
			  dataType == CRYPT_IATTRIBUTE_TRUSTEDCERT_NEXT );
	REQUIRES( ( data == NULL && dataMaxLength == 0 ) || \
			  ( data != NULL && \
			    dataMaxLength >= 16 && dataMaxLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( dataType != CRYPT_IATTRIBUTE_TRUSTEDCERT && \
				dataType != CRYPT_IATTRIBUTE_TRUSTEDCERT_NEXT ) || \
			  data != NULL );

	/* Clear return values */
	if( data != NULL )
		memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	/* If we're being asked for pre-encoded trusted certificate data, return 
	   it to the caller */
	if( dataType == CRYPT_IATTRIBUTE_TRUSTEDCERT || \
		dataType == CRYPT_IATTRIBUTE_TRUSTEDCERT_NEXT )
		{
		return( getTrustedCert( keysetInfoPtr->keyData, 
								keysetInfoPtr->keyDataNoObjects, 
								data, dataMaxLength, dataLength,
								( dataType == CRYPT_IATTRIBUTE_TRUSTEDCERT ) ? \
									TRUE : FALSE ) );
		}

	/* Return a configuration data item */
	return( getConfigItem( keysetInfoPtr->keyData, 
						   keysetInfoPtr->keyDataNoObjects, dataType, 
						   data, dataMaxLength, dataLength ) );
	}

/* Fetch a sequence of certificates.  These functions are called indirectly 
   by the certificate code to fetch the first and subsequent certificates in 
   a certificate chain.  The 'stateInfo' value is handled as an OUT parameter
   rather than the more obvious INOUT since the state is managed explicitly 
   by the caller, who uses it to pass in a next-key-ID reference instead of 
   a current-key-ID one */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 6, 10 ) ) \
static int getItem( INOUT_ARRAY( noPkcs15objects ) PKCS15_INFO *pkcs15info, 
					IN_LENGTH_SHORT const int noPkcs15objects, 
					OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate, 
					OUT int *stateInfo, 
					IN_KEYID const CRYPT_KEYID_TYPE keyIDtype, 
					IN_BUFFER( keyIDlength ) const void *keyID, 
					IN_LENGTH_KEYID const int keyIDlength, 
					IN_ENUM( KEYMGMT_ITEM ) const KEYMGMT_ITEM_TYPE itemType, 
					IN_FLAGS_Z( KEYMGMT ) const int options, 
					INOUT ERROR_INFO *errorInfo )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	const PKCS15_INFO *pkcs15infoPtr;
	BYTE *certDataPtr;
	int certStartOffset, certDataTotalSize, tag, status;

	assert( isWritePtr( pkcs15info, \
						sizeof( PKCS15_INFO ) * noPkcs15objects ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( stateInfo, sizeof( int ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( noPkcs15objects > 0 && noPkcs15objects < MAX_INTLENGTH_SHORT );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_PGPKEYID || \
			  keyIDtype == CRYPT_IKEYID_SUBJECTID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && \
			  options < KEYMGMT_FLAG_MAX );
	REQUIRES( ( options & KEYMGMT_MASK_USAGEOPTIONS ) != \
			  KEYMGMT_MASK_USAGEOPTIONS );
	REQUIRES( errorInfo != NULL );

	/* Clear return values */
	*iCertificate = CRYPT_ERROR;
	*stateInfo = CRYPT_ERROR;

	/* Find the appropriate entry based on the ID */
	if( keyIDtype != CRYPT_IKEYID_SUBJECTID && \
		keyIDlength == 6 && !strCompare( keyID, "[none]", 6 ) )
		{
		/* It's a findFirst() called (findNext() always uses 
		   CRYPT_IKEYID_SUBJECTID as the ID) and it's a wildcard read, 
		   locate the first object associated with a private key */
		pkcs15infoPtr = findEntry( pkcs15info, noPkcs15objects, keyIDtype, 
								   NULL, 0, options, TRUE );
		}
	else
		{
		pkcs15infoPtr = findEntry( pkcs15info, noPkcs15objects, keyIDtype, 
								   keyID, keyIDlength, options, FALSE );
		}
	if( pkcs15infoPtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	*stateInfo = pkcs15infoPtr->index;

	/* When we get called it's via a callback from iCryptImportCertIndirect()
	   that's called as part of a private-key read, and that only gets called
	   if there's certificate data present, the following check ensures that 
	   this is the case */
	ENSURES( pkcs15infoPtr->certData != NULL );

	/* Import the certificate.  This gets somewhat ugly because early drafts 
	   of PKCS #15 wrote the certificate as is while the final version 
	   wrapped it up in a [0] IMPLICIT tag so we can run into both the 
	   original untagged SEQUENCE form and the newer [0] IMPLICIT SEQUENCE.  
	   To handle this we dynamically replace the tag with the standard 
	   SEQUENCE tag and reinstate the original afterwards, this is easier 
	   than trying to pass the special-case decoding requirement down 
	   through the kernel call */
	certDataPtr = ( BYTE * ) pkcs15infoPtr->certData + \
				  pkcs15infoPtr->certOffset;
	certStartOffset = pkcs15infoPtr->certOffset;
	certDataTotalSize = pkcs15infoPtr->certDataSize;
	tag = *certDataPtr;
	*certDataPtr = BER_SEQUENCE;
	REQUIRES( rangeCheck( certStartOffset, 
						  certDataTotalSize - certStartOffset,
						  certDataTotalSize ) );
	setMessageCreateObjectIndirectInfoEx( &createInfo, certDataPtr,
							certDataTotalSize - certStartOffset,
							CRYPT_CERTTYPE_CERTIFICATE,
							( options & KEYMGMT_FLAG_DATAONLY_CERT ) ? \
								KEYMGMT_FLAG_DATAONLY_CERT : \
								KEYMGMT_FLAG_NONE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT, 
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	*certDataPtr = intToByte( tag );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't create certificate from stored certificate "
				  "data" ) );
		}
	if( !( ( ( keyIDtype == CRYPT_IKEYID_KEYID || \
			   keyIDtype == CRYPT_IKEYID_PGPKEYID ) && \
			 ( options & KEYMGMT_FLAG_DATAONLY_CERT ) ) ) )
		{
		/* Make sure that the certificate that we got back is what we 
		   actually asked for.  We can't do this for key IDs with data-only 
		   certificates since the IDs aren't present in the certificate
		   but are generated dynamically in the associated context */
		status = iCryptVerifyID( createInfo.cryptHandle, keyIDtype, keyID, 
								 keyIDlength );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, 
							  IMESSAGE_DECREFCOUNT );
			retExt( status, 
					( status, errorInfo, 
					  "Certificate fetched for ID type %d doesn't actually "
					  "correspond to the given ID", keyIDtype ) );
			}
		}
	*iCertificate = createInfo.cryptHandle;
	if( pkcs15infoPtr->validFrom <= MIN_TIME_VALUE )
		{
		/* Perform an opportunistic update of the validity information if 
		   this hasn't already been set.  Since this is a purely 
		   opportunistic update we ignore any return value */
		( void ) getValidityInfo( pkcs15info, createInfo.cryptHandle );
		}
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 6 ) ) \
static int getFirstItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
								 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
								 OUT int *stateInfo,
								 IN_ENUM( KEYMGMT_ITEM ) \
									const KEYMGMT_ITEM_TYPE itemType,
								 IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
								 IN_BUFFER( keyIDlength ) const void *keyID, 
								 IN_LENGTH_KEYID const int keyIDlength,
								 IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	PKCS15_INFO *pkcs15info = keysetInfoPtr->keyData;
	const int noPkcs15objects = keysetInfoPtr->keyDataNoObjects;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( pkcs15info, \
						sizeof( PKCS15_INFO ) * noPkcs15objects ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( stateInfo, sizeof( int ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_PGPKEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && \
			  options < KEYMGMT_FLAG_MAX );
	REQUIRES( ( options & KEYMGMT_MASK_USAGEOPTIONS ) != \
			  KEYMGMT_MASK_USAGEOPTIONS );

	/* Clear return values */
	*iCertificate = CRYPT_ERROR;
	*stateInfo = CRYPT_ERROR;

	return( getItem( pkcs15info, noPkcs15objects, iCertificate, stateInfo,
					 keyIDtype, keyID, keyIDlength, itemType, options,
					 KEYSET_ERRINFO ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int getNextItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
								OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
								INOUT int *stateInfo, 
								IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	PKCS15_INFO *pkcs15info = keysetInfoPtr->keyData;
	const int noPkcs15objects = keysetInfoPtr->keyDataNoObjects;
	const int lastEntry = *stateInfo;
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isWritePtr( stateInfo, sizeof( int ) ) );
	assert( isWritePtr( pkcs15info, \
						sizeof( PKCS15_INFO ) * noPkcs15objects ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && options < KEYMGMT_FLAG_MAX && \
			  ( options & ~KEYMGMT_MASK_CERTOPTIONS ) == 0 );
	REQUIRES( ( lastEntry >= 0 && lastEntry < noPkcs15objects ) || \
			  lastEntry == CRYPT_ERROR );

	/* Clear return values */
	*iCertificate = CRYPT_ERROR;
	*stateInfo = CRYPT_ERROR;

	/* If the previous certificate was the last one, there's nothing left to 
	   fetch */
	if( lastEntry == CRYPT_ERROR )
		{
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "No more items present" ) );
		}
	ENSURES( lastEntry >= 0 && lastEntry < noPkcs15objects );

	/* If there's no index information present to locate other certificates, 
	   we can't go any further */
	if( pkcs15info[ lastEntry ].issuerNameIDlength <= 0 )
		{
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "No index information available to locate other "
				  "certificates" ) );
		}

	/* Find the certificate for which the subjectNameID matches this 
	   certificate's issuerNameID */
	status = getItem( pkcs15info, noPkcs15objects, iCertificate, stateInfo,
					  CRYPT_IKEYID_SUBJECTID, 
					  pkcs15info[ lastEntry ].issuerNameID,
					  pkcs15info[ lastEntry ].issuerNameIDlength,
					  KEYMGMT_ITEM_PUBLICKEY, options, KEYSET_ERRINFO );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( lastEntry != *stateInfo );		/* Loop detection */

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Keyset Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initPKCS15get( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );

	/* Set the access method pointers */
	keysetInfoPtr->getItemFunction = getItemFunction;
	keysetInfoPtr->getSpecialItemFunction = getSpecialItemFunction;
	keysetInfoPtr->getFirstItemFunction = getFirstItemFunction;
	keysetInfoPtr->getNextItemFunction = getNextItemFunction;

	return( CRYPT_OK );
	}
#endif /* USE_PKCS15 */
