/****************************************************************************
*																			*
*					cryptlib TLS Key Management Routines					*
*					 Copyright Peter Gutmann 1998-2012						*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "session.h"
  #include "ssl.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "session/session.h"
  #include "session/ssl.h"
#endif /* Compiler-specific includes */

#ifdef USE_SSL

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

/* Initialise and destroy the crypto information in the handshake state
   information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initHandshakeCryptInfo( INOUT SESSION_INFO *sessionInfoPtr,
							INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* Clear the handshake information contexts */
	handshakeInfo->md5context = \
		handshakeInfo->sha1context = \
			handshakeInfo->sha2context = CRYPT_ERROR;
#ifdef CONFIG_SUITEB
	handshakeInfo->sha384context = CRYPT_ERROR;
#endif /* CONFIG_SUITEB */
	handshakeInfo->dhContext = \
		handshakeInfo->sessionHashContext = CRYPT_ERROR;

	/* Create the MAC/dual-hash contexts for incoming and outgoing data.
	   SSL uses a pre-HMAC variant for which we can't use real HMAC but have
	   to construct it ourselves from MD5 and SHA-1, TLS uses a straight dual
	   hash and MACs that once a MAC key becomes available at the end of the
	   handshake */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_MD5 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusOK( status ) )
		{
		handshakeInfo->md5context = createInfo.cryptHandle;
		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA1 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		}
	if( cryptStatusOK( status ) )
		{
		handshakeInfo->sha1context = createInfo.cryptHandle;
		if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
			return( CRYPT_OK );
		}
#ifdef CONFIG_SUITEB
	if( cryptStatusOK( status ) )
		{
		/* Create the additional SHA2-384 context that's needed for Suite 
		   B use */
		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA2 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusOK( status ) )
			{
			static const int blockSize = bitsToBytes( 384 );

			handshakeInfo->sha384context = createInfo.cryptHandle;
			status = krnlSendMessage( handshakeInfo->sha384context, 
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &blockSize,
									  CRYPT_CTXINFO_BLOCKSIZE );
			}
		}
#endif /* CONFIG_SUITEB */
	if( cryptStatusOK( status ) )
		{
		/* Create additional contexts that may be needed for TLS 1.2 */
		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA2 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		}
	if( cryptStatusOK( status ) )
		{
		handshakeInfo->sha2context = createInfo.cryptHandle;
		return( CRYPT_OK );
		}

	/* One or more of the contexts couldn't be created, destroy all of the
	   contexts that have been created so far */
	destroyHandshakeCryptInfo( handshakeInfo );
	return( status );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void destroyHandshakeCryptInfo( INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	/* Destroy any active contexts.  We need to do this here (even though
	   it's also done in the general session code) to provide a clean exit in
	   case the session activation fails, so that a second activation attempt
	   doesn't overwrite still-active contexts */
	if( handshakeInfo->md5context != CRYPT_ERROR )
		{
		krnlSendNotifier( handshakeInfo->md5context,
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->md5context = CRYPT_ERROR;
		}
	if( handshakeInfo->sha1context != CRYPT_ERROR )
		{
		krnlSendNotifier( handshakeInfo->sha1context,
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->sha1context = CRYPT_ERROR;
		}
	if( handshakeInfo->sha2context != CRYPT_ERROR )
		{
		krnlSendNotifier( handshakeInfo->sha2context,
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->sha2context = CRYPT_ERROR;
		}
#ifdef CONFIG_SUITEB
	if( handshakeInfo->sha384context != CRYPT_ERROR )
		{
		krnlSendNotifier( handshakeInfo->sha384context,
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->sha384context = CRYPT_ERROR;
		}
#endif /* CONFIG_SUITEB */
	if( handshakeInfo->dhContext != CRYPT_ERROR )
		{
		krnlSendNotifier( handshakeInfo->dhContext, IMESSAGE_DECREFCOUNT );
		handshakeInfo->dhContext = CRYPT_ERROR;
		}
	if( handshakeInfo->sessionHashContext != CRYPT_ERROR )
		{
		krnlSendNotifier( handshakeInfo->sessionHashContext, 
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->sessionHashContext = CRYPT_ERROR;
		}
	}

/* Initialise and destroy the session security contexts */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initSecurityContextsSSL( INOUT SESSION_INFO *sessionInfoPtr
#ifdef CONFIG_SUITEB
									, INOUT SSL_HANDSHAKE_INFO *handshakeInfo
#endif /* CONFIG_SUITEB */
								   )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
#ifdef CONFIG_SUITEB
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
#endif /* CONFIG_SUITEB */

	/* Create the authentication contexts, unless we're using a combined
	   encryption+authentication mode */
	if( !TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_GCM ) )
		{
		const CRYPT_ALGO_TYPE integrityAlgo = sessionInfoPtr->integrityAlgo;

		setMessageCreateObjectInfo( &createInfo, integrityAlgo );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusOK( status ) )
			{
			sessionInfoPtr->iAuthInContext = createInfo.cryptHandle;
			setMessageCreateObjectInfo( &createInfo, integrityAlgo );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
									  OBJECT_TYPE_CONTEXT );
			}
		if( cryptStatusError( status ) )
			{
			destroySecurityContextsSSL( sessionInfoPtr );
			return( status );
			}
		sessionInfoPtr->iAuthOutContext = createInfo.cryptHandle;
#ifdef CONFIG_SUITEB
		if( cryptStatusOK( status ) && \
			handshakeInfo->integrityAlgoParam == bitsToBytes( 384 ) )
			{
			static const int blockSize = bitsToBytes( 384 );

			status = krnlSendMessage( sessionInfoPtr->iAuthInContext, 
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &blockSize,
									  CRYPT_CTXINFO_BLOCKSIZE );
			if( cryptStatusOK( status ) )
				{
				status = krnlSendMessage( sessionInfoPtr->iAuthOutContext, 
										  IMESSAGE_SETATTRIBUTE, 
										  ( MESSAGE_CAST ) &blockSize,
										  CRYPT_CTXINFO_BLOCKSIZE );
				}
			if( cryptStatusError( status ) )
				{
				destroySecurityContextsSSL( sessionInfoPtr );
				return( status );
				}
			}
#endif /* CONFIG_SUITEB */
		}

	/* Create the encryption contexts */
	setMessageCreateObjectInfo( &createInfo, sessionInfoPtr->cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusOK( status ) )
		{
		sessionInfoPtr->iCryptInContext = createInfo.cryptHandle;
		setMessageCreateObjectInfo( &createInfo, sessionInfoPtr->cryptAlgo );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		}
	if( cryptStatusError( status ) )
		{
		destroySecurityContextsSSL( sessionInfoPtr );
		return( status );
		}
	sessionInfoPtr->iCryptOutContext = createInfo.cryptHandle;

	/* If we're using GCM we also need to change the encryption mode from 
	   the default CBC */
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_GCM ) ) 
		{
		static const int mode = CRYPT_MODE_GCM;	/* int vs.enum */

		status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
								  IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &mode,
								  CRYPT_CTXINFO_MODE );
		if( cryptStatusOK( status ) )
			{
			status = krnlSendMessage( sessionInfoPtr->iCryptOutContext,
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &mode,
									  CRYPT_CTXINFO_MODE );
			}
		if( cryptStatusError( status ) )
			{
			destroySecurityContextsSSL( sessionInfoPtr );
			return( status );
			}
		}

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void destroySecurityContextsSSL( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_V( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* Destroy any active contexts */
	if( sessionInfoPtr->iKeyexCryptContext != CRYPT_ERROR )
		{
		krnlSendNotifier( sessionInfoPtr->iKeyexCryptContext,
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iKeyexCryptContext = CRYPT_ERROR;
		}
	if( sessionInfoPtr->iAuthInContext != CRYPT_ERROR )
		{
		krnlSendNotifier( sessionInfoPtr->iAuthInContext,
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iAuthInContext = CRYPT_ERROR;
		}
	if( sessionInfoPtr->iAuthOutContext != CRYPT_ERROR )
		{
		krnlSendNotifier( sessionInfoPtr->iAuthOutContext,
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iAuthOutContext = CRYPT_ERROR;
		}
	if( sessionInfoPtr->iCryptInContext != CRYPT_ERROR )
		{
		krnlSendNotifier( sessionInfoPtr->iCryptInContext,
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iCryptInContext = CRYPT_ERROR;
		}
	if( sessionInfoPtr->iCryptOutContext != CRYPT_ERROR )
		{
		krnlSendNotifier( sessionInfoPtr->iCryptOutContext,
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iCryptOutContext = CRYPT_ERROR;
		}
	}

/* Clone a hash context so that we can continue using the original to hash 
   further messages */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int cloneHashContext( IN_HANDLE const CRYPT_CONTEXT hashContext,
					  OUT_HANDLE_OPT CRYPT_CONTEXT *clonedHashContext )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int hashAlgo, status;	/* int vs.enum */

	assert( isWritePtr( clonedHashContext, sizeof( CRYPT_CONTEXT * ) ) );

	REQUIRES( isHandleRangeValid( hashContext ) );

	/* Clear return value */
	*clonedHashContext = CRYPT_ERROR;

	/* Determine the type of context that we have to clone */
	status = krnlSendMessage( hashContext, IMESSAGE_GETATTRIBUTE,
							  &hashAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );

	/* Create a new hash context and clone the existing one's state into 
	   it */
	setMessageCreateObjectInfo( &createInfo, hashAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( hashContext, IMESSAGE_CLONE, NULL, 
							  createInfo.cryptHandle );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*clonedHashContext = createInfo.cryptHandle;
	
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Keyex Functions								*
*																			*
****************************************************************************/

/* Load a DH/ECDH key into a context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDHcontextSSL( OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
					  IN_BUFFER_OPT( keyDataLength ) const void *keyData, 
					  IN_LENGTH_SHORT_Z const int keyDataLength,
					  IN_HANDLE_OPT const CRYPT_CONTEXT iServerKeyTemplate,
					  IN_ENUM_OPT( CRYPT_ECCCURVE ) \
							const CRYPT_ECCCURVE_TYPE eccParams,
					  const BOOLEAN isTLSLTS )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	int keySize = TLS_DH_KEYSIZE, status;

	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( ( keyData == NULL && keyDataLength == 0 ) || \
			isReadPtrDynamic( keyData, keyDataLength ) );

	REQUIRES( ( keyData == NULL && keyDataLength == 0 ) || \
			  ( keyData != NULL && \
				isShortIntegerRangeNZ( keyDataLength ) ) );
	REQUIRES( iServerKeyTemplate == CRYPT_UNUSED || \
			  isHandleRangeValid( iServerKeyTemplate ) );
	REQUIRES( isEnumRangeOpt( eccParams, CRYPT_ECCCURVE ) );
	REQUIRES( isTLSLTS == TRUE || isTLSLTS == FALSE );

	/* Clear return value */
	*iCryptContext = CRYPT_ERROR;

	/* If we're loading a built-in DH key, match the key size to the server 
	   authentication key size.  If there's no server key present, we 
	   default to the TLS_DH_KEYSIZE-byte key because we don't know how much 
	   processing power the client has */
	if( keyData == NULL && iServerKeyTemplate != CRYPT_UNUSED && \
		eccParams == CRYPT_ECCCURVE_NONE )
		{
		status = krnlSendMessage( iServerKeyTemplate, IMESSAGE_GETATTRIBUTE,
								  &keySize, CRYPT_CTXINFO_KEYSIZE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Create the DH/ECDH context */
	setMessageCreateObjectInfo( &createInfo, \
								( eccParams != CRYPT_ECCCURVE_NONE ) ? \
									CRYPT_ALGO_ECDH : CRYPT_ALGO_DH );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, "TLS key agreement key", 21 );
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_LABEL );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Load the key into the context.  If we're being given externally-
	   supplied DH/ECDH key components, load them, otherwise use the built-
	   in key */
	if( keyData != NULL )
		{
		/* If we're the client we'll have been sent DH/ECDH key components 
		   by the server */
		setMessageData( &msgData, ( MESSAGE_CAST ) keyData, keyDataLength ); 
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE_S, &msgData,
								  isTLSLTS ? CRYPT_IATTRIBUTE_KEY_SSL_EXT : \
											 CRYPT_IATTRIBUTE_KEY_SSL );
		}
	else
		{
#ifdef USE_ECDH 
		/* If we've been given ECC parameter information then we're using
		   ECDH */
		if( eccParams != CRYPT_ECCCURVE_NONE )
			{
			status = krnlSendMessage( createInfo.cryptHandle, 
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &eccParams, 
									  CRYPT_IATTRIBUTE_KEY_DLPPARAM );
			}
		else
#endif /* USE_ECDH */
			{
			/* We're loading a standard DH key of the appropriate size */
			status = krnlSendMessage( createInfo.cryptHandle, 
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &keySize, 
									  CRYPT_IATTRIBUTE_KEY_DLPPARAM );
			}
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		if( keyData == NULL )
			{
			/* If we got an error loading a known-good, fixed-format key 
			   then we report the problem as an internal error rather than 
			   (say) a bad-data error */
			retIntError();
			}
		return( status );
		}
	*iCryptContext = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/* Create the master secret from a shared (PSK) secret value, typically a
   password */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int createSharedPremasterSecret( OUT_BUFFER( premasterSecretMaxLength, \
											 *premasterSecretLength ) \
									void *premasterSecret, 
								 IN_LENGTH_SHORT \
									const int premasterSecretMaxLength, 
								 OUT_LENGTH_BOUNDED_Z( premasterSecretMaxLength ) \
									int *premasterSecretLength,
								 IN_BUFFER( sharedSecretLength ) \
									const void *sharedSecret, 
								 IN_LENGTH_SHORT const int sharedSecretLength,
								 IN_BUFFER_OPT( otherSecretLength ) \
									const void *otherSecret, 
								 IN_LENGTH_PKC_Z const int otherSecretLength,
								 const BOOLEAN isEncodedValue )
	{
	STREAM stream;
	BYTE decodedValue[ 64 + 8 ];
	static const BYTE zeroes[ CRYPT_MAX_TEXTSIZE + 8 ] = { 0 };
	int valueLength = sharedSecretLength;
	int status;

	assert( isWritePtrDynamic( premasterSecret, premasterSecretMaxLength ) );
	assert( isWritePtr( premasterSecretLength, sizeof( int ) ) );
	assert( isReadPtrDynamic( sharedSecret, sharedSecretLength ) );
	assert( otherSecret == NULL || \
			isReadPtrDynamic( otherSecret, otherSecretLength ) );

	REQUIRES( isShortIntegerRangeNZ( premasterSecretMaxLength ) );
	REQUIRES( isShortIntegerRangeNZ( sharedSecretLength ) );
	REQUIRES( ( otherSecret == NULL && otherSecretLength == 0 ) || \
			  ( otherSecret != NULL && \
				otherSecretLength > 0 && \
				otherSecretLength <= CRYPT_MAX_PKCSIZE ) );
	REQUIRES( isEncodedValue == TRUE || isEncodedValue == FALSE );

	/* Clear return values */
	memset( premasterSecret, 0, min( 16, premasterSecretMaxLength ) );
	*premasterSecretLength = 0;

	/* Write the PSK-derived premaster secret value:

		uint16	otherSecretLen
		byte[]	otherSecret		-- DH value for DHE-PSK, zeroes for pure PSK
		uint16	pskLen
		byte[]	psk

	   Because the TLS PRF splits the input into two halves of which one half 
	   is processed by HMAC-MD5 and the other by HMAC-SHA1, it's necessary
	   to extend the PSK in some way to provide input to both halves of the
	   PRF.  In a rather dubious decision, the spec requires that for pure
	   PSK (not DHE-PSK or RSA-PSK) the MD5 half be set to all zeroes, with 
	   only the SHA1 half being used.  This is done by writing otherSecret 
	   as a number of zero bytes equal in length to the password */
	sMemOpen( &stream, premasterSecret, premasterSecretMaxLength );
	if( isEncodedValue )
		{
		/* It's a cryptlib-style encoded password, decode it into its binary
		   value */
		status = decodePKIUserValue( decodedValue, 64, &valueLength,
									 sharedSecret, sharedSecretLength );
		if( cryptStatusError( status ) )
			{
			DEBUG_DIAG(( "Couldn't decode supposedly valid PKI user "
						 "value" ));
			assert( DEBUG_WARN );
			return( status );
			}
		sharedSecret = decodedValue;
		}
	if( otherSecret != NULL )
		{
		writeUint16( &stream, otherSecretLength );
		swrite( &stream, otherSecret, otherSecretLength );
		}
	else
		{
		/* It's pure PSK, otherSecret is a string of zeroes */
		writeUint16( &stream, valueLength );
		swrite( &stream, zeroes, valueLength );
		}
	writeUint16( &stream, valueLength );
	status = swrite( &stream, sharedSecret, valueLength );
	if( isEncodedValue )
		zeroise( decodedValue, valueLength );
	if( cryptStatusError( status ) )
		return( status );
	*premasterSecretLength = stell( &stream );
	sMemDisconnect( &stream );

	return( CRYPT_OK );
	}

#ifdef USE_RSA_SUITES

/* Wrap/unwrap the pre-master secret */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int wrapPremasterSecret( INOUT SESSION_INFO *sessionInfoPtr,
						 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						 OUT_BUFFER( dataMaxLength, *dataLength ) void *data, 
						 IN_LENGTH_SHORT const int dataMaxLength, 
						 OUT_LENGTH_BOUNDED_Z( dataMaxLength ) \
							int *dataLength )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtrDynamic( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );
	REQUIRES( isShortIntegerRangeNZ( dataMaxLength ) );

	/* Clear return values */
	memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	/* Create the premaster secret and wrap it using the server's public
	   key.  Note that the version that we advertise at this point is the
	   version originally offered by the client in its hello message, not
	   the version eventually negotiated for the connection.  This is
	   designed to prevent rollback attacks (but see also the comment in
	   unwrapPremasterSecret() below) */
	handshakeInfo->premasterSecretSize = SSL_SECRET_SIZE;
	handshakeInfo->premasterSecret[ 0 ] = SSL_MAJOR_VERSION;
	handshakeInfo->premasterSecret[ 1 ] = \
						intToByte( handshakeInfo->clientOfferedVersion );
	setMessageData( &msgData,
					handshakeInfo->premasterSecret + VERSIONINFO_SIZE,
					SSL_SECRET_SIZE - VERSIONINFO_SIZE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	setMechanismWrapInfo( &mechanismInfo, data, dataMaxLength,
						  handshakeInfo->premasterSecret, SSL_SECRET_SIZE, 
						  CRYPT_UNUSED, sessionInfoPtr->iKeyexCryptContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_EXPORT,
							  &mechanismInfo, MECHANISM_ENC_PKCS1_RAW );
	if( cryptStatusOK( status ) )
		*dataLength = mechanismInfo.wrappedDataLength;
	clearMechanismInfo( &mechanismInfo );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int unwrapPremasterSecret( INOUT SESSION_INFO *sessionInfoPtr, 
						   INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						   IN_BUFFER( dataLength ) const void *data, 
						   IN_LENGTH_SHORT const int dataLength )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isReadPtrDynamic( data, dataLength ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );
	REQUIRES( isShortIntegerRangeNZ( dataLength ) );

	/* Decrypt the encrypted premaster secret.  In theory we could
	   explicitly defend against Bleichenbacher-type attacks at this point
	   by setting the premaster secret to a pseudorandom value if we get a
	   bad data or (later) an incorrect version error and continuing as
	   normal, however the attack depends on the server returning
	   information required to pinpoint the cause of the failure and
	   cryptlib just returns a generic "failed" response for any handshake
	   failure, so this explicit defence isn't really necessary, and not
	   doing this avoids a trivial DoS attack in which a client sends us
	   junk and forces us to continue with the handshake even though we've
	   detected that it's junk.

	   There's a second, lower-grade level of oracle that an attacker can
	   use in the version check if they can distinguish between a decrypt 
	   failure due to bad PKCS #1 padding and a failure due to a bad version 
	   number (see "Attacking RSA-based Sessions in SSL/TLS", Vlastimil
	   Klima, Ondrej Pokorny, and Tomas Rosa, CHES'03), or many other later 
	   variants of the Bleichenbacher attack that target additional features
	   like missing padding terminators or padding terminators in odd 
	   locations (leading to incorrect payload sizes), and so on.
	   
	   If we use the Bleichenbacher defence and continue the handshake on 
	   bad padding but bail out on a bad version then the two cases can be 
	   distinguished, however since cryptlib bails out immediately in either 
	   case the two shouldn't be distinguishable */
	handshakeInfo->premasterSecretSize = SSL_SECRET_SIZE;
	setMechanismWrapInfo( &mechanismInfo, ( MESSAGE_CAST ) data, dataLength,
						  handshakeInfo->premasterSecret, SSL_SECRET_SIZE, 
						  CRYPT_UNUSED, sessionInfoPtr->privateKey );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_IMPORT,
							  &mechanismInfo, MECHANISM_ENC_PKCS1_RAW );
	if( cryptStatusOK( status ) && \
		mechanismInfo.keyDataLength != SSL_SECRET_SIZE )
		status = CRYPT_ERROR_BADDATA;
	clearMechanismInfo( &mechanismInfo );
	if( cryptStatusError( status ) )
		{
		/* This is a pretty nonspecific error message, but it's useful for
		   diagnosing general decryption problems, as well as the potential 
		   presence of various types of oracle attacks */
		if( status == CRYPT_ERROR_BADDATA )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "RSA decryption of premaster secret produced invalid "
					  "PKCS #1 data" ) );
			}
		retExt( status,
				( status , SESSION_ERRINFO, 
				  "RSA decryption of premaster secret failed" ) );
		}

	/* Make sure that it looks OK.  Note that the version that we check for
	   at this point is the version originally offered by the client in its
	   hello message, not the version eventually negotiated for the
	   connection.  This is designed to prevent rollback attacks */
	if( handshakeInfo->premasterSecret[ 0 ] != SSL_MAJOR_VERSION || \
		handshakeInfo->premasterSecret[ 1 ] != handshakeInfo->clientOfferedVersion )
		{
#if 0	/* 18/12/17 Unlikely to still be an issue */
		/* Microsoft braindamage, older versions of MSIE send the wrong 
		   version number for the premaster secret (making it look like a 
		   rollback attack) so if we're expecting 3.1 from the client and 
		   get 3.0 in the premaster secret then it's MSIE screwing up.  Note 
		   that this bug-check is only applied for SSL and TLS 1.0, for TLS 
		   1.1 and later the check of the version is mandatory */
		if( handshakeInfo->originalVersion <= SSL_MINOR_VERSION_TLS && \
			handshakeInfo->clientOfferedVersion == SSL_MINOR_VERSION_TLS && \
			handshakeInfo->premasterSecret[ 0 ] == SSL_MAJOR_VERSION && \
			handshakeInfo->premasterSecret[ 1 ] == SSL_MINOR_VERSION_SSL )
			{
			setErrorString( ( ERROR_INFO * ) &sessionInfoPtr->errorInfo, 
							"Warning: Accepting invalid premaster secret "
							"version 3.0 (MSIE bug)", 66 );
			}
		else
#endif /* 0 */
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid premaster secret version data 0x%02X 0x%02X, "
					  "expected 0x03 0x%02X",
					  handshakeInfo->premasterSecret[ 0 ],
					  handshakeInfo->premasterSecret[ 1 ],
					  handshakeInfo->clientOfferedVersion ) );
			}
		}

	return( CRYPT_OK );
	}
#endif /* USE_RSA_SUITES */

/****************************************************************************
*																			*
*				Premaster -> Master -> Key Material Functions				*
*																			*
****************************************************************************/

/* Convert a pre-master secret to a master secret, and a master secret to
   keying material */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int premasterToMaster( const SESSION_INFO *sessionInfoPtr, 
							  const SSL_HANDSHAKE_INFO *handshakeInfo, 
							  OUT_BUFFER_FIXED( masterSecretLength ) \
									void *masterSecret, 
							  IN_LENGTH_SHORT const int masterSecretLength )
	{
	MECHANISM_DERIVE_INFO mechanismInfo;
	BYTE nonceBuffer[ 64 + SSL_NONCE_SIZE + SSL_NONCE_SIZE + 8 ];
	int nonceSize;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isReadPtrDynamic( masterSecret, masterSecretLength ) );

	REQUIRES( isShortIntegerRangeNZ( masterSecretLength ) );

	DEBUG_PUTS(( "Premaster secret:" ));
	DEBUG_DUMP_DATA( handshakeInfo->premasterSecret, 
					 handshakeInfo->premasterSecretSize );
#ifdef USE_SSL3
	if( sessionInfoPtr->version == SSL_MINOR_VERSION_SSL )
		{
		memcpy( nonceBuffer, handshakeInfo->clientNonce, SSL_NONCE_SIZE );
		memcpy( nonceBuffer + SSL_NONCE_SIZE, handshakeInfo->serverNonce,
				SSL_NONCE_SIZE );
		setMechanismDeriveInfo( &mechanismInfo, masterSecret,
								masterSecretLength, 
								handshakeInfo->premasterSecret,
								handshakeInfo->premasterSecretSize,
								( CRYPT_ALGO_TYPE ) CRYPT_USE_DEFAULT, 
								nonceBuffer, 
								SSL_NONCE_SIZE + SSL_NONCE_SIZE, 1 );
		return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
								 &mechanismInfo, MECHANISM_DERIVE_SSL ) );
		}
#endif /* USE_SSL3 */

	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_EMS ) )
		{
		REQUIRES( boundsCheck( 22, handshakeInfo->sessionHashSize, 
							   64 + SSL_NONCE_SIZE + SSL_NONCE_SIZE ) );
		memcpy( nonceBuffer, "extended master secret", 22 );
		memcpy( nonceBuffer + 22, handshakeInfo->sessionHash, 
				handshakeInfo->sessionHashSize );
		nonceSize = 22 + handshakeInfo->sessionHashSize;
		}
	else
		{
		memcpy( nonceBuffer, "master secret", 13 );
		memcpy( nonceBuffer + 13, handshakeInfo->clientNonce, 
				SSL_NONCE_SIZE );
		memcpy( nonceBuffer + 13 + SSL_NONCE_SIZE, 
				handshakeInfo->serverNonce, SSL_NONCE_SIZE );
		nonceSize = 13 + SSL_NONCE_SIZE + SSL_NONCE_SIZE;
		}
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		setMechanismDeriveInfo( &mechanismInfo, masterSecret, 
								masterSecretLength, 
								handshakeInfo->premasterSecret,
								handshakeInfo->premasterSecretSize,
								CRYPT_ALGO_SHA2, nonceBuffer, nonceSize, 1 );
		if( handshakeInfo->integrityAlgoParam != 0 )
			mechanismInfo.hashParam = handshakeInfo->integrityAlgoParam;
		return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
								 &mechanismInfo, MECHANISM_DERIVE_TLS12 ) );
		}
	setMechanismDeriveInfo( &mechanismInfo, masterSecret, masterSecretLength,
							handshakeInfo->premasterSecret,
							handshakeInfo->premasterSecretSize,
							( CRYPT_ALGO_TYPE ) CRYPT_USE_DEFAULT, 
							nonceBuffer, nonceSize, 1 );
	return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
							 &mechanismInfo, MECHANISM_DERIVE_TLS ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
static int masterToKeys( const SESSION_INFO *sessionInfoPtr, 
						 const SSL_HANDSHAKE_INFO *handshakeInfo, 
						 IN_BUFFER( masterSecretLength ) \
								const void *masterSecret, 
						 IN_LENGTH_SHORT const int masterSecretLength,
						 OUT_BUFFER_FIXED( keyBlockLength ) void *keyBlock, 
						 IN_LENGTH_SHORT const int keyBlockLength )
	{
	MECHANISM_DERIVE_INFO mechanismInfo;
	BYTE nonceBuffer[ 64 + SSL_NONCE_SIZE + SSL_NONCE_SIZE + 8 ];

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isReadPtrDynamic( masterSecret, masterSecretLength ) );
	assert( isWritePtrDynamic( keyBlock, keyBlockLength ) );

	REQUIRES( isShortIntegerRangeNZ( masterSecretLength ) );
	REQUIRES( isShortIntegerRangeNZ( keyBlockLength ) );

	DEBUG_PUTS(( "Master secret:" ));
	DEBUG_DUMP_DATA( masterSecret, masterSecretLength );
#ifdef USE_SSL3
	if( sessionInfoPtr->version == SSL_MINOR_VERSION_SSL )
		{
		memcpy( nonceBuffer, handshakeInfo->serverNonce, SSL_NONCE_SIZE );
		memcpy( nonceBuffer + SSL_NONCE_SIZE, handshakeInfo->clientNonce,
				SSL_NONCE_SIZE );
		setMechanismDeriveInfo( &mechanismInfo, keyBlock, keyBlockLength,
								masterSecret, masterSecretLength, 
								( CRYPT_ALGO_TYPE ) CRYPT_USE_DEFAULT, 
								nonceBuffer, 
								SSL_NONCE_SIZE + SSL_NONCE_SIZE, 1 );
		return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
								 &mechanismInfo, MECHANISM_DERIVE_SSL ) );
		}
#endif /* USE_SSL3 */

	memcpy( nonceBuffer, "key expansion", 13 );
	memcpy( nonceBuffer + 13, handshakeInfo->serverNonce, SSL_NONCE_SIZE );
	memcpy( nonceBuffer + 13 + SSL_NONCE_SIZE, handshakeInfo->clientNonce,
			SSL_NONCE_SIZE );
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		setMechanismDeriveInfo( &mechanismInfo, keyBlock, keyBlockLength,
								masterSecret, masterSecretLength, 
								CRYPT_ALGO_SHA2, nonceBuffer, 
								13 + SSL_NONCE_SIZE + SSL_NONCE_SIZE, 1 );
		if( handshakeInfo->integrityAlgoParam != 0 )
			mechanismInfo.hashParam = handshakeInfo->integrityAlgoParam;
		return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
								 &mechanismInfo, MECHANISM_DERIVE_TLS12 ) );
		}
	setMechanismDeriveInfo( &mechanismInfo, keyBlock, keyBlockLength,
							masterSecret, masterSecretLength, 
							( CRYPT_ALGO_TYPE ) CRYPT_USE_DEFAULT, 
							nonceBuffer, 
							13 + SSL_NONCE_SIZE + SSL_NONCE_SIZE, 1 );
	return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
							 &mechanismInfo, MECHANISM_DERIVE_TLS ) );
	}

#ifdef USE_EAP

/* Convert a master secret to additional keying material.  Note that we 
   can't use masterToKeys() here because the sub-protocols that use these
   derived values reverse the order of the nonces */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 7 ) ) \
static int masterToKeydata( const SESSION_INFO *sessionInfoPtr, 
							const SSL_HANDSHAKE_INFO *handshakeInfo, 
							IN_BUFFER( masterSecretLength ) \
								const void *masterSecret, 
							IN_LENGTH_SHORT const int masterSecretLength,
							IN_BUFFER( diversifierLength ) \
								const void *diversifier,
							IN_LENGTH_TEXT const int diversifierLength,
							OUT_BUFFER_FIXED( keyBlockLength ) void *keyBlock, 
							IN_LENGTH_SHORT const int keyBlockLength )
	{
	MECHANISM_DERIVE_INFO mechanismInfo;
	BYTE nonceBuffer[ CRYPT_MAX_TEXTSIZE + SSL_NONCE_SIZE + SSL_NONCE_SIZE + 8 ];

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isReadPtrDynamic( masterSecret, masterSecretLength ) );
	assert( isReadPtrDynamic( diversifier, diversifierLength ) );
	assert( isWritePtrDynamic( keyBlock, keyBlockLength ) );

	REQUIRES( isShortIntegerRangeNZ( masterSecretLength ) );
	REQUIRES( diversifierLength >= 1 && \
			  diversifierLength <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( isShortIntegerRangeNZ( keyBlockLength ) );

	memcpy( nonceBuffer, diversifier, diversifierLength );
	memcpy( nonceBuffer + diversifierLength, 
			handshakeInfo->clientNonce, SSL_NONCE_SIZE );
	memcpy( nonceBuffer + diversifierLength + SSL_NONCE_SIZE, 
			handshakeInfo->serverNonce, SSL_NONCE_SIZE );
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		setMechanismDeriveInfo( &mechanismInfo, keyBlock, keyBlockLength,
								masterSecret, masterSecretLength, 
								CRYPT_ALGO_SHA2, nonceBuffer, 
								diversifierLength + SSL_NONCE_SIZE + SSL_NONCE_SIZE, 1 );
		if( handshakeInfo->integrityAlgoParam != 0 )
			mechanismInfo.hashParam = handshakeInfo->integrityAlgoParam;
		return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
								 &mechanismInfo, MECHANISM_DERIVE_TLS12 ) );
		}
	setMechanismDeriveInfo( &mechanismInfo, keyBlock, keyBlockLength,
							masterSecret, masterSecretLength, 
							( CRYPT_ALGO_TYPE ) CRYPT_USE_DEFAULT, 
							nonceBuffer, 
							diversifierLength + SSL_NONCE_SIZE + SSL_NONCE_SIZE, 1 );
	return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
							 &mechanismInfo, MECHANISM_DERIVE_TLS ) );
	}
#endif /* USE_EAP */

/****************************************************************************
*																			*
*								Key-load Functions							*
*																			*
****************************************************************************/

/* Load the SSL/TLS cryptovariables */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int loadKeys( INOUT SESSION_INFO *sessionInfoPtr,
					 const SSL_HANDSHAKE_INFO *handshakeInfo,
					 IN_BUFFER( keyBlockLength ) const void *keyBlock, 
					 IN_LENGTH_SHORT_MIN( 16 ) const int keyBlockLength,
					 const BOOLEAN isClient )
	{
#ifdef USE_SSL3
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
#endif /* USE_SSL3 */
	MESSAGE_DATA msgData;
	BYTE *keyBlockPtr = ( BYTE * ) keyBlock;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isReadPtrDynamic( keyBlock, keyBlockLength ) );

	REQUIRES( keyBlockLength >= ( sessionInfoPtr->authBlocksize * 2 ) + \
								( handshakeInfo->cryptKeysize * 2 ) + \
								( sessionInfoPtr->cryptBlocksize * 2 ) && \
			  keyBlockLength < MAX_INTLENGTH_SHORT );
	REQUIRES( isClient == TRUE || isClient == FALSE );

	/* Load the keys and secrets:

		( client_write_mac || server_write_mac || \
		  client_write_key || server_write_key || \
		  client_write_iv  || server_write_iv )

	   First we load the MAC keys.  For TLS these are proper MAC keys, for
	   SSL we have to build the proto-HMAC ourselves from a straight hash
	   context so we store the raw cryptovariables rather than loading them
	   into a context, and if we're using GCM we skip them since the 
	   encryption key also functions as the authentication key */
	if( !TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_GCM ) )
		{
#ifdef USE_SSL3
		if( sessionInfoPtr->version == SSL_MINOR_VERSION_SSL )
			{
			ENSURES( rangeCheck( sessionInfoPtr->authBlocksize, 0,
								 CRYPT_MAX_HASHSIZE ) );
			REQUIRES( boundsCheck( sessionInfoPtr->authBlocksize, 
								   sessionInfoPtr->authBlocksize, 
								   keyBlockLength ) );
			memcpy( isClient ? sslInfo->macWriteSecret : sslInfo->macReadSecret,
					keyBlockPtr, sessionInfoPtr->authBlocksize );
			memcpy( isClient ? sslInfo->macReadSecret : sslInfo->macWriteSecret,
					keyBlockPtr + sessionInfoPtr->authBlocksize,
					sessionInfoPtr->authBlocksize );
			}
		else
#endif /* USE_SSL3 */
			{
			setMessageData( &msgData, keyBlockPtr, 
							sessionInfoPtr->authBlocksize );
			status = krnlSendMessage( isClient ? \
											sessionInfoPtr->iAuthOutContext : \
											sessionInfoPtr->iAuthInContext,
									  IMESSAGE_SETATTRIBUTE_S, &msgData,
									  CRYPT_CTXINFO_KEY );
			if( cryptStatusError( status ) )
				return( status );
			setMessageData( &msgData, 
							keyBlockPtr + sessionInfoPtr->authBlocksize,
							sessionInfoPtr->authBlocksize );
			status = krnlSendMessage( isClient ? \
											sessionInfoPtr->iAuthInContext: \
											sessionInfoPtr->iAuthOutContext,
									  IMESSAGE_SETATTRIBUTE_S, &msgData,
									  CRYPT_CTXINFO_KEY );
			if( cryptStatusError( status ) )
				return( status );
			}
		keyBlockPtr += sessionInfoPtr->authBlocksize * 2;
		}

	/* Then we load the encryption keys */
	setMessageData( &msgData, keyBlockPtr, handshakeInfo->cryptKeysize );
	status = krnlSendMessage( isClient ? \
									sessionInfoPtr->iCryptOutContext : \
									sessionInfoPtr->iCryptInContext,
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_KEY );
	keyBlockPtr += handshakeInfo->cryptKeysize;
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, keyBlockPtr, handshakeInfo->cryptKeysize );
	status = krnlSendMessage( isClient ? \
									sessionInfoPtr->iCryptInContext : \
									sessionInfoPtr->iCryptOutContext,
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_KEY );
	keyBlockPtr += handshakeInfo->cryptKeysize;
	if( cryptStatusError( status ) )
		return( status );

	/* If we're using a stream cipher, there are no IVs */
	if( isStreamCipher( sessionInfoPtr->cryptAlgo ) )
		return( CRYPT_OK );	/* No IV, we're done */

	/* If we're using GCM then the IV is composed of two parts, an explicit 
	   portion that's sent with every packet and an implicit portion that's 
	   derived from the master secret */
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_GCM ) )
		{
		memcpy( isClient ? sessionInfoPtr->sessionSSL->gcmWriteSalt : \
						   sessionInfoPtr->sessionSSL->gcmReadSalt, 
				keyBlockPtr, GCM_SALT_SIZE );
		memcpy( isClient ? sessionInfoPtr->sessionSSL->gcmReadSalt : \
						   sessionInfoPtr->sessionSSL->gcmWriteSalt, 
				keyBlockPtr + GCM_SALT_SIZE, GCM_SALT_SIZE );
		sessionInfoPtr->sessionSSL->gcmSaltSize = GCM_SALT_SIZE;

		return( CRYPT_OK );
		}

	/* It's a standard block cipher, load the IVs.  This load is actually 
	   redundant for TLS 1.1+ since it uses explicit IVs, but it's easier to 
	   just do it anyway */
	setMessageData( &msgData, keyBlockPtr,
					sessionInfoPtr->cryptBlocksize );
	krnlSendMessage( isClient ? sessionInfoPtr->iCryptOutContext : \
								sessionInfoPtr->iCryptInContext,
					 IMESSAGE_SETATTRIBUTE_S, &msgData,
					 CRYPT_CTXINFO_IV );
	keyBlockPtr += sessionInfoPtr->cryptBlocksize;
	setMessageData( &msgData, keyBlockPtr,
					sessionInfoPtr->cryptBlocksize );
	return( krnlSendMessage( isClient ? sessionInfoPtr->iCryptInContext : \
										sessionInfoPtr->iCryptOutContext,
							 IMESSAGE_SETATTRIBUTE_S, &msgData,
							 CRYPT_CTXINFO_IV ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int initCryptoSSL( INOUT SESSION_INFO *sessionInfoPtr,
				   INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
				   OUT_BUFFER_FIXED( masterSecretSize ) void *masterSecret,
				   IN_LENGTH_SHORT const int masterSecretSize,
				   const BOOLEAN isClient,
				   const BOOLEAN isResumedSession )
	{
	BYTE keyBlock[ MAX_KEYBLOCK_SIZE + 8 ];
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtrDynamic( masterSecret, masterSecretSize ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );
	REQUIRES( isShortIntegerRangeNZ( masterSecretSize ) );
	REQUIRES( isClient == TRUE || isClient == FALSE );
	REQUIRES( isResumedSession == TRUE || isResumedSession == FALSE );

	/* Create the security contexts required for the session */
	status = initSecurityContextsSSL( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a fresh (i.e. non-cached) session, convert the premaster 
	   secret into the master secret */
	if( !isResumedSession )
		{
		status = premasterToMaster( sessionInfoPtr, handshakeInfo,
									masterSecret, masterSecretSize );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		/* We've already got the master secret present from the session that
		   we're resuming from, reuse that */
		REQUIRES( rangeCheck( handshakeInfo->premasterSecretSize, 0,
							  masterSecretSize ) );
		memcpy( masterSecret, handshakeInfo->premasterSecret,
				handshakeInfo->premasterSecretSize );
		}

	/* Convert the master secret into keying material.  Unfortunately we
	   can't delete the master secret at this point because it's still 
	   needed to calculate the MAC for the handshake messages and because 
	   we may still need it in order to add it to the session cache */
	status = masterToKeys( sessionInfoPtr, handshakeInfo, masterSecret,
						   masterSecretSize, keyBlock, MAX_KEYBLOCK_SIZE );
	if( cryptStatusError( status ) )
		{
		zeroise( masterSecret, masterSecretSize );
		return( status );
		}

	/* Load the keys and secrets */
	status = loadKeys( sessionInfoPtr, handshakeInfo, keyBlock, 
					   MAX_KEYBLOCK_SIZE, isClient );
	zeroise( keyBlock, MAX_KEYBLOCK_SIZE );
	if( cryptStatusError( status ) )
		{
		zeroise( masterSecret, masterSecretSize );
		return( status );
		}

	return( CRYPT_OK );
	}

/* TLS versions greater than 1.0 prepend an explicit IV to the data, the
   following function loads this from the packet data stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int loadExplicitIV( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT STREAM *stream, 
					OUT_INT_SHORT_Z int *ivLength )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	MESSAGE_DATA msgData;
	BYTE iv[ CRYPT_MAX_IVSIZE + 8 ];
	int ivSize = sslInfo->ivSize, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( ivLength, sizeof( int ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* Clear return value */
	*ivLength = 0;

	/* Read and load the IV */
	status = sread( stream, iv, sslInfo->ivSize );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Packet IV read" ) );
		}
	INJECT_FAULT( SESSION_SSL_CORRUPT_IV, SESSION_SSL_CORRUPT_IV_1 );
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_GCM ) )
		{
		/* If we're using GCM then the IV has to be assembled from the 
		   implicit and explicit portions */
		REQUIRES( boundsCheck( sslInfo->gcmSaltSize, sslInfo->ivSize,
							   CRYPT_MAX_IVSIZE ) );
		memmove( iv + sslInfo->gcmSaltSize, iv, sslInfo->ivSize );
		memcpy( iv, sslInfo->gcmReadSalt, sslInfo->gcmSaltSize );
		ivSize += sslInfo->gcmSaltSize;
		}
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_ENCTHENMAC ) )
		{
		/* If we're using encrypt-then-MAC then we have to save a copy of
		   the IV for MAC'ing when the packet is processed */
		REQUIRES( rangeCheck( sslInfo->ivSize, 1, CRYPT_MAX_IVSIZE ) );
		memcpy( sslInfo->iv, iv, sslInfo->ivSize );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, iv, ivSize );
		status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
								  IMESSAGE_SETATTRIBUTE_S, &msgData,
								  CRYPT_CTXINFO_IV );
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Packet IV load failed" ) );
		}

	/* Tell the caller how much data we've consumed */
	*ivLength = sslInfo->ivSize;

	return( CRYPT_OK );
	}

/* The EAP protocols that use SSL/TLS derive additional keying data from the
   SSL/TLS master secret.  The following function creates this subprotocol-
   specific additional keying material and adds it as session attributes */

#ifdef USE_EAP

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int addDerivedKeydata( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
					   IN_BUFFER( masterSecretSize ) void *masterSecret,
					   IN_LENGTH_SHORT const int masterSecretSize,
					   IN_ENUM( CRYPT_SUBPROTOCOL ) \
							const CRYPT_SUBPROTOCOL_TYPE type )
	{
	const char *challengeDiversifier = NULL, *keyDiversifier = NULL;
	int challengeDiversifierLength, keyDiversifierLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtrDynamic( masterSecret, masterSecretSize ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );
	REQUIRES( isShortIntegerRangeNZ( masterSecretSize ) );
	REQUIRES( isEnumRange( type, CRYPT_SUBPROTOCOL ) );

	/* Select the appripriate diversifier for the sub-protocol type */
	switch( type )
		{
		case CRYPT_SUBPROTOCOL_EAPTTLS:
			challengeDiversifier = "ttls challenge";
			challengeDiversifierLength = 14;
			keyDiversifier = "ttls keying material";
			keyDiversifierLength = 20;
			break;

#if 0
		case CRYPT_SUBPROTOCOL_EAPTLS:
			// Also has IV using keyDiversifier and empty master secret value
			keyDiversifier = "client EAP encryption";
			keyDiversifierLength = 21;
			break;

		case CRYPT_SUBPROTOCOL_PEAP:
			keyDiversifier = "client EAP encryption";
			keyDiversifierLength = 21;
			break;
#endif

		default:
			retIntError();
		}

	/* Set up the EAP challenge and key values */
	if( challengeDiversifier != NULL )
		{
		BYTE challengeBuffer[ 32 + 8 ];

		status = masterToKeydata( sessionInfoPtr, handshakeInfo, 
								  masterSecret, masterSecretSize,
								  challengeDiversifier, 
								  challengeDiversifierLength,
								  challengeBuffer, 32 );
		if( cryptStatusOK( status ) )
			{
			DEBUG_PUTS(( "EAP challenge:" ));
			DEBUG_DUMP_DATA( challengeBuffer, 32 );
			status = addSessionInfoS( sessionInfoPtr, 
									  CRYPT_SESSINFO_SSL_EAPCHALLENGE,
									  challengeBuffer, 32 );
			}
		zeroise( challengeBuffer, 32 );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, SESSION_ERRINFO, 
					  "EAP challenge value initialistion failed" ) );
			}
		}
	if( keyDiversifier != NULL )
		{
		BYTE keyBuffer[ 128 + 8 ];

		status = masterToKeydata( sessionInfoPtr, handshakeInfo, 
								  masterSecret, masterSecretSize,
								  keyDiversifier, 
								  keyDiversifierLength,
								  keyBuffer, 128 );
		if( cryptStatusOK( status ) )
			{
			DEBUG_PUTS(( "EAP key:" ));
			DEBUG_DUMP_DATA( keyBuffer, 128 );
			status = addSessionInfoS( sessionInfoPtr, 
									  CRYPT_SESSINFO_SSL_EAPKEY,
									  keyBuffer, 128 );
			}
		zeroise( keyBuffer, 128 );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, SESSION_ERRINFO, 
					  "EAP keying value initialistion failed" ) );
			}
		}
	return( CRYPT_OK );
	}
#endif /* USE_EAP */
#endif /* USE_SSL */
