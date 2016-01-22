/****************************************************************************
*																			*
*					cryptlib PKCS #15 Get Public/Private Key				*
*						Copyright Peter Gutmann 1996-2009					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "keyset.h"
  #include "pkcs15.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs15.h"
#endif /* Compiler-specific includes */

/* Define the following to enable a workaround for a MAC keysize bug in
   authEnc private key data in cryptlib 3.4.0 */

#define MAC_KEYSIZE_BUG

#ifdef USE_PKCS15

/* OID information used to read a PKCS #15 keyset */

static const OID_INFO FAR_BSS dataOIDinfo[] = {
	{ OID_CMS_DATA, CRYPT_OK },
	{ NULL, 0 }, { NULL, 0 }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Translate the PKCS #15 usage flags into cryptlib permitted actions.  The
   PKCS #11 use of the 'derive' flag to mean 'allow key agreement' is a bit
   of a kludge, we map it to allowing key-agreement export and import if 
   it's a key-agreement algorithm, if there are further constraints then 
   they'll be handled by the attached certificate.  The PKCS #15 
   nonRepudiation flag doesn't have any definition so we can't do anything 
   with it, although we may need to translate it to allowing signing and/or 
   verification if implementations appear that expect it to be used this 
   way */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int getPermittedActions( IN_FLAGS( PKCS15_USAGE ) const int usageFlags,
								IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
								OUT_FLAGS_Z( ACTION ) int *usage )
	{
	int actionFlags = ACTION_PERM_NONE_ALL;

	REQUIRES( usageFlags >= PKSC15_USAGE_FLAG_NONE && \
			  usageFlags < PKCS15_USAGE_FLAG_MAX );
	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* Clear return value */
	*usage = ACTION_PERM_NONE;

	if( usageFlags & ( PKCS15_USAGE_ENCRYPT | PKCS15_USAGE_WRAP ) )
		actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_ALL );
	if( usageFlags & ( PKCS15_USAGE_DECRYPT | PKCS15_USAGE_UNWRAP ) )
		actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_ALL );
	if( usageFlags & PKCS15_USAGE_SIGN )
		actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGN, ACTION_PERM_ALL );
	if( usageFlags & PKCS15_USAGE_VERIFY )
		actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_ALL );
	if( isKeyxAlgo( cryptAlgo ) && ( usageFlags & PKCS15_USAGE_DERIVE ) )
		actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_ALL ) | \
					   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_ALL );
	if( cryptAlgo == CRYPT_ALGO_RSA )
		{
		/* If there are any restrictions on the key usage then we have to 
		   make it internal-only because of RSA's signature/encryption 
		   duality */
		if( !( ( usageFlags & ( PKCS15_USAGE_ENCRYPT | PKCS15_USAGE_WRAP | \
								PKCS15_USAGE_DECRYPT | PKCS15_USAGE_UNWRAP ) ) && \
			   ( usageFlags & ( PKCS15_USAGE_SIGN | PKCS15_USAGE_VERIFY ) ) ) )
			actionFlags = MK_ACTION_PERM_NONE_EXTERNAL( actionFlags );
		}
	else
		{
		/* Because of the special-case data formatting requirements for DLP
		   algorithms we make the usage internal-only */
		actionFlags = MK_ACTION_PERM_NONE_EXTERNAL( actionFlags );
		}
	if( actionFlags <= ACTION_PERM_NONE_ALL )
		return( CRYPT_ERROR_PERMISSION );
	*usage = actionFlags;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Read Public Key Components						*
*																			*
****************************************************************************/

/* Read public-key components from a PKCS #15 object entry */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 8, 9, 10, 11, 12 ) ) \
int readPublicKeyComponents( const PKCS15_INFO *pkcs15infoPtr,
							 IN_HANDLE const CRYPT_KEYSET iCryptKeysetCallback,
							 IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							 IN_BUFFER( keyIDlength ) const void *keyID, 
							 IN_LENGTH_KEYID const int keyIDlength,
							 const BOOLEAN publicComponentsOnly,
							 IN_HANDLE const CRYPT_DEVICE iDeviceObject, 
							 OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContextPtr,
							 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iDataCertPtr,
							 OUT_FLAGS_Z( ACTION ) int *pubkeyActionFlags, 
							 OUT_FLAGS_Z( ACTION ) int *privkeyActionFlags, 
							 INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CONTEXT iCryptContext;
	CRYPT_CERTIFICATE iDataCert = CRYPT_ERROR;
	STREAM stream;
	int pkcAlgo, status;

	assert( isReadPtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );
	assert( isWritePtr( iCryptContextPtr, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( iDataCertPtr, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isWritePtr( pubkeyActionFlags, sizeof( int ) ) );
	assert( isWritePtr( privkeyActionFlags, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( iCryptKeysetCallback ) );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_PGPKEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( iDeviceObject == SYSTEM_OBJECT_HANDLE || \
			  isHandleRangeValid( iDeviceObject ) );
	REQUIRES( errorInfo != NULL );

	/* Clear return values */
	*iCryptContextPtr = CRYPT_ERROR;
	*iDataCertPtr = CRYPT_ERROR;
	*pubkeyActionFlags = *privkeyActionFlags = ACTION_PERM_NONE;

	/* If we're creating a public-key context we create the certificate or 
	   PKC context normally, if we're creating a private-key context we 
	   create a data-only certificate (if there's certificate information 
	   present) and a partial PKC context ready to accept the private key 
	   components.  If there's a certificate present then we take all of the 
	   information that we need from the certificate, otherwise we use the 
	   public-key data */
#ifdef USE_CERTIFICATES
	if( pkcs15infoPtr->certData != NULL )
		{
		/* There's a certificate present, import it and reconstruct the
		   public-key information from it if we're creating a partial PKC 
		   context */
		status = iCryptImportCertIndirect( &iCryptContext,
								iCryptKeysetCallback, keyIDtype, keyID,
								keyIDlength, publicComponentsOnly ? \
									KEYMGMT_FLAG_NONE : \
									KEYMGMT_FLAG_DATAONLY_CERT );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, errorInfo, 
					  "Couldn't recreate certificate from stored "
					  "certificate data" ) );
			}
		if( !publicComponentsOnly )
			{
			DYNBUF pubKeyDB;

			/* We got the certificate, now create the public part of the 
			   context from the certificate's encoded public-key 
			   components */
			iDataCert = iCryptContext;
			status = dynCreate( &pubKeyDB, iDataCert, 
								CRYPT_IATTRIBUTE_SPKI );
			if( cryptStatusError( status ) )
				return( status );
			sMemConnect( &stream, dynData( pubKeyDB ),
						 dynLength( pubKeyDB ) );
			status = iCryptReadSubjectPublicKey( &stream, &iCryptContext,
												 iDeviceObject, TRUE );
			sMemDisconnect( &stream );
			dynDestroy( &pubKeyDB );
			if( cryptStatusError( status ) )
				{
				krnlSendNotifier( iDataCert, IMESSAGE_DECREFCOUNT );
				retExt( status, 
						( status, errorInfo, 
						  "Couldn't recreate public key from "
						  "certificate" ) );
				}
			}
		}
	else
#endif /* USE_CERTIFICATES */
		{
		const int pubKeyStartOffset = pkcs15infoPtr->pubKeyOffset;
		const int pubKeyTotalSize = pkcs15infoPtr->pubKeyDataSize;

		/* There's no certificate present, create the public-key context
		   directly */
		REQUIRES( rangeCheck( pubKeyStartOffset, 
							  pubKeyTotalSize - pubKeyStartOffset,
							  pubKeyTotalSize ) );
		sMemConnect( &stream, 
					 ( BYTE * ) pkcs15infoPtr->pubKeyData + pubKeyStartOffset,
					 pubKeyTotalSize - pubKeyStartOffset );
		status = iCryptReadSubjectPublicKey( &stream, &iCryptContext,
											 iDeviceObject, 
											 !publicComponentsOnly );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, errorInfo, 
					  "Couldn't recreate public key from stored public key "
					  "data" ) );
			}
		}

	/* Get the permitted usage flags for each object type that we'll be
	   instantiating.  If there's a public key present we apply its usage
	   flags to whichever PKC context we create, even if it's done indirectly
	   via the certificate import.  Since the private key can also perform 
	   the actions of the public key we set its action flags to the union of 
	   the two */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &pkcAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) && pkcs15infoPtr->pubKeyData != NULL )
		{
		status = getPermittedActions( pkcs15infoPtr->pubKeyUsage, pkcAlgo,
									  pubkeyActionFlags );
		}
	if( cryptStatusOK( status ) && !publicComponentsOnly )
		{
		status = getPermittedActions( pkcs15infoPtr->privKeyUsage, pkcAlgo,
									  privkeyActionFlags );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		if( iDataCert != CRYPT_ERROR )
			krnlSendNotifier( iDataCert, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, errorInfo, 
				  "Public/private key usage flags don't allow any type of "
				  "key usage" ) );
		}

	/* Return the newly-created objects to the caller */
	*iCryptContextPtr = iCryptContext;
	*iDataCertPtr = iDataCert;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Read Private Key Components							*
*																			*
****************************************************************************/

/* Import the session key (either a direct session key or a generic-secret
   key used to derive the decryption and MAC contexts) using the supplied
   password */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4, 6 ) ) \
static int importSessionKey( IN_HANDLE const CRYPT_CONTEXT iSessionKey,
							 IN_BUFFER( encryptedKeyDataSize ) \
								const void *encryptedKeyData,
							 IN_LENGTH_SHORT const int encryptedKeyDataSize,
							 IN_BUFFER( passwordLength ) const void *password, 
							 IN_LENGTH_NAME const int passwordLength,
							 const QUERY_INFO *queryInfo )
	{
	CRYPT_CONTEXT iKeyWrapContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	int mode, status;

	assert( isReadPtr( encryptedKeyData, encryptedKeyDataSize ) );
	assert( isReadPtr( password, passwordLength ) );
	assert( isReadPtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( isHandleRangeValid( iSessionKey ) );
	REQUIRES( encryptedKeyDataSize >= 16 && \
			  encryptedKeyDataSize < MAX_INTLENGTH_SHORT );
	REQUIRES( passwordLength >= MIN_NAME_LENGTH && \
			  passwordLength < MAX_ATTRIBUTE_SIZE );

	/* Create the context used to import the session key and derive the user
	   password into it */
	setMessageCreateObjectInfo( &createInfo, queryInfo->cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iKeyWrapContext = createInfo.cryptHandle;
	mode = queryInfo->cryptMode;	/* int vs.enum */
	status = krnlSendMessage( iKeyWrapContext, IMESSAGE_SETATTRIBUTE, 
							  &mode, CRYPT_CTXINFO_MODE );
	if( cryptStatusOK( status ) && \
		queryInfo->keySetupAlgo != CRYPT_ALGO_NONE )
		{
		const int algorithm = queryInfo->keySetupAlgo;	/* int vs.enum */
		status = krnlSendMessage( iKeyWrapContext, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &algorithm, 
								  CRYPT_CTXINFO_KEYING_ALGO );
		}
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iKeyWrapContext, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &queryInfo->keySetupIterations,
								  CRYPT_CTXINFO_KEYING_ITERATIONS );
	if( cryptStatusOK( status ) && queryInfo->keySize > 0 )
		status = krnlSendMessage( iKeyWrapContext, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &queryInfo->keySize,
								  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) queryInfo->salt, 
						queryInfo->saltLength );
		status = krnlSendMessage( iKeyWrapContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_CTXINFO_KEYING_SALT );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) password, passwordLength );
		status = krnlSendMessage( iKeyWrapContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_CTXINFO_KEYING_VALUE );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iKeyWrapContext, IMESSAGE_DECREFCOUNT );

		/* If there's an error in the parameters supplied via the query 
		   information (which were stored with the exported key) we'll get 
		   an argument or attribute error when we try to set the attribute 
		   so we translate it into an error code which is appropriate for 
		   the situation */
		return( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status );
		}

	/* Import the session key, either an actual session key context for PKCS 
	   #15 v1.1 using direct-protected content or a generic-secret context
	   for PKCS #15 v1.2 using direct-protected-ext content */
	status = iCryptImportKey( encryptedKeyData, encryptedKeyDataSize, 
							  CRYPT_FORMAT_CRYPTLIB, iKeyWrapContext,
							  iSessionKey, NULL );
	krnlSendNotifier( iKeyWrapContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		/* Translate any arg/attribute errors as before */
		return( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status );
		}

	return( CRYPT_OK );
	}

/* Initialise decryption and MAC contexts and keys from a generic-secret 
   context */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 2, 3, 4 ) ) \
static int initKeys( IN_HANDLE const CRYPT_CONTEXT iGenericContext,
					 OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
					 OUT_HANDLE_OPT CRYPT_CONTEXT *iMacContext,
					 OUT_HANDLE_OPT CRYPT_CONTEXT *iMacAltContext,
					 const QUERY_INFO *queryInfo )
	{
	CRYPT_CONTEXT iAuthEncCryptContext, iAuthEncMacContext;
#ifdef MAC_KEYSIZE_BUG
	CRYPT_CONTEXT iAuthEncMacAltContext = DUMMY_INIT;
#endif /* MAC_KEYSIZE_BUG */
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MECHANISM_KDF_INFO mechanismInfo;
	STREAM stream;
	int status;

	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( iMacContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( iMacAltContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( isHandleRangeValid( iGenericContext ) );

	/* Clear return values */
	*iCryptContext = *iMacContext = *iMacAltContext = CRYPT_ERROR;

	/* Recreate the encryption and MAC contexts used for the authenticated 
	   encryption from the algorithm parameter data that was stored 
	   alongside the generic-secret context parameter data */
	sMemConnect( &stream, 
				 queryInfo->authEncParamData + queryInfo->encParamStart,
				 queryInfo->encParamLength );
	status = readContextAlgoID( &stream, &iAuthEncCryptContext, NULL, 
								DEFAULT_TAG, ALGOID_CLASS_CRYPT );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, 
				 queryInfo->authEncParamData + queryInfo->macParamStart,
				 queryInfo->macParamLength );
	status = readContextAlgoID( &stream, &iAuthEncMacContext, NULL, 
								DEFAULT_TAG, ALGOID_CLASS_HASH );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Derive the encryption and MAC keys from the generic-secret key */
	setMechanismKDFInfo( &mechanismInfo, iAuthEncCryptContext, 
						 iGenericContext, CRYPT_ALGO_HMAC_SHA1, 
						 "encryption", 10 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_KDF,
							  &mechanismInfo, MECHANISM_DERIVE_PKCS5 );
	if( cryptStatusOK( status ) )
		{
		setMechanismKDFInfo( &mechanismInfo, iAuthEncMacContext, 
							 iGenericContext, CRYPT_ALGO_HMAC_SHA1, 
							 "authentication", 14 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_KDF,
								  &mechanismInfo, MECHANISM_DERIVE_PKCS5 );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iAuthEncMacContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* MAC the EncryptedContentInfo.ContentEncryptionAlgorithmIdentifier 
	   information alongside the payload data to prevent an attacker from 
	   manipulating the algorithm parameters to cause corruption that won't 
	   be detected by the MAC on the payload data */
	status = krnlSendMessage( iAuthEncMacContext, IMESSAGE_CTX_HASH,
							  ( MESSAGE_CAST ) queryInfo->authEncParamData,
							  queryInfo->authEncParamLength );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iAuthEncMacContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

#ifdef MAC_KEYSIZE_BUG
	/* Because of an error in setting the key size for the 3.4.0 cryptlib
	   release, private-key files created by this version use a 128-bit 
	   HMAC-SHA1 key instead of a 160-bit one.  To work around this we 
	   create a second MAC context with a 128-bit key and try that if the
	   initial check with the 160-bit key fails.

	   In addition the RFC draft that 3.4.0 was based on didn't MAC the
	   EncryptedContentInfo.ContentEncryptionAlgorithmIdentifier, so we skip
	   MAC'ing that part of the data as well */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_HMAC_SHA1 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iAuthEncMacContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	else
		{
		static const int macKeySize = bitsToBytes( 128 );

		iAuthEncMacAltContext = createInfo.cryptHandle;
		status = krnlSendMessage( iAuthEncMacAltContext, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &macKeySize, 
								  CRYPT_CTXINFO_KEYSIZE );
		}
	if( cryptStatusOK( status ) )
		{
		setMechanismKDFInfo( &mechanismInfo, iAuthEncMacAltContext, 
							 iGenericContext, CRYPT_ALGO_HMAC_SHA1, 
							 "authentication", 14 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_KDF,
								  &mechanismInfo, MECHANISM_DERIVE_PKCS5 );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iAuthEncMacContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iAuthEncMacAltContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iMacAltContext = iAuthEncMacAltContext;
#endif /* MAC_KEYSIZE_BUG */

	*iCryptContext = iAuthEncCryptContext;
	*iMacContext = iAuthEncMacContext;

	return( CRYPT_OK );
	}

/* Verify the integrity of the encrypted key components and derive the final 
   decryption context from the generic intermediate context that's used to 
   derive the MAC and final decryption contexts.  This replaces the 
   intermediate context with the final decryption context */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 4, 6, 7 ) ) \
static int verifyEncKey( INOUT_HANDLE CRYPT_CONTEXT *iCryptContext,
						 IN_BUFFER( encryptedContentLength ) \
							const void *encryptedContent, 
						 IN_LENGTH_SHORT const int encryptedContentLength, 
						 IN_BUFFER( macValueLength ) \
							const void *macValue, 
						 IN_LENGTH_HASH const int macValueLength, 
						 const QUERY_INFO *queryInfo,
						 INOUT ERROR_INFO *errorInfo )
	{
	const CRYPT_CONTEXT iOriginalCryptContext = *iCryptContext;
	CRYPT_CONTEXT iDerivedCryptContext, iMacContext, iMacAltContext;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( encryptedContent, encryptedContentLength ) );
	assert( isReadPtr( macValue, macValueLength ) );
	assert( isReadPtr( queryInfo, sizeof( QUERY_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( isHandleRangeValid( iOriginalCryptContext ) );
	REQUIRES( encryptedContentLength > 0 && \
			  encryptedContentLength < MAX_INTLENGTH_SHORT );
	REQUIRES( macValueLength >= 16 && macValueLength <= CRYPT_MAX_HASHSIZE );

	/* We're using authenticated encryption so we have to apply an 
	   intermediate step that transforms the generic-secret context into 
	   distinct decryption and MAC contexts.  This also MACs the encryption 
	   metadata that precedes the data payload */
	status = initKeys( iOriginalCryptContext, &iDerivedCryptContext, 
					   &iMacContext, &iMacAltContext, queryInfo );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't recreate encryption and MAC keys needed to "
				  "unwrap the private key" ) );
		}

	/* Verify the integrity of the encrypted private key before trying to 
	   process it */
	status = krnlSendMessage( iMacContext, IMESSAGE_CTX_HASH,
							  ( MESSAGE_CAST ) encryptedContent, 
							  encryptedContentLength );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iMacContext, IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) macValue, 
						macValueLength );
		status = krnlSendMessage( iMacContext, IMESSAGE_COMPARE, 
								  &msgData, MESSAGE_COMPARE_HASH );
		if( cryptStatusError( status ) )
			{
			/* A failed MAC check is reported as a CRYPT_ERROR comparison 
			   result so we have to convert it to a more appropriate status 
			   code */
			status = CRYPT_ERROR_SIGNATURE;
			}
		}
#ifdef MAC_KEYSIZE_BUG
	if( status == CRYPT_ERROR_SIGNATURE )
		{
		/* If we got a failed MAC check, try again with a MAC context with a 
		   128-bit MAC key, a bug in cryptlib version 3.4.0 */
		status = krnlSendMessage( iMacAltContext, IMESSAGE_CTX_HASH,
								  ( MESSAGE_CAST ) encryptedContent, 
								  encryptedContentLength );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( iMacAltContext, IMESSAGE_CTX_HASH, "", 0 );
		if( cryptStatusOK( status ) )
			{
			setMessageData( &msgData, ( MESSAGE_CAST ) macValue, 
							macValueLength );
			status = krnlSendMessage( iMacAltContext, IMESSAGE_COMPARE, 
									  &msgData, MESSAGE_COMPARE_HASH );
			if( cryptStatusError( status ) )
				{
				/* A failed MAC check is reported as a CRYPT_ERROR 
				   comparison result so we have to convert it to a more 
				   appropriate status code */
				status = CRYPT_ERROR_SIGNATURE;
				}
			}
		}
	krnlSendNotifier( iMacAltContext, IMESSAGE_DECREFCOUNT );
#endif /* MAC_KEYSIZE_BUG */
	krnlSendNotifier( iMacContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		/* We convert any failure status encountered at this point into a 
		   generic signature-check failure, which makes explicit what's 
		   going on */
		retExt( CRYPT_ERROR_SIGNATURE, 
				( CRYPT_ERROR_SIGNATURE, errorInfo, 
				  "Private-key integrity check failed" ) );
		}

	/* Replace the original encryption context with the derived one */
	krnlSendNotifier( iOriginalCryptContext, IMESSAGE_DECREFCOUNT );
	*iCryptContext = iDerivedCryptContext;

	return( CRYPT_OK );
	}

/* Read private-key components from a PKCS #15 object entry */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
int readPrivateKeyComponents( const PKCS15_INFO *pkcs15infoPtr,
							  IN_HANDLE const CRYPT_CONTEXT iPrivKeyContext,
							  IN_BUFFER_OPT( passwordLength ) \
									const void *password, 
							  IN_LENGTH_NAME_Z const int passwordLength, 
							  const BOOLEAN isStorageObject, 
							  INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CONTEXT iCryptContext;
	MECHANISM_WRAP_INFO mechanismInfo;
	MESSAGE_DATA msgData;
	QUERY_INFO queryInfo = DUMMY_INIT_STRUCT, contentQueryInfo;
	STREAM stream;
	BYTE macValue[ CRYPT_MAX_HASHSIZE + 8 ];
	BOOLEAN isAuthEnc = FALSE;
	const int privKeyStartOffset = pkcs15infoPtr->privKeyOffset;
	const int privKeyTotalSize = pkcs15infoPtr->privKeyDataSize;
	void *encryptedKey, *encryptedContent = DUMMY_INIT_PTR;
	int encryptedContentLength = DUMMY_INIT, macValueLength = DUMMY_INIT;
	int tag, status;

	assert( isReadPtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( ( isStorageObject && \
			  password == NULL && passwordLength == 0 ) || \
			( !isStorageObject && \
			  isReadPtr( password, passwordLength ) ) );

	REQUIRES( isHandleRangeValid( iPrivKeyContext ) );
	REQUIRES( ( isStorageObject && \
				password == NULL && passwordLength == 0 ) || \
			  ( !isStorageObject && \
				passwordLength >= MIN_NAME_LENGTH && \
				passwordLength < MAX_ATTRIBUTE_SIZE ) );
	REQUIRES( errorInfo != NULL );

	/* Skip the outer wrapper, version number, and header for the SET OF 
	   EncryptionInfo, and query the exported key information to determine 
	   the parameters required to reconstruct the decryption key */
	REQUIRES( rangeCheck( privKeyStartOffset, 
						  privKeyTotalSize - privKeyStartOffset,
						  privKeyTotalSize ) );
	sMemConnect( &stream,
				 ( BYTE * ) pkcs15infoPtr->privKeyData + privKeyStartOffset,
				 privKeyTotalSize - privKeyStartOffset );
	status = tag = peekTag( &stream );
	if( cryptStatusError( status ) )
		return( status );
	if( isStorageObject )
		{
		BYTE storageID[ KEYID_SIZE + 8 ];
		int length;

		/* If this is a PKCS #15 storage object then it'll contain only 
		   private-key metadata with the content being merely a reference
		   to external hardware, so we just read the storage object
		   reference and save it to the dummy context */
		if( tag != BER_SEQUENCE )
			{
			sMemDisconnect( &stream );
			retExt( CRYPT_ERROR_BADDATA, 
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Expected device storage ID, not item type %02X",
					  tag ) );
			}
		readSequence( &stream, NULL );
		status = readOctetString( &stream, storageID, &length, 
								  KEYID_SIZE, KEYID_SIZE );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		setMessageData( &msgData, storageID, KEYID_SIZE );
		return( krnlSendMessage( iPrivKeyContext, IMESSAGE_SETATTRIBUTE_S,
								 &msgData, CRYPT_IATTRIBUTE_DEVICESTORAGEID ) );
		}
	if( tag == MAKE_CTAG( CTAG_OV_DIRECTPROTECTED_EXT ) )
		isAuthEnc = TRUE;
	else
		{
		if( tag != MAKE_CTAG( CTAG_OV_DIRECTPROTECTED ) )
			{
			retExt( CRYPT_ERROR_NOTAVAIL, 
					( CRYPT_ERROR_NOTAVAIL, errorInfo, 
					  "Unrecognised private-key protection type %02X", 
					  tag ) );
			}
		}
	readConstructed( &stream, NULL, 
					 isAuthEnc ? CTAG_OV_DIRECTPROTECTED_EXT : \
								 CTAG_OV_DIRECTPROTECTED );
	readShortInteger( &stream, NULL );
	status = readSet( &stream, NULL );
	if( cryptStatusOK( status ) )
		status = queryAsn1Object( &stream, &queryInfo );
	if( cryptStatusOK( status ) && \
		queryInfo.type != CRYPT_OBJECT_ENCRYPTED_KEY )
		status = CRYPT_ERROR_BADDATA;
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		zeroise( &queryInfo, sizeof( QUERY_INFO ) );
		retExt( status, 
				( status, errorInfo, 
				  "Invalid encrypted private key data header" ) );
		}
	status = sMemGetDataBlock( &stream, &encryptedKey, queryInfo.size );
	if( cryptStatusOK( status ) )
		status = readUniversal( &stream );	/* Skip the exported key */
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		zeroise( &queryInfo, sizeof( QUERY_INFO ) );
		return( status );
		}

	/* Read the header for the encrypted key and make sure that all of the
	   encrypted key data (and the trailing MAC value if we're using 
	   authenticated encryption) is present in the stream */
	status = readCMSencrHeader( &stream, dataOIDinfo, 
				FAILSAFE_ARRAYSIZE( dataOIDinfo, OID_INFO ), &iCryptContext, 
				&contentQueryInfo, isAuthEnc ? \
					READCMS_FLAG_AUTHENC | READCMS_FLAG_DEFINITELENGTH : \
					READCMS_FLAG_DEFINITELENGTH );
	if( cryptStatusOK( status ) )
		{
		encryptedContentLength = contentQueryInfo.size;
		status = sMemGetDataBlock( &stream, &encryptedContent, 
								   encryptedContentLength );
		if( cryptStatusOK( status ) )
			status = sSkip( &stream, encryptedContentLength );
		if( cryptStatusOK( status ) && \
			( encryptedContentLength < MIN_OBJECT_SIZE || \
			  encryptedContentLength > MAX_INTLENGTH_SHORT ) )
			{
			/* Too-small object */
			status = CRYPT_ERROR_BADDATA;
			}
		}
	if( cryptStatusOK( status ) && isAuthEnc )
		{
		/* If we're using authenticated encryption then the encrypted key 
		   data is followed by a MAC value */
		status = readOctetString( &stream, macValue, &macValueLength, 16, 
								  CRYPT_MAX_HASHSIZE );
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		zeroise( &queryInfo, sizeof( QUERY_INFO ) );
		zeroise( &contentQueryInfo, sizeof( QUERY_INFO ) );
		retExt( status, 
				( status, errorInfo, 
				  "Invalid encrypted private key data" ) );
		}

	/* Import the session key using the user password */
	status = importSessionKey( iCryptContext, encryptedKey, queryInfo.size, 
							   password, passwordLength, &queryInfo );
	zeroise( &queryInfo, sizeof( QUERY_INFO ) );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		zeroise( &contentQueryInfo, sizeof( QUERY_INFO ) );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't import the session key used to protect the "
				  "private key" ) );
		}

	/* If we're using authenticated encryption then we have to use an 
	   intermediate step that transforms the generic-secret context into 
	   distinct decryption and MAC contexts.  This also MACs the 
	   encryption metadata that precedes the data payload */
	if( isAuthEnc )
		{
		status = verifyEncKey( &iCryptContext, encryptedContent, 
							   encryptedContentLength, macValue, 
							   macValueLength, &contentQueryInfo, 
							   errorInfo );
		zeroise( &contentQueryInfo, sizeof( QUERY_INFO ) );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		zeroise( &contentQueryInfo, sizeof( QUERY_INFO ) );

	/* Import the encrypted key into the PKC context */
	setMechanismWrapInfo( &mechanismInfo, ( MESSAGE_CAST ) encryptedContent,
						  encryptedContentLength, NULL, 0, iPrivKeyContext,
						  iCryptContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_IMPORT,
							  &mechanismInfo, MECHANISM_PRIVATEKEYWRAP );
	clearMechanismInfo( &mechanismInfo );
	krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		/* We can end up here due to a whole range of possible low-level 
		   problems, to make things easier on the caller we provide a 
		   somewhat more detailed breakdown of possible causes */
		switch( status )
			{
			case CRYPT_ERROR_WRONGKEY:
				retExt( status,
						( status, errorInfo, 
						  "Couldn't unwrap private key, probably due to "
						  "incorrect decryption key being used" ) );

			case CRYPT_ERROR_BADDATA:
				retExt( status,
						( status, errorInfo, 
						  "Private key data corrupted or invalid" ) );

			case CRYPT_ERROR_INVALID:
				retExt( status,
						( status, errorInfo, 
						  "Private key components failed validity check" ) );

			default:
				retExt( status,
						( status, errorInfo, 
						  "Couldn't unwrap/import private key" ) );
			}
		}

	return( CRYPT_OK );
	}
#endif /* USE_PKCS15 */
