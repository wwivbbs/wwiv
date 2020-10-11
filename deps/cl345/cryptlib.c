/****************************************************************************
*																			*
*							cryptlib Core Routines							*
*						Copyright Peter Gutmann 1992-2008					*
*																			*
****************************************************************************/

#include "crypt.h"

/* Prototypes for functions in init.c.  These should actually be annotated
   with CHECK_RETVAL_ACQUIRELOCK( MUTEX_LOCKNAME( initialisation ) and
   RELEASELOCK( MUTEX_LOCKNAME( initialisation ) ) but the mutex locking
   types aren't visible outside the kernel and in any case the annotation is
   only required where the functions are defined, so we just annotate them
   normally here */

CHECK_RETVAL \
int krnlBeginInit( void );
void krnlCompleteInit( void );
CHECK_RETVAL \
int krnlBeginShutdown( void );
RETVAL \
int krnlCompleteShutdown( void );

/* Somewhat untidy direct-access mechanism for functions that have to be 
   performed mid-startup or mid-shutdown */

int destroyObjects( void );
#ifndef CONFIG_CONSERVE_MEMORY_EXTRA
CHECK_RETVAL \
int testFunctionality( void );
CHECK_RETVAL \
int testKernel( void );
#endif /* CONFIG_CONSERVE_MEMORY_EXTRA */

/* Some messages communicate standard data values that are used again and
   again so we predefine values for these that can be used globally */

const int messageValueTrue = TRUE;
const int messageValueFalse = FALSE;
const int messageValueCryptOK = CRYPT_OK;
const int messageValueCryptError = CRYPT_ERROR;
const int messageValueCryptUnused = CRYPT_UNUSED;
const int messageValueCryptUseDefault = CRYPT_USE_DEFAULT;
const int messageValueCursorFirst = CRYPT_CURSOR_FIRST;
const int messageValueCursorNext = CRYPT_CURSOR_NEXT;
const int messageValueCursorPrevious = CRYPT_CURSOR_PREVIOUS;
const int messageValueCursorLast = CRYPT_CURSOR_LAST;

/* Safe pointers need a NULL-equivalent value which we also define here */

const DATAPTR DATAPTR_NULL = DATAPTR_INIT;
const FNPTR FNPTR_NULL = FNPTR_INIT;

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
  #undef USE_THREAD_FUNCTIONS
#endif /* __APPLE__  */

/* If we're fuzzing we also disable threaded init in order to make the 
   startup behaviour deterministic */

#if defined( CONFIG_FUZZ )
  #undef USE_THREAD_FUNCTIONS
#endif /* CONFIG_FUZZ */

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

typedef CHECK_RETVAL \
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
	int i, status = CRYPT_OK, LOOP_ITERATOR;

	assert( isReadPtr( mgmtFunctions, \
					   sizeof( MANAGEMENT_FUNCTION ) * mgmtFunctionCount ) );

	REQUIRES( isShortIntegerRangeNZ( mgmtFunctionCount ) );
	REQUIRES( isEnumRange( action, MANAGEMENT_ACTION ) );

	/* If we're performing a startup and the kernel is shutting down, bail 
	   out now */
	if( ( action == MANAGEMENT_ACTION_INIT ) && krnlIsExiting() )
		return( CRYPT_ERROR_PERMISSION );

	/* Dispatch each management action in turn */
	LOOP_MED( i = 0, i < mgmtFunctionCount && \
					 mgmtFunctions[ i ] != NULL, i++ )
		{
		const int localStatus = mgmtFunctions[ i ]( action );
		if( cryptStatusError( localStatus ) && cryptStatusOK( status ) )
			status = localStatus;

		/* If we're performing a startup and the kernel is shutting down, 
		   bail out now */
		if( ( action == MANAGEMENT_ACTION_INIT ) && krnlIsExiting() )
			return( CRYPT_ERROR_PERMISSION );
		}
	ENSURES( LOOP_BOUND_OK );

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

#ifdef USE_THREAD_FUNCTIONS

void threadedBind( const THREAD_PARAMS *threadParams )
	{
	assert( isReadPtr( threadParams, sizeof( THREAD_PARAMS ) ) );

	( void ) dispatchManagementAction( asyncInitFunctions, 
									   FAILSAFE_ARRAYSIZE( asyncInitFunctions, \
														   MANAGEMENT_FUNCTION ),
									   MANAGEMENT_ACTION_INIT );
	}
#endif /* USE_THREAD_FUNCTIONS */

/* Perform various sanity checks on the build process.  Since this will 
   typically be running on an embedded system there's not much that we can 
   (safely) do in terms of user I/O except to return a special-case return 
   code and hope that the user checks the embedded systems section of the 
   manual for more details, although we do try and produce diagnostic output 
   if this is enabled */

#ifdef INC_ALL					/* For OS/architecture names */
  #include "bn.h"
  #include "osconfig.h"
#else
  #include "bn/bn.h"
  #include "crypt/osconfig.h"
#endif /* Compiler-specific includes */

static BOOLEAN sanityCheckBuild( void )
	{
	BYTE data[ 16 ];

	/* Before we begin any checking, dump general build parameters */
	DEBUG_PUTS(( "** Build parameters **" ));
	DEBUG_PRINT(( "System is " SYSTEM_NAME ", %d bit ", 
				  sizeof( size_t ) * 8 ));
#ifdef DATA_LITTLEENDIAN
	DEBUG_PUTS(( "little-endian." ));
#else
	DEBUG_PUTS(( "big-endian." ));
#endif /* DATA_LITTLEENDIAN */
	DEBUG_PRINT(( "Long long size = %d, long size = %d, int size = %d, "
				  "short size = %d.\n", sizeof( LONGLONG_TYPE ) * 8,
				  sizeof( long ) * 8, sizeof( int ) * 8, 
				  sizeof( short ) * 8 ));
	DEBUG_PRINT(( "char is %d-bit %ssigned, wchar is %d-bit %ssigned.\n",
				  sizeof( char ) * 8, ( ( char ) -1 < 0 ) ? "" : "un", 
				  sizeof( wchar_t ) * 8, ( ( wchar_t ) -1 < 0 ) ? "" : "un" ));
	DEBUG_PRINT(( "Extended build options:" ));
#ifdef USE_CAST
	DEBUG_PRINT(( " USE_CAST" ));
#endif /* USE_CAST */
#ifdef USE_CERT_DNSTRING
	DEBUG_PRINT(( " USE_CERT_DNSTRING" ));
#endif /* USE_CERT_DNSTRING */
#ifdef USE_CERT_OBSOLETE
	DEBUG_PRINT(( " USE_CERT_OBSOLETE" ));
#endif /* USE_CERT_OBSOLETE */
#if defined( USE_CERTLEVEL_PKIX_FULL )
	DEBUG_PRINT(( " USE_CERTLEVEL_PKIX_FULL" ));
#elif defined( USE_CERTLEVEL_PKIX_PARTIAL )
	DEBUG_PRINT(( " USE_CERTLEVEL_PKIX_PARTIAL" ));
#elif defined( USE_CERTLEVEL_STANDARD )
	DEBUG_PRINT(( " USE_CERTLEVEL_STANDARD" ));
#elif defined( USE_CERTLEVEL_REDUCED )
	DEBUG_PRINT(( " USE_CERTLEVEL_REDUCED" ));
#elif defined( USE_CERT_OBSCURE )
	DEBUG_PRINT(( " USE_CERT_OBSCURE" ));
#else
	DEBUG_PRINT(( " (Unknown cert level)" ));
#endif /* Certificate level */
#ifdef USE_CFB
	DEBUG_PRINT(( " USE_CFB" ));
#endif /* USE_CFB */
#ifdef USE_CMSATTR_OBSCURE
	DEBUG_PRINT(( " USE_CMSATTR_OBSCURE" ));
#endif /* USE_CMSATTR_OBSCURE */
#ifdef USE_DEPRECATED_ALGORITHMS
	DEBUG_PRINT(( " USE_DEPRECATED_ALGORITHMS" ));
#endif /* USE_DEPRECATED_ALGORITHMS */
#ifdef USE_DES
	DEBUG_PRINT(( " USE_DES" ));
#endif /* USE_DES */
#ifdef USE_DNSSRV
	DEBUG_PRINT(( " USE_DNSSRV" ));
#endif /* USE_DNSSRV */
#ifdef USE_EAP
	DEBUG_PRINT(( " USE_EAP" ));
#endif /* USE_EAP */
#ifdef USE_ECDH
	DEBUG_PRINT(( " USE_ECDH" ));
#endif /* USE_ECDH */
#ifdef USE_ECDSA
	DEBUG_PRINT(( " USE_ECDSA" ));
#endif /* USE_ECDSA */
#ifdef USE_ELGAMAL
	DEBUG_PRINT(( " USE_ELGAMAL" ));
#endif /* USE_ELGAMAL */
#ifdef USE_EMBEDDED_OS
	DEBUG_PRINT(( " USE_EMBEDDED_OS" ));
#endif /* USE_EMBEDDED_OS */
#ifdef USE_ERRMSGS
	DEBUG_PRINT(( " USE_ERRMSGS" ));
#endif /* USE_ERRMSGS */
#ifdef USE_GCM
	DEBUG_PRINT(( " USE_GCM" ));
#endif /* USE_GCM */
#ifdef USE_HARDWARE
	DEBUG_PRINT(( " USE_HARDWARE" ));
#endif /* USE_HARDWARE */
#ifdef USE_IDEA
	DEBUG_PRINT(( " USE_IDEA" ));
#endif /* USE_IDEA */
#ifdef USE_JAVA
	DEBUG_PRINT(( " USE_JAVA" ));
#endif /* USE_JAVA */
#ifdef USE_LDAP
	DEBUG_PRINT(( " USE_LDAP" ));
#endif /* USE_LDAP */
#ifdef USE_OAEP
	DEBUG_PRINT(( " USE_OAEP" ));
#endif /* USE_OAEP */
#ifdef USE_OBSCURE_ALGORITHMS
	DEBUG_PRINT(( " USE_OBSCURE_ALGORITHMS" ));
#endif /* USE_OBSCURE_ALGORITHMS */
#ifdef USE_ODBC
	DEBUG_PRINT(( " USE_ODBC" ));
#endif /* USE_ODBC */
#ifdef USE_PATENTED_ALGORITHMS
	DEBUG_PRINT(( " USE_PATENTED_ALGORITHMS" ));
#endif /* USE_PATENTED_ALGORITHMS */
#ifdef USE_PGP2
	DEBUG_PRINT(( " USE_PGP2" ));
#endif /* USE_PGP2 */
#ifdef USE_PKCS11
	DEBUG_PRINT(( " USE_PKCS11" ));
#endif /* USE_PKCS11 */
#ifdef USE_PKCS12
	DEBUG_PRINT(( " USE_PKCS12" ));
#endif /* USE_PKCS12 */
#ifdef USE_PKCS12_WRITE
	DEBUG_PRINT(( " USE_PKCS12_WRITE" ));
#endif /* USE_PKCS12_WRITE */
#ifdef USE_PSEUDOCERTIFICATES
	DEBUG_PRINT(( " USE_PSEUDOCERTIFICATES" ));
#endif /* USE_PSEUDOCERTIFICATES */
#ifdef USE_RC2
	DEBUG_PRINT(( " USE_RC2" ));
#endif /* USE_RC2 */
#ifdef USE_RC4
	DEBUG_PRINT(( " USE_RC4" ));
#endif /* USE_RC4 */
#ifdef USE_RSA_SUITES
	DEBUG_PRINT(( " USE_RSA_SUITES" ));
#endif /* USE_RSA_SUITES */
#ifdef USE_SHA2_EXT
	DEBUG_PRINT(( " USE_SHA2_EXT" ));
#endif /* USE_SHA2_EXT */
#ifdef USE_SSH_EXTENDED
	DEBUG_PRINT(( " USE_SSH_EXTENDED" ));
#endif /* USE_SSH_EXTENDED */
#ifdef USE_SSH_CTR
	DEBUG_PRINT(( " USE_SSH_CTR" ));
#endif /* USE_SSH_CTR */
#ifdef USE_SSL3
	DEBUG_PRINT(( " USE_SSL3" ));
#endif /* USE_SSL3 */
#ifdef USE_THREADS
	DEBUG_PRINT(( " USE_THREADS" ));
#endif /* USE_THREADS */
#ifdef USE_TPM
	DEBUG_PRINT(( " USE_TPM" ));
#endif /* USE_TPM */
#ifdef USE_WEBSOCKETS
	DEBUG_PRINT(( " USE_WEBSOCKETS" ));
#endif /* USE_WEBSOCKETS */
	DEBUG_PRINT(( ".\n" ));
	DEBUG_PUTS(( "Build date/time for file " __FILE__ " is " __DATE__ ", " 
				 __TIME__ "." ));
#ifdef __STDC_VERSION__
	DEBUG_PRINT(( "STDC_VERSION = %ld.\n", __STDC_VERSION__ ));
#endif /* __STDC_VERSION__ */
#ifdef _POSIX_VERSION
	DEBUG_PRINT(( "POSIX_VERSION = %ld.\n", _POSIX_VERSION ));
#endif /* _POSIX_VERSION */
#ifdef __clang__
	DEBUG_PUTS(( "clang version = " __clang_version__ "." ));
#endif /* __clang__ */
#ifdef __GNUC__
	DEBUG_PUTS(( "gcc version = " __VERSION__ "." ));
#endif /* __GNUC__ */
#ifdef __HP_cc
	DEBUG_PRINTF(( "HP cc version = %06d.\n", __HP_cc ));
#endif /* __HP_cc */
#ifdef _MSC_VER
	DEBUG_PRINT(( "Visual C version = %ld.\n", _MSC_VER ));
#endif /* _MSC_VER */
#ifdef __SUNPRO_C
	DEBUG_PRINT(( "SunPro cc version = %X.\n", __SUNPRO_C ));
#endif /* __SUNPRO_C */
#if defined( __xlc__ )
	DEBUG_PRINT(( "IBM xlc version = %X.\n", __xlC__ ));
#elif defined( __IBMC__ )
	DEBUG_PRINT(( "IBM c89 version = %X.\n", __IBMC__ ));
#endif /* Different, semi-overlapping IBM compilers */
#if defined( __GNUC__ ) || defined( __clang__ )
	if( !__builtin_constant_p( MK_TOKEN( "1234" ) ) )
		{
		DEBUG_PUTS(( "Warning: Call stack tokens arent being computed by "
					 "the preprocessor,\n         this will lead to code "
					 "bloat in control-flow integrity checks." ));
		}
#endif /* Warn on non-preprocessor evaluation of MK_TOKEN() */

	/* Perform a sanity check to make sure that the endianness was set 
	   right.  The crypto self-test that's performed a bit later on will 
	   catch this problem as well but it's better to do an explicit check 
	   here that catches the endianness problem rather than just returning a 
	   generic self-test fail error */
#ifdef DATA_LITTLEENDIAN
	if( *( ( long * ) "\x80\x00\x00\x00\x00\x00\x00\x00" ) < 0 )
#else
	if( *( ( long * ) "\x80\x00\x00\x00\x00\x00\x00\x00" ) >= 0 )
#endif /* DATA_LITTLEENDIAN */
			{
			/* We should probably sound klaxons as well at this point */
			DEBUG_PUTS(( "CPU endianness is configured incorrectly, see "
						 "the cryptlib manual for details." ));
			return( FALSE );
			}

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
		DEBUG_PUTS(( "Compiler uses variable-length enumerated types, see "
					 "the cryptlib manual for details." ));
		return( FALSE );
		}
#endif /* Microsoft compilers */

	/* Make sure that isValidPointer() isn't reporting valid pointers as
	   being invalid.  This could in theory happen on some hypothetical OS
	   that does odd things with memory layouts so we check for it here */
	if( !isValidPointer( data ) || !isValidPointer( preInitFunctions ) )
		{
		DEBUG_PUTS(( "isValidPointer() macro reports data or code pointer "
					 "is invalid when it's valid, see misc/safety.h." ));
		return( FALSE );
		}

	return( TRUE );
	}

/* Initialise and shut down the system */

CHECK_RETVAL \
int initCryptlib( void )
	{
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int initLevel = 0, status;

	/* Let the user know that we're in the cryptlib startup code if they're in
	   debug mode */
	DEBUG_PUTS(( "" ));
	DEBUG_PUTS(( "***************************" ));
	DEBUG_PUTS(( "* Beginning cryptlib init *" ));
	DEBUG_PUTS(( "***************************" ));
	DEBUG_PUTS(( "" ));

	/* Perform any required sanity checks on the build process.  This would
	   be caught by the self-test but sometimes people don't run this so we
	   perform a minimal sanity check here to avoid failing in the startup
	   self-tests that follow */
	if( !sanityCheckBuild() )
		{
		DEBUG_DIAG(( "Build sanity-check failed" ));
		retIntError();
		}
	CFI_CHECK_UPDATE( "sanityCheckBuild" );

	/* Initiate the kernel startup */
	DEBUG_PUTS(( "** Initialising kernel **" ));
	status = krnlBeginInit();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Kernel init failed, status = %d", status ));
		return( status );
		}
	CFI_CHECK_UPDATE( "krnlBeginInit" );

	/* Verify that core functionality and universal crypto algorithms are 
	   working as required, unless we're running a fuzzing build for which 
	   we don't want to get held up too long in startup */
#if !defined( CONFIG_FUZZ ) && !defined( CONFIG_CONSERVE_MEMORY_EXTRA )
	DEBUG_PUTS(( "** Running general functionality self-tests **" ));
	status = testFunctionality();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "General functionality test failed, status = %d", 
					 status ));
		assert( DEBUG_WARN );
		krnlCompleteShutdown();
		return( CRYPT_ERROR_FAILED );
		}
#endif /* !CONFIG_FUZZ && !CONFIG_CONSERVE_MEMORY_EXTRA */
	CFI_CHECK_UPDATE( "testFunctionality" );

	/* Perform OS-specific additional initialisation inside the kernel init 
	   lock */
	DEBUG_PUTS(( "** Initialising system variables **" ));
	status = initSysVars();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "OS-specific initialisation failed, status = %d", 
					 status ));
		assert( DEBUG_WARN );
		krnlCompleteShutdown();
		return( CRYPT_ERROR_FAILED );
		}
	CFI_CHECK_UPDATE( "initSysVars" );

	/* Perform the multi-phase bootstrap */
	DEBUG_PUTS(( "** Running pre-init actions **" ));
	status = dispatchManagementAction( preInitFunctions, 
									   FAILSAFE_ARRAYSIZE( preInitFunctions, \
														   MANAGEMENT_FUNCTION ),
									   MANAGEMENT_ACTION_PRE_INIT );
	assertNoFault( cryptStatusOK( status ) );
	if( cryptStatusOK( status ) )
		{
		initLevel = 1;
		CFI_CHECK_UPDATE( "preInitFunctions" );
		DEBUG_PUTS(( "** Running init actions **" ));
		status = dispatchManagementAction( initFunctions, 
										   FAILSAFE_ARRAYSIZE( initFunctions, \
															   MANAGEMENT_FUNCTION ),
										   MANAGEMENT_ACTION_INIT );
		assertNoFault( cryptStatusOK( status ) );
		}
	if( cryptStatusOK( status ) )
		{
#ifdef USE_THREAD_FUNCTIONS
		BOOLEAN_INT asyncInit = FALSE;
#endif /* USE_THREAD_FUNCTIONS */

		initLevel = 2;
		CFI_CHECK_UPDATE( "initFunctions" );

		/* Perform the final init phase asynchronously or synchronously 
		   depending on the config option setting.  We always send this 
		   query to the default user object since no other user objects 
		   exist at this time */
		DEBUG_PUTS(( "** Running async init actions **" ));
#ifdef USE_THREAD_FUNCTIONS
		status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, 
								  IMESSAGE_GETATTRIBUTE, &asyncInit, 
								  CRYPT_OPTION_MISC_ASYNCINIT );
		if( cryptStatusOK( status ) && asyncInit == TRUE )
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
#endif /* USE_THREAD_FUNCTIONS */
		status = dispatchManagementAction( asyncInitFunctions, 
										   FAILSAFE_ARRAYSIZE( asyncInitFunctions, \
															   MANAGEMENT_FUNCTION ),
										   MANAGEMENT_ACTION_INIT );
		assertNoFault( cryptStatusOK( status ) );
		}
#if !defined( CONFIG_FUZZ ) && !defined( CONFIG_CONSERVE_MEMORY_EXTRA )
	if( cryptStatusOK( status ) )
		{
		CFI_CHECK_UPDATE( "asyncInitFunctions" );

		/* Everything's set up, verify that the core crypto algorithms and 
		   kernel security mechanisms are working as required, unless we're
		   running a fuzzing build for which we don't want to get held up
		   too long in startup */
		DEBUG_PUTS(( "** Running kernel self-tests **" ));
		status = testKernel();
		assertNoFault( cryptStatusOK( status ) );
		}
#else
	CFI_CHECK_UPDATE( "asyncInitFunctions" );
#endif /* !CONFIG_FUZZ && !CONFIG_CONSERVE_MEMORY_EXTRA */

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
	DEBUG_PUTS(( "** Completing kernel init **" ));
	krnlCompleteInit();
	CFI_CHECK_UPDATE( "krnlCompleteInit" );

	/* Let the user know that the cryptlib startup has completed 
	   successfully if they're in debug mode */
	DEBUG_PUTS(( "" ));
	DEBUG_PUTS(( "***************************" ));
	DEBUG_PUTS(( "* cryptlib init completed *" ));
	DEBUG_PUTS(( "***************************" ));
	DEBUG_PUTS(( "" ));

	REQUIRES( CFI_CHECK_SEQUENCE_8( "sanityCheckBuild", "krnlBeginInit", 
									"testFunctionality", "initSysVars", 
									"preInitFunctions", "initFunctions", 
									"asyncInitFunctions", "krnlCompleteInit" ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL \
int endCryptlib( void )
	{
	int status;

	/* Let the user know that we're in the cryptlib shutdown code if they're 
	   in debug mode */
	DEBUG_PUTS(( "" ));
	DEBUG_PUTS(( "*******************************" ));
	DEBUG_PUTS(( "* Beginning cryptlib shutdown *" ));
	DEBUG_PUTS(( "*******************************" ));
	DEBUG_PUTS(( "" ));

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

	/* Let the user know that the cryptlib shutdown has completed 
	   successfully if they're in debug mode */
	DEBUG_PUTS(( "" ));
	DEBUG_PUTS(( "*******************************" ));
	DEBUG_PUTS(( "* cryptlib shutdown completed *" ));
	DEBUG_PUTS(( "*******************************" ));
	DEBUG_PUTS(( "" ));


	return( status );
	}
