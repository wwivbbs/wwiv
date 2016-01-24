/****************************************************************************
*																			*
*						uC/OS II Randomness-Gathering Code					*
*						 Copyright Peter Gutmann 1996-2005					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c.

   This code represents a template for randomness-gathering only and will
   need to be modified to provide randomness via an FPGA hardware entropy
   source such as a standard ring oscillator generator.  In its current
   form it does not provide any usable entropy and should not be used as an
   entropy source */

/* General includes */

#include "crypt.h"

/* OS-specific includes */

void fastPoll( void )
	{
	return;
	}

void slowPoll( void )
	{
	fastPoll();
	fastPoll();
	fastPoll();
	fastPoll();
	fastPoll();
	}
