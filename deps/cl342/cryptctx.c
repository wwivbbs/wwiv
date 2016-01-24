/****************************************************************************
*																			*
*					  cryptlib Encryption Context Routines					*
*						Copyright Peter Gutmann 1992-2008					*
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

/* The number of bytes of data that we check to make sure that the 
   encryption operation succeeded.  See the comment in encryptData() before 
   changing this */

#define ENCRYPT_CHECKSIZE	16

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

/* Initialise pointers to context-specific storage areas */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initContextConvStorage( INOUT CONTEXT_INFO *contextInfoPtr, 
								   IN_LENGTH_SHORT const int storageAlignSize )
	{
	BOOLEAN isClonedContext = ( contextInfoPtr->ctxConv != NULL ) ? \
							  TRUE : FALSE;
	int diff = DUMMY_INIT, newDiff, stateStorageSize, status;

	/* If this is a cloned context then the cloning operation may have moved 
	   the relative position of the key data around in memory, since the 
	   granularity of its allocation differs from that of the memory 
	   allocator.  In order to determine whether we have to correct things, 
	   we remember the old offset of the keying data from the context 
	   storage and compare it to the new offset */
	if( isClonedContext )
		diff = ptr_diff( contextInfoPtr->ctxConv->key, 
						 contextInfoPtr->ctxConv );

	/* Calculate the offsets of the context storage and keying data */
	contextInfoPtr->ctxConv = ALIGN_CONTEXT_PTR( contextInfoPtr, CONV_INFO );
	contextInfoPtr->ctxConv->key = \
		ptr_align( ( BYTE * ) contextInfoPtr->ctxConv + sizeof( CONV_INFO ), 
				   storageAlignSize );

	/* If this is a newly-initialised context then we're done */
	if( !isClonedContext )
		return( CRYPT_OK );

	/* Check whether the keying data offset has changed from the original to
	   the cloned context */
	newDiff = ptr_diff( contextInfoPtr->ctxConv->key, 
						contextInfoPtr->ctxConv );
	if( newDiff == diff )
		return( CRYPT_OK );

	/* The start of the actual keying data within the keying data memory 
	   block has changed due to the cloned memory block starting at a 
	   different offset, we need to move the keying data do its new 
	   location */
	status = contextInfoPtr->capabilityInfo->getInfoFunction( CAPABILITY_INFO_STATESIZE,
										NULL, &stateStorageSize, 0 );
	if( cryptStatusError( status ) )
		return( status );
	memmove( ( BYTE * ) contextInfoPtr->ctxConv + newDiff, 
			 ( BYTE * ) contextInfoPtr->ctxConv + diff, stateStorageSize );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initContextStorage( INOUT CONTEXT_INFO *contextInfoPtr, 
							   IN_LENGTH_SHORT const int storageAlignSize )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( ( contextInfoPtr->type == CONTEXT_PKC && \
				storageAlignSize == 0 ) || \
			  ( contextInfoPtr->type != CONTEXT_PKC && \
				storageAlignSize >= 4 && storageAlignSize <= 128 ) );

	switch( contextInfoPtr->type )
		{
		case CONTEXT_CONV:
			/* Handling storage for conventional contexts is sufficiently 
			   complicated in the presence of context cloning that we do it 
			   in a separate function */
			return( initContextConvStorage( contextInfoPtr, storageAlignSize ) );

		case CONTEXT_HASH:
			contextInfoPtr->ctxHash = \
					ALIGN_CONTEXT_PTR( contextInfoPtr, HASH_INFO );
			contextInfoPtr->ctxHash->hashInfo = \
					ptr_align( ( BYTE * ) contextInfoPtr->ctxConv + sizeof( HASH_INFO ), 
							   storageAlignSize );
			break;

		case CONTEXT_MAC:
			contextInfoPtr->ctxMAC = \
					ALIGN_CONTEXT_PTR( contextInfoPtr, MAC_INFO );
			contextInfoPtr->ctxMAC->macInfo = \
					ptr_align( ( BYTE * ) contextInfoPtr->ctxConv + sizeof( MAC_INFO ), 
							   storageAlignSize );
			break;

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
	const CAPABILITY_INFO *capabilityInfoPtr = contextInfoPtr->capabilityInfo;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( checkType > MESSAGE_CHECK_NONE && \
			  checkType < MESSAGE_CHECK_LAST );

	/* If it's a check that an object's ready for key generation we can 
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
		/* DH can never be used for encryption or signatures (if it is then
		   we call it Elgamal) and KEA is explicitly for key agreement only.
		   Note that the status of DH is a bit ambiguous in that every DH key
		   is both a public and private key, in order to avoid confusion in 
		   situations where we're checking for real private keys we always 
		   denote a DH context as key-agreement only without taking a side 
		   about whether it's a public or private key */
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
			  checkType == MESSAGE_CHECK_CERT || \
			  checkType == MESSAGE_CHECK_CA );

	/* Check that it's a private key if this is required */
	if( ( checkType == MESSAGE_CHECK_PKC_PRIVATE || \
		  checkType == MESSAGE_CHECK_PKC_DECRYPT || \
		  checkType == MESSAGE_CHECK_PKC_SIGN ) && \
		( contextInfoPtr->flags & CONTEXT_FLAG_ISPUBLICKEY ) )
		return( CRYPT_ARGERROR_OBJECT );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Data Encryption Functions						*
*																			*
****************************************************************************/

/* Encrypt a block of data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptDataConv( INOUT CONTEXT_INFO *contextInfoPtr, 
							IN_BUFFER( dataLength ) void *data, 
							IN_LENGTH_Z const int dataLength )
	{
	BYTE savedData[ ENCRYPT_CHECKSIZE + 8 ];
	const CAPABILITY_INFO *capabilityInfoPtr = contextInfoPtr->capabilityInfo;
	const int savedDataLength = min( dataLength, ENCRYPT_CHECKSIZE );
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( data, dataLength ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_CONV );
	REQUIRES( !needsKey( contextInfoPtr ) );
	REQUIRES( dataLength >= 0 && dataLength < MAX_INTLENGTH );
	REQUIRES( isStreamCipher( capabilityInfoPtr->cryptAlgo ) || \
			  !needsIV( contextInfoPtr->ctxConv->mode ) ||
			  ( contextInfoPtr->flags & CONTEXT_FLAG_IV_SET ) );

	memcpy( savedData, data, savedDataLength );
	status = contextInfoPtr->encryptFunction( contextInfoPtr, data, 
											  dataLength );
	if( cryptStatusError( status ) || savedDataLength <= 8 )
		{
		zeroise( savedData, savedDataLength );
		return( status );
		}

	/* Check for a catastrophic failure of the encryption.  A check of
	   a single block unfortunately isn't completely foolproof for 64-bit
	   blocksize ciphers in CBC mode because of the way the IV is applied to 
	   the input.  For the CBC encryption operation:
					
		out = enc( in ^ IV )
						
	   if out == IV the operation turns into a no-op.  Consider the simple 
	   case where IV == in, so IV ^ in == 0.  Then out = enc( 0 ) == IV, 
	   with the input appearing again at the output.  In fact for a 64-bit 
	   block cipher this can occur during normal operation once every 2^32 
	   blocks.  Although the chances of this happening are fairly low (the 
	   collision would have to occur on the first encrypted block in a 
	   message since that's the one that we check), we check the first two 
	   blocks if we're using a 64-bit block cipher in CBC mode in order to 
	   reduce false positives */
	if( !memcmp( savedData, data, savedDataLength ) )
		{
		zeroise( data, dataLength );
		status = CRYPT_ERROR_FAILED;
		}
	zeroise( savedData, savedDataLength );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptDataPKC( INOUT CONTEXT_INFO *contextInfoPtr, 
						   IN_BUFFER( dataLength ) void *data, 
						   IN_LENGTH_PKC const int dataLength )
	{
	BYTE savedData[ ENCRYPT_CHECKSIZE + 8 ];
	const CAPABILITY_INFO *capabilityInfoPtr = contextInfoPtr->capabilityInfo;
	const BOOLEAN isDLP = isDlpAlgo( capabilityInfoPtr->cryptAlgo );
	const BOOLEAN isECC = isEccAlgo( capabilityInfoPtr->cryptAlgo );
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( data, dataLength ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC );
	REQUIRES( !needsKey( contextInfoPtr ) );

	/* Key agreement algorithms are treated as a special case since they 
	   don't actually encrypt the data */
	if( isKeyxAlgo( capabilityInfoPtr->cryptAlgo ) )
		{
		REQUIRES( dataLength == sizeof( KEYAGREE_PARAMS ) );

		status = contextInfoPtr->encryptFunction( contextInfoPtr, data, 
												  dataLength );
		if( !( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY ) )
			clearTempBignums( contextInfoPtr->ctxPKC );
		return( status );
		}

	REQUIRES( ( ( isDLP || isECC ) && \
				dataLength == sizeof( DLP_PARAMS ) ) || \
			  ( ( !isDLP && !isECC ) && \
			    dataLength >= MIN_PKCSIZE && \
				dataLength <= CRYPT_MAX_PKCSIZE ) );

	memcpy( savedData, ( isDLP || isECC ) ? \
						   ( ( DLP_PARAMS * ) data )->inParam1 : data, 
						   ENCRYPT_CHECKSIZE );
	status = contextInfoPtr->encryptFunction( contextInfoPtr, data, 
											  dataLength );
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY ) )
		clearTempBignums( contextInfoPtr->ctxPKC );
	if( cryptStatusError( status ) )
		{
		zeroise( savedData, ENCRYPT_CHECKSIZE );
		return( status );
		}

	/* Check for a catastrophic failure of the encryption */
	if( isDLP || isECC )
		{
		DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) data;

		if( !memcmp( savedData, dlpParams->outParam, 
					 ENCRYPT_CHECKSIZE ) )
			{
			zeroise( dlpParams->outParam, dlpParams->outLen );
			status = CRYPT_ERROR_FAILED;
			}
		}
	else
		{
		if( !memcmp( savedData, data, ENCRYPT_CHECKSIZE ) )
			{
			zeroise( data, dataLength );
			status = CRYPT_ERROR_FAILED;
			}
		}
	zeroise( savedData, ENCRYPT_CHECKSIZE );

	return( status );
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
	CONTEXT_INFO *contextInfoPtr = ( CONTEXT_INFO * ) objectInfoPtr;
	const CAPABILITY_INFO *capabilityInfo = contextInfoPtr->capabilityInfo;
	int status;

	assert( isWritePtr( objectInfoPtr, sizeof( CONTEXT_INFO ) ) );
	
	REQUIRES( message > MESSAGE_NONE && message < MESSAGE_LAST );
	REQUIRES( messageValue >= 0 && messageValue < MAX_INTLENGTH );

	/* Process destroy object messages */
	if( message == MESSAGE_DESTROY )
		{
		const CONTEXT_TYPE contextType = contextInfoPtr->type;

		REQUIRES( messageDataPtr == NULL && messageValue == 0 );

		/* Perform any algorithm-specific shutdown */
		if( capabilityInfo->endFunction != NULL )
			capabilityInfo->endFunction( contextInfoPtr );

		/* Perform context-type-specific cleanup */
		if( contextType == CONTEXT_PKC )
			freeContextBignums( contextInfoPtr->ctxPKC,
								contextInfoPtr->flags );

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
			return( getContextAttribute( contextInfoPtr, 
										 ( int * ) messageDataPtr,
										 messageValue ) );
		if( message == MESSAGE_GETATTRIBUTE_S )
			return( getContextAttributeS( contextInfoPtr, 
										  ( MESSAGE_DATA * ) messageDataPtr,
										  messageValue ) );
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
		assert( ( message == MESSAGE_CTX_HASH && \
				  ( messageValue == 0 || \
					isReadPtr( messageDataPtr, messageValue ) ) ) || \
				isWritePtr( messageDataPtr, messageValue ) );

		switch( message )
			{
			case MESSAGE_CTX_ENCRYPT:
				if( contextInfoPtr->type == CONTEXT_PKC )
					status = encryptDataPKC( contextInfoPtr, messageDataPtr, 
											 messageValue );
				else
					status = encryptDataConv( contextInfoPtr, messageDataPtr, 
											  messageValue );
				assert( cryptStatusOK( status ) );	/* Debug warning only */
				break;

			case MESSAGE_CTX_DECRYPT:
				REQUIRES( !needsKey( contextInfoPtr ) );
				REQUIRES( contextInfoPtr->type == CONTEXT_PKC || \
						  ( isStreamCipher( capabilityInfo->cryptAlgo ) || \
						    !needsIV( contextInfoPtr->ctxConv->mode ) ||
						    ( contextInfoPtr->flags & CONTEXT_FLAG_IV_SET ) ) );

				status = contextInfoPtr->decryptFunction( contextInfoPtr,
											messageDataPtr, messageValue );
				if( contextInfoPtr->type == CONTEXT_PKC && \
					!( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY ) )
					clearTempBignums( contextInfoPtr->ctxPKC );
				assert( cryptStatusOK( status ) );	/* Debug warning only */
				break;

			case MESSAGE_CTX_SIGN:
				status = capabilityInfo->signFunction( contextInfoPtr,
											messageDataPtr, messageValue );
				if( !( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY ) )
					clearTempBignums( contextInfoPtr->ctxPKC );
				assert( cryptStatusOK( status ) );	/* Debug warning only */
				break;

			case MESSAGE_CTX_SIGCHECK:
				status = capabilityInfo->sigCheckFunction( contextInfoPtr,
											messageDataPtr, messageValue );
				if( !( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY ) )
					clearTempBignums( contextInfoPtr->ctxPKC );
				break;

			case MESSAGE_CTX_HASH:
				REQUIRES( contextInfoPtr->type == CONTEXT_HASH || \
						  contextInfoPtr->type == CONTEXT_MAC );

				/* If we've already completed the hashing/MACing we can't
				   continue */
				if( contextInfoPtr->flags & CONTEXT_FLAG_HASH_DONE )
					return( CRYPT_ERROR_COMPLETE );

				status = capabilityInfo->encryptFunction( contextInfoPtr,
											messageDataPtr, messageValue );
				if( messageValue > 0 )
					{
					/* Usually the MAC initialisation happens when we load 
					   the key, but if we've deleted the MAC value to process 
					   another piece of data it'll happen on-demand so we 
					   have to set the flag here */
					contextInfoPtr->flags |= CONTEXT_FLAG_HASH_INITED;
					}
				else
					{
					/* Usually a hash of zero bytes is used to wrap up an
					   ongoing hash operation, however it can also be the 
					   only operation if a zero-byte string is being hashed.
					   To handle this we have to set the inited flag as well
					   as the done flag */
					contextInfoPtr->flags |= CONTEXT_FLAG_HASH_DONE | \
											 CONTEXT_FLAG_HASH_INITED;
					}
				assert( cryptStatusOK( status ) );	/* Debug warning only */
				break;

			default:
				retIntError();
			}
		return( status );
		}

	/* Process messages that compare object properties or clone the object */
	if( message == MESSAGE_COMPARE )
		{
		const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

		assert( isReadPtr( messageDataPtr, sizeof( MESSAGE_DATA ) ) );

		REQUIRES( messageValue == MESSAGE_COMPARE_HASH || \
				  messageValue == MESSAGE_COMPARE_ICV || \
				  messageValue == MESSAGE_COMPARE_KEYID || \
				  messageValue == MESSAGE_COMPARE_KEYID_PGP || \
				  messageValue == MESSAGE_COMPARE_KEYID_OPENPGP );

		switch( messageValue )
			{
			case MESSAGE_COMPARE_HASH:
				REQUIRES( contextInfoPtr->type == CONTEXT_HASH || \
						  contextInfoPtr->type == CONTEXT_MAC  );

				/* If it's a hash or MAC context, compare the hash value */
				if( !( contextInfoPtr->flags & CONTEXT_FLAG_HASH_DONE ) )
					return( CRYPT_ERROR_INCOMPLETE );
				if( contextInfoPtr->type == CONTEXT_HASH && \
					msgData->length == capabilityInfo->blockSize && \
					compareDataConstTime( msgData->data, 
										  contextInfoPtr->ctxHash->hash,
										  msgData->length ) )
					return( CRYPT_OK );
				if( contextInfoPtr->type == CONTEXT_MAC && \
					msgData->length == capabilityInfo->blockSize && \
					compareDataConstTime( msgData->data, 
										  contextInfoPtr->ctxMAC->mac,
										  msgData->length ) )
					return( CRYPT_OK );
				break;

			case MESSAGE_COMPARE_ICV:
				{
				BYTE icv[ CRYPT_MAX_HASHSIZE + 8 ];

				REQUIRES( contextInfoPtr->type == CONTEXT_CONV );

				if( contextInfoPtr->ctxConv->mode != CRYPT_MODE_GCM )
					return( CRYPT_ERROR_NOTAVAIL );
				status = capabilityInfo->getInfoFunction( CAPABILITY_INFO_ICV, 
									contextInfoPtr, icv, msgData->length );
				if( cryptStatusError( status ) )
					return( status );
				if( compareDataConstTime( msgData->data, icv, 
										  msgData->length ) )
					return( CRYPT_OK );
				break;
				}

			case MESSAGE_COMPARE_KEYID:
				REQUIRES( contextInfoPtr->type == CONTEXT_PKC );

				/* If it's a PKC context, compare the key ID */
				if( msgData->length == KEYID_SIZE && \
					!memcmp( msgData->data, contextInfoPtr->ctxPKC->keyID,
							 KEYID_SIZE ) )
					return( CRYPT_OK );
				break;

			case MESSAGE_COMPARE_KEYID_PGP:
				REQUIRES( contextInfoPtr->type == CONTEXT_PKC );

				/* If it's a PKC context, compare the PGP key ID */
				if( ( contextInfoPtr->flags & CONTEXT_FLAG_PGPKEYID_SET ) && \
					msgData->length == PGP_KEYID_SIZE && \
					!memcmp( msgData->data, contextInfoPtr->ctxPKC->pgp2KeyID,
							 PGP_KEYID_SIZE ) )
					return( CRYPT_OK );
				break;

			case MESSAGE_COMPARE_KEYID_OPENPGP:
				REQUIRES( contextInfoPtr->type == CONTEXT_PKC );

				/* If it's a PKC context, compare the OpenPGP key ID */
				if( ( contextInfoPtr->flags & CONTEXT_FLAG_OPENPGPKEYID_SET ) && \
					msgData->length == PGP_KEYID_SIZE && \
					!memcmp( msgData->data, contextInfoPtr->ctxPKC->openPgpKeyID,
							 PGP_KEYID_SIZE ) )
					return( CRYPT_OK );
				break;

			default:
				retIntError();
			}

		/* The comparison failed */
		return( CRYPT_ERROR );
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
				REQUIRES( contextInfoPtr->objectHandle != iCryptHandle );

				/* We've been cloned, update the object handle and internal 
				   state pointers */
				contextInfoPtr->objectHandle = iCryptHandle;
				status = capabilityInfo->getInfoFunction( CAPABILITY_INFO_STATEALIGNTYPE,
											NULL, &storageAlignSize, 0 );
				if( cryptStatusError( status ) )
					return( status );
				status = initContextStorage( contextInfoPtr, storageAlignSize );
				if( cryptStatusError( status ) )
					return( status );
				break;
				}

			case MESSAGE_CHANGENOTIFY_OWNERHANDLE:
				/* The second stage of a cloning, update the owner handle */
				contextInfoPtr->ownerHandle = *( ( int * ) messageDataPtr );
				break;

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

		REQUIRES( contextInfoPtr->type == CONTEXT_CONV || \
				  contextInfoPtr->type == CONTEXT_MAC || \
				  contextInfoPtr->type == CONTEXT_PKC || \
				  contextInfoPtr->type == CONTEXT_GENERIC );
		REQUIRES( needsKey( contextInfoPtr ) );

		/* If it's a private key context or a persistent context we need to 
		   have a key label set before we can continue */
		if( ( ( contextInfoPtr->type == CONTEXT_PKC ) || \
			  ( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT ) ) && \
			contextInfoPtr->labelSize <= 0 )
			return( CRYPT_ERROR_NOTINITED );

		/* Generate a new key into the context */
		status = contextInfoPtr->generateKeyFunction( contextInfoPtr );
		if( cryptStatusError( status ) )
			return( status );

		/* There's now a key loaded, remember this and disable further key 
		   generation.  The kernel won't allow a keygen anyway once the 
		   object is in the high state but taking this additional step can't 
		   hurt */
		contextInfoPtr->flags |= CONTEXT_FLAG_KEY_SET;
		return( krnlSendMessage( contextInfoPtr->objectHandle,
								 IMESSAGE_SETATTRIBUTE, 
								 ( MESSAGE_CAST ) &actionFlags, 
								 CRYPT_IATTRIBUTE_ACTIONPERMS ) );
		}
	if( message == MESSAGE_CTX_GENIV )
		{
		MESSAGE_DATA msgData;
		BYTE iv[ CRYPT_MAX_IVSIZE + 8 ];
		const int ivSize = capabilityInfo->blockSize;

		REQUIRES( contextInfoPtr->type == CONTEXT_CONV );

		/* If it's not a conventional encryption context or it's a mode that
		   doesn't use an IV then the generate IV operation is meaningless */
		if( !needsIV( contextInfoPtr->ctxConv->mode ) || \
			isStreamCipher( capabilityInfo->cryptAlgo ) )
			return( CRYPT_ERROR_NOTAVAIL );

		/* Generate a new IV and load it */
		setMessageData( &msgData, iv, ivSize );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusOK( status ) )
			status = capabilityInfo->initParamsFunction( contextInfoPtr,
												KEYPARAM_IV, iv, ivSize );
		return( status );
		}

	retIntError();
	}

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
	REQUIRES( objectFlags >= CREATEOBJECT_FLAG_NONE && \
			  objectFlags <= CREATEOBJECT_FLAG_MAX );
	REQUIRES( cryptAlgo > CRYPT_ALGO_NONE && \
			  cryptAlgo < CRYPT_ALGO_LAST );
	REQUIRES( contextType > CONTEXT_NONE && contextType < CONTEXT_LAST );

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
			status = capabilityInfoPtr->getInfoFunction( CAPABILITY_INFO_STATEALIGNTYPE,
												NULL, &stateStorageAlignSize, 0 );
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
				capabilityInfoPtr->encryptOFBFunction != NULL || \
				capabilityInfoPtr->encryptGCMFunction != NULL )
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT,
											   ACTION_PERM_ALL );
			if( capabilityInfoPtr->decryptFunction != NULL || \
				capabilityInfoPtr->decryptCBCFunction != NULL || \
				capabilityInfoPtr->decryptCFBFunction != NULL || \
				capabilityInfoPtr->decryptOFBFunction != NULL || \
				capabilityInfoPtr->decryptGCMFunction != NULL )
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_DECRYPT,
											   ACTION_PERM_ALL );
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
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT,
											   actionPerms );
			if( capabilityInfoPtr->decryptFunction != NULL )
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_DECRYPT,
											   actionPerms );
			if( capabilityInfoPtr->signFunction != NULL )
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGN,
											   actionPerms );
			if( capabilityInfoPtr->sigCheckFunction != NULL )
				actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK,
											   actionPerms );
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

		+-----------+					  --+
		|			|						|
		| CTX_INFO	| --- ctxConv --+		|
		|			|				|		| CONTEXT_INFO_ALIGN_SIZE
		+-----------+				|		|
		|###########| Pad to 8-byte boundary|
		+-----------+ <-------------+	  --+
		|			|						|
		| CONV_INFO	| ---- key -----+		| storageSize
		|			|				|		|
		+-----------+				|	  --+
		|###########| Pad to ALIGNSIZE boundary
		+-----------+ <-------------+	  --- Aligned by initContextStorage()
		|			|
		|	Key		|
		|			|
		+-----------+ 

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
	   being starting somewhere in the first 16 bytes of the allocated 
	   block.  The rest is done by initContextStorage() */
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
	contextInfoPtr->capabilityInfo = capabilityInfoPtr;
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
		contextInfoPtr->flags |= CONTEXT_FLAG_SIDECHANNELPROTECTION;
	if( contextInfoPtr->type == CONTEXT_PKC && \
		!( objectFlags & CREATEOBJECT_FLAG_DUMMY ) )
		{
		status = initContextBignums( contextInfoPtr->ctxPKC, 
									 sideChannelProtectionLevel,
									 isEccAlgo( cryptAlgo ) );
		if( cryptStatusError( status ) )
			{
			/* Enqueue a destroy message for the context and tell the kernel 
			   that we're done */
			krnlSendNotifier( *iCryptContext, IMESSAGE_DESTROY );
			krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE, 
							 MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
			return( status );
			}
		}
	if( contextInfoPtr->type == CONTEXT_CONV )
		{
		/* Set the default encryption mode, which is always CBC if possible,
		   and the corresponding en/decryption handler */
		if( capabilityInfoPtr->encryptCBCFunction != NULL )
			{
			contextInfoPtr->ctxConv->mode = CRYPT_MODE_CBC;
			contextInfoPtr->encryptFunction = \
									capabilityInfoPtr->encryptCBCFunction;
			contextInfoPtr->decryptFunction = \
									capabilityInfoPtr->decryptCBCFunction;
			}
		else
			{
			/* There's no CBC mode available, fall back to increasingly
			   sub-optimal choices of mode.  For stream ciphers the only 
			   available mode is OFB so this isn't a problem, but for 
			   block ciphers it'll cause problems because most crypto 
			   protocols only allow CBC mode.  In addition we don't fall
			   back to GCM, which is a sufficiently unusual mode that we
			   require it to be explicitly enabled by the user */
			if( capabilityInfoPtr->encryptCFBFunction != NULL )
				{
				contextInfoPtr->ctxConv->mode = CRYPT_MODE_CFB;
				contextInfoPtr->encryptFunction = \
									capabilityInfoPtr->encryptCFBFunction;
				contextInfoPtr->decryptFunction = \
									capabilityInfoPtr->decryptCFBFunction;
				}
			else
				{
				if( capabilityInfoPtr->encryptOFBFunction != NULL )
					{
					contextInfoPtr->ctxConv->mode = CRYPT_MODE_OFB;
					contextInfoPtr->encryptFunction = \
									capabilityInfoPtr->encryptOFBFunction;
					contextInfoPtr->decryptFunction = \
									capabilityInfoPtr->decryptOFBFunction;
					}
				else
					{
					contextInfoPtr->ctxConv->mode = CRYPT_MODE_ECB;
					contextInfoPtr->encryptFunction = \
									capabilityInfoPtr->encryptFunction;
					contextInfoPtr->decryptFunction = \
									capabilityInfoPtr->decryptFunction;
					}
				}
			}
		}
	else
		{
		/* There's only one possible en/decryption handler */
		contextInfoPtr->encryptFunction = capabilityInfoPtr->encryptFunction;
		contextInfoPtr->decryptFunction = capabilityInfoPtr->decryptFunction;
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
		initKeyRead( contextInfoPtr );
		initKeyWrite( contextInfoPtr );
		}

	REQUIRES( contextInfoPtr->type == CONTEXT_HASH || \
			  ( contextInfoPtr->loadKeyFunction != NULL && \
				contextInfoPtr->generateKeyFunction != NULL ) );
	REQUIRES( ( cryptAlgo == CRYPT_ALGO_DSA || \
				cryptAlgo == CRYPT_ALGO_ECDSA || \
				isSpecialAlgo( cryptAlgo ) ) || \
			  ( contextInfoPtr->encryptFunction != NULL && \
				contextInfoPtr->decryptFunction != NULL ) );
	REQUIRES( contextInfoPtr->type != CONTEXT_PKC || \
			  ( contextInfoPtr->ctxPKC->writePublicKeyFunction != NULL && \
				contextInfoPtr->ctxPKC->writePrivateKeyFunction != NULL && \
				contextInfoPtr->ctxPKC->readPublicKeyFunction != NULL && \
				contextInfoPtr->ctxPKC->readPrivateKeyFunction != NULL ) );

	/* If this is a dummy object remember that it's just a placeholder with 
	   actions handled externally.  If it's a persistent object (backed by a 
	   permanent key in a crypto device), record this */
	if( objectFlags & CREATEOBJECT_FLAG_DUMMY )
		contextInfoPtr->flags |= CONTEXT_FLAG_DUMMY;
	if( objectFlags & CREATEOBJECT_FLAG_PERSISTENT )
		contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;

	/* We've finished setting up the object type-specific info, tell the
	   kernel that the object is ready for use */
	status = krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusOK( status ) && contextInfoPtr->type == CONTEXT_HASH )
		{
		/* If it's a hash context there's no explicit keygen or load so we
		   need to send an "object initialised" message to get the kernel to
		   move it into the high state.  If this isn't done, any attempt to
		   use the object will be blocked */
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
	const CAPABILITY_INFO FAR_BSS *capabilityInfoPtr;
	int status;

	assert( isReadPtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );
	assert( isReadPtr( auxDataPtr, sizeof( CAPABILITY_INFO ) ) );

	REQUIRES( auxValue >= CREATEOBJECT_FLAG_NONE && \
			  auxValue <= CREATEOBJECT_FLAG_MAX );
	REQUIRES( createInfo->arg1 > CRYPT_ALGO_NONE && \
			  createInfo->arg1 < CRYPT_ALGO_LAST );

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
