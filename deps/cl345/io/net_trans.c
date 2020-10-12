/****************************************************************************
*																			*
*						Network Stream Transport Functions					*
*						Copyright Peter Gutmann 1993-2017					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "stream_int.h"
#else
  #include "io/stream_int.h"
#endif /* Compiler-specific includes */

#ifdef USE_TCP

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
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
	STM_TRANSPORTREAD_FUNCTION transportReadFunction;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );

	/* Set up the function pointers.  We have to do this after the netStream
	   check otherwise we'd potentially be dereferencing a NULL pointer */
	transportReadFunction = ( STM_TRANSPORTREAD_FUNCTION ) \
							FNPTR_GET( netStream->transportReadFunction );
	REQUIRES_S( transportReadFunction != NULL );

	return( transportReadFunction( netStream, buffer, maxLength, length,
								   TRANSPORT_FLAG_NONE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int transportDirectWriteFunction( INOUT STREAM *stream, 
										 IN_BUFFER( maxLength ) const void *buffer, 
										 IN_DATALENGTH const int maxLength,
										 OUT_DATALENGTH_Z int *length )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
	STM_TRANSPORTWRITE_FUNCTION transportWriteFunction;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );

	/* Set up the function pointers.  We have to do this after the netStream
	   check otherwise we'd potentially be dereferencing a NULL pointer */
	transportWriteFunction = ( STM_TRANSPORTWRITE_FUNCTION ) \
							 FNPTR_GET( netStream->transportWriteFunction );
	REQUIRES_S( transportWriteFunction != NULL );

	return( transportWriteFunction( netStream, buffer, maxLength, length,
									TRANSPORT_FLAG_NONE ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerDirect( INOUT NET_STREAM_INFO *netStream )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	FNPTR_SET( netStream->writeFunction, transportDirectWriteFunction );
	FNPTR_SET( netStream->readFunction, transportDirectReadFunction );
	}

/****************************************************************************
*																			*
*					Transport-layer Virtual Stream Functions				*
*																			*
****************************************************************************/

/* Send and receive data with a virtual stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int transportVirtualConnectFunction( INOUT NET_STREAM_INFO *netStream, 
											STDC_UNUSED const char *host, 
											STDC_UNUSED const int hostNameLen,
											STDC_UNUSED const int port )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	
	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( ( host == NULL && hostNameLen == 0 ) || \
			  ( host != NULL && \
			    hostNameLen > 0 && hostNameLen <= MAX_DNS_SIZE ) );
	REQUIRES( port >= MIN_PORT_NUMBER && port < MAX_PORT_NUMBER );

	/* This is a virtual stream so there's nothing to connect to */
	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void transportVirtualDisconnectFunction( INOUT NET_STREAM_INFO *netStream, 
												const BOOLEAN fullDisconnect )

	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_V( fullDisconnect == TRUE || fullDisconnect == FALSE );
	REQUIRES_V( sanityCheckNetStream( netStream ) );

	/* This is a virtual stream so there's nothing to disconnect from */
	}

CHECK_RETVAL_BOOL \
static BOOLEAN transportVirtualOKFunction( void )
	{
	return( TRUE );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int transportVirtualCheckFunction( INOUT NET_STREAM_INFO *netStream )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int transportVirtualGetErrorInfo( INOUT NET_STREAM_INFO *netStream,
										 const void *virtualStateInfo,
										 IN_ERROR const int status )
	{
	STM_VIRTUALGETERRORINFO_FUNCTION virtualGetErrorInfoFunction;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtr( virtualStateInfo, sizeof( void * ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( cryptStatusError( status ) );

	virtualGetErrorInfoFunction = ( STM_VIRTUALGETERRORINFO_FUNCTION ) \
						FNPTR_GET( netStream->virtualGetErrorInfoFunction );
	ENSURES( virtualGetErrorInfoFunction != NULL );

	/* Copy the extended error information up from the transport session to
	   the virtual network stream */
	virtualGetErrorInfoFunction( virtualStateInfo, NETSTREAM_ERRINFO );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int transportVirtualReadFunction( INOUT NET_STREAM_INFO *netStream,
										 OUT_BUFFER( maxLength, *length ) BYTE *buffer,
										 IN_DATALENGTH const int maxLength,
										 OUT_DATALENGTH_Z int *length,
										 IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	STM_READ_FUNCTION virtualGetDataFunction;
	void *virtualStateInfo;
	int status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtrDynamic( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES( flags == TRANSPORT_FLAG_NONE || \
			  flags == TRANSPORT_FLAG_NONBLOCKING || \
			  flags == TRANSPORT_FLAG_BLOCKING );

	/* Clear return value */
	*length = 0;

	/* The buffered transport read functions will call the transport read 
	   functions with TRANSPORT_FLAG_NONBLOCKING set when they perform a
	   speculative read to fill the buffering system with any data that may
	   be available on the network.  In the case of a virtual stream the 
	   data is always available in the virtual stream, and an attempt to
	   peform a nonblocking read is meaningless, so we turn it into a no-op.

	   In practice it's a bit more complicated than this, the virtual 
	   stream is typically backed by some form of physical stream, but we 
	   can't tell what this is and so can't perform a speculative read on it 
	   since there will be unknown latencies involved in the translation 
	   from virtual to possible backing physical media.  In particular, 
	   unlike a network stream we can't say "read up to X bytes but don't
	   wait around if less than X are available", we have to read the exact 
	   number of requested bytes since speculatively trying to read extra
	   data may lead to indefinite delays in the physical layer.
	   
	   Since this is in effect disabling transport buffering, we could call 
	   directly from the higher-level read function, for example HTTP, 
	   through to the transport-level read function without going through 
	   the bufferedTransportRead(), however this would require yet another 
	   layer of indirection adding even further complexity to the already 
	   complex path of, in the case of HTTP over TLS:

		HTTP read -> buffered read -> virtual read -> session read -> transport read

	   In addition the HTTP read code currently hardcodes the calls to the 
	   buffered read functions, and bypassing that would require another 
	   level of function pointers */
	if( flags == TRANSPORT_FLAG_NONBLOCKING )
		return( CRYPT_OK );

	/* Set up the function pointers.  We have to do this after the netStream
	   check otherwise we'd potentially be dereferencing a NULL pointer */
	virtualGetDataFunction = ( STM_READ_FUNCTION ) \
							 FNPTR_GET( netStream->virtualGetDataFunction );
	ENSURES( virtualGetDataFunction != NULL );
	virtualStateInfo = ( void * ) DATAPTR_GET( netStream->virtualStateInfo );
	ENSURES( virtualStateInfo != NULL );

	status = virtualGetDataFunction( virtualStateInfo, buffer, maxLength, 
									 length );
	if( cryptStatusError( status ) )
		{
		return( transportVirtualGetErrorInfo( netStream, virtualStateInfo, 
											  status ) );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int transportVirtualWriteFunction( INOUT NET_STREAM_INFO *netStream,
										  IN_BUFFER( maxLength ) const BYTE *buffer,
										  IN_DATALENGTH const int maxLength,
										  OUT_DATALENGTH_Z int *length,
										  IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	STM_WRITE_FUNCTION virtualPutDataFunction;
	void *virtualStateInfo;
	int status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtrDynamic( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES( flags == TRANSPORT_FLAG_NONE || \
			  flags == TRANSPORT_FLAG_FLUSH );

	/* Clear return value */
	*length = 0;

	/* Set up the function pointers.  We have to do this after the netStream
	   check otherwise we'd potentially be dereferencing a NULL pointer */
	virtualPutDataFunction = ( STM_WRITE_FUNCTION ) \
							 FNPTR_GET( netStream->virtualPutDataFunction );
	REQUIRES( virtualPutDataFunction != NULL );
	virtualStateInfo = ( void * ) DATAPTR_GET( netStream->virtualStateInfo );
	ENSURES( virtualStateInfo != NULL );

	status = virtualPutDataFunction( virtualStateInfo, buffer, maxLength, 
									 length );
	if( cryptStatusOK( status ) && flags == TRANSPORT_FLAG_FLUSH )
		{
		int flushBytesCopied;

		status = virtualPutDataFunction( virtualStateInfo, NULL, 0, 
										 &flushBytesCopied );
		if( cryptStatusOK( status ) )
			*length += flushBytesCopied;
		}
	if( cryptStatusError( status ) )
		{
		return( transportVirtualGetErrorInfo( netStream, virtualStateInfo, 
											  status ) );
		}

	return( CRYPT_OK );
	}

void setAccessMethodTransportVirtual( INOUT NET_STREAM_INFO *netStream )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	FNPTR_SET( netStream->transportConnectFunction, 
			   transportVirtualConnectFunction );
	FNPTR_SET( netStream->transportDisconnectFunction, 
			   transportVirtualDisconnectFunction );
	FNPTR_SET( netStream->transportOKFunction, 
			   transportVirtualOKFunction );
	FNPTR_SET( netStream->transportCheckFunction, 
			   transportVirtualCheckFunction );
	FNPTR_SET( netStream->transportReadFunction, 
			   transportVirtualReadFunction );
	FNPTR_SET( netStream->transportWriteFunction, 
			   transportVirtualWriteFunction );
	}

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
int bufferedTransportRead( INOUT STREAM *stream, 
						   OUT_BUFFER( maxLength, *length ) BYTE *buffer, 
						   IN_DATALENGTH const int maxLength, 
						   OUT_DATALENGTH_Z int *length, 
						   IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
	STM_TRANSPORTREAD_FUNCTION transportReadFunction;
	const int bytesLeft = stream->bufEnd - stream->bufPos;
	int bufferBytesRead, bytesRead, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES_S( isShortIntegerRange( bytesLeft ) );
	REQUIRES_S( isFlagRangeZ( flags, TRANSPORT ) );

	/* Set up the function pointers.  We have to do this after the netStream
	   check otherwise we'd potentially be dereferencing a NULL pointer */
	transportReadFunction = ( STM_TRANSPORTREAD_FUNCTION ) \
							FNPTR_GET( netStream->transportReadFunction );
	REQUIRES_S( transportReadFunction != NULL );

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
			REQUIRES_S( boundsCheckZ( stream->bufPos, maxLength,
									  stream->bufEnd ) );
			memcpy( buffer, stream->buffer + stream->bufPos, maxLength );
			stream->bufPos += maxLength;
			}
		*length = maxLength;

		ENSURES_S( sanityCheckNetStream( netStream ) );

		return( CRYPT_OK );
		}

	/* We're about to refill the buffer, if there's a gap at the start move
	   everything down to make room for the new data */
	if( stream->bufPos > 0 )
		{
		if( bytesLeft > 0 )
			{
			REQUIRES_S( boundsCheck( stream->bufPos, bytesLeft,
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
		REQUIRES_S( boundsCheckZ( stream->bufEnd, bytesToRead, 
								  stream->bufSize ) );
		status = transportReadFunction( netStream, 
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
			status = transportReadFunction( netStream, 
											stream->buffer + stream->bufEnd,
											stream->bufSize - stream->bufEnd,
											&bytesRead, 
											TRANSPORT_FLAG_NONBLOCKING );
			if( cryptStatusOK( status ) )
				stream->bufEnd += bytesRead;
			}
		}
	ENSURES_S( sanityCheckNetStream( netStream ) );

	/* Read as much as we can from the buffer */
	bufferBytesRead = min( maxLength, stream->bufEnd );
	REQUIRES_S( rangeCheck( bufferBytesRead, 1, stream->bufEnd ) );
	memcpy( buffer, stream->buffer, bufferBytesRead );
	stream->bufPos = bufferBytesRead;
	*length = bufferBytesRead;

	/* If we could satisfy the entire read from the buffer, we're done */
	if( maxLength <= bufferBytesRead )	/* Actually length == bufferBytesRead */
		{
		ENSURES_S( sanityCheckNetStream( netStream ) );

		return( CRYPT_OK );
		}

	/* We've drained the stream buffer and there's more to go, read the
	   remainder directly into the caller's buffer.  What to return in case
	   there's a failure at this point is a bit tricky since we can 
	   successfully return some data from the internal buffer but then fail 
	   when we try and replenish the buffer from the network.  For now we 
	   simply force the operation to be atomic since we're reading datagrams 
	   that have to be read in their entirety */
	REQUIRES_S( boundsCheck( bufferBytesRead, maxLength - bufferBytesRead, 
							 maxLength ) );
	status = transportReadFunction( netStream, buffer + bufferBytesRead, 
									maxLength - bufferBytesRead, &bytesRead, 
									TRANSPORT_FLAG_BLOCKING );
	if( cryptStatusError( status ) )
		return( status );
	*length += bytesRead;

	ENSURES_S( sanityCheckNetStream( netStream ) );

	return( CRYPT_OK );
	}

/* Buffered transport-layer write function.  This sits on top of the
   transport-layer write function and combines two (or more, although in
   practice only two ever occur) writes into a single write.  The reason for
   this is that when using TCP transport the delayed-ACK handling means
   that performing two writes followed by a read (typical for HTTP messages) 
   leads to poor performance, usually made even worse by TCP slow-start.

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
   at that point has been ACKed, which means that both sides now restart with
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
   worth and in any case we're only sending small data quantities via these 
   functions we just assemble the whole datagram ourselves, which works 
   across all OSes */

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
	REQUIRES( boundsCheck( bytesWritten, bytesLeftToWrite,
						   netStream->writeBufEnd ) );
	memmove( netStream->writeBuffer, netStream->writeBuffer + bytesWritten,
			 bytesLeftToWrite );
	netStream->writeBufEnd = bytesLeftToWrite;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int bufferedTransportWrite( INOUT STREAM *stream, 
							IN_BUFFER( maxLength ) const BYTE *buffer, 
							IN_DATALENGTH const int maxLength, 
							OUT_DATALENGTH_Z int *length, 
							IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
	STM_TRANSPORTWRITE_FUNCTION transportWriteFunction;
	const BYTE *bufPtr = buffer;
	int byteCount = maxLength, bytesWritten, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( buffer, maxLength ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES_S( flags == TRANSPORT_FLAG_NONE || \
				flags == TRANSPORT_FLAG_FLUSH );

	/* Set up the function pointers.  We have to do this after the netStream
	   check otherwise we'd potentially be dereferencing a NULL pointer */
	transportWriteFunction = ( STM_TRANSPORTWRITE_FUNCTION ) \
							 FNPTR_GET( netStream->transportWriteFunction );
	REQUIRES_S( transportWriteFunction != NULL );

	/* Clear return value */
	*length = 0;

	/* If it's not a flush and the buffer can absorb the data, copy it in and
	   exit */
	if( !( flags & TRANSPORT_FLAG_FLUSH ) && \
		netStream->writeBufEnd + byteCount <= netStream->writeBufSize )
		{
		REQUIRES_S( boundsCheckZ( netStream->writeBufEnd, byteCount,
								  netStream->writeBufSize ) );
		memcpy( netStream->writeBuffer + netStream->writeBufEnd, buffer, 
				byteCount );
		netStream->writeBufEnd += byteCount;
		*length = byteCount;

		ENSURES_S( sanityCheckNetStream( netStream ) );

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
			REQUIRES_S( boundsCheck( netStream->writeBufEnd, bytesToCopy,
									 netStream->writeBufSize ) );
			memcpy( netStream->writeBuffer + netStream->writeBufEnd, buffer,
					bytesToCopy );
			netStream->writeBufEnd += bytesToCopy;
			}
		status = transportWriteFunction( netStream, netStream->writeBuffer, 
							netStream->writeBufEnd, &bytesWritten, 
							TRANSPORT_FLAG_FLUSH );
		if( cryptStatusError( status ) )
			return( status );
		if( bytesWritten < netStream->writeBufEnd )
			{
			status = processIncompleteWrite( netStream, bytesWritten, 
											 bytesToCopy, length );
			if( cryptStatusError( status ) )
				return( status );

			ENSURES_S( sanityCheckNetStream( netStream ) );

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

				ENSURES_S( sanityCheckNetStream( netStream ) );

				return( CRYPT_OK );
				}
			}
		}
	ENSURES_S( netStream->writeBufEnd == 0 );

	/* Write anything that's left directly */
	status = transportWriteFunction( netStream, bufPtr, byteCount, 
									 &bytesWritten, TRANSPORT_FLAG_FLUSH );
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

	ENSURES_S( sanityCheckNetStream( netStream ) );

	return( CRYPT_OK );
	}
#endif /* USE_TCP */
