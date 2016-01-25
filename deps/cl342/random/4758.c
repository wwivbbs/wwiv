/****************************************************************************
*																			*
*						 IBM 4758 Randomness-Gathering Code					*
*						 Copyright Peter Gutmann 1998-2002					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* General includes */

#include <time.h>
#include "../crypt.h"

/* OS-specific includes */

#include "scc_int.h"

/* The size of the returned random data and the number of calls we make for
   a slow poll.  Since the 4758 uses a hardware RNG, it doesn't matter if we
   call it several times in succession */

#define SCC_RANDOM_SIZE		8		/* 64 bits */
#define SCC_NO_CALLS		4		/* 256 bits total */

void fastPoll( void )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ SCC_RANDOM_SIZE ];

	sccGetRandomNumber( buffer, RANDOM_RANDOM );
	setResourceData( &msgData, buffer, SCC_RANDOM_SIZE );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
					 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
	zeroise( buffer, SCC_RANDOM_SIZE );
	}

void slowPoll( void )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ SCC_RANDOM_SIZE * SCC_NO_CALLS ];
	int quality = 100, i;

	for( i = 0; i < SCC_NO_CALLS; i++ )
		sccGetRandomNumber( buffer + ( i * SCC_RANDOM_SIZE ), RANDOM_RANDOM );

	/* Add the data to the randomness pool */
	setResourceData( &msgData, buffer, SCC_RANDOM_SIZE * SCC_NO_CALLS );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
					 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
	zeroise( buffer, SCC_RANDOM_SIZE * SCC_NO_CALLS );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
					 &quality, CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
	}
