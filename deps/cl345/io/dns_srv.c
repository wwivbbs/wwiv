/****************************************************************************
*																			*
*						cryptlib DNS SRV Interface Routines					*
*						Copyright Peter Gutmann 1998-2009					*
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

/* Use DNS SRV to auto-detect host information.  Note that this code is 
   disabled by default, before enabling it you should make sure that your
   system's DNS services can't serve as an attack vector due to the 
   complexity of DNS packet processing.  The Unix DNS interface is 
   particularly bad here, the problematic nature of the requirement that 
   implementations manually disassemble the DNS data themselves has been 
   demonstrated by the numerous bugs that have hit implementations that did 
   this, examples being the bind 9.2.1 gethostans() vulnerability and, in a 
   rather extreme example, the l0pht antisniff 1.0 vulnerability which 
   required no less than three successive patches to the same code to 
   finally eradicate the one bug (!!).  The fact that every new 
   implementation that wants to use this functionality has to independently 
   reinvent the code to do it means that these vulnerabilities will be with 
   us more or less forever, which is why this facility is disabled by 
   default */

#if defined( USE_TCP ) && defined( USE_DNSSRV )

#if defined( _MSC_VER ) || defined( __GNUC__ )
  #pragma message( "  Building with DNS SRV enabled." )
#endif /* Warn about special features enabled */

/****************************************************************************
*																			*
*						 	Windows DNS SRV Interface						*
*																			*
****************************************************************************/

#if defined( __WINDOWS__ ) && !defined( __WIN16__ )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int convertToSrv( OUT_BUFFER_FIXED( srvNameMaxLen ) char *srvName, 
						 IN_LENGTH_DNS const int srvNameMaxLen, 
						 IN_STRING const char *hostName )
	{
	const int hostNameLength = strlen( hostName ) + 1;
	int i, LOOP_ITERATOR;	   /* For trailing '\0' */

	assert( isReadPtrDynamic( srvName, srvNameMaxLen ) );
	assert( isReadPtr( hostName, MIN_DNS_SIZE ) );
	
	ANALYSER_HINT_STRING( hostName );

	REQUIRES( srvNameMaxLen > 16 && srvNameMaxLen <= MAX_DNS_SIZE );
	REQUIRES( srvName != hostName );

	/* Clear return value */
	memset( srvName, 0, min( 16, srvNameMaxLen ) );

	/* Make sure that the (worst-case) result will fit into the output 
	   buffer */
	if( 16 + hostNameLength > srvNameMaxLen )
		return( CRYPT_ERROR_OVERFLOW );

	/* Prepend the service info to the start of the host name.  This
	   converts foo.bar.com into _pkiboot._tcp.bar.com in preparation for
	   the DNS SRV lookup */
	LOOP_LARGE( i = 0, i < hostNameLength, i++ )
		{
		if( hostName[ i ] == '.' )
			break;
		}
	ENSURES( LOOP_BOUND_OK );
	memcpy( srvName, "_pkiboot._tcp.", 14 );
	if( i < hostNameLength )
		{
		REQUIRES( boundsCheck( 14, hostNameLength - i, srvNameMaxLen ) );
		memcpy( srvName + 14, hostName + i, hostNameLength - i );
		}
	else
		{
		REQUIRES( boundsCheck( 14, hostNameLength, srvNameMaxLen ) );
		memcpy( srvName + 14, hostName, hostNameLength );
		}
	srvName[ srvNameMaxLen ] = '\0';

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getSrvFQDN( INOUT NET_STREAM_INFO *netStream, 
					   OUT_BUFFER_FIXED( fqdnMaxLen ) char *fqdn, 
					   IN_LENGTH_DNS const int fqdnMaxLen )
	{
	PDNS_RECORD pDns = NULL;
	struct hostent *hostInfo;
	static char cachedFQDN[ MAX_DNS_SIZE + 8 ];
	static time_t lastFetchTime = 0;
#ifdef __WINCE__
	char fqdnBuffer[ MAX_DNS_SIZE + 8 ], *fqdnPtr = fqdnBuffer;
#else
	char *fqdnPtr;
#endif /* Win32 vs. WinCE */
	int status;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtrDynamic( fqdn, fqdnMaxLen ) );

	REQUIRES( fqdnMaxLen > 0 && fqdnMaxLen <= MAX_DNS_SIZE );

	/* Clear return value */
	memset( fqdn, 0, min( 16, fqdnMaxLen ) );

	/* The uncached FQDN check is quite slow and resource-intensive (it
	   seems to do a full reload of the DNS subsystem), to lighten the load
	   we only try a new one once a minute */
	if( lastFetchTime >= getTime() - 60 )
		{
		strlcpy_s( fqdn, fqdnMaxLen, cachedFQDN );
		return( CRYPT_OK );
		}

	/* If we're doing a full autodetect we first have to determine the local 
	   host's FQDN.  This gets quite tricky because the behavior of
	   gethostbyaddr() changed with Win2K so we have to use the DNS API, but
	   this isn't available in older versions of Windows.  If we are using
	   the DNS API, we have to use the barely-documented
	   DNS_QUERY_BYPASS_CACHE option to get what we want.
	   
	   One relief at least is that the braindead Posix semantics for 
	   gethostname() (see the comment in the Unix getFQDN() below) don't 
	   apply under Windows.  If the buffer size is too small, the call
	   fails, so we don't need to jump through any hoops to deal with this
	   situation */
	if( gethostname( cachedFQDN, MAX_DNS_SIZE ) == 0 && \
		( hostInfo = gethostbyname( cachedFQDN ) ) != NULL )
		{
		int i, LOOP_ITERATOR;

		LOOP_MED( i = 0, i < IP_ADDR_COUNT && \
						 hostInfo->h_addr_list[ i ] != NULL, i++ )
			{
			struct in_addr address;

			/* Reverse the byte order for the in-addr.arpa lookup and
			   convert the address to dotted-decimal notation */
			address.S_un.S_addr = *( ( DWORD * ) hostInfo->h_addr_list[ i ] );
			sprintf_s( cachedFQDN, MAX_DNS_SIZE, "%s.in-addr.arpa",
					   inet_ntoa( address ) );

			/* Check for a name */
			if( DnsQuery( cachedFQDN, DNS_TYPE_PTR, DNS_QUERY_BYPASS_CACHE,
						  NULL, &pDns, NULL ) == ERROR_SUCCESS )
				break;
			pDns = NULL;
			}
		ENSURES( LOOP_BOUND_OK );
		}
	if( pDns == NULL )
		{
		return( setSocketError( netStream, 
								"Couldn't determine FQDN of local machine", 
								40, CRYPT_ERROR_NOTFOUND, TRUE ) );
		}
#ifdef __WINCE__
	unicodeToAscii( fqdnBuffer, MAX_DNS_SIZE, pDns->Data.PTR.pNameHost,
					wcslen( pDns->Data.PTR.pNameHost ) + 1 );
#else
	fqdnPtr = pDns->Data.PTR.pNameHost;
#endif /* Win32 vs. WinCE */
	status = convertToSrv( cachedFQDN, MAX_DNS_SIZE, fqdnPtr );
	DnsFree( pDns, DnsFreeRecordList );
	if( cryptStatusError( status ) )
		{
		return( setSocketError( netStream, 
								"Couldn't convert FQDN into SRV query name", 
								41, CRYPT_ERROR_NOTFOUND, TRUE ) );
		}

	/* Remember the value that we just found to lighten the load on the
	   resolver when we perform repeat queries */
	strlcpy_s( fqdn, fqdnMaxLen, cachedFQDN );
	lastFetchTime = getTime();

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int findHostInfo( INOUT NET_STREAM_INFO *netStream, 
				  OUT_BUFFER_FIXED( hostNameMaxLen ) char *hostName, 
				  IN_LENGTH_DNS const int hostNameMaxLen, 
				  OUT_PORT_Z int *hostPort, 
				  IN_BUFFER( nameLen ) const char *name, 
				  IN_LENGTH_DNS const int nameLen )
	{
	PDNS_RECORD pDns = NULL, pDnsInfo = NULL, pDnsCursor;
	DNS_STATUS dnsStatus;
	char nameBuffer[ MAX_DNS_SIZE + 8 ];
	int priority = 32767, i, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtrDynamic( hostName, hostNameMaxLen ) );
	assert( isWritePtr( hostPort, sizeof( int ) ) );
	assert( isReadPtr( name, MIN_DNS_SIZE ) );

	REQUIRES( hostNameMaxLen > 0 && hostNameMaxLen <= MAX_DNS_SIZE );
	REQUIRES( nameLen > 0 && nameLen < MAX_DNS_SIZE );
	REQUIRES( hostName != name );

	/* Clear return values */
	memset( hostName, 0, min( 16, hostNameMaxLen ) );
	*hostPort = 0;

	/* Convert the name to a null-terminated string */
	REQUIRES( rangeCheck( nameLen, 1, MAX_DNS_SIZE ) );
	memcpy( nameBuffer, name, nameLen );
	nameBuffer[ nameLen ] = '\0';
	name = nameBuffer;

	/* If we're doing a full autodetect we construct the SRV query using the 
	   local machine's FQDN.  This fails more often than not because of 
	   NATing and the use of private networks, but at least we can try */
	if( !strCompareZ( name, "[Autodetect]" ) )
		{
		const int status = getSrvFQDN( netStream, hostName, hostNameMaxLen );
		if( cryptStatusError( status ) )
			return( status );
		name = hostName;
		}

	/* Perform a DNS SRV lookup to find the host info.  SRV has basic load-
	   balancing facilities but for now we just use the highest-priority 
	   host that we find (it's rarely-enough used that we'll be lucky to
	   find SRV info let alone any load-balancing setup) */
	dnsStatus = DnsQuery( ( const LPSTR ) name, DNS_TYPE_SRV, DNS_QUERY_STANDARD,
						  NULL, &pDns, NULL );
	if( dnsStatus != 0 || pDns == NULL )
		{
		int dummy;

		return( getSocketError( netStream, CRYPT_ERROR_NOTFOUND, &dummy ) );
		}
	LOOP_MED( ( pDnsCursor = pDns, i = 0 ), 
			  pDnsCursor != NULL && i < IP_ADDR_COUNT,
			  ( pDnsCursor = pDnsCursor->pNext, i++ ) )
		{
		if( pDnsCursor->Data.SRV.wPriority < priority )
			{
			priority = pDnsCursor->Data.SRV.wPriority;
			pDnsInfo = pDnsCursor;
			}
		}
	ENSURES( LOOP_BOUND_OK );
#ifdef __WINCE__
	if( pDnsInfo == NULL || \
		wcslen( pDnsInfo->Data.SRV.pNameTarget ) + 1 > hostNameMaxLen )
#else
	if( pDnsInfo == NULL || \
		strlen( pDnsInfo->Data.SRV.pNameTarget ) + 1 > hostNameMaxLen )
#endif /* Win32 vs. WinCE */
		{
		DnsFree( pDns, DnsFreeRecordList );
		return( setSocketError( netStream, "Invalid DNS SRV entry for host", 30,
								CRYPT_ERROR_NOTFOUND, TRUE ) );
		}

	/* Copy over the host info for this SRV record */
#ifdef __WINCE__
	unicodeToAscii( hostName, hostNameMaxLen,
					pDnsInfo->Data.SRV.pNameTarget, 
					wcslen( pDnsInfo->Data.SRV.pNameTarget ) + 1 );
#else
	strlcpy_s( hostName, hostNameMaxLen, pDnsInfo->Data.SRV.pNameTarget );
#endif /* Win32 vs. WinCE */
	*hostPort = pDnsInfo->Data.SRV.wPort;

	/* Clean up */
	DnsFree( pDns, DnsFreeRecordList );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Unix DNS SRV Interface							*
*																			*
****************************************************************************/

#elif defined( __UNIX__ ) && \
	  !( defined( __CYGWIN__) || ( defined( sun ) && OSVERSION <= 5 ) || \
		 defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ ) || \
		 defined( __UCLIBC__ ) )

#define SRV_PRIORITY_OFFSET	( NS_RRFIXEDSZ + 0 )
#define SRV_WEIGHT_OFFSET	( NS_RRFIXEDSZ + 2 )
#define SRV_PORT_OFFSET		( NS_RRFIXEDSZ + 4 )
#define SRV_NAME_OFFSET		( NS_RRFIXEDSZ + 6 )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getFQDN( STDC_UNUSED INOUT NET_STREAM_INFO *netStream, 
					OUT_BUFFER_FIXED( fqdnMaxLen ) char *fqdn, 
					IN_LENGTH_DNS const int fqdnMaxLen )
	{
	struct hostent *hostInfo;
	char *hostNamePtr = NULL;
	int addressCount, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtrDynamic( fqdn, fqdnMaxLen ) );

	REQUIRES( fqdnMaxLen > 0 && fqdnMaxLen <= MAX_DNS_SIZE );

	/* Clear return value */
	memset( fqdn, 0, min( 16, fqdnMaxLen ) );

	/* First get the host name and if it's the FQDN, exit.  gethostname() 
	   has the idiotic property that if the name doesn't fit into the given
	   buffer the function will return a (possibly non-null-terminated) 
	   truncated value instead of reporting an error.  At least that's 
	   what the spec says, many implementations don't do this but in case
	   there's one that's stupid enough to actually do so we force null-
	   termination after we've called the function */
	if( gethostname( fqdn, fqdnMaxLen ) == -1 )
		return( CRYPT_ERROR_NOTFOUND );
	fqdn[ fqdnMaxLen - 1 ] = '\0';
	if( strchr( fqdn, '.' ) != NULL )
		{
		/* If the hostname has a dot in it, it's the FQDN */
		return( CRYPT_OK );
		}

	/* Now get the hostent info and walk through it looking for the FQDN */
	if( ( hostInfo = gethostbyname( fqdn ) ) == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	LOOP_MED( addressCount = 0, 
			  addressCount < IP_ADDR_COUNT && \
				hostInfo->h_addr_list[ addressCount ] != NULL, addressCount++ )
		{
		char **aliasPtrPtr;	
		int i, LOOP_ITERATOR_ALT;
	
		/* If the hostname has a dot in it, it's the FQDN.  This should be
		   the same as the gethostname() output, but we check again just in
		   case */
		if( strchr( hostInfo->h_name, '.' ) != NULL )
			{
			hostNamePtr = hostInfo->h_name;
			break;
			}

		/* Try for the FQDN in the aliases */
		if( hostInfo->h_aliases == NULL )
			continue;
		LOOP_MED_ALT( ( aliasPtrPtr = hostInfo->h_aliases, i = 0 ),
					  *aliasPtrPtr != NULL && \
						!strchr( *aliasPtrPtr, '.' ) && i < IP_ADDR_COUNT, 
					  ( aliasPtrPtr++, i++ ) );
		ENSURES( LOOP_BOUND_OK_ALT );
		if( *aliasPtrPtr != NULL )
			{
			hostNamePtr = *aliasPtrPtr;
			break;
			}
		}
	ENSURES( LOOP_BOUND_OK );
	if( hostNamePtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* We found the FQDN, return it to the caller */
	if( strlen( hostNamePtr ) + 1 > fqdnMaxLen )
		return( CRYPT_ERROR_OVERFLOW );
	strlcpy_s( fqdn, fqdnMaxLen, hostNamePtr );
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int findHostInfo( INOUT NET_STREAM_INFO *netStream, 
				  OUT_BUFFER_FIXED( hostNameMaxLen ) char *hostName, 
				  IN_LENGTH_DNS const int hostNameMaxLen, 
				  OUT_PORT_Z int *hostPort, 
				  IN_BUFFER( nameLen ) const char *name, 
				  IN_LENGTH_DNS const int nameLen )
	{
	union {
		HEADER header;
		BYTE buffer[ NS_PACKETSZ + 8 ];
		} dnsQueryInfo;
	BYTE *namePtr, *endPtr;
	char nameBuffer[ MAX_DNS_SIZE + 8 ];
	int resultLen, nameSegmentLen, qCount, aCount, minPriority = 32767;
	int i, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtrDynamic( hostName, hostNameMaxLen ) );
	assert( isWritePtr( hostPort, sizeof( int ) ) );
	assert( isReadPtr( name, MIN_DNS_SIZE ) );

	REQUIRES( hostNameMaxLen > 0 && hostNameMaxLen <= MAX_DNS_SIZE );
	REQUIRES( nameLen > 0 && nameLen < MAX_DNS_SIZE );
	REQUIRES( hostName != name );

	/* Clear return values */
	memset( hostName, 0, min( 16, hostNameMaxLen ) );
	*hostPort = 0;

	/* Convert the name to a null-terminated string */
	REQUIRES( rangeCheck( nameLen, 1, MAX_DNS_SIZE ) );
	memcpy( nameBuffer, name, nameLen );
	nameBuffer[ nameLen ] = '\0';
	name = nameBuffer;

	/* If we're doing a full autodetect, we construct the SRV query using
	   the local machine's FQDN.  This fails more often than not because of
	   NATing and the use of private networks, but at least we can try */
	if( !strCompareZ( name, "[Autodetect]" ) )
		{
		const int status = getFQDN( netStream, hostName, hostNameMaxLen );
		if( cryptStatusError( status ) )
			return( status );
		name = hostName;
		}
#ifdef EBCDIC_CHARS
	else
		{
		/* We're about to use OS functions, convert the input to EBCDIC.  If
		   we've used autodetection the output from getFQDN will already be 
		   in EBCDIC form */
		name = bufferToEbcdic( hostName, name );
		}
#endif /* EBCDIC_CHARS */

	/* Try and fetch a DNS SRV record (RFC 2782) matching the host info.  
	   Unlike Windows' relatively nice DnsQuery() API, Unix has a horribly
	   clunky interface that requires manually grovelling through wire-
	   format data to dig out the bits of interest.  OpenBSD provides a 
	   function getrrsetbyname()/freerrset() that's equivalent to Windows' 
	   DnsQuery()/DnsFree() but it's OpenBSD-only so we can't really rely 
	   on it being present */
	resultLen = res_query( name, C_IN, T_SRV, dnsQueryInfo.buffer,
						   NS_PACKETSZ );
	if( resultLen < NS_HFIXEDSZ || resultLen > NS_PACKETSZ )
		{
		int dummy;

		return( getSocketError( netStream, CRYPT_ERROR_NOTFOUND, &dummy ) );
		}
	if( dnsQueryInfo.header.rcode != 0 || dnsQueryInfo.header.tc != 0 )
		{
		/* If we get a non-zero response code (rcode) or the results were
		   truncated (tc), we can't go any further.  In theory a truncated
		   response is probably OK since many servers return the address
		   records for the host in the Additional Data section to save the
		   client having to perform a second lookup and we don't need these
		   at this point so we can ignore the fact that they've been
		   truncated, but for now we treat truncation as an error */
		return( setSocketError( netStream, 
								"RR contains non-zero response code or "
								"response was truncated", 60,
								CRYPT_ERROR_NOTFOUND, FALSE ) );
		}
	qCount = ntohs( dnsQueryInfo.header.qdcount );
	aCount = ntohs( dnsQueryInfo.header.ancount );
	if( qCount < 0 || qCount > 100 || aCount <= 0 || aCount > 100 )
		{
		/* No answer entries (or a suspicious number of entries, which is 
		   less likely), we're done */
		return( setSocketError( netStream, "RR contains no answer entries", 29,
								CRYPT_ERROR_NOTFOUND, FALSE ) );
		}

	/* Skip the queries */
	namePtr = dnsQueryInfo.buffer + NS_HFIXEDSZ;
	endPtr = dnsQueryInfo.buffer + resultLen;
	LOOP_LARGE( i = 0, i < qCount && namePtr < endPtr && i < 100, i++ )
		{
		nameSegmentLen = dn_skipname( namePtr, endPtr );
		if( nameSegmentLen <= 0 || nameSegmentLen > MAX_DNS_SIZE )
			{
			return( setSocketError( netStream, 
									"RR contains invalid question", 28,
									CRYPT_ERROR_BADDATA, FALSE ) );
			}
		namePtr += nameSegmentLen + NS_QFIXEDSZ;
		}
	ENSURES( LOOP_BOUND_OK );
	if( namePtr > endPtr )
		{
		return( setSocketError( netStream, "RR contains invalid data", 24,
								CRYPT_ERROR_BADDATA, FALSE ) );
		}

	/* Process the answers.  SRV has basic load-balancing facilities, but
	   for now we just use the highest-priority host that we find (it's
	   rarely-enough used that we'll be lucky to find SRV info, let alone
	   any load-balancing setup) */
	LOOP_MED( i = 0, i < aCount, i++ )
		{
		int priority, port;

		nameSegmentLen = dn_skipname( namePtr, endPtr );
		if( nameSegmentLen <= 0 || nameSegmentLen > MAX_DNS_SIZE || \
			namePtr + nameSegmentLen + NS_SRVFIXEDSZ > endPtr )
			{
	        return( setSocketError( netStream, "RR contains invalid answer", 26,
	                                CRYPT_ERROR_BADDATA, FALSE ) );
			}
		namePtr += nameSegmentLen;
		priority = ntohs( *( ( u_short * ) ( namePtr + SRV_PRIORITY_OFFSET ) ) );
		port = ntohs( *( ( u_short * ) ( namePtr + SRV_PORT_OFFSET ) ) );
		namePtr += NS_SRVFIXEDSZ;
		if( priority < minPriority )
			{
			/* We've got a new higher-priority host, use that */
			nameSegmentLen = dn_expand( dnsQueryInfo.buffer, endPtr,
										namePtr, hostName, hostNameMaxLen );
			*hostPort = port;
			minPriority = priority;
			}
		else
			{
			/* It's a lower-priority host, skip it */
			nameSegmentLen = dn_skipname( namePtr, endPtr );
			}
		if( nameSegmentLen <= 0 || nameSegmentLen > MAX_DNS_SIZE || \
			namePtr + nameSegmentLen > endPtr )
			{
	        return( setSocketError( netStream, "RR contains invalid answer", 26,
	                                CRYPT_ERROR_NOTFOUND, FALSE ) );
			}
		hostName[ nameSegmentLen ] = '\0';
		namePtr += nameSegmentLen;
		}
	ENSURES( LOOP_BOUND_OK );
#ifdef EBCDIC_CHARS
	ebcdicToAscii( hostName, strlen( hostName ) );
#endif /* EBCDIC_CHARS */

	return( CRYPT_OK );
	}
#endif /* OS-specific host detection */

#endif /* USE_TCP && USE_DNSSRV */
