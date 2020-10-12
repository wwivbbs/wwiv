/****************************************************************************
*																			*
*					  cryptlib Encryption Context Routines					*
*						Copyright Peter Gutmann 1992-2016					*
*																			*
****************************************************************************/

/* "Modern cryptography is nothing more than a mathematical framework for
	debating the implications of various paranoid delusions"
												- Don Alvarez */

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#include "crypt.h"
#ifdef INC_ALL
  #include "context.h"
  #include "asn1.h"
#else
  #include "context/context.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

/* When we're allocating subtype-specific data to be stored alongside the
   main CONTEXT_INFO, we align it to a certain block size both for efficient
   access and to ensure that the pointer to it from the main CONTEXT_INFO
   doesn't result in a segfault.  The following works for all current 
   processor types */

#define STORAGE_ALIGN_SIZE	8
#define CONTEXT_INFO_ALIGN_SIZE	\
		roundUp( sizeof( CONTEXT_INFO ), STORAGE_ALIGN_SIZE )
#define ALIGN_CONTEXT_PTR( basePtr, type ) \
		( type * ) ( ( BYTE * ) ( basePtr ) + CONTEXT_INFO_ALIGN_SIZE )

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check context data */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkDataItem( IN_BUFFER( dataLen ) const void *data, 
							  IN const int dataLen,
							  IN_LENGTH_SHORT_MIN( 8 ) const int dataMaxLen )
	{
	assert( isReadPtr( data, dataLen ) );

	REQUIRES_B( dataMaxLen >= 8 && dataMaxLen <= MAX_INTLENGTH_SHORT );

	/* If there's no data present, we're done */
	if( isEmptyData( data, dataLen ) )
		return( TRUE );

	/* Check that the data length is within the allowed range */
	if( dataLen <= 0 || dataLen > dataMaxLen ) 
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckContext( const CONTEXT_INFO *contextInfoPtr )
	{
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* Check general context data */
	if( !isEnumRange( contextInfoPtr->type, CONTEXT ) || \
		!CHECK_FLAGS( contextInfoPtr->flags, CONTEXT_FLAG_NONE, 
					  CONTEXT_FLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckContext: General info" ));
		return( FALSE );
		}

	/* Check safe pointers */
	if( !DATAPTR_ISVALID( contextInfoPtr->capabilityInfo ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Safe pointers" ));
		return( FALSE );
		}

	/* Check associated handles */
	if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_STATICCONTEXT ) )
		{
		if( contextInfoPtr->objectHandle != 0 || \
			contextInfoPtr->ownerHandle != 0 )
			{
			DEBUG_PUTS(( "sanityCheckContext: Spurious object handles" ));
			return( FALSE );
			}
		}
	else
		{
		if( !isHandleRangeValid( contextInfoPtr->objectHandle ) || \
			!( contextInfoPtr->ownerHandle == DEFAULTUSER_OBJECT_HANDLE || \
			   isHandleRangeValid( contextInfoPtr->ownerHandle ) ) )
			{
			DEBUG_PUTS(( "sanityCheckContext: Object handles" ));
			return( FALSE );
			}
		}

	/* Check subtype-specific data */
	switch( contextInfoPtr->type )
		{
		case CONTEXT_CONV:
			{
			const CONV_INFO *convInfo;

			/* Check context info storage.  See the discussion in 
			   initContextStorage() for why we check for only 8- and 16-byte 
			   alignment */
			if( !TEST_FLAG( contextInfoPtr->flags, 
							CONTEXT_FLAG_STATICCONTEXT ) )
				{
				if( contextInfoPtr->ctxConv != \
						ALIGN_CONTEXT_PTR( contextInfoPtr, CONV_INFO ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: Crypto context storage" ));
					return( FALSE );
					}
				convInfo = contextInfoPtr->ctxConv;
				if( convInfo->key != \
						ptr_align( ( BYTE * ) convInfo + sizeof( CONV_INFO ), 8 ) && \
					convInfo->key != \
						ptr_align( ( BYTE * ) convInfo + sizeof( CONV_INFO ), 16 ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: Crypto context key storage" ));
					return( FALSE );
					}
				}
			else
				{
				/* Statically-initialised contexts don't have the subtype-
				   specific information contiguous with the main context 
				   storage, but still need to have aligned key storage 
				   memory */
				convInfo = contextInfoPtr->ctxConv;
				if( convInfo->key != ptr_align( convInfo->key, 8 ) && \
					convInfo->key != ptr_align( convInfo->key, 16 ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: Crypto context key storage" ));
					return( FALSE );
					}
				}

			assert( isReadPtr( convInfo, sizeof( CONV_INFO ) ) );

			/* Check general conventional-context data.  The encryption mode
			   can be CRYPT_MODE_NONE until it's explicitly set */
			if( !isEnumRangeOpt( convInfo->mode, CRYPT_MODE ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: Crypto mode" ));
				return( FALSE );
				}

			/* Check key and IV data */
			if( !checkDataItem( convInfo->userKey, convInfo->userKeyLength, 
								CRYPT_MAX_KEYSIZE ) || \
				!checkDataItem( convInfo->iv, convInfo->ivLength, 
								CRYPT_MAX_IVSIZE ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: Crypto key/IV" ));
				return( FALSE );
				}

			/* Check PRF data */
			if( !checkDataItem( convInfo->salt, convInfo->saltLength,
								CRYPT_MAX_HASHSIZE ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: Crypto salt" ));
				return( FALSE );
				}
			if( convInfo->keySetupIterations < 0 || \
				convInfo->keySetupIterations > MAX_KEYSETUP_ITERATIONS )
				{
				DEBUG_PUTS(( "sanityCheckContext: Crypto key setup iterations" ));
				return( FALSE );
				}
			if( convInfo->keySetupAlgorithm != CRYPT_ALGO_NONE && \
				!isHashAlgo( convInfo->keySetupAlgorithm ) && \
				!isMacAlgo( convInfo->keySetupAlgorithm ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: Crypto key setup algorithm" ));
				return( FALSE );
				}

			break;
			}

		case CONTEXT_PKC:
			{
			const PKC_INFO *pkcInfo;

			/* Check context info storage */
			if( !TEST_FLAG( contextInfoPtr->flags, 
							CONTEXT_FLAG_STATICCONTEXT ) )
				{
				if( contextInfoPtr->ctxPKC != \
						ALIGN_CONTEXT_PTR( contextInfoPtr, PKC_INFO ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: PKC context storage" ));
					return( FALSE );
					}
				}
			pkcInfo = contextInfoPtr->ctxPKC;

			assert( isReadPtr( pkcInfo, sizeof( PKC_INFO ) ) );

			/* The PKC info is sufficiently complex that it has its own
			   checking function */
			if( !sanityCheckPKCInfo( pkcInfo ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: PKC info" ));
				return( FALSE );
				}

			break;
			}

		case CONTEXT_HASH:
			{
			const HASH_INFO *hashInfo;

			/* Check context info storage */
			if( !TEST_FLAG( contextInfoPtr->flags, 
							CONTEXT_FLAG_STATICCONTEXT ) )
				{
				if( contextInfoPtr->ctxHash != \
						ALIGN_CONTEXT_PTR( contextInfoPtr, HASH_INFO ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: Hash context storage" ));
					return( FALSE );
					}
				hashInfo = contextInfoPtr->ctxHash;
				if( hashInfo->hashInfo != \
						ptr_align( ( BYTE * ) hashInfo + sizeof( HASH_INFO ), 8 ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: Hash context state storage" ));
					return( FALSE );
					}
				}
			else
				{
				/* Statically-initialised contexts don't have the subtype-
				   specific information contiguous with the main context 
				   storage, but still need to have aligned key storage 
				   memory */
				hashInfo = contextInfoPtr->ctxHash;
				if( hashInfo->hashInfo != ptr_align( hashInfo->hashInfo, 8 ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: Hash context state storage" ));
					return( FALSE );
					}
				}

			assert( isReadPtr( hashInfo, sizeof( HASH_INFO ) ) );

			break;
			}

		case CONTEXT_MAC:
			{
			const MAC_INFO *macInfo;

			/* Check context info storage */
			if( !TEST_FLAG( contextInfoPtr->flags, 
							CONTEXT_FLAG_STATICCONTEXT ) )
				{
				if( contextInfoPtr->ctxMAC != \
						ALIGN_CONTEXT_PTR( contextInfoPtr, MAC_INFO ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: MAC context storage" ));
					return( FALSE );
					}
				macInfo = contextInfoPtr->ctxMAC;
				if( macInfo->macInfo != \
						ptr_align( ( BYTE * ) macInfo + sizeof( MAC_INFO ), 8 ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: MAC context state storage" ));
					return( FALSE );
					}
				}
			else
				{
				/* Statically-initialised contexts don't have the subtype-
				   specific information contiguous with the main context 
				   storage, but still need to have aligned key storage 
				   memory */
				macInfo = contextInfoPtr->ctxMAC;
				if( macInfo->macInfo != ptr_align( macInfo->macInfo, 8 ) )
					{
					DEBUG_PUTS(( "sanityCheckContext: MAC context state storage" ));
					return( FALSE );
					}
				}

			assert( isReadPtr( macInfo, sizeof( MAC_INFO ) ) );

			/* Check key data */
			if( !checkDataItem( macInfo->userKey, macInfo->userKeyLength, 
								CRYPT_MAX_KEYSIZE ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: MAC key" ));
				return( FALSE );
				}

			/* Check PRF data */
			if( !checkDataItem( macInfo->salt, macInfo->saltLength, 
								CRYPT_MAX_HASHSIZE ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: MAC salt" ));
				return( FALSE );
				}
			if( macInfo->keySetupIterations < 0 || \
				macInfo->keySetupIterations > MAX_KEYSETUP_ITERATIONS )
				{
				DEBUG_PUTS(( "sanityCheckContext: MAC key setup iterations" ));
				return( FALSE );
				}
			if( macInfo->keySetupAlgorithm != CRYPT_ALGO_NONE && \
				( macInfo->keySetupAlgorithm < CRYPT_ALGO_FIRST_MAC || \
				  macInfo->keySetupAlgorithm > CRYPT_ALGO_LAST_MAC ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: MAC key setup algorithm" ));
				return( FALSE );
				}

			break;
			}

		case CONTEXT_GENERIC:
			{
			const GENERIC_INFO *genericInfo;

			/* Check context info storage */
			if( contextInfoPtr->ctxGeneric != \
							ALIGN_CONTEXT_PTR( contextInfoPtr, GENERIC_INFO ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: Generic context storage" ));
				return( FALSE );
				}
			genericInfo = contextInfoPtr->ctxGeneric;

			assert( isReadPtr( genericInfo, sizeof( GENERIC_INFO ) ) );

			/* Check key data */
			if( !checkDataItem( genericInfo->genericSecret, 
								genericInfo->genericSecretLength, 
								CRYPT_MAX_KEYSIZE ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: Generic secret" ));
				return( FALSE );
				}

			/* Check KDF data */
			if( !checkDataItem( genericInfo->kdfParams, 
								genericInfo->kdfParamSize,
								CRYPT_MAX_TEXTSIZE ) || \
				!checkDataItem( genericInfo->encAlgoParams, 
								genericInfo->encAlgoParamSize, 
								CRYPT_MAX_TEXTSIZE ) || \
				!checkDataItem( genericInfo->macAlgoParams, 
								genericInfo->macAlgoParamSize, 
								CRYPT_MAX_TEXTSIZE ) )
				{
				DEBUG_PUTS(( "sanityCheckContext: Generic secret parameters" ));
				return( FALSE );
				}

			break;
			}

		default:
			retIntError_Boolean();
		}

	return( TRUE );
	}

/* Check that context function pointers have been set up correctly */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkContextFunctions( IN const CONTEXT_INFO *contextInfoPtr )
	{
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* We can't call sanityCheckContext() at this point because the storage
	   for the context data that we want to sanity-check hasn't been set
	   up yet.  The best that we can do is perform a subset of the standard
	   sanity checks */ 

	switch( contextInfoPtr->type )
		{
		case CONTEXT_CONV:
			if( !FNPTR_ISSET( contextInfoPtr->loadKeyFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->generateKeyFunction ) )
				return( FALSE );
			if( !FNPTR_ISSET( contextInfoPtr->encryptFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->decryptFunction ) )
				return( FALSE );
			break;

		case CONTEXT_PKC:
			{
			const CAPABILITY_INFO *capabilityInfoPtr;

			/* Get the capability info for the context */
			capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
			REQUIRES( capabilityInfoPtr != NULL );

			if( !FNPTR_ISSET( contextInfoPtr->loadKeyFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->generateKeyFunction ) )
				return( FALSE );
			switch( capabilityInfoPtr->cryptAlgo )
				{
				case CRYPT_ALGO_RSA:
					/* RSA can get a bit complicated because the same 
					   operation is used for both sign/verify and decrypt/
					   encrypt, and if the context doesn't support 
					   encryption (for example because it's tied to a 
					   signing-only hardware device) then the absence of an 
					   encrypt/decrypt capability isn't an error */
					if( !( FNPTR_ISSET( contextInfoPtr->encryptFunction ) && \
						   FNPTR_ISSET( contextInfoPtr->decryptFunction ) ) && \
						!( capabilityInfoPtr->signFunction != NULL && \
						   capabilityInfoPtr->sigCheckFunction != NULL ) )
						return( FALSE );
					break;

				case CRYPT_ALGO_DSA:
				case CRYPT_ALGO_ECDSA:
					if( capabilityInfoPtr->signFunction == NULL || \
						capabilityInfoPtr->sigCheckFunction == NULL )
						return( FALSE );
					break;

				default:
					if( !FNPTR_ISSET( contextInfoPtr->encryptFunction ) || \
						!FNPTR_ISSET( contextInfoPtr->decryptFunction ) )
						return( FALSE );
				}
			if( !FNPTR_ISSET( contextInfoPtr->ctxPKC->writePublicKeyFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->ctxPKC->writePrivateKeyFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->ctxPKC->readPublicKeyFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->ctxPKC->readPrivateKeyFunction ) )
				return( FALSE );
			break;
			}

		case CONTEXT_HASH:
			if( !FNPTR_ISSET( contextInfoPtr->encryptFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->decryptFunction ) )
				return( FALSE );
			break;

		case CONTEXT_MAC:
			if( !FNPTR_ISSET( contextInfoPtr->loadKeyFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->generateKeyFunction ) )
				return( FALSE );
			if( !FNPTR_ISSET( contextInfoPtr->encryptFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->decryptFunction ) )
				return( FALSE );
			break;

		case CONTEXT_GENERIC:
			if( !FNPTR_ISSET( contextInfoPtr->loadKeyFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->generateKeyFunction ) )
				return( FALSE );
			break;

		default:
			retIntError();
		}

	return( TRUE );
	}

/* Perform any context-specific checks that a context meets the given 
   requirements (general checks have already been performed by the kernel).  
   Although these checks are automatically performed by the kernel when we 
   try and use the context, they're duplicated here to allow for better 
   error reporting by catching problems when the context is first passed to 
   a cryptlib function rather than much later and at a lower level when the 
   kernel disallows the action */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkContext( INOUT CONTEXT_INFO *contextInfoPtr,
						 IN_ENUM( MESSAGE_CHECK ) \
							const MESSAGE_CHECK_TYPE checkType )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isEnumRange( checkType, MESSAGE_CHECK ) );

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );

	/* If it's a check that an object's ready for key generation then we can 
	   perform the check without requiring any algorithm-specific 
	   gyrations */
	if( checkType == MESSAGE_CHECK_KEYGEN_READY )
		{
		/* Make sure that there isn't already a key loaded */
		if( !needsKey( contextInfoPtr ) )
			return( CRYPT_ERROR_INITED );

		/* Make sure that we can actually generate a key.  This should be
		   enforced by the kernel anyway but we use a backup check here */
		if( capabilityInfoPtr->generateKeyFunction == NULL )
			return( CRYPT_ERROR_NOTAVAIL );

		return( CRYPT_OK );
		}

	/* If it's a check for the (potential) ability to perform conventional 
	   encryption or MACing at some point in the future, without necessarily 
	   currently having a key loaded for the task, we're done */
	if( checkType == MESSAGE_CHECK_CRYPT_READY || \
		checkType == MESSAGE_CHECK_MAC_READY )
		return( CRYPT_OK );

	/* Perform general checks */
	if( contextInfoPtr->type != CONTEXT_HASH && needsKey( contextInfoPtr ) )
		return( CRYPT_ERROR_NOTINITED );

	/* If it's a hash, MAC, conventional encryption, or basic PKC check, 
	   we're done */
	if( checkType == MESSAGE_CHECK_CRYPT || \
		checkType == MESSAGE_CHECK_HASH || \
		checkType == MESSAGE_CHECK_MAC || \
		checkType == MESSAGE_CHECK_PKC )
		return( CRYPT_OK );

	/* Check for key-agreement algorithms */
	if( isKeyxAlgo( capabilityInfoPtr->cryptAlgo ) )
		{
		/* (EC)DH can never be used for encryption or signatures (if it is 
		   then we call it Elgamal).  Note that the status of (EC)DH is a 
		   bit ambiguous in that every (EC)DH key is both a public and 
		   private key, in order to avoid confusion in situations where 
		   we're checking for real private keys we always denote a (EC)DH 
		   context as key-agreement only without taking a side about 
		   whether it's a public or private key */
		return( ( checkType == MESSAGE_CHECK_PKC_KA_EXPORT || \
				  checkType == MESSAGE_CHECK_PKC_KA_IMPORT ) ? \
				CRYPT_OK : CRYPT_ARGERROR_OBJECT );
		}
	if( checkType == MESSAGE_CHECK_PKC_KA_EXPORT || \
		checkType == MESSAGE_CHECK_PKC_KA_IMPORT )
		{
		/* A key agreement check requires a key agreement algorithm */
		return( CRYPT_ARGERROR_OBJECT );
		}

	/* We're down to various public-key checks */
	REQUIRES( checkType == MESSAGE_CHECK_PKC_PRIVATE || \
			  checkType == MESSAGE_CHECK_PKC_ENCRYPT || \
			  checkType == MESSAGE_CHECK_PKC_DECRYPT || \
			  checkType == MESSAGE_CHECK_PKC_SIGCHECK || \
			  checkType == MESSAGE_CHECK_PKC_SIGN || \
			  checkType == MESSAGE_CHECK_PKC_SIGCHECK_CA || \
			  checkType == MESSAGE_CHECK_PKC_SIGN_CA || \
			  checkType == MESSAGE_CHECK_CERT );

	/* Check that it's a private key if this is required */
	if( ( checkType == MESSAGE_CHECK_PKC_PRIVATE || \
		  checkType == MESSAGE_CHECK_PKC_DECRYPT || \
		  checkType == MESSAGE_CHECK_PKC_SIGN ) && \
		TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY ) )
		return( CRYPT_ARGERROR_OBJECT );

	return( CRYPT_OK );
	}

/* Process an attribute compare message */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processCompareMessage( INOUT CONTEXT_INFO *contextInfoPtr, 
								  IN_ENUM( MESSAGE_COMPARE ) \
										const MESSAGE_COMPARE_TYPE message,
								  IN_BUFFER( dataLength ) const void *data,
								  IN_LENGTH_SHORT const int dataLength )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( message == MESSAGE_COMPARE_HASH || \
			  message == MESSAGE_COMPARE_ICV || \
			  message == MESSAGE_COMPARE_KEYID || \
			  message == MESSAGE_COMPARE_KEYID_PGP || \
			  message == MESSAGE_COMPARE_KEYID_OPENPGP );
	REQUIRES( isShortIntegerRangeNZ( dataLength ) );

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );

	switch( message )
		{
		case MESSAGE_COMPARE_HASH:
			REQUIRES( contextInfoPtr->type == CONTEXT_HASH || \
					  contextInfoPtr->type == CONTEXT_MAC  );

			/* If it's a hash or MAC context, compare the hash value */
			if( !TEST_FLAG( contextInfoPtr->flags, 
							CONTEXT_FLAG_HASH_DONE ) )
				return( CRYPT_ERROR_INCOMPLETE );
			if( contextInfoPtr->type == CONTEXT_HASH )
				{
				if( dataLength == capabilityInfoPtr->blockSize && \
					compareDataConstTime( data, contextInfoPtr->ctxHash->hash,
										  dataLength ) )
					return( CRYPT_OK );
				}
			if( contextInfoPtr->type == CONTEXT_MAC )
				{
				if( dataLength == capabilityInfoPtr->blockSize && \
					compareDataConstTime( data, contextInfoPtr->ctxMAC->mac,
										  dataLength ) )
					return( CRYPT_OK );
				}
			break;

		case MESSAGE_COMPARE_ICV:
			{
			BYTE icv[ CRYPT_MAX_HASHSIZE + 8 ];
			int status;

			REQUIRES( contextInfoPtr->type == CONTEXT_CONV );

			if( contextInfoPtr->ctxConv->mode != CRYPT_MODE_GCM )
				return( CRYPT_ERROR_NOTAVAIL );
			status = capabilityInfoPtr->getInfoFunction( CAPABILITY_INFO_ICV, 
									contextInfoPtr, icv, dataLength );
			if( cryptStatusError( status ) )
				return( status );
			if( compareDataConstTime( data, icv, dataLength ) )
				return( CRYPT_OK );
			break;
			}

		case MESSAGE_COMPARE_KEYID:
			REQUIRES( contextInfoPtr->type == CONTEXT_PKC );

			/* If it's a PKC context, compare the key ID */
			if( dataLength == KEYID_SIZE && \
				!memcmp( data, contextInfoPtr->ctxPKC->keyID,
						 KEYID_SIZE ) )
				return( CRYPT_OK );
			break;

#ifdef USE_PGP
		case MESSAGE_COMPARE_KEYID_PGP:
			REQUIRES( contextInfoPtr->type == CONTEXT_PKC );

			/* If it's a PKC context, compare the PGP key ID */
			if( TEST_FLAG( contextInfoPtr->ctxPKC->flags, 
						   PKCINFO_FLAG_PGPKEYID_SET ) && \
				dataLength == PGP_KEYID_SIZE && \
				!memcmp( data, contextInfoPtr->ctxPKC->pgp2KeyID,
						 PGP_KEYID_SIZE ) )
				return( CRYPT_OK );
			break;

		case MESSAGE_COMPARE_KEYID_OPENPGP:
			REQUIRES( contextInfoPtr->type == CONTEXT_PKC );

			/* If it's a PKC context, compare the OpenPGP key ID */
			if( TEST_FLAG( contextInfoPtr->ctxPKC->flags, 
						   PKCINFO_FLAG_OPENPGPKEYID_SET ) && \
				dataLength == PGP_KEYID_SIZE && \
				!memcmp( data, contextInfoPtr->ctxPKC->openPgpKeyID,
						 PGP_KEYID_SIZE ) )
				return( CRYPT_OK );
			break;
#endif /* USE_PGP */

		default:
			retIntError();
		}

	/* The comparison failed */
	return( CRYPT_ERROR );
	}

/****************************************************************************
*																			*
*							Internal Context Functions						*
*																			*
****************************************************************************/

/* Check whether a context can perform a certain operation.  This is used
   frequently enough that it's implemented as a dedicated function, wrapping
   krnlSendMessage() */

CHECK_RETVAL_BOOL \
BOOLEAN checkContextCapability( IN_HANDLE const CRYPT_CONTEXT iCryptContext,
								IN_ENUM( MESSAGE_CHECK ) \
									const MESSAGE_CHECK_TYPE checkType )
	{
	int status;

	REQUIRES_B( isHandleRangeValid( iCryptContext ) );
	REQUIRES_B( isEnumRange( checkType, MESSAGE_CHECK ) );

	status = krnlSendMessage( iCryptContext, IMESSAGE_CHECK, NULL, 
							  checkType );
	return( cryptStatusOK( status ) ? TRUE : FALSE );
	}

/****************************************************************************
*																			*
*							Context Storage Management						*
*																			*
****************************************************************************/

/* Fix up potential alignment issues arising from the cloning of contexts */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int fixupContextStorage( INOUT CONTEXT_INFO *contextInfoPtr, 
								void *typeStorage, 
								const void *subtypeStorage,
								IN_LENGTH_SHORT const int originalOffset,
								IN_LENGTH_SHORT const int storageAlignSize )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;
	int newOffset, stateStorageSize, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isShortIntegerRangeNZ( originalOffset ) );
	REQUIRES( storageAlignSize > 0 && storageAlignSize < 128 );

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );

	/* Check whether the keying data offset has changed from the original to
	   the cloned context */
	newOffset = ptr_diff( subtypeStorage, typeStorage );
	if( newOffset == originalOffset )
		return( CRYPT_OK );

	/* The start of the context subtype data within the context memory block 
	   has changed due to the cloned memory block starting at a different 
	   offset, so we need to move the subtype data do its new location */
	status = capabilityInfoPtr->getInfoFunction( CAPABILITY_INFO_STATESIZE,
												 NULL, &stateStorageSize, 0 );
	if( cryptStatusError( status ) )
		return( status );
	memmove( ( BYTE * ) typeStorage + newOffset, 
			 ( BYTE * ) typeStorage + originalOffset, stateStorageSize );

	return( CRYPT_OK );
	}

/* Initialise pointers to context-specific storage areas */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initContextStorage( INOUT CONTEXT_INFO *contextInfoPtr, 
							   IN_LENGTH_SHORT_Z const int storageAlignSize )
	{
	int offset = 0;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* We can't call sanityCheckContext() at this point because the storage
	   for the context data that we want to sanity-check hasn't been set
	   up yet.  The best that we can do is perform a subset of the standard
	   sanity checks */ 
	REQUIRES( isEnumRange( contextInfoPtr->type, CONTEXT ) && \
			  CHECK_FLAGS( contextInfoPtr->flags, CONTEXT_FLAG_NONE, 
						   CONTEXT_FLAG_MAX ) );
	REQUIRES( ( contextInfoPtr->type == CONTEXT_PKC && \
				storageAlignSize == 0 ) || \
			  ( ( contextInfoPtr->type == CONTEXT_HASH || \
				  contextInfoPtr->type == CONTEXT_MAC || \
				  contextInfoPtr->type == CONTEXT_GENERIC ) && \
				storageAlignSize == 8 ) || \
			  ( contextInfoPtr->type == CONTEXT_CONV && \
				( storageAlignSize == 8 || storageAlignSize == 16 ) ) );
			  /* The storage alignment size defaults to 8 bytes, currently
			     the only other used value is 16 bytes for AES hardware */

	/* This function is used to initialise both pristine and cloned 
	   contexts.  If it's the latter then the cloning operation may have 
	   moved the relative position of the key data around in memory, since 
	   the granularity of its allocation differs from that of the memory 
	   allocator.  To handle this we use the following pattern:

		Remember the old offset of the context subtype data from the 
			context type data.
		Set up the context type and subtype data pointers.
		If this is a cloned context and the new offset of the context 
			subtype data differs from the old one, fix up any alignment 
			issues */
	switch( contextInfoPtr->type )
		{
		case CONTEXT_CONV:
			/* Remember the offset of the subtype data from the type data */
			if( contextInfoPtr->ctxConv != NULL )
				{
				offset = ptr_diff( contextInfoPtr->ctxConv->key, 
								   contextInfoPtr->ctxConv );
				}

			/* Calculate the offsets of the context storage and keying 
			   data */
			contextInfoPtr->ctxConv = \
					ALIGN_CONTEXT_PTR( contextInfoPtr, CONV_INFO );
			contextInfoPtr->ctxConv->key = \
					ptr_align( ( BYTE * ) contextInfoPtr->ctxConv + sizeof( CONV_INFO ), 
							   storageAlignSize );

			/* If this is a new context, we're done */
			if( offset == 0 )
				return( CRYPT_OK );

			/* It's a cloned context, fix up any potential alignment-
			   related issues */
			return( fixupContextStorage( contextInfoPtr, 
										 contextInfoPtr->ctxConv, 
										 contextInfoPtr->ctxConv->key,
										 offset, storageAlignSize ) );

		case CONTEXT_HASH:
			/* Remember the offset of the subtype data from the type data */
			if( contextInfoPtr->ctxHash != NULL )
				{
				offset = ptr_diff( contextInfoPtr->ctxHash->hashInfo,
								   contextInfoPtr->ctxHash );
				}

			/* Calculate the offsets of the context storage and hash state 
			   data */
			contextInfoPtr->ctxHash = \
					ALIGN_CONTEXT_PTR( contextInfoPtr, HASH_INFO );
			contextInfoPtr->ctxHash->hashInfo = \
					ptr_align( ( BYTE * ) contextInfoPtr->ctxHash + sizeof( HASH_INFO ), 
							   storageAlignSize );

			/* If this is a new context, we're done */
			if( offset == 0 )
				return( CRYPT_OK );

			/* It's a cloned context, fix up any potential alignment-
			   related issues */
			return( fixupContextStorage( contextInfoPtr, 
										 contextInfoPtr->ctxHash, 
										 contextInfoPtr->ctxHash->hashInfo,
										 offset, storageAlignSize ) );

		case CONTEXT_MAC:
			/* Remember the offset of the subtype data from the type data */
			if( contextInfoPtr->ctxMAC != NULL )
				{
				offset = ptr_diff( contextInfoPtr->ctxMAC->macInfo,
								   contextInfoPtr->ctxMAC );
				}

			/* Calculate the offsets of the context storage and MAC state 
			   data */
			contextInfoPtr->ctxMAC = \
					ALIGN_CONTEXT_PTR( contextInfoPtr, MAC_INFO );
			contextInfoPtr->ctxMAC->macInfo = \
					ptr_align( ( BYTE * ) contextInfoPtr->ctxMAC + sizeof( MAC_INFO ), 
							   storageAlignSize );

			/* If this is a new context, we're done */
			if( offset == 0 )
				return( CRYPT_OK );

			/* It's a cloned context, fix up any potential alignment-
			   related issues */
			return( fixupContextStorage( contextInfoPtr, 
										 contextInfoPtr->ctxMAC, 
										 contextInfoPtr->ctxMAC->macInfo,
										 offset, storageAlignSize ) );

		case CONTEXT_PKC:
			contextInfoPtr->ctxPKC = \
					ALIGN_CONTEXT_PTR( contextInfoPtr, PKC_INFO );
			break;

		case CONTEXT_GENERIC:
			contextInfoPtr->ctxGeneric = \
					ALIGN_CONTEXT_PTR( contextInfoPtr, GENERIC_INFO );
			break;

		default:
			retIntError();
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Context Message Handler						*
*																			*
****************************************************************************/

/* Handle a message sent to an encryption context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int contextMessageFunction( INOUT TYPECAST( CONTEXT_INFO * ) \
										void *objectInfoPtr,
								   IN_MESSAGE const MESSAGE_TYPE message,
								   void *messageDataPtr,
								   IN_INT_Z const int messageValue )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;
	CONTEXT_INFO *contextInfoPtr = ( CONTEXT_INFO * ) objectInfoPtr;
	int status;

	assert( isWritePtr( objectInfoPtr, sizeof( CONTEXT_INFO ) ) );
	
	REQUIRES( message == MESSAGE_DESTROY || \
			  ( message == MESSAGE_CHANGENOTIFY && \
			    messageValue == MESSAGE_CHANGENOTIFY_OBJHANDLE ) || \
			  sanityCheckContext( contextInfoPtr ) );
			  /* MESSAGE_CHANGENOTIFY_OBJHANDLE is sent after an object 
			     clone operation before the internal state pointers have
				 been corrected from the original to the cloned object */
	REQUIRES( isEnumRange( message, MESSAGE ) );
	REQUIRES( isIntegerRange( messageValue ) );

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );

	/* Process destroy object messages */
	if( message == MESSAGE_DESTROY )
		{
		const CONTEXT_TYPE contextType = contextInfoPtr->type;

		REQUIRES( messageDataPtr == NULL && messageValue == 0 );

		/* Perform any algorithm-specific shutdown */
		if( capabilityInfoPtr->endFunction != NULL )
			capabilityInfoPtr->endFunction( contextInfoPtr );

		/* If this is a context that's implemented via built-in crypto 
		   hardware, perform any required cleanup */
#ifdef HAS_DEVCRYPTO
		if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_HWCRYPTO ) )
			hwCryptoEnd( contextInfoPtr );
#endif /* HAS_DEVCRYPTO */

		/* Perform context-type-specific cleanup */
		if( contextType == CONTEXT_PKC )
			{
			endContextBignums( contextInfoPtr->ctxPKC, 
							   GET_FLAG( contextInfoPtr->flags, 
										 CONTEXT_FLAG_DUMMY ) ? 
									TRUE : FALSE );
			}

		return( CRYPT_OK );
		}

	/* Process attribute get/set/delete messages */
	if( isAttributeMessage( message ) )
		{
		REQUIRES( message == MESSAGE_GETATTRIBUTE || \
				  message == MESSAGE_GETATTRIBUTE_S || \
				  message == MESSAGE_SETATTRIBUTE || \
				  message == MESSAGE_SETATTRIBUTE_S || \
				  message == MESSAGE_DELETEATTRIBUTE );
		REQUIRES( isAttribute( messageValue ) || \
				  isInternalAttribute( messageValue ) );

		if( message == MESSAGE_GETATTRIBUTE )
			{
			return( getContextAttribute( contextInfoPtr, 
										 ( int * ) messageDataPtr,
										 messageValue ) );
			}
		if( message == MESSAGE_GETATTRIBUTE_S )
			{
			return( getContextAttributeS( contextInfoPtr, 
										  ( MESSAGE_DATA * ) messageDataPtr,
										  messageValue ) );
			}
		if( message == MESSAGE_SETATTRIBUTE )
			{
			/* CRYPT_IATTRIBUTE_INITIALISED is purely a notification message 
			   with no parameters so we don't pass it down to the attribute-
			   handling code */
			if( messageValue == CRYPT_IATTRIBUTE_INITIALISED )
				return( CRYPT_OK );

			return( setContextAttribute( contextInfoPtr, 
										 *( ( int * ) messageDataPtr ),
										 messageValue ) );
			}
		if( message == MESSAGE_SETATTRIBUTE_S )
			{
			const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

			return( setContextAttributeS( contextInfoPtr, msgData->data, 
										  msgData->length, messageValue ) );
			}
		if( message == MESSAGE_DELETEATTRIBUTE )
			return( deleteContextAttribute( contextInfoPtr, messageValue ) );

		retIntError();
		}

	/* Process action messages */
	if( isActionMessage( message ) )
		{
		return( processActionMessage( contextInfoPtr, message, 
									  messageDataPtr, messageValue ) );
		}

	/* Process messages that compare object properties or clone the object */
	if( message == MESSAGE_COMPARE )
		{
		const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

		assert( isReadPtr( messageDataPtr, sizeof( MESSAGE_DATA ) ) );

		return( processCompareMessage( contextInfoPtr, messageValue,
									   msgData->data, msgData->length ) );
		}

	/* Process messages that check a context */
	if( message == MESSAGE_CHECK )
		return( checkContext( contextInfoPtr, messageValue ) );

	/* Process internal notification messages */
	if( message == MESSAGE_CHANGENOTIFY )
		{
		switch( messageValue )
			{
			case MESSAGE_CHANGENOTIFY_STATE:
				/* State-change reflected down from the controlling certificate 
				   object, this doesn't affect us */
				break;

			case MESSAGE_CHANGENOTIFY_OBJHANDLE:
				{
				const CRYPT_HANDLE iCryptHandle = *( ( int * ) messageDataPtr );
				int storageAlignSize;

				REQUIRES( contextInfoPtr->type == CONTEXT_CONV || \
						  contextInfoPtr->type == CONTEXT_HASH || \
						  contextInfoPtr->type == CONTEXT_MAC );
				REQUIRES( isHandleRangeValid( iCryptHandle ) );
				REQUIRES( contextInfoPtr->objectHandle != iCryptHandle );

				/* We've been cloned, update the object handle and internal 
				   state pointers, and perform any deep clong operations 
				   that may be required */
				contextInfoPtr->objectHandle = iCryptHandle;
				status = capabilityInfoPtr->getInfoFunction( CAPABILITY_INFO_STATEALIGNTYPE,
											NULL, &storageAlignSize, 0 );
				if( cryptStatusError( status ) )
					return( status );
				status = initContextStorage( contextInfoPtr, storageAlignSize );
				if( cryptStatusError( status ) )
					return( status );
#ifdef HAS_DEVCRYPTO
				if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_HWCRYPTO ) )
					{
					status = hwCryptoCloneState( contextInfoPtr );
					if( cryptStatusError( status ) )
						return( status );
					}
#endif /* HAS_DEVCRYPTO */

				/* Make sure that the context data is valid, both because we 
				   didn't check it earlier since the storage pointers were 
				   set wrong from the cloning process, and to verify that 
				   the fixup operation went OK */
				ENSURES( sanityCheckContext( contextInfoPtr ) );

				break;
				}

			case MESSAGE_CHANGENOTIFY_OWNERHANDLE:
				{
				const CRYPT_HANDLE iCryptHandle = *( ( int * ) messageDataPtr );

				REQUIRES( isHandleRangeValid( iCryptHandle ) );

				/* The second stage of a cloning, update the owner handle */
				contextInfoPtr->ownerHandle = iCryptHandle;
				break;
				}

			default:
				retIntError();
			}

		return( CRYPT_OK );
		}

	/* Process object-specific messages */
	if( message == MESSAGE_CTX_GENKEY )
		{
		static const int actionFlags = \
				MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_ALL ) | \
				MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_ALL ) | \
				MK_ACTION_PERM( MESSAGE_CTX_SIGN, ACTION_PERM_ALL ) | \
				MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_ALL ) | \
				MK_ACTION_PERM( MESSAGE_CTX_HASH, ACTION_PERM_ALL );
		const CTX_GENERATEKEY_FUNCTION generateKeyFunction = \
						( CTX_GENERATEKEY_FUNCTION ) \
						FNPTR_GET( contextInfoPtr->generateKeyFunction );

		REQUIRES( contextInfoPtr->type == CONTEXT_CONV || \
				  contextInfoPtr->type == CONTEXT_MAC || \
				  contextInfoPtr->type == CONTEXT_PKC || \
				  contextInfoPtr->type == CONTEXT_GENERIC );
		REQUIRES( needsKey( contextInfoPtr ) );
		REQUIRES( generateKeyFunction != NULL );

		/* If it's a private key context or a persistent context we need to 
		   have a key label set before we can continue */
		if( ( ( contextInfoPtr->type == CONTEXT_PKC ) || \
			  TEST_FLAG( contextInfoPtr->flags, 
						 CONTEXT_FLAG_PERSISTENT ) ) && \
			contextInfoPtr->labelSize <= 0 )
			return( CRYPT_ERROR_NOTINITED );

		/* Generate a new key into the context */
		status = generateKeyFunction( contextInfoPtr );
		if( cryptStatusError( status ) )
			return( status );

		/* There's now a key loaded, remember this and disable further key 
		   generation.  The kernel won't allow a keygen anyway once the 
		   object is in the high state but taking this additional step can't 
		   hurt */
		SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_KEY_SET );
		return( krnlSendMessage( contextInfoPtr->objectHandle,
								 IMESSAGE_SETATTRIBUTE, 
								 ( MESSAGE_CAST ) &actionFlags, 
								 CRYPT_IATTRIBUTE_ACTIONPERMS ) );
		}
	if( message == MESSAGE_CTX_GENIV )
		{
		MESSAGE_DATA msgData;
		BYTE iv[ CRYPT_MAX_IVSIZE + 8 ];
		const int ivSize = capabilityInfoPtr->blockSize;

		REQUIRES( contextInfoPtr->type == CONTEXT_CONV );

		/* If it's not a conventional encryption context or it's a mode that
		   doesn't use an IV then the generate IV operation is meaningless */
		if( !needsIV( contextInfoPtr->ctxConv->mode ) || \
			isStreamCipher( capabilityInfoPtr->cryptAlgo ) )
			return( CRYPT_ERROR_NOTAVAIL );

		/* Generate a new IV and load it */
		setMessageData( &msgData, iv, ivSize );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusOK( status ) )
			{
			status = capabilityInfoPtr->initParamsFunction( contextInfoPtr,
												KEYPARAM_IV, iv, ivSize );
			}
		return( status );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*					Context Creation/Access Functions						*
*																			*
****************************************************************************/

/* Create an encryption context based on an encryption capability template.
   This is a common function called by devices to create a context once
   they've got the appropriate capability template */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int createContextFromCapability( OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
								 IN_HANDLE const CRYPT_USER iCryptOwner,
								 const CAPABILITY_INFO *capabilityInfoPtr,
								 IN_FLAGS_Z( CREATEOBJECT ) const int objectFlags )
	{
	const CRYPT_ALGO_TYPE cryptAlgo = capabilityInfoPtr->cryptAlgo;
	const CONTEXT_TYPE contextType = \
				( isConvAlgo( cryptAlgo ) ) ? CONTEXT_CONV : \
				( isPkcAlgo( cryptAlgo ) ) ? CONTEXT_PKC : \
				( isHashAlgo( cryptAlgo ) ) ? CONTEXT_HASH : \
				( isMacAlgo( cryptAlgo ) ) ? CONTEXT_MAC : \
				( isSpecialAlgo( cryptAlgo ) ) ? CONTEXT_GENERIC : \
				CONTEXT_NONE;
	CONTEXT_INFO *contextInfoPtr;
	OBJECT_SUBTYPE subType;
	const int createFlags = objectFlags | \
							( needsSecureMemory( contextType ) ? \
							CREATEOBJECT_FLAG_SECUREMALLOC : 0 );
	int sideChannelProtectionLevel, storageSize;
	int stateStorageSize = 0, stateStorageAlignSize = 0;
	int actionFlags = 0, actionPerms = ACTION_PERM_ALL, status;

	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );

	REQUIRES( ( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE ) || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( isFlagRangeZ( objectFlags, CREATEOBJECT ) );
	REQUIRES( isEnumRange( cryptAlgo, CRYPT_ALGO ) );
	REQUIRES( isEnumRange( contextType, CONTEXT ) );

	/* Clear return value */
	*iCryptContext = CRYPT_ERROR;

	/* Get general config information */
	status = krnlSendMessage( iCryptOwner, IMESSAGE_GETATTRIBUTE,
							  &sideChannelProtectionLevel,
							  CRYPT_OPTION_MISC_SIDECHANNELPROTECTION );
	if( cryptStatusError( status ) )
		return( status );
	if( contextType != CONTEXT_PKC )
		{
		status = capabilityInfoPtr->getInfoFunction( CAPABILITY_INFO_STATESIZE,
												NULL, &stateStorageSize, 0 );
		if( cryptStatusOK( status ) )
			{
			status = capabilityInfoPtr->getInfoFunction( CAPABILITY_INFO_STATEALIGNTYPE,
												NULL, &stateStorageAlignSize, 0 );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Set up subtype-specific information */
	switch( contextType )
		{
		case CONTEXT_CONV:
			subType = SUBTYPE_CTX_CONV;
			storageSize = sizeof( CONV_INFO );
			if( capabilityInfoPtr->encryptFunction != NULL || \
				capabilityInfoPtr->encryptCBCFunction != NULL || \
				capabilityInfoPtr->encryptCFBFunction != NULL || \
				capabilityInfoPtr->encryptGCMFunction != NULL )
				{
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT,
											   ACTION_PERM_ALL );
				}
			if( capabilityInfoPtr->decryptFunction != NULL || \
				capabilityInfoPtr->decryptCBCFunction != NULL || \
				capabilityInfoPtr->decryptCFBFunction != NULL || \
				capabilityInfoPtr->decryptGCMFunction != NULL )
				{
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_DECRYPT,
											   ACTION_PERM_ALL );
				}
			actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_GENKEY, ACTION_PERM_ALL );
			break;

		case CONTEXT_PKC:
			subType = SUBTYPE_CTX_PKC;
			storageSize = sizeof( PKC_INFO );
			if( isDlpAlgo( cryptAlgo ) || isEccAlgo( cryptAlgo ) )
				{
				/* The DLP- and ECDLP-based PKC's have somewhat specialised 
				   usage requirements so we don't allow direct access by 
				   users */
				actionPerms = ACTION_PERM_NONE_EXTERNAL;
				}
			if( capabilityInfoPtr->encryptFunction != NULL )
				{
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT,
											   actionPerms );
				}
			if( capabilityInfoPtr->decryptFunction != NULL )
				{
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_DECRYPT,
											   actionPerms );
				}
			if( capabilityInfoPtr->signFunction != NULL )
				{
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGN,
											   actionPerms );
				}
			if( capabilityInfoPtr->sigCheckFunction != NULL )
				{
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK,
											   actionPerms );
				}
			actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_GENKEY, ACTION_PERM_ALL );
			break;

		case CONTEXT_HASH:
			subType = SUBTYPE_CTX_HASH;
			storageSize = sizeof( HASH_INFO );
			actionFlags = MK_ACTION_PERM( MESSAGE_CTX_HASH, ACTION_PERM_ALL );
			break;

		case CONTEXT_MAC:
			subType = SUBTYPE_CTX_MAC;
			storageSize = sizeof( MAC_INFO );
			actionFlags = MK_ACTION_PERM( MESSAGE_CTX_HASH, ACTION_PERM_ALL ) | \
						  MK_ACTION_PERM( MESSAGE_CTX_GENKEY, ACTION_PERM_ALL );
			break;

		case CONTEXT_GENERIC:
			subType = SUBTYPE_CTX_GENERIC;
			storageSize = sizeof( GENERIC_INFO );
			actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_GENKEY, ACTION_PERM_NONE_EXTERNAL );
			break;

		default:
			retIntError();
		}
	if( actionFlags <= 0 )
		{
		/* There are no actions enabled for this capability, bail out rather 
		   than creating an unusable context */
		DEBUG_DIAG(( "No actions available for this capability" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_NOTAVAIL );
		}

	/* Create the context and initialise the variables in it.  The storage is
	   allocated as follows:

		+-----------+						  --+
		|			|							|
		| CTX_INFO	| ---- typeInfo ----+		|
		|			|					|		| CONTEXT_INFO_ALIGN_SIZE
		+-----------+					|		|
		|###########| Pad to 8-byte boundary	|
		+-----------+ <-----------------+	  --+
		|			|							|
		| typeInfo	| -- subtypeInfo ---+		| storageSize
		|			|					|		|
		+-----------+					|	  --+
		|###########| Pad to ALIGNSIZE boundary
		+-----------+ <-----------------+	  --- Aligned by initContextStorage()
		|			|
		|subtypeInfo|
		|			|
		+-----------+ 

	   In the above the typeInfo is one of { CONV_INFO, PKC_INFO, HASH_INFO,
	   MAC_INFO, or GENERIC_INFO }, and the subtypeInfo is the information
	   associated with the type, for example keying data for a CONV_INFO.

	   Note that the typeInfo and subtypeInfo have different alignment 
	   handling, the typeInfo is aligned relative to the CTX_INFO at a 
	   multiple of CONTEXT_INFO_ALIGN_SIZE bytes and the subtypeInfo is 
	   aligned at an absolute memory postion at a multiple of 
	   CAPABILITY_INFO_STATEALIGNTYPE bytes.  The latter is necessary 
	   because the subtypeInfo holds low-level algorithm state information 
	   that can require specific memory alignment when used with exotic 
	   instruction modes like SIMD/vector or crypto hardware-assist 
	   operations.  This leads to some complications when cloning contexts,
	   see the comments in initContextStorage() for details.

	   Since we don't know at this point what the alignment requirements for
	   the key storage will be because we don't know what contextInfoPtr 
	   will by until it's allocated, we over-allocate the state storage by
	   ALIGNSIZE bytes to allow the key data to be aligned with an ALIGNSIZE 
	   block.

	   The aligned-alloc for the key storage can get quite complex, some 
	   Unix systems have posix_memalign(), and VC++ 2005 and newer have 
	   _aligned_malloc(), but these are too non-portable to rely on.  What we
	   have to do instead is allocate enough spare space at the end of the key
	   data for alignment.  Since the address for the start of the key data
	   has to be be a multiple of ALIGNSIZE, adding ALIGNSIZE extra bytes to
	   the allocation guarantees that we have enough space, with the key data
	   starting somewhere in the first 16 bytes of the allocated block.  The 
	   rest is done by initContextStorage() */
	status = krnlCreateObject( iCryptContext, ( void ** ) &contextInfoPtr,
							   CONTEXT_INFO_ALIGN_SIZE + storageSize + \
									( stateStorageSize + stateStorageAlignSize ), 
							   OBJECT_TYPE_CONTEXT, subType, createFlags, 
							   iCryptOwner, actionFlags, 
							   contextMessageFunction );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( contextInfoPtr != NULL );
	contextInfoPtr->objectHandle = *iCryptContext;
	contextInfoPtr->ownerHandle = iCryptOwner;
	INIT_FLAGS( contextInfoPtr->flags, CONTEXT_FLAG_NONE );
	DATAPTR_SET( contextInfoPtr->capabilityInfo, ( void * ) capabilityInfoPtr );
	contextInfoPtr->type = contextType;
#ifdef USE_DEVICES
	contextInfoPtr->deviceObject = \
		contextInfoPtr->altDeviceObject = CRYPT_ERROR;
#endif /* USE_DEVICES */
	status = initContextStorage( contextInfoPtr, stateStorageAlignSize );
	if( cryptStatusError( status ) )
		{
		/* Enqueue a destroy message for the context and tell the kernel 
		   that we're done */
		krnlSendNotifier( *iCryptContext, IMESSAGE_DESTROY );
		krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE, 
						 MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
		return( status );
		}
	if( sideChannelProtectionLevel > 0 )
		{
		SET_FLAG( contextInfoPtr->flags, 
				  CONTEXT_FLAG_SIDECHANNELPROTECTION );
		}
	if( objectFlags & CREATEOBJECT_FLAG_DUMMY )
		{
		/* This is a dummy object, remember that it's just a placeholder 
		   with actions handled externally */  		
		SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY );
		}
	if( objectFlags & CREATEOBJECT_FLAG_PERSISTENT )
		{
		/* If it's a persistent object backed by a permanent key in a crypto 
		   device, record this */
		SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_PERSISTENT );
		}
	if( contextInfoPtr->type == CONTEXT_PKC )
		{
		PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;

		/* Initialise the overall PKC information */
		memset( pkcInfo, 0, sizeof( PKC_INFO ) );
		INIT_FLAGS( pkcInfo->flags, PKCINFO_FLAG_NONE );

		/* If it's a non-dummy object, initialise the bignums */
		if( !( objectFlags & CREATEOBJECT_FLAG_DUMMY ) )
			{
			status = initContextBignums( pkcInfo, isEccAlgo( cryptAlgo ) ? \
												  TRUE : FALSE );
			if( cryptStatusError( status ) )
				{
				/* Enqueue a destroy message for the context and tell the 
				   kernel that we're done */
				krnlSendNotifier( *iCryptContext, IMESSAGE_DESTROY );
				krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE, 
								 MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
				return( status );
				}
			}
		else
			{
			/* Reflect the dummy flag down into the PKC info as well for 
			   situations where only the PKC_INFO data is available */
			SET_FLAG( contextInfoPtr->ctxPKC->flags, PKCINFO_FLAG_DUMMY );
			}
		}
	if( contextInfoPtr->type == CONTEXT_CONV )
		{
		/* Set the default encryption mode, which is always CBC if possible,
		   and the corresponding en/decryption handler */
		if( capabilityInfoPtr->encryptCBCFunction != NULL )
			{
			contextInfoPtr->ctxConv->mode = CRYPT_MODE_CBC;
			FNPTR_SET( contextInfoPtr->encryptFunction,
					   capabilityInfoPtr->encryptCBCFunction );
			FNPTR_SET( contextInfoPtr->decryptFunction,
					   capabilityInfoPtr->decryptCBCFunction );
			}
		else
			{
			/* There's no CBC mode available, fall back to increasingly
			   sub-optimal choices of mode.  For stream ciphers the only 
			   available mode is (pseudo-)CFB so this isn't a problem, but 
			   for block ciphers it'll cause problems because most crypto 
			   protocols only allow CBC mode.  In addition we don't fall
			   back to GCM, which is a sufficiently unusual mode that we
			   require it to be explicitly enabled by the user */
#ifdef USE_CFB
			if( capabilityInfoPtr->encryptCFBFunction != NULL )
				{
				contextInfoPtr->ctxConv->mode = CRYPT_MODE_CFB;
				FNPTR_SET( contextInfoPtr->encryptFunction,
						   capabilityInfoPtr->encryptCFBFunction );
				FNPTR_SET( contextInfoPtr->decryptFunction,
						   capabilityInfoPtr->decryptCFBFunction );
				}
			else
#endif /* USE_CFB */
				{
				contextInfoPtr->ctxConv->mode = CRYPT_MODE_ECB;
				FNPTR_SET( contextInfoPtr->encryptFunction,
						   capabilityInfoPtr->encryptFunction );
				FNPTR_SET( contextInfoPtr->decryptFunction,
						   capabilityInfoPtr->decryptFunction );
				}
			}
		}
	else
		{
		/* There's only one possible en/decryption handler */
		FNPTR_SET( contextInfoPtr->encryptFunction,
				   capabilityInfoPtr->encryptFunction );
		FNPTR_SET( contextInfoPtr->decryptFunction,
				   capabilityInfoPtr->decryptFunction );
		}
	if( contextInfoPtr->type != CONTEXT_HASH )
		{
		/* Set up the key handling functions */
		initKeyHandling( contextInfoPtr );
		}
	if( contextInfoPtr->type == CONTEXT_PKC )
		{
		/* Set up the key read/write functions */
		initKeyID( contextInfoPtr );
		initPubKeyRead( contextInfoPtr );
		initPrivKeyRead( contextInfoPtr );
		initKeyWrite( contextInfoPtr );
		}

	/* Make sure that all the function pointers have been set up as 
	   required */
	ENSURES( checkContextFunctions( contextInfoPtr ) );

	/* We've finished setting up the object type-specific info, tell the
	   kernel that the object is ready for use */
	status = krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusOK( status ) && contextInfoPtr->type == CONTEXT_HASH )
		{
		/* If it's a hash context then there's no explicit keygen or load so 
		   we need to send an "object initialised" message to get the kernel 
		   to move it into the high state.  If this isn't done then any 
		   attempt to use the object will be blocked */
		status = krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_UNUSED, 
								  CRYPT_IATTRIBUTE_INITIALISED );
		}
	if( cryptStatusError( status ) )
		{
		*iCryptContext = CRYPT_ERROR;
		return( status );
		}
	return( CRYPT_OK );
	}

/* Create an encryption context object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int createContext( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				   IN TYPECAST( CAPABILITY_INFO * ) const void *auxDataPtr, 
				   IN_FLAGS_Z( CREATEOBJECT ) const int auxValue )
	{
	CRYPT_CONTEXT iCryptContext;
	const CAPABILITY_INFO *capabilityInfoPtr;
	int status;

	assert( isReadPtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );
	assert( isReadPtr( auxDataPtr, sizeof( CAPABILITY_INFO ) ) );

	REQUIRES( isFlagRangeZ( auxValue, CREATEOBJECT ) );
	REQUIRES( isEnumRange( createInfo->arg1, CRYPT_ALGO ) );

	/* Find the capability corresponding to the algorithm */
	capabilityInfoPtr = findCapabilityInfo( auxDataPtr, createInfo->arg1 );
	if( capabilityInfoPtr == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Pass the call on to the lower-level create function */
	status = createContextFromCapability( &iCryptContext,
										  createInfo->cryptOwner,
										  capabilityInfoPtr, auxValue );
	if( cryptStatusOK( status ) )
		createInfo->cryptHandle = iCryptContext;
	return( status );
	}

#ifdef CONFIG_DIRECT_API

C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 3, 4 ) ) \
C_RET cryptGetDirectAPI( C_IN CRYPT_CONTEXT cryptContext, 
						 C_OUT void C_PTR C_PTR contextPtr,
						 C_OUT CRYPT_DIRECT_FUNCTION C_PTR encryptFunction,
						 C_OUT CRYPT_DIRECT_FUNCTION C_PTR decryptFunction )
	{
	const CONTEXT_INFO *contextInfoPtr;
	CRYPT_DIRECT_FUNCTION encryptFnPtr, decryptFnPtr;
	int dummy, status;

	/* Perform basic parameter error checking */
	status = krnlSendMessage( cryptContext, MESSAGE_GETATTRIBUTE,
							  &dummy, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ERROR_PARAM1 : status );
	if( !isWritePtr( contextPtr, sizeof( void * ) ) )
		return( CRYPT_ERROR_PARAM2 );
	if( !isWritePtr( encryptFunction, sizeof( CRYPT_DIRECT_FUNCTION * ) ) )
		return( CRYPT_ERROR_PARAM3 );
	if( !isWritePtr( decryptFunction, sizeof( CRYPT_DIRECT_FUNCTION * ) ) )
		return( CRYPT_ERROR_PARAM4 );

	/* Clear return values */
	*contextPtr = NULL;
	*encryptFunction = *decryptFunction = NULL;

	/* Retrieve pointers to the internal encrypt/decrypt functions */
	status = krnlAcquireObject( cryptContext, OBJECT_TYPE_CONTEXT,
								( MESSAGE_PTR_CAST ) &contextInfoPtr,
								CRYPT_ERROR_PARAM1 );
	if( cryptStatusError( status ) )
		return( status );
	encryptFnPtr = ( CRYPT_DIRECT_FUNCTION ) \
				   FNPTR_GET( contextInfoPtr->encryptFunction );
	decryptFnPtr = ( CRYPT_DIRECT_FUNCTION ) \
				   FNPTR_GET( contextInfoPtr->decryptFunction );
	krnlReleaseObject( cryptContext );
	if( encryptFnPtr == NULL || decryptFnPtr == NULL )
		return( CRYPT_ERROR_INTERNAL );

	*contextPtr = ( void * ) contextInfoPtr;
	*encryptFunction = encryptFnPtr;
	*decryptFunction = decryptFnPtr;

	return( CRYPT_OK );
	}
#endif /* CONFIG_DIRECT_API */
