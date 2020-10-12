/****************************************************************************
*																			*
*					cryptlib Session Write Support Routines					*
*					  Copyright Peter Gutmann 1998-2017						*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "session.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "session/session.h"
#endif /* Compiler-specific includes */

#ifdef USE_SESSIONS

/* Common code to write data over a secure connection.  This is called by 
   the protocol-specific handlers, which supply three functions of which one 
   is used for write purposes:

	preparePacketFunction()	- Wraps a packet in preparation for sending it.

   The behaviour of the network-level stream handlers when called with given 
   timeout and byte-count values is as follows:

	Timeout		byteCount		Result
	-------		---------		------
		  - error -				error
	  0			  0				0
	  0			> 0				byteCount
	> 0			  0				CRYPT_ERROR_TIMEOUT
	> 0			> 0				byteCount

   Errors encountered in preparePacketFunction() are always fatal.  In 
   theory we could try to recover, however the functions update assorted 
   crypto state such as packet sequence numbers and IVs that would be tricky 
   to roll back, and in practice recoverable errors are likely to be 
   extremely rare (at best perhaps a CRYPT_ERROR_TIMEOUT for a context tied 
   to a device, however even this won't occur since the conventional 
   encryption and MAC contexts are all internal native contexts) so there's 
   little point in trying to make the functions recoverable */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the session state.  This function is called both from the
   higher-level sanityCheckSession() and indirectly from lower-level write 
   functions as a pre/postcondition on write operations */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckSessionWrite( const SESSION_INFO *sessionInfoPtr )
	{
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Request/response sessions only need a single buffer, if this is one of 
	   those then the send buffer isn't used */
	if( sessionInfoPtr->type != CRYPT_SESSION_SSH && \
		sessionInfoPtr->type != CRYPT_SESSION_SSH_SERVER && \
		sessionInfoPtr->type != CRYPT_SESSION_SSL && \
		sessionInfoPtr->type != CRYPT_SESSION_SSL_SERVER )
		{
		if( sessionInfoPtr->sendBuffer != NULL || \
			sessionInfoPtr->sendBufSize != CRYPT_UNUSED || \
			sessionInfoPtr->sendBufPos != 0 || \
			sessionInfoPtr->sendBufStartOfs != 0 || \
			sessionInfoPtr->sendBufPartialBufPos != 0 )
			{
			DEBUG_PUTS(( "sanityCheckSessionWrite: Spurious send buffer" ));
			return( FALSE );
			}

		return( TRUE );
		}

	/* Make sure that the general state is in order.  maxPacketSize has been
	   checked in the read check */
	if( sessionInfoPtr->sendBufSize < MIN_BUFFER_SIZE || \
		sessionInfoPtr->sendBufSize >= MAX_BUFFER_SIZE )
		{
		DEBUG_PUTS(( "sanityCheckSessionWrite: Send buffer size" ));
		return( FALSE );
		}
	if( sessionInfoPtr->sendBuffer != NULL && \
		!safeBufferCheck( sessionInfoPtr->sendBuffer, 
						  sessionInfoPtr->sendBufSize ) )
		{
		DEBUG_PUTS(( "sanityCheckSessionWrite: Send buffer state" ));
		return( FALSE );
		}
	if( sessionInfoPtr->sendBufStartOfs < 0 || \
		sessionInfoPtr->sendBufStartOfs > FIXED_HEADER_MAX )
		{
		DEBUG_PUTS(( "sanityCheckSessionWrite: Send buffer offset" ));
		return( FALSE );
		}
	if( sessionInfoPtr->partialWrite != TRUE && \
		sessionInfoPtr->partialWrite != FALSE )
		{
		DEBUG_PUTS(( "sanityCheckSessionWrite: Partial write" ));
		return( FALSE );
		}

	/* Make sure that the buffer position values are within bounds */
	if( sessionInfoPtr->sendBufPos < sessionInfoPtr->sendBufStartOfs || \
		sessionInfoPtr->sendBufPos >= sessionInfoPtr->sendBufSize )
		{
		DEBUG_PUTS(( "sanityCheckSessionWrite: Send buffer info" ));
		return( FALSE );
		}
	if( sessionInfoPtr->sendBufPartialBufPos < 0 || \
		sessionInfoPtr->sendBufPartialBufPos >= sessionInfoPtr->sendBufPos )
		{
		DEBUG_PUTS(( "sanityCheckSessionWrite: Send buffer partial position" ));
		return( FALSE );
		}
	if( !sessionInfoPtr->partialWrite && \
		sessionInfoPtr->sendBufPos > sessionInfoPtr->sendBufStartOfs + \
									 sessionInfoPtr->maxPacketSize )
		{
		DEBUG_PUTS(( "sanityCheckSessionWrite: Write position" ));
		return( FALSE );
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*							Session Data Write Functions					*
*																			*
****************************************************************************/

/* Get the amount of space remaining in the send buffer

		startOfs	 bufPos
			|			|
			v			v
	+-------+-----------+-----------+---+
	|.......|///////////|...........|	|
	+-------+-----------+-----------+---+
			|<----- maxPacket ----->|
						|<- remain->| */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getRemainingBufferSpace( const SESSION_INFO *sessionInfoPtr )
	{
	const int currentByteCount = sessionInfoPtr->sendBufPos - \
								 sessionInfoPtr->sendBufStartOfs;
	int remainingByteCount;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionWrite( sessionInfoPtr ) );
	REQUIRES( currentByteCount >= 0 && \
			  currentByteCount <= sessionInfoPtr->maxPacketSize && \
			  currentByteCount < MAX_BUFFER_SIZE );
	remainingByteCount = sessionInfoPtr->maxPacketSize - currentByteCount;
	ENSURES( remainingByteCount >= 0 && remainingByteCount < MAX_BUFFER_SIZE );

	return( remainingByteCount );
	}

/* Send data to the remote system.  There are two strategies for handling 
   buffer filling and partial writes, either to fill the buffer as full as 
   possible and write it all at once, or to write complete packets as soon 
   as they're available.  We use the latter strategy both because it 
   considerably simplifies buffer management and because interleaving 
   (asynchronous once it gets to the TCP stack) writes and packet processing 
   increases the chances that the current packet will be successfully 
   dispatched across the network while the next one is being encrypted - 
   trying to write a large amount of data in one go greatly increases the 
   chances that the write won't be completed in one go.

   Session buffer management is handled as follows: The sendBufStartOfs 
   index points to the start of the payload space in the buffer (everything 
   before this is header data).  The maxPacketSize value indicates the end 
   of the payload space relative to the sendBufStartOfs:

	<- header ->|<---- payload ---->|
	+-----------+-------------------+-------+
	|			|///////////////////|		|
	+-----------+-------------------+-------+
				^					^
				|					|
			sbStartOfs		  maxPacketSize

   The sendBufPos index moves from sendBufStartOfs to maxPacketSize after 
   which the data is wrapped up by the protocol-specific code.  At this 
   point sendBufPos usually points past the end of maxPacketSize due to the 
   addition of trailer data such as encryption block padding and a MAC.  
   Once the packet is assembled, the data is flushed and the sendBufPos 
   index reset to sendBufStartOfs:

		   sbStartOfs		  maxPacketSize
				|					|
				v					v
	+-----------+-------+-----------+-------+
	|...........|.......|///////////|///////|
	+-----------+-------+-----------+-------+
	|<---- writtem ---->^<---- to write --->^
						|					|
				  sbPartialBufPos		  sbPos

   As with reads, writes can be non-atomic, although on a more restrictive 
   scale than reads: Once an encrypted packet has been assembled in the 
   write buffer the entire contents must be written before a new packet can 
   be assembled.  This guarantees that when the caller flushes data through 
   to the other side all of the data will be sent (and the other side will 
   have a chance to react to it) before the next load of data can be flushed 
   through.

   Once we have partial data in the send buffer all further attempts to add 
   more data fail until the remainder of the partially-written data has been 
   flushed.  This is handled by setting sendBufPartialBufPos to point to the 
   first byte of unwritten data, so that sendBufPartialBufPos ... sendBufPos 
   remains to be written */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
static int flushData( SESSION_INFO *sessionInfoPtr )
	{
	SES_PREPAREPACKET_FUNCTION preparePacketFunction;
	int bytesToWrite, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionWrite( sessionInfoPtr ) );

	preparePacketFunction = ( SES_PREPAREPACKET_FUNCTION ) \
					FNPTR_GET( sessionInfoPtr->preparePacketFunction );
	REQUIRES( preparePacketFunction != NULL );

	/* If there's no data to flush, exit */
	if( sessionInfoPtr->sendBufPos <= sessionInfoPtr->sendBufStartOfs )
		return( CRYPT_OK );	

	/* If there's no unwritten data from a previous write attempt still 
	   present, prepare to send the new data */
	if( !sessionInfoPtr->partialWrite )
		{
		ENSURES( sessionInfoPtr->sendBufPartialBufPos == 0 );

		/* If there's a layered protocol inside the outer one, prepare the 
		   inner protocol's data packet */
		if( TEST_FLAG( sessionInfoPtr->flags, 
					   SESSION_FLAG_SUBPROTOCOL_ACTIVE ) )
			{
			int newLength;

			status = newLength = \
				prepareInnerPacketFunction( sessionInfoPtr,
						sessionInfoPtr->sendBuffer + sessionInfoPtr->sendBufStartOfs, 
						sessionInfoPtr->sendBufSize - sessionInfoPtr->sendBufStartOfs,
						sessionInfoPtr->sendBufPos - sessionInfoPtr->sendBufStartOfs );
			if( cryptStatusError( status ) )
				return( status );
			ENSURES( newLength > 0 && \
					 sessionInfoPtr->sendBufStartOfs + newLength <= \
												sessionInfoPtr->sendBufPos );
			sessionInfoPtr->sendBufPos = sessionInfoPtr->sendBufStartOfs + newLength;
			}

		status = length = preparePacketFunction( sessionInfoPtr );
		if( cryptStatusError( status ) )
			{
			/* Errors in the crypto are immediately fatal */
			sessionInfoPtr->writeErrorState = status;
			return( status );
			}

		/* Adjust the buffer position to account for the wrapped packet
		   size */
		sessionInfoPtr->sendBufPos = length;
		ENSURES( sessionInfoPtr->sendBufPos > 0 && \
				 sessionInfoPtr->sendBufPos <= sessionInfoPtr->sendBufSize );
		}
	bytesToWrite = sessionInfoPtr->sendBufPos - \
				   sessionInfoPtr->sendBufPartialBufPos;
	ENSURES( bytesToWrite > 0 && bytesToWrite < MAX_BUFFER_SIZE );

	/* Send the data through to the remote system */
	ENSURES( boundsCheckZ( sessionInfoPtr->sendBufPartialBufPos, bytesToWrite,
						   sessionInfoPtr->sendBufPos ) );
	status = length = swrite( &sessionInfoPtr->stream, 
							  sessionInfoPtr->sendBuffer + \
								sessionInfoPtr->sendBufPartialBufPos, 
							  bytesToWrite );
	if( cryptStatusError( status ) && status != CRYPT_ERROR_TIMEOUT )
		{
		/* There was an error other than a (restartable) send timeout,
		   return the error details to the caller */
		sessionInfoPtr->writeErrorState = status;
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}

	/* If the send timed out before all of the data could be written, 
	   record how much still remains to be sent and inform the caller.  We
	   return this special-case code rather than either a timeout or
	   CRYPT_OK / 0 bytes because the caller can turn this into a situation-
	   specific status at the higher level, a timeout error for an explicit
	   flush but a CRYPT_OK / 0 for an implicit flush performed as part of a
	   write */
	if( status == CRYPT_ERROR_TIMEOUT )
		{
		/* We timed out with nothing written, let the caller know */
		sessionInfoPtr->partialWrite = TRUE;
		ENSURES( sanityCheckSessionWrite( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}
	if( length < bytesToWrite )
		{
		/* We wrote at least some part of the packet, adjust the partial-
		   write position by the amount that we wrote */
		sessionInfoPtr->sendBufPartialBufPos += length;
		sessionInfoPtr->partialWrite = TRUE;
		ENSURES( sanityCheckSessionWrite( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}

	ENSURES( length == bytesToWrite );

	/* We sent everything, reset the buffer status values */
	sessionInfoPtr->sendBufPos = sessionInfoPtr->sendBufStartOfs;
	sessionInfoPtr->partialWrite = FALSE;
	sessionInfoPtr->sendBufPartialBufPos = 0;

	/* If there's a layered protocol inside the outer one, write the inner 
	   protocol's packet header in preparation for the future addition of
	   payload data */
	if( TEST_FLAG( sessionInfoPtr->flags, 
				   SESSION_FLAG_SUBPROTOCOL_ACTIVE ) )
		{
		status = length = \
			writeInnerHeaderFunction( sessionInfoPtr,
									  sessionInfoPtr->sendBuffer + \
										sessionInfoPtr->sendBufPos,
									  sessionInfoPtr->sendBufSize - \
										sessionInfoPtr->sendBufPos );
		if( cryptStatusError( status ) )
			return( status );
		sessionInfoPtr->sendBufPos += length;
		}

	ENSURES( sanityCheckSessionWrite( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int putSessionData( INOUT SESSION_INFO *sessionInfoPtr, 
					IN_BUFFER_OPT( dataLength ) const void *data,
					IN_DATALENGTH_Z const int dataLength, 
					OUT_DATALENGTH_Z int *bytesCopied )
	{
	BYTE *dataPtr = ( BYTE * ) data;
	int length = dataLength, availableBuffer, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( data == NULL || isReadPtrDynamic( data, dataLength ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( sanityCheckSessionWrite( sessionInfoPtr ) );
	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( data != NULL && \
				dataLength > 0 && dataLength < MAX_BUFFER_SIZE ) );

	/* Clear return value */
	*bytesCopied = 0;

	/* If there's an error pending (which will always be fatal, see the
	   comment after the flushData() call below), set the current error state
	   to the pending state and return */
	if( cryptStatusError( sessionInfoPtr->pendingWriteErrorState ) )
		{
		REQUIRES( sessionInfoPtr->receiveBufPos == 0 );

		status = sessionInfoPtr->writeErrorState = \
						sessionInfoPtr->pendingWriteErrorState;
		sessionInfoPtr->pendingWriteErrorState = CRYPT_OK;
		return( status );
		}

	/* Update the stream write timeout to the current user-selected write 
	   timeout in case the user has changed the timeout setting.
	   
	   See the long comment in getSessionData() for the unfortunate 
	   interaction of the high-level timeout setting and the very chatty SSH 
	   protocol */
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_WRITETIMEOUT, 
			   sessionInfoPtr->writeTimeout );

	/* If it's a flush, send the data through to the server.  If there's a 
	   timeout error during an explicit flush (that is, some but not all of
	   the data is written, so it's a soft timeout) it's converted into an 
	   explicit hard timeout failure */
	if( length <= 0 )
		{
		const int oldBufPos = sessionInfoPtr->sendBufPartialBufPos;
		int bytesWritten;

		status = flushData( sessionInfoPtr );
		if( status != OK_SPECIAL )
			return( status );

		/* Since a partial write isn't a network-level error condition (it's 
		   only treated as a problem once it gets to the putSessionData() 
		   layer) there's no extended error information set for it so we 
		   have to set the error information here when we turn the partial
		   write into a timeout error */
		bytesWritten = sessionInfoPtr->sendBufPartialBufPos - oldBufPos;
		if( bytesWritten > 0 )
			{
			retExt( CRYPT_ERROR_TIMEOUT,
					( CRYPT_ERROR_TIMEOUT, SESSION_ERRINFO, 
					  "Timeout during flush, only %d bytes were written "
					  "before the timeout of %d seconds expired",
					  sessionInfoPtr->sendBufPartialBufPos, 
					  sessionInfoPtr->writeTimeout ) );
			}
		retExt( CRYPT_ERROR_TIMEOUT,
				( CRYPT_ERROR_TIMEOUT, SESSION_ERRINFO, 
				  "Timeout during flush, no data could be written before "
				  "the timeout of %d seconds expired", 
				  sessionInfoPtr->writeTimeout ) );
		}
	ANALYSER_HINT( dataPtr != NULL );

	/* If there's unwritten data from a previous write still in the buffer, 
	   flush that through first.  Since this isn't an explicit flush by the
	   caller we convert a soft timeout indicator into CRYPT_OK / 0 bytes */
	if( sessionInfoPtr->partialWrite )
		{
		status = flushData( sessionInfoPtr );
		if( cryptStatusError( status ) )
			return( ( status == OK_SPECIAL ) ? CRYPT_OK : status );
		}

	/* If there's too much data to fit into the send buffer we need to send 
	   it through to the host to make room for more */
	status = availableBuffer = getRemainingBufferSpace( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	LOOP_LARGE_CHECK( length >= availableBuffer )
		{
		ENSURES( availableBuffer >= 0 && availableBuffer <= length );

		/* Copy in as much data as we have room for and send it through.  The
		   flush can return one of three classes of values:

			1. An error code, but not CRYPT_ERROR_TIMEOUT, which is handled
			   as case (2) below.
			2. OK_SPECIAL to indicate that some of the requested data 
			   (possibly 0 bytes) was written.
			3. CRYPT_OK to indicate that all of the requested data was
			   written and more can be written if necessary */
		if( availableBuffer > 0 )
			{
			REQUIRES( boundsCheck( sessionInfoPtr->sendBufPos, availableBuffer,
								   sessionInfoPtr->sendBufSize ) );
			memcpy( sessionInfoPtr->sendBuffer + sessionInfoPtr->sendBufPos,
					dataPtr, availableBuffer );
			sessionInfoPtr->sendBufPos += availableBuffer;
			dataPtr += availableBuffer;
			length -= availableBuffer;
			*bytesCopied += availableBuffer;
			}
		status = flushData( sessionInfoPtr );
		if( cryptStatusError( status ) )
			{
			/* If it's a soft timeout indicator convert it to a CRYPT_OK / 
			   0 bytes written */
			if( status == OK_SPECIAL )
				{
				ENSURES( sanityCheckSessionWrite( sessionInfoPtr ) );

				return( CRYPT_OK );
				}

			/* There was a problem flushing the data through, if we managed 
			   to copy anything into the buffer we've made some progress so 
			   we defer it until the next call */
			if( *bytesCopied > 0 )
				{
				sessionInfoPtr->pendingWriteErrorState = status;

				ENSURES( sanityCheckSessionWrite( sessionInfoPtr ) );

				return( CRYPT_OK );
				}

			/* Nothing was copied before the error occurred, it's 
			   immediately fatal */
			return( status );
			}

		/* We've flushed the buffer, update the available-space value */
		status = availableBuffer = getRemainingBufferSpace( sessionInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	/* If there's anything left, it'll fit completely into the send buffer, 
	   just copy it in */
	if( length > 0 )
		{
		ENSURES( length < availableBuffer );

		REQUIRES( boundsCheckZ( sessionInfoPtr->sendBufPos, length, 
							    sessionInfoPtr->sendBufSize ) );
		memcpy( sessionInfoPtr->sendBuffer + sessionInfoPtr->sendBufPos,
				dataPtr, length );
		sessionInfoPtr->sendBufPos += length;
		*bytesCopied += length;
		}

	ENSURES( sanityCheckSessionWrite( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*				Request/response Session Data Write Functions				*
*																			*
****************************************************************************/

#ifdef USE_CERTIFICATES

/* Write a PKI (i.e. ASN.1-encoded) datagram.  Unlike the secure session 
   protocols these operations are always atomic so the write process is much 
   simpler */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writePkiDatagram( INOUT SESSION_INFO *sessionInfoPtr, 
					  IN_BUFFER( contentTypeLen ) const char *contentType, 
					  IN_LENGTH_TEXT const int contentTypeLen )
	{
	HTTP_DATA_INFO httpDataInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtrDynamic( contentType, contentTypeLen ) );

	REQUIRES( sanityCheckSessionWrite( sessionInfoPtr ) );
	REQUIRES( contentTypeLen > 0 && contentTypeLen <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( sessionInfoPtr->receiveBufEnd > 4 && \
			  sessionInfoPtr->receiveBufEnd < MAX_BUFFER_SIZE );

	/* Write the datagram.  Request/response sessions use a single buffer 
	   for both reads and writes, which is why we're (apparently) writing
	   the contents of the read buffer */
	status = initHttpInfoWrite( &httpDataInfo, 
								sessionInfoPtr->receiveBuffer,
								sessionInfoPtr->receiveBufEnd, 
								sessionInfoPtr->receiveBufSize );
	ENSURES( cryptStatusOK( status ) );
	httpDataInfo.contentType = contentType;
	httpDataInfo.contentTypeLen = contentTypeLen;
	status = swrite( &sessionInfoPtr->stream, &httpDataInfo,
					 sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		}
	else
		status = CRYPT_OK;	/* swrite() returns a byte count */
	sessionInfoPtr->receiveBufEnd = 0;

	return( status );
	}
#endif /* USE_CERTIFICATES */
#endif /* USE_SESSIONS */
