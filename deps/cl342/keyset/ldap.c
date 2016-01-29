/****************************************************************************
*																			*
*						cryptlib LDAP Mapping Routines						*
*					  Copyright Peter Gutmann 1998-2004						*
*																			*
****************************************************************************/

/* The following code can be built to use most of the various subtly 
   incompatible LDAP clients.  By default the Windows client is used under 
   Windows and the OpenLDAP client is used elsewhere, this can be overridden 
   by defining NETSCAPE_CLIENT which causes the Netscape client to be used 
   instead.  Old versions of the Windows client were considerably more buggy 
   than the Netscape one, so if you get data corruption and other problems 
   try switching to the Netscape client (see the comment next to ber_free() 
   for more details on some of these problems).  Note that there are at least
   five incompatible LDAP APIs, the one defined in the RFCs, the older 
   OpenLDAP API, the newer OpenLDAP API, the Windows API, and the Netscape 
   API.  The following code tries to auto-adjust itself for all of the
   different versions, but it may need some hand-tweaking.

   A generalisation of this is that you shouldn't be using LDAP for
   certificate storage at all unless you're absolutely forced to.  LDAP
   is a truly awful mechanism for storing and retrieving certificates,
   technical reasons for this may be found in the Godzilla crypto tutorial
   and in any database text written within the last 20 years */

#if defined( INC_ALL )
  #include "crypt.h"
  #include "keyset.h"
#else
  #include "crypt.h"
  #include "keyset/keyset.h"
#endif /* Compiler-specific includes */

#ifdef USE_LDAP

#if defined( _MSC_VER ) || defined( __GNUC__ )
  #pragma message( "  Building with LDAP enabled." )
#endif /* Warn with VC++ */

/* LDAP requires us to set up complicated structures to handle DN's.  The
   following values define the upper limit for DN string data and the
   maximum number of attributes we write to a directory */

#define MAX_DN_STRINGSIZE		1024
#define MAX_LDAP_ATTRIBUTES		20

/* These should really be taken from the system include directory but this
   leads to too many complaints from people who don't read the LDAP
   installation section of the manual */

#if defined( __WINDOWS__ )
  /* cryptlib.h includes a trap for inclusion of wincrypt.h before 
     cryptlib.h which results in a compiler error if both files are 
	 included.  To disable this, we need to undefine the CRYPT_MODE_ECB 
	 defined in cryptlib.h */
  #undef CRYPT_MODE_ECB
  #include <winldap.h>
  #define LDAP_API		LDAPAPI		/* Windows LDAP API type */
  #define timeval		l_timeval	/* Windows uses nonstandard name */
#elif defined( NETSCAPE_CLIENT )
  #include <ldap.h>
  #define LDAP_API		LDAP_CALL	/* Netscape LDAP API type */
  #define ber_free		ldap_ber_free	/* Netscape uses nonstandard name */
#else
  #include <ldap.h>
  #include <sys/time.h>				/* For 'struct timeval' */
  #ifdef LDAP_API
	/* Some OpenLDAP versions have their own LDAP_API macro which is 
	   incompatible with the usage here, so we clear it before we define our
	   own */
	#undef LDAP_API
  #endif /* LDAP_API */
  #define LDAP_API					/* OpenLDAP LDAP API type */
#endif /* Different LDAP client types */

/****************************************************************************
*																			*
*						 	Windows Init/Shutdown Routines					*
*																			*
****************************************************************************/

#ifdef DYNAMIC_LOAD

/* Global function pointers.  These are necessary because the functions need
   to be dynamically linked since older systems won't contain the necessary
   DLL's.  Explicitly linking to them will make cryptlib unloadable on these 
   systems */

static INSTANCE_HANDLE hLDAP = NULL_INSTANCE;

typedef void ( LDAP_API *BER_FREE )( BerElement *ber, int freebuf );
typedef int ( LDAP_API *LDAP_ADD_S )( LDAP *ld, const char *dn, LDAPMod **attrs );
typedef int ( LDAP_API *LDAP_DELETE_S )( LDAP *ld, const char *dn );
typedef char * ( LDAP_API *LDAP_ERR2STRING )( int err );
typedef char * ( LDAP_API *LDAP_FIRST_ATTRIBUTE )( LDAP *ld, LDAPMessage *entry,
										  BerElement **ber );
typedef LDAPMessage * ( LDAP_API *LDAP_FIRST_ENTRY )( LDAP *ld, LDAPMessage *result );
#if defined( __WINDOWS__ )
  typedef int ( LDAP_API *LDAP_GETLASTERROR )( void );
#elif defined( NETSCAPE_CLIENT )
  typedef int ( LDAP_API *LDAP_GET_LDERRNO )( LDAP *ld, char **m, char **s );
#else
  typedef int ( LDAP_API *LDAP_GET_OPTION )( LDAP *ld, int option, void *outvalue );
#endif /* Different LDAP client types */
typedef struct berval ** ( LDAP_API *LDAP_GET_VALUES_LEN )( LDAP *ld, LDAPMessage *entry,
												   const char *attr );
typedef LDAP * ( LDAP_API *LDAP_INIT )( const char *host, int port );
typedef int ( LDAP_API *LDAP_IS_LDAP_URL )( char *url );
typedef void ( LDAP_API *LDAP_MEMFREE )( void *p );
typedef void ( LDAP_API *LDAP_MODSFREE )( LDAPMod **mods, int freemods );
typedef int ( LDAP_API *LDAP_MSGFREE )( LDAPMessage *lm );
typedef LDAPMessage * ( LDAP_API *LDAP_NEXT_ENTRY )( LDAP *ld, LDAPMessage *result );
typedef int ( LDAP_API *LDAP_SEARCH_ST )( LDAP *ld, const char *base, int scope,
								const char *filter, char **attrs,
								int attrsonly, struct timeval *timeout,
								LDAPMessage **res );
typedef int ( LDAP_API *LDAP_SET_OPTION )( LDAP *ld, int option, void *optdata );
typedef int ( LDAP_API *LDAP_SIMPLE_BIND_S )( LDAP *ld, const char *who,
									 const char *passwd );
typedef int ( LDAP_API *LDAP_UNBIND )( LDAP *ld );
typedef int ( LDAP_API *LDAP_URL_SEARCH_ST )( LDAP *ld, char *url, int attrsonly,
											  struct timeval *timeout,
											  LDAPMessage **res );
typedef void ( LDAP_API *LDAP_VALUE_FREE_LEN )( struct berval **vals );
#if defined( __WINDOWS__ ) || defined( NETSCAPE_CLIENT )
  static BER_FREE p_ber_free = NULL;
#endif /* __WINDOWS__ || NETSCAPE_CLIENT */
static LDAP_ADD_S p_ldap_add_s = NULL;
static LDAP_DELETE_S p_ldap_delete_s = NULL;
static LDAP_ERR2STRING p_ldap_err2string = NULL;
static LDAP_FIRST_ATTRIBUTE p_ldap_first_attribute = NULL;
static LDAP_FIRST_ENTRY p_ldap_first_entry = NULL;
#if defined( __WINDOWS__ )
  static LDAP_GETLASTERROR p_LdapGetLastError = NULL;
#elif defined( NETSCAPE_CLIENT )
  static LDAP_GET_LDERRNO p_ldap_get_lderrno = NULL;
#else
  static LDAP_GET_OPTION p_ldap_get_option = NULL;
#endif /* Different LDAP client types */
static LDAP_GET_VALUES_LEN p_ldap_get_values_len = NULL;
static LDAP_INIT p_ldap_init = NULL;
static LDAP_IS_LDAP_URL p_ldap_is_ldap_url = NULL;
static LDAP_MEMFREE p_ldap_memfree = NULL;
static LDAP_NEXT_ENTRY p_ldap_next_entry = NULL;
static LDAP_MSGFREE p_ldap_msgfree = NULL;
static LDAP_SEARCH_ST p_ldap_search_st = NULL;
static LDAP_SET_OPTION p_ldap_set_option = NULL;
static LDAP_SIMPLE_BIND_S p_ldap_simple_bind_s = NULL;
static LDAP_UNBIND p_ldap_unbind = NULL;
static LDAP_URL_SEARCH_ST p_ldap_url_search_st = NULL;
static LDAP_VALUE_FREE_LEN p_ldap_value_free_len = NULL;

/* The use of dynamically bound function pointers vs.statically linked
   functions requires a bit of sleight of hand since we can't give the
   pointers the same names as prototyped functions.  To get around this we
   redefine the actual function names to the names of the pointers */

#define ber_free				p_ber_free
#define ldap_add_s				p_ldap_add_s
#define ldap_delete_s			p_ldap_delete_s
#define ldap_err2string			p_ldap_err2string
#define ldap_first_attribute	p_ldap_first_attribute
#define ldap_first_entry		p_ldap_first_entry
#if defined( __WINDOWS__ )
  #define LdapGetLastError		p_LdapGetLastError
#elif defined( NETSCAPE_CLIENT )
  #define ldap_get_lderrno		p_ldap_get_lderrno
#else
  #define ldap_get_option		p_ldap_get_option
#endif /* Different LDAP client types */
#define ldap_get_values_len		p_ldap_get_values_len
#define ldap_init				p_ldap_init
#define ldap_is_ldap_url		p_ldap_is_ldap_url
#define ldap_memfree			p_ldap_memfree
#define ldap_msgfree			p_ldap_msgfree
#define ldap_next_entry			p_ldap_next_entry
#define ldap_search_st			p_ldap_search_st
#define ldap_set_option			p_ldap_set_option
#define ldap_simple_bind_s		p_ldap_simple_bind_s
#define ldap_unbind				p_ldap_unbind
#define ldap_url_search_st		p_ldap_url_search_st
#define ldap_value_free_len		p_ldap_value_free_len

/* The name of the LDAP driver, in this case the Netscape LDAPv3 driver */

#ifdef __WIN16__
  #define LDAP_LIBNAME			"NSLDSS16.DLL"
#elif defined( __WIN32__ )
  #define LDAP_LIBNAME			"wldap32.dll"
#elif defined( __UNIX__ )
  #if defined( __APPLE__ )
	/* OS X has built-in LDAP support via OpenLDAP */
	#define LDAP_LIBNAME		"libldap.dylib"
  #elif defined NETSCAPE_CLIENT
	#define LDAP_LIBNAME		"libldap50.so"
  #else
	#define LDAP_LIBNAME		"libldap.so"
  #endif /* NETSCAPE_CLIENT */
#endif /* System-specific ODBC library names */

/* Dynamically load and unload any necessary LDAP libraries */

CHECK_RETVAL \
int dbxInitLDAP( void )
	{
#ifdef __WIN16__
	UINT errorMode;
#endif /* __WIN16__ */

	/* If the LDAP module is already linked in, don't do anything */
	if( hLDAP != NULL_INSTANCE )
		return( CRYPT_OK );

	/* Obtain a handle to the module containing the LDAP functions */
#ifdef __WIN16__
	errorMode = SetErrorMode( SEM_NOOPENFILEERRORBOX );
	hLDAP = LoadLibrary( LDAP_LIBNAME );
	SetErrorMode( errorMode );
	if( hLDAP < HINSTANCE_ERROR )
		{
		hLDAP = NULL_INSTANCE;
		return( CRYPT_ERROR );
		}
#else
	if( ( hLDAP = DynamicLoad( LDAP_LIBNAME ) ) == NULL_INSTANCE )
		return( CRYPT_ERROR );
#endif /* __WIN32__ */

	/* Now get pointers to the functions */
#if defined( __WINDOWS__ )
	p_ber_free = ( BER_FREE ) DynamicBind( hLDAP, "ber_free" );
#elif defined( NETSCAPE_CLIENT )
	p_ber_free = ( BER_FREE ) DynamicBind( hLDAP, "ldap_ber_free" );
#endif /* __WINDOWS__ || NETSCAPE_CLIENT */
	p_ldap_add_s = ( LDAP_ADD_S ) DynamicBind( hLDAP, "ldap_add_s" );
	p_ldap_delete_s = ( LDAP_DELETE_S ) DynamicBind( hLDAP, "ldap_delete_s" );
	p_ldap_err2string = ( LDAP_ERR2STRING ) DynamicBind( hLDAP, "ldap_err2string" );
	p_ldap_first_attribute = ( LDAP_FIRST_ATTRIBUTE ) DynamicBind( hLDAP, "ldap_first_attribute" );
	p_ldap_first_entry = ( LDAP_FIRST_ENTRY ) DynamicBind( hLDAP, "ldap_first_entry" );
#if defined( __WINDOWS__ )
	p_LdapGetLastError = ( LDAP_GETLASTERROR ) DynamicBind( hLDAP, "LdapGetLastError" );
#elif defined( NETSCAPE_CLIENT )
	p_ldap_get_lderrno = ( LDAP_GET_LDERRNO ) DynamicBind( hLDAP, "ldap_get_lderrno" );
#else
	p_ldap_get_option = ( LDAP_GET_OPTION ) DynamicBind( hLDAP, "ldap_get_option" );
#endif /* Different LDAP client types */
	p_ldap_get_values_len = ( LDAP_GET_VALUES_LEN ) DynamicBind( hLDAP, "ldap_get_values_len" );
	p_ldap_init = ( LDAP_INIT ) DynamicBind( hLDAP, "ldap_init" );
	p_ldap_is_ldap_url = ( LDAP_IS_LDAP_URL ) DynamicBind( hLDAP, "ldap_is_ldap_url" );
	p_ldap_memfree = ( LDAP_MEMFREE ) DynamicBind( hLDAP, "ldap_memfree" );
	p_ldap_msgfree = ( LDAP_MSGFREE ) DynamicBind( hLDAP, "ldap_msgfree" );
	p_ldap_next_entry = ( LDAP_NEXT_ENTRY ) DynamicBind( hLDAP, "ldap_next_entry" );
	p_ldap_search_st = ( LDAP_SEARCH_ST ) DynamicBind( hLDAP, "ldap_search_st" );
	p_ldap_set_option = ( LDAP_SET_OPTION ) DynamicBind( hLDAP, "ldap_set_option" );
	p_ldap_simple_bind_s = ( LDAP_SIMPLE_BIND_S ) DynamicBind( hLDAP, "ldap_simple_bind_s" );
	p_ldap_unbind = ( LDAP_UNBIND ) DynamicBind( hLDAP, "ldap_unbind" );
	p_ldap_url_search_st = ( LDAP_URL_SEARCH_ST ) DynamicBind( hLDAP, "ldap_url_search_st" );
	p_ldap_value_free_len = ( LDAP_VALUE_FREE_LEN ) DynamicBind( hLDAP, "ldap_value_free_len" );

	/* Make sure that we got valid pointers for every LDAP function */
	if( p_ldap_add_s == NULL ||
#ifdef NETSCAPE_CLIENT
		p_ber_free == NULL ||
#endif /* NETSCAPE_CLIENT */
		p_ldap_delete_s == NULL || p_ldap_err2string == NULL || \
		p_ldap_first_attribute == NULL || p_ldap_first_entry == NULL || \
		p_ldap_init == NULL ||
#if defined( __WINDOWS__ )
		p_LdapGetLastError == NULL ||
#elif defined( NETSCAPE_CLIENT )
		p_ldap_get_lderrno == NULL || p_ldap_is_ldap_url == NULL ||
		p_ldap_url_search_st == NULL ||
#else
		p_ldap_get_option == NULL ||
#endif /* NETSCAPE_CLIENT */
		p_ldap_get_values_len == NULL || p_ldap_memfree == NULL || \
		p_ldap_msgfree == NULL || p_ldap_next_entry == NULL || \
		p_ldap_search_st == NULL || p_ldap_set_option == NULL || \
		p_ldap_simple_bind_s == NULL || p_ldap_unbind == NULL || \
		p_ldap_value_free_len == NULL )
		{
		/* Free the library reference and reset the handle */
		DynamicUnload( hLDAP );
		hLDAP = NULL_INSTANCE;
		return( CRYPT_ERROR );
		}

	/* Some versions of OpenLDAP define ldap_is_ldap_url() but not 
	   ldap_url_search_st() (which makes the former more or less useless), 
	   so if the latter isn't defined we remove the former as well */
	if( p_ldap_url_search_st == NULL )
		p_ldap_is_ldap_url = NULL;

	return( CRYPT_OK );
	}

void dbxEndLDAP( void )
	{
	if( hLDAP != NULL_INSTANCE )
		DynamicUnload( hLDAP );
	hLDAP = NULL_INSTANCE;
	}
#else

CHECK_RETVAL \
int dbxInitLDAP( void )
	{
	return( CRYPT_OK );
	}

void dbxEndLDAP( void )
	{
	}
#endif /* __WINDOWS__ */

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Assign a name for an LDAP object/attribute field */

static void assignFieldName( const CRYPT_USER cryptOwner, char *buffer,
							 CRYPT_ATTRIBUTE_TYPE option )
	{
	MESSAGE_DATA msgData;
	int status;

	setMessageData( &msgData, buffer, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( cryptOwner, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, option );
	assert( cryptStatusOK( status ) );
	buffer[ msgData.length ] = '\0';
	}

/* Get information on an LDAP error */

static void getErrorInfo( KEYSET_INFO *keysetInfoPtr, int ldapStatus )
	{
#ifdef USE_ERRMSGS
	ERROR_INFO *errorInfo = &keysetInfoPtr->errorInfo;
#endif /* USE_ERRMSGS */
#ifndef __WINDOWS__ 
	LDAP_INFO *ldapInfo = keysetInfoPtr->keysetLDAP;
#endif /* !__WINDOWS__ */
#ifdef __WINDOWS__
	int errorCode;
#endif /* __WINDOWS__ */
	char *errorMessage;

#if defined( __WINDOWS__ )
	errorCode = LdapGetLastError();
	if( errorCode == LDAP_SUCCESS )
		{
		/* In true Microsoft fashion LdapGetLastError() can return
		   LDAP_SUCCESS with the error string set to "Success." so if we
		   get this we use the status value returned by the original LDAP
		   function call instead */
		errorCode = ldapStatus;
		}
	errorMessage = ldap_err2string( errorCode );
  #if 0
	/* The exact conditions under which ldap_err2string() does something
	   useful are somewhat undefined, it may be necessary to use the
	   following which works with general Windows error codes rather than
	   special-case LDAP function result codes */
	errorCode = GetLastError();
	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				   NULL, errorCode, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
				   ldapInfo->errorMessage, MAX_ERRMSG_SIZE - 1, NULL );
  #endif /* 0 */
#elif defined( NETSCAPE_CLIENT )
	ldap_get_lderrno( ldapInfo->ld, NULL, &errorMessage );
#else
	ldap_get_option( ldapInfo->ld, LDAP_OPT_ERROR_STRING, &errorMessage );
#endif /* Different LDAP client types */
	if( errorMessage != NULL )
		{
		setErrorString( errorInfo, errorMessage, strlen( errorMessage ) );
		}
	else
		clearErrorString( errorInfo );
	}

/* Map an LDAP error to the corresponding cryptlib error.  The various LDAP
   imlpementations differ slightly in their error codes, so we have to 
   adjust them as appropriate */

static int mapLdapError( const int ldapError, const int defaultError )
	{
	switch( ldapError )
		{
		case LDAP_INAPPROPRIATE_AUTH:
		case LDAP_INVALID_CREDENTIALS:
		case LDAP_AUTH_UNKNOWN:
#if defined( __WINDOWS__ )
		case LDAP_INSUFFICIENT_RIGHTS:
		case LDAP_AUTH_METHOD_NOT_SUPPORTED:
#elif defined( NETSCAPE_CLIENT )
		case LDAP_INSUFFICIENT_ACCESS:
#else
		case LDAP_INSUFFICIENT_ACCESS:
		case LDAP_AUTH_METHOD_NOT_SUPPORTED:
#endif /* Different client types */
			return( CRYPT_ERROR_PERMISSION );

#if defined( __WINDOWS__ )
		case LDAP_ATTRIBUTE_OR_VALUE_EXISTS:
		case LDAP_ALREADY_EXISTS:
#elif defined( NETSCAPE_CLIENT )
		case LDAP_TYPE_OR_VALUE_EXISTS:
#else
		case LDAP_TYPE_OR_VALUE_EXISTS:
		case LDAP_ALREADY_EXISTS:
#endif /* Different client types */
			return( CRYPT_ERROR_DUPLICATE );

#if defined( __WINDOWS__ )
		case LDAP_CONFIDENTIALITY_REQUIRED:
#elif defined( NETSCAPE_CLIENT )
		/* Nothing */
#else
		case LDAP_CONFIDENTIALITY_REQUIRED:
		case LDAP_STRONG_AUTH_REQUIRED:
			return( CRYPT_ERROR_NOSECURE );
#endif /* Different client types */

		case LDAP_INVALID_DN_SYNTAX:
			return( CRYPT_ARGERROR_STR1 );

		case LDAP_TIMELIMIT_EXCEEDED:
		case LDAP_TIMEOUT:
			return( CRYPT_ERROR_TIMEOUT );

#ifndef NETSCAPE_CLIENT
		case LDAP_NO_RESULTS_RETURNED:
#endif /* NETSCAPE_CLIENT */
		case LDAP_NO_SUCH_ATTRIBUTE:
		case LDAP_NO_SUCH_OBJECT:
			return( CRYPT_ERROR_NOTFOUND );

#ifndef NETSCAPE_CLIENT
		case LDAP_NOT_SUPPORTED:
		case LDAP_UNAVAILABLE:
			return( CRYPT_ERROR_NOTAVAIL );
#endif /* NETSCAPE_CLIENT */

		case LDAP_SIZELIMIT_EXCEEDED:
		case LDAP_RESULTS_TOO_LARGE:
			return( CRYPT_ERROR_OVERFLOW );

		case LDAP_NO_MEMORY:
			return( CRYPT_ERROR_MEMORY );
		}

	return( defaultError );
	}

/* Copy attribute information into an LDAPMod structure so it can be written to
   the directory */

static LDAPMod *copyAttribute( const char *attributeName,
							   const void *attributeValue,
							   const int attributeLength )
	{
	LDAPMod *ldapModPtr;

	/* Allocate room for the LDAPMod structure */
	if( ( ldapModPtr = ( LDAPMod * ) clAlloc( "copyAttribute", \
											  sizeof( LDAPMod ) ) ) == NULL )
		return( NULL );

	/* Set up the pointers to the attribute information.  This differs
	   slightly depending on whether we're adding text or binary data */
	if( !attributeLength )
		{
		if( ( ldapModPtr->mod_values = \
					clAlloc( "copyAttribute", \
							 2 * sizeof( void * ) ) ) == NULL )
			{
			clFree( "copyAttribute", ldapModPtr );
			return( NULL );
			}
		ldapModPtr->mod_op = LDAP_MOD_ADD;
		ldapModPtr->mod_type = ( char * ) attributeName;
		ldapModPtr->mod_values[ 0 ] = ( char * ) attributeValue;
		ldapModPtr->mod_values[ 1 ] = NULL;
		}
	else
		{
		if( ( ldapModPtr->mod_bvalues = \
					clAlloc( "copyAttribute", \
							 2 * sizeof( struct berval ) ) ) == NULL )
			{
			clFree( "copyAttribute", ldapModPtr );
			return( NULL );
			}
		ldapModPtr->mod_op = LDAP_MOD_ADD | LDAP_MOD_BVALUES;
		ldapModPtr->mod_type = ( char * ) attributeName;
		ldapModPtr->mod_bvalues[ 0 ]->bv_len = attributeLength;
		ldapModPtr->mod_bvalues[ 0 ]->bv_val = ( char * ) attributeValue;
		ldapModPtr->mod_bvalues[ 1 ] = NULL;
		}

	return( ldapModPtr );
	}

/* Encode DN information in the RFC 1779 reversed format.  We don't have to
   explicitly check for overflows (which will lead to truncation of the 
   resulting encoded DN) because the certificate management code limits the 
   size of each component to a small fraction of the total buffer size.  
   Besides which, it's LDAP, anyone using this crap as a certificate store 
   is asking for it anyway */

static void catComponent( char *dest, const int destLen, char *src )
	{
	int i;

	/* Find the end of the existing destination string */
	for( i = 0; i < destLen && dest[ i ] != '\0'; i++ );
	if( i >= destLen )
		retIntError_Void();

	/* Append the source string, escaping special chars */
	while( i < destLen - 1 && *src != '\0' )
		{
		const char ch = *src++;

		if( ch == ',' )
			{
			dest[ i++ ] = '\\';
			if( i >= destLen )
				break;
			}
		dest[ i++ ] = ch;
		}
	dest[ i ] = '\0';
	}

static int encodeDN( char *dn, const int maxDnLen, char *C, char *SP, 
					 char *L, char *O, char *OU, char *CN )
	{
	strlcpy_s( dn, maxDnLen, "CN=" );
	catComponent( dn, maxDnLen, CN );
	if( *OU )
		{
		strlcat_s( dn, maxDnLen, ",OU=" );
		catComponent( dn, maxDnLen, OU );
		}
	if( *O )
		{
		strlcat_s( dn, maxDnLen, ",O=" );
		catComponent( dn, maxDnLen, O );
		}
	if( *L )
		{
		strlcat_s( dn, maxDnLen, ",L=" );
		catComponent( dn, maxDnLen, L );
		}
	if( *SP )
		{
		strlcat_s( dn, maxDnLen, ",ST=" );	/* Not to be confused with ST=street */
		catComponent( dn, maxDnLen, SP );
		}
	strlcat_s( dn, maxDnLen, ",C=" );
	catComponent( dn, maxDnLen, C );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Directory Open/Close Routines					*
*																			*
****************************************************************************/

/* Close a previously-opened LDAP connection.  We have to have this before
   the init function since it may be called by it if the open process fails.
   This is necessary because the complex LDAP open may require a fairly
   extensive cleanup afterwards */

STDC_NONNULL_ARG( ( 1 ) ) \
static int shutdownFunction( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	LDAP_INFO *ldapInfo = keysetInfoPtr->keysetLDAP;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );

	ldap_unbind( ldapInfo->ld );
	ldapInfo->ld = NULL;

	return( CRYPT_OK );
	}

/* Open a connection to an LDAP directory */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initFunction( INOUT KEYSET_INFO *keysetInfoPtr, 
						 IN_BUFFER( nameLength ) const char *name,
						 IN_LENGTH_SHORT const int nameLength,
						 IN_ENUM( CRYPT_KEYOPT ) const CRYPT_KEYOPT_TYPE options )
	{
	LDAP_INFO *ldapInfo = keysetInfoPtr->keysetLDAP;
	URL_INFO urlInfo;
	const char *ldapUser = NULL;
	char ldapServer[ MAX_URL_SIZE + 8 ];
	int maxEntries = 2, timeout, ldapPort, ldapStatus = LDAP_OTHER, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isReadPtr( name, nameLength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );
	REQUIRES( nameLength >= MIN_URL_SIZE && nameLength < MAX_URL_SIZE );
	REQUIRES( options >= CRYPT_KEYOPT_NONE && options < CRYPT_KEYOPT_LAST );

	/* Check the URL.  The Netscape and OpenLDAP APIs provide the function
	   ldap_is_ldap_url() for this, but this requires a complete LDAP URL
	   rather than just a server name and port */
	if( nameLength > MAX_URL_SIZE - 1 )
		return( CRYPT_ARGERROR_STR1 );
	status = sNetParseURL( &urlInfo, name, nameLength, URL_TYPE_LDAP );
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_STR1 );
	memcpy( ldapServer, urlInfo.host, urlInfo.hostLen );
	ldapServer[ urlInfo.hostLen ] = '\0';
	ldapPort = ( urlInfo.port > 0 ) ? urlInfo.port : LDAP_PORT;
	if( urlInfo.locationLen > 0 )
		ldapUser = urlInfo.location;

	/* Open the connection to the server */
	if( ( ldapInfo->ld = ldap_init( ldapServer, ldapPort ) ) == NULL )
		return( CRYPT_ERROR_OPEN );
	if( ( ldapStatus = ldap_simple_bind_s( ldapInfo->ld, ldapUser, 
										   NULL ) ) != LDAP_SUCCESS )
		{
		getErrorInfo( keysetInfoPtr, ldapStatus );
		ldap_unbind( ldapInfo->ld );
		ldapInfo->ld = NULL;
		return( mapLdapError( ldapStatus, CRYPT_ERROR_OPEN ) );
		}

	/* Set the search timeout and limit the maximum number of returned
	   entries to 2 (setting the search timeout is mostly redundant since we
	   use search_st anyway, however there may be other operations which also
	   require some sort of timeout which can't be explicitly specified */
	krnlSendMessage( keysetInfoPtr->ownerHandle, IMESSAGE_GETATTRIBUTE,
					 &timeout, CRYPT_OPTION_NET_READTIMEOUT );
	if( timeout < 15 )
		{
		/* Network I/O may be set to be nonblocking, so we make sure we try
		   for at least 15s before timing out */
		timeout = 15;
		}
	ldap_set_option( ldapInfo->ld, LDAP_OPT_TIMELIMIT, &timeout );
	ldap_set_option( ldapInfo->ld, LDAP_OPT_SIZELIMIT, &maxEntries );

	/* Set up the names of the objects and attributes */
	assignFieldName( keysetInfoPtr->ownerHandle, ldapInfo->nameObjectClass,
					 CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS );
	assignFieldName( keysetInfoPtr->ownerHandle, ldapInfo->nameFilter,
					 CRYPT_OPTION_KEYS_LDAP_FILTER );
	assignFieldName( keysetInfoPtr->ownerHandle, ldapInfo->nameCACert,
					 CRYPT_OPTION_KEYS_LDAP_CACERTNAME );
	assignFieldName( keysetInfoPtr->ownerHandle, ldapInfo->nameCert,
					 CRYPT_OPTION_KEYS_LDAP_CERTNAME );
	assignFieldName( keysetInfoPtr->ownerHandle, ldapInfo->nameCRL,
					 CRYPT_OPTION_KEYS_LDAP_CRLNAME );
	assignFieldName( keysetInfoPtr->ownerHandle, ldapInfo->nameEmail,
					 CRYPT_OPTION_KEYS_LDAP_EMAILNAME );
	krnlSendMessage( keysetInfoPtr->ownerHandle, IMESSAGE_GETATTRIBUTE,
					 &ldapInfo->objectType,
					 CRYPT_OPTION_KEYS_LDAP_OBJECTTYPE );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Directory Access Routines						*
*																			*
****************************************************************************/

/* Send a query to an LDAP server */

static int sendLdapQuery( LDAP_INFO *ldapInfo, LDAPMessage **resultPtr,
						  const CRYPT_HANDLE iOwnerHandle, const char *dn )
	{
	const CRYPT_CERTTYPE_TYPE objectType = ldapInfo->objectType;
	const char *certAttributes[] = { ldapInfo->nameCert, NULL };
	const char *caCertAttributes[] = { ldapInfo->nameCACert, NULL };
	const char *crlAttributes[] = { ldapInfo->nameCRL, NULL };
	struct timeval ldapTimeout = { 0, 0 };
	int ldapStatus = LDAP_OTHER, timeout;

	/* Network I/O may be set to be nonblocking, so we make sure we try for 
	   at least 15s before timing out */
	krnlSendMessage( iOwnerHandle, IMESSAGE_GETATTRIBUTE, &timeout, 
					 CRYPT_OPTION_NET_READTIMEOUT );
	ldapTimeout.tv_sec = max( timeout, 15 );

	/* If the LDAP search-by-URL functions are available and the key ID is 
	   an LDAP URL, perform a search by URL */
	if( ldap_is_ldap_url != NULL && ldap_is_ldap_url( ( char * ) dn ) )
		{
		return( ldap_url_search_st( ldapInfo->ld, ( char * ) dn, FALSE, 
									&ldapTimeout, resultPtr ) );
		}

	/* Try and retrieve the entry for this DN from the directory.  We use a 
	   base specified by the DN, a chop of 0 (to return only the current 
	   entry), any object class (to get around the problem of 
	   implementations which stash certificates in whatever they feel like), 
	   and look for a certificate attribute.  If the search fails for this 
	   attribute, we try again but this time go for a CA certificate 
	   attribute which unfortunately slows down the search somewhat when the 
	   certificate isn't found but can't really be avoided since there's no 
	   way to tell in advance whether a certificate will be an end entity or 
	   a CA certificate.  To complicate things even further, we may also 
	   need to check for a CRL in case this is what the user is after */
	if( objectType == CRYPT_CERTTYPE_NONE || \
		objectType == CRYPT_CERTTYPE_CERTIFICATE )
		{
		ldapStatus = ldap_search_st( ldapInfo->ld, dn, LDAP_SCOPE_BASE,
									 ldapInfo->nameFilter,
									 ( char ** ) certAttributes, 0,
									 &ldapTimeout, resultPtr );
		if( ldapStatus == LDAP_SUCCESS )
			return( ldapStatus );
		}
	if( objectType == CRYPT_CERTTYPE_NONE || \
		objectType == CRYPT_CERTTYPE_CERTIFICATE )
		{
		ldapStatus = ldap_search_st( ldapInfo->ld, dn, LDAP_SCOPE_BASE,
									 ldapInfo->nameFilter,
									 ( char ** ) caCertAttributes, 0,
									 &ldapTimeout, resultPtr );
		if( ldapStatus == LDAP_SUCCESS )
			return( ldapStatus );
		}
	if( objectType == CRYPT_CERTTYPE_NONE || \
		objectType == CRYPT_CERTTYPE_CRL )
		{
		ldapStatus = ldap_search_st( ldapInfo->ld, dn, LDAP_SCOPE_BASE,
									 ldapInfo->nameFilter,
									 ( char ** ) crlAttributes, 0,
									 &ldapTimeout, resultPtr );
		if( ldapStatus == LDAP_SUCCESS )
			return( ldapStatus );
		}

	return( ldapStatus );
	}

/* Retrieve a key attribute from an LDAP directory */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
static int getItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							OUT_HANDLE_OPT CRYPT_HANDLE *iCryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							IN_BUFFER( keyIDlength ) const void *keyID, 
							IN_LENGTH_KEYID const int keyIDlength,
							STDC_UNUSED void *auxInfo, 
							STDC_UNUSED int *auxInfoLength,
							IN_FLAGS_Z( KEYMGMT ) const int flags )
	{
	LDAP_INFO *ldapInfo = keysetInfoPtr->keysetLDAP;
	LDAPMessage *result DUMMY_INIT_PTR, *resultEntry;
	BerElement *ber;
	struct berval **valuePtrs;
	char dn[ MAX_DN_STRINGSIZE + 8 ];
	char *attributePtr;
	int ldapStatus, status = CRYPT_OK;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCryptHandle, sizeof( CRYPT_HANDLE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY );
	REQUIRES( keyIDtype != CRYPT_KEYID_NONE || iCryptHandle != NULL );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( auxInfo == NULL && *auxInfoLength == 0 );
	REQUIRES( flags >= KEYMGMT_FLAG_NONE && flags < KEYMGMT_FLAG_MAX );

	/* Clear return value */
	*iCryptHandle = CRYPT_ERROR;

	/* Convert the DN into a null-terminated form */
	if( keyIDlength > MAX_DN_STRINGSIZE - 1 )
		return( CRYPT_ARGERROR_STR1 );
	memcpy( dn, keyID, keyIDlength );
	dn[ keyIDlength ] = '\0';

	/* Send the LDAP query to the server */
	ldapStatus = sendLdapQuery( ldapInfo, &result, 
								keysetInfoPtr->ownerHandle, dn );
	if( ldapStatus != LDAP_SUCCESS )
		{
		getErrorInfo( keysetInfoPtr, ldapStatus );
		return( mapLdapError( ldapStatus, CRYPT_ERROR_READ ) );
		}

	/* We got something, start fetching the results */
	if( ( resultEntry = ldap_first_entry( ldapInfo->ld, result ) ) == NULL )
		{
		ldap_msgfree( result );
		return( CRYPT_ERROR_NOTFOUND );
		}

	/* Copy out the certificate */
	if( ( attributePtr = ldap_first_attribute( ldapInfo->ld, resultEntry, 
											   &ber ) ) == NULL )
		{
		ldap_msgfree( result );
		return( CRYPT_ERROR_NOTFOUND );
		}
	valuePtrs = ldap_get_values_len( ldapInfo->ld, resultEntry, 
									 attributePtr );
	if( valuePtrs != NULL )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		/* Create a certificate object from the returned data */
		setMessageCreateObjectIndirectInfo( &createInfo, valuePtrs[ 0 ]->bv_val,
											valuePtrs[ 0 ]->bv_len,
											CRYPT_CERTTYPE_NONE );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
								  &createInfo, OBJECT_TYPE_CERTIFICATE );
		if( cryptStatusOK( status ) )
			{
			status = iCryptVerifyID( createInfo.cryptHandle, keyIDtype, 
									 keyID, keyIDlength );
			if( cryptStatusError( status ) )
				{
				krnlSendNotifier( createInfo.cryptHandle, 
								  IMESSAGE_DECREFCOUNT );
				}
			else
				*iCryptHandle = createInfo.cryptHandle;
			}
		ldap_value_free_len( valuePtrs );
		}
	else
		status = CRYPT_ERROR_NOTFOUND;

	/* Clean up.  The ber_free() function is rather problematic because
	   Netscape uses the nonstandard ldap_ber_free() name (which can be fixed
	   with proprocessor trickery), Microsoft first omitted it entirely (up 
	   to NT4 SP4) and then later added it as a stub (Win2K, rumour has it 
	   that the only reason this function even exists is because the Netscape 
	   client required it), and OpenLDAP doesn't use it at all.  Because it 
	   may or may not exist in the MS client, we call it if we resolved its 
	   address, otherwise we skip it.

	   The function is further complicated by the fact that LDAPv3 says the
	   second parameter should be 0, however the Netscape client docs used to
	   require it to be 1 and the MS client was supposed to ignore it so the
	   code passed in a 1.  Actually the way the MS implementation handles
	   the BER data is that the BerElement returned by ldap_first_attribute()
	   is (despite what the MSDN docs claim) just a data structure pointed to
	   by lm_ber in the LDAPMessage structure, all that
	   ldap_first_attribute() does is redirect the lm_ber pointer inside the
	   LDAPMessage, so actually freeing this wouldn't be a good idea).

	   Later, the Netscape docs were updated to require a 0, presumably to
	   align them with the LDAPv3 spec.  On some systems it makes no
	   difference whether you pass in a 0 or 1 to the call, but on others it
	   can cause an access violation.  Presumably eventually everyone will
	   move to something which implements the new rather than old Netscape-
	   documented behaviour, so we pass in 0 as the argument.

	   It gets worse than this though.  Calling ber_free() with newer
	   versions of the Windows LDAP client with any argument at all causes
	   internal data corruption which typically first results in a soft
	   failure (e.g. a data fetch fails) and then eventually a hard failure
	   such as an access violation after further calls are made.  The only
	   real way to fix this is to avoid calling it entirely, this doesn't
	   seem to leak any more memory than Winsock leaks anyway (that is,
	   there are a considerable number of memory and handle leaks, but the
	   number doesn't increase if ber_free() isn't called).

	   There have been reports that with some older versions of the Windows
	   LDAP client (e.g. the one in Win95) the ldap_msgfree() call generates
	   an exception in wldap.dll, if this is a problem you need to either
	   install a newer LDAP DLL or switch to the Netscape one.

	   The reason for some of the Windows problems are because the
	   wldap32.lib shipped with VC++ uses different ordinals than the
	   wldap32.dll which comes with the OS (see MSKB article Q283199), so
	   that simply using the out-of-the-box development tools with the out-
	   of-the-box OS can result in access violations and assorted other
	   problems */
#ifdef NETSCAPE_CLIENT
	if( ber_free != NULL )
		ber_free( ber, 0 );
#endif /* NETSCAPE_CLIENT */
	ldap_memfree( attributePtr );
	ldap_msgfree( result );

	return( status );
	}

/* Add an entry/attribute to an LDAP directory.  The LDAP behaviour differs
   somewhat from DAP in that assigning a value to a nonexistant attribute
   implicitly creates the required attribute.  In addition deleting the last
   value automatically deletes the entire attribute, the delete item code
   assumes the user is requesting a superset of this behaviour and deletes
   the entire entry */

static int addCert( KEYSET_INFO *keysetInfoPtr, 
					const CRYPT_HANDLE iCryptHandle )
	{
	static const int nameValue = CRYPT_CERTINFO_SUBJECTNAME;
	LDAP_INFO *ldapInfo = keysetInfoPtr->keysetLDAP;
	LDAPMod *ldapMod[ MAX_LDAP_ATTRIBUTES + 8 ];
	MESSAGE_DATA msgData;
	BYTE keyData[ MAX_CERT_SIZE + 8 ];
	char dn[ MAX_DN_STRINGSIZE + 8 ];
	char C[ CRYPT_MAX_TEXTSIZE + 1 + 8 ], SP[ CRYPT_MAX_TEXTSIZE + 1 + 8 ],
		L[ CRYPT_MAX_TEXTSIZE + 1 + 8 ], O[ CRYPT_MAX_TEXTSIZE + 1 + 8 ],
		OU[ CRYPT_MAX_TEXTSIZE + 1 + 8 ], CN[ CRYPT_MAX_TEXTSIZE + 1 + 8 ],
		email[ CRYPT_MAX_TEXTSIZE + 1 + 8 ];
	int keyDataLength, ldapModIndex = 1, status;

	*C = *SP = *L = *O = *OU = *CN = *email = '\0';

	/* Extract the DN and altName components.  This changes the currently
	   selected DN components, but this is OK since we've got the 
	   certificate locked and the prior state will be restored when we 
	   unlock it */
	krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE, 
					 ( MESSAGE_CAST ) &nameValue, CRYPT_ATTRIBUTE_CURRENT );
	setMessageData( &msgData, C, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_COUNTRYNAME );
	if( cryptStatusOK( status ) )
		C[ msgData.length ] = '\0';
	if( cryptStatusOK( status ) || status == CRYPT_ERROR_NOTFOUND )
		{
		setMessageData( &msgData, SP, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S, 
							&msgData, CRYPT_CERTINFO_STATEORPROVINCENAME );
		}
	if( cryptStatusOK( status ) )
		SP[ msgData.length ] = '\0';
	if( cryptStatusOK( status ) || status == CRYPT_ERROR_NOTFOUND )
		{
		setMessageData( &msgData, L, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S, 
							&msgData, CRYPT_CERTINFO_LOCALITYNAME );
		}
	if( cryptStatusOK( status ) )
		L[ msgData.length ] = '\0';
	if( cryptStatusOK( status ) || status == CRYPT_ERROR_NOTFOUND )
		{
		setMessageData( &msgData, O, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S, 
							&msgData, CRYPT_CERTINFO_ORGANIZATIONNAME );
		}
	if( cryptStatusOK( status ) )
		O[ msgData.length ] = '\0';
	if( cryptStatusOK( status ) || status == CRYPT_ERROR_NOTFOUND )
		{
		setMessageData( &msgData, OU, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S, 
							&msgData, CRYPT_CERTINFO_ORGANIZATIONALUNITNAME );
		}
	if( cryptStatusOK( status ) )
		OU[ msgData.length ] = '\0';
	if( cryptStatusOK( status ) || status == CRYPT_ERROR_NOTFOUND )
		{
		setMessageData( &msgData, CN, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S, 
							&msgData, CRYPT_CERTINFO_COMMONNAME );
		}
	if( cryptStatusOK( status ) )
		CN[ msgData.length ] = '\0';
	if( cryptStatusOK( status ) || status == CRYPT_ERROR_NOTFOUND )
		{
		/* Get the string form of the DN */
		status = encodeDN( dn, MAX_DN_STRINGSIZE, C, SP, L, O, OU, CN );
		}
	if( cryptStatusError( status ) )
		{
		/* Convert any low-level certificate-specific error into something 
		   generic which makes a bit more sense to the caller */
		return( CRYPT_ARGERROR_NUM1 );
		}

	/* Get the certificate data */
	setMessageData( &msgData, keyData, MAX_CERT_SIZE );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_CRT_EXPORT, &msgData, 
							  CRYPT_CERTFORMAT_CERTIFICATE );
	keyDataLength = msgData.length;
	if( cryptStatusError( status ) )
		{
		/* Convert any low-level certificate-specific error into something 
		   generic which makes a bit more sense to the caller */
		return( CRYPT_ARGERROR_NUM1 );
		}

	/* Set up the fixed attributes and certificate data.  This currently
	   always adds a certificate as a standard certificate rather than a CA 
	   certificate because of uncertainty over what other implementations
	   will try and look for, once enough other software uses the CA 
	   certificate attribute this can be switched over */
	if( ( ldapMod[ 0 ] = copyAttribute( ldapInfo->nameObjectClass,
										"certPerson", 0 ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	if( ( ldapMod[ ldapModIndex++ ] = copyAttribute( ldapInfo->nameCert,
										keyData, keyDataLength ) ) == NULL )
		status = CRYPT_ERROR_MEMORY;

	/* Set up the DN/identification information */
	if( cryptStatusOK( status ) && *email && \
		( ldapMod[ ldapModIndex++ ] = \
				copyAttribute( ldapInfo->nameEmail, email, 0 ) ) == NULL )
		status = CRYPT_ERROR_MEMORY;
	if( cryptStatusOK( status ) && *CN && \
		( ldapMod[ ldapModIndex++ ] = copyAttribute( "CN", CN, 0 ) ) == NULL )
		status = CRYPT_ERROR_MEMORY;
	if( cryptStatusOK( status ) && *OU && \
		( ldapMod[ ldapModIndex++ ] = copyAttribute( "OU", OU, 0 ) ) == NULL )
		status = CRYPT_ERROR_MEMORY;
	if( cryptStatusOK( status ) && *O && \
		( ldapMod[ ldapModIndex++ ] = copyAttribute( "O", O, 0 ) ) == NULL )
		status = CRYPT_ERROR_MEMORY;
	if( cryptStatusOK( status ) && *L && \
		( ldapMod[ ldapModIndex++ ] = copyAttribute( "L", L, 0 ) ) == NULL )
		status = CRYPT_ERROR_MEMORY;
	if( cryptStatusOK( status ) && *SP && \
		( ldapMod[ ldapModIndex++ ] = copyAttribute( "SP", SP, 0 ) ) == NULL )
		status = CRYPT_ERROR_MEMORY;
	if( cryptStatusOK( status ) && *C && \
		( ldapMod[ ldapModIndex++ ] = copyAttribute( "C", C, 0 ) ) == NULL )
		status = CRYPT_ERROR_MEMORY;
	ldapMod[ ldapModIndex ] = NULL;

	/* Add the new attribute/entry */
	if( cryptStatusOK( status ) )
		{
		int ldapStatus;

		if( ( ldapStatus = ldap_add_s( ldapInfo->ld, dn,
									   ldapMod ) ) != LDAP_SUCCESS )
			{
			getErrorInfo( keysetInfoPtr, ldapStatus );
			status = mapLdapError( ldapStatus, CRYPT_ERROR_WRITE );
			}
		}

	/* Clean up.  We do it the hard way rather than using
	   ldap_mods_free() here partially because the ldapMod[] array
	   isn't malloc()'d, but mostly because for the Netscape client
	   library ldap_mods_free() causes some sort of memory corruption,
	   possibly because it's trying to free the mod_values[] entries
	   which are statically allocated, and for the MS client the
	   function doesn't exist */
	for( ldapModIndex = 0; ldapModIndex < MAX_LDAP_ATTRIBUTES && \
						   ldapMod[ ldapModIndex ] != NULL;
		 ldapModIndex++ )
		{
		if( ldapMod[ ldapModIndex ]->mod_op & LDAP_MOD_BVALUES )
			clFree( "addCert", ldapMod[ ldapModIndex ]->mod_bvalues );
		else
			clFree( "addCert", ldapMod[ ldapModIndex ]->mod_values );
		clFree( "addCert", ldapMod[ ldapModIndex ] );
		}
	if( ldapModIndex >= MAX_LDAP_ATTRIBUTES )
		retIntError();
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							IN_HANDLE const CRYPT_HANDLE iCryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							STDC_UNUSED const char *password, 
							STDC_UNUSED const int passwordLength,
							IN_FLAGS( KEYMGMT ) const int flags )
	{
	BOOLEAN seenNonDuplicate = FALSE;
	int type, iterationCount = 0, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );
	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY );
	REQUIRES( password == NULL && passwordLength == 0 );
	REQUIRES( flags == KEYMGMT_FLAG_NONE );

	/* Make sure we've been given a certificate or certificate chain */
	status = krnlSendMessage( iCryptHandle, MESSAGE_GETATTRIBUTE, &type, 
							  CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_NUM1 );
	if( type != CRYPT_CERTTYPE_CERTIFICATE && \
		type != CRYPT_CERTTYPE_CERTCHAIN )
		return( CRYPT_ARGERROR_NUM1 );

	/* Lock the certificate for our exclusive use (in case it's a 
	   certificate chain, we also select the first certificate in the 
	   chain), update the keyset with the certificate(s), and unlock it to 
	   allow others access */
	krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
					 MESSAGE_VALUE_CURSORFIRST,
					 CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		return( status );
	do
		{
		/* Add the certificate */
		status = addCert( keysetInfoPtr, iCryptHandle );

		/* A certificate being added may already be present, however we 
		   can't fail immediately because what's being added may be a chain 
		   containing further certificates so we keep track of whether we've 
		   successfully added at least one certificate and clear data 
		   duplicate errors */
		if( status == CRYPT_OK )
			seenNonDuplicate = TRUE;
		else
			{
			if( status == CRYPT_ERROR_DUPLICATE )
				status = CRYPT_OK;
			}
		}
	while( cryptStatusOK( status ) && \
		   krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
							MESSAGE_VALUE_CURSORNEXT,
							CRYPT_CERTINFO_CURRENT_CERTIFICATE ) == CRYPT_OK && \
		   iterationCount++ < FAILSAFE_ITERATIONS_MED );
	if( iterationCount >= FAILSAFE_ITERATIONS_MED )
		retIntError();
	krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE, 
					 MESSAGE_VALUE_FALSE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusOK( status ) && !seenNonDuplicate )
		{
		/* We reached the end of the chain without finding anything we could
		   add, return a data duplicate error */
		status = CRYPT_ERROR_DUPLICATE;
		}

	return( status );
	}

/* Delete an entry from an LDAP directory */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int deleteItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							   IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							   IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							   IN_BUFFER( keyIDlength ) const void *keyID, 
							   IN_LENGTH_KEYID const int keyIDlength )
	{
	LDAP_INFO *ldapInfo = keysetInfoPtr->keysetLDAP;
	char dn[ MAX_DN_STRINGSIZE + 8 ];
	int ldapStatus;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || keyIDtype == CRYPT_KEYID_URI );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	/* Convert the DN into a null-terminated form */
	if( keyIDlength > MAX_DN_STRINGSIZE - 1 )
		return( CRYPT_ARGERROR_STR1 );
	memcpy( dn, keyID, keyIDlength );
	dn[ keyIDlength ] = '\0';

	/* Delete the entry */
	if( ( ldapStatus = ldap_delete_s( ldapInfo->ld, dn ) ) != LDAP_SUCCESS )
		{
		getErrorInfo( keysetInfoPtr, ldapStatus );
		return( mapLdapError( ldapStatus, CRYPT_ERROR_WRITE ) );
		}

	return( CRYPT_OK );
	}

#if 0	/* 5/7/10 getItemFunction() hasn't supported this functionality 
				  since 3.3.2, and it was removed there because the kernel 
				  had never allowed this type of access at all, which means
				  that these functions could never have been called.  Since
				  no-one had ever noticed this, there's no point in 
				  continuing to support them */

/* Perform a getFirst/getNext query on the LDAP directory */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 6 ) ) \
static int getFirstItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
								 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
								 STDC_UNUSED int *stateInfo,
								 IN_ENUM( KEYMGMT_ITEM ) \
									const KEYMGMT_ITEM_TYPE itemType,
								 IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
								 IN_BUFFER( keyIDlength ) const void *keyID, 
								 IN_LENGTH_KEYID const int keyIDlength,
								 IN_FLAGS( KEYMGMT ) const int options )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );
	REQUIRES( stateInfo == NULL );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY );
	REQUIRES( keyIDtype != CRYPT_KEYID_NONE );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( options == KEYMGMT_FLAG_NONE );

	return( getItemFunction( keysetInfoPtr, iCertificate, 
							 KEYMGMT_ITEM_PUBLICKEY, CRYPT_KEYID_NAME, 
							 keyID, keyIDlength, NULL, 0, 0 ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int getNextItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
								OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
								STDC_UNUSED int *stateInfo, 
								IN_FLAGS( KEYMGMT ) const int options )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );
	REQUIRES( stateInfo == NULL );
	REQUIRES( options == KEYMGMT_FLAG_NONE );

	return( getItemFunction( keysetInfoPtr, iCertificate, 
							 KEYMGMT_ITEM_PUBLICKEY, CRYPT_KEYID_NONE, 
							 NULL, 0, NULL, 0, 0 ) );
	}
#endif /* 0 */

/* Return status information for the keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isBusyFunction( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );

	return( FALSE );
	}

/* Get/set keyset attributes */

static void *getAttributeDataPtr( KEYSET_INFO *keysetInfoPtr, 
								  const CRYPT_ATTRIBUTE_TYPE type )
	{
	LDAP_INFO *ldapInfo = keysetInfoPtr->keysetLDAP;

	switch( type )
		{
		case CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS:
			return( ldapInfo->nameObjectClass );

		case CRYPT_OPTION_KEYS_LDAP_FILTER:
			return( ldapInfo->nameFilter );

		case CRYPT_OPTION_KEYS_LDAP_CACERTNAME:
			return( ldapInfo->nameCACert );

		case CRYPT_OPTION_KEYS_LDAP_CERTNAME:
			return( ldapInfo->nameCert );

		case CRYPT_OPTION_KEYS_LDAP_CRLNAME:
			return( ldapInfo->nameCRL );

		case CRYPT_OPTION_KEYS_LDAP_EMAILNAME:
			return( ldapInfo->nameEmail );
		}

	return( NULL );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getAttributeFunction( INOUT KEYSET_INFO *keysetInfoPtr, 
								 INOUT void *data,
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	const void *attributeDataPtr;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );

	attributeDataPtr = getAttributeDataPtr( keysetInfoPtr, type );
	if( attributeDataPtr == NULL )
		return( CRYPT_ARGERROR_VALUE );
	return( attributeCopy( data, attributeDataPtr, 
						   strlen( attributeDataPtr ) ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int setAttributeFunction( INOUT KEYSET_INFO *keysetInfoPtr, 
								 const void *data,
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) data;
	BYTE *attributeDataPtr;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );

	attributeDataPtr = getAttributeDataPtr( keysetInfoPtr, type );
	if( attributeDataPtr == NULL )
		return( CRYPT_ARGERROR_VALUE );
	if( msgData->length < 1 || msgData->length > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ARGERROR_STR1 );
	memcpy( attributeDataPtr, msgData->data, msgData->length );
	attributeDataPtr[ msgData->length ] = '\0';

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodLDAP( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );

#ifdef DYNAMIC_LOAD
	/* Make sure that the LDAP driver is bound in */
	if( hLDAP == NULL_INSTANCE )
		return( CRYPT_ERROR_OPEN );
#endif /* DYNAMIC_LOAD */

	/* Set the access method pointers */
	keysetInfoPtr->initFunction = initFunction;
	keysetInfoPtr->shutdownFunction = shutdownFunction;
	keysetInfoPtr->getAttributeFunction = getAttributeFunction;
	keysetInfoPtr->setAttributeFunction = setAttributeFunction;
	keysetInfoPtr->getItemFunction = getItemFunction;
	keysetInfoPtr->setItemFunction = setItemFunction;
	keysetInfoPtr->deleteItemFunction = deleteItemFunction;
	keysetInfoPtr->isBusyFunction = isBusyFunction;

	return( CRYPT_OK );
	}
#endif /* USE_LDAP */
