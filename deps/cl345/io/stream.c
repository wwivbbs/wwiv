/****************************************************************************
*																			*
*							Stream I/O Functions							*
*						Copyright Peter Gutmann 1993-2015					*
*																			*
****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#if defined( INC_ALL )
  #include "stream_int.h"
#else
  #include "io/stream_int.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#ifndef CONFIG_CONSERVE_MEMORY_EXTRA

/* Sanity-check the stream state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckStream( const STREAM *stream )
	{
	assert( isReadPtr( stream, sizeof( STREAM ) ) );

	/* Check general stream metadata */
	if( !isEnumRange( stream->type, STREAM_TYPE ) || \
		!CHECK_FLAGS( stream->flags, STREAM_FLAG_NONE, 
					  STREAM_FLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckStream: General info" ));
		return( FALSE );
		}

	/* Perform stream type-specific checks */
	switch( stream->type )
		{
		case STREAM_TYPE_NULL:
			/* Check that the stream metadata is valid */
			if( GET_FLAGS( stream->flags, STREAM_FLAG_MAX ) != 0 )
				{
				DEBUG_PUTS(( "sanityCheckStream: Spurious null stream flags" ));
				return( FALSE );
				}

			/* Null streams, which act as data sinks, have a virtual 
			   content-length indicator so although the buffer size is zero 
			   the buffer position values can be nonzero to indicate how 
			   much (virtual) data they've absorbed */
			if( stream->buffer != NULL || stream->bufSize != 0 )
				{
				DEBUG_PUTS(( "sanityCheckStream: Spurious null stream buffer" ));
				return( FALSE );
				}
			if( stream->bufPos < 0 || stream->bufPos > stream->bufEnd || 
				stream->bufEnd < 0 || stream->bufEnd >= MAX_BUFFER_SIZE )
				{
				DEBUG_PUTS(( "sanityCheckStream: Null stream position" ));
				return( FALSE );
				}

			/* Null streams have no further requirements so we're done */
			return( TRUE );

		case STREAM_TYPE_MEMORY:
			/* Check that the stream metadata is valid */
			if( TEST_FLAG( stream->flags, STREAM_MFLAG_VFILE ) )
				{
				if( !TEST_FLAGS( stream->flags, 
								 ~( STREAM_FLAG_MASK | \
									STREAM_MFLAG_VFILE | \
									STREAM_FFLAG_MASK ), 0 ) )
					{
					DEBUG_PUTS(( "sanityCheckStream: Memory stream flags" ));
					return( FALSE );
					}
				}
			else
				{
				if( !TEST_FLAGS( stream->flags, ~STREAM_MFLAG_MASK, 0 ) )
					{
					DEBUG_PUTS(( "sanityCheckStream: Spurious memory stream flags" ));
					return( FALSE );
					}
				}
			break;

#ifdef USE_FILES
		case STREAM_TYPE_FILE:
			/* Check that the stream metadata is valid */
			if( !TEST_FLAGS( stream->flags, ~STREAM_FFLAG_MASK, 0 ) )
				{
				DEBUG_PUTS(( "sanityCheckStream: Spurious file stream flags" ));
				return( FALSE );
				}

			/* File streams have to be explicitly connected to a buffer 
			   after creation so if it's a partially-initialised file 
			   stream then we allow an absent buffer */
			if( !TEST_FLAG( stream->flags, STREAM_FFLAG_BUFFERSET ) )
				{
				if( stream->buffer != NULL || stream->bufPos != 0 || \
					stream->bufEnd != 0 || stream->bufSize != 0 )
					{
					DEBUG_PUTS(( "sanityCheckStream: Spurious file buffer" ));
					return( FALSE );
					}

				return( TRUE );
				}

			/* Make sure that the position within the file makes sense */
			if( stream->bufCount < 0 || \
				stream->bufCount >= ( MAX_BUFFER_SIZE / stream->bufSize ) )
				{
				DEBUG_PUTS(( "sanityCheckStream: File stream buffer position" ));
				return( FALSE );
				}

			break;
#endif /* USE_FILES */

#ifdef USE_TCP
		case STREAM_TYPE_NETWORK:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			/* Stream metadata is stored in the netStream structure, not the 
			   main stream so there's no explicit metadata check apart from
			   the check the the netStream pointer is valid */
			if( !DATAPTR_ISVALID( stream->netStream ) )
				{
				DEBUG_PUTS(( "sanityCheckStream: Net stream pointer" ));
				return( FALSE );
				}
			ENSURES( netStream != NULL );

			/* If it's an unbuffered network stream then all buffer values 
			   must be zero */
			if( stream->buffer == NULL )
				{
				if( stream->bufPos != 0 || stream->bufSize != 0 || \
					stream->bufEnd != 0 )
					{
					DEBUG_PUTS(( "sanityCheckStream: Spurious net stream buffer" ));
					return( FALSE );
					}
				}	

			/* Check the network stream information */
			if( !sanityCheckNetStream( netStream ) )
				{
				DEBUG_PUTS(( "sanityCheckStream: Net stream info" ));
				return( FALSE );
				}

			/* If it's an unbuffered stream, we're done */
			if( stream->buffer == NULL )
				return( TRUE );

			break;
			}
#endif /* USE_TCP */

		default:
			return( FALSE );
		}

	/* Everything else requires a buffer */
	if( stream->buffer == NULL )
		{
		DEBUG_PUTS(( "sanityCheckStream: Stream buffer" ));
		return( FALSE );
		}

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
		{
		DEBUG_PUTS(( "sanityCheckStream: Stream buffer info" ));
		return( FALSE );
		}

	/* Make sure that the buffer hasn't been corrupted.  This check doesn't
	   apply to memory streams which are overlaid onto an existing memory 
	   buffer for which we can't insert canaries */
	if( stream->type != STREAM_TYPE_MEMORY )
		{
		if( !safeBufferCheck( stream->buffer, stream->bufSize ) )
			{
			DEBUG_PUTS(( "sanityCheckStream: Buffer corruption" ));
			return( FALSE );
			}
		}

	return( TRUE );
	}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

#ifdef USE_FILES

/* Refill a stream buffer from backing storage */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int refillStream( INOUT STREAM *stream )
	{
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_FILE );
	REQUIRES_S( stream->bufPos >= stream->bufEnd || \
				TEST_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED ) );

	/* If we've reached EOF then we can't refill the stream */
	if( TEST_FLAG( stream->flags, STREAM_FFLAG_EOF ) )
		{
		/* If partial reads are allowed return an indication of how much 
		   data we got.  This only works once, after this the persistent 
		   error state will return an underflow error before we get to this
		   point */
		stream->status = CRYPT_ERROR_UNDERFLOW;
		return( TEST_FLAG( stream->flags, STREAM_FLAG_PARTIALREAD ) ? \
				OK_SPECIAL : CRYPT_ERROR_UNDERFLOW );
		}

	/* If we've moved to a different place in the file prepare to get new 
	   data into the buffer at the new location */
	if( TEST_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED ) && \
		!TEST_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED_NOSKIP ) )
		{
		status = fileSeek( stream, stream->bufCount * stream->bufSize );
		if( cryptStatusError( status ) )
			return( sSetError( stream, status ) );
		}

	/* Try and read more data into the stream buffer */
	status = fileRead( stream, stream->buffer, stream->bufSize, &length );
	if( cryptStatusError( status ) )
		return( sSetError( stream, status ) );
	if( length < stream->bufSize )
		{
		/* If we got less than we asked for, remember that we're at the end
		   of the file */
		SET_FLAG( stream->flags, STREAM_FFLAG_EOF );
		if( length == 0 )
			{
			/* We ran out of input on an exact buffer boundary, if partial 
			   reads are allowed return an indication of how much data we 
			   got.  This only works once, after this the persistent error 
			   state will return an underflow error before we get to this 
			   point */
			stream->status = CRYPT_ERROR_UNDERFLOW;
			return( TEST_FLAG( stream->flags, STREAM_FLAG_PARTIALREAD ) ? \
					OK_SPECIAL : CRYPT_ERROR_UNDERFLOW );
			}
		}

	/* We've refilled the stream buffer from the file, remember the 
	   details */
	if( !TEST_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED ) )
		stream->bufCount++;
	stream->bufEnd = length;
	stream->bufPos = 0;
	CLEAR_FLAGS( stream->flags, ( STREAM_FFLAG_POSCHANGED | \
								  STREAM_FFLAG_POSCHANGED_NOSKIP ) );

	ENSURES_S( sanityCheckStream( stream ) );

	return( CRYPT_OK );
	}

/* Empty a stream buffer to backing storage */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int emptyStream( INOUT STREAM *stream, const BOOLEAN forcedFlush )
	{
	int status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_FILE );
	REQUIRES_S( forcedFlush == TRUE || forcedFlush == FALSE );

	/* If the stream position has been changed, this can only have been from 
	   a rewind of the stream, in which case we move back to the start of 
	   the file */
	if( TEST_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED ) )
		{
		status = fileSeek( stream, 0 );
		if( cryptStatusError( status ) )
			return( sSetError( stream, status ) );
		}

	/* Try and write the data to the stream's backing storage */
	status = fileWrite( stream, stream->buffer, stream->bufPos );
	if( cryptStatusError( status ) )
		return( sSetError( stream, status ) );

	/* Reset the position-changed flag and, if we've written another buffer 
	   full of data, remember the details.  If it's a forced flush we leave
	   everything as is so that we remember the last write position in the 
	   file */
	CLEAR_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED );
	if( !forcedFlush )
		{
		stream->bufCount++;
		stream->bufPos = 0;
		}

	ENSURES_S( sanityCheckStream( stream ) );

	return( CRYPT_OK );
	}
#endif /* USE_FILES */

#ifdef VIRTUAL_FILE_STREAM 

/* Expand a virtual file stream's buffer to make room for new data when it
   fills up */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int expandVirtualFileStream( INOUT STREAM *stream, 
									IN_LENGTH const int length )
	{
	void *newBuffer;
	int newSize;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( sanityCheckStream( stream ) && \
				sIsVirtualFileStream( stream ) );
	REQUIRES_S( length > 0 && length < MAX_BUFFER_SIZE );

	/* If it's a small buffer allocated when we initially read a file and it 
	   doesn't look like we'll be overflowing a standard-size buffer, just 
	   expand it up to STREAM_VFILE_BUFSIZE */
	if( stream->bufSize < STREAM_VFILE_BUFSIZE && \
		stream->bufPos + length < STREAM_VFILE_BUFSIZE - 1024 )
		newSize = STREAM_VFILE_BUFSIZE;
	else
		{
		/* Increase the stream buffer size in STREAM_VFILE_BUFSIZE steps */
		newSize = stream->bufSize + STREAM_VFILE_BUFSIZE;
		}

	/* Allocate the buffer and copy the new data across using a safe realloc 
	   that wipes the original buffer.  If the malloc fails we return 
	   CRYPT_ERROR_OVERFLOW rather than CRYPT_ERROR_MEMORY since the former 
	   is more appropriate for the emulated-I/O environment */
	if( ( newBuffer = clDynAlloc( "expandVirtualFileStream", \
								  stream->bufSize + STREAM_VFILE_BUFSIZE ) ) == NULL )
		return( sSetError( stream, CRYPT_ERROR_OVERFLOW ) );
	memcpy( newBuffer, stream->buffer, stream->bufEnd );
	zeroise( stream->buffer, stream->bufEnd );
	clFree( "expandVirtualFileStream", stream->buffer );
	stream->buffer = newBuffer;
	stream->bufSize = newSize;

	ENSURES_S( sanityCheckStream( stream ) );

	return( CRYPT_OK );
	}
#endif /* VIRTUAL_FILE_STREAM */

/****************************************************************************
*																			*
*							Stream Read Functions							*
*																			*
****************************************************************************/

/* Read data from a stream */

CHECK_RETVAL_RANGE( 0, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int sgetc( INOUT STREAM *stream )
	{
	int ch;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( stream->buffer, stream->bufSize ) );
	
	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_MEMORY || \
				stream->type == STREAM_TYPE_FILE );

	/* If there's a problem with the stream don't try to do anything */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	switch( stream->type )
		{
		case STREAM_TYPE_MEMORY:
			/* Read the data from the stream buffer */
			if( stream->bufPos >= stream->bufEnd )
				return( sSetError( stream, CRYPT_ERROR_UNDERFLOW ) );
			ch = byteToInt( stream->buffer[ stream->bufPos++ ] );
			break;

#ifdef USE_FILES
		case STREAM_TYPE_FILE:
			REQUIRES_S( TEST_FLAG( stream->flags, 
								   STREAM_FFLAG_BUFFERSET ) );

			/* Read the data from the file */
			if( stream->bufPos >= stream->bufEnd || \
				TEST_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED ) )
				{
				int status = refillStream( stream );
				if( cryptStatusError( status ) )
					return( ( status == OK_SPECIAL ) ? 0 : status );
				}
			ch = byteToInt( stream->buffer[ stream->bufPos++ ] );
			break;
#endif /* USE_FILES */

		default:
			retIntError_Stream( stream );
		}

	ENSURES_S( sanityCheckStream( stream ) );

	return( ch );
	}

/* See the comment in stream.h for the use of CHECK_RETVAL rather than
   CHECK_RETVAL_LENGTH */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sread( INOUT STREAM *stream, 
		   OUT_BUFFER_FIXED( length ) void *buffer, 
		   IN_LENGTH const int length )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( stream->type == STREAM_TYPE_NETWORK || \
			isReadPtrDynamic( stream->buffer, stream->bufSize ) );
	assert( isWritePtrDynamic( buffer, length ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();
	if( !isWritePtrDynamic( buffer, length ) )
		retIntError_Stream( stream );

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_MEMORY || \
				stream->type == STREAM_TYPE_FILE || \
				stream->type == STREAM_TYPE_NETWORK );
	REQUIRES_S( length > 0 && length < MAX_BUFFER_SIZE );

	/* If there's a problem with the stream don't try to do anything */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	switch( stream->type )
		{
		case STREAM_TYPE_MEMORY:
			{
			int localLength = length;

#ifdef VIRTUAL_FILE_STREAM 
			/* If partial reads are allowed return whatever's left in the
			   stream buffer.  This only occurs for virtual file streams  
			   that have been translated into memory streams */
			if( TEST_FLAG( stream->flags, STREAM_FLAG_PARTIALREAD ) )
				{
				REQUIRES_S( sIsVirtualFileStream( stream ) );

				localLength = stream->bufEnd - stream->bufPos;
				if( localLength > length )
					localLength = length;
				}
#endif /* VIRTUAL_FILE_STREAM */
#if defined( USE_TCP ) && !defined( NDEBUG )
			if( sIsPseudoHTTPRawStream( stream ) )
				{
				const NET_STREAM_INFO *netStream = \
						DATAPTR_GET( stream->netStream );
				STM_READ_FUNCTION readFunction;
				int bytesRead;

				REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );

				/* Set up the function pointers.  We have to do this after 
				   the netStream check otherwise we'd potentially be 
				   dereferencing a NULL pointer */
				readFunction = ( STM_READ_FUNCTION ) \
							   FNPTR_GET( netStream->readFunction );
				REQUIRES_S( readFunction != NULL );

				status = readFunction( stream, buffer, length, &bytesRead );
				break;
				}
			if( sIsPseudoHTTPStream( stream ) )
				{
				HTTP_DATA_INFO *httpDataInfo = ( HTTP_DATA_INFO * ) buffer;

				REQUIRES_S( localLength == sizeof( HTTP_DATA_INFO ) );

				/* Pseudo-streams using HTTP transport have special 
				   requirements since the output buffer isn't a direct 
				   pointer to the buffer but an HTTP_DATA_INFO containing 
				   information on the HTTP stream, so we have to copy
				   information across to/from the HTTP_DATA_INFO */
				buffer = httpDataInfo->buffer;
				httpDataInfo->bytesAvail = stream->bufEnd;
				localLength = stream->bufEnd;
				}
#endif /* USE_TCP && !NDEBUG */

			/* Read the data from the stream buffer */
			if( stream->bufPos + localLength > stream->bufEnd )
				{
				memset( buffer, 0, min( 16, length ) );	/* Clear output buffer */
				return( sSetError( stream, CRYPT_ERROR_UNDERFLOW ) );
				}
			REQUIRES_S( boundsCheckZ( stream->bufPos, localLength, 
									  stream->bufEnd ) );
			memcpy( buffer, stream->buffer + stream->bufPos, localLength );
			stream->bufPos += localLength;

			/* Usually reads are atomic so we just return an all-OK 
			   indicator, however if we're performing partial reads we need
			   to return an exact byte count */
			status = TEST_FLAG( stream->flags, STREAM_FLAG_PARTIALREAD ) ? \
					 localLength : CRYPT_OK;
#ifndef CONFIG_CONSERVE_MEMORY_EXTRA
			if( sIsPseudoStream( stream ) )
				{
				/* Pseudo-streams are memory streams emulating other stream
				   types, so we need to return a byte count */
				status = localLength;
				}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */
			break;
			}

#ifdef USE_FILES
		case STREAM_TYPE_FILE:
			{
			BYTE *bufPtr = buffer;
			int dataLength, bytesCopied = 0, LOOP_ITERATOR;

			REQUIRES_S( TEST_FLAG( stream->flags, 
								   STREAM_FFLAG_BUFFERSET ) );

			/* Read the data from the file */
			LOOP_LARGE_INITCHECK( dataLength = length, dataLength > 0 )
				{
				const int oldDataLength = dataLength;
				int bytesToCopy;

				/* If the stream buffer is empty try and refill it */
				if( stream->bufPos >= stream->bufEnd || \
					TEST_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED ) )
					{
					status = refillStream( stream );
					if( cryptStatusError( status ) )
						{
						return( ( status == OK_SPECIAL ) ? \
								bytesCopied : status );
						}
					}

				/* Copy as much data as we can out of the stream buffer */
				bytesToCopy = min( dataLength, \
								   stream->bufEnd - stream->bufPos );
				REQUIRES_S( boundsCheckZ( stream->bufPos, bytesToCopy, 
										  stream->bufEnd ) );
				memcpy( bufPtr, stream->buffer + stream->bufPos, 
						bytesToCopy );
				stream->bufPos += bytesToCopy;
				bufPtr += bytesToCopy;
				bytesCopied += bytesToCopy;
				dataLength -= bytesToCopy;
				ENSURES_S( dataLength < oldDataLength );
				}
			ENSURES_S( LOOP_BOUND_OK );

			/* Usually reads are atomic so we just return an all-OK 
			   indicator, however if we're performing partial reads we need
			   to return an exact byte count */
			status = TEST_FLAG( stream->flags, STREAM_FLAG_PARTIALREAD ) ? \
					 bytesCopied : CRYPT_OK;
			break;
			}
#endif /* USE_FILES */

#ifdef USE_TCP
		case STREAM_TYPE_NETWORK:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
			STM_READ_FUNCTION readFunction;
			int bytesRead;

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( netStream->protocol != STREAM_PROTOCOL_HTTP || \
						( netStream->protocol == STREAM_PROTOCOL_HTTP && \
						  length == sizeof( HTTP_DATA_INFO ) ) );

			/* Set up the function pointers.  We have to do this after the 
			   netStream check otherwise we'd potentially be dereferencing a 
			   NULL pointer */
			readFunction = ( STM_READ_FUNCTION ) \
						   FNPTR_GET( netStream->readFunction );
			REQUIRES_S( readFunction != NULL );

			/* Read the data from the network.  Reads are normally atomic
			   but if the partial-write flag is set can be restarted after
			   a timeout */
			status = readFunction( stream, buffer, length, &bytesRead );
			if( cryptStatusError( status ) )
				{
				/* If the lower-level code has indicated that the error 
				   condition is fatal, make it persistent for the stream */
				if( cryptStatusError( netStream->persistentStatus ) )
					stream->status = netStream->persistentStatus;

				/* If it's not a special-case CRYPT_ERROR_COMPLETE status,
				   exit.  We don't make the error persistent since unlike
				   memory or file stream reads, most errors on network reads 
				   are recoverable */
				if( status != CRYPT_ERROR_COMPLETE )
					return( status );

				/* If we get a CRYPT_ERROR_COMPLETE status this means that
				   the other side has closed the connection.  This status is 
				   returned when there are intermediate protocol layers such 
				   as HTTP or tunnelling over a cryptlib session involved.
				   When this occurs we update the stream state and map the 
				   status to a standard read error.  The exact code to 
				   return here is a bit uncertain, it isn't specifically a 
				   read error because either the other side is allowed to 
				   close the connection after it's said its bit (and so it's 
				   not a read error), or it has to perform a 
				   cryptographically protected close (in which case any 
				   non-OK status indicates a problem).  The most sensible 
				   status is probably a read error */
				SET_FLAG( netStream->nFlags, STREAM_NFLAG_LASTMSGR );
				return( CRYPT_ERROR_READ );
				}
			if( !( TEST_FLAG( stream->flags, STREAM_FLAG_PARTIALREAD ) || \
				   TEST_FLAG( netStream->nFlags, STREAM_NFLAG_ENCAPS ) || \
				   TEST_FLAG( netStream->nFlags, STREAM_NFLAG_DGRAM ) ) && \
				bytesRead < length )
				{
				/* If we didn't read all of the data and partial reads 
				   aren't allowed report a read timeout.  Note that this 
				   code path is never taken for HTTP reads (with 
				   STREAM_NFLAG_ENCAPS set) since the length information is
				   conveyed in the HTTP layer */
				retExt( CRYPT_ERROR_TIMEOUT,
						( CRYPT_ERROR_TIMEOUT, NETSTREAM_ERRINFO, 
						  "Read timed out with %d of %d bytes read",
						  bytesRead, length ) );
				}

			/* This is an ugly case where we have to follow the Posix 
			   semantics of returning a read-bytes count as the return 
			   status rather than a by-reference parameter.  If we didn't
			   do this then every trivial memory-stream read would need to
			   pass in a dummy parameter for the read-byte-count value just
			   to handle the one or two calls to a network stream read that
			   needs to return a length */
			status = bytesRead;
			break;
			}
#endif /* USE_TCP */

		default:
			retIntError_Stream( stream );
		}

	ENSURES_S( sanityCheckStream( stream ) );

	return( status );
	}

/****************************************************************************
*																			*
*							Stream Write Functions							*
*																			*
****************************************************************************/

/* Write data to a stream */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sputc( INOUT STREAM *stream, IN_BYTE const int ch )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( stream->type == STREAM_TYPE_NULL || \
			isWritePtrDynamic( stream->buffer, stream->bufSize ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_NULL || \
				stream->type == STREAM_TYPE_MEMORY || \
				stream->type == STREAM_TYPE_FILE );
	REQUIRES_S( !TEST_FLAG( stream->flags, STREAM_FLAG_READONLY ) );
	REQUIRES( ch >= 0 && ch <= 0xFF );

	/* If there's a problem with the stream don't try to do anything until
	   the error is cleared */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	/* If this is a pseudo-stream then writes are discarded */
#ifndef CONFIG_CONSERVE_MEMORY_EXTRA
	if( sIsPseudoStream( stream ) )
		return( CRYPT_OK );
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

	switch( stream->type )
		{
		case STREAM_TYPE_NULL:
			/* It's a null stream, just record the write and return */
			stream->bufPos++;
			if( stream->bufEnd < stream->bufPos )
				stream->bufEnd = stream->bufPos;
			break;

		case STREAM_TYPE_MEMORY:
			/* Write the data to the stream buffer */
			if( stream->bufPos >= stream->bufSize )
				{
#ifdef VIRTUAL_FILE_STREAM 
				if( sIsVirtualFileStream( stream ) )
					{
					int status;

					status = expandVirtualFileStream( stream, 1 );
					if( cryptStatusError( status ) )
						return( status );
					}
				else
#endif /* VIRTUAL_FILE_STREAM */
					return( sSetError( stream, CRYPT_ERROR_OVERFLOW ) );
				}
			stream->buffer[ stream->bufPos++ ] = intToByte( ch );
			if( stream->bufEnd < stream->bufPos )
				stream->bufEnd = stream->bufPos;
#ifdef VIRTUAL_FILE_STREAM 
			if( sIsVirtualFileStream( stream ) )
				{
				/* This is a memory stream emulating a file stream, set the
				   dirty bit */
				SET_FLAG( stream->flags, STREAM_FLAG_DIRTY );
				}
#endif /* VIRTUAL_FILE_STREAM */
			break;

#ifdef USE_FILES
		case STREAM_TYPE_FILE:
			REQUIRES_S( TEST_FLAG( stream->flags, 
								   STREAM_FFLAG_BUFFERSET ) );

			/* Write the data to the file */
			if( stream->bufPos >= stream->bufSize )
				{
				int status;

				status = emptyStream( stream, FALSE );
				if( cryptStatusError( status ) )
					return( status );
				}
			stream->buffer[ stream->bufPos++ ] = intToByte( ch );
			SET_FLAG( stream->flags, STREAM_FLAG_DIRTY );
			break;
#endif /* USE_FILES */

		default:
			retIntError_Stream( stream );
		}

	ENSURES_S( sanityCheckStream( stream ) );

	return( CRYPT_OK );
	}

/* See the comment in stream.h for the use of RETVAL rather than 
   RETVAL_LENGTH */

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int swrite( INOUT STREAM *stream, 
		    IN_BUFFER( length ) const void *buffer, 
			IN_LENGTH const int length )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( stream->type == STREAM_TYPE_NULL || \
			stream->type == STREAM_TYPE_NETWORK || \
			isWritePtrDynamic( stream->buffer, stream->bufSize ) );
	assert( isReadPtrDynamic( buffer, length ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();
	if( !isReadPtrDynamic( buffer, length ) )
		retIntError_Stream( stream );

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_NULL || \
				stream->type == STREAM_TYPE_MEMORY || \
				stream->type == STREAM_TYPE_FILE || \
				stream->type == STREAM_TYPE_NETWORK );
	REQUIRES_S( length > 0 && length < MAX_BUFFER_SIZE );
	REQUIRES_S( !TEST_FLAG( stream->flags, STREAM_FLAG_READONLY ) );

	/* If there's a problem with the stream don't try to do anything until
	   the error is cleared */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	/* If this is a pseudo-stream then writes are discarded */
#ifndef CONFIG_CONSERVE_MEMORY_EXTRA
	if( sIsPseudoStream( stream ) )
		return( CRYPT_OK );
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

	switch( stream->type )
		{
		case STREAM_TYPE_NULL:
			/* It's a null stream, just record the write and return */
			stream->bufPos += length;
			if( stream->bufEnd < stream->bufPos )
				stream->bufEnd = stream->bufPos;
			status = CRYPT_OK;
			break;

		case STREAM_TYPE_MEMORY:
			/* Write the data to the stream buffer */
			if( stream->bufPos + length > stream->bufSize )
				{
#ifdef VIRTUAL_FILE_STREAM 
				if( sIsVirtualFileStream( stream ) )
					{
					status = expandVirtualFileStream( stream, length );
					if( cryptStatusError( status ) )
						return( status );
					}
				else
#endif /* VIRTUAL_FILE_STREAM */
					return( sSetError( stream, CRYPT_ERROR_OVERFLOW ) );
				}
			REQUIRES_S( boundsCheckZ( stream->bufPos, length, 
									  stream->bufSize ) );
			memcpy( stream->buffer + stream->bufPos, buffer, length );
			stream->bufPos += length;
			if( stream->bufEnd < stream->bufPos )
				stream->bufEnd = stream->bufPos;
#ifdef VIRTUAL_FILE_STREAM 
			if( sIsVirtualFileStream( stream ) )
				{
				/* This is a memory stream emulating a file stream, set the
				   dirty bit */
				SET_FLAG( stream->flags, STREAM_FLAG_DIRTY );
				}
#endif /* VIRTUAL_FILE_STREAM */
			status = CRYPT_OK;
			break;

#ifdef USE_FILES
		case STREAM_TYPE_FILE:
			{
			const BYTE *bufPtr = buffer;
			int dataLength, LOOP_ITERATOR;

			REQUIRES_S( TEST_FLAG( stream->flags, 
								   STREAM_FFLAG_BUFFERSET ) );

			/* Write the data to the file */
			LOOP_LARGE_INITCHECK( dataLength = length, dataLength > 0 )
				{
				const int bytesToCopy = \
						min( dataLength, stream->bufSize - stream->bufPos );

				if( bytesToCopy > 0 )
					{
					REQUIRES_S( boundsCheckZ( stream->bufPos, bytesToCopy, 
											  stream->bufSize ) );
					memcpy( stream->buffer + stream->bufPos, bufPtr, 
							bytesToCopy );
					stream->bufPos += bytesToCopy;
					bufPtr += bytesToCopy;
					dataLength -= bytesToCopy;
					}
				if( stream->bufPos >= stream->bufSize )
					{
					status = emptyStream( stream, FALSE );
					if( cryptStatusError( status ) )
						return( status );
					}
				}
			ENSURES_S( LOOP_BOUND_OK );
			SET_FLAG( stream->flags, STREAM_FLAG_DIRTY );
			status = CRYPT_OK;
			break;
			}
#endif /* USE_FILES */

#ifdef USE_TCP
		case STREAM_TYPE_NETWORK:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
			STM_WRITE_FUNCTION writeFunction;
			int bytesToWrite = length, bytesWritten;

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( netStream->protocol != STREAM_PROTOCOL_HTTP || \
						( netStream->protocol == STREAM_PROTOCOL_HTTP && \
						  length == sizeof( HTTP_DATA_INFO ) ) );

			/* Set up the function pointers.  We have to do this after the 
			   netStream check otherwise we'd potentially be dereferencing a 
			   NULL pointer */
			writeFunction = ( STM_WRITE_FUNCTION ) \
							FNPTR_GET( netStream->writeFunction );
			REQUIRES_S( writeFunction != NULL );

			/* For an HTTP stream what we're sending to the write function 
			   is an HTTP_DATA_INFO structure so we have to extract the 
			   actual length information from that */
			if( netStream->protocol == STREAM_PROTOCOL_HTTP )
				{
				const HTTP_DATA_INFO *httpDataInfo = \
									( HTTP_DATA_INFO * ) buffer;
				bytesToWrite = httpDataInfo->bytesToWrite;
				}

			/* Write the data to the network.  Writes are normally atomic
			   but if the partial-write flag is set can be restarted after
			   a timeout */
			status = writeFunction( stream, buffer, length, &bytesWritten );
			if( cryptStatusError( status ) )
				{
				/* If the lower-level code has indicated that the error 
				   condition is fatal, make it persistent for the stream */
				if( cryptStatusError( netStream->persistentStatus ) )
					stream->status = netStream->persistentStatus;

				return( status );
				}
			if( netStream->protocol == STREAM_PROTOCOL_HTTP )
				{
				/* As before, we need to extract the actual written length
				   value from the HTTP data structure */
				const HTTP_DATA_INFO *httpDataInfo = \
									( HTTP_DATA_INFO * ) buffer;
				bytesWritten = httpDataInfo->bytesTransferred;
				}
			if( bytesWritten < bytesToWrite && \
				!TEST_FLAG( stream->flags, STREAM_FLAG_PARTIALWRITE ) )
				{
				retExt( CRYPT_ERROR_TIMEOUT, 
						( CRYPT_ERROR_TIMEOUT, NETSTREAM_ERRINFO, 
						  "Write timed out with %d of %d bytes written",
						  bytesWritten, bytesToWrite ) );
				}
			status = bytesWritten;
			break;
			}
#endif /* USE_TCP */

		default:
			retIntError_Stream( stream );
		}

	ENSURES_S( sanityCheckStream( stream ) );

	return( status );
	}

#ifdef USE_FILES

/* Commit data in a stream to backing storage */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sflush( INOUT STREAM *stream )
	{
	int status = CRYPT_OK, flushStatus;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( stream->buffer, stream->bufSize ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	if( !isReadPtrDynamic( stream->buffer, stream->bufSize ) )
		retIntError_Stream( stream );

	REQUIRES_S( sanityCheckStream( stream ) && \
				( TEST_FLAG( stream->flags, STREAM_FFLAG_BUFFERSET ) || \
				  sIsVirtualFileStream( stream ) ) );
	REQUIRES_S( stream->type == STREAM_TYPE_FILE || \
				sIsVirtualFileStream( stream ) );
	REQUIRES_S( !TEST_FLAG( stream->flags, STREAM_FLAG_READONLY ) );

	/* If there's a problem with the stream don't try to do anything until
	   the error is cleared */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	/* If the data in the stream buffer is unchanged there's nothing to do */
	if( !TEST_FLAG( stream->flags, STREAM_FLAG_DIRTY ) )
		return( CRYPT_OK );

	/* If there's data still in the stream buffer and it's not a virtual 
	   file stream that's handled via a memory stream (for which the data 
	   is committed in an atomic operation when the file is flushed), write 
	   it to disk.  If there's an error at this point we still try and flush 
	   whatever data we have to disk so we don't bail out immediately if 
	   there's a problem */
	if( stream->bufPos > 0 && !sIsVirtualFileStream( stream ) )
		status = emptyStream( stream, TRUE );

	/* Commit the data */
	flushStatus = fileFlush( stream );
	CLEAR_FLAG( stream->flags, STREAM_FLAG_DIRTY );

	return( cryptStatusOK( status ) ? flushStatus : status );
	}
#endif /* USE_FILES */

/****************************************************************************
*																			*
*								Meta-data Functions							*
*																			*
****************************************************************************/

/* Set/clear the error status of a stream.  sSetError() returns the error 
   status that it's passed so that it can be called using
   'return( sSetError( stream, status ) );' */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sSetError( INOUT STREAM *stream, IN_ERROR const int status )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES_S( cryptStatusError( status ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	/* If there's already an error status set don't try and override it */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	stream->status = status;

	return( status );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void sClearError( INOUT STREAM *stream )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError_Void();

	stream->status = CRYPT_OK;
	}

/* Determine whether a stream is a null stream */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sIsNullStream( const STREAM *stream )
	{
	assert( isReadPtr( stream, sizeof( STREAM ) ) );

	/* Check that the input parameters are in order */
	if( !isReadPtr( stream, sizeof( STREAM ) ) )
		retIntError_Boolean();

	return( ( stream->type == STREAM_TYPE_NULL ) ? TRUE : FALSE );
	}


/* Move to an absolute position in a stream */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sseek( INOUT STREAM *stream, IN_LENGTH_Z const long position )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_NULL || \
				stream->type == STREAM_TYPE_MEMORY || \
				stream->type == STREAM_TYPE_FILE );
	REQUIRES_S( position >= 0 && position < MAX_BUFFER_SIZE );

	/* If there's a problem with the stream don't try to do anything */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	switch( stream->type )
		{
		case STREAM_TYPE_NULL:
			/* Move to the position in the stream buffer.  We never get 
			   called directly with an sseek() on a memory stream, but end 
			   up here via a translated sSkip() call */
			REQUIRES( isIntegerRange( position ) );
			stream->bufPos = ( int ) position;
			if( stream->bufEnd < stream->bufPos )
				stream->bufEnd = stream->bufPos;
			break;

		case STREAM_TYPE_MEMORY:
			/* Move to the position in the stream buffer */
			REQUIRES( isIntegerRange( position ) );
			if( ( int ) position > stream->bufSize )
				{
				stream->bufPos = stream->bufSize;
				return( sSetError( stream, CRYPT_ERROR_UNDERFLOW ) );
				}
			stream->bufPos = ( int ) position;
			if( stream->bufEnd < stream->bufPos )
				stream->bufEnd = stream->bufPos;
			break;

#ifdef USE_FILES
		case STREAM_TYPE_FILE:
			{
			const int blockOffset = ( stream->bufSize > 0 ) ? \
									position / stream->bufSize : 0;
			const int byteOffset = ( stream->bufSize > 0 ) ? \
								   position % stream->bufSize : 0;

			/* If it's a currently-disconnected file stream then all we can 
			   do is rewind the stream.  This occurs when we're doing an 
			   atomic flush of data to disk and we rewind the stream prior 
			   to writing the new/updated data.  The next buffer-connect 
			   operation will reset the stream state so there's nothing to 
			   do at this point */
			if( !TEST_FLAG( stream->flags, STREAM_FFLAG_BUFFERSET ) )
				{
				REQUIRES_S( position == 0 );

				return( CRYPT_OK );
				}

			/* Determine which buffer-size block of data we're moving to */
			if( TEST_FLAG( stream->flags, STREAM_FFLAG_EOF ) && \
				blockOffset > stream->bufCount )
				{
				/* If this is the last buffer's worth and we're trying to 
				   move past it, it's an error */
				return( sSetError( stream, CRYPT_ERROR_UNDERFLOW ) );
				}
			if( blockOffset != stream->bufCount )
				{
				/* We're not within the current buffer any more, remember 
				   that we have to explicitly update the file position on
				   the next read */
				SET_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED );

				/* If we're already positioned to read the next bufferful 
				   of data we don't have to explicitly skip ahead to it */
				if( blockOffset == stream->bufCount + 1 ) 
					{
					SET_FLAG( stream->flags, 
							  STREAM_FFLAG_POSCHANGED_NOSKIP );
					}

				stream->bufCount = blockOffset;
				}

			/* Now that we've got the buffer-sized block handled, set up
			   the byte offset within the block */
			if( TEST_FLAG( stream->flags, STREAM_FFLAG_EOF ) && \
				byteOffset > stream->bufEnd )
				{
				/* We've tried to move past EOF, this is an error */
				return( sSetError( stream, CRYPT_ERROR_UNDERFLOW ) );
				}

			stream->bufPos = byteOffset;
			break;
			}
#endif /* USE_FILES */

		default:
			retIntError_Stream( stream );
		}

	ENSURES_S( sanityCheckStream( stream ) );

	return( CRYPT_OK );
	}

/* Return the current posision in a stream */

CHECK_RETVAL_RANGE_NOERROR( 0, MAX_BUFFER_SIZE ) STDC_NONNULL_ARG( ( 1 ) ) \
int stell( const STREAM *stream )
	{
	assert( isReadPtr( stream, sizeof( STREAM ) ) );

	/* Check that the input parameters are in order */
	if( !isReadPtr( stream, sizeof( STREAM ) ) )
		retIntError();

	/* We can't use REQUIRE_S( sanityCheckStream() ) in this case because 
	   the stream is a const parameter.  Since stell() is expected to return 
	   a value in the range 0...stream->bufSize we don't use REQUIRES() 
	   either but simply return an offset of zero */
	REQUIRES_EXT( sanityCheckStream( stream ), 0 );
	REQUIRES_EXT( ( stream->type == STREAM_TYPE_NULL || \
					stream->type == STREAM_TYPE_MEMORY || \
					stream->type == STREAM_TYPE_FILE ), 0 );

	/* If there's a problem with the stream don't try to do anything */
	if( cryptStatusError( stream->status ) )
		{
		DEBUG_DIAG(( "Stream is in invalid state" ));
		assert( DEBUG_WARN );
		return( 0 );
		}

	switch( stream->type )
		{
		case STREAM_TYPE_NULL:
		case STREAM_TYPE_MEMORY:
			return( stream->bufPos );

#ifdef USE_FILES
		case STREAM_TYPE_FILE:
			return( ( stream->bufCount * stream->bufSize ) + \
					stream->bufPos );
#endif /* USE_FILES */
		}

	retIntError_Ext( 0 );
	}

/* Skip a number of bytes in a stream, with a bounds check on the maximum 
   allowable offset to skip */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sSkip( INOUT STREAM *stream, const long offset, 
		   IN_DATALENGTH const long maxOffset )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_NULL || \
				stream->type == STREAM_TYPE_MEMORY || \
				stream->type == STREAM_TYPE_FILE );
	REQUIRES_S( offset > 0 );
	REQUIRES_S( maxOffset > 0 && maxOffset < MAX_BUFFER_SIZE );

	/* If there's a problem with the stream don't try to do anything */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	/* Make sure that the offset to skip is valid */
	if( offset > maxOffset || offset >= MAX_BUFFER_SIZE - stream->bufPos )
		return( CRYPT_ERROR_BADDATA );

	return( sseek( stream, stream->bufPos + offset ) );
	}

/* Peek at the next data value in a stream */

CHECK_RETVAL_RANGE( 0, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int sPeek( INOUT STREAM *stream )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( stream->buffer, stream->bufSize ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_MEMORY || \
				stream->type == STREAM_TYPE_FILE );

	/* If there's a problem with the stream don't try to do anything until
	   the error is cleared */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	/* Read the data from the buffer, but without advancing the read pointer
	   like sgetc() does */
	switch( stream->type )
		{
		case STREAM_TYPE_MEMORY:
			/* Read the data from the stream buffer */
			if( stream->bufPos >= stream->bufEnd )
				return( sSetError( stream, CRYPT_ERROR_UNDERFLOW ) );
			return( stream->buffer[ stream->bufPos ] );

#ifdef USE_FILES
		case STREAM_TYPE_FILE:
			REQUIRES_S( TEST_FLAG( stream->flags, 
								   STREAM_FFLAG_BUFFERSET ) );

			/* Read the data from the file */
			if( stream->bufPos >= stream->bufEnd || \
				TEST_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED ) )
				{
				int status = refillStream( stream );
				if( cryptStatusError( status ) )
					return( ( status == OK_SPECIAL ) ? 0 : status );
				}
			return( stream->buffer[ stream->bufPos ] );
#endif /* USE_FILES */
		}

	retIntError_Stream( stream );
	}

/****************************************************************************
*																			*
*								IOCTL Functions								*
*																			*
****************************************************************************/

/* Perform an IOCTL on a stream.  There are two variations of this, a get and
   a set form, which helps with type-checking compared to the usual do-anything
   ioctl() */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setStreamBuffer( INOUT STREAM *stream, 
							IN_BUFFER_OPT( dataLen ) const void *data, 
							IN_DATALENGTH_Z const int dataLen )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( ( data == NULL && dataLen == 0 ) || \
			isReadPtrDynamic( data, dataLen ) );

	REQUIRES_S( ( data == NULL && dataLen == 0 ) || \
				( data != NULL && \
				  dataLen > 0 && dataLen < MAX_BUFFER_SIZE ) );
	REQUIRES_S( dataLen == 0 || \
				dataLen == 512 || dataLen == 1024 || \
				dataLen == 2048 || dataLen == 4096 || \
				dataLen == 8192 || dataLen == 16384 );

#ifdef VIRTUAL_FILE_STREAM 
	/* If it's a virtual file stream emulated in memory, don't do anything */
	if( sIsVirtualFileStream( stream ) )
		return( CRYPT_OK );
#endif /* VIRTUAL_FILE_STREAM */

	/* Set up the buffer variables.  File streams don't make use of the 
	   bufEnd indicator so we set it to the same value as bufSize to ensure 
	   that the stream passes the sanity checks */
	stream->buffer = ( void * ) data;
	stream->bufSize = stream->bufEnd = dataLen;

	/* We've switched to a new I/O buffer, reset all buffer- and stream-
	   state related variables and remember that we have to reset the stream 
	   position since there may be a position-change pending that hasn't 
	   been reflected down to the underlying file yet (if the position 
	   change was within the same buffer then the POSCHANGED flag won't have 
	   been set since only the bufPos will have changed) */
	stream->bufPos = stream->bufCount = 0;
	sClearError( stream );
	CLEAR_FLAGS( stream->flags, ( STREAM_FFLAG_BUFFERSET | \
								  STREAM_FFLAG_EOF | \
								  STREAM_FFLAG_POSCHANGED_NOSKIP ) );
	SET_FLAG( stream->flags, STREAM_FFLAG_POSCHANGED );
	if( data != NULL )
		SET_FLAG( stream->flags, STREAM_FFLAG_BUFFERSET );

	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sioctlSet( INOUT STREAM *stream, 
			   IN_ENUM( STREAM_IOCTL ) const STREAM_IOCTL_TYPE type, 
			   const int value )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	/* If this is a pseudo-stream then there's no network information 
	   present to set information for */
#ifndef CONFIG_CONSERVE_MEMORY_EXTRA
	if( sIsPseudoStream( stream ) )
		return( CRYPT_OK );
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( ( ( stream->type == STREAM_TYPE_FILE || \
					sIsVirtualFileStream( stream ) ) && \
				( type == STREAM_IOCTL_IOBUFFER || \
				  type == STREAM_IOCTL_PARTIALREAD ) ) || \
				( stream->type == STREAM_TYPE_NETWORK ) );
	REQUIRES_S( isEnumRange( type, STREAM_IOCTL ) );
	REQUIRES_S( isIntegerRange( value ) );

	switch( type )
		{
		case STREAM_IOCTL_IOBUFFER:
			REQUIRES_S( value == 0 );

			return( setStreamBuffer( stream, NULL, 0 ) );

		case STREAM_IOCTL_PARTIALREAD:
			REQUIRES_S( value == FALSE || value == TRUE );

			if( value )
				SET_FLAG( stream->flags, STREAM_FLAG_PARTIALREAD );
			else
				CLEAR_FLAG( stream->flags, STREAM_FLAG_PARTIALREAD );

			return( CRYPT_OK );

		case STREAM_IOCTL_PARTIALWRITE:
			REQUIRES_S( value == FALSE || value == TRUE );

			if( value )
				SET_FLAG( stream->flags, STREAM_FLAG_PARTIALWRITE );
			else
				CLEAR_FLAG( stream->flags, STREAM_FLAG_PARTIALWRITE );

			return( CRYPT_OK );

#ifdef USE_TCP
		case STREAM_IOCTL_READTIMEOUT:
		case STREAM_IOCTL_WRITETIMEOUT:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( isIntegerRange( value ) );

			netStream->timeout = value;
			return( CRYPT_OK );
			}

		case STREAM_IOCTL_HANDSHAKECOMPLETE:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( value == TRUE );
			REQUIRES_S( isIntegerRangeNZ( netStream->timeout ) );
			REQUIRES_S( netStream->savedTimeout >= 0 && \
						netStream->savedTimeout < MAX_NETWORK_TIMEOUT );

			/* The security protocol handshake has completed, change the 
			   stream timeout value from the connect/handshake timeout to
			   the standard data transfer timeout */
			netStream->timeout = netStream->savedTimeout;
			return( CRYPT_OK );
			}

		case STREAM_IOCTL_LASTMESSAGE:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( value == TRUE );
			REQUIRES_S( netStream->protocol == STREAM_PROTOCOL_HTTP );

			SET_FLAG( netStream->nFlags, STREAM_NFLAG_LASTMSGW );
			return( CRYPT_OK );
			}

		case STREAM_IOCTL_HTTPREQTYPES:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( isEnumRange( value, STREAM_HTTPREQTYPE ) );
			REQUIRES_S( netStream->protocol == STREAM_PROTOCOL_HTTP );

			CLEAR_FLAG( netStream->nhFlags, STREAM_NHFLAG_REQMASK );
			switch( value )
				{
				case STREAM_HTTPREQTYPE_GET:
					SET_FLAG( netStream->nhFlags, STREAM_NHFLAG_GET );
					break;

				case STREAM_HTTPREQTYPE_POST:
					SET_FLAG( netStream->nhFlags, STREAM_NHFLAG_POST );
					break;

				case STREAM_HTTPREQTYPE_POST_AS_GET:
					/* This flag modifies the HTTP POST to encode it as a 
					   GET, for b0rken servers that don't support POST */
					SET_FLAGS( netStream->nhFlags,
							   STREAM_NHFLAG_POST | \
							   STREAM_NHFLAG_POST_AS_GET );
					break;

				case STREAM_HTTPREQTYPE_ANY:
					SET_FLAGS( netStream->nhFlags,
							   STREAM_NHFLAG_GET | STREAM_NHFLAG_POST );
					break;

				case STREAM_HTTPREQTYPE_WS_UPGRADE:
					/* The WebSockets upgrade request is sent as a GET with
					   "Connection: upgrade", so we enable GET as well as 
					   the WebSockets upgrade */
					SET_FLAGS( netStream->nhFlags,
							   STREAM_NHFLAG_GET | STREAM_NHFLAG_WS_UPGRADE );
					break;

				default:
					retIntError();
				}

			return( CRYPT_OK );
			}

		case STREAM_IOCTL_CLOSESENDCHANNEL:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
			STM_TRANSPORTDISCONNECT_FUNCTION transportDisconnectFunction;

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( value == TRUE );
			REQUIRES_S( !TEST_FLAG( netStream->nFlags, 
									STREAM_NFLAG_USERSOCKET ) );

			/* Set up the function pointers.  We have to do this after the 
			   netStream check otherwise we'd potentially be dereferencing a 
			   NULL pointer */
			transportDisconnectFunction = \
						( STM_TRANSPORTDISCONNECT_FUNCTION ) \
						FNPTR_GET( netStream->transportDisconnectFunction );
			REQUIRES_S( transportDisconnectFunction != NULL );

			/* If this is a user-supplied socket we can't perform a partial 
			   close without affecting the socket as seen by the user so we 
			   only perform the partial close if it's a cryptlib-controlled 
			   socket */
			if( !TEST_FLAG( netStream->nFlags, STREAM_NFLAG_USERSOCKET ) )
				transportDisconnectFunction( netStream, FALSE );

			return( CRYPT_OK );
			}
#endif /* USE_TCP */
		}

	retIntError_Stream( stream );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sioctlSetString( INOUT STREAM *stream, 
					 IN_ENUM( STREAM_IOCTL ) const STREAM_IOCTL_TYPE type, 
					 IN_BUFFER( dataLen ) const void *data, 
					 IN_DATALENGTH const int dataLen )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( data, dataLen ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	REQUIRES_S( sanityCheckStream( stream ) );
#ifndef CONFIG_CONSERVE_MEMORY_EXTRA
	REQUIRES_S( ( sIsPseudoStream( stream ) && \
				  type == STREAM_IOCTL_ERRORINFO ) || \
				( ( stream->type == STREAM_TYPE_FILE || \
					sIsVirtualFileStream( stream ) ) && \
				  ( type == STREAM_IOCTL_ERRORINFO || \
					type == STREAM_IOCTL_IOBUFFER ) ) || \
				( stream->type == STREAM_TYPE_NETWORK ) );
#else
	REQUIRES_S( ( ( stream->type == STREAM_TYPE_FILE || \
					sIsVirtualFileStream( stream ) ) && \
				  ( type == STREAM_IOCTL_ERRORINFO || \
					type == STREAM_IOCTL_IOBUFFER ) ) || \
				( stream->type == STREAM_TYPE_NETWORK ) );
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */
	REQUIRES_S( isEnumRange( type, STREAM_IOCTL ) );
	REQUIRES_S( dataLen > 0 && dataLen < MAX_BUFFER_SIZE );

	switch( type )
		{
		case STREAM_IOCTL_ERRORINFO:
			{
#ifdef USE_TCP
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
#endif /* USE_TCP */

			REQUIRES_S( dataLen == sizeof( ERROR_INFO ) );

			/* If this stream type doesn't record extended error information
			   then there's nothing to do */
			if( stream->type != STREAM_TYPE_NETWORK )
				return( CRYPT_OK );

#ifdef USE_TCP
			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );

			/* Copy the error information to the stream */
			copyErrorInfo( NETSTREAM_ERRINFO, data );
#endif /* USE_TCP */
			return( CRYPT_OK );
			}

		case STREAM_IOCTL_IOBUFFER:
			REQUIRES_S( dataLen == 0 || \
						dataLen == 512 || dataLen == 1024 || \
						dataLen == 2048 || dataLen == 4096 || \
						dataLen == 8192 || dataLen == 16384 );
			REQUIRES_S( safeBufferCheck( data, dataLen ) );

			return( setStreamBuffer( stream, data, dataLen ) );
		}

	retIntError_Stream( stream );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sioctlGet( INOUT STREAM *stream, 
			   IN_ENUM( STREAM_IOCTL ) const STREAM_IOCTL_TYPE type, 
			   OUT_BUFFER_FIXED( dataMaxLen ) void *data, 
			   IN_LENGTH_SHORT const int dataMaxLen )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( data, dataMaxLen ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( stream, sizeof( STREAM ) ) )
		retIntError();

	/* If this is a pseudo-stream then there's no network information 
	   present to get error information from */
#ifndef CONFIG_CONSERVE_MEMORY_EXTRA
	if( sIsPseudoStream( stream ) )
		{
		memset( data, 0, dataMaxLen );
		return( CRYPT_OK );
		}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

	REQUIRES_S( sanityCheckStream( stream ) );
	REQUIRES_S( stream->type == STREAM_TYPE_NETWORK );
	REQUIRES_S( isEnumRange( type, STREAM_IOCTL ) );
	REQUIRES_S( data != NULL );
	REQUIRES_S( isShortIntegerRangeNZ( dataMaxLen ) );

	switch( type )
		{
#ifdef USE_TCP
		case STREAM_IOCTL_READTIMEOUT:
		case STREAM_IOCTL_WRITETIMEOUT:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( dataMaxLen == sizeof( int ) );

			/* These two values are stored as a shared timeout value
			   which is updated on each data read or write by the
			   caller so there's no need to maintain distinct values */
			*( ( int * ) data ) = netStream->timeout;
			return( CRYPT_OK );
			}

		case STREAM_IOCTL_CONNSTATE:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( dataMaxLen == sizeof( int ) );

			*( ( int * ) data ) = TEST_FLAG( netStream->nFlags, 
											 STREAM_NFLAG_LASTMSGR ) ? \
								  FALSE : TRUE;
			return( CRYPT_OK );
			}

		case STREAM_IOCTL_GETCLIENTNAME:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( isShortIntegerRangeNZ( dataMaxLen ) );

			if( netStream->clientAddressLen <= 0 )
				return( CRYPT_ERROR_NOTFOUND );
			if( netStream->clientAddressLen > dataMaxLen )
				return( CRYPT_ERROR_OVERFLOW );
			REQUIRES_S( rangeCheck( netStream->clientAddressLen, 1, 
									dataMaxLen ) );
			memcpy( data, netStream->clientAddress, 
					netStream->clientAddressLen );

			return( CRYPT_OK );
			}

		case STREAM_IOCTL_GETCLIENTNAMELEN:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( dataMaxLen == sizeof( int ) );

			if( netStream->clientAddressLen <= 0 )
				return( CRYPT_ERROR_NOTFOUND );
			*( ( int * ) data ) = netStream->clientAddressLen;

			return( CRYPT_OK );
			}

		case STREAM_IOCTL_GETPEERTYPE:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( dataMaxLen == sizeof( STREAM_PEER_TYPE ) );

			*( ( STREAM_PEER_TYPE * ) data ) = netStream->systemType;

			return( CRYPT_OK );
			}

		case STREAM_IOCTL_GETCLIENTPORT:
			{
			NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

			REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );
			REQUIRES_S( dataMaxLen == sizeof( int ) );

			if( netStream->clientPort <= 0 )
				return( CRYPT_ERROR_NOTFOUND );
			*( ( int * ) data ) = netStream->clientPort;

			return( CRYPT_OK );
			}
#endif /* USE_TCP */
		}

	retIntError_Stream( stream );
	}

/****************************************************************************
*																			*
*								Misc Functions								*
*																			*
****************************************************************************/

#ifdef USE_FILES

/* Convert a file stream to a memory stream.  Usually this allocates a 
   buffer and reads the stream into it, however if it's a read-only memory-
   mapped file it just creates a second reference to the data to save
   memory */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int sFileToMemStream( OUT STREAM *memStream, 
					  INOUT STREAM *fileStream,
					  OUT_BUFFER_ALLOC_OPT( length ) void **bufPtrPtr, 
					  IN_DATALENGTH const int length )
	{
	void *bufPtr;
	int status;

	assert( isWritePtr( memStream, sizeof( STREAM ) ) );
	assert( isWritePtr( fileStream, sizeof( STREAM ) ) );
	assert( isWritePtr( bufPtrPtr, sizeof( void * ) ) );

	/* Check that the input parameters are in order */
	if( !isWritePtr( memStream, sizeof( STREAM ) ) || \
		!isWritePtr( fileStream, sizeof( STREAM ) ) || \
		!isWritePtr( bufPtrPtr, sizeof( void * ) ) )
		{
		/* Since memStream() is an OUT parameter we can't use it with a
		   retIntError_Stream() but have to use a plain retIntError() */
		retIntError();
		}

	/* We have to use REQUIRES() here rather than REQUIRES_S() since it's 
	   not certain which of the two streams to set the status for */
	REQUIRES( sanityCheckStream( fileStream ) && \
			  TEST_FLAG( fileStream->flags, STREAM_FFLAG_BUFFERSET ) );
	REQUIRES( fileStream->type == STREAM_TYPE_FILE );
	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( memStream, 0, sizeof( STREAM ) );
	*bufPtrPtr = NULL;

#ifdef VIRTUAL_FILE_STREAM 
	/* If it's a read-only memory-mapped file stream create the memory 
	   stream as a reference to the file stream */
	if( TEST_FLAGS( fileStream->flags, 
					STREAM_FLAG_READONLY | STREAM_FFLAG_MMAPPED, 
					STREAM_FLAG_READONLY | STREAM_FFLAG_MMAPPED ) )
		{
		/* Make sure that there's enough data left in the memory-mapped
		   stream to reference it as a file stream */
		if( length > fileStream->bufSize - fileStream->bufPos )
			return( CRYPT_ERROR_UNDERFLOW );

		/* Create a second reference to the memory-mapped stream and advance 
		   the read pointer in the memory-mapped file stream to mimic the 
		   behaviour of a read from it to the memory stream */
		sMemConnect( memStream, fileStream->buffer + fileStream->bufPos, 
					 length );
		status = sSkip( fileStream, length, SSKIP_MAX );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( memStream );
			return( status );
			}
		return( CRYPT_OK );
		}
#endif /* VIRTUAL_FILE_STREAM */

	/* It's a file stream, allocate a buffer for the data and read it in as
	   a memory stream */
	REQUIRES( rangeCheck( length, 1, MAX_BUFFER_SIZE ) );
	if( ( bufPtr = clAlloc( "sFileToMemStream", length ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	status = sread( fileStream, bufPtr, length );
	if( cryptStatusError( status ) )
		{
		clFree( "sFileToMemStream", bufPtr );
		return( status );
		}
	sMemConnect( memStream, bufPtr, length );
	*bufPtrPtr = bufPtr;
	return( CRYPT_OK );
	}
#endif /* USE_FILES */
