/****************************************************************************
*																			*
*							User Routines Header File						*
*						 Copyright Peter Gutmann 1999-2007					*
*																			*
****************************************************************************/

#ifndef _USER_DEFINED

#define _USER_DEFINED

/* Initialisation states for the user object */

typedef enum {
	USER_STATE_NONE,				/* No initialisation state */
	USER_STATE_SOINITED,			/* SSO inited, not usable */
	USER_STATE_USERINITED,			/* User inited, usable */
	USER_STATE_LOCKED,				/* Disabled, not usable */
	USER_STATE_LAST					/* Last possible state */
	} USER_STATE_TYPE;

/* User information flags.  These are:

	FLAG_ZEROISE: Zeroise in progress, further messages (except destroy) are 
			bounced, and all files are deleted on destroy */

#define USER_FLAG_NONE			0x00	/* No flag */
#define USER_FLAG_ZEROISE		0x01	/* Zeroise in progress */

/* Content disposition when writing configuration data */

typedef enum {
	CONFIG_DISPOSITION_NONE,		/* No disposition type */
	CONFIG_DISPOSITION_NO_CHANGE,	/* No change in data and certificates */
	CONFIG_DISPOSITION_EMPTY_CONFIG_FILE,	/* No data/certs present, empty file */
	CONFIG_DISPOSITION_TRUSTED_CERTS_ONLY,	/* Only trusted certs present */
	CONFIG_DISPOSITION_DATA_ONLY,	/* Only configuratin data present */
	CONFIG_DISPOSITION_BOTH,		/* Both data and certificates present */
	CONFIG_DISPOSITION_LAST			/* Last possible disposition type */
	} CONFIG_DISPOSITION_TYPE;

/****************************************************************************
*																			*
*								Data Structures								*
*																			*
****************************************************************************/

/* User information as stored in the user info file */

typedef struct {
	CRYPT_USER_TYPE type;			/* User type */
	USER_STATE_TYPE state;			/* User state */
	BUFFER( CRYPT_MAX_TEXTSIZE, userNameLength ) \
	BYTE userName[ CRYPT_MAX_TEXTSIZE + 8 ];
	int userNameLength;				/* User name */
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE userID[ KEYID_SIZE + 8 ];
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE creatorID[ KEYID_SIZE + 8 ];/* ID of user and creator of this user */
	int fileRef;					/* User info file reference */
	} USER_FILE_INFO;

/* The structure that stores the information on a user */

typedef struct UI {
	/* Control and status information */
	int flags;						/* User flags */
	USER_FILE_INFO userFileInfo;	/* General user info */

	/* User index information for the default user */
	void *userIndexPtr;

	/* Configuration options for this user.  These are managed through the 
	   user config code, so they're just treated as a dynamically-allocated 
	   blob within the user object */
	void *configOptions;
	int configOptionsCount;

	/* Certificate trust information for this user, and a flag indicating
	   whether the trust info has changed and potentially needs to be
	   committed to disk.  This requires access to cert-internal details
	   so it's handled externally via the cert code, the user object just
	   sees the info as an opaque blob */
	void *trustInfoPtr;
	BOOLEAN trustInfoChanged;

	/* The user object contains an associated keyset which is used to store
	   user information to disk.  In addition for SOs and CAs it also 
	   contains an associated encryption context, either a private key (for 
	   an SO) or a conventional key (for a CA) */
	CRYPT_KEYSET iKeyset;			/* Keyset */
	CRYPT_CONTEXT iCryptContext;	/* Private/secret key */

	/* Error information */
	CRYPT_ATTRIBUTE_TYPE errorLocus;/* Error locus */
	CRYPT_ERRTYPE_TYPE errorType;	/* Error type */

	/* The object's handle, used when sending messages to the object when
	   only the xxx_INFO is available */
	CRYPT_HANDLE objectHandle;
	} USER_INFO;

/****************************************************************************
*																			*
*								Internal API Functions						*
*																			*
****************************************************************************/

/* User attribute handling functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getUserAttribute( INOUT USER_INFO *userInfoPtr,
					  OUT_INT_Z int *valuePtr, 
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getUserAttributeS( INOUT USER_INFO *userInfoPtr,
					   INOUT MESSAGE_DATA *msgData, 
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setUserAttribute( INOUT USER_INFO *userInfoPtr,
					  IN_INT_Z const int value, 
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setUserAttributeS( INOUT USER_INFO *userInfoPtr,
					   IN_BUFFER( dataLength ) const void *data,
					   IN_DATALENGTH const int dataLength,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteUserAttribute( INOUT USER_INFO *userInfoPtr,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );

/* Prototypes for functions in user.c */

CHECK_RETVAL_PTR_NONNULL \
const USER_FILE_INFO *getPrimarySoUserInfo( void );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN isZeroisePassword( IN_BUFFER( passwordLen ) const char *password,
						   IN_LENGTH_SHORT const int passwordLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int zeroiseUsers( INOUT USER_INFO *userInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setUserPassword( INOUT USER_INFO *userInfoPtr,
					 IN_BUFFER( passwordLength ) const char *password, 
					 IN_LENGTH_SHORT const int passwordLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initUserIndex( OUT_PTR_OPT void **userIndexPtrPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void endUserIndex( IN void *userIndexPtr );

/* Prototypes for functions in user_cfg.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initOptions( OUT_PTR_COND void **configOptionsPtr, 
				 OUT_INT_SHORT_Z int *configOptionsCount );
STDC_NONNULL_ARG( ( 1 ) ) \
void endOptions( IN_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
					void *configOptions, 
				 IN_INT_SHORT const int configOptionsCount );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setOption( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
					void *configOptions, 
			   IN_INT_SHORT const int configOptionsCount, 
			   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option,
			   IN_INT const int value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setOptionSpecial( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
							void *configOptions, 
					  IN_INT_SHORT const int configOptionsCount, 
					  IN_RANGE_FIXED( CRYPT_OPTION_SELFTESTOK ) \
							const CRYPT_ATTRIBUTE_TYPE option,
					  IN_INT_Z const int value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int setOptionString( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
						void *configOptions, 
					 IN_INT_SHORT const int configOptionsCount, 
					 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option,
					 IN_BUFFER( valueLength ) const char *value, 
					 IN_LENGTH_SHORT const int valueLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int getOption( IN_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
					const void *configOptions, 
			   IN_INT_SHORT const int configOptionsCount, 
			   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option,
			   OUT_INT_Z int *value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5 ) ) \
int getOptionString( IN_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
						const void *configOptions,
					 IN_INT_SHORT const int configOptionsCount, 
					 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option,
					 OUT_PTR_COND const void **strPtrPtr, 
					 OUT_LENGTH_SHORT_Z int *strLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteOption( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
						void *configOptions, 
				  IN_INT_SHORT const int configOptionsCount, 
				  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option );

/* Prototypes for functions in user_rw.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int readConfig( IN_HANDLE const CRYPT_USER iCryptUser, 
				IN_STRING const char *fileName, 
				INOUT_OPT void *trustInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int getConfigDisposition( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
							void *configOptions, 
						  IN_INT_SHORT const int configOptionsCount, 	
						  const void *trustInfoPtr, 
						  OUT_ENUM( CONFIG_DISPOSITION ) \
							CONFIG_DISPOSITION_TYPE *disposition );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int prepareConfigData( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
							void *configOptions, 
					   IN_INT_SHORT const int configOptionsCount, 	
					   OUT_BUFFER_ALLOC_OPT( *dataLength ) void **dataPtrPtr, 
					   OUT_DATALENGTH_Z int *dataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int commitConfigData( IN_STRING const char *fileName,
					  IN_BUFFER_OPT( dataLength ) const void *data, 
					  IN_DATALENGTH_Z const int dataLength,
					  IN_HANDLE_OPT const CRYPT_USER iTrustedCertUserObject );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteConfig( IN_STRING const char *fileName );

#endif /* _USER_DEFINED */
