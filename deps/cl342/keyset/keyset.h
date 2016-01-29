/****************************************************************************
*																			*
*					  cryptlib Keyset Interface Header File 				*
*						Copyright Peter Gutmann 1996-2005					*
*																			*
****************************************************************************/

#ifndef _KEYSET_DEFINED

#define _KEYSET_DEFINED

/* Various include files needed by the DBMS libraries */

#include <time.h>
#ifndef _STREAM_DEFINED
  #if defined( INC_ALL )
	#include "stream.h"
  #else
	#include "io/stream.h"
  #endif /* Compiler-specific includes */
#endif /* _STREAM_DEFINED */

/****************************************************************************
*																			*
*							Keyset Types and Constants						*
*																			*
****************************************************************************/

/* The maximum size of a certificate in binary and base64-encoded form.  For 
   the first fifteen years of use this was 2048 bytes, but given the 
   increasing tendency to shovel all manner of random junk into a 
   certificate, bringing some dangerously close to 2048 bytes (and even 
   larger in some rare cases for oddball certificates) it's now 4096 bytes.

   (Another reason for allowing the increase is that an original motivation
   for capping the value at 2048 bytes was to deal with databases that 
   didn't support BLOBs and/or didn't do VARCHARs (treating them as straight
   CHARs), but both the massively-increased storage available to databases
   since then and the high improbability of a PKI of any appreciable size 
   ever being widely deployed (beyond the existing databases of commercial
   CAs), combined with the near-universal supportof BLOBs in databases, 
   means that we don't need to be so restrictive any more) */

#define MAX_CERT_SIZE		4096
#define MAX_ENCODED_CERT_SIZE ( ( MAX_CERT_SIZE * 4 ) / 3 )

/* Keyset information flags */

#define KEYSET_OPEN			0x01	/* Keyset is open */
#define KEYSET_EMPTY		0x02	/* Keyset is empty */
#define KEYSET_DIRTY		0x04	/* Keyset data has been changed */
#define KEYSET_STREAM_OPEN	0x08	/* Underlying file stream is open */

/* The precise type of the key file that we're working with.  This is used 
   for type checking to make sure we don't try to find private keys in a
   collection of certificates or whatever */

typedef enum {
	KEYSET_SUBTYPE_NONE,			/* Unknown */
	KEYSET_SUBTYPE_PGP_PUBLIC,		/* PGP public keyring */
	KEYSET_SUBTYPE_PGP_PRIVATE,		/* PGP private keyring */
	KEYSET_SUBTYPE_PKCS12,			/* PKCS #12 key mess */
	KEYSET_SUBTYPE_PKCS15,			/* PKCS #15 keys */
	KEYSET_SUBTYPE_LAST				/* Last valid keyset subtype */
	} KEYSET_SUBTYPE;

/****************************************************************************
*																			*
*								Keyset Structures							*
*																			*
****************************************************************************/

/* The internal fields in a keyset that hold data for the various keyset
   types */

typedef enum { KEYSET_NONE, KEYSET_FILE, KEYSET_DBMS, KEYSET_LDAP,
			   KEYSET_HTTP } KEYSET_TYPE;

struct KI;	/* Forward declaration for argument to function pointers */

#ifdef _DBMS_DEFINED

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
		int ( *DBX_OPENDATABASEBACKENDFUNCTION )( INOUT DBMS_STATE_INFO *dbmsStateInfo, 
												  IN_BUFFER( nameLen ) const char *name, 
												  IN_LENGTH_NAME const int nameLen, 
												  IN_ENUM_OPT( CRYPT_KEYOPT ) \
													const CRYPT_KEYOPT_TYPE options, 
												  OUT_FLAGS_Z( DBMS_FEATURE ) \
													int *featureFlags );
typedef STDC_NONNULL_ARG( ( 1 ) ) \
		void ( *DBX_CLOSEDATABASEBACKENDFUNCTION )( INOUT DBMS_STATE_INFO *dbmsStateInfo );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *DBX_PERFORMUPDATEBACKENDFUNCTION )( INOUT DBMS_STATE_INFO *dbmsStateInfo, 
												   IN_BUFFER_OPT( commandLength ) \
													const char *command,
												   IN_LENGTH_SHORT_Z \
													const int commandLength, 
												   IN_ARRAY_OPT( BOUND_DATA_MAXITEMS ) \
													const void *boundData, 
												   IN_ENUM( DBMS_UPDATE ) \
													const DBMS_UPDATE_TYPE updateType );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *DBX_PERFORMQUERYBACKENDFUNCTION )( INOUT DBMS_STATE_INFO *dbmsStateInfo, 
												  IN_BUFFER_OPT( commandLength ) \
													const char *command,
												  IN_LENGTH_SHORT_Z \
													const int commandLength, 
												  OUT_BUFFER_OPT( dataMaxLength, \
																  *dataLength ) \
													void *data, 
												  IN_LENGTH_SHORT_Z \
													const int dataMaxLength,  
												  OUT_LENGTH_BOUNDED_Z( dataMaxLength ) \
													int *dataLength, 
												  IN_OPT const void *boundData, 
												  IN_ENUM_OPT( DBMS_CACHEDQUERY ) \
													const DBMS_CACHEDQUERY_TYPE queryEntry,
												  IN_ENUM( DBMS_QUERY ) \
													const DBMS_QUERY_TYPE queryType );

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
		int ( *DBX_OPENDATABASEFUNCTION )( INOUT struct DI *dbmsInfo, 
										   IN_BUFFER( nameLen ) const char *name, 
										   IN_LENGTH_NAME const int nameLen, 
										   IN_ENUM_OPT( CRYPT_KEYOPT ) \
											const CRYPT_KEYOPT_TYPE options, 
										   OUT_FLAGS_Z( DBMS ) \
											int *featureFlags );
typedef STDC_NONNULL_ARG( ( 1 ) ) \
		void ( *DBX_CLOSEDATABASEFUNCTION )( INOUT struct DI *dbmsInfo );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *DBX_PERFORMUPDATEFUNCTION )( INOUT struct DI *dbmsInfo, 
											IN_STRING_OPT const char *command,
											IN_OPT const void *boundData, 
											IN_ENUM( DBMS_UPDATE ) \
												const DBMS_UPDATE_TYPE updateType );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *DBX_PERFORMSTATICUPDATEFUNCTION )( INOUT struct DI *dbmsInfo, 
												  IN_STRING const char *command );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *DBX_PERFORMQUERYFUNCTION )( INOUT struct DI *dbmsInfo, 
										   IN_STRING_OPT const char *command,
										   OUT_BUFFER_OPT( dataMaxLength, \
														   *dataLength ) \
											void *data, 
										   IN_LENGTH_SHORT_Z \
											const int dataMaxLength, 
										   OUT_LENGTH_BOUNDED_Z( dataMaxLength ) \
											int *dataLength, 
										   IN_OPT const void *boundData,
										   IN_ENUM_OPT( DBMS_CACHEDQUERY ) \
											const DBMS_CACHEDQUERY_TYPE queryEntry, 
										   IN_ENUM( DBMS_QUERY ) \
											const DBMS_QUERY_TYPE queryType );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *DBX_PERFORMSTATICQUERYFUNCTION )( INOUT struct DI *dbmsInfo, 
												 IN_STRING_OPT const char *command,
												 IN_ENUM_OPT( DBMS_CACHEDQUERY ) \
													const DBMS_CACHEDQUERY_TYPE queryEntry, 
												 IN_ENUM( DBMS_QUERY ) \
													const DBMS_QUERY_TYPE queryType );
#endif /* _DBMS_DEFINED */

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *DBX_CERTMGMTFUNCTION )( INOUT struct KI *keysetInfo, 
									   OUT_OPT_HANDLE_OPT \
											CRYPT_CERTIFICATE *iCryptCert,
									   IN_HANDLE_OPT \
											const CRYPT_CERTIFICATE caKey,
									   IN_HANDLE_OPT \
											const CRYPT_CERTIFICATE request,
									   IN_ENUM( CRYPT_CERTACTION ) \
											const CRYPT_CERTACTION_TYPE action );

typedef struct {
	/* The I/O stream and file name */
	STREAM stream;					/* I/O stream for key file */
	char fileName[ MAX_PATH_LENGTH + 8 ];/* Name of key file */

	/* If this keyset is being used as a structured storage object for a 
	   hardware device then we need to record the device handle so that we
	   can associate any items retrieved from the keyset with the 
	   hardware */
	CRYPT_DEVICE iHardwareDevice;
	} FILE_INFO;

typedef struct DI {
	/* DBMS status information */
	int flags;						/* General status flags */

	/* For database types that can use binary blobs we need to bind the
	   locations of variables and use placeholders in the SQL text rather
	   than passing the data as part of the SQL command.  We can't leave this
	   on the stack since it can be referenced by the back-end an arbitrary 
	   amount of time after we initiate the update, so we copy it to the
	   following staging area before we pass control to the database 
	   back-end */
	char boundData[ MAX_ENCODED_CERT_SIZE + 8 ];

	/* The data being sent to the back-end can be communicated over a variety
	   of channels.  If we're using the RPC API, there's a single dispatch 
	   function through which all data is communicated via the RPC mechanism.  
	   If we're not using the RPC API, there are a set of function pointers,
	   one for each back-end access type.  In addition the state information 
	   storage contains the state data needed for the communications 
	   channel */
#if defined( USE_RPCAPI )
	void ( *dispatchFunction )( void *stateInfo, BYTE *buffer );
#elif defined( _DBMS_DEFINED )
	DBX_OPENDATABASEBACKENDFUNCTION openDatabaseBackend;
	DBX_CLOSEDATABASEBACKENDFUNCTION closeDatabaseBackend;
	DBX_PERFORMUPDATEBACKENDFUNCTION performUpdateBackend;
	DBX_PERFORMQUERYBACKENDFUNCTION performQueryBackend;
#else
	int ( *openDatabaseBackend )( void );
	void ( *closeDatabaseBackend )( void );
	int ( *performUpdateBackend )( void );
	int ( *performQueryBackend )( void );
#endif /* Backend function pointers */
	void *stateInfo;

	/* Database back-end access functions.  These use the dispatch function/
	   function pointers above to communicate with the back-end */
#ifdef _DBMS_DEFINED
	DBX_OPENDATABASEFUNCTION openDatabaseFunction;
	DBX_CLOSEDATABASEFUNCTION closeDatabaseFunction;
	DBX_PERFORMUPDATEFUNCTION performUpdateFunction;
	DBX_PERFORMSTATICUPDATEFUNCTION performStaticUpdateFunction;
	DBX_PERFORMQUERYFUNCTION performQueryFunction;
	DBX_PERFORMSTATICQUERYFUNCTION performStaticQueryFunction;
#else
	int ( *openDatabaseFunction )( void ); 
	void ( *closeDatabaseFunction )( void );
	int ( *performUpdateFunction )( void );
	int ( *performStaticUpdateFunction )( void ); 
	int ( *performQueryFunction )( void ); 
	int ( *performStaticQueryFunction )( void ); 
#endif /* _DBMS_DEFINED */

	/* Pointers to database-specific keyset access methods */
	DBX_CERTMGMTFUNCTION certMgmtFunction;
	} DBMS_INFO;

typedef struct {
	/* The I/O stream */
	STREAM stream;					/* I/O stream for HTTP read */

	/* An HTTP fetch differs from the other types of read in that it can
	   return data in multiple chunks depending on how much comes over the
	   net at once.  Because of this we need to track what's come in, and
	   also allocate more buffer space on demand if required.  The following
	   variables handle the on-demand re-allocation of buffer space */
	int bufPos;						/* Current position in buffer */
	} HTTP_INFO;

typedef struct {
	/* LDAP access information */
	void *ld;						/* LDAP connection information */
	void *result;					/* State information for ongoing queries */

	/* The names of the object class and various attributes.  These are
	   stored as part of the keyset context since they may be user-defined */
	char nameObjectClass[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Name of object class */
	char nameFilter[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Name of query filter */
	char nameCACert[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Name of CA certificate attribute */
	char nameCert[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Name of certificate attribute */
	char nameCRL[ CRYPT_MAX_TEXTSIZE + 8 ];		/* Name of CRL attribute */
	char nameEmail[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Name of email addr.attr.*/
	CRYPT_CERTTYPE_TYPE objectType;				/* Preferred obj.type to fetch */

	/* When storing a certificate we need the certificate DN, email address,
	   and certificate expiry date */
	char C[ CRYPT_MAX_TEXTSIZE + 8 ], SP[ CRYPT_MAX_TEXTSIZE + 8 ],
		 L[ CRYPT_MAX_TEXTSIZE + 8 ], O[ CRYPT_MAX_TEXTSIZE + 8 ],
		 OU[ CRYPT_MAX_TEXTSIZE + 8 ], CN[ CRYPT_MAX_TEXTSIZE + 8 ];
	char email[ CRYPT_MAX_TEXTSIZE + 8 ];
	time_t date;
	} LDAP_INFO;

/* Defines to make access to the union fields less messy */

#define keysetFile		keysetInfo.fileInfo
#define keysetDBMS		keysetInfo.dbmsInfo
#define keysetHTTP		keysetInfo.httpInfo
#define keysetLDAP		keysetInfo.ldapInfo

/* The structure that stores information on a keyset */

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *KEY_INITFUNCTION )( INOUT struct KI *keysetInfo, 
							   IN_BUFFER_OPT( nameLength ) const char *name, 
							   IN_LENGTH_NAME_Z const int nameLength,
							   IN_ENUM( CRYPT_KEYOPT ) \
								const CRYPT_KEYOPT_TYPE options );
typedef RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *KEY_SHUTDOWNFUNCTION )( INOUT struct KI *keysetInfo );
#ifdef USE_LDAP
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *KEY_GETATTRIBUTEFUNCTION )( INOUT struct KI *keysetInfo, 
									   OUT void *data,
									   IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE type );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *KEY_SETATTRIBUTEFUNCTION )( INOUT struct KI *keysetInfo, 
									   const void *data,
									   IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE type );
#endif /* USE_LDAP */
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
	int ( *KEY_GETITEMFUNCTION )( INOUT struct KI *keysetInfo,
								  OUT_HANDLE_OPT CRYPT_HANDLE *iCryptHandle,
								  IN_ENUM( KEYMGMT_ITEM ) \
									const KEYMGMT_ITEM_TYPE itemType,
								  IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
								  IN_BUFFER( keyIDlength ) const void *keyID, 
								  IN_LENGTH_KEYID const int keyIDlength,
								  IN_OPT void *auxInfo, 
								  INOUT_OPT int *auxInfoLength, 
								  IN_FLAGS_Z( KEYMGMT ) const int flags );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
	int ( *KEY_GETSPECIALITEMFUNCTION )( INOUT struct KI *keysetInfoPtr,
										 IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE dataType,
										 OUT_BUFFER( dataMaxLength, \
													 *dataLength ) \
											void *data,
										 IN_LENGTH_SHORT \
											const int dataMaxLength,
										 OUT_LENGTH_BOUNDED_Z( dataMaxLength ) \
											int *dataLength );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *KEY_SETITEMFUNCTION )( INOUT struct KI *deviceInfo,
								  IN_HANDLE const CRYPT_HANDLE iCryptHandle,
								  IN_ENUM( KEYMGMT_ITEM ) \
									const KEYMGMT_ITEM_TYPE itemType,
								  IN_BUFFER_OPT( passwordLength ) \
									const char *password, 
								  IN_LENGTH_NAME_Z const int passwordLength,
								  IN_FLAGS( KEYMGMT ) const int flags );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *KEY_SETSPECIALITEMFUNCTION )( INOUT struct KI *deviceInfo,
										 IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE dataType,
										 IN_BUFFER( dataLength ) const void *data, 
										 IN_LENGTH_SHORT const int dataLength );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
	int ( *KEY_DELETEITEMFUNCTION )( INOUT struct KI *keysetInfo,
									 IN_ENUM( KEYMGMT_ITEM ) \
										const KEYMGMT_ITEM_TYPE itemType,
									 IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
									 IN_BUFFER( keyIDlength ) const void *keyID, 
									 IN_LENGTH_KEYID const int keyIDlength );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 6 ) ) \
	int ( *KEY_GETFIRSTITEMFUNCTION )( INOUT struct KI *keysetInfo,
									   OUT_HANDLE_OPT \
										CRYPT_CERTIFICATE *iCertificate,
									   OUT int *stateInfo,
									   IN_ENUM( KEYMGMT_ITEM ) \
										const KEYMGMT_ITEM_TYPE itemType,
									   IN_KEYID \
										const CRYPT_KEYID_TYPE keyIDtype,
									   IN_BUFFER( keyIDlength ) \
										const void *keyID, 
									   IN_LENGTH_KEYID const int keyIDlength,
									   IN_FLAGS_Z( KEYMGMT ) \
										const int options );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
	int ( *KEY_GETNEXTITEMFUNCTION )( INOUT struct KI *keysetInfo,
									  OUT_HANDLE_OPT \
										CRYPT_CERTIFICATE *iCertificate,
									  INOUT_OPT int *stateInfo, 
										/* May be absent for an ongoing query run as 
										   { getItem, getNext, getNext, ... } */
									  IN_FLAGS_Z( KEYMGMT ) const int options );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
	BOOLEAN ( *KEY_ISBUSYFUNCTION )( INOUT struct KI *keysetInfo );

typedef struct KI {
	/* General keyset information */
	KEYSET_TYPE type;				/* Keyset type (native, PGP, X.509, etc) */
	KEYSET_SUBTYPE subType;			/* Keyset subtype (public, private, etc) */
	CRYPT_KEYOPT_TYPE options;		/* Keyset options */
	int flags;						/* Keyset information flags */

	/* Keyset type-specific information */
	union {
		FILE_INFO *fileInfo;
#ifdef USE_DBMS
		DBMS_INFO *dbmsInfo;
#endif /* USE_DBMS */
#ifdef USE_HTTP
		HTTP_INFO *httpInfo;
#endif /* USE_HTTP */
#ifdef USE_LDAP
		LDAP_INFO *ldapInfo;
#endif /* USE_LDAP */
		} keysetInfo;

	/* Pointers to keyset access methods */
	KEY_INITFUNCTION initFunction;
	KEY_SHUTDOWNFUNCTION shutdownFunction;
#ifdef USE_LDAP
	KEY_GETATTRIBUTEFUNCTION getAttributeFunction;
	KEY_SETATTRIBUTEFUNCTION setAttributeFunction;
#endif /* USE_LDAP */
	KEY_GETITEMFUNCTION getItemFunction;
	KEY_GETSPECIALITEMFUNCTION getSpecialItemFunction;
	KEY_SETITEMFUNCTION setItemFunction;
	KEY_SETSPECIALITEMFUNCTION setSpecialItemFunction;
	KEY_DELETEITEMFUNCTION deleteItemFunction;
	KEY_GETFIRSTITEMFUNCTION getFirstItemFunction;
	KEY_GETNEXTITEMFUNCTION getNextItemFunction;
	KEY_ISBUSYFUNCTION isBusyFunction;

	/* Some keysets require keyset-type-specific data storage, which is
	   managed via the following variables. keyDataSize denotes the total
	   size in bytes of the keyData buffer, keyDataNoObjects is the number
	   of objects in the buffer if it's implemented as an array of key data
	   objects */
	BUFFER_OPT_FIXED( keyDataSize ) \
	void *keyData;					/* Keyset data buffer */
	int keyDataSize;				/* Buffer size */
	int keyDataNoObjects;			/* No.of objects in key data buffer */

	/* Error information */
	CRYPT_ATTRIBUTE_TYPE errorLocus;/* Error locus */
	CRYPT_ERRTYPE_TYPE errorType;	/* Error type */

	/* Low-level error information */
	ERROR_INFO errorInfo;

	/* The object's handle and the handle of the user who owns this object.
	   The former is used when sending messages to the object when only the
	   xxx_INFO is available, the latter is used to avoid having to fetch the
	   same information from the system object table */
	CRYPT_HANDLE objectHandle;
	CRYPT_USER ownerHandle;

	/* Variable-length storage for the type-specific data */
	DECLARE_VARSTRUCT_VARS;
	} KEYSET_INFO;

/****************************************************************************
*																			*
*								Keyset Functions							*
*																			*
****************************************************************************/

/* Keyset attribute handling functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getKeysetAttribute( INOUT KEYSET_INFO *keysetInfoPtr,
						OUT_INT_Z int *valuePtr, 
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getKeysetAttributeS( INOUT KEYSET_INFO *keysetInfoPtr,
						 INOUT MESSAGE_DATA *msgData, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setKeysetAttribute( INOUT KEYSET_INFO *keysetInfoPtr,
						IN_INT_Z const int value, 
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setKeysetAttributeS( INOUT KEYSET_INFO *keysetInfoPtr,
						 IN_BUFFER( dataLength ) const void *data,
						 IN_LENGTH const int dataLength,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );

/* Prototypes for keyset mapping functions */

#ifdef USE_ODBC
  CHECK_RETVAL \
  int dbxInitODBC( void );
  void dbxEndODBC( void );
#else
  #define dbxInitODBC()						CRYPT_OK
  #define dbxEndODBC()
#endif /* USE_ODBC */
#ifdef USE_DBMS
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodDBMS( INOUT KEYSET_INFO *keysetInfo,
						   IN_ENUM( CRYPT_KEYSET ) \
							const CRYPT_KEYSET_TYPE type );
#else
  #define setAccessMethodDBMS( x, y )		CRYPT_ARGERROR_NUM1
#endif /* USE_DBMS */
#ifdef USE_HTTP
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodHTTP( INOUT KEYSET_INFO *keysetInfo );
#else
  #define setAccessMethodHTTP( x )			CRYPT_ARGERROR_NUM1
#endif /* USE_HTTP */
#ifdef USE_LDAP
  CHECK_RETVAL \
  int dbxInitLDAP( void );
  void dbxEndLDAP( void );
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodLDAP( INOUT KEYSET_INFO *keysetInfo );
#else
  #define dbxInitLDAP()						CRYPT_OK
  #define dbxEndLDAP()
  #define setAccessMethodLDAP( x )			CRYPT_ARGERROR_NUM1
#endif /* USE_LDAP */
#ifdef USE_PGPKEYS
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodPGPPublic( INOUT KEYSET_INFO *keysetInfo );
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodPGPPrivate( INOUT KEYSET_INFO *keysetInfo );
#else
  #define setAccessMethodPGPPublic( x )		CRYPT_ARGERROR_NUM1
  #define setAccessMethodPGPPrivate( x )	CRYPT_ARGERROR_NUM1
#endif /* USE_PGPKEYS */
#ifdef USE_PKCS12
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodPKCS12( INOUT KEYSET_INFO *keysetInfo );
#else
  #define setAccessMethodPKCS12( x )		CRYPT_ARGERROR_NUM1
#endif /* PKCS #12 */
#ifdef USE_PKCS15
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodPKCS15( INOUT KEYSET_INFO *keysetInfo );
#else
  #define setAccessMethodPKCS15( x )		CRYPT_ARGERROR_NUM1
#endif /* PKCS #15 */
#if defined( USE_PGPKEYS ) && defined( USE_PKCS12 )
  #define isWriteableFileKeyset( type ) \
		  ( ( type ) == KEYSET_SUBTYPE_PGP_PUBLIC || \
			( type ) == KEYSET_SUBTYPE_PKCS12 || \
			( type ) == KEYSET_SUBTYPE_PKCS15 )
#elif defined( USE_PGPKEYS ) 
  #define isWriteableFileKeyset( type ) \
		  ( ( type ) == KEYSET_SUBTYPE_PGP_PUBLIC || \
			( type ) == KEYSET_SUBTYPE_PKCS15 )
#elif defined( USE_PKCS12 )
  #define isWriteableFileKeyset( type ) \
		  ( ( type ) == KEYSET_SUBTYPE_PKCS12 || \
			( type ) == KEYSET_SUBTYPE_PKCS15 )
#else
  #define isWriteableFileKeyset( type ) \
		  ( ( type ) == KEYSET_SUBTYPE_PKCS15 )
#endif /* Writeable keyset subtypes */
#endif /* _KEYSET_DEFINED */
