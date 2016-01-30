/****************************************************************************
*																			*
*						Internal STREAM Header File							*
*					  Copyright Peter Gutmann 1993-2011						*
*																			*
****************************************************************************/

#ifndef _STREAM_INT_DEFINED

#define _STREAM_INT_DEFINED

#if defined( INC_ALL )
  #include "stream.h"
#else
  #include "io/stream.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Stream Constants							*
*																			*
****************************************************************************/

/* The stream types */

typedef enum {
	STREAM_TYPE_NONE,					/* No stream type */
	STREAM_TYPE_NULL,					/* Null stream (/dev/nul) */
	STREAM_TYPE_MEMORY,					/* Memory stream */
	STREAM_TYPE_FILE,					/* File stream */
	STREAM_TYPE_NETWORK,				/* Network stream */
	STREAM_TYPE_LAST					/* Last possible stream type */
	} STREAM_TYPE;

/* General-purpose stream flags.  These are:

	FLAG_DIRTY: Stream buffer contains data that needs to be committed to
		backing store.

	FLAG_PARTIALREAD: Used for network reads to handle timeouts and for file 
		streams when we don't know the full extent of a file stream.  When 
		this is set and we ask for a read of n bytes and there isn't 
		sufficient data present in the file to satisfy the request the 
		stream code returns 0...n bytes rather than an underflow error.

	FLAG_PARTIALWRITE: Used for network streams when performing bulk data 
		transfers, in this case the write may time out and can be restarted
		later rather than returning a timeout error

	FLAG_READONLY: Stream is read-only */

#define STREAM_FLAG_READONLY	0x0001	/* Read-only stream */
#define STREAM_FLAG_PARTIALREAD 0x0002	/* Allow read of less than req.amount */
#define STREAM_FLAG_PARTIALWRITE 0x0004	/* Allow write of less than req.amount */
#define STREAM_FLAG_DIRTY		0x0008	/* Stream contains un-committed data */
#define STREAM_FLAG_MASK		0x000F	/* Mask for general-purpose flags */

/* Memory stream flags.  These are:

	MFLAG_VFILE: The underlying OS doesn't support conventional file I/O (it
		may only support, for example, access to fixed blocks of flash 
		memory) so this is a memory stream emulating a file stream */

#define STREAM_MFLAG_VFILE		0x0010	/* File stream emulated via mem.stream */
#define STREAM_MFLAG_MASK		( 0x0010 | STREAM_FLAG_MASK )	
										/* Mask for memory-only flags */

/* File stream flags.  These are:

	FFLAG_BUFFERSET: Used to indicate that the stream has an I/O buffer 
		associated with it.  A stream can be opened without a buffer, but to
		read/write data it needs to have a buffer associated with it.  Since
		this can be of variable size and sometimes isn't required at all, 
		it's created on-demand rather than always being present, and its 
		presence is indicated by this flag.
	
	FFLAG_EOF: The underlying file has reached EOF, no further data can be 
		read once the current buffer is emptied.
	
	FFLAG_MMAPPED: This is a memory-mapped file stream, used in conjunction
		with MFLAG_VFILE virtual file streams.

	FFLAG_POSCHANGED: The position in the underlying file has changed, 
		requiring the file buffer to be refilled from the new position 
		before data can be read from it */

#define STREAM_FFLAG_BUFFERSET	0x0080	/* Stream has associated buffer */
#define STREAM_FFLAG_EOF		0x0100	/* EOF reached on stream */
#define STREAM_FFLAG_POSCHANGED	0x0200	/* File stream position has changed */
#define STREAM_FFLAG_POSCHANGED_NOSKIP 0x0400	/* New stream pos.is in following block */
#define STREAM_FFLAG_MMAPPED	0x0800	/* File stream is memory-mapped */
#define STREAM_FFLAG_MASK		( 0x0F80 | STREAM_FLAG_MASK )	
										/* Mask for file-only flags */

/* Network stream flags.  Since there are quite a number of these and they're
   only required for the network-specific stream functionality, we give them
   their own flags variable instead of using the overall stream flags.  These
   are:

	NFLAG_ENCAPS: The protocol is running over a lower encapsulation layer 
		that provides additional packet control information, typically 
		packet size and flow control information.  If this flag is set then
		the lower-level read code overrides some error handling that 
		normally takes place at a higher level.  For example if a read of n 
		bytes is requested and the encapsulation layer reports that only m 
		bytes, m < n is present, this isn't treated as a read/timeout error.

	NFLAG_FIRSTREADOK: The first data read from the stream succeeded.  This
		is used to detect problems due to buggy firewall software, see the
		comments in io/tcp.c for details.

	NFLAG_HTTP10: This is an HTTP 1.0 (rather than 1.1) HTTP stream.

	NFLAG_HTTPPROXY/NFLAG_HTTPTUNNEL: HTTP proxy control flags.  When the 
		proxy flag is set, HTTP requests are sent as 
		"GET http://destination-url/location" rather than "GET location".
		When the tunnel flag is set, HTTP requests are sent as explicit
		proxy commands "CONNECT http://destiation-url/".

		Note that the HTTP tunnel flag is currently never set by anything
		due to the removal of the SESSION_USEHTTPTUNNEL flag at a higher
		level, which was only ever set implicitly by being set in the 
		SSL/TLS altProtocolInfo, which in turn was never selected outside
		a USE_CMP_TRANSPORT block.  The location at which it would be
		selected (except for the presence of a USE_CMP_TRANSPORT ifdef) is 
		at line 195 of session/sess_attr.c in versions up to 3.4.1.

	NFLAG_HTTPGET/NFLAG_HTTPPOST: HTTP allowed-actions flags.

	NFLAG_HTTPPOST_AS_GET: Modify the POST to encode it as a GET (ugh), for 
		b0rken servers that don't do POST.

	NFLAG_ISSERVER: The stream is a server stream (default is client).

	NFLAG_LASTMSG: This is the last message in the exchange, after which any
		high-level shutdown (for example at the HTTP level) can be 
		performed.

	NFLAG_USERSOCKET: The network socket was supplied by the user rather 
		than being created by cryptlib, so some actions such as socket
		shutdown should be skipped */

#define STREAM_NFLAG_ISSERVER	0x0001	/* Stream is server rather than client */
#define STREAM_NFLAG_USERSOCKET	0x0002	/* Network socket was supplied by user */
#define STREAM_NFLAG_HTTP10		0x0004	/* HTTP 1.0 stream */
#define STREAM_NFLAG_HTTPPROXY	0x0008	/* Use HTTP proxy format for requests */
#define STREAM_NFLAG_HTTPTUNNEL	0x0010	/* Use HTTP proxy tunnel for connect */
#define STREAM_NFLAG_HTTPGET	0x0020	/* Allow HTTP GET */
#define STREAM_NFLAG_HTTPPOST	0x0040	/* Allow HTTP POST */
#define STREAM_NFLAG_HTTPPOST_AS_GET 0x0080	/* Implement POST as GET */
#define STREAM_NFLAG_LASTMSG	0x0100	/* Last message in exchange */
#define STREAM_NFLAG_ENCAPS		0x0200	/* Network transport is encapsulated */
#define STREAM_NFLAG_FIRSTREADOK 0x0400	/* First data read succeeded */
#define STREAM_NFLAG_HTTPREQMASK ( STREAM_NFLAG_HTTPGET | STREAM_NFLAG_HTTPPOST | \
								   STREAM_NFLAG_HTTPPOST_AS_GET )
										/* Mask for permitted HTTP req.types */

/* Network transport-specific flags.  These are:

	FLAG_FLUSH: Used in writes to buffered streams to force a flush of data in 
		the stream buffers.
	
	FLAG_BLOCKING/FLAG_NONBLOCKING: Used to override the stream default 
		behaviour on reads and writes and force blocking/nonblocking I/O */

#define TRANSPORT_FLAG_NONE		0x00	/* No transport flag */
#define TRANSPORT_FLAG_FLUSH	0x01	/* Flush data on write */
#define TRANSPORT_FLAG_NONBLOCKING 0x02	/* Explicitly perform nonblocking read */
#define TRANSPORT_FLAG_BLOCKING	0x04	/* Explicitly perform blocking read */
#define TRANSPORT_FLAG_MAX		0x07	/* Maximum possible flag value */

/* The size of the memory buffer used for virtual file streams, which are 
   used in CONFIG_NO_STDIO environments to store data before it's committed
   to backing storage */

#if defined( __MVS__ ) || defined( __VMCMS__ ) || \
	defined( __IBM4758__ ) || defined( __TESTIO__ )
  #define VIRTUAL_FILE_STREAM
#endif /* Nonstandard I/O environments */

#define STREAM_VFILE_BUFSIZE	16384

/****************************************************************************
*																			*
*								Stream Structures							*
*																			*
****************************************************************************/

#ifdef USE_TCP

/* The network-stream specific information stored as part of the STREAM
   data type */

typedef struct NS {
	/* General information for the network stream.  For a server the
	   listenSocket is the (possibly shared) common socket that the server 
	   is listening on, the netSocket is the ephemeral socket used for
	   communications */
	STREAM_PROTOCOL_TYPE protocol;/* Network protocol type */
	int nFlags;					/* Network-specific flags */
	int netSocket, listenSocket;/* Network socket */
	CRYPT_SESSION iTransportSession;/* cryptlib session as transport layer */

	/* Network timeout information.  The timeout value depends on whether 
	   the stream is in the connect/handshake phase or the data transfer 
	   phase.  The handshake phase is logically treated as part of the 
	   connect phase even though from the stream point of view it's part of 
	   the data transfer phase.  Initially the stream timeout is set to the 
	   connect timeout and the saved timeout is set to the data transfer 
	   timeout.  Once the connect/handshake has completed, the stream 
	   timeout is set to the saved data transfer timeout and the saved 
	   timeout is cleared */
	int timeout, savedTimeout;	/* Network comms timeout */

	/* Network streams require separate read/write buffers for packet
	   assembly/disassembly so we provide a write buffer alongside the 
	   generic stream read buffer */
	BUFFER( writeBufSize, writeBufEnd ) \
	BYTE *writeBuffer;			/* Write buffer */
	int writeBufSize;			/* Total size of buffer */
	int writeBufEnd;			/* Last buffer position with valid data */

	/* General network-related information.  The server FQDN is held in 
	   dynamically-allocated storage, the optional path for HTTP is a pointer 
	   into the host string at the appropriate location */
	BUFFER_FIXED( hostLen ) \
	char *host;
	int hostLen;
	BUFFER_OPT_FIXED( pathLen ) \
	char *path;
	int pathLen;
	int port;					/* Host name, path on host, and port */
	BUFFER( CRYPT_MAX_TEXTSIZE / 2, clientAddressLen ) \
	char clientAddress[ ( CRYPT_MAX_TEXTSIZE / 2 ) + 4 ];
	int clientAddressLen;		/* Client IP address (dotted-decimal) */
	int clientPort;				/* Client port */

	/* Sometimes we can fingerprint the application running on the peer 
	   system, which is useful for working around buggy implementations.  
	   The following value stores the peer application type, if known */
	STREAM_PEER_TYPE systemType;

	/* If a network error condition is fatal we set the persistentStatus 
	   value.  This is checked by the higher-level stream code and copied 
	   to to stream persistent status if required */
	int persistentStatus;

	/* Last-error information returned from lower-level code */
	ERROR_INFO errorInfo;

	/* Network stream access functions.  The general read and write 
	   functions are for the higher-level network access routines such as 
	   HTTP and CMP I/O, the transport I/O functions are for transport-level 
	   I/O that sits below the general I/O.  Finally, there's an 
	   intermediate function that adds speculative read-ahead buffering to 
	   the transport-level read to improve performance for higher-level 
	   protocols like HTTP that have to read a byte at a time in some 
	   places */
	CHECK_RETVAL_BOOL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	BOOLEAN ( *sanityCheckFunction )( IN const struct ST *stream );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
	int ( *readFunction )( INOUT struct ST *stream, 
						   OUT_BUFFER( maxLength, *length ) void *buffer, 
						   IN_LENGTH const int maxLength, 
						   OUT_LENGTH_Z int *length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
	int ( *writeFunction )( INOUT struct ST *stream, 
							IN_BUFFER( length ) const void *buffer, 
							IN_LENGTH const int maxLength,
							OUT_LENGTH_Z int *length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *transportConnectFunction )( INOUT struct NS *netStream, 
									   IN_BUFFER_OPT( length ) const char *hostName, 
									   IN_LENGTH_DNS_Z const int hostNameLen, 
									   IN_PORT const int port );
	STDC_NONNULL_ARG( ( 1 ) ) \
	void ( *transportDisconnectFunction )( INOUT struct NS *netStream, 
										   const BOOLEAN fullDisconnect );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
	int ( *transportReadFunction )( INOUT struct ST *stream, 
									OUT_BUFFER( maxLength, *length ) BYTE *buffer, 
									IN_LENGTH const int maxLength, 
									OUT_LENGTH_Z int *length, 
									IN_FLAGS_Z( TRANSPORT ) \
									const int flags );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
	int ( *transportWriteFunction )( INOUT struct ST *stream, 
									 IN_BUFFER( length ) const BYTE *buffer, 
									 IN_LENGTH const int maxLength, 
									 OUT_LENGTH_Z int *length, 
									 IN_FLAGS_Z( TRANSPORT ) \
										const int flags );
	CHECK_RETVAL_BOOL_FNPTR \
	BOOLEAN ( *transportOKFunction )( void );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *transportCheckFunction )( INOUT struct NS *netStream );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
	int ( *bufferedTransportReadFunction )( INOUT struct ST *stream, 
											OUT_BUFFER( maxLength, *length ) \
												BYTE *buffer, 
											IN_LENGTH const int maxLength, 
											OUT_LENGTH_Z int *length, 
											IN_FLAGS_Z( TRANSPORT ) \
											const int flags );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
	int ( *bufferedTransportWriteFunction )( INOUT struct ST *stream, 
											 IN_BUFFER( length ) const BYTE *buffer,
											 IN_LENGTH const int maxLength, 
											 OUT_LENGTH_Z int *length, 
											 IN_FLAGS_Z( TRANSPORT ) \
												const int flags );

	/* Variable-length storage for the stream buffers */
	DECLARE_VARSTRUCT_VARS;
	} NET_STREAM_INFO;
#else

typedef void *NET_STREAM_INFO;	/* Dummy for function prototypes */

#endif /* USE_TCP */

/****************************************************************************
*																			*
*							Stream Function Prototypes						*
*																			*
****************************************************************************/

/* Stream query functions to determine whether a stream is a memory-mapped 
   file stream or a virtual file stream.  The memory-mapped stream check is 
   used when we can eliminate extra buffer allocation if all data is 
   available in memory.  The virtual file stream check is used where the 
   low-level access routines have converted a file on a CONFIG_NO_STDIO 
   system to a memory stream that acts like a file stream */

#define sIsMemMappedStream( stream ) \
		( ( ( stream )->type == STREAM_TYPE_FILE ) && \
		  ( ( stream )->flags & STREAM_FFLAG_MMAPPED ) )
#define sIsVirtualFileStream( stream ) \
		( ( ( stream )->type == STREAM_TYPE_MEMORY ) && \
		  ( ( stream )->flags & STREAM_MFLAG_VFILE ) )

/* Network URL processing functions in net_url.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int parseURL( OUT URL_INFO *urlInfo, 
			  IN_BUFFER( urlLen ) const char *url, 
			  IN_LENGTH_SHORT const int urlLen,
			  IN_PORT_OPT const int defaultPort, 
			  IN_ENUM( URL_TYPE ) const URL_TYPE urlTypeHint,
			  const BOOLEAN preParseOnly );

/* Network proxy functions in net_proxy.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int connectViaSocksProxy( INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int connectViaHttpProxy( INOUT STREAM *stream, INOUT ERROR_INFO *errorInfo );
#if defined( __WIN32__ )
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
  int findProxyUrl( OUT_BUFFER( proxyMaxLen, *proxyLen ) char *proxy, 
					IN_LENGTH_DNS const int proxyMaxLen, 
					OUT_LENGTH_DNS_Z int *proxyLen,
					IN_BUFFER( urlLen ) const char *url, 
					IN_LENGTH_DNS const int urlLen );
#else
  #define findProxyUrl( proxy, proxyMaxLen, proxyLen, url, urlLen )	CRYPT_ERROR_NOTFOUND
#endif /* Win32 */

/* Network access mapping functions */

STDC_NONNULL_ARG( ( 1 ) ) \
void setAccessMethodTCP( INOUT NET_STREAM_INFO *netStream );
STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerHTTP( INOUT NET_STREAM_INFO *netStream );
STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerCMP( INOUT NET_STREAM_INFO *netStream );
STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerDirect( INOUT NET_STREAM_INFO *netStream );
STDC_NONNULL_ARG( ( 1 ) ) \
void setStreamLayerBuffering( INOUT NET_STREAM_INFO *netStream,
							  const BOOLEAN useTransportBuffering );
#if 0	/* See comment in net_trans.c */
STDC_NONNULL_ARG( ( 1 ) ) \
void setAccessMethodTransportSession( INOUT NET_STREAM_INFO *netStream );
#else
#define setAccessMethodTransportSession( netStream )
#endif /* 0 */

#endif /* _STREAM_INT_DEFINED */
