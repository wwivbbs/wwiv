/****************************************************************************
*																			*
*					  cryptlib Self-test Utility Routines					*
*						Copyright Peter Gutmann 1997-2011					*
*																			*
****************************************************************************/

#include <ctype.h>
#include "cryptlib.h"
#include "test/test.h"

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */

#ifdef HAS_WIDECHAR
  #include <wchar.h>
#endif /* HAS_WIDECHAR */

/* The keys used with the test code have associated certs that expire at
   some point.  The following value defines the number of days before the
   expiry at which we start printing warnings */

#if defined( _MSC_VER ) && ( _MSC_VER == 1200 ) && !defined( NDEBUG )
  #define EXPIRY_WARN_DAYS		90
#else
  #define EXPIRY_WARN_DAYS		30
#endif /* VC 6 debug/development, give some advance warning */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#ifndef _WIN32_WCE

/* Since ctime() adds a '\n' to the string and may return NULL, we wrap it
   in something that behaves as required */

static char *getTimeString( const time_t theTime, const int bufNo )
	{
	static char timeString[ 2 ][ 64 ], *timeStringPtr;

	assert( bufNo == 0 || bufNo == 1 );

	timeStringPtr = ctime( &theTime );
	if( timeStringPtr == NULL )
		return( "(Not available)" );
	strcpy( timeString[ bufNo ], timeStringPtr );
	timeString[ bufNo ][ strlen( timeStringPtr ) - 1 ] = '\0';	/* Stomp '\n' */

	return( timeString[ bufNo ] );
	}
#else
  #define getTimeString( theTime, bufNo )	"(No time data available)"
#endif /* _WIN32_WCE */

#if defined( _WIN32_WCE ) && _WIN32_WCE < 500

int remove( const char *pathname )
	{
	wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];

	mbstowcs( wcBuffer, pathname, strlen( pathname ) + 1 );
	DeleteFile( wcBuffer );

	return( 0 );
	}
#endif /* WinCE < 5.x */

/****************************************************************************
*																			*
*							General Checking Functions						*
*																			*
****************************************************************************/

/* Windows-specific file accessibility check */

#if defined( __WINDOWS__ ) && !defined( _WIN32_WCE ) 

#pragma comment( lib, "advapi32" )

static int checkFileAccessibleACL( const char *fileName )
	{
	BYTE sidBuffer[ 1024 ];
	SECURITY_DESCRIPTOR *pSID = ( void * ) sidBuffer;
	GENERIC_MAPPING gMapping;
	PRIVILEGE_SET psPrivilege;
	HANDLE hThreadToken;
	DWORD dwPrivilegeLength = sizeof( PRIVILEGE_SET );
	DWORD cbNeeded, dwGrantedAccess;
	BOOL fStatus; 

	if( !GetFileSecurity( fileName, ( OWNER_SECURITY_INFORMATION | \
									   GROUP_SECURITY_INFORMATION | \
									   DACL_SECURITY_INFORMATION), 
						   pSID, 1024, &cbNeeded ) )
		{
		/* We can't access file security information (presumably due to
		   insufficient permissions), there's a problem */
		return( FALSE );
		}
	if( !ImpersonateSelf( SecurityImpersonation ) )
		return( TRUE );
	if( !OpenThreadToken( GetCurrentThread(), TOKEN_ALL_ACCESS, FALSE, &hThreadToken ) )
		{
		RevertToSelf();
		return( TRUE );
		}
	if( !AccessCheck( pSID, hThreadToken, FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, 
					  &gMapping, &psPrivilege, &dwPrivilegeLength,
					  &dwGrantedAccess, &fStatus ) )
		{
		const DWORD dwLastError = GetLastError();

		RevertToSelf();
		CloseHandle( hThreadToken );

		/* If it's FAT32 then there's nothing further to check */
		if( dwLastError == ERROR_NO_SECURITY_ON_OBJECT || \
			dwLastError == ERROR_NOT_SUPPORTED )
			return( TRUE );

		return( FALSE );
		}
	RevertToSelf();
	CloseHandle( hThreadToken );
	if( !fStatus )
		{
		return( FALSE );
		}

	return( TRUE );
	}
#endif /* Windows versions with ACLs */

/* Check that a file is accessible.  This is a generic sanity check to make
   sure that access to keyset files is functioning */

int checkFileAccess( void )
	{
	CRYPT_KEYSET cryptKeyset;
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ];
	int length, failedFileNo = 0, status;

	/* First, check that the file actually exists so that we can return an
	   appropriate error message */
	if( ( filePtr = fopen( convertFileName( CA_PRIVKEY_FILE ),
						   "rb" ) ) == NULL )
		failedFileNo = 1;
	else
		fclose( filePtr );
	if( failedFileNo == 0 && \
		( filePtr = fopen( convertFileName( TEST_PRIVKEY_FILE ), \
						   "rb" ) ) == NULL )
		failedFileNo = 2;
	else
		fclose( filePtr );
	if( failedFileNo > 0 )
		{
		printf( "Couldn't access cryptlib keyset file '%s'.  Please make "
				"sure\nthat all the cryptlib files have been installed "
				"correctly, and the cryptlib\nself-test is being run from "
				"the correct directory.\n", 
				( failedFileNo == 1 ) ? CA_PRIVKEY_FILE : TEST_PRIVKEY_FILE );
		return( FALSE );
		}

	/* Now check for accessibility problems due to filesystem permissions.
	   This can sometimes occur in odd situations for private-key files 
	   (which are set up with fairly restrictive ACLs) when the files have 
	   been copied from one filesystem to another with a different user,
	   so the ACLs grant access to the user on the source filesystem rather
	   than the destination filesystem (this requires a somewhat messed-
	   up copy, since the copier will have had access but the current 
	   requester won't).

	   We check for access to two files, the CA private-key file that ships
	   with cryptlib and the user private-key file that's created when
	   cryptlib is run */
#if defined( __WINDOWS__ ) && !defined( _WIN32_WCE ) 
	if( !checkFileAccessibleACL( CA_PRIVKEY_FILE ) )
		failedFileNo = 1;
	else
		{
		if( !checkFileAccessibleACL( TEST_PRIVKEY_FILE ) )
			failedFileNo = 2;
		}
	if( failedFileNo > 0 )
		{
		printf( "Couldn't access %s cryptlib keyset file '%s'\nfor "
				"read/write/delete.  This is probably due to a filesystem "
				"ACL issue\nin which the current user has insufficient "
				"permissions to perform the\nrequired file access.\n",
				( failedFileNo == 1 ) ? "pre-generated" : "test-run generated",
				( failedFileNo == 1 ) ? CA_PRIVKEY_FILE : TEST_PRIVKEY_FILE );
		return( FALSE );
		}
#endif /* Windows versions with ACLs */

	/* Now read the test files and see if there's any problem due to data 
	   conversion evident */
	filenameFromTemplate( buffer, TESTDATA_FILE_TEMPLATE, 1 );
	if( ( filePtr = fopen( buffer, "rb" ) ) == NULL )
		{
		puts( "Couldn't open binary data test file to check for data "
			  "conversion problems." );
		return( FALSE );
		}
	length = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );
	if( length != 16 || \
		memcmp( buffer, "\x30\x82\x02\x56\x30\x82\x02\x52\r\n\x08\x40\n" "tqz", 16 ) )
		{
		puts( "Binary data is corrupt, probably due to being unzipped or "
			  "copied onto the\nsystem in a mode that tries to translate "
			  "text data during processing/copying." );
		return( FALSE );
		}
#ifdef __UNIX__
	filenameFromTemplate( buffer, TESTDATA_FILE_TEMPLATE, 2 );
	if( ( filePtr = fopen( buffer, "rb" ) ) == NULL )
		{
		puts( "Couldn't open text data test file to check for data "
			  "conversion problems." );
		return( FALSE );
		}
	length = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );
	if( length != 10 || memcmp( buffer, "test\ntest\n" , 10 ) )
		{
		puts( "Text data is still in CRLF-delimited format, probably due "
			  "to being unzipped\nwithout the '-a' option to translate "
			  "text files for Unix systems." );
		return( FALSE );
		}
#endif /* __UNIX__ */

	/* The file exists and is accessible and was copied/installed correctly, 
	   now try and open it using the cryptlib file access functions.  This
	   is a bit of a catch-22 because we're trying to at least open a keyset
	   before the self-test has verified the correct functioning of the
	   keyset-access code, but in almost all cases it's working OK and this
	   provides a useful general sanity-check, since the keyset code would
	   fail in any case when we get to it in the self-test */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  CA_PRIVKEY_FILE, CRYPT_KEYOPT_READONLY );
	if( cryptStatusError( status ) )
		{
		/* If file keyset access isn't available, the inability to access
		   the keyset isn't an error */
		if( status == CRYPT_ERROR_NOTAVAIL )
			return( TRUE );

		printf( "Couldn't access cryptlib keyset file '%s' even though the "
				"file\nexists and is readable.  Please make sure that the "
				"cryptlib self-test is\nbeing run from the correct "
				"directory.\n", CA_PRIVKEY_FILE );
		return( FALSE );
		}
	cryptKeysetClose( cryptKeyset );

	return( TRUE );
	}

/* Check that external network sites are accessible, used to detect 
   potential problems with machines stuck behind firewalls */

int checkNetworkAccess( void )
	{
	CRYPT_KEYSET cryptKeyset;
	int status;

	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_HTTP,
							  TEXT( "www.amazon.com" ), CRYPT_KEYOPT_READONLY );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptKeysetClose( cryptKeyset );

	return( TRUE );
	}

/****************************************************************************
*																			*
*							Import/Export Functions							*
*																			*
****************************************************************************/

/* Import a certificate object */

int importCertFile( CRYPT_CERTIFICATE *cryptCert, const C_STR fileName )
	{
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ];
	int count;

	if( ( filePtr = fopen( convertFileName( fileName ), "rb" ) ) == NULL )
		return( CRYPT_ERROR_OPEN );
	count = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );
    if( count == BUFFER_SIZE )	/* Item too large for buffer */
		return( CRYPT_ERROR_OVERFLOW );

	/* Import the certificate */
	return( cryptImportCert( buffer, count, CRYPT_UNUSED, cryptCert ) );
	}

int importCertFromTemplate( CRYPT_CERTIFICATE *cryptCert,
							const C_STR fileTemplate, const int number )
	{
	BYTE filenameBuffer[ FILENAME_BUFFER_SIZE ];
#ifdef UNICODE_STRINGS
	wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];
#endif /* UNICODE_STRINGS */

	filenameFromTemplate( filenameBuffer, fileTemplate, number );
#ifdef UNICODE_STRINGS
	mbstowcs( wcBuffer, filenameBuffer, strlen( filenameBuffer ) + 1 );
	return( importCertFile( cryptCert, wcBuffer ) );
#else
	return( importCertFile( cryptCert, filenameBuffer ) );
#endif /* UNICODE_STRINGS */
	}

/* Read a key from a key file */

int getPublicKey( CRYPT_CONTEXT *cryptContext, const C_STR keysetName,
				  const C_STR keyName )
	{
	CRYPT_KEYSET cryptKeyset;
	int status;

	/* Read the key from the keyset */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  keysetName, CRYPT_KEYOPT_READONLY );
	if( cryptStatusError( status ) )
		return( status );
	status = cryptGetPublicKey( cryptKeyset, cryptContext, CRYPT_KEYID_NAME,
								keyName );
	if( cryptStatusError( status ) )
		printExtError( cryptKeyset, "cryptGetPublicKey", status, __LINE__ );
	cryptKeysetClose( cryptKeyset );
	return( status );
	}

int getPrivateKey( CRYPT_CONTEXT *cryptContext, const C_STR keysetName,
				   const C_STR keyName, const C_STR password )
	{
	CRYPT_KEYSET cryptKeyset;
	time_t validFrom, validTo;
	int dummy, status;

	/* Read the key from the keyset */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  keysetName, CRYPT_KEYOPT_READONLY );
	if( cryptStatusError( status ) )
		return( status );
	status = cryptGetPrivateKey( cryptKeyset, cryptContext, CRYPT_KEYID_NAME,
								 keyName, password );
	if( cryptStatusError( status ) )
		printExtError( cryptKeyset, "cryptGetPrivateKey", status, __LINE__ );
	cryptKeysetClose( cryptKeyset );
	if( cryptStatusError( status ) )
		return( status );

	/* If the key has a certificate attached, make sure it's still valid 
	   before we hand it back to the self-test functions that will report 
	   the problem as being with the self-test rather than with the 
	   certificate.  We check not just the expiry date but also the expiry 
	   interval to make sure that we don't get false positives on short-
	   validity certificates */
	status = cryptGetAttributeString( *cryptContext,
					CRYPT_CERTINFO_VALIDFROM, &validFrom, &dummy );
	if( cryptStatusError( status ) )
		{
		/* There's no certificate there, this isn't an error */
		return( CRYPT_OK );
		}
	cryptGetAttributeString( *cryptContext,
					CRYPT_CERTINFO_VALIDTO, &validTo, &dummy );
#ifndef _WIN32_WCE
	if( ( validTo - validFrom > ( 86400 * EXPIRY_WARN_DAYS ) ) && \
		validTo - time( NULL ) <= ( 86400 * EXPIRY_WARN_DAYS ) )
		{
		const time_t currentTime = time( NULL );

		puts( "                         ********************" );
		if( validTo <= currentTime )
			{
			puts( "Warning: This key has expired.  Certificate-related "
				  "operations will fail or\n         result in error "
				  "messages from the test code." );
			}
		else
			{
			if( validTo - currentTime <= 86400 )
				{
				puts( "Warning: This key expires today.  Certificate-"
					  "related operations may fail\n         or result in "
					  "error messages from the test code." );
				}
			else
				{
				printf( "Warning: This key will expire in %ld days.  "
						"Certificate-related operations\n         may fail "
						"or result in error messages from the test code.\n",
						( validTo - currentTime ) / 86400 );
				}
			}
		puts( "                         ********************" );
		printf( "Hit a key..." );
		getchar();
		putchar( '\r' );
		}
#endif /* _WIN32_WCE */
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Key File Access Routines						*
*																			*
****************************************************************************/

/* Key file and password-handling access routines */

const C_STR getKeyfileName( const KEYFILE_TYPE type,
							const BOOLEAN isPrivKey )
	{
	switch( type )
		{
		case KEYFILE_X509:
			return( USER_PRIVKEY_FILE );
		case KEYFILE_PGP:
		case KEYFILE_PGP_SPECIAL:
			return( isPrivKey ? PGP_PRIVKEY_FILE : PGP_PUBKEY_FILE );
		case KEYFILE_OPENPGP_HASH:
			return( isPrivKey ? OPENPGP_PRIVKEY_HASH_FILE : OPENPGP_PUBKEY_HASH_FILE );
		case KEYFILE_OPENPGP_AES:
			return( isPrivKey ? OPENPGP_PRIVKEY_AES_FILE : OPENPGP_PUBKEY_AES_FILE );
		case KEYFILE_OPENPGP_CAST:
			return( OPENPGP_PRIVKEY_CAST_FILE );
		case KEYFILE_OPENPGP_RSA:
			return( isPrivKey ? OPENPGP_PRIVKEY_RSA_FILE : OPENPGP_PUBKEY_RSA_FILE );
		case KEYFILE_OPENPGP_MULT:
			return( OPENPGP_PUBKEY_MULT_FILE );
		case KEYFILE_OPENPGP_PARTIAL:
			return( OPENPGP_PRIVKEY_PART_FILE );
		case KEYFILE_NAIPGP:
			return( isPrivKey ? NAIPGP_PRIVKEY_FILE : NAIPGP_PUBKEY_FILE );
		}
	assert( 0 );
	return( TEXT( "notfound" ) );
	}

const C_STR getKeyfilePassword( const KEYFILE_TYPE type )
	{
	switch( type )
		{
		case KEYFILE_X509:
			return( TEST_PRIVKEY_PASSWORD );
		case KEYFILE_PGP:
		case KEYFILE_OPENPGP_HASH:
		case KEYFILE_OPENPGP_RSA:
			return( TEXT( "test1" ) );
		case KEYFILE_NAIPGP:
			return( TEXT( "test10" ) );
		case KEYFILE_OPENPGP_AES:
			return( TEXT( "testkey" ) );
		case KEYFILE_OPENPGP_CAST:
			return( TEXT( "test" ) );
		case KEYFILE_OPENPGP_PARTIAL:
			return( TEXT( "def" ) );
		}
	assert( 0 );
	return( TEXT( "notfound" ) );
	}

const C_STR getKeyfileUserID( const KEYFILE_TYPE type,
							  const BOOLEAN isPrivKey )
	{
	/* If possible we specify user IDs for keys in the middle of the keyring
	   to make sure that we test the ability to correctly handle multiple
	   keys */
	switch( type )
		{
		case KEYFILE_X509:
			return( USER_PRIVKEY_LABEL );
		case KEYFILE_PGP:
			return( TEXT( "test" ) );
		case KEYFILE_PGP_SPECIAL:
			return( TEXT( "suzuki" ) );
		case KEYFILE_NAIPGP:
			return( isPrivKey ? TEXT( "test" ) : TEXT( "test cryptlib" ) );
		case KEYFILE_OPENPGP_HASH:
		case KEYFILE_OPENPGP_RSA:
			return( TEXT( "test1" ) );
		case KEYFILE_OPENPGP_MULT:
			return( TEXT( "NXX2502" ) );
		case KEYFILE_OPENPGP_AES:
			return( TEXT( "Max Mustermann" ) );
		case KEYFILE_OPENPGP_CAST:
			return( TEXT( "Trond" ) );
		}
	assert( 0 );
	return( TEXT( "notfound" ) );
	}

/****************************************************************************
*																			*
*							OS Helper Functions								*
*																			*
****************************************************************************/

#if defined( __BORLANDC__ ) && ( __BORLANDC__ <= 0x310 )

/* BC++ 3.x doesn't have mbstowcs() in the default library, and also defines
   wchar_t as char (!!) so we fake it here */

size_t mbstowcs( char *pwcs, const char *s, size_t n )
	{
	memcpy( pwcs, s, n );
	return( n );
	}
#endif /* BC++ 3.1 or lower */

/* When using multiple threads we need to delay one thread for a small
   amount of time, unfortunately there's no easy way to do this with pthreads
   so we have to provide the following wrapper function that makes an
   (implementation-specific) attempt at it */

#if defined( UNIX_THREADS ) || defined( WINDOWS_THREADS ) || defined( OS2_THREADS )

#if defined( UNIX_THREADS )
  /* This include must be outside the function to avoid weird compiler errors
	 on some systems */
  #include <sys/time.h>
#endif /* UNIX_THREADS */

void delayThread( const int seconds )
	{
#if defined( UNIX_THREADS )
	struct timeval tv = { 0 };

	/* The following should put a thread to sleep for a second on most
	   systems since the select() should be a thread-safe one in the
	   presence of pthreads */
	tv.tv_sec = seconds;
	select( 1, NULL, NULL, NULL, &tv );
#elif defined( WINDOWS_THREADS )
	Sleep( seconds * 1000 );
#endif /* Threading system-specific delay functions */
	}
#endif /* Systems with threading support */

/* Helper functions to make tracking down errors on systems with no console
   a bit less painful.  These just use the debug console as stdout */

#ifdef _WIN32_WCE

void wcPrintf( const char *format, ... )
	{
	wchar_t wcBuffer[ 1024 ];
	char buffer[ 1024 ];
	va_list argPtr;

	va_start( argPtr, format );
	vsprintf( buffer, format, argPtr );
	va_end( argPtr );
	mbstowcs( wcBuffer, buffer, strlen( buffer ) + 1 );
	NKDbgPrintfW( wcBuffer );
	}

void wcPuts( const char *string )
	{
	wcPrintf( "%s\n", string );
	}
#endif /* Console-less environments */

/* Conversion functions used to get Unicode input into generic ASCII
   output */

#ifdef UNICODE_STRINGS

/* Get a filename in an appropriate format for the C runtime library */

const char *convertFileName( const C_STR fileName )
	{
	static char fileNameBuffer[ FILENAME_BUFFER_SIZE ];

	wcstombs( fileNameBuffer, fileName, wcslen( fileName ) + 1 );
	return( fileNameBuffer );
	}

/* Map a filename template to an actual filename, input in Unicode, output in
   ASCII */

void filenameFromTemplate( char *buffer, const wchar_t *fileTemplate,
						   const int count )
	{
	wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];
	int length;

	length = _snwprintf( wcBuffer, FILENAME_BUFFER_SIZE, fileTemplate,
						 count );
	wcstombs( buffer, wcBuffer, length + 1 );
	}

void filenameParamFromTemplate( wchar_t *buffer,
								const wchar_t *fileTemplate,
								const int count )
	{
	_snwprintf( buffer, FILENAME_BUFFER_SIZE, fileTemplate, count );
	}
#endif /* UNICODE_STRINGS */

/****************************************************************************
*																			*
*							Thread Support Functions						*
*																			*
****************************************************************************/

#if defined( WINDOWS_THREADS )

static HANDLE hMutex;

void createMutex( void )
	{
	hMutex = CreateMutex( NULL, FALSE, NULL );
	}
void acquireMutex( void )
	{
	if( WaitForSingleObject( hMutex, 30000 ) == WAIT_TIMEOUT )
		{
		puts( "Warning: Couldn't acquire mutex after 30s wait.  Press a "
			  "key to continue." );
		getchar();
		}
	}
int waitMutex( void )
	{
	if( WaitForSingleObject( hMutex, 30000 ) == WAIT_TIMEOUT )
		return( CRYPT_ERROR_TIMEOUT );
	
	/* Since this is merely a synchronisation operation in which a later 
	   thread waits to catch up to an earlier one, we release the mutex again
	   so other threads can get in */
	releaseMutex();
	return( CRYPT_OK );
	}
void releaseMutex( void )
	{
	if( !ReleaseMutex( hMutex ) )
		{
		puts( "Warning: Couldn't release mutex.  Press a key to continue." );
		getchar();
		}
	}
void destroyMutex( void )
	{
	CloseHandle( hMutex );
	}

void waitForThread( const HANDLE hThread )
	{
	if( WaitForSingleObject( hThread, 15000 ) == WAIT_TIMEOUT )
		{
		puts( "Warning: Server thread is still active due to session "
			  "negotiation failure,\n         this will cause an error "
			  "condition when cryptEnd() is called due\n         to "
			  "resources remaining allocated.  Press a key to continue." );
		getchar();
		}
	CloseHandle( hThread );
	}
#elif defined( UNIX_THREADS )

static pthread_mutex_t mutex;

void createMutex( void )
	{
	pthread_mutex_init( &mutex, NULL );
	}
void acquireMutex( void )
	{
	pthread_mutex_lock( &mutex );
	}
int waitMutex( void )
	{
	pthread_mutex_lock( &mutex );
	
	/* Since this is merely a synchronisation operation in which a later 
	   thread waits to catch up to an earlier one, we release the mutex again
	   so other threads can get in */
	releaseMutex();
	return( CRYPT_OK );
	}
void releaseMutex( void )
	{
	pthread_mutex_unlock( &mutex );
	}
void destroyMutex( void )
	{
	pthread_mutex_destroy( &mutex );
	}

void waitForThread( const pthread_t hThread )
	{
	if( pthread_join( hThread, NULL ) < 0 )
		{
		puts( "Warning: Server thread is still active due to session "
			  "negotiation failure,\n         this will cause an error "
			  "condition when cryptEnd() is called due\n         to "
			  "resources remaining allocated.  Press a key to continue." );
		getchar();
		}
	}

#else

void createMutex( void )
	{
	}
void acquireMutex( void )
	{
	}
void releaseMutex( void )
	{
	}
int waitMutex( void )
	{
	return( CRYPT_OK );
	}
void destroyMutex( void )
	{
	}
#endif /* WINDOWS_THREADS */

#if defined( WINDOWS_THREADS ) || defined( UNIX_THREADS )

/* Dispatch multiple client and server threads and wait for them to exit */

int multiThreadDispatch( THREAD_FUNC clientFunction,
						 THREAD_FUNC serverFunction, const int noThreads )
	{
	THREAD_HANDLE hClientThreads[ MAX_NO_THREADS ];
	THREAD_HANDLE hServerThreads[ MAX_NO_THREADS ];
	int sessionID[ MAX_NO_THREADS ];
	int i;

	assert( noThreads <= MAX_NO_THREADS );

	/* Set up the session ID values */	
	for( i = 0; i < MAX_NO_THREADS; i++ )
		sessionID[ i ] = i;

	/* Start the sessions and wait for them initialise.  We have to wait for
	   some time since the multiple private key reads can take awhile */
	for( i = 0; i < noThreads; i++ )
		{
#ifdef WINDOWS_THREADS
		unsigned int threadID;

		hServerThreads[ i ] = ( HANDLE ) \
						_beginthreadex( NULL, 0, serverFunction,
										&sessionID[ i ], 0, &threadID );
#else
		pthread_t threadHandle;

		hServerThreads[ i ] = 0;
		if( pthread_create( &threadHandle, NULL, serverFunction,
							&sessionID[ i ] ) == 0 )
			hServerThreads[ i ] = threadHandle;
#endif /* Windows vs. pthreads */
		}
	delayThread( 3 );

	/* Connect to the local server */
	for( i = 0; i < noThreads; i++ )
		{
#ifdef WINDOWS_THREADS
		unsigned int threadID;

		hClientThreads[ i ] = ( HANDLE ) \
						_beginthreadex( NULL, 0, clientFunction,
										&sessionID[ i ], 0, &threadID );
#else
		pthread_t threadHandle;

		hServerThreads[ i ] = 0;
		if( pthread_create( &threadHandle, NULL, clientFunction,
							&sessionID[ i ] ) == 0 )
			hClientThreads[ i ] = threadHandle;
#endif /* Windows vs. pthreads */
		}
#ifdef WINDOWS_THREADS
	if( WaitForMultipleObjects( noThreads, hServerThreads, TRUE,
								60000 ) == WAIT_TIMEOUT || \
		WaitForMultipleObjects( noThreads, hClientThreads, TRUE,
								60000 ) == WAIT_TIMEOUT )
#else
	/* Posix doesn't have an ability to wait for multiple threads for mostly
	   religious reasons ("That's not how we do things around here") so we
	   just wait for two token threads */
	pthread_join( hServerThreads[ 0 ], NULL );
	pthread_join( hClientThreads[ 0 ], NULL );
#endif /* Windows vs. pthreads */
		{
		puts( "Warning: Server threads are still active due to session "
			  "negotiation failure,\n         this will cause an error "
			  "condition when cryptEnd() is called due\n         to "
			  "resources remaining allocated.  Press a key to continue." );
		getchar();
		}
#ifdef WINDOWS_THREADS
	for( i = 0; i < noThreads; i++ )
		if( hServerThreads[ i ] != 0 )
			CloseHandle( hServerThreads[ i ] );
	for( i = 0; i < noThreads; i++ )
		if( hClientThreads[ i ] != 0 )
			CloseHandle( hClientThreads[ i ] );
#endif /* Windows vs. pthreads */

	return( TRUE );
	}
#endif /* Windows/Unix threads */

/****************************************************************************
*																			*
*							Timing Support Functions						*
*																			*
****************************************************************************/

/* Get high-resolution timing info */

#ifdef USE_TIMING

#ifdef USE_32BIT_TIME

HIRES_TIME timeDiff( HIRES_TIME startTime )
	{
	HIRES_TIME timeLSB, timeDifference;

#ifdef __WINDOWS__
  #if 1
	LARGE_INTEGER performanceCount;

	/* Sensitive to context switches */
	QueryPerformanceCounter( &performanceCount );
	timeLSB = performanceCount.LowPart;
  #else
	FILETIME dummyTime, kernelTime, userTime;
	int status;

	/* Only accurate to 10ms, returns constant values in VC++ debugger */
	GetThreadTimes( GetCurrentThread(), &dummyTime, &dummyTime,
					&kernelTime, &userTime );
	timeLSB = userTime.dwLowDateTime;
  #endif /* 0 */
#else
	struct timeval tv;

	/* Only accurate to about 1us */
	gettimeofday( &tv, NULL );
	timeLSB = tv.tv_usec;
#endif /* Windows vs.Unix high-res timing */

	/* If we're getting an initial time, return an absolute value */
	if( !startTime )
		return( timeLSB );

	/* We're getting a time difference */
	if( startTime < timeLSB )
		timeDifference = timeLSB - startTime;
	else
		{
#ifdef __WINDOWS__
		/* Windows rolls over at INT_MAX */
		timeDifference = ( 0xFFFFFFFFUL - startTime ) + 1 + timeLSB;
#else
		/* gettimeofday() rolls over at 1M us */
		timeDifference = ( 1000000L - startTime ) + timeLSB;
#endif /* __WINDOWS__ */
		}
	if( timeDifference <= 0 )
		{
		printf( "Error: Time difference = " HIRES_FORMAT_SPECIFIER ", "
				"startTime = " HIRES_FORMAT_SPECIFIER ", "
				"endTime = " HIRES_FORMAT_SPECIFIER ".\n",
				timeDifference, startTime, timeLSB );
		return( 1 );
		}
	return( timeDifference );
	}
#else

HIRES_TIME timeDiff( HIRES_TIME startTime )
	{
	HIRES_TIME timeValue;

#ifdef __WINDOWS__
  #if 1
	LARGE_INTEGER performanceCount;

	/* Sensitive to context switches */
	QueryPerformanceCounter( &performanceCount );
	timeValue = performanceCount.QuadPart;
  #else
	FILETIME dummyTime, kernelTime, userTime;
	int status;

	/* Only accurate to 10ms, returns constant values in VC++ debugger */
	GetThreadTimes( GetCurrentThread(), &dummyTime, &dummyTime,
					&kernelTime, &userTime );
	timeLSB = userTime.dwLowDateTime;
  #endif /* 0 */
#else
	struct timeval tv;

	/* Only accurate to about 1us */
	gettimeofday( &tv, NULL );
	timeValue = ( ( ( HIRES_TIME ) tv.tv_sec ) << 32 ) | tv.tv_usec;
#endif /* Windows vs.Unix high-res timing */

	if( !startTime )
		return( timeValue );
	return( timeValue - startTime );
	}
#endif /* USE_32BIT_TIME */

/* Print timing info.  This gets a bit hairy because we're actually counting 
   low-level timer ticks rather than abstract thread times which means that 
   we'll be affected by things like context switches.  There are two 
   approaches to this:

	1. Take the fastest time, which will be the time least affected by 
	   system overhead.

	2. Apply standard statistical techniques to weed out anomalies.  Since 
	   this is just for testing purposes all we do is discard any results 
	   out by more than 10%, which is crude but reasonably effective.  A 
	   more rigorous approach is to discards results more than n standard 
	   deviations out, but this gets screwed up by the fact that a single 
	   context switch of 20K ticks can throw out results from an execution 
	   time of only 50 ticks.  In any case (modulo context switches) the 
	   fastest, 10%-out, and 2 SD out times are all within about 1% of each 
	   other so all methods are roughly equally accurate */

static void timeDisplay( HIRES_TIME *times, const int noTimes )
	{
	HIRES_TIME timeSum = 0, timeAvg, timeDelta;
	HIRES_TIME timeMin = 1000000L;
	HIRES_TIME timeCorrSum10 = 0;
	HIRES_TIME avgTime;
#ifdef __WINDOWS__
	LARGE_INTEGER performanceCount;
#endif /* __WINDOWS__ */
#ifdef USE_SD
	HIRES_TIME timeCorrSumSD = 0;
	double stdDev;
	int timesCountSD = 0;
#endif /* USE_SD */
	long timeMS, ticksPerSec;
	int i, timesCount10 = 0;

	/* Try and get the clock frequency */
	printf( "Times given in clock ticks of frequency " );
#ifdef __WINDOWS__
	QueryPerformanceFrequency( &performanceCount );
	ticksPerSec = performanceCount.LowPart;
	printf( "%ld", ticksPerSec );
#else
	printf( "~1M" );
	ticksPerSec = 1000000;
#endif /* __WINDOWS__ */
	printf( " ticks per second.\n\n" );

	/* Find the mean execution time */
	for( i = 1; i < noTimes; i++ )
		timeSum += times[ i ];
	timeAvg = timeSum / noTimes;
	timeDelta = timeAvg / 10;	/* 10% variation */

	/* Find the fastest overall time */
	for( i = 1; i < noTimes; i++ )
		{
		if( times[ i ] < timeMin )
			timeMin = times[ i ];
		}

	/* Find the mean time, discarding anomalous results more than 10% out.  
	   We cast the values to longs in order to (portably) print them, if we 
	   want to print the full 64-bit values we have to use nonstandard 
	   extensions like "%I64d" (for Win32) */
	for( i = 1; i < noTimes; i++ )
		{
		if( times[ i ] > timeAvg - timeDelta && \
			times[ i ] < timeAvg + timeDelta )
			{
			timeCorrSum10 += times[ i ];
			timesCount10++;
			}
		}
	if( timesCount10 <= 0 )
		{
		printf( "Error: No times within +/-%ld of %ld.\n",
				( long ) timeDelta, ( long ) timeAvg );
		return;
		}
	avgTime = timeCorrSum10 / timesCount10;
	printf( "Time: min.= %ld, avg.= %ld ", ( long ) timeMin, 
			( long ) avgTime );
	timeMS = ( avgTime * 1000 ) / ticksPerSec;
#if 0	/* Print difference to fastest time, usually only around 1% */
	printf( "(%4d)", ( timeCorrSum10 / timesCount10 ) - timeMin );
#endif /* 0 */

#ifdef USE_SD
	/* Find the standard deviation */
	for( i = 1; i < noTimes; i++ )
		{
		const HIRES_TIME timeDev = times[ i ] - timeAvg;

		timeCorrSumSD += ( timeDev * timeDev );
		}
	stdDev = timeCorrSumSD / NO_TESTS;
	stdDev = sqrt( stdDev );

	/* Find the mean time, discarding anomalous results more than two 
	   standard deviations out */
	timeCorrSumSD = 0;
	timeDelta = ( HIRES_TIME ) stdDev * 2;
	for( i = 1; j < noTimes; i++ )
		{
		if( times[ i ] > timeAvg - timeDelta && \
			times[ i ] < timeAvg + timeDelta )
			{
			timeCorrSumSD += times[ i ];
			timesCountSD++;
			}
		}
	if( timesCountSD == 0 )
		timesCountSD++;	/* Context switch, fudge it */
	printf( "%6ld", ( long ) ( timeCorrSumSD / timesCountSD ) );

#if 1	/* Print difference to fastest and mean times, usually only around
		   1% */
	printf( " (dF = %4d, dM = %4d)\n",
			( timeCorrSumSD / timesCountSD ) - timeMin,
			abs( ( timeCorrSumSD / timesCountSD ) - \
				 ( timeCorrSum10 / timesCount10 ) ) );
#endif /* 0 */
#endif /* USE_SD */

	/* Print the times in ms */
	printf( "\n  Per-op time = " );
	if( timeMS <= 0 )
		printf( "< 1" );
	else
		printf( "%ld", timeMS );
	printf( " ms.\n" );
	}

/* Timing-attack evaluation code */

int testTimingAttack( void )
	{
	CRYPT_CONTEXT cryptContext, decryptContext;
	CRYPT_CONTEXT sessionKeyContext;
	HIRES_TIME times[ 1000 ];
	BYTE encryptedKeyBlob[ 1024 ];
	int length, i, status;

	/* Create the contexts needed for the decryption timing checks */
	status = cryptCreateContext( &sessionKeyContext, CRYPT_UNUSED,
								 CRYPT_ALGO_3DES );
	if( cryptStatusError( status ) )
		return( FALSE );
	status = cryptGenerateKey( sessionKeyContext );
	if( cryptStatusError( status ) )
		return( FALSE );
	status = loadRSAContexts( CRYPT_UNUSED, &cryptContext, &decryptContext );
	if( !status )
		return( FALSE );

	/* Create the encrypted key blob */
	status = cryptExportKey( encryptedKeyBlob, 1024, &length, cryptContext, 
							 sessionKeyContext );
	if( cryptStatusError( status ) )
		{
		printf( "cryptExportKeyEx() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	if( length != 174 )
		{
		printf( "Encrypted key should be %d bytes, was %d, line %d.\n",
				174, length, __LINE__ );
		return( FALSE );
		}
	cryptDestroyContext( sessionKeyContext );

	/* Determine the time for the unmodified decrypt */
#if 0
	for( i = 0; i < 200; i++ )
		{
		HIRES_TIME timeVal;

		cryptCreateContext( &sessionKeyContext, CRYPT_UNUSED, 
							CRYPT_ALGO_3DES );
		timeVal = timeDiff( 0 );
		status = cryptImportKey( encryptedKeyBlob, length, decryptContext, 
								 sessionKeyContext );
		timeVal = timeDiff( timeVal ); 
		cryptDestroyContext( sessionKeyContext );
		if( cryptStatusError( status ) )
			{
			printf( "cryptImportKey() failed with status %s, line %d.\n", 
					status, __LINE__ );
			return( FALSE );
			}
		times[ i ] = timeVal;
		}
#if 0
	printf( "Time for unmodified decrypt:\n" );
	for( i = 0; i < 200; i++ )
		{
		printf( "%5d ", times[ i ] );
		if( ( ( i + 1 ) % 10 ) == 0 )
			putchar( '\n' );
		}
#endif /* 0 */
	timeDisplay( times, 200 );
#endif /* 0 */

	/* Manipulate the encrypted blob and see what timing effect it has */
	for( i = 0; i < 1000; i++ )
		{
		BYTE buffer[ 1024 ], *encryptedKeyPtr;
		HIRES_TIME timeVal;

		/* For the 1024-bit key the encrypted value in the blob ranges from
		   n + 46 to n + 173 (128 bytes, zero-based) */
		encryptedKeyPtr = buffer + 173;
		memcpy( buffer, encryptedKeyBlob, length );
		*encryptedKeyPtr ^= 0x01;
		cryptCreateContext( &sessionKeyContext, CRYPT_UNUSED, 
							CRYPT_ALGO_3DES );
		timeVal = timeDiff( 0 );
		status = cryptImportKey( buffer, length, decryptContext, 
								 sessionKeyContext );
		timeVal = timeDiff( timeVal ); 
		cryptDestroyContext( sessionKeyContext );
		if( !cryptStatusError( status ) )
			{
			printf( "Corrupted import wasn't detected, line %d.\n", 
					__LINE__ );
			return( FALSE );
			}
		times[ i ] = timeVal;
		}
#if 0
	printf( "Time for modified decrypt:\n" );
	for( i = 0; i < 1000; i++ )
		{
		printf( "%5d ", times[ i ] );
		if( ( ( i + 1 ) % 10 ) == 0 )
			putchar( '\n' );
		}
#endif
	timeDisplay( times, 1000 );

	return( TRUE );
	}
#endif /* USE_TIMING */

/****************************************************************************
*																			*
*							Error-handling Functions						*
*																			*
****************************************************************************/

/* Print extended error attribute information */

void printErrorAttributeInfo( const CRYPT_HANDLE cryptHandle )
	{
	int errorType, errorLocus, status;

	status = cryptGetAttribute( cryptHandle, CRYPT_ATTRIBUTE_ERRORTYPE,
								&errorType );
	cryptGetAttribute( cryptHandle, CRYPT_ATTRIBUTE_ERRORLOCUS, &errorLocus );
	if( cryptStatusOK( status ) && errorType != CRYPT_ERRTYPE_NONE )
		{
		printf( "  Error info attributes report locus %d, type %d.\n",
				errorLocus, errorType );
		}
	}

/* Print extended object error information */

void printExtError( const CRYPT_HANDLE cryptHandle,
					const char *functionName, const int functionStatus,
					const int lineNo )
	{
	char errorMessage[ 512 ];
	int errorMessageLength, status;

	printf( "%s failed with error code %d, line %d.\n", functionName,
			functionStatus, lineNo );
	status = cryptGetAttributeString( cryptHandle, CRYPT_ATTRIBUTE_ERRORMESSAGE,
									  errorMessage, &errorMessageLength );
	if( cryptStatusError( status ) )
		{
		puts( "  No extended error information available." );
		printErrorAttributeInfo( cryptHandle );
		return;
		}
	errorMessage[ errorMessageLength ] = '\0';
	printf( "  Error message = %s'%s'.\n",
			( errorMessageLength > ( 80 - 21 ) ) ? "\n  " : "", 
			errorMessage );
	printErrorAttributeInfo( cryptHandle );
	}

/* Exit with an error message.  attrErrorExit() prints the locus and type,
   extErrorExit() prints the extended error code and message */

BOOLEAN attrErrorExit( const CRYPT_HANDLE cryptHandle,
					   const char *functionName, const int errorCode,
					   const int lineNumber )
	{
	printf( "%s failed with error code %d, line %d.\n", functionName,
			errorCode, lineNumber );
	printErrorAttributeInfo( cryptHandle );
	return( FALSE );
	}

BOOLEAN extErrorExit( const CRYPT_HANDLE cryptHandle,
					  const char *functionName, const int errorCode,
					  const int lineNumber )
	{
	printExtError( cryptHandle, functionName, errorCode, lineNumber );
	cryptDestroyObject( cryptHandle );
	return( FALSE );
	}

/****************************************************************************
*																			*
*								Misc. Functions								*
*																			*
****************************************************************************/

/* Some algorithms can be disabled to eliminate patent problems or reduce the
   size of the code.  The following functions are used to select generally
   equivalent alternatives if the required algorithm isn't available.  These
   selections make certain assumptions, namely that at least one of the
   algorithms in the fallback chain is always available (which is guaranteed,
   3DES is used internally), and that they have the same general properties
   as the algorithms they're replacing, which is also usually the case,
   with Blowfish being a first-instance substitute for IDEA, RC2, or RC5, and
   then 3DES as the fallback if Blowfish isn't available */

CRYPT_ALGO_TYPE selectCipher( const CRYPT_ALGO_TYPE algorithm )
	{
	if( cryptStatusOK( cryptQueryCapability( algorithm, NULL ) ) )
		return( algorithm );
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_BLOWFISH, NULL ) ) )
		return( CRYPT_ALGO_BLOWFISH );
	return( CRYPT_ALGO_3DES );
	}

/* Print a hex string */

void printHex( const BYTE *value, const int length )
	{
	int i;

	for( i = 0; i < length; i += 16 )
		{
		const int innerLen = min( length - i, 16 );
		int j;

		for( j = 0; j < innerLen; j++ )
			printf( "%02X ", value[ i + j ] );
		for( ; j < 16; j++ )
			printf( "   " );
		for( j = 0; j < innerLen; j++ )
			{
			const BYTE ch = value[ i + j ];

			printf( "%c", isprint( ch ) ? ch : '.' );
			}
		printf( "\n" );
		}
	}

/* Add a collection of fields to a certificate */

int addCertFields( const CRYPT_CERTIFICATE certificate,
				   const CERT_DATA *certData, const int lineNo )
	{
	int i;

	for( i = 0; certData[ i ].type != CRYPT_ATTRIBUTE_NONE; i++ )
		{
		int status;

		switch( certData[ i ].componentType )
			{
			case IS_NUMERIC:
				status = cryptSetAttribute( certificate,
							certData[ i ].type, certData[ i ].numericValue );
				if( cryptStatusError( status ) )
					{
					printf( "cryptSetAttribute() for entry %d, field ID %d,\n"
							"  value %d, failed with error code %d, line %d.\n",
							i + 1, certData[ i ].type, certData[ i ].numericValue,
							status, lineNo );
					}
				break;

			case IS_STRING:
				status = cryptSetAttributeString( certificate,
							certData[ i ].type, certData[ i ].stringValue,
							certData[ i ].numericValue ? \
								certData[ i ].numericValue : \
								paramStrlen( certData[ i ].stringValue ) );
				if( cryptStatusError( status ) )
					{
#if defined( _MSC_VER ) && ( _MSC_VER == 1200 ) && !defined( NDEBUG )
					if( status == CRYPT_ERROR_INVALID && \
						paramStrlen( certData[ i ].stringValue ) == 2 && \
						!memcmp( certData[ i ].stringValue, "NZ", 2 ) )
						{
						/* Warn about BoundsChecker-induced Heisenbugs */
						puts( "                         ********************" );
						puts( "If you're running this under BoundsChecker "
							  "you need to disable it to complete\nthe test "
							  "since it causes errors in the certificate "
							  "string-checking code.  The\nfollowing error "
							  "is caused by BoundsChecker, not by the "
							  "self-test failing." );
						puts( "                         ********************" );
						}
#endif /* VC++ 6 */
					printf( "cryptSetAttributeString() for entry %d, field ID %d,\n"
							"  value '%s', failed with error code %d, line %d.\n",
							i + 1, certData[ i ].type,
							( char * ) certData[ i ].stringValue, status,
							lineNo );
					}
				break;

#ifdef HAS_WIDECHAR
			case IS_WCSTRING:
				status = cryptSetAttributeString( certificate,
							certData[ i ].type, certData[ i ].stringValue,
							wcslen( certData[ i ].stringValue ) * sizeof( wchar_t ) );
				if( cryptStatusError( status ) )
					{
					printf( "cryptSetAttributeString() for entry %d, field ID %d,\n"
							"  value '%s', failed with error code %d, line %d.\n",
							i + 1, certData[ i ].type,
							( char * ) certData[ i ].stringValue, status,
							lineNo );
					}
				break;
#endif /* HAS_WIDECHAR */

			case IS_TIME:
				status = cryptSetAttributeString( certificate,
							certData[ i ].type, &certData[ i ].timeValue,
							sizeof( time_t ) );
				if( cryptStatusError( status ) )
					printf( "cryptSetAttributeString() for entry %d, field ID %d,\n"
							"  value 0x%lX, failed with error code %d, line %d.\n",
							i + 1, certData[ i ].type, certData[ i ].timeValue,
							status, lineNo );
				break;

			default:
				assert( FALSE );
				return( FALSE );
			}
		if( cryptStatusError( status ) )
			{
			printErrorAttributeInfo( certificate );
			return( FALSE );
			}
		}

	return( TRUE );
	}

/* Compare two blocks and data and check whether they're identical */

int compareData( const void *origData, const int origDataLength,
				 const void *recovData, const int recovDataLength )
	{
	if( origDataLength != recovDataLength )
		{
		printf( "Original length %d doesn't match recovered data "
				"length %d.\n", origDataLength, recovDataLength );

		return( FALSE );
		}
	if( memcmp( origData, recovData, origDataLength ) )
		{
		printf( "Data of length %d doesn't match recovered data:\n", 
				origDataLength );
		printf( "Original data:\n" );
		printHex( origData, min( origDataLength, 64 ) );
		printf( "Recovered data:\n" );
		printHex( recovData, min( origDataLength, 64 ) );

		return( FALSE );
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*								Debug Functions								*
*																			*
****************************************************************************/

/* Write an object to a file for debugging purposes */

#if defined( _MSC_VER ) && \
	!( defined( _WIN32_WCE ) || defined( __PALMSOURCE__ ) )
  #include <direct.h>
  #include <io.h>
#endif /* VC++ Win16/Win32 */

void debugDump( const char *fileName, const void *data, const int dataLength )
	{
	FILE *filePtr;
#ifdef __UNIX__
	const char *tmpPath = getenv( "TMPDIR" );
	char fileNameBuffer[ FILENAME_BUFFER_SIZE ];
	const int tmpPathLen = ( tmpPath != NULL ) ? strlen( tmpPath ) : 0;
#else
	char fileNameBuffer[ 128 ];
#endif /* __UNIX__ */
	const int length = strlen( fileName );
	int count;

	fileNameBuffer[ 0 ] = '\0';
#if defined( _WIN32_WCE )
	/* Under WinCE we don't want to scribble a ton of data into flash every
	   time we're run so we don't try and do anything */
	return;
#elif ( defined( _MSC_VER ) && !defined( __PALMSOURCE__ ) )
	/* If the path isn't absolute, deposit it in a temp directory.  Note
	   that we have to use underscores in front of the Posix functions
	   because these were deprecated starting with VS 2005.  In addition we 
	   have to explicitly exclude oldnames.lib (which usually isn't a 
	   included in the libraries installed with VS) from the link, inclusion 
	   of this is triggered by the compiler seeing the Posix or underscore-
	   Posix functions */
  #if defined( _MSC_VER ) && ( _MSC_VER >= 1400 )
	#pragma comment(linker, "/nodefaultlib:oldnames.lib")
  #endif /* VC++ 2005 and newer misconfiguration */
	if( fileName[ 1 ] != ':' )
		{
		/* It's my code, I can use whatever paths I feel like */
		if( _access( "d:/tmp/", 6 ) == 0 )
			{
			/* There's a data partition available, dump the info there */
			if( _access( "d:/tmp/", 6 ) == -1 && \
				!CreateDirectory( "d:/tmp", NULL ) )
				return;
			strcpy( fileNameBuffer, "d:/tmp/" );
			}
		else
			{
			/* There's no separate data partition, everything's dumped into
			   the same partition */
			if( _access( "c:/tmp/", 6 ) == -1 && \
				!CreateDirectory( "c:/tmp", NULL ) )
				return;
			strcpy( fileNameBuffer, "c:/tmp/" );
			}
		}
#elif defined( __UNIX__ )
	/* If the path isn't absolute, deposit it in a temp directory */
	if( fileName[ 0 ] != '/' )
		{
		if( tmpPathLen > 3 && tmpPathLen < FILENAME_BUFFER_SIZE - 64 )
			{
			strcpy( fileNameBuffer, tmpPath );
			if( fileNameBuffer[ tmpPathLen - 1 ] != '/' )
				strcat( fileNameBuffer + tmpPathLen, "/" );
			}
		else
			strcpy( fileNameBuffer, "/tmp/" );
		}
#else
	fileNameBuffer[ 0 ] = '\0';
#endif /* OS-specific paths */
	strcat( fileNameBuffer, fileName );
	if( length <= 3 || fileName[ length - 4 ] != '.' )
		strcat( fileNameBuffer, ".der" );

#if defined( __VMCMS__ )
	{
	char formatBuffer[ 32 ];

	sprintf( formatBuffer, "wb, recfm=F, lrecl=%d, noseek", dataLength );
	filePtr = fopen( fileNameBuffer, formatBuffer );
	}
	if( filePtr == NULL )
		return;
#else
	if( ( filePtr = fopen( fileNameBuffer, "wb" ) ) == NULL )
		return;
#endif /* __VMCMS__ */
	count = fwrite( data, 1, dataLength, filePtr );
	fclose( filePtr );
	if( count < length )
		{
		printf( "Warning: Couldn't dump '%s' to disk.\n", fileName );
		remove( fileName );
		}
	}

/****************************************************************************
*																			*
*								Session Functions							*
*																			*
****************************************************************************/

/* Print information on the peer that we're talking to */

int printConnectInfo( const CRYPT_SESSION cryptSession )
	{
#ifndef UNICODE_STRINGS
	time_t theTime;
#endif /* UNICODE_STRINGS */
	C_CHR serverName[ 128 ];
	int serverNameLength, serverPort, status;

	status = cryptGetAttributeString( cryptSession, CRYPT_SESSINFO_CLIENT_NAME,
									  serverName, &serverNameLength );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptGetAttribute( cryptSession, CRYPT_SESSINFO_CLIENT_PORT, &serverPort );
#ifdef UNICODE_STRINGS
	serverName[ serverNameLength / sizeof( wchar_t ) ] = TEXT( '\0' );
	printf( "SVR: Connect attempt from %S, port %d", serverName, serverPort );
#else
	serverName[ serverNameLength ] = '\0';
	time( &theTime );
	printf( "SVR: Connect attempt from %s, port %d, on %s.\n", serverName,
			serverPort, getTimeString( theTime, 0 ) );
#endif /* UNICODE_STRINGS */
	fflush( stdout );

	/* Display all the attributes that we've got */
	status = displayAttributes( cryptSession );
	fflush( stdout );
	return( status );
	}

/* Print security info for the session */

int printSecurityInfo( const CRYPT_SESSION cryptSession,
					   const BOOLEAN isServer,
					   const BOOLEAN showFingerprint,
					   const BOOLEAN showServerKeyInfo,
					   const BOOLEAN showClientCertInfo )
	{
	int cryptAlgo, keySize = DUMMY_INIT, version = DUMMY_INIT, status;

	/* Print general security info */
	status = cryptGetAttribute( cryptSession, CRYPT_CTXINFO_ALGO,
								&cryptAlgo );
	if( cryptStatusOK( status ) )
		status = cryptGetAttribute( cryptSession, CRYPT_CTXINFO_KEYSIZE,
									&keySize );
	if( cryptStatusOK( status ) )
		status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_VERSION,
									&version );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't get session security parameters, status %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	printf( "%sSession is protected using algorithm %d with a %d bit key,\n"
			"  protocol version %d.\n", isServer ? "SVR: " : "",
			cryptAlgo, keySize * 8, version );
	if( showServerKeyInfo || showClientCertInfo ) 
		{
		CRYPT_CONTEXT serverKey;

		status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_RESPONSE,
									&serverKey );
		if( cryptStatusOK( status ) )
			{
			status = cryptGetAttribute( serverKey, CRYPT_CTXINFO_ALGO,
										&cryptAlgo );
			if( cryptStatusOK( status ) )
				status = cryptGetAttribute( serverKey, CRYPT_CTXINFO_KEYSIZE,
											&keySize );
			cryptDestroyContext( serverKey );
			}
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't get server security parameters, status %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		printf( "%s key uses algorithm %d, key size %d bits.\n", 
				showClientCertInfo ? "SVR: Client authentication" : "Server", 
				cryptAlgo, keySize * 8 );
		}
	fflush( stdout );
	if( isServer || !showFingerprint )
		return( TRUE );

	status = printFingerprint( cryptSession, FALSE );
	fflush( stdout );
	return( status );
	}

int printFingerprint( const CRYPT_SESSION cryptSession,
					  const BOOLEAN isServer )
	{
	BYTE fingerPrint[ CRYPT_MAX_HASHSIZE ];
	int i, length, status;

	/* Print the server key fingerprint */
	status = cryptGetAttributeString( cryptSession,
									  CRYPT_SESSINFO_SERVER_FINGERPRINT,
									  fingerPrint, &length );
	if( cryptStatusError( status ) )
		{
		printf( "cryptGetAttributeString() failed with error code "
				"%d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	printf( "%sServer key fingerprint =\n  ", isServer ? "SVR: " : "" );
	for( i = 0; i < length; i++ )
		{
		if( i > 0 )
			putchar( ' ' );
		printf( "%02X", fingerPrint[ i ] );
		}
	puts( "." );
	fflush( stdout );

	return( TRUE );
	}

/* Set up a client/server to connect locally.  For the client this simply 
   tells it where to connect, for the server this binds it to the local 
   (loopback) address so we don't inadvertently open up outside ports 
   (admittedly they can't do much except run the hardcoded self-test, but 
   it's better not to do this at all).

   In order to allow testing against non-cryptlib-loopback and/or outside 
   clients we optionally allow it to be set to an explicit address.  This
   can be either an explicit IPv4 localhost (since Windows 7 will bind to
   both IPv4 and IPv6 INADDR_ANY addresses when given "localhost" as an
   argument and refuses connections on the IPv4 interface, only connecting
   on the IPv6 one), or an explicit address for external clients */

#ifdef UNICODE_STRINGS
  #define LOCAL_HOST_NAME			"localhost"
  #define NATIVE_LOCAL_HOST_NAME	L"localhost"
#else
	#define LOCAL_HOST_NAME			"localhost"
	#define NATIVE_LOCAL_HOST_NAME	LOCAL_HOST_NAME
#endif /* UNICODE_STRINGS */
#if 0
  #undef LOCAL_HOST_NAME
  #define LOCAL_HOST_NAME		"127.0.0.1"
#endif /* 0 */
#if 0
  #undef LOCAL_HOST_NAME
  #define LOCAL_HOST_NAME		"192.168.1.45"
#endif /* 0 */

BOOLEAN setLocalConnect( const CRYPT_SESSION cryptSession, const int port )
	{
	int status;

	if( LOCAL_HOST_NAME[ 0 ] != 'l' )
		{
		puts( "Warning: Enabling server on non-local interface '" 
			  LOCAL_HOST_NAME "'." );
		}

	status = cryptSetAttributeString( cryptSession,
									  CRYPT_SESSINFO_SERVER_NAME,
									  NATIVE_LOCAL_HOST_NAME,
									  paramStrlen( NATIVE_LOCAL_HOST_NAME ) );
#ifdef __UNIX__
	/* If we're running under Unix, set the port to a nonprivileged one so
	   that we don't have to run as root.  For anything other than very low-
	   numbered ports (e.g. SSH), the way we determine the port is to repeat
	   the first digit, so e.g. TSA on 318 becomes 3318, this seems to be
	   the method most commonly used */
	if( cryptStatusOK( status ) && port < 1024 )
		{
		if( port < 100 )
			status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_SERVER_PORT,
										port + 4000 );
		else
			status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_SERVER_PORT,
										( ( port / 100 ) * 1000 ) + port );
		}
#endif /* __UNIX__ */
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute/AttributeString() failed with error code "
				"%d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

/* Check whether a remote server might be down, which is treated as a soft 
   fail rather than a hard-fail error condition */

BOOLEAN isServerDown( const CRYPT_SESSION cryptSession,
					  const int errorStatus )
	{
	/* If we get a straight connect error theb we don't treat it as a 
	   serious failure */
	if( errorStatus == CRYPT_ERROR_OPEN || errorStatus == CRYPT_ERROR_NOTFOUND )
		return( TRUE );

	/* Under Unix a connection-refused will be reported as a 
	   CRYPT_ERROR_PERMISSION (under Winsock it's just a generic open 
	   error), so we check for this as an alternative to an open error */
#ifdef __UNIX__
	if( errorStatus == CRYPT_ERROR_PERMISSION )
		{
		char errorMessage[ 512 ];
		int errorMessageLength, status;

		status = cryptGetAttributeString( cryptSession, 
										  CRYPT_ATTRIBUTE_ERRORMESSAGE,
										  errorMessage, &errorMessageLength );
		if( cryptStatusOK( status ) )
			{
			errorMessage[ errorMessageLength ] = '\0';
			if( strstr( errorMessage, "ECONNREFUSED" ) != NULL || \
				strstr( errorMessage, "ETIMEDOUT" ) != NULL )
				return( TRUE );
			}
		}
#endif /* __UNX__ */

	return( FALSE );
	}

/* Run a persistent server session, recycling the connection if the client
   kept the link open */

static void printOperationType( const CRYPT_SESSION cryptSession )
	{
	struct {
		const int operation; 
		const char *name;
		} operationTypeTbl[] = {
		{ CRYPT_REQUESTTYPE_NONE, "(None)" },
		{ CRYPT_REQUESTTYPE_INITIALISATION,	"ir" },
		{ CRYPT_REQUESTTYPE_CERTIFICATE, "cr" },
		{ CRYPT_REQUESTTYPE_KEYUPDATE, "kur" },
		{ CRYPT_REQUESTTYPE_REVOCATION,	"rr" },
		{ CRYPT_REQUESTTYPE_PKIBOOT, "pkiBoot" },
		{ -1, "(Unknown)" }
		};
	char userID[ CRYPT_MAX_TEXTSIZE ];
	int userIDsize = DUMMY_INIT, requestType, i, status;

	status = cryptGetAttribute( cryptSession,
								CRYPT_SESSINFO_CMP_REQUESTTYPE,
								&requestType );
	if( cryptStatusOK( status ) )
		status = cryptGetAttributeString( cryptSession,
									CRYPT_SESSINFO_USERNAME,
									userID, &userIDsize );
	if( cryptStatusError( status ) )
		{
		printf( "cryptGetAttribute/AttributeString() failed with error "
				"code %d, line %d.\n", status, __LINE__ );
		return;
		}
	userID[ userIDsize ] = '\0';
	for( i = 0; operationTypeTbl[ i ].operation != requestType && \
				operationTypeTbl[ i ].operation != -1; i++ );
	printf( "SVR: Operation type was %d = %s, user '%s'.\n",
			requestType, operationTypeTbl[ i ].name, userID );
	fflush( stdout );
	}

int activatePersistentServerSession( const CRYPT_SESSION cryptSession,
									 const BOOLEAN showOperationType )
	{
	BOOLEAN connectionActive = FALSE;
	int status;

	do
		{
		/* Activate the connection */
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE,
									TRUE );
		if( status == CRYPT_ERROR_READ && connectionActive )
			{
			/* The other side closed the connection after a previous
			   successful transaction, this isn't an error */
			return( CRYPT_OK );
			}

		/* Print connection info and check whether the connection is still
		   active.  If it is, we recycle the session so that we can process
		   another request */
		printConnectInfo( cryptSession );
		if( cryptStatusOK( status ) && showOperationType )
			printOperationType( cryptSession );
		cryptGetAttribute( cryptSession, CRYPT_SESSINFO_CONNECTIONACTIVE,
						   &connectionActive );
		}
	while( cryptStatusOK( status ) && connectionActive );

	return( status );
	}

/****************************************************************************
*																			*
*							Attribute Dump Routines							*
*																			*
****************************************************************************/

/* Print a list of all attributes present in an object */

int displayAttributes( const CRYPT_HANDLE cryptHandle )
	{
	int status;

	if( cryptStatusError( \
			cryptSetAttribute( cryptHandle, CRYPT_ATTRIBUTE_CURRENT_GROUP,
							   CRYPT_CURSOR_FIRST ) ) )
		return( TRUE );

	puts( "Attributes present (by cryptlib ID) are:" );
	do
		{
		BOOLEAN firstAttr = TRUE;
		int value;

		status = cryptGetAttribute( cryptHandle,
									CRYPT_ATTRIBUTE_CURRENT_GROUP, &value );
		if( cryptStatusError( status ) )
			{
			printf( "\nCurrent attribute group value read failed with "
					"error code %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		printf( "  Attribute group %d, values =", value );
		do
			{
			status = cryptGetAttribute( cryptHandle, CRYPT_ATTRIBUTE_CURRENT,
										&value );
			if( cryptStatusError( status ) )
				{
				printf( "\nCurrent attribute value read failed with error "
						"code %d, line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			if( !firstAttr )
				putchar( ',' );
			printf( " %d", value );
			firstAttr = FALSE;
			}
		while( cryptSetAttribute( cryptHandle, CRYPT_ATTRIBUTE_CURRENT,
								  CRYPT_CURSOR_NEXT ) == CRYPT_OK );
		puts( "." );
		}
	while( cryptSetAttribute( cryptHandle, CRYPT_ATTRIBUTE_CURRENT_GROUP,
							  CRYPT_CURSOR_NEXT ) == CRYPT_OK );

	/* Reset the cursor to the first attribute.  This is useful for things
	   like envelopes and sessions where the cursor points at the first
	   attribute that needs to be handled */
	cryptSetAttribute( cryptHandle, CRYPT_ATTRIBUTE_CURRENT_GROUP,
					   CRYPT_CURSOR_FIRST );
	return( TRUE );
	}

/****************************************************************************
*																			*
*							Certificate Dump Routines						*
*																			*
****************************************************************************/

/* Check whether a string may be a Unicode string */

static BOOLEAN isUnicode( const BYTE *value, const int length )
	{
	wchar_t wcValue[ 8 + 4 ];

	/* If it's an odd length or too short to reliably guess, report it as 
	   non-Unicode */
	if( ( length % sizeof( wchar_t ) ) || length <= sizeof( wchar_t ) * 2 )
		return( FALSE );

	/* If the first four characters are ASCII then it's unlikely that it'll
	   be Unicode */
	if( isprint( value[ 0 ] ) && isprint( value[ 1 ] ) && \
		isprint( value[ 2 ] ) && isprint( value[ 3 ] ) )
		return( FALSE );

	/* Copy the byte-aligned value into a local wchar_t-aligned buffer for
	   analysis */
	memcpy( wcValue, value, min( 8, length ) );

	/* Check whether the first 3 widechars have identical high bytes.  This
	   isn't totally reliable (e.g. "tanaka" will give a false positive, 
	   { 0x0160, 0x0069, 0x006B } will give a false negative) but it's close
	   enough */
	if( ( wcValue[ 0 ] & 0xFF00 ) == ( wcValue[ 1 ] & 0xFF00 ) && \
		( wcValue[ 0 ] & 0xFF00 ) == ( wcValue[ 2 ] & 0xFF00 ) )
		return( TRUE );

	return( FALSE );
	}

/* The following function performs many attribute accesses, rather than using
   huge numbers of status checks we use the following macro to check each
   attribute access */

#define CHK( function ) \
		status = function; \
		if( cryptStatusError( status ) ) \
			return( certInfoErrorExit( #function, status, __LINE__ ) )

static int certInfoErrorExit( const char *functionCall, const int status,
							  const int line )
	{
	printf( "\n%s failed with status %d, line %d.\n", functionCall,
			status, line );
	return( FALSE );
	}

/* Print a DN or altName */

static int printComponent( const CRYPT_CERTIFICATE certificate,
						   const CRYPT_ATTRIBUTE_TYPE component,
						   const char *prefixString )
	{
	char buffer[ 1024 + 1 ];
	int length, status;

	status = cryptGetAttributeString( certificate, component, NULL, 
									  &length );
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ERROR_NOTAVAIL && \
			component == CRYPT_CERTINFO_DN )
			{
			/* Report this special-case condition explicitly */
			puts( "  (Name contains characters that prevent it from being "
				  "represented as a\n   text string)." ); 
			}
		return( FALSE );
		}
	if( length > 1024 )
		{
		/* This should never happen since the longest permitted component 
		   string has 128 characters, but we check for it just in case */
		puts( "  (Name is too long to display, > 1K characters)." ); 
		return( FALSE );
		}
	status = cryptGetAttributeString( certificate, component, buffer, 
									  &length );
	if( cryptStatusError( status ) )
		return( FALSE );
	if( isUnicode( buffer, length ) )
		{
		wchar_t wcBuffer[ 1024 + 1 ];

		/* Copy the byte-aligned value into a local wchar_t-aligned buffer 
		   for display */
		memcpy( wcBuffer, buffer, length );
		wcBuffer[ length / sizeof( wchar_t ) ] = TEXT( '\0' );
		printf( "  %s = %S.\n", prefixString, wcBuffer ); 
		return( TRUE );
		}
	buffer[ length ] = '\0'; 
	printf( "  %s = %s.\n", prefixString, buffer ); 
	
	return( TRUE );
	}

static int printComponents( const CRYPT_CERTIFICATE certificate,
							const CRYPT_ATTRIBUTE_TYPE component,
							const char *prefixString )
	{
	int status;

	/* Try and print the component if it's present */
	if( !printComponent( certificate, component, prefixString ) )
		return( FALSE );

	/* If it's not a DN or altName component, we're done */
	if( !( component >= CRYPT_CERTINFO_COUNTRYNAME && \
		   component <= CRYPT_CERTINFO_COMMONNAME ) && \
		!( component >= CRYPT_CERTINFO_OTHERNAME_TYPEID && \
		   component > CRYPT_CERTINFO_REGISTEREDID ) )
		return( TRUE );

	/* Check for further components, for multivalued components in altNames */
	CHK( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT_INSTANCE, 
							component ) );
	while( cryptSetAttribute( certificate,
							  CRYPT_ATTRIBUTE_CURRENT_INSTANCE,
							  CRYPT_CURSOR_NEXT ) == CRYPT_OK )
		{
		char buffer[ 64 ];

		sprintf( buffer, "  + %s", prefixString );
		if( !printComponent( certificate, component, buffer ) )
			return( FALSE );
		}
	return( TRUE );
	}

static void printDN( const CRYPT_CERTIFICATE certificate )
	{
	printComponents( certificate, CRYPT_CERTINFO_DN, "DN string" );
	printComponents( certificate, CRYPT_CERTINFO_COUNTRYNAME, "C" );
	printComponents( certificate, CRYPT_CERTINFO_STATEORPROVINCENAME, "S" );
	printComponents( certificate, CRYPT_CERTINFO_LOCALITYNAME, "L" );
	printComponents( certificate, CRYPT_CERTINFO_ORGANIZATIONNAME, "O" );
	printComponents( certificate, CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, "OU" );
	printComponents( certificate, CRYPT_CERTINFO_COMMONNAME, "CN" );
	}

static void printAltName( const CRYPT_CERTIFICATE certificate )
	{
	int status;

	printComponents( certificate, CRYPT_CERTINFO_RFC822NAME, "Email" );
	printComponents( certificate, CRYPT_CERTINFO_DNSNAME, "DNSName" );
	printComponents( certificate, CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER, "EDI Nameassigner" );
	printComponents( certificate, CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME, "EDI Partyname" );
	printComponents( certificate, CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, "URL" );
	printComponents( certificate, CRYPT_CERTINFO_IPADDRESS, "IP" );
	printComponents( certificate, CRYPT_CERTINFO_REGISTEREDID, "Registered ID" );
	status = cryptSetAttribute( certificate, CRYPT_CERTINFO_DIRECTORYNAME,
								CRYPT_UNUSED );
	if( cryptStatusOK( status ) )
		{
		printf( "  altName DN is:\n" );
		printDN( certificate );
		}
	}

/* Print information on a certificate */

int printCertInfo( const CRYPT_CERTIFICATE certificate )
	{
	CRYPT_CERTTYPE_TYPE certType;
	char buffer[ 1024 ];
	int length, value, status;

	CHK( cryptGetAttribute( certificate, CRYPT_CERTINFO_CERTTYPE, &value ) );
	certType = value;

	/* Display the issuer and subject DN */
	if( certType != CRYPT_CERTTYPE_CERTREQUEST && \
		certType != CRYPT_CERTTYPE_REQUEST_CERT && \
		certType != CRYPT_CERTTYPE_REQUEST_REVOCATION && \
		certType != CRYPT_CERTTYPE_RTCS_REQUEST && \
		certType != CRYPT_CERTTYPE_RTCS_RESPONSE && \
		certType != CRYPT_CERTTYPE_OCSP_REQUEST && \
		certType != CRYPT_CERTTYPE_CMS_ATTRIBUTES && \
		certType != CRYPT_CERTTYPE_PKIUSER )
		{
		puts( "Certificate object issuer name is:" );
		CHK( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT,
								CRYPT_CERTINFO_ISSUERNAME ) );
		printDN( certificate );
		if( cryptStatusOK( \
				cryptGetAttribute( certificate,
								   CRYPT_CERTINFO_ISSUERALTNAME, &value ) ) )
			{
			CHK( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT,
									CRYPT_CERTINFO_ISSUERALTNAME ) );
			printAltName( certificate );
			}
		}
	if( certType != CRYPT_CERTTYPE_CRL && \
		certType != CRYPT_CERTTYPE_REQUEST_REVOCATION && \
		certType != CRYPT_CERTTYPE_CMS_ATTRIBUTES && \
		certType != CRYPT_CERTTYPE_RTCS_REQUEST && \
		certType != CRYPT_CERTTYPE_RTCS_RESPONSE && \
		certType != CRYPT_CERTTYPE_OCSP_REQUEST && \
		certType != CRYPT_CERTTYPE_OCSP_RESPONSE )
		{
		puts( "Certificate object subject name is:" );
		CHK( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT,
								CRYPT_CERTINFO_SUBJECTNAME ) );
		printDN( certificate );
		if( cryptStatusOK( \
				cryptGetAttribute( certificate,
								   CRYPT_CERTINFO_SUBJECTALTNAME, &value ) ) )
			{
			CHK( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT,
									CRYPT_CERTINFO_SUBJECTALTNAME ) );
			printAltName( certificate );
			}
		}

	/* Display the validity information */
#ifndef _WIN32_WCE
	if( certType == CRYPT_CERTTYPE_CERTCHAIN ||
		certType == CRYPT_CERTTYPE_CERTIFICATE || \
		certType == CRYPT_CERTTYPE_ATTRIBUTE_CERT )
		{
		time_t validFrom, validTo;

		CHK( cryptGetAttributeString( certificate, CRYPT_CERTINFO_VALIDFROM,
									  &validFrom, &length ) );
		CHK( cryptGetAttributeString( certificate, CRYPT_CERTINFO_VALIDTO,
									  &validTo, &length ) );
		printf( "Certificate is valid from %s to %s.\n", 
				getTimeString( validFrom, 0 ),
				getTimeString( validTo, 1 ) );
		}
	if( certType == CRYPT_CERTTYPE_OCSP_RESPONSE )
		{
		time_t thisUpdate, nextUpdate;

		status = cryptGetAttributeString( certificate, CRYPT_CERTINFO_THISUPDATE,
										  &thisUpdate, &length );
		if( cryptStatusOK( status ) )
			{
			/* RTCS basic responses only return a minimal valid/not valid
			   status so failing to find a time isn't an error */
			status = cryptGetAttributeString( certificate,
											  CRYPT_CERTINFO_NEXTUPDATE,
											  &nextUpdate, &length );
			if( cryptStatusOK( status ) )
				{
				printf( "OCSP source CRL time %s,\n  next update %s.\n", 
						getTimeString( thisUpdate, 0 ), 
						getTimeString( nextUpdate, 1 ) );
				}
			else
				{
				printf( "OCSP source CRL time %s.\n", 
						getTimeString( thisUpdate, 0 ) );
				}
			}
		}
	if( certType == CRYPT_CERTTYPE_CRL )
		{
		time_t thisUpdate, nextUpdate;

		CHK( cryptGetAttributeString( certificate, CRYPT_CERTINFO_THISUPDATE,
									  &thisUpdate, &length ) );
		status = cryptGetAttributeString( certificate, CRYPT_CERTINFO_NEXTUPDATE,
										  &nextUpdate, &length );
		if( cryptStatusOK( status ) )
			{
			printf( "CRL time %s,\n  next update %s.\n", 
					getTimeString( thisUpdate, 0 ), 
					getTimeString( nextUpdate, 1 ) );
			}
		else
			printf( "CRL time %s.\n", getTimeString( thisUpdate, 0 ) );
		}
#endif /* _WIN32_WCE */
	if( certType == CRYPT_CERTTYPE_CRL || \
		certType == CRYPT_CERTTYPE_RTCS_RESPONSE || \
		certType == CRYPT_CERTTYPE_OCSP_RESPONSE )
		{
		int noEntries = 0;

		/* Count and display the entries */
		if( cryptSetAttribute( certificate, CRYPT_CERTINFO_CURRENT_CERTIFICATE,
							   CRYPT_CURSOR_FIRST ) == CRYPT_OK )
			{
			puts( "Revocation/validity list information: " );
			do
				{
#ifndef _WIN32_WCE
				time_t timeStamp = 0;
#endif /* _WIN32_WCE */
				int revStatus, certStatus;

				noEntries++;

				/* Extract response-specific status information */
				if( certType == CRYPT_CERTTYPE_RTCS_RESPONSE )
					{
					CHK( cryptGetAttribute( certificate,
								CRYPT_CERTINFO_CERTSTATUS, &certStatus ) );
					}
				if( certType == CRYPT_CERTTYPE_OCSP_RESPONSE )
					{
					CHK( cryptGetAttribute( certificate,
								CRYPT_CERTINFO_REVOCATIONSTATUS, &revStatus ) );
					}
#ifndef _WIN32_WCE
				if( certType == CRYPT_CERTTYPE_CRL || \
					( certType == CRYPT_CERTTYPE_OCSP_RESPONSE && \
					  revStatus == CRYPT_OCSPSTATUS_REVOKED ) || \
					( certType == CRYPT_CERTTYPE_RTCS_RESPONSE && \
					  certStatus == CRYPT_CERTSTATUS_NOTVALID ) )
					{
					CHK( cryptGetAttributeString( certificate,
								CRYPT_CERTINFO_REVOCATIONDATE, &timeStamp,
								&length ) );
					}
#endif /* _WIN32_WCE */

				/* Make sure we don't print excessive amounts of
				   information */
				if( noEntries >= 20 )
					{
					if( noEntries == 20 )
						puts( "  (Further entries exist, but won't be printed)." );
					continue;
					}

				/* Print details status info */
				switch( certType )
					{
					case CRYPT_CERTTYPE_RTCS_RESPONSE:
						printf( "  Certificate status = %d (%s).\n",
								certStatus,
								( certStatus == CRYPT_CERTSTATUS_VALID ) ? \
									"valid" : \
								( certStatus == CRYPT_CERTSTATUS_NOTVALID ) ? \
									"not valid" : \
								( certStatus == CRYPT_CERTSTATUS_NONAUTHORITATIVE ) ? \
									"only non-authoritative response available" : \
									"unknown" );
						break;

					case CRYPT_CERTTYPE_OCSP_RESPONSE:
						printf( "  Entry %d, rev.status = %d (%s), rev.time "
								"%s.\n", noEntries, revStatus,
								( revStatus == CRYPT_OCSPSTATUS_NOTREVOKED ) ? \
									"not revoked" : \
								( revStatus == CRYPT_OCSPSTATUS_REVOKED ) ? \
									"revoked" : "unknown",
								getTimeString( timeStamp, 0 ) );
						break;

					case CRYPT_CERTTYPE_CRL:
						printf( "  Entry %d, revocation time %s.\n", noEntries,
								getTimeString( timeStamp, 0 ) );
						break;

					default:
						assert( 0 );
					}
				}
			while( cryptSetAttribute( certificate,
									  CRYPT_CERTINFO_CURRENT_CERTIFICATE,
									  CRYPT_CURSOR_NEXT ) == CRYPT_OK );
			}
		printf( "Revocation/validity list has %d entr%s.\n", noEntries,
				( noEntries == 1 ) ? "y" : "ies" );
		}

	/* Display the self-signed status and fingerprint */
	if( cryptStatusOK( cryptGetAttribute( certificate,
									CRYPT_CERTINFO_SELFSIGNED, &value ) ) )
		printf( "Certificate object is %sself-signed.\n",
				value ? "" : "not " );
	if( certType == CRYPT_CERTTYPE_CERTIFICATE || \
		certType == CRYPT_CERTTYPE_CERTCHAIN )
		{
		CHK( cryptGetAttributeString( certificate, CRYPT_CERTINFO_FINGERPRINT,
									  buffer, &length ) );
		printf( "Certificate fingerprint = " );
		printHex( buffer, length );
		}

	/* List the attribute types */
	if( !displayAttributes( certificate ) )
		return( FALSE );

	/* Display common attributes */
	if( cryptStatusError( \
			cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT_GROUP,
							   CRYPT_CURSOR_FIRST ) ) )
		{
		puts( "  (No extensions/attributes)." );
		return( TRUE );
		}
	puts( "Some of the common extensions/attributes are:" );
	if( certType == CRYPT_CERTTYPE_CRL )
		{
		time_t theTime;

		CHK( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT_GROUP,
								CRYPT_CURSOR_FIRST ) );
		status = cryptGetAttribute( certificate, CRYPT_CERTINFO_CRLNUMBER,
									&value );
		if( cryptStatusOK( status ) && value )
			printf( "  crlNumber = %d.\n", value );
		status = cryptGetAttribute( certificate, CRYPT_CERTINFO_DELTACRLINDICATOR,
									&value );
		if( cryptStatusOK( status ) && value )
			printf( "  deltaCRLIndicator = %d.\n", value );
		status = cryptGetAttribute( certificate, CRYPT_CERTINFO_CRLREASON,
									&value );
		if( cryptStatusOK( status ) && value )
			printf( "  crlReason = %d.\n", value );
		status = cryptGetAttributeString( certificate,
								CRYPT_CERTINFO_INVALIDITYDATE, &theTime, &length );
#ifndef _WIN32_WCE
		if( cryptStatusOK( status ) )
			printf( "  invalidityDate = %s.\n", 
					getTimeString( theTime, 0 ) );
#endif /* _WIN32_WCE */
		if( cryptStatusOK( \
				cryptGetAttribute( certificate,
								   CRYPT_CERTINFO_ISSUINGDIST_FULLNAME, &value ) ) )
			{
			CHK( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT,
									CRYPT_CERTINFO_ISSUINGDIST_FULLNAME ) );
			puts( "  issuingDistributionPoint is:" );
			printDN( certificate );
			printAltName( certificate );
			}
		return( TRUE );
		}
#ifndef _WIN32_WCE
	if( certType == CRYPT_CERTTYPE_CMS_ATTRIBUTES )
		{
		time_t signingTime;

		status = cryptGetAttributeString( certificate,
										  CRYPT_CERTINFO_CMS_SIGNINGTIME,
										  &signingTime, &length );
		if( cryptStatusOK( status ) )
			printf( "Signing time %s.\n", getTimeString( signingTime, 0 ) );
		return( TRUE );
		}
#endif /* _WIN32_WCE */
	if( certType == CRYPT_CERTTYPE_PKIUSER )
		{
		CHK( cryptGetAttributeString( certificate, CRYPT_CERTINFO_PKIUSER_ID,
									  buffer, &length ) );
		buffer[ length ] ='\0';
		printf( "  PKI user ID = %s.\n", buffer );
		CHK( cryptGetAttributeString( certificate,
									  CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD,
									  buffer, &length ) );
		buffer[ length ] ='\0';
		printf( "  PKI user issue password = %s.\n", buffer );
		CHK( cryptGetAttributeString( certificate,
									  CRYPT_CERTINFO_PKIUSER_REVPASSWORD,
									  buffer, &length ) );
		buffer[ length ] ='\0';
		printf( "  PKI user revocation password = %s.\n", buffer );
		return( TRUE );
		}
	status = cryptGetAttribute( certificate,
								CRYPT_CERTINFO_KEYUSAGE, &value );
	if( cryptStatusOK( status ) && value )
		{
		static const struct { int flag; const char *name; } usageNames[] = {
			{ CRYPT_KEYUSAGE_DIGITALSIGNATURE, "digSig" },
			{ CRYPT_KEYUSAGE_NONREPUDIATION, "nonRep" },
			{ CRYPT_KEYUSAGE_KEYENCIPHERMENT, "keyEnc" },
			{ CRYPT_KEYUSAGE_DATAENCIPHERMENT, "dataEnc" },
			{ CRYPT_KEYUSAGE_KEYAGREEMENT, "keyAgree" },
			{ CRYPT_KEYUSAGE_KEYCERTSIGN, "certSign" },
			{ CRYPT_KEYUSAGE_CRLSIGN, "crlSign" },
			{ CRYPT_KEYUSAGE_ENCIPHERONLY, "encOnly" },
			{ CRYPT_KEYUSAGE_DECIPHERONLY, "decOnly" },
			{ CRYPT_KEYUSAGE_NONE, NULL }
			};
		BOOLEAN printedUsage = FALSE;
		int i;

		printf( "  keyUsage = %02X (", value );
		for( i = 0; usageNames[ i ].flag != CRYPT_KEYUSAGE_NONE; i++ )
			{
			if( usageNames[ i ].flag & value )
				{
				if( printedUsage )
					printf( ", " );
				printf( "%s", usageNames[ i ].name );
				printedUsage = TRUE;
				}
			}
		puts( ")." );
		}
	status = cryptGetAttribute( certificate,
								CRYPT_CERTINFO_EXTKEYUSAGE, &value );
	if( cryptStatusOK( status ) && value )
		{
		BOOLEAN firstTime = TRUE;

		printf( "  extKeyUsage types = " );
		CHK( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT_GROUP,
								CRYPT_CERTINFO_EXTKEYUSAGE ) );
		do
			{
			CHK( cryptGetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT,
									&value ) );
			printf( "%s%d", firstTime ? "" : ", ", value );
			firstTime = FALSE;
			}
		while( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT,
								  CRYPT_CURSOR_NEXT ) == CRYPT_OK );
		printf( ".\n" );
		}
	status = cryptGetAttribute( certificate, CRYPT_CERTINFO_CA, &value );
	if( cryptStatusOK( status ) && value )
		printf( "  basicConstraints.cA = %s.\n", value ? "True" : "False" );
	status = cryptGetAttribute( certificate, CRYPT_CERTINFO_PATHLENCONSTRAINT,
								&value );
	if( cryptStatusOK( status ) && value )
		printf( "  basicConstraints.pathLenConstraint = %d.\n", value );
	status = cryptGetAttributeString( certificate,
							CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER, buffer, &length );
	if( cryptStatusOK( status ) )
		{
		printf( "  subjectKeyIdentifier = " );
		printHex( buffer, length );
		}
	status = cryptGetAttributeString( certificate,
							CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER, buffer, &length );
	if( cryptStatusOK( status ) )
		{
		printf( "  authorityKeyIdentifier = " );
		printHex( buffer, length );
		}
	status = cryptGetAttributeString( certificate,
							CRYPT_CERTINFO_CERTPOLICYID, buffer, &length );
	if( cryptStatusOK( status ) )
		{
		buffer[ length ] = '\0';
		printf( "  certificatePolicies.policyInformation.policyIdentifier = "
				"%s.\n", buffer );
		status = cryptGetAttributeString( certificate,
							CRYPT_CERTINFO_CERTPOLICY_CPSURI, buffer, &length );
		if( cryptStatusOK( status ) )
			{
			buffer[ length ] = '\0';
			printf( "  certificatePolicies.policyInformation.cpsURI = "
					"%s.\n", buffer );
			}
		status = cryptGetAttributeString( certificate,
							CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION, buffer, &length );
		if( cryptStatusOK( status ) )
			{
			buffer[ length ] = '\0';
			printf( "  certificatePolicies.policyInformation.organisation = "
					"%s.\n", buffer );
			}
		status = cryptGetAttributeString( certificate,
							CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT, buffer, &length );
		if( cryptStatusOK( status ) )
			{
			buffer[ length ] = '\0';
			printf( "  certificatePolicies.policyInformation.explicitText = "
					"%s.\n", buffer );
			}
		}
	if( cryptStatusOK( \
			cryptGetAttribute( certificate,
							   CRYPT_CERTINFO_CRLDIST_FULLNAME, &value ) ) )
		{
		CHK( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT,
								CRYPT_CERTINFO_CRLDIST_FULLNAME ) );
		puts( "  crlDistributionPoint is/are:" );
		do
			{
			printDN( certificate );
			printAltName( certificate );
			}
		while( cryptSetAttribute( certificate, CRYPT_ATTRIBUTE_CURRENT_INSTANCE,
								  CRYPT_CURSOR_NEXT ) == CRYPT_OK );
		}

	return( TRUE );
	}

int printCertChainInfo( const CRYPT_CERTIFICATE certChain )
	{
	int value, count, status;

	/* Make sure it really is a certificate chain */
	CHK( cryptGetAttribute( certChain, CRYPT_CERTINFO_CERTTYPE, &value ) );
	if( value != CRYPT_CERTTYPE_CERTCHAIN )
		{
		printCertInfo( certChain );
		return( TRUE );
		}

	/* Display info on each certificate in the chain.  This uses the cursor
	   mechanism to select successive certificates in the chain from the 
	   leaf up to the root */
	count = 0;
	CHK( cryptSetAttribute( certChain, CRYPT_CERTINFO_CURRENT_CERTIFICATE,
							CRYPT_CURSOR_FIRST ) );
	do
		{
		printf( "Certificate %d\n-------------\n", count++ );
		printCertInfo( certChain );
		printf( "\n" );
		}
	while( cryptSetAttribute( certChain,
			CRYPT_CERTINFO_CURRENT_CERTIFICATE, CRYPT_CURSOR_NEXT ) == CRYPT_OK );

	/* Reset the cursor position in the chain */
	CHK( cryptSetAttribute( certChain, CRYPT_CERTINFO_CURRENT_CERTIFICATE,
							CRYPT_CURSOR_FIRST ) );

	return( TRUE );
	}
