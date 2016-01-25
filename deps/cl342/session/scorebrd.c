/****************************************************************************
*																			*
*							cryptlib Session Scoreboard						*
*						Copyright Peter Gutmann 1998-2011					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "session.h"
  #include "scorebrd.h"
  #include "ssl.h"
#else
  #include "crypt.h"
  #include "session/session.h"
  #include "session/scorebrd.h"
  #include "session/ssl.h"
#endif /* Compiler-specific includes */

#ifdef USE_SSL

/* The minimum and maximum permitted number of entries in the scoreboard */

#define SCOREBOARD_MIN_SIZE		8
#define SCOREBOARD_MAX_SIZE		128

/* The maximum size of any identifier and data value to be stored in the 
   scoreboard.  Since the scoreboard is currently only used for SSL session
   resumption, these are MAX_SESSIONID_SIZE, 32 bytes, and SSL_SECRET_SIZE, 
   48 bytes */

#define SCOREBOARD_KEY_SIZE		MAX_SESSIONID_SIZE
#define SCOREBOARD_DATA_SIZE	SSL_SECRET_SIZE

/* An individual scoreboard entry containing index information and its 
   corresponding data.  This is stored in separate memory blocks because one 
   is allocated in secure nonpageable storage and the other isn't, with 
   scoreboardIndex[] containing pointers into corresponding entries in 
   scoreboardData[] */

typedef BYTE SCOREBOARD_DATA[ SCOREBOARD_DATA_SIZE ];
typedef struct {
	/* Identification information: The checksum and hash of the session ID 
	   (to locate an entry based on the sessionID sent by the client) and 
	   checksum and hash of the FQDN (to locate an entry based on the server
	   FQDN) */
	int sessionCheckValue;
	BUFFER_FIXED( HASH_DATA_SIZE ) \
	BYTE sessionHash[ HASH_DATA_SIZE + 4 ];
	int fqdnCheckValue;
	BUFFER_FIXED( HASH_DATA_SIZE ) \
	BYTE fqdnHash[ HASH_DATA_SIZE + 4 ];

	/* Since a lookup may have to return a session ID value if we're going
	   from an FQDN to session a ID, we have to store the full session ID 
	   value alongside its checksum and hash */
	BUFFER( SCOREBOARD_KEY_SIZE, sessionIDlength ) \
	BYTE sessionID[ SCOREBOARD_KEY_SIZE + 4 ];
	int sessionIDlength;

	/* The scoreboard data, just a pointer into the secure SCOREBOARD_DATA 
	   memory.  The dataLength variable records how much data is actually
	   present out of the SCOREBOARD_DATA_SIZE bytes that are available for
	   use */
	BUFFER( SCOREBOARD_DATA_SIZE, dataLength ) \
	void *data;
	int dataLength;

	/* Miscellaneous information.  We record whether an entry corresponds to
	   server or client data in order to provide logically separate 
	   namespaces for client and server */
	time_t timeStamp;		/* Time entry was added to the scoreboard */
	BOOLEAN isServerData;	/* Whether this is client or server value */
	int uniqueID;			/* Unique ID for this entry */
	} SCOREBOARD_INDEX;

/* The maximum amount of time that an entry is retained in the scoreboard,
   1 hour */

#define SCOREBOARD_TIMEOUT		3600

/* Overall scoreboard information.  Note that the SCOREBOARD_STATE size 
   define in scorebrd.h will need to be updated if this structure is 
   changed */

typedef struct {
	/* Scoreboard index and data storage, and the total number of entries in
	   the scoreboard */
	void *index, *data;			/* Scoreboard index and data */
	int noEntries;				/* Total scoreboard entries */

	/* The last used entry in the scoreboard, and a unique ID for each 
	   scoreboard entry.  This is incremented for each index entry added,
	   so that even if an entry is deleted and then another one with the 
	   same index value added, the uniqueID for the two will be different */
	int lastEntry;				/* Last used entry in scoreboard */
	int uniqueID;				/* Unique ID for scoreboard entry */
	} SCOREBOARD_INFO;

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the scoreboard state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheck( const SCOREBOARD_INFO *scoreboardInfo )
	{
	assert( isReadPtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );

	/* Make sure that the general state is in order */
	if( scoreboardInfo->noEntries < SCOREBOARD_MIN_SIZE || \
		scoreboardInfo->noEntries > SCOREBOARD_MAX_SIZE )
		return( FALSE );
	if( scoreboardInfo->lastEntry < 0 || \
		scoreboardInfo->lastEntry > scoreboardInfo->noEntries )
		return( FALSE );
	if( scoreboardInfo->uniqueID < 0 )
		return( FALSE );

	return( TRUE );
	}

/* Clear a scoreboard entry */

STDC_NONNULL_ARG( ( 1 ) ) \
static void clearScoreboardEntry( SCOREBOARD_INDEX *scoreboardEntryPtr )
	{
	void *savedDataPtr = scoreboardEntryPtr->data;

	assert( isWritePtr( scoreboardEntryPtr, \
						sizeof( SCOREBOARD_INDEX ) ) );
	assert( isWritePtr( scoreboardEntryPtr->data, SCOREBOARD_DATA_SIZE ) );

	REQUIRES_V( scoreboardEntryPtr->data != NULL );

	zeroise( scoreboardEntryPtr->data, SCOREBOARD_DATA_SIZE );
	memset( scoreboardEntryPtr, 0, sizeof( SCOREBOARD_INDEX ) );
	scoreboardEntryPtr->data = savedDataPtr;
	scoreboardEntryPtr->dataLength = 0;
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
						 IN_BUFFER( valueLength ) const void *value, 
						 IN_LENGTH_SHORT const int valueLength,
						 const time_t currentTime )
	{
	int status;

	assert( isWritePtr( scoreboardEntryPtr, sizeof( SCOREBOARD_INDEX ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( ( altKey == NULL && altKeyLength == 0 ) || \
			isReadPtr( altKey, altKeyLength ) );
	assert( isReadPtr( value, valueLength ) );

	REQUIRES( keyCheckValue >= 0 );
	REQUIRES( keyLength >= 8 && keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( ( altKey == NULL && altKeyLength == 0 && \
				altKeyCheckValue == 0 ) || \
			  ( altKey != NULL && \
				altKeyLength >= 2 && altKeyLength < MAX_INTLENGTH_SHORT && \
				altKeyCheckValue >= 0 ) );
	REQUIRES( valueLength > 0 && valueLength <= SCOREBOARD_DATA_SIZE );
	REQUIRES( currentTime > MIN_TIME_VALUE );

	/* Clear the existing data in the entry */
	clearScoreboardEntry( scoreboardEntryPtr );

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
	status = attributeCopyParams( scoreboardEntryPtr->data, 
								  SCOREBOARD_DATA_SIZE, 
								  &scoreboardEntryPtr->dataLength,
								  value, valueLength );
	ENSURES( cryptStatusOK( status ) );
	scoreboardEntryPtr->isServerData = ( altKey == NULL ) ? TRUE : FALSE;
	scoreboardEntryPtr->timeStamp = currentTime;

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
static int findEntry( INOUT SCOREBOARD_INFO *scoreboardInfo,
					  IN_ENUM( SCOREBOARD_KEY ) \
							const SCOREBOARD_KEY_TYPE keyType,
					  IN_BUFFER( keyLength ) const void *key, 
					  IN_LENGTH_SHORT_MIN( 2 ) const int keyLength, 
					  const time_t currentTime, 
					  OUT_INT_SHORT_Z int *position )
	{
	SCOREBOARD_INDEX *scoreboardIndex = scoreboardInfo->index;
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
	int matchPosition = CRYPT_ERROR, i;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( isWritePtr( position, sizeof( int ) ) );
	assert( isWritePtr( scoreboardIndex,
						scoreboardInfo->noEntries * sizeof( SCOREBOARD_INDEX ) ) );

	REQUIRES( keyType > SCOREBOARD_KEY_NONE && \
			  keyType < SCOREBOARD_KEY_LAST );
	REQUIRES( keyLength >= 2 && keyLength < MAX_INTLENGTH_SHORT);
	REQUIRES( currentTime > MIN_TIME_VALUE );

	/* Clear return value */
	*position = CRYPT_ERROR;

	/* Scan the scoreboard expiring old entries, looking for a match 
	   (indicated by matchPosition), and keeping a record of the oldest 
	   entry (recorded by oldestEntry) in case we need to expire an entry to
	   make room for a new one */
	for( i = 0; i < scoreboardInfo->lastEntry && \
				i < FAILSAFE_ITERATIONS_MAX; i++ )
		{
		SCOREBOARD_INDEX *scoreboardEntryPtr = &scoreboardIndex[ i ];

		/* If this entry has expired, delete it */
		if( scoreboardEntryPtr->timeStamp + SCOREBOARD_TIMEOUT < currentTime )
			clearScoreboardEntry( scoreboardEntryPtr );

		/* Check for a free entry and the oldest non-free entry.  We could
		   perform an early-out once we find a free entry but this would
		   prevent any following expired entries from being deleted */
		if( scoreboardEntryPtr->timeStamp <= MIN_TIME_VALUE )
			{
			/* We've found a free entry, remember it for future use if
			   required and continue */
			if( nextFreeEntry == CRYPT_ERROR )
				nextFreeEntry = i;
			continue;
			}
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
	if( lastUsedEntry + 1 < scoreboardInfo->lastEntry )
		scoreboardInfo->lastEntry = lastUsedEntry + 1;

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
		if( scoreboardInfo->lastEntry < scoreboardInfo->noEntries )
			*position = scoreboardInfo->lastEntry;
		else
			{
			/* There are no free positions, overwrite the oldest entry */
			*position = oldestEntry;
			}
		}
	ENSURES( *position >= 0 && *position < scoreboardInfo->noEntries );

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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 6, 8 ) ) \
static int addEntry( INOUT SCOREBOARD_INFO *scoreboardInfo, 
					 IN_BUFFER( keyLength ) const void *key, 
					 IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
					 IN_BUFFER_OPT( altKeyLength ) const void *altKey, 
					 IN_LENGTH_SHORT_Z const int altKeyLength, 
					 IN_BUFFER( valueLength ) const void *value, 
					 IN_LENGTH_SHORT const int valueLength,
					 OUT_INT_Z int *uniqueID )
	{
	SCOREBOARD_INDEX *scoreboardIndex = scoreboardInfo->index;
	SCOREBOARD_INDEX *scoreboardEntryPtr = NULL;
	const time_t currentTime = getTime();
	const BOOLEAN isClient = ( altKey != NULL ) ? TRUE : FALSE;
	const int checkValue = checksumData( key, keyLength );
	int altCheckValue = 0, altPosition = DUMMY_INIT;
	int position, altStatus = CRYPT_ERROR, status;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( ( altKey == NULL && altKeyLength == 0 ) || \
			isReadPtr( altKey, altKeyLength ) );
	assert( isReadPtr( value, valueLength ) );
	assert( isWritePtr( uniqueID, sizeof( int ) ) );
	assert( isWritePtr( scoreboardIndex,
						scoreboardInfo->noEntries * sizeof( SCOREBOARD_INDEX ) ) );

	REQUIRES( keyLength >= 8 && keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( ( altKey == NULL && altKeyLength == 0 ) || \
			  ( altKey != NULL && \
				altKeyLength >= 2 && altKeyLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( valueLength > 0 && valueLength <= SCOREBOARD_DATA_SIZE );

	REQUIRES( sanityCheck( scoreboardInfo ) );

	/* Clear return value */
	*uniqueID = CRYPT_ERROR;

	/* If there's something wrong with the time then we can't perform (time-
	   based) scoreboard management */
	if( currentTime <= MIN_TIME_VALUE )
		return( CRYPT_ERROR_NOTFOUND );
	
	/* Try and find this entry in the scoreboard */
	status = findEntry( scoreboardInfo, isClient ? \
							SCOREBOARD_KEY_SESSIONID_CLI : \
							SCOREBOARD_KEY_SESSIONID_SVR, 
						key, keyLength, currentTime, &position );
	if( cryptStatusError( status ) && status != OK_SPECIAL )
		return( status );
	ENSURES( position >= 0 && position < scoreboardInfo->noEntries );
	if( altKey != NULL )
		{
		altCheckValue = checksumData( altKey, altKeyLength );
		altStatus = findEntry( scoreboardInfo, SCOREBOARD_KEY_FQDN, 
							   altKey, altKeyLength, currentTime, 
							   &altPosition );
		if( cryptStatusError( altStatus ) && altStatus != OK_SPECIAL )
			return( altStatus );
		ENSURES( altPosition >= 0 && \
				 altPosition < scoreboardInfo->noEntries );
		}
	ENSURES( cryptStatusOK( status ) || status == OK_SPECIAL );
	ENSURES( altKey == NULL || \
			 cryptStatusOK( altStatus ) || altStatus == OK_SPECIAL );

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
		if( scoreboardEntryPtr->dataLength != valueLength || \
			memcmp( scoreboardEntryPtr->data, value, valueLength ) )
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
	ENSURES( scoreboardEntryPtr != NULL );

	/* Add the data to the new scoreboard entry position */
	status = addEntryData( scoreboardEntryPtr, checkValue, key, keyLength, 
						   altCheckValue, altKey, altKeyLength, value, 
						   valueLength, currentTime );
	if( cryptStatusError( status ) )
		{
		clearScoreboardEntry( scoreboardEntryPtr );
		return( status );
		}
	*uniqueID = scoreboardEntryPtr->uniqueID = scoreboardInfo->uniqueID++;

	/* If we've used a new entry, update the position-used index */
	if( position >= scoreboardInfo->lastEntry )
		scoreboardInfo->lastEntry = position + 1;

	return( CRYPT_OK );
	}

/* Look up data in the scoreboard */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5, 6 ) ) \
static int lookupScoreboard( INOUT SCOREBOARD_INFO *scoreboardInfo,
							 IN_ENUM( SCOREBOARD_KEY ) \
								const SCOREBOARD_KEY_TYPE keyType,
							 IN_BUFFER( keyLength ) const void *key, 
							 IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
						     OUT SCOREBOARD_LOOKUP_RESULT *lookupResult,
							 OUT_INT_Z int *uniqueID )
	{
	SCOREBOARD_INDEX *scoreboardIndex = scoreboardInfo->index;
	SCOREBOARD_INDEX *scoreboardEntryPtr;
	const time_t currentTime = getTime();
	int position, status;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( isWritePtr( lookupResult, sizeof( SCOREBOARD_LOOKUP_RESULT ) ) );
	assert( isWritePtr( uniqueID, sizeof( int ) ) );
	assert( isWritePtr( scoreboardIndex,
						scoreboardInfo->noEntries * sizeof( SCOREBOARD_INDEX ) ) );

	REQUIRES( keyType > SCOREBOARD_KEY_NONE && \
			  keyType < SCOREBOARD_KEY_LAST );
	REQUIRES( keyLength >= 8 && keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( sanityCheck( scoreboardInfo ) );

	/* Clear return values */
	memset( lookupResult, 0, sizeof( SCOREBOARD_LOOKUP_RESULT ) );
	*uniqueID = CRYPT_ERROR;

	/* If there's something wrong with the time then we can't perform (time-
	   based) scoreboard management */
	if( currentTime <= MIN_TIME_VALUE )
		return( CRYPT_ERROR_NOTFOUND );

	/* Try and find this entry in the scoreboard */
	status = findEntry( scoreboardInfo, keyType, key, keyLength, 
						currentTime, &position );
	if( cryptStatusError( status ) )
		{
		/* An OK_SPECIAL status means that the search found an unused entry 
		   position but not a matching entry (this is used by addEntry()), 
		   anything else is an error */
		return( ( status == OK_SPECIAL ) ? CRYPT_ERROR_NOTFOUND : status );
		}
	ENSURES( position >= 0 && position < scoreboardInfo->noEntries );
	scoreboardEntryPtr = &scoreboardIndex[ position ];

	/* We've found a match, return a pointer to the data (which avoids 
	   copying it out of secure memory) and the unique ID for it */
	lookupResult->key = scoreboardEntryPtr->sessionID;
	lookupResult->keySize = scoreboardEntryPtr->sessionIDlength;
	lookupResult->data = scoreboardEntryPtr->data;
	lookupResult->dataSize = scoreboardEntryPtr->dataLength;
	*uniqueID = scoreboardEntryPtr->uniqueID;

	/* Update the entry's last-access date */
	scoreboardEntryPtr->timeStamp = currentTime;

	ENSURES( sanityCheck( scoreboardInfo ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Scoreboard Access Functions						*
*																			*
****************************************************************************/

/* Add and delete entries to/from the scoreboard.  These are just wrappers
   for the local scoreboard-access function, for use by external code */

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int lookupScoreboardEntry( INOUT TYPECAST( SCOREBOARD_INFO * ) \
								void *scoreboardInfoPtr,
						   IN_ENUM( SCOREBOARD_KEY ) \
								const SCOREBOARD_KEY_TYPE keyType,
						   IN_BUFFER( keyLength ) const void *key, 
						   IN_LENGTH_SHORT_MIN( 2 ) const int keyLength, 
						   OUT SCOREBOARD_LOOKUP_RESULT *lookupResult )
	{
	SCOREBOARD_INFO *scoreboardInfo = scoreboardInfoPtr;
	int uniqueID, status;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( isWritePtr( lookupResult, sizeof( SCOREBOARD_LOOKUP_RESULT ) ) );

	REQUIRES( keyType > SCOREBOARD_KEY_NONE && \
			  keyType < SCOREBOARD_KEY_LAST );
	REQUIRES( keyLength >= 2 && keyLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( lookupResult, 0, sizeof( SCOREBOARD_LOOKUP_RESULT ) );

	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		return( status );
	status = lookupScoreboard( scoreboardInfo, keyType, key, keyLength, 
							   lookupResult, &uniqueID );
	krnlExitMutex( MUTEX_SCOREBOARD );
	return( cryptStatusError( status ) ? status : uniqueID );
	}

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int addScoreboardEntry( INOUT TYPECAST( SCOREBOARD_INFO * ) \
							void *scoreboardInfoPtr,
						IN_BUFFER( keyLength ) const void *key, 
						IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
						IN_BUFFER( valueLength ) const void *value, 
						IN_LENGTH_SHORT const int valueLength )
	{
	SCOREBOARD_INFO *scoreboardInfo = scoreboardInfoPtr;
	int uniqueID, status;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( isReadPtr( value, valueLength ) );

	REQUIRES( keyLength >= 8 && keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( valueLength > 0 && valueLength <= SCOREBOARD_DATA_SIZE );

	/* Add the entry to the scoreboard */
	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		return( status );
	status = addEntry( scoreboardInfo, key, keyLength, NULL, 0,
					   ( void * ) value, valueLength, &uniqueID );
	krnlExitMutex( MUTEX_SCOREBOARD );
	return( cryptStatusError( status ) ? status : uniqueID );
	}

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1, 2, 4, 6 ) ) \
int addScoreboardEntryEx( INOUT TYPECAST( SCOREBOARD_INFO * ) \
								void *scoreboardInfoPtr,
						  IN_BUFFER( keyLength ) const void *key, 
						  IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
						  IN_BUFFER( keyLength ) const void *altKey, 
						  IN_LENGTH_SHORT_MIN( 2 ) const int altKeyLength, 
						  IN_BUFFER( valueLength ) const void *value, 
						  IN_LENGTH_SHORT const int valueLength )
	{
	SCOREBOARD_INFO *scoreboardInfo = scoreboardInfoPtr;
	int uniqueID, status;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( isReadPtr( altKey, altKeyLength ) );
	assert( isReadPtr( value, valueLength ) );

	REQUIRES( keyLength >= 8 && keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( altKeyLength >= 2 && altKeyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( valueLength > 0 && valueLength <= SCOREBOARD_DATA_SIZE );

	/* Add the entry to the scoreboard */
	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		return( status );
	status = addEntry( scoreboardInfo, key, keyLength, altKey, altKeyLength,
					   ( void * ) value, valueLength, &uniqueID );
	krnlExitMutex( MUTEX_SCOREBOARD );
	return( cryptStatusError( status ) ? status : uniqueID );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteScoreboardEntry( INOUT TYPECAST( SCOREBOARD_INFO * ) \
								void *scoreboardInfoPtr, 
							IN_INT_Z const int uniqueID )
	{
	SCOREBOARD_INFO *scoreboardInfo = scoreboardInfoPtr;
	SCOREBOARD_INDEX *scoreboardIndex = scoreboardInfo->index;
	int lastUsedEntry = -1, i, status;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	
	REQUIRES_V( uniqueID >= 0 && \
				uniqueID < MAX_INTLENGTH );

	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		return;

	/* Search the scoreboard for the entry with the given ID */
	for( i = 0; i < scoreboardInfo->lastEntry && \
				i < FAILSAFE_ITERATIONS_MAX; i++ )
		{
		SCOREBOARD_INDEX *scoreboardEntryPtr = &scoreboardIndex[ i ];

		/* If it's an empty entry (due to it having expired or being 
		   deleted), skip it and continue */
		if( scoreboardEntryPtr->timeStamp <= MIN_TIME_VALUE )
			continue;

		/* If we've found the entry that we're after, clear it and exit */
		if( scoreboardEntryPtr->uniqueID == uniqueID )
			{
			clearScoreboardEntry( scoreboardEntryPtr );
			continue;
			}

		/* Remember how far we got */
		lastUsedEntry = i;
		}
	ENSURES_V( i < FAILSAFE_ITERATIONS_MAX );

	/* Since we may have deleted entries at the end of the scoreboard, we 
	   can reduce the lastEntry value to the highest remaining entry */
	scoreboardInfo->lastEntry = lastUsedEntry + 1;

	krnlExitMutex( MUTEX_SCOREBOARD );
	}

/****************************************************************************
*																			*
*							Scoreboard Init/Shutdown						*
*																			*
****************************************************************************/

/* Perform a self-test of the scoreboard functions */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN selfTest( INOUT SCOREBOARD_INFO *scoreboardInfo )
	{
	SCOREBOARD_LOOKUP_RESULT lookupResult;
	int uniqueID1, uniqueID2, foundUniqueID;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	 
	uniqueID1 = addScoreboardEntry( scoreboardInfo, "test key 1", 10,
									"test value 1", 12 );
	if( cryptStatusError( uniqueID1 ) )
		return( FALSE );
	uniqueID2 = addScoreboardEntry( scoreboardInfo, "test key 2", 10,
									"test value 2", 12 );
	if( cryptStatusError( uniqueID2 ) )
		return( FALSE );
	foundUniqueID = lookupScoreboardEntry( scoreboardInfo, 
							SCOREBOARD_KEY_SESSIONID_SVR, "test key 1", 10,
							&lookupResult );
	if( cryptStatusError( foundUniqueID ) )
		return( FALSE );
	if( foundUniqueID != uniqueID1 || \
		lookupResult.keySize != 10 || \
		memcmp( lookupResult.key, "test key 1", 10 ) || \
		lookupResult.dataSize != 12 || \
		memcmp( lookupResult.data, "test value 1", 12 ) )
		return( FALSE );
	deleteScoreboardEntry( scoreboardInfo, uniqueID1 );
	foundUniqueID = lookupScoreboardEntry( scoreboardInfo, 
							SCOREBOARD_KEY_SESSIONID_SVR, "test key 1", 10,
							&lookupResult );
	if( foundUniqueID != CRYPT_ERROR_NOTFOUND )
		return( FALSE );
	deleteScoreboardEntry( scoreboardInfo, uniqueID2 );
	if( scoreboardInfo->lastEntry != 0 || \
		scoreboardInfo->uniqueID != 2 )
		return( FALSE );

	return( TRUE );
	}

/* Initialise and shut down the scoreboard */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initScoreboard( INOUT TYPECAST( SCOREBOARD_INFO * ) \
						void *scoreboardInfoPtr, 
					IN_LENGTH_SHORT_MIN( SCOREBOARD_MIN_SIZE ) \
						const int scoreboardEntries )
	{
	SCOREBOARD_INFO *scoreboardInfo = scoreboardInfoPtr;
	SCOREBOARD_INDEX *scoreboardIndex;
	SCOREBOARD_DATA *scoreboardData;
	int i, status;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );
	
	static_assert( sizeof( SCOREBOARD_STATE ) >= sizeof( SCOREBOARD_INFO ), \
				   "Scoreboard size" );

	REQUIRES( scoreboardEntries >= SCOREBOARD_MIN_SIZE && \
			  scoreboardEntries <= SCOREBOARD_MAX_SIZE );

	status = krnlEnterMutex( MUTEX_SCOREBOARD );
	if( cryptStatusError( status ) )
		return( status );

	/* Initialise the scoreboard */
	memset( scoreboardInfo, 0, sizeof( SCOREBOARD_INFO ) );
	scoreboardInfo->uniqueID = 0;
	scoreboardInfo->lastEntry = 0;
	scoreboardInfo->noEntries = scoreboardEntries;

	/* Initialise the scoreboard data */
	if( ( scoreboardInfo->index = clAlloc( "initScoreboard", \
				scoreboardEntries * sizeof( SCOREBOARD_INDEX ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	status = krnlMemalloc( &scoreboardInfo->data, \
						   scoreboardEntries * sizeof( SCOREBOARD_DATA ) );
	if( cryptStatusError( status ) )
		{
		clFree( "initScoreboard", scoreboardInfo->index );
		memset( scoreboardInfo, 0, sizeof( SCOREBOARD_INFO ) );
		return( status );
		}
	scoreboardIndex = scoreboardInfo->index;
	scoreboardData = scoreboardInfo->data;
	memset( scoreboardIndex, 0, \
			scoreboardEntries * sizeof( SCOREBOARD_INDEX ) );
	for( i = 0; i < scoreboardEntries; i++ )
		{
		scoreboardIndex[ i ].data = &scoreboardData[ i ];
		scoreboardIndex[ i ].dataLength = 0;
		}
	memset( scoreboardInfo->data, 0, \
			scoreboardEntries * sizeof( SCOREBOARD_DATA ) );

	/* Make sure that everything's working as intended */
	if( !selfTest( scoreboardInfo ) )
		{
		status = krnlMemfree( ( void ** ) &scoreboardInfo->data );
		ENSURES( cryptStatusOK( status ) );
		clFree( "initScoreboard", scoreboardInfo->index );
		memset( scoreboardInfo, 0, sizeof( SCOREBOARD_INFO ) );

		retIntError();
		}

	krnlExitMutex( MUTEX_SCOREBOARD );
	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void endScoreboard( INOUT TYPECAST( SCOREBOARD_INFO * ) \
						void *scoreboardInfoPtr )
	{
	SCOREBOARD_INFO *scoreboardInfo = scoreboardInfoPtr;
	int status;

	assert( isWritePtr( scoreboardInfo, sizeof( SCOREBOARD_INFO ) ) );

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
	status = krnlMemfree( ( void ** ) &scoreboardInfo->data );
	ENSURES_V( cryptStatusOK( status ) );	/* See comment above */
	zeroise( scoreboardInfo->index, \
			 scoreboardInfo->noEntries * sizeof( SCOREBOARD_INDEX ) );
	clFree( "endScoreboard", scoreboardInfo->index );
	zeroise( scoreboardInfo, sizeof( SCOREBOARD_INFO ) );

	krnlExitMutex( MUTEX_SCOREBOARD );
	}
#endif /* USE_SSL */
