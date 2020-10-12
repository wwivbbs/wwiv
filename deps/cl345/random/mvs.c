/****************************************************************************
*																			*
*						MVS Randomness-Gathering Code						*
*					  Copyright Peter Gutmann 1999-2017						*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* General includes */

#define _OPEN_SYS		1
#define _OPEN_SYS_EXT	1
#include <sys/ps.h>
#include <sys/times.h>
#include <sys/resource.h>
#include "crypt.h"
#include "random/random.h"

/* Define the MVS assembler module used to gather random data */

#pragma linkage( MVSENT, OS )
#pragma map( readRandom, "MVSENT" )
int readRandom( int length, unsigned char *buffer );

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	512

/* Slow and fast polling routines.  There really isn't much that we can
   get under MVS, and we have to be careful how much we do with readRandom()
   since it can become quite resource-intensive on some systems (where
   "resource-intensive" means "bring down a parallel sysplex in its earlier
   form before additional safeguards were added") so we can't call it from 
   fastPoll() and only get a small amount of data with a slow poll */

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	hreport_t heapReport;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* There really isn't much available under MVS, it would be nice if we
	   could get output from DISPLAY but this requires a level of plumbing
	   that isn't easily managed from C */
	addRandomLong( randomState, clock() );
	if( __heaprpt( &heapReport ) != -1 )
		addRandomData( randomState, &heapReport, sizeof( hreport_t ) );

	endRandomData( randomState, 2 );
	}

static void getProcInfo( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	W_PSPROC wProcInfo;
	int token, entryCount;

	/* Initialise the W_PSPROC structure.  This requires dynamically-
	   allocated internal fields set up by the caller, so we have to go 
	   through a complex malloc() dance to allocate them all */
	memset( &wProcInfo, 0, sizeof( W_PSPROC ) );
	wProcInfo.ps_conttylen = PS_CONTTYBLEN;
	if( ( wProcInfo.ps_conttyptr = malloc( wProcInfo.ps_conttylen ) ) == NULL )
		return;
	wProcInfo.ps_pathlen = PS_PATHBLEN;
	if( ( wProcInfo.ps_pathptr = malloc( wProcInfo.ps_pathlen ) ) == NULL )
		{
		free( wProcInfo.ps_conttyptr );
		return;
		}
	wProcInfo.ps_cmdlen = PS_CMDBLEN_LONG;
	if( ( wProcInfo.ps_cmdptr = malloc( wProcInfo.ps_cmdlen ) ) == NULL )
		{
		free( wProcInfo.ps_conttyptr );
		free( wProcInfo.ps_pathptr );
		return;
		}

	/* Walk through the process table getting information on each process */
	initRandomData( randomState, buffer, RANDOM_BUFSIZE );
    token = w_getpsent( 0, &wProcInfo, sizeof( W_PSPROC ) );
	for( entryCount = 0; token > 0 && entryCount < 256; entryCount++ )
		{
		addRandomData( randomState, &wProcInfo, sizeof( W_PSPROC ) );
	    token = w_getpsent( token, &wProcInfo, sizeof( W_PSPROC ) );
		}
	endRandomData( randomState, min( entryCount, 45 ) );

	/* Clean up */
	free( wProcInfo.ps_conttyptr );
	free( wProcInfo.ps_pathptr );
	free( wProcInfo.ps_cmdptr );
	}

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	MESSAGE_DATA msgData;
	struct _Optn *sysOptions;
	BYTE buffer[ RANDOM_BUFSIZE ], cpuidBuffer[ 16 ];
	int quality = 95, status;

	/* Get static system information.  The system options are a catalogue of
	   about two hundred configuration options and settings for the current
	   system */
	initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	__get_cpuid( cpuidBuffer );
	addRandomData( randomState, cpuidBuffer, 11 );
	endRandomData( randomState, 2 );
	sysOptions = __get_system_settings();
	if( sysOptions != NULL )
		{
		static const int optionsQuality = 10;

		setMessageData( &msgData, sysOptions, sizeof( struct _Optn ) );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_IATTRIBUTE_ENTROPY );
		if( cryptStatusOK( status ) )
			{
			krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
							 ( MESSAGE_CAST ) &optionsQuality, 
							 CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
			}
		}

	/* Get information on all processes in the system */
	getProcInfo();

	/* Get entropy from the low-level polling code in mvsent.s */
	status = readRandom( RANDOM_BUFSIZE, buffer );
	if( status != 0 )
		{
		assert( DEBUG_WARN );
		return;
		}
	setMessageData( &msgData, buffer, RANDOM_BUFSIZE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_ENTROPY );
	if( cryptStatusOK( status ) )
		{
		krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
						 &quality, CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
		}
	zeroise( buffer, sizeof( buffer ) );
	}
