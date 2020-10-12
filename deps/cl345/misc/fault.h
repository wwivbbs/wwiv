/****************************************************************************
*																			*
*						cryptlib Fault-Injection Header File 				*
*						Copyright Peter Gutmann 1998-2015					*
*																			*
****************************************************************************/

#ifndef _FAULT_DEFINED

#define _FAULT_DEFINED

/* In the debug build, the code includes additional functionality to inject 
   faults that verify that possible conditions such as invalid signatures 
   are correctly detected.  The following values are check types that
   can be performed for the higher-level envelope and session objects */

#ifndef NDEBUG

#ifdef CONFIG_FAULTS

/****************************************************************************
*																			*
*							Fault Types and Handling						*
*																			*
****************************************************************************/

/* The various fault types that we can inject */

typedef enum {
	FAULT_NONE,					/* No fault type */

	/* Generic faults */
	FAULT_BADSIG_HASH,			/* Sig.check failure - bad hash/MAC */
	FAULT_BADSIG_DATA,			/* Sig.check failure - bad data */
	FAULT_BADSIG_SIG,			/* Sig.check failure - bad signature */
	FAULT_CORRUPT_ID,			/* Corrupt ID/username */
	FAULT_CORRUPT_AUTHENTICATOR, /* Corrupt password/auth.token */

	/* Low-level mechanism faults.  These are distinct from the generic 
	   faults like FAULT_BADSIG_HASH since that injects the fault at a 
	   higher level like an envelope or session, while these inject the
	   fault at the crypto mechanism level */
	FAULT_MECH_CORRUPT_HASH,	/* Corrupt hash value in signature */
	FAULT_MECH_CORRUPT_SIG,		/* Corrupt signature value */
	FAULT_MECH_CORRUPT_KEY,		/* Corrupt encrypted key */
	FAULT_MECH_CORRUPT_SALT,	/* Corrupt salt in PRF */
	FAULT_MECH_CORRUPT_ITERATIONS,	/* Corrupt PRF iteration count */
	FAULT_MECH_CORRUPT_PRFALGO,	/* Corrupt PRF algorithm */

	/* Envelope faults */
	FAULT_ENVELOPE_CORRUPT_AUTHATTR, /* Corruption of auth.attributes */

	/* CMS protocol-specific faults */
	FAULT_ENVELOPE_CMS_CORRUPT_AUTH_DATA, /* Corruption of authData data */
	FAULT_ENVELOPE_CMS_CORRUPT_AUTH_MAC, /* Corruption of authData MAC */

	/* PGP protocol-specific faults */
	FAULT_ENVELOPE_PGP_CORRUPT_ONEPASS_ID,	/* Corruption of onepass keyID */

	/* Session faults.  The different between CORRUPT_DATA/MAC and 
	   BADSIG_DATA/MAC is that the former is for session payload data in SSL
	   and SSH sessions while the latter is for handshake/protocol data,
	   MACs, and signatures, for example in the SSH handshake or CMP 
	   exchange */
	FAULT_SESSION_CORRUPT_HANDSHAKE, /* Corruption of handshake messages */
	FAULT_SESSION_CORRUPT_NONCE,/* Corruption of sender/recipient nonce */
	FAULT_SESSION_CORRUPT_KEYEX_CLIENT, /* Corruption of client keyex value */
	FAULT_SESSION_CORRUPT_KEYEX_SERVER, /* Corruption of server keyex value */
	FAULT_SESSION_CORRUPT_DATA,	/* Corruption/manip.of session data */
	FAULT_SESSION_CORRUPT_MAC,	/* Corruption of session data MAC value */
	FAULT_SESSION_WRONGCERT,	/* Wrong cert.for URL/email address */

	/* SSH protocol-specific faults */
	FAULT_SESSION_SSH_CORRUPT_EXCHANGE_HASH, /* Corruption of exchange hash */
	FAULT_SESSION_SSH_CORRUPT_CHANNEL_OPEN,	/* Corruption of channel no. on open */
	FAULT_SESSION_SSH_CORRUPT_CHANNEL_DATA,	/* Corruption of channel no. on data */
	FAULT_SESSION_SSH_CORRUPT_CHANNEL_CLOSE, /* Corruption of channel no. on close */
	FAULT_SESSION_SSH_CORRUPT_CHANNEL_REQUEST, /* Corruption of channel no. on req. */

	/* SSL protocol-specific faults */
	FAULT_SESSION_SSL_CORRUPT_FINISHED,	/* Corruption of finished MAC */
	FAULT_SESSION_SSL_CORRUPT_IV,	/* Corruption of data IV */

	/* SCEP protocol-specific faults */
	FAULT_SESSION_SCEP_CORRUPT_MESSAGETYPE,	/* Corruption of message type */
	FAULT_SESSION_SCEP_CORRUPT_TRANSIDVALUE,/* Corruption of transID checksum */

	FAULT_LAST					/* Last possible fault type */
	} FAULT_TYPE;

/* The handler for injecting faults.  This would be called as:

	INJECT_FAULT( SESSION_WRONGCERT, SESSION_WRONGCERT_1 ); */

#define INJECT_FAULT( type, action ) \
		if( faultType == ( FAULT_##type ) ) \
			{ \
			FAULTACTION_##action; \
			}

/****************************************************************************
*																			*
*								Mechanism Faults							*
*																			*
****************************************************************************/

/* Corrupt the hash and signature values */

#define FAULTACTION_MECH_CORRUPT_HASH_1 \
		if( signature != NULL ) \
			{ \
			krnlSendMessage( iHashContext, IMESSAGE_DELETEATTRIBUTE, NULL, \
							 CRYPT_CTXINFO_HASHVALUE ); \
			krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, "1234", 4 ); \
			krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, "", 0 ); \
			}
#define FAULTACTION_MECH_CORRUPT_SIG_1 \
		if( signature != NULL ) \
			buffer[ 8 ]++

/* Corrupt wrapped keys */

#define FAULTACTION_MECH_CORRUPT_KEY_1 \
		if( encryptedKey != NULL ) \
			( ( BYTE * ) mechanismInfo.wrappedData )[ 8 ]++

/* Corrupt the encoded PRF salt, iteration count, and PRF algorithm.  The 
   algorithm is changed from ... 9 == SHA256 to ... 7 == SHA1.  An 
   alternative option is to corrupt to ... 11 == SHA512, but this isn't
   detected at the external-API level because there's no way to change the 
   hash size of SHA2 from the default SHA-256 to SHA-512 via the external
   API, so what's reported as SHA512 is processed as SHA256 */

#define FAULTACTION_MECH_CORRUPT_SALT_1 \
		if( encryptedKey != NULL ) \
			{ \
			assert( ( ( BYTE * ) encryptedKey )[ 20 ] == 0x04 ); \
			assert( ( ( BYTE * ) encryptedKey )[ 21 ] == 0x08 ); \
			( ( BYTE * ) encryptedKey )[ 24 ]++; \
			}
#define FAULTACTION_MECH_CORRUPT_ITERATIONS_1 \
		if( encryptedKey != NULL ) \
			{ \
			assert( ( ( BYTE * ) encryptedKey )[ 30 ] == 0x02 ); \
			assert( ( ( BYTE * ) encryptedKey )[ 31 ] == 0x03 ); \
			( ( BYTE * ) encryptedKey )[ 34 ]--; \
			}
#define FAULTACTION_MECH_CORRUPT_PRFALGO_1 \
		if( encryptedKey != NULL ) \
			{ \
			assert( ( ( BYTE * ) encryptedKey )[ 37 ] == 0x06 ); \
			assert( ( ( BYTE * ) encryptedKey )[ 38 ] == 0x08 ); \
			( ( BYTE * ) encryptedKey )[ 46 ] = 0x07; \
			}

/****************************************************************************
*																			*
*								Envelope Faults								*
*																			*
****************************************************************************/

/* CMS enveloping fault types */

/* Corrupt the payload data or payload hash added to the signedAttrs for 
   signed data */

#define FAULTACTION_BADSIG_DATA_CMS_1 \
		envelopeInfoPtr->buffer[ envelopeInfoPtr->bufPos - 4 ]++
#define FAULTACTION_BADSIG_HASH_CMS_1 \
		{ \
		ACTION_LIST *actionListPtr = DATAPTR_GET( envelopeInfoPtr->actionList ); \
		\
		krnlSendMessage( actionListPtr->iCryptHandle, IMESSAGE_CTX_HASH, "1234", 4 ); \
		}

/* Corrupt the payload data or MAC for authData and authEncData (the same
   corruption is used for both enveloping types) */

#define FAULTACTION_ENVELOPE_CMS_CORRUPT_AUTH_DATA_1 \
		envelopeInfoPtr->buffer[ envelopeInfoPtr->bufPos - 8 ]++
#define FAULTACTION_ENVELOPE_CMS_CORRUPT_AUTH_MAC_1 \
		hash[ 4 ]++

/* Corrupt the authenticated attributes */

#define FAULTACTION_ENVELOPE_CORRUPT_AUTHATTR_CMS_1 \
		if( signature != NULL ) \
			{ \
			assert( cmsAttributeInfo.encodedAttributes[ 46 ] == 0x04 ); \
			assert( cmsAttributeInfo.encodedAttributes[ 47 ] == 0x08 ); \
			cmsAttributeInfo.encodedAttributes[ 48 ]++; \
			}

/* PGP enveloping fault types */

/* Corrupt the keyID/one-pass signature packet keyID */

#define FAULTACTION_CORRUPT_ID_PGP_1 \
		keyID[ 4 ]++
#define FAULTACTION_ENVELOPE_PGP_CORRUPT_ONEPASS_ID_1 \
		keyID[ 4 ]++

/* Corrupt the payload data after the MDC is calculated or corrupt the MDC */

#define FAULTACTION_ENVELOPE_BADSIG_DATA_PGP_1 \
		krnlSendMessage( iMdcContext, IMESSAGE_CTX_HASH, "1234", 4 )
#define FAULTACTION_ENVELOPE_BADSIG_HASH_PGP_1 \
		mdcBuffer[ 4 ]++

/* Corrupt the authenticated attributes */

#define FAULTACTION_ENVELOPE_CORRUPT_AUTHATTR_PGP_1 \
		extraData[ 10 ]++

/****************************************************************************
*																			*
*								Session Faults								*
*																			*
****************************************************************************/

/* SSH session fault types */

/* Corrupt the server hello */

#define FAULTACTION_SESSION_CORRUPT_HANDSHAKE_SSH_1 \
		if( cryptStatusOK( status ) ) \
			( ( BYTE * ) serverHelloPtr )[ serverHelloLength - 1 ]++ 
#define FAULTACTION_SESSION_CORRUPT_HANDSHAKE_SSH_2 \
		( ( BYTE * ) serverHelloPtr )[ serverHelloLength - 1 ]--

/* Corrupt the payload data and MAC */

#define FAULTACTION_SESSION_CORRUPT_DATA_SSH_1 \
		if( isServer( sessionInfoPtr ) ) \
			{ \
			sessionInfoPtr->sendBuffer[ SSH2_HEADER_SIZE + \
										SSH2_PAYLOAD_HEADER_SIZE + 4 ]++; \
			}
#define FAULTACTION_SESSION_CORRUPT_MAC_SSH_1 \
		mac[ 4 ]++

/* Swap the server key for a different one */

#define FAULTACTION_SESSION_WRONGCERT_SSH_1 \
		faultParam1 = sessionInfoPtr->privateKey; \
		status = getSubstituteKey( &sessionInfoPtr->privateKey ); \
		if( cryptStatusError( status ) ) \
			retIntError()
#define FAULTACTION_SESSION_WRONGCERT_SSH_2 \
		krnlSendNotifier( sessionInfoPtr->privateKey, IMESSAGE_DESTROY ); \
		sessionInfoPtr->privateKey = faultParam1

/* Corrupt the keyex hash that's signed and sent to the client */

#define FAULTACTION_SESSION_BADSIG_HASH_SSH_1 \
		if( isServer ) \
			{ \
			krnlSendMessage( handshakeInfo->iExchangeHashContext, \
							 IMESSAGE_CTX_HASH, "x", 1 ); \
			}

/* Corrupt the signature value that's sent to the client */

#define FAULTACTION_SESSION_BADSIG_SIG_SSH_1 \
		( ( BYTE * ) dataPtr )[ sigLength - 4 ]++

/* Corrupt the username and password.  These checks can't be used because 
   the server-side code merely reports what's received to the caller so it
   doesn't care what the value actually is since the check is performed by
   the caller */

#define FAULTACTION_SESSION_CORRUPT_ID_SSH_1 \
		userNameBuffer[ 0 ]++
#define FAULTACTION_SESSION_CORRUPT_AUTHENTICATOR_SSH_1 \
		if( cryptStatusOK( status ) ) \
			stringBuffer[ 0 ]++

/* Corrupt the client keyex value that's sent to the server and server keyex 
   value that's sent to the client */

#define FAULTACTION_SESSION_CORRUPT_KEYEX_CLIENT_SSH_1 \
		keyAgreeParams.publicValue[ 8 ]++
#define FAULTACTION_SESSION_CORRUPT_KEYEX_CLIENT_SSH_2 \
		keyAgreeParams.publicValue[ 8 ]--
#define FAULTACTION_SESSION_CORRUPT_KEYEX_SERVER_SSH_1 \
		handshakeInfo->serverKeyexValue[ 8 ]++
#define FAULTACTION_SESSION_CORRUPT_KEYEX_SERVER_SSH_2 \
		handshakeInfo->serverKeyexValue[ 8 ]--

/* Corrupt the server's exchange hash */

#define FAULTACTION_SESSION_SSH_CORRUPT_EXCHANGE_HASH_1 \
		if( isServer ) \
			handshakeInfo->sessionID[ handshakeInfo->sessionIDlength - 1 ]++

/* Corrupt the server's channel number that it sends to the client.  Note 
   that two of the fault checks can't be used:
   
   The channel close fault check can't be used because, while it's detected 
   on the server side, the overall self-test status comes from the client 
   side, and that's closing down the session so it doesn't perform any 
   further communications with the server that would show up the problem.  
   The result is an error message printed by the server-side test code, but 
   no overall error from the test as a whole.
   
   The channel request fault check can't be used because cryptlib always
   sets want_reply to false on these requests, so there's no message to
   corrupt returned */

#define FAULTACTION_SESSION_SSH_CORRUPT_CHANNEL_OPEN_1 \
		channelNo += 0x100
#define FAULTACTION_SESSION_SSH_CORRUPT_CHANNEL_OPEN_2 \
		channelNo -= 0x100
#define FAULTACTION_SESSION_SSH_CORRUPT_CHANNEL_DATA_1 \
		sseek( &stream, stell( &stream ) - UINT32_SIZE ); \
		writeUint32( &stream, \
					 getCurrentChannelNo( sessionInfoPtr, \
										  CHANNEL_WRITE ) + 0x100 )
#define FAULTACTION_SESSION_SSH_CORRUPT_CHANNEL_CLOSE_1 \
		{ \
		long *channelPtr = ( long * ) &channelNo; \
		( *channelPtr ) += 0x100; \
		}
#define FAULTACTION_SESSION_SSH_CORRUPT_CHANNEL_CLOSE_2 \
		{ \
		long *channelPtr = ( long * ) &channelNo; \
		( *channelPtr ) -= 0x100; \
		}
#define FAULTACTION_SESSION_SSH_CORRUPT_CHANNEL_REQUEST_1 \
		{ \
		long *channelPtr = ( long * ) &prevChannelNo; \
		( *channelPtr ) += 0x100; \
		}
#define FAULTACTION_SESSION_SSH_CORRUPT_CHANNEL_REQUEST_2 \
		{ \
		long *channelPtr = ( long * ) &prevChannelNo; \
		( *channelPtr ) -= 0x100; \
		}

/* SSL session fault types */

/* Corrupt the session ID (this actually gets detected by us first since the 
   client sends its Finished before we do, and the MAC in the Finished 
   doesn't match what we're expecting).  The session ID is arbitrary and not 
   used in any crypto calculations, which makes it safe to corrupt since it 
   has to be detected entirely by a MAC failure */

#define FAULTACTION_SESSION_CORRUPT_HANDSHAKE_SSL_1 \
		faultParam1 = ( stell( stream ) - handshakeInfo->sessionIDlength ) + 4; \
		sessionInfoPtr->sendBuffer[ faultParam1 ]++
#define FAULTACTION_SESSION_CORRUPT_HANDSHAKE_SSL_2 \
		if( cryptStatusOK( status ) ) \
			sessionInfoPtr->sendBuffer[ faultParam1 ]--

/* Corrupt the payload data and MAC */

#define FAULTACTION_SESSION_CORRUPT_DATA_SSL_1 \
		if( !cryptStatusError( status ) ) \
			sessionInfoPtr->sendBuffer[ SSL_HEADER_SIZE + 4 ]++
#define FAULTACTION_SESSION_CORRUPT_MAC_SSL_1 \
		( ( BYTE * ) data )[ payloadLength + 2 ]++

/* Swap the server certificate for a different one */

#define FAULTACTION_SESSION_WRONGCERT_SSL_1 \
		faultParam1 = sessionInfoPtr->privateKey; \
		status = getSubstituteKey( &sessionInfoPtr->privateKey ); \
		if( cryptStatusError( status ) ) \
			retIntError()
#define FAULTACTION_SESSION_WRONGCERT_SSL_2 \
		krnlSendNotifier( sessionInfoPtr->privateKey, IMESSAGE_DESTROY ); \
		sessionInfoPtr->privateKey = faultParam1

/* Corrupt the finished MAC value */

#define FAULTACTION_SESSION_SSL_CORRUPT_FINISHED_1 \
		( ( BYTE * ) hashValues )[ 4 ]++
#define FAULTACTION_SESSION_SSL_CORRUPT_FINISHED_2 \
		( ( BYTE * ) hashValues )[ 4 ]--

/* Corrupt the hash value */

#define FAULTACTION_SESSION_BADSIG_HASH_SSL_1 \
		krnlSendMessage( shaContext, IMESSAGE_DELETEATTRIBUTE, NULL, \
						 CRYPT_CTXINFO_HASHVALUE ); \
		status = krnlSendMessage( shaContext, IMESSAGE_CTX_HASH, \
								  ( MESSAGE_CAST ) keyData, keyDataLength ); \
		if( cryptStatusOK( status ) ) \
			status = krnlSendMessage( shaContext, IMESSAGE_CTX_HASH, "", 0 ); \
		if( cryptStatusError( status ) ) \
			retIntError()

/* Corrupt the server keyex value */

#define FAULTACTION_SESSION_BADSIG_DATA_SSL_1 \
		( ( BYTE * ) keyData )[ 16 ]++
#define FAULTACTION_SESSION_BADSIG_DATA_SSL_2 \
		( ( BYTE * ) keyData )[ 16 ]--

/* Corrupt the server keyex signature */

#define FAULTACTION_SESSION_BADSIG_SIG_SSL_1 \
		( ( BYTE * ) dataPtr )[ 16 ]++

/* Corrupt the IV */

#define FAULTACTION_SESSION_SSL_CORRUPT_IV_1 \
		iv[ 2 ]++

/* CMP session fault types */

/* Corrupt the transactionID */

#define FAULTACTION_SESSION_CORRUPT_ID_CMP_1 \
		protocolInfo.transID[ 4 ]++

/* Corrupt the sender nonce.  This isn't currently used because it's one of 
   the many unnecessary fields that CMP includes in messages, the same 
   function is served by the transactionID (checked for with 
   FAULT_SESSION_CORRUPT_ID) so cryptlib never uses nonces.  Sine there's no
   nonce present, there's nothing to check for corruption */

#define FAULTACTION_SESSION_CORRUPT_NONCE_CMP_1 \
		protocolInfo.senderNonce[ 4 ]++

/* Corrupt the CMP request data (offset 25 = part of the DN), MAC, or 
   signature */

#define FAULTACTION_SESSION_BADSIG_DATA_CMP_1 \
		if( !isServer( sessionInfoPtr ) ) \
			sessionInfoPtr->receiveBuffer[ 25 ]++
#define FAULTACTION_SESSION_BADSIG_HASH_CMP_1 \
		if( !isServer( sessionInfoPtr ) ) \
			sessionInfoPtr->receiveBuffer[ 16 ]++
#define FAULTACTION_SESSION_BADSIG_HASH_CMP_2 \
		if( !isServer( sessionInfoPtr ) ) \
			sessionInfoPtr->receiveBuffer[ 16 ]--
#define FAULTACTION_SESSION_BADSIG_SIG_CMP_1 \
		if( cryptStatusOK( status ) && !isServer( sessionInfoPtr ) ) \
			sessionInfoPtr->receiveBuffer[ stell( &stream ) - 16 ]++

/* SCEP session fault types */

/* Corrupt the transactionID and nonce returned from the server to the 
   client.  The transactionID is checksummed so there are two types of
   corruption possible, one where a new valid transactionID is substituted
   for the old one and one where the transactionID data is corrupted so that
   the checksum no longer validates.  The FAULT_SESSION_CORRUPT_ID 
   substitutes an incorrect (but valid) transactionID for the expected one,
   so we need to use a different type of corruption, 
   FAULT_SESSION_SCEP_CORRUPT_TRANSIDVALUE, to exercise the checksum test */

#define FAULTACTION_SESSION_CORRUPT_TRANSACTIONID_SCEP_1 \
		if( isServer( sessionInfoPtr ) ) \
			{ \
			BYTE buffer[ CRYPT_MAX_HASHSIZE ]; \
			int length; \
			\
			status = decodePKIUserValue( buffer, CRYPT_MAX_HASHSIZE, \
										 &length, userNamePtr->value, \
										 userNamePtr->valueLength ); \
			assert( cryptStatusOK( status ) ); \
			buffer[ 4 ]++; \
			status = encodePKIUserValue( userNamePtr->value, 64, \
										 ( int * ) &userNamePtr->valueLength, \
										 buffer, length, 3 ); \
			assert( cryptStatusOK( status ) ); \
			}
#define FAULTACTION_SESSION_SCEP_CORRUPT_TRANSIDVALUE_1 \
		protocolInfo->transID[ 4 ]++
#define FAULTACTION_SESSION_CORRUPT_NONCE_SCEP_1 \
		protocolInfo->nonce[ 4 ]++

/* Corrupt the user password sent to the server */

#define FAULTACTION_SESSION_CORRUPT_AUTHENTICATOR_SCEP_1 \
		( ( BYTE * ) attributeListPtr->value )[ 4 ]++
#define FAULTACTION_SESSION_CORRUPT_AUTHENTICATOR_SCEP_2 \
		( ( BYTE * ) attributeListPtr->value )[ 4 ]--

/* Corrypt the message type sent from the client to the server */

#define FAULTACTION_SESSION_SCEP_CORRUPT_MESSAGETYPE_1 \
		if( !strcmp( messageType, MESSAGETYPE_PKCSREQ ) ) \
			messageType = "99";

/****************************************************************************
*																			*
*					Fault-Injection Variables and Functions					*
*																			*
****************************************************************************/

/* The global value that records the fault type to inject.  In addition to
   this we may need to store parameter information relating to the fault,
   the faultParam values provide for this storage */

extern FAULT_TYPE faultType;
extern int faultParam1;

/* Get a substitute key to replace the actual one, used to check for 
   detection of use of the wrong key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getSubstituteKey( OUT_HANDLE_OPT CRYPT_CONTEXT *iPrivateKey );

/* Inject faults into a block of memory.  This is called on every call to 
   krnlSendMsg() to simulate SEUs, but is currently disabled */

#define INJECT_MEMORY_FAULTS		injectMemoryFaults

void injectMemoryFaults( void );

#undef INJECT_MEMORY_FAULTS
#define INJECT_MEMORY_FAULTS()

#else

/* Fault injection is disabled unless CONFIG_FAULTS is defined */

#define INJECT_MEMORY_FAULTS()
#define INJECT_FAULT( type, action )

#endif /* CONFIG_FAULTS */

/****************************************************************************
*																			*
*								Fuzzing Handling							*
*																			*
****************************************************************************/

#ifdef CONFIG_FUZZ

/* Bail out from a block of code when we're fuzzing, needed because we can
   only get to a certain point with static data before we can't go any
   further.  The exit code is an error status rather than CRYPT_OK to make
   sure that we exit immediately, which ensures that we don't have to 
   propagate FUZZ_EXIT()s all the way to the top */

#define FUZZ_EXIT()			return( CRYPT_ERROR_NOTAVAIL )

/* Skip a block of code when we're fuzzing, needed because request/response
   protocols don't have a request to send out before reading back the 
   (static) response */

#define FUZZ_SKIP()			return( CRYPT_OK )

/* Sometimes we need to set up additional values that are normally handled
   by skipped code.  This is handled with the following macro */

#define FUZZ_SET( a, b )	( a ) = ( b )

/* If we're fuzzing then some asserts, which exist to draw attention to 
   unusual input data that needs human attention, shouldn't be triggered,
   in which case we no-op them out */

#define assert_nofuzz( x )

#else

#define FUZZ_EXIT()
#define FUZZ_SKIP()
#define FUZZ_SET( a, b )
#define assert_nofuzz	assert

#endif /* CONFIG_FUZZ */

/****************************************************************************
*																			*
*					Removes Fault Injection for non-Debug					*
*																			*
****************************************************************************/

#else

#ifndef CONFIG_FAULTS
  #define INJECT_FAULT( type, action )
  #define INJECT_MEMORY_FAULTS()
#endif /* CONFIG_FAULTS */

#ifndef CONFIG_FUZZ
  #define FUZZ_EXIT()
  #define FUZZ_SKIP()
  #define FUZZ_SET( a, b )
  #define assert_nofuzz	assert
#endif /* CONFIG_FUZZ */

#endif /* !NDEBUG */
#endif /* _FAULT_DEFINED */
