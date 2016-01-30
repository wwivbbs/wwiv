/****************************************************************************
*																			*
*					cryptlib SSL v3/TLS Crypto Routines						*
*					 Copyright Peter Gutmann 1998-2010						*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "session.h"
  #include "ssl.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "session/session.h"
  #include "session/ssl.h"
#endif /* Compiler-specific includes */

/* Proto-HMAC padding data */

#define PROTOHMAC_PAD1_VALUE	0x36
#define PROTOHMAC_PAD2_VALUE	0x5C
#define PROTOHMAC_PAD1			"\x36\x36\x36\x36\x36\x36\x36\x36" \
								"\x36\x36\x36\x36\x36\x36\x36\x36" \
								"\x36\x36\x36\x36\x36\x36\x36\x36" \
								"\x36\x36\x36\x36\x36\x36\x36\x36" \
								"\x36\x36\x36\x36\x36\x36\x36\x36" \
								"\x36\x36\x36\x36\x36\x36\x36\x36"
#define PROTOHMAC_PAD2			"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C" \
								"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C" \
								"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C" \
								"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C" \
								"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C" \
								"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"

#ifdef USE_SSL

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Write packet metadata for input to the MAC/ICV authentication process:

	seq_num || type || version || length */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int writePacketMetadata( OUT_BUFFER( dataMaxLength, *dataLength ) \
									void *data,
								IN_LENGTH_SHORT_MIN( 16 ) const int dataMaxLength,
								OUT_LENGTH_SHORT_Z int *dataLength,
								IN_RANGE( 0, 255 ) const int type,
								IN_INT_Z const long seqNo, 
								IN_RANGE( SSL_MINOR_VERSION_TLS, \
										  SSL_MINOR_VERSION_TLS12 ) const int version,
								IN_LENGTH_Z const int payloadLength )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( dataMaxLength >= 16 && dataMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( type >= 0 && type <= 255 );
	REQUIRES( seqNo >= 0 );
	REQUIRES( version >= SSL_MINOR_VERSION_TLS && \
			  version <= SSL_MINOR_VERSION_TLS12 );
	REQUIRES( payloadLength >= 0 && payloadLength <= MAX_PACKET_SIZE );

	/* Clear return values */
	memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	/* Write the sequence number, packet type, version, and length 
	  information to the output buffer */
	sMemOpen( &stream, data, dataMaxLength );
	writeUint64( &stream, seqNo );
	sputc( &stream, type );
	sputc( &stream, SSL_MAJOR_VERSION );
	sputc( &stream, version );
	status = writeUint16( &stream, payloadLength );
	if( cryptStatusOK( status ) )
		*dataLength = stell( &stream );
	sMemDisconnect( &stream );
	return( status );
	}

/****************************************************************************
*																			*
*							Encrypt/Decrypt Functions						*
*																			*
****************************************************************************/

/* Encrypt/decrypt a data block (in mose cases this also includes the MAC, 
   which has been added to the data by the caller).  The handling of length 
   arguments for these is a bit tricky, for encryption the input is { data, 
   payloadLength } which is padded (if necessary) and the padded length 
   returned in '*dataLength', for decryption the entire data block will be 
   processed but only 'processedDataLength' bytes of result are valid 
   output */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int encryptData( const SESSION_INFO *sessionInfoPtr, 
				 INOUT_BUFFER( dataMaxLength, *dataLength ) \
					BYTE *data, 
				 IN_LENGTH const int dataMaxLength,
				 OUT_LENGTH_Z int *dataLength,
				 IN_LENGTH const int payloadLength )
	{
	int length = payloadLength, status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( dataMaxLength > 0 && dataMaxLength < MAX_INTLENGTH );
	REQUIRES( payloadLength > 0 && \
			  payloadLength <= MAX_PACKET_SIZE + 20 && \
			  payloadLength <= sessionInfoPtr->sendBufSize && \
			  payloadLength <= dataMaxLength );

	/* Clear return value */
	*dataLength = 0;

	/* If it's a block cipher then we need to add end-of-block padding */
	if( sessionInfoPtr->cryptBlocksize > 1 )
		{
		const int padSize = ( sessionInfoPtr->cryptBlocksize - 1 ) - \
						    ( payloadLength & ( sessionInfoPtr->cryptBlocksize - 1 ) );
		int i;

		ENSURES( padSize >= 0 && padSize <= CRYPT_MAX_IVSIZE && \
				 length + padSize + 1 <= dataMaxLength );

		/* Add the PKCS #5-style padding (PKCS #5 uses n, TLS uses n-1) */
		for( i = 0; i < padSize + 1; i++ )
			data[ length++ ] = intToByte( padSize );
		}

	/* Encrypt the data and optional padding */
	status = krnlSendMessage( sessionInfoPtr->iCryptOutContext,
							  IMESSAGE_CTX_ENCRYPT, data, length );
	if( cryptStatusError( status ) )
		return( status );
	*dataLength = length;

	/* If we're using GCM then we have to append the ICV to the data */
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM )
		{
		MESSAGE_DATA msgData;

		REQUIRES( length + sessionInfoPtr->authBlocksize <= dataMaxLength );

		setMessageData( &msgData, data + length, 
						sessionInfoPtr->authBlocksize );
		status = krnlSendMessage( sessionInfoPtr->iCryptOutContext,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_ICV );
		if( cryptStatusError( status ) )
			return( status );
		*dataLength += sessionInfoPtr->authBlocksize;
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int decryptData( SESSION_INFO *sessionInfoPtr, 
				 INOUT_BUFFER_FIXED( dataLength ) \
					BYTE *data, 
				 IN_LENGTH const int dataLength, 
				 OUT_LENGTH_Z int *processedDataLength )
	{
	int length = dataLength, padSize, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( data, dataLength ) );
	assert( isWritePtr( processedDataLength, sizeof( int ) ) );

	REQUIRES( dataLength > 0 && \
			  dataLength <= sessionInfoPtr->receiveBufEnd && \
			  dataLength < MAX_INTLENGTH );

	/* Clear return value */
	*processedDataLength = 0;

	/* Decrypt the data */
	status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
							  IMESSAGE_CTX_DECRYPT, data, length );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Packet decryption failed" ) );
		}

	/* If we're using GCM then we have to check the ICV that follows the 
	   data */
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, data + length, 
						sessionInfoPtr->authBlocksize );
		status = krnlSendMessage( sessionInfoPtr->iCryptInContext, 
								  IMESSAGE_COMPARE, &msgData, 
								  MESSAGE_COMPARE_ICV );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_SIGNATURE,
					( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
					  "Bad message ICV for packet type %d, length %d",
					  data[ 0 ], length ) );
			}
		}

	/* If it's a stream cipher then there's no padding present */
	if( sessionInfoPtr->cryptBlocksize <= 1 )
		{
		*processedDataLength = length;

		return( CRYPT_OK );
		}

	/* If it's a block cipher, we need to remove end-of-block padding.  Up 
	   until TLS 1.1 the spec was silent about any requirement to check the 
	   padding (and for SSLv3 it didn't specify the padding format at all) 
	   so it's not really safe to reject an SSL message if we don't find the 
	   correct padding because many SSL implementations didn't process the 
	   padded data space in any way, leaving it containing whatever was 
	   there before (which can include old plaintext (!!)).  Almost all TLS 
	   implementations get it right (even though in TLS 1.0 there was only a 
	   requirement to generate, but not to check, the PKCS #5-style 
	   padding).  Because of this we only check the padding bytes if we're 
	   talking TLS.

	   First we make sure that the padding information looks OK.  TLS allows 
	   up to 256 bytes of padding (only GnuTLS actually seems to use this 
	   capability though) so we can't check for a sensible (small) padding 
	   length, however we can check this for SSL, which is good because for 
	   that we can't check the padding itself */
	padSize = byteToInt( data[ dataLength - 1 ] );
	if( padSize < 0 || padSize > 255 || \
		( sessionInfoPtr->version == SSL_MINOR_VERSION_SSL && \
		  padSize > sessionInfoPtr->cryptBlocksize - 1 ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid encryption padding value 0x%02X (%d)", 
				  padSize, padSize ) );
		}
	length -= padSize + 1;
	if( length < 0 || length > MAX_INTLENGTH )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Encryption padding adjustment value %d is greater "
				  "than packet length %d", padSize, dataLength ) );
		}

	/* Check for PKCS #5-type padding (PKCS #5 uses n, TLS uses n-1) if 
	   necessary */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS )
		{
		int i;

		for( i = 0; i < padSize; i++ )
			{
			if( data[ length + i ] != padSize )
				{
				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
						  "Invalid encryption padding byte 0x%02X at "
						  "position %d, should be 0x%02X",
						  data[ length + i ], length + i, padSize ) );
				}
			}
		}
	*processedDataLength = length;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								SSL MAC Functions							*
*																			*
****************************************************************************/

/* Perform an SSL MAC of a data block.  We have to provide special-case 
   handling of zero-length blocks since some versions of OpenSSL send these 
   as a kludge in SSL/TLS 1.0 to work around chosen-IV attacks.

   In the following functions we don't check the return value of every 
   single component MAC operation since it would lead to endless sequences
   of 'status = x; if( cSOK( x ) ) ...' chains, on the remote chance that
   there's some transient failure in a single component operation it'll be
   picked up at the end anyway when the overall MAC check fails */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int macDataSSL( IN_HANDLE const CRYPT_CONTEXT iHashContext, 
					   IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
					   IN_BUFFER( macSecretLength ) \
							const void *macSecret, 
					   IN_LENGTH_SHORT const int macSecretLength,
					   IN_INT_Z const long seqNo, 
					   IN_BUFFER_OPT( dataLength ) const void *data, 
					   IN_LENGTH_Z const int dataLength, 
					   IN_RANGE( 0, 255 ) const int type )
	{
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE buffer[ 128 + 8 ];
	const int padSize = ( hashAlgo == CRYPT_ALGO_MD5 ) ? 48 : 40;
	int length = DUMMY_INIT, status;

	assert( isReadPtr( macSecret, macSecretLength ) );
	assert( ( data == NULL && dataLength == 0 ) || \
			isReadPtr( data, dataLength ) );

	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( hashAlgo == CRYPT_ALGO_MD5 || \
			  hashAlgo == CRYPT_ALGO_SHA1 );
	REQUIRES( macSecretLength > 0 && \
			  macSecretLength < MAX_INTLENGTH_SHORT );
	REQUIRES( seqNo >= 0 );
	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( data != NULL && \
				dataLength > 0 && dataLength <= MAX_PACKET_SIZE ) );
	REQUIRES( type >= 0 && type <= 255 );

	/* Set up the sequence number and length data */
	memset( buffer, PROTOHMAC_PAD1_VALUE, padSize );
	sMemOpen( &stream, buffer + padSize, 128 - padSize );
	writeUint64( &stream, seqNo );
	sputc( &stream, type );
	status = writeUint16( &stream, dataLength );
	if( cryptStatusOK( status ) )
		length = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Reset the hash context and generate the inner portion of the MAC:

		hash( MAC_secret || pad1 || seq_num || type || length || data ) */
	krnlSendMessage( iHashContext, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_HASHVALUE );
	krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
					 ( MESSAGE_CAST ) macSecret, macSecretLength );
	krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, buffer,
					 padSize + length );
	if( dataLength > 0 )
		krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
						 ( MESSAGE_CAST ) data, dataLength );
	status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusError( status ) )
		return( status );

	/* Extract the inner hash value */
	memset( buffer, PROTOHMAC_PAD2_VALUE, padSize );
	setMessageData( &msgData, buffer + padSize, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Generate the outer portion of the handshake message's MAC:

		hash( MAC_secret || pad2 || inner_hash ) */
	krnlSendMessage( iHashContext, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_HASHVALUE );
	krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
					 ( MESSAGE_CAST ) macSecret, macSecretLength );
	krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, buffer,
					 padSize + msgData.length );
	return( krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, "", 0 ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int createMacSSL( INOUT SESSION_INFO *sessionInfoPtr, 
				  INOUT_BUFFER( dataMaxLength, *dataLength ) void *data, 
				  IN_LENGTH const int dataMaxLength, 
				  OUT_LENGTH_Z int *dataLength,
				  IN_LENGTH const int payloadLength, 
				  IN_RANGE( 0, 255 ) const int type )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( dataMaxLength > 0 && dataMaxLength < MAX_INTLENGTH );
	REQUIRES( payloadLength > 0 && payloadLength <= MAX_PACKET_SIZE && \
			  payloadLength + sessionInfoPtr->authBlocksize <= dataMaxLength );
	REQUIRES( type >= 0 && type <= 255 );

	/* Clear return value */
	*dataLength = 0;

	/* MAC the payload */
	status = macDataSSL( sessionInfoPtr->iAuthOutContext, 
						 sessionInfoPtr->integrityAlgo,
						 sslInfo->macWriteSecret, 
						 sessionInfoPtr->authBlocksize, sslInfo->writeSeqNo,
						 data, payloadLength, type );
	if( cryptStatusError( status ) )
		return( status );
	sslInfo->writeSeqNo++;

	/* Append the MAC value to the end of the packet */
	ENSURES( rangeCheck( payloadLength, sessionInfoPtr->authBlocksize,
						 dataMaxLength ) );
	setMessageData( &msgData, ( BYTE * ) data + payloadLength,
					sessionInfoPtr->authBlocksize );
	status = krnlSendMessage( sessionInfoPtr->iAuthOutContext, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData, 
							  CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );
	*dataLength = payloadLength + msgData.length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int checkMacSSL( INOUT SESSION_INFO *sessionInfoPtr, 
				 IN_BUFFER( dataLength ) const void *data, 
				 IN_LENGTH const int dataLength, 
				 IN_LENGTH_Z const int payloadLength, 
				 IN_RANGE( 0, 255 ) const int type, 
				 const BOOLEAN noReportError )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );
	REQUIRES( payloadLength >= 0 && payloadLength <= MAX_PACKET_SIZE && \
			  payloadLength + sessionInfoPtr->authBlocksize <= dataLength );
	REQUIRES( type >= 0 && type <= 255 );

	/* MAC the payload.  If the payload length is zero then there's no data 
	   payload, this can happen with some versions of OpenSSL that send 
	   zero-length blocks as a kludge to work around pre-TLS 1.1 chosen-IV
	   attacks */
	if( payloadLength <= 0 )
		{
		status = macDataSSL( sessionInfoPtr->iAuthInContext, 
							 sessionInfoPtr->integrityAlgo,
							 sslInfo->macReadSecret, 
							 sessionInfoPtr->authBlocksize, 
							 sslInfo->readSeqNo, NULL, 0, type );
		}
	else
		{
		status = macDataSSL( sessionInfoPtr->iAuthInContext, 
							 sessionInfoPtr->integrityAlgo,
							 sslInfo->macReadSecret, 
							 sessionInfoPtr->authBlocksize, 
							 sslInfo->readSeqNo, data, payloadLength, type );
		}
	if( cryptStatusError( status ) )
		return( status );
	sslInfo->readSeqNo++;

	/* Compare the calculated MAC to the MAC present at the end of the 
	   data */
	ENSURES( rangeCheckZ( payloadLength, sessionInfoPtr->authBlocksize,
						  dataLength ) );
	setMessageData( &msgData, ( BYTE * ) data + payloadLength,
					sessionInfoPtr->authBlocksize );
	status = krnlSendMessage( sessionInfoPtr->iAuthInContext, 
							  IMESSAGE_COMPARE, &msgData, 
							  MESSAGE_COMPARE_HASH );
	if( cryptStatusError( status ) )
		{
		/* If the error message has already been set at a higher level, 
		   don't update the error information */
		if( noReportError )
			return( CRYPT_ERROR_SIGNATURE );

		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
				  "Bad message MAC for packet type %d, length %d",
				  type, dataLength ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								TLS MAC Functions							*
*																			*
****************************************************************************/

/* Perform a TLS MAC of a data block.  We have to provide special-case 
   handling of zero-length blocks since some versions of OpenSSL send these 
   as a kludge in SSL/TLS 1.0 to work around chosen-IV attacks.

   In the following functions we don't check the return value of every 
   single component MAC operation since it would lead to endless sequences
   of 'status = x; if( cSOK( x ) ) ...' chains, on the remote chance that
   there's some transient failure in a single component operation it'll be
   picked up at the end anyway when the overall MAC check fails */

CHECK_RETVAL \
static int macDataTLS( IN_HANDLE const CRYPT_CONTEXT iHashContext, 
					   IN_INT_Z const long seqNo, 
					   IN_RANGE( SSL_MINOR_VERSION_TLS, \
								 SSL_MINOR_VERSION_TLS12 ) const int version,
					   IN_BUFFER_OPT( dataLength ) const void *data, 
					   IN_LENGTH_Z const int dataLength, 
					   IN_RANGE( 0, 255 ) const int type )
	{
	BYTE buffer[ 64 + 8 ];
	int length, status;

	assert( ( data == NULL && dataLength == 0 ) || \
			isReadPtr( data, dataLength ) );

	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( seqNo >= 0 );
	REQUIRES( version >= SSL_MINOR_VERSION_TLS && \
			  version <= SSL_MINOR_VERSION_TLS12 );
	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( data != NULL && \
				dataLength > 0 && dataLength <= MAX_PACKET_SIZE ) );
	REQUIRES( type >= 0 && type <= 255 );

	/* Set up the packet metadata to be MACed */
	status = writePacketMetadata( buffer, 64, &length, type, seqNo, version,
								  dataLength );
	if( cryptStatusError( status ) )
		return( status );

	/* Reset the hash context and generate the MAC:

		HMAC( metadata || data ) */
	krnlSendMessage( iHashContext, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_HASHVALUE );
	krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, buffer, length );
	if( dataLength > 0 )
		{
		krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
						 ( MESSAGE_CAST ) data, dataLength );
		}
	return( krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, "", 0 ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int createMacTLS( INOUT SESSION_INFO *sessionInfoPtr, 
				  OUT_BUFFER( dataMaxLength, *dataLength ) void *data, 
				  IN_LENGTH const int dataMaxLength, 
				  OUT_LENGTH_Z int *dataLength,
				  IN_LENGTH const int payloadLength, 
				  IN_RANGE( 0, 255 ) const int type )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( dataMaxLength > 0 && dataMaxLength < MAX_INTLENGTH );
	REQUIRES( payloadLength > 0 && payloadLength <= MAX_PACKET_SIZE && \
			  payloadLength + sessionInfoPtr->authBlocksize <= dataMaxLength );
	REQUIRES( type >= 0 && type <= 255 );

	/* Clear return value */
	*dataLength = 0;

	/* MAC the payload */
	status = macDataTLS( sessionInfoPtr->iAuthOutContext, sslInfo->writeSeqNo,
						 sessionInfoPtr->version, data, payloadLength, type );
	if( cryptStatusError( status ) )
		return( status );
	sslInfo->writeSeqNo++;

	/* Append the MAC value to the end of the packet */
	ENSURES( rangeCheck( payloadLength, sessionInfoPtr->authBlocksize,
						 dataMaxLength ) );
	setMessageData( &msgData, ( BYTE * ) data + payloadLength,
					sessionInfoPtr->authBlocksize );
	status = krnlSendMessage( sessionInfoPtr->iAuthOutContext, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData, 
							  CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );
	*dataLength = payloadLength + msgData.length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int checkMacTLS( INOUT SESSION_INFO *sessionInfoPtr, 
				 IN_BUFFER( dataLength ) const void *data, 
				 IN_LENGTH const int dataLength, 
				 IN_LENGTH_Z const int payloadLength, 
				 IN_RANGE( 0, 255 ) const int type, 
				 const BOOLEAN noReportError )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );
	REQUIRES( payloadLength >= 0 && payloadLength <= MAX_PACKET_SIZE && \
			  payloadLength + sessionInfoPtr->authBlocksize <= dataLength );
	REQUIRES( type >= 0 && type <= 255 );

	/* MAC the payload.  If the payload length is zero then there's no data 
	   payload, this can happen with some versions of OpenSSL that send 
	   zero-length blocks as a kludge to work around pre-TLS 1.1 chosen-IV
	   attacks */
	if( payloadLength <= 0 )
		{
		status = macDataTLS( sessionInfoPtr->iAuthInContext, 
							 sslInfo->readSeqNo, sessionInfoPtr->version, 
							 NULL, 0, type );
		}
	else
		{
		status = macDataTLS( sessionInfoPtr->iAuthInContext, 
							 sslInfo->readSeqNo, sessionInfoPtr->version, 
							 data, payloadLength, type );
		}
	if( cryptStatusError( status ) )
		return( status );
	sslInfo->readSeqNo++;

	/* Compare the calculated MAC to the MAC present at the end of the 
	   data */
	ENSURES( rangeCheckZ( payloadLength, sessionInfoPtr->authBlocksize,
						  dataLength ) );
	setMessageData( &msgData, ( BYTE * ) data + payloadLength,
					sessionInfoPtr->authBlocksize );
	status = krnlSendMessage( sessionInfoPtr->iAuthInContext, 
							  IMESSAGE_COMPARE, &msgData, 
							  MESSAGE_COMPARE_HASH );
	if( cryptStatusError( status ) )
		{
		/* If the error message has already been set at a higher level, 
		   don't update the error information */
		if( noReportError )
			return( CRYPT_ERROR_SIGNATURE );

		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
				  "Bad message MAC for packet type %d, length %d",
				  type, dataLength ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								TLS GCM Functions							*
*																			*
****************************************************************************/

/* Perform a TLS GCM integrity check of a data block.  This differs somewhat
   from the more conventional MACing routines because GCM combines the ICV
   generation with encryption, so all that we're actually doing is 
   generating the initial stage of the ICV over the packet metadata handled
   as GCM AAD */

CHECK_RETVAL \
int macDataTLSGCM( IN_HANDLE const CRYPT_CONTEXT iCryptContext, 
				   IN_INT_Z const long seqNo, 
				   IN_RANGE( SSL_MINOR_VERSION_TLS, \
							 SSL_MINOR_VERSION_TLS12 ) const int version,
				   IN_LENGTH_Z const int payloadLength, 
				   IN_RANGE( 0, 255 ) const int type )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ 64 + 8 ];
	int length, status;

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( seqNo >= 0 );
	REQUIRES( version >= SSL_MINOR_VERSION_TLS && \
			  version <= SSL_MINOR_VERSION_TLS12 );
	REQUIRES( payloadLength >= 0 && payloadLength <= MAX_PACKET_SIZE );
	REQUIRES( type >= 0 && type <= 255 );

	/* Set up the packet metadata to be MACed */
	status = writePacketMetadata( buffer, 64, &length, type, seqNo, 
								  version, payloadLength );
	if( cryptStatusError( status ) )
		return( status );

	/* Send the AAD to the GCM context for inclusion in the ICV 
	   calculation */
	setMessageData( &msgData, buffer, length );
	return( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S,
							 &msgData, CRYPT_IATTRIBUTE_AAD ) );
	}

/****************************************************************************
*																			*
*					Handshake Message Hash/MAC Functions					*
*																			*
****************************************************************************/

/* Perform assorted hashing of a data block, a dual dual MD5+SHA-1 hash for 
   SSL or straight SHA-2 hash for TLS 1.2+.  Since this is part of an 
   ongoing message exchange (in other words a failure potentially won't be 
   detected for some time) we check each return value.  This processing was 
   originally done using a dual MD5+SHA-1 hash, however TLS 1.2+ switched to 
   using a single SHA-2 hash, because of this we have to explicitly check 
   which hashing option we're using (in some cases it might be both since we 
   have to speculatively hash initial messages until we've agreed on a 
   version) and only use that hash.
   
   In addition to the overall hashing we may also be running a separate hash 
   of the messages that stops before the other hashing does if certificate-
   based client authentication is being used.  This would add even more 
   overhead to the whole process, however since it's only used with TLS 1.2+
   and in that case is restricted to using SHA-2 via hashing preferences
   sent in the hello messages, we can obtain the necessary hash value by
   cloning the SHA-2 context when we have to generate or verify the client
   signature */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int hashHSData( const SSL_HANDSHAKE_INFO *handshakeInfo,
					   IN_BUFFER( dataLength ) const void *data, 
					   IN_LENGTH const int dataLength )
	{
	int status;

	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );

	if( handshakeInfo->md5context != CRYPT_ERROR )
		{
		status = krnlSendMessage( handshakeInfo->md5context,
								  IMESSAGE_CTX_HASH, ( MESSAGE_CAST ) data,
								  dataLength );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( handshakeInfo->sha1context,
									  IMESSAGE_CTX_HASH, ( MESSAGE_CAST ) data,
									  dataLength );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( handshakeInfo->sha2context != CRYPT_ERROR )
		{
		status = krnlSendMessage( handshakeInfo->sha2context,
								  IMESSAGE_CTX_HASH, ( MESSAGE_CAST ) data,
								  dataLength );
		if( cryptStatusError( status ) )
			return( status );
		}
#ifdef CONFIG_SUITEB
	if( handshakeInfo->sha384context != CRYPT_ERROR )
		{
		status = krnlSendMessage( handshakeInfo->sha384context,
								  IMESSAGE_CTX_HASH, ( MESSAGE_CAST ) data,
								  dataLength );
		if( cryptStatusError( status ) )
			return( status );
		}
#endif /* CONFIG_SUITEB */

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int hashHSPacketRead( const SSL_HANDSHAKE_INFO *handshakeInfo, 
					  INOUT STREAM *stream )
	{
	const int dataLength = sMemDataLeft( stream );
	void *data;
	int status;

	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );

	/* On a read we've just processed the packet header and everything 
	   that's left in the stream is the data to be MACd.  Note that we can't 
	   use sMemGetDataBlockRemaining() for this because that returns the
	   entire available buffer, not just the amount of data in the buffer */
	status = sMemGetDataBlock( stream, &data, dataLength );
	if( cryptStatusOK( status ) )
		{
		ANALYSER_HINT( data != NULL );
		status = hashHSData( handshakeInfo, data, dataLength );
		}
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int hashHSPacketWrite( const SSL_HANDSHAKE_INFO *handshakeInfo, 
					   INOUT STREAM *stream,
					   IN_LENGTH_Z const int offset )
	{
	const int dataStart = offset + SSL_HEADER_SIZE;
	const int dataLength = stell( stream ) - dataStart;
	void *data;
	int status;

	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( offset >= 0 && offset < MAX_INTLENGTH );
	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );

	/* On a write we've just finished writing the packet and everything but
	   the header needs to be MACd */
	status = sMemGetDataBlockAbs( stream, dataStart, &data, dataLength );
	if( cryptStatusOK( status ) )
		{
		ANALYSER_HINT( data != NULL );
		status = hashHSData( handshakeInfo, data, dataLength );
		}
	return( status );
	}

/* Complete the dual MD5/SHA1 hash/MAC or SHA2 MAC that's used in the 
   finished message.  There are no less than three variations of this, one
   for SSL's dual MAC, one for TLS 1.0 - 1.1's IPsec cargo cult 96-bit
   PRF'd dual MAC, and one for TLS 1.2's similarly cargo-cult 96-bit PRF'd
   single MAC.
   
   We don't check the return value of every single component MAC operation 
   since it would lead to endless sequences of 'status = x; 
   if( cSOK( x ) ) ...' chains, on the remote chance that there's some 
   transient failure in a single component operation it'll be picked up at 
   the end anyway when the overall MAC check fails */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5, 6, 8 ) ) \
int completeSSLDualMAC( IN_HANDLE const CRYPT_CONTEXT md5context,
						IN_HANDLE const CRYPT_CONTEXT sha1context, 
						OUT_BUFFER( hashValuesMaxLen, *hashValuesLen )
							BYTE *hashValues, 
						IN_LENGTH_SHORT_MIN( MD5MAC_SIZE + SHA1MAC_SIZE ) \
							const int hashValuesMaxLen,
						OUT_LENGTH_SHORT_Z int *hashValuesLen,
						IN_BUFFER( labelLength ) const char *label, 
						IN_RANGE( 1, 64 ) const int labelLength, 
						IN_BUFFER( masterSecretLen ) const BYTE *masterSecret, 
						IN_LENGTH_SHORT const int masterSecretLen )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( hashValues, hashValuesMaxLen ) );
	assert( isWritePtr( hashValuesLen, sizeof( int ) ) );
	assert( isReadPtr( label, labelLength ) );
	assert( isReadPtr( masterSecret, masterSecretLen ) );

	REQUIRES( isHandleRangeValid( md5context ) );
	REQUIRES( isHandleRangeValid( sha1context ) );
	REQUIRES( hashValuesMaxLen >= MD5MAC_SIZE + SHA1MAC_SIZE && \
			  hashValuesMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( labelLength > 0 && labelLength <= 64 );
	REQUIRES( masterSecretLen > 0 && masterSecretLen < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*hashValuesLen = 0;

	/* Generate the inner portion of the handshake message's MAC:

		hash( handshake_messages || cl/svr_label || master_secret || pad1 )

	   Note that the SHA-1 pad size is 40 bytes and not 44 (to get a total
	   length of 64 bytes), this is due to an error in the spec */
	krnlSendMessage( md5context, IMESSAGE_CTX_HASH, 
					 ( MESSAGE_CAST ) label, labelLength );
	krnlSendMessage( sha1context, IMESSAGE_CTX_HASH, 
					 ( MESSAGE_CAST ) label, labelLength );
	krnlSendMessage( md5context, IMESSAGE_CTX_HASH, 
					 ( MESSAGE_CAST ) masterSecret, masterSecretLen );
	krnlSendMessage( sha1context, IMESSAGE_CTX_HASH, 
					 ( MESSAGE_CAST ) masterSecret, masterSecretLen );
	krnlSendMessage( md5context, IMESSAGE_CTX_HASH, PROTOHMAC_PAD1, 48 );
	krnlSendMessage( sha1context, IMESSAGE_CTX_HASH, PROTOHMAC_PAD1, 40 );
	krnlSendMessage( md5context, IMESSAGE_CTX_HASH, "", 0 );
	krnlSendMessage( sha1context, IMESSAGE_CTX_HASH, "", 0 );
	setMessageData( &msgData, hashValues, MD5MAC_SIZE );
	status = krnlSendMessage( md5context, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, hashValues + MD5MAC_SIZE, SHA1MAC_SIZE );
		status = krnlSendMessage( sha1context, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_HASHVALUE );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Reset the hash contexts */
	krnlSendMessage( md5context, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_HASHVALUE );
	krnlSendMessage( sha1context, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_HASHVALUE );

	/* Generate the outer portion of the handshake message's MAC:

		hash( master_secret || pad2 || inner_hash ) */
	krnlSendMessage( md5context, IMESSAGE_CTX_HASH, 
					 ( MESSAGE_CAST ) masterSecret, masterSecretLen );
	krnlSendMessage( sha1context, IMESSAGE_CTX_HASH, 
					 ( MESSAGE_CAST ) masterSecret, masterSecretLen );
	krnlSendMessage( md5context, IMESSAGE_CTX_HASH, PROTOHMAC_PAD2, 48 );
	krnlSendMessage( sha1context, IMESSAGE_CTX_HASH, PROTOHMAC_PAD2, 40 );
	krnlSendMessage( md5context, IMESSAGE_CTX_HASH, hashValues,
					 MD5MAC_SIZE );
	krnlSendMessage( sha1context, IMESSAGE_CTX_HASH, hashValues + MD5MAC_SIZE,
					 SHA1MAC_SIZE );
	krnlSendMessage( md5context, IMESSAGE_CTX_HASH, "", 0 );
	krnlSendMessage( sha1context, IMESSAGE_CTX_HASH, "", 0 );
	setMessageData( &msgData, hashValues, MD5MAC_SIZE );
	status = krnlSendMessage( md5context, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, hashValues + MD5MAC_SIZE, SHA1MAC_SIZE );
	status = krnlSendMessage( sha1context, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusOK( status ) )
		*hashValuesLen = MD5MAC_SIZE + SHA1MAC_SIZE;
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5, 6, 8 ) ) \
int completeTLSHashedMAC( IN_HANDLE const CRYPT_CONTEXT md5context,
						  IN_HANDLE const CRYPT_CONTEXT sha1context, 
						  OUT_BUFFER( hashValuesMaxLen, *hashValuesLen )
								BYTE *hashValues, 
						  IN_LENGTH_SHORT_MIN( TLS_HASHEDMAC_SIZE ) \
								const int hashValuesMaxLen,
						  OUT_LENGTH_SHORT_Z int *hashValuesLen,
						  IN_BUFFER( labelLength ) const char *label, 
						  IN_RANGE( 1, 64 ) const int labelLength, 
						  IN_BUFFER( masterSecretLen ) const BYTE *masterSecret, 
						  IN_LENGTH_SHORT const int masterSecretLen )
	{
	MECHANISM_DERIVE_INFO mechanismInfo;
	MESSAGE_DATA msgData;
	BYTE hashBuffer[ 64 + ( CRYPT_MAX_HASHSIZE * 2 ) + 8 ];
	int status;

	assert( isWritePtr( hashValues, hashValuesMaxLen ) );
	assert( isWritePtr( hashValuesLen, sizeof( int ) ) );
	assert( isReadPtr( label, labelLength ) );
	assert( isReadPtr( masterSecret, masterSecretLen ) );

	REQUIRES( isHandleRangeValid( md5context ) );
	REQUIRES( isHandleRangeValid( sha1context ) );
	REQUIRES( hashValuesMaxLen >= TLS_HASHEDMAC_SIZE && \
			  hashValuesMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( labelLength > 0 && labelLength <= 64 && \
			  labelLength + MD5MAC_SIZE + SHA1MAC_SIZE <= \
						64 + ( CRYPT_MAX_HASHSIZE * 2 ) );

	/* Clear return value */
	*hashValuesLen = 0;

	memcpy( hashBuffer, label, labelLength );

	/* Complete the hashing and get the MD5 and SHA-1 hashes */
	krnlSendMessage( md5context, IMESSAGE_CTX_HASH, "", 0 );
	krnlSendMessage( sha1context, IMESSAGE_CTX_HASH, "", 0 );
	setMessageData( &msgData, hashBuffer + labelLength, MD5MAC_SIZE );
	status = krnlSendMessage( md5context, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, hashBuffer + labelLength + MD5MAC_SIZE,
						SHA1MAC_SIZE );
		status = krnlSendMessage( sha1context, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_HASHVALUE );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Generate the TLS check value.  This isn't really a hash or a MAC but
	   is generated by feeding the MD5 and SHA1 hashes of the handshake 
	   messages into the TLS key derivation (PRF) function and truncating 
	   the result to 12 bytes (96 bits) IPsec cargo cult protocol design
	   purposes:

		TLS_PRF( label || MD5_hash || SHA1_hash ) */
	setMechanismDeriveInfo( &mechanismInfo, hashValues, 
							TLS_HASHEDMAC_SIZE, ( MESSAGE_CAST ) masterSecret, 
							masterSecretLen, CRYPT_USE_DEFAULT, hashBuffer, 
							labelLength + MD5MAC_SIZE + SHA1MAC_SIZE, 1 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
							  &mechanismInfo, MECHANISM_DERIVE_TLS );
	if( cryptStatusOK( status ) )
		*hashValuesLen = TLS_HASHEDMAC_SIZE;
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4, 5, 7 ) ) \
int completeTLS12HashedMAC( IN_HANDLE const CRYPT_CONTEXT sha2context,
							OUT_BUFFER( hashValuesMaxLen, *hashValuesLen ) \
								BYTE *hashValues, 
							IN_LENGTH_SHORT_MIN( TLS_HASHEDMAC_SIZE ) \
								const int hashValuesMaxLen,
							OUT_LENGTH_SHORT_Z int *hashValuesLen,
							IN_BUFFER( labelLength ) const char *label, 
							IN_RANGE( 1, 64 ) const int labelLength, 
							IN_BUFFER( masterSecretLen ) const BYTE *masterSecret, 
							IN_LENGTH_SHORT const int masterSecretLen )
	{
	MECHANISM_DERIVE_INFO mechanismInfo;
	MESSAGE_DATA msgData;
	BYTE hashBuffer[ 64 + ( CRYPT_MAX_HASHSIZE * 2 ) + 8 ];
	int macSize, status;

	assert( isWritePtr( hashValues, hashValuesMaxLen ) );
	assert( isWritePtr( hashValuesLen, sizeof( int ) ) );
	assert( isReadPtr( label, labelLength ) );
	assert( isReadPtr( masterSecret, masterSecretLen ) );

	REQUIRES( isHandleRangeValid( sha2context ) );
	REQUIRES( hashValuesMaxLen >= TLS_HASHEDMAC_SIZE && \
			  hashValuesMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( labelLength > 0 && labelLength <= 64 && \
			  labelLength + CRYPT_MAX_HASHSIZE <= 64 + ( CRYPT_MAX_HASHSIZE ) );

	/* Clear return value */
	*hashValuesLen = 0;

	memcpy( hashBuffer, label, labelLength );

	/* Get the MAC size */
	status = krnlSendMessage( sha2context, IMESSAGE_GETATTRIBUTE, &macSize,
							  CRYPT_CTXINFO_BLOCKSIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Complete the hashing and get the SHA-2 hash */
	krnlSendMessage( sha2context, IMESSAGE_CTX_HASH, "", 0 );
	setMessageData( &msgData, hashBuffer + labelLength, macSize );
	status = krnlSendMessage( sha2context, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Generate the TLS check value.  This isn't really a hash or a MAC but
	   is generated by feeding the SHA-2 hash of the handshake messages into 
	   the TLS key derivation (PRF) function and truncating the result to 12 
	   bytes (96 bits) for IPsec cargo cult protocol design purposes:

		TLS_PRF( label || SHA2_hash ) */
	setMechanismDeriveInfo( &mechanismInfo, hashValues, 
							TLS_HASHEDMAC_SIZE, ( MESSAGE_CAST ) masterSecret, 
							masterSecretLen, CRYPT_ALGO_SHA2, hashBuffer, 
							labelLength + macSize, 1 );
	if( macSize != bitsToBytes( 256 ) )
		mechanismInfo.hashParam = macSize;
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
							  &mechanismInfo, MECHANISM_DERIVE_TLS12 );
	if( cryptStatusOK( status ) )
		*hashValuesLen = TLS_HASHEDMAC_SIZE;
	return( status );
	}

/****************************************************************************
*																			*
*						Client-Auth Signature Functions						*
*																			*
****************************************************************************/

/* Create/check the signature on an SSL certificate verify message.  
   SSLv3/TLS use a weird signature format that dual-MACs (SSLv3) or hashes 
   (TLS) all of the handshake messages exchanged to date (SSLv3 additionally 
   hashes in further data like the master secret), then signs them using 
   nonstandard PKCS #1 RSA without the ASN.1 wrapper (that is, it uses the 
   raw concatenated SHA-1 and MD5 MAC (SSL) or hash (TLS) of the handshake 
   messages with PKCS #1 padding prepended), unless we're using DSA in which 
   case it drops the MD5 MAC/hash and uses only the SHA-1 one.  
   
   This is an incredible pain to support because it requires running a 
   parallel hash of handshake messages that terminates before the main 
   hashing does, further hashing/MAC'ing of additional data, and the use of 
   weird nonstandard data formats and signature mechanisms that aren't 
   normally supported by anything.  For example if the signing is to be done 
   via a smart card then we can't use the standard PKCS #1 sig mechanism, we 
   can't even use raw RSA and kludge the format together ourselves because 
   some PKCS #11 implementations don't support the _X509 (raw) mechanism, 
   what we have to do is tunnel the nonstandard sig.format information down 
   through several cryptlib layers and then hope that the PKCS #11 
   implementation that we're using (a) supports this format and (b) gets it 
   right.
   
   Another problem (which only occurs for SSLv3) is that the MAC requires 
   the use of the master secret, which isn't available for several hundred 
   more lines of code, so we have to delay producing any more data packets 
   until the master secret is available (either that or squirrel all packets
   away in some buffer somewhere so that they can be hashed later), which 
   severely screws up the handshake processing flow.  TLS is slightly better 
   here since it simply signs MD5-hash || SHA1-hash without requiring the
   use of the master secret, but even then it requires speculatively running 
   an MD5 and SHA-1 hash of all messages on every exchange on the remote 
   chance that the client will be using client certificates.  TLS 1.2
   finally moved to using standard signatures (PKCS #1 for RSA, conventional
   signatures for DSA/ECDSA), but still requires the speculative hashing of
   handshake messages.

   The chances of all of this custom data and signature handling working 
   correctly are fairly low, and in any case there's no advantage to the 
   weird mechanism and format used in SSL/TLS, all that we actually need to 
   do is sign the client and server nonces to ensure signature freshness.  
   Because of this what we actually do is just this, after which we create a 
   standard PKCS #1 signature via the normal cryptlib mechanisms, which 
   guarantees that it'll work with native cryptlib as well as any crypto 
   hardware implementation.  Since client certificates are hardly ever used 
   and when they are it's in a closed environment, it's extremely unlikely 
   that anyone will ever notice.  There'll be far more problems in trying to 
   use the nonstandard SSL/TLS signature mechanism than there are with using 
   a standard (but not-in-the-spec) one.
   
   The one exception to this is, as already mentioned above, TLS 1.2+, for
   which we can finally use a standard signature.  In this case we take
   a clone of the SHA-2 context that's been used to hash the handshake
   messages so far (the use of SHA-2 for this is enforced by the judicious
   use of TLS extensions, see the comments in ssl_ext.c for more on this)
   and sign that */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int createCertVerifyAltHash( const SSL_HANDSHAKE_INFO *handshakeInfo,
									OUT_HANDLE_OPT CRYPT_CONTEXT *iHashContext )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	BYTE nonceBuffer[ 64 + SSL_NONCE_SIZE + SSL_NONCE_SIZE + 8 ];
	int status;

	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( iHashContext, sizeof( CRYPT_CONTEXT ) ) );

	/* Clear return value */
	*iHashContext = CRYPT_ERROR;

	/* Hash the client and server nonces */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA1 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	memcpy( nonceBuffer, "certificate verify", 18 );
	memcpy( nonceBuffer + 18, handshakeInfo->clientNonce, SSL_NONCE_SIZE );
	memcpy( nonceBuffer + 18 + SSL_NONCE_SIZE, handshakeInfo->serverNonce,
			SSL_NONCE_SIZE );
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_CTX_HASH,
							  nonceBuffer, 
							  18 + SSL_NONCE_SIZE + SSL_NONCE_SIZE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_CTX_HASH, nonceBuffer, 0 );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iHashContext = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int createCertVerifyHash( const SESSION_INFO *sessionInfoPtr,
						  INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	CRYPT_CONTEXT iHashContext;
	int status;

	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	/* SSL and TLS 1.0 and 1.1 use a different signature strategy than 
	   TLS 1.2+ (see the long comment above) so this call is a no-op */
	if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
		return( CRYPT_OK );

	/* Clone the current hash state and complete the hashing for the cloned
	   context */
	status = cloneHashContext( handshakeInfo->sha2context, &iHashContext );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	handshakeInfo->certVerifyContext = iHashContext;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int createCertVerify( const SESSION_INFO *sessionInfoPtr,
					  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
					  INOUT STREAM *stream )
	{
	void *dataPtr;
	int dataLength, length = DUMMY_INIT, status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Get a pointer to the signature data block */
	status = sMemGetDataBlockRemaining( stream, &dataPtr, &dataLength );
	if( cryptStatusError( status ) )
		return( status );

	/* Create the signature.  The reason for the min() part of the
	   expression is that iCryptCreateSignature() gets suspicious of very
	   large buffer sizes, for example when the user has specified the use
	   of a huge send buffer */
	if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
		{
		CRYPT_CONTEXT iHashContext;

		/* Create the hash of the data to sign if necessary */
		status = createCertVerifyAltHash( handshakeInfo, &iHashContext );
		if( cryptStatusError( status ) )
			return( status );

		status = iCryptCreateSignature( dataPtr, 
						min( dataLength, MAX_INTLENGTH_SHORT - 1 ), &length, 
						CRYPT_FORMAT_CRYPTLIB, sessionInfoPtr->privateKey, 
						iHashContext, NULL );
		krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
		}
	else
		{
		status = iCryptCreateSignature( dataPtr, 
						min( dataLength, MAX_INTLENGTH_SHORT - 1 ), &length, 
						CRYPT_IFORMAT_TLS12, sessionInfoPtr->privateKey, 
						handshakeInfo->certVerifyContext, NULL );
		krnlSendNotifier( handshakeInfo->certVerifyContext, 
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->certVerifyContext = CRYPT_ERROR;
		}
	if( cryptStatusOK( status ) )
		status = sSkip( stream, length );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int checkCertVerify( const SESSION_INFO *sessionInfoPtr,
					 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
					 INOUT STREAM *stream, 
					 IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
						const int sigLength )
	{
	void *dataPtr;
	int status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sigLength >= MIN_CRYPT_OBJECTSIZE && \
			  sigLength < MAX_INTLENGTH_SHORT );

	/* Get a pointer to the signature data block */
	status = sMemGetDataBlock( stream, &dataPtr, sigLength );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( dataPtr != NULL );

	/* Verify the signature.  The reason for the min() part of the
	   expression is that iCryptCheckSignature() gets suspicious of very
	   large buffer sizes, for example when the user has specified the use
	   of a huge send buffer */
	if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
		{
		CRYPT_CONTEXT iHashContext;

		/* Create the hash of the data to verify if necessary */
		status = createCertVerifyAltHash( handshakeInfo, &iHashContext );
		if( cryptStatusError( status ) )
			return( status );

		status = iCryptCheckSignature( dataPtr, 
						min( sigLength, MAX_INTLENGTH_SHORT - 1 ), 
						CRYPT_FORMAT_CRYPTLIB, sessionInfoPtr->iKeyexAuthContext, 
						iHashContext, CRYPT_UNUSED, NULL );
		krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
		}
	else
		{
		status = iCryptCheckSignature( dataPtr, 
						min( sigLength, MAX_INTLENGTH_SHORT - 1 ), 
						CRYPT_IFORMAT_TLS12, sessionInfoPtr->iKeyexAuthContext, 
						handshakeInfo->certVerifyContext, CRYPT_UNUSED, NULL );
		krnlSendNotifier( handshakeInfo->certVerifyContext, 
						  IMESSAGE_DECREFCOUNT );
		handshakeInfo->certVerifyContext = CRYPT_ERROR;
#ifdef CONFIG_SUITEB_TESTS 
		if( cryptStatusOK( status ) )
			{
			int sigKeySize;

			status = krnlSendMessage( sessionInfoPtr->iKeyexAuthContext, 
									  IMESSAGE_GETATTRIBUTE, &sigKeySize, 
									  CRYPT_CTXINFO_KEYSIZE );
			if( cryptStatusOK( status ) )
				{
				DEBUG_PRINT(( "Verified client's P%d authentication.\n", 
							  bytesToBits( sigKeySize ) ));
				}
			}
#endif /* CONFIG_SUITEB_TESTS */
		}
	return( status );
	}

/****************************************************************************
*																			*
*						Keyex Signature Functions							*
*																			*
****************************************************************************/

/* Create/check the signature on the server key data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
static int createKeyexHash( const SSL_HANDSHAKE_INFO *handshakeInfo,
							OUT_HANDLE_OPT CRYPT_CONTEXT *hashContext,
							IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
							IN_INT_SHORT_Z const int hashParam,
							IN_BUFFER( keyDataLength ) const void *keyData, 
							IN_LENGTH_SHORT const int keyDataLength )
	{
	CRYPT_CONTEXT iHashContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	BYTE nonceBuffer[ SSL_NONCE_SIZE + SSL_NONCE_SIZE + 8 ];
	int status;

	assert( isReadPtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( hashContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( keyData, keyDataLength ) );

	REQUIRES( hashAlgo >= CRYPT_ALGO_FIRST_HASH && \
			  hashAlgo <= CRYPT_ALGO_LAST_HASH );
	REQUIRES( hashParam >= 0 && hashParam < MAX_INTLENGTH_SHORT );
	REQUIRES( keyDataLength > 0 && keyDataLength < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*hashContext = CRYPT_ERROR;

	/* Create the hash context */
	setMessageCreateObjectInfo( &createInfo, hashAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iHashContext = createInfo.cryptHandle;
	if( hashParam != 0 )
		{
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &hashParam,
								  CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}

	/* Hash the client and server nonces and key data */
	memcpy( nonceBuffer, handshakeInfo->clientNonce, SSL_NONCE_SIZE );
	memcpy( nonceBuffer + SSL_NONCE_SIZE, handshakeInfo->serverNonce,
			SSL_NONCE_SIZE );
	status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
							  nonceBuffer, SSL_NONCE_SIZE + SSL_NONCE_SIZE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
								  ( MESSAGE_CAST ) keyData, keyDataLength );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
								  nonceBuffer, 0 );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	*hashContext = iHashContext;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int createKeyexSignature( INOUT SESSION_INFO *sessionInfoPtr, 
						  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						  INOUT STREAM *stream, 
						  IN_BUFFER( keyDataLength ) const void *keyData, 
						  IN_LENGTH_SHORT const int keyDataLength )
	{
	CRYPT_CONTEXT md5Context = DUMMY_INIT, shaContext;
	void *dataPtr;
	int dataLength, sigLength = DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( keyData, keyDataLength ) );

	REQUIRES( keyDataLength > 0 && keyDataLength < MAX_INTLENGTH_SHORT );

	/* Hash the data to be signed */
	status = createKeyexHash( handshakeInfo, &shaContext, 
				( handshakeInfo->keyexSigHashAlgo != CRYPT_ALGO_NONE ) ? \
					handshakeInfo->keyexSigHashAlgo : CRYPT_ALGO_SHA1,
				handshakeInfo->keyexSigHashAlgoParam, keyData, keyDataLength );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
		{
		status = createKeyexHash( handshakeInfo, &md5Context, 
								  CRYPT_ALGO_MD5, 0, keyData, keyDataLength );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( shaContext, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}

	/* Sign the hashes.  The reason for the min() part of the expression is
	   that iCryptCreateSignature() gets suspicious of very large buffer
	   sizes, for example when the user has specified the use of a huge send
	   buffer */
	status = sMemGetDataBlockRemaining( stream, &dataPtr, &dataLength );
	if( cryptStatusOK( status ) )
		{
		if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
			{
			status = iCryptCreateSignature( dataPtr, 
											min( dataLength, \
												 MAX_INTLENGTH_SHORT - 1 ), 
											&sigLength, CRYPT_IFORMAT_TLS12, 
											sessionInfoPtr->privateKey,
											shaContext, NULL );
			}
		else
			{
			SIGPARAMS sigParams;

			initSigParams( &sigParams );
			sigParams.iSecondHash = shaContext;
			status = iCryptCreateSignature( dataPtr, 
											min( dataLength, \
												 MAX_INTLENGTH_SHORT - 1 ), 
											&sigLength, CRYPT_IFORMAT_SSL, 
											sessionInfoPtr->privateKey,
											md5Context, &sigParams );
			}
		}
	if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
		krnlSendNotifier( md5Context, IMESSAGE_DECREFCOUNT );
	krnlSendNotifier( shaContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	return( sSkip( stream, sigLength ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int checkKeyexSignature( INOUT SESSION_INFO *sessionInfoPtr, 
						 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						 INOUT STREAM *stream, 
						 IN_BUFFER( keyDataLength ) const void *keyData, 
						 IN_LENGTH_SHORT const int keyDataLength,
						 const BOOLEAN isECC )
	{
	CRYPT_CONTEXT md5Context = DUMMY_INIT, shaContext;
	CRYPT_ALGO_TYPE hashAlgo = CRYPT_ALGO_SHA1;
	void *dataPtr;
	int dataLength, keyexKeySize, sigKeySize = DUMMY_INIT, hashParam = 0;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( keyData, keyDataLength ) );

	REQUIRES( keyDataLength > 0 && keyDataLength < MAX_INTLENGTH_SHORT );

	/* Make sure that there's enough data present for at least a minimal-
	   length signature and get a pointer to the signature data */
	if( sMemDataLeft( stream ) < ( isECC ? \
								   MIN_PKCSIZE_ECCPOINT : MIN_PKCSIZE ) )
		return( CRYPT_ERROR_BADDATA );
	status = sMemGetDataBlockRemaining( stream, &dataPtr, &dataLength );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( dataPtr != NULL );

	/* TLS 1.2+ precedes the signature itself with an indication of the hash
	   and signature algorithm that's required to verify it, so if we're 
	   using this format then we have to process the identifiers before we
	   can create the signature-verification hashes.

	   We disallow SHA1 since the whole point of TLS 1.2 was to move away 
	   from it, and a poll on the ietf-tls list indicated that all known 
	   implementations (both of them) work fine with this configuration */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		static const MAP_TABLE hashAlgoIDTbl[] = {
			{ 1000, CRYPT_ALGO_SHAng },
#ifdef CONFIG_SUITEB
			{ TLS_HASHALGO_SHA384, CRYPT_ALGO_SHA2 },
#endif /* CONFIG_SUITEB */
			{ TLS_HASHALGO_SHA2, CRYPT_ALGO_SHA2 },
#if 0	/* 2/11/11 Disabled option for SHA1 after poll on ietf-tls list */
			{ TLS_HASHALGO_SHA1, CRYPT_ALGO_SHA1 },
#endif /* 0 */
			{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
			};
		int cryptHashAlgo, tlsHashAlgo;

		/* Get the hash algorithm that we need to use.  We don't care about
		   the signature algorithm since we've already been given the public
		   key for it */
		status = tlsHashAlgo = sgetc( stream );
		if( cryptStatusError( status ) )
			return( status );
		( void ) sgetc( stream );
		status = mapValue( tlsHashAlgo, &cryptHashAlgo, hashAlgoIDTbl, 
						   FAILSAFE_ARRAYSIZE( hashAlgoIDTbl, MAP_TABLE ) );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_NOTAVAIL ); 
		hashAlgo = cryptHashAlgo;	/* int vs.enum */
		if( tlsHashAlgo == TLS_HASHALGO_SHA384 )
			hashParam = bitsToBytes( 384 );
		}

	/* Hash the data to be signed */
	status = createKeyexHash( handshakeInfo, &shaContext, hashAlgo, 
							  hashParam, keyData, keyDataLength );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
		{
		status = createKeyexHash( handshakeInfo, &md5Context, 
								  CRYPT_ALGO_MD5, 0, keyData, keyDataLength );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( shaContext, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}

	/* Check the signature on the hashes.  The reason for the min() part of
	   the expression is that iCryptCheckSignature() gets suspicious of
	   very large buffer sizes, for example when the user has specified the
	   use of a huge send buffer */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		status = iCryptCheckSignature( dataPtr, 
								min( dataLength, MAX_INTLENGTH_SHORT - 1 ),
								CRYPT_IFORMAT_TLS12, 
								sessionInfoPtr->iKeyexCryptContext, 
								shaContext, CRYPT_UNUSED, NULL );
		}
	else
		{
		status = iCryptCheckSignature( dataPtr, 
								min( dataLength, MAX_INTLENGTH_SHORT - 1 ),
								CRYPT_IFORMAT_SSL, 
								sessionInfoPtr->iKeyexCryptContext, 
								md5Context, shaContext, NULL );
		}
	if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
		krnlSendNotifier( md5Context, IMESSAGE_DECREFCOUNT );
	krnlSendNotifier( shaContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the relative strengths of the keyex and signature keys 
	   match.  This is just a general precaution for RSA/DSA, but is 
	   mandated for ECC with Suite B in order to make the appropriate 
	   fashion statement (see the comment below).  When performing the check 
	   we allow a small amount of wiggle room to deal with keygen 
	   differences */
	status = krnlSendMessage( handshakeInfo->dhContext, 
							  IMESSAGE_GETATTRIBUTE, &keyexKeySize, 
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( sessionInfoPtr->iKeyexCryptContext,
								  IMESSAGE_GETATTRIBUTE, &sigKeySize, 
								  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( isECC )
		{
		/* For ECC with Suite B the signature key size has to match the 
		   keyex key size otherwise fashion dictums are violated (if you 
		   could just sign size n with size n+1 then you wouldn't need 
		   hashsize n/n+1 and keysize n/n+1 and whatnot) */
		if( sigKeySize < keyexKeySize - bitsToBytes( 8 ) )
			return( CRYPT_ERROR_NOSECURE );
#ifdef CONFIG_SUITEB
		if( ( sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB ) && \
			sigKeySize > keyexKeySize + bitsToBytes( 8 ) )
			return( CRYPT_ERROR_NOSECURE );
  #ifdef CONFIG_SUITEB_TESTS 
		DEBUG_PRINT(( "Verified server's P%d keyex with P%d signature.\n", 
					  bytesToBits( keyexKeySize ), 
					  bytesToBits( sigKeySize ) ));
  #endif /* CONFIG_SUITEB_TESTS */
#endif /* CONFIG_SUITEB */
		}
	else
		{
		/* For conventional keyex/signatures the bounds are a bit looser 
		   because non-ECC keygen mechanisms can result in a wider variation 
		   of actual vs. nominal key size */
		if( sigKeySize < keyexKeySize - bitsToBytes( 32 ) )
			return( CRYPT_ERROR_NOSECURE );
		}

	/* Skip the signature data */
	return( readUniversal16( stream ) );
	}
#endif /* USE_SSL */
