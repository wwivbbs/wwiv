/****************************************************************************
*																			*
*					  cryptlib SSHv2 Session Write Routines					*
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
*							Sub-packet Management Routines					*
*																			*
****************************************************************************/

/* Unlike SSL, SSH only hashes portions of the handshake, and even then not
   complete packets but arbitrary bits and pieces.  In order to handle this
   we have to be able to break out bits and pieces of data from the stream
   buffer in order to hash them.  The following function extracts a block
   of data from a given position in the stream buffer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int streamBookmarkComplete( INOUT STREAM *stream, 
							OUT_OPT_PTR void **dataPtrPtr, 
							OUT_DATALENGTH_Z int *length, 
							IN_DATALENGTH const int position )
	{
	const int dataLength = stell( stream ) - position;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( dataPtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( position >= 0 && position < MAX_BUFFER_SIZE );
	REQUIRES( dataLength > 0 && dataLength <= stell( stream ) && \
			  dataLength < MAX_BUFFER_SIZE );

	/* Clear return values */
	*dataPtrPtr = NULL;
	*length = 0;

	*length = dataLength;
	return( sMemGetDataBlockAbs( stream, position, dataPtrPtr, dataLength ) );
	}

/* Open a stream to write an SSH2 packet or continue an existing stream to
   write further packets.  This opens the stream (if it's an open), skips
   the storage for the packet header, and writes the packet type */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int openPacketStreamSSH( OUT STREAM *stream, 
						 const SESSION_INFO *sessionInfoPtr,
						 IN_RANGE( SSH_MSG_DISCONNECT, \
								   SSH_MSG_CHANNEL_FAILURE ) 
							const int packetType )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	
	REQUIRES( packetType >= SSH_MSG_DISCONNECT && \
			  packetType <= SSH_MSG_CHANNEL_FAILURE );

	sMemOpen( stream, sessionInfoPtr->sendBuffer, 
			  sessionInfoPtr->sendBufSize - EXTRA_PACKET_SIZE );
	swrite( stream, "\x00\x00\x00\x00\x00", SSH2_HEADER_SIZE );
	return( sputc( stream, packetType ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int openPacketStreamSSHEx( OUT STREAM *stream, 
						   const SESSION_INFO *sessionInfoPtr,
						   IN_DATALENGTH const int bufferSize, 
						   IN_RANGE( SSH_MSG_DISCONNECT, \
									 SSH_MSG_CHANNEL_FAILURE ) 
							const int packetType )
	{
	const int streamSize = bufferSize + SSH2_HEADER_SIZE;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtrDynamic( sessionInfoPtr->sendBuffer, streamSize ) );
	
	REQUIRES( bufferSize > 0 && bufferSize < MAX_BUFFER_SIZE );
	REQUIRES( packetType >= SSH_MSG_DISCONNECT && \
			  packetType <= SSH_MSG_CHANNEL_FAILURE );
	REQUIRES( streamSize > SSH2_HEADER_SIZE && \
			  streamSize <= sessionInfoPtr->sendBufSize - EXTRA_PACKET_SIZE );

	sMemOpen( stream, sessionInfoPtr->sendBuffer, streamSize );
	swrite( stream, "\x00\x00\x00\x00\x00", SSH2_HEADER_SIZE );
	return( sputc( stream, packetType ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int continuePacketStreamSSH( INOUT STREAM *stream, 
							 IN_RANGE( SSH_MSG_DISCONNECT, \
									   SSH_MSG_CHANNEL_FAILURE ) \
								const int packetType,
							 OUT_DATALENGTH_Z int *packetOffset )
	{
	const int offset = stell( stream );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( packetOffset, sizeof( int ) ) );

	REQUIRES( packetType >= SSH_MSG_DISCONNECT && \
			  packetType <= SSH_MSG_CHANNEL_FAILURE );
	REQUIRES( stell( stream ) == 0 || \
			  ( stell( stream ) > SSH2_HEADER_SIZE + 1 && \
				stell( stream ) < MAX_BUFFER_SIZE ) );

	/* Clear return value */
	*packetOffset = 0;

	swrite( stream, "\x00\x00\x00\x00\x00", SSH2_HEADER_SIZE );
	status = sputc( stream, packetType );
	if( cryptStatusError( status ) )
		return( status );
	*packetOffset = offset;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Write/Wrap a Packet							*
*																			*
****************************************************************************/

/* Send an SSH packet.  During the handshake phase we may be sending 
   multiple packets at once, however unlike SSL, SSH requires that each
   packet in a multi-packet group be individually gift-wrapped so we have to
   provide a facility for separately wrapping and sending packets to handle
   this:

	sendBuffer	bStartPtr				  eLen (may be zero)
		|			|							  |
		v			v	|<-- payloadLen --->|	<-+->
		+-----------+---+-------------------+---+---+
		|///////////|hdr|		data		|pad|MAC|
		+-----------+---+-------------------+---+---+
					^						^	|
					|						|	|
				 offset					stell(s)|
					|<-------- length --------->| 

   Packets are usually sent encrypted, however during the handshake phase
   they're sent in plaintext.  We keep these two separate to ensure that 
   there's no chance of some glitch resulting in the accidental sending of 
   plaintext once we enter secure mode */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int wrapPlaintextPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
							 INOUT STREAM *stream,
							 IN_DATALENGTH_Z const int offset )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	STREAM metadataStream;
	BYTE padding[ 128 + 8 ];
	int length = stell( stream ) - offset;
	const int padLength = \
				getPaddedSize( length + SSH2_MIN_PADLENGTH_SIZE ) - length;
	const int payloadLength = length - SSH2_HEADER_SIZE;
	void *bufStartPtr;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( sStatusOK( stream ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );
	REQUIRES( offset >= 0 && offset < MAX_BUFFER_SIZE );
	REQUIRES( length >= SSH2_HEADER_SIZE && length < MAX_BUFFER_SIZE );
	REQUIRES( padLength >= SSH2_MIN_PADLENGTH_SIZE && padLength < 128 );
	REQUIRES( payloadLength >= 0 && payloadLength < length );
	REQUIRES( offset + length <= sessionInfoPtr->sendBufSize );

	/* Adjust the length by the padding, which is required even when there's 
	   no encryption being applied(?), although we set the padding to all 
	   zeroes in this case */
	length += padLength;

	/* Make sure that there's enough room for the padding */
	status = sMemGetDataBlockAbs( stream, offset, &bufStartPtr, length );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Not enough room for padding in data block" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_OVERFLOW );
		}

	/* Add the SSH packet header and all-zero padding:

		uint32		length
		byte		padLen
	  [	byte[]		data ]
		byte[]		padding */
	sMemOpen( &metadataStream, bufStartPtr, SSH2_HEADER_SIZE );
	writeUint32( &metadataStream, 1 + payloadLength + padLength );
	status = sputc( &metadataStream, padLength );
	sMemDisconnect( &metadataStream );
	ENSURES( cryptStatusOK( status ) );
	DEBUG_PRINT(( "Wrote %s (%d) packet, length %d.\n", 
				  getSSHPacketName( ( ( BYTE * ) bufStartPtr )[ LENGTH_SIZE + 1 ] ), 
				  ( ( BYTE * ) bufStartPtr )[ LENGTH_SIZE + 1 ],
				  length - ( LENGTH_SIZE + 1 + ID_SIZE + padLength ) ));
	DEBUG_DUMP_DATA( ( BYTE * ) bufStartPtr + LENGTH_SIZE + 1 + ID_SIZE, 
					 length - ( LENGTH_SIZE + 1 + ID_SIZE + padLength ) );
	memset( padding, 0, padLength );
	status = swrite( stream, padding, padLength );
	ENSURES( cryptStatusOK( status ) );

	sshInfo->writeSeqNo++;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int wrapPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT STREAM *stream,
					IN_DATALENGTH_Z const int offset, 
					const BOOLEAN useQuantisedPadding )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	STREAM metadataStream;
	MESSAGE_DATA msgData;
	BYTE padding[ 128 + 8 ];
	int length = stell( stream ) - offset;
	const int payloadLength = length - SSH2_HEADER_SIZE;
	void *bufStartPtr;
	const int extraLength = sessionInfoPtr->authBlocksize;
	int padLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( sStatusOK( stream ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );
	REQUIRES( offset >= 0 && offset < MAX_BUFFER_SIZE );
	REQUIRES( useQuantisedPadding == TRUE || useQuantisedPadding == FALSE );
	REQUIRES( length >= SSH2_HEADER_SIZE && length < MAX_BUFFER_SIZE );
	REQUIRES( payloadLength >= 0 && payloadLength < length );
	REQUIRES( offset + length + extraLength <= sessionInfoPtr->sendBufSize );

	/* Evaluate the number of padding bytes that we need to add to a packet
	   to make it a multiple of the cipher block size long, with a minimum
	   padding size of SSH2_MIN_PADLENGTH_SIZE bytes */
	if( useQuantisedPadding )
		{
		int LOOP_ITERATOR;

		/* It's something like a user-authentication packet that (probably) 
		   contains a password, adjust the padding to make the overall 
		   packet fixed-length to hide the password length information.
		   Note that we can't pad more than 255 bytes because the padding 
		   schemes has a single-byte pad length value, so we use 128 */
		LOOP_MED( padLength = 128,
				  ( length + SSH2_MIN_PADLENGTH_SIZE ) > padLength,
				  padLength += 128 );
		ENSURES( LOOP_BOUND_OK );
		padLength -= length;
		}
	else
		{
		const int paddedLength = getPaddedSize( length + SSH2_MIN_PADLENGTH_SIZE );
		
		ENSURES( paddedLength >= 16 && paddedLength <= MAX_BUFFER_SIZE ); 
		padLength = paddedLength - length;
		}
	ENSURES( padLength >= SSH2_MIN_PADLENGTH_SIZE && padLength < 256 );
	length += padLength;

	/* Make sure that there's enough room for the padding and MAC */
	status = sMemGetDataBlockAbs( stream, offset, &bufStartPtr, 
								  length + extraLength );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Not enough room for padding and MAC in data block" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_OVERFLOW );
		}

	/* Add the SSH packet header, padding, and MAC:

		uint32		length (excluding MAC size)
		byte		padLen
	  [	byte[]		data ]
		byte[]		padding
		byte[]		MAC */
	sMemOpen( &metadataStream, bufStartPtr, SSH2_HEADER_SIZE );
	writeUint32( &metadataStream, 1 + payloadLength + padLength );
	status = sputc( &metadataStream, padLength );
	sMemDisconnect( &metadataStream );
	ENSURES( cryptStatusOK( status ) );
	DEBUG_PRINT(( "Wrote %s (%d) packet, length %d.\n", 
				  getSSHPacketName( ( ( BYTE * ) bufStartPtr )[ LENGTH_SIZE + 1 ] ), 
				  ( ( BYTE * ) bufStartPtr )[ LENGTH_SIZE + 1 ],
				  length - ( LENGTH_SIZE + 1 + ID_SIZE + padLength ) ));
	DEBUG_DUMP_DATA( ( BYTE * ) bufStartPtr + LENGTH_SIZE + 1 + ID_SIZE, 
					 length - ( LENGTH_SIZE + 1 + ID_SIZE + padLength ) );

	/* Append the padding */
	setMessageData( &msgData, padding, padLength );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
					 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	sMemOpen( &metadataStream, 
			  ( BYTE * ) bufStartPtr + ( length - padLength ), 
			  padLength );
	status = swrite( &metadataStream, padding, padLength );
	sMemDisconnect( &metadataStream );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, padLength, MAX_INTLENGTH_SHORT );
	ENSURES( cryptStatusOK( status ) );

	/* MAC the data and append the MAC to the stream.  We skip the length 
	   value at the start since this is computed by the MAC'ing code */
	status = createMacSSH( sessionInfoPtr->iAuthOutContext,
						   sshInfo->writeSeqNo, 
						   ( BYTE * ) bufStartPtr + LENGTH_SIZE,
						   length + extraLength - LENGTH_SIZE, 
						   length - LENGTH_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	status = sSkip( stream, sessionInfoPtr->authBlocksize, 
					MAX_INTLENGTH_SHORT );
	ENSURES( cryptStatusOK( status ) );

	/* Encrypt the entire packet except for the MAC */
#ifdef USE_SSH_CTR
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSH_PFLAG_CTR ) )
		{
		status = ctrModeCrypt( sessionInfoPtr->iCryptOutContext,
							   sshInfo->writeCTR, 
							   sessionInfoPtr->cryptBlocksize,
							   bufStartPtr, length );
		}
	else
#endif /* USE_SSH_CTR */
	status = krnlSendMessage( sessionInfoPtr->iCryptOutContext,
							  IMESSAGE_CTX_ENCRYPT, bufStartPtr,
							  length );
	if( cryptStatusError( status ) )
		return( status );

	sshInfo->writeSeqNo++;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT STREAM *stream )
	{
	void *dataPtr;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* Send the contents of the stream to the peer */
	length = stell( stream );
	status = sMemGetDataBlockAbs( stream, 0, &dataPtr, length );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( dataPtr != NULL );
	status = swrite( &sessionInfoPtr->stream, dataPtr, length );
	if( cryptStatusError( status ) )
		{
		if( !TEST_FLAG( sessionInfoPtr->flags, 
						SESSION_FLAG_NOREPORTERROR ) )
			{
			sNetGetErrorInfo( &sessionInfoPtr->stream,
							  &sessionInfoPtr->errorInfo );
			}
		return( status );
		}
	DEBUG_DUMP_SSH( dataPtr, length, FALSE );

	return( CRYPT_OK );	/* swrite() returns a byte count */
	}

/* Convenience functions that combine the frequently-used pattern wrap + send
   into one */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int wrapSendPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
						INOUT STREAM *stream )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* Wrap up the payload in an SSH packet and send it */
	status = wrapPacketSSH2( sessionInfoPtr, stream, 0, FALSE );
	if( cryptStatusError( status ) )
		return( status );
	return( sendPacketSSH2( sessionInfoPtr, stream ) );
	}
#endif /* USE_SSH */
