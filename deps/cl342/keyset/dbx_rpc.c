/****************************************************************************
*																			*
*						 cryptlib Database RPC Interface					*
*						Copyright Peter Gutmann 1997-2004					*
*																			*
****************************************************************************/

/* This file isn't a standalone module but is meant to be #included into
   whichever of the database client files that it's used with */

#ifdef USE_RPCAPI

/* Set up query data so it can be sent to the database back-end */

static void extractQueryData( COMMAND_INFO *cmd, time_t *timeValuePtrPtr,
							  void **dataValuePtrPtr,
							  int *dataValueLengthPtr )
	{
	/* Clear return values */
	*timeValuePtrPtr = 0;
	*dataValuePtrPtr = NULL;
	*dataValueLengthPtr = 0;

	/* If there's only one string arg present (the command), we're done */
	if( cmd->noStrArgs < 2 )
		return;

	/* If one of the string args is a bound date, convert it into a time_t */
	if( cmd->strArgLen[ 1 ] == 8 )
		{
		const BYTE *timeValuePtr = cmd->strArg[ 1 ];

		/* Extract the time_t from the 64-bit time value */
		*timeValuePtrPtr = ( time_t ) timeValuePtr[ 4 ] << 24 | \
						   ( time_t ) timeValuePtr[ 5 ] << 16 | \
						   ( time_t ) timeValuePtr[ 6 ] << 8 | \
						   ( time_t ) timeValuePtr[ 7 ];
#ifdef SYSTEM_64BIT
		*timeValuePtrPtr |= ( time_t ) timeValuePtr[ 3 ] << 32;
#endif /* SYSTEM_64BIT */

		/* Since the first arg is the date, the data (if any) will be in
		   the second arg */
		if( cmd->strArgLen[ 2 ] > 0 )
			{
			*dataValuePtrPtr = cmd->strArg[ 2 ];
			*dataValueLengthPtr = cmd->strArgLen[ 2 ];
			}

		return;
		}

	/* There's only one string arg present, which is the data */
	if( cmd->strArgLen[ 1 ] > 0 )
		{
		*dataValuePtrPtr = cmd->strArg[ 1 ];
		*dataValueLengthPtr = cmd->strArgLen[ 1 ];
		}
	}

/* Handlers for the various commands */

int cmdClose( void *stateInfo, COMMAND_INFO *cmd )
	{
	assert( cmd->type == DBX_COMMAND_CLOSE );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 0 );
	assert( cmd->noStrArgs == 0 );

	closeDatabase( stateInfo );
	return( CRYPT_OK );
	}

int cmdGetErrorInfo( void *stateInfo, COMMAND_INFO *cmd )
	{
	assert( cmd->type == DBX_COMMAND_GETERRORINFO );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 0 );
	assert( cmd->noStrArgs == 1 );

	performErrorQuery( stateInfo, &cmd->arg[ 0 ], cmd->strArg[ 0 ] );
	cmd->strArgLen[ 0 ] = strlen( cmd->strArg[ 0 ] );
	return( CRYPT_OK );
	}

int cmdOpen( void *stateInfo, COMMAND_INFO *cmd )
	{
	int hasBinaryBlobs, status;

	assert( cmd->type == DBX_COMMAND_OPEN );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 1 );
	assert( cmd->arg[ 0 ] >= CRYPT_KEYOPT_NONE && \
			cmd->arg[ 0 ] < CRYPT_KEYOPT_LAST );
	assert( cmd->noStrArgs == 1 );

	status = openDatabase( stateInfo, cmd->strArg[ 0 ], cmd->arg[ 0 ],
						   &hasBinaryBlobs );
	if( cryptStatusOK( status ) )
		cmd->arg[ 0 ] = hasBinaryBlobs;
	return( status );
	}

int cmdQuery( void *stateInfo, COMMAND_INFO *cmd )
	{
	const int argIndex = cmd->noStrArgs - 1;
	void *dataValue;
	time_t timeValue;
	int dataValueLength, dataLength, status;

	assert( cmd->type == DBX_COMMAND_QUERY );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 2 );
	assert( cmd->arg[ 0 ] > DBMS_QUERY_NONE && \
			cmd->arg[ 0 ] < DBMS_QUERY_LAST );
	assert( cmd->arg[ 1 ] >= DBMS_CACHEDQUERY_NONE && \
			cmd->arg[ 1 ] < DBMS_CACHEDQUERY_LAST );
	assert( cmd->noStrArgs >= 2 && cmd->noStrArgs <= 4 );

	extractQueryData( cmd, &timeValue, &dataValue, &dataValueLength );
	status = performQuery( stateInfo, cmd->strArg[ 0 ],
						   cmd->strArg[ argIndex ], &dataLength, dataValue,
						   dataValueLength, timeValue, cmd->arg[ 1 ],
						   cmd->arg[ 0 ] );
	if( cryptStatusOK( status ) )
		cmd->strArgLen[ argIndex ] = \
								( cmd->arg[ 0 ] == DBMS_QUERY_NORMAL || \
								  cmd->arg[ 0 ] == DBMS_QUERY_CONTINUE ) ? \
								dataLength : 0;
	return( status );
	}

int cmdUpdate( void *stateInfo, COMMAND_INFO *cmd )
	{
	void *dataValue;
	int dataValueLength;
	time_t timeValue;

	assert( cmd->type == DBX_COMMAND_UPDATE );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 1 );
	assert( cmd->noStrArgs >= 0 && cmd->noStrArgs <= 3 );

	extractQueryData( cmd, &timeValue, &dataValue, &dataValueLength );
	return( performUpdate( stateInfo, cmd->strArg[ 0 ],
						   dataValueLength > 0 ? dataValue : NULL,
						   dataValueLength, timeValue, cmd->arg[ 0 ] ) );
	}

/* Process a command from the client and send it to the appropriate handler */

static const COMMAND_HANDLER commandHandlers[] = {
	NULL, NULL, cmdOpen, cmdClose, cmdQuery, cmdUpdate, cmdGetErrorInfo };

void processCommand( void *stateInfo, BYTE *buffer )
	{
	COMMAND_INFO cmd = { 0 };
	BYTE header[ COMMAND_FIXED_DATA_SIZE ], *bufPtr;
	long totalLength;
	int i, status;

	/* Read the client's message header */
	memcpy( header, buffer, COMMAND_FIXED_DATA_SIZE );

	/* Process the fixed message header and make sure it's valid */
	getMessageType( header, cmd.type, cmd.flags, cmd.noArgs, cmd.noStrArgs );
	totalLength = getMessageLength( header + COMMAND_WORDSIZE );
	if( !dbxCheckCommandInfo( &cmd, totalLength ) || \
		cmd.type == COMMAND_RESULT )
		{
		assert( NOTREACHED );

		/* Return an invalid result message */
		putMessageType( buffer, COMMAND_RESULT, 0, 0, 0 );
		putMessageLength( buffer + COMMAND_WORDSIZE, 0 );
		return;
		}

	/* Read the rest of the clients message */
	bufPtr = buffer + COMMAND_FIXED_DATA_SIZE;
	for( i = 0; i < cmd.noArgs; i++ )
		{
		cmd.arg[ i ] = getMessageWord( bufPtr );
		bufPtr += COMMAND_WORDSIZE;
		}
	for( i = 0; i < cmd.noStrArgs; i++ )
		{
		cmd.strArgLen[ i ] = getMessageWord( bufPtr );
		cmd.strArg[ i ] = bufPtr + COMMAND_WORDSIZE;
		bufPtr += COMMAND_WORDSIZE + cmd.strArgLen[ i ];
		}
	if( !dbxCheckCommandConsistency( &cmd, totalLength ) )
		{
		assert( NOTREACHED );

		/* Return an invalid result message */
		putMessageType( buffer, COMMAND_RESULT, 0, 0, 0 );
		putMessageLength( buffer + COMMAND_WORDSIZE, 0 );
		return;
		}

	/* If it's a command which returns a string value, obtain the returned
	   data in the buffer.  Normally we limit the size to the maximum
	   attribute size, however encoded objects and data popped from
	   envelopes/sessions can be larger than this so we use the entire buffer
	   minus a safety margin */
	if( cmd.type == DBX_COMMAND_QUERY || \
		cmd.type == DBX_COMMAND_GETERRORINFO )
		{
		cmd.strArg[ cmd.noStrArgs ] = bufPtr;
		cmd.strArgLen[ cmd.noStrArgs ] = ( cmd.type == DBX_COMMAND_QUERY ) ? \
										 MAX_ENCODED_CERT_SIZE : MAX_ERRMSG_SIZE;
		cmd.noStrArgs++;
		}

	/* Null-terminate the first string arg if there's one present, either the
	   database name or the SQL command.  If there's something following it
	   in the buffer this is redundant (but safe) because it'll already be
	   followed by the MSB of the next string arg's length, if there's
	   nothing following it it's safe as well */
	if( cmd.type == DBX_COMMAND_OPEN || \
		( cmd.type == DBX_COMMAND_UPDATE && \
		  cmd.arg[ 0 ] != DBMS_UPDATE_ABORT ) || \
		( cmd.type == DBX_COMMAND_QUERY && \
		  ( cmd.arg[ 0 ] == DBMS_QUERY_NORMAL || \
		    cmd.arg[ 0 ] == DBMS_QUERY_CHECK || \
			cmd.arg[ 0 ] == DBMS_QUERY_START ) ) )
		( ( char * ) cmd.strArg[ 0 ] )[ cmd.strArgLen[ 0 ] ] = '\0';

	/* Process the command and copy any return information back to the
	   caller */
	status = commandHandlers[ cmd.type ]( stateInfo, &cmd );
	bufPtr = buffer;
	if( cryptStatusError( status ) )
		{
		/* The command failed, return a simple status value */
		putMessageType( bufPtr, COMMAND_RESULT, 0, 1, 0 );
		putMessageLength( bufPtr + COMMAND_WORDSIZE, COMMAND_WORDSIZE );
		putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, status );
		return;
		}
	if( cmd.type == DBX_COMMAND_OPEN )
		{
		/* Return numeric value */
		putMessageType( bufPtr, COMMAND_RESULT, 0, 2, 0 );
		putMessageLength( bufPtr + COMMAND_WORDSIZE, COMMAND_WORDSIZE * 2 );
		putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, CRYPT_OK );
		putMessageWord( bufPtr + COMMAND_WORD2_OFFSET, cmd.arg[ 0 ] );
		return;
		}
	if( cmd.type == DBX_COMMAND_QUERY )
		{
		const int argIndex = cmd.noStrArgs - 1;
		const long dataLength = cmd.strArgLen[ argIndex ];

		/* Return data and length.  In some cases (during ongoing queries
		   with no submitted SQL data) we can be called without any incoming
		   args, there's no space at the start of the shared input/output
		   buffer so we have to move the returned string back in the buffer
		   to avoid overwriting it with the other information we're about to
		   return */
		if( dataLength )
			memmove( bufPtr + COMMAND_WORD3_OFFSET,
					 cmd.strArg[ argIndex ], dataLength );
		putMessageType( bufPtr, COMMAND_RESULT, 0, 1, 1 );
		putMessageLength( bufPtr + COMMAND_WORDSIZE,
						  ( COMMAND_WORDSIZE * 2 ) + cmd.strArgLen[ argIndex ] );
		putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, CRYPT_OK );
		putMessageWord( bufPtr + COMMAND_WORD2_OFFSET, dataLength );
		return;
		}
	if( cmd.type == DBX_COMMAND_GETERRORINFO )
		{
		const long dataLength = cmd.strArgLen[ 0 ];

		/* Return data and length.  Because we were called without any
		   incoming args, there's no space at the start of the shared input/
		   output buffer so we have to move the returned string back in the
		   buffer to avoid overwriting it with the other information we're
		   about to return */
		if( dataLength )
			memmove( bufPtr + COMMAND_WORD4_OFFSET, cmd.strArg[ 0 ],
					 dataLength );
		putMessageType( bufPtr, COMMAND_RESULT, 0, 2, 1 );
		putMessageLength( bufPtr + COMMAND_WORDSIZE,
						  ( COMMAND_WORDSIZE * 2 ) + cmd.strArgLen[ 0 ] );
		putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, CRYPT_OK );
		putMessageWord( bufPtr + COMMAND_WORD2_OFFSET, cmd.arg[ 0 ] );
		putMessageWord( bufPtr + COMMAND_WORD3_OFFSET, dataLength );
		return;
		}
	putMessageType( bufPtr, COMMAND_RESULT, 0, 1, 0 );
	putMessageLength( bufPtr + COMMAND_WORDSIZE, COMMAND_WORDSIZE );
	putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, CRYPT_OK );
	}
#endif /* USE_RPCAPI */
