/****************************************************************************
*																			*
*						cryptlib Test Routines Header File					*
*						Copyright Peter Gutmann 1995-2012					*
*																			*
****************************************************************************/

/* Define the following to enable/disable various blocks of tests.  Note
   that the self-test uses a sizeable amount of automatic data, on 16-bit
   systems it'll probably be necessary to run each test individually rather
   than in groups as arranged below */

#if 1
#define TEST_SELFTEST		/* Perform internal self-test */
#define TEST_LOWLEVEL		/* Test low-level functions */
#define TEST_RANDOM			/* Test randomness functions */
#define TEST_CONFIG			/* Test configuration functions */
#define TEST_MIDLEVEL		/* Test high-level encr/sig.functions */
#endif /* 0 */
#if 1
#define TEST_CERT			/* Test certificate management functions */
#define TEST_KEYSET			/* Test keyset read functions */
#define TEST_CERTPROCESS	/* Test certificate handling/CA management */
#endif /* 0 */
#if 1
#define TEST_HIGHLEVEL		/* Test high-level encr/sig.functions */
#define TEST_ENVELOPE		/* Test enveloping functions */
#endif /* 0 */
#if 1
#define TEST_SESSION		/* Test session functions */
#define TEST_SESSION_LOOPBACK/* Test session functions via local loopback */
/*#define TEST_USER			// Test user management functions */
#endif /* 0 */

/* The crypto device tests are disabled by default since relatively few users
   will have a crypto device set up so leaving them enabled by default would
   just produce a cascade of device-not-present warnings */

/* #define TEST_DEVICE /**/
#if defined( TEST_DEVICE )
  #ifndef TEST_LOWLEVEL
	#define TEST_LOWLEVEL
  #endif /* TEST_LOWLEVEL */
  #ifndef TEST_ENVELOPE
	#define TEST_ENVELOPE
  #endif /* TEST_ENVELOPE */
#endif /* Low-level and envelope tests are called by the device tests */

/* Some of the device tests can be rather slow, the following defines disable
   these tests for speed reasons.  Note that the Fortezza test can be further
   cut down by not performing the CAW test (which erases any existing data on
   the card), this is turned off by default in testdev.c */

/* #define TEST_DEVICE_FORTEZZA */

/* DH and KEA can't be tested because they use cryptlib-internal mechanisms,
   however by using a custom-modified cryptlib it's possible to test at
   least part of the DH implementation.  If the following is defined, the
   DH key load will be tested */

/* #define TEST_DH */

/* To test the code under Windows CE:

	- If PB can't start the emulator, start it manually via Tools | Configure
	  Platform Manager | StandardSDK Emulator | Properties | Test.
	- Before running the self-test for the first time, from the emulator
	  select Folder Sharing, share the test subdirectory, which will appear
	  as \\Storage Card\ (sharing it while an app is running may crash the
	  emulator).
	- If eVC++ can't connect to the emulator, enable the WCE Config toolbar,
	  frob all the settings (which have only one option anyway).  VC++ will
	  rebuild everything (with exactly the same settings as before), and
	  then it'll work.
	- Only cl32ce.dll can be run in the debugger, test32ce.exe fails with
	  some unknown error code.
	- To test the randomness polling in the emulated environment, first run
	  the Remote Kernel Tracker, which installs the ToolHelp DLL (this isn't
	  installed by default) */

/* When commenting out code for testing, the following macro displays a
   warning that the behaviour has been changed as well as the location of
   the change */

#if defined( __MVS__ ) || defined( __VMCMS__ ) || defined( __ILEC400__ )
  #define KLUDGE_WARN( str )	\
			{ \
			char fileName[ 1000 ]; \
			strncpy( fileName, __FILE__, 1000 ); \
			fileName[ 999 ] = '\0'; \
			__atoe( fileName ); \
			printf( "Kludging " str ", file %s, line %d.\n", fileName, __LINE__ ); \
			}
#else
  #define KLUDGE_WARN( str )	\
			printf( "Kludging " str ", file " __FILE__ ", line %d.\n", __LINE__ );
#endif /* ASCII vs.EBCDIC strings */

/* Include univerally-needed headers */

#if defined( _WIN32_WCE ) && _WIN32_WCE < 400
  #define assert( x )
#else
  #include <assert.h>
#endif /* Systems without assert() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Various useful types */

#define BOOLEAN	int
#define BYTE	unsigned char
#ifndef TRUE
  #define FALSE	0
  #define TRUE	!FALSE
#endif /* TRUE */

/* Sentinel value used to denote non-data/non-values */

#define SENTINEL		-1000

/* A dummy initialistion value used to deal with false-positive compiler 
   warnings */

#ifndef DUMMY_INIT
  #define DUMMY_INIT	0
#endif /* DUMMY_INIT */

/* There are a few OSes broken enough not to define the standard exit codes
   (SunOS springs to mind) so we define some sort of equivalent here just
   in case */

#ifndef EXIT_SUCCESS
  #define EXIT_SUCCESS	0
  #define EXIT_FAILURE	!EXIT_SUCCESS
#endif /* EXIT_SUCCESS */

/* Although min() and max() aren't in the ANSI standard, most compilers have
   them in one form or another, but just enough don't that we need to define 
   them ourselves in some cases */

#if !defined( min )
  #ifdef MIN
	#define min		MIN
	#define max		MAX
  #else
	#define min( a, b )	( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
	#define max( a, b )	( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
  #endif /* Various min/max macros */
#endif /* !min/max */

/* If we're using a 16-bit compiler, record this */

#if defined( __MSDOS__ ) && !defined( __MSDOS32__ )
  #define __MSDOS16__
#endif /* 16-bit DOS */
#if defined( _MSC_VER ) && ( _MSC_VER <= 800 )
  #define __WIN16__
#endif /* 16-bit Windows */

/* It's useful to know if we're running under Windows to enable Windows-
   specific processing */

#if defined( _WINDOWS ) || defined( WIN32 ) || defined( _WIN32 ) || \
	defined( _WIN32_WCE )
  #define __WINDOWS__
#endif /* _WINDOWS || WIN32 || _WIN32 */

/* If we're running in an environment with a Unicode API, we have to be able
   to function with both Unicode and ASCII strings */

#ifdef __WINDOWS__
  #if defined( _WIN32_WCE )
	#undef TEXT
	#define TEXT( x )				L ## x
	#define paramStrlen( x )		( wcslen( x ) * sizeof( wchar_t ) )
	#define paramStrcmp( x, y )		wcscmp( x, y )
	#define UNICODE_STRINGS
  #elif ( defined( WIN32 ) || defined( _WIN32 ) ) && 0
	/* Facility to test WinCE Unicode handling under Win32 */
	#undef TEXT
	#define TEXT( x )				L ## x
	#define paramStrlen( x )		( wcslen( x ) * sizeof( wchar_t ) )
	#define paramStrcmp( x, y )		wcscmp( x, y )
	#define UNICODE_STRINGS
  #else
	#undef TEXT						/* Already defined in windows.h */
	#define TEXT( x )				x
	#define paramStrlen( x )		strlen( x )
	#define paramStrcmp( x, y )		strcmp( x, y )
  #endif /* Windows variants */
#else
  #define TEXT( x )					x
  #define paramStrlen( x )			strlen( x )
  #define paramStrcmp( x, y )		strcmp( x, y )
#endif /* Unicode vs. ASCII API */

/* In certain memory-starved environments we have to kludge things to help
   the compiler along.  The following define tells the compiler to move BSS
   data outside the default data segment */

#if defined( _MSC_VER ) && ( _MSC_VER <= 800 )
  #define FAR_BSS			far
#else
  #define FAR_BSS
#endif /* Win16 */

/* VS 2005 and newer warn if we use non-TR 24731 stdlib functions, since 
   this is only for the test code we disable the warnings.  In addition
   VS in 64-bit mode warns about size_t (e.g. from calling strlen()) <-> 
   int conversion */

#if defined( _MSC_VER ) && ( _MSC_VER >= 1400 )
  #pragma warning( disable: 4267 )	/* int <-> size_t */
  #pragma warning( disable: 4996 )	/* Non-TR 24731 stdlib use */
#endif /* VC++ 2005 or newer */

/* Generic buffer size and dynamically-allocated file I/O buffer size.  The
   generic buffer has to be of a reasonable size so that we can handle 
   S/MIME signature chains, the file buffer should be less than the 16-bit 
   INT_MAX for testing on 16-bit machines and less than 32K for testing on 
   the (16-bit DOS-derived) Watcom C 10 */

#if defined( __MSDOS16__ ) || defined( __WIN16__ )
  #define BUFFER_SIZE			4096
  #define FILEBUFFER_SIZE		20000
#elif defined( __QNX__ ) && defined( __WATCOMC__ ) && ( __WATCOMC__ < 1100 )
  #define BUFFER_SIZE			8192
  #define FILEBUFFER_SIZE		20000
#else
  #define BUFFER_SIZE			16384
  #define FILEBUFFER_SIZE		32768
#endif /* __MSDOS__ && __TURBOC__ */
#define FILENAME_BUFFER_SIZE	512

/* Explicit includes needed by Palm OS, see the comment in crypt.h for more
   details */

#ifdef __PALMSOURCE__
  #include <ctype.h>
  #include <string.h>
#endif /* __PALMSOURCE__ */

/* The ability to get rid of annoying warnings via the project file in BC++
   5.0x is completely broken, the only way to do this is via pragmas in the
   source code */

#if defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x550 )
  #pragma warn -ucp						/* Signed/unsigned char assignment */
#endif /* Broken BC++ 5.0x warning handling */

/* Helper function to make tracking down errors on systems with no console a
   bit less painful */

#ifdef _WIN32_WCE
  #define printf	wcPrintf
  #define puts		wcPuts

  void wcPrintf( const char *format, ... );
  void wcPuts( const char *string );
#endif /* Console-less environments */

/* Try and detect OSes that have threading support, this is needed for some
   operations like async keygen and sleep calls */

#if( ( defined( _AIX ) || defined( __APPLE__ ) || defined( __FreeBSD__ ) || \
	   defined( __NetBSD__ ) || defined( __linux__ ) || \
	   ( defined( sun ) && ( OSVERSION > 4 ) ) ) && !defined( NO_THREADS ) )
  #define UNIX_THREADS

  /* We need to include pthread.h at this point because any number of other
     include files perform all sorts of peculiar and unnatural acts in order
     to make their functions (transparently) thread-safe, triggered by the
     detection of values defined in pthread.h.  Because of this we need to
     include it here as a rubber chicken before other files are pulled in
     even though it's not explicitly needed */
  #include <pthread.h>
#endif /* AIX || OS/X || Linux || Slowaris */
#if ( defined( WIN32 ) || defined( _WIN32 ) ) && !defined( _WIN32_WCE )
  /* We don't test the loopback functionality under WinCE because the
	 _beginthreadx() vs. CreateThread() issue (normally hidden in
	 cryptos.h) causes all sorts of problems */
  #define WINDOWS_THREADS
  #include <process.h>
#endif /* Win32 */
#if defined( __IBMC__ ) && defined( __OS2__ )
  #define OS2_THREADS
#endif /* OS/2 */

/* The loopback sessions require threading support so we only enable their
   use if this is present */

#if defined( TEST_SESSION_LOOPBACK ) && !defined( WINDOWS_THREADS )
  #undef TEST_SESSION_LOOPBACK
#endif /* OSes with threading support */

/* Define various portable data types and functions needed for the threaded
   loopback tests */

#if defined( WINDOWS_THREADS )
  #define THREAD_HANDLE		HANDLE
  #define THREAD_EXIT()		_endthreadex( 0 ); return( 0 )
  #define THREAD_SELF()		GetCurrentThreadId()
  typedef unsigned ( __stdcall *THREAD_FUNC )( void *arg );
#elif defined( UNIX_THREADS )
  #define THREAD_HANDLE		pthread_t
  #define THREAD_EXIT()		pthread_exit( ( void * ) 0 ); return( 0 )
  #define THREAD_SELF()		pthread_self()
  typedef void * ( *THREAD_FUNC )( void *arg );
#else
  #define THREAD_HANDLE		int
  #define THREAD_EXIT()
  #define THREAD_SELF()		0
  typedef void ( *THREAD_FUNC )( void *arg );
#endif /* OS-specific threading functions */

/* The maximum number of threads that we can fire up in the multithreaded 
   tests */

#define MAX_NO_THREADS		10

/* Try and detect OSes that have widechar support */

#if ( defined( __WINDOWS__ ) && \
	  !( defined( __WIN16__ ) || defined( __BORLANDC__ ) ) )
  #define HAS_WIDECHAR
#endif /* Windows with widechar support */
#if defined( __linux__ ) || \
	( defined( sun ) && ( OSVERSION > 4 ) ) || defined( __osf__ )
  #include <wchar.h>
  #define HAS_WIDECHAR
#endif /* Unix with widechar support */

/* If we're running on an EBCDIC system, ensure that we're compiled in 
   EBCDIC mode to test the conversion of character strings */

#if defined( __MVS__ ) || defined( __VMCMS__ )
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */

/* If we're compiling under QNX, make enums a fixed size rather than using
   the variable-length values that the Watcom compiler defaults to */

#if defined( __QNX__ ) && defined( __WATCOMC__ )
  #pragma enum int
#endif /* QNX and Watcom C */

/* The key size to use for the PKC routines.  This is the minimum allowed by
   cryptlib, it speeds up the various tests but shouldn't be used in
   practice */

#define PKC_KEYSIZE			512

/* Since the handling of filenames can get unwieldy when we have large
   numbers of similar files, we use a function to map a filename template
   and number into an actual filename rather the having to use huge
   numbers of defines */

#ifdef UNICODE_STRINGS
  void filenameFromTemplate( char *buffer, const wchar_t *fileTemplate,
							 const int count );
  void filenameParamFromTemplate( wchar_t *buffer,
								  const wchar_t *fileTemplate,
								  const int count );
  const char *convertFileName( const C_STR fileName );
#else
  #define filenameFromTemplate( buffer, fileTemplate, count ) \
		  sprintf( buffer, fileTemplate, count )
  #define filenameParamFromTemplate( buffer, fileTemplate, count ) \
		  sprintf( buffer, fileTemplate, count )
  #define convertFileName( fileName )	fileName
#endif /* Unicode vs. ASCII */

/* Helper functions for limited environments that are missing full stdlib
   support */

#if defined( _WIN32_WCE ) && _WIN32_WCE < 500
  int remove( const char *pathname );
#endif /* WinCE < 5.x */

/* A structure that allows us to specify a collection of extension
   components.  This is used when adding a collection of extensions to a
   cert */

typedef enum { IS_VOID, IS_NUMERIC, IS_STRING, IS_WCSTRING,
			   IS_TIME } COMPONENT_TYPE;

typedef struct {
	const CRYPT_ATTRIBUTE_TYPE type;/* Extension component ID */
	const COMPONENT_TYPE componentType;	/* Component type */
	const int numericValue;			/* Value if numeric */
	const void *stringValue;		/* Value if string */
	const time_t timeValue;			/* Value if time */
	} CERT_DATA;

/****************************************************************************
*																			*
*									Naming									*
*																			*
****************************************************************************/

/* Pull in the OS-specific file names for the test data */

#ifdef _MSC_VER
  #include "filename.h"
#else
  #include "test/filename.h"
#endif /* Braindamaged MSC include handling */

/* When we're using common code to handle a variety of key file types for
   key read/encryption/signing tests, we need to distinguish between the
   different key files to use.  The following types are handled in the test
   code */

typedef enum { KEYFILE_NONE, KEYFILE_X509, KEYFILE_PGP, KEYFILE_PGP_SPECIAL,
			   KEYFILE_OPENPGP_HASH, KEYFILE_OPENPGP_AES, 
			   KEYFILE_OPENPGP_CAST, KEYFILE_OPENPGP_RSA, 
			   KEYFILE_OPENPGP_MULT, KEYFILE_NAIPGP, 
			   KEYFILE_OPENPGP_PARTIAL } KEYFILE_TYPE;

/* The generic password used for password-based encryption, and another one 
   for private key storage */

#define TEST_PASSWORD			TEXT( "password" )
#define TEST_PRIVKEY_PASSWORD	TEXT( "test" )

/* The database keyset type and name.  Under Windoze we use ODBC, for
   anything else we use the first database which is enabled by a preprocessor
   define, defaulting to an internal plugin (which doesn't have to be
   available, if it's not present we continue after printing a warning) */

#if defined( _MSC_VER )
  #define DATABASE_KEYSET_TYPE	CRYPT_KEYSET_ODBC
  #define CERTSTORE_KEYSET_TYPE	CRYPT_KEYSET_ODBC_STORE
#elif defined( DBX_DATABASE )
  #define DATABASE_KEYSET_TYPE	CRYPT_KEYSET_DATABASE
  #define CERTSTORE_KEYSET_TYPE	CRYPT_KEYSET_DATABASE_STORE
#elif defined( DBX_PLUGIN )
  #define DATABASE_KEYSET_TYPE	CRYPT_KEYSET_PLUGIN
  #define CERTSTORE_KEYSET_TYPE	CRYPT_KEYSET_PLUGIN_STORE
#else
  #define DATABASE_KEYSET_TYPE	CRYPT_KEYSET_DATABASE
  #define CERTSTORE_KEYSET_TYPE	CRYPT_KEYSET_DATABASE_STORE
#endif /* Various database backends */
#define DATABASE_KEYSET_NAME		TEXT( "testkeys" )
#define DATABASE_KEYSET_NAME_ASCII	"testkeys"
#define CERTSTORE_KEYSET_NAME		TEXT( "testcertstore" )
#define CERTSTORE_KEYSET_NAME_ASCII	"testcertstore"
#define DATABASE_PLUGIN_KEYSET_NAME	TEXT( "localhost:6500" )
#define DATABASE_PLUGIN_KEYSET_NAME_ASCII	"localhost:6500"

/* The HTTP keyset names (actually URLs for pages containing a cert and
   CRL).  We can't get either Verisign or Thawte root certs because both
   require you to provide all sorts of personal information and click on a
   legal agreement before you can download them (!!!), so we use the CAcert
   root instead */

#define HTTP_KEYSET_CERT_NAME	TEXT( "www.cacert.org/certs/root.der" )
#define HTTP_KEYSET_CRL_NAME	TEXT( "crl.verisign.com/Class1Individual.crl" )
#define HTTP_KEYSET_HUGECRL_NAME TEXT( "crl.verisign.com/RSASecureServer.crl" )

/* Assorted default server names and authentication information, and the PKI
   SRV server (redirecting to mail.cryptoapps.com:8080).  There are so many
   TSP, OCSP, and CMP servers, and they never stay around for long, that we
   allow remapping in the functions where the secure session tests are
   performed */

#define SSH_USER_NAME			TEXT( "test" )
#define SSH_PASSWORD			TEXT( "test" )
#define SSL_USER_NAME			TEXT( "test" )
#define SSL_PASSWORD			TEXT( "test" )
#define PKI_SRV_NAME			TEXT( "_pkiboot._tcp.cryptoapps.com" )
#define TSP_DEFAULTSERVER_NAME	TEXT( "http://timestamping.edelweb.fr/service/tsp" )

/* Labels for the various public-key objects.  These are needed when the
   underlying implementation creates persistent objects (eg keys held in PKCS
   #11 tokens) that need to be identified */

#define RSA_PUBKEY_LABEL		TEXT( "Test RSA public key" )
#define RSA_PRIVKEY_LABEL		TEXT( "Test RSA private key" )
#define RSA_BIG_PRIVKEY_LABEL	TEXT( "Test RSA big private key" )
#define DSA_PUBKEY_LABEL		TEXT( "Test DSA sigcheck key" )
#define DSA_PRIVKEY_LABEL		TEXT( "Test DSA signing key" )
#define ELGAMAL_PUBKEY_LABEL	TEXT( "Test Elgamal public key" )
#define ELGAMAL_PRIVKEY_LABEL	TEXT( "Test Elgamal private key" )
#define DH_KEY1_LABEL			TEXT( "Test DH key #1" )
#define DH_KEY2_LABEL			TEXT( "Test DH key #2" )
#define ECDSA_PUBKEY_LABEL		TEXT( "Test ECDSA sigcheck key" )
#define ECDSA_PRIVKEY_LABEL		TEXT( "Test ECDSA signing key" )
#define CA_PRIVKEY_LABEL		TEXT( "Test RSA private key" )
#define USER_PRIVKEY_LABEL		TEXT( "Test user key" )
#define USER_EMAIL				TEXT( "dave@wetaburgers.com" )
#define DUAL_SIGNKEY_LABEL		TEXT( "Test signing key" )
#define DUAL_ENCRYPTKEY_LABEL	TEXT( "Test encryption key" )
#define SYMMETRIC_KEY_LABEL		TEXT( "Test symmetric key" )

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Prototypes for functions in utils.c */

#if defined( UNIX_THREADS ) || defined( WINDOWS_THREADS ) || defined( OS2_THREADS )
  void delayThread( const int seconds );
#else
  #define delayThread( x )
#endif /* Systems with threading support */
CRYPT_ALGO_TYPE selectCipher( const CRYPT_ALGO_TYPE algorithm );
void printHex( const BYTE *value, const int length );
const C_STR getKeyfileName( const KEYFILE_TYPE type,
							const BOOLEAN isPrivKey );
const C_STR getKeyfilePassword( const KEYFILE_TYPE type );
const C_STR getKeyfileUserID( const KEYFILE_TYPE type,
							  const BOOLEAN isPrivKey );
void printErrorAttributeInfo( const CRYPT_CERTIFICATE certificate );
int displayAttributes( const CRYPT_HANDLE cryptHandle );
int printCertInfo( const CRYPT_CERTIFICATE certificate );
int printCertChainInfo( const CRYPT_CERTIFICATE certChain );
void printExtError( const CRYPT_HANDLE cryptHandle,
					const char *functionName, const int functionStatus,
					const int lineNo );
int importCertFile( CRYPT_CERTIFICATE *cryptCert, const C_STR fileName );
int importCertFromTemplate( CRYPT_CERTIFICATE *cryptCert,
							const C_STR fileTemplate, const int number );
int addCertFields( const CRYPT_CERTIFICATE certificate,
				   const CERT_DATA *certData, const int lineNo );
int checkFileAccess( void );
int checkNetworkAccess( void );
int compareData( const void *origData, const int origDataLength,
				 const void *recovData, const int recovDataLength );
int getPublicKey( CRYPT_CONTEXT *cryptContext, const C_STR keysetName,
				  const C_STR keyName );
int getPrivateKey( CRYPT_CONTEXT *cryptContext, const C_STR keysetName,
				   const C_STR keyName, const C_STR password );
void debugDump( const char *fileName, const void *data,
				const int dataLength );
int printConnectInfo( const CRYPT_SESSION cryptSession );
int printSecurityInfo( const CRYPT_SESSION cryptSession,
					   const BOOLEAN isServer,
					   const BOOLEAN showFingerprint,
					   const BOOLEAN showServerKeyInfo,
					   const BOOLEAN showClientCertInfo );
int printFingerprint( const CRYPT_SESSION cryptSession,
					  const BOOLEAN isServer );
BOOLEAN setLocalConnect( const CRYPT_SESSION cryptSession, const int port );
BOOLEAN isServerDown( const CRYPT_SESSION cryptSession,
					  const int errorStatus );
int activatePersistentServerSession( const CRYPT_SESSION cryptSession,
									 const BOOLEAN showOperationType );

/* Threading support functions, in utils.c */

void createMutex( void );
void acquireMutex( void );
void releaseMutex( void );
int waitMutex( void );
void destroyMutex( void );
#if defined( WINDOWS_THREADS )
  void waitForThread( const HANDLE hThread );
#elif defined( UNIX_THREADS )
  void waitForThread( const pthread_t hThread );
#else
  void waitForThread( const int hThread );
#endif /* Systems with threading support */
int multiThreadDispatch( THREAD_FUNC clientFunction,
						 THREAD_FUNC serverFunction, const int noThreads );

/* Exit with an error message, in utils.c.  attrErrorExit() prints the
   locus and type, extErrorExit() prints the extended error code and
   message */

BOOLEAN attrErrorExit( const CRYPT_HANDLE cryptHandle,
					   const char *functionName, const int errorCode,
					   const int lineNumber );
BOOLEAN extErrorExit( const CRYPT_HANDLE cryptHandle,
					  const char *functionName, const int errorCode,
					  const int lineNumber );

/* Prototypes for functions in certs.c */

BOOLEAN certErrorExit( const CRYPT_HANDLE cryptHandle,
					   const char *functionName, const int errorCode,
					   const int lineNumber );
int setRootTrust( const CRYPT_CERTIFICATE cryptCertChain,
				  BOOLEAN *oldTrustValue, const BOOLEAN newTrustValue );

/* Prototypes for functions in lowlvl.c */

BOOLEAN loadDHKey( const CRYPT_DEVICE cryptDevice,
				   CRYPT_CONTEXT *cryptContext );
BOOLEAN loadRSAContextsEx( const CRYPT_DEVICE cryptDevice,
						   CRYPT_CONTEXT *cryptContext,
						   CRYPT_CONTEXT *decryptContext,
						   const C_STR cryptContextLabel,
						   const C_STR decryptContextLabel,
						   const BOOLEAN useLargeKey,
						   const BOOLEAN useMinimalKey );
BOOLEAN loadRSAContexts( const CRYPT_DEVICE cryptDevice,
						 CRYPT_CONTEXT *cryptContext,
						 CRYPT_CONTEXT *decryptContext );
BOOLEAN loadRSAContextsLarge( const CRYPT_DEVICE cryptDevice,
							  CRYPT_CONTEXT *cryptContext,
							  CRYPT_CONTEXT *decryptContext );
BOOLEAN loadDSAContextsEx( const CRYPT_DEVICE cryptDevice,
						   CRYPT_CONTEXT *sigCheckContext,
						   CRYPT_CONTEXT *signContext,
						   const C_STR sigCheckContextLabel,
						   const C_STR signContextLabel );
BOOLEAN loadDSAContexts( const CRYPT_DEVICE cryptDevice,
						 CRYPT_CONTEXT *sigCheckContext,
						 CRYPT_CONTEXT *signContext );
BOOLEAN loadElgamalContexts( CRYPT_CONTEXT *cryptContext,
							 CRYPT_CONTEXT *decryptContext );
BOOLEAN loadDHContexts( CRYPT_CONTEXT *cryptContext1,
						CRYPT_CONTEXT *cryptContext2 );
BOOLEAN loadECDSAContexts( CRYPT_CONTEXT *sigCheckContext,
						   CRYPT_CONTEXT *signContext );
void destroyContexts( const CRYPT_DEVICE cryptDevice,
					  CRYPT_CONTEXT cryptContext,
					  CRYPT_CONTEXT decryptContext );
int testLowlevel( const CRYPT_DEVICE cryptDevice,
				  const CRYPT_ALGO_TYPE cryptAlgo,
				  const BOOLEAN checkOnly );
int testCrypt( CRYPT_CONTEXT cryptContext, CRYPT_CONTEXT decryptContext,
			   BYTE *buffer, const BOOLEAN isDevice,
			   const BOOLEAN noWarnFail );
int testRSAMinimalKey( void );

/* Prototypes for functions in envelope.c */

int testEnvelopePKCCryptEx( const CRYPT_CONTEXT cryptContext, 
							const CRYPT_HANDLE decryptKeyset );
int testCMSEnvelopeSignEx( const CRYPT_CONTEXT signContext );
int testCMSEnvelopePKCCryptEx( const CRYPT_HANDLE encryptContext,
							   const CRYPT_HANDLE decryptKeyset,
							   const C_STR password, const C_STR recipient );

/* Prototypes for functions in sreqresp.c */

int testSessionTSPServerEx( const CRYPT_CONTEXT privKeyContext );

/* Prototypes for functions in s_scep.c */

int pkiGetUserInfo( C_STR userID, C_STR issuePW, C_STR revPW, 
					const C_STR userName );
int pkiServerInit( CRYPT_CONTEXT *cryptPrivateKey, 
				   CRYPT_KEYSET *cryptCertStore, const C_STR keyFileName,
				   const C_STR keyLabel, const CERT_DATA *pkiUserData,
				   const CERT_DATA *pkiUserAltData, 
				   const CERT_DATA *pkiUserCAData, const char *protocolName );

/****************************************************************************
*																			*
*								Timing Support								*
*																			*
****************************************************************************/

/* Since high-precision timing is rather OS-dependent, we only enable this
   under Windows or Unix where we've got guaranteed high-res timer access */

#if ( defined( __WINDOWS__ ) && defined( _MSC_VER ) ) || \
	( defined( __UNIX__ ) && defined( __GNUC__ ) )

#define USE_TIMING

/* Normally we use 32-bit time values, however there's also (partial)
   support for 64-bit signed values on systems that support this.  This
   isn't normally needed though because timeDiff() deals with overflow/
   wraparound */

#define USE_32BIT_TIME

#if defined( USE_32BIT_TIME )
  typedef unsigned long HIRES_TIME;
  #define HIRES_FORMAT_SPECIFIER	"%lX"
#elif defined( _MSC_VER )
  typedef __int64 HIRES_TIME;
  #define HIRES_FORMAT_SPECIFIER	"%llX"
#elif defined( __GNUC__ )
  typedef long long HIRES_TIME;
  #define HIRES_FORMAT_SPECIFIER	"%llX"
#else
  typedef unsigned long HIRES_TIME;
  #define HIRES_FORMAT_SPECIFIER	"%lX"
#endif /* 32/64-bit time values */

/* Timing support functions.  Call as:

	HIRES_TIME timeVal;

	timeVal = timeDiff( 0 );
	function_to_time();
	result = ( int ) timeDiff( timeVal ); */

HIRES_TIME timeDiff( HIRES_TIME startTime );

#endif /* Windows with MSVC or Unix with gcc */

/****************************************************************************
*																			*
*								Test Functions								*
*																			*
****************************************************************************/

/* Prototypes for functions in highlvl.c */

int testLargeBufferEncrypt( void );
int testDeriveKey( void );
int testRandomRoutines( void );
int testConventionalExportImport( void );
int testMACExportImport( void );
int testKeyExportImport( void );
int testSignData( void );
int testKeygen( void );
int testKeygenAsync( void );
int testKeyExportImportCMS( void );
int testSignDataCMS( void );

/* Prototypes for functions in devices.c */

int testDevices( void );
int testUser( void );

/* Prototypes for functions in keyfile.c */

int testGetPGPPublicKey( void );
int testGetPGPPrivateKey( void );
int testReadWriteFileKey( void );
int testReadWriteAltFileKey( void );
int testImportFileKey( void );
int testReadFilePublicKey( void );
int testAddTrustedCert( void );
int testAddGloballyTrustedCert( void );
int testDeleteFileKey( void );
int testChangeFileKeyPassword( void );
int testUpdateFileCert( void );
int testWriteFileCertChain( void );
int testWriteFileLongCertChain( void );
int testReadFileCert( void );
int testReadFileCertPrivkey( void );
int testReadFileCertChain( void );
int testReadCorruptedKey( void );
int testSingleStepFileCert( void );
int testSingleStepAltFileCert( void );
int testDoubleCertFile( void );
int testRenewedCertFile( void );
int testReadAltFileKey( void );
int testReadMiscFile( void );

/* Prototypes for functions in keydbx.c */

int testWriteCert( void );
int testReadCert( void );
int testKeysetQuery( void );
int testWriteCertDbx( void );
int testWriteCertLDAP( void );
int testReadCertLDAP( void );
int testReadCertURL( void );
int testReadCertHTTP( void );

/* Prototypes for functions in envelope.c */

int testEnvelopeData( void );
int testEnvelopeDataLargeBuffer( void );
int testEnvelopeCompress( void );
int testPGPEnvelopeCompressedDataImport( void );
int testEnvelopeSessionCrypt( void );
int testEnvelopeSessionCryptLargeBuffer( void );
int testEnvelopeCrypt( void );
int testEnvelopePasswordCrypt( void );
int testEnvelopePasswordCryptBoundary( void );
int testEnvelopePasswordCryptImport( void );
int testPGPEnvelopePasswordCryptImport( void );
int testEnvelopePKCCrypt( void );
int testEnvelopePKCCryptAlgo( void );
int testPGPEnvelopePKCCryptImport( void );
int testEnvelopeSign( void );
int testEnvelopeSignAlgo( void );
int testEnvelopeSignOverflow( void );
int testPGPEnvelopeSignedDataImport( void );
int testEnvelopeAuthenticate( void );
int testEnvelopeAuthEnc( void );
int testCMSEnvelopePKCCrypt( void );
int testCMSEnvelopePKCCryptDoubleCert( void );
int testCMSEnvelopePKCCryptImport( void );
int testCMSEnvelopeSign( void );
int testCMSEnvelopeDualSign( void );
int testCMSEnvelopeDetachedSig( void );
int testCMSEnvelopeSignedDataImport( void );

/* Prototypes for functions in certs.c */

int testBasicCert( void );
int testCACert( void );
int testXyzzyCert( void );
int testTextStringCert( void );
int testComplexCert( void );
int testCertExtension( void );
int testCustomDNCert( void );
int testCertAttributeHandling( void );
int testSETCert( void );
int testAttributeCert( void );
int testCRL( void );
int testComplexCRL( void );
int testCertChain( void );
int testCertRequest( void );
int testCertRequestAttrib( void );
int testComplexCertRequest( void );
int testCRMFRequest( void );
int testComplexCRMFRequest( void );
int testRevRequest( void );
int testCMSAttributes( void );
int testRTCSReqResp( void );
int testOCSPReqResp( void );
int testPKIUser( void );
int testCertImport( void );
int testCertImportECC( void );
int testCertReqImport( void );
int testCRLImport( void );
int testCertChainImport( void );
int testOCSPImport( void );
int testBase64CertImport( void );
int testBase64CertChainImport( void );
int testMiscImport( void );
int testNonchainCert( void );
int testCertComplianceLevel( void );
int testPathProcessing( void );
int testPKCS1Padding( void );
int testCertProcess( void );
int testCertManagement( void );

/* Prototypes for functions in scert.c (the EnvTSP one is actually in with
   the enveloping code because the only way to fully exercise the TS
   functionality is by using it to timestamp an S/MIME signature) */

int testSessionSCEP( void );
int testSessionSCEPCACert( void );
int testSessionSCEPServer( void );
int testSessionCMP( void );
int testSessionCMPServer( void );
int testSessionPNPPKI( void );
int testSessionEnvTSP( void );

/* Prototypes for functions in sreqresp.c */

int testSessionHTTPCertstoreServer( void );
int testSessionRTCS( void );
int testSessionRTCSServer( void );
int testSessionOCSP( void );
int testSessionOCSPServer( void );
int testSessionTSP( void );
int testSessionTSPServer( void );

/* Prototypes for functions in ssh.c */

int testSessionUrlParse( void );
int testSessionAttributes( void );
int testSessionSSHv1( void );
int testSessionSSH( void );
int testSessionSSHClientCert( void );
int testSessionSSHPortforward( void );
int testSessionSSHExec( void );
int testSessionSSH_SFTP( void );
int testSessionSSHv1Server( void );
int testSessionSSHServer( void );
int testSessionSSH_SFTPServer( void );

/* Prototypes for functions in ssl.c */

int testSessionSSL( void );
int testSessionSSLLocalSocket( void );
int testSessionSSLClientCert( void );
int testSessionSSLSharedKey( void );
int testSessionSSLServer( void );
int testSessionSSLServerCached( void );
int testSessionSSLServerClientCert( void );
int testSessionTLS( void );
int testSessionTLSServer( void );
int testSessionTLSServerSharedKey( void );
int testSessionTLS11( void );
int testSessionTLS11Server( void );
int testSessionTLS12( void );
int testSessionTLS12ClientCert( void );

/* Functions to test local client/server sessions.  These require threading
   support since they run the client and server in different threads */

#ifdef TEST_SESSION_LOOPBACK
  int testSessionSSHv1ClientServer( void );
  int testSessionSSHClientServer( void );
  int testSessionSSHClientServerDsaKey( void );
  int testSessionSSHClientServerEccKey( void );
  int testSessionSSHClientServerFingerprint( void );
  int testSessionSSHClientServerSFTP( void );
  int testSessionSSHClientServerPortForward( void );
  int testSessionSSHClientServerExec( void );
  int testSessionSSHClientServerMultichannel( void );
  int testSessionSSHClientServerDualThread( void );
  int testSessionSSHClientServerMultiThread( void );
  int testSessionSSLClientServer( void );
  int testSessionSSLClientCertClientServer( void );
  int testSessionTLSClientServer( void );
  int testSessionTLSSharedKeyClientServer( void );
  int testSessionTLSNoSharedKeyClientServer( void );
  int testSessionTLSBulkTransferClientServer( void );
  int testSessionTLSResumeClientServer( void );
  int testSessionTLS11ClientServer( void );
  int testSessionTLS12ClientServer( void );
  int testSessionTLS12ClientServerEccKey( void );
  int testSessionTLS12ClientServerEcc384Key( void );
  int testSessionTLS12ClientCertClientServer( void );
  int testSessionTLSClientServerDualThread( void );
  int testSessionTLSClientServerMultiThread( void );
  int testSessionHTTPCertstoreClientServer( void );
  int testSessionRTCSClientServer( void );
  int testSessionOCSPClientServer( void );
  int testSessionTSPClientServer( void );
  int testSessionTSPClientServerPersistent( void );
  int testSessionSCEPClientServer( void );
  int testSessionSCEPSHA2ClientServer( void );
  int testSessionSCEPCACertClientServer( void );
  int testSessionCMPClientServer( void );
  int testSessionCMPSHA2ClientServer( void );
  int testSessionCMPPKIBootClientServer( void );
  int testSessionCMPFailClientServer( void );
  int testSessionPNPPKIClientServer( void );
  int testSessionPNPPKIDeviceClientServer( void );
  int testSessionPNPPKICAClientServer( void );
  int testSessionPNPPKIIntermedCAClientServer( void );
  int testSessionSuiteBClientServer( void );
#endif /* TEST_SESSION_LOOPBACK */

/* Umbrella tests for the above functions */

BOOLEAN testSelfTest( void );
BOOLEAN testLowLevel( void );
BOOLEAN testRandom( void );
BOOLEAN testConfig( void );
BOOLEAN testDevice( void );
BOOLEAN testMidLevel( void );
BOOLEAN testHighLevel( void );
BOOLEAN testCert( void );
BOOLEAN testCertMgmt( void );
BOOLEAN testKeysetFile( void );
BOOLEAN testKeysetDatabase( void );
BOOLEAN testEnveloping( void );
BOOLEAN testSessions( void );
BOOLEAN testSessionsLoopback( void );
BOOLEAN testUsers( void );

#if defined( __MVS__ ) || defined( __VMCMS__ )
  #pragma convlit( resume )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 819 )
#endif /* IBM medium iron */
