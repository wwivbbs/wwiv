/****************************************************************************
*																			*
*						  cryptlib Thread/Mutex Handling  					*
*						Copyright Peter Gutmann 1992-2011					*
*																			*
****************************************************************************/

#ifndef _THREAD_DEFINED

#define _THREAD_DEFINED

/* In multithreaded environments we need to use mutexes to protect the
   information inside cryptlib data structures from access by other threads
   while we use it.  In most cases (mutexes not taken) mutexes are extremely
   quick, being implemented using compare-and-swap on x86 or load/store
   conditional on most RISC CPUs.

   The types and macros that are needed to handle mutexes are:

	MUTEX_HANDLE			-- Handle for mutexes/semaphores

	MUTEX_DECLARE_STORAGE	-- Declare storage for mutex
	MUTEX_CREATE			-- Initialise mutex
	MUTEX_DESTROY			-- Delete mutex
	MUTEX_LOCK				-- Acquire mutex
	MUTEX_UNLOCK			-- Release mutex

   Before deleting a mutex we lock and unlock it again to ensure that if
   some other thread is holding it they'll release it before we delete it.

   Many systems don't provide re-entrant semaphores/mutexes.  To handle this
   we implement our own re-entrant mutexes on top of the OS ones.  Using
   the Posix terminology, what we do is use mutex_trylock(), which doesn't
   re-lock the mutex if it's already locked, and as a side-benefit can be up
   to twice as fast as mutex_lock(), depending on the OS.  This works as
   follows:

	// Try and lock the mutex
	if( mutex_trylock( mutex ) == error )
		{
		// The mutex is already locked, see who owns it
		if( thread_self() != mutex_owner )
			// Someone else has it locked, wait for it to become available
			mutex_lock( mutex );
		else
			// We have it locked, increment its lock count
			mutex_lockcount++;
		}
	mutex_owner = thread_self();

	// ....

	// Decrement the lock count and if we reach zero, unlock the mutex
	if( mutex_lockcount > 0 )
		mutex_lockcount--;
	else
		{
		mutex_owner = none;
		mutex_unlock( mutex );
		}

   This is safe from race conditions via the following:

	if( mutex_trylock( mutex ) == error )
		{										--> Unlock #1
		if( thread_self() != mutex_owner )
												--> Unlock #2
			mutex_lock( mutex );
		else
			mutex_lockcount++;
		}

   If the mutex is unlocked after the trylock at one of the two given 
   points, we can get the following situations:

	Unlock #1, no relock: mutex owner = none -> we lock the mutex
	Unlock #1, relock by another thread: 
			   mutex owner = other -> we wait on the mutex lock

	Unlock #2, no relock: mutex owner = none -> we lock the mutex
	Unlock #2, relock by another thread: 
			   mutex owner = other -> we wait on the mutex lock

   There's no race condition in the lock process since the only thing that 
   could cause a problem is if we unlock the mutex ourselves, which we can't 
   do.

   For the unlock process, the only possibility for a race condition is in
   conjunction with the lock process, if the owner field isn't reset on 
   unlock then thread #1 could unlock the mutex and thread #2 could lock it
   but be pre-empted before it can (re-)set the owner field.  Then thread #1
   could reacquire it thinking that it still owns the mutex because the 
   owner field hasn't been reset yet from its previous ownership.  For this
   reason it's essential that the owner field be reset before the mutex is
   unlocked.

   Note that this implementation of re-entrant mutexes will cause some 
   static source analysers to give false-positive warnings about not 
   unlocking mutexes, since they don't understand the reference-counting 
   mechanism and assume that the mutex is always locked but not always 
   unlocked.

   An additional benefit to performing our own OS object management is that 
   we can avoid OS-specific exposure issues arising from the use of global 
   objects.  For example under Windows if an object with a given name 
   already exists, CreateXYZ() opens the existing object, ignores the 
   security parameters and the object-held flag (!!), and returns a success 
   status.  The only way to tell that we didn't actually get what we asked 
   for is that GetLastError() will return ERROR_ALREADY_EXISTS.  Without 
   this check we could end up thinking we safely hold the object (via the 
   ignored object-held flag) when in fact we don't.  Under Unix it's no 
   better, since a semaphore set (for example) is disconnected from the 
   processes that use it (access requires an on-demand lookup by semid) it's 
   possible for a name-squatting process to create the set before we do, we 
   "create" (= open) it, and then the other process deletes it and recreates 
   it in a different (and totally unexpected) configuration.  To our process 
   the disconnected lookup makes it appear to be the same semaphore set.  By 
   synthesising any higher-level objects that we need from low-level 
   primitives that aren't visible outside our process, we avoid this type 
   of exposure.

   The types and macros that need to be declared to handle threading are:

	THREAD_HANDLE			-- Handle for threads

	THREADFUNC_DEFINE		-- Define thread function

	THREAD_CREATE			-- Create thread
	THREAD_EXIT				-- Exit from thread
	THREAD_INITIALISER		-- Value to initialise thread handle
	THREAD_SELF				-- Get handle of current thread
	THREAD_SAME				-- Compare two thread handles
	THREAD_SLEEP			-- Sleep for n milliseconds
	THREAD_YIELD			-- Yield thread's timeslice
	THREAD_WAIT				-- Wait for thread to terminate
	THREAD_CLOSE			-- Clean up thread after THREAD_WAIT

   Some systems allow a thread/task handle to be used as a synchronisation
   object while others require a separate semaphore object for
   synchronisation.  To handle this we create a synchronisation semaphore in
   the non-signalled state when we create a thread/task, signal it when the
   task exits, and wait on it in the calling thread/task:

	Parent:									Child:

	syncSem = createSem( 1 );
	thread = createThread( syncSem );
											signal( syncSem );
											exit();
	wait( syncSem );
	destroySem( syncSem );

   If the thread/task handle can be used as a synchronisation object, these
   additional operations are turned into no-ops.

   Several of the embedded OSes are extremely difficult to work with because
   their kernels perform no memory (or, often, resource) management of their
   own, assuming that all memory will be allocated by the caller.  In the
   simplest case this means that the thread stack/workspace has to be user-
   allocated, in the worst case every object handle variable that's normally
   a simple scalar value in other OSes is a composite non-scalar type that
   contains all of the object's data, requiring that the caller manually
   allocate state data for threads, mutexes, and semaphores rather than
   having the OS do it for them.

   For things like mutex and semaphore 'handles', which have a fixed number
   or location, this is manageable by statically allocating the storage in
   advance.  However it significantly complicates things like thread
   handling because the thread that dynamically creates a worker thread has
   to be around later on to clean up after it when it terminates, and the
   state data has to be maintained in external (non-thread) storage.  We
   handle this in one of two ways, either by not using cryptlib-internal
   threads (they're only used for initialisation and keygen, neither of
   which will benefit much from the ability to run them in the background in
   an embedded system), or by wrapping the threading functions in our own
   ones which allocate memory as required and access the information via a
   scalar handle.

   To enable the use of thread wrappers, see the xxx_THREAD_WRAPPERS define
   for each embedded OS type.

   For the embedded OSes that require user-allocated stack space, we use
   the following as the stack size.  The 8192 byte storage area provides 
   enough space for the task control block and about half a dozen levels of 
   function nesting (if no large on-stack arrays are used), this should be 
   enough for background init but probably won't be sufficient for the 
   infinitely-recursive OpenSSL bignum code, so the value may need to be 
   adjusted if background keygen is being used */

#define EMBEDDED_STACK_SIZE		8192

/* Define the following to debug mutex lock/unlock operations (only enabled
   under Windows at the moment) */

/* #define MUTEX_DEBUG */

/****************************************************************************
*																			*
*									AMX										*
*																			*
****************************************************************************/

#if defined( __AMX__ )

/* To use resource-management wrappers for the AMX thread functions,
   undefine the following */

/* #define AMX_THREAD_WRAPPERS */

#include <cjzzz.h>

/* Object handles */

#define THREAD_HANDLE			CJ_ID
#define MUTEX_HANDLE			CJ_ID

/* Mutex management functions.  AMX resource semaphores are re-entrant so we
   don't have to jump through the hoops that are necessary with most other
   OSes */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( cjrmcreate( &krnlData->name##Mutex, NULL ) == CJ_EROK ) \
				krnlData->name##MutexInitialised = TRUE; \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			cjrmrsv( krnlData->name##Mutex, threadPriority(), 0 ); \
			cjrmrls( krnlData->name##Mutex ); \
			cjrmdelete( krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		cjrmrsv( krnlData->name##Mutex, x, 0 )
#define MUTEX_UNLOCK( name ) \
		cjrmrls( krnlData->name##Mutex )

/* Thread management functions.  AMX threads require that the user allocate
   the stack space for them, unlike virtually every other embedded OS, which
   make this at most a rarely-used option.  To handle this, we use our own
   wrappers which hide this mess.  A second problem with AMX threads is that
   there's no obvious way to pass an argument to a thread.  In theory we
   could convey the information by sending it via a mailbox, but this
   requires first conveying the mailbox ID to the new task, which has the
   same problem.

   We create the thread with the same priority as the calling thread, AMX
   threads are created in the suspended state so after we create the thread
   we have to trigger it to start it running */

#define THREADFUNC_DEFINE( name, arg )	void name( cyg_addrword_t arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			BYTE *threadStack = clAlloc( EMBEDDED_STACK_SIZE ); \
			\
			cjsmcreate( &syncHandle, NULL, CJ_SMBINARY ); \
			if( cjtkcreate( &threadHandle, NULL, function, threadStack, \
							EMBEDDED_STACK_SIZE, 0, threadPriority(), \
							0 ) != CJ_EROK ) \
				{ \
				free( threadStack ); \
				status = CRYPT_ERROR; \
				} \
			else \
				{ \
				cjtktrigger( threadHandle ); \
				status = CRYPT_OK; \
				} \
			}
#define THREAD_EXIT( sync )		cjsmsignal( sync ); \
								return
#define THREAD_INITIALISER		CJ_IDNULL
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			cjtkid()
#define THREAD_SLEEP( ms )		cjtkdelay( cjtmconvert( ms ) )
#define THREAD_YIELD()			cjtkdelay( 1 )
#define THREAD_WAIT( sync, status )	\
								if( cjsmwait( sync, threadPriority(), 0 ) != CJ_EROK ) \
									status = CRYPT_ERROR; \
								cjsmdelete( sync )
#define THREAD_CLOSE( sync )

/* Because of the problems with resource management of AMX tasks and
   related metadata, we no-op them out unless we're using wrappers by
   ensuring that any attempt to spawn a thread inside cryptlib fails,
   falling back to the non-threaded alternative.  Note that cryptlib itself
   is still thread-safe, it just can't do its init or keygen in an internal
   background thread */

#ifndef AMX_THREAD_WRAPPERS
  #undef THREAD_CREATE
  #undef THREAD_EXIT
  #undef THREAD_CLOSE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
								status = CRYPT_ERROR
  #define THREAD_EXIT( sync )
  #define THREAD_CLOSE( sync )
#endif /* !AMX_THREAD_WRAPPERS */

/* The AMX task-priority function returns the priority via a reference
   parameter.  Because of this we have to provide a wrapper that returns
   it as a return value */

int threadPriority( void );

/****************************************************************************
*																			*
*									BeOS									*
*																			*
****************************************************************************/

#elif defined( __BEOS__ )

#include <kernel/OS.h>

/* Object handles */

#define THREAD_HANDLE			thread_id
#define MUTEX_HANDLE			thread_id

/* Mutex management functions */

#define MUTEX_DECLARE_STORAGE( name ) \
		sem_id name##Mutex; \
		BOOLEAN name##MutexInitialised; \
		thread_id name##MutexOwner; \
		int name##MutexLockcount
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( ( krnlData->name##Mutex = create_sem( 1, NULL ) ) < B_NO_ERROR ) \
				status = CRYPT_ERROR; \
			else \
				{ \
				krnlData->name##MutexLockcount = 0; \
				krnlData->name##MutexInitialised = TRUE; \
				} \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			acquire_sem( krnlData->name##Mutex ); \
			release_sem( krnlData->name##Mutex ); \
			delete_sem( krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		if( acquire_sem_etc( krnlData->name##Mutex, 1, \
							 B_RELATIVE_TIMEOUT, 0 ) == B_WOULD_BLOCK ) \
			{ \
			if( !THREAD_SAME( krnlData->name##MutexOwner, THREAD_SELF() ) ) \
				acquire_sem( krnlData->name##Mutex ); \
			else \
				krnlData->name##MutexLockcount++; \
			} \
		krnlData->name##MutexOwner = THREAD_SELF();
#define MUTEX_UNLOCK( name ) \
		if( krnlData->name##MutexLockcount > 0 ) \
			krnlData->name##MutexLockcount--; \
		else \
			{ \
			krnlData->name##MutexOwner = THREAD_INITIALISER; \
			release_sem( krnlData->name##Mutex ); \
			}

/* Thread management functions.  BeOS threads are created in the suspended
   state, so after we create the thread we have to resume it to start it
   running */

#define THREADFUNC_DEFINE( name, arg )	thread_id name( void *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			threadHandle = syncHandle = \
				spawn_thread( ( function ), NULL, B_NORMAL_PRIORITY, \
							  ( arg ) ); \
			if( threadHandle < B_NO_ERROR ) \
				status = CRYPT_ERROR; \
			else \
				resume_thread( threadHandle ); \
			}
#define THREAD_EXIT( sync )		exit_thread( 0 )
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			find_thread( NULL )
#define THREAD_SLEEP( ms )		snooze( ms )
#define THREAD_YIELD()			snooze( estimate_max_scheduling_latency( -1 ) + 1 )
#define THREAD_WAIT( sync, status ) \
								{ \
								status_t dummy; \
								\
								if( wait_for_thread( sync, &dummy ) != B_NO_ERROR.) \
									status = CRYPT_ERROR; \
								}
#define THREAD_CLOSE( sync )

/****************************************************************************
*																			*
*								ChorusOS									*
*																			*
****************************************************************************/

#elif defined( __CHORUS__ )

/* To use resource-management wrappers for the AMX thread functions,
   undefine the following */

/* #define AMX_THREAD_WRAPPERS */

#include <chorus.h>
#include <exec/chExec.h>

/* Object handles */

#define THREAD_HANDLE			KnThreadLid
#define MUTEX_HANDLE			KnMutex

/* Mutex management functions.  ChorusOS provides no way to destroy a
   mutex once it's initialised, presumably it gets cleaned up when the
   owning actor terminates */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised; \
		KnThreadLid name##MutexOwner; \
		int name##MutexLockcount
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( mutexInit( &krnlData->name##Mutex ) == K_OK ) \
				{ \
				krnlData->name##MutexLockcount = 0; \
				krnlData->name##MutexInitialised = TRUE; \
				} \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			mutexGet( &krnlData->name##Mutex ); \
			mutexRel( &krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		if( mutexTry( &krnlData->name##Mutex ) == 0 ) \
			{ \
			if( !THREAD_SAME( krnlData->name##MutexOwner, THREAD_SELF() ) ) \
				mutexGet( &krnlData->name##Mutex ); \
			else \
				krnlData->name##MutexLockcount++; \
			} \
		krnlData->name##MutexOwner = THREAD_SELF();
#define MUTEX_UNLOCK( name ) \
		if( krnlData->name##MutexLockcount > 0 ) \
			krnlData->name##MutexLockcount--; \
		else \
			{ \
			krnlData->name##MutexOwner = THREAD_INITIALISER; \
			mutexRel( &krnlData->name##Mutex ); \
			}

/* Thread management functions.  ChorusOS threads require that the user
   allocate the stack space for them, unlike virtually every other embedded
   OS, which make this at most a rarely-used option.  To handle this, we use
   our own wrappers which hide this mess.  A second problem with ChorusOS
   threads is that there's no easy way to pass an argument to a thread, so
   we have to include it as a "software register" value that the thread then
   obtains via threadLoadR().

   ChorusOS provides no way to destroy a semaphore once it's initialised,
   presumably it gets cleaned up when the owning actor terminates */

#define THREADFUNC_DEFINE( name, arg )	void name( void )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			BYTE *threadStack = clAlloc( EMBEDDED_STACK_SIZE ); \
			KnDefaultStartInfo startInfo = { \
				K_START_INFO | K_START_INFO_SOFTREG, K_DEFAULT_STACK_SIZE, \
				function, threadStack, K_USERTHREAD, arg }; \
			\
			semInit( &syncHandle, 1 ); \
			if( threadCreate( K_MYACTOR, &threadHandle, K_ACTIVE, NULL, \
							  &startInfo ) != K_OK ) \
				{ \
				free( threadStack ); \
				status = CRYPT_ERROR; \
				} \
			else \
				status = CRYPT_OK; \
			}
#define THREAD_EXIT( sync )		semV( sync ); \
								threadDelete( K_MYACTOR, K_MYSELF )
#define THREAD_INITIALISER		NULL
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			threadSelf()
#define THREAD_SLEEP( ms )		{ \
								KnTimeVal timeVal; \
								\
								K_MILLI_TO_TIMEVAL( &timeVal, ms ); \
								threadDelay( &timeVal ); \
								}
#define THREAD_YIELD()			threadDelay( K_NOBLOCK )
#define THREAD_WAIT( sync, status ) \
								if( semP( sync, K_NOTIMEOUT ) < 0 ) \
									status = CRYPT_ERROR
#define THREAD_CLOSE( sync )

/* Because of the problems with resource management of Chorus thread stack
   space, we no-op out threads unless we're using wrappers by ensuring that
   any attempt to spawn a thread inside cryptlib fails, falling back to the
   non-threaded alternative.  Note that cryptlib itself is still thread-
   safe, it just can't do its init or keygen in an internal background
   thread */

#ifndef CHORUS_THREAD_WRAPPERS
  #undef THREAD_CREATE
  #undef THREAD_EXIT
  #undef THREAD_CLOSE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
								status = CRYPT_ERROR
  #define THREAD_EXIT( sync )
  #define THREAD_CLOSE( sync )
#endif /* !CHORUS_THREAD_WRAPPERS */

/****************************************************************************
*																			*
*									eCOS									*
*																			*
****************************************************************************/

#elif defined( __ECOS__ )

/* To use resource-management wrappers for the eCOS thread functions,
   undefine the following */

/* #define ECOS_THREAD_WRAPPERS */

#include <cyg/hal/hal_arch.h>
#include <cyg/kernel/kapi.h>

/* Object handles */

#define THREAD_HANDLE			cyg_handle_t
#define MUTEX_HANDLE			cyg_sem_t

/* Indicate that eCOS has non-scalar handles */

#define NONSCALAR_HANDLES

/* Mutex management functions */

#define MUTEX_DECLARE_STORAGE( name ) \
		cyg_mutex_t name##Mutex; \
		BOOLEAN name##MutexInitialised; \
		cyg_handle_t name##MutexOwner; \
		int name##MutexLockcount
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK;	/* Apparently never fails */ \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			cyg_mutex_init( &krnlData->name##Mutex ); \
			krnlData->name##MutexLockcount = 0; \
			krnlData->name##MutexInitialised = TRUE; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			cyg_mutex_lock( &krnlData->name##Mutex ); \
			cyg_mutex_unlock( &krnlData->name##Mutex ); \
			cyg_mutex_destroy( &krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		if( !cyg_mutex_trylock( &krnlData->name##Mutex ) ) \
			{ \
			if( !THREAD_SAME( krnlData->name##MutexOwner, THREAD_SELF() ) ) \
				cyg_mutex_lock( &krnlData->name##Mutex ); \
			else \
				krnlData->name##MutexLockcount++; \
			} \
		krnlData->name##MutexOwner = THREAD_SELF();
#define MUTEX_UNLOCK( name ) \
		if( krnlData->name##MutexLockcount > 0 ) \
			krnlData->name##MutexLockcount--; \
		else \
			{ \
			krnlData->name##MutexOwner = THREAD_INITIALISER; \
			cyg_mutex_unlock( &krnlData->name##Mutex ); \
			}

/* Thread management functions.  eCOS threads require that the user allocate
   the stack space for them, unlike virtually every other embedded OS, which
   make this at most a rarely-used option.  To handle this, we use our own
   wrappers, which hide this mess and provide access via a single scalar
   variable.  For synchronisation we use semaphores, eCOS also provides
   condition variables for this purpose but they require a user-managed
   mutex to synchronise access to them, making them (at best) a primitive
   DIY semaphore.

   We create the thread with the same priority as the calling thread, note
   that this precludes the use of the bitmap scheduler (but not the lottery
   scheduler).  There doesn't seem to be any way to tell whether a thread
   has been successfully created/started or not (!!), the best that we can
   do is assume that if the thread handle is zero or negative then there's
   been a problem.  eCOS threads are created in the suspended state, so
   after we create the thread we have to resume it to start it running.

   The CYGNUM_HAL_STACK_SIZE_TYPICAL provides enough stack space for about
   half a dozen levels of function nesting (if no large on-stack arrays are
   used), this should be enough for background init but probably won't be
   sufficient for the infinitely-recursive OpenSSL bignum code, so the value
   may need to be adjusted if background keygen is being used.

   Thread sleep times are measured in implementation-specific ticks rather
   than ms, but the default is 100Hz so we divide by 10 to convert ms to
   ticks */

#define THREADFUNC_DEFINE( name, arg )	void name( cyg_addrword_t arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			BYTE *threadData = clAlloc( sizeof( cyg_thread ) + \
									    CYGNUM_HAL_STACK_SIZE_TYPICAL ); \
			\
			cyg_semaphore_init( &syncHandle, 0 ); \
			cyg_thread_create( cyg_thread_get_priority( cyg_thread_self() ), \
							   function, ( cyg_addrword_t ) arg, NULL, \
							   threadData + sizeof( cyg_thread ), \
							   CYGNUM_HAL_STACK_SIZE_TYPICAL, \
							   &threadHandle, ( cyg_thread * ) threadData ); \
			if( threadHandle <= 0 ) \
				{ \
				free( threadData ); \
				status = CRYPT_ERROR; \
				} \
			else \
				{ \
				cyg_thread_resume( threadHandle ); \
				status = CRYPT_OK; \
				} \
			}
#define THREAD_EXIT( sync )		cyg_semaphore_post( &sync ); \
								cyg_thread_exit()
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			cyg_thread_self()
#define THREAD_SLEEP( ms )		cyg_thread_delay( ( ms ) / 10 )
#define THREAD_YIELD()			cyg_thread_yield()
#define THREAD_WAIT( sync, status ) \
								if( !cyg_semaphore_wait( &sync ) ) \
									status = CRYPT_ERROR; \
								cyg_semaphore_destroy( &sync )
#define THREAD_CLOSE( sync )	cyg_thread_delete( &sync )

/* Because of the problems with resource management of eCOS threads and
   related metadata, we no-op them out unless we're using wrappers by
   ensuring that any attempt to spawn a thread inside cryptlib fails,
   falling back to the non-threaded alternative.  Note that cryptlib itself
   is still thread-safe, it just can't do its init or keygen in an internal
   background thread */

#ifndef ECOS_THREAD_WRAPPERS
  #undef THREAD_CREATE
  #undef THREAD_EXIT
  #undef THREAD_CLOSE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
								status = CRYPT_ERROR
  #define THREAD_EXIT( sync )
  #define THREAD_CLOSE( sync )
#endif /* !ECOS_THREAD_WRAPPERS */

/****************************************************************************
*																			*
*									EmbOS									*
*																			*
****************************************************************************/

#elif defined( __EmbOS__ )

/* To use resource-management wrappers for the EmbOS thread functions,
   undefine the following */

/* #define EMBOS_THREAD_WRAPPERS */

#include <RTOS.h>

/* Object handles */

#define THREAD_HANDLE			OS_TASK *
#define MUTEX_HANDLE			OS_RSEMA

/* Indicate that EmbOS has non-scalar handles */

#define NONSCALAR_HANDLES

/* Mutex management functions.  EmbOS mutexes are reentrant so we don't have 
   to hand-assemble reentrant mutexes as for many other OSes */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK;	/* Apparently never fails */ \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			OS_CREATERSEMA( &krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = TRUE; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			OS_Use( &krnlData->name##Mutex ); \
			OS_Unuse( &krnlData->name##Mutex ); \
			OS_DeleteRSema( &krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		OS_Use( &krnlData->name##Mutex )
#define MUTEX_UNLOCK( name ) \
		OS_Unuse( &krnlData->name##Mutex )

/* Thread management functions.  EmbOS threads require that the user allocate
   the stack space for them, unlike virtually every other embedded OS, which
   make this at most a rarely-used option.  To handle this, we use our own
   wrappers, which hide this mess and provide access via a single scalar
   variable.

   We create the thread with the same priority as the calling thread.  There 
   doesn't seem to be any way to tell whether a thread has been successfully 
   created/started or not (!!), nor is there any way of indicating how large
   the stack is.
   
   The implementation of THREAD_SAME() is somewhat ugly in that since thread
   data storage is handled by the caller rather than being allocated by the
   OS we end up with a mixture of 'struct' (if we're referring to a block of
   storage for the thread data) and 'struct *' (if we're referring to a thread
   handle returned from a function like OS_GetTaskID()).  THREAD_SAME() takes
   advantage of the fact that when it's used, the first argument is always a
   'struct' and the second is always a 'struct *', which is rather ugly but the
   only other way to do it would be to wrap the thread functions with our own
   code that dynamically allocates thread data so that all handles are 
   'struct *'s */

#define THREADFUNC_DEFINE( name, arg )	void name( void *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			BYTE *threadStack = clAlloc( EMBEDDED_STACK_SIZE ); \
			\
			if( threadStack == NULL ) \
				status = CRYPT_ERROR_MEMORY; \
			else \
				{ \
				OS_CREATERSEMA( &syncHandle ); \
				OS_CREATETASK_EX( threadHandle, NULL, function, \
								  OS_GetPriority( NULL ), threadStack, arg ); \
				status = CRYPT_OK; \
				} \
			}
#define THREAD_EXIT( sync )		OS_Unuse( ( OS_RSEMA * ) &sync ); \
								OS_Terminate( OS_GetTaskID() )
#define THREAD_INITIALISER		{ 0 }
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			OS_GetTaskID()
#define THREAD_SLEEP( ms )		OS_Delay( ms )
#define THREAD_YIELD()			OS_Yield()
#define THREAD_WAIT( sync, status ) \
								OS_Use( &sync ); \
								OS_DeleteRSema( &sync ); \
								status = CRYPT_OK
#define THREAD_CLOSE( sync )

/* Because of the problems with resource management of EmbOS threads and
   related metadata, we no-op them out unless we're using wrappers by
   ensuring that any attempt to spawn a thread inside cryptlib fails,
   falling back to the non-threaded alternative.  Note that cryptlib itself
   is still thread-safe, it just can't do its init or keygen in an internal
   background thread */

#ifndef EMBOS_THREAD_WRAPPERS
  #undef THREAD_CREATE
  #undef THREAD_EXIT
  #undef THREAD_CLOSE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
								status = CRYPT_ERROR
  #define THREAD_EXIT( sync )
  #define THREAD_CLOSE( sync )
#endif /* !EMBOS_THREAD_WRAPPERS */

/****************************************************************************
*																			*
*								FreeRTOS/OpenRTOS							*
*																			*
****************************************************************************/

#elif defined( __FreeRTOS__ )

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/* Object handles */

#define THREAD_HANDLE			xTaskHandle 
#define MUTEX_HANDLE			xSemaphoreHandle 

/* Mutex management functions.  Mutexes aren't recursive but binary 
   semaphores are so we use those instead of mutexes.  In addition most of 
   the functions are implemented via macros rather than functions so we use 
   the handles directly instead of passing a reference.  Finally, there's no 
   way to destroy/clean up a mutex after it's been created because the kernel
   and code are linked into a single monolithic blob that's running the 
   entire time, so we don't perform any explicit cleanup */

#define MUTEX_WAIT_TIME		( 5000 / portTICK_RATE_MS )

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			vSemaphoreCreateBinary( krnlData->name##Mutex ); \
			if( krnlData->name##Mutex == NULL ) \
				status = CRYPT_ERROR; \
			else \
				{ \
				krnlData->name##MutexInitialised = TRUE; \
				status = CRYPT_OK; \
				} \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			xSemaphoreTakeRecursive( krnlData->name##Mutex, \
									 MUTEX_WAIT_TIME ); \
			xSemaphoreGiveRecursive( krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		xSemaphoreTakeRecursive( krnlData->name##Mutex, \
								 MUTEX_WAIT_TIME )
#define MUTEX_UNLOCK( name ) \
		xSemaphoreGiveRecursive( krnlData->name##Mutex )

/* Thread management functions.  FreeRTOS threads are generally sensible and
   don't require user management of stack space as with many other 
   minimalist embedded OSes, although they do have the peculiarity that the 
   stack size is given as a "depth" rather than an actual stack size, where 
   the total size is defined by "depth" x "width" (= machine word size) */

#define THREADFUNC_DEFINE( name, arg )	void name( void *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			portBASE_TYPE result; \
			\
			syncHandle = xSemaphoreCreateMutex(); \
			xSemaphoreTake( syncHandle, MUTEX_WAIT_TIME ); \
			result = xTaskCreate( function, "clibTask", EMBEDDED_STACK_SIZE, \
								  arg, tskIDLE_PRIORITY, &threadHandle ); \
			if( result != pdPASS ) \
				status = CRYPT_ERROR; \
			else \
				status = CRYPT_OK; \
			}
#define THREAD_EXIT( sync )		xSemaphoreGive( &sync )
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			xTaskGetCurrentTaskHandle()
#define THREAD_SLEEP( ms )		vTaskDelay( ( ms ) / portTICK_RATE_MS )
#define THREAD_YIELD()			taskYIELD()
#define THREAD_WAIT( sync, status ) \
								if( xSemaphoreTake( sync, MUTEX_WAIT_TIME ) != pdTRUE ) \
									status = CRYPT_ERROR
#define THREAD_CLOSE( sync )	vTaskDelete( &sync )

/****************************************************************************
*																			*
*									IBM 4758								*
*																			*
****************************************************************************/

#elif defined( __IBM4758__ )

#include <cpqlib.h>

/* Object handles */

#define THREAD_HANDLE			long
#define MUTEX_HANDLE			long

/* Mutex management functions */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Semaphore; \
		BOOLEAN name##SemaphoreInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##SemaphoreInitialised ) \
			{ \
			if( CPCreateSerSem( NULL, 0, 0, \
								&krnlData->name##Semaphore ) == 0 ) \
				krnlData->name##SemaphoreInitialised = TRUE; \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##SemaphoreInitialised ) \
			{ \
			CPSemClaim( krnlData->name##Semaphore, SVCWAITFOREVER ); \
			CPSemRelease( krnlData->name##Semaphore ); \
			CPDelete( krnlData->name##Semaphore, 0 ); \
			krnlData->name##SemaphoreInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		CPSemClaim( krnlData->name##Semaphore, SVCWAITFOREVER )
#define MUTEX_UNLOCK( name ) \
		CPSemRelease( krnlData->name##Semaphore )

/* Thread management functions.  CP/Q doesn't use threads but only supports
   CP/Q tasks.  These function in a somewhat peculiar manner, so this
   facility isn't currently used */

/****************************************************************************
*																			*
*									uITRON									*
*																			*
****************************************************************************/

#elif defined( __ITRON__ )

/* In the following includes, kernel.h is the uITRON kernel.h, not the
   cryptlib one */

#include <itron.h>
#include <kernel.h>

/* Object handles */

#define THREAD_HANDLE			ID
#define MUTEX_HANDLE			ID

/* Mutex management functions.  We could use either semaphores or mutexes
   for this, semaphores are supported under uITRON 3.0 but since we're
   using automatic assignment of handles (which requires uITRON 4.0) we may
   as well use mutexes */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised; \
		ID name##MutexOwner; \
		int name##MutexLockcount
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			static const T_CMTX pk_cmtx = { TA_TFIFO, 0, 0 }; \
			\
			if( ( krnlData->name##Mutex = \
						acre_mtx( ( T_CMTX  * ) &pk_cmtx ) ) < E_OK ) \
				status = CRYPT_ERROR; \
			else \
				{ \
				krnlData->name##MutexLockcount = 0; \
				krnlData->name##MutexInitialised = TRUE; \
				} \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			loc_mtx( krnlData->name##Mutex ); \
			unl_mtx( krnlData->name##Mutex ); \
			del_mtx( krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		if( ploc_mtx( krnlData->name##Mutex ) == E_ILUSE ) \
			{ \
			if( !THREAD_SAME( krnlData->name##MutexOwner, THREAD_SELF() ) ) \
				loc_mtx( krnlData->name##Mutex ); \
			else \
				krnlData->name##MutexLockcount++; \
			} \
		krnlData->name##MutexOwner = threadSelf();
#define MUTEX_UNLOCK( name ) \
		if( krnlData->name##MutexLockcount > 0 ) \
			krnlData->name##MutexLockcount--; \
		else \
			{ \
			krnlData->name##MutexOwner = THREAD_INITIALISER; \
			unl_mtx( krnlData->name##Mutex ); \
			}

/* Thread management functions.  The attributes for task creation are:

	TA_HLNG | TA_ACT	-- C interface, create task in the active rather
						   than suspended state (otherwise we'd have to use
						   act_tsk() to activate it a la BeOS).
	arg					-- Task extended info.
	function			-- Task function.
	TPRI_SELF			-- Same priority as invoking task.
	EMBEDDED_STACK_SIZE	-- Stack size.
	NULL				-- Auto-allocate stack.  This is given as 0 rather
						   than NULL since some uITRON headers define their
						   own NULL as 0, leading to compiler warnings.

   uITRON status values are 8:8 bit pairs with the actual status in the
   low 8 bits.  The sub-values can be extracted with the MERCD() and SERCD()
   (main- and sub-error-code) macros, however simply using the MERCD()
   result isn't safe because it could be the (negative) low 8 bits of a
   (positive overall) return value.  When creating a task we therefore
   consider a status < E_OK as being an error, without trying to pick apart
   the overall value.

   The handling of initialisers is a bit dodgy since TSK_NONE == TSK_SELF
   (== 0) and it isn't even safe to use negative values since in some cases
   these can be valid system task handles.  In general however uITRON
   numbers IDs from 1...n, so using 0 as a non-value is safe.

   Handling of task sleep is also somewhat dodgy, time is measured in clock
   ticks of an implementation-specific duration, the best that we can do is
   to assume that it's close enough to ms.

   In theory we don't really need to use exd_tsk() since returning from a
   task ends it, but we make it explicit to be neat */

#define THREADFUNC_DEFINE( name, arg )	void name( VP_INT *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			static const T_CSEM pk_csem = { TA_TFIFO, 1, 64 }; \
			T_CTSK pk_ctsk = { TA_HLNG | TA_ACT, ( arg ), ( function ), \
							   TPRI_SELF, EMBEDDED_STACK_SIZE, 0 }; \
			\
			syncHandle = acre_sem( ( T_CSEM  * ) &pk_csem ); \
			threadHandle = acre_tsk( &pk_ctsk ); \
			if( threadHandle < E_OK ) \
				{ \
				del_sem( syncHandle ); \
				status = CRYPT_ERROR; \
				} \
			else \
				status = CRYPT_OK; \
			}
#define THREAD_EXIT( sync )		sig_sem( sync ); \
								exd_tsk()
#define THREAD_INITIALISER		TSK_NONE
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			threadSelf()
#define THREAD_SLEEP( ms )		dly_tsk( ms )
#define THREAD_YIELD()			dly_tsk( 0 )
#define THREAD_WAIT( sync, status ) \
								if( wai_sem( sync ) != E_OK ) \
									status = CRYPT_ERROR; \
								del_sem( sync )
#define THREAD_CLOSE( sync )

/* The uITRON thread-self function returns the thread ID via a reference
   parameter since uITRON IDs can be negative and there'd be no way to
   differentiate a thread ID from an error code.  Because of this we have
   to provide a wrapper that returns it as a return value */

ID threadSelf( void );

/****************************************************************************
*																			*
*									MQX										*
*																			*
****************************************************************************/

#elif defined( __MQX__ )

#include <mqx.h>

/* Object handles */

#define THREAD_HANDLE			_task_id
#define MUTEX_HANDLE			MUTEX_STRUCT

/* Mutex management functions.  It's unclear whether MQX mutexes are 
   reentrant or not, if they aren't then the following macros would need
   to be changed to hand-assemble reentrant mutexes as is done for many 
   other OSes */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( _mutex_init( &krnlData->name##Mutex, NULL ) == MQX_OK ) \
				krnlData->name##MutexInitialised = TRUE; \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			_mutex_lock( &krnlData->name##Mutex ); \
			_mutex_unlock( &krnlData->name##Mutex ); \
			_mutex_destroy( &krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		_mutex_lock( &krnlData->name##Mutex )
#define MUTEX_UNLOCK( name ) \
		_mutex_unlock( &krnlData->name##Mutex )

/* Thread management functions.  MQX's task creation is weird in that it 
   doesn't take task parameters like any other OS but requres an integer 
   that identifies an entry in a list of task templates that was specified at 
   compile time.  This is weird in that we can't simply start an arbitrary 
   task by specifying its function name but have to refer to it indirectly.
   Fortunately the only time this capability is used is when we're performing
   an async init, if it were used in other locations then it'd be necessary
   to have THREAD_CREATE compare the 'function' value passed to it with a
   built-in list and map that to an integer ID for _task_create(), which then 
   maps it back to a function name.

   To deal with this strangeness you need to define an entry in your app's
   TASK_TEMPLATE_STRUCT list something like the following:

	{ 0x100, threadedBind, EMBEDDED_STACK_SIZE, 15, NULL, 0, 0, 0 }

   which creates a non-auto-start task with FIFO scheduling and a default 
   timeslice.  Unfortunately because of this bizarre way of specifying things
   we can't set the priority in any sane way, normally we'd use  
   _task_get_priority() with MQX_NULL_TASK_ID to get the current task's 
   priority but this doesn't work because the value has to be allocated at 
   compile time.  We give the task a mid-range priority value of 15 (from a 
   range of 0 = highest ... 31 = lowest), this is somewhat ugly but there
   doesn't seem to be much else that we can do */

#define THREADFUNC_DEFINE( name, arg )	void name( uint32 /* = void* */ arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			_lwsem_create( &syncHandle, 1 ); \
			threadHandle = _task_create( 0, 0x100, ( uint32 ) arg ); \
			if( threadHandle == MQX_NULL_TASK_ID ) \
				status = CRYPT_ERROR; \
			else \
				status = CRYPT_OK; \
			}
#define THREAD_EXIT( sync )		_lwsem_post( &sync ); \
								_task_destroy( MQX_NULL_TASK_ID )
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			_task_get_id()
#define THREAD_SLEEP( ms )		_time_delay( ms )
#define THREAD_YIELD()			_sched_yield()
#define THREAD_WAIT( sync, status ) \
								if( _lwsem_wait( &sync ) != MQX_OK ) \
									status = CRYPT_ERROR; \
								_lwsem_destroy( &sync )
#define THREAD_CLOSE( sync )

/****************************************************************************
*																			*
*									Nucleus									*
*																			*
****************************************************************************/

#elif defined( __Nucleus__ )

#include <nucleus.h>

/* Object handles */

#define THREAD_HANDLE			NU_TASK *
#define MUTEX_HANDLE			NU_SEMAPHORE *

/* Indicate that Nucleus has non-scalar handles */

#define NONSCALAR_HANDLES

/* Mutex management functions.  Nucleus doesn't have a mutex_trylock()-
   equivalent so we have to perform the check by comparing the mutex
   owner to the current task, which in turns means we have to explicitly
   manage the mutex owner value at a level beyond what's necessary with
   other OSes */

#define MUTEX_DECLARE_STORAGE( name ) \
		NU_SEMAPHORE name##Mutex; \
		BOOLEAN name##MutexInitialised; \
		NU_TASK *name##MutexOwner; \
		int name##MutexLockcount
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( NU_Create_Semaphore( &krnlData->name##Mutex, "CLIBSEMA", 1, \
									 NU_PRIORITY ) == NU_SUCCESS ) \
				{ \
				krnlData->name##MutexOwner = NULL; \
				krnlData->name##MutexLockcount = 0; \
				krnlData->name##MutexInitialised = TRUE; \
				} \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			NU_Delete_Semaphore( &krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		if( krnlData->name##MutexOwner == NU_Current_Task_Pointer() ) \
			krnlData->name##MutexLockcount++; \
		else \
			{ \
			NU_Obtain_Semaphore( &krnlData->name##Mutex, NU_SUSPEND ); \
			krnlData->name##MutexOwner = NU_Current_Task_Pointer(); \
			}
#define MUTEX_UNLOCK( name ) \
		if( krnlData->name##MutexLockcount > 0 ) \
			krnlData->name##MutexLockcount--; \
		else \
			{ \
			krnlData->name##MutexOwner = NULL; \
			NU_Release_Semaphore( &krnlData->name##Mutex ); \
			}

/* Thread management functions.  The attributes for task creation are:

	"CLIBTASK"			-- Task label.
	function			-- Task function.
	argc, argv			-- Task arguments
	stackSpace			-- Stack storage.
	EMBEDDED_STACK_SIZE	-- Stack size.
	200					-- Task priority 0...255.
	15					-- Maximum task timeslice in ticks.
	NU_PREEMPT			-- Task is preemptible.
	NU_START			-- Auto-start the task.

   Thread sleep times are measured in implementation-specific ticks rather
   than ms, but the default is 100Hz so we divide by 10 to convert ms to
   ticks */

#define THREADFUNC_DEFINE( name, arg )	void name( UNSIGNED argc, VOID *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			BYTE *stackSpace = clAlloc( EMBEDDED_STACK_SIZE ); \
			STATUS nu_status; \
			\
			nu_status = NU_Create_Task( &threadHandle, "CLIBTASK", function, \
										1, arg, stackSpace, EMBEDDED_STACK_SIZE, \
										200, 15, NU_PREEMPT, NU_START ); \
			if( status != NU_SUCCESS ) \
				status = CRYPT_ERROR; \
			else \
				status = CRYPT_OK; \
			}
#define THREAD_EXIT( sync )		NU_Release_Semaphore( &sync ); \
								NU_Terminate_Task( NU_Current_Task_Pointer() )
#define THREAD_INITIALISER		{ 0 }
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			NU_Current_Task_Pointer()
#define THREAD_SLEEP( ms )		NU_Sleep( ms / 10 )
#define THREAD_YIELD()			NU_Relinquish()
#define THREAD_WAIT( sync, status ) \
								if( NU_Obtain_Semaphore( sync, \
														 NU_SUSPEND ) != NU_SUCCESS ) \
									status = CRYPT_ERROR; \
								NU_Delete_Semaphore( sync )
#define THREAD_CLOSE( sync )	NU_Delete_Task( sync )

/* Because of the problems with resource management of Nucleus threads, we 
   no-op them out unless we're using wrappers by ensuring that any attempt 
   to spawn a threads inside cryptlib fails, falling back to the non-
   threaded alternative.  Note that cryptlib itself is still thread-safe, it 
   just can't do its init or keygen in an internal background thread */

#ifndef NUCLEUS_THREAD_WRAPPERS
  #undef THREAD_CREATE
  #undef THREAD_EXIT
  #undef THREAD_CLOSE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
								status = CRYPT_ERROR
  #define THREAD_EXIT( sync )
  #define THREAD_CLOSE( sync )
#endif /* !NUCLEUS_THREAD_WRAPPERS */

/****************************************************************************
*																			*
*									OS/2									*
*																			*
****************************************************************************/

#elif defined( __OS2__ )

#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#define INCL_DOSDATETIME
#define INCL_DOSPROCESS
#define INCL_WINWINDOWMGR
#define INCL_WINSYS
#include <os2.h>
ULONG DosGetThreadID( void );

/* Object handles */

#define THREAD_HANDLE			TID
#define MUTEX_HANDLE			HEV

/* Mutex management functions */

#define MUTEX_DECLARE_STORAGE( name ) \
		HMTX name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( DosCreateMutexSem( NULL, &krnlData->name##Mutex, 0L, \
								   FALSE ) == NO_ERROR ) \
				krnlData->name##MutexInitialised = TRUE; \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			DosRequestMutexSem( krnlData->name##Mutex, \
								( ULONG ) SEM_INDEFINITE_WAIT ); \
			DosReleaseMutexSem( krnlData->name##Mutex ); \
			DosCloseMutexSem( krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		DosRequestMutexSem( krnlData->name##Mutex, \
							( ULONG ) SEM_INDEFINITE_WAIT )
#define MUTEX_UNLOCK( name ) \
		DosReleaseMutexSem( krnlData->name##Mutex )

/* Thread management functions */

#define THREADFUNC_DEFINE( name, arg )	void _Optlink name( void *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			threadHandle = syncHandle = \
				_beginthread( ( function ), NULL, 8192, ( arg ) ); \
			status = ( threadHandle == -1 ) ? CRYPT_ERROR : CRYPT_OK ); \
			}
#define THREAD_EXIT( sync )		_endthread()
#define THREAD_INITIALISER		0
#define THREAD_SELF()			DosGetThreadID()
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SLEEP( ms )		DosWait( ms )
#define THREAD_YIELD()			DosWait( 0 )
#define THREAD_WAIT( sync, status ) \
								if( DosWaitThread( sync, INFINITE ) != NO_ERROR ) \
									status = CRYPT_ERROR
#define THREAD_CLOSE( sync )

/****************************************************************************
*																			*
*									PalmOS									*
*																			*
****************************************************************************/

#elif defined( __PALMOS__ )

#include <CmnErrors.h>
#include <SysThread.h>

/* Object handles */

#define THREAD_HANDLE			SysHandle
#define MUTEX_HANDLE			SysHandle

/* Mutex management functions.  These are just initialised in a slightly
   odd manner, there isn't any function to explicitly initialise them but
   instead they're statically initialised to a fixed value (NULL), when
   the lock/unlock functions are passed this value they perform the
   initialisation on-demand.  This means that if the underlying hardware
   supports it they can be implemented using atomic operations directly
   on the critical-section value without having to allocate memory for
   a struct to contain the critical-section data */

#define MUTEX_DECLARE_STORAGE( name ) \
		SysCriticalSectionType name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			krnlData->name##Mutex = sysCriticalSectionInitializer; \
			krnlData->name##MutexInitialised = TRUE; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			SysCriticalSectionEnter( &krnlData->name##Mutex ); \
			SysCriticalSectionExit( &krnlData->name##Mutex ); \
			krnlData->name##Mutex = sysCriticalSectionInitializer; \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		SysCriticalSectionEnter( &krnlData->name##Mutex )
#define MUTEX_UNLOCK( name ) \
		SysCriticalSectionExit( &krnlData->name##Mutex )

/* Thread management functions.  PalmOS threads are created in the suspended
   state, so after we create the thread we have to explicitly start it to
   get it running.  The default stack size (via SysThreadCreateEZ()) is a
   pathetic 4K for standard threads or 8K for UI threads, to avoid this we
   have to use the full SysThreadCreate() and specify our own stack size */

#define THREADFUNC_DEFINE( name, arg )	void name( void *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			SysSemaphoreCreateEZ( 0, ( SysHandle * ) &syncHandle ); \
			if( SysThreadCreate( sysThreadNoGroup, "", \
								 sysThreadPriorityNormal, 32768, function, \
								 arg, &threadHandle ) != errNone ) \
				{ \
				SysSemaphoreDestroy( syncHandle ); \
				status = CRYPT_ERROR; \
				} \
			else \
				{ \
				SysThreadStart( threadHandle ); \
				status = CRYPT_OK; \
				} \
			}
#define THREAD_EXIT( sync )		SysSemaphoreSignal( sync ); \
								SysThreadExit()
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			SysCurrentThread()
#define THREAD_SLEEP( ms )		SysThreadDelay( ( ms ) * 1000000L, P_ABSOLUTE_TIMEOUT )
#define THREAD_YIELD()			SysThreadDelay( 0, P_POLL )
#define THREAD_WAIT( sync, status ) \
								if( SysSemaphoreWait( sync, P_WAIT_FOREVER, 0 ) != errNone ) \
									status = CRYPT_ERROR; \
								SysSemaphoreDestroy( sync )
#define THREAD_CLOSE( sync )

/****************************************************************************
*																			*
*									RTEMS									*
*																			*
****************************************************************************/

#elif defined( __RTEMS__ )

#include <rtems.h>

/* Object handles.  These are actually multi-component object IDs, but they
   act like standard handles */

#define THREAD_HANDLE			rtems_id
#define MUTEX_HANDLE			rtems_id

/* RTEMS objects have names, which aren't really names but 32-bit integer
   tags (the term 'name' comes from the fact that they can be initialised
   to four ASCII chars like original Mac resource IDs).  Since we don't
   really care about names, we pass in a dummy name value for all of our
   calls */

#define NO_NAME					0

/* Mutex management functions.  RTEMS semaphores (or at least standard
   counting semaphores, which is what we're using here) are re-entrant so
   we don't have to jump through the hoops that are necessary with most
   other OSes.

   We specify the priority ceiling as zero since it's not used for the
   semaphore type that we're creating */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( rtems_semaphore_create( NO_NAME, 1, RTEMS_DEFAULT_ATTRIBUTES, 0, \
										&krnlData->name##Mutex ) == RTEMS_SUCCESSFUL ) \
				krnlData->name##MutexInitialised = TRUE; \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			rtems_semaphore_obtain( krnlData->name##Mutex, RTEMS_WAIT, 0 ); \
			rtems_semaphore_release( krnlData->name##Mutex ); \
			rtems_semaphore_delete( krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		rtems_semaphore_obtain( krnlData->name##Mutex, RTEMS_WAIT, 0 );
#define MUTEX_UNLOCK( name ) \
		rtems_semaphore_release( krnlData->name##Mutex );

/* Thread management functions.  RTEMS tasks are created in the suspended
   state, so after we create the task we have to resume it to start it
   running.  The attributes for task creation are:

	NO_NAME					-- Task name.
	RTEMS_CURRENT_PRIORITY	-- Task priority.  The documentation is unclear
							   as to whether we can specify this directly as
							   the priority or have to obtain it via a call,
							   we use the call to be safe.
	RTEMS_STACK_SIZE		-- Task stack size.  We use the default size for
							   RTEMS tasks.
	RTEMS_ASR | \			-- Task mode: Enable async signal processing
		RTEMS_INT_LEVEL(0) | \ (default), all interrupts enabled (default),
		RTEMS_PREEMPT | \	   preemptive scheduling (default), timeslicing
		RTEMS_TIMESLICE		   for tasks of the same priority.
	RTEMS_DEFAULT_ATTRIBUTES-- Task attributes: Local task, no FP regs.

   Specifying the default values for the task mode is optional, but we do it
   anyway to make the behaviour explicit.

   We could make the synchronisation semaphore a binary semaphore, but
   there's no indication that this is any more efficient than a counting
   semaphore, and it saves having to create a long list of (non-default)
   attributes to specify this.

   Task sleep times are measured in implementation-specific ticks rather
   than ms, but the default is 10ms so we divide by 10.  If necessary the
   absolute value can be calculated from the microseconds_per_tick field in
   the RTEMS configuration table or from CONFIGURE_MICROSECONDS_PER_TICK in
   confdefs.h */

#define TASK_MODE	( RTEMS_ASR | RTEMS_INTERRUPT_LEVEL(0) | \
					  RTEMS_PREEMPT | RTEMS_TIMESLICE )

#define THREADFUNC_DEFINE( name, arg )	rtems_task name( rtems_task_argument arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			rtems_status_code rtemsStatus; \
			rtems_task_priority rtemsPriority; \
			\
			rtems_task_set_priority( RTEMS_SELF, RTEMS_CURRENT_PRIORITY, \
									 &rtemsPriority ); \
			rtems_semaphore_create( NO_NAME, 1, RTEMS_DEFAULT_ATTRIBUTES, 0, \
									&syncHandle ); \
			rtemsStatus = rtems_task_create( NO_NAME, rtemsPriority, \
											 RTEMS_STACK_SIZE, TASK_MODE, \
											 RTEMS_DEFAULT_ATTRIBUTES, \
											 &threadHandle ); \
			if( rtemsStatus == RTEMS_SUCCESSFUL ) \
				rtemsStatus = rtems_task_start( threadHandle, function, \
												( rtems_task_argument ) arg ); \
			if( rtemsStatus == RTEMS_SUCCESSFUL ) \
				status = CRYPT_OK; \
			else \
				{ \
				rtems_semaphore_delete( syncHandle ); \
				status = CRYPT_ERROR; \
				} \
			}
#define THREAD_EXIT( sync )		rtems_semaphore_release( sync ); \
								rtems_task_delete( RTEMS_SELF );
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			threadSelf()
#define THREAD_SLEEP( ms )		rtems_task_wake_after( ( ms ) / 10 )
#define THREAD_YIELD()			rtems_task_wake_after( RTEMS_YIELD_PROCESSOR )
#define THREAD_WAIT( sync, status ) \
								if( rtems_semaphore_obtain( sync, RTEMS_WAIT, 0 ) != RTEMS_SUCCESSFUL ) \
									status = CRYPT_ERROR; \
								else \
									rtems_semaphore_release( sync ); \
								rtems_semaphore_delete( sync )
#define THREAD_CLOSE( sync )

/* The RTEMS thread-self function returns the task ID via a reference
   parameter, because of this we have to provide a wrapper that returns it
   as a return value */

rtems_id threadSelf( void );

/****************************************************************************
*																			*
*									ThreadX									*
*																			*
****************************************************************************/

#elif defined( __ThreadX__ )

/* To use resource-management wrappers for the ThreadX thread functions,
   undefine the following */

/* #define THREADX_THREAD_WRAPPERS */

//#include <tx_api.h>
#pragma message( "ThreadX header path faked" )
#include <threadx_tx_api.h>

/* Object handles */

#define THREAD_HANDLE			TX_THREAD *
#define MUTEX_HANDLE			TX_MUTEX

/* Indicate that ThreadX has non-scalar handles */

#define NONSCALAR_HANDLES

/* Mutex management functions.  ThreadX mutexes are reentrant so we don't 
   have to hand-assemble reentrant mutexes as for many other OSes.  All 
   ThreadX objects have names (presumably for debugging) but what the 
   requirements for these are aren't documented so we just use a dummy name 
   for everything.  Since we don't care whether the threads waiting on the 
   mutex get woken up in priority order, we use TX_NO_INHERIT to ignore 
   priority inheritance */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( tx_mutex_create( &krnlData->name##Mutex, "name", \
								 TX_NO_INHERIT ) == TX_SUCCESS ) \
				krnlData->name##MutexInitialised = TRUE; \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			tx_mutex_get( &krnlData->name##Mutex, TX_WAIT_FOREVER ); \
			tx_mutex_put( &krnlData->name##Mutex ); \
			tx_mutex_delete( &krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		tx_mutex_get( &krnlData->name##Mutex, TX_WAIT_FOREVER )
#define MUTEX_UNLOCK( name ) \
		tx_mutex_put( &krnlData->name##Mutex )

/* Thread management functions.  ThreadX threads require that the user 
   allocate the stack space for them, unlike virtually every other embedded 
   OS, which make this at most a rarely-used option.  To handle this, we use 
   our own wrappers.

   The thread function's argument is a ULONG, since we use this as a pointer
   to parameter data we declare it as a 'void *' since that's how it's
   treated in practice.

   We give the thread a mid-range priority value and preemption threshold of
   15 (from a range of 0 = highest ... 31 = lowest) and a 50-tick timeslice.
   The timeslice value is HAL-dependent so it's not really possible to tell 
   how much runtime this will actually give the thread, anyone using cryptlib
   with ThreadX will need to set an appropriate value for their HAL.  The
   same goes for the sleep time, the code assumes that 1 tick = 10ms so we
   divide by 10 to convert ms to ticks */

#define THREADFUNC_DEFINE( name, arg )	VOID name( void * /*ULONG*/ arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			BYTE *threadStack = clAlloc( EMBEDDED_STACK_SIZE ); \
			\
			tx_mutex_create( &syncHandle, "name", TX_NO_INHERIT ); \
			if( tx_thread_create( &threadHandle, "name", function, \
								  ( ULONG ) arg, threadStack, EMBEDDED_STACK_SIZE, \
								  15, 50, 15, TX_AUTO_START ) != TX_SUCCESS ) \
				{ \
				free( threadStack ); \
				status = CRYPT_ERROR; \
				} \
			else \
				status = CRYPT_OK; \
			}
#define THREAD_EXIT( sync )		tx_mutex_put( &sync ); \
								tx_thread_terminate( tx_thread_identify() )
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			tx_thread_identify()
#define THREAD_SLEEP( ms )		tx_thread_sleep( ( ms ) / 10 )
#define THREAD_YIELD()			tx_thread_relinquish()
#define THREAD_WAIT( sync, status ) \
								if( !tx_mutex_get( &sync, TX_WAIT_FOREVER ) ) \
									status = CRYPT_ERROR; \
								tx_mutex_delete( &sync )
#define THREAD_CLOSE( sync )	tx_thread_delete( &sync )

/* Because of the problems with resource management of ThreadX threads and
   related metadata we no-op them out unless we're using wrappers by 
   ensuring that any attempt to spawn a thread inside cryptlib fails,
   falling back to the non-threaded alternative.  Note that cryptlib itself
   is still thread-safe, it just can't do its init or keygen in an internal
   background thread */

#ifndef THREADX_THREAD_WRAPPERS
  #undef THREAD_CREATE
  #undef THREAD_EXIT
  #undef THREAD_CLOSE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
								status = CRYPT_ERROR
  #define THREAD_EXIT( sync )
  #define THREAD_CLOSE( sync )
#endif /* !THREADX_THREAD_WRAPPERS */

/****************************************************************************
*																			*
*									T-Kernel								*
*																			*
****************************************************************************/

#elif defined( __TKernel__ )

#include <tkernel.h>

/* Object handles */

#define THREAD_HANDLE			ID
#define MUTEX_HANDLE			ID

/* Mutex management functions.  We could use either semaphores or mutexes
   for this, semaphores are supported under uITRON 3.0 but since we're
   using automatic assignment of handles (which requires uITRON 4.0) we may
   as well use mutexes */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised; \
		ID name##MutexOwner; \
		int name##MutexLockcount
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			static const T_CMTX pk_cmtx = { NULL, TA_TFIFO, 0 }; \
			\
			if( ( krnlData->name##Mutex = \
						tk_cre_mtx( &pk_cmtx ) ) < E_OK ) \
				status = CRYPT_ERROR; \
			else \
				{ \
				krnlData->name##MutexLockcount = 0; \
				krnlData->name##MutexInitialised = TRUE; \
				} \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			tk_loc_mtx( krnlData->name##Mutex, TMO_FEVR ); \
			tk_unl_mtx( krnlData->name##Mutex ); \
			tk_del_mtx( krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		if( tk_loc_mtx( krnlData->name##Mutex, TMO_POL ) == E_TMOUT ) \
			{ \
			if( !THREAD_SAME( krnlData->name##MutexOwner, THREAD_SELF() ) ) \
				tk_loc_mtx( krnlData->name##Mutex, TMO_FEVR ); \
			else \
				krnlData->name##MutexLockcount++; \
			} \
		krnlData->name##MutexOwner = THREAD_SELF();
#define MUTEX_UNLOCK( name ) \
		if( krnlData->name##MutexLockcount > 0 ) \
			krnlData->name##MutexLockcount--; \
		else \
			{ \
			krnlData->name##MutexOwner = THREAD_INITIALISER; \
			tk_unl_mtx( krnlData->name##Mutex ); \
			}

/* Thread management functions.  The attributes for task creation are:

	arg					-- Task argument.
	TA_HLNG				-- C interface.
	function			-- Task function.
	TPRI_SELF			-- Same priority as invoking task.
	EMBEDDED_STACK_SIZE	-- User stack size.
	Other				-- Various parameters left at their default 
						   settings.

   The handling of initialisers is a bit dodgy since TSK_SELF == 0 and it 
   isn't even safe to use negative values since in some cases these can be 
   valid system task handles.  In general however T-Kernel numbers IDs from 
   1...n, so using 0 as a non-value is safe.

   Dealing with the task's priority is a bit complicated since T-Kernel 
   provides no way of determining a task's current priority, for now we
   use TPRI_INI = the priority of the task when it was started, if this 
   isn't appropriate then it'd be necessary to call td_ref_tsk() and read
   the tskpri field from the rtsk data, however this may be just as bad 
   because it returns the priority at read time which may differ from the
   usual priority.

   T-Kernel status values are 8:8 bit pairs with the actual status in the
   low 8 bits.  The sub-values can be extracted with the MERCD() and SERCD()
   (main- and sub-error-code) macros, however simply using the MERCD()
   result isn't safe because it could be the (negative) low 8 bits of a
   (positive overall) return value.  When creating a task we therefore
   consider a status < E_OK as being an error, without trying to pick apart
   the overall value */

#define THREADFUNC_DEFINE( name, arg )	void name( INT stacd, VP arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			static const T_CSEM pk_csem = { NULL, TA_TFIFO, 1, 64 }; \
			T_CTSK pk_ctsk = { ( arg ), TA_HLNG, ( function ), \
							   TPRI_INI, EMBEDDED_STACK_SIZE, 0, 0, 0, 0, 0 }; \
			\
			syncHandle = tk_cre_sem( &pk_csem ); \
			threadHandle = tk_cre_tsk( &pk_ctsk ); \
			if( threadHandle < E_OK ) \
				{ \
				tk_del_sem( syncHandle ); \
				status = CRYPT_ERROR; \
				} \
			else \
				{ \
				tk_sta_tsk( threadHandle, 0 ); \
				status = CRYPT_OK; \
				} \
			}
#define THREAD_EXIT( sync )		tk_sig_sem( sync, 1 ); \
								tk_exd_tsk()
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			tk_get_tid()
#define THREAD_SLEEP( ms )		tk_slp_tsk( ms )
#define THREAD_YIELD()			tk_slp_tsk( 0 )
#define THREAD_WAIT( sync, status ) \
								if( tk_wai_sem( sync, 1, TMO_FEVR ) != E_OK ) \
									status = CRYPT_ERROR; \
								tk_del_sem( sync )
#define THREAD_CLOSE( sync )

/****************************************************************************
*																			*
*									uC/OS-II								*
*																			*
****************************************************************************/

#elif defined( __UCOS__ )

/* uC/OS-II has a pure priority-based scheduler (no round-robin scheduling)
   and makes a task's priority do double duty as the task ID, which means
   that it's unlikely it'll ever get round-robin scheduling without a
   major overhaul of the API.  Because of this, a background task started
   inside cryptlib for initialisation or keygen will either never run or
   always run depending on the priority it's started with, thus making it
   equivalent to performing the operation synchronously.  This means that
   there's no point in using cryptlib-internal tasks, so they're disabled
   unless the following is commented out.  Note that cryptlib is still
   thread-(task)-safe, it just won't use internal tasks for asynchronous
   ops, because uC/OS-II's scheduling will make the synchronous */

/* #define UCOS_USE_TASKS */

/* Most systems handle priority-inversion-avoidance automatically, however
   for some reason in uC/OS-II this has to be managed manually by the user.
   This is done by specifying the priority-inherit priority level that a
   low-priority task is raised to when a high-priority task attempts to
   acquire a mutex that the low-priority task is currently holding.  This
   has to be higher than the priority of any of the tasks that will try
   to acquire the mutex, as well as being different from the task ID/
   priority of any task (another problem caused by the task ID == priority
   issue).  The following is a sample value that'll need to be adjusted
   based on usage by the calling application */

#define UCOS_PIP		10

/* Because of the strict priority scheduling, we have to specify the task
   priority (which then also becomes the task ID) when we create the task.
   The following is a sample task ID, which must be less than UCOS_PIP */

#define UCOS_TASKID		20

/* uC/OS-II typedefs 'BOOLEAN', which clashes with our own BOOLEAN, so we
   undefine it around the include of the uC/OS-II headers */

#undef BOOLEAN

#include <ucos_ucos_ii.h>

#define BOOLEAN			int

/* uC/OS-II defines 'BYTE' for backwards-compatibility with uC/OS-I but
   never uses it, so we remove it again to make our definition visible */

#undef BYTE

/* Object handles */

#define THREAD_HANDLE			INT8U
#define MUTEX_HANDLE			OS_EVENT *

/* Mutex management functions.  uC/OS-II mutexes aren't re-entrant (although
   this is never mentioned explicitly in any documentation the description 
   of how mutexes work in App.Note 1002 makes it clear that they're not), we
   use the standard trylock()-style mechanism to work around this */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised; \
		INT8U name##MutexOwner; \
		int name##MutexLockcount
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			INT8U err; \
			\
			krnlData->name##Mutex = OSMutexCreate( UCOS_PIP, &err ); \
			if( err == OS_ERR_NONE ) \
				{ \
				krnlData->name##MutexLockcount = 0; \
				krnlData->name##MutexInitialised = TRUE; \
				} \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			INT8U err; \
			\
			OSMutexPend( krnlData->name##Mutex, 0, &err ); \
			OSMutexPost( krnlData->name##Mutex ); \
			OSMutexDel( krnlData->name##Mutex, OS_DEL_ALWAYS, &err ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		{ \
		INT8U err; \
		\
		if( OSMutexAccept( krnlData->name##Mutex, &err ) == 0 ) \
			{ \
			if( !THREAD_SAME( krnlData->name##MutexOwner, THREAD_SELF() ) ) \
				OSMutexPend( krnlData->name##Mutex, 0, &err ); \
			else \
				krnlData->name##MutexLockcount++; \
			} \
		krnlData->name##MutexOwner = THREAD_SELF(); \
		}
#define MUTEX_UNLOCK( name ) \
		if( krnlData->name##MutexLockcount > 0 ) \
			krnlData->name##MutexLockcount--; \
		else \
			{ \
			krnlData->name##MutexOwner = THREAD_INITIALISER; \
			OSMutexPost( krnlData->name##Mutex ); \
			}

/* Thread management functions.  Because of the strict priority-based
   scheduling there's no way to perform a yield, the best that we can do
   is sleep for 1ms, which is better than performing a busy wait.

   Thread sleep times are measured in implementation-specific ticks rather
   than ms, so we have to scale the time based on the OS_TICKS_PER_SEC
   value */

#define THREADFUNC_DEFINE( name, arg )	void name( void *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			OS_STK *threadStack = clAlloc( EMBEDDED_STACK_SIZE ); \
			\
			syncHandle = OSSemCreate( 0 ); \
			if( OSTaskCreate( function, arg, \
							  ( BYTE * ) threadStack + ( EMBEDDED_STACK_SIZE - 1 ), \
							  UCOS_TASKID ) != OS_ERR_NONE ) \
				{ \
				free( threadStack ); \
				status = CRYPT_ERROR; \
				} \
			else \
				status = CRYPT_OK; \
			}
#define THREAD_EXIT( sync )		OSSemPost( sync ); \
								OSTaskDel( OS_PRIO_SELF )
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			threadSelf()
#if OS_TICKS_PER_SEC >= 1000
  #define THREAD_SLEEP( ms )	OSTimeDly( ( OS_TICKS_PER_SEC / 1000 ) * ms )
#else
  #define THREAD_SLEEP( ms )	OSTimeDly( max( ( ms * OS_TICKS_PER_SEC ) / 1000, 1 ) )
#endif /* OS_TICKS_PER_SEC time scaling */
#define THREAD_YIELD()			THREAD_SLEEP( 1 )
#define THREAD_WAIT( sync, status ) \
								{ \
								INT8U err; \
								\
								OSSemPend( sync, 0, &err ); \
								if( err != OS_ERR_NONE ) \
									status = CRYPT_ERROR; \
								OSSemDel( sync, OS_DEL_ALWAYS, &err ); \
								}
#define THREAD_CLOSE( sync )

/* uC/OS-II doesn't have a thread-self function, but allows general task
   info to be queried.  Because of this we provide a wrapper that returns
   the task ID as its return value */

INT8U threadSelf( void );

/* Because of the inability to do round-robin scheduling, we no-op out the
   use of internal threads/tasks.  Note that cryptlib itself is still thread-
   safe, it just can't do its init or keygen in an internal background
   thread */

#ifndef UCOS_USE_TASKS
  #undef THREAD_CREATE
  #undef THREAD_EXIT
  #undef THREAD_CLOSE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
								status = CRYPT_ERROR
  #define THREAD_EXIT( sync )
  #define THREAD_CLOSE( sync )
#endif /* !UCOS_USE_TASKS */

/****************************************************************************
*																			*
*								Unix/MVS/XMK								*
*																			*
****************************************************************************/

#elif ( defined( __UNIX__ ) || defined( __XMK__ ) ) && defined( USE_THREADS )

/* Under OSF/1 pthread.h includes c_asm.h which contains a declaration

	long asm( const char *,...);

   that conflicts with the gcc asm keyword.  This asm stuff is only used
   when inline asm alternatives to the Posix threading functions are enabled,
   which isn't done by default so in theory we could also fix this by
   defining asm to something else before including pthread.h, but it's safer
   to just disable inclusion of c_asm.h by pre-defining the guard define.
   This will result in a more useful warning if for some reason inline
   threading functions with asm are enabled */

#if defined( __osf__ ) || defined( __alpha__ )
  #define __C_ASM_H
#endif /* Alpha */

/* Linux threads are a particularly peculiar implementation, being based on
   the Linux clone() system call, which clones an entire process and uses
   a special "manager thread" to provide the appearance of a multithreaded
   application.  This threads == processes model produces very strange
   effects such as the appearance of a mass of (pseudo-)processes, each with
   their own PID, that appear to consume more memory than is physically
   present.  Another problem was that signals, which are done on a per-PID
   basis and should have been consistent across all threads in the process,
   were instead only delivered to one thread/pseudo-process and never got
   any further.  The clone()-based hack results in non-conformance with the
   pthreads spec as well as significant scalability and performance issues.

   The problem was finally (mostly) fixed with Ingo Molnar's native Posix
   thread library (NPTL) patches to the 2.5 development) kernel, which
   still retains the strange clone()-based threading mechanism but provides
   enough kludges to other parts of the kernel that it's not longer so
   obvious.  For example the clone() call has been optimised to make it
   more lightweight, Molnar's O(1) scheduler reduces the overhead of the
   process-per-thread mechanism, fast userspace mutexes eliminate the need
   for interthread signalling to implement locking, and most importantly the
   kernel identification of all threads has been collapsed to a single PID,
   eliminating the confusion caused by the cloned pseudo-processes */

#include <pthread.h>
#include <sys/time.h>
#ifdef __XMK__
  #include <sys/process.h>
  #include <sys/timer.h>
#endif /* Xilinx XMK */

/* Object handles */

#define THREAD_HANDLE			pthread_t
#define MUTEX_HANDLE			pthread_t

/* Mutex management functions.  Most Unix mutex implementations are non-
   re-entrant, which means that re-locking a mutex leads to deadlock
   (charming).  Some implementations can fix this by setting a mutex
   attribute to ensure that it doesn't deadlock using:

	pthread_mutexattr_settype( attr, PTHREAD_MUTEX_RECURSIVE );

   or:

	pthread_mutex_setrecursive();

   but this isn't universal.  To fix the problem, we implement our own
   re-entrant mutexes on top of the Posix ones.

   Due to the complexity of the locking process using pthreads' (usually)
   non-reentrant mutexes, we don't try and lock+unlock the mutex before we
   destroy it.  This isn't a major issue since it's just a safety precaution,
   the kernel should have forced any remaining threads to exit by the time
   the shutdown occurs anyway */

#define MUTEX_DECLARE_STORAGE( name ) \
		pthread_mutex_t name##Mutex; \
		BOOLEAN name##MutexInitialised; \
		pthread_t name##MutexOwner; \
		int name##MutexLockcount
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( pthread_mutex_init( &krnlData->name##Mutex, \
									NULL ) == 0 ) \
				{ \
				krnlData->name##MutexLockcount = 0; \
				krnlData->name##MutexInitialised = TRUE; \
				} \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			pthread_mutex_destroy( &krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		if( pthread_mutex_trylock( &krnlData->name##Mutex ) ) \
			{ \
			if( !THREAD_SAME( krnlData->name##MutexOwner, THREAD_SELF() ) ) \
				pthread_mutex_lock( &krnlData->name##Mutex ); \
			else \
				krnlData->name##MutexLockcount++; \
			} \
		krnlData->name##MutexOwner = THREAD_SELF();
#define MUTEX_UNLOCK( name ) \
		if( krnlData->name##MutexLockcount > 0 ) \
			krnlData->name##MutexLockcount--; \
		else \
			{ \
			krnlData->name##MutexOwner = THREAD_INITIALISER; \
			pthread_mutex_unlock( &krnlData->name##Mutex ); \
			}

/* Instead of the DIY recursive mutexes above it's also possible to use OS
   recursive mutexes if they're available.  Unfortunately there's no easy
   way to reliably test for these (they're often provided as _NP variants,
   or require obscure preprocessor trickery to enable them), and even if
   they're present they may not be supported (pthread_mutexattr_settype()
   returns an error), or the implementation may be flaky (some Linux 
   threading implementations).  Because of this the following use of 
   recursive mutexes needs to be manually enabled.  Note that on most 
   systems that use gcc it's necessary to define either _XOPEN_SOURCE=500 or 
   _GNU_SOURCE to get PTHREAD_MUTEX_RECURSIVE, otherwise only 
   PTHREAD_MUTEX_RECURSIVE_NP is defined, i.e. the standard behaviour of gcc 
   is to be nonstandard */

#if 0

#undef MUTEX_CREATE
#undef MUTEX_LOCK
#undef MUTEX_UNLOCK
#define MUTEX_CREATE( name, status ) \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			pthread_mutexattr_t mutexAttr;\
			\
			pthread_mutexattr_init( &mutexAttr );\
			pthread_mutexattr_settype( &mutexAttr, \
									   PTHREAD_MUTEX_RECURSIVE ); \
			if( pthread_mutex_init( &krnlData->name##Mutex, \
									&mutexAttr ) == 0 ) \
				{ \
				krnlData->name##MutexInitialised = TRUE; \
				status = CRYPT_OK; \
				} \
			else \
				status = CRYPT_ERROR; \
			pthread_mutexattr_destroy ( &mutexAttr );\
			}
#define MUTEX_LOCK( name )		pthread_mutex_lock( &krnlData->name##Mutex )
#define MUTEX_UNLOCK( name )	pthread_mutex_unlock( &krnlData->name##Mutex )

#endif /* 0 */

/* Putting a thread to sleep for a number of milliseconds can be done with
   select() because it should be a thread-safe one in the presence of
   pthreads.  In addition there are some system-specific quirks, these are
   handled by re-defining the macros below in a system-specific manner
   further on.

   Yielding a thread's timeslice gets rather complex due to a confusion of
   non-portable "portable" Posix functions.  Initially there was
   pthread_yield() from draft 4 of the Posix thread standard in 1990,
   popularised in the DCE threading code and picked up by a number of other
   implementations.  At about that time the realtime (1003.1b) and thread
   (1003.1c) standardisation was proceeding independently, with neither side
   knowing which one would make it to standards status first.  As it turned
   out this was 1003.1b with sched_yield().  When the 1003.1c folks were
   looking for every place where the text said "process" but should say
   "thread" once 1003.1c was in effect, they noticed that sched_yield() and
   pthread_yield() were now identical.  Since sched_yield() was already in
   the standard, there was no need for pthread_yield() so it was removed.
   However, some older implementations still do pthread_yield() and some
   (also older) implementations use sched_yield() to yield the processes'
   timeslice rather than the thread's timeslice, further complicated by the
   fact that some implementations like PHUX 10.x/11.x have buggy manpages
   that claim sched_yield() is per-process when in fact it's per-thread
   (PHUX 10.x still had pthread_yield() while 11.x only has sched_yield()).
   The whole is further confused by the fact that in some implementations,
   threads are processes (sort of, e.g. Linux's clone()'d threads and Sun
   LWPs).  In addition Sun have their own thr_yield which is part of their
   UI threads interface and that you have to fall back to occasionally.

   Because of this mess, we try for pthread_yield() if possible (since that
   definitely yields the thread's timeslice), fall back to sched_yield() if
   necessary, and add a special workaround for Sun systems.

   "Posix is portable in the sense that you can use a forklift to move the
    printed volumes around" */

#define THREADFUNC_DEFINE( name, arg )	void *name( void *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			status = pthread_create( &threadHandle, NULL, function, arg ) ? \
					 CRYPT_ERROR : CRYPT_OK; \
			syncHandle = threadHandle; \
			}
#define THREAD_EXIT( sync )		pthread_exit( ( void * ) 0 )
#define THREAD_INITIALISER		0
#define THREAD_SELF()			pthread_self()
#define THREAD_SAME( thread1, thread2 )	pthread_equal( ( thread1 ), ( thread2 ) )
#if defined( __osf__ ) || defined( __alpha__ ) || defined( __APPLE__ )
  #define THREAD_YIELD()		pthread_yield_np()
#elif defined( __MVS__ )
  #define THREAD_YIELD()		pthread_yield( NULL )
#elif defined( sun )
  #if OSVERSION <= 6
	/* Older Slowaris gets a bit complex, SunOS 4.x always returns -1 and 
	   sets errno to ENOSYS when sched_yield() is called, so we use this to 
	   fall back to the UI interface if necessary */
	#define THREAD_YIELD()		{ if( sched_yield() ) thr_yield(); }
  #else
	#define THREAD_YIELD()		sched_yield()
  #endif /* Slowaris 5.7 / 7.x or newer */
#elif defined( _AIX ) || defined( __CYGWIN__ ) || \
	  ( defined( __hpux ) && ( OSVERSION >= 11 ) ) || \
	  defined( __NetBSD__ ) || defined( __QNX__ ) || defined( __UCLIBC__ )
  #define THREAD_YIELD()		sched_yield()
#elif defined( __XMK__ )
  /* The XMK underlying scheduling object is the process context, for which
     the user-visible interface is the thread.  Therefore yielding the
	 underlying process context should yield the associated thread */
  #define THREAD_YIELD()		yield()
#else
  #if defined( __linux__ ) && !defined( __USE_GNU )
	void pthread_yield( void );
  #endif /* Present but not prototyped unless GNU extensions are enabled */
  #define  THREAD_YIELD()		pthread_yield()
#endif /* Not-very-portable Posix portability */
#define THREAD_SLEEP( ms )		{ \
								struct timeval tv = { 0 }; \
								\
								tv.tv_usec = ( ms ) * 1000; \
								select( 1, NULL, NULL, NULL, &tv ); \
								}
#define THREAD_WAIT( sync, status ) \
								if( pthread_join( sync, NULL ) < 0 ) \
									status = CRYPT_ERROR
#define THREAD_CLOSE( sync )

/* OSF1 includes some ghastly kludgery to handle binary compatibility from
   1003.4a to 1003.1c threading functions and inline asm functions with all
   sorts of name mangling and translation of function names and types.
   Unfortunately a straight vanilla compile leaves pthread_self() un-
   prototyped, which means that it's then implicitly prototyped as returned
   an int.  This generates hundreds of warnings of int <-> pointer casting
   problems, so if pthread_self() isn't redefined into one of a dozen
   different mangled versions we prototype it ourselves here */

#if ( defined( __osf__ ) || defined( __alpha__ ) ) && \
	!defined( pthread_self )
  #ifdef _PTHREAD_USE_MANGLED_NAMES_
	#define pthread_self __pthread_self
  #endif /* Name mangling */
  extern pthread_t pthread_self( void );
#endif /* OSF1 pthread_self function prototyping bug */

/* The pthreads implementation on MP-RAS (NCR User Space Threads based on
   CMA threads for DCE) doesn't accept NULL for several of the attribute
   arguments so we have to supply pthread_mutexattr_default attributes */

#ifdef _MPRAS
  #undef MUTEX_CREATE
  #define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( pthread_mutex_init( &krnlData->name##Mutex, \
									pthread_mutexattr_default ) == 0 ) \
				krnlData->name##MutexInitialised = TRUE; \
			else \
				status = CRYPT_ERROR; \
			}

  #undef THREAD_CREATE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			status = pthread_create( &threadHandle, pthread_attr_default, \
									 function, arg ) ? \
					 CRYPT_ERROR : CRYPT_OK ); \
			syncHandle = ( long ) threadHandle; \
			}
#endif /* _MPRAS */

/* Some systems (notably MVS and MP-RAS) use non-scalar pthread_t's, so we
   have to handle initialisation of these specially */

#if defined( __MVS__ ) || defined( _MPRAS )
  #define NONSCALAR_HANDLES
  #undef THREAD_INITIALISER
  #define THREAD_INITIALISER	{ 0 }
#endif /* Non-scalar pthread_t's */

/* XMK doesn't have a select(), however it has a sleep() as part of the timer
   package that performs the same function.  Note that there's a second
   sleep() that takes an argument in seconds rather than ms and that sleeps
   the overall process in the PPC BSP library, but presumably this won't be
   used if the sleep() in the timer package is enabled */

#ifdef __XMK__
  #undef THREAD_SLEEP
  #define THREAD_SLEEP( ms )	sleep( ms )
#endif /* Xilinx XMK */

/* UnixWare/SCO creates threads with a ridiculously small default stack size
   of a few KB or so, which means that the thread can't even start.  To work
   around this we have to use a custom thread-creation function that sets
   the stack size to something reasonable */

#ifdef __SCO_VERSION__
  #undef THREAD_CREATE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			pthread_attr_t attr; \
			\
			pthread_attr_init( &attr ); \
			pthread_attr_setstacksize( &attr, 32768 ); \
			status = pthread_create( &threadHandle, &attr, function, arg ); \
			pthread_attr_destroy( &attr ); \
			if( status ) \
				status = CRYPT_ERROR; \
			else \
				{ \
				status = CRYPT_OK; \
				syncHandle = ( long ) threadHandle; \
				} \
			}
#endif /* UnixWare/SCO */

/****************************************************************************
*																			*
*									VDK										*
*																			*
****************************************************************************/

#elif defined( __VDK__ )

/* VDK has a rather unusual concept of error handling in which it assumes 
   that system calls rarely fail and if they do the error should be handled 
   as an exception by a thread's ErrorFunction (augmenting the standard
   function return code), which is defined in the thread template alongside 
   various other things like the thread's Run function (see the discussion 
   on threads below for more on this).  Error handling via exceptions 
   depends on how the system is linked, if it's linked against the error-
   checking libraries then general error conditions are checked and result
   in an exception (e.g. waiting on an undefined semaphore), if the standard 
   libraries are used then only some critical errors such as timeout on a 
   semaphore result in an exception.

   This dual-path error handling is made worse by the fact that the default 
   ErrorFunction handler invokes a kernel panic (!!!), making clean recovery 
   somewhat difficult */

#include <VDK.h>

/* Object handles */

#define THREAD_HANDLE			VDK_ThreadID
#define MUTEX_HANDLE			VDK_SemaphoreID

/* Mutex management functions.  VDK semaphores are re-entrant so we don't 
   have to jump through the hoops that are necessary with most other OSes.
   
   VDK uses one-size-fits all semaphores and also has the concept of a
   "periodic semaphore" which is actually just a waitable timer triggering
   every n clock ticks.  We disable this functionality and create a basic
   binary semaphore with no (or at least near-infnite) timeout, in other 
   words a mutex */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( ( krnlData->name##Mutex = VDK_CreateSemaphore( 1, 1, 1, 0 ) ) != UINT_MAX ) \
				krnlData->name##MutexInitialised = TRUE; \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			VDK_PendSemaphore( krnlData->name##Mutex, 1000000 ); \
			VDK_PostSemaphore( krnlData->name##Mutex ); \
			VDK_DestroySemaphore( krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		VDK_PendSemaphore( krnlData->name##Mutex, 1000000 ); \
#define MUTEX_UNLOCK( name ) \
		VDK_PostSemaphore( krnlData->name##Mutex );

/* VDK has a somewhat strange concept of thread management in which a thread
   isn't defined as a stream of execution on code but via a template whose 
   details are fixed at compile time, with each template identified by a 
   unique value in a header file.  So instead of starting a thread with
   'CreatThread( threadFnPtr, threadFnParams, threadInfo )' VDK uses
   'CreateThread( threadType )' and the scheduler looks up the details that
   are normally pass in at runtime in a table mapping the thread type to the
   parameters that it needs.

   This is really awkward because it requires that all threads be defined in
   a single location at system build time, which is problematic for a 
   library like cryptlib because the calling code has to be aware of 
   cryptlib-internal thread functions when it's built.  Because of this we
   disable the use of cryptlib-internal threads, these are only used for
   dynamic driver binding which isn't needed under VDK because there are
   unlikely to be PKCS #11 or ODBC ports to it any time soon, or even
   dynamic library support which would be needed before it was possible to
   have loadable drivers.

   VDK threads are exited using a standard 'return' rather than via any OS-
   specific exit mechanism.

   Task sleep times are measured in implementation-specific ticks rather
   than ms, but we can get the duration using VDK_GetTickPeriod() */

#define THREADFUNC_DEFINE( name, arg )	thread_id name( void *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			VDK_ThreadCreationBlock threadInfo; \
			\
			memset( &threadInfo, 0, sizeof( VDK_ThreadCreationBlock ) ); \
			threadInfo.template_id = kDynamicThread; \
			threadInfo.user_data_ptr = arg; \
			threadInfo.pTemplate = /*<thread template, see above>*/; \
			\
			syncHandle = VDK_CreateSemaphore( 1, 1, 1, 0 ); \
			threadHandle = VDK_CreateThreadEx( &threadInfo ); \
			if( threadHandle == UINT_MAX ) \
				{ \
				VDK_DestroySemaphore( syncHandle ); \
				status = CRYPT_ERROR; \
				} \
			else \
				status = CRYPT_OK; \
			}
#define THREAD_EXIT( sync )		VDK_PostSemaphore( sync ); \
								return( 0 )
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			VDK_GetThreadID()
#define THREAD_SLEEP( ms )		VDK_Sleep( VDK_GetTickPeriod() * ms )
#define THREAD_YIELD()			VDK_Yield()
#define THREAD_WAIT( sync, status ) \
								if( !VDK_PendSemaphore( sync, 1000000 ) ) \
									status = CRYPT_ERROR; \
								else \
									VDK_PostSemaphore( sync ); \
								VDK_DestroySemaphore( sync )
#define THREAD_CLOSE( sync )

/* Because of the odd way in which thread management works we no-op out the
   use of internal threads.  Note that cryptlib itself is still thread-safe, 
   it just can't do its init or keygen in an internal background thread */

#ifndef VDK_USE_THREADS
  #undef THREAD_CREATE
  #undef THREAD_EXIT
  #undef THREAD_CLOSE
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
								status = CRYPT_ERROR
  #define THREAD_EXIT( sync )
  #define THREAD_CLOSE( sync )
#endif /* !VDK_USE_THREADS */

/****************************************************************************
*																			*
*									VxWorks									*
*																			*
****************************************************************************/

#elif defined( __VXWORKS__ )

#include <vxWorks.h>
#include <semLib.h>
#include <taskLib.h>

/* Object handles */

#define THREAD_HANDLE			int
#define MUTEX_HANDLE			SEM_ID

/* Mutex management functions.  VxWorks mutual exclusion semaphores (and
   only mutex semaphores) are re-entrant, so we don't have to jump through
   the hoops that are necessary with most other OSes */

#define MUTEX_DECLARE_STORAGE( name ) \
		MUTEX_HANDLE name##Mutex; \
		BOOLEAN name##MutexInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##MutexInitialised ) \
			{ \
			if( ( krnlData->name##Mutex = semMCreate( SEM_Q_FIFO ) ) != NULL ) \
				krnlData->name##MutexInitialised = TRUE; \
			else \
				status = CRYPT_ERROR; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##MutexInitialised ) \
			{ \
			semTake( krnlData->name##Mutex, WAIT_FOREVER ); \
			semGive( krnlData->name##Mutex ); \
			semDelete( krnlData->name##Mutex ); \
			krnlData->name##MutexInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		semTake( krnlData->name##Mutex, WAIT_FOREVER ); \
#define MUTEX_UNLOCK( name ) \
		semGive( krnlData->name##Mutex );

/* Thread management functions.  Some PPC compilers use the FP registers for
   non-FP operations such as working with long long data types (used for
   example in PKC key generation), so if we're building for the PPC we
   create tasks with FP register saving enabled */

#ifdef __ppc__
  #define TASK_ATTRIBUTES	VX_FP_TASK
#else
  #define TASK_ATTRIBUTES	0
#endif /* PPC vs.non-PPC register saves */

/* VxWorks tasks are exited using the standard ANSI exit() function rather
   than any OS-specific exit mechanism.

   Task sleep times are measured in implementation-specific ticks rather
   than ms, but it's usually close enough to allow us to treat them as being
   identical.  If we need truly accurate timing we could call a helper
   function that scales the time based on sysClkRateGet(), but at the moment
   it's only used for general short delays rather than any fixed amount of
   time so this isn't necessary */

#define THREADFUNC_DEFINE( name, arg )	thread_id name( void *arg )
#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
			{ \
			syncHandle = semBCreate( SEM_Q_FIFO, SEM_EMPTY ); \
			threadHandle = taskSpawn( NULL, T_PRIORITY, TASK_ATTRIBUTES, \
									  EMBEDDED_STACK_SIZE, function, \
									  ( int ) arg, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); \
			if( threadHandle == ERROR ) \
				{ \
				semDelete( syncHandle ); \
				status = CRYPT_ERROR; \
				} \
			else \
				status = CRYPT_OK; \
			}
#define THREAD_EXIT( sync )		semGive( sync ); \
								exit( 0 )
#define THREAD_INITIALISER		0
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SELF()			taskIdSelf()
#define THREAD_SLEEP( ms )		taskDelay( ms )
#define THREAD_YIELD()			taskDelay( NO_WAIT )
#define THREAD_WAIT( sync, status ) \
								if( semTake( sync, WAIT_FOREVER ) != OK ) \
									status = CRYPT_ERROR; \
								else \
									semGive( sync ); \
								semDelete( sync )
#define THREAD_CLOSE( sync )

/****************************************************************************
*																			*
*									Win32/WinCE								*
*																			*
****************************************************************************/

#elif ( defined( __WIN32__ ) && !defined( NT_DRIVER ) ) || \
	  defined( __WINCE__ )

#ifndef __WINCE__
  #include <process.h>
#endif /* __WINCE__ */

/* Object handles */

#define THREAD_HANDLE			DWORD
#define MUTEX_HANDLE			HANDLE

/* Mutex management functions.  InitializeCriticalSection() doesn't return 
   an error code but can throw a STATUS_NO_MEMORY exception in certain low-
   memory situations under Win2K/XP/2003 (it was fixed in Vista), however 
   this exception isn't raised in an exception-safe manner (the critical 
   section object is left in a corrupted state) so it can't be safely caught 
   and recovered from.  The result is that there's no point in trying to 
   catch it (this is a known design flaw in the function).

   EnterCriticalSection() is a bit more problematic.  Apart from the
   EXCEPTION_POSSIBLE_DEADLOCK exception (which is raised if the critical 
   section is corrupted, there's not much that can be done here), this can 
   also raise an out-of-memory exception due to on-demand allocation of the 
   event handle required by the critical section.  In Windows NT these were 
   always allocated by InitializeCriticalSection(), however in the Win2K 
   timeframe applications with hundreds of threads and hundreds or even 
   thousands of critical sections were starting to appear, many of which 
   were never user.  To avoid resource-exhaustion problems, Win2K was 
   changed to perform on-demand allocation of event handles in critical 
   sections, and even then only on the first contended acquire (rather than 
   the first time EnterCriticalSection() is called).  To avoid this problem, 
   InitializeCriticalSectionAndSpinCount() was kludged under Win2K to always 
   allocate the event handle if the high bit of the dwSpinCount value was 
   set, but this behaviour was disabled again in WinXP.

   Because of this behaviour, there's no easy safe way to initialise a 
   critical section.  What we'd have to do is create a second thread to 
   force an initialisation by trying to enter the section while another 
   thread holds the lock, forcing the allocation of an event handle.  In 
   theory we could do this (since cryptlib only uses a handful of critical 
   sections), but in practice if the system is so short of resources that it 
   can't allocate event handles any more then the user has bigger things to 
   worry about.  If we were running exclusively under Vista we could use the 
   extremely useful slim reader/writer (SRW) locks, but it'll be awhile yet 
   before we can rely on these (they're also non-reentrant due to their 
   extreme slimness, which means that they have to be managed very 
   carefully).

   Critical sections can lead to lock convoys, a variant of the thundering 
   herd problem that occurs due to an optimisation in lock management used 
   under Windows where a lock with another thread waiting would be handed 
   off to the waiting thread without ever being released.  In other words 
   the handoff consisted of handing ownership over to the waiting thread 
   (without unlocking the lock) instead of unlocking and re-locking with the 
   new thread. This was done to implement fair locking in which threads were 
   serviced in strictly FIFO order.

   This leads to a problem because it extends the lock hold time by the 
   thread context switch latency.  Consider a lock for which a thread 
   arrives every 2000 cycles and executes inside the lock for 1000 cycles.  
   With thread T0 holding the lock, a new thread T1 arrives 500 cycles into 
   the execution.  T0 releases the lock after 1000 cycles, and T1 takes 
   ownership.  However, the context switch time is 4000 cycles (up to 10K 
   cycles), so it can't actually start running inside the lock until cycle 
   5000, even if there are other threads ready to run immediately.  Windows 
   Vista fixed this by making locks slightly unfair, so that an active 
   thread can steal the lock from the one at the front of the wait queue if 
   the latter isn't scheduled to be run.

   Finally, special handling of critical sections occurs during a process 
   shutdown.  Firstly, every thread but the one that initiated the shutdown 
   is killed in a fairly hostile manner.  Then, in order to avoid deadlocks, 
   the system effectively ignores calls to Enter/LeaveCriticalSection(), 
   since the lock may have been held by one of the killed threads.  What 
   this means is that if the thread left lock-protected data in an 
   inconsistent state when it was killed and the shutdown thread comes along 
   and tries to use it, it's going to run into problems.  This is a 
   difficult problem to solve (the MT CRT apparently has a number of 
   problems with this in internal code paths), but luckily is triggered 
   extremely rarely, if ever */

#define MUTEX_DECLARE_STORAGE( name ) \
		CRITICAL_SECTION name##CriticalSection; \
		BOOLEAN name##CriticalSectionInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK;	/* See note above */ \
		if( !krnlData->name##CriticalSectionInitialised ) \
			{ \
			InitializeCriticalSection( &krnlData->name##CriticalSection ); \
			krnlData->name##CriticalSectionInitialised = TRUE; \
			}
#define MUTEX_DESTROY( name ) \
		if( krnlData->name##CriticalSectionInitialised ) \
			{ \
			EnterCriticalSection( &krnlData->name##CriticalSection ); \
			LeaveCriticalSection( &krnlData->name##CriticalSection ); \
			DeleteCriticalSection( &krnlData->name##CriticalSection ); \
			krnlData->name##CriticalSectionInitialised = FALSE; \
			}
#define MUTEX_LOCK( name ) \
		EnterCriticalSection( &krnlData->name##CriticalSection )
#define MUTEX_UNLOCK( name ) \
		LeaveCriticalSection( &krnlData->name##CriticalSection )

/* Mutex debug support */

#ifdef MUTEX_DEBUG

/* A queue to record prior mutex operations */

typedef struct {
	THREAD_HANDLE owner;
	int lockCount;
	int sequenceNo;
	} MUTEX_QUEUE_INFO;

#define MUTEX_QUEUE_SIZE	16

/* Alternative debug versions of the mutex manipulation primitives.  These 
   just record the last MUTEX_QUEUE_SIZE mutex operations in a FIFO queue for
   later analysis */

#undef MUTEX_DECLARE_STORAGE
#define MUTEX_DECLARE_STORAGE( name ) \
		CRITICAL_SECTION name##CriticalSection; \
		BOOLEAN name##CriticalSectionInitialised; \
		MUTEX_QUEUE_INFO name##EntryQueue[ MUTEX_QUEUE_SIZE ]; \
		MUTEX_QUEUE_INFO name##ExitQueue[ MUTEX_QUEUE_SIZE ]; \
		int name##LockCount; \
		int name##SequenceNo
#undef MUTEX_CREATE
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK; \
		if( !krnlData->name##CriticalSectionInitialised ) \
			{ \
			InitializeCriticalSection( &krnlData->name##CriticalSection ); \
			memset( &krnlData->name##EntryQueue, 0, \
					sizeof( MUTEX_QUEUE_INFO ) * MUTEX_QUEUE_SIZE ); \
			memset( &krnlData->name##ExitQueue, 0, \
					sizeof( MUTEX_QUEUE_INFO ) * MUTEX_QUEUE_SIZE ); \
			krnlData->name##LockCount = krnlData->name##SequenceNo = 0; \
			krnlData->name##CriticalSectionInitialised = TRUE; \
			}
#undef MUTEX_LOCK
#undef MUTEX_UNLOCK
#define MUTEX_LOCK( name ) \
		{ \
		int i; \
		\
		EnterCriticalSection( &krnlData->name##CriticalSection ); \
		for( i = 1; i < MUTEX_QUEUE_SIZE; i++ ) \
			krnlData->name##EntryQueue[ i - 1 ] = \
				krnlData->name##EntryQueue[ i ]; \
		krnlData->name##EntryQueue[ MUTEX_QUEUE_SIZE - 1 ].owner = THREAD_SELF(); \
		krnlData->name##EntryQueue[ MUTEX_QUEUE_SIZE - 1 ].lockCount = krnlData->name##LockCount++; \
		krnlData->name##EntryQueue[ MUTEX_QUEUE_SIZE - 1 ].sequenceNo = krnlData->name##SequenceNo++; \
		}
#define MUTEX_UNLOCK( name ) \
		{ \
		int i; \
		\
		for( i = 1; i < MUTEX_QUEUE_SIZE; i++ ) \
			krnlData->name##ExitQueue[ i - 1 ] = \
				krnlData->name##ExitQueue[ i ]; \
		krnlData->name##ExitQueue[ MUTEX_QUEUE_SIZE - 1 ].owner = THREAD_SELF(); \
		krnlData->name##ExitQueue[ MUTEX_QUEUE_SIZE - 1 ].lockCount = krnlData->name##LockCount--; \
		krnlData->name##ExitQueue[ MUTEX_QUEUE_SIZE - 1 ].sequenceNo = krnlData->name##SequenceNo++; \
		LeaveCriticalSection( &krnlData->name##CriticalSection ); \
		}
#endif /* MUTEX_DEBUG */

/* Thread management functions.  Win32 requires a C library-aware wrapper
   around the OS native CreateThread()/ExitThread() calls, with WinCE
   after 2.0 the C runtime is integrated into the OS so we can call them
   directly.

   There are two functions that we can call to get the current thread ID,
   GetCurrentThread() and GetCurrentThreadId().  GetCurrentThread() merely
   returns a constant value that's interpreted by various functions to
   mean "the current thread".  GetCurrentThreadId() returns the thread ID,
   however this isn't the same as the thread handle.
   
   After we wait for the thread, we need to close the handle.  This is
   complicated by the fact that we can only close it once all threads have
   exited the wait, which requires further calisthenics in the function that
   uses it to ensure that the last thread out closes the handle. This also
   means that we can't combine the close with the wait as for other OSes,
   since we can only perform the close once all waits have exited */

#if defined( __WIN32__ )
  #define THREADFUNC_DEFINE( name, arg ) \
				unsigned __stdcall name( void *arg )
  #if defined( _MSC_VER ) && VC_GE_2005( _MSC_VER )
	#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
				{ \
				uintptr_t hThread; \
				\
				hThread = _beginthreadex( NULL, 0, ( function ), ( arg ), 0, \
										  &threadHandle ); \
				syncHandle = ( MUTEX_HANDLE ) hThread; \
				status = ( hThread == 0 ) ? CRYPT_ERROR : CRYPT_OK; \
				}
  #else
	#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
				{ \
				unsigned long hThread; \
				\
				hThread = _beginthreadex( NULL, 0, ( function ), ( arg ), 0, \
										  &threadHandle ); \
				syncHandle = ( MUTEX_HANDLE ) hThread; \
				status = ( hThread == 0 ) ? CRYPT_ERROR : CRYPT_OK; \
				}
  #endif /* Older vs. newer VC++ */
  #define THREAD_EXIT( sync )	_endthreadex( 0 ); return( 0 )
#elif defined( __WINCE__ )
  #define THREADFUNC_DEFINE( name, arg ) \
				DWORD WINAPI name( void *arg )
  #define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
				{ \
				HANDLE hThread; \
				\
				hThread = CreateThread( NULL, 0, ( function ), ( arg ), 0, \
										&threadHandle ); \
				syncHandle = hThread; \
				status = ( hThread == NULL ) ? CRYPT_ERROR : CRYPT_OK; \
				}
  #define THREAD_EXIT( sync )	ExitThread( 0 ); return( 0 )
#endif /* Win32 vs. WinCE */
#define THREAD_INITIALISER		0
#define THREAD_SELF()			GetCurrentThreadId()
#define THREAD_SAME( thread1, thread2 )	( ( thread1 ) == ( thread2 ) )
#define THREAD_SLEEP( ms )		Sleep( ms )
#if defined( __WIN32__ )
  #define THREAD_YIELD()		threadYield()
#else
  #define THREAD_YIELD()		Sleep( 0 )
#endif /* Win32 vs. WinCE */
#define THREAD_WAIT( sync, status ) \
								if( WaitForSingleObject( sync, INFINITE ) != WAIT_OBJECT_0 ) \
									status = CRYPT_ERROR
#define THREAD_CLOSE( sync )	CloseHandle( sync )

/* Yielding a thread on an SMP or HT system is a tricky process, so we have 
   to use a custom function to do this */

void threadYield( void );

#elif defined( __WIN32__ ) && defined( NT_DRIVER )

/* Object handles */

#define THREAD_HANDLE				HANDLE
#define MUTEX_HANDLE				HANDLE

/* Mutex management functions */

#define MUTEX_DECLARE_STORAGE( name ) \
		KMUTEX name##CriticalSection; \
		BOOLEAN name##CriticalSectionInitialised
#define MUTEX_CREATE( name, status ) \
		status = CRYPT_OK;	/* Apparently never fails */ \
		if( !krnlData->name##CriticalSectionInitialised ) \
			{ \
			KeInitializeMutex( &krnlData->name##CriticalSection, 1 ); \
			krnlData->name##CriticalSectionInitialised = TRUE; \
			}
#define MUTEX_DESTROY( name )
#define MUTEX_LOCK( name ) \
		KeWaitForMutexObject( &krnlData->name##CriticalSection, Executive, \
							  KernelMode, FALSE, NULL )
#define MUTEX_UNLOCK( name ) \
		KeReleaseMutex( &krnlData->name##CriticalSection, FALSE )

/****************************************************************************
*																			*
*								Non-threaded OSes							*
*																			*
****************************************************************************/

#else

/* Generic or NOP versions of functions and types declared for those OSes
   that don't support threading */

#define THREAD_HANDLE							int
#define MUTEX_HANDLE							int

#define MUTEX_DECLARE_STORAGE( name )
#define MUTEX_CREATE( name, status )			status = CRYPT_OK
#define MUTEX_DESTROY( name )
#define MUTEX_LOCK( name )
#define MUTEX_UNLOCK( name )

#define THREAD_CREATE( function, arg, threadHandle, syncHandle, status ) \
												status = CRYPT_ERROR
#define THREAD_EXIT( sync )
#define THREAD_INITIALISER						0
#define THREAD_SAME( thread1, thread2 )			TRUE
#define THREAD_SELF()							0
#define THREAD_SLEEP( ms )
#define THREAD_YIELD()
#define THREAD_WAIT( sync, status )
#define THREAD_CLOSE( sync )

#endif /* Threading support macros */

/****************************************************************************
*																			*
*								Generic Defines								*
*																			*
****************************************************************************/

/* To initialise thread/mutex handles we need our own version of DUMMY_INIT,
   since this may be an OS-specific and/or non-scalar value we only define
   it if it hasn't already been defined above */

#ifdef NONSCALAR_HANDLES
  #define DUMMY_INIT_MUTEX		{ 0 }
#else
  #define DUMMY_INIT_MUTEX		( MUTEX_HANDLE ) DUMMY_INIT
#endif /* Scalar vs. non-scalar thread handles */

#endif /* _THREAD_DEFINED */
