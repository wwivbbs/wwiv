/****************************************************************************
*																			*
*					cryptlib Certificate Handling Test Routines				*
*						Copyright Peter Gutmann 1997-2009					*
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

extern BYTE FAR_BSS certBuffer[ BUFFER_SIZE ];

/****************************************************************************
*																			*
*							Certificate Processing Test						*
*																			*
****************************************************************************/

static const CERT_DATA FAR_BSS certRequestData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave Smith" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

static const CERT_DATA FAR_BSS certProcessData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave Smith" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* Re-select the subject name after poking around in the altName */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTNAME },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

/* Create a certification request */

static int createCertRequest( void *certRequest,
							  const CRYPT_ALGO_TYPE cryptAlgo,
							  const BOOLEAN useCRMF )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT cryptContext;
	int length DUMMY_INIT, status;

	/* Create a new key */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, cryptAlgo );
	if( cryptStatusError( status ) )
		return( status );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_LABEL,
							 TEXT( "Private key" ),
							 paramStrlen( TEXT( "Private key" ) ) );
	status = cryptGenerateKey( cryptContext );
	if( cryptStatusError( status ) )
		return( status );

	/* Create the certification request */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED, useCRMF ? \
				CRYPT_CERTTYPE_REQUEST_CERT : CRYPT_CERTTYPE_CERTREQUEST );
	if( cryptStatusError( status ) )
		return( status );
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cryptContext );
	if( cryptStatusError( status ) )
		return( status );
	if( !addCertFields( cryptCert, certRequestData, __LINE__ ) )
		return( -1 );
#ifndef _WIN32_WCE
	if( useCRMF )
		{
		const time_t startTime = time( NULL ) - 1000;
		const time_t endTime = time( NULL ) + 86400;

		/* Since we're using a CRMF request, set some fields that can't
		   be specified in the standard certificate request */
		status = cryptSetAttributeString( cryptCert,
					CRYPT_CERTINFO_VALIDFROM, &startTime, sizeof( time_t ) );
		if( cryptStatusOK( status ) )
			status = cryptSetAttributeString( cryptCert,
					CRYPT_CERTINFO_VALIDTO, &endTime, sizeof( time_t ) );
		}
#endif /* _WIN32_WCE */
	if( cryptStatusOK( status ) )
		status = cryptSignCert( cryptCert, cryptContext );
	if( cryptStatusOK( status ) )
		status = cryptExportCert( certRequest, BUFFER_SIZE, &length,
								  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusOK( status ) )
		status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		return( status );

	/* Clean up */
	cryptDestroyContext( cryptContext );
	return( length );
	}

/* Create a certificate from a certificate request */

static int createCertificate( void *certificate, const void *certRequest,
							  const int certReqLength,
							  const CRYPT_CONTEXT caKeyContext )
	{
	CRYPT_CERTIFICATE cryptCert, cryptCertRequest;
	int length, status;

	/* Import and verify the certification request */
	status = cryptImportCert( certRequest, certReqLength, CRYPT_UNUSED,
							  &cryptCertRequest );
	if( cryptStatusOK( status ) )
		status = cryptCheckCert( cryptCertRequest, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( status );

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_CERTREQUEST, cryptCertRequest );
	if( cryptStatusOK( status ) )
		status = cryptSignCert( cryptCert, caKeyContext );
	if( cryptStatusOK( status ) )
		status = cryptExportCert( certificate, BUFFER_SIZE, &length,
								  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusOK( status ) )
		status = cryptDestroyCert( cryptCert );

	/* Clean up */
	cryptDestroyCert( cryptCertRequest );
	return( ( cryptStatusOK( status ) ) ? length : status );
	}

/* Create a certificate directly, used for algorithms that don't support
   self-signed certificate requests */

static int createCertDirect( void *certificate,
							 const CRYPT_ALGO_TYPE cryptAlgo,
							 const CRYPT_CONTEXT caKeyContext )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT cryptContext;
	int length, status;

	/* Create a new key */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, cryptAlgo );
	if( cryptStatusError( status ) )
		return( status );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_LABEL,
							 TEXT( "Private key" ),
							 paramStrlen( TEXT( "Private key" ) ) );
	status = cryptGenerateKey( cryptContext );
	if( cryptStatusError( status ) )
		return( status );

	/* Create the certification */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cryptContext );
	if( cryptStatusError( status ) )
		return( status );
	if( !addCertFields( cryptCert, certProcessData, __LINE__ ) )
		return( FALSE );
	status = cryptSignCert( cryptCert, caKeyContext );
	if( cryptStatusOK( status ) )
		status = cryptExportCert( certificate, BUFFER_SIZE, &length,
								  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusOK( status ) )
		status = cryptDestroyCert( cryptCert );

	/* Clean up */
	cryptDestroyContext( cryptContext );
	return( ( cryptStatusOK( status ) ) ? length : status );
	}

/* Test the full certification process */

static int certProcess( const CRYPT_ALGO_TYPE cryptAlgo,
						const char *algoName,
						const CRYPT_CONTEXT cryptCAKey,
						const BOOLEAN useCRMF )
	{
	CRYPT_CERTIFICATE cryptCert;
	const char *certName = \
			( cryptAlgo == CRYPT_ALGO_RSA ) ? \
				( useCRMF ? "prcrtrsa_c" : "prcrtrsa" ) : \
			( cryptAlgo == CRYPT_ALGO_DSA ) ? "prcrtdsa" : \
			( cryptAlgo == CRYPT_ALGO_DH ) ? "prcrtdh" : \
			( cryptAlgo == CRYPT_ALGO_ELGAMAL ) ? "prcrtelg" : \
			( cryptAlgo == CRYPT_ALGO_ECDSA ) ? "prcrtecdsa" : \
			( cryptAlgo == CRYPT_ALGO_ECDH ) ? "prcrtecdh" : "prcrtxxx";
	int length, status;

	printf( "Testing %s certificate processing%s...\n", algoName,
			useCRMF ? " from CRMF request" : "" );

	/* Some algorithms can't create self-signed certificate requests so we 
	   have to create the certificate directly */
	if( cryptAlgo != CRYPT_ALGO_ELGAMAL && cryptAlgo != CRYPT_ALGO_DH && \
		cryptAlgo != CRYPT_ALGO_ECDH )
		{
		const char *reqName = \
			( cryptAlgo == CRYPT_ALGO_RSA ) ? \
				( useCRMF ? "prreqrsa_c" : "prreqrsa" ) : \
			( cryptAlgo == CRYPT_ALGO_DSA ) ? "prreqdsa" : \
			( cryptAlgo == CRYPT_ALGO_ECDSA ) ? "prreqecdsa" : "prreqxxx";

		/* Create the certification request */
		status = length = createCertRequest( certBuffer, cryptAlgo, useCRMF );
		if( cryptStatusError( status ) )
			{
			printf( "Certification request creation failed with error code "
					"%d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		debugDump( reqName, certBuffer, length );

		/* Create a certificate from the certification request */
		status = createCertificate( certBuffer, certBuffer, length,
									cryptCAKey );
		}
	else
		status = createCertDirect( certBuffer, cryptAlgo, cryptCAKey );
	if( cryptStatusError( status ) )
		{
		printf( "Certificate creation failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	length = status;
	debugDump( certName, certBuffer, length );

	/* Import the certificate and check its validity using the CA key (we use
	   the private key context since it's handy, in practice we should use
	   the public key certificate */
	status = cryptImportCert( certBuffer, length, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusOK( status ) )
		status = cryptCheckCert( cryptCert, cryptCAKey );
	if( cryptStatusError( status ) )
		{
		printf( "Certificate validation failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroyCert( cryptCert );
	printf( "%s certificate processing succeeded.\n\n", algoName );
	return( TRUE );
	}

int testCertProcess( void )
	{
	CRYPT_CONTEXT cryptCAKey;
	int value, status;

	/* Get the CA's private key */
	status = getPrivateKey( &cryptCAKey, CA_PRIVKEY_FILE,
							CA_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "CA private key read failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Test each PKC algorithm */
	if( !certProcess( CRYPT_ALGO_RSA, "RSA", cryptCAKey, FALSE ) )
		return( FALSE );
	if( !certProcess( CRYPT_ALGO_DSA, "DSA", cryptCAKey, FALSE ) )
		return( FALSE );
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ELGAMAL, NULL ) ) && \
		!certProcess( CRYPT_ALGO_ELGAMAL, "Elgamal", cryptCAKey, FALSE ) )
		return( FALSE );
	if( !certProcess( CRYPT_ALGO_DH, "Diffie-Hellman", cryptCAKey, FALSE ) )
		return( FALSE );
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ECDSA, NULL ) ) && \
		!certProcess( CRYPT_ALGO_ECDSA, "ECDSA", cryptCAKey, FALSE ) )
		return( FALSE );
#if 0
	/* We can't perform the test for ECDH because there's only one algorithm
	   type available for public keys, "ECC", and that doesn't distinguish
	   between an ECDH ECC key and an ECDSA ECC key.  Because of this the 
	   code assumes that any ECC key is an ECDSA key, so that while we can
	   create an ECDH certificate, we can't read it back because it'll be
	   read as an ECDSA certificate */
	if( !certProcess( CRYPT_ALGO_ECDH, "ECDH", cryptCAKey, FALSE ) )
		return( FALSE );
#endif /* 0 */

	/* Run the test again with a CRMF instead of PKCS #10 request */
	if( !certProcess( CRYPT_ALGO_RSA, "RSA", cryptCAKey, TRUE ) )
		return( FALSE );

	/* Now try a different hash algorithm */
	status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH, 
								&value );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH,
					   ( value == CRYPT_ALGO_SHA1 ) ? \
						 CRYPT_ALGO_SHA2 : CRYPT_ALGO_SHA1 );
	status = certProcess( CRYPT_ALGO_RSA, "RSA with non-default hash algorithm", 
						  cryptCAKey, FALSE );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH, value );
	if( !status )
		return( FALSE );

	/* Clean up */
	cryptDestroyContext( cryptCAKey );
	return( TRUE );
	}

/****************************************************************************
*																			*
*							CA Certificate Management Test					*
*																			*
****************************************************************************/

/* Since opening the certificate store for update creates a log entry each 
   time, we open it once at the start and then call a series of sub-tests 
   with the store open throughout the tests.  This also allows us to keep the
   CA key active througout */

static const CERT_DATA FAR_BSS cert1Data[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test user 1" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "test1@testusers.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* Re-select the subject name after poking around in the altName */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTNAME },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS revokableCert1Data[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Revoked cert user 1" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "test2@testusers.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* Re-select the subject name after poking around in the altName */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTNAME },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS revokableCert2Data[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Revoked cert user 2" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "revoked1@testusers.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* Re-select the subject name after poking around in the altName */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTNAME },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS expiredCert1Data[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Expired cert user 1" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "revoked2@testusers.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* Re-select the subject name after poking around in the altName */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTNAME },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS expiredCert2Data[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Expired cert user 2" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "expired2@testusers.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* Re-select the subject name after poking around in the altName */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTNAME },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

/* Certificate requests for which the issue should fail, for various 
   reasons */

static const CERT_DATA FAR_BSS certCA1Data[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test CA user 1" ) },

	/* CA extensions.  These should be rejected/stripped by the certificate
	   management code, since new CAs can only be created by the issuing CA
	   specifying it in the PKI user info */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC,
	  CRYPT_KEYUSAGE_KEYCERTSIGN | CRYPT_KEYUSAGE_CRLSIGN },
	{ CRYPT_CERTINFO_CA, IS_NUMERIC, TRUE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS certCA2Data[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test CA user 2" ) },

	/* CA extensions.  A variant of the above with no basicConstraints but a 
	   CA-only key usage */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC,
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE | CRYPT_KEYUSAGE_KEYCERTSIGN | \
	  CRYPT_KEYUSAGE_CRLSIGN },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
#ifdef USE_CERT_DNSTRING
static const CERT_DATA FAR_BSS certDodgyDNData[] = {
	/* Identification information.  The multi-AVA RDN should be rejected */
	{ CRYPT_CERTINFO_DN, IS_STRING, 0, 
	  TEXT( "cn=www.example.com + cn=www.wetaburgers.com, ou=Procurement, o=Wetaburgers, c=NZ" ) },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
#endif /* USE_CERT_DNSTRING */

/* Add a certification request to the certificate store */

static int addCertRequest( const CRYPT_KEYSET cryptCertStore,
						   const CERT_DATA *certReqData,
						   const BOOLEAN isExpired )
	{
	CRYPT_CONTEXT cryptContext;
	CRYPT_CERTIFICATE cryptCertRequest;
	int length, status;

	/* Generate a (short) key for the request */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 CRYPT_ALGO_RSA );
	if( cryptStatusError( status ) )
		return( status );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_LABEL,
							 TEXT( "Private key" ),
							 paramStrlen( TEXT( "Private key" ) ) );
	status = cryptGenerateKey( cryptContext );
	if( cryptStatusError( status ) )
		{
		printf( "Creation of private key for certificate failed with error "
				"code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Create the certification request.  If we're adding an expiry time
	   we have to make it a CRMF request since a standard request can't
	   handle this */
	status = cryptCreateCert( &cryptCertRequest, CRYPT_UNUSED, isExpired ? \
					CRYPT_CERTTYPE_REQUEST_CERT : CRYPT_CERTTYPE_CERTREQUEST );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptCertRequest,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cryptContext );
#ifndef _WIN32_WCE
	if( cryptStatusOK( status ) && isExpired )
		{
		const time_t theTime = time( NULL ) + 5;

		/* Set the expiry time to a few seconds after the current time to
		   ensure that the certificate has expired by the time we need it.  
		   This is a tiny bit risky since it requires that the interval 
		   between setting this attribute and the creation of the certificate 
		   below is less than five seconds, however there's no easy way to 
		   guarantee the creation of a pre-expired certificate since if we 
		   set the time too far back it won't be created */
		status = cryptSetAttributeString( cryptCertRequest,
					CRYPT_CERTINFO_VALIDTO, &theTime, sizeof( time_t ) );
		}
#endif /* _WIN32_WCE */
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertRequest, "cryptSetAttribute()",
							   status, __LINE__ ) );
	if( !addCertFields( cryptCertRequest, certReqData, __LINE__ ) )
		{
		/* We have to make sure that we exit cleanly here since some of the 
		   tests verify the rejection of invalid request data, so a failure
		   at this point isn't a true error for these tests */
		cryptDestroyCert( cryptCertRequest );
		cryptDestroyContext( cryptContext );
		return( FALSE );
		}
	status = cryptSignCert( cryptCertRequest, cryptContext );
	cryptDestroyContext( cryptContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertRequest, "cryptSignCert()",
							   status, __LINE__ ) );

	/* Export the request, destroy it, and recreate it by importing it again.
	   This is just a pedantic check to make sure that we emulate exactly a
	   real-world scenario of an externally-obtained request */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &length,
							  CRYPT_CERTFORMAT_CERTIFICATE,
							  cryptCertRequest );
	cryptDestroyCert( cryptCertRequest );
	if( cryptStatusOK( status ) )
		status = cryptImportCert( certBuffer, length, CRYPT_UNUSED,
								  &cryptCertRequest );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't export/re-import certificate request, status = "
				"%d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Add the request to the certificate store */
	status = cryptCAAddItem( cryptCertStore, cryptCertRequest );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptCAAddItem()", status,
							  __LINE__ ) );

	return( cryptCertRequest );
	}

/* Add a revocation request to the certificate store.  This code isn't 
   currently used because CMP doesn't allow revocation requests to be 
   signed so we can't create a signed object to add directly but have to 
   come in via CMP */

#if 0

static int addRevRequest( const CRYPT_KEYSET cryptCertStore,
						  const CERT_DATA *certReqData )
	{
	CRYPT_CERTIFICATE cryptCert, cryptCertRequest;
	int i, status;

	/* Find the CN of the certificate we're revoking and use it to fetch the 
	   certificate */
	for( i = 0; certReqData[ i ].type != CRYPT_ATTRIBUTE_NONE; i++ )
		if( certReqData[ i ].type == CRYPT_CERTINFO_COMMONNAME )
			printf( "Revoking certificate for '%s'.\n",
					( char * ) certReqData[ i ].stringValue );
	status = cryptGetPublicKey( cryptCertStore, &cryptCert, CRYPT_KEYID_NAME,
								certReqData[ i ].stringValue );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptGetPublicKey()", status,
							  __LINE__ ) );

	/* Create the revocation request */
	status = cryptCreateCert( &cryptCertRequest, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_REQUEST_REVOCATION );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptCertRequest, CRYPT_CERTINFO_CERTIFICATE,
								cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertRequest, "cryptSetAttribute()",
							   status, __LINE__ ) );
	if( !addCertFields( cryptCertRequest, revRequestData, __LINE__ ) )
		return( FALSE );

	/* Add the request to the certificate store */
	status = cryptCAAddItem( cryptCertStore, cryptCertRequest );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptCAAddItem()", status,
							  __LINE__ ) );

	return( cryptCertRequest );
	}
#endif /* 0 */

/* Issue a certificate from a certificate request, and try and issue an
   invalid certificate from a request, for which the issue process should 
   fail */

static int issueCert( const CRYPT_KEYSET cryptCertStore,
					  const CRYPT_CONTEXT cryptCAKey,
					  const CERT_DATA *certReqData, 
					  const BOOLEAN isExpired )
	{
	CRYPT_CERTIFICATE cryptCertRequest;
	int i, status;

	/* Provide some feedback on what we're doing */
	for( i = 0; certReqData[ i ].type != CRYPT_ATTRIBUTE_NONE; i++ )
		{
		if( certReqData[ i ].type == CRYPT_CERTINFO_COMMONNAME )
			{
			printf( "Issuing certificate for '%s'.\n",
					( char * ) certReqData[ i ].stringValue );
			break;
			}
		}

	/* Issue the certificate via the certificate store */
	cryptCertRequest = addCertRequest( cryptCertStore, certReqData, isExpired );
	if( !cryptCertRequest )
		return( FALSE );
	status = cryptCACertManagement( NULL, CRYPT_CERTACTION_ISSUE_CERT,
									cryptCertStore, cryptCAKey,
									cryptCertRequest );
	cryptDestroyCert( cryptCertRequest );
	if( cryptStatusError( status ) )
		{
		if( isExpired && status == CRYPT_ERROR_INVALID )
			{
			puts( "The short-expiry-time certificate has already expired at "
				  "the time of issue.\nThis happened because there was a "
				  "delay of more than 5s between adding the\nrequest and "
				  "issuing the certificate for it.  Try re-running the test "
				  "on a\nless-heavily-loaded system, or increase the expiry "
				  "delay to more than 5s." );
			return( FALSE );
			}
		return( extErrorExit( cryptCertStore, "cryptCACertManagement()",
							  status, __LINE__ ) );
		}

	return( TRUE );
	}

static int checkInvalidIssueRejected( const CRYPT_KEYSET cryptCertStore,
									  const CRYPT_CONTEXT cryptCAKey,
									  const CERT_DATA *certReqData,
									  const BOOLEAN requestCreationShouldFail )
	{
	CRYPT_CERTIFICATE cryptCertRequest;
	int i, status;

	/* Provide some feedback on what we're doing */
	for( i = 0; certReqData[ i ].type != CRYPT_ATTRIBUTE_NONE; i++ )
		{
		if( certReqData[ i ].type == CRYPT_CERTINFO_COMMONNAME )
			{
			printf( "Issuing certificate for '%s'.\n",
					( char * ) certReqData[ i ].stringValue );
			break;
			}
		}
	if( certReqData[ i ].type == CRYPT_ATTRIBUTE_NONE )
		{
		/* If the certificate doesn't have a CN attribute then it's one with
		   a synthetic DN created to test the DN-validity checks */
		puts( "Issuing certificate for synthetic invalid DN." );
		}

	/* Issue the certificate via the certificate store */
	cryptCertRequest = addCertRequest( cryptCertStore, certReqData, FALSE );
	if( !cryptCertRequest )
		{
		/* In some cases the invalid request will be caught as soon as it's 
		   created (this is a bit unfortunate because we really want to 
		   check whether an import of someone else's invalid request will
		   fail, however this is checked indirectly because it demonstrates
		   that the certificate code won't allow the use of such a value in
		   a certificate */
		if( requestCreationShouldFail )
			{
			puts( "  (This is an expected result since this test verifies "
				  "handling of\n   invalid request data)." );
			return( TRUE );
			}
		return( FALSE );
		}
	status = cryptCACertManagement( NULL, CRYPT_CERTACTION_ISSUE_CERT,
									cryptCertStore, cryptCAKey,
									cryptCertRequest );
	cryptDestroyCert( cryptCertRequest );
	if( cryptStatusError( status ) )
		{
		/* This is a check of the request validity-checking system so the 
		   issue is supposed to fail */
		return( TRUE );
		}

	return( FALSE );
	}

/* Issue a CRL.  Although we can't do this directly (see the comment above
   for the revocation request code) we can at least test the ability to
   create an empty CRL (and if the CMP code has been run there will probably
   be a few revocation entries present to fill the CRL) */

static int issueCRL( const CRYPT_KEYSET cryptCertStore,
					 const CRYPT_CONTEXT cryptCAKey )
	{
	CRYPT_CERTIFICATE cryptCRL;
	int noEntries = 0, status;

	/* Issue the CRL via the certificate store */
	status = cryptCACertManagement( &cryptCRL, CRYPT_CERTACTION_ISSUE_CRL,
									cryptCertStore, cryptCAKey,
									CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptCACertManagement()",
							  status, __LINE__ ) );

	/* Print information on the CRL */
	if( cryptStatusOK( cryptSetAttribute( cryptCRL,
										  CRYPT_CERTINFO_CURRENT_CERTIFICATE,
										  CRYPT_CURSOR_FIRST ) ) )
		do
			noEntries++;
		while( cryptSetAttribute( cryptCRL,
								  CRYPT_CERTINFO_CURRENT_CERTIFICATE,
								  CRYPT_CURSOR_NEXT ) == CRYPT_OK );
	printf( "CRL has %d entr%s.\n", noEntries,
			( noEntries == 1 ) ? "y" : "ies" );
	if( !noEntries )
		puts( "  (This is probably because there haven't been any revocation "
			  "entries added\n   via the CMP test yet)." );

	/* Clean up */
	cryptDestroyCert( cryptCRL );
	return( TRUE );
	}

/* Fetch the issued certificate that was created from a given cert template */

static CRYPT_CERTIFICATE getCertFromTemplate( const CRYPT_KEYSET cryptCertStore,
											  const CERT_DATA *certReqData )
	{
	CRYPT_CERTIFICATE cryptCert;
	int i, status;

	for( i = 0; certReqData[ i ].type != CRYPT_ATTRIBUTE_NONE; i++ )
		{
		if( certReqData[ i ].type == CRYPT_CERTINFO_COMMONNAME )
			break;
		}
	status = cryptGetPublicKey( cryptCertStore, &cryptCert, CRYPT_KEYID_NAME,
							    certReqData[ i ].stringValue );
	return( cryptStatusOK( status ) ? cryptCert : status );
	}

int testCertManagement( void )
	{
	CRYPT_CERTIFICATE cryptCert, cryptCertRequest;
	CRYPT_CONTEXT cryptCAKey;
	CRYPT_KEYSET cryptCertStore;
	time_t certTime;
	int dummy, status;

	puts( "Testing certificate management using certificate store..." );

	/* Get the CA's private key */
	status = getPrivateKey( &cryptCAKey, CA_PRIVKEY_FILE,
							CA_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "CA private key read failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Create the certificate store keyset with a check to make sure that 
	   this access method exists so we can return an appropriate error 
	   message.  If the database table already exists, this will return a 
	   duplicate data error so we retry the open with no flags to open the 
	   existing database keyset for write access */
	status = cryptKeysetOpen( &cryptCertStore, CRYPT_UNUSED,
							  CERTSTORE_KEYSET_TYPE, CERTSTORE_KEYSET_NAME,
							  CRYPT_KEYOPT_CREATE );
	if( cryptStatusOK( status ) )
		puts( "Created new certificate store '" CERTSTORE_KEYSET_NAME_ASCII
			  "'." );
	if( status == CRYPT_ERROR_PARAM3 )
		{
		/* This type of keyset access isn't available, return a special error
		   code to indicate that the test wasn't performed, but that this
		   isn't a reason to abort processing */
		cryptDestroyContext( cryptCAKey );
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( status == CRYPT_ERROR_DUPLICATE )
		status = cryptKeysetOpen( &cryptCertStore, CRYPT_UNUSED,
								  CERTSTORE_KEYSET_TYPE, CERTSTORE_KEYSET_NAME,
								  CRYPT_KEYOPT_NONE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetOpen() failed with error code %d, line %d.\n",
				status, __LINE__ );
		if( status == CRYPT_ERROR_OPEN )
			{
			cryptDestroyContext( cryptCAKey );
			return( CRYPT_ERROR_FAILED );
			}
		return( FALSE );
		}

	/* Create a certificate request, add it to the store, and destroy it, 
	   simulating a delayed certificate issue in which the request can't 
	   immediately be converted into a certificate.  Then read the request 
	   back from the store and issue a certificate based on it */
	puts( "Issuing certificate for 'Test user 1'..." );
	cryptCertRequest = addCertRequest( cryptCertStore, cert1Data, FALSE );
	if( !cryptCertRequest )
		return( FALSE );
	cryptDestroyCert( cryptCertRequest );
	status = cryptCAGetItem( cryptCertStore, &cryptCertRequest,
							 CRYPT_CERTTYPE_REQUEST_CERT, CRYPT_KEYID_NAME,
							 TEXT( "Test user 1" ) );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptCAGetItem()", status,
							  __LINE__ ) );
	status = cryptCACertManagement( &cryptCert, CRYPT_CERTACTION_ISSUE_CERT,
									cryptCertStore, cryptCAKey,
									cryptCertRequest );
	cryptDestroyCert( cryptCertRequest );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptCACertManagement()",
							  status, __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* Issue some more certs, this time directly from the request and without
	   bothering to obtain the resulting certificate.  The first two have a 
	   validity time that expires in a few seconds so that we can use them 
	   to test certificate expiry processing, we issue these first to ensure 
	   that as much time as possible passes due to other operations occurring 
	   before we run the expiry.  The second two are for revocation and CRL 
	   testing */
	if( !issueCert( cryptCertStore, cryptCAKey, expiredCert1Data, TRUE ) )
		return( FALSE );
	if( !issueCert( cryptCertStore, cryptCAKey, expiredCert2Data, TRUE ) )
		return( FALSE );
	if( !issueCert( cryptCertStore, cryptCAKey, revokableCert1Data, FALSE ) )
		return( FALSE );
	if( !issueCert( cryptCertStore, cryptCAKey, revokableCert2Data, FALSE ) )
		return( FALSE );

	/* The following tests are specifically inserted at this point (rather
	   than at some other point in the test run) because they'll add some
	   further delay before the expiry operation */

	/* Try and get a CA certificate issued.  This should fail, since new CAs 
	   can only be created if the issuing CA specifies it (either directly 
	   when it creates the certificate manually or via the PKI user info), 
	   but never at the request of the user */
	if( !checkInvalidIssueRejected( cryptCertStore, cryptCAKey, 
									certCA1Data, TRUE ) || \
		!checkInvalidIssueRejected( cryptCertStore, cryptCAKey, 
									certCA2Data, FALSE ) )
		{
		printf( "Issue of certificate from invalid request succeeded when "
				"it should have failed,\nline %d.\n", __LINE__ );
		return( FALSE );
		}

#ifdef USE_CERT_DNSTRING
	/* Try and get a certificate with a multi-AVA RDN issued.  This should
	   fail, since such DN shenanigans can only be performed if the CA 
	   creates the required DN directly */
	if( !checkInvalidIssueRejected( cryptCertStore, cryptCAKey, 
									certDodgyDNData, FALSE ) )
		{
		printf( "Issue of certificate from invalid request succeeded when "
				"it should have failed,\nline %d.\n", __LINE__ );
		return( FALSE );
		}
#endif /* USE_CERT_DNSTRING */

	/* Get a certificate and (to-be-)revoked certificate from the store and 
	   save them to disk for later tests */
	status = cryptCert = getCertFromTemplate( cryptCertStore, cert1Data );
	if( !cryptStatusError( status ) )
		{
		BYTE fileName[ BUFFER_SIZE ];
		FILE *filePtr;
		int length;

		/* First save the CA certificate */
		filenameFromTemplate( fileName, OCSP_CA_FILE_TEMPLATE, 1 );
		status = cryptExportCert( certBuffer, BUFFER_SIZE, &length,
								  CRYPT_CERTFORMAT_CERTIFICATE, 
								  cryptCAKey );
		if( cryptStatusOK( status ) && \
			( filePtr = fopen( fileName, "wb" ) ) != NULL )
			{
			int count;

			count = fwrite( certBuffer, 1, length, filePtr );
			fclose( filePtr );
			if( count < length )
				{
				remove( fileName );
				puts( "Warning: Couldn't save OCSP CA certificate to disk, "
					  "this will cause later\n         OCSP server tests to "
					  "fail.  Press a key to continue." );
				getchar();
				}
			}

		/* Then the EE certificate */
		filenameFromTemplate( fileName, OCSP_EEOK_FILE_TEMPLATE, 1 );
		status = cryptExportCert( certBuffer, BUFFER_SIZE, &length,
								  CRYPT_CERTFORMAT_CERTIFICATE, 
								  cryptCert );
		if( cryptStatusOK( status ) && \
			( filePtr = fopen( fileName, "wb" ) ) != NULL )
			{
			int count;

			count = fwrite( certBuffer, 1, length, filePtr );
			fclose( filePtr );
			if( count < length )
				{
				remove( fileName );
				puts( "Warning: Couldn't save OCSP non-revoked certificate "
					  "to disk, this will cause later\n         OCSP server "
					  "tests to fail.  Press a key to continue." );
				getchar();
				}
			}
		cryptDestroyCert( cryptCert );
		}
	if( !cryptStatusError( status ) )
		status = cryptCert = getCertFromTemplate( cryptCertStore,
												  revokableCert1Data );
	if( !cryptStatusError( status ) )
		{
		BYTE fileName[ BUFFER_SIZE ];
		FILE *filePtr;
		int length;

		filenameFromTemplate( fileName, OCSP_EEREV_FILE_TEMPLATE, 1 );
		status = cryptExportCert( certBuffer, BUFFER_SIZE, &length,
								  CRYPT_CERTFORMAT_CERTIFICATE, 
								  cryptCert );
		if( cryptStatusOK( status ) && \
			( filePtr = fopen( fileName, "wb" ) ) != NULL )
			{
			int count;

			count = fwrite( certBuffer, 1, length, filePtr );
			fclose( filePtr );
			if( count < length )
				{
				remove( fileName );
				puts( "Warning: Couldn't save OCSP revoked certificate "
					  "to disk, this will cause later\n         OCSP server "
					  "tests to fail.  Press a key to continue." );
				getchar();
				}
			}
		cryptDestroyCert( cryptCert );
		}
	if( cryptStatusError( status ) )
		{
		puts( "Issued certificates couldn't be fetched from the certificate "
			  "store and written\nto disk, the OCSP server test will abort "
			  "when it fails to find\nthese certificates." );
		}

	/* Issue a CRL.  This will probably be a zero-length CRL unless we've run
	   the CMP tests because we can't directly revoke a certificate.  Again, 
	   we perform it before the expiry test because it'll add some further
	   delay */
	if( !issueCRL( cryptCertStore, cryptCAKey ) )
		return( FALSE );

	/* Get the most recent of the expired certs and wait for it to expire
	   if necessary */
	status = cryptCert = getCertFromTemplate( cryptCertStore,
											  expiredCert1Data );
	if( !cryptStatusError( status ) )
		status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_VALIDTO,
										  &certTime, &dummy );
	if( cryptStatusError( status ) )
		{
		puts( "Couldn't get expiry information for expired certificate." );
		return( FALSE );
		}
#ifndef _WIN32_WCE
	if( certTime >= time( NULL ) )
		{
		printf( "Waiting for certificates to expire.." );
		while( certTime >= time( NULL ) )
			{
			delayThread( 1 );
			printf( "." );
			}
		puts( " done." );
		}
#endif /* _WIN32_WCE */
	cryptDestroyCert( cryptCert );

	/* Expire the certs */
	puts( "Expiring certificates..." );
	status = cryptCACertManagement( NULL, CRYPT_CERTACTION_EXPIRE_CERT,
									cryptCertStore, CRYPT_UNUSED,
									CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptCACertManagement()",
							  status, __LINE__ ) );

	/* Clean up */
	cryptDestroyContext( cryptCAKey );
	cryptKeysetClose( cryptCertStore );
	puts( "Certificate management using certificate store succeeded.\n" );
	return( TRUE );
	}
