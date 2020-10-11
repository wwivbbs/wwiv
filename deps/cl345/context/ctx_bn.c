/****************************************************************************
*																			*
*						cryptlib Bignum Support Routines					*
*						Copyright Peter Gutmann 1995-2016					*
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
   runtime.  To deal with this we define a conditional value that can be used 
   to control printing of output.  In addition where possible the diagnostic 
   code itself tries to minimise the conditions under which it produces 
   output */

#if !defined( NDEBUG ) && defined( USE_ERRMSGS )
static const BOOLEAN diagOutput = FALSE;
#endif /* Debug build with USE_ERRMSGS */

/* If we're not using dynamically-allocated bignums then we need to convert 
   the bn_expand() macros that are used throughout the bignum code into 
   no-ops.  The following value represents a non-null location that can be
   used in the bn_expand() macros */

#ifndef BN_ALLOC
int nonNullAddress;
#endif /* BN_ALLOC */

/* If we're debugging the bignum allocation code then the clBnAlloc() macro
   points to the following function */

#ifdef USE_BN_DEBUG_MALLOC

void *clBnAllocFn( const char *fileName, const char *fnName,
				   const int lineNo, size_t size )
	{
	printf( "BNDEBUG: %s:%s:%d %lu bytes.\n", fileName, fnName, 
			lineNo, size );
	return( malloc( size ) );
	}
#endif /* USE_BN_DEBUG_MALLOC */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Make sure that a bignum/BN_CTX's metadata is valid */

CHECK_RETVAL_LENGTH_SHORT_NOERROR STDC_NONNULL_ARG( ( 1 ) ) \
int getBNMaxSize( const BIGNUM *bignum )
	{
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	return( ( bignum->flags & BN_FLG_ALLOC_EXT ) ? BIGNUM_ALLOC_WORDS_EXT : \
			( bignum->flags & BN_FLG_ALLOC_EXT2 ) ? BIGNUM_ALLOC_WORDS_EXT2 : \
			BIGNUM_ALLOC_WORDS );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckBignum( const BIGNUM *bignum )
	{
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	if( bignum->top < 0 || bignum->top > getBNMaxSize( bignum ) )
		return( FALSE );
	if( bignum->neg != TRUE && bignum->neg != FALSE )
		return( FALSE );
	if( bignum->flags < BN_FLG_NONE || bignum->flags > BN_FLG_MAX )
		return( FALSE );
#ifndef NDEBUG
	if( !( bignum->flags & BN_FLG_SCRATCH ) )
		{
		int i, LOOP_ITERATOR;

		LOOP_LARGE( i = bignum->top, i < getBNMaxSize( bignum ), i++ )
			assert( bignum->d[ i ] == 0 );
		ENSURES_B( LOOP_BOUND_OK );
		}
#endif /* Debug mode */

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckBNCTX( const BN_CTX *bnCTX )
	{
	assert( isReadPtr( bnCTX, sizeof( BN_CTX ) ) );

	if( bnCTX->bnArrayMax < 0 || bnCTX->bnArrayMax > BN_CTX_ARRAY_SIZE )
		return( FALSE );
	if( bnCTX->stackPos < 0 || bnCTX->stackPos >= BN_CTX_ARRAY_SIZE )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckBNMontCTX( const BN_MONT_CTX *bnMontCTX )
	{
	assert( isReadPtr( bnMontCTX, sizeof( bnMontCTX ) ) );

	if( !sanityCheckBignum( &bnMontCTX->R ) || \
		!sanityCheckBignum( &bnMontCTX->N ) )
		return( FALSE );
	if( bnMontCTX->flags != 0 && bnMontCTX->flags != BN_FLG_MALLOCED )
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

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_clear( INOUT BIGNUM *bignum )
	{
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_V( sanityCheckBignum( bignum ) );

	if( !( bignum->flags & BN_FLG_STATIC_DATA ) )
		{
		DEBUG_PRINT_COND( diagOutput && bignum->top > 64, \
						  ( "BN max.size = %d words.\n", bignum->top ) );
		zeroise( bignum->d, bnWordsToBytes( getBNMaxSize( bignum ) ) );
		bignum->top = bignum->neg = 0;
		bignum->flags &= ~( BN_FLG_CONSTTIME | BN_FLG_SCRATCH );
		}
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_init( OUT BIGNUM *bignum )
	{
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	memset( bignum, 0, sizeof( BIGNUM ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void BN_init_ext( INOUT void *bignumExtPtr, 
						 const BOOLEAN isExt2Bignum )
	{
	assert( isWritePtr( bignumExtPtr, sizeof( BIGNUM_EXT ) ) );

	REQUIRES_V( isExt2Bignum == TRUE || isExt2Bignum == FALSE );

	if( isExt2Bignum )
		{
		BIGNUM_EXT2 *bignum = bignumExtPtr;

		memset( bignum, 0, sizeof( BIGNUM_EXT2 ) );
		bignum->flags = BN_FLG_ALLOC_EXT2;
		}
	else
		{
		BIGNUM_EXT *bignum = bignumExtPtr;

		memset( bignum, 0, sizeof( BIGNUM_EXT ) );
		bignum->flags = BN_FLG_ALLOC_EXT;
		}
	}

CHECK_RETVAL_PTR \
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

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_free( INOUT BIGNUM *bignum )
	{
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	BN_clear( bignum );
	if( bignum->flags & BN_FLG_MALLOCED )
		clFree( "BN_free", bignum );
	}

/* Duplicate, swap bignums */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
BIGNUM *BN_dup( const BIGNUM *bignum )
	{
	BIGNUM *newBignum;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	newBignum = BN_new();
	if( newBignum == NULL ) 
		return( NULL );
	if( BN_copy( newBignum, bignum ) == NULL )
		{
		BN_free( newBignum );

		return( NULL );
		}

	return( newBignum );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
BIGNUM *BN_copy( INOUT BIGNUM *destBignum, const BIGNUM *srcBignum )
	{
	assert( isWritePtr( destBignum, sizeof( BIGNUM ) ) );
	assert( isReadPtr( srcBignum, sizeof( BIGNUM ) ) );

	REQUIRES_N( destBignum != srcBignum );
	REQUIRES_N( sanityCheckBignum( destBignum ) );
	REQUIRES_N( sanityCheckBignum( srcBignum ) );
	REQUIRES_N( getBNMaxSize( destBignum ) >= srcBignum->top );

	/* Clear out the destination bignum, a general sanitation measure in 
	   case there's un-cleared data left in it from previous operations */
	BN_clear( destBignum );

	/* Copy most of the bignum fields.  We don't copy the maximum-size field
	   or anything but a subset of the flags field since these may differ 
	   for the two bignums (the flags field will be things like 
	   BN_FLG_MALLOCED, BN_FLG_STATIC_DATA, and BN_FLG_ALLOC_EXT) */
	memcpy( destBignum->d, srcBignum->d, bnWordsToBytes( srcBignum->top ) );
	destBignum->flags |= srcBignum->flags & BN_FLG_COPY_MASK;
	destBignum->top = srcBignum->top;
	destBignum->neg = srcBignum->neg;

	return( destBignum );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void BN_swap( INOUT BIGNUM *bignum1, INOUT BIGNUM *bignum2 )
	{
	BIGNUM tmp;
	int bnStatus = BN_STATUS;

	assert( isWritePtr( bignum1, sizeof( BIGNUM ) ) );
	assert( isWritePtr( bignum2, sizeof( BIGNUM ) ) );

	REQUIRES_V( bignum1 != bignum2 );
	REQUIRES_V( !( bignum1->flags & BN_FLG_STATIC_DATA ) );
	REQUIRES_V( !( bignum1->flags & BN_FLG_STATIC_DATA ) );

	BN_init( &tmp );
	CKPTR( BN_copy( &tmp, bignum1 ) );
	CKPTR( BN_copy( bignum1, bignum2 ) );
	CKPTR( BN_copy( bignum2, &tmp ) );
	BN_clear( &tmp );

	ENSURES_V( bnStatusOK( bnStatus ) );
	}

/* Get a bignum with the value 1 */

CHECK_RETVAL_PTR \
const BIGNUM *BN_value_one( void )
	{
	static const BIGNUM bignum = { 1, FALSE, BN_FLG_STATIC_DATA, 
								   { 1, 0, 0, 0 } };

	/* Catch problems arising from changes to bignum struct layout */
	assert( bignum.top == 1 );
	assert( bignum.neg == FALSE );
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

STDC_NONNULL_ARG( ( 1 ) ) \
BN_ULONG BN_get_word( const BIGNUM *bignum )
	{
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_EXT( sanityCheckBignum( bignum ), BN_NAN );

	/* If the result won't fit in a word, return a NaN indicator */
	if( bignum->top > 1 )
		return( BN_NAN );

	/* Bignums with the value zero have a length of zero so we don't try and
	   read a data value from them */
	if( bignum->top < 1 )
		return( 0 );

	return( bignum->d[ 0 ] );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_set_word( INOUT BIGNUM *bignum, const BN_ULONG word )
	{
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( bignum ) );
	REQUIRES_B( !( bignum->flags & BN_FLG_STATIC_DATA ) );

	BN_clear( bignum );
	bignum->d[ 0 ] = word;
	bignum->top = word ? 1 : 0;

	return( TRUE );
	}

/* Count the number of bits used in a word and in a bignum.  The former is 
   the classic log2 problem for which there are about a million clever hacks 
   (including stuffing them into IEEE-754 64-bit floats and fiddling with 
   the bit-representation of those) but they're all endianness/word-size/
   whatever-dependent and since this is never called in time-critical code 
   we just use a straight loop, which works everywhere */

CHECK_RETVAL_LENGTH_SHORT \
int BN_num_bits_word( const BN_ULONG word )
	{
	BN_ULONG value = word;
	int i, LOOP_ITERATOR;

	LOOP_LARGE( i = 0, i < 128 && value > 0, i++ )
		value >>= 1;
	ENSURES( LOOP_BOUND_OK );
	ENSURES( i < 128 );

	return( i );
	}

CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1 ) ) \
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

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_set_bit( INOUT BIGNUM *bignum, 
					IN_RANGE( 0, bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
						int bitNo )
	{
	const int wordIndex = bitNo / BN_BITS2;
	const int bitIndex = bitNo % BN_BITS2;

	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( bignum ) );
	REQUIRES_B( !( bignum->flags & BN_FLG_STATIC_DATA ) );
	REQUIRES_B( bitNo >= 0 && \
				bitNo < bnWordsToBits( getBNMaxSize( bignum ) ) );

	/* If we're extending the bignum, clear the words up to where we insert 
	   the bit.
	   
	   Note that the use of the unified BIGNUM type to also represent a 
	   BIGNUM_EXT/BIGNUM_EXT2 can result in false-positive warnings from 
	   bounds-checking applications that apply the d[] array size from a 
	   BIGNUM to the much larger array in a BIGNUM_EXT/BIGNUM_EXT2 */
	if( bignum->top < wordIndex + 1 )
		{
		const int iterationBound = getBNMaxSize( bignum );
		int index, LOOP_ITERATOR;

		REQUIRES_B( wordIndex < getBNMaxSize( bignum ) );
		LOOP_EXT( index = bignum->top, index < wordIndex + 1, index++, 
				  iterationBound )
			{
			bignum->d[ index ] = 0;
			}
		ENSURES_B( LOOP_BOUND_OK );
		bignum->top = wordIndex + 1;
		}

	/* Set the appropriate bit location */
	bignum->d[ wordIndex ] |= ( BN_ULONG ) 1 << bitIndex;

	ENSURES_B( sanityCheckBignum( bignum ) );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_is_bit_set( const BIGNUM *bignum, /* See comment */ int bitNo )
	{
	const int wordIndex = bitNo / BN_BITS2;
	const int bitIndex = bitNo % BN_BITS2;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( bignum ) );
	REQUIRES_B( bitNo < bnWordsToBits( getBNMaxSize( bignum ) ) );
				/* See comment below */

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

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_high_bit( const BIGNUM *bignum )
	{
	int noBytes = BN_num_bytes( bignum ) - 1;
	BN_ULONG highWord;
	int highByte;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( bignum ) );

	/* Bignums with value zero are special-cased since they have a length of
	   zero */
	if( noBytes < 0 )
		return( 0 );

	/* Extract the topmost nonzero byte in the bignum */
	highWord = bignum->d[ noBytes / BN_BYTES ];
	highByte = ( int ) ( highWord >> ( ( noBytes % BN_BYTES ) * 8 ) );

	return( ( highByte & 0x80 ) ? 1 : 0 );
	}

/* Set the sign flag on a bignum.  This is almost universally ignored by the
   OpenSSL code, which manipulates the sign value directly */

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_set_negative( INOUT BIGNUM *bignum, const int value )
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

RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_normalise( INOUT BIGNUM *bignum )
	{
	const int iterationBound = getBNMaxSize( bignum );
	int LOOP_ITERATOR;

	REQUIRES_B( sanityCheckBignum( bignum ) );

	/* If it's a zero-magnitude bignum then there's nothing to do */
	if( BN_is_zero( bignum ) )
		return( TRUE );

	/* Note that the use of the unified BIGNUM type to also represent a 
	   BIGNUM_EXT/BIGNUM_EXT2 can result in false-positive warnings from 
	   bounds-checking applications that apply the d[] array size from a 
	   BIGNUM to the much larger array in a BIGNUM_EXT/BIGNUM_EXT2 */
	LOOP_EXT_CHECKINC( bignum->top > 0, bignum->top--, iterationBound )
		{
		if( bignum->d[ bignum->top - 1 ] != 0 )
			break;
		}
	ENSURES_B( LOOP_BOUND_OK );

	ENSURES_B( sanityCheckBignum( bignum ) );

	return( TRUE );
	}

/* When we're working with a bignum's internal data, we may end up reducing 
   its magnitude, typically via an operation like BN_op( out, in1, in2 ) 
   where 'out' contained a previous value larger than 'in1 op in2'.  In
   order to deal with this we need to clear any leftover data that wasn't
   replaced by the operation being performed */

RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_clear_top( INOUT BIGNUM *bignum, 
					  IN_RANGE( 0, BIGNUM_ALLOC_WORDS_EXT2 ) const int oldTop )
	{
	const int iterationBound = getBNMaxSize( bignum );
	int i, LOOP_ITERATOR;

	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );

	/* We can't call sanityCheckBignum() at this point because there's
	   potentially leftover data beyond bignum->top, which is the whole
	   reason why this function is being called in the first place */
	REQUIRES_B( oldTop >= 0 && oldTop <= getBNMaxSize( bignum ) );

	/* If we've overwritten any previous contents, we're done */
	if( oldTop <= bignum->top )
		return( TRUE );

	/* Clear any previous bignum data content */
	LOOP_EXT( i = bignum->top, i < oldTop, i++, iterationBound )
		{
		bignum->d[ i ] = 0;
		}
	ENSURES_B( LOOP_BOUND_OK );

	ENSURES_B( sanityCheckBignum( bignum ) );

	return( TRUE );
	}

/****************************************************************************
*																			*
*							BN_CTX Support Routines 						*
*																			*
****************************************************************************/

/* The BN_CTX code up until about 2000 (or cryptlib 3.21) used to be just an 
   array of BN_CTX_NUM = 32 BIGNUMs.  After that it was replaced by an 
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
   second frees up the ones grabbed in BN_foo().  This is why we have the
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
void BN_CTX_init( OUT BN_CTX *bnCTX )
	{
	int i, LOOP_ITERATOR;

	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	memset( bnCTX, 0, sizeof( BN_CTX ) );
	LOOP_MED( i = 0, i < BN_CTX_ARRAY_SIZE, i++ )
		BN_init( &bnCTX->bnArray[ i ] );
	ENSURES_V( LOOP_BOUND_OK );
	ENSURES_V( i == BN_CTX_ARRAY_SIZE );
	LOOP_MED( i = 0, i < BN_CTX_EXTARRAY_SIZE, i++ )
		BN_init_ext( &bnCTX->bnExtArray[ i ], FALSE );
	ENSURES_V( LOOP_BOUND_OK );
	ENSURES_V( i == BN_CTX_EXTARRAY_SIZE );
	LOOP_MED( i = 0, i < BN_CTX_EXT2ARRAY_SIZE, i++ )
		BN_init_ext( &bnCTX->bnExt2Array[ i ], TRUE );
	ENSURES_V( LOOP_BOUND_OK );
	ENSURES_V( i == BN_CTX_EXT2ARRAY_SIZE );
	ENSURES_V( sanityCheckBNCTX( bnCTX ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_final( INOUT BN_CTX *bnCTX )
	{
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_V( sanityCheckBNCTX( bnCTX ) );

	/* Clear the overall BN_CTX */
	zeroise( bnCTX, sizeof( BN_CTX ) );

	/* The various bignums were cleared when the BN_CTX was zeroised, we now 
	   have to reset them to their initial state so that they can be 
	   reused */
	BN_CTX_init( bnCTX );
	DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL1 freed.\nEXT_MUL2 freed.\n" ));
	DEBUG_PRINT_COND( diagOutput, ( "EXT_MONT freed.\n" ));
	}

/* The start and badly-named end functions (it should be finish()) remember 
   the current stack position and unwind to the last stack position */

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_start( INOUT BN_CTX *bnCTX )
	{
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_V( sanityCheckBNCTX( bnCTX ) );

	/* Advance one stack frame */
	bnCTX->stackPos++;
	bnCTX->stack[ bnCTX->stackPos ] = bnCTX->stack[ bnCTX->stackPos - 1 ];

	ENSURES_V( sanityCheckBNCTX( bnCTX ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_end( INOUT BN_CTX *bnCTX )
	{
	int i, LOOP_ITERATOR;

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

	/* Clear each bignum in the current stack frame.  We perform the sanity 
	   check as an assert() rather than an ENSURES_V() because we want to 
	   catch programming errors resulting in random garbage being left in
	   bignums but don't want to abort the BN_CTX cleanup in release code 
	   due to a random bit flip */
	LOOP_EXT( i = bnCTX->stack[ bnCTX->stackPos - 1 ],
			  i < bnCTX->stack[ bnCTX->stackPos ], i++, BN_CTX_ARRAY_SIZE )
		{
		assert( sanityCheckBignum( &bnCTX->bnArray[ i ] ) );
		BN_clear( &bnCTX->bnArray[ i ] );
		}
	ENSURES_V( LOOP_BOUND_OK );

	/* Unwind the stack by one frame */
	bnCTX->stack[ bnCTX->stackPos ] = 0;
	bnCTX->stackPos--;

	ENSURES_V( sanityCheckBNCTX( bnCTX ) );
	}

/* Peel another bignum off the BN_CTX array */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
BIGNUM *BN_CTX_get( INOUT BN_CTX *bnCTX )
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
	ENSURES_N( sanityCheckBignum( bignum ) && BN_is_zero( bignum ) );

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

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
BIGNUM *BN_CTX_get_ext( INOUT BN_CTX *bnCTX, 
						IN_ENUM( BIGNUM_EXT ) const BIGNUM_EXT_TYPE bnExtType )
	{
	BIGNUM *bignum = NULL;

	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_N( isEnumRange( bnExtType, BIGNUM_EXT ) );

	switch( bnExtType )
		{
		case BIGNUM_EXT_MONT:
			DEBUG_PRINT_COND( diagOutput, ( "EXT_MONT acquired.\n" ));
			bignum = ( BIGNUM * ) &bnCTX->bnExtArray[ 0 ];
			break;

		case BIGNUM_EXT_MUL1:
			DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL1 acquired.\n" ));
			bignum = ( BIGNUM * ) &bnCTX->bnExt2Array[ 0 ];
			break;

		case BIGNUM_EXT_MUL2:
			DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL2 acquired.\n" ));
			bignum = ( BIGNUM * ) &bnCTX->bnExt2Array[ 1 ];
			break;
		}
	ENSURES_N( bignum != NULL );
	ENSURES_N( sanityCheckBignum( bignum ) );
			   /* We can't also check that BN_is_zero( bignum ) because we 
			      may be getting it for the purpose of clearing it, from 
				  BN_CTX_end_ext() */

	return( bignum );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_end_ext( INOUT BN_CTX *bnCTX, 
					 IN_ENUM( BIGNUM_EXT ) const BIGNUM_EXT_TYPE bnExtType )
	{
	BIGNUM *bignum;

	/* Perform the standard context cleanup */
	BN_CTX_end( bnCTX );

	ENSURES_V( bnExtType == BIGNUM_EXT_MUL1 || \
			   bnExtType == BIGNUM_EXT_MONT );

	/* Clear the extended-size bignums */
	if( bnExtType == BIGNUM_EXT_MUL1 )
		{
		bignum = BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MUL1 );
		ENSURES_V( bignum != NULL );
		BN_clear( bignum );
		DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL1 cleared.\n" ));
		bignum = BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MUL2 );
		ENSURES_V( bignum != NULL );
		BN_clear( bignum );
		DEBUG_PRINT_COND( diagOutput, ( "EXT_MUL2 cleared.\n" ));
		}
	else
		{
		bignum = BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MONT );
		ENSURES_V( bignum != NULL );
		BN_clear( bignum );
		DEBUG_PRINT_COND( diagOutput, ( "EXT_MONT cleared.\n" ));
		}
	}

/* Dynamically allocate a BN_CTX, only needed by the ECC code */

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL_PTR \
BN_CTX *BN_CTX_new( void )
	{
	BN_CTX *bnCTX;

	bnCTX = clAlloc( "BN_CTX_new", sizeof( BN_CTX ) );
	if( bnCTX == NULL )
		return( NULL );
	BN_CTX_init( bnCTX );
	ENSURES_N( sanityCheckBNCTX( bnCTX ) );

	return( bnCTX );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_free( INOUT BN_CTX *bnCTX )
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

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_MONT_CTX_init( OUT BN_MONT_CTX *bnMontCTX )
	{
	assert( isWritePtr( bnMontCTX, sizeof( BN_MONT_CTX ) ) );

	memset( bnMontCTX, 0, sizeof( BN_MONT_CTX ) );
	BN_init( &bnMontCTX->R );
	BN_init( &bnMontCTX->N );

	ENSURES_V( sanityCheckBNMontCTX( bnMontCTX ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_MONT_CTX_free( INOUT BN_MONT_CTX *bnMontCTX )
	{
	assert( isWritePtr( bnMontCTX, sizeof( BN_MONT_CTX ) ) );

	/* We perform the sanity check as an assert() rather than an ENSURES_V() 
	   because we want to catch programming errors resulting in random 
	   garbage being left in bignums but don't want to abort the BN_MONT_CTX 
	   cleanup in release code due to a random bit flip */
	assert( sanityCheckBNMontCTX( bnMontCTX ) );

	BN_clear( &bnMontCTX->R );
	BN_clear( &bnMontCTX->N );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( bnMontCTX->flags & BN_FLG_MALLOCED )
		clFree( "BN_MONT_CTX_free", bnMontCTX );
#endif /* ECDH || ECDSA */
	}

/* Set parameters for a Montgomery context.  This computes the constant R 
   used to convert a bignum into the Montgomery form aR mod N and records
   R and N */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_MONT_CTX_set( INOUT BN_MONT_CTX *bnMontCTX, const BIGNUM *mod, 
						 INOUT BN_CTX *bnCTX )
	{
	BIGNUM *R, *modWord;
	const int Nbits = roundUp( BN_num_bits( mod ), BN_BITS2 );
	const int flags = bnMontCTX->flags;
	int bnStatus = BN_STATUS;

	assert( isWritePtr( bnMontCTX, sizeof( BN_MONT_CTX ) ) );
	assert( isReadPtr( mod, sizeof( BIGNUM ) ) );
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_B( sanityCheckBignum( mod ) && !BN_is_zero( mod ) && \
				!BN_is_negative( mod ) );
	REQUIRES_B( sanityCheckBNCTX( bnCTX ) );
	REQUIRES_B( BN_cmp( &bnMontCTX->N, mod ) );	/* Ensure not already set */

	/* Clear the Montgomery context entries and record the modulus.  We need
	   to preserve the flags around the init since this records details such
	   as whether the context is dynamically allocated */
	BN_MONT_CTX_init( bnMontCTX );
	bnMontCTX->flags = flags;
	CKPTR( BN_copy( &bnMontCTX->N, mod ) );
	if( bnStatusError( bnStatus ) )
		return( FALSE );

	BN_CTX_start( bnCTX );

	/* Get the temporaries that we'll be working with.  We borrow the 
	   modWord bignum from the bnMontCTX since we won't be setting it until 
	   right at the end */
	R = BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MONT );
	if( R == NULL )
		{
		BN_CTX_end( bnCTX );
		return( FALSE );
		}
	modWord = &bnMontCTX->R;

	/* Set up the auxiliary modulus R as a value one larger than a bignum 
	   word and modWord as a bignum containing the low word of the modulus 
	   (this fulfils the requirement that R > N, or in this case 
	   R > modWord).  
	   
	   modWord can't be zero, or more generally even otherwise the modulus 
	   would be composite, we make the check explicit otherwise the 
	   following modular inverse operation won't work */
	CK( BN_zero( R ) );
	CK( BN_set_bit( R, BN_BITS2 ) );
	CK( BN_set_word( modWord, mod->d[ 0 ] ) );
	if( bnStatusError( bnStatus ) )
		{
		BN_CTX_end( bnCTX );
		return( FALSE );
		}
	ENSURES_B( BN_is_odd( modWord ) );
	if( BN_is_one( modWord ) )
		{
		/* When modWord is 1, the modular inverse is zero, so instead we 
		   have to explicitly set R to all one bits */
		CK( BN_set_word( R, BN_MASK2 ) );
		}
	else
		{
		/* R = R^-1 mod modWord, shifted left one word (i.e. multiplied by 
		   the initial R value) to make word-based divides easier */
		CKPTR( BN_mod_inverse( R, R, modWord, bnCTX ) );
		ENSURES_B( !BN_is_zero( R ) );
		CK( BN_lshift( R, R, BN_BITS2 ) );
		CK( BN_sub_word( R, 1 ) );
		CK( BN_div( R, NULL, R, modWord, bnCTX ) );
		}
	if( bnStatusError( bnStatus ) )
		{
		BN_CTX_end( bnCTX );
		return( FALSE );
		}

	/* Record the least significant word of R */
	bnMontCTX->n0 = R->d[ 0 ];

	/* Set up the value R used for conversion to aR mod N form.  This 
	   temporarily expands the value to an extremely large size so it's done 
	   via an extended bignum */
	CK( BN_zero( R ) );
	CK( BN_set_bit( R, Nbits * 2 ) );
	CK( BN_mod( &bnMontCTX->R, R, &bnMontCTX->N, bnCTX ) );

	BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MONT );
	if( bnStatusError( bnStatus ) )
		return( FALSE );

	ENSURES_B( sanityCheckBNMontCTX( bnMontCTX ) );

	return( TRUE );
	}

/* Convert values to/from Montgomery form.  Note that BN_from_montgomery()
   modifies its input value as part of the conversion process, this is done
   because it saves allocating a temporary bignum only to throw it away 
   again as soon as the conversion is complete, and is safe because the only
   values passed to it are themselves temporaries output from other
   calculations */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_to_montgomery( INOUT BIGNUM *ret, const BIGNUM *a,
						  const BN_MONT_CTX *bnMontCTX,
						  INOUT BN_CTX *bnCTX )
	{
	assert( isWritePtr( ret, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( bnMontCTX, sizeof( BN_MONT_CTX ) ) );
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	/* This is just a wrapper for BN_mod_mul_montgomery() */
	return( BN_mod_mul_montgomery( ret, a, &bnMontCTX->R, bnMontCTX, 
								   bnCTX ) );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_from_montgomery( INOUT BIGNUM *ret, INOUT BIGNUM *aTmp,
							const BN_MONT_CTX *bnMontCTX,
							INOUT BN_CTX *bnCTX )
	{
	const BIGNUM *N = &bnMontCTX->N;
	BIGNUM *aTmpExt = NULL;
	BN_ULONG *aData, carry = 0;
	const int oldTop = ret->top, nLen = N->top;
	const int iterationBound = getBNMaxSize( N );
	int bnStatus = BN_STATUS, i, LOOP_ITERATOR;

	assert( isWritePtr( ret, sizeof( BIGNUM ) ) );
	assert( isReadPtr( aTmp, sizeof( BIGNUM ) ) );
	assert( isReadPtr( bnMontCTX, sizeof( BN_MONT_CTX ) ) );
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_B( sanityCheckBignum( aTmp ) && !BN_is_zero( aTmp ) && \
				!BN_is_negative( aTmp ) );
	REQUIRES_B( ret != aTmp );
	REQUIRES_B( sanityCheckBNMontCTX( bnMontCTX ) );
	REQUIRES_B( sanityCheckBNCTX( bnCTX ) );

	/* If the values get very large then we need to use an extended bignum 
	   as a temporary */
	if( nLen * 2 > getBNMaxSize( aTmp ) )
		{
		BN_CTX_start( bnCTX );
		aTmpExt = BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MONT );
		if( aTmpExt == NULL || BN_copy( aTmpExt, aTmp ) == NULL )
			{
			BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MONT );
			return( FALSE );
			}
		aTmp = aTmpExt;
		}
	BN_set_flags( aTmp, BN_FLG_SCRATCH );
	aData = aTmp->d;

	/* Perform the same operation that's used in BN_sqr() (see the long 
	   comment there for an explanation of what's going on), but in modified 
	   form to make it constant-time */
	LOOP_EXT( i = 0, i < nLen, i++, iterationBound )
		{
		const BN_ULONG aDataVal = aData[ nLen + i ];
		BN_ULONG tmp;

		tmp = bn_mul_add_words( aData + i, N->d, nLen, 
								aData[ i ] * bnMontCTX->n0 ) + carry + aDataVal;
		aData[ nLen + i ] = tmp;

		/* Compute the carry for the next round.  The convoluted logic 
		   expression is required in order to perform the operation in a
		   branch-free manner if possible (depending on what the compiler 
		   does for code generation */
		carry = ( carry | ( tmp != aDataVal ) ) & ( tmp <= aDataVal );
		}
	ENSURES_B( LOOP_BOUND_OK );
	ret->top = nLen;

	/* Perform the final transformation using a constant-time operation in 
	   which, if there's a borrow due to the subtraction, we copy out the 
	   computed result, otherwise we perform a dummy copy of the same data 
	   into an unused memory location */
	if( bn_sub_words( ret->d, aData + nLen, N->d, nLen ) - carry != 0 )
		{
		/* There was a borrow, perform the actual copy */
		memcpy( ret->d, aData + nLen, bnWordsToBytes( nLen ) );
		}
	else
		{
		/* Perform a dummy copy that takes the same time as the real one */
		memcpy( aData, aData + nLen, bnWordsToBytes( nLen ) );
		}
	CK( BN_clear_top( ret, oldTop ) );
	CK( BN_normalise( ret ) );
	if( bnStatusError( bnStatus ) )
		return( FALSE );

	BN_clear( aTmp );
	if( aTmpExt != NULL )
		BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MONT );

	ENSURES_B( sanityCheckBignum( ret ) );

	return( TRUE );
	}

/* Dynamically allocate a BN_MONT_CTX, only needed by the ECC code */

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL_PTR \
BN_MONT_CTX *BN_MONT_CTX_new( void )
	{
	BN_MONT_CTX *bnMontCTX;

	bnMontCTX = clAlloc( "BN_MONT_CTX_new", sizeof( BN_MONT_CTX ) );
	if( bnMontCTX == NULL )
		return( NULL );
	BN_MONT_CTX_init( bnMontCTX );
	bnMontCTX->flags = BN_FLG_MALLOCED;
	ENSURES_N( sanityCheckBNMontCTX( bnMontCTX ) );

	return( bnMontCTX );
	}
#else

CHECK_RETVAL_PTR \
BN_MONT_CTX *BN_MONT_CTX_new( void )
	{
	assert( DEBUG_WARN );
	return( NULL );
	}
#endif /* ECDH || ECDSA */

/****************************************************************************
*																			*
*							BN_RECP_CTX Support Routines 					*
*																			*
****************************************************************************/

/* Initialise/clear a BN_RECP_CTX */

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_RECP_CTX_init( OUT BN_RECP_CTX *bnRecpCTX )
	{
	assert( isWritePtr( bnRecpCTX, sizeof( BN_RECP_CTX ) ) );

	memset( bnRecpCTX, 0, sizeof( BN_RECP_CTX ) );

	BN_init( &bnRecpCTX->N );
	BN_init( &bnRecpCTX->Nr );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_RECP_CTX_free( INOUT BN_RECP_CTX *bnRecpCTX )
	{
	assert( isWritePtr( bnRecpCTX, sizeof( BN_RECP_CTX ) ) );

	/* We perform the sanity check as an assert() rather than an ENSURES_V() 
	   because we want to catch programming errors resulting in random 
	   garbage being left in bignums but don't want to abort the BN_CTX 
	   cleanup in release code  due to a random bit flip */
	assert( sanityCheckBignum( &bnRecpCTX->N ) );
	assert( sanityCheckBignum( &bnRecpCTX->Nr ) );

	BN_clear( &bnRecpCTX->N );
	BN_clear( &bnRecpCTX->Nr );
	}

/* Initialise a BN_RECP_CTX.  The BN_CTX isn't used for anything, we keep it
   just to preserve the original function signature */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_RECP_CTX_set( INOUT BN_RECP_CTX *bnRecpCTX, const BIGNUM *d, 
						 STDC_UNUSED const BN_CTX *bnCTX )
	{
	int bnStatus = BN_STATUS;

	assert( isWritePtr( bnRecpCTX, sizeof( BN_RECP_CTX ) ) );
	assert( isReadPtr( d, sizeof( BIGNUM ) ) );

	UNUSED_ARG( bnCTX );

	/* Clear context fields.  This should already have been done through an 
	   earlier call to BN_RECP_CTX_init(), but given that this is OpenSSL, 
	   we're extra conservative */
	BN_RECP_CTX_init( bnRecpCTX );

	/* N = bignum, Nr = 0 */
	CKPTR( BN_copy( &bnRecpCTX->N, d ) );
	CK( BN_zero( &bnRecpCTX->Nr ) );
	if( bnStatusError( bnStatus ) )
		return( FALSE );

	/* Initialise metadata fields */
	bnRecpCTX->num_bits = BN_num_bits( d );

	return( TRUE );
	}

/****************************************************************************
*																			*
*								Self-test Routines							*
*																			*
****************************************************************************/

#ifndef CONFIG_CONSERVE_MEMORY_EXTRA

CHECK_RETVAL_BOOL \
BOOLEAN testIntBN( void )
	{
	if( !bnmathSelfTest() )
		return( FALSE );

	return( TRUE );
	}
#endif /* CONFIG_CONSERVE_MEMORY_EXTRA */

#endif /* USE_PKC */
