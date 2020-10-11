/****************************************************************************
*																			*
*						cryptlib DNS Interface Routines						*
*						Copyright Peter Gutmann 1998-2014					*
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
*						 		Utility Functions							*
*																			*
****************************************************************************/

/* Return extended error information for a DNS error */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getAddrInfoError( INOUT NET_STREAM_INFO *netStream, 
							 const int errorCode,
							 IN_ERROR const int status )
	{
#ifdef USE_ERRMSGS
  #if defined( __WINDOWS__ )
	BYTE errorStringBuffer[ 1024 + 8 ];
	int errorStringLen;
  #elif defined( USE_IPv6 )
	const char *errorString = gai_strerror( errorCode );
	const int errorStringLen = strlen( errorString );
  #endif /* System-specific error string handling */

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( cryptStatusError( status ) );

	/* Get the text string describing the error that occurred */
  #ifdef __WINDOWS__
	errorStringLen = FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | \
									 FORMAT_MESSAGE_IGNORE_INSERTS | \
									 FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL,
									 errorCode,
									 MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
									 errorStringBuffer, 1024, NULL );
	if( errorStringLen <= 0 )
		{
		memcpy( errorStringBuffer, "<<<Unknown>>>", 13 );
		errorStringLen = 13;
		}
	setErrorString( NETSTREAM_ERRINFO, errorStringBuffer, errorStringLen );
  #elif defined( USE_IPv6 )
	setErrorString( NETSTREAM_ERRINFO, errorString, errorStringLen );
  #else
	mapNetworkError( netStream, errorCode, TRUE, status );
  #endif /* __WINDOWS__ */
#endif /* USE_ERRMSGS */

	/* Make the error status fatal and exit */
	netStream->persistentStatus = status;
	return( status );
	}

/****************************************************************************
*																			*
*						 		IPv6 API Emulation							*
*																			*
****************************************************************************/

/* Emulation of IPv6 DNS lookup functions */

#if !defined( USE_IPv6_DNSAPI )

#define getaddrinfo		my_getaddrinfo
#define freeaddrinfo	my_freeaddrinfo
#define getnameinfo		my_getnameinfo

#ifdef EBCDIC_CHARS
  /* Since we're ASCII internally but the strings passed to these emulated 
     system functions are EBCDIC, we can't do comparisons for character
	 literals since they'll be the ASCII form, not the EBCDIC form.  To
	 deal with this, we explicitly give the EBCDIC character code for 
	 character values that we check for */
  #define DOTTED_DELIMITER		0x4B
#else
  #define DOTTED_DELIMITER		'.'
#endif /* EBCDIC_CHARS */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
static int addAddrInfo( INOUT_OPT struct addrinfo *prevAddrInfoPtr,
						OUT_PTR_COND struct addrinfo **addrInfoPtrPtr,
						IN_BUFFER( addrLen ) const void *address, 
						IN_RANGE( IP_ADDR_SIZE, IP_ADDR_SIZE ) \
							const int addrLen, 
						IN_PORT const int port,
						const int socketType )
	{
	struct addrinfo *addrInfoPtr;
	struct sockaddr_in *sockAddrPtr;

	assert( prevAddrInfoPtr == NULL || \
			isWritePtr( prevAddrInfoPtr, sizeof( struct addrinfo ) ) );
	assert( isWritePtr( addrInfoPtrPtr, sizeof( struct addrinfo * ) ) );
	assert( isReadPtrDynamic( address, addrLen ) );
	
	REQUIRES( addrLen == IP_ADDR_SIZE );
	REQUIRES( port >= MIN_PORT_NUMBER && port < MAX_PORT_NUMBER );

	/* Clear return value */
	*addrInfoPtrPtr = NULL;

	/* Allocate the new element, clear it, and set fixed fields for IPv4 */
	addrInfoPtr = clAlloc( "addAddrInfo", sizeof( struct addrinfo ) );
	if( addrInfoPtr == NULL )
		return( OUT_OF_MEMORY_ERROR );
	sockAddrPtr = clAlloc( "addAddrInfo", sizeof( struct sockaddr_in ) );
	if( sockAddrPtr == NULL )
		{
		clFree( "addAddrInfo", addrInfoPtr );
		return( OUT_OF_MEMORY_ERROR );
		}
	memset( addrInfoPtr, 0, sizeof( struct addrinfo ) );
	memset( sockAddrPtr, 0, sizeof( struct sockaddr_in ) );
	if( prevAddrInfoPtr != NULL )
		prevAddrInfoPtr->ai_next = addrInfoPtr;
	addrInfoPtr->ai_family = PF_INET;
	addrInfoPtr->ai_socktype = socketType;
	addrInfoPtr->ai_protocol = ( socketType == SOCK_STREAM ) ? \
							   IPPROTO_TCP : IPPROTO_UDP;
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3, 4 ) ) \
static int SOCKET_API my_getaddrinfo( IN_STRING_OPT const char *nodename,
									  IN_STRING const char *servname,
									  const struct addrinfo *hints,
									  OUT_PTR_COND struct addrinfo **res )
	{
	struct hostent *pHostent;
	const int nodenameLen = ( nodename != NULL ) ? strlen( nodename ) : 0;
#ifdef EBCDIC_CHARS
	char servBuffer[ 16 + 8 ];
#endif /* EBCDIC_CHARS */
	int port, hostErrno, i, status, LOOP_ITERATOR;
	gethostbyname_vars();

	assert( isReadPtr( hints, sizeof( struct addrinfo ) ) );
	assert( isWritePtr( res, sizeof( struct addrinfo * ) ) );

	static_assert( sizeof( in_addr_t ) == IP_ADDR_SIZE, \
				   "in_addr_t size" );

	ANALYSER_HINT_STRING( nodename );
	ANALYSER_HINT_STRING( servname );

	/* Perform basic error checking.  Since this is supposed to be an 
	   emulation of a (normally) built-in function we don't perform any 
	   REQUIRES()-style checking but only apply the basic checks that the 
	   normal built-in form does */
	if( servname == NULL || hints == NULL || res == NULL || \
		( nodename == NULL && !( hints->ai_flags & AI_PASSIVE ) ) )
		{
		/* Provide a more useful diagnostic than the default Unix one of
		   lighting up a giant '?' in the middle of the dashboard */
		DEBUG_DIAG(( "Invalid argument passed to emulated getaddrinfo()" ));
		return( -1 );
		}

	/* Clear return value */
	*res = NULL;

	/* Convert the address information from the system character set to
	   the internal one if required so that strGetNumeric() can process it.
	   This is required because, as an emulated system function, we're
	   being passed strings in the system character set and need to back-
	   convert them to the internal one if we process them using an internal
	   function rather than a system one */
#ifdef EBCDIC_CHARS
	strcpy( servBuffer, servname );
	bufferToAscii( servBuffer, servBuffer );
	servname = servBuffer;
#endif /* EBCDIC_CHARS */

	/* Convert the text-string port number into a numeric value */
	status = strGetNumeric( servname, strlen( servname ), &port, 1, 65535 );
	if( cryptStatusError( status ) )
		{
		/* Provide a more useful diagnostic than the default Unix one of
		   lighting up a giant '?' in the middle of the dashboard */
		DEBUG_DIAG(( "Invalid servname string passed to emulated "
					 "getaddrinfo()" ));
		return( -1 );
		}

	/* If there's no interface specified and we're creating a server-side
	   socket, prepare to listen on any interface.  Note that BeOS can only
	   bind to one interface at a time, so INADDR_ANY actually binds to the
	   first interface that it finds */
	if( nodename == NULL && ( hints->ai_flags & AI_PASSIVE ) )
		{
		const in_addr_t address = INADDR_ANY;

		return( addAddrInfo( NULL, res, &address, sizeof( in_addr_t  ), 
							 port, hints->ai_socktype ) );
		}
	ENSURES( nodename != NULL );

	/* If it's a dotted address, there's a single address, convert it to
	   in_addr form and return it.  Because it's possible to create DNS 
	   names that look very close to dotted-decimal IP addresses, we scan 
	   the entire node name looking for something that isn't a dotted-
	   decimal address and only assume it's a dotted address if no 
	   exceptions are found.
	   
	   Note for EBCDIC use that since this is an emulation of an OS 
	   function the string is already in EBCDIC form, so we don't use the 
	   cryptlib-internal functions for this */
	LOOP_LARGE( i = 0, i < nodenameLen, i++ )
		{
		if( !isdigit( nodename[ i ] ) && nodename[ i ] != DOTTED_DELIMITER )
			break;
		}
	ENSURES( LOOP_BOUND_OK );
	if( i >= nodenameLen )
		{
		const in_addr_t address = inet_addr( nodename );

		if( isBadAddress( address ) )
			return( HOST_NOT_FOUND_ERROR );
		return( addAddrInfo( NULL, res, &address, sizeof( in_addr_t  ), 
							 port, hints->ai_socktype ) );
		}

	/* It's a host name, convert it to the in_addr form */
	gethostbyname_threadsafe( nodename, pHostent, hostErrno );
	if( pHostent == NULL ) 
		{
		/* Returning an error code at this point gets a bit tricky because
		   we're emulating a system/library call and so can't return a
		   proper getaddrinfo()-style EAI_xxx error.  The best that we can
		   do is return the gethostbyname() errno information if it appears
		   to contain a useful error code, and failing that fall back to
		   the OS-specific host-not-found error code */
		if( hostErrno != 0 && hostErrno != -1 )
			return( hostErrno );
		return( HOST_NOT_FOUND_ERROR );
		}
	ENSURES( pHostent->h_length == IP_ADDR_SIZE );
	LOOP_MED( i = 0, i < IP_ADDR_COUNT && \
					 pHostent->h_addr_list[ i ] != NULL, i++ )
		{
		int netAPIstatus;

		netAPIstatus = addAddrInfo( NULL, res, pHostent->h_addr_list[ i ], 
									pHostent->h_length, port, 
									hints->ai_socktype );
		if( cryptStatusError( netAPIstatus ) )
			return( netAPIstatus );
		}
	ENSURES( LOOP_BOUND_OK );

	return( 0 );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void SOCKET_API my_freeaddrinfo( INOUT struct addrinfo *ai )
	{
	int i, LOOP_ITERATOR;

	assert( isWritePtr( ai, sizeof( struct addrinfo ) ) );

	/* Perform basic error checking.  Since this is supposed to be an 
	   emulation of a (normally) built-in function we don't perform any 
	   REQUIRES()-style checking but only apply the basic checks that the 
	   normal built-in form does */
	if( ai == NULL )
		{
		/* Provide a more useful diagnostic than the default Unix one of
		   lighting up a giant '?' in the middle of the dashboard */
		DEBUG_DIAG(( "Invalid argument passed to emulated freeaddrinfo()" ));
		return;
		}

	LOOP_MED( i = 0, ai != NULL && i < IP_ADDR_COUNT, i++ )
		{
		struct addrinfo *addrInfoCursor = ai;

		ai = ai->ai_next;
		if( addrInfoCursor->ai_addr != NULL )
			clFree( "my_freeaddrinfo", addrInfoCursor->ai_addr );
		clFree( "my_freeaddrinfo", addrInfoCursor );
		}
	ENSURES_V( LOOP_BOUND_OK );
	}
									  
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int SOCKET_API my_getnameinfo( IN_BUFFER( salen ) \
											const struct sockaddr *sa, 
									  IN SIZE_TYPE salen,
									  OUT_BUFFER_FIXED( nodelen ) char *node, 
									  IN_LENGTH_SHORT SIZE_TYPE nodelen,
									  OUT_BUFFER_FIXED( servicelen ) \
											char *service, 
									  IN_LENGTH_SHORT_MIN( 8 ) SIZE_TYPE \
											servicelen,
									  IN int flags )
	{
	const struct sockaddr_in *sockAddr = ( struct sockaddr_in * ) sa;
	const char *ipAddress;
	int ipAddressLen;

	assert( isReadPtrDynamic( sa, salen ) && \
			salen >= sizeof( struct sockaddr_in ) );
	assert( isReadPtrDynamic( node, nodelen ) && nodelen >= 10 );
	assert( isReadPtrDynamic( service, servicelen ) && servicelen >= 8 );

	/* Perform basic error checking.  Since this is supposed to be an 
	   emulation of a (normally) built-in function we don't perform any 
	   REQUIRES()-style checking but only apply the basic checks that the 
	   normal built-in form does */
	if( sa == NULL || salen < sizeof( struct sockaddr_in ) || \
		salen > MAX_INTLENGTH_SHORT || \
		node == NULL || nodelen < 10 || nodelen > MAX_INTLENGTH_SHORT || \
		service == NULL || servicelen < 8 || servicelen > MAX_INTLENGTH_SHORT )
		{
		/* Provide a more useful diagnostic than the default Unix one of
		   lighting up a giant '?' in the middle of the dashboard */
		DEBUG_DIAG(( "Invalid argument passed to emulated getnameinfo()" ));
		return( -1 );
		}

	/* Clear return values */
	strlcpy_s( node, nodelen, "<Unknown>" );
	strlcpy_s( service, servicelen, "0" );

	/* Get the remote system's address and port number */
	if( ( ipAddress = inet_ntoa( sockAddr->sin_addr ) ) == NULL )
		return( -1 );
	ipAddressLen = strlen( ipAddress );
	if( ipAddressLen <= 0 || ipAddressLen > nodelen - 1 )
		return( -1 );
	memcpy( node, ipAddress, ipAddressLen );
	node[ ipAddressLen ] = '\0';
	if( sprintf_s( service, servicelen, "%d",
				   ntohs( sockAddr->sin_port ) ) < 0 )
		return( -1 );

	return( 0 );
	}
#endif /* !USE_IPv6_DNSAPI */

/****************************************************************************
*																			*
*						 			DNS Interface							*
*																			*
****************************************************************************/

/* Get a host's IP address */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAddressInfo( INOUT NET_STREAM_INFO *netStream, 
					OUT_PTR_COND struct addrinfo **addrInfoPtrPtr,
					IN_BUFFER_OPT( nameLen ) const char *name, 
					IN_LENGTH_Z const int nameLen, 
					IN_PORT const int port, const BOOLEAN isServer,
					const BOOLEAN isDgramSocket )
	{
	struct addrinfo hints;
	char nameBuffer[ MAX_DNS_SIZE + 1 + 8 ], portBuffer[ 16 + 8 ];
	int errorCode;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtr( addrInfoPtrPtr, sizeof( struct addrinfo * ) ) );
	assert( isServer || name != NULL );

	REQUIRES( port >= MIN_PORT_NUMBER && port < MAX_PORT_NUMBER );
	REQUIRES( isServer == TRUE || isServer == FALSE );
	REQUIRES( isServer || \
			  ( !isServer && name != NULL ) );
	REQUIRES( ( name == NULL && nameLen == 0 ) || \
			  ( name != NULL && \
				nameLen > 0 && nameLen < MAX_DNS_SIZE ) );
	REQUIRES( isDgramSocket == TRUE || isDgramSocket == FALSE );

	/* Clear return value */
	*addrInfoPtrPtr = NULL;

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
		REQUIRES( rangeCheck( nameLen, 1, MAX_DNS_SIZE ) );
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
	   it needs.
	   
	   This is made worse by confusion over implementation details, for 
	   example early implementations of getaddrinfo() in glibc would always 
	   try an AAAA lookup even on an IPv4-only system/network, resulting in 
	   long delays as the resolver timed out and fell back to a straight A 
	   lookup.  There was some disagreement over whether this was right or 
	   wrong, and how to fix it (IPv6 purists who never noticed the problem 
	   seemed to think that it was right, everyone else thought that it was 
	   wrong).
	   
	   Variations of this problem exist, e.g. if an IPv4 address is in 
	   /etc/hosts and DNS is down, the resolver will still spend ages 
	   (several minutes in some cases) groping around for an IPv6 address 
	   before it finally gives up and falls back to what it already knows 
	   from /etc/hosts.  Switching the hint from AF_UNSPEC to AF_INET 
	   bypasses this problem, but has the downside of disabling IPv6 use.

	   This problem was partially fixed post-RFC 2553 by adding the
	   AI_ADDRCONFIG flag, which tells getaddrinfo() to only do AAAA queries
	   if the system has at least one IPv6 source address configured, and
	   the same for A and IPv4 (in other words it applies some common sense,
	   which is how it should have behaved in the first place).
	   Unfortunately this flag isn't very widely supported yet, so it usually
	   ends up being no-op'd out by the auto-config.
	   
	   In addition under Windows Vista and newer there are always IPv6 
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
	BOOLEAN_INT preferIPV4;

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
	hints.ai_socktype = isDgramSocket ? SOCK_DGRAM : SOCK_STREAM;
	errorCode = getaddrinfo( name, portBuffer, &hints, addrInfoPtrPtr );
	if( errorCode != 0 )
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
		return( getAddrInfoError( netStream, errorCode, 
								  CRYPT_ERROR_OPEN ) );
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
				  OUT_LENGTH_BOUNDED_Z( addressMaxLen ) int *addressLen, 
				  OUT_PORT_Z int *port )
	{
	char nameBuffer[ MAX_DNS_SIZE + 8 ];
	char portBuffer[ 32 + 8 ];
	int nameLength, portLength, localPort, status;

	assert( isReadPtrDynamic( sockAddr, sockAddrLen ) );
	assert( isWritePtrDynamic( address, addressMaxLen ) );
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
	   wspiapi.h used for IPv4-only situations does get it right */
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
	REQUIRES_V( rangeCheck( nameLength, 1, addressMaxLen ) );
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
