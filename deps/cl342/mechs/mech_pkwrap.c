/****************************************************************************
*																			*
*				cryptlib PKC Key Wrap Mechanism Routines					*
*					Copyright Peter Gutmann 1992-2008						*
*																			*
****************************************************************************/

#ifdef INC_ALL
  #include "crypt.h"
  #include "mech_int.h"
  #include "pgp.h"
  #include "asn1.h"
  #include "misc_rw.h"
#else
  #include "crypt.h"
  #include "mechs/mech_int.h"
  #include "misc/pgp.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/misc_rw.h"
#endif /* Compiler-specific includes */

/* Fixed-format maximum-length padding string that works for both PKCS #1 
   and OAEP.  This is used as a dummy value to allow processing to continue 
   in case the decrypt operation fails, reducing the scope for timing
   attacks */

#if CRYPT_MAX_PKCSIZE > 512
  #error Need to adjust dummy PKCS #1/OAEP data value to match CRYPT_MAX_PKCSIZE
#endif /* CRYPT_MAX_PKCSIZE size check */

static const BYTE fixedFormattedValue[] = {
	/* 128 bytes / 1024 bits */
	0x00, 0x02, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	/* 256 bytes / 2048 bits */
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	/* 384 bytes / 3072 bits */
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	/* 512 bytes / 4096 bits */
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
	};

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

#if defined( USE_PGP ) || defined( USE_PGPKEYS )

/* PGP checksums the PKCS #1 wrapped data even though this doesn't really
   serve any purpose since any decryption error will corrupt the PKCS #1
   padding, the following routine calculates this checksum and either 
   appends it to the data or checks it against the stored value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int pgpGenerateChecksum( INOUT_BUFFER_FIXED( dataLength ) void *data, 
								IN_RANGE( 1, CRYPT_MAX_PKCSIZE + UINT16_SIZE ) \
									const int dataLength,
								IN_LENGTH_PKC const int keyDataLength )
	{
	STREAM stream;
	BYTE *dataPtr = data;
	int checksum = 0, i, status;

	assert( isWritePtr( data, dataLength ) );

	REQUIRES( dataLength > 0 && \
			  dataLength <= CRYPT_MAX_PKCSIZE + UINT16_SIZE );
	REQUIRES( keyDataLength == dataLength - UINT16_SIZE );

	/* Calculate the checksum for the MPI */
	for( i = 0; i < keyDataLength; i++ )
		checksum += dataPtr[ i ];

	/* Append the checksum to the MPI data */
	sMemOpen( &stream, dataPtr + keyDataLength, dataLength - keyDataLength );
	status = writeUint16( &stream, checksum );
	sMemDisconnect( &stream );

	return( status );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN pgpVerifyChecksum( IN_BUFFER( dataLength ) const void *data, 
								  IN_RANGE( 1, CRYPT_MAX_PKCSIZE + UINT16_SIZE ) \
									const int dataLength )
	{
	STREAM stream;
	int checksum = 0, storedChecksum, i;

	assert( isReadPtr( data, dataLength ) );

	REQUIRES_B( dataLength > UINT16_SIZE && \
				dataLength <= CRYPT_MAX_PKCSIZE + UINT16_SIZE );

	/* Calculate the checksum for the MPI and compare it to the appended 
	   checksum.  We don't have to check the return status of sgetc() 
	   because an error status returned will cause the checksum comparison
	   to fail */
	sMemConnect( &stream, data, dataLength );
	for( i = 0; i < dataLength - UINT16_SIZE; i++ )
		{
		const int ch = sgetc( &stream );
		if( cryptStatusError( ch ) )
			{
			sMemDisconnect( &stream );
			return( FALSE );
			}
		checksum += ch;
		}
	storedChecksum = readUint16( &stream );
	sMemDisconnect( &stream );

	return( storedChecksum == checksum );
	}

/* PGP includes the session key information alongside the encrypted key so
   it's not really possible to import the key into a context in the
   conventional sense.  Instead, the import code has to create the context
   as part of the import process and return it to the caller.  This is ugly,
   but less ugly than doing a raw import and handling the key directly in
   the calling code */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int pgpExtractKey( OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
						  IN_BUFFER( dataLength ) const BYTE *data, 
						  IN_LENGTH_SHORT const int dataLength )
	{
	CRYPT_ALGO_TYPE cryptAlgo = CRYPT_ALGO_NONE;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	static const int mode = CRYPT_MODE_CFB;	/* int vs.enum */
	int status;

	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( dataLength >= MIN_KEYSIZE && dataLength <= CRYPT_MAX_PKCSIZE );

	/* Clear return value */
	*iCryptContext = CRYPT_ERROR;

	/* Get the session key algorithm.  We delay checking the algorithm ID
	   until after the checksum calculation to reduce the chance of being
	   used as an oracle.

	   Note that there are three different IDs for AES depending on the
	   keysize that's being used, we ignore these since the key size is
	   specified explicitly via the unwrapped key */
	status = pgpToCryptlibAlgo( data[ 0 ], PGP_ALGOCLASS_CRYPT, &cryptAlgo );

	/* Checksum the session key, skipping the algorithm ID at the start and
	   the checksum at the end.  This is actually superfluous since any
	   decryption error will be caught by corrupted PKCS #1 padding with
	   vastly higher probability than this simple checksum, but we do it
	   anyway because other PGP implementations do it too */
	if( !pgpVerifyChecksum( data + 1, dataLength - 1 ) )
		return( CRYPT_ERROR_BADDATA );

	/* Make sure that the algorithm ID is valid.  We only perform the check 
	   at this point because this returns a different error code than the 
	   usual bad-data, we want to be absolutely sure that the problem really 
	   is an unknown algorithm and not the result of scrambled decrypted 
	   data */
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Create the context ready to have the key loaded into it */
	setMessageCreateObjectInfo( &createInfo, cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &mode, CRYPT_CTXINFO_MODE );
	if( cryptStatusError( status ) )
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	else
		*iCryptContext = createInfo.cryptHandle;

	return( CRYPT_OK );
	}
#endif /* USE_PGP || USE_PGPKEYS */

/****************************************************************************
*																			*
*						Low-level Data Wrap/Unwrap Routines					*
*																			*
****************************************************************************/

/* Wrap/unwrap data using a public/private-key context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int pkcWrapData( INOUT MECHANISM_WRAP_INFO *mechanismInfo,
						INOUT_BUFFER_FIXED( wrappedDataLength ) BYTE *wrappedData, 
						IN_LENGTH_PKC const int wrappedDataLength,
						const BOOLEAN usePgpWrap, const BOOLEAN isDlpAlgo )
	{
	BYTE dataSample[ 16 + 8 ];
	const void *samplePtr = wrappedData + ( wrappedDataLength / 2 );
	int status;

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );
	assert( isWritePtr( wrappedData, wrappedDataLength ) );

	REQUIRES( wrappedDataLength >= MIN_PKCSIZE && \
			  wrappedDataLength <= CRYPT_MAX_PKCSIZE );
	REQUIRES( wrappedData[ 0 ] == 0x00 && \
			  ( wrappedData[ 1 ] == 0x01 || wrappedData[ 1 ] == 0x02 ) );

	/* Take a sample of the input for comparison with the output */
	memcpy( dataSample, samplePtr, 16 );

	if( isDlpAlgo )
		{
		DLP_PARAMS dlpParams;

		/* For DLP-based PKC's the output length isn't the same as the key
		   size so we adjust the return length as required */
		setDLPParams( &dlpParams, wrappedData, wrappedDataLength, 
					  wrappedData, mechanismInfo->wrappedDataLength );
		if( usePgpWrap )
			dlpParams.formatType = CRYPT_FORMAT_PGP;
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_ENCRYPT, &dlpParams,
								  sizeof( DLP_PARAMS ) );
		if( cryptStatusOK( status ) )
			mechanismInfo->wrappedDataLength = dlpParams.outLen;
		}
	else
		{
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_ENCRYPT, wrappedData, 
								  wrappedDataLength );
		if( cryptStatusOK( status ) )
			mechanismInfo->wrappedDataLength = wrappedDataLength;
		}
	if( cryptStatusOK( status ) && !memcmp( dataSample, samplePtr, 16 ) )
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
		/* There was a problem with the wrapping, clear the output value */
		zeroise( wrappedData, wrappedDataLength );
		}

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int pkcUnwrapData( MECHANISM_WRAP_INFO *mechanismInfo, 
						  INOUT_BUFFER( dataMaxLength, *dataOutLength ) \
							BYTE *data, 
						  IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) \
							const int dataMaxLength, 
						  OUT_LENGTH_PKC_Z int *dataOutLength, 
						  IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) \
							const int dataInLength, 
						  const BOOLEAN usePgpWrap, 
						  const BOOLEAN isDlpAlgo )
	{
	int status;

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );
	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataOutLength, sizeof( int ) ) );

	REQUIRES( dataMaxLength >= MIN_PKCSIZE && \
			  dataMaxLength <= MAX_INTLENGTH_SHORT );
	REQUIRES( dataInLength >= MIN_PKCSIZE && \
			  dataInLength <= dataMaxLength && \
			  dataInLength <= MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( data, 0, min( 16, dataMaxLength ) );
	*dataOutLength = 0;

	if( isDlpAlgo )
		{
		DLP_PARAMS dlpParams;

		setDLPParams( &dlpParams, mechanismInfo->wrappedData,
					  mechanismInfo->wrappedDataLength, data, 
					  dataMaxLength );
		if( usePgpWrap )
			dlpParams.formatType = CRYPT_FORMAT_PGP;
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_DECRYPT, &dlpParams,
								  sizeof( DLP_PARAMS ) );
		if( cryptStatusOK( status ) )
			{
			*dataOutLength = dlpParams.outLen;
			return( CRYPT_OK );
			}
		}
	else
		{
		status = adjustPKCS1Data( data, dataMaxLength,
								  mechanismInfo->wrappedData,
								  mechanismInfo->wrappedDataLength, 
								  dataInLength );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( mechanismInfo->wrapContext,
									  IMESSAGE_CTX_DECRYPT, data,
									  dataInLength );
		if( cryptStatusOK( status ) )
			{
			*dataOutLength = dataInLength;
			return( CRYPT_OK );
			}
		}

	/* There was a problem with the wrapping, clear the output value */
	zeroise( data, dataMaxLength );
	return( status );
	}

/****************************************************************************
*																			*
*							PKCS #1 Wrap/Unwrap Mechanisms					*
*																			*
****************************************************************************/

/* Generate/recover a PKCS #1 data block */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int generatePkcs1DataBlock( INOUT_BUFFER( dataMaxLen, *dataLength ) \
									BYTE *data, 
								   IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) \
									const int dataMaxLen, 
								   OUT_LENGTH_SHORT_Z int *dataLength, 
								   IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) \
									const int messageLen )
	{
	MESSAGE_DATA msgData;
	const int padSize = dataMaxLen - ( messageLen + 3 );
	int status;

	assert( isWritePtr( data, dataMaxLen ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( dataMaxLen >= MIN_PKCSIZE && dataMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( messageLen >= MIN_KEYSIZE && messageLen < dataMaxLen && \
			  messageLen < MAX_INTLENGTH_SHORT );
	
	/* Clear return values */
	memset( data, 0, min( 16, dataMaxLen ) );
	*dataLength = 0;

	/* Determine PKCS #1 padding parameters and make sure that the key is 
	   long enough to encrypt the payload.  PKCS #1 requires that the 
	   maximum payload size be 11 bytes less than the length to give a 
	   minimum of 8 bytes of random padding */
	if( messageLen > dataMaxLen - 11 )
		return( CRYPT_ERROR_OVERFLOW );

	ENSURES( padSize >= 8 && messageLen + padSize + 3 <= dataMaxLen );

	/* Encode the payload using the PKCS #1 format:
	   
		[ 0 ][ 2 ][ nonzero random padding ][ 0 ][ payload ]

	   Note that the random padding is a nice place for a subliminal channel,
	   especially with the larger public key sizes where you can communicate 
	   more information in the padding than you can in the payload */
	data[ 0 ] = 0;
	data[ 1 ] = 2;
	setMessageData( &msgData, data + 2, padSize );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_RANDOM_NZ );
	if( cryptStatusError( status ) )
		{
		zeroise( data, dataMaxLen );
		return( status );
		}
	data[ 2 + padSize ] = 0;
	*dataLength = 2 + padSize + 1;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int recoverPkcs1DataBlock( IN_BUFFER( dataLength ) const BYTE *data, 
								  IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) \
									const int dataLength, 
								  OUT_LENGTH_SHORT_Z int *padSize )
	{
	int ch, length;

	assert( isReadPtr( data, dataLength ) );
	assert( isWritePtr( padSize, sizeof( int ) ) );

	REQUIRES( dataLength >= MIN_PKCSIZE && dataLength < MAX_INTLENGTH );

	/* Clear return value */
	*padSize = 0;

	/* Undo the PKCS #1 padding:

		[ 0 ][ 2 ][ random nonzero padding ][ 0 ][ payload ]
	
	   with a minimum of 8 bytes padding.  Note that some implementations 
	   may have bignum code that zero-truncates the result which will 
	   produce a CRYPT_ERROR_BADDATA error when we undo the padding, it's 
	   the responsibility of the lower-level crypto layer to reformat the 
	   data to return a correctly-formatted result if necessary.

	   In order to avoid being used as a decription timing oracle we bundle
	   all of the formatting checks into a single location and make the code 
	   as simple and quick as possible.  At best an attacker will get only a 
	   few clock cycles of timing information, which should be lost in the 
	   general noise */
	if( dataLength < 11 + MIN_KEYSIZE )
		{
		/* PKCS #1 padding requires at least 11 (2 + 8 + 1) bytes of 
		   padding data, if there isn't this much present then what we've 
		   got can't be a valid payload */
		return( CRYPT_ERROR_BADDATA );
		}
	if( data[ 0 ] != 0x00 || data[ 1 ] != 0x02 )
		return( CRYPT_ERROR_BADDATA );
	for( length = 2, ch = 0xFF; 
		 length < dataLength - MIN_KEYSIZE && \
			( ch = byteToInt( data[ length ] ) ) != 0x00; 
		 length++ );
	if( ch != 0x00 || length < 11 )
		return( CRYPT_ERROR_BADDATA );
	length++;	/* Skip the final 0x00 */

	/* Sanity check to make sure that the padding data looks OK.  We only do 
	   this in debug mode since it's a probabalistic test and we don't want 
	   to bail out due to a false positive in production code */
	assert( checkEntropy( data + 2, length - 1 ) );

	/* Make sure that there's enough room left after the remaining PKCS #1 
	   padding to hold at least a minimum-length key */
	if( dataLength - length < MIN_KEYSIZE )
		return( CRYPT_ERROR_BADDATA );

	*padSize = length;

	return( CRYPT_OK );
	}

/* Perform PKCS #1 wrapping/unwrapping.  There are several variations of
   this that are handled through common PKCS #1 mechanism functions */

typedef enum { PKCS1_WRAP_NONE, PKCS1_WRAP_NORMAL, PKCS1_WRAP_RAW, 
			   PKCS1_WRAP_PGP, PKCS1_WRAP_LAST } PKCS1_WRAP_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int pkcs1Wrap( INOUT MECHANISM_WRAP_INFO *mechanismInfo,
					  IN_ENUM( PKCS1_WRAP ) const PKCS1_WRAP_TYPE type )
	{
	CRYPT_ALGO_TYPE cryptAlgo;
	BYTE *wrappedData = mechanismInfo->wrappedData, *dataPtr;
	int payloadSize, length, dataBlockSize, status;
#ifdef USE_PGP
	int pgpAlgoID = DUMMY_INIT;
#endif /* USE_PGP */

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	REQUIRES( type > PKCS1_WRAP_NONE && type < PKCS1_WRAP_LAST );

	/* Clear return value */
	if( mechanismInfo->wrappedData != NULL )
		{
		memset( mechanismInfo->wrappedData, 0,
				mechanismInfo->wrappedDataLength );
		}

	/* Get various algorithm parameters */
	status = getPkcAlgoParams( mechanismInfo->wrapContext, &cryptAlgo, 
							   &length );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( length > MIN_PKCSIZE && length <= CRYPT_MAX_PKCSIZE );

	/* If this is just a length check, we're done */
	if( mechanismInfo->wrappedData == NULL )
		{
		/* Determine how long the encrypted value will be.  In the case of
		   Elgamal it's just an estimate since it can change by up to two
		   bytes depending on whether the values have the high bit set or
		   not, which requires zero-padding of the ASN.1-encoded integers.
		   This is rather nasty because it means that we can't tell how 
		   large an encrypted value will be without actually creating it.  
		   The 10-byte length at the start is for the ASN.1 SEQUENCE (= 4) 
		   and 2 * INTEGER (= 2*3) encoding */
		mechanismInfo->wrappedDataLength = \
							( cryptAlgo == CRYPT_ALGO_ELGAMAL ) ? \
							10 + ( 2 * ( length + 1 ) ) : length;
		return( CRYPT_OK );
		}

	/* Make sure that there's enough room for the wrapped key data */
	if( length > mechanismInfo->wrappedDataLength )
		return( CRYPT_ERROR_OVERFLOW );

	/* Get the payload details, either as data passed in by the caller or
	   from the key context */
	if( type == PKCS1_WRAP_RAW )
		payloadSize = mechanismInfo->keyDataLength;
	else
		{
		status = krnlSendMessage( mechanismInfo->keyContext,
								  IMESSAGE_GETATTRIBUTE, &payloadSize,
								  CRYPT_CTXINFO_KEYSIZE );
		if( cryptStatusError( status ) )
			return( status );
		}
#ifdef USE_PGP
	if( type == PKCS1_WRAP_PGP )
		{
		int sessionKeyAlgo;	/* int vs.enum */

		/* PGP includes an additional algorithm specifier and checksum with
		   the wrapped key so we adjust the length to take this into
		   account */
		status = krnlSendMessage( mechanismInfo->keyContext,
								  IMESSAGE_GETATTRIBUTE, &sessionKeyAlgo,
								  CRYPT_CTXINFO_ALGO );
		if( cryptStatusOK( status ) )
			status = cryptlibToPgpAlgo( sessionKeyAlgo, &pgpAlgoID );
		if( cryptStatusError( status ) )
			return( status );
		payloadSize += 3;	/* 1-byte algo ID + 2-byte checksum */
		}
#endif /* USE_PGP */

	/* Perform a preliminary check for an excessively long payload to make
	   it explicit, however generatePkcs1DataBlock() will also perform a 
	   more precise check when it performs the data formatting */
	if( payloadSize >= length )
		return( CRYPT_ERROR_OVERFLOW );

	/* Generate the PKCS #1 data block with room for the payload at the end */
	status = generatePkcs1DataBlock( wrappedData, length, &dataBlockSize, 
									 payloadSize );
	if( cryptStatusError( status ) )
		{
		zeroise( wrappedData, length );
		return( status );
		}
	ENSURES( dataBlockSize + payloadSize == length );

	/* Copy the payload in at the last possible moment, then encrypt it */
	dataPtr = wrappedData + dataBlockSize;
	switch( type )
		{
		case PKCS1_WRAP_NORMAL:
			status = extractKeyData( mechanismInfo->keyContext, dataPtr,
									 payloadSize, "keydata", 7 );
			break;

		case PKCS1_WRAP_RAW:
			memcpy( dataPtr, mechanismInfo->keyData, payloadSize );
			break;

#ifdef USE_PGP
		case PKCS1_WRAP_PGP:
			*dataPtr++ = intToByte( pgpAlgoID );
			status = extractKeyData( mechanismInfo->keyContext, dataPtr,
									 payloadSize - 3, "keydata", 7 );
			if( cryptStatusOK( status ) )
				{
				status = pgpGenerateChecksum( dataPtr, payloadSize - 1,
									payloadSize - ( 1 + UINT16_SIZE ) );
				}
			break;
#endif /* USE_PGP */

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		{
		zeroise( wrappedData, length );
		return( status );
		}

	/* Wrap the encoded data using the public key */
	return( pkcWrapData( mechanismInfo, wrappedData, length,
						 ( type == PKCS1_WRAP_PGP ) ? TRUE : FALSE,
						 ( cryptAlgo == CRYPT_ALGO_ELGAMAL ) ? TRUE : FALSE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int pkcs1Unwrap( INOUT MECHANISM_WRAP_INFO *mechanismInfo,
						IN_ENUM( PKCS1_WRAP ) const PKCS1_WRAP_TYPE type )
	{
	CRYPT_ALGO_TYPE cryptAlgo;
	MESSAGE_DATA msgData;
	BYTE decryptedData[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE *decryptedDataPtr = decryptedData;
	const BYTE *payloadPtr;
	int length, dataBlockSize, unwrapStatus = CRYPT_OK, status;

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	REQUIRES( type > PKCS1_WRAP_NONE && type < PKCS1_WRAP_LAST );

	/* Clear the return value if we're returning raw data */
	if( type == PKCS1_WRAP_RAW )
		memset( mechanismInfo->keyData, 0, mechanismInfo->keyDataLength );

	/* Get various algorithm parameters */
	status = getPkcAlgoParams( mechanismInfo->wrapContext, &cryptAlgo, 
							   &length );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( length > MIN_PKCSIZE && length <= CRYPT_MAX_PKCSIZE );

	/* Decrypt the data */
	status = pkcUnwrapData( mechanismInfo, decryptedData, CRYPT_MAX_PKCSIZE,
							&length, length, 
							( type == PKCS1_WRAP_PGP ) ? TRUE : FALSE,
							( cryptAlgo == CRYPT_ALGO_ELGAMAL ) ? TRUE : FALSE );
	if( cryptStatusError( status ) )
		{
		/* There was a problem with the decryption operation, continue with 
		   dummy PKCS #1-formatted data.  It's uncertain what it would take 
		   to actually encounter this situation since there's no obvious way 
		   to cause it by manipulating the input data (at least in a manner 
		   that causes the decryption to fail in the middle of the 
		   operation rather than just rejecting the input data without even 
		   trying to decrypt it, for example by making it larger than the 
		   modulus for RSA), but it could be possible to achieve through a 
		   memory-corruption attack, causing processing to bail out before 
		   the key-validity check at the end catches the corruption.  On the 
		   other hand skipping half the decrypt will cause a much larger 
		   timing difference than the padding check, so we're probably not 
		   achieving that much here.  Since it doesn't cost anything, we 
		   defend against it anyway */
		decryptedDataPtr = ( BYTE * ) fixedFormattedValue;
		unwrapStatus = status;
		}

	/* Recover the PKCS #1 data block, with the payload at the end */
	status = recoverPkcs1DataBlock( decryptedDataPtr, length, 
									&dataBlockSize );
	if( cryptStatusError( unwrapStatus ) )
		{
		/* If there was an error at the decrypt stage, propagate that rather 
		   than returning the generic CRYPT_ERROR_BADDATA that we'd get from
		   the padding check of the dummy data */
		status = unwrapStatus;
		}
	if( cryptStatusError( status ) )
		{
		zeroise( decryptedData, CRYPT_MAX_PKCSIZE );
		return( status );
		}
	payloadPtr = decryptedData + dataBlockSize;
	length -= dataBlockSize;

	/* Return the result to the caller or load it into a context as a key */
	switch( type )
		{
#ifdef USE_PGP
		case PKCS1_WRAP_PGP:
			/* PGP includes extra wrapping around the key so we have to
			   process that before we can load it */
			status = pgpExtractKey( &mechanismInfo->keyContext, payloadPtr, 
									length );
			if( cryptStatusError( status ) )
				break;
			payloadPtr++;		/* Skip algorithm ID */
			length -= 3;		/* Subtract extra wrapping length */
			if( length < MIN_KEYSIZE )
				return( CRYPT_ERROR_BADDATA );
			/* Fall through */
#endif /* USE_PGP */

		case PKCS1_WRAP_NORMAL:
			/* Load the decrypted keying information into the session key
			   context */
			setMessageData( &msgData, ( MESSAGE_CAST ) payloadPtr, length );
			status = krnlSendMessage( mechanismInfo->keyContext,
									  IMESSAGE_SETATTRIBUTE_S, &msgData,
									  CRYPT_CTXINFO_KEY );
			if( cryptArgError( status ) )
				{
				/* If there was an error with the key value or size convert 
				   the return value into something more appropriate */
				status = CRYPT_ERROR_BADDATA;
				}
			break;

		case PKCS1_WRAP_RAW:
			/* Return the result to the caller */
			if( length > mechanismInfo->keyDataLength )
				status = CRYPT_ERROR_OVERFLOW;
			else
				{
				memcpy( mechanismInfo->keyData, payloadPtr, length );
				mechanismInfo->keyDataLength = length;
				}
			break;

		default:
			retIntError();
		}
	zeroise( decryptedData, CRYPT_MAX_PKCSIZE );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportPKCS1( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( pkcs1Wrap( mechanismInfo,
					   ( mechanismInfo->keyContext == CRYPT_UNUSED ) ? \
					   PKCS1_WRAP_RAW : PKCS1_WRAP_NORMAL ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPKCS1( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( pkcs1Unwrap( mechanismInfo,
						 ( mechanismInfo->keyData != NULL ) ? \
						 PKCS1_WRAP_RAW : PKCS1_WRAP_NORMAL ) );
	}

#ifdef USE_PGP

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportPKCS1PGP( STDC_UNUSED void *dummy, 
					INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( pkcs1Wrap( mechanismInfo, PKCS1_WRAP_PGP ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPKCS1PGP( STDC_UNUSED void *dummy, 
					INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( pkcs1Unwrap( mechanismInfo, PKCS1_WRAP_PGP ) );
	}
#endif /* USE_PGP */

/****************************************************************************
*																			*
*							OAEP Key Wrap/Unwrap Mechanisms					*
*																			*
****************************************************************************/

#ifdef USE_OAEP

/* Get the lHash value used for OAEP.  In theory this should be a hash of a 
   label applied to the OAEP operation but this is never used so what ends
   up being used is a fixed hash of an empty string.  Since this is 
   constant we can use a pre-calculated value for each hash algorithm */

typedef struct {
	const CRYPT_ALGO_TYPE hashAlgo;
	BUFFER_FIXED( lHashSize ) \
	const BYTE FAR_BSS *lHash;
	const int lHashSize;
	} LHASH_INFO;

static const LHASH_INFO FAR_BSS lHashInfo[] = {
	{ CRYPT_ALGO_SHA1, ( const BYTE * )		/* For pedantic compilers */
	  "\xDA\x39\xA3\xEE\x5E\x6B\x4B\x0D\x32\x55\xBF\xEF\x95\x60\x18\x90"
	  "\xAF\xD8\x07\x09", 20 },
	{ CRYPT_ALGO_SHA2, ( const BYTE * )		/* For pedantic compilers */
	  "\xE3\xB0\xC4\x42\x98\xFC\x1C\x14\x9A\xFB\xF4\xC8\x99\x6F\xB9\x24"
	  "\x27\xAE\x41\xE4\x64\x9B\x93\x4C\xA4\x95\x99\x1B\x78\x52\xB8\x55", 32 },
#ifdef USE_SHA2_EXT
	/* SHA2-512 is only available on systems with 64-bit data type support */
	{ CRYPT_ALGO_SHA2, ( const BYTE * )	/* For pedantic compilers */
	  "\xCF\x83\xE1\x35\x7E\xEF\xB8\xBD\xF1\x54\x28\x50\xD6\x6D\x80\x07"
	  "\xD6\x20\xE4\x05\x0B\x57\x15\xDC\x83\xF4\xA9\x21\xD3\x6C\xE9\xCE"
	  "\x47\xD0\xD1\x3C\x5D\x85\xF2\xB0\xFF\x83\x18\xD2\x87\x7E\xEC\x2F"
	  "\x63\xB9\x31\xBD\x47\x41\x7A\x81\xA5\x38\x32\x7A\xF9\x27\xDA\x3E", 64 },
#endif /* USE_SHA2_EXT */
	{ CRYPT_ALGO_NONE, NULL, 0 }, { CRYPT_ALGO_NONE, NULL, 0 }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int getOaepHash( OUT_BUFFER_OPT( lHashMaxLen, *lHashLen ) void *lHash, 
						IN_LENGTH_SHORT_Z const int lHashMaxLen, 
						OUT_LENGTH_SHORT_Z int *lHashLen,
						IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
						IN_INT_SHORT_Z const int hashParam )
	{
	int i;

	assert( ( lHash == NULL && lHashMaxLen == 0 ) || \
			isWritePtr( lHash, lHashMaxLen ) );
	
	REQUIRES( ( lHash == NULL && lHashMaxLen == 0 ) || \
			  ( lHashMaxLen >= CRYPT_MAX_HASHSIZE && \
				lHashMaxLen < MAX_INTLENGTH_SHORT ) );
	REQUIRES( isHashAlgo( hashAlgo ) );

	/* Clear return value */
	if( lHash != NULL )
		zeroise( lHash, lHashMaxLen );
	*lHashLen = 0;

	for( i = 0; lHashInfo[ i ].hashAlgo != CRYPT_ALGO_NONE && \
				i < FAILSAFE_ARRAYSIZE( lHashInfo, LHASH_INFO ); i++ )
		{
		if( lHashInfo[ i ].hashAlgo != hashAlgo )
			continue;
		if( hashParam != 0 && lHashInfo[ i ].lHashSize != hashParam )
			continue;
		if( lHash != NULL )
			memcpy( lHash, lHashInfo[ i ].lHash, lHashInfo[ i ].lHashSize );
		*lHashLen = lHashInfo[ i ].lHashSize;

		return( CRYPT_OK );
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( lHashInfo, LHASH_INFO ) );
	if( lHash != NULL )
		zeroise( lHash, lHashMaxLen );

	return( CRYPT_ERROR_NOTAVAIL );
	}

#define getOaepHashSize( hashLen, hashAlgo, hashParam ) \
		getOaepHash( NULL, 0, hashLen, hashAlgo, hashParam )

/* OAEP mask generation function MGF1 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int mgf1( OUT_BUFFER_FIXED( maskLen ) void *mask, 
				 IN_LENGTH_PKC const int maskLen, 
				 IN_BUFFER( seedLen ) const void *seed, 
				 IN_LENGTH_PKC const int seedLen,
				 IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
				 IN_INT_SHORT_Z const int hashParam )
	{
	HASHFUNCTION hashFunction;
	HASHINFO hashInfo;
	BYTE countBuffer[ 4 + 8 ], maskBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	BYTE *maskOutPtr = mask;
	int hashSize, maskIndex, blockCount = 0, iterationCount;

	assert( isWritePtr( mask, maskLen ) );
	assert( isReadPtr( seed, seedLen ) );

	REQUIRES( maskLen >= 16 && maskLen <= CRYPT_MAX_PKCSIZE );
	REQUIRES( seedLen >= 16 && seedLen <= CRYPT_MAX_PKCSIZE );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashParam >= 0 && hashParam < MAX_INTLENGTH_SHORT );

	getHashParameters( hashAlgo, hashParam, &hashFunction, &hashSize );

	/* Set up the block counter buffer.  This will never have more than the
	   last few bits set (8 bits = 5120 bytes of mask for the smallest hash,
	   SHA-1) so we only change the last byte */
	memset( countBuffer, 0, 4 );

	/* Produce enough blocks of output to fill the mask */
	for( maskIndex = 0, iterationCount = 0; 
		 maskIndex < maskLen && iterationCount < FAILSAFE_ITERATIONS_MED;
		 maskIndex += hashSize, maskOutPtr += hashSize, iterationCount++ )
		{
		const int noMaskBytes = ( maskLen - maskIndex > hashSize ) ? \
								hashSize : maskLen - maskIndex;

		/* Calculate hash( seed || counter ) */
		countBuffer[ 3 ] = ( BYTE ) blockCount++;
		hashFunction( hashInfo, NULL, 0, seed, seedLen, HASH_STATE_START );
		hashFunction( hashInfo, maskBuffer, hashSize, countBuffer, 4, 
					  HASH_STATE_END );
		memcpy( maskOutPtr, maskBuffer, noMaskBytes );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	zeroise( hashInfo, sizeof( HASHINFO ) );
	zeroise( maskBuffer, CRYPT_MAX_HASHSIZE );

	return( CRYPT_OK );
	}

/* Generate/recover an OAEP data block:

							 +----------+---------+-------+
						DB = |  lHash   |    PS   |   M   |
							 +----------+---------+-------+
											|
				  +----------+				V
				  |   seed   |--> MGF ---> xor
				  +----------+				|
						|					|
			   +--+		V					|
			   |00|	   xor <----- MGF <-----|
			   +--+		|					|
				 |		|					|
				 V		V					V
			   +--+----------+----------------------------+
		 EM =  |00|maskedSeed|          maskedDB          |
			   +--+----------+----------------------------+ 
						|					|
						V					V
					   xor <----- MGF <-----|
						|					|
						V					|
				  +----------+				V
				  |   seed   |--> MGF ---> xor
				  +----------+				|
											V
							 +----------+---------+-------+
						DB = |  lHash   |    PS   |   M   |
							 +----------+---------+-------+ */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int generateOaepDataBlock( OUT_BUFFER_FIXED( dataMaxLen ) BYTE *data, 
								  IN_LENGTH_PKC const int dataMaxLen, 
								  IN_BUFFER( messageLen ) const void *message, 
								  IN_RANGE( MIN_KEYSIZE, CRYPT_MAX_KEYSIZE ) \
									const int messageLen,
								  IN_BUFFER( seedLen ) const void *seed, 
								  IN_LENGTH_PKC const int seedLen,
								  IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
								  IN_INT_SHORT_Z const int hashParam )
	{
	BYTE dbMask[ CRYPT_MAX_PKCSIZE + 8 ], seedMask[ CRYPT_MAX_HASHSIZE + 8 ];
	BYTE *maskedSeed, *db;
	int dbLen, i, length, status;

	assert( isWritePtr( data, dataMaxLen ) );
	assert( isReadPtr( message, messageLen ) );
	assert( isReadPtr( seed, seedLen ) );

	REQUIRES( dataMaxLen >= MIN_PKCSIZE && dataMaxLen <= CRYPT_MAX_PKCSIZE );
	REQUIRES( messageLen >= MIN_KEYSIZE && messageLen <= dataMaxLen && \
			  messageLen <= CRYPT_MAX_KEYSIZE );
	REQUIRES( seedLen >= 16 && seedLen <= dataMaxLen && \
			  seedLen <= CRYPT_MAX_PKCSIZE );
	REQUIRES( cryptStatusOK( \
				getOaepHashSize( &i, hashAlgo, hashParam ) ) && seedLen == i );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashParam >= 0 && hashParam < MAX_INTLENGTH_SHORT );

	/* Make sure that the payload fits:

		<------------ dataMaxLen ----------->
		+--+------+-------+----+--+---------+
		|00| seed | lhash | PS |01| message |
		+--+------+-------+----+--+---------+
		  1	 hLen	 hLen	 1	 1	 msgLen

	   Although PS may have a length of zero bytes we require at least one
	   padding byte.  In general the only case where we can ever run into 
	   problems is if we try and use SHA2-512 with a 1024-bit key */
	if( 1 + seedLen + seedLen + 1 + 1 + messageLen > dataMaxLen )
		return( CRYPT_ERROR_OVERFLOW );

	/* Calculate the size and position of the various data quantities */
	maskedSeed = data + 1;
	db = maskedSeed + seedLen;
	dbLen = dataMaxLen - ( 1 + seedLen );

	ENSURES( dbLen >= 16 && dbLen >= messageLen + 1 && \
			 1 + seedLen + dbLen <= dataMaxLen );

	/* db = lHash || zeroes || 0x01 || message */
	memset( db, 0, dbLen );
	status = getOaepHash( db, CRYPT_MAX_PKCSIZE, &length, hashAlgo, 
						  hashParam );
	if( cryptStatusError( status ) )
		return( status );
	db[ dbLen - messageLen - 1 ] = 0x01;
	memcpy( db + dbLen - messageLen, message, messageLen );

	ENSURES( length == seedLen );
	
	/* dbMask = MGF1( seed, dbLen ) */
	status = mgf1( dbMask, dbLen, seed, seedLen, hashAlgo, hashParam );
	ENSURES( cryptStatusOK( status ) );

	/* maskedDB = db ^ dbMask */
	for( i = 0; i < dbLen; i++ )
		db[ i ] ^= dbMask[ i ];

	/* seedMask = MGF1( maskedDB, seedLen ) */
	status = mgf1( seedMask, seedLen, db, dbLen, hashAlgo, hashParam );
	ENSURES( cryptStatusOK( status ) );

	/* maskedSeed = seed ^ seedMask */
	for( i = 0; i < seedLen; i++ )
		{
		maskedSeed[ i ] = \
			intToByte( ( ( const BYTE * ) seed )[ i ] ^ seedMask[ i ] );
		}

	/* data = 0x00 || maskedSeed || maskedDB */
	data[ 0 ] = 0x00;

	zeroise( dbMask, CRYPT_MAX_PKCSIZE );
	zeroise( seedMask, CRYPT_MAX_HASHSIZE );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int recoverOaepDataBlock( OUT_BUFFER( messageMaxLen, *messageLen ) \
									BYTE *message, 
								 IN_LENGTH_PKC const int messageMaxLen, 
								 OUT_LENGTH_PKC_Z int *messageLen, 
								 IN_BUFFER( dataLen ) const void *data, 
								 IN_LENGTH_PKC const int dataLen, 
								 IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
								 IN_INT_SHORT_Z const int hashParam )
	{
	BYTE dbMask[ CRYPT_MAX_PKCSIZE + 8 ], seedMask[ CRYPT_MAX_HASHSIZE + 8 ];
	BYTE dataBuffer[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE *seed, *db;
	int seedLen, dbLen, length, i, dummy, status;

	assert( isWritePtr( message, messageMaxLen ) );
	assert( isWritePtr( messageLen, sizeof( int ) ) );
	assert( isReadPtr( data, dataLen ) );

	REQUIRES( messageMaxLen >= MIN_PKCSIZE && \
			  messageMaxLen <= CRYPT_MAX_PKCSIZE );
	REQUIRES( dataLen >= MIN_PKCSIZE && dataLen <= CRYPT_MAX_PKCSIZE );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashParam >= 0 && hashParam < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	memset( message, 0, min( 16, messageMaxLen ) );
	*messageLen = 0;

	/* Make sure that the MGF requirements are met.  Note that this check 
	   has already been performed by the caller to avoid this being used as 
	   a timing oracle, this is merely here to make the fact that the check 
	   has been done explicit */
	status = getOaepHashSize( &seedLen, hashAlgo, hashParam );
	if( cryptStatusError( status ) )
		return( status );

	/* Take a local copy of the input data, since we're about to operate on 
	   it */
	memcpy( dataBuffer, data, dataLen );

	/* Calculate the size and position of the various data quantities */
	seed = dataBuffer + 1;
	db = seed + seedLen;
	dbLen = dataLen - ( 1 + seedLen );

	ENSURES( dbLen >= 16 && 1 + seedLen + dbLen <= dataLen );

	/* seedMask = MGF1( maskedDB, seedLen ) */
	status = mgf1( seedMask, seedLen, db, dbLen, hashAlgo, hashParam );
	ENSURES( cryptStatusOK( status ) );	/* Can only be an internal error */

	/* seed = maskedSeed ^ seedMask */
	for( i = 0; i < seedLen; i++ )
		seed[ i ] ^= seedMask[ i ];

	/* dbMask = MGF1( seed, dbLen ) */
	status = mgf1( dbMask, dbLen, seed, seedLen, hashAlgo, hashParam );
	ENSURES( cryptStatusOK( status ) );	/* Can only be an internal error */

	/* db = maskedDB ^ dbMask */
	for( i = 0; i < dbLen; i++ )
		db[ i ] ^= dbMask[ i ];

	/* Get the (constant) lHash value */
	status = getOaepHash( dbMask, CRYPT_MAX_PKCSIZE, &dummy, hashAlgo, 
						  hashParam );
	ENSURES( cryptStatusOK( status ) );	/* Can only be an internal error */

	/* Verify that:

		data = 0x00 || [seed] || db 
			 = 0x00 || [seed] || lHash || zeroes || 0x01 || message

	   As before to be careful with the order of the checks, for example we 
	   could check for the leading 0x00 before performing the OAEP 
	   processing but this might allow an attacker to mount a timing attack,
	   see "A chosen ciphertext attack on RSA optimal asymmetric encryption 
	   padding (OAEP)" by James Manger, Proceedings of Crypto'01, LNCS 
	   No.2139, p.230.  To make this as hard as possible we cluster all of 
	   the format checks as close together as we can to try and produce a 
	   near-constant-time accept/reject decision (unfortunately the complex
	   processing required by OAEP makes it more or less impossible to 
	   perform in a timing-independent manner, so the best that we can do
	   is make it as hard as possible to get timing data) */
	if( 1 + seedLen + seedLen + 1 + 1 + MIN_KEYSIZE > dataLen )
		{
		/* Make sure that at least a minimum-length payload fits:

			<------------ dataMaxLen ----------->
			+--+------+-------+----+--+---------+
			|00| seed | lhash | PS |01| message |
			+--+------+-------+----+--+---------+
			  1	 hLen	 hLen	 1	 1	 msgLen
		
		   Again, we perform this check after all formatting operations have
		   completed to try and avoid a timing attack */
		return( CRYPT_ERROR_BADDATA );
		}
	if( dataBuffer[ 0 ] != 0x00 || \
		!compareDataConstTime( db, dbMask, seedLen ) )
		return( CRYPT_ERROR_BADDATA );
	for( i = seedLen; i < dbLen && db[ i ] == 0x00; i++ );
	if( i <= seedLen || i >= dbLen || db[ i++ ] != 0x01 )
		return( CRYPT_ERROR_BADDATA );
	length = dbLen - i;
	if( length < MIN_KEYSIZE )
		return( CRYPT_ERROR_UNDERFLOW );
	if( length > messageMaxLen )
		return( CRYPT_ERROR_OVERFLOW );

	/* Return the recovered message to the caller */
	memcpy( message, db + i, length );
	*messageLen = length;

	zeroise( dbMask, CRYPT_MAX_PKCSIZE );
	zeroise( seedMask, CRYPT_MAX_HASHSIZE );
	zeroise( dataBuffer, CRYPT_MAX_PKCSIZE );

	return( CRYPT_OK );
	}

/* Perform OAEP wrapping/unwrapping */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportOAEP( STDC_UNUSED void *dummy, 
				INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	CRYPT_ALGO_TYPE cryptAlgo;
	BYTE payload[ CRYPT_MAX_KEYSIZE + 8 ], seed[ CRYPT_MAX_HASHSIZE + 8 ];
	int seedLen, payloadSize, length, status;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	/* Clear return value */
	if( mechanismInfo->wrappedData != NULL )
		{
		memset( mechanismInfo->wrappedData, 0,
				mechanismInfo->wrappedDataLength );
		}

	/* Make sure that the OAEP auxiliary algorithm requirements are met */
	status = getOaepHashSize( &seedLen, mechanismInfo->auxInfo, 0 );
	if( cryptStatusError( status ) )
		return( status );

	/* Get various algorithm parameters */
	status = getPkcAlgoParams( mechanismInfo->wrapContext, &cryptAlgo, 
							   &length );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( length > MIN_PKCSIZE && length <= CRYPT_MAX_PKCSIZE );

	/* If this is just a length check, we're done */
	if( mechanismInfo->wrappedData == NULL )
		{
		/* Determine how long the encrypted value will be.  In the case of
		   Elgamal it's just an estimate since it can change by up to two
		   bytes depending on whether the values have the high bit set or
		   not, which requires zero-padding of the ASN.1-encoded integers.
		   This is rather nasty because it means that we can't tell how 
		   large an encrypted value will be without actually creating it.  
		   The 10-byte length at the start is for the ASN.1 SEQUENCE (= 4) 
		   and 2 * INTEGER (= 2*3) encoding */
		mechanismInfo->wrappedDataLength = \
							( cryptAlgo == CRYPT_ALGO_ELGAMAL ) ? \
							10 + ( 2 * ( length + 1 ) ) : length;
		
		return( CRYPT_OK );
		}

	/* Get the payload details from the key context and generate the OAEP 
	   random seed value */
	status = krnlSendMessage( mechanismInfo->keyContext, 
							  IMESSAGE_GETATTRIBUTE, &payloadSize,
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusOK( status ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, seed, seedLen );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_RANDOM );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Extract the key data and process it into an OAEP data block */
	status = extractKeyData( mechanismInfo->keyContext, payload, payloadSize,
							 "keydata", 7 );
	if( cryptStatusOK( status ) )
		{
		status = generateOaepDataBlock( mechanismInfo->wrappedData, length, 
										payload, payloadSize, seed, seedLen,
										mechanismInfo->auxInfo, 0 );
		}
	zeroise( payload, bitsToBytes( CRYPT_MAX_KEYSIZE ) );
	zeroise( seed, CRYPT_MAX_HASHSIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Wrap the encoded data using the public key */
	return( pkcWrapData( mechanismInfo, mechanismInfo->wrappedData, length, 
						 FALSE, ( cryptAlgo == CRYPT_ALGO_ELGAMAL ) ? \
								TRUE : FALSE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importOAEP( STDC_UNUSED void *dummy, 
				INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	CRYPT_ALGO_TYPE cryptAlgo;
	MESSAGE_DATA msgData;
	BYTE decryptedData[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE *decryptedDataPtr = decryptedData;
	BYTE message[ CRYPT_MAX_PKCSIZE + 8 ];
	int length, messageLen, unwrapStatus = CRYPT_OK, status;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	/* Get various algorithm parameters */
	status = getPkcAlgoParams( mechanismInfo->wrapContext, &cryptAlgo, 
							   &length );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( length > MIN_PKCSIZE && length <= CRYPT_MAX_PKCSIZE );

	/* Make sure that the MGF requirements are met.  This check isn't 
	   actually needed until the recoverOaepDataBlock() call but we perform 
	   it here before the decrypt to avoid being used as a timing oracle 
	   since feeding in a non-usable hash function that causes the 
	   processing to bail out right after the decrypt provides a reasonably 
	   precise timer for the decryption */
	status = getOaepHashSize( &length, mechanismInfo->auxInfo, 0 );
	if( cryptStatusError( status ) )
		return( status );

	/* Decrypt the data */
	status = pkcUnwrapData( mechanismInfo, decryptedData, CRYPT_MAX_PKCSIZE,
							&length, length, FALSE,
							( cryptAlgo == CRYPT_ALGO_ELGAMAL ) ? TRUE : FALSE );
	if( cryptStatusError( status ) )
		{
		/* See the long comment in pkcs1Unwrap() for the background on what 
		   we're doing here */
		decryptedDataPtr = ( BYTE * ) fixedFormattedValue;
		unwrapStatus = status;
		}

	/* Recover the payload from the OAEP data block */
	status = recoverOaepDataBlock( message, CRYPT_MAX_PKCSIZE, &messageLen, 
								   decryptedDataPtr, length, 
								   mechanismInfo->auxInfo, 0 );
	zeroise( decryptedData, CRYPT_MAX_PKCSIZE );
	if( cryptStatusError( unwrapStatus ) )
		{
		/* If there was an error at the decrypt stage, propagate that rather 
		   than returning the generic CRYPT_ERROR_BADDATA that we'd get from
		   the padding check of the dummy data */
		status = unwrapStatus;
		}
	if( cryptStatusError( status ) )
		{
		zeroise( message, CRYPT_MAX_PKCSIZE );
		return( status );
		}

	/* Load the decrypted keying information into the session key context */
	setMessageData( &msgData, message, messageLen );
	status = krnlSendMessage( mechanismInfo->keyContext, 
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_KEY );
	if( cryptArgError( status ) )
		{
		/* If there was an error with the key value or size, convert the 
		   return value into something more appropriate */
		status = CRYPT_ERROR_BADDATA;
		}
	zeroise( message, CRYPT_MAX_PKCSIZE );

	return( status );
	}

#if 0

void testOAEP( void )
	{
	const BYTE seed[] = { 0xaa, 0xfd, 0x12, 0xf6, 0x59, 0xca, 0xe6, 0x34, 
						  0x89, 0xb4, 0x79, 0xe5, 0x07, 0x6d, 0xde, 0xc2,
						  0xf0, 0x6c, 0xb5, 0x8f };
	const BYTE message[] = { 0xd4, 0x36, 0xe9, 0x95, 0x69, 0xfd, 0x32, 0xa7,
							 0xc8, 0xa0, 0x5b, 0xbc, 0x90, 0xd3, 0x2c, 0x49 };
	BYTE buffer[ 1024 ], outMessage[ 128 ];
	int seedLen, outLen, status;

	status = getOaepHashSize( &seedLen, CRYPT_ALGO_SHA1, 0 );

	memset( buffer, '*', 1024 );

	status = generateOaepDataBlock( buffer, 128, message, 16, seed, seedLen, 
									CRYPT_ALGO_SHA1, 0 );
	status = recoverOaepDataBlock( outMessage, 128, &outLen, buffer, 128, 
								   CRYPT_ALGO_SHA1, 0 );
	if( outLen != 16 || memcmp( message, outMessage, outLen ) )
		puts( "Bang." );
	puts( "Done." );
	}
#endif /* 0 */

#endif /* USE_OAEP */
