/****************************************************************************
*																			*
*								cryptlib SSHv2 Server						*
*						Copyright Peter Gutmann 1998-2015					*
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

/* Set up the public-key algorithm that we'll be advertising to the client 
   based on the server key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initPubkeyAlgo( INOUT SESSION_INFO *sessionInfoPtr,
						   INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	static const ALGO_STRING_INFO algoStringPubkeyRSATbl[] = {
		{ "rsa-sha2-256", 12, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2 },
		{ "ssh-rsa", 7, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1 },
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }, 
			{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }
		};
	static const ALGO_STRING_INFO algoStringPubkeyDSATbl[] = {
		{ "ssh-dss", 7, CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1 },
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }, 
			{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }
		};
#ifdef USE_ECDSA
	static const ALGO_STRING_INFO algoStringPubkeyECDSATbl[] = {
		{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2 },
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }, 
			{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }
		};
	static const ALGO_STRING_INFO algoStringPubkeyECDSA384Tbl[] = {
		{ "ecdsa-sha2-nistp384", 19, CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2 },
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }, 
			{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }
		};
	static const ALGO_STRING_INFO algoStringPubkeyECDSA521Tbl[] = {
		{ "ecdsa-sha2-nistp521", 19, CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2 },
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }, 
			{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }
		};
	int keySize;
#endif /* USE_ECDSA */
	int pubKeyAlgo, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	/* Find out which algorithm the server key is using */
	status = krnlSendMessage( sessionInfoPtr->privateKey,
							  IMESSAGE_GETATTRIBUTE, &pubKeyAlgo,
							  CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	handshakeInfo->pubkeyAlgo = pubKeyAlgo;	/* int vs.enum */

	/* If it's a standard public-key algorithm, return the algorithm 
	   information directly */
	if( handshakeInfo->pubkeyAlgo == CRYPT_ALGO_RSA )
		{
		handshakeInfo->algoStringPubkeyTbl = algoStringPubkeyRSATbl;
		handshakeInfo->algoStringPubkeyTblNoEntries = \
			FAILSAFE_ARRAYSIZE( algoStringPubkeyRSATbl, ALGO_STRING_INFO );
		return( CRYPT_OK );
		}
	if( handshakeInfo->pubkeyAlgo == CRYPT_ALGO_DSA )
		{
		handshakeInfo->algoStringPubkeyTbl = algoStringPubkeyDSATbl;
		handshakeInfo->algoStringPubkeyTblNoEntries = \
			FAILSAFE_ARRAYSIZE( algoStringPubkeyDSATbl, ALGO_STRING_INFO );
		return( CRYPT_OK );
		}
#ifdef USE_ECDSA
	ENSURES( handshakeInfo->pubkeyAlgo == CRYPT_ALGO_ECDSA );

	/* ECDSA gets more complicated because there are multiple fixed key 
	   sizes possible so we have to vary the algrithm table based on our key 
	   size */
	status = krnlSendMessage( sessionInfoPtr->privateKey, 
							  IMESSAGE_GETATTRIBUTE, &keySize, 
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );
	switch( keySize )
		{
		case bitsToBytes( 256 ):
			handshakeInfo->algoStringPubkeyTbl = algoStringPubkeyECDSATbl;
			handshakeInfo->algoStringPubkeyTblNoEntries = \
				FAILSAFE_ARRAYSIZE( algoStringPubkeyECDSATbl, ALGO_STRING_INFO );
			break;

		case bitsToBytes( 384 ):
			handshakeInfo->algoStringPubkeyTbl = algoStringPubkeyECDSA384Tbl;
			handshakeInfo->algoStringPubkeyTblNoEntries = \
				FAILSAFE_ARRAYSIZE( algoStringPubkeyECDSA384Tbl, ALGO_STRING_INFO );
			break;

		case bitsToBytes( 521 ):
			handshakeInfo->algoStringPubkeyTbl = algoStringPubkeyECDSA521Tbl;
			handshakeInfo->algoStringPubkeyTblNoEntries = \
				FAILSAFE_ARRAYSIZE( algoStringPubkeyECDSA521Tbl, ALGO_STRING_INFO );
			break;

		default:
			retIntError();
		}

	return( CRYPT_OK );
#else
	retIntError();
#endif /* USE_ECDSA */
	}

/* Handle an ephemeral DH key exchange */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processDHE( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE keyData[ ( CRYPT_MAX_PKCSIZE * 2 ) + 16 + 8 ];
	void *keyexInfoPtr DUMMY_INIT_PTR;
	int keyexInfoLength, keyDataStart, keyDataLength, length;
	int keySize, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* Get the keyex key request from the client:

		byte	type = SSH_MSG_KEXDH_GEX_REQUEST_OLD
		uint32	n (bits)

	   or:

		byte	type = SSH_MSG_KEXDH_GEX_REQUEST
		uint32	min (bits)
		uint32	n (bits)
		uint32	max (bits)

	   Since we need to deal with either, readHSPacketSSH2() takes a call to 
	   read SSH_MSG_KEX_DH_GEX_REQUEST_OLD as allowing both.

	   Portions of the the request information are hashed later as part of 
	   the exchange hash so we have to save a copy for then.  We save the
	   original encoded form because some clients send non-integral lengths
	   that don't survive the conversion from bits to bytes */
	status = length = \
		readHSPacketSSH2( sessionInfoPtr, SSH_MSG_KEX_DH_GEX_REQUEST_OLD,
						  ID_SIZE + UINT32_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	streamBookmarkSet( &stream, keyexInfoLength );
	if( sessionInfoPtr->sessionSSH->packetType == SSH_MSG_KEX_DH_GEX_REQUEST )
		{
		int minKeySize;

		/* It's a { min_length, length, max_length } sequence, save a copy
		   and get the length value */
		minKeySize = readUint32( &stream );
		keySize = readUint32( &stream );
		status = readUint32( &stream );
		DEBUG_PRINT(( "Client specified key size range %d...%d (%d "
					  "preferred) bits.\n", minKeySize, status, keySize ));

		/* Some implementations (e.g. OpenSSH >= 6.7) request ridiculous key 
		   sizes, to deal with this we change the effective key size to 
		   CRYPT_MAX_PKCSIZE if the client has asked for a key size > 
		   CRYPT_MAX_PKCSIZE but also specified that they'll accept a 
		   min_length <= CRYPT_MAX_PKCSIZE */
		if( !cryptStatusError( status ) && \
			keySize > bytesToBits( CRYPT_MAX_PKCSIZE ) && \
			minKeySize <= bytesToBits( CRYPT_MAX_PKCSIZE ) )
			{
			DEBUG_PRINT(( "Client requested excessive key size %d bits but "
						  "specified %d...%d bits, using %d bits.\n", 
						  keySize, minKeySize, status, 
						  bytesToBits( CRYPT_MAX_PKCSIZE ) ));
			keySize = bytesToBits( CRYPT_MAX_PKCSIZE );
			}
		}
	else
		{
		/* It's a straight length, save a copy and get the length value */
		status = keySize = readUint32( &stream );
		DEBUG_PRINT(( "Client specified key size %d bits.\n", keySize ));
		}
	if( !cryptStatusError( status ) )
		{
		status = streamBookmarkComplete( &stream, &keyexInfoPtr, 
										 &keyexInfoLength, keyexInfoLength );
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Invalid ephemeral DH key data request packet" ) );
		}
	ANALYSER_HINT( keyexInfoPtr != NULL );
	if( keySize < bytesToBits( MIN_PKCSIZE ) || \
		keySize > bytesToBits( CRYPT_MAX_PKCSIZE ) )
		{
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Client requested invalid ephemeral DH key size %d bits, "
				  "should be %d...%d", keySize, 
				  bytesToBits( MIN_PKCSIZE ), 
				  bytesToBits( CRYPT_MAX_PKCSIZE ) ) );
		}
	REQUIRES( rangeCheck( keyexInfoLength, 1, MAX_ENCODED_KEYEXSIZE ) );
	memcpy( handshakeInfo->encodedReqKeySizes, keyexInfoPtr,
			keyexInfoLength );
	handshakeInfo->encodedReqKeySizesLength = keyexInfoLength;
	handshakeInfo->requestedServerKeySize = bitsToBytes( keySize );

	/* If we're fuzzing the input then we're done */
	FUZZ_SET( handshakeInfo->serverKeySize, 
			  handshakeInfo->requestedServerKeySize );
	FUZZ_SKIP();

	/* Load a DH key of the appropriate size.  Things get quite confusing 
	   here because the spec is a schizophrenic mix of two different 
	   documents, one that specifies the behaviour for the original message 
	   format which uses a single length value and a second one that 
	   specifies the behaviour for the { min, n, max } combination (multi 
	   sunt, qui ad id, quod non proposuerant scribere, alicuius verbi 
	   placentis decore vocentur).
	   
	   The range option was added as an attempted fix for implementations 
	   that couldn't handle the single size option but the real problem is 
	   that the server knows what key sizes are appropriate but the client 
	   has to make the choice, without any knowledge of what the server can 
	   actually handle.  Because of this the spec (in its n-only mindset, 
	   which also applies to the min/n/max version since it's the same 
	   document) contains assorted weasel-words that allow the server to 
	   choose any key size it feels like if the client sends a range 
	   indication that's inappropriate.  Although the spec ends up saying 
	   that the server can do anything it feels like ("The server should 
	   return the smallest group it knows that is larger than the size the 
	   client requested.  If the server does not know a group that is 
	   larger than the client request, then it SHOULD return the largest 
	   group it knows"), we use a least-upper-bound interpretation of the 
	   above, mostly because we store a range of fixed keys of different 
	   sizes and can always find something reasonably close to any 
	   (sensible) requested length */
	status = initDHcontextSSH( &handshakeInfo->iServerCryptContext,
							   &handshakeInfo->serverKeySize, NULL, 0,
							   handshakeInfo->requestedServerKeySize );
	if( cryptStatusError( status ) )
		return( status );

	/* Send the DH key values to the client:

		byte	type = SSH_MSG_KEXDH_GEX_GROUP
		mpint	p
		mpint	g

	   Since this phase of the key negotiation exchanges raw key components
	   rather than the standard SSH public-key format we have to rewrite
	   the public key before we can send it to the client.  What this 
	   involves is stripping the:

		uint32	length
		string	"ssh-dh"

	   header from the start of the datab and then writing what's left to the 
	   packet.  First we export the key data and figure out the location of
	   the payload that we need to send */
	setMessageData( &msgData, keyData, ( CRYPT_MAX_PKCSIZE * 2 ) + 16 );
	status = krnlSendMessage( handshakeInfo->iServerCryptContext, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_KEY_SSH );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, keyData, msgData.length );
	readUint32( &stream );					/* Length */
	status = readUniversal32( &stream );	/* ID string */
	ENSURES( cryptStatusOK( status ) );
	keyDataStart = stell( &stream );
	keyDataLength = sMemDataLeft( &stream );
	sMemDisconnect( &stream );

	/* Then we create and send the SSH packet using as the payload the key
	   data content of the SSH public key */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_KEX_DH_GEX_GROUP );
	if( cryptStatusError( status ) )
		return( status );
	status = swrite( &stream, keyData + keyDataStart, keyDataLength );
	if( cryptStatusOK( status ) )
		status = wrapPlaintextPacketSSH2( sessionInfoPtr, &stream, 0 );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );
	return( status );
	}

/****************************************************************************
*																			*
*							Server-side Connect Functions					*
*																			*
****************************************************************************/

/* Perform the initial part of the handshake with the client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int beginServerHandshake( INOUT SESSION_INFO *sessionInfoPtr,
								 INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	STREAM stream;
	BOOLEAN skipGuessedKeyex = FALSE;
	void *serverHelloPtr DUMMY_INIT_PTR;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int length, serverHelloLength, clientHelloLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* Get the public-key algorithm that we'll be advertising to the client
	   and set the algorithm table used for processing the client hello to
	   match the one that we're offering */
	status = initPubkeyAlgo( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "initPubkeyAlgo" );

	/* SSH hashes the handshake ID strings for integrity-protection purposes, 
	   first the client string that we read previously and then our server 
	   string */
	status = hashHandshakeStrings( handshakeInfo, 
								   sessionInfoPtr->receiveBuffer,
								   sessionInfoPtr->receiveBufEnd,
								   SSH_ID_STRING, SSH_ID_STRING_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "hashHandshakeStrings" );

	/* Now that we've processed the out-of-band data in the receive buffer, 
	   mark it as empty */
	sessionInfoPtr->receiveBufEnd = 0;

	/* Send the server hello packet:

		byte		type = SSH_MSG_KEXINIT
		byte[16]	cookie
		string		keyex algorithms
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

	   The SSH spec leaves the order in which things happen ambiguous, in
	   order to save a while round trip it has provisions for both sides
	   shouting at each other and then a complex interlock process where
	   bits of the initial exchange can be discarded and retried if necessary.
	   This is ugly and error-prone.  The client code solves this by waiting
	   for the server hello, choosing known-good parameters based on what the
	   server communicates in its hello message, and then sending the client 
	   hello immediately followed by the client key exchange data.  Since it 
	   waits for the server to speak first it can choose parameters that are 
	   accepted the first time.

	   Unfortunately this doesn't work if we're the server since we'd end up 
	   waiting for the client to speak first while it waits for us to speak 
	   first, so we have to send the server hello in order to prevent 
	   deadlock.  This works fine with most clients, which take the same
	   approach and wait for the server to speak first.  The message flow is
	   then:

		server hello;
		client hello;
		client keyex;
		server keyex;

	   There are one or two exceptions to this, the worst of which is older
	   versions of the F-Secure client, which has the client speak first 
	   choosing as its preference the incompletely specified 
	   "x509v3-sign-dss" format (see the comment in exchangeServerKeys() 
	   below) that we can't use since no-one's quite sure what the format is 
	   (this was fixed in mid-2004 when the x509v3-* schemes were removed 
	   from the spec since no-one could figure out what they were.  F-Secure 
	   still specifies them, but has moved them down so that they follow 
	   the standard ssh-* schemes).  In this case the message flow is:

		server hello;
		client hello;
		client keyex1;
		client keyex2;
		server keyex;

	   This is handled by having the code that reads the client hello return
	   OK_SPECIAL to indicate that the next packet should be skipped.  An
	   alternative (and simpler) strategy would be to always throw away the
	   client's first keyex sent by older versions of the F-Secure client
	   since they're using an algorithm choice that's impossible to use, but 
	   that implementation-specific approach doesn't generalise well to 
	   other versions or other clients */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, SSH_MSG_KEXINIT );
	if( cryptStatusError( status ) )
		return( status );
	streamBookmarkSetFullPacket( &stream, serverHelloLength );
	status = exportVarsizeAttributeToStream( &stream, SYSTEM_OBJECT_HANDLE,
											 CRYPT_IATTRIBUTE_RANDOM_NONCE,
											 SSH2_COOKIE_SIZE );
	if( cryptStatusOK( status ) )
		{
		int pkcAlgo;

		/* If the server key is a non-ECC key then it can't be used with an 
		   ECC keyex so we have to explicitly disable it (technically it's 
		   possible to mix ECDH with RSA but this is more likely an error 
		   than anything deliberate) */
		status = krnlSendMessage( sessionInfoPtr->privateKey, 
								  IMESSAGE_GETATTRIBUTE, &pkcAlgo,
								  CRYPT_CTXINFO_ALGO );
		if( cryptStatusOK( status ) )
			{
			status = writeAlgoClassList( &stream, isEccAlgo( pkcAlgo ) ? \
											SSH_ALGOCLASS_KEYEX : \
											SSH_ALGOCLASS_KEYEX_NOECC );
			}
		}
	if( cryptStatusOK( status ) )
		{
		status = writeAlgoList( &stream, handshakeInfo->algoStringPubkeyTbl,
								handshakeInfo->algoStringPubkeyTblNoEntries );
		}
	if( cryptStatusOK( status ) )
		status = writeAlgoClassList( &stream, SSH_ALGOCLASS_ENCR );
	if( cryptStatusOK( status ) )
		status = writeAlgoClassList( &stream, SSH_ALGOCLASS_ENCR );
	if( cryptStatusOK( status ) )
		status = writeAlgoClassList( &stream, SSH_ALGOCLASS_MAC );
	if( cryptStatusOK( status ) )
		status = writeAlgoClassList( &stream, SSH_ALGOCLASS_MAC );
	if( cryptStatusOK( status ) )
		status = writeAlgoClassList( &stream, SSH_ALGOCLASS_COPR );
	if( cryptStatusOK( status ) )
		status = writeAlgoClassList( &stream, SSH_ALGOCLASS_COPR );
	if( cryptStatusOK( status ) )
		{
		writeUint32( &stream, 0 );			/* No language tag */
		writeUint32( &stream, 0 );
		sputc( &stream, 0 );				/* Don't try and guess the keyex */
		status = writeUint32( &stream, 0 );	/* Reserved */
		}
	if( cryptStatusOK( status ) )
		{
		status = streamBookmarkComplete( &stream, &serverHelloPtr, 
										 &serverHelloLength, 
										 serverHelloLength );
		}
	INJECT_FAULT( SESSION_CORRUPT_HANDSHAKE, SESSION_CORRUPT_HANDSHAKE_SSH_1 );
	if( cryptStatusOK( status ) )
		status = wrapPlaintextPacketSSH2( sessionInfoPtr, &stream, 0 );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( serverHelloPtr != NULL );
	INJECT_FAULT( SESSION_CORRUPT_HANDSHAKE, SESSION_CORRUPT_HANDSHAKE_SSH_2 );
	CFI_CHECK_UPDATE( "sendServerHello" );

	/* Process the client hello packet and hash the client and server
	   hello.  Since the entire encoded packet (including the type value) 
	   is hashed we have to reconstruct this at the start of the client
	   hello packet */
	status = processHelloSSH( sessionInfoPtr, handshakeInfo,
							  &clientHelloLength, TRUE );
	if( cryptStatusError( status ) )
		{
		if( status != OK_SPECIAL )
			return( status );

		/* There's a guessed keyex following the client hello that we have
		   to skip later (we can't process it at this point because we still
		   need to hash the data sitting in the receive buffer) */
		skipGuessedKeyex = TRUE;
		}
	REQUIRES( rangeCheck( clientHelloLength, 1,
						  sessionInfoPtr->receiveBufSize ) );
	memmove( sessionInfoPtr->receiveBuffer + 1, 
			 sessionInfoPtr->receiveBuffer, clientHelloLength );
	sessionInfoPtr->receiveBuffer[ 0 ] = SSH_MSG_KEXINIT;
	status = hashAsString( handshakeInfo->iExchangeHashContext,
						   sessionInfoPtr->receiveBuffer,
						   clientHelloLength + 1 );
	if( cryptStatusOK( status ) && skipGuessedKeyex )
		{
		/* There's an incorrectly-guessed keyex following the client hello, 
		   skip it */
		status = readHSPacketSSH2( sessionInfoPtr, 
								   ( handshakeInfo->isFixedDH || \
								     handshakeInfo->isECDH ) ?
									SSH_MSG_KEXDH_INIT : \
									SSH_MSG_KEX_DH_GEX_INIT,
								   ID_SIZE + sizeofString32( MIN_PKCSIZE ) );
		}
	if( !cryptStatusError( status ) )	/* readHSPSSH2() returns a length */
		{
		status = hashAsString( handshakeInfo->iExchangeHashContext,
							   serverHelloPtr, serverHelloLength );
		}
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "processHelloSSH" );

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
			status = processDHE( sessionInfoPtr, handshakeInfo );
 			}
		}
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create %s ephemeral key data",
				  handshakeInfo->isECDH ? "ECDH" : "DH" ) );
		}
	CFI_CHECK_UPDATE( "processDHE" );

	/* Process the client keyex:

	   DH:
		byte	type = SSH_MSG_KEXDH_INIT / SSH_MSG_KEXDH_GEX_INIT
		mpint	y
	   ECDH:
		byte	type = SSH_MSG_KEX_ECDH_INIT
		string	q_c

	   SSH_MSG_KEXDH_INIT (isFixedDH) and SSH_MSG_KEX_ECDH_INIT (isECDH) 
	   have the same value so they're handled with the same code.

	   Since we're reading the value as a raw object (as it gets hashed 
	   later), we need to offset the length field of LENGTH_SIZE bytes in
	   later calculations with the data */
	status = length = \
		readHSPacketSSH2( sessionInfoPtr,
						  ( handshakeInfo->isFixedDH || \
						    handshakeInfo->isECDH ) ? \
							SSH_MSG_KEXDH_INIT : SSH_MSG_KEX_DH_GEX_INIT,
						  ID_SIZE + ( handshakeInfo->isECDH ? \
							sizeofString32( MIN_PKCSIZE_ECCPOINT ) : \
							sizeofString32( MIN_PKCSIZE ) ) );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	status = readRawObject32( &stream, handshakeInfo->clientKeyexValue,
							  MAX_ENCODED_KEYEXSIZE,
							  &handshakeInfo->clientKeyexValueLength );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid %s phase 1 keyex value",
				  handshakeInfo->isECDH ? "ECDH" : "DH" ) );
		}
	if( handshakeInfo->isECDH )
		{
		if( !isValidECDHsize( handshakeInfo->clientKeyexValueLength,
							  handshakeInfo->serverKeySize, LENGTH_SIZE ) )
			status = CRYPT_ERROR_BADDATA;
		}
	else
		{
		if( !isValidDHsize( handshakeInfo->clientKeyexValueLength,
							handshakeInfo->serverKeySize, LENGTH_SIZE ) )
			status = CRYPT_ERROR_BADDATA;
		}
	if( cryptStatusError( status ) )
		{
		const int clientKeyexSize = handshakeInfo->isECDH ? \
			extractECDHsize( handshakeInfo->clientKeyexValueLength, LENGTH_SIZE ) : \
			extractDHsize( handshakeInfo->clientKeyexValueLength, LENGTH_SIZE );

		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid %s phase 1 keyex size, expected %d (%d), "
				  "got %d (%d)", handshakeInfo->isECDH ? "ECDH" : "DH",
				  handshakeInfo->serverKeySize, 
				  handshakeInfo->serverKeySize * 8,
				  clientKeyexSize, clientKeyexSize * 8 ) );
		}
	CFI_CHECK_UPDATE( "SSH_MSG_KEXDH_INIT" );

	/* If we're fuzzing the input then we're reading static data for which 
	   we can't go beyond this point */
	FUZZ_EXIT();

	REQUIRES( CFI_CHECK_SEQUENCE_6( "initPubkeyAlgo", "hashHandshakeStrings", 
									"sendServerHello", "processHelloSSH", 
									"processDHE", "SSH_MSG_KEXDH_INIT" ) );
	return( CRYPT_OK );
	}

/* Exchange keys with the client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int exchangeServerKeys( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	KEYAGREE_PARAMS keyAgreeParams;
	STREAM stream;
	void *keyPtr DUMMY_INIT_PTR, *dataPtr;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int keyLength, dataLength, sigLength DUMMY_INIT, packetOffset, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* Create the server DH/ECDH value */
	memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
	status = krnlSendMessage( handshakeInfo->iServerCryptContext,
							  IMESSAGE_CTX_ENCRYPT, &keyAgreeParams,
							  sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusError( status ) )
		return( status );
	sMemOpen( &stream, handshakeInfo->serverKeyexValue, 
			  MAX_ENCODED_KEYEXSIZE );
	if( handshakeInfo->isECDH )
		{
		status = writeString32( &stream, keyAgreeParams.publicValue,
								keyAgreeParams.publicValueLen );
		}
	else
		{
		status = writeInteger32( &stream, keyAgreeParams.publicValue,
								 keyAgreeParams.publicValueLen );
		}
	if( cryptStatusOK( status ) )
		handshakeInfo->serverKeyexValueLength = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "IMESSAGE_CTX_ENCRYPT" );

	/* Build the DH phase 2 keyex packet:

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
		...

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
		...

	   SSH_MSG_KEXDH_REPLY (isFixedDH) and SSH_MSG_KEX_ECDH_REPLY (isECDH) 
	   have the same value so they're handled with the same code.

	   The specification also makes provision for using X.509 and PGP keys,
	   but only so far as to say that keys and signatures are in "X.509 DER"
	   and "PGP" formats, neither of which actually explain what it is
	   that's sent or signed (and no-one on the SSH list can agree on what
	   they're supposed to look like) so we can't use either of them */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  ( handshakeInfo->isFixedDH || \
								    handshakeInfo->isECDH ) ? \
									SSH_MSG_KEXDH_REPLY : \
									SSH_MSG_KEX_DH_GEX_REPLY );
	if( cryptStatusError( status ) )
		return( status );
	streamBookmarkSet( &stream, keyLength );
	INJECT_FAULT( SESSION_WRONGCERT, SESSION_WRONGCERT_SSH_1 );
	status = exportAttributeToStream( &stream, sessionInfoPtr->privateKey,
									  CRYPT_IATTRIBUTE_KEY_SSH );
	if( cryptStatusOK( status ) )
		{
		status = streamBookmarkComplete( &stream, &keyPtr, &keyLength, 
										 keyLength );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( handshakeInfo->iExchangeHashContext,
								  IMESSAGE_CTX_HASH, keyPtr, keyLength );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	INJECT_FAULT( SESSION_WRONGCERT, SESSION_WRONGCERT_SSH_2 );
	INJECT_FAULT( SESSION_CORRUPT_KEYEX_SERVER, 
				  SESSION_CORRUPT_KEYEX_SERVER_SSH_1 );
	status = swrite( &stream, handshakeInfo->serverKeyexValue,
					 handshakeInfo->serverKeyexValueLength );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	INJECT_FAULT( SESSION_CORRUPT_KEYEX_SERVER, 
				  SESSION_CORRUPT_KEYEX_SERVER_SSH_1 );
	CFI_CHECK_UPDATE( "SSH_MSG_KEXDH_REPLY" );

	/* Complete phase 2 of the DH key agreement process to obtain the shared
	   secret value */
	status = completeKeyex( sessionInfoPtr, handshakeInfo, TRUE );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "completeKeyex" );

	/* Sign the hash.  The reason for the min() part of the expression is
	   that iCryptCreateSignature() gets suspicious of very large buffer
	   sizes, for example when the user has specified the use of a 1MB send
	   buffer */
	status = sMemGetDataBlockRemaining( &stream, &dataPtr, &dataLength );
	if( cryptStatusOK( status ) )
		{
		status = iCryptCreateSignature( dataPtr, 
							min( dataLength, MAX_INTLENGTH_SHORT - 1 ), 
							&sigLength, CRYPT_IFORMAT_SSH, 
							sessionInfoPtr->privateKey,
							handshakeInfo->iExchangeHashContext, NULL );
		insertCryptoDelay();
		}
	krnlSendNotifier( handshakeInfo->iExchangeHashContext,
					  IMESSAGE_DECREFCOUNT );
	handshakeInfo->iExchangeHashContext = CRYPT_ERROR;
	if( handshakeInfo->iExchangeHashAltContext != CRYPT_ERROR )
		{
		krnlSendNotifier( handshakeInfo->iExchangeHashAltContext,
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->iExchangeHashAltContext = CRYPT_ERROR;
		}
	if( cryptStatusOK( status ) )
		{
		INJECT_FAULT( BADSIG_SIG, SESSION_BADSIG_SIG_SSH_1 );
		status = sSkip( &stream, sigLength, MAX_INTLENGTH_SHORT );
		}
	if( cryptStatusOK( status ) )
		status = wrapPlaintextPacketSSH2( sessionInfoPtr, &stream, 0 );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	CFI_CHECK_UPDATE( "iCryptCreateSignature" );

	/* Set up the security information required for the session.  We have to 
	   do this before sending the change cipherspec (rather than during the 
	   pause while we're waiting for the other side's response) because we 
	   can only tell the other side to switch to secure mode if we're sure 
	   that we're already in that state ourselves */
	status = initSecurityInfo( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "initSecurityInfo" );

	/* Build our change cipherspec message and send the whole mess through
	   to the client:
		...
		byte	type = SSH_MSG_NEWKEYS

	   After this point the write channel is in the secure state, so we 
	   switch from wrapPlaintextPacketSSH2() to wrapPacketSSH2() */
	status = continuePacketStreamSSH( &stream, SSH_MSG_NEWKEYS, 
									  &packetOffset );
	if( cryptStatusOK( status ) )
		{
		status = wrapPlaintextPacketSSH2( sessionInfoPtr, &stream, 
										  packetOffset );
		}
	if( cryptStatusOK( status ) && handshakeInfo->sendExtInfo )
		{
		/* If the client has indicated that it supports extension 
		   information, send our extensions */
		status = continuePacketStreamSSH( &stream, SSH_MSG_EXT_INFO, 
										  &packetOffset );
		if( cryptStatusOK( status ) )
			status = writeExtensionsSSH( sessionInfoPtr, &stream );
		if( cryptStatusOK( status ) )
			{
			status = wrapPacketSSH2( sessionInfoPtr, &stream, 
									 packetOffset, FALSE );
			}
		}
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	SET_FLAG( sessionInfoPtr->flags, SESSION_FLAG_ISSECURE_WRITE );
	CFI_CHECK_UPDATE( "SSH_MSG_NEWKEYS" );

	REQUIRES( CFI_CHECK_SEQUENCE_6( "IMESSAGE_CTX_ENCRYPT", "SSH_MSG_KEXDH_REPLY", 
									"completeKeyex", "iCryptCreateSignature", 
									"initSecurityInfo", "SSH_MSG_NEWKEYS" ) );
	return( CRYPT_OK );
	}

/* Complete the handshake with the client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int completeServerHandshake( INOUT SESSION_INFO *sessionInfoPtr,
									STDC_UNUSED \
										SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	STREAM stream;
	BOOLEAN userInfoPresent = FALSE;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* If this is the first time through, set up the security information 
	   and wait for the client's pre-authentication */
	if( !TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_PARTIALOPEN ) )
		{
		BYTE stringBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
		int stringLength;

		/* If the caller has supplied user information to match against then 
		   we require a match against the fixed caller-supplied information 
		   rather than accepting what the client sends us and passing it 
		   back to the caller to check */
		if( findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_USERNAME ) != NULL )
			userInfoPresent = TRUE;

		/* Wait for the client's change cipherspec message.  From this point
		   on the read channel is in the secure state */
		status = readHSPacketSSH2( sessionInfoPtr, SSH_MSG_NEWKEYS, 
								   ID_SIZE );
		if( cryptStatusError( status ) )
			return( status );
		SET_FLAG( sessionInfoPtr->flags, SESSION_FLAG_ISSECURE_READ );

		/* Wait for the client's pre-authentication packets, which aren't 
		   used for any authentication but which are required anyway by the
		   protocol.  For some reason SSH requires the use of two messages 
		   where one would do, first an "I'm about to authenticate" packet 
		   and then an "I'm authenticating" packet after that ("What are you 
		   preparing?! You're always preparing! Just go!").  Since the 
		   former isn't useful for anything we clear it to get it out of the 
		   way so that we can perform the actual authentication:

			byte	type = SSH_MSG_SERVICE_REQUEST
			string	service_name = "ssh-userauth"

			byte	type = SSH_MSG_SERVICE_ACCEPT
			string	service_name = "ssh-userauth" */
		status = length = \
			readHSPacketSSH2( sessionInfoPtr, SSH_MSG_SERVICE_REQUEST,
							  ID_SIZE + sizeofString32( 8 ) );
		if( cryptStatusError( status ) )
			return( status );
		sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
		status = readString32( &stream, stringBuffer, CRYPT_MAX_TEXTSIZE,
							   &stringLength );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid service request packet" ) );
			}
		if( stringLength != 12 || \
			memcmp( stringBuffer, "ssh-userauth", 12 ) )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid service request packet '%s'",
					  sanitiseString( stringBuffer, CRYPT_MAX_TEXTSIZE, 
									  stringLength ) ) );
			}
		status = openPacketStreamSSH( &stream, sessionInfoPtr, 
									  SSH_MSG_SERVICE_ACCEPT );
		if( cryptStatusError( status ) )
			return( status );
		status = writeString32( &stream, "ssh-userauth", 12 );
		if( cryptStatusOK( status ) )
			status = wrapSendPacketSSH2( sessionInfoPtr, &stream );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		}
	CFI_CHECK_UPDATE( "SSH_MSG_NEWKEYS" );

	/* Process the client's authentication */
	status = processServerAuth( sessionInfoPtr, handshakeInfo, 
								userInfoPresent );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "processServerAuth" );

	/* Handle the channel open */
	status = length = \
		readHSPacketSSH2( sessionInfoPtr, SSH_MSG_CHANNEL_OPEN,
						  ID_SIZE + sizeofString32( 4 ) + \
							UINT32_SIZE + UINT32_SIZE + UINT32_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	status = processChannelOpen( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "processChannelOpen" );

	REQUIRES( CFI_CHECK_SEQUENCE_3( "SSH_MSG_NEWKEYS", "processServerAuth", 
									"processChannelOpen" ) );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initSSH2serverProcessing( INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	FNPTR_SET( handshakeInfo->beginHandshake, beginServerHandshake );
	FNPTR_SET( handshakeInfo->exchangeKeys, exchangeServerKeys );
	FNPTR_SET( handshakeInfo->completeHandshake, completeServerHandshake );
	}
#endif /* USE_SSH */
