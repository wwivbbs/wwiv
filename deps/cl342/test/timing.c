/****************************************************************************
*																			*
*						  cryptlib Timing Test Routines						*
*						Copyright Peter Gutmann 2003-2008					*
*																			*
****************************************************************************/

/* In order for the DH/DSA timing tests (which use cryptlib-internal
   mechanisms) to work, it's necessary to add the following lines of code to
   the start of cryptDeviceQueryCapability() in cryptapi.c, before any other
   code:

	if( cryptAlgo == 1000 )
		return( krnlSendMessage( device, IMESSAGE_SETATTRIBUTE_S,
								 cryptQueryInfo, CRYPT_IATTRIBUTE_KEY_SPKI ) );
	if( cryptAlgo == 1001 )
		return( krnlSendMessage( device, IMESSAGE_CTX_ENCRYPT,
								 cryptQueryInfo, sizeof( KEYAGREE_PARAMS ) ) );
	if( cryptAlgo == 1002 )
		return( krnlSendMessage( device, IMESSAGE_CTX_DECRYPT,
								 cryptQueryInfo, sizeof( KEYAGREE_PARAMS ) ) );
	if( cryptAlgo == 1003 )
		return( krnlSendMessage( device, IMESSAGE_CTX_SIGN,
								 cryptQueryInfo, sizeof( DLP_PARAMS ) ) );
	if( cryptAlgo == 1004 )
		return( krnlSendMessage( device, IMESSAGE_CTX_SIGCHECK,
								 cryptQueryInfo, sizeof( DLP_PARAMS ) ) );

   This is because the DH/DSA tests are handled by passing magic values into
   the function (which is very rarely used normally).  Remember to remove
   the magic-values check code after use!

   Under Unix, the test code can be built with:

	cc -c -D__UNIX__ testtime.c
	cc -o testtime -lpthread -lresolv testtime.o -L. -lcl

   */

#if defined( _WIN32_WCE ) && _WIN32_WCE < 400
  #define assert( x )
#else
  #include <assert.h>
#endif /* Systems without assert() */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>	/* For sqrt() for standard deviation */
#ifdef _MSC_VER
  #include "../cryptlib.h"
#else
  #include "cryptlib.h"
#endif /* Braindamaged VC++ include handling */
#ifdef __UNIX__
  #include <sys/time.h>
#endif /* __UNIX__ */

/* It's useful to know if we're running under Windows to enable Windows-
   specific processing */

#if defined( _WINDOWS ) || defined( WIN32 ) || defined( _WIN32 ) || \
	defined( _WIN32_WCE )
  #define __WINDOWS__
#endif /* _WINDOWS || WIN32 || _WIN32 || _WIN32_WCE */

/* Various useful types */

#define BOOLEAN	int
#define BYTE	unsigned char
#ifndef TRUE
  #define FALSE	0
  #define TRUE	!FALSE
#endif /* TRUE */

/* Helper function for systems with no console */

#ifdef _WIN32_WCE
  #define printf	wcPrintf
  #define puts		wcPuts

  void wcPrintf( const char *format, ... );
  void wcPuts( const char *string );
#endif /* Console-less environments */

/* Since high-precision timing is rather OS-dependent, we only enable this
   under Windows where we've got guaranteed high-res timer access */

#if defined( __WINDOWS__ ) || defined( __UNIX__ )

/* The number of tests of each algorithm to perform */

#define NO_TESTS	25

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Helper functions for systems with no console.  These just use the debug
   console as stdout */

#ifdef _WIN32_WCE

void wcPrintf( const char *format, ... )
	{
	wchar_t wcBuffer[ 1024 ];
	char buffer[ 1024 ];
	va_list argPtr;

	va_start( argPtr, format );
	vsprintf( buffer, format, argPtr );
	va_end( argPtr );
	mbstowcs( wcBuffer, buffer, strlen( buffer ) + 1 );
	NKDbgPrintfW( wcBuffer );
	}

void wcPuts( const char *string )
	{
	wcPrintf( "%s\n", string );
	}
#endif /* Console-less environments */

/* Check for an algorithm/mode */

static BOOLEAN checkLowlevelInfo( const CRYPT_DEVICE cryptDevice,
								  const CRYPT_ALGO_TYPE cryptAlgo,
								  const CRYPT_MODE_TYPE cryptMode )
	{
	CRYPT_QUERY_INFO cryptQueryInfo;
	const BOOLEAN isDevice = ( cryptDevice != CRYPT_UNUSED ) ? TRUE : FALSE;
	int status;

	if( isDevice )
		status = cryptDeviceQueryCapability( cryptDevice, cryptAlgo,
											 &cryptQueryInfo );
	else
		status = cryptQueryCapability( cryptAlgo, &cryptQueryInfo );
	if( cryptStatusError( status ) )
		{
		/* RC4 is a legacy algorithm that isn't usually enabled so we 
		   silently skip this one */
		if( cryptAlgo == CRYPT_ALGO_RC4 )
			return( FALSE );

		printf( "crypt%sQueryCapability() reports algorithm %d is not "
				"available, status = %d.\n", isDevice ? "Device" : "",
				cryptAlgo, status );
		return( FALSE );
		}
	if( cryptAlgo > CRYPT_ALGO_FIRST_HASH )
		{
		printf( "%s, ", cryptQueryInfo.algoName );
		return( TRUE );
		}
	printf( "%s/%s, block size %d bits, keysize %d bits, ", 
			cryptQueryInfo.algoName, 
			( cryptMode == CRYPT_MODE_ECB ) ? "ECB" : "CBC",
			cryptQueryInfo.blockSize << 3, cryptQueryInfo.keySize << 3 );

	return( TRUE );
	}

/* Load the encryption contexts */

static BOOLEAN loadContexts( CRYPT_CONTEXT *cryptContext, CRYPT_CONTEXT *decryptContext,
							 const CRYPT_DEVICE cryptDevice,
							 const CRYPT_ALGO_TYPE cryptAlgo,
							 const CRYPT_MODE_TYPE cryptMode,
							 const BYTE *key, const int length )
	{
	const BOOLEAN isDevice = ( cryptDevice != CRYPT_UNUSED ) ? TRUE : FALSE;
	const BOOLEAN hasKey = ( cryptAlgo >= CRYPT_ALGO_FIRST_CONVENTIONAL && \
							 cryptAlgo <= CRYPT_ALGO_LAST_CONVENTIONAL ) || \
						   ( cryptAlgo >= CRYPT_ALGO_FIRST_MAC && \
							 cryptAlgo <= CRYPT_ALGO_LAST_MAC );
	BOOLEAN adjustKey = FALSE;
	int status;

	/* Create the encryption context */
	if( isDevice )
		status = cryptDeviceCreateContext( cryptDevice, cryptContext,
										   cryptAlgo );
	else
		status = cryptCreateContext( cryptContext, CRYPT_UNUSED, cryptAlgo );
	if( cryptStatusError( status ) )
		{
		printf( "crypt%sCreateContext() failed with error code %d.\n",
				isDevice ? "Device" : "", status );
		return( FALSE );
		}
	if( cryptAlgo <= CRYPT_ALGO_LAST_CONVENTIONAL )
		{
		status = cryptSetAttribute( *cryptContext, CRYPT_CTXINFO_MODE,
									cryptMode );
		if( cryptStatusError( status ) )
			{
			cryptDestroyContext( *cryptContext );
			if( status == CRYPT_ERROR_NOTAVAIL )
				/* This mode isn't available, return a special-case value to
				   tell the calling code to continue */
				return( status );
			printf( "Encryption mode %d selection failed with status %d.\n",
					cryptMode, status );
			return( FALSE );
			}
		}
	if( hasKey )
		{
		status = cryptSetAttributeString( *cryptContext, CRYPT_CTXINFO_KEY,
										  key, length );
		if( length > 16 && status == CRYPT_ERROR_PARAM4 )
			{
			status = cryptSetAttributeString( *cryptContext, CRYPT_CTXINFO_KEY,
											  key, 16 );
			if( cryptStatusOK( status ) )
				{
				puts( "  Load of full-length key failed, using shorter 128-"
					  "bit key." );
				adjustKey = TRUE;
				}
			}
		if( cryptStatusError( status ) )
			{
			printf( "Key load failed with error code %d.\n", status );
			return( FALSE );
			}
		}
	if( decryptContext == NULL )
		return( TRUE );

	/* Create the decryption context */
	if( cryptDevice == CRYPT_UNUSED )
		status = cryptCreateContext( decryptContext, CRYPT_UNUSED, cryptAlgo );
	else
		status = cryptDeviceCreateContext( cryptDevice, decryptContext,
										   cryptAlgo );
	if( cryptStatusError( status ) )
		{
		printf( "crypt%sCreateContext() failed with error code %d.\n",
				( cryptDevice != CRYPT_UNUSED ) ? "Device" : "", status );
		return( FALSE );
		}
	if( cryptAlgo <= CRYPT_ALGO_LAST_CONVENTIONAL )
		{
		status = cryptSetAttribute( *decryptContext, CRYPT_CTXINFO_MODE,
									cryptMode );
		if( cryptStatusError( status ) )
			{
			cryptDestroyContext( *cryptContext );
			if( status == CRYPT_ERROR_NOTAVAIL )
				/* This mode isn't available, return a special-case value to
				   tell the calling code to continue */
				return( status );
			printf( "Encryption mode %d selection failed with status %d.\n",
					cryptMode, status );
			return( FALSE );
			}
		}
	if( hasKey )
		{
		status = cryptSetAttributeString( *decryptContext, CRYPT_CTXINFO_KEY,
										  key, adjustKey ? 16 : length );
		if( cryptStatusError( status ) )
			{
			printf( "Key load failed with error code %d.\n", status );
			return( FALSE );
			}
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*									Timing Tests							*
*																			*
****************************************************************************/

/* Print timing info.  This gets a bit hairy because we're actually counting 
   timer ticks rather than thread times which means that we'll be affected 
   by things like context switches.  There are two approaches to this:

	1. Take the fastest time, which will be the time least affected by 
	   system overhead.

	2. Apply standard statistical techniques to weed out anomalies.  Since 
	   this is just for testing purposes all we do is discard any results 
	   out by more than 10%, which is crude but reasonably effective.  A 
	   more rigorous approach is to discards results more than n standard 
	   deviations out, but this gets screwed up by the fact that a single 
	   context switch of 20K ticks can throw out results from an execution 
	   time of only 50 ticks.  In any case (modulo context switches) the 
	   fastest, 10%-out, and 2 SD out times are all within about 1% of each 
	   other so all methods are roughly equally accurate */

static void printTimes( HIRES_TIME times[ NO_TESTS + 1 ][ 8 ],
						const int noTimes, long ticksPerSec )
	{
	long pubTimeMS, privTimeMS, throughput;
	int i;

	for( i = 0; i < noTimes; i++ )
		{
		HIRES_TIME timeSum = 0, timeAvg, timeDelta;
		HIRES_TIME timeMin = 1000000L;
		HIRES_TIME timeCorrSum10 = 0;
		HIRES_TIME avgTime;
#ifdef USE_SD
		HIRES_TIME timeCorrSumSD = 0;
		double stdDev;
		int timesCountSD = 0;
#endif /* USE_SD */
		int j, timesCount10 = 0;

		/* Find the mean execution time */
		for( j = 1; j < NO_TESTS + 1; j++ )
			timeSum += times[ j ][ i ];
		timeAvg = timeSum / NO_TESTS;
		timeDelta = timeSum / 10;	/* 10% variation */
		if( timeSum == 0 )
			{
			/* Some ciphers can't provide results for some cases (e.g.
			   AES for 8-byte blocks) */
			printf( "       " );
			continue;
			}

		/* Find the fastest overall time */
		for( j = 1; j < NO_TESTS + 1; j++ )
			{
			if( times[ j ][ i ] < timeMin )
				timeMin = times[ j ][ i ];
			}

		/* Find the mean time, discarding anomalous results more than 10%
		   out.  We cast the values to longs in order to (portably) print
		   them, if we want to print the full 64-bit values we have to
		   use nonstandard extensions like "%I64d" (for Win32) */
		for( j = 1; j < NO_TESTS + 1; j++ )
			{
			if( times[ j ][ i ] > timeAvg - timeDelta && \
				times[ j ][ i ] < timeAvg + timeDelta )
				{
				timeCorrSum10 += times[ j ][ i ];
				timesCount10++;
				}
			}
		if( timesCount10 <= 0 )
			{
			printf( "Error: No times within +/-%d of %d.\n",
					timeDelta, timeAvg );
			continue;
			}
		avgTime = timeCorrSum10 / timesCount10;
		if( noTimes <= 2 && i == 0 )
			{
			printf( "Pub: min.= %d, avg.= %d ", ( long ) timeMin,
					( long ) avgTime );
			pubTimeMS = ( avgTime * 1000 ) / ticksPerSec;
			}
		else
		if( noTimes <= 2 && i == 1 )
			{
			printf( "Priv: min.= %d, avg.= %d ", ( long ) timeMin,
					( long ) avgTime );
			privTimeMS = ( avgTime * 1000 ) / ticksPerSec;
			}
		else
			{
			printf( "%7d", ( long ) avgTime );
			if( i == 6 )
				{
				const long mbTime = ( long ) avgTime * 16;	/* 64kB * 15 = 1MB */

				if( mbTime > ticksPerSec )
					throughput = 0;
				else
					throughput = ticksPerSec / mbTime;	/* In MB/sec */
				}
			}
#if 0	/* Print difference to fastest time, usually only around 1% */
		printf( "(%4d)", ( timeCorrSum10 / timesCount10 ) - timeMin );
#endif /* 0 */

#ifdef USE_SD
		/* Find the standard deviation */
		for( j = 1; j < NO_TESTS + 1; j++ )
			{
			const HIRES_TIME timeDev = times[ j ][ i ] - timeAvg;

			timeCorrSumSD += ( timeDev * timeDev );
			}
		stdDev = timeCorrSumSD / NO_TESTS;
		stdDev = sqrt( stdDev );

		/* Find the mean time, discarding anomalous results more than two
		   standard deviations out */
		timeCorrSumSD = 0;
		timeDelta = ( HIRES_TIME ) stdDev * 2;
		for( j = 1; j < NO_TESTS + 1; j++ )
			{
			if( times[ j ][ i ] > timeAvg - timeDelta && \
				times[ j ][ i ] < timeAvg + timeDelta )
				{
				timeCorrSumSD += times[ j ][ i ];
				timesCountSD++;
				}
			}
		if( timesCountSD == 0 )
			timesCountSD++;	/* Context switch, fudge it */
		printf( "%6d", timeCorrSumSD / timesCountSD );

#if 1	/* Print difference to fastest and mean times, usually only around
		   1% */
		printf( " (dF = %4d, dM = %4d)\n",
				( timeCorrSumSD / timesCountSD ) - timeMin,
				abs( ( timeCorrSumSD / timesCountSD ) - \
					 ( timeCorrSum10 / timesCount10 ) ) );
#endif /* 0 */
#endif /* USE_SD */
		}
	/* If it's a PKC operation print the times in ms */
	if( noTimes <= 2 )
		{
		printf( "\n  Public-key op.time = " );
		if( pubTimeMS <= 0 )
			printf( "< 1" );
		else
			printf( "%ld", pubTimeMS );
		printf( " ms, private-key op.time = %ld ms.\n", privTimeMS );
		}
	else
		{
		if( throughput <= 0 )
			puts( ", throughput < 1 MB/s." );
		else
			printf( ", throughput %ld MB/s.\n", throughput );
		}
	}

static HIRES_TIME encOne( const CRYPT_CONTEXT cryptContext,
						  BYTE *buffer, const int length )
	{
	HIRES_TIME timeVal;

	memset( buffer, '*', length );
	timeVal = timeDiff( 0 );
	cryptEncrypt( cryptContext, buffer, length );
	return( timeDiff( timeVal ) );
	}

static void encTest( const CRYPT_CONTEXT cryptContext,
					 const CRYPT_ALGO_TYPE cryptAlgo, BYTE *buffer,
					 HIRES_TIME times[] )
	{
	int index = 0;

	times[ index++ ] = ( cryptAlgo != CRYPT_ALGO_AES ) ? \
					   encOne( cryptContext, buffer, 8 ) : 0;
	times[ index++ ] = encOne( cryptContext, buffer, 16 );
	times[ index++ ] = encOne( cryptContext, buffer, 64 );
	times[ index++ ] = encOne( cryptContext, buffer, 1024 );
	times[ index++ ] = encOne( cryptContext, buffer, 4096 );
	times[ index++ ] = encOne( cryptContext, buffer, 8192 );
	times[ index++ ] = encOne( cryptContext, buffer, 65536L );
	}

static int encTests( const CRYPT_DEVICE cryptDevice,
					 const CRYPT_ALGO_TYPE cryptAlgo,
					 const CRYPT_ALGO_TYPE cryptMode,
					 BYTE *buffer, long ticksPerSec )
	{
	CRYPT_CONTEXT cryptContext;
	HIRES_TIME times[ NO_TESTS + 1 ][ 8 ], timeVal, timeSum = 0;
	int i, status;

	memset( buffer, 0, 100000L );

	/* Set up the context for use */
	if( !checkLowlevelInfo( cryptDevice, cryptAlgo, cryptMode ) )
		return( FALSE );
	for( i = 0; i < 10; i++ )
		{
		timeVal = timeDiff( 0 );
		status = loadContexts( &cryptContext, NULL, cryptDevice,
							   cryptAlgo, cryptMode,
							   ( BYTE * ) "12345678901234567890",
							   ( cryptAlgo == CRYPT_ALGO_DES ) ? 8 : \
							   ( cryptAlgo == CRYPT_ALGO_3DES || \
							     cryptAlgo == CRYPT_ALGO_RC4 || \
								 cryptAlgo == CRYPT_ALGO_AES ) ? 16 : 0 );
		timeVal = timeDiff( timeVal );
		if( status == CRYPT_ERROR_NOTAVAIL || !status )
			return( FALSE );
		timeSum += timeVal;
		if( i < 9 )
			cryptDestroyContext( cryptContext );
		}
	printf( "setup time = %d ticks.\n", timeSum / 10 );
	puts( "      8     16     64     1K     4K     8K    64K" );
	puts( "  -----  -----  -----  -----  -----  -----  -----" );

	/* Run the encryption tests NO_TESTS times, discarding the first set
	   of results since the cache will be empty at that point */
	for( i = 0; i < NO_TESTS + 1; i++ )
		encTest( cryptContext, cryptAlgo, buffer, times[ i ] );
	printTimes( times, 7, ticksPerSec );

	/* Re-run the encryption tests with a 1-byte misalignment */
	for( i = 0; i < NO_TESTS + 1; i++ )
		encTest( cryptContext, cryptAlgo, buffer + 1, times[ i ] );
	printTimes( times, 7, ticksPerSec );

	/* Re-run the encryption tests with a 4-byte misalignment */
	for( i = 0; i < NO_TESTS + 1; i++ )
		encTest( cryptContext, cryptAlgo, buffer + 4, times[ i ] );
	printTimes( times, 7, ticksPerSec );

	/* Re-run the test 1000 times with various buffer alignments */
	timeVal = 0;
	for( i = 0; i < 1000; i++ )
		timeVal += encOne( cryptContext, buffer, 1024 );
	printf( "Aligned: %d, ", timeVal / 1000 );
	timeVal = 0;
	for( i = 0; i < 1000; i++ )
		timeVal += encOne( cryptContext, buffer + 1, 1024 );
	printf( "misaligned + 1: %d, ", timeVal / 1000 );
	timeVal = 0;
	for( i = 0; i < 1000; i++ )
		timeVal += encOne( cryptContext, buffer + 4, 1024 );
	printf( "misaligned + 4: %d.\n", timeVal / 1000 );

	return( TRUE );
	}

static void performanceTests( const CRYPT_DEVICE cryptDevice, 
							  long ticksPerSec )
	{
	BYTE *buffer;

	if( ( buffer = malloc( 100000L ) ) == NULL )
		{
		puts( "Couldn't 100K allocate test buffer." );
		return;
		}
	putchar( '\n' );
	encTests( cryptDevice, CRYPT_ALGO_DES, CRYPT_MODE_ECB, buffer, ticksPerSec );
	encTests( cryptDevice, CRYPT_ALGO_DES, CRYPT_MODE_CBC, buffer, ticksPerSec );
	putchar( '\n' );
	encTests( cryptDevice, CRYPT_ALGO_3DES, CRYPT_MODE_ECB, buffer, ticksPerSec );
	encTests( cryptDevice, CRYPT_ALGO_3DES, CRYPT_MODE_CBC, buffer, ticksPerSec );
	putchar( '\n' );
	encTests( cryptDevice, CRYPT_ALGO_AES, CRYPT_MODE_CBC, buffer, ticksPerSec );
	putchar( '\n' );
	encTests( cryptDevice, CRYPT_ALGO_MD5, CRYPT_MODE_NONE, buffer, ticksPerSec );
	encTests( cryptDevice, CRYPT_ALGO_SHA, CRYPT_MODE_NONE, buffer, ticksPerSec );
	free( buffer );
	}

/****************************************************************************
*																			*
*								PKC Timing Tests							*
*																			*
****************************************************************************/

/* Timing tests for PKC crypto operations.  Since the DH operations aren't
   visible externally, we have to use the kernel API to perform the timing
   test.  To get the necessary definitions and prototypes, we have to use
   crypt.h, however since we've already included cryptlib.h the built-in
   guards preclude us from pulling it in again with the internal-only values
   defined, so we have to explicitly define things like attribute values
   that normally aren't visible externally */

#undef __WINDOWS__
#undef __WIN16__
#undef __WIN32__
#undef BYTE
#undef BOOLEAN
#ifdef _MSC_VER
  #include "../crypt.h"
#else
  #include "crypt.h"
#endif /* Braindamaged VC++ include handling */

#define CRYPT_IATTRIBUTE_KEY_SPKI	8015

static const BYTE dh1024SPKI[] = {
	0x30, 0x82, 0x01, 0x21,
		0x30, 0x82, 0x01, 0x17,
			0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3E, 0x02, 0x01,
			0x30, 0x82, 0x01, 0x0A,
				0x02, 0x81, 0x81, 0x00,		/* p */
					0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
					0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
					0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
					0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
					0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
					0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
					0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
					0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
					0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
					0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
					0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
					0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
					0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
					0x49, 0x28, 0x66, 0x51, 0xEC, 0xE6, 0x53, 0x81,
					0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				0x02, 0x01,					/* g */
					0x02,
				0x02, 0x81, 0x80,			/* q */
					0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					0xE4, 0x87, 0xED, 0x51, 0x10, 0xB4, 0x61, 0x1A,
					0x62, 0x63, 0x31, 0x45, 0xC0, 0x6E, 0x0E, 0x68,
					0x94, 0x81, 0x27, 0x04, 0x45, 0x33, 0xE6, 0x3A,
					0x01, 0x05, 0xDF, 0x53, 0x1D, 0x89, 0xCD, 0x91,
					0x28, 0xA5, 0x04, 0x3C, 0xC7, 0x1A, 0x02, 0x6E,
					0xF7, 0xCA, 0x8C, 0xD9, 0xE6, 0x9D, 0x21, 0x8D,
					0x98, 0x15, 0x85, 0x36, 0xF9, 0x2F, 0x8A, 0x1B,
					0xA7, 0xF0, 0x9A, 0xB6, 0xB6, 0xA8, 0xE1, 0x22,
					0xF2, 0x42, 0xDA, 0xBB, 0x31, 0x2F, 0x3F, 0x63,
					0x7A, 0x26, 0x21, 0x74, 0xD3, 0x1B, 0xF6, 0xB5,
					0x85, 0xFF, 0xAE, 0x5B, 0x7A, 0x03, 0x5B, 0xF6,
					0xF7, 0x1C, 0x35, 0xFD, 0xAD, 0x44, 0xCF, 0xD2,
					0xD7, 0x4F, 0x92, 0x08, 0xBE, 0x25, 0x8F, 0xF3,
					0x24, 0x94, 0x33, 0x28, 0xF6, 0x73, 0x29, 0xC0,
					0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x03, 0x04, 0x00,
			0x02, 0x01, 0x00 };				/* y */

static BOOLEAN loadDHKey( CRYPT_CONTEXT *cryptContext )
	{
	int status;

	/* Create the DH context */
	status = cryptCreateContext( cryptContext, CRYPT_UNUSED, CRYPT_ALGO_DH );
	if( cryptStatusError( status ) )
		{
		printf( "cryptCreateContext() failed with error code %d.\n",
				status );
		return( FALSE );
		}
	cryptSetAttributeString( *cryptContext, CRYPT_CTXINFO_LABEL,
							 "DH key", strlen( "DH key" ) );
	if( cryptStatusOK( status ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, ( void * ) dh1024SPKI,
						sizeof( dh1024SPKI ) );
		status = cryptDeviceQueryCapability( *cryptContext, 1000,
									( CRYPT_QUERY_INFO * ) &msgData );
		}
	if( cryptStatusError( status ) )
		{
		printf( "DH key load failed, status = %d, line %d.\n", status,
				__LINE__ );
		return( FALSE );
		}
	return( TRUE );
	}

typedef struct {
	const int nLen; const BYTE n[ 128 ];
	const int eLen; const BYTE e[ 3 ];
	const int dLen; const BYTE d[ 128 ];
	const int pLen; const BYTE p[ 64 ];
	const int qLen; const BYTE q[ 64 ];
	const int uLen; const BYTE u[ 64 ];
	const int e1Len; const BYTE e1[ 64 ];
	const int e2Len; const BYTE e2[ 64 ];
	} RSA_KEY;

static const RSA_KEY rsa1024Key = {
	/* n */
	1024,
	{ 0x9C, 0x4D, 0x98, 0x18, 0x67, 0xF9, 0x45, 0xBC,
	  0xB6, 0x75, 0x53, 0x5D, 0x2C, 0xFA, 0x55, 0xE4,
	  0x51, 0x54, 0x9F, 0x0C, 0x16, 0xB1, 0xAF, 0x89,
	  0xF6, 0xF3, 0xE7, 0x78, 0xB1, 0x2B, 0x07, 0xFB,
	  0xDC, 0xDE, 0x64, 0x23, 0x34, 0x87, 0xDA, 0x0B,
	  0xE5, 0xB3, 0x17, 0x16, 0xA4, 0xE3, 0x7F, 0x23,
	  0xDF, 0x96, 0x16, 0x28, 0xA6, 0xD2, 0xF0, 0x0A,
	  0x59, 0xEE, 0x06, 0xB3, 0x76, 0x6C, 0x64, 0x19,
	  0xD9, 0x76, 0x41, 0x25, 0x66, 0xD1, 0x93, 0x51,
	  0x52, 0x06, 0x6B, 0x71, 0x50, 0x0E, 0xAB, 0x30,
	  0xA5, 0xC8, 0x41, 0xFC, 0x30, 0xBC, 0x32, 0xD7,
	  0x4B, 0x22, 0xF2, 0x45, 0x4C, 0x94, 0x68, 0xF1,
	  0x92, 0x8A, 0x4C, 0xF9, 0xD4, 0x5E, 0x87, 0x92,
	  0xA8, 0x54, 0x93, 0x92, 0x94, 0x48, 0xA4, 0xA3,
	  0xEE, 0x19, 0x7F, 0x6E, 0xD3, 0x14, 0xB1, 0x48,
	  0xCE, 0x93, 0xD1, 0xEA, 0x4C, 0xE1, 0x9D, 0xEF },

	/* e */
	17,
	{ 0x01, 0x00, 0x01 },

	/* d */
	1022,
	{ 0x37, 0xE2, 0x66, 0x67, 0x13, 0x85, 0xC4, 0xB1,
	  0x5C, 0x6B, 0x46, 0x8B, 0x21, 0xF1, 0xBF, 0x94,
	  0x0A, 0xA0, 0x3E, 0xDD, 0x8B, 0x9F, 0xAC, 0x2B,
	  0x9F, 0xE8, 0x44, 0xF2, 0x9A, 0x25, 0xD0, 0x8C,
	  0xF4, 0xC3, 0x6E, 0xFA, 0x47, 0x65, 0xEB, 0x48,
	  0x25, 0xB0, 0x8A, 0xA8, 0xC5, 0xFB, 0xB1, 0x11,
	  0x9A, 0x77, 0x87, 0x24, 0xB1, 0xC0, 0xE9, 0xA2,
	  0x49, 0xD5, 0x19, 0x00, 0x41, 0x6F, 0x2F, 0xBA,
	  0x9F, 0x28, 0x47, 0xF9, 0xB8, 0xBA, 0xFF, 0xF4,
	  0x8B, 0x20, 0xC9, 0xC9, 0x39, 0xAB, 0x52, 0x0E,
	  0x8A, 0x5A, 0xAF, 0xB3, 0xA3, 0x93, 0x4D, 0xBB,
	  0xFE, 0x62, 0x9B, 0x02, 0xCC, 0xA7, 0xB4, 0xAE,
	  0x86, 0x65, 0x88, 0x19, 0xD7, 0x44, 0xA7, 0xE4,
	  0x18, 0xB6, 0xCE, 0x01, 0xCD, 0xDF, 0x36, 0x81,
	  0xD5, 0xE1, 0x62, 0xF8, 0xD0, 0x27, 0xF1, 0x86,
	  0xA8, 0x58, 0xA7, 0xEB, 0x39, 0x79, 0x56, 0x41 },

	/* p */
	512,
	{ 0xCF, 0xDA, 0xF9, 0x99, 0x6F, 0x05, 0x95, 0x84,
	  0x09, 0x90, 0xB3, 0xAB, 0x39, 0xB7, 0xDD, 0x1D,
	  0x7B, 0xFC, 0xFD, 0x10, 0x35, 0xA0, 0x18, 0x1D,
	  0x9A, 0x11, 0x30, 0x90, 0xD4, 0x3B, 0xF0, 0x5A,
	  0xC1, 0xA6, 0xF4, 0x53, 0xD0, 0x94, 0xA0, 0xED,
	  0xE0, 0xE4, 0xE0, 0x8E, 0x44, 0x18, 0x42, 0x42,
	  0xE1, 0x2C, 0x0D, 0xF7, 0x30, 0xE2, 0xB8, 0x09,
	  0x73, 0x50, 0x28, 0xF6, 0x55, 0x85, 0x57, 0x03 },

	/* q */
	512,
	{ 0xC0, 0x81, 0xC4, 0x82, 0x6E, 0xF6, 0x1C, 0x92,
	  0x83, 0xEC, 0x17, 0xFB, 0x30, 0x98, 0xED, 0x6E,
	  0x89, 0x92, 0xB2, 0xA1, 0x21, 0x0D, 0xC1, 0x95,
	  0x49, 0x99, 0xD3, 0x79, 0xD3, 0xBD, 0x94, 0x93,
	  0xB9, 0x28, 0x68, 0xFF, 0xDE, 0xEB, 0xE8, 0xD2,
	  0x0B, 0xED, 0x7C, 0x08, 0xD0, 0xD5, 0x59, 0xE3,
	  0xC1, 0x76, 0xEA, 0xC1, 0xCD, 0xB6, 0x8B, 0x39,
	  0x4E, 0x29, 0x59, 0x5F, 0xFA, 0xCE, 0x83, 0xA5 },

	/* u */
	511,
	{ 0x4B, 0x87, 0x97, 0x1F, 0x27, 0xED, 0xAA, 0xAF,
	  0x42, 0xF4, 0x57, 0x82, 0x3F, 0xEC, 0x80, 0xED,
	  0x1E, 0x91, 0xF8, 0xB4, 0x33, 0xDA, 0xEF, 0xC3,
	  0x03, 0x53, 0x0F, 0xCE, 0xB9, 0x5F, 0xE4, 0x29,
	  0xCC, 0xEE, 0x6A, 0x5E, 0x11, 0x0E, 0xFA, 0x66,
	  0x85, 0xDC, 0xFC, 0x48, 0x31, 0x0C, 0x00, 0x97,
	  0xC6, 0x0A, 0xF2, 0x34, 0x60, 0x6B, 0xF7, 0x68,
	  0x09, 0x4E, 0xCF, 0xB1, 0x9E, 0x33, 0x9A, 0x41 },

	/* exponent1 */
	511,
	{ 0x6B, 0x2A, 0x0D, 0xF8, 0x22, 0x7A, 0x71, 0x8C,
	  0xE2, 0xD5, 0x9D, 0x1C, 0x91, 0xA4, 0x8F, 0x37,
	  0x0D, 0x5E, 0xF1, 0x26, 0x73, 0x4F, 0x78, 0x3F,
	  0x82, 0xD8, 0x8B, 0xFE, 0x8F, 0xBD, 0xDB, 0x7D,
	  0x1F, 0x4C, 0xB1, 0xB9, 0xA8, 0xD7, 0x88, 0x65,
	  0x3C, 0xC7, 0x24, 0x53, 0x95, 0x1E, 0x20, 0xC3,
	  0x94, 0x8E, 0x7F, 0x20, 0xCC, 0x2E, 0x88, 0x0E,
	  0x2F, 0x4A, 0xCB, 0xE3, 0xBD, 0x52, 0x02, 0xFB },

	/* exponent2 */
	509,
	{ 0x10, 0x27, 0xD3, 0xD2, 0x0E, 0x75, 0xE1, 0x17,
	  0xFA, 0xB2, 0x49, 0xA0, 0xEF, 0x07, 0x26, 0x85,
	  0xEC, 0x4D, 0xBF, 0x67, 0xFE, 0x5A, 0x25, 0x30,
	  0xDE, 0x28, 0x66, 0xB3, 0x06, 0xAE, 0x16, 0x55,
	  0xFF, 0x68, 0x00, 0xC7, 0xD8, 0x71, 0x7B, 0xEC,
	  0x84, 0xCB, 0xBD, 0x69, 0x0F, 0xFD, 0x97, 0xB9,
	  0xA1, 0x76, 0xD5, 0x64, 0xC6, 0x5A, 0xD7, 0x7C,
	  0x4B, 0xAE, 0xF4, 0xAD, 0x35, 0x63, 0x37, 0x71 }
	};

static BOOLEAN loadRSAKey( const CRYPT_DEVICE cryptDevice,
						   CRYPT_CONTEXT *cryptContext,
						   CRYPT_CONTEXT *decryptContext )
	{
	CRYPT_PKCINFO_RSA *rsaKey;
	const RSA_KEY *rsaKeyTemplate = &rsa1024Key;
	const BOOLEAN isDevice = ( cryptDevice != CRYPT_UNUSED ) ? TRUE : FALSE;
	int status;

	/* Allocate room for the public-key components */
	if( ( rsaKey = ( CRYPT_PKCINFO_RSA * ) malloc( sizeof( CRYPT_PKCINFO_RSA ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );

	/* Create the encryption context */
	if( cryptContext != NULL )
		{
		if( isDevice )
			status = cryptDeviceCreateContext( cryptDevice, cryptContext,
											   CRYPT_ALGO_RSA );
		else
			status = cryptCreateContext( cryptContext, CRYPT_UNUSED,
										 CRYPT_ALGO_RSA );
		if( cryptStatusError( status ) )
			{
			free( rsaKey );
			printf( "crypt%sCreateContext() failed with error code %d.\n",
					isDevice ? "Device" : "", status );
			return( FALSE );
			}
		cryptSetAttributeString( *cryptContext, CRYPT_CTXINFO_LABEL,
								 "RSA public key", strlen( "RSA public key" ) );
		cryptInitComponents( rsaKey, CRYPT_KEYTYPE_PUBLIC );
		cryptSetComponent( rsaKey->n, rsaKeyTemplate->n, rsaKeyTemplate->nLen );
		cryptSetComponent( rsaKey->e, rsaKeyTemplate->e, rsaKeyTemplate->eLen );
		status = cryptSetAttributeString( *cryptContext,
										  CRYPT_CTXINFO_KEY_COMPONENTS, rsaKey,
										  sizeof( CRYPT_PKCINFO_RSA ) );
		cryptDestroyComponents( rsaKey );
		if( cryptStatusError( status ) )
			{
			free( rsaKey );
			printf( "Key load failed with error code %d.\n", status );
			return( FALSE );
			}
		if( decryptContext == NULL )
			{
			/* We're only using a public-key context, return */
			free( rsaKey );
			return( TRUE );
			}
		}

	/* Create the decryption context */
	if( isDevice )
		status = cryptDeviceCreateContext( cryptDevice, decryptContext,
										   CRYPT_ALGO_RSA );
	else
		status = cryptCreateContext( decryptContext, CRYPT_UNUSED,
									 CRYPT_ALGO_RSA );
	if( cryptStatusError( status ) )
		{
		free( rsaKey );
		printf( "crypt%sCreateContext() failed with error code %d.\n",
				isDevice ? "Device" : "", status );
		return( FALSE );
		}
	cryptSetAttributeString( *decryptContext, CRYPT_CTXINFO_LABEL,
							 "RSA private key", strlen( "RSA private key" ) );
	cryptInitComponents( rsaKey, CRYPT_KEYTYPE_PRIVATE );
	cryptSetComponent( rsaKey->n, rsaKeyTemplate->n, rsaKeyTemplate->nLen );
	cryptSetComponent( rsaKey->e, rsaKeyTemplate->e, rsaKeyTemplate->eLen );
	cryptSetComponent( rsaKey->d, rsaKeyTemplate->d, rsaKeyTemplate->dLen );
	cryptSetComponent( rsaKey->p, rsaKeyTemplate->p, rsaKeyTemplate->pLen );
	cryptSetComponent( rsaKey->q, rsaKeyTemplate->q, rsaKeyTemplate->qLen );
	cryptSetComponent( rsaKey->u, rsaKeyTemplate->u, rsaKeyTemplate->uLen );
	cryptSetComponent( rsaKey->e1, rsaKeyTemplate->e1, rsaKeyTemplate->e1Len );
	cryptSetComponent( rsaKey->e2, rsaKeyTemplate->e2, rsaKeyTemplate->e2Len );
	status = cryptSetAttributeString( *decryptContext,
									  CRYPT_CTXINFO_KEY_COMPONENTS, rsaKey,
									  sizeof( CRYPT_PKCINFO_RSA ) );
	cryptDestroyComponents( rsaKey );
	free( rsaKey );
	if( cryptStatusError( status ) )
		{
		printf( "Key load failed with error code %d.\n", status );
		return( FALSE );
		}

	return( TRUE );
	}

typedef struct {
	const int pLen; const BYTE p[ 128 ];
	const int qLen; const BYTE q[ 20 ];
	const int gLen; const BYTE g[ 128 ];
	const int xLen; const BYTE x[ 20 ];
	const int yLen; const BYTE y[ 128 ];
	} DLP_KEY;

static const DLP_KEY dlp1024Key = {
	/* p */
	1024,
	{ 0x03, 0x16, 0x6D, 0x9C, 0x64, 0x0C, 0x65, 0x25,
	  0xDB, 0x73, 0xD0, 0x7D, 0x8B, 0xB2, 0x48, 0xD3,
	  0x70, 0x53, 0xE1, 0x68, 0x0C, 0x2E, 0x98, 0xF7,
	  0xD9, 0xCA, 0x72, 0x10, 0x03, 0x7C, 0x8D, 0x30,
	  0x02, 0xFB, 0xF2, 0xA1, 0x57, 0x61, 0x0B, 0xA0,
	  0x4A, 0x6D, 0x5B, 0x99, 0xCC, 0xB8, 0xD9, 0x0F,
	  0x9F, 0xD4, 0x3C, 0x67, 0x97, 0x35, 0xDE, 0x8D,
	  0x48, 0xE4, 0x7B, 0x7C, 0xEB, 0x69, 0xA4, 0x9F,
	  0x5C, 0x67, 0xA3, 0x6B, 0x27, 0x49, 0xF9, 0x98,
	  0x0D, 0x3B, 0x85, 0xBC, 0xEC, 0x33, 0x39, 0xB1,
	  0x86, 0xFF, 0xAF, 0x98, 0x34, 0x88, 0x30, 0xC3,
	  0x58, 0x62, 0x65, 0xE0, 0xE4, 0xDF, 0xC7, 0xD3,
	  0x2E, 0x36, 0x4F, 0x21, 0x7C, 0x35, 0x86, 0x59,
	  0x02, 0x3C, 0x0D, 0x94, 0x86, 0xC7, 0xC6, 0x59,
	  0x9C, 0x02, 0x66, 0x55, 0x68, 0x1A, 0x77, 0xD2,
	  0x00, 0x6B, 0x61, 0x41, 0x52, 0x26, 0x18, 0x6B },

	/* q */
	160,
	{ 0xE8, 0x5A, 0x93, 0xE9, 0x4D, 0x15, 0xB5, 0x96,
	  0x7E, 0xE3, 0x2A, 0x47, 0x8E, 0xD4, 0xAC, 0x72,
	  0x3D, 0x82, 0xB6, 0x49 },

	/* g */
	1024,
	{ 0x00, 0xA0, 0xED, 0xFF, 0x76, 0x7C, 0x99, 0xA3,
	  0x43, 0x81, 0x12, 0x78, 0x0F, 0x3D, 0x60, 0xCA,
	  0xA7, 0x5D, 0xA4, 0xCF, 0xC7, 0x45, 0xDE, 0x99,
	  0xAF, 0x2F, 0x5A, 0xD2, 0x2B, 0xF1, 0x49, 0xC7,
	  0x6E, 0xA4, 0x29, 0x78, 0xD7, 0xB1, 0xC0, 0x96,
	  0x06, 0x3F, 0x0E, 0xD5, 0x83, 0xCB, 0x41, 0x47,
	  0x91, 0xFD, 0x93, 0x7C, 0xBA, 0x9A, 0x08, 0xBA,
	  0xF0, 0xFE, 0xFE, 0xE1, 0x32, 0x64, 0x14, 0x80,
	  0x46, 0x21, 0xAD, 0x11, 0xC7, 0x99, 0x3A, 0xB5,
	  0x2E, 0xA4, 0xAD, 0xBE, 0xE2, 0x5E, 0x51, 0x3D,
	  0xBB, 0xEA, 0x43, 0x8F, 0x4E, 0x38, 0x4C, 0xDC,
	  0x11, 0x4D, 0xE4, 0x4E, 0x40, 0x48, 0x38, 0x40,
	  0x23, 0x38, 0xC5, 0x86, 0x0E, 0x7B, 0xF0, 0xC7,
	  0x9B, 0xBC, 0x20, 0x7B, 0x2E, 0x27, 0x5D, 0x2A,
	  0x10, 0x4A, 0x7E, 0x30, 0x45, 0x8C, 0x6F, 0x2C,
	  0x77, 0x31, 0x54, 0xA4, 0xCF, 0xEC, 0x36, 0x83 },

	/* x */
	160,
	{ 0xDF, 0x75, 0x2D, 0x11, 0x5F, 0xDB, 0xF9, 0x7A,
	  0x6F, 0x3F, 0x46, 0xC2, 0xE5, 0xBA, 0x19, 0xF8,
	  0xD8, 0x07, 0xEB, 0x7C },

	/* y */
	1024,
	{ 0x01, 0xFE, 0x13, 0x25, 0xBB, 0xBE, 0xC8, 0xAA,
	  0x81, 0x0A, 0x67, 0x12, 0xB9, 0x2D, 0xE3, 0xD4,
	  0x3C, 0xCD, 0x85, 0x5C, 0x86, 0xD3, 0x9E, 0xB7,
	  0xE6, 0x06, 0x09, 0xA2, 0x94, 0x2D, 0xB3, 0x50,
	  0x59, 0x9F, 0x19, 0x2A, 0x60, 0xA3, 0xD9, 0xC0,
	  0x61, 0xE4, 0x8D, 0x13, 0xE1, 0x84, 0xC9, 0x43,
	  0x62, 0x26, 0x8E, 0xD7, 0x91, 0xDC, 0xBD, 0xAA,
	  0x21, 0x03, 0xA7, 0x96, 0xF2, 0x8F, 0x2F, 0xBF,
	  0x22, 0x67, 0xAB, 0x54, 0xB4, 0x8E, 0x76, 0xDC,
	  0x64, 0xCF, 0x6D, 0x21, 0xDD, 0x9B, 0x41, 0x53,
	  0x11, 0x48, 0x93, 0x12, 0x75, 0x75, 0x1B, 0x1F,
	  0xDA, 0x2B, 0x8E, 0x5C, 0x75, 0xDC, 0x5C, 0x77,
	  0xE7, 0xBE, 0x25, 0x6B, 0xB9, 0x9B, 0x5F, 0x63,
	  0x11, 0xAF, 0x54, 0xEC, 0xFE, 0x08, 0xDE, 0x1E,
	  0x4D, 0x38, 0xE3, 0x77, 0x26, 0x2C, 0xDB, 0xDB,
	  0xAB, 0xC8, 0x60, 0x5F, 0x88, 0x5C, 0x98, 0xFE }
	};

static BOOLEAN loadDSAKey( const CRYPT_DEVICE cryptDevice,
						   CRYPT_CONTEXT *signContext,
						   CRYPT_CONTEXT *sigCheckContext )
	{
	CRYPT_PKCINFO_DLP *dsaKey;
	const DLP_KEY *dlpKeyTemplate = &dlp1024Key;
	const BOOLEAN isDevice = ( cryptDevice != CRYPT_UNUSED ) ? TRUE : FALSE;
	int status;

	/* Allocate room for the public-key components */
	if( ( dsaKey = ( CRYPT_PKCINFO_DLP * ) malloc( sizeof( CRYPT_PKCINFO_DLP ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );

	/* Create the encryption context */
	if( signContext != NULL )
		{
		if( isDevice )
			status = cryptDeviceCreateContext( cryptDevice, signContext,
											   CRYPT_ALGO_DSA );
		else
			status = cryptCreateContext( signContext, CRYPT_UNUSED,
										 CRYPT_ALGO_DSA );
		if( cryptStatusError( status ) )
			{
			free( dsaKey );
			printf( "crypt%sCreateContext() failed with error code %d.\n",
					isDevice ? "Device" : "", status );
			return( FALSE );
			}
		cryptSetAttributeString( *signContext, CRYPT_CTXINFO_LABEL,
								 "DSA private key", strlen( "DSA private key" ) );
		cryptInitComponents( dsaKey, CRYPT_KEYTYPE_PRIVATE );
		cryptSetComponent( dsaKey->p, dlpKeyTemplate->p, dlpKeyTemplate->pLen );
		cryptSetComponent( dsaKey->q, dlpKeyTemplate->q, dlpKeyTemplate->qLen );
		cryptSetComponent( dsaKey->g, dlpKeyTemplate->g, dlpKeyTemplate->gLen );
		cryptSetComponent( dsaKey->x, dlpKeyTemplate->x, dlpKeyTemplate->xLen );
		cryptSetComponent( dsaKey->y, dlpKeyTemplate->y, dlpKeyTemplate->yLen );
		status = cryptSetAttributeString( *signContext,
									CRYPT_CTXINFO_KEY_COMPONENTS, dsaKey,
									sizeof( CRYPT_PKCINFO_DLP ) );
		cryptDestroyComponents( dsaKey );
		if( cryptStatusError( status ) )
			{
			free( dsaKey );
			printf( "Key load failed with error code %d.\n", status );
			return( FALSE );
			}
		if( sigCheckContext == NULL )
			{
			/* We're only using a public-key context, return */
			free( dsaKey );
			return( TRUE );
			}
		}

	/* Create the decryption context */
	if( isDevice )
		status = cryptDeviceCreateContext( cryptDevice, sigCheckContext,
										   CRYPT_ALGO_DSA );
	else
		status = cryptCreateContext( sigCheckContext, CRYPT_UNUSED,
									 CRYPT_ALGO_DSA );
	if( cryptStatusError( status ) )
		{
		free( dsaKey );
		printf( "crypt%sCreateContext() failed with error code %d.\n",
				isDevice ? "Device" : "", status );
		return( FALSE );
		}
	cryptSetAttributeString( *sigCheckContext, CRYPT_CTXINFO_LABEL,
							 "DSA public key", strlen( "DSA public key" ) );
	cryptInitComponents( dsaKey, CRYPT_KEYTYPE_PUBLIC );
	cryptSetComponent( dsaKey->p, dlpKeyTemplate->p, dlpKeyTemplate->pLen );
	cryptSetComponent( dsaKey->q, dlpKeyTemplate->q, dlpKeyTemplate->qLen );
	cryptSetComponent( dsaKey->g, dlpKeyTemplate->g, dlpKeyTemplate->gLen );
	cryptSetComponent( dsaKey->y, dlpKeyTemplate->y, dlpKeyTemplate->yLen );
	status = cryptSetAttributeString( *sigCheckContext,
									  CRYPT_CTXINFO_KEY_COMPONENTS, dsaKey,
									  sizeof( CRYPT_PKCINFO_DLP ) );
	cryptDestroyComponents( dsaKey );
	free( dsaKey );
	if( cryptStatusError( status ) )
		{
		printf( "Key load failed with error code %d.\n", status );
		return( FALSE );
		}

	return( TRUE );
	}

/* Time the RSA operation speed */

static int encRSATest( const CRYPT_CONTEXT cryptContext,
					   const CRYPT_CONTEXT decryptContext,
					   HIRES_TIME times[] )
	{
	BYTE buffer[ CRYPT_MAX_PKCSIZE ];
	HIRES_TIME timeVal;
	int status;

	memset( buffer, '*', 128 );
	buffer[ 0 ] = 1;
	timeVal = timeDiff( 0 );
	status = cryptEncrypt( cryptContext, buffer, 128 );
	times[ 0 ] = timeDiff( timeVal );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't encrypt data, status = %d.\n", status );
		return( FALSE );
		}
	timeVal = timeDiff( 0 );
	status = cryptDecrypt( decryptContext, buffer, 128 );
	times[ 1 ] = timeDiff( timeVal );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't decrypt data, status = %d.\n", status );
		return( FALSE );
		}
	return( TRUE );
	}

static BOOLEAN testRSA( long ticksPerSec )
	{
	CRYPT_CONTEXT cryptContext, decryptContext;
	HIRES_TIME times[ NO_TESTS + 1 ][ 8 ];
	int i;

	memset( times, 0, sizeof( times ) );

	/* Load the RSA keys */
	if( !loadRSAKey( CRYPT_UNUSED, &cryptContext, &decryptContext ) )
		return( FALSE );

	/* Encrypt and decrypt a test buffer */
	printf( "RSA 1024-bit " );
	for( i = 0; i < NO_TESTS + 1; i++ )
		{
		int status;

		status = encRSATest( cryptContext, decryptContext, times[ i ] );
		if( !status )
			return( FALSE );
		}
	printTimes( times, 2, ticksPerSec );

	/* Clean up */
	cryptDestroyContext( cryptContext );
	cryptDestroyContext( decryptContext );

	return( TRUE );
	}

/* Time the DSA operation speed */

static BOOLEAN testDSA( long ticksPerSec )
	{
	CRYPT_CONTEXT signContext, sigCheckContext;
	HIRES_TIME times[ NO_TESTS + 1 ][ 8 ];
	int i;

	memset( times, 0, sizeof( times ) );

	/* Load the DSA keys */
	if( !loadDSAKey( CRYPT_UNUSED, &signContext, &sigCheckContext ) )
		return( FALSE );

	/* Sign and verify a test buffer */
	printf( "DSA 1024-bit " );
	for( i = 0; i < NO_TESTS + 1; i++ )
		{
		DLP_PARAMS dlpParams;
		HIRES_TIME timeVal;
		BYTE buffer[ 128 ];
		int sigSize, status;

		/* Perform the test sign/sig.check */
		setDLPParams( &dlpParams, "********************", 20, buffer, 128 );
		timeVal = timeDiff( 0 );
		status = cryptDeviceQueryCapability( signContext, 1002,
									( CRYPT_QUERY_INFO * ) &dlpParams );
		times[ i ][ 0 ] = timeDiff( timeVal );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't create DSA signature, status = %d.\n",
					status );
			return( FALSE );
			}
		sigSize = dlpParams.outLen;

		timeVal = timeDiff( 0 );
		setDLPParams( &dlpParams, "********************", 20, NULL, 0 );
		dlpParams.inParam2 = buffer;
		dlpParams.inLen2 = sigSize;
		status = cryptDeviceQueryCapability( sigCheckContext, 1003,
									( CRYPT_QUERY_INFO * ) &dlpParams );
		times[ i ][ 1 ] = timeDiff( timeVal );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't verify DSA signature, status = %d.\n",
					status );
			return( FALSE );
			}
		}
	printTimes( times, 2, ticksPerSec );

	/* Clean up */
	cryptDestroyContext( signContext );
	cryptDestroyContext( sigCheckContext );

	return( TRUE );
	}

/* Time the DH operation speed */

static BOOLEAN testDH( long ticksPerSec )
	{
	CRYPT_CONTEXT cryptContext;
	HIRES_TIME times[ NO_TESTS + 1 ][ 8 ];
	int i;

	memset( times, 0, sizeof( times ) );

	printf( "DH  1024-bit " );
	for( i = 0; i < NO_TESTS + 1; i++ )
		{
		KEYAGREE_PARAMS keyAgreeParams;
		HIRES_TIME timeVal;
		int status;

		/* Load the DH key */
		if( !loadDHKey( &cryptContext ) )
			return( FALSE );

		/* Perform the DH key agreement */
		memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
		memset( keyAgreeParams.publicValue, '*', 128 );
		keyAgreeParams.publicValueLen = 128;
		timeVal = timeDiff( 0 );
		status = cryptDeviceQueryCapability( cryptContext, 1001,
									( CRYPT_QUERY_INFO * ) &keyAgreeParams );
		times[ i ][ 0 ] = timeDiff( timeVal );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't perform DH key agreement, status = %d.\n",
					status );
			return( FALSE );
			}

		/* Clean up */
		cryptDestroyContext( cryptContext );
		}
	printTimes( times, 1, ticksPerSec );

	return( TRUE );
	}

/****************************************************************************
*																			*
*							Standalone Runtime Support						*
*																			*
****************************************************************************/

/* Standalone main when we're not called from testlib.c */

int main( int argc, char **argv )
	{
	CRYPT_CONTEXT cryptContext;
#ifdef __WINDOWS__
	LARGE_INTEGER performanceCount;
#endif /* __WINDOWS__ */
	long ticksPerSec;
	int status;

	/* Get rid of compiler warnings */
	if( argc || argv );

	/* Initialise cryptlib */
	status = cryptInit();
	if( cryptStatusError( status ) )
		{
		printf( "cryptInit() failed with error code %d.\n", status );
		exit( EXIT_FAILURE );
		}

	/* Try and bypass the randomness-handling by adding some junk (this only
	   works in the Windows debug build), then ensure that the randomness-
	   polling has completed by performing a blocking operation that
	   requires randomness */
#ifndef __WINDOWS__
	puts( "Forcing RNG reseed, this may take a few seconds..." );
#endif /* __WINDOWS__ */
	cryptAddRandom( "xyzzy", 5 );
	cryptCreateContext( &cryptContext, CRYPT_UNUSED, CRYPT_ALGO_DES );
	cryptGenerateKey( cryptContext );
	cryptDestroyContext( cryptContext );

	printf( "Times given in clock ticks of frequency " );
#ifdef __WINDOWS__
	QueryPerformanceFrequency( &performanceCount );
	ticksPerSec = performanceCount.LowPart;
	printf( "%ld", ticksPerSec );
#else
	printf( "~1M" );
	ticksPerSec = 1000000;
#endif /* __WINDOWS__ */
	printf( " ticks per second,\nresult rows are alignment offsets +0, "
			"+1, +4 with %d tests per result.\n\n", NO_TESTS );

	status = testDH( ticksPerSec );
	if( !status )
		{
		puts( "  (Did you make the cryptapi.c changes described at the "
			  "start of timings.c?)" );
		}
	testRSA( ticksPerSec );
	status = testDSA( ticksPerSec );
	if( !status )
		{
		puts( "  (Did you make the cryptapi.c changes described at the "
			  "start of timings.c?)" );
		}
	performanceTests( CRYPT_UNUSED, ticksPerSec );

	/* Clean up */
	cryptEnd();
	return( EXIT_SUCCESS );
	}
#endif /* __WINDOWS__ || __UNIX__ */
