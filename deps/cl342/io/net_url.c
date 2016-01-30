/****************************************************************************
*																			*
*					Network Stream URL Processing Functions					*
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
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the URL parse state */

#define checkUrlValue( data, dataLength, minLength, maxLength ) \
		( ( data == NULL && dataLength == 0 ) || \
		  ( data != NULL && \
			dataLength >= minLength && dataLength <= maxLength ) )

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckURL( const URL_INFO *urlInfo )
	{
	assert( isReadPtr( urlInfo, sizeof( URL_INFO ) ) );

	/* Make sure that the URL type is valid.  URL_TYPE_NONE is valid since 
	   it denotes an unrecognised URL type */
	if( urlInfo->type < URL_TYPE_NONE || urlInfo->type >= URL_TYPE_LAST )
		return( FALSE );

	/* Make sure the schema, user info, and location are either absent or 
	   have valid values */
	if( !checkUrlValue( urlInfo->schema, urlInfo->schemaLen, 
						2 + 3, 16 + 3 ) )	/* [...] + "://" */
		return( FALSE );
	if( !checkUrlValue( urlInfo->userInfo, urlInfo->userInfoLen, 
						1, CRYPT_MAX_TEXTSIZE ) )
		return( FALSE );
	if( !checkUrlValue( urlInfo->location, urlInfo->locationLen, 
						1, CRYPT_MAX_TEXTSIZE ) )
		return( FALSE );

	/* The host always has to be present */
	if( urlInfo->host == NULL || \
		urlInfo->hostLen < 1 || urlInfo->hostLen > MAX_URL_SIZE )
		return( FALSE );

	return( TRUE );
	}

/****************************************************************************
*																			*
*							URL Processing Functions						*
*																			*
****************************************************************************/

/* Check a schema */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkSchema( IN_BUFFER( schemaLen ) const void *schema, 
						IN_RANGE( 2, 16 ) const int schemaLen,
						OUT_ENUM_OPT( URL_TYPE ) URL_TYPE *urlType,
						IN_ENUM_OPT( URL_TYPE ) URL_TYPE urlTypeHint )
	{
	typedef struct {
		BUFFER_FIXED( schemaLength ) \
		const char *schema;
		const int schemaLength;
		const URL_TYPE type;
		} URL_SCHEMA_INFO;
	static const URL_SCHEMA_INFO FAR_BSS urlSchemaInfo[] = {
		{ "http://", 7, URL_TYPE_HTTP },
		{ "https://", 8, URL_TYPE_HTTPS },
		{ "ssh://", 6, URL_TYPE_SSH },
		{ "scp://", 6, URL_TYPE_SSH },
		{ "sftp://", 7, URL_TYPE_SSH },
		{ "cmp://", 6, URL_TYPE_CMP },
		{ "tsp://", 6, URL_TYPE_TSP },
		{ "ldap://", 7, URL_TYPE_LDAP },
		{ NULL, 0, URL_TYPE_NONE }, { NULL, 0, URL_TYPE_NONE }
		};
	URL_TYPE type;
	int i;

	assert( isReadPtr( schema, schemaLen ) );
	assert( isWritePtr( urlType, sizeof( URL_TYPE ) ) );

	REQUIRES( schemaLen >= 2 && schemaLen <= 16 );
	REQUIRES( urlTypeHint >= URL_TYPE_NONE && urlTypeHint < URL_TYPE_LAST );

	/* Clear return value */
	*urlType = URL_TYPE_NONE;

	/* Check whether the schema is one that we recognise */
	for( i = 0; 
		 urlSchemaInfo[ i ].type != URL_TYPE_NONE && \
			i < FAILSAFE_ARRAYSIZE( urlSchemaInfo, URL_SCHEMA_INFO ); 
		 i++ )
		{
		if( urlSchemaInfo[ i ].schemaLength == schemaLen && \
			!strCompare( urlSchemaInfo[ i ].schema, schema, schemaLen ) )
			break;
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( urlSchemaInfo, URL_SCHEMA_INFO ) );
	type = urlSchemaInfo[ i ].type;

	/* If there's no URL hint given, we're done */
	if( urlTypeHint == URL_TYPE_NONE )
		{
		*urlType = type;

		return( CRYPT_OK );
		}

	/* Make sure that the URL type matches the hint */
	switch( urlTypeHint )
		{
		case URL_TYPE_HTTP:
			/* An explicit HTTP URL must really be HTTP and not just a 
			   generic HTTP/HTTPS mix */
			if( type != URL_TYPE_HTTP )
				return( CRYPT_ERROR_BADDATA );
			break;

		case URL_TYPE_HTTPS:
			/* A requirement for an HTTPS URL can also match an HTTP URL, 
			   this type is used for SSL in which the use of HTTPS is 
			   implied by the fact that an SSL session is being used even if 
			   it's a straight HTTP URL */
			if( type != URL_TYPE_HTTP && type != URL_TYPE_HTTPS )
				return( CRYPT_ERROR_BADDATA );
			break;

		case URL_TYPE_SSH:
			if( type != URL_TYPE_SSH )
				return( CRYPT_ERROR_BADDATA );
			break;

		case URL_TYPE_CMP:
			if( type != URL_TYPE_CMP )
				return( CRYPT_ERROR_BADDATA );
			break;

		case URL_TYPE_TSP:
			if( type != URL_TYPE_TSP )
				return( CRYPT_ERROR_BADDATA );
			break;

		case URL_TYPE_LDAP:
			if( type != URL_TYPE_LDAP )
				return( CRYPT_ERROR_BADDATA );
			break;

		default:
			retIntError();
		}
	*urlType = type;

	return( CRYPT_OK );
	}

/* Parse a URI into:

	<schema>://[<user>@]<host>[:<port>]/<path>[?<query>] components

   This function is intended for use from the internal interface (i.e. to
   parse URLs supplied by the caller to the cryptlib API) and not so much
   for the external interface (i.e. URLs supplied by remote systems for
   processing by cryptlib).  Because of this it's rather more liberal with
   what it'll accept than a generic URL parser would be */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int parseURL( OUT URL_INFO *urlInfo, 
			  IN_BUFFER( urlLen ) const char *url, 
			  IN_LENGTH_SHORT const int urlLen,
			  IN_PORT_OPT const int defaultPort, 
			  IN_ENUM( URL_TYPE ) const URL_TYPE urlTypeHint,
			  const BOOLEAN preParseOnly )
	{
	const char *strPtr, *hostName, *location;
	int strLen, hostNameLen, locationLen, offset, minLen, status;

	assert( isWritePtr( urlInfo, sizeof( URL_INFO ) ) );
	assert( isReadPtr( url, urlLen ) );

	REQUIRES( urlLen > 0 && urlLen < MAX_INTLENGTH_SHORT );
	REQUIRES( defaultPort == CRYPT_UNUSED || \
			  ( defaultPort >= MIN_PORT_NUMBER && \
				defaultPort <= MAX_PORT_NUMBER ) );
	REQUIRES( urlTypeHint >= URL_TYPE_NONE && urlTypeHint < URL_TYPE_LAST );

	/* Clear return values */
	memset( urlInfo, 0, sizeof( URL_INFO ) );
	if( defaultPort != CRYPT_UNUSED )
		urlInfo->port = defaultPort;

	/* Make sure that the input contains valid characters */
	for( offset = 0; offset < urlLen; offset++ )
		{
		const int ch = byteToInt( url[ offset ] );

		if( ch <= 0 || ch > 0x7F || !isPrint( ch ) )
			return( CRYPT_ERROR_BADDATA );
		}

	/* Skip leading and trailing whitespace */
	strLen = strStripWhitespace( &strPtr, url, urlLen );
	if( strLen < MIN_DNS_SIZE || strLen >= MAX_URL_SIZE )
		return( CRYPT_ERROR_BADDATA );
	ANALYSER_HINT( strPtr != NULL );

	/* Strip syntactic sugar */
	if( ( offset = strFindStr( strPtr, strLen, "://", 3 ) ) >= 0 )
		{
		/* Extract the URI schema */
		if( offset < 2 || offset > 8 )
			return( CRYPT_ERROR_BADDATA );
		offset += 3;	/* Adjust for "://" */
		urlInfo->schema = strPtr;
		urlInfo->schemaLen = offset;
		strLen = strExtract( &strPtr, strPtr, offset, strLen );
		if( strLen < MIN_DNS_SIZE || strLen > MAX_URL_SIZE )
			return( CRYPT_ERROR_BADDATA );

		/* Check whether the schema is one that we recognise */
		status = checkSchema( urlInfo->schema, urlInfo->schemaLen, 
							  &urlInfo->type, urlTypeHint );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Check for user info before an '@' sign */
	if( ( offset = strFindCh( strPtr, strLen, '@' ) ) >= 0 )
		{
		const char *userInfo;
		int userInfoLen;

		/* Extract the user info */
		if( offset < 1 || offset > MAX_URL_SIZE )
			return( CRYPT_ERROR_BADDATA );
		userInfoLen = strExtract( &userInfo, strPtr, 0, offset );
		if( userInfoLen < 1 || userInfoLen > CRYPT_MAX_TEXTSIZE )
			return( CRYPT_ERROR_BADDATA );
		urlInfo->userInfo = userInfo;
		urlInfo->userInfoLen = userInfoLen;

		/* Skip the user info */
		strLen = strExtract( &strPtr, strPtr, offset + 1, strLen );
		if( strLen < MIN_DNS_SIZE || strLen > MAX_URL_SIZE )
			return( CRYPT_ERROR_BADDATA );
		}

	/* RFC 2732 requires that IPv6 addresses in URLs be delimited by square 
	   brackets (at least one reason being that they use colons in their 
	   string representation, which would conflict with the way that ports
	   are denoted in URLs) so if we find one at the start of the URI we 
	   treat it as an IPv6 address */
	if( *strPtr == '[' && \
		( strLen != 12 || strCompare( strPtr, "[Autodetect]", 12 ) ) )
		{
		/* Locate the end of the RFC 2732 IPv6 address.  The returned offset 
		   can't be greater than the length - 1 but we make the check 
		   explicit here to be sure */
		if( ( offset = strFindCh( strPtr, strLen, ']' ) ) <= 0 )
			return( CRYPT_ERROR_BADDATA );
		if( offset < 2 || offset > strLen - 1 || offset > CRYPT_MAX_TEXTSIZE )
			return( CRYPT_ERROR_BADDATA );

		/* If we're only pre-parsing the IPv6 address for future use rather 
		   than actually parsing it to pass to the network address-
		   resolution functions then we have to leave the square-bracket 
		   delimiters in place for when we perform the actual parse later 
		   on */
		if( preParseOnly )
			{
			hostName = strPtr;
			hostNameLen = offset + 1;	/* Include ']' */
			minLen = 4;
			}
		else
			{
			/* Extract the IPv6 address starting at position 1 (past the 
			   '[') and ending at position 'offset' (before the ']') with 
			   minimum length 2 */
			hostNameLen = strExtract( &hostName, strPtr, 1, offset );
			minLen = 2;
			}
		offset++;	/* Skip ']' */
		}
	else
		{
		int offset2;

		/* It's a non-IPv6 host name, check whether there's anything
		   following the name */
		offset = strFindCh( strPtr, strLen, ':' );
		offset2 = strFindCh( strPtr, strLen, '/' );
		if( offset < 0 )
			offset = offset2;
		else
			{
			REQUIRES( offset >= 0 );
			if( offset2 >= 0 )
				offset = min( offset, offset2 );
			}
		if( offset <= 0 )
			{
			/* The remaining string is the server name, we're done (the 
			   string has already been trimmed in earlier code) */
			urlInfo->host = strPtr;
			urlInfo->hostLen = strLen;

			ENSURES( sanityCheckURL( urlInfo ) );

			return( CRYPT_OK );
			}

		/* There's port/location info following the server name.  Trailing
		   whitespace will be stripped later */
		hostNameLen = strExtract( &hostName, strPtr, 0, offset );
		minLen = MIN_DNS_SIZE;
		}
	if( hostNameLen < minLen || hostNameLen > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_BADDATA );
	urlInfo->host = hostName;
	urlInfo->hostLen = hostNameLen;

	/* If there's nothing beyond the host name, we're done */
	if( offset >= strLen )
		{
		ENSURES( sanityCheckURL( urlInfo ) );

		return( CRYPT_OK );
		}
	strLen = strExtract( &strPtr, strPtr, offset, strLen );
	if( strLen == 1 && *strPtr == '/' )
		{
		/* URLs may end in an optional no-op trailing '/' */
		ENSURES( sanityCheckURL( urlInfo ) );

		return( CRYPT_OK );
		}
	if( strLen < 3 || strLen > MAX_URL_SIZE )
		return( CRYPT_ERROR_BADDATA );

	/* Check for a port after a ':' */
	if( *strPtr == ':' )
		{
		int portStrLen, port;

		/* Skip the colon */
		strLen = strExtract( &strPtr, strPtr, 1, strLen );
		if( strLen < 2 || strLen > MAX_URL_SIZE )
			return( CRYPT_ERROR_BADDATA );

		/* Get the port to connect to.  If it's an invalid port we ignore it
		   and use the default one, which was set earlier */
		for( portStrLen = 0; 
			 portStrLen < strLen && isDigit( strPtr[ portStrLen ] );
			 portStrLen++ );
		if( portStrLen < 2 || portStrLen > 8 )
			return( CRYPT_ERROR_BADDATA );
		status = strGetNumeric( strPtr, portStrLen, &port, 
								MIN_PORT_NUMBER, MAX_PORT_NUMBER );
		if( cryptStatusError( status ) )
			return( status );
		urlInfo->port = port;

		/* If there's nothing beyond the port, we're done */
		if( strLen == portStrLen )
			{
			ENSURES( sanityCheckURL( urlInfo ) );

			return( CRYPT_OK );
			}
		strLen = strExtract( &strPtr, strPtr, portStrLen, strLen );
		if( strLen == 1 && *strPtr == '/' )
			{
			/* URLs may end in an optional no-op trailing '/' */
			ENSURES( sanityCheckURL( urlInfo ) );

			return( CRYPT_OK );
			}
		if( strLen < 3 || strLen > MAX_URL_SIZE )
			return( CRYPT_ERROR_BADDATA );
		}

	/* What's left has to be a location */
	if( *strPtr != '/' )
		return( CRYPT_ERROR_BADDATA );

	/* The location string includes the leading '/' so we set the start 
	   offset to 0 and not 1 */
	locationLen = strExtract( &location, strPtr, 0, strLen );
	if( locationLen < 3 || locationLen > MAX_URL_SIZE )
		return( CRYPT_ERROR_BADDATA );
	urlInfo->location = location;
	urlInfo->locationLen = locationLen;

	ENSURES( sanityCheckURL( urlInfo ) );

	return( CRYPT_OK );
	}

#endif /* USE_TCP */
