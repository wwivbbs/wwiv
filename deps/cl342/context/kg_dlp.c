/****************************************************************************
*																			*
*				cryptlib DLP Key Generation/Checking Routines				*
*						Copyright Peter Gutmann 1997-2008					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "keygen.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "context/keygen.h"
#endif /* Compiler-specific includes */

#if defined( USE_DH ) || defined( USE_DSA ) || defined( USE_ELGAMAL )

/****************************************************************************
*																			*
*							Utility Functions								*
*																			*
****************************************************************************/

/* Enable various side-channel protection mechanisms */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int enableSidechannelProtection( INOUT PKC_INFO *pkcInfo,
										IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( isDlpAlgo( cryptAlgo ) );

	/* Use constant-time modexp() to protect the private key from timing 
	   channels */
	BN_set_flags( &pkcInfo->dlpParam_x, BN_FLG_EXP_CONSTTIME );

	/* Checksum the bignums to try and detect fault attacks.  Since we're
	   setting the checksum at this point there's no need to check the 
	   return value */
	( void ) calculateBignumChecksum( pkcInfo, cryptAlgo );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Determine Discrete Log Exponent Bits				*
*																			*
****************************************************************************/

/* The following function (provided by Colin Plumb) is used to calculate the
   appropriate size exponent for a given prime size which is required to
   provide equivalent security from small-exponent attacks.

   This is based on a paper by Michael Wiener on	| The function defined
   the difficulty of the two attacks, which has		| below (not part of the
   the following table:								| original paper)
													| produces the following
	 Table 1: Subgroup Sizes to Match Field Sizes	| results:
													|
	Size of p	Cost of each attack		Size of q	|	Output	Error
	 (bits)		(instructions or		(bits)		|			(+ is safe)
				 modular multiplies)				|
													|
	   512			9 x 10^17			119			|	137		+18
	   768			6 x 10^21			145			|	153		+8
	  1024			7 x 10^24			165			|	169		+4
	  1280			3 x 10^27			183			|	184		+1
	  1536			7 x 10^29			198			|	198		+0
	  1792			9 x 10^31			212			|	212		+0
	  2048			8 x 10^33			225			|	225		+0
	  2304			5 x 10^35			237			|	237		+0
	  2560			3 x 10^37			249			|	249		+0
	  2816			1 x 10^39			259			|	260		+1
	  3072			3 x 10^40			269			|	270		+1
	  3328			8 x 10^41			279			|	280		+1
	  3584			2 x 10^43			288			|	289		+1
	  3840			4 x 10^44			296			|	297		+1
	  4096			7 x 10^45			305			|	305		+0
	  4352			1 x 10^47			313			|	313		+0
	  4608			2 x 10^48			320			|	321		+1
	  4864			2 x 10^49			328			|	329		+1
	  5120			3 x 10^50			335			|	337		+2

   This function fits a curve to this, which overestimates the size of the
   exponent required, but by a very small amount in the important 1000-4000
   bit range.  It is a quadratic curve up to 3840 bits, and a linear curve
   past that.  They are designed to be C(1) (have the same value and the same
   slope) at the point where they meet */

#define AN		1L		/* a = -AN/AD/65536, the quadratic coefficient */
#define AD		3L
#define M		8L		/* Slope = M/256, i.e. 1/32 where linear starts */
#define TX		3840L	/* X value at the slope point, where linear starts */
#define TY		297L	/* Y value at the slope point, where linear starts */

/* For a slope of M at the point (TX,TY) we only have one degree of freedom
   left in a quadratic curve so use the coefficient of x^2, namely a, as 
   that free parameter.

   y = -AN/AD*((x-TX)/256)^2 + M*(x-TX)/256 + TY
	 = -AN*(x-TX)*(x-TX)/AD/256/256 + M*x/256 - M*TX/256 + TY
	 = -AN*x*x/AD/256/256 + 2*AN*x*TX/AD/256/256 - AN*TX*TX/AD/256/256 \
		+ M*x/256 - M*TX/256 + TY
	 = -AN*(x/256)^2/AD + 2*AN*(TX/256)*(x/256)/AD + M*(x/256) \
		- AN*(TX/256)^2/AD - M*(TX/256) + TY
	 = (AN*(2*TX/256 - x/256) + M*AD)*x/256/AD - (AN*(TX/256)/AD + M)*TX/256 \
		+ TY
	 = (AN*(2*TX/256 - x/256) + M*AD)*x/256/AD \
		- (AN*(TX/256) + M*AD)*TX/256/AD + TY
	 =  ((M*AD + AN*(2*TX/256 - x/256))*x - (AN*(TX/256)+M*AD)*TX)/256/AD + TY
	 =  ((M*AD + AN*(2*TX - x)/256)*x - (AN*(TX/256)+M*AD)*TX)/256/AD + TY
	 =  ((M*AD + AN*(2*TX - x)/256)*x - (M*AD + AN*TX/256)*TX)/256/AD + TY
	 =  (((256*M*AD+2*AN*TX-AN*x)/256)*x - (M*AD + AN*TX/256)*TX)/256/AD + TY

   Since this is for the range 0...TX, in order to avoid having any
   intermediate results less than 0, we need one final rearrangement, and a
   compiler can easily take the constant-folding from there...

	 =  TY + (((256*M*AD+2*AN*TX-AN*x)/256)*x - (M*AD + AN*TX/256)*TX)/256/AD
	 =  TY - ((M*AD + AN*TX/256)*TX - ((256*M*AD+2*AN*TX-AN*x)/256)*x)/256/AD */

CHECK_RETVAL \
static int getDLPexpSize( IN_LENGTH_SHORT_MIN( MIN_PKCSIZE * 8 ) \
							const int primeBits )
	{
	long value;	/* Necessary to avoid problems with 16-bit compilers */

	REQUIRES( primeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  primeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* If it's over TX bits, it's linear */
	if( primeBits > TX )
		value = M * primeBits / 256 - M * TX / 256 + TY;
	else
		{
		/* It's quadratic */
		value = TY - ( ( M * AD + AN * TX / 256 ) * TX - \
					   ( ( 256 * M * AD + AN * 2 * TX - AN * primeBits ) / 256 ) * \
					   primeBits ) / ( AD * 256 );
		}
	ENSURES( value >= 160 && value < 1000 );

	/* At this point we run into a problem with SSH, it hardcodes the 
	   exponent size at 160 bits no matter what the prime size is so that
	   there's no increase in security when the key size is increased.  
	   Because this function generates an exponent whose size matches the 
	   security level of the key, it can't be used to generate DSA keys for 
	   use with SSH.  In order to provide at least basic keys usable with
	   SSH and also for backwards compatiblity with older (non-SSH) 
	   implementations that hardcode DSA key parameters at { 1024, 160 } we 
	   always return a fixed exponent size of 160 bits if the key size is 
	   around 1024 bits */
	if( primeBits <= 1028 )
		return( 160 );

	return( ( int ) value );
	}

/****************************************************************************
*																			*
*			  					Generate DL Primes							*
*																			*
*	Original findGeneratorForPQ() and generateDLPPublicValues() are 		*
*	copyright Kevin J. Bluck 1998.  Remainder copyright Peter Gutmann		*
*	1998-2008.																*
*																			*
****************************************************************************/

/* DLP-based PKCs have various requirements for the generated parameters:

	DSA: p, q, and g of preset lengths (currently p isn't fixed at exactly
		n * 64 bits because of the way the Lim-Lee algorithm works, it's
		possible to get this by iterating the multiplication step until the
		result is exactly n * 64 bits but this doesn't seem worth the
		effort), x = 1...q-1.
	PKCS #3 DH: No g (it's fixed at 2) or q.  Keys of this type can be 
		generated if required, but the current code is configured to always 
		generate X9.42 DH keys.
	X9.42 DH: p, q, and g as for DSA but without the 160-bit SHA1-enforced
		upper limit on q in original versions of the DSA specification so 
		that p can go above 1024 bits (newer versions of the spec finally
		allow larger p and q values in conjunction with post-SHA1 hash
		algorithms), x = 2...q-2.
	Elgamal: As X9.42 DH */

/* The maximum number of factors required to generate a prime using the Lim-
   Lee algorithm.  The value 160 is the minimum safe exponent size */

#define MAX_NO_FACTORS	( ( bytesToBits( CRYPT_MAX_PKCSIZE ) / 160 ) + 1 )

/* The maximum number of small primes required to generate a prime using the
   Lim-Lee algorithm.  There's no fixed bound on this value, but in the worst
   case we start with 
   ~ CRYPT_MAX_PKCSIZE_bits / getDLPexpSize( CRYPT_MAX_PKCSIZE_bits ) primes 
   = ~ 13 values, and add one more prime on each retry.  Typically we need 
   10-15 for keys in the most commonly-used range 512-2048 bits.  In order 
   to simplify the handling of values we allow for 128 primes, which has a 
   vanishingly small probability of failing and also provides a safe upper 
   bound for the number of retries (there's something wrong with the 
   algorithm if it requires anywhere near this many retries) */

#define MAX_NO_PRIMES	128

/* Select a generator g for the prime moduli p and q.  g will be chosen to 
   be of prime order q, where q divides (p - 1), i.e. g generates the 
   subgroup of order q in the multiplicative group of GF(p) 
   (traditionally for PKCS #3 DH g is fixed at 2 which is safe even when 
   it's not a primitive root since it still covers half of the space of 
   possible residues, however we always generate a FIPS 186-style g value) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int findGeneratorForPQ( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *p = &pkcInfo->dlpParam_p, *q = &pkcInfo->dlpParam_q;
	BIGNUM *g = &pkcInfo->dlpParam_g;
	BIGNUM *j = &pkcInfo->tmp1, *gCounter = &pkcInfo->tmp2;
	int bnStatus = BN_STATUS, iterationCount;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* j = (p - 1) / q */
	CK( BN_sub_word( p, 1 ) );
	CK( BN_div( j, NULL, p, q, pkcInfo->bnCTX ) );
	CK( BN_add_word( p, 1 ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Starting gCounter at 3, set g = (gCounter ^ j) mod p until g != 1.
	   Although FIPS 196/X9.30/X9.42 merely require that 1 < g < p-1, if we
	   use small integers it makes this operation much faster.  Note that 
	   we can't use a Montgomery modexp at this point since we haven't
	   evaluated the Montgomery form of p yet */
	CK( BN_set_word( gCounter, 2 ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	for( iterationCount = 0; iterationCount < FAILSAFE_ITERATIONS_MED; 
		 iterationCount++ )
		{
		CK( BN_add_word( gCounter, 1 ) );
		CK( BN_mod_exp( g, gCounter, j, p, pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		if( !BN_is_one( g ) )
			break;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( CRYPT_OK );
	}

/* Generate prime numbers for DLP-based PKC's using the Lim-Lee algorithm:

	p = 2 * q * ( prime[1] * ... prime[n] ) + 1 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateDLPPublicValues( INOUT PKC_INFO *pkcInfo, 
									IN_LENGTH_SHORT_MIN( MIN_PKCSIZE * 8 ) \
										const int pBits )
	{
	const int safeExpSizeBits = getDLPexpSize( pBits );
	const int noChecks = getNoPrimeChecks( pBits );
	const int qBits = safeExpSizeBits;
	BIGNUM llPrimes[ MAX_NO_PRIMES + 8 ], llProducts[ MAX_NO_FACTORS + 8 ];
	BIGNUM *p = &pkcInfo->dlpParam_p, *q = &pkcInfo->dlpParam_q;
	BOOLEAN primeFound;
	int indices[ MAX_NO_FACTORS + 8 ];
	int nPrimes, nFactors, factorBits, i, iterationCount;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( bytesToBits( MIN_PKCSIZE ) > 512 || \
			getDLPexpSize( 512 ) == 160 );
	assert( bytesToBits( MIN_PKCSIZE ) > 1024 || \
			getDLPexpSize( 1024 ) == 160 );
			/* 1024-bit keys have special handling for pre-FIPS 186-3
			   compatibility */
	assert( bytesToBits( MIN_PKCSIZE ) > 1030 || \
			getDLPexpSize( 1030 ) == 168 );
	assert( bytesToBits( MIN_PKCSIZE ) > 1536 || \
			getDLPexpSize( 1536 ) == 198 );
	assert( getDLPexpSize( 2048 ) == 225 );
	assert( getDLPexpSize( 3072 ) == 270 );
	assert( getDLPexpSize( 4096 ) == 305 );

	REQUIRES( pBits >= bytesToBits( MIN_PKCSIZE ) && \
			  pBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );
	REQUIRES( qBits >= 160 && qBits <= pBits && \
			  qBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );
	REQUIRES( safeExpSizeBits >= 160 && safeExpSizeBits < 512 );

	/* Determine how many factors we need and the size in bits of the 
	   factors */
	factorBits = ( pBits - qBits ) - 1;
	nFactors = nPrimes = ( factorBits / safeExpSizeBits ) + 1;
	factorBits /= nFactors;
	ENSURES( factorBits > 0 && \
			 factorBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) && \
			 nFactors > 0 && nFactors <= MAX_NO_FACTORS && \
			 nPrimes > 0 && nPrimes <= MAX_NO_PRIMES );

	/* Generate a random prime q and multiply by 2 to form the base for the
	   other factors */
	status = generatePrime( pkcInfo, q, qBits, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( status );
	CK( BN_lshift1( q, q ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Set up the permutation control arrays and generate the first nFactors 
	   factors */
	memset( llProducts, 0, MAX_NO_FACTORS * sizeof( BIGNUM ) );
	for( i = 0; i < MAX_NO_FACTORS; i++ )
		BN_init( &llProducts[ i ] );
	memset( llPrimes, 0, MAX_NO_PRIMES * sizeof( BIGNUM ) );
	for( i = 0; i < MAX_NO_PRIMES; i++ )
		BN_init( &llPrimes[ i ] );
	for( i = 0; i < nFactors; i++ )
		{
		status = generatePrime( pkcInfo, &llPrimes[ i ], factorBits, 
								CRYPT_UNUSED );
		if( cryptStatusError( status ) )
			goto cleanup;
		}

	for( primeFound = FALSE, iterationCount = 0;
		 !primeFound && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		int indexMoved, innerIterationCount;

		/* Initialize the indices for the permutation.  We try the first 
		   nFactors factors first since any new primes will be added at the 
		   end */
		indices[ nFactors - 1 ] = nPrimes - 1;
		for( i = nFactors - 2; i >= 0; i-- )
			indices[ i ] = indices[ i + 1 ] - 1;
		CK( BN_mul( &llProducts[ nFactors - 1 ], q, &llPrimes[ nPrimes - 1 ], 
					pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			{
			status = getBnStatus( bnStatus );
			goto cleanup;
			}
		indexMoved = nFactors - 2;

		/* Test all possible new prime permutations until a prime is found or 
		   we run out of permutations */
		for( innerIterationCount = 0; 
			 indices[ nFactors - 1 ] > 0 && \
			   innerIterationCount < ( FAILSAFE_ITERATIONS_LARGE * 10 );
			 innerIterationCount++ )
			{
			/* Assemble a new candidate prime 2 * q * primes + 1 from the 
			   currently indexed random primes */
			for( i = indexMoved; bnStatusOK( bnStatus ) && i >= 0; i-- )
				{
				CK( BN_mul( &llProducts[ i ], &llProducts[ i + 1 ],
							&llPrimes[ indices[ i ] ], pkcInfo->bnCTX ) );
				}
			CKPTR( BN_copy( p, &llProducts[ 0 ] ) );
			CK( BN_add_word( p, 1 ) );
			if( bnStatusError( bnStatus ) )
				{
				status = getBnStatus( bnStatus );
				goto cleanup;
				}

			/* If the candidate has a good chance of being prime, try a
			   probabilistic test and exit if it succeeds */
			if( primeSieve( p ) )
				{
				status = primeProbable( pkcInfo, p, noChecks );
				if( cryptStatusError( status ) )
					goto cleanup;
				if( status == TRUE )
					{
					primeFound = TRUE;
					break;
					}
				}

			/* Find the lowest index which is not already at the lowest 
			   possible point and move it down one */
			for( i = 0; i < nFactors; i++ )
				{
				if( indices[ i ] > i )
					{
					indices[ i ]--;
					indexMoved = i;
					break;
					}
				}

			/* If we moved down the highest index we've exhausted all of the 
			   permutations so we have to start over with another prime */
			if( ( indexMoved >= nFactors - 1 ) || ( i >= nFactors ) )
				break;

			/* We haven't changed the highest index, take all of the indices 
			   below the one that we moved down and move them up so that 
			   they're packed up as high as they'll go */
			for( i = indexMoved - 1; i >= 0; i-- )
				indices[ i ] = indices[ i + 1 ] - 1;
			} 
		ENSURES( innerIterationCount < ( FAILSAFE_ITERATIONS_LARGE * 10 ) );

		/* If we haven't found a prime yet, add a new prime to the pool and
		   try again */
		if( !primeFound )
			{
			if( nPrimes >= MAX_NO_PRIMES )
				{
				/* We've run through an extraordinary number of primes, 
				   something is wrong */
				DEBUG_DIAG(( "Iterated through %d primes trying to "
							 "generate DLP key", MAX_NO_PRIMES ));
				assert( DEBUG_WARN );
				status = CRYPT_ERROR_FAILED;
				goto cleanup;
				}
			status = generatePrime( pkcInfo, &llPrimes[ nPrimes++ ], factorBits, 
									CRYPT_UNUSED );
			if( cryptStatusError( status ) )
				goto cleanup;
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* Recover the original value of q by dividing by 2 and find a generator 
	   suitable for p and q */
	CK( BN_rshift1( q, q ) );
	if( bnStatusError( bnStatus ) )
		{
		status = getBnStatus( bnStatus );
		goto cleanup;
		}
	status = findGeneratorForPQ( pkcInfo );

cleanup:

	/* Free the local storage */
	for( i = 0; i < nPrimes; i++ )
		BN_clear_free( &llPrimes[ i ] );
	for( i = 0; i < nFactors; i++ )
		BN_clear_free( &llProducts[ i ] );
	zeroise( llPrimes, MAX_NO_PRIMES * sizeof( BIGNUM ) );
	zeroise( llProducts, MAX_NO_FACTORS * sizeof( BIGNUM ) );

	return( status );
	}

/* Generate the DLP private value x and public value y */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateDLPPrivateValue( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *x = &pkcInfo->dlpParam_x, *q = &pkcInfo->dlpParam_q; 
	const int qBits = BN_num_bits( q );
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* If it's a PKCS #3 DH key there won't be a q value present so we have 
	   to estimate the appropriate x size in the same way that we estimated 
	   the q size when we generated the public key components */
	if( BN_is_zero( q ) )
		{
		return( generateBignum( x, 
					getDLPexpSize( BN_num_bits( &pkcInfo->dlpParam_p ) ),
					0xC0, 0 ) );
		}

	/* Generate the DLP private value x s.t. 2 <= x <= q-2 (this is the 
	   lowest common denominator of FIPS 186's 1...q-1 and X9.42's 2...q-2). 
	   Because the mod q-2 is expensive we do a quick check to make sure 
	   that it's really necessary before calling it */
	status = generateBignum( x, qBits, 0xC0, 0 );
	if( cryptStatusError( status ) )
		return( status );
	CK( BN_sub_word( q, 2 ) );
	if( BN_cmp( x, q ) > 0 )
		{
		/* Trim x down to size.  Actually we get the upper bound as q-3, 
		   but over a 160-bit (minimum) number range this doesn't matter */
		CK( BN_mod( x, x, q, pkcInfo->bnCTX ) );

		/* If the value that we ended up with is too small, just generate a 
		   new value one bit shorter, which guarantees that it'll fit the 
		   criteria (the target is a suitably large random value, not the 
		   closest possible fit within the range) */
		if( bnStatusOK( bnStatus ) && BN_num_bits( x ) < qBits - 5 )
			status = generateBignum( x, qBits - 1, 0xC0, 0 );
		}
	CK( BN_add_word( q, 2 ) );

	return( cryptStatusError( status ) ? status : getBnStatus( bnStatus ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateDLPPublicValue( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *p = &pkcInfo->dlpParam_p, *g = &pkcInfo->dlpParam_g;
	BIGNUM *x = &pkcInfo->dlpParam_x, *y = &pkcInfo->dlpParam_y; 
	int bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	CK( BN_mod_exp_mont( y, g, x, p, pkcInfo->bnCTX, 
						 &pkcInfo->dlpParam_mont_p ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Check a DLP Key								*
*																			*
****************************************************************************/

/* Perform validity checks on the public key.  We have to make the PKC_INFO
   data non-const because the bignum code wants to modify some of the values
   as it's working with them */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDLPDomainParameters( INOUT PKC_INFO *pkcInfo, 
									 const BOOLEAN isPKCS3, 
									 const BOOLEAN isFullyInitialised )
	{
	BIGNUM *p = &pkcInfo->dlpParam_p, *g = &pkcInfo->dlpParam_g;
	BIGNUM *tmp = &pkcInfo->tmp1;
	int length, bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Verify that pLen >= DLPPARAM_MIN_P, pLen <= DLPPARAM_MAX_P */
	length = BN_num_bytes( p );
	if( isShortPKCKey( length ) )
		{
		/* Special-case handling for insecure-sized public keys */
		return( CRYPT_ERROR_NOSECURE );
		}
	if( length < DLPPARAM_MIN_P || length > DLPPARAM_MAX_P )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that p is not (obviously) composite */
	if( !primeSieve( p ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that gLen >= DLPPARAM_MIN_G, gLen <= DLPPARAM_MAX_G */
	length = BN_num_bytes( g );
	if( length < DLPPARAM_MIN_G || length > DLPPARAM_MAX_G )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that g < 256 if it's straight PKCS #3 DH.  This isn't strictly 
	   necessary but use of g in DH typically sets g = 2, the only reason for 
	   setting it to a larger value is either stupidity or a deliberate DoS, 
	   neither of which we want to encourage.  FIPS 186/X9.42 use a g 
	   parameter the same size as p so we can't limit the size as for 
	   PKCS #3 */
	if( isPKCS3 && BN_num_bits( g ) > 8 )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that 2 <= g <= p - 2 */
	if( BN_num_bits( g ) < 2 )
		return( CRYPT_ARGERROR_STR1 );
	CKPTR( BN_copy( tmp, p ) );
	CK( BN_sub_word( tmp, 2 ) );
	if( bnStatusError( bnStatus ) || BN_cmp( g, tmp ) > 0 )
		return( CRYPT_ARGERROR_STR1 );

	/* If it's a PKCS #3 key then the only public values are p and g and 
	   we're done */
	if( isPKCS3 )
		return( CRYPT_OK );

	/* Verify that qLen >= DLPPARAM_MIN_Q, qLen <= DLPPARAM_MAX_Q */
	length = BN_num_bytes( &pkcInfo->dlpParam_q );
	if( length < DLPPARAM_MIN_Q || length > DLPPARAM_MAX_Q )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that q is not (obviously) composite */
	if( !primeSieve( &pkcInfo->dlpParam_q ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that g is a generator of order q.  This check requires 
	   initialisation of values that may not have been performed yet at this 
	   point, in which case the check is performed inline in the main 
	   initCheckDLPkey() code below */
	if( !isFullyInitialised )
		return( CRYPT_OK );
	CK( BN_mod_exp_mont( tmp, g, &pkcInfo->dlpParam_q, p, pkcInfo->bnCTX,
						 &pkcInfo->dlpParam_mont_p ) );
	if( bnStatusError( bnStatus ) || !BN_is_one( tmp ) )
		return( CRYPT_ARGERROR_STR1 );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDLPPublicKey( INOUT PKC_INFO *pkcInfo, 
							  const BOOLEAN isPKCS3 )
	{
	BIGNUM *p = &pkcInfo->dlpParam_p, *y = &pkcInfo->dlpParam_y;
	BIGNUM *tmp = &pkcInfo->tmp1;
	int bnStatus = BN_STATUS, length;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Verify that yLen >= DLPPARAM_MIN_Y, yLen <= DLPPARAM_MAX_Y */
	length = BN_num_bytes( y );
	if( length < DLPPARAM_MIN_Y || length > DLPPARAM_MAX_Y )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that y < p */
	if( BN_cmp( y, p ) >= 0 )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that y has the correct order in the subgroup */
	if( !isPKCS3 )
		{
		CK( BN_mod_exp_mont( tmp, y, &pkcInfo->dlpParam_q, p,
							 pkcInfo->bnCTX, &pkcInfo->dlpParam_mont_p ) );
		if( bnStatusError( bnStatus ) || !BN_is_one( tmp ) )
			return( CRYPT_ARGERROR_STR1 );
		}

	return( CRYPT_OK );
	}

/* Perform validity checks on the private key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDLPPrivateKey( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *p = &pkcInfo->dlpParam_p, *g = &pkcInfo->dlpParam_g;
	BIGNUM *x = &pkcInfo->dlpParam_x, *y = &pkcInfo->dlpParam_y;
	BIGNUM *tmp = &pkcInfo->tmp1;
	int bnStatus = BN_STATUS, length;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Verify that xLen >= DLPPARAM_MIN_X, xLen <= DLPPARAM_MAX_X */
	length = BN_num_bytes( x );
	if( length < DLPPARAM_MIN_X || length > DLPPARAM_MAX_X )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that g^x mod p == y */
	CK( BN_mod_exp_mont( tmp, g, x, p, pkcInfo->bnCTX,
						 &pkcInfo->dlpParam_mont_p ) );
	if( bnStatusError( bnStatus ) || BN_cmp( tmp, y ) )
		return( CRYPT_ARGERROR_STR1 );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Generate/Initialise a DLP Key					*
*																			*
****************************************************************************/

/* Generate and check a generic DLP key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int generateDLPkey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_LENGTH_SHORT_MIN( MIN_PKCSIZE * 8 ) const int keyBits )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	BIGNUM *p = &pkcInfo->dlpParam_p;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( keyBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keyBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* Generate the domain parameters */
	pkcInfo->keySizeBits = keyBits;
	status = generateDLPPublicValues( pkcInfo, keyBits );
	if( cryptStatusError( status ) )
		return( status );

	/* Generate the private key */
	status = generateDLPPrivateValue( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Since the keygen is randomised it may occur that the final size of 
	   the public value that determines its nominal size is slightly smaller 
	   than the requested nominal size.  To handle this we recalculate the 
	   effective key size after we've finished generating the public value
	   that determines its nominal size */
	pkcInfo->keySizeBits = BN_num_bits( p );

	/* Evaluate the Montgomery form of p and calculate y */
	CK( BN_MONT_CTX_set( &pkcInfo->dlpParam_mont_p, p, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	status = generateDLPPublicValue( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the generated values are valid */
	status = checkDLPDomainParameters( pkcInfo, FALSE, TRUE );
	if( cryptStatusOK( status ) )
		status = checkDLPPublicKey( pkcInfo, FALSE );
	if( cryptStatusOK( status ) )
		status = checkDLPPrivateKey( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Enable side-channel protection if required */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		return( CRYPT_OK );
	return( enableSidechannelProtection( pkcInfo, 
							contextInfoPtr->capabilityInfo->cryptAlgo ) );
	}

/* Initialise and check a DLP key.  If the isDH flag is set then it's a DH 
   key and we generate the x value (and by extension the y value) if it's
   not present.  If the isPKCS3 flag is set then it's a PKCS #3 key rather
   than FIPS 186/X9.42, without a q value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initCheckDLPkey( INOUT CONTEXT_INFO *contextInfoPtr, 
					 const BOOLEAN isDH, const BOOLEAN isPKCS3 )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	BIGNUM *p = &pkcInfo->dlpParam_p, *g = &pkcInfo->dlpParam_g;
	BIGNUM *x = &pkcInfo->dlpParam_x, *y = &pkcInfo->dlpParam_y;
	BIGNUM *tmp = &pkcInfo->tmp1;
	const BOOLEAN isPrivateKey = \
		( contextInfoPtr->flags & CONTEXT_FLAG_ISPUBLICKEY ) ? FALSE : TRUE;
	BOOLEAN generatedX = FALSE;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* Make sure that the necessary key parameters have been initialised.  
	   Since PKCS #3 doesn't use the q parameter we only require it for 
	   algorithms that specifically use FIPS 186 values.  DH keys are a bit
	   more complicated because they function as both public and private
	   keys so we don't require an x parameter if it's a DH key */
	if( BN_is_zero( p ) || BN_is_zero( g ) )
		return( CRYPT_ARGERROR_STR1 );
	if( !isPKCS3 && BN_is_zero( &pkcInfo->dlpParam_q ) )
		return( CRYPT_ARGERROR_STR1 );
	if( isPrivateKey && !isDH && BN_is_zero( x ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Make sure that the domain paramters are valid */
	status = checkDLPDomainParameters( pkcInfo, isPKCS3, FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Evaluate the Montgomery form of p, needed for further operations */
	CK( BN_MONT_CTX_set( &pkcInfo->dlpParam_mont_p, p, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	pkcInfo->keySizeBits = BN_num_bits( p );

	/* Special-case verification for non-PKCS #3 (i.e. FIPS 186) keys.  We 
	   have to do this at this point rather than in 
	   checkDLPDomainParameters() because it requires initialisation of 
	   values that haven't been set up yet when checkDLPDomainParameters() 
	   is called */
	if( !isPKCS3 )
		{
		/* Verify that g is a generator of order q */
		CK( BN_mod_exp_mont( tmp, g, &pkcInfo->dlpParam_q, p, pkcInfo->bnCTX,
							 &pkcInfo->dlpParam_mont_p ) );
		if( bnStatusError( bnStatus ) || !BN_is_one( tmp ) )
			return( CRYPT_ARGERROR_STR1 );
		}

	/* If it's a DH key and there's no x value present, generate one now.  
	   This is needed because all DH keys are effectively private keys.  We 
	   also update the context flags to reflect this change in status */
	if( isDH && BN_is_zero( x ) )
		{
		status = generateDLPPrivateValue( pkcInfo );
		if( cryptStatusError( status ) )
			return( status );
		contextInfoPtr->flags &= ~CONTEXT_FLAG_ISPUBLICKEY;
		generatedX = TRUE;
		}

	/* Some sources (specifically PKCS #11) don't make y available for
	   private keys so if the caller is trying to load a private key with a
	   zero y value we calculate it for them.  First, we check to make sure
	   that we have x available to calculate y */
	if( BN_is_zero( y ) && BN_is_zero( x ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Calculate y if required.  This is a bit odd because if we've 
	   generated a new x value it'll cause any existing y value to be 
	   overwritten by a new one based on the newly-generated x value.  This 
	   means that if we load a DH key from a certificate containing an 
	   existing y value then this process will overwrite the value with a 
	   new one, so that the context associated with the certificate will 
	   contain a y value that differs from the one in the certificate.  This 
	   is unfortunate, but again because of the DH key duality we need to 
	   have both an x and a y value present otherwise the key is useless and 
	   in order to get an x value we have to recreate the y value.

	   This process (and DLP public-key ops in general) can be trapdoored as
	   follows to leak the x value over two consecutive exchanges (due to 
	   Adam Young and Moti Yung, "Kleptography: Using Cryptography Against 
	   Cryptography" and "The Prevalence of Kleptographic Attacks on 
	   Discrete-Log Based Cryptosystems").  In the following, x and y are 
	   the public and private values and xA and yA are the attacker's public 
	   and private values (in this particular example x, y are used as DH 
	   values and xA and yA as Elgamal values):

		y1 = g^x1 mod p, saving x1.
		z = g^x1 * yA^-(a * x1 - b) mod p, where a, b are constants.
		x2 = hash( z ).
		y2 = g^x2 mod p.

	   The attacker intercepts the two DH values and computes:

		r = y1^a * g^b mod p.
		z = y1 / r^xA mod p.
		if y2 == g^hash( z ) mod p then x1 is hash( z ).
		z = z / g^W, where W is a fixed odd integer.
		if y2 == g^hash( z ) mod p then x1 is hash( z ).

	   This is why it's a good idea to use public code for this sort of 
	   thing... */
	if( BN_is_zero( y ) || generatedX )
		{
		status = generateDLPPublicValue( pkcInfo );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Make sure that the public key is valid */
	status = checkDLPPublicKey( pkcInfo, isPKCS3 );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the private key is valid */
	if( isPrivateKey || generatedX )
		{
		status = checkDLPPrivateKey( pkcInfo );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Enable side-channel protection if required */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		return( CRYPT_OK );
	return( enableSidechannelProtection( pkcInfo, 
							contextInfoPtr->capabilityInfo->cryptAlgo ) );
	}
#endif /* USE_DH || USE_DSA || USE_ELGAMAL */
