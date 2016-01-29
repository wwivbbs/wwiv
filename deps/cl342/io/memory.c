/****************************************************************************
*																			*
*						  Memory Stream I/O Functions						*
*						Copyright Peter Gutmann 1993-2008					*
*																			*
****************************************************************************/

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

/* Sanity-check the stream state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheck( const STREAM *stream )
	{
	assert( isReadPtr( stream, sizeof( STREAM ) ) );

	/* Null streams have no internal buffer so the buffer position 
	   indicators aren't used */
	if( stream->type == STREAM_TYPE_NULL )
		{
		/* Null streams, which act as data sinks, have a content-size 
		   indicator so although the buffer size is zero the buffer 
		   position values can be nonzero */
		if( stream->bufSize != 0 )
			return( FALSE );
		if( stream->bufPos < 0 || stream->bufPos > stream->bufEnd || 
			stream->bufEnd < 0 || stream->bufEnd >= MAX_BUFFER_SIZE )
			return( FALSE );

		return( TRUE );
		}

	/* If it's not a null stream it has to be a memory stream */
	if( stream->type != STREAM_TYPE_MEMORY )
		return( FALSE );

	/* Make sure that the buffer position is within bounds:

								 bufEnd
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
	 
	return( TRUE );
	}

/****************************************************************************
*																			*
*								Open/Close Functions						*
*																			*
****************************************************************************/

/* Initialise and shut down a memory stream.  Since the return value for the 
   memory stream open functions is rarely (if ever) checked we validate the 
   buffer and length parameters later and create a read-only null stream if 
   they're invalid, so that reads and writes return error conditions if 
   they're attempted.  For the same reason we just use a basic assert() 
   rather than the stronger REQUIRES() so that we can explicitly handle any
   parameter errors later in the code */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initMemoryStream( OUT STREAM *stream, 
							 const BOOLEAN isNullStream )
	{
	/* We don't use a REQUIRES() predicate here for the reasons given in the 
	   comments above */
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Check that the input parameters are in order */
	if( !isWritePtrConst( stream, sizeof( STREAM ) ) )
		retIntError();

	/* Clear the stream data and initialise the stream structure.  Further 
	   initialisation of stream buffer parameters will be done by the 
	   caller */
	memset( stream, 0, sizeof( STREAM ) );
	stream->type = ( isNullStream ) ? STREAM_TYPE_NULL : STREAM_TYPE_MEMORY;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkMemoryStreamParams( INOUT STREAM *stream, 
									IN const void *buffer,
										/* May be unintialised for sMemOpen()
										   so we can't use IN_BUFFER */ 
									IN_LENGTH_Z const int length )
	{
	/* We don't use a REQUIRES() predicate here for the reasons given in the 
	   comments above */
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length > 0 && length < MAX_BUFFER_SIZE );
	assert( isReadPtr( buffer, length ) );

	/* If there's a problem with the parameters, return an error code but
	   also make it a (non-readable, non-writeable) null stream with the 
	   error state set via retIntError_Stream() so that it can be safely 
	   used */
	if( length < 1 || length >= MAX_BUFFER_SIZE || \
		!isReadPtr( buffer, length ) )
		{
		stream->type = STREAM_TYPE_NULL;
		stream->flags = STREAM_FLAG_READONLY;
		retIntError_Stream( stream );
		}
	
	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int shutdownMemoryStream( INOUT STREAM *stream,
								 const BOOLEAN clearStreamBuffer )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Check that the input parameters are in order */
	if( !isWritePtrConst( stream, sizeof( STREAM ) ) )
		retIntError();

	REQUIRES( stream->type == STREAM_TYPE_NULL || \
			  stream->type == STREAM_TYPE_MEMORY );

	/* Clear the stream structure */
	if( clearStreamBuffer && stream->buffer != NULL && stream->bufEnd > 0 )
		zeroise( stream->buffer, stream->bufEnd );
	zeroise( stream, sizeof( STREAM ) );

	return( CRYPT_OK );
	}

/* Open/close a memory stream or a null stream that serves as a data sink, 
   which is useful for implementing sizeof() functions by writing data to
   null streams.  If calling sMemOpenOpt() and the buffer parameter is NULL 
   and the length is zero this creates a null stream, otherwise is creates
   a standard memory stream.  This is useful for functions that follow the
   convention of being passed a null buffer for a length check and a non-
   null buffer to produce output.
   
   We don't use REQUIRES() predicates for these functions for the reasons
   given in the comments in initMemoryStream().

   Note that the open/connect functions are declared with a void return 
   type, this is because they're used in hundreds of locations and the only 
   situation in which they can fail is a programming error.  Because of 
   this, problems are caught by throwing exceptions in debug builds rather 
   than having to add error handling for every case where they're used.  In 
   addition the functions always initialise the stream, setting it to an 
   invalid stream if there's an error, so there's no real need to check a
   return value */ 

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sMemOpen( OUT STREAM *stream, 
			   OUT_BUFFER_FIXED( length ) void *buffer, 
			   IN_LENGTH const int length )
	{
	int status;

	/* REQUIRES() checking done in initMemoryStream() */
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( buffer, length ) );
	assert( length > 0 && length < MAX_BUFFER_SIZE );

	/* Initialise the memory stream */
	status = initMemoryStream( stream, FALSE );
	ENSURES_V( cryptStatusOK( status ) );
	status = checkMemoryStreamParams( stream, buffer, length );
	ENSURES_V( cryptStatusOK( status ) );
	stream->buffer = buffer;
	stream->bufSize = length;

	/* Clear the stream buffer.  Since this can be arbitrarily large we only 
	   clear the entire buffer in the debug version */
#ifdef NDEBUG
	memset( stream->buffer, 0, min( 16, stream->bufSize ) );
#else
	memset( stream->buffer, 0, stream->bufSize );
#endif /* NDEBUG */
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void sMemNullOpen( OUT STREAM *stream )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Initialise the memory stream */
	status = initMemoryStream( stream, TRUE );
	ENSURES_V( cryptStatusOK( status ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void sMemOpenOpt( OUT STREAM *stream, 
				  OUT_BUFFER_OPT_FIXED( length ) void *buffer, 
				  IN_LENGTH_Z const int length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( ( buffer == NULL && length == 0 ) || \
			isReadPtr( buffer, length ) );

	/* Note that the following must be given as 'buffer == NULL' without an
	   additional 'length == 0' because static-analysis tools can't make the
	   connection between 'buffer' and 'length' and will warn that the value
	   passed to sMemOpen() may be NULL */
	if( buffer == NULL )
		{
		sMemNullOpen( stream );
		return;
		}
	sMemOpen( stream, buffer, length );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sMemClose( INOUT STREAM *stream )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheck( stream ) );
	REQUIRES( !( stream->flags & STREAM_FLAG_READONLY ) );
#ifdef CONFIG_FUZZ
	REQUIRES( !( stream->flags & STREAM_MFLAG_PSEUDO ) );
#endif /* CONFIG_FUZZ */

	return( shutdownMemoryStream( stream, TRUE ) );
	}

/* Connect/disconnect a memory stream without destroying the buffer
   contents */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sMemConnect( OUT STREAM *stream, 
				  IN_BUFFER( length ) const void *buffer, 
				  IN_LENGTH const int length )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length > 0 && length < MAX_BUFFER_SIZE );
	assert( isReadPtr( buffer, length ) );

	/* Initialise the memory stream.  We don't use a REQUIRES() predicate 
	   for the reasons given in the comments in initMemoryStream() */
	status = initMemoryStream( stream, FALSE );
	ENSURES_V( cryptStatusOK( status ) );
	status = checkMemoryStreamParams( stream, buffer, length );
	ENSURES_V( cryptStatusOK( status ) );
	stream->buffer = ( void * ) buffer;
	stream->bufSize = length;

	/* Initialise further portions of the stream structure.  This is a read-
	   only stream so what's in the buffer at the start is all we'll ever 
	   get */
	stream->bufEnd = length;
	stream->flags = STREAM_FLAG_READONLY;
	}

#ifndef NDEBUG

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sMemPseudoConnect( OUT STREAM *stream, 
					    IN_BUFFER( length ) const void *buffer,
					    IN_LENGTH const int length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length > 0 && length < MAX_BUFFER_SIZE );
	assert( isReadPtr( buffer, length ) );

	/* Open the stream as a standard memory stream */
	sMemConnect( stream, buffer, length );

	/* We've now got a standard memory stream, modify it to make it pseudo-
	   writeable, in the sense that written data is discarded (this also
	   removes the read-only flag from the standard memory stream) */
	stream->flags = STREAM_MFLAG_PSEUDO;
	}
#endif /* !NDEBUG */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sMemDisconnect( INOUT STREAM *stream )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheck( stream ) );

	return( shutdownMemoryStream( stream, FALSE ) );
	}

/****************************************************************************
*																			*
*							Direct Access Functions							*
*																			*
****************************************************************************/

/* Memory stream direct-access functions, used when the contents of a memory
   stream need to be encrypted/decrypted/signed/MACd.  The basic 
   sMemGetDataBlock() returns a data block of a given size from the current
   stream position, sMemGetDataBlockAbs() returns a data block from the 
   given stream position, and sMemGetDataBlockRemaining() returns a data 
   block containing all remaining data available in the stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getMemoryBlock( INOUT STREAM *stream, 
						   OUT_BUFFER_ALLOC_OPT( length ) void **dataPtrPtr,
						   IN_LENGTH_Z const int position, 
						   IN_DATALENGTH const int length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( dataPtrPtr, sizeof( void * ) ) );

	/* Check that the input parameters are in order */
	if( !isWritePtrConst( stream, sizeof( STREAM ) ) )
		retIntError();

	REQUIRES( sanityCheck( stream ) && \
			  stream->type == STREAM_TYPE_MEMORY );
	REQUIRES_S( position >= 0 && position <= stream->bufSize && \
				position < MAX_BUFFER_SIZE );
	REQUIRES_S( length > 0 && length < MAX_BUFFER_SIZE );

	/* Clear return value */
	*dataPtrPtr = NULL;

	/* If there's a problem with the stream don't try to do anything */
	if( cryptStatusError( stream->status ) )
		return( stream->status );

	/* Make sure that there's enough data available in the stream to satisfy 
	   the request.  We check against bufSize rather than bufEnd since the
	   caller may be asking for access to all remaining data space in the 
	   stream rather than just all data read/written so far */
	if( position + length < 0 || \
		position + length > stream->bufSize )
		return( sSetError( stream, CRYPT_ERROR_UNDERFLOW ) );

	/* Return a pointer to the stream-internal buffer starting at location 
	   'position' of length 'length' bytes */
	*dataPtrPtr = stream->buffer + position;

	return( CRYPT_OK );
	}

CHECK_RETVAL_RANGE_NOERROR( 0, MAX_BUFFER_SIZE ) STDC_NONNULL_ARG( ( 1 ) ) \
int sMemDataLeft( const STREAM *stream )
	{
	assert( isReadPtr( stream, sizeof( STREAM ) ) && \
			stream->type == STREAM_TYPE_MEMORY );

	/* Check that the input parameters are in order */
	if( !isReadPtrConst( stream, sizeof( STREAM ) ) )
		retIntError();

	/* We can't use REQUIRES_S() in this case because the stream is a const 
	   parameter so instead we return a data-left size of zero */
	REQUIRES_EXT( ( sanityCheck( stream ) && \
					stream->type == STREAM_TYPE_MEMORY ), 0 );

	/* If there's a problem with the stream don't try to do anything.  
	   Unlike the standard stream read/write functions this function simply 
	   returns a record of internal stream state rather than reporting the 
	   status of a stream operation, so it's not generally checked by the 
	   caller.  To indicate an error state the best that we can do is to 
	   report zero bytes available, which will result in an underflow error 
	   in the caller */
	if( cryptStatusError( stream->status ) )
		return( 0 );

	return( stream->bufSize - stream->bufPos );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sMemGetDataBlock( INOUT STREAM *stream, 
					  OUT_BUFFER_ALLOC_OPT( dataSize ) void **dataPtrPtr, 
					  IN_DATALENGTH const int dataSize )
	{
	/* REQUIRES() checking done in getMemoryBlock() */
	assert( isReadPtr( stream, sizeof( STREAM ) ) && \
			stream->type == STREAM_TYPE_MEMORY );
	assert( isWritePtr( dataPtrPtr, sizeof( void * ) ) );
	assert( dataSize > 0 && dataSize < MAX_BUFFER_SIZE );

	/* Clear return values */
	*dataPtrPtr = NULL;

	return( getMemoryBlock( stream, dataPtrPtr, stream->bufPos, dataSize ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int sMemGetDataBlockAbs( INOUT STREAM *stream, 
						 IN_DATALENGTH_Z const int position, 
						 OUT_BUFFER_ALLOC_OPT( dataSize ) void **dataPtrPtr, 
						 IN_DATALENGTH const int dataSize )
	{
	/* REQUIRES() checking done in getMemoryBlock() */
	assert( isReadPtr( stream, sizeof( STREAM ) ) && \
			stream->type == STREAM_TYPE_MEMORY );
	assert( isWritePtr( dataPtrPtr, sizeof( void * ) ) );
	assert( position >= 0 && position <= stream->bufSize && \
			position < MAX_BUFFER_SIZE );
	assert( dataSize > 0 && dataSize < MAX_BUFFER_SIZE );

	/* Clear return values */
	*dataPtrPtr = NULL;

	return( getMemoryBlock( stream, dataPtrPtr, position, dataSize ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int sMemGetDataBlockRemaining( INOUT STREAM *stream, 
							   OUT_BUFFER_ALLOC_OPT( *length ) void **dataPtrPtr, 
							   OUT_DATALENGTH_Z int *length )
	{
	const int dataLeft = sMemDataLeft( stream );
	int status;

	assert( isReadPtr( stream, sizeof( STREAM ) ) && \
			stream->type == STREAM_TYPE_MEMORY );
	assert( isWritePtr( dataPtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( length, sizeof( int ) ) );
			/* REQUIRES() checking done in getMemoryBlock() */

	/* Clear return values */
	*dataPtrPtr = NULL;
	*length = 0;

	/* If there's no data remaining, return an underflow error */
	if( cryptStatusError( dataLeft ) )
		return( dataLeft );
	if( dataLeft <= 0 )
		return( CRYPT_ERROR_UNDERFLOW );

	status = getMemoryBlock( stream, dataPtrPtr, stream->bufPos, dataLeft );
	if( cryptStatusError( status ) )
		return( status );
	*length = dataLeft;

	return( CRYPT_OK );
	}
