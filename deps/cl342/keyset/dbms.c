/****************************************************************************
*																			*
*						 cryptlib DBMS Backend Interface					*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#include <ctype.h>
#include <stdarg.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "keyset.h"
  #include "dbms.h"
  #include "rpc.h"
#else
  #include "crypt.h"
  #include "keyset/keyset.h"
  #include "keyset/dbms.h"
  #include "misc/rpc.h"
#endif /* Compiler-specific includes */

#ifdef USE_DBMS

/****************************************************************************
*																			*
*						Backend Database Access Functions					*
*																			*
****************************************************************************/

/* Dispatch functions for various database types.  ODBC is the native keyset
   for Windows and (frequently) Unix, the rest are accessible via database 
   plugins */

#ifdef USE_ODBC
  int initDispatchODBC( DBMS_INFO *dbmsInfo );
#else
  #define initDispatchODBC( dbmsInfo )		CRYPT_ERROR
#endif /* USE_ODBC */
#if defined( USE_DATABASE )
  int initDispatchDatabase( DBMS_INFO *dbmsInfo );
#else
  #define initDispatchDatabase( dbmsInfo )	CRYPT_ERROR
#endif /* General database interface */

/* Database access functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
static int openDatabase( INOUT DBMS_INFO *dbmsInfo, 
						 IN_BUFFER( nameLen ) const char *name,
						 IN_LENGTH_NAME const int nameLen, 
						 IN_ENUM_OPT( CRYPT_KEYOPT ) \
							const CRYPT_KEYOPT_TYPE options, 
						 OUT_FLAGS_Z( DBMS_FEATURE ) int *featureFlags )
	{
	DBMS_STATE_INFO *dbmsStateInfo = dbmsInfo->stateInfo;
	int status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( isReadPtr( name, nameLen ) );
	assert( isWritePtr( featureFlags, sizeof( int ) ) );

	REQUIRES( nameLen >= MIN_NAME_LENGTH && \
			  nameLen < MAX_ATTRIBUTE_SIZE );
	REQUIRES( options >= CRYPT_KEYOPT_NONE && options < CRYPT_KEYOPT_LAST );

	/* Clear return value */
	*featureFlags = DBMS_FEATURE_FLAG_NONE;

	status = dbmsInfo->openDatabaseBackend( dbmsStateInfo, name, nameLen, 
											options, featureFlags );
	if( cryptStatusError( status ) )
		return( status );

	/* Make long-term information returned as a back-end interface-specific
	   feature flags persistent if necessary */
	if( *featureFlags & DBMS_FEATURE_FLAG_BINARYBLOBS )
		dbmsInfo->flags |= DBMS_FLAG_BINARYBLOBS;

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void closeDatabase( INOUT DBMS_INFO *dbmsInfo )
	{
	DBMS_STATE_INFO *dbmsStateInfo = dbmsInfo->stateInfo;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	dbmsInfo->closeDatabaseBackend( dbmsStateInfo );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int performUpdate( INOUT DBMS_INFO *dbmsInfo, 
						  IN_STRING_OPT const char *command,
						  IN_ARRAY_OPT_C( BOUND_DATA_MAXITEMS ) \
							TYPECAST( BOUND_DATA ) const void *boundData,
						  IN_ENUM( DBMS_UPDATE ) \
							const DBMS_UPDATE_TYPE updateType )
	{
	DBMS_STATE_INFO *dbmsStateInfo = dbmsInfo->stateInfo;
	const int commandLength = ( command != NULL ) ? strlen( command ) : 0;
	int status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( ( command == NULL && commandLength == 0 && \
			  updateType == DBMS_UPDATE_ABORT ) || \
			isReadPtr( command, commandLength ) );
	assert( ( boundData == NULL ) || \
			isReadPtr( boundData, \
					   sizeof( BOUND_DATA ) * BOUND_DATA_MAXITEMS ) );

	REQUIRES( ( updateType == DBMS_UPDATE_ABORT && \
				command == NULL && commandLength == 0 ) || \
			  ( updateType != DBMS_UPDATE_ABORT && \
				command != NULL && \
				commandLength > 0 && commandLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( updateType > DBMS_UPDATE_NONE && \
			  updateType < DBMS_UPDATE_LAST );

	/* If we're trying to abort a transaction that was never begun, don't
	   do anything */
	if( updateType == DBMS_UPDATE_ABORT && \
		!( dbmsInfo->flags & DBMS_FLAG_UPDATEACTIVE ) )
		{
		DEBUG_DIAG(( "Tried to roll back non-begun transaction." ));
		return( CRYPT_OK );
		}

	/* Process the update */
	status = dbmsInfo->performUpdateBackend( dbmsStateInfo, command,
											 commandLength, boundData,
											 updateType );
	if( cryptStatusOK( status ) && updateType == DBMS_UPDATE_BEGIN )
		{
		/* We've successfully started a transaction, record the update
		   state */
		dbmsInfo->flags |= DBMS_FLAG_UPDATEACTIVE;
		}
	if( updateType == DBMS_UPDATE_COMMIT || \
		updateType == DBMS_UPDATE_ABORT )
		{
		/* Final commits or rollbacks always end a transaction, even if
		   they're performed as part of a failed transaction */
		dbmsInfo->flags &= ~DBMS_FLAG_UPDATEACTIVE;
		}
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int performStaticUpdate( INOUT DBMS_INFO *dbmsInfo, 
								IN_STRING const char *command )
	{
	return( performUpdate( dbmsInfo, command, NULL, DBMS_UPDATE_NORMAL ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int performQuery( INOUT DBMS_INFO *dbmsInfo, 
						 IN_STRING_OPT const char *command,
						 OUT_BUFFER_OPT( dataMaxLength, *dataLength ) \
							void *data, 
						 IN_LENGTH_SHORT_Z const int dataMaxLength, 
						 OUT_OPT_LENGTH_SHORT_Z int *dataLength, 
						 IN_ARRAY_OPT_C( BOUND_DATA_MAXITEMS ) \
							TYPECAST( BOUND_DATA ) const void *boundData,
						 IN_ENUM_OPT( DBMS_CACHEDQUERY ) \
							const DBMS_CACHEDQUERY_TYPE queryEntry,
						 IN_ENUM( DBMS_QUERY ) const DBMS_QUERY_TYPE queryType )
	{
	DBMS_STATE_INFO *dbmsStateInfo = dbmsInfo->stateInfo;
	const int commandLength = ( command != NULL ) ? strlen( command ) : 0;
	int status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( ( command == NULL && commandLength == 0 && \
			  ( queryType == DBMS_QUERY_CONTINUE || \
				queryType == DBMS_QUERY_CANCEL ) ) || \
			isReadPtr( command, commandLength ) );
	assert( ( data == NULL && dataLength == NULL ) || \
			isWritePtr( data, MAX_QUERY_RESULT_SIZE ) );
	assert( ( boundData == NULL ) || \
			isReadPtr( boundData, \
					   sizeof( BOUND_DATA ) * BOUND_DATA_MAXITEMS ) );

	REQUIRES( ( ( queryType == DBMS_QUERY_CONTINUE || \
				  queryType == DBMS_QUERY_CANCEL ) && \
				command == NULL && commandLength == 0 ) || \
			  ( ( queryType == DBMS_QUERY_START || \
				  queryType == DBMS_QUERY_CHECK || \
				  queryType == DBMS_QUERY_NORMAL ) && \
				command != NULL && \
				commandLength > 0 && commandLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( data == NULL && dataMaxLength == 0 && \
				dataLength == NULL ) || \
			  ( data != NULL && dataMaxLength >= 16 && \
				dataMaxLength < MAX_INTLENGTH_SHORT && \
				dataLength != NULL ) );
	REQUIRES( queryEntry >= DBMS_CACHEDQUERY_NONE && \
			  queryEntry < DBMS_CACHEDQUERY_LAST );
	REQUIRES( queryType > DBMS_QUERY_NONE && \
			  queryType < DBMS_QUERY_LAST );

	/* Additional state checks: If we're starting a new query or performing
	   a point query there can't already be one active and if we're
	   continuing or cancelling an existing query there has to be one
	   already active */
	REQUIRES( ( ( queryType == DBMS_QUERY_START || \
				  queryType == DBMS_QUERY_CHECK || \
				  queryType == DBMS_QUERY_NORMAL ) && \
				!( dbmsInfo->flags & DBMS_FLAG_QUERYACTIVE ) ) ||
			  ( ( queryType == DBMS_QUERY_CONTINUE || \
				  queryType == DBMS_QUERY_CANCEL ) && \
				( dbmsInfo->flags & DBMS_FLAG_QUERYACTIVE ) ) );

	/* Clear return value */
	if( data != NULL )
		{
		memset( data, 0, min( 16, dataMaxLength ) );
		*dataLength = 0;
		}

	/* Process the query */
	status = dbmsInfo->performQueryBackend( dbmsStateInfo, command, 
											commandLength, data, dataMaxLength, 
											dataLength, boundData, 
											queryEntry, queryType );
	if( cryptStatusError( status ) )
		return( status );

	/* Sanity-check the result data from the back-end */
	if( dataLength != NULL && \
		( *dataLength <= 0 || *dataLength > MAX_QUERY_RESULT_SIZE ) )
		{
		DEBUG_DIAG(( "Database backend return invalid data size" ));
		assert( DEBUG_WARN );
		memset( data, 0, min( 16, dataMaxLength ) );
		*dataLength = 0;
		return( CRYPT_ERROR_BADDATA );
		}

	/* Update the state information based on the query that we've just 
	   performed */
	if( queryType == DBMS_QUERY_START  )
		dbmsInfo->flags |= DBMS_FLAG_QUERYACTIVE;
	if( queryType == DBMS_QUERY_CANCEL )
		dbmsInfo->flags &= ~DBMS_FLAG_QUERYACTIVE;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int performStaticQuery( INOUT DBMS_INFO *dbmsInfo, 
							   IN_STRING_OPT const char *command,
							   IN_ENUM_OPT( DBMS_CACHEDQUERY ) \
								const DBMS_CACHEDQUERY_TYPE queryEntry,
							   IN_ENUM( DBMS_QUERY ) \
								const DBMS_QUERY_TYPE queryType )
	{
	return( performQuery( dbmsInfo, command, NULL, 0, NULL, NULL, 
						  queryEntry, queryType ) );
	}

/****************************************************************************
*																			*
*								SQL Rewrite Routines						*
*																			*
****************************************************************************/

/* In order to allow general certificate database queries we have to be able 
   to process user-supplied query strings.  The cryptlib manual contains 
   strong warnings about the correct way to do this (if it's done at all), 
   the best that we can do is apply assorted safety checks of the query data 
   to try and reduce the chances of SQL injection.  Unfortunately this can 
   get arbitrarily complicated:

	';	The standard SQL-injection method, used with values like
		'foo; DROP TABLE bar', or '1=1' to return all entries in a table.

	--	Comment delimiter (other values also exist, e.g. MySQL's '#') to
		truncate queries beyond the end of the injected SQL.

	char(0xNN)	Bypass the first level of filtering, e.g. char(0x41)
		produces the banned character '.

   One additional check that we could do is to try and explicitly strip
   SQL keywords from queries but this is somewhat problematic because apart 
   from the usual trickery (e.g. embedding one SQL keyword inside another so 
   that stripping SELECT from SELSELECTECT will still leave the outer 
   SELECT, requiring recursive stripping, or taking advantage of the fact 
   that VARBINARY values are implicitly cast to VARCHARS so that 0x42434445 
   would turn into ABCD, or further escaping the encoding with values like 
   'sel'+'ect') there are also any number of backend-specific custom 
   keywords and ways of escaping keywords that we can't know about and 
   therefore can't easily strip */

#define	SQL_ESCAPE	'\''

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int copyChar( OUT_BUFFER( bufMaxLen, *bufPos ) char *buffer, 
					 IN_LENGTH_SHORT const int bufMaxLen, 
					 OUT_LENGTH_SHORT_Z int *bufPos, 
					 IN_BYTE const int ch, const BOOLEAN escapeQuotes )
	{
	int position = 0;

	assert( isWritePtr( buffer, bufMaxLen ) );
	assert( isWritePtr( bufPos, sizeof( int ) ) );

	REQUIRES( bufMaxLen > 0 && bufMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( ch >= 0 && ch <= 0xFF );

	/* Clear return value */
	*bufPos = 0;

	/* If it's a control character, skip it */
	if( ( ch & 0x7F ) < ' ' )
		return( CRYPT_OK );

	/* Escape metacharacters that could be misused in queries.  We catch the 
	   obvious ' and ; as well as the less obvious %, which could be used to 
	   hide other metacharacters, and \, used by some databases (e.g. MySQL)
	   as an escape.  Note that none of these characters are valid in base64, 
	   which makes it safe to escape them in the few instances where they do 
	   occur */
	if( ( ch == '\'' && escapeQuotes ) || \
		ch == '\\' || ch == ';' || ch == '%' )
		{
		/* Escape the character */
		buffer[ position++ ] = SQL_ESCAPE;
		if( position >= bufMaxLen )
			return( CRYPT_ERROR_OVERFLOW );
		}

	/* Bypass various dangerous SQL "enhancements".  For Windows ODBC (at
	   least for MS Jet 3.x, but not 4.x any more) the driver will execute 
	   anything delimited by '|'s as an expression (an example being 
	   '|shell("cmd /c echo " & chr(124) & " format c:")|'), because of this 
	   we strip gazintas.  Since ODBC uses '{' and '}' as escape delimiters 
	   we also strip these */
	if( ch != '|' && ch != '{' && ch != '}' )
		buffer[ position++ ] = intToByte( ch );

	/* Make sure that we haven't overflowed the output buffer.  This 
	   overflowing can be done deliberately, for example by using large 
	   numbers of escape chars (which are in turn escaped) to force 
	   truncation of the query beyond the injected SQL if the processing 
	   simply stops at a given point */
	if( position >= bufMaxLen )
		return( CRYPT_ERROR_OVERFLOW );
	*bufPos = position;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int copyStringArg( OUT_BUFFER( bufMaxLen, *bufPos ) char *buffer, 
						  IN_LENGTH_SHORT_Z const int bufMaxLen, 
						  OUT_LENGTH_SHORT_Z int *bufPos, 
						  IN_BUFFER( stringLen ) const char *string,
						  IN_LENGTH_SHORT const int stringLen )
	{
	int index, position = 0;

	assert( isWritePtr( buffer, bufMaxLen ) );
	assert( isWritePtr( bufPos, sizeof( int ) ) );
	assert( isReadPtr( string, stringLen ) );

	REQUIRES( bufMaxLen >= 0 && bufMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( stringLen > 0 && stringLen < MAX_INTLENGTH_SHORT );

	/* Make sure that there's room for at least one more character of 
	   output */
	if( bufMaxLen < 1 )
		return( CRYPT_ERROR_OVERFLOW );

	/* Copy the string to the output buffer with conversion of any special 
	   characters that are used by SQL */
	for( index = 0; index < stringLen && \
					index < FAILSAFE_ITERATIONS_MAX; index++ )
		{
		int charsWritten, status;

		status = copyChar( buffer + position, bufMaxLen - position, 
						   &charsWritten, string[ index ], TRUE );
		if( cryptStatusError( status ) )
			return( status );
		position += charsWritten;
		if( position > bufMaxLen )
			{
			/* Already checked in copyChar() but we double-check here to be 
			   safe */
			return( CRYPT_ERROR_OVERFLOW );
			}
		}
	ENSURES( index < FAILSAFE_ITERATIONS_MAX );
	*bufPos = position;

	return( CRYPT_OK );
	}

/* Format input parameters into SQL queries, replacing meta-values with
   actual column names, and null-terminate the resulting string so that
   it can be fed to the database backend */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int dbmsFormatQuery( OUT_BUFFER( outMaxLength, *outLength ) char *output, 
					 IN_LENGTH_SHORT const int outMaxLength, 
					 OUT_LENGTH_SHORT_Z int *outLength,
					 IN_BUFFER( inputLength ) const char *input, 
					 IN_LENGTH_SHORT const int inputLength )
	{
	int inPos, outPos = 0, status = CRYPT_OK;

	assert( isWritePtr( output, outMaxLength ) );
	assert( isWritePtr( outLength, sizeof( int ) ) );
	assert( isReadPtr( input, inputLength ) );

	REQUIRES( outMaxLength >= 0 && outMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( inputLength > 0 && inputLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( output, 0, min( 16, outMaxLength ) );
	*outLength = 0;

	for( inPos = 0; inPos < inputLength && inPos < FAILSAFE_ITERATIONS_MAX; )
		{
		int length;

		if( input[ inPos ] == '$' )
			{
			typedef struct {
				BUFFER_FIXED( sourceLength ) \
				char *sourceName;
				int sourceLength;
				BUFFER_FIXED( destLength ) \
				char *destName;
				int destLength;
				} NAMEMAP_INFO;
			static const NAMEMAP_INFO nameMapTbl[] = {
				{ "C", 1, "C", 1 }, { "SP", 2, "SP", 2 },
				{ "L", 1, "L", 1 }, { "O", 1, "O", 1 },
				{ "OU", 2, "OU", 2 }, { "CN", 2, "CN", 2 },
				{ "email", 5, "email", 5 }, { "uri", 3, "email", 5 },
				{ "date", 4, "validTo", 7 }, 
				{ NULL, 0, NULL, 0 }, { NULL, 0, NULL, 0 }
				};
			const int fieldPos = inPos + 1;
			const char *fieldName = input + fieldPos;
			int i;

			inPos++;	/* Skip '$' */

			/* Extract the field name and translate it into the table
			   column name */
			while( inPos < inputLength && isAlpha( input[ inPos ] ) )
				inPos++;
			length = inPos - fieldPos;
			if( length <= 0 || length > 5 )
				{
				status = CRYPT_ERROR_BADDATA;
				break;
				}
			for( i = 0; nameMapTbl[ i ].sourceName != NULL && \
						i < FAILSAFE_ARRAYSIZE( nameMapTbl, NAMEMAP_INFO ); 
				 i++ )
				{
				if( length == nameMapTbl[ i ].sourceLength && \
					!strCompare( fieldName, nameMapTbl[ i ].sourceName, \
								 length ) )
					break;
				}
			ENSURES( i < FAILSAFE_ARRAYSIZE( nameMapTbl, NAMEMAP_INFO ) );
			if( nameMapTbl[ i ].sourceName == NULL )
				{
				status = CRYPT_ERROR_BADDATA;
				break;
				}

			/* Copy the translated name to the output buffer */
			status = copyStringArg( output + outPos, outMaxLength - outPos, 
									&length, nameMapTbl[ i ].destName,
									nameMapTbl[ i ].destLength );
			}
		else
			{
			/* Just copy the character over, with a length check.  We don't 
			   escape single quotes in this case because we use these 
			   ourselves in SQL queries */
			status = copyChar( output + outPos, outMaxLength - outPos, 
							   &length, input[ inPos++ ], FALSE );
			}
		if( cryptStatusError( status ) )
			break;
		outPos += length;
		}
	ENSURES( inPos < FAILSAFE_ITERATIONS_MAX );
	if( cryptStatusError( status ) )
		outPos = 0;
	output[ outPos ] = '\0';	/* Add der terminador */
	*outLength = outPos;

	return( status );
	}

/****************************************************************************
*																			*
*							Back-end Interface Routines						*
*																			*
****************************************************************************/

/* Parse a user-supplied database name into individual components, used by
   the database back-end connect functions.  We don't do a syntax check
   (since the exact syntax is database-specific) but merely break the single
   string up into any recognisable components.  The database back-end can
   determine whether the format is valid or not.  The general format that we
   look for is:

	[generic name]
	user:pass
	user@server
	user:pass@server
	user:pass@server/name

   One distinction that we make is that if there's something after an '@'
   and there's no server/name separator present we treat it as a name rather 
   than a server.  In other words @foo results in name=foo while @foo/bar 
   results in server=foo, name=bar.  This is because the most common 
   situation that we have to handle is ODBC, which identifies the database 
   by name rather than by server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int dbmsParseName( INOUT DBMS_NAME_INFO *nameInfo, 
				   IN_BUFFER( nameLen ) const char *name, 
				   IN_LENGTH_NAME const int nameLen )
	{
	int offset, offset2, length;

	assert( isWritePtr( nameInfo, sizeof( DBMS_NAME_INFO ) ) );
	assert( isReadPtr( name, nameLen ) );

	REQUIRES( nameLen >= MIN_NAME_LENGTH && \
			  nameLen < MAX_ATTRIBUTE_SIZE );

	memset( nameInfo, 0, sizeof( DBMS_NAME_INFO ) );

	/* Check for a complex database name */
	if( ( offset = strFindCh( name, nameLen, ':' ) ) < 0 && \
		( offset = strFindCh( name, nameLen, '@' ) ) < 0 )
		{
		/* It's a straightforward name, use it directly */
		nameInfo->name = ( char * ) name;
		nameInfo->nameLen = nameLen;

		return( CRYPT_OK );
		}

	/* Extract the user name */
	length = min( offset, CRYPT_MAX_TEXTSIZE );
	if( length <= 0 )
		return( CRYPT_ERROR_OPEN );
	memcpy( nameInfo->userBuffer, name, length );
	nameInfo->user = nameInfo->userBuffer;
	nameInfo->userLen = length;

	/* We're either at the server name or password, extract the password
	   if there is one */
	ENSURES( name[ offset ] == ':' || name[ offset ] == '@' );
	if( name[ offset++ ] == ':' )
		{
		offset2 = strFindCh( name + offset, nameLen - offset, '@' );
		if( offset2 < 0 )
			offset2 = nameLen - offset;	/* Password is rest of string */
		length = min( offset2, CRYPT_MAX_TEXTSIZE );
		if( length <= 0 )
			return( CRYPT_ERROR_OPEN );
		memcpy( nameInfo->passwordBuffer, name + offset, length );
		nameInfo->password = nameInfo->passwordBuffer;
		nameInfo->passwordLen = length;
		offset += offset2 + 1;
		if( offset >= nameLen )
			return( CRYPT_OK );
		}

	/* Separate the server and database name if necessary */
	offset2 = strFindCh( name + offset, nameLen - offset, '/' );
	if( offset2 >= 0 )
		{
		/* There's a distinction between the server name and database name,
		   extract the server name */
		length = min( offset2, CRYPT_MAX_TEXTSIZE );
		if( length <= 0 )
			return( CRYPT_ERROR_OPEN );
		memcpy( nameInfo->serverBuffer, name + offset, length );
		nameInfo->server = nameInfo->serverBuffer;
		nameInfo->serverLen = length;
		offset += offset2 + 1;
		}

	/* Extract the database name if there is one */
	if( offset < nameLen )
		{
		length = nameLen - offset;
		if( length <= 0 || length > CRYPT_MAX_TEXTSIZE )
			return( CRYPT_ERROR_OPEN );
		memcpy( nameInfo->nameBuffer, name + offset, length );
		nameInfo->name = nameInfo->nameBuffer;
		nameInfo->nameLen = length;
		}

	return( CRYPT_OK );
	}

/* Initialise and shut down a session with a database back-end */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDbxSession( INOUT KEYSET_INFO *keysetInfoPtr, 
					IN_ENUM( CRYPT_KEYSET ) const CRYPT_KEYSET_TYPE type )
	{
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;
	int status = CRYPT_ERROR;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );
	REQUIRES( type > CRYPT_KEYSET_NONE && type < CRYPT_KEYSET_LAST );

	/* Select the appropriate dispatch function for the keyset type */
	switch( type )
		{
		case CRYPT_KEYSET_ODBC:
		case CRYPT_KEYSET_ODBC_STORE:
			status = initDispatchODBC( dbmsInfo );
			break;

		case CRYPT_KEYSET_DATABASE:
		case CRYPT_KEYSET_DATABASE_STORE:
			status = initDispatchDatabase( dbmsInfo );
			break;

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_NUM1 );

	/* Set up the remaining function pointers */
	dbmsInfo->openDatabaseFunction = openDatabase;
	dbmsInfo->closeDatabaseFunction = closeDatabase;
	dbmsInfo->performUpdateFunction = performUpdate;
	dbmsInfo->performStaticUpdateFunction = performStaticUpdate;
	dbmsInfo->performQueryFunction = performQuery;
	dbmsInfo->performStaticQueryFunction = performStaticQuery;

	/* Allocate the database session state information */
	if( ( keysetInfoPtr->keyData = \
			clAlloc( "initDbxSession", sizeof( DBMS_STATE_INFO ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( keysetInfoPtr->keyData, 0, sizeof( DBMS_STATE_INFO ) );
	keysetInfoPtr->keyDataSize = sizeof( DBMS_STATE_INFO );
	dbmsInfo->stateInfo = keysetInfoPtr->keyData;
	if( type == CRYPT_KEYSET_ODBC_STORE || \
		type == CRYPT_KEYSET_DATABASE_STORE )
		dbmsInfo->flags |= DBMS_FLAG_CERTSTORE | DBMS_FLAG_CERTSTORE_FIELDS;

	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int endDbxSession( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );

	/* Free the database session state information if necessary */
	if( keysetInfoPtr->keyData != NULL )
		{
		memset( keysetInfoPtr->keyData, 0, keysetInfoPtr->keyDataSize );
		clFree( "endDbxSession", keysetInfoPtr->keyData );
		keysetInfoPtr->keyData = NULL;
		}

	return( CRYPT_OK );
	}
#endif /* USE_DBMS */
