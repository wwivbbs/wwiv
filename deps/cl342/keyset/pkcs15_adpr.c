/****************************************************************************
*																			*
*					cryptlib PKCS #15 Private-key Add Interface				*
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

#ifdef USE_PKCS15

/* Define the following to write the pre-PKCS #15 v1.2 exponent-size value
   for RSA private keys */

/* #define USE_RSA_EXTRAPARAM */

/* The minimum number of keying iterations to use when deriving a key wrap
   key from a password */

#if defined( CONFIG_SLOW_CPU )
  #define MIN_KEYING_ITERATIONS	800
#elif defined( CONFIG_FAST_CPU )
  #define MIN_KEYING_ITERATIONS	20000
#else
  #define MIN_KEYING_ITERATIONS	5000
#endif /* CONFIG_SLOW_CPU */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Replace existing private-key data with updated information */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void replacePrivkeyData( INOUT PKCS15_INFO *pkcs15infoPtr, 
								IN_BUFFER( newPrivKeyDataSize ) \
									const void *newPrivKeyData, 
								IN_LENGTH_SHORT_MIN( 16 ) \
									const int newPrivKeyDataSize,
								IN_LENGTH_SHORT const int newPrivKeyOffset )
	{
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isReadPtr( newPrivKeyData, newPrivKeyDataSize ) );

	REQUIRES_V( newPrivKeyDataSize >= 16 && \
				newPrivKeyDataSize < MAX_INTLENGTH_SHORT );
	REQUIRES_V( newPrivKeyOffset > 0 && \
				newPrivKeyOffset < newPrivKeyDataSize && \
				newPrivKeyOffset < MAX_INTLENGTH_SHORT );

	/* If we've allocated new storage for the data rather than directly 
	   replacing the existing entry, free the existing one and replace it
	   with the new one */
	if( newPrivKeyData != pkcs15infoPtr->privKeyData )
		{
		if( pkcs15infoPtr->privKeyData != NULL )
			{
			zeroise( pkcs15infoPtr->privKeyData, 
					 pkcs15infoPtr->privKeyDataSize );
			clFree( "replacePrivkeyData", pkcs15infoPtr->privKeyData );
			}
		pkcs15infoPtr->privKeyData = ( void * ) newPrivKeyData;
		}

	/* Update the size information */
	pkcs15infoPtr->privKeyDataSize = newPrivKeyDataSize;
	pkcs15infoPtr->privKeyOffset = newPrivKeyOffset;
	}

/* Calculate the size of and if necessary allocate storage for private-key 
   data.  This function has to be accessible externally because adding or 
   changing a certificate for a private key can change the private-key 
   attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int calculatePrivkeyStorage( OUT_BUFFER_ALLOC_OPT( *newPrivKeyDataSize ) \
								void **newPrivKeyDataPtr, 
							 OUT_LENGTH_SHORT_Z int *newPrivKeyDataSize, 
							 IN_BUFFER_OPT( origPrivKeyDataSize ) \
								const void *origPrivKeyData,
							 IN_LENGTH_SHORT_Z const int origPrivKeyDataSize,
							 IN_LENGTH_SHORT const int privKeySize,
							 IN_LENGTH_SHORT const int privKeyAttributeSize,
							 IN_LENGTH_SHORT const int extraDataSize )
	{
	void *newPrivKeyData;

	assert( isWritePtr( newPrivKeyDataPtr, sizeof( void * ) ) );
	assert( isWritePtr( newPrivKeyDataSize, sizeof( int ) ) ); 
	assert( ( origPrivKeyData == NULL && origPrivKeyDataSize == 0 ) || \
			isReadPtr( origPrivKeyData, origPrivKeyDataSize ) );

	REQUIRES( ( origPrivKeyData == NULL && origPrivKeyDataSize == 0 ) || \
			  ( origPrivKeyData != NULL && origPrivKeyDataSize > 0 && \
				origPrivKeyDataSize < MAX_INTLENGTH_SHORT ) );
	REQUIRES( privKeySize > 0 && privKeySize < MAX_INTLENGTH_SHORT );
	REQUIRES( privKeyAttributeSize > 0 && \
			  privKeyAttributeSize < MAX_INTLENGTH_SHORT );
	REQUIRES( extraDataSize >= 0 && extraDataSize < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*newPrivKeyDataPtr = NULL;
	*newPrivKeyDataSize = 0;

	/* Calculate the new private-key data size */
	*newPrivKeyDataSize = sizeofObject( privKeyAttributeSize + \
										sizeofObject( \
											sizeofObject( privKeySize ) + \
											extraDataSize ) );
	ENSURES( *newPrivKeyDataSize > 0 && \
			 *newPrivKeyDataSize < MAX_INTLENGTH );

	/* If the new data will fit into the existing storage, we're done */
	if( *newPrivKeyDataSize <= origPrivKeyDataSize )
		{
		*newPrivKeyDataPtr = ( void * ) origPrivKeyData;

		return( CRYPT_OK );
		}

	/* Allocate storage for the new data */
	newPrivKeyData = clAlloc( "calculatePrivkeyStorage", *newPrivKeyDataSize );
	if( newPrivKeyData == NULL )
		return( CRYPT_ERROR_MEMORY );
	*newPrivKeyDataPtr = newPrivKeyData;

	return( CRYPT_OK );
	}

/* Update the private-key attributes while leaving the private key itself
   untouched.  This is necessary after updating a certificate associated 
   with a private key, which can affect the key's attributes */

STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
void updatePrivKeyAttributes( INOUT PKCS15_INFO *pkcs15infoPtr,
							  OUT_BUFFER_FIXED( newPrivKeyDataSize ) \
								void *newPrivKeyData, 
							  IN_LENGTH_SHORT_MIN( 16 ) \
								const int newPrivKeyDataSize,
							  IN_BUFFER( privKeyAttributeSize ) \
								const void *privKeyAttributes, 
							  IN_LENGTH_SHORT const int privKeyAttributeSize, 
							  IN_LENGTH_SHORT const int privKeyInfoSize, 
							  IN_TAG const int keyTypeTag )
	{
	STREAM stream;
	BYTE keyBuffer[ MAX_PRIVATE_KEYSIZE + 8 ];
	int newPrivKeyOffset = DUMMY_INIT, status;

	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isWritePtr( newPrivKeyData, newPrivKeyDataSize ) );
	assert( isReadPtr( privKeyAttributes, privKeyAttributeSize ) );

	REQUIRES_V( newPrivKeyDataSize >= 16 && \
				newPrivKeyDataSize < MAX_INTLENGTH_SHORT );
	REQUIRES_V( privKeyAttributeSize > 0 && \
				privKeyAttributeSize < MAX_INTLENGTH_SHORT );
	REQUIRES_V( privKeyInfoSize > 0 && \
				privKeyInfoSize < MAX_PRIVATE_KEYSIZE );
	REQUIRES_V( keyTypeTag == DEFAULT_TAG || \
				( keyTypeTag >= 0 && keyTypeTag < MAX_TAG_VALUE ) );

	/* Since we may be doing an in-place update of the private-key 
	   information we copy the wrapped key data out to a temporary buffer 
	   while we make the changes */
	ENSURES_V( rangeCheck( pkcs15infoPtr->privKeyOffset, privKeyInfoSize,
						   pkcs15infoPtr->privKeyDataSize ) );
	memcpy( keyBuffer, ( BYTE * ) pkcs15infoPtr->privKeyData +
								  pkcs15infoPtr->privKeyOffset,
			privKeyInfoSize );

	/* The corresponding key is already present, we need to update the key
	   attributes since adding the certificate may have changed them.  The
	   key data itself is unchanged so we just memcpy() it across verbatim */
	sMemOpen( &stream, newPrivKeyData, newPrivKeyDataSize );
	writeConstructed( &stream, privKeyAttributeSize + \
							   sizeofObject( \
									sizeofObject( privKeyInfoSize ) ), 
					  keyTypeTag );
	swrite( &stream, privKeyAttributes, privKeyAttributeSize );
	writeConstructed( &stream, ( int ) sizeofObject( privKeyInfoSize ),
					  CTAG_OB_TYPEATTR );
	status = writeSequence( &stream, privKeyInfoSize );
	if( cryptStatusOK( status ) )
		{
		newPrivKeyOffset = stell( &stream );
		status = swrite( &stream, keyBuffer, privKeyInfoSize );
		}
	sMemDisconnect( &stream );
	zeroise( keyBuffer, MAX_PRIVATE_KEYSIZE );
	ENSURES_V( cryptStatusOK( status ) && \
			   !cryptStatusError( checkObjectEncoding( newPrivKeyData, \
													   newPrivKeyDataSize ) ) );

	/* Replace the old data with the newly-written data */
	replacePrivkeyData( pkcs15infoPtr, newPrivKeyData, newPrivKeyDataSize, 
						newPrivKeyOffset );
	}

/****************************************************************************
*																			*
*						Encryption Context Management						*
*																			*
****************************************************************************/

/* Create a strong encryption/MAC context to protect a private key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int createStrongAlgorithmContext( OUT_HANDLE_OPT \
											CRYPT_CONTEXT *iCryptContext,
										 IN_HANDLE const CRYPT_USER iCryptOwner,
										 IN_HANDLE_OPT \
											const CRYPT_CONTEXT iMasterKeyContext,
										 const BOOLEAN isCryptContext )
	{
	CRYPT_CONTEXT iLocalContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MECHANISM_KDF_INFO mechanismInfo;
	int algorithm, status;

	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( iMasterKeyContext == CRYPT_UNUSED || \
			  isHandleRangeValid( iMasterKeyContext ) );

	/* Clear return value */
	*iCryptContext = CRYPT_ERROR;

	/* In the interests of luser-proofing we're rather paranoid and force
	   the use of non-weak algorithms and modes of operation.  In addition
	   since OIDs are only defined for a limited subset of algorithms we 
	   also default to a guaranteed available algorithm if no OID is defined
	   for the algorithm that was requested */
	if( isCryptContext )
		{
		status = krnlSendMessage( iCryptOwner, IMESSAGE_GETATTRIBUTE, 
								  &algorithm, CRYPT_OPTION_ENCR_ALGO );
		if( cryptStatusError( status ) || isWeakCryptAlgo( algorithm ) || \
			cryptStatusError( sizeofAlgoIDex( algorithm, CRYPT_MODE_CBC, 0 ) ) )
			algorithm = CRYPT_ALGO_3DES;
		}
	else
		{
		status = krnlSendMessage( iCryptOwner, IMESSAGE_GETATTRIBUTE, 
								  &algorithm, CRYPT_OPTION_ENCR_MAC );
		if( cryptStatusError( status ) || isWeakMacAlgo( algorithm ) || \
			cryptStatusError( sizeofAlgoID( algorithm ) ) )
			algorithm = CRYPT_ALGO_HMAC_SHA1;
		}

	/* Create the context */
	setMessageCreateObjectInfo( &createInfo, algorithm );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iLocalContext = createInfo.cryptHandle;

	/* Perform any additional initialisation that may be required.  In 
	   particular we need to generate an IV at this point so that it can be 
	   copied to the generic-secret context */
	if( isCryptContext )
		{
		status = krnlSendNotifier( iLocalContext, IMESSAGE_CTX_GENIV );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}

	/* If we're using standard encryption, we're done */
	if( iMasterKeyContext == CRYPT_UNUSED )
		{
		*iCryptContext = iLocalContext;

		return( CRYPT_OK );
		}

	/* Derive the key for the context from the generic-secret context */
	if( isCryptContext )
		{
		setMechanismKDFInfo( &mechanismInfo, iLocalContext, 
							 iMasterKeyContext, CRYPT_ALGO_HMAC_SHA1, 
							 "encryption", 10 );
		}
	else
		{
		setMechanismKDFInfo( &mechanismInfo, iLocalContext, 
							 iMasterKeyContext, CRYPT_ALGO_HMAC_SHA1, 
							 "authentication", 14 );
		}
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_KDF,
							  &mechanismInfo, MECHANISM_DERIVE_PKCS5 );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iCryptContext = iLocalContext;

	return( CRYPT_OK );
	}

/* Send algorithm parameters from the encryption or MAC context to the
   generic-secret context */

CHECK_RETVAL \
static int setAlgoParams( IN_HANDLE const CRYPT_CONTEXT iGenericSecret,
						  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE algorithmParamData[ CRYPT_MAX_TEXTSIZE + 8 ];
	int algorithmParamDataSize = DUMMY_INIT, status;

	REQUIRES( isHandleRangeValid( iGenericSecret ) );
	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( attribute == CRYPT_IATTRIBUTE_ENCPARAMS || \
			  attribute == CRYPT_IATTRIBUTE_MACPARAMS );

	/* Get the algorithm parameter data from the encryption or MAC
	   context */
	sMemOpen( &stream, algorithmParamData, CRYPT_MAX_TEXTSIZE );
	if( attribute == CRYPT_IATTRIBUTE_ENCPARAMS )
		status = writeCryptContextAlgoID( &stream, iCryptContext );
	else
		status = writeContextAlgoID( &stream, iCryptContext, 
									 CRYPT_ALGO_NONE );
	if( cryptStatusOK( status ) )
		algorithmParamDataSize = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Send the encoded parameter information to the generic-secret 
	   context */
	setMessageData( &msgData, algorithmParamData, algorithmParamDataSize );
	return( krnlSendMessage( iGenericSecret, IMESSAGE_SETATTRIBUTE_S, 
							 &msgData, attribute ) );
	}

/* Create encryption contexts to protect the private key data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int createContexts( OUT_HANDLE_OPT CRYPT_CONTEXT *iGenericSecret,
						   OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
						   OUT_HANDLE_OPT CRYPT_CONTEXT *iMacContext,
						   IN_HANDLE const CRYPT_HANDLE iCryptOwner )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( isWritePtr( iGenericSecret, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( iMacContext, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );

	/* Clear return values */
	*iGenericSecret = *iCryptContext = *iMacContext = CRYPT_ERROR;

	/* Create the generic-secret context from which the encryption and MAC
	   keys will be derived and generate a key into it */
	setMessageCreateObjectInfo( &createInfo, CRYPT_IALGO_GENERIC_SECRET );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	*iGenericSecret = createInfo.cryptHandle;
	status = krnlSendNotifier( *iGenericSecret, IMESSAGE_CTX_GENKEY );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( *iGenericSecret, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Create the encryption and MAC contexts and derive the keys for them 
	   from the generic-secret context */
	status = createStrongAlgorithmContext( iCryptContext, iCryptOwner,
										   *iGenericSecret, TRUE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( *iGenericSecret, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	status = createStrongAlgorithmContext( iMacContext, iCryptOwner,
										   *iGenericSecret, FALSE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( *iGenericSecret, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( *iCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Send the algorithm parameters for the encryption and MAC contexts to
	   the generic-secret context */
	status = setAlgoParams( *iGenericSecret, *iCryptContext,
							CRYPT_IATTRIBUTE_ENCPARAMS );
	if( cryptStatusOK( status ) )
		status = setAlgoParams( *iGenericSecret, *iMacContext,
								CRYPT_IATTRIBUTE_MACPARAMS );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( *iGenericSecret, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( *iCryptContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( *iMacContext, IMESSAGE_DECREFCOUNT );
		}
	
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Private-key Wrap Routines						*
*																			*
****************************************************************************/

/* Write a wrapped content-protection key in the form 
   SET OF { [ 0 ] (EncryptedKey) } */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int writeWrappedSessionKey( INOUT STREAM *stream,
								   IN_HANDLE \
										const CRYPT_CONTEXT iSessionKeyContext,
								   IN_HANDLE const CRYPT_USER iCryptOwner,
								   IN_BUFFER( passwordLength ) \
										const char *password,
								   IN_LENGTH_NAME const int passwordLength )
	{
	CRYPT_CONTEXT iCryptContext;
	int iterations, exportedKeySize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( isHandleRangeValid( iSessionKeyContext ) );
	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( passwordLength >= MIN_NAME_LENGTH && \
			  passwordLength < MAX_ATTRIBUTE_SIZE );

	/* In the interests of luser-proofing we force the use of a safe minimum 
	   number of iterations */
	status = krnlSendMessage( iCryptOwner, IMESSAGE_GETATTRIBUTE, &iterations,
							  CRYPT_OPTION_KEYING_ITERATIONS );
	if( cryptStatusError( status ) || iterations < MIN_KEYING_ITERATIONS )
		iterations = MIN_KEYING_ITERATIONS;

	/* Create an encryption context and derive the user password into it */
	status = createStrongAlgorithmContext( &iCryptContext, iCryptOwner, 
										   CRYPT_UNUSED, TRUE );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
							  &iterations, CRYPT_CTXINFO_KEYING_ITERATIONS );
	if( cryptStatusOK( status ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, ( MESSAGE_CAST ) password, 
						passwordLength );
		status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_CTXINFO_KEYING_VALUE );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Determine the size of the exported key and write the encrypted data
	   content field */
	status = iCryptExportKey( NULL, 0, &exportedKeySize, CRYPT_FORMAT_CMS, 
							  iSessionKeyContext, iCryptContext );
	if( cryptStatusOK( status ) )
		{
		void *dataPtr;
		int length;

		writeSet( stream, exportedKeySize );
		status = sMemGetDataBlockRemaining( stream, &dataPtr, &length );
		if( cryptStatusOK( status ) )
			{
			status = iCryptExportKey( dataPtr, length, &exportedKeySize,
									  CRYPT_FORMAT_CMS, iSessionKeyContext, 
									  iCryptContext );
			}
		if( cryptStatusOK( status ) )
			status = sSkip( stream, exportedKeySize );
		}

	/* Clean up */
	krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
	return( status );
	}

/* Write the private key wrapped using the session key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int writeWrappedPrivateKey( OUT_BUFFER( wrappedKeyMaxLength, \
											   *wrappedKeyLength ) \
										void *wrappedKey, 
								   IN_LENGTH_SHORT_MIN( 16 ) \
										const int wrappedKeyMaxLength,
								   OUT_LENGTH_SHORT_Z int *wrappedKeyLength,
								   IN_HANDLE const CRYPT_HANDLE iPrivKeyContext,
								   IN_HANDLE const CRYPT_CONTEXT iCryptContext,
								   IN_HANDLE_OPT const CRYPT_CONTEXT iMacContext,
								   IN_ALGO const CRYPT_ALGO_TYPE pkcAlgo )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	STREAM encDataStream;
	int length = DUMMY_INIT, status;

	assert( isWritePtr( wrappedKey, wrappedKeyMaxLength ) );
	assert( isWritePtr( wrappedKeyLength, sizeof( int ) ) );

	REQUIRES( wrappedKeyMaxLength >= 16 && \
			  wrappedKeyMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iPrivKeyContext ) );
	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( iMacContext == CRYPT_UNUSED || \
			  isHandleRangeValid( iMacContext ) );
	REQUIRES( isPkcAlgo( pkcAlgo ) );

	/* Clear return values */
	memset( wrappedKey, 0, min( 16, wrappedKeyMaxLength ) );
	*wrappedKeyLength = 0;

	/* Export the wrapped private key */
	setMechanismWrapInfo( &mechanismInfo, wrappedKey, wrappedKeyMaxLength, 
						  NULL, 0, iPrivKeyContext, iCryptContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_EXPORT,
							  &mechanismInfo, MECHANISM_PRIVATEKEYWRAP );
	if( cryptStatusOK( status ) )
		length = mechanismInfo.wrappedDataLength;
	clearMechanismInfo( &mechanismInfo );
	if( cryptStatusError( status ) )
		return( status );
	*wrappedKeyLength = length;

	/* MAC the wrapped key data if necessary */
	if( iMacContext != CRYPT_UNUSED )
		{
		status = krnlSendMessage( iMacContext, IMESSAGE_CTX_HASH, 
								  wrappedKey, length );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( iMacContext, IMESSAGE_CTX_HASH, "", 0 );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Try and check that the wrapped key data no longer contains 
	   identifiable structured data.  We can only do this for RSA keys 
	   because the amount of information present for DLP keys (a single 
	   short integer) is too small to reliably check.  This check is 
	   performed in addition to checks already performed by the encryption 
	   code and the key wrap code */
	if( pkcAlgo != CRYPT_ALGO_RSA )
		return( CRYPT_OK );

	/* For RSA keys the data would be:

		SEQUENCE {
			[3] INTEGER,
			...
			}

	   99.9% of all wrapped keys will fail the initial valid-SEQUENCE check 
	   so we provide an early-out for it */
	sMemConnect( &encDataStream, wrappedKey, *wrappedKeyLength );
	status = readSequence( &encDataStream, &length );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &encDataStream );
		return( CRYPT_OK );
		}

	/* The data must contain at least p and q, or at most all key 
	   components */
	if( length < MIN_PKCSIZE * 2 || length > MAX_PRIVATE_KEYSIZE )
		{
		sMemDisconnect( &encDataStream );
		return( CRYPT_OK );
		}

	/* The first key component is p, encoded as '[3] INTEGER' */
	status = readIntegerTag( &encDataStream, NULL, CRYPT_MAX_PKCSIZE, 
							 &length, 3 );
	if( cryptStatusOK( status ) && \
		( length < MIN_PKCSIZE || length > CRYPT_MAX_PKCSIZE ) )
		status = CRYPT_ERROR;
	sMemDisconnect( &encDataStream );
	if( cryptStatusError( status ) )
		return( CRYPT_OK );

	/* We appear to have plaintext data still present in the buffer, clear 
	   it and warn the user */
	zeroise( wrappedKey, wrappedKeyMaxLength );
	DEBUG_DIAG(( "Private key data wasn't encrypted" ));
	assert( DEBUG_WARN );
	return( CRYPT_ERROR_FAILED );
	}

/****************************************************************************
*																			*
*								Add a Private Key							*
*																			*
****************************************************************************/

/* A structure to store various parameters needed when writing a private 
   key */

typedef struct {
	/* Encryption contexts used to secure the private key */
	CRYPT_CONTEXT iGenericContext, iCryptContext, iMacContext;

	/* The encoded private-key attributes */
	BUFFER_FIXED( privKeyAttributeSize ) \
	const void *privKeyAttributes; 
	int privKeyAttributeSize;

	/* Miscellaneous information */
	CRYPT_ALGO_TYPE pkcCryptAlgo;	/* Private-key algorithm */
#ifdef USE_RSA_EXTRAPARAM
	int modulusSize;				/* Optional parameter for RSA keys */
#endif /* USE_RSA_EXTRAPARAM */
	int keyTypeTag;					/* ASN.1 tag for encoding key data */
	} PRIVKEY_WRITE_PARAMS;

#define initPrivKeyParams( params, genericCtx, cryptCtx, macCtx, keyAttr, keyAttrSize, pkcAlgo, modSize, keyTag ) \
		memset( params, 0, sizeof( PRIVKEY_WRITE_PARAMS ) ); \
		( params )->iGenericContext = genericCtx; \
		( params )->iCryptContext = cryptCtx; \
		( params )->iMacContext = macCtx; \
		( params )->privKeyAttributes = keyAttr; \
		( params )->privKeyAttributeSize = keyAttrSize; \
		( params )->pkcCryptAlgo = pkcAlgo; \
		( params )->keyTypeTag = keyTag;
#ifdef USE_RSA_EXTRAPARAM
		( params )->modulusSize = modSize;
#endif /* USE_RSA_EXTRAPARAM */

/* Add private-key metadata to a PKCS #15 storage object using a simplified 
   version if the code in the standard writePrivateKey() to store just the 
   attributes and the external storage reference */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int addPrivateKeyMetadata( INOUT PKCS15_INFO *pkcs15infoPtr, 
								  IN_HANDLE const CRYPT_HANDLE iCryptContext,
								  const PRIVKEY_WRITE_PARAMS *privKeyParams )
	{
	STREAM stream;
	MESSAGE_DATA msgData;
	BYTE storageID[ KEYID_SIZE + 8 ];
	void *newPrivKeyData = pkcs15infoPtr->privKeyData;
	const int privKeySize = sizeofObject( KEYID_SIZE );
	int newPrivKeyDataSize, newPrivKeyOffset = DUMMY_INIT;
	int extraDataSize = 0;
	int status;

	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isReadPtr( privKeyParams, sizeof( PRIVKEY_WRITE_PARAMS ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );

	REQUIRES( privKeyParams->iGenericContext == CRYPT_UNUSED && \
			  privKeyParams->iCryptContext == CRYPT_UNUSED && \
			  privKeyParams->iMacContext == CRYPT_UNUSED );
	REQUIRES( privKeyParams->privKeyAttributeSize > 0 && \
			  privKeyParams->privKeyAttributeSize < MAX_INTLENGTH_SHORT );
	REQUIRES( isPkcAlgo( privKeyParams->pkcCryptAlgo ) );
#ifdef USE_RSA_EXTRAPARAM
	REQUIRES( ( isEccAlgo( privKeyParams->pkcCryptAlgo ) && \
				privKeyParams->modulusSize >= MIN_PKCSIZE_ECC && \
				privKeyParams->modulusSize <= CRYPT_MAX_PKCSIZE_ECC ) || \
			  ( !isEccAlgo( privKeyParams->pkcCryptAlgo ) && \
				privKeyParams->modulusSize >= MIN_PKCSIZE && \
				privKeyParams->modulusSize <= CRYPT_MAX_PKCSIZE ) );
#endif /* USE_RSA_EXTRAPARAM */
	REQUIRES( privKeyParams->keyTypeTag == DEFAULT_TAG || \
			  ( privKeyParams->keyTypeTag >= 0 && \
				privKeyParams->keyTypeTag < MAX_TAG_VALUE ) );

	/* Get the storage ID used to link the metadata to the actual keying 
	   data held in external hardware */
	setMessageData( &msgData, storageID, KEYID_SIZE );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_DEVICESTORAGEID );
	if( cryptStatusError( status ) )
		return( status );

	/* Calculate the private-key storage size */
#ifdef USE_RSA_EXTRAPARAM
	if( privKeyParams->pkcCryptAlgo == CRYPT_ALGO_RSA )
		{
		/* RSA keys have an extra element for PKCS #11 compatibility */
		extraDataSize = sizeofShortInteger( \
							bytesToBits( privKeyParams->modulusSize ) );
		}
#endif /* USE_RSA_EXTRAPARAM */
	status = calculatePrivkeyStorage( &newPrivKeyData, &newPrivKeyDataSize, 
									  pkcs15infoPtr->privKeyData,
									  pkcs15infoPtr->privKeyDataSize,
									  sizeofObject( privKeySize ),
									  privKeyParams->privKeyAttributeSize, 
									  extraDataSize );
	if( cryptStatusError( status ) )
		return( status );

	sMemOpen( &stream, newPrivKeyData, newPrivKeyDataSize );

	/* Write the outer header, attributes, and storage reference */
	writeConstructed( &stream, privKeyParams->privKeyAttributeSize + \
							   sizeofObject( \
									sizeofObject( \
										sizeofObject( privKeySize ) + \
										extraDataSize ) ),
					  privKeyParams->keyTypeTag );
	swrite( &stream, privKeyParams->privKeyAttributes, 
			privKeyParams->privKeyAttributeSize );
	writeConstructed( &stream, 
					  sizeofObject( \
							sizeofObject( privKeySize + extraDataSize ) ), 
					  CTAG_OB_TYPEATTR );
	status = writeSequence( &stream, 
							sizeofObject( privKeySize + extraDataSize ) );
	if( cryptStatusOK( status ) )
		newPrivKeyOffset = stell( &stream );
	if( cryptStatusOK( status ) )
		{
		writeSequence( &stream, privKeySize );
		status = writeOctetString( &stream, storageID, KEYID_SIZE, 
								   DEFAULT_TAG );
#ifdef USE_RSA_EXTRAPARAM
		if( cryptStatusOK( status ) && \
			privKeyParams->pkcCryptAlgo == CRYPT_ALGO_RSA )
			{
			/* RSA keys have an extra element for PKCS #11 compability that 
			   we need to kludge onto the end of the private-key data */
			status = writeShortInteger( &stream, 
								bytesToBits( privKeyParams->modulusSize ), 
								DEFAULT_TAG );
			}
#endif /* USE_RSA_EXTRAPARAM */
		}
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		return( status );
		}
	assert( newPrivKeyDataSize == stell( &stream ) );
	sMemDisconnect( &stream );
	ENSURES( !cryptStatusError( checkObjectEncoding( newPrivKeyData, \
													 newPrivKeyDataSize ) ) );

	/* Replace the old data with the newly-written data */
	replacePrivkeyData( pkcs15infoPtr, newPrivKeyData, 
						newPrivKeyDataSize, newPrivKeyOffset );
	return( CRYPT_OK );
	}

/* Write an encrypted and MACd private key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5, 8, 9, 10, 11 ) ) \
static int writePrivateKey( IN_HANDLE const CRYPT_HANDLE iPrivKeyContext,
							IN_HANDLE const CRYPT_HANDLE iCryptOwner,
							IN_BUFFER( passwordLength ) const char *password, 
							IN_LENGTH_NAME const int passwordLength,
							const PRIVKEY_WRITE_PARAMS *privKeyParams,
							IN_BUFFER_OPT( origPrivKeyDataSize ) \
								const void *origPrivKeyData,
							IN_LENGTH_SHORT_Z const int origPrivKeyDataSize,
							OUT_BUFFER_ALLOC_OPT( *newPrivKeyDataSize ) \
								void **newPrivKeyData,
							OUT_LENGTH_Z int *newPrivKeyDataSize,
							OUT_LENGTH_Z int *newPrivKeyOffset, 
							INOUT ERROR_INFO *errorInfo )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE envelopeHeaderBuffer[ 256 + 8 ], macValue[ CRYPT_MAX_HASHSIZE + 8 ];
	void *encryptedKeyDataPtr, *macDataPtr = DUMMY_INIT_PTR;
	int privKeySize = DUMMY_INIT, extraDataSize = 0, macSize;
	int envelopeHeaderSize, envelopeContentSize;
	int macDataOffset = DUMMY_INIT, macDataLength = DUMMY_INIT;
	int encryptedKeyDataLength, status;

	assert( isReadPtr( password, passwordLength ) );
	assert( isReadPtr( privKeyParams, sizeof( PRIVKEY_WRITE_PARAMS ) ) );
	assert( ( origPrivKeyData == NULL && origPrivKeyDataSize == 0 ) || \
			isReadPtr( origPrivKeyData, origPrivKeyDataSize ) );
	assert( isWritePtr( newPrivKeyData, sizeof( void * ) ) );
	assert( isWritePtr( newPrivKeyDataSize, sizeof( int ) ) );
	assert( isWritePtr( newPrivKeyOffset, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( iPrivKeyContext ) );
	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( passwordLength >= MIN_NAME_LENGTH && \
			  passwordLength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( ( origPrivKeyData == NULL && origPrivKeyDataSize == 0 ) || \
			  ( origPrivKeyData != NULL && origPrivKeyDataSize > 0 && \
				origPrivKeyDataSize < MAX_INTLENGTH_SHORT ) );

	REQUIRES( isHandleRangeValid( privKeyParams->iGenericContext ) );
	REQUIRES( isHandleRangeValid( privKeyParams->iCryptContext ) );
	REQUIRES( isHandleRangeValid( privKeyParams->iMacContext ) );
	REQUIRES( privKeyParams->privKeyAttributeSize > 0 && \
			  privKeyParams->privKeyAttributeSize < MAX_INTLENGTH_SHORT );
	REQUIRES( isPkcAlgo( privKeyParams->pkcCryptAlgo ) );
#ifdef USE_RSA_EXTRAPARAM
	REQUIRES( ( isEccAlgo( privKeyParams->pkcCryptAlgo ) && \
				privKeyParams->modulusSize >= MIN_PKCSIZE_ECC && \
				privKeyParams->modulusSize <= CRYPT_MAX_PKCSIZE_ECC ) || \
			  ( !isEccAlgo( privKeyParams->pkcCryptAlgo ) && \
				privKeyParams->modulusSize >= MIN_PKCSIZE && \
				privKeyParams->modulusSize <= CRYPT_MAX_PKCSIZE ) );
#endif /* USE_RSA_EXTRAPARAM */
	REQUIRES( privKeyParams->keyTypeTag == DEFAULT_TAG || \
			  ( privKeyParams->keyTypeTag >= 0 && \
				privKeyParams->keyTypeTag < MAX_TAG_VALUE ) );

	/* Clear return values */
	*newPrivKeyData = NULL;
	*newPrivKeyDataSize = *newPrivKeyOffset = 0;

	/* Calculate the eventual encrypted key size */
	setMechanismWrapInfo( &mechanismInfo, NULL, 0, NULL, 0, iPrivKeyContext,
						  privKeyParams->iCryptContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_EXPORT,
							  &mechanismInfo, MECHANISM_PRIVATEKEYWRAP );
	if( cryptStatusOK( status ) )
		privKeySize = mechanismInfo.wrappedDataLength;
	clearMechanismInfo( &mechanismInfo );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( privKeySize >= 16 && privKeySize <= 256 + MAX_PRIVATE_KEYSIZE );

	/* Determine the size of the MAC value */
	status = krnlSendMessage( privKeyParams->iMacContext, 
							  IMESSAGE_GETATTRIBUTE, &macSize, 
							  CRYPT_CTXINFO_BLOCKSIZE );
	if( cryptStatusError( status ) )
		return( status );
	macSize = sizeofObject( macSize );

	/* Write the CMS envelope header for the wrapped private key except for 
	   the outermost wrapper, which we have to defer writing until later 
	   because we won't know the size of the encryption context information 
	   or inner CMS header until we've written them.  Since we're using 
	   KEKRecipientInfo we use a version of 2 rather than 0 */
	sMemOpen( &stream, envelopeHeaderBuffer, 256 );
	writeShortInteger( &stream, 2, DEFAULT_TAG );
	status = writeWrappedSessionKey( &stream, privKeyParams->iGenericContext, 
									 iCryptOwner, password, passwordLength );
	if( cryptStatusOK( status ) )
		{
		macDataOffset = stell( &stream );
		status = writeCMSencrHeader( &stream, OID_CMS_DATA, 
									 sizeofOID( OID_CMS_DATA ), privKeySize,
									 privKeyParams->iGenericContext );
		}
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't write envelope header for wrapping private "
				  "key" ) );
		}
	envelopeHeaderSize = stell( &stream );
	envelopeContentSize = envelopeHeaderSize + privKeySize + macSize;
	sMemDisconnect( &stream );

	/* Since we haven't been able to write the outer CMS envelope wrapper 
	   yet we need to adjust the overall size for the additional level of
	   encapsulation */
	privKeySize = ( int ) sizeofObject( envelopeContentSize );

	/* Calculate the private-key storage size */
#ifdef USE_RSA_EXTRAPARAM
	if( privKeyParams->pkcCryptAlgo == CRYPT_ALGO_RSA )
		{
		/* RSA keys have an extra element for PKCS #11 compatibility */
		extraDataSize = sizeofShortInteger( privKeyParams->modulusSize );
		}
#endif /* USE_RSA_EXTRAPARAM */
	status = calculatePrivkeyStorage( newPrivKeyData, newPrivKeyDataSize, 
									  origPrivKeyData, origPrivKeyDataSize,
									  privKeySize, 
									  privKeyParams->privKeyAttributeSize, 
									  extraDataSize );
	if( cryptStatusError( status ) )
		return( status );

	/* MAC the EncryptedContentInfo.ContentEncryptionAlgorithmIdentifier 
	   information alongside the payload data to prevent an attacker from 
	   manipulating the algorithm parameters to cause corruption that won't 
	   be detected by the MAC on the payload data */
	sMemConnect( &stream, envelopeHeaderBuffer + macDataOffset, 
				 envelopeHeaderSize - macDataOffset );
	readSequenceI( &stream, NULL );		/* Outer encapsulation */
	status = readUniversal( &stream );	/* Content-type OID */
	if( cryptStatusOK( status ) )
		status = getStreamObjectLength( &stream, &macDataLength );
	if( cryptStatusOK( status ) )		/* AlgoID */
		{
		status = sMemGetDataBlock( &stream, &macDataPtr, 
								   macDataLength );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( privKeyParams->iMacContext, 
								  IMESSAGE_CTX_HASH, macDataPtr, 
								  macDataLength );
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		if( newPrivKeyData != origPrivKeyData )
			clFree( "writePrivateKey", *newPrivKeyData );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't MAC encryption attributes" ) );
		}

	sMemOpen( &stream, *newPrivKeyData, *newPrivKeyDataSize );

	/* Write the outer header and attributes */
	writeConstructed( &stream, privKeyParams->privKeyAttributeSize + \
							   sizeofObject( sizeofObject( privKeySize ) + \
											 extraDataSize ),
					  privKeyParams->keyTypeTag );
	swrite( &stream, privKeyParams->privKeyAttributes, 
			privKeyParams->privKeyAttributeSize );
	writeConstructed( &stream, 
					  sizeofObject( privKeySize + extraDataSize ), 
					  CTAG_OB_TYPEATTR );
	status = writeSequence( &stream, privKeySize + extraDataSize );
	if( cryptStatusOK( status ) )
		*newPrivKeyOffset = stell( &stream );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		if( newPrivKeyData != origPrivKeyData )
			clFree( "writePrivateKey", *newPrivKeyData );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't write private key attributes" ) );
		}

	/* Write the previously-encoded CMS envelope header and key exchange 
	   information.  Since we now know the size of the envelope header 
	   (which we couldn't write earlier) we can add this too */
	writeConstructed( &stream, envelopeContentSize, 
					  CTAG_OV_DIRECTPROTECTED_EXT );
	status = swrite( &stream, envelopeHeaderBuffer, envelopeHeaderSize );
	if( cryptStatusError( status ) )
		return( status );
	
	/* Write the encrypted private key by exporting it directly into the 
	   stream buffer */
	status = sMemGetDataBlockRemaining( &stream, &encryptedKeyDataPtr, 
										&encryptedKeyDataLength );
	if( cryptStatusOK( status ) )
		{
		status = writeWrappedPrivateKey( encryptedKeyDataPtr, 
										 encryptedKeyDataLength, &privKeySize, 
										 iPrivKeyContext, 
										 privKeyParams->iCryptContext, 
										 privKeyParams->iMacContext, 
										 privKeyParams->pkcCryptAlgo );
		}
	if( cryptStatusOK( status ) )
		status = sSkip( &stream, privKeySize );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		if( newPrivKeyData != origPrivKeyData )
			clFree( "writePrivateKey", *newPrivKeyData );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't write wrapped private key" ) );
		}

	/* Get the MAC value and write it */
	setMessageData( &msgData, macValue, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( privKeyParams->iMacContext,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusOK( status ) )
		status = writeOctetString( &stream, macValue, msgData.length, 
								   DEFAULT_TAG );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		if( newPrivKeyData != origPrivKeyData )
			clFree( "writePrivateKey", *newPrivKeyData );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't write integrity check value for wrapped private "
				  "key" ) );
		}

#ifdef USE_RSA_EXTRAPARAM
	/* RSA keys have an extra element for PKCS #11 compability that we need 
	   to kludge onto the end of the private-key data */
	if( privKeyParams->pkcCryptAlgo == CRYPT_ALGO_RSA )
		{
		status = writeShortInteger( &stream, privKeyParams->modulusSize, 
									DEFAULT_TAG );
		if( cryptStatusError( status ) )
			{
			sMemClose( &stream );
			return( status );
			}
		}
#endif /* USE_RSA_EXTRAPARAM */
	assert( *newPrivKeyDataSize == stell( &stream ) );
	sMemDisconnect( &stream );
	ENSURES( !cryptStatusError( checkObjectEncoding( *newPrivKeyData, \
													 *newPrivKeyDataSize ) ) );

	return( CRYPT_OK );
	}

/* Add a private key to a PKCS #15 collection */

#if 1	/* New (3.4.0+) code to write the private key as AuthEnvData */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6, 11 ) ) \
int pkcs15AddPrivateKey( INOUT PKCS15_INFO *pkcs15infoPtr, 
						 IN_HANDLE const CRYPT_HANDLE iPrivKeyContext,
						 IN_HANDLE const CRYPT_HANDLE iCryptOwner,
						 IN_BUFFER_OPT( passwordLength ) const char *password, 
						 IN_LENGTH_NAME_Z const int passwordLength,
						 IN_BUFFER( privKeyAttributeSize ) \
							const void *privKeyAttributes, 
						 IN_LENGTH_SHORT const int privKeyAttributeSize,
						 IN_ALGO const CRYPT_ALGO_TYPE pkcCryptAlgo, 
						 IN_LENGTH_PKC const int modulusSize, 
						 const BOOLEAN isStorageObject, 
						 INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CONTEXT iGenericContext, iCryptContext, iMacContext;
	PRIVKEY_WRITE_PARAMS privKeyParams;
	void *newPrivKeyData;
	int newPrivKeyDataSize, newPrivKeyOffset, keyTypeTag, status;

	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( ( isStorageObject && password == NULL && passwordLength == 0 ) || \
			( !isStorageObject && isReadPtr( password, passwordLength ) ) );
	assert( isReadPtr( privKeyAttributes, privKeyAttributeSize ) );

	REQUIRES( isHandleRangeValid( iPrivKeyContext ) );
	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( ( isStorageObject && password == NULL && \
				passwordLength == 0 ) || \
			  ( !isStorageObject && password != NULL && \
				passwordLength >= MIN_NAME_LENGTH && \
				passwordLength < MAX_ATTRIBUTE_SIZE ) );
	REQUIRES( privKeyAttributeSize > 0 && \
			  privKeyAttributeSize < MAX_INTLENGTH_SHORT );
	REQUIRES( isPkcAlgo( pkcCryptAlgo ) );
	REQUIRES( ( isEccAlgo( pkcCryptAlgo ) && \
				modulusSize >= MIN_PKCSIZE_ECC && \
				modulusSize <= CRYPT_MAX_PKCSIZE_ECC ) || \
			  ( !isEccAlgo( pkcCryptAlgo ) && \
				modulusSize >= MIN_PKCSIZE && \
				modulusSize <= CRYPT_MAX_PKCSIZE ) );
	REQUIRES( errorInfo != NULL );

	/* Get the tag for encoding the key data */
	status = getKeyTypeTag( CRYPT_UNUSED, pkcCryptAlgo, &keyTypeTag );
	if( cryptStatusError( status ) )
		return( status );

	/* If this is a dummy object (in other words object metadata) being 
	   stored in a PKCS #15 object store then there's nothing present except
	   key attributes and a reference to the key held in external hardware,
	   in which case we use a simplified version of the key-write code */
	if( isStorageObject )
		{
		initPrivKeyParams( &privKeyParams, CRYPT_UNUSED, CRYPT_UNUSED, 
						   CRYPT_UNUSED, privKeyAttributes, 
						   privKeyAttributeSize, pkcCryptAlgo, modulusSize,
						   keyTypeTag );
		status = addPrivateKeyMetadata( pkcs15infoPtr, iPrivKeyContext, 
										&privKeyParams );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, errorInfo, 
					  "Couldn't write private key metadata" ) );
			}

		return( CRYPT_OK );
		}

	/* Create the contexts needed to protect the private-key data */
	status = createContexts( &iGenericContext, &iCryptContext, &iMacContext,
							 iCryptOwner );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't create encryption contexts to protect the "
				  "private key" ) );
		}

	/* Write the encrypted and MACd private key */
	initPrivKeyParams( &privKeyParams, iGenericContext, iCryptContext, 
					   iMacContext, privKeyAttributes, 
					   privKeyAttributeSize, pkcCryptAlgo, modulusSize,
					   keyTypeTag );
	status = writePrivateKey( iPrivKeyContext, iCryptOwner, password, 
							  passwordLength, &privKeyParams,
							  pkcs15infoPtr->privKeyData,
							  pkcs15infoPtr->privKeyDataSize,
							  &newPrivKeyData, &newPrivKeyDataSize,
							  &newPrivKeyOffset, errorInfo );
	krnlSendNotifier( iGenericContext, IMESSAGE_DECREFCOUNT );
	krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
	krnlSendNotifier( iMacContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( newPrivKeyData != NULL );

	/* Replace the old data with the newly-written data */
	replacePrivkeyData( pkcs15infoPtr, newPrivKeyData, 
						newPrivKeyDataSize, newPrivKeyOffset );
	return( CRYPT_OK );
	}

#else	/* Old (pre-3.4.0) code to write the encrypted private key as
		   EnvelopedData rather than AuthEnv'd data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 6, 11 ) ) \
int pkcs15AddPrivateKey( INOUT PKCS15_INFO *pkcs15infoPtr, 
						 IN_HANDLE const CRYPT_HANDLE iCryptContext,
						 IN_HANDLE const CRYPT_HANDLE iCryptOwner,
						 IN_BUFFER( passwordLength ) const char *password, 
						 IN_LENGTH_NAME const int passwordLength,
						 IN_BUFFER( privKeyAttributeSize ) \
							const void *privKeyAttributes, 
						 IN_LENGTH_SHORT const int privKeyAttributeSize,
						 IN_ALGO const CRYPT_ALGO_TYPE pkcCryptAlgo, 
						 IN_LENGTH_PKC const int modulusSize, 
						 const BOOLEAN isStorageObject, 
						 INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CONTEXT iSessionKeyContext;
	MECHANISM_WRAP_INFO mechanismInfo;
	STREAM stream;
	BYTE envelopeHeaderBuffer[ 256 + 8 ];
	void *newPrivKeyData = pkcs15infoPtr->privKeyData;
	int newPrivKeyDataSize, newPrivKeyOffset = DUMMY_INIT;
	int privKeySize = DUMMY_INIT, extraDataSize = 0;
	int keyTypeTag, status;

	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( ( isStorageObject && password == NULL && passwordLength == 0 ) || \
			( !isStorageObject && isReadPtr( password, passwordLength ) ) );
	assert( isReadPtr( privKeyAttributes, privKeyAttributeSize ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( ( isStorageObject && password == NULL && \
				passwordLength == 0 ) || \
			  ( !isStorageObject && password != NULL && \
				passwordLength >= MIN_NAME_LENGTH && \
				passwordLength < MAX_ATTRIBUTE_SIZE ) );
	REQUIRES( privKeyAttributeSize > 0 && \
			  privKeyAttributeSize < MAX_INTLENGTH_SHORT );
	REQUIRES( isPkcAlgo( pkcCryptAlgo ) );
	REQUIRES( ( isEccAlgo( pkcCryptAlgo ) && \
				modulusSize >= MIN_PKCSIZE_ECC && \
				modulusSize <= CRYPT_MAX_PKCSIZE_ECC ) || \
			  ( !isEccAlgo( pkcCryptAlgo ) && \
				modulusSize >= MIN_PKCSIZE && \
				modulusSize <= CRYPT_MAX_PKCSIZE ) );
	REQUIRES( errorInfo != NULL );

	/* Get the tag for encoding the key data */
	status = getKeyTypeTag( CRYPT_UNUSED, pkcCryptAlgo, &keyTypeTag );
	if( cryptStatusError( status ) )
		return( status );

	/* If this is a dummy object (in other words object metadata) being 
	   stored in a PKCS #15 object store then there's nothing present except
	   key attributes and a reference to the key held in external hardware,
	   in which case we use a simplified version if the code that follows */
	if( isStorageObject )
		{
		status = addPrivateKeyMetadata( pkcs15infoPtr, iCryptContext, 
										privKeyAttributes, privKeyAttributeSize,
										pkcCryptAlgo, modulusSize, keyTypeTag );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, errorInfo, 
					  "Couldn't write private key metadata" ) );
			}

		return( CRYPT_OK );
		}

	/* Create a session key context and generate a key and IV into it.  The 
	   IV would be generated automatically later on when we encrypt data for 
	   the first time but we do it explicitly here to catch any possible 
	   errors at a point where recovery is easier */
	status = createStrongEncryptionContext( &iSessionKeyContext, iCryptOwner );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendNotifier( iSessionKeyContext, IMESSAGE_CTX_GENKEY );
	if( cryptStatusOK( status ) )
		status = krnlSendNotifier( iSessionKeyContext, IMESSAGE_CTX_GENIV );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iSessionKeyContext, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't create session key to wrap private key" ) );
		}

	/* Calculate the eventual encrypted key size */
	setMechanismWrapInfo( &mechanismInfo, NULL, 0, NULL, 0, iCryptContext,
						  iSessionKeyContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_EXPORT,
							  &mechanismInfo, MECHANISM_PRIVATEKEYWRAP );
	if( cryptStatusOK( status ) )
		privKeySize = mechanismInfo.wrappedDataLength;
	clearMechanismInfo( &mechanismInfo );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iSessionKeyContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	ENSURES( privKeySize <= 256 + MAX_PRIVATE_KEYSIZE );

	/* Write the CMS envelope header for the wrapped private key except for 
	   the outermost wrapper, which we have to defer writing until later 
	   since we won't know the wrapped session key or inner CMS header size 
	   until we've written them.  Since we're using KEKRecipientInfo we use 
	   a version of 2 rather than 0 */
	sMemOpen( &stream, envelopeHeaderBuffer, 256 );
	writeShortInteger( &stream, 2, DEFAULT_TAG );
	status = writeWrappedSessionKey( &stream, iSessionKeyContext,
									 iCryptOwner, password, passwordLength );
	if( cryptStatusOK( status ) )
		status = writeCMSencrHeader( &stream, OID_CMS_DATA, 
									 sizeofOID( OID_CMS_DATA ), privKeySize,
									 iSessionKeyContext );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		krnlSendNotifier( iSessionKeyContext, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't write envelope header for wrapping private "
				  "key" ) );
		}
	envelopeHeaderSize = stell( &stream );
	envelopeContentSize = envelopeHeaderSize + privKeySize;
	sMemDisconnect( &stream );

	/* Since we haven't been able to write the outer CMS envelope wrapper 
	   yet we need to adjust the overall size for the additional level of
	   encapsulation */
	privKeySize = ( int ) sizeofObject( privKeySize + envelopeHeaderSize );

	/* Calculate the private-key storage size */
#ifdef USE_RSA_EXTRAPARAM
	if( pkcCryptAlgo == CRYPT_ALGO_RSA )
		{
		/* RSA keys have an extra element for PKCS #11 compatibility */
		extraDataSize = sizeofShortInteger( modulusSize );
		}
#endif /* USE_RSA_EXTRAPARAM */
	status = calculatePrivkeyStorage( pkcs15infoPtr, &newPrivKeyData,
									  &newPrivKeyDataSize, privKeySize, 
									  privKeyAttributeSize, 
									  extraDataSize );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iSessionKeyContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	sMemOpen( &stream, newPrivKeyData, newPrivKeyDataSize );

	/* Write the outer header and attributes */
	writeConstructed( &stream, privKeyAttributeSize + \
							   sizeofObject( sizeofObject( privKeySize ) + \
											 extraDataSize ),
					  keyTypeTag );
	swrite( &stream, privKeyAttributes, privKeyAttributeSize );
	writeConstructed( &stream, sizeofObject( privKeySize + extraDataSize ), 
					  CTAG_OB_TYPEATTR );
	status = writeSequence( &stream, privKeySize + extraDataSize );
	if( cryptStatusOK( status ) )
		newPrivKeyOffset = stell( &stream );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		krnlSendNotifier( iSessionKeyContext, IMESSAGE_DECREFCOUNT );
		if( newPrivKeyData != pkcs15infoPtr->privKeyData )
			clFree( "addPrivateKey", newPrivKeyData );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't write private key attributes" ) );
		}

	/* Write the previously-encoded CMS envelope header and key exchange
	   information and follow it with the encrypted private key.  Since we
	   now know the size of the envelope header (which we couldn't write
	   earlier) we can add this now too */
	writeConstructed( &stream, envelopeContentSize, CTAG_OV_DIRECTPROTECTED );
	status = swrite( &stream, envelopeHeaderBuffer, envelopeHeaderSize );
	if( cryptStatusOK( status ) )
		{
		void *dataPtr;
		int length;

		status = sMemGetDataBlockRemaining( &stream, &dataPtr, &length );
		if( cryptStatusOK( status ) )
			status = writeWrappedPrivateKey( dataPtr, length, &privKeySize, 
											 iCryptContext, iSessionKeyContext, 
											 pkcCryptAlgo );
		}
	if( cryptStatusOK( status ) )
		status = sSkip( &stream, privKeySize );
#ifdef USE_RSA_EXTRAPARAM
	if( cryptStatusOK( status ) && pkcCryptAlgo == CRYPT_ALGO_RSA )
		{
		/* RSA keys have an extra element for PKCS #11 compability that we
		   need to kludge onto the end of the private-key data */
		status = writeShortInteger( &stream, modulusSize, DEFAULT_TAG );
		}
#endif /* USE_RSA_EXTRAPARAM */
	krnlSendNotifier( iSessionKeyContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't wrap private key using session key" ) );
		}
	assert( newPrivKeyDataSize == stell( &stream ) );
	sMemDisconnect( &stream );
	ENSURES( !cryptStatusError( checkObjectEncoding( newPrivKeyData, \
													 newPrivKeyDataSize ) ) );

	/* Replace the old data with the newly-written data */
	replacePrivkeyData( pkcs15infoPtr, newPrivKeyData, 
						newPrivKeyDataSize, newPrivKeyOffset );
	return( CRYPT_OK );
	}
#endif /* 0 */
#endif /* USE_PKCS15 */
