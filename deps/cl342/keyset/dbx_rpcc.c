/****************************************************************************
*																			*
*					  cryptlib Database RPC Client Interface				*
*						Copyright Peter Gutmann 1996-2006					*
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
*						Network Database Interface Routines					*
*																			*
****************************************************************************/

#ifdef USE_DATABASE_PLUGIN

#ifdef USE_RPCAPI

static void netEncodeError( BYTE *buffer, const int status )
	{
	putMessageType( buffer, COMMAND_RESULT, 0, 1, 0 );
	putMessageLength( buffer + COMMAND_WORDSIZE, COMMAND_WORDSIZE );
	putMessageWord( buffer + COMMAND_WORD1_OFFSET, status );
	}

void netProcessCommand( void *stateInfo, BYTE *buffer )
	{
	DBMS_STATE_INFO *dbmsInfo = ( DBMS_STATE_INFO * ) stateInfo;
	COMMAND_INFO cmd;
	int length, status;

	memset( &cmd, 0, sizeof( COMMAND_INFO ) );

	/* Get the messge information from the header */
	getMessageType( buffer, cmd.type, cmd.flags,
					cmd.noArgs, cmd.noStrArgs );
	length = getMessageLength( buffer + COMMAND_WORDSIZE );
	if( cmd.type == DBX_COMMAND_OPEN )
		{
		NET_CONNECT_INFO connectInfo;
		BYTE *bufPtr = buffer + COMMAND_FIXED_DATA_SIZE + COMMAND_WORDSIZE;
		int nameLen;

		/* Get the length of the server name and null-terminate it */
		nameLen = getMessageWord( bufPtr );
		bufPtr += COMMAND_WORDSIZE;
		bufPtr[ nameLen ] = '\0';

		/* Connect to the plugin */
		initNetConnectInfo( &connectInfo, DEFAULTUSER_OBJECT_HANDLE,
							CRYPT_ERROR, CRYPT_ERROR, NET_OPTION_HOSTNAME );
		connectInfo.name = bufPtr;
		status = sNetConnect( &dbmsInfo->stream, STREAM_PROTOCOL_TCPIP,
							  &connectInfo, dbmsInfo->errorMessage,
							  &dbmsInfo->errorCode );
		if( cryptStatusError( status ) )
			{
			netEncodeError( buffer, status );
			return;
			}
		}

	/* Send the command to the plugin and read back the response */
	status = swrite( &dbmsInfo->stream, buffer,
					 COMMAND_FIXED_DATA_SIZE + COMMAND_WORDSIZE + length );
	if( cryptStatusOK( status ) )
		status = sread( &dbmsInfo->stream, buffer, COMMAND_FIXED_DATA_SIZE );
	if( !cryptStatusError( status ) )
		{
		/* Perform a consistency check on the returned data */
		getMessageType( buffer, cmd.type, cmd.flags,
						cmd.noArgs, cmd.noStrArgs );
		length = getMessageLength( buffer + COMMAND_WORDSIZE );
		if( !dbxCheckCommandInfo( &cmd, length ) || \
			cmd.type != COMMAND_RESULT )
			status = CRYPT_ERROR_BADDATA;
		}
	if( !cryptStatusError( status ) )
		/* Read the rest of the message */
		status = sread( &dbmsInfo->stream, buffer + COMMAND_FIXED_DATA_SIZE,
						length );

	/* If it's a close command, terminate the connection to the plugin.  We
	   don't do any error checking once we get this far since there's not
	   much that we can still do at this point */
	if( cmd.type == DBX_COMMAND_CLOSE )
		sNetDisconnect( &dbmsInfo->stream );
	else
		if( cryptStatusError( status ) )
			netEncodeError( buffer, status );
	}
#else

int initDispatchNet( DBMS_INFO *dbmsInfo )
	{
	return( CRYPT_ERROR );
	}
#endif /* USE_RPCAPI */

#endif /* USE_DATABASE_PLUGIN */

/****************************************************************************
*																			*
*							Database RPC Routines							*
*																			*
****************************************************************************/

/* Dispatch functions for various database types.  ODBC is the native keyset
   for Windows and (if possible) Unix, a cryptlib-native plugin is the
   fallback for Unix, and the rest are only accessible via database network
   plugins */

#ifdef USE_ODBC
  void odbcProcessCommand( void *stateInfo, BYTE *buffer );
  #define initDispatchODBC( dbmsInfo ) \
		  ( dbmsInfo->dispatchFunction = odbcProcessCommand ) != NULL
#else
  #define initDispatchODBC( dbmsInfo )		CRYPT_ERROR
#endif /* USE_ODBC */
#if defined( USE_DATABASE )
  void databaseProcessCommand( void *stateInfo, BYTE *buffer );
  #define initDispatchDatabase( dbmsInfo ) \
		  ( dbmsInfo->dispatchFunction = databaseProcessCommand ) != NULL
#else
  #define initDispatchDatabase( dbmsInfo )	CRYPT_ERROR
#endif /* General database interface */
#ifdef USE_DATABASE_PLUGIN
  int initDispatchNet( DBMS_INFO *dbmsInfo );
#else
  #define initDispatchNet( dbmsInfo )		CRYPT_ERROR
#endif /* USE_DATABASE_PLUGIN */

/* Make sure that we can fit the largest possible SQL query into the RPC
   buffer */

#if MAX_SQL_QUERY_SIZE + 256 >= DBX_IO_BUFSIZE
  #error Database RPC buffer size is too small, increase DBX_IO_BUFSIZE and rebuild
#endif /* SQL query size larger than RPC buffer size */

/* Dispatch data to the back-end */

static int dispatchCommand( COMMAND_INFO *cmd, void *stateInfo,
							DISPATCH_FUNCTION dispatchFunction )
	{
	COMMAND_INFO sentCmd = *cmd;
	BYTE buffer[ DBX_IO_BUFSIZE + 8 ], *bufPtr = buffer;
	BYTE header[ COMMAND_FIXED_DATA_SIZE + 8 ];
	const int payloadLength = ( cmd->noArgs * COMMAND_WORDSIZE ) + \
							  ( cmd->noStrArgs * COMMAND_WORDSIZE ) + \
							  cmd->strArgLen[ 0 ] + cmd->strArgLen[ 1 ] + \
							  cmd->strArgLen[ 2 ];
	long resultLength;
	int i;

	assert( payloadLength + 32 < DBX_IO_BUFSIZE );
	assert( dispatchFunction != NULL );

	/* Clear the return value */
	memset( cmd, 0, sizeof( COMMAND_INFO ) );

	/* Write the header and message fields to the buffer */
	putMessageType( bufPtr, sentCmd.type, sentCmd.flags,
					sentCmd.noArgs, sentCmd.noStrArgs );
	putMessageLength( bufPtr + COMMAND_WORDSIZE, payloadLength );
	bufPtr += COMMAND_FIXED_DATA_SIZE;
	for( i = 0; i < sentCmd.noArgs; i++ )
		{
		putMessageWord( bufPtr, sentCmd.arg[ i ] );
		bufPtr += COMMAND_WORDSIZE;
		}
	for( i = 0; i < sentCmd.noStrArgs; i++ )
		{
		const int argLength = sentCmd.strArgLen[ i ];

		putMessageWord( bufPtr, argLength );
		if( argLength > 0 )
			memcpy( bufPtr + COMMAND_WORDSIZE, sentCmd.strArg[ i ],
					argLength );
		bufPtr += COMMAND_WORDSIZE + argLength;
		}

	/* Send the command to the server and read back the server's message
	   header */
	dispatchFunction( stateInfo, buffer );
	memcpy( header, buffer, COMMAND_FIXED_DATA_SIZE );

	/* Process the fixed message header and make sure that it's valid */
	getMessageType( header, cmd->type, cmd->flags,
					cmd->noArgs, cmd->noStrArgs );
	resultLength = getMessageLength( header + COMMAND_WORDSIZE );
	if( !dbxCheckCommandInfo( cmd, resultLength ) || \
		cmd->type != COMMAND_RESULT )
		return( CRYPT_ERROR );
	if( ( cmd->noStrArgs && cmd->strArgLen[ 0 ] ) && \
		( sentCmd.type != DBX_COMMAND_QUERY && \
		  sentCmd.type != DBX_COMMAND_GETERRORINFO ) )
		/* Only these commands can return data */
		return( CRYPT_ERROR );

	/* Read the rest of the server's message */
	bufPtr = buffer + COMMAND_FIXED_DATA_SIZE;
	for( i = 0; i < cmd->noArgs; i++ )
		{
		cmd->arg[ i ] = getMessageWord( bufPtr );
		bufPtr += COMMAND_WORDSIZE;
		}
	for( i = 0; i < cmd->noStrArgs; i++ )
		{
		cmd->strArgLen[ i ] = getMessageWord( bufPtr );
		cmd->strArg[ i ] = bufPtr + COMMAND_WORDSIZE;
		bufPtr += COMMAND_WORDSIZE + cmd->strArgLen[ i ];
		}

	/* The first value returned is the status code, if it's nonzero return
	   it to the caller, otherwise move the other values down */
	if( cryptStatusError( cmd->arg[ 0 ] ) )
		return( cmd->arg[ 0 ] );
	assert( cryptStatusOK( cmd->arg[ 0 ] ) );
	for( i = 1; i < cmd->noArgs; i++ )
		cmd->arg[ i - 1 ] = cmd->arg[ i ];
	cmd->arg[ i ] = 0;
	cmd->noArgs--;

	/* Copy any string arg data back to the caller */
	if( cmd->noStrArgs && cmd->strArgLen[ 0 ] )
		{
		const int maxBufSize = ( sentCmd.type == DBX_COMMAND_QUERY ) ? \
							   MAX_QUERY_RESULT_SIZE : MAX_ERRMSG_SIZE;
		const int argIndex = sentCmd.noStrArgs;

		memcpy( sentCmd.strArg[ argIndex ], cmd->strArg[ 0 ],
				min( cmd->strArgLen[ 0 ], maxBufSize ) );
		cmd->strArg[ 0 ] = sentCmd.strArg[ argIndex ];
		}

	return( CRYPT_OK );
	}

/* Initialise query data prior to sending it to the database back-end */

static int initQueryData( COMMAND_INFO *cmd, const COMMAND_INFO *cmdTemplate,
						  BYTE *encodedDate, DBMS_INFO *dbmsInfo,
						  const char *command, const void *boundData,
						  const int boundDataLength, const time_t boundDate,
						  const int type )
	{
	int argIndex = 1;

	memcpy( cmd, cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd->arg[ 0 ] = type;
	if( command != NULL )
		{
		cmd->strArg[ 0 ] = ( void * ) command;
		cmd->strArgLen[ 0 ] = strlen( command );
		}
	if( boundDate > 0 )
		{
#ifndef SYSTEM_64BIT
		assert( sizeof( time_t ) <= 4 );
#endif /* !SYSTEM_64BIT */

		/* Encode the date as a 64-bit value */
		memset( encodedDate, 0, 8 );
#ifdef SYSTEM_64BIT
		encodedDate[ 3 ] = ( BYTE )( ( boundDate >> 32 ) & 0xFF );
#endif /* SYSTEM_64BIT */
		encodedDate[ 4 ] = ( BYTE )( ( boundDate >> 24 ) & 0xFF );
		encodedDate[ 5 ] = ( BYTE )( ( boundDate >> 16 ) & 0xFF );
		encodedDate[ 6 ] = ( BYTE )( ( boundDate >> 8 ) & 0xFF );
		encodedDate[ 7 ] = ( BYTE )( ( boundDate ) & 0xFF );
		cmd->noStrArgs++;
		cmd->strArg[ argIndex ] = encodedDate;
		cmd->strArgLen[ argIndex++ ] = 8;
		}
	if( boundData != NULL )
		{
		/* Copy the bound data into non-ephemeral storage where it'll be
		   accessible to the back-end */
		memcpy( dbmsInfo->boundData, boundData, boundDataLength );
		cmd->noStrArgs++;
		cmd->strArg[ argIndex ] = dbmsInfo->boundData;
		cmd->strArgLen[ argIndex++ ] = boundDataLength;
		}

	return( argIndex );
	}

/* Database access functions */

static int openDatabase( DBMS_INFO *dbmsInfo, const char *name,
						 const int nameLength, const int options, 
						 int *featureFlags )
	{
	static const COMMAND_INFO cmdTemplate = \
		{ DBX_COMMAND_OPEN, COMMAND_FLAG_NONE, 1, 1 };
	COMMAND_INFO cmd;
	int status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = options;
	cmd.strArg[ 0 ] = ( void * ) name;
	cmd.strArgLen[ 0 ] = nameLength;
	status = DISPATCH_COMMAND_DBX( cmdOpen, cmd, dbmsInfo );
	if( cryptStatusOK( status ) && \
		( cmd.arg[ 0 ] & DBMS_HAS_BINARYBLOBS ) )
		dbmsInfo->flags |= DBMS_FLAG_BINARYBLOBS;
	return( status );
	}

static void closeDatabase( DBMS_INFO *dbmsInfo )
	{
	static const COMMAND_INFO cmdTemplate = \
		{ DBX_COMMAND_CLOSE, COMMAND_FLAG_NONE, 0, 0 };
	COMMAND_INFO cmd;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	DISPATCH_COMMAND_DBX( cmdClose, cmd, dbmsInfo );
	}

static void performErrorQuery( DBMS_INFO *dbmsInfo )
	{
	static const COMMAND_INFO cmdTemplate = \
		{ DBX_COMMAND_GETERRORINFO, COMMAND_FLAG_NONE, 0, 1 };
	COMMAND_INFO cmd;
	int status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	/* Clear the return values */
	memset( dbmsInfo->errorMessage, 0, MAX_ERRMSG_SIZE );
	dbmsInfo->errorCode = 0;

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.strArg[ 0 ] = dbmsInfo->errorMessage;
	cmd.strArgLen[ 0 ] = 0;
	status = DISPATCH_COMMAND_DBX( cmdGetErrorInfo, cmd, dbmsInfo );
	if( cryptStatusOK( status ) )
		{
		dbmsInfo->errorCode = cmd.arg[ 0 ];
		dbmsInfo->errorMessage[ cmd.strArgLen[ 0 ] ] = '\0';
		}
	}

static int performUpdate( DBMS_INFO *dbmsInfo, const char *command,
						  const void *boundData, const int boundDataLength,
						  const time_t boundDate,
						  const DBMS_UPDATE_TYPE updateType )
	{
	static const COMMAND_INFO cmdTemplate = \
		{ DBX_COMMAND_UPDATE, COMMAND_FLAG_NONE, 1, 1 };
	COMMAND_INFO cmd;
	BYTE encodedDate[ 8 + 8 ];
	int status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( updateType > DBMS_UPDATE_NONE && \
			updateType < DBMS_UPDATE_LAST );

	/* If we're trying to abort a transaction that was never begun, don't
	   do anything */
	if( updateType == DBMS_UPDATE_ABORT && \
		!( dbmsInfo->flags & DBMS_FLAG_UPDATEACTIVE ) )
		return( CRYPT_OK );

	/* Dispatch the command */
	initQueryData( &cmd, &cmdTemplate, encodedDate, dbmsInfo, command,
				   boundData, boundDataLength, boundDate, updateType );
	status = DISPATCH_COMMAND_DBX( cmdUpdate, cmd, dbmsInfo );
	if( cryptStatusError( status ) )
		performErrorQuery( dbmsInfo );
	else
		{
		/* If we're starting or ending an update, record the update state */
		if( updateType == DBMS_UPDATE_BEGIN )
			dbmsInfo->flags |= DBMS_FLAG_UPDATEACTIVE;
		if( updateType == DBMS_UPDATE_COMMIT || \
			updateType == DBMS_UPDATE_ABORT )
			dbmsInfo->flags &= ~DBMS_FLAG_UPDATEACTIVE;
		}
	return( status );
	}

static int performStaticUpdate( DBMS_INFO *dbmsInfo, const char *command )
	{
	return( performUpdate( dbmsInfo, command, NULL, 0, 0,
						   DBMS_UPDATE_NORMAL ) );
	}

static int performQuery( DBMS_INFO *dbmsInfo, const char *command,
						 char *data, int *dataLength, const char *queryData,
						 const int queryDataLength, const time_t queryDate,
						 const DBMS_CACHEDQUERY_TYPE queryEntry,
						 const DBMS_QUERY_TYPE queryType )
	{
	static const COMMAND_INFO cmdTemplate = \
		{ DBX_COMMAND_QUERY, COMMAND_FLAG_NONE, 2, 1 };
	COMMAND_INFO cmd;
	BYTE encodedDate[ 8 + 8 ];
	int argIndex, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( ( data == NULL && dataLength == NULL ) || \
			isWritePtr( data, 16 ) );
	assert( ( queryData == NULL && queryDataLength == 0 ) || \
			( queryDataLength > 0 && \
			  isReadPtr( queryData, queryDataLength ) ) );
	assert( queryEntry >= DBMS_CACHEDQUERY_NONE && \
			queryEntry < DBMS_CACHEDQUERY_LAST );
	assert( queryType > DBMS_QUERY_NONE && queryType < DBMS_QUERY_LAST );

	/* Additional state checks: If we're starting a new query or performing
	   a point query there can't already be one active, and if we're
	   continuing or cancelling an existing query there has to be one
	   already active */
	if( ( ( queryType == DBMS_QUERY_START || \
			queryType == DBMS_QUERY_CHECK || \
			queryType == DBMS_QUERY_NORMAL ) && \
		  ( dbmsInfo->flags & DBMS_FLAG_QUERYACTIVE ) ) ||
		( ( queryType == DBMS_QUERY_CONTINUE || \
			queryType == DBMS_QUERY_CANCEL ) && \
		  !( dbmsInfo->flags & DBMS_FLAG_QUERYACTIVE ) ) )
		retIntError();

	/* Clear return value */
	if( data != NULL )
		{
		memset( data, 0, 16 );
		*dataLength = 0;
		}

	/* Dispatch the command */
	argIndex = initQueryData( &cmd, &cmdTemplate, encodedDate, dbmsInfo,
							  command, queryData, queryDataLength,
							  queryDate, queryType );
	cmd.arg[ 1 ] = queryEntry;
	cmd.strArg[ argIndex ] = data;
	cmd.strArgLen[ argIndex ] = 0;
	cmd.noStrArgs = argIndex + 1;
	status = DISPATCH_COMMAND_DBX( cmdQuery, cmd, dbmsInfo );
	if( cryptStatusError( status ) )
		{
		performErrorQuery( dbmsInfo );
		return( status );
		}

	/* Update the state information based on the query that we've just
	   performed */
	if( queryType == DBMS_QUERY_START  )
		dbmsInfo->flags |= DBMS_FLAG_QUERYACTIVE;
	if( queryType == DBMS_QUERY_CANCEL )
		dbmsInfo->flags &= ~DBMS_FLAG_QUERYACTIVE;
	if( dataLength != NULL )
		{
		*dataLength = cmd.strArgLen[ argIndex ];
		if( *dataLength <= 0 || *dataLength > MAX_QUERY_RESULT_SIZE )
			{
			assert( NOTREACHED );
			memset( data, 0, 16 );
			*dataLength = 0;
			return( CRYPT_ERROR_BADDATA );
			}
		}
	return( CRYPT_OK );
	}

static int performStaticQuery( DBMS_INFO *dbmsInfo, const char *command,
							   const DBMS_CACHEDQUERY_TYPE queryEntry,
							   const DBMS_QUERY_TYPE queryType )
	{
	return( performQuery( dbmsInfo, command, NULL, NULL, NULL, 0, 0,
						  queryEntry, queryType ) );
	}
#endif /* USE_DBMS */
