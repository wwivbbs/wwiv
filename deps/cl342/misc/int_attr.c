/****************************************************************************
*																			*
*				cryptlib Internal Attribute-list Manipulation API			*
*						Copyright Peter Gutmann 1992-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
#else
  #include "crypt.h"
#endif /* Compiler-specific includes */

/* The minimum size of an attribute-list element (in this case for 
   sessions), used for error checking in debug mode.  The values are various 
   ints and pointers, and the 'previous' and 'next' pointer for the list 
   itself */

#define MIN_ATTRLIST_SIZE	( ( 7 * sizeof( int ) ) + \
							  ( 2 * sizeof( void * ) ) )

/* Movement codes for the attribute cursor */

typedef enum {
	CURSOR_MOVE_NONE,		/* No movement type */
	CURSOR_MOVE_START,		/* Move to first attribute */
	CURSOR_MOVE_PREV,		/* Move to previous attribute */
	CURSOR_MOVE_NEXT,		/* Move to next attribute */
	CURSOR_MOVE_END,		/* Move to last attribute */
	CURSOR_MOVE_LAST		/* Last possible move type */
	} CURSOR_MOVE_TYPE;

/****************************************************************************
*																			*
*							Attribute Location Routines						*
*																			*
****************************************************************************/

/* Find the start and end of an attribute group from an attribute within
   the group */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
void *attributeFindStart( IN_OPT const void *attributePtr,
						  IN GETATTRFUNCTION getAttrFunction )
	{
	CRYPT_ATTRIBUTE_TYPE groupID;
	int iterationCount;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, MIN_ATTRLIST_SIZE ) );
	
	REQUIRES_N( getAttrFunction != NULL );

	if( attributePtr == NULL )
		return( NULL );

	/* Move backwards until we find the start of the attribute */
	if( getAttrFunction( attributePtr, &groupID, NULL, NULL, 
						 ATTR_CURRENT ) == NULL )
		return( NULL );
	ENSURES_N( groupID != CRYPT_ATTRIBUTE_NONE );
	for( iterationCount = 0; iterationCount < FAILSAFE_ITERATIONS_MAX; 
		 iterationCount++ )
		{
		CRYPT_ATTRIBUTE_TYPE prevGroupID;
		const void *prevPtr;

		prevPtr = getAttrFunction( attributePtr, &prevGroupID, NULL, NULL,
								   ATTR_PREV );
		if( prevPtr == NULL || prevGroupID != groupID )
			{
			/* We've reached the start of the list or a different attribute
			   group, this is the start of the current group */
			break;
			}
		attributePtr = prevPtr;
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( ( void * ) attributePtr );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
void *attributeFindEnd( IN_OPT const void *attributePtr,
						IN GETATTRFUNCTION getAttrFunction )
	{
	CRYPT_ATTRIBUTE_TYPE groupID;
	int iterationCount;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, MIN_ATTRLIST_SIZE ) );

	REQUIRES_N( getAttrFunction != NULL );

	if( attributePtr == NULL )
		return( NULL );

	/* Move forwards until we're just before the start of the next
	   attribute */
	if( getAttrFunction( attributePtr, &groupID, NULL, NULL, 
						 ATTR_CURRENT ) == NULL )
		return( NULL );
	ENSURES_N( groupID != CRYPT_ATTRIBUTE_NONE );
	for( iterationCount = 0; iterationCount < FAILSAFE_ITERATIONS_MAX; 
		 iterationCount++ )
		{
		CRYPT_ATTRIBUTE_TYPE nextGroupID;
		const void *nextPtr;

		nextPtr = getAttrFunction( attributePtr, &nextGroupID, NULL, NULL,
								   ATTR_NEXT );
		if( nextPtr == NULL || nextGroupID != groupID )
			{
			/* We've reached the end of the list or a different attribute
			   group, this is the end of the current group */
			break;
			}
		attributePtr = nextPtr;
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( ( void * ) attributePtr );
	}

/* Find an attribute in a list of attributes */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
void *attributeFind( IN_OPT const void *attributePtr,
					 IN GETATTRFUNCTION getAttrFunction,
					 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID )
	{
	CRYPT_ATTRIBUTE_TYPE currAttributeID;
	int iterationCount;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, MIN_ATTRLIST_SIZE ) );

	REQUIRES_N( getAttrFunction != NULL );
	REQUIRES_N( isAttribute( attributeID ) || \
				isInternalAttribute( attributeID ) );

	if( attributePtr == NULL )
		return( NULL );

	/* Find the attribute in the list */
	if( getAttrFunction( attributePtr, NULL, &currAttributeID, NULL, 
						 ATTR_CURRENT ) == NULL )
		return( NULL );
	ENSURES_N( currAttributeID != CRYPT_ATTRIBUTE_NONE );
	for( iterationCount = 0; 
		 attributePtr != NULL && currAttributeID != attributeID && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 iterationCount++ )
		{
		attributePtr = getAttrFunction( attributePtr, NULL,
										&currAttributeID, NULL,
										ATTR_NEXT );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( ( void * ) attributePtr );
	}

/* An extended form of the standard find-attribute function that searches 
   either by attribute group or by attribute + attribute-instance (the
   case of search-by-attribute is handled through findAttribute(), and the
   other combinations aren't valid) */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
void *attributeFindEx( IN_OPT const void *attributePtr,
					   IN GETATTRFUNCTION getAttrFunction,
					   IN_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							const CRYPT_ATTRIBUTE_TYPE groupID,
					   IN_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							const CRYPT_ATTRIBUTE_TYPE attributeID,
					   IN_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							const CRYPT_ATTRIBUTE_TYPE instanceID )
	{
	CRYPT_ATTRIBUTE_TYPE currAttributeID, currInstanceID;
	int iterationCount;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, MIN_ATTRLIST_SIZE ) );

	REQUIRES_N( getAttrFunction != NULL );
	REQUIRES_N( groupID == CRYPT_ATTRIBUTE_NONE || \
				isAttribute( groupID ) || \
				isInternalAttribute( groupID ) );
	REQUIRES_N( attributeID == CRYPT_ATTRIBUTE_NONE || \
				isAttribute( attributeID ) || \
				isInternalAttribute( attributeID ) );
	REQUIRES_N( instanceID == CRYPT_ATTRIBUTE_NONE || \
				isAttribute( instanceID ) || \
				isInternalAttribute( instanceID ) );
	REQUIRES_N( ( groupID != CRYPT_ATTRIBUTE_NONE && \
				  attributeID == CRYPT_ATTRIBUTE_NONE && \
				  instanceID == CRYPT_ATTRIBUTE_NONE ) || \
				( groupID == CRYPT_ATTRIBUTE_NONE && \
				  attributeID != CRYPT_ATTRIBUTE_NONE && \
				  instanceID != CRYPT_ATTRIBUTE_NONE ) );

	if( attributePtr == NULL )
		return( NULL );

	/* Find the attribute group if required */
	if( groupID != CRYPT_ATTRIBUTE_NONE )
		{
		CRYPT_ATTRIBUTE_TYPE currGroupID;

		if( getAttrFunction( attributePtr, &currGroupID, NULL, NULL, 
							 ATTR_CURRENT ) == NULL )
			return( NULL );
		ENSURES_N( currGroupID != CRYPT_ATTRIBUTE_NONE );
		for( iterationCount = 0; 
			 attributePtr != NULL && currGroupID != groupID && \
				iterationCount < FAILSAFE_ITERATIONS_MAX;
			 iterationCount++ )
			{
			attributePtr = getAttrFunction( attributePtr, &currGroupID, 
											NULL, NULL, ATTR_NEXT );
			}
		ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );

		return( ( void * ) attributePtr );
		}

	/* Find the attribute */
	if( getAttrFunction( attributePtr, NULL, &currAttributeID, NULL, 
						 ATTR_CURRENT ) == NULL )
		return( NULL );
	ENSURES_N( currAttributeID != CRYPT_ATTRIBUTE_NONE );
	for( iterationCount = 0; 
		 attributePtr != NULL && currAttributeID != attributeID && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 iterationCount++ )
		{
		attributePtr = getAttrFunction( attributePtr, NULL,
										&currAttributeID, NULL,
										ATTR_NEXT );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );
	if( attributePtr == NULL )
		return( NULL );

	/* Find the attribute instance */
	if( getAttrFunction( attributePtr, NULL, &currAttributeID, 
						 &currInstanceID, ATTR_CURRENT ) == NULL )
		return( NULL );
	ENSURES_N( currAttributeID != CRYPT_ATTRIBUTE_NONE );
	for( iterationCount = 0; 
		 attributePtr != NULL && currAttributeID == attributeID && \
			iterationCount < FAILSAFE_ITERATIONS_MAX; 
		 iterationCount++ )
		{
		if( currInstanceID == instanceID )
			return( ( void * ) attributePtr );
		attributePtr = getAttrFunction( attributePtr, NULL,
										&currAttributeID, &currInstanceID,
										ATTR_NEXT );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( NULL );
	}

/* Find the next instance of an attribute in an attribute group.  This is
   used to step through multiple instances of an attribute, for example in
   a cert extension containing a SEQUENCE OF <attribute> */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
void *attributeFindNextInstance( IN_OPT const void *attributePtr,
								 IN GETATTRFUNCTION getAttrFunction )
	{
	CRYPT_ATTRIBUTE_TYPE groupID, attributeID;
	CRYPT_ATTRIBUTE_TYPE currGroupID, currAttributeID;
	int iterationCount;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, MIN_ATTRLIST_SIZE ) );

	REQUIRES_N( getAttrFunction != NULL );

	if( attributePtr == NULL )
		return( NULL );

	/* Skip the current field */
	if( getAttrFunction( attributePtr, &groupID, &attributeID, NULL, 
						 ATTR_CURRENT ) == NULL )
		return( NULL );
	ENSURES_N( groupID != CRYPT_ATTRIBUTE_NONE && \
			   attributeID != CRYPT_ATTRIBUTE_NONE );
	attributePtr = getAttrFunction( attributePtr, &currGroupID, 
									&currAttributeID, NULL, ATTR_NEXT );
	if( attributePtr == NULL )
		{
		/* No next attribute, we're done */
		return( NULL );
		}
	ENSURES_N( currGroupID != CRYPT_ATTRIBUTE_NONE );

	/* Step through the remaining attributes in the group looking for
	   another occurrence of the current attribute */
	for( iterationCount = 0; \
		 attributePtr != NULL && currGroupID == groupID && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 iterationCount++ )
		{
		if( currAttributeID == attributeID )
			return( ( void * ) attributePtr );
		attributePtr = getAttrFunction( attributePtr, &currGroupID,
										&currAttributeID, NULL,
										ATTR_NEXT );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );

	/* We couldn't find another instance of the attribute in this group */
	return( NULL );
	}

/****************************************************************************
*																			*
*						Attribute Cursor Movement Routines					*
*																			*
****************************************************************************/

/* Moving the cursor by attribute group is a bit more complex than just 
   stepping forwards or backwards along the attribute list.  First we have 
   to find the start or end of the current group.  Then we move to the start 
   of the previous (via ATTR_PREV and attributeFindStart()), or start of the
   next (via ATTR_NEXT) group beyond that.  This has the effect of moving us 
   from anywhere in the current group to the start of the preceding or 
   following group.  Finally, we repeat this as required */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static const void *moveCursorByGroup( const void *currentCursor,
									  IN GETATTRFUNCTION getAttrFunction,
									  IN_ENUM( CURSOR_MOVE ) \
										const CURSOR_MOVE_TYPE cursorMoveType, 
									  IN_INT int count, 
									  const BOOLEAN absMove )
	{
	const void *newCursor = currentCursor, *lastCursor = NULL;
	int iterationCount;

	assert( isReadPtr( currentCursor, MIN_ATTRLIST_SIZE ) );

	REQUIRES_N( getAttrFunction != NULL );
	REQUIRES_N( cursorMoveType > CURSOR_MOVE_NONE && \
				cursorMoveType < CURSOR_MOVE_LAST );
	REQUIRES_N( count > 0 && count <= MAX_INTLENGTH );

	for( iterationCount = 0; \
		 count-- > 0 && newCursor != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 iterationCount++ )
		{
		lastCursor = newCursor;
		if( cursorMoveType == CURSOR_MOVE_START || \
			cursorMoveType == CURSOR_MOVE_PREV )
			{
			/* Move from the start of the current group to the start of the
			   preceding group */
			newCursor = attributeFindStart( newCursor, getAttrFunction );
			if( newCursor != NULL )
				newCursor = getAttrFunction( newCursor, NULL, NULL, NULL,
											 ATTR_PREV );
			if( newCursor != NULL )
				newCursor = attributeFindStart( newCursor, getAttrFunction );
			}
		else
			{
			REQUIRES_N( cursorMoveType == CURSOR_MOVE_NEXT || \
						cursorMoveType == CURSOR_MOVE_END );

			/* Move from the end of the current group to the start of the
			   next group */
			newCursor = attributeFindEnd( newCursor, getAttrFunction );
			if( newCursor != NULL )
				newCursor = getAttrFunction( newCursor, NULL, NULL, NULL,
											 ATTR_NEXT );
			}
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );
	ENSURES_N( lastCursor != NULL );	/* We went through the loop at least once */

	/* If the new cursor is NULL, we've reached the start or end of the
	   attribute list */
	if( newCursor == NULL )
		{
		/* If it's an absolute move we've reached our destination, otherwise
		   there's nowhere left to move to.  We move to the start of the
		   first or last attribute that we got to before we ran out of
		   attributes to make sure that we don't fall off the start/end of
		   the list */
		return( absMove ? \
				attributeFindStart( lastCursor, getAttrFunction ) : NULL );
		}

	/* We've found what we were looking for */
	return( newCursor );
	}

/* Moving by attribute or attribute instance is rather simpler than moving by
   group.  For attributes we move backwards or forwards until we either run 
   out of attributes or the next attribute belongs to a different group.  For 
   attribute instances we move similarly, except that we stop when we reach 
   an attribute whose group type, attribute type, and instance type don't 
   match the current one.  We have to explicitly keep track of whether the 
   cursor was successfully moved rather than checking that its value has 
   changed because some object types implement composite attributes that 
   maintain an attribute-internal virtual cursor, which can return the same 
   attribute pointer multiple times if the move is internal to the 
   (composite) attribute */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static const void *moveCursorByAttribute( const void *currentCursor,
										  GETATTRFUNCTION getAttrFunction,
										  IN_ENUM( CURSOR_MOVE ) \
											const CURSOR_MOVE_TYPE cursorMoveType, 
										  IN_INT int count, 
										  const BOOLEAN absMove )
	{
	CRYPT_ATTRIBUTE_TYPE groupID;
	BOOLEAN cursorMoved = FALSE;
	const void *newCursor = currentCursor;
	int iterationCount;

	assert( isReadPtr( currentCursor, MIN_ATTRLIST_SIZE ) );

	REQUIRES_N( getAttrFunction != NULL );
	REQUIRES_N( cursorMoveType > CURSOR_MOVE_NONE && \
				cursorMoveType < CURSOR_MOVE_LAST );
	REQUIRES_N( count > 0 && count <= MAX_INTLENGTH );

	if( getAttrFunction( currentCursor, &groupID, NULL, NULL, 
						 ATTR_CURRENT ) == NULL )
		return( NULL );
	ENSURES_N( groupID != CRYPT_ATTRIBUTE_NONE );
	if( cursorMoveType == CURSOR_MOVE_START || \
		cursorMoveType == CURSOR_MOVE_PREV )
		{
		CRYPT_ATTRIBUTE_TYPE prevGroupID;
		const void *prevCursor;

		prevCursor = getAttrFunction( newCursor, &prevGroupID, NULL, 
									  NULL, ATTR_PREV );
		for( iterationCount = 0; \
			 count-- > 0 && prevCursor != NULL && prevGroupID == groupID && \
				iterationCount < FAILSAFE_ITERATIONS_MAX;
			 iterationCount++ )
			{
			newCursor = prevCursor;
			prevCursor = getAttrFunction( newCursor, &prevGroupID, NULL, 
										  NULL, ATTR_PREV );
			cursorMoved = TRUE;
			}
		ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );
		}
	else
		{
		CRYPT_ATTRIBUTE_TYPE nextGroupID;
		const void *nextCursor;

		REQUIRES_N( cursorMoveType == CURSOR_MOVE_NEXT || \
					cursorMoveType == CURSOR_MOVE_END );

		nextCursor = getAttrFunction( newCursor, &nextGroupID, NULL,
									  NULL, ATTR_NEXT );
		for( iterationCount = 0; \
			 count-- > 0 && nextCursor != NULL && nextGroupID == groupID && \
				iterationCount < FAILSAFE_ITERATIONS_MAX;
			 iterationCount++ )
			{
			newCursor = nextCursor;
			nextCursor = getAttrFunction( newCursor, &nextGroupID, NULL,
										  NULL, ATTR_NEXT );
			cursorMoved = TRUE;
			}
		ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );
		}

	if( !absMove && !cursorMoved )
		return( NULL );
	return( newCursor );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static const void *moveCursorByInstance( const void *currentCursor,
										 GETATTRFUNCTION getAttrFunction,
										 IN_ENUM( CURSOR_MOVE ) \
											const CURSOR_MOVE_TYPE cursorMoveType, 
										 IN_INT int count, 
										 const BOOLEAN absMove )
	{
	CRYPT_ATTRIBUTE_TYPE groupID, attributeID, instanceID;
	BOOLEAN cursorMoved = FALSE;
	const void *newCursor = currentCursor;
	int iterationCount;

	assert( isReadPtr( currentCursor, MIN_ATTRLIST_SIZE ) );

	REQUIRES_N( getAttrFunction != NULL );
	REQUIRES_N( cursorMoveType > CURSOR_MOVE_NONE && \
				cursorMoveType < CURSOR_MOVE_LAST );
	REQUIRES_N( count > 0 && count <= MAX_INTLENGTH );

	if( getAttrFunction( currentCursor, &groupID, &attributeID, 
						 &instanceID, ATTR_CURRENT ) == NULL )
		return( NULL );
	ENSURES_N( groupID != CRYPT_ATTRIBUTE_NONE && \
			   attributeID != CRYPT_ATTRIBUTE_NONE );
	if( cursorMoveType == CURSOR_MOVE_START || \
		cursorMoveType == CURSOR_MOVE_PREV )
		{
		CRYPT_ATTRIBUTE_TYPE prevGroupID, prevAttrID, prevInstID;
		const void *prevCursor;

		prevCursor = getAttrFunction( newCursor, &prevGroupID,
									  &prevAttrID, &prevInstID,
									  ATTR_PREV );
		for( iterationCount = 0; \
			 count-- > 0 && prevCursor != NULL && prevGroupID == groupID && \
				prevAttrID == attributeID && prevInstID == instanceID && \
				iterationCount < FAILSAFE_ITERATIONS_MAX;
			 iterationCount++ )
			{
			newCursor = prevCursor;
			prevCursor = getAttrFunction( newCursor, &prevGroupID,
										  &prevAttrID, &prevInstID,
										  ATTR_PREV );
			cursorMoved = TRUE;
			}
		ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );
		}
	else
		{
		CRYPT_ATTRIBUTE_TYPE nextGroupID, nextAttrID, nextInstID;
		const void *nextCursor;

		REQUIRES_N( cursorMoveType == CURSOR_MOVE_NEXT || \
					cursorMoveType == CURSOR_MOVE_END );

		nextCursor = getAttrFunction( newCursor, &nextGroupID,
									  &nextAttrID, &nextInstID,
									  ATTR_NEXT );
		for( iterationCount = 0; \
			 count-- > 0 && nextCursor != NULL && nextGroupID == groupID && \
				nextAttrID == attributeID && nextInstID == instanceID && \
				iterationCount < FAILSAFE_ITERATIONS_MAX;
			 iterationCount++ )
			{
			newCursor = nextCursor;
			nextCursor = getAttrFunction( newCursor, &nextGroupID,
										  &nextAttrID, &nextInstID,
										  ATTR_NEXT );
			cursorMoved = TRUE;
			}
		ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );
		}

	if( !absMove && !cursorMoved )
		return( NULL );
	return( newCursor );
	}

/* Move the attribute cursor relative to the current cursor position */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) )\
const void *attributeMoveCursor( IN_OPT const void *currentCursor,
								 IN GETATTRFUNCTION getAttrFunction,
								 IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE attributeMoveType,
								 IN_RANGE( CRYPT_CURSOR_LAST, \
										   CRYPT_CURSOR_FIRST ) /* Values are -ve */
									const int cursorMoveType )
	{
	typedef struct {
		const int moveCode;
		const CURSOR_MOVE_TYPE cursorMoveType;
		} MOVECODE_MAP_INFO;
	static const MOVECODE_MAP_INFO moveCodeMap[] = {
		{ CRYPT_CURSOR_FIRST, CURSOR_MOVE_START },
		{ CRYPT_CURSOR_PREVIOUS, CURSOR_MOVE_PREV },
		{ CRYPT_CURSOR_NEXT, CURSOR_MOVE_NEXT },
		{ CRYPT_CURSOR_LAST, CURSOR_MOVE_END },
		{ 0, CURSOR_MOVE_NONE }, { 0, CURSOR_MOVE_NONE }
		};
	const BOOLEAN absMove = ( cursorMoveType == CRYPT_CURSOR_FIRST || \
							  cursorMoveType == CRYPT_CURSOR_LAST ) ? \
							TRUE : FALSE;
	CURSOR_MOVE_TYPE moveType;
	int count, i;

	assert( currentCursor == NULL || \
			isReadPtr( currentCursor, MIN_ATTRLIST_SIZE ) );

	REQUIRES_N( getAttrFunction != NULL );
	REQUIRES_N( attributeMoveType == CRYPT_ATTRIBUTE_CURRENT_GROUP || \
				attributeMoveType == CRYPT_ATTRIBUTE_CURRENT || \
				attributeMoveType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE );
	REQUIRES_N( cursorMoveType >= CRYPT_CURSOR_LAST && \
				cursorMoveType <= CRYPT_CURSOR_FIRST );	/* Values are -ve */

	/* Positioning in null attribute lists is always unsuccessful */
	if( currentCursor == NULL )
		return( NULL );

	/* Convert the move type into a more logical cursor move code.  We can't
	   use mapValue() for this because the move-type values overlap with the 
	   end-of-table marker value expected by mapValue() */
	for( i = 0; 
		 moveCodeMap[ i ].moveCode != cursorMoveType && \
			moveCodeMap[ i ].moveCode != 0 && \
			i < FAILSAFE_ARRAYSIZE( moveCodeMap, MOVECODE_MAP_INFO ); 
		 i++ );
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( moveCodeMap, MOVECODE_MAP_INFO ) );
	ENSURES_N( moveCodeMap[ i ].moveCode != 0 );
	moveType = moveCodeMap[ i ].cursorMoveType;

	/* Set the amount that we want to move by based on the position code.
	   This means that we can handle the movement in a simple while loop
	   instead of having to special-case it for moves by one item */
	count = absMove ? MAX_INTLENGTH : 1;

	/* Perform the appropriate attribute move type */
	switch( attributeMoveType )
		{
		case CRYPT_ATTRIBUTE_CURRENT_GROUP:
			return( moveCursorByGroup( currentCursor, getAttrFunction, 
									   moveType, count, absMove ) );

		case CRYPT_ATTRIBUTE_CURRENT:
			return( moveCursorByAttribute( currentCursor, getAttrFunction,
										   moveType, count, absMove ) );

		case CRYPT_ATTRIBUTE_CURRENT_INSTANCE:
			return( moveCursorByInstance( currentCursor, getAttrFunction,
										  moveType, count, absMove ) );
		}

	/* Everything else is an error */
	retIntError_Null();
	}
