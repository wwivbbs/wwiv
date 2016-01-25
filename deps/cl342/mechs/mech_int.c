/****************************************************************************
*																			*
*					cryptlib Internal Mechanism Routines					*
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

/****************************************************************************
*																			*
*									PKC Routines							*
*																			*
****************************************************************************/

/* The length of the input data for PKCS #1 transformations is usually
   determined by the key size but sometimes we can be passed data that has 
   been zero-padded (for example data coming from an ASN.1 INTEGER in which 
   the high bit is a sign bit) making it longer than the key size, or that 
   has leading zero byte(s) making it shorter than the key size.  The best 
   place to handle this is somewhat uncertain, it's an encoding issue so it 
   probably shouldn't be visible to the raw crypto routines but putting it 
   at the mechanism layer removes the algorithm-independence of that layer 
   and putting it at the mid-level sign/key-exchange routine layer both 
   removes the algorithm-independence and requires duplication of the code 
   for signatures and encryption.  The best place to put it seems to be at 
   the mechanism layer since an encoding issue really shouldn't be visible 
   at the crypto layer and because it would require duplicating the handling 
   every time a new PKC implementation is plugged in.

   The intent of the size adjustment is to make the data size match the key
   length.  If it's longer we try to strip leading zero bytes.  If it's 
   shorter we pad it with zero bytes to match the key size.  The result is
   either the data adjusted to match the key size or CRYPT_ERROR_BADDATA if
   this isn't possible */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int adjustPKCS1Data( OUT_BUFFER_FIXED( outDataMaxLen ) BYTE *outData, 
					 IN_LENGTH_SHORT_MIN( CRYPT_MAX_PKCSIZE ) \
						const int outDataMaxLen, 
					 IN_BUFFER( inLen ) const BYTE *inData, 
					 IN_LENGTH_SHORT const int inLen, 
					 IN_LENGTH_SHORT const int keySize )
	{
	int length = inLen;

	assert( isWritePtr( outData, outDataMaxLen ) );
	assert( isReadPtr( inData, inLen ) );

	REQUIRES( outDataMaxLen >= CRYPT_MAX_PKCSIZE && \
			  outDataMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( inLen > 0 && inLen <= outDataMaxLen && \
			  inLen < MAX_INTLENGTH_SHORT );
	REQUIRES( keySize >= MIN_PKCSIZE && keySize <= CRYPT_MAX_PKCSIZE );
	REQUIRES( outData != inData );

	/* Make sure that the result will fit in the output buffer.  This has 
	   already been checked by the kernel mechanism ACL and by the 
	   REQUIRES() predicate above but we make the check explicit here */
	if( keySize > outDataMaxLen )
		return( CRYPT_ERROR_OVERFLOW );

	/* Find the start of the data payload.  If it's suspiciously short, 
	   don't try and process it */
	for( length = inLen; 
		 length >= MIN_PKCSIZE - 8 && *inData == 0;
		 length--, inData++ );
	if( length < MIN_PKCSIZE - 8 || length > keySize )
		return( CRYPT_ERROR_BADDATA );

	/* If it's of the correct size, exit */
	if( length == keySize )
		{
		memcpy( outData, inData, keySize );
		return( CRYPT_OK );
		}

	/* We've adjusted the size to account for zero-padding during encoding,
	   now we have to move the data into a fixed-length format to match the
	   key size.  To do this we copy the payload into the output buffer with
	   enough leading-zero bytes to bring the total size up to the key size */
	memset( outData, 0, keySize );
	memcpy( outData + ( keySize - length ), inData, length );

	return( CRYPT_OK );
	}

/* Get PKC algorithm parameters */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int getPkcAlgoParams( IN_HANDLE const CRYPT_CONTEXT pkcContext,
					  OUT_OPT_ALGO_Z CRYPT_ALGO_TYPE *pkcAlgo, 
					  OUT_LENGTH_PKC_Z int *pkcKeySize )
	{
	int status;

	assert( ( pkcAlgo == NULL ) || \
			isWritePtr( pkcAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( isWritePtr( pkcKeySize, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( pkcContext ) );

	/* Clear return values */
	if( pkcAlgo != NULL )
		*pkcAlgo = CRYPT_ALGO_NONE;
	*pkcKeySize = 0;

	/* Get various PKC algorithm parameters */
	if( pkcAlgo != NULL )
		{
		status = krnlSendMessage( pkcContext, IMESSAGE_GETATTRIBUTE, 
								  pkcAlgo, CRYPT_CTXINFO_ALGO );
		if( cryptStatusError( status ) )
			return( status );
		}
	return( krnlSendMessage( pkcContext, IMESSAGE_GETATTRIBUTE, 
							 pkcKeySize, CRYPT_CTXINFO_KEYSIZE ) );
	}

/****************************************************************************
*																			*
*									Hash Routines							*
*																			*
****************************************************************************/

/* Get hash algorithm parameters */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int getHashAlgoParams( IN_HANDLE const CRYPT_CONTEXT hashContext,
					   OUT_ALGO_Z CRYPT_ALGO_TYPE *hashAlgo, 
					   OUT_OPT_LENGTH_HASH_Z int *hashSize )
	{
	int status;

	assert( isWritePtr( hashAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( ( hashSize == NULL ) || \
			isWritePtr( hashSize, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( hashContext ) );

	/* Clear return values */
	*hashAlgo = CRYPT_ALGO_NONE;
	if( hashSize != NULL )
		*hashSize = 0;

	/* Get various PKC algorithm parameters */
	if( hashSize != NULL )
		{
		status = krnlSendMessage( hashContext, IMESSAGE_GETATTRIBUTE, 
								  hashSize, CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			return( status );
		}
	return( krnlSendMessage( hashContext, IMESSAGE_GETATTRIBUTE, 
							 hashAlgo, CRYPT_CTXINFO_ALGO ) );
	}

