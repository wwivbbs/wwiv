/****************************************************************************
*																			*
*						cryptlib TCP/IP Connection Routines					*
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

/* Disable assorted socket handbrakes.  In theory these calls can fail, but 
   there's not much that we can do about it, and in any case things will 
   keep working anyway, so we don't try and handle any errors for this 
   situation */

static void disableNagle( const SOCKET netSocket )
	{
	static const int trueValue = 1;

	( void ) setsockopt( netSocket, DISABLE_NAGLE_LEVEL, 
						 DISABLE_NAGLE_OPTION, ( void * ) &trueValue, 
						 sizeof( int ) );
	}

#ifdef TCP_QUICKACK

static void disableSlowACK( const SOCKET netSocket )
	{
	static const int trueValue = 1;

	( void ) setsockopt( netSocket, IPPROTO_TCP, TCP_QUICKACK,
						 ( void * ) &trueValue, sizeof( int ) );
	}
#else
  #define disableSlowACK( netSocket )
#endif /* TCP_QUICKACK */

#ifdef __MQXRTOS__

/* MQX uses nonstandard forms of accept(), bind(), and connect().  To deal 
   with the resulting compiler warnings we map the parameters to the 
   appropriate types */ 

#define bind( socket, address, address_len ) \
		bind( socket, address, ( uint16_t ) address_len ) 
#define connect( socket, address, address_len ) \
		connect( socket, address, ( uint16_t ) address_len )

static int my_accept( int socket, struct sockaddr *address,
					  int *address_len )
	{
	uint16_t length;
	int retVal;

	/* Pass the call down to the accept() function, mapping the length 
	   parameter to the correct type */
	retVal = accept( socket, address, &length );
	*address_len = length;

	return( retVal );
	}

#define accept		my_accept	/* Replace call to accept() with wrapper */

/* MQX doesn't support SO_REUSEADDR so we provide a wrapper that maps it to
   a no-op */

static int32_t my_setsockopt( uint32_t socket, uint32_t level, 
							  uint32_t option_name, 
							  const void *option_value, 
							  socklen_t option_len )
	{
	/* No-op out SO_REUSEADDR */
	if( option_name == SO_REUSEADDR )
		return( RTCS_OK );

	return( setsockopt( socket, level, option_name, option_value, 
						option_len ) );
	}

#define setsockopt	my_setsockopt	/* Replace call to setsockopt() with wrapper */

#endif /* __MQXRTOS__ */

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
   this whether you want to or not).
   
   A second problem occurs when a thread is blocked in an object waiting on 
   a socket because there's no way to unblock it apart from killing the 
   thread.  In theory we could create some sort of loopback socket and wait 
   for it alongside the listen socket in the pre-accept select wait, 
   signalling a shutdown by closing the loopback socket, but this starts to 
   get ugly.  In order to work around this we maintain a socket pool that 
   serves two functions:

	- Maintains a list of sockets that an object is listening on to allow a
	  listening socket to be reused rather than having to listen on a
	  socket and close it as soon as an incoming connection is made in
	  order to switch to the connected socket.

	- Allows sockets to be closed from another thread, which results in any
	  objects waiting on them being woken up and exiting.

   For now we limit the socket pool to a maximum of SOCKETPOOL_SIZE sockets 
   both as a safety feature to protect against runaway use of sockets in the 
   calling application and because cryptlib was never designed to function 
   as a high-volume server application.  If necessary this can be changed to 
   dynamically expand the socket pool.  
   
   However it's not a good idea to simply remove the restriction entirely, 
   or set it to too high a value, because this can cause problems with 
   excess consumption of kernel resources.  For example under Windows 
   opening several tens of thousands of connections will eventually return 
   WSAENOBUFS when the nonpaged pool is exhausted.  At this point things 
   start to get problematic because many drivers don't handle the inability 
   to allocate memory very well, and can start to fail and render the whole 
   system unstable.  This is a general resource-consumption problem that 
   affects all users of the shared nonpaged pool, but we can at least make 
   sure that we're not the cause of any crashes by limiting our own 
   consumption */

static const SOCKET_INFO SOCKET_INFO_TEMPLATE = \
				{ INVALID_SOCKET, 0, 0, { 0 } };

/* Initialise the socket pool */

CHECK_RETVAL \
int initSocketPool( void )
	{
	SOCKET_INFO *socketInfo = getSocketPoolStorage();
	int i, LOOP_ITERATOR;

	/* Clear the socket pool */
	LOOP_LARGE( i = 0, i < SOCKETPOOL_SIZE, i++ )
		socketInfo[ i ] = SOCKET_INFO_TEMPLATE;
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
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
	SOCKET_INFO *socketInfo = getSocketPoolStorage();
	SOCKET netSocket;
	BYTE addrHash[ ADDRHASH_SIZE + 8 ];
	int addrChecksum DUMMY_INIT, i, status, LOOP_ITERATOR;

	assert( isWritePtr( newSocketPtr, sizeof( SOCKET ) ) );
	assert( isReadPtr( addrInfoPtr, sizeof( struct addrinfo ) ) );

	REQUIRES( isServer == TRUE || isServer == FALSE );

	/* Clear return value */
	*newSocketPtr = INVALID_SOCKET;

	/* Perform any required pre-calculations before we acquire the mutex */
	if( isServer )
		{
		BYTE hashBuffer[ 16 + 8 ];

		addrChecksum = checksumData( addrInfoPtr->ai_addr, 
									 addrInfoPtr->ai_addrlen );
		hashData( hashBuffer, 16, addrInfoPtr->ai_addr, 
								  addrInfoPtr->ai_addrlen );
		memcpy( addrHash, hashBuffer, ADDRHASH_SIZE );
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
		LOOP_LARGE( i = 0, i < SOCKETPOOL_SIZE, i++ )
			{
			if( socketInfo[ i ].refCount > 0 && \
				socketInfo[ i ].addrChecksum == addrChecksum && \
				!memcmp( socketInfo[ i ].addrHash, addrHash, ADDRHASH_SIZE ) )
				{
				if( socketInfo[ i ].refCount >= 10000 )
					{
					krnlExitMutex( MUTEX_SOCKETPOOL );
					DEBUG_DIAG(( "Socket %d in socket pool has a reference "
								 "count > 10,000", i ));
					assert( DEBUG_WARN );
					return( CRYPT_ERROR_OVERFLOW );
					}
				ENSURES_KRNLMUTEX( socketInfo[ i ].refCount > 0 && \
								   socketInfo[ i ].refCount < 10000, 
								   MUTEX_SOCKETPOOL );
				ENSURES_KRNLMUTEX( !isBadSocket( socketInfo[ i ].netSocket ), 
								   MUTEX_SOCKETPOOL );
				socketInfo[ i ].refCount++;
				*newSocketPtr = socketInfo[ i ].netSocket;
				krnlExitMutex( MUTEX_SOCKETPOOL );

				/* The socket already exists, don't perform any further
				   initialisation with it */
				return( CRYPT_OK );
				}
			}
		ENSURES_KRNLMUTEX( LOOP_BOUND_OK, MUTEX_SOCKETPOOL );
		}

	/* Create a new socket entry */
	LOOP_LARGE( i = 0, i < SOCKETPOOL_SIZE, i++ )
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
	ENSURES_KRNLMUTEX( LOOP_BOUND_OK, MUTEX_SOCKETPOOL );
	if( i >= SOCKETPOOL_SIZE )
		{
		krnlExitMutex( MUTEX_SOCKETPOOL );
		DEBUG_DIAG(( "Tried to add more than %d sockets to socket pool", 
					 SOCKETPOOL_SIZE ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_OVERFLOW );
		}
	netSocket = socket( addrInfoPtr->ai_family,
						addrInfoPtr->ai_socktype, 0 );
	if( isBadSocket( netSocket ) )
		{
		krnlExitMutex( MUTEX_SOCKETPOOL );
		return( CRYPT_ERROR_OPEN );
		}
	socketInfo[ i ] = SOCKET_INFO_TEMPLATE;
	socketInfo[ i ].netSocket = netSocket;
	socketInfo[ i ].refCount = 1;
	if( isServer )
		{
		/* Remember the details for this socket so that we can detect another
		   attempt to bind to it */
		socketInfo[ i ].addrChecksum = addrChecksum;
		memcpy( socketInfo[ i ].addrHash, addrHash, ADDRHASH_SIZE );
		}
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
	SOCKET_INFO *socketInfo = getSocketPoolStorage();
	int i, status, LOOP_ITERATOR;

	REQUIRES( !isBadSocket( netSocket ) );

	status = krnlEnterMutex( MUTEX_SOCKETPOOL );
	if( cryptStatusError( status ) )
		return( status );

	/* Add an existing socket entry */
	LOOP_LARGE( i = 0, i < SOCKETPOOL_SIZE, i++ )
		{
		if( isBadSocket( socketInfo[ i ].netSocket ) )
			break;
		}
	ENSURES_KRNLMUTEX( LOOP_BOUND_OK, MUTEX_SOCKETPOOL );
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
	SOCKET_INFO *socketInfo = getSocketPoolStorage();
	int i, status, LOOP_ITERATOR;

	REQUIRES_V( !isBadSocket( netSocket ) );

	status = krnlEnterMutex( MUTEX_SOCKETPOOL );
	if( cryptStatusError( status ) )
		return;

	/* Find the entry for this socket in the pool.  There may not be one
	   present if the pool has received a shutdown signal and closed all
	   network sockets, so if we don't find it we just exit normally */
	LOOP_LARGE( i = 0, i < SOCKETPOOL_SIZE, i++ )
		{
		if( socketInfo[ i ].netSocket == netSocket )
			break;
		}
	ENSURES_KRNLMUTEX_V( LOOP_BOUND_OK, MUTEX_SOCKETPOOL );
	if( i >= SOCKETPOOL_SIZE )
		{
		DEBUG_DIAG(( "Socket to delete not present in socket pool, exiting "
					 "without further action" ));
		krnlExitMutex( MUTEX_SOCKETPOOL );
		return;
		}
	REQUIRES_KRNLMUTEX_V( socketInfo[ i ].refCount > 0, MUTEX_SOCKETPOOL );

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
			socketInfo[ i ].addrChecksum = 0;
			memset( socketInfo[ i ].addrHash, 0, ADDRHASH_SIZE );

			DEBUG_DIAG(( "Couldn't close socket pool socket %d", i ));
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
   unlock a thread blocking in accept()) returns ENOTCONN from the shutdown()
   call rather than shutting down the socket, so an actual shutdown requires 
   setting up a dummy connection to the socket to be shut down before it can 
   be shut down.  Trying to shut down a thread blocked in connect() is more 
   or less impossible under Slowaris 5.x.  Other systems are more flexible, 
   but there's not enough consistency to rely on this */

void netSignalShutdown( void )
	{
	SOCKET_INFO *socketInfo = getSocketPoolStorage();
	int i, status, LOOP_ITERATOR;

	/* Exactly what to do if we can't acquire the mutex is a bit complicated
	   because at this point our primary goal is to force all objects to exit 
	   rather than worrying about socket-pool consistency.  On the other
	   hand if another object is currently in the middle of cleaning up and
	   is holding the socket pool mutex then we don't want to stomp on it 
	   while it's doing its cleanup.  Since failing to acquire the mutex is 
	   a special-case exception condition, it's not even possible to plan for
	   this since it's uncertain under which conditions (if ever) this 
	   situation would occur.  For now we play it safe and don't do anything 
	   if we can't acquire the mutex, which is at least consistent */
	status = krnlEnterMutex( MUTEX_SOCKETPOOL );
	if( cryptStatusError( status ) )
		retIntError_Void();

	/* For each open socket, close it and clear its pool entry */
	LOOP_LARGE( i = 0, i < SOCKETPOOL_SIZE, i++ )
		{
		if( !isBadSocket( socketInfo[ i ].netSocket ) )
			{
			closesocket( socketInfo[ i ].netSocket );
			socketInfo[ i ] = SOCKET_INFO_TEMPLATE;
			}
		}
	ENSURES_KRNLMUTEX_V( LOOP_BOUND_OK, MUTEX_SOCKETPOOL );

	krnlExitMutex( MUTEX_SOCKETPOOL );
	}

/****************************************************************************
*																			*
*							Open a Client Socket							*
*																			*
****************************************************************************/

/* Open a connection to a remote server.  The connection-open function 
   performs that most amazing of all things, the nonblocking connect.  This 
   is currently done in order to allow a shorter timeout than the default 
   fortnight or so but it also allows for two-phase connects in which we 
   start the connect operation, perform further processing (e.g. signing and 
   encrypting data prior to sending it over the connected socket) and then 
   complete the connect before the first read or write.  
   
   Currently we just use a wrapper that performs the two back-to-back as a 
   single operation, so for now it only functions as a timeout-management 
   mechanism - the high-level API for this would be a bit difficult to 
   handle since there's no readily-available facility for handling an 
   interruptible sNetConnect(), the best option would be to handle it via a 
   complete-connect IOCTL.  However since we've got at least a little time 
   to play with in most cases we could perhaps perform a quick entropy poll 
   in the idle interval, if nothing else */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int preOpenSocket( INOUT NET_STREAM_INFO *netStream, 
						  IN_BUFFER( hostNameLen ) const char *host, 
						  IN_LENGTH_DNS const int hostNameLen,
						  IN_PORT const int port )
	{
	SOCKET netSocket DUMMY_INIT;
	struct addrinfo *addrInfoPtr, *addrInfoCursor;
	const BOOLEAN isDgramSocket = \
			TEST_FLAG( netStream->nFlags, STREAM_NFLAG_DGRAM ) ? TRUE : FALSE;
	BOOLEAN nonBlockWarning = FALSE;
	int addressCount, status, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtrDynamic( host, hostNameLen ) );
	
	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( hostNameLen > 0 && hostNameLen <= MAX_DNS_SIZE );
	REQUIRES( port >= MIN_PORT_NUMBER && port < MAX_PORT_NUMBER );

	/* Clear return value */
	netStream->netSocket = INVALID_SOCKET;

	/* Set up addressing information */
	status = getAddressInfo( netStream, &addrInfoPtr, host, hostNameLen, port, 
							 FALSE, isDgramSocket );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( addrInfoPtr != NULL );

	/* Create a socket, make it nonblocking, and start the connect to the
	   remote server, falling back through alternative addresses if the
	   connect fails.  Since this is a nonblocking connect it could still
	   fail during the second phase where we can no longer try to recover
	   by falling back to an alternative address, but it's better than just
	   giving up after the first address that we try.
	   
	   Unfortunately, the fail-on-first-address behaviour is actually what 
	   happens in some cases under Vista/Win7 which, like most other IPv6-
	   enabled systems preferentially tries to provide an IPv6 address for 
	   "localhost" (see the long comment in openServerSocket()) and allows a 
	   connect() to the IPv6 address, but then returns a WSAETIMEDOUT if the 
	   target application is only listening on an IPv4 address.  The code in
	   openServerSocket() has a workaround for this, unwinding the fantasy-
	   world IPv6-by-default back to the real-world IPv4-by-default */
	LOOP_SMALL( ( addrInfoCursor = addrInfoPtr, addressCount = 0 ),
				addrInfoCursor != NULL && addressCount < IP_ADDR_COUNT,
				( addrInfoCursor = addrInfoCursor->ai_next, addressCount++ ) )
		{
		/* If it's not an IPv4 or IPv6 address, continue */
		if( !allowedAddressFamily( addrInfoCursor->ai_family ) )
			continue;

		/* Create a socket and start the connect process */
		status = newSocket( &netSocket, addrInfoCursor, FALSE );
		if( cryptStatusError( status ) )
			continue;
		setSocketNonblocking( netSocket );
		clearErrorState();
		status = connect( netSocket, addrInfoCursor->ai_addr,
						  addrInfoCursor->ai_addrlen );
		nonBlockWarning = isNonblockWarning( netSocket );
		if( status >= 0 || nonBlockWarning )
			{
			/* We've got a successfully-started connect, exit */
			break;
			}
		deleteSocket( netSocket );
		}
	ENSURES( LOOP_BOUND_OK );
	if( addressCount >= IP_ADDR_COUNT )
		{
		/* We went through a suspiciously large number of remote server 
		   addresses without being able to even initiate a connect attempt 
		   to any of them, there's something wrong */
		DEBUG_DIAG(( "Iterated through %d server addresses without being "
					 "able to connect", addressCount ));
		assert( DEBUG_WARN );
		return( mapNetworkError( netStream, 0, FALSE, CRYPT_ERROR_OPEN ) );
		}
	freeAddressInfo( addrInfoPtr );
	if( status < 0 && !nonBlockWarning )
		{
		/* There was an error condition other than a notification that the
		   operation hasn't completed yet */
		return( mapNetworkError( netStream, getErrorCode( INVALID_SOCKET ), 
								 FALSE, CRYPT_ERROR_OPEN ) );
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
	STM_TRANSPORTDISCONNECT_FUNCTION transportDisconnectFunction;
	SIZE_TYPE intLength = sizeof( int );
	int value, status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );

	transportDisconnectFunction = ( STM_TRANSPORTDISCONNECT_FUNCTION ) \
						FNPTR_GET( netStream->transportDisconnectFunction );
	REQUIRES( transportDisconnectFunction != NULL );

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
		transportDisconnectFunction( netStream, TRUE );
		return( status );
		}

	/* The socket is readable or writeable, however this may be because of 
	   an error (it's readable and writeable) or because everything's OK 
	   (it's writeable) or because everything's OK and there's data waiting 
	   (it's readable and writeable again), so we have to see what the error 
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
		/* Berkeley-derived implementation, error is in the value variable */
		if( value != 0 )
			{
			status = mapNetworkError( netStream, value, FALSE, 
									  CRYPT_ERROR_OPEN );
			transportDisconnectFunction( netStream, TRUE );
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
			transportDisconnectFunction( netStream, TRUE );
			return( status );
			}
		}

	/* Turn off Nagle if it's a TCP socket (since we do our own optimised 
	   TCP handling) and make the socket blocking again.  This is necessary 
	   because with a nonblocking socket Winsock will occasionally return 0 
	   bytes from recv() (a sign that the other side has closed the 
	   connection, see the comment in readSocketFunction()) even though the 
	   connection is still fully open, and in any case there's no real need 
	   for a nonblocking socket since we have select() handling timeouts/
	   blocking for us.
	   
	   In theory these calls can fail, but there's not much that we can do 
	   about it, and in any case things will usually keep working anyway, so
	   we don't try and handle any errors for this situation */
	if( !TEST_FLAG( netStream->nFlags, STREAM_NFLAG_DGRAM ) )
		disableNagle( netStream->netSocket );
	setSocketBlocking( netStream->netSocket );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Open a Server Socket							*
*																			*
****************************************************************************/

/* Wait for a connection from a remote client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int openServerSocket( INOUT NET_STREAM_INFO *netStream, 
							 IN_BUFFER_OPT( hostNameLen ) const char *host, 
							 IN_LENGTH_DNS_Z const int hostNameLen,
							 IN_PORT const int port )
	{
	SOCKET listenSocket DUMMY_INIT, netSocket;
	SOCKADDR_STORAGE clientAddr;
	struct addrinfo *addrInfoPtr, *addrInfoCursor;
	static const int trueValue = 1;
	static const int falseValue = 0;
	const BOOLEAN isDgramSocket = \
			TEST_FLAG( netStream->nFlags, STREAM_NFLAG_DGRAM ) ? TRUE : FALSE;
	SIZE_TYPE clientAddrLen = sizeof( SOCKADDR_STORAGE );
	char hostNameBuffer[ MAX_DNS_SIZE + 1 + 8 ];
	int addressCount, errorCode = 0, status, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( ( host == NULL && hostNameLen == 0 ) || \
			isReadPtrDynamic( host, hostNameLen ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( ( host == NULL && hostNameLen == 0 ) || \
			  ( host != NULL && \
				hostNameLen > 0 && hostNameLen <= MAX_DNS_SIZE ) );
	REQUIRES( port >= MIN_PORT_NUMBER && port < MAX_PORT_NUMBER );

	/* Clear return value */
	netStream->netSocket = INVALID_SOCKET;

	/* Convert the host name into the null-terminated string required by the 
	   sockets API if necessary */
	if( host != NULL )
		{
		REQUIRES( rangeCheck( hostNameLen, 1, MAX_DNS_SIZE ) );
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
	   SO_REUSEADDR, with a few technical differences based on how 
	   SO_EXCLUSIVEADDRUSE works that aren't important here.  
	   
	   So now we have to not only close the socket but wait for the system 
	   to send all buffered data, hang around for data acks, send a 
	   disconnect to the remote system, and wait to get a disconnect back.  
	   If the remote system (or a MITM) advertises a zero-length window or 
	   something similar then the connection can remain "active" (in the 
	   sense of preventing a re-bind, although not necessarily doing 
	   anything) more or less indefinitely.

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
							 port, TRUE, isDgramSocket );
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
	LOOP_SMALL( ( addrInfoCursor = addrInfoPtr, addressCount = 0 ), 
				addrInfoCursor != NULL && addressCount < IP_ADDR_COUNT,
				( addrInfoCursor = addrInfoCursor->ai_next, addressCount++ ) )
		{
#ifdef USE_IPv6
		SIZE_TYPE valueLen = sizeof( int );
		int value;
#endif /* USE_IPv6 */

		/* If it's not an IPv4 or IPv6 address, continue */
		if( !allowedAddressFamily( addrInfoCursor->ai_family ) )
			continue;

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
		   the long comment about this in io/dns.c.  In brief what happens 
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
		   option is enabled.  This is indicated by having the IPV6_V6ONLY 
		   socket option set to FALSE, if it's not enabled (so IPV6_V6ONLY 
		   is set to TRUE, disabling IPv4) then we explicitly enable it for 
		   the socket */
#ifdef USE_IPv6
		if( addrInfoCursor->ai_family == AF_INET6 && \
			getsockopt( listenSocket, IPPROTO_IPV6, IPV6_V6ONLY,
						( char * ) &value, &valueLen ) == 0 && value == 1 )
			{
			setsockopt( listenSocket, IPPROTO_IPV6, IPV6_V6ONLY,
						( char * ) &falseValue, sizeof( int ) );
			}
#endif /* USE_IPv6 */

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
			errorCode = getErrorCode( listenSocket );

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
	ENSURES( LOOP_BOUND_OK );
	freeAddressInfo( addrInfoPtr );
	if( addressCount >= IP_ADDR_COUNT )
		{
		/* We went through a suspiciously large number of server addresses 
		   without being able to even initiate a listen attempt on any of 
		   them, there's something wrong */
		DEBUG_DIAG(( "Iterated through %d server addresses without being "
					 "able to listen", addressCount ));
		assert( DEBUG_WARN );
		return( mapNetworkError( netStream, 0, FALSE, CRYPT_ERROR_OPEN ) );
		}
	if( cryptStatusError( status ) )
		{
		/* There was an error setting up the socket, don't try anything
		   further */
		return( mapNetworkError( netStream, 
								 ( errorCode == 0 ) ? \
										getErrorCode( INVALID_SOCKET ) : \
										errorCode, 
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
	netStream->netSocket = INVALID_SOCKET;
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
	   attributes from the listen socket.
	   
	   In addition to all of the blocking/nonblocking shenanigans, we also 
	   need to disable Nagle on the accepted socket.  This may or may not
	   be necessary depending on the sockets implementation, we always
	   explicitly set it to be on the safe side */
	setSocketNonblocking( listenSocket );
	clearErrorState();
	netSocket = accept( listenSocket, ( struct sockaddr * ) &clientAddr,
						&clientAddrLen );
	if( isBadSocket( netSocket ) )
		{
		if( isNonblockWarning( listenSocket ) )
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

	/* Turn off Nagle, since we do our own optimised TCP handling.  In 
	   theory this call can fail, but there's not much that we can do about 
	   it, and in any case things will usually keep working anyway, so we 
	   don't try and handle any errors for this situation */
	disableNagle( netSocket );

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

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Open/Close Sockets								*
*																			*
****************************************************************************/

/* Open/close a network socket */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int openSocketFunction( INOUT NET_STREAM_INFO *netStream, 
							   IN_BUFFER_OPT( hostNameLen) const char *hostName, 
							   IN_LENGTH_DNS_Z const int hostNameLen, 
							   IN_PORT const int port )
	{
	int status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( ( hostName == NULL && hostNameLen == 0 ) || \
			isReadPtrDynamic( hostName, hostNameLen ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( ( hostName == NULL && hostNameLen == 0 ) || \
			  ( hostName != NULL && \
				hostNameLen > 0 && hostNameLen <= MAX_DNS_SIZE ) );
	REQUIRES( port >= MIN_PORT_NUMBER && port < MAX_PORT_NUMBER );
	REQUIRES( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_ISSERVER ) || \
			  hostName != NULL );

	/* If it's a server stream, open a listening socket */
	if( TEST_FLAG( netStream->nFlags, STREAM_NFLAG_ISSERVER ) )
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
			 ( hostNameLen > 0 && hostNameLen <= MAX_DNS_SIZE ) );

	/* It's a client stream, perform a two-part nonblocking open.  Currently
	   the two portions are performed back-to-back, in the future we can
	   interleave the two and perform general crypto processing like hash/
	   MAC context setup for SSL or SSH while the open is completing */
	status = preOpenSocket( netStream, hostName, hostNameLen, port );
	if( cryptStatusOK( status ) )
		status = completeOpen( netStream );
	ENSURES( ( cryptStatusError( status ) && \
			   netStream->netSocket == INVALID_SOCKET ) || \
			 ( cryptStatusOK( status ) && \
			   netStream->netSocket != INVALID_SOCKET ) );
	if( cryptStatusError( status ) )
		{
		/* There was a problem opening the socket, see if we can return 
		   something a bit better than the often rather generic socket-
		   connect error code */
		return( diagnoseConnectionProblem( netStream, hostName, 
										   hostNameLen, status ) );
		}

	return( CRYPT_OK );
	}

/* Close a connection.  Safely handling closes is extremely difficult due to 
   a combination of the way TCP/IP (and TCP stacks) work and various bugs 
   and quirks in implementations.  After a close (and particularly if short-
   timeout non-blocking writes are used) there can still be data left in 
   TCP send buffers, and also as unacknowledged segments on the network.  At 
   this point there's no easy way for the TCP stack to know how long it 
   should hang around trying to get the data out and wait for ACKs to come 
   back.  If it doesn't wait long enough, it'll end up discarding unsent 
   data.  If it waits too long, it could potentially wait forever in the 
   presence of network outages or crashed peers.  What's worse, since the 
   socket is now closed, there's no way to report any problems that may 
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

	REQUIRES_V( sanityCheckNetStream( netStream ) );
	REQUIRES_V( fullDisconnect == TRUE || fullDisconnect == FALSE );

	/* If it's a partial disconnect, close only the send side of the channel.
	   The send-side close can help with ensuring that all data queued for
	   transmission is sent */
	if( !fullDisconnect )
		{
		if( netStream->netSocket != INVALID_SOCKET )
			shutdown( netStream->netSocket, SHUT_WR );
		return;
		}

	/* If it's an open-on-demand HTTP stream then the socket isn't
	   necessarily open even if the stream was successfully connected so we 
	   only close it if necessary.  It's easier handling it at this level
	   than expecting the caller to distinguish between an opened-stream-but-
	   not-opened-socket and a conventional open stream */
	if( netStream->netSocket != INVALID_SOCKET )
		deleteSocket( netStream->netSocket );
	if( netStream->listenSocket != INVALID_SOCKET )
		deleteSocket( netStream->listenSocket );
	netStream->netSocket = netStream->listenSocket = INVALID_SOCKET;
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void setAccessMethodTCPConnect( INOUT NET_STREAM_INFO *netStream )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* Set the access method pointers */
	FNPTR_SET( netStream->transportConnectFunction, openSocketFunction );
	FNPTR_SET( netStream->transportDisconnectFunction, closeSocketFunction );
	}
#endif /* USE_TCP */
