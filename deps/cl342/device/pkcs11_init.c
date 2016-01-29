/****************************************************************************
*																			*
*					cryptlib PKCS #11 Init/Shutdown Routines				*
*						Copyright Peter Gutmann 1998-2009					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "device.h"
  #include "pkcs11_api.h"
  #include "asn1.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "device/device.h"
  #include "device/pkcs11_api.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS11

/* The max. number of drivers we can work with and the max.number of slots
   per driver */

#define MAX_PKCS11_DRIVERS		5
#define MAX_PKCS11_SLOTS		16

/* The default slot to look for tokens in */

#define DEFAULT_SLOT			0

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Get random data from the device */

static int getRandomFunction( DEVICE_INFO *deviceInfo, void *buffer,
							  const int length,
							  MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	CK_RV status;
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	status = C_GenerateRandom( pkcs11Info->hSession, buffer, length );
	return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
	}

/* Perform a self-test */

static int selfTestFunction( void )
	{
	/* PKCS #11 doesn't provide any explicit means of performing a self-
	   test of a capability, and having to go through the gyrations needed 
	   to do this via the PKCS #11 interface is excessively complex, so we
	   just assume that everything is OK.  In general what a device does is
	   device-specific, for example as part of FIPS 140 requirements or even
	   just as general good engineering many will perform a self-test on
	   power-up while others (typically smart cards) barely do any checking
	   at all.  The result that we return here is an implicit "whatever the
	   device does" */
	return( CRYPT_ERROR_NOTAVAIL );
	}

/****************************************************************************
*																			*
*						 		Init/Shutdown Routines						*
*																			*
****************************************************************************/

/* Whether the PKCS #11 library has been initialised or not, this is
   initialised on demand the first time it's accessed */

static BOOLEAN pkcs11Initialised = FALSE;

/* Since we can be using multiple PKCS #11 drivers, we define an array of
   them and access the appropriate one by name */

typedef struct {
	char name[ 32 + 1 + 8 ];		/* Name of device */
	INSTANCE_HANDLE hPKCS11;		/* Handle to driver */
	CK_FUNCTION_LIST_PTR functionListPtr;	/* Driver access information */
	} PKCS11_DRIVER_INFO;

static PKCS11_DRIVER_INFO pkcs11InfoTbl[ MAX_PKCS11_DRIVERS + 8 ];

/* Dynamically load and unload any necessary PKCS #11 drivers */

static int loadPKCS11driver( PKCS11_DRIVER_INFO *pkcs11Info,
							 const char *driverName )
	{
	CK_C_GetFunctionList pC_GetFunctionList;
	CK_INFO info DUMMY_INIT_STRUCT;
	CK_RV status;
#ifdef __WIN16__
	UINT errorMode;
#endif /* __WIN16__ */
	BOOLEAN isInitialised = FALSE;
	int i = 32;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_DRIVER_INFO ) ) );
	assert( isReadPtr( driverName, 4 ) );

	/* Obtain a handle to the device driver module */
#ifdef __WIN16__
	errorMode = SetErrorMode( SEM_NOOPENFILEERRORBOX );
	pkcs11Info->hPKCS11 = LoadLibrary( driverName );
	SetErrorMode( errorMode );
	if( pkcs11Info->hPKCS11 < HINSTANCE_ERROR )
		{
		pkcs11Info->hPKCS11 = NULL_HINSTANCE;
		return( CRYPT_ERROR );
		}
#else
	if( ( pkcs11Info->hPKCS11 = DynamicLoad( driverName ) ) == NULL_INSTANCE )
		return( CRYPT_ERROR );
#endif /* OS-specific dynamic load */

	/* Get the access information for the PKCS #11 library, initialise it, 
	   and get information on the device.  There are four types of PKCS #11 
	   driver around: v1, v1-like claiming to be v2, v2-like claiming to be 
	   v1, and v2.  cryptlib can in theory handle all of these, however 
	   there are some problem areas with v1 (for example v1 uses 16-bit 
	   values while v2 uses 32-bit ones, this is usually OK because data is 
	   passed around as 32-bit values with the high bits zeroed but some 
	   implementations may leave garbage in the high 16 bits that leads to 
	   all sorts of confusion).  Because of this we explicitly fail if 
	   something claims to be v1 even though it might work in practice */
	pC_GetFunctionList = ( CK_C_GetFunctionList ) \
				DynamicBind( pkcs11Info->hPKCS11, "C_GetFunctionList" );
	if( pC_GetFunctionList == NULL )
		status = CKR_GENERAL_ERROR;
	else
		{
		CK_FUNCTION_LIST_PTR functionListPtr;

		/* The following two-step initialisation is needed because PKCS #11 
		   uses a 1-byte alignment on structs, which means that if we pass
		   in the pkcs11Info member address directly we run into alignment
		   problems with 64-bit architectures */
		status = pC_GetFunctionList( &functionListPtr ) & 0xFFFF;
		if( status == CKR_OK )
			pkcs11Info->functionListPtr = functionListPtr;
		}
	if( status != CKR_OK )
		{
		/* Free the library reference and clear the information */
		DynamicUnload( pkcs11Info->hPKCS11 );
		memset( pkcs11Info, 0, sizeof( PKCS11_DRIVER_INFO ) );
		return( CRYPT_ERROR );
		}
	status = C_Initialize( NULL_PTR ) & 0xFFFF;
	if( status == CKR_ARGUMENTS_BAD && \
		strFindStr( driverName, strlen( driverName ), "softokn3.", 9 ) >= 0 )
		{
		typedef struct CK_C_INITIALIZE_ARGS {
			CK_CREATEMUTEX CreateMutex;
			CK_DESTROYMUTEX DestroyMutex;
			CK_LOCKMUTEX LockMutex;
			CK_UNLOCKMUTEX UnlockMutex;
			CK_FLAGS flags;
			CK_VOID_PTR LibraryParameters;
			CK_VOID_PTR pReserved;
			} Netscape_CK_C_INITIALIZE_ARGS;
		Netscape_CK_C_INITIALIZE_ARGS initArgs;

		/* Netscape invented their own extension to CK_C_INITIALIZE_ARGS, 
		   adding a 'LibraryParameters' string to allow the specification of 
		   free-form parameters.  If the Netscape library's C_Initialize()
		   is called then it'll return CKR_ARGUMENTS_BAD (rather than 
		   assuming sensible default values), so that we have to call it 
		   again with explicitly-specified parameters.

		   Even this doesn't work properly though, according to the Netscape 
		   docs we can set LibraryParameters to NULL, but this still results 
		   in C_Initialize() returning CKR_ARGUMENTS_BAD.  Later in the docs
		   is a requirement that the softokn3 library be passed a 
		   "parameters=" string, but it's unclear what parameters need to be 
		   set, to what values, and what effect these values may have on the
		   behaviour of existing configs for applications like Firefox.  The
		   following config lets the C_Initialize() call succeed but causes
		   odd failures later in other PKCS #11 functions */
		DEBUG_DIAG(( "Driver is the buggy Netscape/NSS one that requires "
					 "nonstandard initialisation, see the code comment for "
					 "details" ));
		assert( DEBUG_WARN );
		memset( &initArgs, 0, sizeof( Netscape_CK_C_INITIALIZE_ARGS ) );
		initArgs.LibraryParameters = \
			"configdir='.' certPrefix='' keyPrefix='' secmod='secmod.db' "
			"flags=noModDB,noCertDB,noKeyDB,optimizeSpace,readOnly";
		status = C_Initialize( &initArgs );
		}
	if( status == CKR_OK || status == CKR_CRYPTOKI_ALREADY_INITIALIZED )
		{
		isInitialised = TRUE;
		status = C_GetInfo( &info ) & 0xFFFF;
		if( status == CKR_OK && info.cryptokiVersion.major <= 1 )
			{
			/* It's v1, we can't work with it */
			status = CKR_FUNCTION_NOT_SUPPORTED;
			}
		}
	if( status != CKR_OK )
		{
		if( isInitialised )
			C_Finalize( NULL_PTR );
		DynamicUnload( pkcs11Info->hPKCS11 );
		memset( pkcs11Info, 0, sizeof( PKCS11_DRIVER_INFO ) );
		return( CRYPT_ERROR );
		}

	/* Copy out the device driver's name so that the user can access it by 
	   name.  Some vendors erroneously null-terminate the string so we check 
	   for nulls as well */
	memcpy( pkcs11Info->name, info.libraryDescription, 32 );
	while( i > 0 && ( pkcs11Info->name[ i - 1 ] == ' ' || \
					  !pkcs11Info->name[ i - 1 ] ) )
		i--;
	pkcs11Info->name[ i ] = '\0';

	return( CRYPT_OK );
	}

void deviceEndPKCS11( void )
	{
	int i;

	if( pkcs11Initialised )
		{
		for( i = 0; i < MAX_PKCS11_DRIVERS; i++ )
			{
			if( pkcs11InfoTbl[ i ].hPKCS11 != NULL_INSTANCE )
				{
				PKCS11_DRIVER_INFO *pkcs11Info = &pkcs11InfoTbl[ i ];
						/* Needed in macro-ised form of C_Finalize() */

				C_Finalize( NULL_PTR );

				DynamicUnload( pkcs11InfoTbl[ i ].hPKCS11 );
				}
			pkcs11InfoTbl[ i ].hPKCS11 = NULL_INSTANCE;
			}
		}
	pkcs11Initialised = FALSE;
	}

CHECK_RETVAL \
int deviceInitPKCS11( void )
	{
	int tblIndex = 0, optionIndex, status;

	/* If we've previously tried to initialise the drivers, don't try it 
	   again */
	if( pkcs11Initialised )
		return( CRYPT_OK );
	memset( pkcs11InfoTbl, 0, sizeof( pkcs11InfoTbl ) );

	/* Try and link in each driver specified in the config options.  Since
	   this is a general systemwide config option, we always query the built-
	   in default user object */
	for( optionIndex = 0; optionIndex < MAX_PKCS11_DRIVERS; optionIndex++ )
		{
		MESSAGE_DATA msgData;
		char deviceDriverName[ MAX_PATH_LENGTH + 1 + 8 ];

		setMessageData( &msgData, deviceDriverName, MAX_PATH_LENGTH );
		status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, 
						IMESSAGE_GETATTRIBUTE_S, &msgData, 
						optionIndex + CRYPT_OPTION_DEVICE_PKCS11_DVR01 );
		if( cryptStatusError( status ) )
			continue;
		deviceDriverName[ msgData.length ] = '\0';
		status = loadPKCS11driver( &pkcs11InfoTbl[ tblIndex ], 
								   deviceDriverName );
		if( cryptStatusOK( status ) )
			{
			tblIndex++;
			pkcs11Initialised = TRUE;
			}
		}

	/* If it's a Unix system and there were no drivers explicitly specified,
	   try with the default driver name "libpkcs11.so".  Unlike Windows,
	   where there are large numbers of PKCS #11 vendors and we have to use
	   vendor-specific names, under Unix there are very few vendors and 
	   there's usually only one device/driver in use which inevitably 
	   co-opts /usr/lib for its own use, so we can always try for a standard
	   name and location.  As a backup measure we also try for the nCipher 
	   PKCS #11 driver, which is by far the most commonly used one on Unix 
	   systems (this may sometimes be found as /usr/lib/libcknfast.so).

	   An unfortunate side-effect of this handling is that if there's more
	   than one PKCS #11 driver present and the user forgets to explicitly
	   specify it then this may load the wrong one, however the chances of
	   there being multiple drivers present on a Unix system is close to 
	   zero so it's probably better to take the more user-friendly option
	   of trying to load a default driver */
#ifdef __UNIX__
	if( !pkcs11Initialised )
		{
		status = loadPKCS11driver( &pkcs11InfoTbl[ tblIndex ], 
								   "libpkcs11.so" );
		if( cryptStatusOK( status ) )
			pkcs11Initialised = TRUE;
		}
	if( !pkcs11Initialised )
		{
		status = loadPKCS11driver( &pkcs11InfoTbl[ tblIndex ], 
								   "/opt/nfast/toolkits/pkcs11/libcknfast.so" );
		if( cryptStatusOK( status ) )
			pkcs11Initialised = TRUE;
		}
#endif /* __UNIX__ */
	
	return( pkcs11Initialised ? CRYPT_OK : CRYPT_ERROR );
	}

/****************************************************************************
*																			*
*						 	Device Capability Routines						*
*																			*
****************************************************************************/

/* The reported key size for PKCS #11 implementations is rather inconsistent,
   most are reported in bits, a number don't return a useful value, and a few
   are reported in bytes.  The following macros sort out which algorithms
   have valid key size information and which report the length in bytes */

#define keysizeValid( algo ) \
	( ( algo ) == CRYPT_ALGO_DH || ( algo ) == CRYPT_ALGO_RSA || \
	  ( algo ) == CRYPT_ALGO_DSA || ( algo ) == CRYPT_ALGO_ECDSA || \
	  ( algo ) == CRYPT_ALGO_RC4 )
#define keysizeInBytes( algo )	( FALSE )	/* No currently-used algo */

/* Templates for the various capabilities.  These contain only basic 
   information, the remaining fields are filled in when the capability is 
   set up */

static CAPABILITY_INFO FAR_BSS capabilityTemplates[] = {
	/* Encryption capabilities */
	{ CRYPT_ALGO_DES, bitsToBytes( 64 ), "DES", 3,
		MIN_KEYSIZE, bitsToBytes( 64 ), bitsToBytes( 64 ) },
	{ CRYPT_ALGO_3DES, bitsToBytes( 64 ), "3DES", 4,
		bitsToBytes( 64 + 8 ), bitsToBytes( 128 ), bitsToBytes( 192 ) },
#ifdef USE_RC4
	{ CRYPT_ALGO_RC4, bitsToBytes( 8 ), "RC4", 3,
		MIN_KEYSIZE, bitsToBytes( 128 ), 256 },
#endif /* USE_RC4 */
	{ CRYPT_ALGO_AES, bitsToBytes( 128 ), "AES", 3,
		bitsToBytes( 128 ), bitsToBytes( 128 ), bitsToBytes( 256 ) },

	/* Hash capabilities */
#ifdef USE_MD5
	{ CRYPT_ALGO_MD5, bitsToBytes( 128 ), "MD5", 3,
		bitsToBytes( 0 ), bitsToBytes( 0 ), bitsToBytes( 0 ) },
#endif /* USE_MD5 */
	{ CRYPT_ALGO_SHA1, bitsToBytes( 160 ), "SHA-1", 5,
		bitsToBytes( 0 ), bitsToBytes( 0 ), bitsToBytes( 0 ) },
	{ CRYPT_ALGO_SHA2, bitsToBytes( 256 ), "SHA-2", 5,
		bitsToBytes( 0 ), bitsToBytes( 0 ), bitsToBytes( 0 ) },

	/* MAC capabilities */
	{ CRYPT_ALGO_HMAC_SHA1, bitsToBytes( 160 ), "HMAC-SHA1", 9,
	  bitsToBytes( 64 ), bitsToBytes( 128 ), CRYPT_MAX_KEYSIZE },
	{ CRYPT_ALGO_HMAC_SHA2, bitsToBytes( 256 ), "HMAC-SHA2", 9,
	  bitsToBytes( 64 ), bitsToBytes( 128 ), CRYPT_MAX_KEYSIZE },

	/* Public-key capabilities */
#ifdef USE_DH
	{ CRYPT_ALGO_DH, bitsToBytes( 0 ), "Diffie-Hellman", 14,
		MIN_PKCSIZE, bitsToBytes( 1024 ), CRYPT_MAX_PKCSIZE },
#endif /* USE_DH */
	{ CRYPT_ALGO_RSA, bitsToBytes( 0 ), "RSA", 3,
		MIN_PKCSIZE, bitsToBytes( 1024 ), CRYPT_MAX_PKCSIZE },
#ifdef USE_DSA
	{ CRYPT_ALGO_DSA, bitsToBytes( 0 ), "DSA", 3,
		MIN_PKCSIZE, bitsToBytes( 1024 ), CRYPT_MAX_PKCSIZE },
#endif /* USE_DSA */
#ifdef USE_ECDSA
	{ CRYPT_ALGO_ECDSA, bitsToBytes( 0 ), "ECDSA", 5,
		MIN_PKCSIZE_ECC, bitsToBytes( 256 ), CRYPT_MAX_PKCSIZE_ECC },
#endif /* USE_ECDSA */

	/* Hier ist der Mast zu ende */
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};

/* Query a given capability for a device and fill out a capability 
   information record for it if present */

static CAPABILITY_INFO *getCapability( const DEVICE_INFO *deviceInfo,
									   const PKCS11_MECHANISM_INFO *mechanismInfoPtr,
									   const int maxMechanisms )
	{
	VARIABLE_CAPABILITY_INFO *capabilityInfo;
	CK_MECHANISM_INFO pMechanism;
	CK_RV status;
	const CRYPT_ALGO_TYPE cryptAlgo = mechanismInfoPtr->cryptAlgo;
	const BOOLEAN isPKC = isPkcAlgo( cryptAlgo ) ? TRUE : FALSE;
	const CK_FLAGS keyGenFlag = isPKC ? CKF_GENERATE_KEY_PAIR : CKF_GENERATE;
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;
	int hardwareOnly, i, iterationCount;

	assert( isReadPtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isReadPtr( mechanismInfoPtr, \
					   maxMechanisms * sizeof( PKCS11_MECHANISM_INFO ) ) );

	/* Set up canary values for the mechanism information in case the driver
	   blindly reports success for every mechanism that we ask for */
	memset( &pMechanism, 0, sizeof( CK_MECHANISM_INFO ) );
	pMechanism.ulMinKeySize = 0xA5A5;
	pMechanism.ulMaxKeySize = 0x5A5A;

	/* Get the information for this mechanism.  Since many PKCS #11 drivers
	   implement some of their capabilities using God knows what sort of 
	   software implementation, we provide the option to skip emulated 
	   mechanisms if required */
	status = C_GetMechanismInfo( pkcs11Info->slotID, 
								 mechanismInfoPtr->mechanism,
								 &pMechanism );
	if( status != CKR_OK )
		return( NULL );
	if( pMechanism.ulMinKeySize == 0xA5A5 && \
		pMechanism.ulMaxKeySize == 0x5A5A )
		{
		/* The driver reported that this mechanism is available but didn't
		   update the mechanism information, it's lying */
		DEBUG_DIAG(( "Driver reports that mechanism %X is available even "
					 "though it isn't", mechanismInfoPtr->mechanism ));
		assert( DEBUG_WARN );
		return( NULL );
		}
	status = krnlSendMessage( deviceInfo->ownerHandle, IMESSAGE_GETATTRIBUTE, 
							  &hardwareOnly, 
							  CRYPT_OPTION_DEVICE_PKCS11_HARDWAREONLY );
	if( cryptStatusOK( status ) && hardwareOnly && \
		!( pMechanism.flags & CKF_HW ) )
		{
		DEBUG_DIAG(( "Skipping mechanism %X, which is only available in "
					 "software emulation", mechanismInfoPtr->mechanism ));
		return( NULL );
		}
	if( mechanismInfoPtr->requiredFlags != CKF_NONE )
		{
		/* Make sure that the driver flags indicate support for the specific 
		   functionality that we require */
		if( ( mechanismInfoPtr->requiredFlags & \
			  pMechanism.flags ) != mechanismInfoPtr->requiredFlags )
			{
			DEBUG_DIAG(( "Driver reports that mechanism %X only has "
						 "capabilities %lX when we require %lX", 
						 mechanismInfoPtr->mechanism, 
						 mechanismInfoPtr->requiredFlags & pMechanism.flags,
						 mechanismInfoPtr->requiredFlags ));
//////////////////////////////////
// Kludge to allow it to be used
//////////////////////////////////
//			assert( DEBUG_WARN );
//			return( NULL );
			}
		}

	/* Copy across the template for this capability */
	if( ( capabilityInfo = clAlloc( "getCapability", \
									sizeof( CAPABILITY_INFO ) ) ) == NULL )
		return( NULL );
	for( i = 0; capabilityTemplates[ i ].cryptAlgo != cryptAlgo && \
				capabilityTemplates[ i ].cryptAlgo != CRYPT_ERROR && \
				i < FAILSAFE_ARRAYSIZE( capabilityTemplates, CAPABILITY_INFO ); 
		 i++ );
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( capabilityTemplates, CAPABILITY_INFO ) );
	ENSURES_N( capabilityTemplates[ i ].cryptAlgo != CRYPT_ERROR );
	memcpy( capabilityInfo, &capabilityTemplates[ i ],
			sizeof( CAPABILITY_INFO ) );

	/* Set up the keysize information if there's anything useful available */
	if( keysizeValid( cryptAlgo ) )
		{
		int minKeySize = ( int ) pMechanism.ulMinKeySize;
		int maxKeySize = ( int ) pMechanism.ulMaxKeySize;

		/* Adjust the key size to bytes and make sure that all values are 
		   consistent.  Some implementations report silly bounds (e.g. 1-bit 
		   RSA, "You naughty minKey" or alternatively 4Gbit RSA) so we 
		   adjust them to a sane value if necessary.  We also limit the 
		   maximum key size to match the cryptlib native maximum key size, 
		   both for consistency and because cryptlib performs buffer 
		   allocation based on the maximum native buffer size */
		if( pMechanism.ulMinKeySize < 0 || \
			pMechanism.ulMinKeySize >= 10000L )
			{
			DEBUG_DIAG(( "Driver reports invalid minimum key size %lu for "
						 "%s algorithm", pMechanism.ulMinKeySize,
						 capabilityInfo->algoName ));
			assert( DEBUG_WARN );
			minKeySize = 0;
			}
		if( pMechanism.ulMaxKeySize < 0 || \
			pMechanism.ulMaxKeySize >= 100000L )
			{
			DEBUG_DIAG(( "Driver reports invalid maximum key size %lu for "
						 "%s algorithm", pMechanism.ulMaxKeySize,
						 capabilityInfo->algoName ));
			assert( DEBUG_WARN );
			maxKeySize = 0;
			}
		if( !keysizeInBytes( cryptAlgo ) )
			{
			minKeySize = bitsToBytes( minKeySize );
			maxKeySize = bitsToBytes( maxKeySize );
			}
		if( minKeySize > capabilityInfo->minKeySize )
			capabilityInfo->minKeySize = minKeySize;
		if( capabilityInfo->keySize < capabilityInfo->minKeySize )
			capabilityInfo->keySize = capabilityInfo->minKeySize;
		capabilityInfo->maxKeySize = min( maxKeySize, 
										  capabilityInfo->maxKeySize );
		if( capabilityInfo->maxKeySize < capabilityInfo->minKeySize )
			{
			/* Serious braindamage in the driver, we'll just have to make
			   a sensible guess */
			DEBUG_DIAG(( "Driver reports maximum key size %d < minimum key "
						 "size %d for %s algorithm", 
						 capabilityInfo->maxKeySize, 
						 capabilityInfo->minKeySize, 
						 capabilityInfo->algoName ));
			assert( DEBUG_WARN );
			if( isPKC )
				{
				capabilityInfo->maxKeySize = \
					max( capabilityInfo->minKeySize, bitsToBytes( 2048 ) );
				}
			else
				capabilityInfo->maxKeySize = 16;
			}
		if( capabilityInfo->keySize > capabilityInfo->maxKeySize )
			capabilityInfo->keySize = capabilityInfo->maxKeySize;
		capabilityInfo->endFunction = genericEndFunction;
		}

	/* Set up the device-specific handlers */
	capabilityInfo->selfTestFunction = selfTestFunction;
	capabilityInfo->getInfoFunction = getDefaultInfo;
	if( !isPKC )
		capabilityInfo->initParamsFunction = initGenericParams;
	capabilityInfo->endFunction = mechanismInfoPtr->endFunction;
	capabilityInfo->initKeyFunction = mechanismInfoPtr->initKeyFunction;
	if( pMechanism.flags & keyGenFlag )
		capabilityInfo->generateKeyFunction = \
									mechanismInfoPtr->generateKeyFunction;
	if( pMechanism.flags & CKF_SIGN )
		{
		/* cryptlib treats hashing as an encrypt/decrypt operation while 
		   PKCS #11 treats it as a sign/verify operation, so we have to
		   juggle the function pointers based on the underlying algorithm
		   type */
		if( isPKC )
			capabilityInfo->signFunction = mechanismInfoPtr->signFunction;
		else
			capabilityInfo->encryptFunction = mechanismInfoPtr->encryptFunction;
		}
	if( pMechanism.flags & CKF_VERIFY )
		{
		/* See comment above */
		if( isPKC )
			capabilityInfo->sigCheckFunction = mechanismInfoPtr->sigCheckFunction;
		else
			capabilityInfo->decryptFunction = mechanismInfoPtr->decryptFunction;
		}
	if( pMechanism.flags & CKF_ENCRYPT )
		{
		/* Not all devices implement all modes, so we have to be careful to 
		   set up the pointer for the exact mode that's supported */
		switch( mechanismInfoPtr->cryptMode )
			{
			case CRYPT_MODE_CBC:
				capabilityInfo->encryptCBCFunction = mechanismInfoPtr->encryptFunction;
				break;

			case CRYPT_MODE_CFB:
				capabilityInfo->encryptCFBFunction = mechanismInfoPtr->encryptFunction;
				break;

			case CRYPT_MODE_GCM:
				capabilityInfo->encryptGCMFunction = mechanismInfoPtr->encryptFunction;
				break;

			default:	/* ECB or a PKC */
				capabilityInfo->encryptFunction = mechanismInfoPtr->encryptFunction;
				break;
			}
		}
	if( pMechanism.flags & CKF_DECRYPT )
		{
		/* Not all devices implement all modes, so we have to be careful to 
		   set up the pointer for the exact mode that's supported */
		switch( mechanismInfoPtr->cryptMode )
			{
			case CRYPT_MODE_CBC:
				capabilityInfo->decryptCBCFunction = mechanismInfoPtr->decryptFunction;
				break;

			case CRYPT_MODE_CFB:
				capabilityInfo->decryptCFBFunction = mechanismInfoPtr->decryptFunction;
				break;

			case CRYPT_MODE_GCM:
				capabilityInfo->decryptGCMFunction = mechanismInfoPtr->decryptFunction;
				break;

			default:	/* ECB or a PKC */
				capabilityInfo->decryptFunction = mechanismInfoPtr->decryptFunction;
				break;
			}
		}
	if( cryptAlgo == CRYPT_ALGO_DH && pMechanism.flags & CKF_DERIVE )
		{
		/* DH is a special-case that doesn't really have an encrypt function 
		   and where "decryption" is actually a derivation */
		capabilityInfo->encryptFunction = mechanismInfoPtr->encryptFunction;
		capabilityInfo->decryptFunction = mechanismInfoPtr->decryptFunction;
		}

	/* Keygen capabilities are generally present as separate mechanisms,
	   sometimes CKF_GENERATE/CKF_GENERATE_KEY_PAIR is set for the main 
	   mechanism and sometimes it's set for the separate one so if it isn't 
	   present in the main one we check the alternative one */
	if( !( pMechanism.flags & keyGenFlag ) && \
		( mechanismInfoPtr->keygenMechanism != CKM_NONE ) )
		{
		status = C_GetMechanismInfo( pkcs11Info->slotID, 
									 mechanismInfoPtr->keygenMechanism,
									 &pMechanism );
		if( status == CKR_OK && ( pMechanism.flags & keyGenFlag ) && \
			( !hardwareOnly || ( pMechanism.flags & CKF_HW ) ) )
			{
			/* Some tinkertoy tokens don't implement key generation in 
			   hardware but instead do it on the host PC (!!!) and load the
			   key into the token afterwards, so we have to perform another 
			   check here to make sure that they're doing things right */
			capabilityInfo->generateKeyFunction = \
									mechanismInfoPtr->generateKeyFunction;
			}
		}

	/* Record mechanism-specific parameters if required */
	if( isConvAlgo( cryptAlgo ) || isMacAlgo( cryptAlgo ) )
		{
		capabilityInfo->paramKeyType = mechanismInfoPtr->keyType;
		capabilityInfo->paramKeyGen = mechanismInfoPtr->keygenMechanism;
		capabilityInfo->paramDefaultMech = mechanismInfoPtr->defaultMechanism;
		}

	/* Some drivers report bizarre combinations of capabilities like (for 
	   RSA) sign, verify, and decrypt but not encrypt, which will fail later 
	   sanity checks.  If we run into one of these we force the capabilities 
	   to be consistent by disabling any for which only partial capabilities
	   are supported */
	if( isPkcAlgo( cryptAlgo ) )
		{
		if( capabilityInfo->decryptFunction != NULL && \
			capabilityInfo->encryptFunction == NULL )
			{
			DEBUG_DIAG(( "Driver reports decryption but not encryption "
						 "capability for %s algorithm, disabling "
						 "encryption", capabilityInfo->algoName ));
			capabilityInfo->decryptFunction = NULL;
			}
		if( capabilityInfo->signFunction != NULL && \
			capabilityInfo->sigCheckFunction == NULL )
			{
			DEBUG_DIAG(( "Driver reports signature-generation but not "
						 "signature-verification capability for %s "
						 "algorithm, disabling signing", 
						 capabilityInfo->algoName ));
//////////////////////////////////
// Kludge to allow it to be used
//////////////////////////////////
if( cryptAlgo == CRYPT_ALGO_ECDSA )
capabilityInfo->sigCheckFunction = capabilityInfo->signFunction;
else
			capabilityInfo->signFunction = NULL;
			}

		/* If we've now disabled all capabilities, we can't use this 
		   algorithm */
		if( capabilityInfo->decryptFunction == NULL && \
			capabilityInfo->signFunction == NULL )
			{
			DEBUG_DIAG(( "Use of algorithm %s disabled since no consistent "
						 "set of capabilities is available", 
						 capabilityInfo->algoName ));
			clFree( "getCapability", capabilityInfo );
			assert( DEBUG_WARN );
			return( NULL );
			}
		}

	/* If it's not a conventional encryption algo, we're done */
	if( !isConvAlgo( cryptAlgo ) )
		return( ( CAPABILITY_INFO * ) capabilityInfo );

	/* PKCS #11 handles encryption modes by defining a separate mechanism for
	   each one.  In order to enumerate all the modes available for a 
	   particular algorithm we check for each mechanism in turn and set up 
	   the appropriate function pointers if it's available */
	for( mechanismInfoPtr++, iterationCount = 0; 
		 mechanismInfoPtr->cryptAlgo == cryptAlgo && \
			iterationCount < maxMechanisms; 
		 mechanismInfoPtr++, iterationCount++ )
		{
		/* There's a different form of the existing mechanism available,
		   check whether the driver implements it */
		status = C_GetMechanismInfo( pkcs11Info->slotID, 
									 mechanismInfoPtr->mechanism,
									 &pMechanism );
		if( status != CKR_OK )
			continue;

		/* Set up the pointer for the appropriate encryption mode */
		switch( mechanismInfoPtr->cryptMode )
			{
			case CRYPT_MODE_CBC:
				if( pMechanism.flags & CKF_ENCRYPT )
					capabilityInfo->encryptCBCFunction = \
										mechanismInfoPtr->encryptFunction;
				if( pMechanism.flags & CKF_DECRYPT )
					capabilityInfo->decryptCBCFunction = \
										mechanismInfoPtr->decryptFunction;
				break;
			case CRYPT_MODE_CFB:
				if( pMechanism.flags & CKF_ENCRYPT )
					capabilityInfo->encryptCFBFunction = \
										mechanismInfoPtr->encryptFunction;
				if( pMechanism.flags & CKF_DECRYPT )
					capabilityInfo->decryptCFBFunction = \
										mechanismInfoPtr->decryptFunction;
				break;
			case CRYPT_MODE_GCM:
				if( pMechanism.flags & CKF_ENCRYPT )
					capabilityInfo->encryptGCMFunction = \
										mechanismInfoPtr->encryptFunction;
				if( pMechanism.flags & CKF_DECRYPT )
					capabilityInfo->decryptGCMFunction = \
										mechanismInfoPtr->decryptFunction;
				break;

			default:
				retIntError_Null();
			}
		}
	ENSURES_N( iterationCount < maxMechanisms );

	return( ( CAPABILITY_INFO * ) capabilityInfo );
	}

/* Set the capability information based on device capabilities.  Since
   PKCS #11 devices can have assorted capabilities (and can vary depending
   on what's plugged in), we have to build this up on the fly rather than
   using a fixed table like the built-in capabilities */

static void freeCapabilities( DEVICE_INFO *deviceInfo )
	{
	CAPABILITY_INFO_LIST *capabilityInfoListPtr = \
				( CAPABILITY_INFO_LIST * ) deviceInfo->capabilityInfoList;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	/* If the list was empty, return now */
	if( capabilityInfoListPtr == NULL )
		return;
	deviceInfo->capabilityInfoList = NULL;

	while( capabilityInfoListPtr != NULL )
		{
		CAPABILITY_INFO_LIST *listItemToFree = capabilityInfoListPtr;
		CAPABILITY_INFO *itemToFree = ( CAPABILITY_INFO * ) listItemToFree->info;

		capabilityInfoListPtr = capabilityInfoListPtr->next;
		zeroise( itemToFree, sizeof( CAPABILITY_INFO ) );
		clFree( "freeCapabilities", itemToFree );
		zeroise( listItemToFree, sizeof( CAPABILITY_INFO_LIST ) );
		clFree( "freeCapabilities", listItemToFree );
		}
	}

static int getCapabilities( DEVICE_INFO *deviceInfo,
							const PKCS11_MECHANISM_INFO *mechanismInfoPtr, 
							const int maxMechanisms )
	{
	CAPABILITY_INFO_LIST *capabilityInfoListTail = \
				( CAPABILITY_INFO_LIST * ) deviceInfo->capabilityInfoList;
	int i;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isReadPtr( mechanismInfoPtr, \
					   maxMechanisms * sizeof( PKCS11_MECHANISM_INFO ) ) );

	static_assert( sizeof( CAPABILITY_INFO ) == sizeof( VARIABLE_CAPABILITY_INFO ),
				   "Variable capability-info-struct" );

	/* Find the end of the list to add new capabilities */
	if( capabilityInfoListTail != NULL )
		{
		while( capabilityInfoListTail->next != NULL )
			capabilityInfoListTail = capabilityInfoListTail->next;
		}

	/* Add capability information for each recognised mechanism type */
	for( i = 0; i < maxMechanisms && \
				mechanismInfoPtr[ i ].mechanism != CKM_NONE; i++ )
		{
		CAPABILITY_INFO_LIST *newCapabilityList;
		CAPABILITY_INFO *newCapability;
		const CRYPT_ALGO_TYPE cryptAlgo = mechanismInfoPtr[ i ].cryptAlgo;

		/* If the assertion below triggers then the PKCS #11 driver is 
		   broken since it's returning inconsistent information such as 
		   illegal key length data, conflicting algorithm information, etc 
		   etc.  This assertion is included here to detect buggy drivers 
		   early on rather than forcing users to step through the PKCS #11 
		   glue code to find out why an operation is failing.
		   
		   Because some tinkertoy implementations support only the bare 
		   minimum functionality (e.g.RSA private key ops and nothing else),
		   we allow asymmetric functionality for PKCs */
		newCapability = getCapability( deviceInfo, &mechanismInfoPtr[ i ],
									   maxMechanisms - i );
		if( newCapability == NULL )
			continue;
		REQUIRES( sanityCheckCapability( newCapability ) );
		if( ( newCapabilityList = \
						clAlloc( "getCapabilities", \
								 sizeof( CAPABILITY_INFO_LIST ) ) ) == NULL )
			{
			clFree( "getCapabilities", newCapability );
			continue;
			}
		newCapabilityList->info = newCapability;
		newCapabilityList->next = NULL;
		if( deviceInfo->capabilityInfoList == NULL )
			deviceInfo->capabilityInfoList = newCapabilityList;
		else
			capabilityInfoListTail->next = newCapabilityList;
		capabilityInfoListTail = newCapabilityList;

		/* Since there may be alternative mechanisms to the current one 
		   defined, we have to skip mechanisms until we find a ones for a
		   new algorithm */
		while( mechanismInfoPtr[ i + 1 ].cryptAlgo == cryptAlgo && \
			   i < maxMechanisms )
			i++;
		ENSURES( i < maxMechanisms );
		}
	ENSURES( i < maxMechanisms );

	return( ( deviceInfo->capabilityInfoList == NULL ) ? CRYPT_ERROR : CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Device Init/Shutdown/Device Control Routines			*
*																			*
****************************************************************************/

/* Close a previously-opened session with the device.  We have to have this
   before the initialisation function since it may be called by it if the 
   initialisation process fails */

static void shutdownFunction( DEVICE_INFO *deviceInfo )
	{
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	/* Log out and close the session with the device */
	if( deviceInfo->flags & DEVICE_LOGGEDIN )
		C_Logout( pkcs11Info->hSession );
	C_CloseSession( pkcs11Info->hSession );
	pkcs11Info->hSession = CK_OBJECT_NONE;
	deviceInfo->flags &= ~( DEVICE_ACTIVE | DEVICE_LOGGEDIN );

	/* Free the device capability information */
	freeCapabilities( deviceInfo );
	}

/* Open a session with the device */

static int initFunction( DEVICE_INFO *deviceInfo, const char *name,
						 const int nameLength )
	{
	CK_SESSION_HANDLE hSession;
	CK_SLOT_ID slotList[ MAX_PKCS11_SLOTS + 8 ];
	CK_ULONG slotCount = MAX_PKCS11_SLOTS;
	CK_SLOT_INFO slotInfo;
	CK_TOKEN_INFO tokenInfo;
	CK_RV status;
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;
	const PKCS11_MECHANISM_INFO *mechanismInfoPtr;
	char *labelPtr;
	int tokenSlot = DEFAULT_SLOT, i, labelLength, mechanismInfoSize;
	int cryptStatus, cryptStatus2;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isReadPtr( name, nameLength ) );

	/* Get information on all available slots */
	memset( slotList, 0, sizeof( slotList ) );
	status = C_GetSlotList( TRUE, slotList, &slotCount );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_OPEN ) );
	if( slotCount <= 0 )
		{
		/* There are token slots present but no tokens in the slots */
		return( CRYPT_ERROR_OPEN );
		}

	/* Check whether a token name (used to select the slot) has been 
	   specified */
	for( i = 1; i < nameLength - 1; i++ )
		{
		if( name[ i ] == ':' && name[ i + 1 ] == ':' )
			break;
		}
	if( i < nameLength - 1 )
		{
		const char *tokenName = name + i + 2;	/* Skip '::' */
		const int tokenNameLength = nameLength - ( i + 2 );

		if( tokenNameLength <= 0 )
			return( CRYPT_ARGERROR_STR1 );

		/* Some tokens don't implement named slots, so we also allow them to 
		   be specified using slot counts */
		if( tokenNameLength == 1 && isDigit( *tokenName ) )
			{
			tokenSlot = *tokenName - '0';
			if( tokenSlot < 0 || tokenSlot > 9 )
				return( CRYPT_ARGERROR_STR1 );
			if( tokenSlot > slotCount - 1 )	/* Slots numbered from zero */
				return( CRYPT_ERROR_NOTFOUND );
			status = C_GetTokenInfo( slotList[ tokenSlot ], &tokenInfo );
			if( status != CKR_OK )
				return( CRYPT_ERROR_NOTFOUND );
			}
		else
			{
			/* Check each (named) slot for a token matching the given name */
			for( tokenSlot = 0; tokenSlot < slotCount && \
								tokenSlot < FAILSAFE_ITERATIONS_MED; 
				 tokenSlot++ )
				{
				status = C_GetTokenInfo( slotList[ tokenSlot ], &tokenInfo );
				if( status == CKR_OK && \
					!strCompare( tokenName, tokenInfo.label, tokenNameLength ) )
					break;
				}
			ENSURES( tokenSlot < FAILSAFE_ITERATIONS_MED );
			if( tokenSlot >= slotCount )
				return( CRYPT_ERROR_NOTFOUND );
			}
		}
	pkcs11Info->slotID = slotList[ tokenSlot ];

	/* Get information on device-specific capabilities */
	status = C_GetSlotInfo( pkcs11Info->slotID, &slotInfo );
	if( status != CKR_OK )
		{
		shutdownFunction( deviceInfo );
		return( pkcs11MapError( status, CRYPT_ERROR_OPEN ) );
		}
	if( slotInfo.flags & CKF_REMOVABLE_DEVICE )
		{
		/* The device is removable */
		deviceInfo->flags |= DEVICE_REMOVABLE;
		}
	status = C_GetTokenInfo( pkcs11Info->slotID, &tokenInfo );
	if( status != CKR_OK )
		{
		shutdownFunction( deviceInfo );
		return( pkcs11MapError( status, CRYPT_ERROR_OPEN ) );
		}
	if( tokenInfo.flags & CKF_RNG )
		{
		/* The device has an onboard RNG that we can use */
		deviceInfo->getRandomFunction = getRandomFunction;
		}
#if 0	/* The Spyrus driver for pre-Lynks-II cards returns the local system 
		   time (with a GMT/localtime offset), ignoring the fact that the 
		   token has an onboard clock, so having the CKF_CLOCK_ON_TOKEN not 
		   set is accurate, although having it ignore the presence of the 
		   clock isn't very valid */
	if( !( tokenInfo.flags & CKF_CLOCK_ON_TOKEN ) && \
		( !strCompare( tokenInfo.label, "Lynks Token", 11 ) || \
		  !strCompare( tokenInfo.model, "Rosetta", 7 ) ) )
		{
		/* Fix buggy Spyrus PKCS #11 drivers which claim that the token
		   doesn't have a RTC even though it does (the Rosetta (smart card) 
		   form of the token is even worse, it returns garbage in the label 
		   and manufacturer fields, but the model field is OK).  There is a 
		   chance that there's a genuine problem with the clock (there are 
		   batches of tokens with bad clocks) but the time check that  
		   follows below will catch those */
		tokenInfo.flags |= CKF_CLOCK_ON_TOKEN;
		}
#endif /* 0 */
	if( tokenInfo.flags & CKF_CLOCK_ON_TOKEN )
		{
		const time_t theTime = getTokenTime( &tokenInfo );
		const time_t currentTime = getTime();

		/* The token claims to have an onboard clock that we can use.  Since
		   this could be arbitrarily inaccurate we compare it with the 
		   system time and only rely on it if it's within +/- 1 day of the
		   system time.
		   
		   There is a second check that we should make to catch drivers that
		   claim to read the time from the token but actually use the local
		   computer's time, but this isn't easy to do.  The most obvious way
		   is to set the system time to a bogus value and check whether this
		   matches the returned time, but this is somewhat drastic and 
		   requires superuser privs on most systems.  An alternative is to 
		   check whether the claimed token time exactly matches the system 
		   time, but this will produce false positives if (for example) the
		   token has been recently synchronised to the system time.  For now
		   all we can do is throw an exception if it appears that the token
		   time is faked */
		if( theTime > MIN_TIME_VALUE && \
			theTime >= currentTime - 86400 && \
			theTime <= currentTime + 86400 )
			deviceInfo->flags |= DEVICE_TIME;

		/* If this check is triggered then the token time may be faked since 
		   it's identical to the host system time, see the comment above for 
		   details.  We make an exception for soft-tokens (if we can detect 
		   them), which will (by definition) have the same time as the 
		   system time */
		if( !( pkcs11InfoTbl[ pkcs11Info->deviceNo ].name[ 0 ] && \
			   !strCompare( pkcs11InfoTbl[ pkcs11Info->deviceNo ].name, 
							"Software", 8 ) ) && \
			theTime == currentTime )
			{
			DEBUG_DIAG(( "PKCS #11 token time is the same as the host "
						 "system time, token time may be faked by the "
						 "driver." ));
			assert( DEBUG_WARN );
			}
		}
	if( tokenInfo.flags & CKF_WRITE_PROTECTED )
		{
		/* The device can't have data on it changed */
		deviceInfo->flags |= DEVICE_READONLY;
		}
	if( ( tokenInfo.flags & CKF_LOGIN_REQUIRED ) || \
		!( tokenInfo.flags & CKF_USER_PIN_INITIALIZED ) )
		{
		/* The user needs to log in before using various device functions.
		   We check for the absence of CKF_USER_PIN_INITIALIZED as well as 
		   the more obvious CKF_LOGIN_REQUIRED because if we've got an 
		   uninitialised device there's no PIN set so some devices will 
		   report that there's no login required (or at least none is 
		   possible).  We need to introduce some sort of pipeline stall if 
		   this is the case because otherwise the user could successfully 
		   perform some functions that don't require a login (where the 
		   exact details of what's allowed without a login are device-
		   specific) before running into mysterious failures when they get 
		   to functions that do require a login.  To avoid this, we make an 
		   uninitialised device look like a login-required device, so the 
		   user gets an invalid-PIN error if they try and proceed */
		deviceInfo->flags |= DEVICE_NEEDSLOGIN;
		}
	if( ( pkcs11Info->minPinSize = ( int ) tokenInfo.ulMinPinLen ) < 4 )
		{
		/* Some devices report silly PIN sizes */
		DEBUG_DIAG(( "Driver reports suspicious minimum PIN size %lu, "
					 "using 4", tokenInfo.ulMinPinLen ));
		pkcs11Info->minPinSize = 4;
		}
	if( ( pkcs11Info->maxPinSize = ( int ) tokenInfo.ulMaxPinLen ) < 4 )
		{
		/* Some devices report silly PIN sizes (setting this to ULONG_MAX or
		   4GB, which becomes -1 as an int, counts as silly).  Since we can't
		   differentiate between 0xFFFFFFFF = bogus value and 0xFFFFFFFF = 
		   ULONG_MAX we play it safe and set the limit to 8 bytes, which most
		   devices should be able to handle */
		DEBUG_DIAG(( "Driver reports suspicious maximum PIN size %lu, "
					 "using 8", tokenInfo.ulMinPinLen ));
		pkcs11Info->maxPinSize = 8;
		}
	labelPtr = ( char * ) tokenInfo.label;
	for( labelLength = 32;
		 labelLength > 0 && \
		 ( labelPtr[ labelLength - 1 ] == ' ' || \
		   !labelPtr[ labelLength - 1 ] ); 
		  labelLength-- );	/* Strip trailing blanks/nulls */
	while( labelLength > 0 && *labelPtr == ' ' )
		{
		/* Strip leading blanks */
		labelPtr++;
		labelLength--;
		}
	if( labelLength > 0 )
		{
		memcpy( pkcs11Info->label, labelPtr, labelLength );
		pkcs11Info->labelLen = labelLength;
		sanitiseString( pkcs11Info->label, CRYPT_MAX_TEXTSIZE, 
						labelLength );
		}
	else
		{
		/* There's no label for the token, use the device label instead */
		if( pkcs11InfoTbl[ pkcs11Info->deviceNo ].name[ 0 ] )
			{
			labelLength = \
				min( strlen( pkcs11InfoTbl[ pkcs11Info->deviceNo ].name ),
					 CRYPT_MAX_TEXTSIZE );
			memcpy( pkcs11Info->label, 
					pkcs11InfoTbl[ pkcs11Info->deviceNo ].name, labelLength );
			}
		}
	pkcs11Info->hActiveSignObject = CK_OBJECT_NONE;
	deviceInfo->label = pkcs11Info->label;
	deviceInfo->labelLen = pkcs11Info->labelLen;

	/* Open a session with the device.  This gets a bit awkward because we 
	   can't tell whether a R/W session is OK without opening a session, but 
	   we can't open a session unless we know whether a R/W session is OK, 
	   so we first try for a RW session and if that fails we go for a read-
	   only session */
	status = C_OpenSession( pkcs11Info->slotID, 
							CKF_RW_SESSION | CKF_SERIAL_SESSION, NULL_PTR, 
							NULL_PTR, &hSession );
	if( status == CKR_TOKEN_WRITE_PROTECTED )
		{
		status = C_OpenSession( pkcs11Info->slotID, 
								CKF_SERIAL_SESSION, NULL_PTR, NULL_PTR, 
								&hSession );
		}
	if( status != CKR_OK )
		{
		cryptStatus = pkcs11MapError( status, CRYPT_ERROR_OPEN );
		if( cryptStatus == CRYPT_ERROR_OPEN && \
			!( tokenInfo.flags & CKF_USER_PIN_INITIALIZED ) )
			{
			/* We couldn't do much with the error code, it could be that the
			   token hasn't been initialised yet but unfortunately PKCS #11 
			   doesn't define an error code for this condition.  In addition
			   many tokens will allow a session to be opened and then fail 
			   with a "PIN not set" error at a later point (which allows for
			   more accurate error reporting), however a small number won't
			   allow a session to be opened and return some odd-looking error
			   because there's nothing useful available.  The best way to
			   report this in a meaningful manner to the caller is to check
			   whether the user PIN has been initialised, if it hasn't then 
			   it's likely that the token as a whole hasn't been initialised 
			   so we return a not initialised error */
			cryptStatus = CRYPT_ERROR_NOTINITED;
			}
		return( cryptStatus );
		}
	ENSURES( hSession != CK_OBJECT_NONE );
	pkcs11Info->hSession = hSession;
	deviceInfo->flags |= DEVICE_ACTIVE;

	/* Set up the capability information for this device.  Since there can 
	   be devices that have one set of capabilities but not the other (e.g.
	   a smart card that only performs RSA ops), we allow one of the two
	   sets of mechanism information setups to fail, but not both */
	mechanismInfoPtr = getMechanismInfoPKC( &mechanismInfoSize );
	cryptStatus = getCapabilities( deviceInfo, mechanismInfoPtr, 
								   mechanismInfoSize );
	mechanismInfoPtr = getMechanismInfoConv( &mechanismInfoSize );
	cryptStatus2 = getCapabilities( deviceInfo, mechanismInfoPtr, 
									mechanismInfoSize );
	if( cryptStatusError( cryptStatus ) && cryptStatusError( cryptStatus2 ) )
		{
		shutdownFunction( deviceInfo );
		return( ( cryptStatus == CRYPT_ERROR ) ? \
				CRYPT_ERROR_OPEN : ( int ) cryptStatus );
		}

	return( CRYPT_OK );
	}

/* Set up the function pointers to the init/shutdown methods */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initPKCS11Init( INOUT DEVICE_INFO *deviceInfo,
					IN_BUFFER( nameLength ) const char *name, 
					IN_LENGTH_SHORT const int nameLength )
	{
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;
	int i, driverNameLength = nameLength;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isReadPtr( name, nameLength ) );

	REQUIRES( nameLength > 0 && nameLength < MAX_INTLENGTH_SHORT );

	/* Make sure that the PKCS #11 driver DLL's are loaded */
	if( !pkcs11Initialised )
		return( CRYPT_ERROR_OPEN );

	/* Check whether there's a token name appended to the driver name */
	for( i = 1; i < nameLength - 1; i++ )
		{
		if( name[ i ] == ':' && name[ i + 1 ] == ':' )
			{
			driverNameLength = i;
			break;
			}
		}

	/* If we're auto-detecting the device, use the first one that we find.  
	   There are two basic approaches to this, to keep going until we find
	   something that responds, or to try the first device and report an
	   error if it doesn't respond.  Both have their own problems, keeping
	   going will find (for example) the device in slot 2 if slot 1 is 
	   empty, but will also return a completely unexpected device if slot 1
	   contains a device that isn't responding for some reason.  Conversely,
	   only checking the first device will fail if slot 1 is empty but slot
	   2 isn't.  Users seem to prefer the obvious-fail approach, so we only
	   check the first device and fail if there's a problem.  If they
	   explicitly want a secondary slot, they can specify it by name */
	if( driverNameLength == 12 && \
		!strnicmp( "[Autodetect]", name, driverNameLength ) )
		{
		if( !pkcs11InfoTbl[ 0 ].name[ 0 ] )
			return( CRYPT_ERROR_NOTFOUND );
		pkcs11Info->deviceNo = 0;
		}
	else
		{
		/* Try and find the driver based on its name */
		for( i = 0; i < MAX_PKCS11_DRIVERS; i++ )
			{
			if( !strnicmp( pkcs11InfoTbl[ i ].name, name, driverNameLength ) )
				break;
			}
		if( i >= MAX_PKCS11_DRIVERS )
			return( CRYPT_ERROR_NOTFOUND );
		pkcs11Info->deviceNo = i;
		}

	/* Set up remaining function and access information */
	deviceInfo->initFunction = initFunction;
	deviceInfo->shutdownFunction = shutdownFunction;
	deviceInfo->devicePKCS11->functionListPtr = \
					pkcs11InfoTbl[ pkcs11Info->deviceNo ].functionListPtr;

	return( CRYPT_OK );
	}
#endif /* USE_PKCS11 */
