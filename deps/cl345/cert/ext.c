/****************************************************************************
*																			*
*					Certificate Attribute Management Routines				*
*						Copyright Peter Gutmann 1996-2015					*
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
   long comment for findAttributeFieldEx() for a detailed description */

#define ATTR_BLOB_ATTR		{ ( CRYPT_ATTRIBUTE_TYPE ) 0, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0 }
#define ATTR_COMPLETE_ATTR	{ ( CRYPT_ATTRIBUTE_TYPE ) CRYPT_ERROR, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0 }
#define ATTR_DEFAULT_FIELD	{ ( CRYPT_ATTRIBUTE_TYPE ) 0, \
							  ( CRYPT_ATTRIBUTE_TYPE ) CRYPT_ERROR, \
							  ( CRYPT_ATTRIBUTE_TYPE ) 0 }

static const ATTRIBUTE_LIST blobAttributeData = ATTR_BLOB_ATTR;
static const ATTRIBUTE_LIST completeAttributeData = ATTR_COMPLETE_ATTR;
static const ATTRIBUTE_LIST defaultFieldData = ATTR_DEFAULT_FIELD;
static DATAPTR_ATTRIBUTE defaultFieldAttribute, completeAttribute;

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check attribute data */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckAttributePtr( IN const ATTRIBUTE_LIST *attributeListPtr )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	/* There are three special ATTRIBUTE_LIST values that denote special-
	   case attribute types rather than normal attributes, so a sanity check
	   on them will always fail.  To deal with this we detect when we've 
	   been passed these and don't perform any further checks */
	if( attributeListPtr == &blobAttributeData || \
		attributeListPtr == &completeAttributeData || \
		attributeListPtr == &defaultFieldData )
		return( TRUE );

	/* If it's a blob-type attribute then none of the usual values are set.
	   We don't use isValidAttributeField(), used elsewhere to check for 
	   blob-type attributes, for this check because that only checks the 
	   attributeID, and we want a (fairly) definite match for a blob-type
	   attribute before we assume that it really is one */
	if( attributeListPtr->attributeID == CRYPT_ATTRIBUTE_NONE && \
		attributeListPtr->fieldID == CRYPT_ATTRIBUTE_NONE && \
		attributeListPtr->subFieldID == CRYPT_ATTRIBUTE_NONE )
		{
		/* Make sure that various fields are set/not set as required */
		if( attributeListPtr->oid == NULL || \
			attributeListPtr->encodedSize != 0 || \
			attributeListPtr->fieldType != 0 || \
			TEST_FLAG( attributeListPtr->flags,
					   ~( ATTR_FLAG_IGNORED | ATTR_FLAG_CRITICAL ) ) )
			{
			DEBUG_PUTS(( "sanityCheckAttribute: Blob general information" ));
			return( FALSE );
			}
		if( !DATAPTR_ISVALID( attributeListPtr->prev ) || \
			!DATAPTR_ISVALID( attributeListPtr->next ) )
			{
			DEBUG_PUTS(( "sanityCheckEnvelope: Blob prev/next links" ));
			return( FALSE );
			}

		return( TRUE );
		}

	/* Check general attribute data */
	if( !isValidExtension( attributeListPtr->attributeID ) || \
		!isValidExtension( attributeListPtr->fieldID ) || \
		( attributeListPtr->subFieldID != CRYPT_ATTRIBUTE_NONE && \
		  !isValidExtension( attributeListPtr->subFieldID ) && \
		  !isGeneralNameComponent( attributeListPtr->subFieldID ) && \
		  !isDNComponent( attributeListPtr->subFieldID ) ) )
		{
		DEBUG_PUTS(( "sanityCheckAttribute: Attribute/field/subfield ID" ));
		return( FALSE );
		}
	if( !isShortIntegerRange( attributeListPtr->encodedSize ) || \
		attributeListPtr->fieldType < FIELDTYPE_LAST || \
		attributeListPtr->fieldType > 0xFF || \
		!CHECK_FLAGS( attributeListPtr->flags, ATTR_FLAG_NONE, 
					  ATTR_FLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckAttribute: General info" ));
		return( FALSE );
		}

	/* Check safe pointers */
	if( !DATAPTR_ISVALID( attributeListPtr->prev ) || \
		!DATAPTR_ISVALID( attributeListPtr->next ) )
		{
		DEBUG_PUTS(( "sanityCheckEnvelope: Prev/next links" ));
		return( FALSE );
		}

	/* Check encoding info */
	if( attributeListPtr->fifoEnd < 0 || \
		attributeListPtr->fifoEnd > ENCODING_FIFO_SIZE - 1 || \
		attributeListPtr->fifoPos < 0 || \
		attributeListPtr->fifoPos > attributeListPtr->fifoEnd )
		{
		DEBUG_PUTS(( "sanityCheckAttribute: Encoding FIFO info" ));
		return( FALSE );
		}

	return( TRUE );
	}

CHECK_RETVAL_BOOL \
BOOLEAN sanityCheckAttribute( IN const DATAPTR_ATTRIBUTE attribute )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	/* Check the basic data pointer */
	if( !DATAPTR_ISSET( attribute ) )
		{
		DEBUG_PUTS(( "sanityCheckAttribute: Pointer" ));
		return( FALSE );
		}
	attributeListPtr = DATAPTR_GET( attribute );
	ENSURES_B( attributeListPtr != NULL );

	return( sanityCheckAttributePtr( attributeListPtr ) );
	}

/* Initialise constant-value attribute data, returned from getAttribute() 
   functions to denote special-case conditions such as the presence of a
   default attribute value */

void initAttributes( void )
	{
	DATAPTR_SET( defaultFieldAttribute, 
				 ( ATTRIBUTE_LIST * ) &defaultFieldData );
	DATAPTR_SET( completeAttribute, 
				 ( ATTRIBUTE_LIST * ) &completeAttributeData );
	}

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

	REQUIRES_N( isEnumRange( attrGetType, ATTR ) );

	/* Clear return values */
	if( groupID != NULL )
		*groupID = CRYPT_ATTRIBUTE_NONE;
	if( attributeID != NULL )
		*attributeID = CRYPT_ATTRIBUTE_NONE;
	if( instanceID != NULL )
		*instanceID = CRYPT_ATTRIBUTE_NONE;

	/* Move to the next or previous attribute if required */
	if( attributeListPtr == NULL )
		return( NULL );
	if( !isValidAttributeField( attributeListPtr ) )
		{
		/* It's a blob-type attribute, there's no concept of ordering by
		   field for these since they're opaque blobs so we can't move
		   to a previous/next field */
		return( NULL );
		}
	if( attrGetType == ATTR_PREV )
		{
		REQUIRES_N( DATAPTR_ISVALID( attributeListPtr->prev ) );
		attributeListPtr = DATAPTR_GET( attributeListPtr->prev );
		}
	else
		{
		if( attrGetType == ATTR_NEXT )
			{
			REQUIRES_N( DATAPTR_ISVALID( attributeListPtr->next ) );
			attributeListPtr = DATAPTR_GET( attributeListPtr->next );
			}
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

/* Print an attribute list, for debugging */

#ifndef NDEBUG

void printAttributeList( IN const DATAPTR_ATTRIBUTE attributeList )
	{
	ATTRIBUTE_LIST *attributeListCursor;
	int LOOP_ITERATOR;

	REQUIRES_V( DATAPTR_ISVALID( attributeList ) );

	DEBUG_PRINT(( "Attribute list starting at %lX:\n", 
				  attributeList.dataPtr ));
	LOOP_LARGE( attributeListCursor = DATAPTR_GET( attributeList ), 
				attributeListCursor != NULL,
				attributeListCursor = DATAPTR_GET( attributeListCursor->next ) )
		{
		DEBUG_PRINT(( "This: %8lX Prev: %8lX Next: %8lX attrID = %4d fldID = %4d "
					  "subFldID = %4d.\n", attributeListCursor,
					  attributeListCursor->prev.dataPtr, 
					  attributeListCursor->next.dataPtr, 
					  attributeListCursor->attributeID, 
					  attributeListCursor->fieldID,
					  attributeListCursor->subFieldID ));
		}
	ENSURES_V( LOOP_BOUND_OK );
	}
#endif /* NDEBUG */

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
	int attributeInfoSize, attributeInfoCount, status, LOOP_ITERATOR;

	assert( isReadPtrDynamic( oid, oidLength ) );
	
	REQUIRES_N( attributeType == ATTRIBUTE_CERTIFICATE || \
				attributeType == ATTRIBUTE_CMS );
	REQUIRES_N( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE && \
				oidLength == sizeofOID( oid ) );

	status = getAttributeInfo( attributeType, &attributeInfoPtr, 
							   &attributeInfoSize );
	ENSURES_N( cryptStatusOK( status ) );
	LOOP_LARGE( attributeInfoCount = 0,
				!isAttributeTableEnd( attributeInfoPtr ) && \
					attributeInfoCount < attributeInfoSize,
				( attributeInfoPtr++, attributeInfoCount++ ) )
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
	ENSURES_N( LOOP_BOUND_OK );
	ENSURES_N( attributeInfoCount < attributeInfoSize );

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
	int attributeInfoSize, attributeInfoCount, status, LOOP_ITERATOR;

	assert( attributeID == NULL || \
			isWritePtr( attributeID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );

	REQUIRES_N( attributeType == ATTRIBUTE_CERTIFICATE || \
				attributeType == ATTRIBUTE_CMS );
	REQUIRES_N( isValidExtension( fieldID ) );
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
	LOOP_LARGE( attributeInfoCount = 0, 
				!isAttributeTableEnd( attributeInfoPtr ) && \
					attributeInfoCount < attributeInfoSize,
				( attributeInfoPtr++, attributeInfoCount++ ) )
		{
		const ATTRIBUTE_INFO *altEncodingTable;
		int LOOP_ITERATOR_ALT;

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
			ENSURES_N( isValidExtension( attributeInfoPtr->fieldID ) );
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
		   encoding table so we have to use a generic value (the terminating 
		   condition has been verified by the startup self-check in 
		   ext_def.c) */
		LOOP_MED_ALT( altEncodingTable = attributeInfoPtr->extraData, 
					  !isAttributeTableEnd( altEncodingTable ), 
					  altEncodingTable++ )
			{
			if( altEncodingTable->fieldID == subFieldID )
				{
				if( attributeID != NULL )
					*attributeID = lastAttributeID;
				return( altEncodingTable );
				}
			}
		ENSURES_N( LOOP_BOUND_OK_ALT );

		retIntError_Null();
		}
	ENSURES_N( LOOP_BOUND_OK );
	ENSURES_N( attributeInfoCount < attributeInfoSize );

	/* If the use of all attributes is enabled then we should never reach 
	   this point, however since it's unlikely that we'll have every single 
	   obscure, obsolete, and weird attribute enabled we simply return a not-
	   found error if we get to here */
	return( NULL );
	}

/****************************************************************************
*																			*
*	Internal (ATTRIBUTE_LIST) Attribute Location/Cursor Movement Routines	*
*																			*
****************************************************************************/

/* Find the start of an attribute from a field within the attribute */

CHECK_RETVAL_PTR \
const ATTRIBUTE_LIST *findAttributeStart( IN_OPT const ATTRIBUTE_LIST *attributeListPtr )
	{
	assert( attributeListPtr == NULL || \
			isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	return( attributeFindStart( attributeListPtr, getAttrFunction ) );
	}

/****************************************************************************
*																			*
*  External (DATAPTR_ATTRIBUTE) Attribute Location/Cursor Movement Routines	*
*																			*
****************************************************************************/

/* Find an attribute in a list of certificate attributes by field and 
   subfield ID.  This is an exact-match functions for which we need an
   attribute with { fieldID, subFieldID } matching exactly.  It won't find,
   for example, any match for CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT even
   if one of the attribute fields such as CRYPT_CERTINFO_ISSUINGDIST_FULLNAME
   is present */

CHECK_RETVAL_DATAPTR \
DATAPTR_ATTRIBUTE findAttributeField( IN_DATAPTR_OPT \
											const DATAPTR_ATTRIBUTE attributePtr,
									  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
									  IN_ATTRIBUTE_OPT \
											const CRYPT_ATTRIBUTE_TYPE subFieldID )
	{
	REQUIRES_D( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES_D( isValidExtension( fieldID ) );
	REQUIRES_D( subFieldID == CRYPT_ATTRIBUTE_NONE || \
				( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				  subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );

	/* Early-out check for empty attribute lists */
	if( DATAPTR_ISNULL( attributePtr ) )
		return( DATAPTR_NULL );

	if( subFieldID == CRYPT_ATTRIBUTE_NONE )
		{
		return( dataptrAttributeFind( attributePtr, getAttrFunction, 
									  fieldID ) );
		}
	return( dataptrAttributeFindEx( attributePtr, getAttrFunction, 
									CRYPT_ATTRIBUTE_NONE, fieldID, 
									subFieldID ) );
	}

/* Find an attribute in a list of certificate attributes by field ID, with
   special handling for things that aren't direct matches.  These special 
   cases occur when:

	We're searching via the overall attribute ID for a constructed attribute 
	(e.g. CRYPT_CERTINFO_CA) for which only the individual fields (e.g. 
	CRYPT_CERTINFO_BASICCONSTRAINTS) are present in the attribute list, in 
	which case we return a special-case value to indicate that this is a 
	complete attribute and not just one attribute field.
   
	We're searching for a default-valued field that isn't explicitly present 
	in the attribute list but for which the attribute that contains it is 
	present, in which case we return a special-case value to indicate that 
	this is a default-value field */

CHECK_RETVAL_DATAPTR \
DATAPTR_ATTRIBUTE findAttributeFieldEx( IN_DATAPTR_OPT \
											const DATAPTR_ATTRIBUTE attributePtr,
										IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE fieldID )
	{
	const ATTRIBUTE_LIST *attributeListPtr;
	const ATTRIBUTE_INFO *attributeInfoPtr;
	const ATTRIBUTE_TYPE attributeType = \
							( fieldID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
							ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE;
	DATAPTR_ATTRIBUTE attributeCursor;
	CRYPT_ATTRIBUTE_TYPE attributeID;

	REQUIRES_D( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES_D( isValidExtension( fieldID ) );

	/* Early-out check for empty attribute lists */
	if( DATAPTR_ISNULL( attributePtr ) )
		return( DATAPTR_NULL );

	/* Find the position of this attribute component in the list */
	attributeCursor = dataptrAttributeFind( attributePtr, getAttrFunction, 
											fieldID );
	if( DATAPTR_ISSET( attributeCursor ) )
		return( attributeCursor );

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
		return( DATAPTR_NULL );
		}

	/* We've now got the ID of the overall attribute that contains the 
	   requested field, check whether any other part of the attribute that 
	   contains the field is present in the list of attribute fields.  So 
	   from the previous example of looking for the field 
	   CRYPT_CERTINFO_AUTHORITYINFO_CRLS we'd get a match if e.g. 
	   CRYPT_CERTINFO_AUTHORITYINFO_OCSP was present since they're both in 
	   the same attribute CRYPT_CERTINFO_AUTHORITYINFOACCESS */
	attributeCursor = dataptrAttributeFindEx( attributePtr, getAttrFunction, 
											  attributeID, CRYPT_ATTRIBUTE_NONE, 
											  CRYPT_ATTRIBUTE_NONE );
	if( DATAPTR_ISNULL( attributeCursor ) )
		return( DATAPTR_NULL );
	attributeListPtr = DATAPTR_GET( attributeCursor );
	ENSURES_D( attributeListPtr != NULL );
	if( !isValidAttributeField( attributeListPtr ) )
		return( DATAPTR_NULL );

	/* Some other part of the attribute containing the given field is 
	   present in the attribute list:

		If the attribute info indicates that this field is a default-value 
		then one we return an entry that denotes that this field is pseudo-
		present due to it having a default setting.  Note that this requires 
		that the field be a BOOLEAN DEFAULT FALSE, or at least a numeric 
		value DEFAULT 0, since the returned 'defaultField' reads as having a 
		numeric value zero (this has been verified by the startup self-check 
		in ext_def.c).
		
		If the attribute info indicates that this is an ID for a complete 
		attribute rather than an individual field within it we return an 
		entry to indicate this */
	if( attributeInfoPtr->encodingFlags & FL_DEFAULT )
		return( defaultFieldAttribute );
	if( isAttributeStart( attributeInfoPtr ) )
		return( completeAttribute );

	return( DATAPTR_NULL );
	}

/* Find an attribute in a list of certificate attributes by object 
   identifier, used for blob-type attributes */

CHECK_RETVAL_DATAPTR \
DATAPTR_ATTRIBUTE findAttributeByOID( IN_DATAPTR_OPT \
											const DATAPTR_ATTRIBUTE attributePtr,
									  IN_BUFFER( oidLength ) const BYTE *oid, 
									  IN_LENGTH_OID const int oidLength )
	{
	DATAPTR_ATTRIBUTE attributeCursor;
	ATTRIBUTE_LIST *attributeListCursor;
	int LOOP_ITERATOR;

	assert( isReadPtrDynamic( oid, oidLength ) );
	
	REQUIRES_D( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES_D( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE && \
				oidLength == sizeofOID( oid ) );

	/* Early-out check for empty attribute lists */
	if( DATAPTR_ISNULL( attributePtr ) )
		return( DATAPTR_NULL );

	/* Find the position of this component in the list */
	LOOP_LARGE( ( attributeCursor = attributePtr, 
				  attributeListCursor = DATAPTR_GET( attributePtr ) ),
				attributeListCursor != NULL,
				( attributeCursor = attributeListCursor->next,
				  attributeListCursor = DATAPTR_GET( attributeListCursor->next ) ) )
		{
		REQUIRES_D( sanityCheckAttributePtr( attributeListCursor ) );

		/* If it's not a blob-type attribute, continue */
		if( !checkAttributeListProperty( attributeListCursor, 
										 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
			continue;

		/* If we've found the entry with the required OID, we're done */
		if( oidLength == sizeofOID( attributeListCursor->oid ) && \
			!memcmp( attributeListCursor->oid, oid, oidLength ) )
			return( attributeCursor );
		}
	ENSURES_D( LOOP_BOUND_OK );

	return( DATAPTR_NULL );
	}

/* Find the next instance of an attribute field in an attribute.  This is 
   used to step through multiple instances of a field, for example where the
   attribute is defined as containing a SEQUENCE OF <field> */

CHECK_RETVAL_DATAPTR \
DATAPTR_ATTRIBUTE findNextFieldInstance( IN_DATAPTR_OPT \
											const DATAPTR_ATTRIBUTE attributePtr )
	{
	REQUIRES_D( DATAPTR_ISSET( attributePtr ) );

	return( dataptrAttributeFindNextInstance( attributePtr, getAttrFunction ) );
	}

/* Find a DN in an attribute, where the attribute will be a GeneralName */

CHECK_RETVAL_DATAPTR \
DATAPTR_ATTRIBUTE findDnInAttribute( IN_DATAPTR_OPT \
										const DATAPTR_ATTRIBUTE attributePtr )
	{
	DATAPTR_ATTRIBUTE attributeCursor;
	const ATTRIBUTE_LIST *attributeListPtr;
	CRYPT_ATTRIBUTE_TYPE attributeID, fieldID;
	int LOOP_ITERATOR;

	REQUIRES_D( DATAPTR_ISVALID( attributePtr ) );

	/* Early-out check for empty attribute lists */
	if( DATAPTR_ISNULL( attributePtr ) )
		return( DATAPTR_NULL );

	attributeListPtr = DATAPTR_GET( attributePtr );
	ENSURES_D( attributeListPtr != NULL );

	/* Remember the current GeneralName type so that we don't stray 
	   outside it */
	attributeID = attributeListPtr->attributeID;
	fieldID = attributeListPtr->fieldID;
	REQUIRES_D( isGeneralNameSelectionComponent( fieldID ) );

	/* Search for a DN in the current GeneralName */
	LOOP_LARGE( attributeCursor = attributePtr,
				attributeListPtr != NULL && \
					attributeListPtr->attributeID == attributeID && \
					attributeListPtr->fieldID == fieldID,
				( attributeCursor = attributeListPtr->next,
				  attributeListPtr = DATAPTR_GET( attributeListPtr->next ) ) )
		{
		REQUIRES_D( sanityCheckAttributePtr( attributeListPtr ) );

		if( attributeListPtr->fieldType == FIELDTYPE_DN )
			return( attributeCursor );
		}
	ENSURES_D( LOOP_BOUND_OK );

	return( DATAPTR_NULL );
	}

/* Find an overall attribute in a list of attributes.  This is almost always
   used as a check for the presence of an overall attribute so we provide a 
   separate function checkAttributeXXXPresent() to make this explicit */

CHECK_RETVAL_DATAPTR \
DATAPTR_ATTRIBUTE findAttribute( IN_DATAPTR_OPT \
									const DATAPTR_ATTRIBUTE attributePtr,
								 IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE attributeID,
								 const BOOLEAN isFieldID )
	{
	CRYPT_ATTRIBUTE_TYPE localAttributeID = attributeID;

	REQUIRES_D( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES_D( isValidExtension( attributeID ) );
	REQUIRES_D( isFieldID == TRUE || isFieldID == FALSE );
	
	/* Early-out check for empty attribute lists */
	if( DATAPTR_ISNULL( attributePtr ) )
		return( DATAPTR_NULL );

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
			return( DATAPTR_NULL );
			}
		}
	else
		{
		/* Make sure that we're searching on an attribute ID rather than a 
		   field ID */
		ENSURES_D( \
			fieldIDToAttribute( ( attributeID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
									ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE, 
								attributeID, CRYPT_ATTRIBUTE_NONE, 
								&localAttributeID ) == NULL || \
			attributeID == localAttributeID );
		}

	/* Check whether this attribute is present in the list of attribute 
	   fields */
	return( dataptrAttributeFindEx( attributePtr, getAttrFunction, 
									localAttributeID, CRYPT_ATTRIBUTE_NONE, 
									CRYPT_ATTRIBUTE_NONE ) );
	}

CHECK_RETVAL_BOOL \
BOOLEAN checkAttributePresent( IN_DATAPTR_OPT const DATAPTR_ATTRIBUTE attributePtr,
							   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID )
	{
	DATAPTR_ATTRIBUTE attributeCursor;

	REQUIRES_B( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES_B( isValidExtension( fieldID ) );

	attributeCursor = findAttribute( attributePtr, fieldID, FALSE );
	return( DATAPTR_ISSET( attributeCursor ) ? TRUE : FALSE );
	}

CHECK_RETVAL_BOOL \
BOOLEAN checkAttributeFieldPresent( IN_DATAPTR_OPT const DATAPTR_ATTRIBUTE attributePtr,
									IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID )
	{
	DATAPTR_ATTRIBUTE attributeCursor;

	REQUIRES_B( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES_B( isValidExtension( fieldID ) );

	attributeCursor = findAttributeField( attributePtr, fieldID, 
										  CRYPT_ATTRIBUTE_NONE );
	return( DATAPTR_ISSET( attributeCursor ) ? TRUE : FALSE );
	}

/* Move the attribute cursor relative to the current cursor position.  The 
   reason for the apparently-reversed values in the IN_RANGE() annotation
   are because the values are -ve, so last comes before first */

CHECK_RETVAL_DATAPTR \
DATAPTR_ATTRIBUTE certMoveAttributeCursor( IN_DATAPTR_OPT const DATAPTR_ATTRIBUTE currentCursor,
										   IN_ATTRIBUTE \
												const CRYPT_ATTRIBUTE_TYPE certInfoType,
										   IN_RANGE( CRYPT_CURSOR_LAST, \
													 CRYPT_CURSOR_FIRST) \
												const int position )
	{
	REQUIRES_D( DATAPTR_ISVALID( currentCursor ) );
	REQUIRES_D( certInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP || \
				certInfoType == CRYPT_ATTRIBUTE_CURRENT || \
				certInfoType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE );
	REQUIRES_D( position <= CRYPT_CURSOR_FIRST && \
				position >= CRYPT_CURSOR_LAST );

	return( dataptrAttributeMoveCursor( currentCursor, getAttrFunction,
										certInfoType, position ) );
	}

/****************************************************************************
*																			*
*							Attribute Access Routines						*
*																			*
****************************************************************************/

/* Get/set information for an attribute */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkAttributeListProperty( IN const ATTRIBUTE_LIST *attributeListPtr,
									IN_ENUM( ATTRIBUTE_PROPERTY ) \
										ATTRIBUTE_PROPERTY_TYPE property )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_B( sanityCheckAttributePtr( attributeListPtr ) );
	REQUIRES_B( isEnumRange( property, ATTRIBUTE_PROPERTY ) );

	switch( property )
		{
		case ATTRIBUTE_PROPERTY_BLOBATTRIBUTE:
			return( ( attributeListPtr->fieldID == blobAttributeData.fieldID && \
					  attributeListPtr->attributeID == blobAttributeData.attributeID ) ? \
					TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_COMPLETEATRIBUTE:
			return( ( attributeListPtr->fieldID == completeAttributeData.fieldID && \
					  attributeListPtr->attributeID == completeAttributeData.attributeID ) ? \
					TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_CRITICAL:
			return( TEST_FLAG( attributeListPtr->flags, 
							   ATTR_FLAG_CRITICAL ) ? TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_DEFAULTVALUE:
			return( ( attributeListPtr->fieldID == defaultFieldData.fieldID && \
					  attributeListPtr->attributeID == defaultFieldData.attributeID ) ? \
					  TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_DN:
			return( ( attributeListPtr->fieldType == FIELDTYPE_DN ) ? \
					TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_IGNORED:
			return( TEST_FLAG( attributeListPtr->flags, 
							   ATTR_FLAG_IGNORED ) ? TRUE : FALSE );

		case ATTRIBUTE_PROPERTY_LOCKED:
			return( TEST_FLAG( attributeListPtr->flags,
							   ATTR_FLAG_LOCKED ) ? TRUE : FALSE );
		
		case ATTRIBUTE_PROPERTY_OID:
			return( ( attributeListPtr->fieldType == BER_OBJECT_IDENTIFIER ) ? \
					TRUE : FALSE );
		}

	retIntError_Boolean();
	}

CHECK_RETVAL_BOOL \
BOOLEAN checkAttributeProperty( IN const DATAPTR_ATTRIBUTE attributePtr,
								IN_ENUM( ATTRIBUTE_PROPERTY ) \
									ATTRIBUTE_PROPERTY_TYPE property )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	REQUIRES_B( DATAPTR_ISSET( attributePtr ) );
	REQUIRES_B( isEnumRange( property, ATTRIBUTE_PROPERTY ) );
	attributeListPtr = DATAPTR_GET( attributePtr );
	REQUIRES_B( attributeListPtr != NULL );

	return( checkAttributeListProperty( attributeListPtr, property ) );
	}

void setAttributeProperty( IN const DATAPTR_ATTRIBUTE attributePtr,
						   IN_ENUM( ATTRIBUTE_PROPERTY ) \
								ATTRIBUTE_PROPERTY_TYPE property,
						   IN_INT_Z const int optValue )
	{
	ATTRIBUTE_LIST *attributeListPtr;

	REQUIRES_V( DATAPTR_ISSET( attributePtr ) );
	REQUIRES_V( isEnumRange( property, ATTRIBUTE_PROPERTY ) );
	REQUIRES_V( optValue >= 0 );
	attributeListPtr = DATAPTR_GET( attributePtr );
	REQUIRES_V( attributeListPtr != NULL );

	switch( property )
		{
		case ATTRIBUTE_PROPERTY_CRITICAL:
			REQUIRES_V( optValue == 0 );

			SET_FLAG( attributeListPtr->flags, ATTR_FLAG_CRITICAL );
			return;

		case ATTRIBUTE_PROPERTY_VALUE:
			REQUIRES_V( optValue > 0 );

			attributeListPtr->intValue = optValue;
			return;

		case ATTRIBUTE_PROPERTY_LOCKED:
			REQUIRES_V( optValue == 0 );

			SET_FLAG( attributeListPtr->flags, ATTR_FLAG_LOCKED );
			return;
		}
	
	retIntError_Void();
	}

/* Get attribute ID information */

CHECK_RETVAL \
int getAttributeIdInfo( IN const DATAPTR_ATTRIBUTE attributePtr,
						OUT_OPT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *attributeID,
						OUT_OPT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *fieldID,
						OUT_OPT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *subFieldID )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	assert( attributeID == NULL || \
			isWritePtr( attributeID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( fieldID == NULL || \
			isWritePtr( fieldID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( subFieldID == NULL || \
			isWritePtr( subFieldID, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );

	REQUIRES( DATAPTR_ISSET( attributePtr ) );
	REQUIRES( attributeID != NULL || fieldID != NULL || subFieldID != NULL );
	attributeListPtr = DATAPTR_GET( attributePtr );
	REQUIRES( attributeListPtr != NULL );

	/* If this is a blob attribute then the ID values aren't present */
	if( checkAttributeListProperty( attributeListPtr,
									ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
		return( CRYPT_ERROR_NOTFOUND );

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

CHECK_RETVAL_DATAPTR STDC_NONNULL_ARG( ( 1 ) ) \
CONST_RETURN DATAPTR_ATTRIBUTE getFirstAttribute( OUT ATTRIBUTE_ENUM_INFO *attrEnumInfo,
												  IN_DATAPTR_OPT \
														const DATAPTR_ATTRIBUTE attributePtr,
												  IN_ENUM( ATTRIBUTE_ENUM ) \
														const ATTRIBUTE_ENUM_TYPE enumType )
	{
	assert( isWritePtr( attrEnumInfo, sizeof( ATTRIBUTE_ENUM_INFO ) ) );

	REQUIRES_D( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES_D( isEnumRange( enumType, ATTRIBUTE_ENUM ) );

	/* Clear return value */
	memset( attrEnumInfo, 0, sizeof( ATTRIBUTE_ENUM_INFO ) );
	attrEnumInfo->attributePtr = attributePtr;
	attrEnumInfo->enumType = enumType;

	/* Early-out check for empty attribute lists */
	if( DATAPTR_ISNULL( attributePtr ) )
		return( DATAPTR_NULL );

	switch( enumType )
		{
		case ATTRIBUTE_ENUM_BLOB:
			{
			DATAPTR_ATTRIBUTE attributeCursor;
			const ATTRIBUTE_LIST *attributeListPtr;
			int LOOP_ITERATOR;

			/* Blob attributes are a special case because they're preceded 
			   in the attribute list by non-blob attributes so before we try 
			   and enumerate blob attributes we have to skip any non-blob
			   ones that may be present */
			LOOP_LARGE( ( attributeCursor = attributePtr,
						  attributeListPtr = DATAPTR_GET( attributePtr ) ),
						attributeListPtr != NULL && \
							!checkAttributeProperty( attributeCursor, 
													 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ),
						( attributeCursor = attributeListPtr->next,
						  attributeListPtr = DATAPTR_GET( attributeListPtr->next ) ) )
				{
				REQUIRES_D( sanityCheckAttributePtr( attributeListPtr ) );
				}
			ENSURES_D( LOOP_BOUND_OK );
			attrEnumInfo->attributePtr = attributeCursor;

			/* If there are no blob attributes, we're done */
			if( DATAPTR_ISNULL( attributeCursor ) )
				return( DATAPTR_NULL );

			break;
			}

		case ATTRIBUTE_ENUM_NONBLOB:
			/* If there are no non-blob attributes, we're done */
			if( checkAttributeProperty( attributePtr, 
										ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
				{
				DATAPTR_SET( attrEnumInfo->attributePtr, NULL );
				return( DATAPTR_NULL );
				}
			break;

		default:
			retIntError_Dataptr();
		}

	return( attrEnumInfo->attributePtr );
	}

CHECK_RETVAL_DATAPTR STDC_NONNULL_ARG( ( 1 ) ) \
CONST_RETURN DATAPTR_ATTRIBUTE getNextAttribute( INOUT ATTRIBUTE_ENUM_INFO *attrEnumInfo )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	assert( isWritePtr( attrEnumInfo, sizeof( ATTRIBUTE_ENUM_INFO ) ) );

	REQUIRES_D( DATAPTR_ISSET( attrEnumInfo->attributePtr ) );

	/* Move on to the next attribute in the list */
	attributeListPtr = DATAPTR_GET( attrEnumInfo->attributePtr );
	ENSURES_D( attributeListPtr != NULL );
	attrEnumInfo->attributePtr = attributeListPtr->next;
	if( DATAPTR_ISNULL( attrEnumInfo->attributePtr ) )
		return( DATAPTR_NULL );

	switch( attrEnumInfo->enumType )
		{
		case ATTRIBUTE_ENUM_BLOB:
			break;

		case ATTRIBUTE_ENUM_NONBLOB:
			/* If there are no more non-blob attributes, we're done */
			if( checkAttributeProperty( attrEnumInfo->attributePtr, 
										ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
				{
				DATAPTR_SET( attrEnumInfo->attributePtr, NULL );
				return( DATAPTR_NULL );
				}
			break;

		default:
			retIntError_Dataptr();
		}

	return( attrEnumInfo->attributePtr );
	}

/* Get attribute data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int getAttributeDataValue( IN const DATAPTR_ATTRIBUTE attributePtr,
						   OUT_INT_Z int *value )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	REQUIRES( DATAPTR_ISSET( attributePtr ) );
	attributeListPtr = DATAPTR_GET( attributePtr );
	ENSURES( attributeListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int getAttributeDataTime( IN const DATAPTR_ATTRIBUTE attributePtr,
						  OUT time_t *value )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	REQUIRES( DATAPTR_ISSET( attributePtr ) );
	attributeListPtr = DATAPTR_GET( attributePtr );
	ENSURES( attributeListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );
	REQUIRES( attributeListPtr->fieldType == BER_TIME_GENERALIZED || \
			  attributeListPtr->fieldType == BER_TIME_UTC );

	*value = *( ( time_t * ) attributeListPtr->dataValue );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int getAttributeDataDN( IN const DATAPTR_ATTRIBUTE attributePtr,
						OUT_DATAPTR_COND DATAPTR_DN *dnPtr )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	REQUIRES( DATAPTR_ISSET( attributePtr ) );
	attributeListPtr = DATAPTR_GET( attributePtr );
	ENSURES( attributeListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );
	REQUIRES( attributeListPtr->fieldType == FIELDTYPE_DN );

	*dnPtr = GET_DN_POINTER( attributeListPtr );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int getAttributeDataDNPtr( IN const DATAPTR_ATTRIBUTE attributePtr,
						   OUT_PTR DATAPTR_DN **dnPtrPtr )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	REQUIRES( DATAPTR_ISSET( attributePtr ) );
	attributeListPtr = DATAPTR_GET( attributePtr );
	ENSURES( attributeListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );
	REQUIRES( attributeListPtr->fieldType == FIELDTYPE_DN );

	/* See the comment by the definition of SELECTION_INFO in cert.h for the
	   reason why we return the address of the pointer rather than the 
	   pointer itself */
	*dnPtrPtr = ( DATAPTR_DN * ) &attributeListPtr->dnValue;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int getAttributeDataPtr( IN const DATAPTR_ATTRIBUTE attributePtr,
						 OUT_BUFFER_ALLOC( *dataLength ) void **dataPtrPtr, 
						 OUT_LENGTH_SHORT_Z int *dataLength )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	REQUIRES( DATAPTR_ISSET( attributePtr ) );
	attributeListPtr = DATAPTR_GET( attributePtr );
	ENSURES( attributeListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );
	REQUIRES( attributeListPtr->fieldType == BER_OCTETSTRING || \
			  attributeListPtr->fieldType == BER_OBJECT_IDENTIFIER || \
			  attributeListPtr->fieldType == BER_TIME_GENERALIZED || \
			  attributeListPtr->fieldType == BER_TIME_UTC || \
			  isStringField( attributeListPtr->fieldType ) || \
			  attributeListPtr->fieldType == FIELDTYPE_TEXTSTRING || \
			  isBlobField( attributeListPtr->fieldType ) );

	*dataPtrPtr = attributeListPtr->dataValue;
	*dataLength = attributeListPtr->dataValueLength;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int getBlobAttributeDataPtr( IN const DATAPTR_ATTRIBUTE attributePtr,
							 OUT_BUFFER_ALLOC( *dataLength ) void **dataPtrPtr, 
							 OUT_LENGTH_SHORT_Z int *dataLength )
	{
	const ATTRIBUTE_LIST *attributeListPtr;

	/* This function is identical to getAttributeDataPtr() except for a 
	   type-checking issue, it returns a pointer to a blob attribute's 
	   data */
	REQUIRES( DATAPTR_ISSET( attributePtr ) );
	attributeListPtr = DATAPTR_GET( attributePtr );
	ENSURES( attributeListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );
	REQUIRES( checkAttributeListProperty( attributeListPtr,
										  ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) );

	*dataPtrPtr = attributeListPtr->dataValue;
	*dataLength = attributeListPtr->dataValueLength;

	return( CRYPT_OK );
	}

/* The pattern { findAttributeField(), getAttributeDataXXX() } where XXX ==
   { Value, Time } is used frequently enough that we provide a standard 
   function for it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
int getAttributeFieldValue( IN_DATAPTR_OPT const DATAPTR_ATTRIBUTE attributePtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
							IN_ATTRIBUTE_OPT \
								const CRYPT_ATTRIBUTE_TYPE subFieldID,
							OUT_INT_Z int *value )
	{
	DATAPTR_ATTRIBUTE attributeCursor;

	REQUIRES( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES( isValidExtension( fieldID ) );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );

	/* Clear return value */
	*value = 0;

	/* Find the required attribute field and return its value */
	attributeCursor = findAttributeField( attributePtr, fieldID, 
										  subFieldID );
	if( DATAPTR_ISNULL( attributeCursor ) )
		return( CRYPT_ERROR_NOTFOUND );
	return( getAttributeDataValue( attributeCursor, value ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
int getAttributeFieldTime( IN_DATAPTR_OPT const DATAPTR_ATTRIBUTE attributePtr,
						   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
						   IN_ATTRIBUTE_OPT \
								const CRYPT_ATTRIBUTE_TYPE subFieldID,
						   OUT time_t *value )
	{
	DATAPTR_ATTRIBUTE attributeCursor;

	REQUIRES( DATAPTR_ISVALID( attributePtr ) );
	REQUIRES( isValidExtension( fieldID ) );
	REQUIRES( subFieldID == CRYPT_ATTRIBUTE_NONE || \
			  ( subFieldID >= CRYPT_CERTINFO_FIRST_NAME && \
				subFieldID <= CRYPT_CERTINFO_LAST_GENERALNAME ) );

	/* Clear return value */
	*value = 0;

	/* Find the required attribute field and return its value */
	attributeCursor = findAttributeField( attributePtr, fieldID, 
										  subFieldID );
	if( DATAPTR_ISNULL( attributeCursor ) )
		return( CRYPT_ERROR_NOTFOUND );
	return( getAttributeDataTime( attributeCursor, value ) );
	}

/* Get the default value for an optional field of an attribute */

CHECK_RETVAL \
int getDefaultFieldValue( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr;

	REQUIRES( isValidExtension( fieldID ) );

	attributeInfoPtr = \
		fieldIDToAttribute( ( fieldID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
							ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE, fieldID, 
							CRYPT_ATTRIBUTE_NONE, NULL );
	ENSURES( attributeInfoPtr != NULL );

	return( attributeInfoPtr->defaultValue );
	}

/****************************************************************************
*																			*
*							Attribute Compare Routines						*
*																			*
****************************************************************************/

#ifdef USE_PKIUSER

/* Compare attribute fields */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN compareAttributeField( IN const ATTRIBUTE_LIST *attributeField1,
									  IN const ATTRIBUTE_LIST *attributeField2 )
	{
	assert( isReadPtr( attributeField1, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeField2, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_B( sanityCheckAttributePtr( attributeField1 ) );
	REQUIRES_B( sanityCheckAttributePtr( attributeField2 ) );
	REQUIRES_B( attributeField1->attributeID == attributeField2->attributeID );

	/* Compare attribute IDs */
	if( attributeField1->fieldID != attributeField2->fieldID || \
		attributeField1->subFieldID != attributeField2->subFieldID )
		return( FALSE );

	/* Compare the field type and any relevant attribute flags */
	if( attributeField1->fieldType != attributeField2->fieldType )
		return( FALSE );
	if( GET_FLAGS( attributeField1->flags, ATTR_FLAGS_COMPARE_MASK ) != \
			GET_FLAGS( attributeField2->flags, ATTR_FLAGS_COMPARE_MASK ) )
		return( FALSE );

	/* Compare field data */
	if( attributeField1->fieldType == FIELDTYPE_DN )
		{
		const DATAPTR_DN dn1 = GET_DN_POINTER( attributeField1 );
		const DATAPTR_DN dn2 = GET_DN_POINTER( attributeField2 );

		REQUIRES_B( DATAPTR_ISVALID( dn1 ) );
		REQUIRES_B( DATAPTR_ISVALID( dn2 ) );

		/* DNs are structured data and need to be compared specially */
		return( compareDN( dn1, dn2, FALSE, NULL ) );
		}
	if( attributeField1->fieldType == FIELDTYPE_CHOICE || \
		attributeField1->fieldType == FIELDTYPE_IDENTIFIER )
		{
		/* Choice/identifier fields store the selection value in the 
		   intValue */
		return( ( attributeField1->intValue == attributeField2->intValue ) ? \
				TRUE : FALSE );
		}
	if( attributeField1->fieldType == BER_NULL )
		{
		/* A BER_NULL has no associated data and always compares as 
		   true */
		return( TRUE );
		}
	if( attributeField1->fieldType == BER_INTEGER || \
		attributeField1->fieldType == BER_ENUMERATED || \
		attributeField1->fieldType == BER_BITSTRING || \
		attributeField1->fieldType == BER_BOOLEAN )
		{
		return( ( attributeField1->intValue == attributeField2->intValue ) ? \
				TRUE : FALSE );
		}
	REQUIRES_B( attributeField1->fieldType == BER_OCTETSTRING || \
				attributeField1->fieldType == BER_OBJECT_IDENTIFIER || \
				attributeField1->fieldType == BER_TIME_GENERALIZED || \
				attributeField1->fieldType == BER_TIME_UTC || \
				isStringField( attributeField1->fieldType ) || \
				attributeField1->fieldType == FIELDTYPE_TEXTSTRING || \
				isBlobField( attributeField1->fieldType ) );
	if( attributeField1->dataValueLength != \
							attributeField2->dataValueLength )
		return( FALSE );
	if( attributeField1->dataValueLength > 0 )
		{
		if( memcmp( attributeField1->dataValue, attributeField2->dataValue,
					attributeField1->dataValueLength ) )
			return( FALSE );
		}

	return( TRUE );
	}

/* Compare attributes */

CHECK_RETVAL_BOOL \
BOOLEAN compareAttribute( IN_DATAPTR const DATAPTR_ATTRIBUTE attribute1,
						  IN_DATAPTR const DATAPTR_ATTRIBUTE attribute2 )
	{
	const ATTRIBUTE_LIST *attributeListPtr1;
	const ATTRIBUTE_LIST *attributeListPtr2;
	CRYPT_ATTRIBUTE_TYPE attributeID;
	int LOOP_ITERATOR;

	REQUIRES_B( DATAPTR_ISSET( attribute1 ) );
	REQUIRES_B( DATAPTR_ISSET( attribute2 ) );
	attributeListPtr1 = DATAPTR_GET( attribute1 );
	attributeListPtr2 = DATAPTR_GET( attribute2 );
	REQUIRES_B( attributeListPtr1 != NULL && attributeListPtr2 != NULL );
	REQUIRES_B( attributeListPtr1->attributeID == \
										attributeListPtr2->attributeID );
	attributeID = attributeListPtr1->attributeID;

	/* Compare all of the fields in the attributes */
	LOOP_LARGE_CHECKINC( attributeListPtr1 != NULL && attributeListPtr2 != NULL && \
							attributeListPtr1->attributeID == attributeID && \
							attributeListPtr2->attributeID == attributeID,
						 ( attributeListPtr1 = DATAPTR_GET( attributeListPtr1->next ), \
						   attributeListPtr2 = DATAPTR_GET( attributeListPtr2->next ) ) )
		{
		if( !compareAttributeField( attributeListPtr1, attributeListPtr2 ) )
			return( FALSE );
		}
	ENSURES_B( LOOP_BOUND_OK );

	/* If the first attribute list terminated, make sure that the attribute 
	   doesn't continue in the second list */
	if( attributeListPtr1 == NULL )
		{
		if( attributeListPtr2 != NULL && \
			attributeListPtr2->attributeID == attributeID )
			return( FALSE );

		/* Both attribute lists terminated at the same point */
		return( TRUE );
		}

	/* If the second attribute list terminated, make sure that the attribute 
	   doesn't continue in the first list */
	if( attributeListPtr2 == NULL )
		{
		if( attributeListPtr1->attributeID == attributeID )
			return( FALSE );

		/* Both attribute lists terminated at the same point */
		return( TRUE );
		}

	/* There are more attributes in the list, make sure that the current 
	   attribute doesn't continue in either list */
	if( attributeListPtr1->attributeID == attributeID || \
		attributeListPtr2->attributeID == attributeID )
		return( FALSE );

	return( TRUE );
	}
#endif /* USE_PKIUSER */

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
		{
		status = convertEmail( certInfoPtr, &certInfoPtr->issuerName,
							   CRYPT_CERTINFO_ISSUERALTNAME );
		}
	if( cryptStatusError( status ) )
		return( status );

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* If we're running at a compliance level of 
	   CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL or above, don't try and compensate
	   for dubious attributes */
	status = krnlSendMessage( certInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &complianceLevel, 
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		return( status );
	if( complianceLevel >= CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL )
		return( CRYPT_OK );
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

#ifdef USE_CERT_OBSOLETE
	/* If the only key usage information present is the Netscape one, 
	   convert it into the X.509 equivalent */
	if( !checkAttributePresent( certInfoPtr->attributes, 
								CRYPT_CERTINFO_KEYUSAGE ) )
		{
		DATAPTR_ATTRIBUTE attributePtr;

		attributePtr = findAttributeField( certInfoPtr->attributes, 
										   CRYPT_CERTINFO_NS_CERTTYPE, 
										   CRYPT_ATTRIBUTE_NONE );
		if( DATAPTR_ISSET( attributePtr ) )
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
		}
#endif /* USE_CERT_OBSOLETE */

	return( CRYPT_OK );
	}
#endif /* USE_CERTIFICATES */
