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

	REQUIRES( isEnumRange( cryptAlgo, CRYPT_ALGO ) );

	/* Generic-secret algorithms are non-capabilities used to to store
	   keying data, but that can't perform any operations themselves */
	if( isSpecialAlgo( cryptAlgo ) )
		{
		if( capabilityInfoPtr->encryptFunction != NULL || \
			capabilityInfoPtr->decryptFunction != NULL || \
			isCrypt || isSig )
			{
			DEBUG_PUTS(( "sanityCheckFunctionality: Generic secret" ));
			return( FALSE );
			}

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
		{
		DEBUG_PUTS(( "sanityCheckFunctionality: General capabilities" ));
		return( FALSE );
		}

	/* Perform algorithm class-specific checks */
	if( isConvAlgo( cryptAlgo ) )
		{
		if( isSig )
			{
			DEBUG_PUTS(( "sanityCheckFunctionality: Spurious conv capability" ));
			return( FALSE );
			}
		if( isStreamCipher( cryptAlgo ) )
			{
			if( capabilityInfoPtr->encryptFunction == NULL || \
				capabilityInfoPtr->decryptFunction == NULL )
				{
				DEBUG_PUTS(( "sanityCheckFunctionality: Missing CFB" ));
				return( FALSE );
				}
			if( capabilityInfoPtr->encryptCBCFunction != NULL || \
				capabilityInfoPtr->decryptCBCFunction != NULL || \
				capabilityInfoPtr->encryptCFBFunction != NULL || \
				capabilityInfoPtr->decryptCFBFunction != NULL || \
				capabilityInfoPtr->encryptGCMFunction != NULL || \
				capabilityInfoPtr->decryptGCMFunction != NULL )
				{
				DEBUG_PUTS(( "sanityCheckFunctionality: Spurious stream capability" ));
				return( FALSE );
				}
			}
		else
			{
			if( capabilityInfoPtr->encryptFunction == NULL && \
				capabilityInfoPtr->decryptFunction == NULL && \
				!isCrypt )
				{
				DEBUG_PUTS(( "sanityCheckFunctionality: Missing crypt capability" ));
				return( FALSE );
				}
			}
		if( ( capabilityInfoPtr->encryptCBCFunction != NULL && \
			  capabilityInfoPtr->decryptCBCFunction == NULL ) || \
			( capabilityInfoPtr->encryptCBCFunction == NULL && \
			  capabilityInfoPtr->decryptCBCFunction != NULL ) )
			{
			DEBUG_PUTS(( "sanityCheckFunctionality: Inconsistent CBC" ));
			return( FALSE );
			}
		if( ( capabilityInfoPtr->encryptCFBFunction != NULL && \
			  capabilityInfoPtr->decryptCFBFunction == NULL ) || \
			( capabilityInfoPtr->encryptCFBFunction == NULL && \
			  capabilityInfoPtr->decryptCFBFunction != NULL ) )
			{
			DEBUG_PUTS(( "sanityCheckFunctionality: Inconsistent CFB" ));
			return( FALSE );
			}
		if( ( capabilityInfoPtr->encryptGCMFunction != NULL && \
			  capabilityInfoPtr->decryptGCMFunction == NULL ) || \
			( capabilityInfoPtr->encryptGCMFunction == NULL && \
			  capabilityInfoPtr->decryptGCMFunction != NULL ) )
			{
			DEBUG_PUTS(( "sanityCheckFunctionality: Inconsistent GCM" ));
			return( FALSE );
			}
		
		return( TRUE );
		}
	if( isPkcAlgo( cryptAlgo ) )
		{
		if( capabilityInfoPtr->encryptFunction == NULL && \
			capabilityInfoPtr->decryptFunction == NULL && \
			capabilityInfoPtr->signFunction == NULL && \
			capabilityInfoPtr->sigCheckFunction == NULL )
			{
			DEBUG_PUTS(( "sanityCheckFunctionality: Missing PKC capability" ));
			return( FALSE );
			}
		if( isCrypt )
			{
			DEBUG_PUTS(( "sanityCheckFunctionality: Spurious PKC capability" ));
			return( FALSE );
			}

		return( TRUE );
		}
	if( isHashAlgo( cryptAlgo ) || isMacAlgo( cryptAlgo ) )
		{
		if( capabilityInfoPtr->encryptFunction == NULL || \
			capabilityInfoPtr->decryptFunction == NULL )
			{
			DEBUG_PUTS(( "sanityCheckFunctionality: Missing hash/MAC capability" ));
			return( FALSE );
			}
		if( isCrypt || isSig )
			{
			DEBUG_PUTS(( "sanityCheckFunctionality: Spurious hash/MAC capability" ));
			return( FALSE );
			}

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
	if( !isEnumRange( cryptAlgo, CRYPT_ALGO ) )
		{
		DEBUG_PUTS(( "sanityCheckCapability: Algorithm" ));
		return( FALSE );
		}
	if( capabilityInfoPtr->algoName == NULL || \
		capabilityInfoPtr->algoNameLen < 3 || \
		capabilityInfoPtr->algoNameLen > CRYPT_MAX_TEXTSIZE - 1 )
		{
		DEBUG_PUTS(( "sanityCheckCapability: Algorithm name" ));
		return( FALSE );
		}

	/* Make sure that the minimum functions are present.  We don't check for
	   the presence of the keygen function since the symmetric capabilities
	   use the generic keygen and the hash capabilities don't do keygen at 
	   all */
#ifdef CONFIG_NO_SELFTEST
	if( capabilityInfoPtr->selfTestFunction != NULL )
		{
		DEBUG_PUTS(( "sanityCheckCapability: Suprious self-test function" ));
		return( FALSE );
		}
#else
	if( capabilityInfoPtr->selfTestFunction == NULL )
		{
		DEBUG_PUTS(( "sanityCheckCapability: Self-test function" ));
		return( FALSE );
		}
#endif /* CONFIG_NO_SELFTEST */
	if( capabilityInfoPtr->getInfoFunction == NULL )
		{
		DEBUG_PUTS(( "sanityCheckCapability: Get-info function" ));
		return( FALSE );
		}
	if( !sanityCheckFunctionality( capabilityInfoPtr, cryptAlgo ) )
		{
		DEBUG_PUTS(( "sanityCheckCapability: Functionality" ));
		return( FALSE );
		}

	/* Make sure that the algorithm/mode-specific parameters are
	   consistent */
	if( capabilityInfoPtr->minKeySize > capabilityInfoPtr->keySize || \
		capabilityInfoPtr->maxKeySize < capabilityInfoPtr->keySize )
		{
		DEBUG_PUTS(( "sanityCheckCapability: Key size" ));
		return( FALSE );
		}
	if( isConvAlgo( cryptAlgo ) )
		{
		if( ( capabilityInfoPtr->blockSize < bitsToBytes( 8 ) || \
        	  capabilityInfoPtr->blockSize > CRYPT_MAX_IVSIZE ) || \
			( capabilityInfoPtr->minKeySize < MIN_KEYSIZE || \
			  capabilityInfoPtr->maxKeySize > CRYPT_MAX_KEYSIZE ) )
			{
			DEBUG_PUTS(( "sanityCheckCapability: Conv. block size" ));
			return( FALSE );
			}
		if( capabilityInfoPtr->keySize > MAX_WORKING_KEYSIZE )
			{
			DEBUG_PUTS(( "sanityCheckCapability: Conv. key wrap size" ));
			return( FALSE );	/* Requirement for key wrap */
			}
		if( capabilityInfoPtr->initParamsFunction == NULL || \
			capabilityInfoPtr->initKeyFunction == NULL )
			{
			DEBUG_PUTS(( "sanityCheckCapability: Conv. init functions" ));
			return( FALSE );
			}
		if( !isStreamCipher( cryptAlgo ) && \
			 capabilityInfoPtr->blockSize < bitsToBytes( 64 ) )
			{
			DEBUG_PUTS(( "sanityCheckCapability: Conv. minimum block size" ));
			return( FALSE );
			}

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
			{
			DEBUG_PUTS(( "sanityCheckCapability: PKC key size" ));
			return( FALSE );
			}
		if( capabilityInfoPtr->initKeyFunction == NULL || \
			capabilityInfoPtr->generateKeyFunction == NULL )
			{
			DEBUG_PUTS(( "sanityCheckCapability: PKC Init/generate function" ));
			return( FALSE );
			}

		return( TRUE );
		}
	if( isHashAlgo( cryptAlgo ) )
		{
		if( ( capabilityInfoPtr->blockSize < MIN_HASHSIZE || \
			  capabilityInfoPtr->blockSize > CRYPT_MAX_HASHSIZE ) || \
			( capabilityInfoPtr->minKeySize != 0 || \
			  capabilityInfoPtr->keySize != 0 || \
			  capabilityInfoPtr->maxKeySize != 0 ) )
			{
			DEBUG_PUTS(( "sanityCheckCapability: Hash block size" ));
			return( FALSE );
			}

		return( TRUE );
		}
	if( isMacAlgo( cryptAlgo ) )
		{
		if( ( capabilityInfoPtr->blockSize < MIN_HASHSIZE || \
			  capabilityInfoPtr->blockSize > CRYPT_MAX_HASHSIZE ) || \
			( capabilityInfoPtr->minKeySize < MIN_KEYSIZE || \
			  capabilityInfoPtr->maxKeySize > CRYPT_MAX_KEYSIZE ) )
			{
			DEBUG_PUTS(( "sanityCheckCapability: MAC key size" ));
			return( FALSE );
			}
		if( capabilityInfoPtr->keySize > MAX_WORKING_KEYSIZE )
			{
			DEBUG_PUTS(( "sanityCheckCapability: MAC key wrap size" ));
			return( FALSE );	/* Requirement for key wrap */
			}
		if( capabilityInfoPtr->initKeyFunction == NULL )
			{
			DEBUG_PUTS(( "sanityCheckCapability: MAC init function" ));
			return( FALSE );
			}

		return( TRUE );
		}
	if( isSpecialAlgo( cryptAlgo ) )
		{
		if( capabilityInfoPtr->blockSize != 0 || \
			capabilityInfoPtr->minKeySize < bitsToBytes( 128 ) || \
			capabilityInfoPtr->maxKeySize > CRYPT_MAX_KEYSIZE )
			{
			DEBUG_PUTS(( "sanityCheckCapability: Generic key size" ));
			return( FALSE );
			}
		if( capabilityInfoPtr->initKeyFunction == NULL )
			{
			DEBUG_PUTS(( "sanityCheckCapability: Generic init function" ));
			return( FALSE );
			}

		return( TRUE );
		}

	retIntError_Boolean();
	}

/* Get information from a capability record */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void getCapabilityInfo( OUT CRYPT_QUERY_INFO *cryptQueryInfo,
						const CAPABILITY_INFO *capabilityInfoPtr )
	{
	assert( isWritePtr( cryptQueryInfo, sizeof( CRYPT_QUERY_INFO ) ) );
	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );

	memset( cryptQueryInfo, 0, sizeof( CRYPT_QUERY_INFO ) );
	REQUIRES_V( rangeCheck( capabilityInfoPtr->algoNameLen, 1, 
							CRYPT_MAX_TEXTSIZE ) );
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
const CAPABILITY_INFO *findCapabilityInfo(
						const CAPABILITY_INFO_LIST *capabilityInfoList,
						IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	const CAPABILITY_INFO_LIST *capabilityInfoListPtr;
	int LOOP_ITERATOR;

	assert( isReadPtr( capabilityInfoList, sizeof( CAPABILITY_INFO_LIST ) ) );

	/* Find the capability corresponding to the requested algorithm/mode */
	LOOP_MED( capabilityInfoListPtr = capabilityInfoList, 
			  capabilityInfoListPtr != NULL,
			  capabilityInfoListPtr = DATAPTR_GET( capabilityInfoListPtr->next ) )
		{
		const CAPABILITY_INFO *capabilityInfoPtr = \
						DATAPTR_GET( capabilityInfoListPtr->info );

		REQUIRES_N( capabilityInfoPtr != NULL );
		REQUIRES_N( sanityCheckCapability( capabilityInfoPtr ) );

		if( capabilityInfoPtr->cryptAlgo == cryptAlgo )
			return( capabilityInfoPtr );
		}
	ENSURES_N( LOOP_BOUND_OK );

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
			( length > 0 && isWritePtrDynamic( data, length ) ) );

	REQUIRES( contextInfoPtr == NULL || \
			  sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isEnumRange( type, CAPABILITY_INFO ) );

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
*						Crypto Hardware Interface Functions					*
*																			*
****************************************************************************/

/* Initialise/shut down the crypto hardware interface for a context.  This 
   is used for conventional and hash/MAC algorithms where there's no 
   explicit device, or at least for which it's not worth the overhead of 
   doing everything via a device */

#ifdef HAS_DEVCRYPTO

#include <errno.h>

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int getHWAlgoID( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
						uint32_t *hwAlgoID )
	{
	static const MAP_TABLE hwCryptInfo[] = {
		{ CRYPT_ALGO_3DES, CRYPTO_3DES_CBC },
		{ CRYPT_ALGO_AES, CRYPTO_AES_CBC },
		{ CRYPT_ALGO_SHA1, CRYPTO_SHA1 },
		{ CRYPT_ALGO_SHA2, CRYPTO_SHA2_256 },
		{ CRYPT_ERROR, CRYPT_ERROR },
			{ CRYPT_ERROR, CRYPT_ERROR }
		};

	assert( isWritePtr( hwAlgoID, sizeof( uint32_t ) ) );

	REQUIRES( isEnumRange( cryptAlgo, CRYPT_ALGO ) );

	/* Map the cryptlib algorithm ID to the /dev/crypto equivalent value */
	return( mapValue( cryptAlgo, hwAlgoID, hwCryptInfo, 
					  FAILSAFE_ARRAYSIZE( hwCryptInfo, MAP_TABLE ) ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int hwCryptoInit( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;
	struct session_op session;
	int hwAlgoID, cryptoFD, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );
	
	/* Map the cryptlib algorithm ID to the /dev/crypto equivalent value */
	status = getHWAlgoID( capabilityInfoPtr->cryptAlgo, &hwAlgoID );
	if( cryptStatusError( status ) )
		return( status );

	/* Open the crypto device and set up a session to it */
	cryptoFD = open( "/dev/crypto", O_RDWR, 0 );
	if( cryptoFD < 0 )
		return( CRYPT_ERROR_OPEN );
	fcntl( cryptoFD, F_SETFD, FD_CLOEXEC );
	memset( &session, 0, sizeof( struct session_op ) );
	if( isConvAlgo( capabilityInfoPtr->cryptAlgo ) )
		{
		CONV_INFO *convInfo = contextInfoPtr->ctxConv;

		session.cipher = hwAlgoID;
		session.key = convInfo->userKey;
		session.keylen = convInfo->userKeyLength;
		}
	else
		session.mac = hwAlgoID;
	if( ioctl( cryptoFD, CIOCGSESSION, &session ) )
		{
		close( cryptoFD );
		return( CRYPT_ERROR_NOTAVAIL );
		}

	/* Remember the hardware interface details */
	contextInfoPtr->cryptoFD = cryptoFD;
	contextInfoPtr->sessionID = session.ses;
	SET_FLAGS( contextInfoPtr->flags, 
			   CONTEXT_FLAG_DUMMY | CONTEXT_FLAG_DUMMY_INITED | \
			   CONTEXT_FLAG_HWCRYPTO );

	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int hwCryptoEnd( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_HWCRYPTO ) );

	/* Shut down the crypto session and close the device handle */
	if( contextInfoPtr->cryptoFD > 0 )
		{
		( void ) ioctl( contextInfoPtr->cryptoFD, CIOCFSESSION, 
						&contextInfoPtr->sessionID );
		close( contextInfoPtr->cryptoFD );
		}
	contextInfoPtr->cryptoFD = 0;
	contextInfoPtr->sessionID = 0;

	return( CRYPT_OK );
	}

/* Clone the hardware crypto state, used as part of a context-clone 
   operation */

#ifdef CIOCCPHASH

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int hwCryptoCloneState( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;
	struct session_op session;
	const uint32_t oldSessionID = contextInfoPtr->sessionID;
	const int oldCryptoFD = contextInfoPtr->cryptoFD;
	int cryptoFD, hwAlgoID, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* Since we're referencing the original context's crypto FD and session
	   ID, we clear them before continuing to make sure that if any of the
	   following operations fail the original context's crypto session won't
	   be cleaned up */
	contextInfoPtr->cryptoFD = 0;
	contextInfoPtr->sessionID = 0;

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );

	/* /dev/crypto currently doesn't support copying state from a 
	   conventional-encryption session, only a hash session.  To deal with
	   this we change the context back from a hardware crypto one which
	   can't be cloned to a software one, which can.  
	   
	   This particular situation virtually never occurs since it's only 
	   triggered when cloning a user-supplied raw session-key object in an 
	   envelope (pgp_env.c and res_env.c), and those are essentially never 
	   used.  In any case given the slowdown created by using hardware 
	   crypto, this probably isn't such a bad thing... */
	if( isConvAlgo( capabilityInfoPtr->cryptAlgo ) )
		{
		CONV_INFO *convInfo = contextInfoPtr->ctxConv;

		CLEAR_FLAGS( contextInfoPtr->flags, 
					 CONTEXT_FLAG_DUMMY | CONTEXT_FLAG_DUMMY_INITED | \
					 CONTEXT_FLAG_HWCRYPTO );
		return( capabilityInfoPtr->initKeyFunction( contextInfoPtr, 
						convInfo->userKey, convInfo->userKeyLength ) );
		}

	/* Map the cryptlib algorithm ID to the /dev/crypto equivalent value */
	status = getHWAlgoID( capabilityInfoPtr->cryptAlgo, &hwAlgoID );
	if( cryptStatusError( status ) )
		return( status );

	/* Create a second session and copy the state from the first one into 
	   it.  We have to dup() the crypto FD and initialise a second session
	   on it rather than creating a fresh FD and session via hwCryptoInit() 
	   because session IDs aren't valid across FDs */
	cryptoFD = dup( oldCryptoFD );
	if( cryptoFD < 0 )
		return( CRYPT_ERROR_FAILED );
	memset( &session, 0, sizeof( struct session_op ) );
	if( isConvAlgo( capabilityInfoPtr->cryptAlgo ) )
		{
		CONV_INFO *convInfo = contextInfoPtr->ctxConv;

		session.cipher = hwAlgoID;
		session.key = convInfo->userKey;
		session.keylen = convInfo->userKeyLength;
		DEBUG_DIAG(( "Trying to clone context for conventional algo %d, this "
					 "will fail with the current version of /dev/crypto", 
					 capabilityInfoPtr->cryptAlgo ));
		}
	else
		session.mac = hwAlgoID;
	status = ioctl( cryptoFD, CIOCGSESSION, &session );
	if( status == 0 )
		{
		struct cphash_op copyInfo;

		memset( &copyInfo, 0, sizeof( struct cphash_op ) );
		copyInfo.src_ses = oldSessionID;
		copyInfo.dst_ses = session.ses;
		status = ioctl( cryptoFD, CIOCCPHASH, &copyInfo );
		}
	if( status != 0 )
		{
		DEBUG_DIAG(( "CIOCCPHASH ioctl failed, errno = %d", errno ));
		close( cryptoFD );

		return( CRYPT_ERROR_FAILED );
		}
	contextInfoPtr->cryptoFD = cryptoFD;
	contextInfoPtr->sessionID = session.ses;

	return( CRYPT_OK );
	}
#else

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int hwCryptoCloneState( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	return( CRYPT_ERROR_NOTAVAIL );
	}
#endif /* CIOCCPHASH */

/* Push data through the hardware cryptologic */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int hwCryptoHash( INOUT CONTEXT_INFO *contextInfoPtr,
				  IN_BUFFER( noBytes ) BYTE *buffer, 
				  IN_LENGTH_Z int noBytes )
	{
	struct crypt_op cryptOpInfo;
	int cryptoFlags = COP_FLAG_NONE;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_HWCRYPTO ) );

	/* Set up the processing flags as required */
	if( !TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_HASH_INITED ) )
		cryptoFlags |= COP_FLAG_RESET | COP_FLAG_UPDATE;
	if( noBytes > 0 )
		cryptoFlags |= COP_FLAG_UPDATE;
	else
		cryptoFlags |= COP_FLAG_FINAL;

	/* Set up the crypto info and send it to the cryptologic */	
	memset( &cryptOpInfo, 0, sizeof( struct crypt_op ) );
	cryptOpInfo.ses = contextInfoPtr->sessionID;
	cryptOpInfo.op = COP_ENCRYPT;
	cryptOpInfo.flags = cryptoFlags;
	cryptOpInfo.src = buffer;
	cryptOpInfo.len = noBytes;
	if( noBytes == 0 )
		cryptOpInfo.mac = contextInfoPtr->ctxHash->hash;
	if( ioctl( contextInfoPtr->cryptoFD, CIOCCRYPT, &cryptOpInfo ) ) 
		{
		DEBUG_DIAG(( "CIOCCRYPT hash ioctl failed, errno = %d", errno ));
		return( CRYPT_ERROR_FAILED );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int hwCryptoCrypt( INOUT CONTEXT_INFO *contextInfoPtr,
				   IN_BUFFER( noBytes ) BYTE *buffer, 
				   IN_LENGTH_Z int noBytes,
				   const BOOLEAN doEncrypt )
	{
	struct crypt_op cryptOpInfo;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_HWCRYPTO ) );

	/* Set up the crypto info and send it to the cryptologic */	
	memset( &cryptOpInfo, 0, sizeof( struct crypt_op ) );
	cryptOpInfo.ses = contextInfoPtr->sessionID;
	cryptOpInfo.op = doEncrypt ? COP_ENCRYPT : COP_DECRYPT;
	cryptOpInfo.flags = COP_FLAG_WRITE_IV;
	cryptOpInfo.src = cryptOpInfo.dst = buffer;
	cryptOpInfo.len = noBytes;
	cryptOpInfo.iv = contextInfoPtr->ctxConv->currentIV;
	if( ioctl( contextInfoPtr->cryptoFD, CIOCCRYPT, &cryptOpInfo ) ) 
		{
		DEBUG_DIAG(( "CIOCCRYPT crypt ioctl failed, errno = %d", errno ));
		return( CRYPT_ERROR_FAILED );
		}

	return( CRYPT_OK );
	}
#endif /* HAS_DEVCRYPTO */

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
	assert( isReadPtrDynamic( contextData, contextDataSize ) );

	REQUIRES( isEnumRange( type, CONTEXT ) );
	REQUIRES( contextDataSize >= 32 && \
			  ( ( type != CONTEXT_PKC && \
				  contextDataSize < MAX_INTLENGTH_SHORT ) || \
				( type == CONTEXT_PKC && \
				  contextDataSize < MAX_INTLENGTH ) ) );

	memset( contextInfoPtr, 0, sizeof( CONTEXT_INFO ) );
	memset( contextData, 0, contextDataSize );
	contextInfoPtr->type = type;
	DATAPTR_SET( contextInfoPtr->capabilityInfo, ( void * ) capabilityInfoPtr );
	INIT_FLAGS( contextInfoPtr->flags, CONTEXT_FLAG_STATICCONTEXT );
	switch( type )
		{
		case CONTEXT_CONV:
			REQUIRES( keyData == ptr_align( keyData, 8 ) || \
					  keyData == ptr_align( keyData, 16 ) );
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
			{
			PKC_INFO *pkcInfo = contextData;

			/* PKC context initialisation is a bit more complex because we
			   have to set up all of the bignum values as well.  Since static
			   contexts are only used for self-test operations we set the 
			   side-channel protection level to zero */
			contextInfoPtr->ctxPKC = pkcInfo;
			memset( pkcInfo, 0, sizeof( PKC_INFO ) );
			INIT_FLAGS( pkcInfo->flags, PKCINFO_FLAG_NONE );
			status = initContextBignums( pkcInfo, 
							isEccAlgo( capabilityInfoPtr->cryptAlgo ) ? \
								TRUE : FALSE );
			if( cryptStatusError( status ) )
				return( status );
			initKeyID( contextInfoPtr );
			initPubKeyRead( contextInfoPtr );
			initPrivKeyRead( contextInfoPtr );
			initKeyWrite( contextInfoPtr );		/* For calcKeyID() */
			break;
			}

		default:
			retIntError();
		}

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void staticDestroyContext( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	ENSURES_V( TEST_FLAG( contextInfoPtr->flags, 
						  CONTEXT_FLAG_STATICCONTEXT ) );

	/* If this is a context that's implemented via built-in crypto hardware, 
	   perform any required cleanup */
#ifdef HAS_DEVCRYPTO
	if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_HWCRYPTO ) )
		hwCryptoEnd( contextInfoPtr );
#endif /* HAS_DEVCRYPTO */

	if( contextInfoPtr->type == CONTEXT_PKC )
		{
		endContextBignums( contextInfoPtr->ctxPKC, 
						   GET_FLAG( contextInfoPtr->flags, 
									 CONTEXT_FLAG_DUMMY ) ? \
								TRUE : FALSE );
		}
	memset( contextInfoPtr, 0, sizeof( CONTEXT_INFO ) );
	}

/* Perform a self-test of a cipher, encrypting and decrypting one block of 
   data and comparing it to a fixed test value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 6 ) ) \
int testCipher( IN const CAPABILITY_INFO *capabilityInfo, 
				IN const void *keyDataStorage, 
				IN_BUFFER( keySize ) const void *key, 
				IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) const int keySize, 
				IN const void *plaintext,
				IN const void *ciphertext )
	{
	CONTEXT_INFO contextInfo;
	CONV_INFO contextData;
	BYTE temp[ CRYPT_MAX_IVSIZE + 8 ];
	int status;

	assert( isReadPtr( capabilityInfo, sizeof( CAPABILITY_INFO ) ) );
	assert( isReadPtr( keyDataStorage, 16 ) );
	assert( isReadPtrDynamic( key, keySize ) );
	assert( isReadPtrDynamic( plaintext, capabilityInfo->blockSize ) );
	assert( isReadPtrDynamic( ciphertext, capabilityInfo->blockSize ) );

	REQUIRES( capabilityInfo->encryptFunction != NULL && \
			  capabilityInfo->decryptFunction != NULL );
	REQUIRES( keySize >= MIN_KEYSIZE && keySize <= CRYPT_MAX_KEYSIZE );

	REQUIRES( rangeCheck( capabilityInfo->blockSize, 1, 
						  CRYPT_MAX_IVSIZE ) );
	memcpy( temp, plaintext, capabilityInfo->blockSize );

	status = staticInitContext( &contextInfo, CONTEXT_CONV, capabilityInfo,
								&contextData, sizeof( CONV_INFO ), 
								keyDataStorage );
	if( cryptStatusError( status ) )
		return( status );
	status = capabilityInfo->initKeyFunction( &contextInfo, key, keySize );
	if( cryptStatusOK( status ) )
		{
		status = capabilityInfo->encryptFunction( &contextInfo, temp, 
												  capabilityInfo->blockSize );
		}
	if( cryptStatusOK( status ) && \
		memcmp( ciphertext, temp, capabilityInfo->blockSize ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusOK( status ) )
		{
		status = capabilityInfo->decryptFunction( &contextInfo, temp, 
												  capabilityInfo->blockSize );
		}
	staticDestroyContext( &contextInfo );
	if( cryptStatusError( status ) || \
		memcmp( plaintext, temp, capabilityInfo->blockSize ) )
		return( CRYPT_ERROR_FAILED );
	
	return( CRYPT_OK );
	}

/* Perform a self-test of a hash or MAC */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 6 ) ) \
int testHash( IN const CAPABILITY_INFO *capabilityInfo, 
			  IN_LENGTH_HASH_Z const int hashSize,
			  IN const void *hashDataStorage,
			  IN_BUFFER_OPT( dataLength ) const void *data, 
			  IN_LENGTH_SHORT_Z const int dataLength, 
			  IN const void *hashValue )
	{
	CONTEXT_INFO contextInfo;
	HASH_INFO contextData;
	int status;

	assert( isReadPtr( capabilityInfo, sizeof( CAPABILITY_INFO ) ) );
	assert( isReadPtr( hashDataStorage, 16 ) );
	assert( ( data == NULL && dataLength == 0 ) || \
			isReadPtrDynamic( data, dataLength ) );
	assert( isReadPtrDynamic( hashValue, capabilityInfo->blockSize ) );

	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( data != NULL && \
				isShortIntegerRangeNZ( dataLength ) ) );
	REQUIRES( hashSize == 0 || \
			  ( hashSize >= MIN_HASHSIZE && hashSize <= CRYPT_MAX_HASHSIZE ) );

	status = staticInitContext( &contextInfo, CONTEXT_HASH, capabilityInfo,
								&contextData, sizeof( HASH_INFO ), 
								hashDataStorage );
	if( cryptStatusError( status ) )
		return( status );
	if( hashSize != 0 )
		{
		status = capabilityInfo->initParamsFunction( &contextInfo, 
													 KEYPARAM_BLOCKSIZE, NULL,
													 hashSize );
		}
	if( cryptStatusOK( status ) && data != NULL )
		{
		/* Some of the test vector sets start out with empty strings so we 
		   only call the hash function if we've actually been fed data to 
		   hash */
		status = capabilityInfo->encryptFunction( &contextInfo, 
												  ( void * ) data, 
												  dataLength );
		SET_FLAG( contextInfo.flags, CONTEXT_FLAG_HASH_INITED );
		}
	if( cryptStatusOK( status ) )
		{
		status = capabilityInfo->encryptFunction( &contextInfo, 
												  MKDATA( "" ), 0 );
		}
	if( cryptStatusOK( status ) && \
		memcmp( contextInfo.ctxHash->hash, hashValue, 
				capabilityInfo->blockSize ) )
		status = CRYPT_ERROR_FAILED;
	staticDestroyContext( &contextInfo );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 7 ) ) \
int testMAC( IN const CAPABILITY_INFO *capabilityInfo, 
			 IN const void *macDataStorage,
			 IN_BUFFER( keySize ) const void *key, 
			 IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) const int keySize, 
			 IN_BUFFER( dataLength ) const void *data, 
			 IN_LENGTH_SHORT_MIN( 8 ) const int dataLength,
			 IN const void *hashValue )
	{
	CONTEXT_INFO contextInfo;
	MAC_INFO contextData;
	int status;

	assert( isReadPtr( capabilityInfo, sizeof( CAPABILITY_INFO ) ) );
	assert( isReadPtr( macDataStorage, 16 ) );
	assert( isReadPtrDynamic( key, keySize ) );
	assert( isReadPtrDynamic( data, dataLength ) );
	assert( isReadPtrDynamic( hashValue, capabilityInfo->blockSize ) );

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
		SET_FLAG( contextInfo.flags, CONTEXT_FLAG_HASH_INITED );
		}
	if( cryptStatusOK( status ) )
		{
		status = capabilityInfo->encryptFunction( &contextInfo, 
												  MKDATA( "" ), 0 );
		}
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
	const HASH_FUNCTION function;
	} HASHFUNCTION_INFO;

typedef struct HAI {
	const CRYPT_ALGO_TYPE cryptAlgo;
	const int hashSize;
	const HASH_FUNCTION_ATOMIC function;
	} HASHFUNCTION_ATOMIC_INFO;

STDC_NONNULL_ARG( ( 3 ) ) \
void getHashParameters( IN_ALGO const CRYPT_ALGO_TYPE hashAlgorithm,
						IN_INT_SHORT_Z const int hashParam,
						OUT_PTR HASH_FUNCTION *hashFunction, 
						OUT_OPT_LENGTH_SHORT_Z int *hashOutputSize )
	{
	static const HASHFUNCTION_INFO hashFunctions[] = {
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
	int i, LOOP_ITERATOR;

	assert( isHashAlgo( hashAlgorithm ) );
	assert( isShortIntegerRange( hashParam ) );
			/* We don't use REQUIRES() for this for the reason given in the
			   comments below */
	assert( isWritePtr( hashFunction, sizeof( HASH_FUNCTION ) ) );
	assert( ( hashOutputSize == NULL ) || \
			isWritePtr( hashOutputSize, sizeof( int ) ) );

	/* Fast-path for SHA-1, which is almost always the one that we're being 
	   asked for */
	if( hashAlgorithm == CRYPT_ALGO_SHA1 )
		{
		*hashFunction = shaHashBuffer;
		if( hashOutputSize != NULL )
			*hashOutputSize = SHA_DIGEST_LENGTH;
		return;
		}

	/* Clear return value */
	if( hashOutputSize != NULL )
		*hashOutputSize = 0;

	/* Find the information for the requested hash algorithm */
	LOOP_SMALL( i = 0, hashFunctions[ i ].cryptAlgo != CRYPT_ALGO_NONE && \
					   i < FAILSAFE_ARRAYSIZE( hashFunctions, HASHFUNCTION_INFO ), 
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
	if( !LOOP_BOUND_OK || \
		i >= FAILSAFE_ARRAYSIZE( hashFunctions, HASHFUNCTION_INFO ) || \
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
							  OUT_PTR HASH_FUNCTION_ATOMIC *hashFunctionAtomic, 
							  OUT_OPT_LENGTH_SHORT_Z int *hashOutputSize )
	{
	static const HASHFUNCTION_ATOMIC_INFO hashFunctions[] = {
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
	int i, LOOP_ITERATOR;

	assert( isHashAlgo( hashAlgorithm ) );
	assert( isShortIntegerRange( hashParam ) );
			/* We don't use REQUIRES() for this for the reason given in the
			   comments below */
	assert( isWritePtr( hashFunctionAtomic, sizeof( HASH_FUNCTION_ATOMIC ) ) );
	assert( ( hashOutputSize == NULL ) || \
			isWritePtr( hashOutputSize, sizeof( int ) ) );

	/* Fast-path for SHA-1, which is almost always the one that we're being 
	   asked for */
	if( hashAlgorithm == CRYPT_ALGO_SHA1 )
		{
		*hashFunctionAtomic = shaHashBufferAtomic;
		if( hashOutputSize != NULL )
			*hashOutputSize = SHA_DIGEST_LENGTH;
		return;
		}

	/* Clear return value */
	if( hashOutputSize != NULL )
		*hashOutputSize = 0;

	/* Find the information for the requested hash algorithm */
	LOOP_SMALL( i = 0, hashFunctions[ i ].cryptAlgo != CRYPT_ALGO_NONE && \
					   i < FAILSAFE_ARRAYSIZE( hashFunctions, HASHFUNCTION_ATOMIC_INFO ), 
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
	if( !LOOP_BOUND_OK || \
		i >= FAILSAFE_ARRAYSIZE( hashFunctions, HASHFUNCTION_ATOMIC_INFO ) || \
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
