/****************************************************************************
*																			*
*						cryptlib Internal Debugging API						*
*						Copyright Peter Gutmann 1992-2013					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "stream.h"
#else
  #include "crypt.h"
  #include "io/stream.h"
#endif /* Compiler-specific includes */

/* The following functions are intended purely for diagnostic purposes 
   during development.  They perform minimal checking (for example using 
   assertions rather than returning error codes, since the calling code 
   can't hardwire in tests for their return status), and should only
   be used with a debugger */

/****************************************************************************
*																			*
*							Diagnostic-dump Functions						*
*																			*
****************************************************************************/

#if !defined( NDEBUG ) || defined( DEBUG_DIAGNOSTIC_ENABLE ) 

/* Older versions of the WinCE runtime don't provide complete stdio
   support so we have to emulate it using wrappers for native 
   functions */

#if defined( __WINCE__ ) && _WIN32_WCE < 500

int remove( const char *pathname )
	{
	wchar_t wcBuffer[ _MAX_PATH + 1 ];

	mbstowcs( wcBuffer, pathname, strlen( pathname ) + 1 );
	DeleteFile( wcBuffer );

	return( 0 );
	}
#endif /* WinCE < 5.x doesn't have remove() */

/* Output text to the debug console or whatever the OS'es nearest equivalent
   is.  If possible this is normally done via a macro in a header file that 
   remaps the debug-output macros to the appropriate function but Windows 
   only provides a puts()-equivalent and not a printf()-equivalent and under 
   Unix we need to send the output to stderr which can't easily be done in a 
   macro.  In addition OutputDebugString() under Windows isn't very reliable,
   it's implemented using a 4kB chunk of shared memory DBWIN_BUFFER 
   controlled by a mutex DBWinMutex and two events DBWIN_BUFFER_READY and
   DBWIN_DATA_READY.  OutputDebugString() waits for exclusive access to 
   DBWinMutex, maps in DBWIN_BUFFER, waits for DBWIN_BUFFER_READY to be 
   signalled, copies in up to 4K of data, fires DBWIN_DATA_READY, and
   releases the mutex.  If any of these operations fail, the call to
   OutputDebugString() is treated as a no-op and we never see the output.  
   On the receiving side the debugger performs the obvious actions to 
   receive the data.  Beyond this Rube-Goldberg message-passing mechanism
   there are also problems with permissions on the mutex and the way the
   DACL for it has been tweaked in almost every OS release.  The end result
   of this is that data sent to OutputDebugString() may be randomly lost
   if other threads are also sending data to it (e.g. Visual Studio as part
   of its normal chattiness about modules and threads being loaded/started/
   unloaded/stopped/etc).  The only way to avoid this is to step through
   the code using OutputDebugString() so that enough delay is inserted to
   allow other callers and the code being debugged to get out of each
   others' hair */

#if defined( __WIN32__ )

int debugPrintf( const char *format, ... )
	{
	va_list argPtr;
	char buffer[ 1024 ];
	int length;

	va_start( argPtr, format );

#if VC_GE_2005( _MSC_VER )
	length = vsnprintf_s( buffer, 1024, _TRUNCATE, format, argPtr );
#else
	length = vsprintf( buffer, format, argPtr );
#endif /* VC++ 2005 or newer */
	va_end( argPtr );
	OutputDebugString( buffer );

	return( length );
	}

#elif defined( __UNIX__ )

#include <stdarg.h>			/* Needed for va_list */

int debugPrintf( const char *format, ... )
	{
	va_list argPtr;
	int length;

	va_start( argPtr, format );
	length = vfprintf( stderr, format, argPtr );
	va_end( argPtr );

	return( length );
	}
#endif /* OS-specific debug output functions */

/* Dump a PDU to disk */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void buildFilePath( IN_STRING const char *fileName,
						   OUT_BUFFER_FIXED_C( 1024 ) char *filenameBuffer )
	{
	int i;

	assert( isReadPtr( fileName, 2 ) );
	assert( isWritePtr( filenameBuffer, 1024 ) );

	ANALYSER_HINT_STRING( fileName );

	/* Check whether the filename appears to be an absolute path */
	for( i = 0; fileName[ i ] != '\0'; i++ )
		{
		if( fileName[ i ] == ':' || fileName[ i ] == '/' )
			break;
		}
	if( fileName[ i ] == '\0' )
		{
		/* It's a relative path, put the file in the temp directory */
		strlcpy_s( filenameBuffer, 1024, "/tmp/" );
		strlcat_s( filenameBuffer, 1024, fileName );
		}
	else
		strlcpy_s( filenameBuffer, 1024, fileName );
	strlcat_s( filenameBuffer, 1024, ".der" );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void debugDumpFile( IN_STRING const char *fileName, 
					IN_BUFFER( dataLength ) const void *data, 
					IN_LENGTH_SHORT const int dataLength )
	{
	FILE *filePtr;
	char filenameBuffer[ 1024 ];
	int count DUMMY_INIT;

	assert( isReadPtr( fileName, 2 ) );
	assert( isReadPtr( data, dataLength ) );

	ANALYSER_HINT_STRING( fileName );

	buildFilePath( fileName, filenameBuffer );
#ifdef __STDC_LIB_EXT1__
	if( fopen_s( &filePtr, filenameBuffer, "wb" ) != 0 )
		filePtr = NULL;
#else
	filePtr = fopen( filenameBuffer, "wb" );
#endif /* __STDC_LIB_EXT1__ */
	assert( filePtr != NULL );
	if( filePtr == NULL )
		return;
	if( dataLength > 0 )
		{
		count = fwrite( data, 1, dataLength, filePtr );
		assert( count == dataLength );
		}
	fclose( filePtr );
	if( dataLength > 0 && count < dataLength )
		( void ) remove( filenameBuffer );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void debugDumpFileCert( IN_STRING const char *fileName, 
						IN_HANDLE const CRYPT_CERTIFICATE iCryptCert )
	{
	MESSAGE_DATA msgData;
	FILE *filePtr;
	BYTE certData[ 2048 ];
	char filenameBuffer[ 1024 ];
	int count DUMMY_INIT, status;

	assert( isReadPtr( fileName, 2 ) );
	assert( isHandleRangeValid( iCryptCert ) );

	ANALYSER_HINT_STRING( fileName );

	buildFilePath( fileName, filenameBuffer );
#ifdef __STDC_LIB_EXT1__
	if( fopen_s( &filePtr, filenameBuffer, "wb" ) != 0 )
		filePtr = NULL;
#else
	filePtr = fopen( filenameBuffer, "wb" );
#endif /* __STDC_LIB_EXT1__ */
	assert( filePtr != NULL );
	if( filePtr == NULL )
		return;
	setMessageData( &msgData, certData, 2048 );
	status = krnlSendMessage( iCryptCert, IMESSAGE_CRT_EXPORT, &msgData, 
							  CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusOK( status ) )
		{
		count = fwrite( certData, 1, msgData.length, filePtr );
		assert( count == msgData.length );
		}
	fclose( filePtr );
	if( cryptStatusError( status ) || count < msgData.length )
		( void ) remove( filenameBuffer );
	}

/* Create a hex dump of the first n bytes of a buffer along with the length 
   and a checksum of the entire buffer, used to output a block of hex data 
   along with checksums for debugging things like client/server sessions 
   where it can be used to detect data corruption.  The use of a memory 
   buffer is to allow the hex dump to be performed from multiple threads 
   without them fighting over stdout */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void debugDumpHex( IN_STRING const char *prefixString, 
				   IN_BUFFER( dataLength ) const void *data, 
				   IN_LENGTH_SHORT const int dataLength )
	{
	char dumpBuffer[ 128 ];
	int offset, i, j;

	ANALYSER_HINT_STRING( prefixString );

	offset = sprintf_s( dumpBuffer, 128, "%3s %4d %04X ", prefixString, 
						dataLength, checksumData( data, dataLength ) );
	for( i = 0; i < dataLength; i += 16 )
		{
		const int innerLen = min( dataLength - i, 16 );

		if( i > 0 )
			{
			offset = sprintf_s( dumpBuffer, 128, "%3s           ",
								prefixString );
			}
		for( j = 0; j < innerLen; j++ )
			{
			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "%02X ",
								 ( ( BYTE * ) data )[ i + j ] );
			}
		for( ; j < 16; j++ )
			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "   " );
		for( j = 0; j < innerLen; j++ )
			{
			const BYTE ch = ( ( BYTE * ) data )[ i + j ];

			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "%c",
								 isprint( ch ) ? ch : '.' );
			}
		strcpy_s( dumpBuffer + offset, 128 - offset, "\n" );
		DEBUG_OUT( dumpBuffer );
		}

#if !defined( __WIN32__ ) || defined( __WINCE__ ) || defined( __ECOS__ )
	fflush( stdout );
#endif /* Systems where output doesn't to go stdout */
	}

/* Variants of debugDumpHex() that only output the raw hex data, for use in
   conjunction with DEBUG_PRINT() to output other information about the 
   data */

STDC_NONNULL_ARG( ( 1 ) ) \
void debugDumpData( IN_BUFFER( dataLength ) const void *data, 
					IN_LENGTH_SHORT const int dataLength )
	{
	char dumpBuffer[ 128 ];
	int offset, i, j;

	for( i = 0; i < dataLength; i += 16 )
		{
		const int innerLen = min( dataLength - i, 16 );

		offset = sprintf_s( dumpBuffer, 128, "%04d: ", i );
		for( j = 0; j < innerLen; j++ )
			{
			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "%02X ",
								 ( ( BYTE * ) data )[ i + j ] );
			}
		for( ; j < 16; j++ )
			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "   " );
		for( j = 0; j < innerLen; j++ )
			{
			const BYTE ch = ( ( BYTE * ) data )[ i + j ];

			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "%c",
								 isprint( ch ) ? ch : '.' );
			}
		strcpy_s( dumpBuffer + offset, 128 - offset, "\n" );
		DEBUG_OUT( dumpBuffer );
		}

#if !defined( __WIN32__ ) || defined( __WINCE__ ) || defined( __ECOS__ )
	fflush( stdout );
#endif /* Systems where output doesn't to go stdout */
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void debugDumpStream( INOUT /*STREAM*/ void *streamPtr, 
					  IN_LENGTH const int position,
					  IN_LENGTH const int length )
	{
	STREAM *stream = streamPtr;
	BYTE *dataPtr; 
	int status; 

	/* In some cases we may be asked to dump zero-length packets, since 
	   we're being called unconditionally (the caller can't put conditional
	   code into a debug macro) we just turn the call into a no-op */
	if( length <= 0 )
		return;

	status = sMemGetDataBlockAbs( stream, position, ( void ** ) &dataPtr, 
								  length );
	if( cryptStatusError( status ) )
		return;
	ANALYSER_HINT_V( dataPtr != NULL );
	debugDumpData( dataPtr, length );
	}

/* Dump a stack trace.  Note that under Unix this may require linking with 
   -rdynamic (to force the addition of all symbols, not just public ones, so 
   for example ones for static functions) in order for backtrace_symbols() 
   to be able to display all symbols */

#if defined( __WIN32__ )

#ifndef _WIN64

#include <ImageHlp.h>
#pragma comment( lib, "imagehlp.lib" )

void displayBacktrace( void )
	{
	HANDLE process;
	IMAGEHLP_SYMBOL *symbolInfo;
	BYTE buffer[ sizeof( IMAGEHLP_SYMBOL ) + 1024 ];
	unsigned long prevAddress, address = 1;
	int i;

	/* Horribly nonportable way of walking the stack */
	__asm { mov prevAddress, ebp };

	/* Load debugging symbols for the current process */
	printf( "\nStack trace:\n" );
	process = GetCurrentProcess();
	if( !SymInitialize( process, 
						"D:\\Work\\cryptlib\\debug32_vc6;D:\\Work\\cryptlib\\binaries32_vc6", 
						TRUE ) )
		{
		const DWORD error = GetLastError();
		
		printf( "Couldn't load symbols, error = %d.\n", error );
		return;		
		}
	symbolInfo = ( IMAGEHLP_SYMBOL * ) buffer;

	/* Walk the stack printing addresses and symbols if we can get them */
	for( i = 0; i < 50; i++ ) 
		{ 
		address = ( ( unsigned long * ) prevAddress )[ 1 ]; 
		prevAddress = ( ( unsigned long * ) prevAddress )[ 0 ]; 
		if( address == 0 )
			break;
		memset( symbolInfo, 0, sizeof( IMAGEHLP_SYMBOL ) );
		symbolInfo->SizeOfStruct = sizeof( IMAGEHLP_SYMBOL );
		symbolInfo->MaxNameLength = 512;
		if( !SymGetSymFromAddr( process, address, 0, symbolInfo ) )
			{
			const DWORD error = GetLastError();
			printf( "<Unknown - %d> - 0x%0X\n", error, address );
			continue;
			}
		printf( "%s - 0x%0X\n", symbolInfo->Name, symbolInfo->Address );
		}

	SymCleanup( process );
#if 0
	int noFrames, i;

	noFrames = CaptureStackBackTrace( 1, 100, stackInfo, NULL );
	for( i = 0; i < noFrames; i++ )
		{
		symbolInfo->MaxNameLength = 512;
		SymGetSymFromAddr( process, stackInfo[ i ], 0, symbolInfo );
		printf( "%i: %s - 0x%0X\n", noFrames - i - 1, symbolInfo->Name, symbolInfo->Address );
		}
#endif
	}
#endif /* Win64 */

#elif defined( __UNIX__ ) && defined( __linux__ )

#include <execinfo.h>
 
void displayBacktrace( void )
	{
	void *stackInfo[ 100 ];
	char **stackInfoStrings;
	int i, stackInfoSize;
 
	stackInfoSize = backtrace( stackInfo, 100 );
	stackInfoStrings = backtrace_symbols( stackInfo, stackInfoSize );
 
	for( i = 0; i < stackInfoSize; i++ ) 
		{
		printf( "%p : %s\n", stackInfo[ i ], stackInfoStrings[ i ] );
		}
 
	free( stackInfoStrings );
	}
#endif /* OS-specific backtrace printing */

/* Support function used to access the text string data from an ERROR_INFO
   structure.  Note that this function isn't thread-safe, but that should be
   OK since it's only used for debugging */

#ifdef USE_ERRMSGS

const char *getErrorInfoString( ERROR_INFO *errorInfo )
	{
	static char errorInfoString[ MAX_ERRMSG_SIZE + 1 + 8 ];

	if( errorInfo->errorStringLength <= 0 )
		return( "<<<No further information available>>>" );

	memcpy( errorInfoString, errorInfo->errorString, 
			errorInfo->errorStringLength );
	errorInfoString[ errorInfo->errorStringLength ] = '\0';

	return( errorInfoString );
	}
#endif /* USE_ERRMSGS */

/* Support function used with streams to pull data bytes out of the stream,
   allowing type and content data to be dumped with DEBUG_PRINT() */

RETVAL_RANGE_NOERROR( 0, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int debugGetStreamByte( INOUT /*STREAM*/ void *streamPtr, 
						IN_LENGTH const int position )
	{
	STREAM *stream = streamPtr;
	BYTE *dataPtr; 
	int status; 

	status = sMemGetDataBlockAbs( stream, position, ( void ** ) &dataPtr, 1 );
	if( cryptStatusError( status ) )
		return( 0 );
	return( byteToInt( *dataPtr ) );
	}

#endif /* !NDEBUG || DEBUG_DIAGNOSTIC_ENABLE */

/****************************************************************************
*																			*
*						Fault-injection Support Functions					*
*																			*
****************************************************************************/

#ifndef NDEBUG

/* Variables used for fault-injection tests */

FAULT_TYPE faultType;
int faultParam1;

/* Get a substitute key to replace the actual one, used to check for 
   detection of use of the wrong key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getSubstituteKey( OUT_HANDLE_OPT CRYPT_CONTEXT *iPrivateKey )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	int status;

	/* Clear return value */
	*iPrivateKey = CRYPT_ERROR;

	/* Try and read the certificate chain from the keyset */
	setMessageCreateObjectInfo( &createInfo, CRYPT_KEYSET_FILE );
	createInfo.arg2 = CRYPT_KEYOPT_READONLY;
	createInfo.strArg1 = "test/keys/server2.p15";
	createInfo.strArgLen1 = strlen( createInfo.strArg1 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_KEYSET );
	if( cryptStatusError( status ) )
		return( status );
	setMessageKeymgmtInfo( &getkeyInfo, CRYPT_KEYID_NAME, "Test user key", 13,
						   NULL, 0, KEYMGMT_FLAG_USAGE_SIGN );
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_KEY_GETKEY, 
							  &getkeyInfo, KEYMGMT_ITEM_PUBLICKEY );
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DESTROY );
	if( cryptStatusOK( status ) )
		*iPrivateKey = getkeyInfo.cryptHandle;

	return( status );
	}

/****************************************************************************
*																			*
*								Timing Functions							*
*																			*
****************************************************************************/

#if defined( __WINDOWS__ ) || defined( __UNIX__ )

#ifdef __UNIX__ 
  #include <sys/time.h>		/* For gettimeofday() */
#endif /* __UNIX__  */

/* Get/update high-resolution timer value */

HIRES_TIME debugTimeDiff( HIRES_TIME startTime )
	{
	HIRES_TIME timeValue;
#ifdef __WINDOWS__
	LARGE_INTEGER performanceCount;

	/* Sensitive to context switches */
	QueryPerformanceCounter( &performanceCount );
	timeValue = performanceCount.QuadPart;
#else
	struct timeval tv;

	/* Only accurate to about 1us */
	gettimeofday( &tv, NULL );
	timeValue = ( ( ( HIRES_TIME ) tv.tv_sec ) << 32 ) | tv.tv_usec;
#endif /* Windows vs.Unix high-res timing */

	if( !startTime )
		return( timeValue );
	return( timeValue - startTime );
	}

/* Display high-resulution time value */

int debugTimeDisplay( HIRES_TIME timeValue )
	{
	HIRES_TIME timeMS, ticksPerSec;

	/* Try and get the clock frequency */
#ifdef __WINDOWS__
	LARGE_INTEGER performanceCount;

	QueryPerformanceFrequency( &performanceCount );
	ticksPerSec = performanceCount.QuadPart;
#else
	ticksPerSec = 1000000L;
#endif /* __WINDOWS__ */	

	timeMS = ( timeValue * 1000 ) / ticksPerSec;
	assert( timeMS < INT_MAX );
	if( timeMS <= 0 )
		printf( "< 1" );
	else
		printf( HIRES_FORMAT_SPECIFIER, timeMS );

	return( ( timeMS <= 0 ) ? 1 : ( int ) timeMS );
	}
#endif /* __WINDOWS__ || __UNIX__ */
#endif /* NDEBUG */
