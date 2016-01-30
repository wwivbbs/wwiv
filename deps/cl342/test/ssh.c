/****************************************************************************
*																			*
*							cryptlib SSH Test Routines						*
*						Copyright Peter Gutmann 1998-2005					*
*																			*
****************************************************************************/

#include "cryptlib.h"
#include "test/test.h"

/* Various features can be disabled by configuration options, in order to 
   handle this we need to include the cryptlib config file so that we can 
   selectively disable some tests */

#ifdef __WINDOWS__
  /* For checking for debug-only capabilities */
  #define _OSSPEC_DEFINED
  #define VC_LT_2005( version )		( version < 1400 )
#endif /* __WINDOWS__ */
#include "misc/config.h"

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */

/* Uncomment the following to ask the user for a password rather than using
   a hardcoded password when testing against live accounts */

/* #define USER_SUPPLIED_PASSWORD */
#ifdef USER_SUPPLIED_PASSWORD
  #undef SSH2_SERVER_NAME
  #undef SSH_USER_NAME
  #define SSH2_SERVER_NAME	"testserver"
  #define SSH_USER_NAME		"testname"
#endif /* USER_SUPPLIED_PASSWORD */

/* We can run the SSH self-test with a large variety of options, rather than
   using dozens of boolean option flags to control them all we define
   various test classes that exercise each option type */

typedef enum {
	SSH_TEST_NORMAL,			/* Standard SSHv2 test */
	SSH_TEST_SSH1,				/* SSHv1 test */
	SSH_TEST_DSAKEY,			/* DSA server key instead of RSA */
	SSH_TEST_ECCKEY,			/* ECDSA server key instead of RSA */
	SSH_TEST_CLIENTCERT,		/* Use client public-key for auth */
	SSH_TEST_SUBSYSTEM,			/* Test SFTP subsystem */
	SSH_TEST_PORTFORWARDING,	/* Test port forwarding */
	SSH_TEST_EXEC,				/* Test rexec rather than rsh functionality */
	SSH_TEST_MULTICHANNEL,		/* Test multi-channel handling */
	SSH_TEST_FINGERPRINT,		/* Test (invalid) key fingerprint */
	SSH_TEST_CONFIRMAUTH,		/* Test manual server confirmation of auth.*/
	SSH_TEST_DUALTHREAD,
	SSH_TEST_DUALTHREAD2		/* Two-phase connect via different threads */
	} SSH_TEST_TYPE;

#if defined( TEST_SESSION ) || defined( TEST_SESSION_LOOPBACK )

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Test the ability to parse URLs */

typedef struct {
	const C_STR url;			/* Server URL */
	const C_STR name;			/* Parsed server name */
	const int port;				/* Parsed server port */
	const C_STR userInfo;		/* Parsed user info */
	} URL_PARSE_INFO;

static const FAR_BSS URL_PARSE_INFO urlParseInfo[] = {
	/* IP address forms */
	{ TEXT( "1.2.3.4" ), TEXT( "1.2.3.4" ), 0, NULL },
	{ TEXT( "1.2.3.4:80" ), TEXT( "1.2.3.4" ), 80, NULL },
	{ TEXT( "user@1.2.3.4" ), TEXT( "1.2.3.4" ), 0, TEXT( "user" ) },
	{ TEXT( "[1:2:3:4]" ), TEXT( "[1:2:3:4]" ), 0, NULL },
	{ TEXT( "[1:2:3:4]:80" ), TEXT( "[1:2:3:4]" ), 80, NULL },
	{ TEXT( "user@[1:2:3:4]" ), TEXT( "[1:2:3:4]" ), 0, TEXT( "user" ) },
	{ TEXT( "[::1]" ), TEXT( "[::1]" ), 0, NULL },

	/* General URI forms */
	{ TEXT( "www.server.com" ), TEXT( "www.server.com" ), 0, NULL },
	{ TEXT( "www.server.com:80" ), TEXT( "www.server.com" ), 80, NULL },
	{ TEXT( "http://www.server.com/" ), TEXT( "www.server.com" ), 0, NULL },
	{ TEXT( "http://www.server.com:80" ), TEXT( "www.server.com" ), 80, NULL },
	{ TEXT( "http://user@www.server.com:80" ), TEXT( "www.server.com" ), 80, TEXT( "user" ) },
	{ TEXT( "http://www.server.com/location.php" ), TEXT( "www.server.com" ), 0, NULL },
	{ TEXT( "http://www.server.com:80/location.php" ), TEXT( "www.server.com" ), 80, NULL },
	{ TEXT( "http://www.server.com/location1/location2/location.php" ), TEXT( "www.server.com" ), 0, NULL },

	/* Spurious whitespace */
	{ TEXT( "  www.server.com  :   80 " ), TEXT( "www.server.com" ), 80, NULL },
	{ TEXT( " user  @  www.server.com  :   80 " ), TEXT( "www.server.com" ), 80, NULL },
	{ TEXT( "http:// user  @ www.server.com  :   80 " ), TEXT( "www.server.com" ), 80, TEXT( "user" ) },
	{ TEXT( "www.server.com  :   80 /location.php" ), TEXT( "www.server.com" ), 80, NULL },

	{ NULL, NULL, 0, NULL }
	};

static const FAR_BSS URL_PARSE_INFO invalidUrlParseInfo[] = {
	/* Bad port */
	{ TEXT( "www.server.com:2" ), NULL, 0, NULL },
	{ TEXT( "www.server.com:80abcd" ), NULL, 0, NULL },
	{ TEXT( "www.server.com:abcd" ), NULL, 0, NULL },

	/* Bad general URI */
	{ TEXT( "http://" ), NULL, 0, NULL },
	{ TEXT( "http://xy" ), NULL, 0, NULL },
	{ TEXT( "@www.server.com" ), NULL, 0, NULL },
	{ TEXT( "   @www.server.com" ), NULL, 0, NULL },

	{ NULL, NULL, 0, NULL }
	};

int testSessionUrlParse( void )
	{
	CRYPT_SESSION cryptSession;
	int i, status;

	puts( "Testing session URL parsing..." );

	/* Create a session of the most generic type */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED, CRYPT_SESSION_SSL );
	if( status == CRYPT_ERROR_PARAM3 )	/* SSL session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Set various URLs as the server name and retrieve the parsed form */
	for( i = 0; urlParseInfo[ i ].url != NULL; i++ )
		{
		C_CHR nameBuffer[ 256 ], userInfoBuffer[ 256 ];
		int nameLength, userInfoLength = DUMMY_INIT, port = DUMMY_INIT;

		/* Clear any leftover attributes from previous tests */
		memset( nameBuffer, 0, 16 );
		memset( userInfoBuffer, 0, 16 );
		cryptDeleteAttribute( cryptSession, CRYPT_SESSINFO_SERVER_NAME );
		cryptDeleteAttribute( cryptSession, CRYPT_SESSINFO_SERVER_PORT );
		cryptDeleteAttribute( cryptSession, CRYPT_SESSINFO_USERNAME );

		/* Set the URL */
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SERVER_NAME,
										  urlParseInfo[ i ].url,
										  paramStrlen( urlParseInfo[ i ].url ) );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't set URL '%s', status %d, line %d.\n",
					urlParseInfo[ i ].url, status, __LINE__ );
			return( FALSE );
			}

		/* Make sure that the parsed form is OK */
		status = cryptGetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SERVER_NAME,
										  nameBuffer, &nameLength );
		if( cryptStatusOK( status ) && urlParseInfo[ i ].port )
			status = cryptGetAttribute( cryptSession,
										CRYPT_SESSINFO_SERVER_PORT, &port );
		if( cryptStatusOK( status ) && urlParseInfo[ i ].userInfo != NULL )
			{
			status = cryptGetAttributeString( cryptSession,
											  CRYPT_SESSINFO_USERNAME,
											  userInfoBuffer,
											  &userInfoLength );
			}
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't get parsed URL info for '%s', status %d, "
					"line %d.\n", urlParseInfo[ i ].url, status, __LINE__ );
			return( FALSE );
			}
		if( paramStrlen( urlParseInfo[ i ].name ) != ( size_t ) nameLength || \
			memcmp( nameBuffer, urlParseInfo[ i ].name, nameLength ) || \
			( urlParseInfo[ i ].port && port != urlParseInfo[ i ].port ) || \
			( urlParseInfo[ i ].userInfo != NULL && \
			  memcmp( userInfoBuffer, urlParseInfo[ i ].userInfo,
			  userInfoLength ) ) )
			{
			printf( "Parsed URL info for '%s' is incorrect, line %d.\n",
					urlParseInfo[ i ].url, __LINE__ );
			return( FALSE );
			}
		}

	/* Now try it with invalid URLs */
	for( i = 0; invalidUrlParseInfo[ i ].url != NULL; i++ )
		{
		/* Clear any leftover attributes from previous tests */
		cryptDeleteAttribute( cryptSession, CRYPT_SESSINFO_SERVER_NAME );
		cryptDeleteAttribute( cryptSession, CRYPT_SESSINFO_SERVER_PORT );
		cryptDeleteAttribute( cryptSession, CRYPT_SESSINFO_USERNAME );

		/* Set the URL */
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SERVER_NAME,
										  invalidUrlParseInfo[ i ].url,
										  paramStrlen( invalidUrlParseInfo[ i ].url ) );
		if( cryptStatusOK( status ) )
			{
			printf( "Invalid URL '%s' was accepted as valid, line %d.\n",
					invalidUrlParseInfo[ i ].url, __LINE__ );
			return( FALSE );
			}
		}

	/* Clean up */
	status = cryptDestroySession( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroySession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( "Session URL parsing succeeded.\n" );
	return( TRUE );
	}

/* Test session attribute handling */

int testSessionAttributes( void )
	{
	CRYPT_SESSION cryptSession;
	int status;

	puts( "Testing session attribute handling..." );

	/* Create a server session of the most generic type */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_SSL_SERVER );
	if( status == CRYPT_ERROR_PARAM3 )	/* SSL server session access not avail.*/
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add an initial attribute */
	status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_SERVER_NAME, TEXT( "servername" ),
								paramStrlen( TEXT( "servername" ) ) );
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttributeString() failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Add several username/password pairs */
	status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_USERNAME, TEXT( "test1" ),
								paramStrlen( TEXT( "test1" ) ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_PASSWORD, TEXT( "test1" ),
								paramStrlen( TEXT( "test1" ) ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_USERNAME, TEXT( "test2" ),
								paramStrlen( TEXT( "test2" ) ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_PASSWORD, TEXT( "test2" ),
								paramStrlen( TEXT( "test2" ) ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_USERNAME, TEXT( "test3" ),
								paramStrlen( TEXT( "test3" ) ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_PASSWORD, TEXT( "test3" ),
								paramStrlen( TEXT( "test3" ) ) );
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttributeString() for username/password pairs "
				"failed with error code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Add a duplicate entry and make sure that it's detected */
	status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_USERNAME, TEXT( "test2" ),
								paramStrlen( TEXT( "test2" ) ) );
	if( status != CRYPT_ERROR_DUPLICATE )
		{
		printf( "Addition of duplicate user/password entry wasn't detected, "
				"line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Add a password without a preceding username and make sure that it's
	   detected */
	status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_PASSWORD, TEXT( "invalid_pw" ),
								paramStrlen( TEXT( "invalid_pw" ) ) );
	if( status != CRYPT_ERROR_NOTINITED )
		{
		printf( "Addition of password without username wasn't detected, "
				"line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Add a username without a password and make sure that it's detected */
	status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_USERNAME, TEXT( "valid_name" ),
								paramStrlen( TEXT( "valid_name" ) ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_USERNAME, TEXT( "invalid_name" ),
								paramStrlen( TEXT( "invalid_name" ) ) );
	if( status != CRYPT_ERROR_INITED )
		{
		printf( "Addition of username without password wasn't detected, "
				"line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	status = cryptDestroySession( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroySession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( "Session attribute handling succeeded.\n" );
	return( TRUE );
	}

/****************************************************************************
*																			*
*							SSH Utility Functions							*
*																			*
****************************************************************************/

#if defined( WINDOWS_THREADS ) || defined( UNIX_THREADS )

/* Test the ability to have multiple server threads waiting on a session.
   Since this requries (OS-specific) threading, we just use two sample
   systems, Win32 (Windows threads) and Linux (pthreads).  Since Linux's
   somewhat strange not-quite-a-thread/not-quite-a-process implementation
   can be a bit buggy, we also use another sample pthreads implementation
   (FreeBSD/NetBSD) as a sanity check */

#ifdef WINDOWS_THREADS
  unsigned __stdcall sshServerMultiThread( void *dummy )
#else
  void *sshServerMultiThread( void *dummy )
#endif /* Windows vs. pthreads */
	{
	CRYPT_SESSION cryptSession;
	CRYPT_CONTEXT privateKey;
	BYTE filenameBuffer[ FILENAME_BUFFER_SIZE ];
#ifdef UNICODE_STRINGS
	wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];
#endif /* UNICODE_STRINGS */
	void *fileNamePtr = filenameBuffer;
	int status;

	printf( "Server thread %lX activated.\n", 
			( unsigned long ) THREAD_SELF() );
	fflush( stdout );

	/* Create the session and try to activate it.  We don't do anything
	   beyond that point since this is a test of multi-thread handling
	   capability, not session handling */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_SSH_SERVER );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		THREAD_EXIT();
		}
	if( !setLocalConnect( cryptSession, 22 ) )
		{
		THREAD_EXIT();
		}
	filenameFromTemplate( filenameBuffer, SSH_PRIVKEY_FILE_TEMPLATE, 1 );
#ifdef UNICODE_STRINGS
	mbstowcs( wcBuffer, filenameBuffer, strlen( filenameBuffer ) + 1 );
	fileNamePtr = wcBuffer;
#endif /* UNICODE_STRINGS */
	status = getPrivateKey( &privateKey, fileNamePtr, USER_PRIVKEY_LABEL, 
							TEST_PRIVKEY_PASSWORD );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( cryptSession,
									CRYPT_SESSINFO_PRIVATEKEY, privateKey );
		cryptDestroyContext( privateKey );
		}
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_AUTHRESPONSE,
									TRUE );
	if( cryptStatusError( status ) )
		{
		printf( "Private key read/set failed with error code %d, line %d.\n", 
				status, __LINE__ );
		THREAD_EXIT();
		}
	printf( "Server for thread %lX activated.\n", 
			( unsigned long ) THREAD_SELF() );
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	printConnectInfo( cryptSession );
	if( cryptStatusError( status ) )
		printExtError( cryptSession,
					   "Attempt to activate SSH server session", status,
					   __LINE__ );
	cryptDestroySession( cryptSession );
	printf( "Server for thread %lX has exited.\n", 
			( unsigned long ) THREAD_SELF() );
	fflush( stdout );

	THREAD_EXIT();
	}

#ifdef WINDOWS_THREADS
  unsigned __stdcall sshClientMultiThread( void *dummy )
#else
  void *sshClientMultiThread( void *dummy )
#endif /* Windows vs. pthreads */
	{
	CRYPT_SESSION cryptSession;
	int status;

	printf( "Client thread %lX activated.\n", 
			( unsigned long ) THREAD_SELF() );
	fflush( stdout );

	/* Create the session and try to activate it.  We don't do anything
	   beyond that point since this is a test of multi-thread handling
	   capability, not session handling */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_SSH );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		THREAD_EXIT();
		}
	if( !setLocalConnect( cryptSession, 22 ) )
		{
		THREAD_EXIT();
		}
	status = cryptSetAttribute( cryptSession,
								CRYPT_OPTION_NET_CONNECTTIMEOUT, 10 );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_USERNAME,
										  SSH_USER_NAME,
										  paramStrlen( SSH_USER_NAME ) );
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_PASSWORD,
										  SSH_PASSWORD,
										  paramStrlen( SSH_PASSWORD ) );
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute/AttributeString() failed with error code "
				"%d, line %d.\n", status, __LINE__ );
		THREAD_EXIT();
		}
	printf( "Client for thread %lX activated.\n", 
			( unsigned long ) THREAD_SELF() );
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	printConnectInfo( cryptSession );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession,
					   "Attempt to activate SSH client session", status,
					   __LINE__ );
		}
	cryptDestroySession( cryptSession );
	printf( "Client for thread %lX has exited.\n", 
			( unsigned long ) THREAD_SELF() );
	fflush( stdout );

	THREAD_EXIT();
	}

int testSessionSSHClientServerMultiThread( void )
	{
	return( multiThreadDispatch( sshClientMultiThread, 
								 sshServerMultiThread, MAX_NO_THREADS ) );
	}
#endif /* OS-specific threading functions */

#ifdef USE_SSH_EXTENDED

/* Create an SSH channel */

static int createChannel( const CRYPT_SESSION cryptSession,
						  const C_STR type, const C_STR arg1 )
	{
	int status;

	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_SSH_CHANNEL,
								CRYPT_UNUSED );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SSH_CHANNEL_TYPE,
										  type, paramStrlen( type ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SSH_CHANNEL_ARG1,
										  arg1, paramStrlen( arg1 ) );
	return( status );
	}

/* Print information on an SSH channel */

static BOOLEAN printChannelInfo( const CRYPT_SESSION cryptSession,
								 const SSH_TEST_TYPE testType,
								 const BOOLEAN isServer )
	{
	C_CHR stringBuffer[ CRYPT_MAX_TEXTSIZE + 1 ];
	C_CHR argBuffer[ CRYPT_MAX_TEXTSIZE + 1 ];
	int channel, stringLength, argLength = 0, status;

	status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_SSH_CHANNEL,
								&channel );
	if( cryptStatusOK( status ) )
		status = cryptGetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SSH_CHANNEL_TYPE,
										  stringBuffer, &stringLength );
	if( cryptStatusError( status ) )
		{
		printf( "%sCouldn't query channel ID/type, status %d, line %d.\n",
				isServer ? "SVR: " : "", status, __LINE__ );
		return( FALSE );
		}
#ifdef UNICODE_STRINGS
	stringBuffer[ stringLength / sizeof( wchar_t ) ] = TEXT( '\0' );
#else
	stringBuffer[ stringLength ] = '\0';
#endif /* UNICODE_STRINGS */
	if( !paramStrcmp( stringBuffer, TEXT( "subsystem" ) ) || \
		!paramStrcmp( stringBuffer, TEXT( "direct-tcpip" ) ) )
		{
		status = cryptGetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SSH_CHANNEL_ARG1,
										  argBuffer, &argLength );
		if( cryptStatusError( status ) )
			{
			printf( "%sCouldn't query channel arg, status %d, line %d.\n",
					isServer ? "SVR: " : "", status, __LINE__ );
			return( FALSE );
			}
#ifdef UNICODE_STRINGS
		argBuffer[ argLength / sizeof( wchar_t ) ] = TEXT( '\0' );
		printf( "SVR: Client opened channel #%d, type '%S', arg '%S'.\n",
				channel, stringBuffer, argBuffer );
#else
		argBuffer[ argLength ] = '\0';
		printf( "SVR: Client opened channel #%d, type '%s', arg '%s'.\n",
				channel, stringBuffer, argBuffer );
#endif /* UNICODE_STRINGS */
		fflush( stdout );

		return( TRUE );
		}

	if( testType == SSH_TEST_SUBSYSTEM )
		{
		printf( "SVR: Client requested subsystem but server reported "
				"request as '%s', line %d.\n", stringBuffer, __LINE__ );
		return( FALSE );
		}

#ifdef UNICODE_STRINGS
	printf( "SVR: Client opened channel #%d, type '%S'.\n",
			channel, stringBuffer );
#else
	printf( "SVR: Client opened channel #%d, type '%s'.\n",
			channel, stringBuffer );
#endif /* UNICODE_STRINGS */
	fflush( stdout );
	return( TRUE );
	}
#endif /* USE_SSH_EXTENDED */

/* Print information on data sent over an SSH channel */

static BOOLEAN printDataInfo( CRYPT_SESSION cryptSession,
							  char *buffer, int *bytesCopied,
							  const BOOLEAN isServer )
	{
	int channel = 0, status;

#ifdef USE_SSH_EXTENDED
	status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_SSH_CHANNEL, 
								&channel );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, 
					   isServer ? "SVR: Couldn't get data channel number" : \
								  "Couldn't get data channel number", 
					   status, __LINE__ );
		return( FALSE );
		}
#endif /* USE_SSH_EXTENDED */
	status = cryptPopData( cryptSession, buffer, BUFFER_SIZE, bytesCopied );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, 
					   isServer ? "SVR: Client data read failed" : \
								  "Server data read failed", 
					   status, __LINE__ );
		return( FALSE );
		}
	buffer[ *bytesCopied ] = '\0';
	printf( "%s---- %s sent %d bytes on channel #%d ----\n",
			isServer ? "SVR: " : "", isServer ? "Client" : "Server",
			*bytesCopied, channel );
	if( isServer )
		printf( "SVR: " );
	puts( buffer );
	printf( "%s---- End of output ----\n", isServer ? "SVR: " : "" );
	fflush( stdout );

	return( TRUE );
	}

/* Print information on SSH authorisation info */

static BOOLEAN printAuthInfo( CRYPT_SESSION cryptSession )
	{
	C_CHR stringBuffer[ CRYPT_MAX_TEXTSIZE + 1 ];
	int length, status;

	status = cryptGetAttributeString( cryptSession, CRYPT_SESSINFO_USERNAME,
									  stringBuffer, &length );
	if( cryptStatusOK( status ) )
		{
#ifdef UNICODE_STRINGS
		stringBuffer[ length / sizeof( wchar_t ) ] = TEXT( '\0' );
		printf( "SVR: User name = '%S', ", stringBuffer );
#else
		stringBuffer[ length ] = '\0';
		printf( "SVR: User name = '%s', ", stringBuffer );
#endif /* UNICODE_STRINGS */
		}
	if( cryptStatusOK( status ) )
		status = cryptGetAttributeString( cryptSession, CRYPT_SESSINFO_PASSWORD,
										  stringBuffer, &length );
	if( cryptStatusOK( status ) )
		{
#ifdef UNICODE_STRINGS
		stringBuffer[ length / sizeof( wchar_t ) ] = TEXT( '\0' );
		printf( "password = '%S'.\n", stringBuffer );
#else
		stringBuffer[ length ] = '\0';
		printf( "password = '%s'.\n", stringBuffer );
#endif /* UNICODE_STRINGS */
		}
	if( cryptStatusError( status ) )
		{
		printf( "SVR: Couldn't read client authentication details, "
				"status = %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	fflush( stdout );

	return( TRUE );
	}

/****************************************************************************
*																			*
*								SSH Routines Test							*
*																			*
****************************************************************************/

/* There are various servers running that we can use for testing, the
   following remapping allows us to switch between them.  Notes:

	Server 1: Local loopback.
	Server 2: Sends extraneous lines of text before the SSH ID string
			  (technically allowed by the RFC, but probably not in the way
			  that it's being used here).
	Server 3: Reference ssh.com implementation.
	Server 4: Reference OpenSSH implementation.
	Server 5: OpenSSH with ECC support.  There are two aliases for the same 
			  server, anoncvs is a somewhat nonstandard config that only 
			  allows access via the 'anoncvs' account and is rather abrupt
			  about disconnecting clients, and natsu, which is a more 
			  standard config that behaves more normally.

   To test local -> remote/remote -> local forwarding:

	ssh localhost -v -l test -pw test -L 110:pop3.test.com:110
	ssh localhost -v -l test -pw test -R 110:pop3.test.com:110

  For test purposes we connect to the OpenSSH server for the SSHv2 test
  because this is the most frequently-used one around, so maintaining
  compatibility with it whenever it changes is important.  Using it for test
  connects is slightly antisocial but in practice few people seem to run the
  self-test and we never get past the initial handshake phase so it shouldn't
  be a big deal.  Testing SSHv1 is a bit tricky since there are few of these
  servers still around, in the absence of a convenient test server we just
  try a local connect, but in any case it's been disabled by default for 
  some years so there really isn't anything to test */

static const C_STR FAR_BSS ssh1Info[] = {
	NULL,
	TEXT( "localhost" ),
	NULL
	};
static const C_STR FAR_BSS ssh2Info[] = {
	NULL,
	TEXT( "localhost" ),
	TEXT( "sorrel.humboldt.edu:222" ),
	TEXT( "www.ssh.com" ),
	TEXT( "www.openssh.com" ),
/*	TEXT( "anoncvs.mindrot.org" ),	See comment above */
	TEXT( "natsu.mindrot.org" ),
	NULL
	};

#define SSH1_SERVER_NO	1
#define SSH2_SERVER_NO	4

/* If we're testing dual-thread handling of sessions, we need to provide a
   forward declaration of the threading function since it's called from 
   within the SSH connect code */

#ifdef WINDOWS_THREADS
  unsigned __stdcall ssh2ServerDualThread2( void *dummy );
#endif /* WINDOWS_THREADS */

/* Establish an SSH session.  The generic SSHv1 client test will always step
   up to SSHv2 if the server is v2 (which almost all are), so v1 can't
   easily be generically tested without hardcoding v1-only into
   session/ssh.c.  However, the loopback test, which forces the use of a
   v1-only server, does test the client as a v1 client */

static int connectSSH( const CRYPT_SESSION_TYPE sessionType,
					   const SSH_TEST_TYPE testType,
					   const BOOLEAN localSession )
	{
	CRYPT_SESSION cryptSession;
#ifdef SSH2_SERVER_NAME
	const C_STR serverName = SSH2_SERVER_NAME;
#else
	const C_STR serverName = localSession ? TEXT( "localhost" ) : \
							 ( testType == SSH_TEST_SSH1 ) ? \
								ssh1Info[ SSH1_SERVER_NO ] : \
								ssh2Info[ SSH2_SERVER_NO ];
#endif /* SSH2_SERVER_NAME */
	const BOOLEAN isServer = ( sessionType == CRYPT_SESSION_SSH_SERVER ) ? \
							   TRUE : FALSE;
	char buffer[ BUFFER_SIZE ];
#ifdef USE_SSH_EXTENDED
	int channel;
#endif /* USE_SSH_EXTENDED */
	int bytesCopied, status;

	/* If this is a local session, synchronise the client and server */
	if( localSession )
		{
		if( isServer )
			{
			/* Acquire the init mutex */
			acquireMutex();
			}
		else
			{
			/* We're the client Wait for the server to finish initialising */
			if( waitMutex() == CRYPT_ERROR_TIMEOUT )
				{
				printf( "Timed out waiting for server to initialise, "
						"line %d.\n", __LINE__ );
				return( FALSE );
				}
			}
		}

	/* If this is the dual-thread server test and we're the second server 
	   thread, skip the portions that have already been handled by the first 
	   thread */
#ifdef WINDOWS_THREADS
	if( isServer && testType == SSH_TEST_DUALTHREAD2 )
		goto dualThreadContinue;
#endif /* WINDOWS_THREADS */

	printf( "%sTesting %sSSH%s%s session...\n",
			isServer ? "SVR: " : "",
			localSession ? "local " : "",
			( testType == SSH_TEST_SSH1 ) ? "v1" : "v2",
			( testType == SSH_TEST_DSAKEY ) ? " with DSA server key" : \
			( testType == SSH_TEST_ECCKEY ) ? " with ECDSA server key" : \
			( testType == SSH_TEST_SUBSYSTEM ) ? " SFTP" : \
			( testType == SSH_TEST_PORTFORWARDING ) ? " port-forwarding" : \
			( testType == SSH_TEST_EXEC ) ? " remote exec" : \
			( testType == SSH_TEST_MULTICHANNEL ) ? " multi-channel" : \
			( testType == SSH_TEST_CLIENTCERT ) ? " pubkey-auth" : "" );
	if( !isServer && !localSession )
		{
#ifdef UNICODE_STRINGS
		printf( "  Remote host: %S.\n", serverName );
#else
		printf( "  Remote host: %s.\n", serverName );
#endif /* UNICODE_STRINGS */
		}
	fflush( stdout );

	/* Create the session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED, sessionType );
	if( status == CRYPT_ERROR_PARAM3 )	/* SSH session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Set up the server and user information and activate the session */
	if( isServer )
		{
		CRYPT_CONTEXT privateKey;
		BYTE filenameBuffer[ FILENAME_BUFFER_SIZE ];
#ifdef UNICODE_STRINGS
		wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];
#endif /* UNICODE_STRINGS */
		void *fileNamePtr = filenameBuffer;

		if( !setLocalConnect( cryptSession, 22 ) )
			return( FALSE );
		filenameFromTemplate( filenameBuffer, SSH_PRIVKEY_FILE_TEMPLATE, 
							  ( testType == SSH_TEST_ECCKEY ) ? 3 : \
							  ( testType == SSH_TEST_DSAKEY ) ? 2 : 1 );
#ifdef UNICODE_STRINGS
		mbstowcs( wcBuffer, filenameBuffer, strlen( filenameBuffer ) + 1 );
		fileNamePtr = wcBuffer;
#endif /* UNICODE_STRINGS */
		status = getPrivateKey( &privateKey, fileNamePtr, USER_PRIVKEY_LABEL, 
								TEST_PRIVKEY_PASSWORD );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_PRIVATEKEY, privateKey );
			cryptDestroyContext( privateKey );
			}
		}
	else
		{
		if( localSession )
			{
			if( !setLocalConnect( cryptSession, 22 ) )
				return( FALSE );
			}
		else
			{
			status = cryptSetAttributeString( cryptSession,
									CRYPT_SESSINFO_SERVER_NAME,
									serverName, paramStrlen( serverName ) );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttributeString( cryptSession,
									CRYPT_SESSINFO_USERNAME,
									SSH_USER_NAME,
									paramStrlen( SSH_USER_NAME ) );
			}
		if( cryptStatusOK( status ) )
			{
			if( testType == SSH_TEST_CLIENTCERT )
				{
				CRYPT_CONTEXT privateKey;

				status = getPrivateKey( &privateKey, USER_PRIVKEY_FILE,
									USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
				if( cryptStatusOK( status ) )
					{
					status = cryptSetAttribute( cryptSession,
									CRYPT_SESSINFO_PRIVATEKEY, privateKey );
					cryptDestroyContext( privateKey );
					}
				}
			else
				{
#ifdef USER_SUPPLIED_PASSWORD
				char password[ 256 ];

				printf( "Enter SSHv2 server password: " );
				fgets( password, 255, stdin );
				password[ strlen( password ) - 1 ] = '\0';
				status = cryptSetAttributeString( cryptSession,
									CRYPT_SESSINFO_PASSWORD,
									password, strlen( password ) );
#else
				status = cryptSetAttributeString( cryptSession,
									CRYPT_SESSINFO_PASSWORD,
									SSH_PASSWORD,
									paramStrlen( SSH_PASSWORD ) );
#endif /* User-supplied password */
				}
			}
#ifdef USE_SSH_EXTENDED
		if( cryptStatusOK( status ) && \
			( testType == SSH_TEST_SUBSYSTEM ) )
			{
			status = createChannel( cryptSession, TEXT( "subsystem" ),
									TEXT( "sftp" ) );
			}
		if( cryptStatusOK( status ) && \
			( testType == SSH_TEST_PORTFORWARDING || \
			  testType == SSH_TEST_MULTICHANNEL ) )
			{
			status = createChannel( cryptSession, TEXT( "direct-tcpip" ),
									TEXT( "localhost:1234" ) );
			}
#endif /* USE_SSH_EXTENDED */
		if( cryptStatusOK( status ) && \
			( testType == SSH_TEST_FINGERPRINT ) )
			{
			BYTE fingerPrint[ CRYPT_MAX_HASHSIZE ];

			/* Set a dummy (all-zero) fingerprint to force the connect to
			   fail */
			memset( fingerPrint, 0, CRYPT_MAX_HASHSIZE );
			status = cryptSetAttributeString( cryptSession,
											  CRYPT_SESSINFO_SERVER_FINGERPRINT,
											  fingerPrint, 16 );
			}
#ifdef USE_SSH_EXTENDED
		if( cryptStatusOK( status ) && \
			( testType == SSH_TEST_EXEC ) )
			{
			status = createChannel( cryptSession, TEXT( "exec" ),
									TEXT( "/bin/netstat" ) );
			}
#endif /* USE_SSH_EXTENDED */
		}
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_VERSION,
									( testType == SSH_TEST_SSH1 ) ? 1 : 2 );
	if( cryptStatusOK( status ) && isServer && \
		( testType != SSH_TEST_CONFIRMAUTH && \
		  testType != SSH_TEST_DUALTHREAD ) )
		{
		/* If we're not testing manual confirmation of client auth, have
		   cryptlib automatically confirm the auth */
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_AUTHRESPONSE,
									TRUE );
		}
	if( cryptStatusError( status ) )
		{
		/* If we're trying to enable SSHv1 and it fails, this isn't an error
		   since this protocol is disabled by default */
		if( testType == SSH_TEST_SSH1 )
			{
			puts( "SSHv1 appears to be disbled in this build." );
			cryptDestroySession( cryptSession );
			puts( isServer ? "SVR: SSH server session succeeded.\n" : \
							 "SSH client session succeeded.\n" );
			return( CRYPT_ERROR_NOTAVAIL );
			}

		printf( "%scryptSetAttribute/AttributeString() failed with error "
				"code %d, line %d.\n", isServer ? "SVR: " : "", status, 
				__LINE__ );
		return( FALSE );
		}

	/* Activate the session.  Since we need to be able to process out-of-
	   band signalling such as channel control messages, we set a non-zero
	   timeout for reads */
	cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_READTIMEOUT, 5 );
	if( localSession )
		{
		/* For the loopback test we also increase the connection timeout to 
		   a higher-than-normal level, since this gives us more time for
		   tracing through the code when debugging */
		cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_CONNECTTIMEOUT, 
						   120 );
		}
	if( localSession && isServer )
		{
		/* Tell the client that we're ready to go */
		releaseMutex();
		}
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( isServer )
		{
#ifdef WINDOWS_THREADS
		if( isServer && testType == SSH_TEST_DUALTHREAD && \
			status == CRYPT_ENVELOPE_RESOURCE )
			{
			static CRYPT_SESSION localCryptSession = 0;
			unsigned threadID;

			/* Start a second thread to complete the handshake and exit */
			localCryptSession = cryptSession;
			_beginthreadex( NULL, 0, ssh2ServerDualThread2, NULL, 0, 
							&threadID );
			return( TRUE );

			/* The second thread continues from here */
dualThreadContinue:
			assert( localSession > 0 );
			cryptSession = localCryptSession;

			/* Allow the auth.and complete the handshake */
			puts( "SVR: Confirming authentication to client..." );
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_AUTHRESPONSE, TRUE );
			if( cryptStatusOK( status ) )
				status = cryptSetAttribute( cryptSession,
											CRYPT_SESSINFO_ACTIVE, TRUE );
			}
#endif /* WINDOWS_THREADS */
		if( status == CRYPT_ENVELOPE_RESOURCE )
			{
			/* The client has tried to authenticate themselves, print the
			   info */
			if( !printAuthInfo( cryptSession ) )
				return( FALSE );

			/* Deny the auth.and force them to retry, unless it's a loopback
			   test which is non-interactive and for which the client can't
			   perform an interactive re-auth */
			if( !localSession )
				{
				puts( "SVR: Denying authentication to client, who should "
					  "reauth..." );
				status = cryptSetAttribute( cryptSession,
											CRYPT_SESSINFO_AUTHRESPONSE, 
											FALSE );
				if( cryptStatusOK( status ) )
					status = cryptSetAttribute( cryptSession,
												CRYPT_SESSINFO_ACTIVE, TRUE );
				if( status != CRYPT_ENVELOPE_RESOURCE )
					{
					printExtError( cryptSession, 
								   "SVR: Attempt to deny auth.to client", 
								   status, __LINE__ );
					return( FALSE );
					}
				if( !printAuthInfo( cryptSession ) )
					return( FALSE );
				}

			/* Allow the auth.and complete the handshake */
			puts( "SVR: Confirming authentication to client..." );
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_AUTHRESPONSE, TRUE );
			if( cryptStatusOK( status ) )
				status = cryptSetAttribute( cryptSession,
											CRYPT_SESSINFO_ACTIVE, TRUE );
			}

		/* Now that the handshake is complete, display the connection info */
		if( cryptStatusOK( status ) && !printConnectInfo( cryptSession ) )
			return( FALSE );
		}
	if( cryptStatusError( status ) )
		{
		if( testType == SSH_TEST_FINGERPRINT )
			{
			/* We've forced the connect to fail by using a dummy fingerprint,
			   everything is OK */
			if( isServer )
				printf( "SVR: " );
			puts( "SSH client rejected key with invalid fingerprint." );
			cryptDestroySession( cryptSession );
			puts( isServer ? "SVR: SSH server session succeeded.\n" : \
							 "SSH client session succeeded.\n" );
			fflush( stdout );
			return( TRUE );
			}
		printExtError( cryptSession, isServer ? \
					   "SVR: Attempt to activate SSH server session" : \
					   "Attempt to activate SSH client session", status,
					   __LINE__ );
		if( localSession )
			{
			/* If it's a local session then none of the following soft-
			   failure conditions are valid */
			return( FALSE );
			}
		if( !isServer && isServerDown( cryptSession, status ) )
			{
			puts( "  (Server could be down, faking it and continuing...)\n" );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_FAILED );
			}
		cryptDestroySession( cryptSession );
		if( status == CRYPT_ERROR_WRONGKEY )
			{
			/* This is another possible soft error condition, the default
			   username and password shouldn't be able to get into many
			   machines */
			puts( "  (Incorrect username/password, continuing...)\n" );
			return( TRUE );
			}
		if( status == CRYPT_ERROR_NOSECURE )
			{
			/* Another soft error condition, the server can't handle the
			   security level we want (usually occurs when trying to perform
			   an SSHv2 connect to an SSHv1 server) */
			puts( "  (Insufficiently secure protocol parameters, "
				  "continuing...)\n" );
			return( TRUE );
			}
		return( FALSE );
		}
	if( testType == SSH_TEST_FINGERPRINT )
		{
		printf( "Attempt to connect with invalid key fingerprint succeeded "
				"when it should\nhave failed, line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Report the session security info.  In standard SSH usage 
	   channel == session so we only try and report channel details if the
	   SSH extended capabilities are enabled */
	if( !printSecurityInfo( cryptSession, isServer, TRUE, FALSE, FALSE ) )
		return( FALSE );
#ifdef USE_SSH_EXTENDED
	status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_SSH_CHANNEL,
								&channel );
	if( cryptStatusError( status ) )
		{
		printf( "cryptGetAttributeString() failed with error code "
				"%d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	printf( "%sCurrent channel is #%d.\n", isServer ? "SVR: " : "",
			channel );
#endif /* USE_SSH_EXTENDED */
	fflush( stdout );

	/* Report additional channel-specific information */
	if( isServer )
		{
		/* Display info on any channels that the client has opened.  As with
		   the earlier channel display, we can only do this if SSH extended
		   capabilities are enabled */
#ifdef USE_SSH_EXTENDED
		if( !printChannelInfo( cryptSession, testType, TRUE ) )
			return( FALSE );
#endif /* USE_SSH_EXTENDED */

		/* Process any additional information that the client may throw
		   at us after the user-auth has completed */
		status = cryptPopData( cryptSession, buffer, BUFFER_SIZE,
							   &bytesCopied );
		if( cryptStatusOK( status ) && bytesCopied > 0 )
			{
			printf( "SVR: Client sent additional %d bytes post-"
					"handshake data.\n", bytesCopied );
			fflush( stdout );
			}
#ifdef USE_SSH_EXTENDED
		else
			{
			if( status == CRYPT_ENVELOPE_RESOURCE )
				{
				/* The client performed additional control actions that were
				   handled inline as part of the data-pop, report the
				   details */
				if( !printChannelInfo( cryptSession, testType, TRUE ) )
					return( FALSE );
				}
			}
#endif /* USE_SSH_EXTENDED */
		}

	/* If we're using the SFTP subsystem as a server, use the special-case
	   routines for this */
#if defined( WINDOWS_THREADS ) && 0
	if( testType == SSH_TEST_SUBSYSTEM )
		{
		if( isServer )
			{
			int sftpServer( const CRYPT_SESSION cryptSession );

			status = sftpServer( cryptSession );
			if( cryptStatusError( status ) )
				{
				printf( "SVR: Couldn't receive SFTP data from client, status %d, "
						"line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			cryptDestroySession( cryptSession );
			puts( "SVR: SFTP server session succeeded.\n" );
			fflush( stdout );
			return( TRUE );
			}
		else
			{
			int sftpClient( const CRYPT_SESSION cryptSession );

			status = sftpClient( cryptSession );
			if( cryptStatusError( status ) )
				{
				printf( "Couldn't send SFTP data to server, status %d, line "
						"%d.\n", status, __LINE__ );
				return( FALSE );
				}
			cryptDestroySession( cryptSession );
			puts( "SFTP client session succeeded.\n" );
			fflush( stdout );
			return( TRUE );
			}
		}
#endif /* WINDOWS_THREADS && 0 */

#ifdef USE_SSH_EXTENDED
	/* If we're performing a multi-channel test, open a second channel (the
	   server handles this as part of its general connect-handling) */
	if( testType == SSH_TEST_MULTICHANNEL && !isServer )
		{
		status = createChannel( cryptSession, TEXT( "direct-tcpip" ),
								TEXT( "localhost:5678" ) );
		if( cryptStatusOK( status ) )
			status = cryptGetAttribute( cryptSession,
										CRYPT_SESSINFO_SSH_CHANNEL,
										&channel );
		if( cryptStatusOK( status ) )
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE,
										TRUE );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't open second SSH chanel, status %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		printf( "Opened additional channel #%d to server.\n", channel );
		fflush( stdout );
		}
#endif /* USE_SSH_EXTENDED */

	/* Send data over the SSH link */
	if( isServer )
		{
		/* Send a status message to the client */
		status = cryptPushData( cryptSession, "Welcome to cryptlib, now go "
								"away.\r\n", 35, &bytesCopied );
		if( cryptStatusOK( status ) )
			status = cryptFlushData( cryptSession );
		if( cryptStatusError( status ) || bytesCopied != 35 )
			{
			printf( "SVR: Couldn't send data to client, status %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		}

	/* Wait a bit while data arrives */
	delayThread( 2 );

	/* Print the first lot of output from the other side */
	if( !printDataInfo( cryptSession, buffer, &bytesCopied, isServer ) )
		return( FALSE );

	/* If we're the server, echo the command to the client */
	if( isServer )
		{
		const int clientBytesCopied = bytesCopied;
		int dummy, i;

		/* If it's a multi-channel test, send the response back on a
		   different channel.  The currently-selected channel will be the
		   last one that the client opened (#2), so we can hardcode in
		   #1 for testing purposes */
		if( testType == SSH_TEST_MULTICHANNEL )
			{
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_SSH_CHANNEL, 1 );
			if( cryptStatusError( status ) )
				{
				printf( "SVR: Couldn't select channel #1 to return data to "
						"client, status %d, line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			}
		for( i = 0; i < clientBytesCopied; i++ )
			{
			if( buffer[ i ] < ' ' || buffer[ i ] >= 0x7F )
				buffer[ i ] = '.';
			}
		status = cryptPushData( cryptSession, "Input was [", 11, &dummy );
		if( cryptStatusOK( status ) && clientBytesCopied > 0 )
			status = cryptPushData( cryptSession, buffer, clientBytesCopied,
									&bytesCopied );
		if( cryptStatusOK( status ) )
			status = cryptPushData( cryptSession, "]\r\n", 3, &dummy );
		if( cryptStatusOK( status ) )
			status = cryptFlushData( cryptSession );
		if( cryptStatusError( status ) || bytesCopied != clientBytesCopied )
			{
			printf( "SVR: Couldn't send data to client, status %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		}
	else
		{
		/* We're the client, if it's a session to a Unix ssh server, send a
		   sample command and display the output */
		if( !localSession )
			{
			/* Send a command to the server and get the results */
			status = cryptPushData( cryptSession, "ls -l | head -25\n", 18,
									&bytesCopied );
			if( cryptStatusOK( status ) )
				status = cryptFlushData( cryptSession );
			if( cryptStatusError( status ) || bytesCopied != 18 )
				{
				printf( "Couldn't send data to server, status %d, line "
						"%d.\n", status, __LINE__ );
				return( FALSE );
				}
			puts( "Sent 'ls -l | head -25'" );
			delayThread( 3 );
			if( !printDataInfo( cryptSession, buffer, &bytesCopied, 
								isServer ) )
				return( FALSE );
			}
		else
			{
			/* It's a local session, just send a simple text string for
			   testing */
			status = cryptPushData( cryptSession, "Some test data", 14,
									&bytesCopied );
			if( cryptStatusOK( status ) )
				status = cryptFlushData( cryptSession );
			if( cryptStatusError( status ) || bytesCopied != 14 )
				{
				printf( "Couldn't send data to server, status %d, line "
						"%d.\n", status, __LINE__ );
				return( FALSE );
				}

			/* Make sure that we stay around long enough to get the
			   server's response */
			delayThread( 1 );

			/* Print the server's response */
			if( !printDataInfo( cryptSession, buffer, &bytesCopied, 
								isServer ) )
				return( FALSE );
			}
		}

	/* If we're performing a multi-channel test, close the second channel */
	if( testType == SSH_TEST_MULTICHANNEL )
		{
		if( isServer )
			{
			/* Perform a dummy pop to process the channel close */
			( void ) cryptPopData( cryptSession, buffer, BUFFER_SIZE, 
								   &bytesCopied );
			}
		else
			{
			/* Close the current channel */
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE,
										FALSE );
			if( cryptStatusError( status ) )
				{
				printf( "Couldn't close second SSH chanel, status %d, line "
						"%d.\n", status, __LINE__ );
				return( FALSE );
				}
			printf( "Closed second channel to server.\n" );
			fflush( stdout );
			}
		}

	/* Clean up */
	status = cryptDestroySession( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroySession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	puts( isServer ? "SVR: SSH server session succeeded.\n" : \
					 "SSH client session succeeded.\n" );
	fflush( stdout );
	return( TRUE );
	}

int testSessionSSHv1( void )
	{
#ifdef USE_SSH1
	return( connectSSH( CRYPT_SESSION_SSH, SSH_TEST_SSH1, FALSE ) );
#else
	return( TRUE );
#endif /* USE_SSH1 */
	}
int testSessionSSH( void )
	{
	return( connectSSH( CRYPT_SESSION_SSH, SSH_TEST_NORMAL, FALSE ) );
	}
int testSessionSSHClientCert( void )
	{
	return( connectSSH( CRYPT_SESSION_SSH, SSH_TEST_CLIENTCERT, FALSE ) );
	}
int testSessionSSHPortforward( void )
	{
#ifdef USE_SSH_EXTENDED
	return( connectSSH( CRYPT_SESSION_SSH, SSH_TEST_PORTFORWARDING, FALSE ) );
#else
	return( TRUE );
#endif /* USE_SSH_EXTENDED */
	}
int testSessionSSHExec( void )
	{
#ifdef USE_SSH_EXTENDED
	return( connectSSH( CRYPT_SESSION_SSH, SSH_TEST_EXEC, FALSE ) );
#else
	return( TRUE );
#endif /* USE_SSH_EXTENDED */
	}
int testSessionSSH_SFTP( void )
	{
#ifdef USE_SSH_EXTENDED
	return( connectSSH( CRYPT_SESSION_SSH, SSH_TEST_SUBSYSTEM, FALSE ) );
#else
	return( TRUE );
#endif /* USE_SSH_EXTENDED */
	}
int testSessionSSHv1Server( void )
	{
#ifdef USE_SSH1
	int status;

	createMutex();
	status = connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_SSH1, FALSE );
	destroyMutex();

	return( status );
#else
	return( TRUE );
#endif /* USE_SSH1 */
	}
int testSessionSSHServer( void )
	{
	int status;

	createMutex();
	status = connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_CONFIRMAUTH, FALSE );
	destroyMutex();

	return( status );
	}
int testSessionSSH_SFTPServer( void )
	{
#ifdef USE_SSH_EXTENDED
	int status;

	createMutex();
	status = connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_SUBSYSTEM, FALSE );
	destroyMutex();

	return( status );
#else
	return( TRUE );
#endif /* USE_SSH_EXTENDED */
	}

/* Perform a client/server loopback test */

#ifdef WINDOWS_THREADS

unsigned __stdcall ssh1ServerThread( void *dummy )
	{
	connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_SSH1, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}
unsigned __stdcall ssh2ServerThread( void *dummy )
	{
	connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_NORMAL, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}
unsigned __stdcall ssh2ServerDsaKeyThread( void *dummy )
	{
	connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_DSAKEY, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}
unsigned __stdcall ssh2ServerEccKeyThread( void *dummy )
	{
	connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_ECCKEY, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}
unsigned __stdcall ssh2ServerFingerprintThread( void *dummy )
	{
	connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_FINGERPRINT, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}
unsigned __stdcall ssh2ServerMultichannelThread( void *dummy )
	{
	connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_MULTICHANNEL, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}
unsigned __stdcall ssh2ServerDualThread1( void *dummy )
	{
	connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_DUALTHREAD, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}
unsigned __stdcall ssh2ServerDualThread2( void *dummy )
	{
	connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_DUALTHREAD2, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}
unsigned __stdcall sftpServerThread( void *dummy )
	{
	connectSSH( CRYPT_SESSION_SSH_SERVER, SSH_TEST_SUBSYSTEM, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}

static int sshClientServer( const SSH_TEST_TYPE testType )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0,
							( testType == SSH_TEST_SUBSYSTEM ) ? \
								sftpServerThread : \
							( testType == SSH_TEST_SSH1 ) ? \
								ssh1ServerThread : \
							( testType == SSH_TEST_DSAKEY ) ? \
								ssh2ServerDsaKeyThread : \
							( testType == SSH_TEST_ECCKEY ) ? \
								ssh2ServerEccKeyThread : \
							( testType == SSH_TEST_FINGERPRINT ) ? \
								ssh2ServerFingerprintThread : \
							( testType == SSH_TEST_MULTICHANNEL ) ? \
								ssh2ServerMultichannelThread : \
							( testType == SSH_TEST_DUALTHREAD ) ? \
								ssh2ServerDualThread1 : \
								ssh2ServerThread,
							NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectSSH( CRYPT_SESSION_SSH, testType, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

int testSessionSSHv1ClientServer( void )
	{
#ifdef USE_SSH1
	return( sshClientServer( SSH_TEST_SSH1 ) );
#else
	return( TRUE );
#endif /* USE_SSH1 */
	}
int testSessionSSHClientServer( void )
	{
	return( sshClientServer( SSH_TEST_NORMAL ) );
	}
int testSessionSSHClientServerDsaKey( void )
	{
	return( sshClientServer( SSH_TEST_DSAKEY ) );
	}
int testSessionSSHClientServerEccKey( void )
	{
	/* ECC algorithms may not be available so we only run this test if 
	   they've been enabled */
	if( cryptQueryCapability( CRYPT_ALGO_ECDSA, \
							  NULL ) == CRYPT_ERROR_NOTAVAIL )
		return( TRUE );

	return( sshClientServer( SSH_TEST_ECCKEY ) );
	}
int testSessionSSHClientServerFingerprint( void )
	{
	/* Note that this test tests the correct functioning of a refused
	   connection when an incorrect key fingerprint is used, so it's
	   supposed to fail */
	return( sshClientServer( SSH_TEST_FINGERPRINT ) );
	}
int testSessionSSHClientServerSFTP( void )
	{
	return( sshClientServer( SSH_TEST_SUBSYSTEM ) );
	}
int testSessionSSHClientServerPortForward( void )
	{
#ifdef USE_SSH_EXTENDED
	return( sshClientServer( SSH_TEST_PORTFORWARDING ) );
#else
	return( TRUE );
#endif /* USE_SSH_EXTENDED */
	}
int testSessionSSHClientServerExec( void )
	{
#ifdef USE_SSH_EXTENDED
	return( sshClientServer( SSH_TEST_EXEC ) );
#else
	return( TRUE );
#endif /* USE_SSH_EXTENDED */
	}
int testSessionSSHClientServerMultichannel( void )
	{
#ifdef USE_SSH_EXTENDED
	return( sshClientServer( SSH_TEST_MULTICHANNEL ) );
#else
	return( TRUE );
#endif /* USE_SSH_EXTENDED */
	}
int testSessionSSHClientServerDualThread( void )
	{
	return( sshClientServer( SSH_TEST_DUALTHREAD ) );
	}
#endif /* WINDOWS_THREADS */

/****************************************************************************
*																			*
*							SFTP Routines for SSH							*
*																			*
****************************************************************************/

/* The following code re-uses internal parts of cryptlib, so it provides its
   own dummy functions as stubs for cryptlib-internal ones.  Since this would
   produce link errors when cryptlib is statically linked with the test
   app, we only enable it for the threaded Windows (i.e. DLL) self-test */

#if defined( WINDOWS_THREADS ) && 0

/* The following code is a bare-bones SFTP implementation created purely for
   interop/performance testing of cryptlib's SSH implementation.  It does
   the bare minimum needed to set up an SFTP transfer, and shouldn't be used
   for anything other than testing.

   Rather than creating our own versions of code already present in cryptlib,
   we pull in the cryptlib code wholesale here unless we've built cryptlib as
   a static lib, in which case it'll already be present.  This is a pretty
   ugly hack, but saves having to copy over a pile of cryptlib code.

   Because cryptlib has an internal BYTE type, we need to no-op it out before
   we pull in any cryptlib code */

#undef BYTE
#define BYTE	_BYTE_DUMMY
#ifdef BOOLEAN
  #undef BOOLEAN	/* May be a typedef or a #define */
#endif /* BOOLEAN */
#ifndef STATIC_LIB
  #include "enc_dec/misc_rw.c"
#endif /* Non-static lib cryptlib */
#undef BYTE
#define BYTE	unsigned char

/* Replacements for cryptlib stream routines */

#define sMemDisconnect(	stream )
#define sMemConnect					sMemOpen
#define stell( stream )				( ( stream )->bufPos )

int sSetError( STREAM *stream, const int status )
	{
	stream->status = status;
	return( status );
	}

int sMemOpen( STREAM *stream, void *buffer, const int bufSize )
	{
	memset( stream, 0, sizeof( STREAM ) );
	stream->buffer = ( void * ) buffer;
	stream->bufEnd = bufSize;
	return( CRYPT_OK );
	}

int sread( STREAM *stream, void *buffer, const int count )
	{
	if( stream->bufPos + count > stream->bufEnd )
		{
		sSetError( stream, CRYPT_ERROR_UNDERFLOW );
		return( CRYPT_ERROR_UNDERFLOW );
		}
	memcpy( buffer, stream->buffer + stream->bufPos, count );
	stream->bufPos += count;
	return( CRYPT_OK );
	}

int swrite( STREAM *stream, const void *buffer, const int count )
	{
	if( stream->buffer != NULL )
		{
		if( stream->bufPos + count > stream->bufEnd )
			{
			sSetError( stream, CRYPT_ERROR_OVERFLOW );
			return( CRYPT_ERROR_OVERFLOW );
			}
		memcpy( stream->buffer + stream->bufPos, buffer, count );
		}
	stream->bufPos += count;
	return( CRYPT_OK );
	}

int sgetc( STREAM *stream )
	{
	int ch;

	if( stream->bufPos + 1 > stream->bufEnd )
		{
		sSetError( stream, CRYPT_ERROR_UNDERFLOW );
		return( CRYPT_ERROR_UNDERFLOW );
		}
	ch = stream->buffer[ stream->bufPos ];
	stream->bufPos++;
	return( ch );
	}

int sputc( STREAM *stream, const int data )
	{
	if( stream->buffer != NULL )
		{
		if( stream->bufPos + 1 > stream->bufEnd )
			{
			sSetError( stream, CRYPT_ERROR_OVERFLOW );
			return( CRYPT_ERROR_OVERFLOW );
			}
		stream->buffer[ stream->bufPos++ ] = data;
		}
	else
		stream->bufPos++;
	return( CRYPT_OK );
	}

int sseek( STREAM *stream, const long position )
	{
	return( 0 );
	}

int sPeek( STREAM *stream )
	{
	return( 0 );
	}

int sSkip( STREAM *stream, const long offset )
	{
	return( 0 );
	}

int sMemDataLeft( const STREAM *stream )
	{
	return( stream->bufSize - stream->bufPos );
	}

/* Dummy routines needed in misc_rw.c */

int BN_num_bits( const BIGNUM *a ) { return 0; }
int BN_high_bit( BIGNUM *a ) { return 0; }
BIGNUM *BN_bin2bn( const unsigned char *s, int len, BIGNUM *ret ) { return NULL; }
int	BN_bn2bin( const BIGNUM *a, unsigned char *to ) { return 0; }
int importBignum( BIGNUM *bn, const void *buffer, const int length,
				  const int minLength, const int maxLength, 
				  const BIGNUM *maxRange, const BOOLEAN checkKeysize ) { return -1; }
int exportBignum( void *data, const int dataMaxLength, int *dataLength,
				  const void *bignumPtr ) { return -1; }

/* SFTP command types */

#define SSH_FXP_INIT			1
#define SSH_FXP_VERSION			2
#define SSH_FXP_OPEN			3
#define SSH_FXP_CLOSE			4
#define SSH_FXP_READ			5
#define SSH_FXP_WRITE			6
#define SSH_FXP_LSTAT			7
#define SSH_FXP_FSTAT			8
#define SSH_FXP_SETSTAT			9
#define SSH_FXP_FSETSTAT		10
#define SSH_FXP_OPENDIR			11
#define SSH_FXP_READDIR			12
#define SSH_FXP_REMOVE			13
#define SSH_FXP_MKDIR			14
#define SSH_FXP_RMDIR			15
#define SSH_FXP_REALPATH		16
#define SSH_FXP_STAT			17
#define SSH_FXP_RENAME			18
#define SSH_FXP_READLINK		19
#define SSH_FXP_SYMLINK			20
#define SSH_FXP_STATUS			101
#define SSH_FXP_HANDLE			102
#define SSH_FXP_DATA			103
#define SSH_FXP_NAME			104
#define SSH_FXP_ATTRS			105

/* SFTP attribute presence flags.  When these flags are set, the
   corresponding file attribute value is present */

#define SSH_FILEXFER_ATTR_SIZE			0x01
#define SSH_FILEXFER_ATTR_UIDGID		0x02
#define SSH_FILEXFER_ATTR_PERMISSIONSv3	0x04
#define SSH_FILEXFER_ATTR_ACMODTIME		0x08
#define SSH_FILEXFER_ATTR_ACCESSTIME	0x08
#define SSH_FILEXFER_ATTR_CREATETIME	0x10
#define SSH_FILEXFER_ATTR_MODIFYTIME	0x20
#define SSH_FILEXFER_ATTR_PERMISSIONSv4	0x40
#define SSH_FILEXFER_ATTR_ACL			0x40
#define SSH_FILEXFER_ATTR_OWNERGROUP	0x80
#define SSH_FILEXFER_ATTR_SUBSECOND_TIMES 0x100
#define SSH_FILEXFER_ATTR_EXTENDED		0x80000000

/* SFTP file open/create flags */

#define SSH_FXF_READ			0x01
#define SSH_FXF_WRITE			0x02
#define SSH_FXF_APPEND			0x04
#define SSH_FXF_CREAT			0x08
#define SSH_FXF_TRUNC			0x10
#define SSH_FXF_EXCL			0x20
#define SSH_FXF_TEXT			0x40

/* SFTP file types */

#define SSH_FILETYPE_REGULAR	1
#define SSH_FILETYPE_DIRECTORY	2
#define SSH_FILETYPE_SYMLINK	3
#define SSH_FILETYPE_SPECIAL	4
#define SSH_FILETYPE_UNKNOWN	5

/* SFTP status codes */

#define SSH_FX_OK				0
#define SSH_FX_EOF				1
#define SSH_FX_NO_SUCH_FILE		2
#define SSH_FX_PERMISSION_DENIED 3
#define SSH_FX_FAILURE			4
#define SSH_FX_BAD_MESSAGE		5
#define SSH_FX_NO_CONNECTION	6
#define SSH_FX_CONNECTION_LOST	7
#define SSH_FX_OP_UNSUPPORTED	8
#define SSH_FX_INVALID_HANDLE	9
#define SSH_FX_NO_SUCH_PATH		10
#define SSH_FX_FILE_ALREADY_EXISTS 11
#define SSH_FX_WRITE_PROTECT	12
#define SSH_FX_NO_MEDIA			13

/* A structure to contain SFTP file attributes */

typedef struct {
	BOOLEAN isDirectory;		/* Whether directory or normal file */
	long size;					/* File size */
	int permissions;			/* File permissions */
	time_t ctime, atime, mtime;	/* File create, access, mod times */
	} SFTP_ATTRS;

/* A structure to contain SFTP session information */

#define MAX_HANDLE_SIZE		16

typedef struct {
	int version;				/* SFTP protocol version */
	long id;					/* Session ID */
	BYTE handle[ MAX_HANDLE_SIZE ];	/* File handle */
	int handleSize;
	} SFTP_INFO;

/* Read/write SFTP attributes.  This changed completely from v3 to v4, so we
   have to treat them as special-cases:

	uint32		flags
	byte		file_type
	uint64		size (present if ATTR_SIZE)
	string		owner (present if ATTR_OWNERGROUP)
	string		group (present if ATTR_OWNERGROUP)
	uint32		permissions (present if ATTR_PERMISSIONS)
	uint64		atime (present if ATTR_ACCESSTIME)
	uint32		atime_nseconds (present if ATTR_SUBSECOND_TIMES)
	uint64		createtime (present if ATTR_CREATETIME)
	uint32		createtime_nseconds (present if ATTR_SUBSECOND_TIMES)
	uint64		mtime (present if ATTR_MODIFYTIME)
	uint32		mtime_nseconds (present if ATTR_SUBSECOND_TIMES)
	string		acl (present if ATTR_ACL)
	uint32		extended_count (present if ATTR_EXTENDED)
		string	extended_type
		string	extended_value
   		[ extended_count type/value pairs ] */

static int sizeofAttributes( SFTP_ATTRS *attributes, const int version )
	{
	int size = UINT32_SIZE;	/* Flags */

	if( version < 4 )
		{
		if( attributes->size != CRYPT_UNUSED )
			size += UINT64_SIZE;
		if( attributes->permissions != CRYPT_UNUSED )
			size += UINT32_SIZE;
		if( attributes->atime )
			size += UINT32_SIZE;
		if( attributes->mtime )
			size += UINT32_SIZE;
		}
	else
		{
		size++;
		if( attributes->size != CRYPT_UNUSED )
			size += UINT64_SIZE;
		if( attributes->permissions != CRYPT_UNUSED )
			size += UINT32_SIZE;
		if( attributes->ctime )
			size += UINT64_SIZE;
		if( attributes->atime )
			size += UINT64_SIZE;
		if( attributes->mtime )
			size += UINT64_SIZE;
		}

	return( size );
	}

static int readAttributes( STREAM *stream, SFTP_ATTRS *attributes, const int version )
	{
	long flags;

	memset( attributes, 0, sizeof( SFTP_ATTRS ) );
	attributes->permissions = CRYPT_UNUSED;
	attributes->size = CRYPT_UNUSED;

	/* Read basic attribute information: File size, and owner, and
	   permissions */
	flags = readUint32( stream );
	if( cryptStatusError( flags ) )
		return( flags );
	if( version < 4 )
		{
		if( flags & SSH_FILEXFER_ATTR_SIZE )
			readUint64( stream, &attributes->size );
		if( flags & SSH_FILEXFER_ATTR_UIDGID )
			{
			readUint32( stream );
			readUint32( stream );
			}
		if( flags & SSH_FILEXFER_ATTR_PERMISSIONSv3 )
			attributes->permissions = readUint32( stream );

		/* Read file access and modify times */
		if( flags & SSH_FILEXFER_ATTR_ACMODTIME )
			{
			readUint32Time( stream, &attributes->atime );
			readUint32Time( stream, &attributes->mtime );
			}
		}
	else
		{
		if( flags & SSH_FILEXFER_ATTR_SIZE )
			readUint64( stream, &attributes->size );
		if( flags & SSH_FILEXFER_ATTR_OWNERGROUP )
			{
			readString32( stream, NULL, 0, NULL );
			readString32( stream, NULL, 0, NULL );
			}
		if( flags & SSH_FILEXFER_ATTR_PERMISSIONSv4 )
			attributes->permissions = readUint32( stream );

		/* Read file create, access, and modify times */
		if( flags & SSH_FILEXFER_ATTR_ACCESSTIME )
			{
			readUint64Time( stream, &attributes->atime );
			if( flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES )
				readUint32( stream );
			}
		if( flags & SSH_FILEXFER_ATTR_CREATETIME )
			{
			readUint64Time( stream, &attributes->ctime );
			if( flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES )
				readUint32( stream );
			}
		if( flags & SSH_FILEXFER_ATTR_MODIFYTIME )
			{
			readUint64Time( stream, &attributes->mtime );
			if( flags & SSH_FILEXFER_ATTR_SUBSECOND_TIMES )
				readUint32( stream );
			}
		}

	/* Read ACLs and extended attribute type/value pairs, the one thing that
	   stayed the same from v3 to v4 */
	if( flags & SSH_FILEXFER_ATTR_ACL )
		readString32( stream, NULL, 0, NULL );
	if( flags & SSH_FILEXFER_ATTR_EXTENDED )
		{
		int extAttrCount = readUint32( stream );

		if( cryptStatusError( extAttrCount ) )
			return( extAttrCount );
		while( extAttrCount > 0 )
			{
			readString32( stream, NULL, 0, NULL );
			readString32( stream, NULL, 0, NULL );
			extAttrCount--;
			}
		}

	return( sGetStatus( stream ) );
	}

static int writeAttributes( STREAM *stream, SFTP_ATTRS *attributes, const int version )
	{
	int flags = 0;

	if( version < 4 )
		{
		/* Indicate which attribute values we're going to write */
		if( attributes->size != CRYPT_UNUSED )
			flags |= SSH_FILEXFER_ATTR_SIZE;
		if( attributes->permissions != CRYPT_UNUSED )
			flags |= SSH_FILEXFER_ATTR_PERMISSIONSv3;
		if( attributes->atime )
			flags |= SSH_FILEXFER_ATTR_ACMODTIME;
		writeUint32( stream, flags );

		/* Write the optional attributes */
		if( attributes->size != CRYPT_UNUSED )
			writeUint64( stream, attributes->size );
		if( attributes->permissions != CRYPT_UNUSED )
			writeUint32( stream, attributes->permissions );
		if( attributes->atime )
			{
			writeUint32Time( stream, attributes->atime );
			writeUint32Time( stream, attributes->mtime );
			}
		}
	else
		{
		/* Indicate which attribute values we're going to write */
		if( attributes->size != CRYPT_UNUSED )
			flags |= SSH_FILEXFER_ATTR_SIZE;
		if( attributes->permissions != CRYPT_UNUSED )
			flags |= SSH_FILEXFER_ATTR_PERMISSIONSv4;
		if( attributes->ctime )
			flags |= SSH_FILEXFER_ATTR_CREATETIME;
		if( attributes->atime )
			flags |= SSH_FILEXFER_ATTR_ACCESSTIME;
		if( attributes->mtime )
			flags |= SSH_FILEXFER_ATTR_MODIFYTIME;
		writeUint32( stream, flags );
		sputc( stream, attributes->isDirectory ? \
					   SSH_FILETYPE_DIRECTORY : SSH_FILETYPE_REGULAR );

		/* Write the optional attributes */
		if( attributes->size != CRYPT_UNUSED )
			writeUint64( stream, attributes->size );
		if( attributes->permissions != CRYPT_UNUSED )
			writeUint32( stream, attributes->permissions );
		if( attributes->ctime )
			writeUint64Time( stream, attributes->ctime );
		if( attributes->atime )
			writeUint64Time( stream, attributes->atime );
		if( attributes->mtime )
			writeUint64Time( stream, attributes->mtime );
		}

	return( sGetStatus( stream ) );
	}

/* Read/write SFTP status:

	uint32		id
	uint32		error/status code
	string		error message (ISO-10646 UTF-8 [RFC-2279])
	string		language tag (as defined in [RFC-1766]) */

static int sizeofStatus( const char *sshStatusString )
	{
	return( UINT32_SIZE + UINT32_SIZE + \
			( UINT32_SIZE + strlen( sshStatusString ) ) + \
			UINT32_SIZE );
	}

static int readStatus( STREAM *stream, SFTP_INFO *info )
	{
	static const struct {
		const int sftpStatus, cryptlibStatus;
		} sftpStatusMap[] = {
		{ SSH_FX_OK, CRYPT_OK },
		{ SSH_FX_EOF, CRYPT_ERROR_COMPLETE },
		{ SSH_FX_NO_SUCH_FILE, CRYPT_ERROR_NOTFOUND },
		{ SSH_FX_PERMISSION_DENIED, CRYPT_ERROR_PERMISSION },
		{ SSH_FX_FAILURE, CRYPT_ERROR_FAILED },
		{ SSH_FX_BAD_MESSAGE, CRYPT_ERROR_BADDATA },
		{ SSH_FX_NO_CONNECTION, CRYPT_ERROR_FAILED },
		{ SSH_FX_CONNECTION_LOST, CRYPT_ERROR_FAILED },
		{ SSH_FX_OP_UNSUPPORTED, CRYPT_ERROR_NOTAVAIL },
		{ SSH_FX_INVALID_HANDLE, CRYPT_ERROR_BADDATA },
		{ SSH_FX_NO_SUCH_PATH, CRYPT_ERROR_NOTFOUND },
		{ SSH_FX_FILE_ALREADY_EXISTS, CRYPT_ERROR_DUPLICATE },
		{ SSH_FX_WRITE_PROTECT, CRYPT_ERROR_PERMISSION },
		{ SSH_FX_NO_MEDIA, CRYPT_ERROR_FAILED },
		{ CRYPT_ERROR, CRYPT_ERROR_FAILED }
		};
	int value, i, status;

	/* Read the status info and make sure that it's valid */
	value = readUint32( stream );
	status = readUint32( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( value != info->id )
		return( CRYPT_ERROR_BADDATA );

	/* Translate the SFTP status into a cryptlib status */
	for( i = 0; sftpStatusMap[ i ].sftpStatus != CRYPT_ERROR && \
				sftpStatusMap[ i ].sftpStatus != status; i++ );
	status = sftpStatusMap[ i ].cryptlibStatus;

	return( status );
	}

static int writeStatus( STREAM *stream, SFTP_INFO *info, const int sshStatus,
						const char *sshStatusString )
	{
	writeUint32( stream, info->id );
	writeUint32( stream, sshStatus );
	writeString32( stream, sshStatusString, strlen( sshStatusString ) );
	return( writeString32( stream, "", 0 ) );
	}

static int readSftpPacket( const CRYPT_SESSION cryptSession, void *buffer,
						   const int bufSize )
	{
	int bytesCopied, status;

	status = cryptPopData( cryptSession, buffer, BUFFER_SIZE, &bytesCopied );
	if( cryptStatusError( status ) )
		{
		printf( "SVR: Couldn't read data from SFTP client, status %d, line "
				"%d.\n", status, __LINE__ );
		return( status );
		}
	return( bytesCopied > 0 ? bytesCopied : CRYPT_ERROR_UNDERFLOW );
	}

static int writeSftpPacket( const CRYPT_SESSION cryptSession, const void *data,
							const int length )
	{
	int bytesCopied, status;

	status = cryptPushData( cryptSession, data, length, &bytesCopied );
	if( cryptStatusOK( status ) )
		status = cryptFlushData( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "SVR: Couldn't write data to SFTP client, status %d, line "
				"%d.\n", status, __LINE__ );
		return( status );
		}
	if( bytesCopied < length )
		{
		printf( "SVR: Only wrote %d of %d bytes of SFTP data, line %d.\n",
				bytesCopied, length, __LINE__ );
		return( status );
		}
	return( CRYPT_OK );
	}

static int sendAck( const CRYPT_SESSION cryptSession, SFTP_INFO *sftpInfo )
	{
	STREAM stream;
	BYTE buffer[ 128 ];
	int length;

	/* Ack an SFTP packet */
	sMemOpen( &stream, buffer, 128 );
	writeUint32( &stream, 1 + sizeofStatus( "" ) );
	sputc( &stream, SSH_FXP_STATUS );
	writeStatus( &stream, sftpInfo, SSH_FX_OK, "" );
	length = stell( &stream );
	sMemDisconnect( &stream );
	return( writeSftpPacket( cryptSession, buffer, length ) );
	}

int sftpServer( const CRYPT_SESSION cryptSession )
	{
	STREAM stream;
	SFTP_ATTRS sftpAttrs;
	SFTP_INFO sftpInfo;
	BYTE buffer[ BUFFER_SIZE ], nameBuffer[ 128 ];
	time_t xferTime;
	long xferCount = 0, dataLength;
	int length, value, status;

	cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_READTIMEOUT, 30 );

	memset( &sftpInfo, 0, sizeof( SFTP_INFO ) );

	/* Read the client's FXP_INIT and send our response */
	status = readSftpPacket( cryptSession, buffer, BUFFER_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, buffer, status );
	length = readUint32( &stream );
	value = sgetc( &stream );
	if( ( length != 1 + 4 ) || ( value != SSH_FXP_INIT ) )
		return( CRYPT_ERROR_BADDATA );
	sftpInfo.version = readUint32( &stream );
	sMemDisconnect( &stream );
	printf( "SVR: Client supports SFTP version %d.\n", sftpInfo.version );
	sMemOpen( &stream, buffer, BUFFER_SIZE );
	writeUint32( &stream, 1 + 4 );
	sputc( &stream, SSH_FXP_VERSION );
	writeUint32( &stream, 3 );
	length = stell( &stream );
	sMemDisconnect( &stream );
	status = writeSftpPacket( cryptSession, buffer, length );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the client's FXP_OPEN and send our response */
	status = readSftpPacket( cryptSession, buffer, BUFFER_SIZE );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "SVR: Attempt to read data from "
					   "client", status, __LINE__ );
		return( status );
		}
	sMemConnect( &stream, buffer, status );
	length = readUint32( &stream );
	value = sgetc( &stream );
	if( value == SSH_FXP_STAT )
		{
		/* See what the client is after */
		sftpInfo.id = readUint32( &stream );
		length = readUint32( &stream );
		sread( &stream, nameBuffer, length );
		sMemDisconnect( &stream );
		nameBuffer[ length ] = '\0';
		printf( "SVR: Client tried to stat file '%s'.\n", nameBuffer );
		if( strcmp( nameBuffer, "." ) )
			{
			puts( "SVR: Don't know how to respond to stat request for this "
				  "file." );
			return( CRYPT_ERROR_NOTAVAIL );
			}

		/* Send back a dummy response */
		memset( &sftpAttrs, 0, sizeof( SFTP_ATTRS ) );
		sftpAttrs.isDirectory = TRUE;
		sftpAttrs.permissions = 0777;
		sftpAttrs.size = CRYPT_UNUSED;
		sftpAttrs.atime = sftpAttrs.ctime = sftpAttrs.mtime = time( NULL );
		length = sizeofAttributes( &sftpAttrs, sftpInfo.version );
		sMemOpen( &stream, buffer, BUFFER_SIZE );
		writeUint32( &stream, 1 + UINT32_SIZE + length );
		sputc( &stream, SSH_FXP_ATTRS );
		writeUint32( &stream, sftpInfo.id );
		writeAttributes( &stream, &sftpAttrs, sftpInfo.version );
		length = stell( &stream );
		sMemDisconnect( &stream );
		status = writeSftpPacket( cryptSession, buffer, length );
		if( cryptStatusError( status ) )
			return( status );

		/* See what they want next */
		status = readSftpPacket( cryptSession, buffer, BUFFER_SIZE );
		if( cryptStatusError( status ) )
			{
			printExtError( cryptSession, "SVR: Attempt to read data from "
						   "client", status, __LINE__ );
			return( status );
			}
		sMemConnect( &stream, buffer, status );
		length = readUint32( &stream );
		value = sgetc( &stream );
		}
	if( value == SSH_FXP_OPEN )
		{
		/* See what the client is after */
		sftpInfo.id = readUint32( &stream );
		length = readUint32( &stream );
		sread( &stream, nameBuffer, length );
		value = readUint32( &stream );
		readAttributes( &stream, &sftpAttrs, sftpInfo.version );
		sMemDisconnect( &stream );
		nameBuffer[ length ] = '\0';
		printf( "Client tried to open file '%s', mode %02X, length %d.\n",
				nameBuffer, value, sftpAttrs.size );

		/* Putty for some reason tries to open the current directory for
		   create (rather than the filename), and bails out when it gets a
		   permission-denied.  So I guess we tell it to go ahead... */
		sMemOpen( &stream, buffer, BUFFER_SIZE );
		writeUint32( &stream, 1 + UINT32_SIZE + ( UINT32_SIZE + 1 ) );
		sputc( &stream, SSH_FXP_HANDLE );
		writeUint32( &stream, sftpInfo.id );
		writeUint32( &stream, 1 );
		sputc( &stream, 1 );
		length = stell( &stream );
		sMemDisconnect( &stream );
		status = writeSftpPacket( cryptSession, buffer, length );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Now we're in the write loop... */
	xferTime = time( NULL );
	dataLength = 0;
	while( TRUE )
		{
		/* See what they want next */
		status = readSftpPacket( cryptSession, buffer, BUFFER_SIZE );
		if( cryptStatusError( status ) )
			{
			printExtError( cryptSession, "SVR: Attempt to read data from "
						   "client", status, __LINE__ );
			return( status );
			}
		if( status < 1 )
			{
			printf( "SVR: Read 0 bytes from client.\n" );
			return( CRYPT_ERROR_UNDERFLOW );
			}
		if( dataLength > 0 )
			{
			xferCount += status;
			dataLength -= status;
			printf( "SRV: -------- : %d.\r", xferCount );
			if( dataLength <= 0 )
				break;
			continue;
			}
		sMemConnect( &stream, buffer, status );
		length = readUint32( &stream );
		if( status < BUFFER_SIZE && ( length != status - UINT32_SIZE ) )
			{
			printf( "Didn't read complete packet, length = %d, byte count = "
					"%d.\n", length, status - UINT32_SIZE );
			}
		value = sgetc( &stream );
		if( value != SSH_FXP_WRITE )
			break;
		sftpInfo.id = readUint32( &stream );
		readString32( &stream, nameBuffer, 128, &length );
		readUint64( &stream, &value );
		dataLength = readUint32( &stream );
		printf( "SRV: %8d : %d.\r", value, length );
		xferCount += status - stell( &stream );
		dataLength -= status - stell( &stream );
		sMemDisconnect( &stream );

		/* Ack the write */
		if( dataLength <= 0 )
			{
			status = sendAck( cryptSession, &sftpInfo );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	xferTime = time( NULL ) - xferTime;
	printf( "Transfer time = %d seconds, %ld bytes, %d bytes/sec.\n",
			xferTime, xferCount, xferCount / xferTime );

	/* Clean up */
	if( value != SSH_FXP_CLOSE )
		{
		printf( "SVR: Client sent unexpected packet %d.\n", value );
		return( CRYPT_ERROR_BADDATA );
		}
	sftpInfo.id = readUint32( &stream );
	status = sendAck( cryptSession, &sftpInfo );
	if( cryptStatusError( status ) )
		return( status );
	status = readSftpPacket( cryptSession, buffer, BUFFER_SIZE );
	if( status == CRYPT_ERROR_COMPLETE )
		{
		puts( "SVR: Client has closed the channel." );
		return( CRYPT_OK );
		}
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, buffer, status );
	length = readUint32( &stream );
	value = sgetc( &stream );

	return( CRYPT_OK );
	}

#define SFTP_DATA_AMOUNT	( 1024 * 1024 )

int sftpClient( const CRYPT_SESSION cryptSession )
	{
	STREAM stream;
	SFTP_ATTRS sftpAttrs;
	SFTP_INFO sftpInfo;
	BYTE buffer[ BUFFER_SIZE ];
	long totalLength = SFTP_DATA_AMOUNT;
	int length, value, status;

	cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_READTIMEOUT, 30 );

	memset( &sftpInfo, 0, sizeof( SFTP_INFO ) );

	/* Send our FXP_INIT and read back the response */
	sMemOpen( &stream, buffer, BUFFER_SIZE );
	writeUint32( &stream, 1 + 4 );
	sputc( &stream, SSH_FXP_INIT );
	writeUint32( &stream, 3 );
	length = stell( &stream );
	sMemDisconnect( &stream );
	status = writeSftpPacket( cryptSession, buffer, length );
	if( cryptStatusError( status ) )
		return( status );
	status = readSftpPacket( cryptSession, buffer, BUFFER_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, buffer, status );
	length = readUint32( &stream );
	value = sgetc( &stream );
	if( ( length != 1 + 4 ) || ( value != SSH_FXP_VERSION ) )
		return( CRYPT_ERROR_BADDATA );
	sftpInfo.version = readUint32( &stream );
	sMemDisconnect( &stream );
	printf( "Server supports SFTP version %d.\n", sftpInfo.version );

	/* Open the file to transfer */
	memset( &sftpAttrs, 0, sizeof( SFTP_ATTRS ) );
	sftpAttrs.permissions = 0777;
	sftpAttrs.size = CRYPT_UNUSED;
	sftpAttrs.atime = sftpAttrs.ctime = sftpAttrs.mtime = time( NULL );
	length = sizeofAttributes( &sftpAttrs, sftpInfo.version );
	sMemOpen( &stream, buffer, BUFFER_SIZE );
	writeUint32( &stream, 1 + UINT32_SIZE + ( UINT32_SIZE + 8 ) + UINT32_SIZE + length );
	sputc( &stream, SSH_FXP_OPEN );
	writeUint32( &stream, 1 );
	writeString32( &stream, "test.dat", 8 );
	writeUint32( &stream, SSH_FXF_CREAT | SSH_FXF_WRITE	);
	writeAttributes( &stream, &sftpAttrs, sftpInfo.version );
	length = stell( &stream );
	sMemDisconnect( &stream );
	status = writeSftpPacket( cryptSession, buffer, length );
	if( cryptStatusError( status ) )
		return( status );
	status = readSftpPacket( cryptSession, buffer, BUFFER_SIZE );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "Attempt to read data from server",
					   status, __LINE__ );
		return( status );
		}
	sMemConnect( &stream, buffer, status );
	length = readUint32( &stream );
	value = sgetc( &stream );
	readUint32( &stream );
	readString32( &stream, sftpInfo.handle, MAX_HANDLE_SIZE, 
				  &sftpInfo.handleSize );
	sMemDisconnect( &stream );
	if( value != SSH_FXP_HANDLE )
		{
		printf( "Server sent packet %d, expected file handle.\n", value );
		return( CRYPT_ERROR_BADDATA );
		}

	/* Send the file (just 1MB of test data) */
	sMemOpen( &stream, buffer, BUFFER_SIZE );
	writeUint32( &stream, 1 + UINT32_SIZE + \
						  ( UINT32_SIZE + sftpInfo.handleSize ) + \
						  UINT64_SIZE + ( UINT32_SIZE + SFTP_DATA_AMOUNT ) );
	sputc( &stream, SSH_FXP_WRITE );
	writeUint32( &stream, sftpInfo.id );
	writeString32( &stream, sftpInfo.handle, sftpInfo.handleSize );
	writeUint64( &stream, 0 );
	writeUint32( &stream, SFTP_DATA_AMOUNT );
	length = stell( &stream );
	memset( buffer + length, '*', BUFFER_SIZE - length );
	status = writeSftpPacket( cryptSession, buffer, BUFFER_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	totalLength -= BUFFER_SIZE - length;
	while( totalLength > 0 )
		{
		memset( buffer, '*', BUFFER_SIZE );
		status = writeSftpPacket( cryptSession, buffer,
								  min( totalLength, BUFFER_SIZE ) );
		if( cryptStatusError( status ) )
			return( status );
		totalLength -= min( totalLength, BUFFER_SIZE );
		}

	/* Wait for the ack */
	status = readSftpPacket( cryptSession, buffer, BUFFER_SIZE );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "Attempt to read data from server",
					   status, __LINE__ );
		return( status );
		}

	return( CRYPT_OK );
	}
#endif /* WINDOWS_THREADS && 0 */

#endif /* TEST_SESSION || TEST_SESSION_LOOPBACK */
