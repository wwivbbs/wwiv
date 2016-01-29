/****************************************************************************
*																			*
*						cryptlib System Device Routines						*
*						Copyright Peter Gutmann 1995-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "capabil.h"
  #include "device.h"
  #include "dev_mech.h"
  #include "random.h"
#else
  #include "crypt.h"
  #include "device/capabil.h"
  #include "device/device.h"
  #include "mechs/dev_mech.h"
  #include "random/random.h"
#endif /* Compiler-specific includes */

/* Mechanisms supported by the system device.  These are sorted in order of
   frequency of use in order to make lookups a bit faster */

static const MECHANISM_FUNCTION_INFO FAR_BSS mechanismFunctions[] = {
#ifdef USE_PKC
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_PKCS1, ( MECHANISM_FUNCTION ) exportPKCS1 },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_PKCS1, ( MECHANISM_FUNCTION ) importPKCS1 },
	{ MESSAGE_DEV_SIGN, MECHANISM_SIG_PKCS1, ( MECHANISM_FUNCTION ) signPKCS1 },
	{ MESSAGE_DEV_SIGCHECK, MECHANISM_SIG_PKCS1, ( MECHANISM_FUNCTION ) sigcheckPKCS1 },
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_PKCS1_RAW, ( MECHANISM_FUNCTION ) exportPKCS1 },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_PKCS1_RAW, ( MECHANISM_FUNCTION ) importPKCS1 },
  #ifdef USE_OAEP
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_OAEP, ( MECHANISM_FUNCTION ) exportOAEP },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_OAEP, ( MECHANISM_FUNCTION ) importOAEP },
  #endif /* USE_OAEP */
#endif /* USE_PKC */
#ifdef USE_PGP
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_PKCS1_PGP, ( MECHANISM_FUNCTION ) exportPKCS1PGP },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_PKCS1_PGP, ( MECHANISM_FUNCTION ) importPKCS1PGP },
#endif /* USE_PGP */
#ifdef USE_INT_CMS
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_CMS, ( MECHANISM_FUNCTION ) exportCMS },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_CMS, ( MECHANISM_FUNCTION ) importCMS },
#endif /* USE_INT_CMS */
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PKCS5, ( MECHANISM_FUNCTION ) derivePKCS5 },
#ifdef USE_ENVELOPES
	{ MESSAGE_DEV_KDF, MECHANISM_DERIVE_PKCS5, ( MECHANISM_FUNCTION ) kdfPKCS5 },
#endif /* USE_ENVELOPES */
#if defined( USE_PGP ) || defined( USE_PGPKEYS )
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PGP, ( MECHANISM_FUNCTION ) derivePGP },
#endif /* USE_PGP || USE_PGPKEYS */
#ifdef USE_SSL
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_SSL, ( MECHANISM_FUNCTION ) deriveSSL },
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_TLS, ( MECHANISM_FUNCTION ) deriveTLS },
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_TLS12, ( MECHANISM_FUNCTION ) deriveTLS12 },
	{ MESSAGE_DEV_SIGN, MECHANISM_SIG_SSL, ( MECHANISM_FUNCTION ) signSSL },
	{ MESSAGE_DEV_SIGCHECK, MECHANISM_SIG_SSL, ( MECHANISM_FUNCTION ) sigcheckSSL },
#endif /* USE_SSL */
#ifdef USE_CMP
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_CMP, ( MECHANISM_FUNCTION ) deriveCMP },
#endif /* USE_CMP */
#ifdef USE_PKCS12
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PKCS12, ( MECHANISM_FUNCTION ) derivePKCS12 },
#endif /* USE_PKCS12 */
#if defined( USE_KEYSETS ) && defined( USE_PKC )
	{ MESSAGE_DEV_EXPORT, MECHANISM_PRIVATEKEYWRAP, ( MECHANISM_FUNCTION ) exportPrivateKey },
	{ MESSAGE_DEV_IMPORT, MECHANISM_PRIVATEKEYWRAP, ( MECHANISM_FUNCTION ) importPrivateKey },
	{ MESSAGE_DEV_EXPORT, MECHANISM_PRIVATEKEYWRAP_PKCS8, ( MECHANISM_FUNCTION ) exportPrivateKeyPKCS8 },
	{ MESSAGE_DEV_IMPORT, MECHANISM_PRIVATEKEYWRAP_PKCS8, ( MECHANISM_FUNCTION ) importPrivateKeyPKCS8 },
#endif /* USE_KEYSETS && USE_PKC */
#ifdef USE_PGPKEYS
	{ MESSAGE_DEV_IMPORT, MECHANISM_PRIVATEKEYWRAP_PGP2, ( MECHANISM_FUNCTION ) importPrivateKeyPGP2 },
	{ MESSAGE_DEV_IMPORT, MECHANISM_PRIVATEKEYWRAP_OPENPGP_OLD, ( MECHANISM_FUNCTION ) importPrivateKeyOpenPGPOld },
	{ MESSAGE_DEV_IMPORT, MECHANISM_PRIVATEKEYWRAP_OPENPGP, ( MECHANISM_FUNCTION ) importPrivateKeyOpenPGP },
#endif /* USE_PGPKEYS */
	{ MESSAGE_NONE, MECHANISM_NONE, NULL }, { MESSAGE_NONE, MECHANISM_NONE, NULL }
	};

/* Object creation functions supported by the system device.  These are
   sorted in order of frequency of use in order to make lookups a bit
   faster */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createCertificate( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo, 
					   STDC_UNUSED const void *auxDataPtr, 
					   STDC_UNUSED const int auxValue );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createEnvelope( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo, 
					STDC_UNUSED const void *auxDataPtr, 
					STDC_UNUSED const int auxValue );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createSession( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				   STDC_UNUSED const void *auxDataPtr, 
				   STDC_UNUSED const int auxValue );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createKeyset( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				  STDC_UNUSED const void *auxDataPtr, 
				  STDC_UNUSED const int auxValue );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createDevice( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				  STDC_UNUSED const void *auxDataPtr, 
				  STDC_UNUSED const int auxValue );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createUser( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				STDC_UNUSED const void *auxDataPtr, 
				STDC_UNUSED const int auxValue );

static const CREATEOBJECT_FUNCTION_INFO FAR_BSS createObjectFunctions[] = {
	{ OBJECT_TYPE_CONTEXT, createContext },
#ifdef USE_CERTIFICATES
	{ OBJECT_TYPE_CERTIFICATE, createCertificate },
#endif /* USE_CERTIFICATES */
#ifdef USE_ENVELOPES
	{ OBJECT_TYPE_ENVELOPE, createEnvelope },
#endif /* USE_ENVELOPES */
#ifdef USE_SESSIONS
	{ OBJECT_TYPE_SESSION, createSession },
#endif /* USE_SESSIONS */
#ifdef USE_KEYSETS
	{ OBJECT_TYPE_KEYSET, createKeyset },
#endif /* USE_KEYSETS */
	{ OBJECT_TYPE_DEVICE, createDevice },
	{ OBJECT_TYPE_USER, createUser },
	{ OBJECT_TYPE_NONE, NULL }, { OBJECT_TYPE_NONE, NULL }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Get a random (but not necessarily cryptographically strong random) nonce.
   Some nonces can simply be fresh (for which a monotonically increasing
   sequence will do), some should be random (for which a hash of the
   sequence is adequate), and some need to be unpredictable.  In order to
   avoid problems arising from the inadvertent use of a nonce with the wrong
   properties we use unpredictable nonces in all cases, even where it isn't
   strictly necessary.

   This simple generator divides the nonce state into a public section of
   the same size as the hash output and a private section that contains 64
   bits of data from the crypto RNG, which influences the public section.
   The public and private sections are repeatedly hashed to produce the
   required amount of output.  Note that this leaks a small amount of
   information about the crypto RNG output since an attacker knows that
   public_state_n = hash( public_state_n - 1, private_state ) but this
   isn't a major weakness */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getNonce( INOUT SYSTEMDEV_INFO *systemInfo, 
					 OUT_BUFFER_FIXED( dataLength ) void *data, 
					 IN_LENGTH_SHORT const int dataLength )
	{
	BYTE *noncePtr = data;
	int nonceLength, iterationCount;

	assert( isWritePtr( systemInfo, sizeof( SYSTEMDEV_INFO ) ) );
	assert( isWritePtr( data, dataLength ) );

	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH_SHORT );

	/* If the nonce generator hasn't been initialised yet we set up the
	   hashing and get 64 bits of private nonce state.  What to do if the
	   attempt to initialise the state fails is somewhat debatable.  Since
	   nonces are only ever used in protocols alongside crypto keys and an
	   RNG failure will be detected when the key is generated we can
	   generally ignore a failure at this point.  However nonces are 
	   sometimes also used in non-crypto contexts (for example to generate
	   certificate serial numbers) where this detection in the RNG won't 
	   happen.  On the other hand we shouldn't really abort processing just 
	   because we can't get some no-value nonce data so what we do is retry 
	   the fetch of nonce data (in case the system object was busy and the 
	   first attempt timed out) and if that fails too fall back to the 
	   system time.  This is no longer unpredictable, but the only location 
	   where unpredictability matters is when used in combination with 
	   crypto operations for which the absence of random data will be 
	   detected during key generation */
	if( !systemInfo->nonceDataInitialised )
		{
		MESSAGE_DATA msgData;
		int status;

		/* Get the 64-bit private portion of the nonce data */
		getHashAtomicParameters( CRYPT_ALGO_SHA1, 0,
								 &systemInfo->hashFunctionAtomic,
								 &systemInfo->hashSize );
		setMessageData( &msgData, systemInfo->nonceData + \
								  systemInfo->hashSize, 8 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_RANDOM );
		if( cryptStatusError( status ) )
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_GETATTRIBUTE_S, &msgData,
									  CRYPT_IATTRIBUTE_RANDOM );
		if( cryptStatusError( status ) )
			{
			const time_t theTime = getTime();

			memcpy( systemInfo->nonceData + systemInfo->hashSize, &theTime,
					sizeof( time_t ) );
			}
		systemInfo->nonceDataInitialised = TRUE;
		}
	ENSURES( systemInfo->hashFunctionAtomic != NULL );
	ENSURES( systemInfo->hashSize >= 16 && \
			 systemInfo->hashSize <= CRYPT_MAX_HASHSIZE );

	/* Shuffle the public state and copy it to the output buffer until it's
	   full */
	for( nonceLength = dataLength, iterationCount = 0; 
		 nonceLength > 0 && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		const int bytesToCopy = min( nonceLength, systemInfo->hashSize );

		/* Hash the state and copy the appropriate amount of data to the
		   output buffer */
		systemInfo->hashFunctionAtomic( systemInfo->nonceData, 
										CRYPT_MAX_HASHSIZE, 
										systemInfo->nonceData,
										systemInfo->hashSize + 8 );
		memcpy( noncePtr, systemInfo->nonceData, bytesToCopy );

		/* Move on to the next block of the output buffer */
		noncePtr += bytesToCopy;
		nonceLength -= bytesToCopy;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( CRYPT_OK );
	}

#ifndef CONFIG_NO_SELFTEST

/* Perform the algorithm self-test */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int algorithmSelfTest( INOUT \
								CAPABILITY_INFO_LIST **capabilityInfoListPtrPtr )
	{
	CAPABILITY_INFO_LIST *capabilityInfoListPtr;
	CAPABILITY_INFO_LIST *capabilityInfoListPrevPtr = NULL;
	BOOLEAN algoTested = FALSE;
	int iterationCount, status = CRYPT_OK;

	assert( isReadPtr( capabilityInfoListPtrPtr, \
					   sizeof( CAPABILITY_INFO_LIST * ) ) );

	/* Test each available capability */
	for( capabilityInfoListPtr = *capabilityInfoListPtrPtr, iterationCount = 0;
		 capabilityInfoListPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_MED;
		 capabilityInfoListPtr = capabilityInfoListPtr->next, iterationCount++ )
		{
		const CAPABILITY_INFO *capabilityInfoPtr = capabilityInfoListPtr->info;
		int localStatus;

		REQUIRES( capabilityInfoPtr->selfTestFunction != NULL );

		/* Perform the self-test for this algorithm type */
		localStatus = capabilityInfoPtr->selfTestFunction();
		if( cryptStatusError( localStatus ) )
			{
			/* The self-test failed, remember the status if it's the first 
			   failure and disable this algorithm */
			if( cryptStatusOK( status ) )
				status = localStatus;
			deleteSingleListElement( capabilityInfoListPtrPtr, 
									 capabilityInfoListPrevPtr, 
									 capabilityInfoListPtr );

			/* Since the algorithm self-test is one of the first things that 
			   gets called when cryptlib is built on a new system we make it 
			   easy for developers by printing a diagnostic in the debug 
			   build */
			DEBUG_PRINT(( "Algorithm %s failed self-test.\n", 
						  capabilityInfoPtr->algoName ));
			}
		else
			{
			algoTested = TRUE;

			/* Remember the last successfully-tested capability */
			capabilityInfoListPrevPtr = capabilityInfoListPtr;
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( algoTested ? status : CRYPT_ERROR_NOTFOUND );
	}

/* Perform the mechanism self-test.  This is performed in addition to the 
   algorithm tests if the user requests a test of all algorithms.  Currently
   only key derivation mechanisms are tested since the others either produce
   non-constant results that can't be checked against a fixed value or 
   require the creation of multiple contexts to hold keys */

typedef struct {
	MECHANISM_TYPE mechanismType;
	MECHANISM_DERIVE_INFO mechanismInfo;
	} MECHANISM_TEST_INFO;

#define MECHANISM_OUTPUT_SIZE		32
#define MECHANISM_INPUT_SIZE		32
#define MECHANISM_SALT_SIZE			16

#define MECHANISM_OUTPUT_SIZE_SSL	48
#define MECHANISM_INPUT_SIZE_SSL	48
#define MECHANISM_SALT_SIZE_SSL		64

static const BYTE FAR_BSS inputValue[] = {
	/* More than a single hash block size for SHA-1 */
	0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 
	0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
	0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87, 
	0x78, 0x69, 0x5A, 0x4B, 0x3C, 0x2D, 0x1E, 0x0F,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
	0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
	};
static const BYTE FAR_BSS saltValue[] = {
	/* At least 64 bytes for SSL/TLS PRF */
	0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87, 
	0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
	0x78, 0x69, 0x5A, 0x4B, 0x3C, 0x2D, 0x1E, 0x0F, 
	0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
	0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x80, 0x91, 0xA2, 0xB3, 0xC4, 0xD5, 0xE6, 0xF7, 
	0x08, 0x19, 0x2A, 0x3B, 0x4C, 0x5D, 0x6E, 0x7F
	};
#ifdef USE_PKCS12
static const BYTE FAR_BSS pkcs12saltValue[] = {
	/* PKCS #12 has a single-byte diversifier at the start of the salt */
	0x01,
	0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87, 
	0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
	};
#endif /* USE_PKCS12 */

static const MECHANISM_TEST_INFO FAR_BSS mechanismTestInfo[] = {
	{ MECHANISM_DERIVE_PKCS5,
	  { "\x73\xF7\x8A\xBE\x3C\x9C\x65\x80\x97\x60\x56\xDE\x04\x2A\x0C\x97"
		"\x99\xF5\x06\x0F\x43\x06\xA5\xD0\x74\xC9\xD5\xC5\xA5\x05\xB5\x7F", MECHANISM_OUTPUT_SIZE,
		inputValue, MECHANISM_INPUT_SIZE, CRYPT_ALGO_HMAC_SHA1, 0,
		saltValue, MECHANISM_SALT_SIZE, 10 } },
#if defined( USE_PGP ) || defined( USE_PGPKEYS )
	{ MECHANISM_DERIVE_PGP,
	  { "\x4A\x4B\x90\x09\x27\xF8\xD0\x93\x56\x16\xEA\xC1\x45\xCD\xEE\x05"
		"\x67\xE1\x09\x38\x66\xEB\xB2\xB2\xB9\x1F\xD3\xF7\x48\x2B\xDC\xCA", MECHANISM_OUTPUT_SIZE,
		inputValue, MECHANISM_INPUT_SIZE, CRYPT_ALGO_SHA1, 0,
		saltValue, 8, 10 } },
#endif /* USE_PGP || USE_PGPKEYS */
#ifdef USE_SSL
	{ MECHANISM_DERIVE_SSL,
	  { "\x87\x46\xDD\x7D\xAD\x5F\x48\xB6\xFC\x8D\x92\xC4\xDB\x38\x79\x9A"
		"\x3D\xEA\x22\xFA\xCD\x7E\x86\xD5\x23\x6E\x10\x4C\xBD\x84\x89\xDF"
		"\x1C\x87\x60\xBF\xFA\x2B\xCA\xFE\xFE\x65\xC7\xA2\xCF\x04\xFF\xEB", MECHANISM_OUTPUT_SIZE_SSL,
		inputValue, MECHANISM_INPUT_SIZE_SSL, ( CRYPT_ALGO_TYPE ) CRYPT_USE_DEFAULT, 0,
		saltValue, MECHANISM_SALT_SIZE_SSL, 1 } },				  /* Both MD5 and SHA1 */
	{ MECHANISM_DERIVE_TLS,
	  { "\xD3\xD4\x2F\xD6\xE3\x7D\xC0\x3C\xA6\x9F\x92\xDF\x3E\x40\x0A\x64"
		"\x49\xB4\x0E\xC4\x14\x04\x2F\xC8\xDD\x27\xD5\x1C\x62\xD2\x2C\x97"
		"\x90\xAE\x08\x4B\xEE\xF4\x8D\x22\xF0\x2A\x1E\x38\x2D\x31\xCB\x68", MECHANISM_OUTPUT_SIZE_SSL,
		inputValue, MECHANISM_INPUT_SIZE_SSL, ( CRYPT_ALGO_TYPE ) CRYPT_USE_DEFAULT, 0,
		saltValue, MECHANISM_SALT_SIZE_SSL, 1 } },				  /* Both MD5 and SHA1 */
#endif /* USE_SSL */
#ifdef USE_CMP
	{ MECHANISM_DERIVE_CMP,
	  { "\x80\x0B\x95\x73\x74\x3B\xC1\x63\x6B\x28\x2B\x04\x47\xFD\xF0\x04"
		"\x80\x40\x31\xB1", 20,
		inputValue, MECHANISM_INPUT_SIZE, CRYPT_ALGO_SHA1, 0,
		saltValue, MECHANISM_SALT_SIZE, 10 } },
#endif /* USE_CMP */
#ifdef USE_PKCS12
  #if 0		/* Additional check value from OpenSSL, this only uses 1 iteration */
	{ MECHANISM_DERIVE_PKCS12,
	  { "\x8A\xAA\xE6\x29\x7B\x6C\xB0\x46\x42\xAB\x5B\x07\x78\x51\x28\x4E"
		"\xB7\x12\x8F\x1A\x2A\x7F\xBC\xA3", 24,
		"smeg", 4, CRYPT_ALGO_SHA1, 0,
		"\x01\x0A\x58\xCF\x64\x53\x0D\x82\x3F", 1 + 8, 1 } },
  #endif /* 0 */
  #if 0		/* Additional check value from OpenSSL, now with 1,000 iterations */
	{ MECHANISM_DERIVE_PKCS12,
	  { "\xED\x20\x34\xE3\x63\x28\x83\x0F\xF0\x9D\xF1\xE1\xA0\x7D\xD3\x57"
		"\x18\x5D\xAC\x0D\x4F\x9E\xB3\xD4", 24,
		"queeg", 5, CRYPT_ALGO_SHA1, 0,
		"\x01\x05\xDE\xC9\x59\xAC\xFF\x72\xF7", 1 + 8, 1000 } },
  #endif /* 0 */
	{ MECHANISM_DERIVE_PKCS12,
	  { "\x8B\xFB\x1D\x77\xFE\x78\xFF\xE8\xE9\x69\x76\xE0\xC5\x0A\xB6\xD2"
		"\x64\xEC\xA3\x01\xE9\xD2\xE0\xC0\xBC\x60\x3D\x63\xB2\x4A\xB2\x63", MECHANISM_OUTPUT_SIZE,
		inputValue, MECHANISM_INPUT_SIZE, CRYPT_ALGO_SHA1, 0,
		pkcs12saltValue, 1 + MECHANISM_SALT_SIZE, 10 } },
#endif /* USE_PKCS12 */
	{ MECHANISM_NONE }, { MECHANISM_NONE }
	};

CHECK_RETVAL \
static int mechanismSelfTest( void )
	{
	BYTE buffer[ MECHANISM_OUTPUT_SIZE_SSL + 8 ];
	int i, status;

	for( i = 0; mechanismTestInfo[ i ].mechanismType != MECHANISM_NONE && \
				i < FAILSAFE_ARRAYSIZE( mechanismTestInfo, MECHANISM_TEST_INFO );
		 i++ )
		{
		const MECHANISM_TEST_INFO *mechanismTestInfoPtr = \
											&mechanismTestInfo[ i ];
		MECHANISM_DERIVE_INFO mechanismInfo;

		memcpy( &mechanismInfo, &mechanismTestInfoPtr->mechanismInfo, 
				sizeof( MECHANISM_DERIVE_INFO ) );
		mechanismInfo.dataOut = buffer;
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_DERIVE, &mechanismInfo,
								  mechanismTestInfoPtr->mechanismType );
		if( cryptStatusError( status ) )
			return( status );
		if( memcmp( mechanismTestInfoPtr->mechanismInfo.dataOut, buffer, 
					mechanismTestInfoPtr->mechanismInfo.dataOutLength ) )
			return( CRYPT_ERROR_FAILED );
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( mechanismTestInfo, MECHANISM_TEST_INFO ) );

	return( CRYPT_OK );
	}
#endif /* CONFIG_NO_SELFTEST */

/****************************************************************************
*																			*
*					Device Init/Shutdown/Device Control Routines			*
*																			*
****************************************************************************/

/* Initialise and shut down the system device */

CHECK_RETVAL \
static int initCapabilities( void );		/* Fwd.dec for fn.*/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initFunction( INOUT DEVICE_INFO *deviceInfo, 
						 STDC_UNUSED const char *name,
						 STDC_UNUSED const int nameLength )
	{
	int status;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	REQUIRES( name == NULL && nameLength == 0 );

	/* Set up the capability information for this device */
	status = initCapabilities();
	if( cryptStatusError( status ) )
		return( status );

	/* Set up the randomness information */
	status = initRandomInfo( &deviceInfo->randomInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Complete the initialisation and mark the device as active */
	deviceInfo->label = "cryptlib system device";
	deviceInfo->labelLen = strlen( deviceInfo->label );
	deviceInfo->flags = DEVICE_ACTIVE | DEVICE_LOGGEDIN | DEVICE_TIME;
	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void shutdownFunction( INOUT DEVICE_INFO *deviceInfo )
	{
	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	endRandomInfo( &deviceInfo->randomInfo );
	}

#ifndef CONFIG_NO_SELFTEST

/* Perform a self-test */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int selftestFunction( INOUT DEVICE_INFO *deviceInfo,
							 INOUT MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	CAPABILITY_INFO_LIST **capabilityInfoListPtrPtr = \
		( CAPABILITY_INFO_LIST ** ) &deviceInfo->capabilityInfoList;
	BYTE buffer[ 8 + 8 ];
	int refCount, status;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( messageExtInfo, \
						sizeof( MESSAGE_FUNCTION_EXTINFO ) ) );

	/* The self-tests need randomness for some of their operations, in order
	   to pre-empt a lack of this from causing a failure somewhere deep down
	   in the crypto code we perform a dummy read of first the randomness 
	   source and then the nonce source to force a full initialisation of 
	   the randomness subsystem */
	status = deviceInfo->getRandomFunction( deviceInfo, buffer, 8, NULL );
	if( cryptStatusError( status ) )
		return( status );
	zeroise( buffer, 8 );
	status = getNonce( deviceInfo->deviceSystem, buffer, 8 );
	if( cryptStatusError( status ) )
		return( status );
	zeroise( buffer, 8 );

	/* Perform an algorithm self-test */
	status = algorithmSelfTest( capabilityInfoListPtrPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* Perform the mechanism self-test.  Since this can be quite lengthy and 
	   requires recursive handling of messages by the system object (without
	   actually requiring access to system object state) we unlock it to 
	   avoid it becoming a bottleneck */
	status = krnlSuspendObject( deviceInfo->objectHandle, &refCount );
	if( cryptStatusError( status ) )
		return( status );
	setMessageObjectUnlocked( messageExtInfo );
	return( mechanismSelfTest() );
	}
#endif /* !CONFIG_NO_SELFTEST */

/* Get random data.  We have to unlock the device around the randomness 
   fetch because background polling threads need to be able to send entropy
   data to it:

				System			Randomness
				------			----------
	getRand ------>|				|
			   [Suspend]			|
				   |--------------->|
				   |				|
				   |<===============| Entropy
				   |<===============| Entropy
				   |<===============| Entropy Quality
				   |				|
				   |<---------------|
			   [Resume]				|
   
   If the caller has specified that it's unlockable and the reference count
   is one or less (meaning that we've been sent the message directly), we 
   leave it unlocked.  Otherwise we re-lock it afterwards. 

   Note that there's a tiny chance of a race condition if the system object 
   is destroyed between the unlock and the acquisition of the randomness 
   mutex (which means that the randomInfo could be freed while we're getting 
   the random data), however there's no easy way around this short of using
   a complex multiple-mutex interlock, and in any case there's only so much 
   that we can do to help a user who pulls data structures out from under 
   active threads */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getRandomFunction( INOUT DEVICE_INFO *deviceInfo, 
							  OUT_BUFFER_FIXED( length ) void *buffer,
							  IN_LENGTH_SHORT const int length, 
							  INOUT_OPT MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	int refCount, status;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH_SHORT );

	/* Clear the return value and make sure that we fail the FIPS 140 tests
	   on the output if there's a problem */
	zeroise( buffer, length );

	/* If the system device is already unlocked (which can happen if this 
	   function is called in a loop, for example if multiple chunks of 
	   randomness are read) just return the randomness directly */
	if( messageExtInfo != NULL && isMessageObjectUnlocked( messageExtInfo ) )
		return( getRandomData( deviceInfo->randomInfo, buffer, length ) );

	/* Unlock the system device, get the data, and re-lock it if necessary */
	status = krnlSuspendObject( deviceInfo->objectHandle, &refCount );
	if( cryptStatusError( status ) )
		return( status );
	status = getRandomData( deviceInfo->randomInfo, buffer, length );
	if( messageExtInfo == NULL || refCount > 1 )
		{
		int resumeStatus;

		/* The object isn't unlockable or it's been locked recursively, 
		   re-lock it */
		resumeStatus = krnlResumeObject( SYSTEM_OBJECT_HANDLE, refCount );
		if( cryptStatusError( resumeStatus ) )
			{
			/* We couldn't re-lock the system object, let the caller know.
			   Since this is a shouldn't-occur condition we also warn the 
			   user in the debug version */
			DEBUG_DIAG(( "Failed to re-lock system object" ));
			assert( DEBUG_WARN );
			if( messageExtInfo != NULL )
				setMessageObjectUnlocked( messageExtInfo );
			}
		}
	else
		{
		/* Tell the caller that we've left the object unlocked so they don't
		   have to do anything further with it */
		setMessageObjectUnlocked( messageExtInfo );
		}
	return( status );
	}

/* Handle device control functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int controlFunction( INOUT DEVICE_INFO *deviceInfo,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type,
							IN_BUFFER_OPT( dataLength ) void *data, 
							IN_LENGTH_SHORT_Z const int dataLength,
							INOUT_OPT MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	int refCount, status;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( data == NULL || isReadPtr( data, dataLength ) );
	
	REQUIRES( type == CRYPT_IATTRIBUTE_ENTROPY || \
			  type == CRYPT_IATTRIBUTE_ENTROPY_QUALITY || \
			  type == CRYPT_IATTRIBUTE_RANDOM_POLL || \
			  type == CRYPT_IATTRIBUTE_RANDOM_NONCE || \
			  type == CRYPT_IATTRIBUTE_TIME );
	REQUIRES( ( ( type == CRYPT_IATTRIBUTE_ENTROPY || \
				  type == CRYPT_IATTRIBUTE_RANDOM_NONCE ) && \
				( data != NULL && \
				  dataLength > 0 && dataLength < MAX_INTLENGTH ) ) || \
			  ( type == CRYPT_IATTRIBUTE_TIME && \
				data != NULL && dataLength == sizeof( time_t ) ) || \
			  ( ( type == CRYPT_IATTRIBUTE_ENTROPY_QUALITY || \
				  type == CRYPT_IATTRIBUTE_RANDOM_POLL ) && \
				( data == NULL && \
				  dataLength >= 0 && dataLength < MAX_INTLENGTH_SHORT ) ) );

	/* Handle entropy addition.  Since this can take awhile, we do it with
	   the system object unlocked.  See the comment in getRandomFunction()
	   about the possibility of a race condition */
	if( type == CRYPT_IATTRIBUTE_ENTROPY )
		{
		status = krnlSuspendObject( deviceInfo->objectHandle, &refCount );
		if( cryptStatusError( status ) )
			return( status );
		setMessageObjectUnlocked( messageExtInfo );
		return( addEntropyData( deviceInfo->randomInfo, data, dataLength ) );
		}
	if( type == CRYPT_IATTRIBUTE_ENTROPY_QUALITY )
		{
		status = krnlSuspendObject( deviceInfo->objectHandle, &refCount );
		if( cryptStatusError( status ) )
			return( status );
		setMessageObjectUnlocked( messageExtInfo );
		return( addEntropyQuality( deviceInfo->randomInfo, dataLength ) );
		}
	if( type == CRYPT_IATTRIBUTE_RANDOM_POLL )
		{
		status = krnlSuspendObject( deviceInfo->objectHandle, &refCount );
		if( cryptStatusError( status ) )
			return( status );
		setMessageObjectUnlocked( messageExtInfo );

		/* Perform a slow or fast poll as required */
		if( dataLength == TRUE )
			slowPoll();
		else
			fastPoll();

		return( CRYPT_OK );
		}

	/* Handle nonces */
	if( type == CRYPT_IATTRIBUTE_RANDOM_NONCE )
		return( getNonce( deviceInfo->deviceSystem, data, dataLength ) );

	/* Handle high-reliability time */
	if( type == CRYPT_IATTRIBUTE_TIME )
		{
		time_t *timePtr = ( time_t * ) data;

		*timePtr = getTime();
		return( CRYPT_OK );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*							Device Capability Routines						*
*																			*
****************************************************************************/

/* The cryptlib intrinsic capability list */

#define MAX_NO_CAPABILITIES		32

static const GETCAPABILITY_FUNCTION FAR_BSS getCapabilityTable[] = {
	get3DESCapability,
	getAESCapability,
#ifdef USE_CAST
	getCASTCapability,
#endif /* USE_CAST */
#ifdef USE_DES
	getDESCapability,
#endif /* USE_DES */
#ifdef USE_IDEA
	getIDEACapability,
#endif /* USE_IDEA */
#ifdef USE_RC2
	getRC2Capability,
#endif /* USE_RC2 */
#ifdef USE_RC4
	getRC4Capability,
#endif /* USE_RC4 */

#ifdef USE_MD5
	getMD5Capability,
#endif /* USE_MD5 */
	getSHA1Capability,
	getSHA2Capability,

	getHmacSHA1Capability,
	getHmacSHA2Capability,

#ifdef USE_DH
	getDHCapability,
#endif /* USE_DH */
#ifdef USE_DSA
	getDSACapability,
#endif /* USE_DSA */
#ifdef USE_ELGAMAL
	getElgamalCapability,
#endif /* USE_ELGAMAL */
#ifdef USE_RSA
	getRSACapability,
#endif /* USE_RSA */
#ifdef USE_ECDSA
	getECDSACapability,
#endif /* USE_ECDSA */
#ifdef USE_ECDH
	getECDHCapability,
#endif /* USE_ECDH */

	getGenericSecretCapability,

	/* Vendors may want to use their own algorithms, which aren't part of the
	   general cryptlib suite.  The following provides the ability to include
	   vendor-specific algorithm capabilities defined in the file
	   vendalgo.c */
#ifdef USE_VENDOR_ALGOS
	#include "vendalgo.c"
#endif /* USE_VENDOR_ALGOS */

	/* End-of-list marker */
	NULL, NULL
	};

static CAPABILITY_INFO_LIST FAR_BSS capabilityInfoList[ MAX_NO_CAPABILITIES ];

/* Initialise the capability information */

CHECK_RETVAL \
static int initCapabilities( void )
	{
	int i;

	/* Build the list of available capabilities */
	memset( capabilityInfoList, 0,
			sizeof( CAPABILITY_INFO_LIST ) * MAX_NO_CAPABILITIES );
	for( i = 0; 
		 getCapabilityTable[ i ] != NULL && \
			i < FAILSAFE_ARRAYSIZE( getCapabilityTable, GETCAPABILITY_FUNCTION ); 
		 i++ )
		{
		const CAPABILITY_INFO *capabilityInfoPtr = getCapabilityTable[ i ]();

#ifndef CONFIG_FUZZ
		REQUIRES( sanityCheckCapability( capabilityInfoPtr ) );
#endif /* !CONFIG_FUZZ */

		capabilityInfoList[ i ].info = capabilityInfoPtr;
		capabilityInfoList[ i ].next = NULL;
		if( i > 0 )
			capabilityInfoList[ i - 1 ].next = &capabilityInfoList[ i ];
		}
	REQUIRES( i < FAILSAFE_ARRAYSIZE( getCapabilityTable, \
									  GETCAPABILITY_FUNCTION ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Device Access Routines							*
*																			*
****************************************************************************/

/* Set up the function pointers to the device methods */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setDeviceSystem( INOUT DEVICE_INFO *deviceInfo )
	{
	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	deviceInfo->initFunction = initFunction;
	deviceInfo->shutdownFunction = shutdownFunction;
	deviceInfo->controlFunction = controlFunction;
#ifndef CONFIG_NO_SELFTEST
	deviceInfo->selftestFunction = selftestFunction;
#endif /* !CONFIG_NO_SELFTEST */
	deviceInfo->getRandomFunction = getRandomFunction;
	deviceInfo->capabilityInfoList = capabilityInfoList;
	deviceInfo->createObjectFunctions = createObjectFunctions;
	deviceInfo->createObjectFunctionCount = \
		FAILSAFE_ARRAYSIZE( createObjectFunctions, CREATEOBJECT_FUNCTION_INFO );
	deviceInfo->mechanismFunctions = mechanismFunctions;
	deviceInfo->mechanismFunctionCount = \
		FAILSAFE_ARRAYSIZE( mechanismFunctions, MECHANISM_FUNCTION_INFO );

	return( CRYPT_OK );
	}
