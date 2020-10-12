/****************************************************************************
*																			*
*				cryptlib DLP Key Generation/Checking Routines				*
*						Copyright Peter Gutmann 1997-2015					*
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
	BN_set_flags( &pkcInfo->dlpParam_x, BN_FLG_CONSTTIME );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Determine Discrete Log Exponent Bits				*
*																			*
****************************************************************************/

/* The following function (provided by Colin Plumb) is used to calculate the
   appropriate size exponent for a given prime size which is required to
   provide equivalent security from small-exponent attacks.  Note that it's
   not clear which paper Colin is referring to here, the obvious candidate
   would be "On Diffie-Hellman Key Agreement with Short Exponents" from
   EuroCrypt'96 but it's not present in there and in any case the paper is
   by van Oorschot and Wiener.  This leaves a tech report, possibly from
   Bell-Northern Research.  A more recent reference is the ENISA 
   "Algorithms, key size and parameters report", which (Table 3.1) draws on
   a number of different sources to produce more or less the same figures.

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

CHECK_RETVAL_RANGE( 160, 1000 ) \
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

	ENSURES( isIntegerRange( value ) );
	return( ( int ) value );
	}

/****************************************************************************
*																			*
*			  					Generate DL Primes							*
*																			*
*	Original findGeneratorForPQ() and generateDLPPublicValues() are 		*
*	  copyright Kevin J. Bluck 1998, provided for use in cryptlib.			*
*		  findGeneratorForPQ() has been completely rewritten, 				*
*			generateDLPPublicValues() is heavily modified.					*
*																			*
****************************************************************************/

/* DLP-based PKCs have various requirements for the generated parameters.
   For the generator g it can generate either the entire group (meaning
   g^x mod p can take any value between 1 and p-1) or the subgroup q, an
   appropriately-sized prime.  Ideally we'd choose g = 2 (standard for PKCS 
   #3 DH), however to safely use g = 2 we need a safe prime p = 2q + 1 so 
   the order of any g in the range 2 ... p-2 is either q or 2q.  If we don't 
   use a safe prime then the order of g = 2 can't really be determined 
   without first factoring p-1, in other words there's no easy way to force 
   g to a particular value.

	DSA: p, q, and g of preset lengths (currently p isn't fixed at exactly
		n * 64 bits because of the way the Lim-Lee algorithm works, it's
		possible to get this by iterating the multiplication step until the
		result is exactly n * 64 bits but this doesn't seem worth the
		effort), x = 1...q-1.
	PKCS #3 DH: No g (it's fixed at 2) or q.  Requires safe primes, see the
		comment for X9.42 values.
	X9.42 DH: p, q, and g as for DSA but without the 160-bit SHA1-enforced
		upper limit on q in original versions of the DSA specification so 
		that p can go above 1024 bits.  Newer versions of the spec 
		eventually allowed larger p and q values in conjunction with post-
		SHA1 hash algorithms.  Note that the way g is generated leads to 
		extraordinarily inefficient g values relative to PKCS #3's g = 2, 
		however to safely use g = 2 we'd need a safe prime.  x = 2...q-2.  
	Elgamal: As X9.42 DH.

   Traditionally for PKCS #3 DH g is fixed at 2 which is safe even when it's 
   not a primitive root since it still covers half of the space of possible 
   residues, however this requires the use of a safe prime so we always 
   generate a FIPS 186-style g value */

/* The minimum safe exponent size for a given-size prime */

#define MIN_EXPONENT_SIZE_BITS	160

/* The maximum number of factors required to generate a prime using the Lim-
   Lee algorithm, which evaluates to 25 for a 4 kbit max.prime size */

#define MAX_NO_FACTORS			( ( bytesToBits( CRYPT_MAX_PKCSIZE ) / \
									MIN_EXPONENT_SIZE_BITS ) + 1 )

/* The maximum number of small primes required to generate a prime using the
   Lim-Lee algorithm.  There's no fixed bound on this value, but in the worst
   case we start with 
   ~ CRYPT_MAX_PKCSIZE_bits / getDLPexpSize( CRYPT_MAX_PKCSIZE_bits ) primes 
   = ~ 13 values, and add one more prime on each retry.  Typically we need 
   10-15 for keys in the most commonly-used range 1024-2048 bits.  In order 
   to simplify the handling of values we allow for 64 primes, which has a 
   vanishingly small probability of failing and also provides a safe upper 
   bound for the number of retries (there's something wrong with the 
   algorithm if it requires anywhere near this many retries) */

#define MAX_NO_PRIMES			64

/* Select a generator g for the prime moduli p and q.  g will be chosen to 
   be of prime order q, where q divides (p - 1), i.e. g generates the 
   subgroup of order q in the multiplicative group of GF(p) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int findGeneratorForPQ( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *p = &pkcInfo->dlpParam_p, *q = &pkcInfo->dlpParam_q;
	BIGNUM *g = &pkcInfo->dlpParam_g;
	BIGNUM *j = &pkcInfo->tmp1;
	BN_ULONG gCounter;
	int bnStatus = BN_STATUS, LOOP_ITERATOR;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );

	/* j = (p - 1) / q */
	CKPTR( BN_copy( j, p ) );
	CK( BN_sub_word( j, 1 ) );
	CK( BN_div( j, NULL, j, q, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Starting gCounter at 3, set g = (gCounter ^ j) mod p until g != 1.
	   Although FIPS 196/X9.30/X9.42 merely require that 1 < g < p - 1, if 
	   we use small integers it makes this operation much faster */
	LOOP_MED( gCounter = 3, gCounter < FAILSAFE_ITERATIONS_MED, gCounter++ )
		{
		CK( BN_mod_exp_mont_word( g, gCounter, j, p, &pkcInfo->bnCTX, 
								  &pkcInfo->dlpParam_mont_p ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		if( BN_cmp_word( g, 1 ) > 0 )
			break;
		}
	ENSURES( LOOP_BOUND_OK );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

/* Generate prime numbers for DLP-based PKC's using the Lim-Lee algorithm:

	p = 2 * q * ( prime[1] * ... prime[n] ) + 1 

   This function allows the generation of reproducible (NUMS) prime values 
   if getRandomInfo is provided as a randomness source instead of the 
   default built-in source */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateDLPPublicValues( INOUT PKC_INFO *pkcInfo, 
									IN_LENGTH_SHORT_MIN( MIN_PKCSIZE * 8 ) \
										const int pBits,
									IN_OPT const GET_RANDOM_INFO *getRandomInfo )
	{
	const int safeExpSizeBits = getDLPexpSize( pBits );
	const int noChecks = getNoPrimeChecks( pBits );
	BIGNUM llPrimes[ MAX_NO_PRIMES + 8 ], llProducts[ MAX_NO_FACTORS + 8 ];
	BIGNUM *p = &pkcInfo->dlpParam_p, *q = &pkcInfo->dlpParam_q;
	BOOLEAN primeFound;
	int indices[ MAX_NO_FACTORS + 8 ];
	int nPrimes, nFactors, factorBits, i, LOOP_ITERATOR;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( getDLPexpSize( 1024 ) == MIN_EXPONENT_SIZE_BITS );
			/* 1024-bit keys have special handling for pre-FIPS 186-3
			   compatibility */
	assert( getDLPexpSize( 1030 ) == 168 );
			/* First size over MIN_EXPONENT_SIZE_BITS */
	assert( getRandomInfo == NULL || \
			isReadPtr( getRandomInfo, sizeof( GET_RANDOM_INFO ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( pBits >= bytesToBits( MIN_PKCSIZE ) && \
			  pBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );
	REQUIRES( safeExpSizeBits >= MIN_EXPONENT_SIZE_BITS && \
			  safeExpSizeBits < 512 );

	ENSURES( getDLPexpSize( 1024 ) == MIN_EXPONENT_SIZE_BITS );
			 /* 1024-bit keys have special handling for pre-FIPS 186-3 
				compatibility */
	ENSURES( getDLPexpSize( 1536 ) == 198 );
	ENSURES( getDLPexpSize( 2048 ) == 225 );
	ENSURES( getDLPexpSize( 3072 ) == 270 );
	ENSURES( getDLPexpSize( 4096 ) == 305 );

	/* Determine how many factors we need and the size in bits of the 
	   factors.  For the range checks below, the minimum value for
	   factorBits is 860, the minimum value for nFactors is 5 when
	   MIN_PKCSIZE is 1024 */
	factorBits = ( pBits - safeExpSizeBits ) - 1;
	ENSURES( factorBits >= bytesToBits( MIN_PKCSIZE ) - \
							( MIN_EXPONENT_SIZE_BITS + 1 ) && \
			 factorBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );
	nFactors = nPrimes = ( factorBits / safeExpSizeBits ) + 1;
	ENSURES( nFactors >= ( bytesToBits( MIN_PKCSIZE ) - MIN_EXPONENT_SIZE_BITS ) / \
							MIN_EXPONENT_SIZE_BITS && \
			 nFactors <= MAX_NO_FACTORS );
	ENSURES( nPrimes > ( bytesToBits( MIN_PKCSIZE ) - MIN_EXPONENT_SIZE_BITS ) / \
							MIN_EXPONENT_SIZE_BITS && \
			 nPrimes <= MAX_NO_PRIMES );
	factorBits /= nFactors;

	/* Generate a random prime q and multiply it by 2 to form the base for 
	   the other factors */
	status = generatePrime( pkcInfo, q, safeExpSizeBits, getRandomInfo );
	if( cryptStatusError( status ) )
		return( status );
	CK( BN_lshift1( q, q ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Set up the permutation control arrays and generate the first nFactors 
	   factors */
	LOOP_LARGE( i = 0, i < MAX_NO_FACTORS, i++ )
		BN_init( &llProducts[ i ] );
	ENSURES( LOOP_BOUND_OK );
	LOOP_LARGE( i = 0, i < MAX_NO_PRIMES, i++ )
		BN_init( &llPrimes[ i ] );
	ENSURES( LOOP_BOUND_OK );
	LOOP_LARGE( i = 0, i < nFactors, i++ )
		{
		status = generatePrime( pkcInfo, &llPrimes[ i ], factorBits, 
								getRandomInfo );
		if( cryptStatusError( status ) )
			goto cleanup;
		}
	ENSURES( LOOP_BOUND_OK );

	LOOP_LARGE_INITCHECK( primeFound = FALSE, !primeFound )
		{
		int indexMoved, LOOP_ITERATOR_ALT;

		/* Initialize the indices for the permutation.  We try the first 
		   nFactors factors first since any new primes will be added at the 
		   end.  Since initially nFactors == nPrimes, this is just the
		   identity mapping { 0, 1, 2, 3, 4, ... } until we start adding
		   more primes if we run out of permutations to test */
		indices[ nFactors - 1 ] = nPrimes - 1;
		LOOP_LARGE_ALT( i = nFactors - 2, i >= 0, i-- )
			{
			indices[ i ] = indices[ i + 1 ] - 1;
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		CK( BN_mul( &llProducts[ nFactors - 1 ], q, &llPrimes[ nPrimes - 1 ], 
					&pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			{
			status = getBnStatus( bnStatus );
			goto cleanup;
			}
		indexMoved = nFactors - 2;

		/* Test all possible new prime permutations until a prime is found or 
		   we run out of permutations */
		LOOP_MAX_CHECK_ALT( indices[ nFactors - 1 ] > 0 )
			{
			int LOOP_ITERATOR_ALT2;

			/* Assemble a new candidate prime 2 * q * primes + 1 from the 
			   currently indexed random primes */
			LOOP_LARGE_ALT2( i = indexMoved, 
							 bnStatusOK( bnStatus ) && i >= 0, i-- )
				{
				CK( BN_mul( &llProducts[ i ], &llProducts[ i + 1 ],
							&llPrimes[ indices[ i ] ], &pkcInfo->bnCTX ) );
				}
			ENSURES( LOOP_BOUND_OK_ALT2 );
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
				status = primeProbable( pkcInfo, p, noChecks, &primeFound );
				if( cryptStatusError( status ) )
					goto cleanup;
				if( primeFound )
					break;
				}

			/* Find the lowest index which is not already at the lowest 
			   possible point and move it down one */
			LOOP_LARGE_ALT2( i = 0, i < nFactors, i++ )
				{
				if( indices[ i ] > i )
					{
					indices[ i ]--;
					indexMoved = i;
					break;
					}
				}
			ENSURES( LOOP_BOUND_OK_ALT2 );

			/* If we moved down the highest index then we've exhausted all 
			   of the permutations so we have to start over with another 
			   prime */
			if( ( indexMoved >= nFactors - 1 ) || ( i >= nFactors ) )
				break;

			/* We haven't changed the highest index, take all of the indices 
			   below the one that we moved down and move them up so that 
			   they're packed up as high as they'll go */
			LOOP_LARGE_ALT2( i = indexMoved - 1, i >= 0, i-- )
				{
				indices[ i ] = indices[ i + 1 ] - 1;
				}
			ENSURES( LOOP_BOUND_OK_ALT2 );
			} 
		ENSURES( LOOP_BOUND_OK_ALT );

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
			status = generatePrime( pkcInfo, &llPrimes[ nPrimes++ ], 
									factorBits, getRandomInfo );
			if( cryptStatusError( status ) )
				goto cleanup;
			}
		}
	ENSURES( LOOP_BOUND_OK );

	/* Recover the original value of q by dividing by 2 */
	CK( BN_rshift1( q, q ) );
	if( bnStatusError( bnStatus ) )
		{
		status = getBnStatus( bnStatus );
		goto cleanup;
		}

	/* In theory now that we've got p we need to evaluate its Montgomery 
	   form, however this has already been done as a side-effect of the
	   primality checking in primeProbable(), so all we do here is verify 
	   that it's indeed set */
	ENSURES( !BN_cmp( &pkcInfo->dlpParam_mont_p.N, p ) );

	/* Find a generator suitable for p and q */
	status = findGeneratorForPQ( pkcInfo );

cleanup:

	/* Free the local storage */
	LOOP_LARGE( i = 0, i < MAX_NO_PRIMES, i++ )
		BN_clear_free( &llPrimes[ i ] );
	ENSURES( LOOP_BOUND_OK );
	LOOP_LARGE( i = 0, i < MAX_NO_FACTORS, i++ )
		BN_clear_free( &llProducts[ i ] );
	ENSURES( LOOP_BOUND_OK );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

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

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( ( qBits == 0 ) || \
			  ( qBits >= MIN_EXPONENT_SIZE_BITS && \
				qBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) ) );

	/* If it's a PKCS #3 DH key then there won't be a q value present so we 
	   have to estimate the appropriate x size in the same way that we 
	   estimated the q size when we generated the public key components */
	if( qBits == 0 )
		{
		const DH_DOMAINPARAMS *domainParams = pkcInfo->domainParams;
		const BIGNUM *p = ( domainParams != NULL ) ? \
						  &domainParams->p : &pkcInfo->dlpParam_p;
		const int pLen = BN_num_bits( p );

		REQUIRES( pLen >= bytesToBits( MIN_PKCSIZE ) && \
				  pLen <= bytesToBits( CRYPT_MAX_PKCSIZE ) );
		return( generateBignum( x, getDLPexpSize( pLen ), 0xC0, 0 ) );
		}

	/* Generate the DLP private value x s.t. 2 <= x <= q-2 (this is the 
	   lowest common denominator of FIPS 186's 1...q-1 and X9.42's 2...q-2). 
	   Because the mod q-2 is expensive we do a quick check to make sure 
	   that it's really necessary before evaluating it */
	status = generateBignum( x, qBits, 0xC0, 0 );
	if( cryptStatusError( status ) )
		return( status );
	CK( BN_sub_word( q, 2 ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( BN_cmp( x, q ) > 0 )
		{
		int xLen;

		/* Trim x down to size.  Actually we get the upper bound as q - 3, 
		   but over a MIN_EXPONENT_SIZE_BITS (minimum) number range this 
		   doesn't matter */
		CK( BN_mod( x, x, q, &pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );

		/* If the value that we ended up with is too small, just generate a 
		   new value one bit shorter, which guarantees that it'll fit the 
		   criteria (the target is a suitably large random value, not the 
		   closest possible fit within the range) */
		xLen = BN_num_bits( x );
		REQUIRES( xLen > 0 && xLen <= qBits );
		if( xLen < qBits - 5 )
			status = generateBignum( x, qBits - 1, 0xC0, 0 );
		}
	CK( BN_add_word( q, 2 ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateDLPPublicValue( INOUT PKC_INFO *pkcInfo )
	{
	const DH_DOMAINPARAMS *domainParams = pkcInfo->domainParams;
	const BIGNUM *p = ( domainParams != NULL ) ? \
					  &domainParams->p : &pkcInfo->dlpParam_p;
	const BIGNUM *g = ( domainParams != NULL ) ? \
					  &domainParams->g : &pkcInfo->dlpParam_g;
	BIGNUM *x = &pkcInfo->dlpParam_x, *y = &pkcInfo->dlpParam_y; 
	int bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );

	CK( BN_mod_exp_mont( y, g, x, p, &pkcInfo->bnCTX, 
						 &pkcInfo->dlpParam_mont_p ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Check a DLP Key								*
*																			*
****************************************************************************/

/* Perform validity checks on the public key.  This function is only called 
   for nonstandard, custom domain parameters that don't match the known-good
   built-in values.  
   
   We have to make the PKC_INFO data non-const because the bignum code wants 
   to modify some of the values as it's working with them */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDLPDomainParameters( INOUT PKC_INFO *pkcInfo, 
									 const BOOLEAN isPKCS3, 
									 const BOOLEAN isFullyInitialised )
	{
	BIGNUM *p = &pkcInfo->dlpParam_p, *q = &pkcInfo->dlpParam_q;
	BIGNUM *g = &pkcInfo->dlpParam_g, *tmp = &pkcInfo->tmp1;
	const int gBits = BN_num_bits( g );
	BOOLEAN isPrime;
	int length, bnStatus = BN_STATUS, status;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( isPKCS3 == TRUE || isPKCS3 == FALSE );
	REQUIRES( isFullyInitialised == TRUE || isFullyInitialised == FALSE );
	REQUIRES( gBits > 0 && gBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* Verify that pLen >= DLPPARAM_MIN_P, pLen <= DLPPARAM_MAX_P */
	length = BN_num_bytes( p );
	if( isShortPKCKey( length ) )
		{
		/* Special-case handling for insecure-sized public keys */
		return( CRYPT_ERROR_NOSECURE );
		}
	if( length < DLPPARAM_MIN_P || length > DLPPARAM_MAX_P )
		return( CRYPT_ARGERROR_STR1 );

#ifndef CONFIG_FUZZ
	/* Verify that p is not (obviously) composite.  Note that we can't 
	   use primeProbable() because this updates the Montgomery CTX data, 
	   it's OK to use it early in the keygen process before everything is 
	   set up but not after the pkcInfo is fully initialised so we use
	   primeProbableFermat() instead */
	if( !primeSieve( p ) )
		return( CRYPT_ARGERROR_STR1 );
	if( isFullyInitialised )
		{
		status = primeProbableFermat( pkcInfo, p, &pkcInfo->dlpParam_mont_p, 
									  &isPrime );
		if( cryptStatusError( status ) )
			return( status );
		if( !isPrime )
			return( CRYPT_ARGERROR_STR1 );
		}
#else
	/* If we're fuzzing we skip the relatively expensive non-prime check, 
	   but we still have to at least ensure that the value won't trigger
	   an exception in subsequent code */
	if( !BN_is_odd( p ) )
		return( CRYPT_ARGERROR_STR1 );
#endif /* CONFIG_FUZZ */

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
	if( isPKCS3 && gBits > 8 )
		{
		assert_nofuzz( DEBUG_WARN );	/* Warn in debug build */
		return( CRYPT_ARGERROR_STR1 );
		}

	/* Verify that 2 <= g <= p - 2 */
	if( gBits < 2 )
		return( CRYPT_ARGERROR_STR1 );
	CKPTR( BN_copy( tmp, p ) );
	CK( BN_sub_word( tmp, 2 ) );
	if( bnStatusError( bnStatus ) )
		return( CRYPT_ARGERROR_STR1  );
	if( BN_cmp( g, tmp ) > 0 )
		return( CRYPT_ARGERROR_STR1 );

	/* If it's a PKCS #3 key then the only public values are p and g and 
	   we're done */
	if( isPKCS3 )
		{
		/* There is a further check that we can perform on p, but it's 
		   rather problematic because it only works for safe primes of the 
		   form p = 2q + 1, and even then the checks depend on your 
		   religious inclinations, you've got the choice of either choosing 
		   a value where the generated DH secret is limited to half the 
		   possible values, or one where you leak a bit of the secret 
		   exponent.  
		   
		   For example for g=2, if p is congruent to 11 mod 24 then g is a 
		   quadratic nonresidue and the DH secret covers all possible values 
		   but you leak the LSB of the secret exponent, but if p is 
		   congruent to 11 mod 23 then g is a quadratic residue and the DH 
		   secret only covers half the possible values, but you don't leak 
		   any bits of the exponent (for OpenSSH's g=5, the values are 3 and 
		   7).

		   Once you go to more general values of g, or FIPS 186 primes which
		   should be easily verifiable but aren't because the PKCS #3 form
		   discards the q value that you need for the verification, there 
		   isn't really any checking that can be done.  The result is an ugly
		   yes-biased test that can say "definitely safe" but only "possibly
		   unsafe" (unless we're willing to deal with lots of false 
		   positives).

		   Because of this we only complain about problems in debug mode, if we
		   enabled the rejection of unverifiable primes in release code we'd 
		   Get Letters... */
#ifndef NDEBUG
		{
		BN_ULONG modWord DUMMY_INIT;

		switch( BN_get_word( g ) )
			{
			case 2:
				/* Oakley primes, congruent to 11 mod 24 = leaks LSB, 
				   congruent to 23 mod 24 = only covers half the possible 
				   values */
				CK( BN_mod_word( &modWord, p, 24 ) );
				assert( !bnStatusError( bnStatus ) );
				assert_nofuzz( modWord == 11 || modWord == 23 );
				break;

			case 5:
				/* Used by OpenSSH for no known reason */
				CK( BN_mod_word( &modWord, p, 10 ) );
				assert( !bnStatusError( bnStatus ) );
				assert_nofuzz( modWord == 3 || modWord == 7 );
				break;

			default:
				assert_nofuzz( DEBUG_WARN );
			}
		}

	/* Now we could do a full test for a safe prime, so both p and p' are 
	   primes, but this is very slow and given the inability to reject 
	   values that don't check out, it's not certain what would be gained
	   by it */
#endif /* !NDEBUG */
		return( CRYPT_OK );
		}

	/* Verify that qLen >= DLPPARAM_MIN_Q, qLen <= DLPPARAM_MAX_Q */
	length = BN_num_bytes( q );
	if( length < DLPPARAM_MIN_Q || length > DLPPARAM_MAX_Q )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that q is not (obviously) composite.  Note that we can't 
	   use primeProbable() because this updates the Montgomery CTX data, 
	   it's OK to use it early in the keygen process before everything is 
	   set up but not after the pkcInfo is fully initialised so we use
	   primeProbableFermat() instead */
	if( !primeSieve( q ) )
		return( CRYPT_ARGERROR_STR1 );
	assert( BN_is_zero( &( pkcInfo->montCTX2.N ) ) );
	CK( BN_MONT_CTX_set( &pkcInfo->montCTX2, q, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( CRYPT_ARGERROR_STR1 );
	status = primeProbableFermat( pkcInfo, q, &pkcInfo->montCTX2, &isPrime );
	if( cryptStatusError( status ) )
		return( status );
	if( !isPrime )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that g is a generator of order q, i.e. that g^q mod p == 1.  
	   This check requires initialisation of values that may not have been 
	   performed yet at this point, in which case the check is performed 
	   inline in the main initCheckDLPkey() code below */
	if( !isFullyInitialised )
		return( CRYPT_OK );
	CK( BN_mod_exp_mont( tmp, g, q, p, &pkcInfo->bnCTX,
						 &pkcInfo->dlpParam_mont_p ) );
	if( bnStatusError( bnStatus ) )
		return( CRYPT_ARGERROR_STR1 );
	if( !BN_is_one( tmp ) )
		return( CRYPT_ARGERROR_STR1 );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDLPPublicKey( INOUT PKC_INFO *pkcInfo, 
							  const BOOLEAN isPKCS3 )
	{
	const DH_DOMAINPARAMS *domainParams = pkcInfo->domainParams;
	const BIGNUM *p = ( domainParams != NULL ) ? \
					  &domainParams->p : &pkcInfo->dlpParam_p;
	BIGNUM *y = &pkcInfo->dlpParam_y, *tmp = &pkcInfo->tmp1;
	int bnStatus = BN_STATUS, length;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( isPKCS3 == TRUE || isPKCS3 == FALSE );

	/* Verify that yLen >= DLPPARAM_MIN_Y, yLen <= DLPPARAM_MAX_Y */
	length = BN_num_bytes( y );
	if( length < DLPPARAM_MIN_Y || length > DLPPARAM_MAX_Y )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that y < p */
	if( BN_cmp( y, p ) >= 0 )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that y has the correct order in the subgroup, i.e. that
	   y ^ q mod p == 1 */
	if( !isPKCS3 )
		{
		CK( BN_mod_exp_mont( tmp, y, &pkcInfo->dlpParam_q, p,
							 &pkcInfo->bnCTX, &pkcInfo->dlpParam_mont_p ) );
		if( bnStatusError( bnStatus ) )
			return( CRYPT_ARGERROR_STR1 );
		if( !BN_is_one( tmp ) )
			return( CRYPT_ARGERROR_STR1 );
		}

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

/* Perform validity checks on the private key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDLPPrivateKey( INOUT PKC_INFO *pkcInfo )
	{
	const DH_DOMAINPARAMS *domainParams = pkcInfo->domainParams;
	const BIGNUM *p = ( domainParams != NULL ) ? \
					  &domainParams->p : &pkcInfo->dlpParam_p;
	const BIGNUM *g = ( domainParams != NULL ) ? \
					  &domainParams->g : &pkcInfo->dlpParam_g;
	BIGNUM *x = &pkcInfo->dlpParam_x, *y = &pkcInfo->dlpParam_y;
	BIGNUM *tmp = &pkcInfo->tmp1;
	int bnStatus = BN_STATUS, length;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );

	/* Verify that xLen >= DLPPARAM_MIN_X, xLen <= DLPPARAM_MAX_X */
	length = BN_num_bytes( x );
	if( length < DLPPARAM_MIN_X || length > DLPPARAM_MAX_X )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that g^x mod p == y */
#ifndef CONFIG_FUZZ
	CK( BN_mod_exp_mont( tmp, g, x, p, &pkcInfo->bnCTX,
						 &pkcInfo->dlpParam_mont_p ) );
	if( bnStatusError( bnStatus ) )
		return( CRYPT_ARGERROR_STR1 );
	if( BN_cmp( tmp, y ) )
		return( CRYPT_ARGERROR_STR1 );
#endif /* CONFIG_FUZZ */

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

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
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	BIGNUM *p = &pkcInfo->dlpParam_p;
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( keyBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keyBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );
	REQUIRES( capabilityInfoPtr != NULL );

	/* Generate the domain parameters */
	pkcInfo->keySizeBits = keyBits;
	status = generateDLPPublicValues( pkcInfo, keyBits, NULL );
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
	ENSURES( pkcInfo->keySizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			 pkcInfo->keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* Calculate y */
	status = generateDLPPublicValue( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Enable side-channel protection if required */
	if( TEST_FLAG( contextInfoPtr->flags, 
				   CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		{
		status = enableSidechannelProtection( pkcInfo, 
											  capabilityInfoPtr->cryptAlgo );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Checksum the bignums to try and detect fault attacks.  Since we're
	   setting the checksum at this point there's no need to check the 
	   return value */
	( void ) checksumContextData( pkcInfo, capabilityInfoPtr->cryptAlgo, 
								  TRUE );

	/* Make sure that the generated values are valid */
	status = checkDLPDomainParameters( pkcInfo, FALSE, TRUE );
	if( cryptStatusOK( status ) )
		status = checkDLPPublicKey( pkcInfo, FALSE );
	if( cryptStatusOK( status ) )
		status = checkDLPPrivateKey( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that what we generated is still valid */
	if( cryptStatusError( \
			checksumContextData( pkcInfo, capabilityInfoPtr->cryptAlgo, 
								 TRUE ) ) )
		{
		DEBUG_DIAG(( "Generated DLP key memory corruption detected" ));
		return( CRYPT_ERROR_FAILED );
		}

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

/* Initialise and check a DLP key.  If the isDH flag is set then it's a DH 
   key and we generate the x value (and by extension the y value) if it's
   not present.  If the isPKCS3 flag is set then it's a PKCS #3 key rather
   than FIPS 186/X9.42, without a q value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initCheckDLPkey( INOUT CONTEXT_INFO *contextInfoPtr, 
					 const BOOLEAN isPKCS3 )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	const DH_DOMAINPARAMS *domainParams = pkcInfo->domainParams;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const BIGNUM *p = &pkcInfo->dlpParam_p, *g = &pkcInfo->dlpParam_g;
	BIGNUM *x = &pkcInfo->dlpParam_x, *y = &pkcInfo->dlpParam_y;
	BIGNUM *tmp = &pkcInfo->tmp1;
	const BOOLEAN isPrivateKey = TEST_FLAG( contextInfoPtr->flags, 
											CONTEXT_FLAG_ISPUBLICKEY ) ? \
								 FALSE : TRUE;
	BOOLEAN isDH, generatedX = FALSE;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isPKCS3 == TRUE || isPKCS3 == FALSE );
	REQUIRES( capabilityInfoPtr != NULL );

	isDH = ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DH ) ? TRUE : FALSE;

	/* Make sure that the necessary key parameters have been initialised.  
	   Since PKCS #3 doesn't use the q parameter we only require it for 
	   algorithms that specifically use FIPS 186 values.  DH keys are a bit
	   more complicated because they function as both public and private
	   keys so we don't require an x parameter if it's a DH key */
	if( domainParams == NULL )
		{ 
		if( BN_is_zero( p ) || BN_is_zero( g ) )
			return( CRYPT_ARGERROR_STR1 );
		if( !isPKCS3 && BN_is_zero( &pkcInfo->dlpParam_q ) )
			return( CRYPT_ARGERROR_STR1 );
		}
	if( isPrivateKey && !isDH && BN_is_zero( x ) )
		return( CRYPT_ARGERROR_STR1 );

	/* If we're loading user-supplied parameters, check whether they match 
	   any of the fixed domain parameters, and switch to those if they do */
#if defined( USE_SSH ) || defined( USE_SSL )
	if( domainParams == NULL )
		{
		status = checkFixedDHparams( pkcInfo, &domainParams );
		if( cryptStatusError( status ) )
			{
			if( status != OK_SPECIAL )
				return( status );

			/* The user-supplied parameters match a set of fixed domain
			   parameters, use those directly */
			pkcInfo->domainParams = domainParams;
			BN_clear( &pkcInfo->dlpParam_p );
			BN_clear( &pkcInfo->dlpParam_q );
			BN_clear( &pkcInfo->dlpParam_g );
			}
		}
#endif /* USE_SSH || USE_SSL */
	if( domainParams != NULL )
		{
		/* We're using fixed, known-good parameters */
		p = &domainParams->p; 
		g = &domainParams->g;
		}
	else
		{
		/* We're using non-fixed domain parameters, make sure that they're 
		   valid */
		status = checkDLPDomainParameters( pkcInfo, isPKCS3, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Evaluate the Montgomery form of p, needed for further operations */
	CK( BN_MONT_CTX_set( &pkcInfo->dlpParam_mont_p, p, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( CRYPT_ARGERROR_STR1 );
	pkcInfo->keySizeBits = BN_num_bits( p );
	ENSURES( pkcInfo->keySizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			 pkcInfo->keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* Additional verification that would normally be performed in 
	   checkDLPDomainParameters() but that requires initialisation of values 
	   that haven't been set up yet when checkDLPDomainParameters() is 
	   called */
	if( domainParams == NULL )
		{
		BOOLEAN isPrime;

		/* Verify that p is (probably) prime */
		status = primeProbableFermat( pkcInfo, p, &pkcInfo->dlpParam_mont_p, 
									  &isPrime );
		if( cryptStatusError( status ) )
			return( status );
		if( !isPrime )
			return( CRYPT_ARGERROR_STR1 );

		/* Additional verification that we can perform for non-PKCS #3 (i.e. 
		   FIPS 186) keys, verify that g is a generator of order q */
		if( !isPKCS3 )
			{
			CK( BN_mod_exp_mont( tmp, g, &pkcInfo->dlpParam_q, p, &pkcInfo->bnCTX,
								 &pkcInfo->dlpParam_mont_p ) );
			if( bnStatusError( bnStatus ) )
				return( CRYPT_ARGERROR_STR1 );
			if( !BN_is_one( tmp ) )
				return( CRYPT_ARGERROR_STR1 );
			}
		}

	/* If it's a DH key and there's no x value present, generate one now.  
	   This is needed because all DH keys are effectively private keys.  We 
	   also update the context flags to reflect this change in status */
	if( isDH && BN_is_zero( x ) )
		{
		status = generateDLPPrivateValue( pkcInfo );
		if( cryptStatusError( status ) )
			return( status );
		CLEAR_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY );
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
	   in order to get an x value we have to recreate the y value.  In any 
	   case the use of DH in certificates is nonexistent, so it's not a 
	   condition that we're likely to encounter.

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
	if( TEST_FLAG( contextInfoPtr->flags, 
				   CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		{
		status = enableSidechannelProtection( pkcInfo, 
											  capabilityInfoPtr->cryptAlgo );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Checksum the bignums to try and detect fault attacks.  Since we're
	   setting the checksum at this point there's no need to check the 
	   return value.  Note that this isn't the TOCTOU issue that it appears 
	   to be because the bignum values are read by the calling code from 
	   their stored form a second time and compared to the values that we're 
	   checksumming here */
	( void ) checksumContextData( pkcInfo, capabilityInfoPtr->cryptAlgo, 
								  ( isPrivateKey || generatedX ) ? \
									TRUE : FALSE );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}
#endif /* USE_DH || USE_DSA || USE_ELGAMAL */
