/****************************************************************************
*																			*
*					cryptlib TCP/IP Error Handling Routines					*
*						Copyright Peter Gutmann 1998-2017					*
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
*						 		Error Messages								*
*																			*
****************************************************************************/

#ifdef USE_ERRMSGS 

/* Map of common error codes to strings.  

   The error code supplied by the caller is usually used as the return 
   status code (in which case cryptSpecificCode is set to CRYPT_OK), however 
   if a more specific error code than the default is available then it's 
   specified via the cryptSpecificCode field */

typedef struct {
	const int errorCode;		/* Native error code */
	const int cryptSpecificCode;/* Specific cryptlib error code */
	const BOOLEAN isFatal;		/* Seriousness level */
	BUFFER_FIXED( errorStringLength ) \
	const char *errorString;
	const int errorStringLength;/* Error message */
	} SOCKETERROR_INFO;

#if defined( __WINDOWS__ )

static const SOCKETERROR_INFO socketErrorInfo[] = {
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

#elif defined( __embOS__ )

static const SOCKETERROR_INFO socketErrorInfo[] = {
	{ IP_ERR_MISC, CRYPT_OK, TRUE,
		"IP_ERR_MISC: Miscellaneous error", 32 },
	{ IP_ERR_TIMEDOUT, CRYPT_ERROR_TIMEOUT, FALSE,
		"IP_ERR_TIMEDOUT: Operation timed out", 36 },
	{ IP_ERR_ISCONN, CRYPT_OK, FALSE, 
		"IP_ERR_ISCONN: Socket is already connected", 42 },
	{ IP_ERR_OP_NOT_SUPP, CRYPT_ERROR_NOTAVAIL, TRUE,
		"IP_ERR_OP_NOT_SUPP: Operation not supported for selected socket", 63 },
	{ IP_ERR_CONN_ABORTED, CRYPT_OK, TRUE,
		"IP_ERR_CONN_ABORTED: Connection was aborted", 43 },
	{ IP_ERR_WOULD_BLOCK, CRYPT_OK, TRUE,
		"IP_ERR_WOULD_BLOCK: Socket is in non-blocking state and the current "
		"operation would block the socket if not in non-blocking state", 129 },
	{ IP_ERR_CONN_REFUSED, CRYPT_ERROR_PERMISSION, TRUE,
		"IP_ERR_CONN_REFUSED: Connection refused by peer", 47 },
	{ IP_ERR_CONN_RESET, CRYPT_OK, TRUE,
		"IP_ERR_CONN_RESET: Connection has been reset", 45 },
	{ IP_ERR_NOT_CONN, CRYPT_OK, TRUE,
		"IP_ERR_NOT_CONN: Socket is not connected", 40 },
	{ IP_ERR_ALREADY, CRYPT_OK, FALSE,
		"IP_ERR_ALREADY: Socket already is in the requested state", 57 },
	{ IP_ERR_IN_VAL, CRYPT_OK, TRUE,
		"IP_ERR_IN_VAL: Passed value for configuration is not valid", 58 },
	{ IP_ERR_MSG_SIZE, CRYPT_ERROR_OVERFLOW, FALSE,
		"IP_ERR_MSG_SIZE: Message is too big to send", 43 },
	{ IP_ERR_PIPE, CRYPT_OK, TRUE,
		"IP_ERR_PIPE: Socket is not in the correct state for this "
		"operation", 66 },
	{ IP_ERR_DEST_ADDR_REQ, CRYPT_ERROR_NOTFOUND, TRUE,
		"IP_ERR_DEST_ADDR_REQ: Destination address has not been specified", 64 },
	{ IP_ERR_SHUTDOWN, CRYPT_ERROR_COMPLETE, TRUE,
		"IP_ERR_SHUTDOWN: Connection has been closed as soon as all data "
		"has been received upon a FIN request", 100 },
	{ IP_ERR_NO_PROTO_OPT, CRYPT_OK, TRUE,
		"IP_ERR_NO_PROTO_OPT: Unknown option for setsockopt() or "
		"getsockopt()", 68 },
	{ IP_ERR_NO_MEM, CRYPT_ERROR_MEMORY, TRUE,
		"IP_ERR_NO_MEM: Not enough memory in the memory pool", 51 },
	{ IP_ERR_ADDR_NOT_AVAIL, CRYPT_ERROR_NOTFOUND, TRUE,
		"IP_ERR_ADDR_NOT_AVAIL: No known path to send to the specified "
		"address", 69 },
	{ IP_ERR_ADDR_IN_USE, CRYPT_OK, TRUE,
		"IP_ERR_ADDR_IN_USE: Socket already has a connection to this address "
		"and port or is already bound to this address", 112 },
	{ IP_ERR_IN_PROGRESS, CRYPT_OK, FALSE,
		"IP_ERR_IN_PROGRESS: Operation is still in progress", 50 },
	{ IP_ERR_NO_BUF, CRYPT_ERROR_MEMORY, TRUE,
		"IP_ERR_NO_BUF: No internal buffer was available", 47 },
	{ IP_ERR_NOT_SOCK, CRYPT_OK, TRUE,
		"IP_ERR_NOT_SOCK: Socket has already been opened or has already "
		"been closed", 74 },
	{ IP_ERR_FAULT, CRYPT_OK, TRUE,
		"IP_ERR_FAULT: Generic error for a failed operation", 50 },
	{ IP_ERR_NET_UNREACH, CRYPT_OK, TRUE,
		"IP_ERR_NET_UNREACH: No path to the desired network available", 61 },
	{ IP_ERR_PARAM, CRYPT_OK, TRUE,
		"IP_ERR_PARAM: Invalid parameter to function", 43 },
	{ IP_ERR_LOGIC, CRYPT_OK, TRUE,
		"IP_ERR_LOGIC: Logical error that should not have happened", 57 },
	{ IP_ERR_NOMEM, CRYPT_ERROR_MEMORY, TRUE,
		"IP_ERR_NOMEM: System error: No memory for requested operation", 61 },
	{ IP_ERR_NOBUFFER, CRYPT_ERROR_MEMORY, TRUE,
		"IP_ERR_NOBUFFER: System error: No internal buffer available for "
		"the requested operation", 87 },
	{ IP_ERR_RESOURCE, CRYPT_ERROR_MEMORY, TRUE,
		"IP_ERR_RESOURCE: System error: Not enough free resources available "
		"for the requested operation", 94 },
	{ IP_ERR_BAD_STATE, CRYPT_OK, TRUE,
		"IP_ERR_BAD_STATE: Socket is in an unexpected state", 50 },
	{ IP_ERR_TIMEOUT, CRYPT_ERROR_TIMEOUT, FALSE,
		"IP_ERR_TIMEOUT: Requested operation timed out", 46 },
	{ IP_ERR_NO_ROUTE, CRYPT_OK, TRUE,
		"IP_ERR_NO_ROUTE: Net error: Destination is unreachable", 54 },
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};
#define hostErrorInfo	socketErrorInfo		/* embOS uses unified error codes */

#elif defined( __Nucleus__ )

static const SOCKETERROR_INFO socketErrorInfo[] = {
	{ NU_INVALID_PROTOCOL, CRYPT_OK, TRUE,
		"NU_INVALID_PROTOCOL: Invalid network protocol", 45 },
	{ NU_NO_DATA_TRANSFER, CRYPT_OK, TRUE,
		"NU_NO_DATA_TRANSFER: Data was not written/read during send/receive "
		"function", 75 },
	{ NU_NO_PORT_NUMBER, CRYPT_OK, TRUE,
		"NU_NO_PORT_NUMBER: No local port number was stored in the socket "
		"descriptor", 75 },
	{ NU_NO_TASK_MATCH, CRYPT_OK, TRUE,
		"NU_NO_TASK_MATCH: No task/port number combination existed in the "
		"task table", 75 },
	{ NU_NO_SOCKET_SPACE, CRYPT_ERROR_OVERFLOW, TRUE,
		"NU_NO_SOCKET_SPACE: The socket structure list was full when a new "
		"socket descriptor was requested", 97 },
	{ NU_NO_ACTION, CRYPT_OK, TRUE,
		"NU_NO_ACTION: No action was processed by the function", 53 },
	{ NU_NOT_CONNECTED, CRYPT_OK, TRUE,
		"NU_NOT_CONNECTED: A connection has been closed by the network", 61 },
	{ NU_INVALID_SOCKET, CRYPT_OK, TRUE,
		"NU_INVALID_SOCKET: The socket ID passed in was not in a valid "
		"range", 67 },
	{ NU_NO_SOCK_MEMORY, CRYPT_OK, TRUE,
		"NU_NO_SOCK_MEMORY: Memory allocation failed for internal sockets "
		"structure", 64 },
	{ NU_INVALID_ADDRESS, CRYPT_OK, TRUE,
		"NU_INVALID_ADDRESS: An incomplete address was sent", 50 },
	{ NU_NO_HOST_NAME, CRYPT_OK, TRUE,
		"NU_NO_HOST_NAME: No host name specified in a connect call where a "
		"machine was not previously set up", 99 },
	{ NU_RARP_INIT_FAILED, CRYPT_OK, TRUE,
		"NU_RARP_INIT_FAILED: During initialization RARP failed", 54 },
	{ NU_BOOTP_INIT_FAILED, CRYPT_OK, TRUE,
		"NU_BOOTP_INIT_FAILED: During initialization BOOTP failed", 56 },
	{ NU_INVALID_PORT, CRYPT_OK, TRUE,
		"NU_INVALID_PORT: The port number passed in was not in a valid "
		"range", 67 },
	{ NU_NO_BUFFERS, CRYPT_OK, TRUE,
		"NU_NO_BUFFERS: There were no buffers to place the outgoing packet "
		"in", 68 },
	{ NU_NOT_ESTAB, CRYPT_OK, TRUE,
		"NU_NOT_ESTAB: A connection is open but not in an established state", 66 },
	{ NU_WINDOW_FULL, CRYPT_OK, TRUE,
		"NU_WINDOW_FULL: The foreign host's in window is full", 52 },
	{ NU_NO_SOCKETS, CRYPT_OK, TRUE,
		"NU_NO_SOCKETS: No sockets were specified", 40 },
	{ NU_NO_DATA, CRYPT_OK, TRUE,
		"NU_NO_DATA: None of the specified sockets were data ready", 57 },
	/* NU_Setsockopt()/NU_Getsockopt() errors */
	{ NU_INVALID_LEVEL, CRYPT_OK, TRUE,
		"NU_INVALID_LEVEL: The specified level is invalid", 48 },
	{ NU_INVALID_OPTION, CRYPT_OK, TRUE,
		"NU_INVALID_OPTION: The specified option is invalid", 50 },
	{ NU_INVAL, CRYPT_OK, TRUE,
		"NU_INVAL: General purpose error condition", 41 },
	{ NU_ACCESS, CRYPT_OK, TRUE,
		"NU_ACCESS: The attempted operation is not allowed on the socket", 63 },
	/* Standard socket errors again */
	{ NU_ADDRINUSE, CRYPT_OK, TRUE,
		"NU_ADDRINUSE: The IP Multicast membership already exists", 56 },
	{ NU_HOST_UNREACHABLE, CRYPT_OK, TRUE,
		"NU_HOST_UNREACHABLE: Host unreachable", 37 },
	{ NU_MSGSIZE, CRYPT_OK, TRUE,
		"NU_MSGSIZE: Packet is to large for interface", 44 },
	{ NU_NOBUFS, CRYPT_OK, TRUE,
		"NU_NOBUFS: Could not allocate a memory buffer", 45 },
	{ NU_UNRESOLVED_ADDR, CRYPT_OK, TRUE,
		"NU_UNRESOLVED_ADDR: The MAC address was not resolved", 52 },
	{ NU_CLOSING, CRYPT_OK, TRUE,
		"NU_CLOSING: The other side in a TCP connection has sent a FIN", 61 },
	{ NU_MEM_ALLOC, CRYPT_OK, TRUE,
		"NU_MEM_ALLOC: Failed to allocate memory", 39 },
	{ NU_RESET, CRYPT_OK, TRUE,
		"NU_RESET: A multicast membership was added and the MAC chip needs "
		"to be reset", 77 },
	{ NU_DEVICE_DOWN, CRYPT_OK, TRUE,
		"NU_DEVICE_DOWN: A device being used by the socket has gone down", 63 },
	/* DNS errors */
	{ NU_INVALID_LABEL, CRYPT_OK, TRUE,
		"NU_INVALID_LABEL: Domain name with an invalid label", 51 },
	{ NU_FAILED_QUERY, CRYPT_OK, TRUE,
		"NU_FAILED_QUERY: No response received for a DNS Query", 53 },
	{ NU_DNS_ERROR, CRYPT_OK, TRUE,
		"NU_DNS_ERROR: A general DNS error status", 40 },
	{ NU_NOT_A_HOST, CRYPT_OK, TRUE,
		"NU_NOT_A_HOST: The host name was not found", 42 },
	{ NU_INVALID_PARM, CRYPT_OK, TRUE,
		"NU_INVALID_PARM: A parameter has an invalid value", 49 },
	{ NU_NO_DNS_SERVER, CRYPT_OK, TRUE,
		"NU_NO_DNS_SERVER: No DNS server has been registered with the "
		"stack", 66 },
	/* Standard socket errors again */
	{ NU_NO_ROUTE_TO_HOST, CRYPT_OK, TRUE,
		"NU_NO_ROUTE_TO_HOST: ICMP Destination Unreachable specific "
		"error", 64 },
	{ NU_CONNECTION_REFUSED, CRYPT_OK, TRUE,
		"NU_CONNECTION_REFUSED: ICMP Destination Unreachable specific "
		"error", 66 },
	{ NU_MSG_TOO_LONG, CRYPT_OK, TRUE,
		"NU_MSG_TOO_LONG: ICMP Destination Unreachable specific error", 60 },
	{ NU_BAD_SOCKETD, CRYPT_OK, TRUE,
		"NU_BAD_SOCKETD: Socket descriptor is not valid for the current "
		"operation", 72 },
	{ NU_BAD_LEVEL, CRYPT_OK, TRUE,
		"NU_BAD_LEVEL: ???", 17 },
	{ NU_BAD_OPTION, CRYPT_OK, TRUE,
		"NU_BAD_OPTION: ???", 18 },
	/* IPv6 errors */
	{ NU_DUP_ADDR_FAILED, CRYPT_OK, TRUE,
		"NU_DUP_ADDR_FAILED: ???", 23 },
	{ NU_DISCARD_PACKET, CRYPT_OK, TRUE,
		"NU_DISCARD_PACKET: ???", 22 },
	/* ICMP errors */
	{ NU_DEST_UNREACH_ADMIN, CRYPT_OK, TRUE,
		"NU_DEST_UNREACH_ADMIN: ICMP Destination Unreachable: Packet was "
		"rejected due to administration reasons", 102 },
	{ NU_DEST_UNREACH_ADDRESS, CRYPT_OK, TRUE,
		"NU_DEST_UNREACH_ADDRESS: ICMP Destination Unreachable: Packet was "
		"rejected because destination address doesn't match an address on "
		"the node", 139 },
	{ NU_DEST_UNREACH_PORT, CRYPT_OK, TRUE,
		"NU_DEST_UNREACH_PORT: ICMP Destination Unreachable: Destination "
		"port is not listening on the node", 97 },
	{ NU_TIME_EXCEED_HOPLIMIT, CRYPT_OK, TRUE,
		"NU_TIME_EXCEED_HOPLIMIT: ICMP Time Exceeded: Packet has exceeded "
		"the number of hops that it may make", 100 },
	{ NU_TIME_EXCEED_REASM, CRYPT_OK, TRUE,
		"NU_TIME_EXCEED_REASM: ICMP Time Exceeded: Packet could not be "
		"reassembled in the maximum allowable time", 103 },
	{ NU_PARM_PROB_HEADER, CRYPT_OK, TRUE,
		"NU_PARM_PROB_HEADER: ICMP Parameter Problem: Packet has an error "
		"in the IP header", 81 },
	{ NU_PARM_PROB_NEXT_HDR, CRYPT_OK, TRUE,
		"NU_PARM_PROB_NEXT_HDR: ICMP Parameter Problem: Packet has an "
		"invalid next header value in the IPv6 header", 105 },
	{ NU_PARM_PROB_OPTION, CRYPT_OK, TRUE,
		"NU_PARM_PROB_OPTION: ICMP Parameter Problem: Invalid option "
		"specified in the IP header", 86 },
	{ NU_DEST_UNREACH_NET, CRYPT_OK, TRUE,
		"NU_DEST_UNREACH_NET: ICMP Destination Unreachable: Network is "
		"unreachable", 73 },
	{ NU_DEST_UNREACH_HOST, CRYPT_OK, TRUE,
		"NU_DEST_UNREACH_HOST: ICMP Destination Unreachable: Host is "
		"unreachable", 71 },
	{ NU_DEST_UNREACH_PROT, CRYPT_OK, TRUE,
		"NU_DEST_UNREACH_PROT: ICMP Destination Unreachable: Protocol is "
		"not recognized on the node", 90 },
	{ NU_DEST_UNREACH_FRAG, CRYPT_OK, TRUE,
		"NU_DEST_UNREACH_FRAG: ICMP Destination Unreachable: Packet "
		"requires fragmentation but the node does not support "
		"fragmentation", 125 },
	{ NU_DEST_UNREACH_SRCFAIL, CRYPT_OK, TRUE,
		"NU_DEST_UNREACH_SRCFAIL:  ICMP Destination Unreachable: Source "
		"route failed", 75 },
	{ NU_PARM_PROB, CRYPT_OK, TRUE,
		"NU_PARM_PROB: ICMP Parameter Problem: Packet has an error in the "
		"IP header", 74 },
	{ NU_SOURCE_QUENCH, CRYPT_OK, TRUE,
		"NU_SOURCE_QUENCH: ICMP Source Quench: Node is receiving too many "
		"packets to process", 83 },
	/* Nonblocking socket operation errors */
	{ NU_WOULD_BLOCK, CRYPT_OK, TRUE,
		"NU_WOULD_BLOCK: Socket is non-blocking but blocking is required to "
		"complete the requested action", 96 },
	/* TCP Keepalive errors */
	{ NU_CONNECTION_TIMED_OUT, CRYPT_OK, TRUE,
		"NU_CONNECTION_TIMED_OUT: Connection has been closed due to TCP "
		"Keepalive probes not being answered", 98 },
	/* Nonblocking connect errors */
	{ NU_IS_CONNECTING, CRYPT_OK, TRUE,
		"NU_IS_CONNECTING: Socket is non-blocking and the connection is "
		"being established", 80 },
	/* Standard socket errors again */
	{ NU_SOCKET_CLOSED, CRYPT_OK, TRUE,
		"NU_SOCKET_CLOSED: The specified socket has been closed", 54 },
	{ NU_TABLE_FULL, CRYPT_OK, TRUE,
		"NU_TABLE_FULL: ???", 18 },
	{ NU_NOT_FOUND, CRYPT_OK, TRUE,
		"NU_NOT_FOUND: ???", 17 },
	/* IPv6 extension header errors */
	{ NU_INVAL_NEXT_HEADER, CRYPT_OK, TRUE,
		"NU_INVAL_NEXT_HEADER: ???", 25 },
	{ NU_SEND_ICMP_ERROR, CRYPT_OK, TRUE,
		"NU_SEND_ICMP_ERROR: ???", 23 },
	/* Multicast errors */
	{ NU_MULTI_TOO_MANY_SRC_ADDRS, CRYPT_OK, TRUE,
		"NU_MULTI_TOO_MANY_SRC_ADDRS: Number of source addresses specified "
		"for multicast IP address filtering exceeds "
		"MAX_MULTICAST_SRC_ADDR", 131 },
	{ NU_NOT_A_GROUP_MEMBER, CRYPT_OK, TRUE,
		"NU_NOT_A_GROUP_MEMBER: Socket is not a member of the multicast "
		"group specified", 78 },
	{ NU_TOO_MANY_GROUP_MEMBERS, CRYPT_OK, TRUE,
		"NU_TOO_MANY_GROUP_MEMBERS: Number of multicast groups has been "
		"reached", 70 },
	/* Physical layer errors */
	{ NU_ETH_CABLE_UNPLUGGED, CRYPT_OK, TRUE,
		"NU_ETH_CABLE_UNPLUGGED: Ethernet cable is unplugged", 51 },
	{ NU_ETH_CABLE_PLUGGED_IN, CRYPT_OK, TRUE,
		"NU_ETH_CABLE_PLUGGED_IN: Ethernet cable has been plugged in", 59 },
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};
#define hostErrorInfo	socketErrorInfo		/* Nucleus uses unified error codes */

#elif defined( __MQXRTOS__ )

static const SOCKETERROR_INFO socketErrorInfo[] = {
	/* Generic Posix-equivalent errors.  Some of these vales are only present 
	   for ornamental purposes (the more specific RTCS_xxx values are the 
	   ones that actually get used) so we only map a subset of the more-
	   important ones */
	{ MQX_EACCES, CRYPT_ERROR_PERMISSION, TRUE,
		"MQX_EACCES: Permission denied", 29 },
	{ MQX_EBADF, CRYPT_OK, FALSE,
		"MQX_EBADF: Bad file descriptor", 30 },
	{ MQX_EMFILE, CRYPT_OK, FALSE,
		"MQX_EMFILE: Per-process descriptor table is full", 48 },
	{ MQX_ENODEV, CRYPT_OK, TRUE,
		"MQX_ENODEV: No such device", 26 },
	{ MQX_EPERM, CRYPT_ERROR_PERMISSION, TRUE,
		"MQX_EPERM: Operation not permitted", 34 },
	/* RTCS generic errors */
	{ RTCSERR_OUT_OF_MEMORY, CRYPT_ERROR_MEMORY, FALSE,
		"RTCSERR_OUT_OF_MEMORY: Insufficient system resources available to "
		"complete the call", 83 },
	{ RTCSERR_OUT_OF_BUFFERS, CRYPT_ERROR_MEMORY, FALSE,
		"RTCSERR_OUT_OF_BUFFERS: Insufficient system resources available to "
		"complete the call", 84 },
	{ RTCSERR_TIMEOUT, CRYPT_ERROR_TIMEOUT, FALSE,
		"RTCSERR_TIMEOUT: Function timed out before completion", 53 },
	/* IP-level errors */
	{ RTCSERR_IP_UNREACH, CRYPT_OK, FALSE,
		"RTCSERR_IP_UNREACH: No route to the network or host is "
		"present", 62 },
	{ RTCSERR_IP_TTL, CRYPT_ERROR_OVERFLOW, FALSE,
		"RTCSERR_IP_TTL: TTL expired", 27 },
	{ RTCSERR_IP_SMALLMTU, CRYPT_ERROR_OVERFLOW, FALSE,
		"RTCSERR_IP_SMALLMTU: Message is too large to be sent all at "
		"once", 64 },
	{ RTCSERR_IP_CANTFRAG, CRYPT_ERROR_OVERFLOW, FALSE,
		"RTCSERR_IP_CANTFRAG: Need to fragment but DF bit set", 52 },
	/* "TCP/IP" errors (c.f. "TCP" errors) */
	{ RTCSERR_TCPIP_NO_BUFFS /* = missing B52s */, CRYPT_ERROR_MEMORY, TRUE,
	  "RTCSERR_TCPIP_NO_BUFFS: No buffer space available", 49 },
	/* "TCP" errors (c.f. "TCP/IP" errors */
	{ RTCSERR_TCP_OPEN_FAILED, CRYPT_ERROR_OPEN, TRUE,
		"RTCSERR_TCP_OPEN_FAILED: TcpOpen failed", 39 },
	{ RTCSERR_TCP_IN_PROGRESS, CRYPT_OK, FALSE,
		"RTCSERR_TCP_IN_PROGRESS: Connection already in progress", 55 },
	{ RTCSERR_TCP_ADDR_IN_USE, CRYPT_OK, TRUE,
		"RTCSERR_TCP_ADDR_IN_USE: Address in use", 39 },
	{ RTCSERR_TCP_ADDR_NA, CRYPT_ERROR_NOTFOUND, TRUE,
		"RTCSERR_TCP_ADDR_NA: Specified address is not available from the "
		"local machine", 78 },
	{ RTCSERR_TCP_CONN_ABORTED, CRYPT_OK, TRUE,
		"RTCSERR_TCP_CONN_ABORTED: Software caused connection abort", 58 },
	{ RTCSERR_TCP_CONN_RESET, CRYPT_OK, TRUE,
		"RTCSERR_TCP_CONN_RESET: Connection was forcibly closed by remote "
		"host", 69 },
	{ RTCSERR_TCP_HOST_DOWN, CRYPT_OK, TRUE,
		"RTCSERR_TCP_HOST_DOWN: Host is down", 35 },
	{ RTCSERR_TCP_NOT_CONN, CRYPT_OK, TRUE,
		"RTCSERR_TCP_NOT_CONN: Socket is not connected", 45 },
	{ RTCSERR_TCP_TIMED_OUT, CRYPT_ERROR_MEMORY, TRUE,
	  "RTCSERR_TCP_TIMED_OUT: Operation timed out", 42 },
	{ RTCSERR_TCP_CONN_REFUSED, CRYPT_ERROR_PERMISSION, TRUE,
		"RTCSERR_TCP_CONN_REFUSED: Attempt to connect was rejected", 57 },
	{ RTCSERR_TCP_HOST_UNREACH, CRYPT_OK, FALSE,
		"RTCSERR_TCP_HOST_UNREACH: No route to the network or host is "
		"present", 68 },
	/* Socket errors */
	{ RTCSERR_SOCK_INVALID, CRYPT_OK, TRUE,
		"RTCSERR_SOCK_INVALID: Not a socket", 39 },
	{ RTCSERR_SOCK_INVALID_AF, CRYPT_ERROR_NOTAVAIL, TRUE,
		"RTCSERR_SOCK_INVALID_AF: Address family not supported", 53 },
	{ RTCSERR_SOCK_NOT_CONNECTED, CRYPT_OK, TRUE,
		"RTCSERR_SOCK_NOT_CONNECTED: Socket is not connected", 51 },
	{ RTCSERR_SOCK_IS_CONNECTED, CRYPT_OK, FALSE,
		"RTCSERR_SOCK_IS_CONNECTED: Socket is connected", 46 },
	{ RTCSERR_SOCK_EBADF, CRYPT_OK, FALSE,
		"RTCSERR_SOCK_EBADF: Bad file descriptor", 44 },
	/* DNS errors */
	{ RTCSERR_DNS_NO_NAME_SERVER_RESPONSE, CRYPT_OK, TRUE,
		"RTCSERR_DNS_NO_NAME_SERVER_RESPONSE: No name server response", 60 },
	{ RTCSERR_DNS_NO_RESPONSE_FROM_RESOLVER, CRYPT_OK, TRUE,
		"RTCSERR_DNS_NO_RESPONSE_FROM_RESOLVER: No response from resolver", 64 },
	{ RTCSERR_DNS_INVALID_NAME, CRYPT_OK, TRUE,
		"RTCSERR_DNS_INVALID_NAME: Invalid DNS name", 42 },
	{ RTCSERR_DNS_ALL_SERVERS_QUERIED, CRYPT_OK, TRUE,
		"RTCSERR_DNS_ALL_SERVERS_QUERIED: No response after all servers "
		"queried", 70 },
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};
#define hostErrorInfo	socketErrorInfo		/* MQX uses unified error codes */

#else

static const SOCKETERROR_INFO socketErrorInfo[] = {
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
#ifndef __Telit__
	{ EIO, CRYPT_OK, TRUE,
		"EIO: Input/output error", 24 },
#endif /* Telit */
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
#ifndef __Telit__
	{ ENODEV, CRYPT_OK, TRUE,
		"ENODEV: No such device", 22 },
#endif /* Telit */
	{ ENOPROTOOPT, CRYPT_OK, TRUE,
		"ENOPROTOOPT: Protocol not available", 35 },
	{ ENOTCONN, CRYPT_OK, TRUE,
		"ENOTCONN: Socket is not connected", 33 },
	{ ENOTSOCK, CRYPT_OK, TRUE,
		"ENOTSOCK: Not a socket", 22 },
#endif /* Symbian OS */
#ifndef __Telit__
	{ EPERM, CRYPT_ERROR_PERMISSION, TRUE,
		"EPERM: Operation not permitted", 30 },
	{ ENOMEM, CRYPT_ERROR_MEMORY, TRUE,
		"ENOMEM: Out of memory", 21 },
#endif /* Telit */
	{ EPROTOTYPE, CRYPT_ERROR_NOTAVAIL, TRUE,
		"EPROTOTYPE: Protocol wrong type for socket", 42 },
	{ ETIMEDOUT, CRYPT_ERROR_TIMEOUT, FALSE,
		"ETIMEDOUT: Function timed out before completion", 47 },
#if !( defined( __Quadros__ ) || defined( __Telit__ ) )
	{ HOST_NOT_FOUND, CRYPT_ERROR_NOTFOUND, TRUE,
		"HOST_NOT_FOUND: Not an official hostname or alias", 49 },
  #ifndef __ECOS__ 
	{ NO_ADDRESS, CRYPT_ERROR_NOTFOUND, TRUE,
		"NO_ADDRESS: Name is valid but does not have an IP address at the "
		"name server", 76 },
  #endif /* !ECOS */
	{ TRY_AGAIN, CRYPT_OK, FALSE,
		"TRY_AGAIN: Local server did not receive a response from an "
		"authoritative server", 79 },
#endif /* Quadros || Telit */
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};

static const SOCKETERROR_INFO hostErrorInfo[] = {
#if !( defined( __Quadros__ ) || defined( __Telit__ ) )
	{ HOST_NOT_FOUND, CRYPT_ERROR_NOTFOUND, TRUE,
		"HOST_NOT_FOUND: Host not found", 30 },
  #ifndef __ECOS__ 
	{ NO_ADDRESS, CRYPT_ERROR_NOTFOUND, TRUE,
		"NO_ADDRESS: No address record available for this name", 53 },
  #endif /* !ECOS */
	{ NO_DATA, CRYPT_ERROR_NOTFOUND, TRUE,
		"NO_DATA: Valid name, no data record of requested type", 53 },
	{ TRY_AGAIN,  CRYPT_OK, FALSE,
		"TRY_AGAIN: Local server did not receive a response from an "
		"authoritative server", 79 },
#endif /* Quadros || Telit */
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};
#endif /* System-specific socket error codes */

/****************************************************************************
*																			*
*						 Error Lookup/Mapping Functions						*
*																			*
****************************************************************************/

/* Get and set the low-level error information from a socket- and host-
   lookup-based error.  In theory under Windows we could also use the 
   Network List Manager to try and get additional diagnostic information 
   but this requires playing with COM objects, the significant extra 
   complexity caused by this isn't worth the tiny additional level of 
   granularity that we might gain in reporting errors.  In any case the NLM 
   functionality seems primarily intended for interactively obtaining 
   information before any networking actions are initiated ("Do we currently 
   have an Internet connection?") rather than diagnosing problems afterwards 
   ("What went wrong with the attempt to initiate an Internet connection?") */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int mapNetworkError( NET_STREAM_INFO *netStream, 
					 const int netStreamErrorCode,
					 const BOOLEAN useHostErrorInfo, 
					 IN_ERROR int status )
	{
	const SOCKETERROR_INFO *errorInfo = \
					useHostErrorInfo ? hostErrorInfo : socketErrorInfo;
	const int errorInfoSize = useHostErrorInfo ? \
					FAILSAFE_ARRAYSIZE( hostErrorInfo, SOCKETERROR_INFO ) : \
					FAILSAFE_ARRAYSIZE( socketErrorInfo, SOCKETERROR_INFO );
	int i, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( useHostErrorInfo == TRUE || useHostErrorInfo == FALSE );
	REQUIRES( cryptStatusError( status ) );

	clearErrorString( &netStream->errorInfo );

	/* If there's no further error information available then we can't 
	   report any more detail */
	if( netStreamErrorCode == 0 )
		{
		retExt( status,
				( status, NETSTREAM_ERRINFO, 
				  "Networking error code = 0, no error information "
				  "available" ) );
		}

	/* Try and find a specific error message for this error code */
	LOOP_LARGE( i = 0, i < errorInfoSize && \
					   errorInfo[ i ].errorCode != CRYPT_ERROR, i++ )
		{
		if( errorInfo[ i ].errorCode != netStreamErrorCode )
			continue;

		/* We've found matching error information, return it */
		REQUIRES( errorInfo[ i ].errorStringLength > 10 && \
				  errorInfo[ i ].errorStringLength < 150 );
		setErrorString( NETSTREAM_ERRINFO, errorInfo[ i ].errorString, 
						errorInfo[ i ].errorStringLength );
		if( errorInfo[ i ].cryptSpecificCode != CRYPT_OK )
			{
			/* There's a more specific error code than the generic one that 
			   we've been given available, use that instead */
			status = errorInfo[ i ].cryptSpecificCode;
			}
		if( errorInfo[ i ].isFatal )
			{
			/* It's a fatal error, make it persistent for the stream */
			netStream->persistentStatus = status;
			}
			
		return( status );
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( i < errorInfoSize );

	/* The error code doesn't correspond to anything that we can handle or
	   report on, the best that we can do is report the low-level error 
	   code directly.  Felix, qui potuit rerum cognoscere causas - Virgil */
	retExt( status, 
			( status, NETSTREAM_ERRINFO,
			  "Networking error code = %d, no additional information "
			  "available", netStreamErrorCode ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getSocketError( NET_STREAM_INFO *netStream, 
					IN_ERROR const int status,
					OUT_INT_Z int *socketErrorCode )
	{
	const int errorCode = getErrorCode( netStream->netSocket );

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtr( socketErrorCode, sizeof( int ) ) );

	REQUIRES( cryptStatusError( status ) );

	/* Get the low-level error code and map it to an error string if
	   possible */
	*socketErrorCode = errorCode;

	return( mapNetworkError( netStream, errorCode, FALSE, status ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getHostError( NET_STREAM_INFO *netStream, 
				  IN_ERROR const int status )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( cryptStatusError( status ) );

	/* Get the low-level error code and map it to an error string if
	   possible */
	return( mapNetworkError( netStream, 
							 getHostErrorCode( netStream->netSocket ), TRUE, 
							 status ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setSocketError( INOUT NET_STREAM_INFO *netStream, 
					IN_BUFFER( errorMessageLength ) const char *errorMessage, 
					IN_LENGTH_ERRORMESSAGE const int errorMessageLength,
					IN_ERROR const int status, 
					const BOOLEAN isFatal )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtr( errorMessage, 16 ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( errorMessageLength > 16 && \
			  errorMessageLength <= MAX_INTLENGTH_SHORT );
			  /* MAX_ERRORMESSAGE_SIZE isn't defined at this level */
	REQUIRES( cryptStatusError( status ) );
	REQUIRES( isFatal == TRUE || isFatal == FALSE );

	/* Set a cryptlib-supplied socket error message */
	setErrorString( NETSTREAM_ERRINFO, errorMessage, errorMessageLength );
	if( isFatal )
		{
		/* It's a fatal error, make it persistent for the stream */
		netStream->persistentStatus = status;
		}
	return( status );
	}
#endif /* USE_ERRMSGS */

/****************************************************************************
*																			*
*							Network Diganostic Routines						*
*																			*
****************************************************************************/

/* If a socket open fails we generally can't provide much information to the 
   caller beyond "socket open failed".  This occurs for two reasons, the 
   first being that the network stack dumbs down a lot of the lower-level 
   error information that's returned in the case of a problem, for example 
   ICMP error codes 0, 1, and 5-12 are all combined into "No route to host", 
   2 and 3 both become "Connection refused", and parameter problem 
   notifications all become "Protocol not available".  The second reason is 
   that the caller may be connecting to the wrong port or some similar
   operator error.
   
   To deal with this we perform an opportunistic ping of the first address 
   associated with the name to see if the ICMP reply can tell us more about 
   what's wrong */

#ifdef USE_RAW_SOCKETS

#define getIPVersion( value )		( ( ( value ) & 0xF0 ) >> 4 )
#define getIP4HeaderLength( value )	( ( ( value ) & 0x0F ) << 2 )
#define PACKET_OFFSET_IPVERSION		0	/* Offset of IP version field */
#define PACKET_OFFSET_NEXTHEADER	6	/* Offset of IPv6 next-header field */
#define PACKET_OFFSET_PROTOCOL		9	/* Offset of IPv4 protocol field */
#define IP4_MIN_HEADERSIZE			20	/* Minimum IPv4 header size */
#define IP6_HEADERSIZE				40	/* IPv6 header size */
#define ICMP_MIN_PACKETSIZE			8	/* Minimum ICMP packet size */
#define ICMP_TYPE_ECHO_REPLY		0	/* ICMP packet type = echo reply */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 6 ) ) \
static int pingHost( INOUT NET_STREAM_INFO *netStream,
					 IN_BUFFER( addressInfoLength ) const void *addressInfo,
					 IN_LENGTH_SHORT const int addressInfoLength,
					 OUT_BUFFER( bufMaxSize, *bufLen ) BYTE *buffer, 
					 IN_LENGTH_SHORT_MIN( 64 ) const int bufMaxSize, 
					 OUT_LENGTH_BOUNDED_Z( bufMaxSize ) int *bufLen )
	{
	static const BYTE pingPacket[] = {
		0x08,			/* Type 8 = Echo request */
		0x00,			/* Code 0 */
		0xF7, 0xFF,		/* Checksum, ~0x0800 */
		0x00, 0x00,		/* Unique ID */
		0x00, 0x00		/* Sequence number */
		};
	struct sockaddr recvAddr;
	SIZE_TYPE recvAddrSize = sizeof( struct sockaddr );
	int length, status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtrDynamic( addressInfo, addressInfoLength ) );
	assert( isWritePtrDynamic( buffer, bufMaxSize ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( isShortIntegerRangeNZ( addressInfoLength ) );
	REQUIRES( bufMaxSize >= 64 && bufMaxSize < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( buffer, 0, min( 16, bufMaxSize ) );
	*bufLen = 0;

	/* Send a rudimentary ping packet to the remote system.  Apart from 
	   making the checksum calculation easier, the minimal packet size also
	   means that we sidestep any potential MTU/fragmentation issues.  Note 
	   that we don't connect() the remote system's port and address to the 
	   socket (if that's even possible for a raw/ICMP socket) because we 
	   want to receive all ICMP messages sent to us, not just something 
	   returned from the specific remote system that we're targeting */
	status = sendto( netStream->netSocket, pingPacket, 8, 0, 
					 addressInfo, addressInfoLength );
	if( isSocketError( status ) )
		return( CRYPT_ERROR_WRITE );

	/* Try and read the ping response.  We don't go to too much trouble in 
	   terms of handling exception conditions here since this is an 
	   opportunistic check only, so we only send a single ping with a 10s
	   timeout.

	   The details of what we get back for a raw socket get rather complex.  
	   In general raw sockets can end up receiving most non-TCP/UDP packets
	   (that is, most ICMP/IGMP and all unknown-protocol packets), however
	   by explicitly selecting IPPROTO_ICMP we've told the kernel that we
	   only want to get ICMP packets.  However since we haven't bound the 
	   socket to a remote address (see the comment for sendto() above) we're 
	   going to get copies of all ICMP packets that arrive, not just our 
	   ones.  This makes things a bit complicated since we can't easily 
	   tell whether an incoming ICMP error packet from an address other than 
	   the target address is from (say) a router telling us that our ping
	   can't get through or something related to network traffic from 
	   another process on the system.
	   
	   Under IPv6 we could perform additional filtering with:

		#include <netinet/icmp6.h>

		struct icmp6_filter filter;

		ICMP6_FILTER_SETBLOCKALL( &filter );
		ICMP6_FILTER_SETPASS( ND_xxx, &filter );
		setsockopt( socket, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, 
					sizeof( filter ) );

	   Even then though we have to be careful because there's a race 
	   condition, ICMP packets that arrive between the socket() and 
	   setsockopt() will be enqueued for the raw socket, the filtering is a 
	   performance optimisation rather than an absolute block-list */
	status = ioWait( netStream, 10, TRUE, IOWAIT_READ );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the response data.  For the reason given above, we only try and 
	   read the first 512 bytes of response, ignore ICMP checksum problems 
	   (other layers should be taking care of this, IPv6 removes the 
	   checksums entirely for this reason), and don't check whether the 
	   response came from the intended source (which we could do by 
	   comparing addrInfoCursor->ai_addr->sa_data with recvAddr.sa_data) 
	   since it could be an ICMP error message from an intermediate system.  
	   Performing this address check would also cause problems if the server 
	   was multihomed, so if the incoming socket is bound to the wildcard 
	   address and our packet is sent to a non-primary address (alias) but 
	   comes back from the primary address then we'll appear to have a 
	   non-match */
	memset( buffer, 0, bufMaxSize );
	status = length = recvfrom( netStream->netSocket, buffer, bufMaxSize, 0, 
								&recvAddr, &recvAddrSize );
	if( isSocketError( status ) )
		return( CRYPT_ERROR_READ );
	if( length < IP4_MIN_HEADERSIZE + ICMP_MIN_PACKETSIZE || \
		length > 512 )
		return( CRYPT_ERROR_BADDATA );	

	*bufLen = length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processICMP( IN_BUFFER( icmpPacketLength ) const BYTE *icmpPacket,
						IN_LENGTH_SHORT_MIN( IP4_MIN_HEADERSIZE + ICMP_MIN_PACKETSIZE ) \
							const int icmpPacketLength,
						OUT_LENGTH_BOUNDED_Z( icmpPacketLength ) int *icmpDataStart )
	{
	const int version = getIPVersion( icmpPacket[ PACKET_OFFSET_IPVERSION ] );

	assert( isReadPtrDynamic( icmpPacket, icmpPacketLength ) );

	REQUIRES( icmpPacketLength >= IP4_MIN_HEADERSIZE + ICMP_MIN_PACKETSIZE && \
			  icmpPacketLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*icmpDataStart = 0;

	/* Make sure that we got back something that at least looks like an 
	   IPv4 or IPv6 packet */
	if( version != 4 && version != 6 )
		return( CRYPT_ERROR_BADDATA );

	/* We got back an IPv4 or IPv6 packet, hopefully an ICMP response.  
	   Unfortunately despite the fact that this is an IPPROTO_ICMP socket, 
	   what we get back is a raw IP packet (due to the use of SOCK_RAW), so 
	   first we have to pick apart the IP packet to find the ICMP packet 
	   within it:

			  +-- IP4_MIN_HEADERSIZE
			  |						  +-- ICMP_MIN_PACKETSIZE
			  v						  v
		+-----------+-----------+-----------+-----------+
		| IPv4 hdr	| IPv4 opts	| ICMPv4 hdr| ICMP data	|
		+-----------+-----------+-----------+-----------+
		|<-- 20 --> |<- 0..40 ->|<--- 8 --->| 
		|<--- headerLength ---->|
		|<------------------ length ------------------->|

		+-----------+-----------+
		| IPv4 hdr	| ICMPv4 hdr| Minimum ICMPv4 packet
		+-----------+-----------+ */
	if( version == 4 )
		{
		const int headerLength = getIP4HeaderLength( icmpPacket[ 0 ] );

		/* Make sure that we got enough data back for an ICMP reply */
		if( headerLength < IP4_MIN_HEADERSIZE || \
			headerLength > icmpPacketLength - ICMP_MIN_PACKETSIZE )
			return( CRYPT_ERROR_BADDATA );

		/* Now make sure that we've got an ICMP reply */
		if( icmpPacket[ PACKET_OFFSET_PROTOCOL ] != IPPROTO_ICMP )
			return( CRYPT_ERROR_BADDATA );

		*icmpDataStart = headerLength;

		return( CRYPT_OK );
		}

	/* It's an IPv6 packet (which in the current case is rather unlikely 
	   given that we explicitly send IPv4 pings) and make sure that we've 
	   got enough data for an ICMP reply:

			  +-- IP6_HEADERSIZE
			  |			  +-- ICMP_MIN_PACKETSIZE
			  v			  v
		+-----------+-----------+-----------+
		| IPv6 hdr	| ICMPv6 hdr| ICMP data	|
		+-----------+-----------+-----------+
		|<-- 40 --> |<--- 8 --->| 
		|<------------ length ------------->|

		+-----------+-----------+
		| IPv6 hdr	| ICMPv6 hdr| Minimum ICMPv6 packet
		+-----------+-----------+ */
	ENSURES( version == 6 );
	if( icmpPacketLength < IP6_HEADERSIZE + ICMP_MIN_PACKETSIZE )
		return( CRYPT_ERROR_BADDATA );

	/* Now make sure that we've got an ICMP reply.  The latter serves two 
	   purposes, it checks that we have what we're after and it rejects 
	   packets with extra extension headers between the IPv6 header and the 
	   payload, which we don't bother trying to parse.  The presence of 
	   these in already-rare IPv6 packets should be even more rare given how 
	   many systems will drop any IPv6 packets that contain them (for 
	   example RFC 7872 reports packet drop rates of over 50% for some 
	   extension header types) */
	if( icmpPacket[ PACKET_OFFSET_NEXTHEADER ] != IPPROTO_ICMP )
		return( CRYPT_ERROR_BADDATA );
		
	*icmpDataStart = IP6_HEADERSIZE;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int diagnoseConnectionProblem( INOUT NET_STREAM_INFO *netStream, 
							   IN_BUFFER( hostNameLen ) const char *host, 
							   IN_LENGTH_DNS const int hostNameLen,
							   IN_ERROR const int originalStatus )
	{
	NET_STREAM_INFO diagnosticNetStream;
	SOCKET netSocket = INVALID_SOCKET;
	BYTE buffer[ 512 + 8 ];
	struct addrinfo *addrInfoPtr, *addrInfoCursor;
	int addressCount, length DUMMY_INIT, offset, status, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtrDynamic( host, hostNameLen ) );
	
	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( hostNameLen > 0 && hostNameLen <= MAX_DNS_SIZE );
	REQUIRES( cryptStatusError( originalStatus ) );

	/* Create an ICMP socket (we use a dummy port of 514 for the address 
	   lookup which for UDP is the syslog port, ICMP itself doesn't use 
	   ports).  Under Unix only the superuser can create raw sockets (a
	   restriction dating back to the "if it's on a port < 1024 then you
	   can trust it because root on that machine and I were at Oxford
	   together" days designed to prevent users sending custom packets
	   onto the net) so this will only work on non-Unix systems, but since
	   what we're doing here is opportunistic anyway there's no great need
	   to make it work everywhere.
	   
	   We also always use ICMPv4 via the hardcoded IPPROTO_ICMP rather than
	   choosing IPPROTO_ICMPV6 since again this is an opportunistic probe 
	   and IPv4 is the one most likely to work.   This simplifies other 
	   special-case handling such as the fact that for an ICMPv6 socket the 
	   checksum is calculated for us since there's an additional pseudo-
	   header included while for ICMPv4 we have to calculate it ourselves.
	   
	   Finally, as mentioned in the comment at the start of this function,
	   we only try for the first address rather than trying to iterate
	   through everything that's potentially available */
	status = getAddressInfo( netStream, &addrInfoPtr, host, hostNameLen, 
							 514, FALSE, TRUE );
	if( cryptStatusError( status ) )
		return( originalStatus );
	ANALYSER_HINT( addrInfoPtr != NULL );
	LOOP_MED( ( addrInfoCursor = addrInfoPtr, addressCount = 0 ),
			  addrInfoCursor != NULL && addressCount < IP_ADDR_COUNT,
			  ( addrInfoCursor = addrInfoCursor->ai_next, addressCount++ ) )
		{
		/* If it's not an IPv4 address, continue */
		if( addrInfoCursor->ai_family != AF_INET )
			continue;

		/* We've found an IPv4 address, create a socket for it */
		netSocket = socket( addrInfoCursor->ai_family, SOCK_RAW, 
							IPPROTO_ICMP );
		break;
		}
	ENSURES( LOOP_BOUND_OK );
	if( isBadSocket( netSocket ) )
		{
		freeAddressInfo( addrInfoPtr );
		return( originalStatus );
		}

	/* Set up a dummy network stream to handle the I/O */
	memset( &diagnosticNetStream, 0, sizeof( NET_STREAM_INFO  ) );
	diagnosticNetStream.protocol = STREAM_PROTOCOL_UDP;
	INIT_FLAGS( diagnosticNetStream.nFlags, STREAM_NFLAG_DGRAM );
	INIT_FLAGS( diagnosticNetStream.nhFlags, STREAM_NHFLAG_NONE );
	diagnosticNetStream.port = 514;
	diagnosticNetStream.timeout = 10;
	diagnosticNetStream.netSocket = netSocket;
	diagnosticNetStream.listenSocket = INVALID_SOCKET;
	FNPTR_SET( diagnosticNetStream.writeFunction, pingHost );	/* Dummy fns */
	FNPTR_SET( diagnosticNetStream.readFunction, pingHost );
	FNPTR_SET( diagnosticNetStream.transportConnectFunction, pingHost );
	FNPTR_SET( diagnosticNetStream.transportDisconnectFunction, pingHost );
	FNPTR_SET( diagnosticNetStream.transportReadFunction, pingHost );
	FNPTR_SET( diagnosticNetStream.transportWriteFunction, pingHost );
	FNPTR_SET( diagnosticNetStream.transportOKFunction, pingHost );
	FNPTR_SET( diagnosticNetStream.transportCheckFunction, pingHost );
	FNPTR_SET( diagnosticNetStream.connectFunctionOpt, NULL );
	FNPTR_SET( diagnosticNetStream.disconnectFunctionOpt, NULL );
	DATAPTR_SET( diagnosticNetStream.virtualStateInfo, NULL );
	FNPTR_SET( diagnosticNetStream.virtualGetDataFunction, NULL );
	FNPTR_SET( diagnosticNetStream.virtualPutDataFunction, NULL );
	FNPTR_SET( diagnosticNetStream.virtualGetErrorInfoFunction, NULL );
	ENSURES( sanityCheckNetStream( &diagnosticNetStream ) );

	/* Ping the remote host */
	status = pingHost( &diagnosticNetStream, addrInfoCursor->ai_addr, 
					   addrInfoCursor->ai_addrlen, buffer, 512, &length );
	closesocket( netSocket );
	freeAddressInfo( addrInfoPtr );
	memset( &diagnosticNetStream, 0, sizeof( NET_STREAM_INFO  ) );
	if( cryptStatusError( status ) )
		return( originalStatus );

	/* Postcondition: We've read enough data for an ICMP packet */
	ENSURES( length >= IP4_MIN_HEADERSIZE + ICMP_MIN_PACKETSIZE && \
			 length <= 512 );

	/* Process the ICMP response */
	status = processICMP( buffer, length, &offset );
	if( cryptStatusError( status ) )
		return( originalStatus );

	/* Report the result of the ICMP ping to the caller */
	if( buffer[ offset ] == ICMP_TYPE_ECHO_REPLY )
		{
		retExtErrAlt( originalStatus, 
					  ( originalStatus, NETSTREAM_ERRINFO,
						", however an ICMP ping to the host succeeded, "
						"indicating that the host is up" ) );
		}
	retExtErrAlt( originalStatus, 
				  ( originalStatus, NETSTREAM_ERRINFO,
					", and an ICMP ping to the host returned ICMP packet "
					"type %d, code %d", buffer[ offset ], 
					buffer[ offset + 1 ] ) );
	}
#endif /* USE_RAW_SOCKETS */

/****************************************************************************
*																			*
*						Additional Error Checking Functions					*
*																			*
****************************************************************************/

/* Some buggy firewall software will block any data transfer attempts made 
   after the initial connection setup, if we're in a situation where this 
   can happen then we check for the presence of a software firewall and 
   report a problem due to the firewall rather than a general networking 
   problem */

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
	int i, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );

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
		!pEnumDeviceDrivers( drivers, MAX_DRIVERS * sizeof( LPVOID ), 
							 &cbNeeded ) )
		{
		DynamicUnload( hPSAPI );
		return( CRYPT_ERROR_TIMEOUT );
		}

	/* Check whether a suspect filter driver is present */
	LOOP_MAX( i = 0, i < cbNeeded / sizeof( LPVOID ), i++ )
		{
		typedef struct {
			const char *name;
			const int nameLen;
			const BOOLEAN isMcafee;
			} DRIVER_INFO;
		static const DRIVER_INFO driverInfoTbl[] = {
			{ "cfwids.sys", 10, TRUE },		/* McAfee Personal IDS */
			{ "firelm01.sys", 8, TRUE },	/* McAfee Host IPS */
			{ "firehk4x.sys", 8, TRUE },	/* McAfee Host IPS */
			{ "firehk5x.sys", 8, TRUE },	/* McAfee Host IPS */
			{ "fw220.sys", 5, TRUE },		/* McAfee FW */
			{ "mfefirek.sys", 12, TRUE },	/* McAfree FW Engine */
			{ "mfehidk.sys", 11, TRUE },	/* McAfee link driver */
			{ "mfetdik.sys", 11, TRUE },	/* McAfee FW */
			{ "mfetdi2k.sys", 12, TRUE },	/* McAfee FW */
			{ "mfewfpk.sys", 11, TRUE },	/* McAfee Enterprise FW */
			{ "mpfirewall.sys", 10, TRUE },	/* McAfee Personal FW */
			{ "mvstdi5x.sys", 12, TRUE },	/* McAfee FW driver */
			{ "spbbcdrv.sys", 8, FALSE },	/* Norton Personal FW */
			{ "symfw.sys", 9, FALSE },		/* Symantec FW */
			{ "symids.sys", 10, FALSE },	/* Symantec IDS */
			{ "symndis.sys", 11, FALSE },	/* Norton Personal FW */
			{ "symtdi.sys", 6, FALSE },		/* Symantec TDI */
			{ "teefer.sys", 10, FALSE },	/* Norton/Symantec FW */
			{ "teefer2.sys", 11, FALSE },	/* Norton/Symantec FW */
			{ "teefer3.sys", 11, FALSE },	/* Norton/Symantec FW */
			{ "wpsdrvnt.sys", 12, FALSE },	/* Symantec CMC FW */
			{ NULL, 0, FALSE }, { NULL, 0, FALSE }
			};
		char driverName[ 256 + 8 ];
		int driverNameLen, driverIndex, LOOP_ITERATOR_ALT;

		driverNameLen = pGetDeviceDriverBaseName( drivers[ i ], 
												  driverName, 256 );
		if( driverNameLen <= 0 )
			continue;
		LOOP_MED_ALT( driverIndex = 0, 
					  driverInfoTbl[ driverIndex ].name != NULL && \
						driverIndex < FAILSAFE_ARRAYSIZE( driverInfoTbl, \
														  DRIVER_INFO ),
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
		ENSURES( LOOP_BOUND_OK_ALT );
		}
	ENSURES( LOOP_BOUND_OK );
	DynamicUnload( hPSAPI );

	return( CRYPT_ERROR_TIMEOUT );
	}
#endif /* Win32 */
#endif /* USE_TCP */
