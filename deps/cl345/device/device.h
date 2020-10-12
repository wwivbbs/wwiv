/****************************************************************************
*																			*
*					  cryptlib Device Interface Header File 				*
*						Copyright Peter Gutmann 1998-2015					*
*																			*
****************************************************************************/

#ifndef _DEVICE_DEFINED

#define _DEVICE_DEFINED

#if defined( INC_ALL )
  #include "dev_mech.h"
#else
  #include "mechs/dev_mech.h"
#endif /* Compiler-specific includes */

/* The maximum length of error message we can store */

#define MAX_ERRMSG_SIZE		512

/* Device information flags.  The flags are:

	ACTIVE: A session with the device is currently active and needs to be 
		shut down when the device object is destroyed.

	LOGGEDIN: Whether the user is currently logged in.  

	NEEDSLOGIN: This type of device needs a user login before it can be used, 
		set when the device is first opened.

	NEEDSCLEANUP: The device hasn't been fully initialised yet, so cleanup
		actions (for example deleting incompletely-created objects) need to 
		be performed when it's closed.  This typically happens when a 
		standard { open, configure, close } cycle is interrupted, so that it
		becomes { open, close }, with non- or partially-initialised objects
		being left from the open phase.

	REMOVABLE: Whether the device is removable.
	
	TIME: Device has onboard time source */

#define DEVICE_FLAG_NONE		0x000	/* No device flag */
#define DEVICE_FLAG_NEEDSLOGIN	0x0001	/* User must log in to use dev.*/
#define DEVICE_FLAG_READONLY	0x0002	/* Device can't be written to */
#define DEVICE_FLAG_REMOVABLE	0x0004	/* Device is removable */
#define DEVICE_FLAG_ACTIVE		0x0008	/* Device is currently active */
#define DEVICE_FLAG_LOGGEDIN	0x0010	/* User is logged into device */
#define DEVICE_FLAG_TIME		0x0020	/* Device has on-board time source */
#define DEVICE_FLAG_NEEDSCLEANUP 0x0040	/* Device partially initialised */
#define DEVICE_FLAG_MAX			0x007F	/* Maximum possible flag value */

/* Devices can be used to create further objects.  Most can only create 
   contexts, but the system object can create any kind of object */

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
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

#define NONCERNG_PRIVATE_STATESIZE		8

typedef struct {
	/* Nonce PRNG information.  This uses a nonce RNG state consisting of a
	   block of data of the same size as the nonce hash function followed
	   by 64 bits of private state that's mixed into the hashed data, see
	   the comment in getNonce() in device/system.c for details */
	BUFFER_FIXED( CRYPT_MAX_HASHSIZE + NONCERNG_PRIVATE_STATESIZE ) \
	BYTE nonceData[ ( CRYPT_MAX_HASHSIZE + \
					  NONCERNG_PRIVATE_STATESIZE ) + 8 ];/* Nonce RNG state */
	FNPTR nonceHashFunction;
	int nonceHashSize;
	BOOLEAN nonceDataInitialised;	/* Whether nonce RNG initialised */
	
	/* Checksum for the nonce data.  Note that when adding non nonce-related
	   fields to this structure the checksum calculation in getNonce() in
	   devices/system.c will need to be adjusted */
	int nonceChecksum;				/* Checksum for nonce data */
	} SYSTEMDEV_INFO;

#ifdef USE_PKCS11

typedef struct {
	/* General device information.  The labelBuffer provides storage for the
	   label data pointed to from the DEVICE_INFO */
	int minPinSize, maxPinSize;		/* Minimum, maximum PIN lengths */
	BYTE labelBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Device label */

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
#endif /* USE_PKCS11 */

#ifdef USE_CRYPTOAPI

#if defined( __WIN32__ ) && VC_GE_2005( _MSC_VER )
  #define CAPI_HANDLE ULONG_PTR		/* May be 64 bits on Win64 */
#else
  #define CAPI_HANDLE unsigned long	/* Always 32 bits on Win32 */
#endif /* Older vs.newer CryptoAPI types */

typedef struct {
	/* General device information */
	BYTE labelBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Device label */

	/* Device type-specific information */
	CAPI_HANDLE hProv;				/* CryptoAPI provider handle */
	void *hCertStore;				/* Cert store for key/cert storage */
	CAPI_HANDLE hPrivateKey;		/* Key for session key import/export */
	int privateKeySize;				/* Size of import/export key */
	const void *hCertChain;			/* Cached copy of current cert chain */

	/* Last-error information returned from lower-level code */
	ERROR_INFO errorInfo;
	} CRYPTOAPI_INFO;
#endif /* USE_CRYPTOAPI */

#ifdef USE_HARDWARE

typedef struct {
	/* General device information */
	BYTE labelBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];	/* Device label */

	/* Device type-specific information */
	CRYPT_KEYSET iCryptKeyset;			/* Key storage */

	/* Last-error information returned from lower-level code */
	ERROR_INFO errorInfo;
	} HARDWARE_INFO;
#endif /* USE_HARDWARE */

/* Defines to make access to the union fields less messy */

#define deviceSystem	deviceInfo.systemInfo
#define devicePKCS11	deviceInfo.pkcs11Info
#define deviceCryptoAPI	deviceInfo.cryptoapiInfo
#define deviceHardware	deviceInfo.hardwareInfo

/* The structure which stores information on a device */

struct DI;

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *DEV_INITFUNCTION )( INOUT struct DI *deviceInfo, 
								   IN_BUFFER_OPT( nameLength ) \
										const char *name,
								   IN_LENGTH_TEXT_Z const int nameLength );
typedef STDC_NONNULL_ARG( ( 1 ) ) \
		void ( *DEV_SHUTDOWNFUNCTION )( INOUT struct DI *deviceInfo );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *DEV_CONTROLFUNCTION )( INOUT struct DI *deviceInfo,
									  IN_ATTRIBUTE \
										const CRYPT_ATTRIBUTE_TYPE type,
									  IN_BUFFER_OPT( dataLength ) void *data, 
									  IN_LENGTH_SHORT_Z const int dataLength,
									  INOUT_OPT \
										MESSAGE_FUNCTION_EXTINFO *messageExtInfo );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *DEV_SELFTESTFUNCTION )( INOUT struct DI *deviceInfo,
									   INOUT \
										MESSAGE_FUNCTION_EXTINFO *messageExtInfo );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
		int ( *DEV_GETITEMFUNCTION )( INOUT struct DI *deviceInfo,
									  OUT_HANDLE_OPT \
										CRYPT_CONTEXT *iCryptContext,
									  IN_ENUM( KEYMGMT_ITEM ) \
										const KEYMGMT_ITEM_TYPE itemType,
									  IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
									  IN_BUFFER( keyIDlength ) \
										const void *keyID, 
									  IN_LENGTH_KEYID const int keyIDlength,
									  IN_BUFFER_OPT( *auxInfoLength ) \
										void *auxInfo, 
									  INOUT_LENGTH_SHORT_Z int *auxInfoLength, 
									  IN_FLAGS_Z( KEYMGMT ) const int flags );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *DEV_SETITEMFUNCTION )( INOUT struct DI *deviceInfo,
									  IN_HANDLE \
										const CRYPT_HANDLE iCryptHandle );
typedef RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
		int ( *DEV_DELETEITEMFUNCTION )( INOUT struct DI *deviceInfo,
										 IN_ENUM( KEYMGMT_ITEM ) \
											const KEYMGMT_ITEM_TYPE itemType,
										 IN_KEYID \
											const CRYPT_KEYID_TYPE keyIDtype,
										 IN_BUFFER( keyIDlength ) \
											const void *keyID, 
										 IN_LENGTH_KEYID \
											const int keyIDlength );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
		int ( *DEV_GETFIRSTITEMFUNCTION )( INOUT struct DI *deviceInfo, 
										   OUT_HANDLE_OPT \
											CRYPT_CERTIFICATE *iCertificate,
										   OUT int *stateInfo, 
										   IN_KEYID \
											const CRYPT_KEYID_TYPE keyIDtype,
										   IN_BUFFER( keyIDlength ) \
											const void *keyID, 
										   IN_LENGTH_KEYID const int keyIDlength,
										   IN_ENUM( KEYMGMT_ITEM ) \
											const KEYMGMT_ITEM_TYPE itemType, 
										   IN_FLAGS_Z( KEYMGMT ) \
											const int options );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
		int ( *DEV_GETNEXTITEMFUNCTION )( INOUT struct DI *deviceInfo, 
										  OUT_HANDLE_OPT \
											CRYPT_CERTIFICATE *iCertificate,
										  INOUT int *stateInfo, 
										  IN_FLAGS_Z( KEYMGMT ) \
											const int options );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *DEV_GETRANDOMFUNCTION )( INOUT struct DI *deviceInfo, 
										OUT_BUFFER_FIXED( length ) void *buffer, 
										IN_LENGTH_SHORT const int length,
										INOUT_OPT \
											MESSAGE_FUNCTION_EXTINFO *messageExtInfo );

typedef struct DI {
	/* General device information.  Alongside various handles used to access
	   the device we also record whether the user has authenticated
	   themselves to the device since some devices have multiple user-access
	   states and the user needs to be logged out of one state before they
	   can log in to another state.  In addition we also record the device
	   label which the caller can query for use in prompts displayed to the
	   user */
	CRYPT_DEVICE_TYPE type;			/* Device type */
	SAFE_FLAGS flags;				/* Device information flags */
	BUFFER_FIXED( labelLen ) \
	char *label;					/* Device label */
	int labelLen;

	/* Each device provides various capabilities which are held in the 
	   following list.  When we need to create an object via the device, we
	   look up the requirements in the capability info and feed it to
	   createObjectFromCapability() */
	DATAPTR capabilityInfoList;

	/* Device type-specific information */
	union {
		SYSTEMDEV_INFO *systemInfo;
#ifdef USE_PKCS11
		PKCS11_INFO *pkcs11Info;
#endif /* USE_PKCS11 */
#ifdef USE_CRYPTOAPI
		CRYPTOAPI_INFO *cryptoapiInfo;
#endif /* USE_CRYPTOAPI */
#ifdef USE_HARDWARE
		HARDWARE_INFO *hardwareInfo;
#endif /* USE_HARDWARE */
		} deviceInfo;

	/* Pointers to device access methods */
	FNPTR initFunction, shutdownFunction, controlFunction;
	FNPTR selftestFunction, getItemFunction, setItemFunction;
	FNPTR deleteItemFunction, getFirstItemFunction;
	FNPTR getNextItemFunction, getRandomFunction;

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

/* Sanity-check device data */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckDevice( IN const DEVICE_INFO *deviceInfoPtr );

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
					   IN_BUFFER( nameLength ) const char *name, 
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
