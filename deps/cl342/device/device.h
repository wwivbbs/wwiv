/****************************************************************************
*																			*
*					  cryptlib Device Interface Header File 				*
*						Copyright Peter Gutmann 1998-2002					*
*																			*
****************************************************************************/

#ifndef _DEVICE_DEFINED

#define _DEVICE_DEFINED

/* The maximum length of error message we can store */

#define MAX_ERRMSG_SIZE		512

/* Device information flags.  The "needs login" flag is a general device
   flag which indicates that this type of device needs a user login before
   it can be used and is set when the device is first opened, the "logged in"
   flag is an ephemeral flag which indicates whether the user is currently
   logged in.  The "device active" flag indicates that a session with the
   device is currently active and needs to be shut down when the device
   object is destroyed */

#define DEVICE_NEEDSLOGIN	0x0001	/* User must log in to use dev.*/
#define DEVICE_READONLY		0x0002	/* Device can't be written to */
#define DEVICE_REMOVABLE	0x0004	/* Device is removable */
#define DEVICE_ACTIVE		0x0008	/* Device is currently active */
#define DEVICE_LOGGEDIN		0x0010	/* User is logged into device */
#define DEVICE_TIME			0x0020	/* Device has on-board time source */

/* Devices implement mechanisms in the same way that contexts implement 
   actions.  Since the mechanism space is sparse, dispatching is handled by
   looking up the required mechanism in a table of (action, mechanism, 
   function) triples.  The table is sorted by order of most-frequently-used 
   mechanisms to speed things up, although the overhead is vanishingly small 
   anyway */

typedef CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 2 ) ) \
		int ( *MECHANISM_FUNCTION )( IN_OPT void *deviceInfoPtr,
									 INOUT void *mechanismInfo );
typedef struct {
	const MESSAGE_TYPE action;
	const MECHANISM_TYPE mechanism;
	const MECHANISM_FUNCTION function;
	} MECHANISM_FUNCTION_INFO;

/* Devices can also be used to create further objects.  Most can only create
   contexts, but the system object can create any kind of object */

typedef CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *CREATEOBJECT_FUNCTION )( INOUT \
										MESSAGE_CREATEOBJECT_INFO *objectInfo,
										const void *auxDataPtr,
										const int auxValue );
typedef struct {
	const OBJECT_TYPE type;
	const CREATEOBJECT_FUNCTION function;
	} CREATEOBJECT_FUNCTION_INFO;

/****************************************************************************
*																			*
*								Data Structures								*
*																			*
****************************************************************************/

/* The internal fields in a deviec that hold data for the various keyset
   types.   These are implemented as a union to conserve memory with some of 
   the more data-intensive types.  In addition the structures provide a 
   convenient way to group the device type-specific parameters */

typedef struct {
	/* Nonce PRNG information */
	BUFFER_FIXED( CRYPT_MAX_HASHSIZE ) \
	BYTE nonceData[ ( CRYPT_MAX_HASHSIZE + 8 ) + 8 ];/* Nonce RNG state */
	HASHFUNCTION_ATOMIC hashFunctionAtomic;		/* Nonce hash function */
	int hashSize;
	BOOLEAN nonceDataInitialised;	/* Whether nonce RNG initialised */
	} SYSTEMDEV_INFO;

typedef struct {
	/* General device information */
	int minPinSize, maxPinSize;		/* Minimum, maximum PIN lengths */
	BUFFER( CRYPT_MAX_TEXTSIZE, labelLen ) \
	char label[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Device label */
	int labelLen;

	/* Device type-specific information */
	unsigned long hSession;			/* Session handle */
	long slotID;					/* Slot ID for multi-slot device */
	int deviceNo;					/* Index into PKCS #11 token table */
	void *functionListPtr;			/* PKCS #11 driver function list pointer */
	BUFFER( CRYPT_MAX_TEXTSIZE, defaultSSOPinLen ) \
	char defaultSSOPin[ CRYPT_MAX_TEXTSIZE + 8 ];	/* SSO PIN from dev.init */
	int defaultSSOPinLen;

	/* Device state information.  This records the active object for multi-
	   part operations where we can only perform one per hSession.  Because
	   we have to set this to CRYPT_ERROR if there's none active, we make
	   it a signed long rather than the unsigned long that's usually used
	   for PKCS #11 handles */
	long hActiveSignObject;			/* Currently active sig.object */

	/* Last-error information returned from lower-level code */
	ERROR_INFO errorInfo;
	} PKCS11_INFO;

#if defined( __WIN32__ ) && VC_GE_2005( _MSC_VER )
  #define CAPI_HANDLE ULONG_PTR		/* May be 64 bits on Win64 */
#else
  #define CAPI_HANDLE unsigned long	/* Always 32 bits on Win32 */
#endif /* Older vs.newer CryptoAPI types */

typedef struct {
	/* General device information */
	BUFFER( CRYPT_MAX_TEXTSIZE, labelLen ) \
	char label[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Device label */
	int labelLen;

	/* Device type-specific information */
	CAPI_HANDLE hProv;				/* CryptoAPI provider handle */
	void *hCertStore;				/* Cert store for key/cert storage */
	CAPI_HANDLE hPrivateKey;		/* Key for session key import/export */
	int privateKeySize;				/* Size of import/export key */
	const void *hCertChain;			/* Cached copy of current cert chain */

	/* Last-error information returned from lower-level code */
	ERROR_INFO errorInfo;
	} CRYPTOAPI_INFO;

typedef struct {
	/* General device information */
	BUFFER( CRYPT_MAX_TEXTSIZE, labelLen ) \
	char label[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Device label */
	int labelLen;

	/* Device type-specific information */
	CRYPT_KEYSET iCryptKeyset;			/* Key storage */

	/* Last-error information returned from lower-level code */
	ERROR_INFO errorInfo;
	} HARDWARE_INFO;

/* Defines to make access to the union fields less messy */

#define deviceSystem	deviceInfo.systemInfo
#define devicePKCS11	deviceInfo.pkcs11Info
#define deviceCryptoAPI	deviceInfo.cryptoapiInfo
#define deviceHardware	deviceInfo.hardwareInfo

/* The structure which stores information on a device */

typedef struct DI {
	/* General device information.  Alongside various handles used to access
	   the device we also record whether the user has authenticated
	   themselves to the device since some devices have multiple user-access
	   states and the user needs to be logged out of one state before they
	   can log in to another state.  In addition we also record the device
	   label which the caller can query for use in prompts displayed to the
	   user */
	CRYPT_DEVICE_TYPE type;			/* Device type */
	int flags;						/* Device information flags */
	BUFFER_FIXED( labelLen ) \
	char *label;					/* Device label */
	int labelLen;

	/* Each device provides various capabilities which are held in the 
	   following list.  When we need to create an object via the device, we
	   look up the requirements in the capability info and feed it to
	   createObjectFromCapability() */
	const void *capabilityInfoList;

	/* Device type-specific information */
	union {
		SYSTEMDEV_INFO *systemInfo;
		PKCS11_INFO *pkcs11Info;
		CRYPTOAPI_INFO *cryptoapiInfo;
		HARDWARE_INFO *hardwareInfo;
		} deviceInfo;

	/* Pointers to device access methods */
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *initFunction )( INOUT struct DI *deviceInfo, 
						   STDC_UNUSED const char *name,
						   STDC_UNUSED const int nameLength );
	STDC_NONNULL_ARG( ( 1 ) ) \
	void ( *shutdownFunction )( INOUT struct DI *deviceInfo );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *controlFunction )( INOUT struct DI *deviceInfo,
							  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type,
							  IN_BUFFER_OPT( dataLength ) void *data, 
							  IN_LENGTH_SHORT_Z const int dataLength,
							  INOUT_OPT MESSAGE_FUNCTION_EXTINFO *messageExtInfo );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *selftestFunction )( INOUT struct DI *deviceInfo,
							   INOUT \
							   MESSAGE_FUNCTION_EXTINFO *messageExtInfo );
	int ( *getItemFunction )( INOUT struct DI *deviceInfo,
							  CRYPT_CONTEXT *iCryptContext,
							  const KEYMGMT_ITEM_TYPE itemType,
							  const CRYPT_KEYID_TYPE keyIDtype,
							  IN_BUFFER( keyIDlength ) \
							  const void *keyID, const int keyIDlength,
							  IN_BUFFER_OPT( *auxInfoLength ) \
							  void *auxInfo, int *auxInfoLength, 
							  const int flags ) \
							  STDC_NONNULL_ARG( ( 1, 2, 5 ) );
	int ( *setItemFunction )( INOUT struct DI *deviceInfo,
							  const CRYPT_HANDLE iCryptHandle ) \
							  STDC_NONNULL_ARG( ( 1 ) );
	int ( *deleteItemFunction )( INOUT struct DI *deviceInfo,
								 const KEYMGMT_ITEM_TYPE itemType,
								 const CRYPT_KEYID_TYPE keyIDtype,
								 IN_BUFFER( keyIDlength ) \
								 const void *keyID, const int keyIDlength ) \
								 STDC_NONNULL_ARG( ( 1, 4 ) );
	int ( *getFirstItemFunction )( INOUT struct DI *deviceInfo, 
								   OUT CRYPT_CERTIFICATE *iCertificate,
								   INOUT int *stateInfo, 
								   const CRYPT_KEYID_TYPE keyIDtype,
								   IN_BUFFER( keyIDlength ) \
								   const void *keyID, const int keyIDlength,
								   const KEYMGMT_ITEM_TYPE itemType, 
								   const int options ) \
								   STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) );
	int ( *getNextItemFunction )( INOUT struct DI *deviceInfo, 
								  OUT CRYPT_CERTIFICATE *iCertificate,
								  INOUT int *stateInfo, const int options ) \
								  STDC_NONNULL_ARG( ( 1, 2, 3 ) );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *getRandomFunction)( INOUT struct DI *deviceInfo, 
							   OUT_BUFFER_FIXED( length ) \
							   void *buffer, IN_LENGTH_SHORT const int length,
							   INOUT_OPT \
							   MESSAGE_FUNCTION_EXTINFO *messageExtInfo );

	/* Information for the system device */
	const MECHANISM_FUNCTION_INFO *mechanismFunctions;
	const CREATEOBJECT_FUNCTION_INFO *createObjectFunctions;
	void *randomInfo;
	int mechanismFunctionCount, createObjectFunctionCount;

	/* Error information */
	CRYPT_ATTRIBUTE_TYPE errorLocus;/* Error locus */
	CRYPT_ERRTYPE_TYPE errorType;	/* Error type */

	/* The object's handle and the handle of the user who owns this object.
	   The former is used when sending messages to the object when only the 
	   xxx_INFO is available, the latter is used to avoid having to fetch the
	   same information from the system object table */
	CRYPT_HANDLE objectHandle;
	CRYPT_USER ownerHandle;

	/* Variable-length storage for the type-specific data */
	DECLARE_VARSTRUCT_VARS;
	} DEVICE_INFO;

/****************************************************************************
*																			*
*								Internal API Functions						*
*																			*
****************************************************************************/

/* Device attribute handling functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int getDeviceAttribute( INOUT DEVICE_INFO *deviceInfoPtr,
						OUT_INT_Z int *valuePtr, 
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						INOUT MESSAGE_FUNCTION_EXTINFO *messageExtInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int getDeviceAttributeS( INOUT DEVICE_INFO *deviceInfoPtr,
						 INOUT MESSAGE_DATA *msgData, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						 MESSAGE_FUNCTION_EXTINFO *messageExtInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int setDeviceAttribute( INOUT DEVICE_INFO *deviceInfoPtr,
						IN_INT_Z const int value, 
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						MESSAGE_FUNCTION_EXTINFO *messageExtInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
int setDeviceAttributeS( INOUT DEVICE_INFO *deviceInfoPtr,
						 IN_BUFFER( dataLength ) const void *data,
						 IN_LENGTH const int dataLength,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						 MESSAGE_FUNCTION_EXTINFO *messageExtInfo );

/* Prototypes for device mapping functions */

#ifdef USE_PKCS11
  CHECK_RETVAL \
  int deviceInitPKCS11( void );
  void deviceEndPKCS11( void );
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
  int setDevicePKCS11( INOUT DEVICE_INFO *deviceInfo, 
					   IN_BUFFER( nameLength ) \
					   const char *name, 
					   IN_LENGTH_ATTRIBUTE const int nameLength );
#else
  #define deviceInitPKCS11()			CRYPT_OK
  #define deviceEndPKCS11()
  #define setDevicePKCS11( x, y, z )	CRYPT_ARGERROR_NUM1
#endif /* USE_PKCS11 */
#ifdef USE_CRYPTOAPI
  CHECK_RETVAL \
  int deviceInitCryptoAPI( void );
  void deviceEndCryptoAPI( void );
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setDeviceCryptoAPI( INOUT DEVICE_INFO *deviceInfo );
#else
  #define deviceInitCryptoAPI()			CRYPT_OK
  #define deviceEndCryptoAPI()
  #define setDeviceCryptoAPI( x )		CRYPT_ARGERROR_NUM1
#endif /* USE_CRYPTOAPI */
#ifdef USE_HARDWARE
  CHECK_RETVAL \
  int deviceInitHardware( void );
  void deviceEndHardware( void );
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setDeviceHardware( INOUT DEVICE_INFO *deviceInfo );
#else
  #define deviceInitHardware()			CRYPT_OK
  #define deviceEndHardware()
  #define setDeviceHardware( x )		CRYPT_ARGERROR_NUM1
#endif /* USE_HARDWARE */
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setDeviceSystem( INOUT DEVICE_INFO *deviceInfo );

#endif /* _DEVICE_DEFINED */
