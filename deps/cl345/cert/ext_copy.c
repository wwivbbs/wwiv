/****************************************************************************
*																			*
*						Certificate Attribute Copy Routines					*
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

/* When replicating attributes from one type of certificate object to 
   another (for example from an issuer certificate to a subject certificate 
   when issuing a new certificate) we may have to adjust the attribute 
   information based on the source and destination object roles.  The 
   following values denote the different copy types that we have to handle.  
   Usually this is a direct copy, however if we're copying from subject to 
   issuer we have to adjust attribute IDs such as the altName 
   (subjectAltName -> issuerAltName), if we're copying from issuer to 
   subject we have to adjust path length-based contraints since the new 
   subject is one further down the chain than the issuer */

typedef enum {
	COPY_NONE,				/* No copy type */
	COPY_DIRECT,			/* Direct attribute copy */
	COPY_SUBJECT_TO_ISSUER,	/* Copy of subject attributes to issuer cert */
	COPY_ISSUER_TO_SUBJECT,	/* Copy of issuer attributes to subject cert */
	COPY_LAST				/* Last valid copy type */
	} COPY_TYPE;

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Make a copy of an attribute field, used for copying attributes from a 
   source attribute list to a new destination */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyAttributeField( OUT_PTR_COND ATTRIBUTE_LIST **newDestAttributeField,
							   IN const ATTRIBUTE_LIST *srcAttributeField )
	{
	ATTRIBUTE_LIST *newElement;
	int status = CRYPT_OK;

	assert( isWritePtr( newDestAttributeField, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtr( srcAttributeField, sizeof( ATTRIBUTE_LIST ) ) );

	/* Allocate memory for the new element and copy the information across */
	*newDestAttributeField = NULL;
	if( ( newElement = ( ATTRIBUTE_LIST * ) \
					   clAlloc( "copyAttributeField", \
								sizeofVarStruct( srcAttributeField, \
												 ATTRIBUTE_LIST ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	copyVarStruct( newElement, srcAttributeField, ATTRIBUTE_LIST, dataValue );
	if( srcAttributeField->fieldType == FIELDTYPE_DN )
		{
		DATAPTR_DN dn = GET_DN_POINTER( newElement );
		DATAPTR_DN srcDN = GET_DN_POINTER( srcAttributeField );

		REQUIRES( DATAPTR_ISVALID( dn ) );
		REQUIRES( DATAPTR_ISSET( srcDN ) );

		/* If the field contains a DN, copy that across as well */
		status = copyDN( &dn, srcDN );
		if( cryptStatusError( status ) )
			{
			endVarStruct( newElement, ATTRIBUTE_LIST );
			clFree( "copyAttributeField", newElement );
			return( status );
			}
		SET_DN_POINTER( newElement, dn );
		}
	if( checkAttributeListProperty( srcAttributeField, 
									ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
		{
		/* For blob-type attributes the OID is stored at the end of the
		   attribute data, so we have to update the pointer to this after
		   the copy */
		newElement->oid = newElement->storage + \
						  newElement->dataValueLength;
		}
	DATAPTR_SET( newElement->next, NULL );
	DATAPTR_SET( newElement->prev, NULL );
	ENSURES( sanityCheckAttributePtr( newElement ) );
	*newDestAttributeField = newElement;

	return( CRYPT_OK );
	}

/* Copy an attribute from one attribute list to another */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
static int fixAttributeInfo( INOUT ATTRIBUTE_LIST *attributeListPtr,
							 IN_ENUM( COPY ) const COPY_TYPE copyType,
							 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
							 IN const BOOLEAN targetIsCA )
	{
	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( isEnumRange( copyType, COPY ) );
	REQUIRES( isValidExtension( attributeID ) );
	REQUIRES( targetIsCA == FALSE || targetIsCA == TRUE );

	/* If we're copying from an issuer to a subject attribute list and the 
	   field is an altName or keyIdentifier, change the field type from 
	   issuer.subjectAltName to subject.issuerAltName or
	   issuer.subjectKeyIdentifier to subject.authorityKeyIdentifier */
	if( copyType == COPY_SUBJECT_TO_ISSUER )
		{
		if( attributeID == CRYPT_CERTINFO_SUBJECTALTNAME )
			{
			attributeListPtr->attributeID = \
					attributeListPtr->fieldID = \
								CRYPT_CERTINFO_ISSUERALTNAME;
			}
		if( attributeID == CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER )
			{
			attributeListPtr->attributeID = \
								CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER;
			attributeListPtr->fieldID = \
								CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER;
			}
		}

	/* If we're copying from a subject to an issuer attribute list and it's 
	   a path length-based constraint, adjust the constraint value by one 
	   since we're now one further down the chain */
	if( copyType == COPY_ISSUER_TO_SUBJECT )
		{
		if( attributeListPtr->fieldID == \
								CRYPT_CERTINFO_PATHLENCONSTRAINT || \
			attributeListPtr->fieldID == \
								CRYPT_CERTINFO_REQUIREEXPLICITPOLICY || \
			attributeListPtr->fieldID == \
								CRYPT_CERTINFO_INHIBITPOLICYMAPPING )
			{
			/* If we're already at a path length of zero then we can't 
			   reduce it any further */
			if( attributeListPtr->intValue <= 0 )
				{
				/* If the copy target is a CA then this is an error */
				if( targetIsCA )
					return( CRYPT_ERROR_INVALID );
				
				/* The target is an EE, let the caller know that this 
				   attribute shouldn't be copied */
				return( OK_SPECIAL );
				}

			attributeListPtr->intValue--;
			}
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int copyAttribute( INOUT_PTR DATAPTR_ATTRIBUTE *destHeadPtr,
						  IN const DATAPTR_ATTRIBUTE srcPtr,
						  IN_ENUM( COPY ) const COPY_TYPE copyType )
	{
	CRYPT_ATTRIBUTE_TYPE attributeID, newAttributeID;
	DATAPTR_ATTRIBUTE destHead = *destHeadPtr, newAttributeHead;
	DATAPTR_ATTRIBUTE *newAttributeHeadPtr = &newAttributeHead;
	ATTRIBUTE_LIST *srcListPtr, *insertPoint, *prevElement = NULL;
	ATTRIBUTE_LIST *newAttributeListHead;
	ATTRIBUTE_LIST *newAttributeListTail DUMMY_INIT_PTR;
	BOOLEAN targetIsCA = FALSE;
	int LOOP_ITERATOR;

	assert( isWritePtr( destHeadPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	
	REQUIRES( DATAPTR_ISSET( srcPtr ) );
	REQUIRES( isEnumRange( copyType, COPY ) );

	srcListPtr = DATAPTR_GET( srcPtr );
	REQUIRES( srcListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( srcListPtr ) );
	attributeID = newAttributeID = srcListPtr->attributeID;

	/* If we're re-mapping the destination attribute ID (see the comment
	   further down) we have to insert it at a point corresponding to the 
	   re-mapped ID, not the original ID, to maintain the list's sorted
	   property.  The CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER is only
	   copied at CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL or above so we'll never
	   see this attribute copied at lower compliance levels (this is 
	   enforced in the caller) */
	if( copyType == COPY_SUBJECT_TO_ISSUER )
		{
		if( attributeID == CRYPT_CERTINFO_SUBJECTALTNAME )
			newAttributeID = CRYPT_CERTINFO_ISSUERALTNAME;
		if( attributeID == CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER )
			newAttributeID = CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER;
		}

	/* If we're copying from a CA certificate, determine whether the copy
	   target is also a CA.  This affects the copying of length constraint
	   extensions */
	if( copyType == COPY_ISSUER_TO_SUBJECT )
		{
		DATAPTR_ATTRIBUTE attributePtr;

		attributePtr = findAttributeField( destHead, CRYPT_CERTINFO_CA, 
										   CRYPT_ATTRIBUTE_NONE );
		if( DATAPTR_ISSET( attributePtr ) )
			targetIsCA = TRUE;
		}

	/* Find the location at which to insert this attribute (this assumes 
	   that the fieldIDs are defined in sorted order) */
	LOOP_LARGE( insertPoint = DATAPTR_GET( destHead ), 
				insertPoint != NULL && \
					insertPoint->attributeID < newAttributeID && \
					insertPoint->fieldID != CRYPT_ATTRIBUTE_NONE,
				insertPoint = DATAPTR_GET( insertPoint->next ) )
		{
		prevElement = insertPoint;
		}
	ENSURES( LOOP_BOUND_OK );
	insertPoint = prevElement;

	/* Build a new attribute list containing the attribute fields */
	DATAPTR_SET_PTR( newAttributeHeadPtr, NULL );
	LOOP_LARGE_CHECKINC( srcListPtr != NULL && \
							srcListPtr->attributeID == attributeID,
						 srcListPtr = DATAPTR_GET( srcListPtr->next ) )
		{
		ATTRIBUTE_LIST *newAttributeField;
		int status;

		REQUIRES( sanityCheckAttributePtr( srcListPtr ) );

		/* Copy the field across */
		status = copyAttributeField( &newAttributeField, srcListPtr );
		if( cryptStatusError( status ) )
			{
			if( DATAPTR_ISSET_PTR( newAttributeHeadPtr ) )
				deleteAttributes( newAttributeHeadPtr );
			return( status );
			}

		/* Perform any attribute fixups due to issuer-to-subject/subject-to-
		   issuer copying */
		status = fixAttributeInfo( newAttributeField, copyType, attributeID, 
								   targetIsCA );
		if( cryptStatusError( status ) )
			{
			DATAPTR_ATTRIBUTE newAttribute;

			DATAPTR_SET( newAttribute, newAttributeField );
			( void ) deleteAttributeField( &newAttribute, NULL,	
										   newAttribute, NULL );
			if( status == OK_SPECIAL )
				{
				/* This attribute shouldn't be copied, contine with the next 
				   one */
				continue;
				}
			if( DATAPTR_ISSET_PTR( newAttributeHeadPtr ) )
				deleteAttributes( newAttributeHeadPtr );
			return( status );
			}

		/* Append the new field to the new attribute list */
		insertDoubleListElement( newAttributeHeadPtr, newAttributeListTail, 
								 newAttributeField, ATTRIBUTE_LIST );
		newAttributeListTail = newAttributeField;
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( DATAPTR_ISSET_PTR( newAttributeHeadPtr ) );
	newAttributeListHead = DATAPTR_GET_PTR( newAttributeHeadPtr );
	ENSURES( newAttributeListHead != NULL );

	/* Link the new list into the existing list at the appropriate position */
	insertDoubleListElements( destHeadPtr, insertPoint, 
							  newAttributeListHead, 
							  newAttributeListTail, ATTRIBUTE_LIST );

	return( CRYPT_OK );
	}

/* Copy a length constraint from an issuer CA certificate to a subject CA
   certificate, decrementing the value by one */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int copyLengthConstraint( INOUT DATAPTR_ATTRIBUTE *destHeadPtr,
								 IN const DATAPTR_ATTRIBUTE srcPtr,
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
								 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus )

	{
	DATAPTR_ATTRIBUTE destHead = *destHeadPtr, srcAttribute;
	ATTRIBUTE_LIST *srcListPtr, *destListPtr;

	assert( isWritePtr( destHeadPtr, sizeof( DATAPTR_ATTRIBUTE  ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	
	REQUIRES( DATAPTR_ISSET( srcPtr ) );
#ifdef USE_CERTLEVEL_PKIX_FULL
	REQUIRES( fieldID == CRYPT_CERTINFO_PATHLENCONSTRAINT || \
			  fieldID == CRYPT_CERTINFO_REQUIREEXPLICITPOLICY || \
			  fieldID == CRYPT_CERTINFO_INHIBITPOLICYMAPPING );
#else
	REQUIRES( fieldID == CRYPT_CERTINFO_PATHLENCONSTRAINT );
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* Clear return value */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;

	srcListPtr = DATAPTR_GET( srcPtr );
	REQUIRES( srcListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( srcListPtr ) );

	/* If there's nothing to copy, we're done */
	srcAttribute = findAttributeField( srcPtr, fieldID, CRYPT_ATTRIBUTE_NONE );
	if( DATAPTR_ISNULL( srcAttribute ) )
		return( CRYPT_OK );
	srcListPtr = DATAPTR_GET( srcAttribute );
	ENSURES( srcListPtr != NULL );

	/* There's a length constraint present, if the value is already at zero 
	   then the next certificate must be an EE certificate rather than a
	   CA certificate */
	if( srcListPtr->intValue <= 0 )
		{
		*errorLocus = fieldID;
		return( CRYPT_ERROR_INVALID );
		}

	/* There's something to copy, if it's not already present in the 
	   destination just copy it across */
	destHead = findAttributeField( destHead, fieldID, 
								   CRYPT_ATTRIBUTE_NONE );
	if( DATAPTR_ISNULL( destHead ) )
		{
		CRYPT_ERRTYPE_TYPE dummy;

		/* Copy the field across and update the constraint value */
		return( addAttributeField( destHeadPtr, fieldID, 
								   CRYPT_ATTRIBUTE_NONE,
								   srcListPtr->intValue - 1,
								   GET_FLAGS( srcListPtr->flags, 
											  ATTR_FLAG_MAX ), 
								   errorLocus, &dummy ) );
		}

	/* The same constraint exists in source and destination, set the result
	   value to the lesser of the two, with the necessary decrement applied */
	destListPtr = DATAPTR_GET( destHead );
	ENSURES( destListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( destListPtr  ) );
	if( srcListPtr->intValue <= destListPtr->intValue )
		destListPtr->intValue = srcListPtr->intValue - 1;
	ENSURES( destListPtr->intValue >= 0 );

	return( CRYPT_OK );
	}					

/****************************************************************************
*																			*
*							Copy a Complete Attribute List					*
*																			*
****************************************************************************/

/* Copy a complete attribute list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int copyAttributes( INOUT DATAPTR_ATTRIBUTE *destHeadPtr,
					IN const DATAPTR_ATTRIBUTE srcPtr,
					OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
					OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType )
	{
	const DATAPTR_ATTRIBUTE destHead = *destHeadPtr;
	const ATTRIBUTE_LIST *srcListPtr;
	int LOOP_ITERATOR;

	assert( isWritePtr( destHeadPtr, sizeof( DATAPTR_ATTRIBUTE  ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISSET( srcPtr ) );

	/* Clear return values */
	DATAPTR_SET_PTR( destHeadPtr, NULL );
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	srcListPtr = DATAPTR_GET( srcPtr );
	REQUIRES( srcListPtr != NULL );
	REQUIRES( sanityCheckAttributePtr( srcListPtr ) );

	/* If there are destination attributes present make a first pass down 
	   the list checking that the attribute to copy isn't already present in 
	   the destination attributes, first for recognised attributes and then 
	   for unrecognised ones.  We have to do this separately since once we 
	   begin the copy process it's rather hard to undo it.  There are two 
	   special cases that we could in theory allow:

		1. Some composite attributes could have non-overlapping fields in 
		   the source and destination.

		2. Some attributes can have multiple instances of a field present.

	   This means that we could allow them to appear in both the source and 
	   destination lists, however if this occurs it's more likely to be an 
	   error than a desire to merge two disparate collections of fields or 
	   attributes so we report them as (disallowed) duplicates */
	if( DATAPTR_ISSET( destHead ) )
		{
		const ATTRIBUTE_LIST *attributeListCursor;

		/* Check the non-blob attributes */
		LOOP_LARGE( attributeListCursor = DATAPTR_GET( srcPtr ), 
					attributeListCursor != NULL && \
						!checkAttributeListProperty( attributeListCursor,
													 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ),
					attributeListCursor = DATAPTR_GET( attributeListCursor->next ) )
			{
			ATTRIBUTE_LIST *attributeListCursorNext;
			DATAPTR_ATTRIBUTE attributePtr;

			REQUIRES( sanityCheckAttributePtr( attributeListCursor ) );
			attributeListCursorNext = DATAPTR_GET( attributeListCursor->next );
			ENSURES( attributeListCursorNext == NULL || \
					 !isValidAttributeField( attributeListCursorNext ) || \
					 attributeListCursor->attributeID <= \
							attributeListCursorNext->attributeID );
			attributePtr = findAttributeField( destHead,
											   attributeListCursor->fieldID, 
											   CRYPT_ATTRIBUTE_NONE );
			if( DATAPTR_ISSET( attributePtr ) )
				{
				*errorLocus = attributeListCursor->fieldID;
				*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
				return( CRYPT_ERROR_DUPLICATE );
				}
			}
		ENSURES( LOOP_BOUND_OK );

		/* Check the blob attributes */
		LOOP_LARGE_CHECKINC( attributeListCursor != NULL,
							 attributeListCursor = DATAPTR_GET( attributeListCursor->next ) )
			{
			DATAPTR_ATTRIBUTE attributePtr;

			REQUIRES( sanityCheckAttributePtr( attributeListCursor ) );
			ENSURES( checkAttributeListProperty( attributeListCursor,
												 ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) );
			attributePtr = findAttributeByOID( destHead, attributeListCursor->oid,
											   sizeofOID( attributeListCursor->oid ) );
			if( DATAPTR_ISSET( attributePtr ) )
				{
				/* We can't set the locus for blob-type attributes since 
				   it's not a known attribute */
				*errorLocus = CRYPT_ATTRIBUTE_NONE;
				*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
				return( CRYPT_ERROR_DUPLICATE );
				}
			}
		ENSURES( LOOP_BOUND_OK );
		}

	/* Make a second pass copying everything across, first the non-blob 
	   attributes */
	LOOP_LARGE_CHECK( srcListPtr != NULL && \
					  !checkAttributeListProperty( srcListPtr, 
												   ATTRIBUTE_PROPERTY_BLOBATTRIBUTE ) )
		{
		CRYPT_ATTRIBUTE_TYPE attributeID = srcListPtr->attributeID;
		const ATTRIBUTE_INFO *attributeInfoPtr;
		int status, LOOP_ITERATOR_ALT;

		/* Get the attribute information for the attribute to be copied */
		if( srcListPtr->attributeInfoPtr != NULL )
			attributeInfoPtr = srcListPtr->attributeInfoPtr;
		else
			{
			attributeInfoPtr = \
				fieldIDToAttribute( ( attributeID >= CRYPT_CERTINFO_FIRST_CMS ) ? \
										ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE,
									attributeID, CRYPT_ATTRIBUTE_NONE, NULL );
			}
		ENSURES( attributeInfoPtr != NULL );

		assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

		/* Copy the complete attribute across unless it's one that we 
		   explicitly don't propagate from source to destination */
		if( !( attributeInfoPtr->typeInfoFlags & FL_ATTR_NOCOPY ) )
			{
			DATAPTR_ATTRIBUTE srcAttribute;

			DATAPTR_SET( srcAttribute, ( ATTRIBUTE_LIST * ) srcListPtr );
			status = copyAttribute( destHeadPtr, srcAttribute, COPY_DIRECT );
			if( cryptStatusError( status ) )
				return( status );
			}

		/* Move on to the next attribute.  Since each loop restarts the 
		   bounds check value this unfortunately makes the combined bound
		   2*n rather than just n, but there's no easy way to access the
		   bounds variable from the outer loop */
		LOOP_LARGE_CHECKINC_ALT( srcListPtr != NULL && \
									srcListPtr->attributeID == attributeID,
								 srcListPtr = DATAPTR_GET( srcListPtr->next ) );
		ENSURES( LOOP_BOUND_OK_ALT );
		}
	ENSURES( LOOP_BOUND_OK );

	/* If there are blob-type attributes left at the end of the list, copy
	   them across last */
	if( srcListPtr != NULL )
		{
		ATTRIBUTE_LIST *insertPoint;

		/* Find the end of the destination list */
		LOOP_LARGE( insertPoint = DATAPTR_GET( destHead ), 
					insertPoint != NULL && DATAPTR_ISSET( insertPoint->next ),
					insertPoint = DATAPTR_GET( insertPoint->next ) );
		ENSURES( LOOP_BOUND_OK );

		/* Copy all remaining attributes across */
		LOOP_LARGE_CHECKINC( srcListPtr != NULL,
							 srcListPtr = DATAPTR_GET( srcListPtr->next ) )
			{
			ATTRIBUTE_LIST *newAttribute;
			int status;

			status = copyAttributeField( &newAttribute, srcListPtr );
			if( cryptStatusError( status ) )
				return( status );
			insertDoubleListElement( destHeadPtr, insertPoint, 
									 newAttribute, ATTRIBUTE_LIST );
			}
		ENSURES( LOOP_BOUND_OK );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Copy Specific Attributes						*
*																			*
****************************************************************************/

/* Copy CA constraints from the issuer to the subject */

#ifdef USE_CERTLEVEL_PKIX_FULL

static int copyCAConstraints( INOUT DATAPTR_ATTRIBUTE *destHeadPtr,
							  IN const DATAPTR_ATTRIBUTE srcPtr,
							  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType )
	{
	DATAPTR_ATTRIBUTE destHead = *destHeadPtr, attributePtr;
	DATAPTR_ATTRIBUTE srcPermittedSubtrees, srcExcludedSubtrees;
	int status;

	assert( isWritePtr( destHeadPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISSET( srcPtr ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Check for the presence of permitted/excluded subtrees */
	srcPermittedSubtrees = findAttributeField( srcPtr,
											   CRYPT_CERTINFO_PERMITTEDSUBTREES,
											   CRYPT_ATTRIBUTE_NONE );
	srcExcludedSubtrees = findAttributeField( srcPtr,
											  CRYPT_CERTINFO_EXCLUDEDSUBTREES,
											  CRYPT_ATTRIBUTE_NONE );

	/* If we're copying permitted or excluded subtrees then they can't 
	   already be present in the destination.  We check the two separately 
	   rather than just checking for the overall presence of name 
	   constraints since in theory it's possible to merge permitted and 
	   excluded constraints, so that permitted constraints in the 
	   destination don't clash with excluded constraints in the source (yet 
	   another one of X.509's semantic holes) */
	if( DATAPTR_ISSET( srcPermittedSubtrees ) )
		{
		attributePtr = findAttributeField( destHead, 
										   CRYPT_CERTINFO_PERMITTEDSUBTREES,
										   CRYPT_ATTRIBUTE_NONE );
		if( DATAPTR_ISSET( attributePtr ) )
			{
			*errorLocus = CRYPT_CERTINFO_PERMITTEDSUBTREES;
			*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
			return( CRYPT_ERROR_DUPLICATE );
			}
		}
	if( DATAPTR_ISSET( srcExcludedSubtrees ) )
		{
		attributePtr = findAttributeField( destHead, 
										   CRYPT_CERTINFO_EXCLUDEDSUBTREES,
										   CRYPT_ATTRIBUTE_NONE );
		if( DATAPTR_ISSET( attributePtr ) )
			{
			*errorLocus = CRYPT_CERTINFO_EXCLUDEDSUBTREES;
			*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
			return( CRYPT_ERROR_DUPLICATE );
			}
		}

	/* Copy the fields across */
	if( DATAPTR_ISSET( srcPermittedSubtrees ) )
		{
		status = copyAttribute( destHeadPtr, 
								srcPermittedSubtrees, 
								COPY_SUBJECT_TO_ISSUER );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( DATAPTR_ISSET( srcExcludedSubtrees ) )
		{
		status = copyAttribute( destHeadPtr, 
								srcExcludedSubtrees, 
								COPY_SUBJECT_TO_ISSUER );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* The path-length constraints are a bit easier to handle, if they're 
	   already present then we just use the smaller of the two */
	status = copyLengthConstraint( destHeadPtr, srcPtr, 
								   CRYPT_CERTINFO_REQUIREEXPLICITPOLICY,
								   errorLocus );
	if( cryptStatusOK( status ) )
		{
		status = copyLengthConstraint( destHeadPtr, srcPtr, 
									   CRYPT_CERTINFO_INHIBITPOLICYMAPPING,
									   errorLocus );
		}
	if( cryptStatusError( status ) )
		{
		*errorType = CRYPT_ERRTYPE_ISSUERCONSTRAINT;
		return( status );
		}

	return( CRYPT_OK );
	}
#endif /* USE_CERTLEVEL_PKIX_FULL */

/* Copy attributes that are propagated down certificate chains from an 
   issuer to a subject certificate, changing the field types from subject 
   to issuer and adjusting constraint values at the same time if required */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5 ) ) \
int copyIssuerAttributes( INOUT DATAPTR_ATTRIBUTE *destHeadPtr,
						  IN const DATAPTR_ATTRIBUTE srcPtr,
						  const CRYPT_CERTTYPE_TYPE type,
						  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
						  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType )
	{
	DATAPTR_ATTRIBUTE destHead = *destHeadPtr, attributePtr;
	const ATTRIBUTE_LIST *attributeListPtr;
	int status = CRYPT_OK;

	assert( isWritePtr( destHeadPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISSET( srcPtr ) );
	REQUIRES( isEnumRange( type, CRYPT_CERTTYPE ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* If the destination is a CA certificate and the source has constraint 
	   extensions, copy them over to the destination.  The reason why we
	   copy the constraints even though they're already present in the
	   source is to ensure that they're still present in a certificate chain 
	   even if the parent isn't available.  This can occur for example when 
	   a chain-internal certificate is marked as implicitly trusted and the 
	   chain is only available up to the implicitly-trusted certificate with 
	   the contraint-imposing parent not present */
	attributePtr = findAttributeField( destHead, CRYPT_CERTINFO_CA, 
									   CRYPT_ATTRIBUTE_NONE );
	if( DATAPTR_ISSET( attributePtr ) )
		{
		attributeListPtr = DATAPTR_GET( attributePtr );
		ENSURES( attributeListPtr != NULL );
		REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );
		if( attributeListPtr->intValue > 0 )
			{
			/* Copy any extended CA constraints across */
#ifdef USE_CERTLEVEL_PKIX_FULL
			status = copyCAConstraints( destHeadPtr, srcPtr, 
										errorLocus, errorType );
			if( cryptStatusError( status ) )
				return( status );
#endif /* USE_CERTLEVEL_PKIX_FULL */

			/* Finally, copy the CA basic constraints across */
			status = copyLengthConstraint( destHeadPtr, srcPtr, 
										   CRYPT_CERTINFO_PATHLENCONSTRAINT,
										   errorLocus );
			if( cryptStatusError( status ) )
				{
				*errorType = CRYPT_ERRTYPE_ISSUERCONSTRAINT;
				return( status );
				}
			}
		}

	/* If it's an attribute certificate, that's all that we can copy */
	if( type == CRYPT_CERTTYPE_ATTRIBUTE_CERT )
		return( CRYPT_OK );

	/* Copy the altName and keyIdentifier if these are present.  We don't
	   have to check for their presence in the destination certificate since 
	   they're read-only fields and can't be added by the user.  The
	   CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER becomes the 
	   CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER when copied from the issuer to 
	   the subject so we only copy it at a compliance level of 
	   CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL or above, since it's not enforced 
	   below that level */
	attributePtr = findAttribute( srcPtr, CRYPT_CERTINFO_SUBJECTALTNAME, 
								  TRUE );
	if( DATAPTR_ISSET( attributePtr ) )
		{
		status = copyAttribute( destHeadPtr, attributePtr, 
								COPY_SUBJECT_TO_ISSUER );
		if( cryptStatusError( status ) )
			return( status );
		}
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	attributePtr = findAttribute( srcPtr, CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER, 
								  TRUE );
	if( DATAPTR_ISSET( attributePtr ) )
		{
		status = copyAttribute( destHeadPtr, attributePtr, 
								COPY_SUBJECT_TO_ISSUER );
		if( cryptStatusError( status ) )
			return( status );
		}
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

	/* Copy the authorityInfoAccess if it's present.  This one is a bit 
	   tricky both because it's a multi-valued attribute and some values 
	   may already be present in the destination certificate and because 
	   it's not certain that the issuer certificate's AIA should be the same 
	   as the subject certificate's AIA.  At the moment with monolithic CAs 
	   (i.e. ones that control all the certificates down to the EE) this is 
	   always the case and if it isn't then it's assumed that the CA will 
	   set the EE's AIA to the appropriate value before trying to sign the 
	   certificate.  Because of this we copy the issuer AIA if there's no 
	   subject AIA present, otherwise we assume that the CA has set the 
	   subject AIA to its own choice of value and don't try and copy 
	   anything */
	attributePtr = findAttribute( srcPtr, CRYPT_CERTINFO_AUTHORITYINFOACCESS, 
								  FALSE );
	if( DATAPTR_ISSET( attributePtr ) )
		{
		DATAPTR_ATTRIBUTE aiaAttribute;

		aiaAttribute = findAttribute( destHead, 
									  CRYPT_CERTINFO_AUTHORITYINFOACCESS, FALSE );
		if( DATAPTR_ISNULL( aiaAttribute ) )
			{
			status = copyAttribute( destHeadPtr, attributePtr, 
									COPY_SUBJECT_TO_ISSUER );
			if( cryptStatusError( status ) )
				return( status );
			}
		}

	return( CRYPT_OK );
	}

#ifdef USE_CERTREQ

/* Copy attributes that are propagated from a CRMF certificate request 
   template to the issued certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyCRMFRequestAttributes( INOUT DATAPTR_ATTRIBUTE *destHeadPtr,
							   IN const DATAPTR_ATTRIBUTE srcPtr )
	{
	DATAPTR_ATTRIBUTE attributePtr;
	int status;

	assert( isWritePtr( destHeadPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );

	REQUIRES( DATAPTR_ISSET( srcPtr ) );

	/* Copy the altName across, needed for the additional identification 
	   that it provides */
	attributePtr = findAttribute( srcPtr, CRYPT_CERTINFO_SUBJECTALTNAME, 
								  TRUE );
	if( DATAPTR_ISSET( attributePtr ) )
		{
		status = copyAttribute( destHeadPtr, attributePtr, COPY_DIRECT );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}
#endif /* USE_CERTREQ */

#if defined( USE_CERTVAL ) || defined( USE_CERTREV )

/* Copy attributes that are propagated from an RTCS or OCSP request to a 
   response.  Since only one attribute, the nonce, is copied across we can
   use the same function for both operations */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyRTCSRequestAttributes( INOUT DATAPTR_ATTRIBUTE *destHeadPtr,
							   IN const DATAPTR_ATTRIBUTE srcPtr )
	{
	DATAPTR_ATTRIBUTE destHead = *destHeadPtr, attributePtr;

	assert( isWritePtr( destHeadPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );

	REQUIRES( DATAPTR_ISSET( srcPtr ) );

	/* If the nonce attribute is already present in the destination, delete
	   it */
	attributePtr = findAttributeField( destHead, CRYPT_CERTINFO_OCSP_NONCE, 
									   CRYPT_ATTRIBUTE_NONE );
	if( DATAPTR_ISSET( attributePtr ) )
		{
		( void ) deleteAttributeField( destHeadPtr, NULL, attributePtr, 
									   NULL );
		}

	/* Copy the nonce attribute from the source to the destination.  We don't
	   copy anything else (i.e. we default to deny-all) to prevent the 
	   requester from being able to insert arbitrary attributes into the 
	   response */
	attributePtr = findAttributeField( srcPtr, CRYPT_CERTINFO_OCSP_NONCE, 
									   CRYPT_ATTRIBUTE_NONE );
	if( DATAPTR_ISSET( attributePtr ) )
		return( copyAttribute( destHeadPtr, attributePtr, COPY_DIRECT ) );

	return( CRYPT_OK );
	}
#endif /* USE_CERTVAL || USE_CERTREV */

#ifdef USE_CERTREV

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyOCSPRequestAttributes( INOUT DATAPTR_ATTRIBUTE *destHeadPtr,
							   IN const DATAPTR_ATTRIBUTE srcPtr )
	{
	assert( isWritePtr( destHeadPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );

	REQUIRES( DATAPTR_ISSET( srcPtr ) );

	return( copyRTCSRequestAttributes( destHeadPtr, srcPtr ) );
	}

/* Copy attributes that are propagated from a revocation request to a CRL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyRevocationAttributes( INOUT DATAPTR_ATTRIBUTE *destHeadPtr,
							  IN const DATAPTR_ATTRIBUTE srcPtr )
	{
	DATAPTR_ATTRIBUTE attributePtr;
	int status;

	assert( isWritePtr( destHeadPtr, sizeof( DATAPTR_ATTRIBUTE ) ) );

	REQUIRES( DATAPTR_ISSET( srcPtr ) );

	/* Copy the CRL reason and invalidity date attributes from the source to 
	   the destination.  We don't copy anything else (i.e. we default to 
	   deny-all) to prevent the requester from being able to insert arbitrary 
	   attributes into the CRL */
	attributePtr = findAttribute( srcPtr, CRYPT_CERTINFO_CRLREASON, 
								  FALSE );
	if( DATAPTR_ISSET( attributePtr ) )
		{
		status = copyAttribute( destHeadPtr, attributePtr, COPY_DIRECT );
		if( cryptStatusError( status ) )
			return( status );
		}
	attributePtr = findAttribute( srcPtr, CRYPT_CERTINFO_INVALIDITYDATE, 
								  FALSE );
	if( DATAPTR_ISSET( attributePtr ) )
		return( copyAttribute( destHeadPtr, attributePtr, COPY_DIRECT ) );

	return( CRYPT_OK );
	}
#endif /* USE_CERTREV */
#endif /* USE_CERTIFICATES */
