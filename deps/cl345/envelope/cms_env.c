/****************************************************************************
*																			*
*						cryptlib CMS Enveloping Routines					*
*					    Copyright Peter Gutmann 1996-2016					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "envelope.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "envelope/envelope.h"
#endif /* Compiler-specific includes */

#ifdef USE_CMS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the envelope state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckEnvCMSEnv( const ENVELOPE_INFO *envelopeInfoPtr )
	{
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Check the general envelope state */
	if( !sanityCheckEnvelope( envelopeInfoPtr ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvCMSEnv: Envelope check" ));
		return( FALSE );
		}

	/* Make sure that general envelope state is in order */
	if( envelopeInfoPtr->type != CRYPT_FORMAT_CRYPTLIB && \
		envelopeInfoPtr->type != CRYPT_FORMAT_CMS && \
		envelopeInfoPtr->type != CRYPT_FORMAT_SMIME )
		{
		DEBUG_PUTS(( "sanityCheckEnvCMSEnv: Type" ));
		return( FALSE );
		}
	if( TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_ISDEENVELOPE ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvCMSEnv: General info" ));
		return( FALSE );
		}

	return( TRUE );
	}

/* Check that a requested algorithm type is valid with enveloped data */

CHECK_RETVAL_BOOL \
BOOLEAN cmsCheckAlgo( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
					  IN_MODE_OPT const CRYPT_MODE_TYPE cryptMode )
	{
	REQUIRES_B( cryptAlgo > CRYPT_ALGO_NONE && \
				cryptAlgo < CRYPT_ALGO_LAST_EXTERNAL );
	REQUIRES_B( isEnumRangeOpt( cryptMode, CRYPT_MODE ) );

	return( checkAlgoID( cryptAlgo, cryptMode ) );
	}

/* Retrieve the principal context type from the envelope's action list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getActionContext( const ENVELOPE_INFO *envelopeInfoPtr,
							 OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext )
	{
	const ACTION_LIST *actionListPtr;

	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );

	/* Clear return value */
	*iCryptContext = CRYPT_ERROR;

	switch( envelopeInfoPtr->usage )
		{
		case ACTION_CRYPT:
			if( TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_AUTHENC ) )
				actionListPtr = findAction( envelopeInfoPtr, ACTION_xxx );
			else
				actionListPtr = findAction( envelopeInfoPtr, ACTION_CRYPT );
			break;

		case ACTION_MAC:
			actionListPtr = findAction( envelopeInfoPtr, ACTION_MAC );
			break;

		default:
			retIntError();
		}
	REQUIRES( actionListPtr != NULL );
	REQUIRES( sanityCheckActionList( actionListPtr ) );

	*iCryptContext = actionListPtr->iCryptHandle;

	return( CRYPT_OK );
	}

/* Get the OID for a CMS content type.  If no type is explicitly given, we
   assume raw data */

static const OID_INFO contentOIDs[] = {
	{ OID_CMS_DATA, CRYPT_CONTENT_DATA },
	{ OID_CMS_SIGNEDDATA, CRYPT_CONTENT_SIGNEDDATA },
	{ OID_CMS_ENVELOPEDDATA, CRYPT_CONTENT_ENVELOPEDDATA },
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x07\x04" ), CRYPT_CONTENT_SIGNEDANDENVELOPEDDATA },
	{ OID_CMS_DIGESTEDDATA, CRYPT_CONTENT_DIGESTEDDATA },
	{ OID_CMS_ENCRYPTEDDATA, CRYPT_CONTENT_ENCRYPTEDDATA },
	{ OID_CMS_COMPRESSEDDATA, CRYPT_CONTENT_COMPRESSEDDATA },
	{ OID_CMS_AUTHDATA, CRYPT_CONTENT_AUTHDATA },
	{ OID_CMS_AUTHENVDATA, CRYPT_CONTENT_AUTHENVDATA },
	{ OID_CMS_TSTOKEN, CRYPT_CONTENT_TSTINFO },
	{ OID_MS_SPCINDIRECTDATACONTEXT, CRYPT_CONTENT_SPCINDIRECTDATACONTEXT },
	{ OID_CRYPTLIB_RTCSREQ, CRYPT_CONTENT_RTCSREQUEST },
	{ OID_CRYPTLIB_RTCSRESP, CRYPT_CONTENT_RTCSRESPONSE },
	{ OID_CRYPTLIB_RTCSRESP_EXT, CRYPT_CONTENT_RTCSRESPONSE_EXT },
	{ MKOID( "\x06\x06\x67\x81\x08\x01\x01\x01" ), CRYPT_CONTENT_MRTD },
	{ NULL, 0 }, { NULL, 0 }
	};

CHECK_RETVAL_PTR \
static const BYTE *getContentOID( IN_ENUM( CRYPT_CONTENT ) \
										const CRYPT_CONTENT_TYPE contentType )
	{
	int i, LOOP_ITERATOR;

	REQUIRES_N( isEnumRange( contentType, CRYPT_CONTENT ) );

	LOOP_MED( i = 0, 
			  contentOIDs[ i ].oid != NULL && \
				i < FAILSAFE_ARRAYSIZE( contentOIDs, OID_INFO ),
			  i++ )
		{
		if( contentOIDs[ i ].selectionID == contentType )
			return( contentOIDs[ i ].oid );
		}
	ENSURES_N( LOOP_BOUND_OK );

	retIntError_Null();
	}

/* Copy as much post-data state information (i.e. signatures) from the
   auxiliary buffer to the main buffer as possible */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int copyFromAuxBuffer( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	int bytesCopied, dataLeft;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Copy as much of the signature data as we can across */
	bytesCopied = min( envelopeInfoPtr->bufSize - envelopeInfoPtr->bufPos,
					   envelopeInfoPtr->auxBufPos );
	REQUIRES( boundsCheckZ( envelopeInfoPtr->bufPos, bytesCopied, 
							envelopeInfoPtr->bufSize ) );
	memcpy( envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos,
			envelopeInfoPtr->auxBuffer, bytesCopied );
	envelopeInfoPtr->bufPos += bytesCopied;

	/* Since we're in the post-data state any necessary payload data 
	   segmentation has been completed, however, the caller can't copy out 
	   any post-payload data because it's past the end-of-segment position. 
	   In order to allow the buffer to be emptied to make room for new data 
	   from the auxBuffer we set the end-of-segment position to the end of 
	   the new data */
	envelopeInfoPtr->segmentDataEnd = envelopeInfoPtr->bufPos;

	/* If there's anything left, move it down in the buffer */
	dataLeft = envelopeInfoPtr->auxBufPos - bytesCopied;
	if( dataLeft > 0 )
		{
		REQUIRES( boundsCheck( bytesCopied, dataLeft, 
							   envelopeInfoPtr->auxBufPos ) );
		memmove( envelopeInfoPtr->auxBuffer, 
				 envelopeInfoPtr->auxBuffer + bytesCopied, dataLeft );
		}
	envelopeInfoPtr->auxBufPos = dataLeft;
	
	ENSURES( dataLeft >= 0 );

	return( ( dataLeft > 0 ) ? CRYPT_ERROR_OVERFLOW : CRYPT_OK );
	}

/* Write one or more indefinite-length end-of-contents indicators */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeEOCs( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
					  IN_RANGE( 1, 8 ) const int count )
	{
	static const BYTE indefEOC[ 16 ] = \
						{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	const int dataLeft = envelopeInfoPtr->bufSize - envelopeInfoPtr->bufPos;
	const int eocLength = count * sizeofEOC();

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( count >= 1 && count <= 8 );
	REQUIRES( eocLength >= sizeofEOC() && \
			  eocLength <= ( 8 * sizeofEOC() ) );	/* Count = 1...8 */

	if( dataLeft < eocLength )
		return( CRYPT_ERROR_OVERFLOW );
	REQUIRES( boundsCheckZ( envelopeInfoPtr->bufPos, eocLength, 
							envelopeInfoPtr->bufSize ) );
	memcpy( envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos, indefEOC,
			eocLength );
	envelopeInfoPtr->bufPos += eocLength;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Emit Content-Specific Headers						*
*																			*
****************************************************************************/

/* Write the header fields that encapsulate any enveloped data:

   SignedData/DigestedData */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeSignedDataHeader( INOUT STREAM *stream,
								  const ENVELOPE_INFO *envelopeInfoPtr,
								  const BOOLEAN isSignedData )
	{
	const BYTE *contentOID = getContentOID( envelopeInfoPtr->contentType );
	ACTION_LIST *actionListPtr;
	long dataSize;
	int hashActionSize = 0, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( isSignedData == TRUE || isSignedData == FALSE );
	REQUIRES( contentOID != NULL );

	/* Determine the size of the hash actions */
	LOOP_MED( actionListPtr = DATAPTR_GET( envelopeInfoPtr->actionList ), 
			  actionListPtr != NULL,
			  actionListPtr = DATAPTR_GET( actionListPtr->next ) )
		{
		int actionSize;

		REQUIRES( sanityCheckActionList( actionListPtr ) );

		status = actionSize = \
					sizeofContextAlgoID( actionListPtr->iCryptHandle, 0 );
		if( cryptStatusError( status ) )
			return( status );
		hashActionSize += actionSize;
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( isShortIntegerRangeNZ( hashActionSize ) );
	
	/* Determine the size of the SignedData/DigestedData */
	if( envelopeInfoPtr->payloadSize == CRYPT_UNUSED || \
		TEST_FLAG( envelopeInfoPtr->dataFlags, ENVDATA_FLAG_HASINDEFTRAILER ) )
		dataSize = CRYPT_UNUSED;
	else
		{
		/* Determine the size of the content OID + content */
		dataSize = ( envelopeInfoPtr->payloadSize > 0 ) ? \
			sizeofObject( sizeofObject( envelopeInfoPtr->payloadSize ) ) : 0;
		dataSize = sizeofObject( sizeofOID( contentOID ) + dataSize );

		/* Determine the size of the version, hash algoID, content, 
		   certificate chain, and signatures */
		dataSize = sizeofShortInteger( 1 ) + sizeofObject( hashActionSize ) + \
				   dataSize + envelopeInfoPtr->extraDataSize + \
				   sizeofObject( envelopeInfoPtr->signActionSize );
		}
	ENSURES( dataSize == CRYPT_UNUSED || \
			   ( dataSize >= MIN_CRYPT_OBJECTSIZE && \
				 dataSize < MAX_BUFFER_SIZE ) );

	/* Write the SignedData/DigestedData header, version number, and SET OF
	   DigestInfo */
	if( isSignedData )
		{
		status = writeCMSheader( stream, OID_CMS_SIGNEDDATA, 
								 sizeofOID( OID_CMS_SIGNEDDATA ), 
								 dataSize, FALSE );
		}
	else
		{
		status = writeCMSheader( stream, OID_CMS_DIGESTEDDATA, 
								 sizeofOID( OID_CMS_DIGESTEDDATA ), 
								 dataSize, FALSE );
		}
	if( cryptStatusError( status ) )
		return( status );
	if( envelopeInfoPtr->contentType != CRYPT_CONTENT_DATA )
		{
		/* If the encapsulated content-type isn't Data, the version number
		   is 3 rather than 1 (no known implementation actually pays any 
		   attention to this, but the spec requires it so we may as well do
		   it) */
		writeShortInteger( stream, 3, DEFAULT_TAG );
		}
	else
		writeShortInteger( stream, 1, DEFAULT_TAG );
	writeSet( stream, hashActionSize );
	LOOP_MED( actionListPtr = DATAPTR_GET( envelopeInfoPtr->actionList ), 
			  actionListPtr != NULL,
			  actionListPtr = DATAPTR_GET( actionListPtr->next ) )
		{
		REQUIRES( sanityCheckActionList( actionListPtr ) );

		status = writeContextAlgoID( stream, actionListPtr->iCryptHandle );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	/* Write the inner Data header */
	return( writeCMSheader( stream, contentOID, sizeofOID( contentOID ), 
							envelopeInfoPtr->payloadSize, TRUE ) );
	}

/* EncryptedContentInfo contained within EnvelopedData.  This may also be 
   Authenticated or AuthEnc data so the encryption context can be 
   CRYPT_UNUSED */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int getBlockedPayloadSize( IN_LENGTH_INDEF const long payloadSize, 
								  IN_LENGTH_IV const int blockSize,
								  OUT_LENGTH_INDEF long *blockedPayloadSize )
	{
	assert( isWritePtr( blockedPayloadSize, sizeof( long ) ) );

	REQUIRES( payloadSize == CRYPT_UNUSED || \
			  isIntegerRangeNZ( payloadSize ) );
	REQUIRES( blockSize >= 1 && blockSize <= CRYPT_MAX_IVSIZE );

	/* Clear return value */
	*blockedPayloadSize = 0;

	/* If it's an indefinite length payload the blocked size is also of 
	   indefinite length */
	if( payloadSize == CRYPT_UNUSED )
		{
		*blockedPayloadSize = CRYPT_UNUSED;
		return( CRYPT_OK );
		}

	/* If it's a stream cipher there's no encryption blocking */
	if( blockSize <= 1 )
		{
		*blockedPayloadSize = payloadSize;
		return( CRYPT_OK );
		}

	/* Calculate the size of the payload after PKCS #5 block padding.  This 
	   isn't just the size rounded up to the nearest multiple of the block 
	   size since if the size is already a multiple of the block size it 
	   expands by another block, so we make the payload look one byte longer 
	   before rounding to the block size to ensure the one-block expansion */
	*blockedPayloadSize = roundUp( payloadSize + 1, blockSize );

	ENSURES( *blockedPayloadSize >= MIN_IVSIZE && \
			 *blockedPayloadSize <= payloadSize + CRYPT_MAX_IVSIZE );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeEncryptedContentHeader( INOUT STREAM *stream,
							IN_BUFFER( contentOIDlength ) const BYTE *contentOID, 
							IN_LENGTH_OID const int contentOIDlength,
							IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							IN_LENGTH_INDEF const long payloadSize, 
							IN_LENGTH_IV const long blockSize )
	{
	long blockedPayloadSize;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( contentOID, contentOIDlength ) );
	
	REQUIRES( isHandleRangeValid( iCryptContext ) || \
			  iCryptContext == CRYPT_UNUSED );
	REQUIRES( payloadSize == CRYPT_UNUSED || \
			  isIntegerRangeNZ( payloadSize ) );
	REQUIRES( blockSize >= 1 && blockSize <= CRYPT_MAX_IVSIZE );

	status = getBlockedPayloadSize( payloadSize, blockSize, 
									&blockedPayloadSize );
	if( cryptStatusError( status ) )
		return( status );
	return( writeCMSencrHeader( stream, contentOID, contentOIDlength,
								blockedPayloadSize, iCryptContext ) );
	}

/* EncryptedData, EnvelopedData */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
static int getEncryptedContentSize( const ENVELOPE_INFO *envelopeInfoPtr,
									IN_BUFFER( contentOIDlength ) const BYTE *contentOID, 
									IN_LENGTH_OID const int contentOIDlength,
									OUT_LENGTH_INDEF long *blockedPayloadSize,
									OUT_LENGTH_Z long *encrContentInfoSize )
	{
	CRYPT_CONTEXT iCryptContext;
	long length DUMMY_INIT;
	int status;

	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtrDynamic( contentOID, contentOIDlength ) );
	assert( isWritePtr( blockedPayloadSize, sizeof( long ) ) );
	assert( isWritePtr( encrContentInfoSize, sizeof( long ) ) );

	REQUIRES( contentOIDlength >= MIN_OID_SIZE && \
			  contentOIDlength <= MAX_OID_SIZE );

	/* Clear return values */
	*blockedPayloadSize = *encrContentInfoSize = 0;

	/* Calculate the size of the payload after encryption blocking */
	status = getBlockedPayloadSize( envelopeInfoPtr->payloadSize, 
									envelopeInfoPtr->blockSize,
									blockedPayloadSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Calculate the size of the CMS ContentInfo header */
	status = getActionContext( envelopeInfoPtr, &iCryptContext );
	if( cryptStatusOK( status ) )
		{
		status = length = \
			sizeofCMSencrHeader( contentOID, contentOIDlength, 
								 *blockedPayloadSize, iCryptContext );
		}
	if( cryptStatusError( status ) )
		return( status );
	*encrContentInfoSize = length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeEncryptionHeader( INOUT STREAM *stream, 
								  IN_BUFFER( oidLength ) const BYTE *oid, 
								  IN_LENGTH_OID const int oidLength, 
								  IN_RANGE( 0, 2 ) const int version, 
								  IN_LENGTH_INDEF const long blockedPayloadSize,
								  IN_LENGTH_INDEF const long extraSize )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( oid, oidLength ) );

	REQUIRES( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE );
	REQUIRES( version >= 0 && version <= 2 );
	REQUIRES( ( oidLength == sizeofOID( OID_CMS_AUTHDATA ) && \
				!memcmp( oid, OID_CMS_AUTHDATA, \
						 sizeofOID( OID_CMS_AUTHDATA ) ) ) || \
			  blockedPayloadSize == CRYPT_UNUSED || \
			  ( blockedPayloadSize >= MIN_IVSIZE && \
				blockedPayloadSize < MAX_INTLENGTH ) );
	REQUIRES( extraSize == CRYPT_UNUSED || \
			  ( extraSize > 0 && extraSize < MAX_BUFFER_SIZE ) );

	status = writeCMSheader( stream, oid, oidLength,
							 ( blockedPayloadSize == CRYPT_UNUSED || \
							   extraSize == CRYPT_UNUSED ) ? \
								CRYPT_UNUSED : \
								sizeofShortInteger( 0 ) + extraSize + \
								blockedPayloadSize,
							  FALSE );
	if( cryptStatusError( status ) )
		return( status );
	return( writeShortInteger( stream, version, DEFAULT_TAG ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeEncryptedDataHeader( INOUT STREAM *stream,
									 const ENVELOPE_INFO *envelopeInfoPtr )
	{
	const BYTE *contentOID = getContentOID( envelopeInfoPtr->contentType );
	long blockedPayloadSize, encrContentInfoSize;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( contentOID != NULL );

	/* Calculate the size of the payload due to blocking and the ContentInfo
	   header */
	status = getEncryptedContentSize( envelopeInfoPtr, contentOID,
									  sizeofOID( contentOID ), 
									  &blockedPayloadSize, 
									  &encrContentInfoSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the EncryptedData header, version number, and
	   EncryptedContentInfo header */
	status = writeEncryptionHeader( stream, OID_CMS_ENCRYPTEDDATA, 
									sizeofOID( OID_CMS_ENCRYPTEDDATA ), 0,
									blockedPayloadSize, encrContentInfoSize );
	if( cryptStatusError( status ) )
		return( status );
	return( writeEncryptedContentHeader( stream, contentOID, 
				sizeofOID( contentOID ), envelopeInfoPtr->iCryptContext, 
				envelopeInfoPtr->payloadSize, envelopeInfoPtr->blockSize ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeEnvelopedDataHeader( INOUT STREAM *stream,
									 const ENVELOPE_INFO *envelopeInfoPtr )
	{
	const BYTE *contentOID = getContentOID( envelopeInfoPtr->contentType );
	long blockedPayloadSize, encrContentInfoSize;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( contentOID != NULL );

	/* Calculate the size of the payload due to blocking and the ContentInfo
	   header */
	status = getEncryptedContentSize( envelopeInfoPtr, contentOID,
									  sizeofOID( contentOID ), 
									  &blockedPayloadSize, 
									  &encrContentInfoSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the EnvelopedData header and version number and start of the 
	   SET OF RecipientInfo/EncryptionKeyInfo.  Technically we need to jump 
	   through all sorts of hoops based on the contents and versions of 
	   encapsulated RecipientInfo structures but nothing seems to care about 
	   this so we just use a version of 0 */
	status = writeEncryptionHeader( stream, OID_CMS_ENVELOPEDDATA, 
						sizeofOID( OID_CMS_ENVELOPEDDATA ), 0, 
						blockedPayloadSize,
						( envelopeInfoPtr->cryptActionSize == CRYPT_UNUSED ) ? \
							CRYPT_UNUSED : \
							sizeofObject( envelopeInfoPtr->cryptActionSize ) + \
								encrContentInfoSize );
	if( cryptStatusError( status ) )
		return( status );

	return( ( envelopeInfoPtr->cryptActionSize == CRYPT_UNUSED ) ? \
			writeSetIndef( stream ) : \
			writeSet( stream, envelopeInfoPtr->cryptActionSize ) );
	}

/* AuthenticatedData, AuthEnvData */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeAuthenticatedDataHeader( INOUT STREAM *stream,
							const ENVELOPE_INFO *envelopeInfoPtr )
	{
	const ACTION_LIST *actionListPtr = \
					DATAPTR_GET( envelopeInfoPtr->actionList );
	const BYTE *contentOID = getContentOID( envelopeInfoPtr->contentType );
	int macActionSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( actionListPtr != NULL );
	REQUIRES( sanityCheckActionList( actionListPtr ) );
	REQUIRES( contentOID != NULL );

	/* Determine the encoded size of the MAC info */
	macActionSize = sizeofContextAlgoID( actionListPtr->iCryptHandle, 0 );
	if( cryptStatusError( macActionSize ) )
		return( macActionSize );

	/* Write the AuthenticatedData header and version number and start of 
	   the SET OF RecipientInfo.  Technically this isn't an encryption 
	   header but it uses the same format */
 	if( envelopeInfoPtr->payloadSize == CRYPT_UNUSED )
		{
		status = writeEncryptionHeader( stream, OID_CMS_AUTHDATA, 
										sizeofOID( OID_CMS_AUTHDATA ), 0, 1, 
										CRYPT_UNUSED );
		}
	else
		{
		int macSize, contentInfoSize;

		/* Determine the size of the MAC and the encapsulated content 
		   header */
		status = krnlSendMessage( actionListPtr->iCryptHandle, 
								  IMESSAGE_GETATTRIBUTE, &macSize, 
								  CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			return( status );
		contentInfoSize = sizeofObject( \
							sizeofObject( envelopeInfoPtr->payloadSize ) );
		contentInfoSize = sizeofObject( sizeofOID( contentOID ) + \
										contentInfoSize ) - \
						  envelopeInfoPtr->payloadSize;
		REQUIRES( contentInfoSize >= 16 && \
				  contentInfoSize < MAX_INTLENGTH );

		/* Write the data header */
		status = writeEncryptionHeader( stream, OID_CMS_AUTHDATA, 
					sizeofOID( OID_CMS_AUTHDATA ), 0, 
					envelopeInfoPtr->payloadSize,
					( envelopeInfoPtr->cryptActionSize == CRYPT_UNUSED ) ? \
						CRYPT_UNUSED : \
						sizeofObject( envelopeInfoPtr->cryptActionSize ) + \
							macActionSize + contentInfoSize + \
							sizeofObject( macSize ) );
		}
	if( cryptStatusError( status ) )
		return( status );

	return( ( envelopeInfoPtr->cryptActionSize == CRYPT_UNUSED ) ? \
			writeSetIndef( stream ) : \
			writeSet( stream, envelopeInfoPtr->cryptActionSize ) );
	}

CHECK_RETVAL \
static int setKDFParams( IN_HANDLE const CRYPT_CONTEXT iGenericSecret,
						 IN_ALGO const CRYPT_ALGO_TYPE kdfAlgo )
	{
	static const BYTE *fixedParamData = MKDATA( "\x04\x00\x02\x01\x01" );
	MESSAGE_DATA msgData;				/* OCTET STRING SIZE(0) + INTEGER(1) */
	STREAM stream;
	BYTE kdfParamData[ CRYPT_MAX_TEXTSIZE + 8 ];
	int kdfAlgoIDsize, kdfParamDataSize DUMMY_INIT, status;

	REQUIRES( isHandleRangeValid( iGenericSecret ) );
	REQUIRES( isMacAlgo( kdfAlgo ) && kdfAlgo != CRYPT_ALGO_HMAC_SHA1 );

	/* Get the KDF algoID size */
	status = kdfAlgoIDsize = sizeofAlgoID( kdfAlgo );
	if( cryptStatusError( status ) )
		return( status );

	/* We're using a non-default MAC algorithm, send the custom KDF 
	   parameters to the context:

		CustomParams ::= [ 0 ] SEQUENCE {
			salt			OCTET STRING SIZE(0),
			iterationCount	INTEGER (1),
			prf				AlgorithmIdentifier
			} */
	sMemOpen( &stream, kdfParamData, CRYPT_MAX_TEXTSIZE );
	writeConstructed( &stream, 5 + kdfAlgoIDsize, 0 );
	swrite( &stream, fixedParamData, 5 );
	status = writeAlgoID( &stream, kdfAlgo );
	if( cryptStatusOK( status ) )
		kdfParamDataSize = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Send the encoded parameter information to the generic-secret 
	   context */
	setMessageData( &msgData, kdfParamData, kdfParamDataSize );
	return( krnlSendMessage( iGenericSecret, IMESSAGE_SETATTRIBUTE_S, 
							 &msgData, CRYPT_IATTRIBUTE_KDFPARAMS ) );
	}

CHECK_RETVAL \
static int setAlgoParams( IN_HANDLE const CRYPT_CONTEXT iGenericSecret,
						  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE algorithmParamData[ CRYPT_MAX_TEXTSIZE + 8 ];
	int algorithmParamDataSize DUMMY_INIT, status;

	REQUIRES( isHandleRangeValid( iGenericSecret ) );
	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( attribute == CRYPT_IATTRIBUTE_ENCPARAMS || \
			  attribute == CRYPT_IATTRIBUTE_MACPARAMS );

	/* Get the algorithm parameter data from the encryption or MAC
	   context */
	sMemOpen( &stream, algorithmParamData, CRYPT_MAX_TEXTSIZE );
	if( attribute == CRYPT_IATTRIBUTE_ENCPARAMS )
		status = writeCryptContextAlgoID( &stream, iCryptContext );
	else
		status = writeContextAlgoID( &stream, iCryptContext );
	if( cryptStatusOK( status ) )
		algorithmParamDataSize = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Send the encoded parameter information to the generic-secret 
	   context */
	setMessageData( &msgData, algorithmParamData, algorithmParamDataSize );
	return( krnlSendMessage( iGenericSecret, IMESSAGE_SETATTRIBUTE_S, 
							 &msgData, attribute ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeAuthEncDataHeader( INOUT STREAM *stream,
								   const ENVELOPE_INFO *envelopeInfoPtr )
	{
	CRYPT_CONTEXT iGenericSecret;
	const ACTION_LIST *actionListPtr;
	const BYTE *contentOID = getContentOID( envelopeInfoPtr->contentType );
	long blockedPayloadSize, encrContentInfoSize;
	int macSize = 0, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( contentOID != NULL );

	/* Authenticated encryption derives the encryption and MAC keys from the
	   generic-secret value, with the encryption and MAC algorithm 
	   parameters being provided in the generic-secret's AlgorithmIdentifier
	   value.  In order to work with the generic secret we therefore have to
	   send the optional KDF, encryption and MAC parameter data to the 
	   generic-secret context */
	actionListPtr = findAction( envelopeInfoPtr, ACTION_xxx ); 
	REQUIRES( actionListPtr != NULL );
	REQUIRES( sanityCheckActionList( actionListPtr ) );
	iGenericSecret = actionListPtr->iCryptHandle;
	if( envelopeInfoPtr->defaultMAC != CRYPT_ALGO_HMAC_SHA1 )
		{
		status = setKDFParams( iGenericSecret, envelopeInfoPtr->defaultMAC );
		if( cryptStatusError( status ) )
			return( status );
		}
	actionListPtr = findAction( envelopeInfoPtr, ACTION_CRYPT ); 
	REQUIRES( actionListPtr != NULL );
	REQUIRES( sanityCheckActionList( actionListPtr ) );
	status = setAlgoParams( iGenericSecret, actionListPtr->iCryptHandle,
							CRYPT_IATTRIBUTE_ENCPARAMS );
	if( cryptStatusError( status ) )
		return( status );
	actionListPtr = findAction( envelopeInfoPtr, ACTION_MAC ); 
	REQUIRES( actionListPtr != NULL );
	REQUIRES( sanityCheckActionList( actionListPtr ) );
	status = setAlgoParams( iGenericSecret, actionListPtr->iCryptHandle,
							CRYPT_IATTRIBUTE_MACPARAMS );
	if( cryptStatusError( status ) )
		return( status );

	/* Calculate the size of the payload due to blocking and the ContentInfo
	   header */
	status = getEncryptedContentSize( envelopeInfoPtr, contentOID,
									  sizeofOID( contentOID ), 
									  &blockedPayloadSize, 
									  &encrContentInfoSize );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's definite-length content then we have to determine the size of 
	   the MAC at the end of the data as well */
	if( blockedPayloadSize != CRYPT_UNUSED )
		{
		actionListPtr = findAction( envelopeInfoPtr, ACTION_MAC ); 
		REQUIRES( actionListPtr != NULL );
		REQUIRES( sanityCheckActionList( actionListPtr ) );
		status = krnlSendMessage( actionListPtr->iCryptHandle, 
								  IMESSAGE_GETATTRIBUTE, &macSize, 
								  CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Write the EnvelopedData header and version number and start of the 
	   SET OF RecipientInfo/EncryptionKeyInfo */
	status = writeEncryptionHeader( stream, OID_CMS_AUTHENVDATA, 
						sizeofOID( OID_CMS_AUTHENVDATA ), 0, 
						blockedPayloadSize,
						( envelopeInfoPtr->cryptActionSize == CRYPT_UNUSED ) ? \
							CRYPT_UNUSED : \
							sizeofObject( envelopeInfoPtr->cryptActionSize ) + \
								encrContentInfoSize + \
								sizeofObject( macSize ) );
	if( cryptStatusError( status ) )
		return( status );

	return( ( envelopeInfoPtr->cryptActionSize == CRYPT_UNUSED ) ? \
			writeSetIndef( stream ) : \
			writeSet( stream, envelopeInfoPtr->cryptActionSize ) );
	}

/* CompressedData */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCompressedDataHeader( INOUT STREAM *stream,
									  INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	const BYTE *contentOID = getContentOID( envelopeInfoPtr->contentType );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( contentOID != NULL );

	/* Since compressing the data changes its length we have to use the
	   indefinite-length encoding even if we know how big the payload is */
	envelopeInfoPtr->payloadSize = CRYPT_UNUSED;

	/* Write the CompressedData header, version number, and Zlib algoID */
	status = writeCMSheader( stream, OID_CMS_COMPRESSEDDATA, 
							 sizeofOID( OID_CMS_COMPRESSEDDATA ), 
							 CRYPT_UNUSED, FALSE );
	if( cryptStatusError( status ) )
		return( status );
	writeShortInteger( stream, 0, DEFAULT_TAG );
	writeGenericAlgoID( stream, OID_ZLIB, sizeofOID( OID_ZLIB ) );

	/* Write the inner Data header */
	return( writeCMSheader( stream, contentOID, sizeofOID( contentOID ), 
							CRYPT_UNUSED, TRUE ) );
	}

/****************************************************************************
*																			*
*							Header Processing Routines						*
*																			*
****************************************************************************/

/* Write the envelope header */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeEnvelopeHeader( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	const ACTION_LIST *actionListPtr;
	STREAM stream;
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* If we're encrypting, set up the encryption-related information */
	if( envelopeInfoPtr->usage == ACTION_CRYPT )
		{
		actionListPtr = findAction( envelopeInfoPtr, ACTION_CRYPT );
		ENSURES( actionListPtr != NULL );
		REQUIRES( sanityCheckActionList( actionListPtr ) );
		status = initEnvelopeEncryption( envelopeInfoPtr,
										 actionListPtr->iCryptHandle,
										 CRYPT_ALGO_NONE, CRYPT_MODE_NONE, 
										 NULL, 0, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Write the appropriate CMS header based on the envelope usage.  The
	   DigestedData/ACTION_HASH action is never taken since the higher-level 
	   code assumes that the presence of hash actions indicates the desire 
	   to create signed data and returns an error if no signature actions are 
	   present */
	sMemOpen( &stream, envelopeInfoPtr->buffer, envelopeInfoPtr->bufSize );
	switch( envelopeInfoPtr->usage )
		{
		case ACTION_CRYPT:
			/* If we're using authenticated encryption then we have to use
			   a special-form AuthEnc CMS header even though technically
			   it's just an encrypted envelope */
			if( TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_AUTHENC ) )
				{
				status = writeAuthEncDataHeader( &stream,
												 envelopeInfoPtr );
				break;
				}

			/* It's standard encrypted data */
			if( DATAPTR_ISNULL( envelopeInfoPtr->preActionList ) )
				{
				status = writeEncryptedDataHeader( &stream,
												   envelopeInfoPtr );
				}
			else
				{
				status = writeEnvelopedDataHeader( &stream,
												   envelopeInfoPtr );
				}
			break;

		case ACTION_SIGN:
			status = writeSignedDataHeader( &stream, envelopeInfoPtr, TRUE );
			break;

		case ACTION_HASH:
			status = writeSignedDataHeader( &stream, envelopeInfoPtr, FALSE );
			break;

		case ACTION_COMPRESS:
			status = writeCompressedDataHeader( &stream, envelopeInfoPtr );
			break;

		case ACTION_NONE:
			{
			const BYTE *contentOID = \
							getContentOID( envelopeInfoPtr->contentType );

			REQUIRES( contentOID != NULL );

			status = writeCMSheader( &stream, contentOID, 
									 sizeofOID( contentOID ),
									 envelopeInfoPtr->payloadSize, FALSE );
			break;
			}

		case ACTION_MAC:
			status = writeAuthenticatedDataHeader( &stream, envelopeInfoPtr );
			break;

		default:
			retIntError();
		}
	if( cryptStatusOK( status ) )
		envelopeInfoPtr->bufPos = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're not encrypting with key exchange actions, we're done */
	if( ( envelopeInfoPtr->usage != ACTION_CRYPT && \
		  envelopeInfoPtr->usage != ACTION_MAC ) || \
		DATAPTR_ISNULL( envelopeInfoPtr->preActionList ) )
		{
		/* Set the block size mask to all ones if we're not encrypting since 
		   we can begin and end data segments on arbitrary boundaries, and 
		   inform the caller that we're done */
		if( envelopeInfoPtr->usage != ACTION_CRYPT )
			envelopeInfoPtr->blockSizeMask = ~0;
		DATAPTR_SET( envelopeInfoPtr->lastAction, NULL );

		return( OK_SPECIAL );
		}

	/* Start emitting the key exchange actions */
	actionListPtr = findPreAction( envelopeInfoPtr, ACTION_KEYEXCHANGE_PKC );
	if( actionListPtr == NULL )
		actionListPtr = findPreAction( envelopeInfoPtr, ACTION_KEYEXCHANGE );
	ENSURES( actionListPtr != NULL );
	REQUIRES( sanityCheckActionList( actionListPtr ) );
	DATAPTR_SET( envelopeInfoPtr->lastAction, 
				 ( ACTION_LIST * ) actionListPtr );

	return( CRYPT_OK );
	}

/* Write key exchange actions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeKeyex( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	CRYPT_CONTEXT iCryptContext;
	ACTION_LIST *actionListPtr;
	int status, LOOP_ITERATOR;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Get the appropriate encryption, MAC, or generic-secret context to 
	   export via the keyex actions */
	status = getActionContext( envelopeInfoPtr, &iCryptContext );
	if( cryptStatusError( status ) )
		return( status );

	/* Export the session key/MAC using each of the PKC or conventional 
	   keys.  If it's a conventional key exchange we force the use of the 
	   CMS format since there's no reason to use the cryptlib format */
	LOOP_MED( actionListPtr = DATAPTR_GET( envelopeInfoPtr->lastAction ), 
			  actionListPtr != NULL,
			  actionListPtr = DATAPTR_GET( actionListPtr->next ) )
		{
		const int dataLeft = min( envelopeInfoPtr->bufSize - \
								  envelopeInfoPtr->bufPos, 
								  MAX_INTLENGTH_SHORT - 1 );
		int keyexSize;

		ENSURES( isShortIntegerRange( dataLeft ) );
		REQUIRES( sanityCheckActionList( actionListPtr ) );

		/* Make sure that there's enough room to emit this key exchange 
		   action */
		if( actionListPtr->encodedSize + 128 > dataLeft )
			{
			status = CRYPT_ERROR_OVERFLOW;
			break;
			}

		/* Emit the key exchange action */
		status = iCryptExportKey( envelopeInfoPtr->buffer + \
						envelopeInfoPtr->bufPos, dataLeft, &keyexSize, 
						( actionListPtr->action == ACTION_KEYEXCHANGE ) ? \
						  CRYPT_FORMAT_CMS : envelopeInfoPtr->type,
						iCryptContext, actionListPtr->iCryptHandle );
		if( cryptStatusError( status ) )
			break;
		envelopeInfoPtr->bufPos += keyexSize;
		}
	ENSURES( LOOP_BOUND_OK );
	DATAPTR_SET( envelopeInfoPtr->lastAction, actionListPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's an indefinite-length header, close off the set of key 
	   exchange actions */
	if( envelopeInfoPtr->cryptActionSize == CRYPT_UNUSED )
		return( writeEOCs( envelopeInfoPtr, 1 ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Trailer Processing Routines						*
*																			*
****************************************************************************/

/* Write the signing certificate chain.  This can grow arbitrarily large and 
   in particular can become larger than the main envelope buffer if multiple 
   signatures with long chains and a small envelope buffer are used, so we 
   emit the certificate chain into a dynamically-allocated auxiliary buffer 
   if there isn't enough room to emit it into the main buffer  */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeCertchainTrailer( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	STREAM stream;
	void *certChainBufPtr;
	const int dataLeft = min( envelopeInfoPtr->bufSize - \
							  envelopeInfoPtr->bufPos, 
							  MAX_INTLENGTH_SHORT - 1 );
	const int eocSize = ( envelopeInfoPtr->payloadSize == CRYPT_UNUSED ) ? \
						( 3 * sizeofEOC() ) : 0;
	int certChainBufSize, certChainSize DUMMY_INIT, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	ENSURES( isShortIntegerRange( dataLeft ) );

	/* Check whether there's enough room left in the buffer to emit the 
	   signing certificate chain directly into it */
	if( envelopeInfoPtr->extraDataSize + 64 < dataLeft )
		{
		/* The certificate chain will fit into the envelope buffer */
		certChainBufPtr = envelopeInfoPtr->buffer + \
						  envelopeInfoPtr->bufPos + eocSize;
		certChainBufSize = dataLeft - eocSize;
		}
	else
		{
		/* If there's almost no room left in the buffer anyway tell the 
		   caller that they have to pop some data before they can continue.  
		   Hopefully this will create enough room to emit the certificates 
		   directly into the buffer */
		if( dataLeft < 1024 )
			return( CRYPT_ERROR_OVERFLOW );

		/* We can't emit the certificates directly into the envelope buffer, 
		   allocate an auxiliary buffer for them and from there copy them 
		   into the main buffer */
		REQUIRES( envelopeInfoPtr->auxBuffer == NULL );
		if( ( envelopeInfoPtr->auxBuffer = \
				clDynAlloc( "emitPostamble",
							envelopeInfoPtr->extraDataSize + 64 ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		certChainBufPtr = envelopeInfoPtr->auxBuffer;
		certChainBufSize = envelopeInfoPtr->auxBufSize = \
									envelopeInfoPtr->extraDataSize + 64;
		}

	/* Write the end-of-contents octets for the Data OCTET STRING, [0], and 
	   SEQUENCE if necessary */
	if( envelopeInfoPtr->payloadSize == CRYPT_UNUSED )
		{
		status = writeEOCs( envelopeInfoPtr, 3 );
		if( cryptStatusError( status ) )
			return( status );
		}
	envelopeInfoPtr->lastAction = envelopeInfoPtr->postActionList;

	/* Write the signing certificate chain if it's a CMS signature and 
	   they're not explicitly excluded, followed by the SET OF SignerInfo 
	   header */
	sMemOpen( &stream, certChainBufPtr, certChainBufSize );
	if( ( envelopeInfoPtr->type == CRYPT_FORMAT_CMS || \
		  envelopeInfoPtr->type == CRYPT_FORMAT_SMIME ) && \
		!TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_NOSIGNINGCERTS ) )
		{
		const ACTION_LIST *lastActionPtr;

		lastActionPtr = DATAPTR_GET( envelopeInfoPtr->lastAction );
		ENSURES( lastActionPtr != NULL );
		REQUIRES( sanityCheckActionList( lastActionPtr ) );
		status = exportCertToStream( &stream,
							( envelopeInfoPtr->iExtraCertChain != CRYPT_ERROR ) ? \
							  envelopeInfoPtr->iExtraCertChain : \
							  lastActionPtr->iCryptHandle,
							CRYPT_ICERTFORMAT_CERTSET );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		}
	if( TEST_FLAG( envelopeInfoPtr->dataFlags, 
				   ENVDATA_FLAG_HASINDEFTRAILER ) )
		status = writeSetIndef( &stream );
	else
		status = writeSet( &stream, envelopeInfoPtr->signActionSize );
	if( cryptStatusOK( status ) )
		certChainSize = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're copying data via the auxBuffer flush as much as we can into 
	   the main buffer.  If we can't copy it all in, resulting in an overflow
	   error, we use the OK_SPECIAL status to tell the caller that although
	   an overflow occurred it was due to the auxBuffer copy and not the
	   certificate chain write and it's OK to move on to the next state */
	if( envelopeInfoPtr->auxBufSize > 0 )
		{
		envelopeInfoPtr->auxBufPos = certChainSize;
		status = copyFromAuxBuffer( envelopeInfoPtr );
		return( ( status == CRYPT_ERROR_OVERFLOW ) ? OK_SPECIAL : status );
		}

	/* Since we're in the post-data state any necessary payload data 
	   segmentation has been completed, however the caller can't copy out 
	   any post-payload data because it's past the end-of-segment position.  
	   In order to allow the buffer to be emptied to make room for signature 
	   data we set the end-of-segment position to the end of the new data */
	envelopeInfoPtr->bufPos += certChainSize;
	envelopeInfoPtr->segmentDataEnd = envelopeInfoPtr->bufPos;

	return( CRYPT_OK );
	}

/* Write signatures */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeSignatures( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	ACTION_LIST *actionListPtr;
	int noSigs = 0, status = CRYPT_OK, LOOP_ITERATOR;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Sign each hash using the associated signature key */
	LOOP_MED( actionListPtr = DATAPTR_GET( envelopeInfoPtr->lastAction ), 
			  actionListPtr != NULL,
			  actionListPtr = DATAPTR_GET( actionListPtr->next ) )
		{
		ACTION_LIST *associatedActionPtr;
		SIGPARAMS sigParams;
		const int sigBufSize = min( envelopeInfoPtr->bufSize - \
									envelopeInfoPtr->bufPos, \
									MAX_INTLENGTH_SHORT - 1 );
		int sigSize;

		REQUIRES( sanityCheckActionList( actionListPtr ) );
		REQUIRES( actionListPtr->action == ACTION_SIGN );
		ENSURES( isShortIntegerRange( sigBufSize ) );

		/* Check whether there's enough room left in the buffer to emit the
		   signature directly into it.  Since signatures are fairly small (a
		   few hundred bytes) we always require enough room in the buffer
		   and don't bother with any overflow handling via the auxBuffer */
		if( actionListPtr->encodedSize + 64 > sigBufSize )
			{
			status = CRYPT_ERROR_OVERFLOW;
			break;
			}

		/* Set up any necessary signature parameters such as signature 
		   attributes and timestamps if necessary */
		status = cmsInitSigParams( actionListPtr, envelopeInfoPtr->type, 
								   envelopeInfoPtr->ownerHandle, 
								   &sigParams );
		if( cryptStatusError( status ) )
			break;

		/* Sign the data */
		associatedActionPtr = DATAPTR_GET( actionListPtr->associatedAction );
		REQUIRES( associatedActionPtr != NULL );
		status = iCryptCreateSignature( envelopeInfoPtr->buffer + \
										envelopeInfoPtr->bufPos, sigBufSize, 
							&sigSize, envelopeInfoPtr->type,
							actionListPtr->iCryptHandle,
							associatedActionPtr->iCryptHandle,
							( envelopeInfoPtr->type == CRYPT_FORMAT_CRYPTLIB ) ? \
								NULL : &sigParams );
		if( cryptStatusError( status ) )
			break;

		/* Since the timestamping process can take awhile, we re-verify the
		   envelopeInfoPtr and actionListPtr if there's a TSP involved */
		REQUIRES( sigParams.iTspSession == CRYPT_ERROR || \
				  ( sanityCheckEnvCMSEnv( envelopeInfoPtr ) && \
					sanityCheckActionList( actionListPtr ) ) );

		envelopeInfoPtr->bufPos += sigSize;
		noSigs++;
		}
	ENSURES( LOOP_BOUND_OK );
	DATAPTR_SET( envelopeInfoPtr->lastAction, actionListPtr );

	/* The possibilities for problems when creating a signature are complex 
	   enough that we provide special-case reporting for specific types of
	   problems.  In particular we pull up lower-level information from 
	   signature-creation related objects if they're being used, and if 
	   there are multiple signatures being created we identify the 
	   individual signature that caused the problem */
	if( cryptStatusError( status ) )
		{
		if( actionListPtr->iTspSession != CRYPT_ERROR )
			{
			retExtObj( status, 
					   ( status, ENVELOPE_ERRINFO,
					     actionListPtr->iTspSession,
						 "Couldn't emit signed timestamp to envelope "
						 "trailer" ) );
			}
		if( noSigs <= 0 )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't emit signature to envelope trailer" ) );
			}
		retExt( status,
				( status, ENVELOPE_ERRINFO,
				  "Couldn't emit signature #%d to envelope trailer",
				  noSigs + 1 ) );
		}

	return( CRYPT_OK );
	}

/* Write MAC value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeMAC( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	const ACTION_LIST *actionListPtr;
	STREAM stream;
	MESSAGE_DATA msgData;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	const int eocSize = ( envelopeInfoPtr->payloadSize == CRYPT_UNUSED ) ? \
						( 3 * sizeofEOC() ) : 0;
	const int dataLeft = min( envelopeInfoPtr->bufSize - \
							  envelopeInfoPtr->bufPos, 512 );
	int length DUMMY_INIT, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Make sure that there's room for the MAC data in the buffer */
	if( dataLeft < eocSize + sizeofObject( CRYPT_MAX_HASHSIZE ) )
		return( CRYPT_ERROR_OVERFLOW );
	INJECT_FAULT( ENVELOPE_CMS_CORRUPT_AUTH_DATA, 
				  ENVELOPE_CMS_CORRUPT_AUTH_DATA_1 );

	/* Write the end-of-contents octets for the Data OCTET STRING, [0], and 
	   SEQUENCE if necessary */
	if( envelopeInfoPtr->payloadSize == CRYPT_UNUSED )
		{
		status = writeEOCs( envelopeInfoPtr, 3 );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Get the MAC value and write it to the buffer */
	actionListPtr = findAction( envelopeInfoPtr, ACTION_MAC );
	ENSURES( actionListPtr != NULL );
	REQUIRES( sanityCheckActionList( actionListPtr ) );
	setMessageData( &msgData, hash, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( actionListPtr->iCryptHandle,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );
	INJECT_FAULT( ENVELOPE_CMS_CORRUPT_AUTH_MAC, 
				  ENVELOPE_CMS_CORRUPT_AUTH_MAC_1 );
	sMemOpen( &stream, envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos, 
			  dataLeft );
	status = writeOctetString( &stream, hash, msgData.length, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		length = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	envelopeInfoPtr->bufPos += length;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Emit Envelope Preamble/Postamble				*
*																			*
****************************************************************************/

/* Output as much of the preamble as possible into the envelope buffer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int emitPreamble( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	int status = CRYPT_OK;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheckEnvCMSEnv( envelopeInfoPtr ) );

	/* If we've finished processing the header information, don't do
	   anything */
	if( envelopeInfoPtr->envState == ENVSTATE_DONE )
		return( CRYPT_OK );

	/* If we haven't started doing anything yet perform various final
	   initialisations */
	if( envelopeInfoPtr->envState == ENVSTATE_NONE )
		{
		/* If there's no nested content type set, default to plain data */
		if( envelopeInfoPtr->contentType == CRYPT_CONTENT_NONE )
			envelopeInfoPtr->contentType = CRYPT_CONTENT_DATA;

		/* If there's an absolute data length set, remember it for when we
		   copy in data */
		if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED )
			envelopeInfoPtr->segmentSize = envelopeInfoPtr->payloadSize;

		/* Perform any remaining initialisation.  MAC'd data is a special-
		   case form of encrypted data so we treat them as the same thing
		   at the key exchange level */
		if( envelopeInfoPtr->usage == ACTION_CRYPT || \
			envelopeInfoPtr->usage == ACTION_MAC )
			status = cmsPreEnvelopeEncrypt( envelopeInfoPtr );
		else
			{
			if( envelopeInfoPtr->usage == ACTION_SIGN )
				status = cmsPreEnvelopeSign( envelopeInfoPtr );
			}
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't perform final %s initialisation prior to "
					  "enveloping data", 
					  ( envelopeInfoPtr->usage == ACTION_SIGN ) ? \
						"signing" : "encryption" ) );
			}

		/* Delete any orphaned actions such as automatically-added hash
		   actions that were overridden with user-supplied alternate
		   actions */
		status = deleteUnusedActions( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );

		/* Make sure that we start a new segment when we add the first lot
		   of payload data after we've emitted the header info */
		SET_FLAG( envelopeInfoPtr->dataFlags, ENVDATA_FLAG_SEGMENTCOMPLETE );

		/* We're ready to go, prepare to emit the outer header */
		envelopeInfoPtr->envState = ENVSTATE_HEADER;
		
		ENSURES( checkActions( envelopeInfoPtr ) );
		}

	/* Emit the outer header.  This always follows directly from the final
	   initialisation step but we keep the two logically distinct to 
	   emphasise the fact that the former is merely finalising enveloping 
	   actions without performing any header processing while the latter is 
	   the first stage that actually emits header data */
	if( envelopeInfoPtr->envState == ENVSTATE_HEADER )
		{
		status = writeEnvelopeHeader( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			{
			/* If there's nothing else to emit, we're done */
			if( status == OK_SPECIAL )
				{
				envelopeInfoPtr->envState = ENVSTATE_DONE;

				ENSURES( sanityCheckEnvCMSEnv( envelopeInfoPtr ) );

				return( CRYPT_OK );
				}

			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't create envelope header" ) );
			}

		/* Move on to the next state */
		envelopeInfoPtr->envState = ENVSTATE_KEYINFO;
		}

	/* Handle key export actions */
	if( envelopeInfoPtr->envState == ENVSTATE_KEYINFO )
		{
		status = writeKeyex( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't emit key exchange actions to envelope "
					  "header" ) );
			}

		/* Move on to the next state */
		envelopeInfoPtr->envState = ENVSTATE_ENCRINFO;
		}

	/* Handle encrypted content information */
	if( envelopeInfoPtr->envState == ENVSTATE_ENCRINFO )
		{
		const ACTION_LIST *actionListPtr;
		STREAM stream;
		const BYTE *contentOID = getContentOID( envelopeInfoPtr->contentType );
		const int originalBufPos = envelopeInfoPtr->bufPos;
		const int dataLeft = min( envelopeInfoPtr->bufSize - \
													envelopeInfoPtr->bufPos, \
								  MAX_INTLENGTH_SHORT - 1 );

		REQUIRES( contentOID != NULL );
		REQUIRES( isShortIntegerRangeNZ( dataLeft ) );

		/* Make sure that there's enough room to emit the data header.  The
		   value used is only approximate, if there's not enough room left
		   the write will also return an overflow error */
		if( dataLeft < 256 )
			return( CRYPT_ERROR_OVERFLOW );

		/* Write the encrypted content header */
		sMemOpen( &stream, envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos,
				  dataLeft );
		if( envelopeInfoPtr->usage == ACTION_MAC )
			{
			actionListPtr = DATAPTR_GET( envelopeInfoPtr->actionList );

			ENSURES( actionListPtr != NULL );
			REQUIRES( sanityCheckActionList( actionListPtr ) );

			/* If it's authenticated data, there's a MAC algorithm ID 
			   preceding standard EncapContent */
			status = writeContextAlgoID( &stream, 
										 actionListPtr->iCryptHandle );
			if( cryptStatusOK ( status ) )
				{
				status = writeCMSheader( &stream, contentOID, 
										 sizeofOID( contentOID ),
										 envelopeInfoPtr->payloadSize, 
										 TRUE );
				}
			}
		else
			{
			CRYPT_CONTEXT iCryptContext;

			/* It's encrypted data, it's EncrContent */
			status = getActionContext( envelopeInfoPtr, &iCryptContext );
			if( cryptStatusError( status ) )
				return( status );
			status = writeEncryptedContentHeader( &stream, contentOID,
									sizeofOID( contentOID ), iCryptContext, 
									envelopeInfoPtr->payloadSize, 
									envelopeInfoPtr->blockSize );
			}
		if( cryptStatusOK( status ) )
			envelopeInfoPtr->bufPos += stell( &stream );
		sMemDisconnect( &stream );
		if( cryptStatusOK( status ) && \
			TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_AUTHENC ) )
			{
			const void *macData DUMMY_INIT_PTR;
			int macDataLength DUMMY_INIT;

			/* Get the MAC action that we'll be using to protect the 
			   content */
			actionListPtr = findAction( envelopeInfoPtr, ACTION_MAC );
			ENSURES( actionListPtr != NULL );
			REQUIRES( sanityCheckActionList( actionListPtr ) );

			/* For AuthEnc data we have to MAC the 
			   EncryptedContentInfo.ContentEncryptionAlgorithmIdentifier 
			   information alongside the payload data to stop an attacker 
			   from manipulating the algorithm parameters to cause 
			   corruption that won't be detected by the MAC on the payload 
			   data.  This requires digging down into the encrypted content
			   header to locate the AlgoID data and MACing that */
			sMemConnect( &stream, envelopeInfoPtr->buffer + originalBufPos,
						 envelopeInfoPtr->bufPos - originalBufPos );
			readLongSequence( &stream, NULL );	/* Outer encapsulation */
			status = readUniversal( &stream );	/* Content-type OID */
			if( cryptStatusOK( status ) )
				status = getStreamObjectLength( &stream, &macDataLength );
			if( cryptStatusOK( status ) )		/* AlgoID */
				{
				status = sMemGetDataBlock( &stream, ( void ** ) &macData, 
										   macDataLength );
				}
			if( cryptStatusOK( status ) )
				{
				status = krnlSendMessage( actionListPtr->iCryptHandle,
										  IMESSAGE_CTX_HASH, 
										  ( MESSAGE_CAST ) macData, 
										  macDataLength );
				}
			sMemDisconnect( &stream );
			}
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't emit encrypted content header to envelope "
					  "header" ) );
			}

		/* We're done */
		envelopeInfoPtr->envState = ENVSTATE_DONE;
		}

	ENSURES( sanityCheckEnvCMSEnv( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/* Output as much of the postamble as possible into the envelope buffer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int emitPostamble( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						  STDC_UNUSED const BOOLEAN dummy )
	{
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheckEnvCMSEnv( envelopeInfoPtr ) );

	/* Before we can emit the trailer we need to flush any remaining data
	   from internal buffers */
	if( envelopeInfoPtr->envState == ENVSTATE_NONE )
		{
		const ENV_COPYTOENVELOPE_FUNCTION copyToEnvelopeFunction = \
					( ENV_COPYTOENVELOPE_FUNCTION ) \
					FNPTR_GET( envelopeInfoPtr->copyToEnvelopeFunction );

		REQUIRES( copyToEnvelopeFunction != NULL );

		status = copyToEnvelopeFunction( envelopeInfoPtr, NULL, 0 );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't flush remaining data into envelope "
					  "buffer" ) );
			}
		envelopeInfoPtr->envState = \
					( envelopeInfoPtr->usage == ACTION_SIGN ) ? \
					ENVSTATE_FLUSHED : ENVSTATE_SIGNATURE;
		INJECT_FAULT( BADSIG_DATA, BADSIG_DATA_CMS_1 );
		}

	/* The only message type that has a trailer is signed or authenticated 
	   data so if we're not signing/authenticating data we can exit now */
	if( !( envelopeInfoPtr->usage == ACTION_SIGN || \
		   envelopeInfoPtr->usage == ACTION_MAC || \
		   ( envelopeInfoPtr->usage == ACTION_CRYPT && \
			 TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_AUTHENC ) ) ) )
		{
		/* Emit the various end-of-contents octets if necessary */
		if( envelopeInfoPtr->payloadSize == CRYPT_UNUSED || \
			( envelopeInfoPtr->usage == ACTION_CRYPT &&
			  envelopeInfoPtr->cryptActionSize == CRYPT_UNUSED ) )
			{
			/* Write the end-of-contents octets for the encapsulated data if
			   necessary.  Normally we have two EOCs, however compressed 
			   data requires an extra one due to the explicit tagging */
			if( envelopeInfoPtr->payloadSize == CRYPT_UNUSED && \
				( envelopeInfoPtr->usage == ACTION_CRYPT || \
				  envelopeInfoPtr->usage == ACTION_COMPRESS ) )
				{
				status = writeEOCs( envelopeInfoPtr, 3 + \
									( ( envelopeInfoPtr->usage == \
										ACTION_COMPRESS ) ? \
									  3 : 2 ) );
				}
			else
				{
				/* Write the remaining end-of-contents octets for the OCTET
				   STRING/SEQUENCE, [0], and SEQUENCE */
				status = writeEOCs( envelopeInfoPtr, 3 );
				}
			if( cryptStatusError( status ) )
				{
				retExt( status,
						( status, ENVELOPE_ERRINFO,
						  "Couldn't emit final EOC octets" ) );
				}
			}

		/* Now that we've written the final end-of-contents octets, set the end-
		   of-segment-data pointer to the end of the data in the buffer so that
		   copyFromEnvelope() can copy out the remaining data */
		envelopeInfoPtr->segmentDataEnd = envelopeInfoPtr->bufPos;
		envelopeInfoPtr->envState = ENVSTATE_DONE;

		ENSURES( sanityCheckEnvCMSEnv( envelopeInfoPtr ) );

		return( CRYPT_OK );
		}

	/* If there's any signature data left in the auxiliary buffer try and 
	   empty that first */
	if( envelopeInfoPtr->auxBufPos > 0 )
		{
		status = copyFromAuxBuffer( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't flush remaining signature data into "
					  "envelope buffer" ) );
			}
		}

	/* Handle signing certificate chain */
	if( envelopeInfoPtr->envState == ENVSTATE_FLUSHED )
		{
		status = writeCertchainTrailer( envelopeInfoPtr );
		if( cryptStatusError( status ) && status != OK_SPECIAL )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't emit certificate chain to envelope "
					  "trailer" ) );
			}

		/* Move on to the next state */
		envelopeInfoPtr->envState = ENVSTATE_SIGNATURE;

		/* If we were writing the certificate chain using the auxBuffer as 
		   an intermediate stage because there wasn't enough room to 
		   assemble the complete chain in the main buffer and we then got an
		   overflow error moving the data out into the main buffer we have 
		   to resume later in the signature state */
		if( status == OK_SPECIAL )
			return( CRYPT_ERROR_OVERFLOW );
		}

	/* Handle signing actions */
	REQUIRES( envelopeInfoPtr->envState == ENVSTATE_SIGNATURE );

	/* Write the signatures/MACs.  The process of writing signatures is 
	   complex enough that the function itself sets the extended error
	   information */
	if( envelopeInfoPtr->usage == ACTION_SIGN )
		{
		status = writeSignatures( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		status = writeMAC( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't emit MAC to envelope trailer" ) );
			}
		}

	/* Write the end-of-contents octets for the OCTET STRING/SEQUENCE, [0],
	   and SEQUENCE if necessary.  If the trailer has an indefinite length
	   then we need to add an EOC for the trailer as well */
	if( envelopeInfoPtr->payloadSize == CRYPT_UNUSED || \
		TEST_FLAG( envelopeInfoPtr->dataFlags, 
				   ENVDATA_FLAG_HASINDEFTRAILER ) )
		{
		status = writeEOCs( envelopeInfoPtr,
							3 + \
							( TEST_FLAG( envelopeInfoPtr->dataFlags,
										 ENVDATA_FLAG_HASINDEFTRAILER ) ? \
							  1 : 0 ) );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't emit final EOC octets" ) );
			}
		}

	/* Now that we've written the final end-of-contents octets set the end-
	   of-segment-data pointer to the end of the data in the buffer so that
	   copyFromEnvelope() can copy out the remaining data */
	envelopeInfoPtr->segmentDataEnd = envelopeInfoPtr->bufPos;
	envelopeInfoPtr->envState = ENVSTATE_DONE;

	ENSURES( sanityCheckEnvCMSEnv( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Envelope Access Routines						*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initCMSEnveloping( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	int algorithm, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	
	REQUIRES_V( !TEST_FLAG( envelopeInfoPtr->flags, 
							ENVELOPE_FLAG_ISDEENVELOPE ) );

	/* Set the access method pointers */
	FNPTR_SET( envelopeInfoPtr->processPreambleFunction, emitPreamble );
	FNPTR_SET( envelopeInfoPtr->processPostambleFunction, emitPostamble );
	FNPTR_SET( envelopeInfoPtr->checkAlgoFunction, cmsCheckAlgo );

	/* Set up the processing state information */
	envelopeInfoPtr->envState = ENVSTATE_NONE;

	/* Remember the current default settings for use with the envelope. 
	   We force the use of the CBC encryption mode because this is the 
	   safest and most efficient encryption mode, and the only mode defined 
	   for many CMS algorithms.  Since the CMS algorithms represent only a 
	   subset of what's available we have to drop back to fixed values if 
	   the caller has selected something exotic */
	status = krnlSendMessage( envelopeInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &algorithm, 
							  CRYPT_OPTION_ENCR_HASH );
	if( cryptStatusError( status ) || \
		!checkAlgoID( algorithm, CRYPT_MODE_NONE ) )
		envelopeInfoPtr->defaultHash = CRYPT_ALGO_SHA2;
	else
		envelopeInfoPtr->defaultHash = algorithm;	/* int vs.enum */
	status = krnlSendMessage( envelopeInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &algorithm, 
							  CRYPT_OPTION_ENCR_ALGO );
	if( cryptStatusError( status ) || \
		!checkAlgoID( algorithm, ( algorithm == CRYPT_ALGO_RC4 ) ? \
								 CRYPT_PSEUDOMODE_RC4 : CRYPT_MODE_CBC ) )
		envelopeInfoPtr->defaultAlgo = CRYPT_ALGO_AES;
	else
		envelopeInfoPtr->defaultAlgo = algorithm;	/* int vs.enum */
	status = krnlSendMessage( envelopeInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &algorithm, 
							  CRYPT_OPTION_ENCR_MAC );
	if( cryptStatusError( status ) || \
		!checkAlgoID( algorithm, CRYPT_MODE_NONE ) )
		envelopeInfoPtr->defaultMAC = CRYPT_ALGO_HMAC_SHA2;
	else
		envelopeInfoPtr->defaultMAC = algorithm;	/* int vs.enum */
	}
#endif /* USE_CMS */
