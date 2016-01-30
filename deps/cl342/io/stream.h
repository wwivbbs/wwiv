/****************************************************************************
*																			*
*					STREAM Class Constants and Structures					*
*					  Copyright Peter Gutmann 1993-2007						*
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
#elif defined( __MAC__ )
  #include <Files.h>
#elif defined( __Nucleus__ )
  #include <nucleus.h>
  #include <pcdisk.h>
#elif defined( __PALMOS__ )
  #include <VFSMgr.h>
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

/* Access/option flags for the file stream open call.  The exclusive access
   flag locks the file so that other threads/processes can't open it until
   the current thread/process closes it.  This flag is implicitly set if the
   file R/W bits are FILE_WRITE, which creates a new file.  The difference
   between the private and sensitive flags is that some data may be private
   for a given user but not sensitive (e.g.config info) while other data may
   be private and sensitive (e.g.private keys).  The sensitive flag only has
   an effect on special systems where data can be committed to secure
   storage, since there's usually a very limited amount of this available we
   only use it for sensitive data but not generic private data */

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

/* Stream IOCTL types */

typedef enum {
	STREAM_IOCTL_NONE,				/* No IOCTL type */
	STREAM_IOCTL_IOBUFFER,			/* Working buffer for file streams */
	STREAM_IOCTL_PARTIALREAD,		/* Allow read of less than req.amount */
	STREAM_IOCTL_PARTIALWRITE,		/* Allow write of less then req.amount */
	STREAM_IOCTL_READTIMEOUT,		/* Network read timeout */
	STREAM_IOCTL_WRITETIMEOUT,		/* Network write timeout */
	STREAM_IOCTL_HANDSHAKECOMPLETE,	/* Toggle handshake vs.data timeout */
	STREAM_IOCTL_CONNSTATE,			/* Connection state (open/closed) */
	STREAM_IOCTL_GETCLIENTNAME,		/* Get client name */
	STREAM_IOCTL_GETCLIENTNAMELEN,	/* Get client name length */
	STREAM_IOCTL_GETCLIENTPORT,		/* Get client port */
	STREAM_IOCTL_GETPEERTYPE,		/* Get peer system type */
	STREAM_IOCTL_HTTPREQTYPES,		/* Permitted HTTP request types */
	STREAM_IOCTL_LASTMESSAGE,		/* CMP last message in transaction */
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
	STREAM_HTTPREQTYPE_LAST			/* Last possible HTTP request type */
	} STREAM_HTTPREQTYPE_TYPE;

/* Options for STREAM_IOCTL_GETPEERTYPE */

typedef enum {
	STREAM_PEER_NONE,					/* No information available */
	STREAM_PEER_MICROSOFT,				/* Microsoft IIS */
	STREAM_PEER_LAST
	} STREAM_PEER_TYPE;

/* Stream network protocol types */

typedef enum {
	STREAM_PROTOCOL_NONE,			/* No protocol type */
	STREAM_PROTOCOL_TCPIP,			/* TCP/IP */
	STREAM_PROTOCOL_HTTP,			/* HTTP */
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

/****************************************************************************
*																			*
*								Stream Structures							*
*																			*
****************************************************************************/

/* The STREAM data type */

typedef struct ST {
	/* General information for the stream */
	int type;					/* The stream type, of type STREAM_TYPE */
	int flags;					/* Stream flags */
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
#elif defined( __AMX__ ) || defined( __BEOS__ ) || defined( __ECOS__ ) || \
	  defined( __iOS__ ) || \
	  ( defined( __MVS__ ) && !defined( CONFIG_NO_STDIO ) ) || \
	  defined( __RTEMS__ ) || defined( __SYMBIAN32__ ) || \
	  defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ ) || \
	  defined( __UNIX__ ) || defined( __VXWORKS__ ) || defined( __XMK__ )
	int fd;						/* Backing file for the stream */
  #ifdef __TESTIO__
	char name[ MAX_PATH_LENGTH + 8 ];/* Data item associated with stream */
  #endif /* __TESTIO__ */
#elif defined( __MAC__ )
	short refNum;				/* File stream reference number */
	FSSpec fsspec;				/* File system specification */
#elif defined( __PALMOS__ )
	FileRef fileRef;			/* File reference number */
#elif defined( __FileX__ )
	FX_FILE filePtr;			/* File associated with this stream */
	long position;				/* Position in file */
#elif defined( __UCOSII__ )
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
	void *netStreamInfo;
#endif /* USE_TCP */
	} STREAM;

/* Parsed URL information: schema://user@host:port/location.  This is used 
   to parse URL data from an in-memory string and encodes pointers to 
   locations in the string data */

typedef enum { URL_TYPE_NONE, URL_TYPE_HTTP, URL_TYPE_HTTPS, URL_TYPE_SSH,
			   URL_TYPE_CMP, URL_TYPE_TSP, URL_TYPE_LDAP, 
			   URL_TYPE_LAST } URL_TYPE;

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

/* Parsed HTTP URI information: location?attribute=value.  The contents of a
   string-form URI are broken down into the following fields by the HTTP
   read code */

typedef struct {
	BUFFER( CRYPT_MAX_TEXTSIZE, locationLen ) \
	char location[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, attributeLen ) \
	char attribute[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, valueLen ) \
	char value[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, extraDataLen ) \
	char extraData[ CRYPT_MAX_TEXTSIZE + 8 ];
	int locationLen, attributeLen, valueLen, extraDataLen;
	} HTTP_URI_INFO;

/* Information required when connecting a network stream.  There are so many
   parameters required that we pack them into a struct to keep the interface
   more manageable */

typedef enum {
	NET_OPTION_NONE,			/* No connect option type */
	NET_OPTION_HOSTNAME,		/* Use host/interface name + port */
	NET_OPTION_TRANSPORTSESSION,/* Use network transport session */
	NET_OPTION_NETWORKSOCKET,	/* Use user-supplied network socket */
	NET_OPTION_NETWORKSOCKET_DUMMY,	/* Dummy open to check socket OK */
	NET_OPTION_LAST				/* Last possible connect option type */
	} NET_OPTION_TYPE;

typedef struct {
	/* Network link information, either a remote host and port, a pre-
	   connected network socket, or a cryptlib transport session */
	BUFFER_OPT_FIXED( nameLength ) \
	const char *name;
	int nameLength;
	int port;					/* Remote host info */
	const char *interface;
	int interfaceLength;		/* Local interface info */
	int networkSocket;			/* Pre-connected network socket */
	CRYPT_SESSION iCryptSession;/* cryptlib transport session */

	/* Auxiliary information: Owning user object, network status 
	   information, general option type */
	CRYPT_USER iUserObject;		/* Owning user object */
	int timeout, connectTimeout;/* Connect and data xfer.timeouts */
	NET_OPTION_TYPE options;	/* Connect options */
	} NET_CONNECT_INFO;

#define initNetConnectInfo( netConnectInfo, netUserObject, netTimeout, \
							netConnectTimeout, netOption ) \
	{ \
	memset( netConnectInfo, 0, sizeof( NET_CONNECT_INFO ) ); \
	( netConnectInfo )->networkSocket = CRYPT_ERROR; \
	( netConnectInfo )->iCryptSession = CRYPT_ERROR; \
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
	int bytesAvail, bytesTransferred;	/* Actual data bytes on read */
	BUFFER_FIXED( contentTypeLen ) \
	const char *contentType;		/* HTTP content type */
	int contentTypeLen;	

	/* HTTP read/write control flags.  If the bufferResize flag is set then 
	   the HTTP read code can dynamically resize the buffer in order to read 
	   arbitrary-length input.  If the buffer was resized during the read 
	   then the flag is returned set and the caller has to reset their read 
	   buffer to { buffer, bufSize }.  If no resize took place then the flag 
	   is returned cleared */
	BOOLEAN bufferResize;			/* Buffer is resizeable */

	/* The client's request type and request info (for HTTP GET), and the 
	   server's status in response to a client GET request */
	STREAM_HTTPREQTYPE_TYPE reqType;/* HTTP request type */
	HTTP_URI_INFO *reqInfo;
	int reqStatus;				/* HTTP status in response to request */
	} HTTP_DATA_INFO;

#define initHttpDataInfo( httpDataInfo, dataBuffer, dataLength ) \
	{ \
	memset( httpDataInfo, 0, sizeof( HTTP_DATA_INFO ) ); \
	( httpDataInfo )->buffer= dataBuffer; \
	( httpDataInfo )->bufSize = dataLength; \
	}
#define initHttpDataInfoEx( httpDataInfo, dataBuffer, dataLength, uriInfo ) \
	{ \
	memset( httpDataInfo, 0, sizeof( HTTP_DATA_INFO ) ); \
	memset( uriInfo, 0, sizeof( HTTP_URI_INFO ) ); \
	( httpDataInfo )->buffer= dataBuffer; \
	( httpDataInfo )->bufSize = dataLength; \
	( httpDataInfo )->reqInfo = uriInfo; \
	}

/****************************************************************************
*																			*
*							Stream Function Prototypes						*
*																			*
****************************************************************************/

/* Functions corresponding to traditional/stdio-type I/O */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sputc( INOUT STREAM *stream, IN_BYTE const int ch );
CHECK_RETVAL_RANGE( MAX_ERROR, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
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
CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1 ) ) \
int stell( const STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sioctlSet( INOUT STREAM *stream, 
			   IN_ENUM( STREAM_IOCTL ) const STREAM_IOCTL_TYPE type, 
			   IN_INT const int value );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sioctlSetString( INOUT STREAM *stream, 
					 IN_ENUM( STREAM_IOCTL ) const STREAM_IOCTL_TYPE type, 
					 IN_BUFFER( dataLen ) const void *data, 
					 IN_LENGTH const int dataLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sioctlGet( INOUT STREAM *stream, 
			   IN_ENUM( STREAM_IOCTL ) const STREAM_IOCTL_TYPE type, 
			   OUT_BUFFER_FIXED( dataMaxLen ) void *data, 
			   IN_LENGTH_SHORT const int dataMaxLen );

/* Nonstandard functions: Skip a number of bytes in a stream, peek at the
   next value in the stream */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sSkip( INOUT STREAM *stream, IN_LENGTH const long offset );
CHECK_RETVAL_RANGE( MAX_ERROR, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
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
   function sMemOpenEx() to indicate that it's OK for the buffer value to be 
   NULL */ 

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sMemOpen( OUT STREAM *stream, 
			  OUT_BUFFER_FIXED( length ) void *buffer, 
			  IN_LENGTH const int length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sMemOpenOpt( OUT STREAM *stream, 
				 OUT_BUFFER_OPT_FIXED( length ) void *buffer, 
				 IN_LENGTH_Z const int length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sMemNullOpen( OUT STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sMemClose( INOUT STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sMemConnect( OUT STREAM *stream, 
				 IN_BUFFER( length ) const void *buffer, 
				 IN_LENGTH const int length );
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

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1 ) ) \
int sMemDataLeft( const STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sMemGetDataBlock( INOUT STREAM *stream, 
					  OUT_BUFFER_ALLOC_OPT( dataSize ) void **dataPtrPtr, 
					  IN_LENGTH const int dataSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int sMemGetDataBlockAbs( INOUT STREAM *stream, 
						 IN_LENGTH_Z const int position, 
						 OUT_BUFFER_ALLOC_OPT( dataSize ) void **dataPtrPtr, 
						 IN_LENGTH const int dataSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int sMemGetDataBlockRemaining( INOUT STREAM *stream, 
							   OUT_BUFFER_ALLOC_OPT( *length ) void **dataPtrPtr, 
							   OUT_LENGTH_Z int *length );

/* Functions to work with file streams */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sFileOpen( OUT STREAM *stream, IN_STRING const char *fileName, 
			   IN_FLAGS( FILE ) const int mode );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sFileClose( INOUT STREAM *stream );

/* Convert a file stream to a memory stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int sFileToMemStream( OUT STREAM *memStream, INOUT STREAM *fileStream,
					  OUT_BUFFER_ALLOC_OPT( length ) void **bufPtrPtr, 
					  IN_LENGTH const int length );

/* Functions to work with network streams */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sNetParseURL( OUT URL_INFO *urlInfo, 
				  IN_BUFFER( urlLen ) const char *url, 
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

/* Special-case file I/O calls */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN fileReadonly( IN_STRING const char *fileName );
STDC_NONNULL_ARG( ( 1 ) ) \
void fileClearToEOF( const STREAM *stream );
STDC_NONNULL_ARG( ( 1 ) ) \
void fileErase( IN_STRING const char *fileName );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int fileBuildCryptlibPath( OUT_BUFFER( pathMaxLen, *pathLen ) char *path, 
						   IN_LENGTH_SHORT const int pathMaxLen, 
						   OUT_LENGTH_SHORT_Z int *pathLen,
						   IN_BUFFER( fileNameLen ) const char *fileName, 
						   IN_LENGTH_SHORT const int fileNameLen,
						   IN_ENUM( BUILDPATH_OPTION ) \
						   const BUILDPATH_OPTION_TYPE option );

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
