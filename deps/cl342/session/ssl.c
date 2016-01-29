/****************************************************************************
*																			*
*					cryptlib SSL v3/TLS Session Management					*
*					   Copyright Peter Gutmann 1998-2014					*
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
*								Utility Functions							*
*																			*
****************************************************************************/

#if defined( __WIN32__ ) && !defined( NDEBUG )

/* Dump a message to disk for diagnostic purposes.  The SSL messages are
   broken up into parts by the read/write code so that we can't use the 
   normal DEBUG_DUMP() macro but have to use a special-purpose function that 
   assembles the packet contents if required, as well as providing 
   appropriate naming */

STDC_NONNULL_ARG( ( 1 ) ) \
void debugDumpSSL( const SESSION_INFO *sessionInfoPtr,
				   IN_BUFFER_OPT( buffer1size ) const void *buffer1, 
				   IN_LENGTH_SHORT const int buffer1size,
				   IN_BUFFER_OPT( buffer2size ) const void *buffer2, 
				   IN_LENGTH_SHORT_Z const int buffer2size )
	{
	FILE *filePtr;
	static int messageCount = 1;
	const BYTE *bufPtr = buffer1;
	const BOOLEAN isRead = ( buffer2 != NULL ) ? TRUE : FALSE;
	const BOOLEAN encryptionActive = \
		( isRead && ( sessionInfoPtr->flags & SESSION_ISSECURE_READ ) ) || \
		( !isRead && ( sessionInfoPtr->flags & SESSION_ISSECURE_WRITE ) );
	char fileName[ 1024 + 8 ];

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( buffer1,  buffer1size ) );
	assert( ( buffer2 == NULL && buffer2size == 0 ) || \
			isReadPtr( buffer2, buffer2size ) );

	if( messageCount > 20 )
		return;	/* Don't dump too many messages */
	strlcpy_s( fileName, 1024, "/tmp/" );
	sprintf_s( &fileName[ 5 ], 1024, "tls3%d_%02d%c_", 
			   sessionInfoPtr->version, messageCount++, 
			   isRead ? 'r' : 'w' );
	if( bufPtr[ 0 ] == SSL_MSG_HANDSHAKE && !encryptionActive )
		{
		if( isRead && buffer2size >= 1 )
			{
			strlcat_s( fileName, 1024, 
					   getSSLHSPacketName( ( ( BYTE * ) buffer2 )[ 0 ] ) );
			}
		else
			{
			if( !isRead && buffer1size >= 6 )
				{
				strlcat_s( fileName, 1024, 
						   getSSLHSPacketName( bufPtr[ 5 ] ) );
				}
			else
				strlcat_s( fileName, 1024, "truncated_packet" );
			}
		}
	else	
		strlcat_s( fileName, 1024, getSSLPacketName( bufPtr[ 0 ] ) );
	strlcat_s( fileName, 1024, ".dat" );

#ifdef __STDC_LIB_EXT1__
	if( fopen_s( &filePtr, fileName, "wb" ) != 0 )
		filePtr = NULL;
#else
	filePtr = fopen( fileName, "wb" );
#endif /* __STDC_LIB_EXT1__ */
	if( filePtr != NULL )
		{
		fwrite( buffer1, 1, buffer1size, filePtr );
		if( buffer2 != NULL )
			fwrite( buffer2, 1, buffer2size, filePtr );
		fclose( filePtr );
		}
	}
#endif /* Windows debug mode only */

/* Initialise and destroy the handshake state information */

STDC_NONNULL_ARG( ( 1 ) ) \
static void destroyHandshakeInfo( INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	/* Destroy any active contexts.  We need to do this here (even though
	   it's also done in the general session code) to provide a clean exit in
	   case the session activation fails, so that a second activation attempt
	   doesn't overwrite still-active contexts */
	destroyHandshakeCryptInfo( handshakeInfo );

	zeroise( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initHandshakeInfo( INOUT SESSION_INFO *sessionInfoPtr,
							  OUT_ALWAYS SSL_HANDSHAKE_INFO *handshakeInfo,
							  const BOOLEAN isServer )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	memset( handshakeInfo, 0, sizeof( SSL_HANDSHAKE_INFO ) );
	if( isServer )
		initSSLserverProcessing( handshakeInfo );
	else
		initSSLclientProcessing( handshakeInfo );
	handshakeInfo->originalVersion = sessionInfoPtr->version;
	return( initHandshakeCryptInfo( sessionInfoPtr, handshakeInfo ) );
	}

/* Push and pop the handshake state */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int pushHandshakeInfo( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	const int bufPos = sessionInfoPtr->sendBufSize - \
					   sizeof( SSL_HANDSHAKE_INFO );

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
			   
	/* Save the handshake state so that we can resume the handshake later 
	   on.  This is somewhat ugly in that we need to store 
	   sizeof( SSL_HANDSHAKE_INFO ) bytes of data somewhere, one way to do 
	   this would be to allocate memory, use it for storage, and free it 
	   again, however we have the send buffer sitting there unused so we 
	   save it at the end of the send buffer.  
	   
	   This creates the slight problem that we're saving the premaster 
	   secret in the send buffer and potentially exposing it to a bug in 
	   the send code, however it would have to be a pretty unusual bug to 
	   jump into the send function and then write a block of data all the 
	   way at the end of the buffer, far past where a handshake packet would 
	   be, to the peer */
	REQUIRES( bufPos > 1024 && bufPos < sessionInfoPtr->sendBufSize - 512 );
	sslInfo->savedHandshakeInfo = sessionInfoPtr->sendBuffer + bufPos;
	memcpy( sslInfo->savedHandshakeInfo, handshakeInfo, 
			sizeof( SSL_HANDSHAKE_INFO ) );

	/* Clear the original copy of the handshake info (without doing a full 
	   cleanup of objects and so on), which leaves the copy intact */
	zeroise( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) );
	
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int popHandshakeInfo( INOUT SESSION_INFO *sessionInfoPtr,
							 OUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	/* Restore the saved handshake state so that we can continue a partially-
	   completed handshake */
	REQUIRES( sslInfo->savedHandshakeInfo != NULL );
	memcpy( handshakeInfo, sslInfo->savedHandshakeInfo, 
			sizeof( SSL_HANDSHAKE_INFO ) );
	zeroise( sslInfo->savedHandshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) );
	sslInfo->savedHandshakeInfo = NULL;
	
	return( CRYPT_OK );
	}

/* SSL uses 24-bit lengths in some places even though the maximum packet 
   length is only 16 bits (actually it's limited even further by the spec 
   to 14 bits).  To handle this odd length we define our own read/
   writeUint24() functions that always set the high byte to zero */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
int readUint24( INOUT STREAM *stream )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	status = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( status != 0 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	return( readUint16( stream ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint24( INOUT STREAM *stream, IN_LENGTH_Z const int length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES_S( length >= 0 && \
				length < MAX_PACKET_SIZE + EXTRA_PACKET_SIZE );

	sputc( stream, 0 );
	return( writeUint16( stream, length ) );
	}

/* The ECDH public value is a bit complex to process because it's the usual 
   X9.62 stuff-point-data-into-a-byte-string value, and to make things even 
   messier it's stored with an 8-bit length instead of a 16-bit one so we 
   can't even read it as an integer16U().  To work around this we have to 
   duplicate a certain amount of the integer-read code here */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readEcdhValue( INOUT STREAM *stream,
				   OUT_BUFFER( valueMaxLen, *valueLen ) void *value,
				   IN_LENGTH_SHORT_MIN( 64 ) const int valueMaxLen,
				   OUT_LENGTH_BOUNDED_Z( valueMaxLen ) int *valueLen )
	{
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( value, valueMaxLen ) );
	assert( isWritePtr( valueLen, sizeof( int ) ) );

	REQUIRES( valueMaxLen >= 64 && valueMaxLen < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	memset( value, 0, min( 16, valueMaxLen ) );
	*valueLen = 0;


	/* Get the length (as a byte) and make sure that it's valid */
	status = length = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( isShortECCKey( length / 2 ) )
		return( CRYPT_ERROR_NOSECURE );
	if( length < MIN_PKCSIZE_ECCPOINT || length > MAX_PKCSIZE_ECCPOINT )
		return( CRYPT_ERROR_BADDATA );
	*valueLen = length;

	/* Read the X9.62 point value */
	return( sread( stream, value, length ) );
	}

/* Abort a session startup */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int abortStartup( INOUT SESSION_INFO *sessionInfoPtr,
						 INOUT_OPT SSL_HANDSHAKE_INFO *handshakeInfo,
						 const BOOLEAN cleanupSecurityContexts,
						 IN_ERROR const int status )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( handshakeInfo == NULL || \
			isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	REQUIRES( cryptStatusError( status ) );

	sendHandshakeFailAlert( sessionInfoPtr, 
							( handshakeInfo != NULL && \
							  handshakeInfo->failAlertType != 0 ) ? \
								handshakeInfo->failAlertType : \
								SSL_ALERT_HANDSHAKE_FAILURE );
	if( cleanupSecurityContexts )
		destroySecurityContextsSSL( sessionInfoPtr );
	if( handshakeInfo != NULL )
		destroyHandshakeInfo( handshakeInfo );
	sNetDisconnect( &sessionInfoPtr->stream );
	return( status );
	}

#ifdef CONFIG_SUITEB

/* Check that a private key is valid for Suite B use */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkSuiteBKey( INOUT SESSION_INFO *sessionInfoPtr,
						   IN_HANDLE const CRYPT_CONTEXT cryptContext,
						   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	int keySize, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* Suite B only allows P256 and P384 keys so we need to make sure that
	   the server key is of the appropriate type and size */
	if( cryptAlgo != CRYPT_ALGO_ECDSA )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_CTXINFO_ALGO, 
					  CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ARGERROR_NUM1 );
		}
	status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE, &keySize,
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );
#ifdef CONFIG_SUITEB_TESTS 
	if( suiteBTestValue == SUITEB_TEST_SVRINVALIDCURVE && \
		keySize == bitsToBytes( 521 ) )
		return( CRYPT_OK );
#endif /* CONFIG_SUITEB_TESTS */
	if( keySize != bitsToBytes( 256 ) && keySize != bitsToBytes( 384 ) )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_CTXINFO_KEYSIZE, 
					  CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ARGERROR_NUM1 );
		}

	/* In addition if a specific crypto strength has been configured then 
	   the key size has to correspond to that strength.  At 128 bits we can
	   use both P256 and P384, but at 256 bits we have to use P384 */
	if( ( ( sessionInfoPtr->protocolFlags & \
						SSL_PFLAG_SUITEB ) == SSL_PFLAG_SUITEB_256 ) && \
		keySize != bitsToBytes( 384 ) )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_CTXINFO_KEYSIZE, 
					  CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ARGERROR_NUM1 );
		}

	return( CRYPT_OK );
	}

#ifdef CONFIG_SUITEB_TESTS 

/* Special kludge function used to enable nonstandard behaviour for Suite
   B tests.  The magic value is used in appropriate locations to enable
   nonstandard behaviour for testing purposes.  The values are listed in
   ssl.h */

SUITEB_TEST_VALUE suiteBTestValue = SUITEB_TEST_NONE;
BOOLEAN suiteBTestClientCert = FALSE;

int sslSuiteBTestConfig( const int magicValue )
	{
	REQUIRES( ( magicValue >= SUITEB_TEST_NONE && \
				magicValue < SUITEB_TEST_LAST ) || \
			  magicValue == 1000 );

	/* If it's the client-cert test indicator, set the flag and exit */
	if( magicValue == 1000 )
		{
		suiteBTestClientCert = TRUE;

		return( CRYPT_OK );
		}

	suiteBTestValue = magicValue;
	if( magicValue == 0 )
		{
		/* If we're doing a reset, clear the client-cert test indicator as
		   well */
		suiteBTestClientCert = FALSE;
		}

	return( CRYPT_OK );
	}
#endif /* CONFIG_SUITEB_TESTS  */
#endif /* CONFIG_SUITEB */

/****************************************************************************
*																			*
*						Read/Write SSL/TLS Certificate Chains				*
*																			*
****************************************************************************/

/* Read/write an SSL/TLS certificate chain:

	byte		ID = SSL_HAND_CERTIFICATE
	uint24		len
	uint24		certListLen
	uint24		certLen			| 1...n certificates ordered
	byte[]		certificate		|   leaf -> root */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int readSSLCertChain( INOUT SESSION_INFO *sessionInfoPtr, 
					  INOUT SSL_HANDSHAKE_INFO *handshakeInfo, 
					  INOUT STREAM *stream,
					  OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertChain, 
					  const BOOLEAN isServer )
	{
	CRYPT_CERTIFICATE iLocalCertChain;
	const ATTRIBUTE_LIST *fingerprintPtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1 );
	MESSAGE_DATA msgData;
	BYTE certFingerprint[ CRYPT_MAX_HASHSIZE + 8 ];
#ifdef USE_ERRMSGS
	const char *peerTypeName = isServer ? "Client" : "Server";
#endif /* USE_ERRMSGS */
#ifdef CONFIG_SUITEB
	const char *requiredLengthString = NULL;
#endif /* CONFIG_SUITEB */
	int certAlgo, certFingerprintLength, chainLength, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( iCertChain, sizeof( CRYPT_CERTIFICATE ) ) );

	/* Clear return value */
	*iCertChain = CRYPT_ERROR;

	/* Make sure that the packet header is in order */
	status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
								  SSL_HAND_CERTIFICATE, 
								  isServer ? 0 : LENGTH_SIZE + MIN_CERTSIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( isServer && ( length == 0 || length == LENGTH_SIZE ) )
		{
		/* There is one special case in which a too-short certificate packet 
		   is valid and that's where it constitutes the TLS equivalent of an 
		   SSL no-certificates alert.  SSLv3 sent an 
		   SSL_ALERT_NO_CERTIFICATE alert to indicate that the client 
		   doesn't have a certificate, which is handled by the 
		   readHSPacketSSL() call.  TLS changed this to send an empty 
		   certificate packet instead, supposedly because it lead to 
		   implementation problems (presumably it's necessary to create a 
		   state machine-based implementation to reproduce these problems, 
		   whatever they are).  The TLS 1.0 spec is ambiguous as to what 
		   constitutes an empty packet, it could be either a packet with a 
		   length of zero or a packet containing a zero-length certificate 
		   list so we check for both.  TLS 1.1 fixed this to say that that 
		   certListLen entry has a length of zero.  To report this condition 
		   we fake the error indicators for consistency with the status 
		   obtained from an SSLv3 no-certificate alert */
		retExt( CRYPT_ERROR_PERMISSION,
				( CRYPT_ERROR_PERMISSION, SESSION_ERRINFO, 
				  "Received TLS alert message: No certificate" ) );
		}
	status = chainLength = readUint24( stream );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid certificate chain length information" ) );
		}
	if( chainLength < MIN_CERTSIZE || chainLength >= MAX_INTLENGTH_SHORT || \
		chainLength != length - LENGTH_SIZE )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid certificate chain length %d, should be %d",
				  chainLength, length - LENGTH_SIZE ) );
		}

	/* Import the certificate chain.  This isn't a true certificate chain (in 
	   the sense of being degenerate PKCS #7 SignedData) but a special-case 
	   SSL/TLS-encoded certificate chain */
	status = importCertFromStream( stream, &iLocalCertChain, 
								   DEFAULTUSER_OBJECT_HANDLE,
								   CRYPT_ICERTTYPE_SSL_CERTCHAIN,
								   chainLength, KEYMGMT_FLAG_NONE );
	if( cryptStatusError( status ) )
		{
		/* There are sufficient numbers of broken certificates around that 
		   if we run into a problem importing one we provide a custom error
		   message telling the user to try again with a reduced compliance
		   level */
		if( status == CRYPT_ERROR_BADDATA || status == CRYPT_ERROR_INVALID )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "%s provided a broken/invalid certificate, try again "
					  "with a reduced level of certificate compliance "
					  "checking", peerTypeName ) );
			}
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid certificate chain data" ) );
		}

	/* Get information on the chain */
	status = krnlSendMessage( iLocalCertChain, IMESSAGE_GETATTRIBUTE,
							  &certAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalCertChain, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	setMessageData( &msgData, certFingerprint, CRYPT_MAX_HASHSIZE );
	if( fingerprintPtr != NULL )
		{
		const CRYPT_ATTRIBUTE_TYPE fingerprintAttribute = \
							( fingerprintPtr->valueLength == 32 ) ? \
								CRYPT_CERTINFO_FINGERPRINT_SHA2 : \
							CRYPT_CERTINFO_FINGERPRINT_SHA1;

		/* Use the hint provided by the fingerprint size to select the
		   appropriate algorithm to generate the fingerprint that we want
		   to compare against */
		status = krnlSendMessage( iLocalCertChain, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, fingerprintAttribute );
		}
	else
		{
		/* There's no algorithm hint available, use the default of SHA-1 */
		status = krnlSendMessage( iLocalCertChain, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalCertChain, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	certFingerprintLength = msgData.length;
	if( !isServer && certAlgo != handshakeInfo->authAlgo )
		{
		krnlSendNotifier( iLocalCertChain, IMESSAGE_DECREFCOUNT );
		retExt( CRYPT_ERROR_WRONGKEY,
				( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
				  "Server key algorithm %d doesn't match negotiated "
				  "algorithm %d", certAlgo, handshakeInfo->authAlgo ) );
		}

	/* Either compare the certificate fingerprint to a supplied one or save 
	   it for the caller to examine */
	if( fingerprintPtr != NULL )
		{
		/* The caller has supplied a certificate fingerprint, compare it to 
		   the received certificate's fingerprint to make sure that we're 
		   talking to the right system */
		if( fingerprintPtr->valueLength != certFingerprintLength || \
			memcmp( fingerprintPtr->value, certFingerprint, 
					certFingerprintLength ) )
			{
			krnlSendNotifier( iLocalCertChain, IMESSAGE_DECREFCOUNT );
			retExt( CRYPT_ERROR_WRONGKEY,
					( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
					  "%s key didn't match key fingerprint", peerTypeName ) );
			}
		}
	else
		{
		/* Remember the certificate fingerprint in case the caller wants to 
		   check it.  We don't worry if the add fails, it's a minor thing 
		   and not worth aborting the handshake for */
		( void ) addSessionInfoS( &sessionInfoPtr->attributeList,
								  CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1,
								  certFingerprint, certFingerprintLength );
		}

	/* Make sure that we can perform the required operation using the key
	   that we've been given.  For a client key we need signing capability,
	   for a server key when using DH/ECDH key agreement we also need 
	   signing capability to authenticate the DH/ECDH parameters, and for 
	   an RSA key transport key we need encryption capability.  This 
	   operation also performs a variety of additional checks alongside the 
	   obvious one so it's a good general health check before we go any 
	   further */
	status = krnlSendMessage( iLocalCertChain, IMESSAGE_CHECK, NULL,
							  isServer || \
								isKeyxAlgo( handshakeInfo->keyexAlgo ) ? \
								MESSAGE_CHECK_PKC_SIGCHECK : \
								MESSAGE_CHECK_PKC_ENCRYPT );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalCertChain, IMESSAGE_DECREFCOUNT );
		retExt( CRYPT_ERROR_WRONGKEY,
				( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
				  "%s provided a key incapable of being used for %s",
				  peerTypeName,
				  isServer ? "client authentication" : \
				  isKeyxAlgo( certAlgo ) ? "key exchange authentication" : \
										    "encryption" ) );
		}

	/* For ECC with Suite B there are additional constraints on the key
	   size */
#ifdef CONFIG_SUITEB
	status = krnlSendMessage( iLocalCertChain, IMESSAGE_GETATTRIBUTE,
							  &length, CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );
	switch( sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB )
		{
		case 0:
			/* If we're not configured for Suite B mode then there's
			   nothing to check */
			break;

		case SSL_PFLAG_SUITEB_128:
			/* 128-bit level can be P256 or P384 */
			if( length != bitsToBytes( 256 ) && \
				length != bitsToBytes( 384 ) )
				requiredLengthString = "256- or 384";
			break;

		case SSL_PFLAG_SUITEB_256:
			/* 256-bit level only allows P384 */
			if( length != bitsToBytes( 384 ) )
				requiredLengthString = "384";
			break;

		default:
			retIntError();
		}
	if( requiredLengthString != NULL )	
		{
		krnlSendNotifier( iLocalCertChain, IMESSAGE_DECREFCOUNT );
		retExt( CRYPT_ERROR_WRONGKEY,
				( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
				  "%s provided a %d-bit Suite B key, should have been a "
				  "%s-bit key", peerTypeName, bytesToBits( length ),
				  requiredLengthString ) );
		}
#endif /* CONFIG_SUITEB */

	*iCertChain = iLocalCertChain;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeSSLCertChain( INOUT SESSION_INFO *sessionInfoPtr, 
					   INOUT STREAM *stream )
	{
	int packetOffset, certListOffset DUMMY_INIT, certListEndPos, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	status = continueHSPacketStream( stream, SSL_HAND_CERTIFICATE, 
									 &packetOffset );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionInfoPtr->privateKey == CRYPT_ERROR )
		{
		/* If there's no private key available, write an empty certificate 
		   chain */
		status = writeUint24( stream, 0 );
		if( cryptStatusError( status ) )
			return( status );
		return( completeHSPacketStream( stream, packetOffset ) );
		}

	/* Write a dummy length and export the certificate list to the stream */
	status = writeUint24( stream, 0 );
	if( cryptStatusOK( status ) )
		{
		certListOffset = stell( stream );
		status = exportCertToStream( stream, sessionInfoPtr->privateKey,
									 CRYPT_ICERTFORMAT_SSL_CERTCHAIN );
		}
	if( cryptStatusError( status ) )
		return( status );
	certListEndPos = stell( stream );

	/* Go back and insert the length, then wrap up the packet */
	sseek( stream, certListOffset - LENGTH_SIZE );
	status = writeUint24( stream, certListEndPos - certListOffset );
	sseek( stream, certListEndPos );
	if( cryptStatusError( status ) )
		return( status );
	return( completeHSPacketStream( stream, packetOffset ) );
	}

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

/* Close a previously-opened SSL/TLS session */

STDC_NONNULL_ARG( ( 1 ) ) \
static void shutdownFunction( INOUT SESSION_INFO *sessionInfoPtr )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Clean up SSL/TLS-specific objects if required */
	if( sslInfo->savedHandshakeInfo != NULL )
		{
		SSL_HANDSHAKE_INFO handshakeInfo;
		int status;

		/* We got halfway through the handshake but didn't complete it, 
		   restore the handshake state and use it to shut down the
		   session.  We set a dummy status since this is handled by the
		   higher-level code that called us */
		status = popHandshakeInfo( sessionInfoPtr, &handshakeInfo );
		ENSURES_V( cryptStatusOK( status ) );
		( void ) abortStartup( sessionInfoPtr, &handshakeInfo, FALSE, 
							   CRYPT_ERROR_FAILED );
		return;
		}

	sendCloseAlert( sessionInfoPtr, FALSE );
	sNetDisconnect( &sessionInfoPtr->stream );
	}

/* Connect to an SSL/TLS server/client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int commonStartup( INOUT SESSION_INFO *sessionInfoPtr,
						  const BOOLEAN isServer )
	{
	SSL_HANDSHAKE_INFO handshakeInfo;
	BOOLEAN resumedSession = FALSE;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* TLS 1.2 switched from the MD5+SHA-1 dual hash/MACs to SHA-2 so if the
	   user has requesetd TLS 1.2 or newer we need to make sure that SHA-2
	   is available */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 && \
		!algoAvailable( CRYPT_ALGO_SHA2 ) )
		{
		retExt( CRYPT_ERROR_NOTAVAIL,
				( CRYPT_ERROR_NOTAVAIL, SESSION_ERRINFO, 
				  "TLS 1.2 and newer require the SHA-2 hash algorithms which "
				  "aren't available in this build of cryptlib" ) );
		}

	/* Begin the handshake, unless we're continuing a partially-opened 
	   session */
	if( !( sessionInfoPtr->flags & SESSION_PARTIALOPEN ) )
		{
		/* Initialise the handshake information */
		status = initHandshakeInfo( sessionInfoPtr, &handshakeInfo, 
									isServer );
		if( cryptStatusError( status ) )
			{
			return( abortStartup( sessionInfoPtr, &handshakeInfo, FALSE, 
								  status ) );
			}

		/* Exchange client/server hellos and other pleasantries */
		status = handshakeInfo.beginHandshake( sessionInfoPtr,
											   &handshakeInfo );
		if( cryptStatusError( status ) )
			{
			if( status == OK_SPECIAL )
				resumedSession = TRUE;
			else
				{
				return( abortStartup( sessionInfoPtr, &handshakeInfo, 
									  FALSE, status ) );
				}
			}

		/* Exchange keys with the server */
		if( !resumedSession )
			{
			status = handshakeInfo.exchangeKeys( sessionInfoPtr,
												 &handshakeInfo );
			if( cryptStatusError( status ) )
				return( abortStartup( sessionInfoPtr, &handshakeInfo, TRUE,
									  status ) );
			}

		/* If we're performing manual verification of the peer's 
		   certificate, let the caller know that they have to check it,
		   unless they've specified that they want to allow any certificate
		   (which implies that they'll perform the check after the handshake
		   completes) */
		if( ( sessionInfoPtr->protocolFlags & SSL_PFLAG_MANUAL_CERTCHECK ) && \
			sessionInfoPtr->authResponse != AUTHRESPONSE_SUCCESS )
			{
			/* Save the handshake state so that we can resume the handshake 
			   later on */
			status = pushHandshakeInfo( sessionInfoPtr, &handshakeInfo );
			ENSURES( cryptStatusOK( status ) );

			return( CRYPT_ENVELOPE_RESOURCE );
			}
		}
	else
		{
		/* We're continuing a partially-completed handshake, restore the 
		   handshake state */
		status = popHandshakeInfo( sessionInfoPtr, &handshakeInfo );
		ENSURES( cryptStatusOK( status ) );

		/* Reset the partial-open state since we're about to complete the 
		   open.  This is also done by the calling code once the handshake
		   completes successfully, but we want to do it preemptively 
		   unconditionally */
		sessionInfoPtr->flags &= ~SESSION_PARTIALOPEN;
		}

	/* Complete the handshake */
	status = completeHandshakeSSL( sessionInfoPtr, &handshakeInfo, !isServer,
								   resumedSession );
	destroyHandshakeInfo( &handshakeInfo );
	if( cryptStatusError( status ) )
		return( abortStartup( sessionInfoPtr, NULL, TRUE, status ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int clientStartup( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Complete the handshake using the common client/server code */
	return( commonStartup( sessionInfoPtr, FALSE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int serverStartup( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Complete the handshake using the common client/server code */
	return( commonStartup( sessionInfoPtr, TRUE ) );
	}

/****************************************************************************
*																			*
*						Control Information Management Functions			*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 OUT void *data, 
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	CRYPT_CERTIFICATE *certPtr = ( CRYPT_CERTIFICATE * ) data;
	CRYPT_CERTIFICATE iCryptCert = isServer( sessionInfoPtr ) ? \
		sessionInfoPtr->iKeyexAuthContext : sessionInfoPtr->iKeyexCryptContext;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( type == CRYPT_SESSINFO_RESPONSE || \
			  type == CRYPT_SESSINFO_SSL_OPTIONS );

	/* If the caller is after the current SSL option settings, return them */
	if( type == CRYPT_SESSINFO_SSL_OPTIONS )
		{
		const SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
		int *valuePtr = ( int * ) data;

		*valuePtr = sslInfo->minVersion & SSL_MINVER_MASK;
#ifdef CONFIG_SUITEB
		if( sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB_128 )
			*valuePtr |= CRYPT_SSLOPTION_SUITEB_128;
		if( sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB_256 )
			*valuePtr |= CRYPT_SSLOPTION_SUITEB_256;
#endif /* CONFIG_SUITEB */
		if( sessionInfoPtr->protocolFlags & SSL_PFLAG_MANUAL_CERTCHECK )
			*valuePtr |= CRYPT_SSLOPTION_MANUAL_CERTCHECK;
		if( sessionInfoPtr->protocolFlags & SSL_PFLAG_DISABLE_NAMEVERIFY )
			*valuePtr |= CRYPT_SSLOPTION_DISABLE_NAMEVERIFY;
		if( sessionInfoPtr->protocolFlags & SSL_PFLAG_DISABLE_CERTVERIFY )
			*valuePtr |= CRYPT_SSLOPTION_DISABLE_CERTVERIFY;

		return( CRYPT_OK );
		}

	/* If we didn't get a client/server certificate then there's nothing to 
	   return */
	if( iCryptCert == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTFOUND );

	/* Return the information to the caller */
	krnlSendNotifier( iCryptCert, IMESSAGE_INCREFCOUNT );
	*certPtr = iCryptCert;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int setAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 IN const void *data,
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	const int value = *( ( int * ) data );

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( type == CRYPT_SESSINFO_SSL_OPTIONS );

	/* Set SuiteB options if this is enabled */
#ifdef CONFIG_SUITEB
	if( value & ( CRYPT_SSLOPTION_SUITEB_128 | CRYPT_SSLOPTION_SUITEB_256 ) )
		{
		const int suiteBvalue = value & ( CRYPT_SSLOPTION_SUITEB_128 | \
										  CRYPT_SSLOPTION_SUITEB_256 );

		if( sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB )
			{
			/* If a Suite B configuration option is already set then we 
			   can't set another one on top of it */
			setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_SSL_OPTIONS, 
						  CRYPT_ERRTYPE_ATTR_PRESENT );
			return( CRYPT_ERROR_INITED );
			}
		if( suiteBvalue == ( CRYPT_SSLOPTION_SUITEB_128 | \
							 CRYPT_SSLOPTION_SUITEB_256 ) )
			{
			/* We can't set both the 128-bit and 256-bit security levels at 
			   the same time */
			return( CRYPT_ARGERROR_NUM1 );
			}
		if( suiteBvalue == CRYPT_SSLOPTION_SUITEB_128 )
			sessionInfoPtr->protocolFlags |= SSL_PFLAG_SUITEB_128;
		else
			sessionInfoPtr->protocolFlags |= SSL_PFLAG_SUITEB_256;
		}
#endif /* CONFIG_SUITEB */

	/* Set the minimum protocol version, a two-bit field that contains the 
	   minimum version that we're prepared to accept */
	if( value & SSL_MINVER_MASK )
		sslInfo->minVersion = value & SSL_MINVER_MASK;

	/* By default if a certificate is used we try and verify the server name 
	   against the name(s) in the certificate, and the certificate itself, 
	   but since certificate use is so erratic we allow the user to disable 
	   this if required */
	if( value & CRYPT_SSLOPTION_DISABLE_NAMEVERIFY )
		sessionInfoPtr->protocolFlags |= SSL_PFLAG_DISABLE_NAMEVERIFY;
	if( value & CRYPT_SSLOPTION_DISABLE_CERTVERIFY )
		sessionInfoPtr->protocolFlags |= SSL_PFLAG_DISABLE_CERTVERIFY;

	/* Enable manual checking of certificates if required */
	if( value & CRYPT_SSLOPTION_MANUAL_CERTCHECK )
		sessionInfoPtr->protocolFlags |= SSL_PFLAG_MANUAL_CERTCHECK;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								   IN const void *data,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	const CRYPT_CONTEXT cryptContext = *( ( CRYPT_CONTEXT * ) data );
	int pkcAlgo, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, sizeof( int ) ) );

	REQUIRES( type > CRYPT_ATTRIBUTE_NONE && type < CRYPT_ATTRIBUTE_LAST );

	if( type != CRYPT_SESSINFO_PRIVATEKEY || !isServer( sessionInfoPtr ) )
		return( CRYPT_OK );

	/* Check that the server key that we've been passed is usable.  For an 
	   RSA key we can have either encryption (for RSA keyex) or signing (for 
	   DH keyex) or both, for a DSA or ECDSA key we need signing (for DH/ECDH 
	   keyex) */
	status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE,
							  &pkcAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	switch( pkcAlgo )
		{
		case CRYPT_ALGO_RSA:
			status = krnlSendMessage( cryptContext, IMESSAGE_CHECK, NULL,
									  MESSAGE_CHECK_PKC_DECRYPT );
			if( cryptStatusError( status ) )
				status = krnlSendMessage( cryptContext, IMESSAGE_CHECK, NULL,
										  MESSAGE_CHECK_PKC_SIGN );
			if( cryptStatusError( status ) )
				{
				retExt( CRYPT_ARGERROR_NUM1,
						( CRYPT_ARGERROR_NUM1, SESSION_ERRINFO,
						  "Server key can't be used for encryption or "
						  "signing" ) );
				}
			return( CRYPT_OK );

		case CRYPT_ALGO_DSA:
		case CRYPT_ALGO_ECDSA:
			status = krnlSendMessage( cryptContext, IMESSAGE_CHECK, NULL,
									  MESSAGE_CHECK_PKC_SIGN );
			if( cryptStatusError( status ) )
				{
				retExt( CRYPT_ARGERROR_NUM1,
						( CRYPT_ARGERROR_NUM1, SESSION_ERRINFO,
						  "Server key can't be used for signing" ) );
				}
#ifdef CONFIG_SUITEB
			return( checkSuiteBKey( sessionInfoPtr, cryptContext, pkcAlgo ) );
#else
			return( CRYPT_OK );
#endif /* CONFIG_SUITEB */

		default:
			retExt( CRYPT_ARGERROR_NUM1,
					( CRYPT_ARGERROR_NUM1, SESSION_ERRINFO,
					  "Server key uses algorithm that can't be used with "
					  "SSL/TLS" ) );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*								Get/Put Data Functions						*
*																			*
****************************************************************************/

/* Read/write data over the SSL/TLS link */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readHeaderFunction( INOUT SESSION_INFO *sessionInfoPtr,
							   OUT_ENUM_OPT( READINFO ) \
									READSTATE_INFO *readInfo )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	STREAM stream;
	int packetLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( readInfo, sizeof( READSTATE_INFO ) ) );

	/* Clear return value */
	*readInfo = READINFO_NONE;

	/* Read the SSL/TLS packet header data */
	status = readFixedHeader( sessionInfoPtr, sslInfo->headerBuffer, 
							  sessionInfoPtr->receiveBufStartOfs );
	if( cryptStatusError( status ) )
		{
		/* OK_SPECIAL means that we got a soft timeout before the entire 
		   header was read, so we return zero bytes read to tell the 
		   calling code that there's nothing more to do */
		return( ( status == OK_SPECIAL ) ? 0 : status );
		}

	/* Since data errors are always fatal, we make all errors fatal until
	   we've finished handling the header */
	*readInfo = READINFO_FATAL;

	/* Check for an SSL/TLS alert message */
	if( sslInfo->headerBuffer[ 0 ] == SSL_MSG_ALERT )
		{
		return( processAlert( sessionInfoPtr, sslInfo->headerBuffer, 
							  sessionInfoPtr->receiveBufStartOfs ) );
		}

	/* Process the header data */
	sMemConnect( &stream, sslInfo->headerBuffer, 
				 sessionInfoPtr->receiveBufStartOfs );
	status = checkPacketHeaderSSL( sessionInfoPtr, &stream, &packetLength );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Determine how much data we'll be expecting */
	sessionInfoPtr->pendingPacketLength = \
		sessionInfoPtr->pendingPacketRemaining = packetLength;

	/* Indicate that we got the header */
	*readInfo = READINFO_NOOP;
	return( OK_SPECIAL );
	}

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processBodyFunction( INOUT SESSION_INFO *sessionInfoPtr,
								OUT_ENUM_OPT( READINFO ) \
									READSTATE_INFO *readInfo )
	{
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( readInfo, sizeof( READSTATE_INFO ) ) );

	/* All errors processing the payload are fatal */
	*readInfo = READINFO_FATAL;

	/* If we're potentially performing a rehandshake, process the packet
	   as a handshake message and treat it as a no-op.  What the server
	   does in response to this is implementation-specific, the spec says
	   that a client can ignore this (as we do) at which point the server
	   can close the connection or hang waiting for a rehandshake that'll
	   never come (as IIS does) */
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_CHECKREHANDSHAKE )
		{
		sessionInfoPtr->protocolFlags &= ~SSL_PFLAG_CHECKREHANDSHAKE;
		status = unwrapPacketSSL( sessionInfoPtr, 
								  sessionInfoPtr->receiveBuffer + \
									sessionInfoPtr->receiveBufPos, 
								  sessionInfoPtr->pendingPacketLength, 
								  &length, SSL_MSG_HANDSHAKE );
		if( cryptStatusError( status ) )
			return( status );

		/* Discard the read packet */
		sessionInfoPtr->receiveBufEnd = sessionInfoPtr->receiveBufPos;
		sessionInfoPtr->pendingPacketLength = 0;
		*readInfo = READINFO_NOOP;
		return( OK_SPECIAL );
		}

	/* Unwrap the payload */
	status = unwrapPacketSSL( sessionInfoPtr, 
							  sessionInfoPtr->receiveBuffer + \
								sessionInfoPtr->receiveBufPos, 
							  sessionInfoPtr->pendingPacketLength, 
							  &length, SSL_MSG_APPLICATION_DATA );
	if( cryptStatusError( status ) )
		return( status );

	*readInfo = READINFO_NONE;
	return( length );
	}

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
static int preparePacketFunction( INOUT SESSION_INFO *sessionInfoPtr )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( !( sessionInfoPtr->flags & SESSION_SENDCLOSED ) );
	REQUIRES( !( sessionInfoPtr->protocolFlags & SSL_PFLAG_ALERTSENT ) );

	/* Wrap up the payload ready for sending.  Since this is wrapping in-
	   place data we first open a write stream to add the header, then open
	   a read stream covering the full buffer in preparation for wrapping
	   the packet (the first operation looks a bit counter-intuitive because
	   we're opening a packet stream and then immediately closing it again,
	   but this is as intended since all that we're using it for is to write
	   the packet header at the start).  Note that we connect the later read 
	   stream to the full send buffer (bufSize) even though we only advance 
	   the current stream position to the end of the stream contents 
	   (bufPos), since the packet-wrapping process adds further data to the 
	   stream that exceeds the current stream position */
	status = openPacketStreamSSL( &stream, sessionInfoPtr, 0,
								  SSL_MSG_APPLICATION_DATA );
	if( cryptStatusError( status ) )
		return( status );
	sMemDisconnect( &stream );
	sMemConnect( &stream, sessionInfoPtr->sendBuffer,
				 sessionInfoPtr->sendBufSize );
	status = sSkip( &stream, sessionInfoPtr->sendBufPos, SSKIP_MAX );
	if( cryptStatusOK( status ) )
		status = wrapPacketSSL( sessionInfoPtr, &stream, 0 );
	if( cryptStatusOK( status ) )
		status = stell( &stream );
	INJECT_FAULT( SESSION_CORRUPT_DATA, SESSION_CORRUPT_DATA_SSL_1 );
	sMemDisconnect( &stream );

	return( status );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodSSL( INOUT SESSION_INFO *sessionInfoPtr )
	{
	static const PROTOCOL_INFO protocolInfo = {
		/* General session information */
		FALSE,						/* Request-response protocol */
		SESSION_NONE,				/* Flags */
		SSL_PORT,					/* SSL/TLS port */
		SESSION_NEEDS_PRIVKEYSIGN,	/* Client attributes */
			/* The client private key is optional, but if present it has to
			   be signature-capable */
		SESSION_NEEDS_PRIVATEKEY |	/* Server attributes */
			SESSION_NEEDS_PRIVKEYCERT | \
			SESSION_NEEDS_KEYORPASSWORD,
			/* The server key capabilities are complex enough that they
			   need to be checked specially via checkAttributeFunction(),
			   for an RSA key we can have either encryption (for RSA keyex)
			   or signing (for DH keyex) or both, for a DSA or ECDSA key
			   we need signing (for DH/ECDH keyex).

			   In theory we need neither a private key nor a password 
			   because the caller can provide the password during the
			   handshake in response to a CRYPT_ENVELOPE_RESOURCE
			   notification, however this facility is likely to be 
			   barely-used in comparison to users forgetting to add server
			   certificates and the like, so we require some sort of 
			   server-side key set in advance */
		SSL_MINOR_VERSION_TLS11,	/* TLS 1.1 */
#ifdef USE_SSL3
			SSL_MINOR_VERSION_SSL, SSL_MINOR_VERSION_TLS12,
#else
			SSL_MINOR_VERSION_TLS, SSL_MINOR_VERSION_TLS12,
#endif /* USE_SSL3 */
			/* We default to TLS 1.1 rather than TLS 1.2 because support for 
			   the latter will be minimal a long time, particularly among 
			   things like embedded devices.  However even TLS 1.1 support 
			   is still unreliable, see
			   https://www.trustworthyinternet.org/ssl-pulse, which puts
			   it just as low as TLS 1.2.  We still go with 1.1 however
			   because we need it in order to have support for TLS
			   extensions and, conveniently, explicit IVs */

		/* Protocol-specific information */
		EXTRA_PACKET_SIZE + \
			MAX_PACKET_SIZE,		/* Send/receive buffer size */
		SSL_HEADER_SIZE,			/* Payload data start */
			/* This may be adjusted during the handshake if we're talking
			   TLS 1.1+, which prepends extra data in the form of an IV to
			   the payload */
		MAX_PACKET_SIZE				/* (Default) maximum packet size */
		};

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that the huge list of cipher suites is set up correctly */
	assert( SSL_NULL_WITH_NULL == 0x00 );
	assert( TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA == 0x0B );
	assert( TLS_DH_anon_EXPORT_WITH_RC4_40_MD5 == 0x17 );
	assert( TLS_KRB5_WITH_DES_CBC_SHA == 0x1E );
	assert( TLS_PSK_WITH_NULL_SHA == 0x2C );
	assert( TLS_RSA_WITH_AES_128_CBC_SHA == 0x2F );
	assert( TLS_RSA_WITH_NULL_SHA256 == 0x3B );
	assert( TLS_DH_DSS_WITH_AES_128_CBC_SHA256 == 0x3E );
	assert( TLS_RSA_WITH_CAMELLIA_128_CBC_SHA == 0x41 );
	assert( TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 == 0x67 );
	assert( TLS_RSA_WITH_CAMELLIA_256_CBC_SHA == 0x84 );
	assert( TLS_PSK_WITH_RC4_128_SHA == 0x8A );
	assert( TLS_RSA_WITH_SEED_CBC_SHA == 0x96 );
	assert( TLS_RSA_WITH_AES_128_GCM_SHA256 == 0x9C );
	assert( TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256 == 0xBA );
	assert( TLS_ECDH_ECDSA_WITH_NULL_SHA == 0xC001 );
	assert( TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA == 0xC01A );
	assert( TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 == 0xC023 );
	assert( TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 == 0xC02B );
	assert( TLS_ECDHE_PSK_WITH_RC4_128_SHA == 0xC033 );

	/* Set the access method pointers */
	sessionInfoPtr->protocolInfo = &protocolInfo;
	sessionInfoPtr->shutdownFunction = shutdownFunction;
	sessionInfoPtr->transactFunction = isServer( sessionInfoPtr ) ? \
									   serverStartup : clientStartup;
	sessionInfoPtr->getAttributeFunction = getAttributeFunction;
	sessionInfoPtr->setAttributeFunction = setAttributeFunction;
	sessionInfoPtr->checkAttributeFunction = checkAttributeFunction;
	sessionInfoPtr->readHeaderFunction = readHeaderFunction;
	sessionInfoPtr->processBodyFunction = processBodyFunction;
	sessionInfoPtr->preparePacketFunction = preparePacketFunction;

	return( CRYPT_OK );
	}
#endif /* USE_SSL */
