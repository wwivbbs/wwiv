/****************************************************************************
*																			*
*						Network Stream Transport Functions					*
*						Copyright Peter Gutmann 1993-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "stream_int.h"
#else
  #include "io/stream_int.h"
#endif /* Compiler-specific includes */

#ifdef USE_TCP

/* Network streams can work on multiple levels.  At the lowest level we have
   the raw network I/O layer, handled by calling setAccessMethodXXX(), which
   hooks up the transport-level I/O functions.  If there's a requirement to
   replace the built-in network I/O it can be done by replacing the
   functionality at this level.

   Layered on top of the transport-level I/O via setStreamLayerXXX() is an
   optional higher layer protocol such as HTTP which is added by calling
   the appropriate function to layer the higher-level protocol over the
   transport-level I/O.  Alternatively, we can use setStreamLayerDirect()
   to just pass the call straight down to the transport layer.

   In addition to these two layers the higher level read requires an extra
   buffering layer in order to avoid making repeated calls to the transport-
   level I/O function, which is a particular problem for HTTP which has to
   take input a character at a time.  To avoid this issue we use the
   bufferedRead layer which reads ahead as far as it can and then feeds the
   buffered result back to the caller as required.  We also need to use write
   buffering to avoid potential problems with interactions with some
   transport layers, details are given in the comment for the buffered write
   function.

   The layering looks as follows:

	--- httpRead ---+-- bufferedRead ---+--- tcpRead
		cmpRead							|
										+--- clibRead
										|
	------------------------------------+--- otherRead

	--- httpWrite --+-- bufferedWrite --+---- tcpWrite
		cmpWrite						|
										+---- clibWrite
										|
	------------------------------------+---- otherWrite */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the stream state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckFunction( const STREAM *stream )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;

	assert( isReadPtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_B( netStream != NULL );

	/* Make sure that it's an initialised network stream with valid 
	   parameters */
	if( stream->type != STREAM_TYPE_NETWORK || \
		stream->netStreamInfo == NULL )
		return( FALSE );
	if( netStream->timeout < 0 || netStream->timeout > 300 )
		return( FALSE );

	/* It's an unbuffered network stream, all buffer values must be zero */
	if( stream->buffer != NULL || stream->bufPos != 0 || \
		stream->bufSize != 0 || stream->bufEnd != 0 )
		return( FALSE );
	if( netStream->writeBuffer != NULL || netStream->writeBufSize != 0 || \
		netStream->writeBufEnd != 0 )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckBufferedFunction( const STREAM *stream )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;

	assert( isReadPtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_B( netStream != NULL );

	/* Make sure that it's an initialised network stream with valid 
	   parameters */
	if( stream->type != STREAM_TYPE_NETWORK || \
		stream->netStreamInfo == NULL )
		return( FALSE );
	if( netStream->timeout < 0 || netStream->timeout > 300 )
		return( FALSE );

	/* Make sure that the buffer position is within bounds:

								 bufSize
									|
			<------ buffer ------>	v
		+---------------------------+
		|						|	|
		+---------------------------+
				^				^
				|				|
			 bufPos			 bufEnd */
	if( stream->bufPos < 0 || stream->bufPos > stream->bufEnd || \
		stream->bufEnd < 0 || stream->bufEnd > stream->bufSize || \
		stream->bufSize <= 0 || stream->bufSize >= MAX_BUFFER_SIZE )
		return( FALSE );

	/* Network streams have a second buffer used for writes, make sure that 
	   the write buffer position is within bounds */
	if( netStream->writeBuffer == NULL || \
		netStream->writeBufSize <= 0 || \
		netStream->writeBufSize >= MAX_BUFFER_SIZE )
		return( FALSE );
	if( netStream->writeBufEnd < 0 || \
		netStream->writeBufEnd > netStream->writeBufSize )
		return( FALSE );

	return( TRUE );
	}

/****************************************************************************
*																			*
*					Transport-layer Direct Access Functions					*
*																			*
****************************************************************************/

/* Map the upper-layer I/O functions directly to the transport-layer
   equivalent.  This is used if we're performing raw I/O without any
   intermediate protocol layers or buffering */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int transportDirectReadFunction( INOUT STREAM *stream, 
										OUT_BUFFER( maxLength, *length ) \
											void *buffer, 
										IN_DATALENGTH const int maxLength, 
										OUT_DATALENGTH_Z int *length )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL );
	REQUIRES_S( netStream->sanityCheckFunction( stream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );

	return( netStream->transportReadFunction( stream, buffer, maxLength, 
											  length,
											  TRANSPORT_FLAG_NONE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int transportDirectWriteFunction( INOUT STREAM *stream, 
										 IN_BUFFER( maxLength ) const void *buffer, 
										 IN_DATALENGTH const int maxLength,
										 OUT_DATALENGTH_Z int *length )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL );
	REQUIRES_S( netStream->sanityCheckFunction( stream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );

	return( netStream->transportWriteFunction( stream, buffer, maxLength, 
											   length,
											   TRANSPORT_FLAG_NONE ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerDirect( INOUT NET_STREAM_INFO *netStream )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	netStream->writeFunction = transportDirectWriteFunction;
	netStream->readFunction = transportDirectReadFunction;
	}

/****************************************************************************
*																			*
*					Transport-layer Session Access Functions				*
*																			*
****************************************************************************/

/* This facility is currently unused so we disable it to avoid inadvertent 
   use by users who try to play with undocumented features.  Note that when
   enabling it it'll be necessary to change the annotation for the connect
   function in stream.h since the parameters are currently marked as
   STDC_NONNULL_ARG */

#if 0

/* Send and receive data with a cryptlib session as the transport layer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int transportSessionConnectFunction( INOUT STREAM *stream,
											STDC_UNUSED const char *host, 
											STDC_UNUSED const int hostLen,
											STDC_UNUSED const int port )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;
	int isActive, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	
	REQUIRES_S( netStream != NULL );
	REQUIRES_S( netStream->sanityCheckFunction( stream ) );
	REQUIRES_S( host == NULL && hostLen == 0 );
	REQUIRES_S( port == 0 );

	/* If the transport session hasn't been activated yet, activate it now */
	status = krnlSendMessage( netStream->iTransportSession,
							  IMESSAGE_GETATTRIBUTE, &isActive,
							  CRYPT_SESSINFO_ACTIVE );
	if( cryptStatusOK( status ) && isActive )
		{
		/* The session has been activated, there's nothing to do */
		return( CRYPT_OK );
		}
	status = krnlSendMessage( netStream->iTransportSession,
							  IMESSAGE_SETATTRIBUTE, MESSAGE_VALUE_TRUE,
							  CRYPT_SESSINFO_ACTIVE );
	if( cryptStatusError( status ) )
		return( getSessionErrorInfo( stream, status ) );
	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void transportSessionDisconnectFunction( INOUT STREAM *stream,
												const BOOLEAN fullDisconnect )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_V( netStream != NULL );
	REQUIRES_V( netStream->sanityCheckFunction( stream ) );

	krnlSendNotifier( netStream->iTransportSession, IMESSAGE_DECREFCOUNT );
	}

CHECK_RETVAL_BOOL \
static BOOLEAN transportSessionOKFunction( void )
	{
	return( TRUE );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int transportSessionReadFunction( INOUT STREAM *stream, 
										 OUT_BUFFER( maxLength, *length ) \
											BYTE *buffer, 
										 IN_DATALENGTH const int maxLength, 
										 OUT_DATALENGTH_Z int *length, 
										 IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;
	MESSAGE_DATA msgData;
	int newTimeout = CRYPT_UNUSED, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL );
	REQUIRES_S( stream->sanityCheckFunction( stream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES_S( flags >= TRANSPORT_FLAG_NONE && \
				flags <= TRANSPORT_FLAG_MAX );

	/* Clear return value */
	*length = 0;

	/* Read data from the session, overriding the timeout handling if
	   requested */
	if( ( flags & TRANSPORT_FLAG_NONBLOCKING ) && netStream->timeout > 0 )
		newTimeout = 0;
	else
		{
		if( ( flags & TRANSPORT_FLAG_BLOCKING ) && netStream->timeout == 0 )
			newTimeout = 30;
		}
	if( newTimeout != CRYPT_UNUSED )
		{
		( void ) krnlSendMessage( netStream->iTransportSession, 
								  IMESSAGE_SETATTRIBUTE, &newTimeout, 
								  CRYPT_OPTION_NET_READTIMEOUT );
		}
	setMessageData( &msgData, buffer, maxLength );
	status = krnlSendMessage( netStream->iTransportSession, 
							  IMESSAGE_ENV_POPDATA, &msgData, 0 );
	if( newTimeout != CRYPT_UNUSED )
		{
		( void ) krnlSendMessage( netStream->iTransportSession, 
								  IMESSAGE_SETATTRIBUTE, &stream->timeout, 
								  CRYPT_OPTION_NET_READTIMEOUT );
		}
	if( cryptStatusError( status ) )
		return( getSessionErrorInfo( stream, status ) );
	if( msgData.length < maxLength )
		{
		retExt( CRYPT_ERROR_READ,
				( CRYPT_ERROR_READ, NETSTREAM_ERRINFO, 
				  "Only read %d out of %d bytes via cryptlib session "
				  "object", msgData.length, maxLength ) );
		}
	*length = maxLength;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int transportSessionWriteFunction( INOUT STREAM *stream, 
										  IN_BUFFER( length ) const BYTE *buffer, 
										  IN_DATALENGTH const int maxLength, 
										  OUT_DATALENGTH_Z int *length,
										  IN_FLAGS_Z( TRANSPORT ) \
											const int flags )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( buffer, maxLength ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL );
	REQUIRES_S( netStream->sanityCheckFunction( stream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES_S( flags >= TRANSPORT_FLAG_NONE && \
				flags <= TRANSPORT_FLAG_MAX );

	/* Clear return value */
	*length = 0;

	setMessageData( &msgData, ( MESSAGE_CAST ) buffer, maxLength );
	status = krnlSendMessage( netStream->iTransportSession,
							  IMESSAGE_ENV_PUSHDATA, &msgData, 0 );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( netStream->iTransportSession,
								  IMESSAGE_ENV_PUSHDATA, &msgData, 0 );
		}
	if( cryptStatusError( status ) )
		return( getSessionErrorInfo( stream, status ) );
	*length = maxLength;

	return( CRYPT_OK );
	}

void setAccessMethodTransportSession( INOUT STREAM *stream )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_V( netStream != NULL );
	REQUIRES_V( stream->type == STREAM_TYPE_NETWORK );

	netStream->transportConnectFunction = transportSessionConnectFunction;
	netStream->transportDisconnectFunction = transportSessionDisconnectFunction;
	netStream->transportWriteFunction = transportSessionWriteFunction;
	netStream->transportReadFunction = transportSessionReadFunction;
	netStream->transportOKFunction = transportSessionOKFunction;
	}
#endif /* 0 */

/****************************************************************************
*																			*
*								Buffering Functions							*
*																			*
****************************************************************************/

/* Buffered transport-layer read function.  This sits on top of the
   transport-layer read function and performs speculative read-ahead
   buffering to improve performance in protocols such as HTTP that have to
   read a byte at a time in places:

		   bPos		   bEnd
			|			|
			v			v
	+-------+-----------+-------+
	|		|///////////|		|
	+-------+-----------+-------+
			 -- Read -->

   We fill the buffer to bEnd and then empty it by advancing bPos until 
   there isn't enough data left to satisfy the read, whereupon we move the 
   data down and refill from bEnd:

   bPos		   bEnd
	|			|
	v			v
	+-----------+---------------+
	|///////////|				|
	+-----------+---------------+
				 -- Write --> */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int bufferedTransportReadFunction( INOUT STREAM *stream, 
										  OUT_BUFFER( maxLength, *length ) \
											BYTE *buffer, 
										  IN_DATALENGTH const int maxLength, 
										  OUT_DATALENGTH_Z int *length, 
										  IN_FLAGS_Z( TRANSPORT ) \
											const int flags )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;
	const int bytesLeft = stream->bufEnd - stream->bufPos;
	int bufferBytesRead, bytesRead, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL );
	REQUIRES_S( netStream->sanityCheckFunction( stream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES_S( bytesLeft >= 0 && bytesLeft < MAX_INTLENGTH_SHORT );
	REQUIRES_S( flags >= TRANSPORT_FLAG_NONE && \
				flags <= TRANSPORT_FLAG_MAX );

	/* Clear return value */
	*length = 0;

	/* If there's enough data in the buffer to satisfy the request, return it
	   directly */
	if( maxLength <= bytesLeft )
		{
		if( maxLength == 1 )
			{
			/* Optimisation for char-at-a-time HTTP header reads */
			*buffer = stream->buffer[ stream->bufPos++ ];
			}
		else
			{
			memcpy( buffer, stream->buffer + stream->bufPos, maxLength );
			stream->bufPos += maxLength;
			}
		*length = maxLength;

		ENSURES_S( netStream->sanityCheckFunction( stream ) );

		return( CRYPT_OK );
		}

	/* We're about to refill the buffer, if there's a gap at the start move
	   everything down to make room for the new data */
	if( stream->bufPos > 0 )
		{
		if( bytesLeft > 0 )
			{
			REQUIRES_S( rangeCheck( stream->bufPos, bytesLeft,
								    stream->bufEnd ) );
			memmove( stream->buffer, stream->buffer + stream->bufPos,
					 bytesLeft );
			}
		stream->bufEnd = bytesLeft;
		stream->bufPos = 0;
		}

	ENSURES_S( stream->bufPos == 0 );
	ENSURES_S( maxLength > bytesLeft );

	/* If there's more room in the buffer, refill it */
	if( stream->bufEnd < stream->bufSize )
		{
		int bytesToRead;

		/* Calculate how many bytes we still need to read from the network into 
		   the buffer and how much room there is in it.  If the read count is 
		   less than the available buffer space we only read that much, any 
		   further space will be filled (if possible) by the opportunistic 
		   read that follows */
		bytesToRead = stream->bufSize - stream->bufEnd;
		if( bytesToRead > maxLength )
			bytesToRead = maxLength;

		/* Perform an explicitly blocking read of as many bytes as we can/are
		   asked for.  Since there may be data already present from an
		   earlier speculative read we only read as much as we actually need 
		   in order to fulfill the request */
		status = netStream->transportReadFunction( stream,
										stream->buffer + stream->bufEnd, 
										bytesToRead, &bytesRead, 
										TRANSPORT_FLAG_BLOCKING );
		if( cryptStatusError( status ) )
			return( status );
		stream->bufEnd += bytesRead;

		/* If there's room for more, perform an opportunistic nonblocking 
		   read for whatever might still be there.  An error at this point 
		   isn't fatal since this was only a speculative read  */
		if( stream->bufEnd < stream->bufSize )
			{
			status = netStream->transportReadFunction( stream,
										stream->buffer + stream->bufEnd,
										stream->bufSize - stream->bufEnd,
										&bytesRead, TRANSPORT_FLAG_NONBLOCKING );
			if( cryptStatusOK( status ) )
				stream->bufEnd += bytesRead;
			}
		}
	ENSURES_S( netStream->sanityCheckFunction( stream ) );

	/* Read as much as we can from the buffer */
	bufferBytesRead = min( maxLength, stream->bufEnd );
	memcpy( buffer, stream->buffer, bufferBytesRead );
	stream->bufPos = bufferBytesRead;
	*length = bufferBytesRead;

	/* If we could satisfy the entire read from the buffer, we're done */
	if( maxLength <= bufferBytesRead )	/* Actually length == bufferBytesRead */
		{
		ENSURES_S( netStream->sanityCheckFunction( stream ) );

		return( CRYPT_OK );
		}

	/* We've drained the stream buffer and there's more to go, read the
	   remainder directly into the caller's buffer.  What to return in case
	   there's a failure at this point is a bit tricky since we can 
	   successfully return some data from the internal buffer but then fail 
	   when we try and replenish the buffer from the network.  For now we 
	   simply force the operation to be atomic since we're reading PKI 
	   datagrams that have to be read in their entirety */
	status = netStream->transportReadFunction( stream,
					buffer + bufferBytesRead, maxLength - bufferBytesRead,
					&bytesRead, TRANSPORT_FLAG_BLOCKING );
	if( cryptStatusError( status ) )
		return( status );
	*length += bytesRead;

	ENSURES_S( netStream->sanityCheckFunction( stream ) );

	return( CRYPT_OK );
	}

/* Buffered transport-layer write function.  This sits on top of the
   transport-layer write function and combines two (or more, although in
   practice only two ever occur) writes into a single write.  The reason for
   this is that when using TCP transport the delayed-ACK handling means
   that performing two writes followed by a read (typical for HTTP and CMP
   messages) leads to very poor performance, usually made even worse by TCP
   slow-start.

   The reason for this is that the TCP MSS is typically 1460 bytes on a LAN
   (Ethernet) or 512/536 bytes on a WAN while HTTP headers are ~200-300 
   bytes, far less than the MSS.  When an HTTP message is first sent the TCP 
   congestion window begins at one segment with the TCP slow-start then
   doubling its size for each ACK.  Sending the headers separately will send 
   one short segment and a second MSS-size segment whereupon the TCP stack 
   will wait for the responder's ACK before continuing.  The responder gets 
   both segments and then delays its ACK for 200ms in the hopes of 
   piggybacking it on responder data, which is never sent since it's still
   waiting for the rest of the HTTP body from the initiator.  This results 
   in a 200ms (+ assorted RTT) delay in each message sent.

   There's a somewhat related situation that occurs as a result of TCP slow-
   start and that can't be avoided programmatically in which we can't send 
   more than a single request initially, however most BSD-derived 
   implementations set the server's congestion window to two segments in
   response to receiving the TCP handshake ACK so for the initial message
   exchange the client can send a request of 1MSS and the server a response
   of 2MSS without running into congestion-control problems.

   A related problem is the fact that many TCP implementations will reset the
   congestion window after one retransmission timeout period if all data sent
   at that point has been acked, which means that both sides now restart with
   a congestion window of size 1.  Unfortunately there's nothing that can be
   done about this however hopefully at some point TCP implementations will
   start to fall into line with RFC 3390 and allow initial windows of ~4K,
   which will fix this particular problem.
   
   There are other, non-portable workarounds for this as well but they're so 
   non-portable that they often don't even work across different versions of 
   the same OS (e.g. different versions of the Linux kernel) let alone 
   variants of one OS type (e.g. OpenBSD vs. FreeBSD).  The least nonportable
   one is using writev() to combine a seperate header and body, which exists
   in most Unix versions and Win32.  Easier-to-use but almost totally non-
   portable are facilities like TCP_CORK (newer Linux kernels) and 
   TCP_NOPUSH (some *BSDs) which delay sending buffer contents until the 
   flag is reset again (so the use is "set TCP_CORK, write, write, write,
   reset TCP_CORK").  Because all of these are far more trouble than they're
   worth and in any case we're only sending very small data quantities via
   these functions (PKI messages) we just assemble the whole datagram 
   ourselves, which works across all OSes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int processIncompleteWrite( INOUT NET_STREAM_INFO *netStream, 
								   IN_DATALENGTH const int bytesWritten,
								   IN_DATALENGTH_Z const int newDataToWrite,
								   OUT_DATALENGTH_Z int *newDataWritten )
	{
	const int bytesLeftToWrite = netStream->writeBufEnd - bytesWritten;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( bytesWritten > 0 && bytesWritten < netStream->writeBufEnd && \
			  bytesWritten < MAX_BUFFER_SIZE );
	REQUIRES( newDataToWrite >= 0 && newDataToWrite < MAX_BUFFER_SIZE );
			  /* May be zero if the write buffer was already full */

	/* Clear return value */
	*newDataWritten = 0;

	/* Determine how much was written from what the user gave us.  This is
	   complicated by the fact that the write buffer may already contain 
	   buffered data from a previous write so we want to report to the 
	   caller only what was written from the new data that was supplied:

									|<-- newDataToWrite --->|
		|<---------------------- bufEnd ------------------->|
		+---------------------------+-----------------------+
		| Existing data in buffer	| New data copied in	|
		+---------------------------+-----------------------+
		|<-- bytesWritten --> ........ <-- bytesLeftToWr -->|
	
	   We can tell whether only existing data or newly-copied-in data was
	   written based on whether bytesLeftToWrite covers only the new data 
	   or whether it reaches back into the existing data in the buffer.  If
	   bytesLeftToWrite reaches back into the existing data then no new data
	   could be written */
	if( bytesLeftToWrite < newDataToWrite )
		*newDataWritten = newDataToWrite - bytesLeftToWrite;

	/* We couldn't write all of the data in the buffer, move what's left 
	   down to the start.  This shouldn't be needed since the caller will 
	   convert the failure to write the full amount into a write timeout but 
	   we do it anyway just to be neat */
	REQUIRES( rangeCheck( bytesWritten, bytesLeftToWrite,
						  netStream->writeBufEnd ) );
	memmove( netStream->writeBuffer, netStream->writeBuffer + bytesWritten,
			 bytesLeftToWrite );
	netStream->writeBufEnd = bytesLeftToWrite;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int bufferedTransportWriteFunction( INOUT STREAM *stream, 
										   IN_BUFFER( maxLength ) const BYTE *buffer, 
										   IN_DATALENGTH const int maxLength, 
										   OUT_DATALENGTH_Z int *length, 
										   IN_FLAGS_Z( TRANSPORT ) \
											const int flags )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;
	const BYTE *bufPtr = buffer;
	int byteCount = maxLength, bytesWritten, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( buffer, maxLength ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL );
	REQUIRES_S( netStream->sanityCheckFunction( stream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES_S( flags >= TRANSPORT_FLAG_NONE && \
				flags <= TRANSPORT_FLAG_MAX );

	/* Clear return value */
	*length = 0;

	/* If it's not a flush and the buffer can absorb the data, copy it in and
	   exit */
	if( !( flags & TRANSPORT_FLAG_FLUSH ) && \
		netStream->writeBufEnd + byteCount <= netStream->writeBufSize )
		{
		memcpy( netStream->writeBuffer + netStream->writeBufEnd, buffer, 
				byteCount );
		netStream->writeBufEnd += byteCount;
		*length = byteCount;

		ENSURES_S( netStream->sanityCheckFunction( stream ) );

		return( CRYPT_OK );
		}

	/* It's a flush or there's too much data to buffer, assemble a complete 
	   buffer and write it */
	if( netStream->writeBufEnd > 0 )
		{
		int bytesToCopy;

		/* Calculate how much data we can still add to the buffer.  If the write 
		   count is less than the available buffer size we only write that much */
		bytesToCopy = netStream->writeBufSize - netStream->writeBufEnd;
		if( bytesToCopy > byteCount )
			bytesToCopy = byteCount;
		if( bytesToCopy > 0 )
			{
			memcpy( netStream->writeBuffer + netStream->writeBufEnd, buffer,
					bytesToCopy );
			netStream->writeBufEnd += bytesToCopy;
			}
		status = netStream->transportWriteFunction( stream, 
							netStream->writeBuffer, netStream->writeBufEnd, 
							&bytesWritten, TRANSPORT_FLAG_NONE );
		if( cryptStatusError( status ) )
			return( status );
		if( bytesWritten < netStream->writeBufEnd )
			{
			status = processIncompleteWrite( netStream, bytesWritten, 
											 bytesToCopy, length );
			if( cryptStatusError( status ) )
				return( status );

			ENSURES_S( netStream->sanityCheckFunction( stream ) );

			return( CRYPT_OK );
			}
		netStream->writeBufEnd = 0;
		if( bytesToCopy > 0 ) 
			{
			bufPtr += bytesToCopy;
			byteCount -= bytesToCopy;
			if( byteCount <= 0 )
				{
				/* We've written everything, exit */
				*length = maxLength;

				ENSURES_S( netStream->sanityCheckFunction( stream ) );

				return( CRYPT_OK );
				}
			}
		}
	ENSURES( netStream->writeBufEnd == 0 );

	/* Write anything that's left directly */
	status = netStream->transportWriteFunction( stream, bufPtr, byteCount, 
												&bytesWritten, 
												TRANSPORT_FLAG_NONE );
	if( cryptStatusError( status ) )
		return( status );
	if( bytesWritten < byteCount )
		{
		/* Calculate how much remains to be written.  The overall amount 
		   written was the total amount to write minus what's left 
		   unwritten.  We don't have to update the stream buffer 
		   information this time because the write buffer has already been
		   emptied */
		byteCount -= bytesWritten;
		*length = maxLength - byteCount;
		}
	else
		{
		/* We managed to write everything */
		*length = maxLength;
		}

	ENSURES_S( netStream->sanityCheckFunction( stream ) );

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerBuffering( INOUT NET_STREAM_INFO *netStream,
							  const BOOLEAN useTransportBuffering )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	if( useTransportBuffering )
		{
		netStream->sanityCheckFunction = sanityCheckBufferedFunction;
		netStream->bufferedTransportReadFunction = bufferedTransportReadFunction;
		netStream->bufferedTransportWriteFunction = bufferedTransportWriteFunction;
		}
	else
		{
		netStream->sanityCheckFunction = sanityCheckFunction;
		netStream->bufferedTransportReadFunction = netStream->transportReadFunction;
		netStream->bufferedTransportWriteFunction = netStream->transportWriteFunction;
		}
	}
#endif /* USE_TCP */
