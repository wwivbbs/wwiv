/****************************************************************************
*																			*
*						Network Stream I/O Functions						*
*						Copyright Peter Gutmann 1993-2017					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "stream_int.h"
  #ifdef USE_EAP
	#include "eap.h"			/* For EAP_INFO */
  #endif /* USE_EAP */
  #include "tcp.h"				/* For INVALID_SOCKET */
#else
  #include "io/stream_int.h"
  #ifdef USE_EAP
	#include "io/eap.h"			/* For EAP_INFO */
  #endif /* USE_EAP */
  #include "io/tcp.h"			/* For INVALID_SOCKET */
#endif /* Compiler-specific includes */

#ifdef USE_TCP

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check a network stream */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckNetStream( const NET_STREAM_INFO *netStream )
	{
	assert( isReadPtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* Check overall network stream data.  STREAM_PEER_NONE is a valid
	   setting, and in fact the default, since the peer type is only used to
	   fingerprint buggy peers */
	if( !isEnumRange( netStream->protocol, STREAM_PROTOCOL ) || \
		!CHECK_FLAGS( netStream->nFlags, STREAM_NFLAG_NONE, 
					  STREAM_NFLAG_MAX ) || \
		!CHECK_FLAGS( netStream->nhFlags, STREAM_NHFLAG_NONE, 
					  STREAM_NHFLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckNetStream: General info" ));
		return( FALSE );
		}
	if( netStream->timeout < 0 || \
		netStream->timeout > MAX_NETWORK_TIMEOUT )
		{
		/* Servers wait around more or less indefinitely for incoming
		   connections so we allow a timeout of MAX_INTLENGTH timeout for 
		   servers */
		if( !( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_ISSERVER ) && \
			   netStream->timeout == MAX_INTLENGTH ) )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Timeout" ));
			return( FALSE );
			}
		}
	if( netStream->savedTimeout < 0 || \
		netStream->savedTimeout > MAX_NETWORK_TIMEOUT )
		{
		DEBUG_PUTS(( "sanityCheckNetStream: Saved timeout" ));
		return( FALSE );
		}
	if( !isEnumRangeOpt( netStream->systemType, STREAM_PEER ) )
		{
		DEBUG_PUTS(( "sanityCheckNetStream: System type" ));
		return( FALSE );
		}

	/* Check network-related information */
	if( netStream->host == NULL )
		{
		if( netStream->hostLen != 0 )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Spurious host" ));
			return( FALSE );
			}
		}
	else
		{
		if( netStream->hostLen < MIN_HOST_SIZE || \
			netStream->hostLen > MAX_HOST_SIZE )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Host" ));
			return( FALSE );
			}
		}
	if( netStream->path == NULL )
		{
		if( netStream->pathLen != 0 )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Spurious path" ));
			return( FALSE );
			}
		}
	else
		{
		if( netStream->pathLen < MIN_LOCATION_SIZE || \
			netStream->pathLen > MAX_LOCATION_SIZE )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Path" ));
			return( FALSE );
			}
		}
	if( !TEST_FLAG( netStream->nFlags, STREAM_NFLAG_USERSOCKET ) )
		{
		if( netStream->port < MIN_PORT_NUMBER || \
			netStream->port > MAX_PORT_NUMBER )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Port" ));
			return( FALSE );
			}
		}

	/* If it's an unbuffered network stream, all buffer values must be 
	   zero */
	if( netStream->writeBuffer == NULL )
		{
		if( netStream->writeBufSize != 0 || netStream->writeBufEnd != 0 )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Spurious write buffer" ));
			return( FALSE );
			}
		}
	else
		{
		/* Make sure that the write buffer position is within bounds */
		if( netStream->writeBufSize <= 0 || \
			netStream->writeBufSize >= MAX_BUFFER_SIZE )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Write buffer" ));
			return( FALSE );
			}
		if( netStream->writeBufEnd < 0 || \
			netStream->writeBufEnd > netStream->writeBufSize )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Write buffer info" ));
			return( FALSE );
			}

		/* Make sure that the write buffer hasn't been corrupted */
		if( !safeBufferCheck( netStream->writeBuffer, 
							  netStream->writeBufSize ) )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Write buffer corruption" ));
			return( FALSE );
			}
		}

	/* Check the network stream access functions */
	if( !FNPTR_ISSET( netStream->writeFunction ) || \
		!FNPTR_ISSET( netStream->readFunction ) || \
		!FNPTR_ISSET( netStream->transportConnectFunction ) || \
		!FNPTR_ISSET( netStream->transportDisconnectFunction ) || \
		!FNPTR_ISSET( netStream->transportReadFunction ) || \
		!FNPTR_ISSET( netStream->transportWriteFunction ) || \
		!FNPTR_ISSET( netStream->transportOKFunction ) || \
		!FNPTR_ISSET( netStream->transportCheckFunction ) )
		{
		DEBUG_PUTS(( "sanityCheckNetStream: Access functions" ));
		return( FALSE );
		}
	if( !FNPTR_ISVALID( netStream->connectFunctionOpt ) || \
		!FNPTR_ISVALID( netStream->disconnectFunctionOpt ) )
		{
		DEBUG_PUTS(( "sanityCheckNetStream: Optional access functions" ));
		return( FALSE );
		}
	if( DATAPTR_ISNULL( netStream->virtualStateInfo ) )
		{
		if( !FNPTR_ISNULL( netStream->virtualGetDataFunction ) || \
			!FNPTR_ISNULL( netStream->virtualPutDataFunction ) || \
			!FNPTR_ISNULL( netStream->virtualGetErrorInfoFunction ) )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Spurious virtual functions" ));
			return( FALSE );
			}
		}
	else
		{
		if( !DATAPTR_ISSET( netStream->virtualStateInfo ) || \
			!FNPTR_ISSET( netStream->virtualGetDataFunction ) || \
			!FNPTR_ISSET( netStream->virtualPutDataFunction ) || \
			!FNPTR_ISSET( netStream->virtualGetErrorInfoFunction ) )
			{
			DEBUG_PUTS(( "sanityCheckNetStream: Virtual functions" ));
			return( FALSE );
			}
		}

	return( TRUE );
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
						  OUT_LENGTH_BOUNDED_Z( proxyUrlMaxLen ) \
							int *proxyUrlLen )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );
	assert( isWritePtrDynamic( proxyUrlBuffer, proxyUrlMaxLen ) );
	assert( isWritePtr( proxyUrlLen, sizeof( int ) ) );

	REQUIRES( isEnumRange( protocol, STREAM_PROTOCOL ) );
	REQUIRES( proxyUrlMaxLen > 10 && proxyUrlMaxLen <= MAX_DNS_SIZE );

	/* Clear return value */
	memset( proxyUrlBuffer, 0, min( 16, proxyUrlMaxLen ) );
	*proxyUrlLen = 0;

	/* Check for a local connection, which always bypasses the proxy.  We
	   only use the case-insensitive string compares for the text-format
	   host names since the numeric forms don't need this.  In addition
	   since the IPv4 localhost is a /8, we check for anything with a
	   "127." prefix */
	if( ( hostLen > 4 && !memcmp( host, "127.", 4 ) ) || \
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
			if( connectInfo->options == NET_OPTION_HOSTNAME )
				SET_FLAG( netStream->nhFlags, STREAM_NHFLAG_PROXY );
			else
				SET_FLAG( netStream->nhFlags, STREAM_NHFLAG_TUNNEL );
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
	const STM_TRANSPORTCONNECT_FUNCTION transportConnectFunction = \
						( STM_TRANSPORTCONNECT_FUNCTION ) \
						FNPTR_GET( netStream->transportConnectFunction );
	URL_INFO urlInfo;
	char urlBuffer[ MAX_DNS_SIZE + 8 ];
	const char *url = proxyUrl;
	int urlLen = proxyUrlLen, status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( ( proxyUrl == NULL && proxyUrlLen == 0 ) || \
			isReadPtrDynamic( proxyUrl, proxyUrlLen ) );

	REQUIRES( isEnumRange( options, NET_OPTION ) );
	REQUIRES( ( proxyUrl == NULL && proxyUrlLen == 0 ) || \
			  ( proxyUrl != NULL && \
				proxyUrlLen > 0 && proxyUrlLen <= MAX_DNS_SIZE ) );
	REQUIRES( transportConnectFunction != NULL );

	/* If we're using an already-active network socket supplied by the
	   user, there's nothing to do */
	if( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_USERSOCKET ) )
		{
		/* If it's a dummy open to check parameters that can't be validated
		   at a higher level then we pass the info on down to the low-level 
		   checking routines */
		if( options == NET_OPTION_NETWORKSOCKET_DUMMY )
			{
			const STM_TRANSPORTCHECK_FUNCTION transportCheckFunction = \
						( STM_TRANSPORTCHECK_FUNCTION ) \
						FNPTR_GET( netStream->transportCheckFunction );

			REQUIRES( transportCheckFunction != NULL );

			return( transportCheckFunction( netStream ) );
			}

		return( CRYPT_OK );
		}

	/* If we're not going via a proxy, perform a direct open */
	if( proxyUrl == NULL )
		{
		return( transportConnectFunction( netStream, netStream->host, 
										  netStream->hostLen, netStream->port ) );
		}

	/* We're going via a proxy, if the user has specified automatic proxy
	   detection try and locate the proxy information.  This only works for
	   Windows which has built-in proxy discovery support, for everything 
	   else it's no-op'd out */
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

	/* Process the proxy details, either as supplied by the caller or using
	   the auto-detected URL from above.  Since this is an HTTP proxy we 
	   specify the default port as port 80 */
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
	return( transportConnectFunction( netStream, urlInfo.host, 
									  urlInfo.hostLen, urlInfo.port ) );
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
	int timeout, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );

	REQUIRES( isEnumRange( protocol, STREAM_PROTOCOL ) );
	REQUIRES( isServer == TRUE || isServer == FALSE );

	/* Set up the basic network stream info */
	memset( stream, 0, sizeof( STREAM ) );
	stream->type = STREAM_TYPE_NETWORK;
	INIT_FLAGS( stream->flags, STREAM_FLAG_NONE );
	memset( netStream, 0, sizeof( NET_STREAM_INFO ) );
	netStream->protocol = protocol;
	netStream->port = connectInfo->port;
	netStream->netSocket = netStream->listenSocket = INVALID_SOCKET;
	if( isServer )
		INIT_FLAGS( netStream->nFlags, STREAM_NFLAG_ISSERVER );
	else
		INIT_FLAGS( netStream->nFlags, STREAM_NFLAG_NONE );
#ifdef USE_EAP
	if( protocol == STREAM_PROTOCOL_UDP || protocol == STREAM_PROTOCOL_EAP )
		SET_FLAG( netStream->nFlags, STREAM_NFLAG_DGRAM );
#else
	if( protocol == STREAM_PROTOCOL_UDP )
		SET_FLAG( netStream->nFlags, STREAM_NFLAG_DGRAM );
#endif /* USE_EAP */
	INIT_FLAGS( netStream->nhFlags, STREAM_NHFLAG_NONE );

	/* Initialise the virtual and optional access method pointers.  These 
	   aren't necessarily set by access functions so we have to explicitly 
	   clear them here */ 
	DATAPTR_SET( netStream->virtualStateInfo, NULL );
	FNPTR_SET( netStream->virtualGetDataFunction, NULL );
	FNPTR_SET( netStream->virtualPutDataFunction, NULL );
	FNPTR_SET( netStream->virtualGetErrorInfoFunction, NULL );
	FNPTR_SET( netStream->connectFunctionOpt, NULL );
	FNPTR_SET( netStream->disconnectFunctionOpt, NULL );

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
		status = krnlSendMessage( connectInfo->iUserObject, 
								  IMESSAGE_GETATTRIBUTE, &timeout, 
								  CRYPT_OPTION_NET_CONNECTTIMEOUT );
		if( cryptStatusError( status ) )
			timeout = 30;
		}
	if( timeout < 5 )
		{
		/* Enforce the same minimum connect timeout as the kernel ACLs */
		DEBUG_DIAG(( "Network connect timeout is < 5s, setting to 5s" ));
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
		status = krnlSendMessage( connectInfo->iUserObject, 
								  IMESSAGE_GETATTRIBUTE, &timeout, 
								  CRYPT_OPTION_NET_READTIMEOUT );
		if( cryptStatusError( status ) )
			timeout = 30;
		}
	netStream->savedTimeout = timeout;

	return( CRYPT_OK );
	}

/* Set up pointers to stream-internal buffers if required:

	+--------+
	| STREAM |--+---+
	+--------+  |	|
				|	|
	 netStream	|	| buffer
	+-----------+	|
	v				v
	+---------------+---------+----------+---------+--------+------------+
	|NET_STREAM_INFO|Read buf.|Write buf.|Host name|Loc.name|Subtype data|
	+---------------+---------+----------+---------+--------+------------+
			|				  ^			 ^		   ^		^
			|				  |writeBuffr|host	   |location|subtypeInfo
			|				  |			 |		   |		|
			+-----------------+----------+---------+--------+ */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initStreamStorage( INOUT STREAM *stream,
							  INOUT NET_STREAM_INFO *netStream,
							  IN_LENGTH_SHORT const int netStreamAllocSize,
							  IN_ENUM( STREAM_PROTOCOL ) \
									const STREAM_PROTOCOL_TYPE protocol,
							  IN_OPT const URL_INFO *urlInfo )
	{
	int bufferStorageSize = 0;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( ( urlInfo == NULL ) || \
			isReadPtr( urlInfo, sizeof( URL_INFO ) ) );

	REQUIRES( stream->type == STREAM_TYPE_NETWORK );
			  /* We can't use the sanity-check function because the stream
				 hasn't been fully set up yet */
	REQUIRES( urlInfo == NULL || \
			  ( urlInfo != NULL && urlInfo->host != NULL && \
				isShortIntegerRangeNZ( urlInfo->hostLen ) ) );
	REQUIRES( isEnumRange( protocol, STREAM_PROTOCOL ) );
	REQUIRES( isShortIntegerRange( netStreamAllocSize ) );

	/* If it's an unbuffered stream and there's no URL info to 
	   record, there's nothing to do */
	if( ( protocol == STREAM_PROTOCOL_TCP || \
		  protocol == STREAM_PROTOCOL_UDP ) && urlInfo == NULL )
		return( CRYPT_OK );

	netStream->storageSize = netStreamAllocSize;
	if( protocol != STREAM_PROTOCOL_TCP && protocol != STREAM_PROTOCOL_UDP )
		{
		stream->buffer = SAFEBUFFER_PTR( netStream->storage );
		stream->bufSize = NETSTREAM_BUFFER_SIZE;
		safeBufferInit( stream->buffer, stream->bufSize );
		netStream->writeBuffer = \
					SAFEBUFFER_PTR( netStream->storage + \
									SAFEBUFFER_SIZE( NETSTREAM_BUFFER_SIZE ) );
		netStream->writeBufSize = NETSTREAM_BUFFER_SIZE;
		safeBufferInit( netStream->writeBuffer, netStream->writeBufSize );
		bufferStorageSize = SAFEBUFFER_SIZE( NETSTREAM_BUFFER_SIZE ) + \
							SAFEBUFFER_SIZE( NETSTREAM_BUFFER_SIZE );
		}
	if( urlInfo != NULL )
		{
		netStream->host = ( char * ) netStream->storage + bufferStorageSize;
		REQUIRES( boundsCheckZ( bufferStorageSize, urlInfo->hostLen, 
								netStream->storageSize ) );
		memcpy( netStream->host, urlInfo->host, urlInfo->hostLen );
		netStream->hostLen = urlInfo->hostLen;
		bufferStorageSize += urlInfo->hostLen;
		if( urlInfo->location != NULL )
			{
			netStream->path = ( char * ) netStream->storage + bufferStorageSize;
			REQUIRES( boundsCheckZ( bufferStorageSize, urlInfo->locationLen, 
									netStream->storageSize ) );
			memcpy( netStream->path, urlInfo->location, 
					urlInfo->locationLen );
			netStream->pathLen = urlInfo->locationLen;
#ifdef USE_EAP
			bufferStorageSize += urlInfo->locationLen;
#endif /* USE_EAP */
			}
		netStream->port = urlInfo->port;
		}
#ifdef USE_EAP
	if( protocol == STREAM_PROTOCOL_EAP )
		{
		netStream->subTypeInfo = ( char * ) netStream->storage + bufferStorageSize;
		REQUIRES( boundsCheckZ( bufferStorageSize, sizeof( EAP_INFO ), 
								netStream->storageSize ) );
		}
#endif /* USE_EAP */

	return( CRYPT_OK );
	}

/* Clean up a stream to shut it down */

STDC_NONNULL_ARG( ( 1 ) ) \
static void cleanupStream( INOUT STREAM *stream, 
						   const BOOLEAN cleanupTransport )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_V( cleanupTransport == TRUE || cleanupTransport == FALSE );
	REQUIRES_V( netStream != NULL && sanityCheckNetStream( netStream ) );

	/* Clean up the transport system if necessary */
	if( cleanupTransport && \
		!TEST_FLAG( netStream->nFlags, STREAM_NFLAG_USERSOCKET ) )
		{
		const STM_TRANSPORTDISCONNECT_FUNCTION transportDisconnectFunction = \
						( STM_TRANSPORTDISCONNECT_FUNCTION ) \
						FNPTR_GET( netStream->transportDisconnectFunction );

		REQUIRES_V( transportDisconnectFunction != NULL );

		( void ) transportDisconnectFunction( netStream, TRUE );
		}

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
	REQUIRES_S( ( ( connectInfo->options == NET_OPTION_NETWORKSOCKET || \
					connectInfo->options == NET_OPTION_NETWORKSOCKET_DUMMY ) && \
					urlInfo == NULL ) || \
				( !TEST_FLAG( netStream->nFlags, STREAM_NFLAG_ISSERVER ) && \
				  ( connectInfo->options == NET_OPTION_HOSTNAME || \
				    connectInfo->options == NET_OPTION_VIRTUAL ) && \
				  connectInfo->name != NULL && \
				  isShortIntegerRangeNZ( connectInfo->nameLength ) && \
				  urlInfo != NULL ) || \
				( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_ISSERVER ) && \
				  ( connectInfo->options == NET_OPTION_HOSTNAME || \
				    connectInfo->options == NET_OPTION_VIRTUAL ) && \
				  connectInfo->name == NULL && connectInfo->nameLength == 0 && \
				  ( ( connectInfo->interface == NULL && \
					  connectInfo->interfaceLength == 0 && urlInfo == NULL ) || \
					( connectInfo->interface != NULL && \
					  isShortIntegerRangeNZ( connectInfo->interfaceLength ) && \
					  urlInfo != NULL ) ) ) );

	/* Clear return value */
	if( urlInfo != NULL )
		memset( urlInfo, 0, sizeof( URL_INFO ) );

	/* If it's a user-supplied network socket remember this */
	if( connectInfo->options == NET_OPTION_NETWORKSOCKET || \
		connectInfo->options == NET_OPTION_NETWORKSOCKET_DUMMY )
		{
		netStream->netSocket = connectInfo->networkSocket;
		SET_FLAG( netStream->nFlags, STREAM_NFLAG_USERSOCKET );

		return( CRYPT_OK );
		}

	ENSURES_S( connectInfo->options == NET_OPTION_HOSTNAME || \
			   connectInfo->options == NET_OPTION_VIRTUAL );

	REQUIRES_S( ( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_ISSERVER ) && \
				  connectInfo->name == NULL && \
				  connectInfo->nameLength == 0 ) || \
				( connectInfo->name != NULL && \
				  isShortIntegerRangeNZ( connectInfo->nameLength ) ) );

	/* If it's a virtual stream, set up the callback information that's used 
	   to move data to/from the stream */
	if( connectInfo->options == NET_OPTION_VIRTUAL )
		{
		netStream->virtualGetDataFunction = \
							connectInfo->virtualGetDataFunction;
		netStream->virtualPutDataFunction = \
							connectInfo->virtualPutDataFunction;
		netStream->virtualGetErrorInfoFunction = \
							connectInfo->virtualGetErrorInfoFunction;
		netStream->virtualStateInfo = connectInfo->virtualStateInfo;
		}

	/* If it's a server (i.e. we're opening a listen socket) then the 
	   name is the interface name to bind to, defaulting to the first
	   interface we find/localhost if none is given */
	if( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_ISSERVER ) )
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
				  TEST_FLAG( netStream->nFlags, 
							 STREAM_NFLAG_ISSERVER ) ? \
				  "interface" : "host" ) );
		}

	return( CRYPT_OK );
	}

/* Complete a network connection after the client- or server-specific
   portions have been handled */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 8 ) ) \
static int completeConnect( INOUT STREAM *stream,
							INOUT NET_STREAM_INFO *netStreamTemplate, 
							IN const NET_CONNECT_INFO *connectInfo, 
							IN_OPT const URL_INFO *urlInfo,
							IN_ENUM( STREAM_PROTOCOL ) \
								const STREAM_PROTOCOL_TYPE protocol,
							IN_BUFFER_OPT( proxyUrlLen ) const char *proxyUrl, 
							IN_LENGTH_DNS_Z const int proxyUrlLen,
							INOUT ERROR_INFO *errorInfo )
	{
	const BOOLEAN useTransportBuffering = \
						( protocol == STREAM_PROTOCOL_TCP || \
						  protocol == STREAM_PROTOCOL_UDP ) ? \
						FALSE : TRUE;
	STM_TRANSPORTOK_FUNCTION transportOKFunction;
	NET_STREAM_INFO *netStream;
	void *netStreamInfo;
	int netStreamAllocSize = 0, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( netStreamTemplate, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );
	assert( ( urlInfo == NULL ) || \
			isReadPtr( urlInfo, sizeof( URL_INFO ) ) );
	assert( ( proxyUrl == NULL && proxyUrlLen == 0 ) || \
			isReadPtrDynamic( proxyUrl, proxyUrlLen ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES_S( stream->type == STREAM_TYPE_NETWORK );
				/* We can't use the sanity-check function because the stream
				   hasn't been fully set up yet */
	REQUIRES_S( urlInfo == NULL || \
				( urlInfo != NULL && urlInfo->host != NULL && \
				  isShortIntegerRangeNZ( urlInfo->hostLen ) ) );
	REQUIRES_S( isEnumRange( protocol, STREAM_PROTOCOL ) );
	REQUIRES_S( ( proxyUrl == NULL && proxyUrlLen == 0 ) || \
				( proxyUrl != NULL && \
				  isShortIntegerRangeNZ( proxyUrlLen ) ) );

	/* Set up the access method pointers.  We can use either direct TCP/IP
	   access or a cryptlib stream for transport, and layered over that
	   either HTTP or direct access to the TCP or UDP transport layer */
	switch( connectInfo->options )
		{
		case NET_OPTION_VIRTUAL:
			setAccessMethodTransportVirtual( netStreamTemplate );
			break;

		default:
			setAccessMethodTCP( netStreamTemplate );
			break;
		}
	switch( protocol )
		{
		case STREAM_PROTOCOL_TCP:
		case STREAM_PROTOCOL_UDP:
			setStreamLayerDirect( netStreamTemplate );
			break;

		case STREAM_PROTOCOL_HTTP:
#ifdef USE_HTTP
			setStreamLayerHTTP( netStreamTemplate );
#else
			return( CRYPT_ERROR_NOTAVAIL );
#endif /* USE_HTTP */
			break;

#ifdef USE_EAP
		case STREAM_PROTOCOL_EAP:
			setStreamLayerEAP( netStreamTemplate );
			break;
#endif /* USE_EAP */

		default:
			retIntError_Stream( stream );
		}
	ENSURES_S( FNPTR_ISSET( netStreamTemplate->writeFunction ) && \
			   FNPTR_ISSET( netStreamTemplate->readFunction ) );
	ENSURES_S( FNPTR_ISSET( netStreamTemplate->transportConnectFunction ) && \
			   FNPTR_ISSET( netStreamTemplate->transportDisconnectFunction ) );
	ENSURES_S( FNPTR_ISSET( netStreamTemplate->transportReadFunction ) && \
			   FNPTR_ISSET( netStreamTemplate->transportWriteFunction ) );
	ENSURES_S( FNPTR_ISSET( netStreamTemplate->transportOKFunction ) && \
			   FNPTR_ISSET( netStreamTemplate->transportCheckFunction ) );
	ENSURES_S( DATAPTR_ISVALID( netStreamTemplate->virtualStateInfo ) && \
			   FNPTR_ISVALID( netStreamTemplate->virtualGetDataFunction ) && \
			   FNPTR_ISVALID( netStreamTemplate->virtualPutDataFunction ) && \
			   FNPTR_ISVALID( netStreamTemplate->virtualGetErrorInfoFunction ) );
	ENSURES_S( FNPTR_ISVALID( netStreamTemplate->connectFunctionOpt ) && \
			   FNPTR_ISVALID( netStreamTemplate->disconnectFunctionOpt ) );
	ENSURES_S( TEST_FLAG( netStreamTemplate->nFlags, STREAM_NFLAG_ISSERVER ) || \
			   ( urlInfo != NULL && \
				 urlInfo->host != NULL && urlInfo->hostLen != 0 ) || \
			   netStreamTemplate->netSocket != CRYPT_ERROR );

	/* Wait for any async network driver binding to complete and make sure
	   that the network interface has been initialised */
	transportOKFunction = ( STM_TRANSPORTOK_FUNCTION ) \
						  FNPTR_GET( netStreamTemplate->transportOKFunction );
	ENSURES_S( transportOKFunction != NULL );
	if( !krnlWaitSemaphore( SEMAPHORE_DRIVERBIND ) || !transportOKFunction() )
		{
		/* Clean up */
		zeroise( stream, sizeof( STREAM ) );
		retExt( CRYPT_ERROR_NOTINITED,
				( CRYPT_ERROR_NOTINITED, errorInfo, 
				  "Networking subsystem not available" ) );
		}

	/* Allocate room for the network stream information and set up an alias 
	   into the NETWORK_STREAM_INFO portion.  The memory layout is:

		+---------------+---------+----------+---------+--------+------------+
		|NET_STREAM_INFO|Read buf.|Write buf.|Host name|Loc.name|Subtype data|
		+---------------+---------+----------+---------+--------+------------+

	   with the additional variable-length storage held in 
	   { netStreamInfo->storage, netStreamInfo->storageLen } at the end of
	   the netStream structure.  The read and write buffers are safe buffers,
	   which means that there are canaries preceding and following the buffer
	   data */
	if( useTransportBuffering )
		{
		netStreamAllocSize += SAFEBUFFER_SIZE( NETSTREAM_BUFFER_SIZE ) + \
							  SAFEBUFFER_SIZE( NETSTREAM_BUFFER_SIZE );
		}
	if( urlInfo != NULL )
		netStreamAllocSize += urlInfo->hostLen + urlInfo->locationLen;
#ifdef USE_EAP
	if( protocol == STREAM_PROTOCOL_EAP )
		netStreamAllocSize += sizeof( EAP_INFO );
#endif /* USE_EAP */
	REQUIRES( netStreamAllocSize == 0 || \
			  rangeCheck( netStreamAllocSize, 1, MAX_BUFFER_SIZE ) );
	netStreamInfo = clAlloc( "completeConnect", sizeof( NET_STREAM_INFO ) + \
												netStreamAllocSize );
	if( netStreamInfo == NULL )
		{
		zeroise( stream, sizeof( STREAM ) );
		return( CRYPT_ERROR_MEMORY );
		}
	memset( netStreamInfo, 0, 
			sizeof( NET_STREAM_INFO ) + netStreamAllocSize );
	netStream = netStreamInfo;

	/* Initialise the network stream with the net stream template and set up 
	   internal storage */
	memcpy( netStream, netStreamTemplate, sizeof( NET_STREAM_INFO ) );
	status = initStreamStorage( stream, netStream, netStreamAllocSize, 
								protocol, urlInfo );
	if( cryptStatusError( status ) )
		{
		zeroise( netStreamInfo, 
				 sizeof( NET_STREAM_INFO ) + netStreamAllocSize );
		clFree( "completeConnect", netStreamInfo );
		zeroise( stream, sizeof( STREAM ) );
		return( status );
		}

	/* The network stream is ready to go, connect it to the overall 
	   stream */
	ENSURES_S( sanityCheckNetStream( netStream ) );
	DATAPTR_SET( stream->netStream, netStream );

	/* Open the connection to the remote system */
	status = openNetworkConnection( netStream, connectInfo->options, 
									proxyUrl, proxyUrlLen );
	if( cryptStatusError( status ) )
		{
		/* Copy back the error information to the caller */
		copyErrorInfo( errorInfo, NETSTREAM_ERRINFO );

		/* Clean up */
		cleanupStream( stream, FALSE );
		return( status );
		}

	/* If we're using optional transport protocol-negotiation step, activate 
	   that too */
	if( FNPTR_ISSET( netStream->connectFunctionOpt ) )
		{
		STM_CONNECT_FUNCTION_OPT connectFunction = \
								( STM_CONNECT_FUNCTION_OPT ) \
								FNPTR_GET( netStream->connectFunctionOpt );
		REQUIRES_S( connectFunction != NULL );

		status = connectFunction( stream, connectInfo );
		if( cryptStatusError( status ) )
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
   (usually a URL) into <schema>://<host>[:<port>]/<path>[?<query>]
   components and opens a connection to the host for non-stateless
   protocols */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int sNetConnect( OUT STREAM *stream, 
				 IN_ENUM( STREAM_PROTOCOL ) \
					const STREAM_PROTOCOL_TYPE protocol,
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
	assert( connectInfo->options != NET_OPTION_HOSTNAME || \
			isReadPtrDynamic( connectInfo->name, \
							  connectInfo->nameLength ) );

#ifdef USE_EAP
	REQUIRES( protocol == STREAM_PROTOCOL_TCP || \
			  protocol == STREAM_PROTOCOL_UDP || \
			  protocol == STREAM_PROTOCOL_HTTP || \
			  protocol == STREAM_PROTOCOL_EAP );
#else
	REQUIRES( protocol == STREAM_PROTOCOL_TCP || \
			  protocol == STREAM_PROTOCOL_UDP || \
			  protocol == STREAM_PROTOCOL_HTTP );
#endif /* USE_EAP */
	REQUIRES( isEnumRange( connectInfo->options, NET_OPTION ) );
	REQUIRES( connectInfo->options != NET_OPTION_HOSTNAME || \
			  ( connectInfo->options == NET_OPTION_HOSTNAME && \
			    connectInfo->name != NULL && \
				isShortIntegerRangeNZ( connectInfo->nameLength ) && \
				connectInfo->networkSocket == CRYPT_ERROR ) );
	REQUIRES( ( connectInfo->options != NET_OPTION_NETWORKSOCKET && \
				connectInfo->options != NET_OPTION_NETWORKSOCKET_DUMMY ) || 
			  ( ( connectInfo->options == NET_OPTION_NETWORKSOCKET || \
				  connectInfo->options == NET_OPTION_NETWORKSOCKET_DUMMY ) && \
				connectInfo->name == NULL && connectInfo->nameLength == 0 && \
				connectInfo->interface == NULL && connectInfo->interfaceLength == 0 && \
				connectInfo->networkSocket != CRYPT_ERROR ) );
	REQUIRES( connectInfo->options != NET_OPTION_VIRTUAL ||	\
			  ( connectInfo->options == NET_OPTION_VIRTUAL && \
			    connectInfo->name != NULL && \
				isShortIntegerRangeNZ( connectInfo->nameLength ) && \
				connectInfo->networkSocket == CRYPT_ERROR && \
				FNPTR_ISSET( connectInfo->virtualGetDataFunction ) && \
				FNPTR_ISSET( connectInfo->virtualPutDataFunction ) && 
				FNPTR_ISSET( connectInfo->virtualGetErrorInfoFunction ) && \
				DATAPTR_ISSET( connectInfo->virtualStateInfo ) ) );
	REQUIRES( connectInfo->iUserObject == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( connectInfo->iUserObject ) );
	REQUIRES( ( connectInfo->authName == NULL && \
				connectInfo->authNameLength == 0 && \
				connectInfo->authKey == NULL && \
				connectInfo->authKeyLength == 0 ) || \
			  ( connectInfo->authName != NULL && \
				isShortIntegerRangeNZ( connectInfo->authNameLength ) && \
				connectInfo->authKey != NULL && \
				isShortIntegerRangeNZ( connectInfo->authKeyLength ) ) );

	/* Clear return values */
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Initialise the network stream info */
	status = initStream( stream, &netStream, protocol, connectInfo, FALSE );
	if( cryptStatusError( status ) )
		return( status );
	if( connectInfo->options == NET_OPTION_HOSTNAME || \
		connectInfo->options == NET_OPTION_VIRTUAL )
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
	status = completeConnect( stream, &netStream, connectInfo, urlInfoPtr, 
							  protocol, proxyURL, proxyUrlLen, errorInfo );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckNetStream( &netStream ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int sNetListen( OUT STREAM *stream, 
				IN_ENUM( STREAM_PROTOCOL ) \
					const STREAM_PROTOCOL_TYPE protocol,
				const NET_CONNECT_INFO *connectInfo, 
				OUT ERROR_INFO *errorInfo )
	{
	NET_STREAM_INFO netStream;
	URL_INFO urlInfo, *urlInfoPtr = NULL;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( protocol == STREAM_PROTOCOL_TCP || \
			  protocol == STREAM_PROTOCOL_UDP || \
			  protocol == STREAM_PROTOCOL_HTTP );
	REQUIRES( connectInfo->options == NET_OPTION_HOSTNAME || \
			  connectInfo->options == NET_OPTION_NETWORKSOCKET || \
			  connectInfo->options == NET_OPTION_VIRTUAL );
	REQUIRES( connectInfo->options != NET_OPTION_HOSTNAME || \
			  ( connectInfo->options == NET_OPTION_HOSTNAME && \
				( ( connectInfo->interface == NULL && \
					connectInfo->interfaceLength == 0 ) || \
				  ( connectInfo->interface != NULL && \
					isShortIntegerRangeNZ( connectInfo->interfaceLength ) ) ) && \
				connectInfo->networkSocket == CRYPT_ERROR ) );
	REQUIRES( ( connectInfo->options != NET_OPTION_NETWORKSOCKET && \
				connectInfo->options != NET_OPTION_NETWORKSOCKET_DUMMY ) || 
			  ( ( connectInfo->options == NET_OPTION_NETWORKSOCKET || \
				  connectInfo->options == NET_OPTION_NETWORKSOCKET_DUMMY ) && \
				connectInfo->interface == NULL && connectInfo->interfaceLength == 0 && \
				connectInfo->networkSocket != CRYPT_ERROR ) );
	REQUIRES( connectInfo->options != NET_OPTION_VIRTUAL ||	\
			  ( connectInfo->options == NET_OPTION_VIRTUAL && \
			    connectInfo->name == NULL && connectInfo->nameLength == 0 && \
				connectInfo->networkSocket == CRYPT_ERROR && \
				FNPTR_ISSET( connectInfo->virtualGetDataFunction ) && \
				FNPTR_ISSET( connectInfo->virtualPutDataFunction ) && 
				FNPTR_ISSET( connectInfo->virtualGetErrorInfoFunction ) && \
				DATAPTR_ISSET( connectInfo->virtualStateInfo ) ) );
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
	status = completeConnect( stream, &netStream, connectInfo, urlInfoPtr, 
							  protocol, NULL, 0, errorInfo );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckNetStream( &netStream ) );

	return( CRYPT_OK );
	}

#ifdef CONFIG_FUZZ

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sNetDisconnect( INOUT STREAM *stream )
	{
	return( CRYPT_OK );
	}

#else

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sNetDisconnect( INOUT STREAM *stream )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES_S( netStream != NULL && sanityCheckNetStream( netStream ) );

	cleanupStream( stream, TRUE );

	return( CRYPT_OK );
	}
#endif /* CONFIG_FUZZ */

/* Parse a URL into its various components */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sNetParseURL( OUT URL_INFO *urlInfo, 
				  IN_BUFFER( urlLen ) const BYTE *url, 
				  IN_LENGTH_SHORT const int urlLen, 
				  IN_ENUM_OPT( URL_TYPE ) const URL_TYPE urlTypeHint )
	{
	assert( isWritePtr( urlInfo, sizeof( URL_INFO ) ) );
	assert( isReadPtrDynamic( url, urlLen ) );

	REQUIRES( isShortIntegerRangeNZ( urlLen ) );
	REQUIRES( isEnumRangeOpt( urlTypeHint, URL_TYPE ) );

	return( parseURL( urlInfo, url, urlLen, CRYPT_UNUSED, urlTypeHint, 
					  TRUE ) );
	}

/* Get extended information about an error status on a network connection.
   This has to be made explicit rather than being implicitly propagated up
   from the netStream to the stream in order to preserve an initial error
   state that might otherwise be overwritten by later errors, for example
   an sread() error that's overwritten by an error on close that follows
   the failed read */

#ifdef CONFIG_FUZZ

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sNetGetErrorInfo( INOUT STREAM *stream, OUT ERROR_INFO *errorInfo )
	{
	return;
	}

#else

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sNetGetErrorInfo( INOUT STREAM *stream, OUT ERROR_INFO *errorInfo )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

	assert( isReadPtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES_V( netStream != NULL && sanityCheckNetStream( netStream ) );

	/* Remember the error code and message */
	copyErrorInfo( errorInfo, NETSTREAM_ERRINFO );
	}
#endif /* CONFIG_FUZZ */

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
	UNUSED_ARG( connectInfo );

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
	UNUSED_ARG( connectInfo );

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
				  IN_BUFFER( urlLen ) const BYTE *url, 
				  IN_LENGTH_SHORT const int urlLen, 
				  IN_ENUM_OPT( URL_TYPE ) const URL_TYPE urlTypeHint )
	{
	UNUSED_ARG( url );

	memset( urlInfo, 0, sizeof( URL_INFO ) );

	return( CRYPT_ERROR_BADDATA );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sNetGetErrorInfo( INOUT STREAM *stream, 
					   OUT ERROR_INFO *errorInfo )
	{
	UNUSED_ARG( stream );

	memset( errorInfo, 0, sizeof( ERROR_INFO ) );
	}
#endif /* USE_TCP */
