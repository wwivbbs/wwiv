/****************************************************************************
*																			*
*						Certificate Attribute Write Routines				*
*						 Copyright Peter Gutmann 1996-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
  #include "enc_dec/asn1.h"
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
									 OUT_OPT_PTR \
										ATTRIBUTE_INFO **attributeInfoPtrPtr,
									 OUT_INT_Z int *attributeDataSize )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr;
	BOOLEAN isConstructed = FALSE;

	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isWritePtr( attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO * ) ) );
	assert( isWritePtr( attributeDataSize, sizeof( int ) ) );

	/* Clear return value */
	*attributeInfoPtrPtr = NULL;

	/* If it's a constructed attribute then the encoding information for
	   the outermost wrapper will be recorded in the encoding FIFO, 
	   otherwise it's present directly */
	if( attributeListPtr->fifoEnd > 0 )
		{
		attributeInfoPtr = \
			attributeListPtr->encodingFifo[ attributeListPtr->fifoEnd - 1 ];
		isConstructed = TRUE;
		}
	else
		attributeInfoPtr = attributeListPtr->attributeInfoPtr;
	ENSURES( attributeInfoPtr != NULL );

	/* Determine the size of the attribute payload */
	if( isConstructed && attributeInfoPtr->fieldType != FIELDTYPE_CHOICE )
		{
		*attributeDataSize = ( int ) sizeofObject( \
				attributeListPtr->sizeFifo[ attributeListPtr->fifoEnd - 1 ] );
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
   because there are only a small total number of attributes (usually 3) and 
   we don't have to malloc() storage for each one and manage the stored form 
   if we do things on the fly */

#define ATTR_ENCODED_SIZE	( 16 + MAX_OID_SIZE )

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static ATTRIBUTE_LIST *getNextEncodedAttribute( const ATTRIBUTE_LIST *attributeListPtr,
												OUT_BUFFER_FIXED( prevEncodedFormLength ) \
													BYTE *prevEncodedForm,
												IN_LENGTH_FIXED( ATTR_ENCODED_SIZE ) \
													const int prevEncodedFormLength )
	{
	const ATTRIBUTE_LIST *currentAttributeListPtr = NULL;
	STREAM stream;
	BYTE currentEncodedForm[ ATTR_ENCODED_SIZE + 8 ];
	BYTE buffer[ ATTR_ENCODED_SIZE + 8 ];
	int iterationCount, status;

	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isWritePtr( prevEncodedForm, prevEncodedFormLength ) );

	REQUIRES_N( prevEncodedFormLength == ATTR_ENCODED_SIZE );

	/* Give the current encoded form the maximum possible value */
	memset( buffer, 0, ATTR_ENCODED_SIZE );
	memset( currentEncodedForm, 0xFF, ATTR_ENCODED_SIZE );

	sMemOpen( &stream, buffer, ATTR_ENCODED_SIZE );

	/* Write the known attributes until we reach either the end of the list
	   or the first blob-type attribute */
	for( iterationCount = 0;
		 attributeListPtr != NULL && \
			!checkAttributeProperty( attributeListPtr,
									 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 iterationCount++ )
		{
		CRYPT_ATTRIBUTE_TYPE attributeID;
		const ATTRIBUTE_INFO *attributeInfoPtr;
		int attributeDataSize;

		/* Get the encoding information for the attribute */
		status = getAttributeEncodingInfo( attributeListPtr, 
								( ATTRIBUTE_INFO ** ) &attributeInfoPtr, 
								&attributeDataSize );
		ENSURES_N( cryptStatusOK( status ) );
		attributeID = attributeListPtr->attributeID;

		/* Write the header and OID */
		sseek( &stream, 0 );
		writeSequence( &stream, sizeofOID( attributeInfoPtr->oid ) + \
					   ( int ) sizeofObject( attributeDataSize ) );
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
		for( /* Continue iterationCount from previous loop */ ;
			 attributeListPtr != NULL && \
				attributeListPtr->attributeID == attributeID && \
				iterationCount < FAILSAFE_ITERATIONS_LARGE;
			 attributeListPtr = attributeListPtr->next, iterationCount++ );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* Write the blob-type attributes */
	for( /* Continue iterationCount from previous loop */ ;
		 attributeListPtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		ENSURES_N( checkAttributeProperty( attributeListPtr,
										   ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) );

		/* Write the header and OID */
		sseek( &stream, 0 );
		writeSequence( &stream, sizeofOID( attributeListPtr->oid ) + \
					   ( int ) sizeofObject( attributeListPtr->valueLength ) );
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
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	sMemDisconnect( &stream );

	/* Remember the encoded form of the attribute and return a pointer to
	   it */
	memcpy( prevEncodedForm, currentEncodedForm, ATTR_ENCODED_SIZE );
	return( ( ATTRIBUTE_LIST * ) currentAttributeListPtr );
	}
#endif /* USE_CMSATTR */

/* Determine the size of a set of attributes and validate and preprocess the
   attribute information */

CHECK_RETVAL \
int sizeofAttributes( IN_OPT const ATTRIBUTE_PTR *attributePtr )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;
	int signUnrecognised, attributeSize = 0, iterationCount, status;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	/* If there's nothing to write, return now */
	if( attributeListPtr == NULL )
		return( 0 );

	/* Determine the size of the recognised attributes */
	for( iterationCount = 0;
		 attributeListPtr != NULL && \
			!checkAttributeProperty( attributeListPtr,
									 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		CRYPT_ATTRIBUTE_TYPE attributeID;
		const ATTRIBUTE_INFO *attributeInfoPtr;
		int attributeDataSize;

		/* Get the encoding information for the attribute */
		status = getAttributeEncodingInfo( attributeListPtr, 
								( ATTRIBUTE_INFO ** ) &attributeInfoPtr, 
								&attributeDataSize );
		ENSURES( cryptStatusOK( status ) );
		attributeID = attributeListPtr->attributeID;
		attributeDataSize = ( int ) sizeofObject( attributeDataSize );

		/* Determine the overall attribute size */
		attributeDataSize += sizeofOID( attributeInfoPtr->oid );
		if( ( attributeInfoPtr->typeInfoFlags & FL_ATTR_CRITICAL ) || \
			( attributeListPtr->flags & ATTR_FLAG_CRITICAL ) )
			attributeDataSize += sizeofBoolean();
		attributeSize += ( int ) sizeofObject( attributeDataSize );

		/* Skip everything else in the current attribute */
		for( /* Continue iterationCount from previous loop */ ;
			 attributeListPtr != NULL && \
				attributeListPtr->attributeID == attributeID && \
				iterationCount < FAILSAFE_ITERATIONS_LARGE;
			 attributeListPtr = attributeListPtr->next, iterationCount++ );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* If we're not going to be signing the blob-type attributes, return */
	krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE, 
					 &signUnrecognised, 
					 CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES );
	if( !signUnrecognised )
		return( attributeSize );

	/* Determine the size of the blob-type attributes */
	for( ; attributeListPtr != NULL && \
		   iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		ENSURES( checkAttributeProperty( attributeListPtr,
										 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) );

		attributeSize += ( int ) \
						 sizeofObject( sizeofOID( attributeListPtr->oid ) + \
						 sizeofObject( attributeListPtr->valueLength ) );
		if( attributeListPtr->flags & ATTR_FLAG_CRITICAL )
			attributeSize += sizeofBoolean();
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

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

	/* It's a special-case field, the data size is taken from somewhere 
	   other than the user-supplied data */
	switch( fieldType )
		{
		case FIELDTYPE_BLOB_ANY:
		case FIELDTYPE_BLOB_BITSTRING:
		case FIELDTYPE_BLOB_SEQUENCE:
			/* Fixed-value blob (as opposed to user-supplied one) */
			return( ( int ) attributeInfoPtr->defaultValue );

		case FIELDTYPE_IDENTIFIER:
			return( sizeofOID( attributeInfoPtr->oid ) );

#if 0	/* 28/9/08 This shouldn't ever occur, defaultValue is only used for
				   FIELDTYPE_BLOB_ANY and BOOLEAN fields */
		case BER_INTEGER:
			return( sizeofShortInteger( attributeInfoPtr->defaultValue ) );
#endif /* 0 */

		case BER_SEQUENCE:
		case BER_SET:
			return( ( int ) sizeofObject( *payloadSize ) );
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int calculateFieldSize( const ATTRIBUTE_LIST *attributeListPtr,
							   const ATTRIBUTE_INFO *attributeInfoPtr,
							   const int fieldType )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( fieldType >= FIELDTYPE_TEXTSTRING && fieldType < MAX_TAG );
			  /* The default handler at the end can include fields up to 
			     FIELDTYPE_TEXTSTRING */

	switch( fieldType )
		{
		case FIELDTYPE_BLOB_ANY:
		case FIELDTYPE_BLOB_BITSTRING:
		case FIELDTYPE_BLOB_SEQUENCE:
		case BER_OBJECT_IDENTIFIER:
			return( attributeListPtr->valueLength );

		case FIELDTYPE_DN:
			return( sizeofDN( attributeListPtr->value ) );

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
			return( ( int ) sizeofObject( attributeListPtr->valueLength ) );

		case BER_TIME_GENERALIZED:
			return( sizeofGeneralizedTime() );

		case BER_TIME_UTC:
			return( sizeofUTCTime() );
		}

	return( ( int ) sizeofObject( attributeListPtr->valueLength ) );
	}

/* Write an attribute field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int writeAttributeField( INOUT_OPT STREAM *stream, 
						 INOUT ATTRIBUTE_LIST *attributeListPtr,
						 IN_RANGE( 0, 4 ) const int complianceLevel )
	{
	const BOOLEAN isSpecial = ( attributeListPtr->fifoPos > 0 ) ? TRUE : FALSE;
	const ATTRIBUTE_INFO *attributeInfoPtr = ( isSpecial ) ? \
		attributeListPtr->encodingFifo[ --attributeListPtr->fifoPos ] : \
		attributeListPtr->attributeInfoPtr;
	const void *dataPtr = attributeListPtr->value;
	const int fieldType = attributeInfoPtr->fieldType;
	int tag, size, payloadSize = DUMMY_INIT;

	assert( stream == NULL || isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );

	/* If this is just a marker for a series of CHOICE alternatives, return
	   without doing anything */
	if( fieldType == FIELDTYPE_CHOICE )
		return( CRYPT_OK );

	/* Calculate the size of the encoded data */
	if( isSpecial )
		{
		size = calculateSpecialFieldSize( attributeListPtr, attributeInfoPtr, 
										  &payloadSize, fieldType );
		}
	else
		{
		size = calculateFieldSize( attributeListPtr, attributeInfoPtr, 
								   fieldType );
		}
	if( cryptStatusError( size ) )
		return( size );

	/* If we're just calculating the attribute size, don't write any data */
	if( stream == NULL )
		{
		return( ( attributeInfoPtr->encodingFlags & FL_EXPLICIT ) ? \
				( int ) sizeofObject( size ) : size );
		}

	/* If the field is explicitly tagged, add another layer of wrapping */
	if( attributeInfoPtr->encodingFlags & FL_EXPLICIT )
		writeConstructed( stream, size, attributeInfoPtr->fieldEncodedType );

	/* If the encoded field type differs from the actual field type (because
	   if implicit tagging) and we're not specifically using explicit
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

#if 0	/* 28/9/08 See comment in calculateSpecialFieldSize() */
			case BER_INTEGER:
				return( writeShortInteger( stream, attributeInfoPtr->defaultValue, 
										   tag ) );
#endif /* 0 */

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
								attributeListPtr->valueLength - 1 ) );
				}
			return( swrite( stream, dataPtr, attributeListPtr->valueLength ) );

		case FIELDTYPE_DN:
			return( writeDN( stream, attributeListPtr->value, tag ) );

		case FIELDTYPE_IDENTIFIER:
			ENSURES( tag == DEFAULT_TAG );
			return( swrite( stream, attributeInfoPtr->oid, size ) );

		case FIELDTYPE_TEXTSTRING:
			if( tag == DEFAULT_TAG )
				{
				int status;

				status = getAsn1StringType( dataPtr, 
											attributeListPtr->valueLength, 
											&tag );
				if( cryptStatusError( status ) )
					return( status );
				}
			return( writeCharacterString( stream, dataPtr, 
										  attributeListPtr->valueLength, 
										  tag ) );

		case BER_BITSTRING:
			return( writeBitString( stream, ( int ) \
									attributeListPtr->intValue, tag ) );

		case BER_BOOLEAN:
			return( writeBoolean( stream, ( BOOLEAN ) \
								  attributeListPtr->intValue, tag ) );

		case BER_ENUMERATED:
			return( writeEnumerated( stream, ( int ) \
									 attributeListPtr->intValue, tag ) );

		case BER_INTEGER:
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
								attributeListPtr->valueLength - 1 ) );
				}
			return( swrite( stream, dataPtr, 
							attributeListPtr->valueLength ) );

		case BER_OCTETSTRING:
			return( writeOctetString( stream, dataPtr, 
									  attributeListPtr->valueLength, 
									  tag ) );

		case BER_STRING_BMP:
		case BER_STRING_IA5:
		case BER_STRING_ISO646:
		case BER_STRING_NUMERIC:
		case BER_STRING_PRINTABLE:
		case BER_STRING_UTF8:
			return( writeCharacterString( stream, dataPtr, 
										  attributeListPtr->valueLength,
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
	int attributeDataSize, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtrPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtr( *attributeListPtrPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );

	/* Get the encoding information for the attribute */
	status = getAttributeEncodingInfo( attributeListPtr, 
								( ATTRIBUTE_INFO ** ) &attributeInfoPtr, 
								&attributeDataSize );
	ENSURES( cryptStatusOK( status ) );
	attributeID = attributeListPtr->attributeID;
	if( ( attributeInfoPtr->typeInfoFlags & FL_ATTR_CRITICAL ) || \
		( attributeListPtr->flags & ATTR_FLAG_CRITICAL ) )
		isCritical = TRUE;

	/* Write the outer SEQUENCE, OID, critical flag (if it's set) and 
	   appropriate wrapper for the attribute payload */
	writeSequence( stream, 
				   sizeofOID( attributeInfoPtr->oid ) + \
				   ( isCritical ? sizeofBoolean() : 0 ) + \
				   ( int ) sizeofObject( attributeDataSize ) );
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
	for( iterationCount = 0;
		 attributeListPtr != NULL && \
			attributeListPtr->attributeID == attributeID && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		int innerIterationCount;
			
		/* Write any encapsulating SEQUENCEs if necessary, followed by the 
		   field itself.  In some rare instances we may have a zero-length 
		   SEQUENCE (if all the member(s) of the sequence have default 
		   values) so we only try to write the member if there's encoding 
		   information for it present */
		for( attributeListPtr->fifoPos = attributeListPtr->fifoEnd, \
				innerIterationCount = 0;
			 cryptStatusOK( status ) && \
				attributeListPtr->fifoPos > 0 && \
				innerIterationCount < ENCODING_FIFO_SIZE;
			 innerIterationCount++ )
			{
			status = writeAttributeField( stream, 
									( ATTRIBUTE_LIST * ) attributeListPtr,
									complianceLevel );
			}
		ENSURES( innerIterationCount < ENCODING_FIFO_SIZE );
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
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	*attributeListPtrPtr = attributeListPtr;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeBlobAttribute( INOUT STREAM *stream, 
							   INOUT ATTRIBUTE_LIST **attributeListPtrPtr,
							   const BOOLEAN wrapperTagSet )
	{
	ATTRIBUTE_LIST *attributeListPtr = *attributeListPtrPtr;
	const BOOLEAN isCritical = \
			( attributeListPtr->flags & ATTR_FLAG_CRITICAL ) ? TRUE : FALSE;
	int status;

	/* Write the header, OID, critical flag (if present), and payload 
	   wrapped up as appropriate */
	writeSequence( stream, 
				   sizeofOID( attributeListPtr->oid ) + \
				   ( isCritical ? sizeofBoolean() : 0 ) + \
				   ( int ) sizeofObject( attributeListPtr->valueLength ) );
	swrite( stream, attributeListPtr->oid,
			sizeofOID( attributeListPtr->oid ) );
	if( isCritical )
		writeBoolean( stream, TRUE, DEFAULT_TAG );
	if( wrapperTagSet )
		writeSet( stream, attributeListPtr->valueLength );
	else
		{
		writeOctetStringHole( stream, attributeListPtr->valueLength, 
							  DEFAULT_TAG );
		}
	status = swrite( stream, attributeListPtr->value,
					 attributeListPtr->valueLength );
	if( cryptStatusOK( status ) )
		*attributeListPtrPtr = attributeListPtr->next;
	return( status );
	}

#ifdef USE_CMSATTR

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCmsAttributes( INOUT STREAM *stream, 
						const ATTRIBUTE_LIST *attributeListPtr,
						IN_ENUM( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE type,
						IN_LENGTH const int attributeSize,
						IN_RANGE( 0, 4 ) const int complianceLevel )
	{
	ATTRIBUTE_LIST *currentAttributePtr;
	BYTE currentEncodedForm[ ATTR_ENCODED_SIZE + 8 ];
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( type == CRYPT_CERTTYPE_CMS_ATTRIBUTES || \
			  type == CRYPT_CERTTYPE_RTCS_REQUEST || \
			  type == CRYPT_CERTTYPE_RTCS_RESPONSE );
	REQUIRES( attributeSize > 0 && attributeSize < MAX_INTLENGTH );
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
	for( currentAttributePtr = getNextEncodedAttribute( attributeListPtr, \
														currentEncodedForm,
														ATTR_ENCODED_SIZE ),
			iterationCount = 0;
		 currentAttributePtr != NULL && cryptStatusOK( status ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		currentAttributePtr = getNextEncodedAttribute( attributeListPtr,
													   currentEncodedForm,
													   ATTR_ENCODED_SIZE ),
			iterationCount++ )
		{
		if( checkAttributeProperty( currentAttributePtr,
									ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
			{
			status = writeBlobAttribute( stream, &currentAttributePtr, TRUE );
			}
		else
			{
			status = writeAttribute( stream, &currentAttributePtr, TRUE, 
									 complianceLevel );
			}
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( CRYPT_OK );
	}
#endif /* USE_CMSATTR */

#ifdef USE_CERTREQ

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCertReqAttributes( INOUT STREAM *stream, 
							const ATTRIBUTE_LIST *attributeListPtr,
							IN_RANGE( 0, 4 ) const int complianceLevel )
	{
	int iterationCount, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );

	/* Write any standalone attributes that aren't encapsulated in an
	   extensionRequest.  We stop when we reach the blob-type attributes
	   because, since they're blobs, we don't know whether they're 
	   standalone or extensionRequest-encapsulated ones */
	for( iterationCount = 0;
		 cryptStatusOK( status ) && attributeListPtr != NULL && \
			!checkAttributeProperty( attributeListPtr,
									 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		const ATTRIBUTE_INFO *attributeInfoPtr = \
									attributeListPtr->attributeInfoPtr;

		/* If this is an attribute that's not encapsulated inside an 
		   extensionRequest, write it now.  We have to check for 
		   attributeInfoPtr not being NULL because in the case of an 
		   attribute that contains all-default values (for example the
		   default basicConstraints with cA = DEFAULT FALSE and 
		   pathLenConstraint absent) there won't be any encoding information 
		   present since the entire attribute is empty */
		if( attributeInfoPtr != NULL && \
			( attributeInfoPtr->encodingFlags & FL_SPECIALENCODING ) )
			{
			/* Write the attribute with a SET wrapper tag (for CMS 
			   attributes) rather than an OCTET STRING wrapper tag (for
			   certificate attributes) since this is a FL_SPECIALENCODING 
			   attribute */
			status = writeAttribute( stream, 
									 ( ATTRIBUTE_LIST ** ) &attributeListPtr, 
									 TRUE, complianceLevel );
			if( cryptStatusError( status ) )
				return( status );
			continue;
			}

		/* Move on to the next attribute */
		attributeListPtr = certMoveAttributeCursor( attributeListPtr, 
										CRYPT_ATTRIBUTE_CURRENT_GROUP, 
										CRYPT_CURSOR_NEXT );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCertReqWrapper( INOUT STREAM *stream, 
						 const ATTRIBUTE_LIST *attributeListPtr,
						 IN_LENGTH const int attributeSize,
						 IN_RANGE( 0, 4 ) const int complianceLevel )
	{
	STREAM nullStream;
	int encapsAttributeSize, nonEncapsAttributeSize = DUMMY_INIT;
	int totalAttributeSize = 0, status;

	/* Certificate request attributes can be written in two groups, standard
	   attributes that apply to the request itself and attributes that 
	   should be placed in the certificate that's created from the request,
	   encapsulated inside an extensionRequest attribute.  First we 
	   determine which portion of the attributes will be encoded as is and
	   which will be encapsulated inside an extensionRequest */
	sMemNullOpen( &nullStream );
	status = writeCertReqAttributes( &nullStream, attributeListPtr, 
									 complianceLevel );
	if( cryptStatusOK( status ) )
		nonEncapsAttributeSize = stell( &nullStream );
	sMemClose( &nullStream );
	if( cryptStatusError( status ) )
		return( status );
	encapsAttributeSize = attributeSize - nonEncapsAttributeSize;

	/* Determine the overall size of the attributes */
	if( encapsAttributeSize > 0 )
		{
		totalAttributeSize += ( int ) \
					sizeofObject( \
						sizeofOID( OID_PKCS9_EXTREQ ) + \
						sizeofObject( sizeofObject( encapsAttributeSize ) ) );
		}
	if( nonEncapsAttributeSize > 0 )
		totalAttributeSize += nonEncapsAttributeSize;

	/* Write the overall wrapper for the attributes */
	writeConstructed( stream, totalAttributeSize, CTAG_CR_ATTRIBUTES );

	/* The fact that the attributes are written in two groups means that the 
	   overall attribute size doesn't apply to either of the two groups.  
	   This means that we have to adjust the overall size based on how much 
	   we've written of the unencapsulated attributes */
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
	   an extensionRequest */
	writeSequence( stream, sizeofOID( OID_PKCS9_EXTREQ ) + \
				   ( int ) sizeofObject( \
								sizeofObject( encapsAttributeSize ) ) );
	swrite( stream, OID_PKCS9_EXTREQ, sizeofOID( OID_PKCS9_EXTREQ ) );
	writeSet( stream, ( int ) sizeofObject( encapsAttributeSize ) );
	return( writeSequence( stream, encapsAttributeSize ) );
	}
#endif /* USE_CERTREQ */

/****************************************************************************
*																			*
*						Attribute Collection Write Routines					*
*																			*
****************************************************************************/

/* Write a set of attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeAttributes( INOUT STREAM *stream, 
					 INOUT ATTRIBUTE_PTR *attributePtr,
					 IN_ENUM_OPT( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE type,
					 IN_LENGTH const int attributeSize )
	{
	ATTRIBUTE_LIST *attributeListPtr = attributePtr;
	int signUnrecognised = DUMMY_INIT, complianceLevel, iterationCount;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( type >= CRYPT_CERTTYPE_NONE && type < CRYPT_CERTTYPE_LAST );
			  /* Single CRL entries have the special-case type 
			     CRYPT_CERTTYPE_NONE */
	REQUIRES( attributeSize > 0 && attributeSize < MAX_INTLENGTH );

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

	/* Write the appropriate extensions tag for the certificate object and 
	   determine how far we can read.  
	   
	   CRLs and OCSP requests/responses have two extension types that have 
	   different tagging, per-entry extensions and entire-CRL/request 
	   extensions.  To differentiate between the two we write per-entry 
	   extensions with a type of CRYPT_CERTTYPE_NONE.

	   Certificate requests also have two classes of extensions, the first
	   class is extensions that are meant to go into the certificare that's
	   created from the request (for example keyUsage, basicConstrains,
	   altNames) which are encapsulated inside an extensionRequest 
	   extension, and the second class is extensions that are written as
	   is.  To deal with this we write the as-is extensions first (there
	   usually aren't any of these), and then we write the extensions that
	   go inside the extensionRequest (which is usually all of them) */
	switch( type )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
		case CRYPT_CERTTYPE_CRL:
			writeConstructed( stream, ( int ) sizeofObject( attributeSize ),
							  ( type == CRYPT_CERTTYPE_CERTIFICATE ) ? \
							  CTAG_CE_EXTENSIONS : CTAG_CL_EXTENSIONS );
			status = writeSequence( stream, attributeSize );
			break;

		case CRYPT_CERTTYPE_CERTREQUEST:
			status = writeCertReqWrapper( stream, attributeListPtr, 
										  attributeSize, complianceLevel );
			break;

		case CRYPT_CERTTYPE_REQUEST_CERT:
		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			/* No wrapper, extensions are written directly */
			break;

		case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
		case CRYPT_CERTTYPE_PKIUSER:
		case CRYPT_CERTTYPE_NONE:
			status = writeSequence( stream, attributeSize );
			break;

		case CRYPT_CERTTYPE_OCSP_REQUEST:
			writeConstructed( stream, ( int ) sizeofObject( attributeSize ), 
							  CTAG_OR_EXTENSIONS );
			status = writeSequence( stream, attributeSize );
			break;

		case CRYPT_CERTTYPE_OCSP_RESPONSE:
			writeConstructed( stream, ( int ) sizeofObject( attributeSize ), 
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
	for( iterationCount = 0;
		 cryptStatusOK( status ) && attributeListPtr != NULL && \
			!checkAttributeProperty( attributeListPtr,
									 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		const ATTRIBUTE_INFO *attributeInfoPtr = \
									attributeListPtr->attributeInfoPtr;

		/* If this attribute requires special-case encoding then we don't 
		   write it at this point */
		if( attributeInfoPtr != NULL && \
			( attributeInfoPtr->encodingFlags & FL_SPECIALENCODING ) )
			{
			attributeListPtr = certMoveAttributeCursor( attributeListPtr, 
											CRYPT_ATTRIBUTE_CURRENT_GROUP, 
											CRYPT_CURSOR_NEXT );
			continue;
			}

		status = writeAttribute( stream, &attributeListPtr, FALSE, 
								 complianceLevel );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	if( cryptStatusError( status ) || !signUnrecognised  )
		return( status );

	/* Write the blob-type attributes */
	for( iterationCount = 0;
		 attributeListPtr != NULL && cryptStatusOK( status ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		status = writeBlobAttribute( stream, &attributeListPtr, FALSE );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	return( status );
	}
#endif /* USE_CERTIFICATES */
