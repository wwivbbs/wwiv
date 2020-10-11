/****************************************************************************
*																			*
*					cryptlib Mid and High-Level Test Routines				*
*						Copyright Peter Gutmann 1995-2016					*
*																			*
****************************************************************************/

#include <limits.h>		/* To determine max.buffer size we can encrypt */
#include "cryptlib.h"
#include "test/test.h"

/* Various features can be disabled by configuration options, in order to 
   handle this we need to include the cryptlib config file so that we can 
   selectively disable some tests.
   
   Note that this checking isn't perfect, if cryptlib is built in release
   mode but we include config.h here in debug mode then the defines won't
   match up because the use of debug mode enables extra options that won't
   be enabled in the release-mode cryptlib */
#include "misc/config.h"	/* For checking availability of test options */
#include "misc/consts.h"	/* For DEFAULT_CRYPT_ALGO */

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */
#ifndef NDEBUG
  #include "misc/analyse.h"		/* Needed for fault.h */
  #include "misc/fault.h"
#endif /* !NDEBUG */

#ifndef max				/* Some compilers don't define this */
  #define max( a, b )	( ( ( a ) > ( b ) ) ? ( ( int ) a ) : ( ( int ) b ) )
#endif /* !max */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#if defined( TEST_MIDLEVEL ) || defined( TEST_HIGHLEVEL )

/* Test whether two session keys are identical */

static int compareSessionKeys( const CRYPT_CONTEXT cryptContext1,
							   const CRYPT_CONTEXT cryptContext2 )
	{
	BYTE buffer[ CRYPT_MAX_IVSIZE ];
	int blockSize, ivSize, status;

	status = cryptGetAttribute( cryptContext1, CRYPT_CTXINFO_BLOCKSIZE, 
								&blockSize );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttribute( cryptContext1, CRYPT_CTXINFO_IVSIZE,
									&ivSize );
		}
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttributeString( cryptContext1, CRYPT_CTXINFO_IV,
							 "\x00\x00\x00\x00\x00\x00\x00\x00"
							 "\x00\x00\x00\x00\x00\x00\x00\x00", ivSize );
	cryptSetAttributeString( cryptContext2, CRYPT_CTXINFO_IV,
							 "\x00\x00\x00\x00\x00\x00\x00\x00"
							 "\x00\x00\x00\x00\x00\x00\x00\x00", ivSize );
	memcpy( buffer, "0123456789ABCDEF", max( blockSize, 8 ) );
	status = cryptEncrypt( cryptContext1, buffer, max( blockSize, 8 ) );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptEncrypt() with first key failed with "
				 "error code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptDecrypt( cryptContext2, buffer, max( blockSize, 8 ) );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptDecrypt() with second key failed with "
				 "error code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( memcmp( buffer, "0123456789ABCDEF", max( blockSize, 8 ) ) )
		{
		fputs( "Data decrypted with key2 != plaintext encrypted with key1.", 
			   outputStream );
		return( FALSE );
		}
	return( TRUE );
	}

/* Test whether two hash/MAC values are identical */

static int compareHashValues( const CRYPT_CONTEXT hashContext1,
							  const CRYPT_CONTEXT hashContext2 )
	{
	const BYTE *data = ( BYTE * ) "0123456789ABCDEF"; 
	BYTE hash1[ CRYPT_MAX_HASHSIZE ], hash2[ CRYPT_MAX_HASHSIZE ];
	int length1, length2, status;

	status = cryptEncrypt( hashContext1, ( void * ) data, 16 );
	if( cryptStatusOK( status ) )
		status = cryptEncrypt( hashContext1, "", 0 );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( hashContext1, CRYPT_CTXINFO_HASHVALUE,
										  &hash1, &length1 );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptEncrypt() with first hash/MAC context "
				 "failed with error code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptEncrypt( hashContext2, ( void * ) data, 16 );
	if( cryptStatusOK( status ) )
		status = cryptEncrypt( hashContext2, "", 0 );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( hashContext2, CRYPT_CTXINFO_HASHVALUE,
										  &hash2, &length2 );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptEncrypt() with second hash/MAC context "
				 "failed with error code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( length1 != length2 || memcmp( hash1, hash2, length1 ) )
		{
		fputs( "Hash/MAC value from context1 != hash/MAC value from "
			  "context2.", outputStream );
		return( FALSE );
		}
	return( TRUE );
	}
#endif /* TEST_MIDLEVEL || TEST_HIGHLEVEL */

/****************************************************************************
*																			*
*							Mid-level Routines Test							*
*																			*
****************************************************************************/

#ifdef TEST_MIDLEVEL

/* General-purpose routines to perform a key exchange and sign and sig
   check data */

static int signData( const char *algoName, const CRYPT_ALGO_TYPE algorithm,
					 const CRYPT_CONTEXT externalSignContext,
					 const CRYPT_CONTEXT externalCheckContext,
					 const BOOLEAN useSidechannelProtection,
					 const BOOLEAN useSHA2,
					 const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_OBJECT_INFO cryptObjectInfo;
	CRYPT_CONTEXT signContext, checkContext;
	CRYPT_CONTEXT hashSignContext, hashCheckContext;
	const int extraData = ( formatType == CRYPT_FORMAT_CMS || \
							formatType == CRYPT_FORMAT_SMIME ) ? \
							CRYPT_USE_DEFAULT : CRYPT_UNUSED;
	BYTE buffer[ 1024 ], hashBuffer[] = "abcdefghijklmnopqrstuvwxyz";
	int status, value, length;

	fprintf( outputStream, "Testing %s%s digital signature%s...\n",
			 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "", algoName,
			 useSidechannelProtection ? " with side-channel protection" : "" );

	/* Create SHA/SHA2 hash contexts and hash the test buffer.  We don't
	   complete the hashing if it's a PGP signature since this hashes in
	   extra data before generating the signature.  In addition we use two
	   hash contexts, one to create the signature and the other to verify
	   it, to eliminate common-mode failures and, in the PGP case, because
	   the signature-generation process completes the hashing so we'd need
	   to reset the context before we could use the same one for the
	   checking step */
	status = cryptCreateContext( &hashSignContext, CRYPT_UNUSED,
								 useSHA2 ? CRYPT_ALGO_SHA2 : \
										   CRYPT_ALGO_SHA1 );
	if( cryptStatusOK( status ) )
		{
		status = cryptCreateContext( &hashCheckContext, CRYPT_UNUSED,
									 useSHA2 ? CRYPT_ALGO_SHA2 : \
											   CRYPT_ALGO_SHA1 );
		}
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptEncrypt( hashSignContext, hashBuffer, 26 );
	cryptEncrypt( hashCheckContext, hashBuffer, 26 );
	if( formatType != CRYPT_FORMAT_PGP )
		{
		cryptEncrypt( hashSignContext, hashBuffer, 0 );
		cryptEncrypt( hashCheckContext, hashBuffer, 0 );
		}

	/* Create the appropriate en/decryption contexts */
	if( externalSignContext != CRYPT_UNUSED )
		{
		signContext = externalSignContext;
		checkContext = externalCheckContext;
		}
	else
		{
		switch( algorithm )
			{
			case CRYPT_ALGO_RSA:
				if( useSHA2 )
					{
					/* If we're using SHA-2 then we have to use a larger RSA 
					   key to ensure that the hash values fits into the 
					   signature data */
					status = loadRSAContextsLarge( CRYPT_UNUSED, &checkContext,
												   &signContext );
					}
				else
					{
					status = loadRSAContexts( CRYPT_UNUSED, &checkContext,
											  &signContext );
					}
				break;

			case CRYPT_ALGO_DSA:
				status = loadDSAContexts( CRYPT_UNUSED, &checkContext,
										  &signContext );
				break;

			case CRYPT_ALGO_ECDSA:
				status = loadECDSAContexts( CRYPT_UNUSED, &checkContext, 
											&signContext );
				break;

			default:
				status = FALSE;
			}
		if( !status )
			return( FALSE );
		}

	/* Find out how big the signature will be */
	status = cryptCreateSignatureEx( NULL, 0, &length, formatType,
									 signContext, hashSignContext, extraData );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptCreateSignatureEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	fprintf( outputStream, "cryptCreateSignatureEx() reports signature object "
			 "will be %d bytes long\n", length );
	assert( length <= 1024 );

	/* Sign the hashed data */
	if( useSidechannelProtection )
		{
		( void ) cryptGetAttribute( CRYPT_UNUSED,
									CRYPT_OPTION_MISC_SIDECHANNELPROTECTION, 
									&value );
		cryptSetAttribute( CRYPT_UNUSED,
						   CRYPT_OPTION_MISC_SIDECHANNELPROTECTION, 1 );
		}
	status = cryptCreateSignatureEx( buffer, 1024, &length, formatType,
									 signContext, hashSignContext, extraData );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptCreateSignatureEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( useSidechannelProtection && value != 1 )
		{
		cryptSetAttribute( CRYPT_UNUSED,
						   CRYPT_OPTION_MISC_SIDECHANNELPROTECTION, value );
		}

	/* Query the signed object */
	status = cryptQueryObject( buffer, length, &cryptObjectInfo );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptQueryObject() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	fprintf( outputStream, "cryptQueryObject() reports object type %d, "
			 "algorithm %d, hash algorithm %d.\n", 
			 cryptObjectInfo.objectType, cryptObjectInfo.cryptAlgo, 
			 cryptObjectInfo.hashAlgo );
	memset( &cryptObjectInfo, 0, sizeof( CRYPT_OBJECT_INFO ) );
	if( formatType == CRYPT_FORMAT_CRYPTLIB )
		{
		debugDump( ( algorithm == CRYPT_ALGO_DSA ) ? "sigd" : \
				   ( algorithm == CRYPT_ALGO_ECDSA ) ? "sigec" : \
				   useSHA2 ? "sigr2" : "sigr", buffer, length );
		}
	else
		{
		debugDump( ( algorithm == CRYPT_ALGO_RSA ) ? \
				   "sigr.pgp" : "sigd.pgp", buffer, length );
		}

	/* Check the signature on the hash */
	status = cryptCheckSignature( buffer, length, checkContext, 
								  hashCheckContext );
	if( cryptStatusError( status ) )
		{
		cryptDestroyContext( hashSignContext );
		cryptDestroyContext( hashCheckContext );
		if( externalSignContext == CRYPT_UNUSED )
			destroyContexts( CRYPT_UNUSED, checkContext, signContext );
		fprintf( outputStream, "cryptCheckSignature() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptDestroyContext( hashSignContext );
	cryptDestroyContext( hashCheckContext );
	if( externalSignContext == CRYPT_UNUSED )
		destroyContexts( CRYPT_UNUSED, checkContext, signContext );
	fprintf( outputStream, "Generation and checking of %s digital signature "
			 "succeeded.\n\n", algoName );
	return( TRUE );
	}

static int keyExportImport( const char *algoName,
							const CRYPT_ALGO_TYPE algorithm,
							const CRYPT_CONTEXT externalCryptContext,
							const CRYPT_CONTEXT externalDecryptContext,
							const CRYPT_FORMAT_TYPE formatType )
	{
	const CRYPT_ALGO_TYPE cryptAlgo = ( formatType == CRYPT_FORMAT_PGP ) ? \
									  CRYPT_ALGO_IDEA : CRYPT_ALGO_RC2;
	CRYPT_OBJECT_INFO cryptObjectInfo;
	CRYPT_CONTEXT cryptContext, decryptContext;
	CRYPT_CONTEXT sessionKeyContext1, sessionKeyContext2;
	BYTE *buffer;
	int status, length;

	fprintf( outputStream, "Testing %s%s public-key export/import...\n",
			 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "", algoName );

	/* Create encryption contexts for the session key.  PGP stores the
	   session key information with the encrypted key data, so we can't
	   create the context at this point */
	status = cryptCreateContext( &sessionKeyContext1, CRYPT_UNUSED,
								 selectCipher( cryptAlgo ) );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttribute( sessionKeyContext1, CRYPT_CTXINFO_MODE,
					   CRYPT_MODE_CFB );
	status = cryptGenerateKey( sessionKeyContext1 );
	if( cryptStatusError( status ) )
		return( FALSE );
	if( formatType != CRYPT_FORMAT_PGP )
		{
		status = cryptCreateContext( &sessionKeyContext2, CRYPT_UNUSED,
									 selectCipher( cryptAlgo ) );
		if( cryptStatusError( status ) )
			return( FALSE );
		cryptSetAttribute( sessionKeyContext2, CRYPT_CTXINFO_MODE,
						   CRYPT_MODE_CFB );
		}

	/* Create the appropriate en/decryption contexts */
	if( externalCryptContext != CRYPT_UNUSED )
		{
		cryptContext = externalCryptContext;
		decryptContext = externalDecryptContext;
		}
	else
		{
		if( algorithm == CRYPT_ALGO_ELGAMAL )
			status = loadElgamalContexts( &cryptContext, &decryptContext );
		else
			{
			status = loadRSAContexts( CRYPT_UNUSED, &cryptContext, 
									  &decryptContext );
			}
		if( !status )
			return( FALSE );
		}

	/* Find out how big the exported key will be */
	status = cryptExportKeyEx( NULL, 0, &length, formatType, cryptContext,
							   sessionKeyContext1 );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptExportKeyEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	fprintf( outputStream, "cryptExportKeyEx() reports exported key object "
			 "will be %d bytes long\n", length );
	if( ( buffer = malloc( length ) ) == NULL )
		return( FALSE );

	/* Export the session key */
	status = cryptExportKeyEx( buffer, length, &length, formatType,
							   cryptContext, sessionKeyContext1 );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptExportKeyEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}

	/* Query the encrypted key object */
	status = cryptQueryObject( buffer, length, &cryptObjectInfo );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptQueryObject() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}
	fprintf( outputStream, "cryptQueryObject() reports object type %d, "
			 "algorithm %d.\n", cryptObjectInfo.objectType, 
			 cryptObjectInfo.cryptAlgo );
	memset( &cryptObjectInfo, 0, sizeof( CRYPT_OBJECT_INFO ) );
	if( formatType == CRYPT_FORMAT_CRYPTLIB )
		{
		debugDump( ( algorithm == CRYPT_ALGO_RSA ) ? \
				   "keytrans" : "keytr_el", buffer, length );
		}
	else
		{
		debugDump( ( algorithm == CRYPT_ALGO_RSA ) ? \
				   "keytrans.pgp" : "keytr_el.pgp", buffer, length );
		}

	/* Recreate the session key by importing the encrypted key */
	if( formatType == CRYPT_FORMAT_PGP )
		{
		status = cryptImportKeyEx( buffer, length, decryptContext,
								   CRYPT_UNUSED, &sessionKeyContext2 );
		}
	else
		{
		status = cryptImportKeyEx( buffer, length, decryptContext,
								   sessionKeyContext2, NULL );
		}
	if( cryptStatusError( status ) )
		{
		destroyContexts( CRYPT_UNUSED, sessionKeyContext1, sessionKeyContext2 );
		if( externalCryptContext == CRYPT_UNUSED )
			destroyContexts( CRYPT_UNUSED, cryptContext, decryptContext );
		fprintf( outputStream, "cryptImportKeyEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}

	/* Make sure that the two keys match */
	if( !compareSessionKeys( sessionKeyContext1, sessionKeyContext2 ) )
		return( FALSE );

	/* Clean up */
	destroyContexts( CRYPT_UNUSED, sessionKeyContext1, sessionKeyContext2 );
	if( externalCryptContext == CRYPT_UNUSED )
		destroyContexts( CRYPT_UNUSED, cryptContext, decryptContext );
	fprintf( outputStream, "Export/import of session key via %s-encrypted "
			 "data block\n  succeeded.\n\n", algoName );
	free( buffer );
	return( TRUE );
	}

/* Test the ability to encrypt a large amount of data */

int testLargeBufferEncrypt( void )
	{
	CRYPT_CONTEXT cryptContext;
	CRYPT_QUERY_INFO cryptQueryInfo;
	BYTE *buffer;
	const size_t length = ( INT_MAX <= 32768L ) ? 16384 : 1048576;
	int i, status;

	fputs( "Testing encryption of large data quantity...\n", outputStream );

	status = cryptQueryCapability( DEFAULT_CRYPT_ALGO, &cryptQueryInfo );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Allocate a large buffer and fill it with a known value */
	if( ( buffer = malloc( length ) ) == NULL )
		{
		fprintf( outputStream, "Couldn't allocate buffer of %ld bytes, "
				 "skipping large buffer encryption test.\n", 
				 ( long ) length );
		return( TRUE );
		}
	memset( buffer, '*', length );

	/* Encrypt the buffer */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 DEFAULT_CRYPT_ALGO );
	if( cryptStatusError( status ) )
		{
		free( buffer );
		return( FALSE );
		}
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEY, 
							 "123456789012345678901234", 
							 cryptQueryInfo.keySize );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_IV,
							 "\x00\x00\x00\x00\x00\x00\x00\x00" \
							 "\x00\x00\x00\x00\x00\x00\x00\x00", 
							 cryptQueryInfo.blockSize );
	status = cryptEncrypt( cryptContext, buffer, length );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptEncrypt() of large data quantity "
				 "failed with error code %d, line %d.\n", status, 
				 __LINE__ );
		free( buffer );
		return( FALSE );
		}
	cryptDestroyContext( cryptContext );

	/* Decrypt the buffer */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 DEFAULT_CRYPT_ALGO );
	if( cryptStatusError( status ) )
		{
		free( buffer );
		return( FALSE );
		}
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEY,
							 "123456789012345678901234", 
							 cryptQueryInfo.keySize );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_IV,
							 "\x00\x00\x00\x00\x00\x00\x00\x00" \
							 "\x00\x00\x00\x00\x00\x00\x00\x00", 
							 cryptQueryInfo.blockSize );
	status = cryptDecrypt( cryptContext, buffer, length );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptDecrypt() of large data quantity "
				 "failed with error code %d, line %d.\n", status, 
				 __LINE__ );
		free( buffer );
		return( FALSE );
		}
	cryptDestroyContext( cryptContext );

	/* Make sure that it went OK */
	for( i = 0; i < ( int ) length; i++ )
		{
		if( buffer[ i ] != '*' )
			{
			fprintf( outputStream, "Decrypted data != original plaintext "
					 "at position %d, line %d.\n", i, __LINE__ );
			free( buffer );
			return( FALSE );
			}
		}

	/* Clean up */
	free( buffer );
	fprintf( outputStream , "Encryption of %ld bytes of data "
			 "succeeded.\n\n", ( long ) length );
	return( TRUE );
	}

/* Test the code to derive a fixed-length encryption/MAC key from a 
   variable-length user key */

static int deriveKey( const C_STR userKey, const int userKeyLength,
					  const C_STR description, BOOLEAN useMAC, 
					  BOOLEAN useAltDeriveAlgo )
	{
	CRYPT_CONTEXT cryptContext, decryptContext;
	int hashAlgo, status;

	fprintf( outputStream, "Testing %s key derivation ", 
			 useMAC ? "MAC" : "encryption" );
	if( useAltDeriveAlgo )
		fprintf( outputStream, "using alternative derivation algorithm" );
	else
		fprintf( outputStream, "from %s", description );
	fputs( "...\n", outputStream );

	/* Find out what the default key derivation algorithm is.  This is 
	   required because we need to use something other than the default
	   for the useAltDeriveAgo test */
	status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_KEYING_ALGO, 
								&hashAlgo );
	if( cryptStatusError( status ) )
		return( FALSE );

	if( useMAC )
		{
		/* Create HMAC-SHA1 MAC contexts */
		status = cryptCreateContext( &cryptContext, CRYPT_UNUSED,
									 CRYPT_ALGO_HMAC_SHA1 );
		if( cryptStatusOK( status ) )
			{
			status = cryptCreateContext( &decryptContext, CRYPT_UNUSED,
										 CRYPT_ALGO_HMAC_SHA1 );
			}
		}
	else
		{
		/* Create IDEA/CBC encryption and decryption contexts/and load them 
		   with identical salt values for the key derivation (this is easier 
		   than reading the salt from one and writing it to the other) */
		status = cryptCreateContext( &cryptContext, CRYPT_UNUSED,
									 selectCipher( CRYPT_ALGO_IDEA ) );
		if( cryptStatusOK( status ) )
			{
			status = cryptCreateContext( &decryptContext, CRYPT_UNUSED,
										 selectCipher( CRYPT_ALGO_IDEA ) );
			}
		}
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEYING_SALT,
							 "\x12\x34\x56\x78\x78\x56\x34\x12", 8 );
	cryptSetAttributeString( decryptContext, CRYPT_CTXINFO_KEYING_SALT,
							 "\x12\x34\x56\x78\x78\x56\x34\x12", 8 );
	if( useAltDeriveAlgo )
		{
		const CRYPT_ALGO_TYPE altAlgo = 
				( hashAlgo == CRYPT_ALGO_HMAC_SHA1 ) ? \
				CRYPT_ALGO_HMAC_SHA2 : CRYPT_ALGO_HMAC_SHA1;

		cryptSetAttribute( cryptContext, CRYPT_CTXINFO_KEYING_ALGO,
						   altAlgo );
		cryptSetAttribute( decryptContext, CRYPT_CTXINFO_KEYING_ALGO,
						   altAlgo );
		}

	/* Load a key derived from a user key into both contexts */
	status = cryptSetAttributeString( cryptContext,
									  CRYPT_CTXINFO_KEYING_VALUE,
									  userKey, userKeyLength );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Key derivation failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttributeString( decryptContext,
									  CRYPT_CTXINFO_KEYING_VALUE,
									  userKey, userKeyLength );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Key derivation failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that the two derived keys match */
	if( useMAC )
		{
		if( !compareHashValues( cryptContext, decryptContext ) )
			return( FALSE );
		}
	else
		{
		if( !compareSessionKeys( cryptContext, decryptContext ) )
			return( FALSE );
		}

	/* Clean up */
	destroyContexts( CRYPT_UNUSED, cryptContext, decryptContext );

	fprintf( outputStream, "%s key derivation succeeded.\n\n", 
			 useMAC ? "MAC" : "Encryption" );
	return( TRUE );
	}

int testDeriveKey( void )
	{
	CRYPT_CONTEXT cryptContext;
	C_STR shortUserKey = TEXT( "12345678" );
	C_STR medUserKey = TEXT( "This is a long user key for key derivation testing" );
	C_STR longUserKey = TEXT( "This is a really long user key that exceeds the HMAC input limit for keys" );
#ifdef USE_3DES
	BYTE buffer[ 8 ];
#endif /* USE_3DES */
	int value DUMMY_INIT, status;

	/* Make sure that we can get/set the keying values with equivalent
	   systemwide settings using either the context-specific or global
	   option attributes */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 DEFAULT_CRYPT_ALGO );
	if( cryptStatusError( status ) )
		return( FALSE );
	status = cryptSetAttribute( cryptContext,
								CRYPT_CTXINFO_KEYING_ITERATIONS, 5 );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttribute( cryptContext,
									CRYPT_OPTION_KEYING_ITERATIONS,
									&value );
		}
	cryptDestroyContext( cryptContext );
	if( cryptStatusError( status ) || value != 5 )
		{
		if( cryptStatusError( status ) )
			value = -1;	/* Set dummy value if read failed */
		fprintf( outputStream, "Failed to get/set context attribute via "
				 "equivalent global attribute, error\ncode %d, value %d "
				 "(should be 5), line %d.\n", status, value, __LINE__ );
		return( FALSE );
		}

	/* Test the derivation of encryption keys from short, medium, and long 
	   passwords */
	status = deriveKey( shortUserKey, paramStrlen( shortUserKey ), 
						"short user key", FALSE, FALSE );
	if( status ) 
		{
		status = deriveKey( medUserKey, paramStrlen( medUserKey ), 
							"medium user key", FALSE, FALSE );
		}
	if( status ) 
		{
		status = deriveKey( longUserKey, paramStrlen( longUserKey ), 
							"long user key", FALSE, FALSE );
		}
	if( !status ) 
		return( FALSE );

	/* Test the derivation of MAC keys from short, medium, and long 
	   passwords */
	status = deriveKey( shortUserKey, paramStrlen( shortUserKey ), 
						"short user key", TRUE, FALSE );
	if( status ) 
		{
		status = deriveKey( medUserKey, paramStrlen( medUserKey ), 
							"medium user key", TRUE, FALSE );
		}
	if( status ) 
		{
		status = deriveKey( longUserKey, paramStrlen( longUserKey ), 
							"long user key", TRUE, FALSE );
		}
	if( !status ) 
		return( FALSE );

	/* Test the derivation process using a non-default hash algorithm */
	status = deriveKey( shortUserKey, paramStrlen( shortUserKey ), "", 
						FALSE, TRUE );
	if( !status ) 
		return( FALSE );
	status = deriveKey( shortUserKey, paramStrlen( shortUserKey ), "", 
						TRUE, TRUE );
	if( !status ) 
		return( FALSE );

	/* Test the derivation process using fixed test data: password =
	   "password", hash = HMAC_SHA1, salt = { 0x12 0x34 0x56 0x78 0x78 0x56 
	   0x34 0x12 }, iterations = 5, algorithm = 3DES */
#ifdef USE_3DES
	fputs( "Testing key derivation using fixed test vectors...\n", 
		   outputStream );
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 CRYPT_ALGO_3DES );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttribute( cryptContext, CRYPT_CTXINFO_MODE, CRYPT_MODE_ECB );
	cryptSetAttribute( cryptContext, CRYPT_CTXINFO_KEYING_ALGO, 
					   CRYPT_ALGO_HMAC_SHA1 );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEYING_SALT,
							 "\x12\x34\x56\x78\x78\x56\x34\x12", 8 );
	cryptSetAttribute( cryptContext, CRYPT_CTXINFO_KEYING_ITERATIONS, 5 );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEYING_VALUE,
							 TEXT( "password" ),
							 paramStrlen( TEXT( "password" ) ) );
	memset( buffer, 0, 8 );
	cryptEncrypt( cryptContext, buffer, 8 );
	cryptDestroyContext( cryptContext );
#if 0	/* Older single-DES value */
	if( memcmp( buffer, "\x9B\xBD\x78\xFC\x11\xA3\xA9\x08", 8 ) )
#else
	if( memcmp( buffer, "\x87\x91\x20\xAF\x19\x2F\xB6\x03", 8 ) )
#endif /* 0 */
		{
		fputs( "Derived key value doesn't match predefined test value.", 
			   outputStream );
		return( FALSE );
		}
#endif /* USE_3DES */
	fputs( "Key derivation succeeded.\n\n", outputStream );

	return( TRUE );
	}

/* Test the code to export/import an encrypted key via conventional
   encryption.

   To create the CMS PWRI test vectors, substitute the following code for the
   normal key load:

	const C_STR userKey = "password"; 
	const int userKeyLength = 8;

	cryptCreateContext( &sessionKeyContext1, CRYPT_UNUSED, CRYPT_ALGO_DES );
	cryptSetAttributeString( sessionKeyContext1, CRYPT_CTXINFO_KEY,
							 "\x8C\x63\x7D\x88\x72\x23\xA2\xF9", 8 );
	cryptCreateContext( &cryptContext, CRYPT_UNUSED, CRYPT_ALGO_DES );
	cryptSetAttribute( cryptContext, CRYPT_CTXINFO_KEYING_ITERATIONS, 5 );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEYING_SALT,
							 "\x12\x34\x56\x78\x78\x56\x34\x12", 8 );
	status = cryptSetAttributeString( cryptContext,
									  CRYPT_CTXINFO_KEYING_VALUE,
									  userKey, userKeyLength ); 

	To create the PKCS #5 v2.0 Amd.1 test vectors use:

	const C_STR userKey = "foo123"; 
	const int userKeyLength = 6;

	cryptCreateContext( &cryptContext, CRYPT_UNUSED, CRYPT_ALGO_3DES );
	cryptSetAttribute( cryptContext, CRYPT_CTXINFO_KEYING_ITERATIONS, 1000 );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEYING_SALT,
							 "\x12\x3E\xFF\x3C\x4A\x72\x12\x9C", 8 );
	status = cryptSetAttributeString( cryptContext,
									  CRYPT_CTXINFO_KEYING_VALUE,
									  userKey, userKeyLength ); */

typedef enum { AES_NONE, AES_128_128, AES_128_256, 
			   AES_256_128, AES_256_256 } AES_KEYSIZE_OPT;
			   /* First parameter = session key, second 
			      parameter = wrap key */

static int conventionalExportImport( const CRYPT_CONTEXT cryptContext,
									 const CRYPT_CONTEXT sessionKeyContext1,
									 const CRYPT_CONTEXT sessionKeyContext2,
									 const BOOLEAN useAltAlgo,
									 const AES_KEYSIZE_OPT aesKeysizeOpt )
	{
	CRYPT_OBJECT_INFO cryptObjectInfo;
	CRYPT_CONTEXT decryptContext;
	BYTE *buffer;
	const C_STR userKey = TEXT( "All n-entities must communicate with other n-entities via n-1 entiteeheehees" );
	const int userKeyLength = paramStrlen( userKey );
	int prfAlgo, altPrfAlgo, length, status;

	/* Check what the default PRF algorithm is so that we can set a 
	   different one if required */
	status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_KEYING_ALGO, 
								&prfAlgo );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptGetAttribute() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	altPrfAlgo = ( prfAlgo == CRYPT_ALGO_HMAC_SHA1 ) ? \
				 CRYPT_ALGO_HMAC_SHA2 : CRYPT_ALGO_HMAC_SHA1;

	/* Set the key for the exporting context */
	status = cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEYING_SALT,
									  "\x12\x34\x56\x78\x78\x56\x34\x12", 8 );
	if( cryptStatusOK( status ) && useAltAlgo )
		{
		status = cryptSetAttribute( cryptContext, CRYPT_CTXINFO_KEYING_ALGO,
									altPrfAlgo );
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttributeString( cryptContext,
										  CRYPT_CTXINFO_KEYING_VALUE,
										  userKey, userKeyLength );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptSetAttributeString() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Find out how big the exported key will be */
	status = cryptExportKey( NULL, 0, &length, cryptContext,
							 sessionKeyContext1 );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptExportKey() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	fprintf( outputStream, "cryptExportKey() reports exported key object "
			 "will be %d bytes long\n", length );
	if( ( buffer = malloc( length ) ) == NULL )
		return( FALSE );

	/* Export the session information */
	status = cryptExportKey( buffer, length, &length, cryptContext,
							 sessionKeyContext1 );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptExportKey() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}

	/* Query the encrypted key object */
	status = cryptQueryObject( buffer, length, &cryptObjectInfo );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptQueryObject() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}
	fprintf( outputStream, "cryptQueryObject() reports object type %d, "
			 "algorithm %d, mode %d.\n", cryptObjectInfo.objectType, 
			 cryptObjectInfo.cryptAlgo, cryptObjectInfo.cryptMode );
	fprintf( outputStream, "cryptQueryObject() reports key derivation info "
			 "as an %d-byte salt\n  processed with %d iterations of PRF "
			 "algorithm %d.\n", cryptObjectInfo.saltSize, 
			 cryptObjectInfo.iterations, cryptObjectInfo.hashAlgo );
	if( aesKeysizeOpt == AES_NONE )
		debugDump( "kek", buffer, length );
	else
		{
		const char *fileName = ( aesKeysizeOpt == AES_128_128 ) ? \
								 "kek_aes128_128" : \
							   ( aesKeysizeOpt == AES_128_256 ) ? \
								 "kek_aes128_256" : \
							   ( aesKeysizeOpt == AES_256_128 ) ? \
								 "kek_aes256_128" : "kek_aes256_256";
		debugDump( fileName, buffer, length );
		}
	if( useAltAlgo && cryptObjectInfo.hashAlgo != altPrfAlgo )
		{
		fprintf( outputStream, "Key wrap with PRF algorithm %d reported "
				 "algorithm as %d,\n" "line %d.\n", altPrfAlgo, 
				 cryptObjectInfo.hashAlgo, __LINE__ );
		free( buffer );
		return( FALSE );
		}

	/* Recreate the session key by importing the encrypted key */
	status = cryptCreateContext( &decryptContext, CRYPT_UNUSED,
								 cryptObjectInfo.cryptAlgo );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptCreateContext() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}
	cryptSetAttribute( decryptContext, CRYPT_CTXINFO_MODE,
					   cryptObjectInfo.cryptMode );
	status = cryptSetAttributeString( decryptContext,
									  CRYPT_CTXINFO_KEYING_SALT,
									  cryptObjectInfo.salt,
									  cryptObjectInfo.saltSize );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( decryptContext,
									CRYPT_CTXINFO_KEYING_ITERATIONS,
									cryptObjectInfo.iterations );
		}
	if( cryptStatusOK( status ) && \
		cryptObjectInfo.hashAlgo != CRYPT_ALGO_NONE )
		{
		status = cryptSetAttribute( decryptContext,
									CRYPT_CTXINFO_KEYING_ALGO,
									cryptObjectInfo.hashAlgo );
		}
	if( cryptStatusOK( status ) && \
		( aesKeysizeOpt == AES_128_256 || aesKeysizeOpt == AES_256_256 ) )
		{
		/* The CRYPT_OBJECT_INFO structure doesn't contain an entry for the
		   key size because it hadn't been needed previously and it can't be 
		   changed now without breaking the ABI, so we have to explicitly 
		   set the non-default key size via an externally-supplied parameter 
		   until there's a rev.of the minor version and we can modify the 
		   ABI */
		status = cryptSetAttribute( decryptContext, 
									CRYPT_CTXINFO_KEYSIZE, 32 );
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttributeString( decryptContext,
										  CRYPT_CTXINFO_KEYING_VALUE,
										  userKey, userKeyLength );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptSetAttributeString() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptImportKey( buffer, length, decryptContext,
							 sessionKeyContext2 );
	if( cryptStatusError( status ) )
		{
		cryptDestroyContext( decryptContext );
		fprintf( outputStream, "cryptImportKey() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}

	/* Make sure that the two keys match */
	if( !compareSessionKeys( sessionKeyContext1, sessionKeyContext2 ) )
		return( FALSE );

	/* If we're using variable-length keys, make sure that the lengths are 
	   right.  We only need to check one of the two contexts since the fact 
	   that the key sizes are the same has already been confirmed */
	if( aesKeysizeOpt != AES_NONE )
		{
		const int sessionKeySize = ( aesKeysizeOpt == AES_128_128 || \
									 aesKeysizeOpt == AES_128_256 ) ? \
									 16 : 32;
		const int wrapKeySize = ( aesKeysizeOpt == AES_128_128 || \
								  aesKeysizeOpt == AES_256_128 ) ? \
								  16 : 32;

		status = cryptGetAttribute( sessionKeyContext1, 
									CRYPT_CTXINFO_KEYSIZE, &length );
		if( cryptStatusError( status ) || length != sessionKeySize )
			{
			fprintf( outputStream, "AES session key size is incorrect, "
					 "line %d.\n", __LINE__ );
			return( FALSE );
			}
		status = cryptGetAttribute( decryptContext, 
									CRYPT_CTXINFO_KEYSIZE, &length );
		if( cryptStatusError( status ) || length != wrapKeySize )
			{
			fprintf( outputStream, "AES wrap key size is incorrect, "
					 "line %d.\n", __LINE__ );
			return( FALSE );
			}
		}

	/* Clean up */
	cryptDestroyContext( decryptContext );
	free( buffer );
	return( TRUE );
	}

static int testConv3DES( void )
	{
	CRYPT_CONTEXT cryptContext;
	CRYPT_CONTEXT sessionKeyContext1, sessionKeyContext2;
	int status;

	fputs( "Testing 3DES conventional key export/import...\n", 
		   outputStream );

	/* Create triple-DES contexts for the session key */
	status = cryptCreateContext( &sessionKeyContext1, CRYPT_UNUSED, 
								 CRYPT_ALGO_3DES );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( sessionKeyContext1, CRYPT_CTXINFO_MODE, 
									CRYPT_MODE_CFB );
		}
	if( cryptStatusOK( status ) )
		status = cryptGenerateKey( sessionKeyContext1 );
	if( cryptStatusOK( status ) )
		{
		status = cryptCreateContext( &sessionKeyContext2, CRYPT_UNUSED, 
									 CRYPT_ALGO_3DES );
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( sessionKeyContext2, CRYPT_CTXINFO_MODE, 
									CRYPT_MODE_CFB );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Session key context setup failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Create a CAST context to export the session key */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED,
								 selectCipher( CRYPT_ALGO_CAST ) );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Export key context setup failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Export the key.  We indicate the use of an alternative PRF algorithm 
	   because this is the now-deprecated SHA1, previously the default was
	   SHA1 and the alternative was SHA2 but since the default is now SHA2 
	   the original default has become the alternative */
	if( !conventionalExportImport( cryptContext, sessionKeyContext1,
								   sessionKeyContext2, TRUE, AES_NONE ) )
		return( FALSE );
	cryptDestroyContext( cryptContext );
	destroyContexts( CRYPT_UNUSED, sessionKeyContext1, sessionKeyContext2 );
	fputs( "Export/import of 3DES key via user-key-based conventional\n  "
		   "encryption succeeded.\n", outputStream );

	return( TRUE );
	}

static int testConvAES( const AES_KEYSIZE_OPT aesKeysizeOpt )
	{
	CRYPT_CONTEXT cryptContext;
	CRYPT_CONTEXT sessionKeyContext1, sessionKeyContext2;
	const char *aesKey1Name = ( aesKeysizeOpt == AES_128_128 || \
								aesKeysizeOpt == AES_128_256 ) ? \
							   "AES-128" : "AES-256";
	const char *aesKey2Name = ( aesKeysizeOpt == AES_128_128 || \
								aesKeysizeOpt == AES_256_128 ) ? \
							   "AES-128" : "AES-256";
	int status;

	fprintf( outputStream, "Testing %s conventional key export/import via "
			 "%s...\n", aesKey1Name, aesKey2Name );

	/* Create AES contexts for the session key and another AES context to
	   export the session key.  In addition to using AES we use a non-
	   default PRF algorithm */
	status = cryptCreateContext( &sessionKeyContext1, CRYPT_UNUSED, 
								 selectCipher( CRYPT_ALGO_AES ) );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( sessionKeyContext1, CRYPT_CTXINFO_MODE, 
									CRYPT_MODE_CFB );
		}
	if( cryptStatusOK( status ) && \
		( aesKeysizeOpt == AES_256_128 || aesKeysizeOpt == AES_256_256 ) )
		{
		status = cryptSetAttribute( sessionKeyContext1, 
									CRYPT_CTXINFO_KEYSIZE, 32 );
		}
	if( cryptStatusOK( status ) )
		status = cryptGenerateKey( sessionKeyContext1 );
	if( cryptStatusOK( status ) )
		{
		status = cryptCreateContext( &sessionKeyContext2, CRYPT_UNUSED, 
									 selectCipher( CRYPT_ALGO_AES ) );
		cryptSetAttribute( sessionKeyContext2, CRYPT_CTXINFO_MODE, 
							CRYPT_MODE_CFB );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Session key context setup failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 selectCipher( CRYPT_ALGO_AES ) );
	if( cryptStatusOK( status ) && \
		( aesKeysizeOpt == AES_128_256 || aesKeysizeOpt == AES_256_256 ) )
		{
		status = cryptSetAttribute( cryptContext, 
									CRYPT_CTXINFO_KEYSIZE, 32 );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Export key context setup failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Export the key */
	if( !conventionalExportImport( cryptContext, sessionKeyContext1,
								   sessionKeyContext2, FALSE, 
								   aesKeysizeOpt ) )
		{
		cryptDestroyContext( cryptContext );
		destroyContexts( CRYPT_UNUSED, sessionKeyContext1, sessionKeyContext2 );
		return( FALSE );
		}
	cryptDestroyContext( cryptContext );
	destroyContexts( CRYPT_UNUSED, sessionKeyContext1, sessionKeyContext2 );
	fprintf( outputStream, "Export/import of %s key via user-key-based %s "
			 "conventional encryption\n  succeeded.\n", aesKey1Name, 
			 aesKey2Name );

	return( TRUE );
	}

int testConventionalExportImport( void )
	{
#ifdef USE_INT_CMS
  #ifdef USE_3DES
	if( !testConv3DES() )
		return( FALSE );
  #endif /* USE_3DES */
	if( !testConvAES( AES_128_128 ) )
		return( FALSE );
	if( !testConvAES( AES_128_256 ) )
		return( FALSE );
	if( !testConvAES( AES_256_128 ) )
		return( FALSE );
	if( !testConvAES( AES_256_256 ) )
		return( FALSE );
	fprintf( outputStream, "\n" );
#else
	fputs( "Couldn't run conv. key export tests because CMS support has been "
		  "disabled,\n" "skipping tests...\n", outputStream );
#endif /* USE_INT_CMS */

	return( TRUE );
	}

int testMACExportImport( void )
	{
#ifdef USE_INT_CMS
	CRYPT_OBJECT_INFO cryptObjectInfo;
	CRYPT_CONTEXT cryptContext, decryptContext;
	CRYPT_CONTEXT macContext1, macContext2;
	BYTE mac1[ CRYPT_MAX_HASHSIZE ], mac2[ CRYPT_MAX_HASHSIZE ];
	C_STR userKey = TEXT( "This is a long user key for MAC testing" );
	BYTE *buffer;
	int userKeyLength = paramStrlen( userKey );
	int status, length1, length2;

	fputs( "Testing MAC key export/import...\n", outputStream );

	/* Create HMAC-SHA1 contexts for the MAC key */
	status = cryptCreateContext( &macContext1, CRYPT_UNUSED, 
								 CRYPT_ALGO_HMAC_SHA1 );
	if( cryptStatusOK( status ) )
		status = cryptGenerateKey( macContext1 );
	if( cryptStatusOK( status ) )
		{
		status = cryptCreateContext( &macContext2, CRYPT_UNUSED, 
									 CRYPT_ALGO_HMAC_SHA1 );
		}
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Create an encryption context to export the MAC key */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 DEFAULT_CRYPT_ALGO );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEYING_SALT,
							 "\x12\x34\x56\x78\x78\x56\x34\x12", 8 );
	status = cryptSetAttributeString( cryptContext,
									  CRYPT_CTXINFO_KEYING_VALUE,
									  userKey, userKeyLength );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Find out how big the exported key will be */
	status = cryptExportKey( NULL, 0, &length1, cryptContext, macContext1 );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptExportKey() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	fprintf( outputStream, "cryptExportKey() reports exported key object "
			 "will be %d bytes long\n", length1 );
	if( ( buffer = malloc( length1 ) ) == NULL )
		return( FALSE );

	/* Export the MAC information */
	status = cryptExportKey( buffer, length1, &length1, cryptContext,
							 macContext1 );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptExportKey() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}

	/* Query the encrypted key object */
	status = cryptQueryObject( buffer, length1, &cryptObjectInfo );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptQueryObject() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}
	fprintf( outputStream, "cryptQueryObject() reports object type %d, "
			 "algorithm %d, mode %d.\n", cryptObjectInfo.objectType, 
			 cryptObjectInfo.cryptAlgo, cryptObjectInfo.cryptMode );
	debugDump( "kek_mac", buffer, length1 );

	/* Recreate the MAC key by importing the encrypted key */
	status = cryptCreateContext( &decryptContext, CRYPT_UNUSED,
								 cryptObjectInfo.cryptAlgo );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptCreateContext() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}
	cryptSetAttribute( cryptContext, CRYPT_CTXINFO_MODE,
					   cryptObjectInfo.cryptMode );
	cryptSetAttributeString( decryptContext, CRYPT_CTXINFO_KEYING_SALT,
							 cryptObjectInfo.salt, cryptObjectInfo.saltSize );
	status = cryptSetAttributeString( decryptContext,
									  CRYPT_CTXINFO_KEYING_VALUE,
									  userKey, userKeyLength );
	if( cryptStatusOK( status ) )
		{
		status = cryptImportKey( buffer, length1, decryptContext,
								 macContext2 );
		}
	free( buffer );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptImportKey() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Make sure that the two MAC keys match */
	cryptEncrypt( macContext1, "1234", 4 );
	status = cryptEncrypt( macContext1, "", 0 );
	if( cryptStatusOK( status ) )
		{
		cryptEncrypt( macContext2, "1234", 4 );
		status = cryptEncrypt( macContext2, "", 0 );
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( macContext1,
										  CRYPT_CTXINFO_HASHVALUE,
										  mac1, &length1 );
		}
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( macContext2,
										  CRYPT_CTXINFO_HASHVALUE,
										  mac2, &length2 );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "MAC operation failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( ( length1 != length2 ) || memcmp( mac1, mac2, length1 ) || \
		!memcmp( mac1, "\x00\x00\x00\x00\x00\x00\x00\x00", 8 ) || \
		!memcmp( mac2, "\x00\x00\x00\x00\x00\x00\x00\x00", 8 ) )
		{
		fputs( "Data MAC'd with key1 != data MAC'd with key2.", 
			   outputStream );
		return( FALSE );
		}

	/* Clean up */
	destroyContexts( CRYPT_UNUSED, macContext1, macContext2 );
	destroyContexts( CRYPT_UNUSED, cryptContext, decryptContext );
	fprintf( outputStream, "Export/import of MAC key via user-key-based "
			 "triple DES conventional\n  encryption succeeded.\n\n" );
#else
	fputs( "Couldn't run MAC key export tests because CMS support has been "
		   "disabled,\n" "skipping tests...\n", outputStream );
#endif /* USE_INT_CMS */
	return( TRUE );
	}

/* Test the code to export/import an encrypted key and sign data.  We're not
   as picky with error-checking here since most of the functions have just
   executed successfully.  We check every algorithm type since there are
   different code paths for DLP and non-DLP PKCs */

int testKeyExportImport( void )
	{
#ifdef USE_INT_CMS
	if( !keyExportImport( "RSA", CRYPT_ALGO_RSA, CRYPT_UNUSED, CRYPT_UNUSED, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* RSA */
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ELGAMAL, NULL ) ) && \
		!keyExportImport( "Elgamal", CRYPT_ALGO_ELGAMAL, CRYPT_UNUSED, CRYPT_UNUSED, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Elgamal */
#else
	fputs( "Couldn't run PKC key export tests because CMS support has been "
		   "disabled,\n" "skipping tests...\n", outputStream );
#endif /* USE_INT_CMS */
#ifdef USE_PGP
	if( !keyExportImport( "RSA", CRYPT_ALGO_RSA, CRYPT_UNUSED, CRYPT_UNUSED, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* RSA, PGP format */
	return( keyExportImport( "Elgamal", CRYPT_ALGO_ELGAMAL, CRYPT_UNUSED, CRYPT_UNUSED, CRYPT_FORMAT_PGP ) );
							/* Elgamal, PGP format */
#else
	return( TRUE );
#endif /* USE_PGP */
	}

int testSignData( void )
	{
#ifdef USE_INT_CMS
	if( !signData( "RSA", CRYPT_ALGO_RSA, CRYPT_UNUSED, CRYPT_UNUSED, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* RSA */
	if( !signData( "RSA", CRYPT_ALGO_RSA, CRYPT_UNUSED, CRYPT_UNUSED, TRUE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* RSA, side-channel attack protection */
	if( !signData( "RSA with SHA2", CRYPT_ALGO_RSA, CRYPT_UNUSED, CRYPT_UNUSED, FALSE, TRUE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* RSA with SHA2 */
	if( !signData( "DSA", CRYPT_ALGO_DSA, CRYPT_UNUSED, CRYPT_UNUSED, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* DSA */
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ECDSA, NULL ) ) && \
		!signData( "ECDSA", CRYPT_ALGO_ECDSA, CRYPT_UNUSED, CRYPT_UNUSED, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* ECDSA */
#else
	fputs( "Couldn't run signing tests because CMS support has been "
		   "disabled,\n" "skipping tests...\n", outputStream );
#endif /* USE_INT_CMS */
#ifdef USE_PGP
	if( !signData( "RSA", CRYPT_ALGO_RSA, CRYPT_UNUSED, CRYPT_UNUSED, FALSE, FALSE, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* RSA, PGP format */
	return( signData( "DSA", CRYPT_ALGO_DSA, CRYPT_UNUSED, CRYPT_UNUSED, FALSE, FALSE, CRYPT_FORMAT_PGP ) );
							/* DSA, PGP format */
#else
	return( TRUE );
#endif /* USE_PGP */
	}

/* Test normal and asynchronous public-key generation */

static int keygen( const CRYPT_ALGO_TYPE cryptAlgo, const char *algoName )
	{
	CRYPT_CONTEXT cryptContext;
	BYTE buffer[ BUFFER_SIZE ];
	int length, status;

	fprintf( outputStream, "Testing %s key generation...\n", algoName );

	/* Create an encryption context and generate a (short) key into it.
	   Generating a minimal-length 512 bit key is faster than the default
	   1-2K bit keys */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, cryptAlgo );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_LABEL,
							 TEXT( "Private key" ),
							 paramStrlen( TEXT( "Private key" ) ) );
	status = cryptGenerateKey( cryptContext );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptGenerateKey() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Perform a test operation to check the new key */
	switch( cryptAlgo )
		{
		case CRYPT_ALGO_RSA:
		case CRYPT_ALGO_DSA:
		case CRYPT_ALGO_ECDSA:
			{
			CRYPT_CONTEXT hashContext;
			BYTE hashBuffer[] = "abcdefghijklmnopqrstuvwxyz";

			/* Create an SHA hash context and hash the test buffer */
			status = cryptCreateContext( &hashContext, CRYPT_UNUSED, 
										 CRYPT_ALGO_SHA1 );
			if( cryptStatusError( status ) )
				return( FALSE );
			cryptEncrypt( hashContext, hashBuffer, 26 );
			cryptEncrypt( hashContext, hashBuffer, 0 );

			/* Sign the hashed data and check the signature */
			status = cryptCreateSignature( buffer, BUFFER_SIZE, &length,
										   cryptContext, hashContext );
			if( cryptStatusOK( status ) )
				{
				status = cryptCheckSignature( buffer, length, cryptContext,
											  hashContext );
				}

			/* Clean up */
			cryptDestroyContext( hashContext );
			cryptDestroyContext( cryptContext );
			if( cryptStatusError( status ) )
				{
				fprintf( outputStream, "Sign/signature check with generated "
						 "key failed with error code %d, line %d.\n", status, 
						 __LINE__ );
				return( FALSE );
				}
			break;
			}

		case CRYPT_ALGO_ELGAMAL:
			{
			CRYPT_CONTEXT sessionKeyContext1, sessionKeyContext2;

			/* Test the key exchange */
			status = cryptCreateContext( &sessionKeyContext1, CRYPT_UNUSED,
										 DEFAULT_CRYPT_ALGO );
			if( cryptStatusError( status ) )
				return( FALSE );
			status = cryptCreateContext( &sessionKeyContext2, 
										 CRYPT_UNUSED, DEFAULT_CRYPT_ALGO );
			if( cryptStatusError( status ) )
				{
				cryptDestroyContext( sessionKeyContext1 );
				return( FALSE );
				}
			if( cryptStatusOK( status ) )
				status = cryptGenerateKey( sessionKeyContext1 );
			if( cryptStatusError( status ) )
				return( FALSE );
			status = cryptExportKey( buffer, BUFFER_SIZE, &length, 
									 cryptContext, sessionKeyContext1 );
			if( cryptStatusOK( status ) )
				{
				status = cryptImportKey( buffer, length, cryptContext,
										 sessionKeyContext2 );
				}
			cryptDestroyContext( cryptContext );
			if( cryptStatusError( status ) )
				{
				destroyContexts( CRYPT_UNUSED, sessionKeyContext1,
								 sessionKeyContext2 );
				fprintf( outputStream, "Key exchange with generated key "
						 "failed with error code %d, line %d.\n", status, 
						 __LINE__ );
				return( FALSE );
				}

			/* Make sure that the two keys match */
			if( !compareSessionKeys( sessionKeyContext1, 
									 sessionKeyContext2 ) )
				return( FALSE );

			/* Clean up */
			destroyContexts( CRYPT_UNUSED, sessionKeyContext1,
							 sessionKeyContext2 );
			break;
			}

		case CRYPT_ALGO_DH:
		case CRYPT_ALGO_ECDH:
			{
KLUDGE_WARN( "DH/ECDH test because of absence of DH/ECDH key exchange mechanism" );
cryptDestroyContext( cryptContext );
return( TRUE );

#if 0		/* Get rid if unreachable-code warnings */
			CRYPT_CONTEXT dhContext;
			CRYPT_CONTEXT sessionKeyContext1, sessionKeyContext2;

			/* Test the key exchange */
			status = cryptCreateContext( &sessionKeyContext1, CRYPT_UNUSED,
										 DEFAULT_CRYPT_ALGO );
			if( cryptStatusOK( status ) )
				{
				status = cryptCreateContext( &sessionKeyContext2, 
											 CRYPT_UNUSED, DEFAULT_CRYPT_ALGO );
				}
			if( cryptStatusOK( status ) )
				{
				status = cryptCreateContext( &dhContext, CRYPT_UNUSED, 
											 CRYPT_ALGO_DH );
				}
			if( cryptStatusError( status ) )
				return( FALSE );
			status = cryptExportKey( buffer, BUFFER_SIZE, &length, 
									 cryptContext, sessionKeyContext1 );
			if( cryptStatusOK( status ) )
				{
				status = cryptImportKey( buffer, length, dhContext,
										 sessionKeyContext2 );
				}
			if( cryptStatusOK( status ) )
				{
				status = cryptExportKey( buffer, BUFFER_SIZE, &length, 
										 dhContext, sessionKeyContext2 );
				}
			if( cryptStatusOK( status ) )
				{
				status = cryptImportKey( buffer, length, cryptContext,
										 sessionKeyContext1 );
				}
			cryptDestroyContext( cryptContext );
			cryptDestroyContext( dhContext );
			if( cryptStatusError( status ) )
				{
				destroyContexts( CRYPT_UNUSED, sessionKeyContext1,
								 sessionKeyContext2 );
				fprintf( outputStream, "Key exchange with generated key "
						 "failed with error code %d, line %d.\n", status, 
						 __LINE__ );
				return( FALSE );
				}

			/* Make sure that the two keys match */
			if( !compareSessionKeys( sessionKeyContext1, 
									 sessionKeyContext2 ) )
				return( FALSE );

			/* Clean up */
			destroyContexts( CRYPT_UNUSED, sessionKeyContext1,
							 sessionKeyContext2 );
			break;
#endif /* 0 */
			}

		default:
			fprintf( outputStream, "Unexpected encryption algorithm %d "
					 "found, line %d.\n", cryptAlgo, __LINE__ );
			return( FALSE );
		}

	fprintf( outputStream, "%s key generation succeeded.\n", algoName );
	return( TRUE );
	}

int testKeygen( void )
	{
	if( !keygen( CRYPT_ALGO_RSA, "RSA" ) )
		return( FALSE );
	if( !keygen( CRYPT_ALGO_DSA, "DSA" ) )
		return( FALSE );
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ELGAMAL, NULL ) ) && \
		!keygen( CRYPT_ALGO_ELGAMAL, "Elgamal" ) )
		return( FALSE );
	if( !keygen( CRYPT_ALGO_DH, "DH" ) )
		return( FALSE );
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ECDSA, NULL ) ) && \
		!keygen( CRYPT_ALGO_ECDSA, "ECDSA" ) )
		return( FALSE );
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ECDH, NULL ) ) && \
		!keygen( CRYPT_ALGO_ECDH, "ECDH" ) )
		return( FALSE );
	fprintf( outputStream, "\n" );
	return( TRUE );
	}

/* Test handling of injected faults */

#if defined( CONFIG_FAULTS ) && !defined( NDEBUG )

typedef enum { TEST_SIGN_RSA, TEST_SIGN_ECDSA, TEST_KEYEX_CONV, 
			   TEST_KEYEX_PKC, TEST_PRF } TEST_TYPE;

static int testDebugCheck( const TEST_TYPE testType, const FAULT_TYPE testFaultType )
	{
	int status;

	cryptSetFaultType( testFaultType );
	switch( testType )
		{
		case TEST_SIGN_RSA:
			status = signData( "RSA", CRYPT_ALGO_RSA, CRYPT_UNUSED, 
							   CRYPT_UNUSED, FALSE, FALSE, 
							   CRYPT_FORMAT_CRYPTLIB );
			break;
		
		case TEST_SIGN_ECDSA:
			status = signData( "ECDSA", CRYPT_ALGO_ECDSA, CRYPT_UNUSED, 
							   CRYPT_UNUSED, FALSE, FALSE, 
							   CRYPT_FORMAT_CRYPTLIB );
			break;
		
		case TEST_KEYEX_CONV:
			status = testConvAES( AES_128_128 );
			break;
		
		case TEST_KEYEX_PKC:
			status = keyExportImport( "RSA", CRYPT_ALGO_RSA, CRYPT_UNUSED, 
									  CRYPT_UNUSED, CRYPT_FORMAT_CRYPTLIB );
			break;
		
		case TEST_PRF:
			status = testConvAES( AES_128_128 );
			break;

		default:
			assert( 0 );
		}
	if( status )
		{
		cryptSetFaultType( FAULT_NONE );
		fputs( "  (This test should have led to an enveloping failure but "
			   "didn't, test has\n   failed).\n", outputStream );
		return( FALSE );
		}

	/* These tests are supposed to fail, so if this happens then the overall 
	   test has succeeded */
	fputs( "  (This test checks error handling, so the failure response is "
		   "correct).\n", outputStream );
	return( TRUE );
	}
#endif /* CONFIG_FAULTS && Debug */

int testMidLevelDebugCheck( void )
	{
#if defined( CONFIG_FAULTS ) && !defined( NDEBUG )
	cryptSetFaultType( FAULT_NONE );
	if( !testDebugCheck( TEST_SIGN_RSA, FAULT_MECH_CORRUPT_HASH ) )
		return( FALSE );
	if( !testDebugCheck( TEST_SIGN_RSA, FAULT_MECH_CORRUPT_SIG ) )
		return( FALSE );
	if( !testDebugCheck( TEST_SIGN_ECDSA, FAULT_MECH_CORRUPT_HASH ) )
		return( FALSE );
	if( !testDebugCheck( TEST_SIGN_ECDSA, FAULT_MECH_CORRUPT_SIG ) )
		return( FALSE );
	if( !testDebugCheck( TEST_KEYEX_CONV, FAULT_MECH_CORRUPT_KEY ) )
		return( FALSE );
	if( !testDebugCheck( TEST_KEYEX_PKC, FAULT_MECH_CORRUPT_KEY ) )
		return( FALSE );
	if( !testDebugCheck( TEST_PRF, FAULT_MECH_CORRUPT_SALT ) )
		return( FALSE );
	if( !testDebugCheck( TEST_PRF, FAULT_MECH_CORRUPT_ITERATIONS ) )
		return( FALSE );
	if( !testDebugCheck( TEST_PRF, FAULT_MECH_CORRUPT_PRFALGO ) )
		return( FALSE );
	cryptSetFaultType( FAULT_NONE );
#endif /* CONFIG_FAULTS && Debug */
	return( TRUE );
	}
#endif /* TEST_MIDLEVEL */

/****************************************************************************
*																			*
*							Random Routines Test							*
*																			*
****************************************************************************/

#ifdef TEST_RANDOM

/* Test the randomness gathering routines */

int testRandomRoutines( void )
	{
	CRYPT_CONTEXT cryptContext;
	int status;

	fputs( "Testing randomness routines.  This may take a few seconds...\n",
		   outputStream );

	/* Create an encryption context to generate a key into */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 DEFAULT_CRYPT_ALGO );
	if( cryptStatusError( status ) )
		return( FALSE );
	status = cryptGenerateKey( cryptContext );
	cryptDestroyContext( cryptContext );

	/* Check whether we got enough randomness */
	if( status == CRYPT_ERROR_RANDOM )
		{
		fputs( "The randomness-gathering routines can't acquire enough random information to", 
			   outputStream );
		fputs( "allow key generation and public-key encryption to function.  You will need to", 
			   outputStream );
		fputs( "change the randomness-polling code or reconfigure your system to allow the", 
			   outputStream );
		fputs( "randomness-gathering routines to function.  The code to change can be found", 
			   outputStream );
		fputs( "in random/<osname>.c\n", outputStream );
		return( FALSE );
		}

	fputs( "Randomness-gathering self-test succeeded.\n\n", outputStream );
	return( TRUE );
	}
#endif /* TEST_RANDOM */

/****************************************************************************
*																			*
*							High-level Routines Test						*
*																			*
****************************************************************************/

#ifdef TEST_HIGHLEVEL

/* Test the code to export/import a CMS key */

int testKeyExportImportCMS( void )
	{
	CRYPT_OBJECT_INFO cryptObjectInfo;
	CRYPT_KEYSET cryptKeyset;
	CRYPT_CONTEXT cryptContext;
	CRYPT_CONTEXT sessionKeyContext1, sessionKeyContext2;
	BYTE *buffer;
	int status, length;

	fputs( "Testing CMS public-key export/import...", outputStream );

	/* Get a private key with a certificate chain attached */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  USER_PRIVKEY_FILE, CRYPT_KEYOPT_READONLY );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetPrivateKey( cryptKeyset, &cryptContext,
									 CRYPT_KEYID_NAME, USER_PRIVKEY_LABEL,
									 TEST_PRIVKEY_PASSWORD );
		cryptKeysetClose( cryptKeyset );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Couldn't read private key, status %d, "
				 "line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Create encryption contexts for the exported and imported session 
	   keys */
	status = cryptCreateContext( &sessionKeyContext1, CRYPT_UNUSED, 
								 DEFAULT_CRYPT_ALGO );
	if( cryptStatusOK( status ) )
		status = cryptGenerateKey( sessionKeyContext1 );
	if( cryptStatusOK( status ) )
		{
		status = cryptCreateContext( &sessionKeyContext2, CRYPT_UNUSED, 
									 DEFAULT_CRYPT_ALGO );
		}
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Find out how big the exported key will be */
	status = cryptExportKeyEx( NULL, 0, &length, CRYPT_FORMAT_SMIME,
							   cryptContext, sessionKeyContext1 );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptExportKeyEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	fprintf( outputStream, "cryptExportKeyEx() reports CMS exported key "
			 "will be %d bytes long\n", length );
	if( ( buffer = malloc( length ) ) == NULL )
		return( FALSE );

	/* Export the key */
	status = cryptExportKeyEx( buffer, length, &length, CRYPT_FORMAT_SMIME,
							   cryptContext, sessionKeyContext1 );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptExportKeyEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}

	/* Query the encrypted key object */
	status = cryptQueryObject( buffer, length, &cryptObjectInfo );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptQueryObject() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}
	fprintf( outputStream, "cryptQueryObject() reports object type %d, "
			 "algorithm %d.\n", cryptObjectInfo.objectType, 
			 cryptObjectInfo.cryptAlgo );
	memset( &cryptObjectInfo, 0, sizeof( CRYPT_OBJECT_INFO ) );
	debugDump( "cms_ri", buffer, length );

	/* Import the encrypted key and load it into the session key context */
	status = cryptImportKey( buffer, length, cryptContext,
							 sessionKeyContext2 );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptImportKey() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}

	/* Make sure that the two keys match */
	if( !compareSessionKeys( sessionKeyContext1, sessionKeyContext2 ) )
		return( FALSE );

	/* Clean up */
	destroyContexts( CRYPT_UNUSED, sessionKeyContext1, sessionKeyContext2 );
	cryptDestroyContext( cryptContext );
	fputs( "Export/import of CMS session key succeeded.\n", outputStream );
	free( buffer );
	return( TRUE );
	}

/* Test the code to create an CMS signature */

static const CERT_DATA cmsAttributeData[] = {
	/* We have to be careful when setting CMS attributes because most are 
	   never used by anything so they're only available of use of obscure 
	   attributes is enabled */
#ifdef USE_CMSATTR_OBSCURE
	/* Content type */
	{ CRYPT_CERTINFO_CMS_CONTENTTYPE, IS_NUMERIC, CRYPT_CONTENT_SPCINDIRECTDATACONTEXT },

	/* Odds and ends.  We can't (portably) set the opusInfo name since it's
	   a Unicode string so we only add this one under Windows */
  #ifdef __WINDOWS__
	{ CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME, IS_WCSTRING, 0, L"Program v3.0 SP2" },
  #endif /* __WINDOWS__ */
	{ CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL, IS_STRING, 0, TEXT( "http://bugs-r-us.com" ) },
	{ CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING, IS_NUMERIC, CRYPT_UNUSED },
#else
	/* Content type */
	{ CRYPT_CERTINFO_CMS_CONTENTTYPE, IS_NUMERIC, CRYPT_CONTENT_ENCRYPTEDDATA },
#endif /* USE_CMSATTR_OBSCURE */

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

static int signDataCMS( const char *description,
						const CRYPT_CERTIFICATE signingAttributes,
						const BOOLEAN isCustomAttributes )
	{
	CRYPT_KEYSET cryptKeyset;
	CRYPT_CERTIFICATE cmsAttributes = signingAttributes;
	CRYPT_CONTEXT signContext, hashContext;
	BYTE *buffer, hashBuffer[] = "abcdefghijklmnopqrstuvwxyz";
	int status, length;

	fprintf( outputStream, "Testing %s...\n", description );

	/* Create an SHA hash context and hash the test buffer */
	status = cryptCreateContext( &hashContext, CRYPT_UNUSED, 
								 CRYPT_ALGO_SHA1 );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptEncrypt( hashContext, hashBuffer, 26 );
	cryptEncrypt( hashContext, hashBuffer, 0 );

	/* Get a private key with a certificate chain attached */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  USER_PRIVKEY_FILE, CRYPT_KEYOPT_READONLY );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetPrivateKey( cryptKeyset, &signContext,
									 CRYPT_KEYID_NAME, USER_PRIVKEY_LABEL,
									 TEST_PRIVKEY_PASSWORD );
		cryptKeysetClose( cryptKeyset );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Couldn't read private key, status %d, "
				 "line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Find out how big the signature will be */
	status = cryptCreateSignatureEx( NULL, 0, &length, CRYPT_FORMAT_SMIME,
									 signContext, hashContext, 
									 cmsAttributes );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptCreateSignatureEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	fprintf( outputStream, "cryptCreateSignatureEx() reports CMS signature "
			 "will be %d bytes long\n", length );
	if( ( buffer = malloc( length ) ) == NULL )
		return( FALSE );

	/* Sign the hashed data */
	status = cryptCreateSignatureEx( buffer, length, &length,
									 CRYPT_FORMAT_SMIME, signContext,
									 hashContext, cmsAttributes );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptCreateSignatureEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}
	debugDump( ( signingAttributes == CRYPT_USE_DEFAULT ) ? \
					"cms_sigd" : \
			   ( isCustomAttributes ) ? "cms_sigc" : "cms_sig", 
			   buffer, length );

	/* Check the signature on the hash */
	status = cryptCheckSignatureEx( buffer, length, signContext, hashContext,
			( cmsAttributes == CRYPT_USE_DEFAULT ) ? NULL : &cmsAttributes );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptCheckSignatureEx() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		free( buffer );
		return( FALSE );
		}

	/* Display the signing attributes */
	if( cmsAttributes != CRYPT_USE_DEFAULT )
		printCertInfo( cmsAttributes );

	/* Clean up */
	cryptDestroyContext( hashContext );
	cryptDestroyContext( signContext );
	cryptDestroyCert( cmsAttributes );
	fprintf( outputStream, "Generation and checking of %s succeeded.\n\n", 
			 description );
	free( buffer );
	return( TRUE );
	}

int testSignDataCMS( void )
	{
	CRYPT_CERTIFICATE cmsAttributes;
    const BYTE *extensionData = ( BYTE * ) "\x0C\x04Test";
	int value, status;

	/* First test the basic CMS signature with default attributes (content
	   type, signing time, and message digest) */
	if( !signDataCMS( "CMS signature", CRYPT_USE_DEFAULT, FALSE ) )
		return( FALSE );

	/* Create some CMS attributes and sign the data with the user-defined
	   attributes */
	status = cryptCreateCert( &cmsAttributes, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CMS_ATTRIBUTES );
	if( cryptStatusError( status ) || \
		!addCertFields( cmsAttributes, cmsAttributeData, __LINE__ ) )
		return( FALSE );
	status = signDataCMS( "complex CMS signature", cmsAttributes, FALSE );
	cryptDestroyCert( cmsAttributes );
	if( !status )
		return( status );

	/* Create some custom CMS attributes and sign those too.  In order to do
	   this we have to set the CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES
	   configuration option */
	( void ) cryptGetAttribute( CRYPT_UNUSED, 
								CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES, 
								&value );
	status = cryptCreateCert( &cmsAttributes, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_CMS_ATTRIBUTES );
	if( cryptStatusError( status ) )
		return( FALSE );
	status = cryptAddCertExtension( cmsAttributes, "1.2.3.4.5", 
									CRYPT_UNUSED, extensionData, 6 );
	if( cryptStatusOK( status ) )
		{
		status = cryptSetAttribute( CRYPT_UNUSED,
									CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES, 
									TRUE );
		}
	if( cryptStatusOK( status ) )
		{
		status = signDataCMS( "CMS signature with custom attributes", 
							  cmsAttributes, TRUE );
		}
	cryptSetAttribute( CRYPT_UNUSED, 
					   CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES, value );
	cryptDestroyCert( cmsAttributes );
	if( !status )
		return( status );
	return( status );
	}

#endif /* TEST_HIGHLEVEL */
