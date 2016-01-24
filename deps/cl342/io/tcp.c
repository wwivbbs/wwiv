/****************************************************************************
*																			*
*						cryptlib TCP/IP Interface Routines					*
*						Copyright Peter Gutmann 1998-2007					*
*																			*
****************************************************************************/

#include <ctype.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "stream_int.h"
  #include "tcp.h"
#else
  #include "crypt.h"
  #include "io/stream_int.h"
  #include "io/tcp.h"
#endif /* Compiler-specific includes */

#ifdef USE_TCP

/****************************************************************************
*																			*
*						 		Init/Shutdown Routines						*
*																			*
****************************************************************************/

/* Forward declarations for socket pool functions */

CHECK_RETVAL \
static int initSocketPool( void );
static void endSocketPool( void );

#ifdef __WINDOWS__

#ifndef TEXT
  #define TEXT		/* Win32 windows.h defines this, but not the Win16 one */
#endif /* TEXT */

#ifdef _MSC_VER
  #pragma warning( disable: 4127 )	/* False-positive in winsock.h */
#endif /* VC++ */

/* Global function pointers.  These are necessary because the functions need
   to be dynamically linked since not all systems contain the necessary
   libraries */

static INSTANCE_HANDLE hTCP, hIPv6;

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
		SOCKET ( SOCKET_API *ACCEPT )( IN SOCKET s, OUT struct sockaddr *addr,
									   INOUT int *addrlen );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
		int ( SOCKET_API *BIND )( IN SOCKET s, 
								  const struct sockaddr FAR *addr, 
								  IN_LENGTH_DNS int namelen );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
		int ( SOCKET_API *CONNECT )( IN SOCKET s, IN_BUFFER( namelen ) \
									 const struct sockaddr *name, 
									 IN_LENGTH_DNS int namelen );
typedef STDC_NONNULL_ARG( ( 4, 5 ) ) \
		int ( SOCKET_API *GETSOCKOPT )( IN SOCKET s, IN int level, 
										IN int optname, 
										OUT_BUFFER_FIXED( *optlen ) char *optval, 
										INOUT int FAR *optlen );
typedef CHECK_RETVAL \
		int ( SOCKET_API *LISTEN )( IN SOCKET s, IN int backlog );
typedef CHECK_RETVAL RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
		int ( SOCKET_API *RECV )( IN SOCKET s, 
								  OUT_BUFFER( len, return ) char *buf, 
								  IN_LENGTH int len, IN int flags );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 4, 5 ) ) \
		int ( SOCKET_API *SELECT )( IN int nfds, INOUT_OPT fd_set *readfds, 
									INOUT_OPT fd_set *writefds, 
									INOUT fd_set *exceptfds, 
									const struct timeval *timeout );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
		int ( SOCKET_API *SEND )( IN SOCKET s, 
								  IN_BUFFER( len ) const char *buf, 
								  IN_LENGTH int len, IN int flags );
typedef STDC_NONNULL_ARG( ( 4 ) ) \
		int ( SOCKET_API *SETSOCKOPT )( IN SOCKET s, IN int level, \
										IN int optname,
										IN_BUFFER( optlen ) char *optval, 
										IN int optlen );
typedef int ( SOCKET_API *SHUTDOWN )( IN SOCKET s, IN int how );
typedef CHECK_RETVAL \
		SOCKET ( SOCKET_API *SOCKETFN )( IN int af, IN int type, 
										 IN int protocol );
#ifdef __WINDOWS__
typedef int ( SOCKET_API *CLOSESOCKET )( IN SOCKET s );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
		int ( SOCKET_API *FDISSETFN )( IN SOCKET s, fd_set *fds );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
		int ( SOCKET_API *IOCTLSOCKET )( IN SOCKET s, IN long cmd, 
										 INOUT u_long FAR *argp );
typedef int ( SOCKET_API *WSACLEANUP )( void );
typedef CHECK_RETVAL \
		int ( SOCKET_API *WSAGETLASTERROR )( void );
typedef CHECK_RETVAL \
		int ( SOCKET_API *WSASTARTUP )( IN WORD wVersionRequested, 
										OUT LPWSADATA lpWSAData );
#endif /* __WINDOWS__ */
static ACCEPT paccept = NULL;
static BIND pbind = NULL;
static CONNECT pconnect = NULL;
static GETSOCKOPT pgetsockopt = NULL;
static LISTEN plisten = NULL;
static RECV precv = NULL;
static SELECT pselect = NULL;
static SEND psend = NULL;
static SETSOCKOPT psetsockopt = NULL;
static SHUTDOWN pshutdown = NULL;
static SOCKETFN psocket = NULL;
#ifdef __WINDOWS__
static CLOSESOCKET pclosesocket = NULL;
static FDISSETFN pFDISSETfn = NULL;
static IOCTLSOCKET pioctlsocket = NULL;
static WSACLEANUP pWSACleanup = NULL;
static WSAGETLASTERROR pWSAGetLastError = NULL;
static WSASTARTUP pWSAStartup = NULL;
#endif /* __WINDOWS__ */
#if ( defined( sun ) && OSVERSION > 4 )
  static int *h_errnoPtr;

  #undef getHostErrorCode
  #define getHostErrorCode()	*h_errnoPtr
#endif /* Slowaris */

#define accept				paccept
#define bind				pbind
#define connect				pconnect
#define getsockopt			pgetsockopt
#define listen				plisten
#define recv				precv
#define select				pselect
#define send				psend
#define setsockopt			psetsockopt
#define shutdown			pshutdown
#define socket				psocket
#ifdef __WINDOWS__
#define closesocket			pclosesocket
#define __WSAFDIsSet		pFDISSETfn
#define ioctlsocket			pioctlsocket
#define WSACleanup			pWSACleanup
#ifndef WSAGetLastError
  /* In some environments WSAGetLastError() is a macro that maps to
     GetLastError() */
  #define WSAGetLastError	pWSAGetLastError
  #define DYNLOAD_WSAGETLASTERROR
#endif /* WSAGetLastError */
#define WSAStartup			pWSAStartup
#endif /* __WINDOWS__ */

/* Dynamically load and unload any necessary TCP/IP libraries.  Under Windows
   the dynamic loading is complicated by the existence of Winsock 1 vs.
   Winsock 2, all recent systems use Winsock 2 but we allow for Winsock 1 as
   well just in case */

#ifdef __WINDOWS__
  #ifdef __WIN16__
	#define TCP_LIBNAME			"winsock.dll"
  #elif defined( __WIN32__ )
	#define TCP_LIBNAME			TEXT( "ws2_32.dll" )
	#define WINSOCK_OLD_LIBNAME	TEXT( "wsock32.dll" )
  #elif defined( __WINCE__ )
	#define TCP_LIBNAME			TEXT( "ws2.dll" )
  #else
	#error Unknown Windows variant encountered
  #endif /* Win16/Win32/WinCE */
#else
  #define TCP_LIBNAME			"libsocket.so"
  #define TEXT( x )				x
#endif /* OS-specific TCP/IP library naming */

RETVAL \
int netInitTCP( void )
	{
#ifdef __WINDOWS__
	WSADATA wsaData;
  #ifdef __WIN16__
	UINT errorMode;
  #endif /* __WIN16__ */
	BOOLEAN ip6inWinsock = FALSE;
	int status;
#endif /* __WINDOWS__ */

	/* Obtain a handle to the modules containing the TCP/IP functions */
#ifdef __WINDOWS__
	hTCP = hIPv6 = NULL_INSTANCE;
  #if defined( __WIN16__ )
	errorMode = SetErrorMode( SEM_NOOPENFILEERRORBOX );
	hTCP = DynamicLoad( TCP_LIBNAME );
	SetErrorMode( errorMode );
	if( hTCP < HINSTANCE_ERROR )
		{
		hTCP = NULL_INSTANCE;
		return( CRYPT_ERROR );
		}
  #elif defined( __WIN32__ )
	if( ( hTCP = DynamicLoad( TCP_LIBNAME ) ) == NULL_INSTANCE && \
		( hTCP = DynamicLoad( WINSOCK_OLD_LIBNAME ) ) == NULL_INSTANCE )
		return( CRYPT_ERROR );
	if( DynamicBind( hTCP, "getaddrinfo" ) != NULL )
		ip6inWinsock = TRUE;
	else
		{
		/* Newer releases of Windows put the IPv6 functions in the Winsock 2
		   library, older (non-IPv6-enabled) releases had it available as an
		   experimental add-on using the IPv6 Technology Preview library */
		hIPv6 = DynamicLoad( "wship6.dll" );
		}
  #elif defined( __WINCE__ )
	if( ( hTCP = DynamicLoad( TCP_LIBNAME ) ) == NULL_INSTANCE )
		return( CRYPT_ERROR );
	if( DynamicBind( hTCP, TEXT( "getaddrinfo" ) ) != NULL )
		ip6inWinsock = TRUE;
  #endif /* Win16/Win32/WinCE */
#else
	if( ( hTCP = DynamicLoad( TCP_LIBNAME ) ) == NULL_INSTANCE )
		return( CRYPT_ERROR );
#endif /* OS-specific dynamic load */

	/* Now get pointers to the functions */
	accept = ( ACCEPT ) DynamicBind( hTCP, TEXT( "accept" ) );
	bind = ( BIND ) DynamicBind( hTCP, TEXT( "bind" ) );
	connect = ( CONNECT ) DynamicBind( hTCP, TEXT( "connect" ) );
	getsockopt = ( GETSOCKOPT ) DynamicBind( hTCP, TEXT( "getsockopt" ) );
	listen = ( LISTEN ) DynamicBind( hTCP, TEXT( "listen" ) );
	recv = ( RECV ) DynamicBind( hTCP, TEXT( "recv" ) );
	select = ( SELECT ) DynamicBind( hTCP, TEXT( "select" ) );
	send = ( SEND ) DynamicBind( hTCP, TEXT( "send" ) );
	setsockopt = ( SETSOCKOPT ) DynamicBind( hTCP, TEXT( "setsockopt" ) );
	shutdown = ( SHUTDOWN ) DynamicBind( hTCP, TEXT( "shutdown" ) );
	socket = ( SOCKETFN ) DynamicBind( hTCP, TEXT( "socket" ) );
#ifdef __WINDOWS__
	closesocket = ( CLOSESOCKET ) DynamicBind( hTCP, TEXT( "closesocket" ) );
	__WSAFDIsSet = ( FDISSETFN ) DynamicBind( hTCP, TEXT( "__WSAFDIsSet" ) );
	ioctlsocket = ( IOCTLSOCKET ) DynamicBind( hTCP, TEXT( "ioctlsocket" ) );
	WSACleanup = ( WSACLEANUP ) DynamicBind( hTCP, TEXT( "WSACleanup" ) );
  #ifdef DYNLOAD_WSAGETLASTERROR
	WSAGetLastError = ( WSAGETLASTERROR ) DynamicBind( hTCP, TEXT( "WSAGetLastError" ) );
  #endif /* DYNLOAD_WSAGETLASTERROR */
	WSAStartup = ( WSASTARTUP ) DynamicBind( hTCP, TEXT( "WSAStartup" ) );
	if( ip6inWinsock || hIPv6 != NULL_INSTANCE )
		status = initDNS( hTCP, ip6inWinsock ? hTCP : hIPv6 );
	else
		status = initDNS( hTCP, NULL_INSTANCE );
	if( cryptStatusError( status ) )
		{
		if( hIPv6 != NULL_INSTANCE )
			{
			DynamicUnload( hIPv6 );
			hIPv6 = NULL_INSTANCE;
			}
		DynamicUnload( hTCP );
		hTCP = NULL_INSTANCE;
		return( CRYPT_ERROR );
		}
#endif /* __WINDOWS__ */
#if ( defined( sun ) && OSVERSION > 4 )
	h_errnoPtr = ( int * ) DynamicBind( hTCP, "h_errno" );
	if( h_errnoPtr == NULL )
		{
		DynamicUnload( hTCP );
		hTCP = NULL_INSTANCE;
		return( CRYPT_ERROR );
		}
#endif /* Slowaris */

	/* Make sure that we got valid pointers for every TCP/IP function */
	if( accept == NULL || bind == NULL || connect == NULL || \
		getsockopt == NULL || listen == NULL || recv == NULL || \
		select == NULL || send == NULL || setsockopt == NULL || \
		shutdown == NULL || socket == NULL )
		{
		endDNS( hTCP );
		DynamicUnload( hTCP );
		hTCP = NULL_INSTANCE;
		if( hIPv6 != NULL_INSTANCE )
			{
			DynamicUnload( hIPv6 );
			hIPv6 = NULL_INSTANCE;
			}
		return( CRYPT_ERROR );
		}

#ifdef __WINDOWS__
	if( closesocket == NULL || __WSAFDIsSet == NULL || \
		ioctlsocket == NULL || WSACleanup == NULL ||
  #ifdef DYNLOAD_WSAGETLASTERROR
		WSAGetLastError == NULL ||
  #endif /* DYNLOAD_WSAGETLASTERROR */
		WSAStartup == NULL || \
		( WSAStartup( 2, &wsaData ) && WSAStartup( 1, &wsaData ) ) )
		{
		endDNS( hTCP );
		DynamicUnload( hTCP );
		hTCP = NULL_INSTANCE;
		if( hIPv6 != NULL_INSTANCE )
			{
			DynamicUnload( hIPv6 );
			hIPv6 = NULL_INSTANCE;
			}
		return( CRYPT_ERROR );
		}
#endif /* __WINDOWS__ */

	/* Set up the socket pool state information */
	return( initSocketPool() );
	}

void netEndTCP( void )
	{
	/* Clean up the socket pool state information */
	endSocketPool();

	endDNS( hTCP );
	if( hIPv6 != NULL_INSTANCE )
		DynamicUnload( hIPv6 );
	if( hTCP != NULL_INSTANCE )
		{
#ifdef __WINDOWS__
		/* Wipe the Sheets Afterwards and Cleanup */
		WSACleanup();
#endif /* __WINDOWS__ */
		DynamicUnload( hTCP );
		}
	hTCP = hIPv6 = NULL_INSTANCE;
	}

/* Return the status of the network interface */

CHECK_RETVAL_BOOL \
static BOOLEAN transportOKFunction( void )
	{
	return( hTCP != NULL_INSTANCE ? TRUE : FALSE );
	}
#else

RETVAL \
int netInitTCP( void )
	{
#ifdef __SCO_VERSION__
	struct sigaction act, oact;

	/* Work around the broken SCO/UnixWare signal-handling, which sometimes
	   sends a nonblocking socket a SIGIO (thus killing the process) when
	   waiting in a select() (this may have been fixed by the switch to
	   blocking sockets necessitated by Winsock bugs with non-blocking
	   sockets, and will be fixed long-term when SCO's long death march
	   eventually ends).  Since SIGIO is an alias for SIGPOLL, SCO doesn't 
	   help by reporting this as a "polling alarm".  To fix this we need to 
	   catch and swallow SIGIOs */
	memset( &act, 0, sizeof( act ) );
	act.sa_handler = SIG_IGN;
	sigemptyset( &act.sa_mask );
	if( sigaction( SIGIO, &act, &oact ) < 0 )
		{
		/* This assumes that stderr is open, i.e. that we're not a daemon.
		   This should be the case, at least during the development/debugging
		   stage */
		fprintf( stderr, "cryptlib: sigaction failed, errno = %d, "
				 "file = %s, line = %d.\n", errno, __FILE__, __LINE__ );
		abort();
		}

	/* Check for handler override. */
	if( oact.sa_handler != SIG_DFL && oact.sa_handler != SIG_IGN )
		{
		/* We overwrote the caller's handler, reinstate the old handler and
		   warn them about this */
		fprintf( stderr, "Warning: Conflicting SIGIO handling detected in "
				 "UnixWare socket bug\n         workaround, file " __FILE__
				 ", line %d.  This may cause\n         false SIGIO/SIGPOLL "
				"errors.\n", __LINE__ );
		sigaction( SIGIO, &oact, &act );
		}
#endif /* UnixWare/SCO */

	/* Set up the socket pool state information */
	return( initSocketPool() );
	}

void netEndTCP( void )
	{
	/* Clean up the socket pool state information */
	endSocketPool();

#ifdef __SCO_VERSION__
	signal( SIGIO, SIG_DFL );
#endif /* UnixWare/SCO */
	}

CHECK_RETVAL_BOOL \
static BOOLEAN transportOKFunction( void )
	{
#if defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ )
	static BOOLEAN transportOK = FALSE;

	if( !transportOK )
		{
		SOCKET netSocket;

		/* If the networking subsystem isn't enabled, attempting any network
		   operations will return ENOENT (which isn't a normal return code,
		   but is the least inappropriate thing to return).  In order to
		   check this before we get deep into the networking code, we create
		   a test socket here to make sure that everything is OK.  If the
		   network transport is unavailable, we re-try each time we're
		   called in case it's been enabled in the meantime */
		netSocket = socket( PF_INET, SOCK_STREAM, 0 );
		if( !isBadSocket( netSocket ) )
			{
			closesocket( netSocket );
			transportOK = TRUE;
			}
		}
	return( transportOK );
#else
	return( TRUE );
#endif /* OS-specific socket availability check */
	}
#endif /* __WINDOWS__ */

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Map of common error codes to strings.  The error code supplied by the
   caller is usually used as the return status code, however if a more
   specific error code than the default is available it's specified via the
   cryptSpecificCode member */

typedef struct {
	const int errorCode;		/* Native error code */
	const int cryptSpecificCode;/* Specific cryptlib error code */
	const BOOLEAN isFatal;		/* Seriousness level */
	BUFFER_FIXED( errorStringLength ) \
	const char FAR_BSS *errorString;
	const int errorStringLength;/* Error message */
	} SOCKETERROR_INFO;

#ifdef __WINDOWS__

static const SOCKETERROR_INFO FAR_BSS socketErrorInfo[] = {
	{ WSAECONNREFUSED, CRYPT_ERROR_PERMISSION, TRUE,
		"WSAECONNREFUSED: The attempt to connect was rejected", 52 },
	{ WSAEADDRNOTAVAIL, CRYPT_ERROR_NOTFOUND, TRUE,
		"WSAEADDRNOTAVAIL: The remote address is not a valid address", 59 },
	{ WSAECONNABORTED, CRYPT_OK, TRUE,
		"WSAECONNABORTED: Connection was terminated due to a time-out or "
		"other failure", 77 },
	{ WSAECONNRESET, CRYPT_OK, TRUE,
		"WSAECONNRESET: Connection was reset by the remote host executing "
		"a close", 72 },
	{ WSAEHOSTUNREACH, CRYPT_OK, TRUE,
		"WSAEHOSTUNREACH: Remote host cannot be reached from this host at "
		"this time", 74 },
	{ WSAEMSGSIZE, CRYPT_ERROR_OVERFLOW, FALSE,
		"WSAEMSGSIZE: Message is larger than the maximum supported by the "
		"underlying transport", 85 },
	{ WSAENETDOWN, CRYPT_OK, FALSE,
		"WSAENETDOWN: The network subsystem has failed", 45 },
	{ WSAENETRESET, CRYPT_OK, FALSE,
		"WSAENETRESET: Connection was broken due to keep-alive detecting a "
		"failure while operation was in progress", 105 },
	{ WSAENETUNREACH, CRYPT_ERROR_NOTAVAIL, FALSE,
		"WSAENETUNREACH: Network cannot be reached from this host at this "
		"time", 69 },
	{ WSAENOBUFS, CRYPT_ERROR_MEMORY, FALSE,
		"WSAENOBUFS: No buffer space available", 37 },
	{ WSAENOTCONN, CRYPT_OK, TRUE,
		"WSAENOTCONN: Socket is not connected", 36 },
	{ WSAETIMEDOUT, CRYPT_ERROR_TIMEOUT, FALSE,
		"WSAETIMEDOUT: Function timed out before completion", 50 },
	{ WSAHOST_NOT_FOUND, CRYPT_ERROR_NOTFOUND, FALSE,
		"WSAHOST_NOT_FOUND: Host not found", 34 },
	{ WSATRY_AGAIN,  CRYPT_OK, FALSE,
		"WSATRY_AGAIN: Host not found (non-authoritative)", 48 },
	{ WSANO_ADDRESS,  CRYPT_OK, FALSE,
		"WSANO_ADDRESS: No address record available for this name", 56 },
	{ WSANO_DATA,  CRYPT_OK, FALSE,
		"WSANO_DATA: Valid name, no data record of requested type", 56 },
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};
#define hostErrorInfo	socketErrorInfo		/* Winsock uses unified error codes */

#define TIMEOUT_ERROR	WSAETIMEDOUT		/* Code for timeout error */

#else

static const SOCKETERROR_INFO FAR_BSS socketErrorInfo[] = {
	{ EACCES, CRYPT_ERROR_PERMISSION, TRUE,
		"EACCES: Permission denied", 25 },
	{ EADDRINUSE, CRYPT_OK, TRUE,
		"EADDRINUSE: Address in use", 26 },
	{ EADDRNOTAVAIL, CRYPT_ERROR_NOTFOUND, TRUE,
		"EADDRNOTAVAIL: Specified address is not available from the local "
		"machine", 72 },
	{ EAFNOSUPPORT, CRYPT_ERROR_NOTAVAIL, TRUE,
		"EAFNOSUPPORT: Address family not supported", 42 },
	{ EALREADY, CRYPT_OK, FALSE,
		"EALREADY: Connection already in progress", 41 },
	{ EBADF, CRYPT_OK, FALSE,
		"EBADF: Bad file descriptor", 26 },
#if !( defined( __PALMOS__ ) || defined( __SYMBIAN32__ ) )
	{ ECONNABORTED, CRYPT_OK, TRUE,
		"ECONNABORTED: Software caused connection abort", 46 },
	{ ECONNRESET, CRYPT_OK, TRUE,
		"ECONNRESET: Connection was forcibly closed by remote host", 57 },
#endif /* PalmOS || Symbian OS */
	{ ECONNREFUSED, CRYPT_ERROR_PERMISSION, TRUE,
		"ECONNREFUSED: Attempt to connect was rejected", 45 },
	{ EINPROGRESS, CRYPT_OK, FALSE,
		"EINPROGRESS: Operation in progress", 34 },
	{ EINTR, CRYPT_OK, FALSE,
		"EINTR: Function was interrupted by a signal", 43 },
	{ EIO, CRYPT_OK, TRUE,
		"EIO: Input/output error", 24 },
	{ EISCONN, CRYPT_OK, FALSE,
		"EISCONN: Socket is connected", 28 },
	{ EMFILE, CRYPT_OK, FALSE,
		"EMFILE: Per-process descriptor table is full", 44 },
#ifndef __SYMBIAN32__
	{ EMSGSIZE, CRYPT_ERROR_OVERFLOW, FALSE,
		"EMSGSIZE: Message is too large to be sent all at once", 53 },
	{ ENETUNREACH, CRYPT_OK, FALSE,
		"ENETUNREACH: No route to the network or host is present", 55 },
	{ ENOBUFS, CRYPT_ERROR_MEMORY, FALSE,
		"ENOBUFS: Insufficient system resources available to complete the "
		"call", 69 },
	{ ENODEV, CRYPT_OK, TRUE,
		"ENODEV: No such device", 22 },
	{ ENOPROTOOPT, CRYPT_OK, TRUE,
		"ENOPROTOOPT: Protocol not available", 35 },
	{ ENOTCONN, CRYPT_OK, TRUE,
		"ENOTCONN: Socket is not connected", 33 },
	{ ENOTSOCK, CRYPT_OK, TRUE,
		"ENOTSOCK: Not a socket", 22 },
#endif /* Symbian OS */
	{ EPERM, CRYPT_ERROR_PERMISSION, TRUE,
		"EPERM: Operation not permitted", 30 },
	{ EPROTOTYPE, CRYPT_ERROR_NOTAVAIL, TRUE,
		"EPROTOTYPE: Protocol wrong type for socket", 42 },
	{ ETIMEDOUT, CRYPT_ERROR_TIMEOUT, FALSE,
		"ETIMEDOUT: Function timed out before completion", 47 },
	{ HOST_NOT_FOUND, CRYPT_ERROR_NOTFOUND, TRUE,
		"HOST_NOT_FOUND: Not an official hostname or alias", 49 },
#ifndef __ECOS__
	{ NO_ADDRESS, CRYPT_ERROR_NOTFOUND, TRUE,
		"NO_ADDRESS: Name is valid but does not have an IP address at the "
		"name server", 76 },
#endif /* __ECOS__ */
	{ TRY_AGAIN, CRYPT_OK, FALSE,
		"TRY_AGAIN: Local server did not receive a response from an "
		"authoritative server", 79 },
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};

#define TIMEOUT_ERROR	ETIMEDOUT			/* Code for timeout error */

static const SOCKETERROR_INFO FAR_BSS hostErrorInfo[] = {
	{ HOST_NOT_FOUND, CRYPT_ERROR_NOTFOUND, TRUE,
		"HOST_NOT_FOUND: Host not found", 30 },
#ifndef __ECOS__
	{ NO_ADDRESS, CRYPT_ERROR_NOTFOUND, TRUE,
		"NO_ADDRESS: No address record available for this name", 53 },
#endif /* __ECOS__ */
	{ NO_DATA, CRYPT_ERROR_NOTFOUND, TRUE,
		"NO_DATA: Valid name, no data record of requested type", 53 },
	{ TRY_AGAIN,  CRYPT_OK, FALSE,
		"TRY_AGAIN: Local server did not receive a response from an "
		"authoritative server", 79 },
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};
#endif /* System-specific socket error codes */

/* Get and set the low-level error information from a socket- and host-
   lookup-based error.  In theory under Windows we could also use the 
   Network List Manager to try and get additional diagnostic information 
   but this is only available under Vista and requires playing with COM 
   objects, the significant extra complexity caused by this isn't worth the 
   tiny additional level of granularity that we might gain in reporting 
   errors.  In any case the NLM functionality seems primarily intended for 
   interactively obtaining information before any networking actions are 
   initiated ("Do we currently have an Internet connection?") rather than 
   diagnosing problems afterwards ("What went wrong with the attempt to 
   initiate an Internet connection?") */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int mapError( NET_STREAM_INFO *netStream, 
					 const int netStreamErrorCode,
					 const BOOLEAN useHostErrorInfo, 
					 IN_ERROR int status )
	{
	const SOCKETERROR_INFO *errorInfo = \
					useHostErrorInfo ? hostErrorInfo : socketErrorInfo;
	const int errorInfoSize = useHostErrorInfo ? \
					FAILSAFE_ARRAYSIZE( hostErrorInfo, SOCKETERROR_INFO ) : \
					FAILSAFE_ARRAYSIZE( socketErrorInfo, SOCKETERROR_INFO );
	int i;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( cryptStatusError( status ) );

	clearErrorString( &netStream->errorInfo );
	if( netStreamErrorCode == 0 )
		{
		/* There's no further error information available, we can't report
		   any more detail */
		return( status );
		}
	for( i = 0; i < errorInfoSize && \
				errorInfo[ i ].errorCode != CRYPT_ERROR; i++ )
		{
		if( errorInfo[ i ].errorCode == netStreamErrorCode )
			{
			REQUIRES( errorInfo[ i ].errorStringLength > 16 && \
					  errorInfo[ i ].errorStringLength < 150 );
			setErrorString( NETSTREAM_ERRINFO, errorInfo[ i ].errorString, 
							errorInfo[ i ].errorStringLength );
			if( errorInfo[ i ].cryptSpecificCode != CRYPT_OK )
				{
				/* There's a more specific error code than the generic one
				   that we've been given available, use that instead */
				status = errorInfo[ i ].cryptSpecificCode;
				}
			if( errorInfo[ i ].isFatal )
				{
				/* It's a fatal error, make it persistent for the stream */
				netStream->persistentStatus = status;
				}
			break;
			}
		}
	ENSURES( i < errorInfoSize );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getSocketError( NET_STREAM_INFO *netStream, 
					IN_ERROR const int status,
					OUT_INT_Z int *socketErrorCode )
	{
	const int errorCode = getErrorCode();

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtr( socketErrorCode, sizeof( int ) ) );

	REQUIRES( cryptStatusError( status ) );

	/* Get the low-level error code and map it to an error string if
	   possible */
	*socketErrorCode = errorCode;

	return( mapError( netStream, errorCode, FALSE, status ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getHostError( NET_STREAM_INFO *netStream, 
				  IN_ERROR const int status )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( cryptStatusError( status ) );

	/* Get the low-level error code and map it to an error string if
	   possible */
	return( mapError( netStream, getHostErrorCode(), TRUE, status ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setSocketError( INOUT NET_STREAM_INFO *netStream, 
					IN_BUFFER( errorMessageLength ) const char *errorMessage, 
					IN_LENGTH_ERRORMESSAGE const int errorMessageLength,
					IN_ERROR const int status, const BOOLEAN isFatal )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtr( errorMessage, 16 ) );

	REQUIRES( errorMessageLength > 16 && \
			  errorMessageLength < MAX_INTLENGTH );
	REQUIRES( cryptStatusError( status ) );

	/* Set a cryptlib-supplied socket error message */
	setErrorString( NETSTREAM_ERRINFO, errorMessage, errorMessageLength );
	if( isFatal )
		{
		/* It's a fatal error, make it persistent for the stream */
		netStream->persistentStatus = status;
		}
	return( status );
	}

/* Some buggy firewall software will block any data transfer attempts made 
   after the initial connection setup, if we're in a situation where this 
   can happen then we check for the presence of a software firewall and 
   report a problem due to the firewall rather than a general networking 
   problem if we find one */

#ifdef __WIN32__

#define MAX_DRIVERS     1024

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkFirewallError( INOUT NET_STREAM_INFO *netStream )
	{
	INSTANCE_HANDLE hPSAPI = NULL_INSTANCE;
	typedef BOOL ( WINAPI *ENUMDEVICEDRIVERS )( LPVOID *lpImageBase, DWORD cb,
												LPDWORD lpcbNeeded );
	typedef DWORD ( WINAPI *GETDEVICEDRIVERBASENAME )( LPVOID ImageBase,
													   LPTSTR lpBaseName,
													   DWORD nSize );
	ENUMDEVICEDRIVERS pEnumDeviceDrivers;
	GETDEVICEDRIVERBASENAME pGetDeviceDriverBaseName;
	LPVOID drivers[ MAX_DRIVERS + 8 ];
	DWORD cbNeeded;
	int i;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* Use the PSAPI library to check for the presence of firewall filter
	   drivers.  Since this operation is rarely performed and is only called
	   as part of an error handler in which performance isn't a major factor
	   we load the library on demand each time (which we'd have to do in any 
	   case because it's not supported on older systems) rather than using
	   an on-init load as we do for the networking functions */
	if( ( hPSAPI = DynamicLoad( "psapi.dll" ) ) == NULL_INSTANCE )
		return( CRYPT_ERROR_TIMEOUT );
	pEnumDeviceDrivers = ( ENUMDEVICEDRIVERS ) \
						 GetProcAddress( hPSAPI, "EnumDeviceDrivers" );
	pGetDeviceDriverBaseName = ( GETDEVICEDRIVERBASENAME ) \
							   GetProcAddress( hPSAPI, "GetDeviceDriverBaseNameA" );
	if( pEnumDeviceDrivers == NULL || \
		pGetDeviceDriverBaseName == NULL || \
		!pEnumDeviceDrivers( drivers, MAX_DRIVERS * sizeof( DWORD ), 
							 &cbNeeded ) )
		{
		DynamicUnload( hPSAPI );
		return( CRYPT_ERROR_TIMEOUT );
		}

	/* Check whether a suspect filter driver is present */
	for( i = 0; i < cbNeeded / sizeof( DWORD ); i++ )
		{
		typedef struct {
			const char *name;
			const int nameLen;
			const BOOLEAN isMcafee;
			} DRIVER_INFO;
		static const DRIVER_INFO driverInfoTbl[] = {
			{ "firelm01.sys", 8, TRUE },	/* McAfee Host IPS */
			{ "firehk4x.sys", 8, TRUE },	/* McAfee Host IPS */
			{ "firehk5x.sys", 8, TRUE },	/* McAfee Host IPS */
			{ "fw220.sys", 5, TRUE },		/* McAfee FW */
			{ "mpfirewall.sys", 10, TRUE },	/* McAfee personal FW */
			{ "symtdi.sys", 6, FALSE },		/* Symantec TDI */
			{ "spbbcdrv.sys", 8, FALSE },	/* Norton Personal FW */
			{ NULL, 0, FALSE }, { NULL, 0, FALSE }
			};
		char driverName[ 256 + 8 ];
		int driverNameLen, driverIndex;

		driverNameLen = pGetDeviceDriverBaseName( drivers[ i ], 
												  driverName, 256 );
		if( driverNameLen <= 0 )
			continue;
		for( driverIndex = 0; 
			 driverInfoTbl[ driverIndex ].name != NULL && \
				driverIndex < FAILSAFE_ARRAYSIZE( driverInfoTbl, \
												  DRIVER_INFO );
			 driverIndex++ )
			{
			if( driverNameLen >= driverInfoTbl[ driverIndex ].nameLen && \
				!strnicmp( driverName, driverInfoTbl[ driverIndex ].name,
						   driverInfoTbl[ driverIndex ].nameLen ) )
				{
				DynamicUnload( hPSAPI );
				retExt( CRYPT_ERROR_TIMEOUT,
						( CRYPT_ERROR_TIMEOUT, NETSTREAM_ERRINFO,
						  "Network data transfer blocked, probably due to "
						  "%s firewall software installed on the PC", 
						  driverInfoTbl[ driverIndex ].isMcafee ? \
							"McAfee" : "Symantec/Norton" ) );
				}
			}
		}
	DynamicUnload( hPSAPI );

	return( CRYPT_ERROR_TIMEOUT );
	}
#else
  #define checkFirewallError( netStream )	CRYPT_ERROR_TIMEOUT
#endif /* Win32 */

#if defined( __BEOS__ ) && !defined( BONE_VERSION )

/* BeOS doesn't support checking for anything except readability in select()
   and only supports one or two socket options so we define our own versions 
   of these functions that no-op out unsupported options */

#undef select   /* Restore normal select() around the wrapper */

static int my_select( int socket_range, struct fd_set *read_bits,
					  struct fd_set *write_bits,
					  struct fd_set *exception_bits,
					  struct timeval *timeout )
	{
	/* BeOS doesn't support nonblocking connects, it always waits about a
	   minute for the connect and then times out, so it we get a wait on a
	   connecting socket we report it as being successful by exiting with
	   the fds as set by the caller and a successful return status */
	if( read_bits != NULL && write_bits != NULL )
		return( 1 );

	/* If we're checking for writeability the best that we can do is to
	   always report the socket as writeable.  Since the socket is a 
	   blocking socket the data will (eventually) get written */
	if( read_bits == NULL && write_bits != NULL )
		{
		if( exception_bits != NULL )
			FD_ZERO( exception_bits );
		return( 1 );
		}

	/* Since BeOS doesn't support checking for writeability or errors, we
	   have to clear these values before we call select() so that the 
	   caller won't find anything still set when we return */
	if( write_bits != NULL )
		FD_ZERO( write_bits );
	if( exception_bits != NULL )
		FD_ZERO( exception_bits );

	return( select( socket_range, read_bits, NULL, NULL, timeout ) );
	}

#define select( sockets, readFD, writeFD, exceptFD, timeout ) \
		my_select( sockets, readFD, writeFD, exceptFD, timeout )

static int my_setsockopt( int socket, int level, int option,
						  const void *data, uint size )
	{
	if( option != SO_NONBLOCK && option != SO_REUSEADDR )
		return( 0 );
	return( setsockopt( socket, level, option, data, size ) );
	}

static int my_getsockopt( int socket, int level, int option,
						  void *data, uint *size )
	{
	if( option != SO_ERROR )
		return( 0 );
	*( ( int * ) data ) = 0;	/* Clear return status */

	/* It's unclear whether the following setsockopt actually does anything
	   under BeOS or not.  If it fails, the alternative below may work */
#if 1
	return( setsockopt( socket, level, option, data, *size ) );
#else
	BYTE buffer[ 8 + 8 ];
	int count;

	count = recv( socket, buffer, 0, 0 );
	printf( "recv( 0 ) = %d, errno = %d.\n", count, errno );
	if( count < 0 )
		*( ( int * ) data ) = errno;
#endif /* 1 */
	}
#endif /* BeOS without BONE */

/****************************************************************************
*																			*
*							Network Socket Manager							*
*																			*
****************************************************************************/

/* cryptlib's separation kernel causes some problems with objects that use
   sockets because it doesn't allow sharing of sockets, which is a problem 
   because the Unix server programming model assumes that a single process 
   will listen on a socket and fork off children to handle incoming 
   connections (in fact the accept() function more or less forces you to do
   this whether you want to or not).  A second problem occurs because when a 
   thread is blocked in an object waiting on a socket there's no way to 
   unblock it apart from killing the thread (actually we could create some 
   sort of lookback socket and wait for it alongside the listen socket in 
   the pre-accept select wait, signalling a shutdown by closing the loopback 
   socket, but this starts to get ugly).  In order to work around this we 
   maintain a socket pool that serves two functions:

	- Maintains a list of sockets that an object is listening on to allow a
	  listening socket to be reused rather than having to listen on a
	  socket and close it as soon as an incoming connection is made in
	  order to switch to the connected socket.

	- Allows sockets to be closed from another thread, which results in any
	  objects waiting on them being woken up and exiting.

   For now we limit the socket pool to a maximum of 256 sockets (16 in
   resource-constrained environments) both as a safety feature to protect
   against runaway use of sockets in the calling application and because 
   cryptlib was never designed to function as a high-volume server 
   application.  If necessary this can be changed to dynamically expand the 
   socket pool in the same way that the kernel dynamically expands its 
   object table.  However it's not a good idea to simply remove the 
   restriction entirely (or set it to too high a value) because this can 
   cause problems with excess consumption of kernel resources.  For example 
   under Windows opening several tens of thousands of connections will 
   eventually return WSAENOBUFS when the nonpaged pool is exhausted.  At 
   this point things start to get problematic because many drivers don't 
   handle the inability to allocate memory very well, and can start to fail 
   and render the whole system unstable.  This is a general resource-
   consumption problem that affects all users of the shared nonpaged pool, 
   but we can at least make sure that we're not the cause of any crashes by 
   limiting our own consumption */

#ifdef CONFIG_CONSERVE_MEMORY
  #define SOCKETPOOL_SIZE		16
#else
  #define SOCKETPOOL_SIZE		256
#endif /* CONFIG_CONSERVE_MEMORY */

typedef struct {
	SOCKET netSocket;		/* Socket handle */
	int refCount;			/* Reference count for the socket */
	int iChecksum;			/* Family, interface, and port */
	BYTE iData[ 32 + 8 ];	/*	info for server socket */
	size_t iDataLen;
	} SOCKET_INFO;

static SOCKET_INFO *socketInfo;
static const SOCKET_INFO SOCKET_INFO_TEMPLATE = \
				{ INVALID_SOCKET, 0, 0, { 0 }, 0 };

/* Initialise and shut down the socket pool */

CHECK_RETVAL \
static int initSocketPool( void )
	{
	int i;

	/* Allocate and clear the socket pool */
	if( ( socketInfo = \
			clAlloc( "initSocketPool", SOCKETPOOL_SIZE * \
									   sizeof( SOCKET_INFO ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	for( i = 0; i < SOCKETPOOL_SIZE; i++ )
		socketInfo[ i ] = SOCKET_INFO_TEMPLATE;

	return( CRYPT_OK );
	}

static void endSocketPool( void )
	{
	clFree( "endSocketPool", socketInfo );
	}

/* Create/add and remove a socket to/from the pool.  The difference between
   creating and adding a socket is that newSocket() creates and adds a
   completely new socket while addSocket() adds an externally-created (via
   accept()) socket */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int newSocket( OUT SOCKET *newSocketPtr, 
					  const struct addrinfo *addrInfoPtr,
					  const BOOLEAN isServer )
	{
	SOCKET netSocket;
	int iCheck = DUMMY_INIT, i, status;

	assert( isWritePtr( newSocketPtr, sizeof( SOCKET ) ) );
	assert( isReadPtr( addrInfoPtr, sizeof( struct addrinfo ) ) );

	/* Clear return value */
	*newSocketPtr = INVALID_SOCKET;

	/* Perform any required pre-calculations before we acquire the mutex */
	if( isServer )
		{
		iCheck = checksumData( addrInfoPtr->ai_addr, 
							   addrInfoPtr->ai_addrlen );
		}

	status = krnlEnterMutex( MUTEX_SOCKETPOOL );
	if( cryptStatusError( status ) )
		return( status );

	/* If this is a server socket (i.e. one bound to a specific interface and
	   port), check to see whether there's already a socket bound here and if
	   there is, return the existing socket rather than creating a new one.
	   This check isn't currently totally foolproof since it compares some
	   nonessential fields that may differ for otherwise identical sockets
	   (it's difficult to do this in a clean manner because the comparison
	   becomes very protocol- and implementation-specific).  A workaround
	   would be to check whether the sin_family is AF_INET or AF_INET6 and
	   perform an appropriate situation-specific comparison, but this will
	   break the nice portability that was added by the RFC 2553 
	   reorganisation of socket functions for IPv6 */
	if( isServer )
		{
		for( i = 0; i < SOCKETPOOL_SIZE; i++ )
			{
			if( socketInfo[ i ].refCount > 0 && \
				socketInfo[ i ].iChecksum == iCheck && \
				socketInfo[ i ].iDataLen == addrInfoPtr->ai_addrlen && \
				!memcmp( socketInfo[ i ].iData, addrInfoPtr->ai_addr,
						 addrInfoPtr->ai_addrlen ) )
				{
				if( socketInfo[ i ].refCount >= 10000 )
					{
					krnlExitMutex( MUTEX_SOCKETPOOL );
					DEBUG_DIAG(( "Socket in pool has reference count > 10,000" ));
					assert( DEBUG_WARN );
					return( CRYPT_ERROR_OVERFLOW );
					}
				ENSURES( socketInfo[ i ].refCount > 0 && \
						 socketInfo[ i ].refCount < 10000 );
				ENSURES( !isBadSocket( socketInfo[ i ].netSocket ) );
				socketInfo[ i ].refCount++;
				*newSocketPtr = socketInfo[ i ].netSocket;
				krnlExitMutex( MUTEX_SOCKETPOOL );

				/* The socket already exists, don't perform any further
				   initialisation with it */
				return( CRYPT_OK );
				}
			}
		}

	/* Create a new socket entry */
	for( i = 0; i < SOCKETPOOL_SIZE; i++ )
		{
		/* Check whether this is a zombie socket that we couldn't close
		   earlier, usually due to written data being left in the TCP/IP
		   stack.  As a result it's probably trapped in the TIME_WAIT
		   state, so we periodically try and close it to free up the
		   resource */
		if( socketInfo[ i ].refCount <= 0 && \
			!isBadSocket( socketInfo[ i ].netSocket ) )
			{
			status = closesocket( socketInfo[ i ].netSocket );
			if( !isSocketError( status ) )
				socketInfo[ i ] = SOCKET_INFO_TEMPLATE;
			}

		if( isBadSocket( socketInfo[ i ].netSocket ) )
			break;
		}
	if( i >= SOCKETPOOL_SIZE )
		{
		krnlExitMutex( MUTEX_SOCKETPOOL );
		DEBUG_DIAG(( "Tried to add more than %d sockets to socket pool", 
					 SOCKETPOOL_SIZE ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_OVERFLOW );	/* Should never happen */
		}
	netSocket = socket( addrInfoPtr->ai_family,
						addrInfoPtr->ai_socktype, 0 );
	if( isBadSocket( netSocket ) )
		{
		krnlExitMutex( MUTEX_SOCKETPOOL );
		return( CRYPT_ERROR_OPEN );
		}
	socketInfo[ i ].netSocket = netSocket;
	if( isServer )
		{
		const int addrInfoSize = min( addrInfoPtr->ai_addrlen, 32 );

		/* Remember the details for this socket so that we can detect another
		   attempt to bind to it */
		socketInfo[ i ].iChecksum = checksumData( addrInfoPtr->ai_addr,
												  addrInfoPtr->ai_addrlen );
		memcpy( socketInfo[ i ].iData, addrInfoPtr->ai_addr,
				addrInfoSize );
		socketInfo[ i ].iDataLen = addrInfoSize;
		}
	socketInfo[ i ].refCount = 1;
	*newSocketPtr = netSocket;

	/* If we're creating a new server socket we can't unlock the socket info
	   yet because we need to bind it to a port before we do anything else
	   with it.  If we were to unlock the socket info another thread could
	   perform an accept() on the incompletely set up socket, so we return
	   with the socket info still locked.  When the caller has finished
	   setting it up they call newSocketDone() to signal that the socket is 
	   now really ready for use */
	if( isServer )
		return( OK_SPECIAL );

	krnlExitMutex( MUTEX_SOCKETPOOL );

	return( CRYPT_OK );
	}

static void newSocketDone( void )
	{
	/* The caller has finished setting up a new server socket, unlock the
	   socket info to allow others to access it */
	krnlExitMutex( MUTEX_SOCKETPOOL );
	}

CHECK_RETVAL \
static int addSocket( const SOCKET netSocket )
	{
	int i, status;

	REQUIRES( !isBadSocket( netSocket ) );

	status = krnlEnterMutex( MUTEX_SOCKETPOOL );
	if( cryptStatusError( status ) )
		return( status );

	/* Add an existing socket entry */
	for( i = 0; i < SOCKETPOOL_SIZE; i++ )
		{
		if( isBadSocket( socketInfo[ i ].netSocket ) )
			break;
		}
	if( i >= SOCKETPOOL_SIZE )
		{
		krnlExitMutex( MUTEX_SOCKETPOOL );
		DEBUG_DIAG(( "Tried to add more than %d sockets to socket pool", 
					 SOCKETPOOL_SIZE ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_OVERFLOW );
		}
	socketInfo[ i ] = SOCKET_INFO_TEMPLATE;
	socketInfo[ i ].netSocket = netSocket;
	socketInfo[ i ].refCount = 1;

	krnlExitMutex( MUTEX_SOCKETPOOL );

	return( CRYPT_OK );
	}

static void deleteSocket( const SOCKET netSocket )
	{
	int i, status;

	REQUIRES_V( !isBadSocket( netSocket ) );

	status = krnlEnterMutex( MUTEX_SOCKETPOOL );
	if( cryptStatusError( status ) )
		return;

	/* Find the entry for this socket in the pool.  There may not be one
	   present if the pool has received a shutdown signal and closed all
	   network sockets, so if we don't find it we just exit normally */
	for( i = 0; i < SOCKETPOOL_SIZE; i++ )
		{
		if( socketInfo[ i ].netSocket == netSocket )
			break;
		}
	if( i >= SOCKETPOOL_SIZE )
		{
		krnlExitMutex( MUTEX_SOCKETPOOL );
		return;
		}
	REQUIRES_V( socketInfo[ i ].refCount > 0 );

	/* Decrement the socket's reference count */
	socketInfo[ i ].refCount--;
	if( socketInfo[ i ].refCount <= 0 )
		{
		/* If the reference count has reached zero, close the socket
		   and delete the pool entry */
		status = closesocket( socketInfo[ i ].netSocket );
		if( isSocketError( status ) )
			{
			/* There was a problem closing the socket, mark it as not-
			   present for matching purposes but keep its entry active so
			   that we'll periodically try and close it when we search the
			   socket pool for these slots, and again when we close down */
			socketInfo[ i ].iChecksum = 0;
			memset( socketInfo[ i ].iData, 0,
					sizeof( socketInfo[ i ].iData ) );
			socketInfo[ i ].iDataLen = 0;

			DEBUG_DIAG(( "Couldn't close socket pool socket" ));
			assert( DEBUG_WARN );
			}
		else
			socketInfo[ i ] = SOCKET_INFO_TEMPLATE;
		}

	krnlExitMutex( MUTEX_SOCKETPOOL );
	}

/* Force all objects waiting on sockets to exit by closing their sockets.
   This is the only portable and reliable way to cause them to terminate 
   since an object waiting on a socket is marked as busy by the cryptlib 
   kernel, and in fact will be blocked inside the OS out of reach of even 
   the cryptlib kernel.  Alternatively, the user can provide their own 
   socket externally and close it from the outside, which will unblock the 
   thread waiting on it.

   A somewhat less drastic alternative to closing the socket is to use
   shutdown(), but the behaviour of this is somewhat implementation-specific.
   For example under Slowaris 5.x trying to shutdown a listening socket (to
   unlock a thread blocking in accept()) returns ENOTCONN, so the shutdown
   requires setting up a dummy connection to the socket to be shut down
   before it can actually be shut down.  Trying to shut down a thread blocked
   in connect() is more or less impossible under Slowaris 5.x.  Other systems
   are more flexible, but there's not enough consistency to rely on this */

void netSignalShutdown( void )
	{
	int i, status;

	/* Exactly what to do if we can't acquire the mutex is a bit complicated
	   because at this point our primary goal is to force all objects to exit 
	   rather than worrying about socket-pool consistency.  On the other
	   hand if another object is currently in the middle of cleaning up and
	   is holding the socket pool mutex we don't want to stomp on it while 
	   it's doing its cleanup.  Since failing to acquire the mutex is a 
	   special-case exception condition, it's not even possible to plan for
	   this since it's uncertain under which conditions (if ever) this 
	   situation would occur.  For now we play it by the book and don't do
	   anything if we can't acquire the mutex, which is at least 
	   consistent */
	status = krnlEnterMutex( MUTEX_SOCKETPOOL );
	if( cryptStatusError( status ) )
		retIntError_Void();

	/* For each open socket, close it and set its reference count to zero */
	for( i = 0; i < SOCKETPOOL_SIZE; i++ )
		{
		if( !isBadSocket( socketInfo[ i ].netSocket ) )
			{
			closesocket( socketInfo[ i ].netSocket );
			socketInfo[ i ] = SOCKET_INFO_TEMPLATE;
			}
		}

	krnlExitMutex( MUTEX_SOCKETPOOL );
	}

/****************************************************************************
*																			*
*							Network Socket Interface						*
*																			*
****************************************************************************/

/* Wait for I/O to become possible on a socket.  The particular use of 
   select that we employ here is reasonably optimal under load because we're 
   only asking select() to monitor a single descriptor.  There are a variety 
   of inefficiencies related to select that fall into either the category of 
   user <-> kernel copying or of descriptor list scanning.  For the first 
   category, when calling select() the system has to copy an entire list of 
   descriptors into kernel space and then back out again.  Large selects can 
   potentially contain hundreds or thousands of descriptors, which can in 
   turn involve allocating memory in the kernel and freeing it on return.  
   We're only using one so the amount of data to copy is minimal.

   The second category involves scanning the descriptor list, an O(n) 
   activity.  First the kernel has to scan the list to see whether there's 
   pending activity on a descriptor.  If there aren't any descriptors with 
   activity pending it has to update the descriptor's selinfo entry in the 
   event that the calling process calls tsleep() (used to handle event-based 
   process blocking in the kernel) while waiting for activity on the 
   descriptor.  After the select() returns or the process is woken up from a 
   tsleep() the user process in turn has to scan the list to see which 
   descriptors the kernel indicated as needing attention.  As a result, the 
   list has to be scanned three times.

   These problems arise because select() (and it's cousin poll()) are 
   stateless by design so everything has to be recalculated on each call.  
   After various false starts the kqueue interface is now seen as the best 
   solution to this problem.  However cryptlib's use of only a single 
   descriptor per select() avoids the need to use system-specific and rather 
   non-portable interfaces like kqueue (and earlier alternatives like Sun's 
   /dev/poll, FreeBSD's get_next_event(), and SGI's /dev/imon) */

typedef enum { 
	IOWAIT_NONE,			/* No I/O wait type */
	IOWAIT_READ,			/* Wait for read availability */
	IOWAIT_WRITE,			/* Wait for write availability */
	IOWAIT_CONNECT,			/* Wait for connect to complete */
	IOWAIT_ACCEPT,			/* Wait for accept to complete */
	IOWAIT_LAST				/* Last possible wait type */
	} IOWAIT_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int ioWait( INOUT NET_STREAM_INFO *netStream, 
				   IN_INT_Z const int timeout,
				   const BOOLEAN previousDataRead, 
				   IN_ENUM( IOWAIT ) const IOWAIT_TYPE type )
	{
	static const struct {
		const int status;
		const char *errorString;
		} errorInfo[] = {
		{ CRYPT_ERROR_OPEN, "unknown" },
		{ CRYPT_ERROR_READ, "read" },		/* IOWAIT_READ */
		{ CRYPT_ERROR_WRITE, "write" },		/* IOWAIT_WRITE */
		{ CRYPT_ERROR_OPEN, "connect" },	/* IOWAIT_CONNECT */
		{ CRYPT_ERROR_OPEN, "accept" },		/* IOWAIT_ACCEPT */
		{ CRYPT_ERROR_OPEN, "unknown" }, { CRYPT_ERROR_OPEN, "unknown" }
		};
	MONOTIMER_INFO timerInfo;
	struct timeval tv;
	fd_set readfds, writefds, exceptfds;
	fd_set *readFDPtr = ( type == IOWAIT_READ || \
						  type == IOWAIT_CONNECT || \
						  type == IOWAIT_ACCEPT ) ? &readfds : NULL;
	fd_set *writeFDPtr = ( type == IOWAIT_WRITE || \
						   type == IOWAIT_CONNECT ) ? &writefds : NULL;
	int selectIterations = 0, status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( timeout >= 0 && timeout < MAX_INTLENGTH );
	REQUIRES( type > IOWAIT_NONE && type < IOWAIT_LAST );

	/* Set up the information needed to handle timeouts and wait on the
	   socket.  If there's no timeout, we wait at least 5ms on the theory
	   that it isn't noticeable to the caller but ensures that we at least
	   get a chance to get anything that may be pending.

	   The exact wait time depends on the system, but usually it's quantised
	   to the system timer quantum.  This means that on Unix systems with a
	   1ms timer resolution the wait time is quantised on a 1ms boundary.
	   Under Windows NT/2000/XP/Vista it's quantised on a 10ms boundary 
	   (some early NT systems had a granularity ranging from 7.5 - 15ms but 
	   all newer systems use 10ms) and for Win95/98/ME it's quantised on a 
	   55ms boundary.  In other words when performing a select() on a Win95 
	   box it'll either return immediately or wait some multiple of 55ms 
	   even with the time set to 1ms.

	   In theory we shouldn't have to reset either the fds or the timeval
	   each time through the loop since we're only waiting on one descriptor
	   so it's always set and the timeval is a const, however some versions
	   of Linux can update it if the select fails due to an EINTR (which is
	   the exact reason why we'd be going through the loop a second time in
	   the first place) and/or if a file descriptor changes status (e.g. due 
	   to data becoming available) so we have to reset it each time to be on 
	   the safe side.  It would actually be nice if the tv value were 
	   updated reliably to reflect how long the select() had to wait since 
	   it'd provide a nice source of entropy for the randomness pool (we 
	   could simulate this by readig a high-res timer before and after the
	   select() but that would adds a pile of highly system-dependent code
	   and defeat the intent of making use of using the "free" entropy 
	   that's provided as a side-effect of the select().

	   The wait on connect is a slightly special case, the socket will
	   become writeable if the connect succeeds normally, but both readable
	   and writeable if there's an error on the socket or if there's data
	   already waiting on the connection (i.e. it arrives as part of the
	   connect).  It's up to the caller to check for these conditions */
	status = setMonoTimer( &timerInfo, timeout );
	if( cryptStatusError( status ) )
		return( status );
	do
		{
		if( readFDPtr != NULL )
			{
			FD_ZERO( readFDPtr );
			FD_SET( netStream->netSocket, readFDPtr );
			}
		if( writeFDPtr != NULL )
			{
			FD_ZERO( writeFDPtr );
			FD_SET( netStream->netSocket, writeFDPtr );
			}
		FD_ZERO( &exceptfds );
		FD_SET( netStream->netSocket, &exceptfds );
		tv.tv_sec = timeout;
		tv.tv_usec = ( timeout <= 0 ) ? 5000 : 0;

		/* See if we can perform the I/O */
		status = select( netStream->netSocket + 1, readFDPtr, writeFDPtr,
						 &exceptfds, &tv );

		/* If there's a problem and it's not something transient like an
		   interrupted system call, exit.  For a transient problem, we just
		   retry the select until the overall timeout expires */
		if( isSocketError( status ) && !isRestartableError() )
			{
			int dummy;

			return( getSocketError( netStream, errorInfo[ type ].status, 
									&dummy ) );
			}
		}
	while( isSocketError( status ) && \
		   !checkMonoTimerExpired( &timerInfo ) && \
		   selectIterations++ < FAILSAFE_ITERATIONS_MED );
	if( selectIterations > FAILSAFE_ITERATIONS_MED )
		{
		char errorMessage[ 128 + 8 ];
		int errorMessageLength;

		/* We've gone through the select loop a suspiciously large number
		   of times, there's something wrong.  In theory we could report 
		   this as a more serious error than a simple timeout since it means
		   that there's either a bug in our code or a bug in the select()
		   implementation, but without knowing in advance what caused this
		   can't-occur condition it's difficult to anticipate the correct
		   action to take, so all that we do is warn in the debug build */
		DEBUG_DIAG(( "select() went through %d iterations without "
					 "returning data", FAILSAFE_ITERATIONS_MED ));
		assert( DEBUG_WARN );
		errorMessageLength = sprintf_s( errorMessage, 128,
										"select() on %s went through %d "
										"iterations without returning a "
										"result",
										errorInfo[ type ].errorString, 
										selectIterations );
		return( setSocketError( netStream, errorMessage, errorMessageLength,
								CRYPT_ERROR_TIMEOUT, FALSE ) );
		}

	/* If the wait timed out, either explicitly in the select (status == 0)
	   or implicitly in the wait loop (isSocketError()), report it as a
	   select() timeout error */
	if( status == 0 || isSocketError( status ) )
		{
		char errorMessage[ 128 + 8 ];
		int errorMessageLength;

		/* If we've already received data from a previous I/O, tell the 
		   caller to use that as the transferred byte count even though we 
		   timed out this time round */
		if( previousDataRead )
			return( OK_SPECIAL );

		/* If it's a nonblocking wait (usually used as a poll to determine
		   whether I/O is possible) then a timeout isn't an error (this can
		   be distinguished from the previous OK_SPECIAL return by whether
		   previousDataRead is set or not) */
		if( timeout <= 0 )
			return( OK_SPECIAL );

		/* The select() timed out, exit */
		errorMessageLength = sprintf_s( errorMessage, 128,
										"Timeout on %s (select()) after %d "
										"seconds",
										errorInfo[ type ].errorString, 
										timeout );
		return( setSocketError( netStream, errorMessage, errorMessageLength,
								CRYPT_ERROR_TIMEOUT, FALSE ) );
		}

	/* If there's an exception condition on a socket, exit.  This is
	   implementation-specific, traditionally under Unix this only indicates
	   the arrival of out-of-band data rather than any real error condition,
	   but in some cases it can be used to signal errors.  In these cases we
	   have to explicitly check for an exception condition because some
	   types of errors will result in select() timing out waiting for
	   readability rather than indicating an error and returning.  In 
	   addition for OOB data we could just ignore the notification (which 
	   happens automatically with the default setting of SO_OOBINLINE = 
	   false and a socket owner to receive SIGURG's not set, the OOB data 
	   byte just languishes in a side-buffer), however we shouldn't be 
	   receiving OOB data so we treat that as an error too */
	if( FD_ISSET( netStream->netSocket, &exceptfds ) )
		{
		int socketErrorCode;

		status = getSocketError( netStream, errorInfo[ type ].status, 
								 &socketErrorCode );
		if( socketErrorCode == 0 )
			{
			/* If there's a (supposed) exception condition present but no
			   error information available then this may be a mis-handled
			   select() timeout.  This can happen with Winsock under
			   certain circumstances and seems to be related to another
			   socket-using application performing network I/O at the same 
			   time as we do the select() wait.  Non-Winsock cases can occur 
			   because some implementations don't treat a soft timeout as an 
			   error, and at least one (Tandem) returns EINPROGRESS rather 
			   than ETIMEDOUT, so we insert a timeout error code ourselves.
			   Since we're merely updating the extended internal error 
			   information (we already know what the actual error status
			   is) we don't need to do anything with the mapError() return 
			   value.

			   There is one special-case exception for this and that's when
			   we're waiting on a nonblocking connect, in which case a 
			   failure to connect due to e.g. an ECONNREFUSED will be 
			   reported as a select() error (this can happen under Winsock 
			   in some cases).  Since we can't be sure what the actual 
			   problem is without adding our own timer handling (a fast
			   reject would be due to an explicit notification like 
			   ECONNREFUSED while a slow reject might be an ENETUNREACH
			   or something similar) we can't report much more than a 
			   generic open error.  A genuine timeout error should have
			   been caught by the "wait timed out" code above */
			if( type == IOWAIT_CONNECT )
				{
				( void ) mapError( netStream, 0, FALSE, 
								   CRYPT_ERROR_OPEN );
				}
			else
				{
				( void ) mapError( netStream, TIMEOUT_ERROR, FALSE, 
								   CRYPT_ERROR_TIMEOUT );
				}
			}
		return( status );
		}

	/* The socket is read for reading or writing */
	ENSURES( status > 0 );
	ENSURES( ( type == IOWAIT_READ && \
			   FD_ISSET( netStream->netSocket, &readfds ) ) || \
			 ( type == IOWAIT_WRITE && \
			   FD_ISSET( netStream->netSocket, &writefds ) ) || \
			 ( type == IOWAIT_CONNECT && \
			   ( FD_ISSET( netStream->netSocket, &readfds ) || \
				 FD_ISSET( netStream->netSocket, &writefds ) ) ) || \
			 ( type == IOWAIT_ACCEPT ) );
	return( CRYPT_OK );
	}

/* Open a connection to a remote server or wait for a connection from a 
   remote client.  The connection-open function performs that most amazing 
   of all things, the nonblocking connect.  This is currently done in order 
   to allow a shorter timeout than the default fortnight or so but it also
   allows for two-phase connects in which we start the connect operation,
   perform further processing (e.g. signing and encrypting data prior to
   sending it over the connected socket) and then complete the connect
   before the first read or write.  Currently we just use a wrapper that
   performs the two back-to-back as a single operation, so for now it only 
   functions as a timeout-management mechanism - the high-level API for 
   this would be a bit difficult to handle since there's no readily-
   available facility for handling an interruptible sNetConnect(), the best 
   option would be to handle it via a complete-connect IOCTL.  However since
   we've got at least a little time to play with in most cases we could at
   least perform a quick entropy poll in the idle interval, if nothing 
   else */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int preOpenSocket( INOUT NET_STREAM_INFO *netStream, 
						  IN_BUFFER( hostNameLen ) const char *host, 
						  IN_LENGTH_DNS const int hostNameLen,
						  IN_PORT const int port )
	{
	SOCKET netSocket = DUMMY_INIT;
	struct addrinfo *addrInfoPtr, *addrInfoCursor;
	BOOLEAN nonBlockWarning = FALSE;
	int addressCount, status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtr( host, hostNameLen ) );
	
	REQUIRES( hostNameLen > 0 && hostNameLen <= MAX_DNS_SIZE );
	REQUIRES( port >= 22 && port < 65536L );

	/* Clear return value */
	netStream->netSocket = CRYPT_ERROR;

	/* Set up addressing information */
	status = getAddressInfo( netStream, &addrInfoPtr, host, hostNameLen, port, 
							 FALSE );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( addrInfoPtr != NULL );

	/* Create a socket, make it nonblocking, and start the connect to the
	   remote server, falling back through alternative addresses if the
	   connect fails.  Since this is a nonblocking connect it could still
	   fail during the second phase where we can no longer try to recover
	   by falling back to an alternative address, but it's better than just
	   giving up after the first address that we try (this is actually what 
	   happens in some cases under Vista/Win7 which, like most other IPv6-
	   enabled systems preferentially tries to provide an IPv6 address for 
	   "localhost" (see the long comment in openServerSocket()) and allows a 
	   connect() to the IPv6 address, but then returns a WSAETIMEDOUT if the 
	   target application is only listening on an IPv4 address) */
	for( addrInfoCursor = addrInfoPtr, addressCount = 0;
		 addrInfoCursor != NULL && addressCount < IP_ADDR_COUNT;
		 addrInfoCursor = addrInfoCursor->ai_next, addressCount++ )
		{
		status = newSocket( &netSocket, addrInfoCursor, FALSE );
		if( cryptStatusError( status ) )
			continue;
		setSocketNonblocking( netSocket );
		status = connect( netSocket, addrInfoCursor->ai_addr,
						  addrInfoCursor->ai_addrlen );
		nonBlockWarning = isNonblockWarning();
		if( status >= 0 || nonBlockWarning )
			{
			/* We've got a successfully-started connect, exit */
			break;
			}
		deleteSocket( netSocket );
		}
	if( addressCount >= IP_ADDR_COUNT )
		{
		/* We went through a suspiciously large number of remote server 
		   addresses without being able to even initiate a connect attempt 
		   to any of them, there's something wrong */
		DEBUG_DIAG(( "Iterated through %d server addresses without being "
					 "able to connect", IP_ADDR_COUNT ));
		assert( DEBUG_WARN );
		return( mapError( netStream, 0, FALSE, CRYPT_ERROR_OPEN ) );
		}
	freeAddressInfo( addrInfoPtr );
	if( status < 0 && !nonBlockWarning )
		{
		/* There was an error condition other than a notification that the
		   operation hasn't completed yet */
		return( mapError( netStream, getErrorCode(), FALSE, 
						  CRYPT_ERROR_OPEN ) );
		}
	if( status == 0 )
		{
		/* If we're connecting to a local host the connect can complete
		   immediately rather than returning an in-progress status, in
		   which case we don't need to do anything else */
		netStream->netSocket = netSocket;
		return( CRYPT_OK );
		}

	/* The connect is in progress, mark the stream as not-quite-ready for 
	   use */
/*	netStream->xxx = yyy; */
	netStream->netSocket = netSocket;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int completeOpen( INOUT NET_STREAM_INFO *netStream )
	{
	static const int trueValue = 1;
	SIZE_TYPE intLength = sizeof( int );
	int value, status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* Wait around until the connect completes.  Some select()s limit the
	   size of the second count so we set it to a maximum of about a week,
	   although why anyone would wait around that long (and whether any
	   network stack would even maintain a SYN_SENT for that amount of time)
	   is unclear.
	   
	   BeOS doesn't allow setting a timeout (that is, it doesn't allow
	   asynchronous connects), but it hardcodes in a timeout of about a
	   minute so we get a vaguely similar effect */
	status = ioWait( netStream, min( netStream->timeout, 500000L ), FALSE,
					 IOWAIT_CONNECT );
	if( cryptStatusError( status ) )
		{
		netStream->transportDisconnectFunction( netStream, TRUE );
		return( status );
		}

	/* The socket is readable or writeable, however this may be because of 
	   an error (it's readable and writeable) or because everything's OK 
	   (it's writeable) or because everything's OK and there's data waiting 
	   (it's readable and writeable), so we have to see what the error 
	   condition is for the socket to determine what's really happening.

	   How to best determine all of these conditions is a bit tricky.  Other 
	   possibilities include calling recv() with a length of zero bytes 
	   (returns an error if the connect failed), calling connect() again 
	   (fails with EISCONN if the connect succeeded), and calling 
	   getmsg( netSocket, NULL, NULL, &( flags = 0 ) ) (fails with 
	   errno == EAGAIN or EWOULDBLOCK if the only error is that there's 
	   nothing available yet), but these are somewhat implementation-
	   specific and not consistent across different platforms */
	status = getsockopt( netStream->netSocket, SOL_SOCKET, SO_ERROR,
						 ( void * ) &value, &intLength );
	if( status == 0 )
		{
		/* Berkeley-derived implementation, error is in value variable */
		if( value != 0 )
			{
			status = mapError( netStream, value, FALSE, CRYPT_ERROR_OPEN );
			netStream->transportDisconnectFunction( netStream, TRUE );
			return( status );
			}
		}
	else
		{
		/* Slowaris, error is in errno */
		if( isSocketError( status ) )
			{
			int dummy;

			status = getSocketError( netStream, CRYPT_ERROR_OPEN, &dummy );
			netStream->transportDisconnectFunction( netStream, TRUE );
			return( status );
			}
		}

	/* Turn off Nagle (since we do our own optimised TCP handling) and make 
	   the socket blocking again.  This is necessary because with a 
	   nonblocking socket Winsock will occasionally return 0 bytes from 
	   recv() (a sign that the other side has closed the connection, see the 
	   comment in readSocketFunction()) even though the connection is still 
	   fully open, and in any case there's no real need for a nonblocking  
	   socket since we have select() handling timeouts/blocking for us */
	setsockopt( netStream->netSocket, IPPROTO_TCP, TCP_NODELAY,
				( void * ) &trueValue, sizeof( int ) );
	setSocketBlocking( netStream->netSocket );

	/* We've completed the connection, mark the stream as ready for use */
/*	netStream->xxx = zzz; */
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int openServerSocket( INOUT NET_STREAM_INFO *netStream, 
							 IN_BUFFER_OPT( hostNameLen ) const char *host, 
							 IN_LENGTH_DNS const int hostNameLen,
							 IN_PORT const int port )
	{
	SOCKET listenSocket = DUMMY_INIT, netSocket;
	SOCKADDR_STORAGE clientAddr;
	struct addrinfo *addrInfoPtr, *addrInfoCursor;
	static const int trueValue = 1;
	static const int falseValue = 0;
	SIZE_TYPE clientAddrLen = sizeof( SOCKADDR_STORAGE );
	char hostNameBuffer[ MAX_DNS_SIZE + 1 + 8 ];
	int addressCount, errorCode = 0, status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( ( host == NULL && hostNameLen == 0 ) || \
			isReadPtr( host, hostNameLen ) );

	REQUIRES( ( host == NULL && hostNameLen == 0 ) || \
			  ( host != NULL && \
				hostNameLen > 0 && hostNameLen <= MAX_DNS_SIZE ) );
	REQUIRES( port >= 22 && port < 65536L );

	/* Clear return value */
	netStream->netSocket = CRYPT_ERROR;

	/* Convert the host name into the null-terminated string required by the 
	   sockets API if necessary */
	if( host != NULL )
		{
		REQUIRES( hostNameLen > 0 && hostNameLen < MAX_DNS_SIZE );

		memcpy( hostNameBuffer, host, hostNameLen );
		hostNameBuffer[ hostNameLen ] = '\0';
		host = hostNameBuffer;
		}

	/* Set up addressing information.  If we're not binding to a specified 
	   interface we allow connections on any interface.  Note that in 
	   combination with SO_REUSEADDR and old unpatched Unix kernels this 
	   allows port hijacking by another process running on the same machine 
	   that binds to the port with a more specific binding than "any".  It
	   also allows port hijacking under Windows, where the situation is a 
	   bit more complex.  Actually it's representative of the problem of the
	   port-binding situation in general so it's informative to walk through 
	   the issue.  
	   
	   Windows provides a socket option SO_EXCLUSIVEADDRUSE that can be used 
	   to exclude re-binding to a socket.  Unfortunately what this means is
	   that if this option is set for a socket then the port can't be re-
	   used right after the socket is closed but only after the connection 
	   is no longer active, where "active" means that it's not only not in 
	   the ESTABLISHED state but also not in the FIN, FIN_WAIT, FIN_WAIT_2, 
	   or LAST_ACK state.  However the ability to re-bind to the port at 
	   this point is exactly what SO_REUSEADDR is supposed to allow.  In 
	   other words use of SO_EXCLUSIVEADDRUSE is a bit like not setting 
	   SO_REUSEADDR (with a few technical differences based on how 
	   SO_EXCLUSIVEADDRUSE works that aren't important here).  So we have
	   to not only close the socket but wait for the system to send all
	   buffered data, hang around for data acks, send a disconnect to the 
	   remote system, and wait to get a disconnect back.  If the remote 
	   system (or a MITM) advertises a zero-length window or something 
	   similar then the connection can remain "active" (in the sense of 
	   preventing a re-bind, although not necessarily doing anything) more 
	   or less indefinitely.

	   This is a nasty situation because while SO_EXCLUSIVEADDRUSE can
	   prevent local socket-hijacking attacks it opens us up to remote
	   network-based DoS attacks.  In theory if we have complete control
	   of the application we can use a background thread to wait in a recv()
	   loop until all data is read after performing a shutdown( SD_SEND ) 
	   and if necessary alert the user that something funny is going on, but 
	   since we're a library (and sometimes running as a UI-less service) we 
	   can't really do this.

	   Given the choice between allowing a local session-hijack or a remote 
	   DoS, we opt for the session hijack.  Since we're using secured
	   protocols over the socket this isn't nearly as serious as (say) a 
	   socket being used for straight HTTP */
	status = getAddressInfo( netStream, &addrInfoPtr, host, hostNameLen, 
							 port, TRUE );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( addrInfoPtr != NULL );

	/* Create a new server socket, falling back through alternative 
	   interfaces if the initial socket creation fails.  This may seem less 
	   necessary than for the client-side connect but is required because 
	   getaddrinfo() usually preferentially provides an IPv6 interface even 
	   if there's no IPv6 configured for the system (see the long comment in 
	   getAddressInfo() for more on this), so we have to step through until 
	   we get to an IPv4 interface, or at least one that we can listen on.  
	   Qui habet aures audiendi audiat (the speaker appears to be speaking 
	   metaphorically with 'ears' referring to 'network sockets', latin 
	   having no native term for the latter) */
	for( addrInfoCursor = addrInfoPtr, addressCount = 0; 
		 addrInfoCursor != NULL && addressCount < IP_ADDR_COUNT;
		 addrInfoCursor = addrInfoCursor->ai_next, addressCount++ )
		{
		SIZE_TYPE valueLen = sizeof( int );
		int value;

		status = newSocket( &listenSocket, addrInfoCursor, TRUE );
		if( status == CRYPT_OK )
			{
			/* It's a second thread listening on an existing socket,
			   we're done */
			break;
			}
		if( status != OK_SPECIAL )
			{
			/* There was a problem creating the socket, try again with 
			   another interface */
			continue;
			}

		/* At this point we still have the socket pool locked while we 
		   complete initialisation so we need to call newSocketDone()
		   before we break out of the loop at any point */

		/* Now we run into some problems with IPv4/IPv6 dual stacks, see 
		   the long comment about this in io/dns.c, in brief what happens 
		   is that if there's a choice between using IPv4 or IPv6, most 
		   systems will use IPv6 first.  This is typically encountered 
		   through the first entry in the addrInfo list being an IPv6 
		   interface and the second one being an IPv4 interface, which means 
		   that the default first match will be to an IPv6 interface and not 
		   an IPv4 one.  There's an option to listen on both IPv6 and IPv4 
		   interfaces, but whether this is enabled is system-dependent, most 
		   Unix systems enable it but Windows disables it.

		   In order for things to work as expected we check for the use of 
		   IPv6 and, if that's being used, check whether the dual-stack 
		   option is enabled (indicated, somewhat counterintuitively, by 
		   having the IPV6_V6ONLY socket option set to FALSE).  If it's not 
		   enabled then we explicitly enable it for the socket */
		if( addrInfoCursor->ai_family == PF_INET6 && \
			getsockopt( listenSocket, IPPROTO_IPV6, IPV6_V6ONLY,
						( char * ) &value, &valueLen ) == 0 && value == 1 )
			{
			setsockopt( listenSocket, IPPROTO_IPV6, IPV6_V6ONLY,
						( char * ) &falseValue, sizeof( int ) );
			}

		/* This is a new socket, set SO_REUSEADDR to avoid TIME_WAIT 
		   problems and prepare to accept connections (nemo surdior est 
		   quam is qui non audiet).  Note that BeOS can only bind to one 
		   interface at a time, so if we're binding to INADDR_ANY under 
		   BeOS we actually bind to the first interface that we find */
		if( setsockopt( listenSocket, SOL_SOCKET, SO_REUSEADDR,
						( char * ) &trueValue, sizeof( int ) ) || \
			bind( listenSocket, addrInfoCursor->ai_addr,
				  addrInfoCursor->ai_addrlen ) || \
			listen( listenSocket, 5 ) )
			{
			/* Remember the error code now in case there's a later error
			   in the cleanup functions that overwrites it */
			errorCode = getErrorCode();

			/* Clean up so that we can try again, making sure that we have 
			   an appropriate error status set when we continue in case this 
			   was our last iteration through the loop */
			deleteSocket( listenSocket );
			newSocketDone();
			status = CRYPT_ERROR_OPEN;
			continue;
			}

		/* We've finished initialising the socket, tell the socket pool
		   manager that it's safe to let others access the pool */
		newSocketDone();
		status = CRYPT_OK;
		break;
		}
	freeAddressInfo( addrInfoPtr );
	if( addressCount >= IP_ADDR_COUNT )
		{
		/* We went through a suspiciously large number of server addresses 
		   without being able to even initiate a listen attempt on any of 
		   them, there's something wrong */
		DEBUG_DIAG(( "Iterated through %d server addresses without being "
					 "able to listen", IP_ADDR_COUNT ));
		assert( DEBUG_WARN );
		return( mapError( netStream, 0, FALSE, CRYPT_ERROR_OPEN ) );
		}
	if( cryptStatusError( status ) )
		{
		/* There was an error setting up the socket, don't try anything
		   further */
		return( mapError( netStream, 
						  ( errorCode == 0 ) ? getErrorCode() : errorCode, 
						  FALSE, CRYPT_ERROR_OPEN ) );
		}

	/* Wait for a connection.  At the moment this always waits forever
	   (actually some select()s limit the size of the second count so we
	   set it to a maximum of 1 year's worth), but in the future we could
	   have a separate timeout value for accepting incoming connections to
	   mirror the connection-wait timeout for outgoing connections.

	   Because of the way that accept works, the socket that we eventually
	   and up with isn't the one that we listen on, but we have to
	   temporarily make it the one associated with the stream in order for
	   ioWait() to work */
	netStream->netSocket = listenSocket;
	status = ioWait( netStream, min( netStream->timeout, 30000000L ), FALSE,
					 IOWAIT_ACCEPT );
	netStream->netSocket = CRYPT_ERROR;
	if( cryptStatusError( status ) )
		return( status );

	/* We have an incoming connection ready to go, accept it.  There's a
	   potential complication here in that if a client connects and then
	   immediately sends a RST after the TCP handshake has completed,
	   ioWait() will return with an indication that there's an incoming
	   connection ready to go but the following accept(), if it's called
	   after the RST has arrived, will block waiting for the next incoming
	   connection.  This is rather unlikely in practice, but could occur
	   as part of a DoS by setting the SO_LINGER time to 0 and disconnecting
	   immediately.  This has the effect of turning the accept() with
	   timeout into an indefinite-wait accept().

	   To get around this we make the socket temporarily non-blocking, so
	   that accept() returns an error if the client has closed the
	   connection.  The exact error varies, BSD implementations handle the
	   error internally and return to the accept() while SVR4
	   implementations return either EPROTO (older, pre-Posix behaviour) or
	   ECONNABORTED (newer Posix-compliant behaviour, since EPROTO is also
	   used for other protocol-related errors).

	   Since BSD implementations hide the problem they wouldn't normally
	   return an error, however by temporarily making the socket non-
	   blocking we force it to return an EWOULDBLOCK if this situation
	   occurs.  Since this could lead to a misleading returned error, we
	   intercept it and substitute a custom error string.  Note that when
	   we make the listen socket blocking again, we also have to make the
	   newly-created ephemeral socket blocking, since it inherits its
	   attributes from the listen socket */
	setSocketNonblocking( listenSocket );
	netSocket = accept( listenSocket, ( struct sockaddr * ) &clientAddr,
						&clientAddrLen );
	if( isBadSocket( netSocket ) )
		{
		if( isNonblockWarning() )
			{
			status = setSocketError( netStream, 
									 "Remote system closed the connection "
									 "after completing the TCP handshake", 
									 70, CRYPT_ERROR_OPEN, TRUE );
			}
		else
			{
			int dummy;

			status = getSocketError( netStream, CRYPT_ERROR_OPEN, &dummy );
			}
		setSocketBlocking( listenSocket );
		deleteSocket( listenSocket );
		return( status );
		}
	setSocketBlocking( listenSocket );
	setSocketBlocking( netSocket );

	/* Get the IP address of the connected client.  We could get its full
	   name, but this can slow down connections because of the time that it
	   takes to do the lookup and is less authoritative because of potential
	   spoofing.  In any case the caller can still look up the name if they
	   need it.  Since we don't want to abort an entire network connect just 
	   because we can't return the peer's IP address, do don't do anything
	   with the return value of this function */
	( void ) getNameInfo( ( const struct sockaddr * ) &clientAddr, 
						  clientAddrLen, netStream->clientAddress, 
						  CRYPT_MAX_TEXTSIZE / 2, 
						  &netStream->clientAddressLen, 
						  &netStream->clientPort );

	/* We've got a new connection, add the socket to the pool.  Since this
	   was created externally to the pool we don't use newSocket() to create 
	   a new socket but only add the existing socket */
	status = addSocket( netSocket );
	if( cryptStatusError( status ) )
		{
		/* There was a problem adding the new socket, close it and exit.
		   We don't call deleteSocket() since it wasn't added to the pool,
		   instead we call closesocket() directly */
		closesocket( netSocket );
		return( setSocketError( netStream, 
								"Couldn't add socket to socket pool", 34,
								status, FALSE ) );
		}
	netStream->netSocket = netSocket;
	netStream->listenSocket = listenSocket;

	/* Turn off Nagle, since we do our own optimised TCP handling */
	setsockopt( netStream->netSocket, IPPROTO_TCP, TCP_NODELAY,
				( void * ) &trueValue, sizeof( int ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int openSocketFunction( INOUT NET_STREAM_INFO *netStream, 
							   IN_BUFFER_OPT( hostNameLen) const char *hostName, 
							   IN_LENGTH_DNS_Z const int hostNameLen, 
							   IN_PORT const int port )
	{
	int status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( ( hostName == NULL && hostNameLen == 0 ) || \
			isReadPtr( hostName, hostNameLen ) );

	REQUIRES( ( hostName == NULL && hostNameLen == 0 ) || \
			  ( hostName != NULL && \
				hostNameLen > 0 && hostNameLen <= MAX_DNS_SIZE ) );
	REQUIRES( port >= 22 && port < 65536L );
	REQUIRES( ( netStream->nFlags & STREAM_NFLAG_ISSERVER ) || \
			  hostName != NULL );

	/* If it's a server stream, open a listening socket */
	if( netStream->nFlags & STREAM_NFLAG_ISSERVER )
		{
		const int savedTimeout = netStream->timeout;

		/* Timeouts for server sockets are actually three-level rather than
		   the usual two-level model, there's an initial (pre-connect)
		   timeout while we wait for an incoming connection to arrive, and
		   then we go to the usual session connect vs. session read/write
		   timeout mechanism.  To handle the pre-connect phase we set an
		   (effectively infinite) timeout at this point to ensure that the
		   server always waits forever for an incoming connection to
		   appear */
		netStream->timeout = MAX_INTLENGTH;
		status = openServerSocket( netStream, hostName, hostNameLen, port );
		netStream->timeout = savedTimeout;
		return( status );
		}

	ENSURES( hostName != NULL && \
			 ( hostNameLen > 0 && hostNameLen < MAX_INTLENGTH ) );

	/* It's a client stream, perform a two-part nonblocking open.  Currently
	   the two portions are performed back-to-back, in the future we can
	   interleave the two and perform general crypto processing (e.g. hash/
	   MAC context setup for SSL) while the open is completing */
	status = preOpenSocket( netStream, hostName, hostNameLen, port );
	if( cryptStatusOK( status ) )
		status = completeOpen( netStream );
	ENSURES( ( cryptStatusError( status ) && \
			   netStream->netSocket == CRYPT_ERROR ) || \
			 ( cryptStatusOK( status ) && \
			   netStream->netSocket != CRYPT_ERROR ) );
	return( status );
	}

/* Close a connection.  Safely handling closes is extremely difficult due to 
   a combination of the way TCP/IP (and TCP stacks) work and various bugs 
   and quirks in implementations.  After a close (and particularly if short-
   timeout non-blocking writes are used) there can still be data left in 
   TCP send buffers, and also as unacknowledged segments on the network.  At 
   this point there's no easy way for the TCP stack to know how long it 
   should hang around trying to get the data out and waiting for acks to 
   come back.  If it doesn't wait long enough, it'll end up discarding 
   unsent data.  If it waits too long, it could potentially wait forever in 
   the presence of network outages or crashed peers.  What's worse, since 
   the socket is now closed, there's no way to report any problems that may 
   occur at this point back to the caller.

   We try and handle this with a combination of shutdown() and close(), but 
   due to implementation bugs/quirks and the TCP stack issues mentioned 
   above this doesn't work all of the time.  The details get very 
   implementation-specific, for example with glibc the manpage says that 
   setting SO_LINGER causes shutdown() not to return until queued messages 
   are sent (which is wrong, and non-glibc implementations like PHUX and 
   Solaris specifically point out that only close() is affected), but that 
   shutdown() discards unsent data.  glibc in turn is dependent on the 
   kernel it's running on top of, under Linux shutdown() returns immediately 
   but data is still sent regardless of the SO_LINGER setting.

   BSD Net/2 and later (which many stacks are derived from, including non-
   Unix systems like OS/2) returned immediately from a close() but still 
   sent queued data on a best-effort basis.  With SO_LINGER set and a zero 
   timeout the close was abortive (which Linux also implemented starting 
   with the 2.4 kernel), and with a non-zero timeout it would wait until all 
   the data was sent, which meant that it could block almost indefinitely 
   (minutes or even hours, this is the worst-case behaviour mentioned 
   above).  This was finally fixed in 4.4BSD (although a lot of 4.3BSD-
   derived stacks ended up with the indefinite-wait behaviour), but even 
   then there was some confusion as to whether the wait time was in machine-
   specific ticks or seconds (Posix finally declared it to be seconds).  
   Under Winsock, close() simply discards queued data while shutdown() has 
   the same effect as under Linux, sending enqueued data asynchronously 
   regardless of the SO_LINGER setting.

   This is a real mess to sort out safely, the best that we can do is to 
   perform a shutdown() followed later by a close().  Messing with SO_LINGER 
   is too risky and something like performing an ioWait() doesn't work 
   either because it just results in whoever initiated the shutdown being 
   blocked for the I/O wait time, and waiting for a recv() of 0 bytes isn't 
   safe because the higher-level code may need to read back a shutdown ack 
   from the other side which a recv() performed at this point would 
   interfere with.  Under Windows we could handle it by waiting for an 
   FD_CLOSE to be posted but this requires the use of a window handle which 
   we don't have access to, and which may not even exist for some classes of
   applications */

STDC_NONNULL_ARG( ( 1 ) ) \
static void closeSocketFunction( INOUT NET_STREAM_INFO *netStream, 
								 const BOOLEAN fullDisconnect )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* If it's a partial disconnect, close only the send side of the channel.
	   The send-side close can help with ensuring that all data queued for
	   transmission is sent */
	if( !fullDisconnect )
		{
		if( netStream->netSocket != CRYPT_ERROR )
			shutdown( netStream->netSocket, SHUT_WR );
		return;
		}

	/* If it's an open-on-demand HTTP stream then the socket isn't
	   necessarily open even if the stream was successfully connected so we 
	   only close it if necessary.  It's easier handling it at this level
	   than expecting the caller to distinguish between an opened-stream-but-
	   not-opened-socket and a conventional open stream */
	if( netStream->netSocket != CRYPT_ERROR )
		deleteSocket( netStream->netSocket );
	if( netStream->listenSocket != CRYPT_ERROR )
		deleteSocket( netStream->listenSocket );
	netStream->netSocket = netStream->listenSocket = CRYPT_ERROR;
	}

/* Check an externally-supplied socket to make sure that it's set up as
   required by cryptlib.  See the long comment in tcp.h about the numerous
   problems that this theoretically simple operation actually causes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkSocketFunction( INOUT NET_STREAM_INFO *netStream )
	{
	int value;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* Check that we've been passed a valid network socket, and that it's
	   blocking socket */
	getSocketNonblockingStatus( netStream->netSocket, value );
	if( isSocketError( value ) )
		{
		int dummy;

		return( getSocketError( netStream, CRYPT_ARGERROR_NUM1, &dummy ) );
		}
	if( value )
		{
		return( setSocketError( netStream, "Socket is non-blocking", 22,
								CRYPT_ARGERROR_NUM1, TRUE ) );
		}
	return( CRYPT_OK );
	}

/* Read and write data from and to a socket.  Because data can appear in 
   bits and pieces when reading we have to implement timeout handling at two 
   levels, once via ioWait() and a second time as an overall timeout.  If we 
   only used ioWait() this could potentially stretch the overall timeout to 
   (length * timeout) so we also perform a time check that leads to a worst-
   case timeout of (timeout-1 + timeout).  This is the same as the 
   implementation of SO_SND/RCVTIMEO in Berkeley-derived implementations, 
   where the timeout value is actually an interval timer rather than a 
   absolute timer.

   In addition to the standard stream-based timeout behaviour we can also be 
   called with flags specifying explicit blocking behaviour (for a read 
   where we know that we're expecting a certain amount of data) or explicit 
   nonblocking behaviour (for speculative reads to fill a buffer).  These 
   flags are used by the buffered-read routines, which try and speculatively 
   read as much data as possible to avoid the many small reads required by 
   some protocols.  We don't do the blocking read using MSG_WAITALL since 
   this can (potentially) block forever if not all of the data arrives.

   Finally, if we're performing an explicit blocking read (which is usually 
   done when we're expecting a predetermined number of bytes) we dynamically 
   adjust the timeout so that if data is streaming in at a steady rate we 
   don't abort the read just because there's more data to transfer than we 
   can manage in the originally specified timeout interval.  This is 
   especially useful when transferring large data amounts, for which a one-
   size-fits-all fixed timeout doesn't accurately reflect the amount of time 
   required to transfer the full data amount.

   Handling of return values is as follows:

	timeout		byteCount		return
	-------		---------		------
		0			0				0
		0		  > 0			byteCount
	  > 0			0			CRYPT_ERROR_TIMEOUT
	  > 0		  > 0			byteCount

   At the sread()/swrite() level if the partial-read/write flags aren't set
   for the stream, a byteCount < length is also converted to a
   CRYPTO_ERROR_TIMEOUT */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int readSocketFunction( INOUT STREAM *stream, 
							   OUT_BUFFER( maxLength, *length ) BYTE *buffer, 
							   IN_LENGTH const int maxLength, 
							   OUT_LENGTH int *length, 
							   IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;
	MONOTIMER_INFO timerInfo;
	BYTE *bufPtr = buffer;
	const int timeout = ( flags & TRANSPORT_FLAG_NONBLOCKING ) ? 0 : \
						( flags & TRANSPORT_FLAG_BLOCKING ) ? \
						max( 30, netStream->timeout ) : netStream->timeout;
	int bytesToRead, byteCount = 0, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES_S( stream->type == STREAM_TYPE_NETWORK );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_INTLENGTH );
	REQUIRES_S( ( ( flags & TRANSPORT_FLAG_NONBLOCKING ) && \
					timeout == 0 ) || \
				( !( flags & TRANSPORT_FLAG_NONBLOCKING ) && \
					timeout >= 0 && timeout < MAX_INTLENGTH ) );
	REQUIRES_S( flags == TRANSPORT_FLAG_NONE || \
				flags == TRANSPORT_FLAG_NONBLOCKING || \
				flags == TRANSPORT_FLAG_BLOCKING );

	/* Clear return value */
	*length = 0;

	status = setMonoTimer( &timerInfo, timeout );
	if( cryptStatusError( status ) )
		return( status );
	for( bytesToRead = maxLength, iterationCount = 0;
		 bytesToRead > 0 && \
		 ( timeout <= 0 || !checkMonoTimerExpired( &timerInfo ) ) && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 iterationCount++ )
		{
		int bytesRead;

		/* Wait for data to become available */
		status = ioWait( netStream, timeout, 
						 ( byteCount > 0 ) ? TRUE : FALSE, IOWAIT_READ );
		if( status == OK_SPECIAL )
			{
			/* We got a timeout but either there's already data present from 
			   a previous read or it's a nonblocking wait, so this isn't an
			   error */
			if( byteCount > 0 )
				*length = byteCount;
			return( CRYPT_OK );
			}
		if( cryptStatusError( status ) )
			{
			/* Some buggy firewall software will block any data transfer 
			   attempts made after the initial connection setup 
			   (specifically they'll allow the initial SYN/SYN/ACK to 
			   establish connection state but then block any further 
			   information from being transferred), to handle this we
			   check whether we get a timeout on the first read and if we
			   do then we check for the presence of the software firewall,
			   reporting a problem due to the firewall rather than a
			   general networking problem if we find one */
			if( status == CRYPT_ERROR_TIMEOUT && \
				( netStream->nFlags & STREAM_NFLAG_FIRSTREADOK ) )
				{
				return( checkFirewallError( netStream ) );
				}
			return( status );
			}

		/* We've got data waiting, read it */
		bytesRead = recv( netStream->netSocket, bufPtr, bytesToRead, 0 );
		if( isSocketError( bytesRead ) )
			{
			int dummy;

			/* If it's a restartable read due to something like an
			   interrupted system call, retry the read */
			if( isRestartableError() )
				{
				assert( !"Restartable read, recv() indicated error" );
				continue;
				}

			/* There was a problem with the read */
			return( getSocketError( netStream, CRYPT_ERROR_READ, &dummy ) );
			}
		if( bytesRead <= 0 )
			{
			/* Under some odd circumstances (Winsock bugs when using non-
			   blocking sockets, or calling select() with a timeout of 0),
			   recv() can return zero bytes without an EOF condition being
			   present, even though it should return an error status if this
			   happens (this could also happen under very old SysV
			   implementations using O_NDELAY for nonblocking I/O).  To try
			   and catch this we check for a restartable read due to
			   something like an interrupted system call and retry the read
			   if it is.  Unfortunately this doesn't catch the Winsock zero-
			   delay bug but it may catch problems in other implementations.

			   Unfortunately this doesn't work under all circumstances
			   either.  If the connection is genuinely closed select() will
			   return a data-available status and recv() will return zero,
			   both without changing errno.  If the last status set in errno
			   matches the isRestartableError() check, the code will loop
			   forever.  Because of this we can't use the following check,
			   although since it doesn't catch the Winsock zero-delay bug
			   anyway it's probably no big deal.

			   The real culprit here is the design flaw in recv(), which
			   uses a valid bytes-received value to indicate an out-of-band
			   condition that should be reported via an error code ("There's
			   nowt wrong with owt what mitherin clutterbucks don't barley
			   grummit") */
#if 0	/* See above comment */
			if( isRestartableError() )
				{
				assert( !"Restartable read, recv() indicated no error" );
				continue;
				}
#endif /* 0 */

			/* Once this Winsock bug hits, we've fallen and can't get up any
			   more.  WSAGetLastError() reports no error, select() reports
			   data available for reading, and recv() reports zero bytes
			   read.  If the following is used, the code will loop endlessly
			   (or at least until the loop iteration watchdog triggers) 
			   waiting for data that can never be read */
#if 0	/* See above comment */
			getSocketError( stream, CRYPT_ERROR_READ, &dummy );
			status = ioWait( stream, 0, 0, IOWAIT_READ );
			if( cryptStatusOK( status ) )
				continue;
#endif /* 0 */

			/* "It said its piece, and then it sodded off" - Baldrick,
			   Blackadder's Christmas Carol */
			bytesToRead = 0;	/* Force exit from loop */
			continue;
			}
		bufPtr += bytesRead;
		bytesToRead -= bytesRead;
		byteCount += bytesRead;
		ENSURES_S( bytesToRead >= 0 && bytesToRead < MAX_INTLENGTH );
		ENSURES_S( byteCount > 0 && byteCount < MAX_INTLENGTH );

		/* Remember that we've got some data, used for error diagnosis (see
		   the long comment above) */
		netStream->nFlags |= STREAM_NFLAG_FIRSTREADOK;

		/* If this is a blocking read and we've been moving data at a 
		   reasonable rate (~1K/s) and we're about to time out, adjust the 
		   timeout to give us a bit more time.  This is an adaptive process 
		   that grants us more time for the read if data is flowing at 
		   a reasonable rate, but ensures that we don't hang around forever 
		   if data is trickling in at a few bytes a second */
		if( flags & TRANSPORT_FLAG_BLOCKING )
			{
			ENSURES( timeout > 0 );

			/* If the timer expiry is imminent but data is still flowing in, 
			   extend the timing duration to allow for further data to 
			   arrive.  Because of the minimum flow-rate limit that's 
			   imposed above this is unlikely to be subject to much of a DoS 
			   problem (at worst an attacker can limit us to reading data 
			   at 1K/s, which means 16s for SSL/TLS packets and 32s for SSH 
			   packets), but to make things a bit less predictable we dither 
			   the timeout a bit */
			if( ( byteCount / timeout ) >= 1000 && \
				checkMonoTimerExpiryImminent( &timerInfo, 5 ) )
				{
				extendMonoTimer( &timerInfo, 
								 ( getRandomInteger() % 5 ) + 2 );
				}
			}
		}
	ENSURES_S( iterationCount < FAILSAFE_ITERATIONS_MAX );
	if( maxLength > 0 && byteCount <= 0 )
		{
		/* We didn't get anything because the other side closed the
		   connection.  We report this is a read-complete status rather than
		   a read error since it isn't necessarily a real error */
		return( setSocketError( netStream, 
								"No data was read because the remote system "
								"closed the connection (recv() == 0)", 78,
								CRYPT_ERROR_COMPLETE, TRUE ) );
		}
	*length = byteCount;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int writeSocketFunction( INOUT STREAM *stream, 
								IN_BUFFER( length ) const BYTE *buffer, 
								IN_LENGTH const int maxLength, 
								OUT_LENGTH_Z int *length,
								IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;
	MONOTIMER_INFO timerInfo;
	const BYTE *bufPtr = buffer;
	const int timeout = ( flags & TRANSPORT_FLAG_NONBLOCKING ) ? 0 : \
						( flags & TRANSPORT_FLAG_BLOCKING ) ? \
						max( 30, netStream->timeout ) : netStream->timeout;
	int bytesToWrite, byteCount = 0, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES_S( stream->type == STREAM_TYPE_NETWORK );
	REQUIRES_S( maxLength > 0 && maxLength < MAX_INTLENGTH );
	REQUIRES_S( ( ( flags & TRANSPORT_FLAG_NONBLOCKING ) && \
					timeout == 0 ) || \
				( !( flags & TRANSPORT_FLAG_NONBLOCKING ) && \
					timeout >= 0 && timeout < MAX_INTLENGTH ) );
	REQUIRES_S( flags == TRANSPORT_FLAG_NONE || \
				flags == TRANSPORT_FLAG_NONBLOCKING || \
				flags == TRANSPORT_FLAG_BLOCKING );

	/* Clear return value */
	*length = 0;

	/* Send data to the remote system.  As with the receive-data code we 
	   have to work around a large number of quirks and socket 
	   implementation bugs, although most of the systems that exhibited 
	   these are now extinct or close to it.  Some very old Winsock stacks 
	   (Win3.x and early Win95 era) would almost always indicate that a 
	   socket was writeable even when it wasn't.  Even older (mid-1980s) 
	   Berkeley-derived implementations could return EWOULDBLOCK on a 
	   blocking socket if they couldn't get required mbufs so that even if 
	   select() indicated that the socket was writeable, an actual attempt 
	   to write would return an error since there were no mbufs available.  
	   Under Win95 select() can fail to block on a non-blocking socket, so 
	   that the send() returns EWOULDBLOCK.  One possible reason (related to 
	   the mbuf problem) is that another thread may grab memory between the 
	   select() and the send() so that there's no buffer space available 
	   when send() needs it, although this should really return WSAENOBUFS 
	   rather than WSAEWOULDBLOCK.  There's also a known bug in Win95 (and 
	   possibly Win98 as well, Q177346) under which a select() indicates 
	   writeability but send() returns EWOULDBLOCK.  Another select() 
	   executed after the failed send() then causes select() to suddenly 
	   realise that the socket is non-writeable (accidit in puncto, quod 
	   non seperatur in anno).  Finally, in some cases send() can return an 
	   error but WSAGetLastError() indicates that there's no error, so we 
	   treat it as noise and try again */
	status = setMonoTimer( &timerInfo, timeout );
	if( cryptStatusError( status ) )
		return( status );
	for( bytesToWrite = maxLength, iterationCount = 0;
		 bytesToWrite > 0 && \
			( timeout <= 0 || !checkMonoTimerExpired( &timerInfo ) ) && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 iterationCount++ )
		{
		int bytesWritten;

		/* Wait for the socket to become available */
		status = ioWait( netStream, timeout, 
						 ( byteCount > 0 ) ? TRUE : FALSE, IOWAIT_WRITE );
		if( status == OK_SPECIAL )
			{
			/* We got a timeout but either there's already data present from 
			   a previous read or it's a nonblocking wait, so this isn't an
			   error */
			if( byteCount > 0 )
				{
				*length = byteCount;

				return( CRYPT_OK );
				}
			return( CRYPT_OK );
			}
		if( cryptStatusError( status ) )
			return( status );

		/* Write the data */
		bytesWritten = send( netStream->netSocket, bufPtr, bytesToWrite,
							 MSG_NOSIGNAL );
		if( isSocketError( bytesWritten ) )
			{
			int dummy;

			/* If it's a restartable write due to something like an
			   interrupted system call (or a sockets bug), retry the
			   write */
			if( isRestartableError() )
				{
				assert( !"Restartable write, send() indicated error" );
				continue;
				}

#ifdef __WINDOWS__
			/* If it's a Winsock bug, treat it as a restartable write */
			if( WSAGetLastError() < WSABASEERR )
				{
				assert( !"send() failed but WSAGetLastError() indicated no "
						"error, ignoring" );
				continue;
				}
#endif /* __WINDOWS__ */

			/* There was a problem with the write */
			return( getSocketError( netStream, CRYPT_ERROR_WRITE, &dummy ) );
			}
		bufPtr += bytesWritten;
		bytesToWrite -= bytesWritten;
		byteCount += bytesWritten;
		ENSURES_S( bytesToWrite >= 0 && bytesToWrite < MAX_INTLENGTH );
		ENSURES_S( byteCount > 0 && byteCount < MAX_INTLENGTH );
		}
	ENSURES_S( iterationCount < FAILSAFE_ITERATIONS_MAX );
	*length = byteCount;

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void setAccessMethodTCP( INOUT NET_STREAM_INFO *netStream )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* Set the access method pointers */
	netStream->transportConnectFunction = openSocketFunction;
	netStream->transportDisconnectFunction = closeSocketFunction;
	netStream->transportReadFunction = readSocketFunction;
	netStream->transportWriteFunction = writeSocketFunction;
	netStream->transportOKFunction = transportOKFunction;
	netStream->transportCheckFunction = checkSocketFunction;
	}
#endif /* USE_TCP */
