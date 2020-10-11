/****************************************************************************
*																			*
*						  Win32 Randomness-Gathering Code					*
*	Copyright Peter Gutmann, Matt Thomlinson and Blake Coverett 1996-2017	*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom number
   generator.  For usage conditions, see random.c */

/* General includes */

#include <stdio.h>
#include "crypt.h"
#include "random/random.h"

/* OS-specific includes */

#include <lm.h>				/* For NetStatisticsGet() */
#include <winperf.h>
#include <winioctl.h>
#include <process.h>

/* MinGW doesn't support inline asm (via gas) in its standard install
   so we turn it off just for this file */

#ifdef __MINGW32__
  #define NO_ASM
#endif /* __MINGW32__ */

/* Some new CPU opcodes aren't supported by all compiler versions, if
   they're not available we define them here.  BC++ can only handle the
   emit directive outside an asm block, so we have to terminate the current
   block, emit the opcode, and then restart the asm block.  In addition
   while the 16-bit versions of BC++ had built-in asm support, the 32-bit
   versions removed it and generate a temporary asm file which is passed
   to Tasm32.  This is only included with some high-end versions of BC++
   and it's not possible to order it separately, so if you're building with
   BC++ and get error messages about a missing tasm32.exe, add NO_ASM to
   Project Options | Compiler | Defines */

#if defined( _MSC_VER )
  #if _MSC_VER <= 1100
	#define cpuid		__asm _emit 0x0F __asm _emit 0xA2
	#define rdtsc		__asm _emit 0x0F __asm _emit 0x31
  #endif /* VC++ 5.0 or earlier */
  #define xstore_rng	__asm _emit 0x0F __asm _emit 0xA7 __asm _emit 0xC0
  #define rdrand_eax	__asm _emit 0x0F __asm _emit 0xC7 __asm _emit 0xF0
  #define rdseed_eax	__asm _emit 0x0F __asm _emit 0xC7 __asm _emit 0xF8
#endif /* VC++ */
#if defined __BORLANDC__
  #define cpuid			} __emit__( 0x0F, 0xA2 ); __asm {
  #define rdtsc			} __emit__( 0x0F, 0x31 ); __asm {
  #define xstore_rng	} __emit__( 0x0F, 0xA7, 0xC0 ); __asm {
  #define rdrand_eax	} __emit__( 0x0F, 0xC7, 0xF0 ); __asm {
  #define rdseed_eax	} __emit__( 0x0F, 0xC7, 0xF8 ); __asm {
#endif /* BC++ */

/* Map a value that may be 32 or 64 bits depending on the platform to a 
   long.  For 64-bit debug builds we can't use HandleToLong() because it
   results in a runtime exception due to loss of precision so we mask it to 
   LONG_MAX and then cast it */

#ifdef __WIN64__
  #ifndef NDEBUG
	#define addRandomHandle( randomState, handle ) \
			addRandomLong( randomState, \
						   ( long ) ( ( LONG_PTR ) ( handle ) & LONG_MAX ) )
  #else
	#define addRandomHandle( randomState, handle ) \
			addRandomLong( randomState, HandleToLong( handle ) )
  #endif /* Debug vs.non-debug */
#else
  #define addRandomHandle	addRandomValue
#endif /* 32- vs. 64-bit VC++ */

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	4096
#if RANDOM_BUFSIZE > MAX_INTLENGTH_SHORT
  #error RANDOM_BUFSIZE exceeds randomness accumulator size
#endif /* RANDOM_BUFSIZE > MAX_INTLENGTH_SHORT */

/* Handles to various randomness objects */

static HANDLE hAdvAPI32;	/* Handle to misc.library */
static HANDLE hNTAPI;		/* Handle to Windows kernel library */
static HANDLE hThread;		/* Background polling thread handle */
static unsigned int threadID;	/* Background polling thread ID */

/****************************************************************************
*																			*
*							System RNG Interface							*
*																			*
****************************************************************************/

/* The number of bytes to read from the system RNG on each slow poll */

#define SYSTEMRNG_BYTES		64

/* Intel Chipset CSP type and name */

#define PROV_INTEL_SEC	22
#define INTEL_DEF_PROV	"Intel Hardware Cryptographic Service Provider"

/* A mapping from CryptoAPI to standard data types */

#define HCRYPTPROV			HANDLE

/* Type definitions for function pointers to call CryptoAPI functions */

typedef BOOL ( WINAPI *CRYPTACQUIRECONTEXT )( HCRYPTPROV *phProv,
											  LPCTSTR pszContainer,
											  LPCTSTR pszProvider, DWORD dwProvType,
											  DWORD dwFlags );
typedef BOOL ( WINAPI *CRYPTGENRANDOM )( HCRYPTPROV hProv, DWORD dwLen,
										 BYTE *pbBuffer );
typedef BOOL ( WINAPI *CRYPTRELEASECONTEXT )( HCRYPTPROV hProv, DWORD dwFlags );

/* Somewhat alternative functionality available as a direct call, for 
   Windows XP and newer.  This is the CryptoAPI RNG, which isn't anywhere
   near as good as the HW RNG, but we use it if it's present on the basis
   that at least it can't make things any worse.  This direct access version 
   is only available under Windows XP and newer, we don't go out of our way 
   to access the more general CryptoAPI one since the main purpose of using 
   it is to take advantage of any possible future hardware RNGs that may be 
   added, for example via TCPA devices */

typedef BOOL ( WINAPI *RTLGENRANDOM )( PVOID RandomBuffer, 
									   ULONG RandomBufferLength );

/* Global function pointers. These are necessary because the functions need
   to be dynamically linked since older versions of Windows NT don't contain 
   them */

static CRYPTACQUIRECONTEXT pCryptAcquireContext = NULL;
static CRYPTGENRANDOM pCryptGenRandom = NULL;
static CRYPTRELEASECONTEXT pCryptReleaseContext = NULL;
static RTLGENRANDOM pRtlGenRandom = NULL;

/* Handle to the RNG CSP */

static BOOLEAN systemRngAvailable;	/* Whether system RNG is available */
static HCRYPTPROV hProv;			/* Handle to Intel RNG CSP */

/* Try and connect to the system RNG if there's one present.  In theory we 
   could also try and get data from a TPM if there's one present via the TPM
   base services (TBS), but the TPM functions are only available under Vista 
   and newer (so they'd have to be dynamically bound), the chances of a TPM 
   being present, enabled, and accessible are pretty slim, and since the TBS
   require use to to talk to the TPM at the raw APDU level (not to mention 
   all the horror stories in the MSDN forums about getting any of this stuff 
   to work) the amount of effort required versus the potential gain really 
   doesn't make it worthwhile (one talk on TPM programming calls it "a 
   horrifically bad programming experience"):

	#include <tbs.h>

	TBS_HCONTEXT hTbsContext;
	TBS_CONTEXT_PARAMS tbsContextParams;
	HRESULT hResult;

	// Include dynamic-linking management for all functions

	// Get a handle to the TBS interface
	memset( &tspContextParams, 0, sizeof( TBS_CONTEXT_PARAMS ) );
	tspContextParams.version = TBS_CONTEXT_VERSION_ONE;
	result = Tbsi_Context_Create( &tbsContextParams, &hTbsContext );
	if( SUCCEEDED( hResult ) )
		{
		BYTE tpmCommand[] = {
			0x00, 0xC1,					// WORD: TPM_TAG_RQU_COMMAND
			0x00, 0x00, 0x00, 0x0E,		// LONG: Total packet length
			0x00, 0x00, 0x00, 0x46,		// LONG: Command TPM_ORD_GetRandom
			0x00, 0x00, 0x00, 0x20		// Data: Get 0x20 bytes
		BYTE buffer[ 14 + 32 + 8 ];
		int length;

		hResult = Tbsip_Submit_Command( hTbsContext, 
										TBS_COMMAND_LOCALITY_ZERO,
										TBS_COMMAND_PRIORITY_NORMAL, 
										tpmCommand, 14, buffer, &length );
		if( SUCCEEDED( hResult ) )
			{
			// Start of buffer contains the 14 bytes of returned command 
			// followed by 32 bytes of random data
			}
		Tbsip_Context_Close( hTbsContext );
		} 

   There is a higher-level API, the TSPI, but that's only available as a 
   semi-experimental implementation provided as a custom download, on the
   off chance that someone is using this we optionally provide it under the
   USE_TPM define */

static void initSystemRNG( void )
	{
	systemRngAvailable = FALSE;
	hProv = NULL;
	if( ( hAdvAPI32 = GetModuleHandle( "AdvAPI32.dll" ) ) == NULL )
		return;

	/* Get pointers to the CSP functions.  Although the acquire context
	   function looks like a standard function, it's actually a macro which
	   is mapped to (depending on the build type) CryptAcquireContextA or
	   CryptAcquireContextW, so we access it under the straight-ASCII-
	   function name */
	pCryptAcquireContext = ( CRYPTACQUIRECONTEXT ) GetProcAddress( hAdvAPI32,
													"CryptAcquireContextA" );
	pCryptGenRandom = ( CRYPTGENRANDOM ) GetProcAddress( hAdvAPI32,
													"CryptGenRandom" );
	pCryptReleaseContext = ( CRYPTRELEASECONTEXT ) GetProcAddress( hAdvAPI32,
													"CryptReleaseContext" );

	/* Get a pointer to the native randomness function if it's available.  
	   This isn't exported by name, so we have to get it by ordinal */
	pRtlGenRandom = ( RTLGENRANDOM ) GetProcAddress( hAdvAPI32,
													"SystemFunction036" );

	/* Try and connect to the PIII RNG CSP.  The AMD 768 southbridge (from 
	   the 760 MP chipset) also has a hardware RNG, but there doesn't appear 
	   to be any driver support for this as there is for the Intel RNG so we 
	   can't do much with it.  OTOH the Intel RNG is also effectively dead, 
	   mostly due to virtually nonexistant support/marketing by Intel, it's 
	   included here mostly for form's sake */
	if( ( pCryptAcquireContext == NULL || \
		  pCryptGenRandom == NULL || pCryptReleaseContext == NULL || \
		  pCryptAcquireContext( &hProv, NULL, INTEL_DEF_PROV,
								PROV_INTEL_SEC, 0 ) == FALSE ) && \
		( pRtlGenRandom == NULL ) )
		{
		hAdvAPI32 = NULL;
		hProv = NULL;
		}
	else
		{
		/* Remember that we have a system RNG available for use */
		systemRngAvailable = TRUE;
		}
	}

/* Read data from the system RNG, in theory the PIII hardware RNG but in
   practice the CryptoAPI software RNG */

static void readSystemRNG( void )
	{
	BYTE buffer[ SYSTEMRNG_BYTES + 8 ];
	int quality = 0;

	if( !systemRngAvailable )
		return;

	/* Read SYSTEMRNG_BYTES bytes from the system RNG.  Rather presciently, 
	   the code up until late 2007 stated that "We don't rely on this for 
	   all our randomness requirements (particularly the software RNG) in 
	   case it's broken in some way", and in November 2007 an analysis paper
	   by Leo Dorrendorf, Zvi Gutterman, and Benny Pinkas showed that the
	   CryptoAPI RNG is indeed broken, being neither forwards- nor 
	   backwards-secure, being reseeded far too infrequently, and (as far as
	   their reverse-engineering was able to tell) using far less entropy
	   sources than cryptlib's built-in mechanisms even though the CryptoAPI
	   one is deeply embedded in the OS.

	   The way the CryptoAPI RNG works is that a system RNG is used to key 
	   a set of eight RC4 ciphers, each of which is used in turn in a round-
	   robin fashion to output 20 bytes of randomness which are then hashed 
	   with SHA1, with the operation being approximately:

		output[  0...19 ] = SHA1( RC4[ i++ % 8 ] );
		output[ 20...39 ] = SHA1( RC4[ i++ % 8 ] );
		[...]

	   Each RC4 cipher is re-keyed every 16KB of output, so for 8 ciphers
	   the rekey interval is every 128KB of output.  Furthermore, although
	   the kernel RNG used to key the RC4 ciphers stores its state in-
	   kernel, the RC4 cipher state is stored in user-space and per-process.
	   This means that in most cases the RNG is never re-keyed.  Finally,
	   the way the RNG works means that it's possible to recover earlier
	   state in O( 2^23 ).  See "Cryptanalysis of the Random Number 
	   Generator of the Windows Operating System" for details */
	if( hProv != NULL )
		{
		if( pCryptGenRandom( hProv, SYSTEMRNG_BYTES, buffer ) )
			quality = 70;
		}
	else
		{
		if( pRtlGenRandom( buffer, SYSTEMRNG_BYTES ) )
			quality = 60;
		}
	if( quality > 0 )
		{
		MESSAGE_DATA msgData;
		int status;

		setMessageData( &msgData, buffer, SYSTEMRNG_BYTES );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_ENTROPY );
		if( cryptStatusOK( status ) )
			{
			( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &quality,
									  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
			}
		zeroise( buffer, SYSTEMRNG_BYTES );
		}
	}

/* Clean up the system RNG access */

static void endSystemRNG( void )
	{
	if( hProv != NULL )
		{
		pCryptReleaseContext( hProv, 0 );
		hProv = NULL;
		}
	}

#ifdef USE_TPM

static void readTPMRNG( void )
	{
	TSS_HCONTEXT hTSPIContext;
	TSS_HTPM hTPM;
	BYTE *bufPtr;

	/* Create a TSPI context, connect it to the system TPM, and get the TPM
	   handle from it */
	if( Tspi_Context_Create( &hTSPIContext ) != TSS_SUCCESS )
		return;
    if( Tspi_Context_Connect( hTSPIContext, NULL ) != TSS_SUCCESS || \
		Tspi_Context_GetTpmObject( hTSPIContext, &hTPM ) != TSS_SUCCESS )
		{
		Tspi_Context_Close( hTSPIContext );
        return;
		}

	/* Get random data from the TPM */ 
	if( Tspi_TPM_GetRandom( hTPM, SYSTEMRNG_BYTES, &bufPtr ) == TSS_SUCCESS )
		{
		MESSAGE_DATA msgData;
		static const int quality = 70;
		int status;

		setMessageData( &msgData, bufPtr, SYSTEMRNG_BYTES );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_ENTROPY );
		if( cryptStatusOK( status ) )
			{
			( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &quality,
									  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
			}
		zeroise( bufPtr, SYSTEMRNG_BYTES );
		Tspi_Context_FreeMemory( hTSPIContext, bufPtr );
		}

	/* Clean up */
	Tspi_Context_Close( hTPM );
	Tspi_Context_Free_Memory( hTSPIContext, NULL );
	Tspi_Context_Close( hTSPIContext );
	}
#else
  #define readTPMRNG()
#endif /* USE_TPM */

/****************************************************************************
*																			*
*							External RNG Interface							*
*																			*
****************************************************************************/

/* Interface to the Quantis quantum RNG */

typedef void *QUANTIS_HANDLE;

/* Type definitions for function pointers to call Quantis functions */

typedef int ( *QUANTISOPEN )( int deviceType, unsigned int deviceNumber,
							  QUANTIS_HANDLE* deviceHandle );
typedef int ( *QUANTISREADHANDLE )( QUANTIS_HANDLE deviceHandle, 
									void *buffer, size_t size );
typedef int ( *QUANTISCLOSE )( QUANTIS_HANDLE deviceHandle );

/* Global function pointers. These are necessary because the functions need
   to be dynamically linked since most systems won't contain them */

static QUANTISOPEN pQuantisOpen = NULL;
static QUANTISREADHANDLE pQuantisReadHandled = NULL;
static QUANTISCLOSE pQuantisClose = NULL;

#if 0

typedef struct {
	int deviceNumber;
	int deviceType;
	/* QuantisOperations* */ void *ops;
	void *privateData;
	} QUANTIS_HANDLE;
#endif

/* Quantis device types */

#define QUANTIS_DEVICE_PCI			1	/* PCI device */
#define QUANTIS_DEVICE_USB			2	/* USB device */

/* Quantis error codes */

#define QUANTIS_SUCCESS				0
#define QUANTIS_ERROR_NO_DRIVER		1	/* Invalid driver */
#define QUANTIS_ERROR_INVALID_DEVICE_NUMBER	2 /* Invalid device no.*/
#define QUANTIS_ERROR_INVALID_READ_SIZE	3 /* Invalid size to read (too high) */
#define QUANTIS_ERROR_INVALID_PARAMETER	4 /* Invalid parameter */
#define QUANTIS_ERROR_NO_MEMORY		5	/* Insufficient memory */
#define QUANTIS_ERROR_NO_MODULE		6	/* No module found */
#define QUANTIS_ERROR_IO			7	/* Input/output error */
#define QUANTIS_ERROR_NO_DEVICE		8	/* No such device or dev.disconnected */
#define QUANTIS_ERROR_OPERATION_NOT_SUPPORTED 9 /* Op.not supported */
#define QUANTIS_ERROR_OTHER			99	/* Other error */

/* Handle to the Quantis RNG */

static BOOLEAN externalRngAvailable;	/* Whether external RNG is available */
static HANDLE hQuantis;					/* Handle to Quantis library */
static QUANTIS_HANDLE quantisDevice;	/* Handle to Quantis device */

static void initExternalRNG( void )
	{
	int status;

	externalRngAvailable = FALSE;
	quantisDevice = NULL;

	/* Load the Quantis library.  This is a real pain to do because we have 
	   no idea where the Quantis DLL will be, the Quantis manual puts it in 
	   "<path-to-quantis>\Packages\Windows\lib\<your system arch>\", and
	   as if that isn't vague enough, in 
	   "<path-to-filename>\Libs-Apps\Quantis\<your system arch>" if you 
	   build the library yourself.  Because of this we wouldn't be able to
	   hardcode in an absolute path for SafeLoadLibrary(), but in practice
	   we require that it be stored in the Windows directory to avoid 
	   having to break the security module for safe loads */
	if( ( hQuantis = SafeLoadLibrary( "Quantis.dll" ) ) == NULL )
		{
		/* If we're building the debug build we also allow the use of the
		   emulated RNG library */
#if ( _MSC_VER == 1200 ) && !defined( NDEBUG )
		if( ( hQuantis = LoadLibrary( "Quantis-NoHw.dll" ) ) != NULL )
			{
			DEBUG_DIAG(( "Using software emulation of Quantis hardware "
						 "RNG" ));
			}
		else
#endif /* VC++ 6.0 && !NDEBUG */
		return;
		}

	/* Get pointers to the Quantis API functions */
	pQuantisOpen = ( QUANTISOPEN ) GetProcAddress( hQuantis, "QuantisOpen" );
	pQuantisReadHandled = ( QUANTISREADHANDLE ) GetProcAddress( hQuantis,
													"QuantisReadHandled" );
	pQuantisClose = ( QUANTISCLOSE ) GetProcAddress( hQuantis, "QuantisClose" );
	if( pQuantisOpen == NULL || pQuantisReadHandled == NULL || \
		pQuantisClose == NULL )
		{
		FreeLibrary( hQuantis );
		hQuantis = NULL;
		return;
		}

	/* Try and connect to the Quantis device */
	status = pQuantisOpen ( QUANTIS_DEVICE_PCI, 0, &quantisDevice );
	if( status != QUANTIS_SUCCESS )
		status = pQuantisOpen ( QUANTIS_DEVICE_USB, 0, &quantisDevice );
	if( status != QUANTIS_SUCCESS )
		{
		FreeLibrary( hQuantis );
		hQuantis = NULL;
		DEBUG_DIAG_ERRMSG(( "Quantis hardware RNG device couldn't be "
							"accessed, status %s", 
							getStatusName( status ) ));
		return;
		}

	externalRngAvailable = TRUE;
	}

/* Read data from the external RNG */

static void readExternalRNG( void )
	{
	BYTE buffer[ SYSTEMRNG_BYTES + 8 ];
	int count, quality = 0;

	if( !externalRngAvailable )
		return;

	/* Read SYSTEMRNG_BYTES bytes from the external RNG.  Note that the docs
	   are incorrect for this function since they claim that a successful
	   read products QUANTIS_SUCCES (sic), it's actually a byte count */
	count = pQuantisReadHandled( quantisDevice, buffer, SYSTEMRNG_BYTES );
	if( count >= SYSTEMRNG_BYTES )
		quality = 70;
	if( quality > 0 )
		{
		MESSAGE_DATA msgData;
		int status;

		setMessageData( &msgData, buffer, count );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_ENTROPY );
		if( cryptStatusOK( status ) )
			{
			( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &quality,
									  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
			}
		zeroise( buffer, SYSTEMRNG_BYTES );
		}
	}

/* Clean up the external RNG access */

static void endExternalRNG( void )
	{
	if( hQuantis != NULL )
		{
		FreeLibrary( hQuantis );
		hQuantis = NULL;
		}
	}

/****************************************************************************
*																			*
*							Hardware Monitoring Interface					*
*																			*
****************************************************************************/

/* These interfaces currently support data supplied by MBM, Everest, 
   SysTool, RivaTuner, HMonitor, ATI Tray Tools, CoreTemp, and GPU-Z.  Two 
   notable omissions are SVPro and HWMonitor, unfortunately the authors 
   haven't responded to any requests for interfacing information */

/* MBM data structures, originally by Alexander van Kaam, converted to C by
   Anders@Majland.org, finally updated by Chris Zahrt <techn0@iastate.edu>.
   The __int64 values below are actually (64-bit) doubles, but we overlay 
   them with __int64s since we don't care about the values */

#define BusType		char
#define SMBType		char
#define SensorType	char

typedef struct {
	SensorType iType;			/* Type of sensor */
	int Count;					/* Number of sensor for that type */
	} SharedIndex;

typedef struct {
	SensorType ssType;			/* Type of sensor */
	unsigned char ssName[ 12 ];	/* Name of sensor */
	char sspadding1[ 3 ];		/* Padding of 3 bytes */
	__int64 /*double*/ ssCurrent;/* Current value */
	__int64 /*double*/ ssLow;	/* Lowest readout */
	__int64 /*double*/ ssHigh;	/* Highest readout */
	long ssCount;				/* Total number of readout */
	char sspadding2[ 4 ];		/* Padding of 4 bytes */
	BYTE /*long double*/ ssTotal[ 8 ];	/* Total amout of all readouts */
	char sspadding3[ 6 ];		/* Padding of 6 bytes */
	__int64 /*double*/ ssAlarm1;/* Temp & fan: high alarm; voltage: % off */
	__int64 /*double*/ ssAlarm2;/* Temp: low alarm */
	} SharedSensor;

typedef struct {
	short siSMB_Base;			/* SMBus base address */
	BusType siSMB_Type;			/* SMBus/Isa bus used to access chip */
	SMBType siSMB_Code;			/* SMBus sub type, Intel, AMD or ALi */
	char siSMB_Addr;			/* Address of sensor chip on SMBus */
	unsigned char siSMB_Name[ 41 ];	/* Nice name for SMBus */
	short siISA_Base;			/* ISA base address of sensor chip on ISA */
	int siChipType;				/* Chip nr, connects with Chipinfo.ini */
	char siVoltageSubType;		/* Subvoltage option selected */
	} SharedInfo;

typedef struct {
	__int64 /*double*/ sdVersion;/* Version number (example: 51090) */
	SharedIndex sdIndex[ 10 ];	/* Sensor index */
	SharedSensor sdSensor[ 100 ];	/* Sensor info */
	SharedInfo sdInfo;			/* Misc.info */
	unsigned char sdStart[ 41 ];	/* Start time */
	/* We don't use the next two fields both because they're not random and
	   because it provides a nice safety margin in case of data size mis-
	   estimates (we always under-estimate the buffer size) */
/*	unsigned char sdCurrent[ 41 ];	/* Current time */
/*	unsigned char sdPath[ 256 ];	/* MBM path */
	} SharedData;

/* Read data from MBM via the shared-memory interface */

static void readMBMData( void )
	{
	HANDLE hMBMData;
	const SharedData *mbmDataPtr;

	if( ( hMBMData = OpenFileMapping( FILE_MAP_READ, FALSE,
									  "$M$B$M$5$S$D$" ) ) == NULL )
		return;
	if( ( mbmDataPtr = ( SharedData * ) \
			MapViewOfFile( hMBMData, FILE_MAP_READ, 0, 0, 0 ) ) != NULL )
		{
		MESSAGE_DATA msgData;
		static const int quality = 20;
		int status;

		setMessageData( &msgData, ( MESSAGE_CAST ) mbmDataPtr, 
						sizeof( SharedData ) );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_ENTROPY );
		if( cryptStatusOK( status ) )
			{
			( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &quality,
									  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
			}
		UnmapViewOfFile( mbmDataPtr );
		}
	CloseHandle( hMBMData );
	}

/* Read data from Everest via the shared-memory interface.  Everest returns 
   information as an enormous XML text string so we have to be careful about 
   handling of lengths.  In general the returned length is 1-3K, so we 
   hard-limit it at 2K to ensure there are no problems if the trailing null 
   gets lost */

static void readEverestData( void )
	{
	HANDLE hEverestData;
	const void *everestDataPtr;

	if( ( hEverestData = OpenFileMapping( FILE_MAP_READ, FALSE,
										  "EVEREST_SensorValues" ) ) == NULL )
		return;
	if( ( everestDataPtr = MapViewOfFile( hEverestData, 
										  FILE_MAP_READ, 0, 0, 0 ) ) != NULL )
		{
		const int length = strlen( everestDataPtr );

		if( length > 128 )
			{
			MESSAGE_DATA msgData;
			static const int quality = 40;
			int status;

			setMessageData( &msgData, ( MESSAGE_CAST ) everestDataPtr, 
							min( length, 2048 ) );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE_S, &msgData, 
									  CRYPT_IATTRIBUTE_ENTROPY );
			if( cryptStatusOK( status ) )
				{
				( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
										  IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &quality,
										  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
				}
			}
		UnmapViewOfFile( everestDataPtr );
		}
	CloseHandle( hEverestData );
	}

/* SysTool data structures, from forums.techpowerup.com.  This uses the
   standard shared memory interface, but it gets a bit tricky because it 
   uses the OLE 'VARIANT' data type which we can't access because it's 
   included via a complex nested inclusion framework in windows.h, and the 
   use of '#pragma once' when we included it for use in crypt.h means that 
   we can't re-include any of the necessary files.  Since we don't actually 
   care what's inside a VARIANT, and it has a fixed size of 16 bytes, we 
   just use it as an opaque blob */

typedef BYTE VARIANT[ 16 ];	/* Kludge for OLE data type */
typedef BYTE UINT8;
typedef WORD UINT16;

#define SH_MEM_MAX_SENSORS 128

typedef enum { sUnknown, sNumber, sTemperature, sVoltage, sRPM, sBytes, 
			   sBytesPerSecond, sMhz, sPercentage, sString, sPWM 
			 } SYSTOOL_SENSOR_TYPE;
 
typedef struct {
	WCHAR m_name[ 255 ];	/* Sensor name */
	WCHAR m_section[ 64 ];	/* Section in which this sensor appears */
	SYSTOOL_SENSOR_TYPE m_sensorType;
	LONG m_updateInProgress;/* Nonzero when sensor is being updated */
	UINT32 m_timestamp;		/* GetTickCount() of last update */
	VARIANT m_value;		/* Sensor data */
	WCHAR m_unit[ 8 ];		/* Unit for text output */
	UINT8 m_nDecimals;		/* Default number of decimals for formatted output */
	} SYSTOOL_SHMEM_SENSOR;
 
typedef struct {
	UINT32 m_version;		/* Version of shared memory structure */
	UINT16 m_nSensors;		/* Number of records with data in m_sensors */
	SYSTOOL_SHMEM_SENSOR m_sensors[ SH_MEM_MAX_SENSORS ];
	} SYSTOOL_SHMEM;

static void readSysToolData( void )
	{
	HANDLE hSysToolData;
	const SYSTOOL_SHMEM *sysToolDataPtr;

	if( ( hSysToolData = OpenFileMapping( FILE_MAP_READ, FALSE,
										  "SysToolSensors" ) ) == NULL )
		return;
	if( ( sysToolDataPtr = ( SYSTOOL_SHMEM * ) \
			MapViewOfFile( hSysToolData, FILE_MAP_READ, 0, 0, 0 ) ) != NULL )
		{
		MESSAGE_DATA msgData;
		static const int quality = 40;
		int status;

		setMessageData( &msgData, ( MESSAGE_CAST ) sysToolDataPtr, 
						sizeof( SYSTOOL_SHMEM ) );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_ENTROPY );
		if( cryptStatusOK( status ) )
			{
			( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &quality,
									  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
			}
		UnmapViewOfFile( sysToolDataPtr );
		}
	CloseHandle( hSysToolData );
	}

/* RivaTuner data structures via the shared-memory interface, from the 
   RivaTuner sample source.  The DWORD values below are actually (32-bit) 
   floats, but we overlay them with DWORDs since we don't care about the 
   values.  The information is only accessible when the RivaTuner hardware 
   monitoring window is open.  This one is the easiest of the shared-mem 
   interfaces to monitor because for fowards-compatibility reasons it 
   stores a Windows-style indicator of the size of each struct entry in the 
   data header, so we can get the total size simply by multiplying out the 
   number of entries by the indicated size.  As a safety measure we limit 
   the maximum data size to 2K in case a value gets corrupted */

typedef struct {
	DWORD dwSignature;	/* 'RTHM' if active */
	DWORD dwVersion;	/* Must be 0x10001 or above */
	DWORD dwNumEntries;	/* No.of RTHM_SHARED_MEMORY_ENTRY entries */
	time_t time;		/* Last polling time */
	DWORD dwEntrySize;	/* Size of entries in RTHM_SHARED_MEMORY_ENTRY array */
	} RTHM_SHARED_MEMORY_HEADER;

typedef struct {
	char czSrc[ 32 ];	/* Source description */
	char czDim[ 16 ];	/* Source measurement units */
	DWORD /*float*/ data;	/* Source data */
	DWORD /*float*/ offset;	/* Source offset, e.g. temp.compensation */
	DWORD /*float*/ dataTransformed;/* Source data in transformed(?) form */
	DWORD flags;		/* Misc.flags */
	} RTHM_SHARED_MEMORY_ENTRY;

static void readRivaTunerData( void )
	{
	HANDLE hRivaTunerData;
	RTHM_SHARED_MEMORY_HEADER *rivaTunerHeaderPtr;

	if( ( hRivaTunerData = OpenFileMapping( FILE_MAP_READ, FALSE,
											"RTHMSharedMemory" ) ) == NULL )
		return;
	if( ( rivaTunerHeaderPtr = ( RTHM_SHARED_MEMORY_HEADER * ) \
			MapViewOfFile( hRivaTunerData, FILE_MAP_READ, 0, 0, 0 ) ) != NULL )
		{
		if( rivaTunerHeaderPtr->dwSignature == 'RTHM' && \
			rivaTunerHeaderPtr->dwVersion >= 0x10001 )
			{
			MESSAGE_DATA msgData;
			const BYTE *entryPtr = ( ( BYTE * ) rivaTunerHeaderPtr ) + \
								   sizeof( RTHM_SHARED_MEMORY_HEADER );
			const int entryTotalSize = rivaTunerHeaderPtr->dwNumEntries * \
									   rivaTunerHeaderPtr->dwEntrySize;
			static const int quality = 10;
			int status;

			setMessageData( &msgData, ( MESSAGE_CAST ) entryPtr, 
							min( entryTotalSize, 2048 ) );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE_S, &msgData, 
									  CRYPT_IATTRIBUTE_ENTROPY );
			if( cryptStatusOK( status ) )
				{
				( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
										  IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &quality,
										  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
				}
			}
		UnmapViewOfFile( rivaTunerHeaderPtr );
		}
	CloseHandle( hRivaTunerData );
	}

/* Read data from HMonitor via the shared-memory interface.  The DWORD 
   values below are actually (32-bit) floats, but we overlay them with 
   DWORDs since we don't care about the values.  Like RivaTuner's data the 
   info structure contains a length value at the start, so it's fairly easy 
   to work with */

typedef struct {
	WORD length;
	WORD version;	/* Single 'DWORD length' before version 4.1 */
	DWORD /*float*/ temp[ 3 ];
	DWORD /*float*/ voltage[ 7 ];
	int fan[ 3 ];
	} HMONITOR_DATA;

static void readHMonitorData( void )
	{
	HANDLE hHMonitorData;
	HMONITOR_DATA *hMonitorDataPtr;

	if( ( hHMonitorData = OpenFileMapping( FILE_MAP_READ, FALSE,
										   "Hmonitor_Counters_Block" ) ) == NULL )
		return;
	if( ( hMonitorDataPtr = ( HMONITOR_DATA * ) \
			MapViewOfFile( hHMonitorData, FILE_MAP_READ, 0, 0, 0 ) ) != NULL )
		{
		if( hMonitorDataPtr->version >= 0x4100 && \
			( hMonitorDataPtr->length >= 48 && hMonitorDataPtr->length <= 1024 ) )
			{
			MESSAGE_DATA msgData;
			static const int quality = 40;
			int status;

			setMessageData( &msgData, hMonitorDataPtr, hMonitorDataPtr->length );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE_S, &msgData, 
									  CRYPT_IATTRIBUTE_ENTROPY );
			if( cryptStatusOK( status ) )
				{
				( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
										  IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &quality,
										  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
				}
			}
		UnmapViewOfFile( hMonitorDataPtr );
		}
	CloseHandle( hHMonitorData );
	}

/* Read data from ATI Tray Tools via the shared-memory interface.  This has
   an added twist in that just because it's available to be read doesn't 
   mean that it contains any data, so we check that at least the GPU speed 
   and temp-monitoring-supported flag have nonzero values */

typedef struct {
	DWORD CurGPU;		/* GPU speed */
	DWORD CurMEM;		/* Video memory speed */
	DWORD isGameActive;
	DWORD is3DActive;	/* Boolean: 3D mode active */
	DWORD isTempMonSupported;	/* Boolean: Temp monitoring available */
	DWORD GPUTemp;		/* GPU temp */
	DWORD ENVTemp;		/* Video card temp */
	DWORD FanDuty;		/* Fan duty cycle */
	DWORD MAXGpuTemp, MINGpuTemp;	/* Min/max GPU temp */
	DWORD MAXEnvTemp, MINEnvTemp;	/* Min/max video card temp */
	DWORD CurD3DAA, CurD3DAF;	/* Direct3D info */
	DWORD CurOGLAA, CurOGLAF;	/* OpenGL info */
	DWORD IsActive;		/* Another 3D boolean */
	DWORD CurFPS;		/* FPS rate */
	DWORD FreeVideo;	/* Available video memory */
	DWORD FreeTexture;	/* Available texture memory */
	DWORD Cur3DApi;		/* API used (D3D, OpenGL, etc) */
	DWORD MemUsed;		/* ? */
	} TRAY_TOOLS_DATA;

static void readATITrayToolsData( void )
	{
	HANDLE hTrayToolsData;
	TRAY_TOOLS_DATA *trayToolsDataPtr;

	if( ( hTrayToolsData = OpenFileMapping( FILE_MAP_READ, FALSE,
											"ATITRAY_SMEM" ) ) == NULL )
		return;
	if( ( trayToolsDataPtr = ( TRAY_TOOLS_DATA * ) \
			MapViewOfFile( hTrayToolsData, FILE_MAP_READ, 0, 0, 0 ) ) != NULL )
		{
		if( trayToolsDataPtr->CurGPU >= 100 && \
			trayToolsDataPtr->isTempMonSupported != 0 )
			{
			MESSAGE_DATA msgData;
			static const int quality = 10;
			int status;

			setMessageData( &msgData, trayToolsDataPtr,
							sizeof( TRAY_TOOLS_DATA ) );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE_S, &msgData, 
									  CRYPT_IATTRIBUTE_ENTROPY );
			if( cryptStatusOK( status ) )
				{
				( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
										  IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &quality,
										  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
				}
			}
		UnmapViewOfFile( trayToolsDataPtr );
		}
	CloseHandle( hTrayToolsData );
	}

/* CoreTemp data structure, from www.alcpu.com/CoreTemp.   The DWORD values 
   below are actually (32-bit) floats but we overlay them with DWORDs since 
   we don't care about the values */

typedef struct 
	{
	unsigned int uiLoad[ 256 ];
	unsigned int uiTjMax[ 128 ];
	unsigned int uiCoreCnt;
	unsigned int uiCPUCnt;
	DWORD /*float*/ fTemp[ 256 ];
	DWORD /*float*/ fVID;
	DWORD /*float*/ fCPUSpeed;
	DWORD /*float*/ fFSBSpeed;
	DWORD /*float*/ fMultipier;	
	DWORD /*float*/ sCPUName[ 100 ];
	unsigned char ucFahrenheit;
	unsigned char ucDeltaToTjMax;
	} CORE_TEMP_SHARED_DATA;

static void readCoreTempData( void )
	{
	HANDLE hCoreTempData;
	CORE_TEMP_SHARED_DATA *coreTempDataPtr;

	if( ( hCoreTempData = OpenFileMapping( FILE_MAP_READ, FALSE,
										   "CoreTempMappingObject" ) ) == NULL )
		return;
	if( ( coreTempDataPtr = ( CORE_TEMP_SHARED_DATA * ) \
			MapViewOfFile( hCoreTempData, FILE_MAP_READ, 0, 0, 0 ) ) != NULL )
		{
		MESSAGE_DATA msgData;
		static const int quality = 15;
		int status;

		setMessageData( &msgData, coreTempDataPtr,
						sizeof( CORE_TEMP_SHARED_DATA ) );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_ENTROPY );
		if( cryptStatusOK( status ) )
			{
			( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &quality,
									  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
			}
		UnmapViewOfFile( coreTempDataPtr );
		}
	CloseHandle( hCoreTempData );
	}

/* GPU-Z data structures, from forums.techpowerup.com.  This uses an 
   incredibly wasteful memory layout so we end up having to submit around 
   200kB of data, unfortunately we can't easily pick out the few values of 
   interest because the 'data' entries push out the values of interest, the 
   'sensors' entries, to the end of the memory block */

#define MAX_RECORDS		128

#pragma pack(push, 1)

typedef struct {
	WCHAR key[ 256 ];
	WCHAR value[ 256 ];
	} GPUZ_RECORD;

typedef struct {
	WCHAR name[ 256 ];
	WCHAR unit[ 8 ];
	UINT32 digits;
	double value;
	} GPUZ_SENSOR_RECORD;

typedef struct {
	UINT32 version;		/* Version number, should be 1 */
	volatile LONG busy;	/* Data-update flag */
	UINT32 lastUpdate;	/* GetTickCount() of last update */
	GPUZ_RECORD data[ MAX_RECORDS ];
	GPUZ_SENSOR_RECORD sensors[ MAX_RECORDS ];
	} GPUZ_SH_MEM;

#pragma pack(pop)

static void readGPUZData( void )
	{
	HANDLE hGPUZData;
	const GPUZ_SH_MEM *gpuzDataPtr;

	if( ( hGPUZData = OpenFileMapping( FILE_MAP_READ, FALSE,
									   "GPUZShMem" ) ) == NULL )
		return;
	if( ( gpuzDataPtr = ( GPUZ_SH_MEM * ) \
		  MapViewOfFile( hGPUZData, FILE_MAP_READ, 0, 0, 0 ) ) != NULL )
		{
		if( gpuzDataPtr->version == 1 )
			{
			MESSAGE_DATA msgData;
			static const int quality = 10;
			int status;

			setMessageData( &msgData, ( MESSAGE_CAST ) gpuzDataPtr, 
							sizeof( GPUZ_SH_MEM ) );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE_S, &msgData, 
									  CRYPT_IATTRIBUTE_ENTROPY );
			if( cryptStatusOK( status ) )
				{
				( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
										  IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &quality,
										  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
				}
			}
		UnmapViewOfFile( gpuzDataPtr );
		}
	CloseHandle( hGPUZData );
	}

/****************************************************************************
*																			*
*							Hardware Configuration Data						*
*																			*
****************************************************************************/

/* Read PnP configuration data.  This is mostly static per machine, but
   differs somewhat across machines, which means that it acts as a per-
   machine salt, so that if a static image is deployed (for example as a VM 
   image) then we at least get the PRNG salted differently once the VM is 
   started.

   We have to define the values ourselves here due to some of the values and 
   functions not existing at the time that VC++ 6.0 was released */

typedef void * HDEVINFO;

#define DIGCF_PRESENT		0x02
#define DIGCF_ALLCLASSES	0x04

#define SPDRP_HARDWAREID					0x01
#define SPDRP_PHYSICAL_DEVICE_OBJECT_NAME	0x0E
#define SPDRP_SECURITY						0x17
#define SPDRP_DEVICE_POWER_DATA				0x1E

typedef struct _SP_DEVINFO_DATA {
	DWORD cbSize;
	GUID classGuid;
	DWORD devInst;
	ULONG *reserved;
	} SP_DEVINFO_DATA, *PSP_DEVINFO_DATA;

typedef BOOL ( WINAPI *SETUPDIDESTROYDEVICEINFOLIST )( HDEVINFO DeviceInfoSet );
typedef BOOL ( WINAPI *SETUPDIENUMDEVICEINFO )( HDEVINFO DeviceInfoSet,
												DWORD MemberIndex,
												PSP_DEVINFO_DATA DeviceInfoData );
typedef HDEVINFO ( WINAPI *SETUPDIGETCLASSDEVS )( /*CONST LPGUID*/ void *ClassGuid,
												  /*PCTSTR*/ void *Enumerator,
												  HWND hwndParent, DWORD Flags );
typedef BOOL ( WINAPI *SETUPDIGETDEVICEREGISTRYPROPERTY )( HDEVINFO DeviceInfoSet,
												PSP_DEVINFO_DATA DeviceInfoData,
												DWORD Property, PDWORD PropertyRegDataType,
												PBYTE PropertyBuffer,
												DWORD PropertyBufferSize, PDWORD RequiredSize );

static void readPnPData( void )
	{
	HANDLE hSetupAPI;
	HDEVINFO hDevInfo;
	SETUPDIDESTROYDEVICEINFOLIST pSetupDiDestroyDeviceInfoList = NULL;
	SETUPDIENUMDEVICEINFO pSetupDiEnumDeviceInfo = NULL;
	SETUPDIGETCLASSDEVS pSetupDiGetClassDevs = NULL;
	SETUPDIGETDEVICEREGISTRYPROPERTY pSetupDiGetDeviceRegistryProperty = NULL;
	SP_DEVINFO_DATA devInfoData;
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ], pnpBuffer[ 1024 + 8 ];
	DWORD cbPnPBuffer;
	int deviceCount, status, LOOP_ITERATOR;

	if( ( hSetupAPI = DynamicLoad( "SetupAPI.dll" ) ) == NULL )
		return;

	/* Get pointers to the PnP functions.  Although the get class-devs
	   and get device registry functions look like standard functions,
	   they're actually macros that are mapped to (depending on the build
	   type) xxxA or xxxW, so we access them under the straight-ASCII-
	   function name */
	pSetupDiDestroyDeviceInfoList = ( SETUPDIDESTROYDEVICEINFOLIST ) \
				GetProcAddress( hSetupAPI, "SetupDiDestroyDeviceInfoList" );
	pSetupDiEnumDeviceInfo = ( SETUPDIENUMDEVICEINFO ) \
				GetProcAddress( hSetupAPI, "SetupDiEnumDeviceInfo" );
	pSetupDiGetClassDevs = ( SETUPDIGETCLASSDEVS ) \
				GetProcAddress( hSetupAPI, "SetupDiGetClassDevsA" );
	pSetupDiGetDeviceRegistryProperty = ( SETUPDIGETDEVICEREGISTRYPROPERTY ) \
				GetProcAddress( hSetupAPI, "SetupDiGetDeviceRegistryPropertyA" );
	if( pSetupDiDestroyDeviceInfoList == NULL || \
		pSetupDiEnumDeviceInfo == NULL || pSetupDiGetClassDevs == NULL || \
		pSetupDiGetDeviceRegistryProperty == NULL )
		{
		DynamicUnload( hSetupAPI );
		return;
		}

	/* Get info on all PnP devices */
	hDevInfo = pSetupDiGetClassDevs( NULL, NULL, NULL,
									 DIGCF_PRESENT | DIGCF_ALLCLASSES );
	if( hDevInfo == INVALID_HANDLE_VALUE )
		{
		DynamicUnload( hSetupAPI );
		return;
		}

	/* Enumerate all PnP devices */
	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	if( cryptStatusError( status ) )
		{
		DynamicUnload( hSetupAPI );
		retIntError_Void();
		}
	memset( &devInfoData, 0, sizeof( devInfoData ) );
	devInfoData.cbSize = sizeof( SP_DEVINFO_DATA );
	LOOP_MAX( deviceCount = 0, 
			  pSetupDiEnumDeviceInfo( hDevInfo, deviceCount, 
									  &devInfoData ), 
			  deviceCount++ )
		{
		if( pSetupDiGetDeviceRegistryProperty( hDevInfo, &devInfoData,
											   SPDRP_HARDWAREID, NULL,
											   pnpBuffer, 1024, &cbPnPBuffer ) )
			{
			addRandomData( randomState, pnpBuffer, cbPnPBuffer );
			}
		if( pSetupDiGetDeviceRegistryProperty( hDevInfo, &devInfoData,
											   SPDRP_DEVICE_POWER_DATA, NULL,
											   pnpBuffer, 1024, &cbPnPBuffer ) )
			{
			addRandomData( randomState, pnpBuffer, cbPnPBuffer );
			}
		if( pSetupDiGetDeviceRegistryProperty( hDevInfo, &devInfoData,
											   SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, NULL,
											   pnpBuffer, 1024, &cbPnPBuffer ) )
			{
			addRandomData( randomState, pnpBuffer, cbPnPBuffer );
			}
		if( pSetupDiGetDeviceRegistryProperty( hDevInfo, &devInfoData,
											   SPDRP_SECURITY, NULL,
											   pnpBuffer, 1024, &cbPnPBuffer ) )
			{
			addRandomData( randomState, pnpBuffer, cbPnPBuffer );
			}
		}
	ENSURES_V( LOOP_BOUND_OK );
	pSetupDiDestroyDeviceInfoList( hDevInfo );
	endRandomData( randomState, 10 );

	DynamicUnload( hSetupAPI );
	}

/* Read ACPI BIOS information, with the same requirements as for the PnP 
   read */

typedef UINT ( WINAPI *ENUMSYSTEMFIRMWARETABLES )( DWORD FirmwareTableProviderSignature,
												   PVOID pFirmwareTableBuffer,
												   DWORD BufferSize );
typedef UINT ( WINAPI *GETSYSTEMFIRMWARETABLE )( DWORD FirmwareTableProviderSignature,
												 DWORD FirmwareTableID,
												 PVOID pFirmwareTableBuffer,
												 DWORD BufferSize );

#define NO_FW_IDS	64	/* No. ACPI FW IDs to check */

static void readACPIData( void )
	{
	HANDLE hKernel32;
	ENUMSYSTEMFIRMWARETABLES pEnumSystemFirmwareTables = NULL;
	GETSYSTEMFIRMWARETABLE pGetSystemFirmwareTable = NULL;
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	DWORD fwTableIDs[ NO_FW_IDS + 8 ];
	int noIDs, i, status;

	if( ( hKernel32 = DynamicLoad( "Kernel32.dll" ) ) == NULL )
		return;

	/* Get pointers to the ACPI functions */
	pEnumSystemFirmwareTables = ( ENUMSYSTEMFIRMWARETABLES ) \
				GetProcAddress( hKernel32, "EnumSystemFirmwareTables" );
	pGetSystemFirmwareTable = ( GETSYSTEMFIRMWARETABLE ) \
				GetProcAddress( hKernel32, "GetSystemFirmwareTable" );
	if( pEnumSystemFirmwareTables == NULL || \
		pGetSystemFirmwareTable == NULL )
		return;

	/* Get the IDs of the ACPI information that we want to retrieve, 
	   consisting of four-character IDs encoded as DWORDs.  There are 
	   typically 20-30 of these, NO_FW_IDS should work on most systems.
	   
	   This function has a weird return value, if it's zero or less then 
	   there's been an error, if it's in the range of the buffer that we 
	   passed it then it's the number of entries written to it, and if it's 
	   greater than the buffer size it's the amount of buffer space that we 
	   need to pass in to store the results.  Since this is non-critical
	   data, we don't bother with dynamically resizing the buffer, the given
	   one works in most cases */
	noIDs = pEnumSystemFirmwareTables( 'ACPI', fwTableIDs, 
									   NO_FW_IDS * sizeof( DWORD ) );
	if( noIDs <= sizeof( DWORD ) || \
		noIDs > NO_FW_IDS * sizeof( DWORD ) )
		return;
	noIDs /= sizeof( DWORD );
	ENSURES_V( noIDs > 0 && noIDs <= NO_FW_IDS );

	/* Read the ACPI information for each ID.  As with the firmware table 
	   IDs, we use a fixed-size buffer that works in most cases, the values
	   are only a few dozen or few hundred bytes */
	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	if( cryptStatusError( status ) )
		retIntError_Void();
	for( i = 0; i < noIDs; i++ )
		{
		BYTE acpiBuffer[ 2048 + 8 ];
		int acpiBufCount;

		/* Get the ACPI info for the current ID */
		acpiBufCount = pGetSystemFirmwareTable( 'ACPI', fwTableIDs[ i ], 
												acpiBuffer, 2048 );
		if( acpiBufCount <= 1 || acpiBufCount > 2048 )
			continue;	/* Error or more buffer needed */
		addRandomData( randomState, acpiBuffer, acpiBufCount );
		}
	endRandomData( randomState, 10 );
	}

/* Read network adapter information.  See the comment for readPnPData() for
   the salt-like nature of the data obtained.

   We have to define the values ourselves here due to some of the values and 
   functions not existing at the time that VC++ 6.0 was released */

#define AF_UNSPEC			0
#define GAA_FLAG_NONE		0

#define MAX_HOSTNAME_LEN	128
#define MAX_DOMAIN_NAME_LEN	128
#define MAX_SCOPE_ID_LEN	256

typedef struct {
	char String[ 4 * 4 ];
	} IP_ADDRESS_STRING, *PIP_ADDRESS_STRING, IP_MASK_STRING, *PIP_MASK_STRING;

typedef struct _IP_ADDR_STRING {
	struct _IP_ADDR_STRING* Next;
	IP_ADDRESS_STRING IpAddress;
	IP_MASK_STRING IpMask;
	DWORD Context;
	} IP_ADDR_STRING, *PIP_ADDR_STRING;

typedef struct {
	char HostName[ MAX_HOSTNAME_LEN + 4 ];
	char DomainName[ MAX_DOMAIN_NAME_LEN + 4 ];
	PIP_ADDR_STRING CurrentDnsServer;
	IP_ADDR_STRING DnsServerList;
	UINT NodeType;
	char ScopeId[ MAX_SCOPE_ID_LEN + 4 ];
	UINT EnableRouting;
	UINT EnableProxy;
	UINT EnableDns;
	} FIXED_INFO, *PFIXED_INFO;

typedef DWORD ( WINAPI *GETNETWORKPARAMS )( FIXED_INFO *pFixedInfo, 
											ULONG *pOutBufLen );
typedef ULONG ( WINAPI *GETADAPTERSADDRESSES )( ULONG Family, ULONG Flags, 
							PVOID Reserved,
							/* IP_ADAPTER_ADDRESSES */ void *AdapterAddresses,
							PULONG SizePointer );

#define NETWORK_INFO_BUFSIZE	( RANDOM_BUFSIZE * 2 )

static void readNetworkData( void )
	{
	HANDLE hIphlpAPI;
	GETNETWORKPARAMS pGetNetworkParams = NULL;
	GETADAPTERSADDRESSES pGetAdaptersAddresses = NULL;
	MESSAGE_DATA msgData;
	BYTE outBuf[ NETWORK_INFO_BUFSIZE + 8 ];
	ULONG ulOutBufLen;
	int status;

	if( ( hIphlpAPI = DynamicLoad( "Iphlpapi.dll" ) ) == NULL )
		return;

	/* Get pointers to the network helper functions */
	pGetNetworkParams = ( GETNETWORKPARAMS ) \
				GetProcAddress( hIphlpAPI, "GetNetworkParams" );
	pGetAdaptersAddresses = ( GETADAPTERSADDRESSES ) \
				GetProcAddress( hIphlpAPI, "GetAdaptersAddresses" );
	if( pGetNetworkParams == NULL || pGetAdaptersAddresses == NULL )
		{
		DynamicUnload( hIphlpAPI );
		return;
		}

	/* Get the host name, domain name, DNS server info, and other odds and 
	   ends */
	ulOutBufLen = NETWORK_INFO_BUFSIZE;
	if( pGetNetworkParams( ( FIXED_INFO * ) outBuf, 
						   &ulOutBufLen ) == ERROR_SUCCESS )
		{
		static const int quality = 5;

		setMessageData( &msgData, outBuf, sizeof( FIXED_INFO ) );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_ENTROPY );
		if( cryptStatusOK( status ) )
			{
			( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &quality,
									  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
			}
		}

	/* Get information on all network adapters in the system.  This returns 
	   a linked list of information on every adapter (with per-adapter 
	   internal embedded linked lists for address information (unicast, 
	   anycast, multicast, DNS server), gateway and DNS data, and so on), 
	   all linearised into a fixed memory block.  Alongside this is fixed 
	   data like MAC address, MTU, metrics, link information, DHCPv6 data, 
	   and other odds and ends.

	   MSDN recommends calling this function with an initial 15kB buffer and 
	   then expanding it as required (see
	   http://msdn.microsoft.com/en-us/library/windows/desktop/aa365915%28v=vs.85%29.aspx)
	   but we just use a buffer of RANDOM_BUFSIZE * 2 and don't try and get 
	   fancy if it fails.  Note that ulOutBufLen isn't modified unless the 
	   function returns ERROR_BUFFER_OVERFLOW so the length value that we 
	   pass to addRandomData() is most likely an over-estimate, but there's 
	   no way to tell how much of the buffer got filled without parsing the 
	   IP_ADAPTER_ADDRESSES contents.

	   Since we don't know how much data we're passing to addRandomData(), 
	   we clear the entire buffer beforehand to avoid warnings from memory-
	   checkers that will detect that we're passing uninitialised memory to
	   addRandomData() */
	ulOutBufLen = NETWORK_INFO_BUFSIZE;
	memset( outBuf, 0, NETWORK_INFO_BUFSIZE );
	if( pGetAdaptersAddresses( AF_UNSPEC, GAA_FLAG_NONE, NULL, outBuf, 
							   &ulOutBufLen ) == ERROR_SUCCESS )
		{
		static const int quality = 10;

		setMessageData( &msgData, outBuf, NETWORK_INFO_BUFSIZE );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_ENTROPY );
		if( cryptStatusOK( status ) )
			{
			( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &quality,
									  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
			}
		}

	DynamicUnload( hIphlpAPI );
	}

/****************************************************************************
*																			*
*									Fast Poll								*
*																			*
****************************************************************************/

/* The shared Win32 fast poll routine */

void fastPoll( void )
	{
	static BOOLEAN addedFixedItems = FALSE;
	FILETIME  creationTime, exitTime, kernelTime, userTime;
	SIZE_T minimumWorkingSetSize, maximumWorkingSetSize;
#if VC_GE_2010( _MSC_VER )
	GUITHREADINFO guiThreadInfo;
	MEMORYSTATUSEX memoryStatus;
#else
	MEMORYSTATUS memoryStatus;
#endif /* VC++ >= 2010 */
	HANDLE handle;
	POINT point;
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	int trngValue = 0, status;

	if( krnlIsExiting() )
		return;

	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	if( cryptStatusError( status ) )
		retIntError_Void();

	/* Get various basic pieces of system information: Handle of active
	   window, handle of window with mouse capture, handle of clipboard owner
	   handle of start of clpboard viewer list, pseudohandle of current
	   process, current process ID, pseudohandle of current thread, current
	   thread ID, handle of desktop window, handle  of window with keyboard
	   focus, whether system queue has any events, cursor position for last
	   message, 1 ms time for last message, handle of window with clipboard
	   open, handle of process heap, handle of procs window station, types of
	   events in input queue, and milliseconds since Windows was started.
	   Since a HWND/HANDLE can be a 64-bit value on a 64-bit platform, we 
	   have to use a mapping macro that discards the high 32 bits (which
	   presumably won't be of much interest anyway).
	   
	   Note that there's another call that does almost the same thing as 
	   GetTickCount(), timeGetTime() from the multimedia API, however this 
	   has a fairly constant delta between ticks while GetTickCount() 
	   wanders all over the place (while still averaging a fixed value).  So 
	   timeGetTime() (which uses KeQueryInterruptTime()) isn't necessarily 
	   more accurate than GetTickCount() (which uses KeQueryTickCount()), 
	   just more regular.  In addition GetTickCount() is meant as a 
	   monotonically increasing counter (at 1000 ticks per second) used to
	   measure intervals of time rather than absolute time, so its count 
	   indication "since the system was started" really means "since 
	   whenever the HAL initialised the PIT used for the counter".  Since 
	   we're interested in unpredictability, we use GetTickCount() and not 
	   timeGetTime() */
	addRandomHandle( randomState, GetActiveWindow() );
	addRandomHandle( randomState, GetCapture() );
	addRandomHandle( randomState, GetClipboardOwner() );
	addRandomHandle( randomState, GetClipboardViewer() );
	addRandomHandle( randomState, GetCurrentProcess() );
	addRandomValue( randomState, GetCurrentProcessId() );
	addRandomHandle( randomState, GetCurrentThread() );
	addRandomValue( randomState, GetCurrentThreadId() );
	addRandomHandle( randomState, GetDesktopWindow() );
	addRandomHandle( randomState, GetFocus() );
	addRandomValue( randomState, GetInputState() );
	addRandomValue( randomState, GetMessagePos() );
	addRandomValue( randomState, GetMessageTime() );
	addRandomHandle( randomState, GetOpenClipboardWindow() );
	addRandomHandle( randomState, GetProcessHeap() );
	addRandomHandle( randomState, GetProcessWindowStation() );
#if VC_GE_2010( _MSC_VER )
	/* GetTickCount() overflows approximately every 49 days while 
	   GetTickCount64() doesn't, however for our purposes this is a feature 
	   since the value isn't monotonically incrementing any more, and in
	   any case we'd just end up casting the output of GetTickCount64()
	   to an integer to pass it to addRandomValue.  Using it does however
	   produce a compiler warning, which we disable around the call */
	#pragma warning( push )
	#pragma warning( disable:28159 )
	addRandomValue( randomState, GetTickCount() );
	#pragma warning( pop )
#else
	addRandomValue( randomState, GetTickCount() );
#endif /* VC++ >= 2010 */
	if( krnlIsExiting() )
		return;

	/* Calling the following function can cause problems in some cases in
	   that a calling application eventually stops getting events from its
	   event loop, so we can't (safely) use it as an entropy source */
/*	addRandomValue( randomState, GetQueueStatus( QS_ALLEVENTS ) ); */

	/* Get multiword system information: Current caret position, current
	   mouse cursor position */
	GetCaretPos( &point );
	addRandomData( randomState, &point, sizeof( POINT ) );
	GetCursorPos( &point );
	addRandomData( randomState, &point, sizeof( POINT ) );

	/* Get percent of memory in use, bytes of physical memory, bytes of free
	   physical memory, bytes in paging file, free bytes in paging file, user
	   bytes of address space, and free user bytes */
#if VC_GE_2010( _MSC_VER )
	memoryStatus.dwLength = sizeof( MEMORYSTATUSEX );
	GlobalMemoryStatusEx( &memoryStatus );
	addRandomData( randomState, &memoryStatus, sizeof( MEMORYSTATUSEX ) );
#else
	memoryStatus.dwLength = sizeof( MEMORYSTATUS );
	GlobalMemoryStatus( &memoryStatus );
	addRandomData( randomState, &memoryStatus, sizeof( MEMORYSTATUS ) );
#endif /* VC++ >= 2010 */

	/* Get thread and process creation time, exit time, time in kernel mode,
	   and time in user mode in 100ns intervals */
	handle = GetCurrentThread();
	GetThreadTimes( handle, &creationTime, &exitTime, &kernelTime, &userTime );
	addRandomData( randomState, &creationTime, sizeof( FILETIME ) );
	addRandomData( randomState, &exitTime, sizeof( FILETIME ) );
	addRandomData( randomState, &kernelTime, sizeof( FILETIME ) );
	addRandomData( randomState, &userTime, sizeof( FILETIME ) );
	handle = GetCurrentProcess();
	GetProcessTimes( handle, &creationTime, &exitTime, &kernelTime, &userTime );
	addRandomData( randomState, &creationTime, sizeof( FILETIME ) );
	addRandomData( randomState, &exitTime, sizeof( FILETIME ) );
	addRandomData( randomState, &kernelTime, sizeof( FILETIME ) );
	addRandomData( randomState, &userTime, sizeof( FILETIME ) );

	/* Get assorted window handles and related information associated with 
	   the current thread */
#if VC_GE_2010( _MSC_VER )
	guiThreadInfo.cbSize = sizeof( GUITHREADINFO );
	GetGUIThreadInfo( 0, &guiThreadInfo );
	addRandomData( randomState, &guiThreadInfo, sizeof( GUITHREADINFO ) );
#endif /* VC++ >= 2010 */

	/* Get the minimum and maximum working set size for the current process */
	GetProcessWorkingSetSize( handle, &minimumWorkingSetSize,
							  &maximumWorkingSetSize );
	addRandomValue( randomState, minimumWorkingSetSize );
	addRandomValue( randomState, maximumWorkingSetSize );

	/* The following are fixed for the lifetime of the process so we only
	   add them once */
	if( !addedFixedItems )
		{
		STARTUPINFO startupInfo;

		/* Get name of desktop, console window title, new window position and
		   size, window flags, and handles for stdin, stdout, and stderr */
		startupInfo.cb = sizeof( STARTUPINFO );
		GetStartupInfo( &startupInfo );
		addRandomData( randomState, &startupInfo, sizeof( STARTUPINFO ) );

		/* Remember that we've got the fixed info */
		addedFixedItems = TRUE;
		}

	/* The performance of QPC varies depending on the architecture that it's 
	   running on and on the OS, the MS documentation is somewhat vague 
	   about the details because they vary so much.  Under Win9x/ME it read 
	   the 1.193180 MHz PIC timer.  Under Win32 it changed with almost every
	   OS update.
	   
	   Under the original Windows NT on machines with a uniprocessor HAL 
	   KeQueryPerformanceCounter() used the 3.579545MHz timer and on 
	   machines with a multiprocessor or APIC HAL it used the TSC (the exact 
	   time source was controlled by the HalpUse8254 flag in the kernel).  
	   This choice of time sources was somewhat peculiar because on a 
	   multiprocessor machine it was possible to get completely different 
	   TSC readings depending on which CPU the thread was currently running 
	   on while for uniprocessor machines this wasn't a problem.  This was 
	   dealt with by having the kernel synchronise the TSCs across CPUs at 
	   boot time if a multiprocessor HAL was used (it reset the TSC as part 
	   of its system init).
	   
	   By about Windows 2000/XP it was discovered that some multiprocessor 
	   systems couldn't have their TSCs synchronised in this manner, so you 
	   still got inconsistent results.
	   
	   Under Windows Vista, QPC was changed to use the HPET (which was 
	   mandated for the Vista hardware requirements) or, as a backup, the 
	   ACPI power management (PM) timer, which provided a single counter 
	   source but had a much higher access latency than the TSC.

	   Under Windows 7, the hardware requirements were set for constant-
	   rate TSCs (which is another issue with using the TSC, see below), so 
	   QPC went back to using the TSC rather than the HPET if possible, with
	   fallback to the HPET or PM timer if there were problems (like a non-
	   constant-rate TSC, or oddball stuff like live migration of VMs).

	   Under Windows 8 the TSC is almost always used, with a better TSC
	   synchronisation algorithm for multi-CPU systems, although even then
	   on large multiprocessor systems it may not be possible to synchronise
	   them, leading to the usual fallback to the HPET or PM timer.
	   
	   Another feature of the TSC that affects QPC but not our use is that 
	   mobile CPUs will turn off the TSC when they idle and newer CPUs with 
	   advanced power management/clock throttling would change the rate of 
	   the counter when they clock-throttle to match the current CPU speed, 
	   and some hyperthreading CPUs would turn it off when both threads were 
	   idle (this more or less makes sense since the CPU will be in the 
	   halted state and not executing any instructions to count).  This
	   behaviour is known as a non-invariant TSC.

	   What makes non-invariant TSCs even more exciting is that the 
	   resulting TSC output is somewhat nondeterministic, the only thing 
	   that's guaranteed is that the counter is monotonically incrementing.  
	   For example when a processor with power-saving features and a non-
	   invariant TSC ramps down its core clock the TSC rate is lowered to 
	   correspond to the clock rate.  However various events like cache 
	   probes can cause the core to temporarily return to the original rate 
	   to process the event before dropping back to the throttled rate, 
	   which can cause TSC drift across multiple cores.  Hardware-specific 
	   events like AMDs STPCLK signalling ("shut 'er down ma, she's glowing 
	   red") can reach different cores at different times and lead to 
	   ramping up and down and different times and for different durations, 
	   leading to further drift.  Updated AMD CPUs fixed this by providing 
	   invariant TSCs if the TscInvariant CPUID feature flag is set, and
	   more recent CPUs of all types have invariant TSCs by default.

	   As a result, if we're on a system with multiple CPUs and it doesn't 
	   have an invariant TSC then the TSC skew can also be a source of 
	   entropy.  To some extent this is just timing the context switch time 
	   across CPUs (which in itself is a source of some entropy), but what's 
	   left is the TSC skew across CPUs.  If this is if interest we can do 
	   it with code something like the following.  Note that this is only 
	   sample code, there are various complications such as the fact that 
	   another thread may change the process affinity mask between the call 
	   to the initial GetProcessAffinityMask() and the final 
	   SetThreadAffinityMask() to restore the original mask, even if we 
	   insert another GetProcessAffinityMask() just before the 
	   SetThreadAffinityMask() there's still a small race condition present 
	   but no real way to avoid it without OS API-level support:

		DWORD processAfinityMask, systemAffinityMask, mask = 0x10000;

		// Get the available processors that the thread can be scheduled on 
		if( !GetProcessAffinityMask( GetCurrentProcess(), processAfinityMask, 
									 systemAffinityMask ) )
			return;

		// Move the thread to each processor and read the TSC
		while( mask != 0 )
			{
			mask >>= 1;
			if( !( mask & processAfinityMask ) )
				continue;

			SetThreadAffinityMask( GetCurrentThread(), mask );
			// RDTSC
			}

		// Restore the original thread affinity
		SetThreadAffinityMask( GetCurrentThread(), processAfinityMask );

	   A final complication, which doesn't actually affect us but only 
	   causes problems when trying to count instruction timings, is that 
	   RDTSC is affected by out-of-order instruction execution, for which
	   newer CPUs have a RDTSCP instruction that performs a serialised
	   read.

	   To make things unambiguous, we call RDTSC directly if possible and 
	   fall back to QPC in the highly unlikely situation where this isn't 
	   present */
#if defined( __WIN64__ )
	{
	unsigned __int64 value;
	
	/* x86-64 always has a TSC, and the read is supported as an intrinsic */
	value = __rdtsc();
	addRandomData( randomState, &value, sizeof( __int64 ) );
	}
#else
  #ifndef NO_ASM
	if( getSysVar( SYSVAR_HWINTRINS ) & HWINTRINS_FLAG_RDTSC )
		{
		unsigned long value;

		__asm {
			xor eax, eax
			xor edx, edx		/* Tell VC++ that EDX:EAX will be trashed */
			rdtsc
			mov [value], eax	/* Ignore high 32 bits, which are > 1s res */
			}
		addRandomValue( randomState, value );
		}
  #endif /* NO_ASM */
	else
		{
		LARGE_INTEGER performanceCount;

		if( QueryPerformanceCounter( &performanceCount ) )
			{
			addRandomData( randomState, &performanceCount,
						   sizeof( LARGE_INTEGER ) );
			}
		else
			{
			/* Millisecond accuracy at best... */
			addRandomValue( randomState, GetTickCount() );
			}
		}
#endif /* Win32 vs. Win64 */

	/* If there's a hardware RNG present, read data from it.  In cases where 
	   there's an RNG status flag present, we check that the RNG is still 
	   present/enabled on each fetch since it could (at least in theory) be 
	   disabled by the OS between fetches.  We also read the data into an 
	   explicitly dword-aligned buffer (which the standard buffer should be 
	   anyway, but we make it explicit here just to be safe).  Note that we 
	   have to force alignment using a LONGLONG rather than a #pragma pack, 
	   since chars don't need alignment it would have no effect on the 
	   BYTE [] member */
#ifndef NO_ASM
	if( getSysVar( SYSVAR_HWINTRINS ) & HWINTRINS_FLAG_XSTORE )
		{
		/* VIA CPUs */
		struct alignStruct {
			LONGLONG dummy1;		/* Force alignment of following member */
			BYTE buffer[ 64 ];
			};
		struct alignStruct *rngBuffer = ( struct alignStruct * ) buffer;
		void *bufPtr = rngBuffer->buffer;	/* Get it into a form asm can handle */
		int byteCount = 0;

		/* VS 2015 will warn (incorrectly) about EBX being trashed, assuming 
		   that the presence of the CPUID instruction changes it even though 
		   the Centaur 0xC0000001 op only writes to EDX, and we save EBX 
		   around the call.  There's no easy way to disable this warning, 
		   #pragma warning( disable: 4713 ) only works on a per-function 
		   level which means that it'd need to be disabled for the entire 
		   function just to get rid of the spurious warning when cpuid is 
		   called (and a second time for the pop ebx).  In addition it's
		   not clear whether we can use the VS 2015-native __cpuid() in 
		   place of the inline asm because it takes the CPUID function type 
		   as an int parameter, implying that it's not expecting a value 
		   with the sign bit set (the docs imply that extended function
		   codes above 0x80000000 are permitted, but without a sample 
		   system to test this on any more it's safer to stick to the known-
		   good inline asm) */
		__asm {
			push es
			push ebx			/* Keep VC happy so it won't complain about 
								   corrupting EBX when built without frame 
								   pointer omission */
			mov eax, 0xC0000001	/* Centaur extended feature flags */
			cpuid
			and edx, 01100b
			cmp edx, 01100b		/* Check for RNG present + enabled flags */
			jne rngDisabled		/* RNG was disabled after our initial check */
			push ds
			pop es
			mov edi, bufPtr		/* ES:EDI = buffer */
			xor edx, edx		/* Fetch 8 bytes */
			xstore_rng
			and eax, 011111b	/* Get count of bytes returned */
			jz rngDisabled		/* Nothing read, exit */
			mov [byteCount], eax
		rngDisabled:
			pop ebx
			pop es
			}
		if( byteCount > 0 )
			{
			addRandomData( randomState, bufPtr, byteCount );
			trngValue = 45;
			}
		}
	if( getSysVar( SYSVAR_HWINTRINS ) & HWINTRINS_FLAG_RDRAND )
		{
		/* Intel and AMD CPUs */
		unsigned long rdrandBuffer[ 8 + 8 ];
		int byteCount = 0;

		__asm {
			xor eax, eax		/* Tell VC++ that EAX will be trashed */
			xor ecx, ecx
		trngLoop:
			rdrand_eax
			jnc trngExit		/* TRNG result bad, exit with byteCount = 0 */
			mov [rdrandBuffer+ecx], eax
			add ecx, 4
			cmp ecx, 32			/* Fill 32 bytes worth */
			jl trngLoop
			mov [byteCount], ecx
		trngExit:
			}
		if( byteCount > 0 )
			{
			addRandomData( randomState, rdrandBuffer, byteCount );
			trngValue = 45;
			}
		}
	if( getSysVar( SYSVAR_HWINTRINS ) & HWINTRINS_FLAG_TRNG )
		{
		/* AMD Geode LX.  The procedure for reading this is complex and
		   awkward, and probably only possible in kernel mode (see section
		   6.12.2 of the Geode LX data book).  In addition the CPU line was 
		   more or less abandoned in 2005 so we don't try and do anything 
		   with it */
#if 0
		unsigned long value = 0;

		__asm {
			xor eax, eax
			xor edx, edx		/* Tell VC++ that EDX:EAX will be trashed */
			mov ecx, 0x58002006	/* GLD_MSR_CTRL */
			rdmsr
			and eax, 0110000000000b	
			cmp eax, 0010000000000b	/* Check whether TRNG is enabled */
			jne trngDisabled	/* TRNG isn't enabled */
		trngDisabled:
			}
		if( value )
			{
			addRandomValue( randomState, value );
			trngValue = 20;
			}
#endif /* 0 */
		}
#endif /* NO_ASM */

	/* Flush any remaining data through.  Quality = int( 33 1/3 % ) + any 
	   additional quality from the TRNG if there's one present */
	endRandomData( randomState, 34 + trngValue );
	}

/****************************************************************************
*																			*
*									Slow Poll								*
*																			*
****************************************************************************/

/* Type definitions for functions to call Windows native API functions */

typedef DWORD ( WINAPI *NTQUERYSYSTEMINFORMATION )( DWORD systemInformationClass,
								PVOID systemInformation,
								ULONG systemInformationLength,
								PULONG returnLength );
typedef DWORD ( WINAPI *NTQUERYINFORMATIONPROCESS )( HANDLE processHandle,
								DWORD processInformationClass,
								PVOID processInformation,
								ULONG processInformationLength,
								PULONG returnLength );
typedef DWORD ( WINAPI *NTPOWERINFORMATION )( DWORD powerInformationClass,
								PVOID inputBuffer, ULONG inputBufferLength,
								PVOID outputBuffer, ULONG outputBufferLength );

static NTQUERYSYSTEMINFORMATION pNtQuerySystemInformation = NULL;
static NTQUERYINFORMATIONPROCESS pNtQueryInformationProcess = NULL;
static NTPOWERINFORMATION pNtPowerInformation = NULL;

/* When we query the performance counters, we allocate an initial buffer and
   then reallocate it as required until RegQueryValueEx() stops returning
   ERROR_MORE_DATA.  The following values define the initial buffer size and
   step size by which the buffer is increased */

#define PERFORMANCE_BUFFER_SIZE		65536	/* Start at 64K */
#define PERFORMANCE_BUFFER_STEP		16384	/* Step by 16K */

static void registryPoll( void )
	{
	static int cbPerfData = PERFORMANCE_BUFFER_SIZE;
	PPERF_DATA_BLOCK pPerfData;
	MESSAGE_DATA msgData;
	DWORD dwSize, dwStatus;
	int iterations = 0, status;

	/* Wait for any async keyset driver binding to complete.  You may be
	   wondering what this call is doing here... the reason why it's 
	   necessary is because RegQueryValueEx() will hang indefinitely if the 
	   async driver bind is in progress.  The problem occurs in the dynamic 
	   loading and linking of driver DLL's, which work as follows:

		hDriver = DynamicLoad( DRIVERNAME );
		pFunction1 = ( TYPE_FUNC1 ) GetProcAddress( hDriver, NAME_FUNC1 );
		pFunction2 = ( TYPE_FUNC1 ) GetProcAddress( hDriver, NAME_FUNC2 );

	   Under older Windows versions, if RegQueryValueEx() is called while 
	   the GetProcAddress()'s are in progress, it will hang indefinitely.  
	   This is probably due to some synchronisation problem in the Windows 
	   kernel where the GetProcAddress() calls affect something like a 
	   module reference count or function reference count while 
	   RegQueryValueEx() is trying to take a snapshot of the statistics, 
	   which include the reference counts.  Because of this we have to wait 
	   until any async driver bind has completed before we can call 
	   RegQueryValueEx() */
	if( !krnlWaitSemaphore( SEMAPHORE_DRIVERBIND ) )
		{
		/* The kernel is shutting down, bail out */
		return;
		}

	/* Get information from the system performance counters.  This can take a
	   few seconds to do.  In some environments the call to RegQueryValueEx()
	   can produce an access violation at some random time in the future, in
	   some cases adding a short delay after the following code block makes
	   the problem go away.  This problem is extremely difficult to
	   reproduce, I haven't been able to get it to occur despite running it
	   on a number of machines.  MS knowledge base article Q178887 covers
	   this type of problem, it's typically caused by an external driver or
	   other program that adds its own values under the
	   HKEY_PERFORMANCE_DATA key.  The NT kernel, via Advapi32.dll, calls the
	   required external module to map in the data inside an SEH try/except
	   block, so problems in the module's collect function don't pop up until
	   after it has finished, so the fault appears to occur in Advapi32.dll.
	   There may be problems in the NT kernel as well though, a low-level
	   memory checker indicated that ExpandEnvironmentStrings() in
	   Kernel32.dll, called an interminable number of calls down inside
	   RegQueryValueEx(), was overwriting memory (it wrote twice the
	   allocated size of a buffer to a buffer allocated by the NT kernel).
	   OTOH this could be coming from the external module calling back into
	   the kernel, which eventually causes the problem described above.

	   Possibly as an extension of the problem that the krnlWaitSemaphore()
	   call above works around, running two instances of cryptlib (e.g. two
	   applications that use it) under NT4 can result in one of them hanging
	   in the RegQueryValueEx() call.  This happens only under NT4 and is
	   hard to reproduce in any consistent manner.

	   One workaround that helps a bit is to read the registry as a remote
	   (rather than local) registry, it's possible that the use of a network
	   RPC call isolates the calling app from the problem in that whatever
	   service handles the RPC is taking the hit and not affecting the
	   calling app.  Since this would require another round of extensive
	   testing to verify and the Windows native API call is working fine, 
	   we'll stick with the native API call for now.

	   Some versions of NT4 had a problem where the amount of data returned
	   was mis-reported and would never settle down, because of this the code
	   below includes a safety-catch that bails out after 10 attempts have
	   been made, this results in no data being returned but at does ensure
	   that the thread will terminate.

	   In addition to these problems the code in RegQueryValueEx() that
	   estimates the amount of memory required to return the performance
	   counter information isn't very accurate (it's much worse than the
	   "slightly-inaccurate" level that the MS docs warn about, it's usually
	   wildly off) since it always returns a worst-case estimate which is
	   usually nowhere near the actual amount required.  For example it may
	   report that 128K of memory is required, but only return 64K of data.

	   Even worse than the registry-based performance counters is the
	   performance data helper (PDH) shim that tries to make the counters
	   look like the old Win16 API (which is also used by Win95).  Under 
	   Win32 this can consume tens of MB of memory and huge amounts of CPU 
	   time while it gathers its data, and even running once can still 
	   consume about 1/2MB of memory.

	   "Windows NT is a thing of genuine beauty, if you're seriously into 
	    genuine ugliness.  It's like a woman with a history of insanity in 
		the family, only worse" -- Hans Chloride, "Why I Love Windows NT" */
	REQUIRES_V( rangeCheck( cbPerfData, 1, PERFORMANCE_BUFFER_SIZE ) );
	pPerfData = ( PPERF_DATA_BLOCK ) clAlloc( "registryPoll", cbPerfData );
	while( pPerfData != NULL && iterations++ < 10 )
		{
		dwSize = cbPerfData;
		dwStatus = RegQueryValueEx( HKEY_PERFORMANCE_DATA, "Global", NULL,
									NULL, ( LPBYTE ) pPerfData, &dwSize );
		if( dwStatus == ERROR_SUCCESS )
			{
			if( !memcmp( pPerfData->Signature, L"PERF", 8 ) )
				{
				static const int quality = 100;

				setMessageData( &msgData, pPerfData, dwSize );
				status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
										  IMESSAGE_SETATTRIBUTE_S, &msgData,
										  CRYPT_IATTRIBUTE_ENTROPY );
				if( cryptStatusOK( status ) )
					{
					( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE,
											  IMESSAGE_SETATTRIBUTE,
											  ( MESSAGE_CAST ) &quality,
											  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
					}
				}
			clFree( "registryPoll", pPerfData );
			pPerfData = NULL;
			}
		else
			{
			if( dwStatus == ERROR_MORE_DATA )
				{
				PPERF_DATA_BLOCK pTempPerfData;
				
				cbPerfData += PERFORMANCE_BUFFER_STEP;
				pTempPerfData = ( PPERF_DATA_BLOCK ) realloc( pPerfData, cbPerfData );
				if( pTempPerfData == NULL )
					{
					/* The realloc failed, free the original block and force 
					   a loop exit */
					clFree( "registryPoll", pPerfData );
					pPerfData = NULL;
					}
				else
					pPerfData = pTempPerfData;
				}
			}
		}

	/* Although this isn't documented in the Win32 API docs, it's necessary to
	   explicitly close the HKEY_PERFORMANCE_DATA key after use (it's
	   implicitly opened on the first call to RegQueryValueEx()).  If this
	   isn't done then any system components that provide performance data
	   can't be removed or changed while the handle remains active */
	RegCloseKey( HKEY_PERFORMANCE_DATA );
	}

static void slowPollWindows( void )
	{
	static BOOLEAN addedFixedItems = FALSE;
	static int isWorkstation = CRYPT_ERROR;
	MESSAGE_DATA msgData;
	HANDLE hDevice;
	LPBYTE lpBuffer;
	ULONG ulSize;
	DWORD dwType, dwSize, dwResult;
	void *buffer;
	int nDrive, noResults = 0, status, LOOP_ITERATOR;

	/* Find out whether this is a server or workstation if necessary */
	if( isWorkstation == CRYPT_ERROR )
		{
		HKEY hKey;

		if( RegOpenKeyEx( HKEY_LOCAL_MACHINE,
						  "SYSTEM\\CurrentControlSet\\Control\\ProductOptions",
						  0, KEY_READ, &hKey ) == ERROR_SUCCESS )
			{
			BYTE szValue[ 32 + 8 ];
			dwSize = 32;

			isWorkstation = TRUE;
			if( RegQueryValueEx( hKey, "ProductType", 0, NULL, szValue,
								 &dwSize ) == ERROR_SUCCESS && \
				dwSize >= 5 && strCompare( szValue, "WinNT", 5 ) )
				{
				/* Note: There are (at least) three cases for ProductType:
				   WinNT = NT Workstation, ServerNT = NT Server, LanmanNT =
				   NT Server acting as a Domain Controller */
				isWorkstation = FALSE;
				}

			RegCloseKey( hKey );
			}
		}

	/* The following are fixed for the lifetime of the process so we only
	   add them once */
	if( !addedFixedItems )
		{
		readPnPData();
		readACPIData();
		readNetworkData();
		addedFixedItems = TRUE;
		}

	/* Initialize the Windows native API function pointers if necessary */
	if( hNTAPI == NULL && \
		( hNTAPI = GetModuleHandle( "NTDll.dll" ) ) != NULL )
		{
		/* Get a pointer to the Windows native information query functions */
		pNtQuerySystemInformation = ( NTQUERYSYSTEMINFORMATION ) \
						GetProcAddress( hNTAPI, "NtQuerySystemInformation" );
		pNtQueryInformationProcess = ( NTQUERYINFORMATIONPROCESS ) \
						GetProcAddress( hNTAPI, "NtQueryInformationProcess" );
		if( pNtQuerySystemInformation == NULL || \
			pNtQueryInformationProcess == NULL )
			hNTAPI = NULL;
		pNtPowerInformation = ( NTPOWERINFORMATION ) \
						GetProcAddress( hNTAPI, "NtPowerInformation" );
		}
	if( krnlIsExiting() )
		return;

	/* Get network statistics.  Both workstations and servers by default 
	   will be running both the workstation and server services, we always 
	   get the workstation statistics since the returned structure, a 
	   STAT_WORKSTATION_0, contains vastly more information than the 
	   equivalent STAT_SERVER_0 */
#if VC_GE_2010( _MSC_VER )
	#pragma warning( push )
	#pragma warning( disable:28159 )	/* Incorrect annotation in lmstats.h */
#endif /* VC++ >= 2010 */
	if( NetStatisticsGet( NULL, isWorkstation ? SERVICE_WORKSTATION : \
												SERVICE_SERVER, 0, 0, 
						  &lpBuffer ) == NERR_Success )
		{
		if( NetApiBufferSize( lpBuffer, &dwSize ) == NERR_Success )
			{
			setMessageData( &msgData, lpBuffer, dwSize );
			krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
							 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
			}
		NetApiBufferFree( lpBuffer );
		}
#if VC_GE_2010( _MSC_VER )
	#pragma warning( pop )
#endif /* VC++ >= 2010 */

	/* Get disk I/O statistics for all the hard drives */
	LOOP_MED( nDrive = 0, nDrive < 20, nDrive++ )
		{
		BYTE diskPerformance[ 256 + 8 ];
		char szDevice[ 32 + 8 ];

		/* Check whether we can access this device */
		sprintf_s( szDevice, 32, "\\\\.\\PhysicalDrive%d", nDrive );
		hDevice = CreateFile( szDevice, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
							  NULL, OPEN_EXISTING, 0, NULL );
		if( hDevice == INVALID_HANDLE_VALUE )
			break;

		/* Note: This only works if the user has turned on the disk
		   performance counters with 'diskperf -y'.  These counters were
		   traditionally disabled under Windows NT, although in newer 
		   installs of Win2K and newer the physical disk object is enabled 
		   by default while the logical disk object is disabled by default 
		   ('diskperf -yv' turns on the counters for logical drives in this 
		   case, since they're already on for physical drives).
		   
		   In addition using the documented DISK_PERFORMANCE data structure 
		   to contain the returned data returns ERROR_INSUFFICIENT_BUFFER 
		   (which is wrong) and doesn't change dwSize (which is also wrong) 
		   so we pass in a larger buffer and pre-set dwSize to a safe 
		   value.  Finally, there's a bug in pre-SP4 Win2K in which enabling 
		   diskperf, installing a file system filter driver, and then 
		   disabling diskperf, causes diskperf to corrupt the registry key 
		   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\
		   {71A27CDD-812A-11D0-BEC7-08002BE2092F}\Upper Filters, resulting 
		   in a Stop 0x7B bugcheck */
		dwSize = sizeof( diskPerformance );
		if( DeviceIoControl( hDevice, IOCTL_DISK_PERFORMANCE, NULL, 0,
							 &diskPerformance, 256, &dwSize, NULL ) )
			{
			if( krnlIsExiting() )
				{
				CloseHandle( hDevice );
				return;
				}
			setMessageData( &msgData, &diskPerformance, dwSize );
			krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
							 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
			}
		CloseHandle( hDevice );
		}
	ENSURES_V( LOOP_BOUND_OK );
	if( krnlIsExiting() )
		return;

	/* In theory we should be using the Win32 performance query API to obtain
	   unpredictable data from the system, however this is so unreliable (see
	   the multiple sets of comments in registryPoll()) that it's too risky
	   to rely on it except as a fallback in emergencies.  Instead, we rely
	   mostly on the Windows native API function NtQuerySystemInformation(), 
	   which has the dual advantages that it doesn't have as many (known) 
	   problems as the Win32 equivalent and that it doesn't access the data 
	   indirectly via pseudo-registry keys, which means that it's much faster.  
	   Note that the Win32 equivalent actually works almost all of the time, 
	   the problem is that on one or two systems it can fail in strange ways 
	   that are never the same and can't be reproduced on any other system, 
	   which is why we use the native API here.  Microsoft officially 
	   documented this function in early 2003, so it'll be fairly safe to 
	   use */
	if( ( hNTAPI == NULL ) || \
		( buffer = clAlloc( "slowPollWindows", \
							PERFORMANCE_BUFFER_SIZE ) ) == NULL )
		{
		registryPoll();
		return;
		}

	/* Clear the buffer before use.  We have to do this because even though 
	   NtQuerySystemInformation() tells us that it's filled in ulSize bytes, 
	   it doesn't necessarily mean that it actually has provided that much 
	   data */
	memset( buffer, 0, PERFORMANCE_BUFFER_SIZE );

	/* Scan the first 64 possible information types (we don't bother with
	   increasing the buffer size as we do with the Win32 version of the
	   performance data read, we may miss a few classes but it's no big deal).
	   This scan typically yields around 20 pieces of data, there's nothing
	   in the range 65...128 so chances are there won't be anything above
	   there either */
	LOOP_LARGE( dwType = 0, dwType < 64, dwType++ )
		{
		/* Some information types are write-only (the IDs are shared with
		   a set-information call), we skip these */
		if( dwType == 26 || dwType == 27 || dwType == 38 || \
			dwType == 46 || dwType == 47 || dwType == 48 || \
			dwType == 52 )
			continue;

		/* ID 53 = SystemSessionProcessInformation reads input from the
		   output buffer, which has to contain a session ID and pointer
		   to the actual buffer in which to store the session information.
		   Because this isn't a standard query, we skip this */
		if( dwType == 53 )
			continue;

		/* Query the info for this ID.  Some results (for example for
		   ID = 6, SystemCallCounts) are only available in checked builds
		   of the kernel.  A smaller subcless of results require that
		   certain system config flags be set, for example
		   SystemObjectInformation requires that the
		   FLG_MAINTAIN_OBJECT_TYPELIST be set in NtGlobalFlags.  To avoid
		   having to special-case all of these, we try reading each one and
		   only use those for which we get a success status */
		dwResult = pNtQuerySystemInformation( dwType, buffer,
											  PERFORMANCE_BUFFER_SIZE - 2048,
											  &ulSize );
		if( dwResult != ERROR_SUCCESS )
			continue;

		/* Some calls (e.g. ID = 23, SystemProcessorStatistics, and ID = 24,
		   SystemDpcInformation) incorrectly return a length of zero, so we
		   manually adjust the length to the correct value */
		if( ulSize == 0 )
			{
			if( dwType == 23 )
				ulSize = 6 * sizeof( ULONG );
			if( dwType == 24 )
				ulSize = 5 * sizeof( ULONG );
			}

		/* If we got some data back, add it to the entropy pool.  Note that 
		   just because NtQuerySystemInformation() tells us that it's 
		   provided ulSize bytes doesn't necessarily mean that it has, see 
		   the comment after the malloc() for details */
		if( ulSize > 0 && ulSize <= PERFORMANCE_BUFFER_SIZE - 2048 )
			{
			if( krnlIsExiting() )
				{
				clFree( "slowPollWindows", buffer );
				return;
				}
			setMessageData( &msgData, buffer, ulSize );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_SETATTRIBUTE_S, &msgData,
									  CRYPT_IATTRIBUTE_ENTROPY );
			if( cryptStatusOK( status ) )
				noResults++;
			}
		}
	ENSURES_V( LOOP_BOUND_OK );

	/* Now do the same for the process information.  This call is rather ugly
	   in that it requires an exact length match for the data returned,
	   failing with a STATUS_INFO_LENGTH_MISMATCH error code (0xC0000004) if
	   the length isn't an exact match */
#if 0	/* This requires compiler-level assistance to handle complex nested 
		   structs, alignment issues, and so on.  Without the headers in 
		   which the entries are declared it's almost impossible to do */
	LOOP_LARGE( dwType = 0, dwType < 32, dwType++ )
		{
		static const struct { int type; int size; } processInfo[] = {
			{ CRYPT_ERROR, CRYPT_ERROR }
			};
		int i, LOOP_ITERATOR_ALT;

		LOOP_SMALL_ALT( i = 0, processInfo[ i ].type != CRYPT_ERROR, i++ )
			{
			/* Query the info for this ID */
			dwResult = pNtQueryInformationProcess( GetCurrentProcess(),
											processInfo[ i ].type, buffer,
											processInfo[ i ].size, &ulSize );
			if( dwResult != ERROR_SUCCESS )
				continue;

			/* If we got some data back, add it to the entropy pool */
			if( ulSize > 0 && ulSize <= PERFORMANCE_BUFFER_SIZE - 2048 )
				{
				if( krnlIsExiting() )
					{
					clFree( "slowPollWindows", buffer );
					return;
					}
				setMessageData( &msgData, buffer, ulSize );
				status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
										  IMESSAGE_SETATTRIBUTE_S, &msgData,
										  CRYPT_IATTRIBUTE_ENTROPY );
				if( cryptStatusOK( status ) )
					noResults++;
				}
			}
		ENSURES_V( LOOP_BOUND_OK_ALT );
		}
	ENSURES_V( LOOP_BOUND_OK );
#endif /* 0 */

	/* Finally do the same for the system power status information.  There 
	   are only a limited number of useful information types available so we
	   restrict ourselves to the useful types.  In addition since this
	   function doesn't return length information we have to hardcode in
	   length data */
	if( pNtPowerInformation != NULL )
		{
		static const struct { int type; int size; } powerInfo[] = {
			{ 0, 128 },	/* SystemPowerPolicyAc */
			{ 1, 128 },	/* SystemPowerPolicyDc */
			{ 4, 64 },	/* SystemPowerCapabilities */
			{ 5, 48 },	/* SystemBatteryState */
			{ 11, 48 },	/* ProcessorInformation */
			{ 12, 24 },	/* SystemPowerInformation */
			{ CRYPT_ERROR, CRYPT_ERROR }
			};
		int i;

		LOOP_SMALL( i = 0, powerInfo[ i ].type != CRYPT_ERROR, i++ )
			{
			/* Query the info for this ID */
			dwResult = pNtPowerInformation( powerInfo[ i ].type, NULL, 0, buffer,
											PERFORMANCE_BUFFER_SIZE - 2048 );
			if( dwResult != ERROR_SUCCESS )
				continue;
			if( krnlIsExiting() )
				{
				clFree( "slowPollWindows", buffer );
				return;
				}
			setMessageData( &msgData, buffer, powerInfo[ i ].size );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_SETATTRIBUTE_S, &msgData,
									  CRYPT_IATTRIBUTE_ENTROPY );
			if( cryptStatusOK( status ) )
				noResults++;
			}
		ENSURES_V( LOOP_BOUND_OK );
		}
	clFree( "slowPollWindows", buffer );

	/* If we got enough data, we can leave now without having to try for a
	   Win32-level performance information query */
	if( noResults > 15 )
		{
		static const int quality = 100;

		if( krnlIsExiting() )
			return;
		( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &quality,
								  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
		return;
		}
	if( krnlIsExiting() )
		return;

	/* We couldn't get enough results from the kernel, fall back to the
	   somewhat troublesome registry poll */
	registryPoll();
	}

/* Perform a thread-safe slow poll */

unsigned __stdcall threadSafeSlowPollWindows( void *dummy )
	{
#if 0
	typedef BOOL ( WINAPI *CREATERESTRICTEDTOKEN )( HANDLE ExistingTokenHandle,
								DWORD Flags, DWORD DisableSidCount,
								PSID_AND_ATTRIBUTES SidsToDisable,
								DWORD DeletePrivilegeCount,
								PLUID_AND_ATTRIBUTES PrivilegesToDelete,
								DWORD RestrictedSidCount,
								PSID_AND_ATTRIBUTES SidsToRestrict,
								PHANDLE NewTokenHandle );
	static CREATERESTRICTEDTOKEN pCreateRestrictedToken = NULL;
	static BOOLEAN isInited = FALSE;
#endif /* 0 */

	UNUSED_ARG( dummy );

	/* If the poll performed any kind of active operation like the Unix one
	   rather than just basic data reads it'd be a good idea to drop 
	   privileges before we begin, something that can be performed by the
	   following code.  Note though that this creates all sorts of 
	   complications when cryptlib is running as a service and/or performing
	   impersonation, which is why it's disabled by default */
#if 0
	if( !isInited )
		{
		/* Since CreateRestrictedToken() is a Win2K function we can only use
		   it on a post-NT4 system, and have to bind it at runtime */
		if( getSysVar( SYSVAR_OSVERSION ) > 4 )
			{
			const HINSTANCE hAdvAPI32 = GetModuleHandle( "AdvAPI32.dll" );

			pCreateRestrictedToken = ( CREATERESTRICTEDTOKEN ) \
						GetProcAddress( hAdvAPI32, "CreateRestrictedToken" );
			}
		isInited = TRUE;
		}
	if( pCreateRestrictedToken != NULL )
		{
		HANDLE hToken, hNewToken;

		ImpersonateSelf( SecurityImpersonation );
		OpenThreadToken( GetCurrentThread(),
						 TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | \
						 TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | \
						 TOKEN_IMPERSONATE, TRUE, &hToken );
		CreateRestrictedToken( hToken, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL,
							   0, NULL, &hNewToken );
		SetThreadToken( &hThread, hNewToken );
		}
#endif /* 0 */

	slowPollWindows();
#if 0
	if( pCreateRestrictedToken != NULL )
		RevertToSelf();
#endif /* 0 */
	_endthreadex( 0 );
	return( 0 );
	}

/* Perform a generic slow poll.  This starts the OS-specific poll in a
   separate thread */

void slowPoll( void )
	{
	if( krnlIsExiting() )
		return;

	/* Read data from various hardware sources */
	readSystemRNG();
	readTPMRNG();
	readExternalRNG();
	readMBMData();
	readEverestData();
	readSysToolData();
	readRivaTunerData();
	readHMonitorData();
	readATITrayToolsData();
	readCoreTempData();
	readGPUZData();

	/* Start a threaded slow poll.  If a slow poll is already running, we
	   just return since there isn't much point in running two of them at the
	   same time */
	if( cryptStatusError( krnlEnterMutex( MUTEX_RANDOM ) ) )
		return;
	if( hThread != NULL )
		{
		krnlExitMutex( MUTEX_RANDOM );
		return;
		}

	/* In theory since the thread is gathering info used (eventually) for 
	   crypto keys we could set an ACL on the thread to make it explicit 
	   that no-one else can mess with it:

		void *aclInfo = initACLInfo( THREAD_ALL_ACCESS );

		hThread = ( HANDLE ) _beginthreadex( getACLInfo( aclInfo ),
											 0, threadSafeSlowPollWindows,
											 NULL, 0, &threadID );
		freeACLInfo( aclInfo );

	  However, although this is supposed to be the default access for 
	  threads anyway, when used from a service (running under the 
	  LocalSystem account) under Win2K SP4 and up, the thread creation fails 
	  with error = 22 (invalid parameter).  Presumably MS patched some 
	  security hole or other in SP4, which causes the thread creation to 
	  fail.  Because of this problem, we don't set an ACL for the thread */
	hThread = ( HANDLE ) _beginthreadex( NULL, 0,
										 threadSafeSlowPollWindows,
										 NULL, 0, &threadID );
	krnlExitMutex( MUTEX_RANDOM );
	assert( hThread );
	}

/* Wait for the randomness gathering to finish.  Anything that requires the
   gatherer process to have completed gathering entropy should call
   waitforRandomCompletion(), which will block until the background process
   completes */

CHECK_RETVAL \
int waitforRandomCompletion( const BOOLEAN force )
	{
	DWORD dwResult;
	const DWORD timeout = force ? 2000 : 300000L;
	int status;

	REQUIRES( force == TRUE || force == FALSE );

	/* If there's no polling thread running, there's nothing to do.  Note
	   that this isn't entirely thread-safe because someone may start
	   another poll after we perform this check, but there's no way to
	   handle this without some form of interlock mechanism with the
	   randomness mutex and the WaitForSingleObject().  In any case all
	   that'll happen is that the caller won't get all of the currently-
	   polling entropy */
	if( hThread == NULL )
		return( CRYPT_OK );

	/* Wait for the polling thread to terminate.  If it's a forced shutdown
	   we only wait a short amount of time (2s) before we bail out, 
	   otherwise we hang around for as long as it takes (with a sanity-check
	   upper limit of 5 minutes) */
	dwResult = WaitForSingleObject( hThread, timeout );
	if( dwResult != WAIT_OBJECT_0 )
		{
		/* Since this is a cleanup function there's not much that we can do 
		   at this point, although we warn in debug mode */
		DEBUG_DIAG(( "Error %lX waiting for object", dwResult ));
		assert( DEBUG_WARN );
		return( CRYPT_OK );
		}
	assert( dwResult != WAIT_FAILED );	/* Warn in debug mode */

	/* Clean up */
	status = krnlEnterMutex( MUTEX_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	if( hThread != NULL )
		{
		CloseHandle( hThread );
		hThread = NULL;
		}
	krnlExitMutex( MUTEX_RANDOM );

	return( CRYPT_OK );
	}

/* Initialise and clean up any auxiliary randomness-related objects */

void initRandomPolling( void )
	{
	/* Reset the various module and object handles and status info and
	   initialise the system and external RNG interfaces if they're 
	   present */
	hAdvAPI32 = hThread = NULL;
	initSystemRNG();
	initExternalRNG();
	}

void endRandomPolling( void )
	{
	assert( hThread == NULL );
	endSystemRNG();
	endExternalRNG();
	}
