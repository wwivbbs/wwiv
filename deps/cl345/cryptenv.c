/****************************************************************************
*																			*
*						cryptlib Enveloping Routines						*
*					  Copyright Peter Gutmann 1996-2012						*
*																			*
****************************************************************************/

#include "crypt.h"
#ifdef INC_ALL
  #include "envelope.h"
#else
  #include "envelope/envelope.h"
#endif /* Compiler-specific includes */

/* The default size for the envelope buffer.  On 16-bit systems they're
   smaller because of memory and int size limitations */

#if defined( CONFIG_CONSERVE_MEMORY )
  #define DEFAULT_BUFFER_SIZE		8192
#elif INT_MAX <= 32767
  #define DEFAULT_BUFFER_SIZE		16384
#else
  #define DEFAULT_BUFFER_SIZE		32768
#endif /* OS-specific envelope size defines */

/* When pushing and popping data, overflow and underflow errors can be
   recovered from by adding or removing data so we don't retain the error
   state for these error types */

#define isRecoverableError( status ) \
		( ( status ) == CRYPT_ERROR_OVERFLOW || \
		  ( status ) == CRYPT_ERROR_UNDERFLOW )

#ifdef USE_ENVELOPES

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check overall envelope data.  This is a general check for 
   reasonable values that's more targeted at catching inadvertent memory 
   corruption than a strict sanity check, format-specific sanity checks are
   performed by individual modules */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckEnvelope( const ENVELOPE_INFO *envelopeInfoPtr )
	{
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Check overall envelope data.  The contentType and usage can be un-set 
	   until data is pushed into the envelope, so we allow _NONE values for
	   these as well as actual values */
	if( !isEnumRange( envelopeInfoPtr->type, CRYPT_FORMAT ) || \
		!isEnumRangeOpt( envelopeInfoPtr->contentType, CRYPT_CONTENT ) || \
		!isEnumRangeOpt( envelopeInfoPtr->usage, ACTION ) || \
		envelopeInfoPtr->version < 0 || \
		envelopeInfoPtr->version > 10 )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: General info" ));
		return( FALSE );
		}
	if( !CHECK_FLAGS( envelopeInfoPtr->flags, ENVELOPE_FLAG_NONE, 
					  ENVELOPE_FLAG_MAX ) || \
		!CHECK_FLAGS( envelopeInfoPtr->dataFlags, ENVDATA_FLAG_NONE, 
					  ENVDATA_FLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Flags" ));
		return( FALSE );
		}

	/* Check safe pointers.  We don't have to check the function pointers
	   because they're validated each time they're dereferenced */
	if( !DATAPTR_ISVALID( envelopeInfoPtr->preActionList ) || \
		!DATAPTR_ISVALID( envelopeInfoPtr->actionList ) || \
		!DATAPTR_ISVALID( envelopeInfoPtr->postActionList ) || \
		!DATAPTR_ISVALID( envelopeInfoPtr->lastAction ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Action lists" ));
		return( FALSE );
		}
	if( !DATAPTR_ISVALID( envelopeInfoPtr->contentList ) || \
		!DATAPTR_ISVALID( envelopeInfoPtr->contentListCurrent ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Content list" ));
		return( FALSE );
		}

	/* Check encryption action values */
	if( envelopeInfoPtr->cryptActionSize != CRYPT_UNUSED && \
		!isShortIntegerRange( envelopeInfoPtr->cryptActionSize ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Crypt actions" ));
		return( FALSE );
		}
	if( envelopeInfoPtr->signActionSize != CRYPT_UNUSED && \
		!isShortIntegerRange( envelopeInfoPtr->signActionSize ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Sign actions" ));
		return( FALSE );
		}
	if( !isShortIntegerRange( envelopeInfoPtr->extraDataSize ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Extra data" ));
		return( FALSE );
		}

	/* Check buffer values */
	if( envelopeInfoPtr->bufSize < MIN_BUFFER_SIZE || \
		envelopeInfoPtr->bufSize >= MAX_BUFFER_SIZE )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Buffer size" ));
		return( FALSE );
		}
	if( envelopeInfoPtr->buffer == NULL )
		{
		if( envelopeInfoPtr->bufPos != 0 )
			{
			DEBUG_PUTS(( "sanityCheckEnvelope: Spurious buffer info" ));
			return( FALSE );
			}
		}
	else
		{
		if( envelopeInfoPtr->bufPos < 0 || \
			envelopeInfoPtr->bufPos > envelopeInfoPtr->bufSize )
			{
			DEBUG_PUTS(( "sanityCheckEnvelope: Buffer info" ));
			return( FALSE );
			}
		if( !safeBufferCheck( envelopeInfoPtr->buffer, 
							  envelopeInfoPtr->bufSize ) )
			{
			DEBUG_PUTS(( "sanityCheckEnvelope: Buffer state" ));
			return( FALSE );
			}
		}
	if( envelopeInfoPtr->auxBuffer == NULL )
		{
		if( envelopeInfoPtr->auxBufPos != 0 || \
			envelopeInfoPtr->auxBufSize != 0 )
			{
			DEBUG_PUTS(( "sanityCheckEnvelope: Spurious auxiliary buffer" ));
			return( FALSE );
			}
		}
	else
		{
		if( envelopeInfoPtr->auxBufSize <= 0 || \
			envelopeInfoPtr->auxBufSize > MAX_BUFFER_SIZE || \
			envelopeInfoPtr->auxBufPos < 0 || \
			envelopeInfoPtr->auxBufPos > envelopeInfoPtr->auxBufSize )
			{
			DEBUG_PUTS(( "sanityCheckEnvelope: Auxiliary buffer info" ));
			return( FALSE );
			}
		}
	if( envelopeInfoPtr->partialBufPos < 0 || \
		envelopeInfoPtr->partialBufPos > PARTIAL_BUFFER_SIZE )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Partial buffer info" ));
		return( FALSE );
		}
	if( envelopeInfoPtr->blockBufferPos < 0 || \
		envelopeInfoPtr->blockBufferPos >= CRYPT_MAX_IVSIZE || \
		envelopeInfoPtr->blockSize < 0 || \
		envelopeInfoPtr->blockSize > CRYPT_MAX_IVSIZE )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Block buffer info" ));
		return( FALSE );
		}
	if( envelopeInfoPtr->oobEventCount < 0 || \
		envelopeInfoPtr->oobEventCount > 10 || \
		!isShortIntegerRange( envelopeInfoPtr->oobDataLeft ) || \
		envelopeInfoPtr->oobBufSize < 0 || \
		envelopeInfoPtr->oobBufSize > OOB_BUFFER_SIZE )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: OOB info" ));
		return( FALSE );
		}

	/* Check state information */
	if( !isEnumRangeOpt( envelopeInfoPtr->state, ENVELOPE_STATE ) || \
		!isEnumRangeOpt( envelopeInfoPtr->envState, ENVSTATE ) || \
		!isEnumRangeOpt( envelopeInfoPtr->deenvState, DEENVSTATE ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: State" ));
		return( FALSE );
		}
#ifdef USE_PGP
	if( !isEnumRangeOpt( envelopeInfoPtr->pgpDeenvState, PGP_DEENVSTATE ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: PGP state" ));
		return( FALSE );
		}
#endif /* USE_PGP */

	/* Check data size information */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED && \
		!isIntegerRange( envelopeInfoPtr->payloadSize ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Payload size" ));
		return( FALSE );
		}
	if( !isIntegerRange( envelopeInfoPtr->segmentSize ) || \
		!isIntegerRange( envelopeInfoPtr->dataLeft ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Segment size" ));
		return( FALSE );
		}

	/* Check segmentation values */
	if( !isIntegerRange( envelopeInfoPtr->segmentStart ) || \
		!isIntegerRange( envelopeInfoPtr->segmentDataStart ) || \
		!isIntegerRange( envelopeInfoPtr->segmentDataEnd ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Segment info" ));
		return( FALSE );
		}

	/* Check associated handles */
	if( !isHandleRangeValid( envelopeInfoPtr->objectHandle ) || \
		!( envelopeInfoPtr->ownerHandle == DEFAULTUSER_OBJECT_HANDLE || \
		   isHandleRangeValid( envelopeInfoPtr->ownerHandle ) ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Object handles" ));
		return( FALSE );
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*							Envelope Data Handling Functions				*
*																			*
****************************************************************************/

/* Push data into an envelope */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int envelopePush( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
						 IN_BUFFER_OPT( length ) const void *buffer,
						 IN_DATALENGTH_Z const int length, 
						 OUT_DATALENGTH_Z int *bytesCopied )
	{
	const ENV_PROCESSPOSTAMBLE_FUNCTION processPostambleFunction = \
				( ENV_PROCESSPOSTAMBLE_FUNCTION ) \
				FNPTR_GET( envelopeInfoPtr->processPostambleFunction );
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( ( buffer == NULL && length == 0 ) || \
			isReadPtrDynamic( buffer, length ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( ( buffer == NULL && length == 0 ) || \
			  ( buffer != NULL && length > 0 && length < MAX_BUFFER_SIZE ) );
	REQUIRES( processPostambleFunction != NULL );

	/* Clear return value */
	*bytesCopied = 0;

	/* If we haven't started processing data yet, handle the initial data
	   specially */
	if( envelopeInfoPtr->state == ENVELOPE_STATE_PREDATA )
		{
		const ENV_CHECKMISSINGINFO_FUNCTION checkMissingInfoFunction = \
					( ENV_CHECKMISSINGINFO_FUNCTION ) \
					FNPTR_GET( envelopeInfoPtr->checkMissingInfoFunction );
		const ENV_PROCESSPREAMBLE_FUNCTION processPreambleFunction = \
					( ENV_PROCESSPREAMBLE_FUNCTION ) \
					FNPTR_GET( envelopeInfoPtr->processPreambleFunction );

		REQUIRES( checkMissingInfoFunction != NULL );
		REQUIRES( processPreambleFunction != NULL );

		/* Make sure that all of the information that we need to proceed is 
		   present */
		status = checkMissingInfoFunction( envelopeInfoPtr,
									( buffer == NULL ) ? TRUE : FALSE );
		if( cryptStatusError( status ) )
			return( status );

		/* If the envelope buffer hasn't been allocated yet, allocate it now */
		if( envelopeInfoPtr->buffer == NULL )
			{
			envelopeInfoPtr->buffer = \
								safeBufferAlloc( envelopeInfoPtr->bufSize );
			if( envelopeInfoPtr->buffer == NULL )
				return( CRYPT_ERROR_MEMORY );
			memset( envelopeInfoPtr->buffer, 0, envelopeInfoPtr->bufSize );
			}

		/* Emit the header information into the envelope */
		status = processPreambleFunction( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			{
			if( !isRecoverableError( status ) )
				envelopeInfoPtr->errorState = status;
			return( status );
			}

		/* The envelope is ready to process data, move it into the high
		   state */
		status = krnlSendMessage( envelopeInfoPtr->objectHandle, 
								  IMESSAGE_SETATTRIBUTE, MESSAGE_VALUE_UNUSED, 
								  CRYPT_IATTRIBUTE_INITIALISED );
		if( cryptStatusError( status ) )
			{
			envelopeInfoPtr->errorState = status;
			return( status );
			}

		/* Move on to the data-processing state */
		envelopeInfoPtr->state = ENVELOPE_STATE_DATA;
		}

	/* If we're in the main data processing state, add the data and perform
	   any necessary actions on it */
	if( envelopeInfoPtr->state == ENVELOPE_STATE_DATA )
		{
		const ENV_COPYTOENVELOPE_FUNCTION copyToEnvelopeFunction = \
					( ENV_COPYTOENVELOPE_FUNCTION ) \
					FNPTR_GET( envelopeInfoPtr->copyToEnvelopeFunction );

		REQUIRES( copyToEnvelopeFunction != NULL );

		if( length > 0 )
			{
			/* Copy the data to the envelope */
			status = copyToEnvelopeFunction( envelopeInfoPtr, buffer, 
											 length );
			if( cryptStatusError( status ) )
				{
				if( !isRecoverableError( status ) )
					envelopeInfoPtr->errorState = status;
				return( status );
				}
			*bytesCopied = status;

			return( ( *bytesCopied < length ) ? \
					CRYPT_ERROR_OVERFLOW : CRYPT_OK );
			}

		/* This was a flush, move on to the postdata state */
		envelopeInfoPtr->state = ENVELOPE_STATE_POSTDATA;
		envelopeInfoPtr->envState = ENVSTATE_NONE;
		}

	ENSURES( envelopeInfoPtr->state == ENVELOPE_STATE_POSTDATA );

	/* We're past the main data-processing state, emit the postamble */
	status = processPostambleFunction( envelopeInfoPtr, FALSE );
	if( cryptStatusError( status ) )
		{
		if( !isRecoverableError( status ) )
			envelopeInfoPtr->errorState = status;
		return( status );
		}
	envelopeInfoPtr->state = ENVELOPE_STATE_FINISHED;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int deenvelopePush( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
						   IN_BUFFER_OPT( length ) const void *buffer,
						   IN_DATALENGTH_Z const int length, 
						   OUT_DATALENGTH_Z int *bytesCopied )
	{
	const ENV_COPYTOENVELOPE_FUNCTION copyToEnvelopeFunction = \
				( ENV_COPYTOENVELOPE_FUNCTION ) \
				FNPTR_GET( envelopeInfoPtr->copyToEnvelopeFunction );
	BYTE *bufPtr = ( BYTE * ) buffer;
	int bytesIn = length, status = CRYPT_OK;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( ( buffer == NULL && length == 0 ) || \
			isReadPtrDynamic( buffer, length ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( ( buffer == NULL && length == 0 ) || \
			  ( buffer != NULL && length > 0 && length < MAX_BUFFER_SIZE ) );
	REQUIRES( copyToEnvelopeFunction != NULL );

	/* Clear return value */
	*bytesCopied = 0;

	/* If we haven't started processing data yet, handle the initial data
	   specially */
	if( envelopeInfoPtr->state == ENVELOPE_STATE_PREDATA )
		{
		ENV_PROCESSPREAMBLE_FUNCTION processPreambleFunction;
		BOOLEAN copiedData = FALSE;

		/* Perform any initialisation actions */
		if( envelopeInfoPtr->buffer == NULL )
			{
			/* Allocate the envelope buffer */
			envelopeInfoPtr->buffer = \
								safeBufferAlloc( envelopeInfoPtr->bufSize );
			if( envelopeInfoPtr->buffer == NULL )
				return( CRYPT_ERROR_MEMORY );
			memset( envelopeInfoPtr->buffer, 0, envelopeInfoPtr->bufSize );

			/* Try and determine what the data format being used is.  If it 
			   looks like PGP data, try and process it as such, otherwise 
			   default to PKCS #7/CMS/S/MIME */
#ifdef USE_PGP
			if( length > 0 && ( bufPtr[ 0 ] & 0x80 ) )
				{
				/* When we initially created the envelope we defaulted to CMS
				   formatting so we have to select PGP de-enveloping before 
				   we can continue */
				envelopeInfoPtr->type = CRYPT_FORMAT_PGP;
				initPGPDeenveloping( envelopeInfoPtr );
				}
			else
#endif /* USE_PGP */
				{
				/* The distinction between CRYPT_FORMAT_CRYPTLIB, 
				   CRYPT_FORMAT_PKCS7/CRYPT_FORMAT_CMS, and 
				   CRYPT_FORMAT_SMIME is only necessary when enveloping
				   because it affects the enveloping semantics (for
				   example whether we use issuerAndSerialNumber or
				   keyIdentifier to identify keys), so we identify all
				   PKCS #7/CMS/whatever envelopes as CRYPT_FORMAT_CMS */
				envelopeInfoPtr->type = CRYPT_FORMAT_CMS;
				}
			}
		processPreambleFunction = ( ENV_PROCESSPREAMBLE_FUNCTION ) \
					FNPTR_GET( envelopeInfoPtr->processPreambleFunction );
		ENSURES( processPreambleFunction != NULL );

		/* Since we're processing out-of-band information, just copy it in
		   directly */
		if( bytesIn > 0 )
			{
			const int bytesToCopy = min( envelopeInfoPtr->bufSize - \
										 envelopeInfoPtr->bufPos, bytesIn );

			ENSURES( bytesToCopy >= 0 && bytesToCopy <= length && \
					 envelopeInfoPtr->bufPos + \
						bytesToCopy <= envelopeInfoPtr->bufSize );
			if( bytesToCopy > 0 )
				{
				REQUIRES( boundsCheckZ( envelopeInfoPtr->bufPos, bytesToCopy,
										envelopeInfoPtr->bufSize ) );
				memcpy( envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos,
						bufPtr, bytesToCopy );
				envelopeInfoPtr->bufPos += bytesToCopy;
				bytesIn -= bytesToCopy;
				*bytesCopied = bytesToCopy;
				bufPtr += bytesToCopy;
				copiedData = TRUE;
				}
			}

		/* Process the preamble */
		status = processPreambleFunction( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			{
			/* If we've processed at least some data and it's a non-fatal
			   error like an underflow, suppress it.  We need to do this
			   because the copy function above will always consume input (if
			   it can) and if we returned an error at this point it would
			   indicate to the caller that they can ignore the bytes-copied
			   parameter */
			if( isRecoverableError( status ) )
				{
				if( copiedData )
					return( CRYPT_OK );
				}
			else
				{
				/* It's a fatal error, make it persistent */
				envelopeInfoPtr->errorState = status;
				}
			return( status );
			}

		/* The envelope is ready to process data, move it into the high
		   state */
		status = krnlSendMessage( envelopeInfoPtr->objectHandle, 
								  IMESSAGE_SETATTRIBUTE, MESSAGE_VALUE_UNUSED, 
								  CRYPT_IATTRIBUTE_INITIALISED );
		if( cryptStatusError( status ) )
			{
			/* The envelope may already have been moved into the high state 
			   as a side-effect of adding de-enveloping information (whether 
			   the transition occurs here or in res_denv.c depends on the 
			   order in which the caller performs operations) so if we get a 
			   permission error it's means that we're already in the high 
			   state and we can ignore it */
			if( status != CRYPT_ERROR_PERMISSION )
				{
				envelopeInfoPtr->errorState = status;
				return( status );
				}
			status = CRYPT_OK;
			}

		/* Move on to the data-processing state */
		envelopeInfoPtr->state = ENVELOPE_STATE_DATA;
		}

	/* If we're in the main data processing state, add the data and perform
	   any necessary actions on it */
	if( envelopeInfoPtr->state == ENVELOPE_STATE_DATA )
		{
		/* If there's data to be copied, copy it into the envelope.  If we've
		   come from the predata state we may have zero bytes to copy if
		   everything was consumed by the preamble processing, or there may
		   be room to copy more in now if the preamble processing consumed 
		   some of what was present */
		if( bytesIn > 0 )
			{
			/* Copy the data to the envelope */
			const int byteCount = \
				copyToEnvelopeFunction( envelopeInfoPtr, bufPtr, bytesIn );
			if( cryptStatusError( byteCount ) )
				{
				if( !isRecoverableError( byteCount ) )
					envelopeInfoPtr->errorState = byteCount;
				return( byteCount );
				}
			*bytesCopied += byteCount;
			bytesIn -= byteCount;
			bufPtr += byteCount;
			}

		/* If we've reached the end of the payload (either by having seen the
		   EOC octets with the indefinite encoding or by having reached the 
		   end of the single segment with the definite encoding), move on to 
		   the postdata state */
		if( TEST_FLAG( envelopeInfoPtr->dataFlags, 
					   ENVDATA_FLAG_ENDOFCONTENTS ) || \
			( envelopeInfoPtr->payloadSize != CRYPT_UNUSED && \
			  envelopeInfoPtr->segmentSize <= 0 ) )
			{
			envelopeInfoPtr->state = ENVELOPE_STATE_POSTDATA;
			envelopeInfoPtr->deenvState = DEENVSTATE_NONE;
			}

#ifdef USE_PGP
		/* Special-case for the wierd non-length-encoding that some PGP 
		   implementations use for compressed data which uses neither a
		   definite nor indefinite-length encoding but consists of an 
		   implicit "until you run out of input", see the comment in the PGP
		   readPacketHeader() for more details.  To accomodate this lack of
		   the data's ability to tell us when it's ended, if we're processing
		   PGP compressed data and there's no length information present and
		   it's a data flush we assume that that's all there is */
		if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP && \
			envelopeInfoPtr->usage == ACTION_COMPRESS && \
			envelopeInfoPtr->payloadSize == CRYPT_UNUSED && \
			length <= 0 )
			{
			envelopeInfoPtr->state = ENVELOPE_STATE_POSTDATA;
			envelopeInfoPtr->deenvState = DEENVSTATE_NONE;
			}
#endif /* USE_PGP */
		}

	/* If we're past the main data-processing state, process the postamble */
	if( envelopeInfoPtr->state == ENVELOPE_STATE_POSTDATA )
		{
		const ENV_PROCESSPOSTAMBLE_FUNCTION processPostambleFunction = \
					( ENV_PROCESSPOSTAMBLE_FUNCTION ) \
					FNPTR_GET( envelopeInfoPtr->processPostambleFunction );

		REQUIRES( processPostambleFunction != NULL );

		/* Since we're processing trailer information, just copy it in
		   directly */
		if( bytesIn > 0 )
			{
			const int bytesToCopy = min( envelopeInfoPtr->bufSize - \
										 envelopeInfoPtr->bufPos, bytesIn );

			ENSURES( bytesToCopy >= 0 && bytesToCopy <= bytesIn && \
					 envelopeInfoPtr->bufPos + \
						bytesToCopy <= envelopeInfoPtr->bufSize );
			if( bytesToCopy > 0 )
				{
				REQUIRES( boundsCheckZ( envelopeInfoPtr->bufPos, bytesToCopy,
										envelopeInfoPtr->bufSize ) );
				memcpy( envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos,
						bufPtr, bytesToCopy );
				envelopeInfoPtr->bufPos += bytesToCopy;
				*bytesCopied += bytesToCopy;
				}
			}

		/* Process the postamble.  Note that we need to check the caller-
		   supplied length value rather than the local bytesIn value to 
		   determine whether this is a flush, since bytesIn is adjusted as 
		   data is consumed and may have reached zero by the time we get 
		   here.
		
		   During this processing we can encounter two special types of 
		   recoverable error, CRYPT_ERROR_UNDERFLOW (we need more data to 
		   continue) or OK_SPECIAL (we processed all of the data but there's 
		   out-of-band information still to go).  If it's one of these then 
		   we don't treat it as a standard error */
		status = processPostambleFunction( envelopeInfoPtr,
										   ( length <= 0 ) ? TRUE : FALSE );
		if( cryptStatusError( status ) && status != OK_SPECIAL )
			{
			if( !isRecoverableError( status ) )
				{
				envelopeInfoPtr->errorState = status;

				/* MACd and authenticated-encrypted envelopes differ 
				   somewhat from signed envelopes in that the integrity 
				   check results are available immediately that payload 
				   processing is complete rather than afterwards as the 
				   result of user action with signature metadata.  As a 
				   result the postamble processing can return a 
				   CRYPT_ERROR_SIGNATURE to indicate that although all data 
				   was processed successfully (which would normally produce 
				   a CRYPT_OK result) the integrity check for the data 
				   failed.  To reconcile the two status values we treat the 
				   envelope as if a CRYPT_OK had been returned (by marking 
				   processing as being complete) while recording a 
				   CRYPT_ERROR_SIGNATURE.  However we do return the 
				   signature error since the user may be using authenticated 
				   encryption and not even be aware that they should perform 
				   an explicit check for a signature failure */
				if( status == CRYPT_ERROR_SIGNATURE && \
					( envelopeInfoPtr->usage == ACTION_MAC || \
					  ( envelopeInfoPtr->usage == ACTION_CRYPT && \
						TEST_FLAG( envelopeInfoPtr->flags, 
								   ENVELOPE_FLAG_AUTHENC ) ) ) )
					{
					envelopeInfoPtr->state = ENVELOPE_STATE_FINISHED;
					}
				}
			return( status );
			}
		ENSURES( status == CRYPT_OK || status == OK_SPECIAL );

		/* If the postamble processing routine returns OK_SPECIAL then it's 
		   processed enough of the postamble for the caller to continue, but 
		   there's more to go so we shouldn't change the overall state yet */
		if( status == CRYPT_OK )
			{
			/* We've processed all data, we're done unless it's a detached
			   sig with the data supplied out-of-band */
			envelopeInfoPtr->state = \
						TEST_FLAG( envelopeInfoPtr->flags, 
								   ENVELOPE_FLAG_DETACHED_SIG ) ? \
						ENVELOPE_STATE_EXTRADATA : ENVELOPE_STATE_FINISHED;
			}

		/* At this point we always exit since the out-of-band data has to be
		   processed in a separate push */
		return( CRYPT_OK );
		}

	/* If there's extra out-of-band data present, process it separately.  
	   This is slightly complicated by the fact that the single envelope is
	   being used to process two independent lots of data, so we have to be 
	   careful to distinguish between handling of the main payload data and 
	   handling of this additional out-of-band data */
	if( envelopeInfoPtr->state == ENVELOPE_STATE_EXTRADATA )
		{
		const ENV_PROCESSEXTRADATA_FUNCTION processExtraDataFunction = \
					( ENV_PROCESSEXTRADATA_FUNCTION ) \
					FNPTR_GET( envelopeInfoPtr->processExtraDataFunction );

		REQUIRES( processExtraDataFunction != NULL );

		/* We pass this point twice, the first time round we check the state 
		   and if it's DEENVSTATE_DONE (set when processing of the main data 
		   was completed) we reset it to DEENVSTATE_NONE and make sure that 
		   it's a flush */
		if( envelopeInfoPtr->deenvState == DEENVSTATE_DONE )
			{
			/* We've finished with the main payload data, reset the state for 
			   the additional out-of-band data.  Normally we exit here since 
			   it's a flush, however if the hash value was supplied 
			   externally (which means that hashing was never active, since 
			   it was done by the caller) we drop through to the wrap-up, 
			   since there's no second flush of payload data to be performed 
			   and so the flush applies to both sets of data */
			envelopeInfoPtr->deenvState = DEENVSTATE_NONE;
			if( TEST_FLAG( envelopeInfoPtr->dataFlags, 
						   ENVDATA_FLAG_HASHACTIONSACTIVE ) )
				return( length ? CRYPT_ERROR_BADDATA : CRYPT_OK );
			}

		/* This is just raw additional data so we feed it directly to the 
		   processing function.  If this is a flush then the buffer will be
		   set to NULL which the low-level routines don't allow so we 
		   substitute an empty buffer */
		status = processExtraDataFunction( envelopeInfoPtr, 
							( buffer == NULL ) ? "" : buffer, length );
		if( cryptStatusOK( status ) )
			{
			*bytesCopied = length;
			if( length <= 0 )
				envelopeInfoPtr->state = ENVELOPE_STATE_FINISHED;
			}
		}

	return( status );
	}

/* Pop data from an envelope */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int envelopePop( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
						OUT_BUFFER( length, *bytesCopied ) void *buffer,
						IN_DATALENGTH const int length, 
						OUT_DATALENGTH_Z int *bytesCopied )
	{
	const ENV_COPYFROMENVELOPE_FUNCTION copyFromEnvelopeFunction = \
				( ENV_COPYFROMENVELOPE_FUNCTION ) \
				FNPTR_GET( envelopeInfoPtr->copyFromEnvelopeFunction );
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtrDynamic( buffer, length ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE );
	REQUIRES( copyFromEnvelopeFunction != NULL );

	/* Clear return value */
	*bytesCopied = 0;

	/* If we haven't reached the data yet force a flush to try and get to 
	   it.  We can end up with this condition if the caller doesn't push in
	   any enveloping information, or pushes in enveloping information and 
	   then immediately tries to pop data without an intervening flush (or 
	   implicit flush on the initial push) to resolve the state of the data 
	   in the envelope */
	if( envelopeInfoPtr->state == ENVELOPE_STATE_PREDATA )
		{
		int dummy;

		/* If no data has been pushed yet then we can't go any further.  
		   This typically happens if the user calls cryptPopData() without
		   first calling cryptPushData() */
		if( envelopeInfoPtr->bufPos <= 0 )
			{
			/* There is one situation in which we can pop data without first
			   pushing anything and that's when we're using a detached
			   signature and the hash value has been supplied externally,
			   in which case there's nothing to push */
			if( !TEST_FLAG( envelopeInfoPtr->flags, 
							ENVELOPE_FLAG_DETACHED_SIG ) )
				return( CRYPT_ERROR_UNDERFLOW );
			}

		status = envelopePush( envelopeInfoPtr, NULL, 0, &dummy );
		if( cryptStatusError( status ) )
			return( status );

		/* If we still haven't got anywhere return an underflow error */
		if( envelopeInfoPtr->state == ENVELOPE_STATE_PREDATA )
			return( CRYPT_ERROR_UNDERFLOW );
		}

	/* Copy the data from the envelope to the output */
	status = copyFromEnvelopeFunction( envelopeInfoPtr, buffer, length, 
									   bytesCopied, ENVCOPY_FLAG_NONE );
	if( cryptStatusError( status ) )
		envelopeInfoPtr->errorState = status;
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int deenvelopePop( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
						  OUT_BUFFER( length, *bytesCopied ) void *buffer,
						  IN_DATALENGTH const int length, 
						  OUT_DATALENGTH_Z int *bytesCopied )
	{
	const ENV_COPYFROMENVELOPE_FUNCTION copyFromEnvelopeFunction = \
				( ENV_COPYFROMENVELOPE_FUNCTION ) \
				FNPTR_GET( envelopeInfoPtr->copyFromEnvelopeFunction );
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtrDynamic( buffer, length ) );
	assert( isWritePtr( bytesCopied, sizeof( int ) ) );

	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE );
	REQUIRES( copyFromEnvelopeFunction != NULL );

	/* Clear return value */
	*bytesCopied = 0;

	/* If we haven't reached the data yet force a flush to try and get to 
	   it.  We can end up with this condition if the caller doesn't push in
	   any de-enveloping information, or pushes in de-enveloping information 
	   and then immediately tries to pop data without an intervening flush 
	   (or implicit flush on the initial push) to resolve the state of the 
	   data in the envelope */
	if( envelopeInfoPtr->state == ENVELOPE_STATE_PREDATA )
		{
		int dummy;

		/* If no data has been pushed yet then we can't go any further.  
		   This typically happens if the user calls cryptPopData() without
		   first calling cryptPushData() */
		if( envelopeInfoPtr->bufPos <= 0 )
			return( CRYPT_ERROR_UNDERFLOW );

		status = deenvelopePush( envelopeInfoPtr, NULL, 0, &dummy );
		if( cryptStatusError( status ) )
			return( status );

		/* If we still haven't got anywhere return an underflow error */
		if( envelopeInfoPtr->state == ENVELOPE_STATE_PREDATA )
			return( CRYPT_ERROR_UNDERFLOW );
		}

	/* Copy the data from the envelope to the output */
	status = copyFromEnvelopeFunction( envelopeInfoPtr, buffer, length, 
									   bytesCopied, ENVCOPY_FLAG_NONE );
	if( cryptStatusError( status ) && !isRecoverableError( status ) )
		envelopeInfoPtr->errorState = status;
	return( status );
	}

/****************************************************************************
*																			*
*								Envelope Message Handler					*
*																			*
****************************************************************************/

/* Handle a message sent to an envelope */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int envelopeMessageFunction( INOUT TYPECAST( ENVELOPE_INFO * ) \
										void *objectInfoPtr,
									IN_MESSAGE const MESSAGE_TYPE message,
									void *messageDataPtr,
									IN_INT_Z const int messageValue )
	{
	ENVELOPE_INFO *envelopeInfoPtr = ( ENVELOPE_INFO * ) objectInfoPtr;

	assert( isWritePtr( objectInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( message == MESSAGE_DESTROY || \
			  sanityCheckEnvelope( envelopeInfoPtr ) );
	REQUIRES( isEnumRange( message, MESSAGE ) );
	REQUIRES( isIntegerRange( messageValue ) );

	/* Process destroy object messages */
	if( message == MESSAGE_DESTROY )
		{
		int status = CRYPT_OK;

		/* Check to see whether the envelope still needs operations 
		   performed on it to resolve the state of the data within it (for 
		   example if the caller pushes data but doesn't flush it, there 
		   will be a few bytes left that can't be popped).  For enveloping, 
		   destroying the envelope while it's in any state other than 
		   ENVELOPE_STATE_PREDATA or ENVELOPE_STATE_FINISHED or if there's 
		   data still left in the buffer is regarded as an error.  For de-
		   enveloping we have to be more careful since deenveloping 
		   information required to resolve the envelope state could be 
		   unavailable so we shouldn't return an error if something like a 
		   signature check remains to be done.  What we therefore do is 
		   check to see whether we've processed any data yet and report an 
		   error if there's data left in the envelope or if we're destroying 
		   it in the middle of processing data */
		if( TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_ISDEENVELOPE ) )
			{
			const BOOLEAN isAuthEnvelope = \
					( envelopeInfoPtr->usage == ACTION_MAC || \
					  ( envelopeInfoPtr->usage == ACTION_CRYPT && \
						TEST_FLAG( envelopeInfoPtr->flags, 
								   ENVELOPE_FLAG_AUTHENC ) ) ) ? \
					TRUE : FALSE;

			/* If we've got to the point of processing data in the envelope
			   and there's either more to come or some left to pop, we
			   shouldn't be destroying it yet.  This straightforward check 
			   is complicated by the integrity check performed with 
			   authenticated envelopes, leading to two special cases:
			   
			   1. If the integrity checks fails then the integrity-check-
				  failed status prevents the user from popping the 
				  (corrupted) data so that there may be data left in the 
				  envelope through no fault of the user.  In this case we 
				  don't treat the data-left status as an error.
				  
				2. If an attacker truncates the data in an attempt to 
				   convert a fatal CRYPT_ERROR_SIGNATURE into a more benign 
				   CRYPT_ERROR_UNDERFLOW then we want to try and alert the 
				   caller to the fact that there's a problem, so we convert 
				   a non-finished state for an authenticated envelope into 
				   an integrity-check failure */
			if( envelopeInfoPtr->state == ENVELOPE_STATE_DATA )
				status = CRYPT_ERROR_INCOMPLETE;
			if( ( envelopeInfoPtr->state == ENVELOPE_STATE_POSTDATA || \
				  envelopeInfoPtr->state == ENVELOPE_STATE_FINISHED ) && \
				envelopeInfoPtr->dataLeft > 0 && \
				!( isAuthEnvelope && \
				   envelopeInfoPtr->errorState == CRYPT_ERROR_SIGNATURE ) )
				status = CRYPT_ERROR_INCOMPLETE;
			if( status == CRYPT_ERROR_INCOMPLETE && isAuthEnvelope )
				status = CRYPT_ERROR_SIGNATURE;
			}
		else
			{
			/* If we're in the middle of processing data we shouldn't be
			   destroying the envelope yet */
			if( envelopeInfoPtr->state != ENVELOPE_STATE_PREDATA && \
				envelopeInfoPtr->state != ENVELOPE_STATE_FINISHED )
				status = CRYPT_ERROR_INCOMPLETE;
			if( envelopeInfoPtr->bufPos > 0 )
				status = CRYPT_ERROR_INCOMPLETE;
			}

		/* Delete the action and content lists */
		deleteActionLists( envelopeInfoPtr );
		if( DATAPTR_ISSET( envelopeInfoPtr->contentList ) )
			deleteContentList( envelopeInfoPtr );

#ifdef USE_COMPRESSION
		/* Delete the zlib compression state information if necessary */
		if( TEST_FLAG( envelopeInfoPtr->flags, 
					   ENVELOPE_FLAG_ZSTREAMINITED ) )
			{
			if( TEST_FLAG( envelopeInfoPtr->flags, 
						   ENVELOPE_FLAG_ISDEENVELOPE ) )
				inflateEnd( &envelopeInfoPtr->zStream );
			else
				deflateEnd( &envelopeInfoPtr->zStream );
			}
#endif /* USE_COMPRESSION */

		/* Clean up keysets */
		if( envelopeInfoPtr->iSigCheckKeyset != CRYPT_ERROR )
			{
			krnlSendNotifier( envelopeInfoPtr->iSigCheckKeyset,
							  IMESSAGE_DECREFCOUNT );
			}
		if( envelopeInfoPtr->iEncryptionKeyset != CRYPT_ERROR )
			{
			krnlSendNotifier( envelopeInfoPtr->iEncryptionKeyset,
							  IMESSAGE_DECREFCOUNT );
			}
		if( envelopeInfoPtr->iDecryptionKeyset != CRYPT_ERROR )
			{
			krnlSendNotifier( envelopeInfoPtr->iDecryptionKeyset,
							  IMESSAGE_DECREFCOUNT );
			}

		/* Clean up other envelope objects */
		if( envelopeInfoPtr->iExtraCertChain != CRYPT_ERROR )
			{
			krnlSendNotifier( envelopeInfoPtr->iExtraCertChain,
							  IMESSAGE_DECREFCOUNT );
			}

		/* Clear and free the buffers if necessary */
		if( envelopeInfoPtr->buffer != NULL )
			{
			zeroise( envelopeInfoPtr->buffer, envelopeInfoPtr->bufSize );
			safeBufferFree( envelopeInfoPtr->buffer );
			}
		if( envelopeInfoPtr->auxBuffer != NULL )
			{
			zeroise( envelopeInfoPtr->auxBuffer, envelopeInfoPtr->auxBufSize );
			clFree( "envelopeMessageFunction", envelopeInfoPtr->auxBuffer );
			}

		return( status );
		}

	/* Process attribute get/set/delete messages */
	if( isAttributeMessage( message ) )
		{
		REQUIRES( message == MESSAGE_GETATTRIBUTE || \
				  message == MESSAGE_GETATTRIBUTE_S || \
				  message == MESSAGE_SETATTRIBUTE || \
				  message == MESSAGE_SETATTRIBUTE_S || \
				  message == MESSAGE_DELETEATTRIBUTE );
		REQUIRES( isAttribute( messageValue ) || \
				  isInternalAttribute( messageValue ) );

		if( message == MESSAGE_GETATTRIBUTE )
			{
			return( getEnvelopeAttribute( envelopeInfoPtr, 
										  ( int * ) messageDataPtr,
										  messageValue ) );
			}
		if( message == MESSAGE_GETATTRIBUTE_S )
			{
			return( getEnvelopeAttributeS( envelopeInfoPtr, 
										   ( MESSAGE_DATA * ) messageDataPtr,
										   messageValue ) );
			}
		if( message == MESSAGE_SETATTRIBUTE )
			{
			/* CRYPT_IATTRIBUTE_INITIALISED is purely a notification message 
			   with no parameters so we don't pass it down to the attribute-
			   handling code */
			if( messageValue == CRYPT_IATTRIBUTE_INITIALISED )
				return( CRYPT_OK );

			return( setEnvelopeAttribute( envelopeInfoPtr, 
										  *( ( int * ) messageDataPtr ),
										  messageValue ) );
			}
		if( message == MESSAGE_SETATTRIBUTE_S )
			{
			const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

			return( setEnvelopeAttributeS( envelopeInfoPtr, msgData->data, 
										   msgData->length, messageValue ) );
			}

		retIntError();
		}

	/* Process object-specific messages */
	if( message == MESSAGE_ENV_PUSHDATA )
		{
		MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;
		const int length = msgData->length;
		int bytesCopied, status;

		assert( ( msgData->data == NULL && msgData->length == 0 ) || \
				( isReadPtrDynamic( msgData->data, msgData->length ) ) );

		REQUIRES( ( msgData->data == NULL && msgData->length == 0 ) || \
				  ( msgData->data != NULL && \
				    msgData->length > 0 && msgData->length < MAX_BUFFER_SIZE ) );

		/* Unless we're told otherwise, we've copied zero bytes */
		msgData->length = 0;

		/* Make sure that everything is in order */
		if( length == 0 )
			{
			/* If it's a flush make sure that we're in a state where this is
			   valid.  We can only perform a flush on enveloping if we're in
			   the data or postdata state, on deenveloping a flush can
			   happen at any time since the entire payload could be buffered
			   pending the addition of a deenveloping resource, so the
			   envelope goes from pre -> post in one step.  There is however
			   one special case in which a push in the pre-data state is 
			   valid and that's when we're creating a zero-length CMS signed 
			   message as a means of communicating authenticated attributes 
			   (of all the standard users of CMS, only SCEP normally does 
			   this).  In order to indicate that this special case is in
			   effect we require that the user set the ENVELOPE_ATTRONLY 
			   flag before pushing data, although for completeness we could 
			   also check the CMS attributes for the presence of SCEP 
			   attributes.  The downside of this additional checking is that 
			   it makes any non-SCEP use of signature-only CMS envelopes 
			   impossible */
			if( envelopeInfoPtr->state == ENVELOPE_STATE_FINISHED )
				return( CRYPT_OK );
			if( !TEST_FLAG( envelopeInfoPtr->flags, 
							ENVELOPE_FLAG_ISDEENVELOPE ) && \
				( envelopeInfoPtr->state != ENVELOPE_STATE_DATA && \
				  envelopeInfoPtr->state != ENVELOPE_STATE_POSTDATA ) && \
				!( envelopeInfoPtr->state == ENVELOPE_STATE_PREDATA && \
				   envelopeInfoPtr->usage == ACTION_SIGN && \
				   envelopeInfoPtr->type == CRYPT_FORMAT_CMS && \
				   TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_ATTRONLY ) ) )
				return( CRYPT_ERROR_INCOMPLETE );
			}
		else
			{
			if( envelopeInfoPtr->state == ENVELOPE_STATE_FINISHED )
				return( CRYPT_ERROR_COMPLETE );
			}
		if( cryptStatusError( envelopeInfoPtr->errorState ) )
			return( envelopeInfoPtr->errorState );
		if( !TEST_FLAG( envelopeInfoPtr->flags, 
						ENVELOPE_FLAG_ISDEENVELOPE ) && \
			TEST_FLAG( envelopeInfoPtr->dataFlags, 
					   ENVDATA_FLAG_NOSEGMENT ) && \
			envelopeInfoPtr->payloadSize == CRYPT_UNUSED )
			{
			/* If we're enveloping using a non-segmenting encoding of the 
			   payload then the caller has to explicitly set the payload 
			   size before they can add any data */
			setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_DATASIZE, 
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}

		/* Send the data to the envelope */
		if( TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_ISDEENVELOPE ) )
			{
			status = deenvelopePush( envelopeInfoPtr, msgData->data,
									 length, &bytesCopied );
			}
		else
			{
			status = envelopePush( envelopeInfoPtr, msgData->data,
								   length, &bytesCopied );
			}
		if( cryptStatusOK( status ) )
			msgData->length = bytesCopied;
		else
			{
			/* In some cases data can be copied even if an error status is
			   returned.  The most usual case is when the error is
			   recoverable (underflow or overflow), however when we're de-
			   enveloping we can also copy data but then stall with a 
			   CRYPT_ENVELOPE_RESOURCE notification */
			if( ( isRecoverableError( status ) && bytesCopied > 0 ) || \
				( TEST_FLAG( envelopeInfoPtr->flags, 
							 ENVELOPE_FLAG_ISDEENVELOPE ) && \
				   status == CRYPT_ENVELOPE_RESOURCE && bytesCopied > 0 ) )
				msgData->length = bytesCopied;
			}
		return( status );
		}
	if( message == MESSAGE_ENV_POPDATA )
		{
		MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;
		const int length = msgData->length;
		int bytesCopied, status;

		assert( isWritePtrDynamic( msgData->data, msgData->length ) );

		REQUIRES( msgData->length > 0 && msgData->length < MAX_BUFFER_SIZE );

		/* Unless we're told otherwise, we've copied zero bytes */
		msgData->length = 0;

		/* Make sure that everything is in order */
		if( cryptStatusError( envelopeInfoPtr->errorState ) )
			return( envelopeInfoPtr->errorState );

		/* Get the data from the envelope */
		if( TEST_FLAG( envelopeInfoPtr->flags, 
					   ENVELOPE_FLAG_ISDEENVELOPE ) )
			{
			status = deenvelopePop( envelopeInfoPtr, msgData->data,
									length, &bytesCopied );
			}
		else
			{
			status = envelopePop( envelopeInfoPtr, msgData->data,
								  length, &bytesCopied );
			}
		if( cryptStatusOK( status ) )
			msgData->length = bytesCopied;
		return( status );
		}

	retIntError();
	}

/* Create an envelope.  This is a low-level function encapsulated by
   createEnvelope() and used to manage error exits */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int initEnvelope( OUT_HANDLE_OPT CRYPT_ENVELOPE *iCryptEnvelope,
						 IN_HANDLE const CRYPT_USER iCryptOwner,
						 IN_ENUM( CRYPT_FORMAT ) \
							const CRYPT_FORMAT_TYPE formatType,
						 OUT_PTR_OPT ENVELOPE_INFO **envelopeInfoPtrPtr )
	{
	ENVELOPE_INFO *envelopeInfoPtr;
	const BOOLEAN isDeenvelope = ( formatType == CRYPT_FORMAT_AUTO ) ? \
								 TRUE : FALSE;
	const int subType = \
			isDeenvelope ? SUBTYPE_ENV_DEENV : \
			( formatType == CRYPT_FORMAT_PGP ) ? \
				SUBTYPE_ENV_ENV_PGP : SUBTYPE_ENV_ENV;
	const int storageSize = 3 * sizeof( CONTENT_LIST );
	int status;

	assert( isWritePtr( iCryptEnvelope, sizeof( CRYPT_ENVELOPE * ) ) );
	assert( isWritePtr( envelopeInfoPtrPtr, sizeof( ENVELOPE_INFO * ) ) );

	REQUIRES( ( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE ) || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( formatType > CRYPT_FORMAT_NONE && \
			  formatType < CRYPT_FORMAT_LAST_EXTERNAL );

	/* Clear return values */
	*iCryptEnvelope = CRYPT_ERROR;
	*envelopeInfoPtrPtr = NULL;

	/* If PGP support is disabled we can't specify PGP as a target format */
#ifndef USE_PGP
	if( formatType == CRYPT_FORMAT_PGP )
		return( CRYPT_ARGERROR_NUM1 );
#endif /* USE_PGP */

	/* Create the envelope object */
	status = krnlCreateObject( iCryptEnvelope, ( void ** ) &envelopeInfoPtr, 
							   sizeof( ENVELOPE_INFO ) + storageSize, 
							   OBJECT_TYPE_ENVELOPE, subType, 
							   CREATEOBJECT_FLAG_NONE, iCryptOwner, 
							   ACTION_PERM_NONE_ALL, envelopeMessageFunction );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( envelopeInfoPtr != NULL );
	*envelopeInfoPtrPtr = envelopeInfoPtr;
	envelopeInfoPtr->objectHandle = *iCryptEnvelope;
	envelopeInfoPtr->ownerHandle = iCryptOwner;
	envelopeInfoPtr->bufSize = DEFAULT_BUFFER_SIZE;
	if( isDeenvelope )
		INIT_FLAGS( envelopeInfoPtr->flags, ENVELOPE_FLAG_ISDEENVELOPE );
	else
		INIT_FLAGS( envelopeInfoPtr->flags, ENVELOPE_FLAG_NONE );
	envelopeInfoPtr->type = formatType;
	envelopeInfoPtr->state = ENVELOPE_STATE_PREDATA;
	INIT_FLAGS( envelopeInfoPtr->dataFlags, ENVDATA_FLAG_NONE );
	DATAPTR_SET( envelopeInfoPtr->preActionList, NULL );
	DATAPTR_SET( envelopeInfoPtr->actionList, NULL );
	DATAPTR_SET( envelopeInfoPtr->postActionList, NULL );
	DATAPTR_SET( envelopeInfoPtr->lastAction, NULL );
	DATAPTR_SET( envelopeInfoPtr->contentList, NULL );
	DATAPTR_SET( envelopeInfoPtr->contentListCurrent, NULL );
	envelopeInfoPtr->storageSize = storageSize;
	status = initMemPool( envelopeInfoPtr->memPoolState, 
						  envelopeInfoPtr->storage, storageSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Set up any internal objects to contain invalid handles */
	envelopeInfoPtr->iCryptContext = \
		envelopeInfoPtr->iExtraCertChain = CRYPT_ERROR;
	envelopeInfoPtr->iSigCheckKeyset = envelopeInfoPtr->iEncryptionKeyset = \
		envelopeInfoPtr->iDecryptionKeyset = CRYPT_ERROR;
	envelopeInfoPtr->payloadSize = CRYPT_UNUSED;

	/* Set up the enveloping methods */
	if( isDeenvelope )
		{
		/* For de-enveloping we default to PKCS #7/CMS/SMIME, if the data 
		   is in some other format we'll adjust the function pointers once 
		   the user pushes in the first data quantity */
		initCMSDeenveloping( envelopeInfoPtr );
		initDeenvelopeStreaming( envelopeInfoPtr );
		initDenvResourceHandling( envelopeInfoPtr );
		}
	else
		{
		if( formatType == CRYPT_FORMAT_PGP )
			initPGPEnveloping( envelopeInfoPtr );
		else
			initCMSEnveloping( envelopeInfoPtr );
		initEnvelopeStreaming( envelopeInfoPtr );
		initEnvResourceHandling( envelopeInfoPtr );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createEnvelope( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo, 
					STDC_UNUSED const void *auxDataPtr, 
					STDC_UNUSED const int auxValue )
	{
	CRYPT_ENVELOPE iCryptEnvelope;
	ENVELOPE_INFO *envelopeInfoPtr = NULL;
	int initStatus, status;

	assert( isWritePtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );

	REQUIRES( auxDataPtr == NULL && auxValue == 0 );
	REQUIRES( createInfo->arg1 > CRYPT_FORMAT_NONE && \
			  createInfo->arg1 < CRYPT_FORMAT_LAST_EXTERNAL );

	/* Pass the call on to the lower-level open function */
	initStatus = initEnvelope( &iCryptEnvelope, createInfo->cryptOwner,
							   createInfo->arg1, &envelopeInfoPtr );
	if( cryptStatusError( initStatus ) )
		{
		/* If the create object failed, return immediately */
		if( envelopeInfoPtr == NULL )
			return( initStatus );

		/* Since no object has been created there's nothing to get a 
		   detailed error string from, but we can at least send the 
		   information to the debug output */
		DEBUG_PRINT(( "Envelope create error: %s.\n",
					  ( envelopeInfoPtr->errorInfo.errorStringLength > 0 ) ? \
					    envelopeInfoPtr->errorInfo.errorString : \
						"<<<No information available>>>" ));

		/* The init failed, make sure that the object gets destroyed when we 
		   notify the kernel that the setup process is complete */
		krnlSendNotifier( iCryptEnvelope, IMESSAGE_DESTROY );
		}

	/* We've finished setting up the object-type-specific info, tell the
	   kernel that the object is ready for use */
	status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusError( initStatus ) || cryptStatusError( status ) )
		return( cryptStatusError( initStatus ) ? initStatus : status );
	createInfo->cryptHandle = iCryptEnvelope;

	return( CRYPT_OK );
	}
#endif /* USE_ENVELOPES */
