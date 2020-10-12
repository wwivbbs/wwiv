/****************************************************************************
*																			*
*					  cryptlib CAST-128 Encryption Routines					*
*						Copyright Peter Gutmann 1997-2005					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "cast.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "crypt/cast.h"
#endif /* Compiler-specific includes */

#ifdef USE_CAST

/* Defines to map from EAY to native naming */

#define CAST_BLOCKSIZE			CAST_BLOCK

/* The size of the keyscheduled CAST key */

#define CAST_EXPANDED_KEYSIZE	sizeof( CAST_KEY )

/****************************************************************************
*																			*
*								CAST Self-test Routines						*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* CAST test vectors from CAST specification */

static const struct CAST_TEST {
	BYTE key[ CAST_KEY_LENGTH + 8 ];
	BYTE plaintext[ CAST_BLOCKSIZE + 8 ];
	BYTE ciphertext[ CAST_BLOCKSIZE + 8 ];
	} testCAST[] = {
	{ { 0x01, 0x23, 0x45, 0x67, 0x12, 0x34, 0x56, 0x78,
		0x23, 0x45, 0x67, 0x89, 0x34, 0x56, 0x78, 0x9A },
	  { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF },
	  { 0x23, 0x8B, 0x4F, 0xE5, 0x84, 0x7E, 0x44, 0xB2 } }
	};

/* Test the CAST code against the CAST test vectors */

CHECK_RETVAL \
static int selfTest( void )
	{
	const CAPABILITY_INFO *capabilityInfo = getCASTCapability();
	typedef BYTE KEY_DATA[ CAST_EXPANDED_KEYSIZE + 8 ];
	DECLARE_ALIGN_STRUCT( KEY_DATA_STORAGE, KEY_DATA );
	KEY_DATA_STORAGE keyDataStorage;
	KEY_DATA *keyDataPtr = ALIGN_STRUCT( &keyDataStorage, KEY_DATA, 16 ); 
	int i, status, LOOP_ITERATOR;

	memset( keyDataPtr, 0, CAST_EXPANDED_KEYSIZE );	/* Keep static analysers happy */
	LOOP_SMALL( i = 0, i < sizeof( testCAST ) / sizeof( struct CAST_TEST ), i++ )
		{
		status = testCipher( capabilityInfo, keyDataPtr, testCAST[ i ].key, 
							 CAST_KEY_LENGTH, testCAST[ i ].plaintext,
							 testCAST[ i ].ciphertext );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

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
			( length > 0 && isWritePtrDynamic( data, length ) ) );

	REQUIRES( isEnumRange( type, CAPABILITY_INFO ) );
	REQUIRES( ( contextInfoPtr == NULL ) || \
			  sanityCheckContext( contextInfoPtr ) );

	if( type == CAPABILITY_INFO_STATESIZE )
		{
		int *valuePtr = ( int * ) data;

		*valuePtr = CAST_EXPANDED_KEYSIZE;

		return( CRYPT_OK );
		}

	return( getDefaultInfo( type, contextInfoPtr, data, length ) );
	}

/****************************************************************************
*																			*
*							CAST En/Decryption Routines						*
*																			*
****************************************************************************/

/* Encrypt/decrypt data in ECB mode */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptECB( INOUT CONTEXT_INFO *contextInfoPtr, 
					   INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					   IN_LENGTH int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	int blockCount = noBytes / CAST_BLOCKSIZE;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, noBytes ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isIntegerRangeNZ( noBytes ) );

	while( blockCount-- > 0 )
		{
		/* Encrypt a block of data */
		CAST_ecb_encrypt( buffer, buffer, convInfo->key, CAST_ENCRYPT );

		/* Move on to next block of data */
		buffer += CAST_BLOCKSIZE;
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int decryptECB( INOUT CONTEXT_INFO *contextInfoPtr, 
					   INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					   IN_LENGTH int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	int blockCount = noBytes / CAST_BLOCKSIZE;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, noBytes ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isIntegerRangeNZ( noBytes ) );

	while( blockCount-- > 0 )
		{
		/* Decrypt a block of data */
		CAST_ecb_encrypt( buffer, buffer, convInfo->key, CAST_DECRYPT );

		/* Move on to next block of data */
		buffer += CAST_BLOCKSIZE;
		}

	return( CRYPT_OK );
	}

/* Encrypt/decrypt data in CBC mode */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptCBC( INOUT CONTEXT_INFO *contextInfoPtr, 
					   INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					   IN_LENGTH int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, noBytes ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isIntegerRangeNZ( noBytes ) );

	CAST_cbc_encrypt( buffer, buffer, noBytes, convInfo->key,
					  convInfo->currentIV, CAST_ENCRYPT );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int decryptCBC( INOUT CONTEXT_INFO *contextInfoPtr, 
					   INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					   IN_LENGTH int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, noBytes ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isIntegerRangeNZ( noBytes ) );

	CAST_cbc_encrypt( buffer, buffer, noBytes, convInfo->key,
					  convInfo->currentIV, CAST_DECRYPT );

	return( CRYPT_OK );
	}

/* Encrypt/decrypt data in CFB mode */

#ifdef USE_CFB

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptCFB( INOUT CONTEXT_INFO *contextInfoPtr, 
					   INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					   IN_LENGTH int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	int i, ivCount = convInfo->ivCount, LOOP_ITERATOR;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, noBytes ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isIntegerRangeNZ( noBytes ) );

	/* If there's any encrypted material left in the IV, use it now */
	if( ivCount > 0 )
		{
		int bytesToUse;

		/* Find out how much material left in the encrypted IV we can use */
		bytesToUse = CAST_BLOCKSIZE - ivCount;
		if( noBytes < bytesToUse )
			bytesToUse = noBytes;

		/* Encrypt the data */
		LOOP_SMALL( i = 0, i < bytesToUse, i++ )
			buffer[ i ] ^= convInfo->currentIV[ i + ivCount ];
		ENSURES( LOOP_BOUND_OK );
		REQUIRES( rangeCheck( bytesToUse, 1, CRYPT_MAX_IVSIZE ) );
		memcpy( convInfo->currentIV + ivCount, buffer, bytesToUse );

		/* Adjust the byte count and buffer position */
		noBytes -= bytesToUse;
		buffer += bytesToUse;
		ivCount += bytesToUse;
		}

	while( noBytes > 0 )
		{
		ivCount = ( noBytes > CAST_BLOCKSIZE ) ? CAST_BLOCKSIZE : noBytes;

		/* Encrypt the IV */
		CAST_ecb_encrypt( convInfo->currentIV, convInfo->currentIV,
						  convInfo->key, CAST_ENCRYPT );

		/* XOR the buffer contents with the encrypted IV */
		LOOP_SMALL( i = 0, i < ivCount, i++ )
			buffer[ i ] ^= convInfo->currentIV[ i ];
		ENSURES( LOOP_BOUND_OK );

		/* Shift the ciphertext into the IV */
		REQUIRES( rangeCheck( ivCount, 1, CRYPT_MAX_IVSIZE ) );
		memcpy( convInfo->currentIV, buffer, ivCount );

		/* Move on to next block of data */
		noBytes -= ivCount;
		buffer += ivCount;
		}

	/* Remember how much of the IV is still available for use */
	convInfo->ivCount = ( ivCount % CAST_BLOCKSIZE );

	return( CRYPT_OK );
	}

/* Decrypt data in CFB mode.  Note that the transformation can be made
   faster (but less clear) with temp = buffer, buffer ^= iv, iv = temp
   all in one loop */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int decryptCFB( INOUT CONTEXT_INFO *contextInfoPtr, 
					   INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					   IN_LENGTH int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	BYTE temp[ CAST_BLOCKSIZE + 8 ];
	int i, ivCount = convInfo->ivCount, LOOP_ITERATOR;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, noBytes ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isIntegerRangeNZ( noBytes ) );

	/* If there's any encrypted material left in the IV, use it now */
	if( ivCount > 0 )
		{
		int bytesToUse;

		/* Find out how much material left in the encrypted IV we can use */
		bytesToUse = CAST_BLOCKSIZE - ivCount;
		if( noBytes < bytesToUse )
			bytesToUse = noBytes;

		/* Decrypt the data */
		REQUIRES( rangeCheck( bytesToUse, 1, CAST_BLOCKSIZE ) );
		memcpy( temp, buffer, bytesToUse );
		LOOP_SMALL( i = 0, i < bytesToUse, i++ )
			buffer[ i ] ^= convInfo->currentIV[ i + ivCount ];
		ENSURES( LOOP_BOUND_OK );
		REQUIRES( rangeCheck( bytesToUse, 1, CRYPT_MAX_IVSIZE ) );
		memcpy( convInfo->currentIV + ivCount, temp, bytesToUse );

		/* Adjust the byte count and buffer position */
		noBytes -= bytesToUse;
		buffer += bytesToUse;
		ivCount += bytesToUse;
		}

	while( noBytes > 0 )
		{
		ivCount = ( noBytes > CAST_BLOCKSIZE ) ? CAST_BLOCKSIZE : noBytes;

		/* Encrypt the IV */
		CAST_ecb_encrypt( convInfo->currentIV, convInfo->currentIV,
						  convInfo->key, CAST_ENCRYPT );

		/* Save the ciphertext */
		REQUIRES( rangeCheck( ivCount, 1, CAST_BLOCKSIZE ) );
		memcpy( temp, buffer, ivCount );

		/* XOR the buffer contents with the encrypted IV */
		LOOP_SMALL( i = 0, i < ivCount, i++ )
			buffer[ i ] ^= convInfo->currentIV[ i ];
		ENSURES( LOOP_BOUND_OK );

		/* Shift the ciphertext into the IV */
		REQUIRES( rangeCheck( ivCount, 1, CRYPT_MAX_IVSIZE ) );
		memcpy( convInfo->currentIV, temp, ivCount );

		/* Move on to next block of data */
		noBytes -= ivCount;
		buffer += ivCount;
		}

	/* Remember how much of the IV is still available for use */
	convInfo->ivCount = ( ivCount % CAST_BLOCKSIZE );

	/* Clear the temporary buffer */
	zeroise( temp, CAST_BLOCKSIZE );

	return( CRYPT_OK );
	}
#endif /* USE_CFB */

/****************************************************************************
*																			*
*							CAST Key Management Routines					*
*																			*
****************************************************************************/

/* Key schedule an CAST key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initKey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_BUFFER( keyLength ) const void *key, 
					IN_LENGTH_SHORT const int keyLength )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( keyLength >= MIN_KEYSIZE && keyLength <= CAST_KEY_LENGTH );

	/* Copy the key to internal storage */
	if( convInfo->userKey != key )
		{
		REQUIRES( rangeCheck( keyLength, 1, CRYPT_MAX_KEYSIZE ) );
		memcpy( convInfo->userKey, key, keyLength );
		convInfo->userKeyLength = keyLength;
		}

	CAST_set_key( convInfo->key, CAST_KEY_LENGTH, ( BYTE * ) key );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO capabilityInfo = {
	CRYPT_ALGO_CAST, bitsToBytes( 64 ), "CAST-128", 8,
	MIN_KEYSIZE, bitsToBytes( 128 ), bitsToBytes( 128 ),
	selfTest, getInfo, NULL, initGenericParams, initKey, NULL,
	encryptECB, decryptECB, encryptCBC, decryptCBC
#ifdef USE_CFB
	, encryptCFB, decryptCFB
#endif /* USE_CFB */
	};

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getCASTCapability( void )
	{
	return( &capabilityInfo );
	}

#endif /* USE_CAST */
