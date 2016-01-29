/****************************************************************************
*																			*
*						cryptlib IDEA Encryption Routines					*
*						Copyright Peter Gutmann 1992-2005					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "idea.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "crypt/idea.h"
#endif /* Compiler-specific includes */

#ifdef USE_IDEA

/* Defines to map from EAY to native naming */

#define IDEA_BLOCKSIZE				IDEA_BLOCK

/* A structure to hold the two expanded IDEA keys */

typedef struct {
	IDEA_KEY_SCHEDULE eKey, dKey;
	} IDEA_KEY;

/* The size of the expanded IDEA keys */

#define IDEA_EXPANDED_KEYSIZE		sizeof( IDEA_KEY )

/****************************************************************************
*																			*
*								IDEA Self-test Routines						*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* IDEA test vectors, from the ETH reference implementation */

/* The data structure for the ( key, plaintext, ciphertext ) triplets */

typedef struct {
	const BYTE key[ IDEA_KEY_LENGTH ];
	const BYTE plaintext[ IDEA_BLOCKSIZE ];
	const BYTE ciphertext[ IDEA_BLOCKSIZE ];
	} IDEA_TEST;

static const IDEA_TEST FAR_BSS testIdea[] = {
	{ { 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
		0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08 },
	  { 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03 },
	  { 0x11, 0xFB, 0xED, 0x2B, 0x01, 0x98, 0x6D, 0xE5 } },
	{ { 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
		0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08 },
	  { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 },
	  { 0x54, 0x0E, 0x5F, 0xEA, 0x18, 0xC2, 0xF8, 0xB1 } },
	{ { 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
		0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08 },
	  { 0x00, 0x19, 0x32, 0x4B, 0x64, 0x7D, 0x96, 0xAF },
	  { 0x9F, 0x0A, 0x0A, 0xB6, 0xE1, 0x0C, 0xED, 0x78 } },
	{ { 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
		0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08 },
	  { 0xF5, 0x20, 0x2D, 0x5B, 0x9C, 0x67, 0x1B, 0x08 },
	  { 0xCF, 0x18, 0xFD, 0x73, 0x55, 0xE2, 0xC5, 0xC5 } },
	{ { 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
		0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08 },
	  { 0xFA, 0xE6, 0xD2, 0xBE, 0xAA, 0x96, 0x82, 0x6E },
	  { 0x85, 0xDF, 0x52, 0x00, 0x56, 0x08, 0x19, 0x3D } },
	{ { 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
		0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08 },
	  { 0x0A, 0x14, 0x1E, 0x28, 0x32, 0x3C, 0x46, 0x50 },
	  { 0x2F, 0x7D, 0xE7, 0x50, 0x21, 0x2F, 0xB7, 0x34 } },
	{ { 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
		0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08 },
	  { 0x05, 0x0A, 0x0F, 0x14, 0x19, 0x1E, 0x23, 0x28 },
	  { 0x7B, 0x73, 0x14, 0x92, 0x5D, 0xE5, 0x9C, 0x09 } },
	{ { 0x00, 0x05, 0x00, 0x0A, 0x00, 0x0F, 0x00, 0x14,
		0x00, 0x19, 0x00, 0x1E, 0x00, 0x23, 0x00, 0x28 },
	  { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 },
	  { 0x3E, 0xC0, 0x47, 0x80, 0xBE, 0xFF, 0x6E, 0x20 } },
	{ { 0x3A, 0x98, 0x4E, 0x20, 0x00, 0x19, 0x5D, 0xB3,
		0x2E, 0xE5, 0x01, 0xC8, 0xC4, 0x7C, 0xEA, 0x60 },
	  { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 },
	  { 0x97, 0xBC, 0xD8, 0x20, 0x07, 0x80, 0xDA, 0x86 } },
	{ { 0x00, 0x64, 0x00, 0xC8, 0x01, 0x2C, 0x01, 0x90,
		0x01, 0xF4, 0x02, 0x58, 0x02, 0xBC, 0x03, 0x20 },
	  { 0x05, 0x32, 0x0A, 0x64, 0x14, 0xC8, 0x19, 0xFA },
	  { 0x65, 0xBE, 0x87, 0xE7, 0xA2, 0x53, 0x8A, 0xED } },
	{ { 0x9D, 0x40, 0x75, 0xC1, 0x03, 0xBC, 0x32, 0x2A,
		0xFB, 0x03, 0xE7, 0xBE, 0x6A, 0xB3, 0x00, 0x06 },
	  { 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08 },
	  { 0xF5, 0xDB, 0x1A, 0xC4, 0x5E, 0x5E, 0xF9, 0xF9 } }
	};

/* Test the IDEA code against the test vectors from the ETH reference
   implementation */

static int selfTest( void )
	{
	const CAPABILITY_INFO *capabilityInfo = getIDEACapability();
	BYTE keyData[ IDEA_EXPANDED_KEYSIZE + 8 ];
	int i, status;

	for( i = 0; i < sizeof( testIdea ) / sizeof( IDEA_TEST ); i++ )
		{
		status = testCipher( capabilityInfo, keyData, testIdea[ i ].key, 
							 16, testIdea[ i ].plaintext, 
							 testIdea[ i ].ciphertext );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}
#else
	#define selfTest	NULL
#endif /* !CONFIG_NO_SELFTEST */

/****************************************************************************
*																			*
*								Control Routines							*
*																			*
****************************************************************************/

/* Return context subtype-specific information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int getInfo( IN_ENUM( CAPABILITY_INFO ) const CAPABILITY_INFO_TYPE type, 
					INOUT_OPT CONTEXT_INFO *contextInfoPtr,
					OUT void *data, 
					IN_INT_Z const int length )
	{
	assert( contextInfoPtr == NULL || \
			isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( ( length == 0 && isWritePtr( data, sizeof( int ) ) ) || \
			( length > 0 && isWritePtr( data, length ) ) );

	REQUIRES( type > CAPABILITY_INFO_NONE && type < CAPABILITY_INFO_LAST );

	if( type == CAPABILITY_INFO_STATESIZE )
		{
		int *valuePtr = ( int * ) data;

		*valuePtr = IDEA_EXPANDED_KEYSIZE;

		return( CRYPT_OK );
		}

	return( getDefaultInfo( type, contextInfoPtr, data, length ) );
	}

/****************************************************************************
*																			*
*							IDEA En/Decryption Routines						*
*																			*
****************************************************************************/

/* Encrypt/decrypt data in ECB mode */

static int encryptECB( CONTEXT_INFO *contextInfoPtr, BYTE *buffer,
					   int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	IDEA_KEY *ideaKey = ( IDEA_KEY * ) convInfo->key;
	int blockCount = noBytes / IDEA_BLOCKSIZE;

	while( blockCount-- > 0 )
		{
		/* Encrypt a block of data */
		idea_ecb_encrypt( buffer, buffer, &ideaKey->eKey );

		/* Move on to next block of data */
		buffer += IDEA_BLOCKSIZE;
		}

	return( CRYPT_OK );
	}

static int decryptECB( CONTEXT_INFO *contextInfoPtr, BYTE *buffer,
					   int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	IDEA_KEY *ideaKey = ( IDEA_KEY * ) convInfo->key;
	int blockCount = noBytes / IDEA_BLOCKSIZE;

	while( blockCount-- > 0 )
		{
		/* Decrypt a block of data */
		idea_ecb_encrypt( buffer, buffer, &ideaKey->dKey );

		/* Move on to next block of data */
		buffer += IDEA_BLOCKSIZE;
		}

	return( CRYPT_OK );
	}

/* Encrypt/decrypt data in CBC mode */

static int encryptCBC( CONTEXT_INFO *contextInfoPtr, BYTE *buffer,
					   int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;

	idea_cbc_encrypt( buffer, buffer, noBytes,
					  &( ( IDEA_KEY * ) convInfo->key )->eKey,
					  convInfo->currentIV, IDEA_ENCRYPT );

	return( CRYPT_OK );
	}

static int decryptCBC( CONTEXT_INFO *contextInfoPtr, BYTE *buffer,
					   int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;

	idea_cbc_encrypt( buffer, buffer, noBytes,
					  &( ( IDEA_KEY * ) convInfo->key )->dKey,
					  convInfo->currentIV, IDEA_DECRYPT );

	return( CRYPT_OK );
	}

/* Encrypt/decrypt data in CFB mode */

static int encryptCFB( CONTEXT_INFO *contextInfoPtr, BYTE *buffer,
					   int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	IDEA_KEY *ideaKey = ( IDEA_KEY * ) convInfo->key;
	int i, ivCount = convInfo->ivCount;

	/* If there's any encrypted material left in the IV, use it now */
	if( ivCount > 0 )
		{
		int bytesToUse;

		/* Find out how much material left in the encrypted IV we can use */
		bytesToUse = IDEA_BLOCKSIZE - ivCount;
		if( noBytes < bytesToUse )
			bytesToUse = noBytes;

		/* Encrypt the data */
		for( i = 0; i < bytesToUse; i++ )
			buffer[ i ] ^= convInfo->currentIV[ i + ivCount ];
		memcpy( convInfo->currentIV + ivCount, buffer, bytesToUse );

		/* Adjust the byte count and buffer position */
		noBytes -= bytesToUse;
		buffer += bytesToUse;
		ivCount += bytesToUse;
		}

	while( noBytes > 0 )
		{
		ivCount = ( noBytes > IDEA_BLOCKSIZE ) ? IDEA_BLOCKSIZE : noBytes;

		/* Encrypt the IV */
		idea_ecb_encrypt( convInfo->currentIV, convInfo->currentIV,
						  &ideaKey->eKey );

		/* XOR the buffer contents with the encrypted IV */
		for( i = 0; i < ivCount; i++ )
			buffer[ i ] ^= convInfo->currentIV[ i ];

		/* Shift the ciphertext into the IV */
		memcpy( convInfo->currentIV, buffer, ivCount );

		/* Move on to next block of data */
		noBytes -= ivCount;
		buffer += ivCount;
		}

	/* Remember how much of the IV is still available for use */
	convInfo->ivCount = ( ivCount % IDEA_BLOCKSIZE );

	return( CRYPT_OK );
	}

/* Decrypt data in CFB mode.  Note that the transformation can be made
   faster (but less clear) with temp = buffer, buffer ^= iv, iv = temp
   all in one loop */

static int decryptCFB( CONTEXT_INFO *contextInfoPtr, BYTE *buffer,
					   int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	IDEA_KEY *ideaKey = ( IDEA_KEY * ) convInfo->key;
	BYTE temp[ IDEA_BLOCKSIZE + 8 ];
	int i, ivCount = convInfo->ivCount;

	/* If there's any encrypted material left in the IV, use it now */
	if( ivCount > 0 )
		{
		int bytesToUse;

		/* Find out how much material left in the encrypted IV we can use */
		bytesToUse = IDEA_BLOCKSIZE - ivCount;
		if( noBytes < bytesToUse )
			bytesToUse = noBytes;

		/* Decrypt the data */
		memcpy( temp, buffer, bytesToUse );
		for( i = 0; i < bytesToUse; i++ )
			buffer[ i ] ^= convInfo->currentIV[ i + ivCount ];
		memcpy( convInfo->currentIV + ivCount, temp, bytesToUse );

		/* Adjust the byte count and buffer position */
		noBytes -= bytesToUse;
		buffer += bytesToUse;
		ivCount += bytesToUse;
		}

	while( noBytes > 0 )
		{
		ivCount = ( noBytes > IDEA_BLOCKSIZE ) ? IDEA_BLOCKSIZE : noBytes;

		/* Encrypt the IV */
		idea_ecb_encrypt( convInfo->currentIV, convInfo->currentIV,
						  &ideaKey->eKey );

		/* Save the ciphertext */
		memcpy( temp, buffer, ivCount );

		/* XOR the buffer contents with the encrypted IV */
		for( i = 0; i < ivCount; i++ )
			buffer[ i ] ^= convInfo->currentIV[ i ];

		/* Shift the ciphertext into the IV */
		memcpy( convInfo->currentIV, temp, ivCount );

		/* Move on to next block of data */
		noBytes -= ivCount;
		buffer += ivCount;
		}

	/* Remember how much of the IV is still available for use */
	convInfo->ivCount = ( ivCount % IDEA_BLOCKSIZE );

	/* Clear the temporary buffer */
	zeroise( temp, IDEA_BLOCKSIZE );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							IDEA Key Management Routines					*
*																			*
****************************************************************************/

/* Key schedule an IDEA key */

static int initKey( CONTEXT_INFO *contextInfoPtr, const void *key,
					const int keyLength )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	IDEA_KEY *ideaKey = ( IDEA_KEY * ) convInfo->key;

	/* Copy the key to internal storage */
	if( convInfo->userKey != key )
		memcpy( convInfo->userKey, key, keyLength );
	convInfo->userKeyLength = keyLength;

	/* Generate the expanded IDEA encryption and decryption keys */
	idea_set_encrypt_key( key, &ideaKey->eKey );
	idea_set_decrypt_key( &ideaKey->eKey, &ideaKey->dKey );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO FAR_BSS capabilityInfo = {
	CRYPT_ALGO_IDEA, bitsToBytes( 64 ), "IDEA", 4,
	MIN_KEYSIZE, bitsToBytes( 128 ), bitsToBytes( 128 ),
	selfTest, getInfo, NULL, initGenericParams, initKey, NULL,
	encryptECB, decryptECB, encryptCBC, decryptCBC,
	encryptCFB, decryptCFB
	};

const CAPABILITY_INFO *getIDEACapability( void )
	{
	return( &capabilityInfo );
	}

#endif /* USE_IDEA */
