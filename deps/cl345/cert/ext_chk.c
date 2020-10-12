/****************************************************************************
*																			*
*					Certificate Attribute Checking Routines					*
*						Copyright Peter Gutmann 1996-2018					*
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

/* Validate and preprocess a set of attributes and set up links to the 
   information in the attribute information table in preparation for 
   encoding the attribute.  This is a rather complex process that relies on 
   stepping through the list of attribute fields and the attribute 
   information table in sync and making sure that the list of fields is 
   consistent with the attribute information table.  In addition we set up 
   synchronisation points between the list and table that are used during 
   the encoding process.  For example assume that we have the following 
   attribute:

	attribute ::= SEQUENCE {
		foo		BOOLEAN DEFAULT TRUE,
		bar		SEQUENCE OF OBJECT IDENTIFIER
		}

   The attribute information table would encode this attribute as:

	t1:	OID	SEQUENCE
	t2:		BOOLEAN		OPTIONAL
	t3:		SEQUENCE
	t4:		OID			END

   The first table entry t1 contains the OID and the SEQUENCE wrapper.  For 
   the purposes of comparison with the list this is a no-op and can be 
   skipped since it's only used for encoding purposes.  The next table entry 
   t2 contains the first attribute field, an optional boolean.  The next 
   table entry t3 contains another SEQUENCE wrapper that again is only used 
   for encoding and can be skipped for comparing with the list.  Finally, 
   the last table entry t4 contains the second attribute field, an OID, and 
   the end-of-attribute flag.

   Assuming that the attribute list contains the following:

	BOOLEAN	FALSE	-> t1
	OID		xxx		-> t3

   The attribute validation process sets the synchronisation point for the 
   first attribute list entry to point to t1 and the second one to point to 
   t3.  When we encode the attribute we encode t1 (the OID, critical flag, 
   and SEQUENCE wrapper) and then since the field IDs won't match we step on 
   to t2 and use that to encode the boolean.  We then do the same for t3 
   with the SEQUENCE and OID.

   If the attribute list instead contained only:

	OID		xxx		-> t1

   then this time the attribute validation process sets the synchronisation 
   point to t1.  When encoding we encode t1 as before, step to t2, the field 
   IDs won't match but t2 is optional so we skip it, then encode t3 as for 
   t1 and finally encode the OID using t4.

   At this point we also evaluate the encoded size of each attribute.  For
   invidual fields we just store their encoded size.  For constructed 
   objects we stack the attribute list entry where the constructed object
   starts and, until we reach the end of the constructed object, accumulate
   the total size of the fields that make up the object.  When we reach the
   end of the object we unstack the pointer to the attribute list and store
   the total size in it.

   To handle nested constructed objects we only update the size of the
   topmost item on the stack.  When this is unstacked we add the size of 
   that entry plus the size of its tag and length information to the next
   entry on the stack.

   In addition to updating the size we also record the sequence of table
   entries that are required to encode the constructed item.  A worst-case
   sequence of entries would be:

	SEQUENCE {
		SEQUENCE OPTIONAL { ... }		| Not encoded
		SEQUENCE {
			SEQUENCE OPTIONAL { ... }	| Not encoded
			SEQUENCE {
				value
				}
			}
		}

   which contains an alternating sequence of encoded and non-encoded fields.
   Because of this the validation check performs the complex task of
   recording which table entries are used for the encoding by stacking and
   unstacking them and discarding the ones that evaluate to a zero size
   during the unstacking process */

/* Define the following to print a trace of the certificate fields being 
   checked */

#if !defined( NDEBUG ) && 0
  #define TRACE_FIELDTYPE( attributeInfoPtr ) \
		  { \
		  if( ( attributeInfoPtr ) != NULL && \
			  ( attributeInfoPtr )->description != NULL ) \
			  DEBUG_PUTS(( ( attributeInfoPtr )->description )); \
		  else \
			  DEBUG_PUTS(( "<Unrecognised attribute>" )); \
		  }
  #define TRACE_DEBUG( message ) \
		  DEBUG_PRINT( message )
#else
  #define TRACE_FIELDTYPE( attributeInfoPtr )
  #define TRACE_DEBUG( message )
#endif /* NDEBUG */

/* State machine for checking a CHOICE.  When we get to the start of a
   CHOICE we move from CHOICE_NONE to CHOICE_START.  Once we've checked one
   of the CHOICE options we move to CHOICE_DONE.  If a further option is
   found in the CHOICE_DONE state we record an error.  This is a somewhat
   crude mechanism that works because the only CHOICE fields that can't be
   handled by rewriting them as alternative representations are complete
   attributes so that the CHOICE applies over the entire attribute.  If a
   CHOICE is ever present as an attribute subfield then the checking would 
   be handled by recursively checking it as a subtyped field */

typedef enum { CHOICE_NONE, CHOICE_START, CHOICE_DONE, 
			   CHOICE_LAST } CHOICE_STATE;

/* The attribute parsing state stack.  Each entry in the stack contains the 
   list item that it applies to, the table entry which is used to encode the 
   stacked item, and the size of the item.  We don't bother making these
   safe pointers because they've live only as long as standard stack-based
   pointers */

#define ATTRIBUTE_STACKSIZE		10

typedef struct {
	ATTRIBUTE_LIST *attributeListPtr;	/* List entry that this applies to */
	const ATTRIBUTE_INFO *attributeInfoPtr;	/* Encoding point for sequence */
	int size;							/* Size of sequence */
	} ATTRIBUTE_STACK;

/* When we're performing a check of an attribute we need to pass a lot of 
   state information around, this is contained in the following 
   structure.  We don't bother making these safe pointers because they've 
   live only as long as standard stack-based pointers */

typedef struct {
	/* State information.  When we're encoding a subtyped field (using an
	   alternative encoding table) we need to remember the field ID of the
	   parent to both tell the encoding routines that we're using an
	   alternative encoding table and to remember the overall field ID so we
	   don't treat two adjacent field subfields as though they were part of
	   the same parent field.  If we're not currently encoding a subtyped
	   field, this field is set to CRYPT_ATTRIBUTE_NONE */
	ATTRIBUTE_LIST *attributeListPtr;	/* Position in attribute list */
	const ATTRIBUTE_INFO *attributeInfoPtr;	/* Position in attribute table */
	CRYPT_ATTRIBUTE_TYPE subtypeParent;	/* Parent of subtype being processed */
	CHOICE_STATE choiceState;			/* State of CHOICE processing */

	/* Encoding stack.  When we're encoding subfields the stack contains 
	   items from both the subfield and the encapsulating field so we also
	   record a stack marker position that records how far we have to go 
	   back when popping items after we've finished encoding a subfield */
	ARRAY( ATTRIBUTE_STACKSIZE, stackPos ) \
	ATTRIBUTE_STACK stack[ ATTRIBUTE_STACKSIZE + 8 ];
	int stackPos;					/* Encoding stack position */
	int stackMarkerPos;

	/* Error information */
	CRYPT_ATTRIBUTE_TYPE errorLocus;/* Error locus */
	int errorType;					/* Error type */
	} ATTRIBUTE_CHECK_INFO;

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the attribute check info */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckAttributeCheckInfo( const ATTRIBUTE_CHECK_INFO *attributeCheckInfo )
	{
	assert( isReadPtr( attributeCheckInfo, sizeof( ATTRIBUTE_CHECK_INFO ) ) );

	/* Make sure that the state information is valid */
	if( attributeCheckInfo->attributeInfoPtr == NULL || \
		( attributeCheckInfo->subtypeParent != CRYPT_ATTRIBUTE_NONE && \
		  !isValidExtension( attributeCheckInfo->subtypeParent ) ) || \
		( attributeCheckInfo->choiceState < CHOICE_NONE || \
		  attributeCheckInfo->choiceState >= CHOICE_LAST ) )
		{
		DEBUG_PUTS(( "sanityCheckAttributeCheckInfo: State information" ));
		return( FALSE );
		}

	/* Make sure that the stack is valid */
	if( attributeCheckInfo->stackPos < 0 || \
		attributeCheckInfo->stackPos >= ATTRIBUTE_STACKSIZE || \
		attributeCheckInfo->stackMarkerPos < 0 || \
		attributeCheckInfo->stackMarkerPos > attributeCheckInfo->stackPos )
		{
		DEBUG_PUTS(( "sanityCheckAttributeCheckInfo: Stack information" ));
		return( FALSE );
		}

	/* Make sure that the error information is valid */
	if( !isEnumRangeOpt( attributeCheckInfo->errorLocus, CRYPT_ATTRIBUTE ) )
		{
		DEBUG_PUTS(( "sanityCheckAttributeCheckInfo: Error information" ));
		return( FALSE );
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*							Attribute Stack Management						*
*																			*
****************************************************************************/

/* Stack an item */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int stackInfo( INOUT ATTRIBUTE_CHECK_INFO *attributeCheckInfo,
					  IN_OPT ATTRIBUTE_LIST *attributeListPtr,
					  const ATTRIBUTE_INFO *attributeInfoPtr )
	{
	ATTRIBUTE_STACK *stack = attributeCheckInfo->stack;

	assert( isWritePtr( attributeCheckInfo, \
						sizeof( ATTRIBUTE_CHECK_INFO ) ) );
	assert( attributeListPtr == NULL || \
			isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );

	stack[ attributeCheckInfo->stackPos ].size = 0;
	stack[ attributeCheckInfo->stackPos ].attributeListPtr = attributeListPtr;
	stack[ attributeCheckInfo->stackPos++ ].attributeInfoPtr = attributeInfoPtr;

	ENSURES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );

	return( CRYPT_OK );
	}

/* Once we reach the end of the constructed item we need to unwind the stack
   and update everything that we've gone past.  If it's an optional item (so 
   that nothing gets encoded) we don't do anything.  The count argument 
   specifies the level of unwinding to perform, this can be relative 
   (isRelative = TRUE, in which case we undo 'count' levels of nesting, which 
   may be more than 'count' stack positions if non-nested data was stacked) 
   or absolute (isRelative = FALSE, in which case we undo 'count' stack 
   positions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int updateStackedInfo( INOUT_ARRAY_C( ATTRIBUTE_STACKSIZE ) \
								ATTRIBUTE_STACK *stack, 
							  IN_RANGE( 0, ATTRIBUTE_STACKSIZE - 1 ) \
								const int stackPos,
							  OUT_RANGE( 0, ATTRIBUTE_STACKSIZE - 1 ) \
								int *newStackPosPtr,
							  IN_RANGE( 0, ATTRIBUTE_STACKSIZE - 1 ) int count, 
							  const BOOLEAN isRelative )
	{
	int currentStackPos = stackPos, LOOP_ITERATOR;

	assert( isWritePtr( stack, sizeof( ATTRIBUTE_STACK ) * \
							   ATTRIBUTE_STACKSIZE ) );
	assert( isWritePtr( newStackPosPtr, sizeof( int ) ) );

	static_assert( ENCODING_FIFO_SIZE >= ATTRIBUTE_STACKSIZE, \
				   "Stack size" );

	REQUIRES( stackPos >= 0 && stackPos < ATTRIBUTE_STACKSIZE );
	REQUIRES( count >= 0 && count <= stackPos );
	REQUIRES( isRelative == TRUE || isRelative == FALSE );

	LOOP_EXT_CHECKINC( count > 0, count--, ATTRIBUTE_STACKSIZE + 1 )
		{
		ATTRIBUTE_LIST *attributeFifoPtr;
		const ATTRIBUTE_INFO *attributeInfoPtr;
		int size;

		ENSURES( count > 0 && count <= currentStackPos );

		/* Unstack the current entry */
		currentStackPos--;
		ENSURES( currentStackPos >= 0 && \
				 currentStackPos < ATTRIBUTE_STACKSIZE );
		attributeFifoPtr = stack[ currentStackPos ].attributeListPtr;
		attributeInfoPtr = stack[ currentStackPos ].attributeInfoPtr;
		size = stack[ currentStackPos ].size;
		REQUIRES( attributeFifoPtr == NULL || \
				  sanityCheckAttributePtr( attributeFifoPtr ) );
		ENSURES( isIntegerRange( size ) );

		assert( attributeFifoPtr == NULL || \
				isReadPtr( attributeFifoPtr, sizeof( ATTRIBUTE_LIST ) ) );
		assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

		/* If there's nothing to encode, continue.  There are a few special 
		   cases here where even if the sequence is of zero length we may 
		   have to encode something.  Firstly, if there's a member with a 
		   default value present (resulting in nothing being encoded) we 
		   still have to encode a zero-length sequence.  In addition if all 
		   of the members have non-encoding values (e.g. OIDs and fixed 
		   attributes, none of which are specified by the user) then we have 
		   to encode these even though there's no actual value associated 
		   with them since their mere presence conveys the necessary 
		   information.

		   In addition sometimes we can reach the end of the attribute list 
		   but there are further actions defined in the encoding table (for 
		   example cleanup actions in nested sequences).  In this case the 
		   stacked attributeFifoPtr is NULL and the size is zero so we 
		   perform an additional check to make sure that the pointer is 
		   non-null */
		if( attributeFifoPtr == NULL )
			{
			ENSURES( size == 0 );
			continue;
			}
		if( size <= 0 && \
			!( TEST_FLAG( attributeFifoPtr->flags, 
						  ATTR_FLAG_DEFAULTVALUE ) || \
			   ( attributeInfoPtr->encodingFlags & FL_NONENCODING ) ) )
			continue;

		/* Remember the size and table entry used to encode this stack 
		   entry */
		attributeFifoPtr->sizeFifo[ attributeFifoPtr->fifoEnd ] = size;
		attributeFifoPtr->encodingFifo[ attributeFifoPtr->fifoEnd++ ] = \
								stack[ currentStackPos ].attributeInfoPtr;

		ENSURES( attributeFifoPtr->fifoEnd > 0 && \
				 attributeFifoPtr->fifoEnd < ENCODING_FIFO_SIZE );

		/* If there are no further items on the stack, continue */
		if( currentStackPos <= 0 )
			continue;

		ENSURES( currentStackPos > 0 && \
				 currentStackPos < ATTRIBUTE_STACKSIZE );

		/* If it's a non-constructed field, add the length of the existing and
		   new fields */
		if( attributeInfoPtr->fieldType != BER_SEQUENCE && \
			attributeInfoPtr->fieldType != BER_SET )
			{
			int newLength;

			/* Calculate the size of the encoded field data.  A sequence of
			   identifier fields has a final catch-all entry without an OID 
			   which is only used for decoding, we should never see this 
			   when encoding but we make the check explicit just in case */
			if( attributeInfoPtr->fieldType == FIELDTYPE_IDENTIFIER )
				{
				ENSURES( attributeInfoPtr->oid != NULL );
				newLength = sizeofOID( attributeInfoPtr->oid );
				}
			else
				newLength = attributeInfoPtr->defaultValue;

			/* Add the new length to the existing data size.  Since this is a
			   non-constructed field it doesn't count as a reduction in the
			   nesting level so if we're unnesting by a relative amount we 
			   adjust the nesting count to give a net change of zero for this
			   item */
			stack[ currentStackPos - 1 ].size += size + newLength;
			if( isRelative )
				count++;
			}
		else
			{
			/* It's a constructed field, percolate the encapsulated content
			   size up the stack */
			stack[ currentStackPos - 1 ].size += sizeofShortObject( size );
			}
		}
	ENSURES( LOOP_BOUND_OK );

	*newStackPosPtr = currentStackPos;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Attribute Field Type Checking					*
*																			*
****************************************************************************/

/* Some attributes contain a sequence of items of the attributeTypeAndValue 
   form (i.e. OID, ANY DEFINED BY OID).  To process these we check whether 
   the named value component in the attribute list is present in the current 
   attributeTypeAndValue definition.  If it isn't then the item is given a 
   zero length, which means that it's never encoded since the field is 
   marked as optional.  The following function checks whether a named value 
   component is present in the item */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int checkComponentPresent( IN_RANGE( CRYPT_ATTRIBUTE_NONE + 1,
											CRYPT_IATTRIBUTE_LAST ) \
										const CRYPT_ATTRIBUTE_TYPE fieldID,
								  INOUT ATTRIBUTE_INFO **attributeInfoPtrPtr )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr = *attributeInfoPtrPtr;
	int nestLevel = 0, noEntries, LOOP_ITERATOR;

	assert( isWritePtr( attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO * ) ) );
	assert( isReadPtr( *attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( isEnumRange( fieldID, CRYPT_ATTRIBUTE ) || \
			  ( fieldID == CRYPT_IATTRIBUTE_LAST ) );
			  /* The latter is used when we've run out of values to match 
			     and just want to skip to the end of the 
				 attributeTypeAndValue */

	/* Check each field that we find until we find the end of the
	   attributeTypeAndValue.  This is a bit of an awkward loop because
	   there's no direct loop structure, the loop control checks are
	   performed in the body of the loop.  Because of this we iterate on a
	   dummy variable noEntries in order to bound the loop.  In addition we 
	   don't know how many attribute table entries are left so we have to 
	   use the generic FAILSAFE_ITERATIONS_LARGE for the bounds check */
	LOOP_LARGE( noEntries = 0, noEntries < 500, noEntries++ )
		{
		/* Sanity check to make sure that we don't fall off the end of the 
		   table */
		ENSURES( !isAttributeTableEnd( attributeInfoPtr ) );

		/* Adjust the nesting level depending on whether we're entering or
		   leaving a sequence */
		if( attributeInfoPtr->fieldType == BER_SEQUENCE )
			nestLevel++;
		nestLevel -= decodeNestingLevel( attributeInfoPtr->encodingFlags );

		/* If the field is present in this attributeTypeAndValue, return */
		if( attributeInfoPtr->fieldID == fieldID )
			return( CRYPT_OK );

		/* If we're at the end of the attribute or the attributeTypeAndValue,
		   exit the loop before adjusting the attributeInfoPtr so that we're
		   still pointing at the end-of-attribute field */
		if( nestLevel <= 0 || \
			( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) )
			break;

		attributeInfoPtr++;
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( noEntries < 500 );

	/* The field isn't present, update the pointer to the next
	   attributeTypeAndValue or the end of the attribute */
	*attributeInfoPtrPtr = ( ATTRIBUTE_INFO * ) attributeInfoPtr;

	return( CRYPT_ERROR_NOTFOUND );
	}

/****************************************************************************
*																			*
*								Attribute Checking							*
*																			*
****************************************************************************/

/* Check an entry in the attribute table */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkAttribute( INOUT ATTRIBUTE_CHECK_INFO *attributeCheckInfo );
						   /* Forward declaration for function */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkAttributeEntry( INOUT ATTRIBUTE_CHECK_INFO *attributeCheckInfo,
								OUT BOOLEAN *skipStackUpdate )
	{
	ATTRIBUTE_LIST *attributeListPtr = attributeCheckInfo->attributeListPtr;
	const ATTRIBUTE_INFO *attributeInfoPtr = attributeCheckInfo->attributeInfoPtr;
	ATTRIBUTE_STACK *stack = attributeCheckInfo->stack;
	CRYPT_ATTRIBUTE_TYPE fieldID;

	assert( isWritePtr( attributeCheckInfo, \
						sizeof( ATTRIBUTE_CHECK_INFO ) ) );
	assert( attributeListPtr == NULL || \
			isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( stack, \
						sizeof( ATTRIBUTE_STACK ) * ATTRIBUTE_STACKSIZE ) );

	REQUIRES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );
	REQUIRES( attributeListPtr == NULL || \
			  sanityCheckAttributePtr( attributeListPtr ) );
			  
	/* Clear return values */
	*skipStackUpdate = FALSE; 

	/* Determine the fieldID for the current attribute field.  Once we get 
	   to the end of an attribute list entry we may still run through this 
	   function multiple times with attributeListPtr set to NULL but 
	   attributeInfoPtr advancing on each step, see the comment in 
	   updateStackedInfo() for details */
	if( attributeListPtr == NULL || \
		attributeListPtr->fieldID == CRYPT_ATTRIBUTE_NONE )
		{
		/* If we've reached the end of the list of recognised attributes, 
		   use a non-ID that doesn't match any table entry */
		fieldID = CRYPT_IATTRIBUTE_LAST;
		}
	else
		{
		/* If we're encoding a subtyped field then the fieldID is the field 
		   ID within the parent field, or the subFieldID.  However, for a 
		   CHOICE type the value is a selection with a subtype table, for
		   example CRYPT_CONTENT_DATA within the contentTypeInfo, rather 
		   than an attribute, so we have to use the attribute value as a
		   (pseudo-)fieldID to select the encoding table entry */
		if( attributeCheckInfo->subtypeParent == attributeListPtr->fieldID )
			{
			if( attributeInfoPtr->fieldType == FIELDTYPE_IDENTIFIER )
				{
				fieldID = attributeListPtr->intValue;
				}
			else
				fieldID = attributeListPtr->subFieldID;
			}
		else
			{
			/* It's a standard attribute field */
			fieldID = attributeListPtr->fieldID;
			}
		}

	/* If the field in the attribute list matches the one in the table,
	   process it and move on to the next one */
	if( attributeListPtr != NULL && attributeInfoPtr->fieldID == fieldID )
		{
		int size, status;

		/* If it's a subtyped or CHOICE field, check the components using
		   their own encoding table */
		if( attributeInfoPtr->fieldType == FIELDTYPE_SUBTYPED || \
			attributeInfoPtr->fieldType == FIELDTYPE_CHOICE )
			{
			TRACE_DEBUG(( "Switching to sub-field " ));
			TRACE_FIELDTYPE( attributeCheckInfo->attributeInfoPtr );

			/* Switch to the new encoding table, record the fact that
			   we've done this, and set the new stack top to the level at
			   which we start encoding the subtype */
			if( attributeInfoPtr->fieldType == FIELDTYPE_CHOICE )
				{
				/* Stack the value start position in the attribute list and
				   record the fact that we're processing a CHOICE */
				status = stackInfo( attributeCheckInfo, attributeListPtr,
									attributeInfoPtr );
				if( cryptStatusError( status ) )
					return( status );
				attributeCheckInfo->choiceState = CHOICE_START;
				}
			attributeCheckInfo->attributeInfoPtr = \
					( const ATTRIBUTE_INFO * ) attributeInfoPtr->extraData;
			attributeCheckInfo->subtypeParent = attributeListPtr->fieldID;
			attributeCheckInfo->stackMarkerPos = attributeCheckInfo->stackPos;
			status = checkAttribute( attributeCheckInfo );
			attributeCheckInfo->attributeInfoPtr = attributeInfoPtr;
			attributeCheckInfo->subtypeParent = CRYPT_ATTRIBUTE_NONE;
			attributeCheckInfo->stackMarkerPos = 0;
			if( !( attributeInfoPtr->encodingFlags & FL_OPTIONAL ) && \
				attributeCheckInfo->attributeListPtr == attributeListPtr )
				{
				/* The subtyped field was non-optional but we failed to 
				   match anything in it against the current attribute list 
				   entry (meaning that the atributeListPtr in the check info
				   hasn't been advanced), there's a problem with the 
				   encoding table.  This check is used to catch situations 
				   where a subtyped field is used to encode a CHOICE for 
				   which each CHOICE field is optional but at least one 
				   component of the CHOICE must be present */
				retIntError();
				}

			ENSURES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );

			return( status );
			}

		/* If there's an extended validation function attached to this field,
		   call it */
		if( attributeInfoPtr->extraData != NULL )
			{
			VALIDATION_FUNCTION validationFunction = \
					( VALIDATION_FUNCTION ) attributeInfoPtr->extraData;

			attributeCheckInfo->errorType = \
									validationFunction( attributeListPtr );
			if( attributeCheckInfo->errorType != CRYPT_ERRTYPE_NONE )
				return( CRYPT_ERROR_INVALID );
			}

		/* If this is an optional field and the value is the same as the
		   default value remember that it doesn't get encoded */
		if( ( attributeInfoPtr->encodingFlags & FL_DEFAULT ) && \
			( attributeInfoPtr->defaultValue == attributeListPtr->intValue ) )
			{
			SET_FLAG( attributeListPtr->flags, ATTR_FLAG_DEFAULTVALUE );
			REQUIRES( DATAPTR_ISVALID( attributeListPtr->next ) );
			attributeCheckInfo->attributeListPtr = \
							DATAPTR_GET( attributeListPtr->next );

			return( CRYPT_OK );
			}

		TRACE_DEBUG(( "Checking field " ));
		TRACE_FIELDTYPE( attributeCheckInfo->attributeInfoPtr );

		/* Remember the encoded size of this field */
		attributeListPtr->attributeInfoPtr = attributeInfoPtr;
		status = size = sizeofAttributeField( attributeListPtr );
		if( cryptStatusError( status ) )
			return( status );
		attributeListPtr->encodedSize = size;
		if( attributeCheckInfo->stackPos > 0 )
			{
			stack[ attributeCheckInfo->stackPos - 1 ].size += \
										attributeListPtr->encodedSize;
			}

		/* If this is a CHOICE field update the choice state */
		if( attributeCheckInfo->choiceState != CHOICE_NONE )
			{
			if( attributeCheckInfo->choiceState == CHOICE_DONE )
				{
				/* If we've already processed one of the CHOICE options then 
				   there can't be another one present */
				attributeCheckInfo->errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
				return( CRYPT_ERROR_INVALID );
				}
			if( attributeCheckInfo->choiceState == CHOICE_START )
				{
				/* Remember that we've seen a CHOICE option */
				attributeCheckInfo->choiceState = CHOICE_DONE;
				}
			}

		/* Move on to the next attribute field */
		REQUIRES( DATAPTR_ISVALID( attributeListPtr->next ) );
		attributeCheckInfo->attributeListPtr = \
						DATAPTR_GET( attributeListPtr->next );

		ENSURES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );

		return( CRYPT_OK );
		}

	/* If it's an attributeTypeAndValue sequence, check whether it contains
	   the field that we want:

			t0:	BER_SEQUENCE	IDENTIFIER
			t1:	OID
		  [ t2:	params			NONENCODING ]

	   The initialisation sanity-check has already confirmed the validity of 
	   the above format in the encoding table on startup */
	if( attributeInfoPtr->encodingFlags & FL_IDENTIFIER )
		{
		BOOLEAN endOfAttributeField = FALSE;
		int status;

		status = checkComponentPresent( fieldID, 
										( ATTRIBUTE_INFO ** ) &attributeInfoPtr );
		if( status == CRYPT_ERROR_NOTFOUND )
			{
			/* Since we've jumped over several items we may be pointing at an
			   end-of-sequence flag for which no sequence start was stacked 
			   so we skip the stack update step */
			attributeCheckInfo->attributeInfoPtr = attributeInfoPtr;
			*skipStackUpdate = TRUE;
			return( CRYPT_OK );
			}
		if( cryptStatusError( status ) )
			return( status );

		/* Stack the position of the sequence start and the following OID */
		status = stackInfo( attributeCheckInfo, attributeListPtr, 
							attributeInfoPtr++ );
		if( cryptStatusOK( status ) )
			{
			status = stackInfo( attributeCheckInfo, attributeListPtr, 
								attributeInfoPtr );
			}
		if( cryptStatusError( status ) )
			return( status );

		/* If the OID entry is marked as the end-of-sequence then there are 
		   no parameters attached so we move on to the next entry */
		if( attributeInfoPtr->encodingFlags & FL_SEQEND_MASK )
			endOfAttributeField = TRUE;

		/* Sometimes the OID is followed by a fixed-value blob field that
		   constitutes parameters for the OID, if this is present then we 
		   stack it as well */
		if( attributeInfoPtr[ 1 ].encodingFlags & FL_NONENCODING )
			{
			attributeInfoPtr++;
			status = stackInfo( attributeCheckInfo, attributeListPtr, 
								attributeInfoPtr );
			if( cryptStatusError( status ) )
				return( status );

			/* If the fields are fixed-value we always move on to the next
			   entry since there are no user-supplied parameters present */
			endOfAttributeField = TRUE;
			}

		attributeCheckInfo->attributeInfoPtr = attributeInfoPtr;

		/* If this is all that needs to be encoded move on to the next 
		   attribute field */
		if( endOfAttributeField )
			{
			if( attributeListPtr == NULL )
				attributeCheckInfo->attributeListPtr = NULL;
			else
				{
				REQUIRES( DATAPTR_ISVALID( attributeListPtr->next ) );
				attributeCheckInfo->attributeListPtr = \
								DATAPTR_GET( attributeListPtr->next );
				}
			}

		ENSURES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );

		return( CRYPT_OK );
		}

	/* If it's a SEQUENCE/SET or a non-encoding value then it's a no-op entry
	   used only for encoding purposes and can be skipped, however we need to
	   remember it for later encoding */
	if( attributeInfoPtr->fieldType == BER_SEQUENCE || \
		attributeInfoPtr->fieldType == BER_SET || \
		attributeInfoPtr->encodingFlags & FL_NONENCODING )
		{
		int status;

		/* Stack the sequence or value start position in the attribute list */
		status = stackInfo( attributeCheckInfo, attributeListPtr, 
							attributeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );

		ENSURES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );

		return( CRYPT_OK );
		}

	/* If it's a non-optional field and the attribute field doesn't match,
	   it's an error - attribute attributeID is missing field
	   attributeInfoPtr->fieldID (optional subfield
	   attributeInfoPtr->subFieldID) (set by the error handler in the calling
	   code) */
	if( !( attributeInfoPtr->encodingFlags & FL_OPTIONAL ) )
		{
		attributeCheckInfo->errorType = CRYPT_ERRTYPE_ATTR_ABSENT;
		return( CRYPT_ERROR_NOTINITED );
		}

	ENSURES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );

	return( CRYPT_OK );
	}

/* Check an individual attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkAttribute( INOUT ATTRIBUTE_CHECK_INFO *attributeCheckInfo )
	{
	const ATTRIBUTE_LIST *restartEntry = NULL;
	const ATTRIBUTE_INFO *restartPoint = NULL;
	int restartStackPos = 0, noEntries, status, LOOP_ITERATOR;

	assert( isWritePtr( attributeCheckInfo, \
						sizeof( ATTRIBUTE_CHECK_INFO ) ) );
	assert( isReadPtr( attributeCheckInfo->attributeInfoPtr,
					   sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );

	TRACE_DEBUG(( "Checking attribute " ));
	TRACE_FIELDTYPE( attributeCheckInfo->attributeInfoPtr );

	/* Step through the attribute comparing the fields that are present in
	   the attribute list with the fields that should be present according
	   to the table and set encoding synchronisation points as required.
	   
	   This loop has the same issue as the one discussed in the comment in 
	   checkComponentPresent() */
	LOOP_LARGE( noEntries = 0, noEntries < 500, noEntries++ )
		{
		BOOLEAN skipStackUpdate;
		int typeInfoFlags;

		/* Sanity check to make sure that we don't fall off the end of the 
		   encoding table */
		ENSURES( !isAttributeTableEnd( attributeCheckInfo->attributeInfoPtr ) );

		/* Check whether this is a repeated instance of the same attribute
		   and if it is remember the encoding restart point.  We have to do
		   this before we check the attribute information because it usually
		   updates the information after the check */
		if( restartEntry == NULL && \
			attributeCheckInfo->attributeListPtr != NULL )
			{
			const ATTRIBUTE_LIST *attributeListPtr = \
								attributeCheckInfo->attributeListPtr;
			const ATTRIBUTE_LIST *attributeListNextPtr = \
								DATAPTR_GET( attributeListPtr->next );

			if( attributeListNextPtr != NULL && \
				( attributeListPtr->fieldID == \
							attributeListNextPtr->fieldID ) && \
				( attributeListPtr->subFieldID == \
							attributeListNextPtr->subFieldID ) )
				{
				/* Remember the restart point, the next (repeated) attribute 
				   beyond the current one.  In other words if 
				   attributeListPtr->fieldID == CRYPT_CERTINFO_IPADDRESS and
				   attributeListNextPtr->fieldID == CRYPT_CERTINFO_IPADDRESS 
				   then we need to reset the attributeInfoPtr to its current
				   position once we've processed the first entry in order to 
				   process the second entry */
				restartEntry = attributeListNextPtr;
				restartPoint = attributeCheckInfo->attributeInfoPtr;
				restartStackPos = attributeCheckInfo->stackPos + 1;
				}
			}

		/* Check the current encoding table entry.  checkAttributeEntry() 
		   may skip over several optional items and end up at an end-of-
		   SEQUENCE flag for which no SEQUENCE start was stacked, in which 
		   case we don't perform the stack update */
		status = checkAttributeEntry( attributeCheckInfo, &skipStackUpdate );
		if( cryptStatusError( status ) )
			{
			attributeCheckInfo->errorLocus = \
							attributeCheckInfo->attributeInfoPtr->fieldID;
			return( status );
			}
		if( !skipStackUpdate )
			{
			/* If this is the end of a constructed item unstack it and
			   update the attribute list entry with the length information.
			   If it's a sequence with all fields optional (so that nothing
			   gets encoded) we don't do anything */
			status = updateStackedInfo( attributeCheckInfo->stack,
										attributeCheckInfo->stackPos,
										&attributeCheckInfo->stackPos,
				decodeNestingLevel( attributeCheckInfo->attributeInfoPtr->encodingFlags ),
										TRUE );
			if( cryptStatusError( status ) )
				return( status );
			ENSURES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );
			}

		/* If there's another instance of the same item, don't move on to
		   the next table entry.  We need to check whether we're still at 
		   the restartable attribute because checkAttributeEntry() may have, 
		   via a recursive call back to checkAttribute(), processed both the 
		   initial attribute and the repeated instance(s) of the attribute */
		if( restartEntry != NULL && \
			restartEntry == attributeCheckInfo->attributeListPtr )
			{
			/* Restart at the table entry for the previous instance of the 
			   item and adjust the stack to match */
			attributeCheckInfo->attributeInfoPtr = restartPoint;
			if( attributeCheckInfo->stackPos > restartStackPos )
				{
				status = updateStackedInfo( attributeCheckInfo->stack,
											attributeCheckInfo->stackPos,
											&attributeCheckInfo->stackPos,
											attributeCheckInfo->stackPos - \
												restartStackPos, TRUE );
				if( cryptStatusError( status ) )
					return( status );
				ENSURES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );
				}
			restartEntry = NULL;
			restartPoint = NULL;
			restartStackPos = 0;

			continue;
			}

		/* Move on to the next table entry.  We have to check the
		   continuation flag before we move to the next table entry in order
		   to include processing of the last field in an attribute */
		typeInfoFlags = attributeCheckInfo->attributeInfoPtr->typeInfoFlags;
		attributeCheckInfo->attributeInfoPtr++;
		if( typeInfoFlags & FL_ATTR_ATTREND )
			break;
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( noEntries < 500 );
	attributeCheckInfo->choiceState = CHOICE_NONE;
	
	/* We've reached the end of the attribute, if there are still constructed
	   objects stacked, unstack them and update their length information.  If
	   it's a sequence with all fields optional (so that nothing gets
	   encoded) this won't do anything */
	status = updateStackedInfo( attributeCheckInfo->stack, 
								attributeCheckInfo->stackPos,
								&attributeCheckInfo->stackPos,
								attributeCheckInfo->stackPos - \
									attributeCheckInfo->stackMarkerPos, FALSE );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckAttributeCheckInfo( attributeCheckInfo ) );

	return( CRYPT_OK );
	}

/* Check the entire list of attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 4 ) ) \
int checkAttributes( IN_ENUM( ATTRIBUTE ) const ATTRIBUTE_TYPE attributeType,
					 IN const DATAPTR_ATTRIBUTE listHeadPtr,
					 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
					 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType )
	{
	ATTRIBUTE_CHECK_INFO attributeCheckInfo;
	const ATTRIBUTE_INFO *attributeInfoStartPtr;
	ATTRIBUTE_LIST *attributeListPtr;
	int dummy, status, LOOP_ITERATOR;

	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISSET( listHeadPtr ) );
	REQUIRES( attributeType == ATTRIBUTE_CERTIFICATE || \
			  attributeType == ATTRIBUTE_CMS );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Get the attribute encoding information.  We can't use the size value
	   returned from this because of the nested-loop structure below which 
	   only ever iterates through a subset of the encoding information so we
	   leave it as a dummy value */
	status = getAttributeInfo( attributeType, &attributeInfoStartPtr, 
							   &dummy );
	ENSURES( cryptStatusOK( status ) );

	/* If we've already done a validation pass some of the fields will
	   contain values that were previously set so before we begin we walk
	   down the list resetting the fields that are updated by this
	   function */
	LOOP_LARGE( attributeListPtr = DATAPTR_GET( listHeadPtr ), 
				attributeListPtr != NULL && \
					isValidAttributeField( attributeListPtr ),
				attributeListPtr = DATAPTR_GET( attributeListPtr->next ) )
		{
		const ATTRIBUTE_LIST *attributeListNextPtr;

		REQUIRES( sanityCheckAttributePtr( attributeListPtr ) );

		attributeListNextPtr = DATAPTR_GET( attributeListPtr->next );
		if( attributeListNextPtr != NULL && \
			isValidAttributeField( attributeListNextPtr ) && \
			attributeListPtr->attributeID > \
						attributeListNextPtr->attributeID )
			{
			/* Safety check in case of an invalid attribute list */
			retIntError();
			}
		attributeListPtr->attributeInfoPtr = NULL;
		attributeListPtr->encodedSize = attributeListPtr->fifoPos = \
			attributeListPtr->fifoEnd = 0;
		CLEAR_FLAG( attributeListPtr->flags, ATTR_FLAG_DEFAULTVALUE );
		}
	ENSURES( LOOP_BOUND_OK );

	/* Set up the attribute-checking state information */
	memset( &attributeCheckInfo, 0, sizeof( ATTRIBUTE_CHECK_INFO ) );
	attributeCheckInfo.attributeListPtr = DATAPTR_GET( listHeadPtr );
	attributeCheckInfo.attributeInfoPtr = attributeInfoStartPtr;
	ENSURES( sanityCheckAttributeCheckInfo( &attributeCheckInfo ) );

	/* Walk down the list of known attributes checking each one for
	   consistency */
	LOOP_LARGE_CHECK( attributeCheckInfo.attributeListPtr != NULL && \
					  attributeCheckInfo.attributeListPtr->fieldID != CRYPT_ATTRIBUTE_NONE )
		{
		int LOOP_ITERATOR_ALT;

		/* Find the start of this attribute in the attribute information 
		   table and remember it as an encoding synchronisation point.  
		   Comparing the field ID with the attribute ID is usually valid 
		   because the attribute information table always begins the series 
		   of entries for an attribute with the attribute ID.  The one 
		   exception is where the attribute ID is the same as the field ID 
		   but they're separate entries in the table, in which case the 
		   first entries will contain a FIELDID_FOLLOWS code to indicate 
		   that the following field contains the attribute/fieldID */
		LOOP_LARGE_CHECKINC_ALT( !isAttributeTableEnd( attributeCheckInfo.attributeInfoPtr ),
								 attributeCheckInfo.attributeInfoPtr++ )
			{
			if( attributeCheckInfo.attributeInfoPtr->fieldID == FIELDID_FOLLOWS )
				{
				if( attributeCheckInfo.attributeInfoPtr[ 1 ].fieldID == \
						attributeCheckInfo.attributeListPtr->attributeID )
					break;
				}
			else
				{
				if( attributeCheckInfo.attributeInfoPtr->fieldID == \
						attributeCheckInfo.attributeListPtr->attributeID )
					break;
				}
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		ENSURES( !isAttributeTableEnd( attributeCheckInfo.attributeInfoPtr ) );

		/* Check this attribute */
		status = checkAttribute( &attributeCheckInfo );
		if( cryptStatusError( status ) )
			{
			*errorLocus = attributeCheckInfo.errorLocus;
			*errorType = attributeCheckInfo.errorType;
			return( status );
			}
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}
#endif /* USE_CERTIFICATES */
