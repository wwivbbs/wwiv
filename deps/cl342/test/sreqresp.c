/****************************************************************************
*																			*
*				cryptlib Request/Response Session Test Routines				*
*						Copyright Peter Gutmann 1998-2011					*
*																			*
****************************************************************************/

#include "cryptlib.h"
#include "test/test.h"

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */

/* Prototypes for functions in certs.c */

int initRTCS( CRYPT_CERTIFICATE *cryptRTCSRequest, 
			  const CRYPT_CERTIFICATE cryptCertificateTemplate,
			  const int number, const BOOLEAN multipleCerts );
int initOCSP( CRYPT_CERTIFICATE *cryptOCSPRequest, 
			  CRYPT_CERTIFICATE *cert1, CRYPT_CERTIFICATE *cert2,
			  const int number, const BOOLEAN ocspv2, 
			  const BOOLEAN revokedCert, const BOOLEAN multipleCerts,
			  const CRYPT_SIGNATURELEVEL_TYPE sigLevel,
			  const CRYPT_CONTEXT privKeyContext );

#if defined( TEST_SESSION ) || defined( TEST_SESSION_LOOPBACK )

/****************************************************************************
*																			*
*							HTTP Certstore Routines Test					*
*																			*
****************************************************************************/

/* This isn't really a proper session but just an HTTP certificate store 
   interface, but the semantics for the server side fit the session 
   interface better than the keyset interface */

static int connectCertstoreServer( void )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_KEYSET cryptCertStore;
	int connectionActive, status;

	puts( "Testing HTTP certstore server session..." );

	/* Create the HTTP certstore session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_CERTSTORE_SERVER );
	if( status == CRYPT_ERROR_PARAM3 )	/* Certstore session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	if( !setLocalConnect( cryptSession, 80 ) )
		return( FALSE );

	/* Add the certificate store that we'll be using to provide certs (it's
	   actually just the generic database keyset and not the full 
	   certificate store, because this contains more test certs) */
	status = cryptKeysetOpen( &cryptCertStore, CRYPT_UNUSED,
							  DATABASE_KEYSET_TYPE, DATABASE_KEYSET_NAME,
							  CRYPT_KEYOPT_READONLY );
	if( status == CRYPT_ERROR_PARAM3 )
		{
		/* This type of keyset access isn't available, return a special
		   error code to indicate that the test wasn't performed, but
		   that this isn't a reason to abort processing */
		puts( "SVR: No certificate store available, aborting HTTP certstore "
				  "responder test.\n" );
		cryptDestroySession( cryptSession );
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( cryptSession,
						CRYPT_SESSINFO_KEYSET, cryptCertStore );
		cryptKeysetClose( cryptCertStore );
		}
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptSession, "SVR: cryptSetAttribute()",
							   status, __LINE__ ) );

	/* Activate the server */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	printConnectInfo( cryptSession );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "SVR: Attempt to activate HTTP "
					   "certstore server session", status, __LINE__ );
		cryptDestroySession( cryptSession );
		return( FALSE );
		}

	/* Check whether the session connection is still open */
	status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_CONNECTIONACTIVE,
								&connectionActive );
	if( cryptStatusError( status ) || !connectionActive )
		{
		printExtError( cryptSession, "SVR: Persistent connection has been "
					   "closed, operation", status, __LINE__ );
		return( FALSE );
		}

	/* Activate the connection to handle two more requests */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "SVR: Attempt to perform second HTTP "
					   "certstore server transaction", status, __LINE__ );
		cryptDestroySession( cryptSession );
		return( status );
		}
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "SVR: Attempt to perform third HTTP "
					   "certstore server transaction", status, __LINE__ );
		cryptDestroySession( cryptSession );
		return( status );
		}

	/* Clean up */
	status = cryptDestroySession( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroySession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	puts( "SVR: HTTP certstore server session succeeded.\n" );
	return( TRUE );
	}

static int connectCertstoreClient( void )
	{
	CRYPT_KEYSET cryptKeyset;
	CRYPT_CERTIFICATE cryptCert;
	const C_STR cert1ID = TEXT( "dave@wetaburgers.com" );
	const C_STR cert2ID = TEXT( "notpresent@absent.com" );
	int status;

	/* Open the keyset with a check to make sure this access method exists
	   so we can return an appropriate error message */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_HTTP,
							  TEXT( "localhost" ), CRYPT_KEYOPT_READONLY );
	if( status == CRYPT_ERROR_PARAM3 )
		{
		/* This type of keyset access not available */
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetOpen() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( CRYPT_ERROR_FAILED );
		}

	/* Read a present certificate from the keyset using the ASCII email
	   address */
	status = cryptGetPublicKey( cryptKeyset, &cryptCert, CRYPT_KEYID_EMAIL,
								cert1ID );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptKeyset, "cryptGetPublicKey()", status,
							  __LINE__ ) );
	printf( "Successfully read certificate for '%s'.\n", cert1ID );
	cryptDestroyCert( cryptCert );

	/* Read a non-present certificate from the keyset */
	status = cryptGetPublicKey( cryptKeyset, &cryptCert, CRYPT_KEYID_EMAIL,
								cert2ID );
	if( status == CRYPT_ERROR_NOTFOUND )
		printf( "Successfully processed not-present code for '%s'.\n",
				cert2ID );
	else
		return( extErrorExit( cryptKeyset, "cryptGetPublicKey()", status,
							  __LINE__ ) );

	/* Read the certificate from the keyset using the base64-encoded certID.
	   Since this uses an internal identifier, we can't actually do it from
	   here, this requires modifying the internal keyset read code to
	   substitute the different identifier type.
	   
	   A second purpose for this call is to test the ability of the client
	   to recover from the CRYPT_ERROR_NOTFOUND in the previous call, i.e.
	   the error should be nonfatal with further requests possible */
	status = cryptGetPublicKey( cryptKeyset, &cryptCert, CRYPT_KEYID_EMAIL,
								cert1ID );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptKeyset, "cryptGetPublicKey()", status,
							  __LINE__ ) );
	printf( "Successfully read certificate for '%s'.\n", cert1ID );
	cryptDestroyCert( cryptCert );

	/* Clean up */
	cryptKeysetClose( cryptKeyset );
	return( TRUE );
	}

int testSessionHTTPCertstoreServer( void )
	{
	return( connectCertstoreServer() );
	}

/* Perform a client/server loopback test */

#ifdef WINDOWS_THREADS

unsigned __stdcall certstoreServerThread( void *dummy )
	{
	connectCertstoreServer();
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionHTTPCertstoreClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server and wait for it to initialise */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, certstoreServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectCertstoreClient();
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}
#endif /* WINDOWS_THREADS */

/****************************************************************************
*																			*
*								RTCS Routines Test							*
*																			*
****************************************************************************/

/* There are various test RTCS servers running, the following remapping
   allows us to switch between them.  Implementation peculiarities:

	#1 - cryptlib:
			None */

#define RTCS_SERVER_NO		1
#if RTCS_SERVER_NO == 1
  #define RTCS_SERVER_NAME	TEXT( "http://localhost" )
#endif /* RTCS server name kludge */

/* Perform an RTCS test */

static int connectRTCS( const CRYPT_SESSION_TYPE sessionType,
						const BOOLEAN multipleCerts,
						const BOOLEAN localSession )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_CERTIFICATE cryptRTCSRequest;
	char filenameBuffer[ FILENAME_BUFFER_SIZE ];
#ifdef UNICODE_STRINGS
	wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];
#endif /* UNICODE_STRINGS */
	void *fileNamePtr = filenameBuffer;
	const BOOLEAN isServer = ( sessionType == CRYPT_SESSION_RTCS_SERVER ) ? \
							   TRUE : FALSE;
	int status;

	printf( "%sTesting %sRTCS session...\n", isServer ? "SVR: " : "",
			localSession ? "local " : "" );

	/* If we're the client, wait for the server to finish initialising */
	if( localSession && !isServer && waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n", 
				__LINE__ );
		return( FALSE );
		}

	/* Create the RTCS session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED, sessionType );
	if( status == CRYPT_ERROR_PARAM3 )	/* RTCS session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	if( isServer )
		{
		CRYPT_CONTEXT cryptPrivateKey;
		CRYPT_KEYSET cryptCertStore;

		if( !setLocalConnect( cryptSession, 80 ) )
			return( FALSE );

		/* Add the responder private key */
		filenameFromTemplate( filenameBuffer, SERVER_PRIVKEY_FILE_TEMPLATE, 1 );
#ifdef UNICODE_STRINGS
		mbstowcs( wcBuffer, filenameBuffer, strlen( filenameBuffer ) + 1 );
		fileNamePtr = wcBuffer;
#endif /* UNICODE_STRINGS */
		status = getPrivateKey( &cryptPrivateKey, fileNamePtr, 
								USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_PRIVATEKEY, cryptPrivateKey );
			cryptDestroyContext( cryptPrivateKey );
			}
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptSession, "SVR: cryptSetAttribute()",
								   status, __LINE__ ) );

		/* Add the certificate store that we'll be using to provide 
		   revocation information */
		status = cryptKeysetOpen( &cryptCertStore, CRYPT_UNUSED,
								  DATABASE_KEYSET_TYPE, CERTSTORE_KEYSET_NAME,
								  CRYPT_KEYOPT_READONLY );
		if( status == CRYPT_ERROR_PARAM3 )
			{
			/* This type of keyset access isn't available, return a special
			   error code to indicate that the test wasn't performed, but
			   that this isn't a reason to abort processing */
			puts( "SVR: No certificate store available, aborting RTCS "
				  "responder test.\n" );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_NOTAVAIL );
			}
		if( status == CRYPT_ERROR_OPEN )
			{
			/* The keyset is available, but it hasn't been created yet by an
			   earlier self-test, this isn't a reason to abort processing */
			puts( "SVR: Certificate store hasn't been created yet by "
				  "earlier tests, aborting\n     RTCS responder test.\n" );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_NOTAVAIL );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_KEYSET, cryptCertStore );
			cryptKeysetClose( cryptCertStore );
			}
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptSession, "SVR: cryptSetAttribute()",
								   status, __LINE__ ) );

		/* Tell the client that we're ready to go */
		if( localSession )
			releaseMutex();
		}
	else
		{
		CRYPT_KEYSET cryptKeyset;
		CRYPT_CERTIFICATE cryptCert DUMMY_INIT;

		/* Get the certificate whose status we're checking */
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  DATABASE_KEYSET_TYPE, CERTSTORE_KEYSET_NAME,
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusOK( status ) )
			{
			status = cryptGetPublicKey( cryptKeyset, &cryptCert, 
										CRYPT_KEYID_NAME, 
										TEXT( "Test user 1" ) );
			cryptKeysetClose( cryptKeyset );
			}
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't read certificate for RTCS status check, error "
					"code %d, line %d.\n", status, __LINE__ );
			puts( "  (Has the testCertManagement() code been run?)." );
			return( FALSE );
			}

		/* Create the RTCS request */
		if( !initRTCS( &cryptRTCSRequest, cryptCert, localSession ? \
							1 : RTCS_SERVER_NO, multipleCerts ) )
			return( FALSE );
		cryptDestroyCert( cryptCert );

		/* Set up the server information and activate the session.  In
		   theory the RTCS request will contain all the information needed
		   for the session so there'd be nothing else to add before we
		   activate it, however many certs contain incorrect server URLs so
		   we set the server name manually if necessary, overriding the
		   value present in the RTCS request (via the certificate) */
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_REQUEST,
									cryptRTCSRequest );
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptSession, "cryptSetAttribute()",
								   status, __LINE__ ) );
		cryptDestroyCert( cryptRTCSRequest );
		if( localSession && !setLocalConnect( cryptSession, 80 ) )
			return( FALSE );
#ifdef RTCS_SERVER_NAME
		if( !localSession )
			{
			printf( "Setting RTCS server to %s.\n", RTCS_SERVER_NAME );
			cryptDeleteAttribute( cryptSession, CRYPT_SESSINFO_SERVER_NAME );
			status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_SERVER_NAME, RTCS_SERVER_NAME,
								paramStrlen( RTCS_SERVER_NAME ) );
			if( cryptStatusError( status ) )
				return( attrErrorExit( cryptSession,
									   "cryptSetAttributeString()", status,
									   __LINE__ ) );
			}
#endif /* Kludges for incorrect/missing authorityInfoAccess values */

		/* Wait for the server to finish initialising */
		if( localSession && waitMutex() == CRYPT_ERROR_TIMEOUT )
			{
			printf( "Timed out waiting for server to initialise, line %d.\n",
					__LINE__ );
			return( FALSE );
			}		
		}
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( isServer )
		printConnectInfo( cryptSession );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, isServer ? \
					   "SVR: Attempt to activate RTCS server session" : \
					   "Attempt to activate RTCS client session", status,
					   __LINE__ );
		if( !isServer && isServerDown( cryptSession, status ) )
			{
			puts( "  (Server could be down, faking it and continuing...)\n" );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_FAILED );
			}
		cryptDestroySession( cryptSession );
		return( FALSE );
		}

	/* Obtain the response information */
	if( !isServer )
		{
		CRYPT_CERTIFICATE cryptRTCSResponse;
		
		status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_RESPONSE,
									&cryptRTCSResponse );
		if( cryptStatusError( status ) )
			{
			printf( "cryptGetAttribute() failed with error code %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		printCertInfo( cryptRTCSResponse );
		cryptDestroyCert( cryptRTCSResponse );
		}

	/* Clean up */
	status = cryptDestroySession( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroySession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	puts( isServer ? "SVR: RTCS server session succeeded.\n" : \
					 "RTCS client session succeeded.\n" );
	return( TRUE );
	}

static int connectRTCSDirect( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_SESSION cryptSession;
	int status;

	printf( "Testing direct RTCS query...\n" );

	/* Get the EE certificate */
	status = importCertFromTemplate( &cryptCert, RTCS_FILE_TEMPLATE,
									 RTCS_SERVER_NO );
	if( cryptStatusError( status ) )
		{
		printf( "EE cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Create the RTCS session and add the server URL */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_RTCS );
	if( status == CRYPT_ERROR_PARAM3 )	/* RTCS session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
#ifdef RTCS_SERVER_NAME
	status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_SERVER_NAME, RTCS_SERVER_NAME,
								paramStrlen( RTCS_SERVER_NAME ) );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptSession, "cryptSetAttributeString()",
							   status, __LINE__ ) );
#endif /* Kludges for incorrect/missing authorityInfoAccess values */

	/* Check the certificate directly against the server */
	status = cryptCheckCert( cryptCert, cryptSession );
	printf( "Certificate status check returned %d.\n", status );

	/* Clean up */
	cryptDestroyCert( cryptCert );
	cryptDestroySession( cryptSession );

	puts( "RTCS direct query succeeded.\n" );
	return( TRUE );
	}

int testSessionRTCS( void )
	{
	if( !connectRTCS( CRYPT_SESSION_RTCS, FALSE, FALSE ) )
		return( FALSE );
	if( !connectRTCSDirect() )
		return( FALSE );
#if RTCS_SERVER_NO == 1
	return( connectRTCS( CRYPT_SESSION_RTCS, TRUE, FALSE ) );
#else
	return( TRUE );
#endif /* Server that has a revoked certificate */
	}
int testSessionRTCSServer( void )
	{
	int status;

	createMutex();
	acquireMutex();
	status = connectRTCS( CRYPT_SESSION_RTCS_SERVER, FALSE, FALSE );
	destroyMutex();

	return( status );
	}

/* Perform a client/server loopback test */

#ifdef WINDOWS_THREADS

unsigned __stdcall rtcsServerThread( void *dummy )
	{
	acquireMutex();
	connectRTCS( CRYPT_SESSION_RTCS_SERVER, FALSE, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionRTCSClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server and wait for it to initialise */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, rtcsServerThread,
										 NULL, 0, &threadID );
	Sleep( 2000 );

	/* Connect to the local server */
	status = connectRTCS( CRYPT_SESSION_RTCS, FALSE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}
#endif /* WINDOWS_THREADS */

/****************************************************************************
*																			*
*								OCSP Routines Test							*
*																			*
****************************************************************************/

/* There are various test OCSP servers running, the following remapping
   allows us to switch between them.  Implementation peculiarities:

	#1 - cryptlib:
			None
	#2 - iD2 aka SmartTrust
			AuthorityInfoAccess doesn't match the real server URL, requires
			the SmartTrust server name below to override the AIA value.
			Currently not active.
	#3 - Identrus aka Xetex:
			AuthorityInfoAccess doesn't match the real server URL, requires
			the Xetex server name below to override the AIA value.  Currently
			not active.
	#4 - Thawte aka Valicert
			No AuthorityInfoAccess, requires the Valicert server name below
			to provide a server.  Since all Thawte CA certs are invalid (no
			keyUsage, meaning they're non-CA certs) cryptlib will reject them
			for OCSPv1 queries.
	#5 - Verisign
			No AuthorityInfoAccess, requires the Verisign server name below
			to provide a server.
	#6 - Diginotar
			Have an invalid CA certificate, and (apparently) a broken OCSP
			implementation that gets the IDs wrong (this is par for the
			course for this particular CA).
	#7 - Windows Server 2008
			Returns a permission-denied error with the default server 
			configuration.  This is because Windows Server by default doesn't 
			allow nonces, and responds to any request containing a nonce 
			with a permission-denied error.  Enabling nonces via Revocation 
			Configurations | Action | Edit Properties | Allow Nonce requests 
			corrects this */

#define OCSP_SERVER_NO		5
#if OCSP_SERVER_NO == 2
  #define OCSP_SERVER_NAME	TEXT( "http://ocsp.smarttrust.com:82/ocsp" )
#elif OCSP_SERVER_NO == 3
  #define OCSP_SERVER_NAME	TEXT( "http://ocsp.xetex.com:8080/servlet/ocsp" )
#elif OCSP_SERVER_NO == 4
  #define OCSP_SERVER_NAME	TEXT( "http://ocsp2.valicert.net" )
#elif OCSP_SERVER_NO == 5
  #define OCSP_SERVER_NAME	TEXT( "http://ocsp.verisign.com/ocsp/status" )
#elif OCSP_SERVER_NO == 7
	#define OCSP_SERVER_NAME	TEXT( "http://142.176.86.157/ocsp" )
#endif /* OCSP server name kludge */

/* Perform an OCSP test */

static int connectOCSP( const CRYPT_SESSION_TYPE sessionType,
						const BOOLEAN revokedCert,
						const BOOLEAN multipleCerts,
						const BOOLEAN localSession )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_CERTIFICATE cryptOCSPRequest, cryptCert1, cryptCert2;
	char filenameBuffer[ FILENAME_BUFFER_SIZE ];
#ifdef UNICODE_STRINGS
	wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];
#endif /* UNICODE_STRINGS */
	void *fileNamePtr = filenameBuffer;
#if OCSP_SERVER_NO == 7
	int complianceValue;
#endif /* OCSP servers that return broken resposnes */
	const BOOLEAN isServer = ( sessionType == CRYPT_SESSION_OCSP_SERVER ) ? \
							   TRUE : FALSE;
	int status;

	printf( "%sTesting %sOCSP session...\n", isServer ? "SVR: " : "",
			localSession ? "local " : "" );

	/* If we're the client, wait for the server to finish initialising */
	if( localSession && !isServer && waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n", 
				__LINE__ );
		return( FALSE );
		}

	/* Create the OCSP session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED, sessionType );
	if( status == CRYPT_ERROR_PARAM3 )	/* OCSP session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	if( isServer )
		{
		CRYPT_CONTEXT cryptPrivateKey;
		CRYPT_KEYSET cryptCertStore;

		if( !setLocalConnect( cryptSession, 80 ) )
			return( FALSE );

		/* Add the responder private key */
		filenameFromTemplate( filenameBuffer, SERVER_PRIVKEY_FILE_TEMPLATE, 1 );
#ifdef UNICODE_STRINGS
		mbstowcs( wcBuffer, filenameBuffer, strlen( filenameBuffer ) + 1 );
		fileNamePtr = wcBuffer;
#endif /* UNICODE_STRINGS */
		status = getPrivateKey( &cryptPrivateKey, fileNamePtr,
								USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_PRIVATEKEY, cryptPrivateKey );
			cryptDestroyContext( cryptPrivateKey );
			}
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptSession, "SVR: cryptSetAttribute()",
								   status, __LINE__ ) );

		/* Add the certificate store that we'll be using to provide 
		   revocation information */
		status = cryptKeysetOpen( &cryptCertStore, CRYPT_UNUSED,
								  DATABASE_KEYSET_TYPE, CERTSTORE_KEYSET_NAME,
								  CRYPT_KEYOPT_READONLY );
		if( status == CRYPT_ERROR_PARAM3 )
			{
			/* This type of keyset access isn't available, return a special
			   error code to indicate that the test wasn't performed, but
			   that this isn't a reason to abort processing */
			puts( "SVR: No certificate store available, aborting OCSP "
				  "responder test.\n" );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_NOTAVAIL );
			}
		if( status == CRYPT_ERROR_OPEN )
			{
			/* This is the first of the loopback tests that requires the 
			   presence of a certificate store (created by previous tests), 
			   if we can't open it then we report the issue in a situation-
			   specific manner */
			puts( "SVR: Can't open certificate store, have the earlier "
				  "tests that create this\n     been run?\n" );
			cryptDestroySession( cryptSession );
			return( status );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_KEYSET, cryptCertStore );
			cryptKeysetClose( cryptCertStore );
			}
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptSession, "SVR: cryptSetAttribute()",
								   status, __LINE__ ) );

		/* Tell the client that we're ready to go */
		if( localSession )
			releaseMutex();
		}
	else
		{
		/* Create the OCSP request */
		if( !initOCSP( &cryptOCSPRequest, &cryptCert1, &cryptCert2, 
					   localSession ? 1 : OCSP_SERVER_NO, FALSE, 
					   revokedCert, multipleCerts,
					   CRYPT_SIGNATURELEVEL_NONE, CRYPT_UNUSED ) )
			return( FALSE );

		/* Set up the server information and activate the session.  In
		   theory the OCSP request will contain all the information needed
		   for the session so there'd be nothing else to add before we
		   activate it, however many certs contain incorrect server URLs so
		   we set the server name manually if necessary, overriding the
		   value present in the OCSP request (via the certificate) */
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_REQUEST,
									cryptOCSPRequest );
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptSession, "cryptSetAttribute()",
								   status, __LINE__ ) );
		cryptDestroyCert( cryptOCSPRequest );
		if( localSession && !setLocalConnect( cryptSession, 80 ) )
			return( FALSE );
#ifdef OCSP_SERVER_NAME
		if( !localSession )
			{
			printf( "Setting OCSP server to %s.\n", OCSP_SERVER_NAME );
			cryptDeleteAttribute( cryptSession, CRYPT_SESSINFO_SERVER_NAME );
			status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_SERVER_NAME, OCSP_SERVER_NAME,
								paramStrlen( OCSP_SERVER_NAME ) );
			if( cryptStatusError( status ) )
				{
				return( attrErrorExit( cryptSession,
									   "cryptSetAttributeString()", status,
									   __LINE__ ) );
				}
			}
#endif /* Kludges for incorrect/missing authorityInfoAccess values */
		if( OCSP_SERVER_NO == 1 || localSession )
			{
			/* The cryptlib server doesn't handle the weird v1 certIDs */
			status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_VERSION,
										2 );
			if( cryptStatusError( status ) )
				{
				return( attrErrorExit( cryptSession, "cryptSetAttribute()",
									   status, __LINE__ ) );
				}
			}
#if OCSP_SERVER_NO == 7
		/* Some OCSP server's responses are broken so we have to turn down 
		   the compliance level to allow them to be processed */
		cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   &complianceValue );
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   CRYPT_COMPLIANCELEVEL_OBLIVIOUS );
#endif /* OCSP servers that return broken resposnes */

		/* Wait for the server to finish initialising */
		if( localSession && waitMutex() == CRYPT_ERROR_TIMEOUT )
			{
			printf( "Timed out waiting for server to initialise, line %d.\n",
					__LINE__ );
			return( FALSE );
			}		
		}
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
#if OCSP_SERVER_NO == 7
	if( !isServer )
		{
		/* Restore normal certificate processing */
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   complianceValue );
		}
#endif /* OCSP servers that return broken resposnes */
	if( isServer )
		printConnectInfo( cryptSession );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, isServer ? \
					   "SVR: Attempt to activate OCSP server session" : \
					   "Attempt to activate OCSP client session", status,
					   __LINE__ );
#if OCSP_SERVER_NO == 5
		if( status == CRYPT_ERROR_SIGNATURE )
			{
			char errorMessage[ 512 ];
			int errorMessageLength;

			status = cryptGetAttributeString( cryptSession, 
											  CRYPT_ATTRIBUTE_ERRORMESSAGE,
											  errorMessage, &errorMessageLength );
			if( cryptStatusOK( status ) && errorMessageLength >= 29 && \
				!memcmp( errorMessage, "OCSP response doesn't contain", 29 ) )
				{
				cryptDestroyCert( cryptCert1 );
				if( cryptCert2 != CRYPT_UNUSED )
					cryptDestroyCert( cryptCert2 );
				cryptDestroySession( cryptSession );
				puts( "  (Verisign's OCSP responder sends broken responses, "
					  "continuing...)\n" );
				return( CRYPT_ERROR_FAILED );
				}
			}
#endif /* Verisign's broken OCSP responder */
		if( !isServer && isServerDown( cryptSession, status ) )
			{
			puts( "  (Server could be down, faking it and continuing...)\n" );
			cryptDestroyCert( cryptCert1 );
			if( cryptCert2 != CRYPT_UNUSED )
				cryptDestroyCert( cryptCert2 );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_FAILED );
			}
		cryptDestroySession( cryptSession );
		return( FALSE );
		}

	/* Obtain the response information */
	if( !isServer )
		{
		CRYPT_CERTIFICATE cryptOCSPResponse;
		
		/* Display the status information in the response */
		status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_RESPONSE,
									&cryptOCSPResponse );
		if( cryptStatusError( status ) )
			{
			printf( "cryptGetAttribute() failed with error code %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		printCertInfo( cryptOCSPResponse );

		/* Check each certificate against the response.  This is somewhat
		   redundant since the status has already been displayed by the code
		   above, but it tests the check-against-response functionality */
		status = cryptCheckCert( cryptCert1, cryptOCSPResponse );
		printf( "Check of certificate status against OCSP response reports "
				"status %d.\n", status );
		if( cryptCert2 != CRYPT_UNUSED )
			{
			status = cryptCheckCert( cryptCert2, cryptOCSPResponse );
			printf( "Check of second certificate status against OCSP "
					"response reports status %d.\n", status );
			}

		cryptDestroyCert( cryptOCSPResponse );
		cryptDestroyCert( cryptCert1 );
		if( cryptCert2 != CRYPT_UNUSED )
			cryptDestroyCert( cryptCert2 );
		}

	/* There are so many weird ways to delegate trust and signing authority
	   mentioned in the OCSP RFC without any indication of which one
	   implementors will follow that we can't really perform any sort of
	   automated check since every responder seems to interpret this
	   differently, and many require manual installation of responder certs
	   in order to function */
#if 0
	status = cryptCheckCert( cryptOCSPResponse , CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptOCSPResponse , "cryptCheckCert()",
							   status, __LINE__ ) );
#endif /* 0 */

	/* Clean up */
	status = cryptDestroySession( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroySession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	puts( isServer ? "SVR: OCSP server session succeeded.\n" : \
					 "OCSP client session succeeded.\n" );
	return( TRUE );
	}

static int connectOCSPDirect( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_SESSION cryptSession;
	int status;

	printf( "Testing direct OCSP query...\n" );

	/* Get the EE certificate */
	status = importCertFromTemplate( &cryptCert, OCSP_EEOK_FILE_TEMPLATE,
									 OCSP_SERVER_NO );
	if( cryptStatusError( status ) )
		{
		printf( "EE cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Create the OCSP session and add the server URL */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_OCSP );
	if( status == CRYPT_ERROR_PARAM3 )	/* OCSP session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
#ifdef OCSP_SERVER_NAME
	status = cryptSetAttributeString( cryptSession,
								CRYPT_SESSINFO_SERVER_NAME, OCSP_SERVER_NAME,
								paramStrlen( OCSP_SERVER_NAME ) );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptSession, "cryptSetAttributeString()",
							   status, __LINE__ ) );
#endif /* Kludges for incorrect/missing authorityInfoAccess values */

	/* Check the certificate directly against the server.  This check 
	   quantises the result into a basic pass/fail that doesn't provide as 
	   much detail as the low-level OCSP check, so it's not unusual to get
	   CRYPT_ERROR_INVALID whent he low-level check returns
	   CRYPT_OCSPSTATUS_UNKNOWN */
	status = cryptCheckCert( cryptCert, cryptSession );
	printf( "Certificate status check returned %d.\n", status );

	/* Clean up */
	cryptDestroyCert( cryptCert );
	cryptDestroySession( cryptSession );

	puts( "OCSP direct query succeeded.\n" );
	return( TRUE );
	}

int testSessionOCSP( void )
	{
	if( !connectOCSP( CRYPT_SESSION_OCSP, FALSE, FALSE, FALSE ) )
		return( FALSE );
	if( !connectOCSPDirect() )
		return( FALSE );
#if OCSP_SERVER_NO == 1
	if( !( connectOCSP( CRYPT_SESSION_OCSP, TRUE, FALSE, FALSE ) ) )
		return( FALSE );
	return( connectOCSP( CRYPT_SESSION_OCSP, FALSE, TRUE, FALSE ) );
#else
	return( TRUE );
#endif /* Server that has a revoked certificate */
	}
int testSessionOCSPServer( void )
	{
	return( connectOCSP( CRYPT_SESSION_OCSP_SERVER, FALSE, FALSE, FALSE ) );
	}

/* Perform a client/server loopback test */

#ifdef WINDOWS_THREADS

unsigned __stdcall ocspServerThread( void *dummy )
	{
	acquireMutex();
	connectOCSP( CRYPT_SESSION_OCSP_SERVER, FALSE, FALSE, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionOCSPClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server and wait for it to initialise */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, ocspServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectOCSP( CRYPT_SESSION_OCSP, FALSE, FALSE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

int testSessionOCSPMulticertClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server and wait for it to initialise */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, ocspServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectOCSP( CRYPT_SESSION_OCSP, FALSE, TRUE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}
#endif /* WINDOWS_THREADS */

/****************************************************************************
*																			*
*								TSP Routines Test							*
*																			*
****************************************************************************/

/* There are various test TSP servers running, the following remapping allows
   us to switch between them in the hope of finding at least one which is
   actually working.  Implementation peculiarities:

	#1 - cryptlib:
			None.
	#2 - Peter Sylvester
			Requires Host: header even for HTTP 1.0.
	#3 - Timeproof
			None (currently not active).
	#4 - Korea Mobile Payment Service
			Currently not active.
	#5 - IAIK Graz
			Never been seen active.
	#6 - Fst s.r.l.
			Returns garbled TCP-socket-protocol header.
	#7 - Datum
			Almost never active
	#8 - Chinese University of Hong Kong
			None, info at http://www.e-timestamping.com/status.html.
	#9 - SeMarket
			None.
	#10 - Entrust
			None.
	#11 - nCipher
			Very slow TSP, requires extended read timeout to get response.
	#12 - Comodo
			None.
	#13 - Verisign 
			This "TSA" doesn't support TSP but uses an AuthentiCode-specific 
			mechanism documented at 
			http://msdn.microsoft.com/en-us/library/windows/desktop/bb931395%28v=vs.85%29.aspx. 
			Submitting a TSP request returns the text message "error 
			handling request, status = 0x9300" 
	#14 - SecureSoft 
			None (but uses an invalid policy OID '1 2' in the response).
	#15 - OpenTSA 
			Currently not active, info at http://opentsa.org/#service 

   Note that this only tests the low-level raw TSP mechanism, timestamps are 
   usually used in conjunction with signed (enveloped) data, for which see 
   testSessionEnvTSP() */

#define TSP_SERVER1_NAME	TEXT( "localhost" )
#define TSP_SERVER2_NAME	TEXT( "http://timestamping.edelweb.fr/service/tsp" )
#define TSP_SERVER3_NAME	TEXT( "tcp://test.timeproof.de" )
#define TSP_SERVER4_NAME	TEXT( "tcp://203.238.37.132:3318" )
#define TSP_SERVER5_NAME	TEXT( "tcp://neurath.iaik.at" )
#define TSP_SERVER6_NAME	TEXT( "tcp://ricerca.fst.it" )
#define TSP_SERVER7_NAME	TEXT( "tcp://tssdemo2.datum.com" )
#define TSP_SERVER8_NAME	TEXT( "tcp://ts2.itsc.cuhk.edu.hk:3318" )
#define TSP_SERVER9_NAME	TEXT( "tcp://80.81.104.150" )
#define TSP_SERVER10_NAME	TEXT( "http://vsinterop.entrust.com:7001/verificationserver/rfc3161timestamp" )
#define TSP_SERVER11_NAME	TEXT( "tcp://dse200.ncipher.com" )
#define TSP_SERVER12_NAME	TEXT( "http://timestamp.comodoca.com/rfc3161" )
#define TSP_SERVER13_NAME	TEXT( "http://timestamp.verisign.com/scripts/timstamp.dll" )
#define TSP_SERVER14_NAME	TEXT( "http://ca.signfiles.com/TSAServer.aspx" )
#define TSP_SERVER15_NAME	TEXT( "http://ns.szikszi.hu:8080/tsa" )

#define TSP_SERVER_NAME		TSP_SERVER2_NAME
#define TSP_SERVER_NO		2	/* Only used to identify slow-timeout server #11 */

/* Perform a timestamping test */

static int testTSP( const CRYPT_SESSION cryptSession,
					const BOOLEAN isServer,
					const BOOLEAN isRecycledConnection,
					const BOOLEAN useAltHash,
					const BOOLEAN localSession )
	{
	int status;

	/* If we're the client, wait for the server to finish initialising */
	if( localSession && !isServer && waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n", 
				__LINE__ );
		return( FALSE );
		}

	/* If we're the client, create a message imprint to timestamp */
	if( !isServer )
		{
		CRYPT_CONTEXT hashContext;

		/* Create the hash value to add to the TSP request */
		status = cryptCreateContext( &hashContext, CRYPT_UNUSED, 
									 useAltHash ? CRYPT_ALGO_SHA256 : \
												  CRYPT_ALGO_SHA1 );
		if( cryptStatusError( status ) )
			return( FALSE );
		cryptEncrypt( hashContext, "12345678", 8 );
		cryptEncrypt( hashContext, "", 0 );
		if( isRecycledConnection )
			{
			/* If we're moving further data over an existing connection, 
			   delete the message imprint from the previous run */
			status = cryptDeleteAttribute( cryptSession,
										   CRYPT_SESSINFO_TSP_MSGIMPRINT );
			if( cryptStatusError( status ) )
				{
				printf( "cryptDeleteAttribute() failed with error code %d, "
						"line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			}
		status = cryptSetAttribute( cryptSession,
									CRYPT_SESSINFO_TSP_MSGIMPRINT,
									hashContext );
		if( cryptStatusError( status ) )
			{
			printf( "cryptSetAttribute() failed with error code %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		cryptDestroyContext( hashContext );

		/* If it's a local session, wait for the server to finish 
		   initialising */
		if( localSession && waitMutex() == CRYPT_ERROR_TIMEOUT )
			{
			printf( "Timed out waiting for server to initialise, line %d.\n",
					__LINE__ );
			return( FALSE );
			}		
		}
	else
		{
		/* We're the server, if this is the first connect tell the client 
		   that we're ready to go */
		if( localSession && !isRecycledConnection )
			releaseMutex();
		}

	/* Activate the session and timestamp the message */
#if TSP_SERVER_NO == 11
	cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_READTIMEOUT, 30 );
#endif /* Very slow TSP */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( isServer )
		printConnectInfo( cryptSession );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, isServer ? \
					   "SVR: Attempt to activate TSP server session" : \
					   "Attempt to activate TSP client session", status,
					   __LINE__ );
		if( !isServer && isServerDown( cryptSession, status ) )
			{
			puts( "  (Server could be down, faking it and continuing...)\n" );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_FAILED );
			}
		cryptDestroySession( cryptSession );
		return( FALSE );
		}

	/* There's not much more we can do in the client at this point since the 
	   TSP data is only used internally by cryptlib, OTOH if we get to here 
	   then we've received a valid response from the TSA so all is OK */
	if( !isServer )
		{
		CRYPT_ENVELOPE cryptEnvelope;
		BYTE buffer[ BUFFER_SIZE ];
		int bytesCopied;

		status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_RESPONSE,
									&cryptEnvelope );
		if( cryptStatusError( status ) )
			{
			printExtError( cryptSession, "Attempt to process returned "
						   "timestamp", status, __LINE__ );
			return( FALSE );
			}
		status = cryptPopData( cryptEnvelope, buffer, BUFFER_SIZE,
							   &bytesCopied );
		if( cryptStatusError( status ) )
			{
			printf( "cryptPopData() failed with error code %d, line %d.\n",
					status, __LINE__ );
			return( FALSE );
			}
		printf( "Timestamp data size = %d bytes.\n", bytesCopied );
		debugDump( "tstinfo", buffer, bytesCopied );
		cryptDestroyEnvelope( cryptEnvelope );
		}

	return( TRUE );
	}

static int connectTSP( const CRYPT_SESSION_TYPE sessionType,
					   const CRYPT_HANDLE externalCryptContext,
					   const BOOLEAN persistentConnection,
					   const BOOLEAN localSession )
	{
	CRYPT_SESSION cryptSession;
	const BOOLEAN isServer = ( sessionType == CRYPT_SESSION_TSP_SERVER ) ? \
							   TRUE : FALSE;
	const BOOLEAN useAltHash = ( !isServer && 0 ) ? TRUE : FALSE;
	int status;

	printf( "%sTesting %sTSP session...\n", isServer ? "SVR: " : "",
			localSession ? "local " : "" );

	/* Acquire the init mutex if we're the server */
	if( localSession && isServer && waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n",
				__LINE__ );
		return( FALSE );
		}		

	/* Create the TSP session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED, sessionType );
	if( status == CRYPT_ERROR_PARAM3 )	/* TSP session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "%scryptCreateSession() failed with error code %d, line "
				"%d.\n", isServer ? "SVR: " : "", status, __LINE__ );
		return( FALSE );
		}

	/* Set up the server information and activate the session.  Since this 
	   test explicitly tests the ability to handle persistent connections, 
	   we don't use the general-purpose request/response server wrapper, 
	   which only uses persistent connections opportunistically */
	if( isServer )
		{
		CRYPT_CONTEXT privateKey = externalCryptContext;

		if( !setLocalConnect( cryptSession, 318 ) )
			return( FALSE );
		if( externalCryptContext == CRYPT_UNUSED )
			status = getPrivateKey( &privateKey, TSA_PRIVKEY_FILE,
									USER_PRIVKEY_LABEL,
									TEST_PRIVKEY_PASSWORD );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_PRIVATEKEY, privateKey );
			if( externalCryptContext == CRYPT_UNUSED )
				cryptDestroyContext( privateKey );
			}
		}
	else
		{
		if( localSession )
			{
			if( !setLocalConnect( cryptSession, 318 ) )
				return( FALSE );
			}
		else
			{
			status = cryptSetAttributeString( cryptSession,
							CRYPT_SESSINFO_SERVER_NAME, TSP_SERVER_NAME,
							paramStrlen( TSP_SERVER_NAME ) );
			}
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute/cryptSetAttributeString() failed with "
				"error code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = testTSP( cryptSession, isServer, FALSE, useAltHash, localSession );
	if( status <= 0 )
		return( status );

	/* Check whether the session connection is still open */
	if( persistentConnection )
		{
		int connectionActive;

		status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_CONNECTIONACTIVE,
									&connectionActive );
		if( cryptStatusError( status ) || !connectionActive )
			{
			printExtError( cryptSession, isServer ? \
						   "SVR: Persistent connection has been closed, "
							"operation" : \
						   "Persistent connection has been closed, operation",
						   status, __LINE__ );
			return( FALSE );
			}

		/* Activate the connection to handle two more requests */
		status = testTSP( cryptSession, isServer, TRUE, FALSE, FALSE );
		if( status <= 0 )
			return( status );
		status = testTSP( cryptSession, isServer, TRUE, FALSE, FALSE );
		if( status <= 0 )
			return( status );
		}

	/* Clean up */
	status = cryptDestroySession( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroySession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	printf( isServer ? "SVR: %sTSP server session succeeded.\n\n" : \
					   "%sTSP client session succeeded.\n\n",
			persistentConnection ? "Persistent " : "" );
	return( TRUE );
	}

int testSessionTSP( void )
	{
	return( connectTSP( CRYPT_SESSION_TSP, CRYPT_UNUSED, FALSE, FALSE ) );
	}
int testSessionTSPServer( void )
	{
	return( connectTSP( CRYPT_SESSION_TSP_SERVER, CRYPT_UNUSED, FALSE, FALSE ) );
	}
int testSessionTSPServerEx( const CRYPT_CONTEXT privKeyContext )
	{
	return( connectTSP( CRYPT_SESSION_TSP_SERVER, privKeyContext, FALSE, FALSE ) );
	}

/* Perform a client/server loopback test */

#ifdef WINDOWS_THREADS

unsigned __stdcall tspServerThread( void *dummy )
	{
	acquireMutex();
	connectTSP( CRYPT_SESSION_TSP_SERVER, CRYPT_UNUSED, FALSE, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionTSPClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server and wait for it to initialise */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, tspServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectTSP( CRYPT_SESSION_TSP, CRYPT_UNUSED, FALSE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

unsigned __stdcall tspServerPersistentThread( void *dummy )
	{
	acquireMutex();
	connectTSP( CRYPT_SESSION_TSP_SERVER, CRYPT_UNUSED, TRUE, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionTSPClientServerPersistent( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server and wait for it to initialise */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, tspServerPersistentThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectTSP( CRYPT_SESSION_TSP, CRYPT_UNUSED, TRUE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}
#endif /* WINDOWS_THREADS */

#endif /* TEST_SESSION || TEST_SESSION_LOOPBACK */
