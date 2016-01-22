/****************************************************************************
*																			*
*						cryptlib HMAC-MD5 Hash Routines						*
*						Copyright Peter Gutmann 1997-2005					*
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

/* A structure to hold the initial and current MAC state info.  Rather than
   redoing the key processing each time when we're calculating multiple MACs
   with the same key, we just copy the initial state into the current state */

typedef struct {
	MD5_CTX macState, initialMacState;
	} MAC_STATE;

#define MAC_STATE_SIZE		sizeof( MAC_STATE )

/****************************************************************************
*																			*
*							HMAC-MD5 Self-test Routines						*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* Test the HMAC-MD5 output against the test vectors given in RFC 2104 and
   RFC ???? */

static const struct {
	const char FAR_BSS *key;				/* HMAC key */
	const int keyLength;					/* Length of key */
	const char FAR_BSS *data;				/* Data to hash */
	const int length;						/* Length of data */
	const BYTE digest[ MD5_DIGEST_LENGTH ];	/* Digest of data */
	} FAR_BSS hmacValues[] = {
	{ "\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B", 16,
	  "Hi There", 8,
	  { 0x92, 0x94, 0x72, 0x7A, 0x36, 0x38, 0xBB, 0x1C,
		0x13, 0xF4, 0x8E, 0xF8, 0x15, 0x8B, 0xFC, 0x9D } },
	{ "Jefe", 4,
		"what do ya want for nothing?", 28,
	  { 0x75, 0x0C, 0x78, 0x3E, 0x6A, 0xB0, 0xB5, 0x03,
		0xEA, 0xA8, 0x6E, 0x31, 0x0A, 0x5D, 0xB7, 0x38 } },
	{ "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA", 16,
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD", 50,
	  { 0x56, 0xBE, 0x34, 0x52, 0x1D, 0x14, 0x4C, 0x88,
		0xDB, 0xB8, 0xC7, 0x33, 0xF0, 0xE8, 0xB3, 0xF6 } },
	{ "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
	  "\x11\x12\x13\x14\x15\x16\x17\x18\x19", 25,
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD", 50,
	  { 0x69, 0x7E, 0xAF, 0x0A, 0xCA, 0x3A, 0x3A, 0xEA,
		0x3A, 0x75, 0x16, 0x47, 0x46, 0xFF, 0xAA, 0x79 } },
#if 0	/* Should be trunc.to 96 bits - we don't do truncation */
	{ "\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C", 16,
	  "Test With Truncation", 20,
	  { 0x56, 0x46, 0x1E, 0xF2, 0x34, 0x2E, 0xDC, 0x00,
		0xF9, 0xBA, 0xB9, 0x95, 0x69, 0x0E, 0xFD, 0x4C } },
#endif /* 0 */
	{ "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA", 80,
	  "Test Using Larger Than Block-Size Key - Hash Key First", 54,
	  { 0x6B, 0x1A, 0xB7, 0xFE, 0x4B, 0xD7, 0xBF, 0x8F,
		0x0B, 0x62, 0xE6, 0xCE, 0x61, 0xB9, 0xD0, 0xCD } },
	{ "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA", 80,
	  "Test Using Larger Than Block-Size Key and Larger Than One "
	  "Block-Size Data", 73,
	  { 0x6F, 0x63, 0x0F, 0xAD, 0x67, 0xCD, 0xA0, 0xEE,
		0x1F, 0xB1, 0xF5, 0x62, 0xDB, 0x3A, 0xA5, 0x3E } },
	{ "", 0, NULL, 0, { 0 } }
	};

static int selfTest( void )
	{
	const CAPABILITY_INFO *capabilityInfo = getHmacMD5Capability();
	BYTE macState[ MAC_STATE_SIZE + 8 ];
	int i, status;

	/* Test HMAC-MD5 against the test vectors given in RFC 2104 */
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
*							HMAC-MD5 Hash Routines							*
*																			*
****************************************************************************/

/* Hash data using HMAC-MD5 */

static int hash( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int noBytes )
	{
	MAC_INFO *macInfo = contextInfoPtr->ctxMAC;
	MD5_CTX *md5Info = &( ( MAC_STATE * ) macInfo->macInfo )->macState;

	/* If the hash state was reset to allow another round of MAC'ing, copy 
	   the initial MAC state over into the current MAC state */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_HASH_INITED ) )
		{
		MAC_STATE *macState = macInfo->macInfo;

		memcpy( &macState->macState, &macState->initialMacState, 
				sizeof( MD5_CTX ) );
		}

	if( noBytes > 0 )
		MD5_Update( md5Info, buffer, noBytes );
	else
		{
		BYTE hashBuffer[ MD5_CBLOCK + 8 ], digestBuffer[ MD5_CBLOCK + 8 ];
		int i;

		/* Complete the inner hash and extract the digest */
		MD5_Final( digestBuffer, md5Info );

		/* Perform the of the outer hash using the zero-padded key XOR'd
		   with the opad value followed by the digest from the inner hash */
		memset( hashBuffer, HMAC_OPAD, MD5_CBLOCK );
		memcpy( hashBuffer, macInfo->userKey,
				macInfo->userKeyLength );
		for( i = 0; i < macInfo->userKeyLength; i++ )
			hashBuffer[ i ] ^= HMAC_OPAD;
		MD5_Init( md5Info );
		MD5_Update( md5Info, hashBuffer, MD5_CBLOCK );
		memset( hashBuffer, 0, MD5_CBLOCK );
		MD5_Update( md5Info, digestBuffer, MD5_DIGEST_LENGTH );
		memset( digestBuffer, 0, MD5_DIGEST_LENGTH );
		MD5_Final( macInfo->mac, md5Info );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							HMAC-MD5 Key Management Routines				*
*																			*
****************************************************************************/

/* Set up an HMAC-MD5 key */

static int initKey( CONTEXT_INFO *contextInfoPtr, const void *key, 
					const int keyLength )
	{
	MAC_INFO *macInfo = contextInfoPtr->ctxMAC;
	MD5_CTX *md5Info = &( ( MAC_STATE * ) macInfo->macInfo )->macState;
	BYTE hashBuffer[ MD5_CBLOCK + 8 ];
	int i;

	MD5_Init( md5Info );

	/* If the key size is larger than the MD5 data size, reduce it to the MD5
	   hash size before processing it (yuck.  You're required to do this
	   though) */
	if( keyLength > MD5_CBLOCK )
		{
		/* Hash the user key down to the hash size (MD5_Init() has already
		   been called when the context was created) and use the hashed form
		   of the key */
		MD5_Update( md5Info, ( BYTE * ) key, keyLength );
		MD5_Final( macInfo->userKey, md5Info );
		macInfo->userKeyLength = MD5_DIGEST_LENGTH;

		/* Reset the MD5 state */
		MD5_Init( md5Info );
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
	memset( hashBuffer, HMAC_IPAD, MD5_CBLOCK );
	memcpy( hashBuffer, macInfo->userKey, 
			macInfo->userKeyLength );
	for( i = 0; i < macInfo->userKeyLength; i++ )
		hashBuffer[ i ] ^= HMAC_IPAD;
	MD5_Update( md5Info, hashBuffer, MD5_CBLOCK );
	memset( hashBuffer, 0, MD5_CBLOCK );
	contextInfoPtr->flags |= CONTEXT_FLAG_HASH_INITED;

	/* Save a copy of the initial state in case it's needed later */
	memcpy( &( ( MAC_STATE * ) macInfo->macInfo )->initialMacState, md5Info, 
			sizeof( MD5_CTX ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO FAR_BSS capabilityInfo = {
	CRYPT_ALGO_HMAC_MD5, bitsToBytes( 128 ), "HMAC-MD5", 8,
	bitsToBytes( 64 ), bitsToBytes( 128 ), CRYPT_MAX_KEYSIZE,
	selfTest, getInfo, NULL, NULL, initKey, NULL, hash, hash
	};

const CAPABILITY_INFO *getHmacMD5Capability( void )
	{
	return( &capabilityInfo );
	}

#endif /* USE_MD5 */
