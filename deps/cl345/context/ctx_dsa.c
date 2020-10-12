/****************************************************************************
*																			*
*						cryptlib DSA Encryption Routines					*
*						Copyright Peter Gutmann 1995-2005					*
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

#ifdef USE_DSA

/****************************************************************************
*																			*
*						Predefined DSA p, q, and g Parameters				*
*																			*
****************************************************************************/

/* We never use shared DSA parameters because they allow forgery of
   signatures on certificates.  This works as follows: Suppose that the
   certificate contains a copy of the certificate signer's DSA parameters,
   and the verifier of the certificate has a copy of the signer's public key
   but not the signer's DSA parameters (which are shared with other keys).
   If the verifier uses the DSA parameters from the certificate along with
   the signer's public key to verify the signature on the certificate, then
   an attacker can create bogus certificates by choosing a random u and
   finding its inverse v modulo q (uv is congruent to 1 modulo q).  Then
   take the certificate signer's public key g^x and compute g' = (g^x)^u.
   Then g'^v = g^x.  Using the DSA parameters p, q, g', the signer's public
   key corresponds to the private key v, which the attacker knows.  The
   attacker can then create a bogus certificate, put parameters (p, q, g')
   in it, and sign it with the DSA private key v to create an apparently
   valid certificate.  This works with the DSA OID that makes p, q, and g
   unauthenticated public parameters and y the public key, but not the one
   that makes p, q, g, and y the public key */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Technically, post-FIPS 186-2 DSA can be used with any hash function, 
   including ones with a block size larger than the subgroup order (although 
   in practice the hash function always seems to be matched to the subgroup 
   size).  To handle the possibility of a mismatched size we use the 
   following custom conversion function, which applies the conversion rules 
   for transforming the hash value into an integer from FIPS 186-3, "the 
   leftmost min( N, outlen ) bits of Hash( M )" where N = sizeof( q ).  
   Mathematically, this is equivalent to first converting the value to a 
   bignum and then right-shifting it by hlen - nlen bits, where hlen is the 
   hash length in bits (a more generic way to view the required conversion is 
   'while( BN_num_bits( hash ) > BN_num_bits( n ) { BN_rshift( hash, 1 ); }') */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int hashToBignum( INOUT BIGNUM *bignum, 
						 IN_BUFFER( hashLength ) const void *hash, 
						 IN_LENGTH_HASH const int hashLength, 
						 const BIGNUM *q )
	{
	const int hLen = bytesToBits( hashLength );
	const int qLen = BN_num_bits( q );
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );
	assert( isReadPtrDynamic( hash, hashLength ) );
	assert( isReadPtr( q, sizeof( BIGNUM ) ) );

	REQUIRES( sanityCheckBignum( bignum ) );
	REQUIRES( hashLength >= max( 20, MIN_HASHSIZE ) && \
			  hashLength <= CRYPT_MAX_HASHSIZE );
	REQUIRES( qLen >= bytesToBits( 20 ) && \
			  qLen <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

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
	if( hLen > qLen )
		{
		CK( BN_rshift( bignum, bignum, hLen - qLen ) );
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

/* Since we're doing the self-test using the FIPS 186 values we use the 
   following fixed k and hash values rather than a randomly-generated 
   value */

static const BYTE shaM[] = {
	0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A,
	0xBA, 0x3E, 0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C,
	0x9C, 0xD0, 0xD8, 0x9D
	};

static const BYTE kVal[] = {
	0x35, 0x8D, 0xAD, 0x57, 0x14, 0x62, 0x71, 0x0F,
	0x50, 0xE2, 0x54, 0xCF, 0x1A, 0x37, 0x6B, 0x2B,
	0xDE, 0xAA, 0xDF, 0xBF
	};

/* Perform a pairwise consistency test on a public/private key pair */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN pairwiseConsistencyTest( CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	DLP_PARAMS dlpParams;
	BYTE buffer[ 128 + 8 ];
	int sigSize, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES_B( sanityCheckContext( contextInfoPtr ) );
	REQUIRES_B( capabilityInfoPtr != NULL );

	/* Generate a signature with the private key */
	setDLPParams( &dlpParams, shaM, 20, buffer, 128 );
	dlpParams.inLen2 = -999;
	status = capabilityInfoPtr->signFunction( contextInfoPtr,
						( BYTE * ) &dlpParams, sizeof( DLP_PARAMS ) );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Verify the signature with the public key */
	sigSize = dlpParams.outLen;
	setDLPParams( &dlpParams, shaM, 20, NULL, 0 );
	dlpParams.inParam2 = buffer;
	dlpParams.inLen2 = sigSize;
	status = capabilityInfoPtr->sigCheckFunction( contextInfoPtr,
						( BYTE * ) &dlpParams, sizeof( DLP_PARAMS ) );
	return( cryptStatusOK( status ) ? TRUE : FALSE );
	}

#ifndef CONFIG_NO_SELFTEST

/* Test the DSA implementation using the sample key and hash from FIPS 186
   (we actually use a generated 1024-bit key because the FIPS 186 key at
   512 bits is too short to load).  Because a lot of the high-level 
   encryption routines don't exist yet, we cheat a bit and set up a dummy 
   encryption context with just enough information for the following code to 
   work */

typedef struct {
	const int pLen; const BYTE p[ 128 ];
	const int qLen; const BYTE q[ 20 ];
	const int gLen; const BYTE g[ 128 ];
	const int xLen; const BYTE x[ 20 ];
	const int yLen; const BYTE y[ 128 ];
	} DLP_KEY;

#if 0	/* FIPS 186 test key is too small to be loaded */

static const DLP_PRIVKEY dlpTestKey = {
	/* p */
	64,
	{ 0x8D, 0xF2, 0xA4, 0x94, 0x49, 0x22, 0x76, 0xAA,
	  0x3D, 0x25, 0x75, 0x9B, 0xB0, 0x68, 0x69, 0xCB,
	  0xEA, 0xC0, 0xD8, 0x3A, 0xFB, 0x8D, 0x0C, 0xF7,
	  0xCB, 0xB8, 0x32, 0x4F, 0x0D, 0x78, 0x82, 0xE5,
	  0xD0, 0x76, 0x2F, 0xC5, 0xB7, 0x21, 0x0E, 0xAF,
	  0xC2, 0xE9, 0xAD, 0xAC, 0x32, 0xAB, 0x7A, 0xAC,
	  0x49, 0x69, 0x3D, 0xFB, 0xF8, 0x37, 0x24, 0xC2,
	  0xEC, 0x07, 0x36, 0xEE, 0x31, 0xC8, 0x02, 0x91 },
	/* q */
	20,
	{ 0xC7, 0x73, 0x21, 0x8C, 0x73, 0x7E, 0xC8, 0xEE,
	  0x99, 0x3B, 0x4F, 0x2D, 0xED, 0x30, 0xF4, 0x8E,
	  0xDA, 0xCE, 0x91, 0x5F },
	/* g */
	64,
	{ 0x62, 0x6D, 0x02, 0x78, 0x39, 0xEA, 0x0A, 0x13,
	  0x41, 0x31, 0x63, 0xA5, 0x5B, 0x4C, 0xB5, 0x00,
	  0x29, 0x9D, 0x55, 0x22, 0x95, 0x6C, 0xEF, 0xCB,
	  0x3B, 0xFF, 0x10, 0xF3, 0x99, 0xCE, 0x2C, 0x2E,
	  0x71, 0xCB, 0x9D, 0xE5, 0xFA, 0x24, 0xBA, 0xBF,
	  0x58, 0xE5, 0xB7, 0x95, 0x21, 0x92, 0x5C, 0x9C,
	  0xC4, 0x2E, 0x9F, 0x6F, 0x46, 0x4B, 0x08, 0x8C,
	  0xC5, 0x72, 0xAF, 0x53, 0xE6, 0xD7, 0x88, 0x02 },
	/* x */
	20,
	{ 0x20, 0x70, 0xB3, 0x22, 0x3D, 0xBA, 0x37, 0x2F,
	  0xDE, 0x1C, 0x0F, 0xFC, 0x7B, 0x2E, 0x3B, 0x49,
	  0x8B, 0x26, 0x06, 0x14 },
	/* y */
	64,
	{ 0x19, 0x13, 0x18, 0x71, 0xD7, 0x5B, 0x16, 0x12,
	  0xA8, 0x19, 0xF2, 0x9D, 0x78, 0xD1, 0xB0, 0xD7,
	  0x34, 0x6F, 0x7A, 0xA7, 0x7B, 0xB6, 0x2A, 0x85,
	  0x9B, 0xFD, 0x6C, 0x56, 0x75, 0xDA, 0x9D, 0x21,
	  0x2D, 0x3A, 0x36, 0xEF, 0x16, 0x72, 0xEF, 0x66,
	  0x0B, 0x8C, 0x7C, 0x25, 0x5C, 0xC0, 0xEC, 0x74,
	  0x85, 0x8F, 0xBA, 0x33, 0xF4, 0x4C, 0x06, 0x69,
	  0x96, 0x30, 0xA7, 0x6B, 0x03, 0x0E, 0xE3, 0x33 }
	};
#endif /* 0 */

static const DLP_KEY dlpTestKey = {
	/* p */
	128,
	{ 0x04, 0x4C, 0xDD, 0x5D, 0xB6, 0xED, 0x23, 0xAE, 
	  0xB2, 0xA7, 0x59, 0xE6, 0xF8, 0x3D, 0xA6, 0x27, 
	  0x85, 0xF2, 0xFE, 0xE2, 0xE8, 0xF3, 0xDA, 0xA3, 
	  0x7B, 0xD6, 0x48, 0xD4, 0x44, 0xCA, 0x6E, 0x10, 
	  0x97, 0x6C, 0x1D, 0x6C, 0x39, 0xA7, 0x0C, 0x88, 
	  0x8E, 0x1F, 0xDD, 0xF7, 0x59, 0x69, 0xDA, 0x36, 
	  0xDD, 0xB8, 0x3E, 0x1A, 0xD2, 0x91, 0x3E, 0x30, 
	  0xB1, 0xB5, 0xC2, 0xBC, 0xA9, 0xA3, 0xA5, 0xDE, 
	  0xC7, 0xCF, 0x51, 0x2C, 0x1B, 0x89, 0xD0, 0x71, 
	  0xE3, 0x71, 0xBB, 0x50, 0x86, 0x26, 0x32, 0x9F, 
	  0xF5, 0x4A, 0x9C, 0xB1, 0x78, 0x7B, 0x47, 0x1F, 
	  0x19, 0xC7, 0x26, 0x22, 0x15, 0x62, 0x71, 0xAB, 
	  0xD7, 0x25, 0xA5, 0xE4, 0x68, 0x71, 0x93, 0x5D, 
	  0x1F, 0x29, 0x01, 0x05, 0x9C, 0x57, 0x3A, 0x09, 
	  0xB0, 0xB8, 0xE4, 0xD2, 0x37, 0x90, 0x36, 0x2F, 
	  0xBF, 0x1E, 0x74, 0xB4, 0x6B, 0xE4, 0x66, 0x07 }, 

	/* q */
	20,
	{ 0xFD, 0xD9, 0xC8, 0x5F, 0x73, 0x62, 0xC9, 0x79, 
	  0xEF, 0xD5, 0x09, 0x07, 0x02, 0xE7, 0xF2, 0x90, 
	  0x97, 0x13, 0x26, 0x1D }, 

	/* g */
	128,
	{ 0x02, 0x4E, 0xDD, 0x0D, 0x7F, 0x4D, 0xB1, 0x42, 
	  0x01, 0x50, 0xE7, 0x9A, 0x65, 0x73, 0x8B, 0x31, 
	  0x24, 0x6B, 0xC6, 0x74, 0xA7, 0x68, 0x26, 0x11, 
	  0x06, 0x3C, 0x96, 0xA9, 0xA6, 0x23, 0x12, 0x79, 
	  0xC4, 0xEE, 0x21, 0x88, 0xDD, 0xE3, 0xF0, 0x37, 
	  0xCE, 0x3E, 0x54, 0x53, 0x57, 0x03, 0x30, 0xE4, 
	  0xD3, 0xAB, 0x39, 0x4E, 0x39, 0xDC, 0xA2, 0x88, 
	  0x82, 0xF6, 0xE8, 0xBA, 0xAC, 0xF5, 0x7D, 0x2F, 
	  0x23, 0x9A, 0x09, 0x94, 0xB2, 0x89, 0xA2, 0xC9, 
	  0x7C, 0xBE, 0x4D, 0x48, 0x0E, 0x59, 0x51, 0xB8, 
	  0x7D, 0x99, 0x88, 0x79, 0xA8, 0x13, 0x0E, 0x12, 
	  0x56, 0x9D, 0x4B, 0x2E, 0xE0, 0xE1, 0x37, 0x78, 
	  0x6F, 0xCC, 0x4D, 0x97, 0xA9, 0x02, 0x0E, 0xD2, 
	  0x43, 0x83, 0xEC, 0x4F, 0xC2, 0x70, 0xEF, 0x16, 
	  0xDE, 0xBF, 0xBA, 0xD1, 0x6C, 0x8A, 0x36, 0xEE, 
	  0x42, 0x41, 0xE9, 0xE7, 0x66, 0xAE, 0x46, 0x3B }, 

	/* x */
	20,
	{ 0xD9, 0x41, 0x29, 0xF7, 0x40, 0x32, 0x09, 0x71, 
	  0xB8, 0xE2, 0xB8, 0xCB, 0x74, 0x46, 0x0B, 0xD4, 
	  0xF2, 0xAB, 0x54, 0xA1 }, 

	/* y */
	128,
	{ 0x01, 0x7E, 0x16, 0x5B, 0x65, 0x51, 0x0A, 0xDA, 
	  0x82, 0x1A, 0xD9, 0xF4, 0x1E, 0x66, 0x6D, 0x7D, 
	  0x23, 0xA6, 0x28, 0x2F, 0xE6, 0xC2, 0x03, 0x8E, 
	  0x8C, 0xAB, 0xC2, 0x08, 0x87, 0xC9, 0xE8, 0x51, 
	  0x0A, 0x37, 0x1E, 0xD4, 0x41, 0x7F, 0xA2, 0xC5, 
	  0x48, 0x26, 0xB7, 0xF6, 0xC2, 0x6F, 0xB2, 0xF8, 
	  0xF9, 0x43, 0x43, 0xF9, 0xDA, 0xAB, 0xA2, 0x59, 
	  0x27, 0xBA, 0xC9, 0x1C, 0x8C, 0xAB, 0xC4, 0x90, 
	  0x27, 0xE1, 0x10, 0x39, 0x6F, 0xD2, 0xCD, 0x7C, 
	  0xD1, 0x0B, 0xFA, 0x28, 0xD2, 0x7A, 0x7B, 0x52, 
	  0x8A, 0xA0, 0x5A, 0x0F, 0x10, 0xF7, 0xBA, 0xFD, 
	  0x33, 0x0C, 0x3C, 0xCE, 0xE5, 0xF2, 0xF6, 0x92, 
	  0xED, 0x04, 0xBF, 0xD3, 0xF8, 0x3D, 0x39, 0xCC, 
	  0xAA, 0xCC, 0x0B, 0xB2, 0x6B, 0xD8, 0xB2, 0x8A, 
	  0x5C, 0xCE, 0xDA, 0xF9, 0xE1, 0xA7, 0x23, 0x50, 
	  0xDC, 0xCE, 0xA4, 0xD5, 0xA5, 0x4F, 0x08, 0x0F }
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
								getDSACapability(), &contextData, 
								sizeof( PKC_INFO ), NULL );
	if( cryptStatusError( status ) )
		return( status );
	status = importBignum( &pkcInfo->dlpParam_p, dlpTestKey.p, 
						   dlpTestKey.pLen, DLPPARAM_MIN_P, 
						   DLPPARAM_MAX_P, NULL, KEYSIZE_CHECK_PKC );
	if( cryptStatusOK( status ) ) 
		{
		status = importBignum( &pkcInfo->dlpParam_q, dlpTestKey.q, 
							   dlpTestKey.qLen, DLPPARAM_MIN_Q, 
							   DLPPARAM_MAX_Q, &pkcInfo->dlpParam_p, 
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) ) 
		{
		status = importBignum( &pkcInfo->dlpParam_g, dlpTestKey.g, 
							   dlpTestKey.gLen, DLPPARAM_MIN_G, 
							   DLPPARAM_MAX_G, &pkcInfo->dlpParam_p,
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) ) 
		{
		status = importBignum( &pkcInfo->dlpParam_y, dlpTestKey.y, 
							   dlpTestKey.yLen, DLPPARAM_MIN_Y, 
							   DLPPARAM_MAX_Y, &pkcInfo->dlpParam_p,
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) ) 
		{
		status = importBignum( &pkcInfo->dlpParam_x, dlpTestKey.x, 
							   dlpTestKey.xLen, DLPPARAM_MIN_X, 
							   DLPPARAM_MAX_X, &pkcInfo->dlpParam_p, 
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusError( status ) ) 
		{
		staticDestroyContext( &contextInfo );
		retIntError();
		}
	capabilityInfoPtr = DATAPTR_GET( contextInfo.capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	/* Perform the test sign/sig.check of the FIPS 186 test values */
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
	BYTE buffer[ 128 + 8 ];

	/* Finally, make sure that the memory fault-detection is working */
	pkcInfo->dlpParam_g.d[ 8 ] ^= 0x0011;
	setDLPParams( &dlpParams, shaM, 20, buffer, 128 );
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
	pkcInfo->dlpParam_g.d[ 8 ] ^= 0x0011;
	status = checksumContextData( pkcInfo, CRYPT_ALGO_DSA, TRUE );
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

/* Since DSA signature generation produces two values and the cryptEncrypt()
   model only provides for passing a byte string in and out (or, more
   specifically, the internal bignum data can't be exported to the outside
   world) we need to encode the resulting data into a flat format.  This is
   done by encoding the output as an X9.31 Dss-Sig record:

	Dss-Sig ::= SEQUENCE {
		r	INTEGER,
		s	INTEGER
		} */

/* Sign a single block of data  */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sign( INOUT CONTEXT_INFO *contextInfoPtr, 
				 INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
				 IN_LENGTH_FIXED( sizeof( DLP_PARAMS ) ) int noBytes )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) buffer;
	const PKC_ENCODEDLVALUES_FUNCTION encodeDLValuesFunction = \
					( PKC_ENCODEDLVALUES_FUNCTION ) \
					FNPTR_GET( pkcInfo->encodeDLValuesFunction );
	BIGNUM *p = &pkcInfo->dlpParam_p, *q = &pkcInfo->dlpParam_q;
	BIGNUM *g = &pkcInfo->dlpParam_g, *x = &pkcInfo->dlpParam_x;
	BIGNUM *hash = &pkcInfo->tmp1, *k = &pkcInfo->tmp2, *kInv = &pkcInfo->tmp3;
	BIGNUM *r = &pkcInfo->dlpTmp1, *s = &pkcInfo->dlpTmp2;
	const int qLen = BN_num_bytes( q );
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( dlpParams, sizeof( DLP_PARAMS ) ) );
	assert( isReadPtrDynamic( dlpParams->inParam1, dlpParams->inLen1 ) );
	assert( isWritePtrDynamic( dlpParams->outParam, dlpParams->outLen ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( noBytes == sizeof( DLP_PARAMS ) );
	REQUIRES( dlpParams->inLen1 >= max( 20, MIN_HASHSIZE ) && \
			  dlpParams->inLen1 <= CRYPT_MAX_HASHSIZE );
	REQUIRES( dlpParams->inParam2 == NULL && \
			  ( dlpParams->inLen2 == 0 || dlpParams->inLen2 == -999 ) );
	REQUIRES( dlpParams->outLen >= ( 2 + dlpParams->inLen1 ) * 2 && \
			  dlpParams->outLen < MAX_INTLENGTH_SHORT );
	REQUIRES( encodeDLValuesFunction != NULL );
	REQUIRES( qLen >= DLPPARAM_MIN_Q && qLen <= DLPPARAM_MAX_Q )

	/* Generate the secret random value k.  During the initial self-test
	   the random data pool may not exist yet, and may in fact never exist in
	   a satisfactory condition if there isn't enough randomness present in
	   the system to generate cryptographically strong random numbers.  To
	   bypass this problem, if the caller passes in a second length parameter
	   of -999 we know that it's an internal self-test call and use a fixed
	   bit pattern for k that avoids having to call generateBignum() (this
	   also means that we can use the FIPS 186 self-test value for k).  This 
	   is a somewhat ugly use of 'magic numbers', but it's safe because this
	   function can only be called internally, so all we need to trap is
	   accidental use of the parameter which is normally unused.
	   
	   Since the size of k, in bits, is less than 160 (20 bytes), we give
	   the minimum length as 19 bytes rather than 20 */
	if( dlpParams->inLen2 == -999 )
		{
		status = importBignum( k, ( BYTE * ) kVal, 20, 19, 20, NULL, 
							   KEYSIZE_CHECK_NONE );
		}
	else
		{
		/* Generate the random value k.  FIPS 186 requires (Appendix 3)
		   that this be done with:

			k = G(t,KKEY) mod q

		   where G(t,c) produces a 160-bit output, however this produces a
		   slight bias in k that that allows the private key x to be 
		   recovered once sufficient signatures have been generated.  The 
		   difficulty of recovering x depends on the number of signatures 
		   generated, the number of bits leaked, the how recent the attack 
		   is (they get better over time).  The best reference for this is 
		   probably "The Insecurity of the Digital Signature Algorithm with 
		   Partially Known Nonces" by Phong Nguyen and Igor Shparlinski or 
		   more recently Serge Vaudenay's "Evaluation Report on DSA"), but 
		   as a rule of thumb even three or four known bits and a handful of 
		   signatures is enough to allow recovery of x.  Because of this we 
		   start with a value which is DLP_OVERFLOW_SIZE bytes larger than q 
		   and then do the reduction, eliminating the bias, which was also
		   required by later versions of FIPS 186.
		   
		   We also add (meaning "mix in" rather than strictly 
		   "arithmetically add") the message hash to k to curtail problems 
		   in the incredibly unlikely situation that the RNG value repeats */
		REQUIRES( qLen >= 20 && qLen <= CRYPT_MAX_PKCSIZE );
		status = generateBignumEx( k, bytesToBits( qLen ) + \
									  bytesToBits( DLP_OVERFLOW_SIZE ), 0x80, 
								   0, dlpParams->inParam1, dlpParams->inLen1,
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
	CK( BN_mod( k, k, q, 				/* Reduce k to the correct range */
				&pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Make sure that the result isn't zero (or more generally less than 64
	   bits).  Admittedly the chances of this are infinitesimally small 
	   (2^-160 or less for a value of zero, 2^-96 for 64 bits) but someone's 
	   bound to complain if we don't check */
	ENSURES( BN_num_bytes( k ) > 8 );

	/* Convert the hash value to an integer in the proper range */
	status = hashToBignum( hash, dlpParams->inParam1, dlpParams->inLen1, q );
	if( cryptStatusError( status ) )
		return( status );

	/* r = ( g ^ k mod p ) mod q */
	CK( BN_mod_exp_mont( r, g, k, p, &pkcInfo->bnCTX,
						 &pkcInfo->dlpParam_mont_p ) );
	CK( BN_mod( r, r, q, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* s = k^-1 * ( hash + x * r ) mod q */
	CKPTR( BN_mod_inverse( kInv, k, q,	/* temp = k^-1 mod q */
						   &pkcInfo->bnCTX ) );
	CK( BN_mod_mul( s, x, r, q,			/* s = ( x * r ) mod q */
					&pkcInfo->bnCTX ) );
	CK( BN_add( s, s, hash ) );			/* s = s + hash */
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( BN_cmp( s, q ) > 0 )			/* if s > q */
		{
		CK( BN_sub( s, s, q ) );		/*   s = s - q (fast mod) */
		}
	else
		{
		/* Perform a dummy subtract on a value around the same size as s 
		   (it's one bit shorter) to balance out the timing */
		( void ) BN_sub( k, k, q );
		}
	CK( BN_mod_mul( s, s, kInv, q,		/* s = k^-1 * ( hash + x * r ) mod q */
					&pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Check that neither r = 0 or s = 0.  See the earlier comment where k 
	   is  checked for the real necessity of this check */
	ENSURES( !BN_is_zero( r ) && !BN_is_zero( s ) );

	/* More generally, and usefully, check that the values aren't 
	   suspiciously small.  Since qLen is typically only 20 bytes anyway we
	   can't use the standard 128-bits-of-zeroes check since we only have
	   160 bits to work with in the first place, so we use 80 bits as the
	   best tradeoff between FPs and allowing bogus values through */
	if( BN_num_bytes( r ) < qLen - 10 || BN_num_bytes( s ) < qLen - 10 )
		return( CRYPT_ERROR_BADDATA );

	/* Encode the result as a DL data block */
	status = encodeDLValuesFunction( dlpParams->outParam, dlpParams->outLen, 
									 &dlpParams->outLen, r, s, 
									 dlpParams->formatType );
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
	DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) buffer;
	const PKC_DECODEDLVALUES_FUNCTION decodeDLValuesFunction = \
					( PKC_DECODEDLVALUES_FUNCTION ) \
					FNPTR_GET( pkcInfo->decodeDLValuesFunction );
	BIGNUM *p = &pkcInfo->dlpParam_p, *q = &pkcInfo->dlpParam_q;
	BIGNUM *g = &pkcInfo->dlpParam_g, *y = &pkcInfo->dlpParam_y;
	BIGNUM *r = &pkcInfo->tmp1, *s = &pkcInfo->tmp2;
	BIGNUM *u1 = &pkcInfo->tmp3, *u2 = &pkcInfo->dlpTmp1;	/* Doubles as w */
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( dlpParams, sizeof( DLP_PARAMS ) ) );
	assert( isReadPtrDynamic( dlpParams->inParam1, dlpParams->inLen1 ) );
	assert( isReadPtrDynamic( dlpParams->inParam2, dlpParams->inLen2 ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( noBytes == sizeof( DLP_PARAMS ) );
	REQUIRES( ( dlpParams->formatType == CRYPT_FORMAT_CRYPTLIB && \
				( dlpParams->inLen2 >= 42 && dlpParams->inLen2 <= 128 ) ) || \
			  ( dlpParams->formatType == CRYPT_FORMAT_PGP && \
				( dlpParams->inLen2 >= 42 && dlpParams->inLen2 <= 128 ) ) || \
			  ( dlpParams->formatType == CRYPT_IFORMAT_SSH && \
				dlpParams->inLen2 == 40 ) );
	REQUIRES( dlpParams->inLen1 >= max( 20, MIN_HASHSIZE ) && \
			  dlpParams->inLen1 <= CRYPT_MAX_HASHSIZE );
	REQUIRES( dlpParams->outParam == NULL && dlpParams->outLen == 0 );
	REQUIRES( decodeDLValuesFunction != NULL );

	/* Decode the values from a DL data block and make sure that r and s are
	   valid, i.e. r, s = [1...q-1] */
	status = decodeDLValuesFunction( dlpParams->inParam2, dlpParams->inLen2, 
									 r, s, q, dlpParams->formatType );
	if( cryptStatusError( status ) )
		return( status );

	/* Convert the hash value to an integer in the proper range */
	status = hashToBignum( u1, dlpParams->inParam1, dlpParams->inLen1, q );
	if( cryptStatusError( status ) )
		return( status );

	/* w = s^-1 mod q */
	CKPTR( BN_mod_inverse( u2, s, q,	/* w = s^-1 mod q */
						   &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* u1 = ( hash * w ) mod q */
	CK( BN_mod_mul( u1, u1, u2, q,		/* u1 = ( hash * w ) mod q */
					&pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* u2 = ( r * w ) mod q */
	CK( BN_mod_mul( u2, r, u2, q,		/* u2 = ( r * w ) mod q */
					&pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* v = ( ( ( g^u1 ) * ( y^u2 ) ) mod p ) mod q */
	CK( BN_mod_exp2_mont( u2, g, u1, y, u2, p, &pkcInfo->bnCTX,
						  &pkcInfo->dlpParam_mont_p ) );
	CK( BN_mod( s, u2, q, &pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* If r == s then the signature is good */
	if( BN_cmp( r, s ) )
		return( CRYPT_ERROR_SIGNATURE );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
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
			  keyLength == sizeof( CRYPT_PKCINFO_DLP ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( ( key == NULL && keyLength == 0 ) || \
			  ( key != NULL && keyLength == sizeof( CRYPT_PKCINFO_DLP ) ) );

#ifndef USE_FIPS140
	/* Load the key component from the external representation into the
	   internal bignums unless we're doing an internal load */
	if( key != NULL )
		{
		PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
		const CRYPT_PKCINFO_DLP *dsaKey = ( CRYPT_PKCINFO_DLP * ) key;
		int status;

		if( dsaKey->isPublicKey )
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY );
		else
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPRIVATEKEY );
		status = importBignum( &pkcInfo->dlpParam_p, dsaKey->p, 
							   bitsToBytes( dsaKey->pLen ),
							   DLPPARAM_MIN_P, DLPPARAM_MAX_P, NULL, 
							   KEYSIZE_CHECK_PKC );
		if( cryptStatusOK( status ) )
			{
			status = importBignum( &pkcInfo->dlpParam_q, dsaKey->q, 
								   bitsToBytes( dsaKey->qLen ),
								   DLPPARAM_MIN_Q, DLPPARAM_MAX_Q, 
								   &pkcInfo->dlpParam_p, 
								   KEYSIZE_CHECK_NONE );
			}
		if( cryptStatusOK( status ) )
			{
			status = importBignum( &pkcInfo->dlpParam_g, dsaKey->g, 
								   bitsToBytes( dsaKey->gLen ),
								   DLPPARAM_MIN_G, DLPPARAM_MAX_G,
								   &pkcInfo->dlpParam_p, 
								   KEYSIZE_CHECK_NONE );
			}
		if( cryptStatusOK( status ) )
			{
			status = importBignum( &pkcInfo->dlpParam_y, dsaKey->y, 
								   bitsToBytes( dsaKey->yLen ),
								   DLPPARAM_MIN_Y, DLPPARAM_MAX_Y,
								   &pkcInfo->dlpParam_p, 
								   KEYSIZE_CHECK_NONE );
			}
		if( cryptStatusOK( status ) && !dsaKey->isPublicKey )
			{
			status = importBignum( &pkcInfo->dlpParam_x, dsaKey->x, 
								   bitsToBytes( dsaKey->xLen ),
								   DLPPARAM_MIN_X, DLPPARAM_MAX_X,
								   &pkcInfo->dlpParam_p, 
								   KEYSIZE_CHECK_NONE );
			}
		SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_PBO );
		if( cryptStatusError( status ) )
			return( status );

		ENSURES( sanityCheckPKCInfo( pkcInfo ) );
		}
#endif /* USE_FIPS140 */

	/* Complete the key checking and setup */
	return( initCheckDLPkey( contextInfoPtr, FALSE ) );
	}

/* Generate a key into an encryption context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateKey( INOUT CONTEXT_INFO *contextInfoPtr, 
						IN_LENGTH_SHORT_MIN( MIN_PKCSIZE * 8 ) \
							const int keySizeBits )
	{
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( keySizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	status = generateDLPkey( contextInfoPtr, ( keySizeBits / 64 ) * 64 );
	if( cryptStatusOK( status ) && \
		!pairwiseConsistencyTest( contextInfoPtr ) )
		{
		DEBUG_DIAG(( "Consistency check of freshly-generated DSA key "
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
	CRYPT_ALGO_DSA, bitsToBytes( 0 ), "DSA", 3,
	MIN_PKCSIZE, bitsToBytes( 1024 ), CRYPT_MAX_PKCSIZE,
	selfTest, getDefaultInfo, NULL, NULL, initKey, generateKey,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	sign, sigCheck
	};

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getDSACapability( void )
	{
	return( &capabilityInfo );
	}

#endif /* USE_DSA */
