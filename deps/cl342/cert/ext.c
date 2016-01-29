/****************************************************************************
*																			*
*					Certificate Attribute Management Routines				*
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

/* Special ATTRIBUTE_LIST values to indicate that an attribute field is a 
   blob attribute, a default-value field, or a complete attribute, see the 
   long comment for findAttributeFieldEx() in cert/ext.c for a detailed 
   description */

#define ATTR_BLOB_ATTR		{ ( CRYPT_ATTRIBUTE_TYPE ) 0, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0 }
#define ATTR_COMPLETE_ATTR	{ ( CRYPT_ATTRIBUTE_TYPE ) CRYPT_ERROR, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0 }
#define ATTR_DEFAULT_FIELD	{ ( CRYPT_ATTRIBUTE_TYPE ) 0, \
							  ( CRYPT_ATTRIBUTE_TYPE ) CRYPT_ERROR, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0 }

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Callback function used to provide external access to attribute list-
   internal fields */

CHECK_RETVAL_PTR \
static const void *getAttrFunction( IN_OPT TYPECAST( ATTRIBUTE_LIST * ) \
										const void *attributePtr, 
									OUT_OPT_ATTRIBUTE_Z \
										CRYPT_ATTRIBUTE_TYPE *groupID, 
									OUT_OPT_ATTRIBUTE_Z \
										CRYPT_ATTRIBUTE_TYPE *attributeID, 
									OUT_OPT_ATTRIBUTE_Z \
										CRYPT_ATTRIBUTE_TYPE *instanceID,
									IN_ENUM( ATTR ) const ATTR_TYPE attrGetType )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;

	assert( attributeListPtr == NULL || \
			isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( groupID == NULL || \
			isWritePtr( groupID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( attributeID == NULL || \
			isWritePtr( attributeID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( instanceID == NULL || \
			isWritePtr( instanceID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );

	REQUIRES_N( attrGetType > ATTR_NONE && attrGetType < ATTR_LAST );

	/* Clear return values */
	if( groupID != NULL )
		*groupID = CRYPT_ATTRIBUTE_NONE;
	if( attributeID != NULL )
		*attributeID = CRYPT_ATTRIBUTE_NONE;
	if( instanceID != NULL )
		*instanceID = CRYPT_ATTRIBUTE_NONE;

	/* Move to the next or previous attribute if required */
	if( attributeListPtr == NULL || \
		!isValidAttributeField( attributeListPtr ) )
		return( NULL );
	if( attrGetType == ATTR_PREV )
		attributeListPtr = attributeListPtr->prev;
	else
		{
		if( attrGetType == ATTR_NEXT )
			attributeListPtr = attributeListPtr->next;
		}
	if( attributeListPtr == NULL || \
		!isValidAttributeField( attributeListPtr ) )
		return( NULL );

	/* Return ID information to the caller */
	if( groupID != NULL )
		*groupID = attributeListPtr->attributeID;
	if( attributeID != NULL )
		*attributeID = attributeListPtr->fieldID;
	if( instanceID != NULL )
		*instanceID = attributeListPtr->subFieldID;
	return( attributeListPtr );
	}

/****************************************************************************
*																			*
*								Attribute Type Mapping						*
*																			*
****************************************************************************/

/* Get the attribute information for a given OID.  Note that this function
   looks up whole attributes by OID but won't match an OID that's internal
   to an attribute */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
const ATTRIBUTE_INFO *oidToAttribute( IN_ENUM( ATTRIBUTE ) \
										const ATTRIBUTE_TYPE attributeType,
									  IN_BUFFER( oidLength ) const BYTE *oid, 
									  IN_LENGTH_OID const int oidLength )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr;
	int attributeInfoSize, iterationCount, status;

	assert( isReadPtr( oid, oidLength ) );
	
	REQUIRES_N( attributeType == ATTRIBUTE_CERTIFICATE || \
				attributeType == ATTRIBUTE_CMS );
	REQUIRES_N( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE && \
				oidLength == sizeofOID( oid ) );

	status = getAttributeInfo( attributeType, &attributeInfoPtr, 
							   &attributeInfoSize );
	ENSURES_N( cryptStatusOK( status ) );
	for( iterationCount = 0;
		 !isAttributeTableEnd( attributeInfoPtr ) && \
			iterationCount < attributeInfoSize; \
		 attributeInfoPtr++, iterationCount++ )
		{
		assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

		/* If it's an OID that's internal to an attribute, skip it */
		if( !isAttributeStart( attributeInfoPtr ) )
			continue;

		/* Guaranteed by the initialisation sanity-check */
		ENSURES_N( attributeInfoPtr->oid != NULL );

		/* If the OID matches the current attribute, we're done */
		if( oidLength == sizeofOID( attributeInfoPtr->oid ) && \
			!memcmp( attributeInfoPtr->oid, oid, oidLength ) )
			return( attributeInfoPtr );
		}
	ENSURES_N( iterationCount < attributeInfoSize );

	/* It's an unknown attribute */
	return( NULL );
	}

/* Get the attribute and attributeID for a field ID */

CHECK_RETVAL_PTR \
const ATTRIBUTE_INFO *fieldIDToAttribute( IN_ENUM( ATTRIBUTE ) \
											const ATTRIBUTE_TYPE attributeType,
										  IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE fieldID, 
										  IN_ATTRIBUTE_OPT \
											const CRYPT_ATTRIBUTE_TYPE subFieldID,
										  OUT_OPT_ATTRIBUTE_Z \
											CRYPT_ATTRIBUTE_TYPE *attributeID )
	{
	CRYPT_ATTRIBUTE_TYPE lastAttributeID = CRYPT_ATTRIBUTE_NONE;
	const ATTRIBUTE_INFO *attributeInfoPtr;
	int attributeInfoSize, iterationCount, status;

	assert( attributeID == NULL || \
			isWritePtr( attributeID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );

	REQUIRES_N( attributeType == ATTRIBUTE_CERTIFICATE || \
				attributeType == ATTRIBUTE_CMS );
	REQUIRES_N( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				fieldID <= CRYPT_CERTINFO_LAST );
	REQUIRES_N( subFieldID == CRYPT_ATTRIBUTE_NONE || \
				( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				  subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );

	/* Clear return value */
	if( attributeID != NULL )
		*attributeID = CRYPT_ATTRIBUTE_NONE;

	/* Find the information on this attribute field */
	status = getAttributeInfo( attributeType, &attributeInfoPtr, 
							   &attributeInfoSize );
	ENSURES_N( cryptStatusOK( status ) );
	for( iterationCount = 0; 
		 !isAttributeTableEnd( attributeInfoPtr ) && \
			iterationCount < attributeInfoSize; 
		 attributeInfoPtr++, iterationCount++ )
		{
		const ATTRIBUTE_INFO *altEncodingTable;
		int innerIterationCount;

		assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

		/* If we're looking for an attribute ID and this is the start of a 
		   complete attribute, remember it so that we can report it to the 
		   caller */
		if( attributeID != NULL && isAttributeStart( attributeInfoPtr ) )
			{
			/* Usually the attribute ID is the fieldID for the first entry,
			   however in some cases the attributeID is the same as the
			   fieldID and isn't specified until later on.  For example when 
			   the attribute consists of a SEQUENCE OF field the first
			   entry is the SEQUENCE and the fieldID isn't given until the
			   second entry.  This case is denoted by the fieldID being 
			   FIELDID_FOLLOWS, if this happens then the next entry contains 
			   the fieldID (this requirement has been verified by the 
			   startup self-check in ext_def.c) */
			if( attributeInfoPtr->fieldID == FIELDID_FOLLOWS )
				attributeInfoPtr++;
			ENSURES_N( attributeInfoPtr->fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
					   attributeInfoPtr->fieldID <= CRYPT_CERTINFO_LAST );
			lastAttributeID = attributeInfoPtr->fieldID;
			}

		/* If the field ID for this entry isn't the one that we want, 
		   continue */
		if( attributeInfoPtr->fieldID != fieldID )
			continue;

		/* If we're not after a subfield match or there's no subfield 
		   information present, we're done */
		if( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			attributeInfoPtr->extraData == NULL )
			{
			if( attributeID != NULL )
				*attributeID = lastAttributeID;
			return( attributeInfoPtr );
			}

		/* We're after a subfield match as well, try and match it.  
		   Unfortunately we can't use the attributeInfoSize bounds check 
		   limit here because we don't know the size of the alternative 
		   encoding table so we have to use a generic large value (the
		   terminating condition has been verified by the startup self-
		   check in ext_def.c) */
		for( altEncodingTable = attributeInfoPtr->extraData, \
				innerIterationCount = 0; 
			 !isAttributeTableEnd( altEncodingTable ) && \
				innerIterationCount < FAILSAFE_ITERATIONS_LARGE; 
			 altEncodingTable++, innerIterationCount++ )
			{
			if( altEncodingTable->fieldID == subFieldID )
				{
				if( attributeID != NULL )
					*attributeID = lastAttributeID;
				return( altEncodingTable );
				}
			}

		/* If we reach this point for any reason then it's an error so we 
		   don't have to perform an explicit iteration-count check */
		retIntError_Null();
		}
	ENSURES_N( iterationCount < attributeInfoSize );

	/* If the use of all attributes is enabled then we should never reach 
	   this point, however since it's unlikely that we'll have every single 
	   obscure, obsolete, and weird attribute enabled we simply return a not-
	   found error if we get to here */
	return( NULL );
	}

/****************************************************************************
*																			*
*					Attribute Location/Cursor Movement Routines				*
*																			*
****************************************************************************/

/* Find the start of an attribute from a field within the attribute */

CHECK_RETVAL_PTR \
ATTRIBUTE_LIST *findAttributeStart( IN_OPT const ATTRIBUTE_LIST *attributeListPtr )
	{
	assert( attributeListPtr == NULL || \
			isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	return( attributeFindStart( attributeListPtr, getAttrFunction ) );
	}

/* Find an attribute in a list of certificate attributes by object identifier
   (for blob-type attributes) or by field and subfield ID (for known
   attributes).  These are exact-match functions for which we need an
   attribute with { fieldID, subFieldID } matching exactly.  It won't find,
   for example, any match for CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT even
   if one of the attribute fields such as CRYPT_CERTINFO_ISSUINGDIST_FULLNAME
   is present */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
ATTRIBUTE_PTR *findAttributeByOID( IN_OPT const ATTRIBUTE_PTR *attributePtr,
								   IN_BUFFER( oidLength ) const BYTE *oid, 
								   IN_LENGTH_OID const int oidLength )
	{
	const ATTRIBUTE_LIST *attributeListPtr;
	int iterationCount;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( oid, oidLength ) );
	
	REQUIRES_N( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE && \
				oidLength == sizeofOID( oid ) );

	/* Early-out check for empty attribute lists */
	if( attributePtr == NULL )
		return( NULL );

	/* Find the position of this component in the list */
	for( attributeListPtr = attributePtr, iterationCount = 0;
		 attributeListPtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		/* If it's not a blob-type attribute, continue */
		if( !checkAttributeProperty( attributeListPtr, 
									 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
			continue;

		/* If we've found the entry with the required OID, we're done */
		if( oidLength == sizeofOID( attributeListPtr->oid ) && \
			!memcmp( attributeListPtr->oid, oid, oidLength ) )
			return( ( ATTRIBUTE_PTR * ) attributeListPtr );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( NULL );
	}

CHECK_RETVAL_PTR \
ATTRIBUTE_PTR *findAttributeField( IN_OPT const ATTRIBUTE_PTR *attributePtr,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
								   IN_ATTRIBUTE_OPT \
										const CRYPT_ATTRIBUTE_TYPE subFieldID )
	{
	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_N( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				fieldID <= CRYPT_CERTINFO_LAST );
	REQUIRES_N( subFieldID == CRYPT_ATTRIBUTE_NONE || \
				( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				  subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );

	/* Early-out check for empty attribute lists */
	if( attributePtr == NULL )
		return( NULL );

	if( subFieldID == CRYPT_ATTRIBUTE_NONE )
		return( attributeFind( attributePtr, getAttrFunction, fieldID ) );
	return( attributeFindEx( attributePtr, getAttrFunction, 
							 CRYPT_ATTRIBUTE_NONE, fieldID, subFieldID ) );
	}

/* Find an attribute in a list of certificate attributes by field ID, with
   special handling for things that aren't direct matches.  These special 
   cases occur when:

	We're searching via the overall attribute ID for a constructed attribute 
	(e.g. CRYPT_CERTINFO_CA) for which only the individual fields (e.g. 
	CRYPT_CERTINFO_BASICCONSTRAINTS) are present in the attribute list, in 
	which case we return a special-case value to indicate that this is a 
	complete attribute and not just one attribute field.
   
	We're searching for a default-valued field which isn't explicitly 
	present in the attribute list but for which the attribute that contains
	it is present, in which case we return a special-case value to indicate
	that this is a default-value field */

CHECK_RETVAL_PTR \
ATTRIBUTE_PTR *findAttributeFieldEx( IN_OPT const ATTRIBUTE_PTR *attributePtr,
									 IN_ATTRIBUTE \
										const CRYPT_ATTRIBUTE_TYPE fieldID )
	{
	static const ATTRIBUTE_LIST completeAttribute = ATTR_COMPLETE_ATTR;
	static const ATTRIBUTE_LIST defaultField = ATTR_DEFAULT_FIELD;
	const ATTRIBUTE_LIST *attributeListCursor;
	const ATTRIBUTE_INFO *attributeInfoPtr;
	const ATTRIBUTE_TYPE attributeType = \
							( fieldID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
							ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE;
	CRYPT_ATTRIBUTE_TYPE attributeID;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_N( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				fieldID <= CRYPT_CERTINFO_LAST );

	/* Early-out check for empty attribute lists */
	if( attributePtr == NULL )
		return( NULL );

	/* Find the position of this attribute component in the list */
	attributeListCursor = attributeFind( attributePtr, 
										 getAttrFunction, fieldID );
	if( attributeListCursor != NULL )
		return( ( ATTRIBUTE_PTR * ) attributeListCursor );

	/* This particular attribute field isn't present in the list of 
	   attributes, check whether the overall attribute that contains this 
	   field is present.  First we map the field to the attribute that 
	   contains it, so for example CRYPT_CERTINFO_AUTHORITYINFO_CRLS 
	   would become CRYPT_CERTINFO_AUTHORITYINFOACCESS.  If we're already 
	   searching by overall attribute ID (e.g. searching for 
	   CRYPT_CERTINFO_AUTHORITYINFOACCESS, which wouldn't result in a match 
	   because only the component fields of 
	   CRYPT_CERTINFO_AUTHORITYINFOACCESS are present) then this step is a 
	   no-op */
	attributeInfoPtr = fieldIDToAttribute( attributeType, fieldID, 
										   CRYPT_ATTRIBUTE_NONE, &attributeID );
	if( attributeInfoPtr == NULL )
		{
		/* There's no attribute containing this field, exit */
		return( NULL );
		}

	/* We've now got the ID of the overall attribute that contains the 
	   requested field, check whether any other part of the attribute that 
	   contains the field is present in the list of attribute fields.  So 
	   from the previous example of looking for the field 
	   CRYPT_CERTINFO_AUTHORITYINFO_CRLS we'd get a match if e.g. 
	   CRYPT_CERTINFO_AUTHORITYINFO_OCSP was present since they're both in 
	   the same attribute CRYPT_CERTINFO_AUTHORITYINFOACCESS */
	attributeListCursor = attributeFindEx( attributePtr, getAttrFunction, 
										   attributeID, CRYPT_ATTRIBUTE_NONE, 
										   CRYPT_ATTRIBUTE_NONE );
	if( attributeListCursor == NULL || \
		!isValidAttributeField( attributeListCursor ) )
		return( NULL );

	/* Some other part of the attribute containing the given field is 
	   present in the attribute list.  If the attribute info indicates that 
	   this field is a default-value one we return an entry that denotes 
	   that this field is pseudo-present due to it having a default setting. 
	   Note that this requires that the field be a BOOLEAN DEFAULT FALSE, or
	   at least a numeric value DEFAULT 0, since the returned 'defaultField'
	   reads as having a numeric value zero (this has been verified by the
	   startup self-check in ext_def.c).  If the attribute info indicates 
	   that this is an ID for a complete attribute rather than an individual
	   field within it we return an entry to indicate this */
	if( attributeInfoPtr->encodingFlags & FL_DEFAULT )
		return( ( ATTRIBUTE_PTR * ) &defaultField );
	if( isAttributeStart( attributeInfoPtr ) )
		return( ( ATTRIBUTE_PTR * ) &completeAttribute );

	return( NULL );
	}

/* Find the next instance of an attribute field in an attribute.  This is 
   used to step through multiple instances of a field, for example where the
   attribute is defined as containing a SEQUENCE OF <field> */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ATTRIBUTE_PTR *findNextFieldInstance( const ATTRIBUTE_PTR *attributePtr )
	{
	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	return( attributeFindNextInstance( attributePtr, getAttrFunction ) );
	}

/* Find a DN in an attribute, where the attribute will be a GeneralName */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ATTRIBUTE_PTR *findDnInAttribute( IN_OPT const ATTRIBUTE_PTR *attributePtr )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;
	CRYPT_ATTRIBUTE_TYPE attributeID, fieldID;
	int iterationCount;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	/* If it's an empty attribute list, there's nothing to do */
	if( attributePtr == NULL )
		return( NULL );

	/* Remember the current GeneralName type so that we don't stray 
	   outside it */
	attributeID = attributeListPtr->attributeID;
	fieldID = attributeListPtr->fieldID;
	REQUIRES_N( isGeneralNameSelectionComponent( fieldID ) );

	/* Search for a DN in the current GeneralName */
	for( iterationCount = 0; 
		 attributeListPtr != NULL && \
			attributeListPtr->attributeID == attributeID && \
			attributeListPtr->fieldID == fieldID && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		if( attributeListPtr->fieldType == FIELDTYPE_DN )
			return( ( ATTRIBUTE_PTR * ) attributeListPtr );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( NULL );
	}

/* Find an overall attribute in a list of attributes.  This is almost always
   used as a check for the presence of an overall attribute so we provide a 
   separate function checkAttributeXXXPresent() to make this explicit */

CHECK_RETVAL_PTR \
ATTRIBUTE_PTR *findAttribute( IN_OPT const ATTRIBUTE_PTR *attributePtr,
							  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
							  const BOOLEAN isFieldID )
	{
	CRYPT_ATTRIBUTE_TYPE localAttributeID = attributeID;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_N( attributeID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				attributeID <= CRYPT_CERTINFO_LAST );
	
	if( attributePtr == NULL )
		return( NULL );

	/* If this is a (potential) fieldID rather than an attributeID, find the
	   attributeID for the attribute containing this field */
	if( isFieldID )
		{
		if( fieldIDToAttribute( ( attributeID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
									ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE, 
								attributeID, CRYPT_ATTRIBUTE_NONE, 
								&localAttributeID ) == NULL )
			{
			/* There's no attribute containing this field, exit */
			return( NULL );
			}
		}
	else
		{
		/* Make sure that we're searching on an attribute ID rather than a 
		   field ID */
		ENSURES_N( \
			fieldIDToAttribute( ( attributeID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
									ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE, 
								attributeID, CRYPT_ATTRIBUTE_NONE, 
								&localAttributeID ) == NULL || \
			attributeID == localAttributeID );
		}

	/* Check whether this attribute is present in the list of attribute 
	   fields */
	return( attributeFindEx( attributePtr, getAttrFunction, localAttributeID, 
							 CRYPT_ATTRIBUTE_NONE, CRYPT_ATTRIBUTE_NONE ) );
	}

CHECK_RETVAL_BOOL \
BOOLEAN checkAttributePresent( IN_OPT const ATTRIBUTE_PTR *attributePtr,
							   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID )
	{
	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_B( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				fieldID <= CRYPT_CERTINFO_LAST );

	return( findAttribute( attributePtr, fieldID, FALSE ) != NULL ? \
			TRUE : FALSE );
	}

CHECK_RETVAL_BOOL \
BOOLEAN checkAttributeFieldPresent( IN_OPT const ATTRIBUTE_PTR *attributePtr,
									IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID )
	{
	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_B( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				fieldID <= CRYPT_CERTINFO_LAST );

	return( findAttributeField( attributePtr, fieldID, \
								CRYPT_ATTRIBUTE_NONE ) != NULL ? \
			TRUE : FALSE );
	}

/* Move the attribute cursor relative to the current cursor position.  The 
   reason for the apparently-reversed values in the IN_RANGE() annotation
   are because the values are -ve, so last comes before first */

CHECK_RETVAL_PTR \
ATTRIBUTE_PTR *certMoveAttributeCursor( IN_OPT const ATTRIBUTE_PTR *currentCursor,
										IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE certInfoType,
										IN_RANGE( CRYPT_CURSOR_LAST, \
												  CRYPT_CURSOR_FIRST) \
											const int position )
	{
	assert( currentCursor == NULL || \
			isReadPtr( currentCursor, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_N( certInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP || \
				certInfoType == CRYPT_ATTRIBUTE_CURRENT || \
				certInfoType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE );
	REQUIRES_N( position <= CRYPT_CURSOR_FIRST && \
				position >= CRYPT_CURSOR_LAST );

	return( ( ATTRIBUTE_PTR * ) \
			attributeMoveCursor( currentCursor, getAttrFunction,
								 certInfoType, position ) );
	}

/****************************************************************************
*																			*
*							Attribute Access Routines						*
*																			*
****************************************************************************/

/* Get/set information for an attribute */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkAttributeProperty( const ATTRIBUTE_PTR *attributePtr,
								IN_ENUM( ATTRIBUTE_PROPERTY ) \
									ATTRIBUTE_PROPERTY_TYPE property )
	{
	static const ATTRIBUTE_LIST blobAttribute = ATTR_BLOB_ATTR;
	static const ATTRIBUTE_LIST completeAttribute = ATTR_COMPLETE_ATTR;
	static const ATTRIBUTE_LIST defaultField = ATTR_DEFAULT_FIELD;
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_B( property > ATTRIBUTE_PROPERTY_NONE && \
				property < ATTRIBUTE_PROPERTY_LAST );

	switch( property )
		{
		case ATTRIBUTE_PROPERTY_BLOBATTRIBUTE:
			return( ( attributeListPtr->fieldID == blobAttribute.fieldID && \
					  attributeListPtr->attributeID == blobAttribute.attributeID ) ? \
					TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_COMPLETEATRIBUTE:
			return( ( attributeListPtr->fieldID == completeAttribute.fieldID && \
					  attributeListPtr->attributeID == completeAttribute.attributeID ) ? \
					TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_CRITICAL:
			return( ( attributeListPtr->flags & ATTR_FLAG_CRITICAL ) ? \
					TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_DEFAULTVALUE:
			return( ( attributeListPtr->fieldID == defaultField.fieldID && \
					  attributeListPtr->attributeID == defaultField.attributeID ) ? \
					  TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_DN:
			return( ( attributeListPtr->fieldType == FIELDTYPE_DN ) ? \
					TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_IGNORED:
			return( ( attributeListPtr->flags & ATTR_FLAG_IGNORED ) ? \
					TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_LOCKED:
			return( ( attributeListPtr->flags & ATTR_FLAG_LOCKED ) ? \
					TRUE : FALSE );
		
		case ATTRIBUTE_PROPERTY_OID:
			return( ( attributeListPtr->fieldType == BER_OBJECT_IDENTIFIER ) ? \
					TRUE : FALSE );
		}

	retIntError_Boolean();
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void setAttributeProperty( INOUT ATTRIBUTE_PTR *attributePtr,
						   IN_ENUM( ATTRIBUTE_PROPERTY ) \
								ATTRIBUTE_PROPERTY_TYPE property,
						   IN_INT_Z const int optValue )
	{
	ATTRIBUTE_LIST *attributeListPtr = attributePtr;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_V( property > ATTRIBUTE_PROPERTY_NONE && \
				property < ATTRIBUTE_PROPERTY_LAST );
	REQUIRES_V( optValue >= 0 );

	switch( property )
		{
		case ATTRIBUTE_PROPERTY_CRITICAL:
			REQUIRES_V( optValue == 0 );

			attributeListPtr->flags |= ATTR_FLAG_CRITICAL;
			return;

		case ATTRIBUTE_PROPERTY_VALUE:
			REQUIRES_V( optValue > 0 );

			attributeListPtr->intValue = optValue;
			return;

		case ATTRIBUTE_PROPERTY_LOCKED:
			REQUIRES_V( optValue == 0 );

			attributeListPtr->flags |= ATTR_FLAG_LOCKED;
			return;
		}
	
	retIntError_Void();
	}

/* Get attribute ID information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getAttributeIdInfo( const ATTRIBUTE_PTR *attributePtr,
						OUT_OPT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *attributeID,
						OUT_OPT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *fieldID,
						OUT_OPT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *subFieldID )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( attributeID == NULL || \
			isWritePtr( attributeID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( fieldID == NULL || \
			isWritePtr( fieldID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( subFieldID == NULL || \
			isWritePtr( subFieldID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );

	REQUIRES( attributeID != NULL || fieldID != NULL || subFieldID != NULL );

	/* Return ID information to the caller */
	if( attributeID != NULL )
		*attributeID = attributeListPtr->attributeID;
	if( fieldID != NULL )
		*fieldID = attributeListPtr->fieldID;
	if( subFieldID != NULL )
		*subFieldID = attributeListPtr->subFieldID;

	return( CRYPT_OK );
	}

/* Enumerate entries in an attribute list.  Note that these two functions 
   must be called atomically since they record pointers within the list, 
   which would leave a dangling reference if (say) a delete occurred between
   getNext()'s */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const ATTRIBUTE_PTR *getFirstAttribute( OUT ATTRIBUTE_ENUM_INFO *attrEnumInfo,
										IN_OPT const ATTRIBUTE_PTR *attributePtr,
										IN_ENUM( ATTRIBUTE_ENUM ) \
											const ATTRIBUTE_ENUM_TYPE enumType )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;

	assert( isWritePtr( attrEnumInfo, sizeof( ATTRIBUTE_ENUM_INFO ) ) );
	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_N( enumType > ATTRIBUTE_ENUM_NONE && \
				enumType < ATTRIBUTE_ENUM_LAST );

	/* Clear return value */
	memset( attrEnumInfo, 0, sizeof( ATTRIBUTE_ENUM_INFO ) );
	attrEnumInfo->attributePtr = attributePtr;
	attrEnumInfo->enumType = enumType;

	if( attributePtr == NULL )
		return( NULL );

	switch( enumType )
		{
		case ATTRIBUTE_ENUM_BLOB:
			{
			int iterationCount;

			/* Blob attributes are a special case because they're preceded 
			   in the attribute list by non-blob attributes so before we try 
			   and enumerate blob attributes we have to skip any non-blob
			   ones that may be present */
			for( iterationCount = 0;
				 attributeListPtr != NULL && \
					!checkAttributeProperty( attributeListPtr, 
											 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) && \
					iterationCount < FAILSAFE_ITERATIONS_LARGE; 
				 attributeListPtr = attributeListPtr->next, iterationCount++ );
			ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_LARGE );
			attrEnumInfo->attributePtr = attributeListPtr;

			/* If there are no blob attributes, we're done */
			if( attributeListPtr == NULL )
				return( NULL );

			break;
			}

		case ATTRIBUTE_ENUM_NONBLOB:
			/* If there are no non-blob attributes, we're done */
			if( checkAttributeProperty( attributeListPtr, 
										ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
				{
				attrEnumInfo->attributePtr = NULL;
				return( NULL );
				}
			break;

		default:
			retIntError_Null();
		}

	return( attrEnumInfo->attributePtr );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const ATTRIBUTE_PTR *getNextAttribute( INOUT ATTRIBUTE_ENUM_INFO *attrEnumInfo )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attrEnumInfo->attributePtr;

	assert( isWritePtr( attrEnumInfo, sizeof( ATTRIBUTE_ENUM_INFO ) ) );
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	/* Move on to the next attribute in the list */
	ENSURES_N( attrEnumInfo->attributePtr != NULL );
	attrEnumInfo->attributePtr = \
		( ( ATTRIBUTE_LIST * ) attrEnumInfo->attributePtr )->next;
	if( attrEnumInfo->attributePtr == NULL )
		return( NULL );

	switch( attrEnumInfo->enumType )
		{
		case ATTRIBUTE_ENUM_BLOB:
			break;

		case ATTRIBUTE_ENUM_NONBLOB:
			/* If there are no more non-blob attributes, we're done */
			if( checkAttributeProperty( attributeListPtr, 
										ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
				{
				attrEnumInfo->attributePtr = NULL;
				return( NULL );
				}
			break;

		default:
			retIntError_Null();
		}

	return( attrEnumInfo->attributePtr );
	}

/* Get attribute data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAttributeDataValue( IN const ATTRIBUTE_PTR *attributePtr,
						   OUT_INT_Z int *value )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( attributeListPtr->fieldType == BER_INTEGER || \
			  attributeListPtr->fieldType == BER_ENUMERATED || \
			  attributeListPtr->fieldType == BER_BITSTRING || \
			  attributeListPtr->fieldType == BER_BOOLEAN || \
			  attributeListPtr->fieldType == BER_NULL || \
			  attributeListPtr->fieldType == FIELDTYPE_CHOICE || \
			  attributeListPtr->fieldType == FIELDTYPE_IDENTIFIER );

	*value = attributeListPtr->intValue;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAttributeDataTime( IN const ATTRIBUTE_PTR *attributePtr,
						  OUT time_t *value )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( attributeListPtr->fieldType == BER_TIME_GENERALIZED || \
			  attributeListPtr->fieldType == BER_TIME_UTC );

	*value = *( ( time_t * ) attributeListPtr->value );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAttributeDataDN( IN const ATTRIBUTE_PTR *attributePtr,
						OUT_PTR DN_PTR ***dnPtr )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );
	
	REQUIRES( attributeListPtr->fieldType == FIELDTYPE_DN );

	/* See the comment by the definition of SELECTION_INFO in cert.h for the
	   reason why we return the address of the pointer rather than the 
	   pointer itself */
	*dnPtr = ( DN_PTR ** ) &attributeListPtr->value;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int getAttributeDataPtr( IN const ATTRIBUTE_PTR *attributePtr,
						 OUT_BUFFER_ALLOC( *dataLength ) void **dataPtrPtr, 
						 OUT_LENGTH_SHORT_Z int *dataLength )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributePtr;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	*dataPtrPtr = attributeListPtr->value;
	*dataLength = attributeListPtr->valueLength;

	return( CRYPT_OK );
	}

/* The pattern { findAttributeField(), getAttributeDataXXX() } where XXX ==
   { Value, Time } is used frequently enough that we provide a standard 
   function for it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
int getAttributeFieldValue( IN_OPT const ATTRIBUTE_PTR *attributePtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
							IN_ATTRIBUTE_OPT \
								const CRYPT_ATTRIBUTE_TYPE subFieldID,
							OUT_INT_Z int *value )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  fieldID <= CRYPT_CERTINFO_LAST );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );

	/* Clear return value */
	*value = 0;

	/* Find the required attribute field and return its value */
	attributeListPtr = findAttributeField( attributePtr, fieldID, 
										   subFieldID );
	if( attributeListPtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	return( getAttributeDataValue( attributeListPtr, value ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
int getAttributeFieldTime( IN_OPT const ATTRIBUTE_PTR *attributePtr,
						   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
						   IN_ATTRIBUTE_OPT \
								const CRYPT_ATTRIBUTE_TYPE subFieldID,
						   OUT time_t *value )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  fieldID <= CRYPT_CERTINFO_LAST );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );

	/* Clear return value */
	*value = 0;

	/* Find the required attribute field and return its value */
	attributeListPtr = findAttributeField( attributePtr, fieldID, 
										   subFieldID );
	if( attributeListPtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	return( getAttributeDataTime( attributeListPtr, value ) );
	}

/* Get the default value for an optional field of an attribute */

CHECK_RETVAL \
int getDefaultFieldValue( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr;

	REQUIRES( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  fieldID <= CRYPT_CERTINFO_LAST );

	attributeInfoPtr = \
		fieldIDToAttribute( ( fieldID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
							ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE, fieldID, 
							CRYPT_ATTRIBUTE_NONE, NULL );
	ENSURES( attributeInfoPtr != NULL );

	return( ( int ) attributeInfoPtr->defaultValue );
	}

/****************************************************************************
*																			*
*							Attribute Compare Routines						*
*																			*
****************************************************************************/

/* Compare attribute fields */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN compareAttributeField( const ATTRIBUTE_LIST *attributeField1,
									  const ATTRIBUTE_LIST *attributeField2 )
	{
	assert( isReadPtr( attributeField1, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeField2, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_B( attributeField1->attributeID == attributeField2->attributeID );

	/* Compare attribute IDs */
	if( attributeField1->fieldID != attributeField2->fieldID || \
		attributeField1->subFieldID != attributeField2->subFieldID )
		return( FALSE );

	/* Compare the field type and any relevant attribute flags */
	if( attributeField1->fieldType != attributeField2->fieldType )
		return( FALSE );
	if( ( attributeField1->flags & ATTR_FLAGS_COMPARE_MASK ) != \
				( attributeField2->flags & ATTR_FLAGS_COMPARE_MASK ) )
		return( FALSE );

	/* Compare field data */
	if( attributeField1->fieldType == FIELDTYPE_DN )
		{
		/* DNs are structured data and need to be compared specially */
		return( compareDN( attributeField1->value, attributeField2->value,
						   FALSE, NULL ) );
		}
	if( attributeField1->intValue != attributeField2->intValue || \
		attributeField1->valueLength != attributeField2->valueLength )
		return( FALSE );
	if( attributeField1->valueLength > 0 )
		{
		if( memcmp( attributeField1->value, attributeField2->value,
					attributeField1->valueLength ) )
			return( FALSE );
		}

	return( TRUE );
	}

/* Compare attributes */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN compareAttribute( const ATTRIBUTE_PTR *attribute1,
						  const ATTRIBUTE_PTR *attribute2 )
	{
	const ATTRIBUTE_LIST *attributeListPtr1 = ( ATTRIBUTE_LIST * ) attribute1;
	const ATTRIBUTE_LIST *attributeListPtr2 = ( ATTRIBUTE_LIST * ) attribute2;
	const CRYPT_ATTRIBUTE_TYPE attributeID = attributeListPtr1->attributeID;
	int iterationCount;

	assert( isReadPtr( attributeListPtr1, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeListPtr2, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_B( attributeListPtr1->attributeID == \
										attributeListPtr2->attributeID );

	/* Compare all of the fields in the attributes */
	for( iterationCount = 0;
		 attributeListPtr1 != NULL && attributeListPtr2 != NULL && \
			attributeListPtr1->attributeID == attributeID && \
			attributeListPtr2->attributeID == attributeID && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 attributeListPtr1 = attributeListPtr1->next, \
			attributeListPtr2 = attributeListPtr2->next, \
			iterationCount++ )
		{
		if( !compareAttributeField( attributeListPtr1, attributeListPtr2 ) )
			return( FALSE );
		}
	ENSURES_B( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* We've reached the loop termination condition, if it was because 
	   either of the two attribute lists terminated make sure that it was 
	   at the end of the attribute being compared */
	if( attributeListPtr1 == NULL || attributeListPtr2 == NULL )
		{
		/* One (or both) of the lists terminated, make sure that the 
		   attribute doesn't continue in the other */
		if( attributeListPtr1 == NULL )
			{
			/* The first attribute list terminated, make sure that the 
			   attribute doesn't continue in the second list */
			if( attributeListPtr2 != NULL && \
				attributeListPtr2->attributeID == attributeID )
				return( FALSE );
			}
		else
			{
			/* The second attribute list terminated, make sure that the 
			   attribute doesn't continue in the first list */
			if( attributeListPtr1->attributeID == attributeID )
				return( FALSE );
			}

		/* Both attribute lists terminated at the same point */
		return( TRUE );
		}

	/* There are more attributes in the list, make sure that the current 
	   attribute ended at the same point in both lists */
	if( attributeListPtr1->attributeID == attributeID || \
		attributeListPtr2->attributeID == attributeID )
		return( FALSE );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Miscellaneous Attribute Routines					*
*																			*
****************************************************************************/

/* Fix up certificate attributes, mapping from incorrect values to standards-
   compliant ones */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int fixAttributes( INOUT CERT_INFO *certInfoPtr )
	{
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	int complianceLevel;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* Try and locate email addresses wherever they might be stashed and move
	   them to the certificate altNames */
	status = convertEmail( certInfoPtr, &certInfoPtr->subjectName,
						   CRYPT_CERTINFO_SUBJECTALTNAME );
	if( cryptStatusOK( status ) )
		status = convertEmail( certInfoPtr, &certInfoPtr->issuerName,
							   CRYPT_CERTINFO_ISSUERALTNAME );
	if( cryptStatusError( status ) )
		return( status );

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* If we're running at a compliance level of 
	   CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL or above don't try and compensate
	   for dubious attributes */
	status = krnlSendMessage( certInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &complianceLevel, 
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		return( status );
	if( complianceLevel >= CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL )
		return( CRYPT_OK );
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

	/* If the only key usage information present is the Netscape one, 
	   convert it into the X.509 equivalent */
	if( !checkAttributePresent( certInfoPtr->attributes, 
								CRYPT_CERTINFO_KEYUSAGE ) && \
		findAttributeField( certInfoPtr->attributes, 
							CRYPT_CERTINFO_NS_CERTTYPE, 
							CRYPT_ATTRIBUTE_NONE ) != NULL )
		{
		int keyUsage;

		status = getKeyUsageFromExtKeyUsage( certInfoPtr, &keyUsage,
											 &certInfoPtr->errorLocus, 
											 &certInfoPtr->errorType );
		if( cryptStatusOK( status ) )
			{
			status = addAttributeField( &certInfoPtr->attributes,
										CRYPT_CERTINFO_KEYUSAGE, 
										CRYPT_ATTRIBUTE_NONE, keyUsage, 
										ATTR_FLAG_NONE,  
										&certInfoPtr->errorLocus, 
										&certInfoPtr->errorType );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}
#endif /* USE_CERTIFICATES */
