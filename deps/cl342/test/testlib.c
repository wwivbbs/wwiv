/****************************************************************************
*																			*
*								cryptlib Test Code							*
*						Copyright Peter Gutmann 1995-2012					*
*																			*
****************************************************************************/

#if defined( __Nucleus__ )
  #include <nu_ctype.h>
  #include <nu_string.h>
#else
  #include <ctype.h>	/* For toupper() */
#endif /* OS-specific includes */

#include "cryptlib.h"
#include "test/test.h"

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */

/* Optionally include and activate the Visual Leak Detector library if
   we're running a debug build under VC++ 6.0.  Note that this can't be
   run at the same time as Bounds Checker, since the two interefere with
   each other */

#if defined( _MSC_VER ) && ( _MSC_VER == 1200 ) && 0
  #include "binaries/vld.h"
#endif /* VC++ 6.0 */

/* Optionally include the Intel Thread Checker API to control analysis */

#if defined( _MSC_VER ) && ( _MSC_VER == 1200 ) && 0
  #define USE_TCHECK
  #include "../../../Intel/VTune/tcheck/Include/libittnotify.h"
  #include "../../../Intel/VTune/Analyzer/Include/VtuneApi.h"
  #pragma comment( lib, "C:/Program Files/Intel/VTune/Analyzer/Lib/libittnotify.lib" )
  #pragma comment( lib, "C:/Program Files/Intel/VTune/Analyzer/Lib/VtuneApi.lib " )
  #define THREAD_DEBUG_SUSPEND()	__itt_pause(); VTPause()
  #define THREAD_DEBUG_RESUME()		VTResume(); __itt_resume()
#else
  #define THREAD_DEBUG_SUSPEND()
  #define THREAD_DEBUG_RESUME()
#endif /* VC++ 6.0 with Intel Thread Checker */

/* Warn about nonstandard build options */

#if defined( CONFIG_SUITEB_TESTS ) && defined( _MSC_VER )
  #pragma message( "  Building Suite B command-line test configuration." )
#endif /* CONFIG_SUITEB_TESTS with VC++ */

/* Whether various keyset tests worked, the results are used later to test
   other routines.  We initially set the key read result to TRUE in case the
   keyset read tests are never called, so we can still trying reading the
   keys in other tests */

int keyReadOK = TRUE, doubleCertOK = FALSE;

/* There are some sizeable (for DOS) data structures used, so we increase the
   stack size to allow for them */

#if defined( __MSDOS16__ ) && defined( __TURBOC__ )
  extern unsigned _stklen = 16384;
#endif /* __MSDOS16__ && __TURBOC__ */

/* Prototypes for general debug routines used to evaluate problems with certs
   and envelopes from other apps */

int xxxCertImport( const char *fileName );
int xxxCertCheck( const char *certFileName, const char *caFileNameOpt );
void xxxPubKeyRead( const char *fileName, const char *keyName );
void xxxPrivKeyRead( const char *fileName, const char *keyName, const char *password );
void xxxDataImport( const char *fileName );
void xxxSignedDataImport( const char *fileName );
void xxxEncryptedDataImport( const char *fileName, const char *keyset,
							 const char *password );

/* Prototype for custom key-creation routines */

int createTestKeys( void );

/* Prototype for stress test interface routine */

void smokeTest( void );

/* Prototype for Suite B test entry point */

#if defined( CONFIG_SUITEB_TESTS ) 

int suiteBMain( int argc, char **argv );

#endif /* CONFIG_SUITEB_TESTS */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* The pseudo-CLI VC++ output windows are closed when the program exits so 
   we have to explicitly wait to allow the user to read them */

#if defined( __WINDOWS__ ) && !defined( NDEBUG )

static void cleanExit( const int exitStatus )
	{
	puts( "\nHit a key..." );
	getchar();
	exit( exitStatus );
	}
static void cleanupAndExit( const int exitStatus )
	{
	int status;

	status = cryptEnd();
	if( status == CRYPT_ERROR_INCOMPLETE )
		puts( "cryptEnd() failed with CRYPT_ERROR_INCOMPLETE." );
	cleanExit( exitStatus );
	}
#else

static void cleanExit( const int exitStatus )
	{
	exit( exitStatus );
	}
static void cleanupAndExit( const int exitStatus )
	{
	cryptEnd();
	cleanExit( exitStatus );
	}
#endif /* __WINDOWS__ && !NDEBUG */

/* Update the cryptlib config file.  This code can be used to set the
   information required to load PKCS #11 device drivers:

	- Set the driver path in the CRYPT_OPTION_DEVICE_PKCS11_DVR01 setting
	  below.
	- Add a call to updateConfig() from somewhere (e.g.the test kludge function).
	- Run the test code until it calls updateConfig().
	- Remove the updateConfig() call, then run the test code as normal.
	  The testDevices() call will report the results of trying to use your
	  driver.

  cryptlib's SafeLoadLibrary() will always load drivers from the Windows 
  system directory / %SystemDirectory% unless given an absolute path */

static void updateConfig( void )
	{
#if 0
	const char *driverPath = "acospkcs11.dll";		/* ACOS */
	const char *driverPath = "aetpkss1.dll";		/* AET */
	const char *driverPath = "aloaha_pkcs11.dll";	/* Aloaha */
	const char *driverPath = "etpkcs11.dll";		/* Aladdin eToken */
	const char *driverPath = "psepkcs11.dll";		/* A-Sign */
	const char *driverPath = "asepkcs.dll";			/* Athena */
	const char *driverPath = "c:/temp/bpkcs11.dll";	/* Bloomberg */
	const char *driverPath = "cryst32.dll";			/* Chrysalis */
	const char *driverPath = "c:/program files/luna/cryst201.dll";	/* Chrysalis */
	const char *driverPath = "pkcs201n.dll";		/* Datakey */
	const char *driverPath = "dkck201.dll";			/* Datakey (for Entrust) */
	const char *driverPath = "dkck232.dll";			/* Datakey/iKey (NB: buggy, use 201) */
	const char *driverPath = "c:/program files/eracom/cprov sw/cryptoki.dll";	/* Eracom (old, OK) */
	const char *driverPath = "c:/program files/eracom/cprov runtime/cryptoki.dll";	/* Eracom (new, doesn't work) */
	const char *driverPath = "sadaptor.dll";		/* Eutron */
	const char *driverPath = "ngp11v211.dll";		/* Feitian Technology */
	const char *driverPath = "pk2priv.dll";			/* Gemplus */
	const char *driverPath = "c:/program files/gemplus/gclib.dll";	/* Gemplus */
	const char *driverPath = "cryptoki.dll";		/* IBM */
	const char *driverPath = "csspkcs11.dll";		/* IBM */
	const char *driverPath = "ibmpkcss.dll";		/* IBM */
	const char *driverPath = "id2cbox.dll";			/* ID2 */
	const char *driverPath = "cknfast.dll";			/* nCipher */
	const char *driverPath = "/opt/nfast/toolkits/pkcs11/libcknfast.so";/* nCipher under Unix */
	const char *driverPath = "/usr/lib/libcknfast.so";	/* nCipher under Unix */
	const char *driverPath = "c:/program files/mozilla firefox/softokn3.dll";/* Netscape */
	const char *driverPath = "nxpkcs11.dll";		/* Nexus */
	const char *driverPath = "AuCryptoki2-0.dll";	/* Oberthur */
	const char *driverPath = "opensc-pkcs11.dll";	/* OpenSC */
	const char *driverPath = "micardoPKCS11.dll";	/* Orga Micardo */
	const char *driverPath = "cryptoki22.dll";		/* Rainbow HSM (for USB use Datakey dvr) */
	const char *driverPath = "p11card.dll";			/* Safelayer HSM (for USB use Datakey dvr) */
	const char *driverPath = "slbck.dll";			/* Schlumberger */
	const char *driverPath = "SetTokI.dll";			/* SeTec */
	const char *driverPath = "siecap11.dll";		/* Siemens */
	const char *driverPath = "smartp11.dll";		/* SmartTrust */
	const char *driverPath = "SpyPK11.dll";			/* Spyrus */
#endif /* 0 */
	const char *driverPath = "c:/program files/eracom/cprov sw/cryptoki.dll";	/* Eracom (old, OK) */
	int status;

	printf( "Updating cryptlib configuration to load PKCS #11 driver\n  "
			"'%s'\n  as default driver...", driverPath );

	/* Set the path for a PKCS #11 device driver.  We only enable one of
	   these at a time to speed the startup time */
	status = cryptSetAttributeString( CRYPT_UNUSED, 
									  CRYPT_OPTION_DEVICE_PKCS11_DVR01,
									  driverPath, strlen( driverPath ) );
	if( cryptStatusError( status ) )
		{
		printf( "\n\nError updating PKCS #11 device driver profile, "
				"status %d.\n", status );
		cleanupAndExit( EXIT_FAILURE );
		}

	/* Flush the updated options to disk */
	status = cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CONFIGCHANGED, 
								FALSE );
	if( cryptStatusError( status ) )
		{
		printf( "\n\nError comitting device driver profile update to disk, "
				"status %d.\n", status );
		cleanupAndExit( EXIT_FAILURE );
		}

	puts( " done.\n" );
	cleanupAndExit( EXIT_SUCCESS );
	}

/* Add trusted certs to the config file and make sure that they're
   persistent.  This can't be done in the normal self-test since it requires
   that cryptlib be restarted as part of the test to re-read the config file,
   and because it modifies the cryptlib config file */

static void updateConfigCert( void )
	{
	CRYPT_CERTIFICATE trustedCert;
	int status;

	/* Import the first certificate, make it trusted, and commit the changes */
	importCertFromTemplate( &trustedCert, CERT_FILE_TEMPLATE, 1 );
	cryptSetAttribute( trustedCert, CRYPT_CERTINFO_TRUSTED_IMPLICIT, TRUE );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CONFIGCHANGED, FALSE );
	cryptDestroyCert( trustedCert );
	cryptEnd();

	/* Do the same with a second certificate.  At the conclusion of this, we 
	   should have two trusted certificates on disk */
	status = cryptInit();
	if( cryptStatusError( status ) )
		{
		puts( "Couldn't reload cryptlib configuration." );
		return;
		}
	importCertFromTemplate( &trustedCert, CERT_FILE_TEMPLATE, 2 );
	cryptSetAttribute( trustedCert, CRYPT_CERTINFO_TRUSTED_IMPLICIT, TRUE );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CONFIGCHANGED, FALSE );
	cryptDestroyCert( trustedCert );
	cryptEnd();
	}

/****************************************************************************
*																			*
*								Argument Processing							*
*																			*
****************************************************************************/

/* Flags for the tests that we want to perform */

#define DO_SELFTEST				0x0001
#define DO_LOWLEVEL				0x0002
#define DO_RANDOM				0x0004
#define DO_CONFIG				0x0008
#define DO_DEVICE				0x0010
#define DO_MIDLEVEL				0x0020
#define DO_CERT					0x0040
#define DO_KEYSETFILE			0x0080
#define DO_KEYSETDBX			0x0100
#define DO_CERTPROCESS			0x0200
#define DO_HIGHLEVEL			0x0400
#define DO_ENVELOPE				0x0800
#define DO_SESSION				0x1000
#define DO_SESSIONLOOPBACK		0x2000
#define DO_USER					0x4000

#define DO_ALL					0xFFFF

/* Show usage and exit */

static void usageExit( void )
	{
	puts( "Usage: testlib [-bcdefhiklmoprstu]" );

	puts( "  Encryption function tests:" );
	puts( "       -l = Test low-level functions" );
	puts( "       -m = Test mid-level functions" );
	puts( "       -i = Test high-level functions" );
	puts( "" );

	puts( "  Subsystem tests:" );
	puts( "       -c = Test certificate subsystem" );
	puts( "       -d = Test device subsystem" );
	puts( "       -f = Test file keyset subsystem" );
	puts( "       -k = Test database keyset subsystem" );
	puts( "       -e = Test envelope subsystem" );
	puts( "       -s = Test session subsystem" );
	puts( "       -t = Test session subsystem via loopback interface" );
	puts( "       -u = Test user subsystem" );
	puts( "" );

	puts( "  Miscellaneous tests:" );
	puts( "       -r = Test entropy-gathering routines" );
	puts( "       -b = Perform built-in self-test" );
	puts( "       -o = Test configuration management routines" );
	puts( "       -p = Test certificate processing" );
	puts( "" );

	puts( "  Other options:" );
	puts( "       -h = Display this help text" );
	puts( "       -- = End of arg list" );
	puts( "" );

	puts( "Default is to test all cryptlib subsystems." );
	exit( EXIT_FAILURE );
	}

/* Process command-line arguments */

static int processArgs( int argc, char **argv, 
						int *argFlags, const char **zargPtrPtr )
	{
	const char *zargPtr = NULL;
	BOOLEAN moreArgs = TRUE;

	/* Clear return value */
	*argFlags = 0;
	*zargPtrPtr = NULL;

	/* No args means test everything */
	if( argc <= 0 )
		{
		*argFlags = DO_ALL;

		return( TRUE );
		}

	/* Check for arguments */
	while( argc > 0 && *argv[ 0 ] == '-' && moreArgs )
		{
		const char *argPtr = argv[ 0 ] + 1;

		while( *argPtr )
			{
			switch( toupper( *argPtr ) )
				{
				case '-':
					moreArgs = FALSE;	/* GNU-style end-of-args flag */
					break;

				case 'B':
					*argFlags |= DO_SELFTEST;
					break;

				case 'C':
					*argFlags |= DO_CERT;
					break;

				case 'D':
					*argFlags |= DO_DEVICE;
					break;

				case 'E':
					*argFlags |= DO_ENVELOPE;
					break;

				case 'F':
					*argFlags |= DO_KEYSETFILE;
					break;

				case 'H':
					usageExit();
					break;

				case 'I':
					*argFlags |= DO_HIGHLEVEL;
					break;

				case 'K':
					*argFlags |= DO_KEYSETDBX;
					break;

				case 'L':
					*argFlags |= DO_LOWLEVEL;
					break;

				case 'M':
					*argFlags |= DO_MIDLEVEL;
					break;

				case 'O':
					*argFlags |= DO_CONFIG;
					break;

				case 'P':
					*argFlags |= DO_CERTPROCESS;
					break;

				case 'R':
					*argFlags |= DO_RANDOM;
					break;

				case 'S':
					*argFlags |= DO_SESSION;
					break;

				case 'T':
					*argFlags |= DO_SESSIONLOOPBACK;
					break;

				case 'U':
					*argFlags |= DO_USER;
					break;

				case 'Z':
					zargPtr = argPtr + 1;
					if( *zargPtr == '\0' )
						{
						puts( "Error: Missing argument for -z option." );
						return( FALSE );
						}
					while( argPtr[ 1 ] )
						argPtr++;	/* Skip rest of arg */
					*zargPtrPtr = zargPtr;
					break;

				default:
					printf( "Error: Unknown argument '%c'.\n", *argPtr );
					return( FALSE );
				}
			argPtr++;
			}
		argv++;
		argc--;
		}
	if( argc > 0 )
		{
		printf( "Error: Unknown argument '%s'.\n", argv[ 0 ] );
		return( FALSE );
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*								Misc.Kludges								*
*																			*
****************************************************************************/

/* Generic test code insertion point.  The following routine is called
   before any of the other tests are run and can be used to handle special-
   case tests that aren't part of the main test suite */

static void testKludge( const char *argPtr )
	{
#if 0
	testReadCorruptedKey();
#endif /* 0 */

	/* To test dodgy certificate collections */
#if 0
	int result;

	if( argPtr == NULL )
		{
		printf( "Error: Missing argument.\n" );
		exit( EXIT_FAILURE );
		}
	if( *argPtr == '@' )
		{
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   CRYPT_COMPLIANCELEVEL_OBLIVIOUS );
		argPtr++;
		}
	if( *argPtr == '#' )
		{
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   CRYPT_COMPLIANCELEVEL_PKIX_FULL );
		argPtr++;
		}
	result = xxxCertImport( argPtr );
	cryptEnd();
	exit( result ? EXIT_SUCCESS : EXIT_FAILURE );
#endif /* 1 */

	/* Performance-testing test harness */
#if 0
	void performanceTests( const CRYPT_DEVICE cryptDevice );

	performanceTests( CRYPT_UNUSED );
#endif /* 0 */

	/* Memory diagnostic test harness */
#if 0
	testReadFileCertPrivkey();
	testEnvelopePKCCrypt();		/* Use "Datasize, certificate" */
	testEnvelopeSign();			/* Use "Datasize, certificate" */
#endif /* 0 */

	/* Simple (brute-force) server code. NB: Remember to change
	   setLocalConnect() to not bind the server to localhost if expecting
	   external connections */
#if 0
	while( TRUE )
		testSessionTSPServer();
#endif /* 0 */

	/* Exit point for the test harnesses above, used when we don't want to 
	   fall through to the main test code */
#if 0
	cleanupAndExit( EXIT_SUCCESS );
#endif /* 0 */
	}

/****************************************************************************
*																			*
*								Main Test Code								*
*																			*
****************************************************************************/

/* Comprehensive cryptlib functionality test.  To get the following to run 
   under WinCE as a native console app it's necessary to change the entry 
   point in Settings | Link | Output from WinMainCRTStartup to the 
   undocumented mainACRTStartup, which calls main() rather than WinMain(), 
   however this only works if the system has a native console-mode driver 
   (most don't) */

int main( int argc, char **argv )
	{
	const char *zargPtr = NULL;
	BOOLEAN sessionTestError = FALSE, loopbackTestError = FALSE;
	int flags, status;
	void testSystemSpecific1( void );
	void testSystemSpecific2( void );

	/* If we're running in test mode, run the test code and exit */
#ifdef CONFIG_SUITEB_TESTS
	return( suiteBMain( argc, argv ) );
#endif /* CONFIG_SUITEB_TESTS */

	/* Print a general banner to let the user know what's going on */
	printf( "testlib - cryptlib %d-bit self-test framework.\n", 
			( int ) sizeof( long ) * 8 );	/* Cast for gcc */
	puts( "Copyright Peter Gutmann 1995 - 2012." );
	puts( "" );

	/* Skip the program name and process any command-line arguments */
	argv++; argc--;
	status = processArgs( argc, argv, &flags, &zargPtr );
	if( !status )
		exit( EXIT_FAILURE );

#ifdef USE_TCHECK
	THREAD_DEBUG_SUSPEND();
#endif /* USE_TCHECK */

	/* Make sure that various system-specific features are set right */
	testSystemSpecific1();

	/* VisualAge C++ doesn't set the TZ correctly.  The check for this isn't
	   as simple as it would seem since most IBM compilers define the same
	   preprocessor values even though it's not documented anywhere, so we
	   have to enable the tzset() call for (effectively) all IBM compilers
	   and then disable it for ones other than VisualAge C++ */
#if ( defined( __IBMC__ ) || defined( __IBMCPP__ ) ) && !defined( __VMCMS__ )
	tzset();
#endif /* VisualAge C++ */

	/* Initialise cryptlib */
	printf( "Initialising cryptlib..." );
	status = cryptInit();
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptInit() failed with error code %d, line %d.\n", 
				status, __LINE__ );
		exit( EXIT_FAILURE );
		}
	puts( "done." );

#ifndef TEST_RANDOM
	/* In order to avoid having to do a randomness poll for every test run,
	   we bypass the randomness-handling by adding some junk.  This is only
	   enabled when cryptlib is built in debug mode so it won't work with 
	   any production systems */
  #if defined( __MVS__ ) || defined( __VMCMS__ )
	#pragma convlit( resume )
	cryptAddRandom( "xyzzy", 5 );
	#pragma convlit( suspend )
  #else
	cryptAddRandom( "xyzzy", 5 );
  #endif /* Special-case EBCDIC handling */
#endif /* TEST_RANDOM */

	/* Perform a general sanity check to make sure that the self-test is
	   being run the right way */
	if( !checkFileAccess() )
		{
		cryptEnd();
		exit( EXIT_FAILURE );
		}

	/* Make sure that further system-specific features that require cryptlib 
	   to be initialised to check are set right */
#ifndef _WIN32_WCE
	testSystemSpecific2();
#endif /* WinCE */

#ifdef USE_TCHECK
	THREAD_DEBUG_RESUME();
#endif /* USE_TCHECK */

	/* For general testing purposes we can insert test code at this point to
	   test special cases that aren't covered in the general tests below */
	testKludge( zargPtr );

#ifdef SMOKE_TEST
	/* Perform a general smoke test of the kernel */
	smokeTest();
#endif /* SMOKE_TEST */

	/* Test each block of cryptlib functionality */
	if( ( flags & DO_SELFTEST ) && !testSelfTest() )
		goto errorExit;
	if( ( flags & DO_LOWLEVEL ) && !testLowLevel() )
		goto errorExit;
	if( ( flags & DO_RANDOM ) && !testRandom() )
		goto errorExit;
	if( ( flags & DO_CONFIG ) && !testConfig() )
		goto errorExit;
	if( ( flags & DO_DEVICE ) && !testDevice() )
		goto errorExit;
	if( ( flags & DO_MIDLEVEL ) && !testMidLevel() )
		goto errorExit;
	if( ( flags & DO_CERT ) && !testCert() )
		goto errorExit;
	if( ( flags & DO_KEYSETFILE ) && !testKeysetFile() )
		goto errorExit;
	if( ( flags & DO_KEYSETDBX ) && !testKeysetDatabase() )
		goto errorExit;
	if( ( flags & DO_CERTPROCESS ) && !testCertMgmt() )
		goto errorExit;
	if( ( flags & DO_HIGHLEVEL ) && !testHighLevel() )
		goto errorExit;
	if( ( flags & DO_ENVELOPE ) && !testEnveloping() )
		goto errorExit;
	if( ( flags & DO_SESSION ) && !testSessions() )
		{
		sessionTestError = TRUE;
		goto errorExit;
		}
	if( ( flags & DO_SESSIONLOOPBACK ) && !testSessionsLoopback() )
		{
		loopbackTestError = TRUE;
		goto errorExit;
		}
	if( ( flags & DO_USER  ) && !testUsers() )
		goto errorExit;

	/* Shut down cryptlib */
	status = cryptEnd();
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ERROR_INCOMPLETE )
			{
			puts( "cryptEnd() failed with error code CRYPT_ERROR_INCOMPLETE, "
				  "a code path in the\nself-test code resulted in an error "
				  "return without a full cleanup of objects.\nIf you were "
				  "running the multithreaded loopback tests this may be "
				  "because one\nor more threads lost sync with other threads "
				  "and exited without cleaning up\nits objects.  This "
				  "happens occasionally due to network timing issues or\n"
				  "thread scheduling differences." );
			}
		else
			{
			printf( "cryptEnd() failed with error code %d, line %d.\n", 
					status, __LINE__ );
			}
		goto errorExit1;
		}

	puts( "All tests concluded successfully." );
	return( EXIT_SUCCESS );

	/* All errors end up here */
errorExit:
	cryptEnd();
errorExit1:
	puts( "\nThe test was aborted due to an error being detected.  If you "
		  "want to report\nthis problem, please provide as much information "
		  "as possible to allow it to\nbe diagnosed, for example the call "
		  "stack, the location inside cryptlib where\nthe problem occurred, "
		  "and the values of any variables that might be\nrelevant." );
	if( sessionTestError )
		{
		puts( "\nThe error occurred during one of the network session tests, "
			  "if the error\nmessage indicates a network-level problem such "
			  "as ECONNREFUSED, ECONNRESET,\nor a timeout or read error then "
			  "this is either due to a transient\nnetworking problem or a "
			  "firewall interfering with network connections.  This\nisn't a "
			  "cryptlib error, and doesn't need to be reported." );
		}
	if( loopbackTestError )
		{
		puts( "\nThe error occurred during one of the multi-threaded network "
			  "loopback\ntests, this was probably due to the different threads "
			  "losing synchronisation.\nFor the secure sessions this usually "
			  "results in read/write, timeout, or\nconnection-closed errors "
			  "when one thread is pre-empted for too long.  For the\n"
			  "certificate-management sessions it usually results in an error "
			  "related to the\nserver being pre-empted for too long by database "
			  "updates.  Since the self-\ntest exists only to exercise "
			  "cryptlib's capabilities, it doesn't bother with\ncomplex thread "
			  "synchronisation during the multi-threaded loopback tests.\nThis "
			  "type of error is non-fatal, and should disappear if the test is "
			  "re-run." );
		}

	cleanExit( EXIT_FAILURE );
	return( EXIT_FAILURE );			/* Get rid of compiler warnings */
	}

/* PalmOS wrapper for main() */

#ifdef __PALMSOURCE__

#include <CmnErrors.h>
#include <CmnLaunchCodes.h>

uint32_t PilotMain( uint16_t cmd, void *cmdPBP, uint16_t launchFlags )
	{
	switch( cmd )
		{
		case sysAppLaunchCmdNormalLaunch:
			main( 0, NULL );
		}

	return( errNone );
	}
#endif /* __PALMSOURCE__ */

/* Symbian wrapper for main() */

#ifdef __SYMBIAN__

GLDEF_C TInt E32Main( void )
	{
	main( 0, NULL );

	return( KErrNone );
	}

#ifdef __WINS__

/* Support functions for use under the Windows emulator */

EXPORT_C TInt WinsMain( void )
	{
	E32Main();

	return( KErrNone );
	}

TInt E32Dll( TDllReason )
	{
	/* Entry point for the DLL loader */
	return( KErrNone );
	}
#endif /* __WINS__ */

#endif /* __SYMBIAN__ */

/* Test the system-specific defines in crypt.h.  This is the last function in
   the file because we want to avoid any definitions in crypt.h messing with
   the rest of the test.c code.

   The following include is needed only so we can check whether the defines
   are set right.  crypt.h should never be included in a program that uses
   cryptlib */

#undef __WINDOWS__
#undef __WIN16__
#undef __WIN32__
#undef BOOLEAN
#undef BYTE
#undef FALSE
#undef TRUE
#undef FAR_BSS
#if defined( __MVS__ ) || defined( __VMCMS__ )
  #pragma convlit( resume )
#endif /* Resume ASCII use on EBCDIC systems */
#if defined( __ILEC400__ )
  #pragma convert( 819 )
#endif /* Resume ASCII use on EBCDIC systems */
#ifdef _MSC_VER
  #include "../crypt.h"
#else
  #include "crypt.h"
#endif /* Braindamaged MSC include handling */
#if defined( __MVS__ ) || defined( __VMCMS__ )
  #pragma convlit( suspend )
#endif /* Suspend ASCII use on EBCDIC systems */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* Suspend ASCII use on EBCDIC systems */
#undef mktime		/* Undo mktime() bugfix in crypt.h */

#ifndef _WIN32_WCE

static time_t testTime( const int year )
	{
	struct tm theTime;

	memset( &theTime, 0, sizeof( struct tm ) );
	theTime.tm_isdst = -1;
	theTime.tm_year = 100 + year;
	theTime.tm_mon = 5;
	theTime.tm_mday = 5;
	theTime.tm_hour = 12;
	theTime.tm_min = 13;
	theTime.tm_sec = 14;
	return( mktime( &theTime ) );
	}
#endif /* !WinCE */

void testSystemSpecific1( void )
	{
#if 0	/* See comment below */
	const CRYPT_ATTRIBUTE_TYPE testType = -1;
#endif /* 0 */
	int bigEndian;
#ifndef _WIN32_WCE
	int i;
#endif /* WinCE */

	/* Make sure that we've got the endianness set right.  If the machine is
	   big-endian (up to 64 bits) the following value will be signed,
	   otherwise it will be unsigned.  We can't easily test for things like
	   middle-endianness without knowing the size of the data types, but
	   then again it's unlikely we're being run on a PDP-11 */
	bigEndian = ( *( long * ) "\x80\x00\x00\x00\x00\x00\x00\x00" < 0 );
#ifdef DATA_LITTLEENDIAN
	if( bigEndian )
		{
		puts( "The CPU endianness define is set wrong in crypt.h, this "
			  "machine appears to be\nbig-endian, not little-endian.  Edit "
			  "the file and rebuild cryptlib." );
		exit( EXIT_FAILURE );
		}
#else
	if( !bigEndian )
		{
		puts( "The CPU endianness define is set wrong in crypt.h, this "
			  "machine appears to be\nlittle-endian, not big-endian.  Edit "
			  "the file and rebuild cryptlib." );
		exit( EXIT_FAILURE );
		}
#endif /* DATA_LITTLEENDIAN */

	/* Make sure that the compiler doesn't use variable-size enums (done by, 
	   for example, the PalmOS SDK for backwards compatibility with 
	   architectural decisions made for 68K-based PalmPilots) */
	if( sizeof( CRYPT_ALGO_TYPE ) != sizeof( int ) || \
		sizeof( CRYPT_MODE_TYPE ) != sizeof( int ) ||
		sizeof( CRYPT_ATTRIBUTE_TYPE ) != sizeof( int ) )
		{
		puts( "The compiler you are using treats enumerated types as "
			  "variable-length non-\ninteger values, making it impossible "
			  "to reliably pass the address of an\nenum as a function "
			  "parameter.  To fix this you need to rebuild cryptlib\nwith "
			  "the appropriate compiler option or pragma to ensure that\n"
			  "sizeof( enum ) == sizeof( int )." );
		exit( EXIT_FAILURE );
		}

#if 0	/* The ANSI C default is 'int' with signedness being unimportant, 
		   MSVC defaults to signed, gcc defaults to unsigned, and cryptlib
		   works with either, so whatever the CodeSourcery build of gcc is
		   doing it's more serious than a simple signedness issue */
	/* Make sure that the compiler doesn't use unsigned enums (for example a 
	   mutant CodeSourcery build of gcc for eCos does this) */
	if( testType >= 0 )
		{
		puts( "The compiler you are using treats enumerated types as "
			  "unsigned values,\nmaking it impossible to reliably use enums "
			  "in conjunction with standard\n(signed) integers.  To fix this "
			  "you need to rebuild cryptlib with the\nappropriate compiler "
			  "option or pragma to ensure that enumerated types\nare signed "
			  "like standard data types." );
		exit( EXIT_FAILURE );
		}
#endif /* 0 */

	/* Make sure that mktime() works properly (there are some systems on
	   which it fails well before 2038) */
#ifndef _WIN32_WCE
	for( i = 10; i < 36; i ++ )
		{
		const time_t theTime = testTime( i );

		if( theTime < 0 )
			{
			printf( "Warning: This system has a buggy mktime() that can't "
					"handle dates beyond %d.\n         Some certificate tests "
					"will fail, and long-lived CA certificates\n         won't "
					"be correctly imported.\nPress a key...\n", 2000 + i );
			getchar();
			break;
			}
		}
#endif /* !WinCE */

	/* If we're compiling under Unix with threading support, make sure the
	   default thread stack size is sensible.  We don't perform the check for
	   UnixWare/SCO since this already has the workaround applied */
#if defined( UNIX_THREADS ) && !defined( __SCO_VERSION__ )
	{
	pthread_attr_t attr;
	size_t stackSize;

	pthread_attr_init( &attr );
	pthread_attr_getstacksize( &attr, &stackSize );
    pthread_attr_destroy( &attr );
  #if ( defined( sun ) && OSVERSION > 4 )
	/* Slowaris uses a special-case value of 0 (actually NULL) to indicate
	   the default stack size of 1MB (32-bit) or 2MB (64-bit), so we have to
	   handle this specially */
	if( stackSize < 32768 && stackSize != 0 )
  #else
	if( stackSize < 32768 )
  #endif /* Slowaris special-case handling */
		{
		printf( "The pthread stack size is defaulting to %ld bytes, which is "
				"too small for\ncryptlib to run in.  To fix this, edit the "
				"thread-creation function macro in\ncryptos.h and recompile "
				"cryptlib.\n", ( long ) stackSize );
		exit( EXIT_FAILURE );
		}
	}
#endif /* UNIX_THREADS */
	}

#ifndef _WIN32_WCE	/* Windows CE doesn't support ANSI C time functions */

void testSystemSpecific2( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	const time_t theTime = time( NULL ) - 5;
	int status;

	/* Make sure that the cryptlib and non-cryptlib code use the same time_t
	   size (some systems are moving from 32- to 64-bit time_t, which can 
	   lead to problems if the library and calling code are built with 
	   different sizes) */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ERROR_NOTAVAIL )
			{
			puts( "Couldn't create certificate object for time sanity-check "
				  "because certificate\n" "use has been disabled, skipping "
				  "time sanity check...\n" );
			return;
			}
		puts( "Couldn't create certificate object for time sanity-check." );
		exit( EXIT_FAILURE );
		}
	status = cryptSetAttributeString( cryptCert, CRYPT_CERTINFO_VALIDFROM,
									  &theTime, sizeof( time_t ) );
	cryptDestroyCert( cryptCert );
	if( status == CRYPT_ERROR_PARAM4 )
		{
		printf( "Warning: This compiler is using a %ld-bit time_t data type, "
				"which appears to be\n         different to the one that "
				"was used when cryptlib was built.  This\n         situation "
				"usually occurs when the compiler allows the use of both\n"
				"         32- and 64-bit time_t data types and different "
				"options were\n         selected for building cryptlib and "
				"the test app.  To resolve this,\n         ensure that both "
				"cryptlib and the code that calls it use the same\n"
				"         time_t data type.\n", 
				( long ) sizeof( time_t ) * 8 );
		exit( EXIT_FAILURE );
		}
	}
#endif /* WinCE */
