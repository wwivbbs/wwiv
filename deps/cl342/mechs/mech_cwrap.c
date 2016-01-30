/****************************************************************************
*																			*
*				cryptlib Conventional Key Wrap Mechanism Routines			*
*					  Copyright Peter Gutmann 1992-2008						*
*																			*
****************************************************************************/

#ifdef INC_ALL
  #include "crypt.h"
  #include "mech_int.h"
#else
  #include "crypt.h"
  #include "mechs/mech_int.h"
#endif /* Compiler-specific includes */

/* The size of the key block header, an 8-bit length followed by a 24-bit 
   check value */

#define CMS_KEYBLOCK_HEADERSIZE		4

#ifdef USE_INT_CMS

/****************************************************************************
*																			*
*							CMS Wrap/Unwrap Mechanisms						*
*																			*
****************************************************************************/

/* Determine how many padding bytes are required for a CMS conventional key 
   wrap */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int getPadSize( IN_HANDLE const CRYPT_CONTEXT iExportContext,
					   IN_RANGE( MIN_KEYSIZE, CRYPT_MAX_KEYSIZE ) \
						const int payloadSize, 
					   OUT_LENGTH_SHORT_Z int *padSize )
	{
	int blockSize, totalSize, status;

	assert( isWritePtr( padSize, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( iExportContext ) );
	REQUIRES( payloadSize >= MIN_KEYSIZE && \
			  payloadSize <= CRYPT_MAX_KEYSIZE );

	/* Clear return value */
	*padSize = 0;

	status = krnlSendMessage( iExportContext, IMESSAGE_GETATTRIBUTE,
							  &blockSize, CRYPT_CTXINFO_IVSIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Determine the padding size, which is the amount of padding required to
	   bring the total data size up to a multiple of the block size with a
	   minimum size of two blocks.  Unlike PKCS #5 padding, the total may be 
	   zero */
	totalSize = roundUp( payloadSize, blockSize );
	if( totalSize < blockSize * 2 )
		totalSize = blockSize * 2;
	ENSURES( !( totalSize & ( blockSize - 1 ) ) );
	*padSize = totalSize - payloadSize;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							CMS Wrap/Unwrap Mechanisms						*
*																			*
****************************************************************************/

/* Perform CMS data wrapping */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportCMS( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	MESSAGE_DATA msgData;
	BYTE *keyBlockPtr = ( BYTE * ) mechanismInfo->wrappedData;
	BYTE dataSample[ 16 + 8 ];
	int keySize, padSize, status = CRYPT_OK;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	/* Clear the return value */
	if( mechanismInfo->wrappedData != NULL )
		{
		memset( mechanismInfo->wrappedData, 0,
				mechanismInfo->wrappedDataLength );
		}

	/* Get the key payload details from the key contexts */
	status = krnlSendMessage( mechanismInfo->keyContext, 
							  IMESSAGE_GETATTRIBUTE, &keySize,
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );
	status = getPadSize( mechanismInfo->wrapContext, 
						 CMS_KEYBLOCK_HEADERSIZE + keySize, &padSize );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( padSize >= 0 && padSize <= CRYPT_MAX_IVSIZE );

	/* If this is just a length check, we're done */
	if( mechanismInfo->wrappedData == NULL )
		{
		mechanismInfo->wrappedDataLength = \
							CMS_KEYBLOCK_HEADERSIZE + keySize + padSize;
		return( CRYPT_OK );
		}
	ANALYSER_HINT( mechanismInfo->wrappedDataLength > ( 2 * 8 ) && \
				   mechanismInfo->wrappedDataLength < MAX_INTLENGTH_SHORT );

	/* Make sure that the wrapped key data fits in the output */
	if( CMS_KEYBLOCK_HEADERSIZE + \
					keySize + padSize > mechanismInfo->wrappedDataLength )
		return( CRYPT_ERROR_OVERFLOW );

	/* Pad the payload out with a random nonce if required */
	if( padSize > 0 )
		{
		setMessageData( &msgData, 
						keyBlockPtr + CMS_KEYBLOCK_HEADERSIZE + keySize, 
						padSize );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Format the key block:

		|<--- C_K_HDRSIZE + keySize --->|<- padSz ->|
		+-------+-----------+-----------+-----------+
		| length| chk.value	|	key		|  padding	|
		+-------+-----------+-----------+-----------+
		|<-	1 ->|<--- 3 --->|
		
	   then copy the payload in at the last possible moment and perform two 
	   passes of encryption, retaining the IV from the first pass for the 
	   second pass */
	keyBlockPtr[ 0 ] = intToByte( keySize );
	status = extractKeyData( mechanismInfo->keyContext,
							 keyBlockPtr + CMS_KEYBLOCK_HEADERSIZE,
							 keySize, "keydata", 7 );
	keyBlockPtr[ 1 ] = intToByte( keyBlockPtr[ CMS_KEYBLOCK_HEADERSIZE ] ^ 0xFF );
	keyBlockPtr[ 2 ] = intToByte( keyBlockPtr[ CMS_KEYBLOCK_HEADERSIZE + 1 ] ^ 0xFF );
	keyBlockPtr[ 3 ] = intToByte( keyBlockPtr[ CMS_KEYBLOCK_HEADERSIZE + 2 ] ^ 0xFF );
	memcpy( dataSample, keyBlockPtr, 16 );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_ENCRYPT,
								  mechanismInfo->wrappedData,
								  CMS_KEYBLOCK_HEADERSIZE + keySize + \
									padSize );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_ENCRYPT,
								  mechanismInfo->wrappedData,
								  CMS_KEYBLOCK_HEADERSIZE + keySize + \
									padSize );
		}
	if( cryptStatusOK( status ) && !memcmp( dataSample, keyBlockPtr, 16 ) )
		{
		/* The data to wrap is unchanged, there's been a catastrophic 
		   failure of the encryption.  We don't do a retIntError() at this
		   point because we want to at least continue and zeroise the data
		   first */
		assert( FALSE );
		status = CRYPT_ERROR_FAILED;
		}
	zeroise( dataSample, 16 );
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->wrappedData,
				 mechanismInfo->wrappedDataLength );
		return( status );
		}
	mechanismInfo->wrappedDataLength = CMS_KEYBLOCK_HEADERSIZE + \
									   keySize + padSize;

	return( CRYPT_OK );
	}

/* Perform CMS data unwrapping */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importCMS( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ CRYPT_MAX_KEYSIZE + CRYPT_MAX_IVSIZE + 8 ];
	BYTE ivBuffer[ CRYPT_MAX_IVSIZE + 8 ];
	BYTE *dataEndPtr = buffer + mechanismInfo->wrappedDataLength;
	int blockSize, value, status;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	/* Make sure that the data is a multiple of the cipher block size and 
	   contains at least two encrypted blocks */
	status = krnlSendMessage( mechanismInfo->wrapContext,
							  IMESSAGE_GETATTRIBUTE, &blockSize,
							  CRYPT_CTXINFO_IVSIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( mechanismInfo->wrappedDataLength & ( blockSize - 1 ) )
		return( CRYPT_ERROR_BADDATA );
	if( mechanismInfo->wrappedDataLength < 2 * blockSize )
		return( CRYPT_ERROR_UNDERFLOW );
	if( mechanismInfo->wrappedDataLength > CRYPT_MAX_KEYSIZE + blockSize )
		{
		/* This has already been checked in general via a kernel ACL but we 
		   can perform a more specific check here because we now know the 
		   encryption block size */
		return( CRYPT_ERROR_OVERFLOW );
		}
	ANALYSER_HINT( mechanismInfo->wrappedDataLength >= 8 && \
				   mechanismInfo->wrappedDataLength < CRYPT_MAX_KEYSIZE + 16 );

	/* Save the current IV for the inner decryption */
	setMessageData( &msgData, ivBuffer, CRYPT_MAX_IVSIZE );
	status = krnlSendMessage( mechanismInfo->wrapContext, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData, 
							  CRYPT_CTXINFO_IV );
	if( cryptStatusError( status ) )
		return( status );

	/* Decrypt the n'th block using the n-1'th ciphertext block as the new 
	   IV.  Then decrypt the remainder of the ciphertext blocks using the 
	   decrypted n'th ciphertext block as the IV.  Technically some of the 
	   checks of the return value aren't necessary because the CBC-MAC that 
	   we're performing at the end will detect any failure to set a 
	   parameter but we perform them anyway just to be sure */
	memcpy( buffer, mechanismInfo->wrappedData,
			mechanismInfo->wrappedDataLength );
	setMessageData( &msgData, dataEndPtr - ( 2 * blockSize ), blockSize );
	status = krnlSendMessage( mechanismInfo->wrapContext, 
							  IMESSAGE_SETATTRIBUTE_S, &msgData, 
							  CRYPT_CTXINFO_IV );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_DECRYPT, 
								  dataEndPtr - blockSize, blockSize );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, dataEndPtr - blockSize, blockSize );
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_CTXINFO_IV );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_DECRYPT, buffer,
								  mechanismInfo->wrappedDataLength - blockSize );
		}
	if( cryptStatusError( status ) )
		{
		zeroise( buffer, CRYPT_MAX_KEYSIZE + CRYPT_MAX_IVSIZE );
		return( status );
		}

	/* Decrypt the inner data using the original IV */
	setMessageData( &msgData, ivBuffer, blockSize );
	status = krnlSendMessage( mechanismInfo->wrapContext, 
							  IMESSAGE_SETATTRIBUTE_S, &msgData, 
							  CRYPT_CTXINFO_IV );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_DECRYPT, buffer,
								  mechanismInfo->wrappedDataLength );
		}
	if( cryptStatusError( status ) )
		{
		zeroise( buffer, CRYPT_MAX_KEYSIZE + CRYPT_MAX_IVSIZE );
		return( status );
		}

	/* Make sure that everything is in order and load the decrypted keying 
	   information into the session key context.  This uses a somewhat
	   odd checking mechanism in order to avoid timing attacks (although it's
	   not really certain what an attacker would gain by an ability to do 
	   this).  The checks being performed are:

		if( buffer[ 0 ] < MIN_KEYSIZE || \
			buffer[ 0 ] > MAX_WORKING_KEYSIZE || \
			buffer[ 0 ] > mechanismInfo->wrappedDataLength - \
						  CMS_KEYBLOCK_HEADERSIZE || \
			buffer[ 1 ] != ( buffer[ CMS_KEYBLOCK_HEADERSIZE ] ^ 0xFF ) || \
			buffer[ 2 ] != ( buffer[ CMS_KEYBLOCK_HEADERSIZE + 1 ] ^ 0xFF ) || \
			buffer[ 3 ] != ( buffer[ CMS_KEYBLOCK_HEADERSIZE + 2 ] ^ 0xFF ) )
			error; 

	   If this check fails then it could be due to corruption of the wrapped
	   data but is far more likely to be because the incorrect unwrap key was
	   used, so we return a CRYPT_ERROR_WRONGKEY instead of a 
	   CRYPT_ERROR_BADDATA */
	value = ( buffer[ 0 ] < MIN_KEYSIZE ) | \
			( buffer[ 0 ] > MAX_WORKING_KEYSIZE ) | \
			( buffer[ 0 ] > mechanismInfo->wrappedDataLength - \
							CMS_KEYBLOCK_HEADERSIZE );
	value |= buffer[ 1 ] ^ ( buffer[ CMS_KEYBLOCK_HEADERSIZE ] ^ 0xFF );
	value |= buffer[ 2 ] ^ ( buffer[ CMS_KEYBLOCK_HEADERSIZE + 1 ] ^ 0xFF );
	value |= buffer[ 3 ] ^ ( buffer[ CMS_KEYBLOCK_HEADERSIZE + 2 ] ^ 0xFF );
	if( value != 0 )
		{
		zeroise( buffer, CRYPT_MAX_KEYSIZE + CRYPT_MAX_IVSIZE );
		return( CRYPT_ERROR_WRONGKEY );
		}

	/* Load the recovered key into the session key context */
	setMessageData( &msgData, buffer + CMS_KEYBLOCK_HEADERSIZE, buffer[ 0 ] );
	status = krnlSendMessage( mechanismInfo->keyContext,
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_KEY );
	if( cryptArgError( status ) )
		{
		/* If there was an error with the key value or size, convert the
		   return value into something more appropriate */
		status = CRYPT_ERROR_BADDATA;
		}
	zeroise( buffer, CRYPT_MAX_KEYSIZE + CRYPT_MAX_IVSIZE );

	return( status );
	}
#endif /* USE_INT_CMS */
