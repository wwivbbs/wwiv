/****************************************************************************
*																			*
*						cryptlib ECDH Key Exchange Routines					*
*						Copyright Peter Gutmann 2006-2008					*
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

/* The ECDH key exchange process is somewhat complex because there are two 
   phases involved for both sides, an "export" and an "import" phase, and 
   they have to be performed in the correct order.  The sequence of 
   operations is:

	A.load:		set p, g from fixed or external values
				x(A) = rand, x s.t. 0 < x < q-1

	A.export	y(A) = g^x(A) mod p		error if y != 0 at start
				output = y(A)

	B.load		read p, g / set p, g from external values
				x(B) = rand, x s.t. 0 < x < q-1

	B.import	y(A) = input
				z = y(A)^x(B) mod p

	B.export	y(B) = g^x(B) mod p		error if y != 0 at start
				output = y(B)

	A.import	y(B) = input
				z = y(B)^x(A) mod p

   Note that we have to set x when we load p and g because otherwise we'd 
   have to set x(A) on export and x(B) on import, which is tricky since the 
   ECDH code doesn't know whether it's working with A or B */

#ifdef USE_ECDH

/****************************************************************************
*																			*
*								Algorithm Self-test							*
*																			*
****************************************************************************/

/* Perform a pairwise consistency test on a public/private key pair */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN pairwiseConsistencyTest( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	CONTEXT_INFO checkContextInfo;
	PKC_INFO contextData, *pkcInfo = &contextData;
	PKC_INFO *sourcePkcInfo = contextInfoPtr->ctxPKC;
	KEYAGREE_PARAMS keyAgreeParams1, keyAgreeParams2;
	const CAPABILITY_INFO *capabilityInfoPtr;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* The ECDH pairwise check is a bit more complex than the one for the 
	   other algorithms because there's no matched public/private key pair, 
	   so we have to load a second ECDH key to use for key agreement with 
	   the first one */
	status = staticInitContext( &checkContextInfo, CONTEXT_PKC, 
								getECDHCapability(), &contextData, 
								sizeof( PKC_INFO ), NULL );
	if( cryptStatusError( status ) )
		return( FALSE );
	pkcInfo->curveType = CRYPT_ECCCURVE_P256;
	CKPTR( BN_copy( &pkcInfo->eccParam_qx, &sourcePkcInfo->eccParam_qx ) );
	CKPTR( BN_copy( &pkcInfo->eccParam_qy, &sourcePkcInfo->eccParam_qy ) );
	CKPTR( BN_copy( &pkcInfo->eccParam_d, &sourcePkcInfo->eccParam_d ) );
	if( bnStatusError( bnStatus ) )
		{
		staticDestroyContext( &checkContextInfo );
		return( getBnStatus( bnStatus ) );
		}

	/* Perform the pairwise test using the check key */
	capabilityInfoPtr = checkContextInfo.capabilityInfo;
	memset( &keyAgreeParams1, 0, sizeof( KEYAGREE_PARAMS ) );
	memset( &keyAgreeParams2, 0, sizeof( KEYAGREE_PARAMS ) );
	status = capabilityInfoPtr->initKeyFunction( &checkContextInfo, NULL, 0 );
	if( cryptStatusOK( status ) )
		status = capabilityInfoPtr->encryptFunction( contextInfoPtr,
					( BYTE * ) &keyAgreeParams1, sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusOK( status ) )
		status = capabilityInfoPtr->encryptFunction( &checkContextInfo,
					( BYTE * ) &keyAgreeParams2, sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusOK( status ) )
		status = capabilityInfoPtr->decryptFunction( contextInfoPtr,
					( BYTE * ) &keyAgreeParams2, sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusOK( status ) )
		status = capabilityInfoPtr->decryptFunction( &checkContextInfo,
					( BYTE * ) &keyAgreeParams1, sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusError( status ) || \
		keyAgreeParams1.wrappedKeyLen != keyAgreeParams2.wrappedKeyLen || \
		memcmp( keyAgreeParams1.wrappedKey, keyAgreeParams2.wrappedKey, 
				keyAgreeParams1.wrappedKeyLen ) )
		status = CRYPT_ERROR_FAILED;

	/* Clean up */
	staticDestroyContext( &checkContextInfo );

	return( cryptStatusOK( status ) ? TRUE : FALSE );
	}

#ifndef CONFIG_NO_SELFTEST

/* Test the ECDH implementation (re-)using the test key for the ECDSA test, 
   which comes from from X9.62-2005 section L.4.2.  Because a lot of the 
   high-level encryption routines don't exist yet, we cheat a bit and 
   set up a dummy encryption context with just enough information for the 
   following code to work */

typedef struct {
	const int qxLen; const BYTE qx[ 32 ];
	const int qyLen; const BYTE qy[ 32 ];
	const int dLen; const BYTE d[ 32 ];
	} ECC_KEY;

static const FAR_BSS ECC_KEY ecdhTestKey = {
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
	int status;

	/* Initialise the key components */
	status = staticInitContext( &contextInfo, CONTEXT_PKC, 
								getECDHCapability(), &contextData, 
								sizeof( PKC_INFO ), NULL );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_FAILED );
	pkcInfo->curveType = CRYPT_ECCCURVE_P256;
	status = importBignum( &pkcInfo->eccParam_qx, ecdhTestKey.qx, 
						   ecdhTestKey.qxLen, ECCPARAM_MIN_QX, 
						   ECCPARAM_MAX_QX, NULL, KEYSIZE_CHECK_ECC );
	if( cryptStatusOK( status ) ) 
		status = importBignum( &pkcInfo->eccParam_qy, ecdhTestKey.qy, 
							   ecdhTestKey.qyLen, ECCPARAM_MIN_QY, 
							   ECCPARAM_MAX_QY, NULL, KEYSIZE_CHECK_NONE );
	if( cryptStatusOK( status ) ) 
		status = importBignum( &pkcInfo->eccParam_d, ecdhTestKey.d, 
							   ecdhTestKey.dLen, ECCPARAM_MIN_D, 
							   ECCPARAM_MAX_D, NULL, KEYSIZE_CHECK_NONE );
	if( cryptStatusError( status ) ) 
		{
		staticDestroyContext( &contextInfo );
		retIntError();
		}

	/* Perform the test key exchange on a block of data */
	status = contextInfo.capabilityInfo->initKeyFunction( &contextInfo,  NULL, 0 );
	if( cryptStatusOK( status ) && \
		!pairwiseConsistencyTest( &contextInfo ) )
		status = CRYPT_ERROR_FAILED;

	/* Clean up */
	staticDestroyContext( &contextInfo );

	return( status );
	}
#else
	#define selfTest	NULL
#endif /* !CONFIG_NO_SELFTEST */

/****************************************************************************
*																			*
*							ECDH Key Exchange Routines						*
*																			*
****************************************************************************/

/* Perform phase 1 of ECDH ("export").  We have to append the distinguisher 
   'Fn' to the name since some systems already have 'encrypt' and 'decrypt' 
   in their standard headers */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptFn( INOUT CONTEXT_INFO *contextInfoPtr, 
					  INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					  IN_LENGTH_FIXED( sizeof( KEYAGREE_PARAMS ) ) int noBytes )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	KEYAGREE_PARAMS *keyAgreeParams = ( KEYAGREE_PARAMS * ) buffer;
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( keyAgreeParams, sizeof( KEYAGREE_PARAMS ) ) );

	REQUIRES( noBytes == sizeof( KEYAGREE_PARAMS ) );
	REQUIRES( !BN_is_zero( &pkcInfo->eccParam_qx ) && \
			  !BN_is_zero( &pkcInfo->eccParam_qy ) );

	/* Q is generated either at keygen time for static ECDH or as a side-
	   effect of the implicit generation of the d value for ephemeral ECDH, 
	   so all we have to do is encode it in X9.62 point form to the output */
	status = exportECCPoint( keyAgreeParams->publicValue, CRYPT_MAX_PKCSIZE,
							 &keyAgreeParams->publicValueLen, 
							 &pkcInfo->eccParam_qx, &pkcInfo->eccParam_qy,
							 bitsToBytes( pkcInfo->keySizeBits ) );
	if( cryptStatusError( status ) )
		return( status );

	/* Perform side-channel attack checks if necessary */
	if( ( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) && \
		cryptStatusError( calculateBignumChecksum( pkcInfo, 
												   CRYPT_ALGO_ECDH ) ) )
		{
		return( CRYPT_ERROR_FAILED );
		}
	return( CRYPT_OK );
	}

/* Perform phase 2 of ECDH ("import") */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int decryptFn( INOUT CONTEXT_INFO *contextInfoPtr, 
					  INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					  IN_LENGTH_FIXED( sizeof( KEYAGREE_PARAMS ) ) int noBytes )
	{
	KEYAGREE_PARAMS *keyAgreeParams = ( KEYAGREE_PARAMS * ) buffer;
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	EC_GROUP *ecCTX = pkcInfo->ecCTX;
	EC_POINT *q = pkcInfo->tmpPoint;
	BIGNUM *x = &pkcInfo->tmp1, *y = &pkcInfo->tmp2;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( keyAgreeParams, sizeof( KEYAGREE_PARAMS ) ) );
	assert( isReadPtr( keyAgreeParams->publicValue, 
					   keyAgreeParams->publicValueLen ) );

	REQUIRES( noBytes == sizeof( KEYAGREE_PARAMS ) );
	REQUIRES( keyAgreeParams->publicValueLen >= MIN_PKCSIZE_ECCPOINT && \
			  keyAgreeParams->publicValueLen < MAX_INTLENGTH_SHORT );

	/* The other party's Q value will be stored with the key agreement info 
	   in X9.62 point form rather than having been read in when we read the 
	   ECDH public key */
	status = importECCPoint( &pkcInfo->eccParam_qx, &pkcInfo->eccParam_qy,
							 keyAgreeParams->publicValue,
							 keyAgreeParams->publicValueLen,
							 MIN_PKCSIZE_ECC_THRESHOLD, 
							 CRYPT_MAX_PKCSIZE_ECC, 
							 bitsToBytes( pkcInfo->keySizeBits ),
							 &pkcInfo->eccParam_p, KEYSIZE_CHECK_ECC );
	if( cryptStatusError( status ) )
		return( status );

	/* Fill in point structure with coordinates from Q. */
	CK( EC_POINT_set_affine_coordinates_GFp( ecCTX, q,
											 &pkcInfo->eccParam_qx,
											 &pkcInfo->eccParam_qy,
											 pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Multiply Q by private key d. */
	CK( EC_POINT_mul( ecCTX, q, NULL, q, &pkcInfo->eccParam_d,
					  pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Extract affine coordinates. */
	CK( EC_POINT_get_affine_coordinates_GFp( ecCTX, q, x, y, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* Encode the point.  This gets rather ugly because the only two 
	   protocols that use ECDH, SSL and SSH, both use only the x coordinate
	   rather than the complete point value.  In the interests of 
	   consistency we return the full point, so it's up to the users of the
	   ECDH capability to extract any sub-components that they need.  This
	   isn't so hard to do because of the use of the fixed-length
	   uncompressed format */
	status = exportECCPoint( keyAgreeParams->wrappedKey, CRYPT_MAX_PKCSIZE, 
							 &keyAgreeParams->wrappedKeyLen, x, y,
							 bitsToBytes( pkcInfo->keySizeBits ) );
	if( cryptStatusError( status ) )
		return( status );
	
	/* Perform side-channel attack checks if necessary */
	if( ( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) && \
		cryptStatusError( calculateBignumChecksum( pkcInfo, 
												   CRYPT_ALGO_ECDH ) ) )
		{
		return( CRYPT_ERROR_FAILED );
		}
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
			( isReadPtr( key, keyLength ) && \
			  keyLength == sizeof( CRYPT_PKCINFO_ECC ) ) );

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

		contextInfoPtr->flags |= ( eccKey->isPublicKey ) ? \
							CONTEXT_FLAG_ISPUBLICKEY : CONTEXT_FLAG_ISPRIVATEKEY;
		if( eccKey->curveType == CRYPT_ECCCURVE_NONE )
			{
			status = importBignum( &pkcInfo->eccParam_p, eccKey->p, 
								   bitsToBytes( eccKey->pLen ), 
								   ECCPARAM_MIN_P, ECCPARAM_MAX_P, 
								   NULL, KEYSIZE_CHECK_ECC );
			if( cryptStatusOK( status ) )
				status = importBignum( &pkcInfo->eccParam_a, eccKey->a, 
									   bitsToBytes( eccKey->aLen ), 
									   ECCPARAM_MIN_A, ECCPARAM_MAX_A, 
									   NULL, KEYSIZE_CHECK_NONE );
			if( cryptStatusOK( status ) )
				status = importBignum( &pkcInfo->eccParam_b, eccKey->b, 
									   bitsToBytes( eccKey->bLen ), 
									   ECCPARAM_MIN_B, ECCPARAM_MAX_B, 
									   NULL, KEYSIZE_CHECK_NONE );
			if( cryptStatusOK( status ) )
				status = importBignum( &pkcInfo->eccParam_gx, eccKey->gx, 
									   bitsToBytes( eccKey->gxLen ), 
									   ECCPARAM_MIN_GX, ECCPARAM_MAX_GX, 
									   NULL, KEYSIZE_CHECK_NONE );
			if( cryptStatusOK( status ) )
				status = importBignum( &pkcInfo->eccParam_gy, eccKey->gy, 
									   bitsToBytes( eccKey->gyLen ), 
									   ECCPARAM_MIN_GY, ECCPARAM_MAX_GY, 
									   NULL, KEYSIZE_CHECK_NONE );
			if( cryptStatusOK( status ) )
				status = importBignum( &pkcInfo->eccParam_n, eccKey->n, 
									   bitsToBytes( eccKey->nLen ), 
									   ECCPARAM_MIN_N, ECCPARAM_MAX_N, 
									   NULL, KEYSIZE_CHECK_NONE );
			if( cryptStatusError( status ) )
				return( status );
			}
		else
			{
			if( eccKey->curveType <= CRYPT_ECCCURVE_NONE || \
				eccKey->curveType >= CRYPT_ECCCURVE_LAST )
				return( CRYPT_ARGERROR_STR1 );
			pkcInfo->curveType = eccKey->curveType;
			}
		status = importBignum( &pkcInfo->eccParam_qx, eccKey->qx, 
							   bitsToBytes( eccKey->qxLen ), 
							   ECCPARAM_MIN_QX, ECCPARAM_MAX_QX, 
							   NULL, KEYSIZE_CHECK_NONE );
		if( cryptStatusOK( status ) )
			status = importBignum( &pkcInfo->eccParam_qy, eccKey->qy, 
								   bitsToBytes( eccKey->qyLen ), 
								   ECCPARAM_MIN_QY, ECCPARAM_MAX_QY, 
								   NULL, KEYSIZE_CHECK_NONE );
		if( cryptStatusOK( status ) && !eccKey->isPublicKey )
			status = importBignum( &pkcInfo->eccParam_d, eccKey->d, 
								   bitsToBytes( eccKey->dLen ), 
								   ECCPARAM_MIN_D, ECCPARAM_MAX_D, 
								   NULL, KEYSIZE_CHECK_NONE );
		contextInfoPtr->flags |= CONTEXT_FLAG_PBO;
		if( cryptStatusError( status ) )
			return( status );
		}
#endif /* USE_FIPS140 */

	/* Complete the key checking and setup */
	return( initCheckECCkey( contextInfoPtr, TRUE ) );
	}

/* Generate a key into an encryption context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateKey( INOUT CONTEXT_INFO *contextInfoPtr, 
						IN_LENGTH_SHORT_MIN( MIN_PKCSIZE_ECC * 8 ) \
							const int keySizeBits )
	{
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( keySizeBits >= bytesToBits( MIN_PKCSIZE_ECC ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE_ECC ) );

	status = generateECCkey( contextInfoPtr, keySizeBits );
	if( cryptStatusOK( status ) &&
#ifndef USE_FIPS140
		( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) &&
#endif /* USE_FIPS140 */
		!pairwiseConsistencyTest( contextInfoPtr ) )
		{
		DEBUG_DIAG(( "Consistency check of freshly-generated ECDH key "
					 "failed" ));
		assert( DEBUG_WARN );
		status = CRYPT_ERROR_FAILED;
		}
	return( status );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO FAR_BSS capabilityInfo = {
	CRYPT_ALGO_ECDH, bitsToBytes( 0 ), "ECDH", 4,
	MIN_PKCSIZE_ECC, bitsToBytes( 256 ), CRYPT_MAX_PKCSIZE_ECC,
	selfTest, getDefaultInfo, NULL, NULL, initKey, generateKey, encryptFn, decryptFn
	};

const CAPABILITY_INFO *getECDHCapability( void )
	{
	return( &capabilityInfo );
	}

#endif /* USE_ECDH */
