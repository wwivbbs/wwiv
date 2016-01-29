/****************************************************************************
*																			*
*						cryptlib Bignum Support Routines					*
*						Copyright Peter Gutmann 1995-2014					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
#else
  #include "crypt.h"
  #include "context/context.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKC

/* The vast numbers of iterated and/or recursive calls to bignum code means 
   that any diagnostic print routines produce an enormous increase in 
   runtime, to deal with this we define a conditional value that can be used 
   to control printing of output.  In addition where possible the diagnostic 
   code itself tries to minimise the conditions under which it produces 
   output */

#ifndef NDEBUG
static const BOOLEAN diagOutput = FALSE;
#endif /* !NDEBUG */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Make sure that a bignum/BN_CTX's metadata is valid */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckBignum( const BIGNUM *bignum )
	{
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	if( bignum->dmax < 1 || bignum->dmax > BIGNUM_ALLOC_WORDS_EXT2 )
		return( FALSE );
	if( bignum->top < 0 || bignum->top > bignum->dmax )
		return( FALSE );
	if( bignum->neg != TRUE && bignum->neg != FALSE )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckBNCTX( const BN_CTX *bnCTX )
	{
	assert( isReadPtr( bnCTX, sizeof( BN_CTX ) ) );

	if( bnCTX->bnArrayMax < 0 || bnCTX->bnArrayMax > BN_CTX_ARRAY_SIZE )
		return( FALSE );
	if( bnCTX->stackPos < 0 || bnCTX->stackPos >= BN_CTX_ARRAY_SIZE )
		return( FALSE );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						 Miscellaneous Bignum Routines						*
*																			*
****************************************************************************/

/* Allocate/initialise/clear/free bignums.  The original OpenSSL bignum code 
   allocates storage on-demand, which results in both lots of kludgery to 
   deal with array bounds and buffer sizes moving around, and huge numbers 
   of memory-allocation/reallocation calls as bignum data sizes creep slowly 
   upwards until some sort of steady state is reached, whereupon the bignum 
   is destroyed and a new one allocated and the whole cycle begins anew.

   To avoid all of this memory-thrashing we use a fixed-size memory block 
   for each bignum, which is unfortunately somewhat wasteful but saves a 
   lot of memory allocation/reallocation and accompanying heap fragmentation 
   (see also the BN_CTX code comments further down).

   A useful side-effect of the elimination of dynamic memory allocation is 
   that the large number of null pointer dereferences on allocation failure 
   in the OpenSSL bignum code are never triggered because there's always 
   memory allocated for the bignum */

void BN_clear( BIGNUM *bignum )
	{
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_V( sanityCheckBignum( bignum ) );

	if( !( bignum->flags & BN_FLG_STATIC_DATA ) )
		{
		DEBUG_PRINT_COND( diagOutput && bignum->top > 64, \
						  ( "BN max.size = %d words.\n", bignum->top ) );
		zeroise( bignum->d, bnWordsToBytes( bignum->dmax ) );
		bignum->top = bignum->neg = 0;
		}
	}

void BN_init( BIGNUM *bignum )
	{
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	memset( bignum, 0, sizeof( BIGNUM ) );
	bignum->dmax = BIGNUM_ALLOC_WORDS;
	}

BIGNUM *BN_new( void )
	{
	BIGNUM *bignum;

	bignum = clAlloc( "BN_new", sizeof( BIGNUM ) );
	if( bignum == NULL )
		return( NULL );
	BN_init( bignum );
	bignum->flags = BN_FLG_MALLOCED;

	return( bignum );
	}

void BN_free( BIGNUM *bignum )
	{
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	BN_clear( bignum );
	if( bignum->flags & BN_FLG_MALLOCED )
		clFree( "BN_free", bignum );
	}

/* Duplicate, swap bignums */

BIGNUM *BN_dup( const BIGNUM *bignum )
	{
	BIGNUM *newBignum;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	newBignum = BN_new();
	if( newBignum == NULL ) 
		return( NULL );
	BN_copy( newBignum, bignum );

	return( newBignum );
	}

BIGNUM *BN_copy( BIGNUM *destBignum, const BIGNUM *srcBignum )
	{
	assert( isWritePtr( destBignum, sizeof( BIGNUM ) ) );
	assert( isReadPtr( srcBignum, sizeof( BIGNUM ) ) );

	REQUIRES_N( sanityCheckBignum( destBignum ) );
	REQUIRES_N( sanityCheckBignum( srcBignum ) );
	REQUIRES_N( destBignum->dmax >= srcBignum->top );

	/* Copy most of the bignum fields.  We don't copy the maximum-size field
	   or the flags field since these may differ for the two bignums (the 
	   flags field will be things like BN_FLG_MALLOCED and 
	   BN_FLG_STATIC_DATA) */
	memcpy( destBignum->d, srcBignum->d, bnWordsToBytes( srcBignum->top ) );
	destBignum->top = srcBignum->top;
	destBignum->neg = srcBignum->neg;

	return( destBignum );
	}

void BN_swap( BIGNUM *bignum1, BIGNUM *bignum2 )
	{
	BIGNUM tmp;

	assert( isWritePtr( bignum1, sizeof( BIGNUM ) ) );
	assert( isWritePtr( bignum2, sizeof( BIGNUM ) ) );

	REQUIRES_V( !( bignum1->flags & BN_FLG_STATIC_DATA ) );
	REQUIRES_V( !( bignum1->flags & BN_FLG_STATIC_DATA ) );

	BN_init( &tmp );
	BN_copy( &tmp, bignum1 );
	BN_copy( bignum1, bignum2 );
	BN_copy( bignum2, &tmp );
	BN_clear( &tmp );
	}

/* Get a bignum with the value 1 */

const BIGNUM *BN_value_one( void )
	{
	static const BIGNUM bignum = { BIGNUM_ALLOC_WORDS, 1, FALSE, 
								   BN_FLG_STATIC_DATA, { 1, 0, 0, 0 } };

	/* Catch problems arising from changes to bignum struct layout */
	assert( bignum.dmax == BIGNUM_ALLOC_WORDS );
	assert( bignum.top == 1 );
	assert( bignum.neg == 0 );
	assert( bignum.flags == BN_FLG_STATIC_DATA );
	assert( bignum.d[ 0 ] == 1 && bignum.d[ 1 ] == 0 && bignum.d[ 2 ] == 0 );

	return( &bignum );
	}

/****************************************************************************
*																			*
*						 Manipulate Bignum Values/Data						*
*																			*
****************************************************************************/

/* Get/set a bignum as a word value */

BN_ULONG BN_get_word( const BIGNUM *bignum )
	{
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	/* If the result won't fit in a word, return a NaN indicator */
	if( bignum->top > 1 )
		return( ( BN_ULONG ) BN_MASK2 );

	/* Bignums with the value zero have a length of zero so we don't try and
	   read a data value from them */
	if( bignum->top < 1 )
		return( 0 );

	return( bignum->d[ 0 ] );
	}

int BN_set_word( BIGNUM *bignum, BN_ULONG word )
	{
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_B( !( bignum->flags & BN_FLG_STATIC_DATA ) );

	BN_clear( bignum );
	bignum->d[ 0 ] = word;
	bignum->top = word ? 1 : 0;

	return( 1 );
	}

/* Count the number of bits used in a word and in a bignum.  The former is 
   the classic log2 problem for which there are about a million clever hacks 
   (including stuffing them into IEEE-754 64-bit floats and fiddling with 
   the bit-representation of those) but they're all endianness/word-size/
   whatever-dependent and since this is never called in time-critical code 
   we just use a straight loop, which works everywhere */

int BN_num_bits_word( const BN_ULONG word )
	{
	BN_ULONG value = word;
	int i;

	for( i = 0; i < 128 && value > 0; i++ )
		value >>= 1;
	ENSURES( i < 128 );

	return( i );
	}

int BN_num_bits( const BIGNUM *bignum )
	{
	const int lastWordIndex = bignum->top - 1;
	int bits, status;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES( sanityCheckBignum( bignum ) );

	/* Bignums with value zero are special-cased since they have a length of
	   zero */
	if( bignum->top <= 0 )
		return( 0 );

	status = bits = BN_num_bits_word( bignum->d[ lastWordIndex ] );
	if( cryptStatusError( status ) )
		return( status );
	return( ( lastWordIndex * BN_BITS2 ) + bits );
	}

/* Bit-manipulation operations */

int BN_set_bit( BIGNUM *bignum, int bitNo )
	{
	const int wordIndex = bitNo / BN_BITS2;
	const int bitIndex = bitNo % BN_BITS2;

	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( bignum ) );
	REQUIRES_B( !( bignum->flags & BN_FLG_STATIC_DATA ) );
	REQUIRES_B( bitNo >= 0 && bitNo < bnWordsToBits( bignum->dmax ) );

	/* If we're extending the bignum, clear the words up to where we insert 
	   the bit.
	   
	   Note that the use of the unified BIGNUM type to also represent a 
	   BIGNUM_EXT/BIGNUM_EXT2 can result in false-positive warnings from 
	   bounds-checking applications that apply the d[] array size from a 
	   BIGNUM to the much larger array in a BIGNUM_EXT/BIGNUM_EXT2 */
	if( bignum->top < wordIndex + 1 )
		{
		int index;

		REQUIRES_B( wordIndex < bignum->dmax );
		for( index = bignum->top; index < wordIndex + 1; index++ )
			bignum->d[ index ] = 0;
		ENSURES_B( index < BIGNUM_ALLOC_WORDS_EXT );
		bignum->top = wordIndex + 1;
		}

	/* Set the appropriate bit location */
	bignum->d[ wordIndex ] |= ( BN_ULONG ) 1 << bitIndex;

	return( 1 );
	}

int BN_is_bit_set( const BIGNUM *bignum, int bitNo )
	{
	const int wordIndex = bitNo / BN_BITS2;
	const int bitIndex = bitNo % BN_BITS2;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_B( bitNo < bnWordsToBits( bignum->dmax ) );	/* See comment below */

	/* The OpenSSL bignum code occasionally calls this with negative values 
	   for the bit to check (e.g. the Montgomery modexp code, which contains 
	   a comment that explicitly says it'll be calling this function with 
	   negative bit values) so we have to special-case this condition */
	if( bitNo < 0 )
		return( 0 );

	/* Bits off the end of the bignum are always zero */
	if( wordIndex >= bignum->top )
		return( 0 );

	return( ( bignum->d[ wordIndex ] & ( ( BN_ULONG ) 1 << bitIndex ) ) ? \
			TRUE : FALSE );
	}

int BN_high_bit( const BIGNUM *bignum )
	{
	int noBytes = BN_num_bytes( bignum ) - 1;
	BN_ULONG highWord;
	int highByte;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	/* Bignums with value zero are special-cased since they have a length of
	   zero */
	if( noBytes < 0 )
		return( 0 );

	/* Extract the topmost nonzero byte in the bignum */
	highWord = bignum->d[ noBytes / BN_BYTES ];
	highByte = ( int ) ( highWord >> ( ( noBytes % BN_BYTES ) * 8 ) );

	return( ( highByte & 0x80 ) ? 1 : 0 );
	}

/* Set the sign flag on a bignum */

void BN_set_negative( BIGNUM *bignum, int value )
	{
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	if( BN_is_zero( bignum ) )
		return;
	bignum->neg = value ? TRUE : FALSE;
	}

/* A bignum operation may have reduced the magnitude of the bignum value,
   in which case bignum->top will be left pointing to the head of a long
   string of zeroes.  The following function normalises the representation,
   leaving bignum->top pointing to the first nonzero entry */

void BN_normalise( BIGNUM *bignum )
	{
	int iterationCount;

	REQUIRES_V( sanityCheckBignum( bignum ) );

	/* If it's a zero-magnitude bignum then there's nothing to do */
	if( bignum->top <= 0 )
		return;

	/* Note that the use of the unified BIGNUM type to also represent a 
	   BIGNUM_EXT/BIGNUM_EXT2 can result in false-positive warnings from 
	   bounds-checking applications that apply the d[] array size from a 
	   BIGNUM to the much larger array in a BIGNUM_EXT/BIGNUM_EXT2 */
	for( iterationCount = 0;
		 bignum->top > 0 && iterationCount < BIGNUM_ALLOC_WORDS_EXT;
		 bignum->top--, iterationCount++ )
		{
		if( bignum->d[ bignum->top - 1 ] != 0 )
			break;
		}
	ENSURES_V( iterationCount < BIGNUM_ALLOC_WORDS_EXT );
	}

/****************************************************************************
*																			*
*								Compare Bignums								*
*																			*
****************************************************************************/

/* Compare two bignums */

int bn_cmp_words( const BN_ULONG *bignumData1, const BN_ULONG *bignumData2, 
				  const int length )
	{
	int i;

	assert( isReadPtr( bignumData1, length ) );
	assert( isReadPtr( bignumData2, length ) );

	REQUIRES( length >= 0 && length < BIGNUM_ALLOC_WORDS );

	/* Walk down the bignum until we find a difference */
	for( i = length - 1; i >= 0; i-- )
		{
		if( bignumData1[ i ] != bignumData2[ i ] )
			return( ( bignumData1[ i ] > bignumData2[ i ] ) ? 1 : -1 );
		}

	/* They're identical */
	return( 0 );
	}

int BN_ucmp( const BIGNUM *bignum1, const BIGNUM *bignum2 )
	{
	const int bignum1top = bignum1->top;

	assert( isReadPtr( bignum1, sizeof( BIGNUM ) ) );
	assert( isReadPtr( bignum2, sizeof( BIGNUM ) ) );

	REQUIRES( bignum1top >= 0 && bignum1top < BIGNUM_ALLOC_WORDS_EXT );

	/* If the magnitude differs then we don't need to look at the values */
	if( bignum1top != bignum2->top )
		return( bignum1top - bignum2->top );

	return( bn_cmp_words( bignum1->d, bignum2->d, bignum1top ) );
	}

/* Compare some sort of data for two bignums, used internally by the 
   bn_mul_recursive() routine */

int bn_cmp_part_words( const BN_ULONG *a, const BN_ULONG *b, const int cl, 
					   const int dl )
	{
	const int n = cl - 1;
	int i;

	/* Something bizarre used at one point in BN_mul() as part of the 
	   bn_xxx_part_words() functions, which is where this function should 
	   actually be located.  No idea what it really achieves, but the 
	   original comment reads:

		[This] has the property of performing the operation on arrays of 
		different sizes.  The sizes of those arrays is expressed through cl, 
		which is the common length (basically, min(len(a),len(b))), and dl, 
		which is the delta between the two lengths, calculated as len(a)-
		len(b)

	   This is just a reformat of the original code, best left well enough 
	   alone */
	if( dl < 0 )
		{
		for( i = dl; i < 0; i++ )
			{
			if( b[ n - i ] != 0 )
				return( -1 ); /* a < b */
			}
		}
	if( dl > 0 )
		{
		for( i = dl; i > 0; i-- )
			{
			if( a[ n + i ] != 0 )
				return( 1 ); /* a > b */
			}
		}

	return( bn_cmp_words( a, b, cl ) );
	}

/****************************************************************************
*																			*
*							BN_CTX Support Routines 						*
*																			*
****************************************************************************/

/* The BN_CTX code up until about 2000 (or cryptlib 3.21) used to be just an 
   array of BN_CTX_NUM = 32 BIGNUMs, then after that it was replaced by an
   awkward pool/stack combination that makes something relatively 
   straighforward quite complex.  What's needed is a way of stacking and
   unstacking blocks of BN_CTX_get()s in nested functions:

	BN_foo()
		BN_CTX_start();
		foo_a = BN_CTX_get();
		foo_b = BN_CTX_get();
		foo_c = BN_CTX_get();
		BN_bar()
			BN_CTX_start();
			bar_a = BN_CTX_get();
			bar_b = BN_CTX_get();
			BN_CTX_end();
		BN_CTX_end();

   where the first BN_CTX_end() frees up the BNs grabbed in BN_bar() and the
   second frees up the ones grabbed in BB_foo().  This is why we have the
   stack alongside the bignum array, every time we increase the nesting depth
   by calling BN_CTX_start() we remember the stack position that we need to 
   unwind to when BN_CTX_end() is called.

   All of the complex stack/pool manipulations in the original code, 
   possibly meant to "optimise" the strategy of allocating a single fixed-
   size block of values, actually have a negative effect on memory use 
   because the bookkeeping overhead of dozens of tiny little allocations is 
   more than just allocating the fixed-size block.  Since we can measure the 
   deepest that the allocation ever goes we just use a fixed-size array of 
   bignums set to BN_CTX_ARRAY_SIZE.
   
   The init and end functions just set up and clear a BN_CTX */

STDC_NONNULL_ARG( ( 1 ) ) \
static void BN_init_ext( INOUT void *bignumExtPtr, 
						 const BOOLEAN isExtBignum )
	{
	assert( isWritePtr( bignumExtPtr, sizeof( BIGNUM_EXT ) ) );

	if( isExtBignum )
		{
		BIGNUM_EXT2 *bignum = bignumExtPtr;

		memset( bignum, 0, sizeof( BIGNUM_EXT2 ) );
		bignum->dmax = BIGNUM_ALLOC_WORDS_EXT2;
		}
	else
		{
		BIGNUM_EXT *bignum = bignumExtPtr;

		memset( bignum, 0, sizeof( BIGNUM_EXT ) );
		bignum->dmax = BIGNUM_ALLOC_WORDS_EXT;
		}
	}

void BN_CTX_init( BN_CTX *bnCTX )
	{
	int i;

	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	memset( bnCTX, 0, sizeof( BN_CTX ) );
	for( i = 0; i < BN_CTX_ARRAY_SIZE; i++ )
		BN_init( &bnCTX->bnArray[ i ] );
	ENSURES_V( i == BN_CTX_ARRAY_SIZE );
	for( i = 0; i < BN_CTX_EXTARRAY_SIZE; i++ )
		BN_init_ext( &bnCTX->bnExtArray[ i ], FALSE );
	ENSURES_V( i == BN_CTX_EXTARRAY_SIZE );
	for( i = 0; i < BN_CTX_EXT2ARRAY_SIZE; i++ )
		BN_init_ext( &bnCTX->bnExt2Array[ i ], TRUE );
	ENSURES_V( i == BN_CTX_EXT2ARRAY_SIZE );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void BN_CTX_final( INOUT BN_CTX *bnCTX )
	{
	int i;

	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_V( sanityCheckBNCTX( bnCTX ) );

	/* Clear the overall BN_CTX */
	zeroise( bnCTX, sizeof( BN_CTX ) );

	/* The various bignums were cleared when the BN_CTX was zeroised, we now 
	   have to reset them to their initial state so that they can be 
	   reused */
	for( i = 0; i < BN_CTX_ARRAY_SIZE; i++ )
		BN_init( &bnCTX->bnArray[ i ] );
	ENSURES_V( i == BN_CTX_ARRAY_SIZE );
	for( i = 0; i < BN_CTX_EXTARRAY_SIZE; i++ )
		BN_init_ext( &bnCTX->bnExtArray[ i ], FALSE );
	ENSURES_V( i == BN_CTX_EXTARRAY_SIZE );
	DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL1 freed.\nEXT_MUL2 freed.\n" ));
	for( i = 0; i < BN_CTX_EXT2ARRAY_SIZE; i++ )
		BN_init_ext( &bnCTX->bnExt2Array[ i ], TRUE );
	ENSURES_V( i == BN_CTX_EXT2ARRAY_SIZE );
	DEBUG_PRINT_COND( diagOutput, ( "EXT_MONT freed.\n" ));
	}

/* The start and badly-named end functions (it should be finish()) remember 
   the current stack position and unwind to the last stack position */

void BN_CTX_start( BN_CTX *bnCTX )
	{
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_V( sanityCheckBNCTX( bnCTX ) );

	/* Advance one stack frame */
	bnCTX->stackPos++;
	bnCTX->stack[ bnCTX->stackPos ] = bnCTX->stack[ bnCTX->stackPos - 1 ];

	ENSURES_V( sanityCheckBNCTX( bnCTX ) );
	}

void BN_CTX_end( BN_CTX *bnCTX )
	{
	int i;

	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_V( sanityCheckBNCTX( bnCTX ) );
	REQUIRES_V( bnCTX->stack[ bnCTX->stackPos - 1 ] <= \
						bnCTX->stack[ bnCTX->stackPos ] );

	/* Only enable the following when required, the bignum code performs
	   a huge number of stackings and un-stackings for which the following
	   produces an enormous increase in runtime */
#if 0	
	DEBUG_PRINT(( "bnCTX unstacking from %d to %d.\n", 
				  bnCTX->stack[ bnCTX->stackPos - 1 ],
				  bnCTX->stack[ bnCTX->stackPos ] ));
#endif /* 0 */

	/* Clear each bignum in the current stack frame */
	for( i = bnCTX->stack[ bnCTX->stackPos - 1 ]; 
		 i < bnCTX->stack[ bnCTX->stackPos ] && i < BN_CTX_ARRAY_SIZE; i++ )
		{
		BN_clear( &bnCTX->bnArray[ i ] );
		}
	ENSURES_V( i < BN_CTX_ARRAY_SIZE );

	/* Unwind the stack by one frame */
	bnCTX->stack[ bnCTX->stackPos ] = 0;
	bnCTX->stackPos--;

	ENSURES_V( sanityCheckBNCTX( bnCTX ) );
	}

/* Peel another bignum off the BN_CTX array */

BIGNUM *BN_CTX_get( BN_CTX *bnCTX )
	{
	BIGNUM *bignum;
	const int arrayIndex = bnCTX->stack[ bnCTX->stackPos ] + 1;

	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	if( bnCTX->bnArrayMax >= BN_CTX_ARRAY_SIZE )
		{
		assert( DEBUG_WARN );
		DEBUG_PRINT(( "bnCTX array size overflow.\n" ));

		return( NULL );
		}

	REQUIRES_N( sanityCheckBNCTX( bnCTX ) );

	/* Get the element at the previous top-of-stack */
	bignum = &bnCTX->bnArray[ arrayIndex - 1 ];

	/* Advance the top-of-stack element by one, and increase the last-used 
	   postion if it exceeds the existing one */
	bnCTX->stack[ bnCTX->stackPos ] = arrayIndex;
	if( arrayIndex > bnCTX->bnArrayMax )
		bnCTX->bnArrayMax = arrayIndex;

	ENSURES_N( sanityCheckBNCTX( bnCTX ) );

	/* Return the new element at the top of the stack */
	return( bignum );
	}

/* The bignum multiplication code requires a few temporary values that grow 
   to an enormous size, rather than resizing every bignum that we use to 
   deal with this we return fixed extra-size bignums when this is explicitly 
   required */

BIGNUM *BN_CTX_get_ext( BN_CTX *bnCTX, const BIGNUM_EXT_TYPE bnExtType )
	{
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	ENSURES_N( bnExtType > BIGNUM_EXT_NONE && bnExtType < BIGNUM_EXT_LAST );

	switch( bnExtType )
		{
		case BIGNUM_EXT_MONT:
			DEBUG_PRINT_COND( diagOutput, ( "EXT_MONT acquired.\n" ));
			return( ( BIGNUM * ) &bnCTX->bnExtArray[ 0 ] );

		case BIGNUM_EXT_MUL1:
			DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL1 acquired.\n" ));
			return( ( BIGNUM * ) &bnCTX->bnExt2Array[ 0 ] );

		case BIGNUM_EXT_MUL2:
			DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL2 acquired.\n" ));
			return( ( BIGNUM * ) &bnCTX->bnExt2Array[ 1 ] );
		}

	retIntError_Null();
	}

void BN_CTX_end_ext( BN_CTX *bnCTX, const BIGNUM_EXT_TYPE bnExtType )
	{
	/* Perform the standard context cleanup */
	BN_CTX_end( bnCTX );

	ENSURES_V( bnExtType == BIGNUM_EXT_MUL1 || \
			   bnExtType == BIGNUM_EXT_MONT );

	/* Clear the extended-size bignums */
	if( bnExtType == BIGNUM_EXT_MUL1 )
		{
		BN_clear( BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MUL1 ) );
		DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL1 cleared.\n" ));
		BN_clear( BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MUL2 ) );
		DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL2 cleared.\n" ));
		}
	else
		{
		BN_clear( BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MONT ) );
		DEBUG_PRINT_COND( diagOutput, ( "EXT_MONT cleared.\n" ));
		}
	}

/* Dynamically allocate a BN_CTX, only needed by the ECC code */

#if defined( USE_ECDH ) || defined( USE_ECDSA )

BN_CTX *BN_CTX_new( void )
	{
	BN_CTX *bnCTX;

	bnCTX = clAlloc( "BN_CTX_new", sizeof( BN_CTX ) );
	if( bnCTX == NULL )
		return( NULL );
	BN_CTX_init( bnCTX );

	return( bnCTX );
	}

void BN_CTX_free( BN_CTX *bnCTX )
	{
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_V( sanityCheckBNCTX( bnCTX ) );

	BN_CTX_final( bnCTX );
	clFree( "BN_CTX_free", bnCTX );
	}
#endif /* ECDH || ECDSA */

/****************************************************************************
*																			*
*							BN_MONT_CTX Support Routines 					*
*																			*
****************************************************************************/

/* Initialise/clear a BN_MONT_CTX */

void BN_MONT_CTX_init( BN_MONT_CTX *bnMontCTX )
	{
	assert( isWritePtr( bnMontCTX, sizeof( BN_MONT_CTX ) ) );

	memset( bnMontCTX, 0, sizeof( BN_MONT_CTX ) );
	BN_init( &( bnMontCTX->RR ) );
	BN_init( &( bnMontCTX->N ) );
	}

void BN_MONT_CTX_free( BN_MONT_CTX *bnMontCTX )
	{
	assert( isWritePtr( bnMontCTX, sizeof( BN_MONT_CTX ) ) );

	BN_clear( &( bnMontCTX->RR ) );
	BN_clear( &( bnMontCTX->N ) );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( bnMontCTX->flags & BN_FLG_MALLOCED)
		clFree( "BN_MONT_CTX_free", bnMontCTX );
#endif /* ECDH || ECDSA */
	}

/* Dynamically allocate a BN_MONT_CTX, only needed by the ECC code */

#if defined( USE_ECDH ) || defined( USE_ECDSA )

BN_MONT_CTX *BN_MONT_CTX_new( void )
	{
	BN_MONT_CTX *bnMontCTX;

	bnMontCTX = clAlloc( "BN_MONT_CTX_new", sizeof( BN_MONT_CTX ) );
	if( bnMontCTX == NULL )
		return( NULL );
	BN_MONT_CTX_init( bnMontCTX );
	bnMontCTX->flags = BN_FLG_MALLOCED;

	return( bnMontCTX );
	}
#else

BN_MONT_CTX *BN_MONT_CTX_new( void )
	{
	assert( DEBUG_WARN );
	return( NULL );
	}
#endif /* ECDH || ECDSA */

/****************************************************************************
*																			*
*						Bignum Init/Shutdown Routines 						*
*																			*
****************************************************************************/

/* Clear temporary bignum values used during PKC operations */

STDC_NONNULL_ARG( ( 1 ) ) \
void clearTempBignums( INOUT PKC_INFO *pkcInfo )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	BN_clear( &pkcInfo->tmp1 ); BN_clear( &pkcInfo->tmp2 );
	BN_clear( &pkcInfo->tmp3 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( pkcInfo->isECC )
		{
		BN_clear( &pkcInfo->tmp4 ); BN_clear( &pkcInfo->tmp5 );
		}
#endif /* USE_ECDH || USE_ECDSA */
	BN_CTX_final( &pkcInfo->bnCTX );
	}

/* Initialse and free the bignum information in a context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initContextBignums( INOUT PKC_INFO *pkcInfo, 
						IN_RANGE( 0, 3 ) const int sideChannelProtectionLevel,
						const BOOLEAN isECC )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( sideChannelProtectionLevel >= 0 && \
			  sideChannelProtectionLevel <= 3 );

	/* Initialise the bignum information */
	BN_init( &pkcInfo->param1 ); BN_init( &pkcInfo->param2 );
	BN_init( &pkcInfo->param3 ); BN_init( &pkcInfo->param4 );
	BN_init( &pkcInfo->param5 ); BN_init( &pkcInfo->param6 );
	BN_init( &pkcInfo->param7 ); BN_init( &pkcInfo->param8 );
	BN_init( &pkcInfo->blind1 ); BN_init( &pkcInfo->blind2 );
	BN_init( &pkcInfo->tmp1 ); BN_init( &pkcInfo->tmp2 );
	BN_init( &pkcInfo->tmp3 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isECC )
		{
		pkcInfo->isECC = TRUE;
		BN_init( &pkcInfo->param9 ); BN_init( &pkcInfo->param10 );
		BN_init( &pkcInfo->tmp4 ); BN_init( &pkcInfo->tmp5 );
		pkcInfo->ecCTX = EC_GROUP_new( EC_GFp_simple_method() );
		pkcInfo->ecPoint = EC_POINT_new( pkcInfo->ecCTX );
		pkcInfo->tmpPoint = EC_POINT_new( pkcInfo->ecCTX );
		if( pkcInfo->ecCTX == NULL || pkcInfo->ecPoint == NULL || \
			pkcInfo->tmpPoint == NULL )
			{
			if( pkcInfo->tmpPoint != NULL )
				EC_POINT_free( pkcInfo->tmpPoint );
			if( pkcInfo->ecPoint != NULL )
				EC_POINT_free( pkcInfo->ecPoint );
			if( pkcInfo->ecCTX != NULL )
				EC_GROUP_free( pkcInfo->ecCTX );
			return( CRYPT_ERROR_MEMORY );
			}
		}
#endif /* USE_ECDH || USE_ECDSA */
	BN_CTX_init( &pkcInfo->bnCTX );
	BN_MONT_CTX_init( &pkcInfo->montCTX1 );
	BN_MONT_CTX_init( &pkcInfo->montCTX2 );
	BN_MONT_CTX_init( &pkcInfo->montCTX3 );

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void endContextBignums( INOUT PKC_INFO *pkcInfo, 
						IN_FLAGS( CONTEXT ) const int contextFlags )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES_V( contextFlags >= CONTEXT_FLAG_NONE && \
				contextFlags <= CONTEXT_FLAG_MAX );

	if( !( contextFlags & CONTEXT_FLAG_DUMMY ) )
		{
		BN_clear( &pkcInfo->param1 ); BN_clear( &pkcInfo->param2 );
		BN_clear( &pkcInfo->param3 ); BN_clear( &pkcInfo->param4 );
		BN_clear( &pkcInfo->param5 ); BN_clear( &pkcInfo->param6 );
		BN_clear( &pkcInfo->param7 ); BN_clear( &pkcInfo->param8 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
		if( pkcInfo->isECC )
			{
			BN_clear( &pkcInfo->param9 ); BN_clear( &pkcInfo->param10 );
			}
#endif /* USE_ECDH || USE_ECDSA */
		BN_clear( &pkcInfo->blind1 ); BN_clear( &pkcInfo->blind2 );
		BN_clear( &pkcInfo->tmp1 ); BN_clear( &pkcInfo->tmp2 );
		BN_clear( &pkcInfo->tmp3 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
		if( pkcInfo->isECC )
			{
			BN_clear( &pkcInfo->tmp4 ); BN_clear( &pkcInfo->tmp5 );
			EC_POINT_free( pkcInfo->tmpPoint );
			EC_POINT_free( pkcInfo->ecPoint );
			EC_GROUP_free( pkcInfo->ecCTX );
			}
#endif /* USE_ECDH || USE_ECDSA */
		BN_CTX_final( &pkcInfo->bnCTX );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
		if( !pkcInfo->isECC )
#endif /* USE_ECDH || USE_ECDSA */
			{
			BN_MONT_CTX_free( &pkcInfo->montCTX1 );
			BN_MONT_CTX_free( &pkcInfo->montCTX2 );
			BN_MONT_CTX_free( &pkcInfo->montCTX3 );
			}
		}
	if( pkcInfo->publicKeyInfo != NULL )
		clFree( "freeContextBignums", pkcInfo->publicKeyInfo );
	}

/****************************************************************************
*																			*
*							Bignum Checksum Routines 						*
*																			*
****************************************************************************/

/* Checksum a bignum's metadata and its data payload.  We can't use the 
   standard checksumData() here both because we need to keep a running total 
   of the existing checksum value and because we don't want to truncate the 
   checksum value to 16 bits at the end of each calculation because it's fed 
   to the next round of checksumming.
   
   The checksum function is declared non-static because it's also used to 
   checksum ECC values, if we pulled the ECC-specific checksumming routines 
   in here then we'd need to also include masses of ECC data types and 
   whatnot so they're located in ec_lib.c and call into this function from 
   here */

CHECK_RETVAL_RANGE_NOERROR( 0, INT_MAX ) STDC_NONNULL_ARG( ( 1 ) ) \
int checksumBignumData( IN_BUFFER( length ) const void *data, 
						IN_LENGTH_SHORT const int length,
						IN const int initialSum )
	{
	const unsigned char *dataPtr = data;
	int sum1 = 0, sum2 = initialSum, i;

	assert( isReadPtr( data, length ) );

	REQUIRES_EXT( length > 0 && length < MAX_INTLENGTH_SHORT, 0 );

	for( i = 0; i < length; i++ )
		{
		sum1 += dataPtr[ i ];
		sum2 += sum1;
		}
	return( sum2 );
	}

#define BN_checksum( bignum, checksum ) \
	*( checksum ) = checksumBignumData( bignum, sizeof( BIGNUM ), *( checksum ) )
#define BN_checksum_montgomery( montCTX, checksum ) \
	*( checksum ) = checksumBignumData( montCTX, sizeof( BN_MONT_CTX ), *( checksum ) )

#if defined( USE_ECDH ) || defined( USE_ECDSA )

/* Checksum the ECC structure metadata and their data payloads */

static void BN_checksum_ec_group( IN const EC_GROUP *group, 
								  INOUT_INT_Z int *checksum )
	{
	int value = *checksum;

	/* Checksum the EC_GROUP metadata */
	value = checksumBignumData( group, sizeof( EC_GROUP ), value );

	/* EC_GROUPs have an incredibly complex inner stucture (see 
	   bn/ec_lcl.h), the following checksums the common data without getting 
	   into the lists of method-specific data values */
	BN_checksum( &group->order, &value );
	BN_checksum( &group->cofactor, &value );
	BN_checksum( &group->field, &value );
	BN_checksum( &group->a, &value );
	BN_checksum( &group->b, &value );
	
	*checksum = value;
	}

static void BN_checksum_ec_point( IN const EC_POINT *point, 
								  INOUT_INT_Z int *checksum )
	{
	int value = *checksum;

	/* Checksum the EC_POINT metadata */
	value = checksumBignumData( point, sizeof( EC_POINT ), value );

	/* Checksum the EC_POINT data */
	BN_checksum( &point->X, &value );
	BN_checksum( &point->Y, &value );
	BN_checksum( &point->Z, &value );

	*checksum = value;
	}
#endif /* USE_ECDH || USE_ECDSA */

/* Calculate a bignum checksum */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int bignumChecksum( INOUT PKC_INFO *pkcInfo, 
						   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
						   const BOOLEAN isPrivateKey,
						   OUT_INT_Z int *checksum )
	{
	int value = 0;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* Clear return value */
	*checksum = 0;

	/* Calculate the key data checksum */
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isEccAlgo( cryptAlgo ) )
		{
		BN_checksum( &pkcInfo->eccParam_p, &value );
		BN_checksum( &pkcInfo->eccParam_a, &value );
		BN_checksum( &pkcInfo->eccParam_b, &value );
		BN_checksum( &pkcInfo->eccParam_gx, &value );
		BN_checksum( &pkcInfo->eccParam_gy, &value );
		BN_checksum( &pkcInfo->eccParam_n, &value );
		BN_checksum( &pkcInfo->eccParam_h, &value );
		BN_checksum( &pkcInfo->eccParam_qx, &value );
		BN_checksum( &pkcInfo->eccParam_qy, &value );
		if( isPrivateKey )
			BN_checksum( &pkcInfo->eccParam_d, &value );
		BN_checksum_ec_group( pkcInfo->ecCTX, &value );
		BN_checksum_ec_point( pkcInfo->ecPoint, &value );
		}
	else
#endif /* USE_ECDH || USE_ECDSA */
	if( isDlpAlgo( cryptAlgo ) )
		{
		BN_checksum( &pkcInfo->dlpParam_p, &value );
		BN_checksum( &pkcInfo->dlpParam_g, &value );
		if( !BN_is_zero( &pkcInfo->dlpParam_q ) )
			BN_checksum( &pkcInfo->dlpParam_q, &value );
		BN_checksum( &pkcInfo->dlpParam_y, &value );
		if( isPrivateKey )
			BN_checksum( &pkcInfo->dlpParam_x, &value );
		BN_checksum_montgomery( &pkcInfo->dlpParam_mont_p, &value );
		}
	else
		{
		BN_checksum( &pkcInfo->rsaParam_n, &value );
		BN_checksum( &pkcInfo->rsaParam_e, &value );
		BN_checksum_montgomery( &pkcInfo->rsaParam_mont_n, &value );
		if( isPrivateKey )
			{
			if( !BN_is_zero( &pkcInfo->rsaParam_d ) )
				BN_checksum( &pkcInfo->rsaParam_d, &value );
			BN_checksum( &pkcInfo->rsaParam_p, &value );
			BN_checksum( &pkcInfo->rsaParam_q, &value );
			BN_checksum( &pkcInfo->rsaParam_u, &value );
			BN_checksum( &pkcInfo->rsaParam_exponent1, &value );
			BN_checksum( &pkcInfo->rsaParam_exponent2, &value );
			BN_checksum_montgomery( &pkcInfo->rsaParam_mont_p, &value );
			BN_checksum_montgomery( &pkcInfo->rsaParam_mont_q, &value );
			}
		}
	*checksum = value;

	return( CRYPT_OK ); 
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checksumContextData( INOUT PKC_INFO *pkcInfo, 
						 IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
						 const BOOLEAN isPrivateKey )
	{
	int checksum, status;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* Set or update the data checksum */
	status = bignumChecksum( pkcInfo, cryptAlgo, isPrivateKey, &checksum );
	ENSURES( cryptStatusOK( status ) );
	if( pkcInfo->checksum == 0L )
		pkcInfo->checksum = checksum;
	else
		{
		if( pkcInfo->checksum != checksum )
			return( CRYPT_ERROR );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Bignum Import Routines 						*
*																			*
****************************************************************************/

/* Check that an imported bignum value is valid */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkBignum( const BIGNUM *bignum,
						IN_LENGTH_PKC const int minLength, 
						IN_LENGTH_PKC const int maxLength, 
						IN_OPT const BIGNUM *maxRange,
						IN_ENUM_OPT( KEYSIZE_CHECK_SPECIAL ) \
							const KEYSIZE_CHECK_TYPE checkType )
	{
	BN_ULONG bnWord;
	int bignumLengthBits;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );
	assert( maxRange == NULL || isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES( minLength > 0 && minLength <= maxLength && \
			  maxLength <= CRYPT_MAX_PKCSIZE + \
						   ( ( checkType == KEYSIZE_CHECK_SPECIAL ) ? \
							   DLP_OVERFLOW_SIZE : 0 ) );
	REQUIRES( maxRange == NULL || sanityCheckBignum( maxRange ) );
	REQUIRES( checkType >= KEYSIZE_CHECK_NONE && \
			  checkType < KEYSIZE_CHECK_SPECIAL_LAST );

	/* The following should never happen because BN_bin2bn() works with 
	   unsigned values but we perform the check anyway just in case someone 
	   messes with the underlying bignum code */
	ENSURES( !( BN_is_negative( bignum ) ) )

	/* A zero- or one-valued bignum on the other hand is an error because we 
	   should never find zero or one in a PKC-related value.  This check is 
	   somewhat redundant with the one that follows, we place it here to 
	   make it explicit and because the cost is near zero */
	bnWord = BN_get_word( bignum );
	if( bnWord < ( BN_ULONG ) BN_MASK2 && bnWord <= 1 )
		return( CRYPT_ERROR_BADDATA );

	/* Check that the bignum value falls within the allowed length range.  
	   Before we do the general length check we perform a more specific 
	   check for the case where the length is below the minimum allowed but 
	   still looks at least vaguely valid, in which case we report it as a 
	   too-short key rather than a bad data error.
	   
	   Note that we check the length in bits rather than bytes since the 
	   latter is rounded up, so a length that passes the check based on
	   its rounded-up byte-length value can then fail a later check based 
	   on its actual length in bits */
	bignumLengthBits = BN_num_bits( bignum );
	switch( checkType )
		{
		case KEYSIZE_CHECK_NONE:
			/* No specific weak-key check for this value */
			break;

		case KEYSIZE_CHECK_PKC:
			if( isShortPKCKey( bitsToBytes( bignumLengthBits ) ) )
				return( CRYPT_ERROR_NOSECURE );
			break;

		case KEYSIZE_CHECK_ECC:
			if( isShortECCKey( bitsToBytes( bignumLengthBits ) ) )
				return( CRYPT_ERROR_NOSECURE );
			break;

		case KEYSIZE_CHECK_SPECIAL:
			/* This changes the upper bound of the check rather than the 
			   lower bound so there's no special check required */
			break;

		default:
			retIntError();
		}

	/* Check that the value is within range.  We have to be a bit careful
	   here when doing a bit-length based comparison because the minimum
	   length that can be specified as a byte is 8 bits, so if minLength == 1
	   then we allow any bit-length >= 2 bits, used for DH g values */
	if( minLength == 1 )
		{
		if( bignumLengthBits < 2 || \
			bignumLengthBits > bytesToBits( maxLength ) )
			return( CRYPT_ERROR_BADDATA );
		}
	else
		{
		if( bignumLengthBits < bytesToBits( minLength ) || \
			bignumLengthBits > bytesToBits( maxLength ) )
			return( CRYPT_ERROR_BADDATA );
		}

	/* Finally, if the caller has supplied a maximum-range bignum value, 
	   make sure that the value that we've read is less than this */
	if( maxRange != NULL && BN_cmp( bignum, maxRange ) >= 0 )
		return( CRYPT_ERROR_BADDATA );

	return( CRYPT_OK );
	}

/* Import a bignum from a buffer.  This is a low-level internal routine also
   used by the OpenSSL bignum code, the non-cryptlib-style API is for 
   compatibility purposes */

BIGNUM *BN_bin2bn( const BYTE *buffer, const int length, BIGNUM *bignum )
	{
	int byteCount, wordIndex, index = 0, iterationCount;

	assert( length == 0 || isReadPtr( buffer, length ) );
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_N( length >= 0 && length <= CRYPT_MAX_PKCSIZE );
	REQUIRES_N( sanityCheckBignum( bignum ) );

	/* Clear return value */
	BN_clear( bignum );

	/* Bignums with value zero are special-cased since they have a length of
	   zero */
	if( length <= 0 )
		return( bignum );

	/* Walk down the bignum a word at a time adding the data bytes */
	bignum->top = ( ( length - 1 ) / BN_BYTES ) + 1;
	for( byteCount = length, wordIndex = bignum->top - 1, \
			iterationCount = 0;
		 byteCount > 0 && wordIndex >= 0 && \
			iterationCount < BIGNUM_ALLOC_WORDS;
		 wordIndex--, iterationCount++ )
		{
		BN_ULONG value = 0;
		int noBytes = ( ( byteCount - 1 ) % BN_BYTES ) + 1;
		int innerIterationCount;

		byteCount -= noBytes;
		for( innerIterationCount = 0; 
		     noBytes > 0 && innerIterationCount < BN_BYTES; 
			 noBytes--, innerIterationCount++ )
			value = ( value << 8 ) | buffer[ index++ ];
		ENSURES_N( innerIterationCount <= BN_BYTES );
		bignum->d[ wordIndex ] = value;
		}
	ENSURES_N( iterationCount < BIGNUM_ALLOC_WORDS );
	ENSURES_N( wordIndex == -1 && byteCount == 0 );

	return( bignum );
	}

/* Convert a byte string to a BIGNUM value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int importBignum( INOUT TYPECAST( BIGNUM * ) void *bignumPtr, 
				  IN_BUFFER( length ) const void *buffer, 
				  IN_LENGTH_SHORT const int length,
				  IN_LENGTH_PKC const int minLength, 
				  IN_RANGE( 1, CRYPT_MAX_PKCSIZE + DLP_OVERFLOW_SIZE ) \
						const int maxLength,
				  IN_OPT TYPECAST( BIGNUM * ) const void *maxRangePtr,
				  IN_ENUM_OPT( KEYSIZE_CHECK_SPECIAL ) \
					const KEYSIZE_CHECK_TYPE checkType )
	{
	BIGNUM *bignum = ( BIGNUM * ) bignumPtr;
	BIGNUM *maxRange = ( BIGNUM * ) maxRangePtr;
	int status;

	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );
	assert( isReadPtr( buffer, length ) );
	assert( maxRange == NULL || isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES( minLength > 0 && minLength <= maxLength && \
			  maxLength <= CRYPT_MAX_PKCSIZE + \
						   ( ( checkType == KEYSIZE_CHECK_SPECIAL ) ? \
							   DLP_OVERFLOW_SIZE : 0 ) );
	REQUIRES( maxRange == NULL || sanityCheckBignum( maxRange ) );
	REQUIRES( checkType >= KEYSIZE_CHECK_NONE && \
			  checkType < KEYSIZE_CHECK_SPECIAL_LAST );
	REQUIRES( sanityCheckBignum( bignum ) );

	/* Make sure that we've been given valid input.  This should already 
	   have been checked by the caller using far more specific checks than 
	   the very generic values that we use here, but we perform the check 
	   anyway just to be sure */
	if( length < 1 )
		return( CRYPT_ERROR_BADDATA );
	if( checkType == KEYSIZE_CHECK_SPECIAL )
		{
		if( length > CRYPT_MAX_PKCSIZE + DLP_OVERFLOW_SIZE )
			return( CRYPT_ERROR_BADDATA );
		}
	else
		{
		if( length > CRYPT_MAX_PKCSIZE )
			return( CRYPT_ERROR_BADDATA );
		}

	/* Convert the byte string into a bignum.  Since the only thing that 
	   this does is format a string of bytes as a BN_ULONGs, the only
	   failure that can occur is a CRYPT_ERROR_INTERNAL */
	if( BN_bin2bn( buffer, length, bignum ) == NULL )
		return( CRYPT_ERROR_INTERNAL );

	/* Check the value that we've just imported */
	status = checkBignum( bignum, minLength, maxLength, maxRange, 
						  checkType );
	if( cryptStatusError( status ) )
		BN_clear( bignum );
	return( status );
	}

/****************************************************************************
*																			*
*								Bignum Export Routines 						*
*																			*
****************************************************************************/

/* Export a bignum to a buffer.  This is a low-level internal routine also
   used by the OpenSSL bignum code, the non-cryptlib-style API is for 
   compatibility purposes */

int BN_bn2bin( const BIGNUM *bignum, BYTE *buffer )
	{
	const int length = BN_num_bytes( bignum );
	int byteCount, wordIndex, index = 0, iterationCount;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES( sanityCheckBignum( bignum ) );
	REQUIRES( length >= 0 && length <= CRYPT_MAX_PKCSIZE );

	/* Walk down the bignum a word at a time extracting the data bytes */
	for( byteCount = length, wordIndex = bignum->top - 1, \
			iterationCount = 0;
		 byteCount > 0 && wordIndex >= 0 && \
			iterationCount < BIGNUM_ALLOC_WORDS;
		 wordIndex--, iterationCount++ )
		{
		const BN_ULONG value = bignum->d[ wordIndex ];
		int noBytes = ( ( byteCount - 1 ) % BN_BYTES ) + 1;
		int innerIterationCount;

		byteCount -= noBytes;
		for( innerIterationCount = 0; 
		     noBytes > 0 && innerIterationCount < BN_BYTES; 
			 noBytes--, innerIterationCount++ )
			{
			const int shiftAmount = ( noBytes - 1 ) * 8;

			buffer[ index++ ] = intToByte( value >> shiftAmount );
			}
		ENSURES( innerIterationCount <= BN_BYTES );
		}
	ENSURES( iterationCount < BIGNUM_ALLOC_WORDS );
	ENSURES( wordIndex == -1 && byteCount == 0 );

	return( length );
	}

/* Convert a BIGNUM value to a byte string */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int exportBignum( OUT_BUFFER( dataMaxLength, *dataLength ) void *data, 
				  IN_LENGTH_SHORT_MIN( 16 ) const int dataMaxLength, 
				  OUT_LENGTH_BOUNDED_Z( dataMaxLength ) int *dataLength,
				  IN TYPECAST( BIGNUM * ) const void *bignumPtr )
	{
	BIGNUM *bignum = ( BIGNUM * ) bignumPtr;
	int length, result;

	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES( dataMaxLength >= 16 && dataMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( sanityCheckBignum( bignum ) );

	/* Clear return values */
	memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	/* Make sure that the result will fit into the output buffer */
	length = BN_num_bytes( bignum );
	ENSURES( length > 0 && length <= CRYPT_MAX_PKCSIZE );
	if( length > dataMaxLength )
		return( CRYPT_ERROR_OVERFLOW );

	/* Export the bignum into the output buffer */
	result = BN_bn2bin( bignum, data );
	ENSURES( result == length );
	*dataLength = length;

	return( CRYPT_OK );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

/* Convert a byte string to and from an ECC point.  These are 
   simplifications of the complex functions ec_GFp_simple_point2oct()
   and ec_GFp_simple_oct2point(), which handle point compression and
   on-demand memory allocation and compressed vs. uncompressed vs.
   hybrid representations and other oddities, none of which are of
   much use to us here */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int importECCPoint( INOUT TYPECAST( BIGNUM * ) void *bignumPtr1, 
					INOUT TYPECAST( BIGNUM * ) void *bignumPtr2, 
				    IN_BUFFER( length ) const void *buffer, 
				    IN_LENGTH_SHORT const int length,
					IN_LENGTH_PKC const int minLength, 
					IN_LENGTH_PKC const int maxLength, 
					IN_LENGTH_PKC const int fieldSize,
					IN_OPT const void *maxRangePtr,
					IN_ENUM( KEYSIZE_CHECK ) \
						const KEYSIZE_CHECK_TYPE checkType )
	{
	const BYTE *eccPointData = buffer;
	BIGNUM *bignum1 = ( BIGNUM * ) bignumPtr1;
	BIGNUM *bignum2 = ( BIGNUM * ) bignumPtr2;
	BIGNUM *maxRange = ( BIGNUM * ) maxRangePtr;
	int status;

	assert( isWritePtr( bignum1, sizeof( BIGNUM ) ) );
	assert( isWritePtr( bignum2, sizeof( BIGNUM ) ) );
	assert( isReadPtr( buffer, length ) );
	assert( maxRange == NULL || isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES( sanityCheckBignum( bignum1 ) );
	REQUIRES( sanityCheckBignum( bignum2 ) );
	REQUIRES( minLength > 0 && minLength <= maxLength && \
			  maxLength <= CRYPT_MAX_PKCSIZE_ECC );
	REQUIRES( fieldSize >= MIN_PKCSIZE_ECC && \
			  fieldSize <= CRYPT_MAX_PKCSIZE_ECC );
	REQUIRES( checkType >= KEYSIZE_CHECK_NONE && \
			  checkType < KEYSIZE_CHECK_LAST );
	REQUIRES( sanityCheckBignum( bignum1 ) );
	REQUIRES( sanityCheckBignum( bignum2 ) );

	/* Make sure that we've been given valid input.  This should already 
	   have been checked by the caller using far more specific checks than 
	   the very generic values that we use here, but we perform the check 
	   anyway just to be sure */
	if( length < MIN_PKCSIZE_ECCPOINT_THRESHOLD || \
		length > MAX_PKCSIZE_ECCPOINT )
		return( CRYPT_ERROR_BADDATA );

	/* Decode the ECC point data.  At this point we run into another 
	   artefact of the chronic indecisiveness of the ECC standards authors, 
	   because of the large variety of ways in which ECC data can be encoded 
	   there's no single way of representing a point.  The only encoding 
	   that's actually of any practical use is the straight (x, y) 
	   coordinate form with no special-case encoding or other tricks, 
	   denoted by an 0x04 byte at the start of the data:

		+---+---------------+---------------+
		|ID	|		qx		|		qy		|
		+---+---------------+---------------+
			|<- fldSize --> |<- fldSize --> |

	   There's also a hybrid form that combines the patent encumbrance of 
	   point compression with the size of the uncompressed form that we 
	   could in theory allow for, but it's unlikely that anyone's going to
	   be crazy enough to use this */
	if( length != 1 + ( fieldSize * 2 ) )
		return( CRYPT_ERROR_BADDATA );
	if( eccPointData[ 0 ] != 0x04 )
		return( CRYPT_ERROR_BADDATA );
	eccPointData++;		/* Skip format type byte */
	status = importBignum( bignum1, eccPointData, fieldSize,
						   minLength, maxLength, maxRange, checkType );
	if( cryptStatusError( status ) )
		return( status );
	status = importBignum( bignum2, eccPointData + fieldSize, fieldSize, 
						   minLength, maxLength, maxRange, checkType );
	if( cryptStatusError( status ) )
		BN_clear( bignum1 );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 4, 5 ) ) \
int exportECCPoint( OUT_BUFFER_OPT( dataMaxLength, *dataLength ) void *data, 
					IN_LENGTH_SHORT_Z const int dataMaxLength, 
					OUT_LENGTH_BOUNDED_PKC_Z( dataMaxLength ) int *dataLength,
					IN TYPECAST( BIGNUM * ) const void *bignumPtr1, 
					IN TYPECAST( BIGNUM * ) const void *bignumPtr2,
					IN_LENGTH_PKC const int fieldSize )
	{
	BIGNUM *bignum1 = ( BIGNUM * ) bignumPtr1;
	BIGNUM *bignum2 = ( BIGNUM * ) bignumPtr2;
	BYTE *bufPtr = data;
	int length, result;

	assert( data == NULL || isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );
	assert( isReadPtr( bignum1, sizeof( BIGNUM ) ) );
	assert( isReadPtr( bignum2, sizeof( BIGNUM ) ) );

	REQUIRES( ( data == NULL && dataMaxLength == 0 ) || \
			  ( data != NULL && \
				dataMaxLength >= 16 && \
				dataMaxLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( fieldSize >= MIN_PKCSIZE_ECC && \
			  fieldSize <= CRYPT_MAX_PKCSIZE_ECC );
	REQUIRES( sanityCheckBignum( bignum1 ) );
	REQUIRES( sanityCheckBignum( bignum2 ) );

	/* Clear return values */
	if( data != NULL )
		memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	/* If it's just an encoding-length check then there's nothing to do */
	if( data == NULL )
		{
		*dataLength = 1 + ( fieldSize * 2 );
		return( CRYPT_OK );
		}

	/* Encode the ECC point data, which consists of the Q coordinates 
	   stuffed into a byte string with an encoding-specifier byte 0x04 at 
	   the start:

		+---+---------------+---------------+
		|ID	|		qx		|		qy		|
		+---+---------------+---------------+
			|<-- fldSize -> |<- fldSize --> | */
	if( dataMaxLength < 1 + ( fieldSize * 2 ) )
		return( CRYPT_ERROR_OVERFLOW );
	*bufPtr++ = 0x04;
	memset( bufPtr, 0, fieldSize * 2 );
	length = BN_num_bytes( bignum1 );
	ENSURES( length > 0 && length <= fieldSize );
	result = BN_bn2bin( bignum1, bufPtr + ( fieldSize - length ) );
	ENSURES( result == length );
	bufPtr += fieldSize;
	length = BN_num_bytes( bignum2 );
	ENSURES( length > 0 && length <= fieldSize );
	result = BN_bn2bin( bignum2, bufPtr + ( fieldSize - length ) );
	ENSURES( result == length );
	*dataLength = 1 + ( fieldSize * 2 );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH || USE_ECDSA */

#else

/****************************************************************************
*																			*
*								Non-Bignum Stubs 							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void clearTempBignums( INOUT PKC_INFO *pkcInfo )
	{
	}
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initContextBignums( INOUT PKC_INFO *pkcInfo, 
						IN_RANGE( 0, 3 ) const int sideChannelProtectionLevel )
	{
	}
STDC_NONNULL_ARG( ( 1 ) ) \
void freeContextBignums( INOUT PKC_INFO *pkcInfo, 
						 IN_FLAGS( CONTEXT ) const int contextFlags )
	{
	}
#endif /* USE_PKC */
