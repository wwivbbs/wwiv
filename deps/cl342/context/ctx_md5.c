/****************************************************************************
*																			*
*							cryptlib MD5 Hash Routines						*
*						Copyright Peter Gutmann 1992-2005					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "md5.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "crypt/md5.h"
#endif /* Compiler-specific includes */

#ifdef USE_MD5

#define HASH_STATE_SIZE		sizeof( MD5_CTX )

/****************************************************************************
*																			*
*								MD5 Self-test Routines						*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* Test the MD5 output against the test vectors given in RFC 1321 */

static const struct {
	const char *data;						/* Data to hash */
	const int length;						/* Length of data */
	const BYTE digest[ MD5_DIGEST_LENGTH ];	/* Digest of data */
	} FAR_BSS digestValues[] = {
	{ NULL, 0,
	  { 0xD4, 0x1D, 0x8C, 0xD9, 0x8F, 0x00, 0xB2, 0x04,
		0xE9, 0x80, 0x09, 0x98, 0xEC, 0xF8, 0x42, 0x7E } },
	{ "a", 1,
	  { 0x0C, 0xC1, 0x75, 0xB9, 0xC0, 0xF1, 0xB6, 0xA8,
		0x31, 0xC3, 0x99, 0xE2, 0x69, 0x77, 0x26, 0x61 } },
	{ "abc", 3,
	  { 0x90, 0x01, 0x50, 0x98, 0x3C, 0xD2, 0x4F, 0xB0,
		0xD6, 0x96, 0x3F, 0x7D, 0x28, 0xE1, 0x7F, 0x72 } },
	{ "message digest", 14,
	  { 0xF9, 0x6B, 0x69, 0x7D, 0x7C, 0xB7, 0x93, 0x8D,
		0x52, 0x5A, 0x2F, 0x31, 0xAA, 0xF1, 0x61, 0xD0 } },
	{ "abcdefghijklmnopqrstuvwxyz", 26,
	  { 0xC3, 0xFC, 0xD3, 0xD7, 0x61, 0x92, 0xE4, 0x00,
		0x7D, 0xFB, 0x49, 0x6C, 0xCA, 0x67, 0xE1, 0x3B } },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 62,
	  { 0xD1, 0x74, 0xAB, 0x98, 0xD2, 0x77, 0xD9, 0xF5,
		0xA5, 0x61, 0x1C, 0x2C, 0x9F, 0x41, 0x9D, 0x9F } },
	{ "12345678901234567890123456789012345678901234567890123456789012345678901234567890", 80,
	  { 0x57, 0xED, 0xF4, 0xA2, 0x2B, 0xE3, 0xC9, 0x55,
		0xAC, 0x49, 0xDA, 0x2E, 0x21, 0x07, 0xB6, 0x7A } },
	{ NULL, 0, { 0 } }
	};

static int selfTest( void )
	{
	const CAPABILITY_INFO *capabilityInfo = getMD5Capability();
	BYTE hashState[ HASH_STATE_SIZE + 8 ];
	int i, status;

	/* Test MD5 against the test vectors given in RFC 1320 */
	for( i = 0; digestValues[ i ].data != NULL; i++ )
		{
		status = testHash( capabilityInfo, hashState, digestValues[ i ].data, 
						   digestValues[ i ].length, digestValues[ i ].digest );
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

		*valuePtr = HASH_STATE_SIZE;

		return( CRYPT_OK );
		}

	return( getDefaultInfo( type, contextInfoPtr, data, length ) );
	}

/****************************************************************************
*																			*
*								MD5 Hash Routines							*
*																			*
****************************************************************************/

/* Hash data using MD5 */

static int hash( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int noBytes )
	{
	MD5_CTX *md5Info = ( MD5_CTX * ) contextInfoPtr->ctxHash->hashInfo;

	/* If the hash state was reset to allow another round of hashing,
	   reinitialise things */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_HASH_INITED ) )
		MD5_Init( md5Info );

	if( noBytes > 0 )
		{
		MD5_Update( md5Info, buffer, noBytes );
		}
	else
		{
		MD5_Final( contextInfoPtr->ctxHash->hash, md5Info );
		}

	return( CRYPT_OK );
	}

/* Internal API: Hash a single block of memory without the overhead of
   creating an encryption context */

STDC_NONNULL_ARG( ( 1 ) ) \
void md5HashBuffer( INOUT_OPT HASHINFO hashInfo,
					OUT_BUFFER_OPT_C( outBufMaxLength, 16 ) BYTE *outBuffer,
					IN_LENGTH_SHORT_Z const int outBufMaxLength,
					IN_BUFFER_OPT( inLength ) const void *inBuffer,
					IN_LENGTH_SHORT_Z const int inLength,
					IN_ENUM( HASH_STATE ) const HASH_STATE hashState )
	{
	MD5_CTX *md5Info = ( MD5_CTX * ) hashInfo;

	assert( isWritePtr( hashInfo, sizeof( HASHINFO ) ) );
	assert( ( hashState != HASH_STATE_END && \
			  outBuffer == NULL && outBufMaxLength == 0 ) || \
			( hashState == HASH_STATE_END && \
			  isWritePtr( outBuffer, outBufMaxLength ) && \
			  outBufMaxLength >= 16 ) );
	assert( inBuffer == NULL || isReadPtr( inBuffer, inLength ) );

	if( ( hashState == HASH_STATE_END && outBufMaxLength < 16 ) || \
		( hashState != HASH_STATE_END && inLength <= 0 ) )
		retIntError_Void();

	switch( hashState )
		{
		case HASH_STATE_START:
			MD5_Init( md5Info );
			/* Drop through */

		case HASH_STATE_CONTINUE:
			MD5_Update( md5Info, ( BYTE * ) inBuffer, inLength );
			break;

		case HASH_STATE_END:
			if( inBuffer != NULL )
				MD5_Update( md5Info, ( BYTE * ) inBuffer, inLength );
			MD5_Final( outBuffer, md5Info );
			break;

		default:
			retIntError_Void();
		}
	}

STDC_NONNULL_ARG( ( 1, 3 ) ) \
void md5HashBufferAtomic( OUT_BUFFER_C( outBufMaxLength, 16 ) BYTE *outBuffer,
						  IN_LENGTH_SHORT_MIN( 16 ) const int outBufMaxLength,
						  IN_BUFFER( inLength ) const void *inBuffer,
						  IN_LENGTH_SHORT const int inLength )
	{
	MD5_CTX md5Info;

	assert( isWritePtr( outBuffer, outBufMaxLength ) && \
			outBufMaxLength >= 16 );
	assert( isReadPtr( inBuffer, inLength ) );

	if( outBufMaxLength < 16 || inLength <= 0 )
		retIntError_Void();

	MD5_Init( &md5Info );
	MD5_Update( &md5Info, ( BYTE * ) inBuffer, inLength );
	MD5_Final( outBuffer, &md5Info );
	zeroise( &md5Info, sizeof( MD5_CTX ) );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO FAR_BSS capabilityInfo = {
	CRYPT_ALGO_MD5, bitsToBytes( 128 ), "MD5", 3,
	bitsToBytes( 0 ), bitsToBytes( 0 ), bitsToBytes( 0 ),
	selfTest, getInfo, NULL, NULL, NULL, NULL, hash, hash
	};

const CAPABILITY_INFO *getMD5Capability( void )
	{
	return( &capabilityInfo );
	}

#endif /* USE_MD5 */
