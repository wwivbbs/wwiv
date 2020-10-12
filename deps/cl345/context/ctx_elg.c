/****************************************************************************
*																			*
*					  cryptlib Elgamal Encryption Routines					*
*						Copyright Peter Gutmann 1997-2005					*
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

#ifdef USE_ELGAMAL

/****************************************************************************
*																			*
*								Algorithm Self-test							*
*																			*
****************************************************************************/

/* If we're doing a self-test we use the following fixed k (for the
   signature) and kRandom (for the encryption) data rather than a randomly-
   generated value.  The k value is the 160-bit DSA one from FIPS 186, which 
   seems as good as any */

#if 0	/* Only needed for Elgamal signing */

static const BYTE kVal[] = {
	0x35, 0x8D, 0xAD, 0x57, 0x14, 0x62, 0x71, 0x0F,
	0x50, 0xE2, 0x54, 0xCF, 0x1A, 0x37, 0x6B, 0x2B,
	0xDE, 0xAA, 0xDF, 0xBF
	};
#endif /* 0 */

static const BYTE kRandomVal[] = {
	0x2A, 0x7C, 0x01, 0xFD, 0x62, 0xF7, 0x43, 0x13,
	0x36, 0xFE, 0xE8, 0xF1, 0x68, 0xB2, 0xA2, 0x2F,
	0x76, 0x50, 0xA1, 0x2C, 0x3E, 0x64, 0x8E, 0xFE,
	0x04, 0x58, 0x7F, 0xDE, 0xC2, 0x34, 0xE5, 0x79,
	0xE9, 0x45, 0xB0, 0xDD, 0x5E, 0x56, 0xD7, 0x82,
	0xEF, 0x93, 0xEF, 0x5F, 0xD0, 0x71, 0x8B, 0xA1,
	0x3E, 0xA0, 0x55, 0x6A, 0xB9, 0x6E, 0x72, 0xFE,
	0x17, 0x03, 0x95, 0x50, 0xB7, 0xA1, 0x11, 0xBA,
	};

/* Perform a pairwise consistency test on a public/private key pair */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN pairwiseConsistencyTest( INOUT CONTEXT_INFO *contextInfoPtr,
										const BOOLEAN isGeneratedKey )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	DLP_PARAMS dlpParams;
	BYTE buffer[ ( CRYPT_MAX_PKCSIZE * 2 ) + 32 + 8 ];
	int encrSize, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES_B( sanityCheckContext( contextInfoPtr ) );
	REQUIRES_B( isGeneratedKey == TRUE || isGeneratedKey == FALSE );
	REQUIRES_B( capabilityInfoPtr != NULL );

	/* Encrypt with the public key.  We  */
	memset( buffer, 0, CRYPT_MAX_PKCSIZE );
	memcpy( buffer + 1, "abcde", 5 );
	setDLPParams( &dlpParams, buffer,
				  bitsToBytes( contextInfoPtr->ctxPKC->keySizeBits ),
				  buffer, ( CRYPT_MAX_PKCSIZE * 2 ) + 32 );
	if( !isGeneratedKey )
		{
		/* Force the use of a fixed k value for the encryption test to
		   avoid having to go via the RNG */
		dlpParams.inLen2 = -999;
		}
	status = capabilityInfoPtr->encryptFunction( contextInfoPtr,
						( BYTE * ) &dlpParams, sizeof( DLP_PARAMS ) );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Decrypt with the private key */
	encrSize = dlpParams.outLen;
	setDLPParams( &dlpParams, buffer, encrSize,
				  buffer, ( CRYPT_MAX_PKCSIZE * 2 ) + 32 );
	status = capabilityInfoPtr->decryptFunction( contextInfoPtr,
						( BYTE * ) &dlpParams, sizeof( DLP_PARAMS ) );
	if( cryptStatusError( status ) )
		return( FALSE );
	return( !memcmp( buffer + 1, "abcde", 5 ) );
	}

#ifndef CONFIG_NO_SELFTEST

/* Test the Elgamal implementation using a sample key.  Because a lot of the 
   high-level encryption routines don't exist yet, we cheat a bit and set up 
   a dummy encryption context with just enough information for the following 
   code to work */

typedef struct {
	const int pLen; const BYTE p[ 128 ];
	const int qLen; const BYTE q[ 20 ];
	const int gLen; const BYTE g[ 128 ];
	const int xLen; const BYTE x[ 20 ];
	const int yLen; const BYTE y[ 128 ];
	} DLP_KEY;

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
								getElgamalCapability(), &contextData, 
								sizeof( PKC_INFO ), NULL );
	if( cryptStatusError( status ) )
		return( status );
	status = importBignum( &pkcInfo->dlpParam_p, dlpTestKey.p, 
						   dlpTestKey.pLen, DLPPARAM_MIN_P, 
						   DLPPARAM_MAX_P, NULL, KEYSIZE_CHECK_PKC );
	if( cryptStatusOK( status ) )
		{
		status = importBignum( &pkcInfo->dlpParam_g, dlpTestKey.g, 
							   dlpTestKey.gLen, DLPPARAM_MIN_G, 
							   DLPPARAM_MAX_G, &pkcInfo->dlpParam_p, 
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		status = importBignum( &pkcInfo->dlpParam_q, dlpTestKey.q, 
							   dlpTestKey.qLen, DLPPARAM_MIN_Q, 
							   DLPPARAM_MAX_Q, &pkcInfo->dlpParam_p,
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

	/* Perform a test a sig generation/check and test en/decryption */
#if 0	/* See comment in sig.code */
	memset( buffer, '*', 20 );
	status = capabilityInfoPtr->signFunction( &contextInfoPtr, buffer, -1 );
	if( !cryptStatusError( status ) )
		{
		memmove( buffer + 20, buffer, status );
		memset( buffer, '*', 20 );
		status = capabilityInfoPtr->sigCheckFunction( &contextInfoPtr,
													  buffer, 20 + status );
		}
	if( status != CRYPT_OK )
		status = CRYPT_ERROR_FAILED;
#endif /* 0 */
	status = capabilityInfoPtr->initKeyFunction( &contextInfo, NULL, 0 );
	if( cryptStatusError( status ) || \
		!pairwiseConsistencyTest( &contextInfo, FALSE ) )
		{
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}

	/* The checking for memory faults is performed at the 
	   MESSAGE_CTX_ENCRYPT level, so it won't be detected when we call the
	   function directly via an internal code pointer */
#if 0
	DLP_PARAMS dlpParams;
	BYTE buffer[ ( CRYPT_MAX_PKCSIZE * 2 ) + 32 + 8 ];

	/* Finally, make sure that the memory fault-detection is working */
	pkcInfo->dlpParam_p.d[ 8 ] ^= 0x0011;
	memset( buffer, 0, CRYPT_MAX_PKCSIZE );
	memcpy( buffer + 1, "abcde", 5 );
	setDLPParams( &dlpParams, buffer,
				  bitsToBytes( contextInfo.ctxPKC->keySizeBits ),
				  buffer, ( CRYPT_MAX_PKCSIZE * 2 ) + 32 );
	status = capabilityInfoPtr->encryptFunction( &contextInfo,
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
	pkcInfo->dlpParam_p.d[ 8 ] ^= 0x0011;
	status = checksumContextData( pkcInfo, CRYPT_ALGO_ELGAMAL, TRUE );
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

/* Elgamal signatures have potential security problems (although this isn't
   an issue when they're used in a PKCS #1 manner, OTOH nothing apart from
   cryptlib uses them like this) while the equivalent DSA signatures don't
   (or at least have less than Elgamal).  In addition since nothing uses
   them anyway this code, we don't support Elgamal signing */

/****************************************************************************
*																			*
*						Encrypt/Decrypt a Data Block						*
*																			*
****************************************************************************/

/* Encrypt a single block of data.  We have to append the distinguisher 'Fn'
   to the name since some systems already have 'encrypt' and 'decrypt' in
   their standard headers */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptFn( INOUT CONTEXT_INFO *contextInfoPtr, 
					  INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					  IN_LENGTH_FIXED( sizeof( DLP_PARAMS ) ) int noBytes )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) buffer;
	const PKC_ENCODEDLVALUES_FUNCTION encodeDLValuesFunction = \
					( PKC_ENCODEDLVALUES_FUNCTION ) \
					FNPTR_GET( pkcInfo->encodeDLValuesFunction );
	BIGNUM *p = &pkcInfo->dlpParam_p, *g = &pkcInfo->dlpParam_g;
	BIGNUM *y = &pkcInfo->dlpParam_y;
	BIGNUM *tmp = &pkcInfo->tmp1, *k = &pkcInfo->tmp2;
	BIGNUM *r = &pkcInfo->tmp3, *s = &pkcInfo->dlpTmp1;
	BIGNUM *phi_p = &pkcInfo->dlpTmp2;
	const int length = bitsToBytes( pkcInfo->keySizeBits );
	int i, bnStatus = BN_STATUS, status, LOOP_ITERATOR;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( dlpParams, sizeof( DLP_PARAMS ) ) );
	assert( isReadPtrDynamic( dlpParams->inParam1, dlpParams->inLen1 ) );
	assert( isWritePtrDynamic( dlpParams->outParam, dlpParams->outLen ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( noBytes == sizeof( DLP_PARAMS ) );
	REQUIRES( dlpParams->inLen1 == length );
	REQUIRES( dlpParams->inParam2 == NULL && \
			  ( dlpParams->inLen2 == 0 || dlpParams->inLen2 == -999 ) );
	REQUIRES( dlpParams->outLen >= ( 2 + length ) * 2 && \
			  dlpParams->outLen < MAX_INTLENGTH_SHORT );
	REQUIRES( encodeDLValuesFunction != NULL );

	/* Make sure that we're not being fed suspiciously short data 
	   quantities.  importBignum() performs a more rigorous check, but we
	   use this as a lint filter before performing the relatively expensive
	   random bignum generation and preprocessing */
	LOOP_LARGE( i = 0, i < length, i++ )
		{
		if( ( ( BYTE * ) dlpParams->inParam1 ) [ i ] != 0 )
			break;
		}
	ENSURES( LOOP_BOUND_OK );
	if( length - i < MIN_PKCSIZE - 8 )
		return( CRYPT_ERROR_BADDATA );

	/* Generate the secret random value k.  During the initial self-test
	   the random data pool may not exist yet, and may in fact never exist in
	   a satisfactory condition if there isn't enough randomness present in
	   the system to generate cryptographically strong random numbers.  To
	   bypass this problem, if the caller passes in a second length parameter
	   of -999, we know that it's an internal self-test call and use a fixed
	   bit pattern for k that avoids having to call generateBignum().  This
	   is a somewhat ugly use of 'magic numbers', but it's safe because this
	   function can only be called internally, so all we need to trap is
	   accidental use of the parameter which is normally unused */
	if( dlpParams->inLen2 == -999 )
		{
		status = importBignum( k, ( BYTE * ) kRandomVal, length, 
							   length - 1, length, NULL, KEYSIZE_CHECK_NONE );
		}
	else
		{
		/* Generate the random value k, with the same 32-bit adjustment used
		   in the DSA code to avoid bias in the output (the only real
		   difference is that we eventually reduce it mode phi(p) rather than
		   mod q).

		   We also add (meaning "mix in" rather than strictly 
		   "arithmetically add") the data being encrypted to curtail 
		   problems in the incredibly unlikely situation that the RNG value 
		   repeats */
		status = generateBignumEx( k, 
								   bytesToBits( length + DLP_OVERFLOW_SIZE ), 
								   0x80, 0, dlpParams->inParam1, 
								   dlpParams->inLen1, NULL );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Generate phi( p ) and use it to get k, k < p-1 and k relatively prime
	   to p-1.  Since (p-1)/2 is prime, the initial choice for k will be
	   divisible by (p-1)/2 with probability 2/(p-1), so we'll do at most two
	   gcd operations with very high probability.  A k of (p-3)/2 will be
	   chosen with probability 3/(p-1), and all other numbers from 1 to p-1
	   will be chosen with probability 2/(p-1), giving a nearly uniform
	   distribution of exponents */
	CKPTR( BN_copy( phi_p, p ) );
	CK( BN_sub_word( phi_p, 1 ) );		/* phi( p ) = p - 1 */
	CK( BN_mod( k, k, phi_p,			/* Reduce k to the correct range */
				&pkcInfo->bnCTX ) );
	CK( BN_gcd( s, k, phi_p, &pkcInfo->bnCTX ) );
	LOOP_SMALL_CHECK( bnStatusOK( bnStatus ) && !BN_is_one( s ) )
		{
		CK( BN_sub_word( k, 1 ) );
		CK( BN_gcd( s, k, phi_p, &pkcInfo->bnCTX ) );
		}
	ENSURES( LOOP_BOUND_OK );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Move the input data into a bignum */
	status = importBignum( tmp, ( BYTE * ) dlpParams->inParam1, length,
						   MIN_PKCSIZE - 8, CRYPT_MAX_PKCSIZE, p, 
						   KEYSIZE_CHECK_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/* s = ( y^k * M ) mod p */
	CK( BN_mod_exp_mont( r, y, k, p,	/* y' = y^k mod p */
						 &pkcInfo->bnCTX, &pkcInfo->dlpParam_mont_p ) );
	CK( BN_mod_mul( s, r, tmp, p,		/* s = y'M mod p */
					&pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* r = g^k mod p */
	CK( BN_mod_exp_mont( r, g, k, p, &pkcInfo->bnCTX,
						 &pkcInfo->dlpParam_mont_p ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Encode the result as a DL data block */
	status = encodeDLValuesFunction( dlpParams->outParam, dlpParams->outLen, 
									 &dlpParams->outLen, r, s, 
									 dlpParams->formatType );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

/* Decrypt a single block of data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int decryptFn( INOUT CONTEXT_INFO *contextInfoPtr, 
					  INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					  IN_LENGTH_FIXED( sizeof( DLP_PARAMS ) ) int noBytes )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) buffer;
	const PKC_DECODEDLVALUES_FUNCTION decodeDLValuesFunction = \
					( PKC_DECODEDLVALUES_FUNCTION ) \
					FNPTR_GET( pkcInfo->decodeDLValuesFunction );
	BIGNUM *p = &pkcInfo->dlpParam_p, *x = &pkcInfo->dlpParam_x;
	BIGNUM *r = &pkcInfo->tmp1, *s = &pkcInfo->tmp2, *tmp = &pkcInfo->tmp3;
	const int length = bitsToBytes( pkcInfo->keySizeBits );
	int offset, dummy, bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( dlpParams, sizeof( DLP_PARAMS ) ) );
	assert( isReadPtrDynamic( dlpParams->inParam1, dlpParams->inLen1 ) );
	assert( isWritePtrDynamic( dlpParams->outParam, dlpParams->outLen ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( noBytes == sizeof( DLP_PARAMS ) );
	REQUIRES( dlpParams->inLen1 >= ( 2 + ( length - 2 ) ) * 2 && \
			  dlpParams->inLen1 < MAX_INTLENGTH_SHORT );
	REQUIRES( dlpParams->inParam2 == NULL && dlpParams->inLen2 == 0 );
	REQUIRES( dlpParams->outLen >= length && \
			  dlpParams->outLen < MAX_INTLENGTH_SHORT );
	REQUIRES( decodeDLValuesFunction != NULL );

	/* Decode the values from a DL data block and make sure that r and s are
	   valid, i.e. r, s = [1...p-1] */
	status = decodeDLValuesFunction( dlpParams->inParam1, dlpParams->inLen1, 
									 r, s, p, dlpParams->formatType );
	if( cryptStatusError( status ) )
		return( status );

	/* M = ( s / ( r^x ) ) mod p */
	CK( BN_mod_exp_mont( r, r, x, p,		/* r' = r^x */
						 &pkcInfo->bnCTX, &pkcInfo->dlpParam_mont_p ) );
	CKPTR( BN_mod_inverse( tmp, r, p,		/* r'' = r'^-1 */
						   &pkcInfo->bnCTX ) );
	CK( BN_mod_mul( s, s, tmp, p,			/* s = s * r'^-1 mod p */
					&pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Copy the result to the output.  Since the bignum code performs
	   leading-zero truncation, we have to adjust where we copy the
	   result to in the buffer to take into account extra zero bytes
	   that aren't extracted from the bignum.  In addition we can't use
	   the length returned from exportBignum() because this is the length 
	   of the zero-truncated result, not the full length */
	offset = length - BN_num_bytes( s );
	ENSURES( offset >= 0 && offset <= length );
	if( offset > 0 )
		{
		/* If the resulting value has more than 128 bits of leading zeroes
		   then there's something wrong */
		if( offset > 16 )
			return( CRYPT_ERROR_BADDATA );
		memset( dlpParams->outParam, 0, offset );
		}
	dlpParams->outLen = length;
	status = exportBignum( dlpParams->outParam + offset, 
						   dlpParams->outLen - offset, &dummy, s );
	if( cryptStatusError( status ) )
		return( status );

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
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;

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
		const CRYPT_PKCINFO_DLP *egKey = ( CRYPT_PKCINFO_DLP * ) key;
		int status;

		/* Load the key components into the bignums */
		if( egKey->isPublicKey )
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY );
		else
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPRIVATEKEY );
		status = importBignum( &pkcInfo->dlpParam_p, egKey->p, 
							   bitsToBytes( egKey->pLen ),
							   DLPPARAM_MIN_P, DLPPARAM_MAX_P, NULL, 
							   KEYSIZE_CHECK_PKC );
		if( cryptStatusOK( status ) )
			{
			status = importBignum( &pkcInfo->dlpParam_g, egKey->g, 
								   bitsToBytes( egKey->gLen ),
								   DLPPARAM_MIN_G, DLPPARAM_MAX_G,
								   &pkcInfo->dlpParam_p, 
								   KEYSIZE_CHECK_NONE );
			}
		if( cryptStatusOK( status ) )
			{
			status = importBignum( &pkcInfo->dlpParam_q, egKey->q, 
								   bitsToBytes( egKey->qLen ),
								   DLPPARAM_MIN_Q, DLPPARAM_MAX_Q,
								   &pkcInfo->dlpParam_p, 
								   KEYSIZE_CHECK_NONE );
			}
		if( cryptStatusOK( status ) )
			{
			status = importBignum( &pkcInfo->dlpParam_y, egKey->y, 
								   bitsToBytes( egKey->yLen ),
								   DLPPARAM_MIN_Y, DLPPARAM_MAX_Y,
								   &pkcInfo->dlpParam_p, 
								   KEYSIZE_CHECK_NONE );
			}
		if( cryptStatusOK( status ) && !egKey->isPublicKey )
			{
			status = importBignum( &pkcInfo->dlpParam_x, egKey->x, 
								   bitsToBytes( egKey->xLen ),
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

	/* Complete the key checking and setup.  PGP Elgamal keys don't follow 
	   X9.42 and are effectively PKCS #3 keys so if the key is being 
	   instantiated from PGP key data and doesn't have a q parameter we mark 
	   it as a PKCS #3 key to ensure that it doesn't fail the validity check 
	   for q != 0 */
	if( key == NULL && \
		TEST_FLAG( contextInfoPtr->ctxPKC->flags, 
				   PKCINFO_FLAG_OPENPGPKEYID_SET ) && \
		BN_is_zero( &pkcInfo->dlpParam_q ) )
		{
		/* It's a PGP Elgamal key, treat it as a PKCS #3 key for checking
		   purposes */
		return( initCheckDLPkey( contextInfoPtr, TRUE ) );
		}
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

	status = generateDLPkey( contextInfoPtr, keySizeBits );
	if( cryptStatusOK( status ) &&
#ifndef USE_FIPS140
		TEST_FLAG( contextInfoPtr->flags, 
				   CONTEXT_FLAG_SIDECHANNELPROTECTION ) &&
#endif /* USE_FIPS140 */
		!pairwiseConsistencyTest( contextInfoPtr, TRUE ) )
		{
		DEBUG_DIAG(( "Consistency check of freshly-generated Elgamal key "
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
	CRYPT_ALGO_ELGAMAL, bitsToBytes( 0 ), "Elgamal", 7,
	MIN_PKCSIZE, bitsToBytes( 1536 ), CRYPT_MAX_PKCSIZE,
	selfTest, getDefaultInfo, NULL, NULL, initKey, generateKey, 
	encryptFn, decryptFn
	};

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getElgamalCapability( void )
	{
	return( &capabilityInfo );
	}

#endif /* USE_ELGAMAL */
