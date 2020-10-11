/****************************************************************************
*																			*
*					STREAM Class Constants and Structures					*
*					  Copyright Peter Gutmann 1993-2015						*
*																			*
****************************************************************************/

#ifndef _STREAM_DEFINED

#define _STREAM_DEFINED

#if defined( INC_ALL )
  #include "crypt.h"
#else
  #include "crypt.h"
#endif /* Compiler-specific includes */
#if defined( __WIN32__ ) || defined( __WINCE__ )
  /* Includes are always handled via the normal system includes */
#elif defined( __UNIX__ ) || defined( __BEOS__ ) || defined( __XMK__ )
  #include <unistd.h>		/* For lseek() codes */
#elif defined( __embOS__ )
  #include <FS.h>
#elif defined( __MAC__ )
  #include <Files.h>
#elif defined( __MQXRTOS__ )
  /* MQX handles all stdio-related operations via a complex set of macros
     mapping something to something else and then something else again, to
	 deal with this if we're cross-compiling we pull in the internal file.h
	 header which resolves the conflicts for us */
  #ifdef _MSC_VER
	#include "io/file.h"
  #else
	#include <mfs.h>
  #endif /* Conflicting defines */
#elif defined( __Nucleus__ )
  #include <nucleus.h>
  #include <pcdisk.h>
#elif defined( __PALMOS__ )
  #include <VFSMgr.h>
#elif defined( __Quadros__ )
  #include <udefs_s.h>
  #include <api_s.h>
#elif defined( __SMX__ )
  #include <smx.h>
  #include <smxfs.h>
#elif defined( __Telit__ )
  #include <m2m_fs_api.h>
#elif defined( __UCOSII__ )
  #include <fs_api.h>	/* For uC/FS 2.x, renamed to fs.h in 3.x */
#elif !defined( CONFIG_NO_STDIO )
  #include <stdio.h>
#endif /* System-specific file I/O headers */

/****************************************************************************
*																			*
*								Stream Constants							*
*																			*
****************************************************************************/

/* Access/option flags for the file stream open call.  These are:

	FLAG_EXCLUSIVE_ACCESS: Lock the file so that other threads/processes 
		can't open it until the current thread/process closes it.  This flag 
		is implicitly set if the file R/W bits (via FLAG_RW_MASK) are 
		FILE_WRITE, which creates a new file.  

	FLAG_READ/FLAG_WRITE: Open file for read/write access.

	FLAG_PRIVATE/FLAG_SENSITIVE: Specify the sensitivity of data in a file.  
		The difference between the private and sensitive flags is that some 
		data may be private for a given user but not sensitive (e.g.config 
		info) while other data may be private and sensitive (e.g.private 
		keys).  The sensitive flag only has an effect on special systems 
		where data can be committed to secure storage, since there's usually 
		a very limited amount of this available we only use it for sensitive 
		data but not generic private data */

#define FILE_FLAG_NONE		0x00	/* No file flag */
#define FILE_FLAG_READ		0x01	/* Open file for read access */
#define FILE_FLAG_WRITE		0x02	/* Open file for write access */
#define FILE_FLAG_EXCLUSIVE_ACCESS 0x04	/* Don't allow others access */
#define FILE_FLAG_PRIVATE	0x08	/* Set ACL's to allow owner access only */
#define FILE_FLAG_SENSITIVE	0x10	/* Use secure storage if available */
#define FILE_FLAG_RW_MASK	0x03	/* Mask for R/W bits */
#define FILE_FLAG_MAX		0x1F	/* Maximum possible flag value */

/* Options for the build-path call */

typedef enum {
	BUILDPATH_NONE,					/* No option type */
	BUILDPATH_CREATEPATH,			/* Get path to config file, create if nec.*/
	BUILDPATH_GETPATH,				/* Get path to config file */
	BUILDPATH_RNDSEEDFILE,			/* Get path to random seed file */
	BUILDPATH_LAST					/* Last valid option type */
	} BUILDPATH_OPTION_TYPE;

/* Stream IOCTL types.  These are: 

	IOCTL_CLOSESENDCHANNEL: Network streams, perform a sender-side close of 
		the network channel.

	IOCTL_CONNSTATE/IOCTL_LASTMESSAGE: Network streams, manage the last 
		message in a stream.  IOCTL_LASTMESSAGE is write-only and specifies 
		that this is the last message to be sent, handled in HTTP by adding 
		a "Connection: close" to the header.  IOCTL_CONNSTATE is read-only
		and specifies that the peer has indicated that this is the last 
		message to be received (typically by the recipient seeing an HTTP
		"Connection: close"), so that after processing this message the 
		channel is considered closed.

	IOCTL_ERRORINFO: Set extended error information for the stream.

	IOCTL_GETxxx: Network stream, get various network-related parameters.

	IOCTL_HANDSHAKECOMPLETE: Network streams, change the stream timeout 
		value from being applied to the handshake phase to being applied to 
		the data read/write phase.  Typically the handshake is blocking 
		while the data read/write is nonblocking, so different timeouts 
		apply.

	IOCTL_HTTPREQTYPES: Network streams, specify the HTTP request types (as
		a STREAM_HTTPREQTYPE_xxx) that are permitted for this stream.

	IOCTL_IOBUFFER: File streams, set/clear the working buffer for I/O.

	IOCTL_PARTIALREAD/IOCTL_PARTIALWRITE: File streams, allow a read or 
		write of less than the total data amount specified in the length 
		parameter, used when working with virtual file streams that have 
		been translated into memory streams by the stream.c buffering.

	IOCTL_READTIMEOUT/IOCTL_WRITETIMEOUT: Network streams, set the read/
		write timeout */

typedef enum {
	STREAM_IOCTL_NONE,				/* No IOCTL type */
	STREAM_IOCTL_IOBUFFER,			/* Working buffer for file streams */
	STREAM_IOCTL_PARTIALREAD,		/* Allow read of less than req.amount */
	STREAM_IOCTL_PARTIALWRITE,		/* Allow write of less then req.amount */
	STREAM_IOCTL_READTIMEOUT,		/* Network read timeout */
	STREAM_IOCTL_WRITETIMEOUT,		/* Network write timeout */
	STREAM_IOCTL_HANDSHAKECOMPLETE,	/* Toggle handshake vs.data timeout */
	STREAM_IOCTL_CONNSTATE,			/* Connection state (open/closed) */
	STREAM_IOCTL_LASTMESSAGE,		/* Last message in transaction */
	STREAM_IOCTL_GETCLIENTNAME,		/* Get client name */
	STREAM_IOCTL_GETCLIENTNAMELEN,	/* Get client name length */
	STREAM_IOCTL_GETCLIENTPORT,		/* Get client port */
	STREAM_IOCTL_GETPEERTYPE,		/* Get peer system type */
	STREAM_IOCTL_HTTPREQTYPES,		/* Permitted HTTP request types */
	STREAM_IOCTL_CLOSESENDCHANNEL,	/* Close send side of channel */
	STREAM_IOCTL_ERRORINFO,			/* Set stream extended error info */
	STREAM_IOCTL_LAST				/* Last possible IOCTL type */
	} STREAM_IOCTL_TYPE;

/* Options for STREAM_IOCTL_HTTPREQTYPES */

typedef enum {
	STREAM_HTTPREQTYPE_NONE,		/* No HTTP request type */
	STREAM_HTTPREQTYPE_GET,			/* HTTP GET only */
	STREAM_HTTPREQTYPE_POST,		/* HTTP POST only */
	STREAM_HTTPREQTYPE_POST_AS_GET,	/* HTTP GET acting as a POST (b0rken svrs) */
	STREAM_HTTPREQTYPE_ANY,			/* HTTP GET or POST */
	STREAM_HTTPREQTYPE_WS_UPGRADE,	/* WebSockets Upgrade request */
	STREAM_HTTPREQTYPE_LAST			/* Last possible HTTP request type */
	} STREAM_HTTPREQTYPE_TYPE;

/* Options for STREAM_IOCTL_GETPEERTYPE */

typedef enum {
	STREAM_PEER_NONE,					/* No information available */
	STREAM_PEER_MICROSOFT,				/* Windows Server generic */
	STREAM_PEER_MICROSOFT_2008,			/* Windows Server, 2008 bugs */
	STREAM_PEER_MICROSOFT_2012,			/* Windows Server, 2012 bugs */
	STREAM_PEER_LAST
	} STREAM_PEER_TYPE;

/* Stream network protocol types */

typedef enum {
	STREAM_PROTOCOL_NONE,			/* No protocol type */
	STREAM_PROTOCOL_TCP,			/* TCP */
	STREAM_PROTOCOL_UDP,			/* UDP */
#ifdef USE_HTTP
	STREAM_PROTOCOL_HTTP,			/* HTTP */
#endif /* USE_HTTP */
#ifdef USE_EAP
	STREAM_PROTOCOL_EAP,			/* EAP */
#endif /* USE_EAP */
	STREAM_PROTOCOL_LAST			/* Last possible protocol type */
	} STREAM_PROTOCOL_TYPE;

/* The size of the I/O buffer used to read/write data from/to streams backed 
   by persistent files.  These are allocated on-demand on the stack, so they
   shouldn't be made too big.  In addition since they may correspond 
   directly to underlying storage media blocks (e.g. disk sectors or flash 
   memory segments) they shouldn't be made smaller than the underlying 
   blocksize either.  Finally, they should be a power of two (this isn't a 
   strict requirement of the code, but is in a good idea in general because 
   of storage media constraints) */

#ifdef CONFIG_CONSERVE_MEMORY
  #define STREAM_BUFSIZE		512
#else
  #define STREAM_BUFSIZE		4096
#endif /* CONFIG_CONSERVE_MEMORY */

/* When performing file I/O we need to know how large path names can get in
   order to perform range checking and allocate buffers.  This gets a bit
   tricky since not all systems have PATH_MAX, so we first try for PATH_MAX,
   if that fails we try _POSIX_PATH_MAX (which is a generic 255 bytes and if
   defined always seems to be less than whatever the real PATH_MAX should be),
   if that also fails we grab stdio.h and try and get FILENAME_MAX, with an
   extra check for PATH_MAX in case it's defined in stdio.h instead of
   limits.h where it should be.  FILENAME_MAX isn't really correct since it's
   the maximum length of a filename rather than a path, but some environments
   treat it as if it were PATH_MAX and in any case it's the best that we can
   do in the absence of anything better */

#if defined( PATH_MAX )
  #define MAX_PATH_LENGTH		PATH_MAX
#elif defined( _POSIX_PATH_MAX )
  #define MAX_PATH_LENGTH		_POSIX_PATH_MAX
#elif defined( __embOS__ )
  #define MAX_PATH_LENGTH		FS_MAX_PATH
#elif defined( __FileX__ )
  #define MAX_PATH_LENGTH		FX_MAXIMUM_PATH
#else
  #ifndef FILENAME_MAX
	#include <stdio.h>
  #endif /* FILENAME_MAX */
  #if defined( PATH_MAX )
	#define MAX_PATH_LENGTH		PATH_MAX
  #elif defined( MAX_PATH )
	#define MAX_PATH_LENGTH		MAX_PATH
  #elif defined( FILENAME_MAX )
	#define MAX_PATH_LENGTH		FILENAME_MAX
  #elif defined( __MSDOS16__ )
	#define FILENAME_MAX		80
  #else
	#error Need to add a MAX_PATH_LENGTH define in io/file.h
  #endif /* OS-specific path length defines */
#endif /* PATH_MAX */
#if MAX_PATH_LENGTH <= 32
  #error MAX_PATH_LENGTH is <= 32 characters, check your build environment
#endif /* Too-short MAX_PATH values */

/****************************************************************************
*																			*
*								Stream Structures							*
*																			*
****************************************************************************/

/* The STREAM data type */

typedef struct ST {
	/* General information for the stream */
	int type;					/* The stream type, of type STREAM_TYPE */
	SAFE_FLAGS flags;			/* Stream flags */
	int status;					/* Current stream status (clib error code) */

	/* Information for memory I/O */
	BUFFER_OPT( bufSize, bufEnd ) \
	BYTE *buffer;				/* Buffer to R/W to */
	int bufSize;				/* Total size of buffer */
	int bufPos;					/* Current position in buffer */
	int bufEnd;					/* Last buffer position with valid data */

	/* Information for file I/O */
	int bufCount;				/* File position quantised by buffer size */
#if defined( __WIN32__ ) || defined( __WINCE__ )
	HANDLE hFile;				/* Backing file for the stream */
  #ifdef __TESTIO__
	char name[ MAX_PATH_LENGTH + 8 ];/* Data item associated with stream */
  #endif /* __TESTIO__ */
#elif defined( __AMX__ ) || defined( __Android__ ) || \
	  defined( __BEOS__ ) || defined( __ECOS__ ) || defined( __iOS__ ) || \
	  defined( __MGOS__ ) || \
	  ( defined( __MVS__ ) && !defined( CONFIG_NO_STDIO ) ) || \
	  defined( __RTEMS__ ) || defined( __SYMBIAN32__ ) || \
	  defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ ) || \
	  defined( __UNIX__ ) || defined( __VxWorks__ ) || defined( __XMK__ )
	int fd;						/* Backing file for the stream */
  #ifdef __TESTIO__
	char name[ MAX_PATH_LENGTH + 8 ];/* Data item associated with stream */
  #endif /* __TESTIO__ */
#elif defined( __FileX__ )
	FX_FILE filePtr;			/* File associated with this stream */
	long position;				/* Position in file */
#elif defined( __MAC__ )
	short refNum;				/* File stream reference number */
	FSSpec fsspec;				/* File system specification */
#elif defined( __MQXRTOS__ )
	MQX_FILE *filePtr;			/* File associated with this stream */
#elif defined( __PALMOS__ )
	FileRef fileRef;			/* File reference number */
#elif defined( __Quadros__ )
	FS_FILE *filePtr;			/* File associated with this stream */
#elif defined( __SMX__ )
	FILEHANDLE filePtr;			/* File associated with this stream */
#elif defined( __Telit__ )
	M2M_T_FS_HANDLE filePtr;	/* File associated with this stream */
#elif defined( __UCOSII__ ) || defined( __embOS__ )
	FS_FILE *pFile;				/* File associated with this stream */
#elif defined( CONFIG_NO_STDIO )
  #if defined( __IBM4758__ )
	char name[ 8 + 1 ];			/* Data item associated with stream */
	BOOLEAN isSensitive;		/* Whether stream contains sensitive data */
  #elif defined( __MVS__ ) || defined( __VMCMS__ ) || defined( __TESTIO__ )
	char name[ MAX_PATH_LENGTH + 8 ];/* Data item associated with stream */
  #endif /* Nonstandard I/O enviroments */
#elif defined( __Nucleus__ )
	INT fd;						/* File handle */
#else
	FILE *filePtr;				/* The file associated with this stream */
#endif /* System-specific file I/O information */

	/* Network stream information.  This is dynamically allocated since it's 
	   only used for (relatively rare) network streams and would lead to a 
	   lot of wasted memory in the memory streams that are used constantly 
	   throughout cryptlib */
#ifdef USE_TCP
	DATAPTR netStream;
#endif /* USE_TCP */
	} STREAM;

/* Parsed URL information: schema://user@host:port/location.  This is used 
   to parse URL data from an in-memory string and encodes pointers to 
   locations in the string data */

typedef enum { URL_TYPE_NONE, URL_TYPE_HTTP, URL_TYPE_HTTPS, 
			   URL_TYPE_WEBSOCKET, URL_TYPE_SSH, URL_TYPE_CMP, URL_TYPE_TSP, 
			   URL_TYPE_LDAP, URL_TYPE_LAST } URL_TYPE;

typedef struct {
	URL_TYPE type;
	BUFFER_OPT_FIXED( schemaLen ) \
	const char *schema;
	int schemaLen;
	BUFFER_OPT_FIXED( userInfoLen ) \
	const char *userInfo;
	int userInfoLen;
	BUFFER_OPT_FIXED( hostLen ) \
	const char *host;
	int hostLen;
	BUFFER_OPT_FIXED( locationLen ) \
	const char *location;
	int locationLen;
	int port;
	} URL_INFO;

/* HTTP request information as sent in a GET/POST */

typedef struct {
	/* URI information, attribute=value&extraData */
	const char *attribute;
	int attributeLen;
	const char *value;
	int valueLen;
	const char *extraData;
	int extraDataLen;

	/* Additional information added to request headers */
	const char *protocol;		/* Subprotocol type */
	int protocolLen;
	const char *auth;			/* Access key/response */
	int authLen;
	} HTTP_REQ_INFO;

/* HTTP URI information, location?attribute=value&extraData, parsed from a
   string-form URI by the HTTP read code */

typedef struct {
	/* HTTP URI information */
	BUFFER( CRYPT_MAX_TEXTSIZE, locationLen ) \
	char location[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, attributeLen ) \
	char attribute[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, valueLen ) \
	char value[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, extraDataLen ) \
	char extraData[ CRYPT_MAX_TEXTSIZE + 8 ];
	int locationLen, attributeLen, valueLen, extraDataLen;

	/* Additional information that can be sent in request headers */
	BUFFER( CRYPT_MAX_TEXTSIZE, protocolLen ) \
	char protocol[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, authLen ) \
	char auth[ CRYPT_MAX_TEXTSIZE + 8 ];
	int protocolLen, authLen;
	} HTTP_URI_INFO;

/* Information required when connecting a network stream.  There are so many
   parameters required that we pack them into a struct to keep the interface
   more manageable */

typedef enum {
	NET_OPTION_NONE,			/* No connect option type */
	NET_OPTION_HOSTNAME,		/* Use host/interface name + port */
	NET_OPTION_NETWORKSOCKET,	/* Use user-supplied network socket */
	NET_OPTION_NETWORKSOCKET_DUMMY,	/* Dummy open to check socket OK */
	NET_OPTION_VIRTUAL,			/* Virtual session layered over actual one */
	NET_OPTION_LAST				/* Last possible connect option type */
	} NET_OPTION_TYPE;

typedef struct {
	/* Network link information, either a remote host and port, a pre-
	   connected network socket, or read/write/get-error callbacks and state 
	   storage for a virtual network layer */
	BUFFER_OPT_FIXED( nameLength ) \
	const char *name;
	int nameLength;
	int port;					/* Remote host info */
	const char *interface;
	int interfaceLength;		/* Local interface info */
	int networkSocket;			/* Pre-connected network socket */
	FNPTR virtualGetDataFunction, virtualPutDataFunction;
	FNPTR virtualGetErrorInfoFunction;
	DATAPTR virtualStateInfo;	/* State info for virtual R/W */

	/* Auxiliary information: Owning user object, network status 
	   information, general option type, and optional authentication 
	   information */
	CRYPT_USER iUserObject;		/* Owning user object */
	int timeout, connectTimeout;/* Connect and data xfer.timeouts */
	NET_OPTION_TYPE options;	/* Connect options */
	BUFFER_OPT_FIXED( authNameLength ) \
	const char *authName;
	int authNameLength;			/* Authentication name */
	BUFFER_OPT_FIXED( authKeyLength ) \
	const char *authKey;
	int authKeyLength;			/* Authentication key */
	} NET_CONNECT_INFO;

#define initNetConnectInfo( netConnectInfo, netUserObject, netTimeout, \
							netConnectTimeout, netOption ) \
	{ \
	memset( netConnectInfo, 0, sizeof( NET_CONNECT_INFO ) ); \
	( netConnectInfo )->networkSocket = CRYPT_ERROR; \
	FNPTR_SET( ( netConnectInfo )->virtualGetDataFunction, NULL ); \
	FNPTR_SET( ( netConnectInfo )->virtualPutDataFunction, NULL ); \
	FNPTR_SET( ( netConnectInfo )->virtualGetErrorInfoFunction, NULL ); \
	DATAPTR_SET( ( netConnectInfo )->virtualStateInfo, NULL ); \
	( netConnectInfo )->iUserObject = netUserObject; \
	( netConnectInfo )->timeout = netTimeout; \
	( netConnectInfo )->connectTimeout = netConnectTimeout; \
	( netConnectInfo )->options = netOption; \
	}

/* Information required when reading from/writing to an HTTP stream.  
   Although we're in theory just using HTTP as a universal substrate,
   there's a pile of additional HTTP-related data that we have to convey,
   so when we perform a read or write to an HTTP stream we use a composite
   data parameter */

typedef struct {
	/* Data payload informtion.  On read the { buffer, bufSize } is the 
	   amount of buffer space available to read data, with bytesAvail being
	   the length of the data item being read into the buffer and 
	   bytesTransferred being the amount of data actually transferred.  On 
	   write the { buffer, bufSize } is the data to write and 
	   bytesTransferred is the amount actually transferred.  We have to
	   store this information here because the write call is passed the
	   HTTP_DATA_INFO structure rather than the data buffer so we can't 
	   return a bytes-read or written count as the return value */
	BUFFER_UNSPECIFIED( bufSize ) \
	void *buffer;					/* Data buffer */
	int bufSize;					/* Size of data buffer */
	int bytesToWrite;				/* Bytes to write on write */
	int bytesAvail, bytesTransferred;	/* Actual data bytes on read */
	BUFFER_FIXED( contentTypeLen ) \
	const char *contentType;		/* HTTP content type */
	int contentTypeLen;	

	/* HTTP read/write control flags.  If the bufferResize flag is set then 
	   the HTTP read code can dynamically resize the buffer in order to read 
	   arbitrary-length input.  If the buffer was resized during the read 
	   then the flag is returned set and the caller has to reset their read 
	   buffer to { buffer, bufSize }.  If no resize took place then the flag 
	   is returned cleared.
	   
	   If the responseIsText flag is set then a text/plain response from the 
	   server is valid.  Normally messages have application-specific message 
	   types and the only time we'd see plain text is if the server is 
	   returning an error message (usually outside the spec of the protocol 
	   that we're talking), however if additional capabilities have been 
	   kludged onto the protocol via text-format messages then a response-
	   type of text/pain is valid */
	BOOLEAN bufferResize;			/* Buffer is resizeable */
	BOOLEAN responseIsText;			/* Response from svr.is text/plain */

	/* The client's request type and request info (for HTTP GET/POST) or
	   the server's parsed URI info from the client, and the server's 
	   status in response to a client GET/POST request */
	STREAM_HTTPREQTYPE_TYPE reqType;/* HTTP request type */
	const HTTP_REQ_INFO *reqInfo;
	HTTP_URI_INFO *uriInfo;
	int reqStatus;				/* HTTP status in response to request */
	} HTTP_DATA_INFO;

#define initHttpInfoRead( httpDataInfo, buffer, bufSize ) \
		initHttpInfo( httpDataInfo, buffer, bufSize, 0, NULL, NULL )
#define initHttpInfoReadEx( httpDataInfo, buffer, bufSize, uriInfo ) \
		initHttpInfo( httpDataInfo, buffer, bufSize, 0, NULL, uriInfo )
#define initHttpInfoWrite( httpDataInfo, buffer, dataLength, bufSize ) \
		initHttpInfo( httpDataInfo, buffer, bufSize, dataLength, NULL, NULL )
#define initHttpInfoWriteEx( httpDataInfo, buffer, dataLength, bufSize, reqInfo ) \
		initHttpInfo( httpDataInfo, buffer, bufSize, dataLength, reqInfo, NULL )
#define initHttpInfoReq( httpDataInfo ) \
		initHttpInfo( httpDataInfo, NULL, 0, 0, NULL, NULL )
#define initHttpInfoReqEx( httpDataInfo, reqInfo ) \
		initHttpInfo( httpDataInfo, NULL, 0, 0, reqInfo, NULL )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initHttpInfo( INOUT HTTP_DATA_INFO *httpDataInfo, 
				  OUT_BUFFER_OPT_FIXED( bufSize ) void *buffer, 
				  IN_LENGTH_Z const int bufSize,
				  IN_LENGTH_Z const int dataLength,
				  IN_OPT const HTTP_REQ_INFO *reqInfo,
				  OUT_OPT HTTP_URI_INFO *uriInfo );

/****************************************************************************
*																			*
*							Stream Function Prototypes						*
*																			*
****************************************************************************/

/* Functions corresponding to traditional/stdio-type I/O.

   The annotation for sread() and swrite() are a bit dishonest in that in 
   almost all cases what's returned is a pure status code, it's only for 
   network streams and special-case streams that allow partial reads that a 
   length can be returned.  If we were to use CHECK_RETVAL_LENGTH then the 
   belief that a returned status value isn't really a true status gets 
   propagated up through every function that reads or writes data, leading
   to endless false-positive warnings */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sputc( INOUT STREAM *stream, IN_BYTE const int ch );
CHECK_RETVAL_RANGE( 0, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int sgetc( INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sread( INOUT STREAM *stream, 
		   OUT_BUFFER_FIXED( length ) void *buffer, 
		   IN_LENGTH const int length );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int swrite( INOUT STREAM *stream, 
		    IN_BUFFER( length ) const void *buffer, 
			IN_LENGTH const int length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sflush( INOUT STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sseek( INOUT STREAM *stream, IN_LENGTH_Z const long position );
CHECK_RETVAL_RANGE_NOERROR( 0, MAX_BUFFER_SIZE ) STDC_NONNULL_ARG( ( 1 ) ) \
int stell( const STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sioctlSet( INOUT STREAM *stream, 
			   IN_ENUM( STREAM_IOCTL ) const STREAM_IOCTL_TYPE type, 
			   const int value );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sioctlSetString( INOUT STREAM *stream, 
					 IN_ENUM( STREAM_IOCTL ) const STREAM_IOCTL_TYPE type, 
					 IN_BUFFER( dataLen ) const void *data, 
					 IN_DATALENGTH const int dataLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sioctlGet( INOUT STREAM *stream, 
			   IN_ENUM( STREAM_IOCTL ) const STREAM_IOCTL_TYPE type, 
			   OUT_BUFFER_FIXED( dataMaxLen ) void *data, 
			   IN_LENGTH_SHORT const int dataMaxLen );

/* Nonstandard functions: Skip a number of bytes in a stream, peek at the
   next value in the stream.  The sSkip() call applies a bounds check, the
   define SSKIP_MAX can be used to denote the maximum length allowed */

#define SSKIP_MAX	( MAX_BUFFER_SIZE - 1 )

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sSkip( INOUT STREAM *stream, const long offset, 
		   IN_DATALENGTH const long maxOffset );
CHECK_RETVAL_RANGE( 0, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int sPeek( INOUT STREAM *stream );

/* Inquire as to the health of a stream.  Currently these are only used in 
   debugging assertions, in int_api.c when exporting attributes to a stream,
   and as a safety check in ssl_rw.c/ssh2_rw.c when wrapping a packet that 
   needs direct access to a memory stream */

#define sGetStatus( stream )		( stream )->status
#define sStatusOK( stream )			cryptStatusOK( ( stream )->status )

/* Set/clear a user-defined error state for the stream */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sSetError( INOUT STREAM *stream, IN_ERROR const int status );
STDC_NONNULL_ARG( ( 1 ) ) \
void sClearError( INOUT STREAM *stream );

/* Stream query functions to determine whether a stream is a null stream,
   a memory-mapped file stream, or a virtual file stream.  The null stream 
   check is used to short-circuit unnecessary data transfers in higher-level 
   code where writing to a null stream is used to determine overall data 
   sizes.  The memory-mapped stream check is used when we can eliminate 
   extra buffer allocation if all data is available in memory.  The virtual
   file stream check is used where the low-level access routines have
   converted a file on a CONFIG_NO_STDIO system to a memory stream that acts
   like a file stream */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sIsNullStream( const STREAM *stream );

/* Functions to work with memory streams.  A null stream is a special case of a 
   memory stream that just acts as a data sink.  In some cases we may want to 
   open either a null stream or a standard memory stream depending on whether 
   the caller has specified an output buffer or not, in this case we provide a
   function sMemOpenOpt() to indicate that it's OK for the buffer value to be 
   NULL.
   
   Note that the open/connect functions are declared with a void return 
   type, this is because they're used in hundreds of locations and the only 
   situation in which they can fail is a programming error.  Because of 
   this, problems are caught by throwing exceptions in debug builds rather 
   than having to add error handling for every case where they're used.  In 
   addition the functions always initialise the stream, setting it to an 
   invalid stream if there's an error, so there's no real need to check a
   return value */ 

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sMemOpen( OUT STREAM *stream, 
			   OUT_BUFFER_FIXED( length ) void *buffer, 
			   IN_LENGTH const int length );
STDC_NONNULL_ARG( ( 1 ) ) \
void sMemOpenOpt( OUT STREAM *stream, 
				  OUT_BUFFER_OPT_FIXED( length ) void *buffer, 
				  IN_LENGTH_Z const int length );
STDC_NONNULL_ARG( ( 1 ) ) \
void sMemNullOpen( OUT STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sMemClose( INOUT STREAM *stream );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sMemConnect( OUT STREAM *stream, 
				  IN_BUFFER( length ) const void *buffer, 
				  IN_LENGTH const int length );
#ifndef CONFIG_CONSERVE_MEMORY_EXTRA
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sMemPseudoConnect( OUT STREAM *stream, 
					    IN_BUFFER( length ) const void *buffer,
					    IN_LENGTH const int length );
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sMemDisconnect( INOUT STREAM *stream );

/* Memory stream direct-access functions, used when the contents of a memory
   stream need to be encrypted/decrypted/signed/MACd.  The basic 
   sMemGetDataBlock() returns a data block of a given size from the current
   stream position, sMemGetDataBlockAbs() returns a data block from the 
   given stream position, and sMemGetDataBlockRemaining() returns a data 
   block containing all remaining data available in the stream.  The stream
   parameter is given as an INOUT even though the stream contents aren't
   strictly affected because the functions can set the stream error state 
   in the case of a failure */

CHECK_RETVAL_RANGE_NOERROR( 0, MAX_BUFFER_SIZE ) STDC_NONNULL_ARG( ( 1 ) ) \
int sMemDataLeft( const STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sMemGetDataBlock( INOUT STREAM *stream, 
					  OUT_BUFFER_ALLOC_OPT( dataSize ) void **dataPtrPtr, 
					  IN_DATALENGTH const int dataSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int sMemGetDataBlockAbs( INOUT STREAM *stream, 
						 IN_DATALENGTH_Z const int position, 
						 OUT_BUFFER_ALLOC_OPT( dataSize ) void **dataPtrPtr, 
						 IN_DATALENGTH const int dataSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int sMemGetDataBlockRemaining( INOUT STREAM *stream, 
							   OUT_BUFFER_ALLOC_OPT( *length ) void **dataPtrPtr, 
							   OUT_DATALENGTH_Z int *length );

/* Functions to work with file streams */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sFileOpen( OUT STREAM *stream, IN_STRING const char *fileName, 
			   IN_FLAGS( FILE ) const int mode );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sFileClose( INOUT STREAM *stream );

/* Convert a file stream to a memory stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int sFileToMemStream( OUT STREAM *memStream, 
					  INOUT STREAM *fileStream,
					  OUT_BUFFER_ALLOC_OPT( length ) void **bufPtrPtr, 
					  IN_DATALENGTH const int length );

/* Special-case file I/O calls */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN fileReadonly( IN_STRING const char *fileName );
STDC_NONNULL_ARG( ( 1 ) ) \
void fileClearToEOF( STREAM *stream );
STDC_NONNULL_ARG( ( 1 ) ) \
void fileErase( IN_STRING const char *fileName );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int fileBuildCryptlibPath( OUT_BUFFER( pathMaxLen, *pathLen ) char *path, 
						   IN_LENGTH_SHORT_MIN( 32 ) const int pathMaxLen, 
						   OUT_LENGTH_BOUNDED_Z( pathMaxLen ) int *pathLen,
						   IN_BUFFER( fileNameLen ) const char *fileName, 
						   IN_LENGTH_SHORT const int fileNameLen,
						   IN_ENUM( BUILDPATH ) \
								const BUILDPATH_OPTION_TYPE option );

/* Functions to work with network streams */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sNetParseURL( OUT URL_INFO *urlInfo, 
				  IN_BUFFER( urlLen ) const BYTE *url, 
				  IN_LENGTH_SHORT const int urlLen, 
				  IN_ENUM_OPT( URL_TYPE ) const URL_TYPE urlTypeHint );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int sNetConnect( OUT STREAM *stream, 
				 IN_ENUM( STREAM_PROTOCOL ) const STREAM_PROTOCOL_TYPE protocol,
				 const NET_CONNECT_INFO *connectInfo, 
				 OUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int sNetListen( OUT STREAM *stream, 
				IN_ENUM( STREAM_PROTOCOL ) const STREAM_PROTOCOL_TYPE protocol,
				const NET_CONNECT_INFO *connectInfo, 
				OUT ERROR_INFO *errorInfo );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sNetDisconnect( INOUT STREAM *stream );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void sNetGetErrorInfo( INOUT STREAM *stream, OUT ERROR_INFO *errorInfo );

/* Initialisation/shutdown functions for network stream interfaces */

#ifdef USE_TCP
  RETVAL \
  int netInitTCP( void );
  void netSignalShutdown( void );
  void netEndTCP( void );
#else
  #define netInitTCP()						CRYPT_OK
  #define netSignalShutdown()
  #define netEndTCP()
#endif /* NET_TCP */

#endif /* _STREAM_DEFINED */
