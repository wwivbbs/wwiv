/****************************************************************************
*																			*
*							Semaphores and Mutexes							*
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

/* A pointer to the kernel data block */

static KERNEL_DATA *krnlData = NULL;

/****************************************************************************
*																			*
*							Init/Shutdown Functions							*
*																			*
****************************************************************************/

/* A template to initialise the semaphore table */

static const SEMAPHORE_INFO SEMAPHORE_INFO_TEMPLATE = \
				{ SEMAPHORE_STATE_UNINITED, 0, 0 };

/* Create and destroy the semaphores and mutexes.  Since mutexes usually 
   aren't scalar values and are declared and accessed via macros that 
   manipulate various fields, we have to handle a pile of them individually 
   rather than using an array of mutexes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initSemaphores( INOUT KERNEL_DATA *krnlDataPtr )
	{
	int i, status;

	assert( isWritePtr( krnlDataPtr, sizeof( KERNEL_DATA ) ) );

	static_assert( MUTEX_LAST == 4, "Mutex value" );

	/* Set up the reference to the kernel data block */
	krnlData = krnlDataPtr;

	/* Clear the semaphore table */
	for( i = 0; i < SEMAPHORE_LAST; i++ )
		krnlData->semaphoreInfo[ i ] = SEMAPHORE_INFO_TEMPLATE;

	/* Initialize any data structures required to make the semaphore table
	   thread-safe */
	MUTEX_CREATE( semaphore, status );
	ENSURES( cryptStatusOK( status ) );

	/* Initialize the mutexes */
	MUTEX_CREATE( mutex1, status );
	ENSURES( cryptStatusOK( status ) );
	MUTEX_CREATE( mutex2, status );
	ENSURES( cryptStatusOK( status ) );
	MUTEX_CREATE( mutex3, status );
	ENSURES( cryptStatusOK( status ) );

	return( CRYPT_OK );
	}

void endSemaphores( void )
	{
	REQUIRES_V( ( krnlData->initLevel == INIT_LEVEL_KRNLDATA && \
				  krnlData->shutdownLevel == SHUTDOWN_LEVEL_NONE ) || \
				( krnlData->initLevel == INIT_LEVEL_KRNLDATA && \
				  krnlData->shutdownLevel == SHUTDOWN_LEVEL_MESSAGES ) || \
				( krnlData->initLevel == INIT_LEVEL_FULL && \
				  krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MESSAGES ) );

	/* Signal that kernel mechanisms are no longer available */
	krnlData->shutdownLevel = SHUTDOWN_LEVEL_MUTEXES;

	/* Shut down the mutexes */
	MUTEX_DESTROY( mutex3 );
	MUTEX_DESTROY( mutex2 );
	MUTEX_DESTROY( mutex1 );

	/* Destroy any data structures required to make the semaphore table
	   thread-safe */
	MUTEX_DESTROY( semaphore );
	}

/****************************************************************************
*																			*
*							Semaphore Functions								*
*																			*
****************************************************************************/

/* Under multithreaded OSes, we often need to wait for certain events before
   we can continue (for example when asynchronously accessing system
   objects anything that depends on the object being available needs to
   wait for the access to complete) or handle mutual exclusion when accessing
   a shared resource.  The following functions abstract this handling,
   providing a lightweight semaphore mechanism via mutexes, which is used 
   before checking a system synchronisation object (mutexes usually don't
   require a kernel entry, while semaphores usually do).  The semaphore 
   function works a bit like the Win32 Enter/LeaveCriticalSection() 
   routines, which perform a quick check on a user-level lock and only call 
   the kernel-level handler if necessary (in most cases this isn't 
   necessary).  A useful side-effect is that since they work with 
   lightweight local locks instead of systemwide locking objects, they 
   aren't vulnerable to security problems where (for example) another 
   process can mess with a globally visible object handle.  This is 
   particularly problematic under Windows, where (for example) CreateMutex()
   can return a handle to an already-existing object of the same name rather
   than a newly-created object (there's no O_EXCL functionality).

   Semaphores are one-shots, so that once set and cleared they can't be
   reset.  This is handled by enforcing the following state transitions:

	Uninited -> Set | Clear
	Set -> Set | Clear
	Clear -> Clear

   The handling is complicated somewhat by the fact that on some systems the
   semaphore has to be explicitly deleted, but only the last thread to use it
   can safely delete it.  In order to handle this, we reference-count the
   semaphore and let the last thread out delete it.  This is handled by
   introducing an additional state preClear, which indicates that while the
   semaphore object is still present, the last thread out should delete it,
   bringing it to the true clear state */

void setSemaphore( IN_ENUM( SEMAPHORE ) const SEMAPHORE_TYPE semaphore,
				   const MUTEX_HANDLE object )
	{
	SEMAPHORE_INFO *semaphoreInfo;

	REQUIRES_V( semaphore > SEMAPHORE_NONE && semaphore < SEMAPHORE_LAST );

	/* It's safe to get a pointer to this outside the lock, we just can't
	   access it yet */
	semaphoreInfo = &krnlData->semaphoreInfo[ semaphore ];

	/* Lock the semaphore table, set the semaphore, and unlock it again */
	MUTEX_LOCK( semaphore );
	if( semaphoreInfo->state == SEMAPHORE_STATE_UNINITED )
		{
		/* The semaphore can only be set if it's currently in the uninited 
		   state.  Since the semaphore handle may be a non-scalar type we
		   have to use a cast to make it non-const (semaphoreInfo->object
		   can be const if it's a pointer but has to be non-const if it's
		   a scalar) */
		*semaphoreInfo = SEMAPHORE_INFO_TEMPLATE;
		semaphoreInfo->state = SEMAPHORE_STATE_SET;
		semaphoreInfo->object = ( MUTEX_HANDLE ) object;
		}
	MUTEX_UNLOCK( semaphore );
	}

void clearSemaphore( IN_ENUM( SEMAPHORE ) const SEMAPHORE_TYPE semaphore )
	{
	SEMAPHORE_INFO *semaphoreInfo;

	REQUIRES_V( semaphore > SEMAPHORE_NONE && semaphore < SEMAPHORE_LAST );

	/* It's safe to get a pointer to this outside the lock, we just can't
	   access it yet */
	semaphoreInfo = &krnlData->semaphoreInfo[ semaphore ];

	/* Lock the semaphore table, clear the semaphore, and unlock it again */
	MUTEX_LOCK( semaphore );
	if( semaphoreInfo->state == SEMAPHORE_STATE_SET )
		{
		/* Precondition: The reference count is valid.  Note that we have to
		   make this an assert() rather than a REQUIRES() because the latter
		   would exit with the semaphore still held */
#if !( defined( __WINCE__ ) && _WIN32_WCE < 400 )
		assert( semaphoreInfo[ semaphore ].refCount >= 0 );
#endif /* Fix for bug in PocketPC 2002 emulator with eVC++ 3.0 */

		/* If there are threads waiting on this semaphore, tell the last
		   thread out to turn out the lights */
		if( semaphoreInfo->refCount > 0 )
			semaphoreInfo->state = SEMAPHORE_STATE_PRECLEAR;
		else
			{
			/* No threads waiting on the semaphore, we can delete it */
			THREAD_CLOSE( semaphoreInfo->object );
			*semaphoreInfo = SEMAPHORE_INFO_TEMPLATE;
			}
		}
	MUTEX_UNLOCK( semaphore );
	}

/* Wait on a semaphore.  This occurs in two phases, first we extract the
   information that we need from the semaphore table, then we unlock it and 
   wait on the semaphore if necessary.  This is necessary because the wait 
   can take an indeterminate amount of time and we don't want to tie up the 
   other semaphores while this occurs.  Note that this type of waiting on 
   local (rather than system) semaphores where possible greatly improves
   performance, in some cases the wait on a signalled system semaphore can
   take several seconds whereas waiting on the local semaphore only takes a
   few ms.  Once the wait has completed, we update the semaphore state as
   per the longer description above */

CHECK_RETVAL_BOOL \
BOOLEAN krnlWaitSemaphore( IN_ENUM( SEMAPHORE ) const SEMAPHORE_TYPE semaphore )
	{
	SEMAPHORE_INFO *semaphoreInfo;
	MUTEX_HANDLE object DUMMY_INIT_MUTEX;
	BOOLEAN semaphoreSet = FALSE;
	int status = CRYPT_OK;

	REQUIRES_B( semaphore > SEMAPHORE_NONE && semaphore < SEMAPHORE_LAST );

	/* If we're in a shutdown and the semaphores have been destroyed, don't
	   try and acquire the semaphore mutex.  In this case anything that 
	   they're protecting should be set to a shutdown state in which any 
	   access fails, so this isn't a problem */
	if( krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MUTEXES )
		return( FALSE );

	/* Lock the semaphore table, extract the information that we need, and 
	   unlock it again */
	semaphoreInfo = &krnlData->semaphoreInfo[ semaphore ];
	MUTEX_LOCK( semaphore );
	if( semaphoreInfo->state == SEMAPHORE_STATE_SET )
		{
		/* Precondition: The reference count is valid.  Note that we have to
		   make this an assert() rather than a REQUIRES() because the latter
		   would exit with the semaphore still held */
		assert( semaphoreInfo->refCount >= 0 );

		/* The semaphore is set and not in use, extract the information we
		   require and mark is as being in use */
		object = semaphoreInfo->object;
		semaphoreInfo->refCount++;
		semaphoreSet = TRUE;
		}
	MUTEX_UNLOCK( semaphore );

	/* If the semaphore wasn't set or is in use, exit now */
	if( !semaphoreSet )
		return( TRUE );

	/* Wait on the object */
	assert( memcmp( &object, &SEMAPHORE_INFO_TEMPLATE.object,
					sizeof( MUTEX_HANDLE ) ) );
	THREAD_WAIT( object, status );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Wait on object failed" ));
		assert( DEBUG_WARN );
		return( FALSE );
		}

	/* Lock the semaphore table, update the information, and unlock it
	   again */
	MUTEX_LOCK( semaphore );
	if( semaphoreInfo->state == SEMAPHORE_STATE_SET || \
		semaphoreInfo->state == SEMAPHORE_STATE_PRECLEAR )
		{
		/* The semaphore is still set, update the reference count */
		semaphoreInfo->refCount--;

		/* Inner precondition: The reference count is valid.  Note that we 
		   have to make this an assert() rather than a REQUIRES() because 
		   the latter would exit with the semaphore still held */
		assert( semaphoreInfo->refCount >= 0 );

		/* If the object owner has signalled that it's done with the object
		   and the reference count has reached zero, we can delete it */
		if( semaphoreInfo->state == SEMAPHORE_STATE_PRECLEAR || \
			semaphoreInfo->refCount <= 0 )
			{
			/* No threads waiting on the semaphore, we can delete it */
			THREAD_CLOSE( object );
			*semaphoreInfo = SEMAPHORE_INFO_TEMPLATE;
			}
		}
	MUTEX_UNLOCK( semaphore );
	
	return( TRUE );
	}

/****************************************************************************
*																			*
*								Mutex Functions								*
*																			*
****************************************************************************/

/* Enter and exit a mutex */

CHECK_RETVAL \
int krnlEnterMutex( IN_ENUM( MUTEX ) const MUTEX_TYPE mutex )
	{
	REQUIRES( mutex > MUTEX_NONE && mutex < MUTEX_LAST );

	/* If we're in a shutdown and the mutexes have been destroyed, don't
	   try and acquire them.  In this case anything that they're protecting
	   should be set to a shutdown state in which any access fails, so this
	   isn't a problem */
	if( krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MUTEXES )
		return( CRYPT_ERROR_PERMISSION );

	switch( mutex )
		{
		case MUTEX_SCOREBOARD:
			MUTEX_LOCK( mutex1 );
			break;

		case MUTEX_SOCKETPOOL:
			MUTEX_LOCK( mutex2 );
			break;

		case MUTEX_RANDOM:
			MUTEX_LOCK( mutex3 );
			break;

		default:
			retIntError();
		}

	return( CRYPT_OK );
	}

void krnlExitMutex( IN_ENUM( MUTEX ) const MUTEX_TYPE mutex )
	{
	REQUIRES_V( mutex > MUTEX_NONE && mutex < MUTEX_LAST );

	/* If we're in a shutdown and the mutexes have been destroyed, don't
	   try and acquire them.  In this case anything that they're protecting
	   should be set to a shutdown state in which any access fails, so this
	   isn't a problem */
	if( krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MUTEXES )
		return;

	switch( mutex )
		{
		case MUTEX_SCOREBOARD:
			MUTEX_UNLOCK( mutex1 );
			break;

		case MUTEX_SOCKETPOOL:
			MUTEX_UNLOCK( mutex2 );
			break;

		case MUTEX_RANDOM:
			MUTEX_UNLOCK( mutex3 );
			break;

		default:
			retIntError_Void();
		}
	}
