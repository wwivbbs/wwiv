/****************************************************************************
*																			*
*							cryptlib DBMS Interface							*
*						Copyright Peter Gutmann 1996-2014					*
*																			*
****************************************************************************/

#ifndef _DBMS_DEFINED

#define _DBMS_DEFINED

#include <stdarg.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "stream.h"
#else
  #include "crypt.h"
  #include "io/stream.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*							Database Keyset Headers							*
*																			*
****************************************************************************/

/* Handle ODBC support.  This gets rather complex, with all sorts of special-
   case handling of headers and types required across different systems */

#ifdef USE_ODBC

/* As part of the ever-changing way of identifying Win32, Microsoft changed 
   the predefined constant from WIN32 to _WIN32 in VC++ 2.1.  However the 
   ODBC header files still expect to find WIN32 and if this isn't defined 
   will use the default (i.e. C) calling convention instead of the Pascal 
   convention which is actually used by the ODBC functions.  This means that 
   both the caller and the callee clean up the stack so that for each ODBC 
   call the stack creeps upwards by a few bytes until eventually the local 
   variables and/or return address get trashed.  This problem is usually 
   hidden by the fact that something else defines WIN32 so everything works 
   OK, but the October 1997 development platform upgrade changes this so 
   that compiling the code after this update is installed breaks things.  To 
   avoid this problem, we define WIN32 if it isn't defined, which ensures 
   that the ODBC header files work properly */
#if defined( __WIN32__ ) && !defined( WIN32 )
  #define WIN32
#endif /* __WIN32__ && !WIN32 */

#ifdef __WINDOWS__
  /* Borland have their own weird place to put ODBC headers */
  #if defined( __BORLANDC__ )
	#include <mfc/sqltypes.h>
  #else
	/* UnixODBC defines its own version of various Windows types, if we're
	   building under Windows we have to disable this.  The UnixODBC headers 
	   have a guard ALLREADY_HAVE_WINDOWS_TYPE (sic) but this is all-or-
	   nothing, disabling the defining of Windows *and* SQL types.  Defining 
	   the guard value fixes most compile problems but in order to build it 
	   the commented-out typedefs also need to be defined.  These are 
	   already defined in the standard (Windows) sqltypes.h so their use 
	   needs to be manually enabled for UnixODBC under Windows, which is 
	   unlikely to occur given that it's a Unix-only driver */
	#define ALLREADY_HAVE_WINDOWS_TYPE
	#if 0
	typedef signed short RETCODE;
	typedef short int SWORD;
	typedef long int SDWORD;
	typedef signed char SQLSCHAR;
	typedef HWND SQLHWND;
	#endif /* 0 */

	#pragma warning( push )
	#pragma warning( disable : 4255 )	/* Errors in sqlext.h */
	#include <sql.h>
	#include <sqlext.h>
	#pragma warning( pop )
  #endif /* Compiler-specific include locations */
#else
  #include <sql.h>
  #include <sqlext.h>
#endif /* Windows vs.everything else */

#endif /* USE_ODBC */

/****************************************************************************
*																			*
*							Database Keyset Constants						*
*																			*
****************************************************************************/

/* The size of the ID fields, derived from the base64-encoded first 128 bits 
   of an SHA-1 hash.  The field size value is also given in text form for 
   use in SQL strings */

#define DBXKEYID_SIZE			16		/* Full keyID = 128 bits */
#define ENCODED_DBXKEYID_SIZE	22		/* base64-encoded key ID */
#define TEXT_DBXKEYID_SIZE		"22"

/* The maximum SQL query size */

#define MAX_SQL_QUERY_SIZE		256

/* When performing a query the database glue code limits the maximum returned
   data size to a certain size, the following define allows us to declare a
   fixed-size buffer that we know will always be big enough */

#define MAX_QUERY_RESULT_SIZE	MAX_ENCODED_CERT_SIZE

/* Database status flags.  These are:

	FLAG_BINARYBLOBS: Database supports binary blobs.

	FLAG_CERTSTORE/FLAG_CERTSTORE_FIELDS: Certificate stores are designated 
			by two flags, a main one for standard database/certificate store 
			differentiation and a secondary one that indicates that it's a 
			certificate store opened as a standard database, for example 
			when it's being used for read-only access with a key server.  In 
			this case it's possible to perform extended queries on fields 
			that aren't present in standard databases so we set the 
			secondary flags to indicate that extended queries are possible 
			even though certificate store functionality isn't present.

	FLAG_QUERYACTIVE: A query (returning a multiple-element result set) is
			currently in progress.

	FLAG_UPDATEACTIVE: An update is currently in progress.  This is required 
			because we can sometimes run into a situation where an update 
			falls through to an abort without ever having been begun, this 
			happens if there's a sequence of miscellaneous setup operations 
			taking place and one of them fails before we begin the update.  
			Although it'd be better if the caller handled this, in practice 
			it'd mean passing extra status information (failed vs.failed but 
			need to abort a commenced update) across a number of different 
			functions, to avoid this we record whether an update has begun 
			and if not skip an abort operation if there's no update currently 
			in progress */

#define DBMS_FLAG_NONE			0x00	/* No DBMS flag */
#define DBMS_FLAG_BINARYBLOBS	0x01	/* DBMS supports blobs */
#define DBMS_FLAG_UPDATEACTIVE	0x02	/* Ongoing update in progress */
#define DBMS_FLAG_QUERYACTIVE	0x04	/* Ongoing query in progress */
#define DBMS_FLAG_CERTSTORE		0x08	/* Full certificate store */
#define DBMS_FLAG_CERTSTORE_FIELDS 0x10	/* Certificate store fields */
#define DBMS_FLAG_MAX			0x1F	/* Maximum possible flag value */

/* Database feature information returned when the keyset is opened */

#define DBMS_FEATURE_FLAG_NONE	0x00	/* No DBMS features */
#define DBMS_FEATURE_FLAG_BINARYBLOBS 0x01 /* DBMS supports binary blobs */
#define DBMS_FEATURE_FLAG_READONLY 0x02	/* DBMS doesn't allow write access */
#define DBMS_FEATURE_FLAG_PRIVILEGES 0x04 /* DBMS supports GRANT/REVOKE */
#define DBMS_FEATURE_FLAG_MAX	0x07	/* Maximum possible flag value */

/* The certstore and binary blobs flags are checked often enough that we 
   define a macro for them */

#define hasBinaryBlobs( dbmsInfo ) \
		( ( dbmsInfo )->flags & DBMS_FLAG_BINARYBLOBS )
#define isCertStore( dbmsInfo ) \
		( ( dbmsInfo )->flags & DBMS_FLAG_CERTSTORE )

/* When we perform a DBMS transaction there are several variations on the 
   basic operation type.  The following values tell performQuery() and
   performUpdate() which type of operation to perform */

typedef enum {
	DBMS_QUERY_NONE,				/* No DBMS query */
	DBMS_QUERY_NORMAL,				/* Standard data fetch */
	DBMS_QUERY_CHECK,				/* Check-type fetch, don't fetch data */
	DBMS_QUERY_START,				/* Begin an ongoing query */
	DBMS_QUERY_CONTINUE,			/* Continue an ongoing query */
	DBMS_QUERY_CANCEL,				/* Cancel ongoing query */
	DBMS_QUERY_LAST					/* Last valid DBMS query type */
	} DBMS_QUERY_TYPE;

typedef enum {
	DBMS_UPDATE_NONE,				/* No DBMS update */
	DBMS_UPDATE_NORMAL,				/* Standard update */
	DBMS_UPDATE_BEGIN,				/* Begin a transaction */
	DBMS_UPDATE_CONTINUE,			/* Continue an ongoing transaction */
	DBMS_UPDATE_COMMIT,				/* Commit a transaction */
	DBMS_UPDATE_ABORT,				/* Abort a transaction */
	DBMS_UPDATE_LAST				/* Last valid DBMS update type */
	} DBMS_UPDATE_TYPE;

/* To optimise database accesses we use prepared queries that are prepared
   once and then re-used whenever a new result set is required.  The following
   values are used to refer to the prepared query types */

typedef enum {
	DBMS_CACHEDQUERY_NONE,			/* No cached query */
	DBMS_CACHEDQUERY_CERTID,		/* Query on certificate ID */
	DBMS_CACHEDQUERY_ISSUERID,		/* Query on issuer ID */
	DBMS_CACHEDQUERY_NAMEID,		/* Query in name ID */
	DBMS_CACHEDQUERY_URI,			/* Query on URI */
	DBMS_CACHEDQUERY_LAST			/* Last valid cached query type */
	} DBMS_CACHEDQUERY_TYPE;

#define NO_CACHED_QUERIES	5

/* When we add or read information to/from a table we sometimes have to
   specify type information which is an integer value, however SQL requires
   that things be set out as character strings so we use the following
   defines to provide the string form of the value for insertion into an SQL
   query.  Unfortunately we can't check this at compile time so we have to
   check it via an assertion in the CA dispatch function */

#define TEXT_CERTTYPE_REQUEST_CERT			"5"
#define TEXT_CERTTYPE_REQUEST_REVOCATION	"6"

#define TEXT_CERTACTION_CREATE				"1"
#define TEXTCH_CERTACTION_ADDUSER			'5'
#define TEXT_CERTACTION_REQUEST_CERT		"7"
#define TEXTCH_CERTACTION_REQUEST_CERT		'7'
#define TEXT_CERTACTION_REQUEST_RENEWAL		"8"
#define TEXTCH_CERTACTION_REQUEST_RENEWAL	'8'
#define TEXT_CERTACTION_CERT_CREATION		"10"

/* Special escape strings used in database keys to indicate that the value is
   physically but not logically present.  This is used to handle (currently-)
   incomplete certificate issues and similar events where intermediate state 
   information has to be stored in the database but the object in question 
   isn't ready for use yet */

#define KEYID_ESC1				"--"
#define KEYID_ESC2				"##"
#define KEYID_ESC_SIZE			2

/* The ways in which we can add a certificate object to a table.  Normally 
   we just add the certificate as is, however if we're awaiting confirmation 
   from a user before we can complete the certificate issue process we 
   perform a partial add that marks the certificate as not quite ready for 
   use yet.  A variant of this is when we're renewing a certificate (i.e. 
   re-issuing it with the same key, which is really bad but required by some 
   certificate mismanagement protocols) in which case we have to process the 
   update as a multi-stage process because we're replacing an existing 
   certificate with one which is exactly the same as far as the uniqueness 
   constraints on the certificate store are concerned */

typedef enum {
	CERTADD_NONE,				/* No certificate-add operation */
	CERTADD_NORMAL,				/* Standard one-step add */
	CERTADD_PARTIAL,			/* Partial add */
	CERTADD_PARTIAL_RENEWAL,	/* Partial add with certificate replacement to follow */
	CERTADD_RENEWAL_COMPLETE,	/* Completion of renewal */
	CERTADD_LAST				/* Last valid certificate-add type */
	} CERTADD_TYPE;

/* In order to make reporting of parameter errors in the multi-parameter 
   CA management function easier we provide symbolic defines mapping the 
   CA management-specific parameter type to its corresponding parameter 
   error type */

#define CAMGMT_ARGERROR_CAKEY		CRYPT_ARGERROR_NUM1
#define CAMGMT_ARGERROR_REQUEST		CRYPT_ARGERROR_NUM2
#define CAMGMT_ARGERROR_ACTION		CRYPT_ARGERROR_VALUE

/* To avoid SQL injection attacks and speed up performance we make extensive 
   use of bound parameters.  The following structure is used to communicate 
   these parameters to the database back-end */

typedef enum {
	BOUND_DATA_NONE,		/* No bound data type */
	BOUND_DATA_STRING,		/* Character string */
	BOUND_DATA_BLOB,		/* Binary string */
	BOUND_DATA_TIME,		/* Date/time */
	BOUND_DATA_LAST			/* Last bound data type */
	} BOUND_DATA_TYPE;

typedef struct {
	BOUND_DATA_TYPE type;	/* Type of this data item */
	BUFFER_FIXED( dataLength ) \
	const void *data;		/* Data and data length */
	int dataLength;
	} BOUND_DATA;

#define BOUND_DATA_MAXITEMS		16

/* Macros to initialise the bound-data information.  When we're binding 
   standard data values (i.e. non-blob data) the buffer to contain the value 
   is always present but if there's no data in it then the length will be 
   zero, so we perform a check for a buffer with a length of zero and set 
   the buffer pointer to NULL if this is the case */

#define initBoundData( boundData ) \
		memset( ( boundData ), 0, sizeof( BOUND_DATA ) * BOUND_DATA_MAXITEMS )
#define setBoundData( bdStorage, bdIndex, bdValue, bdValueLen ) \
		{ \
		const int bdLocalIndex = ( bdIndex ); \
		\
		( bdStorage )[ bdLocalIndex ].type = BOUND_DATA_STRING; \
		( bdStorage )[ bdLocalIndex ].data = ( bdValueLen > 0 ) ? bdValue : NULL; \
		( bdStorage )[ bdLocalIndex ].dataLength = bdValueLen; \
		}
#define setBoundDataBlob( bdStorage, bdIndex, bdValue, bdValueLen ) \
		{ \
		const int bdLocalIndex = ( bdIndex ); \
		\
		( bdStorage )[ bdLocalIndex ].type = BOUND_DATA_BLOB; \
		( bdStorage )[ bdLocalIndex ].data = bdValue; \
		( bdStorage )[ bdLocalIndex ].dataLength = bdValueLen; \
		}
#define setBoundDataDate( bdStorage, bdIndex, bdValue ) \
		{ \
		const int bdLocalIndex = ( bdIndex ); \
		\
		( bdStorage )[ bdLocalIndex ].type = BOUND_DATA_TIME; \
		( bdStorage )[ bdLocalIndex ].data = bdValue; \
		( bdStorage )[ bdLocalIndex ].dataLength = sizeof( time_t ); \
		}

/****************************************************************************
*																			*
*							Database Keyset Structures						*
*																			*
****************************************************************************/

/* A structure to parse the database access information into so that it can 
   be used by backend-specific connect functions */

typedef struct {
	BUFFER( CRYPT_MAX_TEXTSIZE, userLen ) \
	char userBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER_OPT( CRYPT_MAX_TEXTSIZE, userLen ) \
	char *user;
	BUFFER( CRYPT_MAX_TEXTSIZE, passwordLen ) \
	char passwordBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER_OPT( CRYPT_MAX_TEXTSIZE, passwordLen ) \
	char *password;
	BUFFER( CRYPT_MAX_TEXTSIZE, serverLen ) \
	char serverBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER_OPT( CRYPT_MAX_TEXTSIZE, serverLen ) \
	char *server;
	BUFFER( CRYPT_MAX_TEXTSIZE, nameLen ) \
	char nameBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER_OPT( CRYPT_MAX_TEXTSIZE, nameLen ) \
	char *name;
	int userLen, passwordLen, serverLen, nameLen;
	} DBMS_NAME_INFO;

/* Database state information maintained by the database back-end specific 
   code */

typedef struct {
	/* DBMS status information */
	BOOLEAN needsUpdate;			/* Whether data remains to be committed */
	BOOLEAN hasBinaryBlobs;			/* Whether back-end supports binary blobs */

	/* Error information returned by the back-end.  This is copied over to 
	   the keyset object as required */
	ERROR_INFO errorInfo;

	/* Database-specific information */
  #ifdef USE_ODBC
	/* ODBC access information */
	SQLHENV hEnv;					/* Environment handle */
	SQLHDBC hDbc;					/* Connection handle */
	BOOLEAN connectionOpen;			/* Whether hDbc connection is active */
	ARRAY_FIXED( NO_CACHED_QUERIES ) \
	SQLHSTMT hStmt[ NO_CACHED_QUERIES + 8 ];/* Statement handles */
	ARRAY_FIXED( NO_CACHED_QUERIES ) \
	BOOLEAN hStmtPrepared[ NO_CACHED_QUERIES + 8 ];/* Whether stmt is prepared on handle */
	BOOLEAN transactIsDestructive;	/* Whether commit/rollback destroys prep'd queries */
	SQLSMALLINT blobType;			/* SQL type of blob data type */
	BUFFER( CRYPT_MAX_TEXTSIZE, blobNameLength ) \
	char blobName[ CRYPT_MAX_TEXTSIZE + 8 ];/* Name of blob data type */
	int blobNameLength;				/* Length of blob data type name */
	SQLINTEGER dateTimeNameColSize;	/* Back-end specific size of datetime column */
	BUFFER( CRYPT_MAX_TEXTSIZE, dateTimeNameLength ) \
	char dateTimeName[ CRYPT_MAX_TEXTSIZE + 8 ];/* Name of datetime data type */
	int dateTimeNameLength;			/* Length of datetime data type name */
	BOOLEAN needLongLength;			/* Back-end needs blob length at bind.time */
	int backendType;				/* Back-end type if special handling is req'd */
  #endif /* USE_ODBC */
  #ifdef USE_DATABASE
	#error Need to add database backend-specific state variables
  #endif /* USE_DATABASE */
  #ifdef USE_TCP
	STREAM stream;					/* Network I/O stream */
  #endif /* USE_TCP */
	} DBMS_STATE_INFO;

/****************************************************************************
*																			*
*								DBMS Access Macros							*
*																			*
****************************************************************************/

/* Macros to make use of the DBMS access functions less painful.  These 
   assume the existence of a variable 'dbmsInfo' that contains DBMS access
   state information */

#define dbmsOpen( name, nameLen, options, featureFlags ) \
		dbmsInfo->openDatabaseFunction( dbmsInfo, name, nameLen, options, featureFlags )
#define dbmsClose() \
		dbmsInfo->closeDatabaseFunction( dbmsInfo )
#define dbmsStaticUpdate( command ) \
		dbmsInfo->performStaticUpdateFunction( dbmsInfo, command )
#define dbmsUpdate( command, updateBoundData, updateType ) \
		dbmsInfo->performUpdateFunction( dbmsInfo, command, updateBoundData, \
										 updateType )
#define dbmsStaticQuery( command, queryEntry, queryType ) \
		dbmsInfo->performStaticQueryFunction( dbmsInfo, command, queryEntry, \
											  queryType )
#define dbmsQuery( command, data, dataMaxLength, dataLength, queryBoundData, queryEntry, queryType ) \
		dbmsInfo->performQueryFunction( dbmsInfo, command, data, dataMaxLength, \
										dataLength, queryBoundData, queryEntry, \
										queryType )

#ifdef USE_RPCAPI
int cmdClose( void *stateInfo, COMMAND_INFO *cmd );
int cmdGetErrorInfo( void *stateInfo, COMMAND_INFO *cmd );
int cmdOpen( void *stateInfo, COMMAND_INFO *cmd );
int cmdQuery( void *stateInfo, COMMAND_INFO *cmd );
int cmdUpdate( void *stateInfo, COMMAND_INFO *cmd );
#endif /* USE_RPCAPI */

/* In order to provide access to the backend-specific error information for
   the cryptlib error message-formatting routines we need to dig down into 
   the DBMS state structure to extract the error information */

#define getDbmsErrorInfo( dbmsInfo ) \
		( &( ( ( DBMS_STATE_INFO * ) ( dbmsInfo )->stateInfo )->errorInfo ) )

/* Other non-macro functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int dbmsFormatQuery( OUT_BUFFER( outMaxLength, *outLength ) char *output, 
					 IN_LENGTH_SHORT const int outMaxLength, 
					 OUT_LENGTH_BOUNDED_Z( outMaxLength ) int *outLength,
					 IN_BUFFER( inputLength ) const char *input, 
					 IN_LENGTH_SHORT const int inputLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int dbmsParseName( OUT DBMS_NAME_INFO *nameInfo, 
				   IN_BUFFER( nameLen ) const char *name, 
				   IN_LENGTH_NAME const int nameLen );

/****************************************************************************
*																			*
*								DBMS Functions								*
*																			*
****************************************************************************/

/* Forward declaration for argument to function pointers.  These structs are
   declared in keyset.h, but that's included after dbms.h since the structs
   in keyset.h depend on values in dbms.h */

struct DI;
struct KI;

/* Prototypes for interface routines in DBMS modules */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDbxSession( INOUT struct KI *keysetInfoPtr, 
					IN_ENUM( CRYPT_KEYSET ) const CRYPT_KEYSET_TYPE type );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int endDbxSession( INOUT struct KI *keysetInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDBMSread( INOUT struct KI *keysetInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDBMSwrite( INOUT struct KI *keysetInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDBMSCA( INOUT struct KI *keysetInfoPtr );

/* Prototypes for functions in dbx_rd.c/dbx_wr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 9 ) ) \
int getItemData( INOUT struct DI *dbmsInfo, 
				 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
				 OUT_OPT int *stateInfo, 
				 IN_ENUM_OPT( KEYMGMT_ITEM ) const KEYMGMT_ITEM_TYPE itemType, 
				 IN_ENUM_OPT( CRYPT_KEYID ) const CRYPT_KEYID_TYPE keyIDtype, 
				 IN_BUFFER_OPT( keyValueLength ) const char *keyValue, 
				 IN_LENGTH_KEYID_Z const int keyValueLength,
				 IN_FLAGS_Z( KEYMGMT ) const int options,
				 INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
int addCert( INOUT struct DI *dbmsInfo, 
			 IN_HANDLE const CRYPT_HANDLE iCryptHandle,
			 IN_ENUM( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE certType, 
			 IN_ENUM( CERTADD ) const CERTADD_TYPE addType,
			 IN_ENUM( DBMS_UPDATE ) const DBMS_UPDATE_TYPE updateType, 
			 INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
int addCRL( INOUT struct DI *dbmsInfo, 
			IN_HANDLE const CRYPT_CERTIFICATE iCryptCRL,
			IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptRevokeCert,
			IN_ENUM( DBMS_UPDATE ) const DBMS_UPDATE_TYPE updateType, 
			INOUT ERROR_INFO *errorInfo );

/* Prototypes for routines in dbx_misc.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int makeKeyID( OUT_BUFFER( keyIdMaxLen, *keyIdLen ) char *keyID, 
			   IN_LENGTH_SHORT_MIN( 16 ) const int keyIdMaxLen,
			   OUT_LENGTH_BOUNDED_Z( keyIdMaxLen ) int *keyIdLen,
			   IN_KEYID const CRYPT_KEYID_TYPE iDtype,
			   IN_BUFFER( idValueLength ) const void *idValue,
			   IN_LENGTH_SHORT const int idValueLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getKeyID( OUT_BUFFER( keyIdMaxLen, *keyIdLen ) char *keyID, 
			  IN_LENGTH_SHORT_MIN( 16 ) const int keyIdMaxLen, 
			  OUT_LENGTH_BOUNDED_Z( keyIdMaxLen ) int *keyIdLen,
			  IN_HANDLE const CRYPT_HANDLE cryptHandle,
			  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE keyIDtype );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getCertKeyID( OUT_BUFFER( keyIdMaxLen, *keyIdLen ) char *keyID, 
				  IN_LENGTH_SHORT_MIN( 16 ) const int keyIdMaxLen, 
				  OUT_LENGTH_BOUNDED_Z( keyIdMaxLen ) int *keyIdLen,
				  IN_HANDLE const CRYPT_CERTIFICATE iCryptCert );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getPkiUserKeyID( OUT_BUFFER( keyIdMaxLen, *keyIdLen ) char *keyID, 
					 IN_LENGTH_SHORT_MIN( 16 ) const int keyIdMaxLen, 
					 OUT_LENGTH_BOUNDED_Z( keyIdMaxLen ) int *keyIdLen,
					 IN_HANDLE const CRYPT_CERTIFICATE iCryptCert );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5 ) ) \
int extractCertData( IN_HANDLE const CRYPT_CERTIFICATE iCryptCert, 
					 IN_INT const int formatType,
					 OUT_BUFFER( certDataMaxLength, *certDataLength ) \
						void *certDataBuffer, 
					 IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
						const int certDataMaxLength, 
					 OUT_LENGTH_BOUNDED_Z( certDataMaxLength ) \
						int *certDataLength );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int resetErrorInfo( INOUT struct DI *dbmsInfo );
CHECK_RETVAL_PTR \
char *getKeyName( IN_ENUM( CRYPT_KEYID ) const CRYPT_KEYID_TYPE keyIDtype );

/* Prototypes for routines in ca_add.c */

CHECK_RETVAL_BOOL \
BOOLEAN checkRequest( IN_HANDLE const CRYPT_CERTIFICATE iCertRequest,
					  IN_ENUM_OPT( CRYPT_CERTACTION ) \
						const CRYPT_CERTACTION_TYPE action );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
int caAddCertRequest( INOUT struct DI *dbmsInfo, 
					  IN_HANDLE const CRYPT_CERTIFICATE iCertRequest,
					  IN_ENUM( CRYPT_CERTTYPE ) \
						const CRYPT_CERTTYPE_TYPE requestType, 
					  const BOOLEAN isRenewal, const BOOLEAN isInitialOp,
					  INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int caAddPKIUser( INOUT struct DI *dbmsInfo, 
				  IN_HANDLE const CRYPT_CERTIFICATE iPkiUser,
				  INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int caDeletePKIUser( INOUT struct DI *dbmsInfo, 
					 IN_ENUM( CRYPT_KEYID ) const CRYPT_KEYID_TYPE keyIDtype,
					 IN_BUFFER( keyIDlength ) const void *keyID, 
					 IN_LENGTH_KEYID const int keyIDlength,
					 INOUT ERROR_INFO *errorInfo );

/* Prototypes for routines in ca_clean.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int caCleanup( INOUT struct DI *dbmsInfo, 
			   IN_ENUM( CRYPT_CERTACTION ) const CRYPT_CERTACTION_TYPE action,
			   INOUT ERROR_INFO *errorInfo );

/* Prototypes for routines in ca_issue.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int completeCertRenewal( INOUT struct DI *dbmsInfo,
						 IN_HANDLE const CRYPT_CERTIFICATE iReplaceCertificate,
						 INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
int caIssueCert( INOUT struct DI *dbmsInfo, 
				 OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
				 IN_HANDLE const CRYPT_CERTIFICATE caKey,
				 IN_HANDLE const CRYPT_CERTIFICATE iCertRequest,
				 IN_ENUM( CRYPT_CERTACTION ) const CRYPT_CERTACTION_TYPE action,
				 INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int caIssueCertComplete( INOUT struct DI *dbmsInfo, 
						 IN_HANDLE const CRYPT_CERTIFICATE iCertificate,
						 IN_ENUM( CRYPT_CERTACTION ) \
							const CRYPT_CERTACTION_TYPE action,
						 INOUT ERROR_INFO *errorInfo );

/* Prototypes for routines in ca_rev.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int revokeCertDirect( INOUT struct DI *dbmsInfo,
					  IN_HANDLE const CRYPT_CERTIFICATE iCertificate,
					  IN_ENUM( CRYPT_CERTACTION ) \
						const CRYPT_CERTACTION_TYPE action,
					  INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
int caRevokeCert( INOUT struct DI *dbmsInfo, 
				  IN_HANDLE const CRYPT_CERTIFICATE iCertRequest,
				  IN_HANDLE_OPT const CRYPT_CERTIFICATE iCertificate,
				  IN_ENUM( CRYPT_CERTACTION ) const CRYPT_CERTACTION_TYPE action,
				  INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int caIssueCRL( INOUT struct DI *dbmsInfo, 
				OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCRL,
				IN_HANDLE const CRYPT_CONTEXT caKey, 
				INOUT ERROR_INFO *errorInfo );

/* Prototypes for routines in ca_misc.c */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int updateCertLog( INOUT struct DI *dbmsInfo, 
				   IN_ENUM( CRYPT_CERTACTION ) const CRYPT_CERTACTION_TYPE action, 
				   IN_BUFFER_OPT( certIDlength ) const char *certID, 
				   IN_LENGTH_SHORT_Z const int certIDlength,
				   IN_BUFFER_OPT( reqCertIDlength ) const char *reqCertID, 
				   IN_LENGTH_SHORT_Z const int reqCertIDlength,
				   IN_BUFFER_OPT( subjCertIDlength ) const char *subjCertID, 
				   IN_LENGTH_SHORT_Z const int subjCertIDlength,
				   IN_BUFFER_OPT( dataLength ) const void *data, 
				   IN_LENGTH_SHORT_Z const int dataLength, 
				   IN_ENUM( DBMS_UPDATE ) const DBMS_UPDATE_TYPE updateType );
RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int updateCertErrorLog( INOUT struct DI *dbmsInfo, 
						IN_ERROR const int errorStatus,
						IN_STRING const char *errorString, 
						IN_BUFFER_OPT( certIDlength ) const char *certID, 
						IN_LENGTH_SHORT_Z const int certIDlength,
						IN_BUFFER_OPT( reqCertIDlength ) const char *reqCertID, 
						IN_LENGTH_SHORT_Z const int reqCertIDlength,
						IN_BUFFER_OPT( subjCertIDlength ) const char *subjCertID, 
						IN_LENGTH_SHORT_Z const int subjCertIDlength,
						IN_BUFFER_OPT( dataLength ) const void *data, 
						IN_LENGTH_SHORT_Z const int dataLength );
RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int updateCertErrorLogMsg( INOUT struct DI *dbmsInfo, 
						   IN_ERROR const int errorStatus,
						   IN_STRING const char *errorString );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int caGetIssuingUser( INOUT struct DI *dbmsInfo, 
					  OUT_HANDLE_OPT CRYPT_CERTIFICATE *iPkiUser,
					  IN_BUFFER( initialCertIDlength ) const char *initialCertID, 
					  IN_LENGTH_FIXED( ENCODED_DBXKEYID_SIZE ) \
						const int initialCertIDlength, 
					  INOUT ERROR_INFO *errorInfo );

#endif /* _DBMS_DEFINED */

