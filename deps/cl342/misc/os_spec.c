/****************************************************************************
*																			*
*					cryptlib OS-specific Support Routines					*
*					  Copyright Peter Gutmann 1992-2007						*
*																			*
****************************************************************************/

#include <ctype.h>
#include <stddef.h>		/* For ptrdiff_t */
#include <stdio.h>
#if defined( INC_ALL )
  #include "crypt.h"
#else
  #include "crypt.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*									AMX										*
*																			*
****************************************************************************/

#if defined( __AMX__ )

#include <cjzzz.h>

/* The AMX task-priority function returns the priority via a reference
   parameter.  Because of this we have to provide a wrapper that returns
   it as a return value */

int threadPriority( void )
	{
	int priority = 0;

	cjtkpradjust( cjtkid(), &priority );
	return( priority );
	}

/****************************************************************************
*																			*
*									ARINC 653								*
*																			*
****************************************************************************/

#elif defined( __ARINC653__ )

#include <apex.h>

/* The ARINC 653 API returns status codes as a by-reference parameter, in 
   cases where we don't care about the return status we need to provide a
   dummy value to take this */

RETURN_CODE_TYPE dummyRetCode;

/* The ARINC 653 thread-self function returns the thread ID via a reference 
   parameter, because of this we have to provide a wrapper that returns it 
   as a return value */

PROCESS_ID_TYPE threadSelf( void )
	{
	PROCESS_ID_TYPE processID;
	RETURN_CODE_TYPE retCode; 

	GET_MY_ID( &processID, &retCode );
	return( processID );
	}

/****************************************************************************
*																			*
*									uC/OS-II								*
*																			*
****************************************************************************/

#elif defined( __UCOS__ )

#undef BOOLEAN					/* See comment in kernel/thread.h */
#include <ucos_ii.h>
#define BOOLEAN			int

/* uC/OS-II doesn't have a thread-self function, but allows general task
   info to be queried.  Because of this we provide a wrapper that returns
   the task ID as its return value */

INT8U threadSelf( void )
	{
	OS_TCB osTCB;

	OSTaskQuery( OS_PRIO_SELF, &osTCB );
	return( osTCB.OSTCBPrio );
	}

/****************************************************************************
*																			*
*									uITRON									*
*																			*
****************************************************************************/

#elif defined( __ITRON__ )

#include <itron.h>

/* The uITRON thread-self function returns the thread ID via a reference
   parameter since uITRON IDs can be negative and there'd be no way to
   differentiate a thread ID from an error code.  Because of this we have
   to provide a wrapper that returns it as a return value */

ID threadSelf( void )
	{
	ID tskid;

	get_tid( &tskid );
	return( tskid );
	}

/****************************************************************************
*																			*
*								IBM Mainframe								*
*																			*
****************************************************************************/

/* VM/CMS, MVS, and AS/400 systems need to convert characters from ASCII <->
   EBCDIC before/after they're read/written to external formats, the
   following functions perform the necessary conversion using the latin-1
   code tables for systems that don't have etoa/atoe */

#elif defined( __MVS__ ) && defined( EBCDIC_CHARS )

#include <stdarg.h>

#ifndef USE_ETOA

/* ISO 8859-1 to IBM Latin-1 Code Page 01047 (EBCDIC) */

static const BYTE asciiToEbcdicTbl[] = {
	0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F,	/* 00 - 07 */
	0x16, 0x05, 0x15, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,	/* 08 - 0F */
	0x10, 0x11, 0x12, 0x13, 0x3C, 0x3D, 0x32, 0x26,	/* 10 - 17 */
	0x18, 0x19, 0x3F, 0x27, 0x1C, 0x1D, 0x1E, 0x1F,	/* 18 - 1F */
	0x40, 0x5A, 0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D,	/* 20 - 27 */
	0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61,	/* 28 - 2F */
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,	/* 30 - 37 */
	0xF8, 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F,	/* 38 - 3F */
	0x7C, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,	/* 40 - 47 */
	0xC8, 0xC9, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,	/* 48 - 4F */
	0xD7, 0xD8, 0xD9, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,	/* 50 - 57 */
	0xE7, 0xE8, 0xE9, 0xAD, 0xE0, 0xBD, 0x5F, 0x6D,	/* 58 - 5F */
	0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,	/* 60 - 67 */
	0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,	/* 68 - 6F */
	0x97, 0x98, 0x99, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,	/* 70 - 77 */
	0xA7, 0xA8, 0xA9, 0xC0, 0x4F, 0xD0, 0xA1, 0x07,	/* 78 - 7F */
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x06, 0x17,	/* 80 - 87 */
	0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x09, 0x0A, 0x1B,	/* 88 - 8F */
	0x30, 0x31, 0x1A, 0x33, 0x34, 0x35, 0x36, 0x08,	/* 90 - 97 */
	0x38, 0x39, 0x3A, 0x3B, 0x04, 0x14, 0x3E, 0xFF,	/* 98 - 9F */
	0x41, 0xAA, 0x4A, 0xB1, 0x9F, 0xB2, 0x6A, 0xB5,	/* A0 - A7 */
	0xBB, 0xB4, 0x9A, 0x8A, 0xB0, 0xCA, 0xAF, 0xBC,	/* A8 - AF */
	0x90, 0x8F, 0xEA, 0xFA, 0xBE, 0xA0, 0xB6, 0xB3,	/* B0 - B7 */
	0x9D, 0xDA, 0x9B, 0x8B, 0xB7, 0xB8, 0xB9, 0xAB,	/* B8 - BF */
	0x64, 0x65, 0x62, 0x66, 0x63, 0x67, 0x9E, 0x68,	/* C0 - C7 */
	0x74, 0x71, 0x72, 0x73, 0x78, 0x75, 0x76, 0x77,	/* C8 - CF */
	0xAC, 0x69, 0xED, 0xEE, 0xEB, 0xEF, 0xEC, 0xBF,	/* D0 - D7 */
	0x80, 0xFD, 0xFE, 0xFB, 0xFC, 0xBA, 0xAE, 0x59,	/* D8 - DF */
	0x44, 0x45, 0x42, 0x46, 0x43, 0x47, 0x9C, 0x48,	/* E0 - E7 */
	0x54, 0x51, 0x52, 0x53, 0x58, 0x55, 0x56, 0x57,	/* E8 - EF */
	0x8C, 0x49, 0xCD, 0xCE, 0xCB, 0xCF, 0xCC, 0xE1,	/* F0 - F7 */
	0x70, 0xDD, 0xDE, 0xDB, 0xDC, 0x8D, 0x8E, 0xDF	/* F8 - FF */
	};

/* IBM Latin-1 Code Page 01047 (EBCDIC) to ISO 8859-1 */

static const BYTE ebcdicToAsciiTbl[] = {
	0x00, 0x01, 0x02, 0x03, 0x9C, 0x09, 0x86, 0x7F,	/* 00 - 07 */
	0x97, 0x8D, 0x8E, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,	/* 08 - 0F */
	0x10, 0x11, 0x12, 0x13, 0x9D, 0x0A, 0x08, 0x87,	/* 10 - 17 */
	0x18, 0x19, 0x92, 0x8F, 0x1C, 0x1D, 0x1E, 0x1F,	/* 18 - 1F */
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x17, 0x1B,	/* 20 - 27 */
	0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x05, 0x06, 0x07,	/* 28 - 2F */
	0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04,	/* 30 - 37 */
	0x98, 0x99, 0x9A, 0x9B, 0x14, 0x15, 0x9E, 0x1A,	/* 38 - 3F */
	0x20, 0xA0, 0xE2, 0xE4, 0xE0, 0xE1, 0xE3, 0xE5,	/* 40 - 47 */
	0xE7, 0xF1, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,	/* 48 - 4F */
	0x26, 0xE9, 0xEA, 0xEB, 0xE8, 0xED, 0xEE, 0xEF,	/* 50 - 57 */
	0xEC, 0xDF, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0x5E,	/* 58 - 5F */
	0x2D, 0x2F, 0xC2, 0xC4, 0xC0, 0xC1, 0xC3, 0xC5,	/* 60 - 67 */
	0xC7, 0xD1, 0xA6, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,	/* 68 - 6F */
	0xF8, 0xC9, 0xCA, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF,	/* 70 - 77 */
	0xCC, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,	/* 78 - 7F */
	0xD8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 80 - 87 */
	0x68, 0x69, 0xAB, 0xBB, 0xF0, 0xFD, 0xFE, 0xB1,	/* 88 - 8F */
	0xB0, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,	/* 90 - 97 */
	0x71, 0x72, 0xAA, 0xBA, 0xE6, 0xB8, 0xC6, 0xA4,	/* 98 - 9F */
	0xB5, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,	/* A0 - A7 */
	0x79, 0x7A, 0xA1, 0xBF, 0xD0, 0x5B, 0xDE, 0xAE,	/* A8 - AF */
	0xAC, 0xA3, 0xA5, 0xB7, 0xA9, 0xA7, 0xB6, 0xBC,	/* B0 - B7 */
	0xBD, 0xBE, 0xDD, 0xA8, 0xAF, 0x5D, 0xB4, 0xD7,	/* B8 - BF */
	0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* C0 - C7 */
	0x48, 0x49, 0xAD, 0xF4, 0xF6, 0xF2, 0xF3, 0xF5,	/* C8 - CF */
	0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,	/* D0 - D7 */
	0x51, 0x52, 0xB9, 0xFB, 0xFC, 0xF9, 0xFA, 0xFF,	/* D8 - DF */
	0x5C, 0xF7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,	/* E0 - E7 */
	0x59, 0x5A, 0xB2, 0xD4, 0xD6, 0xD2, 0xD3, 0xD5,	/* E8 - EF */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* F0 - F7 */
	0x38, 0x39, 0xB3, 0xDB, 0xDC, 0xD9, 0xDA, 0x9F	/* F8 - FF */
	};

/* Convert a string to/from EBCDIC */

int asciiToEbcdic( char *dest, const char *src, const int length )
	{
	int i;

	assert( isReadPtr( src, length ) );
	assert( isWritePtr( dest, length ) );

	for( i = 0; i < length; i++ )
		dest[ i ] = asciiToEbcdicTbl[ ( unsigned int ) src[ i ] ];
	return( CRYPT_OK );
	}

int ebcdicToAscii( char *dest, const char *src, const int length )
	{
	int i;

	assert( isReadPtr( src, length ) );
	assert( isWritePtr( dest, length ) );

	for( i = 0; i < length; i++ )
		dest[ i ] = ebcdicToAsciiTbl[ ( unsigned int ) src[ i ] ];
	return( CRYPT_OK );
	}
#else

int asciiToEbcdic( char *dest, const char *src, const int length )
	{
	assert( isReadPtr( src, length ) );
	assert( isWritePtr( dest, length ) );

	if( dest != src )
		memcpy( dest, src, length );
	return( return( __atoe_l( dest, length ) < 0 ? \
			CRYPT_ERROR_BADDATA : CRYPT_OK );
	}

int ebcdicToAscii( char *dest, const char *src, const int length )
	{
	assert( isReadPtr( src, length ) );
	assert( isWritePtr( dest, length ) );

	if( dest != src )
		memcpy( dest, src, length );
	return( return( __etoa_l( dest, length ) < 0 ? \
			CRYPT_ERROR_BADDATA : CRYPT_OK );
	}
#endif /* USE_ETOA */

/* Convert a string to EBCDIC via a temporary buffer, used when passing an
   ASCII string to a system function that requires EBCDIC */

char *bufferToEbcdic( char *buffer, const char *string )
	{
	strcpy( buffer, string );
	asciiToEbcdic( buffer, buffer, strlen( string ) );
	return( buffer );
	}

/* Table for ctype functions that explicitly use the ASCII character set */

#define A	ASCII_ALPHA
#define L	ASCII_LOWER
#define N	ASCII_NUMERIC
#define S	ASCII_SPACE
#define U	ASCII_UPPER
#define X	ASCII_HEX
#define AL	( A | L )
#define AU	( A | U )
#define ANX	( A | N | X )
#define AUX	( A | U | X )

const BYTE asciiCtypeTbl[ 256 ] = {
	/* 00	   01	   02	   03	   04	   05	   06	   07  */
		0,		0,		0,		0,		0,		0,		0,		0,
	/* 08	   09	   0A	   0B	   0C	   0D	   0E	   0F */
		0,		0,		0,		0,		0,		0,		0,		0,
	/* 10	   11	   12	   13	   14	   15	   16	   17 */
		0,		0,		0,		0,		0,		0,		0,		0,
	/* 18	   19	   1A	   1B	   1C	   1D	   1E	   1F */
		0,		0,		0,		0,		0,		0,		0,		0,
	/*			!		"		#		$		%		&		' */
		A,		A,		A,		A,		A,		A,		A,		A,
	/* 	(		)		*		+		,		-		.		/ */
		A,		A,		A,		A,		A,		A,		A,		A,
	/*	0		1		2		3		4		5		6		7 */
	   ANX,	   ANX,	   ANX,	   ANX,	   ANX,	   ANX,	   ANX,	   ANX,
	/*	8		9		:		;		<		=		>		? */
	   ANX,	   ANX,		A,		A,		A,		A,		A,		A,
	/*	@		A		B		C		D		E		F		G */
		A,	   AUX,	   AUX,	   AUX,	   AUX,	   AUX,	   AUX,	   AU,
	/*	H		I		J		K		L		M		N		O */
	   AU,	   AU,	   AU,	   AU,	   AU,	   AU,	   AU,	   AU,
	/*	P		Q		R		S		T		U		V		W */
	   AU,	   AU,	   AU,	   AU,	   AU,	   AU,	   AU,	   AU,
	/*	X		Y		Z		[		\		]		^		_ */
	   AU,	   AU,	   AU,		A,		A,		A,		A,		A,
	/*	`		a		b		c		d		e		f		g */
		A,	   AL,	   AL,	   AL,	   AL,	   AL,	   AL,	   AL,
	/*	h		i		j		k		l		m		n		o */
	   AL,	   AL,	   AL,	   AL,	   AL,	   AL,	   AL,	   AL,
	/*	p		q		r		s		t		u		v		w */
	   AL,	   AL,	   AL,	   AL,	   AL,	   AL,	   AL,	   AL,
	/*	x		y		z		{		|		}		~	   DL */
	   AL,	   AL,	   AL,		A,		A,		A,		A,		A,
	/* High-bit-set characters */
	0
	};

/* stricmp()/strnicmp() versions that explicitly use the ASCII character
   set.  In order for collation to be handled properly, we have to convert
   to EBCDIC and use the local stricmp()/strnicmp() */

int strCompare( const char *src, const char *dest, int length )
	{
	BYTE buffer1[ MAX_ATTRIBUTE_SIZE + 8 ];
	BYTE buffer2[ MAX_ATTRIBUTE_SIZE + 8 ];

	assert( isReadPtr( src, length ) );

	if( length > MAX_ATTRIBUTE_SIZE )
		return( 1 );	/* Invalid length */

	/* Virtually all strings are 7-bit ASCII, the following optimisation
	   speeds up checking, particularly in cases where we're walking down a
	   list of keywords looking for a match */
	if( *src < 0x80 && *dest < 0x80 && \
		toLower( *src ) != toLower( *dest ) )
		return( 1 );	/* Not equal */

	/* Convert the strings to EBCDIC and use a native compare */
	src = bufferToEbcdic( buffer1, src );
	dest = bufferToEbcdic( buffer2, dest );
	return( strncasecmp( src, dest, length ) );
	}

int strCompareZ( const char *src, const char *dest )
	{
	const int length = strlen( src );

	if( length != strlen( dest ) )
		return( 1 );	/* Lengths differ */
	return( strCompare( src, dest, length ) );
	}

/* sprintf_s() and vsprintf_s() that take ASCII format strings.  Since 
   vsprintf_s() does the same thing as sprintf_s(), we map them both to
   the same function in the os_spec.h header.  Unfortunately we have to
   use vsprintf() to do the actual printing, since MVS doesn't support
   vsnprintf() */

int sPrintf_s( char *buffer, const int bufSize, const char *format, ... )
	{
	BYTE formatBuffer[ MAX_ATTRIBUTE_SIZE + 8 ];
	va_list argPtr;
	const int formatLen = strlen( format ) - 1;
#ifndef NDEBUG
	int i;
#endif /* Debug version */
	int status;

#ifndef NDEBUG
	/* Make sure that we don't have any string args, which would require
	   their own conversion to EBCDIC */
	for( i = 0; i < formatLen; i++ )
		{
		if( format[ i ] == '%' && format[ i + 1 ] == 's' )
			{
			assert( DEBUG_WARN );
			strlcpy_s( buffer, bufSize, 
					   "<<<Unable to format output string>>>" );
			return( -1 );
			}
		}
#endif /* Debug version */
	format = bufferToEbcdic( formatBuffer, format );
	va_start( argPtr, format );
	status = vsprintf( buffer, format, argPtr );
	if( status > 0 )
		ebcdicToAscii( buffer, buffer, status );
	va_end( argPtr );
	return( status );
	}

/****************************************************************************
*																			*
*									PalmOS									*
*																			*
****************************************************************************/

#elif defined( __Nucleus__ )

#include <nucleus.h>

/* Wrappers for the Nucleus OS-level memory allocation functions */

extern NU_MEMORY_POOL Application_Memory;

void *clAllocFn( size_t size )
	{
	STATUS Rc;
	void *ptr;

	Rc = NU_Allocate_Memory( &Application_Memory, &ptr, size, 
							 NU_NO_SUSPEND );
	if( Rc != NU_SUCCESS || ptr == NULL )
		return( NULL );
	return( ptr );
	}

void clFreeFn( void *memblock )
	{
	NU_Deallocate_Memory( memblock );
	}

/****************************************************************************
*																			*
*									PalmOS									*
*																			*
****************************************************************************/

#elif defined( __PALMOS__ )

#include <CmnErrors.h>
#include <CmnLaunchCodes.h>

/* The cryptlib entry point, defined in cryptlib.sld */

uint32_t cryptlibMain( uint16_t cmd, void *cmdPBP, uint16_t launchFlags )
	{
	UNUSED_ARG( cmdPBP );
	UNUSED_ARG( launchFlags );

	switch( cmd )
		{
		case sysLaunchCmdInitialize:
			/* Set up the initialisation lock in the kernel */
			preInit();
			break;

		case sysLaunchCmdFinalize:
			/* Delete the initialisation lock in the kernel */
			postShutdown();
			break;
		}

	return( errNone );
	}

/****************************************************************************
*																			*
*									RTEMS									*
*																			*
****************************************************************************/

#elif defined( __RTEMS__ )

/* The RTEMS thread-self function returns the task ID via a reference
   parameter, because of this we have to provide a wrapper that returns it
   as a return value.  We use RTEMS_SEARCH_ALL_NODES because there isn't
   any other way to specify the local node, this option always searches the
   local node first so it has the desired effect */

#include <rtems.h>

rtems_id threadSelf( void )
	{
	rtems_id taskID;

	rtems_task_ident( RTEMS_SELF, RTEMS_SEARCH_ALL_NODES, &taskID );
	return( taskID );
	}

/****************************************************************************
*																			*
*									Tandem									*
*																			*
****************************************************************************/

/* The Tandem mktime() is broken and can't convert dates beyond 2023, if
   mktime() fails and the year is between then and the epoch try again with
   a time that it can convert */

#elif defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ )

#undef mktime	/* Restore the standard mktime() */

time_t my_mktime( struct tm *timeptr )
	{
	time_t theTime;

	theTime = mktime( timeptr );
	if( theTime < 0 && timeptr->tm_year > 122 && timeptr->tm_year <= 138 )
		{
		timeptr->tm_year = 122;	/* Try again with a safe year of 2022 */
		theTime = mktime( timeptr );
		}
	return( theTime );
	}

/****************************************************************************
*																			*
*									Unix									*
*																			*
****************************************************************************/

#elif defined( __UNIX__ ) && \
	  !( defined( __MVS__ ) || defined( __TANDEM_NSK__ ) || \
		 defined( __TANDEM_OSS__ ) )

#include <sys/time.h>

/* For performance evaluation purposes we provide the following function,
   which returns ticks of the 1us timer */

long getTickCount( long startTime )
	{
	struct timeval tv;
	long timeLSB, timeDifference;

	/* Only accurate to about 1us */
	gettimeofday( &tv, NULL );
	timeLSB = tv.tv_usec;

	/* If we're getting an initial time, return an absolute value */
	if( startTime <= 0 )
		return( timeLSB );

	/* We're getting a time difference */
	if( startTime < timeLSB )
		timeDifference = timeLSB - startTime;
	else
		/* gettimeofday() rolls over at 1M us */
		timeDifference = ( 1000000L - startTime ) + timeLSB;
	if( timeDifference <= 0 )
		{
		printf( "Error: Time difference = %lX, startTime = %lX, "
				"endTime = %lX.\n", timeDifference, startTime, timeLSB );
		return( 1 );
		}
	return( timeDifference );
	}

/* SunOS and older Slowaris have broken sprintf() handling.  In SunOS 4.x
   this was documented as returning a pointer to the output data as per the
   Berkeley original.  Under Slowaris the manpage was changed so that it
   looks like any other sprintf(), but it still returns the pointer to the
   output buffer in some versions so we use a wrapper that checks at
   runtime to see what we've got and adjusts its behaviour accordingly.  In
   fact it's much easier to fix than that, since we have to use vsprintf()
   anyway and this doesn't have the sprintf() problem, this fixes itself
   simply from the use of the wrapper (unfortunately we can't use 
   vsnprintf() because these older OS versions don't include it yet) */

#if defined( sun ) && ( OSVERSION <= 5 )

#include <stdarg.h>

int fixedSprintf( char *buffer, const int bufSize, const char *format, ... )
	{
	va_list argPtr;
	int length;

	va_start( argPtr, format );
	length = vsprintf( buffer, format, argPtr );
	va_end( argPtr );

	return( length );
	}
#endif /* Old SunOS */

/****************************************************************************
*																			*
*									Windows									*
*																			*
****************************************************************************/

#elif defined( __WIN32__ )

/* Yielding a thread on an SMP or HT system is a tricky process,
   particularly on an HT system.  On an HT CPU the OS (or at least apps
   running under the OS) think that there are two independent CPUs present,
   but it's really just one CPU with partitioning of pipeline slots.  So
   when one thread yields, the only effect is that all of its pipeline slots
   get marked as available.  Since the other thread can't utilise those
   slots, the first thread immediately reclaims them and continues to run.
   In addition thread scheduling varies across OS versions, the WinXP
   scheduler was changed to preferentially schedule threads on idle physical
   processors rather than an idle logical processor on a physical processor
   whose other logical processor is (potentially) busy.

   There isn't really any easy way to fix this since it'd require a sleep 
   that works across all CPUs, however one solution is to make the thread 
   sleep for a nonzero time limit iff it's running on a multi-CPU system.  
   There's a second problem though, which relates to thread priorities.  If 
   we're at a higher priority than the other thread then we can call 
   Sleep( 0 ) as much as we like, but the scheduler will never allow the 
   other thread to run since we're a higher-priority runnable thread.  As a 
   result, as soon as we release our timeslice the scheduler will restart us 
   again (the Windows scheduler implements a starvation-prevention mechanism 
   via the balance set manager, but this varies across scheduler versions 
   and isn't something that we want to rely on).  In theory we could do:

		x = GetThreadPriority( GetCurrentThread() );
		SetThreadPriority( GetCurrentThread(), x - 5 );
		Sleep( 0 );		// Needed to effect priority change
		<wait loop>
		SetThreadPriority( GetCurrentThread(), x );
		Sleep( 0 );

   however this is somewhat problematic if the caller is also messing with 
   priorities at the same time.  In fact it can get downright nasty because 
   the balance set manager will, if a thread has been starved for ~3-4 
   seconds, give it its own priority boost to priority 15 (time-critical) to 
   ensure that it'll be scheduled, with the priority slowly decaying back to 
   the normal level each time that it's scheduled.  In addition it'll have 
   its scheduling quantum boosted to 2x the normal duration for a client OS 
   or 4x the normal duration for a server OS.

   To solve this, we always force our thread to go to sleep (to allow a 
   potentially lower-priority thread to leap in and get some work done) even 
   on a single-processor system, but use a slightly longer wait on an 
   HT/multi-processor system.

   (Actually this simplified view isn't quite accurate since on a HT system 
   the scheduler executes the top *two* threads on the two logical 
   processors and on a dual-CPU system they're executed on a physical 
   processor.  In addition on a HT system a lower-priority thread on one 
   logical processor can compete with a higher-priority thread on the other
   logical processor since the hardware isn't aware of thread priorities) */

void threadYield( void )
	{
	static int sleepTime = -1;

	/* If the sleep time hasn't been determined yet, get it now */
	if( sleepTime < 0 )
		{
		SYSTEM_INFO systemInfo;

		GetSystemInfo( &systemInfo );
		sleepTime = ( systemInfo.dwNumberOfProcessors > 1 ) ? 10 : 1;
		}

	/* Yield the CPU for this thread */
	Sleep( sleepTime );
	}

#ifndef NDEBUG

/* For performance evaluation purposes we provide the following function,
   which returns ticks of the 3.579545 MHz hardware timer (see the long
   comment in rndwin32.c for more details on Win32 timing issues) */

CHECK_RETVAL_RANGE( 0, INT_MAX ) \
long getTickCount( long startTime )
	{
	long timeLSB, timeDifference;

#ifndef __BORLANDC__
	LARGE_INTEGER performanceCount;

	/* Sensitive to context switches */
	QueryPerformanceCounter( &performanceCount );
	timeLSB = performanceCount.LowPart;
#else
	FILETIME dummyTime, kernelTime, userTime;

	/* Only accurate to 10ms, returns constant values in VC++ debugger */
	GetThreadTimes( GetCurrentThread(), &dummyTime, &dummyTime,
					&kernelTime, &userTime );
	timeLSB = userTime.dwLowDateTime;
#endif /* BC++ vs. everything else */

	/* If we're getting an initial time, return an absolute value */
	if( startTime <= 0 )
		return( timeLSB );

	/* We're getting a time difference */
	if( startTime < timeLSB )
		timeDifference = timeLSB - startTime;
	else
		{
		/* Windows rolls over at INT_MAX */
		timeDifference = ( 0xFFFFFFFFUL - startTime ) + 1 + timeLSB;
		}
	if( timeDifference <= 0 )
		{
		printf( "Error: Time difference = %lX, startTime = %lX, "
				"endTime = %lX.\n", timeDifference, startTime, timeLSB );
		return( 1 );
		}
	return( timeDifference );
	}
#endif /* Debug version */

/* Borland C++ before 5.50 doesn't have snprintf() so we fake it using
   sprintf().  Unfortunately these are all va_args functions so we can't 
   just map them using macros but have to provide an explicit wrapper to get 
   rid of the size argument */

#if defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x0550 )

int bcSnprintf( char *buffer, const int bufSize, const char *format, ... )
	{
	va_list argPtr;
	int length;

	va_start( argPtr, format );
	length = vsprintf( buffer, format, argPtr );
	va_end( argPtr );

	return( length );
	}

int bcVsnprintf( char *buffer, const int bufSize, const char *format, va_list argPtr )
	{
	return( vsprintf( buffer, format, argPtr ) );
	}
#endif /* BC++ before 5.50 */

/* Safely load a DLL.  This gets quite complicated because different 
   versions of Windows have changed how they search for DLLs to load, and 
   the behaviour of a specific version of Windows can be changed based on
   registry keys and SetDllDirectory().  Traditionally Windows searched
   the app directory, the current directory, the system directory, the
   Windows directory, and the directories in $PATH.  Windows XP SP2 added
   the SafeDllSearchMode registry key, which changes the search order so
   the current directory is searched towards the end rather than towards
   the start, however it's (apparently) only set on new installs, on a
   pre-SP2 install that's been upgraded it's not set.  Windows Vista and
   newer enabled this safe behaviour by default, but even there 
   SetDefaultDllDirectories() can be used to explicitly re-enable unsafe
   behaviour, and AddDllDirectory() can be used to add a path to the set of 
   DLL search paths and SetDllDirectory() can be used to add a new directory 
   to the start of the search order.

   None of these options are terribly useful if we want a DLL to either
   be loaded from the system directory or not at all.  To handle this we
   build an absolute load path and prepend it to the name of the DLL
   being loaded */

#if 0	/* Older code using SHGetFolderPath() */

/* The documented behaviour for the handling of the system directory under 
   Win64 seems to be more or less random:

	http://msdn.microsoft.com/en-us/library/bb762584%28VS.85%29.aspx: 
		CSIDL_SYSTEM = %windir%/System32, CSIDL_SYSTEMX86 = %windir%/System32.
	http://social.technet.microsoft.com/Forums/en/appvgeneralsequencing/thread/c58f7d64-6a23-46f0-998f-0a964c1eff2a:
		CSIDL_SYSTEM = %windir%/Syswow64.
	http://msdn.microsoft.com/en-us/library/windows/desktop/ms538044%28v=vs.85%29.aspx:
		CSIDL_SYSTEM = %windir%/System32, CSIDL_SYSTEMX86 = %windir%/Syswow64.
	http://social.msdn.microsoft.com/Forums/en-US/vcgeneral/thread/f9a54564-1006-42f9-b4d1-b225f370c60c:
		GetSystemDirectory() = %windir%/Syswow64.
	http://msdn.microsoft.com/en-us/library/windows/desktop/dd378457%28v=vs.85%29.aspx:
		CSIDL_SYSTEM = %windir%/System32, 
		CSIDL_SYSTEMX86 = %windir%/System32 for Win32, %windir%/Syswow64 for Win64.

   The current use of CSIDL_SYSTEM to get whatever-the-system-directory-is-
   meant-to-be seems to work, so we'll leave it as is */

#ifndef CSIDL_SYSTEM
  #define CSIDL_SYSTEM		0x25	/* 'Windows/System32' */
#endif /* !CSIDL_SYSTEM */
#ifndef SHGFP_TYPE_CURRENT
  #define SHGFP_TYPE_CURRENT	0
#endif /* !SHGFP_TYPE_CURRENT */

static HMODULE WINAPI loadExistingLibrary( IN_STRING LPCTSTR lpFileName )
	{
	HANDLE hFile;

	assert( isReadPtr( lpFileName, 2 ) );

	ANALYSER_HINT_STRING( lpFileName );

	/* Determine whether the DLL is present and accessible */
	hFile = CreateFile( lpFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL, NULL );
	if( hFile == INVALID_HANDLE_VALUE )
		return( NULL );
	CloseHandle( hFile );

	return( LoadLibrary( lpFileName ) );
	}

static HMODULE WINAPI loadFromSystemDirectory( IN_BUFFER( fileNameLength ) \
													const char *fileName,
											   IN_LENGTH_SHORT_MIN( 1 ) \
													const int fileNameLength )
	{
	char path[ MAX_PATH + 8 ];
	int pathLength;

	ENSURES_EXT( fileNameLength >= 1 && fileNameLength + 8 < MAX_PATH, \
				 NULL );

	assert( isReadPtr( fileName, fileNameLength ) );

	/* Get the path to a DLL in the system directory */
	pathLength = \
		GetSystemDirectory( path, MAX_PATH - ( fileNameLength + 8 ) );
	if( pathLength < 3 || pathLength + fileNameLength > MAX_PATH - 8 )
		return( NULL );
	path[ pathLength++ ] = '\\';
	memcpy( path + pathLength, fileName, fileNameLength );
	path[ pathLength + fileNameLength ] = '\0';

	return( loadExistingLibrary( path ) );
	}

HMODULE WINAPI SafeLoadLibrary( IN_STRING LPCTSTR lpFileName )
	{
	typedef HRESULT ( WINAPI *SHGETFOLDERPATH )( HWND hwndOwner,
										int nFolder, HANDLE hToken,
										DWORD dwFlags, LPTSTR lpszPath );
	typedef struct {
		const char *dllName; const int dllNameLen; 
		} DLL_NAME_INFO;
	static const DLL_NAME_INFO dllNameInfoTbl[] = {
		{ "Crypt32.dll", 11 }, { "ComCtl32.dll", 12 },
		{ "dnsapi.dll", 10 }, { "Mpr.dll", 7 },
		{ "NetAPI32.dll", 12 }, { "ODBC32.dll", 10 },
		{ "SetupAPI.dll", 12 }, { "SHFolder.dll", 12 },
		{ "Shell32.dll", 11 }, { "WinHTTP.dll", 11 },
		{ "wldap32.dll", 11 }, { "ws2_32.dll", 10 },
		{ "wsock32.dll", 11 }, 
		{ NULL, 0 }, { NULL, 0 }
		};
	SHGETFOLDERPATH pSHGetFolderPath;
	HINSTANCE hShell32;
	char path[ MAX_PATH + 8 ];
	const int fileNameLength = strlen( lpFileName );
	int pathLength, i;
	BOOLEAN gotPath = FALSE;

	ANALYSER_HINT_STRING( lpFileName );

	REQUIRES_EXT( fileNameLength >= 1 && fileNameLength < MAX_PATH, \
				  NULL );

	assert( isReadPtr( lpFileName, 2 ) );

	/* If it's Win98 or NT4, just call LoadLibrary directly.  In theory
	   we could try a few further workarounds (see io/file.c) but in 
	   practice bending over backwards to fix search path issues under
	   Win98, which doesn't have ACLs to protect the files in the system
	   directory anyway, isn't going to achieve much, and in any case both
	   of these OSes should be long dead by now */
	if( getSysVar( SYSVAR_OSMAJOR ) <= 4 )
		return( LoadLibrary( lpFileName ) );

	/* If it's already an absolute path, don't try and override it */
	if( lpFileName[ 0 ] == '/' || \
		( fileNameLength >= 3 && isAlpha( lpFileName[ 0 ] ) && \
		  lpFileName[ 1 ] == ':' && lpFileName[ 2 ] == '/' ) )
		return( loadExistingLibrary( lpFileName ) );

	/* If it's a well-known DLL, load it from the system directory */
	for( i = 0; dllNameInfoTbl[ i ].dllName != NULL && \
				i < FAILSAFE_ARRAYSIZE( dllNameInfoTbl, DLL_NAME_INFO ); i++ )
		{
		if( dllNameInfoTbl[ i ].dllNameLen == fileNameLength && \
			!strCompare( dllNameInfoTbl[ i ].dllName, lpFileName, 
						 fileNameLength ) )
			{
			/* It's a standard system DLL, load it from the system 
			   directory */
			return( loadFromSystemDirectory( lpFileName, fileNameLength ) );
			}
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( dllNameInfoTbl, DLL_NAME_INFO ) );

	/* It's a system new enough to support SHGetFolderPath(), get the path
	   to the system directory.  Unfortunately at this point we're in a 
	   catch-22, in order to resolve SHGetFolderPath() we need to call
	   Shell32.dll and if an attacker uses that as the injection point then
	   they can give us a SHGetFolderPath() that'll do whatever they want.  
	   There's no real way to fix this because we have to load Shell32 at
	   some point, either explicitly here or on program load, and since we
	   can't control the load path at either point we can't control what's
	   actually being loaded.  In addition DLLs typically recursively load
	   more DLLs so even if we can control the path of the DLL that we load 
	   directly we can't influence the paths over which further DLLs get 
	   loaded.  So unfortunately the best that we can do is make the 
	   attacker work a little harder rather than providing a full fix */
	hShell32 = loadFromSystemDirectory( "Shell32.dll", 11 );
	if( hShell32 != NULL )
		{
		pSHGetFolderPath = ( SHGETFOLDERPATH ) \
						   GetProcAddress( hShell32, "SHGetFolderPathA" );
		if( pSHGetFolderPath != NULL && \
			pSHGetFolderPath( NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, 
							  path ) == S_OK )
			gotPath = TRUE;
		FreeLibrary( hShell32 );
		}
	if( !gotPath )
		{
		/* If for some reason we couldn't get the path to the Windows system
		   directory this means that there's something drastically wrong,
		   don't try and go any further */
		return( NULL );
		}
	pathLength = strlen( path );
	if( pathLength < 3 || pathLength + fileNameLength > MAX_PATH - 8 )
		{
		/* Under WinNT and Win2K the LocalSystem account doesn't have its 
		   own profile so SHGetFolderPath() will report success but return a 
		   zero-length path if we're running as a service.  To detect this 
		   we have to check for a valid-looking path as well as performing a 
		   general check on the return status.
		   
		   In effect prepending a zero-length path to the DLL name just 
		   turns the call into a standard LoadLibrary() call, but we make 
		   the action explicit here.  Unfortunately this reintroduces the
		   security hole that we were trying to fix, and what's worse it's
		   for the LocalSystem account (sigh). */
		return( LoadLibrary( lpFileName ) );
		}
	path[ pathLength++ ] = '\\';
	memcpy( path + pathLength, lpFileName, fileNameLength );
	path[ pathLength + fileNameLength ] = '\0';

	return( loadExistingLibrary( path ) );
	}
#else	/* Newer code that does the same thing */

#if VC_GE_2005( _MSC_VER )
  #pragma warning( push )
  #pragma warning( disable : 4255 )	/* Errors in VersionHelpers.h */
  #include <VersionHelpers.h>
  #pragma warning( pop )
#endif /* VC++ >= 2005 */

HMODULE WINAPI SafeLoadLibrary( IN_STRING LPCTSTR lpFileName )
	{
	char path[ MAX_PATH + 8 ];
	const int fileNameLength = strlen( lpFileName );
	int pathLength;

	REQUIRES_EXT( fileNameLength >= 1 && fileNameLength < MAX_PATH, \
				  NULL );

	assert( isReadPtr( lpFileName, 2 ) );

	ANALYSER_HINT_STRING( lpFileName );

	/* If it's Win98 or NT4, just call LoadLibrary directly.  In theory
	   we could try a few further workarounds (see io/file.c) but in 
	   practice bending over backwards to fix search path issues under
	   Win98, which doesn't have ACLs to protect the files in the system
	   directory anyway, isn't going to achieve much, and in any case both
	   of these OSes should be long dead by now */
#if VC_LT_2010( _MSC_VER )
	if( getSysVar( SYSVAR_OSMAJOR ) <= 4 )
		return( LoadLibrary( lpFileName ) );
#else
	if( !IsWindowsXPOrGreater() )
		return( LoadLibrary( lpFileName ) );
#endif /* VC++ < 2010 */

	/* If it's already an absolute path, don't try and override it */
	if( lpFileName[ 0 ] == '/' || \
		lpFileName[ 0 ] == '\\' || \
		( fileNameLength >= 3 && isAlpha( lpFileName[ 0 ] ) && \
		  lpFileName[ 1 ] == ':' && \
		  ( lpFileName[ 2 ] == '/' || lpFileName[ 2 ] == '\\' ) ) )
		return( LoadLibrary( lpFileName ) );

	/* Load the DLL from the system directory */
	pathLength = \
		GetSystemDirectory( path, MAX_PATH - ( fileNameLength + 8 ) );
	if( pathLength < 3 || pathLength + fileNameLength > MAX_PATH - 8 )
		return( NULL );
	path[ pathLength++ ] = '\\';
	memcpy( path + pathLength, lpFileName, fileNameLength );
	path[ pathLength + fileNameLength ] = '\0';

	return( LoadLibrary( path ) );
	}
#endif /* Old/New SafeLoadLibrary() */

/* Windows NT/2000/XP/Vista support ACL-based access control mechanisms for 
   system objects so when we create objects such as files and threads we 
   give them an ACL that allows only the creator access.  The following 
   functions return the security info needed when creating objects.  The 
   interface for this has changed in every major OS release, although it 
   never got any better, just differently ugly.  The following code uses the 
   original NT 3.1 interface, which works for all OS versions */

/* The size of the buffer for ACLs and the user token */

#define ACL_BUFFER_SIZE		1024
#define TOKEN_BUFFER_SIZE	256

/* A composite structure to contain the various ACL structures.  This is
   required because ACL handling is a complex, multistage operation that
   requires first creating an ACL and security descriptor to contain it,
   adding an access control entry (ACE) to the ACL, adding the ACL as the
   DACL of the security descriptor, and finally, wrapping the security
   descriptor up in a security attributes structure that can be passed to
   an object-creation function.

   The handling of the TOKEN_INFO is extraordinarily ugly because although
   the TOKEN_USER struct as defined is only 8 bytes long, Windoze allocates
   an extra 24 bytes after the end of the struct into which it stuffs data
   that the SID pointer in the TOKEN_USER struct points to.  This means that
   we can't statically allocate memory of the size of the TOKEN_USER struct
   but have to make it a pointer into a larger buffer that can contain the
   additional invisible data tacked onto the end */

typedef struct {
	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR pSecurityDescriptor;
	PACL pAcl;
	PTOKEN_USER pTokenUser;
	BYTE aclBuffer[ ACL_BUFFER_SIZE + 8 ];
	BYTE tokenBuffer[ TOKEN_BUFFER_SIZE + 8 ];
	} SECURITY_INFO;

/* Initialise an ACL allowing only the creator access and return it to the
   caller as an opaque value */

CHECK_RETVAL_PTR \
void *initACLInfo( const int access )
	{
	SECURITY_INFO *securityInfo;
	HANDLE hToken = INVALID_HANDLE_VALUE;	/* See comment below */
	BOOLEAN tokenOK = FALSE;

	REQUIRES_N( access > 0 );

	/* Allocate and initialise the composite security info structure */
	if( ( securityInfo = \
				clAlloc( "initACLInfo", sizeof( SECURITY_INFO ) ) ) == NULL )
		return( NULL );
	memset( securityInfo, 0, sizeof( SECURITY_INFO ) );
	securityInfo->pAcl = ( PACL ) securityInfo->aclBuffer;
	securityInfo->pTokenUser = ( PTOKEN_USER ) securityInfo->tokenBuffer;

	/* Get the security token for this thread.  First we try for the thread
	   token (which it typically only has when impersonating), if we don't
	   get that we use the token associated with the process.  We also
	   initialise the hToken (above) even though it shouldn't be necessary
	   because Windows tries to read its contents, which indicates there
	   might be problems if it happens to start out with the wrong value */
	if( OpenThreadToken( GetCurrentThread(), TOKEN_QUERY, FALSE, &hToken ) || \
		OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &hToken ) )
		{
		DWORD cbTokenUser;

		tokenOK = GetTokenInformation( hToken, TokenUser,
									   securityInfo->pTokenUser,
									   TOKEN_BUFFER_SIZE, &cbTokenUser );
		CloseHandle( hToken );
		}
	if( !tokenOK )
		{
		clFree( "initACLInfo", securityInfo );
		return( NULL );
		}

	/* Set a security descriptor owned by the current user */
	if( !InitializeSecurityDescriptor( &securityInfo->pSecurityDescriptor,
									   SECURITY_DESCRIPTOR_REVISION ) || \
		!SetSecurityDescriptorOwner( &securityInfo->pSecurityDescriptor,
									 securityInfo->pTokenUser->User.Sid,
									 FALSE ) )
		{
		clFree( "initACLInfo", securityInfo );
		return( NULL );
		}

	/* Set up the discretionary access control list (DACL) with one access
	   control entry (ACE) for the current user */
	if( !InitializeAcl( securityInfo->pAcl, ACL_BUFFER_SIZE,
						ACL_REVISION ) || \
		!AddAccessAllowedAce( securityInfo->pAcl, ACL_REVISION, access,
							  securityInfo->pTokenUser->User.Sid ) )
		{
		clFree( "initACLInfo", securityInfo );
		return( NULL );
		}

	/* Bind the DACL to the security descriptor */
	if( !SetSecurityDescriptorDacl( &securityInfo->pSecurityDescriptor, TRUE,
									securityInfo->pAcl, FALSE ) )
		{
		clFree( "initACLInfo", securityInfo );
		return( NULL );
		}

	assert( IsValidSecurityDescriptor( &securityInfo->pSecurityDescriptor ) );

	/* Finally, set up the security attributes structure */
	securityInfo->sa.nLength = sizeof( SECURITY_ATTRIBUTES );
	securityInfo->sa.bInheritHandle = FALSE;
	securityInfo->sa.lpSecurityDescriptor = &securityInfo->pSecurityDescriptor;

	return( securityInfo );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void freeACLInfo( IN TYPECAST( SECURITY_INFO * ) void *securityInfoPtr )
	{
	SECURITY_INFO *securityInfo = ( SECURITY_INFO * ) securityInfoPtr;

	assert( securityInfoPtr == NULL || \
			isWritePtr( securityInfoPtr, sizeof( SECURITY_INFO ) ) );

	if( securityInfo == NULL )
		return;
	clFree( "freeACLInfo", securityInfo );
	}

/* Extract the security info needed in Win32 API calls from the collection of
   security data that we set up earlier */

void *getACLInfo( INOUT_OPT TYPECAST( SECURITY_INFO * ) void *securityInfoPtr )
	{
	SECURITY_INFO *securityInfo = ( SECURITY_INFO * ) securityInfoPtr;

	assert( securityInfo == NULL || \
			isWritePtr( securityInfo, sizeof( SECURITY_INFO ) ) );

	return( ( securityInfo == NULL ) ? NULL : &securityInfo->sa );
	}

/* The DLL entry point.  In theory we could also call:
 
	HeapSetInformation( GetProcessHeap(), 
						HeapEnableTerminationOnCorruption, NULL, 0 ); 

   but this would have to be dynamically linked since it's only available 
   for Vista and newer OSes, and it could also cause problems when cryptlib 
   is linked with buggy applications that rely on the resilience of the heap 
   manager in order to function since running the app with cryptlib will 
   cause it to crash through no fault of cryptlib's.  Since cryptlib is 
   checked with Bounds Checker, Purify, and Valgrind, which are far more 
   rigorous than the checking performed by the heap manager, there doesn't 
   seem to be much advantage in doing this, but significant disadvantages 
   if users' application bugs are caught by it */

#if !( defined( NT_DRIVER ) || defined( STATIC_LIB ) )

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
	{
	UNUSED_ARG( hinstDLL );
	UNUSED_ARG( lpvReserved );

	switch( fdwReason )
		{
		case DLL_PROCESS_ATTACH:
			/* Disable thread-attach notifications, which we don't do
			   anything with and therefore don't need */
			DisableThreadLibraryCalls( hinstDLL );

			/* Set up the initialisation lock in the kernel */
			preInit();
			break;

		case DLL_PROCESS_DETACH:
			/* Delete the initialisation lock in the kernel */
			postShutdown();
			break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
		}

	return( TRUE );
	}

/* Idiot-proofing.  Yes, there really are people who'll try and register a
   straight DLL */

#ifndef MB_OK
  #define MB_OK					0x00000000L
  #define MB_ICONQUESTION		0x00000020L
  #define MB_ICONEXCLAMATION	0x00000030L
#endif /* MB_OK */

int WINAPI MessageBoxA( HWND hWnd, LPCSTR lpText, LPCSTR lpCaption,
						UINT uType );

#ifndef _WIN64
  #pragma comment( linker, "/export:DllRegisterServer=_DllRegisterServer@0,PRIVATE" )
#endif /* Win64 */
#if defined( __MINGW32__ ) && !defined( STDAPI )
  #define STDAPI	HRESULT __stdcall
#endif /* MinGW without STDAPI defined */

STDAPI DllRegisterServer( void )
	{
	MessageBoxA( NULL, "Why are you trying to register the cryptlib DLL?\n"
				 "It's just a standard Windows DLL, there's nothing\nto be "
				 "registered.", "ESO Error",
				 MB_ICONQUESTION | MB_OK );
	return( E_NOINTERFACE );
	}
#endif /* !( NT_DRIVER || STATIC_LIB ) */

#if VC_LE_VC6( _MSC_VER )

/* Under VC++ 6 assert() can randomly stop working so that only the abort() 
   portion still functions, making it impossible to find out what went wrong.
   To deal with this, os_spec.h redefines the assert() macro to call the 
   following function, which emulates what a correct-functioning assert()
   would do */

#ifndef MB_OKCANCEL
  #define MB_OKCANCEL		0x00000001L
#endif /* MB_OKCANCEL */
#ifndef MB_YESNOCANCEL
  #define MB_YESNOCANCEL	0x00000003L
#endif /* MB_YESNOCANCEL */

void vc6assert( const char *exprString, const char *fileName, 
				const int lineNo )
	{
	const char *cleanFileName;
	char string[ 1024 ], title[ 1024 ];
	int result;

	/* Clean up the pathname */
	cleanFileName = strstr( fileName, "cryptlib" );
	if( cleanFileName != NULL )
		cleanFileName += 9;	/* Skip "cryptlib/" */
	else
		{
		int i, slashCount = 0;

		/* We may be running off some nonstandard path, get as close as we
		   can to the minimal path */
		for( i = strlen( fileName ); i > 0; i-- )
			{
			if( fileName[ i ] == '\\' )
				{
				slashCount++;
				if( slashCount >= 2 )
					break;
				}
			}
		cleanFileName = fileName + i;
		}

	/* Log the output to the debug console */
	sprintf( string, "%s:%d:'%s'.\n", fileName, lineNo, exprString );
	OutputDebugString( string );

	/* Emulate the standard assert() functionality.  Note that the spurious 
	   spaces in the last line of the message are to ensure that the title
	   text doesn't get truncated, since the message box width is determined 
	   by the text in the dialog rather than the title length */
	sprintf( string, "File %s, line %d:\n\n  '%s'.\n\n"
			"Yes to debug, no to continue, cancel to exit.                  ", 
			cleanFileName, lineNo, exprString );
	sprintf( title, "Assertion failed, file %s, line %d", cleanFileName, 
			 lineNo );
	result = MessageBoxA( NULL, string, title, 
						  MB_ICONEXCLAMATION | MB_YESNOCANCEL );
	if( result == IDCANCEL )
		abort();
	if( result == IDYES )
		DebugBreak();
	}
#endif /* VC++ 6.0 */

/* Borland's archaic compilers don't recognise DllMain() but still use the
   OS/2-era DllEntryPoint(), so we have to alias it to DllMain() in order
   for things to be initialised properly */

#if defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x550 )

BOOL WINAPI DllEntryPoint( HINSTANCE hinstDLL, DWORD fdwReason, \
						   LPVOID lpvReserved )
	{
	return( DllMain( hinstDLL, fdwReason, lpvReserved ) );
	}
#endif /* BC++ */

#elif defined( __WIN16__ )

/* WinMain() and WEP() under Win16 are intended for DLL initialisation,
   however it isn't possible to reliably do anything terribly useful in these
   routines.  The reason for this is that the WinMain/WEP functions are
   called by the windows module loader, which has a very limited workspace
   and can cause peculiar behaviour for some functions (allocating/freeing
   memory and loading other modules from these routines is unreliable), the
   order in which WinMain() and WEP() will be called for a set of DLL's is
   unpredictable (sometimes WEP doesn't seem to be called at all), and they
   can't be tracked by a standard debugger.  This is why MS have
   xxxRegisterxxx() and xxxUnregisterxxx() functions in their DLL's.

   Under Win16 on a Win32 system this isn't a problem because the module
   loader has been rewritten to work properly, but it isn't possible to get
   reliable performance under pure Win16, so the DLL entry/exit routines here
   do almost nothing, with the real work being done in cryptInit()/
   cryptEnd() */

HWND hInst;

int CALLBACK LibMain( HINSTANCE hInstance, WORD wDataSeg, WORD wHeapSize, \
					  LPSTR lpszCmdLine )
	{
	/* Remember the proc instance for later */
	hInst = hInstance;

	return( TRUE );
	}

int CALLBACK WEP( int nSystemExit )
	{
	switch( nSystemExit )
		{
		case WEP_SYSTEM_EXIT:
			/* System is shutting down */
			break;

		case WEP_FREE_DLL:
			/* DLL reference count = 0, DLL-only shutdown */
			break;
		}

	return( TRUE );
	}

/* Check whether we're running inside a VM, which is a potential risk for
   cryptovariables.  It gets quite tricky to detect the various VMs so for
   now the only one that we detect is the most widespread one, VMware */

#if defined( __WIN32__ )  && !defined( NO_ASM )

BOOLEAN isRunningInVM( void )
	{
	unsigned int magicValue, version;

	__try {
	__asm {
		push eax
		push ebx
		push ecx
		push edx

		/* Check for VMware via the VMware guest-to-host communications 
		   channel */
		mov eax, 'VMXh'		/* VMware magic value 0x564D5868 */
		xor ebx, ebx		/* Clear parameters register */
		mov ecx, 0Ah		/* Get-version command */
		mov dx, 'VX'		/* VMware I/O port 0x5658 */
		in eax, dx			/* Perform VMware call */
		mov magicValue, ebx	/* VMware magic value */
		mov version, ecx	/* VMware version */

		pop edx
		pop ecx
		pop ebx
		pop eax
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {}

	return( magicValue == 'VMXh' ) ? TRUE : FALSE );
	}
#else

BOOLEAN isRunningInVM( void )
	{
	return( FALSE );
	}
#endif /* __WIN32__ && !NO_ASM */

/****************************************************************************
*																			*
*									Windows CE								*
*																			*
****************************************************************************/

#elif defined( __WINCE__ )

/* Windows CE doesn't provide ANSI standard time functions (although it'd be
   relatively easy to do so, and they are in fact provided in MFC), so we
   have to provide our own */

CHECK_RETVAL \
static LARGE_INTEGER *getTimeOffset( void )
	{
	static LARGE_INTEGER timeOffset = { 0 };

	/* Get the difference between the ANSI/ISO C time epoch and the Windows
	   time epoch if we haven't already done so (we could also hardcode this
	   in as 116444736000000000LL) */
	if( timeOffset.QuadPart == 0 )
		{
		SYSTEMTIME ofsSystemTime;
		FILETIME ofsFileTime;

		memset( &ofsSystemTime, 0, sizeof( SYSTEMTIME ) );
		ofsSystemTime.wYear = 1970;
		ofsSystemTime.wMonth = 1;
		ofsSystemTime.wDay = 1;
		SystemTimeToFileTime( &ofsSystemTime, &ofsFileTime );
		timeOffset.HighPart = ofsFileTime.dwHighDateTime;
		timeOffset.LowPart = ofsFileTime.dwLowDateTime;
		}

	return( &timeOffset );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static time_t fileTimeToTimeT( const FILETIME *fileTime )
	{
	const LARGE_INTEGER *timeOffset = getTimeOffset();
	LARGE_INTEGER largeInteger;

	/* Convert a Windows FILETIME to a time_t by dividing by
	   10,000,000 (to go from 100ns ticks to 1s ticks) */
	largeInteger.HighPart = fileTime->dwHighDateTime;
	largeInteger.LowPart = fileTime->dwLowDateTime;
	largeInteger.QuadPart = ( largeInteger.QuadPart - \
							  timeOffset->QuadPart ) / 10000000L;
	if( sizeof( time_t ) == 4 && \
		largeInteger.QuadPart > 0x80000000UL )
		{
		/* time_t is 32 bits but the converted time is larger than a 32-bit
		   signed value, indicate that we couldn't convert it.  In theory
		   we could check for largeInteger.HighPart == 0 and perform a
		   second check to see if time_t is unsigned, but it's unlikely that
		   this change would be made to the VC++ runtime time_t since it'd
		   break too many existing apps */
		return( -1 );
		}
	return( ( time_t ) largeInteger.QuadPart );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static void timeTToFileTime( FILETIME *fileTime, const time_t timeT )
	{
	const LARGE_INTEGER *timeOffset = getTimeOffset();
	LARGE_INTEGER largeInteger = { timeT };

	/* Convert a time_t to a Windows FILETIME by multiplying by
	   10,000,000 (to go from 1s ticks to 100ns ticks) */
	largeInteger.QuadPart = ( largeInteger.QuadPart * 10000000L ) + \
							timeOffset->QuadPart;
	fileTime->dwHighDateTime = largeInteger.HighPart;
	fileTime->dwLowDateTime = largeInteger.LowPart;
	}

time_t time( time_t *timePtr )
	{
	FILETIME fileTime;
#ifdef __WINCE__
	SYSTEMTIME systemTime;
#endif /* __WINCE__ */

	assert( timePtr == NULL );

	/* Get the time via GetSystemTimeAsFileTime().  Windows CE doesn't have
	   the unified call so we have to assemble it from discrete calls */
#ifdef __WINCE__
	GetSystemTime( &systemTime );
	SystemTimeToFileTime( &systemTime, &fileTime );
#else
	GetSystemTimeAsFileTime( &fileTime );
#endif /* Win32 vs. WinCE */

	return( fileTimeToTimeT( &fileTime ) );
	}

time_t mktime( struct tm *tmStruct )
	{
	SYSTEMTIME systemTime;
	FILETIME fileTime;

	assert( isWritePtr( tmStruct, sizeof( struct tm ) ) );

	/* Use SystemTimeToFileTime() as a mktime() substitute.  The input time
	   seems to be treated as local time, so we have to convert it to GMT
	   before we return it */
	memset( &systemTime, 0, sizeof( SYSTEMTIME ) );
	systemTime.wYear = tmStruct->tm_year + 1900;
	systemTime.wMonth = tmStruct->tm_mon + 1;
	systemTime.wDay = tmStruct->tm_mday;
	systemTime.wHour = tmStruct->tm_hour;
	systemTime.wMinute = tmStruct->tm_min;
	systemTime.wSecond = tmStruct->tm_sec;
	SystemTimeToFileTime( &systemTime, &fileTime );
	LocalFileTimeToFileTime( &fileTime, &fileTime );

	return( fileTimeToTimeT( &fileTime ) );
	}

struct tm *gmtime( const time_t *timePtr )
	{
	static struct tm tmStruct;
	SYSTEMTIME systemTime;
	FILETIME fileTime;

	assert( isReadPtr( timePtr, sizeof( time_t ) ) );

	/* Use FileTimeToSystemTime() as a gmtime() substitute.  Note that this
	   function, like its original ANSI/ISO C counterpart, is not thread-
	   safe */
	timeTToFileTime( &fileTime, *timePtr );
	FileTimeToSystemTime( &fileTime, &systemTime );
	memset( &tmStruct, 0, sizeof( struct tm ) );
	tmStruct.tm_year = systemTime.wYear - 1900;
	tmStruct.tm_mon = systemTime.wMonth - 1;
	tmStruct.tm_mday = systemTime.wDay;
	tmStruct.tm_hour = systemTime.wHour;
	tmStruct.tm_min = systemTime.wMinute;
	tmStruct.tm_sec = systemTime.wSecond;

	return( &tmStruct );
	}

/* When running in debug mode we provide a debugging printf() that sends its 
   output to the debug console.  This is normally done via a macro in a 
   header file that remaps the debug-output macros to the appropriate 
   function, but WinCE's NKDbgPrintfW() requires widechar strings that 
   complicate the macros so we provide a function that performs the 
   conversion before outputting the text */

#if !defined( NDEBUG )

int debugPrintf( const char *format, ... )
	{
	va_list argPtr;
	char buffer[ 1024 ];
	wchar_t wcBuffer[ 1024 ];
	int length, status;

	va_start( argPtr, format );
	length = vsprintf( buffer, format, argPtr );
	va_end( argPtr );
	status = asciiToUnicode( wcBuffer, 1024, buffer, length );
	if( cryptStatusOK( status ) )
		NKDbgPrintfW( L"%s", wcBuffer );
	return( length );
	}
#endif /* Debug build */

/* Windows CE systems need to convert characters from ASCII <-> Unicode
   before/after they're read/written to external formats, the following
   functions perform the necessary conversion.

   winnls.h was already included via the global include of windows.h, however
   it isn't needed for any other part of cryptlib so it was disabled via
   NONLS.  Since winnls.h is now locked out, we have to un-define the guards
   used earlier to get it included */

#undef _WINNLS_
#undef NONLS
#include <winnls.h>

int asciiToUnicode( wchar_t *dest, const int destMaxLen, 
					const char *src, const int length )
	{
	int status;

	assert( isReadPtr( src, length ) );
	assert( isWritePtr( dest, destMaxLen ) );

	/* Note that this function doens't terminate the string if the output is 
	   filled, so it's essential that the caller check the return value to 
	   ensure that they're getting a well-formed string */
	status = MultiByteToWideChar( GetACP(), 0, src, destMaxLen, dest, 
								  length );
	return( status <= 0 ? CRYPT_ERROR_BADDATA : status * sizeof( wchar_t ) );
	}

int unicodeToAscii( char *dest, const int destMaxLen, 
					const wchar_t *src, const int length )
	{
	size_t destLen;
	int status;

	assert( isReadPtr( src, length ) );
	assert( isWritePtr( dest, destMaxLen ) );

	/* Convert the string, overriding the system default char '?', which
	   causes problems if the output is used as a filename.  This function
	   has stupid semantics in that instead of returning the number of bytes
	   written to the output it returns the number of bytes specified as
	   available in the output buffer, zero-filling the rest (in addition as 
	   for MultiByteToWideChar() it won't terminate the string if the output 
	   is filled).  Because there's no way to tell how long the resulting 
	   string actually is we have to use wcstombs() instead, which is 
	   unfortunate because there's nothing that we can do with the maxLength 
	   parameter */
#if 0
	status = WideCharToMultiByte( GetACP(), 0, src, length, dest,
								  length * sizeof( wchar_t ), "_", NULL );
	return( ( status <= 0 ) ? CRYPT_ERROR_BADDATA : wcslen( dest ) );
#else
	status = wcstombs_s( &destLen, dest, destMaxLen, src, 
						 length * sizeof( wchar_t ) );
	return( ( status <= 0 ) ? CRYPT_ERROR_BADDATA : status );
#endif
	}

BOOL WINAPI DllMain( HANDLE hinstDLL, DWORD dwReason, LPVOID lpvReserved )
	{
	UNUSED_ARG( hinstDLL );
	UNUSED_ARG( lpvReserved );

	switch( dwReason )
		{
		case DLL_PROCESS_ATTACH:
			/* Disable thread-attach notifications, which we don't do
			   anything with and therefore don't need */
			DisableThreadLibraryCalls( hinstDLL );

			/* Set up the initialisation lock in the kernel */
			preInit();
			break;

		case DLL_PROCESS_DETACH:
			/* Delete the initialisation lock in the kernel */
			postShutdown();
			break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
		}

	return( TRUE );
	}
#endif /* OS-specific support */

/****************************************************************************
*																			*
*							String Function Support							*
*																			*
****************************************************************************/

/* Match a given substring against a string in a case-insensitive manner.
   If possible we use native calls to handle this since they deal with
   charset-specific issues such as collating sequences, however a few OSes
   don't provide this functionality so we have to do it ourselves */

#ifdef NO_NATIVE_STRICMP

int strnicmp( const char *src, const char *dest, int length )
	{
	assert( isReadPtr( src, length ) );

	while( length-- > 0 )
		{
		char srcCh = *src++, destCh = *dest++;

		/* Need to be careful with toupper() side-effects */
		srcCh = toUpper( srcCh );
		destCh = toUpper( destCh );

		if( srcCh != destCh )
			return( srcCh - destCh );
		}

	return( 0 );
	}

int stricmp( const char *src, const char *dest )
	{
	const int length = strlen( src );

	if( length != strlen( dest ) )
		return( 1 );	/* Lengths differ */
	return( strnicmp( src, dest, length ) );
	}
#endif /* NO_NATIVE_STRICMP */

/****************************************************************************
*																			*
*						Minimal Safe String Function Support				*
*																			*
****************************************************************************/

#ifdef NO_NATIVE_STRLCPY

/* Copy and concatenate a string, truncating it if necessary to fit the 
   destination buffer.  Unfortunately the TR 24731 functions don't do this,
   while the OpenBSD safe-string functions do (but don't implement any of
   the rest of the TR 24731 functionality).  Because the idiot maintainer
   of glibc objects to these functions (even Microsoft recognise their
   utility with the _TRUNCATE semantics for strcpy_s/strcat_s), everyone has 
   to manually implement them in their code, as we do here.  Note that these
   aren't completely identical to the OpenBSD functions, in order to fit the 
   TR 24731 pattern we make the length the second paramter, and give them a
   TR 24731-like _s suffix to make them distinct from the standard OpenBSD
   ones (a macro in os_spec.h is sufficient to map this to the proper 
   functions where they're available in libc) */

int strlcpy_s( char *dest, const int destLen, const char *src )
	{
	int i;

	assert( isWritePtr( dest, destLen ) );
	assert( isReadPtr( src, 1 ) );

	/* Copy as much as we can of the source string onto the end of the 
	   destination string */
	for( i = 0; i < destLen - 1 && *src != '\0'; i++ )
		dest[ i ] = *src++;
	dest[ i ] = '\0';

	return( 1 );
	}

int strlcat_s( char *dest, const int destLen, const char *src )
	{
	int i;

	assert( isWritePtr( dest, destLen ) );

	/* See how long the existing destination string is */
	for( i = 0; i < destLen && dest[ i ] != '\0'; i++ );
	if( i >= destLen )
		{
		DEBUG_DIAG(( "Overflow in strlcat_s" ));
		assert( DEBUG_WARN );
		dest[ destLen - 1 ] = '\0';

		return( 1 );
		}

	/* Copy as much as we can of the source string onto the end of the 
	   destination string */
	while( i < destLen - 1 && *src != '\0' )
		dest[ i++ ] = *src++;
	dest[ i ] = '\0';

	return( 1 );
	}
#endif /* NO_NATIVE_STRLCPY */

/****************************************************************************
*																			*
*								SysVars Support								*
*																			*
****************************************************************************/

#if defined( __WIN32__ )  && \
	!( defined( _M_X64 ) || defined( __MINGW32__ ) || defined( NO_ASM ) )

CHECK_RETVAL \
static int getHWInfo( void )
	{
	BOOLEAN hasAdvFeatures = 0;
	char vendorID[ 12 + 8 ];
	unsigned long processorID, featureFlags;
	int sysCaps = 0;

	/* Check whether the CPU supports extended features like CPUID and 
	   RDTSC, and get any info that we need related to this.  There is an 
	   IsProcessorFeaturePresent() function, but all that this provides is 
	   an indication of the availability of rdtsc (alongside some stuff that 
	   we don't care about, like MMX and 3DNow).  Since we still need to 
	   check for the presence of other features, we do the whole thing 
	   ourselves */
	__asm {
		/* Detect the CPU type */
		pushfd
		pop eax				/* Get EFLAGS in eax */
		mov ebx, eax		/* Save a copy for later */
		xor eax, 0x200000	/* Toggle the CPUID bit */
		push eax
		popfd				/* Update EFLAGS */
		pushfd
		pop eax				/* Get updated EFLAGS back in eax */
		push ebx
		popfd				/* Restore original EFLAGS */
		xor eax, ebx		/* Check if we could toggle CPUID bit */
		jz noCPUID			/* Nope, we can't do anything further */
		mov [hasAdvFeatures], 1	/* Remember that we have CPUID */
		mov [sysCaps], HWCAP_FLAG_RDTSC	/* Remember that we have RDTSC */

		/* We have CPUID, see what we've got */
		xor ecx, ecx
		xor edx, edx		/* Tell VC++ that ECX, EDX will be trashed */
		xor eax, eax		/* CPUID function 0: Get vendor ID */
		cpuid
		mov dword ptr [vendorID], ebx
		mov dword ptr [vendorID+4], edx
		mov dword ptr [vendorID+8], ecx	/* Save vendor ID string */
		mov eax, 1			/* CPUID function 1: Get processor info */
		cpuid
		mov [processorID], eax	/* Save processor ID */
		mov [featureFlags], ecx	/* Save processor feature info */
	noCPUID:
		}

	/* If there's no CPUID support, there are no special HW capabilities
	   available */
	if( !hasAdvFeatures )
		return( HWCAP_FLAG_NONE );

	/* If there's a vendor ID present, check for vendor-specific special
	   features */
	if( !memcmp( vendorID, "CentaurHauls", 12 ) )
		{
	__asm {
		xor ebx, ebx
		xor ecx, ecx		/* Tell VC++ that EBX, ECX will be trashed */
		mov eax, 0xC0000000	/* Centaur extended CPUID info */
		cpuid
		cmp eax, 0xC0000001	/* Need at least release 2 ext.feature set */
		jb endCheck			/* No extended info available */
		mov eax, 0xC0000001	/* Centaur extended feature flags */
		cpuid
		mov eax, edx		/* Work with saved copy of feature flags */
		and eax, 01100b
		cmp eax, 01100b		/* Check for RNG present + enabled flags */
		jz noRNG			/* No, RNG not present or enabled */
		or [sysCaps], HWCAP_FLAG_XSTORE	/* Remember that we have a HW RNG */
	noRNG:
		mov eax, edx
		and eax, 011000000b
		cmp eax, 011000000b	/* Check for ACE present + enabled flags */
		jz noACE			/* No, ACE not present or enabled */
		or [sysCaps], HWCAP_FLAG_XCRYPT	/* Remember that we have HW AES */
	noACE:
		mov eax, edx
		and eax, 0110000000000b
		cmp eax, 0110000000000b	/* Check for PHE present + enabled flags */
		jz noPHE			/* No, PHE not present or enabled */
		or [sysCaps], HWCAP_FLAG_XSHA	/* Remember that we have HW SHA-1/SHA-2 */
	noPHE:
		mov eax, edx
		and eax, 011000000000000b
		cmp eax, 011000000000000b /* Check for PMM present + enabled flags */
		jz endCheck			/* No, PMM not present or enabled */
		or [sysCaps], HWCAP_FLAG_MONTMUL	/* Remember that we have HW bignum */
	endCheck:
		}
		}
	if( !memcmp( vendorID, "AuthenticAMD", 12 ) )
		{
		/* Check for AMD Geode LX, family 0x5 = Geode, model 0xA = LX */
		if( ( processorID & 0x0FF0 ) == 0x05A0 )
			sysCaps |= HWCAP_FLAG_TRNG;
		}
	if( !memcmp( vendorID, "GenuineIntel", 12 ) )
		{
		/* Check for hardware AES support */
		if( featureFlags & ( 1 << 25 ) )
			sysCaps |= HWCAP_FLAG_AES;

		/* Check for the return of a hardware RNG */
		if( featureFlags & ( 1 << 30 ) )
			sysCaps |= HWCAP_FLAG_RDRAND;
		}

	return( sysCaps );
	}

#elif defined( __WIN32__ )  && defined( _M_X64 )

/* 64-bit VC++ doesn't allow inline asm, but does provide the __cpuid() 
   builtin to perform the operation above.  We don't guard this with the 
   NO_ASM check because it's not (technically) done with inline asm, 
   although it's a bit unclear whether an intrinsic qualifies as asm or
   C */

#pragma intrinsic( __cpuid )

typedef struct { unsigned int eax, ebx, ecx, edx; } CPUID_INFO;

STDC_NONNULL_ARG( ( 1 ) ) \
static void cpuID( OUT CPUID_INFO *result, const int type )
	{
	int intResult[ 4 ];	/* That's what the function prototype says */

	/* Clear return value */
	memset( result, 0, sizeof( CPUID_INFO ) );

	/* Get the CPUID data and copy it back to the caller.  We clear it 
	   before calling the __cpuid intrinsic because some analysers don't 
	   know about it and will warn about use of uninitialised memory */
	memset( intResult, 0, sizeof( int ) * 4 );
	__cpuid( intResult, type );
	result->eax = intResult[ 0 ];
	result->ebx = intResult[ 1 ];
	result->ecx = intResult[ 2 ];
	result->edx = intResult[ 3 ];
	}

CHECK_RETVAL_ENUM( HWCAP_FLAG ) \
static int getHWInfo( void )
	{
	CPUID_INFO cpuidInfo;
	char vendorID[ 12 + 8 ];
	int *vendorIDptr = ( int * ) vendorID;
	unsigned long processorID, featureFlags;
	int sysCaps = HWCAP_FLAG_RDTSC;	/* x86-64 always has RDTSC */

	/* Get any CPU info that we need.  There is an 
	   IsProcessorFeaturePresent() function, but all that this provides is 
	   an indication of the availability of rdtsc (alongside some stuff that 
	   we don't care about, like MMX and 3DNow).  Since we still need to 
	   check for the presence of other features, we do the whole thing 
	   ourselves */
	cpuID( &cpuidInfo, 0 );
	vendorIDptr[ 0 ] = cpuidInfo.ebx;
	vendorIDptr[ 1 ] = cpuidInfo.edx;
	vendorIDptr[ 2 ] = cpuidInfo.ecx;
	cpuID( &cpuidInfo, 1 );
	processorID = cpuidInfo.eax;
	featureFlags = cpuidInfo.ecx;

	/* Check for vendor-specific special features */
	if( !memcmp( vendorID, "CentaurHauls", 12 ) )
		{
		/* Get the Centaur extended CPUID info and check whether the feature-
		   flags read capability is present.  VIA only announced their 64-
		   bit CPUs in mid-2010 and availability is limited so it's 
		   uncertain whether this code will ever be exercised, but we provide 
		   it anyway for compatibility with the 32-bit equivalent */
		cpuID( &cpuidInfo, 0xC0000000 );
		if( cpuidInfo.eax >= 0xC0000001 )
			{
			/* Get the Centaur extended feature flags */
			cpuID( &cpuidInfo, 0xC0000000 );
			if( ( cpuidInfo.edx & 0x000C ) == 0x000C )
				sysCaps |= HWCAP_FLAG_XSTORE;
			if( ( cpuidInfo.edx & 0x00C0 ) == 0x00C0 )
				sysCaps |= HWCAP_FLAG_XCRYPT;
			if( ( cpuidInfo.edx & 0x0C00 ) == 0x0C00 )
				sysCaps |= HWCAP_FLAG_XSHA;
			if( ( cpuidInfo.edx & 0x3000 ) == 0x3000 )
				sysCaps |= HWCAP_FLAG_MONTMUL;
			}
		}
	if( !memcmp( vendorID, "AuthenticAMD", 12 ) )
		{
		/* Check for AMD Geode LX, family 0x5 = Geode, model 0xA = LX */
		if( ( processorID & 0x0FF0 ) == 0x05A0 )
			sysCaps |= HWCAP_FLAG_TRNG;
		}
	if( !memcmp( vendorID, "GenuineIntel", 12 ) )
		{
		/* Check for hardware AES support */
		if( featureFlags & ( 1 << 25 ) )
			sysCaps |= HWCAP_FLAG_AES;

		/* Check for the return of a hardware RNG */
		if( featureFlags & ( 1 << 30 ) )
			sysCaps |= HWCAP_FLAG_RDRAND;
		}

	return( sysCaps );
	}

#elif defined( __GNUC__ ) && defined( __i386__ ) && !defined( NO_ASM )

#if HWCAP_FLAG_RDTSC != 0x01
  #error Need to sync HWCAP_FLAG_RDTSC with equivalent asm definition
#endif /* HWCAP_FLAG_RDTSC */

CHECK_RETVAL \
static int getHWInfo( void )
	{
	char vendorID[ 12 + 8 ];
	unsigned long processorID, featureFlags;
	int hasAdvFeatures = 0, sysCaps = 0;

	/* Check whether the CPU supports extended features like CPUID and 
	   RDTSC, and get any info that we need related to this.  The use of ebx 
	   is a bit problematic because gcc (via the IA32 ABI) uses ebx to store 
	   the address of the global offset table and gets rather upset if it 
	   gets changed, so we have to save/restore it around the cpuid call.  
	   We have to be particularly careful here because ebx is used 
	   implicitly in references to sysCaps (which is a static int), so we 
	   save it as close to the cpuid instruction as possible and restore it 
	   immediately afterwards, away from any memory-referencing instructions 
	   that implicitly use ebx */
	asm volatile( "pushf\n\t"
		"popl %%eax\n\t"
		"movl %%eax, %%ecx\n\t"
		"xorl $0x200000, %%eax\n\t"
		"pushl %%eax\n\t"
		"popf\n\t"
		"pushf\n\t"
		"popl %%eax\n\t"
		"pushl %%ecx\n\t"
		"popf\n\t"
		"xorl %%ecx, %%eax\n\t"
		"jz noCPUID\n\t"
		"movl $1, %[hasAdvFeatures]\n\t"/* hasAdvFeatures = TRUE */
		"movl %[HW_FLAG_RDTSC], %[sysCaps]\n\t"		/* sysCaps = HWCAP_FLAG_RDTSC */
		"pushl %%ebx\n\t"	/* Save PIC register */
		"xorl %%eax, %%eax\n\t"	/* CPUID function 0: Get vendor ID */
		"cpuid\n\t"
		"leal %2, %%eax\n\t"
		"movl %%ebx, (%%eax)\n\t"
		"movl %%edx, 4(%%eax)\n\t"
		"movl %%ecx, 8(%%eax)\n\t"
		"movl $1, %%eax\n\t"	/* CPUID function 1: Get processor info */
		"cpuid\n\t"
		"leal %3, %%ebx\n\t"
		"movl %%eax, (%%ebx)\n\t"	/* processorID */
		"leal %4, %%ebx\n\t"
		"movl %%ecx, (%%ebx)\n\t"	/* featureFlags */
		"popl %%ebx\n"		/* Restore PIC register */
	"noCPUID:\n\n"
#if 0	/* See comment in tools/ccopts.sh for why this is disabled */
		".section .note.GNU-stack, \"\", @progbits; .previous\n"
							/* Mark the stack as non-executable.  This is
							   undocumented outside of mailing-list postings
							   and a bit hit-and-miss, but having at least
							   one of these in included asm code doesn't
							   hurt */
#endif /* 0 */
		: [hasAdvFeatures] "=m"(hasAdvFeatures),/* Output */
			[sysCaps] "=m"(sysCaps),
			[vendorID] "=m"(vendorID), 
			[processorID] "=m"(processorID),
			[featureFlags] "=m"(featureFlags)
		: [HW_FLAG_RDTSC] "i"(HWCAP_FLAG_RDTSC)/* Input */
		: "%eax", "%ecx", "%edx"				/* Registers clobbered */
		);

	/* If there's no CPUID support, there are no special HW capabilities
	   available */
	if( !hasAdvFeatures )
		return( HWCAP_FLAG_NONE );

	/* If there's a vendor ID present, check for vendor-specific special
	   features.  Again, we have to be extremely careful with ebx */
	if( !memcmp( vendorID, "CentaurHauls", 12 ) )
		{
	asm volatile( "pushl %%ebx\n\t"	/* Save PIC register */
		"movl $0xC0000000, %%eax\n\t"
		"cpuid\n\t"
		"popl %%ebx\n\t"			/* Restore PIC register */
		"cmpl $0xC0000001, %%eax\n\t"
		"jb endCheck\n\t"
		"pushl %%ebx\n\t"			/* Re-save PIC register */
		"movl $0xC0000001, %%eax\n\t"
		"cpuid\n\t"
		"popl %%ebx\n\t"			/* Re-restore PIC register */
		"movl %%edx, %%eax\n\t"
		"andl $0xC, %%edx\n\t"
		"cmpl $0xC, %%edx\n\t"
		"jz noRNG\n\t"
		"orl %[HW_FLAG_XSTORE], %[sysCaps]\n"	/* HWCAP_FLAG_XSTORE */
	"noRNG:\n\t"
		"movl %%edx, %%eax\n\t"
		"andl $0xC0, %%eax\n\t"
		"cmpl $0xC0, %%eax\n\t"
		"jz noACE\n\t"
		"orl %[HW_FLAG_XCRYPT], %[sysCaps]\n"	/* HWCAP_FLAG_XCRYPT */
	"noACE:\n\t"
		"movl %%edx, %%eax\n\t"
		"andl $0xC00, %%eax\n\t"
		"cmpl $0xC00, %%eax\n\t"
		"jz noPHE\n\t"
		"orl %[HW_FLAG_XSHA], %[sysCaps]\n"		/* HWCAP_FLAG_XSHA */
	"noPHE:\n\t"
		"movl %%edx, %%eax\n\t"
		"andl $0x3000, %%eax\n\t"
		"cmpl $0x3000, %%eax\n\t"
		"jz endCheck\n\t"
		"orl %[HW_FLAG_MONTMUL], %[sysCaps]\n"	/* HWCAP_FLAG_MONTMUL */
	"endCheck:\n\n"
		 : [sysCaps] "=m"(sysCaps)	/* Output */
		 : [HW_FLAG_XSTORE] "i"(HWCAP_FLAG_XSTORE),/* Input */
			[HW_FLAG_XCRYPT] "i"(HWCAP_FLAG_XCRYPT),
			[HW_FLAG_XSHA] "i"(HWCAP_FLAG_XSHA),
			[HW_FLAG_MONTMUL] "i"(HWCAP_FLAG_MONTMUL)
		 : "%eax", "%ecx", "%edx"	/* Registers clobbered */
		);
		}
	if( !memcmp( vendorID, "AuthenticAMD", 12 ) )
		{
		/* Check for AMD Geode LX, family 0x5 = Geode, model 0xA = LX */
		if( ( processorID & 0x0FF0 ) == 0x05A0 )
			sysCaps |= HWCAP_FLAG_TRNG;
		}
	if( !memcmp( vendorID, "GenuineIntel", 12 ) )
		{
		/* Check for hardware AES support */
		if( featureFlags & ( 1 << 25 ) )
			sysCaps |= HWCAP_FLAG_AES;

		/* Check for the return of a hardware RNG */
		if( featureFlags & ( 1 << 30 ) )
			sysCaps |= HWCAP_FLAG_RDRAND;
		}

	return( sysCaps );
	}

#elif defined( __GNUC__ ) && ( defined( __arm ) || defined( __arm__ ) ) && \
	  !defined( NO_ASM ) && 0		/* See comment below */

CHECK_RETVAL \
static int getHWInfo( void )
	{
	int processorID;

	/* Get the ARM CPU type information.  Unfortunately this instruction 
	   (and indeed virtually all of the very useful CP15 registers) are 
	   inaccessible from user mode so it's not safe to perform any of these 
	   operations.  If you're running an embedded OS that runs natively in 
	   supervisor mode then you can try enabling this function to check 
	   whether you have access to the other CP15 registers and their 
	   information about hardware capabilities */
	asm volatile (
		"mrc p15, 0, r0, c0, c0, 0\n\t"
		"str r0, %0\n"
		: "=m"(processorID)
		:
		: "cc", "r0");

	return( HWCAP_FLAG_NONE );
	}
#else

CHECK_RETVAL \
static int getHWInfo( void )
	{
	return( HWCAP_FLAG_NONE );
	}
#endif /* OS-specific support */

/* Initialise OS-specific constants.  This is a bit ugly because the values 
   are often specific to one cryptlib module but there's no (clean) way to
   perform any complex per-module initialisation so we have to know about 
   all of the module-specific sysVar requirements here */

#define MAX_SYSVARS		8

static int sysVars[ MAX_SYSVARS ];

#if ( defined( __WIN32__ ) || defined( __WINCE__ ) )

CHECK_RETVAL \
int initSysVars( void )
	{
#if VC_LT_2010( _MSC_VER )
	OSVERSIONINFO osvi = { sizeof(OSVERSIONINFO) };
#endif /* VC++ < 2010 */
	SYSTEM_INFO systemInfo;

	static_assert( SYSVAR_LAST < MAX_SYSVARS, "System variable value" );

	/* Reset the system variable information */
	memset( sysVars, 0, sizeof( int ) * MAX_SYSVARS );

#if VC_LT_2010( _MSC_VER )
	/* Figure out which version of Windows we're running under */
	if( !GetVersionEx( &osvi ) )
		{
		/* If for any reason the call fails, just use the most likely 
		   values */
		osvi.dwMajorVersion = 5;	/* Win2K and higher */
		osvi.dwPlatformId = VER_PLATFORM_WIN32_NT;
		}
	sysVars[ SYSVAR_OSMAJOR ] = osvi.dwMajorVersion;
	sysVars[ SYSVAR_OSMINOR ] = osvi.dwMinorVersion;

	/* Check for Win32s and Win95/98/ME just in case someone ever digs up 
	   one of these systems and tries to load cryptlib under them */
	if( osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS || \
		osvi.dwPlatformId == VER_PLATFORM_WIN32s )
		{
		DEBUG_DIAG(( "Win32s detected" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_NOTAVAIL );
		}
#endif /* VC++ < 2010 */

	/* Get the system page size */
	GetSystemInfo( &systemInfo );
	sysVars[ SYSVAR_PAGESIZE ] = systemInfo.dwPageSize;

	/* Get system hardware capabilities */
	sysVars[ SYSVAR_HWCAP ] = getHWInfo();

	return( CRYPT_OK );
	}

#elif defined( __UNIX__ )

#include <unistd.h>

CHECK_RETVAL \
int initSysVars( void )
	{
	static_assert( SYSVAR_LAST < MAX_SYSVARS, "System variable value" );

	/* Reset the system variable information */
	memset( sysVars, 0, sizeof( int ) * MAX_SYSVARS );

	/* Get the system page size */
#if defined( _CRAY ) || defined( __hpux ) || defined( _M_XENIX ) || \
	defined( __aux )
  #if defined( _SC_PAGESIZE )
	sysVars[ SYSVAR_PAGESIZE ] = sysconf( _SC_PAGESIZE );
  #elif defined( _SC_PAGE_SIZE )
	sysVars[ SYSVAR_PAGESIZE ] = sysconf( _SC_PAGE_SIZE );
  #else
	sysVars[ SYSVAR_PAGESIZE ] = 4096;	/* Close enough for most systems */
  #endif /* Systems without getpagesize() */
#else
	sysVars[ SYSVAR_PAGESIZE ] = getpagesize();
#endif /* Unix variant-specific brokenness */
	if( sysVars[ SYSVAR_PAGESIZE ] < 1024 )
		{
		DEBUG_DIAG(( "System reports page size < 1024" ));
		assert( DEBUG_WARN );

		/* Suspiciously small reported page size, just assume a sensible 
		   value */
		sysVars[ SYSVAR_PAGESIZE ] = 4096;
		}

	/* Get system hardware capabilities */
	sysVars[ SYSVAR_HWCAP ] = getHWInfo();

#if defined( __IBMC__ ) || defined( __IBMCPP__ )
	/* VisualAge C++ doesn't set the TZ correctly */
	tzset();
#endif /* VisualAge C++ */

	return( CRYPT_OK );
	}

#else

CHECK_RETVAL \
int initSysVars( void )
	{
	/* Reset the system variable information */
	memset( sysVars, 0, sizeof( int ) * MAX_SYSVARS );

	/* Get system hardware capabilities */
	sysVars[ SYSVAR_HWCAP ] = getHWInfo();

	return( CRYPT_OK );
	}
#endif /* OS-specific support */

CHECK_RETVAL \
int getSysVar( IN_ENUM( SYSVAR ) const SYSVAR_TYPE type )
	{
	REQUIRES( type > SYSVAR_NONE && type < SYSVAR_LAST );

	return( sysVars[ type ] );
	}

/****************************************************************************
*																			*
*				Miscellaneous System-specific Support Functions				*
*																			*
****************************************************************************/

/* Align a pointer to a given boundary.  This gets quite complicated because
   the only pointer arithmetic that's normally allowed is addition and 
   subtraction, but to align to a boundary we need to be able to perform 
   bitwise operations.  First we convert the pointer to a char pointer so
   that we can perform normal maths on it, and then we round in the usual
   manner used by roundUp().  Because we have to do pointer-casting we can't 
   use roundUp() directly but have to build our own version here */

#if defined( __WIN32__ ) || defined( __WIN64__ ) 
  #define intptr_t	INT_PTR
#elif defined( __ECOS__ )
  #define intptr_t	unsigned int
#elif defined( __GNUC__ ) && ( __GNUC__ >= 3 )
  #if defined( __VxWorks__ ) && \
	  ( ( _WRS_VXWORKS_MAJOR < 6 ) || \
		( _WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 6 ) )
	/* Older versions of Wind River's toolchain don't include stdint.h,
	   it was added somewhere around VxWorks 6.6 */
	#define intptr_t	int
  #else
	#include <stdint.h>
  #endif /* Nonstandard gcc environments */
#elif defined( SYSTEM_64BIT )
  #define intptr_t	long long
#else
  #define intptr_t	int
#endif /* OS-specific pointer <-> int-type equivalents */

void *ptr_align( const void *ptr, const int units )
	{
	assert( isReadPtr( ptr, 1 ) );
	assert( units > 0 && units < MAX_INTLENGTH_SHORT );

	return( ( void * ) ( ( char * ) ptr + ( -( ( intptr_t )( ptr ) ) & ( units - 1 ) ) ) );
	}

/* Determine the difference between two pointers, with some sanity 
   checking.  This assumes the pointers are fairly close in location,
   used to determine whether pointers that were potentially relocated 
   at some point via ptr_align() have moved */

int ptr_diff( const void *ptr1, const void *ptr2 )
	{
	ptrdiff_t diff;

	assert( isReadPtr( ptr1, 1 ) );
	assert( isReadPtr( ptr2, 1 ) );
	assert( ptr1 >= ptr2 );

	diff = ( const BYTE * ) ptr1 - ( const BYTE * ) ptr2;
	if( diff < 0 )
		diff = -diff;
	if( diff > MAX_INTLENGTH )
		return( -1 );

	return( ( int ) diff );
	}
