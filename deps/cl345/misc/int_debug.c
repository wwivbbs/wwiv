/****************************************************************************
*																			*
*						cryptlib Internal Debugging API						*
*						Copyright Peter Gutmann 1992-2017					*
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
   is.  If possible this is normally done via a macro in a debug.h that 
   remaps the debug-output macros to the appropriate function but Windows 
   only provides a puts()-equivalent and not a printf()-equivalent and under 
   Unix we need to send the output to stderr which can't easily be done in a 
   macro */
   
#if defined( __WIN32__ ) || defined( __WINCE__ )

/* OutputDebugString() under Windows isn't very reliable, it's implemented 
   using a 4kB chunk of shared memory DBWIN_BUFFER controlled by a mutex 
   DBWinMutex and two events DBWIN_BUFFER_READY and DBWIN_DATA_READY.  
   OutputDebugString() waits for exclusive access to DBWinMutex, maps in 
   DBWIN_BUFFER, waits for DBWIN_BUFFER_READY to be signalled, copies in up 
   to 4K of data, fires DBWIN_DATA_READY, and releases the mutex.  If any of 
   these operations fail, the call to OutputDebugString() is treated as a 
   no-op and we never see the output.  On the receiving side the debugger 
   performs the obvious actions to receive the data.  
   
   Beyond this Rube-Goldberg message-passing mechanism there are also 
   problems with permissions on the mutex and the way the DACL for it has 
   been tweaked in almost every OS release.  The end result of this is that 
   data sent to OutputDebugString() may be randomly lost if other threads 
   are also sending data to it (e.g. Visual Studio as part of its normal 
   chattiness about modules and threads being loaded/started/unloaded/
   stopped/etc).  
   
   The only way to avoid this is to step through the code that uses
   OutputDebugString() so that enough delay is inserted to allow other 
   callers and the code being debugged to get out of each others' hair */

int debugPrintf( const char *format, ... )
	{
	va_list argPtr;
	char buffer[ 1024 + 8 ];
	int length;

	va_start( argPtr, format );
#if VC_GE_2005( _MSC_VER )
	length = vsnprintf_s( buffer, 1024, _TRUNCATE, format, argPtr );
#else
	length = vsprintf( buffer, format, argPtr );
#endif /* VC++ 2005 or newer */
	va_end( argPtr );
#if defined( __WIN32__ ) 
	OutputDebugString( buffer );
#else
	NKDbgPrintfW( L"%s", buffer )
#endif /* __WIN32__ */

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
	fflush( stderr );	/* Just in case it gets stuck in a buffer */

	return( length );
	}

#elif defined( __MVS__ ) || defined( __VMCMS__ ) || defined( __ILEC400__ )

/* Implementing debugPrintf() on EBCDIC systems is problematic because all
   internal strings are ASCII, so that both the format specifiers and any
   string arguments that they're being used with won't work with functions
   that expect EBCDIC strings.  To get around this we use the internal 
   vsPrintf_s() that handles all of the format types that cryptlib uses, 
   and then convert the result to EBCDIC */

int debugPrintf( const char *format, ... )
	{
	va_list argPtr;
	char buffer[ 1024 + 8 ];
	int length;

	/* Format the arguments as an ASCII string */
	va_start( argPtr, format );
	length = vsPrintf_s( buffer, 1024, format, argPtr );
	va_end( argPtr );

	/* Convert it to EBCDIC and display it */
	bufferToEbcdic( buffer, buffer );
#pragma convlit( suspend )
	printf( "%s", buffer );
#pragma convlit( resume )
	fflush( stdout );	/* Just in case it gets stuck in a buffer */

	return( length );
	}
#else

#include <stdarg.h>			/* Needed for va_list */

int debugPrintf( const char *format, ... )
	{
	va_list argPtr;
	int length;

	va_start( argPtr, format );
	length = vprintf( format, argPtr );
	va_end( argPtr );

	return( length );
	}
#endif /* OS-specific debug output functions */

/* Dump a PDU to disk */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void buildFilePath( IN_STRING const char *fileName,
						   OUT_BUFFER_FIXED_C( 1024 ) char *filenameBuffer )
	{
	int i, LOOP_ITERATOR;

	assert( isReadPtr( fileName, 2 ) );
	assert( isWritePtr( filenameBuffer, 1024 ) );

	ANALYSER_HINT_STRING( fileName );

	/* Check whether the filename appears to be an absolute path */
	LOOP_MAX( i = 0, fileName[ i ] != '\0', i++ )
		{
		if( fileName[ i ] == ':' || fileName[ i ] == '/' )
			break;
		}
	ENSURES_V( LOOP_BOUND_OK );
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

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static FILE *openFile( IN_STRING const char *fileName,
					   INOUT char *filenameBuffer )
	{
	FILE *filePtr;

	assert( isReadPtr( fileName, 2 ) );

	ANALYSER_HINT_STRING( fileName );

	buildFilePath( fileName, filenameBuffer );
#if defined( EBCDIC_CHARS )
	bufferToEbcdic( filenameBuffer, filenameBuffer );
  #pragma convlit( suspend )	
	filePtr = fopen( filenameBuffer, "wb" );
  #pragma convlit( resume )	
#elif defined( __STDC_LIB_EXT1__ )
	if( fopen_s( &filePtr, filenameBuffer, "wb" ) != 0 )
		filePtr = NULL;
#else
	filePtr = fopen( filenameBuffer, "wb" );
#endif /* __STDC_LIB_EXT1__ */

	return( filePtr );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void debugDumpFile( IN_STRING const char *fileName, 
					IN_BUFFER( dataLength ) const void *data, 
					IN_LENGTH_SHORT const int dataLength )
	{
	FILE *filePtr;
	char filenameBuffer[ 1024 + 8 ];
	int count DUMMY_INIT;

	assert( isReadPtr( fileName, 2 ) );
	assert( isReadPtrDynamic( data, dataLength ) );

	ANALYSER_HINT_STRING( fileName );

	filePtr = openFile( fileName, filenameBuffer );
	assert( filePtr != NULL );
	if( filePtr == NULL )
		return;
	if( dataLength > 0 )
		{
#ifdef __MQXRTOS__
		/* MQX gets fwrite() args wrong */
		count = fwrite( ( void * ) data, 1, dataLength, filePtr );
#else
		count = fwrite( data, 1, dataLength, filePtr );
#endif /* __MQXRTOS__ */
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
	MESSAGE_DATA msgData DUMMY_INIT_STRUCT;
	FILE *filePtr;
	BYTE certData[ 2048 + 8 ];
	char filenameBuffer[ 1024 + 8 ];
	int certType, count DUMMY_INIT, status;

	assert( isReadPtr( fileName, 2 ) );
	assert( isHandleRangeValid( iCryptCert ) );

	ANALYSER_HINT_STRING( fileName );

	filePtr = openFile( fileName, filenameBuffer );
	assert( filePtr != NULL );
	if( filePtr == NULL )
		return;
	status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE, &certType,
							  CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, certData, 2048 );
		status = krnlSendMessage( iCryptCert, IMESSAGE_CRT_EXPORT, &msgData, 
								  ( certType == CRYPT_CERTTYPE_PKIUSER ) ? \
									CRYPT_ICERTFORMAT_DATA : \
									CRYPT_CERTFORMAT_CERTIFICATE );
		}
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
	char dumpBuffer[ 128 + 1 + 8 ];
	int offset, i, j, LOOP_ITERATOR;

	ANALYSER_HINT_STRING( prefixString );

	offset = sprintf_s( dumpBuffer, 128, "%3s %4d %04X ", 
						prefixString, dataLength, 
						checksumData( data, dataLength ) & 0xFFFF );
	LOOP_MAX( i = 0, i < dataLength, i += 16 )
		{
		const int innerLen = min( dataLength - i, 16 );
		int LOOP_ITERATOR_ALT;

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
		LOOP_MAX_CHECKINC_ALT( j < 16, j++ )
			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "   " );
		ENSURES_V( LOOP_BOUND_OK_ALT );
		LOOP_MAX_ALT( j = 0, j < innerLen, j++ )
			{
			const BYTE ch = ( ( BYTE * ) data )[ i + j ];

			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "%c",
								 isPrint( ch ) ? ch : '.' );
			}
		ENSURES_V( LOOP_BOUND_OK_ALT );
		DEBUG_PUTS(( dumpBuffer ));
		}
	ENSURES_V( LOOP_BOUND_OK );

#if !( defined( __WIN32__ ) || defined( __WINCE__ ) || defined( __ECOS__ ) || \
	   ( defined( __MQXRTOS__ ) && defined( MQX_SUPPRESS_STDIO_MACROS ) ) )
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
	char dumpBuffer[ 128 + 1 + 8 ];
	int offset, i, LOOP_ITERATOR;

	LOOP_MAX( i = 0, i < dataLength, i += 16 )
		{
		const int innerLen = min( dataLength - i, 16 );
		int j, LOOP_ITERATOR_ALT;

		offset = sprintf_s( dumpBuffer, 128, "%04d: ", i );
		LOOP_MAX_ALT( j = 0, j < innerLen, j++ )
			{
			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "%02X ",
								 ( ( BYTE * ) data )[ i + j ] );
			}
		ENSURES_V( LOOP_BOUND_OK_ALT );
		LOOP_MAX_CHECKINC_ALT( j < 16, j++ )
			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "   " );
		ENSURES_V( LOOP_BOUND_OK_ALT );
		LOOP_MAX_ALT( j = 0, j < innerLen, j++ )
			{
			const BYTE ch = ( ( BYTE * ) data )[ i + j ];

			offset += sprintf_s( dumpBuffer + offset, 128 - offset, "%c",
								 isPrint( ch ) ? ch : '.' );
			}
		ENSURES_V( LOOP_BOUND_OK_ALT );
		DEBUG_PUTS(( dumpBuffer ));
		}
	ENSURES_V( LOOP_BOUND_OK );

#if !( defined( __WIN32__ ) || defined( __WINCE__ ) || defined( __ECOS__ ) || \
	   ( defined( __MQXRTOS__ ) && defined( MQX_SUPPRESS_STDIO_MACROS ) ) )
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

#if VC_GE_2005( _MSC_VER )

#include <dbghelp.h>
#pragma comment( lib, "dbghelp.lib" )

#ifdef _WIN64 
  #define BINARIES_PATH	"debug64_vs10;binaries64_vs10" 
#else
  #define BINARIES_PATH	"debug32_vs10;binaries32_vs10"
#endif /* Win32 vs. Win64 */

void displayBacktrace( void )
	{
	HANDLE process;
	SYMBOL_INFO *symbolInfo;
	IMAGEHLP_LINE lineInfo;
	BYTE buffer[ sizeof( SYMBOL_INFO ) + ( MAX_SYM_NAME * sizeof( TCHAR ) ) ];
	void *stack[ 64 ];
	int noFrames, i, LOOP_ITERATOR;

	/* Load debugging symbols for the current process */
	DEBUG_PUTS(( "Stack trace:" ));
	process = GetCurrentProcess();
	SymSetOptions( SYMOPT_LOAD_LINES );
	if( !SymInitialize( process, BINARIES_PATH, TRUE ) )
		{
		const DWORD error = GetLastError();
		
		DEBUG_PRINT(( "Couldn't load symbols, error = %d.\n", error ));
		return;		
		}

	/* Get the stack backtrace.  Frame 0 is the current function so we start at
	   1, and we don't display the last four frames since they're system 
	   functions */
	noFrames = CaptureStackBackTrace( 1, 64, stack, NULL );
	if( noFrames < 4 )
		{
		const DWORD error = GetLastError();
		
		DEBUG_PRINT(( "Couldn't capture stack backtrace, error = %d.\n", error ));
		return;		
		}

	/* Set up the SYMBOL_INFO and LINE_INFO structures */
	symbolInfo = ( SYMBOL_INFO * ) buffer;
	memset( symbolInfo, 0, sizeof( SYMBOL_INFO ) );
	symbolInfo->MaxNameLen = MAX_SYM_NAME;
	symbolInfo->SizeOfStruct = sizeof( SYMBOL_INFO );
	memset( &lineInfo, 0, sizeof( IMAGEHLP_LINE ) );
	lineInfo.SizeOfStruct = sizeof( IMAGEHLP_LINE );

	/* Walk the stack printing addresses and symbols if we can get them */
	LOOP_LARGE( i = 0, i < noFrames - 4, i++ ) 
		{
		const DWORD_PTR address = ( DWORD_PTR ) stack[ i ];
		DWORD dwDisplacement;

		if( !SymFromAddr( process, address, 0, symbolInfo ) )
			{
			const DWORD error = GetLastError();
			DEBUG_PRINT(( "<Unknown - %d> - 0x%I64x\n", error, address ));
			continue;
			}
		if( SymGetLineFromAddr( process, address, &dwDisplacement, 
								&lineInfo ) )
			{
			DEBUG_PRINT(( "%s:%d - 0x%I64x\n", symbolInfo->Name, 
						  lineInfo.LineNumber, symbolInfo->Address ));
			}
		else
			{
			DEBUG_PRINT(( "%s - 0x%I64x\n", symbolInfo->Name, 
						  symbolInfo->Address ));
			}
		}
	ENSURES_V( LOOP_BOUND_OK );
	}
#else

/* Older versions of VC++ don't have dbghelp.h/dbghelp.lib, which contains 
   CaptureStackBackTrace(), so we'd either need to dynamically load it or 
   just use alternate code that walks the stack manually from the current 
   EBP */

#include <ImageHlp.h>
#pragma comment( lib, "imagehlp.lib" )

#define BINARIES_PATH	"debug32_vc6;binaries32_vc6"

void displayBacktrace( void )
	{
	HANDLE process;
	IMAGEHLP_SYMBOL *symbolInfo;
	IMAGEHLP_LINE lineInfo;
	BYTE buffer[ sizeof( IMAGEHLP_SYMBOL ) + 1024 ];
	unsigned long prevAddress, address = 1;
	int i, LOOP_ITERATOR;

	/* Horribly nonportable way of walking the stack due to lack of access
	   to CaptureStackBackTrace() */
	__asm { mov prevAddress, ebp };

	/* Load debugging symbols for the current process */
	DEBUG_PUTS(( "Stack trace:" ));
	process = GetCurrentProcess();
	SymSetOptions( SYMOPT_LOAD_LINES );
	if( !SymInitialize( process, BINARIES_PATH, TRUE ) )
		{
		DEBUG_OP( const DWORD error = GetLastError() );
		
		DEBUG_PRINT(( "Couldn't load symbols, error = %d.\n", error ));
		return;		
		}

	/* Set up the SYMBOL_INFO and LINE_INFO structures */
	symbolInfo = ( IMAGEHLP_SYMBOL * ) buffer;
	memset( symbolInfo, 0, sizeof( IMAGEHLP_SYMBOL ) );
	symbolInfo->SizeOfStruct = sizeof( IMAGEHLP_SYMBOL );
	symbolInfo->MaxNameLength = 512;
	memset( &lineInfo, 0, sizeof( IMAGEHLP_LINE ) );
	lineInfo.SizeOfStruct = sizeof( IMAGEHLP_LINE );

	/* Walk the stack printing addresses and symbols if we can get them */
	LOOP_LARGE( i = 0, i < 50, i++ ) 
		{ 
		DWORD dwDisplacement;

		address = ( ( unsigned long * ) prevAddress )[ 1 ]; 
		prevAddress = ( ( unsigned long * ) prevAddress )[ 0 ]; 
		if( address == 0 )
			break;
		if( !SymGetSymFromAddr( process, address, 0, symbolInfo ) )
			{
			DEBUG_OP( const DWORD error = GetLastError() );

			DEBUG_PRINT(( "<Unknown - %d> - 0x%0lX\n", error, address ));
			continue;
			}
		if( SymGetLineFromAddr( process, address, &dwDisplacement, 
								&lineInfo ) )
			{
			DEBUG_PRINT(( "%s:%d - 0x%lX\n", symbolInfo->Name, 
						  lineInfo.LineNumber, symbolInfo->Address ));
			}
		else
			{
			DEBUG_PRINT(( "%s - 0x%0X\n", symbolInfo->Name, 
						  symbolInfo->Address ));
			}
		}
	ENSURES_V( LOOP_BOUND_OK );

	SymCleanup( process );
	}
#endif /* Win32 vs. Win64 */

#elif defined( __UNIX__ ) && \
	  ( defined( __APPLE__ ) || defined( __linux__ ) || defined( __sun ) )

#include <execinfo.h>

#ifdef __GNUC__
  /* Needed with -finstrument */
  void __cyg_profile_func_enter( void *func, void *caller ) \
	   __attribute__(( no_instrument_function ));
  void __cyg_profile_func_enter( void *func, void *caller ) { }	
  void __cyg_profile_func_exit( void *func, void *caller ) \
	   __attribute__(( no_instrument_function ));
  void __cyg_profile_func_exit( void *func, void *caller ) { }
#endif /* __GNUC__ */

void displayBacktrace( void )
	{
	void *stackInfo[ 100 + 8 ];
	char **stackInfoStrings;
	int i, stackInfoSize, LOOP_ITERATOR;
 
	DEBUG_PUTS(( "Stack trace:" ));
	stackInfoSize = backtrace( stackInfo, 100 );
	if( stackInfoSize <= 2 )
		{
		/* See also the comment about -rdynamic at the start of this code 
		   section */
		DEBUG_PUTS(( "Only one level of backtrace available, if this is an "
					 "architecture without\nframe pointers like ARM or MIPS "
					 "then you need to rebuild with\n-finstrument-functions "
					 "and/or -fexceptions" ));
		}
	stackInfoStrings = backtrace_symbols( stackInfo, stackInfoSize );
 
	/* We start at position 1, since the 0-th entry is the current function,
	   i.e. displayBacktrace().  We also stop one before the last entry, 
	   which is typically main() or something similar */
	LOOP_LARGE( i = 1, i < stackInfoSize - 1, i++ ) 
		{
		DEBUG_PRINT(( "%p : %s\n", stackInfo[ i ], stackInfoStrings[ i ] ));
		}
 	ENSURES_V( LOOP_BOUND_OK );

	free( stackInfoStrings );
	}
#endif /* OS-specific backtrace printing */

/* Support function used to access the text string data from an ERROR_INFO
   structure.  Note that this function isn't thread-safe, but that should be
   OK since it's only used for debugging */

#if !defined( NDEBUG ) && defined( USE_ERRMSGS )

const char *getErrorInfoString( const ERROR_INFO *errorInfo )
	{
	static char errorInfoString[ MAX_ERRMSG_SIZE + 8 ];
	const int errorStringLength = \
				min( errorInfo->errorStringLength, MAX_ERRMSG_SIZE - 1 );

	/* If there's no extended error information available, return an 
	   indicator of this */
	if( errorStringLength <= 0 )
		return( "<<<No further information available>>>" );

	memcpy( errorInfoString, errorInfo->errorString, errorStringLength );
	errorInfoString[ errorStringLength ] = '\0';

	return( errorInfoString );
	}
#endif /* !NDEBUG && USE_ERRMSGS */

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
#endif /* Debug || DEBUG_DIAGNOSTIC_ENABLE */

/****************************************************************************
*																			*
*						Fault-injection Support Functions					*
*																			*
****************************************************************************/

#if defined( CONFIG_FAULTS ) && !defined( NDEBUG )

#define PARAM_ACL	int		/* Fake out unneeded types */
#include "kernel/kernel.h"

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

/* Corrupt a random bit in a block of memory */

CHECK_RETVAL \
static int getDeterministicRandomInt( void )
	{
	HASH_FUNCTION_ATOMIC hashFunction;
	static BYTE hashBuffer[ CRYPT_MAX_HASHSIZE ] = { 0 };
	BYTE *hashBufPtr = hashBuffer;
	int hashSize, retVal;

	/* Step the RNG and extract an integer's-worth of data from it.  We go
	   via the intermediate value retVal because mgetLong() is a macro that
	   doesn't work as part of a return statement */
	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunction, &hashSize );
	hashFunction( hashBuffer, hashSize, hashBuffer, hashSize );
	retVal = mgetLong( hashBufPtr );

	return( retVal );
	}

void injectMemoryFault( void )
	{
	const int value = getDeterministicRandomInt();
	const BYTE bitMask = intToByte( 1 << ( value & 3 ) );
	const int bytePos = value >> 3;
	BYTE *dataPtr = ( BYTE * ) getKrnlData();
	const int dataSize = getKrnlDataSize();

	dataPtr[ bytePos % dataSize ] ^= bitMask;
	}
#endif /* CONFIG_FAULTS && Debug */

/****************************************************************************
*																			*
*								Timing Functions							*
*																			*
****************************************************************************/

#if !defined( NDEBUG ) && ( defined( __WINDOWS__ ) || defined( __UNIX__ ) )

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
  #ifdef HIRES_TIME_64BIT
	timeValue = ( ( ( HIRES_TIME ) tv.tv_sec ) << 32 ) | tv.tv_usec;
  #else
	timeValue = tv.tv_usec;
  #endif /* HIRES_TIME_64BIT */
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
#endif /* Debug && ( __WINDOWS__ || __UNIX__ ) */
