/****************************************************************************
*																			*
*					cryptlib Session Scoreboard	Header File					*
*						Copyright Peter Gutmann 1998-2004					*
*																			*
****************************************************************************/

#ifndef _SCOREBRD_DEFINED

#define _SCOREBRD_DEFINED

/****************************************************************************
*																			*
*						Scoreboard Types and Structures						*
*																			*
****************************************************************************/

/* The search key to use for a scoreboard lookup.  We distinguish between
   client and server sessionIDs in order to provide a logically distinct 
   namespace for client and server sessions */

typedef enum {
	SCOREBOARD_KEY_NONE,
	SCOREBOARD_KEY_SESSIONID_CLI,	/* Lookup by client session ID */
	SCOREBOARD_KEY_SESSIONID_SVR,	/* Lookup by server session ID */
	SCOREBOARD_KEY_FQDN,			/* Lookup by server FQDN */
	SCOREBOARD_KEY_LAST
	} SCOREBOARD_KEY_TYPE;

/* The search result from looking up an entry in the scoreboard */

typedef struct {
	/* Scoreboard search key information */
	BUFFER_OPT_FIXED( keySize ) \
	const void *key;
	int keySize;

	/* The data stored with the scoreboard entry */
	BUFFER_OPT_FIXED( dataSize ) \
	const void *data;
	int dataSize;
	} SCOREBOARD_LOOKUP_RESULT;

/* Storage for the scoreboard state.  When passed to scoreboard functions
   it's declared as a void * because to the caller it's an opaque memory 
   block while to the scoreboard routines it's structured storage */

typedef BYTE SCOREBOARD_STATE[ 64 ];

/****************************************************************************
*																			*
*							Scoreboard Functions							*
*																			*
****************************************************************************/

/* Session scoreboard management functions */

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int lookupScoreboardEntry( INOUT void *scoreboardInfoPtr,
						   IN_ENUM( SCOREBOARD_KEY ) \
								const SCOREBOARD_KEY_TYPE keyType,
						   IN_BUFFER( keyLength ) const void *key, 
						   IN_LENGTH_SHORT_MIN( 2 ) const int keyLength, 
						   OUT SCOREBOARD_LOOKUP_RESULT *lookupResult );
CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int addScoreboardEntry( INOUT void *scoreboardInfoPtr,
						IN_BUFFER( keyLength ) const void *key, 
						IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
						IN_BUFFER( valueLength ) const void *value, 
						IN_LENGTH_SHORT const int valueLength );
CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1, 2, 4, 6 ) ) \
int addScoreboardEntryEx( INOUT void *scoreboardInfoPtr,
						  IN_BUFFER( keyLength ) const void *key, 
						  IN_LENGTH_SHORT_MIN( 8 ) const int keyLength, 
						  IN_BUFFER( keyLength ) const void *altKey, 
						  IN_LENGTH_SHORT_MIN( 2 ) const int altKeyLength, 
						  IN_BUFFER( valueLength ) const void *value, 
						  IN_LENGTH_SHORT const int valueLength );
STDC_NONNULL_ARG( ( 1 ) ) \
void deleteScoreboardEntry( INOUT void *scoreboardInfoPtr, 
							IN_INT_Z const int uniqueID );

#ifdef USE_SSL
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int initScoreboard( INOUT void *scoreboardInfoPtr, 
					  IN_LENGTH_SHORT_MIN( 8 ) const int scoreboardEntries );
  STDC_NONNULL_ARG( ( 1 ) ) \
  void endScoreboard( INOUT void *scoreboardInfoPtr );
#else
  #define initScoreboard( scoreboardInfo, scoreboardSize )	CRYPT_OK
  #define endScoreboard( scoreboardInfo )
#endif /* USE_SSL */
#endif /* _SCOREBRD_DEFINED */
