/****************************************************************************
*																			*
*					cryptlib Database Keyset Test Routines					*
*						Copyright Peter Gutmann 1995-2009					*
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

#include <ctype.h>

#ifdef TEST_KEYSET

/* A certificate containing an email address */

#define EMAILADDR_CERT_NO	14

/* A certificate chain that can be added to a database keyset and
   looked up again afterwards */

#define CERT_CHAIN_NO		5

/* Some LDAP keyset names and names of probably-present certs and CRLs.
   These keysets (and their contents) come and go, so we have a variety of
   them and try them in turn until something works.  There's a list of more
   LDAP servers at http://www.dante.net/np/pdi.html, but none of these are
   known to contain certificates.

   Note that the following strings have to be given on one line in order for
   the widechar conversion voodoo to work */

static const struct {
	const C_STR url;			/* LDAP URL for keyset */
	const char FAR_BSS *asciiURL;/* URL in ASCII */
	const C_STR certName;		/* DN for certificate and CRL */
	const C_STR crlName;
	} FAR_BSS ldapUrlInfo[] = {
	{ 0 },
	{ TEXT( "ldap://ldap.diginotar.nl:389" ), "ldap.diginotar.nl",
			/* Long URL form also tests LDAP URL-parsing code.  This was 
			   the genuine original LDAP test URL, it's kept in here now 
			   purely for giggles */
	  TEXT( "cn=Root Certificaat Productie, o=DigiNotar Root,c=NL" ),
	  TEXT( "CN=CRL Productie,O=DigiNotar CRL,C=NL" ) },
	{ TEXT( "ds.katalog.posten.se" ), "ds.katalog.posten.se",
	  TEXT( "cn=Posten CertPolicy_eIDKort_1 CA_nyckel_1, o=Posten_Sverige_AB 556451-4148, c=SE" ),
	  TEXT( "cn=Posten CertPolicy_eIDKort_1 CA_nyckel_1, o=Posten_Sverige_AB 556451-4148, c=SE" ) },
	{ TEXT( "ldap2.zebsign.com" ), "ldap2.zebsign.com",
	  TEXT( "pssUniqueIdentifier=24090, CN=First ZebSign Community ID CA, O=ZebSign - 983163432, C=NO" ) }
	};

#define LDAP_SERVER_NO		1
#define LDAP_ALT_SERVER_NO	2	/* Secondary svr.if main server unavailable */

/****************************************************************************
*																			*
*					Database Keyset Read/Write Tests						*
*																			*
****************************************************************************/

/* Certificate with fields designed to cause problems for some keysets */

static const CERT_DATA FAR_BSS sqlCertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, TEXT( "NZ" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, TEXT( "x'); DROP TABLE certificates; --" ) },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, TEXT( "x' OR 1=1; DROP TABLE certificates; --" ) },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, TEXT( "x'; DROP TABLE certificates; --" ) },

	/* Other information */
	{ CRYPT_CERTINFO_SELFSIGNED, IS_NUMERIC, TRUE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

/* Read/write a certificate from a public-key keyset.  Returns
   CRYPT_ERROR_NOTAVAIL if this keyset type isn't available from this
   cryptlib build, CRYPT_ERROR_FAILED if the keyset/data source access
   failed */

enum { READ_OPTION_NORMAL, READ_OPTION_MULTIPLE };

static int checkKeysetCRL( const CRYPT_KEYSET cryptKeyset,
						   const CRYPT_CERTIFICATE cryptCert,
						   const BOOLEAN isCertChain )
	{
	int errorLocus, status;

	/* Perform a revocation check against the CRL in the keyset */
	printf( "Checking certificate%s against CRL.\n", 
			isCertChain ? " chain" : "" );
	status = cryptCheckCert( cryptCert, cryptKeyset );
	if( cryptStatusOK( status ) )
		return( TRUE );
	if( isCertChain )
		{
		/* Checking a chain against a keyset doesn't really make sense, so
		   this should be rejected */
		if( status == CRYPT_ERROR_PARAM2 )
			return( TRUE );

		printf( "Check of certificate chain against keyset returned %d, "
				"should have been %d.\n", status, CRYPT_ERROR_PARAM2 );
		return( FALSE );
		}
	if( status != CRYPT_ERROR_INVALID )
		{
		return( extErrorExit( cryptKeyset, "cryptCheckCert() (for CRL in "
							  "keyset)", status, __LINE__ ) );
		}

	/* If the certificate has expired then it'll immediately be reported as 
	   invalid without bothering to check the CRL, so we have to perform the 
	   check in oblivious mode to avoid the expiry check */
	status = cryptGetAttribute( cryptCert, CRYPT_ATTRIBUTE_ERRORLOCUS, 
								&errorLocus );
	if( cryptStatusOK( status ) && errorLocus == CRYPT_CERTINFO_VALIDTO )
		{
		int complianceValue;

		puts( "  (Certificate has already expired, re-checking in oblivious "
			  "mode)." );
		( void ) cryptGetAttribute( CRYPT_UNUSED, 
									CRYPT_OPTION_CERT_COMPLIANCELEVEL,
									&complianceValue );
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   CRYPT_COMPLIANCELEVEL_OBLIVIOUS );
		status = cryptCheckCert( cryptCert, cryptKeyset );
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CERT_COMPLIANCELEVEL,
						   complianceValue );
		}
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptKeyset, "cryptCheckCert() (for CRL in "
							  "keyset)", status, __LINE__ ) );

	return( TRUE );
	}

static int testKeysetRead( const CRYPT_KEYSET_TYPE keysetType,
						   const C_STR keysetName,
						   const CRYPT_KEYID_TYPE keyType,
						   const C_STR keyName,
						   const CRYPT_CERTTYPE_TYPE type,
						   const int option )
	{
	CRYPT_KEYSET cryptKeyset;
	CRYPT_CERTIFICATE cryptCert;
	BOOLEAN isCertChain = FALSE;
	int value, status;

	/* Open the keyset with a check to make sure this access method exists
	   so we can return an appropriate error message */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, keysetType,
							  keysetName, CRYPT_KEYOPT_READONLY );
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

	/* Read the certificate from the keyset */
	status = cryptGetPublicKey( cryptKeyset, &cryptCert, keyType, keyName );
	if( cryptStatusError( status ) )
		{
		/* The access to network-accessible keysets can be rather
		   temperamental and can fail at this point even though it's not a
		   fatal error.  The calling code knows this and will continue the
		   self-test with an appropriate warning, so we explicitly clean up
		   after ourselves to make sure we don't get a CRYPT_ORPHAN on
		   shutdown */
		if( keysetType == CRYPT_KEYSET_HTTP && \
			status == CRYPT_ERROR_NOTFOUND )
			{
			/* 404's are relatively common, and non-fatal */
			extErrorExit( cryptKeyset, "cryptGetPublicKey()", status, __LINE__ );
			puts( "  (404 is a common HTTP error, and non-fatal)." );
			return( TRUE );
			}

		return( extErrorExit( cryptKeyset, "cryptGetPublicKey()", status,
							  __LINE__ ) );
		}

	/* Make sure that we got what we were expecting */
	status = cryptGetAttribute( cryptCert, CRYPT_CERTINFO_CERTTYPE, 
								&value );
	if( cryptStatusError( status ) || ( value != type ) )
		{
		printf( "Expecting certificate object type %d, got %d, line %d.", 
				type, value, __LINE__ );
		return( FALSE );
		}
	if( value == CRYPT_CERTTYPE_CERTCHAIN )
		isCertChain = TRUE;
	if( value == CRYPT_CERTTYPE_CERTCHAIN || value == CRYPT_CERTTYPE_CRL )
		{
		value = 0;
		cryptSetAttribute( cryptCert, CRYPT_CERTINFO_CURRENT_CERTIFICATE,
						   CRYPT_CURSOR_FIRST );
		do
			value++;
		while( cryptSetAttribute( cryptCert,
								  CRYPT_CERTINFO_CURRENT_CERTIFICATE,
								  CRYPT_CURSOR_NEXT ) == CRYPT_OK );
		printf( isCertChain ? "Certificate chain length = %d.\n" : \
							  "CRL has %d entries.\n", value );
		}

	/* Check the certificate against the CRL.  Any kind of error is a 
	   failure since the certificate isn't in the CRL */
	if( keysetType != CRYPT_KEYSET_LDAP && \
		keysetType != CRYPT_KEYSET_HTTP )
		{
		if( !checkKeysetCRL( cryptKeyset, cryptCert, isCertChain ) )
			return( FALSE );
		}
	cryptDestroyCert( cryptCert );

	/* If we're reading multiple certs using the same (cached) query type,
	   try re-reading the certificate.  This can't be easily tested from the
	   outside because it's database back-end specific, so it'll require
	   attaching a debugger to the read code to make sure that the cacheing
	   is working as required */
	if( option == READ_OPTION_MULTIPLE )
		{
		int i;

		for( i = 0; i < 3; i++ )
			{
			status = cryptGetPublicKey( cryptKeyset, &cryptCert, keyType,
										keyName );
			if( cryptStatusError( status ) )
				{
				printf( "cryptGetPublicKey() with cached query failed with "
						"error code %d, line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			cryptDestroyCert( cryptCert );
			}
		}

	/* Close the keyset */
	status = cryptKeysetClose( cryptKeyset );
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetClose() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

static int testKeysetWrite( const CRYPT_KEYSET_TYPE keysetType,
							const C_STR keysetName )
	{
	CRYPT_KEYSET cryptKeyset;
	CRYPT_CERTIFICATE cryptCert;
	CRYPT_CONTEXT pubKeyContext, privKeyContext;
	C_CHR filenameBuffer[ FILENAME_BUFFER_SIZE ];
	C_CHR name[ CRYPT_MAX_TEXTSIZE + 1 ];
	int length, status;

	/* Import the certificate from a file - this is easier than creating one
	   from scratch.  We use one of the later certs in the test set, since
	   this contains an email address, which the earlier ones don't */
	status = importCertFromTemplate( &cryptCert, CERT_FILE_TEMPLATE, 
									 EMAILADDR_CERT_NO );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't read certificate from file, status %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that the certificate does actually contain an email 
	   address */
	status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_EMAIL,
									  name, &length );
	if( cryptStatusError( status ) )
		{
		printf( "Certificate doesn't contain an email address and can't be "
				"used for testing,\n  line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Create the database keyset with a check to make sure this access
	   method exists so we can return an appropriate error message.  If the
	   database table already exists, this will return a duplicate data
	   error so we retry the open with no flags to open the existing database
	   keyset for write access */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, keysetType,
							  keysetName, CRYPT_KEYOPT_CREATE );
	if( cryptStatusOK( status ) )
		printf( "Created new certificate database '%s'.\n", keysetName );
	if( status == CRYPT_ERROR_PARAM3 )
		{
		/* This type of keyset access isn't available, return a special error
		   code to indicate that the test wasn't performed, but that this
		   isn't a reason to abort processing */
		cryptDestroyCert( cryptCert );
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( status == CRYPT_ERROR_DUPLICATE )
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, keysetType,
								  keysetName, 0 );
	if( cryptStatusError( status ) )
		{
		cryptDestroyCert( cryptCert );
		printf( "cryptKeysetOpen() failed with error code %d, line %d.\n",
				status, __LINE__ );
		if( status == CRYPT_ERROR_OPEN )
			return( CRYPT_ERROR_FAILED );
		return( FALSE );
		}

	/* Write the key to the database */
	puts( "Adding certificate." );
	status = cryptAddPublicKey( cryptKeyset, cryptCert );
	if( status == CRYPT_ERROR_DUPLICATE )
		{
		/* The key is already present, delete it and retry the write */
		status = cryptGetAttributeString( cryptCert,
								CRYPT_CERTINFO_COMMONNAME, name, &length );
		if( cryptStatusOK( status ) )
			{
#ifdef UNICODE_STRINGS
			length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
			name[ length ] = TEXT( '\0' );
			status = cryptDeleteKey( cryptKeyset, CRYPT_KEYID_NAME, name );
			}
		if( cryptStatusError( status ) )
			return( extErrorExit( cryptKeyset, "cryptDeleteKey()", status,
								  __LINE__ ) );
		status = cryptAddPublicKey( cryptKeyset, cryptCert );
		}
	if( cryptStatusError( status ) )
		{
		printExtError( cryptKeyset, "cryptAddPublicKey()", status, __LINE__ );

		/* LDAP writes can fail due to the chosen directory not supporting the
		   schema du jour, so we're a bit more careful about cleaning up since
		   we'll skip the error and continue processing */
		cryptDestroyCert( cryptCert );
		cryptKeysetClose( cryptKeyset );
		return( FALSE );
		}
	cryptDestroyCert( cryptCert );

	/* Add a second certificate with C=US so that we've got enough certs to 
	   properly exercise the query code.  This certificate is highly unusual 
	   in that it doesn't have a DN, so we have to move up the DN looking 
	   for higher-up values, in this case the OU */
	if( keysetType != CRYPT_KEYSET_LDAP )
		{
		status = importCertFromTemplate( &cryptCert, CERT_FILE_TEMPLATE, 2 );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't read certificate from file, status %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptAddPublicKey( cryptKeyset, cryptCert );
		if( status == CRYPT_ERROR_DUPLICATE )
			{
			status = cryptGetAttributeString( cryptCert,
							CRYPT_CERTINFO_COMMONNAME, name, &length );
			if( cryptStatusError( status ) )
				status = cryptGetAttributeString( cryptCert,
							CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, name, &length );
			if( cryptStatusOK( status ) )
				{
#ifdef UNICODE_STRINGS
				length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
				name[ length ] = TEXT( '\0' );
				status = cryptDeleteKey( cryptKeyset, CRYPT_KEYID_NAME, name );
				}
			if( cryptStatusOK( status ) )
				status = cryptAddPublicKey( cryptKeyset, cryptCert );
			}
		if( cryptStatusError( status ) )
			return( extErrorExit( cryptKeyset, "cryptAddPublicKey()",
								  status, __LINE__ ) );
		cryptDestroyCert( cryptCert );
		}

	/* Add a third certificate with a DN that'll cause problems for some 
	   storage technologies */
	if( !loadRSAContexts( CRYPT_UNUSED, &pubKeyContext, &privKeyContext ) )
		return( FALSE );
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
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSetAttribute()", status,
							   __LINE__ ) );
	if( !addCertFields( cryptCert, sqlCertData, __LINE__ ) )
		return( FALSE );
	status = cryptSignCert( cryptCert, privKeyContext );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptCert, "cryptSignCert()", status,
							   __LINE__ ) );
	destroyContexts( CRYPT_UNUSED, pubKeyContext, privKeyContext );
	status = cryptAddPublicKey( cryptKeyset, cryptCert );
	if( cryptStatusError( status ) )
		{
		/* The key is already present, delete it and retry the write */
		status = cryptGetAttributeString( cryptCert,
								CRYPT_CERTINFO_COMMONNAME, name, &length );
		if( cryptStatusOK( status ) )
			{
#ifdef UNICODE_STRINGS
			length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
			name[ length ] = TEXT( '\0' );
			status = cryptDeleteKey( cryptKeyset, CRYPT_KEYID_NAME, name );
			}
		if( cryptStatusError( status ) )
			return( extErrorExit( cryptKeyset, "cryptDeleteKey()", status,
								  __LINE__ ) );
		status = cryptAddPublicKey( cryptKeyset, cryptCert );
		}
	if( cryptStatusError( status ) )
		{
		return( extErrorExit( cryptKeyset, "cryptAddPublicKey()",
							  status, __LINE__ ) );
		}
	cryptDestroyCert( cryptCert );

	/* Now try the same thing with a CRL.  This code also tests the
	   duplicate-detection mechanism, if we don't get a duplicate error
	   there's a problem */
	puts( "Adding CRL." );
	status = importCertFromTemplate( &cryptCert, CRL_FILE_TEMPLATE, 1 );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't read CRL from file, status %d, line %d.\n", 
				status, __LINE__ );
		return( TRUE );
		}
	status = cryptAddPublicKey( cryptKeyset, cryptCert );
	if( cryptStatusError( status ) && status != CRYPT_ERROR_DUPLICATE )
		return( extErrorExit( cryptKeyset, "cryptAddPublicKey()", status,
							  __LINE__ ) );
	status = cryptAddPublicKey( cryptKeyset, cryptCert );
	if( status != CRYPT_ERROR_DUPLICATE )
		{
		printf( "Addition of duplicate item to keyset failed to produce "
			    "CRYPT_ERROR_DUPLICATE, status %d, line %d.\n", status,
				__LINE__ );
		return( FALSE );
		}
	cryptDestroyCert( cryptCert );

	/* Finally, try it with a certificate chain */
	puts( "Adding certificate chain." );
	filenameParamFromTemplate( filenameBuffer, CERTCHAIN_FILE_TEMPLATE, 
							   CERT_CHAIN_NO );
	status = importCertFile( &cryptCert, filenameBuffer );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't read certificate chain from file, status %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptAddPublicKey( cryptKeyset, cryptCert );
	if( cryptStatusError( status ) && status != CRYPT_ERROR_DUPLICATE )
		return( extErrorExit( cryptKeyset, "cryptAddPublicKey()", status,
							  __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* In addition to the other certs we also add the generic user 
	   certificate, which is used later in other tests.  Since it may have 
	   been added earlier we try and delete it first (we can't use the 
	   existing version since the issuerAndSerialNumber won't match the one 
	   in the private-key keyset) */
	status = getPublicKey( &cryptCert, USER_PRIVKEY_FILE,
						   USER_PRIVKEY_LABEL );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't read user certificate from file, status %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_COMMONNAME,
									  name, &length );
	if( cryptStatusError( status ) )
		return( FALSE );
#ifdef UNICODE_STRINGS
	length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
	name[ length ] = TEXT( '\0' );
	do
		status = cryptDeleteKey( cryptKeyset, CRYPT_KEYID_NAME, name );
	while( cryptStatusOK( status ) );
	status = cryptAddPublicKey( cryptKeyset, cryptCert );
	if( status == CRYPT_ERROR_NOTFOUND )
		{
		/* This can occur if a database keyset is defined but hasn't been
		   initialised yet so the necessary tables don't exist, it can be
		   opened but an attempt to add a key will return a not found error
		   since it's the table itself rather than any item within it that
		   isn't being found */
		status = CRYPT_OK;
		}
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptKeyset, "cryptAddPublicKey()", status,
							  __LINE__ ) );
	cryptDestroyCert( cryptCert );

	/* Finally, if ECC is enabled we also add ECC certificates that are used
	   later in other tests */
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ECDSA, NULL ) ) )
		{
#ifdef UNICODE_STRINGS
		wchar_t wcBuffer[ FILENAME_BUFFER_SIZE ];
#endif /* UNICODE_STRINGS */
		void *fileNamePtr = filenameBuffer;

		/* Add the P256 certificate */
		filenameFromTemplate( filenameBuffer, 
							  SERVER_ECPRIVKEY_FILE_TEMPLATE, 256 );
#ifdef UNICODE_STRINGS
		mbstowcs( wcBuffer, filenameBuffer, strlen( filenameBuffer ) + 1 );
		fileNamePtr = wcBuffer;
#endif /* UNICODE_STRINGS */
		status = getPublicKey( &cryptCert, fileNamePtr,
							   USER_PRIVKEY_LABEL );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't read user certificate from file, status %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptAddPublicKey( cryptKeyset, cryptCert );
		if( cryptStatusError( status ) && status != CRYPT_ERROR_DUPLICATE )
			return( extErrorExit( cryptKeyset, "cryptAddPublicKey()", status,
								  __LINE__ ) );
		cryptDestroyCert( cryptCert );

		/* Add the P384 certificate */
		filenameFromTemplate( filenameBuffer, 
							  SERVER_ECPRIVKEY_FILE_TEMPLATE, 384 );
#ifdef UNICODE_STRINGS
		mbstowcs( wcBuffer, filenameBuffer, strlen( filenameBuffer ) + 1 );
		fileNamePtr = wcBuffer;
#endif /* UNICODE_STRINGS */
		status = getPublicKey( &cryptCert, fileNamePtr,
							   USER_PRIVKEY_LABEL );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't read user certificate from file, status %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptAddPublicKey( cryptKeyset, cryptCert );
		if( cryptStatusError( status ) && status != CRYPT_ERROR_DUPLICATE )
			return( extErrorExit( cryptKeyset, "cryptAddPublicKey()", status,
								  __LINE__ ) );
		cryptDestroyCert( cryptCert );
		}

	/* Make sure the deletion code works properly.  This is an artifact of
	   the way RDBMS' work, the delete query can execute successfully but
	   not delete anything so we make sure the glue code correctly
	   translates this into a CRYPT_DATA_NOTFOUND */
	status = cryptDeleteKey( cryptKeyset, CRYPT_KEYID_NAME,
							 TEXT( "Mr.Not Appearing in this Keyset" ) );
	if( status != CRYPT_ERROR_NOTFOUND )
		{
		puts( "Attempt to delete a nonexistant key reports success, the "
			  "database backend glue\ncode needs to be fixed to handle this "
			  "correctly." );
		return( FALSE );
		}

	/* Close the keyset */
	status = cryptKeysetClose( cryptKeyset );
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetClose() failed with error code %d, line %d.\n",
				status, __LINE__ );
		}

	return( TRUE );
	}

/* Perform a general keyset query */

int testQuery( const CRYPT_KEYSET_TYPE keysetType, const C_STR keysetName )
	{
	CRYPT_KEYSET cryptKeyset;
	int count = 0, status;

	/* Open the database keyset */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, keysetType,
							  keysetName, CRYPT_KEYOPT_READONLY );
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetOpen() failed with error code %d, line %d.\n",
				status, __LINE__ );
		if( status == CRYPT_ERROR_OPEN )
			return( CRYPT_ERROR_FAILED );
		return( FALSE );
		}

	/* Send the query to the database and read back the results */
	status = cryptSetAttributeString( cryptKeyset, CRYPT_KEYINFO_QUERY,
									  TEXT( "$C='US'" ),
									  paramStrlen( TEXT( "$C='US'" ) ) );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptKeyset, "Keyset query", status,
							  __LINE__ ) );
	do
		{
		CRYPT_CERTIFICATE cryptCert;

		status = cryptGetPublicKey( cryptKeyset, &cryptCert,
									CRYPT_KEYID_NONE, NULL );
		if( cryptStatusOK( status ) )
			{
			count++;
			cryptDestroyCert( cryptCert );
			}
		}
	while( cryptStatusOK( status ) );
	if( cryptStatusError( status ) && status != CRYPT_ERROR_COMPLETE )
		return( extErrorExit( cryptKeyset, "cryptGetPublicKey()", status,
							  __LINE__ ) );
	if( count < 2 )
		{
		puts( "Only one certificate was returned, this indicates that the "
			  "database backend\nglue code isn't processing ongoing queries "
			  "correctly." );
		return( FALSE );
		}
	printf( "%d certificate(s) matched the query.\n", count );

	/* Close the keyset */
	status = cryptKeysetClose( cryptKeyset );
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetClose() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

/* Read/write/query a certificate from a database keyset */

int testReadCert( void )
	{
	CRYPT_CERTIFICATE cryptCert;
	C_CHR name[ CRYPT_MAX_TEXTSIZE + 1 ], email[ CRYPT_MAX_TEXTSIZE + 1 ];
	C_CHR filenameBuffer[ FILENAME_BUFFER_SIZE ];
	int length, status;

	/* Get the DN from one of the test certs (the one that we wrote to the
	   keyset earlier with testKeysetWrite() */
	status = importCertFromTemplate( &cryptCert, CERT_FILE_TEMPLATE, 
									 EMAILADDR_CERT_NO );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't read certificate from file, status %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_COMMONNAME,
									  name, &length );
	if( cryptStatusOK( status ) )
		{
#ifdef UNICODE_STRINGS
		length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
		name[ length ] = TEXT( '\0' );
		status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_EMAIL,
										  email, &length );
		}
	if( cryptStatusOK( status ) )
		{
		int i;

#ifdef UNICODE_STRINGS
		length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
		email[ length ] = TEXT( '\0' );

		/* Mess up the case to make sure that case-insensitive matching is
		   working */
		for( i = 0; i < length; i++ )
			{
			if( i & 1 )
				email[ i ] = toupper( email[ i ] );
			else
				email[ i ] = tolower( email[ i ] );
			}
		}
	else
		{
		return( extErrorExit( cryptCert, "cryptGetAttributeString()", status,
							  __LINE__ ) );
		}
	cryptDestroyCert( cryptCert );

	puts( "Testing certificate database read..." );
	status = testKeysetRead( DATABASE_KEYSET_TYPE, DATABASE_KEYSET_NAME,
	 						 CRYPT_KEYID_NAME, name,
							 CRYPT_CERTTYPE_CERTIFICATE,
							 READ_OPTION_NORMAL );
	if( status == CRYPT_ERROR_NOTAVAIL )
		{
		/* Database keyset access not available */
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( status == CRYPT_ERROR_FAILED )
		{
		puts( "This is probably because you haven't set up a database or "
			  "data source for use\nas a key database.  For this test to "
			  "work, you need to set up a database/data\nsource with the "
			  "name '" DATABASE_KEYSET_NAME_ASCII "'.\n" );
		return( TRUE );
		}
	if( !status )
		return( FALSE );
	puts( "Reading certs using cached query." );
	status = testKeysetRead( DATABASE_KEYSET_TYPE, DATABASE_KEYSET_NAME,
	 						 CRYPT_KEYID_EMAIL, email,
							 CRYPT_CERTTYPE_CERTIFICATE,
							 READ_OPTION_MULTIPLE );
	if( !status )
		return( FALSE );

	/* Get the DN from one of the test certificate chains */
	filenameParamFromTemplate( filenameBuffer, CERTCHAIN_FILE_TEMPLATE, 
							   CERT_CHAIN_NO );
	status = importCertFile( &cryptCert, filenameBuffer );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't read certificate chain from file, status %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptGetAttributeString( cryptCert, CRYPT_CERTINFO_COMMONNAME,
									  name, &length );
	if( cryptStatusOK( status ) )
		{
#ifdef UNICODE_STRINGS
		length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
		name[ length ] = TEXT( '\0' );
		}
	cryptDestroyCert( cryptCert );

	/* Now read the complete certificate chain */
	puts( "Reading complete certificate chain." );
	status = testKeysetRead( DATABASE_KEYSET_TYPE, DATABASE_KEYSET_NAME,
	 						 CRYPT_KEYID_NAME, name,
							 CRYPT_CERTTYPE_CERTCHAIN, READ_OPTION_NORMAL );
	if( !status )
		return( FALSE );
	puts( "Certificate database read succeeded.\n" );
	return( TRUE );
	}

int testWriteCert( void )
	{
	int status;

	puts( "Testing certificate database write..." );
	status = testKeysetWrite( DATABASE_KEYSET_TYPE, DATABASE_KEYSET_NAME );
	if( status == CRYPT_ERROR_NOTAVAIL )
		{
		/* Database keyset access not available */
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( status == CRYPT_ERROR_FAILED )
		{
		printf( "This may be because you haven't set up a data source "
				"called '" DATABASE_KEYSET_NAME_ASCII "'\nof type %d that "
				"can be used for the certificate store.  You can "
				"configure\nthe data source type and name using the "
				"DATABASE_KEYSET_xxx settings in\ntest/test.h.\n",
				DATABASE_KEYSET_TYPE );
		return( FALSE );
		}
	if( !status )
		return( FALSE );
	puts( "Certificate database write succeeded.\n" );
	return( TRUE );
	}

int testKeysetQuery( void )
	{
	int status;

	puts( "Testing general certificate database query..." );
	status = testQuery( DATABASE_KEYSET_TYPE, DATABASE_KEYSET_NAME );
	if( status == CRYPT_ERROR_NOTAVAIL )
		{
		/* Database keyset access not available */
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( status == CRYPT_ERROR_FAILED )
		{
		puts( "This is probably because you haven't set up a database or "
			  "data source for use\nas a key database.  For this test to "
			  "work, you need to set up a database/data\nsource with the "
			  "name '" DATABASE_KEYSET_NAME_ASCII "'.\n" );
		return( FALSE );
		}
	if( !status )
		return( FALSE );
	puts( "Certificate database query succeeded.\n" );
	return( TRUE );
	}

/* Read/write/query a certificate from an LDAP keyset */

int testReadCertLDAP( void )
	{
	CRYPT_KEYSET cryptKeyset;
	static const char FAR_BSS *ldapErrorString = \
		"LDAP directory read failed, probably because the standard being "
		"used by the\ndirectory server differs from the one used by the "
		"LDAP client software (pick\na standard, any standard).  If you "
		"know how the directory being used is\nconfigured, you can try "
		"changing the CRYPT_OPTION_KEYS_LDAP_xxx settings to\nmatch those "
		"used by the server.  Processing will continue without treating\n"
		"this as a fatal error.\n";
	const C_STR ldapKeysetName = ldapUrlInfo[ LDAP_SERVER_NO ].url;
	char ldapAttribute1[ CRYPT_MAX_TEXTSIZE + 1 ];
	char ldapAttribute2[ CRYPT_MAX_TEXTSIZE + 1 ];
	char certName[ CRYPT_MAX_TEXTSIZE ], caCertName[ CRYPT_MAX_TEXTSIZE ];
	char crlName[ CRYPT_MAX_TEXTSIZE ];
	int length, status;

	/* LDAP directories come and go, check to see which one is currently
	   around */
	puts( "Testing LDAP directory availability..." );
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_LDAP,
							  ldapKeysetName, CRYPT_KEYOPT_READONLY );
	if( status == CRYPT_ERROR_PARAM3 )
		{
		/* LDAP keyset access not available */
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( status == CRYPT_ERROR_OPEN )
		{
		printf( "%s not available for some odd reason,\n  trying "
				"alternative directory instead...\n", 
				ldapUrlInfo[ LDAP_SERVER_NO ].asciiURL );
		ldapKeysetName = ldapUrlInfo[ LDAP_ALT_SERVER_NO ].url;
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_LDAP, ldapKeysetName,
								  CRYPT_KEYOPT_READONLY );
		}
	if( status == CRYPT_ERROR_OPEN )
		{
		printf( "%s not available either.\n",
				ldapUrlInfo[ LDAP_ALT_SERVER_NO ].asciiURL );
		puts( "None of the test LDAP directories are available.  If you need "
			  "to test the\nLDAP capabilities, you need to set up an LDAP "
			  "directory that can be used\nfor the certificate store.  You "
			  "can configure the LDAP directory using the\nLDAP_KEYSET_xxx "
			  "settings in test/test.h.  Alternatively, if this message\n"
			  "took a long time to appear you may be behind a firewall that "
			  "blocks LDAP\ntraffic.\n" );
		return( FALSE );
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetOpen() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptGetAttributeString( CRYPT_UNUSED,
									  CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS,
									  ldapAttribute1, &length );
	if( cryptStatusOK( status ) )
		{
#ifdef UNICODE_STRINGS
		length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
		ldapAttribute1[ length ] = TEXT( '\0' );
		status = cryptGetAttributeString( cryptKeyset,
										  CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS,
										  ldapAttribute2, &length );
		}
	if( cryptStatusOK( status ) )
		{
#ifdef UNICODE_STRINGS
		length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
		ldapAttribute2[ length ] = TEXT( '\0' );
		}
	if( cryptStatusError( status ) || \
		strcmp( ldapAttribute1, ldapAttribute2 ) )
		{
		printf( "Failed to get/set keyset attribute via equivalent global "
				"attribute, error\ncode %d, value '%s', should be\n'%s', "
				"line %d.\n", status, ldapAttribute2, ldapAttribute1,
				__LINE__ );
		return( FALSE );
		}
	cryptKeysetClose( cryptKeyset );
	printf( "  LDAP directory %s seems to be up, using that for read test.\n",
			ldapKeysetName );

	/* Now it gets tricky, we have to jump through all sorts of hoops to
	   run the tests in an automated manner since the innate incompatibility
	   of LDAP directory setups means that even though cryptlib's LDAP access
	   code retries failed queries with various options, we still need to
	   manually override some settings here.  The simplest option is a direct
	   read with no special-case handling */
	if( !paramStrcmp( ldapKeysetName, ldapUrlInfo[ LDAP_SERVER_NO ].url ) )
		{
		puts( "Testing LDAP certificate read..." );
		status = testKeysetRead( CRYPT_KEYSET_LDAP, ldapKeysetName,
								 CRYPT_KEYID_NAME,
								 ldapUrlInfo[ LDAP_SERVER_NO ].certName,
								 CRYPT_CERTTYPE_CERTIFICATE,
								 READ_OPTION_NORMAL );
		if( !status )
			{
			/* Since we can never be sure about the LDAP schema du jour, we
			   don't treat a failure as a fatal error */
			puts( ldapErrorString );
			return( FALSE );
			}

		/* This directory doesn't contain CRLs (or at least not at any known
		   location) so we skip the CRL read test */
		puts( "LDAP certificate read succeeded (CRL read skipped).\n" );
		return( TRUE );
		}

	/* The secondary LDAP directory that we're using for these tests doesn't
	   recognise the ';binary' modifier which is required by LDAP servers in
	   order to get them to work properly, we have to change the attribute
	   name around the read calls to the format expected by the server.

	   In addition because the magic formula for fetching a CRL doesn't seem
	   to work for certificates, the CRL read is done first */
	puts( "Testing LDAP CRL read..." );
	status = cryptGetAttributeString( CRYPT_UNUSED, 
									  CRYPT_OPTION_KEYS_LDAP_CRLNAME,
									  crlName, &length );
	if( cryptStatusError( status ) )
		return( FALSE );
#ifdef UNICODE_STRINGS
	length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
	certName[ length ] = TEXT( '\0' );
	cryptSetAttributeString( CRYPT_UNUSED, CRYPT_OPTION_KEYS_LDAP_CRLNAME,
							 "certificateRevocationList", 25 );
	status = testKeysetRead( CRYPT_KEYSET_LDAP, ldapKeysetName,
							 CRYPT_KEYID_NAME,
							 ldapUrlInfo[ LDAP_ALT_SERVER_NO ].crlName,
							 CRYPT_CERTTYPE_CRL, READ_OPTION_NORMAL );
	cryptSetAttributeString( CRYPT_UNUSED, CRYPT_OPTION_KEYS_LDAP_CRLNAME,
							 crlName, strlen( crlName ) );
	if( !status )
		{
		/* Since we can never be sure about the LDAP schema du jour, we
		   don't treat a failure as a fatal error */
		puts( ldapErrorString );
		return( FALSE );
		}

	puts( "Testing LDAP certificate read..." );
	status = cryptGetAttributeString( CRYPT_UNUSED, 
									  CRYPT_OPTION_KEYS_LDAP_CERTNAME,
									  certName, &length );
	if( cryptStatusError( status ) )
		return( FALSE );
#ifdef UNICODE_STRINGS
	length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
	certName[ length ] = TEXT( '\0' );
	cryptSetAttributeString( CRYPT_UNUSED, CRYPT_OPTION_KEYS_LDAP_CERTNAME,
							 "userCertificate", 15 );
	status = cryptGetAttributeString( CRYPT_UNUSED, 
									  CRYPT_OPTION_KEYS_LDAP_CACERTNAME,
									  caCertName, &length );
	if( cryptStatusError( status ) )
		return( FALSE );
#ifdef UNICODE_STRINGS
	length /= sizeof( wchar_t );
#endif /* UNICODE_STRINGS */
	certName[ length ] = TEXT( '\0' );
	cryptSetAttributeString( CRYPT_UNUSED, CRYPT_OPTION_KEYS_LDAP_CACERTNAME,
							 "cACertificate", 13 );
	status = testKeysetRead( CRYPT_KEYSET_LDAP, ldapKeysetName,
							 CRYPT_KEYID_NAME,
							 ldapUrlInfo[ LDAP_ALT_SERVER_NO ].certName,
							 CRYPT_CERTTYPE_CERTIFICATE, READ_OPTION_NORMAL );
	cryptSetAttributeString( CRYPT_UNUSED, CRYPT_OPTION_KEYS_LDAP_CERTNAME,
							 certName, strlen( certName ) );
	cryptSetAttributeString( CRYPT_UNUSED, CRYPT_OPTION_KEYS_LDAP_CACERTNAME,
							 caCertName, strlen( caCertName ) );
	if( !status )
		{
		/* Since we can never be sure about the LDAP schema du jour, we
		   don't treat a failure as a fatal error */
		puts( "LDAP directory read failed, probably due to the magic "
			  "incantatation to fetch\na certificate from this server not "
			  "matching the one used to fetch a CRL.\nProcessing will "
			  "continue without treating this as a fatal error.\n" );
		return( FALSE );
		}
	puts( "LDAP certificate/CRL read succeeded.\n" );

	return( TRUE );
	}

int testWriteCertLDAP( void )
	{
	int status;

	puts( "Testing LDAP directory write..." );
	status = testKeysetWrite( CRYPT_KEYSET_LDAP,
							  ldapUrlInfo[ LDAP_SERVER_NO ].url );
	if( status == CRYPT_ERROR_NOTAVAIL )
		{
		/* LDAP keyset access not available */
		return( CRYPT_ERROR_NOTAVAIL );
		}
	if( status == CRYPT_ERROR_FAILED )
		{
		printf( "This is probably because you haven't set up an LDAP "
				"directory for use as the\nkey store.  For this test to "
				"work,you need to set up a directory with the\nname "
				"'%s'.\n\n", ldapUrlInfo[ LDAP_SERVER_NO ].asciiURL );
		return( FALSE );
		}
	if( !status )
		{
		/* Since we can never be sure about the LDAP schema du jour, we
		   don't treat a failure as a fatal error */
		puts( "LDAP directory write failed, probably due to the standard "
			  "being used by the\ndirectory differing from the one used "
			  "by cryptlib (pick a standard, any\nstandard).  Processing "
			  "will continue without treating this as a fatal error.\n" );
		return( FALSE );
		}
	puts( "LDAP directory write succeeded.\n" );
	return( TRUE );
	}

/* Read a certificate from a web page */

int testReadCertURL( void )
	{
	int status;

	/* Test fetching a certificate from a URL via an HTTP proxy.  This 
	   requires a random open proxy for testing (unless the site happens to 
	   be running an HTTP proxy), www.openproxies.com will provide this.
	   
	   Alternatively, for testing from a NZ-local ISP, proxy.xtra.co.nz:8080
	   can also be used */
#if 0	/* This is usually disabled because most people aren't behind HTTP
		   proxies, and abusing other people's misconfigured HTTP caches/
		   proxies for testing isn't very nice */
	puts( "Testing HTTP certificate read from URL via proxy..." );
	cryptSetAttributeString( CRYPT_UNUSED, CRYPT_OPTION_NET_HTTP_PROXY,
							 "proxy.zetwuinwest.com.pl:80", 27 );
	status = testKeysetRead( CRYPT_KEYSET_HTTP, HTTP_KEYSET_CERT_NAME,
							 CRYPT_KEYID_NAME, "[none]",
							 CRYPT_CERTTYPE_CERTIFICATE, READ_OPTION_NORMAL );
	if( status == CRYPT_ERROR_NOTAVAIL )	/* HTTP keyset access not avail.*/
		return( CRYPT_ERROR_NOTAVAIL );
	if( !status )
		return( FALSE );
#endif /* 0 */

	/* Test fetching a certificate from a URL */
	puts( "Testing HTTP certificate read from URL..." );
	status = testKeysetRead( CRYPT_KEYSET_HTTP, HTTP_KEYSET_CERT_NAME,
							 CRYPT_KEYID_NAME, TEXT( "[none]" ),
							 CRYPT_CERTTYPE_CERTIFICATE, READ_OPTION_NORMAL );
	if( status == CRYPT_ERROR_NOTAVAIL )	/* HTTP keyset access not avail.*/
		return( CRYPT_ERROR_NOTAVAIL );
	if( status == CRYPT_ERROR_FAILED )
		{
		puts( "This is probably because the server isn't available or "
			  "inaccessible.\n" );
		return( TRUE );
		}
	if( !status )
		{
		puts( "If this message took a long time to appear, you may be "
			  "behind a firewall\nthat blocks HTTP traffic.\n" );
		return( FALSE );
		}

	/* Test fetching a CRL from a URL */
	puts( "Testing HTTP CRL read from URL..." );
	status = testKeysetRead( CRYPT_KEYSET_HTTP, HTTP_KEYSET_CRL_NAME,
							 CRYPT_KEYID_NAME, TEXT( "[none]" ),
							 CRYPT_CERTTYPE_CRL, READ_OPTION_NORMAL );
	if( status == CRYPT_ERROR_NOTAVAIL )	/* HTTP keyset access not avail.*/
		return( CRYPT_ERROR_NOTAVAIL );
	if( !status )
		return( FALSE );

	/* Test fetching a huge CRL from a URL, to check the ability to read
	   arbitrary-length HTTP data */
#if 1	/* We allow this to be disabled because of the CRL size */
	puts( "Testing HTTP mega-CRL read from URL..." );
	status = testKeysetRead( CRYPT_KEYSET_HTTP, HTTP_KEYSET_HUGECRL_NAME,
							 CRYPT_KEYID_NAME, TEXT( "[none]" ), 
							 CRYPT_CERTTYPE_CRL, READ_OPTION_NORMAL );
	if( status == CRYPT_ERROR_NOTAVAIL )	/* HTTP keyset access not avail.*/
		return( CRYPT_ERROR_NOTAVAIL );
	if( !status )
		return( FALSE );
#endif /* 0 */

	puts( "HTTP certificate/CRL read from URL succeeded.\n" );
	return( TRUE );
	}

/* Read a certificate from an HTTP certificate store */

int testReadCertHTTP( void )
	{
	int status;

	puts( "Testing HTTP certificate store read..." );
	status = testKeysetRead( CRYPT_KEYSET_HTTP, HTTP_KEYSET_CERT_NAME,
							 CRYPT_KEYID_NAME, TEXT( "Verisign" ),
							 CRYPT_CERTTYPE_CERTIFICATE, READ_OPTION_NORMAL );
	if( status == CRYPT_ERROR_NOTAVAIL )	/* HTTP keyset access not avail.*/
		return( CRYPT_ERROR_NOTAVAIL );
	if( status == CRYPT_ERROR_FAILED )
		{
		puts( "This is probably because the server isn't available or "
			  "inaccessible.\n" );
		return( TRUE );
		}
	if( !status )
		{
		puts( "If this message took a long time to appear, you may be "
			  "behind a firewall\nthat blocks HTTP traffic.\n" );
		return( FALSE );
		}

	puts( "HTTP certificate store read succeeded.\n" );
	return( TRUE );
	}

#endif /* TEST_KEYSET */
