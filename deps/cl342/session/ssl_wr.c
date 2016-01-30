/****************************************************************************
*																			*
*					cryptlib SSL v3/TLS Session Write Routines				*
*					   Copyright Peter Gutmann 1998-2010					*
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
*							Sub-packet Management Routines					*
*																			*
****************************************************************************/

/* Open and complete an SSL packet:

	 offset										packetEndOfs
		|											|
		v											v
		+---+---+---+----+--------------------------+
		|ID	|Ver|Len|(IV)|							|
		+---+---+---+----+--------------------------+

   An initial openXXX() starts a new packet at the start of a stream and 
   continueXXX() adds another packet after an existing one, or (for the
   xxxHSXXX() variants) adds a handshake sub-packet within an existing 
   packet.  The continueXXX() operations return the start offset of the new 
   packet within the stream, openXXX() always starts at the start of the SSL 
   send buffer so the start offset is an implied 0.  completeXXX() then goes 
   back to the given offset and deposits the appropriate length value in the 
   header that was written earlier.  So typical usage (with error checking
   omitted for clarity) would be:

	// Change-cipher-spec packet
	openPacketStreamSSL( CRYPT_USE_DEFAULT, SSL_MSG_CHANGE_CIPHER_SPEC );
	write( stream, ... );
	completePacketStreamSSL( stream, 0 );

	// Finished handshake sub-packet within a handshake packet
	continuePacketStreamSSL( SSL_MSG_HANDSHAKE );
	offset = continueHSPacketStream( SSL_HAND_FINISHED );
	write( stream, ... );
	completeHSPacketStream( stream, offset );
	// (Packet stream is completed by wrapPacketSSL()) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int startPacketStream( INOUT STREAM *stream, 
							  const SESSION_INFO *sessionInfoPtr, 
							  IN_RANGE( SSL_HAND_FIRST, \
										SSL_HAND_LAST ) const int packetType )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( packetType >= SSL_MSG_FIRST && packetType <= SSL_MSG_LAST );

	/* Write the packet header:

		byte		ID = packetType
		byte[2]		version = { 0x03, 0x0n }
		uint16		len = 0 (placeholder) 
	  [ byte[]		iv	- TLS 1.1+ only ] */
	sputc( stream, packetType );
	sputc( stream, SSL_MAJOR_VERSION );
	sputc( stream, sessionInfoPtr->version );
	status = writeUint16( stream, 0 );		/* Placeholder */
	if( cryptStatusError( status ) )
		return( status );
	if( ( sessionInfoPtr->flags & SESSION_ISSECURE_WRITE ) && \
		sslInfo->ivSize > 0 )
		{
		MESSAGE_DATA msgData;
		BYTE iv[ CRYPT_MAX_IVSIZE + 8 ];

		setMessageData( &msgData, iv, sslInfo->ivSize );
		krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
						 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		status = swrite( stream, iv, sslInfo->ivSize );
		}
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int openPacketStreamSSL( OUT STREAM *stream, 
						 const SESSION_INFO *sessionInfoPtr, 
						 IN_LENGTH_OPT const int bufferSize, 
						 IN_RANGE( SSL_HAND_FIRST, \
								   SSL_HAND_LAST ) const int packetType )
	{
	const int streamSize = ( bufferSize == CRYPT_USE_DEFAULT ) ? \
						   sessionInfoPtr->sendBufSize - EXTRA_PACKET_SIZE : \
						   bufferSize + sessionInfoPtr->sendBufStartOfs;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) && \
			isWritePtr( sessionInfoPtr->sendBuffer, streamSize ) );

	REQUIRES( bufferSize == CRYPT_USE_DEFAULT || \
			  ( packetType == SSL_MSG_APPLICATION_DATA && \
			    bufferSize == 0 ) || \
			  ( bufferSize > 0 && bufferSize < MAX_INTLENGTH ) );
			  /* When wrapping up data packets we only write the implicit-
				 length header so the buffer size is zero */
	REQUIRES( packetType >= SSL_MSG_FIRST && packetType <= SSL_MSG_LAST );
	REQUIRES( streamSize >= sessionInfoPtr->sendBufStartOfs && \
			  streamSize <= sessionInfoPtr->sendBufSize - EXTRA_PACKET_SIZE );

	/* Create the stream */
	sMemOpen( stream, sessionInfoPtr->sendBuffer, streamSize );
	return( startPacketStream( stream, sessionInfoPtr, packetType ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int continuePacketStreamSSL( INOUT STREAM *stream, 
							 const SESSION_INFO *sessionInfoPtr, 
							 IN_RANGE( SSL_HAND_FIRST, \
									   SSL_HAND_LAST ) const int packetType,
							 OUT_LENGTH_SHORT_Z int *packetOffset )
	{
	const int offset = stell( stream );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( packetOffset, sizeof( int ) ) );
	
	REQUIRES( stell( stream ) >= SSL_HEADER_SIZE );
	REQUIRES( packetType >= SSL_MSG_FIRST && packetType <= SSL_MSG_LAST );

	/* Clear return value */
	*packetOffset = 0;

	/* Continue the stream */
	status = startPacketStream( stream, sessionInfoPtr, packetType );
	if( cryptStatusError( status ) )
		return( status );
	*packetOffset = offset;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int completePacketStreamSSL( INOUT STREAM *stream, 
							 IN_LENGTH_Z const int offset )
	{
	const int packetEndOffset = stell( stream );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( ( offset == 0 || offset >= SSL_HEADER_SIZE ) && \
			  offset <= packetEndOffset - ( ID_SIZE + VERSIONINFO_SIZE ) );

	/* Update the length field at the start of the packet */
	sseek( stream, offset + ID_SIZE + VERSIONINFO_SIZE );
	status = writeUint16( stream, ( packetEndOffset - offset ) - \
								  SSL_HEADER_SIZE );
	sseek( stream, packetEndOffset );

	return( status );
	}

/* Start and complete a handshake packet within an SSL packet.  Since this
   continues an existing packet stream that's been opened using 
   openPacketStreamSSL(), it's denoted as continueXXX() rather than 
   openXXX() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int continueHSPacketStream( INOUT STREAM *stream, 
							IN_RANGE( SSL_HAND_FIRST, \
									  SSL_HAND_LAST ) const int packetType,
							OUT_LENGTH_SHORT_Z int *packetOffset )
	{
	const int offset = stell( stream );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( packetOffset, sizeof( int ) ) );

	REQUIRES( packetType >= SSL_HAND_FIRST && packetType <= SSL_HAND_LAST );

	/* Clear return value */
	*packetOffset = 0;

	/* Write the handshake packet header:

		byte		ID = packetType
		uint24		len = 0 (placeholder) */
	sputc( stream, packetType );
	status = writeUint24( stream, 0 );
	if( cryptStatusError( status ) )
		return( status );
	*packetOffset = offset;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int completeHSPacketStream( INOUT STREAM *stream, 
							IN_LENGTH const int offset )
	{
	const int packetEndOffset = stell( stream );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( offset >= SSL_HEADER_SIZE && \
			  offset <= packetEndOffset - ( ID_SIZE + LENGTH_SIZE ) );
			  /* HELLO_DONE has size zero so 
			     offset == packetEndOffset - HDR_SIZE */

	/* Update the length field at the start of the packet */
	sseek( stream, offset + ID_SIZE );
	status = writeUint24( stream, packetEndOffset - \
								  ( offset + ID_SIZE + LENGTH_SIZE ) );
	sseek( stream, packetEndOffset );
	DEBUG_PRINT(( "Wrote %s (%d) handshake packet, length %ld.\n", \
				  getSSLHSPacketName( DEBUG_GET_STREAMBYTE( stream, offset ) ), 
				  DEBUG_GET_STREAMBYTE( stream, offset ),
				  ( packetEndOffset - offset ) - ( ID_SIZE + LENGTH_SIZE ) ));
	DEBUG_DUMP_STREAM( stream, offset + ( ID_SIZE + LENGTH_SIZE ), 
					   ( packetEndOffset - offset ) - ( ID_SIZE + LENGTH_SIZE ) );

	return( status );
	}

/****************************************************************************
*																			*
*							Write/Wrap a Packet								*
*																			*
****************************************************************************/

/* Wrap an SSL data packet:

	sendBuffer hdrPtr	dataPtr
		|		|			|-------------------			  MAC'd
		v		v			v================================ Encrypted
		+-------+-----+-----+-------------------+-----+-----+
		|///////| hdr | IV	|		data		| MAC | pad |
		+-------+-----+-----+-------------------+-----+-----+
				^<----+---->|<- payloadLength ->^			|
				|	  |		 <-------- bMaxLen -|---------->
			 offset sBufStartOfs		  stell( stream )

	sendBuffer hdrPtr	dataPtr
		|		|			|
		v		v			v============================== AuthEnc'd
		+-------+-----+-----+-----------------------+-----+
		|///////| hdr | IV	|		data			| ICV |
		+-------+-----+-----+-----------------------+-----+
				^<----+---->|<- payloadLength ----->^	  |
				|	  |		 <-------- bMaxLen -----|---->
			 offset sBufStartOfs			  stell( stream )

   This MACs/ICVs the data, adds the IV if necessary, pads and encrypts, and
   updates the header */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int wrapPacketSSL( INOUT SESSION_INFO *sessionInfoPtr, 
				   INOUT STREAM *stream, 
				   IN_LENGTH_Z const int offset )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	STREAM lengthStream;
	const int payloadLength = stell( stream ) - \
							  ( offset + sessionInfoPtr->sendBufStartOfs );
	int bufMaxLen = payloadLength + sMemDataLeft( stream );
	BYTE lengthBuffer[ UINT16_SIZE + 8 ];
	BYTE *dataPtr, *headerPtr;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sessionInfoPtr->flags & SESSION_ISSECURE_WRITE );
	REQUIRES( sStatusOK( stream ) );
	REQUIRES( offset >= 0 && \
			  offset <= stell( stream ) - \
							( payloadLength + \
							  sessionInfoPtr->sendBufStartOfs ) );
	REQUIRES( payloadLength >= 0 && payloadLength <= MAX_PACKET_SIZE && \
			  payloadLength < sessionInfoPtr->sendBufSize - \
							  ( sessionInfoPtr->sendBufStartOfs + \
								sslInfo->ivSize ) );

	/* Get pointers into the data stream for the crypto processing */
	status = sMemGetDataBlockAbs( stream, offset, ( void ** ) &headerPtr, 
								  SSL_HEADER_SIZE + sslInfo->ivSize + \
									bufMaxLen );
	if( cryptStatusError( status ) )
		return( status );
	dataPtr = headerPtr + SSL_HEADER_SIZE + sslInfo->ivSize;
	ENSURES( *headerPtr >= SSL_MSG_FIRST && *headerPtr <= SSL_MSG_LAST );

	/* MAC the payload if we're not using GCM, which combines the MAC with 
	   the encryption */
	if( !( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM ) )
		{
		if( sessionInfoPtr->version == SSL_MINOR_VERSION_SSL )
			status = createMacSSL( sessionInfoPtr, dataPtr, bufMaxLen, 
								   &length, payloadLength, *headerPtr );
		else
			status = createMacTLS( sessionInfoPtr, dataPtr, bufMaxLen, 
								   &length, payloadLength, *headerPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		/* GCM is a stream cipher so the length is the same as the payload 
		   length */
		length = payloadLength;
		}

	/* If it's TLS 1.1+ and we're using a block cipher, adjust for the 
	   explicit IV that precedes the data.  This is because the IV load is
	   handled implicitly by encrypting it as part of the data.  We know 
	   that the resulting values are within bounds because 
	   dataPtr = headerPtr + hdr + IV */
	if( sslInfo->ivSize > 0 && \
		!( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM ) )
		{
		REQUIRES( sessionInfoPtr->sendBufStartOfs >= SSL_HEADER_SIZE + \
													 sslInfo->ivSize ); 
		dataPtr -= sslInfo->ivSize;
		length += sslInfo->ivSize;
		bufMaxLen += sslInfo->ivSize;
		ENSURES( length > 0 && length <= bufMaxLen )
		}
	DEBUG_PRINT(( "Wrote %s (%d) packet, length %ld.\n", 
				  getSSLPacketName( *headerPtr ), *headerPtr, length ));
	DEBUG_DUMP_DATA( dataPtr, length );

	/* If we're using GCM then the IV has to be assembled from implicit and 
	   explicit components and set explicitly.  The reason why it's twelve 
	   bytes is because AES-GCM preferentially uses 96 bits of IV followed 
	   by 32 bits of 000...1, with other lengths being possible but then the
	   data has to be cryptographically reduced to 96 bits before 
	   processing, so TLS specifies a fixed length of 96 bits:

		|<--- 12 bytes ---->|
		+-------+-----------+
		| Salt	|	Nonce	|
		+-------+-----------+
		|<- 4 ->|<--- 8 --->| 

	   In addition we have to process the packet metadata that's normally
	   MACed as GCM AAD */
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM )
		{
		BYTE iv[ CRYPT_MAX_IVSIZE + 8 ];
		MESSAGE_DATA msgData;

		/* Set the GCM IV from a combination of the secret salt value and 
		   the explicitly-communicated IV */
		memcpy( iv, sslInfo->gcmWriteSalt, sslInfo->gcmSaltSize );
		memcpy( iv + sslInfo->gcmSaltSize, dataPtr - sslInfo->ivSize, 
				sslInfo->ivSize );
		setMessageData( &msgData, iv, GCM_IV_SIZE );
		status = krnlSendMessage( sessionInfoPtr->iCryptOutContext,
								  IMESSAGE_SETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_IV );
		if( cryptStatusError( status ) )
			return( status );

		/* Process the packet metadata as GCM AAD */
		status = macDataTLSGCM( sessionInfoPtr->iCryptOutContext, 
								sslInfo->writeSeqNo, sessionInfoPtr->version, 
								length, *headerPtr );
		if( cryptStatusError( status ) )
			return( status );
		sslInfo->writeSeqNo++;
		}

	/* Pad and encrypt the payload */
	status = encryptData( sessionInfoPtr, dataPtr, bufMaxLen, &length, 
						  length );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're using GCM then we have to adjust the length to account for 
	   the IV data at the start (for non-GCM modes this is handled 
	   implicitly by making the IV part of the data to encrypt) */
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM )
		length += sslInfo->ivSize;

	/* Insert the final packet payload length into the packet header.  We 
	   directly copy the data in because the stream may have been opened in 
	   read-only mode if we're using it to write pre-assembled packet data 
	   that's been passed in by the caller */
	sMemOpen( &lengthStream, lengthBuffer, UINT16_SIZE );
	status = writeUint16( &lengthStream, length );
	sMemDisconnect( &lengthStream );
	if( cryptStatusError( status ) )
		return( status );
	memcpy( headerPtr + ID_SIZE + VERSIONINFO_SIZE, lengthBuffer, 
			UINT16_SIZE );

	/* Sync the stream information to match the new payload size */
	return( sSkip( stream, length - ( sslInfo->ivSize + payloadLength ) ) );
	}

/* Wrap up and send an SSL packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendPacketSSL( INOUT SESSION_INFO *sessionInfoPtr, 
				   INOUT STREAM *stream, const BOOLEAN sendOnly )
	{
	const int length = stell( stream );
	void *dataPtr;
	int status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sStatusOK( stream ) );
	REQUIRES( stell( stream ) >= SSL_HEADER_SIZE );

	/* Update the length field at the start of the packet if necessary */
	if( !sendOnly )
		{
		status = completePacketStreamSSL( stream, 0 );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Send the packet to the peer */
	status = sMemGetDataBlockAbs( stream, 0, &dataPtr, length );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( dataPtr != NULL );
	status = swrite( &sessionInfoPtr->stream, dataPtr, length );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	return( CRYPT_OK );	/* swrite() returns a byte count */
	}

/****************************************************************************
*																			*
*								Send SSL Alerts								*
*																			*
****************************************************************************/

/* Send a close alert, with appropriate protection if necessary */

STDC_NONNULL_ARG( ( 1 ) ) \
static void sendAlert( INOUT SESSION_INFO *sessionInfoPtr, 
					   IN_RANGE( SSL_ALERTLEVEL_WARNING, \
								 SSL_ALERTLEVEL_FATAL ) const int alertLevel, 
					   IN_RANGE( SSL_ALERT_FIRST, \
								 SSL_ALERT_LAST ) const int alertType,
					   const BOOLEAN alertReceived )
	{
	STREAM stream;
	int length = DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	
	REQUIRES_V( alertLevel == SSL_ALERTLEVEL_WARNING || \
				alertLevel == SSL_ALERTLEVEL_FATAL );
	REQUIRES_V( alertType >= SSL_ALERT_FIRST && \
				alertType <= SSL_ALERT_LAST );

	/* Make sure that we only send a single alert.  Normally we do this 
	   automatically on shutdown, but we may have already sent it earlier 
	   as part of an error-handler */
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_ALERTSENT )
		return;
	sessionInfoPtr->protocolFlags |= SSL_PFLAG_ALERTSENT;

	/* Create the alert.  We can't really do much with errors at this point, 
	   although we can throw an exception in the debug version to draw 
	   attention to the fact that there's a problem.  The one error type 
	   that we don't complain about is an access permission problem, which 
	   can occur when cryptlib is shutting down, for example when the 
	   current thread is blocked waiting for network traffic and another 
	   thread shuts things down */
	status = openPacketStreamSSL( &stream, sessionInfoPtr, 
								  CRYPT_USE_DEFAULT, SSL_MSG_ALERT );
	if( cryptStatusOK( status ) )
		{
		sputc( &stream, alertLevel );
		status = sputc( &stream, alertType );
		}
	if( cryptStatusOK( status ) )
		{
		if( sessionInfoPtr->flags & SESSION_ISSECURE_WRITE )
			{
			status = wrapPacketSSL( sessionInfoPtr, &stream, 0 );
			assert( cryptStatusOK( status ) || \
					status == CRYPT_ERROR_PERMISSION );
			}
		else
			status = completePacketStreamSSL( &stream, 0 );
		if( cryptStatusOK( status ) )
			length = stell( &stream );
		sMemDisconnect( &stream );
		}
	/* Fall through with status passed on to the following code */

	/* Send the alert.  Note that we didn't exit on an error status in the
	   previous operation (for the reasons given in the comment earlier) 
	   since we can at least perform a clean shutdown even if the creation
	   of the close alert fails */
	if( cryptStatusOK( status ) )
		status = sendCloseNotification( sessionInfoPtr, 
										sessionInfoPtr->sendBuffer, length );
	else
		status = sendCloseNotification( sessionInfoPtr, NULL, 0 );
	if( cryptStatusError( status ) || alertReceived )
		return;

	/* Read back the other side's close alert acknowledgement.  Again, since 
	   we're closing down the session anyway there's not much that we can do 
	   in response to an error */
	( void ) readHSPacketSSL( sessionInfoPtr, NULL, &length, 
							  SSL_MSG_ALERT );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void sendCloseAlert( INOUT SESSION_INFO *sessionInfoPtr, 
					 const BOOLEAN alertReceived )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	sendAlert( sessionInfoPtr, SSL_ALERTLEVEL_WARNING, 
			   SSL_ALERT_CLOSE_NOTIFY, alertReceived );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void sendHandshakeFailAlert( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* We set the alertReceived flag to true when sending a handshake
	   failure alert to avoid waiting to get back an ack, since this 
	   alert type isn't acknowledged by the other side */
	sendAlert( sessionInfoPtr, SSL_ALERTLEVEL_FATAL, 
			   SSL_ALERT_HANDSHAKE_FAILURE, TRUE );
	}
#endif /* USE_SSL */
