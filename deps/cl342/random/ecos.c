/****************************************************************************
*																			*
*							eCOS Randomness-Gathering Code					*
*						 Copyright Peter Gutmann 1996-2005					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c.

   eCos provides very few entropy sources, the code below provides very 
   little usable entropy and should not be relied upon as the sole entropy 
   source for the system but should be augmented with randomness from 
   external hardware sources */

/* General includes */

#include "crypt.h"
#include "random/random.h"

/* OS-specific includes */

#include <cyg/hal/hal_arch.h>
#include <cyg/kernel/kapi.h>
#ifdef CYGPKG_POWER
  #include <cyg/power/power.h>
#endif /* CYGPKG_POWER */
#ifdef CYGPKG_IO_PCI
  #include <cyg/io/pci.h>
#endif /* CYGPKG_IO_PCI */

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	256

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	cyg_handle_t hThread = 0;
	cyg_uint16 threadID = 0;
#ifdef CYGPKG_IO_PCI
	cyg_pci_device_id pciDeviceID;
#endif /* CYGPKG_IO_PCI */
#ifdef CYGPKG_POWER
	PowerController *powerControllerInfo;
#endif /* CYGPKG_POWER */
	int itemsAdded = 0, iterationCount;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get the thread handle, ID, state, priority, and stack usage for 
	   every thread in the system */
	for( iterationCount = 0;
		 cyg_thread_get_next( &hThread, &threadID ) && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		cyg_thread_info threadInfo;

		if( !cyg_thread_get_info( hThread, threadID, &threadInfo ) )
			continue;
		addRandomData( randomState, &threadInfo, sizeof( cyg_thread_info ) );
		itemsAdded++;
		}

	/* Walk the power-management info getting the power-management state for 
	   each device.  This works a bit strangely, the power controller 
	   information is a static table created at system build time so all that 
	   we're doing is walking down an array getting one entry after another */
#ifdef CYGPKG_POWER
	for( powerControllerInfo = &( __POWER__[ 0 ] ), iterationCount = 0;
		 powerControllerInfo != &( __POWER_END__ ) && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 powerControllerInfo++, iterationCount++ )
		{
		const PowerMode mode = \
			power_get_controller_mode( powerControllerInfo );

		addRandomValue( randomState, mode );
		}
#endif /* CYGPKG_POWER */

	/* Add PCI device information if there's PCI support present */
#ifdef CYGPKG_IO_PCI
	if( cyg_pci_find_next( CYG_PCI_NULL_DEVID, &pciDeviceID ) )
		{
		iterationCount = 0;
		do
			{
			cyg_pci_device pciDeviceInfo;

			cyg_pci_get_device_info( pciDeviceID, &pciDeviceInfo );
			addRandomValue( randomState, PowerMode );
			addRandomData( randomState, &pciDeviceInfo, 
						   sizeof( cyg_pci_device ) );
			itemsAdded++;
			}
		while( cyg_pci_find_next( pciDeviceID, &pciDeviceID ) && \
			   iterationCount++ < FAILSAFE_ITERATIONS_MED );
		}
#endif /* CYGPKG_IO_PCI */

	/* eCOS also has a CPU load-monitoring facility that we could in theory 
	   use as a source of entropy but this is really meant for performance-
	   monitoring and isn't very suitable for use as an entropy source.  The 
	   way this works is that you first call a calibration function 
	   cyg_cpuload_calibrate() and then when it you want to get load 
	   statistics call cyg_cpuload_create()/cyg_cpuload_get()/
	   cyg_cpuload_delete(), with get() returning the load over a 0.1s, 1s, 
	   and 10s interval.  The only one of these capabilities that's even 
	   potentially usable is cyg_cpuload_calibrate() and even that's rather 
	   dubious for general use since it runs a thread at the highest priority 
	   level for 0.1s for calibration purposes and measures the elapsed tick 
	   count, which will hardly endear us to other threads in the system.  
	   It's really meant for development-mode load measurements and can't 
	   safely be used as an entropy source */

	/* Flush any remaining data through and produce an estimate of its
	   value.  Unlike its use in standard OSes this isn't really a true 
	   estimate since virtually all of the entropy is coming from the seed
	   file, all this does is complete the seed-file quality estimate to
	   make sure that we don't fail the entropy test */
	endRandomData( randomState, ( itemsAdded > 5 ) ? 20 : 0 );
	}

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE ];
	cyg_uint32 clockTicks;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	hal_clock_read( &clockTicks );

	endRandomData( randomState, 1 );
	}
