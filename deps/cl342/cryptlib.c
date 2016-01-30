/****************************************************************************
*																			*
*							cryptlib Core Routines							*
*						Copyright Peter Gutmann 1992-2008					*
*																			*
****************************************************************************/

#include "crypt.h"

/* Prototypes for functions in init.c */

CHECK_RETVAL \
int krnlBeginInit( void );
void krnlCompleteInit( void );
CHECK_RETVAL \
int krnlBeginShutdown( void );
int krnlCompleteShutdown( void );

/* Temporary kludge for functions that have to be performed mid-startup or 
   mid-shutdown */

int destroyObjects( void );
CHECK_RETVAL \
int testKernel( void );

/* Some messages communicate standard data values that are used again and
   again so we predefine values for these that can be used globally */

const int messageValueTrue = TRUE;
const int messageValueFalse = FALSE;
const int messageValueCryptOK = CRYPT_OK;
const int messageValueCryptError = CRYPT_ERROR;
const int messageValueCryptSignalled = CRYPT_ERROR_SIGNALLED;
const int messageValueCryptUnused = CRYPT_UNUSED;
const int messageValueCryptUseDefault = CRYPT_USE_DEFAULT;
const int messageValueCursorFirst = CRYPT_CURSOR_FIRST;
const int messageValueCursorNext = CRYPT_CURSOR_NEXT;
const int messageValueCursorPrevious = CRYPT_CURSOR_PREVIOUS;
const int messageValueCursorLast = CRYPT_CURSOR_LAST;

/* OS X Snow Leopard broke dlopen(), if it's called from a (sub-)thread then 
   it dies with a SIGTRAP.  Specifically, if you dlopen() a shared library 
   linked with CoreFoundation from a thread and the calling app wasn't 
   linked with CoreFoundation then the function CFInitialize() inside 
   dlopen() checks if the thread is the main thread (specifically 
   CFInitialize is declared with __attribute__ ((constructor))) and if it 
   isn't being called from the main thread it crashes with a SIGTRAP.  The 
   inability to call dlopen() from a thread was apparently a requirement in 
   pre-Snow Leopard versions as well but was never enforced.  One possible 
   workaround for this would be to require that any application that uses 
   cryptlib also link in CoreFoundation, but this will be rather error-
   prone, so we disable asynchronous driver binding instead */

#if defined( __APPLE__ )
  #undef USE_THREADS
#endif /* __APPLE__  */

/****************************************************************************
*																			*
*							Startup/Shutdown Routines						*
*																			*
****************************************************************************/

/* The initialisation and shutdown actions performed for various object
   types.  The pre-init actions are used to handle various preparatory
   actions that are required before the actual init can be performed, for
   example to create the system device and user object, which are needed by
   the init routines.  The pre-shutdown actions are used to signal to various
   subsystems that a shutdown is about to occur, for example to allow the
   networking subsystem to gracefully exit from any currently occurring 
   network I/O.

   The certificate init is somewhat special in that it only performs an
   internal consistency check rather than performing any actual 
   initialisation.  As such it's not performed as part of the asynchronous
   init since it has the potential to abort the cryptlib startup and as
   such can't be allowed to come back at a later date an retroactively shut 
   things down after other crypto operations have already occurred.  In fact
   since it's part of the startup self-test it's done in the pre-init, as a
   failure to complete the self-test will result in an immediate abort of the
   init process.

   The order of the init/shutdown actions is:

					Object type		Action
					-----------		------
	Pre-init:		Cert			Self-test only
					Device			Create system object

	Init:			User			Create default user object
					Keyset			Drivers - keysets			| Done async.
					Device			Drivers - devices			| if
					Session			Drivers - networking		| available
				   [Several]		Kernel self-test

	Pre-shutdown:	Session			Networking - signal socket close
					Device			System object - signal entropy poll end

	Shutdown:		User			Destroy default user object	| Done by
					Device			Destroy system object		| kernel
					Keyset			Drivers - keysets
					Device			Drivers - devices
					Session			Drivers - networking

   The init order is determined by the following object dependencies:

	All -> Device
			(System object handles many message types).
	User -> Keyset, Cert 
			(Default user object reads config data from the default keyset 
			 to init drivers for keysets, devices, and networking, and 
			 trusted certs.  The default keyset isn't read via a loadable 
			 keyset driver so it doesn't require the keyset driver init).
	Self-test -> Several
			(Kernel self-test creates several ephemeral objects in order to 
			 test the kernel mechanisms).

   The shutdown order is determined by the following dependencies:

	Session (Networking needs to shut down to release any objects that are 
			 blocked waiting on network I/O)
	Device (System object needs to shut down ongoing entropy poll)

   After this the shutdown proper can take place.  The shutdown order is
   noncritical, provided that the pre-shutdown actions have occurred.

   In theory the user and system objects are destroyed as part of the 
   standard shutdown, however the kernel prevents these objects from ever
   being explicitly destroyed so they're destroyed implicitly by the
   destroyObjects() cleanup call */

CHECK_RETVAL \
int certManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action );
CHECK_RETVAL \
int deviceManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action );
CHECK_RETVAL \
int keysetManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action );
CHECK_RETVAL \
int sessionManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action );
CHECK_RETVAL \
int userManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action );

typedef CHECK_RETVAL_FNPTR \
		int ( *MANAGEMENT_FUNCTION )( IN_ENUM( MANAGEMENT_ACTION ) \
										const MANAGEMENT_ACTION_TYPE action );

static const MANAGEMENT_FUNCTION preInitFunctions[] = {
  #ifdef USE_CERTIFICATES
	certManagementFunction,
  #endif /* USE_CERTIFICATES */
	deviceManagementFunction, 
	NULL, NULL 
	};
static const MANAGEMENT_FUNCTION initFunctions[] = {
	userManagementFunction, 
	NULL, NULL 
	};
static const MANAGEMENT_FUNCTION asyncInitFunctions[] = {
  #ifdef USE_KEYSETS
	keysetManagementFunction, 
  #endif /* USE_KEYSETS */
	deviceManagementFunction, 
  #ifdef USE_SESSIONS
	sessionManagementFunction, 
  #endif /* USE_SESSIONS */
	NULL, NULL 
	};
static const MANAGEMENT_FUNCTION preShutdownFunctions[] = {
  #ifdef USE_SESSIONS
	sessionManagementFunction, 
  #endif /* USE_SESSIONS */
	deviceManagementFunction, 
	NULL, NULL 
	};
static const MANAGEMENT_FUNCTION shutdownFunctions[] = {
	/*userManagementFunction,*/ /*deviceManagementFunction,*/ 
  #ifdef USE_KEYSETS
	keysetManagementFunction, 
  #endif /* USE_KEYSETS */
	deviceManagementFunction, 
  #ifdef USE_SESSIONS
	sessionManagementFunction, 
  #endif /* USE_SESSIONS */
	NULL, NULL 
	};

/* Dispatch a set of management actions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int dispatchManagementAction( IN_ARRAY( mgmtFunctionCount ) \
										const MANAGEMENT_FUNCTION *mgmtFunctions,
									 IN_INT_SHORT const int mgmtFunctionCount,
									 IN_ENUM( MANAGEMENT_ACTION ) \
										const MANAGEMENT_ACTION_TYPE action )
	{
	int i, status = CRYPT_OK;

	assert( isReadPtr( mgmtFunctions, \
					   sizeof( MANAGEMENT_FUNCTION ) * mgmtFunctionCount ) );

	REQUIRES( mgmtFunctionCount > 0 && \
			  mgmtFunctionCount < MAX_INTLENGTH_SHORT );
	REQUIRES( action > MANAGEMENT_ACTION_NONE && \
			  action < MANAGEMENT_ACTION_LAST );

	/* If we're performing a startup and the kernel is shutting down, bail 
	   out now */
	if( ( action == MANAGEMENT_ACTION_INIT ) && krnlIsExiting() )
		return( CRYPT_ERROR_PERMISSION );

	/* Dispatch each management action in turn */
	for( i = 0; i < mgmtFunctionCount && \
				mgmtFunctions[ i ] != NULL && \
				i < FAILSAFE_ITERATIONS_MED; i++ )
		{
		const int localStatus = mgmtFunctions[ i ]( action );
		if( cryptStatusError( localStatus ) && cryptStatusOK( status ) )
			status = localStatus;

		/* If we're performing a startup and the kernel is shutting down, 
		   bail out now */
		if( ( action == MANAGEMENT_ACTION_INIT ) && krnlIsExiting() )
			return( CRYPT_ERROR_PERMISSION );
		}
	ENSURES( i < FAILSAFE_ITERATIONS_MED );

	return( status );
	}

/* Under various OSes we bind to a number of drivers at runtime.  We can
   either do this sychronously or asynchronously depending on the setting of 
   a config option.  By default we use the async init since it speeds up the 
   startup.  Synchronisation is achieved by having the open/init functions 
   in the modules that require the drivers call krnlWaitSemaphore() on the 
   driver binding semaphore, which blocks until the drivers are bound if an 
   async bind is in progress, or returns immediately if no bind is in 
   progress */

#ifdef USE_THREADS

void threadedBind( const THREAD_PARAMS *threadParams )
	{
	assert( isReadPtr( threadParams, sizeof( THREAD_PARAMS ) ) );

	( void ) dispatchManagementAction( asyncInitFunctions, 
									   FAILSAFE_ARRAYSIZE( asyncInitFunctions, \
														   MANAGEMENT_FUNCTION ),
									   MANAGEMENT_ACTION_INIT );
	}
#endif /* USE_THREADS */

/* Perform various sanity checks on the build process.  Since this will 
   typically be running on an embedded system there's not much that we can 
   (safely) do in terms of user I/O except to return a special-case return 
   code and hope that the user checks the embedded systems section of the 
   manual for more details, although we do try and produce diagnostic output 
   if this is enabled */

static BOOLEAN buildSanityCheck( void )
	{
	/* If we're using a user-defined endianness override (i.e. it's a cross-
	   compile from a difference architecture) perform a sanity check to 
	   make sure that the endianness was set right.  The crypto self-test that's 
	   performed a few lines further down will catch this problem as well but 
	   it's better to do an explicit check here that catches the endianness 
	   problem rather than just returning a generic self-test fail error */
#if defined( CONFIG_DATA_LITTLEENDIAN ) || defined( CONFIG_DATA_BIGENDIAN )
  #ifdef DATA_LITTLEENDIAN
		if( *( ( long * ) "\x80\x00\x00\x00\x00\x00\x00\x00" ) < 0 )
  #else
		if( *( ( long * ) "\x80\x00\x00\x00\x00\x00\x00\x00" ) >= 0 )
  #endif /* DATA_LITTLEENDIAN */
			{
			/* We should probably sound klaxons as well at this point */
			DEBUG_PRINT(( "CPU endianness is configured incorrectly, see "
						  "the cryptlib manual for details" ));
			return( FALSE );
			}
#endif /* Big/little-endian override check */

	/* Make sure that the compiler doesn't use variable-size enums.  This is 
	   done by, for example, the PalmOS SDK for backwards compatibility with 
	   architectural decisions made for 68K-based PalmPilots, and at least 
	   one ARM compiler, as permitted by the ARM AAPCS (section 7.1.3), 
	   which says that "this ABI delegates a choice of representation of 
	   enumerated types to a platform ABI", followed by a long discussion of 
	   all the problems that this causes, after which it's allowed anyway,
	   but it can also be optionally enabled for compilers like gcc, so just 
	   to be safe we check all compilers except the ones that we know never 
	   do this */
#if !defined( _MSC_VER )
	if( sizeof( CRYPT_ALGO_TYPE ) != sizeof( int ) || \
		sizeof( CRYPT_MODE_TYPE ) != sizeof( int ) ||
		sizeof( CRYPT_ATTRIBUTE_TYPE ) != sizeof( int ) )
		{
		DEBUG_PRINT(( "Compiler uses variable-length enumerated types, see "
					  "the cryptlib manual for details" ));
		return( FALSE );
		}
#endif /* Microsoft compilers */

	return( TRUE );
	}

/* Initialise and shut down the system */

CHECK_RETVAL \
int initCryptlib( void )
	{
	int initLevel = 0, status;

	/* Let the user know that we're in the cryptlib startup code if they're in
	   debug mode */
	DEBUG_PRINT(( "\n" ));
	DEBUG_PRINT(( "***************************\n" ));
	DEBUG_PRINT(( "* Beginning cryptlib init *\n" ));
	DEBUG_PRINT(( "***************************\n" ));

	/* Perform any required sanity checks on the build process.  This would
	   be caught by the self-test but sometimes people don't run this so we
	   perform a minimal sanity check here to avoid failing in the startup
	   self-tests that follow */
	if( !buildSanityCheck() )
		{
		DEBUG_DIAG(( "Build sanity-check failed" ));
		retIntError();
		}

	/* Initiate the kernel startup */
	status = krnlBeginInit();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Kernel init failed" ));
		return( status );
		}

	/* Perform OS-specific additional initialisation inside the kernel init 
	   lock */
	status = initSysVars();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "OS-specific initialisation failed" ));
		assert( DEBUG_WARN );
		krnlCompleteShutdown();
		return( CRYPT_ERROR_FAILED );
		}

	/* Perform the multi-phase bootstrap */
	status = dispatchManagementAction( preInitFunctions, 
									   FAILSAFE_ARRAYSIZE( preInitFunctions, \
														   MANAGEMENT_FUNCTION ),
									   MANAGEMENT_ACTION_PRE_INIT );
	assert( cryptStatusOK( status ) );
	if( cryptStatusOK( status ) )
		{
		initLevel = 1;
		status = dispatchManagementAction( initFunctions, 
										   FAILSAFE_ARRAYSIZE( initFunctions, \
															   MANAGEMENT_FUNCTION ),
										   MANAGEMENT_ACTION_INIT );
		assert( cryptStatusOK( status ) );
		}
	if( cryptStatusOK( status ) )
		{
#ifdef USE_THREADS
		BOOLEAN asyncInit = FALSE;
#endif /* USE_THREADS */

		initLevel = 2;

		/* Perform the final init phase asynchronously or synchronously 
		   depending on the config option setting.  We always send this 
		   query to the default user object since no other user objects 
		   exist at this time */
#ifdef USE_THREADS
		status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, 
								  IMESSAGE_GETATTRIBUTE, &asyncInit, 
								  CRYPT_OPTION_MISC_ASYNCINIT );
		if( cryptStatusOK( status ) && asyncInit )
			{
			/* We use the kernel's thread storage for this thread, so we 
			   specify the thread data storage as NULL */
			status = krnlDispatchThread( threadedBind, NULL, NULL, 0,
										 SEMAPHORE_DRIVERBIND );
			if( cryptStatusError( status ) )
				{
				/* The thread couldn't be started, try again with a 
				   synchronous init */
				asyncInit = FALSE;
				}
			}
		if( !asyncInit )
#endif /* USE_THREADS */
		status = dispatchManagementAction( asyncInitFunctions, 
										   FAILSAFE_ARRAYSIZE( asyncInitFunctions, \
															   MANAGEMENT_FUNCTION ),
										   MANAGEMENT_ACTION_INIT );
		assert( cryptStatusOK( status ) );
		}
	if( cryptStatusOK( status ) )
		{
		/* Everything's set up, verify that the core crypto algorithms and 
		   kernel security mechanisms are working as required */
		status = testKernel();
		assert( cryptStatusOK( status ) );
		}

	/* If anything failed, shut down the internal functions and services
	   before we exit */
	if( cryptStatusError( status ) )
		{
		if( initLevel >= 1 )
			{
			/* Shut down any external interfaces */
			( void ) dispatchManagementAction( preShutdownFunctions, 
									FAILSAFE_ARRAYSIZE( preShutdownFunctions, \
														MANAGEMENT_FUNCTION ),
									MANAGEMENT_ACTION_PRE_SHUTDOWN );
			destroyObjects();
			( void ) dispatchManagementAction( shutdownFunctions, 
									FAILSAFE_ARRAYSIZE( shutdownFunctions, \
														MANAGEMENT_FUNCTION ),
									MANAGEMENT_ACTION_SHUTDOWN );
			}
		krnlCompleteShutdown();
		return( status );
		}

	/* Complete the kernel startup */
	krnlCompleteInit();

	/* Let the user know that the cryptlib startup has completed 
	   successfully if they're in debug mode */
	DEBUG_PRINT(( "\n" ));
	DEBUG_PRINT(( "***************************\n" ));
	DEBUG_PRINT(( "* cryptlib init completed *\n" ));
	DEBUG_PRINT(( "***************************\n" ));
	DEBUG_PRINT(( "\n" ));

	return( CRYPT_OK );
	}

CHECK_RETVAL \
int endCryptlib( void )
	{
	int status;

	/* Initiate the kernel shutdown */
	status = krnlBeginShutdown();
	if( cryptStatusError( status ) )
		return( status );

	/* Reverse the process carried out in the multi-phase bootstrap */
	( void ) dispatchManagementAction( preShutdownFunctions, 
							FAILSAFE_ARRAYSIZE( preShutdownFunctions, \
												MANAGEMENT_FUNCTION ),
							MANAGEMENT_ACTION_PRE_SHUTDOWN );
	status = destroyObjects();
	( void ) dispatchManagementAction( shutdownFunctions, 
							FAILSAFE_ARRAYSIZE( shutdownFunctions, \
												MANAGEMENT_FUNCTION ),
							MANAGEMENT_ACTION_SHUTDOWN );

	/* Complete the kernel shutdown */
	krnlCompleteShutdown();
	return( status );
	}
