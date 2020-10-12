/****************************************************************************
*																			*
*						cryptlib HTTP Interface Header						*
*						Copyright Peter Gutmann 1998-2017					*
*																			*
****************************************************************************/

#ifndef _HTTP_DEFINED

#define _HTTP_DEFINED

#if defined( INC_ALL )
  #include "stream_int.h"
#else
  #include "io/stream_int.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						HTTP Constants and Data Structures					*
*																			*
****************************************************************************/

/* The size of the HTTP text-line buffer when we're using a dedicated buffer
   to read header lines rather than the main stream buffer, and the minimum 
   size that we'll accept in functions that use the line buffer.  Any data
   that goes past HTTP_LINEBUF_SIZE is dropped */

#define MIN_LINEBUF_SIZE	512
#define HTTP_LINEBUF_SIZE	1024

/* A macro to determine whether we're talking HTTP 1.0 or 1.1 */

#define isHTTP10( netStream )	( TEST_FLAG( ( netStream )->nhFlags, \
											 STREAM_NHFLAG_HTTP10 ) )

/* HTTP state information passed around the various read/write functions */

#define HTTP_FLAG_NONE		0x00	/* No HTTP info */
#define HTTP_FLAG_CHUNKED	0x01	/* Message used chunked encoding */
#define HTTP_FLAG_TRAILER	0x02	/* Chunked encoding has trailer */
#define HTTP_FLAG_NOOP		0x04	/* No-op data (e.g. 100 Continue) */
#define HTTP_FLAG_TEXTMSG	0x08	/* HTTP content is plain text, probably
									   an error message */
#define HTTP_FLAG_GET		0x10	/* Operation is HTTP GET */
#define HTTP_FLAG_UPGRADE	0x20	/* Operation is HTTP Upgrade */
#define HTTP_FLAG_MAX		0x3F	/* Maximum possible flag value */

/* The minimum and maximum HTTP status values */

#define MIN_HTTP_STATUS		0
#define MAX_HTTP_STATUS		600

/* HTTP header parsing information as used by readHeaderLines() */

typedef struct {
	/* Returned status information: The body content-length, the HTTP error
	   status (if there is one), and general flags information.  The flags
	   parameter is used as both an input and an output parameter */
	int contentLength;	/* HTTP body content length */
	int httpStatus;		/* HTTP error status, if an HTTP error occurs */
	SAFE_FLAGS flags;	/* General flags */

	/* Type-specific returned information: The WebSockets subprotocol and
	   key/response data */
	BUFFER( CRYPT_MAX_TEXTSIZE, wsProtocolLen ) \
	BYTE wsProtocol[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, wsAuthLen ) \
	BYTE wsAuth[ CRYPT_MAX_TEXTSIZE + 8 ];
	int wsProtocolLen, wsAuthLen;

	/* Range-checking information: The minimum and maximum allowable
	   content-length value */
	int minContentLength, maxContentLength;
	} HTTP_HEADER_INFO;

#define initHeaderInfo( headerInfo, minLength, maxLength, hdrFlags ) \
		memset( headerInfo, 0, sizeof( HTTP_HEADER_INFO ) ); \
		INIT_FLAGS( ( headerInfo )->flags, ( hdrFlags ) ); \
		( headerInfo )->minContentLength = ( minLength ); \
		( headerInfo )->maxContentLength = ( maxLength );

/* HTTP status information as used by getHTTPStatusInfo() */

typedef struct {
	const int httpStatus;			/* Numeric status value */
	BUFFER_FIXED( HTTP_STATUSSTRING_LENGTH ) \
	const char *httpStatusString;	/* String status value */
	BUFFER_FIXED( httpErrorStringLength ) \
	const char *httpErrorString;	/* Text description of status */
	const int httpErrorStringLength;
	const int status;				/* Equivalent cryptlib status */
	} HTTP_STATUS_INFO;

/****************************************************************************
*																			*
*							HTTP Function Prototypes						*
*																			*
****************************************************************************/

/* Prototypes for functions in http.c */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckHttpDataInfo( const HTTP_DATA_INFO *httpDataInfo );
CHECK_RETVAL_PTR \
const HTTP_STATUS_INFO *getHTTPStatusInfo( IN_INT const int httpStatus );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendHTTPError( INOUT STREAM *stream, 
				   OUT_BUFFER_FIXED( headerBufMaxLen ) char *headerBuffer, 
				   IN_LENGTH_SHORT_MIN( 256 ) const int headerBufMaxLen, 
				   IN_INT const int httpStatus );
CHECK_RETVAL_RANGE( 0, 8 ) STDC_NONNULL_ARG( ( 1, 3 ) ) \
int checkHTTPID( IN_BUFFER( dataLength ) const char *data, 
				 IN_LENGTH_SHORT const int dataLength, 
				 INOUT STREAM *stream );

/* Prototypes for functions in http_wr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeRequestHeader( INOUT STREAM *stream, 
						IN_OPT const HTTP_REQ_INFO *httpReqInfo,
						IN_BUFFER_OPT( contentTypeLen ) const char *contentType, 
						IN_LENGTH_SHORT_Z const int contentTypeLen, 
						IN_LENGTH_Z const int contentLength,
						const BOOLEAN forceGet );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendHTTPData( INOUT STREAM *stream, 
				  IN_BUFFER( length ) void *buffer, 
				  IN_LENGTH const int length, 
				  IN_FLAGS_Z( HTTP ) const int flags );
STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerHTTPwrite( INOUT NET_STREAM_INFO *netStream );

/* Prototypes for functions in http_parse.c.  Most of these functions don't 
   actually return anything in the buffer that's passed in but merely use it 
   as general scratch buffer to save having to give each function its own
   (sizeable) scratch buffer */

CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int parseUriInfo( INOUT_BUFFER( dataInLength, *dataOutLength ) char *data, 
				  IN_LENGTH_SHORT const int dataInLength, 
				  OUT_LENGTH_BOUNDED_Z( dataInLength ) int *dataOutLength, 
				  INOUT HTTP_URI_INFO *uriInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int readFirstHeaderLine( INOUT STREAM *stream, 
						 OUT_BUFFER_FIXED( dataMaxLength ) char *dataBuffer, 
						 IN_LENGTH_SHORT const int dataMaxLength, 
						 OUT_RANGE( 0, 999 ) int *httpStatus,
						 OUT_BOOL BOOLEAN *isSoftError );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int readHeaderLines( INOUT STREAM *stream, 
					 OUT_BUFFER_FIXED( lineBufMaxLen ) char *lineBuffer, 
					 IN_LENGTH_SHORT_MIN( 256 ) const int lineBufMaxLen,
					 INOUT HTTP_HEADER_INFO *headerInfo,
					 OUT_BOOL BOOLEAN *isSoftError );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readTrailerLines( INOUT STREAM *stream, 
					  OUT_BUFFER_FIXED( lineBufMaxLen ) char *lineBuffer, 
					  IN_LENGTH_SHORT_MIN( 256 ) const int lineBufMaxLen );
STDC_NONNULL_ARG( ( 1, 4 ) ) \
int retTextLineError( INOUT STREAM *stream, IN_ERROR const int status, 
					  const BOOLEAN isTextLineError, 
					  FORMAT_STRING const char *format, 
					  const int value );

#endif /* _HTTP_DEFINED */
