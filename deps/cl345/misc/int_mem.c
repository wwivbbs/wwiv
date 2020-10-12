/****************************************************************************
*																			*
*					cryptlib Internal Memory Management API					*
*						Copyright Peter Gutmann 1992-2016					*
*																			*
****************************************************************************/

#include <stdarg.h>
#if defined( INC_ALL )
  #include "crypt.h"
#else
  #include "crypt.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						Checked Buffer Management Routines					*
*																			*
****************************************************************************/

/* Add cookies around a buffer, used to check for overwrites.  This converts
   a memory buffer:

	| bufPtr
	v
	+---------------+
	|	Buffer		|
	+---------------+
	|<-- bufSize -->|

   into:

			| bufPtr
			v
	+-------+---------------+-------+
	|Cookie |	Buffer		|Cookie	|
	+-------+---------------+-------+
			|<-- bufSize -->|

   To do this we need to convert an allocation of of bufSize bytes into one
   of COOKIE_SIZE + bufSize + COOKIE_SIZE bytes, and then return a pointer 
   to the actual buffer data inside the cookie'd buffer */

/* A buffer cookie, used as a canary to check for overwrites */

#ifdef FIXED_SEED
  #define SAFEBUFFER_COOKIE		( ( uintptr_t ) FIXED_SEED )
#else
  #define SAFEBUFFER_COOKIE		0xAAFFAAFF
#endif /* FIXED_SEED */

/* Initialise a safe buffer with canaries and check that the canaries 
   are still valid.  The annotations here aren't quite correct since 
   we're accessing memory just outside the buffer, but there's no way to 
   correctly annotate what's going on */

STDC_NONNULL_ARG( ( 1 ) ) \
void safeBufferInit( INOUT_BUFFER_FIXED( bufSize ) void *buffer, 
					 IN_DATALENGTH const int bufSize )
	{
	BYTE *bufPtr = ( ( BYTE * ) buffer ) - SAFEBUFFER_COOKIE_SIZE;
	const uintptr_t startCookie = ( uintptr_t ) buffer ^ SAFEBUFFER_COOKIE;
	const uintptr_t endCookie = \
						( ( uintptr_t ) bufPtr + SAFEBUFFER_COOKIE_SIZE + \
										bufSize ) ^ SAFEBUFFER_COOKIE;

	REQUIRES_V( bufSize >= 256 && bufSize < MAX_BUFFER_SIZE );

	/* Insert the cookies, which correspond to the address at which they're
	   stored XOR'd with a fixed magic value */
	*( ( uintptr_t * ) bufPtr ) = startCookie;
	*( ( uintptr_t * ) ( bufPtr + SAFEBUFFER_COOKIE_SIZE + bufSize ) ) = endCookie;
	}

CHECK_RETVAL_PTR \
void *safeBufferAlloc( IN_DATALENGTH const int bufSize )
	{
	void *bufPtr;
	
	REQUIRES_N( bufSize >= MIN_BUFFER_SIZE && bufSize < MAX_BUFFER_SIZE );
	
	/* Allocate the buffer, with extra space for the cookies and overflow 
	   data.  The overflow amount is added implicitly here because the 
	   caller only records the buffer size as being the base size without 
	   the overflow amount, which means that when we perform the cookie 
	   check on { buffer, bufSize } there's additional overflow space 
	   between the purported end of the buffer and the cookie */
	bufPtr = clAlloc( "safeBufferAlloc", SAFEBUFFER_SIZE( bufSize ) + 8 );
	if( bufPtr == NULL )
		return( NULL );
	safeBufferInit( SAFEBUFFER_PTR( bufPtr ), bufSize );

	return( SAFEBUFFER_PTR( bufPtr ) );
	}

void safeBufferFree( const void *buffer )
	{
	void *bufPtr = ( ( BYTE * ) buffer ) - SAFEBUFFER_COOKIE_SIZE;

	/* We can't check the overall buffer state since we don't know its size, 
	   but we can check at least the start cookie for validity */
	if( *( ( uintptr_t * ) bufPtr ) != ( ( uintptr_t ) buffer ^ SAFEBUFFER_COOKIE ) )
		{
		assert( DEBUG_WARN );
		return;
		}

	clFree( "safeBufferFree", bufPtr );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN safeBufferCheck( IN_BUFFER( bufSize ) const void *buffer, 
						 IN_DATALENGTH const int bufSize )
	{
	const BYTE *bufPtr = ( ( const BYTE * ) buffer ) - SAFEBUFFER_COOKIE_SIZE;

	REQUIRES_B( bufSize > 4 && bufSize < MAX_BUFFER_SIZE );

	/* Check that the cookies haven't been disturbed */
	if( *( ( uintptr_t * ) bufPtr ) != ( ( uintptr_t ) buffer ^ SAFEBUFFER_COOKIE ) )
		return( FALSE );
	if( *( ( uintptr_t * ) ( bufPtr + SAFEBUFFER_COOKIE_SIZE + bufSize ) ) != \
		( ( ( uintptr_t ) bufPtr + SAFEBUFFER_COOKIE_SIZE + bufSize ) ^ SAFEBUFFER_COOKIE ) )
		return( FALSE );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Dynamic Buffer Management Routines					*
*																			*
****************************************************************************/

/* Dynamic buffer management functions.  When reading variable-length
   object data we can usually fit the data into a small fixed-length buffer 
   but occasionally we have to cope with larger data amounts that require a 
   dynamically-allocated buffer.  The following routines manage this 
   process, dynamically allocating and freeing a larger buffer if required */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getDynData( OUT DYNBUF *dynBuf, 
					   IN_HANDLE const CRYPT_HANDLE cryptHandle,
					   IN_MESSAGE const MESSAGE_TYPE message, 
					   IN_INT const int messageParam )
	{
	MESSAGE_DATA msgData;
	void *dataPtr = NULL;
	int status;

	assert( isWritePtr( dynBuf, sizeof( DYNBUF ) ) );

	REQUIRES( isHandleRangeValid( cryptHandle ) );
	REQUIRES( ( message == IMESSAGE_GETATTRIBUTE_S && \
				( isAttribute( messageParam ) || \
				  isInternalAttribute( messageParam ) ) ) || \
			  ( message == IMESSAGE_CRT_EXPORT && \
		 		( messageParam == CRYPT_CERTFORMAT_CERTIFICATE || \
				  messageParam == CRYPT_CERTFORMAT_CERTCHAIN ) ) );

	/* Clear return values.  Note that we don't use the usual memset() to 
	   clear the value since the structure contains the storage for the 
	   fixed-size portion of the buffer appended to it, and using memset() 
	   to clear that is just unnecessary overhead */
	dynBuf->data = dynBuf->dataBuffer;
	dynBuf->length = 0;

	/* Get the data from the object */
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( cryptHandle, message, &msgData, messageParam );
	if( cryptStatusError( status ) )
		return( status );
	if( msgData.length > DYNBUF_SIZE )
		{
		/* The data is larger than the built-in buffer size, dynamically
		   allocate a larger buffer */
		if( ( dataPtr = clDynAlloc( "getDynData", msgData.length ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		msgData.data = dataPtr;
		status = krnlSendMessage( cryptHandle, message, &msgData,
								  messageParam );
		if( cryptStatusError( status ) )
			{
			clFree( "getDynData", dataPtr );
			return( status );
			}
		dynBuf->data = dataPtr;
		}
	else
		{
		/* The data will fit into the built-in buffer, read it directly into
		   the buffer */
		msgData.data = dynBuf->data;
		status = krnlSendMessage( cryptHandle, message, &msgData,
								  messageParam );
		if( cryptStatusError( status ) )
			return( status );
		}
	dynBuf->length = msgData.length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int dynCreate( OUT DYNBUF *dynBuf, 
			   IN_HANDLE const CRYPT_HANDLE cryptHandle,
			   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeType )
	{
	assert( isWritePtr( dynBuf, sizeof( DYNBUF ) ) );

	REQUIRES( isHandleRangeValid( cryptHandle ) );
	REQUIRES( isAttribute( attributeType ) || \
			  isInternalAttribute( attributeType ) );

	return( getDynData( dynBuf, cryptHandle, IMESSAGE_GETATTRIBUTE_S,
						attributeType ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int dynCreateCert( OUT DYNBUF *dynBuf, 
				   IN_HANDLE const CRYPT_HANDLE cryptHandle,
				   IN_ENUM( CRYPT_CERTFORMAT ) \
				   const CRYPT_CERTFORMAT_TYPE formatType )
	{
	assert( isWritePtr( dynBuf, sizeof( DYNBUF ) ) );

	REQUIRES( isHandleRangeValid( cryptHandle ) );
	REQUIRES( formatType == CRYPT_CERTFORMAT_CERTIFICATE || \
			  formatType == CRYPT_CERTFORMAT_CERTCHAIN );

	return( getDynData( dynBuf, cryptHandle, IMESSAGE_CRT_EXPORT, 
						formatType ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void dynDestroy( INOUT DYNBUF *dynBuf )
	{
	assert( isWritePtr( dynBuf, sizeof( DYNBUF ) ) );
	assert( isWritePtrDynamic( dynBuf->data, dynBuf->length ) );

	REQUIRES_V( dynBuf->data != NULL );
	REQUIRES_V( dynBuf->length > 0 && dynBuf->length < MAX_BUFFER_SIZE );

	zeroise( dynBuf->data, dynBuf->length );
	if( dynBuf->data != dynBuf->dataBuffer )
		clFree( "dynDestroy", dynBuf->data );
	dynBuf->data = NULL;
	dynBuf->length = 0;
	}

/****************************************************************************
*																			*
*						Memory Pool Management Routines						*
*																			*
****************************************************************************/

/* Memory pool management functions.  When allocating many small blocks of
   memory, especially in resource-constrained systems, it's better if we pre-
   allocate a small memory pool ourselves and grab chunks of it as required,
   falling back to dynamically allocating memory later on if we exhaust the
   pool.  The following functions implement the custom memory pool
   management.  Usage is:

	initMemPool( &memPoolState, storage, storageSize );
	newItem = getMemPool( &memPoolState, newItemSize ) */

typedef struct {
	BUFFER( storageSize, storagePos ) 
	void *storage;					/* Memory pool */
	int storageSize, storagePos;	/* Current usage and total size of pool */
	} MEMPOOL_INFO;

#ifndef CONFIG_CONSERVE_MEMORY_EXTRA

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckMempool( const MEMPOOL_INFO *state )
	{
	/* Make sure that the overall pool size information is in order */
	if( state->storageSize < 64 || \
		state->storageSize >= MAX_INTLENGTH_SHORT )
		{
		DEBUG_PUTS(( "sanityCheckMempool: Storage size" ));
		return( FALSE );
		}

	/* Make sure that the pool allocation information is in order */
	if( state->storagePos < 0 || \
		state->storagePos >= MAX_INTLENGTH_SHORT || \
		state->storagePos > state->storageSize )
		{
		DEBUG_PUTS(( "sanityCheckMempool: Storage position" ));
		return( FALSE );
		}

	return( TRUE );
	}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initMemPool( OUT void *statePtr, 
				  IN_BUFFER( memPoolSize ) void *memPool, 
				  IN_LENGTH_SHORT_MIN( 64 ) const int memPoolSize )
	{
	MEMPOOL_INFO *state = ( MEMPOOL_INFO * ) statePtr;

	assert( isWritePtr( state, sizeof( MEMPOOL_INFO ) ) );
	assert( isWritePtrDynamic( memPool, memPoolSize ) );

#if defined( __WIN32__ ) && defined( _MSC_VER )
	#pragma warning( disable: 4127 )	/* Needed for sizeof() in check */
#endif /* VC++ */
	REQUIRES( sizeof( MEMPOOL_STATE ) >= sizeof( MEMPOOL_INFO ) );
	REQUIRES( memPoolSize >= 64 && memPoolSize < MAX_INTLENGTH_SHORT );
#if defined( __WIN32__ ) && defined( _MSC_VER )
	#pragma warning( 4: 4127 )
#endif /* VC++ */

	memset( state, 0, sizeof( MEMPOOL_INFO ) );
	state->storage = memPool;
	state->storageSize = memPoolSize;

	return( CRYPT_OK );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
void *getMemPool( INOUT void *statePtr, IN_LENGTH_SHORT const int size )
	{
	MEMPOOL_INFO *state = ( MEMPOOL_INFO * ) statePtr;
	BYTE *allocPtr;
	const int allocSize = roundUp( size, sizeof( int ) );

	assert( isWritePtr( state, sizeof( MEMPOOL_INFO ) ) );
	assert( isWritePtrDynamic( state->storage, state->storageSize ) );

	REQUIRES_N( isShortIntegerRangeNZ( size ) );
	REQUIRES_N( allocSize >= sizeof( int ) && \
				allocSize < MAX_INTLENGTH_SHORT );
	REQUIRES_N( sanityCheckMempool( state ) );

	/* If we can't satisfy the request from the memory pool we have to
	   allocate the memory block dynamically */
	if( state->storagePos + allocSize > state->storageSize )
		return( clDynAlloc( "getMemPool", size ) );

	/* We can satisfy the request from the pool:

	 memPool
		|
		v		 <- size -->
		+-------+-----------+-------+
		|		|			|		|
		+-------+-----------+-------+
				^			^
				|			|
			storagePos	storagePos' */
	allocPtr = ( BYTE * ) state->storage + state->storagePos;
	state->storagePos += allocSize;
	ENSURES_N( sanityCheckMempool( state ) );

	return( allocPtr );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void freeMemPool( INOUT void *statePtr, IN void *memblock )
	{
	MEMPOOL_INFO *state = ( MEMPOOL_INFO * ) statePtr;

	assert( isWritePtr( state, sizeof( MEMPOOL_INFO ) ) );
	assert( isWritePtrDynamic( state->storage, state->storageSize ) );

	REQUIRES_V( sanityCheckMempool( state ) );

	/* If the memory block to free lies within the pool, there's nothing to 
	   do */
	if( memblock >= state->storage && \
		memblock < ( void * ) ( ( BYTE * ) state->storage + \
										   state->storageSize ) )
		return;

	/* It's outside the pool and therefore dynamically allocated, free it */
	clFree( "freeMemPool", memblock );
	}

/****************************************************************************
*																			*
*							Debugging Malloc Support						*
*																			*
****************************************************************************/

/* Debugging malloc() that dumps memory usage diagnostics to stdout.  Note
   that these functions are only intended to be used during interactive 
   debugging sessions since they throw exceptions under error conditions 
   rather than returning an error status (the fact that they dump 
   diagnostics to stdout during operation should be a clue as to their
   intended status and usage) */

#ifdef CONFIG_DEBUG_MALLOC

#ifdef __WIN32__
  #include <direct.h>
#endif /* __WIN32__ */

#ifdef __WINCE__

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH_STRING ) STDC_NONNULL_ARG( ( 1 ) ) \
static int wcPrintf( FORMAT_STRING const char *format, ... )
	{
	wchar_t wcBuffer[ 1024 + 8 ];
	char buffer[ 1024 + 8 ];
	va_list argPtr;
	int length;

	va_start( argPtr, format );
	length = vsprintf_s( buffer, 1024, format, argPtr );
	va_end( argPtr );
	if( length < 1 )
		return( length );
	mbstowcs( wcBuffer, buffer, length + 1 );
	NKDbgPrintfW( wcBuffer );

	return( length );
	}

#define printf		wcPrintf

#endif /* __WINCE__ */

static int clAllocIndex = 0;

void *clAllocFn( const char *fileName, const char *fnName,
				 const int lineNo, size_t size )
	{
#ifdef CONFIG_MALLOCTEST
	static int mallocCount = 0, mallocFailCount = 0;
#endif /* CONFIG_MALLOCTEST */
#if defined( __WIN32__ ) || defined( __UNIX__ )
	char buffer[ 512 + 8 ];
#endif /* __WIN32__ || __UNIX__ */
	BYTE *memPtr;
	int length;

	assert( fileName != NULL );
	assert( fnName != NULL );
	assert( lineNo > 0 );
	assert( isIntegerRangeNZ( size ) );

	/* Strip off the leading path components if we can to reduce the amount
	   of noise in the output */
#if defined( __WIN32__ ) || defined( __UNIX__ )
	if( getcwd( buffer, 512 ) != NULL )
		{
		const int pathLen = strlen( buffer ) + 1;	/* Leading path + '/' */

		assert( pathLen < strlen( fileName ) );
		fileName += pathLen;
		}
#endif /* __WIN32__ || __UNIX__ */

	length = DEBUG_PRINT(( "ALLOC: %s:%s:%d", fileName, fnName, lineNo ));
	while( length < 46 )
		{
		DEBUG_PRINT(( " " ));
		length++;
		}
	DEBUG_PRINT(( " %4d - %d bytes.\n", clAllocIndex, size ));
#ifdef CONFIG_MALLOCTEST
	/* If we've exceeded the allocation count, make the next attempt to 
	   allocate memory fail */
	if( mallocCount >= mallocFailCount )
		{
		mallocCount = 0;
		mallocFailCount++;

		return( NULL );
		}
	mallocCount++;
#endif /* CONFIG_MALLOCTEST */
	if( ( memPtr = malloc( size + sizeof( LONG ) ) ) == NULL )
		return( NULL );
	mputLong( memPtr, clAllocIndex );	/* Implicit memPtr += sizeof( LONG ) */
	clAllocIndex++;
	return( memPtr );
	}

void clFreeFn( const char *fileName, const char *fnName,
			   const int lineNo, void *memblock )
	{
#if defined( __WIN32__ ) || defined( __UNIX__ )
	char buffer[ 512 + 8 ];
#endif /* __WIN32__ || __UNIX__ */
	BYTE *memPtr = ( BYTE * ) memblock - sizeof( LONG );
	int index, length;

	assert( fileName != NULL );
	assert( fnName != NULL );
	assert( lineNo > 0 );

	/* Strip off the leading path components if we can to reduce the amount
	   of noise in the output */
#if defined( __WIN32__ ) || defined( __UNIX__ )
	if( getcwd( buffer, 512 ) != NULL )
		{
		const int pathLen = strlen( buffer ) + 1;	/* Leading path + '/' */

		assert( pathLen < strlen( fileName ) );
		fileName += pathLen;
		}
#endif /* __WIN32__ || __UNIX__ */

	index = mgetLong( memPtr );
	memPtr -= sizeof( LONG );		/* mgetLong() changes memPtr */
	length = DEBUG_PRINT(( "FREE : %s:%s:%d", fileName, fnName, lineNo ));
	while( length < 46 )
		{
		DEBUG_PRINT(( " " ));
		length++;
		}
	DEBUG_PRINT(( " %4d.\n", index ));
	free( memPtr );
	}
#endif /* CONFIG_DEBUG_MALLOC */

/* Fault-testing malloc() that fails after a given number of allocations */

#ifdef CONFIG_FAULT_MALLOC

static int currentAllocCount = 0, failAllocCount = 0;
static BOOLEAN allocFailed = FALSE;

void clFaultAllocSetCount( const int number )
	{
	currentAllocCount = 0;
	failAllocCount = number;
	allocFailed = FALSE;
	}

void *clFaultAllocFn( const char *fileName, const char *fnName, 
					  const int lineNo, size_t size )
	{
	/* If we've failed an allocation we probably shouldn't get here again,
	   however if we're running a multithreaded init then the second thread 
	   could try and allocate memory after the first one has failed */
	if( allocFailed )
		{
#ifdef __WIN32__
		DEBUG_PRINT(( "\n<<< Further allocation call from thread %X after "
					  "previous call failed, called from %s line %d in "
					  "%s.>>>\n", GetCurrentThreadId(), fnName, lineNo, 
					  fileName ));
#else
		DEBUG_PRINT(( "\n<<< Further allocation call after previous call "
					  "failed, called from %s line %d in %s.>>>\n", fnName, 
					  lineNo, fileName ));
#endif /* __WIN32__  */
		if( failAllocCount < 15 )
			{
			DEBUG_PRINT(( "<<<  (This could be because of a multithreaded "
						  "init).>>>\n" ));
			DEBUG_PRINT(( "\n" ));
			}
		return( NULL );
		}

	/* If we haven't reached the failure allocation count, return normally */
	if( currentAllocCount < failAllocCount )
		{
		currentAllocCount++;
		return( malloc( size ) );
		}

	/* We've reached the failure count, fail the allocation */
#ifdef __WIN32__
	DEBUG_PRINT(( "\n<<< Failing allocation call #%d for thread %X, called "
				  "from %s line %d in %s.>>>\n\n", failAllocCount + 1, 
				  GetCurrentThreadId(), fnName, lineNo, fileName ));
#else
	DEBUG_PRINT(( "\n<<< Failing at allocation call #%d, called from %s line "
				  "%d in %s.>>>\n\n", failAllocCount + 1, fnName, lineNo, 
				  fileName ));
#endif /* __WIN32__  */
	allocFailed = TRUE;
	return( NULL );
	}
#endif /* CONFIG_FAULT_MALLOC */
