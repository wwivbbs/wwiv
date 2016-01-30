/****************************************************************************
*																			*
*					  cryptlib Randomness Internal Interface				*
*						Copyright Peter Gutmann 1995-2006					*
*																			*
****************************************************************************/

#ifndef _RANDOM_INT_DEFINED

#define _RANDOM_INT_DEFINED

#if defined( INC_ALL )
  #include "des.h"
  #include "random.h"
#else
  #include "crypt/des.h"
  #include "random/random.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*					Randomness Constants and Data Types						*
*																			*
****************************************************************************/

/* The maximum amount of random data needed by any cryptlib operation,
   equivalent to the size of a maximum-length PKC key.  However this isn't
   the absolute length because when generating the k value for DLP
   operations we get n + m bits and then reduce via one of the DLP
   parameters to get the value within range.  If we just got n bits this
   would introduce a bias into the top bit, see the DLP code for more
   details.  Because of this we allow a length slightly larger than the
   maximum PKC key size */

#define MAX_RANDOM_BYTES	( CRYPT_MAX_PKCSIZE + 8 )

/* The size in bytes of the randomness pool and the size of the X9.17
   post-processor generator pool */

#define RANDOMPOOL_SIZE			256
#define X917_POOLSIZE			8

/* The allocated size of the randomness pool, which allows for the overflow
   created by the fact that the hash function blocksize isn't any useful
   multiple of a power of 2 */

#define RANDOMPOOL_ALLOCSIZE	( ( ( RANDOMPOOL_SIZE + 20 - 1 ) / 20 ) * 20 )

/* The number of short samples of previous output that we keep for the FIPS
   140 continuous tests, and the number of retries that we perform if we
   detect a repeat of a previous output */

#define RANDOMPOOL_SAMPLES		16
#define RANDOMPOOL_RETRIES		5

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

/* The size of the X9.17 generator key, 112 bits for EDE 3DES, and the size 
   of the generator output, 64 bits */

#define X917_KEYSIZE	16
#define X917_BLOCKSIZE	X917_POOLSIZE

/* The scheduled DES keys for the X9.17 generator */

typedef struct {
	Key_schedule desKey1, desKey2, desKey3;
	} X917_3DES_KEY;

#define DES_KEYSIZE		sizeof( Key_schedule )

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
	X917_3DES_KEY x917Key;	/* Scheduled 3DES key */
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

#if 0	/* See comment in addEntropyQuality */
	/* Other status information used to check the pool's operation */
	int entropyByteCount;	/* Number of bytes entropy added */
#endif /* 0 */

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
void initRandomPool( INOUT RANDOM_INFO *randomInfo );
STDC_NONNULL_ARG( ( 1 ) ) \
void endRandomPool( INOUT RANDOM_INFO *randomInfo );

/* Prototypes for functions in rand_x917.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int setKeyX917( INOUT RANDOM_INFO *testRandomInfo, 
				IN_BUFFER_C( X917_KEYSIZE ) const BYTE *key,
				IN_BUFFER_C( X917_POOLSIZE ) const BYTE *state,
				IN_BUFFER_OPT_C( X917_POOLSIZE ) const BYTE *dateTime );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int generateX917( INOUT RANDOM_INFO *testRandomInfo, 
				  INOUT_BUFFER_FIXED( length ) BYTE *data,
				  IN_RANGE( 1, MAX_RANDOM_BYTES ) const int length );
CHECK_RETVAL \
int randomAlgorithmSelfTest( void );
CHECK_RETVAL \
int selfTestX917( INOUT RANDOM_INFO *randomInfo, 
				  IN_BUFFER_C( X917_KEYSIZE ) const BYTE *key );
CHECK_RETVAL \
int fipsTestX917( INOUT RANDOM_INFO *randomInfo );

#endif /* _RANDOM_INT_DEFINED */
