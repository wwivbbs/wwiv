/****************************************************************************
*																			*
*						cryptlib DNS Interface Routines						*
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

#ifdef __WINDOWS__

/* Global function pointers.  These are necessary because the functions need
   to be dynamically linked since not all systems contain the necessary
   libraries */

#if ( defined( sun ) && OSVERSION > 4 )
#undef htonl		/* Slowaris has defines that conflict with our ones */
#undef htons
#undef ntohl
#undef ntohs
#endif /* Slowaris */

#ifndef TEXT
  #define TEXT		/* Win32 windows.h defines this, but not the Win16 one */
#endif /* TEXT */

#if defined( _MSC_VER ) && defined( _PREFAST_ ) 
  #define OUT_STRING_OPT	__out_bcount_z
#else
  #define OUT_STRING_OPT( size )
#endif /* Additional markups for Win32 API functions */

typedef void ( SOCKET_API *FREEADDRINFO )( INOUT struct addrinfo *ai ) \
						STDC_NONNULL_ARG( ( 1 ) );
typedef CHECK_RETVAL int ( SOCKET_API *GETADDRINFO )\
						( IN_STRING const char *nodename, 
						  IN_STRING const char *servname, 
						  const struct addrinfo *hints,
						  OUT_PTR struct addrinfo **res );
typedef CHECK_RETVAL struct hostent FAR * ( SOCKET_API *GETHOSTBYNAME )\
						( IN_STRING const char FAR *name ) \
						STDC_NONNULL_ARG( ( 1 ) );
typedef CHECK_RETVAL int ( SOCKET_API *GETNAMEINFO )\
						( IN_BUFFER( salen ) const struct sockaddr *sa, 
						  IN SIZE_TYPE salen,
						  OUT_BUFFER_FIXED( nodelen ) char *node, 
						  IN SIZE_TYPE nodelen,
						  OUT_STRING_OPT( servicelen ) char *service, 
						  IN SIZE_TYPE servicelen, int flags ) \
						STDC_NONNULL_ARG( ( 1, 3, 5 ) );
typedef CHECK_RETVAL u_long ( SOCKET_API *HTONL )( u_long hostlong );
typedef CHECK_RETVAL u_short ( SOCKET_API *HTONS )( u_short hostshort );
typedef CHECK_RETVAL unsigned long ( SOCKET_API *INET_ADDR )\
						( IN_STRING const char FAR *cp ) \
						STDC_NONNULL_ARG( ( 1 ) );
typedef CHECK_RETVAL char FAR * ( SOCKET_API *INET_NTOA )( struct in_addr in );
typedef CHECK_RETVAL u_long ( SOCKET_API *NTOHL )( u_long netlong );
typedef CHECK_RETVAL u_short ( SOCKET_API *NTOHS )( u_short netshort );
typedef int ( SOCKET_API *WSAGETLASTERROR )( void );

static FREEADDRINFO pfreeaddrinfo = NULL;
static GETADDRINFO pgetaddrinfo = NULL;
static GETHOSTBYNAME pgethostbyname = NULL;
static GETNAMEINFO pgetnameinfo = NULL;
static HTONL phtonl = NULL;
static HTONS phtons = NULL;
static INET_ADDR pinet_addr = NULL;
static INET_NTOA pinet_ntoa = NULL;
static NTOHL pntohl = NULL;
static NTOHS pntohs = NULL;
static WSAGETLASTERROR pWSAGetLastError = NULL;

#define freeaddrinfo		pfreeaddrinfo
#define getaddrinfo			pgetaddrinfo
#define gethostbyname		pgethostbyname
#define getnameinfo			pgetnameinfo
#define htonl				phtonl
#define htons				phtons
#define inet_addr			pinet_addr
#define inet_ntoa			pinet_ntoa
#define ntohl				pntohl
#define ntohs				pntohs
#ifndef WSAGetLastError
  /* In some environments WSAGetLastError() is a macro that maps to
     GetLastError() */
  #define WSAGetLastError	pWSAGetLastError
  #define DYNLOAD_WSAGETLASTERROR
#endif /* WSAGetLastError */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int SOCKET_API my_getaddrinfo( IN_STRING const char *nodename,
									  IN_STRING const char *servname,
									  const struct addrinfo *hints,
									  OUT_PTR struct addrinfo **res );
STDC_NONNULL_ARG( ( 1 ) ) \
static void SOCKET_API my_freeaddrinfo( IN struct addrinfo *ai );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int SOCKET_API my_getnameinfo( IN_BUFFER( salen ) \
									  const struct sockaddr *sa,
									  IN SIZE_TYPE salen, 
									  OUT_BUFFER_FIXED( nodelen ) char *node, 
									  IN SIZE_TYPE nodelen, 
									  OUT_BUFFER_FIXED( servicelen ) \
									  char *service, 
									  IN SIZE_TYPE servicelen, IN int flags );

CHECK_RETVAL \
int initDNS( const INSTANCE_HANDLE hTCP, const INSTANCE_HANDLE hAddr )
	{
	/* Get the required TCP/IP functions */
	gethostbyname = ( GETHOSTBYNAME ) DynamicBind( hTCP, TEXT( "gethostbyname" ) );
	htonl = ( HTONL ) DynamicBind( hTCP, TEXT( "htonl" ) );
	htons = ( HTONS ) DynamicBind( hTCP, TEXT( "htons" ) );
	inet_addr = ( INET_ADDR ) DynamicBind( hTCP, TEXT( "inet_addr" ) );
	inet_ntoa = ( INET_NTOA ) DynamicBind( hTCP, TEXT( "inet_ntoa" ) );
	ntohl = ( NTOHL ) DynamicBind( hTCP, TEXT( "ntohl" ) );
	ntohs = ( NTOHS ) DynamicBind( hTCP, TEXT( "ntohs" ) );
  #ifdef DYNLOAD_WSAGETLASTERROR
	WSAGetLastError = ( WSAGETLASTERROR ) DynamicBind( hTCP, TEXT( "WSAGetLastError" ) );
  #endif /* DYNLOAD_WSAGETLASTERROR */
	if( gethostbyname == NULL || htonl == NULL || htons == NULL || \
		inet_addr == NULL || inet_ntoa == NULL || ntohl == NULL || \
		ntohs == NULL )
		return( CRYPT_ERROR );

	/* Set up the IPv6-style name/address functions */
	if( hAddr != NULL_INSTANCE )
		{
		freeaddrinfo = ( FREEADDRINFO ) DynamicBind( hAddr, TEXT( "freeaddrinfo" ) );
		getaddrinfo = ( GETADDRINFO ) DynamicBind( hAddr, TEXT( "getaddrinfo" ) );
		getnameinfo = ( GETNAMEINFO ) DynamicBind( hAddr, TEXT( "getnameinfo" ) );
		if( freeaddrinfo == NULL || getaddrinfo == NULL || \
			getnameinfo == NULL )
			return( CRYPT_ERROR );
		}
	else
		{
		/* If we couldn't dynamically bind the IPv6 name/address functions,
		   use a local emulation */
		getaddrinfo = my_getaddrinfo;
		freeaddrinfo = my_freeaddrinfo;
		getnameinfo = my_getnameinfo;
		}

	/* If the DNS SRV init fails it's not a serious problem, we can continue 
	   without it */
	( void ) initDNSSRV( hTCP );

	return( CRYPT_OK );
	}

void endDNS( const INSTANCE_HANDLE hTCP )
	{
	endDNSSRV( hTCP );
	}
#endif /* __WINDOWS__ */

/****************************************************************************
*																			*
*						 		IPv6 Emulation								*
*																			*
****************************************************************************/

/* Emulation of IPv6 networking functions.  We include these unconditionally
   under Windows because with dynamic binding we can't be sure whether 
   they're needed or not */

#if !defined( IPv6 ) || defined( __WINDOWS__ )

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
static int addAddrInfo( INOUT_OPT struct addrinfo *prevAddrInfoPtr,
						OUT_PTR struct addrinfo **addrInfoPtrPtr,
						IN_BUFFER( addrLen ) const void *address, 
						IN_RANGE( IP_ADDR_SIZE, IP_ADDR_SIZE ) \
						const int addrLen,  IN_PORT const int port )
	{
	struct addrinfo *addrInfoPtr;
	struct sockaddr_in *sockAddrPtr;

	assert( prevAddrInfoPtr == NULL || \
			isWritePtr( prevAddrInfoPtr, sizeof( struct addrinfo ) ) );
	assert( isWritePtr( addrInfoPtrPtr, sizeof( struct addrinfo * ) ) );
	assert( isReadPtr( address, addrLen ) );
	
	REQUIRES( addrLen == IP_ADDR_SIZE );
	REQUIRES( port >= 22 && port < 65536L );

	/* Clear return value */
	*addrInfoPtrPtr = NULL;

	/* Allocate the new element, clear it, and set fixed fields for IPv4 */
	addrInfoPtr = clAlloc( "addAddrInfo", sizeof( struct addrinfo ) );
	sockAddrPtr = clAlloc( "addAddrInfo", sizeof( struct sockaddr_in ) );
	if( addrInfoPtr == NULL || sockAddrPtr == NULL )
		{
		if( addrInfoPtr != NULL )
			clFree( "addAddrInfo", addrInfoPtr );
		if( sockAddrPtr != NULL )
			clFree( "addAddrInfo", sockAddrPtr );
		return( -1 );
		}
	memset( addrInfoPtr, 0, sizeof( struct addrinfo ) );
	memset( sockAddrPtr, 0, sizeof( struct sockaddr_in ) );
	if( prevAddrInfoPtr != NULL )
		prevAddrInfoPtr->ai_next = addrInfoPtr;
	addrInfoPtr->ai_family = PF_INET;
	addrInfoPtr->ai_socktype = SOCK_STREAM;
	addrInfoPtr->ai_protocol = IPPROTO_TCP;
	addrInfoPtr->ai_addrlen = sizeof( struct sockaddr_in );
	addrInfoPtr->ai_addr = ( struct sockaddr * ) sockAddrPtr;

	/* Set the port and address information.  In general we'd copy the
	   address to the sockAddrPtr->sin_addr.s_addr member, however on
	   Crays, which don't have 32-bit data types, this is a 32-bit
	   bitfield, so we have to use the encapsulating struct */
	sockAddrPtr->sin_family = AF_INET;
	sockAddrPtr->sin_port = htons( ( in_port_t ) port );
	memcpy( &sockAddrPtr->sin_addr, address, addrLen );
	*addrInfoPtrPtr = addrInfoPtr;
	return( 0 );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int SOCKET_API my_getaddrinfo( IN_STRING const char *nodename,
									  IN_STRING const char *servname,
									  const struct addrinfo *hints,
									  OUT_PTR struct addrinfo **res )
	{
	struct hostent *pHostent;
	struct addrinfo *currentAddrInfoPtr = NULL;
	int port, hostErrno, i, status;
	gethostbyname_vars();

	assert( isReadPtr( hints, sizeof( struct addrinfo ) ) );
	assert( isWritePtr( res, sizeof( struct addrinfo * ) ) );

	static_assert( sizeof( in_addr_t ) == IP_ADDR_SIZE, \
				   "in_addr_t size" );

	REQUIRES( nodename != NULL || ( hints->ai_flags & AI_PASSIVE ) );
	REQUIRES( servname != NULL );

	/* Clear return value */
	*res = NULL;

	/* Perform basic error checking.  Since this is supposed to be an 
	   emulation of a (normally) built-in function we don't perform any 
	   REQUIRES()-style checking but only apply the basic checks that the 
	   normal built-in form does */
	if( ( nodename == NULL && !( hints->ai_flags & AI_PASSIVE ) ) || \
		servname == NULL || hints == NULL || res == NULL )
		return( -1 );

	/* Convert the text-string port number into a numeric value */
	status = strGetNumeric( servname, strlen( servname ), &port, 1, 65535 );
	if( cryptStatusError( status ) )
		return( - 1 );

	/* If there's no interface specified and we're creating a server-side
	   socket, prepare to listen on any interface.  Note that BeOS can only
	   bind to one interface at a time, so INADDR_ANY actually binds to the
	   first interface that it finds */
	if( nodename == NULL && ( hints->ai_flags & AI_PASSIVE ) )
		{
		const in_addr_t address = INADDR_ANY;

		return( addAddrInfo( NULL, res, &address, sizeof( in_addr_t  ), 
							 port ) );
		}

	/* If it's a dotted address, there's a single address, convert it to
	   in_addr form and return it.  Note for EBCDIC use that since this is
	   an emulation of an OS function the string is already in EBCDIC form,
	   so we don't use the cryptlib-internal functions for this */
	if( isdigit( *nodename ) )
		{
		const in_addr_t address = inet_addr( nodename );

		if( isBadAddress( address ) )
			return( -1 );
		return( addAddrInfo( NULL, res, &address, sizeof( in_addr_t  ), 
							 port ) );
		}

	/* It's a host name, convert it to the in_addr form */
	gethostbyname_threadsafe( nodename, pHostent, hostErrno );
	if( pHostent == NULL ) 
		return( -1 );
	ENSURES( pHostent->h_length == IP_ADDR_SIZE );
	for( i = 0; i < IP_ADDR_COUNT && \
				pHostent->h_addr_list[ i ] != NULL; i++ )
		{
		int status;

		if( currentAddrInfoPtr == NULL )
			{
			status = addAddrInfo( NULL, res, pHostent->h_addr_list[ i ], 
								  pHostent->h_length, port );
			currentAddrInfoPtr = *res;
			}
		else
			status = addAddrInfo( currentAddrInfoPtr, &currentAddrInfoPtr,
								  pHostent->h_addr_list[ i ], 
								  pHostent->h_length, port );
		if( status != 0 )
			{
			freeaddrinfo( *res );
			return( status );
			}
		}
	return( 0 );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void SOCKET_API my_freeaddrinfo( INOUT struct addrinfo *ai )
	{
	int i;

	assert( isWritePtr( ai, sizeof( struct addrinfo ) ) );

	/* Perform basic error checking.  Since this is supposed to be an 
	   emulation of a (normally) built-in function we don't perform any 
	   REQUIRES()-style checking but only apply the basic checks that the 
	   normal built-in form does */
	if( ai == NULL )
		return;

	for( i = 0; ai != NULL && i < IP_ADDR_COUNT; i++ )
		{
		struct addrinfo *addrInfoCursor = ai;

		ai = ai->ai_next;
		if( addrInfoCursor->ai_addr != NULL )
			clFree( "my_freeaddrinfo", addrInfoCursor->ai_addr );
		clFree( "my_freeaddrinfo", addrInfoCursor );
		}
	}
									  
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int SOCKET_API my_getnameinfo( IN_BUFFER( salen ) \
											const struct sockaddr *sa, 
									  IN SIZE_TYPE salen,
									  OUT_BUFFER_FIXED( nodelen ) char *node, 
									  IN_LENGTH_SHORT SIZE_TYPE nodelen,
									  OUT_BUFFER_FIXED( servicelen ) \
											char *service, 
									  IN_LENGTH_SHORT SIZE_TYPE servicelen,
									  IN int flags )
	{
	const struct sockaddr_in *sockAddr = ( struct sockaddr_in * ) sa;
	const char *ipAddress;

	assert( isReadPtr( sa, salen ) && salen >= sizeof( struct sockaddr_in ) );
	assert( isReadPtr( node, nodelen ) && nodelen >= 10 );
	assert( isReadPtr( service, servicelen ) && servicelen >= 8 );

	/* Perform basic error checking.  Since this is supposed to be an 
	   emulation of a (normally) built-in function we don't perform any 
	   REQUIRES()-style checking but only apply the basic checks that the 
	   normal built-in form does */
	if( sa == NULL || \
		salen < sizeof( struct sockaddr_in ) || \
		salen > MAX_INTLENGTH_SHORT || \
		node == NULL || \
		nodelen < 10 || nodelen > MAX_INTLENGTH_SHORT || \
		service == NULL || \
		servicelen < 8 || servicelen > MAX_INTLENGTH_SHORT )
		return( -1 );

	/* Clear return values */
	strlcpy_s( node, nodelen, "<Unknown>" );
	strlcpy_s( service, servicelen, "0" );

	/* Get the remote system's address and port number */
	if( ( ipAddress = inet_ntoa( sockAddr->sin_addr ) ) == NULL )
		return( -1 );
	memcpy( node, ipAddress, nodelen );
	node[ nodelen - 1 ] = '\0';
	if( sprintf_s( service, servicelen, "%d",
				   ntohs( sockAddr->sin_port ) ) < 0 )
		return( -1 );

	return( 0 );
	}
#endif /* !IPv6 || __WINDOWS__ */

/****************************************************************************
*																			*
*						 			DNS Interface							*
*																			*
****************************************************************************/

/* Get a host's IP address */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAddressInfo( INOUT NET_STREAM_INFO *netStream, 
					OUT_PTR struct addrinfo **addrInfoPtrPtr,
					IN_BUFFER_OPT( nameLen ) const char *name, 
					IN_LENGTH_Z const int nameLen, 
					IN_PORT const int port, const BOOLEAN isServer )
	{
	struct addrinfo hints;
	char nameBuffer[ MAX_DNS_SIZE + 8 ], portBuffer[ 16 + 8 ];

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtr( addrInfoPtrPtr, sizeof( struct addrinfo * ) ) );
	assert( isServer || name != NULL );

	REQUIRES( port >= 22 && port < 65536L );
	REQUIRES( isServer || \
			  ( !isServer && name != NULL ) );
	REQUIRES( ( name == NULL && nameLen == 0 ) || \
			  ( name != NULL && \
				nameLen > 0 && nameLen < MAX_DNS_SIZE ) );

	/* Convert the name and port into the null-terminated text-string format 
	   required by getaddrinfo().  The reason why the port is given as a 
	   string rather than a port number is that we can also optionally 
	   specify the port to connect to via a service name.  Of course it's 
	   more or less pot luck whether the service you want is a recognised 
	   one so everyone specifies the port anyway, however the reason why 
	   this unnecessary flexibility is there is because getaddrinfo() was 
	   seen as a universal replacement for a pile of other functions, 
	   including (for this case) getservbyname() */
	if( name != NULL )
		{
		memcpy( nameBuffer, name, nameLen );
		nameBuffer[ nameLen ] = '\0';
		name = nameBuffer;
		}
	sprintf_s( portBuffer, 8, "%d", port );

	/* If we're a client and using auto-detection of a PKI service, try and
	   locate it via DNS SRV */
	if( !isServer && name != NULL && nameLen == 12 && \
		( !memcmp( name, "[Autodetect]", 12 ) || *name == '_' ) )
		{
		int localPort, status;

		status = findHostInfo( netStream, nameBuffer, MAX_DNS_SIZE, 
							   &localPort, name, nameLen );
		if( cryptStatusError( status ) )
			return( status );
		name = nameBuffer;
		sprintf_s( portBuffer, 8, "%d", localPort );
		}

	/* Convert the address information to the system character set if 
	   required */
#ifdef EBCDIC_CHARS
	if( name != NULL )
		bufferToEbcdic( nameBuffer, nameBuffer );
	bufferToEbcdic( portBuffer, portBuffer );
#endif /* EBCDIC_CHARS */

	/* Set up the port information and hint information needed by
	   getaddrinfo().  The use of PF_UNSPEC is a bit problematic because RFC
	   2553 is usually interpreted to mean "look for all addresses" rather
	   than the more sensible "look for any address".  The reason why this
	   is a problem is because getaddrinfo() ends up looking for unnecessary
	   IPv6 addresses, either by returning IPv6 addresses when the system
	   doesn't do IPv6 or spending a lot of time groping around for IPv6
	   stuff and/or further unnecessary addresses when it's already got what
	   it needs.  This is made worse by confusion over implementation
	   details, for example early implementations of getaddrinfo() in glibc
	   would always try an AAAA lookup even on an IPv4-only system/network,
	   resulting in long delays as the resolver timed out and fell back to a
	   straight A lookup.  There was some disagreement over whether this was
	   right or wrong, and how to fix it (IPv6 purists who never noticed the
	   problem seemed to think that it was right, everyone else thought that
	   it was wrong).  Variations of this problem exist, e.g. if an IPv4
	   address is in /etc/hosts and DNS is down, the resolver will still
	   spend ages (several minutes in some cases) groping around for an IPv6
	   address before it finally gives up and falls back to what it already
	   knows from /etc/hosts.  Switching the hint from AF_UNSPEC to AF_INET
	   bypasses this problem, but has the downside of disabling IPv6 use.

	   This problem was partially fixed post-RFC 2553 by adding the
	   AI_ADDRCONFIG flag, which tells getaddrinfo() to only do AAAA queries
	   if the system has at least one IPv6 source address configured, and
	   the same for A and IPv4 (in other words it applies some common sense,
	   which is how it should have behaved in the first place).
	   Unfortunately this flag isn't very widely supported yet, so it usually
	   ends up being no-op'd out by the auto-config.
	   
	   In addition under Windows Vista/Windows 7 there are always IPv6 
	   addresses in use by a bunch of Windows services that simultaneously 
	   listen on IPv4 and IPv6 addresses, so that things regress back to the 
	   pre-AI_ADDRCONFIG behaviour, although there seems to be some 
	   optimisation in place to prevent the DNS-lookup issues from coming 
	   back as well.  Unfortunately this also means that an attempt to look 
	   up an address like "localhost" for a server-side socket will 
	   preferentially return an IPv6 address [::1] rather than an IPv4 
	   address 127.0.0.1, which works if the client also gets a IPv6 address 
	   but not if it's expecting the more usual IPv4 one.

	   Bounds Checker 6.x may crash in the getaddrinfo() call if maximum 
	   checking is enabled.  To fix this, set the checking level to normal 
	   rather than maximum */
	memset( &hints, 0, sizeof( struct addrinfo ) );
	hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG;
	if( isServer )
		{
		/* If it's a server, set the AI_PASSIVE flag so that if the
		   interface that we're binding to isn't explicitly specified we get
		   any interface */
		hints.ai_flags |= AI_PASSIVE;
		}
#if 1
	hints.ai_family = PF_UNSPEC;
#else
	BOOLEAN forceIPv4 = FALSE;
	int preferIPV4;

	status = krnlSendMessage( certInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &preferIPV4,
							  CRYPT_OPTION_PREFERIPV4 );
	if( cryptStatusOK( status ) && preferIPV4 )
		{
		/* Override any potential defaulting to IPv6 by the local system 
		   to force the use of IPv4, which is going to be far more probable 
		   than IPv6 for the foreseeable future */
		hints.ai_family = PF_INET;
		forceIPv4 = TRUE;
		}
	else
		{
		/* Just use whatever the local system gives us */
		hints.ai_family = PF_UNSPEC;
		}
#endif /* 1 */
	hints.ai_socktype = SOCK_STREAM;
	if( getaddrinfo( name, portBuffer, &hints, addrInfoPtrPtr ) )
		{
#if 0
		if( !forceIPv4 )
			return( getHostError( netStream, CRYPT_ERROR_OPEN ) );

		/* We overrode the protocol-type selection, which may have been the 
		   cause of the failure, try again with whatever we can get rather 
		   than forcing IPv4 */
		hints.ai_family = PF_UNSPEC;
		if( getaddrinfo( name, portBuffer, &hints, addrInfoPtrPtr ) )
			return( getHostError( netStream, CRYPT_ERROR_OPEN ) );
#else
		return( getHostError( netStream, CRYPT_ERROR_OPEN ) );
#endif /* 0 */
		}
	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void freeAddressInfo( struct addrinfo *addrInfoPtr )
	{
	assert( isWritePtr( addrInfoPtr, sizeof( struct addrinfo ) ) );

	freeaddrinfo( addrInfoPtr );
	}

STDC_NONNULL_ARG( ( 1, 3, 5, 6 ) ) \
void getNameInfo( IN_BUFFER( sockAddrLen ) const void *sockAddr,
				  IN_LENGTH_SHORT_MIN( 8 ) const int sockAddrLen,
				  OUT_BUFFER( addressMaxLen, *addressLen ) char *address, 
				  IN_LENGTH_DNS const int addressMaxLen, 
				  OUT_LENGTH_DNS_Z int *addressLen, 
				  OUT_PORT_Z int *port )
	{
	char nameBuffer[ MAX_DNS_SIZE + 8 ];
	char portBuffer[ 32 + 8 ];
	int nameLength, portLength, localPort, status;

	assert( isReadPtr( sockAddr, sockAddrLen ) );
	assert( isWritePtr( address, addressMaxLen ) );
	assert( isWritePtr( port, sizeof( int ) ) );

	REQUIRES_V( sockAddrLen >= 8 && sockAddrLen < MAX_INTLENGTH_SHORT );
	REQUIRES_V( addressMaxLen >= CRYPT_MAX_TEXTSIZE / 2 && \
				addressMaxLen <= MAX_DNS_SIZE );

	/* Clear return values */
	memcpy( address, "<Unknown>", 9 );
	*addressLen = 9;
	*port = 0;

	/* Some Windows implementations of getnameinfo() call down to
	   getservbyport() assuming that it will always succeed and therefore
	   leave the port/service argument unchanged when it doesn't, so the 
	   following call must be made with the NI_NUMERICSERV flag specified 
	   (which it would be anyway, cryptlib always treats the port as a 
	   numeric arg).  Oddly enough the macro version of this function in 
	   wspiapi.h used for IPv4-only situations does get it correct */
	if( getnameinfo( sockAddr, sockAddrLen, nameBuffer, MAX_DNS_SIZE, 
					 portBuffer, 32, NI_NUMERICHOST | NI_NUMERICSERV ) != 0 )
		{
		DEBUG_DIAG(( "Couldn't get host name for socket" ));
		assert( DEBUG_WARN );
		return;
		}
	nameLength = strlen( nameBuffer );
	portLength = strlen( portBuffer );
	if( nameLength <= 0 || nameLength > addressMaxLen || \
		portLength <= 0 || portLength > 8 )
		{
		DEBUG_DIAG(( "Returned host name data is invalid" ));
		assert( DEBUG_WARN );
		return;
		}
#ifdef EBCDIC_CHARS
	ebcdicToAscii( nameBuffer, nameBuffer, nameLength );
	ebcdicToAscii( portBuffer, portBuffer, portLength );
#endif /* EBCDIC_CHARS */
	memcpy( address, nameBuffer, nameLength );
	*addressLen = nameLength;
	status = strGetNumeric( portBuffer, portLength, &localPort, 1, 65536 );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Returned host port is invalid" ));
		assert( DEBUG_WARN );
		return;
		}
	*port = localPort;
	}
#endif /* USE_TCP */
