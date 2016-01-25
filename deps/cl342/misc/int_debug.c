/****************************************************************************
*																			*
*						cryptlib Internal Debugging API						*
*						Copyright Peter Gutmann 1992-2008					*
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

#ifndef NDEBUG

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

	/* Check whether the filename appears to be an absolute path */
	for( i = 0; fileName[ i ] != '\0'; i++ )
		{
		if( fileName[ i ] == ':' || fileName[ i ] == '/' )
			break;
		}
	if( fileName[ i ] == '\0' )
		{
		/* It's a relative path, put the file in the temp directory */
#if defined( __WIN32__ )
		GetTempPath( 512, filenameBuffer );
#else
		strlcpy_s( filenameBuffer, 1024, "/tmp/" );
#endif /* __WIN32__ */
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
	int count = DUMMY_INIT;

	assert( isReadPtr( fileName, 2 ) );
	assert( isReadPtr( data, dataLength ) );

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
	int count = DUMMY_INIT, status;

	assert( isReadPtr( fileName, 2 ) );
	assert( isHandleRangeValid( iCryptCert ) );

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
	ANALYSER_HINT( dataPtr != NULL );
	debugDumpData( dataPtr, length );
	}

/* Support function used to access the text string data from an ERROR_INFO
   structure.  Note that this function isn't thread-safe, but that should be
   OK since it's only used for debugging */

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

/* Support function used with streams to pull data bytes out of the stream,
   allowing type and content data to be dumped with DEBUG_PRINT() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
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
#endif /* !NDEBUG */
