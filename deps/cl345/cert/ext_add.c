/****************************************************************************
*																			*
*					Certificate Attribute Add/Delete Routines				*
*						Copyright Peter Gutmann 1996-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Check the validity of an attribute field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 7 ) ) \
static int checkAttributeField( IN_OPT const DATAPTR_ATTRIBUTE attributePtr,
								const ATTRIBUTE_INFO *attributeInfoPtr,
								IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
								IN_ATTRIBUTE_OPT \
									const CRYPT_ATTRIBUTE_TYPE subFieldID,
								IN_INT_Z const int value,
								IN_FLAGS( ATTR ) const int flags, 
								OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES( isValidExtension( fieldID ) );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );
assert( ( flags & ~( ATTR_FLAG_NONE | ATTR_FLAG_CRITICAL | ATTR_FLAG_MULTIVALUED ) ) == 0 );
	REQUIRES( isFlagRangeZ( flags, ATTR ) );

	/* Clear return value */
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Make sure that a valid field has been specified and that this field
	   isn't already present as a non-default entry unless it's a field for
	   which multiple values are allowed */
	if( attributeInfoPtr == NULL )
		return( CRYPT_ARGERROR_VALUE );
	if( DATAPTR_ISSET( attributePtr ) )
		{
		DATAPTR_ATTRIBUTE attributeCursor;

		/* If this attribute is already present and it's not multivalued, we 
		   can't have any duplicate fields */
		attributeCursor = findAttributeField( attributePtr, fieldID, 
											  subFieldID );
		if( DATAPTR_ISSET( attributeCursor ) && \
			!( ( attributeInfoPtr->encodingFlags & FL_MULTIVALUED ) || \
			   ( flags & ATTR_FLAG_MULTIVALUED ) ) )
			{
			*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
			return( CRYPT_ERROR_INITED );
			}
		}

	switch( attributeInfoPtr->fieldType )
		{
		case FIELDTYPE_IDENTIFIER:
			/* It's an identifier, make sure that all parameters are correct */
			if( value != CRYPT_UNUSED )
				return( CRYPT_ARGERROR_NUM1 );
			break;

		case FIELDTYPE_DN:
			/* When creating a new certificate this is a special-case field 
			   that's used as a placeholder to indicate that a DN structure 
			   is being instantiated.  When reading an encoded certificate 
			   this is the decoded DN structure */
			ENSURES( value == CRYPT_UNUSED );
			break;

		case BER_BOOLEAN:
			/* BOOLEAN data is accepted as zero/nonzero so it's always 
			   valid */
			break;

		case BER_INTEGER:
		case BER_ENUMERATED:
		case BER_BITSTRING:
		case BER_NULL:
		case FIELDTYPE_CHOICE:
			/* Check that the range is valid */
			if( value < attributeInfoPtr->lowRange || \
				value > attributeInfoPtr->highRange )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				return( CRYPT_ARGERROR_NUM1 );
				}
			break;

		default:
			retIntError();
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 8, 9 ) ) \
static int checkAttributeFieldString( IN_OPT const DATAPTR_ATTRIBUTE attributePtr,
								const ATTRIBUTE_INFO *attributeInfoPtr,
								IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
								IN_ATTRIBUTE_OPT \
									const CRYPT_ATTRIBUTE_TYPE subFieldID,
								IN_BUFFER( dataLength ) const void *data, 
								IN_LENGTH_ATTRIBUTE const int dataLength,
								IN_FLAGS( ATTR ) const int flags, 
								OUT_LENGTH_SHORT_Z int *newDataLength, 
								OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	int status;

	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isReadPtrDynamic( data, dataLength ) );
	assert( isWritePtr( newDataLength, sizeof( int ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES( isValidExtension( fieldID ) );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );
	REQUIRES( dataLength > 0 && dataLength <= MAX_ATTRIBUTE_SIZE );
assert( ( flags & ~( ATTR_FLAG_NONE | ATTR_FLAG_BLOB_PAYLOAD | ATTR_FLAG_CRITICAL | ATTR_FLAG_MULTIVALUED ) ) == 0 );
	REQUIRES( isFlagRangeZ( flags, ATTR ) );

	/* Clear return value */
	*newDataLength = 0;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Make sure that a valid field has been specified and that this field
	   isn't already present as a non-default entry unless it's a field for
	   which multiple values are allowed */
	if( attributeInfoPtr == NULL )
		return( CRYPT_ARGERROR_VALUE );
	if( DATAPTR_ISSET( attributePtr ) )
		{
		DATAPTR_ATTRIBUTE attributeCursor;

		/* If this attribute is already present and it's not multivalued, we 
		   can't have any duplicate fields */
		attributeCursor = findAttributeField( attributePtr, fieldID, 
											  subFieldID );
		if( DATAPTR_ISSET( attributeCursor ) && \
			!( ( attributeInfoPtr->encodingFlags & FL_MULTIVALUED ) || \
			   ( flags & ATTR_FLAG_MULTIVALUED ) ) )
			{
			*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
			return( CRYPT_ERROR_INITED );
			}
		}

	/* If it's a blob field, don't do any type checking.  This is a special 
	   case that differs from FIELDTYPE_BLOB_xxx in that it corresponds to 
	   an ASN.1 value that's mis-encoded by one or more implementations, so 
	   we have to accept absolutely anything at this point */
	if( flags & ATTR_FLAG_BLOB )
		return( CRYPT_OK );

	/* If it's a DN_PTR there's no further type checking to be done either */
	if( attributeInfoPtr->fieldType == FIELDTYPE_DN )
		return( CRYPT_OK );

	/* If we've been passed an OID it may be encoded as a text string so 
	   before we can continue we have to determine whether any 
	   transformations will be necessary */
	if( attributeInfoPtr->fieldType == BER_OBJECT_IDENTIFIER )
		{
		const BYTE *oidPtr = data;
		BYTE binaryOID[ MAX_OID_SIZE + 8 ];

		/* If it's a BER/DER-encoded OID, make sure that it's valid ASN.1 */
		if( oidPtr[ 0 ] == BER_OBJECT_IDENTIFIER )
			{
			if( dataLength >= MIN_OID_SIZE && dataLength <= MAX_OID_SIZE && \
				sizeofOID( oidPtr ) == dataLength )
				return( CRYPT_OK );
			}
		else
			{
			int length;

			/* It's a text OID, check the syntax and make sure that the 
			   length is valid */
			status = textToOID( data, dataLength, binaryOID, MAX_OID_SIZE, 
								&length );
			if( cryptStatusOK( status ) )
				{
				/* The binary form of the OID differs in length from the 
				   string form, tell the caller that the data length will 
				   change on encoding so that they can allocate an 
				   appropriately-sized buffer for it before continuing */
				*newDataLength = length;
				return( CRYPT_OK );
				}
			}

		*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
		return( CRYPT_ARGERROR_STR1 );
		}

	/* Make sure that the data size is valid */
	if( dataLength < attributeInfoPtr->lowRange || \
		dataLength > attributeInfoPtr->highRange )
		{
		*errorType = CRYPT_ERRTYPE_ATTR_SIZE;
		return( CRYPT_ARGERROR_NUM1 );
		}

	/* If we're not checking the payload in order to handle CAs who stuff 
	   any old rubbish into the fields, exit now unless it's a blob field, 
	   for which we need to find at least valid ASN.1 data */
	if( ( flags & ATTR_FLAG_BLOB_PAYLOAD ) && \
		!isBlobField( attributeInfoPtr->fieldType ) )
		return( CRYPT_OK );

	/* Perform any special-case checking that may be required */
	switch( attributeInfoPtr->fieldType )
		{
		case FIELDTYPE_BLOB_ANY:
			{
			/* It's a blob field, make sure that it's a valid ASN.1 object */
			status = checkCertObjectEncoding( data, dataLength );
			if( cryptStatusError( status ) )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				return( CRYPT_ARGERROR_STR1 );
				}
			break;
			}

		case FIELDTYPE_BLOB_BITSTRING:
		case FIELDTYPE_BLOB_SEQUENCE:
			{
			const int firstByte = byteToInt( ( ( BYTE * ) data )[ 0 ] );
			const int requiredByte = \
					( attributeInfoPtr->fieldType == FIELDTYPE_BLOB_BITSTRING ) ? \
					  BER_BITSTRING : BER_SEQUENCE;

			/* Typed blobs have a bit more information available than just 
			   "it must be a valid ASN.1 object" so we can check that it's 
			   the correct type of object as well as the ASN.1 validity 
			   check */
			if( firstByte != requiredByte )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				return( CRYPT_ARGERROR_STR1 );
				}
			status = checkCertObjectEncoding( data, dataLength );
			if( cryptStatusError( status ) )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				return( CRYPT_ARGERROR_STR1 );
				}
			break;
			}

		case BER_STRING_NUMERIC:
			{
			const BYTE *dataPtr = data;
			int i, LOOP_ITERATOR;

			/* Make sure that it's a numeric string */
			LOOP_EXT( i = 0, i < dataLength, i++, MAX_ATTRIBUTE_SIZE + 1 )
				{
				if( !isDigit( dataPtr[ i ] ) )
					{
					*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
					return( CRYPT_ARGERROR_STR1 );
					}
				}
			ENSURES( LOOP_BOUND_OK );
			break;
			}

		case BER_STRING_IA5:
		case BER_STRING_ISO646:
		case BER_STRING_PRINTABLE:
			/* Make sure that it's an ASCII string of the correct type */
			if( !checkTextStringData( data, dataLength, 
					( attributeInfoPtr->fieldType == BER_STRING_PRINTABLE ) ? \
					TRUE : FALSE ) )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				return( CRYPT_ARGERROR_STR1 );
				}
			break;
		}

	/* Finally, if there's a special-case validation function present, make
	   sure that the value is valid */
	if( attributeInfoPtr->extraData != NULL )
		{
		VALIDATION_FUNCTION validationFunction = \
					( VALIDATION_FUNCTION ) attributeInfoPtr->extraData;
		ATTRIBUTE_LIST checkAttribute;

		/* Since the validation function expects an ATTRIBUTE_LIST entry,
		   we have to create a dummy entry in order to check the raw
		   attribute components */
		memset( &checkAttribute, 0, sizeof( ATTRIBUTE_LIST ) );
		checkAttribute.attributeID = checkAttribute.fieldID = fieldID;
		checkAttribute.subFieldID = subFieldID;
		checkAttribute.dataValue = ( void * ) data;
		checkAttribute.dataValueLength = dataLength;
		INIT_FLAGS( checkAttribute.flags, flags );
		checkAttribute.fieldType = attributeInfoPtr->fieldType;
		DATAPTR_SET( checkAttribute.next, NULL );
		DATAPTR_SET( checkAttribute.prev, NULL );
		ENSURES( sanityCheckAttributePtr( &checkAttribute ) );
		*errorType = validationFunction( &checkAttribute );
		if( *errorType != CRYPT_ERRTYPE_NONE )
			return( CRYPT_ARGERROR_STR1 );
		}

	return( CRYPT_OK );
	}

/* Find the location in the attribute list at which to insert a new attribute 
   field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int findFieldInsertLocation( IN_OPT const DATAPTR_ATTRIBUTE attributePtr,
									OUT_PTR_COND ATTRIBUTE_LIST **insertPointPtrPtr,
									IN_ATTRIBUTE \
										const CRYPT_ATTRIBUTE_TYPE fieldID,
									IN_ATTRIBUTE_OPT \
										const CRYPT_ATTRIBUTE_TYPE subFieldID )
	{
	const ATTRIBUTE_LIST *insertPoint, *prevElement = NULL;
	int LOOP_ITERATOR;

	assert( isWritePtr( insertPointPtrPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES( isValidExtension( fieldID ) );
	REQUIRES( ( subFieldID == CRYPT_ATTRIBUTE_NONE ) || \
			  ( isValidExtension( fieldID ) ) );

	/* Clear return value */
	*insertPointPtrPtr = NULL;

	/* Find the location at which to insert this attribute field in a list 
	   sorted in order of fieldID.  Normally we use a LOOP_MAX(), however 
	   RPKI certificates, which are general-purpose certificates turned into 
	   pseudo-attribute certificates with monstrous lists of capabilities, 
	   can have much larger upper bounds.  To handle these we have to make a 
	   special exception for the upper bound of the loop sanity check */
#ifdef USE_RPKI
	LOOP_EXT_INITCHECK( insertPoint = DATAPTR_GET( attributePtr ), 
						insertPoint != NULL && \
							insertPoint->fieldID != CRYPT_ATTRIBUTE_NONE && \
							insertPoint->fieldID <= fieldID,
						FAILSAFE_ITERATIONS_LARGE * 10 )
#else
	LOOP_LARGE_INITCHECK( insertPoint = DATAPTR_GET( attributePtr ), 
						  insertPoint != NULL && \
							insertPoint->fieldID != CRYPT_ATTRIBUTE_NONE && \
							insertPoint->fieldID <= fieldID )
#endif /* USE_RPKI */
		{
		const ATTRIBUTE_LIST *insertPointNext = \
								DATAPTR_GET( insertPoint->next );

		REQUIRES( sanityCheckAttributePtr( insertPoint ) );
		ENSURES( insertPointNext == NULL || \
				 !isValidAttributeField( insertPointNext ) || \
				 insertPoint->attributeID <= insertPointNext->attributeID );

		/* If it's a composite field that can have multiple fields with the 
		   same field ID (e.g. a GeneralName), exit if the overall field ID 
		   is greater (which means that the component belongs to a different 
		   field entirely) or if the field ID is the same and the subfield 
		   ID is greater (which means that the component belongs to a 
		   different subfield within the field) */
		if( subFieldID != CRYPT_ATTRIBUTE_NONE && \
			insertPoint->fieldID == fieldID && \
			insertPoint->subFieldID > subFieldID )
			break;

		prevElement = insertPoint;
		insertPoint = insertPointNext;
		}
	ENSURES( LOOP_BOUND_OK );
	*insertPointPtrPtr = ( ATTRIBUTE_LIST * ) prevElement;
	
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Add Attribute Data							*
*																			*
****************************************************************************/

/* Add a blob-type attribute to a list of attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3, 6 ) ) \
int addAttribute( IN_ATTRIBUTE const ATTRIBUTE_TYPE attributeType,
				  INOUT DATAPTR_ATTRIBUTE *listHeadPtr, 
				  IN_BUFFER( oidLength ) const BYTE *oid, 
				  IN_LENGTH_OID const int oidLength,
				  const BOOLEAN critical, 
				  IN_BUFFER( dataLength ) const void *data, 
				  IN_LENGTH_SHORT const int dataLength, 
				  IN_FLAGS_Z( ATTR ) const int flags )
	{
	DATAPTR_ATTRIBUTE listHead = *listHeadPtr;
	ATTRIBUTE_LIST *newElement, *insertPoint = NULL;

	assert( isWritePtr( listHeadPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtrDynamic( oid, oidLength ) );
	assert( isReadPtrDynamic( data, dataLength ) );
	assert( ( flags & ( ATTR_FLAG_IGNORED | ATTR_FLAG_BLOB ) ) || \
			cryptStatusOK( checkCertObjectEncoding( data, dataLength ) ) );

	REQUIRES( DATAPTR_ISVALID( listHead ) );
	REQUIRES( attributeType == ATTRIBUTE_CERTIFICATE || \
			  attributeType == ATTRIBUTE_CMS );
	REQUIRES( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE && \
			  oidLength == sizeofOID( oid ) );
	REQUIRES( critical == TRUE || critical == FALSE );
#ifdef USE_RPKI
	REQUIRES( data != NULL && isShortIntegerRangeNZ( dataLength ) );
#else
	REQUIRES( data != NULL && \
			  dataLength > 0 && dataLength <= MAX_ATTRIBUTE_SIZE );
#endif /* USE_RPKI */
assert( ( flags & ~( ATTR_FLAG_NONE | ATTR_FLAG_IGNORED | ATTR_FLAG_BLOB | ATTR_FLAG_MULTIVALUED ) ) == 0 );
	REQUIRES( flags == ATTR_FLAG_NONE || flags == ATTR_FLAG_BLOB || \
			  flags == ( ATTR_FLAG_BLOB | ATTR_FLAG_IGNORED ) );

	/* If this attribute type is already handled as a non-blob attribute,
	   don't allow it to be added as a blob as well.  This avoids problems
	   with the same attribute being added twice, once as a blob and once as
	   a non-blob.  In addition it forces the caller to use the (recommended)
	   normal attribute handling mechanism, which allows for proper type
	   checking */
	if( !( flags & ATTR_FLAG_BLOB ) && \
		oidToAttribute( attributeType, oid, oidLength ) != NULL )
		return( CRYPT_ERROR_PERMISSION );

	/* Find the correct place in the list to insert the new element */
	if( DATAPTR_ISSET( listHead ) )
		{
		ATTRIBUTE_LIST *prevElement = NULL;
		int LOOP_ITERATOR;

		LOOP_LARGE( insertPoint = DATAPTR_GET( listHead ), 
					insertPoint != NULL, 
					insertPoint = DATAPTR_GET( insertPoint->next ) )
			{
			REQUIRES( sanityCheckAttributePtr( insertPoint ) );

			/* Make sure that this blob attribute isn't already present */
			if( checkAttributeListProperty( insertPoint, 
											ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) && \
				oidLength == sizeofOID( insertPoint->oid ) && \
				!memcmp( insertPoint->oid, oid, oidLength ) )
				return( CRYPT_ERROR_INITED );

			prevElement = insertPoint;
			}
		ENSURES( LOOP_BOUND_OK );
		insertPoint = prevElement;
		}

	/* Allocate memory for the new element and copy the information across.  
	   The data is stored in storage ... storage + dataLength, the OID in
	   storage + dataLength ... storage + dataLength + oidLength */
	REQUIRES( rangeCheck( dataLength, 1, MAX_INTLENGTH_SHORT ) );
	REQUIRES( rangeCheck( oidLength, MIN_OID_SIZE, MAX_OID_SIZE ) );
	if( ( newElement = ( ATTRIBUTE_LIST * ) \
					   clAlloc( "addAttribute", sizeof( ATTRIBUTE_LIST ) + \
												dataLength + oidLength ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	initVarStruct( newElement, ATTRIBUTE_LIST, dataLength + oidLength, dataValue );
	newElement->oid = newElement->storage + dataLength;
	memcpy( newElement->oid, oid, oidLength );
	INIT_FLAGS( newElement->flags,
				( flags & ATTR_FLAG_IGNORED ) | \
				( critical ? ATTR_FLAG_CRITICAL : ATTR_FLAG_NONE ) );
	memcpy( newElement->dataValue, data, dataLength );
	newElement->dataValueLength = dataLength;
	DATAPTR_SET( newElement->prev, NULL );
	DATAPTR_SET( newElement->next, NULL );
	ENSURES( sanityCheckAttributePtr( newElement ) );
	insertDoubleListElement( listHeadPtr, insertPoint, newElement, 
							 ATTRIBUTE_LIST );

	return( CRYPT_OK );
	}

/* Add an attribute field to a list of attributes at the appropriate 
   location */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6, 7 ) ) \
int addAttributeField( INOUT DATAPTR_ATTRIBUTE *listHeadPtr,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
					   IN_ATTRIBUTE_OPT \
							const CRYPT_ATTRIBUTE_TYPE subFieldID,
					   const int value,
					   IN_FLAGS_Z( ATTR ) const int flags, 
					   OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
					   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType )
	{
	const ATTRIBUTE_TYPE attributeType = \
							( fieldID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
							ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE;
	const ATTRIBUTE_INFO *attributeInfoPtr;
	CRYPT_ATTRIBUTE_TYPE attributeID;
	DATAPTR_ATTRIBUTE listHead = *listHeadPtr;
	ATTRIBUTE_LIST *newElement, *insertPoint;
	int status;

	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISVALID( listHead ) );
	REQUIRES( isValidExtension( fieldID ) );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );
assert( ( flags & ~( ATTR_FLAG_CRITICAL | ATTR_FLAG_MULTIVALUED | ATTR_FLAG_BLOB_PAYLOAD ) ) == 0 );
	REQUIRES( isFlagRangeZ( flags, ATTR ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Check the field's validity */
	attributeInfoPtr = fieldIDToAttribute( attributeType, fieldID, subFieldID, 
										   &attributeID );
	REQUIRES( attributeInfoPtr != NULL );
	status = checkAttributeField( listHead, attributeInfoPtr, fieldID, 
								  subFieldID, value, flags, errorType );
	if( cryptStatusError( status ) )
		{
		if( *errorType != CRYPT_ERRTYPE_NONE )
			{
			/* If we encountered an error that sets the error type, record 
			   the locus as well */
			*errorLocus = fieldID;
			}
		return( status );
		}

	/* Find the location at which to insert this attribute field */
	status = findFieldInsertLocation( listHead, &insertPoint, fieldID, 
									  subFieldID );
	ENSURES( cryptStatusOK( status ) );

	/* Allocate memory for the new element and copy the information across */
	if( ( newElement = ( ATTRIBUTE_LIST * ) \
					   clAlloc( "addAttributeField", \
								sizeof( ATTRIBUTE_LIST ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( newElement, 0, sizeof( ATTRIBUTE_LIST ) );
	newElement->attributeID = attributeID;
	newElement->fieldID = fieldID;
	newElement->subFieldID = subFieldID;
	INIT_FLAGS( newElement->flags, flags );
	newElement->fieldType = attributeInfoPtr->fieldType;
	DATAPTR_SET( newElement->prev, NULL );
	DATAPTR_SET( newElement->next, NULL );
	switch( attributeInfoPtr->fieldType )
		{
		case BER_BOOLEAN:
			/* Force it to the correct type if it's a boolean */
			newElement->intValue = value ? TRUE : FALSE;
			break;

		case BER_INTEGER:
		case BER_ENUMERATED:
		case BER_BITSTRING:
		case BER_NULL:
			newElement->intValue = value;
			break;

		case FIELDTYPE_CHOICE:
			/* The value for a CHOICE is used to select an entry within a 
			   subtype table rather than being encoded directly, see the
			   comments in checkAttributeEntry() for details */
			REQUIRES( value > 0 && value < 100 );
			newElement->intValue = value;
			break;

		case FIELDTYPE_DN:
			/* This type is present in both addAttributeField() and 
			   addAttributeFieldString(), when creating a new certificate 
			   this is a placeholder to indicate that a DN structure is being 
			   instantiated (value == CRYPT_UNUSED) and when reading an 
			   encoded certificate this is the decoded DN structure 
			   (data == DATAPTR_DN) */
			DATAPTR_SET( newElement->dnValue, NULL );
			ENSURES( value == CRYPT_UNUSED );
			break;

		case FIELDTYPE_IDENTIFIER:
			/* This is a placeholder entry with no explicit value */
			newElement->intValue = CRYPT_UNUSED;
			break;

		}
	ENSURES( sanityCheckAttributePtr( newElement ) );
	insertDoubleListElement( listHeadPtr, insertPoint, newElement, 
							 ATTRIBUTE_LIST );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 7, 8 ) ) \
int addAttributeFieldString( INOUT DATAPTR_ATTRIBUTE *listHeadPtr,
							 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
							 IN_ATTRIBUTE_OPT \
								const CRYPT_ATTRIBUTE_TYPE subFieldID,
							 IN_BUFFER( dataLength ) const void *data, 
							 IN_LENGTH_ATTRIBUTE const int dataLength,
							 IN_FLAGS_Z( ATTR ) const int flags, 
							 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType )
	{
	const ATTRIBUTE_TYPE attributeType = \
							( fieldID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
							ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE;
	const ATTRIBUTE_INFO *attributeInfoPtr;
	CRYPT_ATTRIBUTE_TYPE attributeID;
	DATAPTR_ATTRIBUTE listHead = *listHeadPtr;
	ATTRIBUTE_LIST *newElement, *insertPoint;
	int storageSize = 0, newDataLength, status;

	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( isReadPtrDynamic( data, dataLength ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISVALID( listHead ) );
	REQUIRES( isValidExtension( fieldID ) );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );
	REQUIRES( dataLength > 0 && dataLength <= MAX_ATTRIBUTE_SIZE );
assert( ( flags & ~( ATTR_FLAG_BLOB_PAYLOAD | ATTR_FLAG_CRITICAL | ATTR_FLAG_MULTIVALUED ) ) == 0 );
	REQUIRES( isFlagRangeZ( flags, ATTR ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Check the field's validity */
	attributeInfoPtr = fieldIDToAttribute( attributeType, fieldID, subFieldID, 
										   &attributeID );
	REQUIRES( attributeInfoPtr != NULL );
	status = checkAttributeFieldString( listHead, attributeInfoPtr, fieldID, 
										subFieldID, data, dataLength, flags, 
										&newDataLength, errorType );
	if( cryptStatusError( status ) )
		{
		if( *errorType != CRYPT_ERRTYPE_NONE )
			{
			/* If we encountered an error that sets the error type, record 
			   the locus as well */
			*errorLocus = fieldID;
			}
		return( status );
		}

	/* Find the location at which to insert this attribute field */
	status = findFieldInsertLocation( listHead, &insertPoint, fieldID, 
									  subFieldID );
	ENSURES( cryptStatusOK( status ) );

	/* Allocate memory for the new element and copy the information across */
	if( newDataLength != 0 )
		{
		REQUIRES( attributeInfoPtr->fieldType == BER_OBJECT_IDENTIFIER );

		/* The length has changed due to data en/decoding, use the 
		   en/decoded length for the storage size */
		storageSize = newDataLength;
		}
	else
		{
		/* If it's not a DN pointer then we copy the data in as is */
		if( attributeInfoPtr->fieldType != FIELDTYPE_DN )
			storageSize = dataLength;
		}
	REQUIRES( storageSize == 0 || \
			  rangeCheck( storageSize, 1, MAX_ATTRIBUTE_SIZE ) );
	if( ( newElement = ( ATTRIBUTE_LIST * ) \
					   clAlloc( "addAttributeField", sizeof( ATTRIBUTE_LIST ) + \
													 storageSize ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	initVarStruct( newElement, ATTRIBUTE_LIST, storageSize, dataValue );
	newElement->attributeID = attributeID;
	newElement->fieldID = fieldID;
	newElement->subFieldID = subFieldID;
	INIT_FLAGS( newElement->flags, flags );
	newElement->fieldType = attributeInfoPtr->fieldType;
	DATAPTR_SET( newElement->prev, NULL );
	DATAPTR_SET( newElement->next, NULL );
	switch( attributeInfoPtr->fieldType )
		{
		case BER_OBJECT_IDENTIFIER:
			/* If it's a BER/DER-encoded OID copy it in as is, otherwise 
			   convert it from the text form */
			if( ( ( BYTE * ) data )[ 0 ] == BER_OBJECT_IDENTIFIER )
				{
				memcpy( newElement->dataValue, data, dataLength );
				newElement->dataValueLength = dataLength;
				}
			else
				{
				status = textToOID( data, dataLength, newElement->dataValue, 
									storageSize, 
									&newElement->dataValueLength );
				ENSURES( cryptStatusOK( status ) );
				}
			break;

		case FIELDTYPE_DN:
			{
			DATAPTR_DN dn = *( ( DATAPTR_DN  * ) data );

			REQUIRES( DATAPTR_ISSET( dn ) );

			/* This type is present in both addAttributeField() and 
			   addAttributeFieldString(), when creating a new certificate 
			   this is a placeholder to indicate that a DN structure is being 
			   instantiated (value == CRYPT_UNUSED) and when reading an 
			   encoded certificate this is the decoded DN structure 
			   (data == DATAPTR_DN *) */
			newElement->dnValue = dn;
			break;
			}

		default:
			memcpy( newElement->dataValue, data, dataLength );
			newElement->dataValueLength = dataLength;
			break;
		}
	ENSURES( sanityCheckAttributePtr( newElement ) );
	insertDoubleListElement( listHeadPtr, insertPoint, newElement, 
							 ATTRIBUTE_LIST );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Delete Attribute Data						*
*																			*
****************************************************************************/

/* Delete an attribute/attribute field from a list of attributes, updating
   the list DN cursor at the same time.  This is a somewhat ugly kludge, 
   it's not really possible to do this cleanly since deleting attributes 
   affects the attribute and DN cursors as well */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteAttributeField( INOUT DATAPTR_ATTRIBUTE *attributePtr,
						  INOUT_OPT DATAPTR_ATTRIBUTE *cursorPtr,
						  INOUT DATAPTR_ATTRIBUTE listItem,
						  INOUT_OPT DATAPTR_DN *dnCursor )
	{
	const DATAPTR_ATTRIBUTE cursor = ( cursorPtr == NULL ) ? \
									 DATAPTR_NULL : *cursorPtr;
	ATTRIBUTE_LIST *listItemPtr;

	assert( isWritePtr( attributePtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( cursorPtr == NULL || \
			isWritePtr( cursorPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( dnCursor == NULL || \
			isReadPtr( dnCursor, sizeof( DATAPTR_DN ) ) );

	REQUIRES( DATAPTR_ISVALID( cursor ) );
	REQUIRES( DATAPTR_ISSET( listItem ) );

	listItemPtr = DATAPTR_GET( listItem );
	REQUIRES( listItemPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( listItemPtr ) );

	/* If we're about to delete the field that's pointed to by the attribute 
	   cursor, advance the cursor to the next field.  If there's no next 
	   field, move it to the previous field.  This behaviour is the most
	   logically consistent, it means that we can do things like deleting an
	   entire attribute list by repeatedly deleting a field */
	if( DATAPTR_ISSET( cursor ) && DATAPTR_SAME( cursor, listItem ) )
		{
		ANALYSER_HINT( cursorPtr != NULL );	/* From DATAPTR_ISSET( cursor ) */

		if( DATAPTR_ISSET( listItemPtr->next ) )
			*cursorPtr = listItemPtr->next;
		else
			*cursorPtr = listItemPtr->prev;
		}

	/* Remove the item from the list */
	deleteDoubleListElement( attributePtr, listItemPtr, ATTRIBUTE_LIST );

	/* Clear all data in the item and free the memory */
	if( listItemPtr->fieldType == FIELDTYPE_DN )
		{
		const DATAPTR_ATTRIBUTE dn = ( dnCursor == NULL ) ? \
									 DATAPTR_NULL : *dnCursor;
		DATAPTR_ATTRIBUTE listItemDN = GET_DN_POINTER( listItemPtr );

		REQUIRES( DATAPTR_ISVALID( dn ) );
		REQUIRES( DATAPTR_ISVALID( listItemDN ) );

		/* If we've deleted the DN at the current cursor position, clear the
		   DN cursor */
		if( DATAPTR_ISSET( dn ) && DATAPTR_SAME( dn, listItemDN ) )
			{
			ANALYSER_HINT( dnCursor != NULL );	/* From DATAPTR_ISSET( dn ) */

			DATAPTR_SET_PTR( dnCursor, NULL );
			}
		deleteDN( &listItemDN );
		}
	endVarStruct( listItemPtr, ATTRIBUTE_LIST );
	clFree( "deleteAttributeField", listItemPtr );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int deleteCompositeAttributeField( INOUT DATAPTR_ATTRIBUTE *attributePtr,
								   INOUT DATAPTR_ATTRIBUTE *cursorPtr,
								   INOUT DATAPTR_ATTRIBUTE listItem,
								   INOUT DATAPTR_DN *dnCursor )
	{
	CRYPT_ATTRIBUTE_TYPE attributeID, fieldID;
	DATAPTR_ATTRIBUTE attribute = *attributePtr;
	const DATAPTR_ATTRIBUTE cursor = ( cursorPtr == NULL ) ? \
									 DATAPTR_NULL : *cursorPtr;
	ATTRIBUTE_LIST *listItemPtr, *attributeListCursor;
	int LOOP_ITERATOR;

	assert( isWritePtr( attributePtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( cursorPtr == NULL || \
			isWritePtr( cursorPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( dnCursor == NULL || \
			isReadPtr( dnCursor, sizeof( DATAPTR_DN ) ) );

	REQUIRES( DATAPTR_ISVALID( attribute ) );
	REQUIRES( DATAPTR_ISVALID( cursor ) );
	REQUIRES( DATAPTR_ISSET( listItem ) );

	listItemPtr = DATAPTR_GET( listItem );
	REQUIRES( listItemPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( listItemPtr ) );
	attributeID = listItemPtr->attributeID;
	fieldID = listItemPtr->fieldID;

	/* Delete an attribute field that contains one or more entries of a 
	   subfield.  This is typically used with GeneralName fields, where the 
	   fieldID for the GeneralName is further divided into DN, URI, email 
	   address, and so on, subfields */
	LOOP_MED_INITCHECK( attributeListCursor = listItemPtr, 
						attributeListCursor != NULL && \
							attributeListCursor->attributeID == attributeID && \
							attributeListCursor->fieldID == fieldID )
		{
		DATAPTR_ATTRIBUTE itemToFree;
		int status;

		REQUIRES( sanityCheckAttributePtr( attributeListCursor ) );

		DATAPTR_SET( itemToFree, attributeListCursor );
		attributeListCursor = DATAPTR_GET( attributeListCursor->next );
		status = deleteAttributeField( attributePtr, cursorPtr, itemToFree, 
									   dnCursor );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int deleteAttribute( INOUT DATAPTR_ATTRIBUTE *attributePtr,
					 INOUT DATAPTR_ATTRIBUTE *cursorPtr,
					 INOUT DATAPTR_ATTRIBUTE listItem,
					 INOUT DATAPTR_DN *dnCursor )
	{
	CRYPT_ATTRIBUTE_TYPE attributeID;
	const ATTRIBUTE_LIST *listItemPtr, *attributeListCursor;
	int LOOP_ITERATOR;

	assert( isWritePtr( attributePtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( isWritePtr( cursorPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( isWritePtr( dnCursor, sizeof( DATAPTR_DN ) ) );

	REQUIRES( DATAPTR_ISSET( listItem ) );

	/* If it's a blob-type attribute, everything is contained in this one
	   list item so we only need to destroy that */
	if( checkAttributeProperty( listItem, ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
		{
		return( deleteAttributeField( attributePtr, cursorPtr, listItem, 
									  NULL ) );
		}

	/* Complete attributes should be deleted with deleteCompleteAttribute() */
	assert( !checkAttributeProperty( listItem, 
									 ATTRIBUTE_PROPERTY_COMPLETEATRIBUTE ) );

	/* Find the start of the fields in this attribute */
	listItemPtr = DATAPTR_GET( listItem );
	ENSURES( listItemPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( listItemPtr ) );
	attributeListCursor = findAttributeStart( listItemPtr );
	ENSURES( attributeListCursor != NULL );
	REQUIRES( sanityCheckAttributePtr( attributeListCursor ) );
	attributeID = attributeListCursor->attributeID;

	/* It's an item with multiple fields, destroy each field separately */
	LOOP_LARGE_CHECK( attributeListCursor != NULL && \
					  attributeListCursor->attributeID == attributeID )
		{
		DATAPTR_ATTRIBUTE itemToFree;
		int status;

		REQUIRES( sanityCheckAttributePtr( attributeListCursor ) );

		DATAPTR_SET( itemToFree, ( ATTRIBUTE_LIST * ) attributeListCursor );
		attributeListCursor = DATAPTR_GET( attributeListCursor->next );
		status = deleteAttributeField( attributePtr, cursorPtr, itemToFree, 
									   dnCursor );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int deleteCompleteAttribute( INOUT DATAPTR_ATTRIBUTE *attributePtr,
							 INOUT DATAPTR_ATTRIBUTE *cursorPtr,
							 const CRYPT_ATTRIBUTE_TYPE attributeID,
							 INOUT DATAPTR_DN *dnCursor )
	{
	DATAPTR_ATTRIBUTE attribute = *attributePtr, listItem;
	const DATAPTR_ATTRIBUTE cursor = ( cursorPtr == NULL ) ? \
									 DATAPTR_NULL : *cursorPtr;
	ATTRIBUTE_LIST *attributeListCursor, *attributeListCursorNext;
	int LOOP_ITERATOR;

	assert( isWritePtr( attributePtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( isWritePtr( cursorPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( isReadPtr( dnCursor, sizeof( DATAPTR_DN ) ) );

	REQUIRES( DATAPTR_ISVALID( attribute ) );
	REQUIRES( DATAPTR_ISVALID( cursor ) );
	REQUIRES( isValidExtension( attributeID ) );

	/* We're deleting an entire (constructed) attribute that won't have an 
	   entry in the list so we find the first field of the constructed 
	   attribute that's present in the list and delete that */
	LOOP_LARGE( attributeListCursor = DATAPTR_GET( attribute ), 
				attributeListCursor != NULL && \
					attributeListCursor->attributeID != attributeID,
				attributeListCursor = DATAPTR_GET( attributeListCursor->next ) );
	ENSURES( LOOP_BOUND_OK );
	ENSURES( attributeListCursor != NULL );
	REQUIRES( sanityCheckAttributePtr( attributeListCursor ) );
	attributeListCursorNext = DATAPTR_GET( attributeListCursor->next );
	ENSURES( attributeListCursorNext == NULL || \
			 attributeListCursorNext->attributeID != \
								attributeListCursor->attributeID );
	DATAPTR_SET( listItem, attributeListCursor );
	return( deleteAttributeField( attributePtr, cursorPtr, listItem, 
								  dnCursor ) );
	}

/* Delete a complete set of attributes */

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteAttributes( INOUT DATAPTR_ATTRIBUTE *attributePtr )
	{
	DATAPTR_ATTRIBUTE attributeCursor = *attributePtr;
	ATTRIBUTE_LIST *attributeListCursor;
	int LOOP_ITERATOR;

	assert( isWritePtr( attributePtr, sizeof( DATAPTR_ATTRIBUTE ) ) );

	REQUIRES_V( DATAPTR_ISVALID( attributeCursor ) );

	/* If the list was empty, return now */
	if( DATAPTR_ISNULL( attributeCursor ) )
		return;

	/* Destroy any remaining list items.  See the comment in 
	   findFieldInsertLocation() for the RPKI loop bound */
#ifdef USE_RPKI
	LOOP_EXT_INITCHECK( attributeListCursor = DATAPTR_GET( attributeCursor ),
						attributeListCursor != NULL, 
						FAILSAFE_ITERATIONS_LARGE * 10 )
#else
	LOOP_LARGE_INITCHECK( attributeListCursor = DATAPTR_GET( attributeCursor ),
						  attributeListCursor != NULL )
#endif /* USE_RPKI */
		{
		DATAPTR_ATTRIBUTE itemToFree;

		REQUIRES_V( sanityCheckAttributePtr( attributeListCursor ) );

		DATAPTR_SET( itemToFree, ( ATTRIBUTE_LIST * ) attributeListCursor );
		attributeListCursor = DATAPTR_GET( attributeListCursor->next );
		( void ) deleteAttributeField( attributePtr, NULL, itemToFree, NULL );
		}
	ENSURES_V( LOOP_BOUND_OK );
	ENSURES_V( DATAPTR_ISNULL_PTR( attributePtr ) );
	}
#endif /* USE_CERTIFICATES */
