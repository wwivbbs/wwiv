/****************************************************************************
*																			*
*						Tandem Randomness-Gathering Code					*
*						Copyright Peter Gutmann 2002-2004					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* Define the following to print diagnostic information on where randomness
   is coming from */

/* #define DEBUG_RANDOM	*/

/* General includes */

#include "crypt.h"
#include "random/random.h"

/* OS-specific includes */

#include <cextdecs>
#include <tal.h>

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	4096

/* The number of processes per CPU that we fetch attributes for.  We set an
   upper limit on this to make sure that we don't take too much time doing
   it */

#define NO_PROCESSES	50

/* The CPU attributes that we fetch.  This typically yields 150 words
   of attribute data */

static const short int cpuAttrList[] = {
		2,		/* INT: Processor type */
		7,		/* INT_32: Swappable pages */
		8,		/* INT_32: Free pages */
		9,		/* INT_32: Current locked pages */
		11, 	/* INT_32: Max.locked memory pages */
		12, 	/* INT_32: No.page faults */
		13,		/* INT_32: Scans per memory manager call */
		14,		/* INT_32: No.mem manager scans */
		15,		/* INT_32: Page fault frequency indicator */
		16,		/* INT_32: Paging queue length */
		18,		/* FIXED: Wall clock time */
		19,		/* FIXED: CPU time */
		20,		/* FIXED: Idle time */
		21,		/* FIXED: Interrupt time */
		22,		/* INT: Process queue length */
		23,		/* INT_32: No.dispatch interrupts */
		24,		/* INT[]: Process Control Blocks (PCBs) in low PINs */
		25,		/* INT[]: Process Control Blocks (PCBs) in high PINs */
		26,		/* INT_32[]: Time List Elements (TLEs) */
		27,		/* INT_32[]: Process Time List Elements (TLEs) */
		28,		/* INT: No.breakpoints currently set */
		29,		/* FIXED: Time spent in message sends */
		35,		/* INT_32[24]: Interrupt count */
		36,		/* FIXED: Disk cache hits */
		37,		/* FIXED: Disk I/Os */
		38,		/* INT,INT,/* FIXED: Processor queue state */
		39,		/* INT,INT,/* FIXED: Memory queue state */
		40,		/* INT_32: IPC sequenced messages */
		41,		/* INT_32: IPC unsequenced messages */
		42,		/* INT_32: Correctable memory errors */
		43,		/* INT_32: VM pages created */
		44,		/* FIXED: Time spent in interpreter (for NSR-L CPUs) */
		45,		/* INT_32: Interpreter transitions (for NSR-L CPUs) */
		46,		/* INT_32: Transactions since Measure product started */
		50,		/* FIXED: Time in accelerated mode (for TNS/R CPUs) */
		58,		/* INT_32: Kernel segments in use */
		59		/* INT_32: Maximum segments used */
		};

#define NO_CPU_ATTRS	( sizeof( cpuAttrList ) / sizeof( short int ) )

/* The process attributes that we fetch.  This typically yields 80 words
   of attribute data */

static const short int procAttrList[] = {
		1,		/* INT: Creator access ID */
		2,		/* INT: Process access ID */
		6,		/* INT[10]: gmom process handle */
		7,		/* INT: Job ID */
		8,		/* INT: Process subtype */
		10,		/* INT: Process state */
		11,		/* INT[2]: System process type */
		15,		/* INT: Process list */
		28,		/* INT: Process type */
		30,		/* FIXED: Process time */
		31,		/* INT: Wait state */
		32,		/* INT: Process state */
		35,		/* INT: Context switches */
		41,		/* INT: Process file security */
		42,		/* INT: Current priority */
		43,		/* INT: Initial priority */
		44,		/* INT: Remote creator */
		45,		/* INT: Logged-on state */
		47,		/* INT: Prim. or sec.in process pair */
		48,		/* INT: Process handle */
		53,		/* FIXED: Creation timestamp */
		54,		/* INT: No resident pages */
		55,		/* INT_32: Messages sent */
		56,		/* INT_32: Messages received */
		57,		/* INT: Receive queue length */
		58,		/* INT: Receive queue max.length */
		59,		/* INT_32: Page faults */
		63,		/* INT: Stop mode */
		64,		/* INT: Stop request queue */
		72,		/* INT: Logon flags and states */
		73,		/* INT: Applicable attributes */
		76,		/* INT_32: Curr.process file segment (PFS) size */
		77,		/* INT_32: Max.process file segment (PFS) extent */
		102,	/* INT_32: Base addr.of main stack */
		103,	/* INT_32: Current main stack size */
		104,	/* INT_32: Max.main stack extent */
		105,	/* INT_32: Base addr.of privileged stack */
		106,	/* INT_32: Current privileged stack size */
		107,	/* INT_32: Max.privileged stack extent */
		108,	/* INT_32: Base addr. of global data */
		109,	/* INT_32: Size of global data */
		110,	/* INT_32: Base addr.of native heap */
		111,	/* INT_32: Current native heap size */
		112		/* INT_32: Max.native heap extent */
		};

#define NO_PROC_ATTRS	( sizeof( procAttrList ) / sizeof( short int ) )

/* Get a list of process IDs of all running processes */

static int getProcessList( const short int cpuNo, short int *pidBuffer,
						   const short int pidBufSize )
	{
	const static short int retAttrList[] = { 38 };	/* Process ID */
	const static short int srchAttrList[] = { 9 };	/* Minimum priority */
	const static short int srchValList[] = { 0 };	/* Priority >= 0 */
	short int error, pin = 0, noAttrs;

	/* Get a list of active processes by searching for all processes with
	   a minimum priority (attribute 9) >= 0, returning a list of all
	   process IDs.  The search can return error code 7 (no more matches)
	   if we run out of processes before we run out of buffer space, this
	   isn't an error since we still got some matches */
	error = PROCESS_GETINFOLIST_( cpuNo				/* CPU no.*/
								  ,&pin				/* Process ID */
								  ,					/* Node name */
								  ,					/* Node name length */
								  ,					/* Process handle */
								  ,( short int * ) retAttrList
									  				/* Attrs.to read */
								  ,1				/* No.attrs.to read */
								  ,pidBuffer		/* Returned attrs.*/
								  ,pidBufSize		/* Ret.attrs.buffer size */
								  ,&noAttrs			/* No.returned attrs.*/
								  ,					/* Error info */
								  ,2				/* Find as many as will fit */
								  ,( short int * ) srchAttrList
								  					/* Attrs.to search for */
								  ,1				/* No.attrs.to search for */
								  ,( short int * ) srchValList
								  					/* Attr.value to search for */
								  ,1				/* No.attr.values */
								  );
	return( ( error == 0 || error == 7 ) ? noAttrs : 0 );
	}

/* Get various quick pieces of info */

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	long long totalTime, busyTime, intTime, idleTime;
	_cc_status cc;
	short int value, error;
	int quality = 0;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* CPU usage info */
	cc = CPUTIMES( 						/* Local CPU */
				   ,					/* Local system */
				   ,&totalTime			/* Wall clock time */
				   ,&busyTime			/* CPU time */
				   ,&intTime			/* Interrupt time */
				   ,&idleTime			/* Idle time */
				   );
	if( _status_eq( cc ) )
		{
		addRandomData( randomState, &totalTime, sizeof( long long ) );
		addRandomData( randomState, &busyTime, sizeof( long long ) );
		addRandomData( randomState, &intTime, sizeof( long long ) );
		addRandomData( randomState, &idleTime, sizeof( long long ) );
		quality = 2;
		}

	/* Message queue info */
	error = MESSAGESYSTEMINFO( 4		/* Messages in rcv.queue */
							   ,&value	/* No.messages */
							   );
	if( error == 0 )
		{
		addRandomValue( randomState, value );
		quality++;
		}
	error = MESSAGESYSTEMINFO( 5		/* Messages in send.queue */
							   ,&value	/* No.messages */
							   );
	if( error == 0 )
		{
		addRandomValue( randomState, value );
		quality++;
		}

	/* Runtime of current process in microseconds */
	totalTime = MYPROCESSTIME();
	addRandomData( randomState, &totalTime, sizeof( long long ) );

	/* Flush any remaining data through */
#ifdef DEBUG_RANDOM
	printf( "fastPoll: quality = %d.\n", quality );
#endif /* DEBUG_RANDOM */
	endRandomData( randomState, quality );
	}

/* Enumerate all processes running on all CPUs and fetch the statistics
   that are likely to be high-entropy */

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	const long cpuStatus = PROCESSORSTATUS() & 0xFFFFUL;
	long cpuStatusMask;
	short int cpuNo;
	int attrCount = 0;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Enumerate all available CPUs.  Although the docs indicate that
	   PROCESSORSTATUS() starts numbering CPUs from bit 0, it actually
	   appears to number them from bit 15 */
	for( cpuStatusMask = 0x8000, cpuNo = 0; \
		 cpuStatusMask > 0; \
		 cpuStatusMask >>= 1, cpuNo++ )
		{
		short int pidBuffer[ NO_PROCESSES + 8 ];
		short int noAttrs, error;
		int i, noProcesses;

		/* If this CPU isn't available, continue */
		if( !( cpuStatusMask & cpuStatus ) )
			continue;

#ifdef DEBUG_RANDOM
		printf( "Getting info for CPU #%d.\n", cpuNo );
#endif /* DEBUG_RANDOM */
		/* Get status info for this CPU */
		error = PROCESSOR_GETINFOLIST_(				/* Node name */
										,			/* Node name length */
										,cpuNo		/* CPU no.*/
										,( short int * ) cpuAttrList
													/* Attrs.to read */
										,NO_CPU_ATTRS	/* No.attrs.to read */
										,( short int * ) buffer
														/* Returned attrs.*/
										,RANDOM_BUFSIZE / sizeof( short int )
													/* Ret.attrs.buffer size */
										,&noAttrs	/* No.returned attrs.*/
										);
		if( error == 0 )
			{
#ifdef DEBUG_RANDOM
			printf( "PROCESSOR_GETINFOLIST returned %d attributes.\n",
					noAttrs );
#endif /* DEBUG_RANDOM */
			addRandomData( randomState, buffer,
						   noAttrs * sizeof( short int ) );
			attrCount += noAttrs / 2;
			}

		/* Get status info for the first NO_PROCESSES processes on this
		   CPU */
		noProcesses = getProcessList( cpuNo, pidBuffer, NO_PROCESSES );
		if( noProcesses <= 0 )
			{
			/* If the process-list read fails for some reason, we can still
			   get info for at least the first two processes, which are
			   always present */
			noProcesses = 2;
			pidBuffer[ 0 ] = 0;
			pidBuffer[ 1 ] = 1;
			}
		for( i = 0; i < noProcesses; i++ )
			{
			short int pin = pidBuffer[ i ];

			error = PROCESS_GETINFOLIST_( cpuNo		/* CPU no.*/
										  ,&pin		/* Process ID */
										  ,			/* Node name */
										  ,			/* Node name length */
										  ,			/* Process handle */
										  ,( short int * ) procAttrList
										  			/* Attrs.to read */
										  ,NO_PROC_ATTRS/* No.attrs.to read */
										  ,( short int * ) buffer
													/* Returned attrs.*/
										  ,RANDOM_BUFSIZE / sizeof( short int )
													/* Ret.attrs.buffer size */
										  ,&noAttrs	/* No.returned attrs.*/
										  );
			if( error == 0 )
				{
#ifdef DEBUG_RANDOM
				printf( "PROCESS_GETINFOLIST returned %d attributes for "
						"process %d.\n", noAttrs, pin );
#endif /* DEBUG_RANDOM */
				addRandomData( randomState, buffer,
							   noAttrs * sizeof( short int ) );
				attrCount += max( noAttrs / 5, 10 );
				}
			}
		}

	/* Flush any remaining data through.  Quality = attrCount */
	endRandomData( randomState, min( attrCount, 100 ) );
	}
