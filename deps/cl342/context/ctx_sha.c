/****************************************************************************
*																			*
*							cryptlib SHA Hash Routines						*
*						Copyright Peter Gutmann 1992-2005					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "sha.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "crypt/sha.h"
#endif /* Compiler-specific includes */

#define HASH_STATE_SIZE		sizeof( SHA_CTX )

/****************************************************************************
*																			*
*								SHA Self-test Routines						*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* Test the SHA output against the test vectors given in FIPS 180-1.  We skip
   the third test since this takes several seconds to execute, which leads to
   an unacceptable delay */

static const struct {
	const char FAR_BSS *data;				/* Data to hash */
	const int length;						/* Length of data */
	const BYTE digest[ SHA_DIGEST_LENGTH ];	/* Digest of data */
	} FAR_BSS digestValues[] = {
	{ "abc", 3,
	  { 0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A,
		0xBA, 0x3E, 0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C,
		0x9C, 0xD0, 0xD8, 0x9D } },
	{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
	  { 0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E,
		0xBA, 0xAE, 0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5,
		0xE5, 0x46, 0x70, 0xF1 } },
/*	{ "aaaaa...", 1000000L,
	  { 0x34, 0xAA, 0x97, 0x3C, 0xD4, 0xC4, 0xDA, 0xA4,
		0xF6, 0x1E, 0xEB, 0x2B, 0xDB, 0xAD, 0x27, 0x31,
		0x65, 0x34, 0x01, 0x6F } }, */
	{ NULL, 0, { 0 } }
	};

static int selfTest( void )
	{
	const CAPABILITY_INFO *capabilityInfo = getSHA1Capability();
	BYTE hashState[ HASH_STATE_SIZE + 8 ];
	int i, status;

	/* Test SHA-1 against values given in FIPS 180-1 */
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
*								SHA Hash Routines							*
*																			*
****************************************************************************/

/* Hash data using SHA */

static int hash( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int noBytes )
	{
	SHA_CTX *shaInfo = ( SHA_CTX * ) contextInfoPtr->ctxHash->hashInfo;

	/* If the hash state was reset to allow another round of hashing,
	   reinitialise things */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_HASH_INITED ) )
		SHA1_Init( shaInfo );

	if( noBytes > 0 )
		{
		SHA1_Update( shaInfo, buffer, noBytes );
		}
	else
		SHA1_Final( contextInfoPtr->ctxHash->hash, shaInfo );

	return( CRYPT_OK );
	}

/* Internal API: Hash a single block of memory without the overhead of
   creating an encryption context.  This always uses SHA1 */

STDC_NONNULL_ARG( ( 1 ) ) \
void shaHashBuffer( INOUT_OPT HASHINFO hashInfo,
					OUT_BUFFER_OPT_C( outBufMaxLength, 20 ) BYTE *outBuffer,
					IN_LENGTH_SHORT_Z const int outBufMaxLength,
					IN_BUFFER_OPT( inLength ) const void *inBuffer,
					IN_LENGTH_SHORT_Z const int inLength,
					IN_ENUM( HASH_STATE ) const HASH_STATE hashState )
	{
	SHA_CTX *shaInfo = ( SHA_CTX * ) hashInfo;

	assert( isWritePtr( hashInfo, sizeof( HASHINFO ) ) );
	assert( ( hashState != HASH_STATE_END && \
			  outBuffer == NULL && outBufMaxLength == 0 ) || \
			( hashState == HASH_STATE_END && \
			  isWritePtr( outBuffer, outBufMaxLength ) && \
			  outBufMaxLength >= 20 ) );
	assert( inBuffer == NULL || isReadPtr( inBuffer, inLength ) );

	if( ( hashState == HASH_STATE_END && outBufMaxLength < 20 ) || \
		( hashState != HASH_STATE_END && inLength <= 0 ) )
		retIntError_Void();

	switch( hashState )
		{
		case HASH_STATE_START:
			SHA1_Init( shaInfo );
			/* Drop through */

		case HASH_STATE_CONTINUE:
			SHA1_Update( shaInfo, ( BYTE * ) inBuffer, inLength );
			break;

		case HASH_STATE_END:
			if( inBuffer != NULL )
				SHA1_Update( shaInfo, ( BYTE * ) inBuffer, inLength );
			SHA1_Final( outBuffer, shaInfo );
			break;

		default:
			retIntError_Void();
		}
	}

STDC_NONNULL_ARG( ( 1, 3 ) ) \
void shaHashBufferAtomic( OUT_BUFFER_C( outBufMaxLength, 20 ) BYTE *outBuffer,
						  IN_LENGTH_SHORT_MIN( 20 ) const int outBufMaxLength,
						  IN_BUFFER( inLength ) const void *inBuffer,
						  IN_LENGTH_SHORT const int inLength )
	{
	SHA_CTX shaInfo;

	assert( isWritePtr( outBuffer, outBufMaxLength ) && \
			outBufMaxLength >= 20 );
	assert( isReadPtr( inBuffer, inLength ) );

	if( outBufMaxLength < 20 || inLength <= 0 )
		retIntError_Void();

	SHA1_Init( &shaInfo );
	SHA1_Update( &shaInfo, ( BYTE * ) inBuffer, inLength );
	SHA1_Final( outBuffer, &shaInfo );
	zeroise( &shaInfo, sizeof( SHA_CTX ) );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO FAR_BSS capabilityInfo = {
	CRYPT_ALGO_SHA1, bitsToBytes( 160 ), "SHA-1", 5,
	bitsToBytes( 0 ), bitsToBytes( 0 ), bitsToBytes( 0 ),
	selfTest, getInfo, NULL, NULL, NULL, NULL, hash, hash
	};

const CAPABILITY_INFO *getSHA1Capability( void )
	{
	return( &capabilityInfo );
	}
