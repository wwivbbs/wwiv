/****************************************************************************
*																			*
*					  cryptlib Randomness Internal Interface				*
*						Copyright Peter Gutmann 1995-2015					*
*																			*
****************************************************************************/

#ifndef _RANDOM_INT_DEFINED

#define _RANDOM_INT_DEFINED

#if defined( INC_ALL )
  #ifdef USE_3DES_X917
	#include "des.h"
  #else
	#include "aes.h"
  #endif /* USE_3DES_X917 */
  #include "random.h"
#else
  #ifdef USE_3DES_X917
	#include "crypt/des.h"
  #else
	#include "crypt/aes.h"
  #endif /* USE_3DES_X917 */
  #include "random/random.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*					Randomness Constants and Data Types						*
*																			*
****************************************************************************/

/* The size in bytes of the randomness pool and the size of the X9.17
   post-processor generator pool */

#define RANDOMPOOL_SIZE			256
#ifdef USE_3DES_X917
  #define X917_POOLSIZE			8
#else
  #define X917_POOLSIZE			16
#endif /* USE_3DES_X917 */

/* cryptlib 2.00 to 3.42 use SHA-1 in the PRNG, cryptlib 3.43+ used SHA-2,
   which simplifies things somewhat since the 32-byte output is a multiple
   of the random pool size */

#ifdef USE_SHA1_PRNG
  /* The allocated size of the randomness pool, which allows for the 
     overflow created by the fact that the hash function blocksize, 20 bytes 
	 for SHA-1, isn't any useful multiple of a power of 2 */
  #define PRNG_ALGO				CRYPT_ALGO_SHA1
  #define RANDOMPOOL_ALLOCSIZE	( ( ( RANDOMPOOL_SIZE + 20 - 1 ) / 20 ) * 20 )
#else
  #define PRNG_ALGO				CRYPT_ALGO_SHA2
  #define RANDOMPOOL_ALLOCSIZE	RANDOMPOOL_SIZE
#endif /* SHA-1 vs. SHA-2 PRNG */

/* The number of short samples of previous output that we keep for the FIPS
   140 continuous tests, and the number of retries that we perform if we
   detect a repeat of a previous output */

#define RANDOMPOOL_SAMPLES		16
#define RANDOMPOOL_RETRIES		5

/* The size of the X9.17 generator key, 112/128 bits for 3DES/AES, and the 
   size of the generator output */

#define X917_KEYSIZE			16
#define X917_BLOCKSIZE			X917_POOLSIZE

/* The number of times that we cycle the X9.17 generator before we load new
   key and state variables.  This means that we re-seed for every
   X917_MAX_BYTES of output produced */

#define X917_MAX_BYTES			4096
#define X917_MAX_CYCLES			( X917_MAX_BYTES / X917_POOLSIZE )

/* In order to perform a FIPS 140-compliant check we have to signal a hard
   failure on the first repeat value rather than retrying the operation in
   case it's a one-off fault.  In order to avoid problems with false
   positives we take a larger-than-normal sample of RANDOMPOOL_SAMPLE_SIZE
   bytes for the first value, which we compare as a backup check if the
   standard short sample indicates a repeat */

#define RANDOMPOOL_SAMPLE_SIZE	16

/* The X9.17 generator can run with either the original 3DES algorithm 
   (cryptlib 2.0 to 3.43) or the more recent AES one (cryptlib 3.44+).

   Since there may be alignment requirements for underlying hardware crypto, 
   we have to allow for padding alongside the key storage, see the comment 
   in initX917() for details.  To deal with this we allocate X917_KEYDATA 
   storage and then set up an aligned pointer to the X917_KEY within the 
   X917_KEYDATA block */

#ifdef USE_3DES_X917

typedef struct {
	Key_schedule desKey1, desKey2, desKey3;
	} X917_KEY;
typedef struct {
	Key_schedule desKey1, desKey2, desKey3;
	BYTE padding[ 16 ];
	} X917_KEYDATA;
#define DES_KEYSIZE		sizeof( Key_schedule )

#else

typedef struct {
	aes_encrypt_ctx aesKey;
	} X917_KEY;
typedef struct {
	aes_encrypt_ctx aesKey;
	BYTE padding[ 16 ];
	} X917_KEYDATA;
#define AES_KEYSIZE		sizeof( aes_encrypt_ctx )

#endif /* USE_3DES_X917 */

/* Random pool information.  We keep track of the write position in the
   pool, which tracks where new data is added.  Whenever we add new data the
   write position is updated, once we reach the end of the pool we mix the
   pool and start again at the beginning.  We track the pool status by
   recording the quality of the pool contents (1-100) and the number of
   times the pool has been mixed, we can't draw data from the pool unless
   both of these values have reached an acceptable level.  In addition to
   the pool state information we keep track of the previous
   RANDOMPOOL_SAMPLES output samples to check for stuck-at faults or (short)
   cyles */

typedef struct {
	/* Pool state information */
	BUFFER( RANDOMPOOL_ALLOCSIZE, randomPoolPos ) \
	BYTE randomPool[ RANDOMPOOL_ALLOCSIZE + 8 ];/* Random byte pool */
	int randomPoolPos;		/* Current write position in the pool */

	/* Pool status information */
	int randomQuality;		/* Level of randomness in the pool */
	int randomPoolMixes;	/* Number of times pool has been mixed */

	/* X9.17 generator state information */
	BUFFER_FIXED( X917_POOLSIZE ) \
	BYTE x917Pool[ X917_POOLSIZE + 8 ];	/* Generator state */
	BUFFER_FIXED( X917_POOLSIZE ) \
	BYTE x917DT[ X917_POOLSIZE + 8 ];	/* Date/time vector */
	DATAPTR x917Key;		/* Generator encryption key */
	X917_KEYDATA x917KeyData;
	BOOLEAN x917Inited;		/* Whether generator has been inited */
	int x917Count;			/* No.of times generator has been cycled */
	BOOLEAN useX931;		/* X9.17 vs. X9.31 operation (see code comments */

	/* Information for the FIPS 140 continuous tests */
	ARRAY( RANDOMPOOL_SAMPLES, prevOutputIndex ) \
	unsigned long prevOutput[ RANDOMPOOL_SAMPLES + 2 ];
	ARRAY( RANDOMPOOL_SAMPLES, prevOutputIndex ) \
	unsigned long x917PrevOutput[ RANDOMPOOL_SAMPLES + 2 ];
	int prevOutputIndex;
	BUFFER_FIXED( RANDOMPOOL_SAMPLE_SIZE ) \
	BYTE x917OuputSample[ RANDOMPOOL_SAMPLE_SIZE + 8 ];

#if 0	/* See comment in addEntropyQuality() */
	/* Other status information used to check the pool's operation */
	int entropyByteCount;	/* Number of bytes entropy added */
#endif /* 0 */

	/* Pool integrity-protection checksum.  This is set to zero before the
	   entire RANDOM_INFO structure is checksummed */
	int checksum;			/* Integrity-protection checksum */

	/* Random seed data information if seeding is done from a stored seed */
#ifdef CONFIG_RANDSEED
	BOOLEAN seedProcessed;	/* Whether stored seed has been processed */
	int seedSize;			/* Size of stored seed data */
	int seedUpdateCount;	/* When to update stored seed data */
#endif /* CONFIG_RANDSEED */
	} RANDOM_INFO;

/****************************************************************************
*																			*
*					Randomness Internal Interface Functions					*
*																			*
****************************************************************************/

/* Prototypes for functions in random.c */

STDC_NONNULL_ARG( ( 1 ) ) \
void initRandomPool( OUT RANDOM_INFO *randomInfo );
STDC_NONNULL_ARG( ( 1 ) ) \
void endRandomPool( INOUT RANDOM_INFO *randomInfo );

/* Get storage for the randomness information.  This is normally allocated 
   in non-pageable memory, but for embedded systems it's part of the 
   statically-allocated system storage */

#ifdef USE_EMBEDDED_OS
CHECK_RETVAL_PTR_NONNULL \
void *getRandomInfoStorage( void );
#endif /* USE_EMBEDDED_OS */

/* Prototypes for functions in rand_x917.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int setKeyX917( INOUT RANDOM_INFO *randomInfo, 
				IN_BUFFER_C( X917_KEYSIZE ) const BYTE *key,
				IN_BUFFER_C( X917_POOLSIZE ) const BYTE *state,
				IN_BUFFER_OPT_C( X917_POOLSIZE ) const BYTE *dateTime );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int generateX917( INOUT RANDOM_INFO *randomInfo, 
				  INOUT_BUFFER_FIXED( length ) BYTE *data,
				  IN_RANGE( 1, MAX_RANDOM_BYTES ) const int length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initX917( INOUT RANDOM_INFO *randomInfo );
#ifndef CONFIG_NO_SELFTEST_
CHECK_RETVAL \
int randomAlgorithmSelfTest( void );
CHECK_RETVAL \
int selfTestX917( INOUT RANDOM_INFO *randomInfo, 
				  IN_BUFFER_C( X917_KEYSIZE ) const BYTE *key );
CHECK_RETVAL \
int fipsTestX917( INOUT RANDOM_INFO *randomInfo );
#else
  #define randomAlgorithmSelfTest()			CRYPT_OK
  #define selfTestX917( randomInfo, key )	CRYPT_OK
  #define fipsTestX917( randomInfo )		CRYPT_OK
#endif /* !CONFIG_NO_SELFTEST */
#endif /* _RANDOM_INT_DEFINED */
