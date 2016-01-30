/****************************************************************************
*																			*
*						  cryptlib SSHv2 Crypto Routines					*
*						Copyright Peter Gutmann 1998-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "session.h"
  #include "ssh.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "session/session.h"
  #include "session/ssh.h"
#endif /* Compiler-specific includes */

#ifdef USE_SSH

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Some versions of SSH want to use AES-CTR instead of AES-CBC, which is a 
   pain to deal with because it's not a standard encryption mode for
   cryptlib (or much of anything else).  To deal with this we synthesise it
   from ECB mode */

#if 0	/* Not needed at the moment */

#define KSG_BUFFER_SIZE		512

STDC_NONNULL_ARG( ( 1 ) ) \
static void incCtr( INOUT_BUFFER_FIXED( blockSize ) void *ctr,
					IN_LENGTH_IV const int blockSize )
	{
	BYTE *ctrPtr = ctr;
	int i;

	REQUIRES_V( blockSize > 0 && blockSize <= CRYPT_MAX_IVSIZE );

	/* Walk along the counter incrementing each byte if required */
	for( i = blockSize - 1; i >= 0; i-- )
		{
		if( ctrPtr[ i ]++ != 0 )
			break;
		}
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int ctrModeCrypt( IN_HANDLE const CRYPT_CONTEXT iCryptContext,
				  INOUT_BUFFER_FIXED( dataLength ) void *data,
				  IN_LENGTH const int dataLength )
	{
	BYTE ksgBuffer[ KSG_BUFFER_SIZE + 8 ];
	BYTE *dataPtr = data;
	int length, iterationCount;

	assert( isWritePtr( data, dataLength ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );

	/* Encrypt/decrypt the data in CTR mode KSG_BUFFER_SIZE bytes at a 
	   time */
	for( length = dataLength, iterationCount = 0;
		 length > 0 && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		const int ksgLen = min( length, KSG_BUFFER_SIZE );
		int i, status;

		/* Fill the KSG buffer with the counter values and encrypt them in
		   ECB mode to get the CTR cipher stream */
		for( i = 0; i < ksgLen; i += blockSize )
			{
			incCtr( ctr, blockSize );
			memcpy( ksgBuffer + i, ctr, blockSize );
			}
		status = krnlSendMessage( iCryptContext, IMESSAGE_CTX_ENCRYPT,
								  ksgBuffer, ksgLen );
		if( cryptStatusError( status ) )
			{
			zeroise( ksgBuffer, KSG_BUFFER_SIZE );
			return( status );
			}

		/* Mask/unmask the data with the KSG buffer contents and move on to 
		   the next block */
		for( i = 0; i < ksgLen; i++ )
			dataPtr[ i ] ^= ksgBuffer[ i ];
		dataPtr += ksgLen;
		length -= ksgLen;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* Clean up */
	zeroise( ksgBuffer, KSG_BUFFER_SIZE );

	return( CRYPT_OK );
	}
#endif /* 0 */

/****************************************************************************
*																			*
*							Key Load/Init Functions							*
*																			*
****************************************************************************/

/* Load a DH key into a context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initDHcontextSSH( OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
					  OUT_LENGTH_SHORT_Z int *keySize,
					  IN_BUFFER_OPT( keyDataLength ) const void *keyData, 
					  IN_LENGTH_SHORT_Z const int keyDataLength,
					  IN_LENGTH_SHORT_OPT const int requestedKeySize )
	{
	CRYPT_CONTEXT iDHContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	int keyLength DUMMY_INIT, status;

	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( keySize, sizeof( int ) ) );
	assert( ( isReadPtr( keyData, keyDataLength ) && \
			  requestedKeySize == CRYPT_UNUSED ) || \
			( keyData == NULL && keyDataLength == 0 && \
			  requestedKeySize == CRYPT_USE_DEFAULT ) || \
			( keyData == NULL && keyDataLength == 0 && \
			  requestedKeySize >= MIN_PKCSIZE && \
			  requestedKeySize <= CRYPT_MAX_PKCSIZE ) );

	REQUIRES( ( keyData != NULL && \
				keyDataLength > 0 && \
				keyDataLength < MAX_INTLENGTH_SHORT && \
				requestedKeySize == CRYPT_UNUSED ) || \
			  ( keyData == NULL && keyDataLength == 0 && \
				requestedKeySize == CRYPT_USE_DEFAULT ) || \
			  ( keyData == NULL && keyDataLength == 0 && \
				requestedKeySize >= MIN_PKCSIZE && \
				requestedKeySize <= CRYPT_MAX_PKCSIZE ) );

	/* Clear return values */
	*iCryptContext = CRYPT_ERROR;
	*keySize = 0;

	/* Create the DH context to contain the key */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_DH );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iDHContext = createInfo.cryptHandle;
	setMessageData( &msgData, "SSH DH key", 10 );
	status = krnlSendMessage( iDHContext, IMESSAGE_SETATTRIBUTE_S, &msgData, 
							  CRYPT_CTXINFO_LABEL );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iDHContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* If we're not being given externally-supplied DH key components, get 
	   the actual key size based on the requested key size.  The spec 
	   requires that we use the smallest key size that's larger than the 
	   requested one, we allow for a small amount of slop to ensure that we 
	   don't scale up to some huge key size if the client's size calculation 
	   is off by a few bits */
	if( keyData == NULL )
		{
		status = loadDHcontext( iDHContext, 
								( requestedKeySize == CRYPT_USE_DEFAULT ) ? \
								  SSH2_DEFAULT_KEYSIZE : requestedKeySize );
		}
	else
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) keyData, keyDataLength );
		status = krnlSendMessage( iDHContext, IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_KEY_SSH );
		}
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iDHContext, IMESSAGE_GETATTRIBUTE, 
								  &keyLength, CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		{
		/* If we're trying to load the context from stored data and the load 
		   fails, record the fact that there's a problem */
		if( keyData == NULL )
			{
			DEBUG_DIAG(( "Couldn't create DH context from stored data" ));
			assert( DEBUG_WARN );
			}
		krnlSendNotifier( iDHContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iCryptContext = iDHContext;
	*keySize = keyLength;

	return( CRYPT_OK );
	}

#ifdef USE_ECDH

/* Load one of the fixed SSH ECDH keys into a context.  Since there's no SSH
   format defined for this, we use the SSL format */

static const BYTE FAR_BSS ecdh256SSL[] = {
	0x03,		/* NamedCurve */
	0x00, 0x17	/* P256 */
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initECDHcontextSSH( OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
						OUT_LENGTH_SHORT_Z int *keySize,
						IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	CRYPT_CONTEXT iECDHContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( keySize, sizeof( int ) ) );

	REQUIRES( cryptAlgo == CRYPT_ALGO_ECDH );

	/* Clear return values */
	*iCryptContext = CRYPT_ERROR;
	*keySize = 0;

	/* Create the ECDH context to contain the key */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_ECDH );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iECDHContext = createInfo.cryptHandle;
	setMessageData( &msgData, "SSH ECDH key", 12 );
	status = krnlSendMessage( iECDHContext, IMESSAGE_SETATTRIBUTE_S, &msgData, 
							  CRYPT_CTXINFO_LABEL );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iECDHContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Load the appropriate static ECDH key parameters */
	setMessageData( &msgData, ( MESSAGE_CAST ) ecdh256SSL, 3 );
	status = krnlSendMessage( iECDHContext, IMESSAGE_SETATTRIBUTE_S, &msgData, 
							  CRYPT_IATTRIBUTE_KEY_SSL );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iECDHContext, IMESSAGE_DECREFCOUNT );

		/* We got an error loading a known-good, fixed-format key, report 
		   the problem as an internal error rather than (say) a bad-data 
		   error */
		retIntError();
		}
	*iCryptContext = iECDHContext;
	*keySize = bitsToBytes( 256 );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH */

/* Complete the hashing necessary to generate a cryptovariable and send it
   to a context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 4, 6, 7, 9 ) ) \
static int loadCryptovariable( IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
							   IN_RANGE( 8, 40 ) const int attributeSize, 
							   const HASHFUNCTION hashFunction,
							   IN_RANGE( 20, 32 ) const int hashSize,
							   const HASHINFO initialHashInfo, 
							   IN_BUFFER( nonceLen ) const BYTE *nonce, 
							   IN_RANGE( 1, 4 ) const int nonceLen,
							   IN_BUFFER( dataLen ) const BYTE *data, 
							   IN_LENGTH_SHORT const int dataLen )
	{
	MESSAGE_DATA msgData;
	HASHINFO hashInfo;
	BYTE buffer[ CRYPT_MAX_KEYSIZE + 8 ];
	int status;

	assert( isReadPtr( initialHashInfo, sizeof( HASHINFO ) ) );
	assert( isReadPtr( nonce, nonceLen ) );
	assert( isReadPtr( data, dataLen ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( attribute == CRYPT_CTXINFO_IV || \
			  attribute == CRYPT_CTXINFO_KEY );
	REQUIRES( attributeSize >= 8 && attributeSize <= 40 );
	REQUIRES( hashFunction != NULL );
	REQUIRES( hashSize == 20 || hashSize == 32 );
	REQUIRES( nonceLen >= 1 && nonceLen <= 4 );
	REQUIRES( dataLen > 0 && dataLen < MAX_INTLENGTH_SHORT );

	static_assert( CRYPT_MAX_KEYSIZE >= CRYPT_MAX_HASHSIZE * 2,
				   "Key size buffer as hash target" );

	/* Complete the hashing */
	memcpy( hashInfo, initialHashInfo, sizeof( HASHINFO ) );
	hashFunction( hashInfo, NULL, 0, nonce, nonceLen, HASH_STATE_CONTINUE );
	hashFunction( hashInfo, buffer, CRYPT_MAX_HASHSIZE, data, dataLen, 
				  HASH_STATE_END );
	if( attributeSize > hashSize )
		{
		/* If we need more data than the hashing will provide in one go,
		   generate a second block as:

			hash( shared_secret || exchange_hash || data )

		   where the shared secret and exchange hash are present as the
		   precomputed data in the initial hash information and the data 
		   part is the output of the hash step above */
		memcpy( hashInfo, initialHashInfo, sizeof( HASHINFO ) );
		hashFunction( hashInfo, buffer + hashSize, CRYPT_MAX_HASHSIZE, 
					  buffer, hashSize, HASH_STATE_END );
		}
	zeroise( hashInfo, sizeof( HASHINFO ) );

	/* Send the data to the context */
	setMessageData( &msgData, buffer, attributeSize );
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, attribute );
	zeroise( buffer, CRYPT_MAX_KEYSIZE );

	return( status );
	}

/* Initialise and destroy the security contexts */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initSecurityContextsSSH( INOUT SESSION_INFO *sessionInfoPtr )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	setMessageCreateObjectInfo( &createInfo, sessionInfoPtr->cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusOK( status ) )
		{
		sessionInfoPtr->iCryptInContext = createInfo.cryptHandle;
		setMessageCreateObjectInfo( &createInfo, sessionInfoPtr->cryptAlgo );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		}
	if( cryptStatusOK( status ) )
		{
		sessionInfoPtr->iCryptOutContext = createInfo.cryptHandle;
		krnlSendMessage( sessionInfoPtr->iCryptInContext,
						 IMESSAGE_GETATTRIBUTE, &sessionInfoPtr->cryptBlocksize,
						 CRYPT_CTXINFO_BLOCKSIZE );
		}
#ifdef USE_SSH1
	if( cryptStatusOK( status ) && sessionInfoPtr->version == 1 && \
		sessionInfoPtr->cryptAlgo == CRYPT_ALGO_IDEA )
		{
		const int cryptMode = CRYPT_MODE_CFB;	/* int vs.enum */

		/* SSHv1 uses stream ciphers in places, for which we have to set the
		   mode explicitly */
		status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
								  IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &cryptMode,
								  CRYPT_CTXINFO_MODE );
		if( cryptStatusOK( status ) )
			{
			status = krnlSendMessage( sessionInfoPtr->iCryptOutContext,
									  IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &cryptMode,
									  CRYPT_CTXINFO_MODE );
			}
		}
	if( sessionInfoPtr->version == 2 )	/* Continue on to cSOK() check */
#endif /* USE_SSH1 */
	if( cryptStatusOK( status ) )
		{
		setMessageCreateObjectInfo( &createInfo, sessionInfoPtr->integrityAlgo );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusOK( status ) )
			{
			sessionInfoPtr->iAuthInContext = createInfo.cryptHandle;
			setMessageCreateObjectInfo( &createInfo,
										sessionInfoPtr->integrityAlgo );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
									  OBJECT_TYPE_CONTEXT );
			}
		if( cryptStatusOK( status ) )
			{
			sessionInfoPtr->iAuthOutContext = createInfo.cryptHandle;
			krnlSendMessage( sessionInfoPtr->iAuthInContext,
							 IMESSAGE_GETATTRIBUTE,
							 &sessionInfoPtr->authBlocksize,
							 CRYPT_CTXINFO_BLOCKSIZE );
			}
		}
	if( cryptStatusError( status ) )
		{
		/* One or more of the contexts couldn't be created, destroy all the
		   contexts created so far */
		destroySecurityContextsSSH( sessionInfoPtr );
		}
	return( status );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void destroySecurityContextsSSH( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Destroy any active contexts */
	if( sessionInfoPtr->iKeyexCryptContext != CRYPT_ERROR )
		{
		krnlSendNotifier( sessionInfoPtr->iKeyexCryptContext,
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iKeyexCryptContext = CRYPT_ERROR;
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
	}

/* Set up the security information required for the session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initSecurityInfo( INOUT SESSION_INFO *sessionInfoPtr,
					  INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	HASHFUNCTION hashFunction;
	HASHINFO initialHashInfo;
	const BOOLEAN isClient = isServer( sessionInfoPtr ) ? FALSE : TRUE;
	int keySize, ivSize DUMMY_INIT, hashSize, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	/* Create the security contexts required for the session */
	status = initSecurityContextsSSH( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
							  IMESSAGE_GETATTRIBUTE, &keySize,
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( !isStreamCipher( sessionInfoPtr->cryptAlgo ) )
		{
		status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
								  IMESSAGE_GETATTRIBUTE, &ivSize,
								  CRYPT_CTXINFO_IVSIZE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Get the hash algorithm information and pre-hash the shared secret and
	   exchange hash, which are re-used for all cryptovariables.  The
	   overall hashing is:

		hash( MPI( shared_secret ) || exchange_hash || \
			  nonce || exchange_hash )

	   Note the apparently redundant double hashing of the exchange hash,
	   this is required because the spec refers to it by two different names,
	   the exchange hash and the session ID, and then requires that both be
	   hashed (actually it's a bit more complex than that with issues 
	   related to re-keying but for now it acts as a re-hash of the same
	   data).

	   Before we can hash the shared secret we have to convert it into MPI
	   form, which we do by generating a pseudo-header and hashing that
	   separately.  The nonce is "A", "B", "C", ... */
	getHashParameters( handshakeInfo->exchangeHashAlgo, 0, &hashFunction, 
					   &hashSize );
	if( sessionInfoPtr->protocolFlags & SSH_PFLAG_NOHASHSECRET )
		{
		/* Some implementations erroneously omit the shared secret when
		   creating the keying material.  This is suboptimal but not fatal,
		   since the shared secret is also hashed into the exchange hash */
		hashFunction( initialHashInfo, NULL, 0, handshakeInfo->sessionID,
					  handshakeInfo->sessionIDlength, HASH_STATE_START );
		}
	else
		{
		STREAM stream;
		BYTE header[ 8 + 8 ];
		const int mpiLength = ( handshakeInfo->secretValue[ 0 ] & 0x80 ) ? \
								handshakeInfo->secretValueLength + 1 : \
								handshakeInfo->secretValueLength;
		int headerLength DUMMY_INIT;

		/* Hash the shared secret as an MPI.  We can't use hashAsMPI() for
		   this because it works with contexts rather than the internal hash
		   functions used here */
		sMemOpen( &stream, header, 8 );
		status = writeUint32( &stream, mpiLength );
		if( handshakeInfo->secretValue[ 0 ] & 0x80 )
			{
			/* MPIs are signed values */
			status = sputc( &stream, 0 );
			}
		if( cryptStatusOK( status ) )
			headerLength = stell( &stream );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		hashFunction( initialHashInfo, NULL, 0, header, headerLength,
					  HASH_STATE_START );
		hashFunction( initialHashInfo, NULL, 0, handshakeInfo->secretValue,
					  handshakeInfo->secretValueLength, HASH_STATE_CONTINUE );
		hashFunction( initialHashInfo, NULL, 0, handshakeInfo->sessionID,
					  handshakeInfo->sessionIDlength, HASH_STATE_CONTINUE );
		}

	/* Load the cryptovariables.  The order is:

		client_write_iv, server_write_iv
		client_write_key, server_write_key
		client_write_mac, server_write_mac

	   Although HMAC has a variable-length key and should therefore follow
	   the SSH2_FIXED_KEY_SIZE rule, the key size was in later RFC drafts
	   set to the HMAC block size.  Some implementations erroneously use
	   the fixed-size key, so we adjust the HMAC key size if we're talking
	   to one of these */
	if( !isStreamCipher( sessionInfoPtr->cryptAlgo ) )
		{
		status = loadCryptovariable( isClient ? \
										sessionInfoPtr->iCryptOutContext : \
										sessionInfoPtr->iCryptInContext,
									 CRYPT_CTXINFO_IV, ivSize,
									 hashFunction, hashSize, 
									 initialHashInfo, MKDATA( "A" ), 1,
									 handshakeInfo->sessionID,
									 handshakeInfo->sessionIDlength );
		if( cryptStatusOK( status ) )
			status = loadCryptovariable( isClient ? \
											sessionInfoPtr->iCryptInContext : \
											sessionInfoPtr->iCryptOutContext,
										 CRYPT_CTXINFO_IV, ivSize,
										 hashFunction, hashSize,
										 initialHashInfo, MKDATA( "B" ), 1,
										 handshakeInfo->sessionID,
										 handshakeInfo->sessionIDlength );
		}
	if( cryptStatusOK( status ) )
		status = loadCryptovariable( isClient ? \
										sessionInfoPtr->iCryptOutContext : \
										sessionInfoPtr->iCryptInContext,
									 CRYPT_CTXINFO_KEY, keySize,
									 hashFunction, hashSize,
									 initialHashInfo, MKDATA( "C" ), 1,
									 handshakeInfo->sessionID,
									 handshakeInfo->sessionIDlength );
	if( cryptStatusOK( status ) )
		status = loadCryptovariable( isClient ? \
										sessionInfoPtr->iCryptInContext : \
										sessionInfoPtr->iCryptOutContext,
									 CRYPT_CTXINFO_KEY, keySize,
									 hashFunction, hashSize,
									 initialHashInfo, MKDATA( "D" ), 1,
									 handshakeInfo->sessionID,
									 handshakeInfo->sessionIDlength );
	if( cryptStatusOK( status ) )
		status = loadCryptovariable( isClient ? \
										sessionInfoPtr->iAuthOutContext : \
										sessionInfoPtr->iAuthInContext,
									 CRYPT_CTXINFO_KEY,
									 ( sessionInfoPtr->protocolFlags & \
									   SSH_PFLAG_HMACKEYSIZE ) ? \
										SSH2_FIXED_KEY_SIZE : \
										sessionInfoPtr->authBlocksize,
									 hashFunction, hashSize,
									 initialHashInfo, MKDATA( "E" ), 1,
									 handshakeInfo->sessionID,
									 handshakeInfo->sessionIDlength );
	if( cryptStatusOK( status ) )
		status = loadCryptovariable( isClient ? \
										sessionInfoPtr->iAuthInContext : \
										sessionInfoPtr->iAuthOutContext,
									 CRYPT_CTXINFO_KEY,
									 ( sessionInfoPtr->protocolFlags & \
									   SSH_PFLAG_HMACKEYSIZE ) ? \
										SSH2_FIXED_KEY_SIZE : \
										sessionInfoPtr->authBlocksize,
									 hashFunction, hashSize,
									 initialHashInfo, MKDATA( "F" ), 1,
									 handshakeInfo->sessionID,
									 handshakeInfo->sessionIDlength );
	zeroise( initialHashInfo, sizeof( HASHINFO ) );
	return( status );
	}

/****************************************************************************
*																			*
*								Hash/MAC Data								*
*																			*
****************************************************************************/

/* Hash a value encoded as an SSH string and as an MPI */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int hashAsString( IN_HANDLE const CRYPT_CONTEXT iHashContext,
				  IN_BUFFER( dataLength ) const BYTE *data, 
				  IN_LENGTH_SHORT const int dataLength )
	{
	STREAM stream;
	BYTE buffer[ 128 + 8 ];
	BOOLEAN copiedToBuffer = FALSE;
	int status;

	assert( isReadPtr( data, dataLength ) );

	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH_SHORT );

	/* Prepend the string length to the data and hash it.  If it'll fit into
	   the buffer we copy it over to save a kernel call */
	sMemOpen( &stream, buffer, 128 );
	status = writeUint32( &stream, dataLength );
	if( cryptStatusOK( status ) && dataLength <= sMemDataLeft( &stream ) )
		{
		/* The data will fit into the buffer, copy it so that it can be 
		   hashed in one go */
		status = swrite( &stream, data, dataLength );
		copiedToBuffer = TRUE;
		}
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
								  buffer, stell( &stream ) );
	if( cryptStatusOK( status ) && !copiedToBuffer )
		{
		/* The data didn't fit into the buffer, hash it separately */
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
								  ( MESSAGE_CAST ) data, dataLength );
		}
	sMemClose( &stream );
	if( copiedToBuffer )
		zeroise( buffer, 128 );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int hashAsMPI( IN_HANDLE const CRYPT_CONTEXT iHashContext, 
			   IN_BUFFER( dataLength ) const BYTE *data, 
			   IN_LENGTH_SHORT const int dataLength )
	{
	STREAM stream;
	BYTE buffer[ 8 + 8 ];
	const int length = ( data[ 0 ] & 0x80 ) ? dataLength + 1 : dataLength;
	int headerLength DUMMY_INIT, status;

	assert( isReadPtr( data, dataLength ) );

	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH_SHORT );

	/* Prepend the MPI length to the data and hash it.  Since this is often
	   sensitive data we don't take a local copy but hash it in two parts */
	sMemOpen( &stream, buffer, 8 );
	status = writeUint32( &stream, length );
	if( data[ 0 ] & 0x80 )
		{
		/* MPIs are signed values */
		status = sputc( &stream, 0 );
		}
	if( cryptStatusOK( status ) )
		headerLength = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
							  buffer, headerLength );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
								  ( MESSAGE_CAST ) data, dataLength );
	return( status );
	}

/* MAC the payload of a data packet.  Since we may not have the whole packet
   available at once we can either do this in one go or incrementally.  If 
   it's incremental then packetDataLength is the packet-length value that's
   encoded into a uint32 and hashed while dataLength is the amount of data 
   that we have available to hash right now.  If it's an atomic hash then
   dataLength is both the length and data amount and packetDataLength is 
   zero */

CHECK_RETVAL \
static int macDataSSH( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
					   IN_INT_Z const long seqNo,
					   IN_BUFFER_OPT( dataLength ) const BYTE *data, 
					   IN_DATALENGTH_Z const int dataLength,
					   IN_DATALENGTH_Z const int packetDataLength, 
					   IN_ENUM_OPT( MAC ) const MAC_TYPE macType )
	{
	int status;

	assert( ( data == NULL && dataLength == 0 ) || \
			isReadPtr( data, dataLength ) );

	REQUIRES( isHandleRangeValid( iMacContext ) );
	REQUIRES( ( macType == MAC_END && seqNo == 0 ) || \
			  ( macType != MAC_END && \
				seqNo >= 2 && seqNo < INT_MAX ) );
			  /* Since SSH starts counting packets from the first one but 
				 unlike SSL doesn't MAC them, the seqNo is already nonzero 
				 when we start */
	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( data != NULL && \
				dataLength > 0 && dataLength < MAX_BUFFER_SIZE ) );
			  /* Payload may be zero for packets where all information is 
			     contained in the header */
	REQUIRES( packetDataLength >= 0 && packetDataLength < MAX_BUFFER_SIZE );
	REQUIRES( macType >= MAC_NONE && macType < MAC_LAST );
			  /* If we're doing a non-incremental MAC operation the type is 
			     set to MAC_NONE */

	/* MAC the data and either compare the result to the stored MAC or
	   append the MAC value to the data:

		HMAC( seqNo || length || payload )

	   During the handshake process we have the entire packet at hand
	   (dataLength == packetDataLength) and can process it at once 
	   (macType == MAC_NONE).  When we're processing payload data 
	   (dataLength a subset of packetDataLength) we have to process the 
	   header separately in order to determine how much more we have to read
	   so we have to MAC the packet in two parts (macType == MAC_START/
	   MAC_END) */
	if( macType == MAC_START || macType == MAC_NONE )
		{
		STREAM stream;
		BYTE headerBuffer[ 16 + 8 ];
		int length = ( macType == MAC_NONE ) ? dataLength : packetDataLength;
		int headerLength DUMMY_INIT;

		REQUIRES( ( macType == MAC_NONE && packetDataLength == 0 ) || \
				  ( macType == MAC_START && packetDataLength >= dataLength ) );

		/* Since the payload has had the length stripped during the 
		   speculative read if we're MAC'ing read data we have to 
		   reconstruct it and hash it separately before we hash the data.  
		   If we're doing the hash in parts then the amount of data being 
		   hashed won't match the overall length so the caller needs to 
		   supply the overall packet length as well as the current data 
		   length */
		sMemOpen( &stream, headerBuffer, 16 );
		writeUint32( &stream, seqNo );
		status = writeUint32( &stream, length );
		if( cryptStatusOK( status ) )
			headerLength = stell( &stream );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		krnlSendMessage( iMacContext, IMESSAGE_DELETEATTRIBUTE, NULL,
						 CRYPT_CTXINFO_HASHVALUE );
		status = krnlSendMessage( iMacContext, IMESSAGE_CTX_HASH, 
								  headerBuffer, headerLength );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( dataLength > 0 )
		{
		status = krnlSendMessage( iMacContext, IMESSAGE_CTX_HASH,
								  ( MESSAGE_CAST ) data, dataLength );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( macType == MAC_END || macType == MAC_NONE )
		{
		status = krnlSendMessage( iMacContext, IMESSAGE_CTX_HASH, "", 0 );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int checkMacSSHIncremental( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
							IN_INT_Z const long seqNo,
							IN_BUFFER( dataMaxLength ) const BYTE *data, 
							IN_DATALENGTH const int dataMaxLength, 
							IN_DATALENGTH_Z const int dataLength, 
							IN_DATALENGTH_Z const int packetDataLength, 
							IN_ENUM( MAC ) const MAC_TYPE macType, 
							IN_RANGE( 16, CRYPT_MAX_HASHSIZE ) const int macLength )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isReadPtr( data, dataMaxLength ) );

	REQUIRES( isHandleRangeValid( iMacContext ) );
	REQUIRES( ( macType == MAC_END && seqNo == 0 ) || \
			  ( macType != MAC_END && \
				seqNo >= 2 && seqNo < INT_MAX ) );
			  /* Since SSH starts counting packets from the first one but 
				 unlike SSL doesn't MAC them, the seqNo is already nonzero 
				 when we start */
	REQUIRES( dataMaxLength > 0 && dataMaxLength < MAX_BUFFER_SIZE );
	REQUIRES( ( macType == MAC_END && dataLength == 0 ) || \
			  ( dataLength > 0 && dataLength < MAX_BUFFER_SIZE ) );
			  /* Payload size may be zero for packets where all information
			     is contained in, and has already been MACd as part of, the 
				 header */
	REQUIRES( packetDataLength >= 0 && packetDataLength < MAX_BUFFER_SIZE );
	REQUIRES( macType > MAC_NONE && macType < MAC_LAST );
	REQUIRES( macLength >= 16 && macLength <= CRYPT_MAX_HASHSIZE );
	REQUIRES( ( macType == MAC_START && \
				dataMaxLength == dataLength ) || \
			  ( macType == MAC_END && 
				dataLength + macLength <= dataMaxLength ) );

	/* MAC the payload.  If the data length is zero then there's no data 
	   payload (which can happen with things like a channel close for which 
	   the entire content is carried in the message header), only the MAC
	   data at the end, so we don't pass anything down to macDataSSH() */
	if( dataLength <= 0 )
		{
		status = macDataSSH( iMacContext, seqNo, NULL, 0, 
							 packetDataLength, macType );
		}
	else
		{
		status = macDataSSH( iMacContext, seqNo, data, dataLength, 
							 packetDataLength, macType );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If we're starting the ongoing hashing of a data block, we're done */
	if( macType == MAC_START )
		return( CRYPT_OK );

	/* Compare the calculated MAC value to the stored MAC value */
	setMessageData( &msgData, ( BYTE * ) data + dataLength, macLength );
	return( krnlSendMessage( iMacContext, IMESSAGE_COMPARE, &msgData, 
							 MESSAGE_COMPARE_HASH ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int checkMacSSH( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
				 IN_INT const long seqNo,
				 IN_BUFFER( dataMaxLength ) const BYTE *data, 
				 IN_DATALENGTH const int dataMaxLength, 
				 IN_DATALENGTH_Z const int dataLength, 
				 IN_RANGE( 16, CRYPT_MAX_HASHSIZE ) const int macLength )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isReadPtr( data, dataMaxLength ) );

	REQUIRES( isHandleRangeValid( iMacContext ) );
	REQUIRES( seqNo >= 2 && seqNo < INT_MAX );
			  /* Since SSH starts counting packets from the first one but 
				 unlike SSL doesn't MAC them, the seqNo is already nonzero 
				 when we start */
	REQUIRES( dataMaxLength > 0 && dataMaxLength < MAX_BUFFER_SIZE );
	REQUIRES( dataLength >= 0 && dataLength < MAX_BUFFER_SIZE );
	REQUIRES( macLength >= 16 && macLength <= CRYPT_MAX_HASHSIZE );
	REQUIRES( dataLength + macLength <= dataMaxLength );

	/* MAC the payload.  If the data length is zero then there's no data 
	   payload (which can happen with things like a channel close for which 
	   the entire content is carried in the message header), only the MAC
	   data at the end, so we don't pass anything down to macDataSSH() */
	if( dataLength <= 0 )
		status = macDataSSH( iMacContext, seqNo, NULL, 0, 0, MAC_NONE );
	else
		status = macDataSSH( iMacContext, seqNo, data, dataLength, 0, MAC_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/* Compare the calculated MAC value to the stored MAC value */
	setMessageData( &msgData, ( BYTE * ) data + dataLength, macLength );
	return( krnlSendMessage( iMacContext, IMESSAGE_COMPARE, &msgData, 
							 MESSAGE_COMPARE_HASH ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int createMacSSH( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
				  IN_INT const long seqNo,
				  IN_BUFFER( dataMaxLength ) BYTE *data, 
				  IN_DATALENGTH const int dataMaxLength, 
				  IN_DATALENGTH const int dataLength )
	{
	MESSAGE_DATA msgData;
	BYTE mac[ CRYPT_MAX_HASHSIZE + 8 ];
	int status;

	assert( isWritePtr( data, dataLength ) );

	REQUIRES( isHandleRangeValid( iMacContext ) );
	REQUIRES( seqNo >= 2 && seqNo < INT_MAX );
			  /* Since SSH starts counting packets from the first one but 
				 unlike SSL doesn't MAC them, the seqNo is already nonzero 
				 when we start */
	REQUIRES( dataMaxLength > 0 && dataMaxLength < MAX_BUFFER_SIZE );
	REQUIRES( dataLength > 0 && dataLength < dataMaxLength && \
			  dataLength < MAX_BUFFER_SIZE );

	/* MAC the payload */
	status = macDataSSH( iMacContext, seqNo, data, dataLength, 0, MAC_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/* Append the calculated MAC value to the data */
	setMessageData( &msgData, mac, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iMacContext, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( rangeCheck( dataLength, msgData.length, dataMaxLength ) );
	memcpy( data + dataLength, mac, msgData.length );
	
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Keyex Functions								*
*																			*
****************************************************************************/

/* Complete the DH/ECDH key agreement */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int completeKeyex( INOUT SESSION_INFO *sessionInfoPtr,
				   INOUT SSH_HANDSHAKE_INFO *handshakeInfo,
				   const BOOLEAN isServer )
	{
	KEYAGREE_PARAMS keyAgreeParams;
	MESSAGE_DATA msgData;
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	/* Read the other side's key agreement information.  Note that the size
	   check has already been performed at a higher level when the overall
	   key agreement value was read, this is a secondary check of the MPI
	   payload */
	memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
	if( isServer )
		sMemConnect( &stream, handshakeInfo->clientKeyexValue,
					 handshakeInfo->clientKeyexValueLength );
	else
		sMemConnect( &stream, handshakeInfo->serverKeyexValue,
					 handshakeInfo->serverKeyexValueLength );
	if( handshakeInfo->isECDH )
		{
		/* This is actually a String32 and not an Integer32, however the
		   formats are identical and we need to read the value as an integer
		   to take advantage of the range-checking */
		status = readInteger32( &stream, keyAgreeParams.publicValue,
								&keyAgreeParams.publicValueLen, 
								MIN_PKCSIZE_ECCPOINT, MAX_PKCSIZE_ECCPOINT );
		}
	else
		{
		status = readInteger32( &stream, keyAgreeParams.publicValue,
								&keyAgreeParams.publicValueLen, 
								MIN_PKCSIZE, CRYPT_MAX_PKCSIZE );
		}
	sMemDisconnect( &stream );
	if( cryptStatusOK( status ) )
		{
		if( handshakeInfo->isECDH )
			{
			if( !isValidECDHsize( keyAgreeParams.publicValueLen,
								  handshakeInfo->serverKeySize, 0 ) )
				status = CRYPT_ERROR_BADDATA;
			}
		else
			{
			if( !isValidDHsize( keyAgreeParams.publicValueLen,
								handshakeInfo->serverKeySize, 0 ) )
				status = CRYPT_ERROR_BADDATA;
			}
		}
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid %s phase 1 MPI",
				  handshakeInfo->isECDH ? "ECDH" : "DH" ) );
		}

	/* Perform phase 2 of the DH/ECDH key agreement */
	status = krnlSendMessage( handshakeInfo->iServerCryptContext,
							  IMESSAGE_CTX_DECRYPT, &keyAgreeParams,
							  sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusOK( status ) && handshakeInfo->isECDH )
		{
		const int xCoordLen = ( keyAgreeParams.wrappedKeyLen - 1 ) / 2;

		/* The output of the ECDH operation is an ECC point, but for some
		   unknown reason SSH only uses the x coordinate and not the full
		   point.  To work around this we have to rewrite the point as a
		   standalone x coordinate, which is relatively easy because we're
		   using an "uncompressed" point format:

			+---+---------------+---------------+
			|04	|		qx		|		qy		|
			+---+---------------+---------------+
				|<- fldSize --> |<- fldSize --> | */
		REQUIRES( keyAgreeParams.wrappedKeyLen >= MIN_PKCSIZE_ECCPOINT && \
				  keyAgreeParams.wrappedKeyLen <= MAX_PKCSIZE_ECCPOINT && \
				  ( keyAgreeParams.wrappedKeyLen & 1 ) == 1 && \
				  keyAgreeParams.wrappedKey[ 0 ] == 0x04 );
		memmove( keyAgreeParams.wrappedKey,
				 keyAgreeParams.wrappedKey + 1, xCoordLen );
		keyAgreeParams.wrappedKeyLen = xCoordLen;
		}
	if( cryptStatusOK( status ) )
		{
		ENSURES( rangeCheckZ( 0, keyAgreeParams.wrappedKeyLen, 
							  CRYPT_MAX_PKCSIZE ) );
		memcpy( handshakeInfo->secretValue, keyAgreeParams.wrappedKey,
				keyAgreeParams.wrappedKeyLen );
		handshakeInfo->secretValueLength = keyAgreeParams.wrappedKeyLen;
		}
	zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're using ephemeral DH, hash the requested keyex key length(s)
	   and DH p and g values.  Since this has been deferred until long after
	   the keyex negotiation took place (so that the original data isn't 
	   available any more) we have to recreate the original encoded values 
	   here */
	if( handshakeInfo->requestedServerKeySize > 0 )
		{
		BYTE keyexBuffer[ 128 + ( CRYPT_MAX_PKCSIZE * 2 ) + 8 ];
		const int extraLength = LENGTH_SIZE + sizeofString32( 6 );

		status = krnlSendMessage( handshakeInfo->iExchangeHashContext,
								  IMESSAGE_CTX_HASH,
								  handshakeInfo->encodedReqKeySizes,
								  handshakeInfo->encodedReqKeySizesLength );
		if( cryptStatusError( status ) )
			return( status );
		setMessageData( &msgData, keyexBuffer,
						128 + ( CRYPT_MAX_PKCSIZE * 2 ) );
		status = krnlSendMessage( handshakeInfo->iServerCryptContext,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_KEY_SSH );
		if( cryptStatusOK( status ) )
			{
			status = krnlSendMessage( handshakeInfo->iExchangeHashContext,
									  IMESSAGE_CTX_HASH, 
									  keyexBuffer + extraLength,
									  msgData.length - extraLength );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Hash the client and server DH/ECDH values and shared secret */
	status = krnlSendMessage( handshakeInfo->iExchangeHashContext, 
							  IMESSAGE_CTX_HASH,
							  handshakeInfo->clientKeyexValue,
							  handshakeInfo->clientKeyexValueLength );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( handshakeInfo->iExchangeHashContext, 
								  IMESSAGE_CTX_HASH,
								  handshakeInfo->serverKeyexValue,
								  handshakeInfo->serverKeyexValueLength );
	if( cryptStatusOK( status ) )
		status = hashAsMPI( handshakeInfo->iExchangeHashContext,
							handshakeInfo->secretValue,
							handshakeInfo->secretValueLength );
	if( cryptStatusError( status ) )
		return( status );

	/* Complete the hashing to obtain the exchange hash and then hash *that*
	   to get the hash that the server signs and sends to the client.  The
	   overall hashed data for the exchange hash is:

		string	V_C, client version string (CR and NL excluded)
		string	V_S, server version string (CR and NL excluded)
		string	I_C, client hello
		string	I_S, server hello
		string	K_S, the host key
	 [[	uint32	min, min.preferred keyex key size for ephemeral DH ]]
	  [	uint32	n, preferred keyex key size for ephemeral DH ]
	 [[	uint32	max, max.preferred keyex key size for ephemeral DH ]]
	  [	mpint	p, DH p for ephemeral DH ]
	  [	mpint	g, DH g for ephemeral DH ]
	   DH:
		mpint	e, client DH keyex value
		mpint	f, server DH keyex value
	   ECDH:
		string	Q_C, client ECDH keyex value
		string	Q_S, server ECDH keyex value
		mpint	K, the shared secret

	   The client and server version string ahd hellos and the host key were
	   hashed inline during the handshake.  The optional parameters are for
	   negotiated DH values (see the conditional-hashing code above).  The
	   double-optional parameters are for the revised version of the DH
	   negotiation mechanism, the original only had n, the revised version
	   allowed a { min, n, max } range */
	status = krnlSendMessage( handshakeInfo->iExchangeHashContext, 
							  IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, handshakeInfo->sessionID, 
						CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( handshakeInfo->iExchangeHashContext,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_CTXINFO_HASHVALUE );
		if( cryptStatusOK( status ) )
			handshakeInfo->sessionIDlength = msgData.length;
		}
	if( cryptStatusError( status ) )
		return( status );

	/* At this point we continue the hash-algorithm dance, in most cases 
	   switching back to SHA-1 if we've been using a different algorithm for 
	   the hashing so far.  This is required because while the exchange hash 
	   is calculated using the alternative algorithm that was negotiated 
	   earlier, what gets signed is a SHA-1 hash of that.  Exceptions to 
	   this occur when we're either using an ECC cipher suite or when the 
	   use of SHA-2 has been explicitly indicated, in which case we 
	   stick with SHA-2 */
	if( handshakeInfo->exchangeHashAlgo != handshakeInfo->hashAlgo )
		{
		const CRYPT_CONTEXT tempContext = handshakeInfo->iExchangeHashContext;

		handshakeInfo->iExchangeHashContext = \
				handshakeInfo->iExchangeHashAltContext;
		handshakeInfo->iExchangeHashAltContext = tempContext;
		}
	krnlSendMessage( handshakeInfo->iExchangeHashContext,
					 IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_HASHVALUE );
	krnlSendMessage( handshakeInfo->iExchangeHashContext,
					 IMESSAGE_CTX_HASH, handshakeInfo->sessionID,
					 handshakeInfo->sessionIDlength );
	INJECT_FAULT( SESSION_BADSIG_HASH, SESSION_BADSIG_HASH_SSH_1 );
	return( krnlSendMessage( handshakeInfo->iExchangeHashContext,
							 IMESSAGE_CTX_HASH, "", 0 ) );
	}
#endif /* USE_SSH */
