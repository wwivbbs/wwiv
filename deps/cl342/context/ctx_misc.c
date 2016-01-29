/****************************************************************************
*																			*
*						cryptlib Context Support Routines					*
*						Copyright Peter Gutmann 1995-2012					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #ifdef USE_MD5
	#include "md5.h"
  #endif /* USE_MD5 */
  #include "sha.h"
  #include "sha2.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #ifdef USE_MD5
	#include "crypt/md5.h"
  #endif /* USE_MD5 */
  #include "crypt/sha.h"
  #include "crypt/sha2.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						Capability Management Functions						*
*																			*
****************************************************************************/

/* Check that a capability info record is consistent */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckFunctionality( const CAPABILITY_INFO *capabilityInfoPtr,
										 IN_ALGO CRYPT_ALGO_TYPE cryptAlgo )
	{
	const BOOLEAN isCrypt = \
		( capabilityInfoPtr->encryptCBCFunction != NULL || \
		  capabilityInfoPtr->decryptCBCFunction != NULL || \
		  capabilityInfoPtr->encryptCFBFunction != NULL || \
		  capabilityInfoPtr->decryptCFBFunction != NULL || \
		  capabilityInfoPtr->encryptGCMFunction != NULL || \
		  capabilityInfoPtr->decryptGCMFunction != NULL ) ? TRUE : FALSE;
	const BOOLEAN isSig = \
		( capabilityInfoPtr->signFunction != NULL || \
		  capabilityInfoPtr->sigCheckFunction != NULL ) ? TRUE : FALSE;

	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );

	REQUIRES( cryptAlgo > CRYPT_ALGO_NONE && cryptAlgo < CRYPT_ALGO_LAST );

	/* Generic-secret algorithms are non-capabilities used to to store
	   keying data, but that can't perform any operations themselves */
	if( isSpecialAlgo( cryptAlgo ) )
		{
		if( capabilityInfoPtr->encryptFunction != NULL || \
			capabilityInfoPtr->decryptFunction != NULL || \
			isCrypt || isSig )
			return( FALSE );

		return( TRUE );
		}

	/* We need at least one mechanism pair to be able to do anything useful 
	   with the capability */
	if( ( capabilityInfoPtr->encryptFunction == NULL || \
		  capabilityInfoPtr->decryptFunction == NULL ) && \
		( capabilityInfoPtr->encryptCBCFunction == NULL || \
		  capabilityInfoPtr->decryptCBCFunction == NULL ) && \
		( capabilityInfoPtr->encryptCFBFunction == NULL || \
		  capabilityInfoPtr->decryptCFBFunction == NULL ) && \
		( capabilityInfoPtr->encryptGCMFunction == NULL || \
		  capabilityInfoPtr->decryptGCMFunction == NULL ) && \
		( capabilityInfoPtr->signFunction == NULL || \
		  capabilityInfoPtr->sigCheckFunction == NULL ) )
		return( FALSE );

	/* Perform algorithm class-specific checks */
	if( isConvAlgo( cryptAlgo ) )
		{
		if( isSig )
			return( FALSE );
		if( isStreamCipher( cryptAlgo ) )
			{
			if( capabilityInfoPtr->encryptCFBFunction == NULL || \
				capabilityInfoPtr->decryptCFBFunction == NULL )
				return( FALSE );
			if( capabilityInfoPtr->encryptFunction != NULL || \
				capabilityInfoPtr->decryptFunction != NULL || \
				capabilityInfoPtr->encryptCBCFunction != NULL || \
				capabilityInfoPtr->decryptCBCFunction != NULL || \
				capabilityInfoPtr->encryptCFBFunction != NULL || \
				capabilityInfoPtr->decryptCFBFunction != NULL || \
				capabilityInfoPtr->encryptGCMFunction != NULL || \
				capabilityInfoPtr->decryptGCMFunction != NULL )
				return( FALSE );
			}
		else
			{
			if( capabilityInfoPtr->encryptFunction == NULL && \
				capabilityInfoPtr->decryptFunction == NULL && \
				!isCrypt )
				return( FALSE );
			}
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
		if( ( capabilityInfoPtr->encryptGCMFunction != NULL && \
			  capabilityInfoPtr->decryptGCMFunction == NULL ) || \
			( capabilityInfoPtr->encryptGCMFunction == NULL && \
			  capabilityInfoPtr->decryptGCMFunction != NULL ) )
			return( FALSE );
		
		return( TRUE );
		}
	if( isPkcAlgo( cryptAlgo ) )
		{
		if( capabilityInfoPtr->encryptFunction == NULL && \
			capabilityInfoPtr->decryptFunction == NULL && \
			capabilityInfoPtr->signFunction == NULL && \
			capabilityInfoPtr->sigCheckFunction == NULL )
			return( FALSE );
		if( isCrypt )
			return( FALSE );

		return( TRUE );
		}
	if( isHashAlgo( cryptAlgo ) || isMacAlgo( cryptAlgo ) )
		{
		if( capabilityInfoPtr->encryptFunction == NULL || \
			capabilityInfoPtr->decryptFunction == NULL )
			return( FALSE );
		if( isCrypt || isSig )
			return( FALSE );

		return( TRUE );
		}

	retIntError_Boolean();
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckCapability( const CAPABILITY_INFO *capabilityInfoPtr )
	{
	CRYPT_ALGO_TYPE cryptAlgo = capabilityInfoPtr->cryptAlgo;

	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );

	/* Check the algorithm and mode parameters.  We check for an algorithm
	   name one shorter than the maximum because as returned to an external
	   caller it's an ASCIZ string so we need to allow room for the
	   terminator */
	if( cryptAlgo <= CRYPT_ALGO_NONE || cryptAlgo >= CRYPT_ALGO_LAST )
		return( FALSE );
	if( capabilityInfoPtr->algoName == NULL || \
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
	if( !sanityCheckFunctionality( capabilityInfoPtr, cryptAlgo ) )
		return( FALSE );

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

		return( TRUE );
		}

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

	assert( isReadPtr( capabilityInfoList, sizeof( CAPABILITY_INFO_LIST ) ) );

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
*							Self-test Support Functions						*
*																			*
****************************************************************************/

/* Statically initialise a context for the internal self-test and a few 
   other internal-only functions such as when generating keyIDs for raw 
   encoded key data where no context is available */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int staticInitContext( OUT CONTEXT_INFO *contextInfoPtr, 
					   IN_ENUM( CONTEXT ) const CONTEXT_TYPE type, 
					   const CAPABILITY_INFO *capabilityInfoPtr,
					   OUT_BUFFER_FIXED( contextDataSize ) void *contextData, 
					   IN_LENGTH_MIN( 32 ) const int contextDataSize,
					   IN_OPT const void *keyData )
	{
	int status;
	
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );
	assert( isReadPtr( contextData, contextDataSize ) );

	REQUIRES( type > CONTEXT_NONE && type < CONTEXT_LAST );
	REQUIRES( contextDataSize >= 32 && \
			  ( ( type != CONTEXT_PKC && \
				  contextDataSize < MAX_INTLENGTH_SHORT ) || \
				( type == CONTEXT_PKC && \
				  contextDataSize < MAX_INTLENGTH ) ) );

	memset( contextInfoPtr, 0, sizeof( CONTEXT_INFO ) );
	memset( contextData, 0, contextDataSize );
	contextInfoPtr->type = type;
	contextInfoPtr->capabilityInfo = capabilityInfoPtr;
	contextInfoPtr->flags = CONTEXT_FLAG_STATICCONTEXT;
	switch( type )
		{
		case CONTEXT_CONV:
			contextInfoPtr->ctxConv = ( CONV_INFO * ) contextData;
			contextInfoPtr->ctxConv->key = ( void * ) keyData;
			break;

		case CONTEXT_HASH:
			contextInfoPtr->ctxHash = ( HASH_INFO * ) contextData;
			contextInfoPtr->ctxHash->hashInfo = ( void * ) keyData;
			break;

		case CONTEXT_MAC:
			contextInfoPtr->ctxMAC = ( MAC_INFO * ) contextData;
			contextInfoPtr->ctxMAC->macInfo = ( void * ) keyData;
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
			initPubKeyRead( contextInfoPtr );
			initPrivKeyRead( contextInfoPtr );
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
		endContextBignums( contextInfoPtr->ctxPKC, contextInfoPtr->flags );
	memset( contextInfoPtr, 0, sizeof( CONTEXT_INFO ) );
	}

/* Perform a self-test of a cipher, encrypting and decrypting one block of 
   data and comparing it to a fixed test value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 6 ) ) \
int testCipher( const CAPABILITY_INFO *capabilityInfo, 
				OUT void *keyDataStorage, 
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
			  OUT void *hashDataStorage, 
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
			 OUT void *macDataStorage, 
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
		{ CRYPT_ALGO_SHA1, SHA_DIGEST_LENGTH, shaHashBuffer },
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

	/* Clear return value */
	if( hashOutputSize != NULL )
		*hashOutputSize = 0;

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
		{ CRYPT_ALGO_SHA1, SHA_DIGEST_LENGTH, shaHashBufferAtomic },
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

	/* Clear return value */
	if( hashOutputSize != NULL )
		*hashOutputSize = 0;

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
