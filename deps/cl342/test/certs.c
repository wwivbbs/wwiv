/****************************************************************************
*																			*
*					cryptlib Certificate Handling Test Routines				*
*						Copyright Peter Gutmann 1997-2009					*
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

/* Certificate times.  Note that this value must be greater than the value
   defined by the kernel as MIN_TIME_VALUE, the minimum allowable 
   (backdated) timestamp value.

   Unlike every other system on the planet, the Mac Classic takes the time_t 
   epoch as 1904 rather than 1970 (even VMS, MVS, VM/CMS, the AS/400, Tandem 
   NSK, and God knows what other sort of strangeness stick to 1970 as the 
   time_t epoch).  ANSI and ISO C are very careful to avoid specifying what 
   the epoch actually is, so it's legal to do this in the same way that it's 
   legal for Microsoft to break Kerberos because the standard doesn't say 
   they can't.
   
   Note that the Y2K time-test value isn't really used any more because Y2K
   has long since come and gone, but it's left in here for historical 
   reasons in case someone has some checkbox requirement for something like
   this */

#define ONE_YEAR_TIME	( 365 * 86400L )
#if defined( __MWERKS__ ) || defined( SYMANTEC_C ) || defined( __MRC__ )
  #define CERTTIME_DATETEST	( ( ( 2012 - 1970 ) * ONE_YEAR_TIME ) + 2082844800L )
  #define CERTTIME_Y2KTEST	( ( ( 2020 - 1970 ) * ONE_YEAR_TIME ) + 2082844800L )
#else
  #define CERTTIME_DATETEST	( ( 2012 - 1970 ) * ONE_YEAR_TIME )
  #define CERTTIME_Y2KTEST	( ( 2020 - 1970 ) * ONE_YEAR_TIME )
#endif /* Macintosh-specific weird epoch */
#if CERTTIME_DATETEST < MIN_TIME_VALUE
  #error CERTTIME_DATETEST must be >= MIN_TIME_VALUE
#endif /* Safety check of time test value against MIN_TIME_VALUE */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Set the trust setting for the root CA in a certificate chain.  This is 
   required for the self-test in order to allow signature checks for chains 
   signed by arbitrary CAs to work */

int setRootTrust( const CRYPT_CERTIFICATE cryptCertChain,
				  BOOLEAN *oldTrustValue, const BOOLEAN newTrustValue )
	{
	int status;

	status = cryptSetAttribute( cryptCertChain,
								CRYPT_CERTINFO_CURRENT_CERTIFICATE,
								CRYPT_CURSOR_LAST );
	if( cryptStatusError( status ) )
		return( status );
	if( oldTrustValue != NULL )
		cryptGetAttribute( cryptCertChain, CRYPT_CERTINFO_TRUSTED_IMPLICIT,
						   oldTrustValue );
	return( cryptSetAttribute( cryptCertChain,
							   CRYPT_CERTINFO_TRUSTED_IMPLICIT,
							   newTrustValue ) );
	}

/****************************************************************************
*																			*
*						Certificate Creation Routines Test					*
*																			*
****************************************************************************/

BYTE FAR_BSS certBuffer[ BUFFER_SIZE ];
int certificateLength;

/* Create a series of self-signed certs */

static const CERT_DATA FAR_BSS certData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave Smith" ) },

	/* Self-signed X.509v3 certificate (technically it'd be an X.509v1, but
	   cryptlib automatically adds some required standard attributes so it
	   becomes an X.509v3 certificate) */
	{ CRYPT_CERTINFO_SELFSIGNED, IS_NUMERIC, TRUE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

int testBasicCert( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	int value, status;

#if defined( _MSC_VER ) && ( _MSC_VER <= 800 )
	time_t testTime = time( NULL ), newTime;

	newTime = mktime( localtime( &testTime ) );
	if( newTime == testTime )
		{
		puts( "Illogical local/GMT time detected.  VC++ 1.5x occasionally "
			  "exhibits a bug in\nits time zone handling in which it thinks "
			  "that the local time zone is GMT and\nGMT itself is some "
			  "negative offset from the current time.  This upsets\n"
			  "cryptlibs certificate date validity checking, since "
			  "certificates appear to\nhave inconsistent dates.  Deleting "
			  "all the temporary files and rebuilding\ncryptlib after "
			  "restarting your machine may fix this.\n" );
		return( FALSE );
		}
#endif /* VC++ 1.5 bug check */

	puts( "Testing certificate creation/export..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some certificate components */
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, certData, __LINE__ ) )
		return( FALSE );

	/* Delete a component and replace it with something else */
	status = cryptDeleteAttribute( cryptCert, CRYPT_CERTINFO_COMMONNAME );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptDeleteAttribute()", status,
							   __LINE__ ) );
	cryptSetAttributeString( cryptCert,
				CRYPT_CERTINFO_COMMONNAME, TEXT( "Dave Taylor" ),
				paramStrlen( TEXT( "Dave Taylor" ) ) );

	/* Sign the certificate and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Check the signature.  Since it's self-signed, we don't need to pass in
	   a signature check key */
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Set the certificate usage to untrusted for any purpose, which should 
	   result in the signature check failing */
	cryptSetAttribute( cryptCert, CRYPT_CERTINFO_TRUSTED_USAGE,
					   CRYPT_KEYUSAGE_NONE );
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusOK( status ) )
		{
		puts( "Untrusted certificate signature check succeeded, should "
			  "have failed." );
		return( FALSE );
		}
	cryptDeleteAttribute( cryptCert, CRYPT_CERTINFO_TRUSTED_USAGE );

	/* Export the certificate.  We perform a length check using a null 
	   buffer to make sure that this facility is working as required */
	status = cryptExportCert( NULL, 0, &value, CRYPT_CERTFORMAT_CERTIFICATE,
							  cryptCert );
	if( cryptStatusOK( status ) )
		status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
								  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	if( value != certificateLength )
		{
		puts( "Exported certificate size != actual data size." );
		return( FALSE );
		}
	printf( "Exported certificate is %d bytes long.\n", certificateLength );
	debugDump( "cert", certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		{
		int errorType, errorLocus;

		attrErrorExit( cryptCert, "cryptCheckCert()", status, __LINE__ );
		status = cryptGetAttribute( cryptCert, CRYPT_ATTRIBUTE_ERRORTYPE,
									&errorType );
		if( cryptStatusOK( status ) )
			status = cryptGetAttribute( cryptCert, 
										CRYPT_ATTRIBUTE_ERRORLOCUS, 
										&errorLocus );
		if( cryptStatusOK( status ) && \
			errorType == CRYPT_ERRTYPE_CONSTRAINT && \
			errorLocus == CRYPT_CERTINFO_VALIDFROM )
			{
			puts( "  (If this test was run within +/- 12 hours of a "
				  "daylight savings time (DST)\n   switchover then this is "
				  "a false positive caused by problems in\n   performing "
				  "date calculations using the C standard libraries on days "
				  "that\n   have 23 or 25 hours due to hours missing or "
				  "being repeated.  This problem\n   will correct itself "
				  "once the time is more than 12 hours away from the DST\n"
				  "   switchover, and only affects the certificate-creation "
				  "self-test)." );
			}

		return( FALSE );
		}
	cryptDestroyCert( cryptCert );

	/* Clean up */
	puts( "Certificate creation succeeded.\n" );
	return( TRUE );
	}

static const CERT_DATA FAR_BSS cACertData[] = {
	/* Identification information.  Note the non-heirarchical order of the
	   components to test the automatic arranging of the DN */
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers and CA" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave Himself" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Certification Division" ) },
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },

	/* Self-signed X.509v3 certificate */
	{ CRYPT_CERTINFO_SELFSIGNED, IS_NUMERIC, TRUE },

	/* Start date set to a fixed value to check for problems in date/time
	   conversion routines, expiry date set to > Y2K to test for Y2K 
	   problems.  In theory the start time should be set to < Y2K to 
	   properly test the time handling, however the kernel won't allow
	   times backdated this far so we have to set the value to the minimum
	   allowed by the kernel, which is well after Y2K */
	{ CRYPT_CERTINFO_VALIDFROM, IS_TIME, 0, NULL, CERTTIME_DATETEST },
	{ CRYPT_CERTINFO_VALIDTO, IS_TIME, 0, NULL, CERTTIME_Y2KTEST },

	/* CA extensions.  Policies are very much CA-specific and currently
	   undefined, so we use a dummy OID for a nonexistant private org for
	   now */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC,
	  CRYPT_KEYUSAGE_KEYCERTSIGN | CRYPT_KEYUSAGE_CRLSIGN },
	{ CRYPT_CERTINFO_CA, IS_NUMERIC, TRUE },
	{ CRYPT_CERTINFO_PATHLENCONSTRAINT, IS_NUMERIC, 0 },
	{ CRYPT_CERTINFO_CERTPOLICYID, IS_STRING, 0, TEXT( "1 3 6 1 4 1 9999 1" ) },
		/* Blank line needed due to bug in Borland C++ parser */
	{ CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT, IS_STRING, 0, TEXT( "This policy isn't worth the paper it's not printed on." ) },
	{ CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION, IS_STRING, 0, TEXT( "Honest Joe's used cars and certification authority" ) },
	{ CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS, IS_NUMERIC, 1 },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

int testCACert( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	time_t startTime, endTime = DUMMY_INIT;
	int value, status;

	puts( "Testing CA certificate creation/export..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Test the ability to handle conversion of 32 <-> 64-bit time_t 
	   values in Win32 (but not Win64, where they're always 64-bit) */
#if defined( __WINDOWS__ ) && defined( _WIN32 ) && defined( _MSC_VER ) && \
	!defined( _M_X64 )
	{
	const __int64 time64 = CERTTIME_DATETEST;
	const unsigned int time32 = CERTTIME_DATETEST;

	status = cryptSetAttributeString( cryptCert, CRYPT_CERTINFO_VALIDFROM,
									  &time64, sizeof( time64 ) );
	if( cryptStatusOK( status ) )
		{
		cryptDeleteAttribute( cryptCert, CRYPT_CERTINFO_VALIDFROM );
		status = cryptSetAttributeString( cryptCert, CRYPT_CERTINFO_VALIDFROM,
										  &time32, sizeof( time32 ) );
		cryptDeleteAttribute( cryptCert, CRYPT_CERTINFO_VALIDFROM );
		}
	if( cryptStatusError( status ) )
		{
		printf( "Automatic 32 <-> 32-bit time_t correction failed, "
				"line %d.\n", __LINE__ );
		return( FALSE );
		}
	}
#endif /* Win32 with VC++ */

	/* Add some certificate components */
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, cACertData, __LINE__ ) )
		return( FALSE );

	/* Sign the certificate and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Export the certificate, this time with base64 encoding to make sure 
	   that this works.  As before, we perform a length check using a null
	   buffer to make sure that this facility is working as required */
	status = cryptExportCert( NULL, 0, &value,
							  CRYPT_CERTFORMAT_TEXT_CERTIFICATE, cryptCert );
	if( cryptStatusOK( status ) )
		status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
								  CRYPT_CERTFORMAT_TEXT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	if( value != certificateLength )
		{
		puts( "Exported certificate size != actual data size." );
		return( FALSE );
		}
	printf( "Exported certificate is %d bytes long.\n", certificateLength );
	debugDump( "cacert", certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created.  We make the second
	   parameter to the check function the certificate (rather than 
	   CRYPT_UNUSED as done for the basic self-signed certificate) to check 
	   that this option works as required, and then retry with CRYPT_UNUSED 
	   to check the other possibility (although it's already been checked in 
	   the basic certificate above) */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, cryptCert );
	if( cryptStatusOK( status ) )
		status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );
	status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_VALIDFROM,
									  &startTime, &value );
	if( cryptStatusOK( status ) )
		status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_VALIDTO,
										  &endTime, &value );
	if( cryptStatusError( status ) )
		{
		printf( "Certificate time read failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( startTime != CERTTIME_DATETEST )
		{
		printf( "Warning: Certificate start time is wrong, got %lX, should be "
				"%lX.\n         This is probably due to problems in the "
				"system time handling routines.\n",
				startTime, CERTTIME_DATETEST );
		}
	if( endTime != CERTTIME_Y2KTEST )
		{
		printf( "Warning: Certificate end time is wrong, got %lX, should be "
				"%lX.\n         This is probably due to problems in the "
				"system time handling routines.\n",
				endTime, CERTTIME_Y2KTEST );
		}
	cryptDestroyCert( cryptCert );
#if defined( __WINDOWS__ ) || defined( __linux__ ) || defined( sun )
	if( ( startTime != CERTTIME_DATETEST && \
		  ( startTime - CERTTIME_DATETEST != 3600 && \
			startTime - CERTTIME_DATETEST != -3600 ) ) || \
		( endTime != CERTTIME_Y2KTEST && \
		  ( endTime - CERTTIME_Y2KTEST != 3600 && \
			endTime - CERTTIME_Y2KTEST != -3600 ) ) )
		{
		/* If the time is off by exactly one hour this isn't a problem
		   because the best we can do is get the time adjusted for DST
		   now rather than DST when the certificate was created, a problem 
		   that is more or less undecidable.  In addition we don't 
		   automatically abort for arbitrary systems since date problems 
		   usually arise from incorrectly configured time zone info or bugs 
		   in the system date-handling routines or who knows what, aborting 
		   on every random broken system would lead to a flood of 
		   unnecessary "bug" reports */
		return( FALSE );
		}
#endif /* System with known-good time handling */

	/* Clean up */
	puts( "CA certificate creation succeeded.\n" );
	return( TRUE );
	}

static const CERT_DATA FAR_BSS xyzzyCertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave Smith" ) },

	/* XYZZY certificate */
	{ CRYPT_CERTINFO_XYZZY, IS_NUMERIC, TRUE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

static int xyzzyCert( const BOOLEAN useAltAlgo )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	int status;

	printf( "Testing %sXYZZY certificate creation/export...\n",
			useAltAlgo ? "DSA " : "" );

	/* Create the RSA en/decryption contexts */
	if( useAltAlgo )
		{
		if( !loadDSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
			return( FALSE );
		}
	else
		{
		if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
			return( FALSE );
		}

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some certificate components */
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, xyzzyCertData, __LINE__ ) )
		return( FALSE );

	/* Sign the certificate and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Check the signature.  Since it's self-signed, we don't need to pass in
	   a signature check key */
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Export the certificate */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certificate is %d bytes long.\n", certificateLength );
	debugDump( useAltAlgo ? "certxyd" : "certxy", certBuffer, 
			   certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* Clean up */
	printf( "%sXYZZY certificate creation succeeded.\n\n",
			useAltAlgo ? "DSA " : "" );
	return( TRUE );
	}

int testXyzzyCert( void )
	{
	if( !xyzzyCert( FALSE ) )
		return( FALSE );
	return( xyzzyCert( TRUE ) );
	}

#ifdef HAS_WIDECHAR

static const wchar_t FAR_BSS unicodeStr[] = {
	0x0414, 0x043E, 0x0432, 0x0435, 0x0440, 0x044F, 0x0439, 0x002C,
	0x0020, 0x043D, 0x043E, 0x0020, 0x043F, 0x0440, 0x043E, 0x0432,
	0x0435, 0x0440, 0x044F, 0x0439, 0x0000 };
static const wchar_t FAR_BSS unicode2Str[] = {
	0x004D, 0x0061, 0x0072, 0x0074, 0x0069, 0x006E, 0x0061, 0x0020,
	0x0160, 0x0069, 0x006B, 0x006F, 0x0076, 0x006E, 0x00E1, 0x0000 };
#ifdef __UNIX__		/* Only enabled for native-UTF8 environments */
static const BYTE FAR_BSS utf8OriginalStr[] = {
	0xC3, 0x98, 0xC3, 0x86, 0xC3, 0x85, 0xC3, 0xA6, 0xC3, 0xB8, 0xC3, 
	0xA5, 0x00 };
static const BYTE FAR_BSS utf8EncodedStr[] = {
	0xC3, 0x83, 0xCB, 0x9C, 0xC3, 0x83, 0xE2, 0x80, 0xA0, 0xC3, 0x83, 
	0xE2, 0x80, 0xA6, 0xC3, 0x83, 0xC2, 0xA6, 0xC3, 0x83, 0xC2, 0xB8, 
	0xC3, 0x83, 0xC2, 0xA5, 0x00 };
#endif /* __UNIX__ */

static const CERT_DATA FAR_BSS textStringCertData[] = {
	/* Identification information: A latin-1 string, an obviously Unicode 
	   string, a less-obviously Unicode string (only the 0x160 value is 
	   larger than 8 bits), an ASCII-in-Unicode string, and an ASCII 
	   string */
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Hörr Østerix" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_WCSTRING, 0, unicodeStr },
	{ CRYPT_CERTINFO_LOCALITYNAME, IS_WCSTRING, 0, unicode2Str },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_WCSTRING, 0, L"Dave's Unicode-aware CA with very long string" },
#ifdef __UNIX__
	{ CRYPT_CERTINFO_STATEORPROVINCENAME, IS_STRING, 0, utf8EncodedStr },
#endif /* __UNIX__ */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "GB" ) },

	/* Another XYZZY certificate */
	{ CRYPT_CERTINFO_XYZZY, IS_NUMERIC, TRUE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

int testTextStringCert( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	int i, status;

	puts( "Testing complex string type certificate creation/export..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some certificate components */
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, textStringCertData, __LINE__ ) )
		return( FALSE );

	/* Sign the certificate and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Check the signature.  Since it's self-signed, we don't need to pass in
	   a signature check key */
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Export the certificate */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certificate is %d bytes long.\n", certificateLength );
	debugDump( "certstr", certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Make sure that we can read back what we've written and that it was
	   correctly converted back to the original string value */
	for( i = 0; textStringCertData[ i ].type != CRYPT_CERTINFO_XYZZY; i++ )
		{
		const CRYPT_ATTRIBUTE_TYPE attribute = textStringCertData[ i ].type;
		BYTE buffer[ 256 ];
		int length;

		status = cryptGetAttributeString( cryptCert, attribute, buffer, 
										  &length );
		if( cryptStatusError( status ) )
			{
			printf( "Attempt to read back DN value %d failed with error "
					"code %d, line %d.\n", attribute, status, __LINE__ );
			return( FALSE );
			}
		if( textStringCertData[ i ].componentType == IS_WCSTRING )
			{
#ifdef HAS_WIDECHAR
			const int wstrLen = wcslen( textStringCertData[ i ].stringValue ) * \
								sizeof( wchar_t );
			if( wstrLen != length || \
				memcmp( buffer, textStringCertData[ i ].stringValue, length ) )
				{
				if( attribute == CRYPT_CERTINFO_ORGANIZATIONNAME )
					{
					/* This is an ASCII string disguised as Unicode, which 
					   cryptlib correctly canonicalises back to ASCII */
					continue;
					}
				printf( "Widechar DN value %d read from certificate with value\n", 
						attribute );
				printHex( buffer, length );
				printf( "doesn't match value\n" );
				printHex( textStringCertData[ i ].stringValue, wstrLen );
				printf( "that was written, line %d.\n", __LINE__ );
				return( FALSE );
				}
#endif /* HAS_WIDECHAR */
			}
		else
			{
			const int strLen = paramStrlen( textStringCertData[ i ].stringValue );
			if( strLen != length || \
				memcmp( buffer, textStringCertData[ i ].stringValue, length ) )
				{
				if( attribute == CRYPT_CERTINFO_STATEORPROVINCENAME )
					{
					/* This is a UTF8 string that cryptlib canonicalises into
					   Unicode (since there's no way to tell what the host
					   system uses as its native 8-bit character system) so 
					   the encoded form as read doesn't match what's 
					   written */
					continue;
					}
				printf( "DN value %d read from certificate with value\n", 
						attribute );
				printHex( buffer, length );
				printf( "doesn't match value\n" );
				printHex( textStringCertData[ i ].stringValue, strLen );
				printf( "that was written, line %d.\n", __LINE__ );
				return( FALSE );
				}
			}
		}
	cryptDestroyCert( cryptCert );

	/* Clean up */
	puts( "Complex string type certificate creation succeeded.\n" );
	return( TRUE );
	}
#else

int testTextStringCert( void )
	{
	return( TRUE );
	}
#endif /* Unicode-aware systems */

static const CERT_DATA FAR_BSS complexCertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "US" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers and Netscape CA" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "SSL Certificates" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Robert';DROP TABLE certificates;--" ) },

	/* Self-signed X.509v3 certificate */
	{ CRYPT_CERTINFO_SELFSIGNED, IS_NUMERIC, TRUE },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* Oddball altName components.  Note that the otherName.value must be a
	   DER-encoded ASN.1 object */
	{ CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER, IS_STRING, 0, TEXT( "EDI Name Assigner" ) },
	{ CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME, IS_STRING, 0, TEXT( "EDI Party Name" ) },
	{ CRYPT_CERTINFO_OTHERNAME_TYPEID, IS_STRING, 0, TEXT( "1 3 6 1 4 1 9999 2" ) },
	{ CRYPT_CERTINFO_OTHERNAME_VALUE, IS_STRING, 10, "\x04\x08" "12345678" },

#ifdef USE_CERTLEVEL_PKIX_FULL
	/* Path constraint */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_EXCLUDEDSUBTREES },
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "CZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Brother's CA" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "SSL Certificates" ) },
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* CRL distribution points */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_CRLDIST_FULLNAME },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.revocations.com/crls/" ) },

	/* SubjectInfoAccess */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://192.168.1.1:8080/timesheet.asp?userid=1234;DROP%20TABLE%20USERS" ) },

#ifdef USE_CERT_OBSOLETE
	/* Add a vendor-specific extension, in this case a Thawte strong extranet
	   extension */
	{ CRYPT_CERTINFO_STRONGEXTRANET_ZONE, IS_NUMERIC, 0x99 },
	{ CRYPT_CERTINFO_STRONGEXTRANET_ID, IS_STRING, 0, TEXT( "EXTRA1" ) },
#endif /* USE_CERT_OBSOLETE */

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* Misc funnies */
	{ CRYPT_CERTINFO_OCSP_NOCHECK, IS_NUMERIC, CRYPT_UNUSED },
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

	/* Re-select the subject name after poking around in the altName */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTNAME },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

int testComplexCert( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	C_CHR buffer1[ 64 ], buffer2[ 64 ];
	int length1 = DUMMY_INIT, length2 = DUMMY_INIT, status;

	puts( "Testing complex certificate creation/export..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some certificate components */
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, complexCertData, __LINE__ ) )
		return( FALSE );

	/* Add an OID, read it back, and make sure that the OID en/decoding 
	   worked correctly */
	status = cryptSetAttributeString( cryptCert, CRYPT_CERTINFO_CERTPOLICYID, 
									  TEXT( "1 2 3 4 5" ), 
									  paramStrlen( TEXT( "1 2 3 4 5" ) ) );
	if( cryptStatusOK( status ) )
		status = cryptGetAttributeString( cryptCert, 
										  CRYPT_CERTINFO_CERTPOLICYID, 
										  buffer1, &length1 );
	if( cryptStatusOK( status ) )
		status = cryptDeleteAttribute( cryptCert, CRYPT_CERTINFO_CERTPOLICYID );
	if( cryptStatusOK( status ) && \
		( length1 != ( int ) paramStrlen( TEXT( "1 2 3 4 5" ) ) || \
		  memcmp( buffer1, TEXT( "1 2 3 4 5" ), length1 ) ) )
		{
		printf( "Error in OID en/decoding, line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Add a non-CA basicConstraint, delete it, and re-add it as CA
	   constraint */
	status = cryptSetAttribute( cryptCert, CRYPT_CERTINFO_CA, FALSE );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	status = cryptDeleteAttribute( cryptCert,
								   CRYPT_CERTINFO_BASICCONSTRAINTS );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptDeleteAttribute()", status,
							   __LINE__ ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptCert, CRYPT_CERTINFO_CA, TRUE );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );

	/* Add a disabled attribute and make sure that it's detected.  This can 
	   be done in one of two ways, either directly by the kernel with a 
	   permission error or by the certificate-processing code with a not-
	   available error if we go in indirectly, for example using the 
	   attribute cursor */
#ifndef USE_CERT_OBSOLETE
	status = cryptSetAttribute( cryptCert, 
								CRYPT_CERTINFO_STRONGEXTRANET_ZONE, 1 );
	if( status != CRYPT_ERROR_PARAM2 )
		{
		printf( "Addition of disabled attribute %d wasn't detected, "
				"line %d.\n", CRYPT_CERTINFO_STRONGEXTRANET_ZONE, __LINE__ );
		return( FALSE );
		}
#endif /* USE_CERT_OBSOLETE */
#ifndef USE_CERTLEVEL_PKIX_FULL
	status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT, 
								CRYPT_CERTINFO_EXCLUDEDSUBTREES );
	if( status != CRYPT_ERROR_PARAM3 )
		{
		printf( "Indirect addition of disabled attribute %d wasn't "
				"detected, line %d.\n", CRYPT_CERTINFO_EXCLUDEDSUBTREES, 
				__LINE__ );
		return( FALSE );
		}
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* Sign the certificate and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Make sure that GeneralName component selection is working properly */
	cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT,
					   CRYPT_CERTINFO_SUBJECTALTNAME );
	status = cryptGetAttributeString( cryptCert,
						CRYPT_CERTINFO_RFC822NAME, buffer1, &length1 );
	if( cryptStatusOK( status ) )
		status = cryptGetAttributeString( cryptCert,
						CRYPT_CERTINFO_RFC822NAME, buffer2, &length2 );
	if( cryptStatusError( status ) )
		{
		printf( "Attempt to read and re-read email address failed, line "
				"%d.\n", __LINE__ );
		return( FALSE );
		}
#ifdef UNICODE_STRINGS
	buffer1[ length1 / sizeof( wchar_t ) ] = TEXT( '\0' );
	buffer2[ length2 / sizeof( wchar_t ) ] = TEXT( '\0' );
#else
	buffer1[ length1 ] = '\0';
	buffer2[ length2 ] = '\0';
#endif /* UNICODE_STRINGS */
	if( ( length1 != ( int ) paramStrlen( TEXT( "dave@wetas-r-us.com" ) ) ) || \
		( length1 != length2 ) || \
		memcmp( buffer1, TEXT( "dave@wetas-r-us.com" ), length1 ) || \
		memcmp( buffer2, TEXT( "dave@wetas-r-us.com" ), length2 ) )
		{
		printf( "Email address on read #1 = '%s',\n  read #2 = '%s', should "
				"have been '%s', line %d.\n", buffer1, buffer2,
				"dave@wetas-r-us.com", __LINE__ );
		return( FALSE );
		}

	/* Export the certificate */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certificate is %d bytes long.\n", certificateLength );
	debugDump( "certc", certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* Clean up */
	puts( "Complex certificate creation succeeded.\n" );
	return( TRUE );
	}

int testCertExtension( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	BYTE buffer[ 16 ];
	const char *extensionData = "\x0C\x04Test";
	int value, length, status;

	puts( "Testing certificate with nonstd.extension creation/export..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptCert, CRYPT_CERTINFO_CA, TRUE );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, certData, __LINE__ ) )
		return( FALSE );

	/* Add a nonstandard critical extension */
	status = cryptAddCertExtension( cryptCert, "1.2.3.4.5", TRUE, extensionData, 6 );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptAddCertExtension()", status,
							   __LINE__ ) );

	/* Sign the certificate.  Since we're adding a nonstandard extension we
	   have to set the CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES flag to
	   make sure that cryptlib will sign it */
	cryptGetAttribute( CRYPT_UNUSED,
					   CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES, &value );
	cryptSetAttribute( CRYPT_UNUSED,
					   CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES, TRUE );
	status = cryptSignCert( cryptCert, privKeyContext );
	cryptSetAttribute( CRYPT_UNUSED,
					   CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES, value );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );

	/* Print information on what we've got */
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Export the certificate and make sure that we can read what we 
	   created */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certificate is %d bytes long.\n", certificateLength );
	debugDump( "certext", certBuffer, certificateLength );
	cryptDestroyCert( cryptCert );
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Check the certificate.  Since it contains an unrecognised critical 
	   extension it should be rejected, but accepted at a lowered compliance 
	   level */
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusOK( status ) )
		{
		printf( "Certificate with unrecognised critical extension was "
				"accepted when it should\nhave been rejected, line %d.\n",
				__LINE__ );
		return( FALSE );
		}
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   &value );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   CRYPT_COMPLIANCELEVEL_REDUCED );
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   value );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Read back the nonstandard extension and make sure that it's what we
	   originally wrote */
	status = cryptGetCertExtension( cryptCert, "1.2.3.4.5", &value, buffer,
									16, &length );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptGetCertExtension()", status,
							   __LINE__ ) );
	if( value != TRUE || length != 6 || memcmp( extensionData, buffer, 6 ) )
		{
		printf( "Recovered nonstandard extension data differs from what was "
				"written, line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroyCert( cryptCert );
	puts( "Certificate with nonstd.extension creation succeeded.\n" );
	return( TRUE );
	}

int testCustomDNCert( void )
	{
#ifdef USE_CERT_DNSTRING
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	const C_STR customDN = \
				TEXT( "cn=Dave Taylor + sn=12345, ou=Org.Unit 2\\=1, " )
				TEXT( "ou=Org.Unit 2, ou=Org.Unit 1, " )
				TEXT( "o=Dave's Big Organisation, c=PT" );
	const C_STR invalidDnStrings[] = {
		TEXT( "abc\x01\x64" ) TEXT( "def" ),/* Invalid chars */
		TEXT( "cn=" ),				/* No value */
		TEXT( "cn=\\" ),			/* No escaped char */
		TEXT( "c\\n=x" ),			/* Escape in type */
		TEXT( "cn+x" ),				/* Spurious '+' */
		TEXT( "cn,x" ),				/* Spurious ',' */
		TEXT( "cn=z=y" ),			/* Spurious '=' */
		TEXT( "cn=x," ),			/* Spurious ',' */
		TEXT( "xyz=x" ),			/* Unknown type */
		NULL
		};
	char buffer[ BUFFER_SIZE ];
	int length, i, status;

	puts( "Testing certificate with custom DN creation/export..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptCert, CRYPT_CERTINFO_CA, TRUE );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptCert, CRYPT_CERTINFO_SELFSIGNED, TRUE );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );

	/* Make sure that invalid DN strings are detected */
	for( i = 0; invalidDnStrings[ i ] != NULL; i++ )
		{
		status = cryptSetAttributeString( cryptCert, CRYPT_CERTINFO_DN,
										  invalidDnStrings[ i ], 
										  paramStrlen( invalidDnStrings[ i ] ) );
		if( cryptStatusOK( status ) )
			{
			printf( "Addition of invalid DN string '%s' wasn't detected, "
					"line %d.\n", invalidDnStrings[ i ], __LINE__ );
			return( FALSE );
			}
		}

	/* Add the custom DN in string form */
	status = cryptSetAttributeString( cryptCert, CRYPT_CERTINFO_DN,
									  customDN, paramStrlen( customDN ) );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttributeString()", status,
							   __LINE__ ) );

	/* Sign the certificate and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Export the certificate and make sure that we can read what we 
	   created */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certificate is %d bytes long.\n", certificateLength );
	debugDump( "certext", certBuffer, certificateLength );
	cryptDestroyCert( cryptCert );
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Read back the custom DN and make sure that it's what we originally
	   wrote */
	status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_DN,
									  buffer, &length );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptGetAttributeString()", status,
							   __LINE__ ) );
	if( length != ( int ) paramStrlen( customDN ) || \
		memcmp( customDN, buffer, length ) )
		{
		printf( "Recovered custom DN differs from what was written, line "
				"%d.\n", __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroyCert( cryptCert );
	puts( "Certificate with custom DN creation succeeded.\n" );
#else
	puts( "Skipping custom DN certificate creation/export test because "
		  "support for\nthis capability has been disabled via the cryptlib "
		  "config options.\n" );
#endif /* USE_CERT_DNSTRING */
	return( TRUE );
	}

int testCertAttributeHandling( void )
	{
#ifdef USE_CERT_DNSTRING
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	const C_STR customDN = \
				TEXT( "cn=Dave Taylor, ou=Org.Unit 3, ou=Org.Unit 2, " )
				TEXT( "ou=Org.Unit 1, o=Dave's Big Organisation, c=PT" );
	const C_STR email = TEXT( "dave@example.com" );
	const char *errorString = "(Generic attribute get/set/select error)";
	char buffer[ BUFFER_SIZE ];
	int length, value, status;

	puts( "Testing certificate attribute handling..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptCert, CRYPT_CERTINFO_SELFSIGNED, TRUE );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );

	/* Add the custom DN in string form and an altName component */
	status = cryptSetAttributeString( cryptCert, CRYPT_CERTINFO_DN,
									  customDN, paramStrlen( customDN ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptCert, CRYPT_CERTINFO_EMAIL,
										  email, paramStrlen( email ) );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttributeString()", status,
							   __LINE__ ) );

	/* Sign the certificate */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );

	/* Make sure that the attribute-manipulation routines work as 
	   intended */
	status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT,
								CRYPT_CERTINFO_SUBJECTALTNAME );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT,
									&value );
		if( cryptStatusError( status ) || \
			value != CRYPT_CERTINFO_SUBJECTALTNAME )
			{
			errorString = "Current attribute != subject altName after "
						  "subject altName was selected";
			status = -1;
			}
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_EMAIL,
										  buffer, &length );
		if( cryptStatusError( status ) )
			errorString = "Fetch of email address from altName failed";
		}
	if( cryptStatusOK( status ) )
		{
		/* Should fail since we've now selected the DN in the altName */
		status = cryptGetAttributeString( cryptCert, 
										  CRYPT_CERTINFO_ORGANISATIONALUNITNAME,
										  buffer, &length );
		if( cryptStatusOK( status ) )
			{
			errorString = "OU was returned after altName was selected";
			status = -1;
			}
		else
			status = CRYPT_OK;
		}
	if( cryptStatusError( status ) )
		{
		printf( "%s, line %d.\n", errorString, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT,
								CRYPT_CERTINFO_SUBJECTNAME );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT,
									&value );
		if( cryptStatusError( status ) || \
			value != CRYPT_CERTINFO_SUBJECTNAME )
			{
			errorString = "Current attribute != subject DN after subject DN "
						  "was selected";
			status = -1;
			}
		}
#if 0	/* The following should in theory fail but doesn't because of the 
		   auto-selection of the subject altName when no other GeneralName 
		   is selected.  This is required in order for reads of commonly-
		   used fields like email addresses to work without the user having
		   to explicitly select the subject altName (which they're likely
		   unaware of) first.  This result is slightly non-orthogonal, but 
		   given the choice of enforcing strict orthogonality in a facility
		   that most users will never use vs. making something that's widely
		   used work as expected, the latter is the preferable option */
	if( cryptStatusOK( status ) )
		{
		/* Should fail since the subject DN is the currently selected 
		   attribute */
		status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_EMAIL,
										  buffer, &length );
		if( cryptStatusOK( status ) )
			{
			errorString = "email from altName was returned after subject DN was selected";
			status = -1;
			}
		else
			status = CRYPT_OK;
		}
#endif /* 0 */
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( cryptCert, 
										  CRYPT_CERTINFO_ORGANISATIONALUNITNAME,
										  buffer, &length );
		if( cryptStatusError( status ) )
			errorString = "Fetch of first OU failed";
		}
	if( cryptStatusOK( status ) )
		{
		/* Should fail since there's no current attribute */
		status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT,
									CRYPT_CURSOR_NEXT );
		if( cryptStatusOK( status ) )
			{
			errorString = "CURSOR_NEXT succeeded when no attribute selected";
			status = -1;
			}
		else
			status = CRYPT_OK;
		}
	if( cryptStatusOK( status ) )
		{
		/* Should fail since there's no attribute instance selected */
		status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT_INSTANCE,
									CRYPT_CURSOR_NEXT );
		if( cryptStatusOK( status ) )
			{
			errorString = "CURSOR_NEXT succeeded when no attribute instance selected";
			status = -1;
			}
		else
			status = CRYPT_OK;
		}
	if( cryptStatusError( status ) )
		{
		printf( "%s, line %d.\n", errorString, __LINE__ );
		return( FALSE );
		}
	status = cryptGetAttributeString( cryptCert, 
									  CRYPT_CERTINFO_ORGANISATIONALUNITNAME,
									  buffer, &length );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT_INSTANCE,
									CRYPT_CERTINFO_ORGANISATIONALUNITNAME );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT_INSTANCE,
									&value );
		if( cryptStatusError( status ) || \
			value != CRYPT_CERTINFO_ORGANISATIONALUNITNAME )
			{
			errorString = "Current instance != OU after OU was selected";
			status = -1;
			}
		}
	if( cryptStatusOK( status ) )
		{
		/* Should fail since there's no current attribute */
		status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT,
									CRYPT_CURSOR_NEXT );
		if( cryptStatusOK( status ) )
			{
			errorString = "CURSOR_NEXT succeeded when no attribute selected";
			status = -1;
			}
		else
			status = CRYPT_OK;
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT_INSTANCE,
									CRYPT_CURSOR_NEXT );
		if( cryptStatusError( status ) )
			errorString = "Move to second OU failed";
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( cryptCert, 
										  CRYPT_CERTINFO_ORGANISATIONALUNITNAME,
										  buffer, &length );
		if( cryptStatusError( status ) )
			errorString = "Fetch of second OU failed";
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT_INSTANCE,
									CRYPT_CURSOR_LAST );
		if( cryptStatusError( status ) )
			errorString = "Move to last (third) OU failed";
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( cryptCert, 
										  CRYPT_CERTINFO_ORGANISATIONALUNITNAME,
										  buffer, &length );
		if( cryptStatusError( status ) )
			errorString = "Fetch of third OU failed";
		}
	if( cryptStatusError( status ) )
		{
		printf( "%s, line %d.\n", errorString, __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroyCert( cryptCert );
	puts( "Certificate attribute handling succeeded.\n" );
#else
	puts( "Skipping certificate attribute handling test because support "
		  "for the\nrequired custom DN creation has been disabled via the "
		  "cryptlib config\noptions.\n" );
#endif /* USE_CERT_DNSTRING */
	return( TRUE );
	}

static const CERT_DATA FAR_BSS setCertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers and Temple of SET" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "SET Commerce Division" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave's Cousin Bob" ) },

	/* Self-signed X.509v3 certificate */
	{ CRYPT_CERTINFO_SELFSIGNED, IS_NUMERIC, TRUE },

	/* Add the SET extensions */
	{ CRYPT_CERTINFO_SET_CERTIFICATETYPE, IS_NUMERIC, CRYPT_SET_CERTTYPE_RCA },
	{ CRYPT_CERTINFO_SET_CERTCARDREQUIRED, IS_NUMERIC, TRUE },
	{ CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT, IS_STRING, 20, TEXT( "12345678900987654321" ) },
	{ CRYPT_CERTINFO_SET_MERID, IS_STRING, 0, TEXT( "Wetaburger Vendor" ) },
	{ CRYPT_CERTINFO_SET_MERACQUIRERBIN, IS_STRING, 0, TEXT( "123456" ) },
	{ CRYPT_CERTINFO_SET_MERCHANTLANGUAGE, IS_STRING, 0, TEXT( "English" ) },
	{ CRYPT_CERTINFO_SET_MERCHANTNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers and SET Merchant" ) },
	{ CRYPT_CERTINFO_SET_MERCHANTCITY, IS_STRING, 0, TEXT( "Eketahuna" ) },
	{ CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME, IS_STRING, 0, TEXT( "New Zealand" ) },
	{ CRYPT_CERTINFO_SET_MERCOUNTRY, IS_NUMERIC, 554 },		/* ISO 3166 */

	{ CRYPT_ATTRIBUTE_NONE, 0, 0, NULL }
	};

int testSETCert( void )
	{
#ifdef USE_CERT_OBSOLETE
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	int status;

	puts( "Testing SET certificate creation/export..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some certificate components */
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, setCertData, __LINE__ ) )
		return( FALSE );

	/* Sign the certificate and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Export the certificate */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certificate is %d bytes long.\n", certificateLength );
	debugDump( "certset", certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* Clean up */
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	puts( "SET certificate creation succeeded.\n" );
#else
	puts( "Skipping SET certificate creation/export test because support "
		  "for this\ncertificate type has been disabled via the cryptlib "
		  "config options.\n" );
#endif /* USE_CERT_OBSOLETE */
	return( TRUE );
	}

static const CERT_DATA FAR_BSS attributeCertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NI" ) },		/* Ni! Ni! Ni! */
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers and Attributes" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Attribute Management" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave's Mum" ) },

	{ CRYPT_ATTRIBUTE_NONE, 0, 0, NULL }
	};

int testAttributeCert( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT cryptAuthorityKey;
	int status;

	puts( "Testing attribute certificate creation/export..." );

	/* Get the authority's private key */
	status = getPrivateKey( &cryptAuthorityKey, CA_PRIVKEY_FILE,
							CA_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "Authority private key read failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Create the certificate */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_ATTRIBUTE_CERT );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some certificate components.  Note that we don't add any
	   attributes because these hadn't been defined yet (at least not as of
	   the JTC1 SC21/ITU-T Q.17/7 draft of July 1997) */
	if( !addCertFields( cryptCert, attributeCertData, __LINE__ ) )
		return( FALSE );

	/* Sign the certificate and print information on what we got */
	status = cryptSignCert( cryptCert, cryptAuthorityKey );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Export the certificate */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certificate is %d bytes long.\n", certificateLength );
	debugDump( "certattr", certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, cryptAuthorityKey );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* Clean up */
	cryptDestroyContext( cryptAuthorityKey );
	puts( "Attribute certificate creation succeeded.\n" );
	return( TRUE );
	}

/* Test certification request code.  These create a basic certificate 
   request, a more complex certificate request with all extensions encoded 
   as attributes of an extensionReq, and a request with a separate PKCS #9
   attribute alongside the other attributes in the extensionReq */

static const CERT_DATA FAR_BSS certRequestData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "PT" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave Smith" ) },

	{ CRYPT_ATTRIBUTE_NONE, 0, 0, NULL }
	};

static const CERT_DATA FAR_BSS complexCertRequestData[] = {
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

	/* SSL server and client authentication */
	{ CRYPT_CERTINFO_EXTKEY_SERVERAUTH, IS_NUMERIC, CRYPT_UNUSED },
	{ CRYPT_CERTINFO_EXTKEY_CLIENTAUTH, IS_NUMERIC, CRYPT_UNUSED },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

static const CERT_DATA FAR_BSS certRequestAttribData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "PT" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Dave Smith" ) },

	/* Subject altName */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },

	/* Re-select the subject name after poking around in the altName */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTNAME },

	/* PKCS #9 attribute that isn't encoded as an extensionReq */
	{ CRYPT_CERTINFO_CHALLENGEPASSWORD, IS_STRING, 0, TEXT( "password" ) },

	{ CRYPT_ATTRIBUTE_NONE, 0, 0, NULL }
	};

static int createCertRequest( const char *description, 
							  const CERT_DATA *certInfo,
							  const char *fileName )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	int status;

	printf( "Testing %s creation/export...\n", description );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate object */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTREQUEST );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some certification request components */
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, certInfo, __LINE__ ) )
		return( FALSE );

	/* Sign the certification request and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Check the signature.  Since it's self-signed, we don't need to pass in
	   a signature check key */
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Export the certificate */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported %s is %d bytes long.\n", description, 
			certificateLength );
	debugDump( fileName, certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* Clean up */
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	printf( "Creation of %s succeeded.\n\n", description );
	return( TRUE );
	}

int testCertRequest( void )
	{
	return( createCertRequest( "certification request", certRequestData, 
							   "certreq" ) );
	}

int testComplexCertRequest( void )
	{
	return( createCertRequest( "complex certification request", 
							   complexCertRequestData,
							   "certreqc" ) );
	}

int testCertRequestAttrib( void )
	{
	return( createCertRequest( "certification request with attribute", 
							   certRequestAttribData, "certreqa" ) );
	}

/* Test CRMF certification request code */

int testCRMFRequest( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	int status;

	puts( "Testing CRMF certification request creation/export..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate object */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_REQUEST_CERT );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some certification request components */
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, certRequestData, __LINE__ ) )
		return( FALSE );

	/* Sign the certification request and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Check the signature.  Since it's self-signed, we don't need to pass in
	   a signature check key */
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Export the certificate */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certification request is %d bytes long.\n",
			certificateLength );
	debugDump( "req_crmf", certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* Clean up */
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	puts( "CRMF certification request creation succeeded.\n" );
	return( TRUE );
	}

int testComplexCRMFRequest( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	int status;

	puts( "Testing complex CRMF certification request creation/export..." );

	/* Create the RSA en/decryption contexts */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );

	/* Create the certificate object */
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_REQUEST_CERT );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some certification request components */
	status = cryptSetAttribute( cryptCert,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, complexCertRequestData, __LINE__ ) )
		return( FALSE );

	/* Sign the certification request and print information on what we got */
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	if( !printCertInfo( cryptCert ) )
		return( FALSE );

	/* Check the signature.  Since it's self-signed, we don't need to pass in
	   a signature check key */
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Export the certificate */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certification request is %d bytes long.\n",
			certificateLength );
	debugDump( "req_crmfc", certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCert, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptCheckCert()", status,
							   __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* Clean up */
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	puts( "Complex CRMF certification request creation succeeded.\n" );
	return( TRUE );
	}

/* Test CRL code.  This one represents a bit of a chicken-and-egg problem
   since we need a CA certificate to create the CRL, but we can't read this 
   until the private key file read has been tested, and that requires 
   testing of the certificate management.  At the moment we just assume that 
   private key file reads work for this test */

int testCRL( void )
	{
	CRYPT_CERTIFICATE cryptCRL;
	CRYPT_CONTEXT cryptCAKey;
	int status;

	puts( "Testing CRL creation/export..." );

	/* Get the CA's private key */
	status = getPrivateKey( &cryptCAKey, CA_PRIVKEY_FILE,
							CA_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "CA private key read failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Create the CRL */
	status = cryptCreateCert( &cryptCRL, CRYPT_UNUSED, CRYPT_CERTTYPE_CRL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some CRL components.  In this case the CA is revoking its own
	   key */
	status = cryptSetAttribute( cryptCRL, CRYPT_CERTINFO_CERTIFICATE,
								cryptCAKey );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptSetAttribute()", status,
							   __LINE__ ) );

	/* Sign the CRL */
	status = cryptSignCert( cryptCRL, cryptCAKey );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptSignCert()", status,
							   __LINE__ ) );

	/* Print information on what we've got */
	if( !printCertInfo( cryptCRL ) )
		return( FALSE );

	/* Check the signature.  Since we have the CA private key handy, we
	   use that to check the signature */
	status = cryptCheckCert( cryptCRL, cryptCAKey );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Export the CRL */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCRL );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported CRL is %d bytes long.\n", certificateLength );
	debugDump( "crl", certBuffer, certificateLength );

	/* Destroy the CRL */
	status = cryptDestroyCert( cryptCRL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCRL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCRL, cryptCAKey );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptCheckCert()", status,
							   __LINE__ ) );
	cryptDestroyCert( cryptCRL );
	cryptDestroyContext( cryptCAKey );

	/* Clean up */
	puts( "CRL creation succeeded.\n" );
	return( TRUE );
	}

/* Test complex CRL code */

static const CERT_DATA FAR_BSS complexCRLData[] = {
	/* Next update time */
	{ CRYPT_CERTINFO_NEXTUPDATE, IS_TIME, 0, NULL, CERTTIME_DATETEST + ONE_YEAR_TIME },

	/* CRL number and delta CRL indicator */
	{ CRYPT_CERTINFO_CRLNUMBER, IS_NUMERIC, 1 },
	{ CRYPT_CERTINFO_DELTACRLINDICATOR, IS_NUMERIC, 2 },

	/* Issuing distribution points */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_ISSUINGDIST_FULLNAME },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, TEXT( "http://www.wetas-r-us.com" ) },
	{ CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY, IS_NUMERIC, TRUE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

int testComplexCRL( void )
	{
	CRYPT_CERTIFICATE cryptCRL, cryptRevokeCert;
	CRYPT_CONTEXT cryptCAKey;
	time_t revocationTime;
	int revocationReason = DUMMY_INIT, dummy, status;

	puts( "Testing complex CRL creation/export..." );

	/* Get the CA's private key */
	status = getPrivateKey( &cryptCAKey, CA_PRIVKEY_FILE,
							CA_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "CA private key read failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Create the CRL */
	status = cryptCreateCert( &cryptCRL, CRYPT_UNUSED, CRYPT_CERTTYPE_CRL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some CRL components with per-entry attributes.  In this case the
	   CA is revoking its own key because it was compromised (would you trust
	   this CRL?) and some keys from test certs */
	if( !addCertFields( cryptCRL, complexCRLData, __LINE__ ) )
		return( FALSE );
	status = cryptSetAttribute( cryptCRL, CRYPT_CERTINFO_CERTIFICATE,
								cryptCAKey );
	if( cryptStatusOK( status ) )
		{
		/* The CA key was compromised */
		status = cryptSetAttribute( cryptCRL,
									CRYPT_CERTINFO_CRLREASON,
									CRYPT_CRLREASON_CACOMPROMISE );
		}
	if( cryptStatusOK( status ) )
		status = importCertFromTemplate( &cryptRevokeCert,
										 CRLCERT_FILE_TEMPLATE, 1 );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( cryptCRL, CRYPT_CERTINFO_CERTIFICATE,
									cryptRevokeCert );
		cryptDestroyCert( cryptRevokeCert );
		}
	if( cryptStatusOK( status ) )
		{
		/* Hold certificate, call issuer for details */
		status = cryptSetAttribute( cryptCRL,
									CRYPT_CERTINFO_CRLREASON,
									CRYPT_CRLREASON_CERTIFICATEHOLD );
		if( cryptStatusOK( status ) )
			{
#ifdef USE_CERTLEVEL_PKIX_FULL
			status = cryptSetAttribute( cryptCRL,
										CRYPT_CERTINFO_HOLDINSTRUCTIONCODE,
										CRYPT_HOLDINSTRUCTION_CALLISSUER );
#else
			status = cryptSetAttribute( cryptCRL, 
										CRYPT_CERTINFO_HOLDINSTRUCTIONCODE, 
										CRYPT_HOLDINSTRUCTION_CALLISSUER );
			if( status != CRYPT_ERROR_PARAM2 )
				{
				printf( "Addition of disabled attribute %d wasn't "
						"detected, line %d.\n", 
						CRYPT_CERTINFO_HOLDINSTRUCTIONCODE, __LINE__ );
				return( FALSE );
				}
			status = CRYPT_OK;
#endif /* USE_CERTLEVEL_PKIX_FULL */
			}
		}
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptSetAttribute(), certificate #1", 
							   status, __LINE__ ) );
	status = importCertFromTemplate( &cryptRevokeCert,
									 CRLCERT_FILE_TEMPLATE, 2 );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( cryptCRL, CRYPT_CERTINFO_CERTIFICATE,
									cryptRevokeCert );
		cryptDestroyCert( cryptRevokeCert );
		}
	if( cryptStatusOK( status ) )
		{
		const time_t invalidityDate = CERTTIME_DATETEST - ( ONE_YEAR_TIME / 12 );

		/* The private key was invalid some time ago.  We can't go back too 
		   far because the cryptlib kernel won't allow suspiciously old 
		   dates */
		status = cryptSetAttributeString( cryptCRL,
					CRYPT_CERTINFO_INVALIDITYDATE, &invalidityDate,
					sizeof( time_t ) );
		}
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptSetAttribute(), certificate #2", 
							   status, __LINE__ ) );

	/* Sign the CRL */
	status = cryptSignCert( cryptCRL, cryptCAKey );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptSignCert()", status,
							   __LINE__ ) );

	/* Print information on what we've got */
	if( !printCertInfo( cryptCRL ) )
		return( FALSE );

	/* Check the signature.  Since we have the CA private key handy, we
	   use that to check the signature */
	status = cryptCheckCert( cryptCRL, cryptCAKey );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Export the CRL */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptCRL );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported CRL is %d bytes long.\n", certificateLength );
	debugDump( "crlc", certBuffer, certificateLength );

	/* Destroy the CRL */
	status = cryptDestroyCert( cryptCRL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCRL );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCheckCert( cryptCRL, cryptCAKey );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Check the newly-revoked CA key agains the CRL */
	status = cryptCheckCert( cryptCAKey, cryptCRL );
	if( status != CRYPT_ERROR_INVALID )
		{
		printf( "Revoked certificate wasn't reported as being revoked, "
				"line %d.\n", __LINE__ );
		return( FALSE );
		}
	status = cryptGetAttributeString( cryptCRL, CRYPT_CERTINFO_REVOCATIONDATE,
									  &revocationTime, &dummy );
	if( cryptStatusOK( status ) )
		status = cryptGetAttribute( cryptCRL, CRYPT_CERTINFO_CRLREASON,
									&revocationReason );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCRL, "cryptGetAttribute()", status,
							   __LINE__ ) );
	if( revocationReason != CRYPT_CRLREASON_CACOMPROMISE )
		{
		printf( "Revocation reason was %d, should have been %d, line %d.\n",
				revocationReason, CRYPT_CRLREASON_CACOMPROMISE, __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroyCert( cryptCRL );
	cryptDestroyContext( cryptCAKey );
	puts( "CRL creation succeeded.\n" );
	return( TRUE );
	}

/* Test revocation request code */

static const CERT_DATA FAR_BSS revRequestData[] = {
	/* Revocation reason */
	{ CRYPT_CERTINFO_CRLREASON, IS_NUMERIC, CRYPT_CRLREASON_SUPERSEDED },

	/* Invalidity date */
	{ CRYPT_CERTINFO_INVALIDITYDATE, IS_TIME, 0, NULL, CERTTIME_DATETEST - ( ONE_YEAR_TIME / 12 ) },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

int testRevRequest( void )
	{
	CRYPT_CERTIFICATE cryptCert, cryptRequest;
	FILE *filePtr;
	BYTE buffer[ BUFFER_SIZE ];
	int count, status;

	puts( "Testing revocation request creation/export..." );

	filenameFromTemplate( buffer, CERT_FILE_TEMPLATE, 1 );
	if( ( filePtr = fopen( buffer, "rb" ) ) == NULL )
		{
		puts( "Couldn't find certificate file for revocation request test." );
		return( FALSE );
		}
	count = fread( buffer, 1, BUFFER_SIZE, filePtr );
	fclose( filePtr );
	status = cryptImportCert( buffer, count, CRYPT_UNUSED, &cryptCert );
	if( cryptStatusError( status ) )
		{
		puts( "Certificate import failed, skipping test of revocation "
			  "request..." );
		return( TRUE );
		}

	/* Create the certificate object and add the certificate details and
	   revocation info */
	status = cryptCreateCert( &cryptRequest, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_REQUEST_REVOCATION );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptRequest, CRYPT_CERTINFO_CERTIFICATE,
								cryptCert );
	cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptRequest, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptRequest, revRequestData, __LINE__ ) )
		return( FALSE );

	/* Print information on what we've got */
	if( !printCertInfo( cryptRequest ) )
		return( FALSE );

#if 0	/* CMP doesn't currently allow revocation requests to be signed, so
		   it's treated like CMS attributes as a series of uninitialised
		   attributes */
	/* Export the certificate */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTIFICATE, cryptRequest );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptRequest, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported revocation request is %d bytes long.\n",
			certificateLength );
	debugDump( "req_rev", certBuffer, certificateLength );

	/* Destroy the certificate */
	status = cryptDestroyCert( cryptRequest );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptRequest );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
#endif /* 0 */
	cryptDestroyCert( cryptRequest );

	/* Clean up */
	puts( "Revocation request creation succeeded.\n" );
	return( TRUE );
	}

/* Test certificate chain creation */

static const CERT_DATA FAR_BSS certRequestNoDNData[] = {
	/* Identification information.  There's no DN, only a subject altName.
	   This type of identifier is only possible with a CA-signed certificate 
	   since it contains an empty DN */
	{ CRYPT_CERTINFO_RFC822NAME, IS_STRING, 0, TEXT( "dave@wetas-r-us.com" ) },

	{ CRYPT_ATTRIBUTE_NONE, 0, 0, NULL }
	};

static int createChain( CRYPT_CERTIFICATE *cryptCertChain,
						const CRYPT_CONTEXT cryptCAKey,
						const BOOLEAN useEmptyDN, const BOOLEAN reportError )
	{
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	int status;

	/* Create the certificate chain */
	status = cryptCreateCert( cryptCertChain, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CERTCHAIN );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Create a simple certificate request to turn into the end-user 
	   certificate */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );
	status = cryptSetAttribute( *cryptCertChain,
					CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, pubKeyContext );
	if( cryptStatusOK( status ) && \
		!addCertFields( *cryptCertChain, useEmptyDN ? \
							certRequestNoDNData : certRequestData, 
						__LINE__ ) )
		return( FALSE );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	if( cryptStatusError( status ) )
		{
		printf( "Certificate creation failed with status %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Sign the leaf of the certificate chain */
	status = cryptSignCert( *cryptCertChain, cryptCAKey );
	if( cryptStatusError( status ) )
		{
		cryptDestroyCert( *cryptCertChain );
		if( !reportError )
			return( -1 );
		return( attrErrorExit( *cryptCertChain, "cryptSignCert()", status,
							   __LINE__ ) );
		}

	return( TRUE );
	}

int testCertChain( void )
	{
	CRYPT_CERTIFICATE cryptCertChain;
	CRYPT_CONTEXT cryptCAKey;
	int value, status;

	puts( "Testing certificate chain creation/export..." );

	/* Get the CA's private key */
	status = getPrivateKey( &cryptCAKey, CA_PRIVKEY_FILE,
							CA_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "CA private key read failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Create a new certificate chain */
	if( !createChain( &cryptCertChain, cryptCAKey, FALSE, TRUE ) )
		return( FALSE );

	/* Check the signature.  Since the chain counts as self-signed, we don't
	   have to supply a sig.check key.  Since the DIY CA certificate isn't 
	   trusted we have to force cryptlib to treat it as explicitly trusted 
	   when we try to verify the chain */
	status = setRootTrust( cryptCertChain, &value, 1 );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertChain, "Setting certificate chain "
							   "trusted", status, __LINE__ ) );
	status = cryptCheckCert( cryptCertChain, CRYPT_UNUSED );
	setRootTrust( cryptCertChain, NULL, value );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertChain, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Try the other way of verifying the chain, by making the signing key
	   implicitly trusted */
	status = cryptSetAttribute( cryptCAKey, CRYPT_CERTINFO_TRUSTED_IMPLICIT,
								TRUE );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertChain, "Setting chain signing key "
							   "trusted", status, __LINE__ ) );
	status = cryptCheckCert( cryptCertChain, CRYPT_UNUSED );
	cryptSetAttribute( cryptCAKey, CRYPT_CERTINFO_TRUSTED_IMPLICIT, FALSE );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertChain, "cryptCheckCert()", status,
							   __LINE__ ) );

	/* Finally, make sure that the non-trusted chain doesn't verify */
	status = cryptCheckCert( cryptCertChain, CRYPT_UNUSED );
	if( cryptStatusOK( status ) )
		{
		printf( "Certificate chain verified OK even though it wasn't "
				"trusted, line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Export the certificate chain */
	status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
							  CRYPT_CERTFORMAT_CERTCHAIN, cryptCertChain );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertChain, "cryptExportCert()", status,
							   __LINE__ ) );
	printf( "Exported certificate chain is %d bytes long.\n", 
			certificateLength );
	debugDump( "certchn", certBuffer, certificateLength );

	/* Destroy the certificate chain */
	status = cryptDestroyCert( cryptCertChain );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that we can read what we created */
	status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
							  &cryptCertChain );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	printf( "Checking signatures... " );
	status = setRootTrust( cryptCertChain, &value, 1 );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertChain, "Setting certificate chain "
							   "trusted", status, __LINE__ ) );
	status = cryptCheckCert( cryptCertChain, CRYPT_UNUSED );
	setRootTrust( cryptCertChain, NULL, value );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCertChain, "cryptCheckCert()", status,
							   __LINE__ ) );
	puts( "signatures verified." );

	/* Display info on each certificate in the chain */
	if( !printCertChainInfo( cryptCertChain ) )
		return( FALSE );

	/* Create a second certificate chain with a null DN.  For this to 
	   succeed we have to set the compliance level to 
	   CRYPT_COMPLIANCELEVEL_PKIX_FULL */
	cryptDestroyCert( cryptCertChain );
	status = createChain( &cryptCertChain, cryptCAKey, TRUE, FALSE );
	if( status != -1 )
		{
		printf( "Attempt to create certificate with null DN %s, line %d.\n",
				( status == FALSE ) ? \
					"failed" : "succeeded when it should have failed",
				__LINE__ );
		return( FALSE );
		}
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
					   &value );
	status = cryptSetAttribute( CRYPT_UNUSED, 
								CRYPT_OPTION_CERT_COMPLIANCELEVEL,
								CRYPT_COMPLIANCELEVEL_PKIX_FULL );
	if( cryptStatusError( status ) )
		{
		/* If the maximum level of PKIX weirdness that cryptlib will allow 
		   is less than CRYPT_COMPLIANCELEVEL_PKIX_FULL then we can't
		   perform this test, so we just skip it */
		if( status != CRYPT_ERROR_PARAM3 )
			{
			printf( "Attempt to set compliance level to "
					"CRYPT_COMPLIANCELEVEL_PKIX_FULL failed with error code "
					"%d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		puts( "(Couldn't set compliance level to "
			  "CRYPT_COMPLIANCELEVEL_PKIX_FULL, probably\n because cryptlib "
			  "has been configured not to use this level, skipping final\n"
			  " tests...)" );
		}
	else
		{
		status = createChain( &cryptCertChain, cryptCAKey, TRUE, TRUE );
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   value );
		if( status != TRUE )
			{
			puts( "  (This may be because the internal compliance-level "
				  "handling is wrong)." );
			return( FALSE );
			}
		status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
								  CRYPT_CERTFORMAT_CERTCHAIN, cryptCertChain );
		cryptDestroyCert( cryptCertChain );
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptCertChain, "cryptExportCert()", 
								   status, __LINE__ ) );
		debugDump( "certchndn", certBuffer, certificateLength );
		status = cryptImportCert( certBuffer, certificateLength, CRYPT_UNUSED,
								  &cryptCertChain );
		if( cryptStatusError( status ) )
			{
			printf( "cryptImportCert() failed with error code %d, line %d.\n",
					status, __LINE__ );
			return( FALSE );
			}
		}

	/* Clean up */
	cryptDestroyCert( cryptCertChain );
	cryptDestroyContext( cryptCAKey );
	puts( "Certificate chain creation succeeded.\n" );
	return( TRUE );
	}

/* Test CMS attribute code.  This doesn't actually test much since this
   object type is just a basic data container used for the extended signing
   functions */

static const CERT_DATA FAR_BSS cmsAttributeData[] = {
	/* Content type and an S/MIME capability */
	{ CRYPT_CERTINFO_CMS_CONTENTTYPE, IS_NUMERIC, CRYPT_CONTENT_SIGNEDDATA },
	{ CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA, IS_NUMERIC, CRYPT_UNUSED },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

int testCMSAttributes( void )
	{
	CRYPT_CERTIFICATE cryptAttributes;
	int status;

	puts( "Testing CMS attribute creation..." );

	/* Create the CMS attribute container */
	status = cryptCreateCert( &cryptAttributes, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CMS_ATTRIBUTES );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Add some CMS attribute components */
	if( !addCertFields( cryptAttributes, cmsAttributeData, __LINE__ ) )
		return( FALSE );

	/* Print information on what we've got */
	if( !printCertInfo( cryptAttributes ) )
		return( FALSE );

	/* Destroy the attributes.  We can't do much more than this at this
	   stage since the attributes are only used internally by other
	   functions */
	status = cryptDestroyCert( cryptAttributes );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	puts( "CMS attribute creation succeeded.\n" );
	return( TRUE );
	}

/* Test RTCS request/response code.  This test routine itself doesn't
   actually test much since this object type is just a basic data container
   used for RTCS sessions, however the shared initRTCS() routine is used by
   the RTCS session code to test the rest of the functionality */

int initRTCS( CRYPT_CERTIFICATE *cryptRTCSRequest, 
			  const CRYPT_CERTIFICATE cryptCert, const int number, 
			  const BOOLEAN multipleCerts )
	{
	CRYPT_CERTIFICATE cryptErrorObject;
	C_CHR rtcsURL[ 512 ];
	int count = DUMMY_INIT, status;

	/* Select the RTCS responder location from the EE certificate and read 
	   the URL/FQDN value (this isn't used but is purely for display to the 
	   user) */
	status = cryptSetAttribute( cryptCert, CRYPT_ATTRIBUTE_CURRENT,
								CRYPT_CERTINFO_AUTHORITYINFO_RTCS );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( cryptCert,
								CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER,
								rtcsURL, &count );
		if( status == CRYPT_ERROR_NOTFOUND )
			status = cryptGetAttributeString( cryptCert,
								CRYPT_CERTINFO_DNSNAME, rtcsURL, &count );
		}
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ERROR_NOTFOUND )
			{
			puts( "RTCS responder URL not present in certificate, server "
				  "name must be provided\n  externally." );
			}
		else
			{
			printf( "Attempt to read RTCS responder URL failed with error "
					"code %d, line %d.\n", status, __LINE__ );
			printErrorAttributeInfo( cryptCert );
			return( FALSE );
			}
		}
	else
		{
#ifdef UNICODE_STRINGS
		rtcsURL[ count / sizeof( wchar_t ) ] = TEXT( '\0' );
		printf( "RTCS responder URL = %sS.\n", rtcsURL );
#else
		rtcsURL[ count ] = '\0';
		printf( "RTCS responder URL = %s.\n", rtcsURL );
#endif /* UNICODE_STRINGS */
		}

	/* Create the RTCS request container */
	status = cryptCreateCert( cryptRTCSRequest, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_RTCS_REQUEST );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	cryptErrorObject = *cryptRTCSRequest;

	/* Add the request components */
	status = cryptSetAttribute( *cryptRTCSRequest,
								CRYPT_CERTINFO_CERTIFICATE, cryptCert );
	if( status == CRYPT_ERROR_PARAM3 )
		cryptErrorObject = cryptCert;
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptErrorObject, "cryptSetAttribute()",
							   status, __LINE__ ) );

	/* If we're doing a query with multiple certs, add another certificate.  
	   To keep things simple and avoid having to stockpile a whole 
	   collection of certificates for each responder we just use a random 
	   certificate for which we expect an 'unknown' response */
	if( multipleCerts )
		{
		CRYPT_CERTIFICATE cryptSecondCert;

		status = importCertFromTemplate( &cryptSecondCert, 
										 CERT_FILE_TEMPLATE, 1 );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( *cryptRTCSRequest,
										CRYPT_CERTINFO_CERTIFICATE, 
										cryptSecondCert );
			if( status == CRYPT_ERROR_PARAM3 )
				cryptErrorObject = cryptSecondCert;
			}
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptErrorObject, "cryptSetAttribute()",
								   status, __LINE__ ) );
		cryptDestroyCert( cryptSecondCert );
		}

	return( TRUE );
	}

int testRTCSReqResp( void )
	{
	CRYPT_CERTIFICATE cryptRTCSRequest, cryptCert;
	int status;

	puts( "Testing RTCS request creation..." );

	/* Import the EE certificate for the RTCS request */
	status = importCertFromTemplate( &cryptCert, RTCS_FILE_TEMPLATE, 
									 1 );
	if( cryptStatusError( status ) )
		{
		printf( "cryptImportCert() failed with error code %d, line %d.\n", 
				status, __LINE__ );
		return( FALSE );
		}

	/* Create the RTCS request using the certs and print information on what
	   we've got */
	if( !initRTCS( &cryptRTCSRequest, cryptCert, 1, FALSE ) )
		return( FALSE );
	cryptDestroyCert( cryptCert );
	if( !printCertInfo( cryptRTCSRequest ) )
		return( FALSE );

	/* Destroy the request.  We can't do much more than this at this stage
	   since the request is only used internally by the RTCS session code */
	status = cryptDestroyCert( cryptRTCSRequest );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	puts( "RTCS request creation succeeded.\n" );
	return( TRUE );
	}

/* Test OCSP request/response code.  This test routine itself doesn't
   actually test much since this object type is just a basic data container
   used for OCSP sessions, however the shared initOCSP() routine is used by
   the OCSP session code to test the rest of the functionality */

int initOCSP( CRYPT_CERTIFICATE *cryptOCSPRequest, const int number,
			  const BOOLEAN ocspv2, const BOOLEAN revokedCert,
			  const BOOLEAN multipleCerts,
			  const CRYPT_SIGNATURELEVEL_TYPE sigLevel,
			  const CRYPT_CONTEXT privKeyContext )
	{
	CRYPT_CERTIFICATE cryptOCSPCA, cryptOCSPEE;
	CRYPT_CERTIFICATE cryptErrorObject;
	C_CHR ocspURL[ 512 ];
	int count = DUMMY_INIT, status;

	assert( !ocspv2 );

	/* Import the OCSP CA (if required) and EE certs */
	if( !ocspv2 )
		{
		status = importCertFromTemplate( &cryptOCSPCA,
										 OCSP_CA_FILE_TEMPLATE, number );
		if( cryptStatusError( status ) )
			{
			printf( "CA cryptImportCert() failed with error code %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		}
	status = importCertFromTemplate( &cryptOCSPEE, revokedCert ? \
						OCSP_EEREV_FILE_TEMPLATE: OCSP_EEOK_FILE_TEMPLATE,
						number );
	if( cryptStatusError( status ) )
		{
		printf( "EE cryptImportCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Select the OCSP responder location from the EE certificate and read 
	   the URL/FQDN value (this isn't used but is purely for display to the 
	   user) */
	status = cryptSetAttribute( cryptOCSPEE, CRYPT_ATTRIBUTE_CURRENT,
								CRYPT_CERTINFO_AUTHORITYINFO_OCSP );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( cryptOCSPEE,
							CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER,
							ocspURL, &count );
		if( status == CRYPT_ERROR_NOTFOUND )
			status = cryptGetAttributeString( cryptOCSPEE,
							CRYPT_CERTINFO_DNSNAME, ocspURL, &count );
		}
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ERROR_NOTFOUND )
			{
			puts( "OCSP responder URL not present in certificate, server "
				  "name must be provided\n  externally." );
			}
		else
			{
			printf( "Attempt to read OCSP responder URL failed with error "
					"code %d, line %d.\n", status, __LINE__ );
			printErrorAttributeInfo( cryptOCSPEE );
			return( FALSE );
			}
		}
	else
		{
#ifdef UNICODE_STRINGS
		ocspURL[ count / sizeof( wchar_t ) ] = TEXT( '\0' );
		printf( "OCSP responder URL = %S.\n", ocspURL );
#else
		ocspURL[ count ] = '\0';
		printf( "OCSP responder URL = %s.\n", ocspURL );
#endif /* UNICODE_STRINGS */
		}

	/* Create the OCSP request container */
	status = cryptCreateCert( cryptOCSPRequest, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_OCSP_REQUEST );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	cryptErrorObject = *cryptOCSPRequest;

	/* Add the request components.  Note that if we're using v1 we have to
	   add the CA certificate first since it's needed to generate the 
	   request ID for the EE certificate */
	if( !ocspv2 )
		{
		status = cryptSetAttribute( *cryptOCSPRequest,
							CRYPT_CERTINFO_CACERTIFICATE, cryptOCSPCA );
		if( status == CRYPT_ERROR_PARAM3 )
			cryptErrorObject = cryptOCSPCA;
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( *cryptOCSPRequest,
									CRYPT_CERTINFO_CERTIFICATE, cryptOCSPEE );
		if( status == CRYPT_ERROR_PARAM3 )
			cryptErrorObject = cryptOCSPEE;
		}
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptErrorObject, "cryptSetAttribute()",
							   status, __LINE__ ) );

	/* If we're doing a query with multiple certs, add another certificate.  
	   To keep things simple and avoid having to stockpile a whole 
	   collection of certificates for each responder we just use a random 
	   certificate for which we expect an 'unknown' response */
	if( multipleCerts )
		{
		cryptDestroyCert( cryptOCSPEE );
		status = importCertFromTemplate( &cryptOCSPEE, CERT_FILE_TEMPLATE, 1 );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( *cryptOCSPRequest,
										CRYPT_CERTINFO_CERTIFICATE, cryptOCSPEE );
			if( status == CRYPT_ERROR_PARAM3 )
				cryptErrorObject = cryptOCSPEE;
			}
		if( cryptStatusError( status ) )
			return( attrErrorExit( *cryptOCSPRequest, "cryptSetAttribute()",
								   status, __LINE__ ) );
		}

	/* If we have a signing key, create a signed request */
	if( privKeyContext != CRYPT_UNUSED )
		{
		status = cryptSetAttribute( *cryptOCSPRequest,
							CRYPT_CERTINFO_SIGNATURELEVEL, sigLevel );
		if( cryptStatusError( status ) )
			return( attrErrorExit( *cryptOCSPRequest, "cryptSetAttribute()",
								   status, __LINE__ ) );
		status = cryptSignCert( *cryptOCSPRequest, privKeyContext );
		if( status == CRYPT_ERROR_PARAM3 )
			cryptErrorObject = privKeyContext;
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptErrorObject, "cryptSignCert()",
								   status, __LINE__ ) );
		}

	/* Clean up */
	if( !ocspv2 )
		cryptDestroyCert( cryptOCSPCA );
	cryptDestroyCert( cryptOCSPEE );

	return( TRUE );
	}

int testOCSPReqResp( void )
	{
	CRYPT_CERTIFICATE cryptOCSPRequest;
	CRYPT_CONTEXT cryptPrivateKey;
	int status;

	puts( "Testing OCSP request creation..." );

	/* Create the OCSP request using the certs and print information on what
	   we've got */
	if( !initOCSP( &cryptOCSPRequest, 1, FALSE, FALSE, FALSE,
				   CRYPT_SIGNATURELEVEL_NONE, CRYPT_UNUSED ) )
		return( FALSE );
	puts( "OCSPv1 succeeded." );
	if( !printCertInfo( cryptOCSPRequest ) )
		return( FALSE );

	/* Destroy the request.  We can't do much more than this at this stage
	   since the request is only used internally by the OCSP session code */
	status = cryptDestroyCert( cryptOCSPRequest );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDestroyCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

#if 0	/* OCSPv2 is still in too much of a state of flux to implement this */
	/* Try again with a v2 request.  This only differs from the v1 request in
	   the way the ID generation is handled so we don't bother printing any
	   information on the request */
	if( !initOCSP( &cryptOCSPRequest, 1, TRUE, FALSE, FALSE,
				   CRYPT_SIGNATURELEVEL_NONE, CRYPT_UNUSED ) )
		return( FALSE );
	puts( "OCSPv2 succeeded." );
	cryptDestroyCert( cryptOCSPRequest );
#endif

	/* Finally, create a signed request, first without and then with signing
	   certs */
	status = getPrivateKey( &cryptPrivateKey, USER_PRIVKEY_FILE,
							USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "User private key read failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( !initOCSP( &cryptOCSPRequest, 1, FALSE, FALSE, FALSE,
				   CRYPT_SIGNATURELEVEL_NONE, cryptPrivateKey ) )
		return( FALSE );
	cryptDestroyCert( cryptOCSPRequest );
	puts( "Signed OCSP request succeeded." );
	if( !initOCSP( &cryptOCSPRequest, 1, FALSE, FALSE, FALSE,
				   CRYPT_SIGNATURELEVEL_SIGNERCERT, cryptPrivateKey ) )
		return( FALSE );
	cryptDestroyCert( cryptOCSPRequest );
	puts( "Signed OCSP request with single signing certificate succeeded." );
	if( !initOCSP( &cryptOCSPRequest, 1, FALSE, FALSE, FALSE,
				   CRYPT_SIGNATURELEVEL_ALL, cryptPrivateKey ) )
		return( FALSE );
	cryptDestroyCert( cryptOCSPRequest );
	puts( "Signed OCSP request with signing certificate chain succeeded." );
	cryptDestroyContext( cryptPrivateKey );

	puts( "OCSP request creation succeeded.\n" );
	return( TRUE );
	}

/* Test PKI user information creation.  This doesn't actually test much
   since this object type is just a basic data container used to hold user
   information in a certificate store */

static const CERT_DATA FAR_BSS pkiUserData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test PKI user" ) },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS pkiUserExtData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test extended PKI user" ) },

	/* SSL server and client authentication */
	{ CRYPT_CERTINFO_EXTKEY_SERVERAUTH, IS_NUMERIC, CRYPT_UNUSED },
	{ CRYPT_CERTINFO_EXTKEY_CLIENTAUTH, IS_NUMERIC, CRYPT_UNUSED },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};
static const CERT_DATA FAR_BSS pkiUserCAData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "Dave's Wetaburgers" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "Procurement" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "Test CA PKI user" ) },

	/* CA extensions */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC,
	  CRYPT_KEYUSAGE_KEYCERTSIGN | CRYPT_KEYUSAGE_CRLSIGN },
	{ CRYPT_CERTINFO_CA, IS_NUMERIC, TRUE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

#define PKIUSER_NAME_INDEX	3	/* Index of name in CERT_DATA info */

static int testPKIUserCreate( const CERT_DATA *pkiUserInfo )
	{
	CRYPT_CERTIFICATE cryptPKIUser;
	int status;

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
	if( !addCertFields( cryptPKIUser, pkiUserInfo, __LINE__ ) )
		{
		printf( "Couldn't create PKI user info for user '%s', line %d.\n",
				( char * ) pkiUserInfo[ PKIUSER_NAME_INDEX ].stringValue, 
				__LINE__ );
		return( FALSE );
		}
	cryptDestroyCert( cryptPKIUser );

	return( TRUE );
	}

int testPKIUser( void )
	{
	puts( "Testing PKI user information creation..." );
	if( !testPKIUserCreate( pkiUserData ) )
		return( FALSE );
	if( !testPKIUserCreate( pkiUserExtData ) )
		return( FALSE );
	if( !testPKIUserCreate( pkiUserCAData ) )
		return( FALSE );
	puts( "PKI user information creation succeeded.\n" );
	return( TRUE );
	}
