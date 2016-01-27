/****************************************************************************
*																			*
*						MVS Randomness-Gathering Code						*
*					  Copyright Peter Gutmann 1999-2003						*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* General includes */

#include <sys/times.h>
#include <sys/resource.h>
#include "crypt.h"

/* Define the MVS assembler module used to gather random data */

#pragma linkage( MVSENT, OS )
#pragma map( readRandom, "MVSENT" )
int readRandom( int length, unsigned char *buffer );

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	512

/* Slow and fast polling routines.  There really isn't much that we can
   get under MVS, and we have to be careful how much we do with readRandom()
   since it can become quite resource-intensive on some systems so we can't
   call it from fastPoll() and only get a small amount of data with a slow
   poll */

void fastPoll( void )
	{
	MESSAGE_DATA msgData;
	hreport_t heapReport;
	const clock_t timeStamp = clock();
	int quality = 5;

	/* There really isn't much available under MVS, it would be nice if we
	   could get output from DISPLAY but this requires a level of plumbing
	   that isn't easily managed from C */
	setMessageData( &msgData, &timeStamp, sizeof( clock_t ) );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
					 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
	if( __heaprpt( &heapReport ) != -1 )
		{
		setMessageData( &msgData, &heapReport, sizeof( hreport_t ) );
		krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
						 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
		}
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
					 &quality, CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
	}

void slowPoll( void )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ RANDOM_BUFSIZE ];
	int quality = 95, status;

	status = readRandom( RANDOM_BUFSIZE, buffer );
	assert( status == 0 );
	setMessageData( &msgData, buffer, RANDOM_BUFSIZE );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S, &msgData,
					 CRYPT_IATTRIBUTE_ENTROPY );
	zeroise( buffer, sizeof( buffer ) );
	if( status == 0 )
		krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
						 &quality, CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
	}
