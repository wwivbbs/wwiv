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

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Handle various types of certificate-processing errors */

static BOOLEAN handleCertImportError( const int errorCode, const int lineNo )
	{
	printf( "cryptImportCert() failed with error code %d, line %d.\n",
			errorCode, lineNo );
	return( FALSE );
	}

/****************************************************************************
*																			*
*						Certificate Import Routines Test					*
*																			*
****************************************************************************/

/* Test certificate import code */

static BOOLEAN handleCertError( const CRYPT_CERTIFICATE cryptCert, 
								const int certNo, const int errorCode )
	{
	int errorLocus, status;

	printf( "\n" );
	status = cryptGetAttribute( cryptCert, CRYPT_ATTRIBUTE_ERRORLOCUS, 
								&errorLocus );
	if( cryptStatusError( status ) )
		{
		puts( "Couldn't get error locus for certificate check failure." );
		return( FALSE );
		}

	/* Make sure that we don't fail just because the certificate that we're 
	   using as a test has expired */
	if( errorLocus == CRYPT_CERTINFO_VALIDTO )
		{
		puts( "Warning: Validity check failed because the certificate has "
			  "expired." );
		return( TRUE );
		}

	/* RegTP CA certs are marked as non-CA certs, report the problem and 
	   continue */
	if( certNo == 3 && errorLocus == CRYPT_CERTINFO_CA )
		{
		puts( "Warning: Validity check failed due to RegTP CA certificate "
			  "incorrectly\n         marked as non-CA certificate." );
		return( TRUE );
		}

	/* Certificate #26 has an invalid keyUsage for the key it contains, it's
	   used in order to check for the ability to handle a non-hole BIT 
	   STRING in a location where a hole encoding is normally used so we 
	   don't care about this particular problem */
	if( certNo == 26 && errorLocus == CRYPT_CERTINFO_KEYUSAGE )
		{
		puts( "Warning: Validity check failed due to CA certificate with "
			  "incorrect\n         key usage field (this will be ignored "
			  "since the certificate\n         is used to test for other "
			  "error handling conditions)." );
		return( TRUE );
		}

	return( FALSE );
	}

static int certImport( const int certNo, const BOOLEAN isECC, 
					   const BOOLEAN isBase64 )
	{
	CRYPT_CERTIFICATE cryptCert;
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ];
	int count, value, status;

	printf( "Testing %scertificate #%d import...\n",
			isECC ? "ECC " : isBase64 ? "base64 " : "", certNo );
	filenameFromTemplate( buffer, isECC ? ECC_CERT_FILE_TEMPLATE : \
								  isBase64 ? BASE64CERT_FILE_TEMPLATE : \
											 CERT_FILE_TEMPLATE, certNo );
	if( ( filePtr = fopen( buffer, "rb" ) ) == NULL )
		{
		puts( "Couldn't find certificate file for import test." );
		return( FALSE );
		}
	count = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );

	/* Import the certificate */
	status = cryptImportCert( buffer, count, CRYPT_UNUSED,
							  &cryptCert );
	if( status == CRYPT_ERROR_NOSECURE && !( isECC || isBase64 ) && \
		( certNo == 9 || certNo == 10 ) )	/* 9 = 512-bit, 10 = P12 512-bit */
		{
		/* Some older certs use totally insecure 512-bit keys and can't be
		   processed unless we deliberately allow insecure keys.  
		   Unfortunately this also blocks out the certificate that's used to 
		   check the ability to handle invalid PKCS #1 padding, since this 
		   only uses a 512-bit key, but if necessary it can be tested by 
		   lowering MIN_PKCSIZE when building cryptlib */
		puts( "Warning: Certificate import failed because the certificate "
			  "uses a very short\n         (insecure) key.\n" );
		return( TRUE );
		}
	if( status == CRYPT_ERROR_BADDATA && !( isECC || isBase64 ) \
		&& certNo == 3 )
		{
		puts( "Warning: Certificate import failed for RegTP/Deutsche "
			  "Telekom CA\n         certificate with negative public-key "
			  "values.\n" );
		return( TRUE );
		}
	if( status == CRYPT_ERROR_NOTAVAIL && !( isECC || isBase64 ) && \
		certNo == 21 )
		{
		/* This is an ECDSA certificate, the algorithm isn't enabled by 
		   default */
		puts( "Warning: Certificate import failed because the certificate "
			  "uses an\n         algorithm that isn't enabled in this build "
			  "of cryptlib.\n" );
		return( TRUE );
		}
	if( status == CRYPT_ERROR_BADDATA && !( isECC || isBase64 ) \
		&& certNo == 31 )
		{
		/* This certificate has has the algoID in the signature altered to 
		   make it invalid, since this isn't covered by the signature it
		   isn't detected by many implementations */
		puts( "Certificate import failed for certificate with manipulated "
			  "signature data." );
		puts( "  (This is the correct result for this test).\n" );
		return( TRUE );
		}
	if( status == CRYPT_ERROR_BADDATA && isBase64 && \
		( certNo == 3 || certNo == 4 ) )
		{
		/* These certificates claim to be in PEM format but have a single 
		   continuous block of base64 data, one with and one without
		   the base64 termination characters */
		puts( "Certificate import failed for certificate with invalid PEM "
			  "encoding." );
		puts( "  (This is the correct result for this test).\n" );
		return( TRUE );
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() for certificate #%d failed with error "
				"code %d, line %d.\n", certNo, status, __LINE__ );
		return( FALSE );
		}
	status = cryptGetAttribute( cryptCert, CRYPT_CERTINFO_SELFSIGNED,
								&value );
	if( cryptStatusError( status ) )
		{
		/* Sanity check to make sure that the certificate internal state is
		   consistent - this should never happen */
		printf( "Couldn't get certificate self-signed status, status %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( value )
		{
		printf( "Certificate is self-signed, checking signature... " );
		status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
		if( cryptStatusError( status ) )
			{
			if( !handleCertError( cryptCert, certNo, status ) )
				{
				return( attrErrorExit( cryptCert, "cryptCheckCert()", 
									   status, __LINE__ ) );
				}
			}
		else
			puts( "signature verified." );
		}
	else
		puts( "Certificate is signed, signature key unknown." );

	/* Print information on what we've got */
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Perform a dummy generalised extension read to make sure that nothing
	   goes wrong for this */
	status = cryptGetCertExtension( cryptCert, "1.2.3.4", &value, NULL, 0, 
									&count );
	if( status != CRYPT_ERROR_NOTFOUND )
		{
		printf( "Read of dummy extension didn't fail with "
				"CRYPT_ERROR_NOTFOUND, status %d, line %d.\n", status, 
				__LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroyCert( cryptCert );
	puts( "Certificate import succeeded.\n" );
	return( TRUE );
	}

#if 0	/* Test rig for NISCC certificate data */

static void importTestData( void )
	{
	int i;

	for( i = 1; i <= 110000; i++ )
		{
		CRYPT_CERTIFICATE cryptCert;
		FILE *filePtr;
		BYTE buffer[ BUFFER_SIZE ];
		int count, status;

		if( !( i % 100 ) )
			printf( "%06d\r", i );
/*		filenameFromTemplate( buffer, "/tmp/simple_client/%08d", i ); */
/*		filenameFromTemplate( buffer, "/tmp/simple_server/%08d", i ); */
		filenameFromTemplate( buffer, "/tmp/simple_rootca/%08d", i );
		if( ( filePtr = fopen( buffer, "rb" ) ) == NULL )
			break;
		count = fread( buffer, 1, BUFFER_SIZE, filePtr );
		fclose( filePtr );
		status = cryptImportCert( buffer, count, CRYPT_UNUSED,
								  &cryptCert );
		if( cryptStatusOK( status ) )
			cryptDestroyCert( cryptCert );
		}
	}
#endif /* 0 */

int testCertImport( void )
	{
	int i;

	for( i = 1; i <= 32; i++ )
		{
		if( !certImport( i, FALSE, FALSE ) )
			return( FALSE );
		}
	return( TRUE );
	}

int testCertImportECC( void )
	{
	int i;

	if( cryptQueryCapability( CRYPT_ALGO_ECDSA, NULL ) == CRYPT_ERROR_NOTAVAIL )
		{
		puts( "ECC algorithm support appears to be disabled, skipping "
			  "processing of ECDSA\ncertificates.\n" );
		return( TRUE );
		}

	for( i = 1; i <= 10; i++ )
		{
		if( !certImport( i, TRUE, FALSE ) )
			return( FALSE );
		}
	return( TRUE );
	}

static int certReqImport( const int certNo )
	{
	CRYPT_CERTIFICATE cryptCert;
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ];
	int count, complianceValue, status;

	printf( "Testing certificate request #%d import...\n", certNo );
	filenameFromTemplate( buffer, CERTREQ_FILE_TEMPLATE, certNo );
	if( ( filePtr = fopen( buffer, "rb" ) ) == NULL )
		{
		puts( "Couldn't find certificate file for import test." );
		return( FALSE );
		}
	count = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );

	/* Import the certificate request and check that the signature is valid */
	if( certNo == 3 )
		{
		/* Some of the requests are broken and we have to set the compliance
		   level to oblivious to handle them */
		cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   &complianceValue );
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   CRYPT_COMPLIANCELEVEL_OBLIVIOUS );
		}
	status = cryptImportCert( buffer, count, CRYPT_UNUSED,
							  &cryptCert );
	if( certNo == 3 )
		{
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   complianceValue );
		}
#ifdef __UNIX__
	if( status == CRYPT_ERROR_NOTAVAIL || status == CRYPT_ERROR_BADDATA )
		{
		puts( "The certificate request import failed, probably because "
			  "you're using an\nolder version of unzip that corrupts "
			  "certain types of files when it\nextracts them.  To fix this, "
			  "you need to re-extract test/*.der without\nusing the -a "
			  "option to convert text files.\n" );
		return( TRUE );		/* Skip this test and continue */
		}
#endif /* __UNIX__ */
	if( status == CRYPT_ERROR_NOSECURE && certNo == 1 )
		{
		puts( "Warning: Certificate request import failed because the "
			  "request uses a very short\n         (insecure) key." );
		return( TRUE );
		}
	if( cryptStatusError( status ) )
		return( handleCertImportError( status, __LINE__ ) );
	if( certNo == 5 )
		puts( "  (Skipping signature check because CRMF data is unsigned)." );
	else
		{
		printf( "Checking signature... " );
		status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptCert, "cryptCheckCert()", status, 
								   __LINE__ ) );
		puts( "signature verified." );
		}

	/* Print information on what we've got */
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Clean up */
	cryptDestroyCert( cryptCert );
	puts( "Certificate request import succeeded.\n" );
	return( TRUE );
	}

int testCertReqImport( void )
	{
	int i;

	for( i = 1; i <= 7; i++ )
		{
		if( !certReqImport( i ) )
			return( FALSE );
		}
	return( TRUE );
	}

#define LARGE_CRL_SIZE	45000	/* Large CRL is too big for std.buffer */

static int crlImport( const int crlNo, BYTE *buffer )
	{
	CRYPT_CERTIFICATE cryptCert;
	FILE *filePtr;
	int count, status;

	filenameFromTemplate( buffer, CRL_FILE_TEMPLATE, crlNo );
	if( ( filePtr = fopen( buffer, "rb" ) ) == NULL )
		{
		printf( "Couldn't find CRL file for CRL #%d import test.\n", crlNo );
		return( FALSE );
		}
	count = fread( buffer, 1, LARGE_CRL_SIZE, filePtr );
	fclose( filePtr );
	printf( "CRL #%d has size %d bytes.\n", crlNo, count );

	/* Import the CRL.  Since CRL's don't include the signing certificate, 
	   we can't (easily) check the signature on it */
	status = cryptImportCert( buffer, count, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		return( handleCertImportError( status, __LINE__ ) );

	/* Print information on what we've got and clean up */
	if( !printCertInfo( cryptCert ) )
		return( FALSE );
	cryptDestroyCert( cryptCert );

	return( TRUE );
	}

int testCRLImport( void )
	{
	BYTE *bufPtr;
	int i;

	puts( "Testing CRL import..." );

	/* Since we're working with an unusually large certificate object we 
	   have to dynamically allocate the buffer for it */
	if( ( bufPtr = malloc( LARGE_CRL_SIZE ) ) == NULL )
		{
		puts( "Out of memory." );
		return( FALSE );
		}
	for( i = 1; i <= 4; i++ )
		{
		if( !crlImport( i, bufPtr ) )
			{
			free( bufPtr );
			return( FALSE );
			}
		}

	/* Clean up */
	free( bufPtr );
	puts( "CRL import succeeded.\n" );
	return( TRUE );
	}

static BOOLEAN isSingleCert( const CRYPT_CERTIFICATE cryptCertChain )
	{
	int value, status;

	/* Check whether a certificate chain contains a single non-self-signed 
	   certificate, which means that we can't perform a signature check on 
	   it */
	status = cryptGetAttribute( cryptCertChain, CRYPT_CERTINFO_SELFSIGNED, 
								&value );
	if( cryptStatusOK( status ) && value )
		{
		/* It's a self-signed certificate, we should be able to check this 
		   chain */
		return( FALSE );
		}
	cryptSetAttribute( cryptCertChain, CRYPT_CERTINFO_CURRENT_CERTIFICATE,
					   CRYPT_CURSOR_FIRST );
	if( cryptSetAttribute( cryptCertChain,
						   CRYPT_CERTINFO_CURRENT_CERTIFICATE,
						   CRYPT_CURSOR_NEXT ) == CRYPT_ERROR_NOTFOUND )
		{
		/* There's only a single certificate in the chain and it's not self-
		   signed, we can't check it */
		return( TRUE );
		}

	return( FALSE );
	}

static int checkExpiredCertChain( const CRYPT_CERTIFICATE cryptCertChain )
	{
	int complianceValue, status;

	printf( "Warning: The certificate chain didn't verify because one or "
			"more\n         certificates in it have expired.  Trying again "
			"in oblivious\n         mode... " );
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   &complianceValue );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   CRYPT_COMPLIANCELEVEL_OBLIVIOUS );
	status = cryptCheckCert( cryptCertChain, CRYPT_UNUSED );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   complianceValue );

	return( status );
	}

static BOOLEAN handleCertChainError( const CRYPT_CERTIFICATE cryptCertChain, 
									 const int certNo, const int errorCode )
	{
	int trustValue = CRYPT_UNUSED, errorLocus, status;

	/* If the chain contains a single non-CA certificate, we'll get a 
	   parameter error since we haven't supplied a signing certificate */
	if( errorCode == CRYPT_ERROR_PARAM2 && isSingleCert( cryptCertChain ) )
		{
		/* There's only a single certificate present, we can't do much with 
		   it */
		puts( "\nCertificate chain contains only a single standalone "
			  "certificate, skipping\nsignature check..." );
		return( TRUE );
		}

	/* If it's not a problem with validity, we can't go any further */
	if( errorCode != CRYPT_ERROR_INVALID )
		{
		return( attrErrorExit( cryptCertChain, "cryptCheckCert()", 
							   errorCode, __LINE__ ) );
		}

	/* Check the nature of the problem */
	status = cryptGetAttribute( cryptCertChain, CRYPT_ATTRIBUTE_ERRORLOCUS,
								&errorLocus );
	if( cryptStatusError( status ) )
		{
		puts( "Couldn't get error locus for certificate check failure." );
		return( FALSE );
		}

	/* Try to work around the error */
	status = errorCode;
	if( errorLocus == CRYPT_CERTINFO_TRUSTED_IMPLICIT || \
		errorLocus == CRYPT_CERTINFO_TRUSTED_USAGE )
		{
		/* The error occured because of a problem with the root certificate, 
		   try again with an implicitly-trusted root */
		if( errorLocus == CRYPT_CERTINFO_TRUSTED_IMPLICIT )
			{
			printf( "\nWarning: The certificate chain didn't verify "
					"because it didn't end in a\n         trusted root "
					"certificate.  Checking again using an "
					"implicitly\n         trusted root... " );
			}
		else
			{
			printf( "\nWarning: The certificate chain didn't verify "
					"because the root certificate's\n         key isn't "
					"enabled for this usage.  Checking again using "
					"an\n         implicitly trusted root... " );
			}
		if( cryptStatusError( \
				setRootTrust( cryptCertChain, &trustValue, 1 ) ) )
			{
			printf( "\nAttempt to make chain root implicitly trusted "
					"failed, status = %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptCheckCert( cryptCertChain, CRYPT_UNUSED );
		if( status == CRYPT_ERROR_INVALID )
			cryptGetAttribute( cryptCertChain, CRYPT_ATTRIBUTE_ERRORLOCUS,
							   &errorLocus );
		}
	if( errorLocus == CRYPT_CERTINFO_VALIDTO )
		{
		/* One (or more) certs in the chain have expired, try again with the 
		   compliance level wound down to nothing */
		putchar( '\n' );
		status = checkExpiredCertChain( cryptCertChain );
		if( status == CRYPT_ERROR_PARAM2 && isSingleCert( cryptCertChain ) )
			{
			/* There's only a single certificate present, we can't do much 
			   with it */
			puts( "\nCertificate chain contains only a single standalone "
				  "certificate, skipping\nsignature check..." );
			return( TRUE );
			}
		}
	if( errorLocus == CRYPT_CERTINFO_CERTIFICATE )
		{
		puts( "\nCertificate chain is incomplete (one or more certificates "
			  "needed to\ncomplete the chain are missing), skipping "
			  "signature check..." );
		return( TRUE );
		}

	/* If we changed settings, restore their original values */
	if( trustValue != CRYPT_UNUSED )
		setRootTrust( cryptCertChain, NULL, trustValue );

	/* If we've got a long-enough chain, try again with the next-to-last 
	   certificate marked as trusted */
	if( cryptStatusOK( status ) && certNo == 4 )
		{
		puts( "signatures verified." );
		puts( "Checking again with an intermediate certificate marked as "
			  "trusted... " );
		status = cryptSetAttribute( cryptCertChain,
									CRYPT_CERTINFO_CURRENT_CERTIFICATE,
									CRYPT_CURSOR_LAST );
		if( cryptStatusOK( status ) )
			status = cryptSetAttribute( cryptCertChain,
										CRYPT_CERTINFO_CURRENT_CERTIFICATE,
										CRYPT_CURSOR_PREVIOUS );
		if( cryptStatusOK( status ) )
			status = cryptSetAttribute( cryptCertChain,
										CRYPT_CERTINFO_TRUSTED_IMPLICIT, 1 );
		if( cryptStatusError( status ) )
			return( status );
		status = cryptCheckCert( cryptCertChain, CRYPT_UNUSED );
		if( status == CRYPT_ERROR_INVALID )
			{
			cryptGetAttribute( cryptCertChain, CRYPT_ATTRIBUTE_ERRORLOCUS,
							   &errorLocus );
			if( errorLocus == CRYPT_CERTINFO_VALIDTO )
				{
				/* One (or more) certs in the chain have expired, try again 
				   with the compliance level wound down to nothing */
				status = checkExpiredCertChain( cryptCertChain );
				}
			}
		if( trustValue != CRYPT_UNUSED )
			{
			cryptSetAttribute( cryptCertChain,
							   CRYPT_CERTINFO_TRUSTED_IMPLICIT, 
							   trustValue );
			}
		}

	/* If the lowered-limits check still didn't work, it's an error */
	if( cryptStatusError( status ) )
		{
		putchar( '\n' );
		return( attrErrorExit( cryptCertChain, "cryptCheckCert()", status, 
							   __LINE__ ) );
		}

	puts( "signatures verified." );
	return( TRUE );
	}

static int certChainImport( const int certNo, const BOOLEAN isBase64 )
	{
	CRYPT_CERTIFICATE cryptCertChain;
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ];
	int count, value, status;

	printf( "Testing %scert chain #%d import...\n",
			isBase64 ? "base64 " : "", certNo );
	filenameFromTemplate( buffer, isBase64 ? BASE64CERTCHAIN_FILE_TEMPLATE : \
											 CERTCHAIN_FILE_TEMPLATE, certNo );
	if( ( filePtr = fopen( buffer, "rb" ) ) == NULL )
		{
		puts( "Couldn't find certificate chain file for import test." );
		return( FALSE );
		}
	count = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );
	if( count == BUFFER_SIZE )
		{
		puts( "The certificate buffer size is too small for the certificate "
			  "chain.  To fix\nthis, increase the BUFFER_SIZE value in "
			  "test/testcert.c and recompile the code." );
		return( TRUE );		/* Skip this test and continue */
		}
	printf( "Certificate chain has size %d bytes.\n", count );

	/* Import the certificate chain.  This assumes that the default certs are
	   installed as trusted certs, which is required for cryptCheckCert() */
	status = cryptImportCert( buffer, count, CRYPT_UNUSED,
							  &cryptCertChain );
	if( cryptStatusError( status ) )
		{
		/* If we failed on the RSA e=3 certificate, this is a valid result */
		if( certNo == 2 && status == CRYPT_ERROR_BADDATA )
			{
			printf( "Import of certificate with invalid e=3 key failed, "
					"line %d.\n", __LINE__ );
			puts( "  (This is the correct result for this test).\n" );
			return( TRUE );
			}
		return( handleCertImportError( status, __LINE__ ) );
		}
	if( certNo == 2 )
		{
		printf( "Import of certificate with invalid e=3 key succeeded when "
				"it should have\n  failed, line %d.\n", __LINE__ );
		return( FALSE );
		}
	status = cryptGetAttribute( cryptCertChain, CRYPT_CERTINFO_CERTTYPE, 
								&value );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertChain, "cryptGetAttribute()", 
							   status, __LINE__ ) );
	if( certNo == 1 )
		{
		/* The first chain has length 1 so it should be imported as a 
		   standard certificate */
		if( value != CRYPT_CERTTYPE_CERTIFICATE )
			{
			printf( "Imported object isn't a certificate, line %d.\n", 
					__LINE__ );
			return( FALSE );
			}
		}
	else
		{
		if( value != CRYPT_CERTTYPE_CERTCHAIN )
			{
			printf( "Imported object isn't a certificate chain, line %d.\n", 
					__LINE__ );
			return( FALSE );
			}
		}
	printf( "Checking signatures... " );
	status = cryptCheckCert( cryptCertChain, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		{
		if( !handleCertChainError( cryptCertChain, certNo, status ) )
			return( FALSE );
		}
	else
		puts( "signatures verified." );

	/* Display info on each certificate in the chain */
	if( !printCertChainInfo( cryptCertChain ) )
		return( FALSE );

	/* Clean up */
	cryptDestroyCert( cryptCertChain );
	puts( "Certificate chain import succeeded.\n" );
	return( TRUE );
	}

int testCertChainImport( void )
	{
	int i;

	for( i = 1; i <= 5; i++ )
		{
		if( !certChainImport( i, FALSE ) )
			return( FALSE );
		}
	return( TRUE );
	}

int testOCSPImport( void )
	{
	CRYPT_CERTIFICATE cryptCert, cryptResponderCert;
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ];
	int count, status;

	if( ( filePtr = fopen( convertFileName( OCSP_OK_FILE ), "rb" ) ) == NULL )
		{
		puts( "Couldn't find OCSP OK response file for import test." );
		return( FALSE );
		}
	puts( "Testing OCSP OK response import..." );
	count = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );
	printf( "OCSP OK response has size %d bytes.\n", count );

	/* Import the OCSP OK response.  Because of the choose-your-own-trust-
	   model status of the OCSP RFC we have to supply our own signature
	   check certificate to verify the response */
	status = cryptImportCert( buffer, count, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		return( handleCertImportError( status, __LINE__ ) );
	printf( "Checking signature... " );
	status = importCertFile( &cryptResponderCert, OCSP_CA_FILE );
	if( cryptStatusOK( status ) )
		{
		status = cryptCheckCert( cryptCert, cryptResponderCert );
		cryptDestroyCert( cryptResponderCert );
		}
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );
	puts( "signatures verified." );

	/* Print information on what we've got */
	if( !printCertInfo( cryptCert ) )
		return( FALSE );
	cryptDestroyCert( cryptCert );

	/* Now import the OCSP revoked response.  This has a different CA 
	   certificate than the OK response, to keep things simple we don't 
	   bother with a sig check for this one */
	puts( "Testing OCSP revoked response import..." );
	if( ( filePtr = fopen( convertFileName( OCSP_REV_FILE ), "rb" ) ) == NULL )
		{
		puts( "Couldn't find OCSP revoked response file for import test." );
		return( FALSE );
		}
	count = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );
	printf( "OCSP revoked response has size %d bytes.\n", count );
	status = cryptImportCert( buffer, count, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		return( handleCertImportError( status, __LINE__ ) );

	/* Print information on what we've got */
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Clean up */
	cryptDestroyCert( cryptCert );
	puts( "OCSP import succeeded.\n" );
	return( TRUE );
	}

int testBase64CertImport( void )
	{
	int i;

	/* If this is an EBCDIC system, we can't (easily) import the base64-
	   encoded certificate without complex calisthenics to handle the 
	   different character sets */
#if 'A' == 0xC1
	puts( "Skipping import of base64-encoded data on EBCDIC system.\n" );
	return( TRUE );
#endif /* EBCDIC system */

	for( i = 1; i <= 4; i++ )
		{
		if( !certImport( i, FALSE, TRUE ) )
			return( FALSE );
		}
	return( TRUE );
	}

int testBase64CertChainImport( void )
	{
	int i;

	/* If this is an EBCDIC system, we can't (easily) import the base64-
	   encoded certificate without complex calisthenics to handle the 
	   different character sets */
#if 'A' == 0xC1
	puts( "Skipping import of base64-encoded data on EBCDIC system.\n" );
	return( TRUE );
#endif /* EBCDIC system */

	for( i = 1; i <= 1; i++ )
		if( !certChainImport( i, TRUE ) )
			return( FALSE );
	return( TRUE );
	}

static int miscImport( const char *fileName, const char *description )
	{
	CRYPT_CERTIFICATE cryptCert;
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ];
	int count, status;

	if( ( filePtr = fopen( fileName, "rb" ) ) == NULL )
		{
		printf( "Couldn't find file for %s key import test.\n",
				description );
		return( FALSE );
		}
	count = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );

	/* Import the object.  Since this isn't a certificate we can't do much
	   more with it than this - this is only used to test the low-level
	   code and needs to be run inside a debugger, since the call always
	   fails (the data being imported isn't a certificate) */
	status = cryptImportCert( buffer, count, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) && status != CRYPT_ERROR_BADDATA )
		{
		printf( "cryptImportCert() for %s key failed with error code %d, "
				"line %d.\n", description, status, __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroyCert( cryptCert );
	return( TRUE );
	}

int testMiscImport( void )
	{
	BYTE buffer[ BUFFER_SIZE ];
	int i;

	puts( "Testing base64-encoded SSH/PGP key import..." );
	for( i = 1; i <= 2; i++ )
		{
		filenameFromTemplate( buffer, SSHKEY_FILE_TEMPLATE, i );
		if( !miscImport( buffer, "SSH" ) )
			return( FALSE );
		}
	for( i = 1; i <= 3; i++ )
		{
		filenameFromTemplate( buffer, PGPASCKEY_FILE_TEMPLATE, i );
		if( !miscImport( buffer, "PGP" ) )
			return( FALSE );
		}
	puts( "Import succeeded.\n" );
	return( TRUE );
	}

/* Test handling of certs that chain by DN but not by keyID */

int testNonchainCert( void )
	{
	/* The EE certificate expired in November 2007 so unfortunately we can't 
	   perform this test any more until we can obtain further broken
	   certs from DigiCert */
#if 0
	CRYPT_CERTIFICATE cryptLeafCert, cryptCACert;
	int value, status;

	puts( "Testing handling of incorrectly chained certs..." );

	/* Since this test requires the use of attributes that aren't decoded at
	   the default compliance level, we have to raise it a notch to make sure
	   that we get the certificate attributes necessary to sort out the 
	   mess */
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   &value );
	if( value < CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL )
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL );

	/* Get the EE and incorrectly chained CA certs */
	status = importCertFile( &cryptLeafCert, NOCHAIN_EE_FILE );
	if( cryptStatusOK( status ) )
		status = importCertFile( &cryptCACert, NOCHAIN_CA_FILE );
	if( cryptStatusError( status ) )
		return( FALSE );

	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   value );

	/* Check the EE certificate using the apparently-correct but actually 
	   incorrect CA certificate and make sure that we get the correct error 
	   message */
	status = cryptCheckCert( cryptLeafCert, cryptCACert );
	if( status != CRYPT_ERROR_SIGNATURE )
		{
		printf( "Sig.check of incorrectly chained certificate returned %d, "
				"should have been %d, line %d.\n", status, 
				CRYPT_ERROR_SIGNATURE, __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroyCert( cryptLeafCert );
	cryptDestroyCert( cryptCACert );

	puts( "Handling of incorrectly chained certs succeeded.\n" );
#endif /* 0 */
	return( TRUE );
	}

/* Test certificate handling at various levels of compliance */

int testCertComplianceLevel( void )
	{
	CRYPT_CERTIFICATE cryptCert, cryptCaCert = DUMMY_INIT;
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ];
	int count, value, status;

	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   &value );

	/* Test import of a broken certificate.  The brokenness is an invalid 
	   authorityKeyIdentifier which is processed at level
	   CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL and above, so first we try it in 
	   CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL mode, which should fail, and then
	   again in oblivious mode, which should succeed */
	printf( "Testing certificate handling at various compliance levels "
			"(current = %d)...\n", value );
	if( ( filePtr = fopen( convertFileName( BROKEN_CERT_FILE ), "rb" ) ) == NULL )
		{
		puts( "Couldn't certificate for import test." );
		return( FALSE );
		}
	count = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );
	if( value < CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL )
		{
		status = cryptSetAttribute( CRYPT_UNUSED, 
									CRYPT_OPTION_CERT_COMPLIANCELEVEL,
									CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL );
		if( status == CRYPT_ERROR_PARAM3 )
			{
			puts( "(Couldn't set compliance level to "
				  "CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL, probably\n because "
				  "cryptlib has been configured to not use this level, "
				  "skipping this\n test...)" );
			}
		else
			{
			status = cryptImportCert( buffer, count, CRYPT_UNUSED,
								  &cryptCert );
			}
		if( cryptStatusOK( status ) )
			{
			/* Import in CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL mode should 
			   fail */
			cryptSetAttribute( CRYPT_UNUSED, 
							   CRYPT_OPTION_CERT_COMPLIANCELEVEL, value );
			printf( "cryptImportCert() of broken certificate succeeded when "
					"it should have failed, line %d.\n", __LINE__ );
			return( FALSE );
			}
		}
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   CRYPT_COMPLIANCELEVEL_STANDARD );
	status = cryptImportCert( buffer, count, CRYPT_UNUSED,
							  &cryptCert );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   value );
	if( cryptStatusError( status ) )
		{
		/* Import in standard mode should succeed */
		return( handleCertImportError( status, __LINE__ ) );
		}

	/* Print information on what we've got.  This should only print info for
	   the two basic extensions that are handled in oblivious mode  */
	if( !printCertInfo( cryptCert ) )
		return( FALSE );
	cryptDestroyCert( cryptCert );

	/* Test checking of an expired certificate using a broken CA certificate 
	   in oblivious mode (this checks chaining and the signature, but little 
	   else) */
	status = importCertFile( &cryptCert, BROKEN_USER_CERT_FILE );
	if( cryptStatusOK( status ) )
		status = importCertFile( &cryptCaCert, BROKEN_CA_CERT_FILE );
	if( cryptStatusError( status ) )
		return( handleCertImportError( status, __LINE__ ) );
	status = cryptCheckCert( cryptCert, cryptCaCert );
	if( cryptStatusOK( status ) )
		{
		/* Checking in normal mode should fail */
		printf( "cryptCheckCert() of broken certificate succeeded when it "
				"should have failed, line %d.\n", __LINE__ );
		return( FALSE );
		}
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   CRYPT_COMPLIANCELEVEL_OBLIVIOUS );
	status = cryptCheckCert( cryptCert, cryptCaCert );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   value );
	if( cryptStatusError( status ) )
		{
		/* Checking in oblivious mode should succeed */
		printf( "cryptCheckCert() of broken certificate failed when it "
				"should have succeeded, line %d.\n", __LINE__ );
		return( FALSE );
		}
	cryptDestroyCert( cryptCaCert );
	cryptDestroyCert( cryptCert );

	/* Clean up */
	puts( "Certificate handling at different compliance levels succeeded.\n" );
	return( TRUE );
	}

/****************************************************************************
*																			*
*							NIST Path-processing Test						*
*																			*
****************************************************************************/

/* Test path processing using the NIST PKI test suite.  This doesn't run all
   of the tests since some are somewhat redundant (e.g. path length 
   constraints ending at certificate n in a chain vs.cert n+1 in a chain 
   where both are well short of the constraint length) or require complex
   additional processing (e.g. CRL fetches) which it's difficult to
   automate */

typedef struct {
	const int fileMajor, fileMinor;	/* Major and minor number of file */
	const BOOLEAN isValid;			/* Whether path is valid */
	const BOOLEAN policyOptional;	/* Whether explicit policy optional */
	} PATH_TEST_INFO;

static const PATH_TEST_INFO FAR_BSS pathTestInfo[] = {
	/* Signature verification */
	/*   0 */ { 1, 1, TRUE },
	/*   1 */ { 1, 2, FALSE },
	/*   2 */ { 1, 3, FALSE },
	/*   3 */ { 1, 4, TRUE },
	/*   4 */ { 1, 6, FALSE },

	/* Validity periods */
	/*   5 */ { 2, 1, FALSE },
	/*   6 */ { 2, 2, FALSE },
	/* The second certificate in test 4.2.3 has a validFrom date of 1950 
	   which cryptlib rejects on import as being not even remotely valid (it 
	   can't even be represented in the ANSI/ISO C date format).  Supposedly 
	   half-century-old certs are symptomatic of severely broken software so
	   rejecting this certificate is justified */
/*	**  X ** { 2, 3, TRUE }, */
	/*   7 */ { 2, 4, TRUE },
	/*   8 */ { 2, 5, FALSE },
	/*   9 */ { 2, 6, FALSE },
	/*  10 */ { 2, 7, FALSE },
	/*  11 */ { 2, 8, TRUE },

	/* Name chaining */
	/*  12 */ { 3, 1, FALSE },
	/*  13 */ { 3, 2, FALSE },
	/*  14 */ { 3, 6, TRUE },
	/*  15 */ { 3, 7, TRUE },
	/*  16 */ { 3, 8, TRUE },
	/*  17 */ { 3, 9, TRUE },

	/* 4 = CRLs */

	/* oldWithNew / newWithOld */
	/*  18 */ { 5, 1, TRUE },
	/*  19 */ { 5, 3, TRUE },

	/* Basic constraints */
	/*  20 */ { 6, 1, FALSE },
	/*  21 */ { 6, 2, FALSE },
	/*  22 */ { 6, 3, FALSE },
	/*  23 */ { 6, 4, TRUE },
	/*  24 */ { 6, 5, FALSE },
	/*  25 */ { 6, 6, FALSE },
	/*  26 */ { 6, 7, TRUE },
	/* The second-to-last certificate in the path sets a pathLenConstraint of 
	   zero with the next certificate being a CA certificate (there's no EE 
	   certificate present).  cryptlib treats this as invalid since it can 
	   never lead to a valid path once the EE certificate is added */
	/*  27 */ { 6, 8, FALSE /* TRUE */ },
	/*  28 */ { 6, 9, FALSE },
	/*  29 */ { 6, 10, FALSE },

	/*  30 */ { 6, 11, FALSE },
	/*  31 */ { 6, 12, FALSE },
	/*  32 */ { 6, 13, TRUE },
	/* This has the same problem as 4.6.8 */
	/*  33 */ { 6, 14, FALSE /* TRUE */ },
	/* The following are 4.5.x-style oldWithNew / newWithOld but with path
	   constraints */
	/*  34 */ { 6, 15, TRUE },
	/*  35 */ { 6, 16, FALSE },
	/*  36 */ { 6, 17, TRUE },

	/* Key usage */
	/*  37 */ { 7, 1, FALSE },
	/*  38 */ { 7, 2, FALSE },
	/*  39 */ { 7, 3, TRUE },

	/* Policies */
	/*  40 */ { 8, 1, TRUE },
	/*  41 */ { 8, 2, TRUE },
	/* The first certificate asserts a policy that differs from that of all 
	   other certificates in the path.  If no explicit policy is required 
	   (by setting CRYPT_OPTION_REQUIREPOLICY to FALSE) it will verify, 
	   otherwise it won't */
	/*  42 */ { 8, 3, TRUE, TRUE },	/* Policy optional */
	/*  43 */ { 8, 3, FALSE },
	/*  44 */ { 8, 4, FALSE },
	/*  45 */ { 8, 5, FALSE },
	/*  46 */ { 8, 6, TRUE },
	/* The false -> true changes below (4.8.7, 4.8.8, 4.8.9) occur because 
	   cryptlib takes its initial policy from the first CA certificate with 
	   a policy that it finds.  This is due to real-world issues where re-
	   parented certificate chains due to re-sold CA root keys end up with 
	   different policies in the root and the next-level-down pseudo-root 
	   and problems where the root is an extension-less X.509v1 certificate.
	   The same issue crops up in the 4.10.* / 4.11.* tests further down */
	/*  47 */ { 8, 7, /* FALSE */ TRUE },
	/*  48 */ { 8, 8, /* FALSE */ TRUE },
	/*  49 */ { 8, 9, /* FALSE */ TRUE },
	/*  50 */ { 8, 10, TRUE },
	/*  51 */ { 8, 11, TRUE },
	/*  52 */ { 8, 12, FALSE },
	/*  53 */ { 8, 13, TRUE },
	/*  54 */ { 8, 14, TRUE },
	/*  55 */ { 8, 15, TRUE },
	/*  56 */ { 8, 20, TRUE },

	/* Policy constraints.  For these tests policy handling is dictated by
	   policy constraints so we don't require explicit policies */
	/*  57 */ { 9, 1, TRUE },
	/*  58 */ { 9, 2, TRUE, TRUE },
	/* The NIST test value for this one is wrong.  RFC 3280 section 4.2.1.12
	   says:

		If the requireExplicitPolicy field is present, the value of
		requireExplicitPolicy indicates the number of additional
		certificates that may appear in the path before an explicit policy
		is required for the entire path.  When an explicit policy is
		required, it is necessary for all certificates in the path to
		contain an acceptable policy identifier in the certificate policies
		extension.

	   Test 4.9.3 has requireExplicitPolicy = 4 in a chain of 4 certs for
	   which the last one has no policy.  NIST claims this shouldn't
	   validate, which is incorrect */
	/*  59 */ { 9, 3, TRUE /* FALSE */, TRUE },
	/*  60 */ { 9, 4, TRUE, TRUE },
	/*  61 */ { 9, 5, FALSE, TRUE },
	/*  62 */ { 9, 6, TRUE, TRUE },
	/*  63 */ { 9, 7, FALSE, TRUE },
	/*  64 */ { 9, 8, FALSE, TRUE },

	/* 10, 11 = Policy mappings.  The false -> true changes below (4.10.2,
	   4.10.4) occur because cryptlib takes its initial policy from the 
	   first CA certificate with a policy that it finds.  This is due to 
	   real-world issues where re-parented certificate chains due to re-sold 
	   CA root keys end up with different policies in the root and the next-
	   level-down pseudo-root and problems where the root is an extension-
	   less X.509v1 certificate */
	/*  65 */ { 10, 1, TRUE },
	/*  66 */ { 10, 2, /* FALSE */ TRUE },
	/*  67 */ { 10, 3, TRUE },
	/*  68 */ { 10, 4, /* FALSE */ TRUE },
	/*  69 */ { 10, 5, TRUE },
	/*  70 */ { 10, 6, TRUE },
	/*  71 */ { 10, 7, FALSE },
	/*  72 */ { 10, 8, FALSE },
	/*  73 */ { 10, 9, TRUE },
	/* The test for 4.10.10 is a special case because it contains  the 
	   retroactively triggered requireExplicitPolicy extension, see the long 
	   comment in chk_chn.c for a discussion of this, for now we just 
	   disable the check by recording it as a value-true check because even 
	   the standards committee can't explain why it does what it does */
	/*  74 */ { 10, 10, /* FALSE */ TRUE },
	/*  75 */ { 10, 11, TRUE },
	/*  76 */ { 10, 12, TRUE },
	/*  77 */ { 10, 13, TRUE },
	/*  78 */ { 10, 14, TRUE },

	/* Policy inhibitPolicy.  The false -> true changes below (4.11.1) are 
	   as for the 10.x tests */
	/*  79 */ { 11, 1, /* FALSE */ TRUE },
	/* The NIST test value for 4.11.2 is wrong, the top-level CA 
	   certificate sets inhibitPolicyMapping to 1, the mid-level CA 
	   certificate maps policy 1 to policy 3, and the EE certificte asserts
	   policy 3, however at this point inhibitPolicyMapping is in effect
	   and policy 3 is no longer valid */
	/*  80 */ { 11, 2, /* TRUE */ FALSE },
	/*  81 */ { 11, 3, FALSE },
	/* The NIST test value for 4.11.4 is wrong for the same reason as for 
	   4.11.2, except that the point of failure is a second sub-CA rather 
	   than the EE */
	/*  82 */ { 11, 4, /* TRUE */ FALSE },
	/*  83 */ { 11, 5, FALSE },
	/*  84 */ { 11, 6, FALSE },
	/* The NIST test value for 4.11.7 is wrong for the same reason as for 
	   4.11.2, except that the mapping is from policy 1 to policy 2 instead 
	   of policy 3 */
	/*  85 */ { 11, 7, /* TRUE */ FALSE },
	/*  86 */ { 11, 8, FALSE },
	/*  87 */ { 11, 9, FALSE },
	/*  88 */ { 11, 10, FALSE },
	/*  89 */ { 11, 11, FALSE },

	/* Policy inhibitAny */
	/*  90 */ { 12, 1, FALSE },
	/*  91 */ { 12, 2, TRUE },
	/*  92 */ { 12, 3, TRUE },
	/*  93 */ { 12, 4, FALSE },
	/*  94 */ { 12, 5, FALSE },
	/*  95 */ { 12, 6, FALSE },
	/* The NIST test results for 4.12.7 and 4.12.9 are wrong or more 
	   specifically the PKIX spec is wrong, contradicting itself in the body 
	   of the spec and the path-processing pseudocode in that there's no 
	   path-kludge exception for policy constraints in the body but there is 
	   one in the pseudocode.  Since these chains contain path-kludge certs 
	   the paths are invalid - they would only be valid if there was a path-
	   kludge exception for inhibitAnyPolicy.  Note that 4.9.7 and 4.9.8 
	   have the same conditions for requireExplicitPolicy but this time the 
	   NIST test results go the other way.  So although the PKIX spec is 
	   wrong the NIST test is also wrong in that it applies an inconsistent 
	   interpretation of the contradictions in the PKIX spec */
	/*  96 */ { 12, 7, FALSE /* TRUE */ },
	/*  97 */ { 12, 8, FALSE },
	/*  98 */ { 12, 9, FALSE /* TRUE */ },
	/*  99 */ { 12, 10, FALSE },

	/* Name constraints */
	/* 100 */ { 13, 1, TRUE },
	/* 101 */ { 13, 2, FALSE },
	/* 102 */ { 13, 3, FALSE },
	/* 103 */ { 13, 4, TRUE },
	/* 104 */ { 13, 5, TRUE },
	/* 105 */ { 13, 6, TRUE },
	/* 106 */ { 13, 7, FALSE },
	/* 107 */ { 13, 8, FALSE },
	/* 108 */ { 13, 9, FALSE },
	/* 109 */ { 13, 10, FALSE },
	/* 110 */ { 13, 11, TRUE },
	/* 111 */ { 13, 12, FALSE },
	/* 112 */ { 13, 13, FALSE },
	/* 113 */ { 13, 14, TRUE },
	/* 114 */ { 13, 15, FALSE },
	/* 115 */ { 13, 16, FALSE },
	/* 116 */ { 13, 17, FALSE },
	/* 117 */ { 13, 18, TRUE },
	/* 118 */ { 13, 19, TRUE },
	/* 119 */ { 13, 20, FALSE },
	/* 120 */ { 13, 21, TRUE },
	/* 121 */ { 13, 22, FALSE },
	/* 122 */ { 13, 23, TRUE },
	/* 123 */ { 13, 24, FALSE },
	/* 124 */ { 13, 25, TRUE },
	/* 125 */ { 13, 26, FALSE },
	/* 126 */ { 13, 27, TRUE },
	/* 127 */ { 13, 28, FALSE },
	/* 188 */ { 13, 29, FALSE },
	/* 129 */ { 13, 30, TRUE },
	/* 130 */ { 13, 31, FALSE },
	/* 131 */ { 13, 32, TRUE },
	/* 132 */ { 13, 33, FALSE },
	/* 133 */ { 13, 34, TRUE },
	/* 134 */ { 13, 35, FALSE },
	/* 135 */ { 13, 36, TRUE },
	/* 136 */ { 13, 37, FALSE },
	/* The NIST test results for 4.13.38 are wrong.  PKIX section 4.2.1.11
	   says:

		DNS name restrictions are expressed as foo.bar.com.  Any DNS name
		that can be constructed by simply adding to the left hand side of
		the name satisfies the name constraint.  For example,
		www.foo.bar.com would satisfy the constraint but foo1.bar.com would
		not.

	   The permitted subtree is testcertificates.gov and the altName is
	   mytestcertificates.gov which satisfies the above rule so the path
	   should be valid and not invalid */
	/* 137 */ { 13, 38, TRUE /* FALSE */ },

	/* 14, 15 = CRLs */

	/* Private certificate extensions */
	/* 138 */ { 16, 1, TRUE },
	/* 139 */ { 16, 2, FALSE },
	{ 0, 0 }
	};

static int testPath( const PATH_TEST_INFO *pathInfo )
	{
	CRYPT_CERTIFICATE cryptCertPath;
	char pathName[ 64 ];
	int pathNo, requirePolicy, status;

	/* Convert the composite path info into a single number used for fetching
	   the corresponding data file */
	sprintf( pathName, "4%d%d", pathInfo->fileMajor, pathInfo->fileMinor );
	pathNo = atoi( pathName );

	/* Test the path */
	sprintf( pathName, "4.%d.%d", pathInfo->fileMajor, pathInfo->fileMinor );
	printf( "  Path %s%s...", pathName, pathInfo->policyOptional ? \
			" without explicit policy" : "" );
	status = importCertFromTemplate( &cryptCertPath,
									 PATHTEST_FILE_TEMPLATE, pathNo );
	if( cryptStatusError( status ) )
		{
		printf( "Certificate import for test path %s failed, status %d, "
				"line %d.\n", pathName, status, __LINE__ );
		return( FALSE );
		}
	if( pathInfo->policyOptional )
		{
		/* By default we require policy chaining, for some tests we can turn
		   this off to check non-explict policy processing */
		cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_REQUIREPOLICY,
						   &requirePolicy );
		assert( requirePolicy != FALSE );
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_REQUIREPOLICY,
						   FALSE );
		status = cryptCheckCert( cryptCertPath, CRYPT_UNUSED );
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_REQUIREPOLICY,
						   requirePolicy );
		}
	else
		status = cryptCheckCert( cryptCertPath, CRYPT_UNUSED );
	if( pathInfo->isValid )
		{
		if( cryptStatusError( status ) )
			{
			puts( " didn't verify even though it should be valid." );
			return( attrErrorExit( cryptCertPath, "cryptCheckCert()",
								   status, __LINE__ ) );
			}
		}
	else
		{
		if( cryptStatusOK( status ) )
			{
			puts( " verified even though it should have failed." );
			return( FALSE );
			}
		}
	puts( " succeeded." );
	cryptDestroyCert( cryptCertPath );

	return( TRUE );
	}

int testPathProcessing( void )
	{
	CRYPT_CERTIFICATE cryptRootCert;
	int certTrust = DUMMY_INIT, complianceLevel, i, status;

	puts( "Testing path processing..." );

	/* Get the root certificate and make it implicitly trusted and crank the
	   compliance level up to maximum, since we're going to be testing some
	   pretty obscure extensions */
	status = importCertFromTemplate( &cryptRootCert,
									 PATHTEST_FILE_TEMPLATE, 0 );
	if( cryptStatusOK( status ) )
		status = setRootTrust( cryptRootCert, &certTrust, 1 );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't create trusted root certificate for path "
				"processing, line %d.\n", __LINE__ );
		return( FALSE );
		}
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   &complianceLevel );
	status = cryptSetAttribute( CRYPT_UNUSED, 
								CRYPT_OPTION_CERT_COMPLIANCELEVEL,
								CRYPT_COMPLIANCELEVEL_PKIX_FULL );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't set compliance level to "
				"CRYPT_COMPLIANCELEVEL_PKIX_FULL, line %d.\nThis may be "
				"because cryptlib has been configured not to run at this "
				"level.\n", __LINE__ );
		return( FALSE );
		}

	/* Process each certificate path and make sure that it succeeds or fails 
	   as required */
	for( i = 0; pathTestInfo[ i ].fileMajor != 0; i++ )
		{
		if( !testPath( &pathTestInfo[ i ] ) )
			break;
		}
	setRootTrust( cryptRootCert, NULL, certTrust );
	cryptDestroyCert( cryptRootCert );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   complianceLevel );
	if( pathTestInfo[ i ].fileMajor )
		return( FALSE );

	puts( "Path processing succeeded." );
	return( TRUE );
	}

/****************************************************************************
*																			*
*							Miscellaneous Tests								*
*																			*
****************************************************************************/

/* Test handling of invalid PKCS #1 padding in certificate signatures.  Note 
   that running this test properly requires disabling the PKCS#1 padding 
   format check in decodePKCS1() in mechs/mech_sig.c since the signatures 
   have such an obviously dodgy format that they don't even make it past the 
   basic padding sanity check */

int testPKCS1Padding( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	int i, complianceValue, status;

	puts( "Testing invalid PKCS #1 padding handling..." );

	/* The test certs don't have a keyUsage present in a CA certificate so 
	   we have to lower the compliance level to be able to get past this 
	   check to the signatures */
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   &complianceValue );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   CRYPT_COMPLIANCELEVEL_OBLIVIOUS );
	for( i = 1; i <= 11; i++ )
		{
		status = importCertFromTemplate( &cryptCert, PADTEST_FILE_TEMPLATE,
										 i );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't import certificate for PKCS #1 padding check, "
					"status %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
		if( cryptStatusOK( status ) )
			{
			printf( "Certificate with bad PKSC #1 padding verified, should "
					"have failed, line %d.\n", __LINE__ );
			return( FALSE );
			}
		cryptDestroyCert( cryptCert );
		}
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   complianceValue );

	puts( "Padding handling succeeded (all certs rejected).\n" );
	return( TRUE );
	}

/* Generic test routines used for debugging.  These are only meant to be
   used interactively, and throw exceptions rather than returning status
   values */

int xxxCertImport( const char *fileName )
	{
	CRYPT_CERTIFICATE cryptCert;
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ], *bufPtr = buffer;
	long length, count;
	int status;

	filePtr = fopen( fileName, "rb" );
	assert( filePtr != NULL );
	fseek( filePtr, 0L, SEEK_END );
	length = ftell( filePtr );
	fseek( filePtr, 0L, SEEK_SET );
	if( length > BUFFER_SIZE )
		{
		bufPtr = malloc( length );
		assert( bufPtr != NULL );
		}
	count = fread( bufPtr, 1, length, filePtr );
	assert( count == length );
	fclose( filePtr );
	status = cryptImportCert( bufPtr, count, CRYPT_UNUSED, &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "Certificate import failed, status = %d.\n", status );
		assert( cryptStatusOK( status ) );
		if( bufPtr != buffer )
			free( bufPtr );
		return( FALSE );
		}
	if( bufPtr != buffer )
		free( bufPtr );
	printCertChainInfo( cryptCert );
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );	/* Opportunistic only */
	if( cryptStatusError( status ) )
		{
		printf( "(Opportunistic) certificate check failed, status = %d.\n", 
				status );
		printErrorAttributeInfo( cryptCert );
		}
	cryptDestroyCert( cryptCert );

	return( cryptStatusOK( status ) ? TRUE : FALSE );
	}

int xxxCertCheck( const C_STR certFileName, const C_STR caFileNameOpt )
	{
	CRYPT_CERTIFICATE cryptCert, cryptCaCert;
	int status;

	status = importCertFile( &cryptCert, certFileName );
	if( cryptStatusError( status ) )
		{
		printf( "Certificate import failed, status = %d.\n", status );
		assert( cryptStatusOK( status ) );
		return( FALSE );
		}
	if( caFileNameOpt == NULL )
		{
		/* It's a self-signed certificate or certificate chain, make it
		   implicitly trusted in order to allow the signature check to
		   work */
		cryptCaCert = CRYPT_UNUSED;
		status = cryptSetAttribute( cryptCert,
									CRYPT_CERTINFO_CURRENT_CERTIFICATE,
									CRYPT_CURSOR_LAST );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptCert,
										CRYPT_CERTINFO_TRUSTED_IMPLICIT, 1 );
			}
		assert( cryptStatusOK( status ) );
		}
	else
		{
		status = importCertFile( &cryptCaCert, caFileNameOpt );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't import certificate from '%s', status = %d.\n", 
					caFileNameOpt, status );
			cryptDestroyCert( cryptCert );
			return( FALSE );
			}
		}
	status = cryptCheckCert( cryptCert, cryptCaCert );
	if( cryptStatusError( status ) )
		{
		printf( "Certificate check failed, status = %d.\n", status );
		printErrorAttributeInfo( cryptCert );
		}
	assert( cryptStatusOK( status ) );
	cryptDestroyCert( cryptCert );
	cryptDestroyCert( cryptCaCert );

	return( cryptStatusOK( status ) ? TRUE : FALSE );
	}
