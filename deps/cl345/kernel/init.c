/****************************************************************************
*																			*
*								Kernel Initialisation						*
*						Copyright Peter Gutmann 1997-2005					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "acl.h"
  #include "kernel.h"
#else
  #include "crypt.h"
  #include "kernel/acl.h"
  #include "kernel/kernel.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check object data */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckObject( const OBJECT_INFO *objectInfoPtr )
	{
	assert( isReadPtr( objectInfoPtr, sizeof( OBJECT_INFO ) ) );

	/* Check general object data */
	if( !isEnumRange( objectInfoPtr->type, OBJECT_TYPE ) )
		{
		DEBUG_PUTS(( "sanityCheckObject: Type" ));
		return( FALSE );
		}
	if( !isEnumRange( objectInfoPtr->subType, SUBTYPE ) )
		{
		DEBUG_PUTS(( "sanityCheckObject: Subtype" ));
		return( FALSE );
		}
	if( !CHECK_FLAGS( objectInfoPtr->flags, OBJECT_FLAG_NONE, 
					  OBJECT_FLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckObject: Flags" ));
		return( FALSE );
		}
	if( !DATAPTR_ISSET( objectInfoPtr->objectPtr ) )
		{
		DEBUG_PUTS(( "sanityCheckObject: Object info" ));
		return( FALSE );
		}
	if( objectInfoPtr->type == OBJECT_TYPE_CONTEXT && \
		objectInfoPtr->subType == SUBTYPE_CTX_PKC )
		{
		if( objectInfoPtr->objectSize < 1024 || \
			objectInfoPtr->objectSize > MAX_INTLENGTH )
			{
			DEBUG_PUTS(( "sanityCheckObject: PKC object size" ));
			return( FALSE );
			}
		}
	else
		{
		if( objectInfoPtr->objectSize < 32 || \
			objectInfoPtr->objectSize > MAX_INTLENGTH_SHORT )
			{
			DEBUG_PUTS(( "sanityCheckObject: Object size" ));
			return( FALSE );
			}
		}

	/* Check safe pointers.  We check the function pointer as well even 
	   though it's somewhat redundant because it's validated each time its 
	   dereferenced */
	if( !DATAPTR_ISVALID( objectInfoPtr->objectPtr ) )
		{
		DEBUG_PUTS(( "sanityCheckObject: Object pointer" ));
		return( FALSE );
		}
	if( !FNPTR_ISVALID( objectInfoPtr->messageFunction ) )
		{
		DEBUG_PUTS(( "sanityCheckObject: Message function" ));
		return( FALSE );
		}

	/* Check object properties */
	if( objectInfoPtr->type == OBJECT_TYPE_CONTEXT )
		{
		switch( objectInfoPtr->subType )
			{
			case SUBTYPE_CTX_CONV:
				if( objectInfoPtr->actionFlags & \
						~( MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_GENKEY, ACTION_PERM_ALL ) ) )
					{
					DEBUG_PUTS(( "sanityCheckObject: Conventional actions" ));
					return( FALSE );
					}
				break;

			case SUBTYPE_CTX_PKC:
				if( objectInfoPtr->actionFlags & \
						~( MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_SIGN, ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_GENKEY, ACTION_PERM_ALL ) ) )
					{
					DEBUG_PUTS(( "sanityCheckObject: PKC actions" ));
					return( FALSE );
					}
				break;

			case SUBTYPE_CTX_HASH:
				if( objectInfoPtr->actionFlags & \
						~MK_ACTION_PERM( MESSAGE_CTX_HASH, ACTION_PERM_ALL ) )
					{
					DEBUG_PUTS(( "sanityCheckObject: Hash actions" ));
					return( FALSE );
					}
				break;

			case SUBTYPE_CTX_MAC:
				if( objectInfoPtr->actionFlags & \
						~( MK_ACTION_PERM( MESSAGE_CTX_HASH, ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_GENKEY, ACTION_PERM_ALL ) ) )
					{
					DEBUG_PUTS(( "sanityCheckObject: MAC actions" ));
					return( FALSE );
					}
				break;

			case SUBTYPE_CTX_GENERIC:
				if( ( objectInfoPtr->actionFlags & \
						~MK_ACTION_PERM( MESSAGE_CTX_GENKEY, ACTION_PERM_ALL ) ) && \
					( objectInfoPtr->actionFlags != ACTION_PERM_NONE_ALL ) )
					{
					DEBUG_PUTS(( "sanityCheckObject: Generic actions" ));
					return( FALSE );
					}
				break;

			default:
				retIntError_Boolean();
			}
		}
	else
		{
		if( objectInfoPtr->actionFlags != ACTION_PERM_FLAG_NONE )
			{
			DEBUG_PUTS(( "sanityCheckObject: Spurious actions" ));
			return( FALSE );
			}
		}
	if( !isIntegerRange( objectInfoPtr->intRefCount ) || \
		!isIntegerRange( objectInfoPtr->extRefCount ) )
		{
		DEBUG_PUTS(( "sanityCheckObject: Reference count" ));
		return( FALSE );
		}
	if( !isIntegerRange( objectInfoPtr->lockCount ) ) 
		{
		DEBUG_PUTS(( "sanityCheckObject: Lock count" ));
		return( FALSE );
		}

	/* Check object methods and related objects */
	if( ( objectInfoPtr->type == OBJECT_TYPE_DEVICE && \
		  objectInfoPtr->owner == CRYPT_UNUSED ) || \
		( objectInfoPtr->type == OBJECT_TYPE_USER && \
		  objectInfoPtr->owner == SYSTEM_OBJECT_HANDLE ) )
		{
		/* The system object and default user object have special-case 
		   properties */
		if( objectInfoPtr->dependentObject != CRYPT_ERROR || \
			objectInfoPtr->dependentDevice != CRYPT_ERROR )
			{
			DEBUG_PUTS(( "sanityCheckObject: Spurious dependent objects" ));
			return( FALSE );
			}
		}
	else

		{
		if( objectInfoPtr->owner != DEFAULTUSER_OBJECT_HANDLE && \
			!isHandleRangeValid( objectInfoPtr->owner ) )
			{
			DEBUG_PUTS(( "sanityCheckObject: Owner handle" ));
			return( FALSE );
			}
		if( objectInfoPtr->dependentObject != CRYPT_ERROR && \
			!isHandleRangeValid( objectInfoPtr->dependentObject ) )
			{
			DEBUG_PUTS(( "sanityCheckObject: Dependent object" ));
			return( FALSE );
			}
		if( objectInfoPtr->dependentDevice != CRYPT_ERROR && \
			objectInfoPtr->dependentDevice != SYSTEM_OBJECT_HANDLE && \
			!isHandleRangeValid( objectInfoPtr->dependentDevice ) )
			{
			DEBUG_PUTS(( "sanityCheckObject: Dependent device" ));
			return( FALSE );
			}
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*								Thread Functions							*
*																			*
****************************************************************************/

/* Execute a function in a background thread.  This takes a pointer to the
   function to execute in the background thread, a set of parameters to pass
   to the function, and an optional semaphore ID to set once the thread is
   started.  A function is run via a background thread as follows:

	void threadFunction( const THREAD_FUNCTION_PARAMS *threadParams )
		{
		}

	initThreadParams( &threadParams, ptrParam, intParam );
	krnlDispatchThread( threadFunction, &threadParams, SEMAPHORE_ID ) */

#ifdef USE_THREAD_FUNCTIONS

/* The function that's run as a thread.  This calls the user-supplied
   service function with the user-supplied parameters */

THREADFUNC_DEFINE( threadServiceFunction, threadInfoPtr )
	{
	const THREAD_INFO *threadInfo = ( THREAD_INFO * ) threadInfoPtr;
	const THREAD_FUNCTION threadFunction = ( THREAD_FUNCTION ) \
							FNPTR_GET( threadInfo->threadFunction );
	ORIGINAL_INT_VAR( intParam, threadInfo->threadParams.intParam );
	ORIGINAL_INT_VAR( semaphore, threadInfo->semaphore );
		/* Note that the above two macros give initialised-but-not-referenced
		   warnings in release builds */

	assert( isReadPtr( threadInfoPtr, sizeof( THREAD_INFO ) ) );
	assert( threadServiceFunction != NULL );
			/* We can't use a REQUIRES() because of the polymorphic return 
			   type */
	if( threadFunction == NULL )
		{
		/* It's a bit unclear what we should do in this case since it's a 
		   shouldn't-occur condition, exiting now seems to be the least
		   unsafe action */
		THREAD_EXIT( threadInfo->syncHandle );
		}

	/* We're running as a thread, call the thread service function and clear
	   the associated semaphore (if there is one) when we're done.  We check
	   to make sure that the thread params are unchanged to catch erroneous
	   use of stack-based storage for the parameter data */
	threadFunction( &threadInfo->threadParams );
	assert( threadInfo->threadParams.intParam == ORIGINAL_VALUE( intParam ) );
	assert( threadInfo->semaphore == ORIGINAL_VALUE( semaphore ) );
	if( threadInfo->semaphore != SEMAPHORE_NONE )
		clearSemaphore( threadInfo->semaphore );
	THREAD_EXIT( threadInfo->syncHandle );
	}

/* Dispatch a function in a background thread.  If the threadParams value
   is NULL we use the kernel's thread data storage, otherwise we use the
   caller-provided storage */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int krnlDispatchThread( THREAD_FUNCTION threadFunction,
						THREAD_STATE threadState, void *ptrParam, 
						const int intParam, const SEMAPHORE_TYPE semaphore )
	{
	KERNEL_DATA *krnlData = getKrnlData();
	THREAD_INFO *threadInfo = \
			( threadState == NULL ) ? &krnlData->threadInfo : \
									  ( THREAD_INFO * ) threadState;
	THREAD_HANDLE dummy = THREAD_INITIALISER;
	int status;

	assert( threadState == NULL || \
			isWritePtr( threadState, sizeof( THREAD_STATE ) ) );

	static_assert( sizeof( THREAD_STATE ) >= sizeof( THREAD_INFO ), \
				   "Thread storage size" );

	/* Preconditions: The parameters appear valid, and it's a valid
	   semaphore (SEMAPHORE_NONE is valid since it indicates that the caller
	   doesn't want a semaphore set) */
	REQUIRES( threadFunction != NULL );
	REQUIRES( isEnumRangeOpt( semaphore, SEMAPHORE ) );

	/* Initialise the thread parameters */
	memset( threadInfo, 0, sizeof( THREAD_INFO ) );
	FNPTR_SET( threadInfo->threadFunction, threadFunction );
	threadInfo->threadParams.ptrParam = ptrParam;
	threadInfo->threadParams.intParam = intParam;
	threadInfo->semaphore = semaphore;

	/* Fire up the thread and set the associated semaphore if required.
	   There's no problem with the thread exiting before we set the
	   semaphore because it's a one-shot, so if the thread gets there first
	   the attempt to set the semaphore below is ignored */
	THREAD_CREATE( threadServiceFunction, threadInfo, dummy,
				   threadInfo->syncHandle, status );
	if( cryptStatusOK( status ) && semaphore != SEMAPHORE_NONE )
		setSemaphore( semaphore, threadInfo->syncHandle );
	return( status );
	}
#endif /* USE_THREAD_FUNCTIONS */

/****************************************************************************
*																			*
*							Pre-initialisation Functions					*
*																			*
****************************************************************************/

/* Correct initialisation of the kernel is handled by having the object
   management functions check the state of the initialisation flag before
   they do anything and returning CRYPT_ERROR_NOTINITED if cryptlib hasn't
   been initialised.  Since everything in cryptlib depends on the creation
   of objects, any attempt to use cryptlib without it being properly
   initialised are caught.

   Reading the initialisation flag presents something of a chicken-and-egg
   problem since the read should be protected by the intialisation mutex,
   but we can't try and grab it unless the mutex has been initialised.  If
   we just read the flag directly and rely on the object map mutex to
   protect access we run into a potential race condition on shutdown:

	thread1								thread2

	inited = T							read inited = T
	inited = F, destroy objects
										lock objects, die

   The usual way to avoid this is to perform an interlocked mutex lock, but
   this isn't possible here since the initialisation mutex may not be
   initialised.

   If possible we use dynamic initialisation of the kernel to resolve this,
   taking advantage of stubs that the compiler inserts into the code to
   perform initialisation functions when cryptlib is loaded.  If the
   compiler doesn't support this, we have to use static initialisation.
   This has a slight potential race condition if two threads call the init
   function at the same time, but in practice the only thing that can happen
   is that the initialisation mutex gets initialised twice, leading to a
   small resource leak when cryptlib shuts down */

#if defined( __WIN32__ ) || defined( __WINCE__ )
  /* Windows supports dynamic initialisation by allowing the init/shutdown
	 functions to be called from DllMain(), however if we're building a
	 static library there won't be a DllMain() so we have to do a static
	 init */
  #ifdef STATIC_LIB
	#define STATIC_INIT
  #endif /* STATIC_LIB */
#elif defined( __GNUC__ ) && defined( __PIC__ ) && defined( USE_THREADS )
  /* If we're being built as a shared library with gcc, we can use
	 constructor and destructor functions to automatically perform pre-init
	 and post-shutdown functions in a thread-safe manner.  By telling gcc
	 to put the preInit() and postShutdown() functions in the __CTOR_LIST__
	 and __DTOR_LIST__, they're called automatically before dlopen/dlclose
	 return */
  void preInit( void ) __attribute__ ((constructor));
  void postShutdown( void ) __attribute__ ((destructor));
#elif defined( __SUNPRO_C ) && ( __SUNPRO_C >= 0x570 )
  /* The value of __SUNPRO_C bears no relation whatsoever to the actual 
	 version number of the compiler and even Sun's docs give different 
	 values in different places for the same compiler version, but 0x570 
	 seems to work */
  #pragma init ( preInit )
  #pragma fini ( postShutdown )
#elif defined( __PALMOS__ )
  /* PalmOS supports dynamic initialisation by allowing the init/shutdown
	 functions to be called from PilotMain */
#else
  #define STATIC_INIT
#endif /* Systems not supporting dynamic initialisation */

/* Before we can begin and end the initialisation process, we need to
   initialise the initialisation lock.  This gets a bit complex, and is
   handled in the following order of preference:

	A. Systems where the OS contacts a module to tell it to initialise itself
	   before it's called directly for the first time.

	B. Systems where statically initialising the lock to an all-zero value is
	   equivalent to intialising it at runtime.

	C. Systems where the lock must be statically initialised at runtime.

   A and B are thread-safe, C isn't thread-safe but unlikely to be a problem
   except in highly unusual situations (two different threads entering
   krnlBeginInit() at the same time) and not something that we can fix
   without OS support.

   To handle this pre-initialisation, we provide the following functions for
   use with case A, statically initialise the lock to handle case B, and
   initialise it if required in krnlBeginInit() to handle case C */

void preInit( void )
	{
	KERNEL_DATA *krnlData = getKrnlData();	/* For MUTEX access */
	int status;

	initBuiltinStorage();
	MUTEX_CREATE( initialisation, status );
	if( cryptStatusError( status ) )
		{
		/* Error handling at this point gets a bit complicated since these 
		   functions are called before the main() is called or dlopen()
		   returns, so there's no way to react to an error status.  Even
		   the debug exception thrown by retIntError() may be dangerous,
		   but it's only used in the debug version when (presumably) some
		   sort of debugging support is present.  In any case if the mutex
		   create fails at this point (a) something is seriously wrong and 
		   (b) presumably successive mutex creations will fail as well, at
		   which point they can be detected */
		retIntError_Void();
		}
	}

void postShutdown( void )
	{
	KERNEL_DATA *krnlData = getKrnlData();	/* For MUTEX access */

	MUTEX_DESTROY( initialisation );
	destroyBuiltinStorage();
	}

/****************************************************************************
*																			*
*							Initialisation Functions						*
*																			*
****************************************************************************/

/* Begin and complete the kernel initialisation, leaving the initialisation
   mutex locked between the two calls to allow external initialisation of
   further, non-kernel-related items */

CHECK_RETVAL_ACQUIRELOCK( MUTEX_LOCKNAME( initialisation ) ) \
int krnlBeginInit( void )
	{
	KERNEL_DATA *krnlData = getKrnlData();
	int status;

#ifdef STATIC_INIT
	/* If the krnlData hasn't been set up yet, set it up now */
	if( krnlData->initLevel <= INIT_LEVEL_NONE )
		preInit();
#endif /* STATIC_INIT */

	/* Lock the initialisation mutex to make sure that other threads don't
	   try to access it */
	MUTEX_LOCK( initialisation );

	/* If we're already initialised, don't to anything */
	if( krnlData->initLevel > INIT_LEVEL_NONE )
		{
		MUTEX_UNLOCK( initialisation );
		return( CRYPT_ERROR_INITED );
		}

#ifndef USE_EMBEDDED_OS
	/* If the time is screwed up we can't safely do much since so many
	   protocols and operations depend on it, however since embedded 
	   systems may not have RTCs or if they have them they're inevitably 
	   not set right, we don't perform this sanity-check if it's an
	   embedded build */
	if( getTime() <= MIN_TIME_VALUE )
		{
		MUTEX_UNLOCK( initialisation );
		DEBUG_DIAG(( "System time is severely messed up, cannot continue "
					 "without a correctly set system clock" ));
		retIntError();
		}
#endif /* USE_EMBEDDED_OS */

	/* Initialise the ephemeral portions of the kernel data block.  Since
	   the shutdown level value is non-ephemeral (it has to persist across
	   shutdowns to handle threads that may still be active inside cryptlib
	   when a shutdown occurs), we have to clear this explicitly */
	clearKernelData();
	krnlData->shutdownLevel = SHUTDOWN_LEVEL_NONE;

	/* Initialise all of the kernel modules.  Except for the allocation of
	   the kernel object table this is all straight static initialistion
	   and self-checking, so we should never fail at this stage */
	status = initAllocation();
	if( cryptStatusOK( status ) )
		status = initAttributeACL();
	if( cryptStatusOK( status ) )
		status = initCertMgmtACL();
	if( cryptStatusOK( status ) )
		status = initInternalMsgs();
	if( cryptStatusOK( status ) )
		status = initKeymgmtACL();
	if( cryptStatusOK( status ) )
		status = initMechanismACL();
	if( cryptStatusOK( status ) )
		status = initMessageACL();
	if( cryptStatusOK( status ) )
		status = initObjects();
	if( cryptStatusOK( status ) )
		status = initObjectAltAccess();
	if( cryptStatusOK( status ) )
		status = initSemaphores();
	if( cryptStatusOK( status ) )
		status = initSendMessage();
	if( cryptStatusError( status ) )
		{
		MUTEX_UNLOCK( initialisation );
#ifdef CONFIG_FAULT_MALLOC
		/* If we're using memory fault-injection then a failure at this 
		   point is expected */
		return( CRYPT_ERROR_MEMORY );
#else
		retIntError();
#endif /* CONFIG_FAULT_MALLOC */
		}

	/* The kernel data block has been initialised */
	krnlData->initLevel = INIT_LEVEL_KRNLDATA;

	return( CRYPT_OK );
	}

RELEASELOCK( MUTEX_LOCKNAME( initialisation ) ) \
void krnlCompleteInit( void )
	{
	KERNEL_DATA *krnlData = getKrnlData();

	/* We've completed the initialisation process */
	krnlData->initLevel = INIT_LEVEL_FULL;

	MUTEX_UNLOCK( initialisation );
	}

/* Begin and complete the kernel shutdown, leaving the initialisation
   mutex locked between the two calls to allow external shutdown of
   further, non-kernel-related items.  The shutdown proceeds as follows:

	lock initialisation mutex;
	signal internal worker threads (async.init, randomness poll)
		to exit (shutdownLevel = SHUTDOWN_LEVEL_THREADS);
	signal all non-destroy messages to fail
		(shutdownLevel = SHUTDOWN_LEVEL_MESSAGES in destroyObjects());
	destroy objects (via destroyObjects());
	shut down kernel modules;
	shut down kernel mechanisms (semaphores, messages)
		(shutdownLevel = SHUTDOWN_LEVEL_MUTEXES);
	clear kernel data; */

CHECK_RETVAL_ACQUIRELOCK( MUTEX_LOCKNAME( initialisation ) ) \
int krnlBeginShutdown( void )
	{
	KERNEL_DATA *krnlData = getKrnlData();

	/* Lock the initialisation mutex to make sure that other threads don't
	   try to access it */
	MUTEX_LOCK( initialisation );

	/* We can only begin a shutdown if we're fully initialised */
	REQUIRES_MUTEX( krnlData->initLevel == INIT_LEVEL_FULL, \
					initialisation );

	/* If we're already shut down, don't to anything */
	if( krnlData->initLevel <= INIT_LEVEL_NONE )
		{
		MUTEX_UNLOCK( initialisation );
		return( CRYPT_ERROR_NOTINITED );
		}
	krnlData->initLevel = INIT_LEVEL_KRNLDATA;

	/* Signal all remaining internal threads to exit (dum differtur, vita 
	   transcurrit) */
	krnlData->shutdownLevel = SHUTDOWN_LEVEL_THREADS;

	return( CRYPT_OK );
	}

RETVAL_RELEASELOCK( MUTEX_LOCKNAME( initialisation ) ) \
int krnlCompleteShutdown( void )
	{
	KERNEL_DATA *krnlData = getKrnlData();

	/* Once the kernel objects have been destroyed, we're in the closing-down
	   state in which no more messages are processed.  There are a few 
	   special-case situations such as a shutdown that occurs because of a
	   failure to initialise that we also need to handle */
	REQUIRES( ( krnlData->initLevel == INIT_LEVEL_KRNLDATA && \
				krnlData->shutdownLevel == SHUTDOWN_LEVEL_NONE ) || \
			  ( krnlData->initLevel == INIT_LEVEL_KRNLDATA && \
				krnlData->shutdownLevel == SHUTDOWN_LEVEL_MESSAGES ) || \
			  ( krnlData->initLevel == INIT_LEVEL_FULL && \
				krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MESSAGES ) );

	/* Shut down all of the kernel modules */
	endAllocation();
	endAttributeACL();
	endCertMgmtACL();
	endInternalMsgs();
	endKeymgmtACL();
	endMechanismACL();
	endMessageACL();
	endObjects();
	endObjectAltAccess();
	endSemaphores();
	endSendMessage();

	/* At this point all kernel services have been shut down */
	ENSURES( krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MUTEXES );

	/* Turn off the lights on the way out.  Note that the kernel data-
	   clearing operation leaves the shutdown level set to handle any
	   threads that may still be active */
	clearKernelData();
	krnlData->shutdownLevel = SHUTDOWN_LEVEL_ALL;
	MUTEX_UNLOCK( initialisation );

#ifdef STATIC_INIT
	postShutdown();
#endif /* STATIC_INIT */

	return( CRYPT_OK );
	}

/* Indicate to a cryptlib-internal worker thread that the kernel is shutting
   down and the thread should exit as quickly as possible.  We don't protect
   this check with a mutex since it can be called after the kernel mutexes
   have been destroyed.  This lack of mutex protection for the flag isn't a
   serious problem, it's checked at regular intervals by worker threads so
   if the thread misses the flag update it'll bve caught at the next check */

CHECK_RETVAL_BOOL \
BOOLEAN krnlIsExiting( void )
	{
	const KERNEL_DATA *krnlData = getKrnlData();

	return( ( krnlData->shutdownLevel >= SHUTDOWN_LEVEL_THREADS ) ? \
			TRUE : FALSE );
	}

/****************************************************************************
*																			*
*							Miscellaneous Functions							*
*																			*
****************************************************************************/

/* Oddball functions that are placed here because there's no other obvious
   place for them.
   
   Sleep for a given number of milliseconds.  This function is used to 
   dither results from (failed) crypto operations in order to make them less
   susceptible to timing attacks */

CHECK_RETVAL \
int krnlWait( IN_RANGE( 1, 10000 ) const int milliSeconds )
	{
	REQUIRES( milliSeconds >= 1 && milliSeconds <= 10000 );

	THREAD_SLEEP( milliSeconds );

	return( CRYPT_OK );
	}
