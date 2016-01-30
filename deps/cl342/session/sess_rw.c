/****************************************************************************
*																			*
*				cryptlib Session Read/Write Support Routines				*
*					  Copyright Peter Gutmann 1998-2011						*
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

/* Common code to read and write data over a secure connection.  This is 
   called by the protocol-specific handlers, which supply three functions:

	readHeaderFunction()	- Reads the header for a packet and sets up
							  length information.
	processBodyFunction()	- Processes the body of a packet.
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

   Errors encountered in processBodyFunction() and preparePacketFunction() 
   are always fatal.  In theory we could try to recover, however the 
   functions update assorted crypto state such as packet sequence numbers 
   and IVs that would be tricky to roll back, and in practice recoverable 
   errors are likely to be extremely rare (at best perhaps a 
   CRYPT_ERROR_TIMEOUT for a context tied to a device, however even this 
   won't occur since the conventional encryption and MAC contexts are all 
   internal native contexts) so there's little point in trying to make the 
   functions recoverable */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the session state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckRead( const SESSION_INFO *sessionInfoPtr )
	{
	const int pendingPacketLength = sessionInfoPtr->pendingPacketLength;
	const int pendingPacketRemaining = sessionInfoPtr->pendingPacketRemaining;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that the general state is in order */
	if( sessionInfoPtr->receiveBufSize < MIN_BUFFER_SIZE || \
		sessionInfoPtr->receiveBufSize >= MAX_INTLENGTH )
		return( FALSE );

	/* Make sure that the buffer position values are within bounds */
	if( sessionInfoPtr->receiveBufEnd < 0 || \
		sessionInfoPtr->receiveBufEnd > sessionInfoPtr->receiveBufSize || \
		sessionInfoPtr->receiveBufPos < 0 || \
		sessionInfoPtr->receiveBufPos > sessionInfoPtr->receiveBufEnd )
		return( FALSE );
	if( sessionInfoPtr->partialHeaderRemaining < 0 || \
		sessionInfoPtr->partialHeaderRemaining > FIXED_HEADER_MAX )
		return( FALSE );

	/* If we haven't started processing data yet there's no packet 
	   information present */
	if( pendingPacketLength == 0 && pendingPacketRemaining == 0 )
		return( TRUE );

	/* Make sure that packet information is within bounds */
	if( pendingPacketLength < 0 || \
		pendingPacketLength >= sessionInfoPtr->receiveBufSize || \
		pendingPacketRemaining < 0 || \
		pendingPacketRemaining >= sessionInfoPtr->receiveBufSize )
		return( FALSE );
	if( ( sessionInfoPtr->receiveBufEnd - \
		  sessionInfoPtr->receiveBufPos ) + pendingPacketRemaining != \
		pendingPacketLength )
		return( FALSE );

	/* Make sure that packet header information is within bounds */
	if( sessionInfoPtr->partialHeaderRemaining < 0 || \
		sessionInfoPtr->partialHeaderRemaining > 16 )
		return( FALSE );	/* 16 = SSH header size */

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckWrite( const SESSION_INFO *sessionInfoPtr )
	{
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that the general state is in order */
	if( sessionInfoPtr->sendBufSize < MIN_BUFFER_SIZE || \
		sessionInfoPtr->sendBufSize >= MAX_INTLENGTH )
		return( FALSE );
	if( sessionInfoPtr->sendBufStartOfs < 0 || \
		sessionInfoPtr->sendBufStartOfs > FIXED_HEADER_MAX )
		return( FALSE );

	/* Make sure that the buffer position values are within bounds */
	if( sessionInfoPtr->sendBufPos < sessionInfoPtr->sendBufStartOfs || \
		sessionInfoPtr->sendBufPos >= sessionInfoPtr->sendBufSize )
		return( FALSE );
	if( sessionInfoPtr->sendBufPartialBufPos < 0 || \
		sessionInfoPtr->sendBufPartialBufPos >= sessionInfoPtr->sendBufPos )
		return( FALSE );
	if( !sessionInfoPtr->partialWrite )
		{
		if( sessionInfoPtr->sendBufPos > sessionInfoPtr->sendBufStartOfs + \
										 sessionInfoPtr->maxPacketSize )
			return( FALSE );
		}
	else
		{
		if( sessionInfoPtr->sendBufPartialBufPos >= sessionInfoPtr->sendBufPos )
			return( FALSE );
		}


	return( TRUE );
	}

/****************************************************************************
*																			*
*						Secure Session Data Read Functions					*
*																			*
****************************************************************************/

/* The read data code uses a helper function tryRead() that either reads
   everything which is available or to the end of the current packet.  In
   other words it's an atomic, all-or-nothing function that can be used by
   higher-level code to handle network-level packetisation.  
   
   Buffer management is handled as follows: The bufPos index always points 
   to the end of the decoded data (i.e. data that can be used by the user), 
   if there's no partial packet present this index is the same as bufEnd:

	bPos/bEnd
		|
		v
	----+------------------------
	....|
	----+------------------------

   After readHeaderFunction() has been called pendingPacketRemaining 
   contains the number of bytes required to complete the packet:
  
	bPos/bEnd
		|
		v
	----+-----------------------+----
	....|						|
	----+-----------------------+----
		|<---- pPL == pPR ----->|

   tryRead() then attempts to fill the buffer with the packet data, with 
   bufEnd pointing to the end of the received data and advancing as more 
   data is read:

	  bPos			  bEnd
		|				|
		v				v
	----+---------------+-------+----
	....|///////////////|		|
	----+---------------+-------+----
		|				|<-pPR->|
		|<-------- pPL -------->|

   When we reach the end of tryRead(), which means that 
   pendingPacketRemaining reaches zero, we process the complete packet in 
   the buffer with processBody():

	  bPos					  bEnd
		|						|
		v						v
	----+-----------------------+----
	....|///////////////////////|
	----+-----------------------+----
		|<-------- pPL -------->|
								| pPR = 0

   If processBody() completes successfully then bufPos and bufEnd are 
   adjusted to point to the end of the new data:

							bPos/bEnd
								|
								v
	----+-----------------------+----
	....|.......................|
	----+-----------------------+----

   The handling of any header data present at the start of the packet
   depends on the packet format, if the header is independent of the
   encrypted data it's handled entirely by the readHeaderFunction() and 
   there's no need to provide special-case handling.  If the header is part 
   of the encrypted data then decryption is a two-stage operation in which
   readHeaderFunction() decrypts just enough of the packet to extract and
   process the header (depositing any leftover non-header data at the start
   of the buffer) and processBodyFunction() processes the rest of the data.

   Errors in the readHeaderFunction() are fatal if they come from the session
   protocol level (e.g. a MAC failure or bad packet) and nonfatal if they
   come from the network layer below the session (the stream-level code has
   its own handling of fatal vs. nonfatal errors, so we don't try and get
   down to that level) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int tryRead( INOUT SESSION_INFO *sessionInfoPtr, 
					OUT_LENGTH_Z int *bytesRead,
					OUT_ENUM_OPT( READSTATE ) READSTATE_INFO *readInfo )
	{
	int length, bytesLeft, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( bytesRead, sizeof( int ) ) );
	assert( isWritePtr( readInfo, sizeof( READSTATE_INFO ) ) );

	REQUIRES( sanityCheckRead( sessionInfoPtr ) );

	/* Clear return values */
	*bytesRead = 0;
	*readInfo = READINFO_NONE;

	/* If there's no pending packet information present, try and read it.
	   This can return one of four classes of values:

		1. An error code.
		2. Zero, to indicate that nothing was read.
		3. OK_SPECIAL and read information READINFO_NOOP to indicate that 
		   header data but no payload data was read.
		4. A byte count and read information READINFO_HEADERPAYLOAD to 
		   indicate that some payload data was read as part of the header */
	if( sessionInfoPtr->pendingPacketLength <= 0 )
		{
		status = length = \
			sessionInfoPtr->readHeaderFunction( sessionInfoPtr, readInfo );
		if( cryptStatusError( status ) )
			{
			/* Anything other than OK_SPECIAL to indicate a no-op read is an
			   error */
			if( status != OK_SPECIAL )
				return( status );
			ENSURES( *readInfo == READINFO_NOOP );
			}
		else
			{
			/* If nothing was read, we're done */
			if( length <= 0 )
				{
				*bytesRead = 0;
				return( CRYPT_OK );
				}
			}
		ENSURES( ( status == OK_SPECIAL && *readInfo == READINFO_NOOP ) || \
				 ( length > 0 && *readInfo == READINFO_HEADERPAYLOAD ) );
		if( *readInfo == READINFO_HEADERPAYLOAD )
			{
			/* Some protocols treat the header information for a secured
			   data packet as part of the data so when we read the header we
			   can get part of the payload included in the read.  When the
			   protocol-specific header read code obtains some payload data
			   alongside the header it returns READINFO_HEADERPAYLOAD to
			   indicate that the packet information needs to be adjusted for 
			   the packet header data that was just read */
			sessionInfoPtr->receiveBufEnd += length;
			sessionInfoPtr->pendingPacketRemaining -= length;
			}
		}
	ENSURES( sessionInfoPtr->partialHeaderRemaining == 0 );

	/* Figure out how much we can read.  If there's not enough room in the 
	   receive buffer to read at least 1K of packet data, don't try anything 
	   until the user has emptied more data from the buffer */
	bytesLeft = sessionInfoPtr->receiveBufSize - sessionInfoPtr->receiveBufEnd;
	if( bytesLeft < 1024 )
		{
		ENSURES( sanityCheckRead( sessionInfoPtr ) );

		*bytesRead = 0;
		return( CRYPT_OK );
		}
	if( bytesLeft > sessionInfoPtr->pendingPacketRemaining )
		{
		/* Limit the amount of data to read to the remaining packet size */
		bytesLeft = sessionInfoPtr->pendingPacketRemaining;
		}

	/* Try and read more of the packet */
	ENSURES( rangeCheckZ( sessionInfoPtr->receiveBufEnd, bytesLeft,
						  sessionInfoPtr->receiveBufSize ) );
	status = length = \
		sread( &sessionInfoPtr->stream,
			   sessionInfoPtr->receiveBuffer + sessionInfoPtr->receiveBufEnd,
			   bytesLeft );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	if( length <= 0 )
		{
		/* Nothing read, try again later.  This happens only if we're using
		   non-blocking reads (i.e. polled I/O), if any kind of timeout is
		   specified then we'll get a timeout error if no data is read */
		ENSURES( sanityCheckRead( sessionInfoPtr ) );

		return( 0 );
		}
	sessionInfoPtr->receiveBufEnd += length;
	sessionInfoPtr->pendingPacketRemaining -= length;
	if( sessionInfoPtr->pendingPacketRemaining > 0 )
		{
		/* We got some but not all of the data, try again later */
		*readInfo = READINFO_PARTIAL;

		ENSURES( sanityCheckRead( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}
	ENSURES( sessionInfoPtr->pendingPacketRemaining == 0 );
	ENSURES( sanityCheckRead( sessionInfoPtr ) );

	/* We've got a complete packet in the buffer, process it */
	status = length = \
		sessionInfoPtr->processBodyFunction( sessionInfoPtr, readInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Adjust the data size indicators to account for the processed packet */
	sessionInfoPtr->receiveBufEnd = sessionInfoPtr->receiveBufPos + length;
	sessionInfoPtr->receiveBufPos = sessionInfoPtr->receiveBufEnd;
	sessionInfoPtr->pendingPacketLength = 0;
	*bytesRead = length;

	ENSURES( sanityCheckRead( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

/* Get data from the remote system */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int getData( INOUT SESSION_INFO *sessionInfoPtr, 
					OUT_BUFFER( length, *bytesCopied ) BYTE *buffer, 
					IN_LENGTH const int length, 
					OUT_LENGTH_Z int *bytesCopied )
	{
	const int bytesToCopy = min( length, sessionInfoPtr->receiveBufPos );
	READSTATE_INFO readInfo;
	int bytesRead, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );
	REQUIRES( bytesToCopy >= 0 && bytesToCopy < MAX_INTLENGTH );
	REQUIRES( sanityCheckRead( sessionInfoPtr ) );

	/* Clear return values */
	memset( buffer, 0, min( 16, length ) );
	*bytesCopied = 0;

	/* Copy over as much data as we can and move any remaining data down to 
	   the start of the receive buffer.  We copy out up to receiveBufPos, 
	   the end of the decoded data, but move up to receiveBufEnd, the 
	   combined decoded data and any as-yet-undecoded partial data that
	   follows the decoded data */
	if( bytesToCopy > 0 )
		{
		const int remainder = sessionInfoPtr->receiveBufEnd - bytesToCopy;

		ENSURES( remainder >= 0 && remainder < MAX_INTLENGTH );

		memcpy( buffer, sessionInfoPtr->receiveBuffer, bytesToCopy );
		if( remainder > 0 )
			{
			/* There's decoded and/or non-decoded data left, move it down to
			   the start of the buffer */
			ENSURES( rangeCheck( bytesToCopy, remainder,
								 sessionInfoPtr->receiveBufEnd ) );
			memmove( sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBuffer + bytesToCopy, remainder );
			sessionInfoPtr->receiveBufPos -= bytesToCopy;
			sessionInfoPtr->receiveBufEnd = remainder;
			}
		else
			{
			/* We've consumed all of the data in the buffer, reset the buffer
			   information */
			sessionInfoPtr->receiveBufPos = sessionInfoPtr->receiveBufEnd = 0;
			}

		/* Remember how much we've copied and, if we've satisfied the 
		   request, exit */
		*bytesCopied = bytesToCopy;
		if( bytesToCopy >= length )
			{
			ENSURES( sanityCheckRead( sessionInfoPtr ) );

			return( CRYPT_OK );
			}
		}
	ENSURES( sessionInfoPtr->receiveBufPos == 0 );

	/* Try and read a complete packet.  This can return one of four classes 
	   of values:

		1. An error code.
		2. Zero to indicate that nothing was read (only happens on non-
		   blocking reads performing polled I/O, a blocking read will return 
		   a timeout error) or that there isn't enough room left in the read 
		   buffer to read any more.
		3a.OK_SPECIAL and read information READINFO_PARTIAL to indicate that 
		   a partial packet (not enough to process) was read.
		3b.OK_SPECIAL and read information READINFO_NOOP to indicate that a 
		   no-op packet was read and the caller should try again without 
		   changing the read timeout value.
		4. A byte count if a complete packet was read and processed */
	status = tryRead( sessionInfoPtr, &bytesRead, &readInfo );
	if( cryptStatusError( status ) && status != OK_SPECIAL )
		{
		/* If it's an internal error then the states of the by-reference
		   values may be undefined so we can't check any further */
		if( isInternalError( status ) )
			return( status );

		/* If there's an error reading data, only return an error status if 
		   we haven't already returned all existing/earlier data.  This 
		   ensures that the caller can drain out any remaining data from the 
		   session buffer before they start getting error returns */
		if( *bytesCopied <= 0 )
			{
			if( readInfo == READINFO_FATAL )
				sessionInfoPtr->readErrorState = status;
			return( status );
			}

		/* We got some data before encountering the error, if it's fatal 
		   save the pending error state for later while returning the read 
		   byte count to the caller.  Note that this results in non-fatal 
		   errors being quietly dropped if data is otherwise available, the 
		   alternative would be to save it as a pending (specially-marked) 
		   non-fatal error, however since this error type by definition can 
		   be resumed it may already have resolved itself by the next time 
		   that we're called so this is safe to do */
		if( readInfo == READINFO_FATAL )
			sessionInfoPtr->pendingReadErrorState = status;
		return( OK_SPECIAL );
		}

	/* If we read a partial packet and there's room for the rest of the 
	   packet in the buffer, set a minimum timeout to try and get the rest 
	   of the packet.  This is safe because tryRead() could have behaved in 
	   only one of two ways:

		1. Blocking read, in which case we waited for the full timeout 
		   period anyway and a small additional timeout won't be noticed.
		2. Nonblocking read, in which case waiting for a nonzero time could 
		   potentially have retrieved more data */
	if( status == OK_SPECIAL )
		{
		REQUIRES( readInfo == READINFO_PARTIAL || \
				  readInfo == READINFO_NOOP );
		if( readInfo == READINFO_PARTIAL && \
			sessionInfoPtr->pendingPacketRemaining <= \
				sessionInfoPtr->receiveBufSize - sessionInfoPtr->receiveBufEnd )
			{
			sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_READTIMEOUT, 1 );
			}

		ENSURES( sanityCheckRead( sessionInfoPtr ) );

		return( CRYPT_OK );
		}
	ENSURES( cryptStatusOK( status ) );

	/* If we got nothing, exit */
	if( bytesRead <= 0 )
		{
		ENSURES( sanityCheckRead( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}

	/* Make the stream nonblocking if it was blocking before.  This is 
	   necessary to avoid having the stream always block for the set timeout 
	   value on the last read */
	ENSURES( bytesRead > 0 && bytesRead < MAX_INTLENGTH );
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_READTIMEOUT, 0 );

	ENSURES( sanityCheckRead( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int getSessionData( INOUT SESSION_INFO *sessionInfoPtr, 
					OUT_BUFFER( dataMaxLength, *bytesCopied ) void *data, 
					IN_LENGTH const int dataMaxLength, 
					OUT_LENGTH_Z int *bytesCopied )
	{
	BYTE *dataPtr = data;
	int dataLength = dataMaxLength, iterationCount, status = CRYPT_OK;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( dataMaxLength > 0 && dataMaxLength < MAX_INTLENGTH );
	REQUIRES( sanityCheckRead( sessionInfoPtr ) );

	/* Clear return values */
	memset( data, 0, min( 16, dataMaxLength ) );
	*bytesCopied = 0;

	/* If there's an error pending (which will always be fatal, see the
	   comment after the tryRead() call in getData()) set the current error 
	   state to the pending state and return */
	if( cryptStatusError( sessionInfoPtr->pendingReadErrorState ) )
		{
		REQUIRES( sessionInfoPtr->receiveBufPos == 0 );

		status = sessionInfoPtr->readErrorState = \
						sessionInfoPtr->pendingReadErrorState;
		sessionInfoPtr->pendingReadErrorState = CRYPT_OK;
		return( status );
		}

	/* Update the stream read timeout to the current user-selected read 
	   timeout in case the user has changed the timeout setting */
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_READTIMEOUT, 
			   sessionInfoPtr->readTimeout );

	for( iterationCount = 0;
		 dataLength > 0 && iterationCount < FAILSAFE_ITERATIONS_MAX; 
		 iterationCount++ )
		{
		int count;

		/* Get the next packets-worth of data.  This can return one of three
		   classes of values:

			1. An error code.
			2. OK_SPECIAL to indicate that some data was read but no more is
			   available.
			3. CRYPT_OK to indicate that data was read and more may be 
			   available */
		status = getData( sessionInfoPtr, dataPtr, dataLength, &count );
		if( cryptStatusError( status ) && status != OK_SPECIAL )
			break;

		/* We got at least some data, update the buffer indicators */
		if( count > 0 )
			{
			*bytesCopied += count;
			dataPtr += count;
			dataLength -= count;
			}
		if( status == OK_SPECIAL )
			{
			/* The was the last of the data, exit */
			break;
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );

	ENSURES( sanityCheckRead( sessionInfoPtr ) );

	/* If we got at least some data or encountered a soft timeout then the 
	   operation was (nominally) successful, otherwise it's an error */
	return( ( *bytesCopied > 0 || status == OK_SPECIAL ) ? \
			CRYPT_OK : status );
	}

/* Read a fixed-size packet header, called by the secure data session
   routines to read the fixed header on a data packet.  There are two
   variations of this, an atomic-read readFixedHeaderAtomic() used during 
   the handshake phase that requires all data to be read and treats timeouts 
   as hard errors and a partial-read readFixedHeader() used during the 
   data-transfer phase that treats timeouts as soft errors.

   Buffer handling for the soft-timeout version is as follows:

		| <- hdrSize ->	|
		+---------------+
		|///////|		|
		+---------------+
				|<--+-->|
					|
			  partialHdrRem

   The data is read into the header buffer until partialHeaderRemaining
   drops to zero.  The function returns OK_SPECIAL until this happens */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readFixedHeaderAtomic( INOUT SESSION_INFO *sessionInfoPtr, 
						   OUT_BUFFER_FIXED( headerLength ) void *headerBuffer, 
						   IN_LENGTH_SHORT_MIN( FIXED_HEADER_MIN ) \
								const int headerLength )
	{
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( headerBuffer, sizeof( headerLength ) ) );

	REQUIRES( headerLength >= FIXED_HEADER_MIN && \
			  headerLength <= FIXED_HEADER_MAX );
	REQUIRES( sanityCheckRead( sessionInfoPtr ) );

	/* Clear return value */
	memset( headerBuffer, 0, min( 16, headerLength ) );

	/* Try and read the remaining header bytes */
	status = length = \
		sread( &sessionInfoPtr->stream, headerBuffer, headerLength );
	if( cryptStatusError( status ) )
		{
		/* We could be trying to read an ack for a close packet sent in 
		   response to an earlier error, in which case we don't want the
		   already-present error information overwritten by network
		   error information, so if the no-report-error flag is set we 
		   don't update the extended error information */
		if( sessionInfoPtr->flags & SESSION_NOREPORTERROR )
			return( status );

		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}

	/* We've timed out during the handshake phase, it's a hard timeout 
	   error */
	if( length != headerLength )
		{
		if( sessionInfoPtr->flags & SESSION_NOREPORTERROR )
			return( status );
		retExt( CRYPT_ERROR_TIMEOUT,
				( CRYPT_ERROR_TIMEOUT, SESSION_ERRINFO, 
				  "Timeout during packet header read, only got %d of %d "
				  "bytes", length, headerLength ) );
		}
	ENSURES( sanityCheckRead( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readFixedHeader( INOUT SESSION_INFO *sessionInfoPtr, 
					 OUT_BUFFER_FIXED( headerLength ) void *headerBuffer, 
					 IN_LENGTH_SHORT_MIN( FIXED_HEADER_MIN ) \
							const int headerLength )
	{
	BYTE *bufPtr = headerBuffer;
	int bytesToRead, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( headerBuffer, sizeof( headerLength ) ) );

	REQUIRES( headerLength >= FIXED_HEADER_MIN && \
			  headerLength <= FIXED_HEADER_MAX );
	REQUIRES( sanityCheckRead( sessionInfoPtr ) );

	/* We can't clear the return value at this point because there may 
	   already be a partial header present in the buffer */

	/* If it's the first attempt at reading the header, set the total byte
	   count */
	if( sessionInfoPtr->partialHeaderRemaining <= 0 )
		{
		sessionInfoPtr->partialHeaderRemaining = headerLength;
		bytesToRead = headerLength;
		}
	else
		{
		/* We've already got a partial header present in the buffer, read 
		   the remaining header data.  Note that the existing partial header 
		   size may be zero (i.e. partialHeaderRemaining == headerLength) 
		   if we got a soft-timeout on a previous call to readFixedHeader().  
		   This happens on any read in which the peer has sent only a single 
		   packet and the packet fits entirely in the read buffer, and occurs 
		   because we follow up every full packet read with an opportunistic 
		   zero-timeout second read to check if further packets are 
		   pending */
		bufPtr += headerLength - sessionInfoPtr->partialHeaderRemaining;
		bytesToRead = sessionInfoPtr->partialHeaderRemaining;
		}
	ENSURES( bytesToRead > 0 && bytesToRead <= headerLength );
	ENSURES( sessionInfoPtr->partialHeaderRemaining > 0 && \
			 sessionInfoPtr->partialHeaderRemaining <= headerLength );

	/* Now we can clear the return value */
	memset( bufPtr, 0, min( 16, bytesToRead ) );

	/* Try and read the remaining header bytes */
	ENSURES( rangeCheckZ( headerLength - sessionInfoPtr->partialHeaderRemaining,
						  bytesToRead, headerLength ) );
	status = length = \
		sread( &sessionInfoPtr->stream, bufPtr, bytesToRead );
	if( cryptStatusError( status ) )
		{
		/* We could be trying to read an ack for a close packet sent in 
		   response to an earlier error, in which case we don't want the
		   already-present error information overwritten by network
		   error information, so if the no-report-error flag is set we 
		   don't update the extended error information */
		if( sessionInfoPtr->flags & SESSION_NOREPORTERROR )
			return( status );

		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	sessionInfoPtr->partialHeaderRemaining -= length;

	/* If we didn't get the whole header, treat it as a soft timeout error */
	if( sessionInfoPtr->partialHeaderRemaining > 0 )
		{
		ENSURES( sanityCheckRead( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}

	/* We've got the whole header ready to process */
	ENSURES( sessionInfoPtr->partialHeaderRemaining == 0 );

	ENSURES( sanityCheckRead( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Secure Session Data Write Functions					*
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

	REQUIRES( currentByteCount >= 0 && \
			  currentByteCount <= sessionInfoPtr->maxPacketSize && \
			  currentByteCount < MAX_INTLENGTH );
	remainingByteCount = sessionInfoPtr->maxPacketSize - currentByteCount;
	ENSURES( remainingByteCount >= 0 && remainingByteCount  < MAX_INTLENGTH );

	return( remainingByteCount );
	}

/* Send data to the remote system.  There are two strategies for handling 
   buffer filling and partial writes, either to fill the buffer as full as 
   possible and write it all at once, or to write complete packets as soon 
   as they're available.  We use the latter strategy both because it 
   considerably simplifies buffer management and because interleaving 
   (asynchronous) writes and packet processing increases the chances that 
   the current packet will be successfully dispatched across the network 
   while the next one is being encrypted - trying to asynchronously write a 
   large amount of data in one go practically guarantees that the write 
   won't complete.

   Session buffer management is handled as follows: The startOfs index 
   points to the start of the payload space in the buffer (everything before 
   this is header data).  The maxPacketSize value indicates the end of the 
   payload space relative to the startOfs:

	<- hdr->|<-- payload -->|
	+-------+---------------+---+
	|		|///////////////|	|
	+-------+---------------+---+
			^				^
			|				|
		startOfs	  maxPacketSize

   The bufPos index moves from startsOfs to maxPacketSize after which the 
   data is wrapped up by the protocol-specific code.  At this point bufPos
   usually points past the end of maxPacketSize due to the addition of
   trailer data such as encryption block padding and a MAC.  Once the
   packet is assembled, the data is flushed and the bufPos index reset to
   startOfs:

		startOfs	  maxPacketSize
			|				|
			v				v
	+-------+-------+-------+---+
	|.......|.......|///////|///|
	+-------+-------+-------+---+
	|<-- writtem -->^<--- to -->^
					|	write	|
			  partialBufPos	  bufPos

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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int flushData( SESSION_INFO *sessionInfoPtr )
	{
	int bytesToWrite, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckWrite( sessionInfoPtr ) );

	/* If there's no data to flush, exit */
	if( sessionInfoPtr->sendBufPos <= sessionInfoPtr->sendBufStartOfs )
		return( CRYPT_OK );	

	/* If there's no unwritten data from a previous write attempt still 
	   present, prepare to send the new data */
	if( !sessionInfoPtr->partialWrite )
		{
		ENSURES( sessionInfoPtr->sendBufPartialBufPos == 0 );

		status = length = \
			sessionInfoPtr->preparePacketFunction( sessionInfoPtr );
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
	ENSURES( bytesToWrite > 0 && bytesToWrite < MAX_INTLENGTH );

	/* Send the data through to the remote system */
	ENSURES( rangeCheckZ( sessionInfoPtr->sendBufPartialBufPos, bytesToWrite,
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
		ENSURES( sanityCheckWrite( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}
	if( length < bytesToWrite )
		{
		/* We wrote at least some part of the packet, adjust the partial-
		   write position by the amount that we wrote */
		sessionInfoPtr->sendBufPartialBufPos += length;
		sessionInfoPtr->partialWrite = TRUE;
		ENSURES( sanityCheckWrite( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}

	ENSURES( length == bytesToWrite );

	/* We sent everything, reset the buffer status values */
	sessionInfoPtr->sendBufPos = sessionInfoPtr->sendBufStartOfs;
	sessionInfoPtr->partialWrite = FALSE;
	sessionInfoPtr->sendBufPartialBufPos = 0;

	ENSURES( sanityCheckWrite( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int putSessionData( INOUT SESSION_INFO *sessionInfoPtr, 
					IN_BUFFER_OPT( dataLength ) const void *data,
					IN_LENGTH_Z const int dataLength, 
					OUT_LENGTH_Z int *bytesCopied )
	{
	BYTE *dataPtr = ( BYTE * ) data;
	int length = dataLength, availableBuffer, iterationCount, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( data == NULL || isReadPtr( data, dataLength ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( data != NULL && \
				dataLength > 0 && dataLength < MAX_INTLENGTH ) );
	REQUIRES( sanityCheckWrite( sessionInfoPtr ) );

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
	   timeout in case the user has changed the timeout setting */
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
	for( iterationCount = 0;
		 length >= availableBuffer && \
		 	iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
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
			ENSURES( rangeCheck( sessionInfoPtr->sendBufPos, availableBuffer,
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
				ENSURES( sanityCheckWrite( sessionInfoPtr ) );

				return( CRYPT_OK );
				}

			/* There was a problem flushing the data through, if we managed 
			   to copy anything into the buffer we've made some progress so 
			   we defer it until the next call */
			if( *bytesCopied > 0 )
				{
				sessionInfoPtr->pendingWriteErrorState = status;

				ENSURES( sanityCheckWrite( sessionInfoPtr ) );

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
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* If there's anything left, it'll fit completely into the send buffer, 
	   just copy it in */
	if( length > 0 )
		{
		ENSURES( length < availableBuffer );

		ENSURES( rangeCheck( sessionInfoPtr->sendBufPos, length, 
							 sessionInfoPtr->maxPacketSize ) );
		memcpy( sessionInfoPtr->sendBuffer + sessionInfoPtr->sendBufPos,
				dataPtr, length );
		sessionInfoPtr->sendBufPos += length;
		*bytesCopied += length;
		}

	ENSURES( sanityCheckWrite( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*				Request/response Session Data Handling Functions			*
*																			*
****************************************************************************/

/* Read/write a PKI (i.e. ASN.1-encoded) datagram.  Unlike the secure 
   session protocols these operations are always atomic so the read/write
   process is much simpler */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readPkiDatagram( INOUT SESSION_INFO *sessionInfoPtr )
	{
	HTTP_DATA_INFO httpDataInfo;
	int length = DUMMY_INIT, complianceLevel, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Some servers send back sufficiently broken responses that they won't
	   pass the validity check on the data that we perform after we read it, 
	   so we allow it to be disabled by setting the compliance level to
	   oblivious */
	status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, 
							  IMESSAGE_GETATTRIBUTE, &complianceLevel, 
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		complianceLevel = CRYPT_COMPLIANCELEVEL_STANDARD;

	/* Read the datagram */
	sessionInfoPtr->receiveBufEnd = 0;
	initHttpDataInfo( &httpDataInfo, sessionInfoPtr->receiveBuffer,
					  sessionInfoPtr->receiveBufSize );
	status = sread( &sessionInfoPtr->stream, &httpDataInfo,
					sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	length = httpDataInfo.bytesAvail;
	if( length < 4 || length >= MAX_INTLENGTH )
		{
		/* Perform a sanity check on the length.  This avoids triggering
		   assertions in the debug build and provides somewhat more specific 
		   information for the caller than the invalid-encoding error that 
		   we'd get later */
		retExt( CRYPT_ERROR_UNDERFLOW,
				( CRYPT_ERROR_UNDERFLOW, SESSION_ERRINFO, 
				  "Invalid PKI message length %d", status ) );
		}

	/* Find out how much data we got and perform a firewall check that
	   everything is OK.  We rely on this rather than the read byte count
	   since checking the ASN.1, which is the data that will actually be
	   processed, avoids any potential problems with implementations where
	   the transport layer gets data lengths slightly wrong */
	if( complianceLevel > CRYPT_COMPLIANCELEVEL_OBLIVIOUS )
		{
		status = length = checkObjectEncoding( sessionInfoPtr->receiveBuffer, 
											   length );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, SESSION_ERRINFO, 
					  "Invalid PKI message encoding" ) );
			}
		}
	sessionInfoPtr->receiveBufEnd = length;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writePkiDatagram( INOUT SESSION_INFO *sessionInfoPtr, 
					  IN_BUFFER_OPT( contentTypeLen ) \
							const char *contentType, 
					  IN_LENGTH_TEXT_Z const int contentTypeLen )
	{
	HTTP_DATA_INFO httpDataInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( contentType == NULL || \
			isReadPtr( contentType, contentTypeLen ) );

	REQUIRES( ( contentType == NULL && contentTypeLen ) || \
			  ( contentType != NULL && \
				contentTypeLen > 0 && contentTypeLen <= CRYPT_MAX_TEXTSIZE ) );
	REQUIRES( sessionInfoPtr->receiveBufEnd > 4 && \
			  sessionInfoPtr->receiveBufEnd < MAX_INTLENGTH );

	/* Write the datagram.  Request/response sessions use a single buffer 
	   for both reads and writes, which is why we're (apparently) writing
	   the contents of the read buffer */
	initHttpDataInfo( &httpDataInfo, sessionInfoPtr->receiveBuffer,
					  sessionInfoPtr->receiveBufEnd );
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
#endif /* USE_SESSIONS */
