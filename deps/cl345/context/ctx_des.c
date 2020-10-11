/****************************************************************************
*																			*
*						cryptlib DES Encryption Routines					*
*						Copyright Peter Gutmann 1995-2005					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "des.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "crypt/des.h"
#endif /* Compiler-specific includes */

#ifdef USE_DES

/* The DES block size */

#define DES_BLOCKSIZE			8

#if defined( INC_ALL )
  #include "testdes.h"
#else
  #include "crypt/testdes.h"
#endif /* Compiler-specific includes */

/* The scheduled DES key and size of the keyscheduled DES key */

#define DES_KEY					Key_schedule
#define DES_KEYSIZE				sizeof( Key_schedule )

/****************************************************************************
*																			*
*							DES Self-test Routines							*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* Test the DES implementation against the test vectors given in NBS Special
   Publication 500-20, 1980 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int testLoop( const DES_TEST *testDES, int iterations )
	{
	const CAPABILITY_INFO *capabilityInfo = getDESCapability();
	typedef BYTE KEY_DATA[ DES_KEYSIZE + 8 ];
	DECLARE_ALIGN_STRUCT( KEY_DATA_STORAGE, KEY_DATA );
	KEY_DATA_STORAGE keyDataStorage;
	KEY_DATA *keyDataPtr = ALIGN_STRUCT( &keyDataStorage, KEY_DATA, 8 ); 
	int i, status, LOOP_ITERATOR;

	LOOP_LARGE( i = 0, i < iterations, i++ )
		{
		/* The self-test uses weak keys, which means they'll be rejected by 
		   the key-load function if it checks for these.  For the OpenSSL
		   DES implementation we can kludge around this by temporarily 
		   clearing the global des_check_key value, but for other 
		   implementations some alternative workaround will be necessary */
		des_check_key = FALSE;
		status = testCipher( capabilityInfo, keyDataPtr, testDES[ i ].key, 
							 DES_BLOCKSIZE, testDES[ i ].plaintext,
							 testDES[ i ].ciphertext );
		des_check_key = TRUE;
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

CHECK_RETVAL \
static int selfTest( void )
	{
	/* Check the DES test vectors.  Note that we don't explicitly test
	   the RS values, however these are tested implicitly since they're
	   just the decrypt side of the KP tests */
	if( ( testLoop( testIP, sizeof( testIP ) / \
							sizeof( DES_TEST ) ) != CRYPT_OK ) || \
		( testLoop( testVP, sizeof( testVP ) / \
							sizeof( DES_TEST ) ) != CRYPT_OK ) || \
		( testLoop( testKP, sizeof( testKP ) / \
							sizeof( DES_TEST ) ) != CRYPT_OK ) || \
		( testLoop( testDP, sizeof( testDP ) / \
							sizeof( DES_TEST ) ) != CRYPT_OK ) || \
		( testLoop( testSB, sizeof( testSB ) / \
							sizeof( DES_TEST ) ) != CRYPT_OK ) )
		return( CRYPT_ERROR_FAILED );

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

		*valuePtr = DES_KEYSIZE;

		return( CRYPT_OK );
		}

	return( getDefaultInfo( type, contextInfoPtr, data, length ) );
	}

/****************************************************************************
*																			*
*							DES En/Decryption Routines						*
*																			*
****************************************************************************/

/* Encrypt/decrypt data in ECB mode */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptECB( INOUT CONTEXT_INFO *contextInfoPtr, 
					   INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					   IN_LENGTH int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	int blockCount = noBytes / DES_BLOCKSIZE;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, noBytes ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isIntegerRangeNZ( noBytes ) );

	while( blockCount-- > 0 )
		{
		/* Encrypt a block of data */
		des_ecb_encrypt( ( C_Block * ) buffer, ( C_Block * ) buffer, 
						 *( DES_KEY * ) convInfo->key, DES_ENCRYPT );

		/* Move on to next block of data */
		buffer += DES_BLOCKSIZE;
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int decryptECB( INOUT CONTEXT_INFO *contextInfoPtr, 
					   INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					   IN_LENGTH int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	int blockCount = noBytes / DES_BLOCKSIZE;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, noBytes ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isIntegerRangeNZ( noBytes ) );

	while( blockCount-- > 0 )
		{
		/* Decrypt a block of data */
		des_ecb_encrypt( ( C_Block * ) buffer, ( C_Block * ) buffer, 
						 *( DES_KEY * ) convInfo->key, DES_DECRYPT );

		/* Move on to next block of data */
		buffer += DES_BLOCKSIZE;
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

	des_ncbc_encrypt( buffer, buffer, noBytes, *( DES_KEY * ) convInfo->key,
					  ( C_Block * ) convInfo->currentIV, DES_ENCRYPT );

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

	des_ncbc_encrypt( buffer, buffer, noBytes, *( DES_KEY * ) convInfo->key,
					  ( C_Block * ) convInfo->currentIV, DES_DECRYPT );

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
		bytesToUse = DES_BLOCKSIZE - ivCount;
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
		ivCount = ( noBytes > DES_BLOCKSIZE ) ? DES_BLOCKSIZE : noBytes;

		/* Encrypt the IV */
		des_ecb_encrypt( ( C_Block * ) convInfo->currentIV,
						 ( C_Block * ) convInfo->currentIV,
						 *( DES_KEY * ) convInfo->key, DES_ENCRYPT );

		/* XOR the buffer contents with the encrypted IV */
		LOOP_MED( i = 0, i < ivCount, i++ )
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
	convInfo->ivCount = ( ivCount % DES_BLOCKSIZE );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int decryptCFB( INOUT CONTEXT_INFO *contextInfoPtr, 
					   INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					   IN_LENGTH int noBytes )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	BYTE temp[ DES_BLOCKSIZE + 8 ];
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
		bytesToUse = DES_BLOCKSIZE - ivCount;
		if( noBytes < bytesToUse )
			bytesToUse = noBytes;

		/* Decrypt the data */
		REQUIRES( rangeCheck( bytesToUse, 1, DES_BLOCKSIZE ) );
		memcpy( temp, buffer, bytesToUse );
		LOOP_MED( i = 0, i < bytesToUse, i++ )
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
		ivCount = ( noBytes > DES_BLOCKSIZE ) ? DES_BLOCKSIZE : noBytes;

		/* Encrypt the IV */
		des_ecb_encrypt( ( C_Block * ) convInfo->currentIV,
						 ( C_Block * ) convInfo->currentIV,
						 *( DES_KEY * ) convInfo->key, DES_ENCRYPT );

		/* Save the ciphertext */
		REQUIRES( rangeCheck( ivCount, 1, DES_BLOCKSIZE ) );
		memcpy( temp, buffer, ivCount );

		/* XOR the buffer contents with the encrypted IV */
		LOOP_MED( i = 0, i < ivCount, i++ )
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
	convInfo->ivCount = ( ivCount % DES_BLOCKSIZE );

	/* Clear the temporary buffer */
	zeroise( temp, DES_BLOCKSIZE );

	return( CRYPT_OK );
	}
#endif /* USE_CFB */

/****************************************************************************
*																			*
*							DES Key Management Routines						*
*																			*
****************************************************************************/

/* Key schedule a DES key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initKey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_BUFFER( keyLength ) const void *key, 
					IN_LENGTH_SHORT const int keyLength )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( keyLength >= MIN_KEYSIZE && keyLength <= DES_BLOCKSIZE );

	/* Copy the key to internal storage */
	if( convInfo->userKey != key )
		{
		REQUIRES( rangeCheck( keyLength, 1, CRYPT_MAX_KEYSIZE ) );
		memcpy( convInfo->userKey, key, keyLength );
		convInfo->userKeyLength = keyLength;
		}

	/* Call the libdes key schedule code.  Returns with -1 if the key parity
	   is wrong (which never occurs since we force the correct parity) or -2
	   if a weak key is used.  In theory this could leave us open to timing
	   attacks (a memcmp() implemented as a bytewise operation will exit on 
	   the first mis-matching byte), but in practice the trip through the
	   kernel adds enough skew to make the one or two clock cycle difference
	   undetectable */
	des_set_odd_parity( ( C_Block * ) convInfo->userKey );
	if( des_key_sched( ( C_Block * ) convInfo->userKey, 
					   *( DES_KEY * ) convInfo->key ) )
		return( CRYPT_ARGERROR_STR1 );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO capabilityInfo = {
	CRYPT_ALGO_DES, bitsToBytes( 64 ), "DES", 3,
	MIN_KEYSIZE, bitsToBytes( 64 ), bitsToBytes( 64 ),
	selfTest, getInfo, NULL, initGenericParams, initKey, NULL,
	encryptECB, decryptECB, encryptCBC, decryptCBC
#ifdef USE_CFB
	, encryptCFB, decryptCFB
#endif /* USE_CFB */
	};

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getDESCapability( void )
	{
	return( &capabilityInfo );
	}
#endif /* USE_DES */
