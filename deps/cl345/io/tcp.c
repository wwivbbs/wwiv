/****************************************************************************
*																			*
*						cryptlib TCP/IP Interface Routines					*
*						Copyright Peter Gutmann 1998-2016					*
*																			*
****************************************************************************/

#include <ctype.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "stream_int.h"
  #include "tcp.h"
  #include "tcp_int.h"
#else
  #include "crypt.h"
  #include "io/stream_int.h"
  #include "io/tcp.h"
  #include "io/tcp_int.h"
#endif /* Compiler-specific includes */

#ifdef USE_TCP

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Check an externally-supplied socket to make sure that it's set up as
   required by cryptlib.  See the long comment in tcp.h about the numerous
   problems that this theoretically simple operation actually causes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkSocketFunction( INOUT NET_STREAM_INFO *netStream )
	{
	int value;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );

	/* Check that we've been passed a valid network socket, and that it's a
	   blocking socket.  getSocketNonblockingStatus() is a complex macro
	   that tries to return the non-blocking status as a boolean */
	getSocketNonblockingStatus( netStream->netSocket, value );
	if( value )
		{
		return( setSocketError( netStream, "Socket is non-blocking", 22,
								CRYPT_ARGERROR_NUM1, TRUE ) );
		}
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 		Init/Shutdown Routines						*
*																			*
****************************************************************************/

/* Initialise and shut down the network interface */

RETVAL \
int netInitTCP( void )
	{
#ifdef __WINDOWS__
	WSADATA wsaData;
#endif /* __WINDOWS__ */

#ifdef __WINDOWS__
	if( WSAStartup( 2, &wsaData ) != 0 )
		{
		DEBUG_DIAG(( "Couldn't initialise Winsock" ));
		return( CRYPT_ERROR );
		}
#endif /* __WINDOWS__ */

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
#ifdef __WINDOWS__
	/* Wipe the Sheets Afterwards and Cleanup */
	WSACleanup();
#endif /* __WINDOWS__ */

#ifdef __SCO_VERSION__
	signal( SIGIO, SIG_DFL );
#endif /* UnixWare/SCO */
	}

/* Return the status of the network interface */

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

/****************************************************************************
*																			*
*						 System-Specific Problem Workarounds				*
*																			*
****************************************************************************/

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
	   minute for the connect and then times out, so if we get a wait on a
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

#ifdef __embOS__

/* embOS doesn't have any equivalent to errno, it's necessary to explicitly 
   fetch the error code from getsockopt() */

static int lastErrno = 0;

int getErrno( SOCKET socket )
	{
	int errno, status;

	/* If there's no socket available to get the error code from, return a 
	   generic error */
	if( socket == INVALID_SOCKET )
		return( IP_ERR_MISC );

	/* Try and get the last socket error.  If this fails, we return the 
	   generic IP_ERR_MISC */
	status = IP_SOCK_getsockopt( socket, SOL_SOCKET, SO_ERROR, &errno, 
								 sizeof( int ) );
	if( status != 0 )
		return( IP_ERR_MISC );

	/* Reading the SO_ERROR value clears it, so if we read a zero value we 
	   return the last read error value, to mimic the behaviour of the Unix 
	   errno */
	if( errno == 0 )
		return( lastErrno );

	lastErrno = errno;
	return( errno );
	}

void clearErrno( void )
	{
	lastErrno = 0;
	}

/* embOS/IP gets getsockopt() wrong so we have to define our own version 
   that overrides the embOS one.  First we change getsockopt() back to its
   original define, IP_SOCK_getsockopt(), and then we call it via the 
   wrapper */

#undef getsockopt
#define getsockopt				IP_SOCK_getsockopt

int my_getsockopt( int sockfd, int level, int optname, void *optval, 
				   int *optlen )
	{
	return( getsockopt( sockfd, level, optname, optval, *optlen ) );
	}

/* embOS/IP doesn't have inet_ntoa() or inet_addr() so we have to provide 
   our own */

char *inet_ntoa( const struct in_addr in )
	{
	static char buffer[ 32 + 8 ];
	
	IP_PrintIPAddr( buffer, in.s_addr, 32 );
	return( buffer );
	}

unsigned long inet_addr( const char *cp )
	{
	U32 address;
	int status;

	/* It's unclear whether IP_ResolveHost() will convert dotted-decimal 
	   strings directly to addresses, for now we use this unless someone
	   complains, in order to avoid having to implement a complete
	   inet_addr() ourselves */
	status = IP_ResolveHost( cp, &address, 10000 );
	if( status != 0 )
		return( INADDR_NONE );
	return( address );
	}
#endif /* __embOS__ */

#ifdef __MQXRTOS__

/* MQX has a select() that looks mostly like a normal select() but isn't,
   this wrapper maps the standard select() to the MQX version */

static int my_select( int socket_range, rtcs_fd_set *read_bits,
					  rtcs_fd_set *write_bits,
					  rtcs_fd_set *exception_bits,
					  struct timeval *timeout )
	{
	uint32_t timeout_ms;

	/* Turn the seconds : microseconds timeout value into a millisecond 
	   value */
	timeout_ms = ( timeout->tv_sec * 1000 ) * ( timeout->tv_usec / 1000 );

	/* The rounding from microseconds to milliseconds can leave the timeout
	   set to zero, which is bad for MQX sinze a value of zero means wait
	   indefinitely.  If we get a zero timeout we make it one */
	if( timeout_ms == 0 )
		timeout_ms = 1;

	/* Pass the call down to the native select() */
	return( select( socket_range, read_bits, write_bits, exception_bits, 
					timeout_ms ) );
	}

#define select		my_select	/* Replace call to select() with wrapper */

/* MQX uses a nonstandard form of send(), to deal with the resulting 
   compiler warning we map the parameter to the appropriate type */ 

#define send( socket, buffer, count, flags ) \
		send( socket, ( char * ) buffer, count, flags )

#endif /* __MQXRTOS__ */

STDC_NONNULL_ARG( ( 1 ) ) \
void setAccessMethodTCP( INOUT NET_STREAM_INFO *netStream )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* Set the access method pointers */
	FNPTR_SET( netStream->transportOKFunction, transportOKFunction );
	FNPTR_SET( netStream->transportCheckFunction, checkSocketFunction );
	setAccessMethodTCPConnect( netStream );
	setAccessMethodTCPReadWrite( netStream );
	}
#endif /* USE_TCP */
