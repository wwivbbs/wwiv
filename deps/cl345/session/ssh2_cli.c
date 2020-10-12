/****************************************************************************
*																			*
*								cryptlib SSHv2 Client						*
*						Copyright Peter Gutmann 1998-2016					*
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

/* Generate/check an SSH key fingerprint.  This is simply an MD5 hash of the 
   server's key/certificate data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processKeyFingerprint( INOUT SESSION_INFO *sessionInfoPtr,
								  IN_BUFFER( keyDataLength ) const void *keyData,
								  IN_LENGTH_SHORT const int keyDataLength )
	{
	HASH_FUNCTION_ATOMIC hashFunctionAtomic;
	const ATTRIBUTE_LIST *attributeListPtr = \
				findSessionInfo( sessionInfoPtr,
								 CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1 );
	BYTE fingerPrint[ CRYPT_MAX_HASHSIZE + 8 ];
	int hashSize;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtrDynamic( keyData, keyDataLength ) );

	REQUIRES( isShortIntegerRangeNZ( keyDataLength ) );

	getHashAtomicParameters( CRYPT_ALGO_MD5, 0, &hashFunctionAtomic, 
							 &hashSize );
	hashFunctionAtomic( fingerPrint, CRYPT_MAX_HASHSIZE, 
						keyData, keyDataLength );
	if( attributeListPtr == NULL )
		{
		/* Remember the value for the caller.  Recording it as a "SHA1" hash
		   is a bit odd since it's an MD5 hash, but the use of MD5 hashes 
		   has been deprecated everywhere else so there's no other way to
		   convey it */
		return( addSessionInfoS( sessionInfoPtr,
								 CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1,
								 fingerPrint, hashSize ) );
		}

	/* In the unlikely event that the user has passed us a SHA-1 
	   fingerprint, which isn't allowed by the spec but no doubt someone out 
	   there's using it based on the fact that the SSH architecture draft 
	   suggested a SHA-1 fingerprint while the SSH fingerprint draft 
	   required an MD5 one, calculate that instead */
	if( attributeListPtr->valueLength == 20 )
		{
		getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, 
								 &hashSize );
		hashFunctionAtomic( fingerPrint, CRYPT_MAX_HASHSIZE, 
							keyData, keyDataLength );
		}

	/* There's an existing fingerprint value, make sure that it matches what
	   we just calculated */
	if( attributeListPtr->valueLength != hashSize || \
		memcmp( attributeListPtr->value, fingerPrint, hashSize ) )
		{
		/* If there's enough fingerprint data present we can be a bit more
		   specific in our error message */
		if( attributeListPtr->valueLength >= 8 )
			{
#ifdef USE_ERRMSGS
			char fingerPrintText[ CRYPT_MAX_TEXTSIZE + 8 ];
			char reqFingerPrintText[ CRYPT_MAX_TEXTSIZE + 8 ];
#endif /* USE_ERRMSGS */

			formatFingerprint( fingerPrintText, CRYPT_MAX_TEXTSIZE,
							   fingerPrint, hashSize );
			formatFingerprint( reqFingerPrintText, CRYPT_MAX_TEXTSIZE,
							   attributeListPtr->value, 
							   attributeListPtr->valueLength );
			retExt( CRYPT_ERROR_WRONGKEY,
					( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
					  "Server key fingerprint '%s' doesn't match requested "
					  "fingerprint '%s'", fingerPrintText,
					  reqFingerPrintText ) );
			}
		retExt( CRYPT_ERROR_WRONGKEY,
				( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
				  "Server key fingerprint doesn't match requested "
				  "fingerprint" ) );
		}

	return( CRYPT_OK );
	}

/* Handle an ephemeral DH key exchange */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int processDHE( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT SSH_HANDSHAKE_INFO *handshakeInfo,
					   INOUT STREAM *stream, 
					   INOUT KEYAGREE_PARAMS *keyAgreeParams )
	{
	const int keyDataHdrSize = LENGTH_SIZE + sizeofString32( 6 );
	void *keyexInfoPtr DUMMY_INIT_PTR;
	int keyexInfoLength DUMMY_INIT, length, packetOffset, dummy, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( keyAgreeParams, sizeof( KEYAGREE_PARAMS ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/*	...
		byte	type = SSH_MSG_KEXDH_GEX_REQUEST_NEW
		uint32	min = MIN_PKCSIZE bits
		uint32	n = SSH_DEFAULT_KEYSIZE (as bits)
		uint32	max = CRYPT_MAX_PKCSIZE (as bits)

	   There's an older format for this that just allows the client to 
	   specify a single preferred key size:

		byte	type = SSH_MSG_KEXDH_GEX_REQUEST_OLD
		uint32	n = SSH_DEFAULT_KEYSIZE bits

	   which some older servers may still require (some will just drop the 
	   connection without any error response if they encounter the newer 
	   packet type), however some new servers will in turn reject the older
	   form if the preferred key size that's given doesn't match their 
	   configured key size.  Since more servers handle the new form and will
	   reject the old form if the key size doesn't match, we send the new 
	   form, however there is one widely-used buggy server that will generate
	   an invalid keyex signature if sent an SSH_MSG_KEX_DH_GEX_REQUEST 
	   rather than an SSH_MSG_KEX_DH_GEX_REQUEST_OLD, presumably due to 
	   hashing the wrong packet format, so we optionally send that as a
	   workaround */
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSH_PFLAG_OLDGEX ) )
		{
		status = continuePacketStreamSSH( stream, 
										  SSH_MSG_KEX_DH_GEX_REQUEST_OLD,
										  &packetOffset );
		if( cryptStatusOK( status ) )
			{
			streamBookmarkSet( stream, keyexInfoLength );
			status = writeUint32( stream, bytesToBits( SSH2_DEFAULT_KEYSIZE ) );
			}
		}
	else
		{
		status = continuePacketStreamSSH( stream, SSH_MSG_KEX_DH_GEX_REQUEST,
										  &packetOffset );
		if( cryptStatusOK( status ) )
			{
			streamBookmarkSet( stream, keyexInfoLength );
			writeUint32( stream, bytesToBits( MIN_PKCSIZE ) );
			writeUint32( stream, bytesToBits( SSH2_DEFAULT_KEYSIZE ) );
			status = writeUint32( stream, bytesToBits( CRYPT_MAX_PKCSIZE ) );
			}
		}
	if( cryptStatusOK( status ) )
		{
		status = streamBookmarkComplete( stream, &keyexInfoPtr, 
										 &keyexInfoLength, keyexInfoLength );
		}
	if( cryptStatusOK( status ) )
		status = wrapPlaintextPacketSSH2( sessionInfoPtr, stream, packetOffset );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, stream );
	sMemDisconnect( stream );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( keyexInfoPtr != NULL );

	/* Remember the encoded key size information for later when we generate 
	   the exchange hash */
	REQUIRES( rangeCheck( keyexInfoLength, 1, ENCODED_REQKEYSIZE ) );
	memcpy( handshakeInfo->encodedReqKeySizes, keyexInfoPtr,
			keyexInfoLength );
	handshakeInfo->encodedReqKeySizesLength = keyexInfoLength;

	/* Process the ephemeral DH key:

		byte	type = SSH_MSG_KEXDH_GEX_GROUP
		mpint	p
		mpint	g */
	status = length = \
		readHSPacketSSH2( sessionInfoPtr, SSH_MSG_KEX_DH_GEX_GROUP,
						  ID_SIZE + sizeofString32( MIN_PKCSIZE ) + \
							sizeofString32( 1 ) );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( stream, sessionInfoPtr->receiveBuffer, length );
	streamBookmarkSet( stream, keyexInfoLength );
	status = readInteger32Checked( stream, NULL, &dummy, MIN_PKCSIZE, 
								   CRYPT_MAX_PKCSIZE );
	if( cryptStatusOK( status ) )
		{
		status = readInteger32( stream, NULL, &dummy, 1, 
								CRYPT_MAX_PKCSIZE );
		}
	if( cryptStatusOK( status ) )
		{
		status = streamBookmarkComplete( stream, &keyexInfoPtr, 
										 &keyexInfoLength, 
										 keyexInfoLength );
		}
	sMemDisconnect( stream );
	if( cryptStatusError( status ) )
		{
		/* Some misconfigured servers may use very short keys, we perform
		   a special-case check for these and return a more specific message
		   than the generic bad-data */
		if( status == CRYPT_ERROR_NOSECURE )
			{
			retExt( CRYPT_ERROR_NOSECURE,
					( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
					  "Insecure DH key used in key exchange" ) );
			}

		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid DH ephemeral key data packet" ) );
		}
	ANALYSER_HINT( keyexInfoPtr != NULL );

	/* Since this phase of the key negotiation exchanges raw key components
	   rather than the standard SSH public-key format we have to rewrite the 
	   raw key components into a standard SSH key so that we can import it:

			From:					To:
								string		[ key/certificate ]
									string	"ssh-dh"
			mpint	p				mpint	p
			mpint	g				mpint	g */
	REQUIRES( boundsCheck( keyDataHdrSize, keyexInfoLength, 
						   sessionInfoPtr->receiveBufSize ) );
	memmove( ( BYTE * ) keyexInfoPtr + keyDataHdrSize, keyexInfoPtr, 
			 keyexInfoLength );
	sMemOpen( stream, keyexInfoPtr, keyDataHdrSize );
	writeUint32( stream, sizeofString32( 6 ) + keyexInfoLength );
	status = writeString32( stream, "ssh-dh", 6 );
	sMemDisconnect( stream );
	ENSURES( cryptStatusOK( status ) );

	/* Set up the DH context from the server's parameters and perform phase 
	   1 of the DH key agreement process */
	status = initDHcontextSSH( &handshakeInfo->iServerCryptContext,
							   &handshakeInfo->serverKeySize, keyexInfoPtr,
							   keyDataHdrSize + keyexInfoLength,
							   CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create DH ephemeral key data" ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Client-side Connect Functions						*
*																			*
****************************************************************************/

/* Perform the initial part of the handshake with the server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int beginClientHandshake( INOUT SESSION_INFO *sessionInfoPtr,
								 INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	KEYAGREE_PARAMS keyAgreeParams DUMMY_INIT_STRUCT;
	STREAM stream;
	void *clientHelloPtr DUMMY_INIT_PTR, *keyexPtr DUMMY_INIT_PTR;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int serverHelloLength, clientHelloLength, keyexLength DUMMY_INIT;
	int packetOffset = 0, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* The higher-level code has already read the server version information, 
	   send back our own version information */
	status = swrite( &sessionInfoPtr->stream, SSH_ID_STRING "\r\n",
					 SSH_ID_STRING_SIZE + 2 );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}

	/* SSH hashes the handshake ID strings for integrity-protection purposes, 
	   first our client string and then the server string that we read 
	   previously */
	status = hashHandshakeStrings( handshakeInfo, 
								   SSH_ID_STRING, SSH_ID_STRING_SIZE,
								   sessionInfoPtr->receiveBuffer,
								   sessionInfoPtr->receiveBufEnd );
	if( cryptStatusError( status ) )
		return( status );

	/* Now that we've processed the out-of-band data in the receive buffer, 
	   mark it as empty */
	sessionInfoPtr->receiveBufEnd = 0;

	/* In theory we could opportunistically set up a DH context while we 
	   wait for the server to digest our version information and send back 
	   its response, but in practice there are so many different ways in
	   which SSH can modify what the keyex does that we have to wait until
	   after the server hello to do anything */

	/* Process the server hello */
	status = processHelloSSH( sessionInfoPtr, handshakeInfo,
							  &serverHelloLength, FALSE );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "processHelloSSH" );

	/* Build the client hello and DH/ECDH phase 1 keyex packet:

		byte		type = SSH_MSG_KEXINIT
		byte[16]	cookie
		string		keyex algorithms = DH/DHE/ECDH
		string		pubkey algorithms
		string		client_crypto algorithms
		string		server_crypto algorithms
		string		client_mac algorithms
		string		server_mac algorithms
		string		client_compression algorithms = "none"
		string		server_compression algorithms = "none"
		string		client_language = ""
		string		server_language = ""
		boolean		first_keyex_packet_follows = FALSE
		uint32		reserved = 0
		...

	   The SSH spec leaves the order in which things happen ambiguous, in
	   order to save a whole round trip it has provisions for both sides
	   shouting at each other and then a complex interlock process where
	   bits of the initial exchange can be discarded and retried if necessary.
	   This is ugly and error-prone so what we do is wait for the server
	   hello (already done earlier), choose known-good algorithms, and then
	   send the client hello immediately followed by the client keyex.
	   Since we wait for the server to speak first we can choose parameters
	   that are accepted the first time.  In theory this means that we can
	   set keyex_follows to true (since a correct keyex packet always
	   follows the hello), however because of the nondeterministic initial
	   exchange the spec requires that a (guessed) keyex be discarded by the
	   server if the hello doesn't match (even if the keyex does):

		svr: hello
		client: matched hello, keyex
		svr: (discard keyex)

	   To avoid this problem we set keyex_follows to false to make it clear
	   to the server that the keyex is the real thing and shouldn't be
	   discarded */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, SSH_MSG_KEXINIT );
	if( cryptStatusError( status ) )
		return( status );
	streamBookmarkSetFullPacket( &stream, clientHelloLength );
	status = exportVarsizeAttributeToStream( &stream, SYSTEM_OBJECT_HANDLE,
											 CRYPT_IATTRIBUTE_RANDOM_NONCE,
											 SSH2_COOKIE_SIZE );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	status = writeAlgoStringEx( &stream, handshakeInfo->keyexAlgo,
								handshakeInfo->exchangeHashAlgo, 0,
								handshakeInfo->isFixedDH ? \
									SSH_ALGOSTRINGINFO_EXTINFO_ALTDHALGOS : \
									SSH_ALGOSTRINGINFO_EXTINFO );
	if( cryptStatusOK( status ) )
		{
		status = writeAlgoStringEx( &stream, handshakeInfo->pubkeyAlgo, 
									handshakeInfo->hashAlgo, 0,
									SSH_ALGOSTRINGINFO_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		status = writeAlgoStringEx( &stream, sessionInfoPtr->cryptAlgo,
									TEST_FLAG( sessionInfoPtr->protocolFlags, \
											   SSH_PFLAG_CTR ) ? \
										CRYPT_MODE_ECB : CRYPT_MODE_CBC, 
									handshakeInfo->cryptKeysize,
									SSH_ALGOSTRINGINFO_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		status = writeAlgoStringEx( &stream, sessionInfoPtr->cryptAlgo,
									TEST_FLAG( sessionInfoPtr->protocolFlags, \
											   SSH_PFLAG_CTR ) ? \
										CRYPT_MODE_ECB : CRYPT_MODE_CBC, 
									handshakeInfo->cryptKeysize,
									SSH_ALGOSTRINGINFO_NONE );
		}
	if( cryptStatusOK( status ) )
		status = writeAlgoString( &stream, sessionInfoPtr->integrityAlgo );
	if( cryptStatusOK( status ) )
		status = writeAlgoString( &stream, sessionInfoPtr->integrityAlgo );
	if( cryptStatusOK( status ) )
		status = writeAlgoClassList( &stream, SSH_ALGOCLASS_COPR );
	if( cryptStatusOK( status ) )
		status = writeAlgoClassList( &stream, SSH_ALGOCLASS_COPR );
	if( cryptStatusError( status ) )
		return( status );
	writeUint32( &stream, 0 );	/* No language tag */
	writeUint32( &stream, 0 );
	sputc( &stream, 0 );		/* Tell the server not to discard the packet */
	status = writeUint32( &stream, 0 );	/* Reserved */
	if( cryptStatusOK( status ) )
		{
		status = streamBookmarkComplete( &stream, &clientHelloPtr, 
										 &clientHelloLength, 
										 clientHelloLength );
		}
	if( cryptStatusOK( status ) )
		status = wrapPlaintextPacketSSH2( sessionInfoPtr, &stream, 0 );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	ANALYSER_HINT( clientHelloPtr != NULL );
	CFI_CHECK_UPDATE( "writeClientHello" );

	/* Hash the client and server hello messages.  We have to do this now
	   rather than deferring it until we're waiting on network traffic from
	   the server because they may get overwritten by the keyex negotiation
	   data if we're using a non-builtin DH key value.  In addition since the
	   entire encoded packet (including the type value) is hashed we have to
	   reconstruct this at the start of the packet */
	status = hashAsString( handshakeInfo->iExchangeHashContext, 
						   clientHelloPtr, clientHelloLength );
	if( cryptStatusOK( status ) )
		{
		REQUIRES( rangeCheck( serverHelloLength, 1,
							  sessionInfoPtr->receiveBufSize ) );
		memmove( sessionInfoPtr->receiveBuffer + 1, 
				 sessionInfoPtr->receiveBuffer, serverHelloLength );
		sessionInfoPtr->receiveBuffer[ 0 ] = SSH_MSG_KEXINIT;
		status = hashAsString( handshakeInfo->iExchangeHashContext,
							   sessionInfoPtr->receiveBuffer, 
							   serverHelloLength + 1 );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	/* SSH has a whole range of ways to specify the same keyex algorithm 
	   (see the long comment in ssh.h), the following multi-option code is
	   required to handle them.  The original way to do it was to use an
	   explicitly-specified DH key, but an alternative mechanism uses a
	   negotiated DH key value, in which case we have to request the keyex 
	   key from the server.  This additional negotiation requires 
	   disconnecting and re-connecting the data packet stream since it 
	   exchanges further data with the server, so if there's an error return 
	   we don't disconnect the stream before we exit */
	if( handshakeInfo->isFixedDH )
		{
		REQUIRES( !handshakeInfo->isECDH );

		/* It's a fixed-size DH key */
		status = initDHcontextSSH( &handshakeInfo->iServerCryptContext,
								   &handshakeInfo->serverKeySize, NULL, 0,
								   handshakeInfo->requestedServerKeySize );
		}
	else
		{
#ifdef USE_ECDH 
		/* A second possibility is when we're using ECDH rather than DH, for 
		   which we have to use ECDH contexts and values */
		if( handshakeInfo->isECDH )
			{
			/* Set up the ECDH context from the server's parameters and 
			   perform phase 1 of the ECDH key agreement process */
			status = initECDHcontextSSH( &handshakeInfo->iServerCryptContext,
										 &handshakeInfo->serverKeySize, 
										 handshakeInfo->keyexAlgo );
			}
		else
#endif /* USE_ECDH */
			{
			status = processDHE( sessionInfoPtr, handshakeInfo, &stream,
								 &keyAgreeParams );
			if( cryptStatusError( status ) )
				{
				/* processDHE() has already disconnected the stream */
				return( status );
				}
			}
		}
	if( cryptStatusOK( status ) )
		{
		memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
		status = krnlSendMessage( handshakeInfo->iServerCryptContext,
								  IMESSAGE_CTX_ENCRYPT, &keyAgreeParams,
								  sizeof( KEYAGREE_PARAMS ) );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create %s ephemeral key data",
				  handshakeInfo->isECDH ? "ECDH" : "DH" ) );
		}
	CFI_CHECK_UPDATE( "processDHE" );

	/* If we're fuzzing the input then we're reading static data and no 
	   further input is processed after this point, exit now to minimise
	   the overhead during fuzz testing */
	FUZZ_SET( handshakeInfo->clientKeyexValueLength,
			  handshakeInfo->serverKeySize );
	FUZZ_SKIP();

	/*	...
	   DH:
		byte	type = SSH_MSG_KEXDH_INIT / SSH_MSG_KEXDH_GEX_INIT
		mpint	y
	   ECDH:
		byte	type = SSH_MSG_KEX_ECDH_INIT
		string	q_c 

	   SSH_MSG_KEXDH_INIT (isFixedDH) and SSH_MSG_KEX_ECDH_INIT (isECDH) 
	   have the same value so they're handled with the same code */
	if( handshakeInfo->isFixedDH || handshakeInfo->isECDH )
		{
		/* It's a DH/ECDH exchange with static keys */
		status = continuePacketStreamSSH( &stream, SSH_MSG_KEXDH_INIT,
										  &packetOffset );
		}
	else
		{
		/* processDHE() has disconnected the stream as part of the ephemeral 
		   DH packet exchange so we need to create a new stream */
		status = openPacketStreamSSH( &stream, sessionInfoPtr, 
									  SSH_MSG_KEX_DH_GEX_INIT );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( cryptStatusOK( status ) )
		{
		streamBookmarkSet( &stream, keyexLength );
		if( handshakeInfo->isECDH )
			{
			status = writeString32( &stream, keyAgreeParams.publicValue,
									keyAgreeParams.publicValueLen );
			}
		else
			{
			INJECT_FAULT( SESSION_CORRUPT_KEYEX_CLIENT,	
						  SESSION_CORRUPT_KEYEX_CLIENT_SSH_1 );
			status = writeInteger32( &stream, keyAgreeParams.publicValue,
									 keyAgreeParams.publicValueLen );
			INJECT_FAULT( SESSION_CORRUPT_KEYEX_CLIENT,	
						  SESSION_CORRUPT_KEYEX_CLIENT_SSH_2 );
			}
		}
	if( cryptStatusOK( status ) )
		{
		status = streamBookmarkComplete( &stream, &keyexPtr, &keyexLength, 
										 keyexLength );
		}
	if( cryptStatusOK( status ) )
		status = wrapPlaintextPacketSSH2( sessionInfoPtr, &stream, packetOffset );
	if( cryptStatusOK( status ) )
		{
		/* Send the whole mess to the server.  Since SSH, unlike SSL,
		   requires that each packet in a multi-packet group be individually
		   gift-wrapped we have to first assemble the packets via 
		   wrapPacket() and then send them in a group via sendPacket() with
		   the send-only flag set */
		status = sendPacketSSH2( sessionInfoPtr, &stream );
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( keyexPtr != NULL );
	CFI_CHECK_UPDATE( "SSH_MSG_KEXDH_INIT" );

	/* Save the MPI-encoded client DH keyex value/octet string-encoded client 
	   ECDH keyex value for later, when we need to hash it */
	REQUIRES( rangeCheck( keyexLength, 1, MAX_ENCODED_KEYEXSIZE ) );
	memcpy( handshakeInfo->clientKeyexValue, keyexPtr, keyexLength );
	handshakeInfo->clientKeyexValueLength = keyexLength;

	/* Set up PKC information while we wait for the server to process our
	   response */
	setMessageCreateObjectInfo( &createInfo, handshakeInfo->pubkeyAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	sessionInfoPtr->iKeyexAuthContext = createInfo.cryptHandle;
	CFI_CHECK_UPDATE( "IMESSAGE_DEV_CREATEOBJECT" );

	REQUIRES( CFI_CHECK_SEQUENCE_5( "processHelloSSH", "writeClientHello", 
									"processDHE", "SSH_MSG_KEXDH_INIT", 
									"IMESSAGE_DEV_CREATEOBJECT" ) );
	return( CRYPT_OK );
	}

/* Exchange keys with the server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int exchangeClientKeys( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	CRYPT_ALGO_TYPE pubkeyAlgo;
	STREAM stream;
	MESSAGE_DATA msgData;
	void *keyPtr DUMMY_INIT_PTR, *keyBlobPtr DUMMY_INIT_PTR;
	void *sigPtr DUMMY_INIT_PTR;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int keyLength DUMMY_INIT, keyBlobLength, sigLength, length;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* Process the DH/ECDH phase 2 keyex packet:

	  DH + RSA/DSA
		byte		type = SSH_MSG_KEXDH_REPLY / SSH_MSG_KEXDH_GEX_REPLY 
		string		[ server key/certificate ]
			string	"ssh-rsa"	"ssh-dss"
			mpint	e			p			
			mpint	n			q
			mpint				g
			mpint				y
		mpint		y'
		string		[ signature of handshake data ]
			string	"ssh-rsa"	"ssh-dss"
			string	signature	signature

	   ECDH + ECDSA
		byte		SSH_MSG_KEX_ECDH_REPLY
		string		[ server key/certificate ]
			string	"ecdsa-sha2-*"
			string	"*"				-- The "*" portion from the above field
			string	Q
		string		q_s
		string		[ signature of handshake data ]
			string	"ecdsa-sha2-*"
			string	signature

	   SSH_MSG_KEXDH_REPLY (isFixedDH) and SSH_MSG_KEX_ECDH_REPLY (isECDH) 
	   have the same value so they're handled with the same code.

	   First, we read and hash the server key/certificate.  Since this is
	   already encoded as an SSH string we can hash it directly.

	   The two bookmarks are for the key as a whole and the "key blob" as
	   the raw key components that are hashed to create the key fingerprint.
	   
	   When we read the packet header we set the minimum key size to 512 
	   bits instead of MIN_PKCSIZE in order to provide better diagnostics if 
	   the server is using weak keys since otherwise the data will be 
	   rejected in the header read long before it gets to the keysize 
	   check */
	status = length = \
		readHSPacketSSH2( sessionInfoPtr, 
						  ( handshakeInfo->isFixedDH || \
						    handshakeInfo->isECDH ) ? \
							SSH_MSG_KEXDH_REPLY : SSH_MSG_KEX_DH_GEX_REPLY,
						  ID_SIZE + LENGTH_SIZE + sizeofString32( 6 ) + \
							sizeofString32( 1 ) + \
							sizeofString32( bitsToBytes( 512 ) - 4 ) + \
							sizeofString32( bitsToBytes( 512 ) - 4 ) + \
							LENGTH_SIZE + sizeofString32( 6 ) + 40 );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	streamBookmarkSet( &stream, keyLength );
	status = checkReadPublicKey( &stream, &pubkeyAlgo, &keyBlobLength, 
								 SESSION_ERRINFO );
	if( cryptStatusOK( status ) )
		{
		/* Key fingerprint data */
		status = streamBookmarkComplete( &stream, &keyBlobPtr, 
										 &keyBlobLength, keyBlobLength );
		}
	if( cryptStatusOK( status ) )
		{
		/* Overall key data */
		status = streamBookmarkComplete( &stream, &keyPtr, &keyLength, 
										 keyLength );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );

		/* Some misconfigured servers may use very short keys, we perform
		   a special-case check for these and return a more specific message
		   than the generic bad-data response */
		if( status == CRYPT_ERROR_NOSECURE )
			{
			retExt( CRYPT_ERROR_NOSECURE,
					( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
					  "Insecure server public key used in key exchange" ) );
			}

		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid %s phase 2 server public key data",
				  handshakeInfo->isECDH ? "ECDH" : "DH" ) );
		}
	if( pubkeyAlgo != handshakeInfo->pubkeyAlgo )
		{
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid %s phase 2 public key algorithm %s, expected %s",
				  handshakeInfo->isECDH ? "ECDH" : "DH", 
				  getAlgoName( pubkeyAlgo ), 
				  getAlgoName( handshakeInfo->pubkeyAlgo ) ) );
		}
#ifndef CONFIG_FUZZ
	setMessageData( &msgData, keyPtr, keyLength );
	status = krnlSendMessage( sessionInfoPtr->iKeyexAuthContext,
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_KEY_SSH );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		retExt( cryptArgError( status ) ? \
				CRYPT_ERROR_BADDATA : status,
				( cryptArgError( status ) ? \
				  CRYPT_ERROR_BADDATA : status, SESSION_ERRINFO, 
				  "Invalid %s phase 2 server public key value",
				  handshakeInfo->isECDH ? "ECDH" : "DH" ) );
		}
	ANALYSER_HINT( keyBlobPtr != NULL );
	status = krnlSendMessage( handshakeInfo->iExchangeHashContext,
							  IMESSAGE_CTX_HASH, keyPtr, keyLength );
#endif /* CONFIG_FUZZ */
	if( cryptStatusOK( status ) )
		{
		/* The fingerprint is computed from the "key blob" which is
		   different from the server key.  The server key is the full key
		   while the "key blob" is only the raw key components (e, n for
		   RSA, p, q, g, y for DSA).  Note that, as with the old PGP 2.x key
		   hash mechanism, this allows key spoofing (although it isn't quite
		   as bad as the PGP 2.x key fingerprint mechanism) since it doesn't
		   hash an indication of the key type or format */
		status = processKeyFingerprint( sessionInfoPtr,
										keyBlobPtr, keyBlobLength );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	CFI_CHECK_UPDATE( "SSH_MSG_KEXDH_REPLY" );

	/* Read the server DH/ECDH keyex value and complete the DH/ECDH key 
	   agreement */
	status = readRawObject32( &stream, handshakeInfo->serverKeyexValue,
							  MAX_ENCODED_KEYEXSIZE,
							  &handshakeInfo->serverKeyexValueLength );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid %s phase 2 keyex value",
				  handshakeInfo->isECDH ? "ECDH" : "DH" ) );
		}
	if( handshakeInfo->isECDH )
		{
		if( !isValidECDHsize( handshakeInfo->serverKeyexValueLength,
							  handshakeInfo->serverKeySize, 
							  LENGTH_SIZE ) )
			status = CRYPT_ERROR_BADDATA;
		}
	else
		{
		if( !isValidDHsize( handshakeInfo->serverKeyexValueLength,
							handshakeInfo->serverKeySize, 
							LENGTH_SIZE ) )
			status = CRYPT_ERROR_BADDATA;
		}
	if( cryptStatusError( status ) )
		{
		const int serverKeyexSize = handshakeInfo->isECDH ? \
			extractECDHsize( handshakeInfo->serverKeyexValueLength, LENGTH_SIZE ) : \
			extractDHsize( handshakeInfo->serverKeyexValueLength, LENGTH_SIZE );

		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid %s phase 2 keyex size, expected %d (%d), "
				  "got %d (%d)", handshakeInfo->isECDH ? "ECDH" : "DH",
				  handshakeInfo->serverKeySize, 
				  handshakeInfo->serverKeySize * 8, 
				  serverKeyexSize, serverKeyexSize * 8 ) );
		}
	status = completeKeyex( sessionInfoPtr, handshakeInfo, FALSE );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	CFI_CHECK_UPDATE( "completeKeyex" );

	/* Prepare to process the handshake packet signature ("What are you 
	   preparing?! You're always preparing! Just go!") */
	streamBookmarkSet( &stream, sigLength );
	status = length = readUint32( &stream );
	if( !cryptStatusError( status ) && !isShortIntegerRangeNZ( length ) )
		status = CRYPT_ERROR_BADDATA;
	if( !cryptStatusError( status ) )
		status = sSkip( &stream, length, MAX_INTLENGTH_SHORT );
	if( cryptStatusOK( status ) )
		{
		status = streamBookmarkComplete( &stream, &sigPtr, &sigLength, 
										 sigLength );
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid %s phase 2 packet signature data",
				  handshakeInfo->isECDH ? "ECDH" : "DH" ) );
		}
	ANALYSER_HINT( sigPtr != NULL );

	/* Since there are no crypto contexts set up during fuzzing, we can't go 
	   any further */
	FUZZ_EXIT();

	/* Some implementations incorrectly format the signature packet,
	   omitting the algorithm name and signature blob length for DSA sigs
	   (that is, they just encode two 20-byte values instead of a properly-
	   formatted signature):

				Right							Wrong
		string		[ signature data ]		string	[ nothing ]
			string	"ssh-dss"
			string	signature						signature

	   If we're talking to one of these versions we check to see whether the 
	   packet is correctly formatted (that is, that it has the algorithm-
	   type string present as required) and if it isn't present rewrite it 
	   into the correct form so that we can verify the signature.  This 
	   check requires that the signature format be one of the SSH standard 
	   types but since we can't (by definition) handle proprietary formats 
	   this isn't a problem.  What we're specifically checking for here is 
	   the *absence* of any known algorithm string, if any known string 
	   (even one for a format that we can't handle) is present then the
	   signature is in the correct format, it's only if we don't find a
	   match for any known string that we have to rewrite the signature */
#ifdef USE_DSA
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSH_PFLAG_SIGFORMAT ) && \
		( pubkeyAlgo == CRYPT_ALGO_DSA ) && \
		( memcmp( ( BYTE * ) sigPtr + LENGTH_SIZE + LENGTH_SIZE,
				  "ssh-dss", 7 ) && \
		  memcmp( ( BYTE * ) sigPtr + LENGTH_SIZE + LENGTH_SIZE,
				  "x509v3-sign-dss", 15 ) && \
		  memcmp( ( BYTE * ) sigPtr + LENGTH_SIZE + LENGTH_SIZE,
				  "spki-sign-dss", 13 ) && \
		  memcmp( ( BYTE * ) sigPtr + LENGTH_SIZE + LENGTH_SIZE,
				  "pgp-sign-dss", 12 ) ) )
		{
		int fixedSigLength DUMMY_INIT;

		/* Rewrite the signature to fix up the overall length at the start 
		   and insert the algorithm name and signature length.  We can 
		   safely reuse the receive buffer for this because the start 
		   contains the complete server key/certificate and DH keyex value 
		   which is far longer than the 12 bytes of header plus signature 
		   that we'll be writing there */
		sMemOpen( &stream, sessionInfoPtr->receiveBuffer,
				  LENGTH_SIZE + sizeofString32( 7 ) + sigLength );
		writeUint32( &stream, sizeofString32( 7 ) + sigLength );
		writeString32( &stream, "ssh-dss", 7 );
		status = swrite( &stream, sigPtr, sigLength );
		if( cryptStatusOK( status ) )
			fixedSigLength = stell( &stream );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );

		/* The rewritten signature is now at the start of the buffer, update
		   the signature pointer and size to accomodate the added header */
		sigPtr = sessionInfoPtr->receiveBuffer;
		sigLength = fixedSigLength;
		}
#endif /* USE_DSA */

	/* Finally, verify the server's signature on the exchange hash */
	status = iCryptCheckSignature( sigPtr, sigLength, CRYPT_IFORMAT_SSH,
								   sessionInfoPtr->iKeyexAuthContext,
								   handshakeInfo->iExchangeHashContext, 
								   CRYPT_UNUSED, NULL );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid handshake data signature" ) );
		}
	CFI_CHECK_UPDATE( "iCryptCheckSignature" );

	/* We don't need the exchange hash contexts any more, get rid of them */
	krnlSendNotifier( handshakeInfo->iExchangeHashContext,
					  IMESSAGE_DECREFCOUNT );
	handshakeInfo->iExchangeHashContext = CRYPT_ERROR;
	if( handshakeInfo->iExchangeHashAltContext != CRYPT_ERROR )
		{
		krnlSendNotifier( handshakeInfo->iExchangeHashAltContext,
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->iExchangeHashAltContext = CRYPT_ERROR;
		}
	CFI_CHECK_UPDATE( "cleanup" );

	REQUIRES( CFI_CHECK_SEQUENCE_4( "SSH_MSG_KEXDH_REPLY", "completeKeyex", 
									"iCryptCheckSignature", "cleanup" ) );
	return( CRYPT_OK );
	}

/* Complete the handshake with the server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int completeClientHandshake( INOUT SESSION_INFO *sessionInfoPtr,
									INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	STREAM stream;
	BYTE stringBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int stringLength, packetOffset, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* Set up the security information required for the session */
	status = initSecurityInfo( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "initSecurityInfo" );

	/* Build our change cipherspec message and request authentication with
	   the server:

		byte	type = SSH_MSG_NEWKEYS
		...

	   After this point the write channel is in the secure state, so we 
	   switch from wrapPlaintextPacketSSH2() to wrapPacketSSH2() */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, SSH_MSG_NEWKEYS );
	if( cryptStatusError( status ) )
		return( status );
	status = wrapPlaintextPacketSSH2( sessionInfoPtr, &stream, 0 );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	SET_FLAG( sessionInfoPtr->flags, SESSION_FLAG_ISSECURE_WRITE );
	CFI_CHECK_UPDATE( "SSH_MSG_NEWKEYS" );

#if 0
	/*   byte       SSH_MSG_EXT_INFO
	     uint32     nr-extensions
	       string   extension-name
	       string   extension-value (binary) */
	status = continuePacketStreamSSH( &stream, SSH_MSG_EXT_INFO, 
									  &packetOffset );
	if( cryptStatusOK( status ) )
		{
		writeUint32( &stream, 1 );
		writeString32( &stream, "global-requests-ok", 18 );
		status = writeUint32( &stream, 0 );
		}
	if( cryptStatusOK( status ) )
		{
		status = wrapPacketSSH2( sessionInfoPtr, &stream, packetOffset, 
								 FALSE );
		}
#endif /* Test handling of trigger for global request after authentication */ 

	/*	...
		byte	type = SSH_MSG_SERVICE_REQUEST
		string	service_name = "ssh-userauth".
		
	   For some reason SSH requires the use of two authentication messages, 
	   an "I'm about to authenticate" packet and an "I'm authenticating" 
	   packet, so we have to perform the authentication in two parts (dum 
	   loquimur, fugerit invida aetas) */
	status = continuePacketStreamSSH( &stream, SSH_MSG_SERVICE_REQUEST,
									  &packetOffset );
	if( cryptStatusOK( status ) )
		status = writeString32( &stream, "ssh-userauth", 12 );
	if( cryptStatusOK( status ) )
		{
		status = wrapPacketSSH2( sessionInfoPtr, &stream, packetOffset, 
								 FALSE );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	CFI_CHECK_UPDATE( "SSH_MSG_SERVICE_REQUEST" );

	/* Send the whole mess to the server.  This is yet another place where 
	   the SSH spec's vagueness over message ordering causes problems.  SSL 
	   at this point uses a Finished message in which the client and server 
	   do a mutual proof-of-possession of encryption and MAC keys via a 
	   pipeline-stalling message that prevents any further (sensitive) data 
	   from being exchanged until the PoP has concluded (the SSL Finished 
	   also authenticates the handshake messages) but SSH doesn't have any
	   such requirements.  The signed exchange hash from the server proves 
	   to the client that the server knows the master secret but not 
	   necessarily that the client and server share encryption and MAC keys.  
	   Without this mutual PoP the client could potentially end up sending 
	   passwords to the server using an incorrect (and potentially weak) key 
	   if it's messed up and derived the key incorrectly.  Although mutual 
	   PoP isn't a design goal of the SSH handshake we do it anyway (as far 
	   as we can without a proper Finished message), although this 
	   introduces a pipeline stall at this point.
	   
	   In addition because of the aforementioned ambiguity over message 
	   ordering we have to send our change cipherspec first because some 
	   implementations will stop and wait before they send their one, so if 
	   they don't see our one first they lock up.  To make this even more 
	   entertaining these are typically older ssh.com implementations with a 
	   whole smorgasbord of handshaking and crypto bugs, because of the lack 
	   of PoP and the fact that we have to send the first encrypted/MACd
	   message, encountering any of these bugs results in garbage from the
	   server followed by a closed connection with no ability to diagnose 
	   the problem.

	   Complicating things even further is the fact that the SSH extension
	   request is sent immediately after the change cipherspec, before
	   receiving confirmation that that crypto has been correctly set up.
	   This is because of some vague desire to provide for the insertion
	   of extensions before the userauth is performed in case there's a 
	   need to exchange extensions that may affect the authentication 
	   process.  What this applies to in practice is "server-sig-algs",
	   which defines the algorithms that the server will accept for 
	   "publickey"-format userauth.

	   The spec in fact says that after a key exchange with implicit server 
	   authentication the client has to wait for the server to send a 
	   service-accept packet before continuing, however it never explains 
	   what implicit (and, by extension, explicit) server authentication 
	   actually are.  This text is a leftover from an extremely early SSH 
	   draft in which the only keyex mechanism was "double-encrypting-sha", 
	   a mechanism that required a pipeline stall at this point because the 
	   client wasn't able to authenticate the server until it received the 
	   first encrypted/MAC'ed message from it.  To extricate ourselves from 
	   the confusion due to the missing definition we could define "implicit 
	   authentication" to be "Something completely different from what we're 
	   doing here" which means that we could send the two packets together 
	   without having to wait for the server, but it's probably better to 
	   use SSL-tyle Finished semantics at this point even if it adds an 
	   extra RTT delay */
	status = sendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Wait for the server's change cipherspec message.  From this point
	   on the read channel is also in the secure state */
	status = readHSPacketSSH2( sessionInfoPtr, SSH_MSG_NEWKEYS, ID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	SET_FLAG( sessionInfoPtr->flags, SESSION_FLAG_ISSECURE_READ );
	CFI_CHECK_UPDATE( "readHSPacketSSH2" );

	/* Wait for the server's service-accept message that should follow in
	   response to the service request sent after out change cipherspec.  
	   Some buggy versions send an empty service-accept packet so we only 
	   check the contents if it's a correctly-formatted packet */
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, 
				   SSH_PFLAG_EMPTYSVCACCEPT ) )
		{
		/* It's a buggy implementation, just check for the presence of a 
		   packet without looking at the contents */
		status = readHSPacketSSH2( sessionInfoPtr, SSH_MSG_SERVICE_ACCEPT, 
								   ID_SIZE );
		if( cryptStatusError( status ) )
			{
			/* This is the first message after the change cipherspec, a 
			   basic packet format error is more likely to be due to an 
			   incorrect key than an actual format error */
			retExtErr( CRYPT_ERROR_WRONGKEY,
					   ( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
						 SESSION_ERRINFO, 
						 "Invalid packet data for SSH_MSG_SERVICE_ACCEPT, "
						 "probably due to incorrect encryption keys being "
						 "negotiated during the handshake:" ) );
			}
		}
	else
		{
		int length;

		/* Check the service-accept packet:

			byte	type = SSH_MSG_SERVICE_ACCEPT
			string	service_name = "ssh-userauth".
			
		   This may also be an extension info packet if the server is using 
		   extensions: 
		   
			byte		type = SSH_MSG_EXT_INFO
			uint32		no_extensions
				string	name
				string	value (binary data) */
		status = length = \
			readHSPacketSSH2( sessionInfoPtr, SSH_MSG_SPECIAL_SERVICEACCEPT,
							  ID_SIZE + UINT32_SIZE );
		if( cryptStatusError( status ) )
			{
			/* This is the first message after the change cipherspec, a 
			   basic packet format error is more likely to be due to an 
			   incorrect key than an actual format error */
			retExtErr( CRYPT_ERROR_WRONGKEY,
					   ( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
						 SESSION_ERRINFO, 
						 "Invalid packet data for SSH_MSG_SERVICE_ACCEPT, "
						 "probably due to incorrect encryption keys being "
						 "negotiated during the handshake:" ) );
			}
		if( sessionInfoPtr->sessionSSH->packetType == SSH_MSG_EXT_INFO )
			{
			/* The server sent extension information, process it */
			sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
			status = readExtensionsSSH( sessionInfoPtr, &stream );
			sMemDisconnect( &stream );
			if( cryptStatusError( status ) )
				return( status );

			/* Retry the service-accept read */
			status = length = \
				readHSPacketSSH2( sessionInfoPtr, SSH_MSG_SERVICE_ACCEPT,
								  ID_SIZE + sizeofString32( 8 ) );
			if( cryptStatusError( status ) )
				return( status );
			}
		sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
		status = readString32( &stream, stringBuffer, CRYPT_MAX_TEXTSIZE,
							   &stringLength );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) || \
			stringLength != 12 || \
			memcmp( stringBuffer, "ssh-userauth", 12 ) )
			{
			/* More of a sanity check than anything else, the MAC should 
			   have caught any keying problems */
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid service accept packet" ) );
			}
		}
	CFI_CHECK_UPDATE( "serviceAccept" );

	/* Try and authenticate ourselves to the server */
	status = processClientAuth( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "processClientAuth" );

	/* We've finally made it through all of the formalities (post proelia
	   praemia), create (if necessary) and open a channel */
	if( getCurrentChannelNo( sessionInfoPtr, \
							 CHANNEL_READ ) == UNUSED_CHANNEL_NO )
		{
		/* The user hasn't specified any channel details, create a
		   channel of the default type */
		status = createChannel( sessionInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
#if 1
	status = sendChannelOpen( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "sendChannelOpen" );

	REQUIRES( CFI_CHECK_SEQUENCE_7( "initSecurityInfo", "SSH_MSG_NEWKEYS",
									"SSH_MSG_SERVICE_REQUEST", 
									"readHSPacketSSH2", "serviceAccept",
									"processClientAuth", 
									"sendChannelOpen" ) );
	return( CRYPT_OK );
#else	/* Test handling of OpenSSH "no-more-sessions@openssh.com" */
	status = sendChannelOpen( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* byte	type = SSH_MSG_GLOBAL_REQUEST
	   string	request_name = "no-more-sessions@openssh.com"
	   boolean	want_reply = FALSE */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, CRYPT_USE_DEFAULT,
								  SSH_MSG_GLOBAL_REQUEST );
	writeString32( &stream, "no-more-sessions@openssh.com", 29 );
	sputc( &stream, 0 );
	status = wrapPacketSSH2( sessionInfoPtr, &stream, 0, TRUE, TRUE );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream, TRUE );
	sMemDisconnect( &stream );
	return( status );
#endif /* 0 */
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initSSH2clientProcessing( INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	FNPTR_SET( handshakeInfo->beginHandshake, beginClientHandshake );
	FNPTR_SET( handshakeInfo->exchangeKeys, exchangeClientKeys );
	FNPTR_SET( handshakeInfo->completeHandshake, completeClientHandshake );
	}
#endif /* USE_SSH */
