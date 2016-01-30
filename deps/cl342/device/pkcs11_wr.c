/****************************************************************************
*																			*
*					  cryptlib PKCS #11 Item Write Routines					*
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

#ifdef USE_PKCS11

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Convert a time_t to a PKCS #11 CK_DATE */

STDC_NONNULL_ARG( ( 1 ) ) \
static void convertDate( OUT CK_DATE *date, const time_t theTime )
	{
	STREAM stream;
	BYTE dateBuffer[ 32 + 8 ];
	int cryptStatus;

	assert( isWritePtr( date, sizeof( CK_DATE ) ) );

	/* Clear return value */
	memset( date, 0, sizeof( CK_DATE ) );

	/* Convert the time_t value to an ASN.1 time string that we can use to
	   populate the CK_DATE fields, which are stored as ASCII text strings */
	sMemOpen( &stream, dateBuffer, 32 );
	cryptStatus = writeGeneralizedTime( &stream, theTime, DEFAULT_TAG );
	sMemDisconnect( &stream );
	ENSURES_V( cryptStatusOK( cryptStatus ) );
	memcpy( &date->year, dateBuffer + 2, 4 );
	memcpy( &date->month, dateBuffer + 6, 2 );
	memcpy( &date->day, dateBuffer + 8, 2 );
	}

/****************************************************************************
*																			*
*						 	Certificate R/W Routines						*
*																			*
****************************************************************************/

/* Set up certificate information and load it into the device */

#define addTemplateValue( certTemplatePtr, valueType, valuePtr, valueLen ) \
		{ \
		( certTemplatePtr ).type = valueType; \
		( certTemplatePtr ).pValue = valuePtr; \
		( certTemplatePtr ).ulValueLen = valueLen; \
		}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int updateCertificate( INOUT PKCS11_INFO *pkcs11Info, 
							  IN_HANDLE const CRYPT_HANDLE iCryptHandle,
							  const BOOLEAN isLeafCert )
	{
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	static const CK_BBOOL bTrue = TRUE;
	CK_DATE startDate, endDate;
	CK_ATTRIBUTE certTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_ID, NULL_PTR, 0 },
		{ CKA_SUBJECT, NULL_PTR, 0 },
		{ CKA_ISSUER, NULL_PTR, 0 },
		{ CKA_SERIAL_NUMBER, NULL_PTR, 0 },
		{ CKA_VALUE, NULL_PTR, 0 },
		/* Optional fields, filled in if required and the driver supports this */
		{ CKA_NONE, NULL_PTR, 0 },	/*  8 */
		{ CKA_NONE, NULL_PTR, 0 },	/*  9 */
		{ CKA_NONE, NULL_PTR, 0 },	/* 10 */
		{ CKA_NONE, NULL_PTR, 0 },	/* 11 */
		};
	CK_OBJECT_HANDLE hObject;
	CK_RV status;
	MESSAGE_DATA msgData;
	DYNBUF subjectDB, iAndSDB, certDB;
	BYTE keyID[ CRYPT_MAX_HASHSIZE + 8 ];
	BOOLEAN hasURL = FALSE;
	time_t theTime;
	char label[ CRYPT_MAX_TEXTSIZE + 8 ], uri[ MAX_URL_SIZE + 8 ];
	int templateCount = 8, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptHandle ) );

	/* Get the keyID from the certificate */
	setMessageData( &msgData, keyID, CRYPT_MAX_HASHSIZE );
	cryptStatus = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								   &msgData, CRYPT_IATTRIBUTE_KEYID );
	if( cryptStatusError( cryptStatus ) )
		return( CRYPT_ARGERROR_NUM1 );
	certTemplate[ 3 ].pValue = msgData.data;
	certTemplate[ 3 ].ulValueLen = msgData.length;

	/* If it's a leaf certificate, use the keyID to locate the corresponding 
	   public or private key object.  This is used as a check to ensure that 
	   the certificate corresponds to a key in the device.  In theory this 
	   would allow us to read the label from the key so that we can reuse it 
	   for the certificate, but there doesn't seem to be any good reason for 
	   this and it could lead to problems with multiple certificates with the 
	   same labels so we don't do it */
	if( isLeafCert )
		{
		static const CK_OBJECT_CLASS privkeyClass = CKO_PRIVATE_KEY;
		static const CK_OBJECT_CLASS pubkeyClass = CKO_PUBLIC_KEY;
		CK_ATTRIBUTE keyTemplate[] = {
			{ CKA_CLASS, ( CK_VOID_PTR ) &privkeyClass, sizeof( CK_OBJECT_CLASS ) },
			{ CKA_ID, NULL_PTR, 0 }
			};

		keyTemplate[ 1 ].pValue = certTemplate[ 3 ].pValue;
		keyTemplate[ 1 ].ulValueLen = certTemplate[ 3 ].ulValueLen;
		cryptStatus = findObject( pkcs11Info, &hObject, keyTemplate, 2 );
		if( cryptStatusError( cryptStatus ) )
			{
			/* Couldn't find a private key with this ID, try for a public key */
			keyTemplate[ 0 ].pValue = ( CK_VOID_PTR ) &pubkeyClass;
			cryptStatus = findObject( pkcs11Info, &hObject, keyTemplate, 2 );
			}
		if( cryptStatusError( cryptStatus ) )
			return( CRYPT_ARGERROR_NUM1 );
		}

	/* Get the validFrom and validTo dates.  These aren't currently used for
	   anything, but could be used in the future to handle superceded 
	   certificates in the same way that it's done for PKCS #15 keysets */
	setMessageData( &msgData, &theTime, sizeof( time_t ) );
	cryptStatus = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								   &msgData, CRYPT_CERTINFO_VALIDFROM );
	if( cryptStatusOK( cryptStatus ) )
		{
		convertDate( &startDate, theTime );
		setMessageData( &msgData, &theTime, sizeof( time_t ) );
		cryptStatus = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
									   &msgData, CRYPT_CERTINFO_VALIDTO );
		}
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	convertDate( &endDate, theTime );

	/* Get the subjectName and issuerAndSerialNumber from the certificate */
	cryptStatus = dynCreate( &subjectDB, iCryptHandle, 
							 CRYPT_IATTRIBUTE_SUBJECT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = dynCreate( &iAndSDB, iCryptHandle, 
							 CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
	if( cryptStatusError( cryptStatus ) )
		{
		dynDestroy( &subjectDB );
		return( cryptStatus );
		}
	certTemplate[ 4 ].pValue = dynData( subjectDB );
	certTemplate[ 4 ].ulValueLen = dynLength( subjectDB );
	cryptStatus = addIAndSToTemplate( &certTemplate[ 5 ], dynData( iAndSDB ), 
									  dynLength( iAndSDB ) );
	if( cryptStatusError( cryptStatus ) )
		{
		dynDestroy( &subjectDB );
		dynDestroy( &iAndSDB );
		return( cryptStatus );
		}

	/* Get the certificate data */
	cryptStatus = dynCreateCert( &certDB, iCryptHandle, 
								 CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusError( cryptStatus ) )
		{
		dynDestroy( &subjectDB );
		dynDestroy( &iAndSDB );
		return( cryptStatus );
		}
	certTemplate[ 7 ].pValue = dynData( certDB );
	certTemplate[ 7 ].ulValueLen = dynLength( certDB );

	/* Get the certificate holder name (label) from the certificate if 
	  available */
	setMessageData( &msgData, label, CRYPT_MAX_TEXTSIZE  );
	cryptStatus = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								   &msgData, CRYPT_IATTRIBUTE_HOLDERNAME );
	if( cryptStatusOK( cryptStatus ) )
		{
		/* We've found a holder name, use it as the certificate object 
		   label */
		addTemplateValue( certTemplate[ templateCount ], 
						  CKA_LABEL, msgData.data, msgData.length );
		templateCount++;
		}

	/* Add the certificate dates.  These have to be located between the 
	   label and the URI so that we can selectively back out the attributes 
	   that don't work for this driver, see the comments further down for 
	   more details */
	addTemplateValue( certTemplate[ templateCount ], 
					  CKA_START_DATE, ( CK_VOID_PTR ) &startDate, sizeof( CK_DATE ) );
	templateCount++;
	addTemplateValue( certTemplate[ templateCount ], 
					  CKA_END_DATE, ( CK_VOID_PTR ) &endDate, sizeof( CK_DATE ) );
	templateCount++;

	/* Get the URI from the certificate if available */
	setMessageData( &msgData, uri, MAX_URL_SIZE );
	cryptStatus = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								   &msgData, CRYPT_IATTRIBUTE_HOLDERURI );
	if( cryptStatusOK( cryptStatus ) )
		{
		/* We've found a holder URI, use it as the certificate object URL */
		addTemplateValue( certTemplate[ templateCount ], 
						  CKA_URL, msgData.data, msgData.length );
		templateCount++;
		hasURL = TRUE;
		}

	/* Reset the status value, which may contain error values due to not 
	   finding various object attributes above */
	cryptStatus = CRYPT_OK;

	/* We've finally got everything available, try and update the device with
	   the certificate data.  In theory we should also set CKA_PRIVATE = FALSE
	   but the Dallas iButton driver doesn't allow this so we have to rely on
	   drivers doing the right thing with the default setting */
	status = C_CreateObject( pkcs11Info->hSession,
							 ( CK_ATTRIBUTE_PTR ) certTemplate, templateCount, 
							 &hObject );
	if( hasURL && ( status == CKR_TEMPLATE_INCONSISTENT || \
					status == CKR_ATTRIBUTE_TYPE_INVALID ) )
		{
		/* Support for the PKCS #11 v2.20 attribute CKA_URL is pretty hit-
		   and-miss, some drivers from ca.2000 support it but others from 
		   ca.2007 still don't so if we get a CKR_ATTRIBUTE_TYPE_INVALID 
		   return code we try again without the CKA_URL */
		templateCount--;
		status = C_CreateObject( pkcs11Info->hSession,
								 ( CK_ATTRIBUTE_PTR ) certTemplate, 
								 templateCount, &hObject );
		}
	if( status == CKR_TEMPLATE_INCONSISTENT || \
		status == CKR_ATTRIBUTE_TYPE_INVALID )
		{
		/* Even support for dates is hit-and-miss so if we're still getting
		   CKR_ATTRIBUTE_TYPE_INVALID we try again without the 
		   CKA_START_DATE/CKA_END_DATE */
		templateCount -= 2;
		status = C_CreateObject( pkcs11Info->hSession,
								 ( CK_ATTRIBUTE_PTR ) certTemplate, 
								 templateCount, &hObject );
		}
	if( status != CKR_OK )
		cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );

	/* Clean up */
	dynDestroy( &subjectDB );
	dynDestroy( &iAndSDB );
	dynDestroy( &certDB );
	return( cryptStatus );
	}

/* Update a device using the certificates in a certificate chain */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int updateCertChain( INOUT PKCS11_INFO *pkcs11Info, 
							IN_HANDLE const CRYPT_CERTIFICATE iCryptCert )
	{
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	CK_ATTRIBUTE certTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		{ CKA_ISSUER, NULL_PTR, 0 },
		{ CKA_SERIAL_NUMBER, NULL_PTR, 0 },
		};
	BOOLEAN isLeafCert = TRUE, seenNonDuplicate = FALSE;
	int value, iterationCount, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptCert ) );

	/* If we've been passed a standalone certificate, check whether it's 
	   implicitly trusted, which allows it to be added without requiring the 
	   presence of a corresponding public/private key in the device */
	cryptStatus = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE, &value, 
								   CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusError( cryptStatus ) )
		{
		return( ( cryptStatus == CRYPT_ARGERROR_OBJECT ) ? \
				CRYPT_ARGERROR_NUM1 : cryptStatus );
		}
	if( value == CRYPT_CERTTYPE_CERTIFICATE )
		{
		cryptStatus = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE,
									   &value, 
									   CRYPT_CERTINFO_TRUSTED_IMPLICIT );
		if( cryptStatusError( cryptStatus ) )
			return( CRYPT_ARGERROR_NUM1 );

		/* If the certificate is implicitly trusted we indicate that it's 
		   (effectively) a non-leaf certificate so that it can be added even 
		   if there's no corresponding key already in the device */
		if( value )
			isLeafCert = FALSE;
		}

	/* Add each certificate in the chain to the device */
	for( iterationCount = 0; iterationCount < FAILSAFE_ITERATIONS_MED; 
		 iterationCount++ )
		{
		CK_OBJECT_HANDLE hObject;
		DYNBUF iAndSDB;

		/* If the certificate is already present, don't do anything */
		cryptStatus = dynCreate( &iAndSDB, iCryptCert, 
								 CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
		if( cryptStatusError( cryptStatus ) )
			return( cryptStatus );
		cryptStatus = addIAndSToTemplate( &certTemplate[ 2 ], 
										  dynData( iAndSDB ), 
										  dynLength( iAndSDB ) );
		if( cryptStatusError( cryptStatus ) )
			{
			/* In theory we could simply skip any certificates for which we 
			   can't decode the iAndS, but in practice it's probably better 
			   to fail and warn the user than to continue with only some 
			   certificates added */
			dynDestroy( &iAndSDB );
			return( cryptStatus );
			}
		cryptStatus = findObject( pkcs11Info, &hObject, certTemplate, 4 );
		dynDestroy( &iAndSDB );
		if( cryptStatusError( cryptStatus ) )
			{
			/* The certificate isn't already present, write it */
			cryptStatus = updateCertificate( pkcs11Info, iCryptCert, 
											 isLeafCert );
			if( cryptStatusError( cryptStatus ) )
				return( cryptStatus );
			isLeafCert = FALSE;
			seenNonDuplicate = TRUE;
			}

		/* Try and move to the next certificate */
		cryptStatus = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
									   MESSAGE_VALUE_CURSORNEXT,
									   CRYPT_CERTINFO_CURRENT_CERTIFICATE );
		if( cryptStatusError( cryptStatus ) )
			break;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	
	return( seenNonDuplicate ? CRYPT_OK : CRYPT_ERROR_DUPLICATE );
	}

/****************************************************************************
*																			*
*						 	Write an Item to a Device						*
*																			*
****************************************************************************/

/* Update a device with a certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setItemFunction( INOUT DEVICE_INFO *deviceInfo, 
							IN_HANDLE const CRYPT_HANDLE iCryptHandle )
	{
	CRYPT_CERTIFICATE iCryptCert;
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;
	int value, cryptStatus;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptHandle ) );

	/* If the certificate isn't signed then we can't store it in this 
	   state */
	cryptStatus = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE,
								   &value, CRYPT_CERTINFO_IMMUTABLE );
	if( cryptStatusError( cryptStatus ) || !value )
		return( CRYPT_ERROR_NOTINITED );

	/* Lock the certificate for our exclusive use (in case it's a 
	   certificate chain we also select the first certificate in the 
	   chain), update the device with the certificate, and unlock it to 
	   allow others access */
	cryptStatus = krnlSendMessage( iCryptHandle, IMESSAGE_GETDEPENDENT, 
								   &iCryptCert, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
									   MESSAGE_VALUE_TRUE, 
									   CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
								   MESSAGE_VALUE_CURSORFIRST, 
								   CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = updateCertChain( pkcs11Info, iCryptCert );
	( void ) krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
							  MESSAGE_VALUE_FALSE, CRYPT_IATTRIBUTE_LOCKED );

	return( cryptStatus );
	}

/****************************************************************************
*																			*
*						 	Delete an Item from a Device					*
*																			*
****************************************************************************/

/* Delete an object in a device */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int deleteItemFunction( INOUT DEVICE_INFO *deviceInfo,
							   IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							   IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							   IN_BUFFER( keyIDlength ) const void *keyID, 
							   IN_LENGTH_KEYID const int keyIDlength )
	{
	static const CK_OBJECT_CLASS pubkeyClass = CKO_PUBLIC_KEY;
	static const CK_OBJECT_CLASS privkeyClass = CKO_PRIVATE_KEY;
	static const CK_OBJECT_CLASS secKeyClass = CKO_SECRET_KEY;
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	CK_ATTRIBUTE certTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		{ CKA_LABEL, ( CK_VOID_PTR ) keyID, keyIDlength }
		};
	CK_ATTRIBUTE keyTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &pubkeyClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_LABEL, ( CK_VOID_PTR ) keyID, keyIDlength }
		};
	CK_OBJECT_HANDLE hPrivkey = CK_OBJECT_NONE, hCertificate = CK_OBJECT_NONE;
	CK_OBJECT_HANDLE hPubkey = CK_OBJECT_NONE, hSecretKey = CK_OBJECT_NONE;
	CK_RV status;
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;
	int cryptStatus;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY || \
			  itemType == KEYMGMT_ITEM_SECRETKEY );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	/* Find the object to delete based on the label.  Since we can have 
	   multiple related objects (e.g. a key and a certificate) with the same 
	   label, a straight search for all objects with a given label could 
	   return CRYPT_ERROR_DUPLICATE so we search for the objects by type as 
	   well as label.  In addition even a search for specific objects can 
	   return CRYPT_ERROR_DUPLICATE so we use the -Ex version of findObject() 
	   to make sure we don't get an error if multiple objects exist.  
	   Although cryptlib won't allow more than one object with a given label 
	   to be created, other applications might create duplicate labels.  The 
	   correct behaviour in these circumstances is uncertain, what we do for 
	   now is delete the first object we find that matches the label.
	   
	   First we try for a certificate and use that to find associated keys */
	cryptStatus = findObjectEx( pkcs11Info, &hCertificate, certTemplate, 3 );
	if( cryptStatusOK( cryptStatus ) )
		{
		/* We got a certificate, if there are associated keys delete them as 
		   well */
		cryptStatus = findObjectFromObject( pkcs11Info, hCertificate, 
											CKO_PUBLIC_KEY, &hPubkey );
		if( cryptStatusError( cryptStatus ) )
			hPubkey = CK_OBJECT_NONE;
		cryptStatus = findObjectFromObject( pkcs11Info, hCertificate, 
											CKO_PRIVATE_KEY, &hPrivkey );
		if( cryptStatusError( cryptStatus ) )
			hPrivkey = CK_OBJECT_NONE;
		}
	else
		{
		/* We didn't find a certificate with the given label, try for 
		   public, private, and secret keys */
		cryptStatus = findObjectEx( pkcs11Info, &hPubkey, keyTemplate, 2 );
		if( cryptStatusError( cryptStatus ) )
			hPubkey = CK_OBJECT_NONE;
		keyTemplate[ 0 ].pValue = ( CK_VOID_PTR ) &privkeyClass;
		cryptStatus = findObjectEx( pkcs11Info, &hPrivkey, keyTemplate, 2 );
		if( cryptStatusError( cryptStatus ) )
			hPrivkey = CK_OBJECT_NONE;
		keyTemplate[ 0 ].pValue = ( CK_VOID_PTR ) &secKeyClass;
		cryptStatus = findObjectEx( pkcs11Info, &hSecretKey, keyTemplate, 2 );
		if( cryptStatusError( cryptStatus ) )
			hSecretKey = CK_OBJECT_NONE;

		/* There may be an unlabelled certificate present, try and find it 
		   by looking for a certificate matching the key ID */
		if( hPubkey != CK_OBJECT_NONE || hPrivkey != CK_OBJECT_NONE )
			{
			cryptStatus = findObjectFromObject( pkcs11Info, 
							( hPrivkey != CK_OBJECT_NONE ) ? hPrivkey : hPubkey, 
							CKO_CERTIFICATE, &hCertificate );
			if( cryptStatusError( cryptStatus ) )
				hCertificate = CK_OBJECT_NONE;
			}
		}

	/* If we found a public key with a given label but no private key, try 
	   and find a matching private key by ID, and vice versa */
	if( hPubkey != CK_OBJECT_NONE && hPrivkey == CK_OBJECT_NONE )
		{
		cryptStatus = findObjectFromObject( pkcs11Info, hPubkey, 
											CKO_PRIVATE_KEY, &hPrivkey );
		if( cryptStatusError( cryptStatus ) )
			hPrivkey = CK_OBJECT_NONE;
		}
	if( hPrivkey != CK_OBJECT_NONE && hPubkey == CK_OBJECT_NONE )
		{
		cryptStatus = findObjectFromObject( pkcs11Info, hPrivkey, 
											CKO_PUBLIC_KEY, &hPubkey );
		if( cryptStatusError( cryptStatus ) )
			hPubkey = CK_OBJECT_NONE;
		}
	if( hPrivkey == CK_OBJECT_NONE && hPubkey == CK_OBJECT_NONE && \
		hSecretKey == CK_OBJECT_NONE && hCertificate == CK_OBJECT_NONE )
		return( CRYPT_ERROR_NOTFOUND );

	/* Reset the status values, which may contain error values due to not 
	   finding various objects to delete above */
	cryptStatus = CRYPT_OK;
	status = CKR_OK;

	/* Delete the objects */
	if( hCertificate != CK_OBJECT_NONE )
		status = C_DestroyObject( pkcs11Info->hSession, hCertificate );
	if( hPubkey != CK_OBJECT_NONE )
		{
		int status2;

		status2 = C_DestroyObject( pkcs11Info->hSession, hPubkey );
		if( status2 != CKR_OK && status == CKR_OK )
			status = status2;
		}
	if( hPrivkey != CK_OBJECT_NONE )
		{
		int status2;

		status2 = C_DestroyObject( pkcs11Info->hSession, hPrivkey );
		if( status2 != CKR_OK && status == CKR_OK )
			status = status2;
		}
	if( hSecretKey != CK_OBJECT_NONE )
		{
		int status2;

		status2 = C_DestroyObject( pkcs11Info->hSession, hSecretKey );
		if( status2 != CKR_OK && status == CKR_OK )
			status = status2;
		}
	if( status != CKR_OK )
		cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	return( cryptStatus );
	}

/****************************************************************************
*																			*
*							Device Access Routines							*
*																			*
****************************************************************************/

/* Set up the function pointers to the write methods */

STDC_NONNULL_ARG( ( 1 ) ) \
void initPKCS11Write( INOUT DEVICE_INFO *deviceInfo )
	{
	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	deviceInfo->setItemFunction = setItemFunction;
	deviceInfo->deleteItemFunction = deleteItemFunction;
	}
#endif /* USE_PKCS11 */
