/****************************************************************************
*																			*
*						cryptlib Low-level Test Routines					*
*						Copyright Peter Gutmann 1995-2004					*
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

/* The size of the test buffers */

#define TESTBUFFER_SIZE		256

/* Since the DH/ECDH operations aren't visible externally, we have to use 
   the kernel API to perform the test.  To get the necessary definitions 
   and prototypes, we have to use crypt.h, however since we've already 
   included cryptlib.h the built-in guards preclude us from pulling it in 
   again with the internal-only values defined, so we have to explicitly 
   define things like attribute values that normally aren't visible 
   externally */

#ifdef TEST_DH
  #undef __WINDOWS__
  #undef __WIN16__
  #undef __WIN32__
  #undef BYTE
  #include "crypt.h"
#endif /* TEST_DH */

#if defined( TEST_LOWLEVEL ) || defined( TEST_KEYSET )	/* Needed for PGP keysets */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Check for an algorithm/mode */

static BOOLEAN checkLowlevelInfo( const CRYPT_DEVICE cryptDevice,
								  const CRYPT_ALGO_TYPE cryptAlgo )
	{
	CRYPT_QUERY_INFO cryptQueryInfo;
	const BOOLEAN isDevice = ( cryptDevice != CRYPT_UNUSED ) ? TRUE : FALSE;
	int status;

	if( isDevice )
		status = cryptDeviceQueryCapability( cryptDevice, cryptAlgo,
											 &cryptQueryInfo );
	else
		status = cryptQueryCapability( cryptAlgo, &cryptQueryInfo );
	if( cryptStatusError( status ) )
		{
		printf( "crypt%sQueryCapability() reports algorithm %d is not "
				"available, status = %d.\n", isDevice ? "Device" : "",
				cryptAlgo, status );
		return( FALSE );
		}
#ifdef UNICODE_STRINGS
	printf( "cryptQueryCapability() reports availability of %S algorithm "
			"with\n  block size %d bits", cryptQueryInfo.algoName,
			cryptQueryInfo.blockSize << 3 );
#else
	printf( "cryptQueryCapability() reports availability of %s algorithm "
			"with\n  block size %d bits", cryptQueryInfo.algoName,
			cryptQueryInfo.blockSize << 3 );
#endif /* UNICODE_STRINGS */
	if( cryptAlgo < CRYPT_ALGO_FIRST_HASH || cryptAlgo > CRYPT_ALGO_LAST_HASH )
		{
		printf( ", keysize %d-%d bits (recommended = %d bits)",
				cryptQueryInfo.minKeySize << 3,
				cryptQueryInfo.maxKeySize << 3, cryptQueryInfo.keySize << 3 );
		}
	puts( "." );

	return( TRUE );
	}

/* Set a pair of encrypt/decrypt buffers to a known state, and make sure
   that they're still in that known state */

static void initTestBuffers( BYTE *buffer1, BYTE *buffer2, const int length )
	{
#if defined( __MVS__ ) || defined( __VMCMS__ )
  #pragma convlit( resume )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 819 )
#endif /* IBM medium iron */
	/* Set the buffers to a known state */
	memset( buffer1, '*', length );
	memcpy( buffer1, "12345678", 8 );		/* For endianness check */
	if( buffer2 != NULL )
		memcpy( buffer2, buffer1, length );
#if defined( __MVS__ ) || defined( __VMCMS__ )
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */
	}

static BOOLEAN checkTestBuffers( const BYTE *buffer1, const BYTE *buffer2 )
	{
	/* Make sure that everything went OK */
	if( !memcmp( buffer1, buffer2, TESTBUFFER_SIZE ) )
		return( TRUE );
	puts( "Error: Decrypted data != original plaintext." );

	/* Try and guess at block chaining problems */
	if( !memcmp( buffer1, "12345678****", 12 ) )
		{
		puts( "\t\bIt looks like there's a problem with block chaining." );
		return( FALSE );
		}

	/* Try and guess at endianness problems - we want "1234" */
	if( !memcmp( buffer1, "4321", 4 ) )
		{
		puts( "\t\bIt looks like the 32-bit word endianness is reversed." );
		return( FALSE );
		}
	if( !memcmp( buffer1, "2143", 4 ) )
		{
		puts( "\t\bIt looks like the 16-bit word endianness is reversed." );
		return( FALSE );
		}
	if( buffer1[ 0 ] >= '1' && buffer1[ 0 ] <= '9' )
		{
		puts( "\t\bIt looks like there's some sort of endianness problem "
			  "which is\n\t more complex than just a reversal." );
		return( FALSE );
		}
	puts( "\t\bIt's probably more than just an endianness problem." );
	return( FALSE );
	}

/* Load the encryption contexts */

static BOOLEAN loadContexts( CRYPT_CONTEXT *cryptContext, CRYPT_CONTEXT *decryptContext,
							 const CRYPT_DEVICE cryptDevice,
							 const CRYPT_ALGO_TYPE cryptAlgo,
							 const CRYPT_MODE_TYPE cryptMode,
							 const BYTE *key, const int length )
	{
	const BOOLEAN isDevice = ( cryptDevice != CRYPT_UNUSED ) ? TRUE : FALSE;
	const BOOLEAN hasKey = ( cryptAlgo >= CRYPT_ALGO_FIRST_CONVENTIONAL && \
							 cryptAlgo <= CRYPT_ALGO_LAST_CONVENTIONAL ) || \
						   ( cryptAlgo >= CRYPT_ALGO_FIRST_MAC && \
							 cryptAlgo <= CRYPT_ALGO_LAST_MAC );
	BOOLEAN adjustKey = FALSE;
	int status;

	/* Create the encryption context */
	if( isDevice )
		status = cryptDeviceCreateContext( cryptDevice, cryptContext,
										   cryptAlgo );
	else
		status = cryptCreateContext( cryptContext, CRYPT_UNUSED, cryptAlgo );
	if( cryptStatusError( status ) )
		{
		printf( "crypt%sCreateContext() failed with error code %d, "
				"line %d.\n", isDevice ? "Device" : "", status, __LINE__ );
		return( FALSE );
		}
	if( cryptAlgo <= CRYPT_ALGO_LAST_CONVENTIONAL )
		{
		status = cryptSetAttribute( *cryptContext, CRYPT_CTXINFO_MODE,
									cryptMode );
		if( cryptStatusError( status ) )
			{
			cryptDestroyContext( *cryptContext );
			if( status == CRYPT_ERROR_NOTAVAIL )
				{
				/* This mode isn't available, return a special-case value to
				   tell the calling code to continue */
				return( status );
				}
			printf( "Encryption mode %d selection failed with status %d, "
					"line %d.\n", cryptMode, status, __LINE__ );
			return( FALSE );
			}
		}
	if( hasKey )
		{
		status = cryptSetAttributeString( *cryptContext, CRYPT_CTXINFO_KEY,
										  key, length );
		if( length > 16 && status == CRYPT_ERROR_PARAM4 )
			{
			status = cryptSetAttributeString( *cryptContext, CRYPT_CTXINFO_KEY,
											  key, 16 );
			if( cryptStatusOK( status ) )
				{
				puts( "  Load of full-length key failed, using shorter 128-"
					  "bit key." );
				adjustKey = TRUE;
				}
			}
		if( cryptStatusError( status ) )
			{
			printf( "Encryption key load failed with error code %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}
	if( decryptContext == NULL )
		return( TRUE );

	/* Create the decryption context */
	if( cryptDevice == CRYPT_UNUSED )
		status = cryptCreateContext( decryptContext, CRYPT_UNUSED, cryptAlgo );
	else
		status = cryptDeviceCreateContext( cryptDevice, decryptContext,
										   cryptAlgo );
	if( cryptStatusError( status ) )
		{
		printf( "crypt%sCreateContext() failed with error code %d, "
				"line %d.\n", ( cryptDevice != CRYPT_UNUSED ) ? \
							  "Device" : "", status, __LINE__ );
		return( FALSE );
		}
	if( cryptAlgo <= CRYPT_ALGO_LAST_CONVENTIONAL )
		{
		status = cryptSetAttribute( *decryptContext, CRYPT_CTXINFO_MODE,
									cryptMode );
		if( cryptStatusError( status ) )
			{
			cryptDestroyContext( *cryptContext );
			if( status == CRYPT_ERROR_NOTAVAIL )
				{
				/* This mode isn't available, return a special-case value to
				   tell the calling code to continue */
				return( status );
				}
			printf( "Encryption mode %d selection failed with status %d, "
					"line %d.\n", cryptMode, status, __LINE__ );
			return( FALSE );
			}
		}
	if( hasKey )
		{
		status = cryptSetAttributeString( *decryptContext, CRYPT_CTXINFO_KEY,
										  key, adjustKey ? 16 : length );
		if( cryptStatusError( status ) )
			{
			printf( "Decryption key load failed with error code %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}

	return( TRUE );
	}

/* Perform a test en/decryption */

int testCrypt( CRYPT_CONTEXT cryptContext, CRYPT_CONTEXT decryptContext,
			   BYTE *buffer, const BOOLEAN isDevice,
			   const BOOLEAN noWarnFail )
	{
	BYTE iv[ CRYPT_MAX_IVSIZE ];
	BYTE localBuffer[ TESTBUFFER_SIZE ];
	int cryptAlgo, cryptMode, status;

	/* If the user hasn't supplied a test buffer, use our own one */
	if( buffer == NULL )
		{
		buffer = localBuffer;
		initTestBuffers( buffer, NULL, TESTBUFFER_SIZE );
		}

	/* Find out about the algorithm we're using */
	cryptGetAttribute( cryptContext, CRYPT_CTXINFO_ALGO, &cryptAlgo );
	cryptGetAttribute( cryptContext, CRYPT_CTXINFO_MODE, &cryptMode );
	if( cryptAlgo <= CRYPT_ALGO_LAST_CONVENTIONAL && \
		( cryptMode == CRYPT_MODE_CFB || cryptMode == CRYPT_MODE_OFB ) )
		{
		/* Encrypt the buffer in two odd-sized chunks */
		status = cryptEncrypt( cryptContext, buffer, 79 );
		if( cryptStatusOK( status ) )
			status = cryptEncrypt( cryptContext, buffer + 79,
								   TESTBUFFER_SIZE - 79 );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't encrypt data, status = %d, line %d.\n", 
					status, __LINE__ );
			return( status );
			}

		/* Copy the IV from the encryption to the decryption context if
		   necessary */
		if( cryptAlgo != CRYPT_ALGO_RC4 )
			{
			int ivLength;

			status = cryptGetAttributeString( cryptContext, CRYPT_CTXINFO_IV,
											  iv, &ivLength );
			if( cryptStatusError( status ) )
				{
				printf( "Couldn't retrieve IV after encryption, status = %d, "
						"line %d.\n", status, __LINE__ );
				return( status );
				}
			status = cryptSetAttributeString( decryptContext, CRYPT_CTXINFO_IV,
											  iv, ivLength );
			if( cryptStatusError( status ) )
				{
				printf( "Couldn't load IV for decryption, status = %d, "
						"line %d.\n", status, __LINE__ );
				return( status );
				}
			}

		/* Decrypt the buffer in different odd-size chunks */
		status = cryptDecrypt( decryptContext, buffer, 125 );
		if( cryptStatusOK( status ) )
			status = cryptDecrypt( decryptContext, buffer + 125,
								   TESTBUFFER_SIZE - 125 );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't decrypt data, status = %d, line %d.\n", 
					status, __LINE__ );
			return( status );
			}

		return( CRYPT_OK );
		}
	if( cryptAlgo <= CRYPT_ALGO_LAST_CONVENTIONAL && \
		( cryptMode == CRYPT_MODE_ECB || cryptMode == CRYPT_MODE_CBC || \
		  cryptMode == CRYPT_MODE_GCM ) )
		{
		/* Encrypt the buffer in two odd-sized chunks */
		status = cryptEncrypt( cryptContext, buffer, 80 );
		if( cryptStatusOK( status ) )
			status = cryptEncrypt( cryptContext, buffer + 80,
								   TESTBUFFER_SIZE - 80 );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't encrypt data, status = %d, line %d.\n", 
					status, __LINE__ );
			return( status );
			}

		/* Copy the IV from the encryption to the decryption context if
		   necessary */
		if( cryptMode != CRYPT_MODE_ECB )
			{
			int ivLength;

			status = cryptGetAttributeString( cryptContext, CRYPT_CTXINFO_IV,
											  iv, &ivLength );
			if( cryptStatusError( status ) )
				printf( "Couldn't retrieve IV after encryption, status = %d, "
						"line %d.\n", status, __LINE__ );
			status = cryptSetAttributeString( decryptContext, CRYPT_CTXINFO_IV,
											  iv, ivLength );
			if( cryptStatusError( status ) )
				printf( "Couldn't load IV for decryption, status = %d, "
						"line %d.\n", status, __LINE__ );
			}

		/* Decrypt the buffer in different odd-size chunks */
		status = cryptDecrypt( decryptContext, buffer, 128 );
		if( cryptStatusOK( status ) )
			status = cryptDecrypt( decryptContext, buffer + 128,
								   TESTBUFFER_SIZE - 128 );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't decrypt data, status = %d, line %d.\n", 
					status, __LINE__ );
			return( status );
			}

		return( CRYPT_OK );
		}
#ifdef TEST_DH
	if( cryptAlgo == CRYPT_ALGO_DH || cryptAlgo == CRYPT_ALGO_ECDH )
		{
		KEYAGREE_PARAMS keyAgreeParams;
		int status;

		/* Perform the DH/ECDH key agreement */
		memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
#if 0
		status = krnlSendMessage( cryptContext, IMESSAGE_CTX_ENCRYPT,
								  &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( cryptContext, IMESSAGE_CTX_DECRYPT,
									  &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
#else
		status = cryptDeviceQueryCapability( cryptContext, 1001,
									( CRYPT_QUERY_INFO * ) &keyAgreeParams );
		if( cryptStatusOK( status ) )
			status = cryptDeviceQueryCapability( cryptContext, 1002,
										( CRYPT_QUERY_INFO * ) &keyAgreeParams );
#endif /* 0 */
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't perform DH/ECDH key agreement, status = %d, "
					"line %d.\n", status, __LINE__ );
			return( status );
			}

		return( CRYPT_OK );
		}
#endif /* TEST_DH */
	if( cryptAlgo == CRYPT_ALGO_RSA )
		{
		static const BYTE FAR_BSS rsa512Value[] = \
			"\x4E\x1F\x2F\x10\xA9\xFB\x4F\xD9\xC1\x25\x79\x7A\x36\x00\x58\xD0"
			"\x9E\x8B\x9F\xBA\xC7\x04\x10\x77\xDB\xBC\xC9\xD1\x70\xCD\xF6\x86"
			"\xA4\xDC\x39\xA9\x57\xD7\xC7\xE0\x87\xF2\x31\xDF\x83\x7d\x27\x0E"
			"\xB4\xA6\x93\x3D\x11\xEB\xA5\x0E\x42\x66\x7B\x30\x50\x84\xC1\x81";
		static const BYTE FAR_BSS rsa1024Value[] = \
			"\x84\x8E\x00\x3E\x49\x11\x0D\x42\x4C\x71\x6B\xB4\xCF\x13\xDD\xCD"
			"\x12\x30\x56\xC2\x4A\x55\x3B\xD8\x30\xA2\xB8\x73\xA7\xAB\xF0\x7A"
			"\x2E\x07\x20\xCC\xBE\xEA\x58\x03\x56\xF6\x18\x27\x28\x4F\xE1\x02"
			"\xC6\x49\x79\x6C\xB4\x7E\x6C\xC6\x93\x2E\xF1\x46\x83\x15\x5A\xB7"
			"\x7D\xCC\x21\xEE\x4E\x3E\x0B\x8B\x85\xEE\x08\x21\xE6\xA7\x31\x53"
			"\x2E\x92\x3D\x2D\xB0\xD4\xA1\x30\xF4\xE9\xEB\x37\xBF\xCD\x2F\xE1"
			"\x60\x89\x19\xB6\x8C\x01\xFB\xD8\xAC\xF5\xC7\x4B\xB4\x74\x8A\x35"
			"\x79\xE6\xE0\x48\xBD\x9C\x9F\xD7\x4A\x1C\x8A\x58\xAB\xA9\x3C\x44";
		BYTE testBuffer[ TESTBUFFER_SIZE ], tmpBuffer[ 32 ];
		BOOLEAN encryptOK = TRUE;
		int length;

		/* Take a copy of the input so that we can compare it with the 
		   decrypted output and find out how much data we need to process */
		memcpy( testBuffer, buffer, TESTBUFFER_SIZE );
		cryptGetAttribute( cryptContext, CRYPT_CTXINFO_KEYSIZE, &length );

		/* Since we're doing raw RSA encryption we need to format the data
		   specially to work with the RSA key being used.  If we're using the
		   cryptlib native routines, we need to ensure that the magnitude of
		   the integer corresponding to the data to be encrypted is less than
		   the modulus, which we do by setting the first byte of the buffer
		   to 1.  If we're using a crypto device, we need to create a (dummy)
		   PKCS #1-like format since some devices expect to see PKCS #1-
		   formatted data as input to/output from the RSA encryption/
		   decryption operation */
		memcpy( tmpBuffer, buffer, 18 );
		if( isDevice )
			{
			memcpy( buffer, "\x00\x02\xA5\xA5\xA5\xA5\xA5\xA5"
							"\xA5\xA5\xA5\xA5\xA5\xA5\xA5\xA5"
							"\xA5\x00", 18 );
			}
		else
			buffer[ 0 ] = 1;

		/* Since the PKC algorithms only handle a single block, we only
		   perform a single encrypt and decrypt operation */
		status = cryptEncrypt( cryptContext, buffer, length );
		if( cryptStatusError( status ) )
			{
			if( !noWarnFail )
				printf( "Couldn't encrypt data, status = %d, line %d.\n", 
						status, __LINE__ );
			return( status );
			}
		if( cryptAlgo == CRYPT_ALGO_RSA && \
			!isDevice && buffer != localBuffer && \
			memcmp( buffer, ( length == 64 ) ? rsa512Value : rsa1024Value,
					length ) )
			{
			/* For a non-randomized PKC the encryption of the fixed value
			   produces known output, so if we're being called from with a
			   fixed test key from testLowlevel() we make sure that this 
			   matches the expected value.  This makes diagnosing problems 
			   rather easier */
			puts( "The actual encrypted value doesn't match the expected value." );
			encryptOK = FALSE;
			}
		status = cryptDecrypt( decryptContext, buffer, length );
		if( cryptStatusError( status ) )
			{
			if( !noWarnFail )
				{
				if( encryptOK )
					{
					printf( "Couldn't decrypt data even though the "
							"encrypted input data was valid,\nstatus = "
							"%d, line %d.\n", status, __LINE__ );
					}
				else
					{
					printf( "Couldn't decrypt data, probably because the "
							"data produced by the encrypt step\nwas "
							"invalid, status = %d, line %d.\n", status, 
							__LINE__ );
					}
				}
			return( status );
			}
		if( isDevice )
			memcpy( buffer, tmpBuffer, 18 );
		else
			buffer[ 0 ] = tmpBuffer[ 0 ];

		/* Make sure that the recovered result matches the input data */
		if( memcmp( buffer, testBuffer, length ) )
			{
			if( encryptOK )
				{
				/* This could happen with simple-minded CRT implementations
				   that only work when p > q (the test key has p < q in
				   order to find this problem) */
				puts( "Decryption failed even though encryption produced "
					  "valid data.  The RSA\ndecryption step is broken." );
				}
			else
				{
				puts( "Decryption failed because the encryption step "
					  "produced invalid data. The RSA\nencryption step is "
					  "broken." );
				}
			return( CRYPT_ERROR_FAILED );
			}
		else
			if( !encryptOK )
				{
				puts( "Decryption succeeded even though encryption produced "
					  "invalid data.  The RSA\nimplementation is broken." );
				return( CRYPT_ERROR_FAILED );
				}

		return( CRYPT_OK );
		}
	if( cryptAlgo >= CRYPT_ALGO_FIRST_HASH && \
		cryptAlgo <= CRYPT_ALGO_LAST_MAC )
		{
		/* Hash the buffer in two odd-sized chunks.  Note the use of the hash
		   wrap-up call, this is the only time when we can call
		   cryptEncrypt() with a zero length */
		status = cryptEncrypt( cryptContext, buffer, 80 );
		if( cryptStatusOK( status ) )
			status = cryptEncrypt( cryptContext, buffer + 80,
								   TESTBUFFER_SIZE - 80 );
		if( cryptStatusOK( status ) )
			status = cryptEncrypt( cryptContext, buffer + TESTBUFFER_SIZE, 0 );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't %s data, status = %d, line %d.\n", 
					( cryptAlgo >= CRYPT_ALGO_FIRST_MAC ) ? "MAC" : "hash",
					status, __LINE__ );
			return( status );
			}

		/* If we're just testing for the ability to use a context, the same 
		   hash context may be used for both operations, in which case we 
		   have to reset the context between the two */
		if( cryptContext == decryptContext )
			cryptDeleteAttribute( cryptContext, CRYPT_CTXINFO_HASHVALUE );

		/* Hash the buffer in different odd-size chunks */
		status = cryptEncrypt( decryptContext, buffer, 128 );
		if( cryptStatusOK( status ) )
			status = cryptEncrypt( decryptContext, buffer + 128,
								   TESTBUFFER_SIZE - 128 );
		if( cryptStatusOK( status ) )
			status = cryptEncrypt( decryptContext, buffer + TESTBUFFER_SIZE, 0 );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't %s data, status = %d, line %d.\n", 
					( cryptAlgo >= CRYPT_ALGO_FIRST_MAC ) ? "MAC" : "hash",
					status, __LINE__ );
			return( status );
			}

		return( CRYPT_OK );
		}

	printf( "Unknown encryption algorithm/mode %d.\n", cryptAlgo );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Low-level Routines Test							*
*																			*
****************************************************************************/

/* Test an algorithm/mode implementation */

int testLowlevel( const CRYPT_DEVICE cryptDevice,
				  const CRYPT_ALGO_TYPE cryptAlgo, const BOOLEAN checkOnly )
	{
	CRYPT_MODE_TYPE cryptMode = CRYPT_MODE_ECB;
	CRYPT_CONTEXT cryptContext, decryptContext;
	BYTE buffer[ TESTBUFFER_SIZE ], testBuffer[ TESTBUFFER_SIZE ];
	const BOOLEAN isDevice = ( cryptDevice != CRYPT_UNUSED ) ? TRUE : FALSE;
	BOOLEAN modesTested[ 8 ] = { 0 }, testSucceeded = FALSE;
	int status;

	/* Initialise the test buffers */
	initTestBuffers( buffer, testBuffer, TESTBUFFER_SIZE );

	/* Check cryptlib's capabilities */
	if( !checkLowlevelInfo( cryptDevice, cryptAlgo ) )
		return( FALSE );

	/* If we're only doing a capability check, don't try anything else */
	if( checkOnly )
		return( TRUE );

	/* Since DH/ECDH and KEA only perform key agreement rather than a true 
	   key exchange we can't test their encryption capabilities unless we're
	   using a custom-modified version of cryptlib */
#ifndef TEST_DH
	if( cryptAlgo == CRYPT_ALGO_DH || cryptAlgo == CRYPT_ALGO_ECDH )
		return( TRUE );
#endif /* TEST_DH */

	/* Test each mode of an algorithm.  We have to be very careful about
	   destroying any objects we create before we exit, because objects left
	   active in a device will prevent it from being shut down once the
	   tests have completed */
	do
		{
		/* Set up an encryption context, load a user key into it, and
		   perform a key setup */
		switch( cryptAlgo )
			{
			case CRYPT_ALGO_DES:
				status = loadContexts( &cryptContext, &decryptContext,
									   cryptDevice, cryptAlgo, cryptMode,
									   ( BYTE * ) "12345678", 8 );
				break;

			case CRYPT_ALGO_CAST:
			case CRYPT_ALGO_IDEA:
			case CRYPT_ALGO_AES:
				status = loadContexts( &cryptContext, &decryptContext,
									   cryptDevice, cryptAlgo, cryptMode,
									   ( BYTE * ) "1234567887654321", 16 );
				break;

			case CRYPT_ALGO_3DES:
				status = loadContexts( &cryptContext, &decryptContext,
									   cryptDevice, cryptAlgo, cryptMode,
									   ( BYTE * ) "123456788765432112345678", 24 );
				break;

			case CRYPT_ALGO_RC2:
			case CRYPT_ALGO_RC4:
			case CRYPT_ALGO_RC5:
			case CRYPT_ALGO_BLOWFISH:
			case CRYPT_ALGO_HMAC_MD5:
			case CRYPT_ALGO_HMAC_SHA1:
			case CRYPT_ALGO_HMAC_RIPEMD160:
			case CRYPT_ALGO_HMAC_SHA2:
				status = loadContexts( &cryptContext, &decryptContext,
									   cryptDevice, cryptAlgo, cryptMode,
									   ( BYTE * ) "1234567890098765432112345678900987654321", 40 );
				break;

			case CRYPT_ALGO_MD5:
			case CRYPT_ALGO_SHA1:
			case CRYPT_ALGO_SHA2:
			case CRYPT_ALGO_RIPEMD160:
				status = loadContexts( &cryptContext, &decryptContext,
									   cryptDevice, cryptAlgo, CRYPT_MODE_NONE,
									   ( BYTE * ) "", 0 );
				break;

#ifdef TEST_DH
			case CRYPT_ALGO_DH:
				status = loadDHKey( cryptDevice, &cryptContext );
				break;
#endif /* TEST_DH */

			case CRYPT_ALGO_RSA:
				status = loadRSAContexts( cryptDevice, &cryptContext,
										  &decryptContext );
				break;

			case CRYPT_ALGO_DSA:
				status = loadDSAContexts( cryptDevice, &cryptContext,
										  &decryptContext );
				break;

			case CRYPT_ALGO_ELGAMAL:
				status = loadElgamalContexts( &cryptContext, &decryptContext );
				break;

			case CRYPT_ALGO_ECDSA:
				status = loadECDSAContexts( &cryptContext, &decryptContext );
				break;

#ifdef TEST_DH
			case CRYPT_ALGO_ECDH:
				status = loadECDHKey( cryptDevice, &cryptContext );
				break;
#endif /* TEST_DH */

			default:
				printf( "Unknown encryption algorithm ID %d, cannot perform "
						"encryption test, line %d.\n", cryptAlgo, __LINE__ );
				return( FALSE );
			}
		if( status == CRYPT_ERROR_NOTAVAIL )
			{
			/* It's a conventional algorithm for which this mode isn't
			   available, try a different mode */
			cryptMode++;
			continue;
			}
		if( !status )
			return( FALSE );

		/* DLP-based algorithms can't be called directly from user code
		   because of the special data-formatting requirements */
		if( cryptAlgo == CRYPT_ALGO_DSA || cryptAlgo == CRYPT_ALGO_ELGAMAL || \
			cryptAlgo == CRYPT_ALGO_ECDSA )
			{
			destroyContexts( cryptDevice, cryptContext, decryptContext );
			return( TRUE );
			}

		/* Perform a test en/decryption */
		status = testCrypt( cryptContext, decryptContext, buffer, isDevice,
							FALSE );
		if( cryptStatusError( status ) )
			{
			destroyContexts( cryptDevice, cryptContext, decryptContext );
			if( isDevice && status == CRYPT_ERROR_NOTAVAIL )
				{
				/* Some primitive tokens or accelerators support only the
				   barest minimum of functionality, which may include being
				   able to create objects but not use them (e.g. public key
				   objects in a device which is just an RSA private-key
				   modexp engine).  Because of this we may get a
				   CRYPT_ERROR_NOTAVAIL when we try and perform a low-level
				   crypto test, this isn't normally a problem for cryptlib
				   high-level objects because public-key ops are always done
				   in software, but when we explicitly try to do them in the
				   token it's a problem.  Because of this we report a problem
				   but continue anyway */
				puts( "The crypto device reported that this operation isn't "
					  "available even though it\nsupports the use of "
					  "encryption objects that implement this algorithm.  "
					  "This\nis probably a bare-bones device that only "
					  "supports minimal functionality (eg\nprivate-key "
					  "decryption but not encryption)." );
				continue;
				}
			return( FALSE );
			}

		/* Make sure that everything went OK */
		if( cryptAlgo >= CRYPT_ALGO_FIRST_HASH )
			{
			BYTE hash1[ CRYPT_MAX_HASHSIZE ], hash2[ CRYPT_MAX_HASHSIZE ];
			int length1, length2;

			status = cryptGetAttributeString( cryptContext, CRYPT_CTXINFO_HASHVALUE,
											  hash1, &length1 );
			cryptGetAttributeString( decryptContext, CRYPT_CTXINFO_HASHVALUE,
									 hash2, &length2 );
			if( cryptStatusError( status ) )
				{
				printf( "Couldn't get hash information, status = %d, "
						"line %d.\n", status, __LINE__ );
				destroyContexts( cryptDevice, cryptContext, decryptContext );
				return( FALSE );
				}
			if( ( length1 != length2 ) || memcmp( hash1, hash2, length1 ) )
				{
				puts( "Error: Hash value of identical buffers differs." );
				destroyContexts( cryptDevice, cryptContext, decryptContext );
				return( FALSE );
				}
			if( !memcmp( hash1, "\x00\x00\x00\x00\x00\x00\x00\x00", 8 ) || \
				!memcmp( hash2, "\x00\x00\x00\x00\x00\x00\x00\x00", 8 ) )
				{
				puts( "Error: Hash contains all zeroes." );
				destroyContexts( cryptDevice, cryptContext, decryptContext );
				return( FALSE );
				}

			/* Make sure that we can get repeatable results after deleting
			   the hash/MAC and rehashing the data */
			status = cryptDeleteAttribute( cryptContext,
										   CRYPT_CTXINFO_HASHVALUE );
			if( cryptStatusOK( status ) )
				status = cryptDeleteAttribute( decryptContext,
											   CRYPT_CTXINFO_HASHVALUE );
			if( cryptStatusError( status ) )
				{
				printf( "Deletion of hash/MAC value failed with status %d, "
						"line %d.\n", status, __LINE__ );
				destroyContexts( cryptDevice, cryptContext, decryptContext );
				return( FALSE );
				}
			if( cryptStatusError( testCrypt( cryptContext, decryptContext,
											 buffer, isDevice, FALSE ) ) )
				{
				destroyContexts( cryptDevice, cryptContext, decryptContext );
				return( FALSE );
				}
			status = cryptGetAttributeString( cryptContext, CRYPT_CTXINFO_HASHVALUE,
											  hash1, &length1 );
			if( cryptStatusError( status ) )
				{
				printf( "Couldn't get hash information for re-hashed data, "
						"status = %d, line %d.\n", status, __LINE__ );
				destroyContexts( cryptDevice, cryptContext, decryptContext );
				return( FALSE );
				}
			if( ( length1 != length2 ) || memcmp( hash1, hash2, length1 ) )
				{
				puts( "Error: Hash value of re-hashed data differs." );
				destroyContexts( cryptDevice, cryptContext, decryptContext );
				return( FALSE );
				}
			}
		else
			{
			/* If it's a PKC we'll have performed the check during the
			   encrypt/decrypt step */
			if( cryptAlgo < CRYPT_ALGO_FIRST_PKC && \
				!checkTestBuffers( buffer, testBuffer ) )
				{
				destroyContexts( cryptDevice, cryptContext, decryptContext );
				return( FALSE );
				}
			}

		/* Remember that at least one test succeeded */
		testSucceeded = TRUE;
		if( cryptAlgo < CRYPT_ALGO_LAST_CONVENTIONAL )
			modesTested[ cryptMode++ ] = TRUE;

		/* Clean up */
		destroyContexts( cryptDevice, cryptContext, decryptContext );
		}
	while( cryptAlgo < CRYPT_ALGO_LAST_CONVENTIONAL && \
		   cryptMode < CRYPT_MODE_LAST );

	/* If it's a conventional algorithm, report the encryption modes that
	   were tested */
	if( cryptAlgo < CRYPT_ALGO_LAST_CONVENTIONAL )
		{
		printf( "  Encryption modes tested:" );
		if( modesTested[ CRYPT_MODE_ECB ] )
			printf( " ECB" );
		if( modesTested[ CRYPT_MODE_CBC ] )
			printf( " CBC" );
		if( modesTested[ CRYPT_MODE_CFB ] )
			printf( " CFB" );
		if( modesTested[ CRYPT_MODE_OFB ] )
			printf( " OFB" );
		if( modesTested[ CRYPT_MODE_GCM ] )
			printf( " GCM" );
		puts( "." );
		}

	/* Make sure that at least one of the algorithm's modes was tested */
	if( !testSucceeded )
		{
		puts( "No processing modes were found for this algorithm.\n" );
		return( FALSE );
		}

	return( TRUE );
	}

/* Test the ability of the RSA key-load code to reconstruct a full RSA key
   from only the minimal non-CRT components */

int testRSAMinimalKey( void )
	{
	CRYPT_CONTEXT cryptContext, decryptContext;
	BYTE buffer[ TESTBUFFER_SIZE ], testBuffer[ TESTBUFFER_SIZE ];
	int status;

	puts( "Testing ability to recover CRT components for RSA private key..." );

	/* Load the RSA contexts from the minimal (non-CRT) RSA key */
	status = loadRSAContextsEx( CRYPT_UNUSED, &cryptContext, &decryptContext,
								RSA_PUBKEY_LABEL, RSA_PRIVKEY_LABEL, FALSE, 
								TRUE );
	if( !status )
		return( FALSE );

	/* Initialise the test buffers */
	initTestBuffers( buffer, testBuffer, TESTBUFFER_SIZE );

	/* Make sure that we can encrypt and decrypt with the reconstituted CRT
	   private key */
	status = testCrypt( cryptContext, decryptContext, buffer, FALSE, FALSE );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Clean up */
	destroyContexts( CRYPT_UNUSED, cryptContext, decryptContext );

	puts( "RSA CRT component recovery test succeeded." );
	return( TRUE );
	}

/****************************************************************************
*																			*
*								Performance Tests							*
*																			*
****************************************************************************/

/* General performance characteristics test.  Since high-precision timing is
   rather OS-dependent, we only enable this under Windows where we've got
   guaranteed high-res timer access */

#if defined( __WINDOWS__ ) && defined( _MSC_VER ) && ( _MSC_VER >= 1100 )

#include <math.h>	/* For sqrt() for standard deviation */

#define NO_TESTS	25

/* Print timing info.  This gets a bit hairy because we're actually counting
   timer ticks rather than thread times, which means we'll be affected by
   things like context switches.  There are two approaches to this:

	1. Take the fastest time, which will be the time least affected by system
	   overhead.

	2. Apply standard statistical techniques to weed out anomalies.  Since
	   this is just for testing purposes all we do is discard any results
	   out by more than 10%, which is crude but reasonably effective.  A
	   more rigorous approach is to discards results more than n standard
	   deviations out, but this gets screwed up by the fact that a single
	   context switch of 20K ticks can throw out results from an execution
	   time of only 50 ticks.  In any case (modulo context switches) the
	   fastest, 10%-out, and 2 SD out times are all within about 1% of each
	   other, so all methods are roughly equally accurate */

static void printTimes( long times[ NO_TESTS + 1 ][ 8 ] )
	{
	int i;

	for( i = 0; i < 7; i++ )
		{
		long timeSum = 0, timeAvg, timeDelta;
		long timeMin = 1000000L, timeCorrSum10 = 0, timeCorrSumSD = 0;
#ifdef USE_SD
		double stdDev;
#endif /* USE_SD */
		int j, timesCount10 = 0, timesCountSD = 0;

		/* Find the mean execution time */
		for( j = 1; j < NO_TESTS + 1; j++ )
			timeSum += times[ j ][ i ];
		timeAvg = timeSum / NO_TESTS;
		timeDelta = timeSum / 10;	/* 10% variation */
		if( timeSum == 0 )
			{
			/* Some ciphers can't provide results for some cases (e.g.
			   AES for 8-byte blocks) */
			printf( "      " );
			continue;
			}

		/* Find the fastest overall time */
		for( j = 1; j < NO_TESTS + 1; j++ )
			if( times[ j ][ i ] < timeMin )
				timeMin = times[ j ][ i ];

		/* Find the mean time, discarding anomalous results more than 10%
		   out */
		for( j = 1; j < NO_TESTS + 1; j++ )
			if( times[ j ][ i ] > timeAvg - timeDelta && \
				times[ j ][ i ] < timeAvg + timeDelta )
				{
				timeCorrSum10 += times[ j ][ i ];
				timesCount10++;
				}
		printf( "%6d", timeCorrSum10 / timesCount10 );
#if 0	/* Print difference to fastest time, usually only around 1% */
		printf( "(%4d)", ( timeCorrSum10 / timesCount10 ) - timeMin );
#endif /* 0 */

#ifdef USE_SD
		/* Find the standard deviation */
		for( j = 1; j < NO_TESTS + 1; j++ )
			{
			const long timeDev = times[ j ][ i ] - timeAvg;

			timeCorrSumSD += ( timeDev * timeDev );
			}
		stdDev = timeCorrSumSD / NO_TESTS;
		stdDev = sqrt( stdDev );

		/* Find the mean time, discarding anomalous results more than two
		   standard deviations out */
		timeCorrSumSD = 0;
		timeDelta = ( long ) stdDev * 2;
		for( j = 1; j < NO_TESTS + 1; j++ )
			if( times[ j ][ i ] > timeAvg - timeDelta && \
				times[ j ][ i ] < timeAvg + timeDelta )
				{
				timeCorrSumSD += times[ j ][ i ];
				timesCountSD++;
				}
		if( timesCountSD == 0 )
			timesCountSD++;	/* Context switch, fudge it */
		printf( "%6d", timeCorrSumSD / timesCountSD );

#if 1	/* Print difference to fastest and mean times, usually only around
		   1% */
		printf( " (dF = %4d, dM = %4d)\n",
				( timeCorrSumSD / timesCountSD ) - timeMin,
				abs( ( timeCorrSumSD / timesCountSD ) - \
					 ( timeCorrSum10 / timesCount10 ) ) );
#endif /* 0 */
#endif /* USE_SD */
		}
	printf( "\n" );
	}

static long encOne( const CRYPT_CONTEXT cryptContext, BYTE *buffer,
					const int length )
	{
	unsigned long timeVal;
	int status;

	memset( buffer, '*', length );
	timeVal = timeDiff( 0 );
	status = cryptEncrypt( cryptContext, buffer, length );
	return( timeDiff( timeVal ) );
	}

static int encTest( const CRYPT_CONTEXT cryptContext,
					const CRYPT_ALGO_TYPE cryptAlgo, BYTE *buffer,
					long times[] )
	{
	int index = 0;

	times[ index++ ] = ( cryptAlgo != CRYPT_ALGO_AES ) ? \
					   encOne( cryptContext, buffer, 8 ) : 0;
	times[ index++ ] = encOne( cryptContext, buffer, 16 );
	times[ index++ ] = encOne( cryptContext, buffer, 64 );
	times[ index++ ] = encOne( cryptContext, buffer, 1024 );
	times[ index++ ] = encOne( cryptContext, buffer, 4096 );
	times[ index++ ] = encOne( cryptContext, buffer, 8192 );
	times[ index++ ] = encOne( cryptContext, buffer, 65536L );
	return( TRUE );
	}

static int encTests( const CRYPT_DEVICE cryptDevice,
					 const CRYPT_ALGO_TYPE cryptAlgo,
					 const CRYPT_ALGO_TYPE cryptMode,
					 BYTE *buffer )
	{
	CRYPT_CONTEXT cryptContext;
	unsigned long times[ NO_TESTS + 1 ][ 8 ], timeVal, timeSum = 0;
	int i, status;

	memset( buffer, 0, 100000L );

	/* Set up the context for use */
	if( !checkLowlevelInfo( cryptDevice, cryptAlgo ) )
		return( FALSE );
	for( i = 0; i < 10; i++ )
		{
		timeVal = timeDiff( 0 );
		status = loadContexts( &cryptContext, NULL, cryptDevice,
							   cryptAlgo, cryptMode,
							   ( BYTE * ) "12345678901234567890",
							   ( cryptAlgo == CRYPT_ALGO_DES ) ? 8 : \
							   ( cryptAlgo == CRYPT_ALGO_3DES || \
							     cryptAlgo == CRYPT_ALGO_RC4 || \
								 cryptAlgo == CRYPT_ALGO_AES ) ? 16 : 0 );
		timeVal = timeDiff( timeVal );
		if( status == CRYPT_ERROR_NOTAVAIL || !status )
			return( FALSE );
		timeSum += timeVal;
		if( i < 9 )
			cryptDestroyContext( cryptContext );
		}
	printf( "Setup time = %d ticks.\n", timeSum / 10 );
	puts( "     8    16    64    1K    4K    8K   64K" );
	puts( "  ----  ----  ----  ----  ----  ----  ----" );

	/* Run the encryption tests NO_TESTS times, discard the first set of
	   results since the cache will be empty at that point */
	for( i = 0; i < NO_TESTS + 1; i++ )
		encTest( cryptContext, cryptAlgo, buffer, times[ i ] );
	printTimes( times );

	/* Re-run the encryption tests with a 1-byte misalignment */
	for( i = 0; i < NO_TESTS + 1; i++ )
		encTest( cryptContext, cryptAlgo, buffer + 1, times[ i ] );
	printTimes( times );

	/* Re-run the encryption tests with a 4-byte misalignment */
	for( i = 0; i < NO_TESTS + 1; i++ )
		encTest( cryptContext, cryptAlgo, buffer + 4, times[ i ] );
	printTimes( times );

	/* Re-run the test 1000 times with various buffer alignments */
	timeVal = 0;
	for( i = 0; i < 1000; i++ )
		timeVal += encOne( cryptContext, buffer, 1024 );
	printf( "Aligned: %d ", timeVal / 1000 );
	timeVal = 0;
	for( i = 0; i < 1000; i++ )
		timeVal += encOne( cryptContext, buffer + 1, 1024 );
	printf( "misaligned + 1: %d ", timeVal / 1000 );
	timeVal = 0;
	for( i = 0; i < 1000; i++ )
		timeVal += encOne( cryptContext, buffer + 4, 1024 );
	printf( "misaligned + 4: %d.\n", timeVal / 1000 );

	return( TRUE );
	}

void performanceTests( const CRYPT_DEVICE cryptDevice )
	{
	LARGE_INTEGER performanceCount;
	BYTE *buffer;

	QueryPerformanceFrequency( &performanceCount );
	printf( "Clock ticks %d times per second.\n", performanceCount.LowPart );
	if( ( buffer = malloc( 100000L ) ) == NULL )
		{
		puts( "Couldn't 100K allocate test buffer." );
		return;
		}
	encTests( CRYPT_UNUSED, CRYPT_ALGO_DES, CRYPT_MODE_ECB, buffer );
	encTests( CRYPT_UNUSED, CRYPT_ALGO_DES, CRYPT_MODE_CBC, buffer );
	encTests( CRYPT_UNUSED, CRYPT_ALGO_3DES, CRYPT_MODE_ECB, buffer );
	encTests( CRYPT_UNUSED, CRYPT_ALGO_3DES, CRYPT_MODE_CBC, buffer );
	encTests( CRYPT_UNUSED, CRYPT_ALGO_RC4, CRYPT_MODE_OFB, buffer );
	encTests( CRYPT_UNUSED, CRYPT_ALGO_AES, CRYPT_MODE_CBC, buffer );
	encTests( CRYPT_UNUSED, CRYPT_ALGO_MD5, CRYPT_MODE_NONE, buffer );
	encTests( CRYPT_UNUSED, CRYPT_ALGO_SHA1, CRYPT_MODE_NONE, buffer );
	free( buffer );
	}
#endif /* Win32 with VC++ */

#endif /* TEST_LOWLEVEL || TEST_KEYSET */
