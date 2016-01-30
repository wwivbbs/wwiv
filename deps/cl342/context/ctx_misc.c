/****************************************************************************
*																			*
*						cryptlib Context Support Routines					*
*						Copyright Peter Gutmann 1995-2011					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #ifdef USE_MD5
	#include "md5.h"
  #endif /* USE_MD5 */
  #ifdef USE_RIPEMD160
	#include "ripemd.h"
  #endif /* USE_RIPEMD160 */
  #include "sha.h"
  #ifdef USE_SHA2
	#include "sha2.h"
  #endif /* USE_SHA2 */
#else
  #include "crypt.h"
  #include "context/context.h"
  #ifdef USE_MD5
	#include "crypt/md5.h"
  #endif /* USE_MD5 */
  #ifdef USE_RIPEMD160
	#include "crypt/ripemd.h"
  #endif /* USE_RIPEMD160 */
  #include "crypt/sha.h"
  #ifdef USE_SHA2
	#include "crypt/sha2.h"
  #endif /* USE_SHA2 */
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						Capability Management Functions						*
*																			*
****************************************************************************/

/* Check that a capability info record is consistent */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckCapability( const CAPABILITY_INFO *capabilityInfoPtr,
							   const BOOLEAN asymmetricOK )
	{
	CRYPT_ALGO_TYPE cryptAlgo = capabilityInfoPtr->cryptAlgo;

	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );

	/* Check the algorithm and mode parameters.  We check for an algorithm
	   name one shorter than the maximum because as returned to an external
	   caller it's an ASCIZ string so we need to allow room for the
	   terminator */
	if( cryptAlgo <= CRYPT_ALGO_NONE || cryptAlgo >= CRYPT_ALGO_LAST || \
		capabilityInfoPtr->algoName == NULL || \
		capabilityInfoPtr->algoNameLen < 3 || \
		capabilityInfoPtr->algoNameLen > CRYPT_MAX_TEXTSIZE - 1 )
		return( FALSE );

	/* Make sure that the minimum functions are present.  We don't check for
	   the presence of the keygen function since the symmetric capabilities
	   use the generic keygen and the hash capabilities don't do keygen at 
	   all */
	if( capabilityInfoPtr->selfTestFunction == NULL || \
		capabilityInfoPtr->getInfoFunction == NULL )
		return( FALSE );
	if( isStreamCipher( cryptAlgo ) )
		{
		if( capabilityInfoPtr->encryptOFBFunction == NULL || \
			capabilityInfoPtr->decryptOFBFunction == NULL )
			return( FALSE );
		}
	else
		{
		if( asymmetricOK )
			{
			/* If asymmetric capabilities (e.g. decrypt but not encrypt,
			   present in some tinkertoy tokens) are permitted then we only 
			   check that there's at least one useful capability available */
			if( capabilityInfoPtr->decryptFunction == NULL && \
				capabilityInfoPtr->signFunction == NULL )
				return( FALSE );
			}
		else
			{
			if( !isSpecialAlgo( cryptAlgo ) )
				{
				/* We need at least one mechanism pair to be able to do 
				   anything useful with the capability */
				if( ( capabilityInfoPtr->encryptFunction == NULL || \
					  capabilityInfoPtr->decryptFunction == NULL ) && \
					( capabilityInfoPtr->encryptCBCFunction == NULL || \
					  capabilityInfoPtr->decryptCBCFunction == NULL ) && \
					( capabilityInfoPtr->encryptCFBFunction == NULL || \
					  capabilityInfoPtr->decryptCFBFunction == NULL ) && \
					( capabilityInfoPtr->encryptOFBFunction == NULL || \
					  capabilityInfoPtr->decryptOFBFunction == NULL ) && \
					( capabilityInfoPtr->encryptGCMFunction == NULL || \
					  capabilityInfoPtr->decryptGCMFunction == NULL ) && \
					( capabilityInfoPtr->signFunction == NULL || \
					  capabilityInfoPtr->sigCheckFunction == NULL ) )
					return( FALSE );
				}
			}
		}

	/* Make sure that the algorithm/mode-specific parameters are
	   consistent */
	if( capabilityInfoPtr->minKeySize > capabilityInfoPtr->keySize || \
		capabilityInfoPtr->maxKeySize < capabilityInfoPtr->keySize )
		return( FALSE );
	if( isConvAlgo( cryptAlgo ) )
		{
		if( ( capabilityInfoPtr->blockSize < bitsToBytes( 8 ) || \
        	  capabilityInfoPtr->blockSize > CRYPT_MAX_IVSIZE ) || \
			( capabilityInfoPtr->minKeySize < MIN_KEYSIZE || \
			  capabilityInfoPtr->maxKeySize > CRYPT_MAX_KEYSIZE ) )
			return( FALSE );
		if( capabilityInfoPtr->keySize > MAX_WORKING_KEYSIZE )
			return( FALSE );	/* Requirement for key wrap */
		if( capabilityInfoPtr->initParamsFunction == NULL || \
			capabilityInfoPtr->initKeyFunction == NULL )
			return( FALSE );
		if( !isStreamCipher( cryptAlgo ) && \
			 capabilityInfoPtr->blockSize < bitsToBytes( 64 ) )
			return( FALSE );
		if( ( capabilityInfoPtr->encryptCBCFunction != NULL && \
			  capabilityInfoPtr->decryptCBCFunction == NULL ) || \
			( capabilityInfoPtr->encryptCBCFunction == NULL && \
			  capabilityInfoPtr->decryptCBCFunction != NULL ) )
			return( FALSE );
		if( ( capabilityInfoPtr->encryptCFBFunction != NULL && \
			  capabilityInfoPtr->decryptCFBFunction == NULL ) || \
			( capabilityInfoPtr->encryptCFBFunction == NULL && \
			  capabilityInfoPtr->decryptCFBFunction != NULL ) )
			return( FALSE );
		if( ( capabilityInfoPtr->encryptOFBFunction != NULL && \
			  capabilityInfoPtr->decryptOFBFunction == NULL ) || \
			( capabilityInfoPtr->encryptOFBFunction == NULL && \
			  capabilityInfoPtr->decryptOFBFunction != NULL ) )
			return( FALSE );
		if( ( capabilityInfoPtr->encryptGCMFunction != NULL && \
			  capabilityInfoPtr->decryptGCMFunction == NULL ) || \
			( capabilityInfoPtr->encryptGCMFunction == NULL && \
			  capabilityInfoPtr->decryptGCMFunction != NULL ) )
			return( FALSE );

		return( TRUE );
		}

	/* We've checked the conventional algorithms, beyond this point there
	   shouldn't be any conventional encryption modes present */
	if( capabilityInfoPtr->encryptCBCFunction != NULL || \
		capabilityInfoPtr->decryptCBCFunction != NULL || \
		capabilityInfoPtr->encryptCFBFunction != NULL || \
		capabilityInfoPtr->decryptCFBFunction != NULL || \
		capabilityInfoPtr->encryptOFBFunction != NULL || \
		capabilityInfoPtr->decryptOFBFunction != NULL || \
		capabilityInfoPtr->encryptGCMFunction != NULL || \
		capabilityInfoPtr->decryptGCMFunction != NULL )
		return( FALSE );

	/* Check any remaining algorithm types */
	if( isPkcAlgo( cryptAlgo ) )
		{
		const int minKeySize = isEccAlgo( cryptAlgo ) ? \
							   MIN_PKCSIZE_ECC : MIN_PKCSIZE;

		if( capabilityInfoPtr->blockSize != 0 || \
			( capabilityInfoPtr->minKeySize < minKeySize || \
			  capabilityInfoPtr->maxKeySize > CRYPT_MAX_PKCSIZE ) )
			return( FALSE );
		if( capabilityInfoPtr->initKeyFunction == NULL || \
			capabilityInfoPtr->generateKeyFunction == NULL )
			return( FALSE );

		return( TRUE );
		}
	if( isHashAlgo( cryptAlgo ) )
		{
		if( ( capabilityInfoPtr->blockSize < bitsToBytes( 128 ) || \
			  capabilityInfoPtr->blockSize > CRYPT_MAX_HASHSIZE ) || \
			( capabilityInfoPtr->minKeySize != 0 || \
			  capabilityInfoPtr->keySize != 0 || \
			  capabilityInfoPtr->maxKeySize != 0 ) )
			return( FALSE );

		return( TRUE );
		}
	if( isMacAlgo( cryptAlgo ) )
		{
		if( ( capabilityInfoPtr->blockSize < bitsToBytes( 128 ) || \
			  capabilityInfoPtr->blockSize > CRYPT_MAX_HASHSIZE ) || \
			( capabilityInfoPtr->minKeySize < MIN_KEYSIZE || \
			  capabilityInfoPtr->maxKeySize > CRYPT_MAX_KEYSIZE ) )
			return( FALSE );
		if( capabilityInfoPtr->keySize > MAX_WORKING_KEYSIZE )
			return( FALSE );	/* Requirement for key wrap */
		if( capabilityInfoPtr->initKeyFunction == NULL )
			return( FALSE );

		return( TRUE );
		}
	if( isSpecialAlgo( cryptAlgo ) )
		{
		if( capabilityInfoPtr->blockSize != 0 || \
			capabilityInfoPtr->minKeySize < bitsToBytes( 128 ) || \
			capabilityInfoPtr->maxKeySize > CRYPT_MAX_KEYSIZE )
			return( FALSE );
		if( capabilityInfoPtr->encryptFunction != NULL || \
			capabilityInfoPtr->decryptFunction != NULL )
			return( FALSE );
		if( capabilityInfoPtr->initKeyFunction == NULL )
			return( FALSE );

		return( TRUE );
		}

	retIntError_Boolean();
	}

/* Get information from a capability record */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void getCapabilityInfo( OUT CRYPT_QUERY_INFO *cryptQueryInfo,
						const CAPABILITY_INFO FAR_BSS *capabilityInfoPtr )
	{
	assert( isWritePtr( cryptQueryInfo, sizeof( CRYPT_QUERY_INFO ) ) );
	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );

	memset( cryptQueryInfo, 0, sizeof( CRYPT_QUERY_INFO ) );
	memcpy( cryptQueryInfo->algoName, capabilityInfoPtr->algoName,
			capabilityInfoPtr->algoNameLen );
	cryptQueryInfo->algoName[ capabilityInfoPtr->algoNameLen ] = '\0';
	cryptQueryInfo->blockSize = capabilityInfoPtr->blockSize;
	cryptQueryInfo->minKeySize = capabilityInfoPtr->minKeySize;
	cryptQueryInfo->keySize = capabilityInfoPtr->keySize;
	cryptQueryInfo->maxKeySize = capabilityInfoPtr->maxKeySize;
	}

/* Find the capability record for a given encryption algorithm */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const CAPABILITY_INFO FAR_BSS *findCapabilityInfo(
						const CAPABILITY_INFO_LIST *capabilityInfoList,
						IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	const CAPABILITY_INFO_LIST *capabilityInfoListPtr;
	int iterationCount;

	assert( isReadPtr( capabilityInfoList, sizeof( CAPABILITY_INFO ) ) );

	/* Find the capability corresponding to the requested algorithm/mode */
	for( capabilityInfoListPtr = capabilityInfoList, iterationCount = 0;
		 capabilityInfoListPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_MED;
		 capabilityInfoListPtr = capabilityInfoListPtr->next, iterationCount++ )
		{
		if( capabilityInfoListPtr->info->cryptAlgo == cryptAlgo )
			return( capabilityInfoListPtr->info );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( NULL );
	}

/****************************************************************************
*																			*
*							Shared Context Functions						*
*																			*
****************************************************************************/

/* Default handler to get object subtype-specific information.  This 
   fallback function is called if the object-specific primary get-info 
   handler doesn't want to handle the query */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int getDefaultInfo( IN_ENUM( CAPABILITY_INFO ) const CAPABILITY_INFO_TYPE type, 
					INOUT_OPT CONTEXT_INFO *contextInfoPtr,
					OUT void *data, 
					IN_INT_Z const int length )

	{
	assert( contextInfoPtr == NULL || \
			isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( ( length == 0 && isWritePtr( data, sizeof( int ) ) ) || \
			( length > 0 && isWritePtr( data, length ) ) );

	REQUIRES( type > CAPABILITY_INFO_NONE && type < CAPABILITY_INFO_LAST );

	switch( type )
		{
		case CAPABILITY_INFO_STATESIZE:
			{
			int *valuePtr = ( int * ) data;

			/* If we're falling through to a default handler for this then 
			   it's because the context has no state information */
			*valuePtr = 0;

			return( CRYPT_OK );
			}

		case CAPABILITY_INFO_STATEALIGNTYPE:
			{
			int *valuePtr = ( int * ) data;

			/* Default alignment for the algorithm-specific state data is
			   64 bits */
			*valuePtr = 8;

			return( CRYPT_OK );
			}
		}

	retIntError();
	}

/****************************************************************************
*																			*
*							Bignum Support Routines 						*
*																			*
****************************************************************************/

#ifdef USE_PKC

/* Clear temporary bignum values used during PKC operations */

STDC_NONNULL_ARG( ( 1 ) ) \
void clearTempBignums( INOUT PKC_INFO *pkcInfo )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	BN_clear( &pkcInfo->tmp1 );
	BN_clear( &pkcInfo->tmp2 );
	BN_clear( &pkcInfo->tmp3 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( pkcInfo->isECC )
		{
		BN_clear( &pkcInfo->tmp4 );
		BN_clear( &pkcInfo->tmp5 );
		}
#endif /* USE_ECDH || USE_ECDSA */
	BN_CTX_clear( pkcInfo->bnCTX );
	}

/* Initialse and free the bignum information in a context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initContextBignums( INOUT PKC_INFO *pkcInfo, 
						IN_RANGE( 0, 3 ) const int sideChannelProtectionLevel,
						const BOOLEAN isECC )
	{
	BN_CTX *bnCTX;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( sideChannelProtectionLevel >= 0 && \
			  sideChannelProtectionLevel <= 3 );

	/* Perform any required memory allocations */
	bnCTX = BN_CTX_new();
	if( bnCTX == NULL )
		return( CRYPT_ERROR_MEMORY );

	/* Initialise the bignum information */
	BN_init( &pkcInfo->param1 );
	BN_init( &pkcInfo->param2 );
	BN_init( &pkcInfo->param3 );
	BN_init( &pkcInfo->param4 );
	BN_init( &pkcInfo->param5 );
	BN_init( &pkcInfo->param6 );
	BN_init( &pkcInfo->param7 );
	BN_init( &pkcInfo->param8 );
	if( sideChannelProtectionLevel > 0 )
		{
		BN_init( &pkcInfo->blind1 );
		BN_init( &pkcInfo->blind2 );
		}
	BN_init( &pkcInfo->tmp1 );
	BN_init( &pkcInfo->tmp2 );
	BN_init( &pkcInfo->tmp3 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isECC )
		{
		pkcInfo->isECC = TRUE;
		BN_init( &pkcInfo->tmp4 );
		BN_init( &pkcInfo->tmp5 );
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
			BN_CTX_free( bnCTX );
			return( CRYPT_ERROR_MEMORY );
			}
		}
#endif /* USE_ECDH || USE_ECDSA */
	pkcInfo->bnCTX = bnCTX;
	BN_MONT_CTX_init( &pkcInfo->montCTX1 );
	BN_MONT_CTX_init( &pkcInfo->montCTX2 );
	BN_MONT_CTX_init( &pkcInfo->montCTX3 );

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void freeContextBignums( INOUT PKC_INFO *pkcInfo, 
						 IN_FLAGS( CONTEXT ) const int contextFlags )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES_V( contextFlags >= CONTEXT_FLAG_NONE && \
				contextFlags <= CONTEXT_FLAG_MAX );

	if( !( contextFlags & CONTEXT_FLAG_DUMMY ) )
		{
		BN_clear_free( &pkcInfo->param1 );
		BN_clear_free( &pkcInfo->param2 );
		BN_clear_free( &pkcInfo->param3 );
		BN_clear_free( &pkcInfo->param4 );
		BN_clear_free( &pkcInfo->param5 );
		BN_clear_free( &pkcInfo->param6 );
		BN_clear_free( &pkcInfo->param7 );
		BN_clear_free( &pkcInfo->param8 );
		if( contextFlags & CONTEXT_FLAG_SIDECHANNELPROTECTION )
			{
			BN_clear_free( &pkcInfo->blind1 );
			BN_clear_free( &pkcInfo->blind2 );
			}
		BN_clear_free( &pkcInfo->tmp1 );
		BN_clear_free( &pkcInfo->tmp2 );
		BN_clear_free( &pkcInfo->tmp3 );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
		if( pkcInfo->isECC )
			{
			BN_clear_free( &pkcInfo->tmp4 );
			BN_clear_free( &pkcInfo->tmp5 );
			EC_POINT_free( pkcInfo->tmpPoint );
			EC_POINT_free( pkcInfo->ecPoint );
			EC_GROUP_free( pkcInfo->ecCTX );
			}
#endif /* USE_ECDH || USE_ECDSA */
		BN_MONT_CTX_free( &pkcInfo->montCTX1 );
		BN_MONT_CTX_free( &pkcInfo->montCTX2 );
		BN_MONT_CTX_free( &pkcInfo->montCTX3 );
		BN_CTX_free( pkcInfo->bnCTX );
		}
	if( pkcInfo->publicKeyInfo != NULL )
		clFree( "contextMessageFunction", pkcInfo->publicKeyInfo );
	}

/* Calculate a bignum checksum */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int metadataChecksum( INOUT PKC_INFO *pkcInfo, 
							 IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
							 OUT_INT_Z int *checksum )
	{
	int value = 0;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* Clear return value */
	*checksum = 0;

	/* Calculate the key data checksum */
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isEccAlgo( cryptAlgo ) )
		{
		BN_checksum_metadata( &pkcInfo->eccParam_p, &value );
		BN_checksum_metadata( &pkcInfo->eccParam_a, &value );
		BN_checksum_metadata( &pkcInfo->eccParam_b, &value );
		BN_checksum_metadata( &pkcInfo->eccParam_gx, &value );
		BN_checksum_metadata( &pkcInfo->eccParam_gy, &value );
		BN_checksum_metadata( &pkcInfo->eccParam_n, &value );
		BN_checksum_metadata( &pkcInfo->eccParam_h, &value );
		BN_checksum_metadata( &pkcInfo->eccParam_qx, &value );
		BN_checksum_metadata( &pkcInfo->eccParam_qy, &value );
		BN_checksum_metadata( &pkcInfo->eccParam_d, &value );
		BN_checksum_montgomery_metadata( &pkcInfo->eccParam_mont_p, &value );
		BN_checksum_montgomery_metadata( &pkcInfo->eccParam_mont_n, &value );
		BN_checksum_ec_group_metadata( pkcInfo->ecCTX, &value );
		BN_checksum_ec_point_metadata( pkcInfo->ecPoint, &value );
		}
	else
#endif /* USE_ECDH || USE_ECDSA */
	if( isDlpAlgo( cryptAlgo ) )
		{
		BN_checksum_metadata( &pkcInfo->dlpParam_p, &value );
		BN_checksum_metadata( &pkcInfo->dlpParam_g, &value );
		BN_checksum_metadata( &pkcInfo->dlpParam_q, &value );
		BN_checksum_metadata( &pkcInfo->dlpParam_y, &value );
		BN_checksum_metadata( &pkcInfo->dlpParam_x, &value );
		BN_checksum_montgomery_metadata( &pkcInfo->dlpParam_mont_p, &value );
		}
	else
		{
		BN_checksum_metadata( &pkcInfo->rsaParam_n, &value );
		BN_checksum_metadata( &pkcInfo->rsaParam_e, &value );
		BN_checksum_metadata( &pkcInfo->rsaParam_d, &value );
		BN_checksum_metadata( &pkcInfo->rsaParam_p, &value );
		BN_checksum_metadata( &pkcInfo->rsaParam_q, &value );
		BN_checksum_metadata( &pkcInfo->rsaParam_u, &value );
		BN_checksum_metadata( &pkcInfo->rsaParam_exponent1, &value );
		BN_checksum_metadata( &pkcInfo->rsaParam_exponent2, &value );
		BN_checksum_montgomery_metadata( &pkcInfo->rsaParam_mont_n, &value );
		BN_checksum_montgomery_metadata( &pkcInfo->rsaParam_mont_p, &value );
		BN_checksum_montgomery_metadata( &pkcInfo->rsaParam_mont_q, &value );
		}
	*checksum = value;

	return( CRYPT_OK ); 
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int bignumChecksum( INOUT PKC_INFO *pkcInfo, 
						   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
						   OUT_INT_Z int *checksum )
	{
	int value = 0;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* Clear return value */
	*checksum = 0;

	/* Calculate the key data checksum */
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isEccAlgo( cryptAlgo ) )
		{
		BN_checksum( &pkcInfo->eccParam_p, &value );
		BN_checksum( &pkcInfo->eccParam_a, &value );
		BN_checksum( &pkcInfo->eccParam_b, &value );
		BN_checksum( &pkcInfo->eccParam_gx, &value );
		BN_checksum( &pkcInfo->eccParam_gy, &value );
		BN_checksum( &pkcInfo->eccParam_n, &value );
		BN_checksum( &pkcInfo->eccParam_h, &value );
		BN_checksum( &pkcInfo->eccParam_qx, &value );
		BN_checksum( &pkcInfo->eccParam_qy, &value );
		BN_checksum( &pkcInfo->eccParam_d, &value );
		BN_checksum_montgomery( &pkcInfo->eccParam_mont_p, &value );
		BN_checksum_montgomery( &pkcInfo->eccParam_mont_n, &value );
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
		BN_checksum( &pkcInfo->dlpParam_x, &value );
		BN_checksum_montgomery( &pkcInfo->dlpParam_mont_p, &value );
		}
	else
		{
		BN_checksum( &pkcInfo->rsaParam_n, &value );
		BN_checksum( &pkcInfo->rsaParam_e, &value );
		BN_checksum( &pkcInfo->rsaParam_d, &value );
		BN_checksum( &pkcInfo->rsaParam_p, &value );
		BN_checksum( &pkcInfo->rsaParam_q, &value );
		BN_checksum( &pkcInfo->rsaParam_u, &value );
		BN_checksum( &pkcInfo->rsaParam_exponent1, &value );
		BN_checksum( &pkcInfo->rsaParam_exponent2, &value );
		BN_checksum_montgomery( &pkcInfo->rsaParam_mont_n, &value );
		BN_checksum_montgomery( &pkcInfo->rsaParam_mont_p, &value );
		BN_checksum_montgomery( &pkcInfo->rsaParam_mont_q, &value );
		}
	*checksum = value;

	return( CRYPT_OK ); 
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int calculateBignumChecksum( INOUT PKC_INFO *pkcInfo, 
							 IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	int checksum, status;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* Set or update the metadata checksum */
	status = metadataChecksum( pkcInfo, cryptAlgo, &checksum );
	ENSURES( cryptStatusOK( status ) );
	if( pkcInfo->metadataChecksum == 0L )
		pkcInfo->metadataChecksum = checksum;
	else
		{
		if( pkcInfo->metadataChecksum != checksum )
			return( CRYPT_ERROR );
		}

	/* Set or update the data checksum */
	status = bignumChecksum( pkcInfo, cryptAlgo, &checksum );
	ENSURES( cryptStatusOK( status ) );
	if( pkcInfo->checksum == 0L )
		pkcInfo->checksum = checksum;
	else
		{
		if( pkcInfo->checksum != checksum )
			return( CRYPT_ERROR );
		}

	return( CRYPT_OK );
	}

/* Convert a byte string to and from a BIGNUM value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkBignum( const BIGNUM *bignum,
						IN_LENGTH_PKC const int minLength, 
						IN_LENGTH_PKC const int maxLength, 
						IN_OPT const BIGNUM *maxRange,
						IN_ENUM_OPT( KEYSIZE_CHECK ) \
							const KEYSIZE_CHECK_TYPE checkType )
	{
	BN_ULONG bnWord;
	int bignumLength;

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );
	assert( maxRange == NULL || isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES( minLength > 0 && minLength <= maxLength && \
			  maxLength <= CRYPT_MAX_PKCSIZE + \
						   ( ( checkType == KEYSIZE_CHECK_SPECIAL ) ? \
							   DLP_OVERFLOW_SIZE : 0 ) );
	REQUIRES( checkType >= KEYSIZE_CHECK_NONE && \
			  checkType < KEYSIZE_CHECK_LAST_SPECIAL );

	/* The following should never happen because BN_bin2bn() works with 
	   unsigned values but we perform the check anyway just in case someone 
	   messes with the underlying bignum code */
	ENSURES( !( BN_is_negative( bignum ) ) )

	/* A zero- or one-valued bignum on the other hand is an error because we 
	   should never find zero or one in a PKC-related value.  This check is 
	   somewhat redundant with the one that follows, we place it here to 
	   make it explicit and because the cost is near zero */
	bnWord = BN_get_word( bignum );
	if( bnWord < BN_MASK2 && bnWord <= 1 )
		return( CRYPT_ERROR_BADDATA );

	/* Check that the bignum value falls within the allowed length range.  
	   Before we do the general length check we perform a more specific 
	   check for the case where the length is below the minimum allowed but 
	   still looks at least vaguely valid, in which case we report it as a 
	   too-short key rather than a bad data error */
	bignumLength = BN_num_bytes( bignum );
	switch( checkType )
		{
		case KEYSIZE_CHECK_NONE:
			/* No specific weak-key check for this value */
			break;

		case KEYSIZE_CHECK_PKC:
			if( isShortPKCKey( bignumLength ) )
				return( CRYPT_ERROR_NOSECURE );
			break;

		case KEYSIZE_CHECK_ECC:
			if( isShortECCKey( bignumLength ) )
				return( CRYPT_ERROR_NOSECURE );
			break;

		case KEYSIZE_CHECK_SPECIAL:
			/* This changes the upper bound of the check rather than the 
			   lower bound so there's no special check required */
			break;

		default:
			retIntError();
		}
	if( bignumLength < minLength || bignumLength > maxLength )
		return( CRYPT_ERROR_BADDATA );

	/* Finally, if the caller has supplied a maximum-range bignum value, 
	   make sure that the value that we've read is less than this */
	if( maxRange != NULL && BN_cmp( bignum, maxRange ) >= 0 )
		return( CRYPT_ERROR_BADDATA );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int importBignum( INOUT TYPECAST( BIGNUM * ) void *bignumPtr, 
				  IN_BUFFER( length ) const void *buffer, 
				  IN_LENGTH_SHORT const int length,
				  IN_LENGTH_PKC const int minLength, 
				  IN_LENGTH_PKC const int maxLength, 
				  IN_OPT const void *maxRangePtr,
				  IN_ENUM_OPT( KEYSIZE_CHECK ) \
					const KEYSIZE_CHECK_TYPE checkType )
	{
	BIGNUM *bignum = ( BIGNUM * ) bignumPtr;
	BIGNUM *maxRange = ( BIGNUM * ) maxRangePtr;

	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );
	assert( isReadPtr( buffer, length ) );
	assert( maxRange == NULL || isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES( minLength > 0 && minLength <= maxLength && \
			  maxLength <= CRYPT_MAX_PKCSIZE + \
						   ( ( checkType == KEYSIZE_CHECK_SPECIAL ) ? \
							   DLP_OVERFLOW_SIZE : 0 ) );
	REQUIRES( checkType >= KEYSIZE_CHECK_NONE && \
			  checkType < KEYSIZE_CHECK_LAST_SPECIAL );

	/* Make sure that we've been given valid input.  This should have been 
	   checked by the caller anyway using far more specific checks than the
	   very generic values that we use here, but we perform the check anyway
	   just to be sure */
	if( length < 1 )
		return( CRYPT_ERROR_BADDATA );
	if( checkType == KEYSIZE_CHECK_SPECIAL )
		{
		if( length > CRYPT_MAX_PKCSIZE + DLP_OVERFLOW_SIZE )
			return( CRYPT_ERROR_BADDATA );
		}
	else
		{
		if( length > CRYPT_MAX_PKCSIZE )
			return( CRYPT_ERROR_BADDATA );
		}

	/* Convert the byte string into a bignum */
	if( BN_bin2bn( buffer, length, bignum ) == NULL )
		return( CRYPT_ERROR_MEMORY );

	/* Check the value that we've just imported */
	return( checkBignum( bignum, minLength, maxLength, maxRange, 
						 checkType ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int exportBignum( OUT_BUFFER( dataMaxLength, *dataLength ) void *data, 
				  IN_LENGTH_SHORT_MIN( 16 ) const int dataMaxLength, 
				  OUT_LENGTH_SHORT_Z int *dataLength,
				  IN TYPECAST( BIGNUM * ) const void *bignumPtr )
	{
	BIGNUM *bignum = ( BIGNUM * ) bignumPtr;
	int length, result;

	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES( dataMaxLength >= 16 && dataMaxLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	/* Make sure that the result will fit into the output buffer */
	length = BN_num_bytes( bignum );
	ENSURES( length > 0 && length <= CRYPT_MAX_PKCSIZE );

	/* Export the bignum into the output buffer */
	result = BN_bn2bin( bignum, data );
	ENSURES( result == length );
	*dataLength = length;

	return( CRYPT_OK );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

/* Convert a byte string to and from an ECC point */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int importECCPoint( INOUT TYPECAST( BIGNUM * ) void *bignumPtr1, 
					INOUT TYPECAST( BIGNUM * ) void *bignumPtr2, 
				    IN_BUFFER( length ) const void *buffer, 
				    IN_LENGTH_SHORT const int length,
					IN_LENGTH_PKC const int minLength, 
					IN_LENGTH_PKC const int maxLength, 
					IN_LENGTH_PKC const int fieldSize,
					IN_OPT const void *maxRangePtr,
					IN_ENUM( KEYSIZE_CHECK ) \
						const KEYSIZE_CHECK_TYPE checkType )
	{
	const BYTE *eccPointData = buffer;
	BIGNUM *bignum1 = ( BIGNUM * ) bignumPtr1;
	BIGNUM *bignum2 = ( BIGNUM * ) bignumPtr2;
	BIGNUM *maxRange = ( BIGNUM * ) maxRangePtr;
	int status;

	assert( isWritePtr( bignum1, sizeof( BIGNUM ) ) );
	assert( isWritePtr( bignum2, sizeof( BIGNUM ) ) );
	assert( isReadPtr( buffer, length ) );
	assert( maxRange == NULL || isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES( minLength > 0 && minLength <= maxLength && \
			  maxLength <= CRYPT_MAX_PKCSIZE_ECC );
	REQUIRES( fieldSize >= MIN_PKCSIZE_ECC && \
			  fieldSize <= CRYPT_MAX_PKCSIZE_ECC );
	REQUIRES( checkType >= KEYSIZE_CHECK_NONE && \
			  checkType < KEYSIZE_CHECK_LAST );

	/* Make sure that we've been given valid input.  This should have been 
	   checked by the caller anyway using far more specific checks than the
	   very generic values that we use here, but we perform the check anyway
	   just to be sure */
	if( length < MIN_PKCSIZE_ECCPOINT_THRESHOLD || \
		length > MAX_PKCSIZE_ECCPOINT )
		return( CRYPT_ERROR_BADDATA );

	/* Decode the ECC point data.  At this point we run into another 
	   artefact of the chronic indecisiveness of the ECC standards authors, 
	   because of the large variety of ways in which ECC data can be encoded 
	   there's no single way of representing a point.  The only encoding 
	   that's actually of any practical use is the straight (x, y) 
	   coordinate form with no special-case encoding or other tricks, 
	   denoted by an 0x04 byte at the start of the data:

		+---+---------------+---------------+
		|ID	|		qx		|		qy		|
		+---+---------------+---------------+
			|<- fldSize --> |<- fldSize --> |

	   There's also a hybrid form that combines the patent encumbrance of 
	   point compression with the size of the uncompressed form that we 
	   could in theory allow for, but it's unlikely that anyone's going to
	   be crazy enough to use this */
	if( length != ( fieldSize * 2 ) + 1 )
		return( CRYPT_ERROR_BADDATA );
	if( eccPointData[ 0 ] != 0x04 )
		return( CRYPT_ERROR_BADDATA );
	status = importBignum( bignum1, eccPointData + 1, fieldSize,
						   minLength, maxLength, maxRange, checkType );
	if( cryptStatusError( status ) )
		return( status );
	return( importBignum( bignum2, eccPointData + 1 + fieldSize, 
						  fieldSize, minLength, maxLength, maxRange, 
						  checkType ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 4, 5 ) ) \
int exportECCPoint( OUT_BUFFER_OPT( dataMaxLength, *dataLength ) void *data, 
					IN_LENGTH_SHORT_Z const int dataMaxLength, 
					OUT_LENGTH_SHORT_Z int *dataLength,
					IN TYPECAST( BIGNUM * ) const void *bignumPtr1, 
					IN TYPECAST( BIGNUM * ) const void *bignumPtr2,
					IN_LENGTH_PKC const int fieldSize )
	{
	BIGNUM *bignum1 = ( BIGNUM * ) bignumPtr1;
	BIGNUM *bignum2 = ( BIGNUM * ) bignumPtr2;
	BYTE *bufPtr = data;
	int length, result;

	assert( data == NULL || isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );
	assert( isReadPtr( bignum1, sizeof( BIGNUM ) ) );
	assert( isReadPtr( bignum2, sizeof( BIGNUM ) ) );

	REQUIRES( ( data == NULL && dataMaxLength == 0 ) || \
			  ( data != NULL && \
				dataMaxLength >= 16 && \
				dataMaxLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( fieldSize >= MIN_PKCSIZE_ECC && \
			  fieldSize <= CRYPT_MAX_PKCSIZE_ECC );

	/* Clear return values */
	if( data != NULL )
		memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	/* If it's just an encoding-length check there's nothing to do */
	if( data == NULL )
		{
		*dataLength = 1 + ( fieldSize * 2 );
		return( CRYPT_OK );
		}

	/* Encode the ECC point data, which consists of the Q coordinates 
	   stuffed into a byte string with an encoding-specifier byte 0x04 at 
	   the start:

		+---+---------------+---------------+
		|ID	|		qx		|		qy		|
		+---+---------------+---------------+
			|<-- fldSize -> |<- fldSize --> | */
	if( dataMaxLength < 1 + ( fieldSize * 2 ) )
		return( CRYPT_ERROR_OVERFLOW );
	*bufPtr++ = 0x04;
	memset( bufPtr, 0, fieldSize * 2 );
	length = BN_num_bytes( bignum1 );
	ENSURES( length > 0 && length <= fieldSize );
	result = BN_bn2bin( bignum1, bufPtr + ( fieldSize - length ) );
	ENSURES( result == length );
	bufPtr += fieldSize;
	length = BN_num_bytes( bignum2 );
	ENSURES( length > 0 && length <= fieldSize );
	result = BN_bn2bin( bignum2, bufPtr + ( fieldSize - length ) );
	ENSURES( result == length );
	*dataLength = 1 + ( fieldSize * 2 );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH || USE_ECDSA */

#else

STDC_NONNULL_ARG( ( 1 ) ) \
void clearTempBignums( INOUT PKC_INFO *pkcInfo )
	{
	}
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initContextBignums( INOUT PKC_INFO *pkcInfo, 
						IN_RANGE( 0, 3 ) const int sideChannelProtectionLevel )
	{
	}
STDC_NONNULL_ARG( ( 1 ) ) \
void freeContextBignums( INOUT PKC_INFO *pkcInfo, 
						 IN_FLAGS( CONTEXT ) const int contextFlags )
	{
	}
#endif /* USE_PKC */

/****************************************************************************
*																			*
*							Self-test Support Functions						*
*																			*
****************************************************************************/

/* Statically initialise a context for the internal self-test and a few 
   other internal-only functions such as when generating keyIDs for raw 
   encoded key data where no context is available */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int staticInitContext( OUT CONTEXT_INFO *contextInfoPtr, 
					   IN_ENUM( CONTEXT_TYPE ) const CONTEXT_TYPE type, 
					   const CAPABILITY_INFO *capabilityInfoPtr,
					   OUT_BUFFER_FIXED( contextDataSize ) void *contextData, 
					   IN_LENGTH_SHORT_MIN( 32 ) const int contextDataSize,
					   IN_OPT void *keyData )
	{
	int status;
	
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );
	assert( isReadPtr( contextData, contextDataSize ) );

	REQUIRES( type > CONTEXT_NONE && type < CONTEXT_LAST );
	REQUIRES( contextDataSize >= 32 && \
			  contextDataSize < MAX_INTLENGTH_SHORT );

	memset( contextInfoPtr, 0, sizeof( CONTEXT_INFO ) );
	memset( contextData, 0, contextDataSize );
	contextInfoPtr->type = type;
	contextInfoPtr->capabilityInfo = capabilityInfoPtr;
	contextInfoPtr->flags = CONTEXT_FLAG_STATICCONTEXT;
	switch( type )
		{
		case CONTEXT_CONV:
			contextInfoPtr->ctxConv = ( CONV_INFO * ) contextData;
			contextInfoPtr->ctxConv->key = keyData;
			break;

		case CONTEXT_HASH:
			contextInfoPtr->ctxHash = ( HASH_INFO * ) contextData;
			contextInfoPtr->ctxHash->hashInfo = keyData;
			break;

		case CONTEXT_MAC:
			contextInfoPtr->ctxMAC = ( MAC_INFO * ) contextData;
			contextInfoPtr->ctxMAC->macInfo = keyData;
			break;

		case CONTEXT_PKC:
			/* PKC context initialisation is a bit more complex because we
			   have to set up all of the bignum values as well.  Since static
			   contexts are only used for self-test operations we set the 
			   side-channel protection level to zero */
			contextInfoPtr->ctxPKC = ( PKC_INFO * ) contextData;
			status = initContextBignums( contextData, 0,
							isEccAlgo( capabilityInfoPtr->cryptAlgo ) );
			if( cryptStatusError( status ) )
				return( status );
			initKeyID( contextInfoPtr );
			initKeyRead( contextInfoPtr );
			initKeyWrite( contextInfoPtr );		/* For calcKeyID() */
			break;

		default:
			retIntError();
		}

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void staticDestroyContext( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	ENSURES_V( contextInfoPtr->flags & CONTEXT_FLAG_STATICCONTEXT );

	if( contextInfoPtr->type == CONTEXT_PKC )
		{
		freeContextBignums( contextInfoPtr->ctxPKC, 
					( contextInfoPtr->capabilityInfo->cryptAlgo == \
					  CRYPT_ALGO_RSA ) ? CONTEXT_FLAG_SIDECHANNELPROTECTION : 0 );
		}
	memset( contextInfoPtr, 0, sizeof( CONTEXT_INFO ) );
	}

/* Perform a self-test of a cipher, encrypting and decrypting one block of 
   data and comparing it to a fixed test value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 6 ) ) \
int testCipher( const CAPABILITY_INFO *capabilityInfo, 
				INOUT void *keyDataStorage, 
				IN_BUFFER( keySize ) const void *key, 
				IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) const int keySize, 
				const void *plaintext,
				const void *ciphertext )
	{
	CONTEXT_INFO contextInfo;
	CONV_INFO contextData;
	BYTE temp[ CRYPT_MAX_IVSIZE + 8 ];
	int status;

	assert( isReadPtr( capabilityInfo, sizeof( CAPABILITY_INFO ) ) );
	assert( isWritePtr( keyDataStorage, 16 ) );
	assert( isReadPtr( key, keySize ) );
	assert( isReadPtr( plaintext, capabilityInfo->blockSize ) );
	assert( isReadPtr( ciphertext, capabilityInfo->blockSize ) );

	REQUIRES( keySize >= MIN_KEYSIZE && keySize <= CRYPT_MAX_KEYSIZE );

	memcpy( temp, plaintext, capabilityInfo->blockSize );

	status = staticInitContext( &contextInfo, CONTEXT_CONV, capabilityInfo,
								&contextData, sizeof( CONV_INFO ), 
								keyDataStorage );
	if( cryptStatusError( status ) )
		return( status );
	status = capabilityInfo->initKeyFunction( &contextInfo, key, keySize );
	if( cryptStatusOK( status ) )
		status = capabilityInfo->encryptFunction( &contextInfo, temp, 
												  capabilityInfo->blockSize );
	if( cryptStatusOK( status ) && \
		memcmp( ciphertext, temp, capabilityInfo->blockSize ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusOK( status ) )
		status = capabilityInfo->decryptFunction( &contextInfo, temp, 
												  capabilityInfo->blockSize );
	staticDestroyContext( &contextInfo );
	if( cryptStatusError( status ) || \
		memcmp( plaintext, temp, capabilityInfo->blockSize ) )
		return( CRYPT_ERROR_FAILED );
	
	return( CRYPT_OK );
	}

/* Perform a self-test of a hash or MAC */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
int testHash( const CAPABILITY_INFO *capabilityInfo, 
			  INOUT void *hashDataStorage, 
			  IN_BUFFER_OPT( dataLength ) const void *data, 
			  IN_LENGTH_SHORT_Z const int dataLength, 
			  const void *hashValue )
	{
	CONTEXT_INFO contextInfo;
	HASH_INFO contextData;
	int status;

	assert( isReadPtr( capabilityInfo, sizeof( CAPABILITY_INFO ) ) );
	assert( isWritePtr( hashDataStorage, 16 ) );
	assert( ( data == NULL && dataLength == 0 ) || \
			isReadPtr( data, dataLength ) );
	assert( isReadPtr( hashValue, capabilityInfo->blockSize ) );

	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( data != NULL && \
				dataLength > 0 && dataLength < MAX_INTLENGTH_SHORT ) );

	status = staticInitContext( &contextInfo, CONTEXT_HASH, capabilityInfo,
								&contextData, sizeof( HASH_INFO ), 
								hashDataStorage );
	if( cryptStatusError( status ) )
		return( status );
	if( data != NULL )
		{
		/* Some of the test vector sets start out with empty strings so we 
		   only call the hash function if we've actually been fed data to 
		   hash */
		status = capabilityInfo->encryptFunction( &contextInfo, 
												  ( void * ) data, 
												  dataLength );
		contextInfo.flags |= CONTEXT_FLAG_HASH_INITED;
		}
	if( cryptStatusOK( status ) )
		status = capabilityInfo->encryptFunction( &contextInfo, 
												  MKDATA( "" ), 0 );
	if( cryptStatusOK( status ) && \
		memcmp( contextInfo.ctxHash->hash, hashValue, 
				capabilityInfo->blockSize ) )
		status = CRYPT_ERROR_FAILED;
	staticDestroyContext( &contextInfo );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 7 ) ) \
int testMAC( const CAPABILITY_INFO *capabilityInfo, 
			 INOUT void *macDataStorage, 
			 IN_BUFFER( keySize ) const void *key, 
			 IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) const int keySize, 
			 IN_BUFFER( dataLength ) const void *data, 
			 IN_LENGTH_SHORT_MIN( 8 ) const int dataLength,
			 const void *hashValue )
	{
	CONTEXT_INFO contextInfo;
	MAC_INFO contextData;
	int status;

	assert( isReadPtr( capabilityInfo, sizeof( CAPABILITY_INFO ) ) );
	assert( isWritePtr( macDataStorage, 16 ) );
	assert( isReadPtr( key, keySize ) );
	assert( isReadPtr( data, dataLength ) );
	assert( isReadPtr( hashValue, capabilityInfo->blockSize ) );

	REQUIRES( keySize >= 4 && keySize < MAX_INTLENGTH_SHORT );
	REQUIRES( dataLength >= 8 && dataLength < MAX_INTLENGTH_SHORT );

	status = staticInitContext( &contextInfo, CONTEXT_MAC, capabilityInfo,
								&contextData, sizeof( MAC_INFO ), 
								macDataStorage );
	if( cryptStatusError( status ) )
		return( status );
	status = capabilityInfo->initKeyFunction( &contextInfo, key, keySize );
	if( cryptStatusOK( status ) )
		{
		status = capabilityInfo->encryptFunction( &contextInfo, 
												  ( void * ) data, 
												  dataLength );
		contextInfo.flags |= CONTEXT_FLAG_HASH_INITED;
		}
	if( cryptStatusOK( status ) )
		status = capabilityInfo->encryptFunction( &contextInfo, 
												  MKDATA( "" ), 0 );
	if( cryptStatusOK( status ) && \
		memcmp( contextInfo.ctxMAC->mac, hashValue, 
				capabilityInfo->blockSize ) )
		status = CRYPT_ERROR_FAILED;
	staticDestroyContext( &contextInfo );

	return( status );
	}

/****************************************************************************
*																			*
*							Hash External Access Functions					*
*																			*
****************************************************************************/

/* Determine the parameters for a particular hash algorithm */

typedef struct HI {
	const CRYPT_ALGO_TYPE cryptAlgo;
	const int hashSize;
	const HASHFUNCTION function;
	} HASHFUNCTION_INFO;

typedef struct HAI {
	const CRYPT_ALGO_TYPE cryptAlgo;
	const int hashSize;
	const HASHFUNCTION_ATOMIC function;
	} HASHFUNCTION_ATOMIC_INFO;

STDC_NONNULL_ARG( ( 3 ) ) \
void getHashParameters( IN_ALGO const CRYPT_ALGO_TYPE hashAlgorithm,
						IN_INT_SHORT_Z const int hashParam,
						OUT_PTR HASHFUNCTION *hashFunction, 
						OUT_OPT_LENGTH_SHORT_Z int *hashOutputSize )
	{
	static const HASHFUNCTION_INFO FAR_BSS hashFunctions[] = {
#ifdef USE_MD5
		{ CRYPT_ALGO_MD5, MD5_DIGEST_LENGTH, md5HashBuffer },
#endif /* USE_MD5 */
#ifdef USE_RIPEMD160
		{ CRYPT_ALGO_RIPEMD160, RIPEMD160_DIGEST_LENGTH, ripemd160HashBuffer },
#endif /* USE_RIPEMD160 */
		{ CRYPT_ALGO_SHA1, SHA_DIGEST_LENGTH, shaHashBuffer },
#ifdef USE_SHA2
		{ CRYPT_ALGO_SHA2, SHA256_DIGEST_SIZE, sha2HashBuffer },
  #ifdef USE_SHA2_EXT
		/* The extended SHA2 variants are only available on systems with 64-
		   bit data type support */
	#if defined( CONFIG_SUITEB )
		{ CRYPT_ALGO_SHA2, SHA384_DIGEST_SIZE, sha2_ExtHashBuffer },
	#else
		{ CRYPT_ALGO_SHA2, SHA512_DIGEST_SIZE, sha2_ExtHashBuffer },
	#endif /* Suite B vs. generic use */
  #endif /* USE_SHA2_EXT */
#endif /* USE_SHA2 */
		{ CRYPT_ALGO_NONE, SHA_DIGEST_LENGTH, shaHashBuffer },
			{ CRYPT_ALGO_NONE, SHA_DIGEST_LENGTH, shaHashBuffer }
		};
	int i;

	assert( isHashAlgo( hashAlgorithm ) );
	assert( hashParam >= 0 && hashParam < MAX_INTLENGTH_SHORT );
			/* We don't use REQUIRES() for this for the reason given in the
			   comments below */
	assert( isWritePtr( hashFunction, sizeof( HASHFUNCTION ) ) );
	assert( ( hashOutputSize == NULL ) || \
			isWritePtr( hashOutputSize, sizeof( int ) ) );

	/* Find the information for the requested hash algorithm */
	for( i = 0; hashFunctions[ i ].cryptAlgo != CRYPT_ALGO_NONE && \
				i < FAILSAFE_ARRAYSIZE( hashFunctions, HASHFUNCTION_INFO ); 
		 i++ )
		{
		/* If this isn't the algorithm that we're looking for, continue */
		if( hashFunctions[ i ].cryptAlgo != hashAlgorithm )
			continue;

		/* If we're looking for a specific hash-size match and this isn't
		   it, continue */
		if( hashParam != 0 && hashFunctions[ i ].hashSize != hashParam )
			continue;

		/* We've found what we're after */
		break;
		}
	if( i >= FAILSAFE_ARRAYSIZE( hashFunctions, HASHFUNCTION_INFO ) || \
		hashFunctions[ i ].cryptAlgo == CRYPT_ALGO_NONE )
		{
		/* Make sure that we always get some sort of hash function rather 
		   than just dying.  This code always works because the internal 
		   self-test has confirmed the availability and functioning of SHA-1 
		   on startup */
		*hashFunction = shaHashBuffer;
		if( hashOutputSize != NULL )
			*hashOutputSize = SHA_DIGEST_LENGTH;
		retIntError_Void();
		}

	*hashFunction = hashFunctions[ i ].function;
	if( hashOutputSize != NULL )
		*hashOutputSize = hashFunctions[ i ].hashSize;
	}

STDC_NONNULL_ARG( ( 3 ) ) \
void getHashAtomicParameters( IN_ALGO const CRYPT_ALGO_TYPE hashAlgorithm,
							  IN_INT_SHORT_Z const int hashParam,
							  OUT_PTR HASHFUNCTION_ATOMIC *hashFunctionAtomic, 
							  OUT_OPT_LENGTH_SHORT_Z int *hashOutputSize )
	{
	static const HASHFUNCTION_ATOMIC_INFO FAR_BSS hashFunctions[] = {
#ifdef USE_MD5
		{ CRYPT_ALGO_MD5, MD5_DIGEST_LENGTH, md5HashBufferAtomic },
#endif /* USE_MD5 */
#ifdef USE_RIPEMD160
		{ CRYPT_ALGO_RIPEMD160, RIPEMD160_DIGEST_LENGTH, ripemd160HashBufferAtomic },
#endif /* USE_RIPEMD160 */
		{ CRYPT_ALGO_SHA1, SHA_DIGEST_LENGTH, shaHashBufferAtomic },
#ifdef USE_SHA2
		{ CRYPT_ALGO_SHA2, SHA256_DIGEST_SIZE, sha2HashBufferAtomic },
  #ifdef USE_SHA2_EXT
		/* The extended SHA2 variants are only available on systems with 64-
		   bit data type support */
	#if defined( CONFIG_SUITEB )
		{ CRYPT_ALGO_SHA2, SHA384_DIGEST_SIZE, sha2_ExtHashBufferAtomic },
	#else
		{ CRYPT_ALGO_SHA2, SHA512_DIGEST_SIZE, sha2_ExtHashBufferAtomic },
	#endif /* Suite B vs. generic use */
  #endif /* USE_SHA2_EXT */
#endif /* USE_SHA2 */
		{ CRYPT_ALGO_NONE, SHA_DIGEST_LENGTH, shaHashBufferAtomic },
			{ CRYPT_ALGO_NONE, SHA_DIGEST_LENGTH, shaHashBufferAtomic }
		};
	int i;

	assert( isHashAlgo( hashAlgorithm ) );
	assert( hashParam >= 0 && hashParam < MAX_INTLENGTH_SHORT );
			/* We don't use REQUIRES() for this for the reason given in the
			   comments below */
	assert( isWritePtr( hashFunctionAtomic, sizeof( HASHFUNCTION_ATOMIC ) ) );
	assert( ( hashOutputSize == NULL ) || \
			isWritePtr( hashOutputSize, sizeof( int ) ) );

	/* Find the information for the requested hash algorithm */
	for( i = 0; hashFunctions[ i ].cryptAlgo != CRYPT_ALGO_NONE && \
				i < FAILSAFE_ARRAYSIZE( hashFunctions, HASHFUNCTION_ATOMIC_INFO ); 
		 i++ )
		{
		/* If this isn't the algorithm that we're looking for, continue */
		if( hashFunctions[ i ].cryptAlgo != hashAlgorithm )
			continue;

		/* If we're looking for a specific hash-size match and this isn't
		   it, continue */
		if( hashParam != 0 && hashFunctions[ i ].hashSize != hashParam )
			continue;

		/* We've found what we're after */
		break;
		}
	if( i >= FAILSAFE_ARRAYSIZE( hashFunctions, HASHFUNCTION_ATOMIC_INFO ) || \
		hashFunctions[ i ].cryptAlgo == CRYPT_ALGO_NONE )
		{
		/* Make sure that we always get some sort of hash function rather 
		   than just dying.  This code always works because the internal 
		   self-test has confirmed the availability and functioning of SHA-1 
		   on startup */
		*hashFunctionAtomic = shaHashBufferAtomic;
		if( hashOutputSize != NULL )
			*hashOutputSize = SHA_DIGEST_LENGTH;
		retIntError_Void();
		}

	*hashFunctionAtomic = hashFunctions[ i ].function;
	if( hashOutputSize != NULL )
		*hashOutputSize = hashFunctions[ i ].hashSize;
	}
