/****************************************************************************
*																			*
*							cryptlib HTTP Routines							*
*					  Copyright Peter Gutmann 1998-2017						*
*																			*
****************************************************************************/

#include <ctype.h>
#include <stdio.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "http.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "io/http.h"
#endif /* Compiler-specific includes */

#ifdef USE_HTTP

/* HTTP error/warning messages and equivalent cryptlib status codes.  This 
   mapping is somewhat problematic in that the HTTP-level status codes can 
   be overwritten/replaced by intermediaries like proxies and captive 
   portals, for example if an intermediate device is under heavy load or 
   thinks it's under attack then it can return a different HTTP status than 
   what the actual client or server sent.  In addition at the same time that 
   we're mapping HTTP status codes to approximate cryptlib codes, so the 
   remote system could be mapping its status codes to approximate HTTP ones.  
   For this reason we map the HTTP codes to the most generic equivalents 
   possible, mostly CRYPT_ERROR_READ with an occasional 
   CRYPT_ERROR_PERMISSION when it's obviously that, e.g. a 403.

   The mapped status for 30x redirects is somewhat special-case, see the 
   comment in readResponseHeader() for details.  
   
   This table also contains known non-HTTP codes in the expectation that, 
   when used as a general-purpose substrate, it'll be pressed into use in 
   all sorts of situations */

#define HTTP_STATUSSTRING_LENGTH	3

static const HTTP_STATUS_INFO httpStatusInfo[] = {
	{ 100, "100", "Continue", 8, OK_SPECIAL },
#ifdef USE_WEBSOCKETS
	{ 101, "101", "Switching Protocols", 19, OK_SPECIAL },
#else
	{ 101, "101", "Switching Protocols", 19, CRYPT_ERROR_READ },
#endif /* USE_WEBSOCKETS */
	{ 110, "110", "Warning: Response is stale", 26, CRYPT_OK },
	{ 111, "111", "Warning: Revalidation failed", 28, CRYPT_OK },
	{ 112, "112", "Warning: Disconnected operation", 31, CRYPT_OK },
	{ 113, "113", "Warning: Heuristic expiration", 29, CRYPT_OK },
	{ 199, "199", "Warning: Miscellaneous warning", 30, CRYPT_OK },
	{ 200, "200", "OK", 2, CRYPT_OK },
	{ 201, "201", "Created", 7, CRYPT_ERROR_READ },
	{ 202, "202", "Accepted", 8, CRYPT_ERROR_READ },
	{ 203, "203", "Non-Authoritative Information", 29, CRYPT_OK },
	{ 204, "204", "No Content", 10, CRYPT_ERROR_READ },
	{ 205, "205", "Reset Content", 13, CRYPT_ERROR_READ },
	{ 206, "206", "Partial Content", 15, CRYPT_ERROR_READ },
	{ 214, "214", "Warning: Transformation applied", 31, CRYPT_OK },
	{ 250, "250", "RTSP: Low on Storage Space", 26, CRYPT_OK },
	{ 299, "299", "Warning: Miscellaneous persistent warning", 41, CRYPT_OK },
	{ 300, "300", "Multiple Choices", 16, CRYPT_ERROR_READ },
	{ 301, "301", "Moved Permanently", 17, OK_SPECIAL },
	{ 302, "302", "Moved Temporarily/Found", 23, OK_SPECIAL },
	{ 303, "303", "See Other", 9, CRYPT_ERROR_READ },
	{ 304, "304", "Not Modified", 12, CRYPT_ERROR_READ },
	{ 305, "305", "Use Proxy", 9, CRYPT_ERROR_READ },
	{ 306, "306", "Unused/obsolete", 15, CRYPT_ERROR_READ },
	{ 307, "307", "Temporary Redirect", 18, OK_SPECIAL },
	{ 400, "400", "Bad Request", 11, CRYPT_ERROR_READ },
	{ 401, "401", "Unauthorized", 12, CRYPT_ERROR_PERMISSION },
	{ 402, "402", "Payment Required", 16, CRYPT_ERROR_READ },
	{ 403, "403", "Forbidden", 9, CRYPT_ERROR_PERMISSION },
	{ 404, "404", "Not Found", 9, CRYPT_ERROR_NOTFOUND },
	{ 405, "405", "Method Not Allowed", 18, CRYPT_ERROR_NOTAVAIL },
	{ 406, "406", "Not Acceptable", 14, CRYPT_ERROR_PERMISSION },
	{ 407, "407", "Proxy Authentication Required", 29, CRYPT_ERROR_PERMISSION },
	{ 408, "408", "Request Time-out", 16, CRYPT_ERROR_READ },
	{ 409, "409", "Conflict", 8, CRYPT_ERROR_READ },
	{ 410, "410", "Gone", 4, CRYPT_ERROR_NOTFOUND },
	{ 411, "411", "Length Required", 15, CRYPT_ERROR_READ },
	{ 412, "412", "Precondition Failed", 19, CRYPT_ERROR_READ },
	{ 413, "413", "Request Entity too Large", 24, CRYPT_ERROR_OVERFLOW },
	{ 414, "414", "Request-URI too Large", 21, CRYPT_ERROR_OVERFLOW },
	{ 415, "415", "Unsupported Media Type", 22, CRYPT_ERROR_READ },
	{ 416, "416", "Requested range not satisfiable", 31, CRYPT_ERROR_READ },
	{ 417, "417", "Expectation Failed", 18, CRYPT_ERROR_READ },
	{ 418, "418", "I'm a teapot", 12, CRYPT_ERROR_READ },
	{ 421, "421", "Misdirected Request", 19, CRYPT_ERROR_READ },
	{ 422, "422", "Unprocessable Entity", 20, CRYPT_ERROR_READ },
	{ 423, "423", "Locked", 6, CRYPT_ERROR_READ },
	{ 424, "424", "Failed Dependency", 17, CRYPT_ERROR_READ },
	{ 426, "426", "Upgrade Required", 16, CRYPT_ERROR_READ },
	{ 428, "428", "Precondition Required", 21, CRYPT_ERROR_READ },
	{ 429, "429", "Too Many Requests", 17, CRYPT_ERROR_OVERFLOW },
	{ 431, "431", "Request Header Fields Too Large", 31, CRYPT_ERROR_OVERFLOW },
	{ 444, "444", "Connection Closed Without Response", 34, CRYPT_ERROR_READ },
	{ 451, "451", "RTSP: Parameter not Understood", 30, CRYPT_ERROR_BADDATA },
#if 0	/* Also allegedly... */
	{ 451, "451", "Unavailable For Legal Reasons", 29, CRYPT_ERROR_READ },
#endif /* 0 */
	{ 452, "452", "RTSP: Conference not Found", 26, CRYPT_ERROR_NOTFOUND },
	{ 453, "453", "RTSP: Not enough Bandwidth", 26, CRYPT_ERROR_NOTAVAIL },
	{ 454, "454", "RTSP: Session not Found", 23, CRYPT_ERROR_NOTFOUND },
	{ 455, "455", "RTSP: Method not Valid in this State", 36, CRYPT_ERROR_NOTAVAIL },
	{ 456, "456", "RTSP: Header Field not Valid for Resource", 41, CRYPT_ERROR_NOTAVAIL },
	{ 457, "457", "RTSP: Invalid Range", 19, CRYPT_ERROR_READ },
	{ 458, "458", "RTSP: Parameter is Read-Only", 28, CRYPT_ERROR_PERMISSION },
	{ 459, "459", "RTSP: Aggregate Operation not Allowed", 37, CRYPT_ERROR_PERMISSION },
	{ 460, "460", "RTSP: Only Aggregate Operation Allowed", 38, CRYPT_ERROR_PERMISSION },
	{ 461, "461", "RTSP: Unsupported Transport", 27, CRYPT_ERROR_NOTAVAIL },
	{ 462, "462", "RTSP: Destination Unreachable", 29, CRYPT_ERROR_OPEN },
	{ 499, "499", "Client Closed Request", 21, CRYPT_ERROR_READ },
	{ 500, "500", "Internal Server Error", 21, CRYPT_ERROR_READ },
	{ 501, "501", "Not Implemented", 15, CRYPT_ERROR_NOTAVAIL },
	{ 502, "502", "Bad Gateway", 11, CRYPT_ERROR_READ },
	{ 503, "503", "Service Unavailable", 19, CRYPT_ERROR_NOTAVAIL },
	{ 504, "504", "Gateway Time-out", 16, CRYPT_ERROR_TIMEOUT },
	{ 505, "505", "HTTP Version not supported", 26, CRYPT_ERROR_READ },
	{ 510, "510", "HTTP-Ext: Not Extended", 22, CRYPT_ERROR_READ },
	{ 551, "551", "RTSP: Option not supported", 26, CRYPT_ERROR_READ },
	{ 0, NULL, "Unrecognised HTTP status condition", 34, CRYPT_ERROR_READ },
		{ 0, NULL, "Unrecognised HTTP status condition", 34, CRYPT_ERROR_READ }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check HTTP data info */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckHttpDataInfo( const HTTP_DATA_INFO *httpDataInfo )
	{
	assert( isReadPtr( httpDataInfo, sizeof( HTTP_DATA_INFO ) ) );

	/* Check HTTP data information */
	if( httpDataInfo->buffer == NULL )
		{
		if( httpDataInfo->bufSize != 0 || \
			httpDataInfo->bytesToWrite != 0 )
			{
			DEBUG_PUTS(( "sanityCheckHttpDataInfo: Spurious data info" ));
			return( FALSE );
			}
		}
	else
		{
		if( httpDataInfo->bufSize < MIN_LINEBUF_SIZE || \
			httpDataInfo->bufSize > MAX_BUFFER_SIZE || \
			!safeBufferCheck( httpDataInfo->buffer, httpDataInfo->bufSize ) )
			{
			DEBUG_PUTS(( "sanityCheckHttpDataInfo: Data buffer" ));
			return( FALSE );
			}
		}
	if( httpDataInfo->bytesToWrite < 0 || \
		httpDataInfo->bytesToWrite > httpDataInfo->bufSize )
		{
		DEBUG_PUTS(( "sanityCheckHttpDataInfo: Data buffer write variables" ));
		return( FALSE );
		}
	if( httpDataInfo->bytesAvail < 0 || \
		httpDataInfo->bytesAvail > httpDataInfo->bufSize || \
		httpDataInfo->bytesTransferred < 0 || \
		httpDataInfo->bytesTransferred > httpDataInfo->bufSize )
		{
		DEBUG_PUTS(( "sanityCheckHttpDataInfo: Data buffer read variables" ));
		return( FALSE );
		}
	if( httpDataInfo->contentType != NULL )
		{
		if( httpDataInfo->contentTypeLen < 1 || \
			httpDataInfo->contentTypeLen > CRYPT_MAX_TEXTSIZE )
			{
			DEBUG_PUTS(( "sanityCheckHttpDataInfo: Content type info" ));
			return( FALSE );
			}
		}
	else
		{
		if( httpDataInfo->contentTypeLen != 0 )
			{
			DEBUG_PUTS(( "sanityCheckHttpDataInfo: Spurious content type length" ));
			return( FALSE );
			}
		}

	/* Check HTTP control variables */
	if( ( httpDataInfo->bufferResize != TRUE && \
		  httpDataInfo->bufferResize != FALSE ) || \
		( httpDataInfo->responseIsText != TRUE && \
		  httpDataInfo->responseIsText != FALSE ) || \
		httpDataInfo->reqType < STREAM_HTTPREQTYPE_NONE || \
		httpDataInfo->reqType >= STREAM_HTTPREQTYPE_LAST )
		{
		DEBUG_PUTS(( "sanityCheckHttpDataInfo: HTTP control variables" ));
		return( FALSE );
		}
	if( cryptStatusError( httpDataInfo->reqStatus ) )
		{
		/* If we're sending an error response then there can't also be data 
		   present */
		if( httpDataInfo->bytesToWrite != 0 )
			{
			DEBUG_PUTS(( "sanityCheckHttpDataInfo: Spurious data for error response" ));
			return( FALSE );
			}
		}
	else
		{
		if( httpDataInfo->reqStatus != 0 )
			{
			DEBUG_PUTS(( "sanityCheckHttpDataInfo: Request status" ));
			return( FALSE );
			}
		}
	if( httpDataInfo->reqInfo != NULL && \
		httpDataInfo->uriInfo != NULL )
		{
		DEBUG_PUTS(( "sanityCheckHttpDataInfo: Spurious req/URI info" ));
		return( FALSE );
		}

	return( TRUE );
	}

/* Initialise HTTP data info.  This is a rather complex function because it 
   has to handle reads and writes.  On read we use { buffer, bufSize } to
   read into.  On write we also use { buffer, bufSize } as our write buffer 
   but only write dataLength bytes from that.  In addition on read we fill
   the uriInfo with the peer's URI information, and on write we set the
   request information (URI and other details) from the reqInfo */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initHttpInfo( INOUT HTTP_DATA_INFO *httpDataInfo, 
				  OUT_BUFFER_OPT_FIXED( bufSize ) void *buffer, 
				  IN_LENGTH_Z const int bufSize,
				  IN_LENGTH_Z const int dataLength,
				  IN_OPT const HTTP_REQ_INFO *reqInfo,
				  OUT_OPT HTTP_URI_INFO *uriInfo )
	{
	assert( isWritePtr( httpDataInfo, sizeof( HTTP_DATA_INFO ) ) );
	assert( buffer == NULL || isWritePtr( buffer, bufSize ) );
	assert( reqInfo == NULL || isReadPtr( reqInfo, sizeof( HTTP_REQ_INFO ) ) );
	assert( uriInfo == NULL || isReadPtr( uriInfo, sizeof( HTTP_URI_INFO ) ) );

	REQUIRES( dataLength >= 0 && dataLength < MAX_BUFFER_SIZE && \
			  dataLength <= bufSize );
	REQUIRES( ( buffer == NULL && bufSize == 0 && dataLength == 0 ) || \
			  ( buffer != NULL && \
				bufSize >= MIN_LINEBUF_SIZE && bufSize < MAX_BUFFER_SIZE && \
				safeBufferCheck( buffer, bufSize ) ) );
	REQUIRES( reqInfo == NULL || uriInfo == NULL );

	memset( httpDataInfo, 0, sizeof( HTTP_DATA_INFO ) );
	httpDataInfo->buffer = buffer;
	httpDataInfo->bufSize = bufSize;
	httpDataInfo->bytesToWrite = dataLength;
	httpDataInfo->reqInfo = reqInfo;
	if( uriInfo != NULL )
		{
		memset( uriInfo, 0, sizeof( HTTP_URI_INFO ) );
		httpDataInfo->uriInfo = uriInfo;
		}

	ENSURES( sanityCheckHttpDataInfo( httpDataInfo ) );

	return( CRYPT_OK );
	}

/* Return the HTTP_STATUS_INFO that corresponds to a given HTTP status 
   code */

CHECK_RETVAL_PTR \
const HTTP_STATUS_INFO *getHTTPStatusInfo( IN_INT const int httpStatus )
	{
	static const HTTP_STATUS_INFO defaultStatusInfo = \
		{ 400, "400", "Bad Request", 11, CRYPT_ERROR_READ };
	int i, LOOP_ITERATOR;

	REQUIRES_N( httpStatus >= MIN_HTTP_STATUS && \
				httpStatus < MAX_HTTP_STATUS );

	/* Find the HTTP error string that corresponds to the HTTP status
	   value */
	LOOP_LARGE( i = 0, httpStatusInfo[ i ].httpStatus > 0 && \
					   httpStatusInfo[ i ].httpStatus != httpStatus && \
					   i < FAILSAFE_ARRAYSIZE( httpStatusInfo, HTTP_STATUS_INFO ),
			  i++ );
	ENSURES_N( LOOP_BOUND_OK );
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( httpStatusInfo, HTTP_STATUS_INFO ) );
	if( httpStatusInfo[ i ].httpStatus > 0 )
		return( &httpStatusInfo[ i ] );

	/* We couldn't find any matching status information, return the default 
	   status */
	return( &defaultStatusInfo );
	}

/* Check an "HTTP 1.x" ID string.  No PKI client should be sending us a 0.9
   ID, so we only allow 1.x */

CHECK_RETVAL_RANGE( 0, 8 ) STDC_NONNULL_ARG( ( 1, 3 ) ) \
int checkHTTPID( IN_BUFFER( dataLength ) const char *data, 
				 IN_LENGTH_SHORT const int dataLength, 
				 INOUT STREAM *stream )
	{
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );

	assert( isReadPtrDynamic( data, dataLength ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( isShortIntegerRangeNZ( dataLength ) );
	REQUIRES( netStream != NULL && sanityCheckNetStream( netStream ) );

	if( dataLength < 8 || strCompare( data, "HTTP/1.", 7 ) )
		return( CRYPT_ERROR_BADDATA );
	if( data[ 7 ] == '0' )
		SET_FLAG( netStream->nhFlags, STREAM_NHFLAG_HTTP10 );
	else
		{
		if( data[ 7 ] != '1' )
			return( CRYPT_ERROR_BADDATA );
		}

	return( 8 );
	}

/****************************************************************************
*																			*
*							Error Handling Functions						*
*																			*
****************************************************************************/

/* Exit with extended error information after a readTextLine() call */

STDC_NONNULL_ARG( ( 1, 4 ) ) \
int retTextLineError( INOUT STREAM *stream, 
					  IN_ERROR const int status, 
					  const BOOLEAN isTextLineError, 
					  FORMAT_STRING const char *format, 
					  const int value )
	{
#ifdef USE_ERRMSGS
	NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
#endif /* USE_ERRMSGS */

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( cryptStatusError( status ) );
	assert( isReadPtr( format, 4 ) );

	REQUIRES( isTextLineError == TRUE || isTextLineError == FALSE );
#ifdef USE_ERRMSGS
	REQUIRES( netStream != NULL && sanityCheckNetStream( netStream ) );
#endif /* USE_ERRMSGS */

	/* If the extended error information came up from a lower level than 
	   readCharFunction(), pass it on up to the caller */
	if( !isTextLineError )
		return( status );

	/* Extend the readTextLine()-level error information with higher-level
	   detail.  This allows us to provide a more useful error report 
	   ("Problem with line x") than just the rather low-level view provided 
	   by readTextLine() ("Invalid character 0x8F at position 12").  The
	   argument handling is:

		printf( format, value ) || printf( stream->errorInfo->errorString );

	   so that 'format' has the form 'High-level error %d: ", to which the
	   low-level string is then appended */
	retExtErr( status, 
			   ( status, NETSTREAM_ERRINFO, NETSTREAM_ERRINFO, 
			     format, value ) );
	}

/* Send an HTTP error message.  This function is somewhat unusually placed
   with the general HTTP parsing functions because it's used by both read 
   and write code but needs access to the HTTP status decoding table, which 
   is part of the parsing code */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendHTTPError( INOUT STREAM *stream, 
   				   OUT_BUFFER_FIXED( headerBufMaxLen ) char *headerBuffer, 
				   IN_LENGTH_SHORT_MIN( MIN_LINEBUF_SIZE ) \
						const int headerBufMaxLen, 
				   IN_INT const int httpStatus )
	{
	const NET_STREAM_INFO *netStream = DATAPTR_GET( stream->netStream );
	const HTTP_STATUS_INFO *httpStatusInfoPtr;
	STREAM headerStream;
	int length DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( headerBuffer, headerBufMaxLen ) );

	REQUIRES( headerBufMaxLen >= MIN_LINEBUF_SIZE && \
			  headerBufMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( httpStatus >= 0 && httpStatus < 600 );
	REQUIRES( netStream != NULL && sanityCheckNetStream( netStream ) );

	/* Find the HTTP error information that corresponds to the HTTP status
	   value */
	httpStatusInfoPtr = getHTTPStatusInfo( httpStatus );
	REQUIRES( httpStatusInfoPtr != NULL );

	/* Send the error message to the peer */
	sMemOpen( &headerStream, headerBuffer, headerBufMaxLen );
	swrite( &headerStream, isHTTP10( netStream ) ? \
			"HTTP/1.0 " : "HTTP/1.1 ", 9 );
	swrite( &headerStream, httpStatusInfoPtr->httpStatusString, 
			HTTP_STATUSSTRING_LENGTH );
	sputc( &headerStream, ' ' );
	swrite( &headerStream, httpStatusInfoPtr->httpErrorString, 
			httpStatusInfoPtr->httpErrorStringLength );
	swrite( &headerStream, "\r\n", 2 );
	if( httpStatus == 501 )
		{
		/* Since the assumption on the web is that anything listening for
		   HTTP requests is a conventional web server, we provide a bit more
		   information to (probable) browsers that connect and send a GET
		   request.  This is also useful for some browsers that hang around
		   forever waiting for content if they don't see anything following
		   the HTTP error status */
		swrite( &headerStream, "Content-Length: 139\r\n\r\n", 23 );
		swrite( &headerStream,
				"<html><head><title>Invalid PKI Server Request</title></head>"
				"<body>This is a PKI messaging service, not a standard web "
				"server.</body></html>", 139 );
		}
	status = swrite( &headerStream, "\r\n", 2 );
	if( cryptStatusOK( status ) )
		length = stell( &headerStream );
	sMemDisconnect( &headerStream );
	ENSURES( cryptStatusOK( status ) );
	return( sendHTTPData( stream, headerBuffer, length,
						  TRANSPORT_FLAG_FLUSH ) );
	}
#endif /* USE_HTTP */
