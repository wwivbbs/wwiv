/****************************************************************************
*																			*
*					  cryptlib Datagram Decoding Routines					*
*						Copyright Peter Gutmann 1996-2013					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "misc_rw.h"
  #include "pgp_rw.h"
  #include "envelope.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/misc_rw.h"
  #include "enc_dec/pgp_rw.h"
  #include "envelope/envelope.h"
#endif /* Compiler-specific includes */

/*			 .... NO! ...				   ... MNO! ...
		   ..... MNO!! ...................... MNNOO! ...
		 ..... MMNO! ......................... MNNOO!! .
		.... MNOONNOO!	 MMMMMMMMMMPPPOII!	 MNNO!!!! .
		 ... !O! NNO! MMMMMMMMMMMMMPPPOOOII!! NO! ....
			...... ! MMMMMMMMMMMMMPPPPOOOOIII! ! ...
		   ........ MMMMMMMMMMMMPPPPPOOOOOOII!! .....
		   ........ MMMMMOOOOOOPPPPPPPPOOOOMII! ...
			....... MMMMM..	   OPPMMP	 .,OMI! ....
			 ...... MMMM::	 o.,OPMP,.o	  ::I!! ...
				 .... NNM:::.,,OOPM!P,.::::!! ....
				  .. MMNNNNNOOOOPMO!!IIPPO!!O! .....
				 ... MMMMMNNNNOO:!!:!!IPPPPOO! ....
				   .. MMMMMNNOOMMNNIIIPPPOO!! ......
				  ...... MMMONNMMNNNIIIOO!..........
			   ....... MN MOMMMNNNIIIIIO! OO ..........
			......... MNO! IiiiiiiiiiiiI OOOO ...........
		  ...... NNN.MNO! . O!!!!!!!!!O . OONO NO! ........
		   .... MNNNNNO! ...OOOOOOOOOOO .  MMNNON!........
		   ...... MNNNNO! .. PPPPPPPPP .. MMNON!........
			  ...... OO! ................. ON! .......
				 ................................

   Be very careful when modifying this code, the data manipulation that it
   performs is somewhat tricky */

#ifdef USE_ENVELOPES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Sanity-check the envelope state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheck( const ENVELOPE_INFO *envelopeInfoPtr )
	{
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Make sure that the buffer position is within bounds */
	if( envelopeInfoPtr->buffer == NULL || \
		envelopeInfoPtr->bufPos < 0 || \
		envelopeInfoPtr->bufPos > envelopeInfoPtr->bufSize || \
		envelopeInfoPtr->bufSize < MIN_BUFFER_SIZE || \
		envelopeInfoPtr->bufSize >= MAX_BUFFER_SIZE )
		return( FALSE );

	/* Make sure that the block buffer position is within bounds */
	if( envelopeInfoPtr->blockSize > 0 && \
		( envelopeInfoPtr->blockBufferPos < 0 || \
		  envelopeInfoPtr->blockBufferPos >= envelopeInfoPtr->blockSize || \
		  envelopeInfoPtr->blockSize > CRYPT_MAX_IVSIZE ) )
		return( FALSE );

	/* Make sure that the partial buffer position is within bounds */
	if( envelopeInfoPtr->partialBufPos < 0 || \
		envelopeInfoPtr->partialBufPos > PARTIAL_BUFFER_SIZE )
		return( FALSE );

	/* Make sure that the out-of-band data buffer is within bounds */
	if( envelopeInfoPtr->oobBufSize < 0 || \
		envelopeInfoPtr->oobBufSize > OOB_BUFFER_SIZE )
		return( FALSE );

	/* Make sure that the envelope internal bookeeping is OK */
	if( envelopeInfoPtr->segmentSize < 0 || \
		envelopeInfoPtr->segmentSize >= MAX_INTLENGTH || \
		envelopeInfoPtr->dataLeft < 0 || \
		envelopeInfoPtr->dataLeft >= MAX_INTLENGTH )
		return( FALSE );

	return( TRUE );
	}

/* Handle the end-of-data, with PKCS #5 block padding if necessary:

			   pad
	+-------+-------+-------+
	|		|		|		|
	+-------+-------+-------+
			^		^
			|		|
		 padPtr	  bPos 

   This function needs to return a different error status value if 
   authenticated encryption is being used, because corruption of the PKCS #5
   padding is probably due to message data corruption (there's admittedly 
   also the extremely unlikely possibility that it's due to buggy sending 
   software).  Since the padding check occurs before the final MAC check, 
   the ensuing CRYPT_ERROR_BADDATA would override the later 
   CRYPT_ERROR_SIGNATURE from the MAC check.  In order to deal with this we
   convert a CRYPT_ERROR_BADDATA to a CRYPT_ERROR_SIGNATURE if authenticated
   encryption is being used */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processDataEnd( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	const int errorStatus = \
			( envelopeInfoPtr->dataFlags & ENVDATA_AUTHENCACTIONSACTIVE ) ? \
			CRYPT_ERROR_SIGNATURE : CRYPT_ERROR_BADDATA;
	int value = 0;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* If we're using a block cipher, undo the PKCS #5 padding which is
	   present at the end of the block */
	if( envelopeInfoPtr->blockSize > 1 )
		{
		const BYTE *padPtr;
		int padSize, i;

		/* Make sure that the padding size is valid.  There's no easy way to 
		   perform these checks in a timing-independent manner because we're 
		   using them to reject completely malformed data (out-of-bounds 
		   array references), but hopefully the few cycles difference won't 
		   be measurable in the overall scheme of things */
		padSize = envelopeInfoPtr->buffer[ envelopeInfoPtr->bufPos - 1 ];
		if( padSize < 1 || padSize > envelopeInfoPtr->blockSize || \
			padSize > envelopeInfoPtr->bufPos )
			return( errorStatus );

		/* Adjust the buffer for the padding */
		envelopeInfoPtr->bufPos -= padSize;
		ENSURES( envelopeInfoPtr->bufPos >= 0 && \
				 envelopeInfoPtr->bufPos < envelopeInfoPtr->bufSize );
		padPtr = envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos;

		/* Check the padding data in a timing-independent manner */
		for( i = 0; i < padSize - 1; i++ )
			value |= padPtr[ i ] ^ padSize;
		if( value != 0 )
			return( errorStatus );
		}

	/* Remember that we've reached the end of the payload and where the
	   payload ends ("This was the end of the river all right") */
	envelopeInfoPtr->dataFlags |= ENVDATA_ENDOFCONTENTS;
	envelopeInfoPtr->dataLeft = envelopeInfoPtr->bufPos;

	/* If this is PGP data and there's an MDC packet tacked onto the end of 
	   the payload, record the fact that it's non-payload data 
	   (processPgpSegment() has ensured that there's enough data present to 
	   contain a full MDC packet) */
#ifdef USE_PGP
	if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP && \
		( envelopeInfoPtr->dataFlags & ENVDATA_HASATTACHEDOOB ) )
		{
		envelopeInfoPtr->dataLeft -= PGP_MDC_PACKET_SIZE;
		ENSURES( envelopeInfoPtr->dataLeft > 0 && \
				 envelopeInfoPtr->dataLeft < MAX_INTLENGTH );
		}
#endif /* USE_PGP */

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Payload Segment Processing Routines					*
*																			*
****************************************************************************/

/* The minimum number of bytes of data that we need in order to try and 
   process a segment header.  For a PGP envelope a partial header is a 
   single byte, for a PKCS #7/CMS envelope it's two bytes (tag + length). 
   The setting can't be set too high because anything below the limit is 
   absorbed into the temporary header buffer, if the final header size is 
   less than what's absorbed by the buffer then the data will be lost 
   because data can't be pushed back out of the header buffer into the 
   main envelope buffer without messing up the buffer accounting due to data 
   (apparently) appearing out of nowhere */

#define MIN_HEADER_BYTES	2

/* Check for special-case segment conditions for which no further segment-
   processing action is necessary */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isEndOfSegment( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* If we've already processed the entire payload, don't do anything.
	   This can happen when we're using the definite encoding form and the 
	   EOC flag is set elsewhere as soon as the entire payload has been 
	   copied to the buffer */
	if( envelopeInfoPtr->dataFlags & ENVDATA_ENDOFCONTENTS )
		{
		REQUIRES( envelopeInfoPtr->segmentSize <= 0 );

		return( TRUE );
		}

	/* It's a standard segment */
	return( FALSE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isFixedLengthSegment( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* If the payload data is segmented but the first segment doesn't have 
	   an explicit length then the length of the first segment is defined 
	   as "whatever's left".  This is used to handle PGP's odd indefinite-
	   length encoding, for which the initial length has already been read 
	   when the packet header was read */
	if( envelopeInfoPtr->dataFlags & ENVDATA_NOFIRSTSEGMENT )
		{
		REQUIRES( envelopeInfoPtr->type == CRYPT_FORMAT_PGP );

		envelopeInfoPtr->dataFlags &= ~ENVDATA_NOFIRSTSEGMENT;
		envelopeInfoPtr->segmentSize = envelopeInfoPtr->payloadSize;
		envelopeInfoPtr->payloadSize = CRYPT_UNUSED;

		return( TRUE );
		}

	/* If we're using the definite encoding form there's a single segment 
	   equal in length to the entire payload */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED )
		{
		envelopeInfoPtr->segmentSize = envelopeInfoPtr->payloadSize;

		return( TRUE );
		}

	/* If we're using the indefinite form but it's an envelope type that
	   doesn't segment data then the length is implicitly defined as "until 
	   we run out of input".  This odd situation is encountered for PGP
	   envelopes when working with compressed data for which there's no 
	   length stored or when we're synchronising the envelope data prior to 
	   processing and there are abitrary further packets (typically PGP 
	   signature packets, where we want to process the packets in a 
	   connected series rather than stopping at the end of the first packet 
	   in the series) following the current one.  In both cases we don't 
	   know the overall length because we'd need to be able to look ahead an 
	   arbitrary distance in the stream to figure out where the compressed 
	   data or any further packets end */
	if( envelopeInfoPtr->dataFlags & ENVDATA_NOLENGTHINFO )
		{
		REQUIRES( envelopeInfoPtr->type == CRYPT_FORMAT_PGP );
		REQUIRES( envelopeInfoPtr->segmentSize <= 0 );

		return( TRUE );
		}

	/* It's a standard segment */
	return( FALSE );
	}

/* Process a CMS- or PGP-format sub-segment */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processCmsSegment( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
							  INOUT STREAM *stream, 
							  OUT_LENGTH_Z long *segmentLength )
	{
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( segmentLength, sizeof( long ) ) );

	/* Clear return value */
	*segmentLength = 0;

	/* Check for the EOCs that mark the end of the overall data */
	status = checkEOC( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( status == TRUE )
		{
		/* We've seen the EOC, wrap up the processing */
		return( processDataEnd( envelopeInfoPtr ) );
		}

	/* It's a new sub-segment, get its length */
	status = readLongGenericHole( stream, segmentLength, BER_OCTETSTRING );
	if( cryptStatusError( status ) )
		return( status );
	if( *segmentLength == CRYPT_UNUSED )
		{
		/* An indefinite-length encoding within a constructed data item 
		   isn't allowed */
		return( CRYPT_ERROR_BADDATA );
		}

	return( CRYPT_OK );
	}

#ifdef USE_PGP

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processPgpSegment( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
							  INOUT STREAM *stream, 
							  OUT_LENGTH_Z long *segmentLength )
	{
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( segmentLength, sizeof( long ) ) );

	/* Clear return value */
	*segmentLength = 0;

	/* Get the next sub-segment's length */
	status = pgpReadPartialLength( stream, segmentLength );
	if( cryptStatusError( status ) )
		{
		/* If we get an OK_SPECIAL returned then it's just an indication 
		   that we've got another partial length (with other segments to 
		   follow) and not an actual error */
		if( status == OK_SPECIAL )
			return( CRYPT_OK );

		/* Alongside normal errors this may also be an OK_SPECIAL to 
		   indicate that we got another partial length (with other segments 
		   to follow), which the caller will handle as a non-error */
		return( status );
		}

	/* If it's a terminating zero-length segment, wrap up the processing.
	   Unlike CMS, PGP can add other odds and ends at this point so we don't 
	   exit yet but fall through to the code that follows */
	if( *segmentLength <= 0 )
		{
		status = processDataEnd( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* We've now reached the last segment, if this is a packet with an MDC 
	   packet tacked on and the MDC data is larger than the length of the 
	   last segment, adjust its effective size to zero and pretend that it's 
	   not there since we can't process it and trying to do so would only 
	   give a false-positive error.
	   
	   This is rather problematic in that if the sender chooses to break the 
	   MDC packet across the partial-header boundary it'll include some of 
	   the MDC data with the payload, but there's no easy solution to this, 
	   the problem lies in the PGP spec for allowing a length encoding form 
	   that makes one-pass processing impossible.  Hopefully implementations 
	   will realise this and never break the MDC data over a partial-length 
	   header.
	   
	   What's worse though is that with this strategy an attacker can force 
	   us to skip processing the MDC by rewriting (with some effort because 
	   of PGP's weird power-of-two segment length constraints) the packet 
	   segments so that the MDC is always split across packets.  The "cure" 
	   to this is probably far worse than the disease since it would require 
	   buffering the last PGP_MDC_PACKET_SIZE - 1 bytes until we're sure 
	   that the next packet isn't an EOC so that we still need the previous 
	   lot of decrypted data.  This would lead to severe usability problems 
	   because the user can never be allowed to extract the last 
	   PGP_MDC_PACKET_SIZE - 1 bytes of data that they've pushed, all for a 
	   capability that they don't even know exists */
	if( ( envelopeInfoPtr->dataFlags & ENVDATA_HASATTACHEDOOB ) && \
		*segmentLength < PGP_MDC_PACKET_SIZE )
		{
		DEBUG_DIAG(( "MDC data was broken over a partial-length segment" ));
		assert( DEBUG_WARN );

		envelopeInfoPtr->dataFlags &= ~ENVDATA_HASATTACHEDOOB;
		*segmentLength = 0;
		}

	/* Convert the last segment into a definite-length segment.  When we 
	   return from this the calling code will immediately call 
	   getNextSegment() again since we've consumed some input, at which 
	   point the definite-length payload size will be set and the call will 
	   return with OK_SPECIAL to tell the caller that there's no more length 
	   information to fetch */
	envelopeInfoPtr->payloadSize = *segmentLength;
	*segmentLength = 0;

	return( CRYPT_OK );
	}
#endif /* USE_PGP */

/* Decode the header for the next segment in the buffer.  Returns the number
   of bytes consumed */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int getNextSegment( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
						   IN_BUFFER( length ) const BYTE *buffer, 
						   IN_LENGTH const int length, 
						   OUT_LENGTH_SHORT_Z int *bytesConsumed )
	{
	STREAM stream;
	long segmentLength;
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( buffer, length ) );
	assert( isWritePtr( bytesConsumed, sizeof( int ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE );

	/* Clear return values */
	*bytesConsumed = 0;

	/* If there's not enough data left to contain the header for a
	   reasonable-sized segment, tell the caller to try again with more data 
	   (the bytesConsumed value has already been set to zero earlier) */
	if( length < MIN_HEADER_BYTES )
		return( OK_SPECIAL );

	/* Get the sub-segment info */
	sMemConnect( &stream, buffer, length );
#ifdef USE_PGP
	if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP )
		{
		status = processPgpSegment( envelopeInfoPtr, &stream, 
									&segmentLength );
		}
	else
#endif /* USE_PGP */
		{
		status = processCmsSegment( envelopeInfoPtr, &stream, 
								    &segmentLength );
		}
	if( cryptStatusOK( status ) )
		*bytesConsumed = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		/* If we got an underflow error this isn't fatal since we can 
		   continue when the user pushes more data, so we return normally
		   with bytesConsumed set to zero */
		if( status == CRYPT_ERROR_UNDERFLOW )
			return( OK_SPECIAL );

		return( status );
		}
	ENSURES( *bytesConsumed > 0 && *bytesConsumed <= length );

	/* We got the length, return the information to the caller */
	envelopeInfoPtr->segmentSize = segmentLength;

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/* Process the header for a new payload data segment. There's one specific 
   situation where we can run into problems and that's where we're pushing 
   indefinite-length data and run out of data halfway through a tag, either 
   an EOC or an OCTET STRING segment (or its PGP equivalent):

	+-------+-----------+-----+---------+
	| Header|	Body	|00 00| Trailer |
	+-------+-----------+-----+---------+
						   ^	
						   |
						length
						   +-------+
								   v
	+-------+--------+----------+--------+----------+-----+---------+
	| Header|04 xx xx|	Body	|04 xx xx|	Body	|00 00|	Trailer |
	+-------+--------+----------+--------+----------+-----+---------+

   In this case getNextSegment() will return OK_SPECIAL and we have to 
   buffer the data somewhere until the next push.  We can't report the 
   remainder to the caller as un-consumed data because this may be an 
   implicit push, for example when we add a keying resource to an encrypted 
   envelope, which continues processing with previously-pushed data when it 
   initialises the cryptovariables from the data.  In this case since no 
   data is being pushed there's no way to report that some of the data was 
   unconsumed, so we have to store it in the partial-header buffer until the 
   next push.
   
   This function returns additional operation-control information in the 
   form of a SEG_ACTION_TYPE, which can be one of the following:

	SEG_ACTION_NONE: Segment information for the next segment has been 
					 obtained, processing of data can continue.

	SEG_ACTION_BREAK: Data consists of a single segment, caller should break 
					  from segment-handling loop.

	SEG_ACTION_CALLEREXIT: Caller should exit since no further action is 
						   possible.

	SEG_ACTION_CONTINUE: No-op segment (e.g. a zero-size segment), caller 
						 should try again */

typedef enum {
	SEG_ACTION_NONE,		/* No special segment action */
	SEG_ACTION_BREAK,		/* Caller should break from seg-handling loop */
	SEG_ACTION_CALLEREXIT,	/* Caller should exit */
	SEG_ACTION_CONTINUE,	/* No-op segment, caller should try again */
	SEG_ACTION_LAST			/* Last possible segment action */
	} SEG_ACTION_TYPE;

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
static int processSegment( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						   IN_BUFFER( length ) const BYTE *buffer, 
						   IN_LENGTH const int length,
						   OUT_LENGTH_SHORT_Z int *bytesConsumed,
						   OUT_ENUM_OPT( SEG_ACTION ) \
								SEG_ACTION_TYPE *segAction )
	{
	BYTE *bufPtr = ( BYTE * ) buffer;
	const BYTE *headerPtr = bufPtr;
	int headerLength = length, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( buffer, length ) );
	assert( isWritePtr( bytesConsumed, sizeof( int ) ) );
	assert( isWritePtr( segAction, sizeof( SEG_ACTION_TYPE ) ) );

	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE );
	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* Clear return values */
	*bytesConsumed = 0;
	*segAction = SEG_ACTION_NONE;

	/* If we've already processed the entire payload, don't do anything */
	if( isEndOfSegment( envelopeInfoPtr ) )
		{
		REQUIRES( envelopeInfoPtr->partialBufPos == 0 );

		/* We're done, tell the caller to exit */
		*segAction = SEG_ACTION_CALLEREXIT;
		return( CRYPT_OK );
		}

	/* If it's a fixed-length segment, no further action is necessary */
	if( isFixedLengthSegment( envelopeInfoPtr ) )
		{
		REQUIRES( envelopeInfoPtr->partialBufPos == 0 );

		*segAction = SEG_ACTION_BREAK;
		return( CRYPT_OK );
		}

	/* At this point we're after new segment information */
	REQUIRES( envelopeInfoPtr->segmentSize == 0 );

	/* If there's buffered partial header data present from a previous 
	   operation, use that along with any new data to try and construct a 
	   complete header */
	if( envelopeInfoPtr->partialBufPos > 0 )
		{
		const int remainder = min( PARTIAL_BUFFER_SIZE - \
											envelopeInfoPtr->partialBufPos,
								   length );
		if( remainder > 0 )
			{
			memcpy( envelopeInfoPtr->partialBuffer + \
						envelopeInfoPtr->partialBufPos, bufPtr, remainder );
			}
		headerPtr = envelopeInfoPtr->partialBuffer;
		headerLength = envelopeInfoPtr->partialBufPos + remainder;
		}

	/* Try and get the next segment's information from the header data */
	status = getNextSegment( envelopeInfoPtr, headerPtr, headerLength, 
							 bytesConsumed );
	if( cryptStatusError( status ) )
		{
		/* If we don't have enough input data left to read the information 
		   for the next segment, buffer what we've got so far and tell the 
		   caller that we've consumed all of our input */
		if( status == OK_SPECIAL )
			{
			ENSURES( *bytesConsumed <= 0 );

			/* Save the partial header information for next time */
			REQUIRES( rangeCheckZ( envelopeInfoPtr->partialBufPos,
								   length, PARTIAL_BUFFER_SIZE ) );
			memcpy( envelopeInfoPtr->partialBuffer + \
						envelopeInfoPtr->partialBufPos, bufPtr, length );
			envelopeInfoPtr->partialBufPos += length;

			/* We've absorbed any remaining data into the partial-header 
			   buffer, tell the caller to exit */
			*bytesConsumed = length;
			*segAction = SEG_ACTION_CALLEREXIT;
			return( CRYPT_OK );
			}

		return( status );
		}

	/* We've got information on a new segment, clear the buffered header 
	   data if necessary and adjust for how much data we've consumed */
	if( envelopeInfoPtr->partialBufPos > 0 )
		{
		ENSURES( envelopeInfoPtr->partialBufPos <= *bytesConsumed );

		*bytesConsumed -= envelopeInfoPtr->partialBufPos;
		envelopeInfoPtr->partialBufPos = 0;
		}

	/* If we've reached the EOC or consumed all of the input data, exit */
	if( ( envelopeInfoPtr->dataFlags & ENVDATA_ENDOFCONTENTS ) || \
		( length - *bytesConsumed ) <= 0 )
		{
		*segAction = SEG_ACTION_CALLEREXIT;
		return( CRYPT_OK );
		}

	/* We've got a new data segment, if it's of nonzero size we're done, 
	   otherwise the caller has to try again */
	*segAction = ( envelopeInfoPtr->segmentSize > 0 ) ? \
				 SEG_ACTION_NONE : SEG_ACTION_CONTINUE;								
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Copy to Envelope							*
*																			*
****************************************************************************/

/* Copy encrypted data blocks into the envelope buffer with any overflow 
   held in the block buffer.  Only complete blocks are copied into the main
   envelope buffer, if there's not enough data present for a complete block
   it's temporarily held in the block buffer:

			 bytesFromBB			  bytesToBB
		bufPos--+ |						  |
				v<+>|<-- qBytesToCopy ->|<+>|
	+-----------+-----------------------+	|
	|			|///|	|		|		|	|		Main buffer
	+-----------+-----------------------+-------+
				  ^			 ^			|///|	|	Overflow block buffer
				  |			 |			+-------+
			 Prev.bBuf	  New data		  ^	
			 contents					  |
									New data remaining */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int copyEncryptedDataBlocks( INOUT ENVELOPE_INFO *envelopeInfoPtr,
									IN_BUFFER( length ) const BYTE *buffer, 
									IN_LENGTH const int length,
									OUT_DATALENGTH_Z int *bytesCopied )
	{
	BYTE *bufPtr = envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos;
	int bytesFromBB = 0, quantizedBytesToCopy, bytesToBB, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( buffer, length ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE && \
			  envelopeInfoPtr->bufPos + \
				envelopeInfoPtr->blockBufferPos + \
				length <= envelopeInfoPtr->bufSize + \
						  envelopeInfoPtr->blockSize );
	REQUIRES( !( envelopeInfoPtr->dataFlags & ENVDATA_NOLENGTHINFO ) );

	/* Clear return value */
	*bytesCopied = 0;

	/* If the new data will fit entirely into the block buffer, copy it in
	   now and return */
	if( envelopeInfoPtr->blockBufferPos + length < envelopeInfoPtr->blockSize )
		{
		memcpy( envelopeInfoPtr->blockBuffer + envelopeInfoPtr->blockBufferPos, 
				buffer, length );
		envelopeInfoPtr->blockBufferPos += length;

		/* Adjust the segment size based on what we've consumed */
		envelopeInfoPtr->segmentSize -= length;
		*bytesCopied = length;

		ENSURES( sanityCheck( envelopeInfoPtr ) );

		return( CRYPT_OK );
		}

	/* If there isn't room in the main buffer for even one more block, exit
	   without doing anything (bytesCopied is still set to zero from the 
	   earlier code).  This leads to slightly anomalous behaviour where, 
	   with no room for a complete block in the main buffer, copying in a 
	   data length smaller than the block buffer will lead to the data being 
	   absorbed by the block buffer due to the previous section of code, but 
	   copying in a length larger than the block buffer will result in no 
	   data at all being absorbed even if there's still room in the block 
	   buffer, see the long comment in copyData() for a full discussion of 
	   this process */
	if( envelopeInfoPtr->bufPos + \
			envelopeInfoPtr->blockSize > envelopeInfoPtr->bufSize )
		{
		/* There's no room for even one more block */
		return( CRYPT_OK );	
		}

	/* There's room for at least one more block in the buffer.  First, if
	   there are leftover bytes in the block buffer, move them into the main
	   buffer */
	if( envelopeInfoPtr->blockBufferPos > 0 )
		{
		REQUIRES( bytesFromBB >= 0 && \
				  bytesFromBB <= envelopeInfoPtr->blockSize );

		bytesFromBB = envelopeInfoPtr->blockBufferPos;
		memcpy( bufPtr, envelopeInfoPtr->blockBuffer, bytesFromBB );
		}
	envelopeInfoPtr->blockBufferPos = 0;

	/* Determine how many bytes we can copy into the buffer to fill it to
	   the nearest available block size */
	quantizedBytesToCopy = ( length + bytesFromBB ) & \
						   envelopeInfoPtr->blockSizeMask;
	quantizedBytesToCopy -= bytesFromBB;
	ENSURES( quantizedBytesToCopy > 0 && quantizedBytesToCopy <= length && \
			 envelopeInfoPtr->bufPos + bytesFromBB + \
					quantizedBytesToCopy <= envelopeInfoPtr->bufSize );
	ENSURES( ( ( bytesFromBB + quantizedBytesToCopy ) & \
			   ( envelopeInfoPtr->blockSize - 1 ) ) == 0 );

	/* Now copy across a number of bytes which is a multiple of the block
	   size and decrypt them.  Note that we have to use memmove() rather
	   than memcpy() because if we're sync'ing data in the buffer we're
	   doing a copy within the buffer rather than copying in data from
	   an external source */
	memmove( bufPtr + bytesFromBB, buffer, quantizedBytesToCopy );
	if( envelopeInfoPtr->dataFlags & ENVDATA_AUTHENCACTIONSACTIVE )
		{
		/* We're performing authenticated encryotion, hash the ciphertext
		   before decrypting it */
		status = hashEnvelopeData( envelopeInfoPtr->actionList, bufPtr,
								   bytesFromBB + quantizedBytesToCopy );
		if( cryptStatusError( status ) )
			return( status );
		}
	status = krnlSendMessage( envelopeInfoPtr->iCryptContext,
							  IMESSAGE_CTX_DECRYPT, bufPtr,
							  bytesFromBB + quantizedBytesToCopy );
	if( cryptStatusError( status ) )
		return( status );
	envelopeInfoPtr->bufPos += bytesFromBB + quantizedBytesToCopy;
	envelopeInfoPtr->segmentSize -= length;
	ENSURES( envelopeInfoPtr->bufPos >= 0 && \
			envelopeInfoPtr->bufPos <= envelopeInfoPtr->bufSize );
	ENSURES( envelopeInfoPtr->segmentSize >= 0 && \
			 envelopeInfoPtr->segmentSize < MAX_INTLENGTH );

	/* If the payload has a definite length and we've reached its end, set
	   the EOC flag to make sure that we don't go any further */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED && \
		envelopeInfoPtr->segmentSize <= 0 )
		{
		status = processDataEnd( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );

		ENSURES( sanityCheck( envelopeInfoPtr ) );

		*bytesCopied = length;
		return( CRYPT_OK );
		}

	/* Copy any remainder (the difference between the amount to copy and the
	   blocksize-quantized amount) into the block buffer */
	bytesToBB = length - quantizedBytesToCopy;
	REQUIRES( bytesToBB >= 0 && bytesToBB <= envelopeInfoPtr->blockSize );
	if( bytesToBB > 0 )
		{
		memcpy( envelopeInfoPtr->blockBuffer, buffer + quantizedBytesToCopy,
				bytesToBB );
		}
	envelopeInfoPtr->blockBufferPos = bytesToBB;

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	*bytesCopied = length;
	return( CRYPT_OK );
	}

/* Copy possibly encrypted data into the envelope with special handling for
   block encryption modes.  Returns the number of bytes copied:

						  bPos			  bSize
							|				|
							v				v
	+-----------------------+---------------+
	|		|		|		|		|		|	Main buffer
	+-----------------------+---------------+

							+-------+
							|///|	|			Overflow block buffer
							+-------+
								^	^
								|blBufSize
							 blBufPos

    The main buffer only contains data amounts quantised to the encryption
	block size.  Any additional data is copied into the block buffer, a
	staging buffer used to accumulate data until it can be transferred to
	the main buffer for decryption */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int copyData( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
					 IN_BUFFER( length ) const BYTE *buffer, 
					 IN_DATALENGTH const int length,
					 OUT_DATALENGTH_Z int *bytesCopied )
	{
	BYTE *bufPtr = envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos;
	int bytesToCopy = length, bytesLeft, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( buffer, length ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE );

	/* Clear return value */
	*bytesCopied = 0;

	/* Figure out how much we can copy across.  First we calculate the
	   minimum of the amount of data passed in and the amount remaining in
	   the current segment */
	if( !( envelopeInfoPtr->dataFlags & ENVDATA_NOLENGTHINFO ) && \
		bytesToCopy > envelopeInfoPtr->segmentSize )
		bytesToCopy = envelopeInfoPtr->segmentSize;

	/* Now we check to see if this is affected by the total free space
	   remaining in the buffer.  If we're processing data blocks we can have
	   two cases, one in which the limit is the amount of buffer space
	   available and the other in which the limit is the amount of data
	   available.  If the limit is set by the available data then we don't 
	   have to worry about flushing extra data out of the block buffer into 
	   the main buffer but if the limit is set by the available buffer space 
	   we have to reduce the amount that we can copy in based on any extra 
	   data that will be flushed out of the block buffer.

	   There are two possible approaches that can be used when the block
	   buffer is involved.  The first one copies as much as we can into the
	   buffer and, if that isn't enough, maxes out the block buffer with as
	   much remaining data as possible.  The second only copies in as much as
	   can fit into the buffer, even if there's room in the block buffer for
	   a few more bytes.  The second approach is preferable because although
	   either will give the impression of a not-quite-full buffer into which
	   no more data can be copied, the second minimizes the amount of data
	   which is moved into and out of the block buffer.

	   The first approach may seem slightly more logical, but will only
	   cause confusion in the long run.  Consider copying (say) 43 bytes to
	   a 43-byte buffer.  The first time this will succeed, after which there
	   will be 40 bytes in the buffer (reported to the caller) and 3 in the
	   block buffer.  If the caller tries to copy in 3 more bytes to "fill"
	   the main buffer, they'll again vanish into the block buffer.  A second
	   call with three more bytes will copy 2 bytes and return with 1 byte
	   uncopied.  In effect this method of using the block buffer extends the
	   blocksize-quantized main buffer by the size of the block buffer, which
	   will only cause confusion when data appears to vanish when copied into
	   it.

	   In the following length calculation the block buffer content is 
	   counted as part of the total content in order to implement the second
	   buffer-filling strategy */
	bytesLeft = envelopeInfoPtr->bufSize - \
				( envelopeInfoPtr->bufPos + envelopeInfoPtr->blockBufferPos );
	if( bytesLeft <= 0 )
		{
		/* There's no room left to copy anything in, return now (bytesCopied 
		   is still set to zero from the earlier code).  We can't check this 
		   in the calling code because it doesn't know about the internal 
		   buffer-handling strategy that we use so we perform an explicit 
		   check here */
		return( CRYPT_OK );
		}
	if( bytesLeft < bytesToCopy )
		bytesToCopy = bytesLeft;
	ENSURES( bytesToCopy > 0 && bytesToCopy <= length );

	/* If its a block encryption mode then we need to provide special 
	   handling for odd data lengths that don't match the block size */
	if( envelopeInfoPtr->blockSize > 1 )
		{
		return( copyEncryptedDataBlocks( envelopeInfoPtr, buffer,
										 bytesToCopy, bytesCopied ) );
		}

	/* It's unencrypted data or data that's encrypted with a stream cipher, 
	   just copy over as much of the segment as we can and decrypt it if 
	   necessary.  We use memmove() for the same reason as given above */
	memmove( bufPtr, buffer, bytesToCopy );
	if( envelopeInfoPtr->iCryptContext != CRYPT_ERROR )
		{
		if( envelopeInfoPtr->dataFlags & ENVDATA_AUTHENCACTIONSACTIVE )
			{
			/* We're performing authenticated encryption, MAC the 
			   ciphertext before decrypting it (this is the technique that's 
			   used for CMS data) */
			status = hashEnvelopeData( envelopeInfoPtr->actionList, bufPtr,
									   bytesToCopy );
			if( cryptStatusError( status ) )
				return( status );
			}
		status = krnlSendMessage( envelopeInfoPtr->iCryptContext,
								  IMESSAGE_CTX_DECRYPT, bufPtr,
								  bytesToCopy );
		if( cryptStatusError( status ) )
			return( status );
		if( envelopeInfoPtr->dataFlags & ENVDATA_HASHACTIONSACTIVE )
			{
			int bytesToHash = bytesToCopy;

			/* We're performing integrity-protected encryption, hash the 
			   plaintext after decrypting it (this is the technique that's
			   used for PGP data).

			   PGP complicates things further by optionally tacking an
			   MDC packet onto the end of the data payload, which is 
			   encrypted but not hashed.  To handle this, if there's an
			   MDC packet present and we've reached the end of the payload 
			   data then we have to adjust the amount of data that gets 
			   hashed.  Using the length check to detect EOF is safe even
			   in the presence of indefinite-length data because 
			   processPgpSegment() converts the last segment of indefinite-
			   length data into a definite-length one (so payloadSize != 
			   CRYPT_UNUSED), and also ensures that there's enough data 
			   present to contain a full MDC packet */
			if( ( envelopeInfoPtr->dataFlags & ENVDATA_HASATTACHEDOOB ) && \
				envelopeInfoPtr->payloadSize != CRYPT_UNUSED && \
				bytesToCopy >= envelopeInfoPtr->segmentSize )
				{
				bytesToHash -= PGP_MDC_PACKET_SIZE;
				ENSURES( bytesToHash > 0 && bytesToHash < MAX_BUFFER_SIZE );
				}
			status = hashEnvelopeData( envelopeInfoPtr->actionList, bufPtr,
									   bytesToHash );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	envelopeInfoPtr->bufPos += bytesToCopy;
	if( !( envelopeInfoPtr->dataFlags & ENVDATA_NOLENGTHINFO ) )
		envelopeInfoPtr->segmentSize -= bytesToCopy;

	/* If the payload has a definite length and we've reached its end, set
	   the EOC flag to make sure that we don't go any further (this also
	   handles PGP's form of indefinite-length encoding in which each 
	   segment except the last is indefinite-length and the last one is a
	   standard definite-length segment) */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED && \
		envelopeInfoPtr->segmentSize <= 0 )
		{
		status = processDataEnd( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	ENSURES( bytesToCopy > 0 && bytesToCopy <= length );
	ENSURES( sanityCheck( envelopeInfoPtr ) );

	*bytesCopied = bytesToCopy;
	return( CRYPT_OK );
	}

/* Copy data into the de-enveloping envelope.  Returns the number of bytes
   copied */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyToDeenvelope( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							 IN_BUFFER( length ) const BYTE *buffer, 
							 IN_LENGTH const int length )
	{
	BYTE *bufPtr = ( BYTE * ) buffer;
	const int maxIterations = ( length <= FAILSAFE_ITERATIONS_LARGE * 1024 ) ? \
								FAILSAFE_ITERATIONS_LARGE : \
								length / FAILSAFE_ITERATIONS_LARGE;
	int currentLength = length, bytesCopied, iterationCount;
		/* The calculation for maxIterations is necessary in order to deal 
		   with the use of very large data quantities and buffers, if the
		   input data contains (say) 1K segments and 10MB of data then we
		   can exceed the fixed FAILSAFE_ITERATIONS_xxx value so we have to
		   adjust it dynamically based on the data size */

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( buffer, length ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE );
	REQUIRES( maxIterations > 0 && maxIterations < MAX_INTLENGTH );

	/* If we're trying to copy data into a full buffer, return a count of 0
	   bytes (the caller may convert this to an overflow error if 
	   necessary) */
	if( envelopeInfoPtr->bufPos >= envelopeInfoPtr->bufSize )
		return( 0 );

	/* If we're verifying a detached signature, just hash the data and exit.
	   We don't have to check whether hashing is active or not because it'll
	   always be active for detached data since this is the only thing 
	   that's done with it */
	if( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG )
		{
		int status;

		REQUIRES( envelopeInfoPtr->dataFlags & ENVDATA_HASHACTIONSACTIVE );

		status = hashEnvelopeData( envelopeInfoPtr->actionList,
								   buffer, currentLength );
		return( cryptStatusError( status ) ? status : currentLength );
		}

	/* Keep processing data until we either run out of input or we can't copy
	   in any more data.  The code sequence within this loop acts as a simple
	   FSM so that if we exit at any point then the next call to this
	   function will resume where we left off */
	for( iterationCount = 0; \
		 iterationCount < maxIterations && currentLength > 0; \
		 iterationCount++ )
		{
		SEG_ACTION_TYPE segAction = ( envelopeInfoPtr->segmentSize <= 0 ) ? \
									SEG_ACTION_CONTINUE : SEG_ACTION_NONE;
		int segmentCount, status;

		/* If there's no segment information currently available then we 
		   need to process a segment header before we can handle any data.  
		   The use of a loop is necessary to handle some broken 
		   implementations that emit zero-length sub-segments, and as a 
		   corollary it also helps avoid a pile of special-case code to 
		   manage PGP's strange way of handling the last segment in 
		   indefinite-length encodings.  We limit the segment count to 
		   FAILSAFE_ITERATIONS_SMALL sub-segments to make sure that we don't 
		   spend forever trying to process extremely broken data */
		for( segmentCount = 0; \
			 segAction == SEG_ACTION_CONTINUE && \
				segmentCount < FAILSAFE_ITERATIONS_SMALL; \
			 segmentCount++ )
			{
			int bytesConsumed;

			status = processSegment( envelopeInfoPtr, bufPtr, currentLength, 
									 &bytesConsumed, &segAction );
			if( cryptStatusError( status ) )
				return( status );
			switch( segAction )
				{
				case SEG_ACTION_CALLEREXIT:
					{
					const int bytesLeft = currentLength - bytesConsumed;

					/* We've completed processing the payload data, exit */
					ENSURES( bytesLeft >= 0 && bytesLeft < length );
					ENSURES( sanityCheck( envelopeInfoPtr ) );
					
					return( length - bytesLeft );
					}

				case SEG_ACTION_BREAK:
					/* The data consists of a single segment, there's 
					   nothing further to do.  Since segAction isn't set to 
					   SEG_ACTION_CONTINUE, we'll exit the segment loop at 
					   the end of this iteration */
					ENSURES( bytesConsumed == 0 );
					break;

				case SEG_ACTION_CONTINUE:
				case SEG_ACTION_NONE:
					/* We either need to process another segment in order to 
					   continue or we've got segment information and can 
					   exit the loop.
					   
					   We can get bytesConsumed == 0 if all input was taken
					   from the header buffer and the header itself was less
					   than MIN_HEADER_BYTES in size.  This occurs because
					   we don't try and process data quantities less than 
					   MIN_HEADER_BYTES, if the header eventually fits inside
					   MIN_HEADER_BYTES then no data is consumed while 
					   processing it */
					ENSURES( bytesConsumed >= 0 );
					break;

				default:
					retIntError();
				}

			/* Adjust the payload information by the amount of data that was
			   consumed and continue */
			bufPtr += bytesConsumed;
			currentLength -= bytesConsumed;
			}
		if( segmentCount >= FAILSAFE_ITERATIONS_SMALL )
			{
			/* We've processed FAILSAFE_ITERATIONS_SMALL consecutive sub-
			   segments in a row, there's something wrong with the input 
			   data */
			return( CRYPT_ERROR_BADDATA );
			}
		ENSURES( currentLength > 0 && currentLength <= length && \
				 currentLength < MAX_BUFFER_SIZE );

		/* Copy the data into the envelope, decrypting it as we go if
		   necessary.  In theory we could also check to see whether any
		   more data can fit into the buffer before trying to copy it in
		   but since this would require knowledge of the internal buffer-
		   handling strategy used by copyData() we always call it and rely
		   on a bytesCopied value of zero to indicate that the tank is
		   full */
		status = copyData( envelopeInfoPtr, bufPtr, currentLength, 
						   &bytesCopied );
		if( cryptStatusError( status ) )
			return( status );
		bufPtr += bytesCopied;
		currentLength -= bytesCopied;

		ENSURES( sanityCheck( envelopeInfoPtr ) );
		ENSURES( currentLength >= 0 && currentLength <= length && \
				 currentLength < MAX_BUFFER_SIZE );

		/* If we couldn't copy any more data then we're done */
		if( bytesCopied <= 0 )
			break;

		assert( ( envelopeInfoPtr->segmentSize >= 0 ) || \
				( ( envelopeInfoPtr->dataFlags & ENVDATA_NOSEGMENT ) && \
				  ( envelopeInfoPtr->payloadSize == CRYPT_UNUSED ) && \
				  ( envelopeInfoPtr->segmentSize == CRYPT_UNUSED ) ) );
		}
	ENSURES( iterationCount < maxIterations );

	ENSURES( sanityCheck( envelopeInfoPtr ) );
	return( length - currentLength );
	}

/****************************************************************************
*																			*
*								Copy from Envelope							*
*																			*
****************************************************************************/

/* Copy buffered out-of-band data from earlier processing to the output.  
   This is only required for PGP envelopes where we have to burrow down into
   nested content in order to figure out what to do with it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int copyOobData( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
						OUT_BUFFER( maxLength, *length ) BYTE *buffer, 
						IN_DATALENGTH const int maxLength, 
						OUT_DATALENGTH_Z int *length,
						const BOOLEAN retainInBuffer )
	{
	const int oobBytesToCopy = min( maxLength, envelopeInfoPtr->oobBufSize );
	const int oobRemainder = envelopeInfoPtr->oobBufSize - oobBytesToCopy;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES( oobBytesToCopy > 0 && \
			  oobBytesToCopy <= envelopeInfoPtr->oobBufSize && \
			  oobBytesToCopy <= OOB_BUFFER_SIZE );

	memcpy( buffer, envelopeInfoPtr->oobBuffer, oobBytesToCopy );
	*length = oobBytesToCopy;

	/* If we're retaining the data in the OOB, we're done */
	if( retainInBuffer )
		{
		ENSURES( sanityCheck( envelopeInfoPtr ) );

		return( CRYPT_OK );
		}

	/* We're moving data out of the OOB buffer, adjust the OOB buffer 
	   contents */
	if( oobRemainder > 0 )
		{
		REQUIRES( rangeCheck( oobBytesToCopy, oobRemainder,
							  OOB_BUFFER_SIZE ) );
		memmove( envelopeInfoPtr->oobBuffer,
				 envelopeInfoPtr->oobBuffer + oobBytesToCopy, oobRemainder );
		}
	envelopeInfoPtr->oobBufSize = oobRemainder;

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/* Copy data from the envelope.  Returns the number of bytes copied */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int copyFromDeenvelope( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
							   OUT_BUFFER( maxLength, *length ) BYTE *buffer, 
							   IN_DATALENGTH const int maxLength, 
							   OUT_DATALENGTH_Z int *length, 
							   IN_FLAGS_Z( ENVCOPY ) const int flags )
	{
	const BOOLEAN isLookaheadRead = ( flags & ENVCOPY_FLAG_OOBDATA ) ? \
									TRUE : FALSE;
	int bytesToCopy = maxLength, bytesCopied, oobBytesCopied = 0;
	int remainder, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES( !isLookaheadRead || \
			  ( isLookaheadRead && \
			    ( bytesToCopy > 0 && bytesToCopy <= OOB_BUFFER_SIZE ) ) );
	REQUIRES( flags == ENVCOPY_FLAG_NONE || flags == ENVCOPY_FLAG_OOBDATA );

	/* Clear return values */
	memset( buffer, 0, min( 16, bytesToCopy ) );
	*length = 0;

	/* If we're verifying a detached signature the data is communicated 
	   out-of-band so there's nothing to copy out (the length is still set
	   to zero from the earlier code */
	if( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG )
		return( CRYPT_OK );

	/* If there's buffered out-of-band data from an earlier lookahead read 
	   present, insert it into the output stream */
	if( envelopeInfoPtr->oobBufSize > 0 )
		{
		status = copyOobData( envelopeInfoPtr, buffer, bytesToCopy, 
							  &oobBytesCopied, isLookaheadRead );
		if( cryptStatusError( status ) )
			return( status );
		bytesToCopy -= oobBytesCopied;
		buffer += oobBytesCopied;
		if( bytesToCopy <= 0 )
			{
			*length = oobBytesCopied;

			ENSURES( sanityCheck( envelopeInfoPtr ) );
			return( CRYPT_OK );
			}
		}
	ENSURES( bytesToCopy > 0 && bytesToCopy <= maxLength && \
			 bytesToCopy < MAX_BUFFER_SIZE );

	/* If we're using compression, expand the data from the buffer to the
	   output via the zStream */
#ifdef USE_COMPRESSION
	if( envelopeInfoPtr->flags & ENVELOPE_ZSTREAMINITED )
		{
		const int originalInLength = bytesToCopy;
		const int bytesIn = \
			( envelopeInfoPtr->dataLeft > 0 && \
			  envelopeInfoPtr->dataLeft < envelopeInfoPtr->bufPos ) ? \
			envelopeInfoPtr->dataLeft : envelopeInfoPtr->bufPos;

		/* Decompress the data into the output buffer.  Note that we use the
		   length value to determine the length of the output rather than
		   bytesToCopy since the ratio of bytes in the buffer to bytes of
		   output isn't 1:1 as it is for other content types.

		   When using PGP 2.x-compatible decompression we have to allow a
		   return status of Z_BUF_ERROR because it uses a compression format
		   from a pre-release version of InfoZip that doesn't include
		   header or trailer information so the decompression code can't
		   definitely tell that it's reached the end of its input data but
		   can only report that it can't go any further.

		   When we're trying to suck all remaining compressed data from the 
		   zstream we may get a (rather misleadingly named) Z_BUF_ERROR if 
		   there's both no internally buffered data left and no new input 
		   available to produce any output.  If this is the case we simply
		   treat it as a nothing-copied condition, since decompression will
		   continue when the user provides more input.

		   We can also get a Z_BUF_ERROR for some types of (non-fatal) error
		   situations, for example if we're flushing out data still present
		   in the zstream (avail_in == 0) and there's a problem such as the
		   compressor needing more data but there's none available, the zlib
		   code will report it as a Z_BUF_ERROR.  In this case we convert it
		   into a (recoverable) underflow error, which isn't always accurate
		   but is more useful than the generic CRYPT_ERROR_FAILED */
		envelopeInfoPtr->zStream.next_in = envelopeInfoPtr->buffer;
		envelopeInfoPtr->zStream.avail_in = bytesIn;
		envelopeInfoPtr->zStream.next_out = buffer;
		envelopeInfoPtr->zStream.avail_out = bytesToCopy;
		status = inflate( &envelopeInfoPtr->zStream, Z_SYNC_FLUSH );
		if( status != Z_OK && status != Z_STREAM_END && \
			!( status == Z_BUF_ERROR && \
			   envelopeInfoPtr->type == CRYPT_FORMAT_PGP ) && \
			!( status == Z_BUF_ERROR && bytesIn == 0 ) )
			{
			ENSURES( status != Z_STREAM_ERROR );	/* Parameter error */
			return( ( status == Z_DATA_ERROR ) ? CRYPT_ERROR_BADDATA : \
					( status == Z_MEM_ERROR ) ? CRYPT_ERROR_MEMORY : \
					( status == Z_BUF_ERROR ) ? CRYPT_ERROR_UNDERFLOW : \
					CRYPT_ERROR_FAILED );
			}

		/* Adjust the status information based on the data copied from the
		   buffer into the zStream (bytesCopied) and the data flushed from
		   the zStream to the output (bytesToCopy) */
		bytesCopied = bytesIn - envelopeInfoPtr->zStream.avail_in;
		bytesToCopy -= envelopeInfoPtr->zStream.avail_out;
		ENSURES( bytesCopied >= 0 && bytesCopied < MAX_BUFFER_SIZE && \
				 bytesToCopy >= 0 && bytesToCopy <= originalInLength && \
				 bytesToCopy < MAX_BUFFER_SIZE );

		/* If we're doing a lookahead read we can't just copy the data out 
		   of the envelope buffer as we would for any other content type 
		   because we can't undo the decompression step, so we copy the data 
		   that was decompressed into the output buffer to a local buffer 
		   and insert it into the output stream on the next non-lookahead 
		   read.  The size of the read from the OOB buffer has been limited
		   previously when existing data was detected in the OOB buffer so 
		   the total of the current OOB buffer contents and the amount to 
		   read can never exceed the OOB buffer size */
		if( isLookaheadRead )
			{
			REQUIRES( envelopeInfoPtr->oobBufSize + \
					  originalInLength <= OOB_BUFFER_SIZE );

			memcpy( envelopeInfoPtr->oobBuffer + envelopeInfoPtr->oobBufSize,
					buffer, originalInLength );
			envelopeInfoPtr->oobBufSize += originalInLength;
			}
		}
	else
#endif /* USE_COMPRESSION */
		{
		/* Copy out as much of the data as we can, making sure that we don't
		   overrun into any following data */
		if( bytesToCopy > envelopeInfoPtr->bufPos )
			bytesToCopy = envelopeInfoPtr->bufPos;
		if( envelopeInfoPtr->dataLeft > 0 && \
			bytesToCopy > envelopeInfoPtr->dataLeft )
			bytesToCopy = envelopeInfoPtr->dataLeft;
		ENSURES( bytesToCopy >= 0 && bytesToCopy <= maxLength && \
				 bytesToCopy < MAX_BUFFER_SIZE );

		/* We perform the postcondition check here because there are 
		   numerous exit points in the following code and this avoids having 
		   to check at every one */
		ENSURES( sanityCheck( envelopeInfoPtr ) );

		/* If we're using a block encryption mode and we haven't seen the
		   end-of-contents yet and there's no data waiting in the block
		   buffer (which would mean that there's more data to come), we
		   can't copy out the last block because it might contain padding,
		   so we decrease the effective data amount by one block's worth.

		   Note that this overestimates the amount to retain by up to 
		   blockSize bytes since we really need to count from the start 
		   rather than the end of the data.  That is, if the caller pushes 
		   in blockSize + 1 bytes then we could pop up to blockSize bytes 
		   since the caller has to push at least blockSize - 1 further bytes 
		   to complete the next block.  The current strategy of always 
		   reserving a full block only allows 1 byte to be popped. 

		   Keeping track of how many complete blocks have been processed 
		   just to handle this obscure corner case really isn't worth it, 
		   due to a coding bug this check was performed incorrectly and it 
		   took nine years before anyone triggered it so it's not worth 
		   adding additional handling for this situation */
		if( envelopeInfoPtr->blockSize > 1 && \
			!( envelopeInfoPtr->dataFlags & ENVDATA_ENDOFCONTENTS ) && \
			envelopeInfoPtr->blockBufferPos <= 0 )
			bytesToCopy -= envelopeInfoPtr->blockSize;

		/* If we've ended up with nothing to copy (e.g. due to blocking
		   requirements), exit */
		if( bytesToCopy <= 0 )
			{
			*length = oobBytesCopied;
			return( CRYPT_OK );
			}
		ENSURES( bytesToCopy > 0 && bytesToCopy < MAX_BUFFER_SIZE );

		/* If we've seen the end-of-contents octets and there's no payload
		   left to copy out, exit */
		if( ( envelopeInfoPtr->dataFlags & ENVDATA_ENDOFCONTENTS ) && \
			envelopeInfoPtr->dataLeft <= 0 )
			{
			*length = oobBytesCopied;
			return( CRYPT_OK );
			}

		/* If we're doing a lookahead read just copy the data out without
		   adjusting the read-data values */
		if( isLookaheadRead )
			{
			REQUIRES( bytesToCopy > 0 && bytesToCopy <= OOB_BUFFER_SIZE );

			memcpy( buffer, envelopeInfoPtr->buffer, bytesToCopy );
			*length = bytesToCopy;
			return( CRYPT_OK );
			}

		/* Hash the payload data if necessary.  This is used for signed/
		   authenticated-data hashing, PGP also hashes the payload for 
		   encrypted data when it's generating an MDC but that needs to be 
		   done as part of the decryption due to special-case processing 
		   requirements */
		if( ( envelopeInfoPtr->dataFlags & ENVDATA_HASHACTIONSACTIVE ) && \
			envelopeInfoPtr->usage != ACTION_CRYPT )
			{
			status = hashEnvelopeData( envelopeInfoPtr->actionList,
									   envelopeInfoPtr->buffer, 
									   bytesToCopy );
			if( cryptStatusError( status ) )
				return( status );
			}

		/* We're not using compression, copy the data across directly */
		memcpy( buffer, envelopeInfoPtr->buffer, bytesToCopy );
		bytesCopied = bytesToCopy;
		}
	ENSURES( envelopeInfoPtr->bufPos - bytesCopied >= 0 );

	/* Move any remaining data down to the start of the buffer  */
	remainder = envelopeInfoPtr->bufPos - bytesCopied;
	ENSURES( remainder >= 0 && remainder < MAX_BUFFER_SIZE && \
			 bytesCopied >= 0 && bytesCopied < MAX_BUFFER_SIZE && \
			 bytesCopied + remainder <= envelopeInfoPtr->bufSize );
	if( remainder > 0 && bytesCopied > 0 )
		{
		REQUIRES( rangeCheck( bytesCopied, remainder, 
							  envelopeInfoPtr->bufSize ) );
		memmove( envelopeInfoPtr->buffer, 
				 envelopeInfoPtr->buffer + bytesCopied, remainder );
		}
	envelopeInfoPtr->bufPos = remainder;

	/* If there's data following the payload, adjust the end-of-payload
	   pointer to reflect the data that we've just copied out */
	if( envelopeInfoPtr->dataLeft > 0 && bytesCopied > 0 )
		envelopeInfoPtr->dataLeft -= bytesCopied;
	ENSURES( envelopeInfoPtr->dataLeft >= 0 && \
			 envelopeInfoPtr->dataLeft < MAX_INTLENGTH );
	*length = oobBytesCopied + bytesToCopy;

	ENSURES( sanityCheck( envelopeInfoPtr ) );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Extra Data Management Functions						*
*																			*
****************************************************************************/

/* Synchronise the deenveloping data stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int syncDeenvelopeData( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							   INOUT STREAM *stream )
	{
	const long dataStartPos = stell( stream );
	const int oldBufPos = envelopeInfoPtr->bufPos;
	const int bytesLeftToCopy = sMemDataLeft( stream );
	int bytesCopied;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( dataStartPos >= 0 && dataStartPos < MAX_BUFFER_SIZE );

	/* After the envelope header has been processed, what's left is payload
	   data that requires special processing because of segmenting and
	   decryption and hashing requirements, so we feed it in via a
	   copyToDeenvelope() of the data in the buffer.  This is a rather ugly
	   hack, but it works because we're moving data backwards in the buffer
	   so there shouldn't be any problems for the rare instances where the
	   data overlaps.  In the worst case (PKCS #7/CMS short definite-length
	   OCTET STRING) we only consume two bytes, the tag and one-byte length,
	   but since we're using memmove() in copyData() this shouldn't be a
	   problem.

	   Since we're in effect restarting from the payload data, we reset
	   everything that counts to point back to the start of the buffer where
	   we'll be moving the payload data.  We don't have to worry about the
	   copyToDeenvelope() overflowing the envelope since the source is the
	   envelope buffer so the data must fit within the envelope */
	envelopeInfoPtr->bufPos = 0;
	if( bytesLeftToCopy <= 0 )
		{
		/* Handle the special case of the data ending at exactly this 
		   point.  There's nothing further to do since all data has been
		   consumed and the next push will give us the payload data */
		sseek( stream, 0 );
		return( CRYPT_OK );
		}
	sMemDisconnect( stream );
	sMemConnect( stream, envelopeInfoPtr->buffer, bytesLeftToCopy );
	bytesCopied = envelopeInfoPtr->copyToEnvelopeFunction( envelopeInfoPtr,
							envelopeInfoPtr->buffer + dataStartPos, 
							bytesLeftToCopy );
	if( cryptStatusError( bytesCopied ) )
		{
		/* Undo the buffer position reset.  This isn't 100% effective if
		   there are multiple segments present and we hit an error after
		   we've copied down enough data to overwrite what's at the start,
		   but in most cases it allows us to undo the copyToEnvelope(), and 
		   if the data is corrupted we won't be able to get any further 
		   anyway */
		envelopeInfoPtr->bufPos = oldBufPos;
		ENSURES( sanityCheck( envelopeInfoPtr ) );
		return( bytesCopied );
		}
	ENSURES( bytesCopied >= 0 && bytesCopied < MAX_BUFFER_SIZE );

	/* If we copied over less than the total available and have hit the
	   end-of-data marker it means that there's extra data following the
	   payload.  We need to move this down to the end of the decoded payload 
	   data since copyToDeenvelope() stops copying as soon as it hits the 
	   end-of-data.  We use memmove() rather than memcpy() for this since 
	   we're copying to/from the same buffer:

				  dataStartPos
						|
						v<--- bLeft --->|
		+---------------+---------------+--------
		|				|///////////////|			Start
		+---------------+---------------+--------

				  dataStartPos
						|
		|<- bCop -->|	v		|<-bToC>|
		+---------------+---------------+--------
		|//payload//|	|		|///////|			After copyToDeenvelope
		+---------------+---------------+--------
					^			^
					|			|
				dataLeft  dStartPos + bCopied

		+-----------+-------+--------------------
		|//payload//|///////|						EOC
		+-----------+-------+--------------------
					^		^
					|		|
				 dLeft	  bufPos */
	if( ( envelopeInfoPtr->dataFlags & ENVDATA_ENDOFCONTENTS ) && \
		bytesCopied < bytesLeftToCopy )
		{
		const int bytesToCopy = bytesLeftToCopy - bytesCopied;

		REQUIRES( bytesToCopy > 0 && bytesToCopy < MAX_BUFFER_SIZE && \
				  envelopeInfoPtr->dataLeft + \
						bytesToCopy <= envelopeInfoPtr->bufSize && \
				  bytesCopied + dataStartPos + \
						bytesToCopy <= envelopeInfoPtr->bufSize );
		memmove( envelopeInfoPtr->buffer + envelopeInfoPtr->dataLeft,
				 envelopeInfoPtr->buffer + dataStartPos + bytesCopied,
				 bytesToCopy );
		envelopeInfoPtr->bufPos = envelopeInfoPtr->dataLeft + bytesToCopy;

		ENSURES( sanityCheck( envelopeInfoPtr ) );
		return( CRYPT_OK );
		}

	/* At this point we've copied everything over */
	ENSURES( bytesCopied == bytesLeftToCopy );

	ENSURES( sanityCheck( envelopeInfoPtr ) );
	return( CRYPT_OK );
	}

/* Process additional out-of-band data that doesn't get copied into/out of
   the de-enveloping envelope */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processExtraData( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							 IN_BUFFER( length ) const void *buffer, 
							 IN_DATALENGTH_Z const int length )
	{
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( length == 0 || isReadPtr( buffer, length ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( length >= 0 && length < MAX_BUFFER_SIZE );

	/* If the hash value was supplied externally (which means that there's
	   nothing for us to hash since it's already been done by the caller),
	   there won't be any hash actions active and we can return immediately */
	if( !( envelopeInfoPtr->dataFlags & ( ENVDATA_HASHACTIONSACTIVE | \
										  ENVDATA_AUTHENCACTIONSACTIVE ) ) )
		return( ( length > 0 ) ? CRYPT_ERROR_BADDATA : CRYPT_OK );

	/* If we're still processing data, hash it.  Since authenticated-
	   encrypted content hashes the ciphertext before decryption rather than 
	   the plaintext we only perform hashing of nonzero-length data if we're 
	   working with straight signed or MACed data */
	if( length > 0 )
		{
		if( !( envelopeInfoPtr->dataFlags & ENVDATA_HASHACTIONSACTIVE ) )
			return( CRYPT_OK );
		return( hashEnvelopeData( envelopeInfoPtr->actionList, buffer, 
								  length ) );
		}

	/* We're finishing up the hashing, clear the hashing-active flag to
	   prevent data from being hashed again if it's processed by other
	   code such as copyFromDeenvelope() */
	status = hashEnvelopeData( envelopeInfoPtr->actionList, "", 0 );
	if( cryptStatusError( status ) )
		return( status );
	envelopeInfoPtr->dataFlags &= ~( ENVDATA_HASHACTIONSACTIVE | \
									 ENVDATA_AUTHENCACTIONSACTIVE );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Envelope Access Routines						*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initDeenvelopeStreaming( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	/* Set the access method pointers */
	envelopeInfoPtr->copyToEnvelopeFunction = copyToDeenvelope;
	envelopeInfoPtr->copyFromEnvelopeFunction = copyFromDeenvelope;
	envelopeInfoPtr->syncDeenvelopeData = syncDeenvelopeData;
	envelopeInfoPtr->processExtraData = processExtraData;
	}
#endif /* USE_ENVELOPES */
