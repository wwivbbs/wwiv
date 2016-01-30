/****************************************************************************
*																			*
*						cryptlib CMP Session Test Routines					*
*						Copyright Peter Gutmann 1998-2009					*
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

/* If we're running the test with a crypto device, we have to set an extended
   timeout because of the long time it takes many devices to generate keys */

#define NET_TIMEOUT		180

#if defined( TEST_SESSION ) || defined( TEST_SESSION_LOOPBACK )

/****************************************************************************
*																			*
*								CMP Test Data								*
*																			*
****************************************************************************/

/* There are various CMP test CAs available, the following mappings can be
   used to test different ones.  Implementation peculiarities:

	#1 - cryptlib: Implicitly revokes certificate being replaced during a 
			kur (this is a requirement for maintaining certificate store 
			consistency).
			Tested: ir, cr/kur, rr
	#2 - cryptlib with PKIBoot/PnP PKI functionality, otherwise as for #1.
	#3 - Certicom: Requires signature for revocation rather than MAC,
			requires that all certs created after the ir one have the same
			DN as the ir certificate.
			Tested: ir, cr/kur, rr
	#4 - ssh old: None (recently re-issued their CA certificate which is 
			broken, CA couldn't be re-tested.  In addition since CMP 
			identifies the sender by DN the new certificate can't be 
			distinguished from the old one, causing all sig checks to 
			fail).
			Tested (late 2000): ir, cr/kur, rr
	#5 - ssh new:
	#6 - Entrust: Won't allow altNames, changes sender and request DN,
			returns rejected response under an altered DN belonging to a
			completely different EE for anything but ir.
			Tested: ir
	#7 - Trustcenter: Requires HTTPS and pre-existing trusted private key
			distributed as PKCS #12 file, couldn't be tested.
	#8 - Baltimore: Server unavailable for testing.
			Tested: -
	#9 - Initech: Needs DN cn=CryptLIB EE 1,o=INITECH,c=KR.
			Tested: ir, cr/kur, rr
	#10 - RSA labs: Rejects signed requests, couldn't be tested beyond initial
			(MACd) ir.  Attempt to revoke newly-issued certificate with MACd 
			rr returns error indicating that the certificate is already 
			revoked.
			Tested: ir
	#11 - Cylink: Invalid CA root certificate, requires use of DN from RA 
			rather than CA when communicating with server.
			Tested: - 
	#12 - Insta-Certifier: Returned garbled bits of the "tcp" protocol 
			header in earlier versions, later versions improved somewhat,
			however a kur is rejected with the message "CRMF did not contain 
			oldCertId control".  Information at 
			http://www.certificate.fi/resources/demos/demo.htm.
			Tested: ir, cr */

#define CA_CRYPTLIB				1
#define CA_CRYPTLIB_PNPPKI		2

#define CA_NO					CA_CRYPTLIB

typedef struct {
	const char FAR_BSS *name;
	const C_CHR FAR_BSS *url, FAR_BSS *user, FAR_BSS *password, FAR_BSS *revPassword;
	} CA_INFO;

static const CA_INFO FAR_BSS caInfoTbl[] = {
	{ NULL },	/* Dummy so index == CA_NO */
	{ /*1*/ "cryptlib", TEXT( "http://localhost" ), TEXT( "interop" ), TEXT( "interop" ) },
	{ /*2*/	"cryptlib/PKIBoot", /*"_pkiboot._tcp.cryptoapps.com"*/TEXT( "http://localhost" ), TEXT( "interop" ), TEXT( "interop" ) },
	{ /*3*/ "Certicom", TEXT( "cmp://gandalf.trustpoint.com:8081" ), TEXT( "interop" ), TEXT( "interop" ) },
	{ /*4*/ "ssh", TEXT( "cmp://interop-ca.ssh.com:8290" ), TEXT( "123456" ), TEXT( "interop" ) },
	{ /*5*/ "ssh", TEXT( "http://pki.ssh.com:8080/pkix/" ), TEXT( "62154" ), TEXT( "ssh" ) },
	{ /*6*/ "Entrust", TEXT( "cmp://204.101.128.45:829" ), TEXT( "39141091" ), TEXT( "ABCDEFGHIJK" ) },
	{ /*7*/ "Trustcenter", TEXT( "cmp://demo.trustcenter.de/cgi-bin/cmp:829" ), TEXT( "interop" ), TEXT( "interop" ) },
	{ /*8*/ "Baltimore", TEXT( "cmp://hip.baltimore.ie:8290" ), TEXT( "pgutmann" ), TEXT( "the-magical-land-near-oz" ) },
	{ /*9*/ "Initech", TEXT( "cmp://61.74.133.49:8290" ), TEXT( "interop" ), TEXT( "interop" ) },
	{ /*A*/ "RSA", TEXT( "cmp://ca1.kcspilot.com:32829" ), TEXT( "interop" ), TEXT( "interop" ) },
	{ /*B*/ "Cylink", TEXT( "cmp://216.252.217.227:8082" ), TEXT( "3986" ), TEXT( "11002" ) /* "3987", "6711" */ },
	{ /*C*/ "Insta-Certifier", TEXT( "http://pki.certificate.fi:8700/pkix/" ), TEXT( "3078" ), TEXT( "insta" ) }
	};

/* Enable additional tests if we're using cryptlib as the server */

#if ( CA_NO == CA_CRYPTLIB ) || ( CA_NO == CA_CRYPTLIB_PNPPKI )
  #define SERVER_IS_CRYPTLIB
#endif /* Extra tests for cryptib CA */

/* Define the following to work around CA bugs/quirks */

#if ( CA_NO == 3 )			/* Certicom */
  #define SERVER_IR_DN
#endif /* CA that requires same DN in cr as ir */
#if ( CA_NO == 6 )			/* Entrust */
  #define SERVER_NO_ALTNAMES
#endif /* CAs that won't allow altNames in requests */
#if ( CA_NO == 9 )			/* Initech */
  #define SERVER_FIXED_DN
#endif /* CAs that require a fixed DN in requests */

/* The following defines can be used to selectively enable or disable some
   of the test (for example to do an ir + rr, or ir + kur + rr) */

#ifdef SERVER_IS_CRYPTLIB
  #define TEST_IR_NODN
  #define TEST_IR
/*#define TEST_DUP_IR */
  #define TEST_KUR
  #define TEST_CR
  #define TEST_RR

  /* Set the following to FALSE to use a MAC'd rr instead of a signed one */
  #define USE_SIGNED_RR		TRUE

  /* 4 certificate reqs (two ir variants), 1 rev.req (kur = impl.rev).  
	 
	 The duplicate-ir check is currently disabled because it's enforced via 
	 database transaction constraints, which means that once the initial ir 
	 has been recorded all further issue operations with the same ID are 
	 excluded by the presence of the ID for the ir.  This is a strong 
	 guarantee that subsequent requests with the same ID will be disallowed, 
	 but not terribly useful for self-test purposes */
  #define NO_CA_REQUESTS	( 5 + 0 )
#else
  #define TEST_IR
  #define TEST_KUR
  #define TEST_CR
  #define TEST_RR

  /* Loopback test requires SERVER_IS_CRYPTLIB */
  #define NO_CA_REQUESTS	0
#endif /* SERVER_IS_CRYPTLIB */

/* Define the following to enable testing of servers where the initial DN 
   (and optional additional information like the altName) is supplied by the 
   server (i.e. the user supplies a null DN) */

#ifdef SERVER_IS_CRYPTLIB
  #define SERVER_PROVIDES_DN
#endif /* CAs where the server provides the DN */

/* PKI user data to authorise the issuing of the various certificates */

static const CERT_DATA FAR_BSS cmpPkiUserFullDNData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test PKI user" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS cmpPkiUserPartialDNData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS cmpPkiUserCaData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test CA PKI user" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@ca.wetas-r-us.com" ) },

	/* CA extensions */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC,
	  CRYPT_KEYUSAGE_KEYCERTSIGN | CRYPT_KEYUSAGE_CRLSIGN },
	{ CRYPT_CERTINFO_CA, IS_NUMERIC, TRUE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

/* Certificate request data for the various types of certificates that the 
   cryptlib CMP CA can return */

static const CERT_DATA FAR_BSS cmpCryptlibRequestNoDNData[] = {	/* For ir */
	/* Identification information - none, it's provided by the server */

	/* Subject altName - none, it's provided by the server */

	/* Signature-only key */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC, CRYPT_KEYUSAGE_DIGITALSIGNATURE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS cmpCryptlibRequestData[] = {		/* For cr */
	/* Identification information.  The rest of the DN is provided by the 
	   CA.  Note that the ability to handle full DNs in the request is
	   tested by the kur, whose contents aren't defined here since they're
	   taken from the certificate being updated */
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave's Signature Key" ) },

	/* Subject altName.  The email address is provided by the CA */
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* Signature-only key */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC, CRYPT_KEYUSAGE_DIGITALSIGNATURE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS cmpCryptlibDsaRequestData[] = {	/* Unused */
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave's DSA Key" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

/* Certificate request data for the various types of certs that a CMP CA can 
   return */

static const CERT_DATA FAR_BSS cmpRsaSignRequestData[] = {		/* For cr */
	/* Identification information */
  #ifdef SERVER_FIXED_DN
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "KR" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "INITECH" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "CryptLIB EE 1" ) },
  #else
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave's Signature Key" ) },
  #endif /* CAs that require a fixed DN in requests */

	/* Subject altName */
#ifndef SERVER_NO_ALTNAMES
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },
#endif /* CAs that won't allow altNames in requests */

	/* Signature-only key */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC, CRYPT_KEYUSAGE_DIGITALSIGNATURE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS cmpRsaEncryptRequestData[] = {	/* Unused */
	/* Identification information */
#ifdef SERVER_FIXED_DN
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "KR" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "INITECH" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "CryptLIB EE 1" ) },
#else
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave's Encryption Key" ) },
#endif /* CAs that require a fixed DN in requests */

	/* Subject altName */
#ifndef SERVER_NO_ALTNAMES
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },
#endif /* CAs that won't allow altNames in requests */

	/* Encryption-only key */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC, CRYPT_KEYUSAGE_KEYENCIPHERMENT },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS cmpRsaCaRequestData[] = {		/* For ir-CA */
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave's Intermediate CA Key" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave-ca@wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* CA key */
	{ CRYPT_CERTINFO_CA, IS_NUMERIC, TRUE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Create various CMP objects */

static int createCmpNewKeyRequest( const CERT_DATA *requestData,
								   const CRYPT_ALGO_TYPE cryptAlgo,
								   const BOOLEAN useFixedKey,
								   const CRYPT_KEYSET cryptKeyset )
	{
	CRYPT_CERTIFICATE cryptRequest;
	CRYPT_CONTEXT cryptContext;
	int status;

	/* It's a new request, generate a private key and create a self-signed 
	   request */
	if( useFixedKey )
		{
		/* Use a fixed private key, for testing purposes */
		if( cryptAlgo == CRYPT_ALGO_RSA )
			loadRSAContextsEx( CRYPT_UNUSED, NULL, &cryptContext, NULL,
							   USER_PRIVKEY_LABEL, FALSE, FALSE );
		else
			loadDSAContextsEx( CRYPT_UNUSED, NULL, &cryptContext, NULL, 
							   USER_PRIVKEY_LABEL );
		status = CRYPT_OK;
		}
	else
		{
		status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
									 cryptAlgo );
		if( cryptStatusError( status ) )
			return( FALSE );
		cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_LABEL,
								 USER_PRIVKEY_LABEL,
								 paramStrlen( USER_PRIVKEY_LABEL ) );
		status = cryptGenerateKey( cryptContext );
		}
	if( cryptStatusOK( status ) )
		status = cryptCreateCert( &cryptRequest, CRYPT_UNUSED,
								  CRYPT_CERTTYPE_REQUEST_CERT );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptRequest,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cryptContext );
	if( cryptStatusOK( status ) && \
		!addCertFields( cryptRequest, requestData, __LINE__ ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusOK( status ) )
		status = cryptSignCert( cryptRequest, cryptContext );
	if( cryptStatusOK( status ) && cryptKeyset != CRYPT_UNUSED )
		{
		if( cryptStatusError( \
				cryptAddPrivateKey( cryptKeyset, cryptContext,
									TEST_PRIVKEY_PASSWORD ) ) )
			return( FALSE );
		}
	cryptDestroyContext( cryptContext );
	if( cryptStatusError( status ) )
		{
		printf( "Creation of CMP request failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( cryptRequest );
	}

static int createCmpRequest( const CERT_DATA *requestData,
							 const CRYPT_CONTEXT privateKey,
							 const CRYPT_ALGO_TYPE cryptAlgo,
							 const BOOLEAN useFixedKey,
							 const CRYPT_KEYSET cryptKeyset )
	{
	CRYPT_CERTIFICATE cryptRequest;
	time_t startTime;
	int dummy, status;

	/* If there's no existing private key available, generate a new key 
	   along with the request */
	if( privateKey == CRYPT_UNUSED )
		return( createCmpNewKeyRequest( requestData, cryptAlgo, useFixedKey, 
										cryptKeyset ) );

	/* We're updating an existing certificate we have to vary something in 
	   the request to make sure that the result doesn't duplicate an existing 
	   certificate, to do this we fiddle the start time */
	status = cryptGetAttributeString( privateKey, CRYPT_CERTINFO_VALIDFROM,
									  &startTime, &dummy );
	if( cryptStatusError( status ) )
		return( FALSE );
	startTime++;

	/* It's an update of existing information, sign the request with the 
	   given private key */
	status = cryptCreateCert( &cryptRequest, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_REQUEST_CERT );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptRequest,
						CRYPT_CERTINFO_CERTIFICATE, privateKey );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptRequest,
						CRYPT_CERTINFO_VALIDFROM, &startTime, sizeof( time_t ) );
	if( cryptStatusOK( status ) )
		status = cryptSignCert( cryptRequest, privateKey );
	if( cryptKeyset != CRYPT_UNUSED )
		{
		if( cryptStatusError( \
				cryptAddPrivateKey( cryptKeyset, privateKey,
									TEST_PRIVKEY_PASSWORD ) ) )
			return( FALSE );
		}
	if( cryptStatusError( status ) )
		{
		printf( "Creation of CMP request failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( cryptRequest );
	}

static int createCmpRevRequest( const CRYPT_CERTIFICATE cryptCert )
	{
	CRYPT_CERTIFICATE cryptRequest;
	int status;

	/* Create the CMP (CRMF) revocation request */
	status = cryptCreateCert( &cryptRequest, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_REQUEST_REVOCATION );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptRequest, CRYPT_CERTINFO_CERTIFICATE,
									cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "Creation of CMP revocation request failed with error code "
				"%d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( cryptRequest );
	}

static int createCmpSession( const CRYPT_CONTEXT cryptCACert,
							 const C_STR server, const C_STR user,
							 const C_STR password,
							 const CRYPT_CONTEXT privateKey,
							 const BOOLEAN isRevocation,
							 const BOOLEAN isUpdate,
							 const BOOLEAN isPKIBoot,
							 const CRYPT_KEYSET pnpPkiKeyset )
	{
	CRYPT_SESSION cryptSession;
	int status;

	assert( cryptCACert == CRYPT_UNUSED || !isPKIBoot );

	/* Create the CMP session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_CMP );
	if( status == CRYPT_ERROR_PARAM3 )	/* CMP session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Set up the user and server information.  Revocation requests can be
	   signed or MACd so we handle either.  When requesting a certificate 
	   using a signed request (i.e.not an initialisation request) we use an 
	   update since we're reusing the previously-generated certificate data 
	   to request a new one and some CAs won't allow this reuse for a 
	   straight request but require explicit use of an update request */
	if( privateKey != CRYPT_UNUSED )
		{
		status = cryptSetAttribute( cryptSession,
									CRYPT_SESSINFO_CMP_REQUESTTYPE,
									isRevocation ? \
										CRYPT_REQUESTTYPE_REVOCATION : \
									isUpdate ? \
										CRYPT_REQUESTTYPE_KEYUPDATE : \
										CRYPT_REQUESTTYPE_CERTIFICATE );
		if( cryptStatusOK( status ) )
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_PRIVATEKEY,
										privateKey );
		}
	else
		{
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_USERNAME, user,
										  paramStrlen( user ) );
		if( cryptStatusOK( status ) )
			status = cryptSetAttributeString( cryptSession,
									CRYPT_SESSINFO_PASSWORD, password,
									paramStrlen( password ) );
		if( cryptStatusOK( status ) && pnpPkiKeyset != CRYPT_UNUSED )
			{
			/* PnP PKI, no request type, keyset to store returned data */
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_CMP_PRIVKEYSET,
										pnpPkiKeyset );
			}
		else
			{
			/* Standard transaction, specify the request type */
			status = cryptSetAttribute( cryptSession,
										CRYPT_SESSINFO_CMP_REQUESTTYPE,
										isPKIBoot ? \
											CRYPT_REQUESTTYPE_PKIBOOT : \
										isRevocation ? \
											CRYPT_REQUESTTYPE_REVOCATION : \
											CRYPT_REQUESTTYPE_INITIALISATION );
			}
		}
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
										CRYPT_SESSINFO_SERVER_NAME, server,
										paramStrlen( server ) );
	if( cryptStatusOK( status ) && cryptCACert != CRYPT_UNUSED )
		status = cryptSetAttribute( cryptSession,
									CRYPT_SESSINFO_CACERTIFICATE,
									cryptCACert );
	if( cryptStatusError( status ) )
		{
		printf( "Addition of session information failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( cryptSession );
	}

/* Request a particular certificate type */

static int requestCert( const char *description, const CA_INFO *caInfoPtr,
						const C_STR readKeysetName,
						const C_STR writeKeysetName,
						const CERT_DATA *requestData,
						const CRYPT_ALGO_TYPE cryptAlgo,
						const CRYPT_CONTEXT cryptCACert,
						const BOOLEAN isPKIBoot, 
						const BOOLEAN isPnPPKI,
						CRYPT_CERTIFICATE *issuedCert )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_KEYSET cryptKeyset = CRYPT_UNUSED;
	CRYPT_CONTEXT privateKey = CRYPT_UNUSED;
	CRYPT_CERTIFICATE cryptCmpResponse;
	const BOOLEAN useExistingKey = ( requestData == NULL ) ? TRUE : FALSE;
	BOOLEAN hasC = FALSE, hasCN = FALSE;
	int status;

	assert( !isPKIBoot || !isPnPPKI );
	assert( !( isPnPPKI && writeKeysetName == NULL ) );

	if( requestData != NULL )
		{
		int i;

		for( i = 0; requestData[ i ].type != CRYPT_ATTRIBUTE_NONE; i++ )
			{
			if( requestData[ i ].type == CRYPT_CERTINFO_COUNTRYNAME )
				hasC = TRUE;
			if( requestData[ i ].type == CRYPT_CERTINFO_COMMONNAME )
				hasCN = TRUE;
			}
		}
	if( isPKIBoot )
		puts( "Testing standalone PKIBoot with no certificate request" );
	else
		{
		printf( "Testing %s processing", description );
		if( requestData == NULL )
			printf( "with implicitly-supplied subject DN" );
		else
			{
			if( hasCN )
				printf( " with partial subject DN" );
			else
				{
				if( !hasC )
					printf( " with CA-supplied subject DN" );
				}
			}
		}
	puts( "..." );

	/* Read the key needed to request a new certificate from a keyset if 
	   necessary and create a keyset to save a new key to if required.  We 
	   have to do the write last in case the read and write keyset are the 
	   same */
	if( readKeysetName != NULL )
		{
		status = getPrivateKey( &privateKey, readKeysetName,
								USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't get private key to request new certificate, "
					"status = %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}
	if( writeKeysetName != NULL )
		{
		assert( !isPKIBoot );	/* Keyset -> PnPPKI, not just PKIBoot */
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_FILE, writeKeysetName,
								  CRYPT_KEYOPT_CREATE );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't create keyset to store certificate to, "
					"status = %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}

	/* Create the CMP session */
	cryptSession = createCmpSession( cryptCACert, caInfoPtr->url,
									 caInfoPtr->user, caInfoPtr->password,
									 privateKey, FALSE, useExistingKey,
									 isPKIBoot, 
									 isPnPPKI ? cryptKeyset : CRYPT_UNUSED );
	if( cryptSession <= 0 )
		{
		if( cryptKeyset != CRYPT_UNUSED )
			cryptKeysetClose( cryptKeyset );
		return( cryptSession );
		}

	/* Set up the request.  Some CAs explicitly disallow multiple dissimilar 
	   certs to exist for the same key (in fact for non-test servers other 
	   CAs probably enforce this as well) but generating a new key for each 
	   request is time-consuming so we only do it if it's enforced by the 
	   CA */
	if( !isPKIBoot )
		{
		CRYPT_CERTIFICATE cryptCmpRequest;

#if defined( SERVER_IS_CRYPTLIB ) || defined( SERVER_FIXED_DN )
		cryptCmpRequest = createCmpRequest( requestData,
								useExistingKey ? privateKey : CRYPT_UNUSED,
								cryptAlgo, FALSE, cryptKeyset );
#else
		KLUDGE_WARN( "fixed key for request" );
		cryptCmpRequest = createCmpRequest( requestData,
								useExistingKey ? privateKey : CRYPT_UNUSED,
								cryptAlgo, TRUE, cryptKeyset );
#endif /* cryptlib and Initech won't allow two certs for same key */
		if( !cryptCmpRequest )
			return( FALSE );
		if( privateKey != CRYPT_UNUSED )
			cryptDestroyContext( privateKey );
		status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_REQUEST,
									cryptCmpRequest );
		cryptDestroyCert( cryptCmpRequest );
		if( cryptStatusError( status ) )
			{
			printf( "cryptSetAttribute() failed with error code %d, line %d.\n",
					status, __LINE__ );
			return( FALSE );
			}
		}

	/* Activate the session */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( cryptStatusError( status ) )
		{
		if( cryptKeyset != CRYPT_UNUSED )
			cryptKeysetClose( cryptKeyset );
		printExtError( cryptSession, "Attempt to activate CMP client session",
					   status, __LINE__ );
#ifdef SERVER_IS_CRYPTLIB
		if( status == CRYPT_ERROR_NOTFOUND )
			{
			char errorMessage[ 512 ];
			int errorMessageLength;

			/* If there's something else listening on the local port that
			   we use for CMP then it's unlikely to respond to a CMP 
			   request, so we add a special-case check for this and don't 
			   treat it as a fatal error */
			status = cryptGetAttributeString( cryptSession,
											  CRYPT_ATTRIBUTE_ERRORMESSAGE,
											  errorMessage, 
											  &errorMessageLength );
			cryptDestroySession( cryptSession );
			if( cryptStatusOK( status ) && \
				errorMessageLength > 13 && 
				!memcmp( errorMessage, "HTTP response", 13 ) )
				{
				puts( "  (Something other than a CMP server is listening on "
					  "the local port used for\n   testing, "
					  "continuing...)\n" );
				return( CRYPT_ERROR_FAILED );
				}
			}
#endif /* SERVER_IS_CRYPTLIB */
		if( isServerDown( cryptSession, status ) )
			{
			puts( "  (Server could be down, faking it and continuing...)\n" );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_FAILED );
			}
		cryptDestroySession( cryptSession );
		if( status == CRYPT_ERROR_FAILED )
			{
			/* A general failed response is more likely to be due to the
			   server doing something unexpected than a cryptlib problem so
			   we don't treat it as a fatal error */
			puts( "  (This is more likely to be an issue with the server than "
				  "with cryptlib,\n   faking it and continuing...)\n" );
			return( CRYPT_ERROR_FAILED );
			}
		return( FALSE );
		}

	/* If it's a PKIBoot, which just sets (implicitly) trusted certs, we're
	   done */
	if( isPKIBoot )
		{
		cryptDestroySession( cryptSession );
		return( TRUE );
		}

	/* Obtain the response information */
	status = cryptGetAttribute( cryptSession, CRYPT_SESSINFO_RESPONSE,
								&cryptCmpResponse );
	cryptDestroySession( cryptSession );
	if( cryptStatusError( status ) )
		{
		printf( "cryptGetAttribute() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
#ifndef SERVER_IS_CRYPTLIB
	puts( "Returned certificate details are:" );
	printCertInfo( cryptCmpResponse );
#endif /* Keep the cryptlib results on one screen */
	if( cryptKeyset != CRYPT_UNUSED )
		{
		status = cryptAddPublicKey( cryptKeyset, cryptCmpResponse );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't write certificate to keyset, status = %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		cryptKeysetClose( cryptKeyset );
		}
	if( issuedCert != NULL )
		*issuedCert = cryptCmpResponse;
	else
		cryptDestroyCert( cryptCmpResponse );

	/* Clean up */
	printf( "Successfully processed %s.\n\n", description );
	return( TRUE );
	}

/* Revoke a previously-issued certificate */

static int revokeCert( const char *description, const CA_INFO *caInfoPtr,
					   const C_STR keysetName,
					   const CRYPT_CERTIFICATE certToRevoke,
					   const CRYPT_CONTEXT cryptCACert,
					   const BOOLEAN signRequest )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_CERTIFICATE cryptCmpRequest, cryptCert = certToRevoke;
	int status;

	printf( "Testing %s revocation processing...\n", description );

	/* Get the certificate to revoke if necessary.  This may have been 
	   obtained as part of the issue process, so it's stored with the key
	   rather than being directly present */
	if( cryptCert == CRYPT_UNUSED )
		{
		CRYPT_KEYSET cryptKeyset;

		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_FILE, keysetName,
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusOK( status ) )
			status = cryptGetPublicKey( cryptKeyset, &cryptCert,
										CRYPT_KEYID_NAME,
										USER_PRIVKEY_LABEL );
		cryptKeysetClose( cryptKeyset );
		if( cryptStatusError( status ) )
			{
			puts( "Couldn't fetch certificate to revoke.\n" );
			return( FALSE );
			}
		}

	/* In some cases the server won't accept a revocation password so we 
	   have to get the private key and then sign the request */
	if( signRequest )
		{
		CRYPT_KEYSET cryptKeyset;
		CRYPT_CONTEXT privateKey = CRYPT_UNUSED;

		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_FILE, keysetName,
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusOK( status ) && signRequest )
			status = getPrivateKey( &privateKey, keysetName,
									USER_PRIVKEY_LABEL,
									TEST_PRIVKEY_PASSWORD );
		cryptKeysetClose( cryptKeyset );
		if( cryptStatusError( status ) )
			{
			puts( "Couldn't fetch certificate/key to revoke.\n" );
			return( FALSE );
			}

		/* Create the CMP session and revocation request */
		cryptSession = createCmpSession( cryptCACert, caInfoPtr->url, NULL, 
										 NULL, privateKey, TRUE, FALSE, 
										 FALSE, CRYPT_UNUSED );
		if( privateKey != CRYPT_UNUSED )
			cryptDestroyContext( privateKey );
		}
	else
		{
		/* Create the CMP session and revocation request */
		cryptSession = createCmpSession( cryptCACert, caInfoPtr->url,
										 caInfoPtr->user, caInfoPtr->revPassword,
										 CRYPT_UNUSED, TRUE, FALSE, FALSE,
										 CRYPT_UNUSED );
		}
	if( cryptSession <= 0 )
		return( cryptSession );
	cryptCmpRequest = createCmpRevRequest( cryptCert );
	if( !cryptCmpRequest )
		return( FALSE );

	/* Set up the request and activate the session */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_REQUEST,
								cryptCmpRequest );
	cryptDestroyCert( cryptCmpRequest );
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "Attempt to activate CMP client session",
					   status, __LINE__ );
		if( cryptCert != certToRevoke )
			cryptDestroyCert( cryptCert );
		if( isServerDown( cryptSession, status ) )
			{
			puts( "  (Server could be down, faking it and continuing...)\n" );
			cryptDestroySession( cryptSession );
			return( CRYPT_ERROR_FAILED );
			}
		if( status == CRYPT_ERROR_FAILED )
			{
			/* A general failed response is more likely to be due to the
			   server doing something unexpected than a cryptlib problem so
			   we don't treat it as a fatal error */
			puts( "  (This is more likely to be an issue with the server than "
				  "with cryptlib,\n   faking it and continuing...)\n" );
			return( CRYPT_ERROR_FAILED );
			}
		return( FALSE );
		}

	/* Clean up */
	if( cryptCert != certToRevoke )
		cryptDestroyCert( cryptCert );
	cryptDestroySession( cryptSession );
	printf( "%s processing succeeded.\n\n", description );
	return( TRUE );
	}

/* Get the user name and password for a PKI user */

static int getPkiUserInfo( const C_STR pkiUserName,
						   CA_INFO *caInfoPtr, C_STR userID, 
						   C_STR issuePW, C_STR revPW  )
	{
	int status;

	/* cryptlib implements per-user (rather than shared interop) IDs and 
	   passwords so we have to read the user ID and password information 
	   before we can perform any operations */
	status = pkiGetUserInfo( userID, issuePW, revPW, pkiUserName );
	if( status == CRYPT_ERROR_NOTAVAIL )
		return( status );
	if( !status )
		return( FALSE );
	memcpy( caInfoPtr, &caInfoTbl[ CA_NO ], sizeof( CA_INFO ) );
	caInfoPtr->name = "cryptlib";
	caInfoPtr->user = userID;
	caInfoPtr->password = issuePW;
	caInfoPtr->revPassword = revPW;

	return( TRUE );
	}

/****************************************************************************
*																			*
*								CMP Routines Test							*
*																			*
****************************************************************************/

/* Test the full range of CMP functionality.  This performs the following
   tests:

	RSA sign:
		ir + ip + reject (not performed since requires cmp.c mod)
		ir + ip + certconf + pkiconf
		cr + cp + certconf + pkiconf
		kur + kup + certconf + pkiconf
		rr + rp (of ir certificate)
		rr + rp (of kur certificate)
	RSA encr.:
		ir + ip + reject (requires cmp.c mod)
		ir + ip + certconf + pkiconf
		rr + rp (of ir certificate)
	DSA:
		cr + cp + certconf + pkiconf (success implies that ir/kur/rr
						works since they've already been tested for RSA) */

/* Connect to a cryptlib server, for the loopback tests */

static int connectCryptlibCMP( const BOOLEAN usePKIBoot, 
							   const BOOLEAN localSession )
	{
	CRYPT_CERTIFICATE cryptCACert = CRYPT_UNUSED, cryptCert;
	C_CHR readFileName[ FILENAME_BUFFER_SIZE ];
	C_CHR writeFileName[ FILENAME_BUFFER_SIZE ];
	CA_INFO caInfo;
	C_CHR userID[ 64 ], issuePW[ 64 ], revPW[ 64 ];
	int status;

	/* Wait for the server to finish initialising */
	if( localSession && waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n",
				__LINE__ );
		return( FALSE );
		}

	/* Make sure that the required user information is present.  If it isn't
	   then the CA auditing will detect a request from a nonexistent user 
	   and refuse to issue a certificate */
	status = pkiGetUserInfo( NULL, NULL, NULL, TEXT( "Test PKI user" ) );
	if( status == CRYPT_ERROR_NOTAVAIL )
		{
		/* Cert store operations aren't available, exit but continue with
		   other tests */
		return( TRUE );
		}
	if( !status )
		{
		puts( "CA certificate store doesn't contain the PKI user "
			  "information needed to\nauthenticate certificate issue "
			  "operations.  This is probably because the\nserver loopback "
			  "test (which initialises the certificate store) hasn't been "
			  "run yet.\nSkipping CMP test.\n" );
		return( CRYPT_ERROR_NOTAVAIL );
		}

	/* If we're not doing PKIBoot (which obtains the CA's certificate during 
	   the PKIBoot process), get the certificate of the CA who will issue 
	   the certificate */
	if( !usePKIBoot )
		{
		status = importCertFromTemplate( &cryptCACert, CMP_CA_FILE_TEMPLATE,
										 CA_NO );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't get cryptlib CMP CA certificate, status = %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}

	/* Initialisation request.  We perform two ir's, the first with the CA
	   supplying the full user DN and email address, the second with the CA 
	   supplying a partial DN and no email address, and the user filling in 
	   the rest.  We continue with the certificate from the second ir, 
	   because the first one doesn't allow anything other than a kur since 
	   the DN is fixed */
	status = getPkiUserInfo( TEXT( "Test PKI user" ), &caInfo, 
							 userID, issuePW, revPW );
	if( status != TRUE )
		{
		if( !usePKIBoot )
			cryptDestroyCert( cryptCACert );
		return( FALSE );
		}
	filenameParamFromTemplate( writeFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
	status = requestCert( "certificate init.request (ir)", &caInfo, NULL, 
						  usePKIBoot ? NULL : writeFileName,
						  cmpCryptlibRequestNoDNData, CRYPT_ALGO_RSA, 
						  cryptCACert, usePKIBoot, FALSE, NULL );
	if( status != TRUE )
		{
		/* If this is the self-test and there's a non-fatal error, make sure
		   we don't fail with a CRYPT_ERROR_INCOMPLETE when we're finished */
		if( !usePKIBoot )
			cryptDestroyCert( cryptCACert );
		return( status );
		}
	if( usePKIBoot )
		{
		/* If we're testing the PKIBoot capability there's only a single
		   request to process and we're done */
		return( TRUE );
		}
	delayThread( 2 );	/* Wait for server to recycle */
	status = getPkiUserInfo( TEXT( "Procurement" ), &caInfo, userID, 
							 issuePW, revPW );
	if( status != TRUE )
		{
		if( !usePKIBoot )
			cryptDestroyCert( cryptCACert );
		return( FALSE );
		}
	filenameParamFromTemplate( writeFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
	status = requestCert( "certificate init.request (ir)", &caInfo, NULL, 
						  usePKIBoot ? NULL : writeFileName,
						  cmpCryptlibRequestData, CRYPT_ALGO_RSA, 
						  cryptCACert, FALSE, FALSE, &cryptCert );
	if( status != TRUE )
		return( FALSE );
#ifdef TEST_DUP_IR
	/* Attempt a second ir using the same PKI user data.  This should fail,
	   since the certificate store only allows a single ir per user */
	if( requestCert( "Duplicate init.request", &caInfo, NULL, NULL,
					 cmpCryptlibRequestNoDNData, CRYPT_ALGO_RSA, 
					 cryptCACert, FALSE, TRUE, NULL ) )
		{
		printf( "Duplicate init request wasn't detected by the CMP server, "
				"line %d.\n\n", __LINE__ );
		cryptDestroyCert( cryptCACert );
		return( FALSE );
		}
#endif /* TEST_DUP_IR */

	/* Certificate request.  We have to perform this test before the kur 
	   because cryptlib implicitly revokes the certificate being replaced, 
	   which means that we can't use it to authenticate requests any more 
	   once the kur has been performed */
	filenameParamFromTemplate( readFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
	filenameParamFromTemplate( writeFileName, CMP_PRIVKEY_FILE_TEMPLATE, 2 );
	status = requestCert( "certificate request (cr)", &caInfo, readFileName, 
						  writeFileName, cmpCryptlibRequestData, 
						  CRYPT_ALGO_RSA, cryptCACert, FALSE, FALSE, NULL );
	if( status != TRUE )
		{
		cryptDestroyCert( cryptCert );
		cryptDestroyCert( cryptCACert );
		return( status );
		}
	delayThread( 2 );		/* Wait for server to recycle */

	/* Key update request.  This updates the existing key that we've just
	   written to a file, so there's no new certificate-request data that
	   gets submitted.  In other words the new certificate that's issued is
	   identical to the existing one except for the date/time value.

	   Since we've just created the certificate, we have to delete it so 
	   that we can replace it with the kur'd form */
	cryptDestroyCert( cryptCert );

	/* If it's a CA that implicitly revokes the certificate being replaced 
	   (in which case tracking things gets a bit too complicated since we 
	   now need to use the updated rather than original certificate to 
	   authenticate the request) we just leave it unrevoked (the first 
	   certificate is always revoked) */
	filenameParamFromTemplate( readFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
	status = requestCert( "certificate update (kur)", &caInfo, readFileName, 
						  NULL, NULL, CRYPT_UNUSED, cryptCACert, FALSE, 
						  FALSE, &cryptCert );
	if( status != TRUE )
		{
		cryptDestroyCert( cryptCACert );
		return( status );
		}
	delayThread( 2 );		/* Wait for server to recycle */
#if 0
	/* DSA certificate request.  We have to get this now because we're about 
	   to revoke the certificate we're using to sign the requests.  This 
	   currently isn't checked since it's a standard signature-only 
	   certificate request whose processing has already been checked in
	   testCertProcess() */
	filenameParamFromTemplate( readFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
	status = requestCert( "DSA certificate", &caInfo, readFileName, NULL,
						  cmpDsaRequestData, CRYPT_ALGO_DSA, cryptCACert,
						  FALSE, FALSE, NULL );
	if( status != TRUE )
		return( status );
	delayThread( 2 );		/* Wait for server to recycle */
#endif /* 0 */

	/* Revocation request.  The current certificate is a kur'd certificate 
	   for which the original has been implicitly revoked by the kur process 
	   and what we have now is an ephemeral test-only certificate whose 
	   details weren't stored (so they can't be passed to requestCert(), we 
	   can't do much else with it */
	cryptDestroyCert( cryptCert );

	/* We requested a second certificate whose details were recorded, revoke 
	   that.  Note that we have to sign this with the second certificate 
	   since the first one was the implicitly-revoked kur'd one */
	filenameParamFromTemplate( readFileName, CMP_PRIVKEY_FILE_TEMPLATE, 2 );
	status = revokeCert( "RSA signing certificate", &caInfo, readFileName,
						 CRYPT_UNUSED, cryptCACert, USE_SIGNED_RR );
	if( status != TRUE )
		{
		cryptDestroyCert( cryptCACert );
		return( status );
		}

	/* Clean up */
	cryptDestroyCert( cryptCACert );
	return( TRUE );
	}

/* Connect to a non-cryptlib CMP server */

static int connectCMP( void )
	{
	CRYPT_CERTIFICATE cryptCACert = CRYPT_UNUSED, cryptCert;
	C_CHR readFileName[ FILENAME_BUFFER_SIZE ];
	C_CHR writeFileName[ FILENAME_BUFFER_SIZE ];
	const CA_INFO *caInfoPtr = &caInfoTbl[ CA_NO ];
	int status;

	/* Get the certificate of the CA who will issue the certificate */
	status = importCertFromTemplate( &cryptCACert, CMP_CA_FILE_TEMPLATE,
									 CA_NO );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't get CMP CA certificate, status = %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Test each certificate request type */

#ifdef TEST_IR
	/* Initialisation request.  We define REVOKE_FIRST_CERT to indicate that 
	   we can revoke this one later on */
	#define REVOKE_FIRST_CERT
	filenameParamFromTemplate( writeFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
	status = requestCert( "certificate init.request (ir)", caInfoPtr, NULL, 
						  writeFileName, cmpRsaSignRequestData, 
						  CRYPT_ALGO_RSA, cryptCACert, FALSE, FALSE, 
						  &cryptCert );
	if( status != TRUE )
		{
		/* If this is the self-test and there's a non-fatal error, make sure
		   we don't fail with a CRYPT_ERROR_INCOMPLETE when we're finished */
		cryptDestroyCert( cryptCACert );
		return( status );
		}
#endif /* TEST_IR */

#ifdef TEST_CR
	/* Certificate request.  We have to perform this test before the kur 
	   since some CAs implicitly revoke the certificate being replaced, 
	   which means that we can't use it to authenticate requests any more 
	   once the kur has been performed.  We define REVOKE_SECOND_CERT to 
	   indicate that we can revoke this one later on alongside the ir/kur'd 
	   certificate, and save a copy to a file for later use */
	#define REVOKE_SECOND_CERT
	filenameParamFromTemplate( readFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
	filenameParamFromTemplate( writeFileName, CMP_PRIVKEY_FILE_TEMPLATE, 2 );
	status = requestCert( "certificate request (cr)", caInfoPtr, 
						  readFileName, writeFileName, cmpRsaSignRequestData,
						  CRYPT_ALGO_RSA, cryptCACert, FALSE, FALSE, NULL );
	if( status != TRUE )
		{
  #if defined( TEST_IR )
		cryptDestroyCert( cryptCert );
  #endif /* TEST_IR || TEST_KUR */
		cryptDestroyCert( cryptCACert );
		return( status );
		}
#endif /* TEST_CR */

#ifdef TEST_KUR
	/* Key update request.  This updates the existing key that we've just
	   written to a file, so there's no new certificate-request data that
	   gets submitted.  In other words the new certificate that's issued is
	   identical to the existing one except for the dates/time values */
  #ifdef TEST_IR
	/* We just created the certificate, delete it so we can replace it with 
	   the updated form */
	cryptDestroyCert( cryptCert );
  #endif /* TEST_IR */

	/* If it's a CA that implicitly revokes the certificate being replaced 
	   (in which case tracking things gets a bit too complicated since we 
	   now need to use the updated rather than original certificate to 
	   authenticate the request) we just leave it unrevoked (the first 
	   certificate is always revoked) */
	filenameParamFromTemplate( readFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
	status = requestCert( "certificate update (kur)", caInfoPtr, 
						  readFileName, NULL, NULL, CRYPT_UNUSED,
						  cryptCACert, FALSE, FALSE, &cryptCert );
	if( status != TRUE )
		{
		cryptDestroyCert( cryptCACert );
		return( status );
		}
#endif /* TEST_KUR */
#if 0
	/* Encryption-only certificate request.  This test requires a change in
	   certsign.c because when creating a certificate request cryptlib 
	   always allows signing for the request even if it's an encryption-only 
	   key (this is required for PKCS #10, see the comment in the kernel 
	   code).  Because of this a request will always appear to be associated 
	   with a signature-enabled key so it's necessary to make a code change 
	   to disallow this.  Disallowing sigs for encryption-only keys would 
	   break PKCS #10 since it's then no longer possible to create the self-
	   signed request, this is a much bigger concern than CMP.  Note that 
	   this functionality is tested by the PnP PKI test, which creates the
	   necessary encryption-only requests internally and can do things that
	   we can't do from the outside */
	status = requestCert( "encryption-only certificate", caInfoPtr,
						  readFileName, writeFileName, 
						  cmpRsaEncryptRequestData, CRYPT_ALGO_RSA, 
						  cryptCACert, FALSE, FALSE, NULL );
	if( status != TRUE )
		return( status );
#endif /* 0 */

	/* Revocation request */
#ifdef TEST_RR
	filenameParamFromTemplate( readFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
  #ifdef REVOKE_FIRST_CERT
	#ifdef SERVER_IR_DN
	status = revokeCert( "RSA initial/updated certificate", caInfoPtr,
						 readFileName, cryptCert, cryptCACert, TRUE );
	#else
	status = revokeCert( "RSA initial/updated certificate", caInfoPtr,
						 readFileName, cryptCert, cryptCACert, FALSE );
	#endif /* Certicom requires signed request */
	cryptDestroyCert( cryptCert );
  #elif !defined( TEST_KUR )
	/* We didn't issue the first certificate in this run, try revoking it 
	   from the certificate stored in the key file unless we're talking to a 
	   CA that implicitly revokes the certificate being replaced during a 
	   kur */
	status = revokeCert( "RSA initial/updated certificate", caInfoPtr,
						 readFileName, CRYPT_UNUSED, cryptCACert, TRUE );
  #else
	/* This is a kur'd certificate for which the original has been 
	   implicitly revoked, we can't do much else with it */
	cryptDestroyCert( cryptCert );
  #endif /* REVOKE_FIRST_CERT */
	if( status != TRUE )
		{
		cryptDestroyCert( cryptCACert );
		return( status );
		}
  #ifdef REVOKE_SECOND_CERT
	/* We requested a second certificate, revoke that too.  Note that we 
	   have to sign this with the second certificate since the first one may 
	   have just been revoked */
	filenameParamFromTemplate( readFileName, CMP_PRIVKEY_FILE_TEMPLATE, 2 );
	status = revokeCert( "RSA signing certificate", caInfoPtr, readFileName,
						 CRYPT_UNUSED, cryptCACert, TRUE );
	if( status != TRUE )
		{
		cryptDestroyCert( cryptCACert );
		return( status );
		}
  #endif /* REVOKE_SECOND_CERT */
#endif /* TEST_RR */

	/* Clean up */
	cryptDestroyCert( cryptCACert );
	return( TRUE );
	}

static int connectCMPFail( const int count )
	{
	static const CERT_DATA FAR_BSS cmpFailRequestData1[] = {
		/* Fails when tested against the full-DN PKI user because the CN 
		   doesn't match the PKI user CN */
		{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
		{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
		{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
		{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Not the test PKI user" ) },

		{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
		};
	static const CERT_DATA FAR_BSS cmpFailRequestData2[] = {
		/* Fails when tested against the full-DN PKI user because there's 
		   already an email address present in the PKI user data */
#if 0
		{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },
#endif /* 0 */
		{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

		{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
		};
	static const CERT_DATA FAR_BSS cmpFailRequestData3[] = {
		/* Fails when tested against the partial-DN (no CN specified) PKI 
		   user because the OU doesn't match the PKI user OU even though the 
		   CN is allowed */
		{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Not procurement" ) },
		{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test PKI user" ) },

		{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
		};
	static const CERT_DATA FAR_BSS *cmpFailRequestDataTbl[] = {
		cmpFailRequestData1, cmpFailRequestData2, cmpFailRequestData3
		};
	static const C_STR FAR_BSS cmpFailRequestNameTbl[] = {
		TEXT( "Test PKI user" ),	/* Template has full DN + email */
		TEXT( "Test PKI user" ),	/* Template has full DN + email */
		TEXT( "Procurement" )		/* Template has no CN or email */
		};
	static const char *cmpFailRequestDescriptionTbl[] = {
		"request containing full DN with CN mis-matching\n  pkiUser CN",
		"request containing extra field in altName\n  not present in pkiUser altName",
		"request containing partial DN with OU\n  mis-matching pkiUser CN"
		};
	CRYPT_CERTIFICATE cryptCACert = CRYPT_UNUSED, cryptCert;
	C_CHR writeFileName[ FILENAME_BUFFER_SIZE ];
	CA_INFO caInfo;
	C_CHR userID[ 64 ], issuePW[ 64 ], revPW[ 64 ];
	char message[ 128 ];
	int status;

	/* Wait for the server to finish initialising */
	if( count == 0 && waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n",
				__LINE__ );
		return( FALSE );
		}

	/* Get the certificate of the CA who will issue the certificate and the 
	   PKI user details */
	status = importCertFromTemplate( &cryptCACert, CMP_CA_FILE_TEMPLATE,
									 CA_CRYPTLIB );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't get CMP CA certificate, status = %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = getPkiUserInfo( TEXT( "Test PKI user" ), &caInfo, 
							 userID, issuePW, revPW );
	if( status != TRUE )
		{
		cryptDestroyCert( cryptCACert );
		return( FALSE );
		}

	/* Send the request, which should be rejected by the server because 
	   something in the request differs from what's in the PKI user 
	   template */
	filenameParamFromTemplate( writeFileName, CMP_PRIVKEY_FILE_TEMPLATE, 1 );
	sprintf( message, "invalid request %d with %s,", count + 1,
			 cmpFailRequestDescriptionTbl[ count ] );
	status = requestCert( message, &caInfo, NULL, NULL, 
						  cmpFailRequestDataTbl[ count ], CRYPT_ALGO_RSA, 
						  cryptCACert, FALSE, FALSE, &cryptCert );
	cryptDestroyCert( cryptCACert );
	if( status )
		{
		/* The request should have been rejected */
		puts( "Invalid CMP request should have been rejected, but "
			  "wasn't.\n" );
		return( FALSE );
		}

	/* The request was successfully rejected, let the user know that the 
	   error spew was supposed to be there */
	puts( "  (This isn't an error since we're checking for the rejection "
		  "of invalid\n   requests)." );
	return( TRUE );
	}

int testSessionCMP( void )
	{
	return( connectCMP() );
	}

/* Test the plug-and-play PKI functionality */

static int connectPNPPKI( const BOOLEAN isCaUser, const BOOLEAN useDevice,
						  const BOOLEAN localSession )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_KEYSET cryptKeyset;
	C_CHR userID[ 64 ], issuePW[ 64 ], revPW[ 64 ];
	int status;

	/* Create the CMP session */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_CMP );
	if( status == CRYPT_ERROR_PARAM3 )	/* CMP session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateSession() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Open the device/create the keyset to contain the keys.  This doesn't
	   perform a full device.c-style auto-configure but assumes that it's
	   talking to a device that's already been initialised and is ready to
	   go */
	if( useDevice )
		{
		status = cryptDeviceOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_DEVICE_PKCS11,
								  TEXT( "[Autodetect]" ) );
		if( cryptStatusError( status ) )
			{
			printf( "Crypto device open failed with error code %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptSetAttributeString( cryptKeyset,
										  CRYPT_DEVINFO_AUTHENT_USER,
										  "test", 4 );
		if( cryptStatusError( status ) )
			{
			printf( "\nDevice login failed with error code %d, line %d.\n",
					status, __LINE__ );
			return( FALSE );
			}
		if( cryptDeleteKey( cryptKeyset, CRYPT_KEYID_NAME,
							TEXT( "Signature key" ) ) == CRYPT_OK )
			puts( "(Deleted a signature key object, presumably a leftover "
				  "from a previous run)." );
		if( cryptDeleteKey( cryptKeyset, CRYPT_KEYID_NAME,
							TEXT( "Encryption key" ) ) == CRYPT_OK )
			puts( "(Deleted an encryption key object, presumably a leftover "
				  "from a previous run)." );
		}
	else
		{
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_FILE, isCaUser ? \
										PNPCA_PRIVKEY_FILE : PNP_PRIVKEY_FILE,
								  CRYPT_KEYOPT_CREATE );
		if( cryptStatusError( status ) )
			{
			printf( "User keyset create failed with error code %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}

	/* Wait for the server to finish initialising */
	if( localSession && waitMutex() == CRYPT_ERROR_TIMEOUT )
		{
		printf( "Timed out waiting for server to initialise, line %d.\n",
				__LINE__ );
		return( FALSE );
		}

	/* Get information needed for enrolment */
	status = pkiGetUserInfo( userID, issuePW, revPW, isCaUser ? \
								TEXT( "Test CA PKI user" ) : \
								TEXT( "Test PKI user" ) );
	if( status == CRYPT_ERROR_NOTAVAIL )
		{
		/* Certificate store operations aren't available, exit but continue 
		   with other tests */
		return( TRUE );
		}
	else
		{
		if( !status )
			return( FALSE );
		}

	/* Set up the information we need for the plug-and-play PKI process */
	status = cryptSetAttributeString( cryptSession,
									  CRYPT_SESSINFO_USERNAME, userID,
									  paramStrlen( userID ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_PASSWORD,
										  issuePW, paramStrlen( issuePW ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SERVER_NAME,
										  caInfoTbl[ CA_CRYPTLIB_PNPPKI ].url,
										  paramStrlen( caInfoTbl[ CA_CRYPTLIB_PNPPKI ].url ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptSession,
									CRYPT_SESSINFO_CMP_PRIVKEYSET,
									cryptKeyset );
	if( cryptStatusOK( status ) && useDevice )
		{
		/* Keygen on a device can take an awfully long time for some devices,
		   so we set an extended timeout to allow for this */
		cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_READTIMEOUT,
						   NET_TIMEOUT );
		status = cryptSetAttribute( cryptSession,
									CRYPT_OPTION_NET_WRITETIMEOUT,
									NET_TIMEOUT );
		}
	if( useDevice )
		cryptDeviceClose( cryptKeyset );
	else
		cryptKeysetClose( cryptKeyset );
	if( cryptStatusError( status ) )
		{
		printf( "Addition of session information failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Activate the session */
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "Attempt to activate plug-and-play PKI "
					   "client session", status, __LINE__ );
		cryptDestroySession( cryptSession );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroySession( cryptSession );

	/* If this is the intermediate CA certificate, change the password to 
	   allow it to be used with the standard PnP PKI test */
	if( isCaUser )
		{
		CRYPT_CONTEXT cryptKey = DUMMY_INIT;

		/* Get the newly-issued key */
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_FILE, PNPCA_PRIVKEY_FILE,
								  CRYPT_KEYOPT_NONE );
		if( cryptStatusOK( status ) )
			{
			status = cryptGetPrivateKey( cryptKeyset, &cryptKey,
										 CRYPT_KEYID_NAME,
										 TEXT( "Signature key" ), issuePW );
			cryptKeysetClose( cryptKeyset );
			}
		if( cryptStatusError( status ) )
			{
			printf( "Certified private-key read failed with error code %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}

		/* Replace the keyset with one with the key protected with a
		   different password */
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_FILE, PNPCA_PRIVKEY_FILE,
								  CRYPT_KEYOPT_CREATE );
		if( cryptStatusOK( status ) )
			{
			status = cryptAddPrivateKey( cryptKeyset, cryptKey,
										 TEST_PRIVKEY_PASSWORD );
			cryptKeysetClose( cryptKeyset );
			}
		cryptDestroyContext( cryptKey );
		if( cryptStatusError( status ) )
			{
			printf( "Certified private-key password change failed with error "
					"code %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}

	return( TRUE );
	}

int testSessionPNPPKI( void )
	{
	return( connectPNPPKI( FALSE, FALSE, FALSE ) );
	}

/* Test the CMP server */

static int cmpServerSingleIteration( const CRYPT_CONTEXT cryptPrivateKey,
									 const CRYPT_KEYSET cryptCertStore,
									 const BOOLEAN useDevice )
	{
	CRYPT_SESSION cryptSession;
	int status;

	/* Create the CMP session and add the CA key and certificate store */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_CMP_SERVER );
	if( cryptStatusError( status ) )
		{
		printf( "SVR: cryptCreateSession() failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_PRIVATEKEY, cryptPrivateKey );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_KEYSET, cryptCertStore );
	if( cryptStatusOK( status ) && useDevice )
		{
		/* Keygen on a device can take an awfully long time for some devices,
		   so we set an extended timeout to allow for this */
		cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_READTIMEOUT,
						   NET_TIMEOUT );
		status = cryptSetAttribute( cryptSession,
									CRYPT_OPTION_NET_WRITETIMEOUT,
									NET_TIMEOUT );
		}
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptSession, "SVR: cryptSetAttribute()",
							   status, __LINE__ ) );
	if( !setLocalConnect( cryptSession, 80 ) )
		return( FALSE );

	/* Activate the session */
	status = activatePersistentServerSession( cryptSession, TRUE );
	if( cryptStatusError( status ) )
		{
		status = extErrorExit( cryptSession, "SVR: Attempt to activate CMP "
							   "server session", status, __LINE__ );
		cryptDestroySession( cryptSession );
		return( status );
		}

	/* We processed the request, clean up */
	cryptDestroySession( cryptSession );
	return( TRUE );
	}

static int cmpServer( void )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_CONTEXT cryptCAKey;
	CRYPT_KEYSET cryptCertStore;
	int i, status;

	/* Acquire the init mutex */
	acquireMutex();

	puts( "SVR: Testing CMP server session..." );

	/* Perform a test create of a CMP server session to verify that we can
	   do this test */
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_CMP_SERVER );
	if( status == CRYPT_ERROR_PARAM3 )	/* CMP session access not available */
		return( CRYPT_ERROR_NOTAVAIL );
	if( cryptStatusError( status ) )
		{
		printf( "SVR: cryptCreateSession() failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	cryptDestroySession( cryptSession );

	/* Set up the server-side objects */
	if( !pkiServerInit( &cryptCAKey, &cryptCertStore, CA_PRIVKEY_FILE,
						CA_PRIVKEY_LABEL, cmpPkiUserFullDNData, 
						cmpPkiUserPartialDNData, cmpPkiUserCaData, 
						"CMP" ) )
		return( FALSE );

	/* Tell the client that we're ready to go */
	releaseMutex();

	/* Run the server several times to handle the different requests */
	for( i = 0; i < NO_CA_REQUESTS; i++ )
		{
		printf( "SVR: Running server iteration %d.\n", i + 1 );
		if( !cmpServerSingleIteration( cryptCAKey, cryptCertStore, FALSE ) )
			{
#if defined( SERVER_IS_CRYPTLIB ) && defined( TEST_DUP_IR )
			/* If we're running the loopback test and this is the second
			   iteration, the client is testing the ability to detect a
			   duplicate ir, so a failure is expected */
			if( i == 1 )
				{
				puts( "SVR: Failure was due to a rejected duplicate request "
					  "from the client,\n     continuing..." );
				continue;
				}
#endif /* SERVER_IS_CRYPTLIB && TEST_DUP_IR */
			break;
			}
		}
	if( i < NO_CA_REQUESTS )
		{
		if( i == 0 )
			return( FALSE );
		printf( "SVR: Only %d of %d server requests were processed.\n", i,
				NO_CA_REQUESTS );
		return( FALSE );
		}
	puts( "SVR: All server requests were successfully processed." );

	/* Issue a CRL to make sure that the revocation was performed correctly.
	   We do this now because the certificate management self-test can't 
	   easily perform the check because it requires a CMP-revoked 
	   certificate in order to function */
	if( i >= NO_CA_REQUESTS )
		{
		CRYPT_CERTIFICATE cryptCRL;
		int noEntries = 0;

		/* Issue the CRL */
		status = cryptCACertManagement( &cryptCRL, CRYPT_CERTACTION_ISSUE_CRL,
										cryptCertStore, cryptCAKey,
										CRYPT_UNUSED );
		if( cryptStatusError( status ) )
			return( extErrorExit( cryptCertStore, "cryptCACertManagement()",
								  status, __LINE__ ) );

		/* Make sure that the CRL contains at least one entry */
		if( cryptStatusOK( cryptSetAttribute( cryptCRL,
											  CRYPT_CERTINFO_CURRENT_CERTIFICATE,
											  CRYPT_CURSOR_FIRST ) ) )
			do
				noEntries++;
			while( cryptSetAttribute( cryptCRL,
									  CRYPT_CERTINFO_CURRENT_CERTIFICATE,
									  CRYPT_CURSOR_NEXT ) == CRYPT_OK );
		if( noEntries <= 0 )
			{
			puts( "CRL created from revoked certificate is empty, should "
				  "contain at least one\ncertificate entry." );
			return( FALSE );
			}

		/* Clean up */
		cryptDestroyCert( cryptCRL );
		}

	/* Clean up */
	cryptKeysetClose( cryptCertStore );
	cryptDestroyContext( cryptCAKey );
	puts( "SVR: CMP session succeeded.\n" );
	return( TRUE );
	}

int testSessionCMPServer( void )
	{
	int status;

	createMutex();
	status = cmpServer();
	destroyMutex();

	return( status );
	}

static int cmpServerFail( void )
	{
	CRYPT_CONTEXT cryptCAKey;
	CRYPT_KEYSET cryptCertStore;
	int i;

	/* Acquire the init mutex */
	acquireMutex();

	puts( "SVR: Testing CMP server for rejection of invalid requests..." );

	/* Set up the server-side objects */
	if( !pkiServerInit( &cryptCAKey, &cryptCertStore, CA_PRIVKEY_FILE,
						CA_PRIVKEY_LABEL, cmpPkiUserFullDNData, 
						cmpPkiUserPartialDNData, cmpPkiUserCaData, 
						"CMP" ) )
		return( FALSE );

	/* Tell the client that we're ready to go */
	releaseMutex();

	/* Run the server several times to handle the different requests, which 
	   should all be rejected */
	for( i = 0; i < 3; i++ )
		{
		printf( "SVR: Running server iteration %d.\n", i + 1 );
		if( cmpServerSingleIteration( cryptCAKey, cryptCertStore, FALSE ) )
			{
			puts( "SVR: CMP request succeeded when it should have "
				  "failed.\n" );
			return( FALSE );
			}
		}

	/* Clean up */
	cryptKeysetClose( cryptCertStore );
	cryptDestroyContext( cryptCAKey );

	puts( "SVR: CMP invalid requests successfully rejected.\n" );
	return( TRUE );
	}

/* Perform a client/server loopback test */

#ifdef WINDOWS_THREADS

static int pnppkiServer( const BOOLEAN pkiBootOnly, const BOOLEAN isCaUser,
						 const BOOLEAN isIntermediateCA,
						 const BOOLEAN useDevice )
	{
	CRYPT_CONTEXT cryptCAKey;
	CRYPT_KEYSET cryptCertStore;

	/* Acquire the PNP PKI init mutex */
	acquireMutex();

	printf( "SVR: Testing %s server session%s...\n",
			pkiBootOnly ? "PKIBoot" : "plug-and-play PKI",
			isCaUser ? " for CA certificate" : \
				isIntermediateCA ? " using intermediate CA" : "" );

	/* Get the information needed by the server */
	if( isIntermediateCA )
		{
		/* The intermediate CA has a PnP-generated, so the key label is
		   the predefined PnP signature key one */
		if( !pkiServerInit( &cryptCAKey, &cryptCertStore,
							PNPCA_PRIVKEY_FILE, TEXT( "Signature key" ),
							cmpPkiUserFullDNData, cmpPkiUserPartialDNData, 
							cmpPkiUserCaData, "CMP" ) )
			return( FALSE );
		}
	else
		{
		if( !pkiServerInit( &cryptCAKey, &cryptCertStore, CA_PRIVKEY_FILE,
							CA_PRIVKEY_LABEL, cmpPkiUserFullDNData,
							cmpPkiUserPartialDNData, cmpPkiUserCaData, 
							"CMP" ) )
			return( FALSE );
		}

	/* Tell the client that we're ready to go */
	releaseMutex();

	/* Run the server once to handle the plug-and-play PKI process */
	if( !cmpServerSingleIteration( cryptCAKey, cryptCertStore, useDevice ) )
		return( FALSE );

	/* Clean up */
	cryptKeysetClose( cryptCertStore );
	cryptDestroyContext( cryptCAKey );

	puts( "SVR: Plug-and-play PKI session succeeded.\n" );
	return( TRUE );
	}

unsigned __stdcall cmpServerThread( void *dummy )
	{
	cmpServer();
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionCMPClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

#ifndef SERVER_IS_CRYPTLIB
	/* Because the code has to handle so many CA-specific peculiarities, we
	   can only perform this test when the CA being used is the cryptlib
	   CA */
	puts( "Error: The local CMP session test only works with the cryptlib "
		  "CA." );
	return( FALSE );
#endif /* !SERVER_IS_CRYPTLIB */

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, cmpServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectCryptlibCMP( FALSE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

int testSessionCMPSHA2ClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int value, status;

#ifndef SERVER_IS_CRYPTLIB
	/* Because the code has to handle so many CA-specific peculiarities, we
	   can only perform this test when the CA being used is the cryptlib
	   CA */
	puts( "Error: The local CMP session test only works with the cryptlib "
		  "CA." );
	return( FALSE );
#endif /* !SERVER_IS_CRYPTLIB */

	/* Switch the hash algorithm to SHA-2 */
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH, &value );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH,
					   CRYPT_ALGO_SHA2 );

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, cmpServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server */
	status = connectCryptlibCMP( FALSE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH, value );

	return( status );
	}

unsigned __stdcall cmpPKIBootServerThread( void *dummy )
	{
	pnppkiServer( TRUE, FALSE, FALSE, FALSE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionCMPPKIBootClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

#ifndef SERVER_IS_CRYPTLIB
	/* Because the code has to handle so many CA-specific peculiarities, we
	   can only perform this test when the CA being used is the cryptlib
	   CA */
	puts( "Error: The local CMP session test only works with the cryptlib "
		  "CA." );
	return( FALSE );
#endif /* !SERVER_IS_CRYPTLIB */

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, cmpPKIBootServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server with PKIBoot enabled */
	status = connectCryptlibCMP( TRUE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

unsigned __stdcall cmpPnPPKIServerThread( void *dummy )
	{
	/* Call with the third parameter set to TRUE to use a chain of CA certs
	   (i.e. an intermediate CA between the root and end user) rather than
	   a single CA certificate directly issuing the certificate to the end 
	   user */
	pnppkiServer( FALSE, FALSE, FALSE, FALSE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionPNPPKIClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, cmpPnPPKIServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server with PKIBoot enabled */
	status = connectPNPPKI( FALSE, FALSE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

unsigned __stdcall cmpPnPPKIDeviceServerThread( void *dummy )
	{
	/* Call with the third parameter set to TRUE to use a chain of CA certs
	   (i.e. an intermediate CA between the root and end user) rather than
	   a single CA certificate directly issuing the certificate to the end 
	   user */
	pnppkiServer( FALSE, FALSE, FALSE, TRUE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionPNPPKIDeviceClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, cmpPnPPKIDeviceServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server with PKIBoot enabled */
	status = connectPNPPKI( FALSE, TRUE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

unsigned __stdcall cmpPnPPKICaServerThread( void *dummy )
	{
	pnppkiServer( FALSE, TRUE, FALSE, FALSE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionPNPPKICAClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, cmpPnPPKICaServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server with PKIBoot enabled */
	status = connectPNPPKI( TRUE, FALSE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

unsigned __stdcall cmpPnPPKIIntermedCaServerThread( void *dummy )
	{
	pnppkiServer( FALSE, FALSE, TRUE, FALSE );
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionPNPPKIIntermedCAClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, cmpPnPPKIIntermedCaServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server with PKIBoot enabled */
	status = connectPNPPKI( FALSE, FALSE, TRUE );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}

unsigned __stdcall cmpFailServerThread( void *dummy )
	{
	cmpServerFail();
	_endthreadex( 0 );
	return( 0 );
	}

int testSessionCMPFailClientServer( void )
	{
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server */
	createMutex();
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, cmpFailServerThread,
										 NULL, 0, &threadID );
	Sleep( 1000 );

	/* Connect to the local server with several requests that should fail */
	status = connectCMPFail( 0 );
	if( status )
		status = connectCMPFail( 1 );
	if( status )
		status = connectCMPFail( 2 );
	waitForThread( hThread );
	destroyMutex();
	return( status );
	}
#endif /* WINDOWS_THREADS */

#endif /* TEST_SESSION || TEST_SESSION_LOOPBACK */
