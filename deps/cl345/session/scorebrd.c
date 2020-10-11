/****************************************************************************
*																			*
*							cryptlib Session Scoreboard						*
*						Copyright Peter Gutmann 1998-2016					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "scorebrd.h"
  #include "scorebrd_int.h"
#else
  #include "crypt.h"
  #include "session/scorebrd.h"
  #include "session/scorebrd_int.h"
#endif /* Compiler-specific includes */

#ifdef USE_SSL

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#ifndef CONFIG_CONSERVE_MEMORY_EXTRA

/* Sanity-check the scoreboard state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckScoreboard( const SCOREBOARD_INDEX_INFO *scoreboardIndexInfo )
	{
	assert( isReadPtr( scoreboardIndexInfo, 
					   sizeof( SCOREBOARD_INDEX_INFO ) ) );

	/* Make sure that the general state is in order */
	if( scoreboardIndexInfo->lastEntry < 0 || \
		scoreboardIndexInfo->lastEntry > SCOREBOARD_ENTRIES )
		{
		DEBUG_PUTS(( "sanityCheckScoreboard: Scoreboard last entry" ));
		return( FALSE );
		}
	if( scoreboardIndexInfo->uniqueID < 0 )
		{
		DEBUG_PUTS(( "sanityCheckScoreboard: Scoreboard unique ID" ));
		return( FALSE );
		}

	/* Check safe pointers */
	if( !DATAPTR_ISVALID( scoreboardIndexInfo->data ) )
		{
		DEBUG_PUTS(( "sanityCheckScoreboard: Safe pointers" ));
		return( FALSE );
		}

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckScoreboardEntry( const SCOREBOARD_INDEX *scoreboardIndex )
	{
	void *dataPtr;

	assert( isReadPtr( scoreboardIndex, 
					   sizeof( SCOREBOARD_INDEX ) ) );

	/* Check lookup information */
	if( scoreboardIndex->sessionIDlength <= 0 || \
		scoreboardIndex->sessionIDlength > SCOREBOARD_KEY_SIZE )
		{
		DEBUG_PUTS(( "sanityCheckScoreboardEntry: Lookup information" ));
		return( FALSE );
		}

	/* Check scoreboard data */
	if( scoreboardIndex->dataLength < 1 || \
		scoreboardIndex->dataLength > SCOREBOARD_DATA_SIZE )
		{
		DEBUG_PUTS(( "sanityCheckScoreboardEntry: Data size" ));
		return( FALSE );
		}
	dataPtr = DATAPTR_GET( scoreboardIndex->data );
	if( dataPtr == NULL || \
		checksumData( dataPtr, scoreboardIndex->dataLength ) != \
					  scoreboardIndex->dataChecksum )
		{
		DEBUG_PUTS(( "sanityCheckScoreboardEntry: Data" ));
		return( FALSE );
		}

	/* Check miscellaneous information */
	if( ( scoreboardIndex->isServerData != TRUE && \
		  scoreboardIndex->isServerData != FALSE ) || \
		scoreboardIndex->uniqueID < 0 || \
		scoreboardIndex->uniqueID > INT_MAX - 10 )
		{
		DEBUG_PUTS(( "sanityCheckScoreboardEntry: Miscellaneous information" ));
		return( FALSE );
		}

	return( TRUE );
	}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

/* Check whether a scoreboard entry is empty.  This checks a number of 
   values in order to deal with false positives caused by corruption of a
   single value, if this check declares the value non-empty then it has to
   pass a sanityCheckScoreboardEntry() check immediately afterwards */

static BOOLEAN isEmptyEntry( const SCOREBOARD_INDEX *scoreboardEntryPtr )
	{
	assert( isReadPtr( scoreboardEntryPtr, sizeof( SCOREBOARD_INDEX ) ) );
	
	if( scoreboardEntryPtr->sessionCheckValue == 0 && \
		scoreboardEntryPtr->fqdnCheckValue == 0 && \
		scoreboardEntryPtr->sessionIDlength == 0 && \
		scoreboardEntryPtr->dataLength == 0 && \
		scoreboardEntryPtr->timeStamp <= MIN_TIME_VALUE )
		return( TRUE );
	
	return( FALSE );
	}

/* Clear a scoreboard entry */

STDC_NONNULL_ARG( ( 1 ) ) \
static void clearScoreboardEntry( SCOREBOARD_INDEX *scoreboardEntryPtr )
	{
	void *savedDataPtr;

	assert( isWritePtr( scoreboardEntryPtr, \
						sizeof( SCOREBOARD_INDEX ) ) );

	savedDataPtr = DATAPTR_GET( scoreboardEntryPtr->data );
	REQUIRES_V( savedDataPtr != NULL );
	zeroise( savedDataPtr, SCOREBOARD_DATA_SIZE );
	memset( scoreboardEntryPtr, 0, sizeof( SCOREBOARD_INDEX ) );
	DATAPTR_SET( scoreboardEntryPtr->data, savedDataPtr );
	
	ENSURES_V( isEmptyEntry( scoreboardEntryPtr ) );
	}

/* Add a scoreboard entry */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 8 ) ) \
static int addEntryData( INOUT SCOREBOARD_INDEX *scoreboardEntryPtr, 
						 IN_INT_Z const int keyCheckValue,
						 IN_BUFFER( keyLength ) const void *key, 
						 IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
						 IN_INT_Z const int altKeyCheckValue,
						 IN_BUFFER_OPT( altKeyLength ) const void *altKey, 
						 IN_LENGTH_SHORT_Z const int altKeyLength, 
						 const SCOREBOARD_INFO *scoreboardInfo,
						 const time_t currentTime )
	{
	void *dataPtr;
	int status;

	assert( isWritePtr( scoreboardEntryPtr, sizeof( SCOREBOARD_INDEX ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( ( altKey == NULL && altKeyLength == 0 ) || \
			isReadPtrDynamic( altKey, altKeyLength ) );
	assert( isReadPtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );

	REQUIRES( keyCheckValue >= 0 );
	REQUIRES( keyLength >= SCOREBOARD_KEY_MIN && \
			  keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( ( altKey == NULL && altKeyLength == 0 && \
				altKeyCheckValue == 0 ) || \
			  ( altKey != NULL && \
				altKeyLength >= SCOREBOARD_KEY_MIN && \
				altKeyLength < MAX_INTLENGTH_SHORT && \
				altKeyCheckValue >= 0 ) );
	REQUIRES( currentTime > MIN_TIME_VALUE );

	/* Clear the existing data in the entry */
	clearScoreboardEntry( scoreboardEntryPtr );
	dataPtr = DATAPTR_GET( scoreboardEntryPtr->data );
	ENSURES( dataPtr != NULL );

	/* Copy across the key and value (Amicitiae nostrae memoriam spero 
	   sempiternam fore - Cicero) */
	scoreboardEntryPtr->sessionCheckValue = keyCheckValue;
	hashData( scoreboardEntryPtr->sessionHash, HASH_DATA_SIZE, 
			  key, keyLength );
	if( altKey != NULL )
		{
		scoreboardEntryPtr->fqdnCheckValue = altKeyCheckValue;
		hashData( scoreboardEntryPtr->fqdnHash, HASH_DATA_SIZE, 
				  altKey, altKeyLength );
		}
	status = attributeCopyParams( scoreboardEntryPtr->sessionID, 
								  SCOREBOARD_KEY_SIZE, 
								  &scoreboardEntryPtr->sessionIDlength,
								  key, keyLength );
	ENSURES( cryptStatusOK( status ) );
	status = attributeCopyParams( dataPtr, SCOREBOARD_DATA_SIZE, 
								  &scoreboardEntryPtr->dataLength,
								  scoreboardInfo->data, 
								  scoreboardInfo->dataSize );
	ENSURES( cryptStatusOK( status ) );
	scoreboardEntryPtr->dataChecksum = \
				checksumData( dataPtr, scoreboardEntryPtr->dataLength );
	scoreboardEntryPtr->metaData = scoreboardInfo->metaData;
	scoreboardEntryPtr->isServerData = ( altKey == NULL ) ? TRUE : FALSE;
	scoreboardEntryPtr->timeStamp = currentTime;

	ENSURES( sanityCheckScoreboardEntry( scoreboardEntryPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Scoreboard Management Functions						*
*																			*
****************************************************************************/

/* Find an entry, returning its position in the scoreboard.  This function 
   currently uses a straightforward linear search with entries clustered 
   towards the start of the scoreboard.  Although this may seem somewhat 
   suboptimal, since cryptlib isn't running as a high-performance web server 
   the scoreboard will rarely contain more than a handful of entries (if 
   any).  In any case a quick scan through a small number of integers is 
   probably still faster than the complex in-memory database lookup schemes 
   used by many servers, and is also required to handle things like 
   scoreboard LRU management */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 6 ) ) \
static int findEntry( INOUT SCOREBOARD_INDEX_INFO *scoreboardIndexInfo,
					  IN_ENUM( SCOREBOARD_KEY ) \
							const SCOREBOARD_KEY_TYPE keyType,
					  IN_BUFFER( keyLength ) const void *key, 
					  IN_LENGTH_SHORT_MIN( 2 ) const int keyLength, 
					  const time_t currentTime, 
					  OUT_INT_SHORT_Z int *position )
	{
	BYTE hashValue[ HASH_DATA_SIZE + 8 ];
	const BOOLEAN keyIsSessionID = \
		( keyType == SCOREBOARD_KEY_SESSIONID_CLI || \
		  keyType == SCOREBOARD_KEY_SESSIONID_SVR ) ? TRUE : FALSE;
	const BOOLEAN isServerMatch = \
		( keyType == SCOREBOARD_KEY_SESSIONID_SVR ) ? TRUE : FALSE;
	BOOLEAN dataHashed = FALSE;
	time_t oldestTime = currentTime;
	const int checkValue = checksumData( key, keyLength );
	int nextFreeEntry = CRYPT_ERROR, lastUsedEntry = 0, oldestEntry = 0;
	int matchPosition = CRYPT_ERROR, i, LOOP_ITERATOR;

	assert( isWritePtr( scoreboardIndexInfo, 
						sizeof( SCOREBOARD_INDEX_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( isWritePtr( position, sizeof( int ) ) );

	REQUIRES( isEnumRange( keyType, SCOREBOARD_KEY ) );
	REQUIRES( keyLength >= SCOREBOARD_KEY_MIN && \
			  keyLength < MAX_INTLENGTH_SHORT);
	REQUIRES( currentTime > MIN_TIME_VALUE );

	/* Clear return value */
	*position = CRYPT_ERROR;

	/* Scan the scoreboard expiring old entries, looking for a match 
	   (indicated by matchPosition), and keeping a record of the oldest 
	   entry (recorded by oldestEntry) in case we need to expire an entry to
	   make room for a new one */
	LOOP_MAX( i = 0, i < scoreboardIndexInfo->lastEntry, i++ )
		{
		SCOREBOARD_INDEX *scoreboardEntryPtr = &scoreboardIndexInfo->index[ i ];

		/* If this entry has expired, delete it */
		if( scoreboardEntryPtr->timeStamp + SCOREBOARD_TIMEOUT < currentTime )
			clearScoreboardEntry( scoreboardEntryPtr );

		/* Check for a free entry and the oldest non-free entry.  We could
		   perform an early-out once we find a free entry but this would
		   prevent any following expired entries from being deleted */
		if( isEmptyEntry( scoreboardEntryPtr ) )
			{
			/* We've found a free entry, remember it for future use if
			   required and continue */
			if( nextFreeEntry == CRYPT_ERROR )
				nextFreeEntry = i;
			continue;
			}
		REQUIRES( sanityCheckScoreboardEntry( scoreboardEntryPtr ) );
		lastUsedEntry = i;
		if( scoreboardEntryPtr->timeStamp < oldestTime )
			{
			/* We've found an older entry than the current oldest entry,
			   remember it */
			oldestTime = scoreboardEntryPtr->timeStamp;
			oldestEntry = i;
			}

		/* If we've already found a match then we're just scanning for LRU
		   purposes and we don't need to go any further */
		if( matchPosition != CRYPT_ERROR )
			continue;

		/* Make sure that this entry is appropriate for the match type that
		   we're performing */
		if( scoreboardEntryPtr->isServerData != isServerMatch )
			continue;

		/* Perform a quick check using a checksum of the name to weed out
		   most entries */
		if( ( keyIsSessionID && \
			  scoreboardEntryPtr->sessionCheckValue == checkValue ) || \
			( !keyIsSessionID && \
			  scoreboardEntryPtr->fqdnCheckValue == checkValue ) )
			{
			void *hashPtr = keyIsSessionID ? \
								scoreboardEntryPtr->sessionHash : \
								scoreboardEntryPtr->fqdnHash;

			if( !dataHashed )
				{
				hashData( hashValue, HASH_DATA_SIZE, key, keyLength );
				dataHashed = TRUE;
				}
			if( !memcmp( hashPtr, hashValue, HASH_DATA_SIZE ) )
				{
				/* Remember the match position.  We can't immediately exit 
				   at this point because we still need to look for the last 
				   used entry and potentually shrink the scoreboard-used 
				   size */
				matchPosition = i;
				}
			}
		}
	ENSURES( i < FAILSAFE_ITERATIONS_MAX );

	/* If the total number of entries has shrunk due to old entries expiring,
	   reduce the overall scoreboard-used size */
	if( lastUsedEntry + 1 < scoreboardIndexInfo->lastEntry )
		scoreboardIndexInfo->lastEntry = lastUsedEntry + 1;

	/* If we've found a match, we're done */
	if( matchPosition >= 0 )
		{
		*position = matchPosition;
		return( CRYPT_OK );
		}

	/* The entry wasn't found, return the location where we can add a new 
	   entry */
	if( nextFreeEntry >= 0 )
		{
		/* We've found a freed-up existing position (which will be before 
		   any remaining free entries), add the new entry there */
		*position = nextFreeEntry;
		}
	else
		{
		/* If there are still free positions in the scoreboard, use the next
		   available one */
		if( scoreboardIndexInfo->lastEntry < SCOREBOARD_ENTRIES )
			*position = scoreboardIndexInfo->lastEntry;
		else
			{
			/* There are no free positions, overwrite the oldest entry */
			*position = oldestEntry;
			}
		}
	ENSURES( *position >= 0 && *position < SCOREBOARD_ENTRIES );

	/* Let the caller know that this is an indication of a free position 
	   rather than a match */
	return( OK_SPECIAL );
	}

/* Add an entry to the scoreboard.  The strategy for updating entries can 
   get quite complicated.  In the following the server-side cases are 
   denoted with -S and the client-side cases with -C:

	  Case	|	key		|	altKey	|	Action
			| (sessID)	|  (FQDN)	|
	--------+-----------+-----------+---------------------------------------
	  1-S	|  no match	|	absent	| Add entry
	--------+-----------+-----------+---------------------------------------
	  2-S	|	match	|	absent	| Add-special (see below)
	--------+-----------+-----------+---------------------------------------
	  3-C	|  no match	|  no match	| Add entry
	--------+-----------+-----------+---------------------------------------
	  4-C	|  no match	|	match	| Replace existing match.  This situation
			|			|			| has presumably occurred because we've
			|			|			| re-connected to a server with a full
			|			|			| handshake and were allocated a new 
			|			|			| session ID.
	--------+-----------+-----------+---------------------------------------
	  5-C	|	match	|  no match	| Clear entry.  This situation shouldn't
			|			|			| occur, it means that we've somehow 
			|			|			| acquired a session ID with a different
			|			|			| server.
	--------+-----------+-----------+---------------------------------------
	  6-C	|	match	|	match	| Add-special (see below)
	--------+-----------+-----------+---------------------------------------
	  7-C	|  match-1	|  match-2	| Match, but at different locations, 
			|			|			| clear both entries (variant of case
			|			|			| 5-C).

   Add-special is a conditional add, if the data value that we're trying to 
   add corresponds to the existing one (and the search keys matched as well)
   then it's an update of an existing entry and we update its timestamp.  If
   the data value doesn't match (but the search keys did) then something 
   funny is going on and we clear the existing entry.  If we simply ignore 
   the add attempt then it'll appear to the caller that we've added the new 
   value when in fact we've retained the existing one.  If on the other hand 
   we overwrite the old value with the new one then it'll allow an attacker 
   to replace existing scoreboard contents with attacker-controlled ones.

   In theory not every case listed above can occur because information is 
   only added for new (non-resumed) sessions, so for example case 2-S 
   wouldn't occur because if there's already a match for the session ID then 
   it'd result in a resumed session and so the information wouldn't be added 
   a second time.  However there are situations in which these oddball cases 
   can occur, in general not for servers (even with two threads racing each 
   other for scoreboard access) because it'd require that the cryptlib 
   server allocate the same session ID twice, but it can occur for clients 
   if (case 5-C) two servers allocate us the same session ID or (case 4-C) 
   two threads simultaneously connect to the same server, with FQDNs the 
   same but session IDs different */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 6, 7 ) ) \
static int addEntry( INOUT SCOREBOARD_INDEX_INFO *scoreboardIndexInfo, 
					 IN_BUFFER( keyLength ) const void *key, 
					 IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
					 IN_BUFFER_OPT( altKeyLength ) const void *altKey, 
					 IN_LENGTH_SHORT_Z const int altKeyLength, 
					 const SCOREBOARD_INFO *scoreboardInfo,
					 OUT int *uniqueID )
	{
	SCOREBOARD_INDEX *scoreboardIndex;
	SCOREBOARD_INDEX *scoreboardEntryPtr = NULL;
	const time_t currentTime = getTime();
	const BOOLEAN isClient = ( altKey != NULL ) ? TRUE : FALSE;
	int checkValue, altCheckValue = 0, altPosition DUMMY_INIT;
	int position, altStatus = CRYPT_ERROR, status;

	assert( isWritePtr( scoreboardIndexInfo, 
						sizeof( SCOREBOARD_INDEX_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( ( altKey == NULL && altKeyLength == 0 ) || \
			isReadPtrDynamic( altKey, altKeyLength ) );
	assert( isReadPtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	assert( isWritePtr( uniqueID, sizeof( int ) ) );

	REQUIRES( keyLength >= SCOREBOARD_KEY_MIN && \
			  keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( ( altKey == NULL && altKeyLength == 0 ) || \
			  ( altKey != NULL && \
				altKeyLength >= SCOREBOARD_KEY_MIN && \
				altKeyLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return value */
	*uniqueID = CRYPT_ERROR;

	status = checkValue = checksumData( key, keyLength );
	if( cryptStatusError( status ) )
		return( status );
	
	/* If there's something wrong with the time then we can't perform (time-
	   based) scoreboard management */
	if( currentTime <= MIN_TIME_VALUE )
		return( CRYPT_ERROR_NOTFOUND );

	/* Try and find this entry in the scoreboard */
	status = findEntry( scoreboardIndexInfo, isClient ? \
							SCOREBOARD_KEY_SESSIONID_CLI : \
							SCOREBOARD_KEY_SESSIONID_SVR, 
						key, keyLength, currentTime, &position );
	if( cryptStatusError( status ) && status != OK_SPECIAL )
		return( status );
	ENSURES( position >= 0 && position < SCOREBOARD_ENTRIES );
	if( altKey != NULL )
		{
		altCheckValue = checksumData( altKey, altKeyLength );
		if( cryptStatusError( altCheckValue ) )
			return( altCheckValue );
		altStatus = findEntry( scoreboardIndexInfo, SCOREBOARD_KEY_FQDN, 
							   altKey, altKeyLength, currentTime, 
							   &altPosition );
		if( cryptStatusError( altStatus ) && altStatus != OK_SPECIAL )
			return( altStatus );
		ENSURES( altPosition >= 0 && \
				 altPosition < SCOREBOARD_ENTRIES );
		}
	ENSURES( cryptStatusOK( status ) || status == OK_SPECIAL );
	ENSURES( altKey == NULL || \
			 cryptStatusOK( altStatus ) || altStatus == OK_SPECIAL );
	scoreboardIndex = scoreboardIndexInfo->index;

	/* We've done the match-checking, now we have to act on the results.  
	   The different result-value settings and corresponding actions are:

		  Case	|		sessID		|		FQDN		| Action
		--------+-------------------+-------------------+-----------------
			1	|  s = MT, pos = x	|		!altK		| Add at x
		--------+-------------------+-------------------+-----------------
			2	|  s = OK, pos = x	|		!altK		| Add-special at x
		--------+-------------------+-------------------+-----------------
			3	|  s = MT, pos = x	| aS = MT, aPos = x	| Add at x
		--------+-------------------+-------------------+-----------------
			4	|  s = MT, pos = x	| aS = OK, aPos = y	| Replace at y
		--------+-------------------+-------------------+-----------------
			5	|  s = OK, pos = x	| aS = MT, aPos = y	| Clear at x
		--------+-------------------+-------------------+-----------------
			6	|  s = OK, pos = x	| aS = OK, aPos = x	| Add-special at x
		--------+-------------------+-------------------+-----------------
			7	|  s = OK, pos = x	| aS = OK, aPos = y	| Clear at x and y */
	if( cryptStatusOK( status ) )
		{
		void *dataPtr;

		/* We matched on the main key (session ID), handle cases 2-S, 5-C, 
		   6-C and 7-C */
		if( altKey != NULL && position != altPosition )
			{
			/* Cases 5-C + 7-C, clear */
			clearScoreboardEntry( &scoreboardIndex[ position ] );
			return( CRYPT_ERROR_NOTFOUND );
			}

		/* Cases 2-S + 6-C, add-special */
		ENSURES( altKey == NULL || ( cryptStatusOK( altStatus ) && \
									 position == altPosition ) );
		scoreboardEntryPtr = &scoreboardIndex[ position ];
		REQUIRES( sanityCheckScoreboardEntry( scoreboardEntryPtr ) );
		dataPtr = DATAPTR_GET( scoreboardEntryPtr->data );
		REQUIRES( dataPtr != NULL );
		if( scoreboardEntryPtr->dataLength != scoreboardInfo->dataSize || \
			memcmp( dataPtr, scoreboardInfo->data, scoreboardInfo->dataSize ) )
			{
			/* The search keys match but the data doesn't, something funny 
			   is going on */
			clearScoreboardEntry( &scoreboardIndex[ position ] );
			assert( DEBUG_WARN );
			return( CRYPT_ERROR_NOTFOUND );
			}
		scoreboardEntryPtr->timeStamp = currentTime;

		return( CRYPT_OK );
		}
	REQUIRES( status == OK_SPECIAL );

	/* We didn't match on the main key (session ID), check for a match on 
	   the alt.key (FQDN) */
	if( cryptStatusOK( altStatus ) )
		{
		/* Case 4-C, add at location 'altPosition' */
		ENSURES( position != altPosition );
		scoreboardEntryPtr = &scoreboardIndex[ altPosition ];
		}
	else
		{
		/* Cases 1-S + 3-C, add at location 'position' */
		ENSURES( altKey == NULL || \
				 ( altStatus == OK_SPECIAL && position == altPosition ) )
		scoreboardEntryPtr = &scoreboardIndex[ position ];
		}

	/* It's either an empty entry being added or an existing entry being 
	   updated */
	REQUIRES( isEmptyEntry( scoreboardEntryPtr ) || \
			  sanityCheckScoreboardEntry( scoreboardEntryPtr ) );

	/* Add the data to the new scoreboard entry position */
	status = addEntryData( scoreboardEntryPtr, checkValue, key, keyLength, 
						   altCheckValue, altKey, altKeyLength, 
						   scoreboardInfo, currentTime );
	if( cryptStatusError( status ) )
		{
		clearScoreboardEntry( scoreboardEntryPtr );
		return( status );
		}
	if( scoreboardIndexInfo->uniqueID >= INT_MAX - 100 )
		{
		/* If we're about to wrap, reset the uniqueID value to the initial 
		   value.  This can happen on 16-bit systems */
		scoreboardIndexInfo->uniqueID = 0;
		}
	*uniqueID = scoreboardEntryPtr->uniqueID = \
				scoreboardIndexInfo->uniqueID++;

	/* If we've used a new entry, update the position-used index */
	if( position >= scoreboardIndexInfo->lastEntry )
		scoreboardIndexInfo->lastEntry = position + 1;

	return( CRYPT_OK );
	}

/* Look up data in the scoreboard */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5, 6 ) ) \
static int lookupScoreboard( INOUT SCOREBOARD_INDEX_INFO *scoreboardIndexInfo,
							 IN_ENUM( SCOREBOARD_KEY ) \
								const SCOREBOARD_KEY_TYPE keyType,
							 IN_BUFFER( keyLength ) const void *key, 
							 IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
						     OUT SCOREBOARD_INFO *scoreboardInfo,
							 OUT_INT_Z int *uniqueID )
	{
	SCOREBOARD_INDEX *scoreboardEntryPtr;
	const time_t currentTime = getTime();
	int position, status;

	assert( isWritePtr( scoreboardIndexInfo, 
						sizeof( SCOREBOARD_INDEX_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	assert( isWritePtr( uniqueID, sizeof( int ) ) );

	REQUIRES( isEnumRange( keyType, SCOREBOARD_KEY ) );
	REQUIRES( keyLength >= SCOREBOARD_KEY_MIN && \
			  keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( sanityCheckScoreboard( scoreboardIndexInfo ) );

	/* Clear return values */
	memset( scoreboardInfo, 0, sizeof( SCOREBOARD_INFO ) );
	*uniqueID = CRYPT_ERROR;

	/* If there's something wrong with the time then we can't perform (time-
	   based) scoreboard management */
	if( currentTime <= MIN_TIME_VALUE )
		return( CRYPT_ERROR_NOTFOUND );

	/* Try and find this entry in the scoreboard */
	status = findEntry( scoreboardIndexInfo, keyType, key, keyLength, 
						currentTime, &position );
	if( cryptStatusError( status ) )
		{
		/* An OK_SPECIAL status means that the search found an unused entry 
		   position but not a matching entry (this is used by addEntry()), 
		   anything else is an error */
		return( ( status == OK_SPECIAL ) ? CRYPT_ERROR_NOTFOUND : status );
		}
	ENSURES( position >= 0 && position < SCOREBOARD_ENTRIES );
	scoreboardEntryPtr = &scoreboardIndexInfo->index[ position ];
	REQUIRES( sanityCheckScoreboardEntry( scoreboardEntryPtr ) );

	/* We've found a match, return a pointer to the data (which avoids 
	   copying it out of secure memory) and the unique ID for it */
	scoreboardInfo->key = scoreboardEntryPtr->sessionID;
	scoreboardInfo->keySize = scoreboardEntryPtr->sessionIDlength;
	scoreboardInfo->data = DATAPTR_GET( scoreboardEntryPtr->data );
	ENSURES( scoreboardInfo->data != NULL );
	scoreboardInfo->dataSize = scoreboardEntryPtr->dataLength;
	scoreboardInfo->metaData = scoreboardEntryPtr->metaData;
	*uniqueID = scoreboardEntryPtr->uniqueID;

	/* Update the entry's last-access date */
	scoreboardEntryPtr->timeStamp = currentTime;

	ENSURES( sanityCheckScoreboard( scoreboardIndexInfo ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Scoreboard Access Functions						*
*																			*
****************************************************************************/

/* Add and delete entries to/from the scoreboard.  These are just wrappers
   for the local scoreboard-access function, for use by external code */

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int lookupScoreboardEntry( INOUT TYPECAST( SCOREBOARD_INDEX_INFO * ) \
								void *scoreboardIndexInfoPtr,
						   IN_ENUM( SCOREBOARD_KEY ) \
								const SCOREBOARD_KEY_TYPE keyType,
						   IN_BUFFER( keyLength ) const void *key, 
						   IN_LENGTH_SHORT_MIN( 2 ) const int keyLength, 
						   OUT SCOREBOARD_INFO *scoreboardInfo )
	{
	SCOREBOARD_INDEX_INFO *scoreboardIndexInfo = scoreboardIndexInfoPtr;
	int uniqueID, status;

	assert( isWritePtr( scoreboardIndexInfo, 
						sizeof( SCOREBOARD_INDEX_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( isWritePtr( scoreboardInfo, 
						sizeof( SCOREBOARD_INFO ) ) );

	REQUIRES( sanityCheckScoreboard( scoreboardIndexInfo ) );
	REQUIRES( isEnumRange( keyType, SCOREBOARD_KEY ) );
	REQUIRES( keyLength >= SCOREBOARD_KEY_MIN && \
			  keyLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( scoreboardInfo, 0, sizeof( SCOREBOARD_INFO ) );

	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		return( status );
	status = lookupScoreboard( scoreboardIndexInfo, keyType, key, keyLength, 
							   scoreboardInfo, &uniqueID );
	krnlExitMutex( MUTEX_SCOREBOARD );
	return( cryptStatusError( status ) ? status : uniqueID );
	}

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int addScoreboardEntry( INOUT void *scoreboardIndexInfoPtr,
						IN_BUFFER( keyLength ) const void *key, 
						IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
						const SCOREBOARD_INFO *scoreboardInfo )
	{
	SCOREBOARD_INDEX_INFO *scoreboardIndexInfo = scoreboardIndexInfoPtr;
	int uniqueID, status;

	assert( isWritePtr( scoreboardIndexInfo, 
						sizeof( SCOREBOARD_INDEX_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( isReadPtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );

	REQUIRES( sanityCheckScoreboard( scoreboardIndexInfo ) );
	REQUIRES( keyLength >= SCOREBOARD_KEY_MIN && \
			  keyLength < MAX_INTLENGTH_SHORT );

	/* Add the entry to the scoreboard */
	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		return( status );
	status = addEntry( scoreboardIndexInfo, key, keyLength, NULL, 0,
					   scoreboardInfo, &uniqueID );
	krnlExitMutex( MUTEX_SCOREBOARD );

	ENSURES( sanityCheckScoreboard( scoreboardIndexInfo ) );

	return( cryptStatusError( status ) ? status : uniqueID );
	}

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1, 2, 4, 6 ) ) \
int addScoreboardEntryEx( INOUT void *scoreboardIndexInfoPtr,
						  IN_BUFFER( keyLength ) const void *key, 
						  IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
						  IN_BUFFER( keyLength ) const void *altKey, 
						  IN_LENGTH_SHORT_MIN( 2 ) const int altKeyLength, 
						  const SCOREBOARD_INFO *scoreboardInfo )
	{
	SCOREBOARD_INDEX_INFO *scoreboardIndexInfo = scoreboardIndexInfoPtr;
	int uniqueID, status;

	assert( isWritePtr( scoreboardIndexInfo, 
						sizeof( SCOREBOARD_INDEX_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( isReadPtrDynamic( altKey, altKeyLength ) );
	assert( isReadPtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );

	REQUIRES( sanityCheckScoreboard( scoreboardIndexInfo ) );
	REQUIRES( keyLength >= SCOREBOARD_KEY_MIN && \
			  keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( altKeyLength >= SCOREBOARD_KEY_MIN && \
			  altKeyLength < MAX_INTLENGTH_SHORT );

	/* Add the entry to the scoreboard */
	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		return( status );
	status = addEntry( scoreboardIndexInfo, key, keyLength, altKey, 
					   altKeyLength, scoreboardInfo, &uniqueID );
	krnlExitMutex( MUTEX_SCOREBOARD );

	ENSURES( sanityCheckScoreboard( scoreboardIndexInfo ) );

	return( cryptStatusError( status ) ? status : uniqueID );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteScoreboardEntry( INOUT TYPECAST( SCOREBOARD_INDEX_INFO * ) \
								void *scoreboardIndexInfoPtr, 
							IN_INT_Z const int uniqueID )
	{
	SCOREBOARD_INDEX_INFO *scoreboardIndexInfo = scoreboardIndexInfoPtr;
	int lastUsedEntry = -1, i, status, LOOP_ITERATOR;

	assert( isWritePtr( scoreboardIndexInfo, 
						sizeof( SCOREBOARD_INDEX_INFO ) ) );
	
	REQUIRES_V( sanityCheckScoreboard( scoreboardIndexInfo ) );
	REQUIRES_V( isIntegerRange( uniqueID ) );

	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		return;

	/* Search the scoreboard for the entry with the given ID */
	LOOP_MAX( i = 0, i < scoreboardIndexInfo->lastEntry, i++ )
		{
		SCOREBOARD_INDEX *scoreboardEntryPtr = &scoreboardIndexInfo->index[ i ];

		/* If it's an empty entry (due to it having expired or being 
		   deleted), skip it and continue */
		if( isEmptyEntry( scoreboardEntryPtr ) )
			continue;

		REQUIRES_V( sanityCheckScoreboardEntry( scoreboardEntryPtr ) );

		/* If we've found the entry that we're after, clear it and exit */
		if( scoreboardEntryPtr->uniqueID == uniqueID )
			{
			clearScoreboardEntry( scoreboardEntryPtr );
			continue;
			}

		/* Remember how far we got */
		lastUsedEntry = i;
		}
	ENSURES_KRNLMUTEX_V( LOOP_BOUND_OK, MUTEX_SCOREBOARD );

	/* Since we may have deleted entries at the end of the scoreboard, we 
	   can reduce the lastEntry value to the highest remaining entry */
	scoreboardIndexInfo->lastEntry = lastUsedEntry + 1;

	krnlExitMutex( MUTEX_SCOREBOARD );
	}

/****************************************************************************
*																			*
*							Scoreboard Init/Shutdown						*
*																			*
****************************************************************************/

/* Perform a self-test of the scoreboard functions */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN selfTest( INOUT SCOREBOARD_INDEX_INFO *scoreboardIndexInfo )
	{
	SCOREBOARD_INFO scoreboardInfo;
	int uniqueID1, uniqueID2, foundUniqueID, status;

	assert( isWritePtr( scoreboardIndexInfo, 
						sizeof( SCOREBOARD_INDEX_INFO ) ) );

	/* Add two entries to the scoreboard */
	memset( &scoreboardInfo, 0, sizeof( SCOREBOARD_INFO ) );
	scoreboardInfo.data = "test value 1";
	scoreboardInfo.dataSize = 12;
	status = uniqueID1 = \
		addScoreboardEntry( scoreboardIndexInfo, "test key 1", 10,
							&scoreboardInfo );
	if( cryptStatusError( status ) )
		return( FALSE );
	scoreboardInfo.data = "test value 2";
	scoreboardInfo.dataSize = 12;
	status = uniqueID2 = \
		addScoreboardEntry( scoreboardIndexInfo, "test key 2", 10,
							&scoreboardInfo );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Read them back and delete them */
	status = foundUniqueID = \
		lookupScoreboardEntry( scoreboardIndexInfo, SCOREBOARD_KEY_SESSIONID_SVR, 
							   "test key 1", 10, &scoreboardInfo );
	if( cryptStatusError( status ) )
		return( FALSE );
	if( foundUniqueID != uniqueID1 || \
		scoreboardInfo.keySize != 10 || \
		memcmp( scoreboardInfo.key, "test key 1", 10 ) || \
		scoreboardInfo.dataSize != 12 || \
		memcmp( scoreboardInfo.data, "test value 1", 12 ) )
		{
		return( FALSE );
		}
	deleteScoreboardEntry( scoreboardIndexInfo, uniqueID1 );
	foundUniqueID = lookupScoreboardEntry( scoreboardIndexInfo, 
							SCOREBOARD_KEY_SESSIONID_SVR, "test key 1", 10,
							&scoreboardInfo );
	if( foundUniqueID != CRYPT_ERROR_NOTFOUND )
		return( FALSE );
	deleteScoreboardEntry( scoreboardIndexInfo, uniqueID2 );
	if( scoreboardIndexInfo->lastEntry != 0 || \
		scoreboardIndexInfo->uniqueID != 2 )
		return( FALSE );

#ifndef NDEBUG
	{
	char dataString[ 16 + 8 ];
	int i, LOOP_ITERATOR;

	memset( &scoreboardInfo, 0, sizeof( SCOREBOARD_INFO ) );
	scoreboardInfo.data = dataString;
	scoreboardInfo.dataSize = 4;
	LOOP_LARGE( i = 0, i < SCOREBOARD_ENTRIES + 10, i++ )
		{
		sprintf_s( dataString, 16, "%04X", i );

		status = \
			addScoreboardEntry( scoreboardIndexInfo, dataString, 4,
								&scoreboardInfo );
		if( cryptStatusError( status ) )
			return( FALSE );
		}
	ENSURES( LOOP_BOUND_OK );
	}
#endif /* NDEBUG */

	return( TRUE );
	}

/* Initialise and shut down the scoreboard */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initScoreboard( INOUT TYPECAST( SCOREBOARD_INDEX_INFO * ) \
						void *scoreboardIndexInfoPtr )
	{
	SCOREBOARD_INDEX_INFO *scoreboardIndexInfo = scoreboardIndexInfoPtr;
	SCOREBOARD_DATA *scoreboardData;
	int i, status, LOOP_ITERATOR;

	assert( isWritePtr( scoreboardIndexInfo, 
						sizeof( SCOREBOARD_INDEX_INFO ) ) );
	
	/* Allocate memory for the scoreboard, which we can do before acquiring 
	   the scoreboard mutex */
	status = krnlMemalloc( ( void ** ) &scoreboardData, \
						   SCOREBOARD_ENTRIES * sizeof( SCOREBOARD_DATA ) );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Couldn't allocate %d bytes secure memory for "
					 "scoreboard data",
					 SCOREBOARD_ENTRIES * sizeof( SCOREBOARD_DATA ) ));
		return( status );
		}
	memset( scoreboardData, 0, \
			SCOREBOARD_ENTRIES * sizeof( SCOREBOARD_DATA ) );

	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		{
		status = krnlMemfree( ( void ** ) &scoreboardData );
		return( status );
		}

	/* Initialise the scoreboard */
	memset( scoreboardIndexInfo, 0, sizeof( SCOREBOARD_INDEX_INFO ) );
	LOOP_MAX( i = 0, i < SCOREBOARD_ENTRIES, i++ )
		{
		DATAPTR_SET( scoreboardIndexInfo->index[ i ].data, 
					 &scoreboardData[ i ] );
		}
	ENSURES_KRNLMUTEX( LOOP_BOUND_OK, MUTEX_SCOREBOARD );
	DATAPTR_SET( scoreboardIndexInfo->data, scoreboardData );

	/* Make sure that everything's working as intended */
#ifndef CONFIG_FUZZ
	if( !selfTest( scoreboardIndexInfo ) )
		{
		status = krnlMemfree( ( void ** ) &scoreboardData );
		ENSURES_KRNLMUTEX( cryptStatusOK( status ), MUTEX_SCOREBOARD );
		memset( scoreboardIndexInfo, 0, sizeof( SCOREBOARD_INDEX_INFO ) );
		DEBUG_DIAG(( "Couldn't initialise scoreboard" ));

		krnlExitMutex( MUTEX_SCOREBOARD );
		retIntError();
		}
#endif /* !CONFIG_FUZZ */

	krnlExitMutex( MUTEX_SCOREBOARD );

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void endScoreboard( INOUT TYPECAST( SCOREBOARD_INDEX_INFO * ) \
						void *scoreboardIndexInfoPtr )
	{
	SCOREBOARD_INDEX_INFO *scoreboardIndexInfo = scoreboardIndexInfoPtr;
	SCOREBOARD_DATA *scoreboardData;
	int status;

	/* Shut down the scoreboard.  We acquire the mutex while we're doing 
	   this to ensure that any threads still using it have exited before we 
	   destroy it.  Exactly what to do if we can't acquire the mutex is a 
	   bit complicated because failing to acquire the mutex is a special-
	   case exception condition so it's not even possible to plan for this 
	   since it's uncertain under which conditions (if ever) it would 
	   occur.  For now we play it by the book and don't do anything if we 
	   can't acquire the mutex, which is at least consistent */
	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	ENSURES_V( cryptStatusOK( status ) );	/* See comment above */

	/* Clear and free the scoreboard */
	scoreboardData = DATAPTR_GET( scoreboardIndexInfo->data );
	ENSURES_KRNLMUTEX_V( scoreboardData != NULL, MUTEX_SCOREBOARD );
	status = krnlMemfree( ( void ** ) &scoreboardData );
	ENSURES_KRNLMUTEX_V( cryptStatusOK( status ), MUTEX_SCOREBOARD );
	zeroise( scoreboardIndexInfo, sizeof( SCOREBOARD_INDEX_INFO ) );

	krnlExitMutex( MUTEX_SCOREBOARD );
	}
#endif /* USE_SSL */
