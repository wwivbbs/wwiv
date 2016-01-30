/****************************************************************************
*																			*
*						cryptlib Suite B Test Routines						*
*						Copyright Peter Gutmann 2009-2011					*
*																			*
****************************************************************************/

#include <ctype.h>		/* For arg-parsing */
#include "cryptlib.h"
#include "test/test.h"

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */

#ifdef CONFIG_SUITEB_TESTS

/* Test-specific flags to control details of each test */

#define TESTFLAG_SENDHTTPREQ		0x01	/* Send HTTP req.over TLS link */
#define TESTFLAG_GENERIC			0x02	/* Don't print "A.x.y.z" for test */

/* Special kludge function in cryptapi.c to enable nonstandard behaviour for
   Suite B tests.  The definitions below are copined from session/ssl.h */

typedef enum {
	SUITEB_TEST_NONE,			/* No special test behaviour */
	SUITEB_TEST_CLIINVALIDCURVE,/* Client sends non-Suite B curve */
	SUITEB_TEST_SVRINVALIDCURVE,/* Server sends non-Suite B curve */
	SUITEB_TEST_BOTHCURVES,		/* Client must send P256 and P384 as supp.curves */
	SUITEB_TEST_BOTHSIGALGOS,	/* Client must send SHA256 and SHA384 as sig.algos */
	SUITEB_TEST_LAST
	} SUITEB_TEST_VALUE;

C_RET cryptSuiteBTestConfig( C_IN int magicValue );

/****************************************************************************
*																			*
*							Suite B Test Definitions						*
*																			*
****************************************************************************/

/* Defines to make the table entries smaller and/or easier to understand */

#define SECLVL_128		CRYPT_SSLOPTION_SUITEB_128
#define SECLVL_256		CRYPT_SSLOPTION_SUITEB_256
#define SECLVL_BOTH		SECLVL_128	/* For 5430bis, "128" now imples "128+256" */
#define FORCE_TLS12		CRYPT_SSLOPTION_MINVER_TLS12

#define P521			521		/* For invalid-curve tests */
#define P384			384
#define P256			256

/* Some entries are aliases for other entries, to make this easier to 
   specify we use the following define to fill in the unused fields */

#define ALIAS			0, 0, 0, 0, 0, 0
#define ALIAS_SPECIAL	0, 0, 0, 0, 0

/* Special-case handling situations */

typedef enum { 
	SPECIAL_NONE,

	/* Client tests */
	/* Client must reject invalid curve (in the form of server key) from server */
	SPECIAL_SVR_INVALIDCURVE,
	/* Client must send sig_algo extensions for both P256 and P384 curves */
	SPECIAL_BOTH_SIGALGO,
	/* Client must send supported_curves extension for both P256 and P384 
	   curves */
	SPECIAL_BOTH_SUPPCURVES,
	/* Client/server must send TLS alert due to protocol violation */
	SPECIAL_CLI_TLSALERT,
	SPECIAL_SVR_TLSALERT,
	
	/* Server tests */
	/* Server must reject invalid supported-elliptic-curve ext.from client */
	SPECIAL_CLI_INVALIDCURVE,
	/* Server must send supported_sig_algo ext for both P256 and P384 */
	SPECIAL_BOTH_SUPPALGO,
	SPECIAL_LAST
	} SPECIAL_HANDLING_TYPE;

/* A table that defines the conditions for the various tests.  Note that 
   some of the test requirements are handled internally by cryptlib so 
   they're not explicitly tested here, as a result some of the entries may 
   appear to be duplicates */

typedef struct {
	const char *testName;

	const int clientOptions;
	const int clientAuthKeySizeBits;	/* 0 = no client auth.used */

	const int serverOptions;
	const int serverKeySizeBits;

	const BOOLEAN result;
	const SPECIAL_HANDLING_TYPE handlingType;

	const char *aliasTestName;
	} SUITEB_TEST_INFO;

static const SUITEB_TEST_INFO clientTestInfo[] = {
		{ NULL,			0,				0,		0,				0,		0 },
		/* ------------------------------- Client Tests ------------------------------- */
		/* Test			Client parm	Client key	Server parm	Server key	Result	Special */
		/* ----			-----------	----------	-----------	----------	------	------- */
/* 1*/	{ "A.1.1.1",	SECLVL_128,		0,		SECLVL_128,		P256,	TRUE },
/* 2*/	{ "A.1.1.2",	SECLVL_128,		0,		SECLVL_128,		P521,	FALSE,	SPECIAL_SVR_INVALIDCURVE },
/* 3*/	{ "A.1.2.1",	SECLVL_256,		0,		SECLVL_256,		P384,	TRUE },
/* 4*/	{ "A.1.2.2",	SECLVL_256,		0,		SECLVL_256,		P521,	FALSE,	SPECIAL_SVR_INVALIDCURVE },
/* 5*/	{ "A.1.3.1", ALIAS, "A.1.1.1" },
/* 6*/	{ "A.1.4.1", ALIAS, "A.1.1.1" },
/* 7*/	{ "A.1.5.1", ALIAS_SPECIAL, SPECIAL_CLI_TLSALERT, "A.1.1.2" },
/* 8*/	{ "A.1.5.2", ALIAS_SPECIAL, SPECIAL_CLI_TLSALERT, "A.1.2.2" },
/* 9*/	{ "A.1.6.1", ALIAS, "A.1.4.1" },
/*10*/	{ "A.1.7.1",	SECLVL_128,		P256,	SECLVL_BOTH,	P384,	TRUE },
/*11*/	{ "A.1.7.2",	SECLVL_BOTH,	P384,	SECLVL_128,		P256,	TRUE },
/*12*/	{ "A.1.8.1", ALIAS, "A.1.1.1" },
/*13*/	{ "A.1.9.1",	SECLVL_128,		0,		SECLVL_BOTH,	P384,	TRUE },
/*14*/	{ "A.1.10.1", ALIAS, "A.1.2.1" },
/*15*/	{ "A.1.11.1", ALIAS, "A.1.2.1" },
/*16*/	{ "A.1.12.1", ALIAS, "A.1.1.1" },
/*17*/	{ "A.1.13.1",	SECLVL_128,		0,		SECLVL_128 | FORCE_TLS12,	P256,	TRUE },
/*18*/	{ "A.1.14.1",	SECLVL_128,		0,		SECLVL_128,		P256,	TRUE },
/*19*/	{ "A.1.15.1",	SECLVL_128,		0,		SECLVL_128,		P256,	TRUE },
/*20*/	{ "A.1.16.1",	SECLVL_256,		0,		SECLVL_256,		P384,	TRUE },
/*21*/	{ "A.1.17.1", ALIAS, "A.1.14.1" },
/*22*/	{ "A.1.18.1", ALIAS, "A.1.14.1" },
/*23*/	{ "A.1.19.1", ALIAS, "A.1.1.1" },
/*24*/	{ "A.1.20.1", ALIAS, "A.1.1.1" },
/*25*/	{ "A.1.21.1", ALIAS, "A.1.2.1" },
/*26*/	{ "A.1.22.1", ALIAS, "A.1.1.1" },
/*27*/	{ "A.1.21.2", ALIAS, "A.1.2.1" },
/*28*/	{ "A.1.24.1",	SECLVL_128,		0,		SECLVL_128,		P256,	TRUE },
/*29*/	{ "A.1.24.2",	SECLVL_256,		0,		SECLVL_256,		P384,	TRUE },
/*30*/	{ "A.1.25.1", ALIAS, "A.1.24.1" },
/*31*/	{ "A.1.26.1",	SECLVL_128,		0,		SECLVL_128,		P384,	TRUE,	SPECIAL_BOTH_SUPPCURVES },
/*32*/	{ "A.1.27.1", ALIAS, "A.1.24.2" },
/*33*/	{ "A.1.28.1", ALIAS, "A.1.1.1" },
/*34*/	{ "A.1.29.1", ALIAS, "A.2.8.1" },		// Wrong, should be an A.1.x ref.
/*35*/	{ "A.1.30.1", ALIAS, "A.1.2.1" },
/*36*/	{ "A.1.31.1", ALIAS, "A.1.1.1" },
/*37*/	{ "A.1.32.1", ALIAS, "A.1.1.1" },
/*38*/	{ "A.1.33.1", ALIAS, "A.1.7.1" },
/*39*/	{ "A.1.34.1",	SECLVL_128,		0,		SECLVL_128,		P256,	TRUE },
/*40*/	{ "A.1.34.2",	SECLVL_256,		0,		SECLVL_256,		P384,	TRUE },
/*41*/	{ "A.1.35.1", ALIAS, "A.1.34.1" },
/*42*/	{ "A.1.36.1",	SECLVL_BOTH,		0,	SECLVL_BOTH,	P384,	TRUE,	SPECIAL_BOTH_SIGALGO },
/*43*/	{ "A.1.37.1", ALIAS, "A.1.34.2" },
/*44*/	{ "A.1.39.1",	SECLVL_128,		P256,	SECLVL_128,		P256,	TRUE },
/*45*/	{ "A.1.39.2",	SECLVL_256,		P384,	SECLVL_256,		P384,	TRUE },
/*46*/	{ "A.1.40.1", ALIAS, "A.1.39.1" },
/*47*/	{ "A.1.41.1", ALIAS, "A.1.39.2" },
		/* ----			-----------	----------	-----------	----------	------	------- */
		/* Test			Client parm	Client key	Server parm	Server key	Result	Special */
		/* ------------------------------- Client Tests ------------------------------- */
		{ NULL }, { NULL }
		};

static const SUITEB_TEST_INFO serverTestInfo[] = {
		{ NULL,			0,				0,		0,				0,		0 },
		/* ------------------------------- Server Tests ------------------------------- */
		/* Test			Client parm	Client key	Server parm	Server key	Result	Special */
		/* ----			-----------	----------	-----------	----------	------	------- */
/* 1*/	{ "A.2.1.1",	SECLVL_128,		0,		SECLVL_128,		P256,	TRUE },
/* 2*/	{ "A.2.1.2",	SECLVL_256,		0,		SECLVL_128,		P256,	FALSE,	SPECIAL_CLI_INVALIDCURVE },
/* 3*/	{ "A.2.2.1",	SECLVL_256,		0,		SECLVL_256,		P384,	TRUE },
/* 4*/	{ "A.2.2.2",	SECLVL_256,		0,		SECLVL_256,		P384,	FALSE,	SPECIAL_CLI_INVALIDCURVE },
/* 5*/	{ "A.2.5.1", ALIAS, "A.2.1.2" },
/* 6*/	{ "A.2.5.1", ALIAS, "A.2.2.2" },
/* 7*/	{ "A.2.3.1", ALIAS_SPECIAL, SPECIAL_SVR_TLSALERT, "A.2.1.2" },
/* 8*/	{ "A.2.4.1", ALIAS_SPECIAL, SPECIAL_SVR_TLSALERT, "A.2.2.2" },
/* 9*/	{ "A.2.6.1",	SECLVL_128,		P384,	SECLVL_BOTH,	P256,	TRUE },
/*10*/	{ "A.2.6.2",	SECLVL_128,		P256,	SECLVL_BOTH,	P384,	TRUE },
/*11*/	{ "A.2.7.1", ALIAS, "A.2.1.1" },
/*12*/	{ "A.2.8.1",	SECLVL_128,		P384,	SECLVL_BOTH,	P256,	TRUE },
/*13*/	{ "A.2.9.1", ALIAS, "A.2.2.1" },
/*14*/	{ "A.2.10.1", ALIAS, "A.2.2.1" },
/*15*/	{ "A.2.11.1", ALIAS, "A.2.6.1" },
/*16*/	{ "A.2.12.1",	SECLVL_128 | FORCE_TLS12, 0, SECLVL_128, P256,	TRUE },
/*17*/	{ "A.2.13.1",	SECLVL_BOTH,	0,		SECLVL_128,		P256,	TRUE },
/*18*/	{ "A.2.14.1",	SECLVL_256,		0,		SECLVL_256,		P384,	TRUE },
/*19*/	{ "A.2.15.1", ALIAS, "A.2.1.1" },
/*20*/	{ "A.2.16.1", ALIAS, "A.2.1.1" },
/*21*/	{ "A.2.17.1", ALIAS, "A.2.2.1" },
/*22*/	{ "A.2.18.1", ALIAS, "A.2.1.1" },
/*23*/	{ "A.2.18.2", ALIAS, "A.2.2.1" },
/*24*/	{ "A.2.20.1", ALIAS, "A.2.1.1" },
/*25*/	{ "A.2.21.1", ALIAS, "A.2.8.1" },
/*26*/	{ "A.2.22.1", ALIAS, "A.2.2.1" },
/*27*/	{ "A.2.23.1",	SECLVL_128,		0,		SECLVL_BOTH,	P256,	FALSE,	SPECIAL_CLI_INVALIDCURVE },
/*28*/	{ "A.2.24.1", ALIAS_SPECIAL, SPECIAL_SVR_TLSALERT, "A.2.23.1" },
/*29*/	{ "A.2.26.1",	SECLVL_128,		P256,	SECLVL_128,		P256,	TRUE },
/*30*/	{ "A.2.26.2",	SECLVL_256,		P384,	SECLVL_256,		P384,	TRUE },
/*31*/	{ "A.2.27.1", ALIAS, "A.2.26.1" },
/*32*/	{ "A.2.28.1",	SECLVL_BOTH,	P256,	SECLVL_BOTH,	P256,	TRUE,	SPECIAL_BOTH_SUPPALGO },
/*33*/	{ "A.2.29.1", ALIAS, "A.2.26.2" },
/*34*/	{ "A.2.30.1", ALIAS, "A.2.1.1" },
/*35*/	{ "A.2.30.2", ALIAS, "A.2.1.2" },
/*36*/	{ "A.2.31.1", ALIAS, "A.2.30.1" },
/*37*/	{ "A.2.32.1", ALIAS, "A.2.30.1" },
/*38*/	{ "A.2.33.1", ALIAS, "A.2.30.2" },
		/* ----			-----------	----------	-----------	----------	------	------- */
		/* Test			Client parm	Client key	Server parm	Server key	Result	Special */
		/* ------------------------------- Server Tests ------------------------------- */

	{ NULL }, { NULL }
	};

#define SUITEB_FIRST_CLIENT		1
#define SUITEB_LAST_CLIENT		48
#define SUITEB_FIRST_SERVER		1
#define SUITEB_LAST_SERVER		38

/* Display the test numbers and their corresponding test names */

static int showTest( const SUITEB_TEST_INFO *testInfoPtr )
	{
	int i;

	for( i = 0; i < 6; i++ )
		{
		if( testInfoPtr->testName == NULL )
			return( FALSE );
		printf( "    %8s", testInfoPtr->testName );
		testInfoPtr++;
		}
	putchar( '\n' );

	return( TRUE );
	}

static void showTestInfo( void )
	{
	int i;

	puts( "  Suite B client tests:" );
	for( i = 1; clientTestInfo[ i ].testName != NULL; i += 6 )
		{
		if( !showTest( &clientTestInfo[ i ] ) )
			{
			printf( "\n" );
			break;
			}
		}
	puts( "\n  Suite B server tests:" );
	for( i = 1; serverTestInfo[ i ].testName != NULL; i += 6 )
		{
		if( !showTest( &serverTestInfo[ i ] ) )
			{
			printf( "\n" );
			break;
			}
		}
	}

/* Look up the test number corresponding to a test name.  Returns +ve for a 
   client test, -ve for a server test, 0 for no-match */

static int lookupTestNo( const char *testName )
	{
	int i;

	for( i = 1; clientTestInfo[ i ].testName != NULL; i++ )
		{
		if( !strcmp( testName + 1, clientTestInfo[ i ].testName + 1 ) )
			return( i );
		}
	for( i = 1; serverTestInfo[ i ].testName != NULL; i++ )
		{
		if( !strcmp( testName + 1, serverTestInfo[ i ].testName + 1 ) )
			return( -i );
		}

	return( 0 );
	}

/****************************************************************************
*																			*
*							Suite B TLS Routines Test						*
*																			*
****************************************************************************/

/* Find an alias for an existing test */

static const SUITEB_TEST_INFO *findAliasTest( const SUITEB_TEST_INFO *testInfo,
											  const char *testName )
	{
	int i;

	for( i = 0; testInfo[ i ].testName != NULL; i++ )
		{
		if( !strcmp( testInfo[ i ].testName, testName ) )
			return( &testInfo[ i ] );
		}

	assert( 0 );
	return( NULL );
	}

/* Establish a Suite B server session */

static int suitebServer( const int testNo, const char *hostName, 
						 const int port, const int flags,
						 const BOOLEAN isServerTest )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_CONTEXT privateKey;
	const SUITEB_TEST_INFO *testInfoPtr = isServerTest ? \
					&serverTestInfo[ testNo ] : &clientTestInfo[ testNo ];
	const char *testName = testInfoPtr->testName;
	const BOOLEAN isLoopbackTest = \
			( !strcmp( hostName, "localhost" ) && port == 0 ) ? TRUE : FALSE;
	const BOOLEAN sendHTTP = \
			( flags & TESTFLAG_SENDHTTPREQ ) ? TRUE : FALSE;
	char filenameBuffer[ FILENAME_BUFFER_SIZE ];
#ifdef UNICODE_STRINGS
	wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];
#endif /* UNICODE_STRINGS */
	SPECIAL_HANDLING_TYPE handlingTypeAlt = SPECIAL_NONE;
	void *fileNamePtr = filenameBuffer;
	int status;

	/* Make sure that we've been given a valid test number to run */
	if( isServerTest )
		{
		if( testNo < SUITEB_FIRST_SERVER || testNo > SUITEB_LAST_SERVER )
			return( FALSE );
		}
	else
		{
		if( testNo < SUITEB_FIRST_CLIENT || testNo > SUITEB_LAST_CLIENT )
			return( FALSE );
		}

	/* If it's an alias for another test, select the base test */
	if( testInfoPtr->aliasTestName != NULL )
		{
		handlingTypeAlt = testInfoPtr->handlingType;
		testInfoPtr = findAliasTest( isServerTest ? \
								&serverTestInfo[ 1 ] : &clientTestInfo[ 1 ],
								testInfoPtr->aliasTestName );
		if( testInfoPtr == NULL )
			{
			assert( 0 );
			return( FALSE );
			}
		}

	/* Acquire the init mutex */
	acquireMutex();
	cryptSuiteBTestConfig( SUITEB_TEST_NONE );	/* Clear any custom config */

	printf( "SVR: Running Suite B server " );
	if( flags & TESTFLAG_GENERIC )
		printf( "as generic test server.\n" );
	else
		printf( "with test %s.\n", testInfoPtr->testName );

	/* Create the SSL/TLS session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED, 
								 CRYPT_SESSION_SSL_SERVER );
	if( status == CRYPT_ERROR_PARAM3 )	/* SSL/TLS session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_VERSION, 3 );
	if( cryptStatusOK( status ) && testInfoPtr->serverOptions != 0 )
		{
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_SSL_OPTIONS, 
									testInfoPtr->serverOptions );
		}
	if( testInfoPtr->clientAuthKeySizeBits > 0 ) 
		{
		/* Tell the test code to expect a client certificate */
		cryptSuiteBTestConfig( 1000 );
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Set up the server information */
	if( isLoopbackTest )
		{
		/* We're running the loopback test, set up a local connect */
		if( !setLocalConnect( cryptSession, 443 ) )
			return( FALSE );
		}
	else
		{
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SERVER_NAME,
										  hostName, strlen( hostName ) );
		if( cryptStatusOK( status ) && port != 0 && port != 443 )
			status = cryptSetAttribute( cryptSession, 
										CRYPT_SESSINFO_SERVER_PORT, port );
		if( cryptStatusError( status ) )
			{
			printf( "cryptSetAttribute()/cryptSetAttributeString() failed "
					"with error code %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}

	/* Set any custom server configuration that may be required.  We have to 
	   do this before we set the server key since some of the tests involve
	   invalid server keys */
	switch( testInfoPtr->handlingType )
		{
		case SPECIAL_SVR_INVALIDCURVE:
			/* Server sends non-Suite B curve */
			status = cryptSuiteBTestConfig( SUITEB_TEST_SVRINVALIDCURVE );
			break;

		case SPECIAL_BOTH_SUPPCURVES:
			/* Client must send both P256 and P384 in supported curves 
			   extension */
			status = cryptSuiteBTestConfig( SUITEB_TEST_BOTHCURVES );
			break;

		case SPECIAL_BOTH_SIGALGO:
			/* Client must send both SHA256 and SHA384 in signature algos
			   extension */
			status = cryptSuiteBTestConfig( SUITEB_TEST_BOTHSIGALGOS );
			break;
		}
	if( cryptStatusError( status ) )
		{
		printf( "Custom config set failed with error code %d, line %d.\n", 
				status, __LINE__ );
		return( FALSE );
		}

	/* Add the server key */
	filenameFromTemplate( filenameBuffer, SERVER_ECPRIVKEY_FILE_TEMPLATE, 
						  testInfoPtr->serverKeySizeBits );
#ifdef UNICODE_STRINGS
	mbstowcs( wcBuffer, filenameBuffer, strlen( filenameBuffer ) + 1 );
	fileNamePtr = wcBuffer;
#endif /* UNICODE_STRINGS */
	status = getPrivateKey( &privateKey, fileNamePtr, USER_PRIVKEY_LABEL,
							TEST_PRIVKEY_PASSWORD );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_PRIVATEKEY,
									privateKey );
		cryptDestroyContext( privateKey );
		}
	if( cryptStatusError( status ) )
		{
		printf( "SVR: cryptSetAttribute/AttributeString() failed with error "
				"code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* For the loopback test we also increase the connection timeout to a 
	   higher-than-normal level, since this gives us more time for tracing 
	   through the code when debugging */
	cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_CONNECTTIMEOUT, 120 );

	/* Tell the client that we're ready to go */
	releaseMutex();

	/* Activate the server session */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( ( testInfoPtr->result && !cryptStatusOK( status ) ) || \
		( !testInfoPtr->result && !cryptStatusError( status ) ) )
		{
		if( testInfoPtr->result )
			printf( "SVR: Test %s failed, should have succeeded.\n",
					testName );
		else
			printf( "SVR: Test %s succeeded, should have failed.\n",
					testName );
		if( cryptStatusError( status ) )
			{
			printExtError( cryptSession, "SVR: Failure reason is:", status, 
						   __LINE__ );
			}
		cryptDestroySession( cryptSession );

		return( FALSE );
		}

	/* Perform any custom post-activation checking that may be required */
	if( testInfoPtr->handlingType != 0 || handlingTypeAlt != 0 )
		{
		const SPECIAL_HANDLING_TYPE handlingType = \
				( handlingTypeAlt != 0 ) ? handlingTypeAlt : \
										   testInfoPtr->handlingType;
		BYTE buffer[ 1024 ];
		int length;

		switch( handlingType )
			{
			case SPECIAL_CLI_TLSALERT:
				status = cryptGetAttributeString( cryptSession, 
									CRYPT_ATTRIBUTE_ERRORMESSAGE, buffer, 
									&length );
				if( cryptStatusError( status ) || \
					memcmp( buffer, "Received TLS alert", 18 ) )
					{
					printf( "SVR: Test %s should have returned a TLS alert "
							"but didn't.\n", testName );
					return( FALSE );
					}
				break;
			}
		}

	/* If we're being asked to send HTTP data, return a basic HTML page */
	if( sendHTTP )
		{
		const char serverReply[] = \
			"HTTP/1.0 200 OK\n"
			"Date: Fri, 7 September 2010 20:02:07 GMT\n"
			"Server: cryptlib Suite B test\n"
			"Content-Type: text/html\n"
			"Connection: Close\n"
			"\n"
			"<!DOCTYPE HTML SYSTEM \"html.dtd\">\n"
			"<html>\n"
			"<head>\n"
			"<title>cryptlib Suite B test page</title>\n"
			"<body>\n"
			"Test message from the cryptlib Suite B server.<p>\n"
			"</body>\n"
			"</html>\n";
		char buffer[ FILEBUFFER_SIZE ];
		int bytesCopied;

		/* Print the text of the request from the client */
		status = cryptPopData( cryptSession, buffer, FILEBUFFER_SIZE,
							   &bytesCopied );
		if( cryptStatusError( status ) )
			{
			printExtError( cryptSession, "SVR: Attempt to read data from "
						   "client", status, __LINE__ );
			cryptDestroySession( cryptSession );
			return( FALSE );
			}
		buffer[ bytesCopied ] = '\0';
		printf( "---- Client sent %d bytes ----\n", bytesCopied );
		puts( buffer );
		puts( "---- End of output ----" );

		/* Send a reply */
		status = cryptPushData( cryptSession, serverReply,
								sizeof( serverReply ) - 1, &bytesCopied );
		if( cryptStatusOK( status ) )
			status = cryptFlushData( cryptSession );
		if( cryptStatusError( status ) || \
			bytesCopied != sizeof( serverReply ) - 1 )
			{
			printExtError( cryptSession, "Attempt to send data to client", 
						   status, __LINE__ );
			cryptDestroySession( cryptSession );
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
	printf( "SVR: Suite B server test %s succeeded.\n", testName );

	return( TRUE );
	}

/* Establish a Suite B client session */

static int suitebClient( const int testNo, const char *hostName, 
						 const int port, const int flags,
						 const BOOLEAN isServerTest )
	{
	CRYPT_SESSION cryptSession;
	const SUITEB_TEST_INFO *testInfoPtr = isServerTest ? \
					&serverTestInfo[ testNo ] : &clientTestInfo[ testNo ];
	const BOOLEAN isLoopbackTest = \
			( !strcmp( hostName, "localhost" ) && port == 0 ) ? TRUE : FALSE;
	const BOOLEAN sendHTTP = \
			( flags & TESTFLAG_SENDHTTPREQ ) ? TRUE : FALSE;
	const char *testName = testInfoPtr->testName;
	SPECIAL_HANDLING_TYPE handlingTypeAlt = SPECIAL_NONE;
	int status;

	/* Make sure that we've been given a valid test number to run */
	if( isServerTest )
		{
		if( testNo < SUITEB_FIRST_SERVER || testNo > SUITEB_LAST_SERVER )
			return( FALSE );
		}
	else
		{
		if( testNo < SUITEB_FIRST_CLIENT || testNo > SUITEB_LAST_CLIENT )
			return( FALSE );
		}

	/* If it's an alias for another test, select the base test */
	if( testInfoPtr->aliasTestName != NULL )
		{
		handlingTypeAlt = testInfoPtr->handlingType;
		testInfoPtr = findAliasTest( isServerTest ? \
								&serverTestInfo[ 1 ] : &clientTestInfo[ 1 ],
								testInfoPtr->aliasTestName );
		if( testInfoPtr == NULL )
			{
			assert( 0 );
			return( FALSE );
			}
		}

	/* Wait for the server to finish initialising */
	if( waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n", 
				__LINE__ );
		return( FALSE );
		}
	if( !isLoopbackTest )
		{
		/* Clear any custom config, provided we're not running a loopback 
		   test, in which case we'd be overwriting the options that have 
		   already been set by the server */
		cryptSuiteBTestConfig( SUITEB_TEST_NONE );
		}

	printf( "Running Suite B client " );
	if( flags & TESTFLAG_GENERIC )
		printf( "as generic test client.\n" );
	else
		printf( "with test %s.\n", testInfoPtr->testName );

	/* Create the SSL/TLS session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED, 
								 CRYPT_SESSION_SSL );
	if( status == CRYPT_ERROR_PARAM3 )	/* SSL/TLS session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_VERSION, 3 );
	if( cryptStatusOK( status ) && testInfoPtr->clientOptions != 0 )
		{
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_SSL_OPTIONS, 
									testInfoPtr->clientOptions );
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Set up the client information */
	if( isLoopbackTest )
		{
		/* We're running the loopback test, set up a local connect */
		if( !setLocalConnect( cryptSession, 443 ) )
			return( FALSE );
		}
	else
		{
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SERVER_NAME,
										  hostName, strlen( hostName ) );
		if( cryptStatusOK( status ) && port != 0 && port != 443 )
			status = cryptSetAttribute( cryptSession, 
										CRYPT_SESSINFO_SERVER_PORT, port );
		if( cryptStatusError( status ) )
			{
			printf( "cryptSetAttribute()/cryptSetAttributeString() failed "
					"with error code %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}
	if( cryptStatusOK( status ) && \
		testInfoPtr->clientAuthKeySizeBits > 0 )
		{
		CRYPT_CONTEXT privateKey;
		char filenameBuffer[ FILENAME_BUFFER_SIZE ];
#ifdef UNICODE_STRINGS
		wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];
#endif /* UNICODE_STRINGS */
		void *fileNamePtr = filenameBuffer;

		/* Depending on which server we're testing against we need to use 
		   different private keys */
		filenameFromTemplate( filenameBuffer, SERVER_ECPRIVKEY_FILE_TEMPLATE, 
							  testInfoPtr->clientAuthKeySizeBits );
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
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute/AttributeString() failed with error code "
				"%d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* For the loopback test we also increase the connection timeout to a 
	   higher-than-normal level, since this gives us more time for tracing 
	   through the code when debugging */
	cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_CONNECTTIMEOUT, 120 );

	/* Set any custom client configuration that may be required */
	switch( testInfoPtr->handlingType )
		{
		case SPECIAL_CLI_INVALIDCURVE:
			/* Client sends non-Suite B curve */
			status = cryptSuiteBTestConfig( SUITEB_TEST_CLIINVALIDCURVE );
			break;

		case SPECIAL_BOTH_SUPPALGO:
			/* Client must send supported_curves extension for both P256 
			   and P384 curves */
			status = cryptSuiteBTestConfig( SUITEB_TEST_BOTHSIGALGOS );
			break;
		}
	if( cryptStatusError( status ) )
		{
		printf( "Custom config set failed with error code %d, line %d.\n", 
				status, __LINE__ );
		return( FALSE );
		}

	/* Activate the client session */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( ( testInfoPtr->result && !cryptStatusOK( status ) ) || \
		( !testInfoPtr->result && !cryptStatusError( status ) ) )
		{
		if( testInfoPtr->result )
			printf( "Test %s failed, should have succeeded.\n", testName );
		else
			printf( "Test %s succeeded, should have failed.\n", testName );
		if( cryptStatusError( status ) )
			{
			printExtError( cryptSession, "Failure reason is:", status, 
						   __LINE__ );
			}
		cryptDestroySession( cryptSession );

		return( FALSE );
		}

	/* Perform any custom post-activation checking that may be required */
	if( testInfoPtr->handlingType != 0 || handlingTypeAlt != 0 )
		{
		const SPECIAL_HANDLING_TYPE handlingType = \
				( handlingTypeAlt != 0 ) ? handlingTypeAlt : \
										   testInfoPtr->handlingType;
		BYTE buffer[ 1024 ];
		int length;

		switch( handlingType )
			{
			case SPECIAL_CLI_INVALIDCURVE:
			case SPECIAL_BOTH_SUPPALGO:
				/* Handled by checking whether the session activation 
				   failed/succeeded */
				break;

			case SPECIAL_SVR_TLSALERT:
				status = cryptGetAttributeString( cryptSession, 
									CRYPT_ATTRIBUTE_ERRORMESSAGE, buffer, 
									&length );
				if( cryptStatusError( status ) || \
					memcmp( buffer, "Received TLS alert", 18 ) )
					{
					printf( "Test %s should have returned a TLS alert but "
							"didn't.\n", testName );
					return( FALSE );
					}
				break;

			case SPECIAL_SVR_INVALIDCURVE:
				/* Handled/checked on the server */
				break;

			default:
				assert( 0 );
				return( FALSE );
			}
		}

	/* If we're being asked to send HTTP data, send a basic GET */
	if( sendHTTP )
		{
		const char *fetchString = "GET / HTTP/1.0\r\n\r\n";
		const int fetchStringLen = sizeof( fetchString ) - 1;
		int bytesCopied;

		status = cryptPushData( cryptSession, fetchString,
								fetchStringLen, &bytesCopied );
		if( cryptStatusOK( status ) )
			status = cryptFlushData( cryptSession );
		if( cryptStatusError( status ) || bytesCopied != fetchStringLen )
			{
			printExtError( cryptSession, "Attempt to send data to server", 
						   status, __LINE__ );
			cryptDestroySession( cryptSession );
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
	printf( "Suite B client test %s succeeded.\n", testName );

	return( TRUE );
	}

static int suitebTest( const BOOLEAN isServer, const int testNo, 
					   const char *hostName, const int port, 
					   const int flags )
	{
	BOOLEAN isServerTest = FALSE;
	int value = testNo, status;

	if( value < 0 )
		{
		isServerTest = TRUE;
		value = -value;
		}

	/* Set up default argument values if required */
	if( hostName == NULL )
		hostName = "localhost";

	/* Run the test client or server */
	createMutex();
	if( isServer )
		status = suitebServer( value, hostName, port, flags, 
							   isServerTest );
	else
		status = suitebClient( value, hostName, port, flags, 
							   isServerTest );
	destroyMutex();
	return( status );
	}

#ifdef WINDOWS_THREADS

unsigned __stdcall suitebServerThread( void *arg )
	{
	BOOLEAN isServerTest = FALSE;
	int value = *( ( int * ) arg );

	if( value < 0 )
		{
		isServerTest = TRUE;
		value = -value;
		}
	suitebServer( value, "localhost", 0, FALSE, isServerTest );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionSuiteBClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	BOOLEAN isServerTest = FALSE;
	int value = -1, status;	/* +ve = client, -ve = server */

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, suitebServerThread,
										 &value, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	if( value < 0 )
		{
		isServerTest = TRUE;
		value = -value;
		}
	status = suitebClient( value, "localhost", 0, FALSE, isServerTest );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}
#endif /* WINDOWS_THREADS */

/****************************************************************************
*																			*
*								Suite B Keygen								*
*																			*
****************************************************************************/

/* Generate an ECDSA private key and certificate request */

static const CERT_DATA FAR_BSS certRequestData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "localhost" ) },

	{ CRYPT_ATTRIBUTE_NONE, 0, 0, NULL }
	};

static int generateKey( const int keyBits, const char *certRequestFileName )
	{
	CRYPT_KEYSET cryptKeyset;
	CRYPT_CERTIFICATE cryptCertRequest;
	CRYPT_CONTEXT cryptKey;
	FILE *filePtr;
	BYTE certBuffer[ BUFFER_SIZE ];
	char filenameBuffer[ FILENAME_BUFFER_SIZE ];
	void *fileNamePtr = filenameBuffer;
	int length, count, status;

	/* Generate a key to certify.  We can't just reuse the built-in test key
	   because this has already been used as the CA key and the keyset code
	   won't allow it to be added to a keyset as both a CA key and user key,
	   so we have to generate a new one */
	status = cryptCreateContext( &cryptKey, CRYPT_UNUSED, CRYPT_ALGO_ECDSA );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptKey, CRYPT_CTXINFO_KEYSIZE, 
									keyBits >> 3 );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttributeString( cryptKey, CRYPT_CTXINFO_LABEL,
										  USER_PRIVKEY_LABEL,
										  paramStrlen( USER_PRIVKEY_LABEL ) );
		}
	if( cryptStatusOK( status ) )
		status = cryptGenerateKey( cryptKey );
	if( cryptStatusError( status ) )
		{
		printf( "Key generation failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Create the certificate request for the new key */
	status = cryptCreateCert( &cryptCertRequest, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTREQUEST );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptCertRequest,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cryptKey );
	if( cryptStatusOK( status ) && \
		!addCertFields( cryptCertRequest, certRequestData, __LINE__ ) )
		return( FALSE );
	if( cryptStatusOK( status ) )
		status = cryptSignCert( cryptCertRequest, cryptKey );
	if( cryptStatusOK( status ) )
		{
		status = cryptExportCert( certBuffer, BUFFER_SIZE, &length,
								  CRYPT_CERTFORMAT_CERTIFICATE, 
								  cryptCertRequest );
		cryptDestroyCert( cryptCertRequest );
		}
	if( cryptStatusError( status ) )
		{
		printf( "Certificate request creation failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( ( filePtr = fopen( certRequestFileName, "wb" ) ) != NULL )
		{
		count = fwrite( certBuffer, 1, length, filePtr );
		fclose( filePtr );
		}
	if( filePtr == NULL || count < length )
		{
		printf( "Couldn't write certificate request to disk, line %d.\n", 
				__LINE__ );
		return( FALSE );
		}

	/* Create the keyset and add the private key to it */
	filenameFromTemplate( filenameBuffer, SERVER_ECPRIVKEY_FILE_TEMPLATE, 
						  keyBits );
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  fileNamePtr, CRYPT_KEYOPT_CREATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetOpen() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptAddPrivateKey( cryptKeyset, cryptKey,
								 TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptKeyset, "cryptAddPrivateKey()", status, 
					   __LINE__ );
		return( FALSE );
		}
	cryptDestroyContext( cryptKey );
	cryptKeysetClose( cryptKeyset );

	return( TRUE );
	}

/* Add an ECDSA certificate to a previously-generated key */

static int updateKey( const int keyBits, const char *certFileName )
	{
	CRYPT_KEYSET cryptKeyset;
	CRYPT_CERTIFICATE cryptCert;
	char filenameBuffer[ FILENAME_BUFFER_SIZE ];
	void *fileNamePtr = filenameBuffer;
	int status;

	/* Import the certificate from the file */
	status = importCertFile( &cryptCert, certFileName );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't import certificate from file, status %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Add the certificate to the file */
	filenameFromTemplate( filenameBuffer, SERVER_ECPRIVKEY_FILE_TEMPLATE, 
						  keyBits );
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  fileNamePtr, CRYPT_KEYOPT_NONE );
	if( cryptStatusOK( status ) )
		{
		status = cryptAddPublicKey( cryptKeyset, cryptCert );
		cryptKeysetClose( cryptKeyset );
		}
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't update private key with certificate, status %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*							Suite B Test Application						*
*																			*
****************************************************************************/

/* Show usage and exit */

static void usageExit( void )
	{
	puts( "Usage: testlib c|s [-h hostname] [-p port] [-s] <test>" );

	puts( "  Operation type:" );
	puts( "    c = Run test as the client." );
	puts( "    s = Run test as the server." );
	puts( "    <test> = Test name, e.g. 'A.1.1.1', or '128'/'256' to run as a" );
	puts( "             generic 128/256-bit client or server." );
	puts( "" );

	puts( "  Options:" );
	puts( "    -s = Send HTTP request." );
	puts( "    -h <hostname> = Host name (default localhost)." );
	puts( "    -p <port> = Port (default 443 or 4443)." );
	puts( "    -- = End of arg list." );
	puts( "" );
	showTestInfo();
	puts( "" );

	puts( "  Keygen commands:" );
	puts( "" );
	puts( "    kg [256|384] <cert-request filename>" );
	puts( "      = Generate 256/384-bit key and PKCS #10 cert request." );
	puts( "    ka [256|384] <certificate filename>" );
	puts( "      = Add issued certificate to previously-generated 256/384-bit key." );
	puts( "" );

	exit( EXIT_FAILURE );
	}

/* Process command-line arguments */

static int processArgs( int argc, char **argv, int *testNo, 
						const char **stringArg, int *intArg, 
						BOOLEAN *isServer, BOOLEAN *sendHTTPheader )
	{
	const char *argPtr = argv[ 0 ];
	BOOLEAN moreArgs = TRUE;
	int value;

	/* Clear return values */
	*testNo = *intArg = 0;
	*stringArg = NULL;
	*isServer = *sendHTTPheader = FALSE;

	/* No args means display a usage message */
	if( argc <= 0 )
		usageExit();

	/* Process the main command */
	switch( tolower( *argPtr ) )
		{
		case 'c':
			*isServer = FALSE;
			argv++; argc--;
			break;
		
		case 's':
			*isServer = TRUE;
			argv++; argc--;
			break;

		case 'k':
			switch( tolower( argPtr[ 1 ] ) )
				{
				case 'g':
					*testNo = -1;	/* Generate key */
					break;

				case 'a':
					*testNo = -2;	/* Add certificate */
					break;

				default:
					printf( "Error: Unknown key management option '%c'.\n", 
							argPtr[ 1 ] );
					return( FALSE );
				}

			/* Get the key size */
			argv++; argc--;
			if( argc <= 0 )
				{
				puts( "Error: Missing key size." );
				return( FALSE );
				}
			argPtr = argv[ 0 ];
			value = -1;
			if( *argPtr == '2' || *argPtr == '3' )
				value = atoi( argv[ 0 ] );
			if( value != 256 && value != 384 )
				{
				puts( "Error: Key size must be '256' or '384'." );
				return( FALSE );
				}
			*intArg = value;

			/* Get the filename */
			argv++; argc--;
			if( argc <= 0 )
				{
				puts( "Error: Missing file name." );
				return( FALSE );
				}
			*stringArg = argv[ 0 ];
			if( *testNo == -2 )
				{
				FILE *filePtr;

				/* Make sure that the specified input file exists */
				filePtr = fopen( argv[ 0 ], "rb" );
				if( filePtr == NULL )
					{
					printf( "Error: Can't open '%s'.\n", argv[ 0 ] );
					return( FALSE );
					}
				fclose( filePtr );
				}

			/* Make sure that's all */
			argv++; argc--;
			if( argc != 0 )
				{
				printf( "Error: Spurious option '%s'.\n", argv[ 0 ] );
				return( FALSE );
				}
			break;

		default:
			usageExit();
		}

	/* Process any additional arguments */
	while( argc > 0 && moreArgs )
		{
		argPtr = argv[ 0 ];
		if( *argPtr == '1' || *argPtr == '2' )
			{
			/* The user has asked to run as a generic 128/256-bit client or 
			   server, map it to an equivalent test number */
			value = atoi( argPtr );
			if( value != 128 && value != 256 )
				{
				puts( "Error: Generic client/server type must be '128' "
					  "or '256'." );
				return( FALSE );
				}
			if( value == 128 )
				argPtr = "A.1.1.1";
			else
				argPtr = "A.1.2.1";
			}
		if( *argPtr == 'a' || *argPtr == 'A' )
			{
			int value;

			if( *testNo != 0 )
				usageExit();

			/* Perform a basic validity test that the argument is in the 
			   format "A.[12].[1-3]" */
			if( argPtr[ 1 ] != '.' || \
				( argPtr[ 2 ] != '1' && argPtr[ 2 ] != '2' ) || \
				argPtr[ 3 ] != '.' || \
				( argPtr[ 4 ] < '1' || argPtr[ 4 ] > '3' ) )
				usageExit();
			value = lookupTestNo( argPtr );
			if( value == 0 )
				{
				printf( "Error: Unknown test type '%s'.\n", argPtr );
				return( FALSE );
				}
			*testNo = value;
			argv++;
			argc--;
			continue;
			}
		if( *argPtr != '-' )
			{
			printf( "Error: Unknown argument '%s'.\n", argPtr );
			return( FALSE );
			}
		argPtr++;
		while( *argPtr )
			{
			switch( toupper( *argPtr ) )
				{
				case '-':
					moreArgs = FALSE;	/* GNU-style end-of-args flag */
					break;

				case 'H':
					if( *stringArg != NULL )
						usageExit();
					argv++; argc--;
					if( argc <= 0 )
						{
						puts( "Error: Missing host name" );
						return( FALSE );
						}
					*stringArg = argv[ 0 ];
					argPtr = "";
					break;

				case 'P':
					if( *intArg != 0 )
						usageExit();
					argv++; argc--;
					if( argc <= 0 )
						{
						puts( "Error: Missing port number" );
						return( FALSE );
						}
					value = atoi( argv[ 0 ] );
					if( value < 20 || value > 64000 )
						usageExit();
					*intArg = value;
					argPtr = "";
					break;

				case 'S':
					if( *sendHTTPheader != FALSE )
						usageExit();
					*sendHTTPheader = TRUE;
					argPtr++;
					break;

				default:
					printf( "Error: Unknown argument '%c'.\n", *argPtr );
					return( FALSE );
				}
			}
		argv++;
		argc--;
		}
	if( *testNo == 0 )
		{
		puts( "Error: Missing test name" );
		return( FALSE );
		}

	return( TRUE );
	}

/* The main entry point for the Suite B test application */

int suiteBMain( int argc, char **argv )
	{
	const char *stringArg;
	BOOLEAN isServer, sendHTTPheader;
	int testNo, intArg, status;
	void testSystemSpecific1( void );
	void testSystemSpecific2( void );

	printf( "testlib - cryptlib %d-bit Suite B test framework.\n", 
			( int ) sizeof( long ) * 8 );	/* Cast for gcc */
	puts( "Copyright Peter Gutmann 1995 - 2011." );
	puts( "" );

	/* Skip the program name and process any command-line arguments */
	argv++; argc--;
	status = processArgs( argc, argv, &testNo, &stringArg, &intArg, 
						  &isServer, &sendHTTPheader );
	if( !status )
		exit( EXIT_FAILURE );

	/* Make sure that various system-specific features are set right */
	testSystemSpecific1();

	/* Initialise cryptlib */
	status = cryptInit();
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptInit() failed with error code %d, line %d.\n", 
				status, __LINE__ );
		exit( EXIT_FAILURE );
		}

	/* In order to avoid having to do a randomness poll for every test run,
	   we bypass the randomness-handling by adding some junk.  This is only
	   enabled when cryptlib is built in debug mode so it won't work with 
	   any production systems */
	cryptAddRandom( "xyzzy", 5 );

	/* Perform a general sanity check to make sure that the self-test is
	   being run the right way */
	if( !checkFileAccess() )
		exit( EXIT_FAILURE );

	/* Make sure that further system-specific features that require cryptlib 
	   to be initialised to check are set right */
	testSystemSpecific2();

	/* If the test number is negative, it's a special-case non-test 
	   operation */
	if( testNo < 0 )
		{
		switch( testNo )
			{
			case -1:
				status = generateKey( intArg, stringArg );
				if( status == TRUE )
					printf( "Wrote %d-bit certificate request to "
							"file '%s'.\n\n", intArg, stringArg );
				break;

			case -2:
				status = updateKey( intArg, stringArg );
				if( status == TRUE )
					printf( "Updated %d-bit key with certificate from "
							"file '%s'.\n\n", intArg, stringArg );
				break;
			}
		}
	else
		{
		/* Run the selected test */
		status = suitebTest( isServer, testNo, stringArg, intArg, 
							 sendHTTPheader );
		}

	/* Clean up and exit */
	cryptEnd();
	return( ( status == TRUE ) ? EXIT_SUCCESS : EXIT_FAILURE );
	}
#endif /* CONFIG_SUITEB_TESTS */
