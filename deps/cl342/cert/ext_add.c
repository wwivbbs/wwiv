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

/* Normally we use FAILSAFE_ITERATIONS_LARGE as the upper bound on loop
   iterations, however RPKI certificates, which are general-purpose 
   certificates turned into pseudo-attribute certificates with monstrous 
   lists of capabilities, can have much larger upper bounds.  To handle 
   these we have to make a special exception for the upper bound of the 
   loop sanity check */

#define FAILSAFE_ITERATIONS_LARGE_SPECIAL	( FAILSAFE_ITERATIONS_LARGE * 10 )

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Check the validity of an attribute field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 7 ) ) \
static int checkAttributeField( IN_OPT const ATTRIBUTE_LIST *attributeListPtr,
								const ATTRIBUTE_INFO *attributeInfoPtr,
								IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
								IN_ATTRIBUTE_OPT \
									const CRYPT_ATTRIBUTE_TYPE subFieldID,
								IN_INT_Z const int value,
								IN_FLAGS( ATTR ) const int flags, 
								OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	assert( attributeListPtr == NULL || \
			isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  fieldID <= CRYPT_CERTINFO_LAST );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );
assert( ( flags & ~( ATTR_FLAG_NONE | ATTR_FLAG_CRITICAL | ATTR_FLAG_MULTIVALUED ) ) == 0 );
	REQUIRES( flags >= ATTR_FLAG_NONE && flags <= ATTR_FLAG_MAX );

	/* Clear return value */
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Make sure that a valid field has been specified and that this field
	   isn't already present as a non-default entry unless it's a field for
	   which multiple values are allowed */
	if( attributeInfoPtr == NULL )
		return( CRYPT_ARGERROR_VALUE );
	if( attributeListPtr != NULL && \
		findAttributeField( attributeListPtr, fieldID, subFieldID ) != NULL )
		{
		/* If it's not multivalued, we can't have any duplicate fields */
		if( !( ( attributeInfoPtr->encodingFlags & FL_MULTIVALUED ) || \
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
static int checkAttributeFieldString( IN_OPT const ATTRIBUTE_LIST *attributeListPtr,
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

	assert( attributeListPtr == NULL || \
			isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );
	assert( isWritePtr( newDataLength, sizeof( int ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  fieldID <= CRYPT_CERTINFO_LAST );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );
	REQUIRES( dataLength > 0 && dataLength <= MAX_ATTRIBUTE_SIZE );
assert( ( flags & ~( ATTR_FLAG_NONE | ATTR_FLAG_BLOB_PAYLOAD | ATTR_FLAG_CRITICAL | ATTR_FLAG_MULTIVALUED ) ) == 0 );
	REQUIRES( flags >= ATTR_FLAG_NONE && flags <= ATTR_FLAG_MAX );

	/* Clear return value */
	*newDataLength = 0;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Make sure that a valid field has been specified and that this field
	   isn't already present as a non-default entry unless it's a field for
	   which multiple values are allowed */
	if( attributeInfoPtr == NULL )
		return( CRYPT_ARGERROR_VALUE );
	if( attributeListPtr != NULL && \
		findAttributeField( attributeListPtr, fieldID, subFieldID ) != NULL )
		{
		/* If it's not multivalued, we can't have any duplicate fields */
		if( !( ( attributeInfoPtr->encodingFlags & FL_MULTIVALUED ) || \
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
	   any old rubbish into the fields exit now unless it's a blob field, 
	   for which we need to find at least valid ASN.1 data */
	if( ( flags & ATTR_FLAG_BLOB_PAYLOAD ) && \
		!isBlobField( attributeInfoPtr->fieldType ) )
		return( CRYPT_OK );

	/* Perform any special-case checking that may be required */
	switch( attributeInfoPtr->fieldType )
		{
		case FIELDTYPE_BLOB_ANY:
			/* It's a blob field, make sure that it's a valid ASN.1 object */
			status = checkObjectEncoding( data, dataLength );
			if( cryptStatusError( status ) )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				return( CRYPT_ARGERROR_STR1 );
				}
			break;

		case FIELDTYPE_BLOB_BITSTRING:
		case FIELDTYPE_BLOB_SEQUENCE:
			{
			const int firstByte = ( ( BYTE * ) data )[ 0 ];

			/* Typed blobs have a bit more information available than just 
			   "it must be a valid ASN.1 object" so we can check that it's 
			   the correct type of object as well as the ASN.1 validity 
			   check */
			if( attributeInfoPtr->fieldType == FIELDTYPE_BLOB_BITSTRING )
				{
				if( firstByte != BER_BITSTRING )
					{
					*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
					return( CRYPT_ARGERROR_STR1 );
					}
				}
			else
				{
				if( firstByte != BER_SEQUENCE )
					{
					*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
					return( CRYPT_ARGERROR_STR1 );
					}
				}
			status = checkObjectEncoding( data, dataLength );
			if( cryptStatusError( status ) )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				return( CRYPT_ARGERROR_STR1 );
				}
			break;
			}

		case BER_STRING_NUMERIC:
			{
			const char *dataPtr = data;
			int i;

			/* Make sure that it's a numeric string */
			for( i = 0; i < dataLength; i++ )
				{
				if( !isDigit( dataPtr[ i ] ) )
					{
					*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
					return( CRYPT_ARGERROR_STR1 );
					}
				}
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

	/* Finally, if there's any special-case validation function present, make
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
		checkAttribute.fieldID = fieldID;
		checkAttribute.subFieldID = subFieldID;
		checkAttribute.value = ( void * ) data;
		checkAttribute.valueLength = dataLength;
		checkAttribute.flags = flags;
		checkAttribute.fieldType = attributeInfoPtr->fieldType;
		*errorType = validationFunction( &checkAttribute );
		if( *errorType != CRYPT_ERRTYPE_NONE )
			return( CRYPT_ARGERROR_STR1 );
		}

	return( CRYPT_OK );
	}

/* Find the location in the attribute list at which to insert a new attribute 
   field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int findFieldInsertLocation( IN_OPT const ATTRIBUTE_LIST *attributeListPtr,
									OUT_PTR_COND ATTRIBUTE_LIST **insertPointPtrPtr,
									IN_ATTRIBUTE \
										const CRYPT_ATTRIBUTE_TYPE fieldID,
									IN_ATTRIBUTE_OPT \
										const CRYPT_ATTRIBUTE_TYPE subFieldID )
	{
	const ATTRIBUTE_LIST *insertPoint, *prevElement = NULL;
	int iterationCount;

	assert( attributeListPtr == NULL || \
			isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( insertPointPtrPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  fieldID <= CRYPT_CERTINFO_LAST );
	REQUIRES( ( subFieldID == CRYPT_ATTRIBUTE_NONE ) || \
			  ( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				fieldID <= CRYPT_CERTINFO_LAST ) );

	/* Clear return value */
	*insertPointPtrPtr = NULL;

	/* Find the location at which to insert this attribute field in a list 
	   sorted in order of fieldID */
	for( insertPoint = attributeListPtr, iterationCount = 0;
		 insertPoint != NULL && \
			insertPoint->fieldID != CRYPT_ATTRIBUTE_NONE && \
			insertPoint->fieldID <= fieldID && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE_SPECIAL;
		 iterationCount++ )
		{
		ENSURES( insertPoint->next == NULL || \
				 !isValidAttributeField( insertPoint->next ) || \
				 insertPoint->attributeID <= insertPoint->next->attributeID );

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
		insertPoint = insertPoint->next;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE_SPECIAL );
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
				  INOUT ATTRIBUTE_PTR **listHeadPtr, 
				  IN_BUFFER( oidLength ) const BYTE *oid, 
				  IN_LENGTH_OID const int oidLength,
				  const BOOLEAN critical, 
				  IN_BUFFER( dataLength ) const void *data, 
				  IN_LENGTH_SHORT const int dataLength, 
				  IN_FLAGS_Z( ATTR ) const int flags )
	{
	ATTRIBUTE_LIST *newElement, *insertPoint = NULL;

	assert( isWritePtr( listHeadPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtr( oid, oidLength ) );
	assert( isReadPtr( data, dataLength ) );
	assert( ( flags & ( ATTR_FLAG_IGNORED | ATTR_FLAG_BLOB ) ) || \
			!cryptStatusError( checkObjectEncoding( data, dataLength ) ) );

	REQUIRES( attributeType == ATTRIBUTE_CERTIFICATE || \
			  attributeType == ATTRIBUTE_CMS );
	REQUIRES( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE && \
			  oidLength == sizeofOID( oid ) );
	REQUIRES( data != NULL && \
			  dataLength > 0 && dataLength <= MAX_INTLENGTH_SHORT );
			  /* This has to be MAX_INTLENGTH_SHORT rather than 
			     MAX_ATTRIBUTE_SIZE in order to handle RPKI certificates
				 with monster ACLs pretending to be attribute certificates */
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
	if( *listHeadPtr != NULL )
		{
		ATTRIBUTE_LIST *prevElement = NULL;
		int iterationCount;

		for( insertPoint = *listHeadPtr, iterationCount = 0; 
			 insertPoint != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE;
			 insertPoint = insertPoint->next, iterationCount++ )
			{
			/* Make sure that this blob attribute isn't already present */
			if( checkAttributeProperty( insertPoint, 
										ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) && \
				oidLength == sizeofOID( insertPoint->oid ) && \
				!memcmp( insertPoint->oid, oid, oidLength ) )
				return( CRYPT_ERROR_INITED );

			prevElement = insertPoint;
			}
		ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
		insertPoint = prevElement;
		}

	/* Allocate memory for the new element and copy the information across.  
	   The data is stored in storage ... storage + dataLength, the OID in
	   storage + dataLength ... storage + dataLength + oidLength */
	if( ( newElement = ( ATTRIBUTE_LIST * ) \
					   clAlloc( "addAttribute", sizeof( ATTRIBUTE_LIST ) + \
												dataLength + oidLength ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	initVarStruct( newElement, ATTRIBUTE_LIST, dataLength + oidLength );
	newElement->oid = newElement->storage + dataLength;
	memcpy( newElement->oid, oid, oidLength );
	newElement->flags = ( flags & ATTR_FLAG_IGNORED ) | \
						( critical ? ATTR_FLAG_CRITICAL : ATTR_FLAG_NONE );
	memcpy( newElement->value, data, dataLength );
	newElement->valueLength = dataLength;
	insertDoubleListElements( ( ATTRIBUTE_LIST ** ) listHeadPtr, 
							  insertPoint, newElement, newElement );

	return( CRYPT_OK );
	}

/* Add an attribute field to a list of attributes at the appropriate 
   location */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6, 7 ) ) \
int addAttributeField( INOUT ATTRIBUTE_PTR **listHeadPtr,
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
	CRYPT_ATTRIBUTE_TYPE attributeID;
	const ATTRIBUTE_INFO *attributeInfoPtr = fieldIDToAttribute( attributeType,
										fieldID, subFieldID, &attributeID );
	ATTRIBUTE_LIST **attributeListPtr = ( ATTRIBUTE_LIST ** ) listHeadPtr;
	ATTRIBUTE_LIST *newElement, *insertPoint;
	int status;

	assert( isWritePtr( listHeadPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  fieldID <= CRYPT_CERTINFO_LAST );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );
assert( ( flags & ~( ATTR_FLAG_CRITICAL | ATTR_FLAG_MULTIVALUED | ATTR_FLAG_BLOB_PAYLOAD ) ) == 0 );
	REQUIRES( flags >= ATTR_FLAG_NONE && flags <= ATTR_FLAG_MAX );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Sanity-check the state */
	ENSURES( attributeInfoPtr != NULL );

	/* Check the field's validity */
	status = checkAttributeField( *attributeListPtr, attributeInfoPtr, 
								  fieldID, subFieldID, value, flags, 
								  errorType );
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
	status = findFieldInsertLocation( *attributeListPtr, &insertPoint, 
									  fieldID, subFieldID );
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
	newElement->flags = flags;
	newElement->fieldType = attributeInfoPtr->fieldType;
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
		case FIELDTYPE_CHOICE:
			newElement->intValue = value;
			if( attributeInfoPtr->fieldType == FIELDTYPE_CHOICE )
				{
				/* For encoding purposes the subfield ID is set to the ID of 
				   the CHOICE selection.  This isn't strictly speaking a 
				   CRYPT_ATTRIBUTE_TYPE but something like a 
				   CRYPT_HOLDINSTRUCTION_TYPE or CRYPT_CONTENT_TYPE, but
				   we store it in a CRYPT_ATTRIBUTE_TYPE field */
				REQUIRES( newElement->intValue > 0 && \
						  newElement->intValue < 100 );

				newElement->subFieldID = ( CRYPT_ATTRIBUTE_TYPE ) \
										   newElement->intValue;
				}
			break;

		case FIELDTYPE_DN:
			/* This type is present in both addAttributeField() and 
			   addAttributeFieldString(), when creating a new certificate 
			   this is a placeholder to indicate that a DN structure is being 
			   instantiated (value == CRYPT_UNUSED) and when reading an 
			   encoded certificate this is the decoded DN structure 
			   (data == DN_PTR) */
			ENSURES( value == CRYPT_UNUSED );
			break;

		case FIELDTYPE_IDENTIFIER:
			/* This is a placeholder entry with no explicit value */
			newElement->intValue = CRYPT_UNUSED;
			break;

		}
	insertDoubleListElement( attributeListPtr, insertPoint, newElement );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 7, 8 ) ) \
int addAttributeFieldString( INOUT ATTRIBUTE_PTR **listHeadPtr,
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
	CRYPT_ATTRIBUTE_TYPE attributeID;
	const ATTRIBUTE_INFO *attributeInfoPtr = fieldIDToAttribute( attributeType,
										fieldID, subFieldID, &attributeID );
	ATTRIBUTE_LIST **attributeListPtr = ( ATTRIBUTE_LIST ** ) listHeadPtr;
	ATTRIBUTE_LIST *newElement, *insertPoint;
	int storageSize = 0, newDataLength, status;

	assert( isWritePtr( listHeadPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtr( data, dataLength ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  fieldID <= CRYPT_CERTINFO_LAST );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );
	REQUIRES( dataLength > 0 && dataLength <= MAX_ATTRIBUTE_SIZE );
assert( ( flags & ~( ATTR_FLAG_BLOB_PAYLOAD | ATTR_FLAG_CRITICAL | ATTR_FLAG_MULTIVALUED ) ) == 0 );
	REQUIRES( flags >= ATTR_FLAG_NONE && flags <= ATTR_FLAG_MAX );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Sanity-check the state */
	ENSURES( attributeInfoPtr != NULL );

	/* Check the field's validity */
	status = checkAttributeFieldString( *attributeListPtr, attributeInfoPtr, 
										fieldID, subFieldID, data, 
										dataLength, flags, &newDataLength, 
										errorType );
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
	status = findFieldInsertLocation( *attributeListPtr, &insertPoint, 
									  fieldID, subFieldID );
	ENSURES( cryptStatusOK( status ) );

	/* Allocate memory for the new element and copy the information across */
	if( newDataLength != 0 )
		{
		ENSURES( attributeInfoPtr->fieldType == BER_OBJECT_IDENTIFIER );

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
	if( ( newElement = ( ATTRIBUTE_LIST * ) \
					   clAlloc( "addAttributeField", sizeof( ATTRIBUTE_LIST ) + \
													 storageSize ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	initVarStruct( newElement, ATTRIBUTE_LIST, storageSize );
	newElement->attributeID = attributeID;
	newElement->fieldID = fieldID;
	newElement->subFieldID = subFieldID;
	newElement->flags = flags;
	newElement->fieldType = attributeInfoPtr->fieldType;
	switch( attributeInfoPtr->fieldType )
		{
		case BER_OBJECT_IDENTIFIER:
			/* If it's a BER/DER-encoded OID copy it in as is, otherwise 
			   convert it from the text form */
			if( ( ( BYTE * ) data )[ 0 ] == BER_OBJECT_IDENTIFIER )
				{
				memcpy( newElement->value, data, dataLength );
				newElement->valueLength = dataLength;
				}
			else
				{
				status = textToOID( data, dataLength, newElement->value, 
									storageSize, &newElement->valueLength );
				ENSURES( cryptStatusOK( status ) );
				}
			break;

		case FIELDTYPE_DN:
			/* This type is present in both addAttributeField() and 
			   addAttributeFieldString(), when creating a new certificate 
			   this is a placeholder to indicate that a DN structure is being 
			   instantiated (value == CRYPT_UNUSED) and when reading an 
			   encoded certificate this is the decoded DN structure 
			   (data == DN_PTR) */
			newElement->value = ( void * ) data;
			break;

		default:
			memcpy( newElement->value, data, dataLength );
			newElement->valueLength = dataLength;
			break;
		}
	insertDoubleListElement( attributeListPtr, insertPoint, newElement );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Delete Attribute Data						*
*																			*
****************************************************************************/

/* Delete an attribute/attribute field from a list of attributes, updating
   the list cursor at the same time.  This is a somewhat ugly kludge, it's
   not really possible to do this cleanly since deleting attributes affects
   the attribute cursor */

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int deleteAttributeField( INOUT ATTRIBUTE_PTR **attributePtr,
						  INOUT_OPT ATTRIBUTE_PTR **listCursorPtr,
						  INOUT ATTRIBUTE_PTR *listItemPtr,
						  IN_OPT const DN_PTR *dnCursor )
	{
	ATTRIBUTE_LIST *listItem = ( ATTRIBUTE_LIST * ) listItemPtr;
	ATTRIBUTE_LIST *listPrevPtr = listItem->prev;
	ATTRIBUTE_LIST *listNextPtr = listItem->next;
	BOOLEAN deletedDN = FALSE;

	assert( isWritePtr( attributePtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( *attributePtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( listCursorPtr == NULL || \
			isWritePtr( listCursorPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( listItem, sizeof( ATTRIBUTE_LIST ) ) );
	assert( dnCursor == NULL || \
			isReadPtr( dnCursor, sizeof( DN_PTR_STORAGE ) ) );

	/* If we're about to delete the field that's pointed to by the attribute 
	   cursor, advance the cursor to the next field.  If there's no next 
	   field, move it to the previous field.  This behaviour is the most
	   logically consistent, it means that we can do things like deleting an
	   entire attribute list by repeatedly deleting a field */
	if( listCursorPtr != NULL && *listCursorPtr == listItem )
		*listCursorPtr = ( listNextPtr != NULL ) ? listNextPtr : listPrevPtr;

	/* Remove the item from the list */
	deleteDoubleListElement( attributePtr, listItem );

	/* Clear all data in the item and free the memory */
	if( listItem->fieldType == FIELDTYPE_DN )
		{
		/* If we've deleted the DN at the current cursor position, remember
		   this so that we can warn the caller */
		if( dnCursor != NULL && dnCursor == &listItem->value )
			deletedDN = TRUE;
		deleteDN( ( DN_PTR ** ) &listItem->value );
		}
	endVarStruct( listItem, ATTRIBUTE_LIST );
	clFree( "deleteAttributeField", listItem );

	/* If we deleted the DN at the current cursor position return a 
	   special-case code to let the caller know */
	return( deletedDN ? OK_SPECIAL : CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int deleteCompositeAttributeField( INOUT ATTRIBUTE_PTR **attributePtr,
								   INOUT ATTRIBUTE_PTR **listCursorPtr,
								   INOUT ATTRIBUTE_PTR *listItemPtr,
								   IN_OPT const DN_PTR *dnCursor )
	{
	ATTRIBUTE_LIST *listItem = ( ATTRIBUTE_LIST * ) listItemPtr;
	const CRYPT_ATTRIBUTE_TYPE attributeID = listItem->attributeID;
	const CRYPT_ATTRIBUTE_TYPE fieldID = listItem->fieldID;
	ATTRIBUTE_LIST *attributeListCursor;
	BOOLEAN deletedDN = FALSE;
	int iterationCount;

	assert( isWritePtr( attributePtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( listCursorPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( listItem, sizeof( ATTRIBUTE_LIST ) ) );
	assert( dnCursor == NULL || \
			isReadPtr( dnCursor, sizeof( DN_PTR_STORAGE ) ) );

	/* Delete an attribute field that contains one or more entries of a 
	   subfield.  This is typically used with GeneralName fields, where the 
	   fieldID for the GeneralName is further divided into DN, URI, email 
	   address, and so on, subfields */
	for( attributeListCursor = listItem, \
			iterationCount = 0;
		 attributeListCursor != NULL && \
			attributeListCursor->attributeID == attributeID && \
			attributeListCursor->fieldID == fieldID && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		ATTRIBUTE_LIST *itemToFree = attributeListCursor;

		attributeListCursor = attributeListCursor->next;
		if( deleteAttributeField( attributePtr, listCursorPtr,
								  itemToFree, dnCursor ) == OK_SPECIAL )
			{
			/* If we've deleted the DN at the current cursor position, 
			   remember this so that we can warn the caller */
			deletedDN = TRUE;
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	/* If we deleted the DN at the current cursor position return a 
	   special-case code to let the caller know */
	return( deletedDN ? OK_SPECIAL : CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int deleteAttribute( INOUT ATTRIBUTE_PTR **attributeListPtr,
					 INOUT_OPT ATTRIBUTE_PTR **listCursorPtr,
					 INOUT ATTRIBUTE_PTR *listItemPtr,
					 IN_OPT const DN_PTR *dnCursor )
	{
	CRYPT_ATTRIBUTE_TYPE attributeID;
	ATTRIBUTE_LIST *listItem = ( ATTRIBUTE_LIST * ) listItemPtr;
	ATTRIBUTE_LIST *attributeListCursor;
	int iterationCount, status = CRYPT_OK;

	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( *attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( listCursorPtr == NULL || \
			isWritePtr( listCursorPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( listItem, sizeof( ATTRIBUTE_LIST ) ) );
	assert( dnCursor == NULL || \
			isReadPtr( dnCursor, sizeof( DN_PTR_STORAGE ) ) );

	/* If it's a blob-type attribute, everything is contained in this one
	   list item so we only need to destroy that */
	if( checkAttributeProperty( listItem, 
								ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
		{
		return( deleteAttributeField( attributeListPtr, listCursorPtr, 
									  listItem, NULL ) );
		}

	/* Complete attributes should be deleted with deleteCompleteAttribute() */
	assert( !checkAttributeProperty( listItem, 
									 ATTRIBUTE_PROPERTY_COMPLETEATRIBUTE ) );

	/* Find the start of the fields in this attribute */
	attributeListCursor = findAttributeStart( listItem );
	ENSURES( attributeListCursor != NULL );
	attributeID = attributeListCursor->attributeID;

	/* It's an item with multiple fields, destroy each field separately */
	for( iterationCount = 0;
		 attributeListCursor != NULL && \
			attributeListCursor->attributeID == attributeID && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		ATTRIBUTE_LIST *itemToFree = attributeListCursor;
		int localStatus;

		attributeListCursor = attributeListCursor->next;
		localStatus = deleteAttributeField( attributeListPtr, listCursorPtr, 
											itemToFree, dnCursor );
		if( cryptStatusError( localStatus ) && status != OK_SPECIAL )
			{
			/* Remember the error code, giving priority to DN cursor-
			   modification notifications */
			status = localStatus;
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( status );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int deleteCompleteAttribute( INOUT ATTRIBUTE_PTR **attributeListPtr,
							 INOUT ATTRIBUTE_PTR **listCursorPtr,
							 const CRYPT_ATTRIBUTE_TYPE attributeID,
							 IN_OPT const DN_PTR *dnCursor )
	{
	ATTRIBUTE_LIST *attributeListCursor;
	int iterationCount;

	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( *attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isWritePtr( listCursorPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( dnCursor == NULL || \
			isReadPtr( dnCursor, sizeof( DN_PTR_STORAGE ) ) );

	REQUIRES( attributeID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  attributeID <= CRYPT_CERTINFO_LAST );

	/* We're deleting an entire (constructed) attribute that won't have an 
	   entry in the list so we find the first field of the constructed 
	   attribute that's present in the list and delete that */
	for( attributeListCursor = *attributeListPtr, iterationCount = 0;
		 attributeListCursor != NULL && \
			attributeListCursor->attributeID != attributeID && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 attributeListCursor = attributeListCursor->next, iterationCount++ );
	ENSURES( attributeListCursor != NULL );
	ENSURES( attributeListCursor->next == NULL || \
			 attributeListCursor->next->attributeID != \
				attributeListCursor->attributeID );
	return( deleteAttributeField( attributeListPtr, listCursorPtr, 
								  attributeListCursor, dnCursor ) );
	}

/* Delete a complete set of attributes */

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteAttributes( INOUT ATTRIBUTE_PTR **attributeListPtr )
	{
	ATTRIBUTE_LIST *attributeListCursor = *attributeListPtr;
	int iterationCount;

	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST * ) ) );

	/* If the list was empty, return now */
	if( attributeListCursor == NULL )
		return;

	/* Destroy any remaining list items */
	for( iterationCount = 0;
		 attributeListCursor != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE_SPECIAL;
		 iterationCount++ )
		{
		ATTRIBUTE_LIST *itemToFree = attributeListCursor;

		attributeListCursor = attributeListCursor->next;
		deleteAttributeField( attributeListPtr, NULL, itemToFree, NULL );
		}
	ENSURES_V( iterationCount < FAILSAFE_ITERATIONS_LARGE_SPECIAL );
	ENSURES_V( *attributeListPtr == NULL );
	}
#endif /* USE_CERTIFICATES */
