/****************************************************************************
*																			*
*					cryptlib PKC_INFO Bignum Support Routines				*
*						Copyright Peter Gutmann 1995-2015					*
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

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Make sure that the metadata for bignums in a PKC_INFO is valid */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckPKCInfo( const PKC_INFO *pkcInfo )
	{
	assert( isReadPtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Check the PKC info metadata */
	if( !CHECK_FLAGS( pkcInfo->flags, PKCINFO_FLAG_NONE, 
					  PKCINFO_FLAG_MAX ) )
		return( FALSE );
	if( pkcInfo->keySizeBits < 0 || \
		pkcInfo->keySizeBits > bytesToBits( CRYPT_MAX_PKCSIZE ) )
		return( FALSE );
	if( pkcInfo->publicKeyInfo == NULL )
		{
		if( pkcInfo->publicKeyInfoSize != 0 )
			return( FALSE );
		}
	else
		{
		if( pkcInfo->publicKeyInfoSize < MIN_CRYPT_OBJECTSIZE || \
			pkcInfo->publicKeyInfoSize >= MAX_INTLENGTH_SHORT )
			return( FALSE );
		}

	/* If it's a dummy context with the keys held in hardware, we're done.
	   In theory we could check for BN_is_zero() for each of the bignums, 
	   but that only looks at the metadata to see if the bignum has a length
	   of zero, and it's not clear what this would achieve */
	if( TEST_FLAG( pkcInfo->flags, PKCINFO_FLAG_DUMMY ) )
		return( TRUE );

	/* Check the PKC info bignums */
	if( !sanityCheckBignum( &pkcInfo->param1 ) || \
		!sanityCheckBignum( &pkcInfo->param2 ) || \
		!sanityCheckBignum( &pkcInfo->param3 ) || \
		!sanityCheckBignum( &pkcInfo->param4 ) || \
		!sanityCheckBignum( &pkcInfo->param5 ) || \
		!sanityCheckBignum( &pkcInfo->param6 ) || \
		!sanityCheckBignum( &pkcInfo->param7 ) || \
		!sanityCheckBignum( &pkcInfo->param8 ) || \
		!sanityCheckBignum( &pkcInfo->blind1 ) || \
		!sanityCheckBignum( &pkcInfo->blind2 ) || \
		!sanityCheckBignum( &pkcInfo->tmp1 ) || \
		!sanityCheckBignum( &pkcInfo->tmp2 ) || \
		!sanityCheckBignum( &pkcInfo->tmp3 ) )
		return( FALSE );
	if( !sanityCheckBNCTX( &pkcInfo->bnCTX ) )
		return( FALSE );
	if( !sanityCheckBNMontCTX( &pkcInfo->montCTX1 ) || \
		!sanityCheckBNMontCTX( &pkcInfo->montCTX2 ) || \
		!sanityCheckBNMontCTX( &pkcInfo->montCTX3 ) )
		return( FALSE );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Bignum Init/Shutdown Routines 						*
*																			*
****************************************************************************/

/* Clear temporary bignum values used during PKC operations */

STDC_NONNULL_ARG( ( 1 ) ) \
void clearTempBignums( INOUT PKC_INFO *pkcInfo )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	BN_clear( &pkcInfo->tmp1 ); BN_clear( &pkcInfo->tmp2 );
	BN_clear( &pkcInfo->tmp3 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( pkcInfo->isECC )
		{
		BN_clear( &pkcInfo->eccParam_tmp4 ); 
		BN_clear( &pkcInfo->eccParam_tmp5 );
		}
#endif /* USE_ECDH || USE_ECDSA */
	BN_CTX_final( &pkcInfo->bnCTX );
	}

/* Initialse and free the bignum information in a context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initContextBignums( INOUT PKC_INFO *pkcInfo, 
						const BOOLEAN isECC )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( isECC == TRUE || isECC == FALSE );

	/* Initialise the overall PKC information */
	memset( pkcInfo, 0, sizeof( PKC_INFO ) );
	INIT_FLAGS( pkcInfo->flags, PKCINFO_FLAG_NONE );

	/* Initialise the bignum information */
	BN_init( &pkcInfo->param1 ); BN_init( &pkcInfo->param2 );
	BN_init( &pkcInfo->param3 ); BN_init( &pkcInfo->param4 );
	BN_init( &pkcInfo->param5 ); BN_init( &pkcInfo->param6 );
	BN_init( &pkcInfo->param7 ); BN_init( &pkcInfo->param8 );
	BN_init( &pkcInfo->blind1 ); BN_init( &pkcInfo->blind2 );
	BN_init( &pkcInfo->tmp1 ); BN_init( &pkcInfo->tmp2 );
	BN_init( &pkcInfo->tmp3 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isECC )
		{
		pkcInfo->isECC = TRUE;
		pkcInfo->ecCTX = EC_GROUP_new( EC_GFp_simple_method() );
		pkcInfo->ecPoint = EC_POINT_new( pkcInfo->ecCTX );
		pkcInfo->tmpPoint = EC_POINT_new( pkcInfo->ecCTX );
		if( pkcInfo->ecCTX == NULL || pkcInfo->ecPoint == NULL || \
			pkcInfo->tmpPoint == NULL )
			{
			if( pkcInfo->tmpPoint != NULL )
				EC_POINT_free( pkcInfo->tmpPoint );
			if( pkcInfo->ecPoint != NULL )
				EC_POINT_free( pkcInfo->ecPoint );
			if( pkcInfo->ecCTX != NULL )
				EC_GROUP_free( pkcInfo->ecCTX );
			return( CRYPT_ERROR_MEMORY );
			}
		}
#endif /* USE_ECDH || USE_ECDSA */
	BN_CTX_init( &pkcInfo->bnCTX );
	BN_MONT_CTX_init( &pkcInfo->montCTX1 );
	BN_MONT_CTX_init( &pkcInfo->montCTX2 );
	BN_MONT_CTX_init( &pkcInfo->montCTX3 );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void endContextBignums( INOUT PKC_INFO *pkcInfo, 
						IN const BOOLEAN isDummyContext )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( sanityCheckPKCInfo( pkcInfo ) );
			/* We don't make this an ENSURES since that would prevent 
			   clearing the data.  Since the cleanup is just a series of 
			   memset()s of fixed-sized data it doesn't matter if a field 
			   is in an inconsistent state */

	REQUIRES_V( isDummyContext == TRUE || isDummyContext == FALSE );

	if( !isDummyContext )
		{
		BN_clear( &pkcInfo->param1 ); BN_clear( &pkcInfo->param2 );
		BN_clear( &pkcInfo->param3 ); BN_clear( &pkcInfo->param4 );
		BN_clear( &pkcInfo->param5 ); BN_clear( &pkcInfo->param6 );
		BN_clear( &pkcInfo->param7 ); BN_clear( &pkcInfo->param8 );
		BN_clear( &pkcInfo->blind1 ); BN_clear( &pkcInfo->blind2 );
		BN_clear( &pkcInfo->tmp1 ); BN_clear( &pkcInfo->tmp2 );
		BN_clear( &pkcInfo->tmp3 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
		if( pkcInfo->isECC )
			{
			EC_POINT_free( pkcInfo->tmpPoint );
			EC_POINT_free( pkcInfo->ecPoint );
			EC_GROUP_free( pkcInfo->ecCTX );
			}
#endif /* USE_ECDH || USE_ECDSA */
		BN_CTX_final( &pkcInfo->bnCTX );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
		if( !pkcInfo->isECC )
#endif /* USE_ECDH || USE_ECDSA */
			{
			BN_MONT_CTX_free( &pkcInfo->montCTX1 );
			BN_MONT_CTX_free( &pkcInfo->montCTX2 );
			BN_MONT_CTX_free( &pkcInfo->montCTX3 );
			}
		}
	if( pkcInfo->publicKeyInfo != NULL )
		clFree( "freeContextBignums", pkcInfo->publicKeyInfo );
	}

/****************************************************************************
*																			*
*							Bignum Checksum Routines 						*
*																			*
****************************************************************************/

/* Checksum a bignum's metadata and its data payload.  We can't use the 
   standard checksumData() here both because we need to keep a running total 
   of the existing checksum value and because we don't want to truncate the 
   checksum value to 16 bits at the end of each calculation because it's fed 
   to the next round of checksumming */

CHECK_RETVAL_RANGE_NOERROR( 0, INT_MAX ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checksumBignumData( IN_BUFFER( length ) const void *data, 
							   IN_LENGTH_SHORT const int length,
							   IN const int initialSum )
	{
	const unsigned char *dataPtr = data;
	int sum1 = 0, sum2 = initialSum, i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( data, length ) );

	REQUIRES_EXT( isShortIntegerRangeNZ( length ), 0 );

	LOOP_MAX( i = 0, i < length, i++ )
		{
		sum1 += dataPtr[ i ];
		sum2 += sum1;
		}
	ENSURES_EXT( LOOP_BOUND_OK, 0 );
	return( sum2 );
	}

#define BN_checksum( bignum, checksum ) \
	*( checksum ) = checksumBignumData( bignum, sizeof( BIGNUM ), *( checksum ) )
#define BN_checksum_montgomery( montCTX, checksum ) \
	*( checksum ) = checksumBignumData( montCTX, sizeof( BN_MONT_CTX ), *( checksum ) )

#if defined( USE_ECDH ) || defined( USE_ECDSA )

/* Checksum the ECC structure metadata and their data payloads */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void BN_checksum_ec_group( IN const EC_GROUP *group, 
								  INOUT int *checksum )
	{
	int value = *checksum;

	assert( isReadPtr( group, sizeof( EC_GROUP ) ) );
	assert( isWritePtr( checksum, sizeof( int ) ) );

	/* Checksum the EC_GROUP metadata */
	value = checksumBignumData( group, sizeof( EC_GROUP ), value );

	/* EC_GROUPs have an incredibly complex inner stucture (see 
	   bn/ec_lcl.h), the following checksums the common data without getting 
	   into the lists of method-specific data values */
	BN_checksum( &group->order, &value );
	BN_checksum( &group->cofactor, &value );
	BN_checksum( &group->field, &value );
	BN_checksum( &group->a, &value );
	BN_checksum( &group->b, &value );
	
	*checksum = value;
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void BN_checksum_ec_point( IN const EC_POINT *point, 
								  INOUT int *checksum )
	{
	int value = *checksum;

	assert( isReadPtr( point, sizeof( EC_POINT ) ) );
	assert( isWritePtr( checksum, sizeof( int ) ) );

	/* Checksum the EC_POINT metadata */
	value = checksumBignumData( point, sizeof( EC_POINT ), value );

	/* Checksum the EC_POINT data */
	BN_checksum( &point->X, &value );
	BN_checksum( &point->Y, &value );
	BN_checksum( &point->Z, &value );

	*checksum = value;
	}
#endif /* USE_ECDH || USE_ECDSA */

/* Calculate a bignum checksum */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int bignumChecksum( INOUT PKC_INFO *pkcInfo, 
						   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
						   const BOOLEAN isPrivateKey,
						   OUT int *checksum )
	{
	int value = 0;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isWritePtr( checksum, sizeof( int ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );
	REQUIRES( isPrivateKey == TRUE || isPrivateKey == FALSE );

	/* Clear return value */
	*checksum = 0;

	/* Calculate the key data checksum */
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isEccAlgo( cryptAlgo ) )
		{
		BN_checksum( &pkcInfo->eccParam_qx, &value );
		BN_checksum( &pkcInfo->eccParam_qy, &value );
		if( isPrivateKey )
			BN_checksum( &pkcInfo->eccParam_d, &value );
		BN_checksum_ec_group( pkcInfo->ecCTX, &value );
		BN_checksum_ec_point( pkcInfo->ecPoint, &value );
		}
	else
#endif /* USE_ECDH || USE_ECDSA */
	if( isDlpAlgo( cryptAlgo ) )
		{
		BN_checksum( &pkcInfo->dlpParam_p, &value );
		BN_checksum( &pkcInfo->dlpParam_g, &value );
		BN_checksum( &pkcInfo->dlpParam_q, &value );
		BN_checksum( &pkcInfo->dlpParam_y, &value );
		if( cryptAlgo == CRYPT_ALGO_DH )
			BN_checksum( &pkcInfo->dhParam_yPrime, &value );
		if( isPrivateKey )
			BN_checksum( &pkcInfo->dlpParam_x, &value );
		BN_checksum_montgomery( &pkcInfo->dlpParam_mont_p, &value );
		}
	else
		{
		/* Note that we don't checksum the (optional) blinding values 
		   rsaParam_blind_k and rsaParam_blind_kInv, since these are updated
		   on every RSA operation so the checksum would change every time 
		   they're used.  Since these are random values, a fault will only
		   make them even more random */
		BN_checksum( &pkcInfo->rsaParam_n, &value );
		BN_checksum( &pkcInfo->rsaParam_e, &value );
		BN_checksum_montgomery( &pkcInfo->rsaParam_mont_n, &value );
		if( isPrivateKey )
			{
			BN_checksum( &pkcInfo->rsaParam_d, &value );
			BN_checksum( &pkcInfo->rsaParam_p, &value );
			BN_checksum( &pkcInfo->rsaParam_q, &value );
			BN_checksum( &pkcInfo->rsaParam_u, &value );
			BN_checksum( &pkcInfo->rsaParam_exponent1, &value );
			BN_checksum( &pkcInfo->rsaParam_exponent2, &value );
			BN_checksum_montgomery( &pkcInfo->rsaParam_mont_p, &value );
			BN_checksum_montgomery( &pkcInfo->rsaParam_mont_q, &value );
			}
		}
	*checksum = value;

	return( CRYPT_OK ); 
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checksumContextData( INOUT PKC_INFO *pkcInfo, 
						 IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
						 const BOOLEAN isPrivateKey )
	{
	int checksum, status;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );
	REQUIRES( isPrivateKey == TRUE || isPrivateKey == FALSE );

	/* Set or update the data checksum */
	status = bignumChecksum( pkcInfo, cryptAlgo, isPrivateKey, &checksum );
	ENSURES( cryptStatusOK( status ) );
	if( pkcInfo->checksum == 0L )
		pkcInfo->checksum = checksum;
	else
		{
		if( pkcInfo->checksum != checksum )
			return( CRYPT_ERROR );
		}

	/* Check static domain parameters if required */
	if( pkcInfo->domainParams != NULL )
		{
#if defined( USE_ECDH ) || defined( USE_ECDSA )
		if( !checksumDomainParameters( pkcInfo->domainParams, pkcInfo->isECC ) )
			return( CRYPT_ERROR );
#else
		if( !checksumDomainParameters( pkcInfo->domainParams, FALSE ) )
			return( CRYPT_ERROR );
#endif /* USE_ECDH || USE_ECDSA */
		}

	return( CRYPT_OK );
	}

/* Checksum a static bignum, which is one where the bignum image is stored
   in read-only memory.  This is used for things like DLP and ECDLP domain
   parameters, for which there's no need to parse and process them into 
   dynamically-allocated RAM space.

   We can't use the standard Fletcher checksum for this because it performs
   poorly on data with long strings of repeated bits, particularly all-zero
   and all-one words, which occur in both the DLP and ECDLP domain 
   parameters.  What we need is a function with good avalanche, of which
   MurmurHash seems to be the best tradeoff between effectiveness in
   defeating bit flips and the like and speed.

   The principal alternative is CityHash (and its follow-on FarmHash), but 
   that's incredibly complex (FarmHash is 12K LOC, do have a cow), and in 
   any case CityHash only exists as 64- and 128-bit variants, there's no 
   32-bit version of the hash.

   The following code is based on the MurmurHash3 implementation by Austin 
   Appleby, who placed it in the public domain.  It's built around a core
   that takes the input word and pre-mixes it using multiplies by two 
   constants and a rotate (thus the Murmur name, multiply-and-rotate), xors 
   the result into the hash value, and them mixes the hash using a rotate 
   and multiply-accumulate:

	k *= c1;
	k = rotl( k, r1 );
	k *= c2;

	h ^= k;

	h = rotl( h, r1 );
	h = h * m1 + n1; */

#ifdef _MSC_VER
  #define ROTL32( x, r )	_rotl( x, r )
  #define ROTL64( x, r )	_rotl64( x, r )
#else
  /* Most compilers have rotate-recognisers, this is the form that will 
     produce a native rotate instruction */
  #define ROTL32( x, r )	( ( ( x ) << ( r ) ) | ( ( x ) >> ( 32 - ( r ) ) ) )
  #define ROTL64( x, r )	( ( ( x ) << ( r ) ) | ( ( x ) >> ( 64 - ( r ) ) ) )
#endif /* _MSC_VER */

#if BN_BITS2 == 64

#define CONST_1		0x87C37B91114253D5ULL
#define CONST_2		0x4CF5AD432745937FULL

static BN_ULONG fmix64( BN_ULONG k )
	{
	/* The constants were generated by Austin Appleby using a simulated-
	   annealing algorithm, and will avalanche all bits of 'h' to within 
	   a 0.25% bias */ 
	k ^= k >> 33;
	k *= 0xFF51AFD7ED558CCDULL;
	k ^= k >> 33;
	k *= 0xC4CEB9FE1A85EC53ULL;
	k ^= k >> 33;

	return( k );
	}

static BN_ULONG MurmurHash3( const BN_ULONG *data, const int len, BN_ULONG seed )
	{
	BN_ULONG h1 = seed, h2 = seed;
	const int nblocks = ( len + 1 ) / 2;
	int i, LOOP_ITERATOR;

	assert( isReadPtr( data, len * sizeof( BN_ULONG ) ) );

	ENSURES_B( len > 0 && len <= BIGNUM_ALLOC_WORDS_EXT2 );

	/* Process each input word, two words at a time */
	LOOP_EXT( i = 0, i < nblocks, i += 2, BIGNUM_ALLOC_WORDS_EXT2 )
		{
		BN_ULONG k1 = data[ i ];
		BN_ULONG k2 = data[ i + 1 ];

		k1 *= CONST_1; 
		k1 = ROTL64( k1, 31 ); 
		k1 *= CONST_2; 
		h1 ^= k1;

		h1 = ROTL64( h1, 27 ); 
		h1 += h2; 
		h1 = ( h1 * 5 ) + 0x52DCE729;

		k2 *= CONST_2; 
		k2 = ROTL64( k2, 33 ); 
		k2 *= CONST_1; 
		h2 ^= k2;

		h2 = ROTL64( h2, 31 ); 
		h2 += h1; 
		h2 = ( h2 * 5 ) + 0x38495AB5;
		}
	ENSURES_B( LOOP_BOUND_OK );

	/* Finalisation and avalanche */
	h1 ^= len; 
	h2 ^= len;
	h1 += h2;
	h2 += h1;
	h1 = fmix64( h1 );
	h2 = fmix64( h2 );
	h1 += h2;
/*	h2 += h1; Not used for 64-bit result */

	return( h1 );
	}
#else

#define CONST_1		0xCC9E2D51UL
#define CONST_2		0x1B873593UL

static BN_ULONG MurmurHash3( const BN_ULONG *data, const int len, BN_ULONG seed )
	{
	BN_ULONG h1 = seed;
	int i, LOOP_ITERATOR;

	assert( isReadPtr( data, len * sizeof( BN_ULONG ) ) );

	ENSURES_B( len > 0 && len <= BIGNUM_ALLOC_WORDS_EXT2 );

	/* Process each input word */
	LOOP_EXT( i = 0, i < len, i++, BIGNUM_ALLOC_WORDS_EXT2 )
		{
		BN_ULONG k1 = data[ i ];

		k1 *= CONST_1;
		k1 = ROTL32( k1, 15 );
		k1 *= CONST_2;
    
		h1 ^= k1;
		h1 = ROTL32( h1,13 ); 
		h1 = ( h1 * 5 ) + 0xE6546B64UL;
		}
	ENSURES_B( LOOP_BOUND_OK );

	/* Finalisation and avalanche using the fmix32() version of the 64-bit 
	   fmix64(), see the comment on that for how it works */
	h1 ^= len;
	h1 ^= h1 >> 16;
	h1 *= 0x85EBCA6BUL;
	h1 ^= h1 >> 13;
	h1 *= 0xC2B2AE35UL;
	h1 ^= h1 >> 16;

	return( h1 );
	} 
#endif /* 64- vs 32-bit */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checksumStaticBignum( const BIGNUM *bignum,
									 const BN_ULONG checksum )
	{
	BN_ULONG hash;
	int i, LOOP_ITERATOR;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	/* Make sure that the metadata fields are valid.  These have fixed 
	   values so we just check that they contain the values that they're
	   supposed to */
	if( bignum->neg != FALSE || \
		bignum->flags != BN_FLG_STATIC_DATA )
		return( FALSE );
	if( bignum->top <= 0 || bignum->top > BIGNUM_ALLOC_WORDS )
		return( FALSE );

	/* Check that the data matches the checksum */
	hash = MurmurHash3( bignum->d, bignum->top, 0 );
	if( hash != checksum )
		return( FALSE );

	/* Finally, make sure that the rest of the bignum data is all zeroes */
	LOOP_EXT( i = bignum->top, i < BIGNUM_ALLOC_WORDS, i++,
			  BIGNUM_ALLOC_WORDS )
		{
		if( bignum->d[ i ] != 0 )
			return( FALSE );
		}
	ENSURES_B( LOOP_BOUND_OK );

	return( TRUE );
	}

/* Print a bignum checksum, used when hardcoding constant values like DLP 
   and ECDLP domain parameters from their bignum representations */

#if defined( DEBUG_DIAGNOSTIC_ENABLE ) && !defined( NDEBUG )

void printBignumChecksum( const BIGNUM *bignum )
	{
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

#if BN_BITS2 == 64
	DEBUG_PRINT(( "0x%016llXULL, ", 
				  MurmurHash3( bignum->d, bignum->top, 0 ) ));
#else
	DEBUG_PRINT(( "0x%08X, ", 
				  MurmurHash3( bignum->d, bignum->top, 0 ) ));
#endif /* 64- vs 32-bit */
	}
#endif /* DEBUG_DIAGNOSTIC_ENABLE && Debug */

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checksumECCParameters( const ECC_DOMAINPARAMS *domainParams )
	{
	assert( isReadPtr( domainParams, sizeof( DH_DOMAINPARAMS ) ) );

	if( !checksumStaticBignum( &domainParams->p, domainParams->p_checksum ) || \
		!checksumStaticBignum( &domainParams->a, domainParams->a_checksum ) || \
		!checksumStaticBignum( &domainParams->b, domainParams->b_checksum ) || \
		!checksumStaticBignum( &domainParams->gx, domainParams->gx_checksum ) || \
		!checksumStaticBignum( &domainParams->gy, domainParams->gy_checksum ) || \
		!checksumStaticBignum( &domainParams->n, domainParams->n_checksum ) || \
		!checksumStaticBignum( &domainParams->h, domainParams->h_checksum ) )
		return( FALSE );

	return( TRUE );
	}
#endif /* USE_ECDH || USE_ECDSA */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checksumDHParameters( const DH_DOMAINPARAMS *domainParams )
	{
	assert( isReadPtr( domainParams, sizeof( DH_DOMAINPARAMS ) ) );

	if( !checksumStaticBignum( &domainParams->p, domainParams->p_checksum ) || \
		!checksumStaticBignum( &domainParams->q, domainParams->q_checksum ) || \
		!checksumStaticBignum( &domainParams->g, domainParams->g_checksum ) )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checksumDomainParameters( const void *domainParams, 
								  const BOOLEAN isECC )
	{
	REQUIRES( isECC == TRUE || isECC == FALSE );

#if defined( USE_ECDH ) || defined( USE_ECDSA )
	return( isECC ? checksumECCParameters( domainParams ) : \
					checksumDHParameters( domainParams ) );
#else
	return( checksumDHParameters( domainParams ) );
#endif /* USE_ECDH || USE_ECDSA */
	}
#else

/****************************************************************************
*																			*
*								Non-Bignum Stubs 							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void clearTempBignums( INOUT PKC_INFO *pkcInfo )
	{
	}
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initContextBignums( INOUT PKC_INFO *pkcInfo, 
						const BOOLEAN isECC )
	{
	}
STDC_NONNULL_ARG( ( 1 ) ) \
void freeContextBignums( INOUT PKC_INFO *pkcInfo, 
						 IN_FLAGS( CONTEXT ) const int contextFlags )
	{
	}
#endif /* USE_PKC */
