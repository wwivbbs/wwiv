/****************************************************************************
*																			*
*						cryptlib ECDSA Encryption Routines					*
*			Copyright Matthias Bruestle and Peter Gutmann 2006-2014			*
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

#ifdef USE_ECDSA

/* ECDSA has the same problem with parameters that DSA does (see the comment
   in the DSA code for details), see "Digital Signature Schemes with Domain 
   Parameters", Serge Vaudenay, ACISP'04, p.188.

   In addition, ECDSA signatures have an interesting, or perhaps annoying,
   property that they're trivially malleable.  With ECDSA a signature (r, s) 
   is still valid when it's given as (r, N-s), so there are two distinct 
   encoded forms that will both verify as a signature.  It's not clear that 
   this is a vulnerability in any protocol used by cryptlib, although it's 
   quite possible to design one where it causes problems.  For example 
   PKIX's braindamaged preference for blacklist-based "validation" means 
   that the blacklist can be bypassed by encoding the ECDSA signature into 
   its alternate form, so the signature is still valid but the item that 
   contains it is no longer on the blacklist.

   We could check for and reject signatures that aren't in a particular 
   form, but this will just end up rejecting signatures created by some 
   library that happens to generate them one way or the other.  In addition 
   we could make sure that our signatures are always in a particular form, 
   but again it's not clear which one is the most appropriate one.  The 
   X9.62 test vectors are created using the > N/2 form, while some might 
   regard the < N/2 form as the more correct one.  A particularly amusing 
   trick would be to leak the private key by using the (r, s) form as a 
   subliminal channel.

   To enable both generation of the < N/2 form and checking for the < N/2 
   form, uncomment the following */

/* #define CHECK_ECDSA_MALLEABILITY */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Technically, ECDSA can be used with any hash function, including ones 
   with a block size larger than the subgroup order (although in practice
   the hash function always seems to be matched to the subgroup size).  To
   handle the possibility of a mismatched size we use the following custom 
   conversion function, which applies the conversion rules for transforming 
   the hash value into an integer from X9.62.  For a group order n, of size 
   nlen (where 2 ^ (nlen-1) <= n < 2 ^ nlen), X9.62 mandates that the hash 
   value is first truncated to its leftmost nlen bits if nlen is smaller 
   than the hash value bit length before conversion to a bignum.  
   Mathematically, this is equivalent to first converting the value to a 
   bignum and then right-shifting it by hlen - nlen bits, where hlen is the 
   hash length in bits (a more generic way to view the required conversion 
   is 'while( BN_num_bits( hash ) > BN_num_bits( n ) 
   { BN_rshift( hash, 1 ); }').  Finally, we reduce the value modulo n, 
   which is a simple matter of a compare-and-subtract */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int hashToBignum( INOUT BIGNUM *bignum, 
						 IN_BUFFER( hashLength ) const void *hash, 
						 IN_LENGTH_HASH const int hashLength, 
						 const BIGNUM *n )
	{
	const int hLen = bytesToBits( hashLength );
	const int nLen = BN_num_bits( n );
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );
	assert( isReadPtrDynamic( hash, hashLength ) );
	assert( isReadPtr( n, sizeof( BIGNUM ) ) );

	REQUIRES( sanityCheckBignum( bignum ) );
	REQUIRES( hashLength >= max( 20, MIN_HASHSIZE ) && \
			  hashLength <= CRYPT_MAX_HASHSIZE );
	REQUIRES( nLen >= 20 && nLen <= bytesToBits( CRYPT_MAX_PKCSIZE_ECC ) );

	/* Convert the hash value into a bignum.  We have to be careful when
	   we specify the bounds because, with increasingly smaller 
	   probabilities, the leading bytes of the hash value may be zero.
	   The check used here gives one in 4 billion chance of a false
	   positive */
	status = importBignum( bignum, hash, hashLength, 
						   hashLength - 3, hashLength + 1, NULL, 
						   KEYSIZE_CHECK_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/* Shift out any extra bits */
	if( hLen > nLen )
		{
		CK( BN_rshift( bignum, bignum, hLen - nLen ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		}

	/* Make sure that the value really is smaller than the group order */
	if( BN_cmp( bignum, n ) >= 0 )
		{
		CK( BN_sub( bignum, bignum, n ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		}

	ENSURES( sanityCheckBignum( bignum ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Algorithm Self-test							*
*																			*
****************************************************************************/

/* The SHA-256 hash of the string "Example of ECDSA with ansip256r1 and 
   SHA-256".  Note that X9.62-2005 contains both the message text and its 
   hash value, but the text (as given in X9.62) is wrong since it uses 
   'ansix9p256r1' instead of 'ansip256r1'.  The text above matches the given 
   hash value, and the rest of the test vector */

static const BYTE shaM[] = {
	0x1B, 0xD4, 0xED, 0x43, 0x0B, 0x0F, 0x38, 0x4B,
	0x4E, 0x8D, 0x45, 0x8E, 0xFF, 0x1A, 0x8A, 0x55,
	0x32, 0x86, 0xD7, 0xAC, 0x21, 0xCB, 0x2F, 0x68,
	0x06, 0x17, 0x2E, 0xF5, 0xF9, 0x4A, 0x06, 0xAD
	};

/* For the self-test we use the following fixed k data rather than a 
   randomly-generated value.  The corresponding signature value for the 
   fixed k with P256 should be:

	r = D73CD3722BAE6CC0B39065BB4003D8ECE1EF2F7A8A55BFD677234B0B3B902650
	s = D9C88297FEFED8441E08DDA69554A6452B8A0BD4A0EA1DDB750499F0C2298C2F */

static const BYTE kVal[] = {
	0xA0, 0x64, 0x0D, 0x49, 0x57, 0xF2, 0x7D, 0x09,
	0x1A, 0xB1, 0xAE, 0xBC, 0x69, 0x94, 0x9D, 0x96,
	0xE5, 0xAC, 0x2B, 0xB2, 0x83, 0xED, 0x52, 0x84,
	0xA5, 0x67, 0x47, 0x58, 0xB1, 0x2F, 0x08, 0xDF
	};

#define ECDSA_TESTVECTOR_SIZE	32

/* Perform a pairwise consistency test on a public/private key pair */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN pairwiseConsistencyTest( CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	DLP_PARAMS dlpParams;
	BYTE buffer[ 8 + ( 2 * CRYPT_MAX_PKCSIZE_ECC ) + 8 ];
	int sigSize, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES_B( sanityCheckContext( contextInfoPtr ) );
	REQUIRES_B( capabilityInfoPtr != NULL );

	/* Generate a signature with the private key */
	setDLPParams( &dlpParams, shaM, 32, buffer, 
				  8 + ( 2 * CRYPT_MAX_PKCSIZE_ECC ) );
	dlpParams.inLen2 = -999;
	status = capabilityInfoPtr->signFunction( contextInfoPtr,
						( BYTE * ) &dlpParams, sizeof( DLP_PARAMS ) );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Verify the signature with the public key */
	sigSize = dlpParams.outLen;
	setDLPParams( &dlpParams, shaM, 32, NULL, 0 );
	dlpParams.inParam2 = buffer;
	dlpParams.inLen2 = sigSize;
	status = capabilityInfoPtr->sigCheckFunction( contextInfoPtr,
						( BYTE * ) &dlpParams, sizeof( DLP_PARAMS ) );
	return( cryptStatusOK( status ) ? TRUE : FALSE );
	}

#ifndef CONFIG_NO_SELFTEST

/* Test the ECDSA implementation using the test vectors from ANSI
   X9.62-2005, in this case from section L.4.2, which uses P-256.  Note that
   the test vector contains the Q point in compressed format only.  Qy can 
   be computed from the private key d or by using point decompression, which 
   was done manually here: Qy is one of the square roots of Qx^3-3*Qx+b (the 
   compressed format contains the LSB of Qy, here 1, which allows us to 
   unambiguously choose the square root).

   Because a lot of the high-level encryption routines don't exist yet, we 
   cheat a bit and set up a dummy encryption context with just enough 
   information for the following code to work */

typedef struct {
	const int qxLen; const BYTE qx[ 32 ];
	const int qyLen; const BYTE qy[ 32 ];
	const int dLen; const BYTE d[ 32 ];
	} ECC_KEY;

static const ECC_KEY ecdsaTestKey = {
	/* qx */
	32,
	{ 0x59, 0x63, 0x75, 0xE6, 0xCE, 0x57, 0xE0, 0xF2,
	  0x02, 0x94, 0xFC, 0x46, 0xBD, 0xFC, 0xFD, 0x19,
	  0xA3, 0x9F, 0x81, 0x61, 0xB5, 0x86, 0x95, 0xB3,
	  0xEC, 0x5B, 0x3D, 0x16, 0x42, 0x7C, 0x27, 0x4D },
	32,
	/* qy */
	{ 0x42, 0x75, 0x4D, 0xFD, 0x25, 0xC5, 0x6F, 0x93,
	  0x9A, 0x79, 0xF2, 0xB2, 0x04, 0x87, 0x6B, 0x3A,
	  0x3A, 0xB1, 0xCE, 0xB2, 0xE4, 0xFF, 0x57, 0x1A,
	  0xBF, 0x4F, 0xBF, 0x36, 0x32, 0x6C, 0x8B, 0x27 },
	32,
	/* d */
	{ 0x2C, 0xA1, 0x41, 0x1A, 0x41, 0xB1, 0x7B, 0x24,
	  0xCC, 0x8C, 0x3B, 0x08, 0x9C, 0xFD, 0x03, 0x3F,
	  0x19, 0x20, 0x20, 0x2A, 0x6C, 0x0D, 0xE8, 0xAB,
	  0xB9, 0x7D, 0xF1, 0x49, 0x8D, 0x50, 0xD2, 0xC8 }
	};

CHECK_RETVAL \
static int selfTest( void )
	{
	CONTEXT_INFO contextInfo;
	PKC_INFO contextData, *pkcInfo = &contextData;
	const CAPABILITY_INFO *capabilityInfoPtr;
	int status;

	/* Initialise the key components */
	status = staticInitContext( &contextInfo, CONTEXT_PKC, 
								getECDSACapability(), &contextData, 
								sizeof( PKC_INFO ), NULL );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_FAILED );
	pkcInfo->curveType = CRYPT_ECCCURVE_P256;
	status = importBignum( &pkcInfo->eccParam_qx, ecdsaTestKey.qx, 
						   ecdsaTestKey.qxLen, ECCPARAM_MIN_QX, 
						   ECCPARAM_MAX_QX, NULL, KEYSIZE_CHECK_ECC );
	if( cryptStatusOK( status ) ) 
		{
		status = importBignum( &pkcInfo->eccParam_qy, ecdsaTestKey.qy, 
							   ecdsaTestKey.qyLen, ECCPARAM_MIN_QY, 
							   ECCPARAM_MAX_QY, NULL, KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) ) 
		{
		status = importBignum( &pkcInfo->eccParam_d, ecdsaTestKey.d, 
							   ecdsaTestKey.dLen, ECCPARAM_MIN_D, 
							   ECCPARAM_MAX_D, NULL, KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusError( status ) ) 
		{
		staticDestroyContext( &contextInfo );
		retIntError();
		}
	capabilityInfoPtr = DATAPTR_GET( contextInfo.capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	/* Perform the test sign/sig.check of the X9.62 test values */
	status = capabilityInfoPtr->initKeyFunction( &contextInfo, NULL, 0 );
	if( cryptStatusError( status ) || \
		!pairwiseConsistencyTest( &contextInfo ) )
		{
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}

	/* Try it again with side-channel protection enabled */
	SET_FLAG( contextInfo.flags, CONTEXT_FLAG_SIDECHANNELPROTECTION );
	if( !pairwiseConsistencyTest( &contextInfo ) )
		{
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}

	/* The checking for memory faults is performed at the 
	   MESSAGE_CTX_ENCRYPT level, so it won't be detected when we call the
	   function directly via an internal code pointer */
#if 0
	DLP_PARAMS dlpParams;
	BYTE buffer[ 8 + ( 2 * CRYPT_MAX_PKCSIZE_ECC ) + 8 ];

	/* Finally, make sure that the memory fault-detection is working */
	pkcInfo->eccParam_qx.d[ 1 ] ^= 0x1001;
	setDLPParams( &dlpParams, shaM, 32, buffer, 
				  8 + ( 2 * CRYPT_MAX_PKCSIZE_ECC ) );
	dlpParams.inLen2 = -999;
	status = capabilityInfoPtr->signFunction( &contextInfo,
						( BYTE * ) &dlpParams, sizeof( DLP_PARAMS ) );
	if( cryptStatusOK( status ) )
		{
		/* The fault-detection couldn't detect a bit-flip, there's a 
		   problem */
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}
#else
	/* Emulation of what the above code would do */
	pkcInfo->eccParam_qx.d[ 1 ] ^= 0x1001;
	status = checksumContextData( pkcInfo, CRYPT_ALGO_ECDSA, TRUE );
	if( !cryptStatusError( status ) )
		{
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}
#endif /* 0 */

	/* Clean up */
	staticDestroyContext( &contextInfo );

	return( CRYPT_OK );
	}
#else
	#define selfTest	NULL
#endif /* !CONFIG_NO_SELFTEST */

/****************************************************************************
*																			*
*							Create/Check a Signature						*
*																			*
****************************************************************************/

/* Since ECDSA signature generation produces two values and the 
   cryptEncrypt() model only provides for passing a byte string in and out 
   (or, more specifically, the internal bignum data can't be exported to the 
   outside world) we need to encode the resulting data into a flat format.  
   This is done by encoding the output as an X9.31 Dss-Sig record, which is
   also used for ECDSA:

	Dss-Sig ::= SEQUENCE {
		r	INTEGER,
		s	INTEGER
		} */

/* Sign a single block of data.  There's a possibility of fault attacks 
   against ECDSA as detailed by a variety of authors, "Differential Fault 
   Attacks on Elliptic Curve Cryptosystems", Ingrid Biehl, Bernd Meyer and 
   Volker Mueller, Proceedings of Crypto 2000, Springer-Verlag LNCS No.1880, 
   August 2000, p.131, "Validation of Elliptic Curve Public Keys", Adrian 
   Antipa, Daniel Brown, Alfred Menezes, Ren� Struik and Scott Vanstone, 
   Proceedings of the Public Key Cryptography Conference (PKC'03), Springer-
   Verlag LNCS No.2567, January 2003, p.211, "Elliptic Curve Cryptosystems 
   in the Presence of Permanent and Transient Faults", Mathieu Ciet and Marc 
   Joye, Designs, Codes and Cryptography, Vol.36, No.1 (2005), p.33, "Error 
   Detection and Fault Tolerance in ECSM Using Input Randomisation", Agustin 
   Dominguez-Oviedo and M. Anwar Hasan, IEEE Transactions on Dependable and 
   Secure Computing, Vol.6, No.6, p.175, and "Bit-flip Faults on Elliptic 
   Curve Base Fields, Revisited", Taechan Kim and Mehdi Tibouchi, 
   Proceedings of the Applied Cryptography and Network Security Conference 
   (ACNS'14), Springer-Verlag LNCS No.8479, p.163, and no doubt many more in
   the future.
   
   In some cases this can be defended against by point validation, i.e. 
   through the use of isPointOnCurve() in kg_ecc.c, but a much simpler 
   solution is just to verify the private-key operation with the matching 
   public-key operation after we perform it.  This operation is handled at a 
   higher level (to accomodate algorithms like RSA for which the private-key 
   operation could be a sign or a decrypt and we only need to check the 
   sign), performing a signature verify after each signature generation at 
   the crypto mechanism level */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sign( INOUT CONTEXT_INFO *contextInfoPtr, 
				 INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
				 IN_LENGTH_FIXED( sizeof( DLP_PARAMS ) ) int noBytes )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	DLP_PARAMS *eccParams = ( DLP_PARAMS * ) buffer;
	const PKC_ENCODEDLVALUES_FUNCTION encodeDLValuesFunction = \
					( PKC_ENCODEDLVALUES_FUNCTION ) \
					FNPTR_GET( pkcInfo->encodeDLValuesFunction );
	const ECC_DOMAINPARAMS *domainParams = pkcInfo->domainParams;
	const BIGNUM *n = &domainParams->n;
	BIGNUM *hash = &pkcInfo->tmp1, *x = &pkcInfo->tmp2;
	BIGNUM *k = &pkcInfo->tmp3, *r = &pkcInfo->eccParam_tmp4;
	BIGNUM *s = &pkcInfo->eccParam_tmp5;
	EC_GROUP *ecCTX = pkcInfo->ecCTX;
	EC_POINT *kg = pkcInfo->tmpPoint;
	const int nLen = BN_num_bytes( n );
	int bnStatus = BN_STATUS, status = CRYPT_OK;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( eccParams, sizeof( DLP_PARAMS ) ) );
	assert( isReadPtrDynamic( eccParams->inParam1, eccParams->inLen1 ) );
	assert( isWritePtrDynamic( eccParams->outParam, eccParams->outLen ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( pkcInfo->domainParams != NULL );
	REQUIRES( noBytes == sizeof( DLP_PARAMS ) );
	REQUIRES( eccParams->inParam2 == NULL && \
			  ( eccParams->inLen2 == 0 || eccParams->inLen2 == -999 ) );
	REQUIRES( eccParams->outLen >= MIN_CRYPT_OBJECTSIZE && \
			  eccParams->outLen < MAX_INTLENGTH_SHORT );
	REQUIRES( encodeDLValuesFunction != NULL );
	REQUIRES( nLen >= ECCPARAM_MIN_N && nLen <= ECCPARAM_MAX_N );

	/* Generate the secret random value k.  During the initial self-test the 
	   random data pool may not exist yet, and may in fact never exist in a 
	   satisfactory condition if there isn't enough randomness present in 
	   the system to generate cryptographically strong random numbers.  To 
	   bypass this problem, if the caller passes in a second length 
	   parameter of -999 we know that it's an internal self-test call and 
	   use a fixed bit pattern for k that avoids having to call 
	   generateBignum() (this also means that we can use the fixed self-test 
	   value for k).  This is a somewhat ugly use of 'magic numbers' but 
	   it's safe because this function can only be called internally so all 
	   that we need to trap is accidental use of the parameter which is 
	   normally unused */
	if( eccParams->inLen2 == -999 )
		{
		status = importBignum( k, ( BYTE * ) kVal, ECDSA_TESTVECTOR_SIZE, 
							   ECDSA_TESTVECTOR_SIZE - 1, 
							   ECDSA_TESTVECTOR_SIZE, NULL,
							   KEYSIZE_CHECK_NONE );
		}
	else
		{

		/* Generate the random value k from [1...n-1], i.e. a random value 
		   mod n.  Using a random value of the same length as r would 
		   produce a slight bias in k that leaks a small amount of the 
		   private key in each signature.  Because of this we start with a 
		   value which is DLP_OVERFLOW_SIZE larger than r and then do the 
		   reduction, eliminating the bias.

		   We also add (meaning "mix in" rather than strictly 
		   "arithmetically add") the message hash to k to curtail problems 
		   in the incredibly unlikely situation that the RNG value repeats */
		status = generateBignumEx( k, bytesToBits( nLen ) + \
									  bytesToBits( DLP_OVERFLOW_SIZE ), 0x80, 
								   0, eccParams->inParam1, eccParams->inLen1,
								   NULL );
		}
	if( cryptStatusError( status ) )
		return( status );
	if( TEST_FLAG( contextInfoPtr->flags, 
				   CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		{
		/* Use constant-time modexp() to protect the secret random value 
		   from timing channels.  We could also use blinding, but neither of
		   these measures are actually terribly useful because we're using a 
		   random exponent each time so the timing information isn't of much
		   use to an attacker */
		BN_set_flags( k, BN_FLG_CONSTTIME );
		}
	CK( BN_mod( k, k, n, 				/* Reduce k to the correct range */
				&pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Make sure that the result isn't zero (or more generally less than 64
	   bits).  Admittedly the chances of this are infinitesimally small 
	   (2^-256, the size of the smallest curve, or less for a value of zero, 
	   2^-128 for 64 bits) but someone's bound to complain if we don't 
	   check */
	ENSURES( BN_num_bytes( k ) > 8 );

	/* Convert the hash value to an integer in the proper range */
	status = hashToBignum( hash, eccParams->inParam1, eccParams->inLen1, n );
	if( cryptStatusError( status ) )
		return( status );

	/* Compute the point kG.  EC_POINT_mul() extracts the generator G from 
	   the curve definition (and see the long comment in sigCheck() about 
	   the peculiarities of this function) */
	CK( EC_POINT_mul( ecCTX, kg, k, NULL, NULL, &pkcInfo->bnCTX ) );	
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* r = kG.x mod G.r (s is a dummy) */
	CK( EC_POINT_get_affine_coordinates_GFp( ecCTX, kg, x, s, 
											 &pkcInfo->bnCTX ) );
	CK( BN_mod( r, x, n, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* k = ( k^-1 ) mod n */
	CKPTR( BN_mod_inverse( k, k, n, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* s = k^-1 * ( d * r + e ) mod n */
	CK( BN_mod_mul( s, &pkcInfo->eccParam_d, r, n, &pkcInfo->bnCTX ) );
	CK( BN_mod_add( s, s, hash, n, &pkcInfo->bnCTX ) );
	CK( BN_mod_mul( s, s, k, n, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Optional code to ensure that the signature is always in < N/2 form, 
	   see the comment about ECDSA malleability further up */
#ifdef CHECK_ECDSA_MALLEABILITY
	CKPTR( BN_copy( x, n ) );
	CK( BN_rshift( x, x, 1 ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( BN_cmp( s, x ) > 0 )
		{
		CK( BN_sub( s, n, s ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		}
#endif /* CHECK_ECDSA_MALLEABILITY */

	/* Check that neither r = 0 or s = 0.  See the earlier comment where k 
	   is checked for the real necessity of this check */
	ENSURES( !BN_is_zero( r ) && !BN_is_zero( s ) );

	/* More generally, and usefully, check that the values aren't 
	   suspiciously small */
	if( BN_num_bytes( r ) < nLen - 16 || BN_num_bytes( s ) < nLen - 16 )
		return( CRYPT_ERROR_BADDATA );

	/* Encode the result as a DL data block */
	status = encodeDLValuesFunction( eccParams->outParam, eccParams->outLen, 
									 &eccParams->outLen, r, s, 
									 eccParams->formatType );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

/* Signature check a single block of data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sigCheck( INOUT CONTEXT_INFO *contextInfoPtr, 
					 IN_BUFFER( noBytes ) BYTE *buffer, 
					 IN_LENGTH_FIXED( sizeof( DLP_PARAMS ) ) int noBytes )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	DLP_PARAMS *eccParams = ( DLP_PARAMS * ) buffer;
	const PKC_DECODEDLVALUES_FUNCTION decodeDLValuesFunction = \
					( PKC_DECODEDLVALUES_FUNCTION ) \
					FNPTR_GET( pkcInfo->decodeDLValuesFunction );
	const ECC_DOMAINPARAMS *domainParams = pkcInfo->domainParams;
	const BIGNUM *n = &domainParams->n;
	BIGNUM *qx = &pkcInfo->eccParam_qx, *qy = &pkcInfo->eccParam_qy;
	BIGNUM *u1 = &pkcInfo->tmp1, *u2 = &pkcInfo->tmp2;
	BIGNUM *r = &pkcInfo->tmp3, *s = &pkcInfo->eccParam_tmp4;
	EC_GROUP *ecCTX = pkcInfo->ecCTX;
	EC_POINT *u1gu2q = pkcInfo->tmpPoint, *u2q DUMMY_INIT_PTR;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( eccParams, sizeof( DLP_PARAMS ) ) );
	assert( isReadPtrDynamic( eccParams->inParam1, eccParams->inLen1 ) );
	assert( isReadPtrDynamic( eccParams->inParam2, eccParams->inLen2 ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( pkcInfo->domainParams != NULL );
	REQUIRES( noBytes == sizeof( DLP_PARAMS ) );
	REQUIRES( eccParams->outParam == NULL && eccParams->outLen == 0 );
	REQUIRES( decodeDLValuesFunction != NULL );

	/* Decode the values from a DL data block and make sure that r and s are
	   valid, i.e. r, s = [1...n-1] */
	status = decodeDLValuesFunction( eccParams->inParam2, eccParams->inLen2, 
									 r, s, n, eccParams->formatType );
	if( cryptStatusError( status ) )
		return( status );

	/* Optional code to check that the signature is always in < N/2 form, 
	   see the comment about ECDSA malleability further up */
#ifdef CHECK_ECDSA_MALLEABILITY
	CKPTR( BN_copy( u1, n ) );
	CK( BN_rshift( u1, u1, 1 ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( BN_cmp( s, u1 ) > 0 )
		return( CRYPT_ERROR_SIGNATURE );
#endif /* CHECK_ECDSA_MALLEABILITY */

	/* Convert the hash value to an integer in the proper range. */
	status = hashToBignum( u1, eccParams->inParam1, eccParams->inLen1, n );
	if( cryptStatusError( status ) )
		return( status );

	/* We've got all the data that we need, allocate the EC points working 
	   variable */
	CKPTR( u2q = EC_POINT_new( ecCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* w = s^-1 mod G.r */
	CKPTR( BN_mod_inverse( u2, s, n, &pkcInfo->bnCTX ) );

	/* u1 = ( hash * w ) mod G.r */
	CK( BN_mod_mul( u1, u1, u2, n, &pkcInfo->bnCTX ) );

	/* u2 = ( r * w ) mod G.r */
	CK( BN_mod_mul( u2, r, u2, n, &pkcInfo->bnCTX ) );

	/* R = u1*G + u2*Q.  EC_POINT_mul() is a somewhat weird function that 
	   supports faster ECDSA signature verification by allowing two point 
	   multiplications to be done in a single call to EC_POINT_mul().  The 
	   implementation of the multiplication process uses window-based 
	   optimizations (also known as "Shamir's trick", according to the 
	   "Guide to Elliptic Curve Cryptography") which computes nG + mQ faster 
	   than if both point multiplications were done separately */
	CK( EC_POINT_set_affine_coordinates_GFp( ecCTX, u2q, qx, qy, 
											 &pkcInfo->bnCTX ) );
	CK( EC_POINT_mul( ecCTX, u1gu2q, u1, u2q, u2, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		{
		EC_POINT_free( u2q );
		return( getBnStatus( bnStatus ) );
		}

	/* Convert point (x1, y1) to an integer r':

		r' = p((x1, y1)) mod n
		   = x1 mod n */
	CK( EC_POINT_get_affine_coordinates_GFp( ecCTX, u1gu2q, u1, u2, 
											 &pkcInfo->bnCTX ) );
	CK( BN_mod( u1, u1, n, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		{
		EC_POINT_free( u2q );
		return( getBnStatus( bnStatus ) );
		}

	/* Clean up */
	EC_POINT_free( u2q );

	/* If r == r' then the signature is good */
	if( BN_cmp( r, u1 ) )
		return( CRYPT_ERROR_SIGNATURE );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( status );
	}

/****************************************************************************
*																			*
*								Key Management								*
*																			*
****************************************************************************/

/* Load key components into an encryption context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initKey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_BUFFER_OPT( keyLength ) const void *key,
					IN_LENGTH_SHORT_OPT const int keyLength )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( ( key == NULL && keyLength == 0 ) || \
			( isReadPtrDynamic( key, keyLength ) && \
			  keyLength == sizeof( CRYPT_PKCINFO_ECC ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( ( key == NULL && keyLength == 0 ) || \
			  ( key != NULL && keyLength == sizeof( CRYPT_PKCINFO_ECC ) ) );

#ifndef USE_FIPS140
	/* Load the key component from the external representation into the
	   internal bignums unless we're doing an internal load */
	if( key != NULL )
		{
		PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
		const CRYPT_PKCINFO_ECC *eccKey = ( CRYPT_PKCINFO_ECC * ) key;
		int status;

		if( eccKey->isPublicKey )
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY );
		else
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPRIVATEKEY );
#if 0	/* We don't allow explicit ECC parameters since all curves are 
		   predefined */
		if( eccKey->curveType == CRYPT_ECCCURVE_NONE )
			{
			status = importBignum( &pkcInfo->eccParam_p, eccKey->p, 
								   bitsToBytes( eccKey->pLen ),
								   ECCPARAM_MIN_P, ECCPARAM_MAX_P,
								   NULL, KEYSIZE_CHECK_ECC );
			if( cryptStatusOK( status ) )
				{
				status = importBignum( &pkcInfo->eccParam_a, eccKey->a, 
									   bitsToBytes( eccKey->aLen ),
									   ECCPARAM_MIN_A, ECCPARAM_MAX_A,
									   NULL, KEYSIZE_CHECK_NONE );
				}
			if( cryptStatusOK( status ) )
				{
				status = importBignum( &pkcInfo->eccParam_b, eccKey->b, 
									   bitsToBytes( eccKey->bLen ),
									   ECCPARAM_MIN_B, ECCPARAM_MAX_B,
									   NULL, KEYSIZE_CHECK_NONE );
				}
			if( cryptStatusOK( status ) )
				{
				status = importBignum( &pkcInfo->eccParam_gx, eccKey->gx, 
									   bitsToBytes( eccKey->gxLen ),
									   ECCPARAM_MIN_GX, ECCPARAM_MAX_GX,
									   NULL, KEYSIZE_CHECK_NONE );
				}
			if( cryptStatusOK( status ) )
				{
				status = importBignum( &pkcInfo->eccParam_gy, eccKey->gy, 
									   bitsToBytes( eccKey->gyLen ),
									   ECCPARAM_MIN_GY, ECCPARAM_MAX_GY,
									   NULL, KEYSIZE_CHECK_NONE );
				}
			if( cryptStatusOK( status ) )
				{
				status = importBignum( &pkcInfo->eccParam_n, eccKey->n, 
									   bitsToBytes( eccKey->nLen ),
									   ECCPARAM_MIN_N, ECCPARAM_MAX_N,
									   NULL, KEYSIZE_CHECK_NONE );
				}
			if( cryptStatusError( status ) )
				return( status );
			}
		else
#endif /* 0 */
			{
			if( !isEnumRange( eccKey->curveType, CRYPT_ECCCURVE ) )
				return( CRYPT_ARGERROR_STR1 );
			pkcInfo->curveType = eccKey->curveType;
			}
		status = importBignum( &pkcInfo->eccParam_qx, eccKey->qx, 
							   bitsToBytes( eccKey->qxLen ),
							   ECCPARAM_MIN_QX, ECCPARAM_MAX_QX,
							   NULL, KEYSIZE_CHECK_NONE );
		if( cryptStatusOK( status ) )
			{
			status = importBignum( &pkcInfo->eccParam_qy, eccKey->qy, 
								   bitsToBytes( eccKey->qyLen ),
								   ECCPARAM_MIN_QY, ECCPARAM_MAX_QY,
								   NULL, KEYSIZE_CHECK_NONE );
			}
		if( cryptStatusOK( status ) && !eccKey->isPublicKey )
			{
			status = importBignum( &pkcInfo->eccParam_d, eccKey->d, 
								   bitsToBytes( eccKey->dLen ),
								   ECCPARAM_MIN_D, ECCPARAM_MAX_D,
								   NULL, KEYSIZE_CHECK_NONE );
			}
		SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_PBO );
		if( cryptStatusError( status ) )
			return( status );

		ENSURES( sanityCheckPKCInfo( pkcInfo ) );
		}
#endif /* USE_FIPS140 */

	/* Complete the key checking and setup */
	return( initCheckECCkey( contextInfoPtr, FALSE ) );
	}

/* Generate a key into an encryption context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateKey( INOUT CONTEXT_INFO *contextInfoPtr, 
						IN_LENGTH_SHORT_MIN( MIN_PKCSIZE_ECC * 8 ) \
							const int keySizeBits )
	{
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( keySizeBits >= bytesToBits( MIN_PKCSIZE_ECC ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE_ECC ) );

	status = generateECCkey( contextInfoPtr, keySizeBits );
	if( cryptStatusOK( status ) &&
#ifndef USE_FIPS140
		TEST_FLAG( contextInfoPtr->flags, 
				   CONTEXT_FLAG_SIDECHANNELPROTECTION ) &&
#endif /* USE_FIPS140 */
		!pairwiseConsistencyTest( contextInfoPtr ) )
		{
		DEBUG_DIAG(( "Consistency check of freshly-generated ECDSA key "
					 "failed" ));
		assert( DEBUG_WARN );
		status = CRYPT_ERROR_FAILED;
		}
	return( cryptArgError( status ) ? CRYPT_ERROR_FAILED : status );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO capabilityInfo = {
	CRYPT_ALGO_ECDSA, bitsToBytes( 0 ), "ECDSA", 5,
	MIN_PKCSIZE_ECC, bitsToBytes( 256 ), CRYPT_MAX_PKCSIZE_ECC,
	selfTest, getDefaultInfo, NULL, NULL, initKey, generateKey,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	sign, sigCheck
	};

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getECDSACapability( void )
	{
	return( &capabilityInfo );
	}
#endif /* USE_ECDSA */
