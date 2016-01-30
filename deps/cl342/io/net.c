/****************************************************************************
*																			*
*						Network Stream I/O Functions						*
*						Copyright Peter Gutmann 1993-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "stream_int.h"
#else
  #include "io/stream_int.h"
#endif /* Compiler-specific includes */

/* When we allocate the readahead/write buffers for the network transport 
   (see the comment at the start of net_trans.c) we try and make them an
   optimal size to minimise unnecessary copying and not negatively affect
   network I/O.  If we make them too big then we'll have to move too much 
   data around when we partially empty them, if we make them too small then 
   the buffering effect is suboptimal.  Since what we're buffering is PKI
   traffic a 4K buffer should get most messages in one go.  This also
   matches many network stacks that use 4K I/O buffers, the BSD default */

#define NETWORK_BUFFER_SIZE		4096
#if NETWORK_BUFFER_SIZE > MAX_INTLENGTH_SHORT
  #error NETWORK_BUFFER_SIZE exceeds buffered I/O length check size
#endif /* NETWORK_BUFFER_SIZE > MAX_INTLENGTH_SHORT */

#ifdef USE_TCP

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#if defined( __WIN32__ ) && !defined( NDEBUG ) && 0

/* Code to test HTTP header parsing, call from just before the 
   openConnection() call in completeConnect() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int transportFileFunction( INOUT STREAM *stream, 
								  IN_BUFFER( length ) void *buffer, 
								  IN_LENGTH const int length, 
								  IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	FILE *filePtr = ( FILE * ) stream->callbackFunction;

	return( fread( buffer, 1, length, filePtr ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void testHttp( INOUT STREAM *stream )
	{
	STREAM streamCopy;
	ERROR_INFO errorInfoCopy;
	FILE *reportFile = stdout;
	void *buffer;
	int i, status;

	stream->protocol = STREAM_PROTOCOL_HTTP;
	stream->transportReadFunction = transportFileFunction;
	sioctlSet( stream, STREAM_IOCTL_HTTPREQTYPES, STREAM_HTTPREQTYPE_GET );
	if( ( buffer = clAlloc( "testHTTP", 16384 ) ) == NULL )
		{
		puts( "Out of memory." );
		return;
		}
#if 1	/* Either stdout or a file */
	reportFile = fopen( "r:/http_report.txt", "w" );
	if( reportFile == NULL )
		{
		printf( "Couldn't open file for report, hit a key." );
		getchar();
		putchar( '\n' );
		exit( EXIT_FAILURE );
		}
#endif
//	for( i = 1455; i <= 2000; i++ )
//	for( i = 0; i < 1000; i++ )
//	for( i = 1000; i < 1999; i++ )
//	for( i = 2000; i < 2999; i++ )
//	for( i = 3000; i < 3999; i++ )
	for( i = 0; i <= 3965; i++ )
		{
		FILE *filePtr;
		char fileName[ 128 ];

		sprintf( fileName, "d:/tmp/testcases/%08d", i );
		filePtr = fopen( fileName, "rb" );
		if( filePtr == NULL )
			{
			printf( "Failed to open file #%d, hit a key.", i );
			getchar();
			putchar( '\n' );
			continue;
			}
		memcpy( &streamCopy, stream, sizeof( STREAM ) );
		memcpy( &errorInfoCopy, stream->errorInfo, sizeof( ERROR_INFO ) );
		stream->callbackFunction = ( CALLBACKFUNCTION ) filePtr;/* Kludge */
		fprintf( reportFile, "%04d: ", i );
		if( reportFile != stdout )
			{
			if( !( i % 10 ) )
				putchar( '\n' );
			printf( "%04d ", i );
			}
		status = sread( stream, buffer, 16384 );
		fclose( filePtr );
		if( !cryptStatusError( status ) )
			{
			fprintf( reportFile, 
					 "%d: cryptlib error: HTTP error not detected.\n", 
					 status );
			}
		else
			{
			fprintf( reportFile, "%d %s.\n", status, 
					 stream->errorInfo->errorString );
			}
		fflush( reportFile );
		memcpy( stream, &streamCopy, sizeof( STREAM ) );
		memcpy( stream->errorInfo, &errorInfoCopy, sizeof( ERROR_INFO ) );
		}
	if( reportFile != stdout )
		fclose( reportFile );
	putchar( '\n' );
	}
#else
  #define testHttp( stream )
#endif /* Win32 debug build only */

/* Copy error information from a cryptlib transport-layer session into a
   stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getSessionErrorInfo( INOUT NET_STREAM_INFO *netStream, 
								IN_ERROR const int errorStatus )
	{
	MESSAGE_DATA msgData;
	char errorString[ MAX_ERRMSG_SIZE + 8 ];
	int status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( cryptStatusError( errorStatus ) );

	clearErrorString( &netStream->errorInfo );
	setMessageData( &msgData, errorString, MAX_ERRMSG_SIZE );
	status = krnlSendMessage( netStream->iTransportSession, 
							  IMESSAGE_GETATTRIBUTE, &msgData, 
							  CRYPT_ATTRIBUTE_ERRORMESSAGE );
	if( cryptStatusOK( status ) )
		setErrorString( NETSTREAM_ERRINFO, errorString, msgData.length );

	return( errorStatus );
	}

/* Check for the use of a proxy when opening a stream */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 3, 4, 6, 8 ) ) \
static int checkForProxy( INOUT NET_STREAM_INFO *netStream, 
						  IN_ENUM( STREAM_PROTOCOL ) \
							const STREAM_PROTOCOL_TYPE protocol,
						  const NET_CONNECT_INFO *connectInfo,
						  IN_BUFFER( hostLen ) const char *host, 
						  IN_LENGTH_DNS const int hostLen,
						  OUT_BUFFER( proxyUrlMaxLen, *proxyUrlLen ) \
							char *proxyUrlBuffer, 
						  IN_LENGTH_DNS const int proxyUrlMaxLen, 
						  OUT_LENGTH_DNS_Z int *proxyUrlLen )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );
	assert( isWritePtr( proxyUrlBuffer, proxyUrlMaxLen ) );
	assert( isWritePtr( proxyUrlLen, sizeof( int ) ) );

	REQUIRES( protocol > STREAM_PROTOCOL_NONE && \
			  protocol < STREAM_PROTOCOL_LAST );
	REQUIRES( proxyUrlMaxLen > 10 && proxyUrlMaxLen <= MAX_DNS_SIZE );

	/* Clear return value */
	memset( proxyUrlBuffer, 0, min( 16, proxyUrlMaxLen ) );
	*proxyUrlLen = 0;

	/* Check for a local connection, which always bypasses the proxy.  We
	   only use the case-insensitive string compares for the text-format
	   host names since the numeric forms don't need this */
	if( ( hostLen == 9 && !memcmp( host, "127.0.0.1", 9 ) ) || \
		( hostLen == 3 && !memcmp( host, "::1", 3 ) ) || \
		( hostLen == 9 && !strCompare( host, "localhost", 9 ) ) || \
		( hostLen == 10 && !strCompare( host, "localhost.", 10 ) ) )
		/* Are you local? */
		{
		/* This is a local socket!  We'll have no proxies here! */
		return( CRYPT_OK );
		}

	/* Check to see whether we're going through a proxy.  First we check for 
	   a protocol-specific HTTP proxy (if appropriate), if there's none then 
	   we check for the more generic case of a SOCKS proxy */
	if( protocol == STREAM_PROTOCOL_HTTP )
		{
		/* Check whether there's an HTTP proxy configured */
		setMessageData( &msgData, proxyUrlBuffer, proxyUrlMaxLen );
		status = krnlSendMessage( connectInfo->iUserObject,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_OPTION_NET_HTTP_PROXY );
		if( cryptStatusOK( status ) )
			{
			netStream->nFlags |= \
				( connectInfo->options == NET_OPTION_HOSTNAME ) ? \
				STREAM_NFLAG_HTTPPROXY : STREAM_NFLAG_HTTPTUNNEL;
			*proxyUrlLen = msgData.length;

			return( OK_SPECIAL );
			}
		}

	/* Check whether there's a SOCKS proxy configured */
	setMessageData( &msgData, proxyUrlBuffer, proxyUrlMaxLen );
	status = krnlSendMessage( connectInfo->iUserObject,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_OPTION_NET_SOCKS_SERVER );
	if( cryptStatusOK( status ) )
		{
		*proxyUrlLen = msgData.length;

		return( OK_SPECIAL );
		}

	/* There's no proxy configured */
	return( CRYPT_OK );
	}

/* Connect a network stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int openNetworkConnection( INOUT NET_STREAM_INFO *netStream, 
								  IN_ENUM( NET_OPTION ) \
									const NET_OPTION_TYPE options,
								  IN_BUFFER_OPT( proxyUrlLen ) const char *proxyUrl, 
								  IN_LENGTH_DNS_Z const int proxyUrlLen )
	{
	URL_INFO urlInfo;
	char urlBuffer[ MAX_DNS_SIZE + 8 ];
	const char *url = proxyUrl;
	int urlLen = proxyUrlLen, status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( ( proxyUrl == NULL && proxyUrlLen == 0 ) || \
			isReadPtr( proxyUrl, proxyUrlLen ) );

	REQUIRES( options > NET_OPTION_NONE && options < NET_OPTION_LAST );
	REQUIRES( ( proxyUrl == NULL && proxyUrlLen == 0 ) || \
			  ( proxyUrl != NULL && \
				proxyUrlLen > 0 && proxyUrlLen <= MAX_DNS_SIZE ) );

	/* If we're using an already-active network socket supplied by the
	   user, there's nothing to do */
	if( netStream->nFlags & STREAM_NFLAG_USERSOCKET )
		{
		/* If it's a dummy open to check parameters that can't be validated
		   at a higher level pass the info on down to the low-level checking 
		   routines */
		if( options == NET_OPTION_NETWORKSOCKET_DUMMY )
			return( netStream->transportCheckFunction( netStream ) );

		return( CRYPT_OK );
		}

	/* If we're not going via a proxy, perform a direct open */
	if( proxyUrl == NULL )
		{
		return( netStream->transportConnectFunction( netStream, netStream->host, 
													 netStream->hostLen,
													 netStream->port ) );
		}

	/* We're going via a proxy, if the user has specified automatic proxy
	   detection try and locate the proxy information */
	if( !strCompareZ( proxyUrl, "[Autodetect]" ) )
		{
		status = findProxyUrl( urlBuffer, MAX_DNS_SIZE, &urlLen, 
							   netStream->host, netStream->hostLen );
		if( cryptStatusError( status ) )
			{
			/* The proxy URL was invalid, provide more information for the
			   caller */
			retExt( CRYPT_ERROR_OPEN,
					( CRYPT_ERROR_OPEN, NETSTREAM_ERRINFO, 
					  "Couldn't auto-detect HTTP proxy" ) );
			}
		url = urlBuffer;
		}

	/* Process the proxy details.  Since this is an HTTP proxy we specify 
	   the default port as port 80 */
	status = parseURL( &urlInfo, url, urlLen, 80, URL_TYPE_HTTP, FALSE );
	if( cryptStatusError( status ) )
		{
		/* The proxy URL was invalid, provide more information for the
		   caller */
		retExt( CRYPT_ERROR_OPEN,
				( CRYPT_ERROR_OPEN, NETSTREAM_ERRINFO, 
				  "Invalid HTTP proxy URL" ) );
		}

	/* Since we're going via a proxy, open the connection to the proxy
	   rather than directly to the target system.  */
	return( netStream->transportConnectFunction( netStream, urlInfo.host, 
												 urlInfo.hostLen, 
												 urlInfo.port ) );
	}

/****************************************************************************
*																			*
*					Network Stream Init/Shutdown Functions					*
*																			*
****************************************************************************/

/* Initialise the network stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int initStream( OUT STREAM *stream, 
					   OUT NET_STREAM_INFO *netStream,
					   IN_ENUM( STREAM_PROTOCOL ) \
						const STREAM_PROTOCOL_TYPE protocol,
					   INOUT const NET_CONNECT_INFO *connectInfo,
					   const BOOLEAN isServer )
	{
	int timeout;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );

	REQUIRES( protocol > STREAM_PROTOCOL_NONE && \
			  protocol < STREAM_PROTOCOL_LAST );

	/* Set up the basic network stream info */
	memset( stream, 0, sizeof( STREAM ) );
	stream->type = STREAM_TYPE_NETWORK;
	memset( netStream, 0, sizeof( NET_STREAM_INFO ) );
	netStream->protocol = protocol;
	netStream->port = connectInfo->port;
	netStream->netSocket = netStream->listenSocket = CRYPT_ERROR;
	netStream->iTransportSession = CRYPT_ERROR;
	if( isServer )
		netStream->nFlags = STREAM_NFLAG_ISSERVER;

	/* Set up the stream timeout information.  While we're connecting the 
	   stream timeout is the connect timeout.  Once we've connected it's set
	   to the data transfer timeout, so initially we set the stream timeout
	   to the connect timeout and the saved timeout to the data transfer
	   timeout */
	if( connectInfo->connectTimeout != CRYPT_ERROR )
		{
		/* There's an explicit timeout specified, use that */
		timeout = connectInfo->connectTimeout;
		}
	else
		{
		/* Get the default timeout from the user object */
		if( cryptStatusError( \
				krnlSendMessage( connectInfo->iUserObject, IMESSAGE_GETATTRIBUTE,
								 &timeout, CRYPT_OPTION_NET_CONNECTTIMEOUT ) ) )
			timeout = 30;
		}
	if( timeout < 5 )
		{
		/* Enforce the same minimum connect timeout as the kernel ACLs */
		DEBUG_DIAG(( "Timeout is < 5s" ));
		assert( DEBUG_WARN );
		timeout = 5;
		}
	netStream->timeout = timeout;
	if( connectInfo->timeout != CRYPT_ERROR )
		{
		/* There's an explicit timeout specified, use that */
		timeout = connectInfo->timeout;
		}
	else
		{
		/* Get the default timeout from the user object */
		if( cryptStatusError( \
				krnlSendMessage( connectInfo->iUserObject, IMESSAGE_GETATTRIBUTE,
								 &timeout, CRYPT_OPTION_NET_READTIMEOUT ) ) )
			timeout = 30;
		}
	netStream->savedTimeout = timeout;

	return( CRYPT_OK );
	}

/* Clean up a stream to shut it down */

STDC_NONNULL_ARG( ( 1 ) ) \
static void cleanupStream( INOUT STREAM *stream, 
						   const BOOLEAN cleanupTransport )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_V( netStream != NULL );
	REQUIRES_V( netStream->sanityCheckFunction( stream ) );

	/* Clean up the transport system if necessary */
	if( cleanupTransport && !( netStream->nFlags & STREAM_NFLAG_USERSOCKET ) )
		netStream->transportDisconnectFunction( netStream, TRUE );

	/* Clean up stream-related buffers if necessary */
	zeroise( netStream, sizeof( NET_STREAM_INFO ) + netStream->storageSize );
	clFree( "cleanupStream", netStream );

	zeroise( stream, sizeof( STREAM ) );
	}

/****************************************************************************
*																			*
*						Network Stream Connect Functions					*
*																			*
****************************************************************************/

/* Process network connect options */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
static int processConnectOptions( INOUT STREAM *stream, 
								  INOUT NET_STREAM_INFO *netStream,
								  OUT_OPT URL_INFO *urlInfo,
								  const NET_CONNECT_INFO *connectInfo,
								  INOUT ERROR_INFO *errorInfo )
	{
	const void *name = connectInfo->name;
	int nameLength = connectInfo->nameLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( ( urlInfo == NULL ) || \
			isWritePtr( urlInfo, sizeof( URL_INFO ) ) );
	assert( isReadPtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );

	REQUIRES_S( stream->type == STREAM_TYPE_NETWORK );
				/* We can't use the sanity-check function because the stream
				   hasn't been fully set up yet */
	REQUIRES_S( ( ( connectInfo->options == NET_OPTION_TRANSPORTSESSION || \
					connectInfo->options == NET_OPTION_NETWORKSOCKET || \
					connectInfo->options == NET_OPTION_NETWORKSOCKET_DUMMY ) && \
					urlInfo == NULL ) || \
				( !( netStream->nFlags & STREAM_NFLAG_ISSERVER ) && \
				  connectInfo->options == NET_OPTION_HOSTNAME && \
				  connectInfo->name != NULL && urlInfo != NULL ) || \
				( ( netStream->nFlags & STREAM_NFLAG_ISSERVER ) && \
				  connectInfo->options == NET_OPTION_HOSTNAME && \
				  connectInfo->name == NULL && \
				  ( ( connectInfo->interface == NULL && urlInfo == NULL ) || \
					( connectInfo->interface != NULL && urlInfo != NULL ) ) ) );

	/* Clear return value */
	if( urlInfo != NULL )
		memset( urlInfo, 0, sizeof( URL_INFO ) );

	/* If we're running over a cryptlib transport layer set up the 
	   transport session handle */
	if( connectInfo->options == NET_OPTION_TRANSPORTSESSION )
		{
		netStream->iTransportSession = connectInfo->iCryptSession;

		return( CRYPT_ERROR_NOTAVAIL );	/* See comment in net_trans.c */
		}

	/* If it's a user-supplied network socket remember this */
	if( connectInfo->options == NET_OPTION_NETWORKSOCKET || \
		connectInfo->options == NET_OPTION_NETWORKSOCKET_DUMMY )
		{
		netStream->netSocket = connectInfo->networkSocket;
		netStream->nFlags |= STREAM_NFLAG_USERSOCKET;

		return( CRYPT_OK );
		}

	ENSURES_S( connectInfo->options == NET_OPTION_HOSTNAME );

	REQUIRES_S( ( ( netStream->nFlags & STREAM_NFLAG_ISSERVER ) && \
				  connectInfo->name == NULL && \
				  connectInfo->nameLength == 0 ) || \
				( connectInfo->name != NULL && \
				  connectInfo->nameLength > 0 && \
				  connectInfo->nameLength < MAX_INTLENGTH_SHORT ) );

	/* If it's a server (i.e. we're opening a listen socket) then the 
	   name is the interface name to bind to, defaulting to the first
	   interface we find/localhost if none is given */
	if( netStream->nFlags & STREAM_NFLAG_ISSERVER )
		{
		if( connectInfo->interface == NULL )
			return( CRYPT_OK );
		name = connectInfo->interface;
		nameLength = connectInfo->interfaceLength;
		}
	ENSURES( urlInfo != NULL );
	ENSURES( name != NULL );

	/* Parse the URI into its various components */
	status = parseURL( urlInfo, name, nameLength, connectInfo->port,
					   ( netStream->protocol == STREAM_PROTOCOL_HTTP ) ? \
							URL_TYPE_HTTP : URL_TYPE_NONE, FALSE );
	if( cryptStatusError( status ) )
		{
		/* There's an error in the URL format, provide more information to 
		   the caller */
		retExt( CRYPT_ERROR_OPEN,
				( CRYPT_ERROR_OPEN, errorInfo, 
				  "Invalid %s name/URL", 
				  ( netStream->nFlags & STREAM_NFLAG_ISSERVER ) ? \
				  "interface" : "host" ) );
		}
	return( CRYPT_OK );
	}

/* Complete a network connection after the client- or server-specific
   portions have been handled */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 9 ) ) \
static int completeConnect( INOUT STREAM *stream,
							INOUT NET_STREAM_INFO *netStreamTemplate, 
							IN_OPT const URL_INFO *urlInfo,
							IN_ENUM( STREAM_PROTOCOL ) \
								const STREAM_PROTOCOL_TYPE protocol,
							IN_ENUM( NET_OPTION ) const NET_OPTION_TYPE options,
							IN_BUFFER_OPT( proxyUrlLen ) const char *proxyUrl, 
							IN_LENGTH_DNS_Z const int proxyUrlLen,
							IN_HANDLE const CRYPT_USER iUserObject, 
							INOUT ERROR_INFO *errorInfo )
	{
	const BOOLEAN useTransportBuffering = \
						( options == NET_OPTION_TRANSPORTSESSION || \
						  protocol == STREAM_PROTOCOL_TCPIP ) ? \
						FALSE : TRUE;
	void *netStreamData;
	int netStreamDataSize = 0, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( netStreamTemplate, sizeof( NET_STREAM_INFO ) ) );
	assert( ( urlInfo == NULL ) || \
			isReadPtr( urlInfo, sizeof( URL_INFO ) ) );
	assert( ( proxyUrl == NULL && proxyUrlLen == 0 ) || \
			isReadPtr( proxyUrl, proxyUrlLen ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES_S( stream->type == STREAM_TYPE_NETWORK );
				/* We can't use the sanity-check function because the stream
				   hasn't been fully set up yet */
	REQUIRES( urlInfo == NULL || \
			  ( urlInfo != NULL && \
				urlInfo->host != NULL && urlInfo->hostLen > 0 ) );
	REQUIRES_S( protocol > STREAM_PROTOCOL_NONE && \
				protocol < STREAM_PROTOCOL_LAST );
	REQUIRES_S( options > NET_OPTION_NONE && options < NET_OPTION_LAST );
	REQUIRES_S( ( proxyUrl == NULL && proxyUrlLen == 0 ) || \
				( proxyUrl != NULL && \
				  proxyUrlLen > 0 && proxyUrlLen <= MAX_DNS_SIZE ) );
	REQUIRES_S( ( iUserObject == DEFAULTUSER_OBJECT_HANDLE ) || \
				  isHandleRangeValid( iUserObject ) );

	/* Set up the access method pointers.  We can use either direct TCP/IP
	   access or a cryptlib stream for transport, and layered over that
	   either HTTP, the CMP socket protocol, or direct access to the
	   transport layer */
	if( options == NET_OPTION_TRANSPORTSESSION )
		setAccessMethodTransportSession( netStreamTemplate );
	else
		setAccessMethodTCP( netStreamTemplate );
	switch( protocol )
		{
		case STREAM_PROTOCOL_HTTP:
#ifdef USE_HTTP
			setStreamLayerHTTP( netStreamTemplate );
#else
			return( CRYPT_ERROR_NOTAVAIL );
#endif /* USE_HTTP */
			break;

		case STREAM_PROTOCOL_TCPIP:
			setStreamLayerDirect( netStreamTemplate );
			break;

		default:
			retIntError_Stream( stream );
		}
	setStreamLayerBuffering( netStreamTemplate, useTransportBuffering );

	ENSURES( netStreamTemplate->sanityCheckFunction != NULL );
	ENSURES( netStreamTemplate->writeFunction != NULL && \
			 netStreamTemplate->readFunction != NULL );
	ENSURES( netStreamTemplate->transportConnectFunction != NULL && \
			 netStreamTemplate->transportDisconnectFunction != NULL );
	ENSURES( netStreamTemplate->transportReadFunction != NULL && \
			 netStreamTemplate->transportWriteFunction != NULL );
	ENSURES( netStreamTemplate->transportOKFunction != NULL && \
			 netStreamTemplate->transportCheckFunction != NULL );
	ENSURES( netStreamTemplate->bufferedTransportReadFunction != NULL && \
			 netStreamTemplate->bufferedTransportWriteFunction != NULL );
	ENSURES( ( netStreamTemplate->nFlags & STREAM_NFLAG_ISSERVER ) || \
			 ( urlInfo != NULL && \
			   urlInfo->host != NULL && urlInfo->hostLen != 0 ) || \
			 netStreamTemplate->netSocket != CRYPT_ERROR );

#if 0	/* 5/5/08 See comment in net_trans.c */
	/* If we're running over a cryptlib session, make sure that we wait around
	   for a minimum amount of time during network comms in case the user has
	   specified nonblocking behaviour or quick timeouts */
	if( options == NET_OPTION_TRANSPORTSESSION )
		{
		static const int fixedTimeout = 30;
		int timeout;

		status = krnlSendMessage( iUserObject, IMESSAGE_GETATTRIBUTE,
								  &timeout, CRYPT_OPTION_NET_CONNECTTIMEOUT );
		if( cryptStatusOK( status ) && timeout < fixedTimeout )
			{
			( void ) krnlSendMessage( netStreamTemplate->iTransportSession,
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &fixedTimeout,
									  CRYPT_OPTION_NET_CONNECTTIMEOUT );
			}
		status = krnlSendMessage( iUserObject, IMESSAGE_GETATTRIBUTE,
								  &timeout, CRYPT_OPTION_NET_READTIMEOUT );
		if( cryptStatusOK( status ) && timeout < fixedTimeout )
			{
			( void ) krnlSendMessage( netStreamTemplate->iTransportSession, 
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &fixedTimeout,
									  CRYPT_OPTION_NET_READTIMEOUT );
			}
		status = krnlSendMessage( iUserObject, IMESSAGE_GETATTRIBUTE,
								  &timeout, CRYPT_OPTION_NET_WRITETIMEOUT );
		if( cryptStatusOK( status ) && timeout < fixedTimeout )
			{
			( void ) krnlSendMessage( netStreamTemplate->iTransportSession, 
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &fixedTimeout,
									  CRYPT_OPTION_NET_WRITETIMEOUT );
			}
		status = CRYPT_OK;	/* Reset status from above checks */
		}
#endif /* 0 */

	/* Wait for any async network driver binding to complete and make sure
	   that the network interface has been initialised */
	if( !krnlWaitSemaphore( SEMAPHORE_DRIVERBIND ) || \
		!netStreamTemplate->transportOKFunction() )
		{
		/* Clean up */
		zeroise( stream, sizeof( STREAM ) );
		retExt( CRYPT_ERROR_NOTINITED,
				( CRYPT_ERROR_NOTINITED, errorInfo, 
				  "Networking subsystem not available" ) );
		}

	/* Allocate room for the network stream information */
	if( useTransportBuffering )
		netStreamDataSize += NETWORK_BUFFER_SIZE + NETWORK_BUFFER_SIZE;
	if( urlInfo != NULL )
		netStreamDataSize += urlInfo->hostLen + urlInfo->locationLen;
	netStreamData = clAlloc( "completeConnect", sizeof( NET_STREAM_INFO ) + \
												netStreamDataSize );
	if( netStreamData == NULL )
		{
		zeroise( stream, sizeof( STREAM ) );
		return( CRYPT_ERROR_MEMORY );
		}

	/* Initialise the network stream with the net stream template and set up 
	   pointers to buffers if required */
	memcpy( netStreamData, netStreamTemplate, sizeof( NET_STREAM_INFO ) );
	if( useTransportBuffering || urlInfo != NULL )
		{
		NET_STREAM_INFO *netStreamInfo = netStreamData;
		BYTE *netStreamDataPtr = ( BYTE * ) netStreamData + \
											sizeof( NET_STREAM_INFO );

		netStreamInfo->storageSize = netStreamDataSize;
		if( useTransportBuffering )
			{
			stream->buffer = netStreamDataPtr;
			stream->bufSize = NETWORK_BUFFER_SIZE;
			netStreamInfo->writeBuffer = netStreamDataPtr + \
										 NETWORK_BUFFER_SIZE;
			netStreamInfo->writeBufSize = NETWORK_BUFFER_SIZE;
			netStreamDataPtr += NETWORK_BUFFER_SIZE + NETWORK_BUFFER_SIZE;
			}
		if( urlInfo != NULL )
			{
			netStreamInfo->host = ( char * ) netStreamDataPtr;
			memcpy( netStreamInfo->host, urlInfo->host, urlInfo->hostLen );
			netStreamInfo->hostLen = urlInfo->hostLen;
			if( urlInfo->location != NULL )
				{
				netStreamInfo->path = ( char * ) netStreamDataPtr + \
												 urlInfo->hostLen;
				memcpy( netStreamInfo->path, urlInfo->location, 
						urlInfo->locationLen );
				netStreamInfo->pathLen = urlInfo->locationLen;
				}
			netStreamInfo->port = urlInfo->port;
			}
		}
	stream->netStreamInfo = netStreamData;

	/* Open the connection to the remote system */
	status = openNetworkConnection( stream->netStreamInfo, options, 
									proxyUrl, proxyUrlLen );
	if( cryptStatusError( status ) )
		{
		NET_STREAM_INFO *netStream = \
				( NET_STREAM_INFO * ) stream->netStreamInfo;

		REQUIRES_S( netStream != NULL );

		/* Copy back the error information to the caller */
		copyErrorInfo( errorInfo, NETSTREAM_ERRINFO );

		/* Clean up */
		cleanupStream( stream, FALSE );
		return( status );
		}

	/* If we're not going through a proxy, we're done */
	if( proxyUrl == NULL )
		return( CRYPT_OK );

#ifdef USE_HTTP
	/* Complete the connect via the appropriate proxy type */
	status = connectViaHttpProxy( stream, errorInfo );
	if( cryptStatusError( status ) )
		{
		NET_STREAM_INFO *netStream = \
				( NET_STREAM_INFO * ) stream->netStreamInfo;

		REQUIRES_S( netStream != NULL );
		
		/* Copy back the error information to the caller */
		copyErrorInfo( errorInfo, NETSTREAM_ERRINFO );

		/* Clean up */
		cleanupStream( stream, FALSE );
		return( status );
		}
#else
	cleanupStream( stream, FALSE );
	retExt( CRYPT_ERROR_NOTAVAIL,
			( CRYPT_ERROR_NOTAVAIL, errorInfo, 
			  "HTTP proxy support not available" ) );
#endif /* USE_HTTP */

	return( CRYPT_OK );
	}

/* Open and close a network connection.  This parses a location string
   (usually a URL) into <scheme>://<host>[:<port>]/<path>[?<query>]
   components and opens a connection to the host for non-stateless
   protocols */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int sNetConnect( OUT STREAM *stream, 
				 IN_ENUM( STREAM_PROTOCOL ) const STREAM_PROTOCOL_TYPE protocol,
				 const NET_CONNECT_INFO *connectInfo, 
				 OUT ERROR_INFO *errorInfo )
	{
	NET_STREAM_INFO netStream;
	URL_INFO urlInfo, *urlInfoPtr = NULL;
	char proxyUrlBuffer[ MAX_DNS_SIZE + 8 ], *proxyURL = NULL;
	int proxyUrlLen = 0, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );
	assert( connectInfo->options != NET_OPTION_HOSTNAME || 
			( connectInfo->options == NET_OPTION_HOSTNAME && \
			  isReadPtr( connectInfo->name, connectInfo->nameLength ) && \
			  ( connectInfo->nameLength > 0 && \
				connectInfo->nameLength < MAX_INTLENGTH_SHORT ) && \
			  connectInfo->iCryptSession == CRYPT_ERROR && \
			  connectInfo->networkSocket == CRYPT_ERROR ) );

	REQUIRES( protocol == STREAM_PROTOCOL_TCPIP || \
			  protocol == STREAM_PROTOCOL_HTTP );
	REQUIRES( connectInfo->options > NET_OPTION_NONE && \
			  connectInfo->options < NET_OPTION_LAST );
	REQUIRES( connectInfo->options != NET_OPTION_HOSTNAME || 
			  ( connectInfo->options == NET_OPTION_HOSTNAME && \
			    connectInfo->name != NULL && \
				( connectInfo->nameLength > 0 && \
				  connectInfo->nameLength < MAX_INTLENGTH_SHORT ) && \
				connectInfo->iCryptSession == CRYPT_ERROR && \
				connectInfo->networkSocket == CRYPT_ERROR ) );
	REQUIRES( connectInfo->options != NET_OPTION_TRANSPORTSESSION || \
			  ( connectInfo->options == NET_OPTION_TRANSPORTSESSION && \
				connectInfo->name == NULL && connectInfo->nameLength == 0 && \
				connectInfo->interface == NULL && connectInfo->interfaceLength == 0 && \
				connectInfo->iCryptSession != CRYPT_ERROR && \
				connectInfo->networkSocket == CRYPT_ERROR ) );
	REQUIRES( ( connectInfo->options != NET_OPTION_NETWORKSOCKET && \
				connectInfo->options != NET_OPTION_NETWORKSOCKET_DUMMY ) || 
			  ( ( connectInfo->options == NET_OPTION_NETWORKSOCKET || \
				  connectInfo->options == NET_OPTION_NETWORKSOCKET_DUMMY ) && \
				connectInfo->name == NULL && connectInfo->nameLength == 0 && \
				connectInfo->interface == NULL && connectInfo->interfaceLength == 0 && \
				connectInfo->iCryptSession == CRYPT_ERROR && \
				connectInfo->networkSocket != CRYPT_ERROR ) );
	REQUIRES( connectInfo->iUserObject == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( connectInfo->iUserObject ) );

	/* Clear return values */
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Initialise the network stream info */
	status = initStream( stream, &netStream, protocol, connectInfo, FALSE );
	if( cryptStatusError( status ) )
		return( status );
	if( connectInfo->options == NET_OPTION_HOSTNAME )
		urlInfoPtr = &urlInfo;
	status = processConnectOptions( stream, &netStream, urlInfoPtr, 
									connectInfo, errorInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( connectInfo->options == NET_OPTION_HOSTNAME )
		{
		int proxyUrlLength;

		ANALYSER_HINT( urlInfoPtr != NULL );

		/* Check for the use of a proxy to establish the connection.  This 
		   function will return OK_SPECIAL if there's a proxy present */
		status = checkForProxy( &netStream, protocol, connectInfo, 
								urlInfoPtr->host, urlInfoPtr->hostLen,
								proxyUrlBuffer, MAX_DNS_SIZE, 
								&proxyUrlLength );
		if( cryptStatusError( status ) )
			{
			if( status != OK_SPECIAL )
				return( status );

			/* There's a proxy present, go via the proxy rather than 
			   directly to the user-supplied URL */
			proxyURL = proxyUrlBuffer;
			proxyUrlLen = proxyUrlLength;
			}
		}

	/* Set up access mechanisms and complete the connection */
	return( completeConnect( stream, &netStream, urlInfoPtr, protocol, 
							 connectInfo->options, proxyURL, proxyUrlLen, 
							 connectInfo->iUserObject, errorInfo ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int sNetListen( OUT STREAM *stream, 
				IN_ENUM( STREAM_PROTOCOL ) const STREAM_PROTOCOL_TYPE protocol,
				const NET_CONNECT_INFO *connectInfo, 
				OUT ERROR_INFO *errorInfo )
	{
	NET_STREAM_INFO netStream;
	URL_INFO urlInfo, *urlInfoPtr = NULL;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( protocol == STREAM_PROTOCOL_TCPIP || \
			  protocol == STREAM_PROTOCOL_HTTP );
	REQUIRES( connectInfo->options == NET_OPTION_HOSTNAME || \
			  connectInfo->options == NET_OPTION_TRANSPORTSESSION || \
			  connectInfo->options == NET_OPTION_NETWORKSOCKET );
	REQUIRES( connectInfo->options != NET_OPTION_HOSTNAME || 
			  ( connectInfo->options == NET_OPTION_HOSTNAME && \
				connectInfo->iCryptSession == CRYPT_ERROR && \
				connectInfo->networkSocket == CRYPT_ERROR ) );
	REQUIRES( connectInfo->options != NET_OPTION_TRANSPORTSESSION || \
			  ( connectInfo->options == NET_OPTION_TRANSPORTSESSION && \
				connectInfo->interface == NULL && connectInfo->interfaceLength == 0 && \
				connectInfo->iCryptSession != CRYPT_ERROR && \
				connectInfo->networkSocket == CRYPT_ERROR ) );
	REQUIRES( ( connectInfo->options != NET_OPTION_NETWORKSOCKET && \
				connectInfo->options != NET_OPTION_NETWORKSOCKET_DUMMY ) || 
			  ( ( connectInfo->options == NET_OPTION_NETWORKSOCKET || \
				  connectInfo->options == NET_OPTION_NETWORKSOCKET_DUMMY ) && \
				connectInfo->interface == NULL && connectInfo->interfaceLength == 0 && \
				connectInfo->iCryptSession == CRYPT_ERROR && \
				connectInfo->networkSocket != CRYPT_ERROR ) );
	REQUIRES( connectInfo->iUserObject == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( connectInfo->iUserObject ) );
	REQUIRES( connectInfo->name == NULL && connectInfo->nameLength == 0 );

	/* Clear the return values */
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Initialise the network stream info */
	status = initStream( stream, &netStream, protocol, connectInfo, TRUE );
	if( cryptStatusError( status ) )
		return( status );
	if( connectInfo->options == NET_OPTION_HOSTNAME && \
		connectInfo->interface != NULL )
		urlInfoPtr = &urlInfo;
	status = processConnectOptions( stream, &netStream, urlInfoPtr, 
									connectInfo, errorInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Set up access mechanisms and complete the connection */
	return( completeConnect( stream, &netStream, urlInfoPtr, protocol, 
							 connectInfo->options, NULL, 0,
							 connectInfo->iUserObject, errorInfo ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sNetDisconnect( INOUT STREAM *stream )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL );
	REQUIRES_S( netStream->sanityCheckFunction( stream ) );

	cleanupStream( stream, TRUE );

	return( CRYPT_OK );
	}

/* Parse a URL into its various components */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sNetParseURL( OUT URL_INFO *urlInfo, 
				  IN_BUFFER( urlLen ) const char *url, 
				  IN_LENGTH_SHORT const int urlLen, 
				  IN_ENUM_OPT( URL_TYPE ) const URL_TYPE urlTypeHint )
	{
	assert( isWritePtr( urlInfo, sizeof( URL_INFO ) ) );
	assert( isReadPtr( url, urlLen ) );

	REQUIRES( urlLen > 0 && urlLen < MAX_INTLENGTH_SHORT );
	REQUIRES( urlTypeHint >= URL_TYPE_NONE && urlTypeHint < URL_TYPE_LAST );

	return( parseURL( urlInfo, url, urlLen, CRYPT_UNUSED, urlTypeHint, 
					  TRUE ) );
	}

/* Get extended information about an error status on a network connection */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sNetGetErrorInfo( INOUT STREAM *stream, OUT ERROR_INFO *errorInfo )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;

	assert( isReadPtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES_V( netStream != NULL );
	REQUIRES_V( netStream->sanityCheckFunction( stream ) );

	/* Remember the error code and message.  If we're running over a
	   cryptlib transport session we have to first pull the information up 
	   from the session, since getSessionErrorInfo() passes through the 
	   error status from the caller (which in this case is CRYPT_OK since 
	   we're just using it as a data-fetch function) we don't check the 
	   return code */
	if( netStream->iTransportSession != CRYPT_ERROR )
		( void ) getSessionErrorInfo( netStream, CRYPT_OK );
	copyErrorInfo( errorInfo, NETSTREAM_ERRINFO );
	}

#else

/****************************************************************************
*																			*
*							Network Stream Stubs							*
*																			*
****************************************************************************/

/* If there's no networking support present we replace the network access
   routines with dummy ones that always return an error */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int sNetConnect( OUT STREAM *stream, 
				 IN_ENUM( STREAM_PROTOCOL ) const STREAM_PROTOCOL_TYPE protocol,
				 const NET_CONNECT_INFO *connectInfo, 
				 INOUT ERROR_INFO *errorInfo )
	{
	memset( stream, 0, sizeof( STREAM ) );
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );
	return( CRYPT_ERROR_OPEN );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int sNetListen( OUT STREAM *stream, 
				IN_ENUM( STREAM_PROTOCOL ) const STREAM_PROTOCOL_TYPE protocol,
				const NET_CONNECT_INFO *connectInfo, 
				INOUT ERROR_INFO *errorInfo )
	{
	memset( stream, 0, sizeof( STREAM ) );
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );
	return( CRYPT_ERROR_OPEN );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sNetDisconnect( INOUT STREAM *stream )
	{
	UNUSED_ARG( stream );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sNetParseURL( INOUT URL_INFO *urlInfo, 
				  IN_BUFFER( urlLen ) const char *url, 
				  IN_LENGTH_SHORT const int urlLen, 
				  IN_ENUM_OPT( URL_TYPE ) const URL_TYPE urlTypeHint )
	{
	memset( urlInfo, 0, sizeof( URL_INFO ) );

	return( CRYPT_ERROR_BADDATA );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sNetGetErrorInfo( INOUT STREAM *stream, OUT ERROR_INFO *errorInfo )
	{
	UNUSED_ARG( stream );

	memset( errorInfo, 0, sizeof( ERROR_INFO ) );
	}
#endif /* USE_TCP */
