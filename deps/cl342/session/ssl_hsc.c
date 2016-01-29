/****************************************************************************
*																			*
*			cryptlib SSL v3/TLS Handshake Completion Management				*
*					Copyright Peter Gutmann 1998-2012						*
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

/* Pre-encoded finished message header that we can use for message hashing:

	byte		ID = SSL_HAND_FINISHED
	uint24		len = 16 + 20 (SSL), 12 (TLS) */

#define FINISHED_TEMPLATE_SIZE				4

static const BYTE finishedTemplateSSL[] = \
		{ SSL_HAND_FINISHED, 0, 0, MD5MAC_SIZE + SHA1MAC_SIZE };
static const BYTE finishedTemplateTLS[] = \
		{ SSL_HAND_FINISHED, 0, 0, TLS_HASHEDMAC_SIZE };

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Destroy cloned hash contexts, used to clean up dual-hash (SSL, TLS 1.0-1.1)
   or single-hash (TLS 1.2+) contexts */

static void destroyHashContexts( IN_HANDLE_OPT const CRYPT_CONTEXT hashContext1,
								 IN_HANDLE_OPT const CRYPT_CONTEXT hashContext2,
								 IN_HANDLE_OPT const CRYPT_CONTEXT hashContext3 )
	{
	REQUIRES_V( ( isHandleRangeValid( hashContext1 ) && \
				  isHandleRangeValid( hashContext2 ) && \
				  hashContext3 == CRYPT_ERROR ) || \
				( hashContext1 == CRYPT_ERROR && \
				  hashContext2 == CRYPT_ERROR && \
				  isHandleRangeValid( hashContext3 ) ) );

	if( hashContext1 != CRYPT_ERROR )
		krnlSendNotifier( hashContext1, IMESSAGE_DECREFCOUNT );
	if( hashContext2 != CRYPT_ERROR )
		krnlSendNotifier( hashContext2, IMESSAGE_DECREFCOUNT );
	if( hashContext3 != CRYPT_ERROR )
		krnlSendNotifier( hashContext3, IMESSAGE_DECREFCOUNT );
	}

/* Add the current session information to the session cache */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int addSessionToCache( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							  IN_BUFFER( masterSecretSize ) void *masterSecret,
							  IN_LENGTH_SHORT const int masterSecretSize,
							  const BOOLEAN isClient )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	int cachedID, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( masterSecret, masterSecretSize ) );

	REQUIRES( masterSecretSize > 0 && \
			  masterSecretSize < MAX_INTLENGTH_SHORT );

	/* If we're the client then we have to add additional information to the
	   cache, in this case the server's name/address so that we can look up
	   the information if we try to reconnect later */
	if( isClient )
		{
		const ATTRIBUTE_LIST *attributeListPtr;

		attributeListPtr = findSessionInfo( sessionInfoPtr->attributeList,
											CRYPT_SESSINFO_SERVER_NAME );
		if( attributeListPtr == NULL )
			{
			/* If the connection was established by passing cryptlib a raw
			   network socket then there's no server name information 
			   present, so we can't cache anything based on this */
			return( CRYPT_OK );
			}
		status = cachedID = \
				addScoreboardEntryEx( sslInfo->scoreboardInfoPtr,
									  handshakeInfo->sessionID,
									  handshakeInfo->sessionIDlength,
									  attributeListPtr->value,
									  attributeListPtr->valueLength,
									  masterSecret, masterSecretSize );
		}
	else
		{
		BYTE sessionIDbuffer[ KEYID_SIZE + 8 ];
		const BYTE *sessionIDptr = handshakeInfo->sessionID;
		int sessionIDlength = handshakeInfo->sessionIDlength;

		/* We're the server, add the client's state information indexed by 
		   the sessionID */
		if( handshakeInfo->hashedSNIpresent )
			{
			/* If there's an SNI present, update the session ID to include 
			   it */
			status = convertSNISessionID( handshakeInfo, sessionIDbuffer, 
										  KEYID_SIZE );
			if( cryptStatusError( status ) )
				return( status );
			sessionIDptr = sessionIDbuffer;
			sessionIDlength = KEYID_SIZE;
			}
		status = cachedID = \
				addScoreboardEntry( sslInfo->scoreboardInfoPtr,
									sessionIDptr, sessionIDlength,
									masterSecret, masterSecretSize );
		}
	if( cryptStatusError( status ) )
		return( status );
	sslInfo->sessionCacheID = cachedID;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Read/Write Handshake Completion Messages				*
*																			*
****************************************************************************/

/* Read/write the handshake completion data (change cipherspec + finished) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readHandshakeCompletionData( INOUT SESSION_INFO *sessionInfoPtr,
										IN_BUFFER( hashValuesLength ) \
											const BYTE *hashValues,
										IN_LENGTH_SHORT const int hashValuesLength )
	{
	STREAM stream;
	BYTE macBuffer[ MD5MAC_SIZE + SHA1MAC_SIZE + 8 ];
	const int macValueLength = \
					( sessionInfoPtr->version <= SSL_MINOR_VERSION_SSL ) ? \
					MD5MAC_SIZE + SHA1MAC_SIZE : TLS_HASHEDMAC_SIZE;
	int length, value, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( hashValues, hashValuesLength ) );

	REQUIRES( hashValuesLength == macValueLength );

	/* Process the other side's change cipher spec:

		byte		type = SSL_MSG_CHANGE_CIPHER_SPEC
		byte[2]		version = { 0x03, 0x0n }
		uint16		len = 1
		byte		1 */
	status = readHSPacketSSL( sessionInfoPtr, NULL, &length,
							  SSL_MSG_CHANGE_CIPHER_SPEC );
	if( cryptStatusError( status ) )
		{
		/* If we don't get the change cipherspec at this point this may be
		   because the server asked us for client authentication but we 
		   skipped it because we don't have a certificate, in which case
		   we return extended error information indicating this */
		if( !isServer( sessionInfoPtr ) && \
			( sessionInfoPtr->protocolFlags & SSL_PFLAG_CLIAUTHSKIPPED ) )
			{
			retExtErrAlt( status, 
						  ( status, SESSION_ERRINFO, 
							", probably due to missing client "
							"authentication" ) );
			}
		return( status );
		}
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	value = sgetc( &stream );
	sMemDisconnect( &stream );
	if( value != 1 )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid change cipher spec packet payload, expected "
				  "0x01, got 0x%02X", value ) );
		}

	/* Change cipher spec was the last message not subject to security
	   encapsulation so we turn on security for the read channel after
	   seeing it.  In addition if we're using TLS 1.1+ explicit IVs the
	   effective header size changes because of the extra IV data, so we
	   record the size of the additional IV data and update the receive
	   buffer start offset to accomodate it */
	sessionInfoPtr->flags |= SESSION_ISSECURE_READ;
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS11 && \
		sessionInfoPtr->cryptBlocksize > 1 )
		{
		sessionInfoPtr->sessionSSL->ivSize = sessionInfoPtr->cryptBlocksize;
		sessionInfoPtr->receiveBufStartOfs += sessionInfoPtr->sessionSSL->ivSize;
		}
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM )
		{
		/* If we're using GCM then the IV is partially explicit and 
		   partially implicit, and unrelated to the cipher block size */
		sessionInfoPtr->sessionSSL->ivSize = \
					GCM_IV_SIZE - sessionInfoPtr->sessionSSL->gcmSaltSize;
		sessionInfoPtr->receiveBufStartOfs += sessionInfoPtr->sessionSSL->ivSize;
		}

	/* Process the other side's finished message.  Since this is the first 
	   chance that we have to test whether our crypto keys are set up 
	   correctly, we report problems with decryption or MACing or a failure 
	   to find any recognisable header as a wrong key rather than a bad data 
	   error.  In addition we signal the fact that the other side may 
	   respond unexpectedly because of the use of encryption to 
	   readHSPacketSSL() by specifying a special-case packet type, see the 
	   comment in readHSPacketSSL() for how this is handled and why it's 
	   necessary:

		byte		ID = SSL_HAND_FINISHED
		uint24		len
			SSLv3						TLS
		byte[16]	MD5 MAC			byte[12]	hashedMAC
		byte[20]	SHA-1 MAC */
	status = readHSPacketSSL( sessionInfoPtr, NULL, &length, 
							  SSL_MSG_FIRST_ENCRHANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	status = unwrapPacketSSL( sessionInfoPtr, sessionInfoPtr->receiveBuffer, 
							  length, &length, SSL_MSG_HANDSHAKE );
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ERROR_BADDATA || \
			status == CRYPT_ERROR_SIGNATURE )
			{
			retExtErr( CRYPT_ERROR_WRONGKEY,
					   ( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
						 SESSION_ERRINFO, 
						 "Decrypted data was corrupt, probably due to "
						 "incorrect encryption keys being negotiated "
						 "during the handshake: " ) );
			}
		return( status );
		}
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	status = checkHSPacketHeader( sessionInfoPtr, &stream, &length,
								  SSL_HAND_FINISHED, macValueLength );
	if( cryptStatusOK( status ) )
		{
		if( length != macValueLength )
			{
			/* A length mis-match can only be an overflow, since an
			   underflow would be caught by checkHSPacketHeader() */
			status = CRYPT_ERROR_OVERFLOW;
			}
		else
			status = sread( &stream, macBuffer, macValueLength );
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ERROR_BADDATA )
			{
			retExt( CRYPT_ERROR_WRONGKEY,
					( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
					  "Invalid handshake finished packet, probably due to "
					  "incorrect encryption keys being negotiated during "
					  "the handshake" ) );
			}
		return( status );
		}

	/* Make sure that the dual MAC/hashed MAC of all preceding messages is
	   valid */
	if( !compareDataConstTime( hashValues, macBuffer, macValueLength ) )
		{
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
				  "Bad MAC for handshake messages, handshake messages were "
				  "corrupted/modified" ) );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeHandshakeCompletionData( INOUT SESSION_INFO *sessionInfoPtr,
										 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
										 IN_BUFFER( hashValuesLength ) \
											const BYTE *hashValues, 
										 IN_LENGTH_SHORT const int hashValuesLength,
										 const BOOLEAN continuedStream )
	{
	STREAM *stream = &handshakeInfo->stream;
	int offset = 0, ccsEndPos, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isReadPtr( hashValues, hashValuesLength ) );

	REQUIRES( hashValuesLength > 0 && \
			  hashValuesLength < MAX_INTLENGTH_SHORT );

	/* Build the change cipher spec packet:

		byte		type = SSL_MSG_CHANGE_CIPHER_SPEC
		byte[2]		version = { 0x03, 0x0n }
		uint16		len = 1
		byte		1

	   Since change cipher spec is its own protocol, we use SSL-level packet
	   encoding rather than handshake protocol-level encoding */
	if( continuedStream )
		{
		status = continuePacketStreamSSL( stream, sessionInfoPtr,
										  SSL_MSG_CHANGE_CIPHER_SPEC, 
										  &offset );
		}
	else
		{
		status = openPacketStreamSSL( stream, sessionInfoPtr, 
									  CRYPT_USE_DEFAULT,
									  SSL_MSG_CHANGE_CIPHER_SPEC );
		}
	if( cryptStatusError( status ) )
		return( status );
	status = sputc( stream, 1 );
	if( cryptStatusOK( status ) )
		status = completePacketStreamSSL( stream, offset );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}

	/* Change cipher spec was the last message not subject to security
	   encapsulation so we turn on security for the write channel after
	   seeing it.  In addition if we're using TLS 1.1+ explicit IVs the
	   effective header size changes because of the extra IV data, so we
	   record the size of the additional IV data and update the receive
	   buffer start offset to accomodate it */
	sessionInfoPtr->flags |= SESSION_ISSECURE_WRITE;
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS11 && \
		sessionInfoPtr->cryptBlocksize > 1 )
		{
		sessionInfoPtr->sessionSSL->ivSize = sessionInfoPtr->cryptBlocksize;
		sessionInfoPtr->sendBufStartOfs += sessionInfoPtr->sessionSSL->ivSize;
		}
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM )
		{
		/* If we're using GCM then the IV is partially explicit and 
		   partially implicit, and unrelated to the cipher block size */
		sessionInfoPtr->sessionSSL->ivSize = \
					GCM_IV_SIZE - sessionInfoPtr->sessionSSL->gcmSaltSize;
		sessionInfoPtr->sendBufStartOfs += sessionInfoPtr->sessionSSL->ivSize;
		}

	/* Build the finished packet.  The initiator sends the MAC of the
	   contents of every handshake packet before the finished packet, the
	   responder sends the MAC of the contents of every packet before its own
	   finished packet but including the MAC of the initiator's packet
	   contents:

		byte		ID = SSL_HAND_FINISHED
		uint24		len
			SSLv3						TLS
		byte[16]	MD5 MAC			byte[12]	hashedMAC
		byte[20]	SHA-1 MAC */
	status = continuePacketStreamSSL( stream, sessionInfoPtr,
									  SSL_MSG_HANDSHAKE, &ccsEndPos );
	if( cryptStatusOK( status ) )
		status = continueHSPacketStream( stream, SSL_HAND_FINISHED, 
										 &offset );
	if( cryptStatusOK( status ) )
		status = swrite( stream, hashValues, hashValuesLength );
	if( cryptStatusOK( status ) )
		status = completeHSPacketStream( stream, offset );
	if( cryptStatusOK( status ) )
		status = wrapPacketSSL( sessionInfoPtr, stream, ccsEndPos );
	if( cryptStatusOK( status ) )
		status = sendPacketSSL( sessionInfoPtr, stream,
								TRUE );
	sMemDisconnect( stream );

	return( status );
	}

/****************************************************************************
*																			*
*						Complete the SSL/TLS Handshake						*
*																			*
****************************************************************************/

/* Complete the handshake with the client or server.  The logic gets a bit
   complex here because the roles of the client and server are reversed if
   we're resuming a session:

		Normal					Resumed
	Client		Server		Client		Server
	------		------		------		------
		   <--- ...			Hello  --->
	KeyEx  --->					   <---	Hello
	------------------------------------------ completeHandshakeSSL()
	CCS	   --->					   <--- CCS
	Fin	   --->					   <--- Fin
		   <---	CCS			CCS	   --->
		   <---	Fin			Fin	   --->

   Because of this the handshake-completion step treats the two sides as
   initiator and responder rather than client and server.  The overall flow
   is then:

	dualMAC/MAC( initiator );
	if( !initiator )
		read initiator CCS + Fin;
	dualMAC/MAC( responder );
	send initiator/responder CCS + Fin;
	if( initiator )
		read responder CCS + Fin; */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int completeHandshakeSSL( INOUT SESSION_INFO *sessionInfoPtr,
						  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						  const BOOLEAN isClient,
						  const BOOLEAN isResumedSession )
	{
	const CRYPT_CONTEXT initiatorMD5context = handshakeInfo->md5context;
	const CRYPT_CONTEXT initiatorSHA1context = handshakeInfo->sha1context;
	const CRYPT_CONTEXT initiatorSHA2context = handshakeInfo->sha2context;
	CRYPT_CONTEXT responderMD5context = CRYPT_ERROR;
	CRYPT_CONTEXT responderSHA1context = CRYPT_ERROR;
	CRYPT_CONTEXT responderSHA2context = CRYPT_ERROR;
	BYTE masterSecret[ SSL_SECRET_SIZE + 8 ];
	BYTE initiatorHashes[ ( CRYPT_MAX_HASHSIZE * 2 ) + 8 ];
	BYTE responderHashes[ ( CRYPT_MAX_HASHSIZE * 2 ) + 8 ];
#ifdef USE_SSL3
	const void *sslInitiatorString, *sslResponderString;
	int sslLabelLength;
#endif /* USE_SSL3 */
	const void *tlsInitiatorString, *tlsResponderString;
	const BOOLEAN isInitiator = isResumedSession ? !isClient : isClient;
	const BOOLEAN updateSessionCache = 	\
			( !isResumedSession && handshakeInfo->sessionIDlength > 0 ) ? \
			TRUE : FALSE;
	int initiatorHashLength, responderHashLength;
	int tlsLabelLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	REQUIRES( MAX_KEYBLOCK_SIZE >= ( sessionInfoPtr->authBlocksize + \
									 handshakeInfo->cryptKeysize +
									 sessionInfoPtr->cryptBlocksize ) * 2 );
	REQUIRES( handshakeInfo->authAlgo == CRYPT_ALGO_NONE || \
			  ( isEccAlgo( handshakeInfo->keyexAlgo ) && \
				handshakeInfo->premasterSecretSize >= MIN_PKCSIZE_ECC ) || \
			  ( !isEccAlgo( handshakeInfo->keyexAlgo ) && \
				handshakeInfo->premasterSecretSize >= SSL_SECRET_SIZE ) );

	/* Perform the necessary juggling of values for the reversed message
	   flow of resumed sessions */
	if( isResumedSession )
		{
		/* Resumed session, initiator = server, responder = client */
#ifdef USE_SSL3
		sslInitiatorString = SSL_SENDER_SERVERLABEL;
		sslResponderString = SSL_SENDER_CLIENTLABEL;
#endif /* USE_SSL3 */
		tlsInitiatorString = "server finished";
		tlsResponderString = "client finished";
		}
	else
		{
		/* Normal session, initiator = client, responder = server */
#ifdef USE_SSL3
		sslInitiatorString = SSL_SENDER_CLIENTLABEL;
		sslResponderString = SSL_SENDER_SERVERLABEL;
#endif /* USE_SSL3 */
		tlsInitiatorString = "client finished";
		tlsResponderString = "server finished";
		}
#ifdef USE_SSL3
	sslLabelLength = SSL_SENDERLABEL_SIZE;
#endif /* USE_SSL3 */
	tlsLabelLength = 15;

	/* Initialise and load cryptovariables into all encryption contexts */
	status = initCryptoSSL( sessionInfoPtr, handshakeInfo, masterSecret,
							SSL_SECRET_SIZE, isClient, isResumedSession );
	if( cryptStatusError( status ) )
		return( status );

	/* At this point the hashing of the initiator and responder diverge.
	   The initiator sends its change cipherspec and finished messages 
	   first, so the hashing stops there, while the responder has to keep 
	   hasing the initiator's messages until it's its turn to send its 
	   change cipherspec and finished messages.  To handle this we clone 
	   the initiator's hash context(s) so that we can contine the hashing 
	   after the initiator has wrapped things up */
#ifndef CONFIG_FUZZ
	if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
		{
		status = cloneHashContext( initiatorMD5context, 
								   &responderMD5context );
		if( cryptStatusOK( status ) )
			{
			status = cloneHashContext( initiatorSHA1context, 
									   &responderSHA1context );
			if( cryptStatusError( status ) )
				krnlSendNotifier( responderMD5context, IMESSAGE_DECREFCOUNT );
			}
		}
	else
		{
		status = cloneHashContext( initiatorSHA2context, 
								   &responderSHA2context );
		}
	if( cryptStatusError( status ) )
		{
		zeroise( masterSecret, SSL_SECRET_SIZE );
		return( status );
		}

	/* Complete the dual-MAC/MAC of the initiator-side messages and, if 
	   we're the responder, check that the MACs match the ones supplied by
	   the initiator */
#ifdef USE_SSL3
	if( sessionInfoPtr->version <= SSL_MINOR_VERSION_SSL )
		{
		status = completeSSLDualMAC( initiatorMD5context, initiatorSHA1context,
							initiatorHashes, CRYPT_MAX_HASHSIZE * 2,
							&initiatorHashLength, sslInitiatorString, 
							sslLabelLength, masterSecret, SSL_SECRET_SIZE );
		}
	else
#endif /* USE_SSL3 */
		{
		if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
			{
			status = completeTLSHashedMAC( initiatorMD5context, 
							initiatorSHA1context, initiatorHashes, 
							CRYPT_MAX_HASHSIZE * 2, &initiatorHashLength, 
							tlsInitiatorString, tlsLabelLength, masterSecret, 
							SSL_SECRET_SIZE );
			}
		else
			{
			status = completeTLS12HashedMAC( initiatorSHA2context,
							initiatorHashes, CRYPT_MAX_HASHSIZE,
							&initiatorHashLength, tlsInitiatorString, 
							tlsLabelLength, masterSecret, SSL_SECRET_SIZE );
			}
		}
#else
	initiatorHashLength = TLS_HASHEDMAC_SIZE;
	status = CRYPT_OK;
#endif /* CONFIG_FUZZ */
	if( cryptStatusOK( status ) && !isInitiator )
		{
		status = readHandshakeCompletionData( sessionInfoPtr, 
											  initiatorHashes, 
											  initiatorHashLength );
		}
	if( cryptStatusError( status ) )
		{
		zeroise( masterSecret, SSL_SECRET_SIZE );
		destroyHashContexts( responderMD5context, responderSHA1context,
							 responderSHA2context );
		return( status );
		}
#ifndef CONFIG_FUZZ

	/* Now that we have the initiator MACs, complete the dual-hashing/
	   hashing and dual-MAC/MAC of the responder-side messages and destroy 
	   the master secret unless we need to keep it around to update the
	   session cache.  We haven't created the full message yet at this 
	   point so we manually hash the individual pieces so that we can 
	   finally get rid of the master secret */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		status = krnlSendMessage( responderSHA2context, IMESSAGE_CTX_HASH,
				( MESSAGE_CAST ) finishedTemplateTLS, FINISHED_TEMPLATE_SIZE );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( responderSHA2context, IMESSAGE_CTX_HASH, 
									  initiatorHashes, initiatorHashLength );
		}
	else
		{
		const BYTE *finishedTemplate = \
			( sessionInfoPtr->version <= SSL_MINOR_VERSION_SSL ) ? \
			finishedTemplateSSL : finishedTemplateTLS;

		status = krnlSendMessage( responderMD5context, IMESSAGE_CTX_HASH,
				( MESSAGE_CAST ) finishedTemplate, FINISHED_TEMPLATE_SIZE );
		if( cryptStatusOK( status ) )
			{
			status = krnlSendMessage( responderSHA1context, IMESSAGE_CTX_HASH,
				( MESSAGE_CAST ) finishedTemplate, FINISHED_TEMPLATE_SIZE );
			}
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( responderMD5context, IMESSAGE_CTX_HASH, 
									  initiatorHashes, initiatorHashLength );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( responderSHA1context, IMESSAGE_CTX_HASH,
									  initiatorHashes, initiatorHashLength );
		}
	if( cryptStatusError( status ) )
		{
		zeroise( masterSecret, SSL_SECRET_SIZE );
		destroyHashContexts( responderMD5context, responderSHA1context,
							 responderSHA2context );
		return( status );
		}
#ifdef USE_SSL3
	if( sessionInfoPtr->version <= SSL_MINOR_VERSION_SSL )
		{
		status = completeSSLDualMAC( responderMD5context, responderSHA1context,
							responderHashes, CRYPT_MAX_HASHSIZE * 2,
							&responderHashLength, sslResponderString, 
							sslLabelLength, masterSecret, SSL_SECRET_SIZE );
		}
	else
#endif /* USE_SSL3 */
		{
		if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
			{
			status = completeTLSHashedMAC( responderMD5context, 
							responderSHA1context, responderHashes, 
							CRYPT_MAX_HASHSIZE * 2, &responderHashLength, 
							tlsResponderString, tlsLabelLength, masterSecret, 
							SSL_SECRET_SIZE );
			}
		else
			{
			status = completeTLS12HashedMAC( responderSHA2context, 
							responderHashes, CRYPT_MAX_HASHSIZE * 2, 
							&responderHashLength, tlsResponderString, 
							tlsLabelLength, masterSecret, SSL_SECRET_SIZE );
			}
		}
	if( !updateSessionCache )
		zeroise( masterSecret, SSL_SECRET_SIZE );
	destroyHashContexts( responderMD5context, responderSHA1context,
						 responderSHA2context );
	if( cryptStatusError( status ) )
		{
		zeroise( masterSecret, SSL_SECRET_SIZE );
		return( status );
		}

	/* Send our MACs to the other side and read back their response if
	   necessary.  The initiatorHashLength is the same as the 
	   responderHashLength (it's just a naming difference based on the
	   role that we're playing) so we use initiatorHashLength for both */
	status = writeHandshakeCompletionData( sessionInfoPtr, handshakeInfo,
										   isInitiator ? initiatorHashes : \
														 responderHashes,
										   initiatorHashLength,	
										   /* Same as responderHashLength */
										   ( isClient && !isResumedSession ) || \
										   ( !isClient && isResumedSession ) );
#endif /* CONFIG_FUZZ */
	if( cryptStatusOK( status ) && isInitiator )
		{
		status = readHandshakeCompletionData( sessionInfoPtr, responderHashes,
											  initiatorHashLength );
		}
	if( cryptStatusOK( status ) && updateSessionCache )
		{
		/* The handshake completed successfully, add the master secret to 
		   the session cache */
		status = addSessionToCache( sessionInfoPtr, handshakeInfo, 
									masterSecret, SSL_SECRET_SIZE,
									isClient );
		}
	zeroise( masterSecret, SSL_SECRET_SIZE );
	return( status );
	}
#endif /* USE_SSL */
