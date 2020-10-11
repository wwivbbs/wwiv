/****************************************************************************
*																			*
*					ASN.1 Supplemental Read/Write Routines					*
*						Copyright Peter Gutmann 1992-2014					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_INT_ASN1

/****************************************************************************
*																			*
*							Message Digest Routines							*
*																			*
****************************************************************************/

/* Read/write a message digest value.  This is another one of those oddball
   functions which is present here because it's the least inappropriate place
   to put it */

CHECK_RETVAL_LENGTH_SHORT \
int sizeofMessageDigest( IN_ALGO const CRYPT_ALGO_TYPE hashAlgo, 
						 IN_LENGTH_HASH const int hashSize )
	{
	int algoInfoSize, hashInfoSize, status;

	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashSize >= MIN_HASHSIZE && hashSize <= CRYPT_MAX_HASHSIZE );

	if( isParameterisedHashAlgo( hashAlgo ) )
		status = algoInfoSize = sizeofAlgoIDex( hashAlgo, hashSize );
	else
		status = algoInfoSize = sizeofAlgoID( hashAlgo );
	ENSURES( !cryptStatusError( status ) );
	hashInfoSize = sizeofObject( hashSize );
	ENSURES( algoInfoSize > 8 && algoInfoSize < MAX_INTLENGTH_SHORT );
	ENSURES( hashInfoSize > hashSize && hashInfoSize < MAX_INTLENGTH_SHORT );

	return( sizeofObject( algoInfoSize + hashInfoSize ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int writeMessageDigest( INOUT STREAM *stream, 
						IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
						IN_BUFFER( hashSize ) const void *hash, 
						IN_LENGTH_HASH const int hashSize )
	{
	int algoInfoSize, status;
	
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( hash, hashSize ) );

	REQUIRES_S( isHashAlgo( hashAlgo ) );
	REQUIRES_S( hashSize >= MIN_HASHSIZE && hashSize <= CRYPT_MAX_HASHSIZE );

	if( isParameterisedHashAlgo( hashAlgo ) )
		{
		status = algoInfoSize = sizeofAlgoIDex( hashAlgo, hashSize );
		ENSURES( !cryptStatusError( status ) );
		writeSequence( stream, algoInfoSize + sizeofShortObject( hashSize ) );
		status = writeAlgoIDex( stream, hashAlgo, hashSize, 0 );
		}
	else
		{
		status = algoInfoSize = sizeofAlgoID( hashAlgo );
		ENSURES( !cryptStatusError( status ) );
		writeSequence( stream, algoInfoSize + sizeofShortObject( hashSize ) );
		status = writeAlgoID( stream, hashAlgo );
		}
	if( cryptStatusOK( status ) )
		status = writeOctetString( stream, hash, hashSize, DEFAULT_TAG );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int readMessageDigest( INOUT STREAM *stream, 
					   OUT_ALGO_Z CRYPT_ALGO_TYPE *hashAlgo,
					   OUT_BUFFER( hashMaxLen, *hashSize ) void *hash, 
					   IN_LENGTH_HASH const int hashMaxLen, 
					   OUT_LENGTH_BOUNDED_Z( hashMaxLen ) int *hashSize )
	{
	int hashAlgoSize DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( hashAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( isWritePtrDynamic( hash, hashMaxLen ) );
	assert( isWritePtr( hashSize, sizeof( int ) ) );

	REQUIRES_S( hashMaxLen >= MIN_HASHSIZE && hashMaxLen <= 8192 );

	/* Clear the return values */
	memset( hash, 0, min( 16, hashMaxLen ) );
	*hashSize = 0;

	/* Read the message digest, enforcing sensible size values */
	status = readSequence( stream, NULL );
	if( cryptStatusOK( status ) )
		{
		status = readAlgoIDex( stream, hashAlgo, NULL, &hashAlgoSize, 
							   ALGOID_CLASS_HASH );
		}
	if( cryptStatusOK( status ) )
		{
		status = readOctetString( stream, hash, hashSize, MIN_HASHSIZE, 
								  hashMaxLen );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a parameterised hash algorithm, make sure that the amount of 
	   hash data matches the algorithm parameter */
	if( hashAlgoSize != 0 && hashAlgoSize != *hashSize )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								CMS Header Routines							*
*																			*
****************************************************************************/

/* Read and write CMS headers.  When reading CMS headers we check a bit more
   than just the header OID, which means that we need to provide additional
   information alongside the OID information.  This is provided as
   CMS_CONTENT_INFO in the OID info extra data field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readCMSheader( INOUT STREAM *stream, 
				   IN_ARRAY( noOidInfoEntries ) const OID_INFO *oidInfo, 
				   IN_RANGE( 1, 50 ) const int noOidInfoEntries, 
				   OUT_OPT_INT_Z int *selectionID,
				   OUT_OPT_LENGTH_INDEF long *dataSize, 
				   IN_FLAGS_Z( READCMS ) const int flags )
	{
	const OID_INFO *oidInfoPtr;
	BOOLEAN isData = FALSE, isDetachedSig = FALSE;
	long savedLength = CRYPT_UNUSED, savedLengthDataStart DUMMY_INIT;
	long length, value;
	int tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( oidInfo, \
							  sizeof( OID_INFO ) * noOidInfoEntries ) );
	assert( dataSize == NULL || isWritePtr( dataSize, sizeof( long ) ) );

	REQUIRES_S( noOidInfoEntries > 0 && noOidInfoEntries <= 50 );
	REQUIRES_S( isFlagRangeZ( flags, READCMS ) );
	REQUIRES_S( !( ( flags & ( READCMS_FLAG_DEFINITELENGTH | \
							   READCMS_FLAG_DEFINITELENGTH_OPT ) ) && \
				   ( dataSize == NULL ) ) );
	REQUIRES_S( !( ( flags & READCMS_FLAG_WRAPPERONLY ) && \
				   ( oidInfo[ 0 ].extraInfo != NULL ) ) );
	REQUIRES_S( !( flags & READCMS_FLAG_AUTHENC ) );

	/* Clear return values */
	if( selectionID != NULL )
		*selectionID = 0;
	if( dataSize != NULL )
		*dataSize = 0;

	/* Read the outer SEQUENCE and OID.  We can't use a normal
	   readSequence() here because the data length could be much longer than
	   the maximum allowed in the readSequence() sanity check */
	status = readLongSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length != CRYPT_UNUSED )
		{
		savedLength = length;
		savedLengthDataStart = stell( stream );
		}
	status = readOIDEx( stream, oidInfo, noOidInfoEntries, &oidInfoPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* If the content type is data then the content is an OCTET STRING 
	   rather than a SEQUENCE so we remember the type for later.  Since 
	   there are a pile of CMS OIDs of the same length as OID_CMS_DATA, we 
	   check for a match on the last byte before we perform a full OID 
	   match */
	static_assert_opt( sizeofOID( OID_CMS_DATA ) == 11, \
					   "Data OID size" );
	if( sizeofOID( oidInfoPtr->oid ) == sizeofOID( OID_CMS_DATA ) && \
		oidInfoPtr->oid[ 10 ] == OID_CMS_DATA[ 10 ] && \
		!memcmp( oidInfoPtr->oid, OID_CMS_DATA, \
				 sizeofOID( OID_CMS_DATA ) ) )
		{
		isData = TRUE;
		}

	/* Check for the special-case situation of a detached signature, for 
	   which the the total content consists only of the OID, which means 
	   that the overall object is SEQUENCE { OID data } */
	if( length != CRYPT_UNUSED )
		{
		if( length <= sizeofOID( oidInfoPtr->oid ) )
			{
			if( length != sizeofOID( oidInfoPtr->oid ) )
				return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			isDetachedSig = TRUE;
			}
		}
	else
		{
		/* Some Microsoft software produces an indefinite encoding for a 
		   single OID so we have to check for this */
		status = checkEOC( stream );
		if( cryptStatusError( status ) )
			return( status );
		if( status == TRUE )
			isDetachedSig = TRUE;
		}
	if( isDetachedSig )
		{
		/* It appears to be a detached signature, make sure that the 
		   requirements are met, namely that it's data content and present
		   in an inner header */
		if( !( isData && ( flags & READCMS_FLAG_INNERHEADER ) ) )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

		/* It's a detached signature, we're done */
		if( selectionID != NULL )
			*selectionID = oidInfoPtr->selectionID;
		return( CRYPT_OK );
		}

	/* Read the content [0] tag and OCTET STRING/SEQUENCE.  This requires
	   some special-case handling, see the comment in writeCMSHeader() for
	   more details */
	status = readLongConstructed( stream, &length, 0 );
	if( cryptStatusError( status ) )
		return( status );
	if( length != CRYPT_UNUSED )
		{
		savedLength = length;
		savedLengthDataStart = stell( stream );
		}
	if( flags & READCMS_FLAG_WRAPPERONLY )
		{
		/* We're only reading the outer wrapper in order to accomodate
		   redundantly nested CMS content types, don't try and read
		   any further */
		ENSURES( !( flags & ( READCMS_FLAG_DEFINITELENGTH | \
							  READCMS_FLAG_DEFINITELENGTH_OPT ) ) );
		if( dataSize != NULL )
			*dataSize = length;
		if( selectionID != NULL )
			*selectionID = oidInfoPtr->selectionID;
		return( CRYPT_OK );
		}
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( isData )
		{
		/* It's pure data content, it must be an OCTET STRING */
		if( tag != BER_OCTETSTRING && \
			tag != ( BER_OCTETSTRING | BER_CONSTRUCTED ) )
			{
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		}
	else
		{
		if( flags & READCMS_FLAG_INNERHEADER )
			{
			/* It's an inner header, it should be an OCTET STRING but
			   alternative interpretations are possible based on the old
			   PKCS #7 definition of inner content */
			if( tag != BER_OCTETSTRING && \
				tag != ( BER_OCTETSTRING | BER_CONSTRUCTED ) && \
				tag != BER_SEQUENCE )
				{
				return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
				}
			}
		else
			{
			/* It's an outer header containing other than data, it must be a
			   SEQUENCE */
			if( tag != BER_SEQUENCE )
				return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		}
	status = readLongGenericHole( stream, &length, tag );
	if( cryptStatusError( status ) )
		return( status );
	if( length == CRYPT_UNUSED && \
		( flags & ( READCMS_FLAG_DEFINITELENGTH | \
					READCMS_FLAG_DEFINITELENGTH_OPT ) ) )
		{
		/* We've been asked to provide a definite length but the currently
		   available length information is indefinite, see if there's length
		   information present from an earlier header */
		if( savedLength == CRYPT_UNUSED )
			{
			/* If we've been asked to provide a definite length but there's 
			   none available, return an error */
			if( flags & READCMS_FLAG_DEFINITELENGTH )
				return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		else
			{
			/* The content length is the originally read length minus the
			   data read since that point */
			length = savedLength - ( stell( stream ) - savedLengthDataStart );
			if( !isIntegerRangeNZ( length ) )
				return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		}

	/* If it's structured (i.e. not data in an OCTET STRING), check the
	   version number of the content if required */
	if( !isData && oidInfoPtr->extraInfo != NULL )
		{
		const CMS_CONTENT_INFO *contentInfoPtr = oidInfoPtr->extraInfo;
		const int startPos = stell( stream );

		status = readShortInteger( stream, &value );
		if( cryptStatusError( status ) )
			return( status );
		if( value < contentInfoPtr->minVersion || \
			value > contentInfoPtr->maxVersion )
			{
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		
		/* Adjust the length value for the additional content that we've 
		   read if necessary */
		if( length != CRYPT_UNUSED )
			{
			length -= stell( stream ) - startPos;
			if( !isIntegerRangeNZ( length ) )
				return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		}

	/* Finally, if there's a definite length given then there has to be 
	   some content present, at least a SEQUENCE/OCTET STRING containing a
	   single ASN.1 item */
	if( length != CRYPT_UNUSED )
		{
		/* Data is a raw OCTET STRING rather than compound data, so the 
		   length can be a single byte */
		if( isData )
			{
			if( length < 1 )
				return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		else
			{
			if( length < sizeofObject( sizeofObject( 1 ) ) )
				return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		}

	if( dataSize != NULL )
		*dataSize = length;
	if( selectionID != NULL )
		*selectionID = oidInfoPtr->selectionID;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCMSheader( INOUT STREAM *stream, 
					IN_BUFFER( contentOIDlength ) const BYTE *contentOID, 
					IN_LENGTH_OID const int contentOIDlength,
					IN_LENGTH_INDEF const long dataSize, 
					const BOOLEAN isInnerHeader )
	{
	BOOLEAN isOctetString = ( isInnerHeader || \
							  ( contentOIDlength == 11 && \
							  !memcmp( contentOID, OID_CMS_DATA, 11 ) ) ) ? \
							TRUE : FALSE;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( contentOID, contentOIDlength ) && \
			contentOIDlength == sizeofOID( contentOID ) );

	REQUIRES_S( contentOID[ 0 ] == BER_OBJECT_IDENTIFIER );
	REQUIRES_S( contentOIDlength >= MIN_OID_SIZE && \
				contentOIDlength <= MAX_OID_SIZE );
	REQUIRES_S( dataSize == CRYPT_UNUSED || isIntegerRange( dataSize ) );
				/* May be zero for degenerate (detached) signatures */
	REQUIRES( isInnerHeader == TRUE || isInnerHeader == FALSE );

	/* The handling of the wrapper type for the content is rather complex.
	   If it's an outer header, it's an OCTET STRING for data and a SEQUENCE
	   for everything else.  If it's an inner header it usually follows the
	   same rule, however for signed data the content was changed from

		content [0] EXPLICIT ANY DEFINED BY contentType OPTIONAL

	   in PKCS #7 to

		eContent [0] EXPLICIT OCTET STRING OPTIONAL

	   for CMS (it was always an OCTET STRING for encrypted data).  To
	   complicate things, there are some older implementations based on the
	   original PKCS #7 interpretation that use a SEQUENCE (namely
	   AuthentiCode).  To resolve this we use an OCTET STRING for inner
	   content unless the content type is spcIndirectDataContext */
	if( isInnerHeader && contentOIDlength == 12 && \
		!memcmp( contentOID, OID_MS_SPCINDIRECTDATACONTEXT, 12 ) )
		isOctetString = FALSE;

	/* If a size is given, write the definite form */
	if( dataSize != CRYPT_UNUSED )
		{
		int status;

		writeSequence( stream, contentOIDlength + ( ( dataSize > 0 ) ? \
					   sizeofObject( sizeofObject( dataSize ) ) : 0 ) );
		status = swrite( stream, contentOID, contentOIDlength );
		if( dataSize <= 0 )
			return( status );	/* No content, exit */
		writeConstructed( stream, sizeofObject( dataSize ), 0 );
		if( isOctetString )
			return( writeOctetStringHole( stream, dataSize, DEFAULT_TAG ) );
		return( writeSequence( stream, dataSize ) );
		}

	/* No size given, write the indefinite form */
	writeSequenceIndef( stream );
	swrite( stream, contentOID, contentOIDlength );
	writeCtag0Indef( stream );
	return( isOctetString ? writeOctetStringIndef( stream ) : \
							writeSequenceIndef( stream ) );
	}

/* Read and write an encryptedContentInfo header.  The inner content may be
   implicitly or explicitly tagged depending on the exact content type */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofCMSencrHeader( IN_BUFFER( contentOIDlength ) const BYTE *contentOID, 
						 IN_LENGTH_OID const int contentOIDlength,
						 IN_LENGTH_INDEF const long dataSize, 
						 IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	STREAM nullStream;
	int length, cryptInfoSize DUMMY_INIT, status;

	assert( isReadPtrDynamic( contentOID, contentOIDlength ) && \
			contentOIDlength == sizeofOID( contentOID ) );

	REQUIRES( contentOID[ 0 ] == BER_OBJECT_IDENTIFIER );
	REQUIRES( contentOIDlength >= MIN_OID_SIZE && \
			  contentOIDlength <= MAX_OID_SIZE );
	REQUIRES( dataSize == CRYPT_UNUSED || isIntegerRangeNZ( dataSize ) );
	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Determine the encoded size of the AlgorithmIdentifier */
	sMemNullOpen( &nullStream );
	status = writeCryptContextAlgoID( &nullStream, iCryptContext );
	if( cryptStatusOK( status ) )
		cryptInfoSize = stell( &nullStream );
	sMemClose( &nullStream );
	if( cryptStatusError( status ) )
		return( status );

	/* Calculate the encoded size of the SEQUENCE + OID + AlgoID + [0] for
	   the definite or indefinite forms */
	if( dataSize == CRYPT_UNUSED )
		{
		/* The size 2 is for the tag + 0x80 indefinite-length indicator and 
		   the EOC octets at the end */
		return( 2 + contentOIDlength + cryptInfoSize + 2 );
		}
	length = sizeofObject( contentOIDlength + cryptInfoSize + \
						   sizeofObject( dataSize ) ) - dataSize;
	ENSURES( isIntegerRange( length ) );
	return( length );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readCMSencrHeader( INOUT STREAM *stream, 
					   IN_ARRAY( noOidInfoEntries ) const OID_INFO *oidInfo,
					   IN_RANGE( 1, 50 ) const int noOidInfoEntries, 
					   OUT_OPT_INT_Z int *selectionID,
					   OUT_OPT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
					   OUT_OPT QUERY_INFO *queryInfo,
					   IN_FLAGS_Z( READCMS ) const int flags )
	{
	QUERY_INFO localQueryInfo, *queryInfoPtr = ( queryInfo == NULL ) ? \
											   &localQueryInfo : queryInfo;
	long length;
	int tag, selectionValue, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( oidInfo, \
							  sizeof( OID_INFO ) * noOidInfoEntries ) );
	assert( iCryptContext == NULL || \
			isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( queryInfo == NULL || \
			isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES_S( noOidInfoEntries > 0 && noOidInfoEntries <= 50 );
	REQUIRES_S( isFlagRangeZ( flags, READCMS ) );
	REQUIRES_S( ( flags & ~( READCMS_FLAG_AUTHENC | \
							 READCMS_FLAG_DEFINITELENGTH ) ) == 0 );

	/* Clear return values */
	if( selectionID != NULL )
		*selectionID = 0;
	if( iCryptContext != NULL )
		*iCryptContext = CRYPT_ERROR;
	memset( queryInfoPtr, 0, sizeof( QUERY_INFO ) );

	/* Read the outer SEQUENCE, content-type OID, and encryption 
	   AlgorithmIdentifier.  We can't use a normal readSequence() here 
	   because the data length could be much longer than the maximum allowed 
	   in the readSequence() sanity check */
	readLongSequence( stream, NULL );
	status = readOID( stream, oidInfo, noOidInfoEntries, &selectionValue );
	if( cryptStatusOK( status ) )
		{
		status = readContextAlgoID( stream, iCryptContext, queryInfoPtr,
						DEFAULT_TAG, ( flags & READCMS_FLAG_AUTHENC ) ? \
							ALGOID_CLASS_AUTHENC : ALGOID_CLASS_CRYPT );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Set up any further query info fields.  Since this isn't a proper key 
	   exchange or signature object we can't properly set up all of the 
	   remaining fields like the type (it's not any CRYPT_OBJECT_TYPE) or 
	   version fields */
	queryInfoPtr->formatType = CRYPT_FORMAT_CMS;

	/* Read the content [0] tag, which may be either primitive or constructed
	   depending on the content */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
	status = readLongGenericHole( stream, &length, tag );
	if( cryptStatusOK( status ) )
		{
		/* Make sure that the inner content type has the correct tag */
		if( tag != MAKE_CTAG( 0 ) && tag != MAKE_CTAG_PRIMITIVE( 0 ) )
			{
			sSetError( stream, CRYPT_ERROR_BADDATA );
			status = CRYPT_ERROR_BADDATA;
			}

		/* If we've been asked to provide a definite length but there's none 
		   available, return an error */
		if( ( flags & READCMS_FLAG_DEFINITELENGTH ) && \
			length == CRYPT_UNUSED )
			{
			sSetError( stream, CRYPT_ERROR_BADDATA );
			status = CRYPT_ERROR_BADDATA;
			}
		}
	if( cryptStatusError( status ) )
		{
		if( iCryptContext != NULL )
			krnlSendNotifier( *iCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	if( selectionID != NULL )
		*selectionID = selectionValue;
	queryInfoPtr->size = length;

	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCMSencrHeader( INOUT STREAM *stream, 
						IN_BUFFER( contentOIDlength ) const BYTE *contentOID, 
						IN_LENGTH_OID const int contentOIDlength,
						IN_LENGTH_INDEF const long dataSize,
						IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	STREAM nullStream;
	int cryptInfoSize DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( contentOID, contentOIDlength ) && \
			contentOIDlength == sizeofOID( contentOID ) );

	REQUIRES_S( contentOID[ 0 ] == BER_OBJECT_IDENTIFIER );
	REQUIRES_S( contentOIDlength >= MIN_OID_SIZE && \
				contentOIDlength <= MAX_OID_SIZE );
	REQUIRES_S( dataSize == CRYPT_UNUSED || isIntegerRangeNZ( dataSize ) );
	REQUIRES_S( isHandleRangeValid( iCryptContext ) );

	/* Determine the encoded size of the AlgorithmIdentifier */
	sMemNullOpen( &nullStream );
	status = writeCryptContextAlgoID( &nullStream, iCryptContext );
	if( cryptStatusOK( status ) )
		cryptInfoSize = stell( &nullStream );
	sMemClose( &nullStream );
	if( cryptStatusError( status ) )
		return( status );

	/* If a size is given, write the definite form */
	if( dataSize != CRYPT_UNUSED )
		{
		writeSequence( stream, contentOIDlength + cryptInfoSize + \
					   sizeofObject( dataSize ) );
		swrite( stream, contentOID, contentOIDlength );
		status = writeCryptContextAlgoID( stream, iCryptContext );
		if( cryptStatusError( status ) )
			return( status );
		return( writeOctetStringHole( stream, dataSize, 0 ) );
		}

	/* No size given, write the indefinite form */
	writeSequenceIndef( stream );
	swrite( stream, contentOID, contentOIDlength );
	status = writeCryptContextAlgoID( stream, iCryptContext );
	if( cryptStatusError( status ) )
		return( status );
	return( writeCtag0Indef( stream ) );
	}
#endif /* USE_INT_ASN1 */
