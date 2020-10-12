/****************************************************************************
*																			*
*						Mongoose OS Randomness-Gathering Code				*
*						 Copyright Peter Gutmann 1996-2017					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c.

   This code represents a template for randomness-gathering only and will
   need to be modified to provide randomness via an external source.  In its
   current form it does not provide any usable entropy and should not be
   used as an entropy source */

/* General includes */

#include <mgos.h>
#include "crypt.h"
#include "random/random.h"

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	256

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	struct mgos_uart_stats *uartStats;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	addRandomLong( randomState, mgos_uptime() );
	addRandomLong( randomState, mgos_get_free_heap_size() );
	addRandomLong( randomState, mgos_get_fs_memory_usage() );
	addRandomLong( randomState, mgos_wifi_sta_get_rssi() );
	addRandomLong( randomState, mgos_mqtt_num_unsent_bytes() );
	addRandomLong( randomState, mgos_uart_read_avail() );
	addRandomLong( randomState, mgos_uart_write_avail() );
	if( ( uartStats = mgos_uart_get_stats() ) != NULL )
		addRandomData( randomState, uartStats, sizeof( uartStats ) );
	endRandomData( randomState, 2 );
	}

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	const char *strPtr;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	addRandomLong( randomState, mgos_get_free_fs_size() );
	addRandomLong( randomState, mgos_get_cpu_freq() );
	addRandomLong( randomState, mgos_wifi_get_status() );
	if( ( strPtr = mgos_wifi_get_connected_ssid() ) != NULL )
		{
		addRandomData( randomState, strPtr, strlen( strPtr ) );
		free( strPtr );
		}
	if( ( strPtr = mgos_wifi_get_sta_default_dns() ) != NULL )
		{
		addRandomData( randomState, strPtr, strlen( strPtr ) );
		free( strPtr );
		}
	endRandomData( randomState, 5 );
	}
