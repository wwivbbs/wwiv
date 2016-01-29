/****************************************************************************
*																			*
*						 cryptlib ODBC Mapping Routines						*
*						Copyright Peter Gutmann 1996-2012					*
*																			*
****************************************************************************/

#include <stdio.h>		/* For sprintf() */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "dbms.h"
  #include "keyset.h"
#else
  #include "crypt.h"
  #include "keyset/dbms.h"
  #include "keyset/keyset.h"
#endif /* Compiler-specific includes */

/* ODBC functions can return either SQL_SUCCESS or SQL_SUCCESS_WITH_INFO to
   indicate successful completion, to make them easier to work with we use
   a general status-check macro along the lines of cryptStatusOK() */

#define sqlStatusOK( status ) \
		( ( status ) == SQL_SUCCESS || ( status ) == SQL_SUCCESS_WITH_INFO )

/* DBMS back-ends that require special handling */

enum { DBMS_NONE, DBMS_ACCESS, DBMS_INTERBASE, DBMS_MYSQL, DBMS_POSTGRES };

/* The level at which we want SQLDiagRec() to return error information to 
   us */

typedef enum { SQL_ERRLVL_NONE, SQL_ERRLVL_STMT, SQL_ERRLVL_DBC, 
			   SQL_ERRLVL_ENV, SQL_ERRLVL_LAST } SQL_ERRLVL_TYPE;

/* When rewriting an SQL query we have to provide a slightly larger buffer 
   to allow for possible expansion of some SQL strings */

#define SQL_QUERY_BUFSIZE	( MAX_SQL_QUERY_SIZE + 64 )

/* Some ODBC functions take either pointers or small integer values cast to 
   pointers to indicate certain magic values.  This causes problems in 
   64-bit environments because the LLP64 model means that pointers are 64 
   bits while ints and longs are 32 bits.  To deal with this we define the 
   following data-conversion macros for 32- and 64-bit environments */

#ifdef __WIN64__
  #define VALUE_TO_PTR	ULongToPtr
#else
  #define VALUE_TO_PTR	( SQLPOINTER )
#endif /* 32- vs. 64-bit environment */

#ifdef USE_ODBC

/* When processing bound data we need to store the state information used by 
   SQLBindParameter() until the SQL operation completes.  The following 
   structure provides the necessary state storage */

typedef struct {
	SQLINTEGER lengthStorage[ BOUND_DATA_MAXITEMS + 8 ];
	SQL_TIMESTAMP_STRUCT timestampStorage;
	} BOUND_DATA_STATE;

/****************************************************************************
*																			*
*						 		Init/Shutdown Routines						*
*																			*
****************************************************************************/

#ifdef DYNAMIC_LOAD

/* Global function pointers.  These are necessary because the functions need
   to be dynamically linked since not all systems contain the necessary
   shared libraries.  Explicitly linking to them will make cryptlib 
   unloadable on some systems.

   MSDN updates from late 2000 defined SQLROWCOUNT themselves (which could be
   fixed by undefining it), however after late 2002 the value was typedef'd,
   requring all sorts of extra trickery to handle the different cases.
   Because of this issue, this particular function is typedef'd with a _FN 
   suffix.
   
   Several of the ODBC functions allow such a mess of parameters, with 
   options for pointers to be cast to integers indexing a table of data 
   values hidden under someone's bed and other peculiarities, that several 
   of the following markups are only approximations for the way the 
   functions are used here.  If markups are absent entirely (e.g. for the
   SQLSetXXXAttr() functions) it's because the polymorphism of parameters 
   doesn't allow any coherent annotation to be given */

static INSTANCE_HANDLE hODBC = NULL_INSTANCE;

typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLALLOCHANDLE ) \
					( SQLSMALLINT HandleType, IN_OPT SQLHANDLE InputHandle, 
					OUT_PTR SQLHANDLE *OutputHandlePtr );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLBINDPARAMETER ) \
					( IN SQLHSTMT StatementHandle, 
					SQLUSMALLINT ParameterNumber, SQLSMALLINT InputOutputType,
					SQLSMALLINT ValueType, SQLSMALLINT ParameterType,
					SQLUINTEGER ColumnSize, SQLSMALLINT DecimalDigits,
					IN_BUFFER_OPT( BufferLength ) SQLPOINTER ParameterValuePtr, 
					SQLINTEGER BufferLength, 
					INOUT_OPT SQLINTEGER *StrLen_or_IndPtr );
typedef SQLRETURN ( SQL_API *SQLCLOSECURSOR )( IN SQLHSTMT StatementHandle );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLCONNECT ) \
					( IN SQLHDBC ConnectionHandle,
					IN_BUFFER( NameLength1 ) SQLCHAR *ServerName, 
					SQLSMALLINT NameLength1,
					IN_BUFFER( NameLength2 ) SQLCHAR *UserName, 
					SQLSMALLINT NameLength2,
					IN_BUFFER( NameLength3 ) SQLCHAR *Authentication, 
					SQLSMALLINT NameLength3 );
typedef SQLRETURN ( SQL_API *SQLDISCONNECT )( IN SQLHDBC ConnectionHandle );
typedef SQLRETURN ( SQL_API *SQLENDTRAN )( SQLSMALLINT HandleType,
					IN SQLHANDLE Handle, SQLSMALLINT CompletionType );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLEXECDIRECT ) \
					( SQLHSTMT StatementHandle,
					IN_BUFFER( TextLength ) SQLCHAR *StatementText, 
					SQLINTEGER TextLength );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLEXECUTE ) \
					( IN SQLHSTMT StatementHandle );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLFETCH ) \
					( IN SQLHSTMT StatementHandle );
typedef SQLRETURN ( SQL_API *SQLFREEHANDLE )( SQLSMALLINT HandleType,
					IN SQLHANDLE Handle );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLGETDATA ) \
					( SQLHSTMT StatementHandle, SQLUSMALLINT ColumnNumber, 
					SQLSMALLINT TargetType, 
					OUT_BUFFER( BufferLength, *StrLen_or_IndPtr ) \
						SQLPOINTER TargetValuePtr, 
					SQLINTEGER BufferLength, SQLINTEGER *StrLen_or_IndPtr );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLGETDIAGREC ) \
					( SQLSMALLINT HandleType,
					IN SQLHANDLE Handle, SQLSMALLINT RecNumber,
					OUT_BUFFER_FIXED( SQL_SQLSTATE_SIZE ) SQLCHAR *Sqlstate, 
					OUT SQLINTEGER *NativeErrorPtr,
					OUT_BUFFER( BufferLength, *TextLengthPtr ) \
						SQLCHAR *MessageText, 
					SQLSMALLINT BufferLength, SQLSMALLINT *TextLengthPtr );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLGETINFO ) \
					( IN SQLHDBC ConnectionHandle,
					SQLUSMALLINT InfoType, 
					OUT_BUFFER( BufferLength, *StringLengthPtr ) \
						SQLPOINTER InfoValuePtr, 
					SQLSMALLINT BufferLength, SQLSMALLINT *StringLengthPtr );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLGETSTMTATTR ) \
					( IN SQLHSTMT StatementHandle,
					SQLINTEGER Attribute, OUT SQLPOINTER ValuePtr,
					SQLINTEGER BufferLength, 
					STDC_UNUSED SQLINTEGER *StringLengthPtr );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLGETTYPEINFO ) \
					( IN SQLHSTMT StatementHandle,
					SQLSMALLINT DataType );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLPREPARE ) \
					( IN SQLHSTMT StatementHandle,
					IN_BUFFER( TextLength ) SQLCHAR *StatementText, 
					SQLINTEGER TextLength );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLROWCOUNT_FN ) \
					( IN SQLHSTMT StatementHandle,
					OUT SQLINTEGER *RowCountPtr );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLSETCONNECTATTR ) \
					( IN SQLHDBC ConnectionHandle,
					SQLINTEGER Attribute, SQLPOINTER ValuePtr,
					SQLINTEGER StringLength );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLSETENVATTR ) \
					( IN SQLHENV EnvironmentHandle,
					SQLINTEGER Attribute, SQLPOINTER ValuePtr,
					SQLINTEGER StringLength );
typedef CHECK_RETVAL SQLRETURN ( SQL_API *SQLSETSTMTATTR ) \
					( IN SQLHSTMT StatementHandle,
					SQLINTEGER Attribute, SQLPOINTER ValuePtr,
					SQLINTEGER StringLength );

static SQLALLOCHANDLE pSQLAllocHandle = NULL;
static SQLBINDPARAMETER pSQLBindParameter = NULL;
static SQLCLOSECURSOR pSQLCloseCursor = NULL;
static SQLCONNECT pSQLConnect = NULL;
static SQLDISCONNECT pSQLDisconnect = NULL;
static SQLENDTRAN pSQLEndTran = NULL;
static SQLEXECDIRECT pSQLExecDirect = NULL;
static SQLEXECUTE pSQLExecute = NULL;
static SQLFETCH pSQLFetch = NULL;
static SQLFREEHANDLE pSQLFreeHandle = NULL;
static SQLGETDATA pSQLGetData = NULL;
static SQLGETDIAGREC pSQLGetDiagRec = NULL;
static SQLGETINFO pSQLGetInfo = NULL;
static SQLGETSTMTATTR pSQLGetStmtAttr = NULL;
static SQLGETTYPEINFO pSQLGetTypeInfo = NULL;
static SQLPREPARE pSQLPrepare = NULL;
static SQLROWCOUNT_FN pSQLRowCount = NULL;
static SQLSETCONNECTATTR pSQLSetConnectAttr = NULL;
static SQLSETENVATTR pSQLSetEnvAttr = NULL;
static SQLSETSTMTATTR pSQLSetStmtAttr = NULL;

/* The use of dynamically bound function pointers vs. statically linked
   functions requires a bit of sleight of hand since we can't give the
   pointers the same names as prototyped functions.  To get around this we
   redefine the actual function names to the names of the pointers */

#define SQLAllocHandle			pSQLAllocHandle
#define SQLBindParameter		pSQLBindParameter
#define SQLCloseCursor			pSQLCloseCursor
#define SQLConnect				pSQLConnect
#define SQLDisconnect			pSQLDisconnect
#define SQLEndTran				pSQLEndTran
#define SQLExecDirect			pSQLExecDirect
#define SQLExecute				pSQLExecute
#define SQLFetch				pSQLFetch
#define SQLFreeHandle			pSQLFreeHandle
#define SQLGetData				pSQLGetData
#define SQLGetDiagRec			pSQLGetDiagRec
#define SQLGetInfo				pSQLGetInfo
#define SQLGetStmtAttr			pSQLGetStmtAttr
#define SQLGetTypeInfo			pSQLGetTypeInfo
#define SQLPrepare				pSQLPrepare
#define SQLRowCount				pSQLRowCount
#define SQLSetConnectAttr		pSQLSetConnectAttr
#define SQLSetEnvAttr			pSQLSetEnvAttr
#define SQLSetStmtAttr			pSQLSetStmtAttr

/* Depending on whether we're running under Win16, Win32, or Unix we load the
   ODBC driver under a different name */

#if defined( __WIN16__ )
  #define ODBC_LIBNAME			"ODBC.DLL"
#elif defined( __WIN32__ )
  #define ODBC_LIBNAME			"ODBC32.DLL"
#elif defined( __UNIX__ )
  #if defined( __APPLE__ )
	/* OS X has built-in ODBC support via iODBC */
	#define ODBC_LIBNAME		"libiodbc.dylib"
  #else
	/* Currently we default to UnixODBC, which uses libodbc.so.  If this
	   fails, we fall back to the next-most-common one, iODBC.  If you're
	   using something else, you'll need to change the entry below to
	   specify your library name */
	#define ODBC_LIBNAME		"libodbc.so"
	#define ODBC_ALT_LIBNAME	"libiodbc.so"
  #endif /* Mac OS X vs. other Unixen */
#endif /* System-specific ODBC library names */

/* Dynamically load and unload any necessary DBMS libraries */

CHECK_RETVAL \
int dbxInitODBC( void )
	{
#ifdef __WIN16__
	UINT errorMode;
#endif /* __WIN16__ */

	/* If the ODBC module is already linked in, don't do anything */
	if( hODBC != NULL_INSTANCE )
		return( CRYPT_OK );

	/* Obtain a handle to the module containing the ODBC functions */
#if defined( __WIN16__ )
	errorMode = SetErrorMode( SEM_NOOPENFILEERRORBOX );
	hODBC = LoadLibrary( ODBC_LIBNAME );
	SetErrorMode( errorMode );
	if( hODBC < HINSTANCE_ERROR )
		{
		hODBC = NULL_INSTANCE;
		return( CRYPT_ERROR_NOTAVAIL );
		}
#elif defined( __UNIX__ ) && !defined( __APPLE__ )
	if( ( hODBC = DynamicLoad( ODBC_LIBNAME ) ) == NULL_INSTANCE && \
		( hODBC = DynamicLoad( ODBC_ALT_LIBNAME ) ) == NULL_INSTANCE )
		return( CRYPT_ERROR_NOTAVAIL );
#else
	if( ( hODBC = DynamicLoad( ODBC_LIBNAME ) ) == NULL_INSTANCE )
		return( CRYPT_ERROR_NOTAVAIL );
#endif /* __WIN32__ */

	/* Now get pointers to the functions */
	pSQLAllocHandle = ( SQLALLOCHANDLE ) DynamicBind( hODBC, "SQLAllocHandle" );
	pSQLBindParameter = ( SQLBINDPARAMETER ) DynamicBind( hODBC, "SQLBindParameter" );
	pSQLCloseCursor = ( SQLCLOSECURSOR ) DynamicBind( hODBC, "SQLCloseCursor" );
	pSQLConnect = ( SQLCONNECT ) DynamicBind( hODBC, "SQLConnect" );
	pSQLDisconnect = ( SQLDISCONNECT ) DynamicBind( hODBC, "SQLDisconnect" );
	pSQLEndTran = ( SQLENDTRAN ) DynamicBind( hODBC, "SQLEndTran" );
	pSQLExecDirect = ( SQLEXECDIRECT ) DynamicBind( hODBC, "SQLExecDirect" );
	pSQLExecute = ( SQLEXECUTE ) DynamicBind( hODBC, "SQLExecute" );
	pSQLFetch = ( SQLFETCH ) DynamicBind( hODBC, "SQLFetch" );
	pSQLFreeHandle = ( SQLFREEHANDLE ) DynamicBind( hODBC, "SQLFreeHandle" );
	pSQLGetData = ( SQLGETDATA ) DynamicBind( hODBC, "SQLGetData" );
	pSQLGetDiagRec = ( SQLGETDIAGREC ) DynamicBind( hODBC, "SQLGetDiagRec" );
	pSQLGetInfo = ( SQLGETINFO ) DynamicBind( hODBC, "SQLGetInfo" );
	pSQLGetStmtAttr = ( SQLGETSTMTATTR ) DynamicBind( hODBC, "SQLGetStmtAttr" );
	pSQLGetTypeInfo = ( SQLGETTYPEINFO ) DynamicBind( hODBC, "SQLGetTypeInfo" );
	pSQLPrepare = ( SQLPREPARE ) DynamicBind( hODBC, "SQLPrepare" );
	pSQLRowCount = ( SQLROWCOUNT_FN ) DynamicBind( hODBC, "SQLRowCount" );
	pSQLSetConnectAttr = ( SQLSETCONNECTATTR ) DynamicBind( hODBC, "SQLSetConnectAttr" );
	pSQLSetEnvAttr = ( SQLSETENVATTR ) DynamicBind( hODBC, "SQLSetEnvAttr" );
	pSQLSetStmtAttr = ( SQLSETSTMTATTR ) DynamicBind( hODBC, "SQLSetStmtAttr" );

	/* Make sure that we got valid pointers for every ODBC function */
	if( pSQLAllocHandle == NULL || pSQLBindParameter == NULL ||
		pSQLCloseCursor == NULL || pSQLConnect == NULL ||
		pSQLDisconnect == NULL || pSQLEndTran == NULL ||
		pSQLExecDirect == NULL || pSQLExecute == NULL ||
		pSQLFetch == NULL || pSQLFreeHandle == NULL ||
		pSQLGetData == NULL || pSQLGetDiagRec == NULL ||
		pSQLGetInfo == NULL || pSQLGetStmtAttr == NULL ||
		pSQLGetTypeInfo == NULL || pSQLPrepare == NULL || 
		pSQLSetConnectAttr == NULL || pSQLSetEnvAttr == NULL || 
		pSQLSetStmtAttr == NULL )
		{
		/* Free the library reference and reset the handle */
		DynamicUnload( hODBC );
		hODBC = NULL_INSTANCE;
		return( CRYPT_ERROR_NOTAVAIL );
		}

	return( CRYPT_OK );
	}

void dbxEndODBC( void )
	{
	if( hODBC != NULL_INSTANCE )
		DynamicUnload( hODBC );
	hODBC = NULL_INSTANCE;
	}
#else

CHECK_RETVAL \
int dbxInitODBC( void )
	{
	return( CRYPT_OK );
	}

void dbxEndODBC( void )
	{
	}
#endif /* DYNAMIC_LOAD */

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Get information on an ODBC error.  The statement handle is specified as a
   distinct parameter because it may be an ephemeral handle not part of the
   state information data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getErrorInfo( INOUT DBMS_STATE_INFO *dbmsInfo, 
						 IN_ENUM( SQL_ERRLVL ) const SQL_ERRLVL_TYPE errorLevel, 
						 const SQLHSTMT hStmt, 
						 IN_ERROR const int defaultStatus )
	{
#ifdef USE_ERRMSGS
	ERROR_INFO *errorInfo = &dbmsInfo->errorInfo;
#endif /* USE_ERRMSGS */
	SQLCHAR szSqlState[ SQL_SQLSTATE_SIZE + 8 ];
	SQLCHAR errorString[ MAX_ERRMSG_SIZE + 1 + 8 ];
	SQLHANDLE handle;
	SQLINTEGER dwNativeError = 0;
	SQLSMALLINT handleType, errorStringLength;
	SQLRETURN sqlStatus;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );
	
	REQUIRES( errorLevel == SQL_ERRLVL_STMT || \
			  errorLevel == SQL_ERRLVL_DBC || \
			  errorLevel == SQL_ERRLVL_ENV );
	REQUIRES( ( errorLevel == SQL_ERRLVL_STMT && \
				hStmt != SQL_NULL_HSTMT ) || \
			  ( errorLevel != SQL_ERRLVL_STMT && \
				hStmt == SQL_NULL_HSTMT ) );
	REQUIRES( cryptStatusError( defaultStatus ) );

	/* Set up the handle information for the diagnostic information that we 
	   want to retrieve */
	switch( errorLevel )
		{
		case SQL_ERRLVL_STMT:
			handleType = SQL_HANDLE_STMT;
			handle = hStmt;
			break;

		case SQL_ERRLVL_DBC:
			handleType = SQL_HANDLE_DBC;
			handle = dbmsInfo->hDbc;
			break;

		case SQL_ERRLVL_ENV:
			handleType = SQL_HANDLE_ENV;
			handle = dbmsInfo->hEnv;
			break;

		default:
			retIntError();
		}

	/* Get the ODBC error information at the most detailed level that we can 
	   manage */
	sqlStatus = SQLGetDiagRec( handleType, handle, 1, szSqlState,
							   &dwNativeError, errorString, MAX_ERRMSG_SIZE, 
							   &errorStringLength );
	if( !sqlStatusOK( sqlStatus ) && errorLevel == SQL_ERRLVL_STMT )
		{
		/* If we couldn't get information at the statement-handle level, try 
		   again at the connection handle level */
		sqlStatus = SQLGetDiagRec( SQL_HANDLE_DBC, dbmsInfo->hDbc, 1,
								   szSqlState, &dwNativeError,
								   errorString, MAX_ERRMSG_SIZE, 
								   &errorStringLength );
		}
	if( !sqlStatusOK( sqlStatus ) )
		{
		DEBUG_DIAG(( "Couldn't read error information from database "
					 "backend" ));
		assert( DEBUG_WARN );	/* Catch this if it ever occurs */
		setErrorString( errorInfo, 
						"Couldn't get error information from database "
						"backend", 52 );
		return( CRYPT_ERROR_READ );
		}

	/* In some (rare) cases SQLGetDiagRec() can return an empty error string
	   with only szSqlState set, in which case we clear the error string */
	if( errorStringLength > 0 )
		{
		/* Since the error string has come from the database source we 
		   sanitise it before returning it to the caller.  The +1 is for
		   the '\0' that sanitiseString() adds to the string */
		sanitiseString( errorString, errorStringLength + 1, 
						errorStringLength );
		setErrorString( errorInfo, errorString, errorStringLength );
		}
	else
		clearErrorString( errorInfo );

	/* Check for a not-found error status.  We can also get an sqlStatus of
	   SQL_NO_DATA with SQLSTATE set to "00000" and the error message string
	   empty in some cases, in which case we provide our own error string */
	if( !memcmp( szSqlState, "S0002", 5 ) ||	/* ODBC 2.x */
		!memcmp( szSqlState, "42S02", 5 ) ||	/* ODBC 3.x */
		( !memcmp( szSqlState, "00000", 5 ) && \
		  sqlStatus == SQL_NO_DATA ) )
		{
		/* Make sure that the caller gets a sensible error message if they
		   try to examine the extended error information */
		if( errorStringLength <= 0 )
			setErrorString( errorInfo, "No data found", 13 );

		return( CRYPT_ERROR_NOTFOUND );
		}

	/* When we're trying to create a new keyset, there may already be one
	   present giving an S0001/42S01 (table already exists) or S0011/42S11 
	   (index already exists) error .  We could check for the table by doing 
	   a dummy read, but it's easier to just try the update anyway and 
	   convert the error code to the correct value here if there's a 
	   problem */
	if( !memcmp( szSqlState, "S0001", 5 ) ||
		!memcmp( szSqlState, "S0011", 5 ) ||	/* ODBC 2.x */
		!memcmp( szSqlState, "42S01", 5 ) ||
		!memcmp( szSqlState, "42S11", 5 ) )		/* ODBX 3.x */
		return( CRYPT_ERROR_DUPLICATE );

	/* This one is a bit odd: An integrity constraint violation occurred,
	   which means (among other things) that an attempt was made to write a
	   duplicate value to a column constrained to contain unique values.  It
	   can also include things like writing a NULL value to a column
	   constrained to be NOT NULL, but this wouldn't normally happen so we
	   can convert this one to a duplicate data error */
	if( !memcmp( szSqlState, "23000", 5 ) )
		return( CRYPT_ERROR_DUPLICATE );

	return( defaultStatus );
	}

/* Dump debugging information on an ODBC error to the error log.  This is 
   used to record oddball errors during debugging that don't necessarily
   affect overall operation but that should be fixed anyway */

#if !defined( NDEBUG ) && defined( DEBUG_DIAGNOSTIC_ENABLE )

#define DUMP_ODBCERROR	debugDiagOdbcError

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void debugDiagOdbcError( IN_STRING const char *functionName,
						 INOUT DBMS_STATE_INFO *dbmsInfo, 
						 IN_ENUM( SQL_ERRLVL ) const SQL_ERRLVL_TYPE errorLevel, 
						 const SQLHSTMT hStmt )
	{
	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );
	assert( isReadPtr( functionName, 8 ) );
	assert( errorLevel == SQL_ERRLVL_STMT || \
			errorLevel == SQL_ERRLVL_DBC || \
			errorLevel == SQL_ERRLVL_ENV );
	assert( ( errorLevel == SQL_ERRLVL_STMT && \
			hStmt != SQL_NULL_HSTMT ) || \
		  ( errorLevel != SQL_ERRLVL_STMT && \
			hStmt == SQL_NULL_HSTMT ) );

	( void ) getErrorInfo( dbmsInfo, errorLevel, hStmt, 
						   CRYPT_ERROR_INTERNAL );
	DEBUG_DIAG(( "%s reports: %s.\n", functionName,
				 getErrorInfoString( &dbmsInfo->errorInfo ) ));
	}
#else
  #define DUMP_ODBCERROR( dbmsInfo, functionName, errorLevel, hStmt );
#endif /* !NDEBUG */

/* Rewrite the SQL query to handle the back-end specific blob and date type, 
   and to work around problems with some back-end types (and we're 
   specifically talking Access here) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 6 ) ) \
static int rewriteString( INOUT_BUFFER( stringMaxLength, \
										*stringLength ) char *string, 
						  IN_LENGTH_SHORT const int stringMaxLength, 
						  OUT_LENGTH_BOUNDED_Z( stringMaxLength ) \
								int *stringLength, 
						  IN_LENGTH_SHORT const int subStringLength, 
						  IN_LENGTH_SHORT const int origStringLength, 
						  IN_BUFFER( newSubStringLength ) \
								const char *newSubString, 
						  IN_LENGTH_SHORT const int newSubStringLength )
	{
	const int remainder = origStringLength - subStringLength;
	const int newStringLength = newSubStringLength + remainder;

	assert( isWritePtr( string, stringMaxLength ) );
	assert( isReadPtr( newSubString, newSubStringLength ) );

	REQUIRES( stringMaxLength > 0 && stringMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( subStringLength > 0 && \
			  subStringLength < origStringLength && \
			  subStringLength < MAX_INTLENGTH_SHORT );
	REQUIRES( origStringLength > 0 && \
			  origStringLength <= stringMaxLength && \
			  origStringLength < MAX_INTLENGTH_SHORT );
	REQUIRES( newSubStringLength > 0 && \
			  newSubStringLength < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*stringLength = 0;

	/* Make sure that the parameters are in order and there's room to 
	   rewrite the string */
	ENSURES( remainder > 0 && newStringLength > 0 && \
			 newStringLength < stringMaxLength );

	/* Open/close up a gap and replace the existing substring with the new 
	   one:

									origStringLength
		|<----------- string -------------->|
		+---------------+-------------------+-----------+
		|///////////////|...................|			|
		+---------------+-------------------+-----------+
		|<- subString ->|								|
												stringMaxLength */
	REQUIRES( rangeCheck( newSubStringLength, remainder, stringMaxLength ) );
	memmove( string + newSubStringLength, string + subStringLength, 
			 remainder );
	memcpy( string, newSubString, newSubStringLength );
	*stringLength = newSubStringLength - subStringLength;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
static int convertQuery( INOUT DBMS_STATE_INFO *dbmsInfo, 
						 OUT_BUFFER( queryMaxLen, *queryLength ) char *query,
						 IN_LENGTH_SHORT_MIN( 16 ) const int queryMaxLen, 
						 OUT_LENGTH_BOUNDED_Z( queryMaxLen ) \
								int *queryLength,
						 IN_BUFFER( commandLength ) const char *command,
						 IN_LENGTH_SHORT const int commandLength )
	{
	int currentLength = commandLength;
	int offset, length, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );
	assert( isWritePtr( query, queryMaxLen ) );
	assert( isReadPtr( command, commandLength ) );
	
	REQUIRES( queryMaxLen >= 16 && queryMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( commandLength > 0 && commandLength < queryMaxLen && \
			  commandLength < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*queryLength = 0;

	/* Copy the SQL command across to the query buffer */
	memcpy( query, command, commandLength );

	/* If it's a CREATE TABLE command rewrite the blob and date types to the 
	   appropriate values for the database back-end */
	if( !strCompare( command, "CREATE TABLE", 12 ) )
		{
		offset = strFindStr( query, currentLength, " BLOB", 5 );
		if( offset > 0 )
			{
			offset++;	/* Skip space before blob name */

			status = rewriteString( query + offset, queryMaxLen - offset, 
									&length, 4, currentLength - offset, 
									dbmsInfo->blobName, 
									dbmsInfo->blobNameLength );
			if( cryptStatusError( status ) )
				return( status );
			currentLength += length;
			}
		offset = strFindStr( query, currentLength, " DATETIME", 9 );
		if( offset > 0 && \
			!( dbmsInfo->dateTimeNameLength == 8 && \
			   !strCompare( dbmsInfo->dateTimeName, "DATETIME", 8 ) ) )
			{
			offset++;	/* Skip space before date/time name */
			status = rewriteString( query + offset, queryMaxLen - offset, 
									&length, 8, currentLength - offset, 
									dbmsInfo->dateTimeName, 
									dbmsInfo->dateTimeNameLength );
			if( cryptStatusError( status ) )
				return( status );
			currentLength += length;
			}
		}

	/* If it's not one of the back-ends that require special-case handling,  
	   we're done */
	switch( dbmsInfo->backendType )
		{
		case DBMS_ACCESS:
			/* If it's not a SELECT/DELETE with wildcards used, there's 
			   nothing to do */
			if( ( strCompare( query, "SELECT", 6 ) && \
				  strCompare( query, "DELETE", 6 ) ) || \
				  strFindStr( query, currentLength, " LIKE ", 6 ) <= 7 )
				{
				*queryLength = currentLength;
				return( CRYPT_OK );
				}
			break;

		case DBMS_INTERBASE:
			/* If it's not a CREATE TABLE/INSERT/DELETE/SELECT with the 
			   'type' column involved, there's nothing to do */
			if( strCompare( query, "CREATE TABLE", 12 ) && \
				strCompare( query, "SELECT", 6 ) && \
				strCompare( query, "DELETE", 6 ) && \
				strCompare( query, "INSERT", 6 ) )
				{
				*queryLength = currentLength;
				return( CRYPT_OK );
				}
			if( strFindStr( query, currentLength, " type ", 6 ) <= 7 )
				{
				*queryLength = currentLength;
				return( CRYPT_OK );
				}
			break;

		default:
			/* Currently no other back-ends need special handling */
			*queryLength = currentLength;
			return( CRYPT_OK );
		}

	/* Unlike everything else in the known universe Access uses * and ?
	   instead of the standard SQL wildcards so if we find a LIKE ... %
	   we rewrite the % as a * */
	if( ( dbmsInfo->backendType == DBMS_ACCESS ) && \
		( offset = strFindStr( query, currentLength, " LIKE ", 6 ) ) > 0 )
		{
		int i;

		/* Search up to 6 characters ahead for a wildcard and replace it
		   with the one needed by Access if we find it.  We search 6 chars
		   ahead because the higher-level SQL code uses expressions like
		   "SELECT .... WHERE foo LIKE '--%'", which is 5 chars plus one as
		   a safety margin */
		for( i = offset + 7; i < offset + 11 && i < currentLength; i++ )
			{
			if( query[ i ] == '%' )
				query[ i ] = '*';
			}
		}

	/* Interbase treats TYPE as a reserved word so we can't use 'type' for a
	   column name */
	if( ( dbmsInfo->backendType == DBMS_INTERBASE ) && \
		( offset = strFindStr( query, currentLength, " type ", 6 ) ) > 0 )
		{
		offset++;	/* Skip space before type name */
		status = rewriteString( query + offset, queryMaxLen - offset, 
								&length, 4, currentLength - offset, 
								"ctype", 5 );
		if( cryptStatusError( status ) )
			return( status );
		currentLength += length;
		}

	*queryLength = currentLength;
	return( CRYPT_OK );
	}

/* Bind parameters for a query/update.  The caller has to supply the bound
   data storage since it still has to exist later on when the query is
   executed */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3, 4 ) ) \
static int bindParameters( const SQLHSTMT hStmt, 
						   IN_ARRAY_C( BOUND_DATA_MAXITEMS ) \
							const BOUND_DATA *boundData,
						   OUT BOUND_DATA_STATE *boundDataState,
						   INOUT DBMS_STATE_INFO *dbmsInfo )
	{
	SQLUSMALLINT paramNo = 1;
	int i;

	assert( isReadPtr( boundData, \
					   sizeof( BOUND_DATA ) * BOUND_DATA_MAXITEMS ) );
	assert( isWritePtr( boundDataState, sizeof( BOUND_DATA_STATE ) ) );
	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );

	memset( boundDataState, 0, sizeof( BOUND_DATA_STATE ) );

	/* Bind in any necessary parameters to the hStmt */
	for( i = 0; i < BOUND_DATA_MAXITEMS && \
				boundData[ i ].type != BOUND_DATA_NONE; i++ )
		{
		const BOUND_DATA *boundDataPtr = &boundData[ i ];
		SQLSMALLINT valueType, parameterType;
		SQLRETURN sqlStatus;

		if( boundDataPtr->type == BOUND_DATA_TIME )
			{
			SQL_TIMESTAMP_STRUCT *timestampStorage = \
								 &boundDataState->timestampStorage;
			struct tm timeInfo, *timeInfoPtr = &timeInfo;
		
			REQUIRES( boundDataPtr->dataLength == sizeof( time_t ) );

			/* Sanity check the input parameters */
			timeInfoPtr = gmTime_s( boundDataPtr->data, timeInfoPtr );
			if( timeInfoPtr == NULL )
				return( CRYPT_ERROR_BADDATA );

			/* Bind in the date.  The handling of the ColumnSize parameter 
			   is ugly, this value should be implicit in the underlying data 
			   type but a small number of back-ends (e.g. ones derived from 
			   the Sybase 4.2 code line, which includes the current Sybase 
			   and SQL Server) may support multiple time representations and 
			   require an explicit length indicator to decide which one they 
			   should use.  This means that we have to provide an explicit 
			   length value as a hint to the driver, see the comment in 
			   getDatatypeInfo() for how this is obtained */
			memset( timestampStorage, 0, sizeof( SQL_TIMESTAMP_STRUCT ) );
			timestampStorage->year = ( SQLSMALLINT ) ( timeInfoPtr->tm_year + 1900 );
			timestampStorage->month = ( SQLUSMALLINT ) ( timeInfoPtr->tm_mon + 1 );
			timestampStorage->day = ( SQLUSMALLINT ) timeInfoPtr->tm_mday;
			timestampStorage->hour = ( SQLUSMALLINT ) timeInfoPtr->tm_hour;
			timestampStorage->minute = ( SQLUSMALLINT ) timeInfoPtr->tm_min;
			timestampStorage->second = ( SQLUSMALLINT ) timeInfoPtr->tm_sec;
			sqlStatus = SQLBindParameter( hStmt, paramNo++, SQL_PARAM_INPUT,
										  SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP,
										  dbmsInfo->dateTimeNameColSize, 0,
										  timestampStorage, 0, NULL );
			if( !sqlStatusOK( sqlStatus ) )
				return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
									  CRYPT_ERROR_BADDATA ) );
			continue;
			}

		assert( ( boundDataPtr->dataLength == 0 ) || \
				isReadPtr( boundDataPtr->data, boundDataPtr->dataLength ) );

		REQUIRES( boundDataPtr != NULL );
		REQUIRES( boundDataPtr->type == BOUND_DATA_STRING || \
				  boundDataPtr->type == BOUND_DATA_BLOB );
		REQUIRES( dbmsInfo->hasBinaryBlobs || \
				  ( !dbmsInfo->hasBinaryBlobs && \
					boundDataPtr->type == BOUND_DATA_STRING ) );
		REQUIRES( ( boundDataPtr->data == NULL && \
					boundDataPtr->dataLength == 0 ) || \
				  ( boundDataPtr->data != NULL && \
					boundDataPtr->dataLength > 0 && 
					boundDataPtr->dataLength < MAX_INTLENGTH_SHORT ) );
				  /* Bound data of { NULL, 0 } denotes a null parameter */

		/* Null parameters have to be handled specially.  Note that we have 
		   to set the ColumnSize parameter (no.6) to a nonzero value (even 
		   though it's ignored, since this is a null parameter) otherwise 
		   some drivers will complain about an "Invalid precision value" */
		if( boundDataPtr->dataLength <= 0 )
			{
			static const SQLINTEGER nullDataValue = SQL_NULL_DATA;

			sqlStatus = SQLBindParameter( hStmt, paramNo++, SQL_PARAM_INPUT,
										  SQL_C_CHAR, SQL_C_CHAR, 1, 0, NULL, 0, 
										  ( SQLINTEGER * ) &nullDataValue );
			if( !sqlStatusOK( sqlStatus ) )
				return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
									  CRYPT_ERROR_BADDATA ) );
			continue;
			}

		/* Bind in the query data */
		if( boundDataPtr->type == BOUND_DATA_BLOB )
			{
			valueType = SQL_C_BINARY;
			parameterType = dbmsInfo->blobType;
			}
		else
			valueType = parameterType = SQL_C_CHAR;
		boundDataState->lengthStorage[ i ] = boundDataPtr->dataLength;
		sqlStatus = SQLBindParameter( hStmt, paramNo++, SQL_PARAM_INPUT,
									  valueType, parameterType,
									  boundDataPtr->dataLength, 0,
									  ( SQLPOINTER ) boundDataPtr->data, 
									  boundDataPtr->dataLength, 
									  &boundDataState->lengthStorage[ i ] );
		if( !sqlStatusOK( sqlStatus ) )
			return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
								  CRYPT_ERROR_BADDATA ) );
		}
	ENSURES( i < BOUND_DATA_MAXITEMS );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 Get Database Back-end Information					*
*																			*
****************************************************************************/

/* Get data type information for this data source.  Since SQLGetTypeInfo() 
   returns a variable (and arbitrary) length result set we have to call
   SQLCloseCursor() after each fetch before performing a new query */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int getBlobInfo( INOUT DBMS_STATE_INFO *dbmsInfo, 
						const SQLSMALLINT type,
						OUT_LENGTH_SHORT_Z int *maxFieldSize )
	{
	const SQLHSTMT hStmt = dbmsInfo->hStmt[ DBMS_CACHEDQUERY_NONE ];
	SQLRETURN sqlStatus;
	SQLINTEGER blobNameLength, count, dummy;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );

	/* Clear return value */
	*maxFieldSize = 0;

	/* Check for support for the requested type and get the results of the 
	   transaction.  If the database doesn't support this we'll get an 
	   SQL_NO_DATA status */
	sqlStatus = SQLGetTypeInfo( hStmt, type );
	if( !sqlStatusOK( sqlStatus ) )
		return( CRYPT_ERROR );
	sqlStatus = SQLFetch( hStmt );
	if( !sqlStatusOK( sqlStatus ) )
		{
		SQLCloseCursor( hStmt );
		return( CRYPT_ERROR );
		}

	/* Get the type name (result column 1) and column size (= maximum
	   possible field length, result column 3).  We only check the second
	   return code since they both apply to the same row */
	SQLGetData( hStmt, 1, SQL_C_CHAR, dbmsInfo->blobName,
				CRYPT_MAX_TEXTSIZE, &blobNameLength );
	sqlStatus = SQLGetData( hStmt, 3, SQL_C_SLONG, &count,
							sizeof( SQLINTEGER ), &dummy );
	SQLCloseCursor( hStmt );
	if( !sqlStatusOK( sqlStatus ) )
		return( CRYPT_ERROR );
	if( dbmsInfo->backendType == DBMS_MYSQL && blobNameLength == 0 )
		{
		/* Some older versions of the MySQL ODBC driver don't return a 
		   length value so we have to set it ourselves by taking the length 
		   of the returned string.  The null-termination occurs as a side-
		   effect of the buffer being initialised to zeroes */
		blobNameLength = strlen( dbmsInfo->blobName );
		}
	dbmsInfo->blobNameLength = ( int ) blobNameLength;
#ifdef __UNIX__
	if( dummy != sizeof( SQLINTEGER ) )
		{
		fprintf( stderr, "\ncryptlib: The ODBC driver is erroneously "
				 "returning a %ld-byte integer value\n          when a "
				 "%d-byte SQLINTEGER value is requested, which will "
				 "overwrite\n          adjacent memory locations.  To fix "
				 "this you need to recompile\n          with whatever "
				 "preprocessor options your ODBC header files require\n"
				 "          to force the use of 64-bit ODBC data types (and "
				 "report this issue\n          to the ODBC driver vendor so "
				 "that they can sync the driver and\n          headers)."
				 "\n\n", ( long ) dummy, ( int ) sizeof( SQLINTEGER ) );
				 /* Cast is nec. because sizeof() can be 32 or 64 bits */
		}
#endif /* __UNIX__ */
	*maxFieldSize = count;

	/* We've got the type information, remember the details.  Postgres has 
	   problems handling blobs via ODBC (or even in general) since it uses 
	   its own BYTEA (byte array) type that's not really usable as an SQL 
	   blob type because of weird escaping requirements when 
	   sending/receiving data.  In addition it requires other Postgres-
	   specific oddities like specifying 'ByteaAsLongVarBinary=1' in the 
	   connect string.  So even though the back-end sort-of supports blobs 
	   we can't actually use them */
	if( ( type == SQL_LONGVARBINARY ) && \
		( dbmsInfo->backendType != DBMS_POSTGRES ) )
		dbmsInfo->hasBinaryBlobs = TRUE;
	dbmsInfo->blobType = type;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getDateTimeInfo( INOUT DBMS_STATE_INFO *dbmsInfo )
	{
	const SQLHSTMT hStmt = dbmsInfo->hStmt[ DBMS_CACHEDQUERY_NONE ];
	SQLRETURN sqlStatus;
	SQLINTEGER dateTimeNameLength, columnSize, dummy;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );

	/* The Postgres driver doesn't correctly detect the date/time type used
	   by the back-end so we have to hard-code in the actual value */
	if( dbmsInfo->backendType == DBMS_POSTGRES )
		{
		memcpy( dbmsInfo->dateTimeName, "TIMESTAMP", 9 );
		dbmsInfo->dateTimeNameLength = 9;
		dbmsInfo->dateTimeNameColSize = 16;

		return( CRYPT_OK );
		}

	/* Get information on the back-end's date+time data type  This changed 
	   from SQL_TIMESTAMP in ODBC 2.x to SQL_TYPE_TIMESTAMP in ODBC 3.x, 
	   since 3.x will be more common we try the 3.x version first and if 
	   that fails fall back to 2.x */
	sqlStatus = SQLGetTypeInfo( hStmt, SQL_TYPE_TIMESTAMP );
	if( !sqlStatusOK( sqlStatus ) )
		{
		DEBUG_DIAG(( "Database backend uses pre-ODBC 3.0 data types" ));
		assert( DEBUG_WARN );	/* Warn of absenceof ODBC 3.0 types */
		sqlStatus = SQLGetTypeInfo( hStmt, SQL_TIMESTAMP );
		}
	if( !sqlStatusOK( sqlStatus ) )
		return( CRYPT_ERROR );

	/* Fetch the results of the transaction and get the type name (result 
	   column 1) and column size (result column 3) */
	sqlStatus = SQLFetch( hStmt );
	if( !sqlStatusOK( sqlStatus ) )
		{
		SQLCloseCursor( hStmt );
		return( CRYPT_ERROR );
		}
	sqlStatus = SQLGetData( hStmt, 1, SQL_C_CHAR, dbmsInfo->dateTimeName,
							CRYPT_MAX_TEXTSIZE, &dateTimeNameLength );
	if( sqlStatusOK( sqlStatus ) )
		sqlStatus = SQLGetData( hStmt, 3, SQL_C_SLONG,
								&dbmsInfo->dateTimeNameColSize,
								sizeof( SQLINTEGER ), &dummy );
	if( !sqlStatusOK( sqlStatus ) )
		{
		SQLCloseCursor( hStmt );
		return( CRYPT_ERROR );
		}
	if( dbmsInfo->backendType == DBMS_MYSQL && dateTimeNameLength == 0 )
		{
		/* Some older versions of the MySQL ODBC driver don't return a 
		   length value so we have to set it ourselves by taking the length 
		   of the returned string.  The null-termination occurs as a side-
		   effect of the buffer being initialised to zeroes */
		dateTimeNameLength = strlen( dbmsInfo->dateTimeName );
		}
	dbmsInfo->dateTimeNameLength = ( int ) dateTimeNameLength;

	/* The column size argument is quite problematic because although some 
	   back-ends have a fixed size for this (and usually ignore the column-
	   size parameter) others allow multiple time representations and 
	   require an explicit column-size indicator to decide which one they 
	   should use.  The ODBC standard representation for example uses 19 
	   chars (yyyy-mm-dd hh:mm:ss) for the full date+time that we use here 
	   but also allows a 16-char version without the seconds and a 20+n-char 
	   version for n digits of fractional seconds.  The back-end however may 
	   use a completely different value, for example Oracle encodes the full 
	   date+time as 7 bytes (century, year, month, day, hour, minute, 
	   second).  To get around this we get the first column-size value 
	   (which is usually the only one available) and if this is the same as 
	   the ODBC standard minimum-size column we try for more results to see 
	   if the full date+time form is available, and use that if it is */
	if( dbmsInfo->dateTimeNameColSize != 16 )
		{
		/* This isn't a potentially problematic column size, we're done */
		SQLCloseCursor( hStmt );
		return( CRYPT_OK );
		}
		
	/* If the back-end has reported the short (no-seconds) ODBC-default 
	   format, see whether it'll support the longer (with seconds) format 
	   instead */
	sqlStatus = SQLFetch( hStmt );
	if( !sqlStatusOK( sqlStatus ) )
		{
		SQLCloseCursor( hStmt );
		return( CRYPT_ERROR );
		}
	sqlStatus = SQLGetData( hStmt, 3, SQL_C_SLONG, &columnSize, 
							sizeof( SQLINTEGER ), &dummy );
	if( sqlStatusOK( sqlStatus ) && columnSize == 19 )
		dbmsInfo->dateTimeNameColSize = columnSize;
	SQLCloseCursor( hStmt );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getDatatypeInfo( INOUT DBMS_STATE_INFO *dbmsInfo, 
							OUT_FLAGS_Z( DBMS ) int *featureFlags )
	{
#ifdef USE_ERRMSGS
	ERROR_INFO *errorInfo = &dbmsInfo->errorInfo;
#endif /* USE_ERRMSGS */
	const SQLHSTMT hStmt = dbmsInfo->hStmt[ DBMS_CACHEDQUERY_NONE ];
	SQLRETURN sqlStatus;
	SQLSMALLINT bufLen;
	SQLUSMALLINT transactBehaviour;
	SQLINTEGER attrLength;
	SQLUINTEGER privileges;
	char buffer[ 8 + 8 ];
	int maxBlobSize, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );
	assert( isWritePtr( featureFlags, sizeof( int ) ) );

	/* Clear return value */
	*featureFlags = DBMS_FEATURE_FLAG_NONE;

	/* First we see what the back-end's blob data type is.  Usually it'll
	   be binary blobs, if that doesn't work we try for char blobs */
	status = getBlobInfo( dbmsInfo, SQL_LONGVARBINARY, &maxBlobSize );
	if( cryptStatusError( status ) )
		status = getBlobInfo( dbmsInfo, SQL_LONGVARCHAR, &maxBlobSize );
	if( cryptStatusError( status ) )
		return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
							  CRYPT_ERROR_OPEN ) );
	if( dbmsInfo->hasBinaryBlobs )
		*featureFlags |= DBMS_FEATURE_FLAG_BINARYBLOBS;

	/* If we couldn't get a blob type or the type is too short to use,
	   report it back as a database open failure */
	if( maxBlobSize < MAX_ENCODED_CERT_SIZE )
		{
		char errorMessage[ 128 + 8 ];
		int errorMessageLength;

		errorMessageLength = \
			sprintf_s( errorMessage, 128,
					   "Database blob type can only store %d bytes, we need "
					   "at least %d", maxBlobSize, MAX_ENCODED_CERT_SIZE );
		setErrorString( errorInfo, errorMessage, errorMessageLength );
		return( CRYPT_ERROR_OPEN );
		}

	/* Sanity check, make sure that the source can return the required
	   amount of data.  A number of data sources don't support this
	   attribute (it's mostly meant to be set by applications rather than
	   being read, and is intended to be used to reduce network traffic) and
	   in addition the maximum query size is pretty short (the longest is a 
	   few hundred bytes for the table creation commands) so we don't worry 
	   if it's not available.  In addition to the maximum-size check we also 
	   have to perform a minimum-size check since a value of zero is used to 
	   indicate no length limit */
	sqlStatus = SQLGetStmtAttr( hStmt, SQL_ATTR_MAX_LENGTH,
								( SQLPOINTER ) &attrLength, SQL_IS_INTEGER,
								NULL );
	if( sqlStatusOK( sqlStatus ) && \
		attrLength > 0 && attrLength < MAX_SQL_QUERY_SIZE )
		{
		char errorMessage[ 128 + 8 ];
		int errorMessageLength;

		errorMessageLength = \
			sprintf_s( errorMessage, 128,
					   "Database back-end can only transmit %d bytes per "
					   "message, we need at least %d", attrLength, 
					   MAX_SQL_QUERY_SIZE );
		setErrorString( errorInfo, errorMessage, errorMessageLength );
		return( CRYPT_ERROR_OPEN );
		}

	/* Now do the same thing for the date+time data type */
	status = getDateTimeInfo( dbmsInfo );
	if( cryptStatusError( status ) )
		return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
							  CRYPT_ERROR_OPEN ) );

	/* Determine whether we can write to the database (result = 'Y') or not
	   (result = 'N') */
	sqlStatus = SQLGetInfo( dbmsInfo->hDbc, SQL_DATA_SOURCE_READ_ONLY,
							buffer, 8, &bufLen );
	if( sqlStatusOK( sqlStatus ) && *buffer == 'Y' )
		*featureFlags |= DBMS_FEATURE_FLAG_READONLY;

	/* Determine whether GRANT/REVOKE capabilities are available.  This gets
	   a bit messy because it only specifies which extended GRANT/REVOKE
	   options are available rather than whether GRANT/REVOKE is available
	   at all.  To handle this we treat GRANT/REVOKE as being available if
	   any information is returned (SQL Server typically returns only
	   SQL_SG_WITH_GRANT_OPTION while other sources like DB2, Postgres, and
	   Sybase return the correct set of flags) and not available if nothing
	   is returned (Access, dBASE, Paradox, etc).  To make things especially
	   challenging, Informix returns nothing for SQL_SQL92_GRANT but does
	   return something for SQL_SQL92_REVOKE so we have to check both and
	   allow GRANT/REVOKE if either test positive */
	sqlStatus = SQLGetInfo( dbmsInfo->hDbc, SQL_SQL92_GRANT,
							( SQLPOINTER ) &privileges,
							sizeof( SQLUINTEGER ), &bufLen );
	if( sqlStatusOK( sqlStatus ) && privileges )
		*featureFlags |= DBMS_FEATURE_FLAG_PRIVILEGES;
	sqlStatus = SQLGetInfo( dbmsInfo->hDbc, SQL_SQL92_REVOKE,
							( SQLPOINTER ) &privileges,
							sizeof( SQLUINTEGER ), &bufLen );
	if( sqlStatusOK( sqlStatus ) && privileges )
		*featureFlags |= DBMS_FEATURE_FLAG_PRIVILEGES;

	/* Check how the back-end reacts to commit/rollback operations.  If
	   transactions are destructive (that is, prepared statements are
	   cleared when a commit/rollback is performed) we have to clear the
	   hStmtPrepared[] array to indicate that all statements have to be
	   re-prepared.  Fortunately this is quite rare, both because most
	   back-ends don't do this (for virtually all ODBC-accessible data
	   sources (SQL Server, Access, dBASE, Paradox, etc etc) the behaviour
	   is SQL_CB_CLOSE, meaning that the currently active cursor is closed
	   but there's no need to call SQLPrepare() again) and because 
	   transactions are used with CA certificate stores opened in read/write 
	   mode */
	sqlStatus = SQLGetInfo( dbmsInfo->hDbc, SQL_CURSOR_COMMIT_BEHAVIOR,
							&transactBehaviour, sizeof( SQLUSMALLINT ),
							&bufLen );
	if( sqlStatusOK( sqlStatus ) && transactBehaviour == SQL_CB_DELETE )
		{
		DEBUG_DIAG(( "Database uses destructive transactions" ));
		assert( DEBUG_WARN );
		dbmsInfo->transactIsDestructive = TRUE;
		}
	sqlStatus = SQLGetInfo( dbmsInfo->hDbc, SQL_CURSOR_ROLLBACK_BEHAVIOR,
							&transactBehaviour, sizeof( SQLUSMALLINT ),
							&bufLen );
	if( sqlStatusOK( sqlStatus ) && transactBehaviour == SQL_CB_DELETE )
		{
		DEBUG_DIAG(( "Database uses destructive transactions" ));
		assert( DEBUG_WARN );
		dbmsInfo->transactIsDestructive = TRUE;
		}

	return( CRYPT_OK );
	}

/* Get the back-end type for this data source, which allows us to work 
   around back-end-specific bugs and peculiarities */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getBackendInfo( INOUT DBMS_STATE_INFO *dbmsInfo )
	{
	SQLRETURN sqlStatus;
	SQLSMALLINT bufLen;
	char buffer[ 128 + 8 ];

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );

	/* Check for various back-ends that require special-case handling */
	sqlStatus = SQLGetInfo( dbmsInfo->hDbc, SQL_DBMS_NAME, buffer, 128 - 1,
							&bufLen );
	if( sqlStatusOK( sqlStatus ) )
		{
		buffer[ bufLen ] = '\0';	/* Keep static source anal.tools happy */
		if( bufLen >= 6 && !strCompare( buffer, "Access", 6 ) )
			dbmsInfo->backendType = DBMS_ACCESS;
		if( bufLen >= 9 && !strCompare( buffer, "Interbase", 9 ) )
			dbmsInfo->backendType = DBMS_INTERBASE;
		if( bufLen >= 5 && !strCompare( buffer, "MySQL", 5 ) )
			dbmsInfo->backendType = DBMS_MYSQL;
		if( bufLen >= 12 && !strCompare( buffer, "PostgreSQL", 10 ) )
			dbmsInfo->backendType = DBMS_POSTGRES;
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Database Open/Close Routines					*
*																			*
****************************************************************************/

/* Close a previously-opened ODBC connection.  We have to have this before
   openDatabase() since it may be called by openDatabase() if the open
   process fails */

STDC_NONNULL_ARG( ( 1 ) ) \
static void closeDatabase( INOUT DBMS_STATE_INFO *dbmsInfo )
	{
	SQLRETURN sqlStatus;
	int i;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );

	/* Commit the transaction.  The default transaction mode is auto-commit
	   so the SQLEndTran() call isn't strictly necessary, but we play it
	   safe anyway */
	if( dbmsInfo->needsUpdate )
		{
		sqlStatus = SQLEndTran( SQL_HANDLE_DBC, dbmsInfo->hDbc, SQL_COMMIT );
		if( !sqlStatusOK( sqlStatus ) )
			{
			DUMP_ODBCERROR( "SQLEndTran()", dbmsInfo, SQL_ERRLVL_DBC, 
							SQL_NULL_HSTMT );
			assert( DEBUG_WARN );	/* Catch this if it ever occurs */
			}
		dbmsInfo->needsUpdate = FALSE;
		}

	/* Clean up */
	for( i = 0; i < NO_CACHED_QUERIES; i++ )
		{
		if( dbmsInfo->hStmt[ i ] != NULL )
			{
			sqlStatus = SQLFreeHandle( SQL_HANDLE_STMT, dbmsInfo->hStmt[ i ] );
			if( !sqlStatusOK( sqlStatus ) )
				{
				DUMP_ODBCERROR( "SQLFreeHandle()", dbmsInfo, 
								SQL_ERRLVL_STMT, dbmsInfo->hStmt[ i ] );
				assert( DEBUG_WARN );	/* Catch this if it ever occurs */
				}
			dbmsInfo->hStmtPrepared[ i ] = FALSE;
			dbmsInfo->hStmt[ i ] = NULL;
			}
		}
	if( dbmsInfo->connectionOpen )
		{
		sqlStatus = SQLDisconnect( dbmsInfo->hDbc );
		if( !sqlStatusOK( sqlStatus ) )
			{
			DUMP_ODBCERROR( "SQLDisconnect()", dbmsInfo, SQL_ERRLVL_DBC, 
							SQL_NULL_HSTMT );
			assert( DEBUG_WARN );	/* Catch this if it ever occurs */
			}
		dbmsInfo->connectionOpen = FALSE;
		}
	sqlStatus = SQLFreeHandle( SQL_HANDLE_DBC, dbmsInfo->hDbc );
	if( !sqlStatusOK( sqlStatus ) )
		{
		DUMP_ODBCERROR( "SQLFreeHandle()", dbmsInfo, SQL_ERRLVL_DBC, 
						SQL_NULL_HSTMT );
		assert( DEBUG_WARN );	/* Catch this if it ever occurs */
		}
	sqlStatus = SQLFreeHandle( SQL_HANDLE_ENV, dbmsInfo->hEnv );
	assert( sqlStatusOK( sqlStatus ) );			/* Warn of potential errors */
	dbmsInfo->hDbc = NULL;
	dbmsInfo->hEnv = NULL;
	}

/* Open a connection to a data source.  We don't check the return codes for
   many of the parameter-fiddling functions since the worst that can happen
   if they fail is that performance will be somewhat suboptimal and it's not
   worth abandoning the database open just because some obscure tweak isn't
   supported.

   In addition to the main hStmt handle we also allocate a number of 
   additional hStmts used to contain pre-prepared, cached instances of 
   frequently-executed queries.  This means that the expensive step of 
   parsing the SQL query, validating it against the system catalog, 
   preparing an access plan, and optimising the plan, are only performed 
   once on the first query rather than at every single access */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
static int openDatabase( INOUT DBMS_STATE_INFO *dbmsInfo, 
						 IN_BUFFER( nameLen ) const char *name,
						 IN_LENGTH_NAME const int nameLen, 
						 IN_ENUM_OPT( CRYPT_KEYOPT ) \
							const CRYPT_KEYOPT_TYPE options, 
						 OUT_FLAGS_Z( DBMS ) int *featureFlags )
	{
#ifdef USE_ERRMSGS
	ERROR_INFO *errorInfo = &dbmsInfo->errorInfo;
#endif /* USE_ERRMSGS */
	DBMS_NAME_INFO nameInfo;
	SQLRETURN sqlStatus;
	int i, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );
	assert( isReadPtr( name, nameLen ) );
	assert( isWritePtr( featureFlags, sizeof( int ) ) );

	REQUIRES( nameLen >= MIN_NAME_LENGTH && \
			  nameLen < MAX_ATTRIBUTE_SIZE );
	REQUIRES( options >= CRYPT_KEYOPT_NONE && options < CRYPT_KEYOPT_LAST );

	/* Clear return values */
	memset( dbmsInfo, 0, sizeof( DBMS_STATE_INFO ) );
	*featureFlags = DBMS_FEATURE_FLAG_NONE;

#ifdef DYNAMIC_LOAD
	/* Make sure that the driver is bound in */
	if( hODBC == NULL_INSTANCE )
		return( CRYPT_ERROR_OPEN );
#endif /* DYNAMIC_LOAD */

	/* Parse the data source into its individual components */
	status = dbmsParseName( &nameInfo, name, nameLen );
	if( cryptStatusError( status ) )
		return( status );

	/* Allocate environment and connection handles.  Before we do anything
	   with the environment handle we have to set the ODBC version to 3 or
	   any succeeding calls will fail with a function sequence error.  God
	   knows why they couldn't assume a default setting of ODBC 3.x for this
	   value when it requires an ODBC 3.x function call to get here in the
	   first place */
	sqlStatus = SQLAllocHandle( SQL_HANDLE_ENV, SQL_NULL_HANDLE,
								&dbmsInfo->hEnv );
	if( !sqlStatusOK( sqlStatus ) )
		{
		/* We can't get any error details without at least an environment
		   handle so all that we can do is return a generic allocation error
		   message.  If we get a failure at this point (and in particular
		   on the very first ODBC call) it's usually a sign of an incorrect
		   ODBC install or config (on non-Windows systems where it's not
		   part of the OS), since the ODBC driver can't initialise itself */
#ifdef __WINDOWS__
		setErrorString( errorInfo, 
						"Couldn't allocate database connection handle", 44 );
#else
		setErrorString( errorInfo, 
						"Couldn't allocate database connection handle, this "
						"is probably due to an incorrect ODBC driver install "
						"or an invalid configuration", 130 );
#endif /* __WINDOWS__ */
		return( CRYPT_ERROR_OPEN );
		}
	sqlStatus = SQLSetEnvAttr( dbmsInfo->hEnv, SQL_ATTR_ODBC_VERSION,
								VALUE_TO_PTR( SQL_OV_ODBC3 ),
								SQL_IS_INTEGER );
	if( sqlStatusOK( sqlStatus ) )
		sqlStatus = SQLAllocHandle( SQL_HANDLE_DBC, dbmsInfo->hEnv,
									&dbmsInfo->hDbc );
	if( !sqlStatusOK( sqlStatus ) )
		{
		status = getErrorInfo( dbmsInfo, SQL_ERRLVL_ENV, SQL_NULL_HSTMT,
							   CRYPT_ERROR_OPEN );
		SQLFreeHandle( SQL_HANDLE_ENV, dbmsInfo->hEnv );
		return( status );
		}

	/* Once everything is set up the way that we want it, try to connect to 
	   a data source and allocate a statement handle.  If there's an error
	   we dump the information to the error log, since this is a create-
	   object function and so extended error information can't be read since
	   the object won't be created */
	sqlStatus = SQLConnect( dbmsInfo->hDbc,
							( SQLCHAR * ) nameInfo.name, 
							( SQLSMALLINT ) nameInfo.nameLen,
							( SQLCHAR * ) nameInfo.user, 
							( SQLSMALLINT ) nameInfo.userLen,
							( SQLCHAR * ) nameInfo.password, 
							( SQLSMALLINT ) nameInfo.passwordLen );
	if( !sqlStatusOK( sqlStatus ) )
		{
		status = getErrorInfo( dbmsInfo, SQL_ERRLVL_DBC, SQL_NULL_HSTMT,
							   CRYPT_ERROR_OPEN );
		DEBUG_DIAG(( "SQLConnect returned error %d, status:\n  '%s'.", 
					 sqlStatus, dbmsInfo->errorInfo.errorString ));
		closeDatabase( dbmsInfo );
		return( status );
		}
	dbmsInfo->connectionOpen = TRUE;

	/* Now that the connection is open, allocate the statement handles */
	for( i = 0; i < NO_CACHED_QUERIES && sqlStatusOK( sqlStatus ); i++ )
		{
		sqlStatus = SQLAllocHandle( SQL_HANDLE_STMT, dbmsInfo->hDbc,
									&dbmsInfo->hStmt[ i ] );
		}
	if( !sqlStatusOK( sqlStatus ) )
		{
		status = getErrorInfo( dbmsInfo, SQL_ERRLVL_DBC, SQL_NULL_HSTMT,
							   CRYPT_ERROR_OPEN );
		DEBUG_DIAG(( "SQLAllocHandle returned error %d, status:\n  '%s'.", 
					 sqlStatus, dbmsInfo->errorInfo.errorString ));
		closeDatabase( dbmsInfo );
		return( status );
		}

	/* Set the access mode to read-only if we can.  The default is R/W, but
	   setting it to read-only optimises transaction management */
	if( options == CRYPT_KEYOPT_READONLY )
		{
		( void ) SQLSetStmtAttr( dbmsInfo->hDbc, SQL_ATTR_ACCESS_MODE,
								 VALUE_TO_PTR( SQL_MODE_READ_ONLY ), 
								 SQL_IS_INTEGER );
		}

	/* Set the cursor type to forward-only (which should be the default
	   anyway), concurrency to read-only if we're opening the database in
	   read-only mode (this again should be the default), and turn off
	   scanning for escape clauses in the SQL strings, which lets the driver
	   pass the string directly to the data source.  The latter improves
	   both performance and (to some extent) security by reducing the
	   chances of hostile SQL injection, or at least by requiring specially
	   crafted back-end specific SQL rather than generic ODBC SQL to
	   function */
	for( i = 0; i < NO_CACHED_QUERIES; i++ )
		{
		( void ) SQLSetStmtAttr( dbmsInfo->hStmt[ i ], SQL_ATTR_CURSOR_TYPE,
								 VALUE_TO_PTR( SQL_CURSOR_FORWARD_ONLY ),
								 SQL_IS_INTEGER );
		if( options == CRYPT_KEYOPT_READONLY )
			{
			( void ) SQLSetStmtAttr( dbmsInfo->hStmt[ i ], 
									 SQL_ATTR_CONCURRENCY,
									 VALUE_TO_PTR( SQL_CONCUR_READ_ONLY ),
									 SQL_IS_INTEGER );
			}
		( void ) SQLSetStmtAttr( dbmsInfo->hStmt[ i ], SQL_ATTR_NOSCAN,
								 VALUE_TO_PTR( SQL_NOSCAN_ON ), 
								 SQL_IS_INTEGER );
		}

	/* Get various driver and data source-specific information that we may
	   need later on */
	status = getDatatypeInfo( dbmsInfo, featureFlags );
	if( cryptStatusOK( status ) )
		status = getBackendInfo( dbmsInfo );
	if( cryptStatusError( status ) )
		{
		closeDatabase( dbmsInfo );
		return( status );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Database Read Routines							*
*																			*
****************************************************************************/

/* Fetch data from a query */

CHECK_RETVAL STDC_NONNULL_ARG( ( 6 ) ) \
static int fetchData( const SQLHSTMT hStmt, 
					  OUT_BUFFER_OPT( dataMaxLength, *dataLength ) \
						char *data,
					  IN_LENGTH_SHORT_Z const int dataMaxLength, 
					  OUT_OPT_LENGTH_SHORT_Z int *dataLength, 
					  IN_ENUM( DBMS_QUERY ) const DBMS_QUERY_TYPE queryType,
					  INOUT DBMS_STATE_INFO *dbmsInfo )
	{
#ifdef USE_ERRMSGS
	ERROR_INFO *errorInfo = &dbmsInfo->errorInfo;
#endif /* USE_ERRMSGS */
	const SQLSMALLINT dataType = ( SQLSMALLINT  ) \
								 ( ( dbmsInfo->hasBinaryBlobs ) ? \
								   SQL_C_BINARY : SQL_C_CHAR );
	SQLRETURN sqlStatus;
	SQLINTEGER length;

	assert( ( queryType == DBMS_QUERY_CHECK && \
			  data == NULL && dataMaxLength == 0 && dataLength == NULL ) || \
			( queryType != DBMS_QUERY_CHECK && \
			  isWritePtr( data, dataMaxLength ) && \
			  isWritePtr( dataLength, sizeof( int ) ) ) );
	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );

	REQUIRES( ( queryType == DBMS_QUERY_CHECK && \
				data == NULL && dataMaxLength == 0 && \
				dataLength == NULL ) || \
			  ( queryType != DBMS_QUERY_CHECK && \
				data != NULL && dataMaxLength >= 16 && \
				dataMaxLength < MAX_INTLENGTH_SHORT && \
				dataLength != NULL ) );
	REQUIRES( queryType > DBMS_QUERY_NONE && \
			  queryType < DBMS_QUERY_LAST );

	/* Clear return value */
	if( data != NULL )
		{
		memset( data, 0, min( 16, dataMaxLength ) );
		*dataLength = 0;
		}

	/* Get the results of the transaction */
	sqlStatus = SQLFetch( hStmt );
	if( !sqlStatusOK( sqlStatus ) )
		{
		/* If the fetch status is SQL_NO_DATA, indicating the end of the
		   result set, we handle it specially since some drivers only return
		   the basic error code and don't provide any further diagnostic
		   information to be fetched by SQLGetDiagRec() */
		if( sqlStatus == SQL_NO_DATA )
			{
			if( queryType == DBMS_QUERY_CONTINUE )
				{ 
				setErrorString( errorInfo, "No more data found", 18 );
				}
			else
				{
				setErrorString( errorInfo, "No data found", 13 );
				}
			return( CRYPT_ERROR_NOTFOUND );
			}
		return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
							  CRYPT_ERROR_READ ) );
		}

	/* If we're just doing a presence check we don't bother fetching data */
	if( queryType == DBMS_QUERY_CHECK )
		return( CRYPT_OK );

	/* Read the data */
	sqlStatus = SQLGetData( hStmt, 1, dataType, data, dataMaxLength, 
							&length );
	if( !sqlStatusOK( sqlStatus ) )
		return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
							  CRYPT_ERROR_READ ) );
	*dataLength = ( int ) length;
	return( CRYPT_OK );
	}

/* Perform a transaction that returns information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int performQuery( INOUT DBMS_STATE_INFO *dbmsInfo, 
						 IN_BUFFER_OPT( commandLength ) const char *command,
						 IN_LENGTH_SHORT_Z const int commandLength, 
						 OUT_BUFFER_OPT( dataMaxLength, *dataLength ) \
							void *data, 
						 IN_LENGTH_SHORT_Z const int dataMaxLength, 
						 OUT_LENGTH_BOUNDED_Z( dataMaxLength ) \
							int *dataLength, 
						 IN_ARRAY_OPT_C( BOUND_DATA_MAXITEMS ) \
							TYPECAST( BOUND_DATA ) const void *boundData,
						 IN_ENUM_OPT( DBMS_CACHEDQUERY ) \
							const DBMS_CACHEDQUERY_TYPE queryEntry,
						 IN_ENUM( DBMS_QUERY ) const DBMS_QUERY_TYPE queryType )
	{
	const SQLHSTMT hStmt = dbmsInfo->hStmt[ queryEntry ];
	BOUND_DATA_STATE boundDataState;
	SQLRETURN sqlStatus;
	int status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );
	assert( ( command == NULL && commandLength == 0 && \
			  ( queryType == DBMS_QUERY_CONTINUE || \
				queryType == DBMS_QUERY_CANCEL ) ) || \
			isReadPtr( command, commandLength ) );
	assert( ( data == NULL && dataMaxLength == 0 && dataLength == NULL ) || \
			( isWritePtr( data, dataMaxLength ) && \
			  isWritePtr( dataLength, sizeof( int ) ) ) );
	assert( ( boundData == NULL ) || \
			isReadPtr( boundData, \
					   sizeof( BOUND_DATA ) * BOUND_DATA_MAXITEMS ) );

	REQUIRES( ( ( queryType == DBMS_QUERY_CONTINUE || \
				  queryType == DBMS_QUERY_CANCEL ) && \
				command == NULL && commandLength == 0 ) || \
			  ( ( queryType == DBMS_QUERY_START || \
				  queryType == DBMS_QUERY_CHECK || \
				  queryType == DBMS_QUERY_NORMAL ) && \
				command != NULL && \
				commandLength > 0 && commandLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( data == NULL && dataMaxLength == 0 && \
				dataLength == NULL ) || \
			  ( data != NULL && dataMaxLength >= 16 && \
				dataMaxLength < MAX_INTLENGTH_SHORT && \
				dataLength != NULL ) );
	REQUIRES( queryEntry >= DBMS_CACHEDQUERY_NONE && \
			  queryEntry < DBMS_CACHEDQUERY_LAST );
	REQUIRES( queryType > DBMS_QUERY_NONE && \
			  queryType < DBMS_QUERY_LAST );

	/* Clear return value */
	if( dataLength != NULL )
		*dataLength = 0;

	/* If we're starting a new query, handle the query initialisation and
	   parameter binding */
	if( queryType == DBMS_QUERY_START || \
		queryType == DBMS_QUERY_CHECK || \
		queryType == DBMS_QUERY_NORMAL )
		{
		/* Prepare the query for execution if necessary.  The entry at 
		   position DBMS_CACHEDQUERY_NONE is never cached so the following 
		   code is always executed for this case */
		if( !dbmsInfo->hStmtPrepared[ queryEntry ] )
			{
			char query[ SQL_QUERY_BUFSIZE + 8 ];
			int queryLength;

			status = convertQuery( dbmsInfo, query, SQL_QUERY_BUFSIZE, 
								   &queryLength, command, commandLength );
			if( cryptStatusError( status ) )
				return( status );
			sqlStatus = SQLPrepare( hStmt, ( SQLCHAR * ) query, 
									queryLength );
			if( !sqlStatusOK( sqlStatus ) )
				return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
									  CRYPT_ERROR_READ ) );
			if( queryEntry != DBMS_CACHEDQUERY_NONE )
				dbmsInfo->hStmtPrepared[ queryEntry ] = TRUE;
			}

		/* Bind in any query parameters that may be required */
		if( boundData != NULL )
			{
			status = bindParameters( hStmt, boundData, &boundDataState, 
									 dbmsInfo );
			if( cryptStatusError( status ) )
				return( status );
			}
		}

	switch( queryType )
		{
		case DBMS_QUERY_START:
			/* Execute the query */
			sqlStatus = SQLExecute( hStmt );
			if( !sqlStatusOK( sqlStatus ) )
				return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
									  CRYPT_ERROR_READ ) );

			/* If we're starting an ongoing query with results to be fetched
			   later, we're done */
			if( data == NULL )
				return( CRYPT_OK );

			/* Drop through to fetch the first set of results */

		case DBMS_QUERY_CONTINUE:
			assert( isWritePtr( data, dataMaxLength ) );

			REQUIRES( data != NULL && \
					  dataMaxLength >= 16 && \
					  dataMaxLength < MAX_INTLENGTH_SHORT );

			/* We're in the middle of a continuing query, fetch the next set
			   of results.  If we've run out of results (indicated by a not-
			   found status) we explicitly signal to the caller that the
			   query has completed */
			status = fetchData( dbmsInfo->hStmt[ queryEntry ], data,
								dataMaxLength, dataLength, 
								DBMS_QUERY_CONTINUE, dbmsInfo );
			return( cryptStatusOK( status ) ? CRYPT_OK : \
					( status == CRYPT_ERROR_NOTFOUND ) ? \
					CRYPT_ERROR_COMPLETE : status );

		case DBMS_QUERY_CANCEL:
			/* Cancel any outstanding requests to clear the hStmt and make 
			   it ready for re-use */
			SQLCloseCursor( dbmsInfo->hStmt[ queryEntry ] );
			return( CRYPT_OK );

		case DBMS_QUERY_CHECK:
		case DBMS_QUERY_NORMAL:
			/* Only return a maximum of a single row in response to a point
			   query.  This is a simple optimisation to ensure that the
			   database client doesn't start sucking across huge amounts of
			   data when it's not necessary */
			( void ) SQLSetStmtAttr( hStmt, SQL_ATTR_MAX_ROWS, 
									 VALUE_TO_PTR( 1 ), SQL_IS_INTEGER );

			/* Execute the SQL statement and fetch the results */
			sqlStatus = SQLExecute( hStmt );
			if( sqlStatusOK( sqlStatus ) )
				{
				status = fetchData( hStmt, data, dataMaxLength, dataLength, 
									queryType, dbmsInfo );
				SQLCloseCursor( hStmt );
				}
			else
				{
				status = getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
									   CRYPT_ERROR_READ );
				}

			/* Reset the statement handle's multi-row result handling */
			( void ) SQLSetStmtAttr( hStmt, SQL_ATTR_MAX_ROWS, 
									 VALUE_TO_PTR( 0 ), SQL_IS_INTEGER );
			return( status );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*						 	Database Write Routines							*
*																			*
****************************************************************************/

/* Perform a transaction that updates the database without returning any
   data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int performUpdate( INOUT DBMS_STATE_INFO *dbmsInfo, 
						  IN_BUFFER_OPT( commandLength ) const char *command,
						  IN_LENGTH_SHORT_Z const int commandLength, 
						  IN_ARRAY_OPT_C( BOUND_DATA_MAXITEMS ) \
							TYPECAST( BOUND_DATA ) const void *boundData,
						  IN_ENUM( DBMS_UPDATE ) \
							const DBMS_UPDATE_TYPE updateType )
	{
	const SQLHSTMT hStmt = dbmsInfo->hStmt[ DBMS_CACHEDQUERY_NONE ];
	BOUND_DATA_STATE boundDataState;
	SQLRETURN sqlStatus;
	char query[ SQL_QUERY_BUFSIZE + 8 ];
	int queryLength, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_STATE_INFO ) ) );
	assert( ( command == NULL && commandLength == 0 && \
			  updateType == DBMS_UPDATE_ABORT ) || \
			isReadPtr( command, commandLength ) );
	assert( ( boundData == NULL ) || \
			isReadPtr( boundData, \
					   sizeof( BOUND_DATA ) * BOUND_DATA_MAXITEMS ) );

	REQUIRES( ( updateType == DBMS_UPDATE_ABORT && \
				command == NULL && commandLength == 0 ) || \
			  ( updateType != DBMS_UPDATE_ABORT && \
				command != NULL && \
				commandLength > 0 && commandLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( updateType > DBMS_UPDATE_NONE && \
			  updateType < DBMS_UPDATE_LAST );

	/* If we're aborting a transaction, roll it back, re-enable autocommit,
	   and clean up */
	if( updateType == DBMS_UPDATE_ABORT )
		{
		sqlStatus = SQLEndTran( SQL_HANDLE_DBC, dbmsInfo->hDbc, 
								SQL_ROLLBACK );
		( void ) SQLSetConnectAttr( dbmsInfo->hDbc, SQL_ATTR_AUTOCOMMIT,
									VALUE_TO_PTR( SQL_AUTOCOMMIT_ON ),
									SQL_IS_UINTEGER );
		if( !sqlStatusOK( sqlStatus ) )
			return( getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
								  CRYPT_ERROR_WRITE ) );
		return( CRYPT_OK );
		}

	/* If it's the start of a transaction, turn autocommit off */
	if( updateType == DBMS_UPDATE_BEGIN )
		{
		( void ) SQLSetConnectAttr( dbmsInfo->hDbc, SQL_ATTR_AUTOCOMMIT,
									VALUE_TO_PTR( SQL_AUTOCOMMIT_OFF ),
									SQL_IS_UINTEGER );
		}

	/* Bind in any necessary parameters to the hStmt */
	if( boundData != NULL )
		{
		status = bindParameters( hStmt, boundData, &boundDataState,
								 dbmsInfo );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Execute the command */
	status = convertQuery( dbmsInfo, query, SQL_QUERY_BUFSIZE, &queryLength,
						   command, commandLength );
	if( cryptStatusError( status ) )
		return( status );
	sqlStatus = SQLExecDirect( hStmt, ( SQLCHAR * ) query, queryLength );
	if( !sqlStatusOK( sqlStatus ) )
		{
		/* If we were supposed to begin a transaction but it failed, reset
		   the autocommit state.  This is necessary because the 
		   DBMS_FLAG_UPDATEACTIVE flag won't be set if the transaction
		   isn't started and therefore the DBMS_UPDATE_COMMIT/
		   DBMS_UPDATE_ABORT action that has to eventually follow a 
		   DBMS_UPDATE_BEGIN will never be performed */
		if( updateType == DBMS_UPDATE_BEGIN )
			{
			( void ) SQLSetConnectAttr( dbmsInfo->hDbc, SQL_ATTR_AUTOCOMMIT,
										VALUE_TO_PTR( SQL_AUTOCOMMIT_ON ),
										SQL_IS_UINTEGER );
			}

		/* The return status from a delete operation can be reported in
		   several ways at the whim of the driver.  Some drivers always
		   report success even though nothing was found to delete (more
		   common in ODBC 2.x drivers, see the code further on for the
		   handling for this), others report a failure to delete anything
		   with an SQL_NO_DATA status (more common in ODBC 3.x drivers).
		   For this case we convert the overall status to a
		   CRYPT_ERROR_NOTFOUND and update the sqlStatus as required if we
		   need to continue */
		if( sqlStatus == SQL_NO_DATA && \
			command != NULL && commandLength >= 6 && \
			!strCompare( command, "DELETE", 6 ) )
			{
			status = CRYPT_ERROR_NOTFOUND;
			if( updateType != DBMS_UPDATE_COMMIT )
				return( status );
			}
		else
			{
			/* If we hit an error at this point we can only exit if we're
			   not finishing a transaction.  If we are, the commit turns
			   into an abort further down */
			status = getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
								   CRYPT_ERROR_WRITE );
			if( updateType != DBMS_UPDATE_COMMIT )
				return( status );
			}
		}
	else
		{
		/* If we're performing a delete the operation will succeed even
		   though nothing was found to delete so we make sure that we
		   actually changed something */
		if( command != NULL && commandLength >= 6 && \
			!strCompare( command, "DELETE", 6 ) )
			{
			SQLINTEGER rowCount;

			sqlStatus = SQLRowCount( hStmt, &rowCount );
			if( !sqlStatusOK( sqlStatus ) || rowCount <= 0 )
				status = CRYPT_ERROR_NOTFOUND;
			}
		}

	/* If it's the end of a transaction, commit the transaction and turn
	   autocommit on again */
	if( updateType == DBMS_UPDATE_COMMIT )
		{
		/* If we've had a failure before this point, abort, otherwise
		   commit.  The SQLSMALLINT cast is necessary (although spurious) in 
		   some development environments */
		sqlStatus = SQLEndTran( SQL_HANDLE_DBC, dbmsInfo->hDbc,
								( SQLSMALLINT ) \
								( cryptStatusError( status ) ? \
								  SQL_ROLLBACK : SQL_COMMIT ) );
		if( dbmsInfo->transactIsDestructive )
			{
			int i;

			/* If transactions are destructive for this back-end type, 
			   invalidate all prepared statements */
			for( i = 0; i < NO_CACHED_QUERIES; i++ )
				dbmsInfo->hStmtPrepared[ i ] = FALSE;
			}
		( void ) SQLSetConnectAttr( dbmsInfo->hDbc, SQL_ATTR_AUTOCOMMIT,
									VALUE_TO_PTR( SQL_AUTOCOMMIT_ON ),
									SQL_IS_UINTEGER );
		if( cryptStatusOK( status ) && !sqlStatusOK( sqlStatus ) )
			{
			status = getErrorInfo( dbmsInfo, SQL_ERRLVL_STMT, hStmt,
								   CRYPT_ERROR_WRITE );
			}
		}

	return( status );
	}

#ifndef USE_RPCAPI

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDispatchODBC( INOUT DBMS_INFO *dbmsInfo )
	{
	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	dbmsInfo->openDatabaseBackend = openDatabase;
	dbmsInfo->closeDatabaseBackend = closeDatabase;
	dbmsInfo->performUpdateBackend = performUpdate;
	dbmsInfo->performQueryBackend = performQuery;

	return( CRYPT_OK );
	}
#else

/* Pull in the shared database RPC routines, renaming the generic dispatch
   function to the ODBC-specific one which is called directly by the
   marshalling code */

#define processCommand( stateInfo, buffer ) \
		odbcProcessCommand( stateInfo, buffer )
#include "dbx_rpc.c"

#endif /* !USE_RPCAPI */

#endif /* USE_ODBC */
