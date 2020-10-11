/****************************************************************************
*																			*
*						  cryptlib HTTP Write Routines						*
*						Copyright Peter Gutmann 1998-2017					*
*																			*
****************************************************************************/

#include <ctype.h>
#include <stdio.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "http.h"
#else
  #include "crypt.h"
  #include "io/http.h"
#endif /* Compiler-specific includes */

#ifdef USE_HTTP

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Encode a string as per RFC 1866 (although the list of characters that 
   need to be escaped is itself given in RFC 2396, section 2.2 "Reserved 
   Characters").  Characters that are permitted/not permitted are:

	 !"#$%&'()*+,-./:;<=>?@[\]^_`{|}~
	x..x.xx....x...xxxxxxxxxxxx.xxxxx

   Because of this it's easier to check for the most likely permitted
   characters (alphanumerics) first, and only then to check for any special-
   case characters */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encodeRFC1866( INOUT STREAM *headerStream, 
						  IN_BUFFER( stringLength) const char *string, 
						  IN_LENGTH_SHORT const int stringLength )
	{
	static const char allowedChars[] = "$-_.!*'(),\"/\x00\x00";
					  /* RFC 1738 section 2.2 "URL Character Encoding 
						 Issues" + '/' */
	int index, status, LOOP_ITERATOR;

	assert( isWritePtr( headerStream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( string, stringLength ) );

	REQUIRES( isShortIntegerRangeNZ( stringLength ) );

	LOOP_MAX( index = 0, index < stringLength, index++ )
		{
		const int ch = byteToInt( string[ index ] );
		int i, LOOP_ITERATOR_ALT;

		if( isAlnum( ch ) )
			{
			status = sputc( headerStream, ch );
			if( cryptStatusError( status ) )
				return( status );
			continue;
			}
		if( ch == ' ' )
			{
			status = sputc( headerStream, '+' );
			if( cryptStatusError( status ) )
				return( status );
			continue;
			}
		LOOP_MED_ALT( i = 0, 
					  allowedChars[ i ] != '\0' && \
						ch != allowedChars[ i ] && \
						i < FAILSAFE_ARRAYSIZE( allowedChars, char ), i++ );
		ENSURES( LOOP_BOUND_OK_ALT );
		ENSURES( i < FAILSAFE_ARRAYSIZE( allowedChars, char ) );
		if( allowedChars[ i ] != '\0' )
			{
			/* It's in the allowed-chars list, output it verbatim */
			sputc( headerStream, ch );
			}
		else
			{
			char escapeString[ 8 + 8 ];
			int escapeStringLen;

			/* It's a special char, escape it */
			escapeStringLen = sprintf_s( escapeString, 8, "%%%02X", ch );
			ENSURES( escapeStringLen > 0 && escapeStringLen < 8 );
			status = swrite( headerStream, escapeString, escapeStringLen );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	
	return( CRYPT_OK );
	}

/* If we time out when sending HTTP header data this would usually be 
   reported as a CRYPT_ERROR_TIMEOUT by the lower-level network I/O 
   routines, however due to the multiple layers of I/O and the fact that to 
   the caller the write of the out-of-band HTTP header data (which can occur 
   as part of a standard HTTP write, but also in a GET or when sending an 
   error response) is invisible, we have to perform an explicit check to 
   make sure that we sent everything */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendHTTPData( INOUT STREAM *stream, 
				  IN_BUFFER( length ) void *buffer, 
				  IN_LENGTH const int length, 
				  IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
	int bytesWritten, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_BUFFER_SIZE );
	REQUIRES( flags == TRANSPORT_FLAG_NONE || \
			  flags == TRANSPORT_FLAG_FLUSH );
	REQUIRES( netStream != NULL && sanityCheckNetStream( netStream ) );

	status = bufferedTransportWrite( stream, buffer, length, &bytesWritten, 
									 flags );
	if( cryptStatusError( status ) )
		{
		/* Network-level error, the lower-level layers have reported the 
		   error details */
		return( status );
		}
	if( bytesWritten < length )
		{
		/* The write timed out, convert the incomplete HTTP header write to 
		   the appropriate timeout error */
		retExt( CRYPT_ERROR_TIMEOUT, 
				( CRYPT_ERROR_TIMEOUT, NETSTREAM_ERRINFO, 
				  "HTTP write timed out before all data could be written" ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Write Request Header						*
*																			*
****************************************************************************/

/* Write an HTTP request header.  The forceGet flag is used when we should 
   be using a POST but a broken server forces the use of a GET */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeReqMethod( INOUT STREAM *stream, 
						   const NET_STREAM_INFO *netStream,
						   const BOOLEAN usePost )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( usePost == TRUE || usePost == FALSE );

	if( TEST_FLAG( netStream->nhFlags, STREAM_NHFLAG_TUNNEL ) )
		swrite( stream, "CONNECT ", 8 );
	else
		{
		if( usePost )
			swrite( stream, "POST ", 5 );
		else
			swrite( stream, "GET ", 4 );
		}
	if( TEST_FLAG( netStream->nhFlags,
				   STREAM_NHFLAG_PROXY | STREAM_NHFLAG_TUNNEL ) )
		{
		/* If we're going through an HTTP proxy/tunnel, send an absolute URL 
		   rather than just the relative location */
		if( TEST_FLAG( netStream->nhFlags, STREAM_NHFLAG_PROXY ) )
			swrite( stream, "http://", 7 );
		status = swrite( stream, netStream->host, netStream->hostLen );
		if( cryptStatusOK( status ) && netStream->port != 80 )
			{
			char portString[ 16 + 8 ];
			int portStringLength;

			portStringLength = sprintf_s( portString, 16, ":%d", 
										  netStream->port );
			ENSURES( portStringLength > 0 && portStringLength < 16 );
			status = swrite( stream, portString, portStringLength );
			}
		if( cryptStatusError( status ) )
			return( status );
		}
	if( !TEST_FLAG( netStream->nhFlags, STREAM_NHFLAG_TUNNEL ) )
		{
		if( netStream->path != NULL && netStream->pathLen > 0 )
			{
			status = swrite( stream, netStream->path, 
							 netStream->pathLen );
			}
		else
			status = sputc( stream, '/' );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeReqURI( INOUT STREAM *stream, 
						IN const HTTP_REQ_INFO *httpReqInfo )
	{
	int status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( httpReqInfo, sizeof( HTTP_REQ_INFO * ) ) );

	/* If there's no additional URI data present then we're done */
	if( httpReqInfo->attributeLen == 0 && httpReqInfo->valueLen == 0 && \
		httpReqInfo->extraDataLen == 0 )
		return( CRYPT_OK );

	REQUIRES( httpReqInfo->attributeLen > 0 && \
			  httpReqInfo->valueLen > 0 );

	/* Send optional additional request information:

		...?attribute=value&extraData */
	sputc( stream, '?' );
	swrite( stream, httpReqInfo->attribute, httpReqInfo->attributeLen );
	sputc( stream, '=' );
	status = encodeRFC1866( stream, httpReqInfo->value, 
							httpReqInfo->valueLen );
	if( cryptStatusOK( status ) && httpReqInfo->extraDataLen > 0 )
		{
		sputc( stream, '&' );
		status = swrite( stream, httpReqInfo->extraData, 
						 httpReqInfo->extraDataLen );
		}

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeReqTrailer( INOUT STREAM *stream, 
							const NET_STREAM_INFO *netStream )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* If it's an HTTP 1.0 request we just write the basic header */
	if( isHTTP10( netStream ) )
		{
		swrite( stream, " HTTP/1.0\r\n", 11 );
		return( swrite( stream, "Connection: keep-alive\r\n", 24 ) );
		}

	/* HTTP 1.1 has an addition Host: header, as well as other situation-
	   specific headers, present */
	swrite( stream, " HTTP/1.1\r\nHost: ", 17 );
	swrite( stream, netStream->host, netStream->hostLen );
#ifdef USE_WEBSOCKETS
	if( TEST_FLAG( netStream->nhFlags, STREAM_NHFLAG_WS_UPGRADE ) )
		return( swrite( stream, "\r\n", 2 ) );
#endif /* USE_WEBSOCKETS */
	swrite( stream, "\r\n", 2 );
	if( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_LASTMSGW ) )
		return( swrite( stream, "Connection: close\r\n", 19 ) );

	/* The following shouldn't be required for HTTP 1.1 but there are a 
	   sufficient number of broken caches and proxies around that we need to 
	   include it for the ones that helpfully close the *HTTP 1.1 
	   persistent* connection for us if they don't see an HTTP 1.0 keepalive 
	   in the header */
	return( swrite( stream, "Connection: keep-alive\r\n", 24 ) );
	}

#ifdef USE_WEBSOCKETS

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeReqUpgradeHeaders( INOUT STREAM *stream, 
								   IN const HTTP_REQ_INFO *httpReqInfo )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( httpReqInfo, sizeof( HTTP_REQ_INFO * ) ) );

	REQUIRES( isShortIntegerRangeNZ( httpReqInfo->authLen ) );

	/* We're doing a WebSockets protocol upgrade, write the necessary 
	   upgrade headers */
	swrite( stream, "Upgrade: websocket\r\n", 20 );
	swrite( stream, "Connection: upgrade\r\n", 21 );
	swrite( stream, "Sec-WebSocket-Key: ", 19 );
	swrite( stream, httpReqInfo->auth, httpReqInfo->authLen );
	swrite( stream, "\r\n", 2 );
	if( httpReqInfo->protocolLen > 0 )
		{
		swrite( stream, "Sec-WebSocket-Protocol: ", 24 );
		swrite( stream, httpReqInfo->protocol, httpReqInfo->protocolLen ); 
		swrite( stream, "\r\n", 2 );
		}
	swrite( stream, "Sec-Websocket-Version: 13\r\n", 27 );
	return( swrite( stream, "\r\n", 2 ) );
	}
#endif /* USE_WEBSOCKETS */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeContentHeaders( INOUT STREAM *stream, 
								IN_BUFFER( contentTypeLen ) \
									const char *contentType, 
								IN_LENGTH_SHORT const int contentTypeLen, 
								IN_LENGTH const int contentLength )
	{
	char lengthString[ 16 + 8 ];
	int lengthStringLength;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( contentType, contentTypeLen ) );

	REQUIRES( isShortIntegerRangeNZ( contentTypeLen ) );
	REQUIRES( contentLength > 0 && contentLength < MAX_BUFFER_SIZE );

	/* In terms of cacheing, there are two control directives that we're
	   interested in, no-cache and no-store.  The HTTP 1.0 no-cache doesn't 
	   mean that the data won't be cached, merely that it has to be re-
	   validated on each fetch, while the HTTP 1.1 no-store actually means
	   what no-cache would seem to mean, that the data will never be stored/
	   cached.  However, no-store was meant mostly as a privacy mechanism,
	   indicating that a cache shouldn't store user credentials or PII.  In
	   the case of browsers, in order to get what you'd expect from no-cache
	   you need to use both no-cache and no-store, with different ones
	   affecting different browsers, and with the additional twist that some
	   browsers implement no-cache like it was no-store */
	swrite( stream, "Content-Type: ", 14 );
	swrite( stream, contentType, contentTypeLen );
	swrite( stream, "\r\nContent-Length: ", 18 );
	lengthStringLength = sprintf_s( lengthString, 16, "%d", 
									contentLength );
	ENSURES( lengthStringLength > 0 && lengthStringLength < 16 );
	swrite( stream, lengthString, lengthStringLength );
	swrite( stream, "\r\nCache-Control: no-cache, no-store\r\n", 37 );
	return( swrite( stream, "\r\n", 2 ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeRequestHeader( INOUT STREAM *stream, 
						IN_OPT const HTTP_REQ_INFO *httpReqInfo,
						IN_BUFFER_OPT( contentTypeLen ) const char *contentType, 
						IN_LENGTH_SHORT_Z const int contentTypeLen, 
						IN_LENGTH_Z const int contentLength,
						const BOOLEAN forceGet )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
	STREAM headerStream;
	char headerBuffer[ HTTP_LINEBUF_SIZE + 8 ];
	const int transportFlag = ( contentLength > 0 && !forceGet ) ? \
							  TRANSPORT_FLAG_NONE : TRANSPORT_FLAG_FLUSH;
	int headerLength DUMMY_INIT, status DUMMY_INIT;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( ( httpReqInfo == NULL ) || \
			isReadPtr( httpReqInfo, sizeof( HTTP_REQ_INFO * ) ) );
	assert( ( contentLength == 0 && contentType == NULL && \
			  contentTypeLen == 0 ) || \
			( contentLength >= 1 && \
			  isReadPtrDynamic( contentType, contentTypeLen ) ) );
	
	REQUIRES( ( contentLength == 0 && contentType == NULL && \
				contentTypeLen == 0 ) || \
			  ( contentLength > 0 && contentLength < MAX_BUFFER_SIZE && \
			    contentType != NULL && \
				isShortIntegerRangeNZ( contentTypeLen ) ) );
	REQUIRES( ( httpReqInfo == NULL ) || \
			  ( httpReqInfo != NULL && \
				( ( httpReqInfo->attributeLen == 0 && \
					httpReqInfo->valueLen == 0 ) || \
				  ( httpReqInfo->valueLen > 0 ) ) ) );
	REQUIRES( forceGet == TRUE || forceGet == FALSE );
	REQUIRES( netStream != NULL && sanityCheckNetStream( netStream ) );

	sMemOpen( &headerStream, headerBuffer, HTTP_LINEBUF_SIZE );
	status = writeReqMethod( &headerStream, netStream,
							 ( contentLength > 0 && !forceGet ) ? \
							   TRUE : FALSE );
	ENSURES( cryptStatusOK( status ) );
	if( httpReqInfo != NULL )
		{
		status = writeReqURI( &headerStream, httpReqInfo );
		ENSURES( cryptStatusOK( status ) );
		}
	status = writeReqTrailer( &headerStream, netStream );
	ENSURES( cryptStatusOK( status ) );
#ifdef USE_WEBSOCKETS
	if( TEST_FLAG( netStream->nhFlags, STREAM_NHFLAG_WS_UPGRADE ) )
		{
		REQUIRES( httpReqInfo != NULL );
		status = writeReqUpgradeHeaders( &headerStream, httpReqInfo );
		}
	else
#endif /* USE_WEBSOCKETS */
		{
		if( !forceGet && contentLength > 0 )
			{
			status = writeContentHeaders( &headerStream, contentType, 
										  contentTypeLen, contentLength );
			}
		else
			status = swrite( &headerStream, "\r\n", 2 );
		}
	if( cryptStatusOK( status ) )
		headerLength = stell( &headerStream );
	sMemDisconnect( &headerStream );
	ENSURES( cryptStatusOK( status ) );
	return( sendHTTPData( stream, headerBuffer, headerLength, 
						  transportFlag ) );
	}

/****************************************************************************
*																			*
*								Write Response Header						*
*																			*
****************************************************************************/

/* Write an HTTP response header */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeRespMethod( INOUT STREAM *stream, 
							const NET_STREAM_INFO *netStream,
							const BOOLEAN isHTTP10 )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( isHTTP10 == TRUE || isHTTP10 == FALSE );

	/* Write the appropriate response method */
	if( isHTTP10 )
		{
		swrite( stream, "HTTP/1.0 200 OK\r\n", 17 );
		return( swrite( stream, "Connection: keep-alive\r\n", 24 ) );
		}
#ifdef USE_WEBSOCKETS
	if( TEST_FLAG( netStream->nhFlags, STREAM_NHFLAG_WS_UPGRADE ) )
		{
		return( swrite( stream, 
						"HTTP/1.1 101 Switching Underwear\r\n", 34 ) );
		}
#endif /* USE_WEBSOCKETS */
	swrite( stream, "HTTP/1.1 200 OK\r\n", 17 );
	if( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_LASTMSGW ) )
		return( swrite( stream, "Connection: close\r\n", 19 ) );

	/* This shouldn't be required for HTTP 1.1 but there are a sufficient 
	   number of broken caches and proxies around that we need to include it 
	   for the ones that helpfully close the *HTTP 1.1 persistent* 
	   connection for us if they don't see an HTTP 1.0 keepalive in the 
	   header */
	return( swrite( stream, "Connection: keep-alive\r\n", 24 ) );
	}

#ifdef USE_WEBSOCKETS

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeRespUpgradeHeaders( INOUT STREAM *stream, 
									IN const HTTP_REQ_INFO *httpReqInfo )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( httpReqInfo, sizeof( HTTP_REQ_INFO * ) ) );

	REQUIRES( isShortIntegerRangeNZ( httpReqInfo->authLen ) );

	/* We're doing a WebSockets protocol upgrade, write the necessary 
	   upgrade headers */
	swrite( stream, "Upgrade: websocket\r\n", 20 );
	swrite( stream, "Connection: upgrade\r\n", 21 );
	swrite( stream, "Sec-WebSocket-Accept: ", 22 );
	swrite( stream, httpReqInfo->auth, httpReqInfo->authLen );
	swrite( stream, "\r\n", 2 );
	if( httpReqInfo->protocolLen > 0 )
		{
		swrite( stream, "Sec-WebSocket-Protocol: ", 24 );
		swrite( stream, httpReqInfo->protocol, httpReqInfo->protocolLen ); 
		swrite( stream, "\r\n", 2 );
		}
	return( swrite( stream, "\r\n", 2 ) );
	}
#endif /* USE_WEBSOCKETS */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeResponseHeader( INOUT STREAM *stream, 
								IN_OPT const HTTP_REQ_INFO *httpReqInfo,
								IN_BUFFER_OPT( contentTypeLen ) \
									const char *contentType, 
								IN_LENGTH_SHORT_Z const int contentTypeLen, 
								IN_LENGTH_Z const int contentLength )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
	STREAM headerStream;
	char headerBuffer[ HTTP_LINEBUF_SIZE + 8 ];
	const int transportFlag = ( contentLength > 0 ) ? \
							  TRANSPORT_FLAG_NONE : TRANSPORT_FLAG_FLUSH;
	int headerLength DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( ( contentLength == 0 && contentType == NULL && \
			  contentTypeLen == 0 && \
			  isReadPtr( httpReqInfo, sizeof( HTTP_REQ_INFO ) ) ) || \
			( contentLength >= 1 && \
			  isReadPtrDynamic( contentType, contentTypeLen ) && \
			  httpReqInfo == NULL ) );

	REQUIRES( ( contentLength == 0 && contentType == NULL && \
				contentTypeLen == 0 && httpReqInfo != NULL && \
				httpReqInfo->authLen != 0  ) || \
			  ( contentLength > 0 && contentLength < MAX_BUFFER_SIZE && \
			    contentType != NULL && \
				isShortIntegerRangeNZ( contentTypeLen ) && \
				httpReqInfo == NULL ) );
	REQUIRES( netStream != NULL && sanityCheckNetStream( netStream ) );

	sMemOpen( &headerStream, headerBuffer, HTTP_LINEBUF_SIZE );
	status = writeRespMethod( &headerStream, netStream, 
							  isHTTP10( netStream ) );
	ENSURES( cryptStatusOK( status ) );
#ifdef USE_WEBSOCKETS
	if( TEST_FLAG( netStream->nhFlags, STREAM_NHFLAG_WS_UPGRADE ) )
		{
		REQUIRES( httpReqInfo != NULL );
		status = writeRespUpgradeHeaders( &headerStream, httpReqInfo );
		}
	else
#endif /* USE_WEBSOCKETS */
		{
		if( contentLength > 0 )
			{
			status = writeContentHeaders( &headerStream, contentType, 
										  contentTypeLen, contentLength );
			}
		else
			status = swrite( &headerStream, "\r\n", 2 );
		}
	if( cryptStatusOK( status ) )
		headerLength = stell( &headerStream );
	sMemDisconnect( &headerStream );
	ENSURES( cryptStatusOK( status ) );
	return( sendHTTPData( stream, headerBuffer, headerLength, 
						  transportFlag ) );
	}

/****************************************************************************
*																			*
*							HTTP Access Functions							*
*																			*
****************************************************************************/

/* Write data to an HTTP stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int writeFunction( INOUT STREAM *stream, 
						  IN_BUFFER( maxLength ) const void *buffer, 
						  IN_LENGTH_FIXED( sizeof( HTTP_DATA_INFO ) ) \
							const int maxLength, 
						  OUT_DATALENGTH_Z int *length )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
	HTTP_DATA_INFO *httpDataInfo = ( HTTP_DATA_INFO * ) buffer;
	BOOLEAN forceGet = FALSE;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	
	REQUIRES( netStream != NULL && sanityCheckNetStream( netStream ) );
	REQUIRES( maxLength == sizeof( HTTP_DATA_INFO ) );
	REQUIRES( sanityCheckHttpDataInfo( httpDataInfo ) );

	/* Clear return value */
	*length = 0;

	/* Send the out-of-band HTTP header data to the client or server */
	if( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_ISSERVER ) )
		{
		/* If it's an error status response, send the translated error 
		   status and exit */
		if( cryptStatusError( httpDataInfo->reqStatus ) )
			{
			char headerBuffer[ HTTP_LINEBUF_SIZE + 8 ];

			status = sendHTTPError( stream, headerBuffer, HTTP_LINEBUF_SIZE,
						( httpDataInfo->reqStatus == CRYPT_ERROR_NOTFOUND ) ? \
							404 : \
						( httpDataInfo->reqStatus == CRYPT_ERROR_PERMISSION ) ? \
							401 : 400 );
			if( cryptStatusError( status ) )
				return( status );
			*length = maxLength;

			return( CRYPT_OK );
			}

		status = writeResponseHeader( stream, httpDataInfo->reqInfo, 
									  httpDataInfo->contentType,
									  httpDataInfo->contentTypeLen,
									  httpDataInfo->bytesToWrite );
		}
	else
		{
		REQUIRES( TEST_FLAG( netStream->nhFlags, 
							 STREAM_NHFLAG_TUNNEL ) || \
				  TEST_FLAG( netStream->nhFlags, 
							 STREAM_NHFLAG_GET ) || \
				  TEST_FLAG( netStream->nhFlags, 
							 STREAM_NHFLAG_POST_AS_GET ) || \
				  httpDataInfo->contentTypeLen > 0 );
		REQUIRES( !( TEST_FLAG( netStream->nhFlags, 
								STREAM_NHFLAG_PROXY ) && 
					 TEST_FLAG( netStream->nhFlags, 
								STREAM_NHFLAG_TUNNEL ) ) );
		REQUIRES( netStream->host != NULL && netStream->hostLen > 0 );

		/* If we have to override the use of the (correct) POST with the 
		   (incorrect) GET in order to deal with a broken server then 
		   instead of writing the full request header we write the start of 
		   the "HTTP GET..." line and then exit, leaving the rest of the GET 
		   URI to be written as the payload data */
		if( TEST_FLAG( netStream->nhFlags, STREAM_NHFLAG_POST_AS_GET ) )
			{
			status = writeRequestHeader( stream, httpDataInfo->reqInfo, 
										 NULL, 0, 0, TRUE );
			forceGet = TRUE;
			}
		else
			{
			status = writeRequestHeader( stream, httpDataInfo->reqInfo, 
										 httpDataInfo->contentType,
										 httpDataInfo->contentTypeLen,
										 httpDataInfo->bytesToWrite, FALSE );
			}
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If we're sending a headers-only request, for example for a GET 
	   request or to return an HTTP error, we're done */
	if( httpDataInfo->buffer == NULL )
		return( CRYPT_OK );

	/* Send the payload data to the client/server */
	status = bufferedTransportWrite( stream, httpDataInfo->buffer, 
									 httpDataInfo->bytesToWrite, 
									 &httpDataInfo->bytesTransferred, 
									 forceGet ? TRANSPORT_FLAG_NONE : \
												TRANSPORT_FLAG_FLUSH );
	if( cryptStatusError( status ) )
		return( status );
	if( forceGet )
		{
		STREAM headerStream;
		char headerBuffer[ HTTP_LINEBUF_SIZE + 8 ];
		int headerLength DUMMY_INIT;

		/* We've been forced to override the use of a POST with a GET due to 
		   a broken server so the header write was split into two parts with 
		   the request data in the middle, we now have to send the remainder 
		   of the header */
		sMemOpen( &headerStream, headerBuffer, HTTP_LINEBUF_SIZE );
		status = writeReqTrailer( &headerStream, netStream );
		ENSURES( cryptStatusOK( status ) );
		status = swrite( &headerStream, "\r\n", 2 );
		if( cryptStatusOK( status ) )
			headerLength = stell( &headerStream );
		sMemDisconnect( &headerStream );
		ENSURES( cryptStatusOK( status ) );
		status = sendHTTPData( stream, headerBuffer, headerLength, 
							   TRANSPORT_FLAG_FLUSH );
		if( cryptStatusError( status ) )
			return( status );
		}
	*length = maxLength;

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerHTTPwrite( INOUT NET_STREAM_INFO *netStream )
	{
	/* Set the remaining access method pointers */
	FNPTR_SET( netStream->writeFunction, writeFunction );
	}
#endif /* USE_HTTP */
