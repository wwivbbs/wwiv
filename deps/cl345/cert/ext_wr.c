/****************************************************************************
*																			*
*						Certificate Attribute Write Routines				*
*						 Copyright Peter Gutmann 1996-2015					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Get the encoding information needed to encode an attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int getAttributeEncodingInfo( const ATTRIBUTE_LIST *attributeListPtr,
									 OUT_PTR_COND \
										ATTRIBUTE_INFO **attributeInfoPtrPtr,
									 OUT_INT_Z int *attributeDataSize )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr;
	const int fifoEndPos = attributeListPtr->fifoEnd - 1;

	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isWritePtr( attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO * ) ) );
	assert( isWritePtr( attributeDataSize, sizeof( int ) ) );

	REQUIRES( fifoEndPos >= -1 && fifoEndPos < ENCODING_FIFO_SIZE - 1 );

	/* Clear return value */
	*attributeInfoPtrPtr = NULL;

	/* If it's a constructed attribute then the encoding information for
	   the outermost wrapper will be recorded in the encoding FIFO, 
	   otherwise it's present directly */
	if( fifoEndPos >= 0 )
		attributeInfoPtr = attributeListPtr->encodingFifo[ fifoEndPos ];
	else
		attributeInfoPtr = attributeListPtr->attributeInfoPtr;
	ENSURES( attributeInfoPtr != NULL );

	/* Determine the size of the attribute payload */
	if( fifoEndPos >= 0 && attributeInfoPtr->fieldType != FIELDTYPE_CHOICE )
		{
		const int attributePayloadSize = \
						attributeListPtr->sizeFifo[ fifoEndPos ];

		REQUIRES( isShortIntegerRange( attributePayloadSize ) );
		*attributeDataSize = sizeofShortObject( attributePayloadSize );
		}
	else
		*attributeDataSize = attributeListPtr->encodedSize;
	*attributeInfoPtrPtr = ( ATTRIBUTE_INFO * ) attributeInfoPtr;

	return( CRYPT_OK );
	}

#ifdef USE_CMSATTR

/* When we write the attributes as a SET OF Attribute (as CMS does) we have 
   to sort them by encoded value.  This is an incredible nuisance since it 
   requires that each value be encoded and stored in encoded form and then 
   the encoded forms sorted and emitted in that order.  To avoid this hassle 
   we keep a record of the current lowest encoded form and then find the 
   next one by encoding enough information (the SEQUENCE and OID, CMS 
   attributes don't have critical flags) on the fly to distinguish them.  
   This is actually less overhead than storing the encoded form for sorting 
   because there are only a small total number of attributes (usually 3-5,
   of which the main two are signingTime and messageDigest) and we don't 
   have to malloc() storage for each one and manage the stored form if we do 
   things on the fly */

#define ATTR_ENCODED_SIZE	( 16 + MAX_OID_SIZE )

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static ATTRIBUTE_LIST *getNextEncodedAttribute( const ATTRIBUTE_LIST *attributeListPtr,
												INOUT_BUFFER_FIXED( prevEncodedFormLength ) \
													BYTE *prevEncodedForm,
												IN_LENGTH_FIXED( ATTR_ENCODED_SIZE ) \
													const int prevEncodedFormLength )
	{
	const ATTRIBUTE_LIST *currentAttributeListPtr = NULL;
	STREAM stream;
	BYTE currentEncodedForm[ ATTR_ENCODED_SIZE + 8 ];
	BYTE buffer[ ATTR_ENCODED_SIZE + 8 ];
	int status, LOOP_ITERATOR;

	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isWritePtrDynamic( prevEncodedForm, prevEncodedFormLength ) );

	REQUIRES_N( prevEncodedFormLength == ATTR_ENCODED_SIZE );

	/* Give the current encoded form the maximum possible value */
	memset( currentEncodedForm, 0xFF, ATTR_ENCODED_SIZE );

	sMemOpen( &stream, buffer, ATTR_ENCODED_SIZE );

	/* Write the known attributes until we reach either the end of the list
	   or the first blob-type attribute */
	LOOP_LARGE_CHECK( attributeListPtr != NULL && \
					  !checkAttributeListProperty( attributeListPtr,
												   ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
		{
		CRYPT_ATTRIBUTE_TYPE attributeID;
		const ATTRIBUTE_INFO *attributeInfoPtr;
		int attributeDataSize, LOOP_ITERATOR_ALT;

		REQUIRES_N( sanityCheckAttributePtr( attributeListPtr ) );

		/* Get the encoding information for the attribute */
		status = getAttributeEncodingInfo( attributeListPtr, 
								( ATTRIBUTE_INFO ** ) &attributeInfoPtr, 
								&attributeDataSize );
		ENSURES_N( cryptStatusOK( status ) );
		attributeID = attributeListPtr->attributeID;

		/* Reset the stream data */
		sseek( &stream, 0 );
		memset( buffer, 0, ATTR_ENCODED_SIZE );

		/* Write the header and OID */
		writeSequence( &stream, sizeofOID( attributeInfoPtr->oid ) + \
								sizeofShortObject( attributeDataSize ) );
		status = swrite( &stream, attributeInfoPtr->oid,
						 sizeofOID( attributeInfoPtr->oid ) );
		ENSURES_N( cryptStatusOK( status ) );

		/* Check to see whether this is larger than the previous value but 
		   smaller than any other one that we've seen.  If it is, remember 
		   it.  A full-length memcmp() is safe here because no encoded form 
		   can be a prefix of another form so we always exit before we get 
		   into the leftover data from previous encodings */
		if( memcmp( prevEncodedForm, buffer, ATTR_ENCODED_SIZE ) < 0 && \
			memcmp( buffer, currentEncodedForm, ATTR_ENCODED_SIZE ) < 0 )
			{
			memcpy( currentEncodedForm, buffer, ATTR_ENCODED_SIZE );
			currentAttributeListPtr = attributeListPtr;
			}

		/* Move on to the next attribute */
		LOOP_LARGE_CHECKINC_ALT( attributeListPtr != NULL && \
								 attributeListPtr->attributeID == attributeID,
								 attributeListPtr = DATAPTR_GET( attributeListPtr->next ) );
		ENSURES_N( LOOP_BOUND_OK_ALT );
		}
	ENSURES_N( LOOP_BOUND_OK );

	/* Write the blob-type attributes */
	LOOP_LARGE_CHECKINC( attributeListPtr != NULL, 
						 attributeListPtr = DATAPTR_GET( attributeListPtr->next ) )
		{
		ENSURES_N( sanityCheckAttributePtr( attributeListPtr ) );
		ENSURES_N( checkAttributeListProperty( attributeListPtr,
											   ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) );

		/* Write the header and OID */
		sseek( &stream, 0 );
		writeSequence( &stream, 
					   sizeofOID( attributeListPtr->oid ) + \
					   sizeofShortObject( attributeListPtr->dataValueLength ) );
		status = swrite( &stream, attributeListPtr->oid,
						 sizeofOID( attributeListPtr->oid ) );
		ENSURES_N( cryptStatusOK( status ) );

		/* Check to see whether this is larger than the previous value but
		   smaller than any other one that we've seen.  If it is, remember 
		   it */
		if( memcmp( prevEncodedForm, buffer, ATTR_ENCODED_SIZE ) < 0 && \
			memcmp( buffer, currentEncodedForm, ATTR_ENCODED_SIZE ) < 0 )
			{
			memcpy( currentEncodedForm, buffer, ATTR_ENCODED_SIZE );
			currentAttributeListPtr = attributeListPtr;
			}
		}
	ENSURES_N( LOOP_BOUND_OK );

	sMemDisconnect( &stream );

	/* Remember the encoded form of the attribute and return a pointer to
	   it */
	memcpy( prevEncodedForm, currentEncodedForm, ATTR_ENCODED_SIZE );
	return( ( ATTRIBUTE_LIST * ) currentAttributeListPtr );
	}
#endif /* USE_CMSATTR */

/* Determine the size of a set of attributes and validate and preprocess the
   attribute information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int calculateAttributeSizes( const ATTRIBUTE_LIST *attributeListPtr,
									const BOOLEAN hasSpecialEncoding,
									OUT_LENGTH_Z int *attributeSize,
									OUT_LENGTH_Z int *encapsAttributeSize )
	{
	BOOLEAN_INT signUnrecognised;
	int status, LOOP_ITERATOR;

	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isWritePtr( attributeSize, sizeof( int ) ) );
	assert( isWritePtr( encapsAttributeSize, sizeof( int ) ) );

	REQUIRES( hasSpecialEncoding == TRUE || hasSpecialEncoding == FALSE );

	/* Clear return values */
	*attributeSize = *encapsAttributeSize = 0;

	/* Determine the size of the recognised attributes */
	LOOP_LARGE_CHECK( attributeListPtr != NULL && \
					  !checkAttributeListProperty( attributeListPtr,
												   ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
		{
		CRYPT_ATTRIBUTE_TYPE attributeID;
		const ATTRIBUTE_INFO *attributeInfoPtr;
		int attributeDataSize, LOOP_ITERATOR_ALT;

		ENSURES( sanityCheckAttributePtr( attributeListPtr ) );

		/* Get the encoding information for the attribute */
		status = getAttributeEncodingInfo( attributeListPtr, 
								( ATTRIBUTE_INFO ** ) &attributeInfoPtr, 
								&attributeDataSize );
		ENSURES( cryptStatusOK( status ) );
		attributeID = attributeListPtr->attributeID;
		attributeDataSize = sizeofShortObject( attributeDataSize );

		/* Determine the attribute data size */
		attributeDataSize += sizeofOID( attributeInfoPtr->oid );
		if( ( attributeInfoPtr->typeInfoFlags & FL_ATTR_CRITICAL ) || \
			TEST_FLAG( attributeListPtr->flags, ATTR_FLAG_CRITICAL ) )
			attributeDataSize += sizeofBoolean();
		attributeDataSize = sizeofShortObject( attributeDataSize );

		/* Some certificate objects (and specifically PKCS #10 requests) 
		   have two classes of extensions, ones that apply to the request 
		   itself and ones that apply to the certificate that the request 
		   will be turned into, which are themselves encapsulated inside 
		   their own extension type.  To deal with this we record sizes for 
		   encapsulated and non-encapsulated extensions */
		if( hasSpecialEncoding )
			{
			if( attributeInfoPtr->encodingFlags & FL_SPECIALENCODING )
				*attributeSize += attributeDataSize;
			else
				*encapsAttributeSize += attributeDataSize;
			}
		else
			{
			/* It's a standard extension */
			*attributeSize += attributeDataSize;
			}

		/* Skip everything else in the current attribute */
		LOOP_LARGE_CHECKINC_ALT( attributeListPtr != NULL && \
								 attributeListPtr->attributeID == attributeID,
								 attributeListPtr = DATAPTR_GET( attributeListPtr->next ) );
		ENSURES( LOOP_BOUND_OK_ALT );
		}
	ENSURES( LOOP_BOUND_OK );

	/* If we're not going to be signing the blob-type attributes, return */
	status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, 
							  IMESSAGE_GETATTRIBUTE, &signUnrecognised, 
							  CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES );
	if( cryptStatusError( status ) || !signUnrecognised )
		return( CRYPT_OK );

	/* Determine the size of the blob-type attributes.  This has the same 
	   problem as the recognised attributes, but this time even more so, 
	   since the attribute isn't recognised we don't know whether it should
	   apply to the request or to the certificate that the request will be
	   turned into.  Based on a user survey it seems that in the rare cases
	   when custom attributes are used in requests, they're intended to
	   apply to the certificate that the request will be turned into, so we
	   include them in the encapsulated attributes */
	LOOP_LARGE_CHECKINC( attributeListPtr != NULL, 
						 attributeListPtr = DATAPTR_GET( attributeListPtr->next ) )
		{
		int attributeDataSize;

		ENSURES( sanityCheckAttributePtr( attributeListPtr ) );
		ENSURES( checkAttributeListProperty( attributeListPtr,
											 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) );

		attributeDataSize = \
					sizeofShortObject( sizeofOID( attributeListPtr->oid ) + \
					sizeofShortObject( attributeListPtr->dataValueLength ) );
		if( TEST_FLAG( attributeListPtr->flags, ATTR_FLAG_CRITICAL ) )
			attributeDataSize += sizeofBoolean();
		if( hasSpecialEncoding )
			*encapsAttributeSize += attributeDataSize;
		else
			*attributeSize += attributeDataSize;
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

CHECK_RETVAL_LENGTH_SHORT \
int sizeofAttributes( IN_DATAPTR_OPT const DATAPTR_ATTRIBUTE attributePtr,
					  IN_ENUM_OPT( CRYPT_CERTTYPE ) \
							const CRYPT_CERTTYPE_TYPE type )
	{
	const ATTRIBUTE_LIST *attributeListPtr;
	const BOOLEAN hasSpecialEncoding = \
					( type == CRYPT_CERTTYPE_CERTREQUEST ) ? TRUE : FALSE;
	int attributeSize, encapsAttributeSize, status;

	REQUIRES( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES( isEnumRangeOpt( type, CRYPT_CERTTYPE ) );
			  /* Single CRL, OCSP, and RTCS entries have the special-case 
			     type CRYPT_CERTTYPE_NONE */

	/* If there are no attributes, return now */
	if( DATAPTR_ISNULL( attributePtr ) )
		return( 0 );

	attributeListPtr = DATAPTR_GET( attributePtr );
	REQUIRES( attributeListPtr != NULL ); 
	REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );

	/* Calculate the size of the attributes */
	status = calculateAttributeSizes( attributeListPtr, hasSpecialEncoding,
									  &attributeSize, &encapsAttributeSize );
	ENSURES( cryptStatusOK( status ) );

	/* If it's a PKCS #10 request, add any extra size from the encapsulated
	   certificate attributes */
	if( hasSpecialEncoding && encapsAttributeSize > 0 )
		{
		attributeSize += sizeofShortObject( \
							sizeofOID( OID_PKCS9_EXTREQ ) + \
							sizeofShortObject( \
								sizeofShortObject( encapsAttributeSize ) ) );
		}
	
	return( attributeSize );
	}

/****************************************************************************
*																			*
*					Attribute/Attribute Field Write Routines				*
*																			*
****************************************************************************/

/* Calculate the size of an attribute field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int calculateSpecialFieldSize( const ATTRIBUTE_LIST *attributeListPtr,
									  const ATTRIBUTE_INFO *attributeInfoPtr,
									  OUT_LENGTH_SHORT_Z int *payloadSize, 
									  const int fieldType )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( payloadSize, sizeof( int ) ) );

	REQUIRES( isBlobField( fieldType ) || \
			  ( fieldType == FIELDTYPE_IDENTIFIER ) || \
			  ( fieldType > 0 && fieldType < MAX_TAG ) );

	/* Determine the size of the data payload */
	*payloadSize = attributeListPtr->sizeFifo[ attributeListPtr->fifoPos ];
	ENSURES( isShortIntegerRange( *payloadSize ) );

	/* It's a special-case field, the data size is taken from somewhere 
	   other than the user-supplied data */
	switch( fieldType )
		{
		case FIELDTYPE_BLOB_ANY:
		case FIELDTYPE_BLOB_BITSTRING:
		case FIELDTYPE_BLOB_SEQUENCE:
			/* Fixed-value blob (as opposed to user-supplied one) */
			return( attributeInfoPtr->defaultValue );

		case FIELDTYPE_IDENTIFIER:
			return( sizeofOID( attributeInfoPtr->oid ) );

		case BER_SEQUENCE:
		case BER_SET:
			return( sizeofShortObject( *payloadSize ) );
		}

	retIntError();
	}

CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int calculateFieldSize( const ATTRIBUTE_LIST *attributeListPtr,
							   const ATTRIBUTE_INFO *attributeInfoPtr,
							   const int fieldType )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( fieldType >= FIELDTYPE_LAST && fieldType < MAX_TAG );
			  /* The default handler at the end can include fields up to 
			     FIELDTYPE_LAST */

	switch( fieldType )
		{
		case FIELDTYPE_BLOB_ANY:
		case FIELDTYPE_BLOB_BITSTRING:
		case FIELDTYPE_BLOB_SEQUENCE:
		case BER_OBJECT_IDENTIFIER:
			return( attributeListPtr->dataValueLength );

		case FIELDTYPE_DN:
			{
			const DATAPTR_DN dn = GET_DN_POINTER( attributeListPtr );

			REQUIRES( DATAPTR_ISVALID( dn ) );

			return( sizeofDN( dn ) );
			}

		case FIELDTYPE_IDENTIFIER:
			return( sizeofOID( attributeInfoPtr->oid ) );

		case BER_BITSTRING:
			return( sizeofBitString( attributeListPtr->intValue ) );

		case BER_BOOLEAN:
			return( sizeofBoolean() );

		case BER_ENUMERATED:
			return( sizeofEnumerated( attributeListPtr->intValue ) );

		case BER_INTEGER:
			return( sizeofShortInteger( attributeListPtr->intValue ) );

		case BER_NULL:
			/* This is stored as a pseudo-numeric value CRYPT_UNUSED so we 
			   can't fall through to the default handler */
			return( sizeofNull() );

		case BER_OCTETSTRING:
			return( sizeofShortObject( attributeListPtr->dataValueLength ) );

		case BER_TIME_GENERALIZED:
			return( sizeofGeneralizedTime() );

		case BER_TIME_UTC:
			return( sizeofUTCTime() );
		}

	return( sizeofShortObject( attributeListPtr->dataValueLength ) );
	}

/* Determine the encoded size of an attribute field */

CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofAttributeField( INOUT ATTRIBUTE_LIST *attributeListPtr )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr;
	BOOLEAN isSpecial;
	int fieldType, size, status;

	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );

	/* Get the encoding information for this attribute field */
	isSpecial = ( attributeListPtr->fifoPos > 0 ) ? TRUE : FALSE;
	if( isSpecial )
		{
#if 0	/* 18/12/17 Never reached in any test code */
		assert( DEBUG_WARN );
		attributeListPtr->fifoPos--;	/* Move down to the next entry */
										// Should we be modifying this?
		attributeInfoPtr = attributeListPtr->encodingFifo[ attributeListPtr->fifoPos ];
#endif /* 0 */
		retIntError();
		}
	else
		attributeInfoPtr = attributeListPtr->attributeInfoPtr;
	ENSURES( attributeInfoPtr != NULL );
	fieldType = attributeInfoPtr->fieldType;

	/* If this is just a marker for a series of CHOICE alternatives, return
	   without doing anything */
	if( fieldType == FIELDTYPE_CHOICE )
		{
#if 0	/* 18/12/17 Never reached in any test code */
		assert( DEBUG_WARN );
		return( 0 );
#endif /* 0 */
		retIntError();
		}

	/* Calculate the size of the encoded data */
#if 0	/* 18/12/17 Never reached due to isSpecial exit above */
	if( isSpecial )
		{
		int dummy;

		status = size = \
			calculateSpecialFieldSize( attributeListPtr, attributeInfoPtr,
									   &dummy, fieldType );
		}
	else
#endif /* 0 */
		{
		status = size = \
			calculateFieldSize( attributeListPtr, attributeInfoPtr, 
								fieldType );
		}
	if( cryptStatusError( status ) )
		return( status );

	return( ( attributeInfoPtr->encodingFlags & FL_EXPLICIT ) ? \
			sizeofShortObject( size ) : size );
	}

/* Write an attribute field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeAttributeField( INOUT STREAM *stream, 
								INOUT ATTRIBUTE_LIST *attributeListPtr,
								IN_RANGE( 0, 4 ) const int complianceLevel )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr;
	const BOOLEAN isSpecial = ( attributeListPtr->fifoPos > 0 ) ? TRUE : FALSE;
	const void *dataPtr = attributeListPtr->dataValue;
	int fieldType, tag, size, payloadSize DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );

	/* Get the encoding information for this attribute field */
	if( isSpecial )
		{
		REQUIRES( attributeListPtr->fifoPos > 0 && \
				  attributeListPtr->fifoPos < ENCODING_FIFO_SIZE );
		attributeListPtr->fifoPos--;	/* Move down to the next entry */
		attributeInfoPtr = attributeListPtr->encodingFifo[ attributeListPtr->fifoPos ];
		}
	else
		attributeInfoPtr = attributeListPtr->attributeInfoPtr;
	ENSURES( attributeInfoPtr != NULL );
	fieldType = attributeInfoPtr->fieldType;

	/* If this is just a marker for a series of CHOICE alternatives, return
	   without doing anything */
	if( fieldType == FIELDTYPE_CHOICE )
		return( CRYPT_OK );

	/* Calculate the size of the encoded data */
	if( isSpecial )
		{
		status = size = \
			calculateSpecialFieldSize( attributeListPtr, attributeInfoPtr,
									   &payloadSize, fieldType );
		}
	else
		{
		status = size = \
			calculateFieldSize( attributeListPtr, attributeInfoPtr, 
								fieldType );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If the field is explicitly tagged, add another layer of wrapping */
	if( attributeInfoPtr->encodingFlags & FL_EXPLICIT )
		{
		status = writeConstructed( stream, size, 
								   attributeInfoPtr->fieldEncodedType );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If the encoded field type differs from the actual field type (because
	   of implicit tagging) and we're not specifically using explicit
	   tagging and it's not a DN in a GeneralName (which is a tagged IMPLICIT
	   SEQUENCE overridden to make it EXPLICIT because of the tagged CHOICE
	   encoding rules) set the tag to the encoded field type rather than the
	   actual field type */
	if( attributeInfoPtr->fieldEncodedType >= 0 && \
		!( attributeInfoPtr->encodingFlags & FL_EXPLICIT ) && \
		attributeInfoPtr->fieldType != FIELDTYPE_DN )
		tag = attributeInfoPtr->fieldEncodedType;
	else
		tag = DEFAULT_TAG;

	/* Write the data as appropriate */
	if( isSpecial )
		{
		/* If it's a special-case field, the data is taken from somewhere
		   other than the user-supplied data */
		switch( fieldType )
			{
			case FIELDTYPE_BLOB_ANY:
			case FIELDTYPE_BLOB_BITSTRING:
			case FIELDTYPE_BLOB_SEQUENCE:
				/* Fixed-value blob (as opposed to user-supplied one) */
				return( swrite( stream, attributeInfoPtr->extraData, size ) );

			case FIELDTYPE_IDENTIFIER:
				return( swrite( stream, attributeInfoPtr->oid, size ) );

			case BER_SEQUENCE:
			case BER_SET:
				if( tag != DEFAULT_TAG )
					return( writeConstructed( stream, payloadSize, tag ) );
				return( ( fieldType == BER_SET ) ? \
						writeSet( stream, payloadSize ) : \
						writeSequence( stream, payloadSize ) );
			}
		
		retIntError();
		}

	/* It's a standard object, take the data from the user-supplied data */
	switch( fieldType )
		{
		case FIELDTYPE_BLOB_ANY:
		case FIELDTYPE_BLOB_BITSTRING:
		case FIELDTYPE_BLOB_SEQUENCE:
			if( tag != DEFAULT_TAG )
				{
				/* This gets a bit messy because the blob is stored in 
				   encoded form in the attribute, to write it as a tagged 
				   value we have to write a different first byte */
				sputc( stream, getFieldEncodedTag( attributeInfoPtr ) );
				return( swrite( stream, ( ( BYTE * ) dataPtr ) + 1,
								attributeListPtr->dataValueLength - 1 ) );
				}
			return( swrite( stream, dataPtr, 
					attributeListPtr->dataValueLength ) );

		case FIELDTYPE_DN:
			{
			const DATAPTR_DN dn = GET_DN_POINTER( attributeListPtr );

			REQUIRES( DATAPTR_ISVALID( dn ) );

			return( writeDN( stream, dn, tag ) );
			}

		case FIELDTYPE_IDENTIFIER:
			ENSURES( tag == DEFAULT_TAG );
			return( swrite( stream, attributeInfoPtr->oid, size ) );

		case FIELDTYPE_TEXTSTRING:
			if( tag == DEFAULT_TAG )
				{
				int newStringLen, dummy;

				status = getAsn1StringInfo( dataPtr, 
								attributeListPtr->dataValueLength, 
								&dummy, &tag, &newStringLen, FALSE );
				if( cryptStatusError( status ) )
					return( status );

				/* If the only way to encode the string is to convert it 
				   into a wider encoding type then we can't safely emit 
				   it */
				if( newStringLen != attributeListPtr->dataValueLength )
					return( CRYPT_ERROR_BADDATA );
				}
			return( writeCharacterString( stream, dataPtr, 
										  attributeListPtr->dataValueLength, 
										  tag ) );

		case BER_BITSTRING:
			REQUIRES( isIntegerRange( attributeListPtr->intValue ) );
			return( writeBitString( stream, ( int ) \
									attributeListPtr->intValue, tag ) );

		case BER_BOOLEAN:
			REQUIRES( isIntegerRange( attributeListPtr->intValue ) );
			return( writeBoolean( stream, attributeListPtr->intValue ? \
								  TRUE : FALSE, tag ) );

		case BER_ENUMERATED:
			REQUIRES( isIntegerRange( attributeListPtr->intValue ) );
			return( writeEnumerated( stream, ( int ) \
									 attributeListPtr->intValue, tag ) );

		case BER_INTEGER:
			REQUIRES( isIntegerRange( attributeListPtr->intValue ) );
			return( writeShortInteger( stream, attributeListPtr->intValue, 
									   tag ) );

		case BER_NULL:
			return( writeNull( stream, tag ) );

		case BER_OBJECT_IDENTIFIER:
			if( tag != DEFAULT_TAG )
				{
				/* This gets a bit messy because the OID is stored in 
				   encoded form in the attribute, to write it as a tagged 
				   value we have to write a different first byte */
				sputc( stream, getFieldEncodedTag( attributeInfoPtr ) );
				return( swrite( stream, ( ( BYTE * ) dataPtr ) + 1,
								attributeListPtr->dataValueLength - 1 ) );
				}
			return( swrite( stream, dataPtr, 
							attributeListPtr->dataValueLength ) );

		case BER_OCTETSTRING:
			return( writeOctetString( stream, dataPtr, 
									  attributeListPtr->dataValueLength, 
									  tag ) );

		case BER_STRING_BMP:
		case BER_STRING_IA5:
		case BER_STRING_ISO646:
		case BER_STRING_NUMERIC:
		case BER_STRING_PRINTABLE:
		case BER_STRING_UTF8:
			return( writeCharacterString( stream, dataPtr, 
										  attributeListPtr->dataValueLength,
										  ( tag == DEFAULT_TAG ) ? \
											fieldType : \
											MAKE_CTAG_PRIMITIVE( tag ) ) );

		case BER_TIME_GENERALIZED:
			return( writeGeneralizedTime( stream, *( time_t * ) dataPtr, 
										  tag ) );

		case BER_TIME_UTC:
			return( writeUTCTime( stream, *( time_t * ) dataPtr, tag ) );
		}

	retIntError();
	}

/* Write an attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeAttribute( INOUT STREAM *stream, 
						   INOUT ATTRIBUTE_LIST **attributeListPtrPtr,
						   const BOOLEAN wrapperTagSet, 
						   IN_RANGE( 0, 4 ) const int complianceLevel )
	{
	CRYPT_ATTRIBUTE_TYPE attributeID;
	const ATTRIBUTE_INFO *attributeInfoPtr;
	ATTRIBUTE_LIST *attributeListPtr = *attributeListPtrPtr;
	BOOLEAN isCritical = FALSE;
	int attributeDataSize, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtrPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtr( *attributeListPtrPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( wrapperTagSet == TRUE || wrapperTagSet == FALSE );
	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );

	/* Get the encoding information for the attribute */
	status = getAttributeEncodingInfo( attributeListPtr, 
								( ATTRIBUTE_INFO ** ) &attributeInfoPtr, 
								&attributeDataSize );
	ENSURES( cryptStatusOK( status ) );
	attributeID = attributeListPtr->attributeID;
	if( ( attributeInfoPtr->typeInfoFlags & FL_ATTR_CRITICAL ) || \
		TEST_FLAG( attributeListPtr->flags, ATTR_FLAG_CRITICAL ) )
		isCritical = TRUE;

	/* Write the outer SEQUENCE, OID, critical flag (if it's set) and 
	   appropriate wrapper for the attribute payload */
	writeSequence( stream, 
				   sizeofOID( attributeInfoPtr->oid ) + \
				   ( isCritical ? sizeofBoolean() : 0 ) + \
				   sizeofShortObject( attributeDataSize ) );
	swrite( stream, attributeInfoPtr->oid,
			sizeofOID( attributeInfoPtr->oid ) );
	if( isCritical )
		writeBoolean( stream, TRUE, DEFAULT_TAG );
	if( wrapperTagSet )
		status = writeSet( stream, attributeDataSize );
	else
		status = writeOctetStringHole( stream, attributeDataSize, 
									   DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the current attribute */
	LOOP_MED_CHECKINC( attributeListPtr != NULL && \
					   attributeListPtr->attributeID == attributeID,
					   attributeListPtr = DATAPTR_GET( attributeListPtr->next ) )
		{
		int LOOP_ITERATOR_ALT;

		REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );

		/* Write any encapsulating SEQUENCEs if necessary, followed by the 
		   field itself.  In some rare instances we may have a zero-length 
		   SEQUENCE (if all the member(s) of the sequence have default 
		   values) so we only try to write the member if there's encoding 
		   information for it present.  
		   
		   Note that this loop doesn't update the FIFO position since this 
		   is updated by the call to writeAttributeField() */
		LOOP_EXT_INITCHECK_ALT( attributeListPtr->fifoPos = \
									attributeListPtr->fifoEnd, 
							   cryptStatusOK( status ) && \
									attributeListPtr->fifoPos > 0, 
							   ENCODING_FIFO_SIZE + 1 )
			{
			status = writeAttributeField( stream, 
									( ATTRIBUTE_LIST * ) attributeListPtr,
									complianceLevel );
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		if( cryptStatusOK( status ) && \
			attributeListPtr->attributeInfoPtr != NULL )
			{
			status = writeAttributeField( stream, 
									( ATTRIBUTE_LIST * ) attributeListPtr,
									complianceLevel );
			}
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	*attributeListPtrPtr = attributeListPtr;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeBlobAttribute( INOUT STREAM *stream, 
							   INOUT ATTRIBUTE_LIST *attributeListPtr,
							   const BOOLEAN wrapperTagSet )
	{
	const BOOLEAN isCritical = TEST_FLAG( attributeListPtr->flags, 
										  ATTR_FLAG_CRITICAL ) ? TRUE : FALSE;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( wrapperTagSet == TRUE || wrapperTagSet == FALSE );

	/* Write the header, OID, critical flag (if present), and payload 
	   wrapped up as appropriate */
	writeSequence( stream, 
			sizeofOID( attributeListPtr->oid ) + \
			( isCritical ? sizeofBoolean() : 0 ) + \
			sizeofShortObject( attributeListPtr->dataValueLength ) );
	swrite( stream, attributeListPtr->oid,
			sizeofOID( attributeListPtr->oid ) );
	if( isCritical )
		writeBoolean( stream, TRUE, DEFAULT_TAG );
	if( wrapperTagSet )
		writeSet( stream, attributeListPtr->dataValueLength );
	else
		{
		writeOctetStringHole( stream, 
							  attributeListPtr->dataValueLength, 
							  DEFAULT_TAG );
		}
	status = swrite( stream, attributeListPtr->dataValue,
					 attributeListPtr->dataValueLength );
	return( status );
	}

#ifdef USE_CMSATTR

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCmsAttributes( INOUT STREAM *stream, 
							   IN const ATTRIBUTE_LIST *attributeListPtr,
							   IN_ENUM( CRYPT_CERTTYPE ) \
									const CRYPT_CERTTYPE_TYPE type,
							   IN_LENGTH const int attributeSize,
							   IN_RANGE( 0, 4 ) const int complianceLevel )
	{
	ATTRIBUTE_LIST *currentAttributePtr;
	BYTE currentEncodedForm[ ATTR_ENCODED_SIZE + 8 ];
	int status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( type == CRYPT_CERTTYPE_CMS_ATTRIBUTES || \
			  type == CRYPT_CERTTYPE_RTCS_REQUEST || \
			  type == CRYPT_CERTTYPE_RTCS_RESPONSE );
	REQUIRES( isIntegerRangeNZ( attributeSize ) );
	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );

	/* Write the wrapper, depending on the object type */
	if( type == CRYPT_CERTTYPE_RTCS_REQUEST )
		status = writeSet( stream, attributeSize );
	else
		{
		status = writeConstructed( stream, attributeSize, 
								  ( type == CRYPT_CERTTYPE_CMS_ATTRIBUTES ) ? \
									CTAG_SI_AUTHENTICATEDATTRIBUTES : \
									CTAG_RP_EXTENSIONS );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Write the attributes in sorted form */
	memset( currentEncodedForm, 0, ATTR_ENCODED_SIZE );	/* Set lowest encoded form */
	LOOP_LARGE( currentAttributePtr = \
					getNextEncodedAttribute( attributeListPtr, 
											 currentEncodedForm, 
											 ATTR_ENCODED_SIZE ),
				currentAttributePtr != NULL && cryptStatusOK( status ),
				currentAttributePtr = \
					getNextEncodedAttribute( attributeListPtr, 
											 currentEncodedForm,
											 ATTR_ENCODED_SIZE ) )
		{
		REQUIRES( sanityCheckAttributePtr( currentAttributePtr ) );

		if( checkAttributeListProperty( currentAttributePtr,
										ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
			{
			status = writeBlobAttribute( stream, currentAttributePtr, TRUE );
			currentAttributePtr = DATAPTR_GET( currentAttributePtr->next );
			}
		else
			{
			status = writeAttribute( stream, &currentAttributePtr, TRUE, 
									 complianceLevel );
			}
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}
#endif /* USE_CMSATTR */

#ifdef USE_CERTREQ

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCertReqAttributes( INOUT STREAM *stream, 
								   IN const ATTRIBUTE_LIST *attributeListPtr,
								   IN_RANGE( 0, 4 ) const int complianceLevel )
	{
	int status = CRYPT_OK, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );

	/* Write any standalone attributes that aren't encapsulated in an
	   extensionRequest.  We stop when we reach the blob-type attributes
	   because, since they're blobs, we don't know whether they're 
	   standalone or extensionRequest-encapsulated ones */
	LOOP_LARGE_CHECK( cryptStatusOK( status ) && \
					  attributeListPtr != NULL && \
					  !checkAttributeListProperty( attributeListPtr,
												   ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
		{
		const ATTRIBUTE_INFO *attributeInfoPtr;
		DATAPTR_ATTRIBUTE attributePtr;

		REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );
		attributeInfoPtr = attributeListPtr->attributeInfoPtr;

		/* If this is an attribute that's not encapsulated inside an 
		   extensionRequest (denoted by it having the FL_SPECIALENCODING
		   flag set), write it now.  We have to check for attributeInfoPtr 
		   not being NULL because in the case of an attribute that contains 
		   all-default values (for example the default basicConstraints with 
		   cA = DEFAULT FALSE and pathLenConstraint absent) there won't be 
		   any encoding information present since the entire attribute is 
		   empty */
		if( attributeInfoPtr != NULL && \
			( attributeInfoPtr->encodingFlags & FL_SPECIALENCODING ) )
			{
			/* Since this is a CMS attribute we write it with a SET wrapper 
			   tag rather than an OCTET STRING wrapper tag, indicated by 
			   setting the wrapperTagSet flag to TRUE */
			status = writeAttribute( stream, 
									 ( ATTRIBUTE_LIST ** ) &attributeListPtr, 
									 TRUE, complianceLevel );
			if( cryptStatusError( status ) )
				return( status );
			continue;
			}

		/* Move on to the next attribute */
		DATAPTR_SET( attributePtr, ( ATTRIBUTE_LIST * ) attributeListPtr );
		attributePtr = certMoveAttributeCursor( attributePtr, 
												CRYPT_ATTRIBUTE_CURRENT_GROUP, 
												CRYPT_CURSOR_NEXT );
		attributeListPtr = DATAPTR_GET( attributePtr );
		}
	ENSURES( LOOP_BOUND_OK );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCertReqWrapper( INOUT STREAM *stream, 
								IN const ATTRIBUTE_LIST *attributeListPtr,
								IN_RANGE( 0, 4 ) const int complianceLevel )
	{
	int encapsAttributeSize, nonEncapsAttributeSize, totalAttributeSize = 0;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );

	/* Certificate request attributes can be written in two groups, standard
	   attributes that apply to the request itself and attributes that 
	   should be placed in the certificate that's created from the request,
	   encapsulated inside an extensionRequest attribute.  First we 
	   determine which portion of the attributes will be encoded as is and
	   which will be encapsulated inside an extensionRequest */
	status = calculateAttributeSizes( attributeListPtr, TRUE,
									  &nonEncapsAttributeSize, 
									  &encapsAttributeSize );
	ENSURES( cryptStatusOK( status ) );

	/* Determine the overall size of the attributes */
	if( encapsAttributeSize > 0 )
		{
		totalAttributeSize += \
					sizeofShortObject( \
						sizeofOID( OID_PKCS9_EXTREQ ) + \
						sizeofShortObject( \
							sizeofShortObject( encapsAttributeSize ) ) );
		}
	if( nonEncapsAttributeSize > 0 )
		totalAttributeSize += nonEncapsAttributeSize;

	/* Write the overall wrapper for the attributes */
	status = writeConstructed( stream, totalAttributeSize, 
							   CTAG_CR_ATTRIBUTES );
	if( cryptStatusError( status ) )
		return( status );

	/* If there are non-encapsulated attributes present, write them first */
	if( nonEncapsAttributeSize > 0 ) 
		{
		status = writeCertReqAttributes( stream, attributeListPtr, 
										 complianceLevel );
		if( cryptStatusError( status ) )
			return( status );
		if( encapsAttributeSize <= 0 )
			{
			/* There's nothing left to write as an encapsulated attribute, 
			   we're done */
			return( CRYPT_OK );
			}
		}

	/* Write the wrapper for the remaining attributes, encapsulated inside 
	   a PKCS #9 extensionRequest attribute */
	writeSequence( stream, sizeofOID( OID_PKCS9_EXTREQ ) + \
				   sizeofShortObject( \
							sizeofShortObject( encapsAttributeSize ) ) );
	swrite( stream, OID_PKCS9_EXTREQ, sizeofOID( OID_PKCS9_EXTREQ ) );
	writeSet( stream, sizeofShortObject( encapsAttributeSize ) );
	return( writeSequence( stream, encapsAttributeSize ) );
	}
#endif /* USE_CERTREQ */

/****************************************************************************
*																			*
*						Attribute Collection Write Routines					*
*																			*
****************************************************************************/

/* Write a set of attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAttributes( INOUT STREAM *stream, 
					 INOUT const DATAPTR_ATTRIBUTE attributePtr,
					 IN_ENUM_OPT( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE type,
					 IN_LENGTH const int attributeSize )
	{
	ATTRIBUTE_LIST *attributeListPtr;
	BOOLEAN_INT signUnrecognised DUMMY_INIT;
	int complianceLevel, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( DATAPTR_ISSET( attributePtr ) );
	REQUIRES( isEnumRangeOpt( type, CRYPT_CERTTYPE ) );
			  /* Single CRL, OCSP, and RTCS entries have the special-case 
			     type CRYPT_CERTTYPE_NONE */
	REQUIRES( isIntegerRangeNZ( attributeSize ) );

	attributeListPtr = DATAPTR_GET( attributePtr );
	ENSURES( attributeListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );

	/* Some attributes have odd encoding/handling requirements that can 
	   cause problems for other software so we only enforce peculiarities 
	   required by some standards at higher compliance levels.  In addition 
	   we only sign unrecognised attributes if we're explicitly asked to do 
	   so by the user */
	status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE,
							  IMESSAGE_GETATTRIBUTE, &complianceLevel,
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, 
								  IMESSAGE_GETATTRIBUTE, &signUnrecognised,
								  CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* CMS attributes work somewhat differently from normal attributes in 
	   that, since they're encoded as a SET OF Attribute, they have to be 
	   sorted according to their encoded form before being written.  For 
	   this reason we don't write them sorted by OID as with the other 
	   attributes but keep writing the next-lowest attribute until they've 
	   all been written */
#ifdef USE_CMSATTR
	if( type == CRYPT_CERTTYPE_CMS_ATTRIBUTES || \
		type == CRYPT_CERTTYPE_RTCS_REQUEST || \
		type == CRYPT_CERTTYPE_RTCS_RESPONSE )
		{
		return( writeCmsAttributes( stream, attributeListPtr, type, 
									attributeSize, complianceLevel ) );
		}
#endif /* USE_CMSATTR */

	/* Write the appropriate wrapper for the extensions.  CRLs and OCSP 
	   requests/responses have two extension types that have different 
	   tagging, per-entry extensions and entire-CRL/request extensions.  To 
	   differentiate between the two we write per-entry extensions with a 
	   type of CRYPT_CERTTYPE_NONE.

	   Certificate requests also have two classes of extensions, the first
	   class is extensions that are meant to go into the certificare that's
	   created from the request (for example keyUsage, basicConstrains,
	   altNames) which are encapsulated inside a PKCS #9 extensionRequest 
	   extension, and the second class is extensions that are written as
	   is.  To deal with this we write the as-is extensions first as part
	   of the wrapper write (there usually aren't any of these), and then we 
	   write the extensions that go inside the PKCS #9 extensionRequest 
	   (which is usually all of them) */
	switch( type )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
		case CRYPT_CERTTYPE_CRL:
			writeConstructed( stream, sizeofShortObject( attributeSize ),
							  ( type == CRYPT_CERTTYPE_CERTIFICATE ) ? \
							  CTAG_CE_EXTENSIONS : CTAG_CL_EXTENSIONS );
			status = writeSequence( stream, attributeSize );
			break;

#ifdef USE_CERTREQ
		case CRYPT_CERTTYPE_CERTREQUEST:
			status = writeCertReqWrapper( stream, attributeListPtr, 
										  complianceLevel );
			break;
#endif /* USE_CERTREQ */

		case CRYPT_CERTTYPE_REQUEST_CERT:
		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			/* No wrapper, extensions are written directly */
			status = CRYPT_OK;
			break;

		case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
		case CRYPT_CERTTYPE_PKIUSER:
		case CRYPT_CERTTYPE_NONE:
			status = writeSequence( stream, attributeSize );
			break;

		case CRYPT_CERTTYPE_OCSP_REQUEST:
			writeConstructed( stream, sizeofShortObject( attributeSize ), 
							  CTAG_OR_EXTENSIONS );
			status = writeSequence( stream, attributeSize );
			break;

		case CRYPT_CERTTYPE_OCSP_RESPONSE:
			writeConstructed( stream, sizeofShortObject( attributeSize ), 
							  CTAG_OP_EXTENSIONS );
			status = writeSequence( stream, attributeSize );
			break;

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Write the known attributes until we reach either the end of the list
	   or the first blob-type attribute */
	LOOP_LARGE_CHECK( cryptStatusOK( status ) && \
					  attributeListPtr != NULL && \
					  !checkAttributeListProperty( attributeListPtr,
												   ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
		{
		const ATTRIBUTE_INFO *attributeInfoPtr;

		REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );
		attributeInfoPtr = attributeListPtr->attributeInfoPtr;

		/* If this attribute requires special-case encoding (which in 
		   practice means that it's a PKCS #10 non-encapsulated attribute) 
		   then we don't write it at this point since it's already been 
		   written as part of the certificate-object wrapper write */
		if( attributeInfoPtr != NULL && \
			( attributeInfoPtr->encodingFlags & FL_SPECIALENCODING ) )
			{
			DATAPTR_ATTRIBUTE attributeCursor;

			DATAPTR_SET( attributeCursor, attributeListPtr );
			attributeCursor = certMoveAttributeCursor( attributeCursor, 
											CRYPT_ATTRIBUTE_CURRENT_GROUP, 
											CRYPT_CURSOR_NEXT );
			attributeListPtr = DATAPTR_GET( attributeCursor );
			ENSURES( attributeListPtr == NULL || \
					 sanityCheckAttributePtr( attributeListPtr ) );
			continue;
			}

		status = writeAttribute( stream, &attributeListPtr, FALSE, 
								 complianceLevel );
		}
	ENSURES( LOOP_BOUND_OK );
	if( cryptStatusError( status ) || !signUnrecognised  )
		return( status );

	/* Write the blob-type attributes */
	LOOP_LARGE_CHECKINC( attributeListPtr != NULL && cryptStatusOK( status ),
						 attributeListPtr = DATAPTR_GET( attributeListPtr->next ) )
		{
		status = writeBlobAttribute( stream, attributeListPtr, FALSE );
		}
	ENSURES( LOOP_BOUND_OK );
	return( status );
	}
#endif /* USE_CERTIFICATES */
