/****************************************************************************
*																			*
*					  cryptlib Datagram Encoding Routines					*
*						Copyright Peter Gutmann 1996-2012					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "envelope.h"
#else
  #include "enc_dec/asn1.h"
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

	/* Make sure that the partial and out-of-band data buffer positions are 
	   clear (they're only used for de-enveloping) */
	if( envelopeInfoPtr->partialBufPos != 0 || \
		envelopeInfoPtr->oobBufSize != 0 )
		return( FALSE );

	/* If we're drained the envelope buffer, we're done */
	if( envelopeInfoPtr->segmentStart == 0 && \
		envelopeInfoPtr->segmentDataStart == 0 && \
		envelopeInfoPtr->bufPos == 0 )
		return( TRUE );

	/* Make sure that the buffer internal bookeeping is OK.  First we apply 
	   the general one-size-fits-all checks, then we apply further 
	   situation-specific checks */
	if( envelopeInfoPtr->segmentStart < 0 || \
		envelopeInfoPtr->segmentStart > envelopeInfoPtr->segmentDataStart || \
		envelopeInfoPtr->segmentStart > MAX_BUFFER_SIZE )
		return( FALSE );
	if( envelopeInfoPtr->segmentDataStart < 0 || \
		envelopeInfoPtr->segmentDataStart > envelopeInfoPtr->bufPos || \
		envelopeInfoPtr->segmentDataStart > MAX_BUFFER_SIZE )
		return( FALSE );
	
	/* The situation-specific checks get a bit complicated because we have 
	   to distinguish between definite- and indefinite-length encodings.  
	   For the definite length segmentStart == segmentDataStart since there 
	   are no intermediate segment headers, for the indefinite length 
	   segmentStart < segmentDataStart to accomodate the intervening header.

	   In some rare cases segmentDataStart can be the same as bufPos if 
	   we're using compression and all input data was absorbed by the 
	   zStream buffer so we check for segmentDataStart > bufPos rather than 
	   segmentDataStart >= bufPos */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED )
		{
		/* It's a non-segmenting encoding so segmentStart must track 
		   segmentDataStart */
		if( envelopeInfoPtr->segmentStart != \
						envelopeInfoPtr->segmentDataStart )
			return( FALSE );

		return( TRUE );
		}
	if( envelopeInfoPtr->segmentStart >= envelopeInfoPtr->bufPos )
		return( FALSE );
	if( envelopeInfoPtr->dataFlags & ENVDATA_SEGMENTCOMPLETE )
		{
		/* If we're just started a new segment and there's no data left in 
		   the envelope buffer, segmentStart may be the same as 
		   segmentDataStart */
		if( envelopeInfoPtr->segmentStart == 0 && \
			envelopeInfoPtr->segmentDataStart == 0 )
			return( TRUE );
		}
	if( envelopeInfoPtr->segmentStart >= envelopeInfoPtr->segmentDataStart )
		return( FALSE );

	return( TRUE );
	}

/* Apply the hash/MAC actions in the action list to data.  This function is 
   shared with decode.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int hashEnvelopeData( const ACTION_LIST *actionListPtr,
					  IN_BUFFER( dataLength ) const void *data, 
					  IN_LENGTH_Z const int dataLength )
	{
	int iterationCount, status;

	assert( isReadPtr( actionListPtr, sizeof( ACTION_LIST ) ) );
	assert( dataLength == 0 || isReadPtr( data, dataLength ) );

	REQUIRES( data != NULL );
	REQUIRES( dataLength >= 0 && dataLength < MAX_BUFFER_SIZE );

	for( iterationCount = 0;
		 actionListPtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 actionListPtr = actionListPtr->next, iterationCount++ )
		{
		/* If we're using authenticated encryption there may be other 
		   actions present in the action list so we only hash/MAC where
		   required */
		if( actionListPtr->action != ACTION_HASH && \
			actionListPtr->action != ACTION_MAC )
			continue;

		/* If the hashing has already been completed due to it being an
		   externally-supplied value for a detached signature, we don't need 
		   to do anything further */
		if( actionListPtr->flags & ACTION_HASHCOMPLETE )
			{
			REQUIRES( dataLength == 0 );

			continue;
			}

		status = krnlSendMessage( actionListPtr->iCryptHandle,
								  IMESSAGE_CTX_HASH, ( MESSAGE_CAST ) data, 
								  dataLength );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Header Processing Routines						*
*																			*
****************************************************************************/

/* Determine the length of the encoded length value and the threshold at which the
   length encoding changes for constructed indefinite-length strings.  The
   length encoding is the actual length if <= 127, or a one-byte length-of-
   length followed by the length if > 127 */

#define TAG_SIZE					1	/* Useful symbolic define */

#if INT_MAX > 32767

#define lengthOfLength( length )	( ( length < 0x80 ) ? 1 : \
									  ( length < 0x100 ) ? 2 : \
									  ( length < 0x10000 ) ? 3 : \
									  ( length < 0x1000000 ) ? 4 : 5 )

#define findThreshold( length )		( ( length < 0x80 ) ? 0x7F : \
									  ( length < 0x100 ) ? 0xFF : \
									  ( length < 0x10000 ) ? 0xFFFF : \
									  ( length < 0x1000000 ) ? 0xFFFFFF : INT_MAX )
#else

#define lengthOfLength( length )	( ( length < 0x80 ) ? 1 : \
									  ( length < 0x100 ) ? 2 : 3 )

#define findThreshold( length )		( ( length < 0x80 ) ? 127 : \
									  ( length < 0x100 ) ? 0xFF : INT_MAX )
#endif /* 32-bit ints */

/* Begin a new segment in the buffer.  The layout is:

		  bufPos
			v
			tag	len		 payload
	+-------+-+---+---------------------+-------+
	|		| |	  |						|		|
	+-------+-+---+---------------------+-------+
			^	  ^						^
			|	  |						|
		 sStart sDataStart			sDataEnd 

   If we're using a definite-length encoding then 
   segmentStart == segmentDataStart = bufPos */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int beginSegment( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	int segHeaderSize = 0;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* If we're using an indefinite-length encoding we have to factor in the 
	   length of the segment header.  Since we can't determine the overall
	   segment data size at this point we use a worst-case estimate in which
	   the segment fills the entire buffer */
	if( envelopeInfoPtr->payloadSize == CRYPT_UNUSED )
		{
		segHeaderSize = TAG_SIZE + \
						lengthOfLength( envelopeInfoPtr->bufSize );
		}

	/* Make sure that there's enough room in the buffer to accommodate the
	   start of a new segment.  In the worst case this is 6 bytes (OCTET
	   STRING tag + 5-byte length) + 15 bytes (blockBuffer contents for a
	   128-bit block cipher).  Although in practice we could eliminate this
	   condition, it would require tracking a lot of state information to
	   record which data had been encoded into the buffer and whether the
	   blockBuffer data had been copied into the buffer, so to keep it
	   simple we require enough room to do everything at once */
	if( envelopeInfoPtr->bufPos + segHeaderSize + \
			envelopeInfoPtr->blockBufferPos >= envelopeInfoPtr->bufSize )
		return( CRYPT_ERROR_OVERFLOW );

	/* Adjust the buffer position indicators to handle potential
	   intermediate headers */
	envelopeInfoPtr->segmentStart = envelopeInfoPtr->bufPos;
	if( envelopeInfoPtr->payloadSize == CRYPT_UNUSED )
		{
		/* Begin a new segment after the end of the current segment.  We
		   always leave enough room for the largest allowable length field
		   because we may have a short segment at the end of the buffer which
		   is moved to the start of the buffer after data is copied out,
		   turning it into a longer segment.  For this reason we rely on
		   completeSegment() to get the length right and move any data down
		   as required */
		envelopeInfoPtr->bufPos += segHeaderSize;
		}
	envelopeInfoPtr->segmentDataStart = envelopeInfoPtr->bufPos;
	ENSURES( envelopeInfoPtr->bufPos + \
			 envelopeInfoPtr->blockBufferPos <= envelopeInfoPtr->bufSize );

	/* Now copy anything left in the block buffer to the start of the new
	   segment.  We know that everything will fit because we've checked
	   earlier on that the header and blockbuffer contents will fit into
	   the remaining space */
	if( envelopeInfoPtr->blockBufferPos > 0 )
		{
		REQUIRES( rangeCheckZ( envelopeInfoPtr->bufPos,
							   envelopeInfoPtr->blockBufferPos,
							   envelopeInfoPtr->bufSize ) );
		memcpy( envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos,
				envelopeInfoPtr->blockBuffer, envelopeInfoPtr->blockBufferPos );
		envelopeInfoPtr->bufPos += envelopeInfoPtr->blockBufferPos;
		}
	envelopeInfoPtr->blockBufferPos = 0;

	/* We've started the new segment, mark it as incomplete */
	envelopeInfoPtr->dataFlags &= ~ENVDATA_SEGMENTCOMPLETE;

	ENSURES( sanityCheck( envelopeInfoPtr ) );
	return( CRYPT_OK );
	}

/* Complete a segment of data in the buffer.  This is incredibly complicated
   because we need to take into account the indefinite-length encoding (which
   has a variable-size length field) and the quantization to the cipher block
   size.  In particular the indefinite-length encoding means that we can
   never encode a block with a size of 130 bytes (we get tag + length + 127 =
   129, then tag + length-of-length + length + 128 = 131), and the same for
   the next boundary at 256 bytes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int encodeSegmentHeader( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	STREAM stream;
	const BOOLEAN isEncrypted = \
			( envelopeInfoPtr->iCryptContext != CRYPT_ERROR ) ? TRUE : FALSE;
	const int oldHdrLen = envelopeInfoPtr->segmentDataStart - \
						  envelopeInfoPtr->segmentStart;
	BOOLEAN needsPadding = \
			( envelopeInfoPtr->dataFlags & ENVDATA_NEEDSPADDING ) ? TRUE : FALSE;
	int dataLen = envelopeInfoPtr->bufPos - envelopeInfoPtr->segmentDataStart;
	int hdrLen, remainder = 0, status;

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* If we're adding PKCS #5 padding try and add one block's worth of
	   pseudo-data.  This adjusted data length is then fed into the block
	   size quantisation process, after which any odd-sized remainder is
	   ignored, and the necessary padding bytes are added to account for the
	   difference between the actual and padded size */
	if( needsPadding )
		{
		/* Check whether the padding will fit onto the end of the data.  This
		   check isn't completely accurate since the length encoding might
		   shrink by one or two bytes and allow a little extra data to be
		   squeezed in, however the extra data could cause the length
		   encoding to expand again, requiring a complex adjustment process.
		   To make things easier we ignore this possibility at the expense of
		   emitting one more segment than is necessary in a few very rare
		   cases */
		if( envelopeInfoPtr->segmentDataStart + dataLen + \
			envelopeInfoPtr->blockSize < envelopeInfoPtr->bufSize )
			dataLen += envelopeInfoPtr->blockSize;
		else
			needsPadding = FALSE;
		}
	ENSURES( dataLen > 0 && envelopeInfoPtr->segmentDataStart + \
									dataLen <= envelopeInfoPtr->bufSize );

	/* Now that we've made any necessary adjustments to the data length,
	   determine the length of the length encoding (which may have grown or
	   shrunk since we initially calculated it when we began the segment) */
	hdrLen = ( envelopeInfoPtr->payloadSize == CRYPT_UNUSED ) ? \
			 TAG_SIZE + lengthOfLength( dataLen ) : 0;

	/* Quantize and adjust the length if we're encrypting in a block mode:

			   segDataStart					  bufPos
		segStart	|							|
			v		v<--------- dLen ---------->v
		----+-------+---------------------------+
			|  hdr	|///////////////////////|	|
		----+-------+---------------------------+
					|<------ qTotLen ------>|<+>|
											  |
										  remainder */
	if( isEncrypted )
		{
		int quantisedTotalLen, threshold;

		/* Determine the length due to cipher block-size quantisation */
		quantisedTotalLen = dataLen & envelopeInfoPtr->blockSizeMask;

		/* If the block-size quantisation has moved the quantised length
		   across a length-of-length encoding boundary, adjust hdrLen to
		   account for this */
		threshold = findThreshold( quantisedTotalLen );
		if( quantisedTotalLen <= threshold && dataLen > threshold )
			hdrLen--;

		/* Remember how many bytes we can't fit into the current block
		   (these will be copied into the block buffer for later use), and
		   the new size of the data due to quantisation */
		remainder = dataLen - quantisedTotalLen;
		dataLen = quantisedTotalLen;
		}
	ENSURES( ( envelopeInfoPtr->payloadSize != CRYPT_UNUSED && hdrLen == 0 ) || \
			 ( envelopeInfoPtr->payloadSize == CRYPT_UNUSED && \
			   hdrLen > 0 && hdrLen <= TAG_SIZE + 5 ) );
	ENSURES( ( envelopeInfoPtr->blockSize == 0 && remainder == 0 ) || \
			 ( envelopeInfoPtr->blockSize > 0 && \
			   remainder >= 0 && remainder < envelopeInfoPtr->blockSize && \
			   remainder <= CRYPT_MAX_IVSIZE  ) );

	/* If there's not enough data present to do anything, tell the caller */
	if( dataLen <= 0 )
		return( CRYPT_ERROR_UNDERFLOW );
	ENSURES( dataLen > 0 && dataLen < MAX_BUFFER_SIZE );

	/* If there's a header between segments and the header length encoding
	   has shrunk (either due to the cipher block size quantization
	   shrinking the segment or because we've wrapped up a segment at less
	   than the original projected length), move the data down.  In the
	   worst case the shrinking can cover several bytes if we go from a
	   > 255 byte segment to a <= 127 byte one:

			   segDataStart					  bufPos
		segStart	|							|
			v		v							v
		----+-------+---------------------------+
			|  hdr	|///////////////////////////|	Before
		----+-------+---------------------------+
			|<--+-->|
				|
			oldHdrLen

		   segDataStart'				 bufPos'
		segStart|							|
			v	v							v
		----+-------+-----------------------+---+
			|hdr|///////////////////////////|	|	After
		----+-------+-----------------------+---+
			|<+>|<+>|						|<+>|
			  |	  |							  |
			  | delta						delta
			hdrLen */
	if( hdrLen > 0 && hdrLen < oldHdrLen )
		{
		BYTE *segmentDataPtr = envelopeInfoPtr->buffer + \
							   envelopeInfoPtr->segmentStart;
		const int delta = oldHdrLen - hdrLen;

		memmove( segmentDataPtr + hdrLen, segmentDataPtr + oldHdrLen,
				 envelopeInfoPtr->bufPos - envelopeInfoPtr->segmentDataStart );
		envelopeInfoPtr->bufPos -= delta;
		envelopeInfoPtr->segmentDataStart -= delta;
		}
	ENSURES( sanityCheck( envelopeInfoPtr ) );
	ENSURES( envelopeInfoPtr->segmentDataStart + \
						dataLen <= envelopeInfoPtr->bufSize );

	/* If we need to add PKCS #5 block padding, do so now (we know from the
	   needsPadding and quantisedTotalLen check above that there's enough 
	   room for this).  Since the extension of the data length to allow for 
	   padding data is performed by adding one block of pseudo-data and 
	   letting the block quantisation system take care of any discrepancies 
	   we can calculate the padding amount as the difference between any 
	   remainder after quantisation and the block size */
	if( needsPadding )
		{
		const int padSize = envelopeInfoPtr->blockSize - remainder;
		int i;

		ENSURES( padSize > 0 && padSize <= envelopeInfoPtr->blockSize && \
				 envelopeInfoPtr->bufPos + \
						padSize <= envelopeInfoPtr->bufSize );

		/* Add the block padding and set the remainder to zero, since we're
		   now at an even block boundary */
		for( i = 0; i < padSize; i++ )
			{
			envelopeInfoPtr->buffer[ envelopeInfoPtr->bufPos + i ] = \
												intToByte( padSize );
			}
		envelopeInfoPtr->bufPos += padSize;
		envelopeInfoPtr->dataFlags &= ~ENVDATA_NEEDSPADDING;
		ENSURES( envelopeInfoPtr->bufPos >= 0 && \
				 envelopeInfoPtr->bufPos <= envelopeInfoPtr->bufSize );
		}
	else
		{
		/* If there are any bytes left over move them across into the block 
		   buffer */
		if( remainder > 0 )
			{
			REQUIRES( envelopeInfoPtr->bufPos > remainder );
			memcpy( envelopeInfoPtr->blockBuffer,
					envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos - \
											  remainder, remainder );
			envelopeInfoPtr->blockBufferPos = remainder;
			envelopeInfoPtr->bufPos -= remainder;
			}
		}

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	/* If we're using the definite length form, exit */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED )
		return( CRYPT_OK );

	/* Insert the OCTET STRING header into the data stream */
	sMemOpen( &stream, envelopeInfoPtr->buffer + \
					   envelopeInfoPtr->segmentStart, hdrLen );
	status = writeOctetStringHole( &stream, dataLen, DEFAULT_TAG );
	ENSURES( cryptStatusOK( status ) && stell( &stream ) == hdrLen );
	sMemDisconnect( &stream );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int completeSegment( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							const BOOLEAN forceCompletion )
	{
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* If we're enveloping data using indefinite encoding and we're not at
	   the end of the data, don't emit a sub-segment containing less then 32
	   bytes of data.  This is to protect against users who write code that
	   performs byte-at-a-time enveloping, at least we can quantize the data
	   amount to make it slightly more efficient.  The reason for the
	   magic value of 32 bytes is that with a default buffer size of 32K 
	   the loop exit at FAILSAFE_ITERATIONS_LARGE in copyToDeenvelope() 
	   won't be triggered if the user's code produces worst-case conditions.
	   
	   As a side-effect it avoids occasional inefficiencies at boundaries 
	   where one or two bytes may still be hanging around from a previous 
	   data block since they'll be coalesced into the following block */
	if( !forceCompletion && \
		envelopeInfoPtr->payloadSize == CRYPT_UNUSED && \
		( envelopeInfoPtr->bufPos - envelopeInfoPtr->segmentDataStart ) < 32 )
		{
		/* We can't emit any of the small sub-segment, however there may be
		   (non-)data preceding this that we can hand over so we set the
		   segment data end value to the start of the segment */
		envelopeInfoPtr->segmentDataEnd = envelopeInfoPtr->segmentStart;
		return( CRYPT_OK );
		}

	/* Wrap up the segment */
	if( !( envelopeInfoPtr->dataFlags & ENVDATA_NOSEGMENT ) )
		{
		status = encodeSegmentHeader( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( envelopeInfoPtr->iCryptContext != CRYPT_ERROR )
		{
		void *dataPtr = envelopeInfoPtr->buffer + \
						envelopeInfoPtr->segmentDataStart;
		const int dataLen = envelopeInfoPtr->bufPos - \
							envelopeInfoPtr->segmentDataStart;

		status = krnlSendMessage( envelopeInfoPtr->iCryptContext,
								  IMESSAGE_CTX_ENCRYPT, dataPtr, dataLen );
		if( cryptStatusError( status ) )
			return( status );
		if( envelopeInfoPtr->dataFlags & ENVDATA_AUTHENCACTIONSACTIVE )
			{
			/* We're performing authenticated encryotion, hash the 
			   ciphertext now that it's available */
			status = hashEnvelopeData( envelopeInfoPtr->actionList, dataPtr,
									   dataLen );
			if( cryptStatusError( status ) )
				return( status );
			}
		}

	/* Remember how much data is now available to be read out */
	envelopeInfoPtr->segmentDataEnd = envelopeInfoPtr->bufPos;

	/* Mark this segment as complete */
	envelopeInfoPtr->dataFlags |= ENVDATA_SEGMENTCOMPLETE;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Copy to Envelope							*
*																			*
****************************************************************************/

/* Flush any remaining data through into the envelope buffer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int flushEnvelopeData( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	BOOLEAN needNewSegment = \
				( envelopeInfoPtr->dataFlags & ENVDATA_NEEDSPADDING ) ? \
				TRUE : FALSE;
	int status;

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* If we're using an explicit payload length make sure that we copied in 
	   as much data as was explicitly declared */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED && \
		envelopeInfoPtr->segmentSize != 0 )
		return( CRYPT_ERROR_UNDERFLOW );

#ifdef USE_COMPRESSION
	/* If we're using compression, flush any remaining data out of the
	   zStream */
	if( envelopeInfoPtr->flags & ENVELOPE_ZSTREAMINITED )
		{
		int bytesToCopy;

		/* If we've just completed a segment, begin a new one.  This action
		   is slightly anomalous in that normally a flush can't add more
		   data to the envelope and so we'd never need to start a new
		   segment during a flush, however since we can have arbitrarily
		   large amounts of data trapped in subspace via zlib we need to be
		   able to handle starting new segments at this point */
		if( envelopeInfoPtr->dataFlags & ENVDATA_SEGMENTCOMPLETE )
			{
			status = beginSegment( envelopeInfoPtr );
			if( cryptStatusError( status ) )
				return( status );
			if( envelopeInfoPtr->bufPos >= envelopeInfoPtr->bufSize )
				return( CRYPT_ERROR_OVERFLOW );
			}

		/* Flush any remaining compressed data into the envelope buffer */
		bytesToCopy = envelopeInfoPtr->bufSize - envelopeInfoPtr->bufPos;
		envelopeInfoPtr->zStream.next_in = NULL;
		envelopeInfoPtr->zStream.avail_in = 0;
		envelopeInfoPtr->zStream.next_out = envelopeInfoPtr->buffer + \
											envelopeInfoPtr->bufPos;
		envelopeInfoPtr->zStream.avail_out = bytesToCopy;
		status = deflate( &envelopeInfoPtr->zStream, Z_FINISH );
		if( status != Z_STREAM_END && status != Z_OK )
			{
			/* There was some problem other than the output buffer being
			   full */
			retIntError();
			}

		/* Adjust the status information based on the data flushed out of
		   the zStream.  We don't have to check for the output buffer being
		   full because this case is already handled by the check of the
		   deflate() return value */
		envelopeInfoPtr->bufPos += bytesToCopy - \
								   envelopeInfoPtr->zStream.avail_out;
		ENSURES( envelopeInfoPtr->bufPos >= 0 && \
				 envelopeInfoPtr->bufPos <= envelopeInfoPtr->bufSize );

		/* If we didn't finish flushing data because the output buffer is
		   full, complete the segment and tell the caller that they need to
		   pop some data */
		if( status == Z_OK )
			{
			status = completeSegment( envelopeInfoPtr, TRUE );
			return( cryptStatusError( status ) ? \
					status : CRYPT_ERROR_OVERFLOW );
			}
		}
#endif /* USE_COMPRESSION */

	/* If we're encrypting data with a block cipher we need to add PKCS #5
	   padding at the end of the last block */
	if( envelopeInfoPtr->blockSize > 1 )
		{
		envelopeInfoPtr->dataFlags |= ENVDATA_NEEDSPADDING;
		if( envelopeInfoPtr->dataFlags & ENVDATA_SEGMENTCOMPLETE )
			{
			/* The current segment has been wrapped up, we need to begin a
			   new segment to contain the padding */
			needNewSegment = TRUE;
			}
		}

	/* If we're carrying over the padding requirement from a previous block 
	   we need to begin a new block before we can try and add the padding.
	   This can happen if there was data left after the previous segment was
	   completed or if the addition of padding would have overflowed the
	   buffer when the segment was completed, in other words if the
	   needPadding flag is still set from the previous call */
	if( needNewSegment )
		{
		status = beginSegment( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		if( envelopeInfoPtr->bufPos >= envelopeInfoPtr->bufSize )
			{
			/* We've filled the envelope buffer with the new segment header,
			   we can't copy in anything */
			return( CRYPT_ERROR_OVERFLOW );
			}
		}

	/* Complete the segment if necessary */
	if( !( envelopeInfoPtr->dataFlags & ENVDATA_SEGMENTCOMPLETE ) || \
		( envelopeInfoPtr->dataFlags & ENVDATA_NEEDSPADDING ) )
		{
		status = completeSegment( envelopeInfoPtr, TRUE );
		if( cryptStatusError( status ) )
			return( status );

		/* If there wasn't sufficient room to add the trailing PKCS #5 
		   padding tell the caller to try again */
		if( envelopeInfoPtr->dataFlags & ENVDATA_NEEDSPADDING )
			return( CRYPT_ERROR_OVERFLOW );
		}

	/* If there's no hashing being performed or we've completed the hashing, 
	   we're done.  In addition unlike CMS, PGP handles authenticated 
	   attributes by extending the hashing of the payload data to cover the 
	   additional attributes, so if we're using the PGP format we can't wrap 
	   up the hashing yet */
	if( !( envelopeInfoPtr->dataFlags & ( ENVDATA_HASHACTIONSACTIVE | \
										  ENVDATA_AUTHENCACTIONSACTIVE ) ) || \
		envelopeInfoPtr->type == CRYPT_FORMAT_PGP )
		return( CRYPT_OK );

	/* We've finished processing everything, complete each hash action */
	return( hashEnvelopeData( envelopeInfoPtr->actionList, "", 0 ) );
	}

/* Copy data into the envelope.  Returns the number of bytes copied or an
   overflow error if we're trying to flush data and there isn't room to
   perform the flush (this somewhat peculiar case is because the caller
   expects to have 0 bytes copied in this case) */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
static int copyToEnvelope( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						   IN_BUFFER_OPT( length ) const BYTE *buffer, 
						   IN_LENGTH_Z const int length )
	{
	BOOLEAN needCompleteSegment = FALSE;
	BYTE *bufPtr;
	int bytesToCopy, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( length == 0 || isReadPtr( buffer, length ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( ( buffer == NULL && length == 0 ) || \
			  ( buffer != NULL && length >= 0 && length < MAX_BUFFER_SIZE ) );

	/* If we're trying to copy into a full buffer, return a count of 0 bytes
	   unless we're trying to flush the buffer (the calling routine may
	   convert the zero byte count to an overflow error if necessary) */
	if( envelopeInfoPtr->bufPos >= envelopeInfoPtr->bufSize )
		return( ( length > 0 ) ? 0 : CRYPT_ERROR_OVERFLOW );

	/* If we're generating a detached signature just hash the data and exit */
	if( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG )
		{
		/* Unlike CMS, PGP handles authenticated attributes by extending the 
		   hashing of the payload data to cover the additional attributes 
		   so if this is a flush and we're using the PGP format then we 
		   can't wrap up the hashing yet */
		if( length <= 0 && envelopeInfoPtr->type == CRYPT_FORMAT_PGP )
			return( 0 );

		status = hashEnvelopeData( envelopeInfoPtr->actionList, 
								   ( length > 0 ) ? \
										buffer : ( const void * ) "", length );
										/* Cast needed for gcc */
		return( cryptStatusError( status ) ? status : length );
		}

	/* If we're flushing data, wrap up the segment and exit.  An OK status 
	   translates to zero bytes copied */
	if( length <= 0 )
		{
		status = flushEnvelopeData( envelopeInfoPtr );
		return( cryptStatusError( status ) ? status : 0 );
		}

	/* If we're using an explicit payload length make sure that we don't try 
	   and copy in more data than has been explicitly declared */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED && \
		length > envelopeInfoPtr->segmentSize )
		return( CRYPT_ERROR_OVERFLOW );

	/* If we've just completed a segment, begin a new one before we add any
	   data */
	if( envelopeInfoPtr->dataFlags & ENVDATA_SEGMENTCOMPLETE )
		{
		status = beginSegment( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( ( status == CRYPT_ERROR_OVERFLOW ) ? 0 : status );
		if( envelopeInfoPtr->bufPos >= envelopeInfoPtr->bufSize )
			{
			/* We've filled the envelope buffer with the new segment header,
			   we can't copy in anything */
			return( 0 );
			}
		}

	/* Copy over as much as we can fit into the envelope buffer */
	bufPtr = envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos;
	bytesToCopy = envelopeInfoPtr->bufSize - envelopeInfoPtr->bufPos;
	ENSURES( bytesToCopy > 0 && \
			 envelopeInfoPtr->bufPos + \
					bytesToCopy <= envelopeInfoPtr->bufSize );
#ifdef USE_COMPRESSION
	if( envelopeInfoPtr->flags & ENVELOPE_ZSTREAMINITED )
		{
		/* Compress the data into the envelope buffer */
		envelopeInfoPtr->zStream.next_in = ( BYTE * ) buffer;
		envelopeInfoPtr->zStream.avail_in = length;
		envelopeInfoPtr->zStream.next_out = bufPtr;
		envelopeInfoPtr->zStream.avail_out = bytesToCopy;
		status = deflate( &envelopeInfoPtr->zStream, Z_NO_FLUSH );
		if( status != Z_OK )
			{
			/* There was some problem other than the output buffer being
			   full */
			retIntError();
			}

		/* Adjust the status information based on the data copied into the
		   zStream and flushed from the zStream into the buffer */
		envelopeInfoPtr->bufPos += bytesToCopy - \
								   envelopeInfoPtr->zStream.avail_out;
		bytesToCopy = length - envelopeInfoPtr->zStream.avail_in;

		/* If the buffer is full (there's no more room left for further
		   input) we need to close off the segment */
		if( envelopeInfoPtr->zStream.avail_out <= 0 )
			needCompleteSegment = TRUE;
		}
	else
#endif /* USE_COMPRESSION */
		{
		/* We're not using compression */
		if( bytesToCopy > length )
			bytesToCopy = length;
		memcpy( bufPtr, buffer, bytesToCopy );
		envelopeInfoPtr->bufPos += bytesToCopy;

		/* Hash the data if necessary */
		if( envelopeInfoPtr->dataFlags & ENVDATA_HASHACTIONSACTIVE )
			{
			status = hashEnvelopeData( envelopeInfoPtr->actionList, bufPtr,
									   bytesToCopy );
			if( cryptStatusError( status ) )
				return( status );
			}

		/* If the buffer is full (i.e. we've been fed more input data than we
		   could copy into the buffer) we need to close off the segment */
		if( bytesToCopy < length )
			needCompleteSegment = TRUE;
		}

	/* Adjust the bytes-left counter if necessary */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED )
		envelopeInfoPtr->segmentSize -= bytesToCopy;

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	/* Close off the segment if necessary */
	if( needCompleteSegment )
		{
		status = completeSegment( envelopeInfoPtr, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( bytesToCopy );
	}

/****************************************************************************
*																			*
*								Copy from Envelope							*
*																			*
****************************************************************************/

/* Copy data from the envelope and begin a new segment in the newly-created
   room.  If called with a zero length value this will create a new segment
   without moving any data.  Returns the number of bytes copied */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int copyFromEnvelope( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
							 OUT_BUFFER( maxLength, *length ) BYTE *buffer, 
							 IN_DATALENGTH const int maxLength, 
							 OUT_DATALENGTH_Z int *length, 
							 IN_FLAGS_Z( ENVCOPY ) const int flags )
	{
	int bytesToCopy = maxLength, remainder;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( maxLength == 0 || isWritePtr( buffer, maxLength ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES( flags == ENVCOPY_FLAG_NONE );

	/* Clear return values */
	memset( buffer, 0, min( 16, maxLength ) );
	*length = 0;

	/* If the caller wants more data than there is available in the set of
	   completed segments try to wrap up the next segment to make more data
	   available */
	if( bytesToCopy > envelopeInfoPtr->segmentDataEnd )
		{
		/* Try and complete the segment if necessary.  This may not be
		   possible if we're using a block encryption mode and there isn't
		   enough room at the end of the buffer to encrypt a full block.  In
		   addition if we're generating a detached signature the data is
		   communicated out-of-band so there's no segmenting */
		if( !( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG ) && \
			!( envelopeInfoPtr->dataFlags & ENVDATA_SEGMENTCOMPLETE ) )
			{
			const int status = completeSegment( envelopeInfoPtr, FALSE );
			if( cryptStatusError( status ) )
				return( status );
			}

		/* Return all of the data that we've got */
		if( envelopeInfoPtr->segmentDataEnd < bytesToCopy )
			bytesToCopy = envelopeInfoPtr->segmentDataEnd;
		}
	remainder = envelopeInfoPtr->bufPos - bytesToCopy;
	ENSURES( bytesToCopy >= 0 && bytesToCopy <= envelopeInfoPtr->bufPos );
	ENSURES( remainder >= 0 && remainder <= envelopeInfoPtr->bufPos );

	/* Copy the data out and move any remaining data down to the start of the
	   buffer  */
	if( bytesToCopy > 0 )
		{
		memcpy( buffer, envelopeInfoPtr->buffer, bytesToCopy );

		/* Move any remaining data down in the buffer */
		if( remainder > 0 )
			{
			REQUIRES( rangeCheck( bytesToCopy, remainder,
								  envelopeInfoPtr->bufPos ) );
			memmove( envelopeInfoPtr->buffer,
					 envelopeInfoPtr->buffer + bytesToCopy, remainder );
			}
		envelopeInfoPtr->bufPos = remainder;

		/* Update the segment location information.  The segment start
		   values track the start position of the last completed segment and
		   aren't updated until we begin a new segment so they may 
		   temporarily go negative at this point when the data from the last
		   completed segment is moved past the start of the buffer.  If this
		   happens we set them to a safe value of zero to ensure that they
		   pass the sanity checks elsewhere in the code */
		envelopeInfoPtr->segmentStart -= bytesToCopy;
		if( envelopeInfoPtr->segmentStart < 0 )
			envelopeInfoPtr->segmentStart = 0;
		envelopeInfoPtr->segmentDataStart -= bytesToCopy;
		if( envelopeInfoPtr->segmentDataStart < 0 )
			envelopeInfoPtr->segmentDataStart = 0;
		envelopeInfoPtr->segmentDataEnd -= bytesToCopy;
		ENSURES( envelopeInfoPtr->segmentDataEnd >= 0 && \
				 envelopeInfoPtr->segmentDataEnd < MAX_BUFFER_SIZE );
		}
	*length = bytesToCopy;

	ENSURES( sanityCheck( envelopeInfoPtr ) );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Envelope Access Routines						*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initEnvelopeStreaming( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	/* Set the access method pointers */
	envelopeInfoPtr->copyToEnvelopeFunction = copyToEnvelope;
	envelopeInfoPtr->copyFromEnvelopeFunction = copyFromEnvelope;
	}
#endif /* USE_ENVELOPES */
