/****************************************************************************
*																			*
*						ChorusOS Randomness-Gathering Code					*
*						 Copyright Peter Gutmann 1996-2005					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c.

   This code represents a template for randomness-gathering only and will
   need to be modified to provide randomness via an external source.  In its
   current form it does not provide any usable entropy and should not be
   used as an entropy source */

/* General includes */

#include <chorus.h>
#include <afexec.h>
#include <exec/chTime.h>
#include "crypt.h"

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	256

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	KnTimeVal timeVal;

	/* The sysTime() ns timer seems to be quantised to ms rather than
	   being true ns timer output.  Other (supposedly) high-speed timers
	   like sysBench() are no better, on x86 hardware this is quantised
	   to 5ms intervals */
	initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	sysTime( &timeVal );
	addRandomData( randomState, &timeVal, sizeof( KnTimeVal ) );
	endRandomData( randomState, 1 );
	}

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	addRandomValue( randomState, agetId() );
	endRandomData( randomState, 1 );

	/* ChorusOS provides the MkStat family of calls (equivalent to Sun's
	   kstats), but these can only be called in supervisor mode */
	return;
	}
