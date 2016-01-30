/****************************************************************************
*																			*
*						 Network Stream Proxy Management					*
*						Copyright Peter Gutmann 1993-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "stream_int.h"
#else
  #include "io/stream_int.h"
#endif /* Compiler-specific includes */

#ifdef USE_TCP

/****************************************************************************
*																			*
*							SOCKS Proxy Management							*
*																			*
****************************************************************************/

/* Open a connection through a Socks proxy.  This is currently disabled
   since it doesn't appear to be used by anyone */

#if 0

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int connectViaSocksProxy( INOUT STREAM *stream )
	{
	MESSAGE_DATA msgData;
	BYTE socksBuffer[ 64 + CRYPT_MAX_TEXTSIZE + 8 ], *bufPtr = socksBuffer;
	char userName[ CRYPT_MAX_TEXTSIZE + 8 ];
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( stream->type == STREAM_TYPE_NETWORK );

	/* Get the SOCKS user name, defaulting to "cryptlib" if there's none
	   set */
	setMessageData( &msgData, userName, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_OPTION_NET_SOCKS_USERNAME );
	if( cryptStatusOK( status ) )
		userName[ msgData.length ] = '\0';
	else
		strlcpy_s( userName, CRYPT_MAX_TEXTSIZE, "cryptlib" );

	/* Build up the SOCKSv4 request string:

		BYTE: version = 4
		BYTE: command = 1 (connect)
		WORD: port
		LONG: IP address
		STRING: userName + '\0'

	   Note that this has a potential problem in that it requires a DNS 
	   lookup by the client, which can lead to problems if the client
	   can't get DNS requests out because only SOCKSified access is allowed.
	   A related problem occurs when SOCKS is being used as a tunnelling
	   interface because the DNS lookup will communicate data about the 
	   client to an observer outside the tunnel.

	   To work around this there's a so-called SOCKSv4a protocol that has 
	   the SOCKS proxy perform the lookup:

		BYTE: version = 4
		BYTE: command = 1 (connect)
		WORD: port
		LONG: IP address = 0x00 0x00 0x00 0xFF
		STRING: userName + '\0'
		STRING: FQDN + '\0'

	   Unfortunately there's no way to tell whether a SOCKS server supports
	   4a or only 4, but in any case since SOCKS support is currently 
	   disabled we leave the poke-and-hope 4a detection until such time as
	   someone actually requests it */
	*bufPtr++ = 4; *bufPtr++ = 1;
	mputWord( bufPtr, netStream->port );
	status = getIPAddress( stream, bufPtr, netStream->host );
	strlcpy_s( bufPtr + 4, CRYPT_MAX_TEXTSIZE, userName );
	length = 1 + 1 + 2 + 4 + strlen( userName ) + 1;
	if( cryptStatusError( status ) )
		{
		netStream->transportDisconnectFunction( stream, TRUE );
		return( status );
		}

	/* Send the data to the server and read back the reply */
	status = netStream->transportWriteFunction( stream, socksBuffer, length,
												TRANSPORT_FLAG_FLUSH );
	if( cryptStatusOK( status ) )
		status = netStream->transportReadFunction( stream, socksBuffer, 8,
												   TRANSPORT_FLAG_BLOCKING );
	if( cryptStatusError( status ) )
		{
		/* The involvement of a proxy complicates matters somewhat because
		   we can usually connect to the proxy OK but may run into problems
		   going from the proxy to the remote server, so if we get an error
		   at this stage (which will typically show up as a read error from
		   the proxy) we report it as an open error instead */
		if( status == CRYPT_ERROR_READ || status == CRYPT_ERROR_COMPLETE )
			status = CRYPT_ERROR_OPEN;
		netStream->transportDisconnectFunction( stream, TRUE );
		return( status );
		}

	/* Make sure that everything is OK:

		BYTE: null = 0
		BYTE: status = 90 (OK)
		WORD: port
		LONG: IP address */
	if( socksBuffer[ 1 ] != 90 )
		{
		int i;

		netStream->transportDisconnectFunction( stream, TRUE );
		strlcpy_s( netStream->errorInfo->errorString, MAX_ERRMSG_SIZE, 
				   "Socks proxy returned" );
		for( i = 0; i < 8; i++ )
			{
			sprintf_s( netStream->errorInfo->errorString + 20 + ( i * 3 ),
					   MAX_ERRMSG_SIZE - ( 20 + ( i * 3 ) ), " %02X", 
					   socksBuffer[ i ] );
			}
		strlcat_s( netStream->errorInfo->errorString, MAX_ERRMSG_SIZE, "." );
		netStream->errorCode = socksBuffer[ 1 ];
		return( CRYPT_ERROR_OPEN );
		}

	return( CRYPT_OK );
	}
#endif /* 0 */

/****************************************************************************
*																			*
*							HTTP Proxy Management							*
*																			*
****************************************************************************/

#ifdef USE_HTTP

/* Open a connection via an HTTP proxy */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int connectViaHttpProxy( INOUT STREAM *stream, 
						 INOUT ERROR_INFO *errorInfo )
	{
	NET_STREAM_INFO *netStream = ( NET_STREAM_INFO * ) stream->netStreamInfo;
	HTTP_DATA_INFO httpDataInfo;
	BYTE buffer[ 64 + 8 ];
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES_S( netStream != NULL );
	REQUIRES_S( stream->type == STREAM_TYPE_NETWORK );

	/* Open the connection via the proxy.  To do this we temporarily layer
	   HTTP I/O over the TCP I/O, then once the proxy messaging has been
	   completed we re-set the stream to pure TCP I/O and clear any stream
	   flags that were set during the proxying */
	setStreamLayerHTTP( netStream );
	initHttpDataInfo( &httpDataInfo, "", 0 );
	status = netStream->writeFunction( stream, &httpDataInfo,
									   sizeof( HTTP_DATA_INFO ), &length );
	if( cryptStatusOK( status ) )
		{
		initHttpDataInfo( &httpDataInfo, buffer, 64 );
		status = netStream->readFunction( stream, &httpDataInfo,
										  sizeof( HTTP_DATA_INFO ), &length );
		}
	setStreamLayerDirect( netStream );
	stream->flags = 0;
	if( cryptStatusError( status ) )
		{
		/* The involvement of a proxy complicates matters somewhat because
		   we can usually connect to the proxy OK but may run into problems
		   going from the proxy to the remote server so if we get an error
		   at this stage (which will typically show up as a read error from
		   the proxy) we report it as an open error instead */
		if( status == CRYPT_ERROR_READ || status == CRYPT_ERROR_COMPLETE )
			status = CRYPT_ERROR_OPEN;
		copyErrorInfo( errorInfo, NETSTREAM_ERRINFO );
		netStream->transportDisconnectFunction( netStream, TRUE );
		}

	return( status );
	}
#endif /* USE_HTTP */

/****************************************************************************
*																			*
*							Proxy Autoconfig Management						*
*																			*
****************************************************************************/

/* Try and auto-detect HTTP proxy information */

#if defined( __WIN32__ )

/* The autoproxy functions were only documented in WinHTTP 5.1 so we have to
   provide the necessary defines and structures ourselves */

#ifndef WINHTTP_ACCESS_TYPE_DEFAULT_PROXY

#define HINTERNET	HANDLE

typedef struct {
	DWORD dwFlags;
	DWORD dwAutoDetectFlags;
	LPCWSTR lpszAutoConfigUrl;
	LPVOID lpvReserved;
	DWORD dwReserved;
	BOOL fAutoLogonIfChallenged;
	} WINHTTP_AUTOPROXY_OPTIONS;

typedef struct {
	DWORD dwAccessType;
	LPWSTR lpszProxy;
	LPWSTR lpszProxyBypass;
	} WINHTTP_PROXY_INFO;

typedef struct {
	BOOL fAutoDetect;
	LPWSTR lpszAutoConfigUrl;
	LPWSTR lpszProxy;
	LPWSTR lpszProxyBypass;
	} WINHTTP_CURRENT_USER_IE_PROXY_CONFIG;

#define WINHTTP_AUTOPROXY_AUTO_DETECT	1
#define WINHTTP_AUTO_DETECT_TYPE_DHCP	1
#define WINHTTP_AUTO_DETECT_TYPE_DNS_A	2
#define WINHTTP_ACCESS_TYPE_NO_PROXY	1
#define WINHTTP_NO_PROXY_NAME			NULL
#define WINHTTP_NO_PROXY_BYPASS			NULL

#endif /* WinHTTP 5.1 defines and structures */

typedef HINTERNET ( *WINHTTPOPEN )( LPCWSTR pwszUserAgent, DWORD dwAccessType,
									LPCWSTR pwszProxyName, LPCWSTR pwszProxyBypass,
									DWORD dwFlags );
typedef BOOL ( *WINHTTPGETDEFAULTPROXYCONFIGURATION )( WINHTTP_PROXY_INFO* pProxyInfo );
typedef BOOL ( *WINHTTPGETIEPROXYCONFIGFORCURRENTUSER )(
								WINHTTP_CURRENT_USER_IE_PROXY_CONFIG *pProxyConfig );
typedef BOOL ( *WINHTTPGETPROXYFORURL )( HINTERNET hSession, LPCWSTR lpcwszUrl,
										 WINHTTP_AUTOPROXY_OPTIONS *pAutoProxyOptions,
										 WINHTTP_PROXY_INFO *pProxyInfo );
typedef BOOL ( *WINHTTPCLOSEHANDLE )( HINTERNET hInternet );

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int findProxyUrl( OUT_BUFFER( proxyMaxLen, *proxyLen ) char *proxy, 
				  IN_LENGTH_DNS const int proxyMaxLen, 
				  OUT_LENGTH_BOUNDED_Z( proxyMaxLen ) int *proxyLen,
				  IN_BUFFER( urlLen ) const char *url, 
				  IN_LENGTH_DNS const int urlLen )
	{
	static HMODULE hWinHTTP = NULL;
	static WINHTTPOPEN pWinHttpOpen = NULL;
	static WINHTTPGETDEFAULTPROXYCONFIGURATION pWinHttpGetDefaultProxyConfiguration = NULL;
	static WINHTTPGETIEPROXYCONFIGFORCURRENTUSER pWinHttpGetIEProxyConfigForCurrentUser = NULL;
	static WINHTTPGETPROXYFORURL pWinHttpGetProxyForUrl = NULL;
	static WINHTTPCLOSEHANDLE pWinHttpCloseHandle = NULL;
	WINHTTP_AUTOPROXY_OPTIONS autoProxyOptions = \
			{ WINHTTP_AUTOPROXY_AUTO_DETECT,
			  WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A,
			  NULL, NULL, 0, FALSE };
	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxyInfo;
	WINHTTP_PROXY_INFO proxyInfo;
	HINTERNET hSession;
	char urlBuffer[ MAX_DNS_SIZE + 1 + 8 ];
	wchar_t unicodeURL[ MAX_DNS_SIZE + 1 + 8 ];
	size_t unicodeUrlLen, wcsProxyLen;
	int offset, length, proxyStatus;

	assert( isWritePtr( proxy, proxyMaxLen ) );
	assert( isWritePtr( proxyLen, sizeof( int ) ) );
	assert( isReadPtr( url, urlLen ) );

	REQUIRES( proxyMaxLen >= 10 && proxyMaxLen <= MAX_DNS_SIZE );
	REQUIRES( urlLen > 0 && urlLen <= MAX_DNS_SIZE );

	/* Under Win2K SP3 and Windows XP and newer (2003, Vista, etc), or at 
	   least Windows versions with WinHTTP 5.1 installed in some way (it 
	   officially shipped with the versions mentioned earlier) we can use 
	   WinHTTP AutoProxy support, which implements the Web Proxy Auto-
	   Discovery (WPAD) protocol from an internet draft that expired in May 
	   2001.  Under older versions of Windows we have to use the WinINet 
	   InternetGetProxyInfo, however this consists of a ghastly set of 
	   kludges that were never meant to be exposed to the outside world 
	   (they were only crowbarred out of MS as part of the DoJ consent 
	   decree) and user experience with them is that they don't really work 
	   except in the one special way in which MS-internal code calls them.  
	   Since we don't know what this is, we use the WinHTTP functions 
	   instead */
	if( hWinHTTP == NULL )
		{
		if( ( hWinHTTP = DynamicLoad( "WinHTTP.dll" ) ) == NULL )
			return( CRYPT_ERROR_NOTFOUND );

		pWinHttpOpen = ( WINHTTPOPEN ) \
						GetProcAddress( hWinHTTP, "WinHttpOpen" );
		pWinHttpGetDefaultProxyConfiguration = ( WINHTTPGETDEFAULTPROXYCONFIGURATION ) \
						GetProcAddress( hWinHTTP, "WinHttpGetDefaultProxyConfiguration" );
		pWinHttpGetIEProxyConfigForCurrentUser = ( WINHTTPGETIEPROXYCONFIGFORCURRENTUSER ) \
						GetProcAddress( hWinHTTP, "WinHttpGetIEProxyConfigForCurrentUser" );
		pWinHttpGetProxyForUrl = ( WINHTTPGETPROXYFORURL ) \
						GetProcAddress( hWinHTTP, "WinHttpGetProxyForUrl" );
		pWinHttpCloseHandle = ( WINHTTPCLOSEHANDLE ) \
						GetProcAddress( hWinHTTP, "WinHttpCloseHandle" );
		if( pWinHttpOpen == NULL || pWinHttpGetProxyForUrl == NULL || \
			pWinHttpCloseHandle == NULL )
			{
			DynamicUnload( hWinHTTP );
			return( CRYPT_ERROR_NOTFOUND );
			}
		}

	/* Autoproxy discovery using WinHttpGetProxyForUrl() can be awfully slow,
	   often taking several seconds since it requires probing for proxy info
	   first using DHCP and then if that fails using DNS.  Since this is done
	   via a blocking call everything blocks while it's in progress.  To help 
	   mitigate this we try for proxy info direct from the registry if it's 
	   available, avoiding the lengthy autodiscovery process.  This also 
	   means that discovery will work if no auto-discovery support is present,
	   for example on servers where the admin has set the proxy config
	   directly with ProxyCfg.exe */
	if( pWinHttpGetDefaultProxyConfiguration != NULL && \
		pWinHttpGetDefaultProxyConfiguration( &proxyInfo ) && \
		proxyInfo.lpszProxy != NULL )
		{
		proxyStatus = wcstombs_s( &wcsProxyLen, proxy, proxyMaxLen,
								  proxyInfo.lpszProxy, MAX_DNS_SIZE );
		GlobalFree( proxyInfo.lpszProxy );
		if( proxyInfo.lpszProxyBypass != NULL )
			GlobalFree( proxyInfo.lpszProxyBypass );
		if( proxyStatus == 0 )
			{
			*proxyLen = wcsProxyLen;
			return( CRYPT_OK );
			}
		}

	/* The next fallback is to get the proxy info from MSIE.  This is also
	   usually much quicker than WinHttpGetProxyForUrl() although sometimes
	   it seems to fall back to that, based on the longish delay involved.
	   Another issue with this is that it won't work in a service process
	   that isn't impersonating an interactive user (since there isn't a
	   current user), but in that case we just fall back to
	   WinHttpGetProxyForUrl() */
	if( pWinHttpGetIEProxyConfigForCurrentUser != NULL && \
		pWinHttpGetIEProxyConfigForCurrentUser( &ieProxyInfo ) )
		{
		proxyStatus = wcstombs_s( &wcsProxyLen, proxy, proxyMaxLen,
								  ieProxyInfo.lpszProxy, MAX_DNS_SIZE );
		if( ieProxyInfo.lpszAutoConfigUrl != NULL )
			GlobalFree( ieProxyInfo.lpszAutoConfigUrl );
		if( ieProxyInfo.lpszProxy != NULL )
			GlobalFree( ieProxyInfo.lpszProxy );
		if( ieProxyInfo.lpszProxyBypass != NULL )
			GlobalFree( ieProxyInfo.lpszProxyBypass );
		if( proxyStatus == 0 )
			{
			*proxyLen = wcsProxyLen;
			return( CRYPT_OK );
			}
		}

	/* WinHttpGetProxyForUrl() requires a schema for the URL that it's
	   performing a lookup on, if the URL doesn't contain one we use a
	   default value of "http://".  In addition we need to convert the
	   raw octet string into a null-terminated string for the mbstowcs_s()
	   Unicode conversion and following WinHttpGetProxyForUrl() lookup */
	if( strFindStr( url, urlLen, "://", 3 ) < 0 )
		{
		strlcpy_s( urlBuffer, MAX_DNS_SIZE, "http://" );
		offset = 7;
		length = MAX_DNS_SIZE - offset;
		}
	else
		{
		/* There's already a schema present, not need to manually add one */
		offset = 0;
		length = urlLen;
		}
	memcpy( urlBuffer + offset, url, length );
	urlBuffer[ offset + length ] = '\0';

	/* Locate the proxy used for accessing the resource at the supplied URL.
	   We have to convert to and from Unicode because the WinHTTP functions
	   all take Unicode strings as args.

	   WinHttpGetProxyForUrl() can be rather flaky, in some cases it'll fail
	   instantly (without even trying auto-discovery) with GetLastError() =
	   87 (parameter error) but then calling it again some time later works
	   fine.  Because of this we leave it as the last resort after trying
	   all of the other get-proxy mechanisms */
	hSession = pWinHttpOpen( L"cryptlib/1.0",
							 WINHTTP_ACCESS_TYPE_NO_PROXY,
							 WINHTTP_NO_PROXY_NAME,
							 WINHTTP_NO_PROXY_BYPASS, 0 );
	if( hSession == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	if( mbstowcs_s( &unicodeUrlLen, unicodeURL, MAX_DNS_SIZE,
					urlBuffer, MAX_DNS_SIZE ) != 0 )
		{
		pWinHttpCloseHandle( hSession );
		return( CRYPT_ERROR_NOTFOUND );
		}
	unicodeURL[ unicodeUrlLen ] = L'\0';
	memset( &proxyInfo, 0, sizeof( WINHTTP_PROXY_INFO ) );
	if( pWinHttpGetProxyForUrl( hSession, unicodeURL, &autoProxyOptions,
								&proxyInfo ) != TRUE )
		{
		pWinHttpCloseHandle( hSession );
		return( CRYPT_ERROR_NOTFOUND );
		}
	proxyStatus = wcstombs_s( &wcsProxyLen, proxy, proxyMaxLen,
							  proxyInfo.lpszProxy, MAX_DNS_SIZE );
	GlobalFree( proxyInfo.lpszProxy );
	if( proxyInfo.lpszProxyBypass != NULL )
		GlobalFree( proxyInfo.lpszProxyBypass );
	pWinHttpCloseHandle( hSession );
	if( proxyStatus != 0 )
		return( CRYPT_ERROR_NOTFOUND );
	*proxyLen = wcsProxyLen;

	return( CRYPT_OK );
	}

#if 0

typedef BOOL ( WINAPI *INTERNETGETPROXYINFO )( LPCSTR lpszUrl, DWORD dwUrlLength,
							LPSTR lpszUrlHostName, DWORD dwUrlHostNameLength,
							LPSTR* lplpszProxyHostName,
							LPDWORD lpdwProxyHostNameLength );
typedef BOOL ( WINAPI *INTERNETINITIALIZEAUTOPROXYDLL )( DWORD dwVersion,
							LPSTR lpszDownloadedTempFile, LPSTR lpszMime,
							AutoProxyHelperFunctions* lpAutoProxyCallbacks,
							LPAUTO_PROXY_SCRIPT_BUFFER lpAutoProxyScriptBuffer );

static int findProxyUrl( char *proxy, const int proxyMaxLen, 
						 const char *url, const int urlLen )
	{
	static INTERNETGETPROXYINFO pInternetGetProxyInfo = NULL;
	static INTERNETINITIALIZEAUTOPROXYDLL pInternetInitializeAutoProxyDll = NULL;
	URL_INFO urlInfo;
	char urlHost[ MAX_DNS_SIZE + 8 ];
	char *proxyHost = NULL;
	int proxyHostLen, status;

	assert( isWritePtr( proxy, proxyMaxLen ) );
	assert( isReadPtr( url, urlLen ) );

	REQUIRES( proxyMaxLen > 10 && proxyMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( urlLen > 0 && urlLen < MAX_INTLENGTH_SHORT );

	/* This gets somewhat complicated, under Win2K SP3 and XP and newer (or 
	   at least Windows versions with WinHTTP 5.1 installed in some way, it
	   officially shipped with the versions mentioned earlier) we can use
	   WinHTTP AutoProxy support, which implements the Web Proxy Auto-
	   Discovery (WPAD) protocol from an internet draft that expired in May
	   2001.  Under older versions of Windows we have to use the WinINet
	   InternetGetProxyInfo.

	   These functions were never meant to be used by the general public
	   (see the comment below) so they work in an extremely peculiar way
	   and only with the exact calling sequence that's used by MS code - it
	   looks like they were only intended as components of Windows-internal
	   implementation of proxy support since they require manual handling
	   of proxy config script downloading, parsing, and all manner of other
	   stuff that really doesn't concern us.  Because of the extreme
	   difficulty in doing anything with these functions we use the WinHTTP
	   approach instead */
	if( pInternetGetProxyInfo == NULL )
		{
		HMODULE hModJS;

		if( ( hModJS = DynamicLoad( "JSProxy.dll" ) ) == NULL )
			return( CRYPT_ERROR_NOTFOUND );

		pInternetGetProxyInfo = ( INTERNETGETPROXYINFO ) \
					GetProcAddress( hModJS, "InternetGetProxyInfo" );
		pInternetInitializeAutoProxyDll = ( INTERNETINITIALIZEAUTOPROXYDLL ) \
					GetProcAddress( hModJS, "InternetInitializeAutoProxyDll" );
		if( pInternetGetProxyInfo == NULL || \
			pInternetInitializeAutoProxyDll == NULL )
			{
			DynamicUnload( hModJS );
			return( CRYPT_ERROR_NOTFOUND );
			}

		pInternetInitializeAutoProxyDll( 0, TempFile, NULL,
										 &HelperFunctions, NULL )
		}

	/* InternetGetProxyInfo() is a somewhat screwball undocumented function
	   that was crowbarred out of MS as part of the DoJ consent decree.  It
	   takes as input four parameters that do the work of a single
	   parameter, the null-terminated target URL string.  The documentation
	   for the function was initially wrong but has since been partially
	   corrected in places after user complaints, although there are still 
	   missing parts as well as possible errors (why is it necessary to 
	   specify a length for a supposedly null-terminated string?).  In order 
	   to meet the strange input-parameter requirements we have to pre-
	   parse the target URL in order to provide the various bits and pieces 
	   that InternetGetProxyInfo() requires */
	status = parseURL( &urlInfo, url, strlen( url ), 80, URL_TYPE_HTTP );
	if( cryptStatusError( status ) )
		return( status );
	if( urlInfo.hostLen > MAX_DNS_SIZE )
		return( CRYPT_ERROR_OVERFLOW );
	memcpy( urlHost, urlInfo.host, urlInfo.hostLen );
	urlHost[ urlInfo.hostLen ] = '\0';
	if( !pInternetGetProxyInfo( url, strlen( url ), urlHost, urlInfo.hostLen,
								&proxyHost, &proxyHostLen ) )
		return( CRYPT_ERROR_NOTFOUND );
	memcpy( proxy, proxyHost, proxyHostLen );
	proxy[ proxyHostLen ] = '\0';
	GlobalFree( proxyHost );
	return( CRYPT_OK );
	}
#endif /* 0 */

#endif /* Win32 */

#endif /* USE_TCP */
