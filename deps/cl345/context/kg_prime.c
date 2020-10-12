/****************************************************************************
*																			*
*					cryptlib Prime Generation/Checking Routines				*
*						Copyright Peter Gutmann 1997-2016					*
*																			*
****************************************************************************/

/* The Usenet Oracle has pondered your question deeply.
   Your question was:

   > O Oracle Most Wise,
   >
   > What is the largest prime number?

   And in response, thus spake the Oracle:

   } This is a question which has stumped some of the best minds in
   } mathematics, but I will explain it so that even you can understand it.
   } The first prime is 2, and the binary representation of 2 is 10.
   } Consider the following series:
   }
   }	Prime	Decimal Representation	Representation in its own base
   }	1st		2						10
   }	2nd		3						10
   }	3rd		5						10
   }	4th		7						10
   }	5th		11						10
   }	6th		13						10
   }	7th		17						10
   }
   } From this demonstration you can see that there is only one prime, and
   } it is ten. Therefore, the largest prime is ten.
													-- The Usenet Oracle */

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

/* Enable the following to cross-check the Miller-Rabin test using an 
   alternative form of the Miller-Rabin test that merges the test loop and 
   the modexp at the start.  Note that this displays diagnostic timing 
   output and expects to use Pentium performance counters for timing so it's 
   only (optionally) enabled for Win32 debug */

#if defined( __WIN32__ ) && !defined( NDEBUG ) && 0
  #define CHECK_PRIMETEST
#endif /* Win32 debug */

#ifdef USE_PKC

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* BN_cmp() is a bit of a tricky function because there's no way to signal
   an error from it.  This means that when performing a compare for 
   primality-testing purposes an error return looks like a valid compare 
   result.  To deal with this, we implement a fail-open version of BN_cmp() 
   that validates its parameters as BN_cmp() would and returns a compare-
   failed status, specifically a less-than result, if there's a problem.  
   Since the primality-checking functions treat a compare-equal (status == 
   0) as success, this means that we can never erroneously report a non-
   prime as prime due to an error occurring */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int BN_cmp_checked( const BIGNUM *bignum1, const BIGNUM *bignum2 )
	{
	if( !sanityCheckBignum( bignum1 ) || !sanityCheckBignum( bignum2 ) )
		return( -1 );
	return( BN_cmp( bignum1, bignum2 ) );
	}

/* Get random data, used for seeding bignums for prime generation.  This is
   just a wrapper for krnlSendMessage() that can be overridden by a user-
   defined function if required, for example for deterministic DLP parameter
   generation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int getRandomData( INOUT_OPT void *dummy, 
						  OUT_BUFFER_FIXED( noBytes ) void *buffer, 
						  IN_LENGTH_PKC const int noBytes )
	{
	MESSAGE_DATA msgData;

	assert( isWritePtrDynamic( buffer, noBytes ) );

	REQUIRES( dummy == NULL );
	REQUIRES( noBytes > 0 && noBytes <= CRYPT_MAX_PKCSIZE );
	
	setMessageData( &msgData, buffer, noBytes );
	return( krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							 IMESSAGE_GETATTRIBUTE_S, &msgData,
							 CRYPT_IATTRIBUTE_RANDOM ) );
	}

#ifdef CHECK_PRIMETEST

/* Witness function, modified from original BN code as found at a UFO crash 
   site.  This looks nothing like a standard Miller-Rabin test because it 
   merges the modexp that usually needs to be performed as the first 
   portion of the test process and the remainder of the checking.  Destroys 
   param6 + 7 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5, 6, 7 ) ) \
static int witnessOld( INOUT PKC_INFO *pkcInfo, INOUT BIGNUM *a, 
					   INOUT BIGNUM *n1, INOUT BIGNUM *mont_n1, 
					   INOUT BIGNUM *mont_1, INOUT BN_MONT_CTX *montCTX_n )
	{
	BIGNUM *y = &pkcInfo->param6;
	BIGNUM *yPrime = &pkcInfo->param7;		/* Safe to destroy */
	BN_CTX *ctx = &pkcInfo->bnCTX;
	BIGNUM *mont_a = &ctx->bn[ ctx->tos++ ];
	const int k = BN_num_bits( n1 );
	int i, bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isWritePtr( a, sizeof( BIGNUM ) ) );
	assert( isWritePtr( n, sizeof( BIGNUM ) ) );
	assert( isWritePtr( n1, sizeof( BIGNUM ) ) );
	assert( isWritePtr( mont_n1, sizeof( BIGNUM ) ) );
	assert( isWritePtr( mont_1, sizeof( BIGNUM ) ) );
	assert( isWritePtr( montCTX_n, sizeof( BN_MONT_CTX ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( sanityCheckBignum( a ) );
	REQUIRES( sanityCheckBignum( n1 ) );
	REQUIRES( sanityCheckBignum( mont_n1 ) );
	REQUIRES( sanityCheckBignum( mont_1 ) );
	REQUIRES( sanityCheckBNMontCTX( montCTX_n ) );
	REQUIRES( k > 0 && k <= bytesToBits( CRYPT_MAX_PKCSIZE );

	/* All values are manipulated in their Montgomery form so before we 
	   begin we have to convert a to this form as well */
	if( !BN_to_montgomery( mont_a, a, montCTX_n, &pkcInfo->bnCTX ) )
		{
		ctx->tos--;
		return( CRYPT_ERROR_FAILED );
		}

	CKPTR( BN_copy( y, mont_1 ) );
	for ( i = k - 1; i >= 0; i-- )
		{
		/* Perform the y^2 mod n check.  yPrime = y^2 mod n, if yPrime == 1
		   it's composite (this condition is virtually never met) */
		CK( BN_mod_mul_montgomery( yPrime, y, y, montCTX_n, 
								   &pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) || \
			( !BN_cmp( yPrime, mont_1 ) && \
			  BN_cmp( y, mont_1 ) && BN_cmp( y, mont_n1 ) ) )
			{
			ctx->tos--;
			return( TRUE );
			}

		/* Perform another step of the modexp */
		if( BN_is_bit_set( n1, i ) )
			{
			CK( BN_mod_mul_montgomery( y, yPrime, mont_a, montCTX_n, 
									   &pkcInfo->bnCTX ) );
			if( bnStatusError( bnStatus ) )
				{
				ctx->tos--;
				return( TRUE );
				}
			}
		else
			{
			BIGNUM *tmp;

			/* Input and output to modmult can't be the same, so we have to
			   swap the pointers */
			tmp = y; y = yPrime; yPrime = tmp;
			}
		}
	ctx->tos--;

	/* Finally we have y = a^u mod n.  If y == 1 (mod n) it's prime,
	   otherwise it's composite */
	return( BN_cmp( y, mont_1 ) ? TRUE : FALSE );
	}

/* Perform noChecks iterations of the Miller-Rabin probabilistic primality 
   test.  Destroys param8, tmp1-3, mont1 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int primeProbableOld( INOUT PKC_INFO *pkcInfo, 
							 INOUT BIGNUM *candidate, 
							 IN_RANGE( 1, 100 ) const int noChecks )
	{
	BIGNUM *check = &pkcInfo->tmp1;
	BIGNUM *candidate_1 = &pkcInfo->tmp2;
	BIGNUM *mont_candidate_1 = &pkcInfo->tmp3;
	BIGNUM *mont_1 = &pkcInfo->param8;		/* Safe to destroy */
	BN_MONT_CTX *montCTX_candidate = &pkcInfo->montCTX1;
	int i, bnStatus = BN_STATUS, status, LOOP_ITERATOR;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isWritePtr( candidate, sizeof( BIGNUM ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( noChecks >= 1 && noChecks <= 100 );

	/* Set up various values */
	CK( BN_MONT_CTX_set( montCTX_candidate, candidate, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	CK( BN_to_montgomery( mont_1, BN_value_one(), montCTX_candidate, 
						  &pkcInfo->bnCTX ) );
	CKPTR( BN_copy( candidate_1, candidate ) );
	CK( BN_sub_word( candidate_1, 1 ) );
	CK( BN_to_montgomery( mont_candidate_1, candidate_1, montCTX_candidate, 
						  &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Perform n iterations of Miller-Rabin */
	LOOP_LARGE( i = 0, i < noChecks, i++ )
		{
		/* Instead of using a bignum for the Miller-Rabin check we use a
		   series of small primes.  The reason for this is that if bases a1
		   and a2 are strong liars for n then their product a1*a2 is also 
		   very likely to be a strong liar so using a composite base 
		   doesn't give us any great advantage.  In addition an initial test 
		   with a=2 is beneficial since most composite numbers will fail 
		   Miller-Rabin with a=2, and exponentiation with base 2 is faster 
		   than general-purpose exponentiation.  Finally, using small values 
		   instead of random bignums is both significantly more efficient 
		   and much easier on the RNG.   In theory in order to use the first 
		   noChecks small primes as the base instead of using random bignum 
		   bases we would have to assume that the extended Riemann 
		   hypothesis holds (without this, which allows us to use values 
		   1 < check < 2 * log( candidate )^2, we'd have to pick random 
		   check values as required for Monte Carlo algorithms), however the 
		   requirement for random bases assumes that the candidates could be 
		   chosen maliciously to be pseudoprime to any reasonable list of 
		   bases, thus requiring random bases to evade the problem.  
		   Obviously we're not going to do this so one base is as good as 
		   another, and small primes work well (even a single Fermat test 
		   has a failure probability of around 10e-44 for 512-bit primes if 
		   you're not trying to cook the primes, this is why Fermat works as 
		   a verification of the Miller-Rabin test in generatePrime()) */
		BN_set_word( check, primeTbl[ i ] );
		status = witnessOld( pkcInfo, check, candidate, candidate_1, 
							 mont_candidate_1, mont_1, montCTX_candidate );
		if( cryptStatusError( status ) )
			return( status );
		if( status )
			return( FALSE );	/* It's not a prime */
		}
	ENSURES( LOOP_BOUND_OK );

	/* It's prime */
	return( TRUE );
	}
#endif /* CHECK_PRIMETEST */

/****************************************************************************
*																			*
*							Generate a Prime Number							*
*																			*
****************************************************************************/

/* Non-primality witness-of-compositeness function:

	x(0) = a^u mod n
	if x(0) = 1 || x(0) = n - 1 
		return "probably-prime"

	for i = 1 to k
		x(i) = x(i-1)^2 mod n
		if x(i) = n - 1
			return "probably-prime"
		if x(i) = 1
			return "composite";
	return "composite"

   Since it's a yes-biased Monte Carlo algorithm this witness function can
   only answer "probably-prime" so we reduce the uncertainty by iterating
   for the Miller-Rabin test.  Destroys a.
   
   Note that this returns an error status or a boolean TRUE/FALSE so we give
   the return value as CHECK_RETVAL_RANGE( FALSE, TRUE ) rather than
   CHECK_RETVAL_BOOL */

CHECK_RETVAL_RANGE( FALSE, TRUE ) STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5, 7 ) ) \
static int witness( INOUT PKC_INFO *pkcInfo, INOUT BIGNUM *a, 
					const BIGNUM *n, const BIGNUM *n_1, const BIGNUM *u, 
					IN_LENGTH_PKC const int k, 
					INOUT BN_MONT_CTX *montCTX_n )
	{
	int i, bnStatus = BN_STATUS, LOOP_ITERATOR;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isWritePtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( n, sizeof( BIGNUM ) ) );
	assert( isReadPtr( n_1, sizeof( BIGNUM ) ) );
	assert( isReadPtr( u, sizeof( BIGNUM ) ) );
	assert( isReadPtr( montCTX_n, sizeof( BN_MONT_CTX ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( sanityCheckBignum( a ) );
	REQUIRES( sanityCheckBignum( n ) );
	REQUIRES( sanityCheckBignum( n_1 ) );
	REQUIRES( sanityCheckBignum( u ) );
	REQUIRES( sanityCheckBNMontCTX( montCTX_n ) );
	REQUIRES( k >= 1 && k <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* x(0) = a^u mod n.  If x(0) == 1 || x(0) == n - 1 it's probably
	   prime */
	CK( BN_mod_exp_mont( a, a, u, n, &pkcInfo->bnCTX, montCTX_n ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( BN_is_one( a ) || !BN_cmp_checked( a, n_1 ) )
		return( FALSE );		/* Probably prime */

	LOOP_LARGE( i = 1, i < k, i++ )
		{
		/* x(i) = x(i-1)^2 mod n */
		CK( BN_mod_mul( a, a, a, n, &pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		if( !BN_cmp_checked( a, n_1 ) )
			return( FALSE );	/* Probably prime */
		if( BN_is_one( a ) )
			{
			/* At this point we could perform an additional test:

				g = gcd( x(i-1)-1, n )

			  If g > 1 then the candidate is composite with factor g, if not 
			  then it's composite but not a power of a prime, i.e. there are 
			  no x, y >= 2 s.t. x^y == n.  This can be used when checking 
			  the RSA modulus for validity, however it requires an 
			  additional temporary bignum that we don't really have.  In 
			  theory we could overload pkcInfo->blind1, which isn't used
			  yet when primeProbable() is called, but this is asking for
			  trouble if any code path ever sets blind1 before 
			  primeProbable() is called.  In any case it's uncertain whether
			  all of this is worth the effort... */
			return( TRUE );		/* Composite */
			}
		}
	ENSURES( LOOP_BOUND_OK );

	ENSURES( sanityCheckBignum( a ) );

	return( TRUE );
	}

/* Perform noChecks iterations of the Miller-Rabin probabilistic primality 
   test (n = candidate prime, a = randomly-chosen check value):

	evaluate u s.t. n - 1 = 2^k * u, u odd

	for i = 1 to noChecks
		if witness( a, n, n-1, u, k )
			return "composite"

	return "prime"

  Destroys tmp1-3, mont1 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int primeProbable( INOUT PKC_INFO *pkcInfo, 
				   INOUT BIGNUM *n, 
				   IN_RANGE( 1, 100 ) const int noChecks,
				   OUT BOOLEAN *isPrime )
	{
	BIGNUM *a = &pkcInfo->tmp1, *n_1 = &pkcInfo->tmp2, *u = &pkcInfo->tmp3;
	int i, k, bnStatus = BN_STATUS, status, LOOP_ITERATOR;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isWritePtr( n, sizeof( BIGNUM ) ) );
	assert( isWritePtr( isPrime, sizeof( BOOLEAN ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( sanityCheckBignum( n ) );
	REQUIRES( noChecks >= 1 && noChecks <= 100 );

	/* Clear return value */
	*isPrime = FALSE;

	/* Set up various values */
	CK( BN_MONT_CTX_set( &pkcInfo->montCTX1, n, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Evaluate u as n - 1 = 2^k * u.  Obviously the less one bits in the 
	   LSBs of n the more efficient this test becomes, however with a 
	   randomly-chosen n value we get an exponentially-decreasing chance 
	   of losing any bits after the first one, which will always be zero 
	   since n starts out being odd */
	CKPTR( BN_copy( n_1, n ) );
	CK( BN_sub_word( n_1, 1 ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	LOOP_LARGE( k = 1, !BN_is_bit_set( n_1, k ), k++ )
		/* No loop body */ ;
	ENSURES( LOOP_BOUND_OK );
	CK( BN_rshift( u, n_1, k ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Perform n iterations of Miller-Rabin */
	LOOP_LARGE( i = 0, i < noChecks, i++ )
		{
		/* Instead of using a bignum for the Miller-Rabin check we use a
		   series of small primes.  The reason for this is that if bases a1
		   and a2 are strong liars for n then their product a1 * a2 is also 
		   very likely to be a strong liar so using a composite base 
		   doesn't give us any great advantage.  In addition an initial test 
		   with a = 2 is beneficial since most composite numbers will fail 
		   Miller-Rabin with a = 2, and exponentiation with base 2 is faster 
		   than general-purpose exponentiation.  Finally, using small values 
		   instead of random bignums is both significantly more efficient 
		   and much easier on the RNG.
		   
		   In theory in order to use the first noChecks small primes as the 
		   base instead of using random bignum bases we would have to assume 
		   that the extended Riemann hypothesis holds (without this, which 
		   allows us to use values 1 < check < 2 * log( candidate )^2, we'd 
		   have to pick random check values as required for Monte Carlo 
		   algorithms), however the requirement for random bases assumes that 
		   the candidates could be chosen maliciously to be pseudoprime to 
		   any reasonable list of bases, thus requiring random bases to 
		   evade the problem.  

		   Obviously we're not going to do this so one base is as good as 
		   another, and small primes work well (even a single Fermat test 
		   has a failure probability of around 10e-44 for 512-bit primes if 
		   you're not trying to cook the primes, this is why Fermat works as 
		   a verification of the Miller-Rabin test in generatePrime()).
		   
		   There is one situation in which we could run into problems with
		   maliciously-chosen candidates and that's if the values are used 
		   in SRP, see Bleichenbacher's "Breaking a Cryptographic Protocol 
		   with Pseudoprimes", PKC'05.  We don't do SRP or similar PAKEs, if
		   we ever do then the primality test will have to be strengthened */
		CK( BN_set_word( a, getSieveEntry( i ) ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		status = witness( pkcInfo, a, n, n_1, u, k, &pkcInfo->montCTX1 );
		if( cryptStatusError( status ) )
			return( status );
		if( status )
			{
			*isPrime = FALSE;
			return( CRYPT_OK );	/* It's not a prime */
			}
		}
	ENSURES( LOOP_BOUND_OK );

	ENSURES( sanityCheckBignum( n ) );

	/* It's prime */
	*isPrime = TRUE;
	return( CRYPT_OK );
	}

/* Perform a Fermat primality test to the base 2 as a quick screening 
   alternative to a full M-R test */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int primeProbableFermat( INOUT PKC_INFO *pkcInfo, 
						 const BIGNUM *n,
						 INOUT BN_MONT_CTX *montCTX_n,
						 OUT BOOLEAN *isPrime )
	{
	int bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isReadPtr( n, sizeof( BIGNUM ) ) );
	assert( isWritePtr( montCTX_n, sizeof( BN_MONT_CTX ) ) );
	assert( isWritePtr( isPrime, sizeof( BOOLEAN ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( sanityCheckBignum( n ) );
	REQUIRES( !BN_is_zero( &montCTX_n->N ) );

	/* Clear return value */
	*isPrime = FALSE;

	/* Perform a Fermat test to the base 2 (Fermat = a^p-1 mod p == 1 -> 
	   a^p mod p == a, for all a).  This isn't as reliable as Miller-Rabin 
	   but can use the fast BN_mod_exp_mont_word() to perform a (somewhat)
	   quicker screening check */
	CK( BN_mod_exp_mont_word( &pkcInfo->tmp1, 2, n, n, &pkcInfo->bnCTX, 
							  montCTX_n ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( !BN_is_word( &pkcInfo->tmp1, 2 ) )
		{
		*isPrime = FALSE;
		return( CRYPT_OK );	/* It's not a prime */
		}

	/* It's prime */
	*isPrime = TRUE;
	return( CRYPT_OK );
	}

/* Generate a prime.  This isn't of any special form, e.g. a strong prime,
   since it's only relevant for Pollard's p - 1 algorithm and with smaller
   prime sizes that we're working with, algorithms like NFS aren't affected.
   In addition if the strong primes are generated according to the one 
   standard that actually requires them, X9.31, then there's a significant
   loss of entropy, around 20 bits, due to the low density of "strong" 
   primes, i.e. a great many possible primes get discarded in favour of the 
   "strong" one, see "The Million-Key Question: Investigating the Origins of 
   RSA Public Keys" by Svenda et al.

   If the exponent is present this will also verify that 
   gcd( (p - 1)(q - 1), exponent ) = 1, which is required for RSA */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int generatePrimeEx( INOUT PKC_INFO *pkcInfo, 
							INOUT BIGNUM *candidate, 
							IN_LENGTH_SHORT_MIN( 120 ) const int noBits, 
							IN_INT_OPT const long exponent,
							IN_OPT const GET_RANDOM_INFO *getRandomInfo )
	{
	const GETRANDOMDATA_FUNCTION getRandomFunction = \
			( getRandomInfo != NULL ) ? \
				( GETRANDOMDATA_FUNCTION ) \
				FNPTR_GET( getRandomInfo->getRandomFunction ) : getRandomData;
	void *getRandomState = ( getRandomInfo != NULL ) ? \
			getRandomInfo->getRandomState : NULL;
	const int noChecks = getNoPrimeChecks( noBits );
	BOOLEAN *sieveArray, primeFound = FALSE;
	int oldOffset = 0, bnStatus = BN_STATUS, status, LOOP_ITERATOR;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isWritePtr( candidate, sizeof( BIGNUM ) ) );
	assert( getRandomInfo == NULL || \
			isReadPtr( getRandomInfo, sizeof( GET_RANDOM_INFO ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( sanityCheckBignum( candidate ) );
	REQUIRES( ( exponent == CRYPT_UNUSED && \
				noBits >= 120 && noBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) ) || \
			  ( exponent != CRYPT_UNUSED && \
				noBits >= bytesToBits( MIN_PKCSIZE ) / 2 && \
				noBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) ) );
			  /* The value of 120 doesn't correspond to any key size but is 
			     the minimal value for a prime that we'd generate using the 
				 Lim-Lee algorithm */
	REQUIRES( exponent == CRYPT_UNUSED || \
			  ( exponent >= 17 && exponent < INT_MAX - 1000 && \
			  getRandomInfo == NULL ) );
	REQUIRES( getRandomFunction != NULL );

	/* Start with a cryptographically strong odd random number ("There is a 
	   divinity in odd numbers", William Shakespeare, "Merry Wives of 
	   Windsor").  We set the two high bits so that (when generating RSA 
	   keys) pq will end up exactly 2n bits long */
	status = generateBignumEx( candidate, noBits, 0xC0, 0x1, NULL, 0, 
							   getRandomInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Allocate the array */
	if( ( sieveArray = clAlloc( "generatePrime", \
								SIEVE_SIZE * sizeof( BOOLEAN ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );

	LOOP_LARGE_CHECK( !primeFound && !cryptStatusError( status ) )
		{
		int offset, startPoint, LOOP_ITERATOR_ALT;

		/* Set up the sieve array for the number and pick a random starting
		   point */
		status = initSieve( sieveArray, SIEVE_SIZE, candidate );
		if( cryptStatusOK( status ) )
			{
			status = getRandomFunction( getRandomState, &startPoint, 
										sizeof( int ) );
			}
		if( cryptStatusError( status ) )
			break;
		startPoint &= SIEVE_SIZE - 1;
		if( startPoint <= 0 )
			startPoint = 1;		/* Avoid getting stuck on zero */

		/* Perform a random-probing search for a prime (poli, poli, di 
		   umbuendo).  "On generation of probably primes by incremental
		   search" by Jørgen Brandt and Ivan Damgård, Proceedings of
		   Crypto'92, (LNCS Vol.740), p.358, shows that for an n-bit
		   number we'll find a prime after O( n ) steps by incrementing
		   the start value by 2 each time */
		LOOP_EXT_ALT( offset = nextSievePosition( startPoint ), 
					  offset != startPoint, 
					  offset = nextSievePosition( offset ), SIEVE_SIZE + 1 )
			{
#ifdef CHECK_PRIMETEST
			LARGE_INTEGER tStart, tStop;
			BOOLEAN passedFermat, passedOldPrimeTest;
			int oldTicks, newTicks, ratio;
#endif /* CHECK_PRIMETEST */
			BN_ULONG remainder DUMMY_INIT;

			ENSURES( offset > 0 && offset < SIEVE_SIZE );
			ENSURES( offset != oldOffset );

			/* If this candidate is divisible by anything, continue */
			if( sieveArray[ offset ] != 0 )
				continue;

			/* Adjust the candidate by the number of nonprimes that we've
			   skipped */
			if( offset > oldOffset )
				{
				CK( BN_add_word( candidate, ( offset - oldOffset ) * 2 ) );
				}
			else
				{
				CK( BN_sub_word( candidate, ( oldOffset - offset ) * 2 ) );
				}
			if( bnStatusError( bnStatus ) )
				{
				status = getBnStatus( bnStatus );
				break;
				}
			oldOffset = offset;

#if defined( CHECK_PRIMETEST )
			/* Perform a Fermat test to the base 2 */
			CK( BN_MONT_CTX_set( &pkcInfo->montCTX1, candidate, 
								 &pkcInfo->bnCTX ) );
			passedFermat = primeProbableFermat( pkcInfo, candidate,
												&pkcInfo->montCTX1 );

			/* Perform the older probabilistic test */
			QueryPerformanceCounter( &tStart );
			status = primeProbableOld( pkcInfo, candidate, noChecks );
			QueryPerformanceCounter( &tStop );
			assert( tStart.HighPart == tStop.HighPart );
			oldTicks = tStop.LowPart - tStart.LowPart;
			if( cryptStatusError( status ) )
				break;
			passedOldPrimeTest = status;

			/* Perform the probabilistic test */
			QueryPerformanceCounter( &tStart );
			status = primeProbable( pkcInfo, candidate, noChecks );
			QueryPerformanceCounter( &tStop );
			assert( tStart.HighPart == tStop.HighPart );
			newTicks = tStop.LowPart - tStart.LowPart;
			ratio = ( oldTicks * 100 ) / newTicks;
			printf( "%4d bits, old MR = %6d ticks, new MR = %6d ticks, "
					"ratio = %d.%d\n", noBits, oldTicks, newTicks, 
					ratio / 100, ratio % 100 );
			if( status != passedFermat || status != passedOldPrimeTest )
				{
				printf( "Fermat reports %d, old Miller-Rabin reports %d, "
						"new Miller-Rabin reports %d.\n", 
						passedFermat, passedOldPrimeTest, status );
				getchar();
				}
#else
			status = primeProbable( pkcInfo, candidate, noChecks, 
									&primeFound );
#endif /* CHECK_PRIMETEST */
			if( cryptStatusError( status ) )
				break;
			if( !primeFound )
				continue;

			/* If it's not for RSA use then we've found our candidate */
			if( exponent == CRYPT_UNUSED )
				break;

			/* It's for use with RSA, check the RSA condition that
			   gcd( p - 1, exp ) == 1.  Since exp is a small prime we can do
			   this efficiently by checking that ( p - 1 ) mod exp != 0 */
			CK( BN_sub_word( candidate, 1 ) );
			if( !bnStatusError( bnStatus ) )
				CK( BN_mod_word( &remainder, candidate, exponent ) );
			CK( BN_add_word( candidate, 1 ) );
			if( bnStatusError( bnStatus ) )
				{
				status = getBnStatus( bnStatus );
				break;
				}
			if( remainder > 0 )
				{
				primeFound = TRUE;
				break;
				}

			/* It's a prime, but not the right sort of prime */
			primeFound = FALSE;
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( cryptStatusError( status ) || primeFound );

	/* Clean up */
	zeroise( sieveArray, SIEVE_SIZE * sizeof( BOOLEAN ) );
	clFree( "generatePrime", sieveArray );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckBignum( candidate ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int generatePrime( INOUT PKC_INFO *pkcInfo, 
				   INOUT BIGNUM *candidate, 
				   IN_LENGTH_SHORT_MIN( 120 ) const int noBits, 
				   IN_OPT const GET_RANDOM_INFO *getRandomInfo )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isWritePtr( candidate, sizeof( BIGNUM ) ) );
	assert( getRandomInfo == NULL || \
			isReadPtr( getRandomInfo, sizeof( GET_RANDOM_INFO ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( sanityCheckBignum( candidate ) );
	REQUIRES( noBits >= 120 && noBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );
			  /* The value of 120 doesn't correspond to any key size but is 
			     the minimal value for a prime that we'd generate using the 
				 Lim-Lee algorithm */

	return( generatePrimeEx( pkcInfo, candidate, noBits, CRYPT_UNUSED, 
							 getRandomInfo ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int generatePrimeRSA( INOUT PKC_INFO *pkcInfo, 
					  INOUT BIGNUM *candidate, 
					  IN_LENGTH_SHORT_MIN( bytesToBits( MIN_PKCSIZE ) / 2 ) \
							const int noBits, 
					  IN_INT const long exponent )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isWritePtr( candidate, sizeof( BIGNUM ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );
	REQUIRES( sanityCheckBignum( candidate ) );
	REQUIRES( noBits >= bytesToBits( MIN_PKCSIZE ) / 2 && \
			  noBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );
	REQUIRES( exponent >= 17 && exponent < INT_MAX - 1000 );

	return( generatePrimeEx( pkcInfo, candidate, noBits, exponent, NULL ) );
	}

/****************************************************************************
*																			*
*							Generate a Random Bignum						*
*																			*
****************************************************************************/

/* Generate a bignum of a specified length with the given high and low 8 
   bits.  'high' is merged into the high 8 bits of the number (set it to 0x80
   to ensure that the number is exactly 'bits' bits long, i.e. 2^(bits-1) <=
   bn < 2^bits), 'low' is merged into the low 8 bits (set it to 1 to ensure
   that the number is odd).  In almost all cases used in cryptlib, 'high' is
   set to 0xC0 and low is set to 0x01.

   The parameters also allow for two additional inputs, the first a fixed-
   length seed for DSA/ECDSA deterministic signatures to deal with potential 
   RNG issues, the second a user-definable randomness function for 
   verifiable generation of DLP parameters.

   We don't need to pagelock the bignum buffer that we're using because it's 
   being accessed continuously while there's data in it so there's little 
   chance that it'll be swapped unless the system is already thrashing */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int generateBignumEx( OUT BIGNUM *bignum, 
					  IN_LENGTH_SHORT_MIN( 120 ) const int noBits, 
					  IN_BYTE const int high, IN_BYTE const int low,
					  IN_BUFFER_OPT( seedLength ) const void *seed,
					  IN_LENGTH_SHORT_OPT const int seedLength,
					  IN_OPT const void *getRandomInfoPtr )
	{
	const GET_RANDOM_INFO *getRandomInfo = getRandomInfoPtr;
	const GETRANDOMDATA_FUNCTION getRandomFunction = \
			( getRandomInfo != NULL ) ? \
				( GETRANDOMDATA_FUNCTION ) \
				FNPTR_GET( getRandomInfo->getRandomFunction ) : getRandomData;
	void *getRandomState = ( getRandomInfo != NULL ) ? \
			getRandomInfo->getRandomState : NULL;
	BYTE buffer[ CRYPT_MAX_PKCSIZE + DLP_OVERFLOW_SIZE + 8 ];
	const int noBytes = bitsToBytes( noBits );
	int bnStatus, status, LOOP_ITERATOR;

	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );
	assert( seed == NULL || isReadPtrDynamic( seed, seedLength ) );
	assert( getRandomInfoPtr == NULL || \
			isReadPtr( getRandomInfoPtr, sizeof( GET_RANDOM_INFO ) ) );

	REQUIRES( sanityCheckBignum( bignum ) );
	REQUIRES( noBits >= 120 && \
			  noBits <= bytesToBits( CRYPT_MAX_PKCSIZE + DLP_OVERFLOW_SIZE ) );
			  /* The value of 120 doesn't correspond to any key size but is 
			     the minimal value for a prime that we'd generate using the 
				 Lim-Lee algorithm.  The extra DLP_OVERFLOW_SIZE bits added 
				 to CRYPT_MAX_PKCSIZE are for DLP algorithms where we reduce 
				 the bignum mod p or q and use a few extra bits to avoid the 
				 resulting value being biased, see the DLP code for 
				 details */
	REQUIRES( high > 0 && high <= 0xFF );
	REQUIRES( low >= 0 && low <= 0xFF );
			  /* The lower bound may be zero if we're generating e.g. a 
			     blinding value or some similar non-key-data value */
	REQUIRES( ( seed == NULL && seedLength == 0 ) || \
			  ( seed != NULL && isShortIntegerRangeNZ( seedLength ) ) );
	REQUIRES( getRandomFunction != NULL );
	REQUIRES( noBytes >= bitsToBytes( 120 ) && \
			  noBytes <= CRYPT_MAX_PKCSIZE + DLP_OVERFLOW_SIZE );
			  /* See comment above */

	/* Clear the return value */
	bnStatus = BN_zero( bignum );
	ENSURES( bnStatusOK( bnStatus ) );

	/* Load the random data into the bignum buffer */
	status = getRandomFunction( getRandomState, buffer, noBytes );
	if( cryptStatusError( status ) )
		{
		zeroise( buffer, noBytes );
		return( status );
		}

	/* Mix in the seed value if there's one present.  This is used for 
	   DLP/ECDLP operations where the (phenomenally low) likelihood of the 
	   RNG producing repeated values would lead to a loss of the private
	   key */
	if( seed != NULL )
		{
		const BYTE *seedPtr = seed;
		const int length = min( noBytes, seedLength );
		int i;

		LOOP_LARGE( i = 0, i < length, i++ )
			buffer[ i ] ^= seedPtr[ i ];
		ENSURES( LOOP_BOUND_OK );
		}

	/* Merge in the specified low bits, mask off any excess high bits, and
	   merge in the specified high bits.  This is a bit more complex than
	   just masking in the byte values because the bignum may not be a
	   multiple of 8 bytes long */
	buffer[ noBytes - 1 ] |= low;
	buffer[ 0 ] &= 0xFF >> ( -noBits & 7 );
	buffer[ 0 ] |= high >> ( -noBits & 7 );
	if( noBits & 7 )
		buffer[ 1 ] |= ( high << ( noBits & 7 ) ) & 0xFF;

	/* Turn the contents of the buffer into a bignum */
	if( noBytes > CRYPT_MAX_PKCSIZE )
		{
		/* We've created an oversize bignum for use in a DLP algorithm (see 
		   the comment above about DLP_OVERFLOW_SIZE), the upper bound is 
		   somewhat larger than CRYPT_MAX_PKCSIZE */
		status = importBignum( bignum, buffer, noBytes, max( noBytes - 8, 1 ),
							   CRYPT_MAX_PKCSIZE + DLP_OVERFLOW_SIZE, NULL, 
							   KEYSIZE_CHECK_SPECIAL );
		}
	else
		{
		status = importBignum( bignum, buffer, noBytes, max( noBytes - 8, 1 ),
							   CRYPT_MAX_PKCSIZE, NULL, KEYSIZE_CHECK_NONE );
		}
	zeroise( buffer, noBytes );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckBignum( bignum ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int generateBignum( OUT BIGNUM *bignum, 
					IN_LENGTH_SHORT_MIN( 120 ) const int noBits, 
					IN_BYTE const int high, IN_BYTE const int low )
	{
	return( generateBignumEx( bignum, noBits, high, low, NULL, 0, NULL ) );
	}

#if 0

/* Composite value that passes the primeProbable() test as described in 
   "Prime and Prejudice: Primality Testing Under Adversarial Conditions",
   Martin Albrecht, Jake Massimo, Kenny Paterson and Juraj Somorovsky.

   This is an expected result, see the long comment in primeProbable().
   
   The 2315-bit value is generated as follows:

	n = p1 x p2 x p3

	p1 = 2^576 x 0x24a027808260908b96d740bef8355ded63f6edb7f70de9a9
		+ 2^384 x 0xb99c408f131cef3855b4b0aea6b17a4469ed5a7ec8b2be62
		+ 2^192 x 0x66c3a9eae83a6769e175cb2598256da977b9e191b9b847a7
		+ 2^0 x 0xe2cf4750d9bc2d64ccd3406f5db662c22c3fc65e3c56eff3

	pi = ki ( p1 - 1 ) + 1

	( k2,k3 ) = ( 641, 677 ) 

   So p1 =
	( 2^576 * 0x24a027808260908b96d740bef8355ded63f6edb7f70de9a9 ) + 
	( 2^384 * 0xb99c408f131cef3855b4b0aea6b17a4469ed5a7ec8b2be62 ) + 
	( 2^192 * 0x66c3a9eae83a6769e175cb2598256da977b9e191b9b847a7 ) + 
	( 2^0 * 0xe2cf4750d9bc2d64ccd3406f5db662c22c3fc65e3c56eff3 )

	0x24a027808260908b96d740bef8355ded63f6edb7f70de9a9b99c408f131cef385\
	5b4b0aea6b17a4469ed5a7ec8b2be6266c3a9eae83a6769e175cb2598256da977b9\
	e191b9b847a7e2cf4750d9bc2d64ccd3406f5db662c22c3fc65e3c56eff3

   p2 = 
	( 641 * ( p1 - 1 ) ) + 1
	( 641 * ( 0x24a027808260908b96d740bef8355ded63f6edb7f70de9a9b99c408\
	f131cef3855b4b0aea6b17a4469ed5a7ec8b2be6266c3a9eae83a6769e175cb2598\
	256da977b9e191b9b847a7e2cf4750d9bc2d64ccd3406f5db662c22c3fc65e3c56e\
	ff3 - 1 ) ) + 1

	0x5bb502e8c673c9ed84b0f91e2b7da02f674d4939a199d611f9c03da63edb72fc0\
	e996e654f6263254d3b4f9774878eb4634fec752f7a3cf01d87f1a921f5b79554c8\
	6dcde2066b6b5ee9019171302da964dcf456d9a5ad4830cbafb1f515aeccf3

   p3 = 
	( 677 * ( p1 - 1 ) ) + 1
	( 677 * ( 0x24a027808260908b96d740bef8355ded63f6edb7f70de9a9b99c408\
	f131cef3855b4b0aea6b17a4469ed5a7ec8b2be6266c3a9eae83a6769e175cb2598\
	256da977b9e191b9b847a7e2cf4750d9bc2d64ccd3406f5db662c22c3fc65e3c56e\
	ff3 - 1 ) ) + 1

	0x60db8876d8c95e4125e73e3906652164c95c02a78057caedd7da36ba5d8b849ff\
	aa6d73dded35856ec20b05148c0b17a39c3705a3822737b013c823a6b5afb01299e\
	91866024557eface2798cfcea40b91aaa96682d3532b7f04a7973591e88afb

   n = p1 x p2 x p3
	0x24a027808260908b96d740bef8355ded63f6edb7f70de9a9b99c408f131cef385\
	5b4b0aea6b17a4469ed5a7ec8b2be6266c3a9eae83a6769e175cb2598256da977b9\
	e191b9b847a7e2cf4750d9bc2d64ccd3406f5db662c22c3fc65e3c56eff3 * 
	0x5bb502e8c673c9ed84b0f91e2b7da02f674d4939a199d611f9c03da63edb72fc0\
	e996e654f6263254d3b4f9774878eb4634fec752f7a3cf01d87f1a921f5b79554c8\
	6dcde2066b6b5ee9019171302da964dcf456d9a5ad4830cbafb1f515aeccf3 * 
	0x60db8876d8c95e4125e73e3906652164c95c02a78057caedd7da36ba5d8b849ff\
	aa6d73dded35856ec20b05148c0b17a39c3705a3822737b013c823a6b5afb01299e\
	91866024557eface2798cfcea40b91aaa96682d3532b7f04a7973591e88afb

	4f6cfc0001b66240de95fba380575ed10b55a7e00f7bfb5f4d9a54156e73858737a\
	3c9a290e99a7e9b96358b78c4bb5e944ed98f69ad414dbbc21725c0825facbe07bf\
	2019512a8cf00183d9c185ec2f97ec1cec8b3d0de6d08f9069d809664ee8fde15c3\
	cb10ffafe6891e1fed987b577dc7c13355c8b9828f578d7164af90b388665ee6a74\
	597b2318a871d5887d832876d390a99d683fea506d8f84fbd8391ed2ae990e66078\
	d0fe3d67d1f369b67fdc402eee9a75985d34d1b8f5fc9f818f16d6cf4c863d710d1\
	b86fed5b5b50b367b04783634a997e17afc59db0af2d64fcec265b02c3cf2d7896d\
	ee97dbaaa55f41e28d9e7daeb8b820565c7d51690bbf84f6abc727ddc2e8f5115fe\
	4d69995008c7b805c4509f04b2c98c2819355f5bcb3 */

static const BYTE compositePrime[] = {
	0x04, 0xF6, 0xCF, 0xC0, 0x00, 0x1B, 0x66, 0x24,
	0x0D, 0xE9, 0x5F, 0xBA, 0x38, 0x05, 0x75, 0xED,
	0x10, 0xB5, 0x5A, 0x7E, 0x00, 0xF7, 0xBF, 0xB5,
	0xF4, 0xD9, 0xA5, 0x41, 0x56, 0xE7, 0x38, 0x58,
	0x73, 0x7A, 0x3C, 0x9A, 0x29, 0x0E, 0x99, 0xA7,
	0xE9, 0xB9, 0x63, 0x58, 0xB7, 0x8C, 0x4B, 0xB5,
	0xE9, 0x44, 0xED, 0x98, 0xF6, 0x9A, 0xD4, 0x14,
	0xDB, 0xBC, 0x21, 0x72, 0x5C, 0x08, 0x25, 0xFA,
	0xCB, 0xE0, 0x7B, 0xF2, 0x01, 0x95, 0x12, 0xA8,
	0xCF, 0x00, 0x18, 0x3D, 0x9C, 0x18, 0x5E, 0xC2,
	0xF9, 0x7E, 0xC1, 0xCE, 0xC8, 0xB3, 0xD0, 0xDE,
	0x6D, 0x08, 0xF9, 0x06, 0x9D, 0x80, 0x96, 0x64,
	0xEE, 0x8F, 0xDE, 0x15, 0xC3, 0xCB, 0x10, 0xFF,
	0xAF, 0xE6, 0x89, 0x1E, 0x1F, 0xED, 0x98, 0x7B,
	0x57, 0x7D, 0xC7, 0xC1, 0x33, 0x55, 0xC8, 0xB9,
	0x82, 0x8F, 0x57, 0x8D, 0x71, 0x64, 0xAF, 0x90,
	0xB3, 0x88, 0x66, 0x5E, 0xE6, 0xA7, 0x45, 0x97,
	0xB2, 0x31, 0x8A, 0x87, 0x1D, 0x58, 0x87, 0xD8,
	0x32, 0x87, 0x6D, 0x39, 0x0A, 0x99, 0xD6, 0x83,
	0xFE, 0xA5, 0x06, 0xD8, 0xF8, 0x4F, 0xBD, 0x83,
	0x91, 0xED, 0x2A, 0xE9, 0x90, 0xE6, 0x60, 0x78,
	0xD0, 0xFE, 0x3D, 0x67, 0xD1, 0xF3, 0x69, 0xB6,
	0x7F, 0xDC, 0x40, 0x2E, 0xEE, 0x9A, 0x75, 0x98,
	0x5D, 0x34, 0xD1, 0xB8, 0xF5, 0xFC, 0x9F, 0x81,
	0x8F, 0x16, 0xD6, 0xCF, 0x4C, 0x86, 0x3D, 0x71,
	0x0D, 0x1B, 0x86, 0xFE, 0xD5, 0xB5, 0xB5, 0x0B,
	0x36, 0x7B, 0x04, 0x78, 0x36, 0x34, 0xA9, 0x97,
	0xE1, 0x7A, 0xFC, 0x59, 0xDB, 0x0A, 0xF2, 0xD6,
	0x4F, 0xCE, 0xC2, 0x65, 0xB0, 0x2C, 0x3C, 0xF2,
	0xD7, 0x89, 0x6D, 0xEE, 0x97, 0xDB, 0xAA, 0xA5,
	0x5F, 0x41, 0xE2, 0x8D, 0x9E, 0x7D, 0xAE, 0xB8,
	0xB8, 0x20, 0x56, 0x5C, 0x7D, 0x51, 0x69, 0x0B,
	0xBF, 0x84, 0xF6, 0xAB, 0xC7, 0x27, 0xDD, 0xC2,
	0xE8, 0xF5, 0x11, 0x5F, 0xE4, 0xD6, 0x99, 0x95,
	0x00, 0x8C, 0x7B, 0x80, 0x5C, 0x45, 0x09, 0xF0,
	0x4B, 0x2C, 0x98, 0xC2, 0x81, 0x93, 0x55, 0xF5,
	0xBC, 0xB3 
	};

void checkCompositePrime( void )
	{
	CONTEXT_INFO contextInfo;
	PKC_INFO contextData, *pkcInfo = &contextData;
	BOOLEAN primeFound;
	const int noChecks = getNoPrimeChecks( bytesToBits( 290 ) );
	int status;

	memset( &contextInfo, 0, sizeof( CONTEXT_INFO ) );
	memset( pkcInfo, 0, sizeof( PKC_INFO ) );

	status = staticInitContext( &contextInfo, CONTEXT_PKC, 
								getRSACapability(), pkcInfo, 
								sizeof( PKC_INFO ), NULL );
	status = importBignum( &pkcInfo->rsaParam_n, compositePrime, 
						   290, 289, 290, NULL, KEYSIZE_CHECK_PKC );
	status = primeSieve( &pkcInfo->rsaParam_n );
	status = primeProbable( pkcInfo, &pkcInfo->rsaParam_n, noChecks, 
							&primeFound );
	}
#endif /* 0 */
#endif /* USE_PKC */
