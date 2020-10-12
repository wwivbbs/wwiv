/****************************************************************************
*																			*
*						cryptlib Bignum Maths Routines						*
*						Copyright Peter Gutmann 1995-2017					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "bn_lcl.h"
  #include "context.h"
#else
  #include "crypt.h"
  #include "bn/bn_lcl.h"	/* For macros used in BN_div() */
  #include "context/context.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKC

/****************************************************************************
*																			*
*								Add/Subtract Bignums						*
*																			*
****************************************************************************/

/* Add and subtract two bignums, r = a + b, r = a - b */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_uadd( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b )
	{
	BN_ULONG carry;
	const int oldTop = r->top;
	int length = max( a->top, b->top );

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( b, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) );
	REQUIRES_B( sanityCheckBignum( b ) );

	/* Add the two values, propagating the carry if required */
	carry = bn_add_words( r->d, a->d, b->d, length );
	if( carry )
		r->d[ length++ ] = 1;
	r->top = length;
	BN_set_negative( r, FALSE );
	BN_clear_top( r, oldTop );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

/* Subtract two bignums, r = a - b */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_usub( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b )
	{
	BN_ULONG carry;
	const int oldTop = r->top;
	int length = max( a->top, b->top ), bnStatus = BN_STATUS;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( b, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) );
	REQUIRES_B( sanityCheckBignum( b ) );
	REQUIRES_B( BN_cmp( a, b ) >= 0 );

	/* Subtract the two values.  The carry should be zero since a >= b */
	carry = bn_sub_words( r->d, a->d, b->d, length );
	ENSURES_B( !carry );
	r->top = length;
	BN_set_negative( r, FALSE );
	BN_clear_top( r, oldTop );

	/* The subtraction may have reduced the size of the resulting value so 
	   we have to normalise it before we return */
	CK( BN_normalise( r ) );
	if( bnStatusError( bnStatus ) )
		return( FALSE );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

/* Signed versions of the above: r = a + b, r = a - b.  These are just 
   wrappers around BN_uadd()/BN_usub() that deal with sign bits as
   appropriate */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_add( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b )
	{
	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( b, sizeof( BIGNUM ) ) );

	/* a can be negative via the BN_mod_inverse() used in Montgomery 
	   ops */
	REQUIRES_B( sanityCheckBignum( a ) );
	REQUIRES_B( sanityCheckBignum( b ) && !BN_is_negative( b ) );

	/* If a is negative then -a + b becomes b - a.  We can't pass this down 
	   to BN_sub() because that expects positive numbers so we have to 
	   handle the special case here, but in any case it's straightforward
	   because abs( a ) <= b so the result is always a positive number */
	if( BN_is_negative( a ) ) 
		{
		REQUIRES_B( BN_ucmp( a, b ) <= 0 ); 

		if( !BN_usub( r, b, a ) )
			return( FALSE );
		BN_set_negative( r, FALSE );

		return( TRUE );
		}

	/* It's a straight add */
	return( BN_uadd( r, a, b ) );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_sub( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b )
	{
	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( b, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_negative( a ) );
	REQUIRES_B( sanityCheckBignum( b ) && !BN_is_negative( b ) );

	/* If a < b then the result is the difference as a negative number */
	if( BN_ucmp( a, b ) < 0 ) 
		{
		if( !BN_usub( r, b, a ) )
			return( FALSE );
		BN_set_negative( r, TRUE );

		return( TRUE );
		}

	/* It's a straight subtract */
	return( BN_usub( r, a, b ) );
	}

/****************************************************************************
*																			*
*								Shift Bignums								*
*																			*
****************************************************************************/

/* Shift a bignum left or right */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN BN_lshift( INOUT BIGNUM *r, const BIGNUM *a, 
				   IN_RANGE( 0, bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
						const int shiftAmount )
	{
	const int wordShiftAmount = shiftAmount / BN_BITS2;
	const int bitShiftAmount = shiftAmount % BN_BITS2;
	const int bitShiftRemainder = BN_BITS2 - bitShiftAmount;
	const BN_ULONG *aData = a->d;
	const int oldTop = r->top;
	const int iterationBound = getBNMaxSize( a );
	BN_ULONG *rData = r->d;
	int i, LOOP_ITERATOR;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) );
	REQUIRES_B( shiftAmount > 0 && \
				shiftAmount < bytesToBits( CRYPT_MAX_PKCSIZE ) );
	REQUIRES_B( a->top + wordShiftAmount < getBNMaxSize( r ) );

	/* Copy across the sign bit.  a can be negative via the BN_mod_inverse() 
	   used in Montgomery ops */
	BN_set_negative( r, BN_is_negative( a ) );

	/* If we're shifting a word at a time then it's just a straight copy 
	   operation */
	if( bitShiftAmount == 0 )
		{
		/* Move the words up, starting from the MSB and moving down to the 
		   LSB */
		LOOP_EXT( i = a->top - 1, i >= 0, i--, iterationBound )
			{
			rData[ wordShiftAmount + i ] = aData[ i ];
			}
		ENSURES_B( LOOP_BOUND_OK );

		/* Set the new top based on what we've shifted up */
		r->top = a->top + wordShiftAmount;
		}
	else
		{
		BN_ULONG left, right = 0;

		/* Shift everything up by taking two words and extracting out the 
		   single word of shifted data that we need */
		LOOP_EXT( i = a->top - 1, i >= 0, i--, iterationBound ) 
			{
			left = aData[ i ];
			rData[ wordShiftAmount + i + 1 ] = \
				( right << bitShiftAmount ) | ( left >> bitShiftRemainder );
			right = left;
			}
		ENSURES_B( LOOP_BOUND_OK );
		rData[ wordShiftAmount ] = right << bitShiftAmount;

		/* Set the new top based on what we've shifted up */
		r->top = a->top + wordShiftAmount;
		if( rData[ r->top ] != 0 )
			{
			/* We shifted bits off the end of the last word, extend the 
			   length by one word */
			r->top++;
			ENSURES( r->top <= getBNMaxSize( r ) );
			}
		}
	BN_clear_top( r, oldTop );

	/* Clear the space that we've shifted up from.  This can become an issue 
	   when r == a */
	LOOP_EXT( i = 0, i < wordShiftAmount, i++, iterationBound )
		{ 
		rData[ i ] = 0;
		}
	ENSURES_B( LOOP_BOUND_OK );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN BN_rshift( INOUT BIGNUM *r, const BIGNUM *a, 
				   IN_RANGE( 0, bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
						const int shiftAmount )
	{
	const int wordShiftAmount = shiftAmount / BN_BITS2;
	const int bitShiftAmount = shiftAmount % BN_BITS2;
	const int bitShiftRemainder = BN_BITS2 - bitShiftAmount;
	const int wordsToShift = a->top - wordShiftAmount;
	const BN_ULONG *aData = a->d;
	const int oldTop = r->top;
	const int iterationBound = getBNMaxSize( a );
	BN_ULONG *rData = r->d;
	int i, LOOP_ITERATOR;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_negative( a ) );
	REQUIRES_B( shiftAmount > 0 && \
				shiftAmount < bytesToBits( CRYPT_MAX_PKCSIZE ) );
	REQUIRES_B( wordShiftAmount < a->top || BN_is_zero( a ) );
	REQUIRES_B( wordShiftAmount + wordsToShift < getBNMaxSize( r ) );

	/* a can be zero when we're being called from routines like 
	   BN_mod_inverse() that iterate until a certain value is reached */
	if( BN_is_zero( a ) )
		{
		int bnStatus = BN_STATUS;

		CK( BN_zero( r ) );
		ENSURES_B( bnStatusOK( bnStatus ) );

		return( TRUE );
		}

	/* Clear the sign bit.  We know that it's not set since a can't be 
	   negative, so we just clear it unconditionally */ 
	BN_set_negative( r, FALSE );

	/* If we're shifting a word at a time then it's just a straight copy 
	   operation */
	if( bitShiftAmount == 0 ) 
		{
		/* Move the words down, starting from the LSB and moving down to the 
		   MSB */
		LOOP_EXT( i = 0, i < wordsToShift, i++, iterationBound )
			{
			rData[ i ] = aData[ wordShiftAmount + i ];
			}
		ENSURES_B( LOOP_BOUND_OK );

		/* Set the new top based on what we've shifted down */
		r->top = wordsToShift;
		} 
	else
		{
		BN_ULONG left, right;

		/* Shift everything down by taking two words and extracting out the 
		   single word of shifted data that we need.  Since we're taking two 
		   words at a time (i.e. reading ahead by one word), we only iterate
		   to wordsToShift - 1 */
		left = aData[ wordShiftAmount ];
		LOOP_EXT( i = 0, i < wordsToShift - 1, i++, iterationBound ) 
			{
			right = aData[ wordShiftAmount + i + 1 ];
			rData[ i ] = ( left >> bitShiftAmount ) | ( right << bitShiftRemainder );
			left = right;
			}
		ENSURES_B( LOOP_BOUND_OK );

		/* Set the new top based on what we've shifted down */
		r->top = wordsToShift - 1;

		/* Add in any remaining bits if required */
		left >>= bitShiftAmount;
		if( left > 0 )
			rData[ r->top++ ] = left;
		}
	BN_clear_top( r, oldTop );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

/****************************************************************************
*																			*
*							Perform Word Ops on Bignums						*
*																			*
****************************************************************************/

/* Add and subtract words to/from a bignum */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_add_word( INOUT BIGNUM *a, const BN_ULONG w )
	{
	BN_ULONG *aData = a->d, word = w;
	const int iterationBound = getBNMaxSize( a );
	int i, LOOP_ITERATOR;

	assert( isWritePtr( a, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( word > 0 );

	/* Do an add with carry.  We can't use the bn_xxx_words() form because
	   it adds the words of two bignums, not a word to a bignum */
	LOOP_EXT( i = 0, i < a->top, i++, iterationBound )
		{
		aData[ i ] += word;

		/* If there wasn't an overflow then we're done, otherwise continue 
		   with carry */
		if( word <= aData[ i ] )
			break;
		word = 1;
		}
	ENSURES_B( LOOP_BOUND_OK );

	/* If we've overflowed onto a new word, increase the length of the 
	   bignum.  The logic here is as follows, if we got as far as a->top
	   without exiting the loop (so i >= a->top) then we're propagating a
	   carry (a->top is at least 1 since a is non-zero so we've passed at 
	   least once through the loop, setting word = 1), so we know that the 
	   top word has the value 1 */
	if( i >= a->top )
		aData[ a->top++ ] = 1;

	ENSURES_B( sanityCheckBignum( a ) );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_sub_word( INOUT BIGNUM *a, const BN_ULONG w )
	{
	BN_ULONG *aData = a->d, word = w;
	const int iterationBound = getBNMaxSize( a );
	int i, LOOP_ITERATOR;

	assert( isWritePtr( a, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( word > 0 );
	REQUIRES_B( a->top > 1 || aData[ 0 ] >= w );
				/* Result shouldn't go negative */

	/* The bignum is larger than the value that we're subtracting, do a 
	   simple subtract with carry.  At this point we again run into the
	   bizarro non-orthogonality of the bn_xxx_words() routines where they
	   sometimes act on { bignum, word }, sometimes on { bignum, bignum },
	   and sometimes on { hiWord, loWord, word }.  In this case it's
	   { bignum, bignum } so we have to synthesise the { bignum, word }
	   form outselves */
	LOOP_EXT( i = 0, i < a->top, i++, iterationBound )
		{
		/* If we can satisfy the subtract from the current bignum word then
		   we're done */
		if( aData[ i ] >= word )
			{
			aData[ i ] -= word;
			break;
			}

		/* Subtract the word with carry */
		aData[ i ] -= word;
		word = 1;
		}
	ENSURES_B( LOOP_BOUND_OK );

	/* If we've cleared the top word, decrease the overall bignum length */
	if( aData[ a->top - 1 ] == 0 )
		a->top--;

	ENSURES_B( sanityCheckBignum( a ) );

	return( TRUE );
	}

/* Multiply and divide (more accurately, perform a modulo operation) words 
   to/from a bignum */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_mul_word( INOUT BIGNUM *a, const BN_ULONG w )
	{
	BN_ULONG *aData = a->d, word;

	assert( isWritePtr( a, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( w > 0 );

	/* For once we've got a bn_xxx_words() form that's actually useful, so 
	   we just call down to that */
	word = bn_mul_words( aData, aData, a->top, w );
	if( word > 0 )
		aData[ a->top++ ] = word;

	ENSURES_B( sanityCheckBignum( a ) );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN BN_mod_word( OUT BN_ULONG *r, const BIGNUM *a, const BN_ULONG w )
	{
	const BN_ULONG *aData = a->d;
	BN_ULONG value = 0;
	const int iterationBound = getBNMaxSize( a );
	int i, LOOP_ITERATOR;

	assert( isWritePtr( r, sizeof( BN_ULONG ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( w > 0 );

	/* Clear return value */
	*r = 0;

	/* Now we run into yet another member of the non-orthogonal OpenSSL zoo,
	   this time a function that divides hi:lo by a word, so
	   result = bn_div_words( hi, lo, word ).  To work with this, we walk
	   down the bignum a double-word at a time, propagating the result as
	   we go */
	LOOP_EXT( i = a->top - 1, i >= 0, i--, iterationBound )
		{
		BN_ULONG tmp;

		tmp = bn_div_words( value, aData[ i ], w );
		value = aData[ i ] - ( tmp * w );
		}
	ENSURES_B( LOOP_BOUND_OK );

	*r = value;

	return( TRUE );
	}

/****************************************************************************
*																			*
*									Square Bignums							*
*																			*
****************************************************************************/

/* Square an array of BN_ULONGs */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static BOOLEAN bn_square( INOUT BN_ULONG *r, const BN_ULONG *a, 
						  IN_RANGE( 0, BIGNUM_ALLOC_WORDS ) const int length, 
						  INOUT BN_ULONG *tmp )
	{
	BN_ULONG carry;
	const int max = length * 2;
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( r, sizeof( BN_ULONG ) * length ) );
	assert( isReadPtrDynamic( a, sizeof( BN_ULONG ) * length ) );
	assert( isReadPtrDynamic( tmp, sizeof( BN_ULONG ) * ( length * 2 ) ) );

	REQUIRES_B( length > 0 && length <= BIGNUM_ALLOC_WORDS );

	/* Walk up the bignum multiplying out the words in it.  For the least-
	   significant word it's a straight multiply, for subsequent words it's
	   a multiply-accumulate.  To see what's going on here it's helpful to 
	   consider each round in turn:

		r[ length ]		=	 mul( r + 1, a + 1, length - 1, a[ 0 ] );
		r[ length + 1 ] = mulacc( r + 3, a + 2, length - 2, a[ 1 ] );
		r[ length + 2 ] = mulacc( r + 5, a + 3, length - 3, a[ 2 ] );
		r[ length + 3 ] = mulacc( r + 7, a + 4, length - 4, a[ 3 ] );

	   Since we're doubling the value, the low word becomes zero, and we also
	   set the high word to zero to handle possible carries in the 
	   bn_add_words() that follows */
	r[ 0 ] = r[ max - 1 ] = 0;
	if( length > 1 )
		{
		r[ length ] = bn_mul_words( &r[ 1 ], &a[ 1 ], length - 1, a[ 0 ] );
		LOOP_EXT( i = 1, i < length - 1, i++, BIGNUM_ALLOC_WORDS )
			{
			r[ length + i ] = bn_mul_add_words( &r[ ( i * 2 ) + 1 ], 
												&a[ i + 1 ], length - ( i + 1 ), 
												a[ i ] );
			}
		ENSURES_B( LOOP_BOUND_OK );
		}
	carry = bn_add_words( r, r, r, max );
	ENSURES_B( carry == 0 );
	bn_sqr_words( tmp, a, length );
	carry = bn_add_words( r, r, tmp, max );
	ENSURES_B( carry == 0 );

	return( TRUE );
	}

/* Square a bignum: r = a * a */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_sqr( INOUT BIGNUM *r, const BIGNUM *a, INOUT BN_CTX *bnCTX )
	{
	BIGNUM *rTmp = r, *tmp;
	const int length = a->top;
	int oldTop, bnStatus = BN_STATUS;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( sanityCheckBNCTX( bnCTX ) );
	REQUIRES_B( length > 0 && length < BIGNUM_ALLOC_WORDS );
	REQUIRES_B( 2 * a->top <= getBNMaxSize( r ) );

	BN_CTX_start( bnCTX );
	if( a == r )
		{
		/* If the input is the same as the output, we need to use
		   a temporary value to compute the result into */
		rTmp = BN_CTX_get( bnCTX );
		if( rTmp == NULL )
			{
			BN_CTX_end( bnCTX );
			return( FALSE );
			}
		}
	oldTop = rTmp->top;

	/* Call the internal bn_square() function with a temporary as scratch 
	   space */
	tmp = BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MUL1 );
	ENSURES_B( tmp != NULL );
	BN_set_flags( tmp, BN_FLG_SCRATCH );
	CK( bn_square( rTmp->d, a->d, length, tmp->d ) );
	if( bnStatusError( bnStatus ) )
		{
		BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MUL1 );
		return( bnStatus );
		}

	/* Squaring a value typically doubles its size, however if the top word
	   of the original value has the high half clear then the result is one 
	   word shorter */
	rTmp->top = 2 * length;
	if( ( a->d[ length - 1 ] & BN_MASK2h ) == 0 )
		rTmp->top--;
	BN_clear_top( rTmp, oldTop );
	if( rTmp != r )
		{
		/* Since the input was the same as the output we need to copy the 
		   temporary back to the output */
		CKPTR( BN_copy( r, rTmp ) );
		if( bnStatusError( bnStatus ) )
			{
			BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MUL1 );
			return( bnStatus );
			}
		}

	/* Clean up */
	BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MUL1 );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Perform Division Ops on Bignums						*
*																			*
****************************************************************************/

/* Use the top two words of the numerator and divisor to calculate a 
   multiplier s.t. | numerator - divisor * multiplier | < divisor.  This 
   performs a variety of system-specific operations chosen based on what the 
   hardware is capable of and what's most efficient, to make things easier 
   we borrow some of the core macros from OpenSSL, since they've already 
   sorted out which option needs to be used where.
   
   numeratorData points at the MSW of the numerator, since the bignum data 
   is in little-endian format subsequent words are at negative offsets to
   this.  We know the there are always at least 3 words present due to the
   normalisation and padding process in BN_div() */

#ifdef BN_LLONG						/* BN_ULONG 32-bit, BN_ULLONG 64-bit */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN calculateMultiplier( OUT BN_ULONG *multiplier, 
									const BN_ULONG *numeratorData,
									const BN_ULONG divisorMSW, 
									const BN_ULONG divisorMSWnext ) 
	{
	const BN_ULONG numeratorMSW = numeratorData[ 0 ];
	const BN_ULONG numeratorMSWnext = numeratorData[ -1 ];
	const BN_ULONG numeratorMSWnext2 = numeratorData[ -2 ];
	BN_ULLONG tempWord;
	BN_ULONG mult, multRemainder;

	assert( isWritePtr( multiplier, sizeof( BN_ULONG ) ) );
	assert( isReadPtr( numeratorData, sizeof( BN_ULONG ) * 3 ) );

	/* Clear return value */
	*multiplier = 0;

	/* If the numerator is larger than the divisor then the multiplier is 
	   all one bits */
	if( numeratorMSW >= divisorMSW )
		{
		*multiplier = BN_MASK2;

		return( TRUE );
		}

	/* Calculate the initial estimate for the multiplier */
  #ifdef BN_DIV2W
	/* Double-word divide available (most modern compilers), use this if 
	   possible */
	mult = ( BN_ULONG ) \
		( ( ( ( ( BN_ULLONG ) numeratorMSW ) << BN_BITS2 ) | numeratorMSWnext ) / divisorMSW );
  #else
	mult = bn_div_words( numeratorMSW, numeratorMSWnext, divisorMSW );
  #endif /* BN_DIV2W */
	multRemainder = ( numeratorMSWnext - divisorMSW * mult ) & BN_MASK2;
	tempWord = ( BN_ULLONG ) divisorMSWnext * mult;

	/* Refine the estimate.  This operation isn't branch-free, but since 
	   it's just a few low-complexity instructions it shouldn't be subject 
	   to any timing issues, particularly compared to the variable-time
	   divide that we've just performed.

	   For the remainder add-overflow-check, we could use 
	   __builtin_uaddl_overflow() (gcc/clang) or ULongAdd() (Windows), 
	   however clang generates the same code regardless of whether the 
	   intrinsic or the portable overflow-check is used and gcc actually 
	   generates *worse* code for the intrinsic (as well as worse code than 
	   clang in both cases), so we just stick with the portable version */
	if( ( ( ( ( BN_ULLONG ) multRemainder ) << BN_BITS2 ) | numeratorMSWnext2 ) < tempWord )
		{
		mult--;
		multRemainder += divisorMSW;
		if( multRemainder >= divisorMSW )	/* No overflow yet */
			{
			tempWord -= divisorMSWnext;
			if( ( ( ( ( BN_ULLONG ) multRemainder ) << BN_BITS2 ) | numeratorMSWnext2 ) < tempWord )
				{
				mult--;

				ENSURES( multRemainder + divisorMSW < divisorMSW );
				}
			}
		}

	*multiplier = mult;

	return( TRUE );
	}
#else								/* BN_ULONG 64-bit */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN calculateMultiplier( OUT BN_ULONG *multiplier, 
									const BN_ULONG *numeratorData,
									const BN_ULONG divisorMSW, 
									const BN_ULONG divisorMSWnext ) 
	{
	const BN_ULONG numeratorMSW = numeratorData[ 0 ];
	const BN_ULONG numeratorMSWnext = numeratorData[ -1 ];
	const BN_ULONG numeratorMSWnext2 = numeratorData[ -2 ];
	BN_ULONG mult, multRemainder, tempWordLow, tempWordHigh;

	assert( isWritePtr( multiplier, sizeof( BN_ULONG ) ) );
	assert( isReadPtr( numeratorData, sizeof( BN_ULONG ) * 2 ) );

	/* Clear return value */
	*multiplier = 0;

	/* If the numerator is larger than the divisor then the multiplier is 
	   all one bits */
	if( numeratorMSW >= divisorMSW )
		{
		*multiplier = BN_MASK2;

		return( TRUE );
		}

	/* Calculate the initial estimate for the multiplier */
	mult = bn_div_words( numeratorMSW, numeratorMSWnext, divisorMSW );
	multRemainder = ( numeratorMSWnext - divisorMSW * mult ) & BN_MASK2;
  #if defined( BN_UMULT_LOHI )
	/* Compiler-based 128-bit multiply available */
	BN_UMULT_LOHI( tempWordLow, tempWordHigh, divisorMSWnext, mult );
  #elif defined( BN_UMULT_HIGH )
	/* Inline asm 64 x 64 -> 64:64 multiply available */
	tempWordLow = divisorMSWnext * mult;
	tempWordHigh = BN_UMULT_HIGH( divisorMSWnext, mult );
  #else
	{
	BN_ULONG multiplierLow, multiplierHigh;

	/* The method of last resort when no double-word multiply is 
	   available, break the values up into words and multiply and 
	   add everything in pieces */
	tempWordLow = LBITS( divisorMSWnext );
	tempWordHigh = HBITS( divisorMSWnext );
	multiplierLow = LBITS( mult );
	multiplierHigh = HBITS( mult );
	mul64( tempWordLow, tempWordHigh, multiplierLow, multiplierHigh );
	}
  #endif /* BN_UMULT_xxx options */

	/* Perform the same fixup as in the BN_LLONG case, but expressed in 
	   terms of high:low pairs since we can't store the value in a single 
	   word */
	if( ( multRemainder < tempWordHigh ) || \
		( multRemainder == tempWordHigh && numeratorMSWnext2 < tempWordLow ) )
		{
		mult--;
		multRemainder += divisorMSW;
		if( multRemainder >= divisorMSW )	/* No overflow yet */
			{
			if( tempWordLow < divisorMSWnext )
				tempWordHigh--;
			tempWordLow -= divisorMSWnext;
			if( ( multRemainder < tempWordHigh ) || \
				( multRemainder == tempWordHigh && numeratorMSWnext2 < tempWordLow ) )
				{
				mult--;

				ENSURES( multRemainder + divisorMSW < divisorMSW );
				}
			}
		}

	*multiplier = mult;

	return( TRUE );
	}
#endif /* !BN_LLONG */

/* Calculate quotient = numerator / divisor, remainder = numerator % divisor.  
   Either the quotient or the remainder may be NULL, so that the function can 
   be used to calculate both BN_div() and BN_mod().

   There's no explicitly constant-time form of this function since it's not 
   clear whether this is either an issue or actually feasible.  The reason 
   why it's probably not an issue is that it's only used (in the BN_mod() 
   form) in three places where online queries can be made via secure 
   sessions, in the RSA code which is blinded so it doesn't matter, in the 
   (EC)DSA code to reduce k where it doesn't matter, and in the (EC)DSA code 
   to create the signature, where it's working with the output created from 
   the random k and again doesn't really matter (in addition there's the 
   dithering timing-protection on private-key ops in the secure session code).
   
   The reason why it's not clear whether it's actually feasible is that the 
   division instruction can vary in time by dozens of cycles, so even a 
   single divide can have more timing-related difference than any extra 
   branches and similar code differences.  Even if we took a measure like
   zero-padding values to make them a constant length, the fact that we're
   operating on long strings of zeroes will still produce significant, and
   measureable, timing differences */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 3, 4, 5 ) ) \
BOOLEAN BN_div( INOUT_OPT BIGNUM *quotient, INOUT_OPT BIGNUM *remainder, 
				const BIGNUM *numerator, const BIGNUM *divisor,
				INOUT BN_CTX *bnCTX )
	{
	BIGNUM *normalisedNumerator, *normalisedDivisor, *result, *temp;
	BN_ULONG *numeratorData, *numeratorDataCurrent, *resultData;
	BN_ULONG divisorMSW, divisorMSWnext;
	const BOOLEAN isNegative = BN_is_negative( numerator );
	const int iterationBound = getBNMaxSize( numerator );
	const int oldQuotientSize = ( quotient != NULL ) ? quotient->top : 0;
	int numeratorWordCount, divisorWordCount, wordsToProcess;
	int normalisationShiftAmt, i, bnStatus = BN_STATUS, LOOP_ITERATOR;

	assert( quotient == NULL || isWritePtr( quotient, sizeof( BIGNUM ) ) );
	assert( remainder == NULL || isWritePtr( remainder, sizeof( BIGNUM ) ) );
	assert( isReadPtr( numerator, sizeof( BIGNUM ) ) );
	assert( isReadPtr( divisor, sizeof( BIGNUM ) ) );
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	/* Numerator can be negative via the BN_mod_inverse() used in Montgomery 
	   ops */
	REQUIRES_B( quotient == NULL || sanityCheckBignum( quotient ) );
	REQUIRES_B( remainder == NULL || sanityCheckBignum( remainder ) );
	REQUIRES_B( quotient != NULL || remainder != NULL );
	REQUIRES_B( sanityCheckBignum( numerator ) );
	REQUIRES_B( sanityCheckBignum( divisor ) && !BN_is_zero( divisor ) && \
				!BN_is_negative( divisor ) );
	REQUIRES_B( sanityCheckBNCTX( bnCTX ) );

	/* Check for the simplest case of divisor > numerator, resulting in 
	   either quotient = zero, remainder = numerator, or quotient = one,
	   remainder = zero */
	if( BN_ucmp( divisor, numerator ) >= 0 ) 
		{
		if( BN_ucmp( divisor, numerator ) == 0 )
			{
			if( quotient != NULL )
				CK( BN_one( quotient ) );
			if( remainder != NULL )
				CK( BN_zero( remainder ) );
			}
		else
			{
			if( quotient != NULL )
				CK( BN_zero( quotient ) );
			if( remainder != NULL && remainder != numerator )
				CKPTR( BN_copy( remainder, numerator ) );
			}
		if( bnStatusError( bnStatus ) )
			return( FALSE );

		return( TRUE );
		}

	BN_CTX_start( bnCTX );
	normalisedNumerator = BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MUL1 );
	normalisedDivisor = BN_CTX_get( bnCTX );
	if( quotient != NULL )
		result = quotient;
	else
		{
		/* The caller doesn't want the quotient returned (in other words 
		   we're being called as BN_mod()), compute it to a temporary 
		   value */
		result = BN_CTX_get( bnCTX );
		}
	temp = BN_CTX_get( bnCTX );
	if( normalisedNumerator == NULL || normalisedDivisor == NULL || \
		result == NULL || temp == NULL )
		{
		BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MUL1 );
		return( FALSE );
		}
	BN_set_flags( temp, BN_FLG_SCRATCH );

	/* Normalise the numerator and divisor */
	normalisationShiftAmt = BN_BITS2 - ( BN_num_bits( divisor ) % BN_BITS2 );
	CK( BN_lshift( normalisedDivisor, divisor, normalisationShiftAmt ) );
	normalisationShiftAmt += BN_BITS2;
	CK( BN_lshift( normalisedNumerator, numerator, normalisationShiftAmt ) );
	if( bnStatusError( bnStatus ) )
		{
		BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MUL1 );
		return( FALSE );
		}
	BN_set_negative( normalisedDivisor, FALSE );
	BN_set_negative( normalisedNumerator, FALSE );

	/* calculateMultiplier() requires three words of numerator data, we have
	   at least two present due to the normalisation process above but if we 
	   need a third we pad the numerator data with a leading zero word.  
	   Note that this denormalises the result, so we have to BN_normalise() 
	   the value at the end of this function */
	if( normalisedNumerator->top < 3 ) 
		normalisedNumerator->d[ normalisedNumerator->top++ ] = 0;

	/* Now that we've finished fiddling the numerator and divisor, remember 
	   various values related to them.  We know that numeratorWordCount is 
	   at least 3 (required by calculateMultiplier()) and wordsToProcess is 
	   at least 1 due to the normalisation/padding step above, these are 
	   required in the operations below */
	divisorWordCount = normalisedDivisor->top;
	numeratorWordCount = normalisedNumerator->top;
	wordsToProcess = numeratorWordCount - divisorWordCount;
	numeratorData = &normalisedNumerator->d[ numeratorWordCount - 1 ];
	numeratorDataCurrent = &normalisedNumerator->d[ wordsToProcess ];
	divisorMSW = normalisedDivisor->d[ divisorWordCount - 1 ];
	if( divisorWordCount > 1 )
		divisorMSWnext = normalisedDivisor->d[ divisorWordCount - 2 ];
	else
		divisorMSWnext = 0;
	ENSURES_B( numeratorWordCount >= 3 && \
			   numeratorWordCount <= BIGNUM_ALLOC_WORDS_EXT );
	ENSURES_B( wordsToProcess >= 1 && \
			   wordsToProcess <= BIGNUM_ALLOC_WORDS );

	/* Similarly, remember values relating to the result */
	result->top = wordsToProcess;
	resultData = &result->d[ wordsToProcess - 1 ];

	/* Evaluate the MSW.  Note that what's being modified here is the 
	   local copy of the normalised numerator, not the actual numerator 
	   passed in by the caller */
	if( BN_ucmp_words( numeratorDataCurrent, divisorWordCount, 
					   normalisedDivisor ) >= 0 )
		{	
		bn_sub_words( numeratorDataCurrent, numeratorDataCurrent, 
					  normalisedDivisor->d, divisorWordCount );
		*resultData-- = 1;
		} 
	else
		{
		result->top--;
		resultData--;
		}

	/* Iterate through evey word of numerator/divisor performing a divide 
	   step on each one */
	LOOP_EXT( i = 0, i < wordsToProcess - 1, ( i++, numeratorData-- ), 
			  iterationBound ) 
		{
		BN_ULONG multiplier DUMMY_INIT, word;

		/* Calculate the multiplier (see the comment for 
		   calculateMultiplier()), multiply it by the divisor and fix up the 
		   result.  This is required because we've only used the top two 
		   words of the numerator and divisor when we calculated the 
		   multipler, requiring fixups for edge cases at this point */
		CK( calculateMultiplier( &multiplier, numeratorData, 
								 divisorMSW, divisorMSWnext ) );
		if( bnStatusError( bnStatus ) )
			{
			BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MUL1 );
			return( FALSE );
			}
		word = bn_mul_words( temp->d, normalisedDivisor->d, 
							 divisorWordCount, multiplier );
		temp->d[ divisorWordCount ] = word;
		numeratorDataCurrent--;
		if( bn_sub_words( numeratorDataCurrent, numeratorDataCurrent, 
						  temp->d, divisorWordCount + 1 ) ) 
			{
			multiplier--;
			if( bn_add_words( numeratorDataCurrent, numeratorDataCurrent, 
							  normalisedDivisor->d, divisorWordCount ) )
				{
				( *numeratorData )++;
				}
			}

		/* Store the current result and move on to the next word */
		*resultData-- = multiplier;
		}
	ENSURES_B( LOOP_BOUND_OK );

	/* Clean up the quotient and remainder, which includes restoring the 
	   remainder from its earlier normalised form */
	if( quotient != NULL )
		{
		CK( BN_clear_top( quotient, oldQuotientSize ) );
		if( bnStatusError( bnStatus ) )
			{
			BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MUL1 );
			return( FALSE );
			}
		BN_set_negative( quotient, isNegative );
		}
	if( remainder != NULL ) 
		{
		/* Re-normalise the numerator because of the possible padding added
		   earlier, then restore it from its shifted (normalised) form */
		CK( BN_normalise( normalisedNumerator ) );
		CK( BN_rshift( remainder, normalisedNumerator, 
					   normalisationShiftAmt ) );
		if( bnStatusError( bnStatus ) )
			{
			BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MUL1 );
			return( FALSE );
			}
		BN_set_negative( remainder, isNegative );
		}
	BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MUL1 );

	ENSURES_B( quotient == NULL || sanityCheckBignum( quotient ) );
	ENSURES_B( remainder == NULL || sanityCheckBignum( remainder ) );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Perform Modulus Ops on Bignums						*
*																			*
****************************************************************************/

/* BN_mod() variant that always returns a positive result (nn = non-
   negative) in the range { 0...abs( d ) - 1 }.  m can be negative via the 
   BN_mod_inverse() used in Montgomery ops */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_nnmod( INOUT BIGNUM *r, const BIGNUM *m, const BIGNUM *d, 
				  INOUT BN_CTX *bnCTX )
	{
	int bnStatus = BN_STATUS;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( m, sizeof( BIGNUM ) ) );
	assert( isReadPtr( d, sizeof( BIGNUM ) ) );
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_B( sanityCheckBignum( m ) && !BN_is_zero( m ) );
	REQUIRES_B( sanityCheckBignum( d ) && !BN_is_zero( d ) && \
				!BN_is_negative( d ) );
	REQUIRES_B( sanityCheckBNCTX( bnCTX ) );

	/* Perform the mod operation and, if the result is negative, add d to 
	   make it positive again */
	CK( BN_mod( r, m, d, bnCTX ) );
	if( bnStatusOK( bnStatus) && BN_is_negative( r ) )
		CK( BN_add( r, r, d ) );
	if( bnStatusError( bnStatus ) )
		return( bnStatus );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

/* BN_mod_add/sub that assume that a and b are positive and less than m, 
   which avoids the need for an expensive mod operation */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_mod_add_quick( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
						  const BIGNUM *m )
	{
	int bnStatus = BN_STATUS, LOOP_ITERATOR;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( b, sizeof( BIGNUM ) ) );
	assert( isReadPtr( m, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( sanityCheckBignum( b ) && !BN_is_zero( b ) && \
				!BN_is_negative( b ) );
	REQUIRES_B( sanityCheckBignum( m ) && !BN_is_zero( m ) && \
				!BN_is_negative( m ) );
	REQUIRES_B( BN_ucmp( a, m ) < 0 &&  BN_ucmp( b, m ) < 0 );

	/* Quick form of mod that subtracts m if the value ends up greater 
	   than m.  In some extremely rare cases (r large, m small) we have to 
	   subtract m twice to get r within range */
	CK( BN_uadd( r, a, b ) );
	LOOP_SMALL_CHECK( bnStatusOK( bnStatus ) && BN_ucmp( r, m ) >= 0 )
		{
		/* r is bigger than m, get it back within range */
		CK( BN_usub( r, r, m ) );
		}
	ENSURES( LOOP_BOUND_OK );
	if( bnStatusError( bnStatus ) )
		return( bnStatus );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_mod_sub_quick( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
						  const BIGNUM *m )
	{
	int bnStatus = BN_STATUS, LOOP_ITERATOR;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( b, sizeof( BIGNUM ) ) );
	assert( isReadPtr( m, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( sanityCheckBignum( b ) && !BN_is_zero( b ) && \
				!BN_is_negative( b ) );
	REQUIRES_B( sanityCheckBignum( m ) && !BN_is_zero( m ) && \
				!BN_is_negative( m ) );
	REQUIRES_B( BN_ucmp( a, m ) < 0 &&  BN_ucmp( b, m ) < 0 );

	/* Quick form of mod that adds m if the value ends up negative.  We 
	   have to use BN_sub() rather than BN_usub() since b can be larger
	   than a so that r goes negative */
	CK( BN_sub( r, a, b ) );
	LOOP_SMALL_CHECK( bnStatusOK( bnStatus ) && BN_is_negative( r ) )
		{
		/* r is negative, get it back within range.  Note that we have to use 
		   BN_add() rather than BN_uadd() since r is negative */
		CK( BN_add( r, r, m ) );
		}
	ENSURES( LOOP_BOUND_OK );
	if( bnStatusError( bnStatus ) )
		return( bnStatus );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

/* BN_mod_lshift() that assumes that a is positive and less than m, avoiding
   an expensive modulus operation */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
BOOLEAN BN_mod_lshift_quick( BIGNUM *r, const BIGNUM *a,
							 IN_RANGE( 0, bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
								const int shiftAmount,
							 const BIGNUM *m )
	{
	int shiftCount, bnStatus = BN_STATUS, LOOP_ITERATOR;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( m, sizeof( BIGNUM ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( shiftAmount > 0 && \
				shiftAmount < bytesToBits( CRYPT_MAX_PKCSIZE ) );
	REQUIRES_B( sanityCheckBignum( m ) && !BN_is_zero( m ) && \
				!BN_is_negative( m ) );
	REQUIRES_B( BN_cmp( a, m ) < 0 );

	/* Initialise the output value if it's not the same as the input */
	if( r != a )
		{
		CKPTR( BN_copy( r, a ) );
		if( bnStatusError( bnStatus ) )
			return( FALSE );
		}

	/* Convert a shift-and-mod into a series of far less expensive shift-and-
	   subtracts.  We do this by ensuring that the shifts are capped so that
	   r can only grow to the same size as m, in which case the reduction 
	   step is just a subtract */
	LOOP_EXT_INITCHECK( shiftCount = shiftAmount, shiftCount > 0, 
						bytesToBits( CRYPT_MAX_PKCSIZE ) )
		{
		int shift;

		/* Set a shift amount that ensures that we can still do a reduction
		   with a simple subtract.  The first time through this shifts
		   sufficient bits that r grows to the same size as m, after which
		   it typically shifts one bit at a time, so the total number of
		   iterations is 1 + ( sizeof_bits( m ) - sizeof_bits( a ) ) */
		shift = BN_num_bits( m ) - BN_num_bits( r );
		ENSURES_B( shift >= 0 && shift < bytesToBits( CRYPT_MAX_PKCSIZE ) );
		if( shift > shiftCount )
			shift = shiftCount;
		else
			{
			/* If the bignums are the same size, make sure that we shift by
			   at least one bit */
			if( shift == 0 )
				shift = 1;
			}

		/* Perform the shift and reduction */
		CK( BN_lshift( r, r, shift ) );
		if( bnStatusOK( bnStatus) && BN_cmp( r, m ) >= 0 )
			CK( BN_sub( r, r, m ) );
		if( bnStatusError( bnStatus ) )
			return( bnStatus );
		shiftCount -= shift;
		}
	ENSURES_B( LOOP_BOUND_OK );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}
#endif /* USE_ECDH || USE_ECDSA */

/* Generic modmult without special tricks like Montgomery maths */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5 ) ) \
BOOLEAN BN_mod_mul( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
					const BIGNUM *m, INOUT BN_CTX *ctx )
	{
	BIGNUM *tmp;
	int bnStatus = BN_STATUS;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( b, sizeof( BIGNUM ) ) );
	assert( isReadPtr( m, sizeof( BIGNUM ) ) );
	assert( isWritePtr( ctx, sizeof( BN_CTX ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( sanityCheckBignum( b ) && !BN_is_zero( b ) && \
				!BN_is_negative( b ) );
	REQUIRES_B( sanityCheckBignum( m ) && !BN_is_zero( m ) && \
				!BN_is_negative( m ) );
	REQUIRES_B( sanityCheckBNCTX( ctx ) );

	/* If a and b are the same then we can use a more efficient squaring op 
	   rather than a multiply */
	if( !BN_cmp( a, b ) )
		return( BN_mod_sqr( r, a, m, ctx ) );

    BN_CTX_start( ctx );
    tmp = BN_CTX_get( ctx );
	if( tmp == NULL )
		{
		BN_CTX_end( ctx );
		return( FALSE );
		}
	CK( BN_mul( tmp, a, b, ctx ) );
	CK( BN_mod( r, tmp, m, ctx ) );
	BN_CTX_end( ctx );
	if( bnStatusError( bnStatus ) )
		return( bnStatus );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

/* Mod square */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_mod_sqr( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *m, 
					INOUT BN_CTX *ctx )
	{
	int bnStatus = BN_STATUS;

	assert( isWritePtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( m, sizeof( BIGNUM ) ) );
	assert( isWritePtr( ctx, sizeof( BN_CTX ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( sanityCheckBignum( m ) && !BN_is_zero( m ) && \
				!BN_is_negative( m ) );
	REQUIRES_B( sanityCheckBNCTX( ctx ) );

	/* Since we know that r can't be negative after the squaring (which it 
	   couldn't be in any case since a is positive), we can just call 
	   BN_mod() directly */
    CK( BN_sqr( r, a, ctx ) );
	CK( BN_mod( r, r, m, ctx ) );
	if( bnStatusError( bnStatus ) )
		return( bnStatus );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Perform Montgomery Ops on Bignums					*
*																			*
****************************************************************************/

/* Montgomery modmult */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5 ) ) \
BOOLEAN BN_mod_mul_montgomery( INOUT BIGNUM *r, const BIGNUM *a, 
							   const BIGNUM *b, const BN_MONT_CTX *bnMontCTX, 
							   INOUT BN_CTX *bnCTX )
	{
	BIGNUM *tmp;
	int bnStatus = BN_STATUS;

	assert( isReadPtr( r, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( b, sizeof( BIGNUM ) ) );
	assert( isReadPtr( bnMontCTX, sizeof( BN_MONT_CTX ) ) );
	assert( isWritePtr( bnCTX, sizeof( BN_CTX ) ) );

	REQUIRES_B( sanityCheckBignum( a ) && !BN_is_zero( a ) && \
				!BN_is_negative( a ) );
	REQUIRES_B( sanityCheckBignum( b ) && !BN_is_zero( b ) && \
				!BN_is_negative( b ) );
	REQUIRES_B( sanityCheckBNMontCTX( bnMontCTX ) );
	REQUIRES_B( sanityCheckBNCTX( bnCTX ) );
	REQUIRES_B( BN_cmp( a, &bnMontCTX->N ) <= 0 );
	REQUIRES_B( BN_cmp( b, &bnMontCTX->N ) <= 0 );

	BN_CTX_start( bnCTX );

	/* Since we're dealing with oversize values that temporarily get very 
	   large, we have to use an extended bignum */
	tmp = BN_CTX_get_ext( bnCTX, BIGNUM_EXT_MONT );
	if( tmp == NULL )
		{
		BN_CTX_end( bnCTX );
		return( FALSE );
		}
	BN_set_flags( tmp, BN_FLG_SCRATCH );

	/* Perform the multiply and convert the result back from the Montgomery 
	   form */
	CK( BN_mul( tmp, a, b, bnCTX ) );
	CK( BN_from_montgomery( r, tmp, bnMontCTX, bnCTX ) )

	BN_CTX_end_ext( bnCTX, BIGNUM_EXT_MONT );
	if( bnStatusError( bnStatus ) )
		return( FALSE );

	ENSURES_B( sanityCheckBignum( r ) );

	return( TRUE );
	}

/****************************************************************************
*																			*
*								Compare Bignums								*
*																			*
****************************************************************************/

/* Compare two bignums */

STDC_NONNULL_ARG( ( 1 ) ) \
int BN_cmp_word( const BIGNUM *bignum, const BN_ULONG word )
	{
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	/* If the bignum is negative then the (unsigned) word is always
	   larger.  This can occur via the usual mechanism of the 
	   BN_mod_inverse() used in Montgomery ops */
	if( BN_is_negative( bignum ) )
		return( -1 );

	/* If the bignum is longer than one word then the word being compared is 
	   smaller */
	if( bignum->top > 1 )
		return( 1 );

	/* If the bignum is zero then the word is either equal or larger */
	if( bignum->top <= 0 )
		return( ( word == 0 ) ? 0 : -1 );

	if( bignum->d[ 0 ] != word )
		return( ( bignum->d[ 0 ] > word ) ? 1 : -1 );
	
	/* They're identical */
	return( 0 );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int BN_ucmp( const BIGNUM *bignum1, const BIGNUM *bignum2 )
	{
	const int bignum1top = bignum1->top;

	assert( isReadPtr( bignum1, sizeof( BIGNUM ) ) );
	assert( isReadPtr( bignum2, sizeof( BIGNUM ) ) );

	REQUIRES_B( bignum1top >= 0 && bignum1top < getBNMaxSize( bignum1 ) );
				/* There's not really any error return value that we can use 
				   here, the best that we can do is return zero on error */

	/* If we're comparing a bignum to itself (which can happen in functions
	   that take bignums as parameters in multiple locations) then we don't 
	   need to explicitly compare the contents */
	if( bignum1 == bignum2 )
		return( 0 );

	return( BN_ucmp_words( bignum1->d, bignum1top, bignum2 ) );
	}

STDC_NONNULL_ARG( ( 1, 3 ) ) \
int BN_ucmp_words( const BN_ULONG *bignumData1, 
				   IN_RANGE( 0, BIGNUM_ALLOC_WORDS ) const int bignum1Length, 
				   const BIGNUM *bignum2 )
	{
	assert( bignum1Length == 0 || \
			isReadPtrDynamic( bignumData1, bignum1Length ) );
	assert( isReadPtr( bignum2, sizeof( BIGNUM ) ) );

	REQUIRES_B( bignum1Length >= 0 && bignum1Length <= BIGNUM_ALLOC_WORDS );
				/* There's not really any error return value that we can use 
				   here, the best that we can do is return zero on error */

	/* If the magnitude differs then we don't need to look at the values */
	if( bignum1Length != bignum2->top )
		return( ( bignum1Length > bignum2->top ) ? 1 : -1 );

	return( bn_cmp_words( bignumData1, bignum2->d, bignum1Length ) );
	}

/* Internal function that compares the words of two bignums */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int bn_cmp_words( const BN_ULONG *bignumData1, const BN_ULONG *bignumData2, 
				  IN_RANGE( 0, BIGNUM_ALLOC_WORDS ) const int length )
	{
	int i, LOOP_ITERATOR;

	assert( length == 0 || isReadPtrDynamic( bignumData1, length ) );
	assert( length == 0 || isReadPtrDynamic( bignumData2, length ) );

	REQUIRES_B( length >= 0 && length <= BIGNUM_ALLOC_WORDS );
				/* There's not really any error return value that we can use 
				   here, the best that we can do is return zero on error */

	/* Walk down the bignum until we find a difference */
	LOOP_EXT( i = length - 1, i >= 0, i--, BIGNUM_ALLOC_WORDS )
		{
		if( bignumData1[ i ] != bignumData2[ i ] )
			return( ( bignumData1[ i ] > bignumData2[ i ] ) ? 1 : -1 );
		}
	ENSURES_B( LOOP_BOUND_OK );

	/* They're identical */
	return( 0 );
	}

/* An oddball compare function used in BN_mul() via the bn_mul_recursive() 
   routine.  This compares two lots of bignum data of different lengths.
   Instead of doing the sensible thing and passing in { a, aLen } and
   { b, bLen }, we get passed { a, b, min( aLen, bLen ), aLen - bLen },
   where the last value can be negative if a < b, and have to figure things
   out from there */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int bn_cmp_part_words( const BN_ULONG *a, const BN_ULONG *b, 
					   IN_RANGE( 0, BIGNUM_ALLOC_WORDS_EXT ) const int cl, 
					   IN_RANGE( -BIGNUM_ALLOC_WORDS_EXT, \
								 BIGNUM_ALLOC_WORDS_EXT ) const int dl )
	{
	const BN_ULONG *data = ( dl < 0 ) ? b : a;
	const int max = ( dl < 0 ) ? -dl + cl : dl + cl;
	int i, LOOP_ITERATOR;

	REQUIRES_B( cl >= 0 && cl < BIGNUM_ALLOC_WORDS_EXT );
	REQUIRES_B( dl > -BIGNUM_ALLOC_WORDS_EXT && \
				dl < BIGNUM_ALLOC_WORDS_EXT );
	REQUIRES_B( max >= 0 && max < BIGNUM_ALLOC_WORDS_EXT );
				/* There's not really any error return value that we can use 
				   here, the best that we can do is return zero on error */

	/* Compare the overflow portions of length dl.  If any of the overflow
	   portion is nonzero then a or b is larger, depending on whether dl is
	   positive or negative */
	LOOP_EXT( i = cl, i < max, i++, BIGNUM_ALLOC_WORDS_EXT )
		{
		if( data[ i ] != 0 )
			return( ( dl < 0 ) ? -1 : 1 );
		}
	ENSURES_B( LOOP_BOUND_OK );

	/* Compare the common-length portions */
	return( bn_cmp_words( a, b, cl ) );
	}
#endif /* USE_PKC */
