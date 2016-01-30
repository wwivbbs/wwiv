/****************************************************************************
*																			*
*				cryptlib RSA Key Generation/Checking Routines				*
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

/* We use F4 as the default public exponent e unless the user chooses to
   override this with some other value:

	Fn = 2^(2^n) + 1, n = 0...4.

	F0 = 3, F1 = 5, F2 = 17, F3 = 257, F4 = 65537.
   
   The older (X.509v1) recommended value of 3 is insecure for general use 
   and more recent work indicates that values like 17 (used by PGP) are also 
   insecure against the Hastad attack.  We could work around this by using 
   41 or 257 as the exponent, however current best practice favours F4 
   unless you're doing banking standards, in which case you set e=2 (EMV) 
   and use raw, unpadded RSA (HBCI) to make it easier for students to break 
   your banking security as a homework exercise.

   Since some systems may be using 16-bit bignum component values we use an
   exponent of 257 for these cases to ensure that it fits in a single
   component value */

#ifndef RSA_PUBLIC_EXPONENT
  #ifdef SIXTEEN_BIT
	#define RSA_PUBLIC_EXPONENT		257
  #else
	#define RSA_PUBLIC_EXPONENT		65537L
  #endif /* 16-bit bignum components */
#endif /* RSA_PUBLIC_EXPONENT */

/* The minimum allowed public exponent.  In theory this could go as low as 3,
   however there are all manner of obscure corner cases that have to be 
   checked if this exponent is used and in general the necessary checking 
   presents a more or less intractable problem.  To avoid this minefield we
   require a minimum exponent of at 17, the next generally-used value above 
   3.  However even this is only used by PGP 2.x, the next minimum is 33 (a
   weird value used by OpenSSH until mid-2010, see the comment further 
   down), 41 (another weird value used by GPG until mid-2006), and then 257 
   or (in practice) F4 / 65537 by everything else */

#if defined( USE_PGP ) || defined( USE_PGPKEYS )
  #define MIN_PUBLIC_EXPONENT		17
#elif defined( USE_SSH )
  #define MIN_PUBLIC_EXPONENT		33
#else
  #define MIN_PUBLIC_EXPONENT		257
#endif /* Smallest exponents used by various crypto protocols */
#if ( MIN_PUBLIC_EXPONENT <= 0xFF && RSAPARAM_MIN_E > 1 ) || \
	( MIN_PUBLIC_EXPONENT <= 0xFFFF && RSAPARAM_MIN_E > 2 )
  #error RSAPARAM_MIN_E is too large for MIN_PUBLIC_EXPONENT
#endif /* MIN_PUBLIC_EXPONENT size > RSAPARAM_MIN_E value */

/****************************************************************************
*																			*
*							Utility Functions								*
*																			*
****************************************************************************/

/* Enable various side-channel protection mechanisms */

#if defined( __WINCE__ ) && defined( ARMV4 ) && defined( NDEBUG )
  #pragma optimize( "g", off )
#endif /* eVC++ 4.0 ARMv4 optimiser bug */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int enableSidechannelProtection( INOUT PKC_INFO *pkcInfo, 
										const BOOLEAN isPrivateKey )
	{
	BIGNUM *n = &pkcInfo->rsaParam_n, *e = &pkcInfo->rsaParam_e;
	BIGNUM *k = &pkcInfo->rsaParam_blind_k;
	BIGNUM *kInv = &pkcInfo->rsaParam_blind_kInv;
	MESSAGE_DATA msgData;
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	int noBytes = bitsToBytes( pkcInfo->keySizeBits );
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Generate a random bignum for blinding.  Since this merely has to be 
	   unpredictable to an outsider but not cryptographically strong, and to 
	   avoid having more crypto RNG output than necessary sitting around in 
	   memory, we get it from the nonce PRNG rather than the crypto one.  In
	   addition we don't have to perform a range check on import to see if 
	   it's larger than 'n' since we're about to reduce it mod n in the next 
	   step, and doing so would give false positives */
	setMessageData( &msgData, buffer, noBytes );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	if( cryptStatusOK( status ) )
		{
		buffer[ 0 ] &= 0xFF >> ( -pkcInfo->keySizeBits & 7 );
		status = importBignum( k, buffer, noBytes, MIN_PKCSIZE - 8, 
							   CRYPT_MAX_PKCSIZE, NULL, 
							   KEYSIZE_CHECK_NONE );
		}
	zeroise( buffer, noBytes );
	if( cryptStatusError( status ) )
		return( status );

	/* Set up the blinding and unblinding values */
	CK( BN_mod( k, k, n, pkcInfo->bnCTX ) );	/* k = rand() mod n */
	CKPTR( BN_mod_inverse( kInv, k, n, pkcInfo->bnCTX ) );
												/* kInv = k^-1 mod n */
	CK( BN_mod_exp_mont( k, k, e, n, pkcInfo->bnCTX,
						 &pkcInfo->rsaParam_mont_n ) );
												/* k = k^e mod n */
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Use constant-time modexp() to protect the private key from timing 
	   channels if required */
	if( isPrivateKey )
		{
		BN_set_flags( &pkcInfo->rsaParam_exponent1, BN_FLG_EXP_CONSTTIME );
		BN_set_flags( &pkcInfo->rsaParam_exponent2, BN_FLG_EXP_CONSTTIME );
		}

	/* Checksum the bignums to try and detect fault attacks.  Since we're
	   setting the checksum at this point there's no need to check the 
	   return value */
	( void ) calculateBignumChecksum( pkcInfo, CRYPT_ALGO_RSA );

	return( CRYPT_OK );
	}

#if defined( __WINCE__ ) && defined( ARMV4 ) && defined( NDEBUG )
  #pragma optimize( "g", on )
#endif /* eVC++ 4.0 ARMv4 optimiser bug */

/* Adjust p and q if necessary to ensure that the CRT decrypt works */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int fixCRTvalues( INOUT PKC_INFO *pkcInfo, 
						 const BOOLEAN fixPKCSvalues )
	{
	BIGNUM *p = &pkcInfo->rsaParam_p, *q = &pkcInfo->rsaParam_q;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Make sure that p > q, which is required for the CRT decrypt */
	if( BN_cmp( p, q ) >= 0 )
		return( CRYPT_OK );

	/* Swap the values p and q and, if necessary, the PKCS parameters e1
	   and e2 that depend on them (e1 = d mod (p - 1) and
	   e2 = d mod (q - 1)), and recompute u = qInv mod p */
	BN_swap( p, q );
	if( !fixPKCSvalues )
		return( CRYPT_OK );
	BN_swap( &pkcInfo->rsaParam_exponent1, &pkcInfo->rsaParam_exponent2 );
	return( BN_mod_inverse( &pkcInfo->rsaParam_u, q, p,
							pkcInfo->bnCTX ) != NULL ? \
			CRYPT_OK : CRYPT_ERROR_FAILED );
	}

/* Evaluate the Montgomery forms for public and private components */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getRSAMontgomery( INOUT PKC_INFO *pkcInfo, 
							 const BOOLEAN isPrivateKey )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Evaluate the public value */
	if( !BN_MONT_CTX_set( &pkcInfo->rsaParam_mont_n, &pkcInfo->rsaParam_n,
						  pkcInfo->bnCTX ) )
		return( CRYPT_ERROR_FAILED );
	if( !isPrivateKey )
		return( CRYPT_OK );

	/* Evaluate the private values */
	return( BN_MONT_CTX_set( &pkcInfo->rsaParam_mont_p, &pkcInfo->rsaParam_p,
							 pkcInfo->bnCTX ) && \
			BN_MONT_CTX_set( &pkcInfo->rsaParam_mont_q, &pkcInfo->rsaParam_q,
							 pkcInfo->bnCTX ) ? \
			CRYPT_OK : CRYPT_ERROR_FAILED );
	}

/****************************************************************************
*																			*
*							Check an RSA Key								*
*																			*
****************************************************************************/

/* Perform validity checks on the public key.  We have to make the PKC_INFO
   data non-const because the bignum code wants to modify some of the values
   as it's working with them */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkRSAPublicKeyComponents( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *n = &pkcInfo->rsaParam_n, *e = &pkcInfo->rsaParam_e;
	const BN_ULONG eWord = BN_get_word( e );
	int length;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Verify that nLen >= RSAPARAM_MIN_N (= MIN_PKCSIZE), 
	   nLen <= RSAPARAM_MAX_N (= CRYPT_MAX_PKCSIZE) */
	length = BN_num_bytes( n );
	if( isShortPKCKey( length ) )
		{
		/* Special-case handling for insecure-sized public keys */
		return( CRYPT_ERROR_NOSECURE );
		}
	if( length < RSAPARAM_MIN_N || length > RSAPARAM_MAX_N )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that n is not (obviously) composite */
	if( !primeSieve( n ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that e >= MIN_PUBLIC_EXPONENT, eLen <= RSAPARAM_MAX_E 
	   (= 32 bits).  The latter check is to preclude DoS attacks due to 
	   ridiculously large e values.  BN_get_word() works even on 16-bit 
	   systems because it returns BN_MASK2 (== UINT_MAX) if the value 
	   can't be represented in a machine word */
	if( eWord < MIN_PUBLIC_EXPONENT || \
		BN_num_bytes( e ) > RSAPARAM_MAX_E )
		return( CRYPT_ARGERROR_STR1 );

	/* Perform a second check to make sure that e will fit into a signed 
	   integer.  This isn't strictly required since a BN_ULONG is unsigned 
	   but it's unlikely that anyone would consciously use a full 32-bit e
	   value (well, except for the German RegTP, who do all sorts of other
	   bizarre things as well) so we weed out any attempts to use one here */
	if( BN_num_bits( e ) >= bytesToBits( sizeof( int ) ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that e is a small prime.  The easiest way to do this would be
	   to compare it to a set of standard values but there'll always be some
	   wierdo implementation that uses a nonstandard value and that would
	   therefore fail the test so we perform a quick check that just tries
	   dividing by all primes below 1000.  In addition since in almost all
	   cases e will be one of a standard set of values we don't bother with 
	   the trial division unless it's an unusual value.  This test isn't
	   perfect but it'll catch obvious non-primes */
	if( eWord != 17 && eWord != 257 && eWord != 65537L && !primeSieve( e ) )
		{
		/* OpenSSH versions up to 5.4 (released in 2010) hardcoded e = 35, 
		   which is both a suboptimal exponent (it's less efficient that a 
		   safer value like 257 or F4) and non-prime.  The reason for this 
		   was that the original SSH used an e relatively prime to 
		   (p-1)(q-1), choosing odd (in both senses of the word) 
		   numbers > 31.  33 or 35 probably ended up being chosen frequently 
		   so it was hardcoded into OpenSSH for cargo-cult reasons, finally 
		   being fixed after more than a decade to use F4.  In order to use 
		   pre-5.4 OpenSSH keys that use this odd value we make a special-
		   case exception for SSH use */
#ifdef USE_SSH
		if( eWord == 33 || eWord == 35 )
			return( CRYPT_OK );
#endif /* USE_SSH */

		return( CRYPT_ARGERROR_STR1 );
		}
	
	return( CRYPT_OK );
	}

/* Perform validity checks on the private key.  We have to make the PKC_INFO
   data non-const because the bignum code wants to modify some of the values
   as it's working with them */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkRSAPrivateKeyComponents( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *n = &pkcInfo->rsaParam_n, *e = &pkcInfo->rsaParam_e;
	BIGNUM *d = &pkcInfo->rsaParam_d, *p = &pkcInfo->rsaParam_p;
	BIGNUM *q = &pkcInfo->rsaParam_q;
	BIGNUM *p1 = &pkcInfo->tmp1, *q1 = &pkcInfo->tmp2, *tmp = &pkcInfo->tmp3;
	const BN_ULONG eWord = BN_get_word( e );
	int bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Verify that p, q aren't (obviously) composite */
	if( !primeSieve( p ) || !primeSieve( q ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that |p-q| > 128 bits.  FIPS 186-3 requires only 100 bits, 
	   this check is slightly more conservative but in any case both values 
	   are somewhat arbitrary and are merely meant to delimit "not too 
	   close".  There's a second more obscure check that we could in theory 
	   perform to make sure that p and q don't have the least significant 
	   nLen / 4 bits the same (which would still lead to |p-q| > 128), this 
	   would make the Boneh/Durfee attack marginally less improbable (result 
	   by Zhao and Qi).  Since the chance of them having 256 LSB bits the 
	   same is vanishingly small and the Boneh/Dufree attack requires 
	   special properties for d (see the comment in generateRSAkey()) we 
	   don't bother with this check */
	if( BN_cmp( p, q ) >= 0 )
		{
		CKPTR( BN_copy( tmp, p ) );
		CK( BN_sub( tmp, tmp, q ) );
		}
	else
		{
		CKPTR( BN_copy( tmp, q ) );
		CK( BN_sub( tmp, tmp, p ) );
		}
	if( bnStatusError( bnStatus ) || \
		BN_num_bits( tmp ) < 128 )
		return( CRYPT_ARGERROR_STR1 );

	/* Calculate p - 1, q - 1 */
	CKPTR( BN_copy( p1, p ) );
	CK( BN_sub_word( p1, 1 ) );
	CKPTR( BN_copy( q1, q ) );
	CK( BN_sub_word( q1, 1 ) );
	if( bnStatusError( bnStatus ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that n = p * q */
	CK( BN_mul( tmp, p, q, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) || BN_cmp( n, tmp ) != 0 )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that:

		p, q < d
		( d * e ) mod p-1 == 1 
		( d * e ) mod q-1 == 1
	
	   Some implementations don't store d since it's not needed when the CRT
	   shortcut is used so we can only perform this check if d is present */
	if( !BN_is_zero( d ) )
		{
		if( BN_cmp( p, d ) >= 0 || BN_cmp( q, d ) >= 0 )
			return( CRYPT_ARGERROR_STR1 );
		CK( BN_mod_mul( tmp, d, e, p1, pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) || !BN_is_one( tmp ) )
			return( CRYPT_ARGERROR_STR1 );
		CK( BN_mod_mul( tmp, d, e, q1, pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) || !BN_is_one( tmp ) )
			return( CRYPT_ARGERROR_STR1 );
		}

#ifdef USE_FIPS140
	/* Verify that sizeof( d ) > sizeof( p ) / 2, a weird requirement set by 
	   FIPS 186-3.  This is one of those things where the probability of the
	   check going wrong in some way outweighs the probability of the 
	   situation actually occurring by about two dozen orders of magnitude 
	   so we only do this when we have to.  The fact that this parameter is
	   never even used makes the check even less meaningful */
	if( BN_num_bits( d ) <= pkcInfo->keySizeBits )
		return( CRYPT_ARGERROR_STR1 );
#endif /* USE_FIPS140 */

	/* Verify that ( q * u ) mod p == 1.  In some cases the p and q values 
	   haven't been set up yet for the CRT decrypt to work (see the comment
	   in fixCRTvalues()) so we have to be prepared to accept them in either
	   order */
	if( BN_cmp( p, q ) >= 0 )
		CK( BN_mod_mul( tmp, q, &pkcInfo->rsaParam_u, p, pkcInfo->bnCTX ) );
	else
		CK( BN_mod_mul( tmp, p, &pkcInfo->rsaParam_u, q, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) || !BN_is_one( tmp ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that e1 < p, e2 < q */
	if( BN_cmp( &pkcInfo->rsaParam_exponent1, p ) >= 0 || \
		BN_cmp( &pkcInfo->rsaParam_exponent2, q ) >= 0 )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that u < p, where u was calculated as q^-1 mod p */
	if( BN_cmp( &pkcInfo->rsaParam_u, p ) >= 0 )
		return( CRYPT_ARGERROR_STR1 );

	/* A very small number of systems/compilers can't handle 32 * 32 -> 64
	   ops which means that we have to use 16-bit bignum components.  For 
	   the common case where e = F4 the value won't fit into a 16-bit bignum
	   component so we have to use the full BN_mod() form of the checks that 
	   are carried out further on */
#ifdef SIXTEEN_BIT
	CK( BN_mod( tmp, p1, e, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) || BN_is_zero( tmp ) )
		return( CRYPT_ARGERROR_STR1 );
	CK( BN_mod( tmp, q1, e, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) || BN_is_zero( tmp ) )
		return( CRYPT_ARGERROR_STR1 );
	return( CRYPT_OK );
#endif /* Systems without 32 * 32 -> 64 ops */

	/* Verify that gcd( ( p - 1 )( q - 1), e ) == 1
	
	   Since e is a small prime we can do this much more efficiently by 
	   checking that:

		( p - 1 ) mod e != 0
		( q - 1 ) mod e != 0 */
	if( BN_mod_word( p1, eWord ) == 0 || BN_mod_word( q1, eWord ) == 0 )
		return( CRYPT_ARGERROR_STR1 );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Initialise/Check an RSA Key						*
*																			*
****************************************************************************/

/* Generate an RSA key pair into an encryption context.  For FIPS 140 
   purposes the keygen method used here complies with FIPS 186-3 Appendix 
   B.3, "IFC Key Pair Generation", specifically method B.3.3, "Generation of 
   Random Primes that are Probably Prime".  Note that FIPS 186-3 provides a
   range of key-generation methods and allows implementations to select one
   that's appropriate, this implementation provides the one in B.3.3, with
   the exception that it allows keys in the range MIN_PKC_SIZE ... 
   CRYPT_MAX_PKCSIZE to be generated.  FIPS 186-3 is rather confusing in
   that it discusses conditions and requirements for generating pairs from
   512 ... 3072 bits and then gives different lengths and restrictions on
   lengths depending on which portion of text you consult.  Because of this
   confusion, and the fact that telling users that they can't generate the
   key that they want because of some obscure document that they've never
   even heard of will cause friction, we leave it as a policy decision to
   define the appropriate key size to use */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int generateRSAkey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_LENGTH_SHORT_MIN( MIN_PKCSIZE * 8 ) const int keyBits )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	BIGNUM *d = &pkcInfo->rsaParam_d, *p = &pkcInfo->rsaParam_p;
	BIGNUM *q = &pkcInfo->rsaParam_q;
	BIGNUM *tmp = &pkcInfo->tmp1;
	int pBits, qBits, bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	
	REQUIRES( keyBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keyBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* Determine how many bits to give to each of p and q */
	pBits = ( keyBits + 1 ) / 2;
	qBits = keyBits - pBits;
	pkcInfo->keySizeBits = pBits + qBits;

	/* Generate the primes p and q and set them up so that the CRT decrypt
	   will work.  FIPS 186-3 requires that they be in the range 
	   sqr(2) * 2^(keyBits-1) ... 2^keyBits (so that pq will be exactly 
	   keyBits long), but this is guaranteed by the way that generatePrime()
	   selects its prime values so we don't have to check explicitly for it
	   here */
	BN_set_word( &pkcInfo->rsaParam_e, RSA_PUBLIC_EXPONENT );
	status = generatePrime( pkcInfo, p, pBits, RSA_PUBLIC_EXPONENT );
	if( cryptStatusOK( status ) )
		status = generatePrime( pkcInfo, q, qBits, RSA_PUBLIC_EXPONENT );
	if( cryptStatusOK( status ) )
		status = fixCRTvalues( pkcInfo, FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Compute d = eInv mod (p - 1)(q - 1) */
	CK( BN_sub_word( p, 1 ) );
	CK( BN_sub_word( q, 1 ) );
	CK( BN_mul( tmp, p, q, pkcInfo->bnCTX ) );
	CKPTR( BN_mod_inverse( d, &pkcInfo->rsaParam_e, tmp, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

#ifdef USE_FIPS140
	/* Check that sizeof( d ) > sizeof( p ) / 2, a weird requirement set by 
	   FIPS 186-3.  This is one of those things where the probability of the
	   check going wrong in some way outweighs the probability of the 
	   situation actually occurring by about two dozen orders of magnitude 
	   so we only do this when we have to.  The fact that this parameter is
	   never even used makes the check even less meaningful.
	   
	   (This check possibly has something to do with defending against 
	   Wiener's continued-fraction attack, which requires d < n^(1/4) in
	   order to succeed, later extended into the range d < n^(0.29) by
	   Boneh and Durfee/Bloemer and May and d < 1/2 n^(1/2) by Maitra and 
	   Sarkar) */
	if( BN_num_bits( d ) <= pkcInfo->keySizeBits / 2 )
		return( CRYPT_ERROR_FAILED );
#endif /* USE_FIPS140 */

	/* Compute e1 = d mod (p - 1), e2 = d mod (q - 1) */
	CK( BN_mod( &pkcInfo->rsaParam_exponent1, d,
				p, pkcInfo->bnCTX ) );
	CK( BN_mod( &pkcInfo->rsaParam_exponent2, d, q, pkcInfo->bnCTX ) );
	CK( BN_add_word( p, 1 ) );
	CK( BN_add_word( q, 1 ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Compute n = pq, u = qInv mod p */
	CK( BN_mul( &pkcInfo->rsaParam_n, p, q, pkcInfo->bnCTX ) );
	CKPTR( BN_mod_inverse( &pkcInfo->rsaParam_u, q, p, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Since the keygen is randomised it may occur that the final size of 
	   the public value that determines its nominal size is slightly smaller 
	   than the requested nominal size.  To handle this we recalculate the 
	   effective key size after we've finished generating the public value
	   that determines its nominal size */
	pkcInfo->keySizeBits = BN_num_bits( &pkcInfo->rsaParam_n );

	/* Evaluate the Montgomery forms */
	status = getRSAMontgomery( pkcInfo, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the generated values are valid */
	status = checkRSAPublicKeyComponents( pkcInfo );
	if( cryptStatusOK( status ) )
		status = checkRSAPrivateKeyComponents( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Enable side-channel protection if required */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		return( CRYPT_OK );
	return( enableSidechannelProtection( pkcInfo, TRUE ) );
	}

/* Initialise and check an RSA key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initCheckRSAkey( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	BIGNUM *n = &pkcInfo->rsaParam_n, *e = &pkcInfo->rsaParam_e;
	BIGNUM *d = &pkcInfo->rsaParam_d, *p = &pkcInfo->rsaParam_p;
	BIGNUM *q = &pkcInfo->rsaParam_q;
	const BOOLEAN isPrivateKey = \
		( contextInfoPtr->flags & CONTEXT_FLAG_ISPUBLICKEY ) ? FALSE : TRUE;
	int bnStatus = BN_STATUS, status = CRYPT_OK;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* Make sure that the necessary key parameters have been initialised */
	if( BN_is_zero( n ) || BN_is_zero( e ) )
		return( CRYPT_ARGERROR_STR1 );
	if( isPrivateKey )
		{
		if( BN_is_zero( p ) || BN_is_zero( q ) )
			return( CRYPT_ARGERROR_STR1 );
		if( BN_is_zero( d ) && \
			( BN_is_zero( &pkcInfo->rsaParam_exponent1 ) || \
			  BN_is_zero( &pkcInfo->rsaParam_exponent2 ) ) )
			{
			/* Either d or e1+e2 must be present, d isn't needed if we have 
			   e1+e2 and e1+e2 can be reconstructed from d */
			return( CRYPT_ARGERROR_STR1 );
			}
		}

	/* Make sure that the public key parameters are valid */
	status = checkRSAPublicKeyComponents( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a public key, we're done */
	if( !isPrivateKey )
		{
		/* Precompute the Montgomery forms of required values */
		status = getRSAMontgomery( pkcInfo, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		pkcInfo->keySizeBits = BN_num_bits( &pkcInfo->rsaParam_n );

		/* Enable side-channel protection if required */
		if( !( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
			return( CRYPT_OK );
		return( enableSidechannelProtection( pkcInfo, FALSE ) );
		}

	/* If we're not using PKCS keys that have exponent1 = d mod ( p - 1 )
	   and exponent2 = d mod ( q - 1 ) precalculated, evaluate them now.
	   If there's no u precalculated, evaluate it now */
	if( BN_is_zero( &pkcInfo->rsaParam_exponent1 ) )
		{
		BIGNUM *exponent1 = &pkcInfo->rsaParam_exponent1;
		BIGNUM *exponent2 = &pkcInfo->rsaParam_exponent2;

		/* exponent1 = d mod ( p - 1 ) ) */
		CKPTR( BN_copy( exponent1, p ) );
		CK( BN_sub_word( exponent1, 1 ) );
		CK( BN_mod( exponent1, d, exponent1, pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );

		/* exponent2 = d mod ( q - 1 ) ) */
		CKPTR( BN_copy( exponent2, q ) );
		CK( BN_sub_word( exponent2, 1 ) );
		CK( BN_mod( exponent2, d, exponent2, pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		}
	if( BN_is_zero( &pkcInfo->rsaParam_u ) )
		{
		CKPTR( BN_mod_inverse( &pkcInfo->rsaParam_u, q, p,
							   pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		}

	/* We've got the remaining components set up, perform further validity 
	   checks on the private key */
	status = checkRSAPrivateKeyComponents( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that p and q are set up correctly for the CRT decryption 
	   (we can do this after checkRSAPrivateKeyComponents() since it'll work
	   with the CRT values in any order) */
	status = fixCRTvalues( pkcInfo, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Precompute the Montgomery forms of required values */
	status = getRSAMontgomery( pkcInfo, TRUE );
	if( cryptStatusError( status ) )
		return( status );
	pkcInfo->keySizeBits = BN_num_bits( &pkcInfo->rsaParam_n );

	/* Enable side-channel protection if required */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		return( CRYPT_OK );
	return( enableSidechannelProtection( pkcInfo, TRUE ) );
	}
