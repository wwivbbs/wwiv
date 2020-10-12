/****************************************************************************
*																			*
*					cryptlib Encryption Context Action Routines				*
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
#else
  #include "context/context.h"
#endif /* Compiler-specific includes */

/* The number of bytes of data that we check to make sure that the 
   encryption operation succeeded.  See the comment in encryptData() before 
   changing this */

#define ENCRYPT_CHECKSIZE	16

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Perform a validity check on keying/state information in a context, used 
   to defend against side-channel attacks */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkContextStateData( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;
	int status = CRYPT_OK;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES_B( sanityCheckContext( contextInfoPtr ) );
	REQUIRES_B( contextInfoPtr->type == CONTEXT_CONV || \
				contextInfoPtr->type == CONTEXT_PKC )

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES_B( capabilityInfoPtr != NULL );

	/* If it's a context with the keying information held externally then we 
	   can't check it */
	if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) )
		return( TRUE );

	/* Make sure that the keying information hasn't been corrupted */
	if( contextInfoPtr->type == CONTEXT_PKC )
		{
		status = checksumContextData( contextInfoPtr->ctxPKC, 
					capabilityInfoPtr->cryptAlgo, 
					TEST_FLAG( contextInfoPtr->flags, 
							   CONTEXT_FLAG_ISPUBLICKEY ) ? FALSE : TRUE );
		}
	else
		{
		const CONV_INFO *convInfo = contextInfoPtr->ctxConv;

		if( checksumData( convInfo->key, \
						  convInfo->keyDataSize ) != convInfo->keyDataChecksum )
			status = CRYPT_ERROR_FAILED;
		}
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "%s key memory corruption detected for object %d", 
					 capabilityInfoPtr->algoName, 
					 contextInfoPtr->objectHandle ));
		assert( DEBUG_WARN );
		return( FALSE );
		}

	return( TRUE );
	}

/* Recover from an en/decryption failure.  This replaces the data being en/
   decrypted/signed/verified with appropriate values to ensure that no 
   plaintext or other sensitive information is leaked even if the caller
   ignores the return code */

STDC_NONNULL_ARG( ( 1 ) ) \
static void sanitiseFailedData( INOUT_BUFFER_FIXED( dataLength ) void *data, 
								IN_LENGTH_Z const int dataLength,
								IN_MESSAGE const MESSAGE_TYPE message,
								IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	void *dataPtr = data;
	int length = dataLength, status;

	assert( isWritePtrDynamic( data, dataLength ) );

	REQUIRES_V( isIntegerRange( dataLength ) );
	REQUIRES_V( message >= MESSAGE_CTX_ENCRYPT && message <= MESSAGE_CTX_HASH );
	REQUIRES_V( isEnumRange( cryptAlgo, CRYPT_ALGO ) );

	/* If it's a PKC algorithm then the input may be structured data, so we 
	   have to extract the reference to the actual data being processed from
	   it.  This gets a bit complicated because, depending on the point at 
	   which the operation failed, the output-length may have been cleared 
	   (alongside the output data).  To deal with this we use the maximum
	   length possible for keyex, and either the full output length (if it's
	   available) or at least the minimum permitted length for a DLP/ECDLP 
	   operation (2 * SHA-1 size) if it's not */
	if( isPkcAlgo( cryptAlgo ) )
		{
		if( isKeyxAlgo( cryptAlgo ) )
			{
			KEYAGREE_PARAMS *keyAgreeParams = ( KEYAGREE_PARAMS * ) data;

			dataPtr = ( message == MESSAGE_CTX_ENCRYPT ) ? \
					  keyAgreeParams->publicValue : keyAgreeParams->wrappedKey;
			length = CRYPT_MAX_PKCSIZE;
			}
		else
			{
			if( isDlpAlgo( cryptAlgo ) || isEccAlgo( cryptAlgo ) )
				{
				DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) data;

				dataPtr = dlpParams->outParam;
				length = max( dlpParams->outLen, 20 + 20 );
				}
			}
		}

	/* If it's a failed en/decrypt then we replace the data with random 
	   noise.  On encrypt this means that the plaintext is replaced with 
	   non-decryptable garbage that looks encrypted.  On decrypt this means 
	   that the plaintext is also replaced with garbage, for decryption of 
	   data this doesn't really matter but for decryption of keying material 
	   it means that we continue with junk keys that don't reveal anything 
	   to an attacker */
	if( message == MESSAGE_CTX_ENCRYPT || message == MESSAGE_CTX_DECRYPT )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, dataPtr, length );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusError( status ) )
			{
			/* The attempt to fill with random garbage failed, fall back to
			   fixed, but non-zero, data */
			memset( dataPtr, '*', length );
			}
		}
	else
		{
		/* It's a failed sign/signature verify, clear the output to ensure
		   that nothing is leaked */
		memset( dataPtr, 0, length );
		}
	}

/****************************************************************************
*																			*
*								Encrypt Data								*
*																			*
****************************************************************************/

/* Encrypt a block of data using a conventional cipher */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptDataConv( INOUT CONTEXT_INFO *contextInfoPtr, 
							INOUT_BUFFER_FIXED( dataLength ) void *data, 
							IN_LENGTH_Z const int dataLength )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;
	CTX_ENCRYPT_FUNCTION encryptFunction;
	const int savedDataLength = min( dataLength, ENCRYPT_CHECKSIZE );
	BYTE savedData[ ENCRYPT_CHECKSIZE + 8 ];
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( data, dataLength ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_CONV );
	REQUIRES( !needsKey( contextInfoPtr ) );
	REQUIRES( isIntegerRange( dataLength ) );

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( isStreamCipher( capabilityInfoPtr->cryptAlgo ) || \
			  !needsIV( contextInfoPtr->ctxConv->mode ) ||
			  TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_IV_SET ) );

	/* Get function pointers for the context */
	encryptFunction = ( CTX_ENCRYPT_FUNCTION ) \
					  FNPTR_GET( contextInfoPtr->encryptFunction );
	REQUIRES( encryptFunction != NULL );

	/* Save a copy of the plaintext, and encrypt it */
	REQUIRES( rangeCheck( savedDataLength, 1, ENCRYPT_CHECKSIZE ) );
	memcpy( savedData, data, savedDataLength );
	status = encryptFunction( contextInfoPtr, data, dataLength );
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
		status = CRYPT_ERROR_FAILED;

	zeroise( savedData, savedDataLength );
	return( status );
	}

/* Encrypt a block of data using a PKC */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptDataPKC( INOUT CONTEXT_INFO *contextInfoPtr, 
						   INOUT_BUFFER_FIXED( dataLength ) void *data, 
						   IN_LENGTH_PKC const int dataLength )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;
	CTX_ENCRYPT_FUNCTION encryptFunction;
	BYTE savedData[ ENCRYPT_CHECKSIZE + 8 ];
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( data, dataLength ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC );
	REQUIRES( !needsKey( contextInfoPtr ) );

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );

	/* Get function pointers for the context */
	encryptFunction = ( CTX_ENCRYPT_FUNCTION ) \
					  FNPTR_GET( contextInfoPtr->encryptFunction );
	REQUIRES( encryptFunction != NULL );

	/* Key agreement algorithms are treated as a special case since they 
	   don't actually encrypt the data */
	if( isKeyxAlgo( capabilityInfoPtr->cryptAlgo ) )
		{
		REQUIRES( dataLength == sizeof( KEYAGREE_PARAMS ) );

		return( encryptFunction( contextInfoPtr, data, dataLength ) );
		}

	/* DLP algorithms have composite parameters and are handled differently 
	   from standard algorithms */
	if( isDlpAlgo( capabilityInfoPtr->cryptAlgo ) || \
		isEccAlgo( capabilityInfoPtr->cryptAlgo ) )
		{
		DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) data;

		REQUIRES( dataLength == sizeof( DLP_PARAMS ) );

		/* Save a copy of the plaintext, and encrypt it */
		memcpy( savedData, dlpParams->inParam1, ENCRYPT_CHECKSIZE );
		status = encryptFunction( contextInfoPtr, data, dataLength );
		if( cryptStatusError( status ) )
			{
			zeroise( savedData, ENCRYPT_CHECKSIZE );
			return( status );
			}

		/* Check for a catastrophic failure of the encryption */
		if( !memcmp( savedData, dlpParams->outParam, 
					 ENCRYPT_CHECKSIZE ) )
			status = CRYPT_ERROR_FAILED;

		zeroise( savedData, ENCRYPT_CHECKSIZE );

		return( status );
		}

	REQUIRES( dataLength >= MIN_PKCSIZE && \
			  dataLength <= CRYPT_MAX_PKCSIZE );

	/* Save a copy of the plaintext, and encrypt it */
	memcpy( savedData, data, ENCRYPT_CHECKSIZE );
	status = encryptFunction( contextInfoPtr, data, dataLength );
	if( cryptStatusError( status ) )
		{
		zeroise( savedData, ENCRYPT_CHECKSIZE );
		return( status );
		}

	/* Check for a catastrophic failure of the encryption */
	if( !memcmp( savedData, data, ENCRYPT_CHECKSIZE ) )
		status = CRYPT_ERROR_FAILED;

	zeroise( savedData, ENCRYPT_CHECKSIZE );

	return( status );
	}

/****************************************************************************
*																			*
*								Encryption Data								*
*																			*
****************************************************************************/

/* Process an action message */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int processActionMessage( INOUT CONTEXT_INFO *contextInfoPtr, 
						  IN_MESSAGE const MESSAGE_TYPE message,
						  INOUT_BUFFER_FIXED( dataLength ) void *data, 
						  IN_LENGTH_PKC const int dataLength )
	{
	const CAPABILITY_INFO *capabilityInfoPtr;
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( ( message == MESSAGE_CTX_HASH && \
			  ( dataLength == 0 || \
			    isReadPtrDynamic( data, dataLength ) ) ) || \
			isWritePtrDynamic( data, dataLength ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( message >= MESSAGE_CTX_ENCRYPT && message <= MESSAGE_CTX_HASH );
	REQUIRES( isIntegerRange( dataLength ) );

	/* Get the capability info for the context */
	capabilityInfoPtr = DATAPTR_GET( contextInfoPtr->capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );

	switch( message )
		{
		case MESSAGE_CTX_ENCRYPT:
			if( !checkContextStateData( contextInfoPtr ) )
				return( CRYPT_ERROR_FAILED );
			if( contextInfoPtr->type == CONTEXT_PKC )
				status = encryptDataPKC( contextInfoPtr, data, dataLength );
			else
				status = encryptDataConv( contextInfoPtr, data, dataLength );
			if( contextInfoPtr->type == CONTEXT_PKC && \
				!TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) )
				clearTempBignums( contextInfoPtr->ctxPKC );
			if( cryptStatusOK( status ) && \
				!checkContextStateData( contextInfoPtr ) )
				status = CRYPT_ERROR_FAILED;
			if( cryptStatusError( status ) )
				{
				assert_nofuzz( DEBUG_WARN );
				sanitiseFailedData( data, dataLength, message, 
									capabilityInfoPtr->cryptAlgo );
				}
			break;

		case MESSAGE_CTX_DECRYPT:
			{
			const CTX_ENCRYPT_FUNCTION decryptFunction = \
						( CTX_ENCRYPT_FUNCTION ) \
						FNPTR_GET( contextInfoPtr->decryptFunction );

			REQUIRES( decryptFunction != NULL );

			if( !checkContextStateData( contextInfoPtr ) )
				return( CRYPT_ERROR_FAILED );
			status = decryptFunction( contextInfoPtr, data, dataLength );
			if( contextInfoPtr->type == CONTEXT_PKC && \
				!TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) )
				clearTempBignums( contextInfoPtr->ctxPKC );
			if( cryptStatusOK( status ) && \
				!checkContextStateData( contextInfoPtr ) )
				status = CRYPT_ERROR_FAILED;
			if( cryptStatusError( status ) )
				{
				assert_nofuzz( DEBUG_WARN );
				sanitiseFailedData( data, dataLength, message, 
									capabilityInfoPtr->cryptAlgo );
				}
			break;
			}

		case MESSAGE_CTX_SIGN:
			if( !checkContextStateData( contextInfoPtr ) )
				return( CRYPT_ERROR_FAILED );
			status = capabilityInfoPtr->signFunction( contextInfoPtr,
													  data, dataLength );
			if( !TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) )
				clearTempBignums( contextInfoPtr->ctxPKC );
			if( cryptStatusOK( status ) && \
				!checkContextStateData( contextInfoPtr ) )
				status = CRYPT_ERROR_FAILED;
			if( cryptStatusError( status ) )
				{
				assert( DEBUG_WARN );
				sanitiseFailedData( data, dataLength, message, 
									capabilityInfoPtr->cryptAlgo );
				}
			break;

		case MESSAGE_CTX_SIGCHECK:
			if( !checkContextStateData( contextInfoPtr ) )
				return( CRYPT_ERROR_FAILED );
			status = capabilityInfoPtr->sigCheckFunction( contextInfoPtr,
														  data, dataLength );
			if( !TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) )
				clearTempBignums( contextInfoPtr->ctxPKC );
			if( cryptStatusOK( status ) && \
				!checkContextStateData( contextInfoPtr ) )
				status = CRYPT_ERROR_FAILED;
			if( cryptStatusError( status ) && !isDataError( status ) )
				{
				assert( DEBUG_WARN );
				sanitiseFailedData( data, dataLength, message, 
									capabilityInfoPtr->cryptAlgo );
				}
			break;

		case MESSAGE_CTX_HASH:
			{
			/* We don't check the state for these since there's not much 
			   that can be done in terms of an attack, we'll just produce a
			   random hash/MAC value that can't be verified */
			REQUIRES( contextInfoPtr->type == CONTEXT_HASH || \
					  contextInfoPtr->type == CONTEXT_MAC );

			/* If we've already completed the hashing/MACing then we can't 
			   continue */
			if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_HASH_DONE ) )
				return( CRYPT_ERROR_COMPLETE );

			status = capabilityInfoPtr->encryptFunction( contextInfoPtr,
														 data, dataLength );
			if( cryptStatusError( status ) )
				{
				assert( DEBUG_WARN );
				break;
				}
			if( dataLength > 0 )
				{
				/* Usually the MAC initialisation happens when we load the 
				   key, but if we've deleted the MAC value to process 
				   another piece of data it'll happen on-demand so we have 
				   to set the flag here */
				SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_HASH_INITED );
				}
			else
				{
				/* Usually a hash of zero bytes is used to wrap up an
				   ongoing hash operation, however it can also be the only 
				   operation if a zero-byte string is being hashed.  To 
				   handle this we have to set the inited flag as well as the 
				   done flag */
				SET_FLAG( contextInfoPtr->flags,
						  CONTEXT_FLAG_HASH_DONE | CONTEXT_FLAG_HASH_INITED );
				}
			break;
			}

		default:
			retIntError();
		}

	return( status );
	}
