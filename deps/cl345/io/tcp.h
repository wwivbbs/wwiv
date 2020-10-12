/****************************************************************************
*																			*
*						cryptlib TCP/IP Interface Header					*
*						Copyright Peter Gutmann 1998-2016					*
*																			*
****************************************************************************/

#ifdef USE_TCP

#ifndef _TCP_DEFINED

#define _TCP_DEFINED

/****************************************************************************
*																			*
*						 				AMX									*
*																			*
****************************************************************************/

#if defined( __AMX__ )

#include <kn_sock.h>

/* All KwikNet functions have kn_ prefix, to use the standard sockets API
   names we have to redefine them to the usual names */

#define accept				kn_accept
#define bind				kn_bind
#define closesocket			kn_close
#define connect				kn_connect
#define getsockopt			kn_getsockopt
#define listen				kn_listen
#define recv				kn_recv
#define select				kn_select
#define send				kn_send
#define setsockopt			kn_setsockopt
#define shutdown			kn_shutdown
#define socket				kn_socket

/****************************************************************************
*																			*
*						 				BeOS								*
*																			*
****************************************************************************/

/* If we're building under BeOS the system may have the new(er) BONE (BeOs
   Network Environment) network stack.  This didn't quite make it into BeOS
   v5 before the demise of Be Inc but was leaked after Be folded, as was the
   experimental/developmental Dano release of BeOS, which would have become
   BeOS 5.1 and also has a newer network stack.  In order to detect this we
   have to pull in sys/socket.h before we try anything else */

#elif defined( __BEOS__ )

#include <sys/socket.h>

/* If we're using the original (rather minimal) BeOS TCP/IP stack, we have
   to provide a customised interface for it rather than using the same one
   as the generic Unix/BSD interface */

#if !defined( BONE_VERSION ) && !defined( __HAIKU__ )

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <socket.h>

/* BeOS doesn't define any of the PF_xxx's, howewever it does define some 
   of the AF_xxx equivalents, since these are synonyms we just define the 
   PF_xxx's ourselves */

#define PF_UNSPEC				0
#define PF_INET					AF_INET

/* BeOS doesn't define NO_ADDRESS, but NO_DATA is a synonym for this */

#define NO_ADDRESS				NO_DATA

/* BeOS doesn't have raw sockets, which are used for ICMP messages, however
   we can use a datagram socket for this instead */

#define SOCK_RAW				SOCK_DGRAM

/* BeOS doesn't support checking for anything except readability in select()
   and only supports one or two socket options, so we define our own
   versions of these functions that no-op out unsupported options */

#define select( sockets, readFD, writeFD, exceptFD, timeout ) \
		my_select( sockets, readFD, writeFD, exceptFD, timeout )
#define getsockopt( socket, level, optname, optval, optlen ) \
		my_getsockopt( socket, level, optname, optval, optlen )
#define setsockopt( socket, level, optname, optval, optlen ) \
		my_setsockopt( socket, level, optname, optval, optlen )

/* The following options would be required, but aren't provided by BeOS.  If
   you're building under a newer BeOS version that supports these options,
   you'll also need to update my_set/setsockopt() to no longer no-op them
   out */

#define SO_ERROR				-1
#define TCP_NODELAY				-1

#endif /* !BONE_VERSION && !__HAIKU__ */

/****************************************************************************
*																			*
*						 			embOS									*
*																			*
****************************************************************************/

#elif defined( __embOS__ )

#define IP_BSD_COMPLIANCE		1	/* Enable BSD sockets compatible API */
#include <IP.h>

/* embOS/IP uses nonstandard names for some of its constants */

#define FD_SETSIZE				IP_FD_SETSIZE

/* embOS/IP doesn't have an IPPROTO_TCP/UDP or AF_UNSPEC, however it's only 
   used by the IPv6 emulation layer so we can set it to anything we like 
   (embOS defines PF_UNSPEC to AF_UNSPEC, but forgets to define AF_UNSPEC) */

#if !defined( USE_IPv6_DNSAPI )
  #define IPPROTO_TCP			1
  #define IPPROTO_TCP			2
  #define AF_UNSPEC				1
#endif /* !USE_IPv6_DNSAPI */

/* embOS/IP gets getsockopt() wrong so we have to define our own version 
   that overrides the embOS one */

#undef getsockopt				/* Defined in IP_socket.h */
#define getsockopt				my_getsockopt

int my_getsockopt( int sockfd, int level, int optname, void *optval, 
				   int *optlen );

/* embOS doesn't have any equivalent to errno/h_errno, it's necessary to 
   explicitly fetch the error code via a call to getsockopt() */

int getErrno( int socket );
void clearErrno( void );

#define getErrorCode( socket )	getErrno( socket )
#define getHostErrorCode( socket ) \
								getErrno( socket )
#define clearErrorState			clearErrno

/* embOS/IP doesn't have inet_ntoa() or inet_addr() so we have to provide 
   our own */

char *inet_ntoa( const struct in_addr in );
unsigned long inet_addr( const char *cp );

/****************************************************************************
*																			*
*						 			uITRON									*
*																			*
****************************************************************************/

#elif defined( __ITRON__ )

/* uITRON has a TCP/IP API but it doesn't seem to be widely used, and the
   only available documentation is in Japanese.  If you need TCP/IP support
   under uITRON and have an implementation available, you can add the
   appropriate interface by replacing tcp.c and dns.c with the equivalent 
   uITRON API glue code */

#error You need to set up the TCP/IP headers and interface in tcp.c/dns.c

/****************************************************************************
*																			*
*						 				MQX									*
*																			*
****************************************************************************/

#elif defined( __MQXRTOS__ )

#include <mqx.h>
#include <rtcs.h>
#if RTCSCFG_ENABLE_IP6
  #define USE_IPv6
#endif /* RTCSCFG_ENABLE_IP6 */

/* MQX doesn't (by default) support IPv6 but it does support the new IPv6 
   DNS API functions */

#define USE_IPv6_DNSAPI

/* MQX doesn't define any of the PF_xxx's, howewever it does define
   the AF_xxx equivalents */

#define PF_UNSPEC				AF_UNSPEC

/* MQX doesn't support SO_REUSEADDR, to deal with this we call setsockopt()
   through a wrapper that no-ops out any attempt to use it */

#define SO_REUSEADDR			9999

/* MQX doesn't provide a SOCKADDR_STORAGE so we define our own version */

struct sockaddr_storage {
	union {
		struct sockaddr_in6 bigSockaddrStruct;
		char padding[ 128 ];
		} dummyMember;
	};

/* MQX doesn't implement IPV6_V6ONLY (needed for getsockopt()), the 
   following define gives it an out-of-range value (see rtcs_sock.h) that 
   results in getsockopt() failing, so the operation is skipped */

#ifndef IPV6_V6ONLY
  #define IPV6_V6ONLY			5000
#endif /* !IPV6_V6ONLY */

/* MQX has a select() that looks mostly like a normal select() but isn't,
   it uses its own fd_set types and macros and the last parameter for 
   select() is given as milliseconds rather than a struct timeval.  To deal
   with this we map the standard names to the MQX ones and call the MQX
   select via a wrapper that sets up the parameters correctly */

#define fd_set					rtcs_fd_set

#define FD_SET					RTCS_FD_SET
#define FD_ISSET				RTCS_FD_ISSET
#define FD_ZERO					RTCS_FD_ZERO
#define FD_SETSIZE				RTCSCFG_FD_SETSIZE

struct timeval {
	time_t tv_sec;
	time_t tv_usec;
	};

/****************************************************************************
*																			*
*						 			Nucleus									*
*																			*
****************************************************************************/

#elif defined( __Nucleus__ )

/* Nucleus has it's own functions for network I/O that provide a sort of 
   weird parallel-universe version of the standard sockets API, we map these 
   to standard sockets functions, types, and constants */

#include <nu_net.h>

#define sockaddr_in				SCK_SOCKADDR_IP_STRUCT
#define sin_family				sck_family
#define sin_port				sck_port
#define sin_addr				sck_addr

#define AF_INET					SK_FAM_IP
#define FD_SETSIZE				FD_ELEMENTS
#define INADDR_ANY				IP_ADDR_ANY
#define PF_INET					SK_FAM_IP
#define PF_INET6				SK_FAM_IP6
#define PF_UNSPEC				SK_FAM_UNSPEC
#define SOCK_STREAM				NU_TYPE_STREAM

#define fd_set					struct nu_fd_set
// Nucleus typedefs struct nu_fd_set -> FD_SET, which clashes with
// the standard sockets FD_SET.
#define in_addr_t				UINT32
#define in_port_t				UINT16

#define accept					NU_Accept
#define bind					NU_Bind
#define close					NU_Close_Socket
#define connect					NU_Connect
#define gethostbyname			NU_Get_Host_By_Name
#define getsockopt				NU_Getsockopt
#define listen					NU_Listen
#define recv					NU_Recv
#define send					NU_Send
#define select					NU_Select
#define setsockopt				NU_Setsockopt
#define shutdown				NU_Shutdown
#define socket					NU_Socket

#define FD_ZERO					NU_FD_Init
#define FD_ISSET				NU_FD_Check
#define FD_SET					NU_FD_Set

/* Nucleus NET has IPv6 support, but in a very hit-and-miss manner, for
   example the EAI_xxx values aren't defined (so the autodetection in the
   IPv6 section won't work), but then values like IPV6_V6ONLY are defined.
   On the other hand standard functions like getaddrinfo() don't exist at
   all, so for now we have to restrict ourselves to IPv4.  In order to
   deal with the erratic presence of IPv6 values we undefine any conflicting
   ones as required */

#undef IPV6_V6ONLY

/****************************************************************************
*																			*
*						Quadros/ThreadX (via TreckNet stack)				*
*																			*
****************************************************************************/

#elif defined( __Quadros__ ) || defined( __ThreadX__ )

/* Quadros uses the TreckNet stack.  ThreadX doesn't have native socket 
   support, there is a ThreadX component called NetX but everyone seems to 
   use assorted non-ThreadX network stacks, of which TreckNet is the most
   common */

#include <trsocket.h>

#undef USE_DNSSRV
#undef __WINDOWS__

/* Some versions of the Treck stack don't support all IPv6 options */

#ifndef NI_NUMERICSERV
  #define NI_NUMERICSERV	0	/* Unnecessary for Treck stack */
#endif /* !NI_NUMERICSERV */

/* The Treck stack doesn't implement IPV6_V6ONLY (needed for getsockopt()), 
   the following define gives it an out-of-range value that results in 
   getsockopt() failing, so the operation is skipped */

#ifndef IPV6_V6ONLY
  #define IPV6_V6ONLY			5000
#endif /* !IPV6_V6ONLY */

/* Like Windows, Treck uses special names for close() and ioctl() to avoid
   conflicts with standard system calls, and defines special functions for
   obtaining error information rather than using a static errno-type
   value */

#define closesocket				tfClose
#define ioctlsocket				tfIoctl
#define getErrorCode( socket )	tfGetSocketError( socket )
#define getHostErrorCode( socket ) \
								tfGetSocketError( socket )

/* Map Treck's nonstandard error names to more standard ones */

#ifndef EADDRNOTAVAIL
  #define EBADF					TM_EBADF
  #define EACCES				TM_EACCES
  #define EADDRINUSE			TM_EADDRINUSE
  #define EADDRNOTAVAIL			TM_EADDRNOTAVAIL
  #define EAFNOSUPPORT			TM_EAFNOSUPPORT
  #define EAGAIN				TM_EAGAIN
  #define EALREADY				TM_EALREADY
  #define ECONNABORTED			TM_ECONNABORTED
  #define ECONNREFUSED			TM_ECONNREFUSED
  #define ECONNRESET			TM_ECONNRESET
  #define EINPROGRESS			TM_EINPROGRESS
  #define EINTR					TM_EINTR
  #define EIO					TM_EIO
  #define EISCONN				TM_EISCONN
  #define EMFILE				TM_EMFILE
  #define EMSGSIZE				TM_EMSGSIZE
  #define ENETUNREACH			TM_ENETUNREACH
  #define ENOBUFS				TM_ENOBUFS
  #define ENODEV				TM_ENODEV
  #define ENOPROTOOPT			TM_ENOPROTOOPT
  #define ENOTCONN				TM_ENOTCONN
  #define ENOTSOCK				TM_ENOTSOCK
  #define EPERM					TM_EPERM
  #define EPROTOTYPE			TM_EPROTOTYPE
  #define ETIMEDOUT				TM_ETIMEDOUT
  #define NO_DATA				TM_NO_DATA
#endif /* Standard error names not defined */

/* TreckNet doesn't have an h_errno and no-one seems to know what the 
   substitute for it is, if any, so we no-op it out */

#define h_errno					0

/****************************************************************************
*																			*
*									Telit									*
*																			*
****************************************************************************/

#elif defined( __Telit__ )

/* Telit redefines the entire standard sockets API using its own wrappers
   that overlay the BSD sockets API.  To deal with this without having to 
   create a complete additional wrapping layer to go back to the BSD sockets 
   API, we redefine the Telit interface and values back to standard BSD ones.
   
   Adding to the problem, the Telit stdio.h pulls in headers that define
   their own versions of some of the values that we use here, so we have to
   undefine them in order to redefine them to the Telit values */

#include <m2m_type.h>
#include <m2m_socket_api.h>

#undef FD_ISSET
#undef FD_SET
#undef FD_SETSIZE
#undef FD_ZERO
#undef fd_set

/* Constants */

#define AF_INET					M2M_SOCKET_BSD_AF_INET
#define FD_SETSIZE				M2M_SOCKET_BSD_FD_SETSIZE
#define INADDR_ANY				M2M_SOCKET_BSD_INADDR_ANY
#define IPPROTO_ICMP			M2M_SOCKET_BSD_IPPROTO_ICMP
#define IPPROTO_TCP				M2M_SOCKET_BSD_IPPROTO_TCP
#define IPPROTO_UDP				M2M_SOCKET_BSD_IPPROTO_UDP
#define PF_INET					M2M_SOCKET_BSD_PF_INET
#define PF_UNSPEC				M2M_SOCKET_BSD_PF_UNSPEC
#define SHUT_WR					M2M_SOCKET_BSD_SHUT_WR
#define SO_ERROR				M2M_SOCKET_BSD_SO_ERROR
#define SO_REUSEADDR			M2M_SOCKET_BSD_SO_REUSEADDR
#define SOCK_DGRAM				M2M_SOCKET_BSD_SOCK_DGRAM
#define SOCK_RAW				M2M_SOCKET_BSD_SOCK_RAW
#define SOCK_STREAM				M2M_SOCKET_BSD_SOCK_STREAM
#define SOL_SOCKET				M2M_SOCKET_BSD_SOL_SOCKET
#define TCP_NODELAY				M2M_SOCKET_BSD_TCP_NODELAY

/* Data structures.  The sockaddr/sockaddr_in/hostent structures are another 
   Telit specialty, they're defined as:

	typedef struct M2M_SOCKET_BSD_XXX { ... } M2M_SOCKET_BSD_XXX;

   so they're both a tag and a typedef, and can be used as both
   M2M_SOCKET_BSD_XXX and 'struct M2M_SOCKET_BSD_XXX' */

#define fd_set					M2M_SOCKET_BSD_FD_SET
#define hostent					M2M_SOCKET_BSD_HOSTENT
#define sockaddr				M2M_SOCKET_BSD_SOCKADDR
#define sockaddr_in				M2M_SOCKET_BSD_SOCKADDR_IN
#define timeval					M2M_SOCKET_BSD_TIMEVAL
#define tv_sec					m_tv_sec
#define tv_usec					m_tv_usec

/* Functions */

#define accept					m2m_socket_bsd_accept
#define bind					m2m_socket_bsd_bind
#define closesocket				m2m_socket_bsd_close
#define connect					m2m_socket_bsd_connect
#define FD_ISSET				m2m_socket_bsd_fd_isset_func
#define FD_SET					m2m_socket_bsd_fd_set_func
#define FD_ZERO					m2m_socket_bsd_fd_zero_func
#define getsockopt				m2m_socket_bsd_get_sock_opt
#define htons					m2m_socket_bsd_htons
#define inet_addr				m2m_socket_bsd_inet_addr
#define inet_ntoa( addr )		m2m_socket_bsd_addr_str( addr.s_addr )
#define listen					m2m_socket_bsd_listen
#define ntohs					m2m_socket_bsd_ntohs
#define recv					m2m_socket_bsd_recv
#define recvfrom				m2m_socket_bsd_recv_from
#define select					m2m_socket_bsd_select
#define send					m2m_socket_bsd_send
#define sendto					m2m_socket_bsd_send_to
#define shutdown				m2m_socket_bsd_shutdown
#define setsockopt				m2m_socket_bsd_set_sock_opt
#define socket					m2m_socket_bsd_socket

/* Error names */

#define EACCES					M2M_SOCKET_BSD_EACCES
#define ENOMEM					M2M_SOCKET_BSD_ENOBUFS
#define EADDRINUSE				M2M_SOCKET_BSD_EADDRINUSE
#define EADDRNOTAVAIL			M2M_SOCKET_BSD_EADDRNOTAVAIL
#define EAFNOSUPPORT			M2M_SOCKET_BSD_EAFNOSUPPORT
#define EALREADY				M2M_SOCKET_BSD_EALREADY
#define EBADF					M2M_SOCKET_BSD_EBADF      
#define ECONNABORTED			M2M_SOCKET_BSD_ECONNABORTED   
#define ECONNREFUSED			M2M_SOCKET_BSD_ECONNREFUSED   
#define ECONNRESET				M2M_SOCKET_BSD_ECONNRESET     
#define EINPROGRESS				M2M_SOCKET_BSD_EINPROGRESS    
#define EINTR					M2M_SOCKET_BSD_EINTR          
#define EISCONN					M2M_SOCKET_BSD_EISCONN        
#define EMFILE					M2M_SOCKET_BSD_EMFILE         
#define EMSGSIZE				M2M_SOCKET_BSD_EMSGSIZE       
#define ENETUNREACH				M2M_SOCKET_BSD_ENETUNREACH    
#define ENOBUFS					M2M_SOCKET_BSD_ENOBUFS        
#define ENOPROTOOPT				M2M_SOCKET_BSD_ENOPROTOOPT    
#define ENOTCONN				M2M_SOCKET_BSD_ENOTCONN       
#define ENOTSOCK				M2M_SOCKET_BSD_ENOTSOCK       
#define EPROTOTYPE				M2M_SOCKET_BSD_EPROTOTYPE     
#define ETIMEDOUT				M2M_SOCKET_BSD_ETIMEDOUT      

/* Telit defines special functions to get error codes rather than using 
   global variables, although they're still global-variable equivalents 
   since there's a single errno shared across all sockets */

#define getErrorCode( socket )	m2m_socket_errno()
#define getHostErrorCode( socket ) \
								m2m_socket_errno()

/****************************************************************************
*																			*
*						 Unix and Unix-compatible Systems					*
*																			*
****************************************************************************/

/* Guardian sockets originally couldn't handle nonblocking I/O like standard
   BSD sockets, but required the use of a special non-blocking socket type
   (nowait sockets) and the use of AWAITIOX() on the I/O tag returned from
   the nowait socket call, since the async state was tied to this rather
   than to the socket handle.  One of the early G06 releases added select()
   support, although even the latest documentation still claims that
   select() isn't supported.  To avoid having to support two completely
   different interfaces, we use the more recent (and BSD standard) select()
   interface.  Anyone running this code on old systems will have to add
   wrappers for the necessary socket_nw()/accept_nw()/AWAITIOX() calls */

#elif ( defined( __BEOS__ ) && \
		( defined( BONE_VERSION ) || defined( __HAIKU__ ) ) ) || \
	  defined( __ECOS__ ) || defined( __MVS__ ) || \
	  defined( __PALMOS__ ) || defined( __RTEMS__ ) || \
	  defined ( __SYMBIAN32__ ) || defined( __TANDEM_NSK__ ) || \
	  defined( __TANDEM_OSS__ ) || defined( __UNIX__ )

/* C_IN is a cryptlib.h value which is also defined in some versions of
   netdb.h, so we have to undefine it before we include any network header
   files */

#undef C_IN

/* PHUX and Tandem OSS have broken networking headers that require manually
   defining _XOPEN_SOURCE_EXTENDED in order for various function prototypes
   to be enabled.  The Tandem variant of this problem has all the function
   prototypes for the NSK target and a comment by the 'else' that follows
   saying that it's for the OSS target, but then an ifdef for
   _XOPEN_SOURCE_EXTENDED that prevents it from being enabled unless
   _XOPEN_SOURCE_EXTENDED is also defined */

#if ( defined( __hpux ) && ( OSVERSION >= 10 ) ) || defined( _OSS_TARGET )
  #define _XOPEN_SOURCE_EXTENDED	1
#endif /* Workaround for inconsistent networking headers */

/* In OS X 10.3 (Panther), Apple broke the bind interface by changing the
   BIND_4_COMPAT define to BIND_8_COMPAT ("Apple reinvented the wheel and
   made it square" is one of the more polite comments on this change).  In
   order to get things to work, we have to define BIND_8_COMPAT here, which
   forces the inclusion of nameser_compat.h when we include nameser.h.  All
   (non-Apple) systems automatically define BIND_4_COMPAT to force this
   inclusion, since Bind9 support (in the form of anything other than the
   installed binaries) is still pretty rare.
   
   In addition to this, we need to explicitly define the BSD-style 
   BYTE_ORDER (to agument the existing Gnu-style __BYTE_ORDER__) since
   nameser_compat.h explicitly checks for its presence at the start.  
   Finally, we also need to define _DARWIN_C_SOURCE to ensure the correct
   functioning of various BSD-isms in nameser_compat.h.
   
   Apple actually managed to make the wheel a cube, not a square */

#if defined( __APPLE__ ) && !defined( BIND_8_COMPAT )
  #define BIND_8_COMPAT
  #ifndef BYTE_ORDER
	#define LITTLE_ENDIAN	1234
	#define BIG_ENDIAN		4321
	#if ( __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )
	  #define BYTE_ORDER	LITTLE_ENDIAN
	#else
	  #define BYTE_ORDER	BIG_ENDIAN
	#endif /* Big vs. little-endian */
  #endif /* BYTE_ORDER */
#endif /* Apple braindamage */

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#if defined( __APPLE__ ) || defined( __BEOS__ ) || defined( __bsdi__ ) || \
	defined( __FreeBSD__ ) || defined( __hpux ) || defined( __MVS__ ) || \
	defined( __NetBSD__ ) || defined( __OpenBSD__ ) || defined( __QNX__ ) || \
	( defined( sun ) && OSVERSION <= 5 ) || defined( __SYMBIAN32__ ) || \
	defined( __VMCMS__ )
  #include <netinet/in.h>
#endif /* OS x || BeOS || *BSDs || PHUX || SunOS 4.x/2.5.x || Symbian OS */
#include <arpa/inet.h>
#if !( defined( __CYGWIN__ ) || defined( __PALMOS__ ) || \
	   defined( __SYMBIAN32__ ) || defined( USE_EMBEDDED_OS ) )
  #include <arpa/nameser.h>
#endif /* Cygwin || Symbian OS */
#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* The following have conflicting definitions in xti.h */
  #undef T_NULL
  #undef T_UNSPEC
#endif /* MVS || VM */
#if !defined( __MVS__ )
  /* netinet/tcp.h is a BSD-ism, but all Unixen seem to use this even if
     XPG4 and SUS say it should be in xti.h */
  #include <netinet/tcp.h>
#endif /* !MVS */
#if !( defined( __CYGWIN__ ) || defined( __PALMOS__ ) || \
	   defined( __SYMBIAN32__ ) || defined( USE_EMBEDDED_OS ) )
  #include <resolv.h>
#endif /* Cygwin || Symbian OS */
#if !defined( TCP_NODELAY ) && !defined( USE_EMBEDDED_OS )
  #include <xti.h>
  #if defined( __MVS__ ) || defined( __VMCMS__ )
	/* The following have conflicting definitions in nameser.h */
	#undef T_NULL
	#undef T_UNSPEC
  #endif /* MVS || VM */
#endif /* TCP_NODELAY */
#ifdef __SCO_VERSION__
  #include <signal.h>
  #ifndef SIGIO
	#include <sys/signal.h>
  #endif /* SIGIO not defined in signal.h - only from SCO */
#endif /* UnixWare/SCO */
#if defined( _AIX ) || defined( __PALMOS__ ) || defined( __QNX__ )
  #include <sys/select.h>
#endif /* Aches || Palm OS || QNX */
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef __PALMOS__
  /* Needed for close().  unistd.h, which contains this, is normally
     included by default in Unix environments, but isn't for PalmOS */
  #include <unistd.h>
#endif /* Palm OS */

/* AIX and SCO don't define sockaddr_storage in their IPv6 headers so if
   we detect the use if IPv6 (via IPv6-only status codes) we define a 
   placeholder equivalent here */

#if ( ( defined( _AIX ) && OSVERSION <= 5 ) || \
	  defined( __SCO_VERSION__ ) ) && \
	defined( EAI_BADFLAGS ) && defined( EAI_NONAME )
  struct sockaddr_storage {
		union {
			struct sockaddr_in6 bigSockaddrStruct;
			char padding[ 128 ];
			} dummyMember;
		};
#endif /* IPv6 versions without sockaddr_storage */

/* PHUX generally doesn't define h_errno, we have to be careful here since
   later versions may use macros to get around threading issues so we check
   for the existence of a macro with the given name before defining our own
   version */

#if defined( __hpux ) && !defined( h_errno )
  /* Usually missing from netdb.h */
  extern int h_errno;
#endif /* PHUX && !h_errno */

/****************************************************************************
*																			*
*						 			VxWorks									*
*																			*
****************************************************************************/

#elif defined( __VxWorks__ )

/* The VxWorks' netBufLib.h header defines its own clFree() that conflicts 
   with our one.  To deal with this we either use the nonportable 
   push_macro()/pop_macro() pragma if they're available or we re-include 
   misc/debug.h after overriding the include-once mechanism by undefining 
   _DEBUG_DEFINED and overriding the clAlloc()/clFree()-definition-once 
   mechanism by undefining clAlloc().  
   
   gcc's support for pop_macro() is typically buggy, so if we don't get 
   clFree() defined after we pop it we fall back to the re-include as well. 
   This is why it's done as a #ifndef rather than a #else */

#if defined( __GNUC__ ) || defined( _MSC_VER )
  #pragma push_macro( "clFree" )
#endif /* push_macro() support */
#undef clFree

#include <ioLib.h>
#include <selectLib.h>
#include <hostLib.h>
#include <sockLib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet6/in6.h>
#include <sys/socket.h>

#undef clFree
#if defined( __GNUC__ ) || defined( _MSC_VER )
  #pragma pop_macro( "clFree" )
#endif /* push_macro() support */
#ifndef clFree				/* See comment above */
  #undef _DEBUG_DEFINED		/* Override include-once */
  #undef clAlloc			/* Override define-once */
  #include "misc/debug.h"
#endif /* push_macro() support */

/* Although VxWorks defines AI_NUMERICSERV, any attempt to use it with 
   getaddrinfo() produces an EAI_BADFLAGS error, so we no-op it out */

#undef AI_NUMERICSERV
#define AI_NUMERICSERV	0

/* VxWorks doesn't have an h_errno and no-one seems to know what the 
   substitute for it is, if any, so we no-op it out */

#define h_errno			0

/****************************************************************************
*																			*
*						 			Windows									*
*																			*
****************************************************************************/

#elif defined( __WINDOWS__ )

/* Winsock2 wasn't available until VC++/eVC++ 4.0 so if we're running an
   older version we have to use the Winsock1 interface */

#if defined( _MSC_VER ) && ( _MSC_VER <= 800 ) || \
	defined( __WINCE__ ) && ( _WIN32_WCE < 400 )
  #include <winsock.h>
#else
  #include <winsock2.h>
  #include <ws2tcpip.h>
#endif /* Older WinCE vs. newer WinCE and Win32 */

/* VC++ 7 and newer have IPv6 support included in ws2tcpip.h, VC++ 6 can
   have it bolted-on using the IPv6 Technology Preview but it's not present
   by default.  In addition the Tech.Preview is quite buggy and unstable,
   leaking handles and memory and in some cases leading to runaway memory
   consumption that locks up the machine if the process isn't killed in
   time, so we don't want to encourage its use */

#if defined( _MSC_VER ) && ( _MSC_VER > 1300 )
  /* #include <tpipv6.h> */	/* From IPv6 Tech.Preview */
#endif /* VC++ 7 and newer */

/* VC++ 7 and newer have DNS headers, for older versions (or for builds 
   using the DDK) we have to define the necessary types and constants 
   ourselves */

#if defined( _MSC_VER ) && ( _MSC_VER > 1300 ) && !defined( WIN_DDK )
  #include <windns.h>
#elif defined( _MSC_VER ) && ( _MSC_VER > 800 )
  /* windns.h is for newer compilers and many people don't have it yet, not 
	 helped by the fact that it's also changed over time.  For example,
	 DnsRecordListFree() has also been DnsFreeRecordList() and DnsFree() at
	 various times, with the parameters changing to match.  Because of this,
	 we have to define our own (very cut-down) subset of what's in there
	 here.  We define PIP4_ARRAY as a void * since it's only used to specify
	 optional DNS servers to query, we never need this so we just set the
	 parameter to NULL.  As with the DnsXXX functions, PIP4_ARRAY has
	 changed over time.  It was known as PIP_ARRAY in the original VC++ .NET
	 release but was renamed PIP4_ARRAY for .NET 2003, although some MSDN
	 entries still refer to PIP_ARRAY even in the 2003 version */
  typedef LONG DNS_STATUS;
  typedef void *PIP4_ARRAY;
  typedef DWORD IP4_ADDRESS;
  typedef enum { DnsFreeFlat, DnsFreeRecordList } DNS_FREE_TYPE;
  typedef enum { DnsConfigPrimaryDomainName_W, DnsConfigPrimaryDomainName_A,
				 DnsConfigPrimaryDomainName_UTF8, DnsConfigAdapterDomainName_W,
				 DnsConfigAdapterDomainName_A, DnsConfigAdapterDomainName_UTF8,
				 DnsConfigDnsServerList, DnsConfigSearchList,
				 DnsConfigAdapterInfo, DnsConfigPrimaryHostNameRegistrationEnabled,
				 DnsConfigAdapterHostNameRegistrationEnabled,
				 DnsConfigAddressRegistrationMaxCount, DnsConfigHostName_W,
				 DnsConfigHostName_A, DnsConfigHostName_UTF8,
				 DnsConfigFullHostName_W, DnsConfigFullHostName_A,
				 DnsConfigFullHostName_UTF8 } DNS_CONFIG_TYPE;
  #define DNS_TYPE_A				1
  #define DNS_TYPE_PTR				12
  #define DNS_TYPE_SRV				33
  #define DNS_QUERY_STANDARD		0
  #define DNS_QUERY_BYPASS_CACHE	8
  typedef struct {
	/* Technically these are DWORDs, but only integers are allowed for
	   bitfields.  This is OK in this case because sizeof( int ) ==
	   sizeof( DWORD ) */
	unsigned int Section : 2;
	unsigned int Delete : 1;
	unsigned int CharSet : 2;
	unsigned int Unused : 3;
	unsigned int Reserved : 24;
	} DNS_RECORD_FLAGS;
  typedef struct {
	IP4_ADDRESS IpAddress;
	} DNS_A_DATA, *PDNS_A_DATA;
  typedef struct {
	LPTSTR pNameHost;
	} DNS_PTR_DATA, *PDNS_PTR_DATA;
  typedef struct {
	LPTSTR pNameTarget;
	WORD wPriority;
	WORD wWeight;
	WORD wPort;
	WORD Pad;
	} DNS_SRV_DATA, *PDNS_SRV_DATA;
  typedef struct _DnsRecord {
	struct _DnsRecord *pNext;
	LPTSTR pName;
	WORD wType;
	WORD wDataLength;
	union {
		DWORD DW;
		DNS_RECORD_FLAGS S;
	} Flags;
	DWORD dwTtl;
	DWORD dwReserved;
	union {
		DNS_A_DATA A;
		DNS_PTR_DATA PTR, Ptr,
					 NS, Ns,
					 CNAME, Cname,
					 MB, Mb,
					 MD, Md,
					 MF, Mf,
					 MG, Mg,
					 MR, Mr;
	#if 0
		DNS_MINFO_DATA MINFO, Minfo,
					   RP, Rp;
		DNS_MX_DATA MX, Mx,
					AFSDB, Afsdb,
					RT, Rt;
		DNS_TXT_DATA HINFO, Hinfo,
					 ISDN, Isdn,
					 TXT, Txt,
					 X25;
		DNS_NULL_DATA Null;
		DNS_WKS_DATA WKS, Wks;
		DNS_AAAA_DATA AAAA;
		DNS_KEY_DATA KEY, Key;
		DNS_SIG_DATA SIG, Sig;
		DNS_ATMA_DATA ATMA, Atma;
		DNS_NXT_DATA NXT, Nxt;
	#endif /* 0 */
		DNS_SRV_DATA SRV, Srv;
	#if 0
		DNS_TKEY_DATA TKEY, Tkey;
		DNS_TSIG_DATA TSIG, Tsig;
		DNS_WINS_DATA WINS, Wins;
		DNS_WINSR_DATA WINSR, WinsR,
					   NBSTAT, Nbstat;
	#endif /* 0 */
		} Data;
	} DNS_RECORD, *PDNS_RECORD;
#endif /* VC++ 7 and newer vs. older versions */

/* The Winsock FD_SET() in newer versions of VC++ uses a comma expression 
   that results in the following warning wherever it's used:

	warning C4548: expression before comma has no effect; expected 
				   expression with side-effect

   In theory we could use the __pragma operator introduced in VS 2010
   to disable the warning:

   FD_SET ->
	__pragma( warning( push ) ) \
	__pragma( warning( disable:4548 ) ) \
	FD_SET( ... );
	__pragma( warning( pop ) )

   but there's no obvious way to define a new macro to replace an existing
   one, so for now we'll have to live with the warnings */

/* For backwards-compatibility purposes, wspiapi.h overrides the new address/
   name-handling functions introduced for IPv6 with complex macros that
   substitute inline function calls that try and dynamically load different
   libraries depending on the Windows version and call various helper
   functions to provide the same service.  Since we dynamically load the
   required libraries, we don't need any of this complexity, so we undefine
   the macros in order to make our own ones work */

#ifdef getaddrinfo
  #undef freeaddrinfo
  #undef getaddrinfo
  #undef getnameinfo
#endif /* getaddrinfo defined as macros in wspiapi.h */

/* Set up the appropriate calling convention for the Winsock API */

#if defined( WSAAPI )
  #define SOCKET_API	WSAAPI
#elif defined( WINSOCKAPI )
  #define SOCKET_API	WINSOCKAPI
#else
  #define SOCKET_API	FAR PASCAL
#endif /* WSAAPI */

/****************************************************************************
*																			*
*						 		Custom TCP Stacks							*
*																			*
****************************************************************************/

#elif defined( USE_LWIP )

#define LWIP_DNS		1		/* Needed for DNS lookups */

#include "lwip/sockets.h"
#include "lwip/netdb.h"

/* LWIP has small pieces of IPv6 (getaddrinfo(), freeaddrinfo()) but none of
   the surrounding functions (gai_strerror()) or defines (AI_PASSIVE,
   NI_NUMERICxxx, IPPROTO_IPV6, IPV6_V6ONLY, etc), so we remove the two
   xxxaddrinfo()s (which exist as macros) and rename the LWIP addrinfo 
   struct to something else so that we can replace it with out own one */

#undef getaddrinfo
#undef freeaddrinfo
#define addrinfo		_lwip_addrinfo

/* LWIP doesn't define NO_ADDRESS, but NO_DATA is a synonym for this, or
   IPPROTO_ICMP */

#define NO_ADDRESS				NO_DATA
#define IPPROTO_ICMP			1 

/****************************************************************************
*																			*
*						 			Other Systems							*
*																			*
****************************************************************************/

#else

#error You need to set up OS-specific networking include handling in tcp.h

#endif /* OS-specific includes and defines */

/****************************************************************************
*																			*
*						 	General/Portability Defines						*
*																			*
****************************************************************************/

/* The size of a (v4) IP address and the number of IP addresses that we try
   to connect to for a given host, used if we're providing an emulated
   (IPv4-only) getaddrinfo() */

#define IP_ADDR_SIZE	4
#define IP_ADDR_COUNT	16

/* Test for common socket errors.  For isBadSocket() we don't just compare
   the socket to INVALID_SOCKET but perform a proper range check both to
   catch any problems with buggy implementations that may return something
   other than -1 to indicate an error, and because we're going to use the
   value in FD_xyz() macros that often don't perform any range checking, and
   a value outside the range 0...FD_SETSIZE can cause segfaults and other 
   problems.  In addition we exclude stdin/stdout/stderr if they're present, 
   since a socket with these handle values is somewhat suspicious,

   The one exception to this is Windows sockets, which don't use a Berkeley-
   type bitflag representation and therefore don't have the range problems 
   that the Berkeley implementation does.  In addition they define a socket
   as an opaque unsigned type for which all values apart from INVALID_SOCKET 
   are (theoretically) valid, so we can't perform a range check like we
   could for non-Windows implementations.
   
   Dealing with error reporting via the global variable errno is a pain
   because it's only set on error (so it's not cleared if there's no error), 
   and in some cases (odd embedded stacks) not even for that on some code 
   paths.  To deal with this we have to clear it before any call for which 
   it's checked afterwards.  In theory all the checks are guarded with 
   ( function_return == -1 && errno == xxx ), but always clearring errno
   makes for more defensive programming */

#ifndef INVALID_SOCKET
  #define INVALID_SOCKET			( -1 )
#endif /* INVALID_SOCKET */
#if defined( __WINDOWS__ )
  #define isBadSocket( socket )		( ( socket ) == INVALID_SOCKET )
#elif defined( STDERR_FILENO )
  #define isBadSocket( socket )		( ( socket ) <= STDERR_FILENO || \
									  ( socket ) >= FD_SETSIZE )
#else
  #define isBadSocket( socket )		( ( socket ) <= 0 || \
									  ( socket ) >= FD_SETSIZE )
#endif /* STDERR_FILENO */

#ifndef SOCKET_ERROR
  #define SOCKET_ERROR				( -1 )
#endif /* !SOCKET_ERROR */
#ifndef INADDR_NONE
  #define INADDR_NONE				( ( in_addr_t ) -1 )
#endif /* !INADDR_NONE */
#define isSocketError( status )		( ( status ) == SOCKET_ERROR )
#define isBadAddress( address )		( ( address ) == INADDR_NONE )
#if defined( __MQXRTOS__ ) && ( RTCS_ERROR != SOCKET_ERROR )
  /* MQX defines a return value RTCS_ERROR which happens to coincide with
     the standard socket error value -1, we check this just to make sure 
	 that the values are consistent */
  #error SOCKET_ERROR isnt the same as RTCS_ERROR
#endif /* __MQXRTOS__ */

#if defined( __BEOS__ )
  #define clearErrorState()			errno = 0
  #if defined( BONE_VERSION )
	/* BONE returns "Operation now in progress" */
	#define isNonblockWarning( socket ) \
									( errno == EWOULDBLOCK || \
									  errno == 0x80007024 )
  #else
	/* BeOS, even though it supposedly doesn't support nonblocking
	   sockets, can return EWOULDBLOCK */
	#define isNonblockWarning( socket ) \
									( errno == EWOULDBLOCK )
  #endif /* BeOS with/without BONE */
#elif defined( __embOS__ )
  #define isNonblockWarning( socket ) \
									( getErrno( socket ) == IP_ERR_WOULD_BLOCK )
#elif defined( __MQXRTOS__ )
  /* It's not clear if MQX supports nonblocking connects, according to the 
     docs a connect always blocks and MQX_EINPROGRESS may only exist for
	 Posix compatibility purposes */
  #define clearErrorState()			RTCS_set_errno( MQX_OK )
  #define isNonblockWarning( socket ) \
									( RTCS_errno == MQX_EINPROGRESS )
#elif defined( __SYMBIAN32__ )
  /* Symbian OS doesn't support nonblocking I/O */
  #define isNonblockWarning( socket ) \
									0
#elif defined( __Telit__ )
  #define clearErrorState()			/* No way to clear errors */
  #define isNonblockWarning( socket ) \
									( m2m_socket_errno() == M2M_SOCKET_BSD_EINPROGRESS )
#elif defined( __WINDOWS__ )
  #define isNonblockWarning( socket ) \
									( WSAGetLastError() == WSAEWOULDBLOCK )
#else
  #define clearErrorState()			errno = 0
  #define isNonblockWarning( socket ) \
									( errno == EINPROGRESS )
#endif /* OS-specific socket error handling */

/* Every system has its own way of indicating a timeout and a nonblocking
   connect error.  The following pseudo-error codes are mapped to the 
   appropriate system-specific values */

#if defined( __WINDOWS__ )
  #define TIMEOUT_ERROR				WSAETIMEDOUT
  #define NONBLOCKCONNECT_ERROR		WSAECONNREFUSED
#elif defined( __embOS__ )
  #define TIMEOUT_ERROR				IP_ERR_TIMEOUT
  #define NONBLOCKCONNECT_ERROR		IP_ERR_CONN_REFUSED
#elif defined( __MQXRTOS__ )
  #define TIMEOUT_ERROR				RTCSERR_TCP_TIMED_OUT
  #define NONBLOCKCONNECT_ERROR		RTCSERR_TCP_CONN_REFUSED
#elif defined( __Nucleus__ )
  #define TIMEOUT_ERROR				NU_TIMEOUT
  #define NONBLOCKCONNECT_ERROR		NU_CONNECTION_REFUSED
#else
  #define TIMEOUT_ERROR				ETIMEDOUT
  #define NONBLOCKCONNECT_ERROR		ECONNREFUSED
#endif /* System-specific error types */

/* Values used to disable Nagle via setsockopt() */

#if defined( __embOS__ )
  #define DISABLE_NAGLE_LEVEL		SOL_SOCKET
  #define DISABLE_NAGLE_OPTION		TCP_NODELAY
#elif defined( __MQXRTOS__ )
  #define DISABLE_NAGLE_LEVEL		SOL_TCP
  #define DISABLE_NAGLE_OPTION		OPT_NO_NAGLE_ALGORITHM
#else
  #define DISABLE_NAGLE_LEVEL		IPPROTO_TCP
  #define DISABLE_NAGLE_OPTION		TCP_NODELAY
#endif /* __MQXRTOS__ */

/* Error code handling */

#if defined( __WINDOWS__ )
  #define getErrorCode( socket )	WSAGetLastError()
  #define getHostErrorCode( socket ) \
									WSAGetLastError()
#elif defined( __MQXRTOS__ )
  #define getErrorCode( socket )	RTCS_get_errno()
  #define getHostErrorCode( socket ) \
									RTCS_get_errno()
#elif !defined( getErrorCode )
  #if !defined( clearErrorState )
	#define clearErrorState()		errno = 0
  #endif /* !clearErrorState() */
  #define getErrorCode( socket )	errno
  #if ( defined( __MVS__ ) && defined( _OPEN_THREADS ) )
	/* MVS converts this into a hidden function in the presence of threads,
	   but not transparently like other systems */
	#define getHostErrorCode( socket ) \
									( *__h_errno() )
  #else
	#define getHostErrorCode( socket ) \
									h_errno
  #endif /* MVS */
#endif /* OS-specific error code handling */

/* Some OSes use a distinct socket handle type and require the use of 
   separate closesocket() and ioctlsocket() functions because socket handles
   aren't the same as standard OS handles */

#if !defined( __WINDOWS__ ) && !defined( SOCKET )
  #define SOCKET					int
#endif /* SOCKET not already typedef'd or defined */
#if !defined( __WINDOWS__ ) && !defined( __MQXRTOS__ ) && \
	!defined( closesocket )
  #if !defined( __BEOS__ ) || \
	  ( defined( __BEOS__ ) && defined( BONE_VERSION ) )
	#define closesocket				close
  #endif /* BeOS without BONE */
  #define ioctlsocket				ioctl
#endif /* OS-specific portability defines */

/* Many systems don't define the in_*_t's */

#if defined( __APPLE__ ) || defined( __BEOS__ ) || \
	defined( __bsdi__ ) || defined( _CRAY ) || \
	defined( __CYGWIN__ ) || defined( __FreeBSD__ ) || \
	defined( __hpux ) || defined( __linux__ ) || \
	defined( __NetBSD__ ) || defined( __OpenBSD__ ) || \
	defined( __QNX__ ) || ( defined( sun ) && OSVERSION <= 5 ) || \
	defined( __WINDOWS__ )
  #ifndef in_addr_t
	#define in_addr_t				u_long
	#define in_port_t				u_short
  #endif /* in_addr_t */
#elif defined( __embOS__ )
	#define in_addr_t				U32
	#define in_port_t				U16
#elif defined( __MQXRTOS__ ) || defined( USE_LWIP )
	#define in_addr_t				unsigned long
	#define in_port_t				unsigned short
#elif defined( __Telit__ )
	#define in_addr_t				UINT32
	#define in_port_t				UINT16
#endif /* Systems without in_*_t's */

/* The handling of size parameters to socket functions is, as with most
   things Unix, subject to random portability problems.  The traditional
   BSD sockets API used int for size parameters to socket functions.  Posix 
   decided it'd be better to use size_t, but then people complained that 
   this wasn't binary-compatible with existing usage because on 64-bit
   systems size_t != int.  Instead of changing it back to int, Posix defined
   a new type, socklen_t, which may or may not be an int.  So some systems
   have int, some have size_t, some have socklen_t defined to int, and some
   have socklen_t defined to something else.  
   
   PHUX, as usual, is particularly bad, defaulting to the BSD form with int 
   unless you define _XOPEN_SOURCE_EXTENDED, in which case you get socklen_t 
   but it's mapped to size_t without any change in the sockets API, which 
   still expects int (the PHUX select() has a similar problem, see the 
   comment in random/unix.c).  
   
   Finally, MQX uses uint16_t everywhere except getsockopt(), where it's a 
   uint32_t.  There's no easy way to deal with this so we have to provide a
   wrapper that uses the correct type.  Making SIZE_TYPE an int/uint32_t is
   the last painful, with only one function, accept(), needing to be wrapped.
   
   To resolve this (where it's possible), we try and use socklen_t if we 
   detect its presence, otherwise we use int where we know it's safe to do 
   so, and failing that we fall back to size_t */

#if defined( socklen_t ) || defined( __socklen_t_defined ) || \
	defined( _SOCKLEN_T )
  #define SIZE_TYPE					socklen_t
#elif defined( __BEOS__ ) || defined( _CRAY ) || defined( __WINDOWS__ )
  #define SIZE_TYPE					int
#else
  #define SIZE_TYPE					size_t
#endif /* Different size types */

/* The Bind namespace (via nameser.h) was cleaned up between the old (widely-
   used) Bind4 API and the newer (little-used) Bind8/9 one.  In order to
   handle both, we use the newer definitions, but map them back to the Bind4
   forms if required.  The only thing this doesn't give us is the HEADER
   struct, which seems to have no equivalent in Bind8/9 */

#ifndef NS_PACKETSZ
  #define NS_PACKETSZ				PACKETSZ
  #define NS_HFIXEDSZ				HFIXEDSZ
  #define NS_RRFIXEDSZ				RRFIXEDSZ
  #define NS_QFIXEDSZ				QFIXEDSZ
#endif /* Bind8 names */

/* Older versions of QNX don't define HFIXEDSZ */

#if defined( __QNX__ ) && ( OSVERSION <= 4 )
  #define HFIXEDSZ					12
#endif /* QNX 4.x */

/* Values defined in some environments but not in others.  MSG_NOSIGNAL is
   used to avoid SIGPIPEs on writes if the other side closes the connection,
   if it's not implemented in this environment we just clear the flag */

#ifndef SHUT_WR
  #define SHUT_WR					1
#endif /* SHUT_WR */
#ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL				0
#endif /* MSG_NOSIGNAL */

/* If we can't connect, we perform some basic diagnostics to see whether the
   host is up and reachable.  This requires the use of raw sockets, which 
   aren't available in all environments.
   
   embOS supports raw sockets but no sub-protocols within them (e.g. 
   IPPROTO_ICMP), and while there's direct support for pinging a host via 
   IP_SendPing() it's both a custom API and there's no way to see the 
   returned packet, just an OK/not OK return value, so we don't try and do 
   anything further with this */

#if !defined( __embOS__  ) && !defined( __MQXRTOS__ )
  #define USE_RAW_SOCKETS
#endif /* !__embOS__ && !__MQXRTOS__ */

/* For some connections that involve long-running sessions we need to be
   able to gracefully recover from local errors such as an interrupted system
   call, and remote errors such as the remote process or host crashing and
   restarting, which we can do by closing and re-opening the connection.  The
   various situations are:

	Local error:
		Retry the call on EAGAIN or EINTR

	Process crashes and restarts:
		Write: Remote host sends a RST in response to an attempt to continue
				a TCP session that it doesn't remember, which is reported
				locally as the dreaded (if you ssh or NNTP to remote hosts a
				lot) connection reset by peer error.
		Read: Remote host sends a FIN, we read 0 bytes.

	Network problem:
		Write: Data is re-sent, if a read is pending it returns ETIMEDOUT,
				otherwise write returns EPIPE or SIGPIPE (although we try
				and avoid the latter using MSG_NOSIGNAL).  Some
				implementations may also return ENETUNREACH or EHOSTUNREACH
				if they receive the right ICMP information.
		Read: See above, without the write sematics.

	Host crashes and restarts:
		Write: Looks like a network outage until the host is restarted, then
				gets an EPIPE/SIGPIPE.
		Read: As for write, but gets a ECONNRESET.

   The following macros check for various non-fatal/recoverable error
   conditions, in the future we may want to address some of the others listed
   above as well.  A restartable error is a local error for which we can
   retry the call, a recoverable error is a remote error for which we would
   need to re-establish the connection.  Note that any version of Winsock
   newer than the 16-bit ones shouldn't give us an EINPROGRESS, however some
   early stacks would still give this on occasions such as when another
   thread was doing (blocking) name resolution, and even with the very latest
   versions this is still something that can cause problems for other
   threads */

#if defined( __WINDOWS__ )
  #define clearErrorState()
  #define isRecoverableError( status )	( ( status ) == WSAECONNRESET )
  #define isRestartableError( socket )	( WSAGetLastError() == WSAEWOULDBLOCK || \
										  WSAGetLastError() == WSAEINPROGRESS )
  #define isTimeoutError( socket )		( WSAGetLastError() == WSAETIMEDOUT )
#elif defined( __embOS__ )
  #define isRecoverableError( status )	( ( status ) == IP_ERR_CONN_RESET )
  #define isRestartableError( socket )	( getErrno( socket ) == IP_ERR_WOULD_BLOCK || \
										  getErrno( socket ) == IP_ERR_IN_PROGRESS )
  #define isTimeoutError( socket )		( getErrno( socket ) == IP_ERR_TIMEOUT )
#elif defined( __MQXRTOS__ )
  /* We can't use RTCS_get_errno() to get the error since this clears the 
     error state after it's read, so we have to access RTCS_errno 
	 directly.  In addition it's not clear if MQX_EINTR or MQX_EAGAIN will
	 ever be returned, they're Posix-compatibility codes that probably have
	 no equivalent in MQX */
  #define isRecoverableError( status )	( ( status ) == RTCSERR_TCP_CONN_RESET )
  #define isRestartableError( socket )	( RTCS_errno == MQX_EINTR || \
										  RTCS_errno == MQX_EAGAIN )
  #define isTimeoutError( socket )		( RTCS_errno == RTCSERR_TIMEOUT || \
										  RTCS_errno == RTCSERR_TCP_TIMED_OUT )
#elif defined( __Telit__ )
  #define isRecoverableError( status )	( ( status ) == M2M_SOCKET_BSD_ECONNRESET )
  #define isRestartableError( socket )	( m2m_socket_errno() == M2M_SOCKET_BSD_EINTR )
  #define isTimeoutError( socket )		( m2m_socket_errno() == M2M_SOCKET_BSD_ETIMEDOUT )
#else
  #if !defined( clearErrorState )
	#define clearErrorState()			errno = 0
  #endif /* !clearErrorState() */
  #define isRecoverableError( status )	( ( status ) == ECONNRESET )
  #define isRestartableError( socket )	( errno == EINTR || errno == EAGAIN )
  #define isTimeoutError()				( errno == ETIMEDOUT )
#endif /* OS-specific status codes */

/****************************************************************************
*																			*
*						 		IPv6 Defines								*
*																			*
****************************************************************************/

/* Now that we've included all of the networking headers, try and guess
   whether this is an IPv6-enabled system.  We can detect this by the
   existence of definitions for the EAI_xxx return values from
   getaddrinfo().  Note that we can't safely detect it using the more
   obvious AF_INET6 since many headers defined this in anticipation of IPv6
   long before the remaining code support was present */

#if defined( EAI_BADFLAGS ) && defined( EAI_NONAME )
  #define USE_IPv6
  #define USE_IPv6_DNSAPI
#endif /* getaddrinfo() return values defined */

/* Some systems have just enough IPv6 defines present to be awkward (BeOS 
   with the BONE network stack) so we temporarily define IPv6 and then use a 
   stack-specific subset of IPv6 defines further on */

#if defined( __BEOS__ ) && defined( BONE_VERSION )
  #define USE_IPv6
#endif /* BeOS with BONE */

/* The generic sockaddr struct used to reserve storage for protocol-specific
   sockaddr structs.  The IPv4 equivalent is given below in the IPv6-
   emulation definitions */

#ifdef USE_IPv6
  #define SOCKADDR_STORAGE			struct sockaddr_storage
#endif /* IPv6 */

/* IPv6 emulation functions used to provide a single consistent interface.
   We distinguish between USE_IPv6 for full IPv6 functionality and 
   USE_IPv6_DNSAPI for support for the DNS functions introduced in IPv6, 
   getaddrinfo() et al, without full IPv6 support */

#ifndef USE_IPv6_DNSAPI
  /* The addrinfo struct used by getaddrinfo() */
  struct addrinfo {
	int ai_flags;				/* AI_PASSIVE, NI_NUMERICHOST */
	int ai_family;				/* PF_INET */
	int ai_socktype;			/* SOCK_STREAM */
	int ai_protocol;			/* IPPROTO_TCP */
	size_t ai_addrlen;			/* Length of ai_addr */
	char *ai_canonname;			/* CNAME for nodename */
	ARRAY_FIXED( ai_addrlen ) \
	struct sockaddr *ai_addr;	/* IPv4 or IPv6 sockaddr */
	struct addrinfo *ai_next;	/* Next addrinfo structure list */
	};

  /* getaddrinfo() flags and values */
  #define AI_PASSIVE		0x1		/* Flag for hints are for getaddrinfo() */

  /* An emulation of the getaddrinfo() function family */
  #define getaddrinfo		my_getaddrinfo
  #define freeaddrinfo		my_freeaddrinfo
  #define getnameinfo		my_getnameinfo

  /* Error codes returned by the emulated getaddrinfo() */
  #if defined( __WINDOWS__ )
	#define OUT_OF_MEMORY_ERROR		WSAENOBUFS
	#define HOST_NOT_FOUND_ERROR	WSAHOST_NOT_FOUND
  #elif defined( __embOS__ )
	#define OUT_OF_MEMORY_ERROR		IP_ERR_NO_MEM
	#define HOST_NOT_FOUND_ERROR	IP_ERR_ADDR_NOT_AVAIL
  #elif defined( __MQXRTOS__ )
	#define OUT_OF_MEMORY_ERROR		RTCSERR_OUT_OF_MEMORY
	#define HOST_NOT_FOUND_ERROR	RTCSERR_DNS_INVALID_NAME
  #elif defined( __Nucleus__ )
	#define OUT_OF_MEMORY_ERROR		NU_NO_SOCK_MEMORY
	#define HOST_NOT_FOUND_ERROR	NU_NOT_A_HOST
  #else
	#define OUT_OF_MEMORY_ERROR		ENOMEM
	#if !( defined( __Quadros__ ) || defined( __Telit__ ) )
	  #define HOST_NOT_FOUND_ERROR	HOST_NOT_FOUND
	#else
	  #define HOST_NOT_FOUND_ERROR	EADDRNOTAVAIL	/* No proper error code */
	#endif /* !( __Quadros__ || __Telit__ ) */
  #endif /* System-specific error types */

  /* Windows uses the Pascal calling convention for these functions, we hide 
     this behind a define that becomes a no-op on non-Windows systems */
  #ifndef SOCKET_API
	#define SOCKET_API
  #endif /* SOCKET_API */
#endif /* USE_IPv6_DNSAPI */

#ifndef USE_IPv6
  /* The generic sockaddr struct used to reserve storage for protocol-
     specific sockaddr structs.  This isn't quite right but since all
	 we're using it for is to reserve storage (we never actually look
	 inside it) it's OK to use here  */
  typedef char SOCKADDR_STORAGE[ 128 ];

  /* getnameinfo() flags and values.  Windows uses different values for 
     these than anyone else, and even if we're not on an explicitly IPv6-
	 enabled system we could still end up dynamically pulling in the 
	 required libraries, so we need to ensure that we're using the same flag
	 values that Windows does */
  #ifdef __WINDOWS__
	#define NI_NUMERICHOST	0x2		/* Return numeric form of host addr.*/
	#define NI_NUMERICSERV	0x8		/* Return numeric form of host port */
  #else
	#define NI_NUMERICHOST	0x1		/* Return numeric form of host addr.*/
	#define NI_NUMERICSERV	0x2		/* Return numeric form of host port */
  #endif /* __WINDOWS__ */

  /* get/setsockopt() flags and values.  Again, we have to use slightly
     different values for Windows in some cases */
  #define IPPROTO_IPV6		41		/* IPv6 */
  #ifndef IPV6_V6ONLY
	/* May be overridden by an earlier define for some stacks that turns 
	   it into a no-op */
	#if defined( __WINDOWS__ ) || defined( __VxWorks__ )
	  #define IPV6_V6ONLY	27		/* Force dual stack to use only IPv6 */
	#else
	  #define IPV6_V6ONLY	26		/* Force dual stack to use only IPv6 */
	#endif /* __WINDOWS__ */
  #endif /* IPV6_V6ONLY */
#else
  /* IPV6_V6ONLY isn't universally defined under Windows even if IPv6 
	 support is available.  The situations under which this occurs are 
	 rather unclear, it's happened for some x86-64 builds (although not for 
	 straight x86 builds on the same machine), for older WinCE builds, and
	 in one case for an x86 build using VS 2005, possibly caused by 
	 differences between VS and WinSDK headers.  To resolve this mess, if 
	 IPv6 is defined under Windows but IPV6_V6ONLY isn't, we explicitly 
	 define it ourselves */
  #if defined( __WINDOWS__ ) && !defined( IPV6_V6ONLY )
	#define IPV6_V6ONLY		27		/* Force dual stack to use only IPv6 */
  #endif /* Some Windows build environments */
#endif /* USE_IPv6 */

/* A subset of the above for BeOS with the BONE network stack.  See the
   full IPv6 version above for descriptions of the entries */

#if defined( __BEOS__ ) && defined( BONE_VERSION )
  #undef USE_IPv6					/* We really don't do IPv6 */

  typedef char SOCKADDR_STORAGE[ 128 ];

  #define getaddrinfo		my_getaddrinfo
  #define freeaddrinfo		my_freeaddrinfo
  #define getnameinfo		my_getnameinfo

  static int my_getaddrinfo( const char *nodename, const char *servname,
							 const struct addrinfo *hints,
							 struct addrinfo **res );
  static void my_freeaddrinfo( struct addrinfo *ai );
  static int my_getnameinfo( const struct sockaddr *sa, SIZE_TYPE salen,
							 char *node, SIZE_TYPE nodelen,
							 char *service, SIZE_TYPE servicelen,
							 int flags );
#endif /* BeOS with BONE */

/* Older versions of the Sun development tools define some IPv6 options but 
   not others */

#if defined( USE_IPv6 ) && defined( __SUNPRO_C )
  #ifndef IPV6_V6ONLY
	#define IPV6_V6ONLY	26		/* Force dual stack to use only IPv6 */
  #endif /* !IPV6_V6ONLY */
#endif /* Slowaris with older Sun compilers */

/****************************************************************************
*																			*
*						 		Resolver Defines							*
*																			*
****************************************************************************/

/* Values defined in some environments but not in others.  T_SRV and
   NS_SRVFIXEDSZ are used for DNS SRV lookups.  Newer versions of bind use a
   ns_t_srv enum for T_SRV but since we can't autodetect this via the
   preprocessor we always define T_SRV ourselves */

#ifndef T_SRV
  #define T_SRV						33
#endif /* !T_SRV */
#ifndef NS_SRVFIXEDSZ
  #define NS_SRVFIXEDSZ				( NS_RRFIXEDSZ + 6 )
#endif /* !NS_SRVFIXEDSZ */
#ifndef AI_ADDRCONFIG
  #define AI_ADDRCONFIG				0
#endif /* !AI_ADDRCONFIG */
#ifndef AI_NUMERICSERV
  #define AI_NUMERICSERV			0
#endif /* !AI_NUMERICSERV */

/* Check whether an address family returned from a DNS lookup is allowed 
   (meaning recognised) */

#ifdef USE_IPv6
  #define allowedAddressFamily( family ) \
		  ( ( ( family ) == AF_INET ) || ( ( family ) == AF_INET6 ) )
#else
  #define allowedAddressFamily( family )	( ( family ) == AF_INET )
#endif /* IPv6 */

/* gethostbyname is a problem function because the standard version is non-
   thread-safe due to the use of static internal storage to contain the
   returned host info.  Some OSes (Windows, PHUX >= 11.0, OSF/1 >= 4.0,
   Aches >= 4.3) don't have a problem with this because they use thread
   local storage, but others either require the use of nonstandard _r
   variants or simply don't handle it at all.  To make it even more
   entertaining, there are at least three different variations of the _r
   form:

	Linux (and glibc systems in general, but not BeOS with BONE):

	int gethostbyname_r( const char *name, struct hostent *result_buf,
						 char *buf, size_t buflen, struct hostent **result,
						 int *h_errnop);

	Slowaris >= 2.5.1, IRIX >= 6.5, QNX:

	struct hostent *gethostbyname_r( const char *name,
									 struct hostent *result, char *buffer,
									 int buflen, int *h_errnop );

	OSF/1, Aches (deprecated, see above):

	int gethostbyname_r( const char *name, struct hostent *hptr,
						 struct hostent_data *hdptr );

   To work around this mess, we define macros for thread-safe versions of
   gethostbyname that can be retargeted to the appropriate function as
   required.
   
   In addition, Telit and embOS barely have any DNS functionality, and in
   contrast to all of their other BSD-equivalent functions provide a 
   gethostbyname() or equivalent that's nothing like the BSD version, which 
   means that we have to emulate it via a complex wrapper */

#if defined( USE_THREADS ) && defined( __GLIBC__ ) && ( __GLIBC__ >= 2 ) && \
	( !defined( __BEOS__ ) || !defined( BONE_VERSION ) )
  #define gethostbyname_vars() \
		  char hostBuf[ 4096 ]; \
		  struct hostent hostEnt;
  #define gethostbyname_threadsafe( hostName, hostEntPtr, hostErrno ) \
		  hostErrno = 0; \
		  if( gethostbyname_r( hostName, &hostEnt, hostBuf, 4096, &hostEntPtr, &hostErrno ) < 0 ) \
			hostEntPtr = NULL
#elif defined( USE_THREADS ) && \
	  ( ( defined( sun ) && OSVERSION > 4 ) || \
		( defined( __sgi ) && OSVERSION >= 6 ) || defined( __QNX__ ) )
  #define gethostbyname_vars() \
		  char hostBuf[ 4096 ]; \
		  struct hostent hostEnt;
  #define gethostbyname_threadsafe( hostName, hostEntPtr, hostErrno ) \
		  hostErrno = 0; \
		  hostEntPtr = gethostbyname_r( hostName, &hostEnt, hostBuf, 4096, &hostErrno )
#elif defined( USE_THREADS ) && ( defined( USE_LWIP ) )
  #define gethostbyname_vars() \
		  char hostBuf[ 1024 ]; \
		  struct hostent hostEnt;
  #define gethostbyname_threadsafe( hostName, hostEntPtr, hostErrno ) \
		  hostErrno = 0; \
		  if( gethostbyname_r( hostName, &hostEnt, hostBuf, 1024, &hostEntPtr, &hostErrno ) < 0 ) \
			hostEntPtr = NULL
#elif defined( __embOS__ )
  #define gethostbyname_vars() \
		  char *hAddrList[ 2 ]; \
		  struct hostent hostEnt;
  #define gethostbyname_threadsafe( hostName, hostEntPtr, hostErrno ) \
		  { \
		  U32 hostAddress; \
		  int result; \
		  \
		  hostErrno = 0; \
		  result = IP_ResolveHost( hostName, &hostAddress, 10000 ); \
		  if( result < 0 ) \
			{ \
			hostEntPtr = NULL; \
			hostErrno = IP_ERR_ADDR_NOT_AVAIL; \
			} \
		  else \
			{ \
			hostEntPtr = &hostEnt; \
			memset( hostEntPtr, 0, sizeof( struct hostent ) ); \
			hostEntPtr->h_addrtype = AF_INET; \
			hostEntPtr->h_length = IP_ADDR_SIZE; \
			hAddrList[ 0 ] = ( CHAR * ) hostAddress; /* Already in network order */ \
			hAddrList[ 1 ] = NULL; \
			hostEntPtr->h_addr_list = hAddrList; \
			} \
		  }
#elif defined( __Telit__ )
  #define gethostbyname_vars() \
		  char *hAddrList[ 2 ]; \
		  M2M_SOCKET_BSD_HOSTENT hostEnt;
  #define gethostbyname_threadsafe( hostName, hostEntPtr, hostErrno ) \
		  { \
		  UINT32 hostAddress; \
		  \
		  hostErrno = 0; \
		  hostAddress = m2m_socket_bsd_get_host_by_name( hostName ); \
		  if( hostAddress == 0 ) \
			{ \
			hostEntPtr = NULL; \
			hostErrno = m2m_socket_errno(); \
			} \
		  else \
			{ \
			hostEntPtr = &hostEnt; \
			memset( hostEntPtr, 0, sizeof( M2M_SOCKET_BSD_HOSTENT ) ); \
			hostEntPtr->h_addrtype = M2M_SOCKET_BSD_AF_INET; \
			hostEntPtr->h_length = IP_ADDR_SIZE; \
			hAddrList[ 0 ] = ( CHAR * ) m2m_socket_bsd_htonl( hostAddress ); \
			hAddrList[ 1 ] = NULL; \
			hostEntPtr->h_addr_list = hAddrList; \
			} \
		  }
#else
  #define gethostbyname_vars()
  #define gethostbyname_threadsafe( hostName, hostEntPtr, hostErrno ) \
		  hostEntPtr = gethostbyname( hostName ); \
		  hostErrno = h_errno
#endif /* Various gethostbyname variants */

/****************************************************************************
*																			*
*						 	Non-blocking I/O Defines						*
*																			*
****************************************************************************/

/* The traditional way to set a descriptor to nonblocking mode was an
   ioctl with FIONBIO, however Posix prefers the O_NONBLOCK flag for fcntl()
   so we use this if it's available, with some exceptions for systems like
   VxWorks where it's present but doesn't work as expected.

   Unfortunately if we haven't got the fcntl() interface available there's
   no way to determine whether a socket is non-blocking or not, which is
   particularly troublesome for Windows where we need to ensure that the
   socket is blocking in order to avoid Winsock bugs with nonblocking
   sockets.  Although WSAIoctl() would appear to provide an interface for
   obtaining the nonblocking status, it doesn't provide any more
   functionality than ioctlsocket(), returning an error if we try and read
   the FIONBIO value.

   If we're just using this as a basic valid-socket check we could also use 
   ( GetFileType( ( HANDLE ) stream->netSocket ) == FILE_TYPE_PIPE ) ? 0 : \
   WSAEBADF to check that it's a socket, but there's a bug under all Win9x 
   versions for which GetFileType() on a socket returns FILE_TYPE_UNKNOWN, 
   so we can't reliably detect a socket with this.  In any case though 
   ioctlsocket() will return WSAENOTSOCK if it's not a socket, so this is 
   covered by the default handling anyway.

   The best that we can do in this case is to force the socket to be
   blocking, which somewhat voids the guarantee that we leave the socket as
   we found it, but OTOH if we've been passed an invalid socket the caller
   will have to abort and fix the problem anyway, so changing the socket
   state isn't such a big deal.

   BeOS is even worse, not only is there no way to determine whether a
   socket is blocking or not, it'll also quite happily perform socket
   functions like setsockopt() on a file descriptor (for example stdout),
   so we can't even use this as a check for socket validity as it is under
   other OSes.  Because of this the check socket function will always
   indicate that something vaguely handle-like is a valid socket.

   When we get the nonblocking status, if there's an error getting the
   status we report it as a non-blocking socket, which results in the socket
   being reported as invalid, the same as if it were a a genuine non-
   blocking socket.
   
   If we're using the ioctlsocket() interface we make the argument an
   unsigned long, in most cases this is a 'void *' but under Windows it's
   an 'unsigned long *' so we use the most restrictive type */

#if defined( F_GETFL ) && defined( F_SETFL ) && defined( O_NONBLOCK ) && \
	!defined( __VxWorks__ )
  #define getSocketNonblockingStatus( socket, value ) \
			{ \
			value = fcntl( socket, F_GETFL, 0 ); \
			value = ( isSocketError( value ) || ( value & O_NONBLOCK ) ) ? \
					TRUE : FALSE; \
			}
  #define setSocketNonblocking( socket ) \
			{ \
			const int flags = fcntl( socket, F_GETFL, 0 ); \
			fcntl( socket, F_SETFL, flags | O_NONBLOCK ); \
			}
  #define setSocketBlocking( socket ) \
			{ \
			const int flags = fcntl( socket, F_GETFL, 0 ); \
			fcntl( socket, F_SETFL, flags & ~O_NONBLOCK ); \
			}
#elif defined( FIONBIO )
  #define getSocketNonblockingStatus( socket, value ) \
			{ \
			unsigned long nonBlock = 0; \
			value = ioctlsocket( socket, FIONBIO, &nonBlock ); \
			value = isSocketError( value ) ? TRUE : FALSE; \
			}
  #define setSocketNonblocking( socket ) \
			{ \
			unsigned long nonBlock = 1; \
			ioctlsocket( socket, FIONBIO, &nonBlock ); \
			}
  #define setSocketBlocking( socket ) \
			{ \
			unsigned long nonBlock = 0; \
			ioctlsocket( socket, FIONBIO, &nonBlock ); \
			}
#elif defined( __AMX__ ) || defined( __BEOS__ ) || defined( __embOS__ )
  #define getSocketNonblockingStatus( socket, value ) \
			{ \
			int nonBlock = 0; \
			value = getsockopt( socket, SOL_SOCKET, SO_NONBLOCK, &nonBlock, sizeof( int ) ); \
			value = ( isSocketError( value ) || nonBlock ) ? \
					TRUE : FALSE; \
			}
  #define setSocketNonblocking( socket ) \
			{ \
			int nonBlock = 1; \
			setsockopt( socket, SOL_SOCKET, SO_NONBLOCK, &nonBlock, sizeof( int ) ); \
			}
  #define setSocketBlocking( socket ) \
			{ \
			int nonBlock = 0; \
			setsockopt( socket, SOL_SOCKET, SO_NONBLOCK, &nonBlock, sizeof( int ) ); \
			}
#elif defined( __MQXRTOS__ )
  #define getSocketNonblockingStatus( socket, value ) \
			{ \
			uint32_t size = sizeof( int ); \
			int nonBlock = 0; \
			value = getsockopt( socket, SOL_SOCKET, OPT_SEND_NOWAIT, &nonBlock, &size ); \
			value = ( isSocketError( value ) || nonBlock ) ? \
					TRUE : FALSE; \
			}
  #define setSocketNonblocking( socket ) \
			{ \
			int nonBlock = 1; \
			setsockopt( socket, SOL_SOCKET, OPT_SEND_NOWAIT, &nonBlock, sizeof( int ) ); \
			}
  #define setSocketBlocking( socket ) \
			{ \
			int nonBlock = 0; \
			setsockopt( socket, SOL_SOCKET, OPT_SEND_NOWAIT, &nonBlock, sizeof( int ) ); \
			}
#elif defined( __Nucleus__ )
  /* Nucleus doesn't provide a mechanism to check whether a socket is non-
     blocking, however the only time that this capability is required is 
	 when we're checking a user-provided socket.  These are created 
	 blocking by default under Nucleus, so we just hardwire the check to say 
	 that it's blocking */
  #define getSocketNonblockingStatus( socket, value )	value = FALSE
  #define setSocketNonblocking( socket ) \
			NU_Fcntl( socket, NU_SETFLAG, NU_NO_BLOCK )
  #define setSocketBlocking( socket ) \
			NU_Fcntl( socket, NU_SETFLAG, NU_BLOCK )
#elif defined( __SYMBIAN32__ )
  /* Symbian OS doesn't support nonblocking I/O */
  #define getSocketNonblockingStatus( socket, value )	value = FALSE
  #define setSocketNonblocking( socket )
  #define setSocketBlocking( socket )
#elif defined( __Telit__ )
  /* Telit doesn't provide a mechanism to check whether a socket is non-
     blocking, however the only time that this capability is required is 
	 when we're checking a user-provided socket.  These appear to be created
	 blocking by default under Telit, so we just hardwire the check to say 
	 that it's blocking */
  #define getSocketNonblockingStatus( socket, value )	value = FALSE
  #define setSocketNonblocking( socket ) \
			{ \
			INT32 nonBlock = 1; \
			m2m_socket_bsd_ioctl( socket, M2M_SOCKET_BSD_FIONBIO, &nonBlock ); \
			}
  #define setSocketBlocking( socket ) \
			{ \
			INT32 nonBlock = 0; \
			m2m_socket_bsd_ioctl( socket, M2M_SOCKET_BSD_FIONBIO, &nonBlock ); \
			}
#else
  #error Need to create macros to handle nonblocking I/O
#endif /* Handling of blocking/nonblocking sockets */

/****************************************************************************
*																			*
*						 	Misc.Functions and Defines						*
*																			*
****************************************************************************/

/* Prototypes for functions in dns.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAddressInfo( INOUT NET_STREAM_INFO *netStream, 
					OUT_PTR_COND struct addrinfo **addrInfoPtrPtr,
					IN_BUFFER_OPT( nameLen ) const char *name, 
					IN_LENGTH_Z const int nameLen, 
					IN_PORT const int port, const BOOLEAN isServer,
					const BOOLEAN isStreamSocket );
STDC_NONNULL_ARG( ( 1 ) ) \
void freeAddressInfo( struct addrinfo *addrInfoPtr );
STDC_NONNULL_ARG( ( 1, 3, 5, 6 ) ) \
void getNameInfo( IN_BUFFER( sockAddrLen ) const void *sockAddr,
				  IN_LENGTH_SHORT_MIN( 8 ) const int sockAddrLen,
				  OUT_BUFFER( addressMaxLen, *addressLen ) char *address, 
				  IN_LENGTH_DNS const int addressMaxLen, 
				  OUT_LENGTH_BOUNDED_Z( addressMaxLen ) int *addressLen, 
				  OUT_PORT_Z int *port );

/* Prototypes for functions in dns_srv.c */

#ifdef USE_DNSSRV
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
  int findHostInfo( INOUT NET_STREAM_INFO *netStream, 
					OUT_BUFFER_FIXED( hostNameMaxLen ) char *hostName, 
					IN_LENGTH_DNS const int hostNameMaxLen, 
					OUT_PORT_Z int *hostPort, 
					IN_BUFFER( nameLen ) const char *name, 
					IN_LENGTH_DNS const int nameLen );
#else
  /* If there's no DNS support available in the OS there's not much that we
	 can do to handle automatic host detection.  Setting hostPort as a side-
	 effect is necessary because the #define otherwise no-ops it out, 
	 leading to declared-but-not-used warnings from some compilers */
  #define findHostInfo( netStream, hostName, hostNameLen, hostPort, name, nameLen )	\
		  setSocketError( netStream, "DNS SRV services not available", 30, \
						  CRYPT_ERROR_NOTAVAIL, FALSE ); \
		  memset( hostName, 0, min( 16, hostNameLen ) ); \
		  *( hostPort ) = 0
#endif /* USE_DNSSRV */
#endif /* _TCP_DEFINED */
#endif /* USE_TCP */
