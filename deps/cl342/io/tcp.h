/****************************************************************************
*																			*
*						cryptlib TCP/IP Interface Header					*
*						Copyright Peter Gutmann 1998-2007					*
*																			*
****************************************************************************/

#ifdef USE_TCP

/****************************************************************************
*																			*
*						 				AMX									*
*																			*
****************************************************************************/

#ifdef __AMX__

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

#endif /* AMX */

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

#ifdef __BEOS__
  #include <sys/socket.h>
#endif /* __BEOS__ */

/* If we're using the original (rather minimal) BeOS TCP/IP stack, we have
   to provide a customised interface for it rather than using the same one
   as the generic Unix/BSD interface */

#if defined( __BEOS__ ) && !defined( BONE_VERSION )

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <socket.h>

/* BeOS doesn't define any of the PF_xxx's, howewever it does defines
   some of the AF_xxx equivalents, since these are synonyms we just define
   the PF_xxx's ourselves */

#define PF_UNSPEC				0
#define PF_INET					AF_INET

/* BeOS doesn't define in_*_t's */

#define in_addr_t				u_long
#define in_port_t				u_short

/* BeOS doesn't define NO_ADDRESS, but NO_DATA is a synonym for this */

#define NO_ADDRESS				NO_DATA

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

/****************************************************************************
*																			*
*						 			ThreadX									*
*																			*
****************************************************************************/

#elif defined( __ThreadX__ )

/* ThreadX doesn't have native socket support, there is a ThreadX component
   called NetX but everyone seems to use assorted non-ThreadX network 
   stacks.  The following is a representative entry for Treck's network 
   stack */

#include "r:/temp/trsocket.h"

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
  #define IPV6_V6ONLY		5000
#endif /* !IPV6_V6ONLY */

/* Like Windows, Treck uses special names for close() and ioctl() to avoid
   conflicts with standard system calls, and defines special functions for
   obtaining error information rather than using a static errno-type
   value */

#define closesocket			tfClose
#define ioctlsocket			tfIoctl
#define getErrorCode()		tfGetSocketError( netStream->netSocket )
#define getHostErrorCode()	tfGetSocketError( netStream->netSocket )

/* Map Treck's nonstandard error names to more standard ones */

#ifndef EADDRNOTAVAIL
  #define EADDRNOTAVAIL		TM_EADDRNOTAVAIL
  #define EAGAIN			TM_EAGAIN
  #define ECONNABORTED		TM_ECONNABORTED
  #define ECONNREFUSED		TM_ECONNREFUSED
  #define ECONNRESET		TM_ECONNRESET
  #define EINPROGRESS		TM_EINPROGRESS
  #define EINTR				TM_EINTR
  #define EMFILE			TM_EMFILE
  #define EMSGSIZE			TM_EMSGSIZE
  #define ENETUNREACH		TM_ENETUNREACH
  #define ENOBUFS			TM_ENOBUFS
  #define ENOTCONN			TM_ENOTCONN
  #define ETIMEDOUT			TM_ETIMEDOUT
  #define HOST_NOT_FOUND	TM_DNS_ENAME_ERROR
  #define NO_ADDRESS		TM_DNS_EANSWER
  #define NO_DATA			TM_DNS_EANSWER	/* No equivalent in Treck stack */
  #define TRY_AGAIN			TM_EAGAIN
#endif /* Standard error names not defined */

/****************************************************************************
*																			*
*						 			uITRON									*
*																			*
****************************************************************************/

#elif defined( __ITRON__ )

/* uITRON has a TCP/IP API but it doesn't seem to be widely used, and the
   only available documentation is in Japanese.  If you need TCP/IP support
   under uITRON and have an implementation available, you can add the
   appropriate interface by replacing net_tcp.c and net_dns.c with the
   equivalent uITRON API glue code */

#error You need to set up the TCP/IP headers and interface in net_tcp.c/net_dns.c

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

#elif ( defined( __BEOS__ ) && defined( BONE_VERSION ) ) || \
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
   installed binaries) is still pretty rare */

#if defined( __APPLE__ ) && !defined( BIND_8_COMPAT )
  #define BIND_8_COMPAT
#endif /* Mac OS X without backwards-compatibility bind define */

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

/* AIX and SCO don't define sockaddr_storage in their IPv6 headers so we
   define a placeholder equivalent here */

#if defined( IPv6 ) && \
	( ( defined( _AIX ) && OSVERSION <= 5 ) || defined( __SCO_VERSION__ ) )
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

#elif defined( __VXWORKS__ )

/* VxWorks includes a (somewhat minimal) BSD sockets implementation, if
   you're using a newer, enhanced implementation or a third-party
   alternative with more complete functionality (and less bugs), you can use
   the generic Unix headers and defines further down */

#include <socket.h>
#include <selectLib.h>

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
*						 			Other Systems							*
*																			*
****************************************************************************/

#else

#error You need to set up OS-specific networking include handling in net_tcp.h

#endif /* OS-specific includes and defines */

/****************************************************************************
*																			*
*						 	General/Portability Defines						*
*																			*
****************************************************************************/

/* Now that we've included all of the networking headers, try and guess
   whether this is an IPv6-enabled system.  We can detect this by the
   existence of definitions for the EAI_xxx return values from
   getaddrinfo().  Note that we can't safely detect it using the more
   obvious AF_INET6 since many headers defined this in anticipation of IPv6
   long before the remaining code support was present */

#if defined( EAI_BADFLAGS ) && defined( EAI_NONAME )
  #define IPv6
#endif /* getaddrinfo() return values defined */

/* BeOS with the BONE network stack has the necessary IPv6 defines but no
   actual IPv6 support, so we disable it again */

#if defined( __BEOS__ ) && defined( BONE_VERSION ) && defined( IPv6 )
  #undef IPv6
#endif /* BeOS with BONE */

/* The size of a (v4) IP address and the number of IP addresses that we try
   to connect to for a given host, used if we're providing an emulated
   (IPv4-only) getaddrinfo() */

#define IP_ADDR_SIZE	4
#define IP_ADDR_COUNT	16

/* Test for common socket errors.  For isBadSocket() we don't just compare
   the socket to INVALID_SOCKET but perform a proper range check both to
   catch any problems with buggy implementations that may return something
   other than -1 to indicate an error, and because we're going to use the
   value in FD_xyz() macros which often don't perform any range checking,
   and a value outside the range 0...FD_SETSIZE can cause segfaults and
   other problems.  In addition we exclude stdin/stdout/stderr if they're
   present, since a socket with these handle values is somewhat suspicious,
   The one exception to this is Windows sockets, which don't use a Berkeley-
   type bitflag representation and therefore don't have the range problems 
   that the Berkeley implementation does */

#ifndef __WINDOWS__
  #define INVALID_SOCKET			-1
#endif /* __WINDOWS__ */
#if defined( __WINDOWS__ )
  #define isBadSocket( socket )		( ( socket ) <= 0 )
#elif defined( STDERR_FILENO )
  #define isBadSocket( socket )		( ( socket ) <= STDERR_FILENO || \
									  ( socket ) >= FD_SETSIZE )
#else
  #define isBadSocket( socket )		( ( socket ) <= 0 || \
									  ( socket ) >= FD_SETSIZE )
#endif /* STDERR_FILENO */
#ifdef __WINDOWS__
  #define isSocketError( status )	( ( status ) == SOCKET_ERROR )
  #define isBadAddress( address )	( ( address ) == INADDR_NONE )
#else
  #define isSocketError( status )	( ( status ) == -1 )
  #define isBadAddress( address )	( ( address ) == ( in_addr_t ) -1 )
#endif /* Windows vs. other systems */
#if defined( __SYMBIAN32__ )
  /* Symbian OS doesn't support nonblocking I/O */
  #define isNonblockWarning()		0
#elif defined( __BEOS__ )
  #if defined( BONE_VERSION )
	/* BONE returns "Operation now in progress" */
	#define isNonblockWarning()		( errno == EWOULDBLOCK || \
									  errno == 0x80007024 )
  #else
	/* BeOS, even though it supposedly doesn't support nonblocking
	   sockets, can return EWOULDBLOCK */
	#define isNonblockWarning()		( errno == EWOULDBLOCK )
  #endif /* BeOS with/without BONE */
#elif defined( __WINDOWS__ )
  #define isNonblockWarning()		( WSAGetLastError() == WSAEWOULDBLOCK )
#else
	#define isNonblockWarning()		( errno == EINPROGRESS )
#endif /* OS-specific socket error handling */

/* Error code handling */

#if defined( __WINDOWS__ )
  #define getErrorCode()			WSAGetLastError()
  #define getHostErrorCode()		WSAGetLastError()
#elif !defined( getErrorCode )
  #define getErrorCode()			errno
  #if ( defined( __MVS__ ) && defined( _OPEN_THREADS ) )
	/* MVS converts this into a hidden function in the presence of threads,
	   but not transparently like other systems */
	#define getHostErrorCode()		( *__h_errno() )
  #else
	#define getHostErrorCode()		h_errno
  #endif /* MVS */
#endif /* OS-specific error code handling */

/* Windows and BeOS use a distinct socket handle type and require the use of
   separate closesocket() and ioctlsocket() functions because socket handles
   aren't the same as standard Windows/BeOS handles */

#ifdef SOCKET
  /* MP-RAS has already defined this */
  #undef SOCKET
#endif /* SOCKET */
#define SOCKET						int
#if !defined( __WINDOWS__ ) && !defined( closesocket )
  #if !defined( __BEOS__ ) || \
	  ( defined( __BEOS__ ) && defined( BONE_VERSION ) )
	#define closesocket				close
  #endif /* BeOS without BONE */
  #define ioctlsocket				ioctl
#endif /* OS-specific portability defines */

/* The generic sockaddr struct used to reserve storage for protocol-specific
   sockaddr structs.  The IPv4 equivalent is given further down in the IPv6-
   mapping definitions */

#ifdef IPv6
  #define SOCKADDR_STORAGE			struct sockaddr_storage
#endif /* IPv6 */

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
#endif /* Older Unixen without in_*_t's */

/* The handling of size parameters to socket functions is, as with most
   things Unix, subject to random portability problems.  The traditional
   BSD sockets API used int for size parameters to socket functions, Posix 
   decided it'd be better to use size_t, but then people complained that 
   this wasn't binary-compatible with existing usage because on 64-bit
   systems size_t != int.  Instead of changing it back to int, Posix defined
   a new type, socklen_t, which may or may not be an int.  So some systems
   have int, some have size_t, some have socklen_t defined to int, and some
   have socklen_t defined to something else.  PHUX, as usual, is 
   particularly bad, defaulting to the BSD form with int unless you define 
   _XOPEN_SOURCE_EXTENDED, in which case you get socklen_t but it's mapped 
   to size_t without any change in the sockets API, which still expects int
   (the PHUX select() has a similar problem, see the comment in 
   random/unix.c).
   
   To resolve this, we try and use socklen_t if we detect its presence,
   otherwise we use int where we know it's safe to do so, and failing that 
   we fall back to size_t */

#if defined( socklen_t ) || defined( __socklen_t_defined )
  #define SIZE_TYPE					socklen_t
#elif defined( __APPLE__ ) || defined( __BEOS__ ) || defined( _CRAY ) || \
	  defined( __WINDOWS__ )
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

/* Older versions of QNX don't define HFIXEDSZ either */

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

#ifdef __WINDOWS__
  #define isRecoverableError( status )	( ( status ) == WSAECONNRESET )
  #define isRestartableError()			( WSAGetLastError() == WSAEWOULDBLOCK || \
										  WSAGetLastError() == WSAEINPROGRESS )
  #define isTimeoutError()				( WSAGetLastError() == WSAETIMEDOUT )
#else
  #define isRecoverableError( status )	( ( status ) == ECONNRESET )
  #define isRestartableError()			( errno == EINTR || errno == EAGAIN )
  #define isTimeoutError()				( errno == ETIMEDOUT )
#endif /* OS-specific status codes */

/****************************************************************************
*																			*
*						 		Resolver Defines							*
*																			*
****************************************************************************/

/* BeOS with the BONE network stack has just enough IPv6 defines present to
   be awkward, so we temporarily re-enable IPv6 and then use a BONE-specific
   subset of IPv6 defines further on */

#if defined( __BEOS__ ) && defined( BONE_VERSION )
  #define IPv6
#endif /* BeOS with BONE */

/* IPv6 emulation functions used to provide a single consistent interface */

#ifndef IPv6
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

  /* The generic sockaddr struct used to reserve storage for protocol-
     specific sockaddr structs.  This isn't quite right but since all
	 we're using it for is to reserve storage (we never actually look
	 inside it) it's OK to use here  */
  typedef char SOCKADDR_STORAGE[ 128 ];

  /* getaddrinfo() flags and values */
  #define AI_PASSIVE		0x1		/* Flag for hints are for getaddrinfo() */

  /* getnameinfo() flags and values.  We have to use slightly different
     values for these under Windows because Windows uses different values
	 for these than anyone else, and even if we're not on an explicitly
	 IPv6-enabled system we could still end up dynamically pulling in the
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
  #ifdef __WINDOWS__
	#define IPV6_V6ONLY		27		/* Force dual stack to use only IPv6 */
  #else
	#define IPV6_V6ONLY		26		/* Force dual stack to use only IPv6 */
  #endif /* __WINDOWS__ */

  /* If there's no getaddrinfo() available and we're not using dynamic
     linking, use an emulation of the function */
  #ifndef __WINDOWS__
	#define getaddrinfo		my_getaddrinfo
	#define freeaddrinfo	my_freeaddrinfo
	#define getnameinfo		my_getnameinfo

	static int my_getaddrinfo( const char *nodename, const char *servname,
							   const struct addrinfo *hints,
							   struct addrinfo **res );
	static void my_freeaddrinfo( struct addrinfo *ai );
	static int my_getnameinfo( const struct sockaddr *sa, SIZE_TYPE salen,
							   char *node, SIZE_TYPE nodelen,
							   char *service, SIZE_TYPE servicelen,
							   int flags );

	/* Windows uses the Pascal calling convention for these functions, we
	   hide this behind a define that becomes a no-op on non-Windows
	   systems */
	#define SOCKET_API
  #endif /* __WINDOWS__ */
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
#endif /* IPv6 */

/* A subset of the above for BeOS with the BONE network stack.  See the
   full IPv6 version above for descriptions of the entries */

#if defined( __BEOS__ ) && defined( BONE_VERSION )
  #undef IPv6						/* We really don't do IPv6 */

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
   required */

#if defined( USE_THREADS ) && defined( __GLIBC__ ) && ( __GLIBC__ >= 2 ) && \
	( !defined( __BEOS__ ) || !defined( BONE_VERSION ) )
  #define gethostbyname_vars() \
		  char hostBuf[ 4096 ]; \
		  struct hostent hostEnt;
  #define gethostbyname_threadsafe( hostName, hostEntPtr, hostErrno ) \
		  if( gethostbyname_r( hostName, &hostEnt, hostBuf, 4096, &hostEntPtr, &hostErrno ) < 0 ) \
			hostEntPtr = NULL
#elif defined( USE_THREADS ) && \
	  ( ( defined( sun ) && OSVERSION > 4 ) || \
		( defined( __sgi ) && OSVERSION >= 6 ) || defined( __QNX__ ) )
  #define gethostbyname_vars() \
		  char hostBuf[ 4096 ]; \
		  struct hostent hostEnt;
  #define gethostbyname_threadsafe( hostName, hostEntPtr, hostErrno ) \
		  hostEntPtr = gethostbyname_r( hostName, &hostEnt, hostBuf, 4096, &hostErrno )
#else
  #define gethostbyname_vars()
  #define gethostbyname_threadsafe( hostName, hostEntPtr, hostErrno ) \
		  hostEntPtr = gethostbyname( hostName ); \
		  hostErrno = h_errno;
#endif /* Various gethostbyname variants */

/****************************************************************************
*																			*
*						 	Non-blocking I/O Defines						*
*																			*
****************************************************************************/

/* The traditional way to set a descriptor to nonblocking mode was an
   ioctl with FIONBIO, however Posix prefers the O_NONBLOCK flag for fcntl
   so we use this if it's available.

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

#if defined( F_GETFL ) && defined( F_SETFL ) && defined( O_NONBLOCK )
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
			unsigned long nonBlock = FALSE; \
			value = ioctlsocket( socket, FIONBIO, &nonBlock ); \
			value = isSocketError( value ) ? TRUE : FALSE; \
			}
  #define setSocketNonblocking( socket ) \
			{ \
			unsigned long nonBlock = TRUE; \
			ioctlsocket( socket, FIONBIO, &nonBlock ); \
			}
  #define setSocketBlocking( socket ) \
			{ \
			unsigned long nonBlock = FALSE; \
			ioctlsocket( socket, FIONBIO, &nonBlock ); \
			}
#elif defined( __AMX__ ) || defined( __BEOS__ )
  #define getSocketNonblockingStatus( socket, value ) \
			{ \
			int nonBlock = FALSE; \
			setsockopt( socket, SOL_SOCKET, SO_NONBLOCK, &nonBlock, sizeof( int ) ); \
			}
  #define setSocketNonblocking( socket ) \
			{ \
			int nonBlock = TRUE; \
			setsockopt( socket, SOL_SOCKET, SO_NONBLOCK, &nonBlock, sizeof( int ) ); \
			}
  #define setSocketBlocking( socket ) \
			{ \
			int nonBlock = FALSE; \
			setsockopt( socket, SOL_SOCKET, SO_NONBLOCK, &nonBlock, sizeof( int ) ); \
			}
#elif defined( __SYMBIAN32__ )
  /* Symbian OS doesn't support nonblocking I/O */
  #define getSocketNonblockingStatus( socket, value )	value = 0
  #define setSocketNonblocking( socket )
  #define setSocketBlocking( socket )
#else
  #error Need to create macros to handle nonblocking I/O
#endif /* Handling of blocking/nonblocking sockets */

/****************************************************************************
*																			*
*						 	Misc.Functions and Defines						*
*																			*
****************************************************************************/

/* DNS dynamic-binding init/shutdown functions */

#ifdef __WINDOWS__
  CHECK_RETVAL \
  int initDNS( const INSTANCE_HANDLE hTCP, const INSTANCE_HANDLE hAddr );
  void endDNS( const INSTANCE_HANDLE hTCP );
  #ifdef USE_DNSSRV
	CHECK_RETVAL \
	int initDNSSRV( const INSTANCE_HANDLE hTCP );
	void endDNSSRV( const INSTANCE_HANDLE hTCP );
  #else
	#define initDNSSRV( hTCP )		CRYPT_ERROR
	#define endDNSSRV( hTCP )
  #endif /* USE_DNSSRV */
#endif /* __WINDOWS__ */

/* Prototypes for functions in dns.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAddressInfo( INOUT NET_STREAM_INFO *netStream, 
					OUT_PTR struct addrinfo **addrInfoPtrPtr,
					IN_BUFFER_OPT( nameLen ) const char *name, 
					IN_LENGTH_Z const int nameLen, 
					IN_PORT const int port, const BOOLEAN isServer );
STDC_NONNULL_ARG( ( 1 ) ) \
void freeAddressInfo( struct addrinfo *addrInfoPtr );
STDC_NONNULL_ARG( ( 1, 3, 5, 6 ) ) \
void getNameInfo( IN_BUFFER( sockAddrLen ) const void *sockAddr,
				  IN_LENGTH_SHORT_MIN( 8 ) const int sockAddrLen,
				  OUT_BUFFER( addressMaxLen, *addressLen ) \
				  char *address, IN_LENGTH_DNS const int addressMaxLen, 
				  OUT_LENGTH_DNS_Z int *addressLen, 
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

/* Prototypes for functions in tcp.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getSocketError( NET_STREAM_INFO *netStream, 
					IN_ERROR const int status,
					OUT_INT_Z int *socketErrorCode );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getHostError( NET_STREAM_INFO *netStream, IN_ERROR const int status );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setSocketError( INOUT NET_STREAM_INFO *netStream, 
					IN_BUFFER( errorMessageLength ) const char *errorMessage, 
					IN_LENGTH_ERRORMESSAGE const int errorMessageLength,
					IN_ERROR const int status, const BOOLEAN isFatal );

#endif /* USE_TCP */
