/****************************************************************************
*																			*
*						cryptlib HMAC-SHA2 Hash Routines					*
*						Copyright Peter Gutmann 2004-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "sha2.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "crypt/sha2.h"
#endif /* Compiler-specific includes */

#ifdef USE_SHA2

/* A structure to hold the initial and current MAC state info.  Rather than
   redoing the key processing each time when we're calculating multiple MACs
   with the same key, we just copy the initial state into the current state */

typedef struct {
	sha2_ctx macState, initialMacState;
	} MAC_STATE;

#define MAC_STATE_SIZE		sizeof( MAC_STATE )

#ifndef SHA384_DIGEST_SIZE
  /* These may not be defined on non 64-bit systems */
  #define SHA384_DIGEST_SIZE			48
  #define SHA512_DIGEST_SIZE			64
  #define sha2_begin( size, ctx )		sha256_begin( ( ctx )->uu->ctx256 )
  #define sha2_hash( data, len, ctx )	sha256_hash( data, len, ( ctx )->uu->ctx256 )
  #define sha2_end( hash, ctx )			sha256_end( hash, ( ctx )->uu->ctx256 )
#endif /* SHA384_DIGEST_SIZE */

/****************************************************************************
*																			*
*							HMAC-SHA2 Self-test Routines					*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* Test the HMAC-SHA2 output against the test vectors given in RFC 4231 */

static const struct {
	const char FAR_BSS *key;				/* HMAC key */
	const int keyLength;					/* Length of key */
	const char FAR_BSS *data;				/* Data to hash */
	const int length;						/* Length of data */
	const BYTE digest[ SHA256_DIGEST_SIZE ];	/* Digest of data */
	} FAR_BSS hmacValues[] = {
	{ "\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B"
	  "\x0B\x0B\x0B\x0B", 20,
	  "Hi There", 8,
	  { 0xB0, 0x34, 0x4C, 0x61, 0xD8, 0xDB, 0x38, 0x53,
	    0x5C, 0xA8, 0xAF, 0xCE, 0xAF, 0x0B, 0xF1, 0x2B,
		0x88, 0x1D, 0xC2, 0x00, 0xC9, 0x83, 0x3D, 0xA7,
		0x26, 0xE9, 0x37, 0x6C, 0x2E, 0x32, 0xCF, 0xF7 } },
	{ "Jefe", 4,
		"what do ya want for nothing?", 28,
	  { 0x5B, 0xDC, 0xC1, 0x46, 0xBF, 0x60, 0x75, 0x4E,
	    0x6A, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xC7,
		0x5A, 0x00, 0x3F, 0x08, 0x9D, 0x27, 0x39, 0x83,
		0x9D, 0xEC, 0x58, 0xB9, 0x64, 0xEC, 0x38, 0x43 } },
	{ "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA", 20,
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD", 50,
	  { 0x77, 0x3E, 0xA9, 0x1E, 0x36, 0x80, 0x0E, 0x46,
	    0x85, 0x4D, 0xB8, 0xEB, 0xD0, 0x91, 0x81, 0xA7,
		0x29, 0x59, 0x09, 0x8B, 0x3E, 0xF8, 0xC1, 0x22,
		0xD9, 0x63, 0x55, 0x14, 0xCE, 0xD5, 0x65, 0xFE } },
	{ "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
	  "\x11\x12\x13\x14\x15\x16\x17\x18\x19", 25,
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD", 50,
	  { 0x82, 0x55, 0x8A, 0x38, 0x9A, 0x44, 0x3C, 0x0E,
	    0xA4, 0xCC, 0x81, 0x98, 0x99, 0xF2, 0x08, 0x3A,
		0x85, 0xF0, 0xFA, 0xA3, 0xE5, 0x78, 0xF8, 0x07,
		0x7A, 0x2E, 0x3F, 0xF4, 0x67, 0x29, 0x66, 0x5B } },
#if 0	/* Should be truncated to 128 bits - we don't do truncation */
	{ "\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C"
	  "\x0C\x0C\x0C\x0C", 20,
	  "Test With Truncation", 20,
	  { 0xA3, 0xB6, 0x16, 0x74, 0x73, 0x10, 0x0E, 0xE0,
	    0x6E, 0x0C, 0x79, 0x6C, 0x29, 0x55, 0x55, 0x2B } },
#endif /* 0 */
	{ "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA", 131,
	  "Test Using Larger Than Block-Size Key - Hash Key First", 54,
	  { 0x60, 0xE4, 0x31, 0x59, 0x1E, 0xE0, 0xB6, 0x7F,
	    0x0D, 0x8A, 0x26, 0xAA, 0xCB, 0xF5, 0xB7, 0x7F,
		0x8E, 0x0B, 0xC6, 0x21, 0x37, 0x28, 0xC5, 0x14,
		0x05, 0x46, 0x04, 0x0F, 0x0E, 0xE3, 0x7F, 0x54 } },

	{ "", 0, NULL, 0, { 0 } }
	};

static int selfTest( void )
	{
	const CAPABILITY_INFO *capabilityInfo = getHmacSHA2Capability();
	BYTE macState[ MAC_STATE_SIZE + 8 ];
	int i, status;

	/* Test HMAC-SHA2 against the test vectors given in RFC 4231 */
	for( i = 0; hmacValues[ i ].data != NULL; i++ )
		{
		status = testMAC( capabilityInfo, macState, hmacValues[ i ].key, 
						  hmacValues[ i ].keyLength, hmacValues[ i ].data, 
						  hmacValues[ i ].length, hmacValues[ i ].digest );
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

		*valuePtr = MAC_STATE_SIZE;

		return( CRYPT_OK );
		}

	return( getDefaultInfo( type, contextInfoPtr, data, length ) );
	}

/****************************************************************************
*																			*
*							HMAC-SHA2 Hash Routines							*
*																			*
****************************************************************************/

/* Hash data using HMAC-SHA2 */

static int hash( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int noBytes )
	{
	MAC_INFO *macInfo = contextInfoPtr->ctxMAC;
	sha2_ctx *shaInfo = &( ( MAC_STATE * ) macInfo->macInfo )->macState;

	/* If the hash state was reset to allow another round of MAC'ing, copy
	   the initial MAC state over into the current MAC state */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_HASH_INITED ) )
		{
		MAC_STATE *macState = macInfo->macInfo;

		memcpy( &macState->macState, &macState->initialMacState,
				sizeof( sha2_ctx ) );
		}

	if( noBytes > 0 )
		sha2_hash( buffer, noBytes, shaInfo );
	else
		{
		const int digestSize = contextInfoPtr->capabilityInfo->blockSize;
		BYTE hashBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
		BYTE digestBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
		int i;

		/* Complete the inner hash and extract the digest */
		sha2_end( digestBuffer, shaInfo );

		/* Perform the of the outer hash using the zero-padded key XOR'd
		   with the opad value followed by the digest from the inner hash */
		memset( hashBuffer, HMAC_OPAD, SHA256_BLOCK_SIZE );
		memcpy( hashBuffer, macInfo->userKey,
				macInfo->userKeyLength );
		for( i = 0; i < macInfo->userKeyLength; i++ )
			hashBuffer[ i ] ^= HMAC_OPAD;
		sha2_begin( digestSize, shaInfo );
		sha2_hash( hashBuffer, SHA256_BLOCK_SIZE, shaInfo );
		memset( hashBuffer, 0, SHA256_BLOCK_SIZE );
		sha2_hash( digestBuffer, digestSize, shaInfo );
		memset( digestBuffer, 0, digestSize );
		sha2_end( macInfo->mac, shaInfo );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						HMAC-SHA2 Key Management Routines					*
*																			*
****************************************************************************/

/* Set up an HMAC-SHA2 key */

static int initKey( CONTEXT_INFO *contextInfoPtr, const void *key,
					const int keyLength )
	{
	MAC_INFO *macInfo = contextInfoPtr->ctxMAC;
	sha2_ctx *shaInfo = &( ( MAC_STATE * ) macInfo->macInfo )->macState;
	BYTE hashBuffer[ SHA256_BLOCK_SIZE + 8 ];
	const int digestSize = contextInfoPtr->capabilityInfo->blockSize;
	int i;

	sha2_begin( digestSize, shaInfo );

	/* If the key size is larger than tha SHA2 data size, reduce it to the
	   SHA2 hash size before processing it (yuck.  You're required to do this
	   though) */
	if( keyLength > SHA256_BLOCK_SIZE )
		{
		/* Hash the user key down to the hash size (sha2_begin() has already
		   been called when the context was created) and use the hashed form
		   of the key */
		sha2_hash( ( void * ) key, keyLength, shaInfo );
		sha2_end( macInfo->userKey, shaInfo );
		macInfo->userKeyLength = digestSize;

		/* Reset the SHA2 state */
		sha2_begin( digestSize, shaInfo );
		}
	else
		{
		/* Copy the key to internal storage */
		if( macInfo->userKey != key )
			memcpy( macInfo->userKey, key, keyLength );
		macInfo->userKeyLength = keyLength;
		}

	/* Perform the start of the inner hash using the zero-padded key XOR'd
	   with the ipad value */
	memset( hashBuffer, HMAC_IPAD, SHA256_BLOCK_SIZE );
	memcpy( hashBuffer, macInfo->userKey,
			macInfo->userKeyLength );
	for( i = 0; i < macInfo->userKeyLength; i++ )
		hashBuffer[ i ] ^= HMAC_IPAD;
	sha2_hash( hashBuffer, SHA256_BLOCK_SIZE, shaInfo );
	memset( hashBuffer, 0, SHA256_BLOCK_SIZE );
	contextInfoPtr->flags |= CONTEXT_FLAG_HASH_INITED;

	/* Save a copy of the initial state in case it's needed later */
	memcpy( &( ( MAC_STATE * ) macInfo->macInfo )->initialMacState, shaInfo,
			sizeof( sha2_ctx ) );

	return( CRYPT_OK );
	}

/* Initialise algorithm parameters */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initParams( INOUT CONTEXT_INFO *contextInfoPtr, 
					   IN_ENUM( KEYPARAM ) const KEYPARAM_TYPE paramType,
					   IN_OPT const void *data, 
					   IN_INT const int dataLength )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_MAC );
	REQUIRES( paramType > KEYPARAM_NONE && paramType < KEYPARAM_LAST );

	/* SHA-2 has a variable-length output, selectable by setting the 
	   blocksize attribute */
	if( paramType == KEYPARAM_BLOCKSIZE )
		{
#ifdef USE_SHA2_EXT
  #ifdef CONFIG_SUITEB
		static const CAPABILITY_INFO FAR_BSS capabilityInfo = {
				CRYPT_ALGO_SHA2, bitsToBytes( 384 ), "HMAC-SHA384", 11,
				bitsToBytes( 64 ), bitsToBytes( 128 ), CRYPT_MAX_KEYSIZE,
				selfTest, getInfo, NULL, NULL, initKey, NULL, hash, hash
				};
  #else
		static const CAPABILITY_INFO FAR_BSS capabilityInfo = {
				CRYPT_ALGO_SHA2, bitsToBytes( 512 ), "HMAC-SHA512", 11,
				bitsToBytes( 64 ), bitsToBytes( 128 ), CRYPT_MAX_KEYSIZE,
				selfTest, getInfo, NULL, NULL, initKey, NULL, hash, hash
				};
  #endif /* CONFIG_SUITEB */
#endif /* USE_SHA2_EXT */

		/* The default SHA-2 variant is SHA-256, so an attempt to set this 
		   size is a no-op */
		if( dataLength == SHA256_DIGEST_SIZE )
			return( CRYPT_OK );

#ifdef USE_SHA2_EXT
		/* Switch to the appropriate variant of SHA-2.  Note that the 
		   initParamsFunction pointer for this version is NULL rather than
		   pointing to this function, so once the output size has been set 
		   it can't be changed again */
  #ifdef CONFIG_SUITEB
		if( dataLength != SHA384_DIGEST_SIZE )
			return( CRYPT_ARGERROR_NUM1 );
		contextInfoPtr->capabilityInfo = &capabilityInfo;
  #else
		if( dataLength != SHA512_DIGEST_SIZE )
			return( CRYPT_ARGERROR_NUM1 );
		contextInfoPtr->capabilityInfo = &capabilityInfo;
  #endif /* CONFIG_SUITEB */

		return( CRYPT_OK );
#else
		return( CRYPT_ARGERROR_NUM1 );
#endif /* USE_SHA2_EXT */
		}

	/* Pass the call on down to the global parameter-handling function */	
	return( initGenericParams( contextInfoPtr, paramType, data, 
							   dataLength ) );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO FAR_BSS capabilityInfo = {
	CRYPT_ALGO_HMAC_SHA2, bitsToBytes( 256 ), "HMAC-SHA2", 9,
	bitsToBytes( 64 ), bitsToBytes( 256 ), CRYPT_MAX_KEYSIZE,
	selfTest, getInfo, NULL, initParams, initKey, NULL, hash, hash
	};

const CAPABILITY_INFO *getHmacSHA2Capability( void )
	{
	return( &capabilityInfo );
	}

#endif /* USE_SHA2 */
