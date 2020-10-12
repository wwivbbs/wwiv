/****************************************************************************
*																			*
*						VM/CMS Randomness-Gathering Code					*
*						  Copyright Peter Gutmann 1999						*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c.

   This code represents a template for randomness-gathering only and will
   need to be modified to provide OS-specific randomness.  In its current
   form it does not provide any usable entropy and should not be used as an
   entropy source */

/* General includes */

#include "crypt.h"

/* OS-specific includes */

void fastPoll( void )
	{
	MESSAGE_DATA msgData;
	time_t timeStamp = time( NULL );

	/* There's not much we can do under VM */
	setMessageData( &msgData, &timeStamp, sizeof( time_t ) );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S, &msgData,
					 CRYPT_IATTRIBUTE_ENTROPY );
	}

void slowPoll( void )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ 128 ];
	int quality = 1, total = 128;

	/* Kludge something here */
	setMessageData( &msgData, buffer, total );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S, &msgData,
					 CRYPT_IATTRIBUTE_ENTROPY );
	zeroise( buffer, sizeof( buffer ) );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE, &quality,
					 CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
	}
