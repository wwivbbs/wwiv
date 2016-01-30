/****************************************************************************
*																			*
*				cryptlib SSL v3/TLS Hello Handshake Management				*
*					Copyright Peter Gutmann 1998-2009						*
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

/* Process a session ID */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processSessionID( INOUT SESSION_INFO *sessionInfoPtr,
							 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							 INOUT STREAM *stream )
	{
	BYTE sessionID[ MAX_SESSIONID_SIZE + 8 ];
	int sessionIDlength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Get the session ID information */
	status = sessionIDlength = sgetc( stream );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid session ID information" ) );
		}
	if( sessionIDlength <= 0 )
		{
		/* No session ID, we're done */
		return( CRYPT_OK );
		}
	if( sessionIDlength < 1 || sessionIDlength > MAX_SESSIONID_SIZE )
		{
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid session ID length %d, should be 1...%d", 
				  sessionIDlength, MAX_SESSIONID_SIZE ) );
		}
	status = sread( stream, sessionID, sessionIDlength );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid session ID data" ) );
		}

	/* If we're the server and it's not (potentially) one of our sessionIDs, 
	   we're done */
	if( isServer( sessionInfoPtr ) && sessionIDlength != SESSIONID_SIZE )
		return( CRYPT_OK );

	/* It's a potentially resumed session, remember the details and let the 
	   caller know */
	memcpy( handshakeInfo->sessionID, sessionID, sessionIDlength );
	handshakeInfo->sessionIDlength = sessionIDlength;
	return( OK_SPECIAL );
	}

#ifdef CONFIG_SUITEB

/* For Suite B the first suite must be ECDHE/AES128-GCM/SHA256 or 
   ECDHE/AES256-GCM/SHA384 depending on the security level and the second
   suite, at the 128-bit security level, must be ECDHE/AES256-GCM/SHA384.  
   This is one of those pedantic checks that requires vastly more work to 
   support its nitpicking than the whole check is worth (since the same 
   thing is checked anyway when we check the suite strength), but it's 
   required by the spec */

CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
static int checkSuiteBSuiteSelection( IN_RANGE( SSL_FIRST_VALID_SUITE, \
												SSL_LAST_SUITE - 1 ) \
										const int cipherSuite,
									  const int flags,
									  const BOOLEAN isFirstSuite,
									  INOUT ERROR_INFO *errorInfo )
	{
	const char *precedenceString = isFirstSuite ? "First" : "Second";
	const char *suiteName = NULL;

	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( cipherSuite >= SSL_FIRST_VALID_SUITE && \
			  cipherSuite < SSL_LAST_SUITE );
	REQUIRES( ( flags & ~( SSL_PFLAG_SUITEB ) ) == 0 );

	if( isFirstSuite )
		{
		switch( flags )
			{
			case SSL_PFLAG_SUITEB_128:
				if( cipherSuite != TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 )
					suiteName = "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
				break;

			case SSL_PFLAG_SUITEB_256:
				if( cipherSuite != TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 )
					suiteName = "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";
				break;

			default:
				retIntError();
			}
		}
	else
		{
		switch( flags )
			{
			case SSL_PFLAG_SUITEB_128:
				if( cipherSuite != TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 )
					suiteName = "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";
				break;

			case SSL_PFLAG_SUITEB_256:
				/* For the 256-bit level there are no further requirements */
				break;

			default:
				retIntError();
			}
		}
	if( suiteName != NULL )
		{
		retExt( CRYPT_ERROR_NOTAVAIL,
				( CRYPT_ERROR_NOTAVAIL, errorInfo, 
				  "%s cipher suite for Suite B at the %d-bit security "
				  "level must be %s", precedenceString, 
				  ( flags == SSL_PFLAG_SUITEB_128 ) ? 128 : 256, 
				  suiteName ) );
		}

	/* At the 256-bit level there's an additional requirement that the 
	   client not offer any P256 cipher suites, or specifically that it not
	   offer the one P256 cipher suite allowed for Suite B (whether it can 
	   offer non-Suite B P256 cipher suites is left ambiguous) */
	if( flags == SSL_PFLAG_SUITEB_256 && \
		cipherSuite == TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 )
		{
		retExt( CRYPT_ERROR_NOTAVAIL,
				( CRYPT_ERROR_NOTAVAIL, errorInfo, 
				  "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 cipher suite "
				  "can't be offered at the 256-bit security level" ) );
		}

	return( CRYPT_OK );
	}
#endif /* CONFIG_SUITEB */

/****************************************************************************
*																			*
*								Legacy SSLv2 Functions						*
*																			*
****************************************************************************/

#ifdef ALLOW_SSLV2_HELLO	/* See warning in ssl.h */

/* Process an SSLv2 client hello:

	uint16		suiteLen
	uint16		sessIDlen
	uint16		nonceLen
	uint24[]	suites
	byte[]		sessID
	byte[]		nonce

   The v2 type and version have already been processed in readPacketSSL() 
   since this information, which is moved into the header in v3, is part of 
   the body in v2.  What's left for the v2 hello is the remainder of the 
   payload */

static int processCipherSuite( SESSION_INFO *sessionInfoPtr, 
							   SSL_HANDSHAKE_INFO *handshakeInfo, 
							   STREAM *stream, const int noSuites );

int processHelloSSLv2( SESSION_INFO *sessionInfoPtr, 
					   SSL_HANDSHAKE_INFO *handshakeInfo, 
					   STREAM *stream)
	{
	int suiteLength, sessionIDlength, nonceLength, status;

	/* Read the SSLv2 hello */
	suiteLength = readUint16( stream );
	sessionIDlength = readUint16( stream );
	nonceLength = readUint16( stream );
	if( suiteLength < 3 || ( suiteLength % 3 ) != 0 || \
		sessionIDlength < 0 || sessionIDlength > MAX_SESSIONID_SIZE || \
		nonceLength < 16 || nonceLength > SSL_NONCE_SIZE )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid legacy SSLv2 hello packet" ) );
		}
	status = processCipherSuite( sessionInfoPtr, handshakeInfo, stream, 
								 suiteLength / 3 );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionIDlength > 0 )
		sSkip( stream, sessionIDlength );
	return( sread( stream, handshakeInfo->clientNonce + \
						   SSL_NONCE_SIZE - nonceLength, nonceLength ) );
	}
#endif /* ALLOW_SSLV2_HELLO */

/****************************************************************************
*																			*
*							Negotiate a Cipher Suite						*
*																			*
****************************************************************************/

/* Set up the crypto information based on the cipher suite */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int setSuiteInfo( INOUT SESSION_INFO *sessionInfoPtr,
						 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						 const CIPHERSUITE_INFO *cipherSuiteInfoPtr )
	{
	CRYPT_QUERY_INFO queryInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isReadPtr( cipherSuiteInfoPtr, sizeof( CIPHERSUITE_INFO ) ) );

	handshakeInfo->cipherSuite = cipherSuiteInfoPtr->cipherSuite;
	handshakeInfo->keyexAlgo = cipherSuiteInfoPtr->keyexAlgo;
	handshakeInfo->authAlgo = cipherSuiteInfoPtr->authAlgo;
	handshakeInfo->cryptKeysize = cipherSuiteInfoPtr->cryptKeySize;
	sessionInfoPtr->cryptAlgo = cipherSuiteInfoPtr->cryptAlgo;
	sessionInfoPtr->integrityAlgo = cipherSuiteInfoPtr->macAlgo;
	handshakeInfo->integrityAlgoParam = cipherSuiteInfoPtr->macParam;
	if( sessionInfoPtr->version <= SSL_MINOR_VERSION_SSL )
		{
		/* SSL uses a proto-HMAC which requires that we synthesize it from
		   raw hash functionality */
		sessionInfoPtr->integrityAlgo = \
			( sessionInfoPtr->integrityAlgo == CRYPT_ALGO_HMAC_MD5 ) ? \
			CRYPT_ALGO_MD5 : CRYPT_ALGO_SHA1;
		}
	sessionInfoPtr->authBlocksize = cipherSuiteInfoPtr->macBlockSize;
	if( cipherSuiteInfoPtr->flags & CIPHERSUITE_FLAG_GCM )
		{
		/* GCM is a stream cipher with special-case requirements */
		sessionInfoPtr->cryptBlocksize = 1;
		sessionInfoPtr->protocolFlags |= SSL_PFLAG_GCM;
		}
	else
		{
		/* It's a standard cipher, get the block size */
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_QUERYCAPABILITY, &queryInfo,
								  sessionInfoPtr->cryptAlgo );
		if( cryptStatusError( status ) )
			return( status );
		sessionInfoPtr->cryptBlocksize = queryInfo.blockSize;
		}

	return( CRYPT_OK );
	}

/* Choose the best cipher suite from a list of suites */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processCipherSuite( INOUT SESSION_INFO *sessionInfoPtr, 
							   INOUT SSL_HANDSHAKE_INFO *handshakeInfo, 
							   INOUT STREAM *stream, 
							   IN_RANGE( 1, MAX_CIPHERSUITES ) \
									const int noSuites )
	{
	const CIPHERSUITE_INFO **cipherSuiteInfo;
	const BOOLEAN isServer = isServer( sessionInfoPtr ) ? TRUE : FALSE;
	BOOLEAN allowDH = algoAvailable( CRYPT_ALGO_DH );
	BOOLEAN allowECC = algoAvailable( CRYPT_ALGO_ECDH ) && \
					   algoAvailable( CRYPT_ALGO_ECDSA );
	BOOLEAN allowRSA = algoAvailable( CRYPT_ALGO_RSA );
	BOOLEAN allowTLS12 = \
		( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 ) ? TRUE : FALSE;
	int cipherSuiteInfoSize, suiteIndex = 999, altSuiteIndex = 999;
	int i, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( noSuites > 0 && noSuites <= MAX_CIPHERSUITES );

	/* Get the information for the supported cipher suites */
	status = getCipherSuiteInfo( &cipherSuiteInfo, &cipherSuiteInfoSize, 
								 isServer );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're the server then our choice of possible suites is constrained 
	   by the server key that we're using, figure out what we can use */
	if( isServer && sessionInfoPtr->privateKey != CRYPT_ERROR )
		{
		int pkcAlgo;

		/* To be usable for DH/ECC the server key has to be signature-
		   capable */
		status = krnlSendMessage( sessionInfoPtr->privateKey, 
								  IMESSAGE_CHECK, NULL, 
								  MESSAGE_CHECK_PKC_SIGN );
		if( cryptStatusError( status ) )
			allowDH = allowECC = FALSE;

		/* To be usable for ECC or RSA the server key has to itself be an 
		   ECC or RSA key */
		status = krnlSendMessage( sessionInfoPtr->privateKey, 
								  IMESSAGE_GETATTRIBUTE, &pkcAlgo,
								  CRYPT_CTXINFO_ALGO );
		if( cryptStatusError( status ) )
			allowECC = allowRSA = FALSE;
		else
			{
			if( !isEccAlgo( pkcAlgo ) )
				allowECC = FALSE;
			if( pkcAlgo != CRYPT_ALGO_RSA )
				allowRSA = FALSE;
			}
		}

	for( i = 0; i < noSuites; i++ )
		{
		const CIPHERSUITE_INFO *cipherSuiteInfoPtr = NULL;
		int newSuite, newSuiteIndex;

#ifdef ALLOW_SSLV2_HELLO	/* See warning in ssl.h */
		/* If we're reading an SSLv2 hello and it's an SSLv2 suite (the high
		   byte is nonzero), skip it and continue */
		if( handshakeInfo->isSSLv2 )
			{
			newSuite = sgetc( stream );
			if( cryptStatusError( newSuite ) )
				{
				retExt( newSuite,
						( newSuite, SESSION_ERRINFO, 
						  "Invalid cipher suite information" ) );
				}
			if( newSuite != 0 )
				{
				readUint16( stream );
				continue;
				}
			}
#endif /* ALLOW_SSLV2_HELLO */

		/* Get the cipher suite information */
		status = newSuite = readUint16( stream );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Invalid cipher suite information" ) );
			}

		/* If we're the client and we got back our canary method-of-last-
		   resort suite from the server without having seen another suite
		   that we can use first, the server is incapable of handling non-
		   crippled crypto.  Veni, vidi, volo in domum redire */
		if( !isServer && suiteIndex >= cipherSuiteInfoSize && \
			newSuite == SSL_RSA_EXPORT_WITH_RC4_40_MD5 )
			{
			retExt( CRYPT_ERROR_NOSECURE,
					( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
					  "Server rejected attempt to connect using "
					  "non-crippled encryption" ) );
			}

		/* If it's an obviously non-valid suite, continue.  Note that we 
		   have to perform this check after the canary check above since the
		   canary is an invalid suite */
		if( newSuite < SSL_FIRST_VALID_SUITE || newSuite >= SSL_LAST_SUITE )
			continue;

		/* When resuming a cached session the client is required to offer
		   as one of its suites the original suite that was used.  There's
		   no good reason for this requirement (it's probable that the spec
		   is intending that there be at least one cipher suite and that if
		   there's only one it should really be the one originally
		   negotiated) and it complicates implementation of shared-secret
		   key sessions so we don't perform this check */

		/* Try and find the information for the proposed cipher suite */
		for( newSuiteIndex = 0; 
			 newSuiteIndex < cipherSuiteInfoSize && \
				cipherSuiteInfo[ newSuiteIndex ]->cipherSuite != SSL_NULL_WITH_NULL;
			 newSuiteIndex++ )
			{
			if( cipherSuiteInfo[ newSuiteIndex ]->cipherSuite == newSuite )
				{
				cipherSuiteInfoPtr = cipherSuiteInfo[ newSuiteIndex ];
				break;
				}
			}
		ENSURES( newSuiteIndex < cipherSuiteInfoSize );
#ifdef CONFIG_SUITEB
		if( ( sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB ) && \
			( i == 0 || i == 1 ) )
			{
			status = checkSuiteBSuiteSelection( \
							( cipherSuiteInfoPtr == NULL ) ? \
								SSL_FIRST_VALID_SUITE /* Dummy value */ : \
								cipherSuiteInfoPtr->cipherSuite,
							sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB,
							( i == 0 ) ? TRUE : FALSE, SESSION_ERRINFO );
			if( cryptStatusError( status ) )
				return( status );
			}
#endif /* CONFIG_SUITEB */
		if( cipherSuiteInfoPtr == NULL )
			continue;

		/* Perform a short-circuit check, if the new suite is inherently 
		   less-preferred than what we've already got then there's no point 
		   to performing any of the remaining checks since it'll never be
		   selected */
		if( cipherSuiteInfoPtr->flags & CIPHERSUITE_FLAG_ECC )
			{
			if( newSuiteIndex > altSuiteIndex )
				continue;
			}
		else
			{
			if( newSuiteIndex > suiteIndex )
				continue;
			}

		/* Make sure that the required algorithms are available.  If we're
		   using TLS-PSK then there's no authentication algorithm because 
		   the exchange is authenticated using the PSK.  We don't have to 
		   check the keyex algorithm because that's handled via the 
		   algorithm-class check below except for RSA, which is implicitly
		   checked by the fact that it's also used for the (checked) 
		   authentication algorithm */
		if( cipherSuiteInfoPtr->authAlgo != CRYPT_ALGO_NONE && \
			!algoAvailable( cipherSuiteInfoPtr->authAlgo ) )
			continue;
		if( !algoAvailable( cipherSuiteInfoPtr->cryptAlgo ) )
			continue;
		if( !algoAvailable( cipherSuiteInfoPtr->macAlgo ) )
			continue;

		/* If it's a suite that's disabled because of external constraints 
		   then we can't use it */
		if( !allowRSA && \
			( cipherSuiteInfoPtr->keyexAlgo == CRYPT_ALGO_RSA || \
			  cipherSuiteInfoPtr->authAlgo == CRYPT_ALGO_RSA ) )
			continue;
		if( !allowDH && \
			( cipherSuiteInfoPtr->flags & CIPHERSUITE_FLAG_DH ) )
			continue;
		if( !allowECC && \
			( cipherSuiteInfoPtr->flags & CIPHERSUITE_FLAG_ECC ) )
			continue;
		if( !allowTLS12 && \
			( cipherSuiteInfoPtr->flags & CIPHERSUITE_FLAG_TLS12 ) )
			continue;

		/* If we're only able to do basic TLS-PSK because there's no private 
		   key present and the suite requires a private key then we can't 
		   use this suite */
		if( isServer && sessionInfoPtr->privateKey == CRYPT_ERROR && \
			cipherSuiteInfoPtr->keyexAlgo != CRYPT_ALGO_NONE )
			continue;

		/* If the new suite is more preferred (i.e. with a lower index) than 
		   the existing one, use that.  The presence of the ECC suites 
		   significantly complicates this process because the ECC curve 
		   information sent later on in the handshake can retroactively 
		   disable an already-negotiated ECC cipher suite, forcing a fallback 
		   to a non-ECC suite (this soft-fail fallback is also nasty for the
		   user since they can't guarantee that they're actually using ECC
		   if they ask for it).  To handle this we keep track of both the
		   most-preferred (non-ECC) suite and the most preferred ECC suite 
		   so that we can switch later if necessary */
		if( cipherSuiteInfoPtr->flags & CIPHERSUITE_FLAG_ECC )
			{
			if( newSuiteIndex < altSuiteIndex )
				altSuiteIndex = newSuiteIndex;
			}
		else
			{
			if( newSuiteIndex < suiteIndex )
				suiteIndex = newSuiteIndex;
			}
		}

	/* If the only matching suite that we found was an ECC one, set it to 
	   the primary suite (which can then be retroactively knocked out as per 
	   the comment earlier) */
	if( suiteIndex >= cipherSuiteInfoSize )
		{
		suiteIndex = altSuiteIndex;
		altSuiteIndex = 999;
		}

	/* If we couldn't find anything to use, exit.  The range comparison is 
	   actually for whether it's still set to the original value of 999 but 
	   some source code analysis tools think that it's an index check so we 
	   compare to the upper bound of the array size instead */
	if( suiteIndex >= cipherSuiteInfoSize && \
		altSuiteIndex >= cipherSuiteInfoSize )
		{
		retExt( CRYPT_ERROR_NOTAVAIL,
				( CRYPT_ERROR_NOTAVAIL, SESSION_ERRINFO, 
				  "No encryption mechanism compatible with the remote "
				  "system could be found" ) );
		}

	/* We got a cipher suite that we can handle, set up the crypto information */
	status = setSuiteInfo( sessionInfoPtr, handshakeInfo,
						   cipherSuiteInfo[ suiteIndex ] );
	if( cryptStatusError( status ) )
		return( status );

	/* If we found an ECC suite and it's not already been selected due to 
	   there being no other suites available, remember this in case we later 
	   find out that we can use it */
	if( altSuiteIndex < cipherSuiteInfoSize )
		{
		REQUIRES( allowECC );

		handshakeInfo->eccSuiteInfoPtr = cipherSuiteInfo[ altSuiteIndex ];
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Process Client/Server Hello 						*
*																			*
****************************************************************************/

/* Process the client/server hello:

		byte		ID = SSL_HAND_CLIENT_HELLO / SSL_HAND_SERVER_HELLO
		uint24		len
		byte[2]		version = { 0x03, 0x0n }
		byte[32]	nonce
		byte		sessIDlen		-- May receive nonzero len +
		byte[]		sessID			-- <len> bytes data

			Client						Server
		uint16		suiteLen		-
		uint16[]	suites			uint16		suite
		byte		coprLen = 1		-
		byte		copr = 0		byte		copr = 0 
	  [	uint16	extListLen			-- RFC 3546/RFC 4366
			byte	extType
			uint16	extLen
			byte[]	extData ] */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int processHelloSSL( INOUT SESSION_INFO *sessionInfoPtr, 
					 INOUT SSL_HANDSHAKE_INFO *handshakeInfo, 
					 INOUT STREAM *stream, const BOOLEAN isServer )
	{
	BOOLEAN potentiallyResumedSession = FALSE;
	int endPos, length, suiteLength = 1, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Check the header and version information */
	if( isServer )
		{
		status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
									  SSL_HAND_CLIENT_HELLO,
									  VERSIONINFO_SIZE + SSL_NONCE_SIZE + \
										1 + ( UINT16_SIZE * 2 ) + 1 + 1 );
		}
	else
		{
		status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
									  SSL_HAND_SERVER_HELLO,
									  VERSIONINFO_SIZE + SSL_NONCE_SIZE + \
										1 + UINT16_SIZE + 1 );
		}
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;
	status = processVersionInfo( sessionInfoPtr, stream,
								 isServer ? \
									&handshakeInfo->clientOfferedVersion : \
									NULL );
	if( cryptStatusError( status ) )
		return( status );

	/* Since we now know which protocol version we're using, we can turn off
	   any hashing that we don't require any more */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		if( handshakeInfo->md5context != CRYPT_ERROR )
			{
			krnlSendNotifier( handshakeInfo->md5context,
							  IMESSAGE_DECREFCOUNT );
			handshakeInfo->md5context = CRYPT_ERROR;
			krnlSendNotifier( handshakeInfo->sha1context,
							  IMESSAGE_DECREFCOUNT );
			handshakeInfo->sha1context = CRYPT_ERROR;
			}
		}
	else
		{
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
		}

	/* Process the nonce and session ID */
	status = sread( stream, isServer ? \
						handshakeInfo->clientNonce : \
						handshakeInfo->serverNonce, SSL_NONCE_SIZE );
	if( cryptStatusOK( status ) )
		status = processSessionID( sessionInfoPtr, handshakeInfo, stream );
	if( cryptStatusError( status ) )
		{
		if( status == OK_SPECIAL )
			potentiallyResumedSession = TRUE;
		else
			return( status );
		}

	/* Process the cipher suite information */
	if( isServer )
		{
		/* We're reading the client hello, the packet contains a
		   selection of suites preceded by a suite count */
		status = suiteLength = readUint16( stream );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid cipher suite information" ) );
			}
		if( suiteLength < UINT16_SIZE || \
			suiteLength > ( UINT16_SIZE * MAX_CIPHERSUITES ) || \
			( suiteLength % UINT16_SIZE ) != 0 )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid cipher suite length %d", 
					  suiteLength ) );
			}
		suiteLength /= UINT16_SIZE;
		}
	status = processCipherSuite( sessionInfoPtr, handshakeInfo, stream,
								 suiteLength );
	if( cryptStatusError( status ) )
		return( status );

	/* Process the compression suite information.  Since we don't implement
	   compression all that we need to do is check that the format is valid
	   and then skip the suite information */
	if( isServer )
		{
		/* We're reading the client hello, the packet contains a selection 
		   of suites preceded by a suite count */
		status = suiteLength = sgetc( stream );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid compression suite information" ) );
			}
		if( suiteLength < 1 || suiteLength > 20 )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid compression suite length %d, should be "
					  "1...20", suiteLength ) );
			}
		}
	status = sSkip( stream, suiteLength );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid compression algorithm information" ) );
		}

	/* If there's extra data present at the end of the packet, check for TLS
	   extension data */
	if( endPos - stell( stream ) > 0 )
		{
		const int extensionLength = endPos - stell( stream );

		if( extensionLength < UINT16_SIZE || \
			extensionLength >= MAX_INTLENGTH_SHORT )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "TLS hello contains %d bytes extraneous data", 
					  extensionLength ) );
			}
		status = readExtensions( sessionInfoPtr, handshakeInfo, stream,
								 extensionLength );
		if( cryptStatusError( status ) )
			return( status );
		handshakeInfo->hasExtensions = TRUE;
		}

	/* If we're the server, perform any special-case handling required by 
	   the fact that the selection of an ECC cipher suite can be 
	   retroactively modified by by TLS extensions that disable its use
	   again */
	if( isServer )
		{
		if( handshakeInfo->disableECC )
			{
			/* If the only available suite is an ECC one but it's been 
			   disabled through an incompatible choice of client-selected 
			   algorithm parameters then we can't continue */
			if( isEccAlgo( handshakeInfo->keyexAlgo ) )
				{
				retExt( CRYPT_ERROR_NOTAVAIL,
						( CRYPT_ERROR_NOTAVAIL, SESSION_ERRINFO, 
						  "Client specified use of an ECC cipher suite but "
						  "didn't provide any compatible ECC parameters" ) );
				}
			}
		else
			{
			/* If the client has chosen an ECC suite and it hasn't 
			   subsequently been disabled by an incompatible choice of 
			   client-selected parameters, switch to the ECC suite.  If the
			   only available option was an ECC suite then it'll already
			   have been enabled so we don't need to do it */
			if( handshakeInfo->eccSuiteInfoPtr != NULL )
				{
				status = setSuiteInfo( sessionInfoPtr, handshakeInfo, 
									   handshakeInfo->eccSuiteInfoPtr );
				if( cryptStatusError( status ) )
					return( status );
				}

			/* If we're using an ECC cipher suite (either due to it being 
			   the only suite available or because it was selected above) 
			   and there's no ECC curve selected by the client, default to 
			   P256 */
			if( isEccAlgo( handshakeInfo->keyexAlgo ) && \
				handshakeInfo->eccCurveID == CRYPT_ECCCURVE_NONE )
				handshakeInfo->eccCurveID = CRYPT_ECCCURVE_P256;
			}
		}

	/* If we're using Suite B and the MAC algorithm is an extended 
	   HMAC-SHA-2 algorithm (which means that the hash algorithm will also 
	   be extended SHA-2), replace the straight SHA2 context with the 
	   extended version */
#ifdef CONFIG_SUITEB
	if( sessionInfoPtr->integrityAlgo == CRYPT_ALGO_HMAC_SHA2 && \
		handshakeInfo->integrityAlgoParam == bitsToBytes( 384 ) )
		{
		krnlSendNotifier( handshakeInfo->sha2context,
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->sha2context = handshakeInfo->sha384context;
		handshakeInfo->sha384context = CRYPT_ERROR;
		}
#endif /* CONFIG_SUITEB */

	return( potentiallyResumedSession ? OK_SPECIAL : CRYPT_OK );
	}
#endif /* USE_SSL */
