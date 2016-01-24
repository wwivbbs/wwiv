/****************************************************************************
*																			*
*							OS/2 Randomness-Gathering Code					*
*			Copyright Peter Gutmann and Stuart Woolford 1996-2001			*
*							Updated by Mario Korva 1998						*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* General includes */

#include "crypt.h"

#define INCL_DOSMISC
#define INCL_DOSDATETIME
#define INCL_DOSPROCESS
#define INCL_WINWINDOWMGR
#define INCL_WINSYS
#include <os2.h>

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	4096

/* DosQProcStatus() = DOSCALLS.154 */

#if defined(__32BIT__)
  #define PTR(ptr, ofs)  ((void *) ((char *) (ptr) + (ofs)))
#else
  #define DosQueryModuleName DosGetModName
  #define APIENTRY16 APIENTRY
  #if defined(M_I86SM) || defined(M_I86MM)
    #define PTR(ptr, ofs)  ((void *) ((char *) (ptr) + (ofs)))
  #else
    #define PTR(ptr, ofs)  ((void *) ((char *) (((ULONG) procstat & 0xFFFF0000) | (USHORT) (ptr)) + (ofs)))
    /* kludge to transform 0:32 into 16:16 pointer in this special case */
  #endif
#endif

#ifdef __EMX__

USHORT _THUNK_FUNCTION(DosQProcStatus) (PVOID pBuf, USHORT cbBuf);
USHORT _THUNK_FUNCTION(DosGetPrty) (USHORT usScope, PUSHORT pusPriority, USHORT pid);

USHORT _DosQProcStatus(PVOID pBuf, USHORT cbBuf)
{
  _THUNK_PROLOG(4+2);
  _THUNK_FAR16(_emx_32to16(pBuf));       /* PVOID  pBuf  = 4 bytes */
  _THUNK_SHORT(cbBuf);                   /* USHORT cbBuf = 2 bytes */
  return (USHORT) _THUNK_CALL(DosQProcStatus);
}

#define DosQProcStatus _DosQProcStatus

#else

USHORT APIENTRY16 DosQProcStatus(PVOID pBuf, USHORT cbBuf);

#endif

/* DosQProcStatus()
   Query Process Status Info API used by PSTAT system utility.
   To use this undocumented function add following to DEF file:
IMPORTS
  DOSQPROCSTATUS = DOSCALLS.154


   The DosQProcStatus API is a 16 bit API that returns information that
   summarizes the system resources that are in use of an OS/2 2.0 system.
   DosQProcStatus reports on the following classes of OS/2 2.0 system resources:

   Processes and Threads
   Dynamic Link Library Modules
   16 bit System Semaphores
   Named Shared Memory Segments

   DosQProcStatus returns a buffer that is filled with a series of sections
   of resource information:

   1. Pointer section
   2. Global data section
   3. Section with Process and Thread records - one Process record for each process
      immediately followed by a set of Thread records - one Thread record for each
      thread within the process
   4. Section consisting of 16 Bit System Semaphore records
   5. Section consisting of Executable Module records
   6. Section consisting of Shared Memory Segment records

   Definition of system records returned by DosQProcStatus()
   from IBM document entitled:

   DosQProcStatus API for IBM OS/2 Version 2.0
   May 11, 1992 */

typedef struct qsGrec_s {             /* Global record */
        ULONG         cThrds;         /* number of threads in use */
        ULONG         Reserved1;
        ULONG         Reserved2;
}qsGrec_t;

typedef struct qsTrec_s {             /* Thread record */
        ULONG         RecType;        /* Record Type */
                                      /* Thread rectype = 100 */
        USHORT        tid;            /* thread ID */
        USHORT        slot;           /* "unique" thread slot number */
        ULONG         sleepid;        /* sleep id thread is sleeping on */
        ULONG         priority;       /* thread priority */
        ULONG         systime;        /* thread system time */
        ULONG         usertime;       /* thread user time */
        UCHAR         state;          /* thread state */
        UCHAR         padchar;        /* ? */
        USHORT        padshort;       /* ? */
} qsTrec_t;

typedef struct qsPrec_s {             /* Process record */
        ULONG         RecType;        /* type of record being processed */
                                      /* process rectype = 1            */
        qsTrec_t  FAR *pThrdRec;      /* ptr to 1st thread rec for this prc*/
        USHORT        pid;            /* process ID */
        USHORT        ppid;           /* parent process ID */
        ULONG         type;           /* process type */
        ULONG         stat;           /* process status */
        ULONG         sgid;           /* process screen group */
        USHORT        hMte;           /* program module handle for process */
        USHORT        cTCB;           /* # of TCBs in use in process */
        ULONG         Reserved1;
        void    FAR   *Reserved2;
        USHORT        c16Sem;         /*# of 16 bit system sems in use by proc*/
        USHORT        cLib;           /* number of runtime linked libraries */
        USHORT        cShrMem;        /* number of shared memory handles */
        USHORT        Reserved3;
        USHORT  FAR   *p16SemRec;     /*ptr to head of 16 bit sem inf for proc*/
        USHORT  FAR   *pLibRec;       /*ptr to list of runtime lib in use by  */
                                      /*process*/
        USHORT  FAR   *pShrMemRec;    /*ptr to list of shared mem handles in  */
                                      /*use by process*/
        USHORT  FAR   *Reserved4;
} qsPrec_t;

typedef struct qsS16Headrec_s {       /* 16 Bit System Semaphore record */
        ULONG         SRecType;       /* semaphore rectype = 3 */
        ULONG         Reserved1;      /* overlays NextRec of 1st qsS16rec_t*/
        ULONG         Reserved2;
        ULONG         S16TblOff;      /* index of first semaphore,SEE PSTAT OUTPUT*/
                                      /* System Semaphore Information Section     */
} qsS16Headrec_t;

typedef struct qsMrec_s {             /* Shared Memory Segment record */
        ULONG         MemNextRec;     /* offset to next record in buffer */
        USHORT        hmem;           /* handle for shared memory */
        USHORT        sel;            /* shared memory selector */
        USHORT        refcnt;         /* reference count */
        CHAR          Memname;        /* start of shared memory name string */
} qsMrec_t;

typedef struct qsLrec_s {             /* Executable Module record */
        void  FAR       *pNextRec;    /* pointer to next record in buffer */
        USHORT        hmte;           /* handle for this mte */
        USHORT        Reserved1;      /* Reserved */
        ULONG         ctImpMod;       /* # of imported modules in table */
        ULONG         Reserved2;      /* Reserved */
        ULONG         Reserved3;      /* Reserved */
        UCHAR     FAR *pName;         /* ptr to name string following stru */
} qsLrec_t;

typedef struct qsPtrRec_s {             /* Pointer record */
        qsGrec_t        *pGlobalRec;    /* ptr to the global data section   */
        qsPrec_t        *pProcRec;      /* ptr to process record section    */
        qsS16Headrec_t  *p16SemRec;     /* ptr to 16 bit sem section        */
        qsMrec_t        *pShrMemRec;    /* ptr to shared mem section        */
        qsLrec_t        *pLibRec;       /* ptr to exe module record section */
} qsPtrRec_t;

void fastPoll( void )
	{
	static BOOLEAN addedFixedItems = FALSE;
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	DATETIME dt;
	ULONG querybuffer[ 26 + 8 ];
	PTIB ptib = NULL;
	PPIB ppib = NULL;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get various (fairly constant) pieces of machine information and
	   timestamps, date and time, the thread information block and process
	   information block. The DosQuerySysInfo() call returns more or less
	   static information with a single 32-bit counter which is incremented
	   roughly every 31ms */
	DosQuerySysInfo( 1, 26, ( PVOID ) querybuffer, sizeof( querybuffer ) );
	addRandomData( randomState, querybuffer, sizeof( querybuffer ) );
	DosGetDateTime( &dt );
	addRandomData( randomState, &dt, sizeof( DATETIME ) );

	/* Process and Thread information */
	DosGetInfoBlocks( &ptib, &ppib );
	addRandom( randomState, ptib );
	addRandom( randomState, ppib );
	addRandomData( randomState, ptib, sizeof( TIB ) );  /* TIB */
	addRandomData( randomState, ppib, sizeof( PIB ) );  /* PIB */
	addRandomData( randomState, ptib->tib_ptib2, sizeof( TIB2 ) ); /* TIB2 */

	/* The following are fixed for the lifetime of the process so we only
	   add them once */
	if( !addedFixedItems )
		{
		/* Command line and environment strings */
		addRandomData( randomState, ppib->pib_pchcmd,
					   strlen( ppib->pib_pchcmd ) );
		addRandomData( randomState, ppib->pib_pchenv,
					   strlen( ppib->pib_pchenv ) );
		addedFixedItems = TRUE;
		}

	/* Running time information of all system threads */
	{
    #define PTR(ptr, ofs)  ((void *) ((char *) (ptr) + (ofs)))
    qsPtrRec_t *procstat;
    qsPrec_t *proc;
    qsTrec_t *thread;
    USHORT rc,i;
    UCHAR x;

    /* Get information about system and user time of all threads.
       This information is changing all the time.  */

    procstat = malloc (0x8000); /* Large buffer for process information */
    rc = DosQProcStatus ( (PVOID) procstat, 0x8000);    /* Query process info */
    if (!rc) {
        addRandom( randomState, procstat );
        addRandomData( randomState, procstat, sizeof( qsPtrRec_t ) );
    }
    for ( proc = PTR( procstat->pProcRec, 0 );
          proc -> RecType == 1;
          proc = PTR( proc->pThrdRec, proc->cTCB * sizeof( qsTrec_t ) ) ) {
        for ( i = 0, thread = PTR( proc->pThrdRec, 0 );
              i < proc->cTCB;
              i++, thread++ ) {
            /* use low byte of (systime+usertime) */
            x = (thread->systime + thread->usertime) & 0xFF;
            /* but only if nonzero */
            if( x )
              addRandom( randomState, x );
        }
    }
    free (procstat);
	}

	/* Flush any remaining data through */
	endRandomData( randomState, 10 );
	}

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	HWND hwndParent;
	HWND hwndNext;
	HENUM henum;
	PID pib;
	TID tib;
	RECTL rcl;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Enumerate all PM windows */
	hwndParent = HWND_DESKTOP;
	henum = WinBeginEnumWindows( hwndParent );
	while( ( hwndNext = WinGetNextWindow( henum ) ) != NULLHANDLE )
		{
		addRandom( randomState, hwndNext );

		WinQueryWindowProcess( hwndNext, &pib, &tib );
		addRandomData( randomState, &tib, sizeof( TIB ) );
		addRandomData( randomState, &pib, sizeof( PIB ) );
		WinQueryWindowRect( hwndNext, &rcl );
		addRandomData( randomState, &rcl, sizeof( RECTL ) );
		}
	WinEndEnumWindows( henum );

	/* Flush any remaining data through */
	endRandomData( randomState, 50 );
	}

/* Get current thread ID */

ULONG DosGetThreadID( void )
	{
	PTIB ptib = NULL;
	PPIB ppib = NULL;

	/* Process and Thread information */
	DosGetInfoBlocks( &ptib, &ppib );
	return ppib->pib_ulpid;
	}
