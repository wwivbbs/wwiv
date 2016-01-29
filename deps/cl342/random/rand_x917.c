/****************************************************************************
*																			*
*						cryptlib X9.17 Generator Routines					*
*						Copyright Peter Gutmann 1995-2007					*
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

/* Sanity-check the X9.17 randomness state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheck( const RANDOM_INFO *randomInfo )
	{
	assert( isReadPtr( randomInfo, sizeof( RANDOM_INFO ) ) );

	/* Make sure that the X9.17 generator accounting information is within
	   bounds.  See the comment in generateX917() for the high-range check */
	if( randomInfo->x917Count < 0 || \
		randomInfo->x917Count > X917_MAX_CYCLES + \
								( MAX_RANDOM_BYTES / X917_POOLSIZE ) )
		return( FALSE );

	return( TRUE );
	}

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

/* A macro to make what's being done by the generator easier to follow */

#define tdesEncrypt( data, key ) \
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
	X917_3DES_KEY *des3Key = &randomInfo->x917Key;
	int desStatus;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isReadPtr( key, X917_KEYSIZE ) );
	assert( isReadPtr( state, X917_KEYSIZE ) );
	assert( dateTime == NULL || isReadPtr( dateTime, X917_KEYSIZE ) );

	/* Precondition: the key and seed aren't being taken from the same 
	   location */
	REQUIRES( sanityCheck( randomInfo ) );
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

	ENSURES( sanityCheck( randomInfo ) );

	return( CRYPT_OK );
	}

/* Run the X9.17 generator over a block of data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int generateX917( INOUT RANDOM_INFO *randomInfo, 
				  INOUT_BUFFER_FIXED( length ) BYTE *data, 
				  IN_RANGE( 1, MAX_RANDOM_BYTES ) const int length )
	{
	BYTE encTime[ X917_POOLSIZE + 8 ], *dataPtr = data;
	int dataBlockPos;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isReadPtr( data, length ) );

	/* Precondition: The generator has been initialised, we're not asking 
	   for more data than the maximum that should be needed, and the
	   cryptovariables aren't past their use-by date */
	REQUIRES( sanityCheck( randomInfo ) );
	REQUIRES( randomInfo->x917Inited == TRUE );
	REQUIRES( length > 0 && length <= MAX_RANDOM_BYTES );
	REQUIRES( randomInfo->x917Count >= 0 && \
			  randomInfo->x917Count < X917_MAX_CYCLES );

	/* Process as many blocks of output as needed.  We can't check the
	   return value of the encryption call because there isn't one, however
	   the 3DES code has gone through a self-test when the randomness
	   subsystem was initialised.  This can run the generator for slightly 
	   more than X917_MAX_CYCLES if we're already close to the limit before 
	   we start, but this isn't a big problem, it's only an approximate 
	   reset-count measure anyway */
	for( dataBlockPos = 0; dataBlockPos < length;
		 dataBlockPos += X917_POOLSIZE )
		{
		const int bytesToCopy = min( length - dataBlockPos, X917_POOLSIZE );
		int i;
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
			memcpy( encTime, dataPtr, bytesToCopy );

			/* Inner precondition: The DT buffer contains the input data */
			FORALL( k, 0, bytesToCopy,
					encTime[ k ] == data[ dataBlockPos + k ] );
			}

		/* out = Enc( Enc( DT ) ^ V(n) ); */
		tdesEncrypt( encTime, &randomInfo->x917Key );
		for( i = 0; i < X917_POOLSIZE; i++ )
			randomInfo->x917Pool[ i ] ^= encTime[ i ];
		tdesEncrypt( randomInfo->x917Pool, &randomInfo->x917Key );
		memcpy( dataPtr, randomInfo->x917Pool, bytesToCopy );

		/* Postcondition: The internal state has been copied to the output
		   (ick) */
		FORALL( k, 0, bytesToCopy, \
				data[ dataBlockPos + k ] == randomInfo->x917Pool[ k ] );

		/* V(n+1) = Enc( Enc( DT ) ^ out ); */
		for( i = 0; i < X917_POOLSIZE; i++ )
			randomInfo->x917Pool[ i ] ^= encTime[ i ];
		tdesEncrypt( randomInfo->x917Pool, &randomInfo->x917Key );

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

			for( i = X917_POOLSIZE - 1; i >= 0; i-- )
				{
				randomInfo->x917DT[ i ]++;
				if( randomInfo->x917DT[ i ] != 0 )
					break;
				}

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

	/* Postcondition: We processed all of the data */
	ENSURES( dataPtr == data + length );

	zeroise( encTime, X917_POOLSIZE );

	/* Postcondition: Nulla vestigia retrorsum */
	FORALL( i, 0, X917_POOLSIZE,
			encTime[ i ] == 0 );

	ENSURES( sanityCheck( randomInfo ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						X9.17 Generator Self-test Routines					*
*																			*
****************************************************************************/

/* X9.17/X9.31 generator test vectors.  The first set of values used are
   from the NIST publication "The Random Number Generator Validation System
   (RNGVS)" (unfortunately the MCT values for this are wrong so they can't
   be used), the second set are from test data used by an eval lab, and the
   third set are the values used for cryptlib's FIPS evaluation */

#define RNG_TEST_NIST		0
#define RNG_TEST_INFOGARD	1
#define RNG_TEST_FIPSEVAL	2

#define RNG_TEST_VALUES		RNG_TEST_INFOGARD

#if ( RNG_TEST_VALUES == RNG_TEST_NIST )
  #define VST_ITERATIONS	5
#elif ( RNG_TEST_VALUES == RNG_TEST_INFOGARD )
  #define VST_ITERATIONS	64
#elif ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )
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

static const X917_MCT_TESTDATA FAR_BSS x917MCTdata = {	/* Monte Carlo Test */
#if ( RNG_TEST_VALUES == RNG_TEST_NIST )	/* These values are wrong */
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
#endif /* Different test vectors */
	};

static const X917_VST_TESTDATA FAR_BSS x917VSTdata = {	/* Variable Seed Test (VST) */
#if ( RNG_TEST_VALUES == RNG_TEST_NIST )
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
#endif /* Different test vectors */
	};

/* Helper functions to output the test data in the format required for the
   FIPS eval */

#if ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )

static void printVector( const char *description, const BYTE *data )
	{
	int i;

	printf( "%s = ", description );
	for( i = 0; i < 8; i++ )
		printf( "%02x", data[ i ] );
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

	/* Test the SHA-1 functionality */
	capabilityInfo = getSHA1Capability();
	status = capabilityInfo->selfTestFunction();
	if( cryptStatusError( status ) )
		return( status );

	/* Test the 3DES (and DES) functionality */
	capabilityInfo = get3DESCapability();
	status = capabilityInfo->selfTestFunction();
	if( cryptStatusError( status ) )
		return( status );

	return( CRYPT_OK );
	}

/* Test the X9.17 generator */

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
		memcmp( buffer, "\xF0\x8D\xD4\xDE\xFA\x2C\x80\x11", X917_BLOCKSIZE ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusOK( status ) )
		status = generateX917( testRandomInfo, buffer, X917_BLOCKSIZE );
	if( cryptStatusOK( status ) && \
		memcmp( buffer, "\xA0\xA9\x4E\xEC\xCD\xD9\x28\x7F", X917_BLOCKSIZE ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusOK( status ) )
		status = generateX917( testRandomInfo, buffer, X917_BLOCKSIZE );
	if( cryptStatusOK( status ) && \
		memcmp( buffer, "\x70\x82\x64\xED\x83\x88\x40\xE4", X917_BLOCKSIZE ) )
		status = CRYPT_ERROR_FAILED;
	
	return( status );
	}

CHECK_RETVAL \
int fipsTestX917( INOUT RANDOM_INFO *testRandomInfo )
	{
	/* The following tests can take quite some time on slower CPUs because
	   they're iterated tests so we only run them if we can assume that 
	   there's a reasonably fast CPU present */
#if !defined( CONFIG_SLOW_CPU )
	BYTE keyBuffer[ X917_KEYSIZE + 8 ];
	BYTE buffer[ X917_BLOCKSIZE + 8 ];
	int i, isX931, status;

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
		for( i = 0; cryptStatusOK( status ) && i < 10000; i++ )
			{
			testRandomInfo->x917Count = 0;
			status = generateX917( testRandomInfo, buffer, X917_BLOCKSIZE );
			}
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
	for( isX931 = FALSE; isX931 <= TRUE; isX931++ )
		{
		BYTE V[ X917_BLOCKSIZE + 8 ], DT[ X917_BLOCKSIZE + 8 ];

		/* Run through the tests twice, once using the X9.17 interpretation 
		   and a second time using the X9.31 interpretation */
		memcpy( V, x917VSTdata.initV, X917_BLOCKSIZE );
		memcpy( DT, x917VSTdata.initDT, X917_BLOCKSIZE );
		for( i = 0; i < VST_ITERATIONS; i++ )
			{
			int j;

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

			/* V = V >> 1, shifting in 1 bits;
			   DT = DT + 1 */
			for( j = X917_BLOCKSIZE - 1; j > 0; j-- )
				{
				if( V[ j - 1 ] & 1 )
					V[ j ] = ( V[ j ] >> 1 ) | 0x80;
				}
			V[ 0 ] = ( V[ 0 ] >> 1 ) | 0x80;
			for( j = X917_BLOCKSIZE - 1; j >= 0; j-- )
				{
				DT[ j ]++;
				if( DT[ j ] != 0 )
					break;
				}
			}
		}
#if ( RNG_TEST_VALUES == RNG_TEST_FIPSEVAL )
	/* Since this is run as a background thread we need to flush the output 
	   before the thread terminates, otherwise it'll be discarded */
	fflush( stdout );
#endif /* FIPS eval data output */
#endif /* Slower CPUs */

	return( CRYPT_OK );
	}
