/****************************************************************************
*																			*
*						cryptlib HTTP Interface Header						*
*						Copyright Peter Gutmann 1998-2006					*
*																			*
****************************************************************************/

#ifdef USE_HTTP

#if defined( INC_ALL )
  #include "stream_int.h"
#else
  #include "io/stream_int.h"
#endif /* Compiler-specific includes */

/* The size of the HTTP text-line buffer when we're using a dedicated buffer
   to read header lines rather than the main stream buffer.  Anything more
   than this is dropped */

#define HTTP_LINEBUF_SIZE	1024

/* A macro to determine whether we're talking HTTP 1.0 or 1.1 */

#define isHTTP10( stream )	( ( stream )->flags & STREAM_NFLAG_HTTP10 )

/* HTTP state information passed around the various read/write functions */

#define HTTP_FLAG_NONE		0x00	/* No HTTP info */
#define HTTP_FLAG_CHUNKED	0x01	/* Message used chunked encoding */
#define HTTP_FLAG_TRAILER	0x02	/* Chunked encoding has trailer */
#define HTTP_FLAG_NOOP		0x04	/* No-op data (e.g. 100 Continue) */
#define HTTP_FLAG_TEXTMSG	0x08	/* HTTP content is plain text, probably
									   an error message */
#define HTTP_FLAG_GET		0x10	/* Operation is HTTP GET */
#define HTTP_FLAG_MAX		0x1F	/* Maximum possible flag value */

/* HTTP header parsing information as used by readHeaderLines() */

typedef struct {
	/* Returned status information: The body content-length, the HTTP error
	   status (if there is one), and general flags information.  The flags
	   parameter is used as both an input and an output parameter */
	int contentLength;	/* HTTP body content length */
	int httpStatus;		/* HTTP error status, if an HTTP error occurs */
	int flags;			/* General flags */

	/* Range-checking information: The minimum and maximum allowable
	   content-length value */
	int minContentLength, maxContentLength;
	} HTTP_HEADER_INFO;

#define initHeaderInfo( headerInfo, minLength, maxLength, hdrFlags ) \
		memset( headerInfo, 0, sizeof( HTTP_HEADER_INFO ) ); \
		( headerInfo )->flags = ( hdrFlags ); \
		( headerInfo )->minContentLength = ( minLength ); \
		( headerInfo )->maxContentLength = ( maxLength );

/* Prototypes for functions in http_wr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeRequestHeader( INOUT STREAM *stream, 
						IN_OPT const HTTP_URI_INFO *httpReqInfo,
						IN_BUFFER_OPT( contentTypeLen ) const char *contentType, 
						IN_LENGTH_SHORT_Z const int contentTypeLen, 
						IN_LENGTH_Z const int contentLength,
						const BOOLEAN forceGet );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendHTTPData( INOUT STREAM *stream, 
				  IN_BUFFER( length ) void *buffer, 
				  IN_LENGTH const int length, 
				  IN_FLAGS( HTTP ) const int flags );
STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerHTTPwrite( INOUT NET_STREAM_INFO *netStream );

/* Prototypes for functions in http_parse.c.  Most of these functions don't 
   actually return anything in the buffer that's passed in but merely use it 
   as general scratch buffer to save having to give each function its own
   (sizeable) scratch buffer */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendHTTPError( INOUT STREAM *stream, 
				   OUT_BUFFER_FIXED( headerBufMaxLen ) char *headerBuffer, 
				   IN_LENGTH_SHORT_MIN( 256 ) const int headerBufMaxLen, 
				   IN_INT const int httpStatus );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int checkHTTPID( IN_BUFFER( dataLength ) const char *data, 
				 IN_LENGTH_SHORT const int dataLength, 
				 INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int parseUriInfo( INOUT_BUFFER( dataInLength, *dataOutLength ) char *data, 
				  IN_LENGTH_SHORT const int dataInLength, 
				  OUT_LENGTH_SHORT_Z int *dataOutLength, 
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

#endif /* USE_HTTP */
