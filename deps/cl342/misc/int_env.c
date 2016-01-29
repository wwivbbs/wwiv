/****************************************************************************
*																			*
*					cryptlib Internal Enveloping API						*
*					Copyright Peter Gutmann 1992-2011						*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

#ifdef USE_ENVELOPES

/****************************************************************************
*																			*
*							Data Wrap/Unwrap Functions						*
*																			*
****************************************************************************/

/* General-purpose enveloping functions, used by various high-level
   protocols */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int envelopeWrap( IN_BUFFER( inDataLength ) const void *inData, 
				  IN_DATALENGTH_MIN( 16 ) const int inDataLength, 
				  OUT_BUFFER( outDataMaxLength, \
							  *outDataLength ) void *outData, 
				  IN_DATALENGTH_MIN( 16 ) const int outDataMaxLength, 
				  OUT_DATALENGTH_Z int *outDataLength, 
				  IN_ENUM( CRYPT_FORMAT ) const CRYPT_FORMAT_TYPE formatType,
				  IN_ENUM_OPT( CRYPT_CONTENT ) \
					const CRYPT_CONTENT_TYPE contentType,
				  IN_HANDLE_OPT const CRYPT_HANDLE iPublicKey,
				  OUT ERROR_INFO *errorInfo )
	{
	CRYPT_ENVELOPE iCryptEnvelope;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	const int minBufferSize = max( MIN_BUFFER_SIZE, inDataLength + 512 );
	int status;

	assert( isReadPtr( inData, inDataLength ) );
	assert( isWritePtr( outData, outDataMaxLength ) );
	assert( isWritePtr( outDataLength, sizeof( int ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( inDataLength > 16 && inDataLength < MAX_BUFFER_SIZE );
	REQUIRES( outDataMaxLength > 16 && \
			  outDataMaxLength >= inDataLength + 512 && \
			  outDataMaxLength < MAX_BUFFER_SIZE );
	REQUIRES( formatType == CRYPT_FORMAT_CRYPTLIB || \
			  formatType == CRYPT_FORMAT_CMS );
	REQUIRES( contentType >= CRYPT_CONTENT_NONE && \
			  contentType < CRYPT_CONTENT_LAST );
	REQUIRES( ( iPublicKey == CRYPT_UNUSED ) || \
			  isHandleRangeValid( iPublicKey ) );

	/* Clear return values.  Note that we can't clear the output buffer 
	   at this point since this function is frequently used for in-place 
	   processing, so we clear it after we've pushed the input data */
	*outDataLength = 0;
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Create an envelope to wrap the data, add the encryption key if
	   necessary, and pop the wrapped result */
	setMessageCreateObjectInfo( &createInfo, formatType );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_ENVELOPE );
	if( cryptStatusError( status ) )
		{
		memset( outData, 0, min( 16, outDataMaxLength ) );
		return( status );
		}
	iCryptEnvelope = createInfo.cryptHandle;
	( void ) krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &minBufferSize, 
							  CRYPT_ATTRIBUTE_BUFFERSIZE );
	status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &inDataLength,
							  CRYPT_ENVINFO_DATASIZE );
	if( cryptStatusOK( status ) && contentType != CRYPT_CONTENT_NONE )
		{
		const int value = contentType;	/* int vs.enum */
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &value,
								  CRYPT_ENVINFO_CONTENTTYPE );
		}
	if( cryptStatusOK( status ) && iPublicKey != CRYPT_UNUSED )
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &iPublicKey,
								  CRYPT_ENVINFO_PUBLICKEY );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) inData, inDataLength );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_PUSHDATA,
								  &msgData, 0 );
		if( cryptStatusOK( status ) )
			{
			ENSURES( msgData.length >= inDataLength );
			}
		}
	memset( outData, 0, min( 16, outDataMaxLength ) );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_PUSHDATA,
								  &msgData, 0 );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, outData, outDataMaxLength );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_POPDATA,
								  &msgData, 0 );
		if( cryptStatusOK( status ) )
			{
			ENSURES( msgData.length > inDataLength && \
					 msgData.length < outDataMaxLength );
			}
		if( cryptStatusOK( status ) )
			*outDataLength = msgData.length;
		}
	if( cryptStatusError( status ) )
		{
		/* Fetch any addition error information that may be available */
		readErrorInfo( errorInfo, iCryptEnvelope );
		}
	krnlSendNotifier( iCryptEnvelope, IMESSAGE_DECREFCOUNT );

	ENSURES( cryptStatusError( status ) || \
			 !cryptStatusError( checkObjectEncoding( outData, \
													 *outDataLength ) ) );
	assert( !cryptArgError( status ) );
	return( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int envelopeUnwrap( IN_BUFFER( inDataLength ) const void *inData, 
					IN_DATALENGTH_MIN( 16 ) const int inDataLength,
					OUT_BUFFER( outDataMaxLength, \
								*outDataLength ) void *outData, 
					IN_DATALENGTH_MIN( 16 ) const int outDataMaxLength,
					OUT_DATALENGTH_Z int *outDataLength, 
					IN_HANDLE_OPT const CRYPT_CONTEXT iPrivKey,
					OUT ERROR_INFO *errorInfo )
	{
	CRYPT_ENVELOPE iCryptEnvelope;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	const int minBufferSize = max( MIN_BUFFER_SIZE, inDataLength );
	int status;

	assert( isReadPtr( inData, inDataLength ) );
	assert( isWritePtr( outData, outDataMaxLength ) );
	assert( isWritePtr( outDataLength, sizeof( int ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( inDataLength > 16 && inDataLength < MAX_BUFFER_SIZE );
	REQUIRES( outDataMaxLength > 16 && \
			  outDataMaxLength >= inDataLength && \
			  outDataMaxLength < MAX_BUFFER_SIZE );
	REQUIRES( ( iPrivKey == CRYPT_UNUSED ) || \
			  isHandleRangeValid( iPrivKey ) );

	/* Clear return values.  Note that we can't clear the output buffer 
	   at this point since this function is frequently used for in-place 
	   processing, so we clear it after we've pushed the input data */
	*outDataLength = 0;
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Create an envelope to unwrap the data, add the decryption key if
	   necessary, and pop the unwrapped result.  In theory we could use 
	   checkASN1() here to perform a safety check of the envelope data
	   prior to processing but this has already been done by the calling
	   code when the datagram containing the enveloped data was read so
	   we don't need to repeat the (rather heavyweight) operation here */
	setMessageCreateObjectInfo( &createInfo, CRYPT_FORMAT_AUTO );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_ENVELOPE );
	if( cryptStatusError( status ) )
		{
		memset( outData, 0, min( 16, outDataMaxLength ) );
		return( status );
		}
	iCryptEnvelope = createInfo.cryptHandle;
	( void ) krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &minBufferSize, 
							  CRYPT_ATTRIBUTE_BUFFERSIZE );
	setMessageData( &msgData, ( MESSAGE_CAST ) inData, inDataLength );
	status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_PUSHDATA,
							  &msgData, 0 );
	if( cryptStatusOK( status ) )
		{
		ENSURES( msgData.length >= inDataLength );
		}
	memset( outData, 0, min( 16, outDataMaxLength ) );
	if( status == CRYPT_ENVELOPE_RESOURCE )
		{
		/* If the caller wasn't expecting encrypted data, let them know */
		if( iPrivKey == CRYPT_UNUSED )
			status = CRYPT_ERROR_WRONGKEY;
		else
			{
			status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &iPrivKey,
									  CRYPT_ENVINFO_PRIVATEKEY );
			}
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_PUSHDATA,
								  &msgData, 0 );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, outData, outDataMaxLength );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_POPDATA,
								  &msgData, 0 );
		if( cryptStatusOK( status ) )
			{
			ENSURES( msgData.length < inDataLength && \
					 msgData.length < outDataMaxLength );
			}
		}
	if( cryptStatusError( status ) )
		{
		/* Fetch any addition error information that may be available */
		readErrorInfo( errorInfo, iCryptEnvelope );
		}
	krnlSendNotifier( iCryptEnvelope, IMESSAGE_DECREFCOUNT );
	if( cryptStatusOK( status ) )
		*outDataLength = msgData.length;

	assert( !cryptArgError( status ) );
	return( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status );
	}

/****************************************************************************
*																			*
*							Data Sign/Verify Functions						*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5 ) ) \
int envelopeSign( IN_BUFFER_OPT( inDataLength ) const void *inData, 
				  IN_DATALENGTH_Z const int inDataLength,
				  OUT_BUFFER( outDataMaxLength, \
							  *outDataLength ) void *outData, 
				  IN_DATALENGTH_MIN( 16 ) const int outDataMaxLength,
				  OUT_DATALENGTH_Z int *outDataLength, 
				  IN_ENUM_OPT( CRYPT_CONTENT ) \
					const CRYPT_CONTENT_TYPE contentType,
				  IN_HANDLE const CRYPT_CONTEXT iSigKey,
				  IN_HANDLE_OPT const CRYPT_CERTIFICATE iCmsAttributes,
				  OUT ERROR_INFO *errorInfo )
	{
	CRYPT_ENVELOPE iCryptEnvelope;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	const int minBufferSize = max( MIN_BUFFER_SIZE, inDataLength + 1024 );
	int status;

	assert( ( inData == NULL && inDataLength == 0 ) || \
			isReadPtr( inData, inDataLength ) );
	assert( isWritePtr( outData, outDataMaxLength ) );
	assert( isWritePtr( outDataLength, sizeof( int ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( ( inData != NULL && \
				inDataLength > 16 && inDataLength < MAX_BUFFER_SIZE ) || \
			  ( inData == NULL && inDataLength == 0 && \
				contentType == CRYPT_CONTENT_NONE && \
				isHandleRangeValid( iCmsAttributes ) ) );
	REQUIRES( outDataMaxLength > 16 && \
			  outDataMaxLength >= inDataLength + 512 && \
			  outDataMaxLength < MAX_BUFFER_SIZE );
	REQUIRES( contentType >= CRYPT_CONTENT_NONE && \
			  contentType < CRYPT_CONTENT_LAST );
	REQUIRES( isHandleRangeValid( iSigKey ) );
	REQUIRES( iCmsAttributes == CRYPT_UNUSED || \
			  isHandleRangeValid( iCmsAttributes ) );

	/* Clear return values.  Note that we can't clear the output buffer 
	   at this point since this function is frequently used for in-place 
	   processing, so we clear it after we've pushed the input data */
	*outDataLength = 0;
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Create an envelope to sign the data, add the signature key and
	   optional signing attributes, and pop the signed result */
	setMessageCreateObjectInfo( &createInfo, CRYPT_FORMAT_CMS );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_ENVELOPE );
	if( cryptStatusError( status ) )
		{
		memset( outData, 0, min( 16, outDataMaxLength ) );
		return( status );
		}
	iCryptEnvelope = createInfo.cryptHandle;
	( void ) krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &minBufferSize, 
							  CRYPT_ATTRIBUTE_BUFFERSIZE );
	status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &inDataLength,
							  CRYPT_ENVINFO_DATASIZE );
	if( cryptStatusOK( status ) && contentType != CRYPT_CONTENT_NONE )
		{
		const int value = contentType;	/* int vs.enum */
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &value,
								  CRYPT_ENVINFO_CONTENTTYPE );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &iSigKey,
								  CRYPT_ENVINFO_SIGNATURE );
		}
	if( cryptStatusOK( status ) && iCmsAttributes != CRYPT_UNUSED )
		{
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &iCmsAttributes,
								  CRYPT_ENVINFO_SIGNATURE_EXTRADATA );
		}
	if( cryptStatusOK( status ) )
		{
		/* If there's no data supplied it's an attributes-only message
		   containing only authenticated attributes */
		if( inDataLength <= 0 )
			{
			status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
									  MESSAGE_VALUE_TRUE,
									  CRYPT_IATTRIBUTE_ATTRONLY );
			}
		else
			{
			setMessageData( &msgData, ( MESSAGE_CAST ) inData, inDataLength );
			status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_PUSHDATA,
									  &msgData, 0 );
			if( cryptStatusOK( status ) )
				{
				ENSURES( msgData.length >= inDataLength );
				}
			}
		}
	memset( outData, 0, min( 16, outDataMaxLength ) );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_PUSHDATA,
								  &msgData, 0 );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, outData, outDataMaxLength );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_POPDATA,
								  &msgData, 0 );
		if( cryptStatusOK( status ) )
			{
			ENSURES( msgData.length > inDataLength && \
					 msgData.length < outDataMaxLength );
			}
		if( cryptStatusOK( status ) )
			*outDataLength = msgData.length;
		}
	if( cryptStatusError( status ) )
		{
		/* Fetch any addition error information that may be available */
		readErrorInfo( errorInfo, iCryptEnvelope );
		}
	krnlSendNotifier( iCryptEnvelope, IMESSAGE_DECREFCOUNT );

	ENSURES( cryptStatusError( status ) || \
			 !cryptStatusError( checkObjectEncoding( outData, \
													 *outDataLength ) ) );
	assert( !cryptArgError( status ) );
	return( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5, 7 ) ) \
int envelopeSigCheck( IN_BUFFER( inDataLength ) const void *inData, 
					  IN_DATALENGTH_MIN( 16 ) const int inDataLength,
					  OUT_BUFFER( outDataMaxLength, \
								  *outDataLength ) void *outData, 
					  IN_DATALENGTH_MIN( 16 ) const int outDataMaxLength,
					  OUT_DATALENGTH_Z int *outDataLength, 
					  IN_HANDLE_OPT const CRYPT_CONTEXT iSigCheckKey,
					  OUT_STATUS int *sigResult, 
					  OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iSigningCert,
					  OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iCmsAttributes,
					  OUT ERROR_INFO *errorInfo )
	{
	CRYPT_ENVELOPE iCryptEnvelope;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData DUMMY_INIT_STRUCT;
	const int minBufferSize = max( MIN_BUFFER_SIZE, inDataLength );
	int status;

	assert( isReadPtr( inData, inDataLength ) );
	assert( isWritePtr( outData, outDataMaxLength ) );
	assert( isWritePtr( outDataLength, sizeof( int ) ) );
	assert( isWritePtr( sigResult, sizeof( int ) ) );
	assert( iSigningCert == NULL || \
			isWritePtr( iSigningCert, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( iCmsAttributes == NULL || \
			isWritePtr( iCmsAttributes, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( inDataLength > 16 && inDataLength < MAX_BUFFER_SIZE );
	REQUIRES( outDataMaxLength > 16 && \
			  outDataMaxLength >= inDataLength && \
			  outDataMaxLength < MAX_BUFFER_SIZE );
	REQUIRES( iSigCheckKey == CRYPT_UNUSED || \
			  isHandleRangeValid( iSigCheckKey ) );

	/* Clear return values.  Note that we can't clear the output buffer 
	   at this point since this function is frequently used for in-place 
	   processing, so we clear it after we've pushed the input data */
	*outDataLength = 0;
	*sigResult = CRYPT_ERROR;
	if( iSigningCert != NULL )
		*iSigningCert = CRYPT_ERROR;
	if( iCmsAttributes != NULL )
		*iCmsAttributes = CRYPT_ERROR;
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Create an envelope to sig.check the data, push in the signed data and
	   sig.check key, and pop the result.  We also speculatively set the
	   attributes-only flag to let the enveloping code know that a signed
	   message with no content is a zero-data-length message rather than a
	   detached signature, which is what this type of message would normally
	   be.  In theory we could use checkASN1() here to perform a safety 
	   check of the envelope data prior to processing, but this has already 
	   been done by the calling code when the datagram containing the 
	   enveloped data was read so we don't need to repeat the (rather 
	   heavyweight) operation here */
	setMessageCreateObjectInfo( &createInfo, CRYPT_FORMAT_AUTO );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_ENVELOPE );
	if( cryptStatusError( status ) )
		{
		memset( outData, 0, min( 16, outDataMaxLength ) );
		return( status );
		}
	iCryptEnvelope = createInfo.cryptHandle;
	( void ) krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &minBufferSize, 
							  CRYPT_ATTRIBUTE_BUFFERSIZE );
	status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, 
							  CRYPT_IATTRIBUTE_ATTRONLY );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) inData, inDataLength );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_PUSHDATA,
								  &msgData, 0 );
		}
	if( cryptStatusOK( status ) )
		{
		ENSURES( msgData.length >= inDataLength );
		}
	memset( outData, 0, min( 16, outDataMaxLength ) );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_PUSHDATA,
								  &msgData, 0 );
		}
	if( cryptStatusOK( status ) && iSigCheckKey != CRYPT_UNUSED )
		{
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &iSigCheckKey,
								  CRYPT_ENVINFO_SIGNATURE );
		}
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_GETATTRIBUTE,
								  sigResult, CRYPT_ENVINFO_SIGNATURE_RESULT );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, outData, outDataMaxLength );
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_ENV_POPDATA,
								  &msgData, 0 );
		if( cryptStatusOK( status ) )
			{
			ENSURES( msgData.length < inDataLength && \
					 msgData.length < outDataMaxLength );
			}
		}
	if( cryptStatusOK( status ) && iSigningCert != NULL )
		{
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_GETATTRIBUTE,
								  iSigningCert,
								  CRYPT_ENVINFO_SIGNATURE );
		}
	if( cryptStatusOK( status ) && iCmsAttributes != NULL )
		{
		status = krnlSendMessage( iCryptEnvelope, IMESSAGE_GETATTRIBUTE,
								  iCmsAttributes,
								  CRYPT_ENVINFO_SIGNATURE_EXTRADATA );
		if( cryptStatusError( status ) && iSigningCert != NULL )
			{
			krnlSendNotifier( *iSigningCert, IMESSAGE_DECREFCOUNT );
			*iSigningCert = CRYPT_ERROR;
			}
		}
	if( cryptStatusError( status ) )
		{
		/* Fetch any addition error information that may be available */
		readErrorInfo( errorInfo, iCryptEnvelope );
		}
	krnlSendNotifier( iCryptEnvelope, IMESSAGE_DECREFCOUNT );
	if( cryptStatusOK( status ) )
		*outDataLength = msgData.length;

	assert( !cryptArgError( status ) );
	return( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status );
	}
#endif /* USE_ENVELOPES */
