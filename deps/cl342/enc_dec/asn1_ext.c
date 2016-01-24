/****************************************************************************
*																			*
*					ASN.1 Supplemental Read/Write Routines					*
*						Copyright Peter Gutmann 1992-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*							Message Digest Routines							*
*																			*
****************************************************************************/

/* Read/write a message digest value.  This is another one of those oddball
   functions which is present here because it's the least inappropriate place
   to put it */

CHECK_RETVAL_LENGTH \
int sizeofMessageDigest( IN_ALGO const CRYPT_ALGO_TYPE hashAlgo, 
						 IN_LENGTH_HASH const int hashSize )
	{
	const int hashParam = isParameterisedHashAlgo( hashAlgo ) ? hashSize : 0;
	int algoInfoSize, hashInfoSize;

	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashSize >= 16 && hashSize <= CRYPT_MAX_HASHSIZE );

	algoInfoSize = sizeofAlgoIDex( hashAlgo, hashParam, 0 );
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
	const int hashParam = isParameterisedHashAlgo( hashAlgo ) ? hashSize : 0;
	int status;
	
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( hash, hashSize ) );

	REQUIRES_S( isHashAlgo( hashAlgo ) );
	REQUIRES_S( hashSize >= 16 && hashSize <= CRYPT_MAX_HASHSIZE );

	writeSequence( stream, sizeofAlgoIDex( hashAlgo, hashParam, 0 ) + \
				   ( int ) sizeofObject( hashSize ) );
	status = writeAlgoIDex( stream, hashAlgo, hashParam, 0 );
	if( cryptStatusOK( status ) )
		status = writeOctetString( stream, hash, hashSize, DEFAULT_TAG );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int readMessageDigest( INOUT STREAM *stream, 
					   OUT_ALGO_Z CRYPT_ALGO_TYPE *hashAlgo,
					   OUT_BUFFER( hashMaxLen, *hashSize ) void *hash, 
					   IN_LENGTH_HASH const int hashMaxLen, 
					   OUT_LENGTH_SHORT_Z int *hashSize )
	{
	int hashAlgoSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( hashAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( isWritePtr( hash, hashMaxLen ) );
	assert( isWritePtr( hashSize, sizeof( int ) ) );

	REQUIRES_S( hashMaxLen >= 16 && hashMaxLen <= 8192 );

	/* Clear the return values */
	memset( hash, 0, min( 16, hashMaxLen ) );
	*hashSize = 0;

	/* Read the message digest, enforcing sensible size values */
	readSequence( stream, NULL );
	status = readAlgoIDex( stream, hashAlgo, NULL, &hashAlgoSize, 
						   ALGOID_CLASS_HASH );
	if( cryptStatusOK( status ) )
		status = readOctetString( stream, hash, hashSize, 16, hashMaxLen );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a parameterised hash algorithm, make sure that the amount of 
	   hash data matches the algorithm parameter */
	if( hashAlgoSize != 0 && hashAlgoSize != *hashSize )
		return( CRYPT_ERROR_BADDATA );

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
				   OUT_OPT_LENGTH_INDEF long *dataSize, 
				   IN_FLAGS_Z( READCMS ) const int flags )
	{
	const OID_INFO *oidInfoPtr;
	BOOLEAN isData = FALSE;
	long savedLength = CRYPT_UNUSED, savedLengthDataStart = DUMMY_INIT;
	long length, value;
	int tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( oidInfo, sizeof( OID_INFO ) * noOidInfoEntries ) );
	assert( dataSize == NULL || isWritePtr( dataSize, sizeof( long ) ) );

	REQUIRES_S( noOidInfoEntries > 0 && noOidInfoEntries <= 50 );
	REQUIRES_S( flags >= READCMS_FLAG_NONE && flags < READCMS_FLAG_MAX );
	REQUIRES_S( !( ( flags & ( READCMS_FLAG_DEFINITELENGTH | \
							   READCMS_FLAG_DEFINITELENGTH_OPT ) ) && \
				   ( dataSize == NULL ) ) );
	REQUIRES_S( !( ( flags & READCMS_FLAG_WRAPPERONLY ) && \
				   ( oidInfo[ 0 ].extraInfo != NULL ) ) );
	REQUIRES_S( !( flags & READCMS_FLAG_AUTHENC ) );

	/* Clear the return value */
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

	/* If the content type is data the content is an OCTET STRING rather 
	   than a SEQUENCE so we remember the type for later.  Since there
	   are a pile of CMS OIDs of the same length as OID_CMS_DATA, we check 
	   for a match on the last byte before we perform a full OID match */
	static_assert_opt( sizeofOID( OID_CMS_DATA ) == 11, \
					   "Data OID size" );
	if( sizeofOID( oidInfoPtr->oid ) == sizeofOID( OID_CMS_DATA ) && \
		oidInfoPtr->oid[ 10 ] == OID_CMS_DATA[ 10 ] && \
		!memcmp( oidInfoPtr->oid, OID_CMS_DATA, \
				 sizeofOID( OID_CMS_DATA ) ) )
		isData = TRUE;

	/* If it's a definite length, check for special-case situations like 
	   detached signatures */
	if( length != CRYPT_UNUSED )
		{
		/* If the content is supplied externally (for example with a 
		   detached signature), denoted by the fact that the total content 
		   consists only of the OID, we're done */
		if( length <= sizeofOID( oidInfoPtr->oid ) )
			return( oidInfoPtr->selectionID );
		}
	else
		{
		/* Some Microsoft software produces an indefinite encoding for a 
		   single OID so we have to check for this */
		status = checkEOC( stream );
		if( cryptStatusError( status ) )
			return( status );
		if( status == TRUE )
			{
			/* We've seen EOC octets, the item has zero length (for example
			   with a detached signature), we're done */
			return( oidInfoPtr->selectionID );
			}
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
		return( oidInfoPtr->selectionID );
		}
	tag = peekTag( stream );
	if( cryptStatusError( tag ) )
		return( tag );
	if( isData )
		{
		/* It's pure data content, it must be an OCTET STRING */
		if( tag != BER_OCTETSTRING && \
			tag != ( BER_OCTETSTRING | BER_CONSTRUCTED ) )
			status = CRYPT_ERROR_BADDATA;
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
				status = CRYPT_ERROR_BADDATA;
			}
		else
			{
			/* It's an outer header containing other than data, it must be a
			   SEQUENCE */
			if( tag != BER_SEQUENCE )
				status = CRYPT_ERROR_BADDATA;
			}
		}
	if( cryptStatusError( status ) )
		return( sSetError( stream, status ) );
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
			}
		}

	/* If it's structured (i.e. not data in an OCTET STRING), check the
	   version number of the content if required */
	if( !isData && oidInfoPtr->extraInfo != NULL )
		{
		const CMS_CONTENT_INFO *contentInfoPtr = oidInfoPtr->extraInfo;
		const long startPos = stell( stream );

		status = readShortInteger( stream, &value );
		if( cryptStatusError( status ) )
			return( status );
		if( value < contentInfoPtr->minVersion || \
			value > contentInfoPtr->maxVersion )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		
		/* Adjust the length value for the additional content that we've 
		   read if necessary */
		if( length != CRYPT_UNUSED )
			length -= stell( stream ) - startPos;
		}

	if( dataSize != NULL )
		*dataSize = length;
	return( oidInfoPtr->selectionID );
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
	assert( isReadPtr( contentOID, contentOIDlength ) && \
			contentOIDlength == sizeofOID( contentOID ) );

	REQUIRES_S( contentOID[ 0 ] == BER_OBJECT_IDENTIFIER );
	REQUIRES_S( contentOIDlength >= MIN_OID_SIZE && \
				contentOIDlength <= MAX_OID_SIZE );
	REQUIRES_S( dataSize == CRYPT_UNUSED || \
				( dataSize >= 0 && dataSize < MAX_INTLENGTH ) );
				/* May be zero for degenerate (detached) signatures */

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
					   ( int ) sizeofObject( sizeofObject( dataSize ) ) : 0 ) );
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
	int status, cryptInfoSize = DUMMY_INIT;

	assert( isReadPtr( contentOID, contentOIDlength ) && \
			contentOIDlength == sizeofOID( contentOID ) );

	REQUIRES( contentOID[ 0 ] == BER_OBJECT_IDENTIFIER );
	REQUIRES( contentOIDlength >= MIN_OID_SIZE && \
			  contentOIDlength <= MAX_OID_SIZE );
	REQUIRES( dataSize == CRYPT_UNUSED || \
			  ( dataSize > 0 && dataSize < MAX_INTLENGTH ) );
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
	   the definite or indefinite forms (the size 2 is for the tag + 0x80
	   indefinite-length indicator and the EOC octets at the end) */
	if( dataSize != CRYPT_UNUSED )
		{
		return( ( int ) \
				( sizeofObject( contentOIDlength + \
								cryptInfoSize + \
								sizeofObject( dataSize ) ) - dataSize ) );
		}
	return( 2 + contentOIDlength + cryptInfoSize + 2 );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readCMSencrHeader( INOUT STREAM *stream, 
					   IN_ARRAY( noOidInfoEntries ) const OID_INFO *oidInfo,
					   IN_RANGE( 1, 50 ) const int noOidInfoEntries, 
					   OUT_OPT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
					   OUT_OPT QUERY_INFO *queryInfo,
					   IN_FLAGS_Z( READCMS ) const int flags )
	{
	QUERY_INFO localQueryInfo, *queryInfoPtr = ( queryInfo == NULL ) ? \
											   &localQueryInfo : queryInfo;
	long length;
	int selectionID, tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( oidInfo, sizeof( OID_INFO ) * noOidInfoEntries ) );
	assert( iCryptContext == NULL || \
			isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( queryInfo == NULL || \
			isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES_S( noOidInfoEntries > 0 && noOidInfoEntries <= 50 );
	REQUIRES_S( flags >= READCMS_FLAG_NONE && flags < READCMS_FLAG_MAX );
	REQUIRES_S( ( flags & ~( READCMS_FLAG_AUTHENC | \
							 READCMS_FLAG_DEFINITELENGTH ) ) == 0 );

	/* Clear return values */
	if( iCryptContext != NULL )
		*iCryptContext = CRYPT_ERROR;
	memset( queryInfoPtr, 0, sizeof( QUERY_INFO ) );

	/* Read the outer SEQUENCE, content-type OID, and encryption 
	   AlgorithmIdentifier.  We can't use a normal readSequence() here 
	   because the data length could be much longer than the maximum allowed 
	   in the readSequence() sanity check */
	readLongSequence( stream, NULL );
	status = readOID( stream, oidInfo, noOidInfoEntries, &selectionID );
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
	tag = peekTag( stream );
	if( cryptStatusError( tag ) )
		return( tag );
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
	queryInfoPtr->size = length;

	return( selectionID );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCMSencrHeader( INOUT STREAM *stream, 
						IN_BUFFER( contentOIDlength ) const BYTE *contentOID, 
						IN_LENGTH_OID const int contentOIDlength,
						IN_LENGTH_INDEF const long dataSize,
						IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	STREAM nullStream;
	int cryptInfoSize = DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contentOID, contentOIDlength ) && \
			contentOIDlength == sizeofOID( contentOID ) );

	REQUIRES_S( contentOID[ 0 ] == BER_OBJECT_IDENTIFIER );
	REQUIRES_S( contentOIDlength >= MIN_OID_SIZE && \
				contentOIDlength <= MAX_OID_SIZE );
	REQUIRES_S( dataSize == CRYPT_UNUSED || \
				( dataSize > 0 && dataSize < MAX_INTLENGTH ) );
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
					   ( int ) sizeofObject( dataSize ) );
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
