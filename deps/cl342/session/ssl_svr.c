/****************************************************************************
*																			*
*					cryptlib SSL v3/TLS Server Management					*
*					   Copyright Peter Gutmann 1998-2011					*
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

/* Determine whether the server needs to request client certificates/client
   authentication.  This is normally determined by whether an access-control
   keyset is available, but for the Suite B tests in which any test 
   certificate is regarded as being acceptable it can be overridden with a
   self-test flag */

#ifdef CONFIG_SUITEB_TESTS 
#define clientCertAuthRequired( sessionInfoPtr ) \
		( sessionInfoPtr->cryptKeyset != CRYPT_ERROR || suiteBTestClientCert )
#else
#define clientCertAuthRequired( sessionInfoPtr ) \
		( sessionInfoPtr->cryptKeyset != CRYPT_ERROR )
#endif /* CONFIG_SUITEB_TESTS */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Read the client certificate chain and make sure that the certificate 
   being presented is valid for access */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readCheckClientCerts( INOUT SESSION_INFO *sessionInfoPtr, 
								 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
								 INOUT STREAM *stream )
	{
#ifndef CONFIG_SUITEB_TESTS 
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	MESSAGE_DATA msgData;
	BYTE certID[ KEYID_SIZE + 8 ];
#endif /* !CONFIG_SUITEB_TESTS */
#ifdef CONFIG_SUITEB
	int length;
#endif /* CONFIG_SUITEB */
	int status;

	/* Read the client certificate chain */
	status = readSSLCertChain( sessionInfoPtr, handshakeInfo, stream, 
							   &sessionInfoPtr->iKeyexAuthContext, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the client certificate is present in our certificate 
	   store.  Since we've already got a copy of the certificate, we only do 
	   a presence check rather than actually fetching the certificate */
#ifndef CONFIG_SUITEB_TESTS 
	setMessageData( &msgData, certID, KEYID_SIZE );
	status = krnlSendMessage( sessionInfoPtr->iKeyexAuthContext, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData, 
							  CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusOK( status ) )
		{
		setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_CERTID, certID, 
							   KEYID_SIZE, NULL, 0, KEYMGMT_FLAG_CHECK_ONLY );
		status = krnlSendMessage( sessionInfoPtr->cryptKeyset, 
								  IMESSAGE_KEY_GETKEY, &getkeyInfo, 
								  KEYMGMT_ITEM_PUBLICKEY );
		}
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
				  "Client certificate is not trusted for authentication "
				  "purposes" ) );
		}
#endif /* CONFIG_SUITEB_TESTS */

	/* Make sure that the key is of the appropriate size for the Suite B 
	   security level.  At the 128-bit level both P256 and P384 are allowed, 
	   at the 256-bit level only P384 is allowed */
#ifdef CONFIG_SUITEB
	status = krnlSendMessage( sessionInfoPtr->iKeyexAuthContext, 
							  IMESSAGE_GETATTRIBUTE, &length,
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusOK( status ) )
		{
		const int suiteBtype = \
						sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB;

		if( suiteBtype == SSL_PFLAG_SUITEB_256 )
			{
			if( length != bitsToBytes( 384 ) )
				{
				retExt( CRYPT_ERROR_INVALID,
						( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
						  "Client Suite B certificate uses %d-bit key at "
						  "256-bit security level, should use 384-bit key", 
						  bytesToBits( length ) ) );
				}
			}
		else
			{
			if( length != bitsToBytes( 256 ) && \
				length != bitsToBytes( 384 ) )
				{
				retExt( CRYPT_ERROR_INVALID,
						( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
						  "Client Suite B certificate uses %d-bit key at "
						  "128-bit security level, should use 256- or "
						  "384-bit key", bytesToBits( length ) ) );
				}
			}
		}
#endif /* CONFIG_SUITEB */

	return( CRYPT_OK );
	}


/* Write the certificate request:

	  [	byte		ID = SSL_HAND_SERVER_CERTREQUEST ]
	  [	uint24		len				-- Written by caller ]
		byte		certTypeLen
		byte[]		certType = { RSA, DSA, ECDSA }
	  [	uint16	sigHashListLen		-- TLS 1.2 ]
	  [		byte	hashAlgoID		-- TLS 1.2 ]
	  [		byte	sigAlgoID		-- TLS 1.2 ]
		uint16		caNameListLen = 4
			uint16	caNameLen = 2
			byte[]	caName = { 0x30, 0x00 }

   This message is a real mess, it originally had a rather muddled 
   certificate-type indicator (which included things like "Ephemeral DH 
   signed with RSA") and an equally ambiguous CA list that many 
   implementations either left empty or filled with the name of every CA 
   that they'd ever heard of.  TLS 1.2 added a means of indicating which 
   signature and hash algorithms were acceptable, which is kind of essential 
   because the explosion of hash algorithms in 1.2 means that a server would 
   have to run parallel hashes of every handshake message for every possible 
   hash algorithm until the client sends their certificate-verify message 
   (!!).  In other words although it was planned as a means of indicating 
   the server's capabilities, it actually acts as a mechanism for keeping 
   the client-auth process manageable */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeCertRequest( INOUT SESSION_INFO *sessionInfoPtr, 
							 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							 INOUT STREAM *stream )
	{
	const BOOLEAN rsaAvailable = algoAvailable( CRYPT_ALGO_RSA );
	const BOOLEAN dsaAvailable = algoAvailable( CRYPT_ALGO_DSA );
	const BOOLEAN ecdsaAvailable = algoAvailable( CRYPT_ALGO_ECDSA );

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	REQUIRES( rsaAvailable || ecdsaAvailable );

	/* Write the certificate type */
	sputc( stream, ( rsaAvailable ? 1 : 0 ) + \
				   ( dsaAvailable ? 1 : 0 ) + \
				   ( ecdsaAvailable ? 1 : 0 ) );
	if( rsaAvailable )
		sputc( stream, TLS_CERTTYPE_RSA );
	if( dsaAvailable )
		sputc( stream, TLS_CERTTYPE_DSA );
	if( ecdsaAvailable )
		sputc( stream, TLS_CERTTYPE_ECDSA );

	/* Write the list of accepted signature and hash algorithms if required.  
	   In theory we could write the full list of algorithms, but thanks to 
	   SSL/TLS' braindamaged way of handling certificate-based 
	   authentication (see the comment above) this would make the 
	   certificate-authentication process unmanageable.  To get around this 
	   we only allow one single algorithm, the SHA-2 default for TLS 1.2+.  
	   In addition we can't allow DSA because it's only defined for use with 
	   SHA-1 (unless we go for "DSA-2" / FIPS 186-3 key sizes like 2048 
	   bits, which nothing seems to support).

	   For Suite B things get a bit more complicated because this allows 
	   both SHA-256 and SHA-384, dropping us straight back into the mess 
	   that we've just escaped from.  To get around this we specify the
	   use of the hash algorithm that we've negotiated for the handshake,
	   on the assumption that a client using SHA384 for the handshake isn't 
	   going to then try and use SHA256 for the authentication */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
#ifdef CONFIG_SUITEB
		writeUint16( stream, 2 );
		if( handshakeInfo->keyexSigHashAlgoParam == bitsToBytes( 384 ) )
			sputc( stream, TLS_HASHALGO_SHA384 );
		else
			sputc( stream, TLS_HASHALGO_SHA2 );
		sputc( stream, TLS_SIGALGO_ECDSA );
#else
		writeUint16( stream, ( rsaAvailable ? 2 : 0 ) + \
							 ( ecdsaAvailable ? 2 : 0 ) );
		if( rsaAvailable )
			{
			sputc( stream, TLS_HASHALGO_SHA2 );
			sputc( stream, TLS_SIGALGO_RSA );
			}
		if( ecdsaAvailable )
			{
			sputc( stream, TLS_HASHALGO_SHA2 );
			sputc( stream, TLS_SIGALGO_ECDSA );
			}
#endif /* CONFIG_SUITEB */
		}

	/* Write the CA name list */ 
	writeUint16( stream, UINT16_SIZE + 2 );
	writeUint16( stream, 2 );
	return( swrite( stream, "\x30\x00", 2 ) );
	}

/****************************************************************************
*																			*
*							Server-side Connect Functions					*
*																			*
****************************************************************************/

/* Perform the initial part of the handshake with the client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int beginServerHandshake( INOUT SESSION_INFO *sessionInfoPtr, 
						  INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	STREAM *stream = &handshakeInfo->stream;
	SCOREBOARD_LOOKUP_RESULT lookupResult = DUMMY_INIT_STRUCT;
	MESSAGE_DATA msgData;
	int length, resumedSessionID = CRYPT_ERROR;
	int packetOffset, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	/* Read and process the client hello */
	status = readHSPacketSSL( sessionInfoPtr, handshakeInfo, &length,
							  SSL_MSG_FIRST_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( stream, sessionInfoPtr->receiveBuffer, length );
#ifdef ALLOW_SSLV2_HELLO		/* See warning in ssl.h */
	if( handshakeInfo->isSSLv2 )
		{
		int processHelloSSLv2( SESSION_INFO *sessionInfoPtr, 
							   SSL_HANDSHAKE_INFO *handshakeInfo, 
							   STREAM *stream );

		status = processHelloSSLv2( sessionInfoPtr, handshakeInfo, stream );
		}
	else
#endif /* ALLOW_SSLV2_HELLO */
	status = processHelloSSL( sessionInfoPtr, handshakeInfo, stream, TRUE );
	sMemDisconnect( stream );
	if( cryptStatusError( status ) )
		{
		if( status != OK_SPECIAL )
			return( status );

		/* The client has sent us a sessionID in an attempt to resume a 
		   previous session, see if it's in the session cache */
		resumedSessionID = \
			lookupScoreboardEntry( sessionInfoPtr->sessionSSL->scoreboardInfoPtr,
					SCOREBOARD_KEY_SESSIONID_SVR, handshakeInfo->sessionID, 
					handshakeInfo->sessionIDlength,
					&lookupResult );
#ifdef CONFIG_SUITEB_TESTS 
		resumedSessionID = CRYPT_ERROR;	/* Disable for Suite B tests */
#endif /* CONFIG_SUITEB_TESTS */
		}

	/* Handle session resumption.  If it's a new session or the session data 
	   has expired from the cache, generate a new session ID */
	if( cryptStatusError( resumedSessionID ) )
		{
		setMessageData( &msgData, handshakeInfo->sessionID, SESSIONID_SIZE );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusError( status ) )
			return( status );
		handshakeInfo->sessionIDlength = SESSIONID_SIZE;
		}
	else
		{
		/* We're resuming a previous session, remember the premaster secret */
		status = attributeCopyParams( handshakeInfo->premasterSecret, 
									  SSL_SECRET_SIZE,
									  &handshakeInfo->premasterSecretSize,
									  lookupResult.data, 
									  lookupResult.dataSize );
		ENSURES( cryptStatusOK( status ) );
		}

	/* Get the nonce that's used to randomise all crypto operations and set 
	   up the server DH/ECDH context if necessary */
	setMessageData( &msgData, handshakeInfo->serverNonce, SSL_NONCE_SIZE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	if( cryptStatusOK( status ) && isKeyxAlgo( handshakeInfo->keyexAlgo ) )
		{
		status = initDHcontextSSL( &handshakeInfo->dhContext, NULL, 0,
							( handshakeInfo->authAlgo != CRYPT_ALGO_NONE ) ? \
							sessionInfoPtr->privateKey : CRYPT_UNUSED,
							isEccAlgo( handshakeInfo->keyexAlgo ) ? \
								handshakeInfo->eccCurveID : CRYPT_ECCCURVE_NONE );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Build the server hello, certificate, optional certificate request, 
	   and done packets:

		byte		ID = SSL_HAND_SERVER_HELLO
		uint24		len
		byte[2]		version = { 0x03, 0x0n }
		byte[32]	nonce
		byte		sessIDlen
		byte[]		sessID
		uint16		suite
		byte		copr = 0
	  [	uint16	extListLen		-- RFC 3546/RFC 4366
			byte	extType
			uint16	extLen
			byte[]	extData ] 
		...

	   We have to be careful how we handle extensions because the RFC makes 
	   the rather optimistic assumption that implementations can handle the 
	   presence of unexpected data at the end of the hello packet, to avoid 
	   problems with this we avoid sending extensions unless they're in 
	   response to extensions already sent by the client */
	status = openPacketStreamSSL( stream, sessionInfoPtr, CRYPT_USE_DEFAULT, 
								  SSL_MSG_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	status = continueHSPacketStream( stream, SSL_HAND_SERVER_HELLO, 
									 &packetOffset );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	sputc( stream, SSL_MAJOR_VERSION );
	sputc( stream, sessionInfoPtr->version );
	swrite( stream, handshakeInfo->serverNonce, SSL_NONCE_SIZE );
	sputc( stream, handshakeInfo->sessionIDlength );
	swrite( stream, handshakeInfo->sessionID, 
			handshakeInfo->sessionIDlength );
	writeUint16( stream, handshakeInfo->cipherSuite ); 
	status = sputc( stream, 0 );	/* No compression */
	if( handshakeInfo->hasExtensions )
		status = writeServerExtensions( stream, handshakeInfo );
	if( cryptStatusOK( status ) )
		status = completeHSPacketStream( stream, packetOffset );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}

	/* If it's a resumed session then the server hello is followed 
	   immediately by the change cipherspec, which is sent by the shared 
	   handshake completion code */
	if( !cryptStatusError( resumedSessionID ) )
		{
		status = completePacketStreamSSL( stream, 0 );
		if( cryptStatusOK( status ) )
			status = hashHSPacketWrite( handshakeInfo, stream, 0 );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}

		/* Tell the caller that it's a resumed session, leaving the stream
		   open in order to write the change cipherspec message that follows 
		   the server hello in a resumed session */
		DEBUG_PRINT(( "Resuming session with client based on "
					  "sessionID = \n" ));
		DEBUG_DUMP_DATA( handshakeInfo->sessionID, 
						 handshakeInfo->sessionIDlength );
		return( OK_SPECIAL );
		}

	/*	...	(optional server supplemental data)
		byte		ID = SSL_HAND_SUPPLEMENTAL_DATA
		uint24		len
		uint16		type
		uint16		len
		byte[]		value
		... */

	/*	...
		(optional server certificate chain)
		... */
	if( handshakeInfo->authAlgo != CRYPT_ALGO_NONE )
		{
		status = writeSSLCertChain( sessionInfoPtr, stream );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		}

	/*	...			(optional server keyex)
		byte		ID = SSL_HAND_SERVER_KEYEXCHANGE
		uint24		len
	   DH:
		uint16		dh_pLen
		byte[]		dh_p
		uint16		dh_gLen
		byte[]		dh_g
		uint16		dh_YsLen
		byte[]		dh_Ys
	  [	byte		hashAlgoID		-- TLS 1.2 ]
	  [	byte		sigAlgoID		-- TLS 1.2 ]
		uint16		signatureLen
		byte[]		signature
	   ECDH:
		byte		curveType
		uint16		namedCurve
		uint8		ecPointLen		-- NB uint8 not uint16
		byte[]		ecPoint
	  [	byte		hashAlgoID		-- TLS 1.2 ]
	  [	byte		sigAlgoID		-- TLS 1.2 ]
		uint16		signatureLen
		byte[]		signature */
	if( isKeyxAlgo( handshakeInfo->keyexAlgo ) )
		{
		KEYAGREE_PARAMS keyAgreeParams;
		void *keyData = DUMMY_INIT_PTR;
		int keyDataOffset, keyDataLength = DUMMY_INIT;

		/* Perform phase 1 of the DH/ECDH key agreement process */
		memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
		status = krnlSendMessage( handshakeInfo->dhContext,
								  IMESSAGE_CTX_ENCRYPT, &keyAgreeParams,
								  sizeof( KEYAGREE_PARAMS ) );
		if( cryptStatusError( status ) )
			{
			zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
			sMemDisconnect( stream );
			return( status );
			}

		/* Write the DH/ECDH key parameters and public value and sign them */
		status = continueHSPacketStream( stream, SSL_HAND_SERVER_KEYEXCHANGE, 
										 &packetOffset );
		if( cryptStatusError( status ) )
			{
			zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
			sMemDisconnect( stream );
			return( status );
			}
		keyDataOffset = stell( stream );
		status = exportAttributeToStream( stream, handshakeInfo->dhContext,
										  CRYPT_IATTRIBUTE_KEY_SSL );
		if( cryptStatusOK( status ) )
			{
			if( isEccAlgo( handshakeInfo->keyexAlgo ) )
				{
				sputc( stream, keyAgreeParams.publicValueLen );
				swrite( stream, keyAgreeParams.publicValue,
						keyAgreeParams.publicValueLen );
				}
			else
				{
				status = writeInteger16U( stream, keyAgreeParams.publicValue, 
										  keyAgreeParams.publicValueLen );
				}
			}
		if( cryptStatusOK( status ) )
			{
			keyDataLength = stell( stream ) - keyDataOffset;
			status = sMemGetDataBlockAbs( stream, keyDataOffset, &keyData, 
										  keyDataLength );
			}
		if( cryptStatusOK( status ) )
			{
			ANALYSER_HINT( keyData != NULL );

			status = createKeyexSignature( sessionInfoPtr, handshakeInfo,
										   stream, keyData, keyDataLength );
			}
		zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		if( cryptStatusOK( status ) )
			status = completeHSPacketStream( stream, packetOffset );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		}

	/*	...			(optional request for client certificate authentication)
		byte		ID = SSL_HAND_SERVER_CERTREQUEST
		uint24		len
		byte		certTypeLen
		byte[]		certType = { RSA, DSA, ECDSA }
	  [	uint16	sigHashListLen		-- TLS 1.2 ]
	  [		byte	hashAlgoID		-- TLS 1.2 ]
	  [		byte	sigAlgoID		-- TLS 1.2 ]
		uint16		caNameListLen = 4
			uint16	caNameLen = 2
			byte[]	caName = { 0x30, 0x00 }
		... */
	if( clientCertAuthRequired( sessionInfoPtr ) )
		{
		status = continueHSPacketStream( stream, SSL_HAND_SERVER_CERTREQUEST, 
										 &packetOffset );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		status = writeCertRequest( sessionInfoPtr, handshakeInfo, stream );
		if( cryptStatusOK( status ) )
			status = completeHSPacketStream( stream, packetOffset );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		}

	/*	...
		byte		ID = SSL_HAND_SERVER_HELLODONE
		uint24		len = 0 */
	status = continueHSPacketStream( stream, SSL_HAND_SERVER_HELLODONE, 
									 &packetOffset );
	if( cryptStatusOK( status ) )
		status = completeHSPacketStream( stream, packetOffset );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}

	/* Send the combined server packets to the client.  We perform the 
	   assorted hashing of the packets in between the network ops where 
	   it's effectively free */
	status = sendPacketSSL( sessionInfoPtr, stream, FALSE );
	if( cryptStatusOK( status ) )
		status = hashHSPacketWrite( handshakeInfo, stream, 0 );
	sMemDisconnect( stream );
	return( status );
	}

/* Exchange keys with the client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int exchangeServerKeys( INOUT SESSION_INFO *sessionInfoPtr, 
						INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	STREAM *stream = &handshakeInfo->stream;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	/* Read the response from the client and, if we're expecting a client 
	   certificate, make sure that it's present */
	status = readHSPacketSSL( sessionInfoPtr, handshakeInfo, &length,
							  SSL_MSG_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( stream, sessionInfoPtr->receiveBuffer, length );
	if( clientCertAuthRequired( sessionInfoPtr ) )
		{
		/* Read the client certificate chain and make sure that the 
		   certificate being presented is valid for access */
		status = readCheckClientCerts( sessionInfoPtr, handshakeInfo, 
									   stream );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}

		/* Read the next packet(s) if necessary */
		status = refreshHSStream( sessionInfoPtr, handshakeInfo );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Process the client key exchange packet:

		byte		ID = SSL_HAND_CLIENT_KEYEXCHANGE
		uint24		len
	   DH:
		uint16		yLen
		byte[]		y
	   ECDH:
		uint16		ecPointLen
		byte[]		ecPoint
	   PSK:
		uint16		userIDLen
		byte[]		userID 
	   RSA:
	  [ uint16		encKeyLen		-- TLS 1.x ]
		byte[]		rsaPKCS1( byte[2] { 0x03, 0x0n } || byte[46] random ) */
	status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
								  SSL_HAND_CLIENT_KEYEXCHANGE, 
								  UINT16_SIZE + 1 );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	if( isKeyxAlgo( handshakeInfo->keyexAlgo ) )
		{
		KEYAGREE_PARAMS keyAgreeParams;
		const BOOLEAN isECC = isEccAlgo( handshakeInfo->keyexAlgo );

		memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
		if( isECC )
			{
			status = readEcdhValue( stream, keyAgreeParams.publicValue,
									CRYPT_MAX_PKCSIZE, 
									&keyAgreeParams.publicValueLen );
			}
		else
			{
			status = readInteger16UChecked( stream, 
											keyAgreeParams.publicValue,
											&keyAgreeParams.publicValueLen,
											MIN_PKCSIZE, CRYPT_MAX_PKCSIZE );
			}
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );

			/* Some misconfigured clients may use very short keys, we 
			   perform a special-case check for these and return a more 
			   specific message than the generic bad-data error */
			if( status == CRYPT_ERROR_NOSECURE )
				{
				retExt( CRYPT_ERROR_NOSECURE,
						( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
						  "Insecure key used in key exchange" ) );
				}

			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid DH/ECDH phase 2 key agreement data" ) );
			}

		/* Perform phase 2 of the DH/ECDH key agreement */
		status = krnlSendMessage( handshakeInfo->dhContext,
								  IMESSAGE_CTX_DECRYPT, &keyAgreeParams, 
								  sizeof( KEYAGREE_PARAMS ) );
		if( cryptStatusError( status ) )
			{
			zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
			sMemDisconnect( stream );
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Invalid DH/ECDH phase 2 key agreement value" ) );
			}
		if( isECC )
			{
			const int xCoordLen = ( keyAgreeParams.wrappedKeyLen - 1 ) / 2;

			/* The output of the ECDH operation is an ECC point, but for
			   some unknown reason TLS only uses the x coordinate and not 
			   the full point.  To work around this we have to rewrite the
			   point as a standalone x coordinate, which is relatively
			   easy because we're using an "uncompressed" point format: 

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
		ENSURES( rangeCheckZ( 0, keyAgreeParams.wrappedKeyLen,
							  CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE ) );
		memcpy( handshakeInfo->premasterSecret, keyAgreeParams.wrappedKey,
				keyAgreeParams.wrappedKeyLen );
		handshakeInfo->premasterSecretSize = keyAgreeParams.wrappedKeyLen;
		zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		}
	else
		{
		if( handshakeInfo->authAlgo == CRYPT_ALGO_NONE )
			{
			const ATTRIBUTE_LIST *attributeListPtr;
			BYTE userID[ CRYPT_MAX_TEXTSIZE + 8 ];

			/* Read the client user ID and make sure that it's a valid 
			   user.  Handling non-valid users is somewhat problematic,
			   we can either bail out immediately or invent a fake 
			   password for the (non-)user and continue with that.  The
			   problem with this is that it doesn't really help hide 
			   whether the user is valid or not due to the fact that we're 
			   still vulnerable to a timing attack because it takes 
			   considerably longer to generate the random password than it 
			   does to read a fixed password string from memory, so an 
			   attacker can tell from the timing whether the username is 
			   valid or not.  In addition usability research on real-world 
			   users indicates that this actually reduces security while 
			   having little to no tangible benefit.  Because of this we 
			   don't try and fake out the valid/invalid user name indication 
			   but just exit immediately if an invalid name is found */
			length = readUint16( stream );
			if( length < 1 || length > CRYPT_MAX_TEXTSIZE || \
				cryptStatusError( sread( stream, userID, length ) ) )
				{
				sMemDisconnect( stream );
				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
						  "Invalid client user ID" ) );
				}
			attributeListPtr = \
				findSessionInfoEx( sessionInfoPtr->attributeList,
								   CRYPT_SESSINFO_USERNAME, userID, length );
			if( attributeListPtr == NULL )
				{
				sMemDisconnect( stream );
				retExt( CRYPT_ERROR_WRONGKEY,
						( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
						  "Unknown user name '%s'", 
						  sanitiseString( userID, CRYPT_MAX_TEXTSIZE, 
										  length ) ) );
				}

			/* Select the attribute with the user ID and move on to the
			   associated password */
			sessionInfoPtr->attributeListCurrent = \
								( ATTRIBUTE_LIST * ) attributeListPtr;
			attributeListPtr = attributeListPtr->next;
			ENSURES( attributeListPtr != NULL && \
					 attributeListPtr->attributeID == CRYPT_SESSINFO_PASSWORD );

			/* Create the shared premaster secret from the user password */
			status = createSharedPremasterSecret( \
							handshakeInfo->premasterSecret,
							CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE,
							&handshakeInfo->premasterSecretSize, 
							attributeListPtr->value,
							attributeListPtr->valueLength,
							( attributeListPtr->flags & ATTR_FLAG_ENCODEDVALUE ) ? \
								TRUE : FALSE );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( stream );
				retExt( status, 
						( status, SESSION_ERRINFO, 
						  "Couldn't create master secret from shared "
						  "secret/password value" ) );
				}
			}
		else
			{
			BYTE wrappedKey[ CRYPT_MAX_PKCSIZE + 8 ];

			if( sessionInfoPtr->version <= SSL_MINOR_VERSION_SSL )
				{
				/* The original Netscape SSL implementation didn't provide a 
				   length for the encrypted key and everyone copied that so 
				   it became the de facto standard way to do it (sic faciunt 
				   omnes.  The spec itself is ambiguous on the topic).  This 
				   was fixed in TLS (although the spec is still ambigous) so 
				   the encoding differs slightly between SSL and TLS.  To 
				   work around this we have to duplicate a certain amount of
				   the integer-read code here */
				if( isShortPKCKey( length ) )
					status = CRYPT_ERROR_NOSECURE;
				else
					{
					if( length < MIN_PKCSIZE || length > CRYPT_MAX_PKCSIZE || \
						cryptStatusError( sread( stream, wrappedKey, length ) ) )
						status = CRYPT_ERROR_BADDATA;
					}
				}
			else
				{
				status = readInteger16UChecked( stream, wrappedKey, &length, 
												MIN_PKCSIZE, 
												CRYPT_MAX_PKCSIZE );
				}
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( stream );

				/* Some misconfigured clients may use very short keys, we 
				   perform a special-case check for these and return a more 
				   specific message than the generic bad-data */
				if( status == CRYPT_ERROR_NOSECURE )
					{
					retExt( CRYPT_ERROR_NOSECURE,
							( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
							  "Insecure key used in key exchange" ) );
					}

				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
						  "Invalid RSA encrypted key data" ) );
				}

			/* Decrypt the pre-master secret */
			status = unwrapPremasterSecret( sessionInfoPtr, handshakeInfo,
											wrappedKey, length );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( stream );
				return( status );
				}
			}
		}

	/* If we're expecting a client certificate, process the client 
	   certificate verify */
	if( clientCertAuthRequired( sessionInfoPtr ) )
		{
		const BOOLEAN isECC = isEccAlgo( handshakeInfo->keyexAlgo );

		/* Since the client certificate-verify message requires the hash of
		   all handshake packets up to this point, we have to interrupt the
		   processing to calculate the hash before we continue */
		status = createCertVerifyHash( sessionInfoPtr, handshakeInfo );
		if( cryptStatusError( status ) )
			return( status );

		/* Read the next packet(s) if necessary */
		status = refreshHSStream( sessionInfoPtr, handshakeInfo );
		if( cryptStatusError( status ) )
			return( status );

		/* Process the client certificate verify packet:

			byte		ID = SSL_HAND_CLIENT_CERTVERIFY
			uint24		len
			byte[]		signature */
		status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
									  SSL_HAND_CLIENT_CERTVERIFY, 
									  isECC ? MIN_PKCSIZE_ECCPOINT : \
											  MIN_PKCSIZE );
		if( cryptStatusOK( status ) )
			status = checkCertVerify( sessionInfoPtr, handshakeInfo, stream, 
									  length );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		}
	sMemDisconnect( stream );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initSSLserverProcessing( SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	handshakeInfo->beginHandshake = beginServerHandshake;
	handshakeInfo->exchangeKeys = exchangeServerKeys;
	}
#endif /* USE_SSL */
