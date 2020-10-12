/****************************************************************************
*																			*
*						cryptlib Envelope Action Management					*
*						Copyright Peter Gutmann 1996-2016					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "envelope.h"
#else
  #include "envelope/envelope.h"
#endif /* Compiler-specific includes */

/* The maximum number of actions that we can add to an action list */

#define MAX_ACTIONS		FAILSAFE_ITERATIONS_MED - 1

#ifdef USE_ENVELOPES

/****************************************************************************
*																			*
*								Find an Action								*
*																			*
****************************************************************************/

/* Sanity-check an action-list entry */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckActionList( IN const ACTION_LIST *actionListPtr )
	{
	assert( isReadPtr( actionListPtr, sizeof( ACTION_LIST ) ) );

	/* Check overall action list data */
	if( !isEnumRange( actionListPtr->action, ACTION ) || \
		!CHECK_FLAGS( actionListPtr->flags, ACTION_FLAG_NONE, 
					  ACTION_FLAG_MAX ) || \
		( actionListPtr->encodedSize != CRYPT_UNUSED && \
		  !isShortIntegerRange( actionListPtr->encodedSize ) ) )
		{
		DEBUG_PUTS(( "sanityCheckActionList: General info" ));
		return( FALSE );
		}

	/* Check safe pointers.  We don't have to check the function pointers
	   because they're validated each time they're dereferenced */
	if( !DATAPTR_ISVALID( actionListPtr->next ) || \
		!DATAPTR_ISVALID( actionListPtr->associatedAction ) )
		{
		DEBUG_PUTS(( "sanityCheckActionList: Safe pointers" ));
		return( FALSE );
		}

	/* Check object handles */
	if( ( actionListPtr->iCryptHandle != CRYPT_ERROR && \
		  !isHandleRangeValid( actionListPtr->iCryptHandle ) ) || \
		( actionListPtr->iExtraData != CRYPT_ERROR && \
		  !isHandleRangeValid( actionListPtr->iExtraData ) ) || \
		( actionListPtr->iTspSession != CRYPT_ERROR && \
		  !isHandleRangeValid( actionListPtr->iTspSession ) ) )
		{
		DEBUG_PUTS(( "sanityCheckActionList: Object handles" ));
		return( FALSE );
		}

	return( TRUE );
	}

/* Find an action of a given type and the last action of a given type.
   Since the lists are sorted by action type, the generic findAction()
   finds the start of an action group.
   
   The casting to a non-const is a bit ugly but is necessitated by the fact 
   that while the functions don't change the action list entries, the caller 
   will */

CHECK_RETVAL_PTR \
static ACTION_LIST *findActionEx( IN const ACTION_LIST *actionListPtr,
								  IN_ENUM( ACTION ) const ACTION_TYPE actionType )
	{
	int LOOP_ITERATOR;

	assert( isReadPtr( actionListPtr, sizeof( ACTION_LIST ) ) );

	REQUIRES_N( actionType == ACTION_KEYEXCHANGE_PKC || \
				actionType == ACTION_KEYEXCHANGE || \
				actionType == ACTION_xxx || \
				actionType == ACTION_CRYPT || \
				actionType == ACTION_MAC || \
				actionType == ACTION_HASH || \
				actionType == ACTION_SIGN );

	LOOP_MED_CHECKINC( actionListPtr != NULL,
					   actionListPtr = DATAPTR_GET( actionListPtr->next ) )
		{
		if( actionListPtr->action == actionType )
			return( ( ACTION_LIST * ) actionListPtr );
		}
	ENSURES_N( LOOP_BOUND_OK );

	return( NULL );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ACTION_LIST *findPreAction( IN const ENVELOPE_INFO *envelopeInfoPtr,
							IN_ENUM( ACTION ) const ACTION_TYPE actionType )
	{
	ACTION_LIST *actionListPtr = \
					DATAPTR_GET( envelopeInfoPtr->preActionList );

	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES_N( DATAPTR_ISVALID( envelopeInfoPtr->preActionList ) );
	if( actionListPtr == NULL )
		return( NULL );

	return( findActionEx( actionListPtr, actionType ) );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ACTION_LIST *findAction( IN const ENVELOPE_INFO *envelopeInfoPtr,
						 IN_ENUM( ACTION ) const ACTION_TYPE actionType )
	{
	ACTION_LIST *actionListPtr = \
					DATAPTR_GET( envelopeInfoPtr->actionList );

	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES_N( DATAPTR_ISVALID( envelopeInfoPtr->actionList ) );
	if( actionListPtr == NULL )
		return( NULL );

	return( findActionEx( actionListPtr, actionType ) );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ACTION_LIST *findPostAction( IN const ENVELOPE_INFO *envelopeInfoPtr,
							 IN_ENUM( ACTION ) const ACTION_TYPE actionType )
	{
	ACTION_LIST *actionListPtr = \
					DATAPTR_GET( envelopeInfoPtr->postActionList );

	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES_N( DATAPTR_ISVALID( envelopeInfoPtr->postActionList ) );
	if( actionListPtr == NULL )
		return( NULL );

	return( findActionEx( actionListPtr, actionType ) );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ACTION_LIST *findLastAction( IN const ENVELOPE_INFO *envelopeInfoPtr,
							 IN_ENUM( ACTION ) const ACTION_TYPE actionType )
	{
	const ACTION_LIST *actionListPtr, *prevActionPtr;
	int LOOP_ITERATOR;

	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES_N( sanityCheckEnvelope( envelopeInfoPtr ) );
	REQUIRES_N( actionType == ACTION_HASH || \
				actionType == ACTION_SIGN );

	/* Get the appropriate action list for this action.  Since there are 
	   only two actions that we can look for, both related to signing, the 
	   action list to use can be inferred from the action type */
	if( actionType == ACTION_HASH )
		actionListPtr = DATAPTR_GET( envelopeInfoPtr->actionList );
	else
		actionListPtr = DATAPTR_GET( envelopeInfoPtr->postActionList );
	if( actionListPtr == NULL )
		return( NULL );
	ENSURES_N( sanityCheckActionList( actionListPtr ) );

	/* Find the start of the action group */
	actionListPtr = findActionEx( actionListPtr, actionType );
	if( actionListPtr == NULL )
		return( NULL );
	ENSURES_N( sanityCheckActionList( actionListPtr ) );

	/* Find the end of the action group */
	LOOP_MED( prevActionPtr = actionListPtr,
			  actionListPtr != NULL && actionListPtr->action == actionType,
			  ( prevActionPtr = actionListPtr, 
			    actionListPtr = DATAPTR_GET( actionListPtr->next ) ) );
	ENSURES_N( LOOP_BOUND_OK );

	return( ( ACTION_LIST * ) prevActionPtr );
	}

/* An indirect action-check function that uses a caller-supplied callback to 
   verify a match for an action */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ACTION_LIST *findActionIndirect( const ACTION_LIST *actionListStart,
								 IN CHECKACTION_FUNCTION checkActionFunction,
								 IN_INT_Z const int intParam )
	{
	const ACTION_LIST *actionListPtr;
	int LOOP_ITERATOR;

	assert( isReadPtr( actionListStart, sizeof( ACTION_LIST ) ) );

	REQUIRES_N( checkActionFunction != NULL );

	LOOP_MED( actionListPtr = actionListStart, actionListPtr != NULL,
			  actionListPtr = DATAPTR_GET( actionListPtr->next ) )
		{
		int status;
		
		REQUIRES_N( sanityCheckActionList( actionListPtr ) );
		 
		status = checkActionFunction( actionListPtr, intParam );
		if( cryptStatusOK( status ) )
			return( ( ACTION_LIST * ) actionListPtr );
		}
	ENSURES_N( LOOP_BOUND_OK );

	return( NULL );
	}

/****************************************************************************
*																			*
*								Add/Delete an Action						*
*																			*
****************************************************************************/

/* Check whether more actions can be added to an action list */

CHECK_RETVAL_BOOL \
BOOLEAN moreActionsPossible( IN_OPT const ACTION_LIST *actionListPtr )
	{
	int actionCount, LOOP_ITERATOR;

	assert( actionListPtr == NULL || \
			isReadPtr( actionListPtr, sizeof( ACTION_LIST ) ) );

	LOOP_MED( actionCount = 0,
			  actionListPtr != NULL && actionCount < MAX_ACTIONS,
			  ( actionListPtr = DATAPTR_GET( actionListPtr->next ), actionCount++ ) );
	ENSURES_B( LOOP_BOUND_OK );

	return( ( actionCount < MAX_ACTIONS ) ? TRUE : FALSE );
	}

/* Add a new action to the end of an action group in an action list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int createNewAction( OUT_OPT_PTR_COND ACTION_LIST **newActionPtrPtr,
							INOUT ENVELOPE_INFO *envelopeInfoPtr,
							IN_ENUM( ACTIONLIST ) \
								const ACTIONLIST_TYPE actionListType,
							IN_ENUM( ACTION ) const ACTION_TYPE actionType,
							IN_HANDLE const CRYPT_HANDLE cryptHandle )							
	{
	ACTION_LIST *actionListPtr, *prevActionPtr = NULL, *newItem;
	int LOOP_ITERATOR;

	assert( newActionPtrPtr == NULL || \
			isWritePtr( newActionPtrPtr, sizeof( ACTION_LIST * ) ) );
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( isEnumRange( actionListType, ACTIONLIST ) );
	REQUIRES( actionType == ACTION_KEYEXCHANGE_PKC || \
			  actionType == ACTION_KEYEXCHANGE || \
			  actionType == ACTION_xxx || \
			  actionType == ACTION_CRYPT || \
			  actionType == ACTION_MAC || \
			  actionType == ACTION_HASH || \
			  actionType == ACTION_SIGN );
	REQUIRES( isHandleRangeValid( cryptHandle ) );

	/* Clear return value */
	if( newActionPtrPtr != NULL )
		*newActionPtrPtr = NULL;

	/* Get the appropriate action list */
	switch( actionListType )
		{
		case ACTIONLIST_PREACTION:
			REQUIRES( DATAPTR_ISVALID( envelopeInfoPtr->preActionList ) );
			actionListPtr = DATAPTR_GET( envelopeInfoPtr->preActionList );
			break;
		
		case ACTIONLIST_ACTION:
			REQUIRES( DATAPTR_ISVALID( envelopeInfoPtr->actionList ) );
			actionListPtr = DATAPTR_GET( envelopeInfoPtr->actionList );
			break;
		
		case ACTIONLIST_POSTACTION:
			REQUIRES( DATAPTR_ISVALID( envelopeInfoPtr->postActionList ) );
			actionListPtr = DATAPTR_GET( envelopeInfoPtr->postActionList );
			break;

		default:
			retIntError();
		}

	/* Create the new action list item */
	if( ( newItem = getMemPool( envelopeInfoPtr->memPoolState, \
								sizeof( ACTION_LIST ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( newItem, 0, sizeof( ACTION_LIST ) );
	newItem->action = actionType;
	INIT_FLAGS( newItem->flags, ACTION_FLAG_NONE );
	newItem->iCryptHandle = cryptHandle;
	newItem->iExtraData = CRYPT_ERROR;
	newItem->iTspSession = CRYPT_ERROR;
	DATAPTR_SET( newItem->associatedAction, NULL );
	DATAPTR_SET( newItem->next, NULL );
	ENSURES( sanityCheckActionList( newItem ) );

	/* Find the last action in the action group */
	LOOP_MED_CHECKINC( actionListPtr != NULL && \
							actionListPtr->action <= actionType,
					   actionListPtr = DATAPTR_GET( actionListPtr->next ) )
		{
		prevActionPtr = actionListPtr;
		}
	ENSURES( LOOP_BOUND_OK );

	/* Append the new action to the appropriate action list */
	switch( actionListType )
		{
		case ACTIONLIST_PREACTION:
			insertSingleListElement( envelopeInfoPtr->preActionList, 
									 prevActionPtr, newItem, ACTION_LIST );
			break;
		
		case ACTIONLIST_ACTION:
			insertSingleListElement( envelopeInfoPtr->actionList, 
									 prevActionPtr, newItem, ACTION_LIST );
			break;
		
		case ACTIONLIST_POSTACTION:
			insertSingleListElement( envelopeInfoPtr->postActionList, 
									 prevActionPtr, newItem, ACTION_LIST );
			break;

		default:
			retIntError();
		}
	if( newActionPtrPtr != NULL )
		*newActionPtrPtr = newItem;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int addActionEx( OUT_OPT_PTR_COND ACTION_LIST **newActionPtrPtr,
				 INOUT ENVELOPE_INFO *envelopeInfoPtr,
				 IN_ENUM( ACTIONLIST ) const ACTIONLIST_TYPE actionListType,
				 IN_ENUM( ACTION ) const ACTION_TYPE actionType,
				 IN_HANDLE const CRYPT_HANDLE cryptHandle )
	{
	assert( isWritePtr( newActionPtrPtr, sizeof( ACTION_LIST * ) ) );
		/* Rest are checked in createNewAction() */

	return( createNewAction( newActionPtrPtr, envelopeInfoPtr, 
							 actionListType, actionType, cryptHandle ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addAction( INOUT ENVELOPE_INFO *envelopeInfoPtr,
			   IN_ENUM( ACTION ) const ACTION_TYPE actionType,
			   IN_HANDLE const CRYPT_HANDLE cryptHandle )
	{
	return( createNewAction( NULL, envelopeInfoPtr, ACTIONLIST_ACTION, 
							 actionType, cryptHandle ) );
	}

/* Replace a context in an action with a different one, used to update an
   existing action when circumstances change */

STDC_NONNULL_ARG( ( 1 ) ) \
int replaceAction( INOUT ACTION_LIST *actionListItem,
				   IN_HANDLE const CRYPT_HANDLE cryptHandle )
	{
	assert( isWritePtr( actionListItem, sizeof( ACTION_LIST ) ) );

	REQUIRES( isHandleRangeValid( cryptHandle ) );
	REQUIRES( actionListItem->iCryptHandle != CRYPT_ERROR && \
			  actionListItem->iExtraData == CRYPT_ERROR && \
			  actionListItem->iTspSession == CRYPT_ERROR );
	REQUIRES( sanityCheckActionList( actionListItem ) );

	/* Delete the existing action context and replace it with the new one */
	krnlSendNotifier( actionListItem->iCryptHandle, IMESSAGE_DECREFCOUNT );
	actionListItem->iCryptHandle = cryptHandle;

	return( CRYPT_OK );
	}

/* Delete an action from an action list */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void deleteActionListItem( INOUT MEMPOOL_STATE memPoolState,
								  INOUT ACTION_LIST *actionListItem )
	{
	assert( isWritePtr( memPoolState, sizeof( MEMPOOL_STATE ) ) );
	assert( isWritePtr( actionListItem, sizeof( ACTION_LIST ) ) );

	/* Destroy any attached objects and information if necessary and
	   clear the list item memory */
	if( actionListItem->iCryptHandle != CRYPT_ERROR )
		krnlSendNotifier( actionListItem->iCryptHandle, IMESSAGE_DECREFCOUNT );
	if( actionListItem->iExtraData != CRYPT_ERROR )
		krnlSendNotifier( actionListItem->iExtraData, IMESSAGE_DECREFCOUNT );
	if( actionListItem->iTspSession != CRYPT_ERROR )
		krnlSendNotifier( actionListItem->iTspSession, IMESSAGE_DECREFCOUNT );
	zeroise( actionListItem, sizeof( ACTION_LIST ) );
	freeMemPool( memPoolState, actionListItem );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int deleteAction( INOUT ENVELOPE_INFO *envelopeInfoPtr,	
						 INOUT ACTION_LIST *actionListItem )
	{
	ACTION_LIST *listPrevPtr = DATAPTR_GET( envelopeInfoPtr->actionList );
	int LOOP_ITERATOR;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( actionListItem, sizeof( ACTION_LIST ) ) );

	REQUIRES( listPrevPtr != NULL );

	/* Find the previons entry in the list */
	LOOP_MED_CHECKINC( listPrevPtr != NULL && \
							DATAPTR_GET( listPrevPtr->next ) != actionListItem,
					   listPrevPtr = DATAPTR_GET( listPrevPtr->next ) );
	ENSURES( LOOP_BOUND_OK );

	/* Remove the item from the list */
	deleteSingleListElement( envelopeInfoPtr->actionList, listPrevPtr, 
							 actionListItem, ACTION_LIST );

	/* Clear all data in the list item and free the memory */
	deleteActionListItem( envelopeInfoPtr->memPoolState, actionListItem );
	
	return( CRYPT_OK );
	}

/* Delete the action lists */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void deleteActionList( INOUT MEMPOOL_STATE memPoolState,
							  INOUT ACTION_LIST *actionListPtr )
	{
	int LOOP_ITERATOR;

	assert( isWritePtr( memPoolState, sizeof( MEMPOOL_STATE ) ) );
	assert( isReadPtr( actionListPtr, sizeof( ACTION_LIST ) ) );

	LOOP_MED_CHECK( actionListPtr != NULL )
		{
		ACTION_LIST *actionListItem = actionListPtr;

		REQUIRES_V( DATAPTR_ISVALID( actionListPtr->next ) );
		actionListPtr = DATAPTR_GET( actionListPtr->next );
		deleteActionListItem( memPoolState, actionListItem );
		}
	ENSURES_V( LOOP_BOUND_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteActionLists( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	ACTION_LIST *actionListPtr;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES_V( sanityCheckEnvelope( envelopeInfoPtr ) );

	REQUIRES_V( DATAPTR_ISVALID( envelopeInfoPtr->preActionList ) );
	if( ( actionListPtr = \
				DATAPTR_GET( envelopeInfoPtr->preActionList ) ) != NULL )
		{
		deleteActionList( envelopeInfoPtr->memPoolState, actionListPtr );
		DATAPTR_SET( envelopeInfoPtr->preActionList, NULL );
		}
	REQUIRES_V( DATAPTR_ISVALID( envelopeInfoPtr->actionList ) );
	if( ( actionListPtr = \
				DATAPTR_GET( envelopeInfoPtr->actionList ) ) != NULL )
		{
		deleteActionList( envelopeInfoPtr->memPoolState, actionListPtr );
		DATAPTR_SET( envelopeInfoPtr->actionList, NULL );
		}
	REQUIRES_V( DATAPTR_ISVALID( envelopeInfoPtr->postActionList ) );
	if( ( actionListPtr = \
				DATAPTR_GET( envelopeInfoPtr->postActionList ) ) != NULL )
		{
		deleteActionList( envelopeInfoPtr->memPoolState, actionListPtr );
		DATAPTR_SET( envelopeInfoPtr->postActionList, NULL );
		}
	}

/* Delete any orphaned actions, for example automatically-added hash actions
   that were overridden by user-supplied alternate actions */

STDC_NONNULL_ARG( ( 1 ) ) \
int deleteUnusedActions( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	ACTION_LIST *actionListPtr;
	int status, LOOP_ITERATOR;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheckEnvelope( envelopeInfoPtr ) );

	/* Check for unattached encryption, hash/MAC, or generic-secret actions 
	   and delete them */
	LOOP_MED_INITCHECK( actionListPtr = DATAPTR_GET( envelopeInfoPtr->actionList ), 
						actionListPtr != NULL )
		{
		ACTION_LIST *actionListCurrent = actionListPtr;

		REQUIRES( DATAPTR_ISVALID( actionListPtr->next ) );
		actionListPtr = DATAPTR_GET( actionListPtr->next );
		if( ( actionListCurrent->action == ACTION_CRYPT || \
			  actionListCurrent->action == ACTION_HASH || \
			  actionListCurrent->action == ACTION_MAC || \
			  actionListCurrent->action == ACTION_xxx ) && \
			TEST_FLAG( actionListCurrent->flags, 
					   ACTION_FLAG_NEEDSCONTROLLER ) )
			{
			status = deleteAction( envelopeInfoPtr, actionListCurrent );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Check an Action								*
*																			*
****************************************************************************/

/* Check a new action to make sure that it isn't already present in the
   action list, producing an ACTION_RESULT outcome */

CHECK_RETVAL_ENUM( ACTION ) \
ACTION_RESULT checkAction( IN_OPT const ACTION_LIST *actionListStart,
						   IN_ENUM( ACTION ) const ACTION_TYPE actionType, 
						   IN_HANDLE const CRYPT_HANDLE cryptHandle )
	{
	ACTION_LIST *actionListPtr = ( ACTION_LIST * ) actionListStart;
	MESSAGE_DATA msgData;
	BYTE keyID[ KEYID_SIZE + 8 ];
	int algorithm DUMMY_INIT, status, LOOP_ITERATOR;

	assert( actionListPtr == NULL || \
			isReadPtr( actionListPtr, sizeof( ACTION_LIST ) ) );

	REQUIRES_EXT( actionListPtr == NULL || \
				  sanityCheckActionList( actionListPtr ), 
				  ACTION_RESULT_ERROR );
	REQUIRES_EXT( ( actionType == ACTION_KEYEXCHANGE_PKC || \
					actionType == ACTION_KEYEXCHANGE || \
					actionType == ACTION_CRYPT || \
					actionType == ACTION_MAC || \
					actionType == ACTION_HASH || \
					actionType == ACTION_SIGN ), ACTION_RESULT_ERROR );
	REQUIRES_EXT( isHandleRangeValid( cryptHandle ), ACTION_RESULT_ERROR );

	/* If the action list is empty, there's nothing to check */
	if( actionListPtr == NULL )
		return( ACTION_RESULT_EMPTY );

	/* Get identification information for the action object */
	switch( actionType )
		{
		case ACTION_KEYEXCHANGE:
			/* For conventional key wrap we can't really do much, for raw
			   action objects we'd check the algorithm for duplicates but
			   it's perfectly valid to wrap a single session/MAC key using
			   multiple key wrap objects with the same algorithm */
			status = CRYPT_OK;
			break;

		case ACTION_KEYEXCHANGE_PKC:
		case ACTION_SIGN:
			/* It's a PKC object, get the key ID */
			setMessageData( &msgData, keyID, KEYID_SIZE );
			status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
									  &msgData, CRYPT_IATTRIBUTE_KEYID );
			break;

		case ACTION_HASH:
		case ACTION_MAC:
		case ACTION_CRYPT:
			/* It's a raw action object, get the algorithm */
			status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE,
									  &algorithm, CRYPT_CTXINFO_ALGO );
			break;

		default:
			retIntError_Ext( ACTION_RESULT_ERROR );
		}
	if( cryptStatusError( status ) )
		return( ACTION_RESULT_ERROR );

	/* Walk down the list from the first to the last action in the action
	   group checking each one in turn */
	LOOP_MED( actionListPtr = findActionEx( actionListPtr, actionType ), 
			  actionListPtr != NULL && actionListPtr->action == actionType,
			  actionListPtr = DATAPTR_GET( actionListPtr->next ) )
		{
		BOOLEAN isDuplicate = FALSE;
		int actionAlgo;

		REQUIRES_EXT( actionListPtr == NULL || \
					  sanityCheckActionList( actionListPtr ), 
					  ACTION_RESULT_ERROR );

		/* Make sure that we haven't added this action already.  This can
		   get a bit tricky both because detecting some types of duplicates
		   is rather hard and because the definition of what's an invalid
		   duplicate varies somewhat.  For a hash, MAC, and encryption
		   action we only allow one action of a given algorithm type to
		   be added.  For a PKC key exchange or signature action we only
		   allow one action for a given key to be added.  For a conventional
		   key exchange action we should in theory check for duplicates in
		   some form but it's not certain what constitutes a duplicate (for
		   example are two otherwise identical actions with a different
		   number of key setup iterations considered duplicates or not?) so
		   for now we assume that the user won't do anything silly (in any 
		   case for any key exchange action the only thing that a duplicate 
		   will do is result in unnecessary bloating of the envelope 
		   header).

		   In addition to the more sophisticated checks we also perform a 
		   few more basic ones for the same object being added twice, which
		   doesn't catch e.g. inadvertent use of the same keying material
		   but does catch simple programming errors */
		if( actionListPtr->iCryptHandle == cryptHandle )
			return( ACTION_RESULT_INITED );
		switch( actionType )
			{
			case ACTION_KEYEXCHANGE:
				/* It's a conventional key exchange, there's not much that
				   we can check */
				break;

			case ACTION_KEYEXCHANGE_PKC:
			case ACTION_SIGN:
				/* It's a PKC key exchange or signature action, compare the
				   two objects by comparing their keys */
				setMessageData( &msgData, keyID, KEYID_SIZE );
				if( cryptStatusOK( \
						krnlSendMessage( actionListPtr->iCryptHandle,
										 IMESSAGE_COMPARE, &msgData,
										 MESSAGE_COMPARE_KEYID ) ) )
					isDuplicate = TRUE;
				break;

			case ACTION_HASH:
			case ACTION_MAC:
			case ACTION_CRYPT:
				/* It's a hash/MAC or session key object, compare the two
				   objects by comparing their algorithms */
				if( cryptStatusOK( \
					krnlSendMessage( actionListPtr->iCryptHandle,
									 IMESSAGE_GETATTRIBUTE, &actionAlgo,
									 CRYPT_CTXINFO_ALGO ) ) && \
					actionAlgo == algorithm )
					isDuplicate = TRUE;
				break;

			}
		if( isDuplicate )
			{
			/* If the action was added automatically/implicitly as the
			   result of adding another action then the first attempt to add
			   it explicitly by the caller isn't an error.  The caller will
			   treat the ACTION_RESULT_PRESENT code as CRYPT_OK */
			if( TEST_FLAG( actionListPtr->flags, 
						   ACTION_FLAG_ADDEDAUTOMATICALLY ) )
				{
				CLEAR_FLAG( actionListPtr->flags, 
							ACTION_FLAG_ADDEDAUTOMATICALLY );
				return( ACTION_RESULT_PRESENT );
				}

			return( ACTION_RESULT_INITED );
			}
		}
	ENSURES_EXT( LOOP_BOUND_OK, ACTION_RESULT_ERROR );

	return( ACTION_RESULT_OK );
	}

/* An indirect action-check function that uses a caller-supplied callback to 
   verify each action */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int checkActionIndirect( IN const ACTION_LIST *actionListStart,
						 IN CHECKACTION_FUNCTION checkActionFunction,
						 IN_INT_Z const int intParam )
	{
	const ACTION_LIST *actionListPtr;
	int LOOP_ITERATOR;

	assert( isReadPtr( actionListStart, sizeof( ACTION_LIST ) ) );

	REQUIRES( sanityCheckActionList( actionListStart ) );
	REQUIRES( checkActionFunction != NULL );

	LOOP_MED( actionListPtr = actionListStart, actionListPtr != NULL, 
			  actionListPtr = DATAPTR_GET( actionListPtr->next ) )
		{
		int status;

		REQUIRES( sanityCheckActionList( actionListPtr ) );

		status = checkActionFunction( actionListPtr, intParam );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

/* Check that the actions in an envelope are consistent.  There are two 
   approaches to this, take the envelope usage and check that everything is 
   consistent with it, or take the actions and make sure that they're 
   consistent with the usage (and each other).  We perform the latter type 
   of check, which is somewhat simpler.  The requirements that we enforce 
   are:

			|	Pre		|	In		|	Post	|
	--------+-----------+-----------+-----------+-----
	  SIG	|	  -		|	Hash	|	 Sig	| CMS
			|	  -		| 1x Hash	|  1x Sig	| PGP
	--------+-----------+-----------+-----------+-----
	  MAC	| Keyex,PKC	|  1x MAC	|	  -		| CMS
			|	  -		|	  -		|	  -		| PGP
	--------+-----------+-----------+-----------+-----
	  COPR	|	  -		|	  -		|	  -		| CMS
			|	  -		|	  -		|	  -		| PGP
	--------+-----------+-----------+-----------+-----
	  ENCR	| Keyex,PKC	|	Crypt	|	  -		| CMS
			|	 PKC	| 1x Crypt	|	  -		| PGP

   In the case of ENCR the pre-actions can be absent if we're using raw 
   session-key encryption */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkActions( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	const ACTION_LIST *preActionListPtr = \
					DATAPTR_GET( envelopeInfoPtr->preActionList );
	const ACTION_LIST *actionListPtr = \
					DATAPTR_GET( envelopeInfoPtr->actionList );
	const ACTION_LIST *postActionListPtr = \
					DATAPTR_GET( envelopeInfoPtr->postActionList );
	const ACTION_LIST *actionListCursor, *actionListPtrNext;
	int LOOP_ITERATOR;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES_B( sanityCheckEnvelope( envelopeInfoPtr ) );

	/* If there are no pre-, post-, or main actions (i.e. it's a compressed
	   or data-only envelope), we're done */
	if( actionListPtr == NULL )
		{
		/* Make sure that the envelope has the appropriate usage for these 
		   actions */
		if( envelopeInfoPtr->usage != ACTION_COMPRESS && \
			envelopeInfoPtr->usage != ACTION_NONE )
			return( FALSE );

		/* There can be no pre- or post-actions present for this usage */
		if( preActionListPtr != NULL || postActionListPtr != NULL )
			return( FALSE );

		return( TRUE );
		}
	REQUIRES_B( DATAPTR_ISVALID( actionListPtr->next ) );
	actionListPtrNext = DATAPTR_GET( actionListPtr->next );
	REQUIRES_B( actionListPtrNext == NULL || \
				sanityCheckActionList( actionListPtrNext ) );

	/* If there are pre-actions it has to be a key exchange followed by 
	   encryption or MAC actions */
	if( preActionListPtr != NULL )
		{
		int cryptActionCount = 0, macActionCount = 0;
		int genericSecretActionCount = 0;

		/* Make sure that the envelope has the appropriate usage for these 
		   actions */
		if( envelopeInfoPtr->usage != ACTION_CRYPT && \
			envelopeInfoPtr->usage != ACTION_MAC )
			return( FALSE );

		/* If there's a pre-action then there has to be a main action 
		   list.  This is checked by the actionListPtr check earlier so
		   there's no need to explicitly check it again here */
		ENSURES_B( actionListPtr != NULL );

		/* Pre-actions can only be key exchange actions and have to be sorted 
		   by action group */
		LOOP_MED( actionListCursor = preActionListPtr, 
				  actionListCursor != NULL && \
					actionListCursor->action == ACTION_KEYEXCHANGE_PKC,
				  actionListCursor = DATAPTR_GET( actionListCursor->next ) );
		ENSURES_B( LOOP_BOUND_OK );
		if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP && \
			actionListCursor != NULL )
			{
			/* PGP can't have any conventional keyex actions since the 
			   password is used to directly derive the session key */
			return( FALSE );
			}
		LOOP_MED_CHECKINC( actionListCursor != NULL && \
								actionListCursor->action == ACTION_KEYEXCHANGE,
						   actionListCursor = DATAPTR_GET( actionListCursor->next ) );
		ENSURES_B( LOOP_BOUND_OK );
		if( actionListCursor != NULL )
			return( FALSE );

		/* PGP only supports an encryption action followed by an optional 
		   hash action for encryption with MDC */
		if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP )
			{
			if( actionListPtr->action != ACTION_CRYPT )
				return( FALSE );
			if( actionListPtrNext != NULL )
				{
				REQUIRES_B( DATAPTR_ISVALID( actionListPtrNext->next ) );
				if( actionListPtrNext->action != ACTION_HASH || \
					DATAPTR_ISSET( actionListPtrNext->next ) )
					return( FALSE );
				}

			/* There can't be any post-actions */
			if( postActionListPtr != NULL )
				return( FALSE );

			return( TRUE );
			}

		/* Key exchange must be followed by a single encryption, one or more 
		   MAC actions, or a sequence of { generic-secret, encryption, MAC } 
		   actions.  First we count the actions present */
		LOOP_MED( actionListCursor = actionListPtr, 
				  actionListCursor != NULL,
				  actionListCursor = DATAPTR_GET( actionListCursor->next ) )
			{
			REQUIRES_B( sanityCheckActionList( actionListCursor ) );

			switch( actionListCursor->action )
				{
				case ACTION_xxx:
					genericSecretActionCount++;
					break;
				
				case ACTION_CRYPT:
					cryptActionCount++;
					break;

				case ACTION_MAC:
					macActionCount++;
					break;

				default:
					return( FALSE );
				}
			}
		ENSURES_B( LOOP_BOUND_OK );

		/* Then we make sure that what's present follows the requirements 
		   given above */
		if( genericSecretActionCount > 0 )
			{
			/* AuthEnc envelope, we need a sequence of { generic-secret, 
			   encryption, MAC } */
			if( genericSecretActionCount != 1 || \
				cryptActionCount != 1 || macActionCount != 1 )
				return( FALSE );
			}
		else
			{
			if( cryptActionCount > 0 )
				{
				/* Encrypted envelope, we need a single encryption action */
				if( cryptActionCount > 1 || \
					genericSecretActionCount != 0 || macActionCount != 0 )
					return( FALSE );
				}
			else
				{
				/* MACed envelope, we need one or more MAC actions (the check
				   for genericSecretActionCount is redudant since we already
				   know that it's 0, but it's included here to document the
				   required condition) */
				if( genericSecretActionCount != 0 || cryptActionCount != 0 )
					return( FALSE );
				}
			}

		/* There can't be any post-actions */
		if( postActionListPtr != NULL )
			return( FALSE );

		return( TRUE );
		}

	/* If there are post-actions then it has to be a hash follwed by 
	   signature actions */
	if( postActionListPtr != NULL )
		{
		int hashActionCount = 0, sigActionCount = 0;

		/* Make sure that the envelope has the appropriate usage for these 
		   actions */
		if( envelopeInfoPtr->usage != ACTION_SIGN )
			return( FALSE );

		/* If there's a post-action then there can't be a pre-action 
		   list.  This is checked by the preActionListPtr check earlier so
		   there's no need to explicitly check it again here */
		ENSURES_B( preActionListPtr == NULL );

		/* The signature must be preceded by one or more hash actions */
		LOOP_MED( actionListCursor = actionListPtr, 
				  actionListCursor != NULL, 
				  actionListCursor = DATAPTR_GET( actionListCursor->next ) )
			{
			REQUIRES_B( sanityCheckActionList( actionListCursor ) );

			if( actionListCursor->action != ACTION_HASH )
				return( FALSE );
			hashActionCount++;
			}
		ENSURES_B( LOOP_BOUND_OK );

		/* PGP can only have a single hash per signed envelope */
		if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP && hashActionCount > 1 )
			return( FALSE );

		/* Hash actions must be followed by one or more signature actions */
		LOOP_MED( actionListCursor = postActionListPtr, 
				  actionListCursor != NULL,
				  actionListCursor = DATAPTR_GET( actionListCursor->next ) )
			{
			REQUIRES_B( sanityCheckActionList( actionListCursor ) );

			if( actionListCursor->action != ACTION_SIGN )
				return( FALSE );
			sigActionCount++;
			}
		ENSURES_B( LOOP_BOUND_OK );

		/* PGP can only have a single signature, multiple signatures are 
		   handled by nesting envelopes */
		if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP && sigActionCount > 1 )
			return( FALSE );

		return( TRUE );
		}

	/* If there's a standalone session-key encryption action then it has to 
	   be the only action present */
	if( actionListPtr->action == ACTION_CRYPT )
		{
		/* Make sure that the envelope has the appropriate usage for these 
		   actions */
		if( envelopeInfoPtr->usage != ACTION_CRYPT )
			return( FALSE );

		/* If we're performing authenticated encryption then the encryption
		   action has to be followed by a MAC action (CMS) or a hash action
		   (PGP, which is encrypted and sort-of keyed so it's a sort-of 
		   MAC) */
		if( TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_AUTHENC ) )
			{
			const ACTION_TYPE requiredActionType = \
				( envelopeInfoPtr->type == CRYPT_FORMAT_PGP ) ? \
				ACTION_HASH : ACTION_MAC;

			if( actionListPtrNext == NULL || \
				actionListPtrNext->action != requiredActionType )
				return( FALSE );
			REQUIRES_B( DATAPTR_ISVALID( actionListPtrNext->next ) );
			if( DATAPTR_ISSET( actionListPtrNext->next ) )
				return( FALSE );

			return( TRUE );
			}

		/* PGP can optionally follow an encryption action with a hash action
		   for encryption with MDC */
		if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP && \
			actionListPtrNext != NULL )
			{
			if( actionListPtrNext->action != ACTION_HASH )
				return( FALSE );
			REQUIRES_B( DATAPTR_ISVALID( actionListPtrNext->next ) );
			if( DATAPTR_ISSET( actionListPtrNext->next ) )
				return( FALSE );

			return( TRUE );
			}

		/* There can only be one encryption action present */
		if( actionListPtrNext != NULL )
			return( FALSE );

		return( TRUE );
		}

	/* If we're processing PGP-encrypted data with an MDC at the end of the 
	   encrypted data then it's possible to have an encryption envelope with
	   a hash action (which must be followed by an encryption action) */
	if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP && \
		actionListPtr->action == ACTION_HASH && \
		actionListPtrNext != NULL && \
		actionListPtrNext->action == ACTION_CRYPT )
		{
		/* Make sure that the envelope has the appropriate usage for these 
		   actions */
		if( envelopeInfoPtr->usage != ACTION_CRYPT )
			return( FALSE );

		/* Make sure that the encryption action is the only other action */
		if( actionListPtrNext->action != ACTION_CRYPT )
			return( FALSE );
		REQUIRES_B( DATAPTR_ISVALID( actionListPtrNext->next ) );
		if( DATAPTR_ISSET( actionListPtrNext->next ) )
			return( FALSE );

		return( TRUE );
		}

	/* If it's a MACd envelope then there can only be a single MAC action 
	   present */
	if( envelopeInfoPtr->usage == ACTION_MAC )
		{
		/* Make sure that there's only a single MAC action present */
		if( actionListPtr->action != ACTION_MAC || \
			actionListPtrNext != NULL )
			return( FALSE );

		return( TRUE );
		}

	/* Anything else has to be a signing envelope */
	if( envelopeInfoPtr->usage != ACTION_SIGN )
		return( FALSE );

	/* When we're de-enveloping a signed envelope we can have standalone
	   hash actions before we get to the signature data and add post-
	   actions */
	if( TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_ISDEENVELOPE ) && \
		actionListPtr->action == ACTION_HASH )
		{
		LOOP_MED_CHECKINC( actionListPtr != NULL,
						   actionListPtr = DATAPTR_GET( actionListPtr->next ) )
			{
			REQUIRES_B( sanityCheckActionList( actionListPtr ) );

			if( actionListPtr->action != ACTION_HASH )
				return( FALSE );
			}
		ENSURES_B( LOOP_BOUND_OK );

		return( TRUE );
		}

	/* Everything else is an error */
	return( FALSE );
	}
#endif /* USE_ENVELOPES */
