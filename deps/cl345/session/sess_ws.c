/****************************************************************************
*																			*
*					  cryptlib Session WebSockets Routines					*
*						Copyright Peter Gutmann 2015-2017					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "session.h"
  #include "ssl.h"
  #include "websockets.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "session/session.h"
  #include "session/ssl.h"
  #include "session/websockets.h"
#endif /* Compiler-specific includes */

#ifdef USE_WEBSOCKETS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the WebSockets state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckSessionSSLWS( IN const SESSION_INFO *sessionInfoPtr )
	{
	const SSL_WS_INFO *wsInfo = &sessionInfoPtr->sessionSSL->wsInfo;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( wsInfo, sizeof( SSL_WS_INFO ) ) );

	/* Check the SSL session state */
	if( !sanityCheckSessionSSL( sessionInfoPtr ) )
		{
		DEBUG_PUTS(( "sanityCheckSessionSSLWS: SSL state check" ));
		return( FALSE );
		}

	/* Check header information */
	if( wsInfo->headerBytesRequired < 0 || \
		wsInfo->headerBytesRequired > SSL_WS_BUFSIZE || \
		wsInfo->headerBufPos < 0 || \
		wsInfo->headerBufPos > wsInfo->headerBytesRequired )
		{
		DEBUG_PUTS(( "sanityCheckSessionSSLWS: Header info" ));
		return( FALSE );
		}

	/* Check miscellaneous metadata */
	if( wsInfo->maskPos < 0 || wsInfo->maskPos > WS_MASK_SIZE - 1 || \
		wsInfo->totalLength < 0 || wsInfo->totalLength >= MAX_BUFFER_SIZE || \
		( wsInfo->sendPong != TRUE && wsInfo->sendPong != FALSE ) || \
		( wsInfo->sendClose != TRUE && wsInfo->sendClose != FALSE ) )
		{
		DEBUG_PUTS(( "sanityCheckSessionSSLWS: Metadata" ));
		return( FALSE );
		}

	return( TRUE );
	}

/* Initialise an HTTP virtual stream layered on top of the TLS session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initVirtualStream( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT STREAM *stream )
	{
	NET_CONNECT_INFO connectInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheckSessionSSLWS( sessionInfoPtr ) );

	/* Initialise the virtual stream information */
	initNetConnectInfo( &connectInfo, sessionInfoPtr->ownerHandle,
						sessionInfoPtr->readTimeout, 
						sessionInfoPtr->connectTimeout,
						NET_OPTION_VIRTUAL );
	if( !isServer( sessionInfoPtr ) )
		{
		const ATTRIBUTE_LIST *attributeListPtr;

		/* Add the server name, whose presence has been checked for
		   earlier */
		attributeListPtr = findSessionInfo( sessionInfoPtr,
											CRYPT_SESSINFO_SERVER_NAME );
		ENSURES( attributeListPtr != NULL );
		connectInfo.name = attributeListPtr->value;
		connectInfo.nameLength = attributeListPtr->valueLength;
		}
	connectInfo.port = 443;		/* Dummy port value */
	FNPTR_SET( connectInfo.virtualGetDataFunction, getSessionData );
	FNPTR_SET( connectInfo.virtualPutDataFunction, putSessionData );
	FNPTR_SET( connectInfo.virtualGetErrorInfoFunction, 
			   getSessionErrorInfo );
	DATAPTR_SET( connectInfo.virtualStateInfo, sessionInfoPtr );

	/* Set up the virtual stream.  Since this is a virtual stream the listen/
	   connect merely sets up the appropriate type of server or client 
	   stream, there's no actual network connection being established */
	if( isServer( sessionInfoPtr ) )
		{
		status = sNetListen( stream, STREAM_PROTOCOL_HTTP, &connectInfo, 
							 &sessionInfoPtr->errorInfo );
		}
	else
		{
		status = sNetConnect( stream, STREAM_PROTOCOL_HTTP, &connectInfo, 
							  &sessionInfoPtr->errorInfo );
		}
	if( cryptStatusOK( status ) )
		{
		status = sioctlSet( stream, STREAM_IOCTL_HTTPREQTYPES, 
							STREAM_HTTPREQTYPE_WS_UPGRADE );
		}
	return( status );
	}

#ifdef USE_ERRMSGS

/* Get a string description of WebSocket packet types, used for diagnostic 
   error messages */

CHECK_RETVAL_PTR_NONNULL \
static const char *getPacketName( IN_BYTE const int packetType )
	{
	static const OBJECT_NAME_INFO packetNameInfo[] = {
		{ WS_PACKET_TEXT, "text data" },
		{ WS_PACKET_BINARY, "binary data" },
		{ WS_PACKET_TEXT_PARTIAL, "indefinite text data" },
		{ WS_PACKET_BINARY_PARTIAL, "indefinite binary data" },
		{ WS_PACKET_CLOSE, "close" },
		{ WS_PACKET_PING, "ping" },
		{ WS_PACKET_PONG, "pong" },
		{ CRYPT_ERROR, "<Unknown type>" },
			{ CRYPT_ERROR, "<Unknown type>" }
		};

	REQUIRES_EXT( ( packetType >= 0 && packetType <= 0xFF ),
				  "<Internal error>" );

	return( getObjectName( packetNameInfo,
						   FAILSAFE_ARRAYSIZE( packetNameInfo, \
											   OBJECT_NAME_INFO ),
						   packetType ) );
	}
#endif /* USE_ERRMSGS */

#if defined( __WIN32__ ) && defined( USE_ERRMSGS ) && !defined( NDEBUG ) && \
	!defined( CONFIG_FUZZ )

/* Dump a message to disk for diagnostic purposes */

#define DEBUG_DUMP_WS( buffer, bufSize ) \
		debugDumpWS( sessionInfoPtr, buffer, bufSize )

STDC_NONNULL_ARG( ( 1 ) ) \
void debugDumpWS( const SESSION_INFO *sessionInfoPtr,
				  IN_BUFFER( bufSize ) const void *buffer, 
				  IN_LENGTH_SHORT const int bufSize )
	{
	FILE *filePtr;
	static int messageCount = 1;
	char fileName[ 1024 + 8 ];

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtrDynamic( buffer,  bufSize ) );

	if( messageCount > 20 )
		return;	/* Don't dump too many messages */
	strlcpy_s( fileName, 1024, "/tmp/" );
	sprintf_s( &fileName[ 5 ], 1024, "websockets_%02d", messageCount++ );
	strlcat_s( fileName, 1024, ".dat" );

#ifdef __STDC_LIB_EXT1__
	if( fopen_s( &filePtr, fileName, "wb" ) != 0 )
		filePtr = NULL;
#else
	filePtr = fopen( fileName, "wb" );
#endif /* __STDC_LIB_EXT1__ */
	if( filePtr != NULL )
		{
		fwrite( buffer, 1, bufSize, filePtr );
		fclose( filePtr );
		}
	}
#else
  #define DEBUG_DUMP_WS( buffer, bufSize )
#endif /* Windows debug mode only */

/****************************************************************************
*																			*
*							WebSockets Handshake Functions					*
*																			*
****************************************************************************/

/* The size of the buffer used for the virtual HTTP stream.  This value must
   be at least as large as MIN_LINEBUF_SIZE in io/http.c */

#define HTTP_BUFSIZE		512

/* Calculate the WebSockets authentication value from a given key value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int calculateAuthValue( OUT_BUFFER_FIXED( outBufMaxLength ) \
									BYTE *outBuffer, 
							   IN_LENGTH_SHORT_MIN( WS_ENCODED_AUTH_SIZE ) \
									const int outBufMaxLength,
							   OUT_LENGTH_SHORT_Z int *outLength,
							   IN_BUFFER( keyLength ) const void *key,
							   IN_LENGTH_FIXED( WS_ENCODED_KEY_SIZE ) \
									const int keyLength )
	{
	HASH_FUNCTION_ATOMIC hashFunctionAtomic;
	BYTE authValue[ CRYPT_MAX_TEXTSIZE + WS_KEY_GUID_SIZE + 8 ];
	BYTE hashValue[ CRYPT_MAX_HASHSIZE + 8 ];
	int hashSize, status;

	assert( isWritePtr( outBuffer, outBufMaxLength ) );
	assert( isWritePtr( outLength, sizeof( int ) ) );
	assert( isReadPtr( key, keyLength ) );

	REQUIRES( outBufMaxLength >= WS_ENCODED_AUTH_SIZE && \
			  outBufMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( keyLength == WS_ENCODED_KEY_SIZE );

	/* Clear return value */
	memset( outBuffer, 0, min( 16, outBufMaxLength ) );
	*outLength = 0;

	/* Concatenate the key and WebSockets GUID value, hash the resulting 
	   string with SHA-1, and base64-encode the hash to get the 
	   authentication value */
	REQUIRES( boundsCheck( keyLength, WS_KEY_GUID_SIZE, 
						   CRYPT_MAX_TEXTSIZE + WS_KEY_GUID_SIZE ) );
	memcpy( authValue, key, keyLength );
	memcpy( authValue + keyLength, WS_KEY_GUID, WS_KEY_GUID_SIZE );
	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, 
							 &hashSize );
	hashFunctionAtomic( hashValue, hashSize, authValue, 
						keyLength + WS_KEY_GUID_SIZE );
	status = base64encode( outBuffer, outBufMaxLength, outLength, 
						   hashValue, hashSize, CRYPT_CERTTYPE_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/* base64 encoding in raw format strips the padding so we need to re-
	   add the padding byte that follows the encoded data */
	REQUIRES( *outLength == WS_ENCODED_AUTH_SIZE - 1 );
	outBuffer[ *outLength ] = '=';
	*outLength += 1;
	
	ENSURES( *outLength == WS_ENCODED_AUTH_SIZE );

	return( CRYPT_OK );
	}

/* Make sure that the client and server agree on which WebSockets sub-
   protocol to use */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 6 ) ) \
static int checkProtocol( IN_BUFFER( reqProtocolLen ) \
								const BYTE *reqProtocol,
						  IN_LENGTH_TEXT const int reqProtocolLen,
						  IN_BUFFER( respProtocolLen ) \
								const BYTE *respProtocol,
						  IN_LENGTH_TEXT_Z const int respProtocolLen,
						  const BOOLEAN isServer,
						  INOUT ERROR_INFO *errorInfo )
	{
	char reqProtocolName[ CRYPT_MAX_TEXTSIZE + 1 + 8 ];
	char respProtocolName[ CRYPT_MAX_TEXTSIZE + 1 + 8 ];

	assert( isReadPtr( reqProtocol, reqProtocolLen ) );
	assert( ( !isServer && respProtocolLen == 0 ) || \
			isReadPtr( respProtocol, respProtocolLen ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( reqProtocolLen >= 1 && reqProtocolLen <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( ( !isServer && respProtocolLen == 0 ) || \
			  ( respProtocolLen >= 1 && \
				respProtocolLen <= CRYPT_MAX_TEXTSIZE ) );
	REQUIRES( isServer == TRUE || isServer == FALSE );

	/* Format the protocol names for display */
	REQUIRES( rangeCheck( reqProtocolLen, 1, CRYPT_MAX_TEXTSIZE ) );
	memcpy( reqProtocolName, reqProtocol, reqProtocolLen );
	sanitiseString( reqProtocolName, reqProtocolLen + 1, 
					reqProtocolLen ); 
	if( respProtocolLen > 0 )
		{
		REQUIRES( rangeCheck( respProtocolLen, 1, CRYPT_MAX_TEXTSIZE ) );
		memcpy( respProtocolName, respProtocol, respProtocolLen );
		sanitiseString( respProtocolName, respProtocolLen + 1, 
						respProtocolLen ); 
		}

	/* If we're the server, make sure that the client requested the protocol 
	   that we provide */
	if( isServer )
		{
		if( reqProtocolLen != respProtocolLen || \
			memcmp( reqProtocol, respProtocol, reqProtocolLen ) )
			{
			retExt( CRYPT_ERROR_INVALID,
					( CRYPT_ERROR_INVALID, errorInfo,
					  "Requested WebSockets protocol '%s' doesn't match "
					  "allowed protocol '%s'", reqProtocolName,
					  respProtocolName ) );
			}

		return( CRYPT_OK );
		}

	/* Make sure that the server returned the protocol that we requested in 
	   its response */
	if( respProtocolLen <= 0 )
		{
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, errorInfo,
				  "WebSockets server didn't acknowledge requested "
				  "protocol '%s'", reqProtocolName ) );
		}
	if( reqProtocolLen != respProtocolLen || \
		memcmp( reqProtocol, respProtocol, reqProtocolLen ) )
		{
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, errorInfo,
				  "Returned WebSockets protocol '%s' doesn't match "
				  "requested protocol '%s'", respProtocolName, 
				  reqProtocolName ) );
		}

	return( CRYPT_OK );
	}

/* Activate a WebSockets protocol layered over the base protocol for a 
   session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int activateWebSocketsClient( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const ATTRIBUTE_LIST *attributeListPtr;
	HTTP_DATA_INFO httpDataInfo;
	HTTP_REQ_INFO httpReqInfo;
	HTTP_URI_INFO httpUriInfo;
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE buffer[ SAFEBUFFER_SIZE( HTTP_BUFSIZE ) ] STACK_ALIGN_DATA;
	BYTE nonce[ WS_KEY_SIZE + 8 ], encodedNonce[ CRYPT_MAX_TEXTSIZE + 8 ]; 
	char authValue[ CRYPT_MAX_TEXTSIZE + 8 ];
	int encodedNonceLen DUMMY_INIT, authValueLength, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionSSLWS( sessionInfoPtr ) );

	memset( &httpReqInfo, 0, sizeof( HTTP_REQ_INFO ) );

	/* Fill in the WebSockets sub-protocol if available */
	attributeListPtr = findSessionInfo( sessionInfoPtr,
										CRYPT_SESSINFO_SSL_WSPROTOCOL );
	if( attributeListPtr != NULL )
		{
		ENSURES( rangeCheck( attributeListPtr->valueLength, 1,
							 CRYPT_MAX_TEXTSIZE ) );
		httpReqInfo.protocol = attributeListPtr->value;
		httpReqInfo.protocolLen = attributeListPtr->valueLength;
		}

	/* Create the WebSockets key */
	setMessageData( &msgData, nonce, WS_KEY_SIZE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	if( cryptStatusOK( status ) )
		{
		status = base64encode( encodedNonce, CRYPT_MAX_TEXTSIZE, 
							   &encodedNonceLen, nonce, WS_KEY_SIZE, 
							   CRYPT_CERTTYPE_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		httpReqInfo.auth = encodedNonce;
		httpReqInfo.authLen = encodedNonceLen;
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Layer a virtual stream over the top of the session */
	status = initVirtualStream( sessionInfoPtr, &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Send the WebSockets upgrade request to the server */
	status = initHttpInfoReqEx( &httpDataInfo, &httpReqInfo );
	ENSURES( cryptStatusOK( status ) );
	status = swrite( &stream, &httpDataInfo, sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &stream, &sessionInfoPtr->errorInfo );
		sNetDisconnect( &stream );
		return( status );
		}

	/* Read the response to the WebSockets upgrade request */
	safeBufferInit( SAFEBUFFER_PTR( buffer ), HTTP_BUFSIZE );
	status = initHttpInfoReadEx( &httpDataInfo, SAFEBUFFER_PTR( buffer ),
								 HTTP_BUFSIZE, &httpUriInfo );
	ENSURES( cryptStatusOK( status ) );
	status = sread( &stream, &httpDataInfo, sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &stream, &sessionInfoPtr->errorInfo );
		sNetDisconnect( &stream );
		return( status );
		}
	sNetDisconnect( &stream );

	/* Make sure that the auth.key is valid */
	status = calculateAuthValue( authValue, CRYPT_MAX_TEXTSIZE, 
								 &authValueLength, encodedNonce, 
								 encodedNonceLen );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( authValueLength == WS_ENCODED_AUTH_SIZE );
	if( httpUriInfo.authLen != WS_ENCODED_AUTH_SIZE || \
		memcmp( httpUriInfo.auth, authValue, authValueLength ) )
		{
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO,
				  "Returned WebSockets authentication value doesn't match "
				  "calculated value" ) );
		}

	/* If we specified a protocol in our request, make sure that the server 
	   returned the same one in its response */
	if( httpReqInfo.protocolLen > 0 )
		{
		status = checkProtocol( httpReqInfo.protocol, httpReqInfo.protocolLen,
								httpUriInfo.protocol, httpUriInfo.protocolLen,
								FALSE, SESSION_ERRINFO );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Prepare the session buffer for the upcoming WebSockets packet 
	   exchange by writing the WebSockets header to it */
	status = length = \
		writeInnerHeaderFunction( sessionInfoPtr,
								  sessionInfoPtr->sendBuffer + \
										sessionInfoPtr->sendBufPos,
								  sessionInfoPtr->sendBufSize - \
										sessionInfoPtr->sendBufPos );
	if( cryptStatusError( status ) )
		return( status );
	sessionInfoPtr->sendBufPos += length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int activateWebSocketsServer( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const ATTRIBUTE_LIST *attributeListPtr;
	HTTP_DATA_INFO httpDataInfo;
	HTTP_REQ_INFO httpReqInfo;
	HTTP_URI_INFO httpUriInfo;
	STREAM stream;
	BYTE buffer[ SAFEBUFFER_SIZE( HTTP_BUFSIZE ) ] STACK_ALIGN_DATA;
	char authValue[ CRYPT_MAX_TEXTSIZE + 8 ];
	int authValueLength, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionSSLWS( sessionInfoPtr ) );

	/* Layer a virtual stream over the top of the session */
	status = initVirtualStream( sessionInfoPtr, &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the client's WebSockets upgrade request */
	safeBufferInit( SAFEBUFFER_PTR( buffer ), HTTP_BUFSIZE );
	status = initHttpInfoReadEx( &httpDataInfo, SAFEBUFFER_PTR( buffer ),
								 HTTP_BUFSIZE, &httpUriInfo );
	ENSURES( cryptStatusOK( status ) );
	status = sread( &stream, &httpDataInfo, sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &stream, &sessionInfoPtr->errorInfo );
		sNetDisconnect( &stream );
		return( status );
		}

	/* If there's a WebSockets sub-protocol set, make sure that it matches 
	   what the client requested */
	attributeListPtr = findSessionInfo( sessionInfoPtr,
										CRYPT_SESSINFO_SSL_WSPROTOCOL );
	if( attributeListPtr != NULL )
		{
		status = checkProtocol( httpUriInfo.protocol, httpUriInfo.protocolLen,
								attributeListPtr->value, 
								attributeListPtr->valueLength, TRUE, 
								SESSION_ERRINFO );
		if( cryptStatusError( status ) )
			{
			int localStatus;

			sNetDisconnect( &stream );

			/* The client requested a different protocol than what we 
			   provide, let them know that it's not available.  There isn't 
			   any particularly good HTTP-level status code to communicate 
			   this, "Not acceptable" seems to be the least inappropriate 
			   one */
			localStatus = initHttpInfoReq( &httpDataInfo );
			ENSURES( cryptStatusOK( localStatus ) ); 
			httpDataInfo.reqStatus = 406;	/* Not acceptable */
			( void ) swrite( &sessionInfoPtr->stream, &httpDataInfo, 
							 sizeof( HTTP_DATA_INFO ) );
			return( status );
			}
		}

	/* Make sure that the auth.key is valid */
	status = calculateAuthValue( authValue, CRYPT_MAX_TEXTSIZE, 
								 &authValueLength, httpUriInfo.auth, 
								 httpUriInfo.authLen );
	if( cryptStatusError( status ) )
		{
		int localStatus;

		sNetDisconnect( &stream );

		/* The auth key isn't valid, send back the most okayest HTTP status 
		   code that exists for this situation */
		localStatus = initHttpInfoReq( &httpDataInfo );
		ENSURES( cryptStatusOK( localStatus ) ); 
		httpDataInfo.reqStatus = 403;	/* Forbidden */
		( void ) swrite( &sessionInfoPtr->stream, &httpDataInfo, 
						 sizeof( HTTP_DATA_INFO ) );
		return( status );
		}

	/* Send the WebSockets upgrade acknowledgement to the client */
	status = initHttpInfoReqEx( &httpDataInfo, &httpReqInfo );
	ENSURES( cryptStatusOK( status ) );
	httpReqInfo.auth = authValue;
	httpReqInfo.authLen = authValueLength;
	if( httpUriInfo.protocolLen > 0 )
		{
		httpReqInfo.protocol = httpUriInfo.protocol;
		httpReqInfo.protocolLen = httpUriInfo.protocolLen;
		}
	status = swrite( &stream, &httpDataInfo, sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &stream, &sessionInfoPtr->errorInfo );
		sNetDisconnect( &stream );
		return( status );
		}

	sNetDisconnect( &stream );

	/* Prepare the session buffer for the upcoming WebSockets packet 
	   exchange by writing the WebSockets header to it */
	status = length = \
		writeInnerHeaderFunction( sessionInfoPtr,
								  sessionInfoPtr->sendBuffer + \
										sessionInfoPtr->sendBufPos,
								  sessionInfoPtr->sendBufSize - \
										sessionInfoPtr->sendBufPos );
	if( cryptStatusError( status ) )
		return( status );
	sessionInfoPtr->sendBufPos += length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int closeWebSockets( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionSSLWS( sessionInfoPtr ) );

	/* If we've already closed the higher-level protocol that we're running 
	   inside then we're done */
	if( TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_SENDCLOSED ) )
		return( CRYPT_OK );

	/* At this point things get a bit tricky because we're expected to send 
	   WebSockets data over the TLS session that we're being called from.  
	   The standard putSessionData() process has the WebSockets layer modify 
	   the contents of the send buffer before it gets passed on to the TLS 
	   write code, but in this case we're being called from the 
	   closeSession() code, not the putSessionData() code, so there's no 
	   write to follow.

	   An additional complication is provided by the fact that we're running 
	   inside a TLS tunnel, for which the spec requires that when a close 
	   alert is received, "The other party MUST respond with a close_notify 
	   alert of its own and close down the connection immediately, 
	   discarding any pending writes".  The WebSockets content is presumably 
	   a pending write, so on receipt of a TLS close from the other side we 
	   need to respond with a TLS-level close rather than a WebSockets-level 
	   close.

	   This leaves a close initiated by our side.  However, since we're 
	   again running inside a TLS tunnel, our TLS close takes precedence 
	   over the other side's WebSockets close, so even if we sent a 
	   WebSockets close, the other side would have to discard their 
	   WebSockets close response and send a TLS close back.

	   If we were going to send a WebSockets level close, the following code
	   sketch outlines how it might be done */
#if 0
	SSL_WS_INFO *wsInfo = &sessionInfoPtr->sessionSSL->wsInfo;
	BYTE buffer[ WS_CLOSE_SIZE + WS_CLOSE_DATA_SIZE + 8 ];

	/* Send a Close to the other side */
	memcpy( buffer, isServer( sessionInfoPtr ) ? \
					WS_CLOSE_DATA_SERVER : WS_CLOSE_DATA_CLIENT, 
			WS_CLOSE_SIZE + WS_CLOSE_DATA_SIZE );
	if( !isServer( sessionInfoPtr ) )
		{
		int i, LOOP_ITERATOR;

		/* If we're the client then we have to mask the Close payload data 
		   before sending it */
		LOOP_SMALL( i = 0, i < WS_CLOSE_DATA_SIZE, i++ )
			{
			buffer[ WS_CLOSE_SIZE + i ] ^= wsInfo->mask[ i % WS_MASK_SIZE ];
			}
		ENSURES( LOOP_BOUND_OK );
		}

	/* If there's not enough room in the receive buffer to read at least 1K
	   of packet data then we can't try anything further */
	if( sessionInfoPtr->receiveBufSize - sessionInfoPtr->receiveBufEnd < \
		min( sessionInfoPtr->pendingPacketRemaining, 1024 ) )
		return( CRYPT_OK );

	/* If we're in the middle of reading other data then there's no hope of
	   reading back the other side's Close without first clearing all of the 
	   other data, which is unlikely to happen because the session has been 
	   closed (this can happen when the caller requests a shutdown of the 
	   session in the middle of a data transfer), in which case we just 
	   exit */
	if( sessionInfoPtr->receiveBufPos != sessionInfoPtr->receiveBufEnd )
		return( CRYPT_OK );
#endif /* 0 */

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							WebSockets Read Functions						*
*																			*
****************************************************************************/

/* Decode the WebSockets fixed-size header and length information */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processFixedHeader( INOUT SSL_WS_INFO *wsInfo,
							   INOUT ERROR_INFO *errorInfo,
							   const BOOLEAN isServer )
	{
	assert( isWritePtr( wsInfo, sizeof( SSL_WS_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( isServer == TRUE || isServer == FALSE );

	/* Decode the packet type */
	wsInfo->type = byteToInt( wsInfo->headerBuffer[ WS_TYPE_OFFSET ] );
	switch( wsInfo->type )
		{
		case WS_PACKET_TEXT:
		case WS_PACKET_BINARY:
			break;

		case WS_PACKET_TEXT_PARTIAL:
		case WS_PACKET_BINARY_PARTIAL:
			break;

		case WS_PACKET_CLOSE:
			/* Remember that we need to close our side of the connection */
			wsInfo->sendClose = TRUE;
			break;

		case WS_PACKET_PING:
			/* Remember that we need to respond with a Pong packet when 
			   possible */
			wsInfo->sendPong = TRUE;
			break;

		case WS_PACKET_PONG:
			break;

		default:
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Unknown WebSockets packet type %X encountered", 
					  wsInfo->type ) );
		}

	/* Decode the length information.  Note that what's recorded at this 
	   point as the length may be a length-of-length code indicating that 
	   the actual length follows in a later, separate length field */
	wsInfo->totalLength = \
		byteToInt( wsInfo->headerBuffer[ WS_LENGTHCODE_OFFSET ] & WS_LENGTH_MASK );
	wsInfo->headerBytesRequired = isServer ? WS_BASE_HEADER_LENGTH_CLIENT : \
											 WS_BASE_HEADER_LENGTH_SERVER;
	if( wsInfo->totalLength == WS_LENGTH_16BIT )
		wsInfo->headerBytesRequired += UINT16_SIZE;
	else
		{
		if( wsInfo->totalLength == WS_LENGTH_64BIT )
			wsInfo->headerBytesRequired += UINT64_SIZE; 
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processLength( INOUT STREAM *stream,
						  INOUT SSL_WS_INFO *wsInfo,
						  INOUT ERROR_INFO *errorInfo )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( wsInfo, sizeof( SSL_WS_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	/* Read the separate length field if there's one present */
	if( wsInfo->totalLength > WS_LENGTH_THRESHOLD )
		{
		int length, status;

		switch( wsInfo->totalLength )
			{
			case WS_LENGTH_16BIT:
				status = length = readUint16( stream );
				break;

			case WS_LENGTH_64BIT:
				/* Although this is nominally a 64-bit value, it's bounded
				   to a standard integer by the read code */
				status = length = readUint64( stream );
				break;

			default:
				retIntError();
			}
		if( cryptStatusError( status ) )	
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Invalid length value for %s WebSockets packet", 
					  getPacketName( wsInfo->type ) ) );
			}
		wsInfo->totalLength = length;
		}

	/* Make sure that the length is valid */
	if( wsInfo->totalLength < 0 || wsInfo->totalLength >= MAX_BUFFER_SIZE )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Invalid length %d for %s WebSockets packet, should be "
				  "0...%d", wsInfo->totalLength, 
				  getPacketName( wsInfo->type ), MAX_BUFFER_SIZE ) );
		}

	return( CRYPT_OK );
	}

/* Unmask WebSockets payload data */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int unmaskData( INOUT_BUFFER_FIXED( bufSize ) BYTE *buffer,
					   IN_DATALENGTH const int bufSize,
					   INOUT SSL_WS_INFO *wsInfo )
	{
	int maskIndex = wsInfo->maskPos, i, LOOP_ITERATOR;

	assert( isWritePtr( buffer, bufSize ) );
	assert( isWritePtr( wsInfo, sizeof( SSL_WS_INFO ) ) );

	REQUIRES( bufSize > 0 && bufSize < MAX_BUFFER_SIZE );

	LOOP_MAX( i = 0, i < bufSize, ( i++, maskIndex++ ) )
		buffer[ i ] ^= wsInfo->mask[ maskIndex % WS_MASK_SIZE ];
	ENSURES( LOOP_BOUND_OK );
	wsInfo->maskPos = maskIndex % WS_MASK_SIZE;

	return( CRYPT_OK );
	}

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int processPayload( INOUT_BUFFER_FIXED( bufSize ) BYTE *buffer,
						   IN_DATALENGTH_Z const int bufSize,
						   INOUT SSL_WS_INFO *wsInfo,
						   const BOOLEAN isServer )
	{
	BOOLEAN processedAllData = FALSE;
	int payloadBytes, status;

	assert( isWritePtr( buffer, bufSize ) );
	assert( isWritePtr( wsInfo, sizeof( SSL_WS_INFO ) ) );

	REQUIRES( bufSize >= 0 && bufSize < MAX_BUFFER_SIZE );
	REQUIRES( isServer == TRUE || isServer == FALSE );

	/* Determine how many bytes we still need to process, either everything 
	   still available or the remainder of the packet */
	if( wsInfo->totalLength >= bufSize )
		{
		/* The packet encompasses the remaining data */
		payloadBytes = bufSize;
		processedAllData = TRUE;
		}
	else
		{
		/* The packet ends before the buffer contents do, only process the 
		   remaining packet data */
		payloadBytes = wsInfo->totalLength;
		}

	/* Process any remaining payload data if required */
	if( payloadBytes > 0 )
		{
		/* Unmask the data if required */
		if( isServer )
			{
			status = unmaskData( buffer, payloadBytes, wsInfo );
			if( cryptStatusError( status ) )
				return( status );
			}

		/* Adjust the total length by the number of bytes processed */
		wsInfo->totalLength -= payloadBytes;
		ENSURES( wsInfo->totalLength >= 0 && \
				 wsInfo->totalLength < MAX_BUFFER_SIZE - 1 );
		}

	/* If we've processed the entire buffer contents then let the caller 
	   know via a special return code */
	return( processedAllData ? OK_SPECIAL : payloadBytes );
	}

/* Decode a WebSockets data packet inside a standard packet.  This takes 
   input data { buffer, bufSize } and processes one packet, returning the
   new end-of-data position in *bufEnd and as return status either a byte
   count of how much data was processed as part of the packet payload (with
   further data available for processing) or OK_SPECIAL to indicate that the 
   entire buffer contents were processed and no further processing is 
   necessary */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int processInnerPacketFunction( INOUT SESSION_INFO *sessionInfoPtr,
								INOUT_BUFFER( bufSize, *bufEnd ) BYTE *buffer,
								IN_DATALENGTH const int bufSize,
								OUT_DATALENGTH_Z int *bufEnd )
	{
	SSL_WS_INFO *wsInfo = &sessionInfoPtr->sessionSSL->wsInfo;
	STREAM stream;
	const BOOLEAN isServer = isServer( sessionInfoPtr ) ? TRUE : FALSE;
	const int oldHeaderBufPos = wsInfo->headerBufPos;
	int bytesCopied, bytesFromBuffer, remainingData, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( buffer, bufSize ) );
	assert( isWritePtr( bufEnd, sizeof( int ) ) );

	static_assert( WS_HEADER_LENGTH_CLIENT <= SSL_WS_BUFSIZE,
				   "WebSockets header buffer size" );

	REQUIRES( sanityCheckSessionSSLWS( sessionInfoPtr ) );
	REQUIRES( bufSize > 0 && bufSize < MAX_BUFFER_SIZE );

	/* Clear return values */
	*bufEnd = 0;

	/* If we're still in the middle of processing a packet, process it and 
	   exit.  The test for this condition is based on there being no header
	   data in the process of being read, and length information (from a
	   previous header read) being available */
	if( wsInfo->headerBytesRequired <= 0 && wsInfo->totalLength > 0 )
		{
		*bufEnd = bufSize;

		return( processPayload( buffer, bufSize, wsInfo, isServer ) );
		}

	DEBUG_DUMP_WS( buffer, bufSize );

	/* Read as much of the input data as we can into the header buffer */
	bytesCopied = min( SSL_WS_BUFSIZE - wsInfo->headerBufPos, bufSize );
	REQUIRES( boundsCheckZ( wsInfo->headerBufPos, bytesCopied, 
							SSL_WS_BUFSIZE ) );
	memcpy( wsInfo->headerBuffer + wsInfo->headerBufPos, buffer, 
			bytesCopied );
	wsInfo->headerBufPos += bytesCopied;

	/* If this is the start of a new header, process the type and 
	   length-of-length information */
	if( wsInfo->headerBytesRequired <= 0 )
		{
		/* If there isn't at least a type and length-code field available 
		   then we can't go any further */
		if( wsInfo->headerBufPos < 1 + 1 )
			{
			*bufEnd = 0;
			return( OK_SPECIAL );	/* All data consumed */
			}

		status = processFixedHeader( wsInfo, SESSION_ERRINFO, isServer );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If there's not enough data present to continue, we're done */
	if( wsInfo->headerBufPos < wsInfo->headerBytesRequired )
		{
		*bufEnd = 0;
		return( OK_SPECIAL );		/* All data consumed */
		}

	/* We've got all the data we need to process the header, remember how 
	   many bytes we took from the buffer */
	bytesFromBuffer = wsInfo->headerBytesRequired - oldHeaderBufPos;

	sMemConnect( &stream, wsInfo->headerBuffer + WS_LENGTH_OFFSET,
				 wsInfo->headerBytesRequired );

	/* Process any additional length information */
	status = processLength( &stream, wsInfo, SESSION_ERRINFO );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	DEBUG_PRINT(( "Read %s (%X) WebSockets packet, length %ld.\n", 
				  getPacketName( wsInfo->type ), wsInfo->type, 
				  wsInfo->totalLength ));

	/* If we're the server, read the client's masking information */
	if( isServer )
		{
		/* Make sure that the data sent by the client has been masked 
		   (because the spec says so, that's why) */
		if( !( wsInfo->headerBuffer[ WS_LENGTHCODE_OFFSET ] & WS_MASK_FLAG ) )
			{
			sMemDisconnect( &stream );
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Client's WebSockets data isn't masked for %s packet", 
					  getPacketName( wsInfo->type ) ) );
			}

		/* Get the mask value */
		status = sread( &stream, wsInfo->mask, WS_MASK_SIZE );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		wsInfo->maskPos = 0;
		}

	sMemDisconnect( &stream );

	/* Move any remaining data down in the input buffer */
	remainingData = bufSize - bytesFromBuffer;
	if( remainingData > 0 )
		{
		REQUIRES( boundsCheck( bytesFromBuffer, remainingData, bufSize ) );
		memmove( buffer, buffer + bytesFromBuffer, remainingData );
		}
	*bufEnd = remainingData;

	/* We've finished processing the header, reset the header information */
	memset( wsInfo->headerBuffer, 0, SSL_WS_BUFSIZE );
	wsInfo->headerBufPos = wsInfo->headerBytesRequired = 0;

	/* Process the remaning data in the buffer */
	return( processPayload( buffer, remainingData, wsInfo, isServer ) );
	}

/****************************************************************************
*																			*
*							WebSockets Write Functions						*
*																			*
****************************************************************************/

/* Send additional out-of-band data packets inside a WebSockets stream */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int sendExtraPacketData( INOUT_BUFFER_FIXED( bufSize ) BYTE *buffer,
								IN_DATALENGTH const int bufSize,
								INOUT SSL_WS_INFO *wsInfo,
								const BOOLEAN isServer )
	{
	int length = 0;

	assert( isWritePtr( buffer, bufSize ) );
	assert( isWritePtr( wsInfo, sizeof( SSL_WS_INFO ) ) );

	REQUIRES( bufSize > 0 && bufSize < MAX_BUFFER_SIZE );
	REQUIRES( isServer == TRUE || isServer == FALSE );

	/* Send a Pong to the other side if required.  The spec is unclear over
	   whether it's necessary to send a Pong if a Close is also being sent,
	   since it doesn't make much sense to send one if we've closing the
	   channel we only send one if there's no Close pending */
	if( wsInfo->sendPong && !wsInfo->sendClose )
		{
		/* If there's no room for a Pong packet, return */
		if( WS_PONG_SIZE > bufSize )
			return( 0 );

		/* Send an almighty Pong to the other side */
		REQUIRES( rangeCheck( WS_PONG_SIZE, 1, bufSize ) );
		memcpy( buffer, 
				isServer ? WS_PONG_DATA_SERVER : WS_PONG_DATA_CLIENT, 
				WS_PONG_SIZE );
		length += WS_PONG_SIZE;
		wsInfo->sendPong = FALSE;
		}

	/* Send a close to the other side if required */
	if( wsInfo->sendClose )
		{
		/* If there's no room for a Close packet, return */
		if( length + WS_CLOSE_SIZE + WS_CLOSE_DATA_SIZE > bufSize )
			return( 0 );

		/* Send a Close to the other side */
		REQUIRES( rangeCheck( length + WS_CLOSE_SIZE + WS_CLOSE_DATA_SIZE, 1, 
							  bufSize ) );
		memcpy( buffer + length, 
				isServer ? WS_CLOSE_DATA_SERVER : WS_CLOSE_DATA_CLIENT, 
				WS_CLOSE_SIZE + WS_CLOSE_DATA_SIZE );
		if( !isServer )
			{
			int i, LOOP_ITERATOR;

			/* If we're the client then we have to mask the Close payload 
			   data before sending it */
			LOOP_SMALL( i = 0, i < WS_CLOSE_DATA_SIZE, i++ )
				{
				buffer[ length + WS_CLOSE_SIZE + i ] ^= \
								wsInfo->mask[ i % WS_MASK_SIZE ];
				}
			ENSURES( LOOP_BOUND_OK );
			}
		length += WS_CLOSE_SIZE + WS_CLOSE_DATA_SIZE;
		wsInfo->sendClose = FALSE;
		}

	return( length );
	}

/* Prepare a WebSockets data packet inside a standard packet */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeInnerHeaderFunction( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT_BUFFER_FIXED( bufSize ) BYTE *buffer,
							  IN_DATALENGTH const int bufSize )
	{
	STREAM stream;
	const int headerSize = isServer( sessionInfoPtr ) ? 
						   WS_HEADER_LENGTH_SERVER : WS_HEADER_LENGTH_CLIENT;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( buffer, bufSize ) );

	REQUIRES( sanityCheckSessionSSLWS( sessionInfoPtr ) );
	REQUIRES( bufSize >= headerSize && bufSize < MAX_BUFFER_SIZE );

	/* Write a WebSockets header.  We always write a 16-bit length since 
	   this is the most likely value, it can't be longer than 16 bits due to 
	   the TLS packet size limit, and if it's less it'll only be a hundred
	   or so bytes so we move it down when we wrap the packet to get an 
	   8-bit length */
	sMemOpen( &stream, buffer, headerSize );
	sputc( &stream, WS_PACKET_BINARY );
	if( isServer( sessionInfoPtr ) )
		sputc( &stream, WS_LENGTH_16BIT );
	else
		sputc( &stream, WS_MASK_FLAG | WS_LENGTH_16BIT );
	status = writeUint16( &stream, 0 );		/* Placeholder */
	if( !isServer( sessionInfoPtr ) )
		status = swrite( &stream, WS_MASK_VALUE, WS_MASK_SIZE );
	ENSURES( cryptStatusOK( status ) );
	length = stell( &stream );
	sMemDisconnect( &stream );

	return( length );
	}

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2 ) ) \
int prepareInnerPacketFunction( INOUT SESSION_INFO *sessionInfoPtr,
								INOUT_BUFFER_FIXED( bufSize ) BYTE *buffer,
								IN_DATALENGTH const int bufSize,
								IN_DATALENGTH const int dataSize )
	{
	SSL_WS_INFO *wsInfo = &sessionInfoPtr->sessionSSL->wsInfo;
	STREAM stream;
	const BYTE *maskPtr = buffer + WS_MASK_OFFSET;
	const BOOLEAN isServer = isServer( sessionInfoPtr ) ? TRUE : FALSE;
	int headerSize = isServer ? WS_HEADER_LENGTH_SERVER : \
								WS_HEADER_LENGTH_CLIENT;
	const int payloadSize = dataSize - headerSize;
	int extraDataLength = 0, i, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( buffer, bufSize ) );

	REQUIRES( sanityCheckSessionSSLWS( sessionInfoPtr ) );
	REQUIRES( bufSize >= headerSize && bufSize < MAX_BUFFER_SIZE );
	REQUIRES( dataSize >= headerSize && dataSize <= bufSize );
	REQUIRES( isIntegerRange( payloadSize ) );

	/* Add the length field as required */
	if( payloadSize > WS_LENGTH_THRESHOLD )
		{
		/* If there are more than WS_LENGTH_THRESHOLD bytes in the buffer 
		   then we can write the length field directly into the header */
		sMemOpen( &stream, buffer + WS_LENGTH_OFFSET, UINT16_SIZE );
		status = writeUint16( &stream, payloadSize );
		sMemDisconnect( &stream );
		ENSURES( cryptStatusOK( status ) );
		}
	else
		{
		const int bytesToMove = isServer? payloadSize : \
										  WS_MASK_SIZE + payloadSize;

		/* There are WS_LENGTH_THRESHOLD bytes or less in the buffer so the
		   length can be encoded as part of the header, move the payload 
		   data down and insert the length byte at the appropriate 
		   location */
		REQUIRES( boundsCheck( WS_LENGTH_OFFSET + UINT16_SIZE, bytesToMove, 
							   dataSize ) );
		memmove( buffer + WS_LENGTH_OFFSET, 
				 buffer + WS_LENGTH_OFFSET + UINT16_SIZE, bytesToMove );
		buffer[ WS_LENGTHCODE_OFFSET ] = intToByte( payloadSize );
		if( !isServer )
			buffer[ WS_LENGTHCODE_OFFSET ] |= WS_MASK_FLAG;

		/* Since we've just eliminated the length field, we need to adjust 
		   various values to match */
		maskPtr -= UINT16_SIZE;
		headerSize -= UINT16_SIZE;
		}

	/* If we're the client then we have to mask the payload */
	if( !isServer )
		{
		BYTE *payloadPtr = buffer + headerSize;

		/* Mask the payload contents */
		LOOP_MAX( i = 0, i < payloadSize, i++ )
			payloadPtr[ i ] ^= maskPtr[ i % WS_MASK_SIZE ];
		ENSURES( LOOP_BOUND_OK );
		}

	/* Finally, if there's a response to a recent out-of-band packet 
	   required, append it to the packet data.  This is more or less the 
	   best that we can do in terms of handling out-of-band chatter, we can
	   only send data to the peer when the caller initiates it, so 
	   piggybacking the out-of-band data on a standard packet write is the
	   best strategy for dealing with this.  Luckily the spec accommodates
	   this in the case of the Ping packet by not requiring an immediate 
	   response to a Ping, allowing the receiver to collapse multiple Pings 
	   into a single response, and instructing the peer to ignore 
	   unsolicited Pongs, which our response may seem to be if it's delayed 
	   too long */
	if( ( wsInfo->sendPong || wsInfo->sendClose ) && \
		( headerSize + payloadSize < bufSize ) )
		{
		int length;

		status = length = sendExtraPacketData( buffer + headerSize + payloadSize,
											   bufSize - ( headerSize + payloadSize ),
											   wsInfo, isServer );
		if( cryptStatusError( status ) )
			return( status );
		extraDataLength += length;
		}

	return( headerSize + payloadSize + extraDataLength );
	}

/****************************************************************************
*																			*
*							Sub-protocol Access Routines					*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setSubprotocolWebSockets( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Set the access method pointers */
	if( isServer( sessionInfoPtr ) )
		{
		FNPTR_SET( sessionInfoPtr->activateInnerSubprotocolFunction, 
				   activateWebSocketsServer );
		}
	else
		{
		FNPTR_SET( sessionInfoPtr->activateInnerSubprotocolFunction, 
				   activateWebSocketsClient );
		}
	FNPTR_SET( sessionInfoPtr->closeInnerSubprotocolFunction, 
			   closeWebSockets );

	return( CRYPT_OK );
	}
#endif /* USE_WEBSOCKETS */
