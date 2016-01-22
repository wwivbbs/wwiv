/****************************************************************************
*																			*
*						  BeOS Randomness-Gathering Code					*
*			Copyright Peter Gutmann and Osma Ahvenlampi 1996-2003			*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* General includes */

#include "crypt.h"
#include "random/random.h"

/* These get defined by the Be headers */

#undef min
#undef max

/* OS-specific includes */

#include <fcntl.h>
#include <sys/time.h>
#include <kernel/fs_info.h>
#include <kernel/OS.h>
#include <kernel/image.h>

/* The following is defined in <interface/InterfaceDefs.h>, however we
   can't include that header because it uses some C++ keywords and so won't
   compile when used with C code.

   Unfortunately the C++-isms go further than that, extending to the
   interface kit functions like idle_time(), modifiers(), and get_key_info(),
   which are only present in libraries in C++ name-mangled form and can't
   safely be called from a C-only module */

typedef struct {
	uint32  modifiers;
	uint8   key_states[16];
	} key_info;

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	4096

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	struct timeval tv;
	system_info info;
	bigtime_t idleTime;
	uint32 value;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	gettimeofday( &tv, NULL );
	addRandomValue( randomState, tv.tv_sec );
	addRandomValue( randomState, tv.tv_usec );

	/* Get the number of microseconds since the user last provided any input
	   to any part of the system, the state of keyboard shift keys */
#if 0	/* See comment at start */
	idleTime = idle_time();
	addRandomData( randomState, &idleTime, sizeof( bigtime_t ) );
	value = modifiers();
	addRandomValue( randomState, value );
#endif /* 0 */

	/* Get various fixed values (the 64-bit machine ID, CPU count and type(s),
	   clock speed, platform type, etc) and variable resources (number of in-
	   use pages, semaphores, ports, threads, teams, number of page faults,
	   and number of microseconds the CPU has been active) */
	get_system_info( &info );
	addRandomData( randomState, &info, sizeof( info ) );

	/* Flush any remaining data through */
	endRandomData( randomState, 5 );
	}

#define DEVRANDOM_BITS		4096

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	key_info keyInfo;
	team_info teami;
	thread_info threadi;
	area_info areai;
	port_info porti;
	sem_info semi;
	image_info imagei;
	double temperature;
	int32 devID, cookie;
	int fd, value;

	if( ( fd = open( "/dev/urandom", O_RDONLY ) ) >= 0 )
		{
		MESSAGE_DATA msgData;
		BYTE buffer[ ( DEVRANDOM_BITS / 8 ) + 8 ];
		static const int quality = 100;

		/* Read data from /dev/urandom, which won't block (although the
		   quality of the noise is lesser). */
		read( fd, buffer, DEVRANDOM_BITS / 8 );
		setMessageData( &msgData, buffer, DEVRANDOM_BITS / 8 );
		krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
						 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
		zeroise( buffer, DEVRANDOM_BITS / 8 );
		close( fd );

		krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
						 ( MESSAGE_CAST ) &quality,
						 CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
		return;
		}

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get the state of all keys on the keyboard and various other
	   system states */
#if 0	/* See comment at start */
	if( get_key_info( &keyInfo ) == B_NO_ERROR )
		addRandomData( randomState, &keyInfo, sizeof( key_info ) );
#endif /* 0 */
	value = is_computer_on();	/* Returns 1 if computer is on */
	addRandomValue( randomState, value );
	temperature = is_computer_on_fire();	/* MB temp.if on fire */
	addRandomData( randomState, &temperature, sizeof( double ) );

	/* Get information on all running teams (thread groups, ie applications).
	   This returns the team ID, number of threads, images, and areas,
	   debugger port and thread ID, program args, and uid and gid */
	cookie = 0;
	while( get_next_team_info( &cookie, &teami ) == B_NO_ERROR )
		addRandomData( randomState, &teami, sizeof( teami ) );

	/* Get information on all running threads.  This returns the thread ID,
	   team ID, thread name and state (eg running, suspended, asleep,
	   blocked), the thread priority, elapsed user and kernel time, and
	   thread stack information */
	cookie = 0;
	while( get_next_thread_info( 0, &cookie, &threadi ) == B_NO_ERROR )
		{
		addRandomValue( randomState, has_data( threadi.thread ) );
		addRandomData( randomState, &threadi, sizeof( threadi ) );
		}

	/* Get information on all memory areas (chunks of virtual memory).  This
	   returns the area ID, name, size, locking scheme and protection bits,
	   ID of the owning team, start address, number of resident bytes, copy-
	   on-write count, an number of pages swapped in and out */
	cookie = 0;
	while( get_next_area_info( 0, &cookie, &areai ) == B_NO_ERROR )
		addRandomData( randomState, &areai, sizeof( areai ) );

	/* Get information on all message ports.  This returns the port ID, ID of
	   the owning team, message queue length, number of messages in the
	   queue, and total number of messages processed */
	cookie = 0;
	while( get_next_port_info( 0, &cookie, &porti ) == B_NO_ERROR )
		addRandomData( randomState, &porti, sizeof( porti ) );

	/* Get information on all semaphores.  This returns the semaphore and
	   owning team ID, the name, thread count, and the ID of the last thread
	   which acquired the semaphore */
	cookie = 0;
	while( get_next_sem_info( 0, &cookie, &semi ) == B_NO_ERROR )
		addRandomData( randomState, &semi, sizeof( semi ) );

	/* Get information on all images (code blocks, eg applications, shared
	   libraries, and add-on images (DLL's on steroids).  This returns the
	   image ID and type (app, library, or add-on), the order in which the
	   image was loaded compared to other images, the address of the init
	   and shutdown routines, the device and node where the image lives,
	   and the image text and data sizes) */
	cookie = 0;
	while( get_next_image_info( 0, &cookie, &imagei ) == B_NO_ERROR )
		addRandomData( randomState, &imagei, sizeof( imagei ) );

	/* Get information on all storage devices.  This returns the device
	   number, root inode, various device parameters such as I/O block size,
	   and the number of free and used blocks and inodes */
	devID = 0;
	while( next_dev( &devID ) >= 0 )
		{
		fs_info fsInfo;

		if( fs_stat_dev( devID, &fsInfo ) == B_NO_ERROR )
			addRandomData( randomState, &fsInfo, sizeof( fs_info ) );
		}

	/* Flush any remaining data through */
	endRandomData( randomState, 100 );
	}
