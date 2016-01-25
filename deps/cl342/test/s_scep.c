/****************************************************************************
*																			*
*					cryptlib SCEP Session Test Routines						*
*					Copyright Peter Gutmann 1998-2009						*
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

#if defined( TEST_SESSION ) || defined( TEST_SESSION_LOOPBACK )

/****************************************************************************
*																			*
*								SCEP Test Data								*
*																			*
****************************************************************************/

/* There were various SCEP test servers available at some point, although 
   most of them are either semi- or entirely nonfunctional or vanished years
   ago.  The following mappings can be used to test different ones.  
   Implementation peculiarities:

	#1 - cryptlib: None.

	#2 - SSH (www.ssh.com/support/testzone/pki.html): Invalid CA 
	     certificates, and disappeared some years ago.

	#3 - OpenSCEP (openscep.othello.ch): Seems to be permanently unavailable.

	#4 - Entrust (freecerts.entrust.com/vpncerts/cep.htm): Only seems to be
		 set up to handle Cisco gear.

	#5 - EJBCA: Server has vanished.
	
	#6 - Insta-Certifier: Information at 
		 http://www.certificate.fi/resources/demos/demo.htm.

	#7 - Microsoft NDES: Reverted to a fresh install with no content several 
		 years ago, but used to return an invalid CA certificate.  The code 
		 sets the compliance level to CRYPT_COMPLIANCELEVEL_OBLIVIOUS to 
		 handle this.  GetCACaps 
		 (via http://qa4-mdm3.qa4.imdmdemo.com/certsrv/mscep/?operation=GetCACaps) 
		 is implemented but broken, returns a zero-length response.
	
	#8 - Windows Server 2008: Also returns an invalid CA certificate 
		 (keyUsage is set to keyEncipherment but not digitalSignature).  The 
		 code sets the compliance level to CRYPT_COMPLIANCELEVEL_OBLIVIOUS 
		 to handle this */

#define SCEP_NO		1

typedef struct {
	const char FAR_BSS *name;
	const C_CHR FAR_BSS *url, FAR_BSS *user, FAR_BSS *password, FAR_BSS *caCertUrl;
	} SCEP_INFO;

static const SCEP_INFO FAR_BSS scepInfo[] = {
	{ NULL },	/* Dummy so index == SCEP_NO */
	{ /*1*/ "cryptlib", TEXT( "http://localhost/pkiclient.exe" ), NULL, NULL, 
			TEXT( "http://localhost/pkiclient.exe?operation=GetCACert&message=*" ) },
			/* The CA certificate URL may be overridden by passing a 
			   CA-cert-read flag to the SCEP client function, it's tested 
			   both with an explicitly-added certificate via a file and
			   an implicitly-read certificate via GetCACert */
	{ /*2*/ "SSH", TEXT( "http://pki.ssh.com:8080/scep/pkiclient.exe" ), 
			TEXT( "ssh" ), TEXT( "ssh" ),
			TEXT( "http://pki.ssh.com:8080/scep/pkiclient.exe?operation=GetCACert&message=test-ca1.ssh.com" ) },
	{ /*3*/ "OpenSCEP", TEXT( "http://openscep.othello.ch/pkiclient.exe" ), 
			TEXT( "????" ), TEXT( "????" ), NULL },
	{ /*4*/ "Entrust", TEXT( "http://vpncerts.entrust.com/pkiclient.exe" ), 
			TEXT( "????" ), TEXT( "????" ), NULL },
	{ /*5*/ "EJBCA", TEXT( "http://q-rl-xp:8080/ejbca/publicweb/apply/scep/pkiclient.exe" ),
			TEXT( "test2" ), TEXT( "test2" ),
			TEXT( "http://q-rl-xp:8080/ejbca/publicweb/webdist/certdist?cmd=nscacert&issuer=O=Test&+level=1" ) },
	{ /*6*/ "Insta-Certifier", TEXT( "http://pki.certificate.fi:8082/scep/" ), 
			TEXT( "user" ), TEXT( "scep" ), NULL },
	{ /*7*/ "Microsoft NDES", TEXT( "http://qa4-mdm3.qa4.imdmdemo.com" ), 
			TEXT( "cryptlibtest" ), TEXT( "password!1" ), 
			TEXT( "http://qa4-mdm3.qa4.imdmdemo.com/certsrv/mscep/?operation=GetCACert&message=qa" ) },
	{ /*8*/ "Win Server 2008", TEXT( "http://142.176.86.157" ), 
			TEXT( "cryptlibtest" ), TEXT( "password!1" ), 
			TEXT( "http://142.176.86.157/certsrv/mscep/?operation=GetCACert&message=qa" ) }
	};

/* SCEP requires that its CA certificates be usable for decryption of request
   messages and signing of response messages.  Some SCEP CAs don't have the
   appropriate keyUsage bits for this set, in which case we have to process 
   the certificates in oblivious mode in order to use them */

#if ( SCEP_NO == 6 ) || ( SCEP_NO == 7 ) || ( SCEP_NO == 8 ) 
  #define SCEP_BROKEN_CA_CERT
#endif /* Insta-Certifier, Windows Server 2008 */

/* Certificate request data for the certificate from the SCEP server.  Note 
   that we have to set the CN to the PKI user CN, for CMP ir's we just omit 
   the DN entirely and have the server provide it for us but since SCEP uses 
   PKCS #10 requests we need to provide a DN, and since we provide it it has 
   to match the PKI user DN */

static const CERT_DATA FAR_BSS scepRequestData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test SCEP PKI user" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

/* PKI user data to authorise the issuing of the various certs */

static const CERT_DATA FAR_BSS scepPkiUserData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test SCEP PKI user" ) },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Add a PKI user to the certificate store */

static int addPKIUser( const CRYPT_KEYSET cryptCertStore,
					   const CERT_DATA *pkiUserData,
					   const BOOLEAN isSCEP, 
					   const BOOLEAN testErrorChecking )
	{
	CRYPT_CERTIFICATE cryptPKIUser;
	CRYPT_SESSION cryptSession;
	C_CHR userID[ CRYPT_MAX_TEXTSIZE + 1 ], issuePW[ CRYPT_MAX_TEXTSIZE + 1 ];
	int length, status;

	puts( "-- Adding new PKI user information --" );

	/* Create the PKI user object and add the user's identification
	   information */
	status = cryptCreateCert( &cryptPKIUser, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_PKIUSER );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	if( !addCertFields( cryptPKIUser, pkiUserData, __LINE__ ) )
		return( FALSE );

	/* Add the user info to the certificate store */
	status = cryptCAAddItem( cryptCertStore, cryptPKIUser );
	if( status == CRYPT_ERROR_DUPLICATE )
		{
		C_CHR userIdentifier[ CRYPT_MAX_TEXTSIZE + 1 ];

		/* Get the name of the duplicate user.  Since this may be just a
		   template we fall back to higher-level DN components if there's
		   no CN present */
		status = cryptGetAttributeString( cryptPKIUser,
										  CRYPT_CERTINFO_COMMONNAME,
										  userIdentifier, &length );
		if( status == CRYPT_ERROR_NOTFOUND )
			status = cryptGetAttributeString( cryptPKIUser,
											  CRYPT_CERTINFO_ORGANISATIONALUNITNAME,
											  userIdentifier, &length );
		if( status == CRYPT_ERROR_NOTFOUND )
			status = cryptGetAttributeString( cryptPKIUser,
											  CRYPT_CERTINFO_ORGANISATIONNAME,
											  userIdentifier, &length );
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptPKIUser, "cryptGetAttribute()",
								   status, __LINE__ ) );
#ifdef UNICODE_STRINGS
		length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
		userIdentifier[ length ] = TEXT( '\0' );

		/* The PKI user info was already present, for SCEP this isn't a
		   problem since we can just re-use the existing info, but for CMP
		   we can only authorise a single certificate issue per user so we 
		   have to delete the existing user info and try again */
		if( isSCEP )
			{
			/* The PKI user info is already present from a previous run, get
			   the existing info */
			puts( "PKI user information is already present from a previous "
				  "run, reusing existing\n  PKI user data..." );
			cryptDestroyCert( cryptPKIUser );
			status = cryptCAGetItem( cryptCertStore, &cryptPKIUser,
									 CRYPT_CERTTYPE_PKIUSER, CRYPT_KEYID_NAME,
									 userIdentifier );
			}
		else
			{
			puts( "PKI user information is already present from a previous "
				  "run, deleting existing\n  PKI user data..." );
			status = cryptCADeleteItem( cryptCertStore, CRYPT_CERTTYPE_PKIUSER,
										CRYPT_KEYID_NAME, userIdentifier );
			if( cryptStatusError( status ) )
				return( extErrorExit( cryptCertStore, "cryptCADeleteItem()",
									  status, __LINE__ ) );
			status = cryptCAAddItem( cryptCertStore, cryptPKIUser );
			}
		}
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptCAAdd/GetItem()", status,
							  __LINE__ ) );

	/* Display the information for the new user */
	if( !printCertInfo( cryptPKIUser ) )
		return( FALSE );
	status = cryptGetAttributeString( cryptPKIUser,
									  CRYPT_CERTINFO_PKIUSER_ID,
									  userID, &length );
	if( cryptStatusOK( status ) )
		{
#ifdef UNICODE_STRINGS
		length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
		userID[ length ] = '\0';
		status = cryptGetAttributeString( cryptPKIUser,
									CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD,
									issuePW, &length );
		}
	if( cryptStatusOK( status ) )
		{
#ifdef UNICODE_STRINGS
		length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
		issuePW[ length ] = '\0';
		}
	else
		{
		return( attrErrorExit( cryptPKIUser, "cryptGetAttribute()", status,
							   __LINE__ ) );
		}
	puts( "-- New PKI user information ends --\n" );

	/* If we're not testing the error-checking capability of the user
	   identifiers, we're done */
	if( !testErrorChecking )
		{
		cryptDestroyCert( cryptPKIUser );
		return( TRUE );
		}

	/* Make sure that the error-checking in the user information works via a
	   dummy CMP client session.  We have to check both passwords to reduce 
	   false positives since it's just a simple integrity check meant to 
	   catch typing errors rather than a cryptographically strong check */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED, 
								 CRYPT_SESSION_CMP );
	if( cryptStatusError( status ) )
		return( FALSE );
	if( userID[ 2 ] >= TEXT( 'A' ) && userID[ 2 ] < TEXT( 'Z' ) )
		userID[ 2 ]++;
	else
		userID[ 2 ] = TEXT( 'A' );
	if( issuePW[ 8 ] >= TEXT( 'A' ) && issuePW[ 8 ] < TEXT( 'Z' ) )
		issuePW[ 8 ]++;
	else
		issuePW[ 8 ] = TEXT( 'A' );
	status = cryptSetAttributeString( cryptSession, CRYPT_SESSINFO_USERNAME,
									  userID, paramStrlen( userID ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession, CRYPT_SESSINFO_PASSWORD,
										  issuePW, paramStrlen( issuePW ) );
	if( cryptStatusOK( status ) )
		{
		puts( "Integrity check of user ID and password failed to catch "
			  "errors in the data.\n(This check isn't foolproof and is "
			  "intended only to catch typing errors when\nentering the "
			  "data.  Try running the test again to see if the problem "
			  "still\noccurs)." );
		return( FALSE );
		}
	cryptDestroySession( cryptSession );

	/* Clean up */
	cryptDestroyCert( cryptPKIUser );
	return( TRUE );
	}

/* Get information on a PKI user */

int pkiGetUserInfo( C_STR userID, C_STR issuePW, C_STR revPW, 
					const C_STR userName )
	{
	CRYPT_KEYSET cryptCertStore;
	CRYPT_CERTIFICATE cryptPKIUser;
	int length, status;

	/* cryptlib implements per-user (rather than shared interop) IDs and
	   passwords so we need to read the user ID and password information
	   before we can perform any operations.  First we get the PkiUser
	   object */
	status = cryptKeysetOpen( &cryptCertStore, CRYPT_UNUSED,
							  CERTSTORE_KEYSET_TYPE, CERTSTORE_KEYSET_NAME,
							  CRYPT_KEYOPT_READONLY );
	if( status == CRYPT_ERROR_PARAM3 )
		{
		/* This type of keyset access isn't available, return a special error
		   code to indicate that the test wasn't performed, but that this
		   isn't a reason to abort processing */
		puts( "No certificate store available, aborting CMP/SCEP test.\n" );
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetOpen() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCAGetItem( cryptCertStore, &cryptPKIUser,
							 CRYPT_CERTTYPE_PKIUSER, CRYPT_KEYID_NAME,
							 userName );
	cryptKeysetClose( cryptCertStore );
	if( cryptStatusError( status ) )
		{
		/* Only report error info if it's not a basic presence check */
		if( userID != NULL )
			extErrorExit( cryptCertStore, "cryptCAGetItem()", status, __LINE__ );
		return( FALSE );
		}

	/* If it's a presence check only, we're done */
	if( userID == NULL )
		{
		cryptDestroyCert( cryptPKIUser );
		return( TRUE );
		}

	/* Then we extract the information from the PkiUser object */
	status = cryptGetAttributeString( cryptPKIUser,
									  CRYPT_CERTINFO_PKIUSER_ID,
									  userID, &length );
	if( cryptStatusOK( status ) )
		{
		userID[ length ] = '\0';
		status = cryptGetAttributeString( cryptPKIUser,
									CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD,
									issuePW, &length );
		}
	if( cryptStatusOK( status ) )
		issuePW[ length ] = '\0';
	if( cryptStatusOK( status ) && revPW != NULL )
		{
		status = cryptGetAttributeString( cryptPKIUser,
									CRYPT_CERTINFO_PKIUSER_REVPASSWORD,
									revPW, &length );
		if( cryptStatusOK( status ) )
			revPW[ length ] = '\0';
		}
	cryptDestroyCert( cryptPKIUser );
	if( cryptStatusError( status ) )
		{
		attrErrorExit( cryptPKIUser, "cryptGetAttribute()", status,
					   __LINE__ );
		return( FALSE );
		}

	/* We've got what we need, tell the user what we're doing */
	printf( "Using user name %s, password %s.\n", userID, issuePW );
	return( TRUE );
	}

/* Set up objects and information needed by a server-side PKI session */

int pkiServerInit( CRYPT_CONTEXT *cryptPrivateKey, 
				   CRYPT_KEYSET *cryptCertStore, const C_STR keyFileName,
				   const C_STR keyLabel, const CERT_DATA *pkiUserData,
				   const CERT_DATA *pkiUserAltData, 
				   const CERT_DATA *pkiUserCAData, const char *protocolName )
	{
	const BOOLEAN isSCEP = !strcmp( protocolName, "SCEP" ) ? TRUE : FALSE;
	int status;

	/* Get the certificate store to use with the session.  Before we use the 
	   store we perform a cleanup action to remove any leftover requests from
	   previous runs */
	status = cryptKeysetOpen( cryptCertStore, CRYPT_UNUSED,
							  CERTSTORE_KEYSET_TYPE, CERTSTORE_KEYSET_NAME,
							  CRYPT_KEYOPT_CREATE );
	if( status == CRYPT_ERROR_PARAM3 )
		{
		/* This type of keyset access isn't available, return a special error
		   code to indicate that the test wasn't performed, but that this
		   isn't a reason to abort processing */
		printf( "SVR: No certificate store available, aborting %s server "
				"test.\n\n", protocolName );
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( status == CRYPT_ERROR_DUPLICATE )
		status = cryptKeysetOpen( cryptCertStore, CRYPT_UNUSED,
								  CERTSTORE_KEYSET_TYPE, CERTSTORE_KEYSET_NAME,
								  CRYPT_KEYOPT_NONE );
	if( cryptStatusError( status ) )
		{
		printf( "SVR: cryptKeysetOpen() failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptCACertManagement( NULL, CRYPT_CERTACTION_CLEANUP, 
									*cryptCertStore, CRYPT_UNUSED, 
									CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		{
		printf( "SVR: CA certificate store cleanup failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Create the EE and CA PKI users */
	puts( "Creating PKI user..." );
	if( !addPKIUser( *cryptCertStore, pkiUserData, isSCEP, !isSCEP ) )
		return( FALSE );
	if( pkiUserAltData != NULL && \
		!addPKIUser( *cryptCertStore, pkiUserAltData, isSCEP, FALSE ) )
		return( FALSE );
	if( pkiUserCAData != NULL && \
		!addPKIUser( *cryptCertStore, pkiUserCAData, isSCEP, FALSE ) )
		return( FALSE );

	/* Get the CA's private key */
	status = getPrivateKey( cryptPrivateKey, keyFileName,
							keyLabel, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "SVR: CA private key read failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*								SCEP Routines Test							*
*																			*
****************************************************************************/

/* Get an SCEP CA certificate */

static int getScepCACert( const C_STR caCertUrl,
						  CRYPT_CERTIFICATE *cryptCACert )
	{
	CRYPT_KEYSET cryptKeyset;
	int status;

	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_HTTP,
							  caCertUrl, CRYPT_KEYOPT_READONLY );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetPublicKey( cryptKeyset, cryptCACert, CRYPT_KEYID_NAME,
									TEXT( "[None]" ) );
		cryptKeysetClose( cryptKeyset );
		}
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptKeyset, "cryptGetPublicKey()",
							  status, __LINE__ ) );

	return( TRUE );
	}

/* Perform an SCEP test */

static int connectSCEP( const BOOLEAN localSession,
						const BOOLEAN userSuppliesCACert )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_CERTIFICATE cryptRequest = DUMMY_INIT, cryptResponse, cryptCACert;
	CRYPT_CONTEXT cryptContext;
#if ( SCEP_NO == 1 )
	C_CHR userID[ 64 ], password[ 64 ];
	const C_STR userPtr;
	const C_STR passwordPtr;
#else
	const C_STR userPtr = scepInfo[ SCEP_NO ].user;
	const C_STR passwordPtr = scepInfo[ SCEP_NO ].password;
#endif /* cryptlib SCEP server */
#ifdef SCEP_BROKEN_CA_CERT
	int complianceValue;
#endif /* SCEP servers with broken CA certificates */
	int status;

	printf( "Testing %s SCEP session%s...\n", scepInfo[ SCEP_NO ].name,
			userSuppliesCACert ? "" : " with CA certificate read" );

	/* Wait for the server to finish initialising */
	if( localSession && waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n", 
				__LINE__ );
		return( FALSE );
		}

#if ( SCEP_NO == 1 )
	/* If we're doing a loopback test, make sure that the required user info
	   is present.  If it isn't, the CA auditing will detect a request from
	   a nonexistant user and refuse to issue a certificate */
	if( !pkiGetUserInfo( NULL, NULL, NULL, TEXT( "Test SCEP PKI user" ) ) )
		{
		puts( "CA certificate store doesn't contain the PKI user "
			  "information needed to\nauthenticate certificate issue "
			  "operations, can't perform SCEP test.\n" );
		return( CRYPT_ERROR_NOTAVAIL );
		}
#endif /* cryptlib SCEP server */

#ifdef SCEP_BROKEN_CA_CERT
	/* Some SCEP server's CA certificates are broken so we have to turn 
	   down the compliance level to allow them to be used */
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   &complianceValue );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   CRYPT_COMPLIANCELEVEL_OBLIVIOUS );
#endif /* SCEP servers with broken CA certificates */

	/* Get the issuing CA's certificate if required */
	if( userSuppliesCACert )
		{
		if( scepInfo[ SCEP_NO ].caCertUrl != NULL )
			{
			if( !getScepCACert( scepInfo[ SCEP_NO ].caCertUrl, 
								&cryptCACert ) )
				return( FALSE );
			}
		else
			{
			status = importCertFromTemplate( &cryptCACert, 
											 SCEP_CA_FILE_TEMPLATE, SCEP_NO );
			if( cryptStatusError( status ) )
				{
				printf( "Couldn't get SCEP CA certificate, status = %d, "
						"line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			}
		}

	/* cryptlib implements per-user (rather than shared interop) IDs and
	   passwords so we need to read the user ID and password information
	   before we can perform any operations */
#if ( SCEP_NO == 1 )
	status = pkiGetUserInfo( userID, password, NULL,
							 TEXT( "Test SCEP PKI user" ) );
	if( !status || status == CRYPT_ERROR_NOTAVAIL )
		{
		if( userSuppliesCACert )
			cryptDestroyCert( cryptCACert );

		/* If certificate store operations aren't available, exit but 
		   continue with other tests, otherwise abort the tests */
		return( ( status == CRYPT_ERROR_NOTAVAIL ) ? TRUE : FALSE );
		}
	userPtr = userID;
	passwordPtr = password;
#endif /* cryptlib SCEP server */

	/* Create the SCEP session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_SCEP );
	if( status == CRYPT_ERROR_PARAM3 )	/* SCEP session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that the SCEP client's checking for invalid transactionIDs
	   works (this is an obscure problem caused by the SCEP protocol, see 
	   the SCEP client code comments for more information) */
	status = cryptSetAttributeString( cryptSession,
									  CRYPT_SESSINFO_USERNAME,
									  TEXT( "abc@def" ), 
									  paramStrlen( TEXT( "abc@def" ) ) );
	if( cryptStatusOK( status ) )
		{
		printf( "Addition of invalid SCEP user information wasn't detected, "
				"line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Set up the user and server information */
	status = cryptSetAttributeString( cryptSession,
									  CRYPT_SESSINFO_USERNAME,
									  userPtr, paramStrlen( userPtr ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_PASSWORD,
										  passwordPtr, paramStrlen( passwordPtr ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
									CRYPT_SESSINFO_SERVER_NAME,
									scepInfo[ SCEP_NO ].url,
									paramStrlen( scepInfo[ SCEP_NO ].url ) );
	if( userSuppliesCACert )
		{
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_CACERTIFICATE,
										cryptCACert );
			}
		cryptDestroyCert( cryptCACert );
		}
	if( cryptStatusError( status ) )
		{
		printf( "Addition of SCEP user/server information failed with error "
				"code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Create the (unsigned) PKCS #10 request */
#if ( SCEP_NO == 1 )
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 CRYPT_ALGO_RSA );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_LABEL,
							 USER_PRIVKEY_LABEL,
							 paramStrlen( USER_PRIVKEY_LABEL ) );
	status = cryptGenerateKey( cryptContext );
	if( cryptStatusOK( status ) )
#else
	loadRSAContextsEx( CRYPT_UNUSED, NULL, &cryptContext, NULL,
					   USER_PRIVKEY_LABEL, FALSE, FALSE );
#endif /* cryptlib SCEP server */
	status = cryptCreateCert( &cryptRequest, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTREQUEST );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptRequest,
							CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cryptContext );
	if( cryptStatusOK( status ) && \
		!addCertFields( cryptRequest, scepRequestData, __LINE__ ) )
		status = CRYPT_ERROR_FAILED;
#if 0
	if( cryptStatusOK( status ) )
		status = cryptSignCert( cryptRequest, cryptContext );
#endif
	if( cryptStatusError( status ) )
		{
		printf( "Creation of PKCS #10 request failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Set up the private key and request */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_PRIVATEKEY,
								cryptContext );
	cryptDestroyContext( cryptContext );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_REQUEST,
									cryptRequest );
	cryptDestroyCert( cryptRequest );
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* If we've explicitly fetched the issuing CA's certificate via an HTTP
	   request then the server session has already been run, so we need to
	   wait for it to recycle before we continue */
	if( localSession && userSuppliesCACert && \
		scepInfo[ SCEP_NO ].caCertUrl != NULL && \
		waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n", 
				__LINE__ );
		return( FALSE );
		}

	/* Activate the session */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( status == CRYPT_ENVELOPE_RESOURCE )
		{
		/* The server has indicated that the certificate-issue operation is 
		   pending, try again in case it works this time */
		printExtError( cryptSession, "Attempt to activate SCEP client "
					   "session", status, __LINE__ );
		puts( "  (Retrying operation in case certificate issue is now "
			  "approved...)\n" );
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, 
									TRUE );
		}
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "Attempt to activate SCEP client "
					   "session", status, __LINE__ );
		if( isServerDown( cryptSession, status ) )
			{
			puts( "  (Server could be down, faking it and continuing...)\n" );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_FAILED );
			}
		cryptDestroySession( cryptSession );
		return( FALSE );
		}

#ifdef SCEP_BROKEN_CA_CERT
	/* Restore normal certificate checking */
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   complianceValue );
#endif /* SCEP servers with broken CA certificates */

	/* Print the session security information */
	printFingerprint( cryptSession, FALSE );

	/* Obtain the response information */
	status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_RESPONSE,
								&cryptResponse );
	if( cryptStatusOK( status ) && !userSuppliesCACert )
		status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_RESPONSE,
									&cryptCACert );
	cryptDestroySession( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "cryptGetAttribute() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
#if ( SCEP_NO != 1 )
	puts( "Returned certificate details are:" );
	printCertInfo( cryptResponse );
	if( !userSuppliesCACert )
		{
		puts( "Returned CA certificate details are:" );
		printCertInfo( cryptCACert );
		}
#endif /* Keep the cryptlib results on one screen */

	/* Clean up */
	cryptDestroyCert( cryptResponse );
	if( !userSuppliesCACert )
		cryptDestroyCert( cryptCACert );
	puts( "SCEP client session succeeded.\n" );
	return( TRUE );
	}

int testSessionSCEP( void )
	{
	return( connectSCEP( FALSE, TRUE ) );
	}

int testSessionSCEPCACert( void )
	{
	return( connectSCEP( FALSE, FALSE ) );
	}

enum { MUTEX_NONE, MUTEX_ACQUIRE, MUTEX_ACQUIRE_REACQUIRE };

static int scepServer( const int mutexBehaviour )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_CONTEXT cryptCAKey;
	CRYPT_KEYSET cryptCertStore;
	int status;

	/* Acquire the init mutex */
	if( mutexBehaviour == MUTEX_ACQUIRE || \
		mutexBehaviour == MUTEX_ACQUIRE_REACQUIRE )
		acquireMutex();

	printf( "SVR: Testing SCEP server session%s...\n",
			( mutexBehaviour == MUTEX_ACQUIRE_REACQUIRE ) ? \
				" (GetCACert portion)" : \
			( mutexBehaviour == MUTEX_NONE ) ? \
				" following GetCACert" : "" );

	/* Perform a test create of a SCEP server session to verify that we can
	   do this test */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_SCEP_SERVER );
	if( status == CRYPT_ERROR_PARAM3 )	/* SCEP session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "SVR: cryptCreateSession() failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	cryptDestroySession( cryptSession );

	/* Set up the server-side objects */
	if( !pkiServerInit( &cryptCAKey, &cryptCertStore, SCEPCA_PRIVKEY_FILE,
						USER_PRIVKEY_LABEL, scepPkiUserData, NULL, NULL, 
						"SCEP" ) )
		return( FALSE );

	/* Create the SCEP session and add the CA key and certificate store */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_SCEP_SERVER );
	if( cryptStatusError( status ) )
		{
		printf( "SVR: cryptCreateSession() failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_PRIVATEKEY, cryptCAKey );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_KEYSET, cryptCertStore );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptSession, "SVR: cryptSetAttribute()",
							   status, __LINE__ ) );

	/* Tell the client that we're ready to go */
	releaseMutex();

	/* Activate the session */
	status = activatePersistentServerSession( cryptSession, FALSE );
	if( cryptStatusError( status ) )
		{
		cryptKeysetClose( cryptCertStore );
		cryptDestroyContext( cryptCAKey );
		return( extErrorExit( cryptSession, "SVR: Attempt to activate SCEP "
							  "server session", status, __LINE__ ) );
		}

	/* If we're running a second server session, reacquire the mutex for
	   the client to wait on */
	if( mutexBehaviour == MUTEX_ACQUIRE_REACQUIRE )
		acquireMutex();

	/* Clean up */
	cryptDestroySession( cryptSession );
	cryptKeysetClose( cryptCertStore );
	cryptDestroyContext( cryptCAKey );

	if( mutexBehaviour == MUTEX_ACQUIRE_REACQUIRE )
		puts( "SVR: SCEP session (GetCACert portion) succeeded." );
	else
		puts( "SVR: SCEP session succeeded.\n" );
	return( TRUE );
	}

int testSessionSCEPServer( void )
	{
	int status;

	createMutex();
	status = scepServer( MUTEX_ACQUIRE );
	destroyMutex();

	return( status );
	}

/* Perform a client/server loopback test */

#ifdef WINDOWS_THREADS

unsigned __stdcall scepServerThread( void *dummy )
	{
	/* Since we're doing a GetCACert before the main SCEP session we have to
	   run the server twice */
	scepServer( MUTEX_ACQUIRE_REACQUIRE );
	scepServer( MUTEX_NONE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionSCEPClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

#if ( SCEP_NO != 1 )
	/* Because the code has to handle so many CA-specific peculiarities, we
	   can only perform this test when the CA being used is the cryptlib
	   CA */
	puts( "Error: The local SCEP session test only works with SCEP_NO == 1." );
	return( FALSE );
#endif /* cryptlib CA */

	/* Start the server and wait for it to initialise */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, scepServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectSCEP( TRUE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

int testSessionSCEPSHA2ClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int value, status;

#if ( SCEP_NO != 1 )
	/* Because the code has to handle so many CA-specific peculiarities, we
	   can only perform this test when the CA being used is the cryptlib
	   CA */
	puts( "Error: The local SCEP session test only works with SCEP_NO == 1." );
	return( FALSE );
#endif /* cryptlib CA */

	/* Switch the hash algorithm to SHA-2 */
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH, &value );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH,
					   CRYPT_ALGO_SHA2 );

	/* Start the server and wait for it to initialise */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, scepServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectSCEP( TRUE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH, value );

	return( status );
	}

unsigned __stdcall scepServerCACertThread( void *dummy )
	{
	scepServer( MUTEX_ACQUIRE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionSCEPCACertClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

#if ( SCEP_NO != 1 )
	/* Because the code has to handle so many CA-specific peculiarities, we
	   can only perform this test when the CA being used is the cryptlib
	   CA */
	puts( "Error: The local SCEP session test only works with SCEP_NO == 1." );
	return( FALSE );
#endif /* cryptlib CA */

	/* Start the server and wait for it to initialise */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, scepServerCACertThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectSCEP( TRUE, FALSE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}
#endif /* WINDOWS_THREADS */

#endif /* TEST_SESSION || TEST_SESSION_LOOPBACK */
