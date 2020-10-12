/****************************************************************************
*																			*
*					cryptlib Session Read Support Routines					*
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

/* Common code to read data over a secure connection.  This is called by the 
   protocol-specific handlers, which supply three functions of which two are 
   used for read purposes:

	readHeaderFunction()	- Reads the header for a packet and sets up
							  length information.
	processBodyFunction()	- Processes the body of a packet.

   The behaviour of the network-level stream handlers when called with given 
   timeout and byte-count values is as follows:

	Timeout		byteCount		Result
	-------		---------		------
		  - error -				error
	  0			  0				0
	  0			> 0				byteCount
	> 0			  0				CRYPT_ERROR_TIMEOUT
	> 0			> 0				byteCount

   Errors encountered in processBodyFunction() are always fatal.  In theory 
   we could try to recover, however the functions update assorted crypto 
   state such as packet sequence numbers and IVs that would be tricky to 
   roll back, and in practice recoverable errors are likely to be extremely 
   rare (at best perhaps a CRYPT_ERROR_TIMEOUT for a context tied to a 
   device, however even this won't occur since the conventional encryption 
   and MAC contexts are all internal native contexts) so there's little 
   point in trying to make the functions recoverable */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the session state.  This function is called both from the
   higher-level sanityCheckSession() and indirectly from lower-level read 
   functions as a pre/postcondition on read operations */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckSessionRead( const SESSION_INFO *sessionInfoPtr )
	{
	const int pendingPacketLength = sessionInfoPtr->pendingPacketLength;
	const int pendingPacketRemaining = sessionInfoPtr->pendingPacketRemaining;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that the general state is in order */
	if( sessionInfoPtr->receiveBufSize < MIN_BUFFER_SIZE || \
		sessionInfoPtr->receiveBufSize >= MAX_BUFFER_SIZE )
		{
		DEBUG_PUTS(( "sanityCheckSessionRead: Receive buffer size" ));
		return( FALSE );
		}
	if( sessionInfoPtr->receiveBuffer != NULL && \
		!safeBufferCheck( sessionInfoPtr->receiveBuffer, 
						  sessionInfoPtr->receiveBufSize ) )
		{
		DEBUG_PUTS(( "sanityCheckSessionRead: Receive buffer state" ));
		return( FALSE );
		}
	if( sessionInfoPtr->type == CRYPT_SESSION_SSH || \
		sessionInfoPtr->type == CRYPT_SESSION_SSH_SERVER || \
		sessionInfoPtr->type == CRYPT_SESSION_SSL || \
		sessionInfoPtr->type == CRYPT_SESSION_SSL_SERVER )
		{
		if( sessionInfoPtr->maxPacketSize < PACKET_SIZE_MIN || \
			sessionInfoPtr->maxPacketSize > PACKET_SIZE_MAX )
			{
			DEBUG_PUTS(( "sanityCheckSessionRead: Max packet size" ));
			return( FALSE );
			}
		}
	else
		{
		if( sessionInfoPtr->maxPacketSize != 0 )
			{
			DEBUG_PUTS(( "sanityCheckSessionRead: Spurious max packet size" ));
			return( FALSE );
			}
		}

	/* Make sure that the buffer position values are within bounds.  The 
	   difference between receiveBufStartOfs and partialHeaderRemaining
	   is that the former records a constant value, the position at the
	   start of the buffer where the metadata ends, while the latter records 
	   a variable, the amount of metadata currently present */
	if( sessionInfoPtr->receiveBufEnd < 0 || \
		sessionInfoPtr->receiveBufEnd > sessionInfoPtr->receiveBufSize || \
		sessionInfoPtr->receiveBufPos < 0 || \
		sessionInfoPtr->receiveBufPos > sessionInfoPtr->receiveBufEnd )
		{
		DEBUG_PUTS(( "sanityCheckSessionRead: Receive buffer info" ));
		return( FALSE );
		}
	if( sessionInfoPtr->receiveBufStartOfs < 0 || \
		sessionInfoPtr->receiveBufStartOfs > FIXED_HEADER_MAX || \
		sessionInfoPtr->partialHeaderRemaining < 0 || \
		sessionInfoPtr->partialHeaderRemaining > FIXED_HEADER_MAX )
		{
		DEBUG_PUTS(( "sanityCheckSessionRead: Receive buffer packet info" ));
		return( FALSE );
		}

	/* If we haven't started processing data yet then there's no packet 
	   information present */
	if( pendingPacketLength == 0 && pendingPacketRemaining == 0 )
		return( TRUE );

	/* Make sure that packet information is within bounds */
	if( pendingPacketLength < 0 || \
		pendingPacketLength >= sessionInfoPtr->receiveBufSize || \
		pendingPacketRemaining < 0 || \
		pendingPacketRemaining >= sessionInfoPtr->receiveBufSize )
		{
		DEBUG_PUTS(( "sanityCheckSessionRead: Pending packet info" ));
		return( FALSE );
		}
	if( ( sessionInfoPtr->receiveBufEnd - \
		  sessionInfoPtr->receiveBufPos ) + pendingPacketRemaining != \
		pendingPacketLength )
		{
		DEBUG_PUTS(( "sanityCheckSessionRead: Pending packet calculation" ));
		return( FALSE );
		}

	return( TRUE );
	}

/* Process an inner protocol's data packets.  This iterates through the 
   payload contents until it's all been consumed, potentially handling
   multiple packets in one go.  So for example an input buffer that
   contained (H = header, O = out-of-band protocol packet):

   bufPtr								 bufEnd
	|										|
	v										v
	+-+---------+-+---------+-+-+---------+-+-------+
	|H|/////////|H|\\\\\\\\\|O|H|~~~~~~~~~|O|		|
	+-+---------+-+---------+-+-+---------+-+-------+

   would be converted in turn into:

	bytes-	bufPtr						bufEnd
	processed |							  |
	|<------->v							  v
	+---------+-+---------+-+-+---------+-+---------+
	|/////////|H|\\\\\\\\\|O|H|~~~~~~~~~|O|			|
	+---------+-+---------+-+-+---------+-+---------+

			  bytes	  bufPtr		  bufEnd
			  processed	v				|
			  |<------->|				V
	+---------+---------+-+-+---------+-+-----------+
	|/////////|\\\\\\\\\|O|H|~~~~~~~~~|O|			|
	+---------+---------+-+-+---------+-+-----------+

					  bufPtr		bufEnd
						|			  |
						v			  v
	+---------+---------+-+---------+-+-------------+
	|/////////|\\\\\\\\\|H|~~~~~~~~~|O|				|
	+---------+---------+-+---------+-+-------------+

						bytes	bPtr/bEnd
						processed |	|
						|<------->v	v
	+---------+---------+---------+-+---------------+
	|/////////|\\\\\\\\\|~~~~~~~~~|O|				|
	+---------+---------+---------+-+---------------+

	+---------+---------+---------+-----------------+
	|/////////|\\\\\\\\\|~~~~~~~~~|					|
	+---------+---------+---------+-----------------+ 

   Note that in some cases bytesProcessed can be zero if an out-of-band 
   packet with no payload is removed */

#define MAX_PACKETS			16

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processInnerProtocolData( INOUT SESSION_INFO *sessionInfoPtr,
									 INOUT_BUFFER_FIXED( bufSize ) \
										BYTE *buffer,
									 IN_DATALENGTH const int bufSize )
	{
	BYTE *bufPtr = buffer;
	int bufEnd = bufSize, totalBytesProcessed, noPackets, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( buffer, bufSize ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( bufSize > 0 && bufSize < MAX_BUFFER_SIZE );

	/* Walk down the buffer processing each packet in it in turn */
	LOOP_MED( ( totalBytesProcessed = 0, noPackets = 0 ), 
			  totalBytesProcessed < bufEnd && noPackets < MAX_PACKETS, 
			  noPackets++ )
		{
		int bytesProcessed, status;

#if 1
		status = bytesProcessed = \
			processInnerPacketFunction( sessionInfoPtr, bufPtr, bufEnd, 
										&bufEnd );
#else
		status = bytesProcessed = \
			processInnerPacketFunction( sessionInfoPtr, bufPtr, 3,//bufEnd, 
										&bufEnd );
		if( status == OK_SPECIAL )	bytesProcessed = 3;
		bufPtr += bytesProcessed;
		status = bytesProcessed = \
			processInnerPacketFunction( sessionInfoPtr, bufPtr, 3,//bufEnd, 
										&bufEnd );
		if( status == OK_SPECIAL )	bytesProcessed = 3;
		bufPtr += bytesProcessed;
		status = bytesProcessed = \
			processInnerPacketFunction( sessionInfoPtr, bufPtr, 2,//bufEnd, 
										&bufEnd );
		if( status == OK_SPECIAL )	bytesProcessed = 2;
		bufPtr += bytesProcessed;
		status = bytesProcessed = \
			processInnerPacketFunction( sessionInfoPtr, bufPtr, 2,//bufEnd, 
										&bufEnd );
		if( status == OK_SPECIAL )	bytesProcessed = 2;
		bufPtr += bytesProcessed;
		status = bytesProcessed = \
			processInnerPacketFunction( sessionInfoPtr, bufPtr, 5,//bufEnd, 
										&bufEnd );
#endif
		if( cryptStatusError( status ) )
			{
			/* If the packet processing returns OK_SPECIAL then all the data
			   in the buffer has been consumed */
			if( status == OK_SPECIAL )
				break;

			return( status );
			}
		totalBytesProcessed += bytesProcessed;
		bufPtr += bytesProcessed;
		}
	ENSURES( LOOP_BOUND_OK );
	if( noPackets >= MAX_PACKETS )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Encountered more than %d inner protocol packets",
				  noPackets ) );
		}

	return( bufEnd );
	}

/* Data injection point for fuzzing the inner protocol */

#ifdef CONFIG_FUZZ

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int fuzzInnerProtocol( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT_BUFFER_FIXED( bufSize ) BYTE *buffer,
					   IN_DATALENGTH const int bufSize )
	{
	return( processInnerProtocolData( sessionInfoPtr, buffer, bufSize ) );
	}
#endif /* CONFIG_FUZZ */

/****************************************************************************
*																			*
*								Header-Read Functions						*
*																			*
****************************************************************************/

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
			partialHdrRemaining

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
	assert( isWritePtr( headerBuffer, headerLength ) );

	REQUIRES( sanityCheckSessionRead( sessionInfoPtr ) );
	REQUIRES( headerLength >= FIXED_HEADER_MIN && \
			  headerLength <= FIXED_HEADER_MAX );

	/* Clear return value */
	memset( headerBuffer, 0, min( 16, headerLength ) );

	/* Try and read the header bytes */
	status = length = \
		sread( &sessionInfoPtr->stream, headerBuffer, headerLength );
	if( cryptStatusError( status ) )
		{
		/* We could be trying to read an ack for a close packet sent in 
		   response to an earlier error, in which case we don't want the
		   already-present error information overwritten by network
		   error information, so if the no-report-error flag is set we 
		   don't update the extended error information */
		if( TEST_FLAG( sessionInfoPtr->flags, 
					   SESSION_FLAG_NOREPORTERROR ) )
			return( status );

		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}

	/* We've timed out during the handshake phase, it's a hard timeout 
	   error */
	if( length != headerLength )
		{
		/* See the comment above for the early-exit condition */
		if( TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_NOREPORTERROR ) )
			return( status );

		retExt( CRYPT_ERROR_TIMEOUT,
				( CRYPT_ERROR_TIMEOUT, SESSION_ERRINFO, 
				  "Timeout during packet header read, only got %d of %d "
				  "bytes", length, headerLength ) );
		}
	ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

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

	REQUIRES( sanityCheckSessionRead( sessionInfoPtr ) );
	REQUIRES( headerLength >= FIXED_HEADER_MIN && \
			  headerLength <= FIXED_HEADER_MAX );

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
	ENSURES( boundsCheckZ( headerLength - sessionInfoPtr->partialHeaderRemaining,
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
		if( TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_NOREPORTERROR ) )
			return( status );

		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	sessionInfoPtr->partialHeaderRemaining -= length;

	/* If we didn't get the whole header, treat it as a soft timeout error */
	if( sessionInfoPtr->partialHeaderRemaining > 0 )
		{
		ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}

	/* We've got the whole header ready to process */
	ENSURES( sessionInfoPtr->partialHeaderRemaining == 0 );

	ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Data Read Functions						*
*																			*
****************************************************************************/

/* The read data code uses a helper function tryRead() that either reads
   everything which is available or to the end of the current packet.  In
   other words it's an atomic, all-or-nothing function that can be used by
   higher-level code to handle network-level packetisation.  
   
   Buffer management is handled as follows: The receiveBufPos index always 
   points to the end of the decoded data (i.e. data that can be used by the 
   user), if there's no partial packet present this index is the same as 
   receiveBufEnd:

   rbPos/rbEnd
		|
		v
	----+------------------------
	....|
	----+------------------------

   After readHeaderFunction() has been called pendingPacketRemaining 
   contains the number of bytes required to complete the packet of length
   pendingPacketLength:
  
   rbPos/rbEnd
		|
		v
	----+-----------------------+----
	....|						|
	----+-----------------------+----
		|<---- pPL == pPR ----->|

   tryRead() then attempts to fill the buffer with the packet data, with 
   receiveBufEnd pointing to the end of the received data and advancing as 
   more data is read:

	  rbPos			  rbEnd
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

	  rbPos					  rbEnd
		|						|
		v						v
	----+-----------------------+----
	....|///////////////////////|
	----+-----------------------+----
		|<-------- pPL -------->|
								| pPR = 0

   If processBody() completes successfully then receiveBufPos and 
   receiveBufEnd are adjusted to point to the end of the new data:

							rbPos/rbEnd
								|
								v
	----+-----------------------+----
	....|.......................|
	----+-----------------------+----

   The handling of any header data present at the start of the packet
   depends on the packet format.  If the header is independent of the
   encrypted data then it's handled entirely by the readHeaderFunction() and 
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

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int tryRead( INOUT SESSION_INFO *sessionInfoPtr, 
					OUT_DATALENGTH_Z int *bytesRead,
					OUT_ENUM_OPT( READINFO ) READSTATE_INFO *readInfo )
	{
	SES_READHEADER_FUNCTION readHeaderFunction;
	SES_PROCESSBODY_FUNCTION processBodyFunction;
	int length, bytesLeft, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( bytesRead, sizeof( int ) ) );
	assert( isWritePtr( readInfo, sizeof( READSTATE_INFO ) ) );

	REQUIRES( sanityCheckSessionRead( sessionInfoPtr ) );

	readHeaderFunction = ( SES_READHEADER_FUNCTION ) \
						 FNPTR_GET( sessionInfoPtr->readHeaderFunction );
	processBodyFunction = ( SES_PROCESSBODY_FUNCTION ) \
						  FNPTR_GET( sessionInfoPtr->processBodyFunction );
	REQUIRES( readHeaderFunction != NULL );
	REQUIRES( processBodyFunction != NULL );

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
		status = length = readHeaderFunction( sessionInfoPtr, readInfo );
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
			/* If nothing was read then we're done */
			if( length <= 0 )
				{
				ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

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
		ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

		*bytesRead = 0;
		return( CRYPT_OK );
		}
	if( bytesLeft > sessionInfoPtr->pendingPacketRemaining )
		{
		/* Limit the amount of data to read to the remaining packet size */
		bytesLeft = sessionInfoPtr->pendingPacketRemaining;
		}

	/* Try and read more of the packet */
	ENSURES( boundsCheckZ( sessionInfoPtr->receiveBufEnd, bytesLeft,
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
		ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

		return( 0 );
		}
	sessionInfoPtr->receiveBufEnd += length;
	sessionInfoPtr->pendingPacketRemaining -= length;
	if( sessionInfoPtr->pendingPacketRemaining > 0 )
		{
		/* We got some but not all of the data, try again later */
		*readInfo = READINFO_PARTIAL;

		ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}
	ENSURES( sessionInfoPtr->pendingPacketRemaining == 0 );
	ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

	/* We've got a complete packet in the buffer, process it */
	status = length = processBodyFunction( sessionInfoPtr, readInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* If there's a layered protocol inside the outer one, process the inner
	   protocol data */
	if( TEST_FLAG( sessionInfoPtr->flags, 
				   SESSION_FLAG_SUBPROTOCOL_ACTIVE ) )
		{
		status = length = \
			processInnerProtocolData( sessionInfoPtr,
									  sessionInfoPtr->receiveBuffer + \
										sessionInfoPtr->receiveBufPos, 
									  length );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Adjust the data size indicators to account for the processed packet */
	sessionInfoPtr->receiveBufEnd = sessionInfoPtr->receiveBufPos + length;
	sessionInfoPtr->receiveBufPos = sessionInfoPtr->receiveBufEnd;
	sessionInfoPtr->pendingPacketLength = 0;
	*bytesRead = length;

	ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

/* Get data from the remote system */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int getData( INOUT SESSION_INFO *sessionInfoPtr, 
					OUT_BUFFER( length, *bytesCopied ) BYTE *buffer, 
					IN_DATALENGTH const int length, 
					OUT_DATALENGTH_Z int *bytesCopied )
	{
	const int bytesToCopy = min( length, sessionInfoPtr->receiveBufPos );
	READSTATE_INFO readInfo;
	int bytesRead, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( buffer, length ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( sanityCheckSessionRead( sessionInfoPtr ) );
	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE );
	REQUIRES( bytesToCopy >= 0 && bytesToCopy < MAX_BUFFER_SIZE );

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

		ENSURES( remainder >= 0 && remainder < MAX_BUFFER_SIZE );

		REQUIRES( rangeCheck( bytesToCopy, 1, length ) );
		memcpy( buffer, sessionInfoPtr->receiveBuffer, bytesToCopy );
		if( remainder > 0 )
			{
			/* There's decoded and/or non-decoded data left, move it down to
			   the start of the buffer */
			REQUIRES( boundsCheck( bytesToCopy, remainder,
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
			ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

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
		/* If it's a crypto-related error, register it so that 
		   countermeasures can be taken */
		if( readInfo == READINFO_FATAL_CRYPTO )
			{
			registerCryptoFailure();
			readInfo = READINFO_FATAL;
			}

		/* If it's an internal error then the states of the by-reference
		   values may be undefined (the internal error could have been 
		   triggered before the by-reference values could be cleared) so we 
		   can't check any further */
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

		ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

		return( CRYPT_OK );
		}
	ENSURES( cryptStatusOK( status ) );

	/* If we got nothing, exit */
	if( bytesRead <= 0 )
		{
		ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

		return( OK_SPECIAL );
		}
	ENSURES( bytesRead > 0 && bytesRead < MAX_BUFFER_SIZE );

	/* Make the stream nonblocking if it was blocking before.  This is 
	   necessary to avoid having the stream always block for the set timeout 
	   value on the last read */
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_READTIMEOUT, 0 );

	ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int getSessionData( INOUT SESSION_INFO *sessionInfoPtr, 
					OUT_BUFFER( dataMaxLength, *bytesCopied ) void *data, 
					IN_DATALENGTH const int dataMaxLength, 
					OUT_DATALENGTH_Z int *bytesCopied )
	{
	BYTE *dataPtr = data;
	int dataLength = dataMaxLength, status = CRYPT_OK, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( dataMaxLength > 0 && dataMaxLength < MAX_BUFFER_SIZE );

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
	   timeout in case the user has changed the timeout setting.
	   
	   This isn't perfect in the case of the extremely chatty SSH protocol 
	   because what looks like a read to the user can involve several reads 
	   and writes under the hood, and what gets used for those is the 
	   overall high-level timeout for the operation as a whole, with the 
	   hidden internal reads and writes using that as their shared setting.  
	   
	   For example if the code performs a read and hits an SSH channel 
	   command it has to send back a response, leading possibly to further 
	   reads and writes, before it can actually read any data.  So the top-
	   level read command sets a read timeout and then the much lower-level 
	   code ends up in a hidden long exchange with the other side using the
	   same timeout that was set for the overall read before the higher-
	   level read code gets to complete the actual read */
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_READTIMEOUT, 
			   sessionInfoPtr->readTimeout );

	LOOP_MAX_INITCHECK( dataLength = dataMaxLength, dataLength > 0 )
		{
		int byteCount;

		/* Get the next packets-worth of data.  This can return one of three
		   classes of values:

			1. An error code.
			2. OK_SPECIAL to indicate that some data was read but no more is
			   available.
			3. CRYPT_OK to indicate that data was read and more may be 
			   available */
		status = getData( sessionInfoPtr, dataPtr, dataLength, &byteCount );
		if( cryptStatusError( status ) && status != OK_SPECIAL )
			break;

		/* We got at least some data, update the buffer indicators */
		if( byteCount > 0 )
			{
			*bytesCopied += byteCount;
			dataPtr += byteCount;
			dataLength -= byteCount;
			}
		if( status == OK_SPECIAL )
			{
			/* That was the last of the data, exit */
			break;
			}
		}
	ENSURES( LOOP_BOUND_OK );

	ENSURES( sanityCheckSessionRead( sessionInfoPtr ) );

	/* If we got at least some data or encountered a soft timeout then the 
	   operation was (nominally) successful, otherwise it's an error */
	return( ( *bytesCopied > 0 || status == OK_SPECIAL ) ? \
			CRYPT_OK : status );
	}

/****************************************************************************
*																			*
*				Request/response Session Data Read Functions				*
*																			*
****************************************************************************/

#ifdef USE_CERTIFICATES

/* Read a PKI (i.e. ASN.1-encoded) datagram.  Unlike the secure session 
   protocols these operations are always atomic so the read process is much 
   simpler */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readPkiDatagram( INOUT SESSION_INFO *sessionInfoPtr,
					 IN_LENGTH_SHORT_MIN( 4 ) const int minMessageSize )
	{
	HTTP_DATA_INFO httpDataInfo;
	int length DUMMY_INIT, complianceLevel, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( minMessageSize >= 4 && minMessageSize < MAX_INTLENGTH_SHORT );

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
	status = initHttpInfoRead( &httpDataInfo, 
							   sessionInfoPtr->receiveBuffer,
							   sessionInfoPtr->receiveBufSize );
	ENSURES( cryptStatusOK( status ) );
	status = sread( &sessionInfoPtr->stream, &httpDataInfo,
					sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	length = httpDataInfo.bytesAvail;

	/* Perform a sanity check on the length.  This avoids triggering 
	   assertions in the debug build and provides somewhat more specific 
	   information for the caller than the invalid-encoding error that we'd 
	   get later */
	if( length < minMessageSize || length >= MAX_BUFFER_SIZE )
		{
		retExt( CRYPT_ERROR_UNDERFLOW,
				( CRYPT_ERROR_UNDERFLOW, SESSION_ERRINFO, 
				  "Invalid PKI message length %d, should be %d to %d", 
				  length, minMessageSize, MAX_BUFFER_SIZE ) );
		}

	/* Find out how much data we got and perform a firewall check that
	   everything is OK.  We rely on this rather than the read byte count
	   since checking the ASN.1, which is the data that will actually be
	   processed, avoids any potential problems with implementations where
	   the transport layer gets data lengths slightly wrong */
	if( complianceLevel > CRYPT_COMPLIANCELEVEL_OBLIVIOUS )
		{
#ifdef CONFIG_FUZZ
		status = getObjectLength( sessionInfoPtr->receiveBuffer, length, 
								  &length );
#else
		status = checkObjectEncodingLength( sessionInfoPtr->receiveBuffer, 
											length, &length );
#endif /* CONFIG_FUZZ */
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, SESSION_ERRINFO, 
					  "Invalid PKI message encoding" ) );
			}
		}

	/* Perform the same check as before, but this time on the real object 
	   size rather than the amount of data read */
	if( length < minMessageSize || length >= MAX_BUFFER_SIZE )
		{
		retExt( CRYPT_ERROR_UNDERFLOW,
				( CRYPT_ERROR_UNDERFLOW, SESSION_ERRINFO, 
				  "Invalid PKI message length %d, should be %d to %d", 
				  length, minMessageSize, MAX_BUFFER_SIZE ) );
		}

	sessionInfoPtr->receiveBufEnd = length;
	return( CRYPT_OK );
	}
#endif /* USE_CERTIFICATES */
#endif /* USE_SESSIONS */
