/****************************************************************************
*																			*
*			cryptlib Session-specific Attribute Support Routines			*
*					  Copyright Peter Gutmann 1998-2008						*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "session.h"
#else
  #include "crypt.h"
  #include "session/session.h"
#endif /* Compiler-specific includes */

#ifdef USE_SESSIONS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Helper function used to access internal attributes within an attribute 
   group */

#if 0	/* Currently unused, may be enabled in a later version with a move 
		   to composite attributes for host/client information */

/* Reset the internal virtual cursor in a attribute-list item after we've 
   moved the attribute cursor */

#define resetVirtualCursor( attributeListPtr ) \
		if( attributeListPtr != NULL ) \
			attributeListPtr->flags |= ATTR_FLAG_CURSORMOVED

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN accessFunction( INOUT ATTRIBUTE_LIST *attributeListPtr,
							   IN_ENUM( ATTR ) const ATTR_TYPE attrGetType )
	{
	static const CRYPT_ATTRIBUTE_TYPE attributeOrderList[] = {
				CRYPT_SESSINFO_NAME, CRYPT_SESSINFO_PASSWORD,
				CRYPT_SESSINFO_KEY, CRYPT_ATTRIBUTE_NONE, 
				CRYPT_ATTRIBUTE_NONE };
	USER_INFO *userInfoPtr = attributeListPtr->value;
	CRYPT_ATTRIBUTE_TYPE attributeID = userInfoPtr->cursorPos;
	BOOLEAN doContinue;
	int iterationCount = 0, LOOP_ITERATOR;

	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_B( isEnumRange( attrGetType, ATTR ) );

	/* If we've just moved the cursor onto this attribute, reset the 
	   position to the first internal attribute */
	if( attributeListPtr->flags & ATTR_FLAG_CURSORMOVED )
		{
		attributeID = userInfoPtr->cursorPos = \
						CRYPT_ENVINFO_SIGNATURE_RESULT;
		attributeListPtr->flags &= ~ATTR_FLAG_CURSORMOVED;
		}

	/* If it's an information fetch, return the currently-selected 
	   attribute */
	if( attrGetType == ATTR_NONE )
		return( attributeID );

	LOOP_LARGE_INITCHECK( doContinue = TRUE, doContinue )
		{
		int i, LOOP_ITERATOR_ALT;

		/* Find the position of the current sub-attribute in the attribute 
		   order list and use that to get its successor/predecessor sub-
		   attribute */
		LOOP_SMALL_ALT( i = 0, 
						attributeOrderList[ i ] != attributeID && \
							attributeOrderList[ i ] != CRYPT_ATTRIBUTE_NONE && \
							i < FAILSAFE_ARRAYSIZE( attributeOrderList, \
													CRYPT_ATTRIBUTE_TYPE ), 
						i++ );
		ENSURES_B( LOOP_BOUND_OK_ALT );
		ENSURES_B( i < FAILSAFE_ARRAYSIZE( attributeOrderList, \
										   CRYPT_ATTRIBUTE_TYPE ) );
		if( attributeOrderList[ i ] == CRYPT_ATTRIBUTE_NONE )
			attributeID = CRYPT_ATTRIBUTE_NONE;
		else
			{
			if( attrGetType == ATTR_PREV )
				{
				attributeID = ( i < 1 ) ? CRYPT_ATTRIBUTE_NONE : \
										  attributeOrderList[ i - 1 ];
				}
			else
				attributeID = attributeOrderList[ i + 1 ];
			}
		if( attributeID == CRYPT_ATTRIBUTE_NONE )
			{
			/* We've reached the first/last sub-attribute within the current 
			   item/group, tell the caller that there are no more sub-
			   attributes present and they have to move on to the next 
			   group */
			return( FALSE );
			}

		/* Check whether the required sub-attribute is present.  If not, we
		   continue and try the next one */
		doContinue = FALSE;
		switch( attributeID )
			{
			case CRYPT_SESSINFO_NAME:
				break;	/* Always present */
				
			case CRYPT_SESSINFO_PASSWORD:
				if( userInfoPtr->passwordLen <= 0 )
					doContinue = TRUE;
				break;
	
			case CRYPT_SESSINFO_KEY:
				if( userInfoPtr->key == CRYPT_ERROR )
					doContinue = TRUE;
				break;

			default:
				retIntError_Boolean();
			}
		}
	ENSURES_B( LOOP_BOUND_OK );
	attributeListPtr->attributeCursorEntry = attributeID;
	
	return( TRUE );
	}
#else
  #define resetVirtualCursor( attributeListPtr )
#endif /* 0 */

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
	ATTRIBUTE_LIST *attributeListPtr = ( ATTRIBUTE_LIST * ) attributePtr;
	BOOLEAN subGroupMove;
	int value, status;

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

	/* Move to the next or previous attribute if required.  This isn't just a
	   case of following the previous/next links because some attribute-list 
	   items contain an entire attribute group so that positioning by 
	   attribute merely changes the current selection within the group 
	   (== attribute-list item) rather than moving to the previous/next 
	   entry.  Because of this we have to special-case the code for 
	   composite items and allow virtual positioning within the item */
	if( attributeListPtr == NULL )
		return( NULL );
	subGroupMove = ( attrGetType == ATTR_PREV || \
					 attrGetType == ATTR_NEXT ) && \
				   TEST_FLAG( attributeListPtr->flags, 
							  ATTR_FLAG_COMPOSITE );
	if( subGroupMove )
		{
		ATTRACCESS_FUNCTION accessFunction;

		REQUIRES_N( attrGetType == ATTR_NEXT || attrGetType == ATTR_PREV );
		REQUIRES_N( TEST_FLAG( attributeListPtr->flags, 
							   ATTR_FLAG_COMPOSITE ) );

		accessFunction = ( ATTRACCESS_FUNCTION ) \
						 FNPTR_GET( attributeListPtr->accessFunction );
		REQUIRES_N( accessFunction != NULL );
		status = accessFunction( attributeListPtr, attrGetType, &value );
		if( cryptStatusError( status ) )
			return( NULL );
		subGroupMove = value;
		}

	/* If we're moving by group, move to the next/previous attribute list
	   item and reset the internal virtual cursor.  Note that we always 
	   advance the cursor to the next/previous attribute, it's up to the 
	   calling code to manage attribute-by-attribute vs. group-by-group 
	   moves */
	if( !subGroupMove && attrGetType != ATTR_CURRENT )
		{
		if( attrGetType == ATTR_PREV )
			{
			REQUIRES_N( DATAPTR_ISVALID( attributeListPtr->prev ) );
			attributeListPtr = DATAPTR_GET( attributeListPtr->prev );
			}
		else
			{
			REQUIRES_N( DATAPTR_ISVALID( attributeListPtr->next ) );
			attributeListPtr = DATAPTR_GET( attributeListPtr->next );
			}
		resetVirtualCursor( attributeListPtr );
		}
	if( attributeListPtr == NULL )
		return( NULL );

	/* Return ID information to the caller.  We only return the group ID if
	   we've moved within the attribute group, if we've moved from one group
	   to another we leave it cleared because sessions can contain multiple
	   groups with the same ID and returning an ID identical to the one from
	   the group that we've moved out of would make it look as if we're still 
	   within the same group.  Note that this relies somewhat on the 
	   implementation behaviour of the attribute-move functions, which first 
	   get the current group using ATTR_CURRENT and then move to the next or 
	   previous using ATTR_NEXT/PREV */
	if( groupID != NULL && ( attrGetType == ATTR_CURRENT || subGroupMove ) )
		*groupID = attributeListPtr->groupID;
	if( attributeID != NULL )
		{
		if( TEST_FLAG( attributeListPtr->flags, ATTR_FLAG_COMPOSITE ) )
			{
			ATTRACCESS_FUNCTION accessFunction;

			accessFunction = ( ATTRACCESS_FUNCTION ) \
							 FNPTR_GET( attributeListPtr->accessFunction );
			REQUIRES_N( accessFunction != NULL );
			status = accessFunction( attributeListPtr, ATTR_NONE, &value );
			if( cryptStatusError( status ) )
				return( NULL );
			*attributeID = value;
			}
		else
			*attributeID = attributeListPtr->attributeID;
		}

	return( attributeListPtr );
	}

/* Lock ephemeral attributes so that they can't be deleted any more by
   resetEphemeralAttributes().  This just clears the ephemeral flag so that
   they're treated as normal attributes */

STDC_NONNULL_ARG( ( 1 ) ) \
void lockEphemeralAttributes( INOUT ATTRIBUTE_LIST *attributeListHead )
	{
	ATTRIBUTE_LIST *attributeListCursor;
	int LOOP_ITERATOR;

	assert( isWritePtr( attributeListHead, sizeof( ATTRIBUTE_LIST * ) ) );

	/* Clear the ATTR_FLAG_EPHEMERAL flag on all attributes */
	LOOP_MAX( attributeListCursor = attributeListHead, 
			  attributeListCursor != NULL,
			  attributeListCursor = DATAPTR_GET( attributeListCursor->next ) )
		{
		CLEAR_FLAG( attributeListCursor->flags, ATTR_FLAG_EPHEMERAL );
		}
	ENSURES_V( LOOP_BOUND_OK );
	}

/* Check that a set of attributes is well-formed.  We can perform most of 
   the checking as the attributes are added but some checks (for example
   whether each username has a corresponding password) aren't possible 
   until all of the attributes are present */

CHECK_RETVAL_ENUM( CRYPT_ATTRIBUTE ) \
CRYPT_ATTRIBUTE_TYPE checkMissingInfo( IN_OPT const ATTRIBUTE_LIST *attributeListHead,
									   const BOOLEAN isServer )
	{
	const ATTRIBUTE_LIST *attributeListPtr = attributeListHead;

	assert( attributeListHead == NULL || \
			isReadPtr( attributeListHead, sizeof( ATTRIBUTE_LIST * ) ) );

	REQUIRES( isServer == TRUE || isServer == FALSE );

	if( attributeListPtr == NULL )
		return( CRYPT_ATTRIBUTE_NONE );

	/* Make sure that every username attribute is paired up with a 
	   corresponding authentication attribute.  This only applies to 
	   servers because clients use a session-wide private key for 
	   authentication, the presence of which is checked elsewhere */
	if( isServer )
		{
		int LOOP_ITERATOR;

		LOOP_MAX_CHECK( ( attributeListPtr = \
							attributeFind( attributeListPtr, getAttrFunction, 
										   CRYPT_SESSINFO_USERNAME ) ) != NULL )
			{
			/* Make sure that there's a matching authentication attribute.  
			   This is currently a password but in future versions could 
			   also be a public key */
			REQUIRES_EXT( DATAPTR_ISVALID( attributeListPtr->next ), \
						  CRYPT_ATTRIBUTE_NONE );
			attributeListPtr = DATAPTR_GET( attributeListPtr->next );
			if( attributeListPtr == NULL || \
				attributeListPtr->attributeID != CRYPT_SESSINFO_PASSWORD )
				return( CRYPT_SESSINFO_PASSWORD );

			/* Move on to the next attribute */
			REQUIRES_EXT( DATAPTR_ISVALID( attributeListPtr->next ), \
						  CRYPT_ATTRIBUTE_NONE );
			attributeListPtr = DATAPTR_GET( attributeListPtr->next );
			}
		ENSURES_EXT( LOOP_BOUND_OK, CRYPT_SESSINFO_ACTIVE );
		}

	return( CRYPT_ATTRIBUTE_NONE );
	}

/****************************************************************************
*																			*
*					Attribute Cursor Management Routines					*
*																			*
****************************************************************************/

/* Get/set the attribute cursor */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getSessionAttributeCursor( INOUT SESSION_INFO *sessionInfoPtr,
							   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE sessionInfoType,
							   OUT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *valuePtr )
	{
	ATTRIBUTE_LIST *attributeListHead = \
							DATAPTR_GET( sessionInfoPtr->attributeList ); 
	ATTRIBUTE_LIST *attributeListCursor = \
							DATAPTR_GET( sessionInfoPtr->attributeListCurrent ); 

	assert( attributeListHead == NULL || \
			isReadPtr( attributeListHead, sizeof( ATTRIBUTE_LIST ) ) );
	assert( attributeListCursor == NULL || \
			isReadPtr( attributeListCursor, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isWritePtr( valuePtr, sizeof( int ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( sessionInfoType == CRYPT_ATTRIBUTE_CURRENT || \
			  sessionInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP || \
			  ( sessionInfoType > CRYPT_SESSINFO_FIRST && \
				sessionInfoType < CRYPT_SESSINFO_LAST ) );
	REQUIRES( DATAPTR_ISVALID( sessionInfoPtr->attributeList ) );
	REQUIRES( DATAPTR_ISVALID( sessionInfoPtr->attributeListCurrent ) );

	/* Clear return value */
	*valuePtr = CRYPT_ATTRIBUTE_NONE;

	/* We're querying something that resides in the attribute list, make 
	   sure that there's an attribute list present.  If it's present but 
	   nothing is selected, select the first entry */
	if( attributeListCursor == NULL )
		{
		if( attributeListHead == NULL )
			return( CRYPT_ERROR_NOTFOUND );
		attributeListCursor = attributeListHead;
		resetVirtualCursor( attributeListCursor );
		DATAPTR_SET( sessionInfoPtr->attributeListCurrent,
					 attributeListCursor );
		}
	REQUIRES( attributeListCursor != NULL );

	/* If we're reading the group, return the group type */
	if( sessionInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP ) 
		*valuePtr = attributeListCursor->groupID;
	else
		{
		/* If it's a single-attribute group, return the attribute type */
		if( !TEST_FLAG( attributeListCursor->flags, 
						ATTR_FLAG_COMPOSITE ) )
			*valuePtr = attributeListCursor->groupID;
		else
			{
			const ATTRACCESS_FUNCTION accessFunction = \
					( ATTRACCESS_FUNCTION ) \
					FNPTR_GET( attributeListCursor->accessFunction );
			int value, status;

			REQUIRES( accessFunction != NULL );

			/* It's a composite type, get the currently-selected sub-attribute */
			status = accessFunction( attributeListCursor, ATTR_NONE, &value );
			if( cryptStatusError( status ) )
				return( status );
			*valuePtr = value;
			}
		}
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setSessionAttributeCursor( INOUT SESSION_INFO *sessionInfoPtr,
							   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE sessionInfoType,
							   IN_RANGE( CRYPT_CURSOR_LAST, \
										 CRYPT_CURSOR_FIRST ) /* Values are -ve */
									const int position )
	{
	ATTRIBUTE_LIST *attributeListHead = \
					DATAPTR_GET( sessionInfoPtr->attributeList );
	ATTRIBUTE_LIST *attributeListCursor = \
					DATAPTR_GET( sessionInfoPtr->attributeListCurrent );

	assert( attributeListHead == NULL || \
			isReadPtr( attributeListHead, sizeof( ATTRIBUTE_LIST ) ) );
	assert( attributeListCursor == NULL || \
			isReadPtr( attributeListCursor, sizeof( ATTRIBUTE_LIST ) ) );
	
	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( sessionInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP || \
			  sessionInfoType == CRYPT_ATTRIBUTE_CURRENT );
	REQUIRES( position <= CRYPT_CURSOR_FIRST && \
			  position >= CRYPT_CURSOR_LAST );
	REQUIRES( DATAPTR_ISVALID( sessionInfoPtr->attributeList ) );
	REQUIRES( DATAPTR_ISVALID( sessionInfoPtr->attributeListCurrent ) );

	/* If it's an absolute positioning code, pre-set the attribute cursor if 
	   required */
	if( position == CRYPT_CURSOR_FIRST || position == CRYPT_CURSOR_LAST )
		{
		if( attributeListHead == NULL )
			return( CRYPT_ERROR_NOTFOUND );

		/* If it's an absolute attribute positioning code reset the 
		   attribute cursor to the start of the list before we try to move 
		   it, and if it's an attribute positioning code initialise the 
		   attribute cursor if necessary */
		if( sessionInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP || \
			attributeListCursor == NULL )
			{
			attributeListCursor = attributeListHead;
			resetVirtualCursor( attributeListCursor );
			}

		/* If there are no attributes present let the caller know.  Note 
		   that this is a redundant check because if attributeListCursor was 
		   NULL above then it had attributeListHead assigned to it, and 
		   that's non-NULL from the other check above, however we leave the
		   following in to make things a bit more explicit */
		if( attributeListCursor == NULL )
			return( CRYPT_ERROR_NOTFOUND );
		}
	else
		{
		/* It's a relative positioning code, return a not-inited error 
		   rather than a not-found error if the cursor isn't set since there 
		   may be attributes present but the cursor hasn't been initialised 
		   yet by selecting the first or last absolute attribute */
		if( attributeListCursor == NULL )
			return( CRYPT_ERROR_NOTINITED );
		}

	/* Move the cursor */
	attributeListCursor = ( ATTRIBUTE_LIST * ) \
						  attributeMoveCursor( attributeListCursor, getAttrFunction,
											   sessionInfoType, position );
	if( attributeListCursor == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	DATAPTR_SET( sessionInfoPtr->attributeListCurrent, attributeListCursor );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Find an Attribute							*
*																			*
****************************************************************************/

/* Find a session attribute by type */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const ATTRIBUTE_LIST *findSessionInfo( const SESSION_INFO *sessionInfoPtr,
									   IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE attributeID )
	{
	const ATTRIBUTE_LIST *attributeListPtr = \
							DATAPTR_GET( sessionInfoPtr->attributeList ); 

	assert( attributeListPtr == NULL || \
			isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_N( attributeID > CRYPT_SESSINFO_FIRST && \
				attributeID < CRYPT_SESSINFO_LAST );
	REQUIRES_N( DATAPTR_ISVALID( sessionInfoPtr->attributeList ) );

	return( attributeFind( attributeListPtr, getAttrFunction, attributeID ) );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const ATTRIBUTE_LIST *findSessionInfoNext( const ATTRIBUTE_LIST *attributeListPtr,
										   IN_ATTRIBUTE \
												const CRYPT_ATTRIBUTE_TYPE attributeID )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES_N( attributeID > CRYPT_SESSINFO_FIRST && \
				attributeID < CRYPT_SESSINFO_LAST );

	return( attributeFind( attributeListPtr, getAttrFunction, attributeID ) );
	}

/* Find a session attribute by type and content */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 3 ) ) \
const ATTRIBUTE_LIST *findSessionInfoEx( const SESSION_INFO *sessionInfoPtr,
										 IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE attributeID,
										 IN_BUFFER( valueLength ) const void *value, 
										 IN_LENGTH_SHORT const int valueLength )
	{
	const ATTRIBUTE_LIST *attributeListPtr = \
							DATAPTR_GET( sessionInfoPtr->attributeList ); 
	const ATTRIBUTE_LIST *attributeListCursor;
	int LOOP_ITERATOR;

	assert( attributeListPtr == NULL || \
			isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isReadPtrDynamic( value, valueLength ) );

	REQUIRES_N( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES_N( attributeID > CRYPT_SESSINFO_FIRST && \
				attributeID < CRYPT_SESSINFO_LAST );
	REQUIRES_N( isShortIntegerRangeNZ( valueLength ) );
	REQUIRES_N( DATAPTR_ISVALID( sessionInfoPtr->attributeList ) );

	/* Find the first attribute of this type */
	attributeListCursor = attributeFind( attributeListPtr, getAttrFunction, 
										 attributeID );
	if( attributeListCursor == NULL )
		return( NULL );

	/* Walk down the rest of the list looking for an attribute entry whose 
	   contents match the requested contents.  Unfortunately we can't use 
	   attributeFindNextInstance() to help us because that finds the next 
	   instance of the current attribute in an attribute group, not the next 
	   instance in an interleaved set of attributes */
	LOOP_MAX_CHECKINC( attributeListCursor != NULL,
					   attributeListCursor = DATAPTR_GET( attributeListCursor->next ) )
		{
		if( attributeListCursor->attributeID == attributeID && \
			attributeListCursor->valueLength == valueLength && \
			!memcmp( attributeListCursor->value, value, valueLength ) )
			break;
		}
	ENSURES_N( LOOP_BOUND_OK );

	return( attributeListCursor );
	}

/****************************************************************************
*																			*
*								Add an Attribute							*
*																			*
****************************************************************************/

/* Add a session attribute.  There are three versions of this function, the
   standard version and two extended versions that allow the caller to 
   specify an access function to access session subtype-specific internal
   attributes when the data being added is structured session-type-specific
   data, and that allow the use of a set of ATTR_FLAG_xxx flags to provide 
   precise control over the attribute handling */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int addInfo( INOUT SESSION_INFO *sessionInfoPtr,
					IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE groupID,
					IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
					IN_BUFFER_OPT( dataLength ) const void *data, 
					IN_LENGTH_SHORT const int dataLength, 
					IN_LENGTH_SHORT_Z const int dataMaxLength, 
					IN_OPT const ATTRACCESS_FUNCTION accessFunction, 
					IN_FLAGS_Z( ATTR ) const int flags )
	{
	ATTRIBUTE_LIST *insertPoint = DATAPTR_GET( sessionInfoPtr->attributeList );
	ATTRIBUTE_LIST *newElement;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( ( data == NULL ) || \
			( isReadPtrDynamic( data, dataLength ) && \
			  dataLength <= dataMaxLength ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( groupID > CRYPT_SESSINFO_FIRST && \
			  groupID < CRYPT_SESSINFO_LAST );
	REQUIRES( attributeID > CRYPT_SESSINFO_FIRST && \
			  attributeID < CRYPT_SESSINFO_LAST );
	REQUIRES( ( data == NULL && dataMaxLength == 0 ) || \
			  ( data != NULL && \
				dataLength > 0 && dataLength <= dataMaxLength && \
				dataMaxLength > 0 && dataMaxLength < MAX_INTLENGTH_SHORT ) );
			  /* String = { data, dataLength, dataMaxLength }, 
			     int = dataLength */
	REQUIRES( isFlagRangeZ( flags, ATTR ) );
	REQUIRES( !( flags & ATTR_FLAG_COMPOSITE ) || \
			  accessFunction != NULL );
	REQUIRES( DATAPTR_ISVALID( sessionInfoPtr->attributeList ) );

	/* Find the correct insertion point and make sure that the attribute 
	   isn't already present */
	if( insertPoint != NULL )
		{
		ATTRIBUTE_LIST *prevElement = NULL;
		int LOOP_ITERATOR;

		LOOP_MAX_CHECKINC( insertPoint != NULL,
						   insertPoint = DATAPTR_GET( insertPoint->next ) )
			{
			/* If this is a non-multivalued attribute, make sure that it
			   isn't already present */
			if( !( flags & ATTR_FLAG_MULTIVALUED ) && \
				insertPoint->attributeID == attributeID )
				return( CRYPT_ERROR_INITED );

			prevElement = insertPoint;
			}
		ENSURES( LOOP_BOUND_OK );
		insertPoint = prevElement;
		}

	/* Allocate memory for the new element and copy the information across.  
	   The data is stored in storage ... storage + dataLength, with storage
	   reserved up to dataMaxLength (if it's greater than dataLength) to
	   allow the contents to be replaced with a new fixed-length value  */
	REQUIRES( dataMaxLength == 0 || \
			  rangeCheck( dataMaxLength, 1, MAX_INTLENGTH_SHORT ) );
	if( ( newElement = ( ATTRIBUTE_LIST * ) \
					   clAlloc( "addSessionAttribute", sizeof( ATTRIBUTE_LIST ) + \
													   dataMaxLength ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	initVarStruct( newElement, ATTRIBUTE_LIST, dataMaxLength, value );
	newElement->groupID = groupID;
	newElement->attributeID = attributeID;
	INIT_FLAGS( newElement->flags, flags );
	FNPTR_SET( newElement->accessFunction, accessFunction );
	DATAPTR_SET( newElement->next, NULL );
	DATAPTR_SET( newElement->prev, NULL );
	if( data == NULL )
		newElement->intValue = dataLength;
	else
		{
		assert( isReadPtrDynamic( data, dataLength ) );

		memcpy( newElement->value, data, dataLength );
		newElement->valueLength = dataLength;
		}
	insertDoubleListElement( &sessionInfoPtr->attributeList, insertPoint, 
							 newElement, ATTRIBUTE_LIST );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addSessionInfo( INOUT SESSION_INFO *sessionInfoPtr,
					IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
					IN_INT_Z const int value )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( attributeID > CRYPT_SESSINFO_FIRST && \
			  attributeID < CRYPT_SESSINFO_LAST );
	REQUIRES( isIntegerRange( value ) );

	/* Pre-3.3 kludge: Set the groupID to the attributeID since groups 
	   aren't defined yet */
	return( addInfo( sessionInfoPtr, attributeID, attributeID, NULL, 
					 value, 0, NULL, ATTR_FLAG_NONE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int addSessionInfoS( INOUT SESSION_INFO *sessionInfoPtr,
					 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
					 IN_BUFFER( dataLength ) const void *data, 
					 IN_LENGTH_SHORT const int dataLength )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtrDynamic( data, dataLength ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( attributeID > CRYPT_SESSINFO_FIRST && \
			  attributeID < CRYPT_SESSINFO_LAST );
	REQUIRES( isShortIntegerRangeNZ( dataLength ) );

	/* Pre-3.3 kludge: Set the groupID to the attributeID since groups 
	   aren't defined yet */
	return( addInfo( sessionInfoPtr, attributeID, attributeID, data, 
					 dataLength, dataLength, NULL, ATTR_FLAG_NONE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int addSessionInfoEx( INOUT SESSION_INFO *sessionInfoPtr,
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
					  IN_BUFFER( dataLength ) const void *data, 
					  IN_LENGTH_SHORT const int dataLength, 
					  IN_FLAGS_Z( ATTR ) const int flags )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtrDynamic( data, dataLength ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( attributeID > CRYPT_SESSINFO_FIRST && \
			  attributeID < CRYPT_SESSINFO_LAST );
	REQUIRES( isShortIntegerRangeNZ( dataLength ) );
assert( ( flags & ~( ATTR_FLAG_NONE | ATTR_FLAG_ENCODEDVALUE | ATTR_FLAG_MULTIVALUED ) ) == 0 );
	REQUIRES( isFlagRangeZ( flags, ATTR ) );

	/* Pre-3.3 kludge: Set the groupID to the attributeID since groups 
	   aren't defined yet */
	return( addInfo( sessionInfoPtr, attributeID, attributeID, data, 
					 dataLength, dataLength, NULL, flags ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int addSessionInfoComposite( INOUT SESSION_INFO *sessionInfoPtr,
							 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
							 const ATTRACCESS_FUNCTION accessFunction, 
							 IN_BUFFER( dataLength ) const void *data, 
							 IN_LENGTH_SHORT const int dataLength,
							 IN_FLAGS( ATTR ) const int flags )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtrDynamic( data, dataLength ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( attributeID > CRYPT_SESSINFO_FIRST && \
			  attributeID < CRYPT_SESSINFO_LAST );
	REQUIRES( accessFunction != NULL );
	REQUIRES( isShortIntegerRangeNZ( dataLength ) );
assert( flags == ATTR_FLAG_MULTIVALUED || flags == ATTR_FLAG_COMPOSITE || \
		flags == ( ATTR_FLAG_MULTIVALUED | ATTR_FLAG_COMPOSITE ) );
	REQUIRES( isFlagRange( flags, ATTR ) );

	/* For composite attributes the groupID is the attributeID, with the
	   actual attributeID being returned by the accessFunction */
	return( addInfo( sessionInfoPtr, attributeID, attributeID, data, 
					 dataLength, dataLength, accessFunction, flags ) );
	}

/* Update a session attribute, either by replacing an existing entry if it
   already exists or by adding a new entry.  Since we can potentially update
   the entry later we specify two length values, the length of the data 
   currently being added and the maximum length that this value may take in
   the future */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int updateSessionInfo( INOUT SESSION_INFO *sessionInfoPtr,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
					   IN_BUFFER( dataLength ) const void *data, 
					   IN_LENGTH_SHORT const int dataLength,
					   IN_LENGTH_SHORT const int dataMaxLength, 
					   IN_FLAGS_Z( ATTR ) const int flags )
	{
	ATTRIBUTE_LIST *attributeListPtr = DATAPTR_GET( sessionInfoPtr->attributeList );

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtrDynamic( data, dataLength ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( attributeID > CRYPT_SESSINFO_FIRST && \
			  attributeID < CRYPT_SESSINFO_LAST );
	REQUIRES( dataLength > 0 && dataLength <= dataMaxLength && \
			  dataLength < MAX_INTLENGTH_SHORT );
	REQUIRES( isShortIntegerRangeNZ( dataMaxLength ) );
assert( ( flags & ~( ATTR_FLAG_NONE | ATTR_FLAG_EPHEMERAL | ATTR_FLAG_ENCODEDVALUE ) ) == 0 );
	REQUIRES( isFlagRangeZ( flags, ATTR ) );
	REQUIRES( !( flags & ATTR_FLAG_MULTIVALUED ) );
	REQUIRES( DATAPTR_ISVALID( sessionInfoPtr->attributeList ) );

	/* Find the first attribute of this type */
	attributeListPtr = attributeFind( attributeListPtr, getAttrFunction, 
									  attributeID );

	/* If the attribute is already present, update the value */
	if( attributeListPtr != NULL )
		{
		REQUIRES( attributeListPtr->attributeID == attributeID );
		REQUIRES( ( attributeListPtr->valueLength == 0 && \
					!memcmp( attributeListPtr->value, \
							 "\x00\x00\x00\x00", 4 ) ) || \
				  ( attributeListPtr->valueLength > 0 ) );

		assert( isReadPtrDynamic( data, dataLength ) );

		REQUIRES( dataLength <= sizeofVarStruct( attributeListPtr, \
												 ATTRIBUTE_LIST ) - \
								sizeof( ATTRIBUTE_LIST ) );
		zeroise( attributeListPtr->value, attributeListPtr->valueLength );
		memcpy( attributeListPtr->value, data, dataLength );
		attributeListPtr->valueLength = dataLength;
		return( CRYPT_OK );
		}

	/* The attribute isn't already present, it's a straight add */
	return( addInfo( sessionInfoPtr, attributeID, attributeID, data, 
					 dataLength, dataMaxLength, NULL, flags ) );
	}

/****************************************************************************
*																			*
*								Delete an Attribute							*
*																			*
****************************************************************************/

/* Delete one, or a complete set of, session attributes */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int deleteSessionInfo( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT ATTRIBUTE_LIST *attributeListPtr )
	{
	const ATTRIBUTE_LIST *attributeListCurrent = \
						DATAPTR_GET( sessionInfoPtr->attributeListCurrent );

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( DATAPTR_ISVALID( sessionInfoPtr->attributeListCurrent ) );

	/* If we're about to delete the attribute that's pointed to by the 
	   current-attribute pointer, advance it to the next attribute.  If 
	   there's no next attribute, move it to the previous attribute.  This 
	   behaviour is the most logically consistent, it means that we can do 
	   things like deleting an entire attribute list by repeatedly deleting 
	   a single attribute */
	if( attributeListCurrent == attributeListPtr )
		{
		ATTRIBUTE_LIST *attributeListCursor = \
								DATAPTR_GET( attributeListPtr->next );

		REQUIRES( DATAPTR_ISVALID( attributeListPtr->next ) );

		if( attributeListCursor == NULL )
			{
			REQUIRES( DATAPTR_ISVALID( attributeListPtr->prev ) );

			attributeListCursor = DATAPTR_GET( attributeListPtr->prev );
			}
		DATAPTR_SET( sessionInfoPtr->attributeListCurrent, 
					 attributeListCursor );
		}

	/* Remove the item from the list */
	deleteDoubleListElement( &sessionInfoPtr->attributeList, 
							 attributeListPtr, ATTRIBUTE_LIST );

	/* Clear all data in the list item and free the memory */
	endVarStruct( attributeListPtr, ATTRIBUTE_LIST );
	clFree( "deleteSessionInfo", attributeListPtr );

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteSessionInfoAll( INOUT SESSION_INFO *sessionInfoPtr )
	{
	ATTRIBUTE_LIST *attributeListCursor = \
						DATAPTR_GET( sessionInfoPtr->attributeList );
	int LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_V( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES_V( DATAPTR_ISVALID( sessionInfoPtr->attributeList ) );

	/* If the list was empty, return now */
	if( attributeListCursor == NULL )
		{
		REQUIRES_V( DATAPTR_ISNULL( sessionInfoPtr->attributeListCurrent ) );
		return;
		}

	/* Destroy any remaining list items */
	LOOP_MAX_CHECK( attributeListCursor != NULL )
		{
		ATTRIBUTE_LIST *itemToFree = attributeListCursor;

		REQUIRES_V( DATAPTR_ISVALID( attributeListCursor->next ) );
		attributeListCursor = DATAPTR_GET( attributeListCursor->next );
		deleteSessionInfo( sessionInfoPtr, itemToFree );
		}
	ENSURES_V( LOOP_BOUND_OK );

	ENSURES_V( DATAPTR_ISNULL( sessionInfoPtr->attributeList ) );
	ENSURES_V( DATAPTR_ISNULL( sessionInfoPtr->attributeListCurrent ) );
	}
#endif /* USE_SESSIONS */
