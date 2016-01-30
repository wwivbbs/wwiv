/****************************************************************************
*																			*
*						cryptlib HMAC-SHA Hash Routines						*
*						Copyright Peter Gutmann 1997-2005					*
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

/* A structure to hold the initial and current MAC state info.  Rather than
   redoing the key processing each time when we're calculating multiple MACs
   with the same key, we just copy the initial state into the current state */

typedef struct {
	SHA_CTX macState, initialMacState;
	} MAC_STATE;

#define MAC_STATE_SIZE		sizeof( MAC_STATE )

/****************************************************************************
*																			*
*							HMAC-SHA Self-test Routines						*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* Test the HMAC-SHA output against the test vectors given in RFC ???? */

static const struct {
	const char FAR_BSS *key;				/* HMAC key */
	const int keyLength;					/* Length of key */
	const char FAR_BSS *data;				/* Data to hash */
	const int length;						/* Length of data */
	const BYTE digest[ SHA_DIGEST_LENGTH ];	/* Digest of data */
	} FAR_BSS hmacValues[] = {
	{ "\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B"
	  "\x0B\x0B\x0B\x0B", 20,
	  "Hi There", 8,
	  { 0xB6, 0x17, 0x31, 0x86, 0x55, 0x05, 0x72, 0x64,
		0xE2, 0x8B, 0xC0, 0xB6, 0xFB, 0x37, 0x8C, 0x8E,
		0xF1, 0x46, 0xBE, 0x00 } },
	{ "Jefe", 4,
		"what do ya want for nothing?", 28,
	  { 0xEF, 0xFC, 0xDF, 0x6A, 0xE5, 0xEB, 0x2F, 0xA2,
		0xD2, 0x74, 0x16, 0xD5, 0xF1, 0x84, 0xDF, 0x9C,
		0x25, 0x9A, 0x7C, 0x79 } },
	{ "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
	  "\xAA\xAA\xAA\xAA", 20,
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
	  "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD", 50,
	  { 0x12, 0x5D, 0x73, 0x42, 0xB9, 0xAC, 0x11, 0xCD,
		0x91, 0xA3, 0x9A, 0xF4, 0x8A, 0xA1, 0x7B, 0x4F,
		0x63, 0xF1, 0x75, 0xD3 } },
	{ "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
	  "\x11\x12\x13\x14\x15\x16\x17\x18\x19", 25,
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	  "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD", 50,
	  { 0x4C, 0x90, 0x07, 0xF4, 0x02, 0x62, 0x50, 0xC6,
		0xBC, 0x84, 0x14, 0xF9, 0xBF, 0x50, 0xC8, 0x6C,
		0x2D, 0x72, 0x35, 0xDA } },
#if 0	/* Should be truncated to 96 bits - we don't do truncation */
	{ "\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C"
	  "\x0C\x0C\x0C\x0C", 20,
	  "Test With Truncation", 20,
	  { 0x4C, 0x1A, 0x03, 0x42, 0x4B, 0x55, 0xE0, 0x7F,
		0xE7, 0xF2, 0x7B, 0xE1, 0xD5, 0x8B, 0xB9, 0x32,
		0x4A, 0x9A, 0x5A, 0x04 } },
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
	  { 0xAA, 0x4A, 0xE5, 0xE1, 0x52, 0x72, 0xD0, 0x0E,
		0x95, 0x70, 0x56, 0x37, 0xCE, 0x8A, 0x3B, 0x55,
		0xED, 0x40, 0x21, 0x12 } },
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
	  { 0xE8, 0xE9, 0x9D, 0x0F, 0x45, 0x23, 0x7D, 0x78,
		0x6D, 0x6B, 0xBA, 0xA7, 0x96, 0x5C, 0x78, 0x08,
		0xBB, 0xFF, 0x1A, 0x91 } },
	{ "", 0, NULL, 0, { 0 } }
	};

static int selfTest( void )
	{
	const CAPABILITY_INFO *capabilityInfo = getHmacSHA1Capability();
	BYTE macState[ MAC_STATE_SIZE + 8 ];
	int i, status;

	/* Test HMAC-SHA against the test vectors given in RFC ???? */
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
*							HMAC-SHA Hash Routines							*
*																			*
****************************************************************************/

/* Hash data using HMAC-SHA */

static int hash( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int noBytes )
	{
	MAC_INFO *macInfo = contextInfoPtr->ctxMAC;
	SHA_CTX *shaInfo = &( ( MAC_STATE * ) macInfo->macInfo )->macState;

	/* If the hash state was reset to allow another round of MAC'ing, copy
	   the initial MAC state over into the current MAC state */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_HASH_INITED ) )
		{
		MAC_STATE *macState = macInfo->macInfo;

		memcpy( &macState->macState, &macState->initialMacState,
				sizeof( SHA_CTX ) );
		}

	if( noBytes > 0 )
		SHA1_Update( shaInfo, buffer, noBytes );
	else
		{
		BYTE hashBuffer[ SHA_CBLOCK + 8 ];
		BYTE digestBuffer[ SHA_DIGEST_LENGTH + 8 ];
		int i;

		/* Complete the inner hash and extract the digest */
		SHA1_Final( digestBuffer, shaInfo );

		/* Perform the of the outer hash using the zero-padded key XOR'd
		   with the opad value followed by the digest from the inner hash */
		memset( hashBuffer, HMAC_OPAD, SHA_CBLOCK );
		memcpy( hashBuffer, macInfo->userKey,
				macInfo->userKeyLength );
		for( i = 0; i < macInfo->userKeyLength; i++ )
			hashBuffer[ i ] ^= HMAC_OPAD;
		SHA1_Init( shaInfo );
		SHA1_Update( shaInfo, hashBuffer, SHA_CBLOCK );
		memset( hashBuffer, 0, SHA_CBLOCK );
		SHA1_Update( shaInfo, digestBuffer, SHA_DIGEST_LENGTH );
		memset( digestBuffer, 0, SHA_DIGEST_LENGTH );
		SHA1_Final( macInfo->mac, shaInfo );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							HMAC-SHA Key Management Routines				*
*																			*
****************************************************************************/

/* Set up an HMAC-SHA key */

static int initKey( CONTEXT_INFO *contextInfoPtr, const void *key,
					const int keyLength )
	{
	MAC_INFO *macInfo = contextInfoPtr->ctxMAC;
	SHA_CTX *shaInfo = &( ( MAC_STATE * ) macInfo->macInfo )->macState;
	BYTE hashBuffer[ SHA_CBLOCK + 8 ];
	int i;

	SHA1_Init( shaInfo );

	/* If the key size is larger than tha SHA data size, reduce it to the
	   SHA hash size before processing it (yuck.  You're required to do this
	   though) */
	if( keyLength > SHA_CBLOCK )
		{
		/* Hash the user key down to the hash size (SHA1_Init() has already
		   been called when the context was created) and use the hashed form
		   of the key */
		SHA1_Update( shaInfo, ( void * ) key, keyLength );
		SHA1_Final( macInfo->userKey, shaInfo );
		macInfo->userKeyLength = SHA_DIGEST_LENGTH;

		/* Reset the SHA state */
		SHA1_Init( shaInfo );
		}
	else
		{
		/* Copy the key to internal storage.  The memset() is unnecessary 
		   but used to produce more or less constant timing across 
		   different key sizes */
		if( macInfo->userKey != key )
			{
			memcpy( macInfo->userKey, key, keyLength );
			memset( macInfo->userKey + keyLength, 0,
					CRYPT_MAX_KEYSIZE - keyLength );
			}
		macInfo->userKeyLength = keyLength;
		}

	/* Perform the start of the inner hash using the zero-padded key XOR'd
	   with the ipad value.  We do this in a manner that tries to minimise 
	   timing information that may reveal the length of the password, given 
	   the amount of other stuff that's going on it's highly unlikely that 
	   this will ever ben an issue but we do it just in case */
	memcpy( hashBuffer, macInfo->userKey,
			macInfo->userKeyLength );
	if( macInfo->userKeyLength < SHA_CBLOCK )
		{
		memset( hashBuffer + macInfo->userKeyLength, 0, 
				SHA_CBLOCK - macInfo->userKeyLength );
		}
	for( i = 0; i < SHA_CBLOCK; i++ )
		hashBuffer[ i ] ^= HMAC_IPAD;
	SHA1_Update( shaInfo, hashBuffer, SHA_CBLOCK );
	memset( hashBuffer, 0, SHA_CBLOCK );
	contextInfoPtr->flags |= CONTEXT_FLAG_HASH_INITED;

	/* Save a copy of the initial state in case it's needed later */
	memcpy( &( ( MAC_STATE * ) macInfo->macInfo )->initialMacState, shaInfo,
			sizeof( SHA_CTX ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO FAR_BSS capabilityInfo = {
	CRYPT_ALGO_HMAC_SHA1, bitsToBytes( 160 ), "HMAC-SHA", 8,
	bitsToBytes( 64 ), bitsToBytes( 160 ), CRYPT_MAX_KEYSIZE,
	selfTest, getInfo, NULL, NULL, initKey, NULL, hash, hash
	};

const CAPABILITY_INFO *getHmacSHA1Capability( void )
	{
	return( &capabilityInfo );
	}
