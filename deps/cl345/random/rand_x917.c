/****************************************************************************
*																			*
*						cryptlib X9.17 Generator Routines					*
*						Copyright Peter Gutmann 1995-2017					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "random_int.h"
#else
  #include "crypt.h"
  #include "random/random_int.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#ifndef CONFIG_CONSERVE_MEMORY_EXTRA

/* Sanity-check the X9.17 randomness state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckRandom( const RANDOM_INFO *randomInfo )
	{
	const BYTE *keyDataPtr;

	assert( isReadPtr( randomInfo, sizeof( RANDOM_INFO ) ) );

	/* Check safe pointers */
	if( !DATAPTR_ISSET( randomInfo->x917Key ) )
		{
		DEBUG_PUTS(( "sanityCheckRandom: Safe pointers" ));
		return( FALSE );
		}

	/* Check that the keying data is aligned to the requirements of any 
	   underlying hardware implementation */
	keyDataPtr = DATAPTR_GET( randomInfo->x917Key );
	if( keyDataPtr != ptr_align( keyDataPtr, 16 ) )
		{
		DEBUG_PUTS(( "sanityCheckRandom: X.917 key alignment" ));
		return( FALSE );
		}

	/* Make sure that the X9.17 generator accounting information is within
	   bounds.  See the comment in generateX917() for the high-range check */
	if( randomInfo->x917Count < 0 || \
		randomInfo->x917Count > X917_MAX_CYCLES + \
								( MAX_RANDOM_BYTES / X917_POOLSIZE ) )
		{
		DEBUG_PUTS(( "sanityCheckRandom: X9.17 count" ));
		return( FALSE );
		}

	return( TRUE );
	}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

/****************************************************************************
*																			*
*								ANSI X9.17 Generator						*
*																			*
****************************************************************************/

/* The ANSI X9.17 Annex C generator has a number of problems (besides just
   being slow) including a tiny internal state, use of fixed keys, no
   entropy update, revealing the internal state to an attacker whenever it
   generates output, and a horrible vulnerability to state compromise.  For
   FIPS 140 compliance however we need to use an approved generator (even
   though Annex C is informative rather than normative and contains only "an
   example of a pseudorandom key and IV generator" so that it could be argued
   that any generator based on X9.17 3DES is permitted), which is why this
   generator appears here.

   In order to minimise the potential for damage we employ it as a post-
   processor for the pool (since X9.17 produces a 1-1 mapping it can never
   make the output any worse), using as our timestamp input the main RNG
   output.  This is perfectly valid since X9.17 requires the use of DT, "a
   date/time vector which is updated on each key generation", a requirement
   which is met by the fastPoll() which is performed before the main pool is
   mixed.  The cryptlib representation of the date and time vector is as a
   hash of assorted incidental data and the date and time.  The fact that
   99.9999% of the value of the generator is coming from the, uhh, timestamp
   is as coincidental as the side effect of the engine cooling fan in the
   Brabham ground effect cars.

   Some eval labs may not like this use of DT, in which case it's also
   possible to inject the extra seed material into the generator by using
   the X9.31 interpretation of X9.17, which makes the V value an externally-
   modifiable value.  In this interpretation the "generator" has degenerated 
   to little more than a 3DES encryption of V, which can hardly have been 
   the intent of the X9.17 designers.  In other words the X9.17 operation:

	out = Enc( Enc( in ) ^ V(n) );
	V(n+1) = Enc( Enc( in ) ^ out );

   degenerates to:

	out = Enc( Enc( DT ) ^ in );

   since V is overwritten on each iteration.  If the eval lab requires this
   interpretation rather than the more sensible DT one then this can be
   enabled by supplying a dateTime value to setKeyX917(), although we don't 
   do it by default since it's so far removed from the real X9.17 
   generator */

#ifdef USE_3DES_X917

/* A macro to make what's being done by the generator easier to follow */

#define rngEncrypt( data, key ) \
		des_ecb3_encrypt( ( C_Block * ) ( data ), ( C_Block * ) ( data ), \
						  ( key )->desKey1, ( key )->desKey2, \
						  ( key )->desKey3, DES_ENCRYPT )

/* Set the X9.17 generator key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int setKeyX917( INOUT RANDOM_INFO *randomInfo, 
				IN_BUFFER_C( X917_KEYSIZE ) const BYTE *key,
				IN_BUFFER_C( X917_POOLSIZE ) const BYTE *state,
				IN_BUFFER_OPT_C( X917_POOLSIZE ) const BYTE *dateTime )
	{
	X917_KEY *des3Key = &randomInfo->x917Key;
	int desStatus;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isReadPtr( key, X917_KEYSIZE ) );
	assert( isReadPtr( state, X917_KEYSIZE ) );
	assert( dateTime == NULL || isReadPtr( dateTime, X917_KEYSIZE ) );

	/* Precondition: the key and seed aren't being taken from the same 
	   location */
	REQUIRES( sanityCheckRandom( randomInfo ) );
	REQUIRES( memcmp( key, state, X917_POOLSIZE ) );

	/* Remember that we're about to reset the generator state */
	randomInfo->x917Inited = FALSE;

	/* Schedule the DES keys.  Rather than performing the third key schedule
	   we just copy the first scheduled key into the third one, since it's 
	   the same key in EDE mode */
	des_set_odd_parity( ( C_Block * ) key );
	des_set_odd_parity( ( C_Block * ) ( key + bitsToBytes( 64 ) ) );
	desStatus = des_key_sched( ( des_cblock * ) key, des3Key->desKey1 );
	if( desStatus == 0 )
		{
		desStatus = des_key_sched( ( des_cblock * ) \
								   ( key + bitsToBytes( 64 ) ),
								   des3Key->desKey2 );
		}
	memcpy( des3Key->desKey3, des3Key->desKey1, DES_KEYSIZE );
	if( desStatus )
		{
		/* There was a problem initialising the keys, don't try and go any
		   further */
		ENSURES( randomInfo->x917Inited == FALSE );
		return( CRYPT_ERROR_RANDOM );
		}

	/* Set up the generator state value V(0) and DT if we're using the X9.31
	   interpretation */
	memcpy( randomInfo->x917Pool, state, X917_POOLSIZE );
	if( dateTime != NULL )
		{
		memcpy( randomInfo->x917DT, dateTime, X917_POOLSIZE );
		randomInfo->useX931 = TRUE;
		}

	/* We've initialised the generator and reset the cryptovariables, we're
	   ready to go */
	randomInfo->x917Inited = TRUE;
	randomInfo->x917Count = 0;

	ENSURES( sanityCheckRandom( randomInfo ) );

	return( CRYPT_OK );
	}
#else

/* Macros to make what's being done by the generator easier to follow */

#define AES_KEY		aes_encrypt_ctx
#define rngEncrypt( data, key ) \
		aes_ecb_encrypt( ( data ), ( data ), X917_BLOCKSIZE, ( key ) )

/* Set the X9.17 generator key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int setKeyX917( INOUT RANDOM_INFO *randomInfo, 
				IN_BUFFER_C( X917_KEYSIZE ) const BYTE *key,
				IN_BUFFER_C( X917_POOLSIZE ) const BYTE *state,
				IN_BUFFER_OPT_C( X917_POOLSIZE ) const BYTE *dateTime )
	{
	AES_KEY *aesKey = DATAPTR_GET( randomInfo->x917Key );
	int aesStatus;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isReadPtr( key, X917_KEYSIZE ) );
	assert( isReadPtr( state, X917_KEYSIZE ) );
	assert( dateTime == NULL || isReadPtr( dateTime, X917_KEYSIZE ) );

	/* Precondition: the key and seed aren't being taken from the same 
	   location */
	REQUIRES( sanityCheckRandom( randomInfo ) );
	REQUIRES( memcmp( key, state, X917_POOLSIZE ) );

	/* Remember that we're about to reset the generator state */
	randomInfo->x917Inited = FALSE;

	/* Schedule the AES key */
	aesStatus = aes_encrypt_key128( key, aesKey );
	if( aesStatus != EXIT_SUCCESS )
		{
		/* There was a problem initialising the keys, don't try and go any
		   further */
		ENSURES( randomInfo->x917Inited == FALSE );
		return( CRYPT_ERROR_RANDOM );
		}

	/* Set up the generator state value V(0) and DT if we're using the X9.31
	   interpretation */
	memcpy( randomInfo->x917Pool, state, X917_POOLSIZE );
	if( dateTime != NULL )
		{
		memcpy( randomInfo->x917DT, dateTime, X917_POOLSIZE );
		randomInfo->useX931 = TRUE;
		}

	/* We've initialised the generator and reset the cryptovariables, we're
	   ready to go */
	randomInfo->x917Inited = TRUE;
	randomInfo->x917Count = 0;

	ENSURES( sanityCheckRandom( randomInfo ) );

	return( CRYPT_OK );
	}
#endif /* USE_3DES_X917 */

/* Run the X9.17 generator over a block of data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int generateX917( INOUT RANDOM_INFO *randomInfo, 
				  INOUT_BUFFER_FIXED( length ) BYTE *data, 
				  IN_RANGE( 1, MAX_RANDOM_BYTES ) const int length )
	{
	AES_KEY *aesKey = DATAPTR_GET( randomInfo->x917Key );
	BYTE encTime[ X917_POOLSIZE + 8 ], *dataPtr = data;
	int dataBlockPos, LOOP_ITERATOR;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isReadPtrDynamic( data, length ) );

	/* Precondition: The generator has been initialised, we're not asking 
	   for more data than the maximum that should be needed, and the
	   cryptovariables aren't past their use-by date */
	REQUIRES( sanityCheckRandom( randomInfo ) );
	REQUIRES( randomInfo->x917Inited == TRUE );
	REQUIRES( length > 0 && length <= MAX_RANDOM_BYTES );
	REQUIRES( randomInfo->x917Count >= 0 && \
			  randomInfo->x917Count < X917_MAX_CYCLES );

	/* Process as many blocks of output as needed.  We can't check the
	   return value of the encryption call because there isn't one, however
	   the encryption code has gone through a self-test when the randomness
	   subsystem was initialised.  This can run the generator for slightly 
	   more than X917_MAX_CYCLES if we're already close to the limit before 
	   we start, but this isn't a big problem, it's only an approximate 
	   reset-count measure anyway */
	LOOP_LARGE( dataBlockPos = 0, dataBlockPos < length,
				dataBlockPos += X917_POOLSIZE )
		{
		const int bytesToCopy = min( length - dataBlockPos, X917_POOLSIZE );
		int i, LOOP_ITERATOR_ALT;
		ORIGINAL_INT_VAR( x917Count, randomInfo->x917Count );

		/* Precondition: We're processing from 1...X917_POOLSIZE bytes of
		   data */
		REQUIRES( bytesToCopy >= 1 && bytesToCopy <= X917_POOLSIZE );

		/* Set the seed from the user-supplied data.  This varies depending
		   on whether we're using the X9.17 or X9.31 interpretation of
		   seeding */
		if( randomInfo->useX931 )
			{
			/* It's the X9.31 interpretation, there's no further user seed
			   input apart from the V and DT that we set initially */
			memcpy( encTime, randomInfo->x917DT, X917_POOLSIZE );
			}
		else
			{
			/* It's the X9.17 seed-via-DT interpretation, the user input is
			   DT.  Copy in as much timestamp (+ other assorted data) as we
			   can into the DT value */
			REQUIRES( rangeCheck( bytesToCopy, 1, X917_POOLSIZE ) );
			memcpy( encTime, dataPtr, bytesToCopy );

			/* Inner precondition: The DT buffer contains the input data */
			FORALL( k, 0, bytesToCopy,
					encTime[ k ] == data[ dataBlockPos + k ] );
			}

		/* out = Enc( Enc( DT ) ^ V(n) ); */
		rngEncrypt( encTime, aesKey );
		LOOP_EXT_ALT( i = 0, i < X917_POOLSIZE, i++, X917_POOLSIZE + 1 )
			randomInfo->x917Pool[ i ] ^= encTime[ i ];
		ENSURES( LOOP_BOUND_OK_ALT );
		rngEncrypt( randomInfo->x917Pool, aesKey );
		REQUIRES( boundsCheckZ( dataBlockPos, bytesToCopy, length ) );
		memcpy( dataPtr, randomInfo->x917Pool, bytesToCopy );

		/* Postcondition: The internal state has been copied to the output
		   (ick) */
		FORALL( k, 0, bytesToCopy, \
				data[ dataBlockPos + k ] == randomInfo->x917Pool[ k ] );

		/* V(n+1) = Enc( Enc( DT ) ^ out ); */
		LOOP_EXT_ALT( i = 0, i < X917_POOLSIZE, i++, X917_POOLSIZE + 1 )
			randomInfo->x917Pool[ i ] ^= encTime[ i ];
		ENSURES( LOOP_BOUND_OK_ALT );
		rngEncrypt( randomInfo->x917Pool, aesKey );

		/* If we're using the X9.31 interpretation, update DT to meet the
		   monotonically increasing time value requirement.  Although the
		   spec doesn't explicitly state this, the published test vectors
		   increment the rightmost byte so the value is treated as big-
		   endian */
		if( randomInfo->useX931 )
			{
			ORIGINAL_INT_VAR( lsb1, randomInfo->x917DT[ X917_POOLSIZE - 1 ] );
			ORIGINAL_INT_VAR( lsb2, randomInfo->x917DT[ X917_POOLSIZE - 2 ] );
			ORIGINAL_INT_VAR( lsb3, randomInfo->x917DT[ X917_POOLSIZE - 3 ] );

			LOOP_EXT_ALT( i = X917_POOLSIZE - 1, i >= 0, i--, X917_POOLSIZE )
				{
				randomInfo->x917DT[ i ]++;
				if( randomInfo->x917DT[ i ] != 0 )
					break;
				}
			ENSURES( LOOP_BOUND_OK_ALT );

			/* Postcondition: The value has been incremented by one */
			ENSURES( ( randomInfo->x917DT[ X917_POOLSIZE - 1 ] == \
							ORIGINAL_VALUE( lsb1 ) + 1 ) || \
					 ( randomInfo->x917DT[ X917_POOLSIZE - 1 ] == 0 && \
					   randomInfo->x917DT[ X917_POOLSIZE - 2 ] == \
							ORIGINAL_VALUE( lsb2 ) + 1 ) || \
					 ( randomInfo->x917DT[ X917_POOLSIZE - 1 ] == 0 && \
					   randomInfo->x917DT[ X917_POOLSIZE - 2 ] == 0 && \
					   randomInfo->x917DT[ X917_POOLSIZE - 3 ] == \
							ORIGINAL_VALUE( lsb3 ) + 1 ) );
			}

		/* Move on to the next block */
		dataPtr += bytesToCopy;
		randomInfo->x917Count++;

		/* Postcondition: We've processed one more block of data */
		ENSURES( dataPtr == data + dataBlockPos + bytesToCopy );
		ENSURES( randomInfo->x917Count == ORIGINAL_VALUE( x917Count ) + 1 );
		}
	ENSURES( LOOP_BOUND_OK );

	/* Postcondition: We processed all of the data */
	ENSURES( dataPtr == data + length );

	zeroise( encTime, X917_POOLSIZE );

	/* Postcondition: Nulla vestigia retrorsum */
	FORALL( i, 0, X917_POOLSIZE,
			encTime[ i ] == 0 );

	ENSURES( sanityCheckRandom( randomInfo ) );

	return( CRYPT_OK );
	}

/* Initialise the X9.17 generator */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initX917( INOUT RANDOM_INFO *randomInfo )
	{
	void *keyDataPtr = &randomInfo->x917KeyData;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );

	/* The X9.17 RNG calls down into low-level internal code at a level
	   that bypasses the usual encryption context management.  Since the
	   underlying hardware implementation may impose alignment constraints
	   on the key storage, we have to manually align it here.  The alignment
	   value that we use is 16 bytes, required by some AES hardware */
#ifdef USE_3DES_X917
	static_assert( sizeof( X917_KEYDATA ) >= ( 3 * DES_KEYSIZE ) + 16,
				   "X.917 key storage" );
#else
	static_assert( sizeof( X917_KEYDATA ) >= AES_KEYSIZE + 16,
				   "X.917 key storage" );
#endif /* USE_3DES_X917 */
	keyDataPtr = ( void * ) roundUp( ( uintptr_t ) keyDataPtr, 16 );
	DATAPTR_SET( randomInfo->x917Key, keyDataPtr );

	ENSURES( sanityCheckRandom( randomInfo ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						X9.17 Generator Self-test Routines					*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* X9.17/X9.31 generator test vectors.  The first set of values used are
   from the NIST publication "The Random Number Generator Validation System
   (RNGVS)" (unfortunately the MCT values for this are wrong so they can't
   be used), the second set are from test data used by an eval lab, and the
   third set are the values used for cryptlib's FIPS evaluation */

#define RNG_TEST_3DES_NIST	0
#define RNG_TEST_INFOGARD	1
#define RNG_TEST_FIPSEVAL	2
#define RNG_TEST_AES_NIST	3

#ifdef USE_3DES_X917
  #define RNG_TEST_VALUES	RNG_TEST_INFOGARD
#else
  #define RNG_TEST_VALUES	RNG_TEST_AES_NIST
#endif /* USE_3DES_X917 */

#if ( RNG_TEST_VALUES == RNG_TEST_3DES_NIST )
  #define VST_ITERATIONS	5
#elif ( RNG_TEST_VALUES == RNG_TEST_INFOGARD )
  #define VST_ITERATIONS	64
#elif ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )
  #define VST_ITERATIONS	64
#elif ( RNG_TEST_VALUES == RNG_TEST_AES_NIST )
  #define VST_ITERATIONS	64
#endif /* VST iterations */

typedef struct {
	/* The values are declared with an extra byte of storage since they're
	   initialised from strings, which have an implicit '\0' at the end */
	const BYTE key[ X917_KEYSIZE + 1 ];
	const BYTE DT[ X917_BLOCKSIZE + 1 ], V[ X917_BLOCKSIZE + 1 ];
	const BYTE R[ X917_BLOCKSIZE + 1 ];
	} X917_MCT_TESTDATA;

typedef struct {
	const BYTE key[ X917_KEYSIZE + 1 ];
	const BYTE initDT[ X917_BLOCKSIZE + 1 ], initV[ X917_BLOCKSIZE + 1 ];
	const BYTE R[ VST_ITERATIONS ][ X917_BLOCKSIZE + 1 ];
	} X917_VST_TESTDATA;

static const X917_MCT_TESTDATA x917MCTdata = {	/* Monte Carlo Test */
#if ( RNG_TEST_VALUES == RNG_TEST_3DES_NIST )	/* These values are wrong */
	/* Key1 = 75C71AE5A11A232C
	   Key2 = 40256DCD94F767B0
	   DT = C89A1D888ED12F3C
	   V = D5538F9CF450F53C
	   R = 77C695C33E51C8C0 */
	"\x75\xC7\x1A\xE5\xA1\x1A\x23\x2C\x40\x25\x6D\xCD\x94\xF7\x67\xB0",
	"\xC8\x9A\x1D\x88\x8E\xD1\x2F\x3C",
	"\xD5\x53\x8F\x9C\xF4\x50\xF5\x3C",
	"\x77\xC6\x95\xC3\x3E\x51\xC8\xC0"
#elif ( RNG_TEST_VALUES == RNG_TEST_INFOGARD )
	/* Key1 = 625BB5131A45F492
	   Key2 = 70971C9E0D4C9792
	   DT = 5F328264B787B098
	   V = A24F6E0EE43204CD
	   R = C7AC1E8F100CC30A */
	"\x62\x5B\xB5\x13\x1A\x45\xF4\x92\x70\x97\x1C\x9E\x0D\x4C\x97\x92",
	"\x5F\x32\x82\x64\xB7\x87\xB0\x98",
	"\xA2\x4F\x6E\x0E\xE4\x32\x04\xCD",
	"\xC7\xAC\x1E\x8F\x10\x0C\xC3\x0A"
#elif ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )
	/* Key1 = A45BF2E50D153710
	   Key2 = 79832F38A89B2AB0
	   DT = 8219E01B2A6958BB
	   V = 283176BA23FA3181
	   R = ? */
	"\xA4\x5B\xF2\xE5\x0D\x15\x37\x10\x79\x83\x2F\x38\xA8\x9B\x2A\xB0",
	"\x82\x19\xE0\x1B\x2A\x69\x58\xBB",
	"\x28\x31\x76\xBA\x23\xFA\x31\x81",
	0
#elif ( RNG_TEST_VALUES == RNG_TEST_AES_NIST )
	/* Key = F7D36762B9915F1ED585EB8E91700EB2
	   DT = 259E67249288597A4D61E7C0E690AFAE
	   V = 35CC0EA481FC8A4F5F05C7D4667233B2
	   R = 26A6B3D33B8E7E68B75D9630EC036314 */
	"\xF7\xD3\x67\x62\xB9\x91\x5F\x1E\xD5\x85\xEB\x8E\x91\x70\x0E\xB2",
	"\x25\x9E\x67\x24\x92\x88\x59\x7A\x4D\x61\xE7\xC0\xE6\x90\xAF\xAE",
	"\x35\xCC\x0E\xA4\x81\xFC\x8A\x4F\x5F\x05\xC7\xD4\x66\x72\x33\xB2",
	"\x26\xA6\xB3\xD3\x3B\x8E\x7E\x68\xB7\x5D\x96\x30\xEC\x03\x63\x14"
#endif /* Different test vectors */
	};

static const X917_VST_TESTDATA x917VSTdata = {	/* Variable Seed Test (VST) */
#if ( RNG_TEST_VALUES == RNG_TEST_3DES_NIST )
	/* Count = 0
	   Key1 = 75C71AE5A11A232C
	   Key2 = 40256DCD94F767B0
	   DT = C89A1D888ED12F3C
	   V = 80000000000000000 */
	"\x75\xC7\x1A\xE5\xA1\x1A\x23\x2C\x40\x25\x6D\xCD\x94\xF7\x67\xB0",
	"\xC8\x9A\x1D\x88\x8E\xD1\x2F\x3C",
	"\x80\x00\x00\x00\x00\x00\x00\x00",
	  /* Count = 0, V = 8000000000000000, R = 944DC7210D6D7FD7 */
	{ "\x94\x4D\xC7\x21\x0D\x6D\x7F\xD7",
	  /* Count = 1, V = C000000000000000, R = AF1A648591BB7C2C */
	  "\xAF\x1A\x64\x85\x91\xBB\x7C\x2C",
	  /* Count = 2, V = E000000000000000, R = 221839B07451E423 */
	  "\x22\x18\x39\xB0\x74\x51\xE4\x23",
	  /* Count = 3, V = F000000000000000, R = EBA9271E04043712 */
	  "\xEB\xA9\x27\x1E\x04\x04\x37\x12",
	  /* Count = 4, V = F800000000000000, R = 02433C9417A3326F */
	  "\x02\x43\x3C\x94\x17\xA3\x32\x6F" }
#elif ( RNG_TEST_VALUES == RNG_TEST_INFOGARD )
	/* Count = 0
	   Key1 = 3164916EA2C87AAE
	   Key2 = 2ABC323EFB9802E3
	   DT = 65B9108277AC0582
	   V = 80000000000000000 */
	"\x31\x64\x91\x6E\xA2\xC8\x7A\xAE\x2A\xBC\x32\x3E\xFB\x98\x02\xE3",
	"\x65\xB9\x10\x82\x77\xAC\x05\x82",
	"\x80\x00\x00\x00\x00\x00\x00\x00",
	  /* Count = 0, V = 8000000000000000, R = D8015B966ADE69BA */
	{ "\xD8\x01\x5B\x96\x6A\xDE\x69\xBA",
	  /* Count = 1, V = C000000000000000, R = E737E18734365F43 */
	  "\xE7\x37\xE1\x87\x34\x36\x5F\x43",
	  /* Count = 2, V = E000000000000000, R = CA8F00C1DF28FCFF */
	  "\xCA\x8F\x00\xC1\xDF\x28\xFC\xFF",
	  /* Count = 3, V = F000000000000000, R = 9FF307027622FA2A */
	  "\x9F\xF3\x07\x02\x76\x22\xFA\x2A",
	  /* Count = 4, V = F800000000000000, R = 0A4BB2E54842648E */
	  "\x0A\x4B\xB2\xE5\x48\x42\x64\x8E",
	  /* Count = 5, V = FC00000000000000, R = FFAD84A57EE0DE37 */
	  "\xFF\xAD\x84\xA5\x7E\xE0\xDE\x37",
	  /* Count = 6, V = FE00000000000000, R = 0CF064313A7889FD */
	  "\x0C\xF0\x64\x31\x3A\x78\x89\xFD",
	  /* Count = 7, V = FF00000000000000, R = 97B6854447D95A01 */
	  "\x97\xB6\x85\x44\x47\xD9\x5A\x01",
	  /* Count = 8, V = ff80000000000000, R = 55272f900ae13948 */
	  "\x55\x27\x2F\x90\x0A\xE1\x39\x48",
	  /* Count = 9, V = ffc0000000000000, R = dbd731bdf9875a04 */
	  "\xDB\xD7\x31\xBD\xF9\x87\x5A\x04",
	  /* Count = 10, V = ffe0000000000000, R = b19589a371d4942d */
	  "\xB1\x95\x89\xA3\x71\xD4\x94\x2D",
	  /* Count = 11, V = fff0000000000000, R = 8da8f8e8c59fc497 */
	  "\x8D\xA8\xF8\xE8\xC5\x9F\xC4\x97",
	  /* Count = 12, V = fff8000000000000, R = ddfbf3f319bcda42 */
	  "\xDD\xFB\xF3\xF3\x19\xBC\xDA\x42",
	  /* Count = 13, V = fffc000000000000, R = a72ddd98d1744844 */
	  "\xA7\x2D\xDD\x98\xD1\x74\x48\x44",
	  /* Count = 14, V = fffe000000000000, R = de0835034456629e */
	  "\xDE\x08\x35\x03\x44\x56\x62\x9E",
	  /* Count = 15, V = ffff000000000000, R = e977daafef7aa5e0 */
	  "\xE9\x77\xDA\xAF\xEF\x7A\xA5\xE0",
	  /* Count = 16, V = ffff800000000000, R = 019c3edc5ae93ab8 */
	  "\x01\x9C\x3E\xDC\x5A\xE9\x3A\xB8",
	  /* Count = 17, V = ffffc00000000000, R = 163c3dbe31ffd91b */
	  "\x16\x3C\x3D\xBE\x31\xFF\xD9\x1B",
	  /* Count = 18, V = ffffe00000000000, R = f2045893945b4774 */
	  "\xF2\x04\x58\x93\x94\x5B\x47\x74",
	  /* Count = 19, V = fffff00000000000, R = 50c88799fc1ec55d */
	  "\x50\xC8\x87\x99\xFC\x1E\xC5\x5D",
	  /* Count = 20, V = fffff80000000000, R = 1545f463986e1511 */
	  "\x15\x45\xF4\x63\x98\x6E\x15\x11",
	  /* Count = 21, V = fffffc0000000000, R = 55f999624fe045a6 */
	  "\x55\xF9\x99\x62\x4F\xE0\x45\xA6",
	  /* Count = 22, V = fffffe0000000000, R = e3e0db844bca7505 */
	  "\xE3\xE0\xDB\x84\x4B\xCA\x75\x05",
	  /* Count = 23, V = ffffff0000000000, R = 8fb4b76d808562d7 */
	  "\x8F\xB4\xB7\x6D\x80\x85\x62\xD7",
	  /* Count = 24, V = ffffff8000000000, R = 9d5457baaeb496e4 */
	  "\x9D\x54\x57\xBA\xAE\xB4\x96\xE4",
	  /* Count = 25, V = ffffffc000000000, R = 2b8abff2bdc82366 */
	  "\x2B\x8A\xBF\xF2\xBD\xC8\x23\x66",
	  /* Count = 26, V = ffffffe000000000, R = 3936c324d09465af */
	  "\x39\x36\xC3\x24\xD0\x94\x65\xAF",
	  /* Count = 27, V = fffffff000000000, R = 1983dd227e55240e */
	  "\x19\x83\xDD\x22\x7E\x55\x24\x0E",
	  /* Count = 28, V = fffffff800000000, R = 866cf6e6dc3d03fb */
	  "\x86\x6C\xF6\xE6\xDC\x3D\x03\xFB",
	  /* Count = 29, V = fffffffc00000000, R = 03d10b0f17b04b59 */
	  "\x03\xD1\x0B\x0F\x17\xB0\x4B\x59",
	  /* Count = 30, V = fffffffe00000000, R = 3eeb1cd0248e25a6 */
	  "\x3E\xEB\x1C\xD0\x24\x8E\x25\xA6",
	  /* Count = 31, V = ffffffff00000000, R = 9d8bd4b8c3e425dc */
	  "\x9D\x8B\xD4\xB8\xC3\xE4\x25\xDC",
	  /* Count = 32, V = ffffffff80000000, R = bc515d3a0a719be1 */
	  "\xBC\x51\x5D\x3A\x0A\x71\x9B\xE1",
	  /* Count = 33, V = ffffffffc0000000, R = 1b35fb4aca4ac47c */
	  "\x1B\x35\xFB\x4A\xCA\x4A\xC4\x7C",
	  /* Count = 34, V = ffffffffe0000000, R = f8338668b6ead493 */
	  "\xF8\x33\x86\x68\xB6\xEA\xD4\x93",
	  /* Count = 35, V = fffffffff0000000, R = cdfa8e5ffa2deb17 */
	  "\xCD\xFA\x8E\x5F\xFA\x2D\xEB\x17",
	  /* Count = 36, V = fffffffff8000000, R = c965a35109044ca3 */
	  "\xC9\x65\xA3\x51\x09\x04\x4C\xA3",
	  /* Count = 37, V = fffffffffc000000, R = 8da70c88167b2746 */
	  "\x8D\xA7\x0C\x88\x16\x7B\x27\x46",
	  /* Count = 38, V = fffffffffe000000, R = 22ba92a21a74eb5b */
	  "\x22\xBA\x92\xA2\x1A\x74\xEB\x5B",
	  /* Count = 39, V = ffffffffff000000, R = 1fba0fab823a85e7 */
	  "\x1F\xBA\x0F\xAB\x82\x3A\x85\xE7",
	  /* Count = 40, V = ffffffffff800000, R = 656f4fc91245073d */
	  "\x65\x6F\x4F\xC9\x12\x45\x07\x3D",
	  /* Count = 41, V = ffffffffffc00000, R = a803441fb939f09c */
	  "\xA8\x03\x44\x1F\xB9\x39\xF0\x9C",
	  /* Count = 42, V = ffffffffffe00000, R = e3f30bb6aed64331 */
	  "\xE3\xF3\x0B\xB6\xAE\xD6\x43\x31",
	  /* Count = 43, V = fffffffffff00000, R = 6a75588b5e6f5ea4 */
	  "\x6A\x75\x58\x8B\x5E\x6F\x5E\xA4",
	  /* Count = 44, V = fffffffffff80000, R = ec95ad55ac684e93 */
	  "\xEC\x95\xAD\x55\xAC\x68\x4E\x93",
	  /* Count = 45, V = fffffffffffc0000, R = b2a79a0ebfb96c4e */
	  "\xB2\xA7\x9A\x0E\xBF\xB9\x6C\x4E",
	  /* Count = 46, V = fffffffffffe0000, R = 480263bb6146006f */
	  "\x48\x02\x63\xBB\x61\x46\x00\x6F",
	  /* Count = 47, V = ffffffffffff0000, R = c0d8b711395b290f */
	  "\xC0\xD8\xB7\x11\x39\x5B\x29\x0F",
	  /* Count = 48, V = ffffffffffff8000, R = a3f39193fe3d526d */
	  "\xA3\xF3\x91\x93\xFE\x3D\x52\x6D",
	  /* Count = 49, V = ffffffffffffc000, R = 6f50ba964d94d153 */
	  "\x6F\x50\xBA\x96\x4D\x94\xD1\x53",
	  /* Count = 50, V = ffffffffffffe000, R = ff8240a77c67bb8d */
	  "\xFF\x82\x40\xA7\x7C\x67\xBB\x8D",
	  /* Count = 51, V = fffffffffffff000, R = 7f95c72fd9b38ff6 */
	  "\x7F\x95\xC7\x2F\xD9\xB3\x8F\xF6",
	  /* Count = 52, V = fffffffffffff800, R = 7fbdf1428f44aac1 */
	  "\x7F\xBD\xF1\x42\x8F\x44\xAA\xC1",
	  /* Count = 53, V = fffffffffffffc00, R = 04cec286480ab97b */
	  "\x04\xCE\xC2\x86\x48\x0A\xB9\x7B",
	  /* Count = 54, V = fffffffffffffe00, R = 86562948c1cf8ec0 */
	  "\x86\x56\x29\x48\xC1\xCF\x8E\xC0",
	  /* Count = 55, V = ffffffffffffff00, R = b1a1c0f20c71b267 */
	  "\xB1\xA1\xC0\xF2\x0C\x71\xB2\x67",
	  /* Count = 56, V = ffffffffffffff80, R = f357a25c7dacbca8 */
	  "\xF3\x57\xA2\x5C\x7D\xAC\xBC\xA8",
	  /* Count = 57, V = ffffffffffffffc0, R = 8f8f4e0e348bf185 */
	  "\x8F\x8F\x4E\x0E\x34\x8B\xF1\x85",
	  /* Count = 58, V = ffffffffffffffe0, R = 52a21df35fa70190 */
	  "\x52\xA2\x1D\xF3\x5F\xA7\x01\x90",
	  /* Count = 59, V = fffffffffffffff0, R = 8be78733594af616 */
	  "\x8B\xE7\x87\x33\x59\x4A\xF6\x16",
	  /* Count = 60, V = fffffffffffffff8, R = e03a051b4ca826e5 */
	  "\xE0\x3A\x05\x1B\x4C\xA8\x26\xE5",
	  /* Count = 61, V = fffffffffffffffc, R = 5c4b73bb5901c3cf */
	  "\x5C\x4B\x73\xBB\x59\x01\xC3\xCF",
	  /* Count = 62, V = fffffffffffffffe, R = e5d7fc8415bfb0f0 */
	  "\xE5\xD7\xFC\x84\x15\xBF\xB0\xF0",
	  /* Count = 63, V = ffffffffffffffff, R = 9417d7247eaa5159 */
	  "\x94\x17\xD7\x24\x7E\xAA\x51\x59" }
#elif ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )
	/* COUNT = 0
	   Key1 = 3D3D0289DAEC867A
	   Key2 = 29B3F2C7F12C40E5
	   DT = 6FC8AE5CA678E042
	   V = 80000000000000000 */
	"\x3D\x3D\x02\x89\xDA\xEC\x86\x7A\x29\xB3\xF2\xC7\xF1\x2C\x40\xE5",
	"\x6F\xC8\xAE\x5C\xA6\x78\xE0\x42",
	"\x80\x00\x00\x00\x00\x00\x00\x00",
	{ 0 }
#elif ( RNG_TEST_VALUES == RNG_TEST_AES_NIST )
	/* Key = 7213395B28586FE64026056638110B3C
	   DT = 947529F603EDB0CF6927F65EDBBBC593
	   V = 80000000000000000000000000000000 */
	"\x72\x13\x39\x5B\x28\x58\x6F\xE6\x40\x26\x05\x66\x38\x11\x0B\x3C",
	"\x94\x75\x29\xF6\x03\xED\xB0\xCF\x69\x27\xF6\x5E\xDB\xBB\xC5\x93",
	"\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
	  /* Count = 0, V = 80000000000000000000000000000000, 
					R = 339CEF70DA546707B2944591890394A3 */
	{ "\x33\x9C\xEF\x70\xDA\x54\x67\x07\xB2\x94\x45\x91\x89\x03\x94\xA3",
	  /* Count = 1, V = C0000000000000000000000000000000
					R = CC96309772E727E71BAF70C361E626AE */
	  "\xCC\x96\x30\x97\x72\xE7\x27\xE7\x1B\xAF\x70\xC3\x61\xE6\x26\xAE",
	  /* Count = 2, V = E0000000000000000000000000000000
					R = 793B0B004CE8543B24D26BC76EF84C19 */
	  "\x79\x3B\x0B\x00\x4C\xE8\x54\x3B\x24\xD2\x6B\xC7\x6E\xF8\x4C\x19",
	  /* Count = 3, V = F0000000000000000000000000000000
					R = 00A6100F3AC3C0AFC7194D75863BB97D */
	  "\x00\xA6\x10\x0F\x3A\xC3\xC0\xAF\xC7\x19\x4D\x75\x86\x3B\xB9\x7D",
	  /* Count = 4, V = F8000000000000000000000000000000
					R = DB6DCD4CDAFFD704E4AC9BA46448771A */
	  "\xDB\x6D\xCD\x4C\xDA\xFF\xD7\x04\xE4\xAC\x9B\xA4\x64\x48\x77\x1A",
	  /* Count = 5, V = FC000000000000000000000000000000
					R = 29BB29E1CA7EFFE2807C674628AE97FF */
	  "\x29\xBB\x29\xE1\xCA\x7E\xFF\xE2\x80\x7C\x67\x46\x28\xAE\x97\xFF",
	  /* Count = 6, V = FE000000000000000000000000000000
					R = 67290EA5D230E13E73E9223929078BD9 */
	  "\x67\x29\x0E\xA5\xD2\x30\xE1\x3E\x73\xE9\x22\x39\x29\x07\x8B\xD9",
	  /* Count = 7, V = FF000000000000000000000000000000
					R = 00A4362875F0FF7E2E58C616CA22A961 */
	  "\x00\xA4\x36\x28\x75\xF0\xFF\x7E\x2E\x58\xC6\x16\xCA\x22\xA9\x61",
	  /* Count = 8, V = FF800000000000000000000000000000
					R = 1C689C09F84BEAA4785F7507CE99D909 */
	  "\x1C\x68\x9C\x09\xF8\x4B\xEA\xA4\x78\x5F\x75\x07\xCE\x99\xD9\x09",
	  /* Count = 9, V = FFC00000000000000000000000000000
					R = 5C83858DA3C8D53EBCE32FA44764A2C9 */
	  "\x5C\x83\x85\x8D\xA3\xC8\xD5\x3E\xBC\xE3\x2F\xA4\x47\x64\xA2\xC9",
	  /* Count = 10, V = FFE00000000000000000000000000000
					 R = 5E4F68F6D5BEB7C7855518B34E2BA2F6 */
	  "\x5E\x4F\x68\xF6\xD5\xBE\xB7\xC7\x85\x55\x18\xB3\x4E\x2B\xA2\xF6",
	  /* Count = 11, V = FFF00000000000000000000000000000
					 R = ED7FE3B42B8724CE68C070E61588D11A */
	  "\xED\x7F\xE3\xB4\x2B\x87\x24\xCE\x68\xC0\x70\xE6\x15\x88\xD1\x1A",
	  /* Count = 12, V = FFF80000000000000000000000000000
					 R = FFA4846475CB9F83261F0A04FD11368E */
	  "\xFF\xA4\x84\x64\x75\xCB\x9F\x83\x26\x1F\x0A\x04\xFD\x11\x36\x8E",
	  /* Count = 13, V = FFFC0000000000000000000000000000
					 R = 3125F56CE4C048A5B33803C8020C8E6C */
	  "\x31\x25\xF5\x6C\xE4\xC0\x48\xA5\xB3\x38\x03\xC8\x02\x0C\x8E\x6C",
	  /* Count = 14, V = FFFE0000000000000000000000000000
					 R = 7CC527BAA5B2B3ADD8E2198B326B8555 */
	  "\x7C\xC5\x27\xBA\xA5\xB2\xB3\xAD\xD8\xE2\x19\x8B\x32\x6B\x85\x55",
	  /* Count = 15, V = FFFF0000000000000000000000000000
					 R = 95810F62AEE87ACCE306B4FAFE30831B */
	  "\x95\x81\x0F\x62\xAE\xE8\x7A\xCC\xE3\x06\xB4\xFA\xFE\x30\x83\x1B",
	  /* Count = 16, V = FFFF8000000000000000000000000000
					 R = 69C8E966EE3EDAA1F78022D65F23D21A */
	  "\x69\xC8\xE9\x66\xEE\x3E\xDA\xA1\xF7\x80\x22\xD6\x5F\x23\xD2\x1A",
	  /* Count = 17, V = FFFFC000000000000000000000000000
					 R = 5B5A4B89F521B3BA43B2F1FB226DA412 */
	  "\x5B\x5A\x4B\x89\xF5\x21\xB3\xBA\x43\xB2\xF1\xFB\x22\x6D\xA4\x12",
	  /* Count = 18, V = FFFFE000000000000000000000000000
					 R = 669763CDAC9C776108BB0A3AC9E8717B */
	  "\x66\x97\x63\xCD\xAC\x9C\x77\x61\x08\xBB\x0A\x3A\xC9\xE8\x71\x7B",
	  /* Count = 19, V = FFFFF000000000000000000000000000
					 R = 7420DFD43C1E1CDDE2E97EDC02C1C88A */
	  "\x74\x20\xDF\xD4\x3C\x1E\x1C\xDD\xE2\xE9\x7E\xDC\x02\xC1\xC8\x8A",
	  /* Count = 20, V = FFFFF800000000000000000000000000
					 R = 0C1D2142B51720BDCFF11FF41CB573CD */
	  "\x0C\x1D\x21\x42\xB5\x17\x20\xBD\xCF\xF1\x1F\xF4\x1C\xB5\x73\xCD",
	  /* Count = 21, V = FFFFFC00000000000000000000000000
					 R = 05B089E217EFE8A06F25D8226F7075F8 */
	  "\x05\xB0\x89\xE2\x17\xEF\xE8\xA0\x6F\x25\xD8\x22\x6F\x70\x75\xF8",
	  /* Count = 22, V = FFFFFE00000000000000000000000000
					 R = 12F408B060A676019A430173E9236802 */
	  "\x12\xF4\x08\xB0\x60\xA6\x76\x01\x9A\x43\x01\x73\xE9\x23\x68\x02",
	  /* Count = 23, V = FFFFFF00000000000000000000000000
					 R = 84DECCB0A2E30E1DF4601F1EAB0A5498 */
	  "\x84\xDE\xCC\xB0\xA2\xE3\x0E\x1D\xF4\x60\x1F\x1E\xAB\x0A\x54\x98",
	  /* Count = 24, V = FFFFFF80000000000000000000000000
					 R = 4AF9217A6587BB66C72093F9FA52DFEF */
	  "\x4A\xF9\x21\x7A\x65\x87\xBB\x66\xC7\x20\x93\xF9\xFA\x52\xDF\xEF",
	  /* Count = 25, V = FFFFFFC0000000000000000000000000
					 R = 6C552CD4E79B887C7F8A82A204BB5BC3 */
	  "\x6C\x55\x2C\xD4\xE7\x9B\x88\x7C\x7F\x8A\x82\xA2\x04\xBB\x5B\xC3",
	  /* Count = 26, V = FFFFFFE0000000000000000000000000
					 R = 835E991A3FEF8A735574B3C8D6C18EE3 */
	  "\x83\x5E\x99\x1A\x3F\xEF\x8A\x73\x55\x74\xB3\xC8\xD6\xC1\x8E\xE3",
	  /* Count = 27, V = FFFFFFF0000000000000000000000000
					 R = 7D4A1BAD6049ED9F6105B54081E0A47B */
	  "\x7D\x4A\x1B\xAD\x60\x49\xED\x9F\x61\x05\xB5\x40\x81\xE0\xA4\x7B",
	  /* Count = 28, V = FFFFFFF8000000000000000000000000
					 R = E4B2E5FD7C6AE9C7009B358DBFBBC40E */
	  "\xE4\xB2\xE5\xFD\x7C\x6A\xE9\xC7\x00\x9B\x35\x8D\xBF\xBB\xC4\x0E",
	  /* Count = 29, V = FFFFFFFC000000000000000000000000
					 R = 601ED99FA91B1EE1E7B12E1B55CBBF39 */
	  "\x60\x1E\xD9\x9F\xA9\x1B\x1E\xE1\xE7\xB1\x2E\x1B\x55\xCB\xBF\x39",
	  /* Count = 30, V = FFFFFFFE000000000000000000000000
					 R = 8C9E1F94FE91B8BC3BF62CA875595199 */
	  "\x8C\x9E\x1F\x94\xFE\x91\xB8\xBC\x3B\xF6\x2C\xA8\x75\x59\x51\x99",
	  /* Count = 31, V = FFFFFFFF000000000000000000000000
					 R = BC7F2371D7ACE178EC967D3FD85FAB35 */
	  "\xBC\x7F\x23\x71\xD7\xAC\xE1\x78\xEC\x96\x7D\x3F\xD8\x5F\xAB\x35",
	  /* Count = 32, V = FFFFFFFF800000000000000000000000
					 R = 9E909514330BC5898CB6CE6C3A72798B */
	  "\x9E\x90\x95\x14\x33\x0B\xC5\x89\x8C\xB6\xCE\x6C\x3A\x72\x79\x8B",
	  /* Count = 33, V = FFFFFFFFC00000000000000000000000
					 R = 9E8D2E53EA2B457941A6344B01B9E623 */
	  "\x9E\x8D\x2E\x53\xEA\x2B\x45\x79\x41\xA6\x34\x4B\x01\xB9\xE6\x23",
	  /* Count = 34, V = FFFFFFFFE00000000000000000000000
					 R = 9FD7CF92C009BD7823AA4B098245FE07 */
	  "\x9F\xD7\xCF\x92\xC0\x09\xBD\x78\x23\xAA\x4B\x09\x82\x45\xFE\x07",
	  /* Count = 35, V = FFFFFFFFF00000000000000000000000
					 R = F9D462B9FC073EF766623ACC9E813D79 */
	  "\xF9\xD4\x62\xB9\xFC\x07\x3E\xF7\x66\x62\x3A\xCC\x9E\x81\x3D\x79",
 	  /* Count = 36, V = FFFFFFFFF80000000000000000000000
					 R = BC8B053176EF602EAB420C4C71C94B7D */
	  "\xBC\x8B\x05\x31\x76\xEF\x60\x2E\xAB\x42\x0C\x4C\x71\xC9\x4B\x7D",
	  /* Count = 37, V = FFFFFFFFFC0000000000000000000000
					 R = D914FF8866033081BE23C7A6357E88A1 */
	  "\xD9\x14\xFF\x88\x66\x03\x30\x81\xBE\x23\xC7\xA6\x35\x7E\x88\xA1",
	  /* Count = 38, V = FFFFFFFFFE0000000000000000000000
					 R = 056D2D88CECCF5EB1E14F6950E4F98CA */
	  "\x05\x6D\x2D\x88\xCE\xCC\xF5\xEB\x1E\x14\xF6\x95\x0E\x4F\x98\xCA",
	  /* Count = 39, V = FFFFFFFFFF0000000000000000000000
					 R = 580F8E925DCB03206A0832E3CF956D44 */
	  "\x58\x0F\x8E\x92\x5D\xCB\x03\x20\x6A\x08\x32\xE3\xCF\x95\x6D\x44",
	  /* Count = 40, V = FFFFFFFFFF8000000000000000000000
					 R = 1F6CB3420F701353B16464869FD9777B */
	  "\x1F\x6C\xB3\x42\x0F\x70\x13\x53\xB1\x64\x64\x86\x9F\xD9\x77\x7B",
	  /* Count = 41, V = FFFFFFFFFFC000000000000000000000
					 R = 003130793596F2B1E2D5BDE2B48AE312 */
	  "\x00\x31\x30\x79\x35\x96\xF2\xB1\xE2\xD5\xBD\xE2\xB4\x8A\xE3\x12",
	  /* Count = 42, V = FFFFFFFFFFE000000000000000000000
					 R = 267B9B7EF90A732F836DE5B95DC3C5AB */
	  "\x26\x7B\x9B\x7E\xF9\x0A\x73\x2F\x83\x6D\xE5\xB9\x5D\xC3\xC5\xAB",
	  /* Count = 43, V = FFFFFFFFFFF000000000000000000000
					 R = F41155BEE8AF3EE58E9DAE215F8CD565 */
	  "\xF4\x11\x55\xBE\xE8\xAF\x3E\xE5\x8E\x9D\xAE\x21\x5F\x8C\xD5\x65",
	  /* Count = 44, V = FFFFFFFFFFF800000000000000000000
					 R = E5EAC38320CE55F8A2AA8BA80D2A1814 */
	  "\xE5\xEA\xC3\x83\x20\xCE\x55\xF8\xA2\xAA\x8B\xA8\x0D\x2A\x18\x14",
	  /* Count = 45, V = FFFFFFFFFFFC00000000000000000000
					 R = 762D7889797C3DE5CC043E7A523D4355 */
	  "\x76\x2D\x78\x89\x79\x7C\x3D\xE5\xCC\x04\x3E\x7A\x52\x3D\x43\x55",
	  /* Count = 46, V = FFFFFFFFFFFE00000000000000000000
					 R = 07EABC9D857245EB77703C4C8A37A07A */
	  "\x07\xEA\xBC\x9D\x85\x72\x45\xEB\x77\x70\x3C\x4C\x8A\x37\xA0\x7A",
	  /* Count = 47, V = FFFFFFFFFFFF00000000000000000000
					 R = EE442D216C2BCA9056F2841A302BC7DB */
	  "\xEE\x44\x2D\x21\x6C\x2B\xCA\x90\x56\xF2\x84\x1A\x30\x2B\xC7\xDB",
	  /* Count = 48, V = FFFFFFFFFFFF80000000000000000000
					 R = 8C6D73BD41AF6120D542E0B96262C090 */
	  "\x8C\x6D\x73\xBD\x41\xAF\x61\x20\xD5\x42\xE0\xB9\x62\x62\xC0\x90",
	  /* Count = 49, V = FFFFFFFFFFFFC0000000000000000000
					 R = 6D6E205A2525666E46AE794096FF27E0 */
	  "\x6D\x6E\x20\x5A\x25\x25\x66\x6E\x46\xAE\x79\x40\x96\xFF\x27\xE0",
	  /* Count = 50, V = FFFFFFFFFFFFE0000000000000000000
					 R = EAF9559B192A779CD5381802A07BE6E9 */
	  "\xEA\xF9\x55\x9B\x19\x2A\x77\x9C\xD5\x38\x18\x02\xA0\x7B\xE6\xE9",
	  /* Count = 51, V = FFFFFFFFFFFFF0000000000000000000
					 R = 8C611A402887C46010E3CD979708B225 */
	  "\x8C\x61\x1A\x40\x28\x87\xC4\x60\x10\xE3\xCD\x97\x97\x08\xB2\x25",
	  /* Count = 52, V = FFFFFFFFFFFFF8000000000000000000
					 R = A83310B018994CBC81C7BC021F4D4258 */
	  "\xA8\x33\x10\xB0\x18\x99\x4C\xBC\x81\xC7\xBC\x02\x1F\x4D\x42\x58",
	  /* Count = 53, V = FFFFFFFFFFFFFC000000000000000000
					 R = 2BB0F59C40204AC4538B9857F90F89FB */
	  "\x2B\xB0\xF5\x9C\x40\x20\x4A\xC4\x53\x8B\x98\x57\xF9\x0F\x89\xFB",
	  /* Count = 54, V = FFFFFFFFFFFFFE000000000000000000
					 R = 1FD58C060D2AC2A47A054D9E61CEFE23 */
	  "\x1F\xD5\x8C\x06\x0D\x2A\xC2\xA4\x7A\x05\x4D\x9E\x61\xCE\xFE\x23",
	  /* Count = 55, V = FFFFFFFFFFFFFF000000000000000000
					 R = 8C609188F847C46AE07212FB7F72B237 */
	  "\x8C\x60\x91\x88\xF8\x47\xC4\x6A\xE0\x72\x12\xFB\x7F\x72\xB2\x37",
	  /* Count = 56, V = FFFFFFFFFFFFFF800000000000000000
					 R = 9C8377288804E9C72487A23CCA4F1847 */
	  "\x9C\x83\x77\x28\x88\x04\xE9\xC7\x24\x87\xA2\x3C\xCA\x4F\x18\x47",
	  /* Count = 57, V = FFFFFFFFFFFFFFC00000000000000000
					 R = 2CA0336293178A7B1E3EECB073722FD9 */
	  "\x2C\xA0\x33\x62\x93\x17\x8A\x7B\x1E\x3E\xEC\xB0\x73\x72\x2F\xD9",
	  /* Count = 58, V = FFFFFFFFFFFFFFE00000000000000000
					 R = 522FA2D0681B68C0409F14A7BBC10F35 */
	  "\x52\x2F\xA2\xD0\x68\x1B\x68\xC0\x40\x9F\x14\xA7\xBB\xC1\x0F\x35",
	  /* Count = 59, V = FFFFFFFFFFFFFFF00000000000000000
					 R = 68BD7966A4B327BA5CD7454B71473441 */
	  "\x68\xBD\x79\x66\xA4\xB3\x27\xBA\x5C\xD7\x45\x4B\x71\x47\x34\x41",
	  /* Count = 60, V = FFFFFFFFFFFFFFF80000000000000000
					 R = 67B5ED9CCC8849E20ED1339CF527CF31 */
	  "\x67\xB5\xED\x9C\xCC\x88\x49\xE2\x0E\xD1\x33\x9C\xF5\x27\xCF\x31",
	  /* Count = 61, V = FFFFFFFFFFFFFFFC0000000000000000
					 R = A58EA39069A3F84D5F5F30B3D42269F1 */
	  "\xA5\x8E\xA3\x90\x69\xA3\xF8\x4D\x5F\x5F\x30\xB3\xD4\x22\x69\xF1",
	  /* Count = 62, V = FFFFFFFFFFFFFFFE0000000000000000
					 R = 11EE2C950E9B942D5A2D6BF9A0CCE85C */
	  "\x11\xEE\x2C\x95\x0E\x9B\x94\x2D\x5A\x2D\x6B\xF9\xA0\xCC\xE8\x5C",
	  /* Count = 63, V = FFFFFFFFFFFFFFFF0000000000000000
					 R = A704BA2BD785A1D3F9E97F58E93D6A53 */
	  "\xA7\x04\xBA\x2B\xD7\x85\xA1\xD3\xF9\xE9\x7F\x58\xE9\x3D\x6A\x53"
	  /* The test vectors continue for another 64 values to cover all 128
	     bits of V, there's no obvious benefit to running through them all 
		 (just the first two values are enough, one to cover the RNG itself 
		 and the second to make sure the V update is working) so we stop at 
		 64 values */
	}
#endif /* Different test vectors */
	};

/* Helper functions to output the test data in the format required for the
   FIPS eval */

#if ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )

static void printVector( const char *description, const BYTE *data )
	{
	int i, LOOP_ITERATOR;

	printf( "%s = ", description );
	LOOP_SMALL( i = 0, i < 8, i++ )
		printf( "%02x", data[ i ] );
	ENSURES( LOOP_BOUND_OK );
	putchar( '\n' );
	}

static void printVectors( const BYTE *key, const BYTE *dt, const BYTE *v,
						  const BYTE *r, const int count )
	{
	printf( "COUNT = %d\n", count );
	printVector( "Key1", key );
	printVector( "Key2", key + 8 );
	printVector( "DT", dt );
	printVector( "V", v );
	printVector( "R", r );
	}
#endif /* FIPS eval data output */

/* Self-test code for the two crypto algorithms that are used for random
   number generation.  The self-test of these two algorithms is performed
   every time the randomness subsystem is initialised.  Note that the same
   tests have already been performed as part of the startup self-test but
   we perform them again here for the benefit of the randomness subsystem,
   which doesn't necessarily trust (or even know about) the startup self-
   test */

#if defined( INC_ALL )
  #include "capabil.h"
#else
  #include "device/capabil.h"
#endif /* Compiler-specific includes */

CHECK_RETVAL \
int randomAlgorithmSelfTest( void )
	{
	const CAPABILITY_INFO *capabilityInfo;
	int status;

	/* Test the hash algorithm functionality */
#ifdef USE_SHA1_PRNG
	capabilityInfo = getSHA1Capability();
#else
	capabilityInfo = getSHA2Capability();
#endif /* USE_SHA1_PRNG */
	status = capabilityInfo->selfTestFunction();
	if( cryptStatusError( status ) )
		return( status );

	/* Test the encryption algorithm functionality */
#ifdef USE_3DES_X917
	capabilityInfo = get3DESCapability();
#else
	capabilityInfo = getAESCapability();
#endif /* USE_3DES_X917 */
	status = capabilityInfo->selfTestFunction();
	if( cryptStatusError( status ) )
		return( status );

	return( CRYPT_OK );
	}

/* Test the X9.17 generator */

#if defined( USE_SHA1_PRNG )	/* SHA-1 + 3DES */
  #define PRNG_OUTPUT_STEP1	"\xF0\x8D\xD4\xDE\xFA\x2C\x80\x11"
  #define PRNG_OUTPUT_STEP2	"\xA0\xA9\x4E\xEC\xCD\xD9\x28\x7F"
  #define PRNG_OUTPUT_STEP3	"\x70\x82\x64\xED\x83\x88\x40\xE4"
#elif defined( USE_3DES_X917 )	/* SHA-2 + 3DES */
  #define PRNG_OUTPUT_STEP1	"\x7F\x93\x4E\x84\x3B\x79\xB3\x96"
  #define PRNG_OUTPUT_STEP2	"\x43\xD5\x6A\x6D\x97\x71\xA8\x12"
  #define PRNG_OUTPUT_STEP3	"\xAE\xEF\x82\x6F\xC1\x5B\x44\xAF"
#else							/* SHA-2 + AES */
  #define PRNG_OUTPUT_STEP1	"\x8A\xB2\x91\x01\x94\x33\x5C\x51\xB6\x44\x15\x42\xA7\x4B\x1D\xFE"
  #define PRNG_OUTPUT_STEP2	"\xDB\x5B\xB2\xA0\xEE\xCB\xA6\x4B\xEE\x85\x8C\xEA\xF7\xCA\x13\x74"
  #define PRNG_OUTPUT_STEP3	"\x02\x86\x91\x4C\xB8\x61\x2C\xA5\xA7\xEB\xD7\x34\x5F\x3C\x17\x38"
#endif /* SHA-1 vs. SHA-2 PRNG */

CHECK_RETVAL \
int selfTestX917( INOUT RANDOM_INFO *testRandomInfo, 
				  IN_BUFFER_C( X917_KEYSIZE ) const BYTE *key )
	{
	BYTE buffer[ X917_BLOCKSIZE + 8 ];
	int status;

	assert( isWritePtr( testRandomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isReadPtr( key, X917_KEYSIZE ) );

	/* Check that the ANSI X9.17 PRNG is working correctly */
	memset( buffer, 0, 16 );
	status = setKeyX917( testRandomInfo, key, key + X917_KEYSIZE, NULL );
	if( cryptStatusError( status ) )
		return( status );
	status = generateX917( testRandomInfo, buffer, X917_BLOCKSIZE );
	if( cryptStatusOK( status ) && \
		memcmp( buffer, PRNG_OUTPUT_STEP1, X917_BLOCKSIZE ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusOK( status ) )
		status = generateX917( testRandomInfo, buffer, X917_BLOCKSIZE );
	if( cryptStatusOK( status ) && \
		memcmp( buffer, PRNG_OUTPUT_STEP2, X917_BLOCKSIZE ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusOK( status ) )
		status = generateX917( testRandomInfo, buffer, X917_BLOCKSIZE );
	if( cryptStatusOK( status ) && \
		memcmp( buffer, PRNG_OUTPUT_STEP3, X917_BLOCKSIZE ) )
		status = CRYPT_ERROR_FAILED;
	
	return( status );
	}

#if !defined( CONFIG_SLOW_CPU )

CHECK_RETVAL \
static int fipsTest( INOUT RANDOM_INFO *testRandomInfo,
					 const BOOLEAN isX931 )
	{
	BYTE keyBuffer[ X917_KEYSIZE + 8 ],  buffer[ X917_BLOCKSIZE + 8 ];
	BYTE V[ X917_BLOCKSIZE + 8 ], DT[ X917_BLOCKSIZE + 8 ];
	int i, status, LOOP_ITERATOR;

	assert( isWritePtr( testRandomInfo, sizeof( RANDOM_INFO ) ) );

	REQUIRES( isX931 == TRUE || isX931 == FALSE );

	/* Run through the tests twice, once using the X9.17 interpretation and 
	   a second time using the X9.31 interpretation */
	memcpy( V, x917VSTdata.initV, X917_BLOCKSIZE );
	memcpy( DT, x917VSTdata.initDT, X917_BLOCKSIZE );
	LOOP_EXT( i = 0, i < VST_ITERATIONS, i++, VST_ITERATIONS + 1 )
		{
		int j, LOOP_ITERATOR_ALT;

		initRandomPool( testRandomInfo );
		memcpy( keyBuffer, x917VSTdata.key, X917_KEYSIZE );
		memcpy( buffer, DT, X917_BLOCKSIZE );
		status = setKeyX917( testRandomInfo, keyBuffer, V, \
							 isX931 ? DT : NULL );
		if( cryptStatusOK( status ) )
			status = generateX917( testRandomInfo, buffer, X917_BLOCKSIZE );
#if ( RNG_TEST_VALUES != RNG_TEST_FIPSEVAL )
		if( cryptStatusOK( status ) && \
			memcmp( buffer, x917VSTdata.R[ i ], X917_BLOCKSIZE ) )
			status = CRYPT_ERROR_FAILED;
#endif /* FIPS eval data output */
		endRandomPool( testRandomInfo );
		if( cryptStatusError( status ) )
			retIntError();
#if ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )
		if( isX931 )
			{
			printVectors( x917VSTdata.key, DT, V, buffer, i );
			putchar( '\n' );
			}
#endif /* FIPS eval data output */

		/* V = V >> 1, shifting in 1 bits; DT = DT + 1 */
		LOOP_EXT_ALT( j = X917_BLOCKSIZE - 1, j > 0, j--, 
					   X917_BLOCKSIZE )
			{
			if( V[ j - 1 ] & 1 )
				V[ j ] = intToByte( ( V[ j ] >> 1 ) | 0x80 );
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		V[ 0 ] = intToByte( ( V[ 0 ] >> 1 ) | 0x80 );
		LOOP_EXT_ALT( j = X917_BLOCKSIZE - 1, j >= 0, j--, 
					  X917_BLOCKSIZE )
			{
			DT[ j ]++;
			if( DT[ j ] != 0 )
				break;
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}
#endif /* Slower CPUs */

CHECK_RETVAL \
int fipsTestX917( INOUT RANDOM_INFO *testRandomInfo )
	{
	/* The following tests can take quite some time on slower CPUs because
	   they're iterated tests so we only run them if we can assume that 
	   there's a reasonably fast CPU present */
#if !defined( CONFIG_SLOW_CPU )
	BYTE keyBuffer[ X917_KEYSIZE + 8 ];
	BYTE buffer[ X917_BLOCKSIZE + 8 ];
	int i, status, LOOP_ITERATOR;

	assert( isWritePtr( testRandomInfo, sizeof( RANDOM_INFO ) ) );

	/* Check the ANSI X9.17 PRNG again, this time using X9.31 test vectors.
	   These aren't test vectors from X9.31 but vectors used to certify an 
	   X9.17 generator when run in X9.31 mode (we actually run the test 
	   twice, once in X9.17 seed-via-DT mode and once in X9.31 seed-via-V 
	   mode).  We have to do this after the above test since they're run as 
	   a linked series of tests going from the lowest-level cryptlib and 
	   ANSI PRNGs to the top-level overall random number generation system.  
	   Inserting this test in the middle would upset the final result 
	   values */
	initRandomPool( testRandomInfo );
	memcpy( keyBuffer, x917MCTdata.key, X917_KEYSIZE );
	memset( buffer, 0, X917_BLOCKSIZE );
	status = setKeyX917( testRandomInfo, keyBuffer, x917MCTdata.V,
						 x917MCTdata.DT );
	if( cryptStatusOK( status ) )
		{
		LOOP_EXT( i = 0, cryptStatusOK( status ) && i < 10000, i++, 10001 )
			{
			testRandomInfo->x917Count = 0;
			status = generateX917( testRandomInfo, buffer, X917_BLOCKSIZE );
			}
		ENSURES( LOOP_BOUND_OK );
		}
#if ( RNG_TEST_VALUES != RNG_TEST_FIPSEVAL )
	if( cryptStatusOK( status ) && \
		memcmp( buffer, x917MCTdata.R, X917_BLOCKSIZE ) )
		status = CRYPT_ERROR_FAILED;
#endif /* FIPS eval data output */
	if( cryptStatusError( status ) )
		retIntError();
	endRandomPool( testRandomInfo );
#if ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )
	printf( "[X9.31]\n[2-Key TDES]\n\n" );
	printVectors( x917MCTdata.key, x917MCTdata.DT, x917MCTdata.V, buffer, 0 );
	printf( "\n\n[X9.31]\n[2-Key TDES]\n\n" );
#endif /* FIPS eval data output */
	status = fipsTest( testRandomInfo, FALSE );
	if( cryptStatusOK( status ) )
		status = fipsTest( testRandomInfo, TRUE );
	if( cryptStatusError( status ) )
		return( status );
#if ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )
	/* Since this is run as a background thread we need to flush the output 
	   before the thread terminates, otherwise it'll be discarded */
	fflush( stdout );
#endif /* FIPS eval data output */
#endif /* Slower CPUs */

	return( CRYPT_OK );
	}
#endif /* CONFIG_NO_SELFTEST */
