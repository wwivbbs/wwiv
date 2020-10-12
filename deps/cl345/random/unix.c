/****************************************************************************
*																			*
*						  Unix Randomness-Gathering Code					*
*						Copyright Peter Gutmann 1996-2018					*
*	Contributions to slow-poll code by Paul Kendall and Chris Wedgwood		*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* Define the following to a nonzero value to print diagnostic information 
   on where randomness is coming from */

#ifndef NDEBUG
  #define debugRandom		0
#endif /* NDEBUG */

/* General includes */

#if defined( __GNUC__ )
  #define _GNU_SOURCE
#endif /* Needed for newer functions like setresuid() */
#include <stdio.h>
#include <time.h>
#include "crypt.h"
#include "random/random.h"

/* Unix and Unix-like systems share the same makefile, make sure that the
   user isn't trying to use the Unix randomness code under a non-Unix (but
   Unix-like) system.  This would be pretty unlikely since the makefile
   automatically adjusts itself based on the environment it's running in,
   but we use the following safety check just in case.  We have to perform
   this check before we try any includes because Unix and the Unix-like
   systems don't have the same header files */

#ifdef __BEOS__
  #error For the BeOS build you need to edit $MISCOBJS in the makefile to use 'beos' and not 'unix'
#endif /* BeOS has its own randomness-gathering file */
#ifdef __ECOS__
  #error For the eCOS build you need to edit $MISCOBJS in the makefile to use 'ecos' and not 'unix'
#endif /* eCOS has its own randomness-gathering file */
#ifdef __ITRON__
  #error For the uITRON build you need to edit $MISCOBJS in the makefile to use 'itron' and not 'unix'
#endif /* uITRON has its own randomness-gathering file */
#ifdef __PALMSOURCE__
  #error For the PalmOS build you need to edit $MISCOBJS in the makefile to use 'palmos' and not 'unix'
#endif /* PalmOS has its own randomness-gathering file */
#ifdef __RTEMS__
  #error For the RTEMS build you need to edit $MISCOBJS in the makefile to use 'rtems' and not 'unix'
#endif /* RTEMS has its own randomness-gathering file */
#if defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ )
  #error For the Tandem build you need to edit $MISCOBJS in the makefile to use 'tandem' and not 'unix'
#endif /* Tandem OSS has its own randomness-gathering file */
#if defined( __VxWorks__ )
  #error For the VxWorks build you need to edit $MISCOBJS in the makefile to use 'vxworks' and not 'unix'
#endif /* VxWorks has its own randomness-gathering file */
#if defined( __XMK__ )
  #error For the Xilinx XMK build you need to edit $MISCOBJS in the makefile to use 'xmk' and not 'unix'
#endif /* XMK has its own randomness-gathering file */

/* Some systems built on top of Unix kernels and/or Unix-like systems don't 
   support certain functionality like the SYSV shared memory that's used for 
   last-line-of-defence entropy polling, or only support it in an erratic 
   manner that we can't rely on.  For these systems we replace the shared-
   memory-based polling with a stub the prints an error message for the
   caller */

#if defined( __Android__ ) || ( defined( __QNX__ ) && OSVERSION <= 4 )
  #define NO_SYSV_SHAREDMEM
#endif /* Android || QNX <= 4.x */

/* OS-specific includes */

#if defined( __hpux ) && ( OSVERSION >= 10 )
  #define _XOPEN_SOURCE_EXTENDED
#endif /* Workaround for inconsistent PHUX networking headers */
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#if !( defined( __QNX__ ) || defined( __MVS__ ) )
  #include <sys/errno.h>
  #include <sys/ipc.h>
#endif /* !( QNX || MVS ) */
#include <sys/time.h>		/* SCO and SunOS need this before resource.h */
#ifndef __QNX__
  #if defined( _MPRAS ) && !( defined( _XOPEN_SOURCE ) && \
	  defined( __XOPEN_SOURCE_EXTENDED ) )
	/* On MP-RAS 3.02, the X/Open test macros must be set to include
	   getrusage(). */
	#define _XOPEN_SOURCE			1
	#define _XOPEN_SOURCE_EXTENDED	1
	#define MPRAS_XOPEN_DEFINES
  #endif /* MP-RAS */
  #include <sys/resource.h>
  #if defined( MPRAS_XOPEN_DEFINES )
	#undef _XOPEN_SOURCE
	#undef _XOPEN_SOURCE_EXTENDED
	#undef MPRAS_XOPEN_DEFINES
  #endif /* MP-RAS */
#endif /* QNX */
#ifdef __linux__ 
  #include <linux/random.h>
#endif /* Linux */
#ifdef __iOS__ 
  #include <Security/SecRandom.h>
#endif /* __iOS__  */
#if defined( _AIX ) || defined( __QNX__ )
  #include <sys/select.h>
#endif /* Aches || QNX */
#ifdef _AIX
  #include <sys/systemcfg.h>
#endif /* Aches */
#ifdef __CYGWIN__
  #include <signal.h>
  #include <sys/ipc.h>
  #include <sys/sem.h>
  #include <sys/shm.h>
#endif /* CYGWIN */
#if !( defined( __Android__ ) || defined( __CYGWIN__ ) || defined( __QNX__ ) )
  #include <sys/shm.h>
#endif /* !( __Android__  || Cygwin || QNX ) */
#if defined( __linux__ ) && ( defined(__i386__) || defined(__x86_64__) )
  #include <sys/timex.h>	/* For rdtsc() */
#endif /* Linux on x86 */
#if defined( __FreeBSD__ ) || defined( __NetBSD__ ) || \
	defined( __OpenBSD__ ) || defined( __APPLE__ )
  /* Get BSD version macros.  This is less useful, or at least portable, 
     than it seems because the recommended 'BSD' macro is frozen in time in 
	 the 1990s, and so each BSD variant has defined its own replacement, or
	 in some cases several for good measures.  FreeBSD has 
	 '__FreeBSD_version' as a 7-digit decimal with the MSB being the major
	 version, NetBSD has 'NetBSD' but this is frozen in 1999, the current 
	 form is '__NetBSD_Version__' as an 8-digit decimal with the MSB being 
	 the major version, OpenBSD has 'OpenBSD' in the BSD-standard YYYYMM 
	 format, and Apple has a 'NeXTBSD' frozen in 1995 but otherwise only
	 '__APPLE_CC__' for the compiler version, otherwise all we have is
	 <TargetConditionals.h> to tell us whether we're building for OS X vs.
	 iOS (TARGET_OS_*) and the CPU type (TARGET_CPU_*) */
  #include <sys/param.h>
#endif /* *BSDs */
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>		/* Verschiedene komische Typen */
#include <sys/un.h>
#include <sys/wait.h>
/* #include <kitchensink.h> */

/* Crays and QNX 4.x don't have a rlimit/rusage so we have to fake the
   functions and structs */

#if defined( _CRAY ) || \
	( defined( __QNX__ ) && OSVERSION <= 4 )
  #define setrlimit( x, y )
  struct rlimit { int dummy1, dummy2; };
  struct rusage { int dummy; };
#endif /* Systems without rlimit/rusage */

/* If we're using threads, we have to protect the entropy gatherer with
   a mutex to prevent multiple threads from trying to initate polls at the
   same time.  Unlike the kernel mutexes, we don't have to worry about
   being called recursively, so there's no need for special-case handling
   for Posix' nasty non-reentrant mutexes */

#ifdef USE_THREADS
  #include <pthread.h>

  #define lockPollingMutex()	pthread_mutex_lock( &gathererMutex )
  #define unlockPollingMutex()	pthread_mutex_unlock( &gathererMutex )
#else
  #define lockPollingMutex()
  #define unlockPollingMutex()
#endif /* USE_THREADS */

/* glibc, in line with its general policy of insanity, caches the pid so
   that changes aren't visible (see 
   http://yarchive.net/comp/linux/getpid_caching.html).  Because of this, 
   we have to bypass the braindamage by invoking the pid syscall directly 
   in order to get the actual pid */

#if defined( __GNUC__ ) && defined( SYS_getpid )
	#define getpid()	( ( pid_t ) syscall( SYS_getpid ) )
#endif /* glibc braindamage */

/* The size of the intermediate buffer used to accumulate polled data,
   and an extra-large version for calls that return lots of data */

#define RANDOM_BUFSIZE			4096
#define BIG_RANDOM_BUFSIZE		( RANDOM_BUFSIZE * 2 )

#if BIG_RANDOM_BUFSIZE >= MAX_INTLENGTH_SHORT
  #error RANDOM_BUFSIZE exceeds randomness accumulator size
#endif /* RANDOM_BUFSIZE >= MAX_INTLENGTH_SHORT */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#if defined( __hpux ) && ( OSVERSION == 9 )

/* PHUX 9.x doesn't support getrusage in libc (wonderful...) */

#include <syscall.h>

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int getrusage( int who, OUT struct rusage *rusage )
	{
	return( syscall( SYS_getrusage, who, rusage ) );
	}
#endif /* __hpux */

/****************************************************************************
*																			*
*									Fast Poll								*
*																			*
****************************************************************************/

/* Fast poll - not terribly useful.  SCO has a gettimeofday() prototype but
   no actual system call that implements it, and no getrusage() at all, so
   we use times() instead */

#if defined( _CRAY ) || defined( _M_XENIX )
  #include <sys/times.h>
#endif /* Systems without getrusage() */

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
#if !( defined( _CRAY ) || defined( _M_XENIX ) )
	struct timeval tv;
  #if !( defined( __QNX__ ) && OSVERSION <= 4 )
	struct rusage rusage;
  #endif /* !QNX 4.x */
#else
	struct tms tms;
#endif /* Systems without getrusage() */
#ifdef _AIX
	timebasestruct_t cpuClockInfo;
#endif /* Aches */
#if ( defined( sun ) && ( OSVERSION >= 5 ) )
	hrtime_t hrTime;
#endif /* Slowaris */
#if defined( __clang__ ) && \
	!( defined( __arm ) || defined( __arm__ ) || \
	   defined( __aarch64__ ) || defined( __arm64 ) )
  #if __has_builtin( __builtin_readcyclecounter )
	unsigned long long cycleCounterValue;
  #endif /* Built-in function support */
#endif /* __clang__ with readcyclecounter() */
#if ( defined( __QNX__ ) && OSVERSION >= 5 )
	uint64_t clockCycles;
#endif /* QNX */
#if defined( _POSIX_TIMERS ) && ( _POSIX_TIMERS > 0 ) && 0	/* See below */
	struct timespec timeSpec;
#endif /* _POSIX_TIMERS > 0 */
	int status;

	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	ENSURES_V( cryptStatusOK( status ) );

	/* Mix in the process ID.  This doesn't change per process but will
	   change if the process forks, ensuring that the child data differs 
	   from the parent */
	addRandomValue( randomState, getpid() );

#if !( defined( _CRAY ) || defined( _M_XENIX ) )
	gettimeofday( &tv, NULL );
	addRandomValue( randomState, tv.tv_sec );
	addRandomValue( randomState, tv.tv_usec );

	/* SunOS 5.4 has the function call but no prototypes for it, if you're
	   compiling this under 5.4 you'll have to copy the header files from 5.5
	   or something similar */
  #if !( defined( __QNX__ ) && OSVERSION <= 4 )
	getrusage( RUSAGE_SELF, &rusage );
	addRandomData( randomState, &rusage, sizeof( struct rusage ) );
  #endif /* !QNX 4.x */
#else
	/* Merely a subset of getrusage(), but it's the best that we can do.  On
	   Crays it provides access to the hardware clock, so the data is quite
	   good */
	times( &tms );
	addRandomData( randomState, &tms, sizeof( struct tms ) );
#endif /* Systems without getrusage() */
#ifdef _AIX
	/* Add the value of the nanosecond-level CPU clock or time base register */
	read_real_time( &cpuClockInfo, sizeof( timebasestruct_t ) );
	addRandomData( randomState, &cpuClockInfo, sizeof( timebasestruct_t ) );
#endif /* _AIX */
#if ( defined( sun ) && ( OSVERSION >= 5 ) )
	/* Read the Sparc %tick register (equivalent to the P5 TSC).  This is
	   only readable by the kernel by default, although Solaris 8 and newer
	   make it readable in user-space.  To do this portably, we use
	   gethrtime(), which does the same thing */
	hrTime = gethrtime();
	addRandomData( randomState, &hrTime, sizeof( hrtime_t ) );
#endif /* Slowaris */
#ifdef rdtscl
	if( getSysCaps() & SYSCAP_FLAG_RDTSC )
		{
		unsigned long tscValue;

		rdtscl( tscValue );
		addRandomValue( randomState, tscValue );
		}
#endif /* rdtsc available */
#if defined( __clang__ ) && \
	!( defined( __arm ) || defined( __arm__ ) || \
	   defined( __aarch64__ ) || defined( __arm64 ) )
  /* ARM systems have a cycle-counter that's accessed via 
     "mrs reg, PMCCNTR_EL0", but access from user-space to this, and the
	 Performance Monitoring Unit (PMU) that it's part of, is usually 
	 disabled by default.  What this means is that the compiler will emit 
	 an instruction that faults when executed, see e.g.
	 http://neocontra.blogspot.com/2013/05/user-mode-performance-counters-for.html
	 http://zhiyisun.github.io/2016/03/02/How-to-Use-Performance-Monitor-Unit-(PMU)-of-64-bit-ARMv8-A-in-Linux.html  
	 Because it's unlikely to work in practice, we disable it for ARM
	 systems even if the compiler would otherwise give us the intrinsic 
	 needed to access it */
  #if __has_builtin( __builtin_readcyclecounter )
	cycleCounterValue = __builtin_readcyclecounter();

	addRandomData( randomState, &cycleCounterValue, sizeof( long long ) );
  #endif /* Built-in function support */
#endif /* __clang__ */
#if ( defined( __QNX__ ) && OSVERSION >= 5 )
	/* Return the output of RDTSC or its equivalent on other systems.  We
	   don't worry about locking the thread to a CPU since we're not using
	   it for timing (in fact being run on another CPU helps the entropy) */
	clockCycles = ClockCycles();
	addRandomData( randomState, &clockCycles, sizeof( uint64_t ) );
#endif /* QNX */
	/* Get real-time (or as close to it as possible) clock information if 
	   it's available.  These values are platform-specific, but typically use 
	   a high-resolution source like the Pentium TSC.
	   
	   (Unfortunately we can't safely use this function because it's often 
	   not present in libc but needs to be pulled in via the real-time 
	   extensions library librt instead, which causes all sorts of 
	   portability problems) */
#if defined( _POSIX_TIMERS ) && ( _POSIX_TIMERS > 0 ) && 0
	if( clock_gettime( CLOCK_REALTIME, &timeSpec ) == 0 )
		addRandomData( randomState, &timeSpec, sizeof( struct timespec ) );
	if( clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &timeSpec ) == 0 )
		addRandomData( randomState, &timeSpec, sizeof( struct timespec ) );
	if( clock_gettime( CLOCK_THREAD_CPUTIME_ID, &timeSpec ) == 0 )
		addRandomData( randomState, &timeSpec, sizeof( struct timespec ) );
#endif /* _POSIX_TIMERS > 0 */

	/* Flush any remaining data through */
	endRandomData( randomState, 1 );
	}

/****************************************************************************
*																			*
*						System-specific Slow Poll Sources					*
*																			*
****************************************************************************/

/* *BSD-specific poll using sysctl */

#if defined( __FreeBSD__ ) || defined( __NetBSD__ ) || \
	defined( __OpenBSD__ ) || defined( __APPLE__ )

#define USE_SYSCTL
#include <sys/sysctl.h>
#include <netinet/in.h>				/* For CTL_NET identifiers */
#if defined( __APPLE__ )
  /* The iPhone native SDK (but not the emulator) gets the paths for some of
	 the include files wrong, so we have to use absolute paths.  To make 
	 things even crazier, starting with XCode 10 Apple decided that 
	 /usr/include wasn't where include files should go (see
	 https://apple.stackexchange.com/questions/337940/why-is-usr-include-missing-i-have-xcode-and-command-line-tools-installed-moja)
	 so we have to hardcode bizarro SDK paths for newer versions.  This is
	 made even more complicated by the fact that there's no compile-time 
	 mechanism for checking which version of XCode is being used, so we have
	 to use the badly-misnamed __IPHONE_OS_VERSION_MAX_ALLOWED define for 
	 the purpose */
  #if TARGET_OS_IPHONE || TARGET_OS_WATCH
	#if ( __IPHONE_OS_VERSION_MAX_ALLOWED >= 100000 )
	  #include "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/net/route.h"
	  #include "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/sys/gmon.h"
	#else
	  #include "/usr/include/net/route.h"
	  #include "/usr/include/sys/gmon.h"
	#endif /* Apple insanity */
  #else
	#include <net/route.h>			/* For CTL_NET:AF_ROUTE:0:AF_INET:\
									   NET_RT_FLAGS idents */
	#include <sys/gmon.h>			/* For CTL_KERN:KERN_PROF identifiers */
  #endif /* Native iOS SDK vs. everything else */
#else
  #include <net/route.h>			/* For CTL_NET:AF_ROUTE:0:AF_INET:\
									   NET_RT_FLAGS idents */
  #include <sys/gmon.h>				/* For CTL_KERN:KERN_PROF identifiers */
  #if defined( __NetBSD__ )
	#include <uvm/uvm_param.h>		/* For CTL_VM identifiers */
  #else
	#include <vm/vm_param.h>		/* For CTL_VM identifiers */
  #endif /* BSD-variant-specific include paths */
#endif /* OS-specific include paths */

typedef struct {
	const int mibCount;				/* Number of MIB entries */
	const int mib[ 6 ];				/* MIB values for this info */
	const int quality;				/* Entropy quality if present */
	} SYSCTL_INFO;

static const SYSCTL_INFO sysctlInfo[] = {
	/* Hardware info */
	{ 2, { CTL_HW, HW_MACHINE } },	/* Machine class */
	{ 2, { CTL_HW, HW_MACHINE_ARCH } }, /* Machine architecture */
	{ 2, { CTL_HW, HW_MODEL } },	/* Machine model */
#ifdef HW_IOSTATS
	{ 2, { CTL_HW, HW_IOSTATS } },	/* struct io_sysctl for each device 
									   containing microsecond times and byte 
									   counts */
#endif /* HW_IOSTATS */
	{ 2, { CTL_HW, HW_PHYSMEM } },	/* Physical memory */
#ifdef HW_REALMEM
	{ 2, { CTL_HW, HW_REALMEM } },	/* Real memory */
#endif /* HW_REALMEM */
	{ 2, { CTL_HW, HW_USERMEM } },	/* Non-kernel memory */
#ifdef HW_DISKSTATS
	{ 2, { CTL_HW, HW_DISKSTATS } },/* struct diskstats for each disk,
									   structure format undocumented */
#endif /* HW_DISKSTATS */

	/* Kernel info */
	{ 2, { CTL_KERN, KERN_BOOTTIME }, 1 }, 
									/* Time system was booted */
	{ 2, { CTL_KERN, KERN_CLOCKRATE } },
									/* struct clockinfo of clock frequecies */
#ifdef KERN_CP_TIME
	{ 2, { CTL_KERN, KERN_CP_TIME }, 1 },
									/* Number of clock ticks in different CPU
									   states */
#endif /* KERN_CP_TIME */
#ifdef KERN_DRIVERS
	{ 2, { CTL_KERN, KERN_DRIVERS } },/* struct kinfo_drivers with major/minor
									   ID and name for all drivers */
#endif /* KERN_DRIVERS */
#ifdef KERN_EVCNT
	{ 2, { CTL_KERN, KERN_EVCNT } },/* struct evcnts for all active event 
									   counters.  There usually won't be any
									   set so this won't produce a result from
									   sysctl() */
#endif /* KERN_EVCNT */
	{ 2, { CTL_KERN, KERN_FILE }, 5 }, 
									/* struct xfile for each file in the system, 
									   containing process IDs, fd no., ref.count, 
									   file offset, vnode, and flags.  Produces a
									   huge amount of output so typically gets
									   truncated at SYSCTL_BUFFER_SIZE */
#ifdef KERN_HARDCLOCK_TICKS
	{ 2, { CTL_KERN, KERN_HARDCLOCK_TICKS } },
									/* Number of hardclock (hard real-time timer) 
									   ticks */
#endif /* KERN_HARDCLOCK_TICKS */
	{ 2, { CTL_KERN, KERN_HOSTID } }, /* Host ID */
	{ 2, { CTL_KERN, KERN_HOSTNAME } },	/* Hostname */
#ifdef KERN_HOSTUUID
	{ 2, { CTL_KERN, KERN_HOSTUUID } },	/* Host UUID */
#endif /* KERN_HOSTUUID */
#ifdef KERN_NTPTIME
	{ 2, { CTL_KERN, KERN_NTPTIME }, 1 },
									/* struct ntptimeval with NTP accuracy
									   information */
	{ 2, { CTL_KERN, KERN_TIMEX }, 3 }, 
									/* struct timex containing detailed NTP
									   statistics.  This is a useful source
									   but typically isn't available so won't 
									   produce a result from sysctl() */
#endif /* KERN_NTPTIME */
#ifdef KERN_OSRELDATE
	{ 2, { CTL_KERN, KERN_OSRELDATE } }, /* OS release version */
#endif /* KERN_OSRELDATE */
	{ 2, { CTL_KERN, KERN_OSRELEASE } }, /* OS release string */
	{ 2, { CTL_KERN, KERN_OSREV } }, /* System revision string */
	{ 2, { CTL_KERN, KERN_OSTYPE } }, /* System type string */
	{ 3, { CTL_KERN, KERN_PROC, KERN_PROC_ALL }, 20, }, 
									/* struct kinfo_proc for each process 
									   containing a struct proc and a struct 
									   eproc which contain everything you 
									   could want to know about a process,
									   see /sys/sys/proc.h and 
									   /sys/sys/kinfo_proc.h.  Produces a
									   huge amount of output so typically 
									   gets truncated at SYSCTL_BUFFER_SIZE */
#ifdef KERN_PROC2
	{ 6, { CTL_KERN, KERN_PROC2, KERN_PROC_ALL, 0, sizeof( struct kinfo_proc2 ), 20 }, 10, }, 
									/* struct kinfo_proc2 containing a 
									   superset of struct kinfo_proc.  We 
									   give this a lower weight than it 
									   actually contains because a of the 
									   contents have already been obtained
									   via KERN_PROC.  Produces a huge 
									   amount of output so typically gets
									   truncated at SYSCTL_BUFFER_SIZE */
#endif /* KERN_PROC2 */
	{ 3, { CTL_KERN, KERN_PROF, GPROF_COUNT }, 10 },
									/* If kernel is compiled for profiling, 
									   an array of statistical program 
									   counter counts.  This typically isn't
									   enabled so won't produce a result from
									   sysctl() */
#ifdef KERN_TKSTAT
	{ 3, { CTL_KERN, KERN_TKSTAT, KERN_TKSTAT_CANCC } },
	{ 3, { CTL_KERN, KERN_TKSTAT, KERN_TKSTAT_NIN } },
	{ 3, { CTL_KERN, KERN_TKSTAT, KERN_TKSTAT_NOUT } },
	{ 3, { CTL_KERN, KERN_TKSTAT, KERN_TKSTAT_RAWCC } },
									/* Terminal chars sent/received */
#endif /* KERN_TKSTAT */
	{ 2, { CTL_KERN, KERN_VERSION } }, /* System version string  */
	{ 2, { CTL_KERN, KERN_VNODE }, 15 }, 
									/* struct xvnode for each vnode, see 
									   /sys/sys/vnode.h.  Produces a huge 
									   amount of output so typically gets
									   truncated at SYSCTL_BUFFER_SIZE */

	/* Networking info */
	{ 6, { CTL_NET, AF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0 }, 5 },
									/* IPv4 routing table */
	{ 6, { CTL_NET, AF_ROUTE, 0, AF_INET6, NET_RT_DUMP, 0 }, 5 },
									/* IPv6 routing table */
	{ 6, { CTL_NET, AF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO }, 5 },
									/* ARP cache */

	/* VM info */
#ifdef VM_LOADAVG 
	{ 2, { CTL_VM, VM_LOADAVG } },	/* Load average */
#endif /* VM_LOADAVG */
#ifdef VM_TOTAL
	{ 2, { CTL_VM, VM_TOTAL }, 5 },	/* struct vmtotal with count of jobs in
									   various states and amounts of virtual
									   memory */
#endif /* VM_TOTAL */
#ifdef VM_METER
	{ 2, { CTL_VM, VM_METER }, 5 },	/* Another name for VM_TOTAL */
#endif /* VM_METER */
#ifdef VM_UVMEXP
	{ 2, { CTL_VM, VM_UVMEXP }, 10 },	
									/* struct uvmexp containing global state 
									   of the UVM system */
#endif /* VM_UVMEXP */
	{ 0, { 0 } }, { 0, { 0 } }
	};

#define SYSCTL_BUFFER_SIZE			16384

CHECK_RETVAL \
static int getSysctlData( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ BIG_RANDOM_BUFSIZE + 8 ];
	BYTE sysctlBuffer[ SYSCTL_BUFFER_SIZE + 8 ];
	int quality = 0, i, status;

	status = initRandomData( randomState, buffer, BIG_RANDOM_BUFSIZE );
	ENSURES_EXT( cryptStatusOK( status ), 0 );

	/* Get each set of sysctl() information.  Since some of the information 
	   returned can be rather lengthy, we optionally send it directly to the 
	   randomness pool rather than using the accumulator */
	for( i = 0; sysctlInfo[ i ].mibCount != 0; i++ )
		{
		size_t size = SYSCTL_BUFFER_SIZE;

		/* Since we only care about the information that's returned as an 
		   entropy source, we treat a buffer-not-large-enough error (errno
		   = ENOMEM) as an OK status for the purpose of providing entropy.  
		   Since ENOMEM can be returned for reasons other than the buffer 
		   not being large enough, we sanity-check the result by requiring
		   that at least half of the buffer was filled with information */
		errno = 0;
 		status = sysctl( ( int * ) sysctlInfo[ i ].mib, 
						 sysctlInfo[ i ].mibCount, 
						 sysctlBuffer, &size, NULL, 0 );
		if( status == -1 && errno == ENOMEM )
			{
			DEBUG_DIAG(( "Overflow in sysctl %d:%d, using %d bytes", 
						 sysctlInfo[ i ].mib[ 0 ], sysctlInfo[ i ].mib[ 1 ], 
						 size ));
			if( size >= SYSCTL_BUFFER_SIZE / 2 )
				status = 0;
			}
		if( status )
			continue;
		DEBUG_PRINT_COND( debugRandom,
						  ( "Got %d bytes data from sysctl %d:%d.\n", 
							size, sysctlInfo[ i ].mib[ 0 ], 
							sysctlInfo[ i ].mib[ 1 ] ));

		/* Some sysctl() calls can successfully return zero bytes, so we 
		   check for this special case and continue */
		if( size <= 0 )
			continue;

		/* We got some data, add it to the entropy pool */
		if( size > BIG_RANDOM_BUFSIZE )
			{
			MESSAGE_DATA msgData;

			setMessageData( &msgData, sysctlBuffer, size );
			krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
							 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
			}
		else
			addRandomData( randomState, sysctlBuffer, size );
		quality += sysctlInfo[ i ].quality;
		}

	/* Flush any remaining data through and provide an estimate of its
	   value.  We cap it at 80 to ensure that some data is still coming 
	   from other sources */
	if( quality > 80 )
		quality = 80;
	endRandomData( randomState, quality );

	DEBUG_PRINT_COND( debugRandom,
					  ( "sysctl contributed %d value.\n", quality ));

	return( quality );
	}
#endif /* *BSDs */

/* BSD/Linux-specific polling using getifaddrs() */

#if defined( __FreeBSD__ ) || defined( __NetBSD__ ) || \
	defined( __OpenBSD__ ) || defined( __APPLE__ ) || \
	defined( __linux__  )

#define USE_GETIFADDRS
#include <ifaddrs.h>
#include <netinet/in.h>
#ifdef __linux__
  #include <linux/if_link.h>
#endif /* __linux__ */

CHECK_RETVAL \
static int getIfaddrsData( void )
	{
	RANDOM_STATE randomState;
	struct ifaddrs *ifAddr, *ifAddrPtr;
	BYTE buffer[ BIG_RANDOM_BUFSIZE + 8 ];
	int quality = 0, status;

	status = initRandomData( randomState, buffer, BIG_RANDOM_BUFSIZE );
	ENSURES_EXT( cryptStatusOK( status ), 0 );

	/* Walk through all interfaces getting statistics for them.  The only
	   really interesting one is AF_PACKET, which gives us packet 
	   statistics */
	if( getifaddrs( &ifAddr ) < 0 )
		return( 0 );	
	for( ifAddrPtr = ifAddr; ifAddrPtr != NULL; 
		 ifAddrPtr = ifAddrPtr->ifa_next )
		{
		void *infoPtr = NULL;
		int infoSize = 0;
		
		if( ifAddrPtr->ifa_addr == NULL )
			continue;

		switch( ifAddrPtr->ifa_addr->sa_family )
			{
			case AF_INET:
				infoPtr = ifAddrPtr->ifa_addr;
				infoSize = sizeof( struct sockaddr_in );
				break;
				
			case AF_INET6:
				infoPtr = ifAddrPtr->ifa_addr;
				infoSize = sizeof( struct sockaddr_in6 );
				break;

#ifdef AF_PACKET
			case AF_PACKET:
				if( ifAddrPtr->ifa_data == NULL )
					break;
				infoPtr = ifAddrPtr->ifa_data;
				infoSize = sizeof( struct rtnl_link_stats );
				quality += 2;
				break;
#endif /* AF_PACKET */
			
			default:
				break;
			}
		if( infoPtr == NULL )
			continue;
		addRandomData( randomState, infoPtr, infoSize );
		}
	freeifaddrs( ifAddr );

	/* Flush any remaining data through and provide an estimate of its
	   value.  We cap it at 80 to ensure that some data is still coming 
	   from other sources */
	if( quality > 15 )
		quality = 15;
	endRandomData( randomState, quality );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "getifaddrs contributed %d entropy quality.\n", 
					    quality ));

	return( quality );
	}
#endif /* *BSDs and Linux */

/* BSD-specific polling using getfsstat() */

#if defined( __FreeBSD__ ) || defined( __NetBSD__ ) || \
	defined( __OpenBSD__ ) || defined( __APPLE__ )

#define USE_GETFSSTAT
#include <sys/mount.h>

#define FSSTAT_BUFFER_SIZE		16384

CHECK_RETVAL \
static int getFsstatData( void )
	{
	RANDOM_STATE randomState;
	struct ifaddrs *ifAddr, *ifAddrPtr;
	BYTE buffer[ BIG_RANDOM_BUFSIZE + 8 ];
	BYTE fsstatBuffer[ FSSTAT_BUFFER_SIZE + 8 ];
	int quality = 0, count, status;

	status = initRandomData( randomState, buffer, BIG_RANDOM_BUFSIZE );
	ENSURES_EXT( cryptStatusOK( status ), 0 );

	/* Get statistics for all mounted filesystems, both standard static 
	   information and dynamic data like free space, free file inodes, 
	   and read/write statistics */
	count = getfsstat( NULL, 0, MNT_NOWAIT );
	if( count <= 0 || count * sizeof( struct statfs ) > FSSTAT_BUFFER_SIZE )
		return( 0 );
	count = getfsstat( ( struct statfs * ) fsstatBuffer, FSSTAT_BUFFER_SIZE, 
					   MNT_NOWAIT );
	if( count > 0 )
		{
		addRandomData( randomState, fsstatBuffer, 
					   count * sizeof( struct statfs ) );
		quality = 3;
		}

	endRandomData( randomState, quality );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "getfsstat contributed %d value.\n", quality ));

	return( quality );
	}
#endif /* *BSDs */

/* Slowaris-specific slow poll using kstat, which provides kernel statistics.
   Since there can be a hundred or more of these, we use a larger-than-usual
   intermediate buffer to cut down on kernel traffic.
   
   Unfortunately Slowaris is the only OS that provides this interface, some
   of the *BSDs have kenv, but this just returns fixed information like PCI
   bus device addresses and so on, and isn't useful for providing entropy */

#if ( defined( sun ) && ( OSVERSION >= 5 ) )

#define USE_KSTAT
#include <kstat.h>

CHECK_RETVAL \
static int getKstatData( void )
	{
	kstat_ctl_t *kc;
	kstat_t *ksp;
	RANDOM_STATE randomState;
	BYTE buffer[ BIG_RANDOM_BUFSIZE + 8 ];
	int noEntries = 0, quality, status;

	/* Try and open a kernel stats handle */
	if( ( kc = kstat_open() ) == NULL )
		return( 0 );

	status = initRandomData( randomState, buffer, BIG_RANDOM_BUFSIZE );
	ENSURES_EXT( cryptStatusOK( status ), 0 );

	/* Walk down the chain of stats reading each one.  Since some of the
	   stats can be rather lengthy, we optionally send them directly to
	   the randomness pool rather than using the accumulator, with a safe
	   bound on the data size to make sure we're not trying to inject
	   arbitrary-length data into the pool */
	for( ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next )
		{
		if( kstat_read( kc, ksp, NULL ) == -1 || \
			ksp->ks_data_size <= 0 )
			continue;
		addRandomData( randomState, ksp, sizeof( kstat_t ) );
		if( ksp->ks_data_size > BIG_RANDOM_BUFSIZE )
			{
			MESSAGE_DATA msgData;

			setMessageData( &msgData, ksp->ks_data, 
							min( ksp->ks_data_size, MAX_INTLENGTH_SHORT - 1 ) );
			krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
							 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
			}
		else
			addRandomData( randomState, ksp->ks_data, ksp->ks_data_size );
		noEntries++;
		}
	kstat_close( kc );

	/* Flush any remaining data through and produce an estimate of its
	   value.  We require that we get at least 50 entries and give them a
	   maximum value of 35 to ensure that some data is still coming from
	   other sources */
	quality = ( noEntries > 50 ) ? 35 : 0;
	endRandomData( randomState, quality );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "kstat contributed %d value.\n", quality ));

	return( quality );
	}
#endif /* Slowaris */

/* SYSV /proc interface, which provides assorted information that usually
   has to be obtained the hard way via a slow poll.

   Note that getProcData() gets data from the legacy /proc pseudo-filesystem
   using ioctls() whereas getProcFSdata() gets data from the current /procfs
   filesystem using file reads.  Because of this we don't enable it for
   systems like Linux even though it's available since we both get far better
   data from procfs and because we want to avoid double-counting the entropy
   that we're getting */

#if ( defined( sun ) && ( OSVERSION >= 5 ) ) || defined( __osf__ ) || \
	  defined( __alpha__ )

#define USE_PROC
#include <sys/procfs.h>

CHECK_RETVAL \
static int getProcData( void )
	{
#ifdef PIOCSTATUS
	prstatus_t prStatus;
#endif /* PIOCSTATUS */
#ifdef PIOCPSINFO
	prpsinfo_t prMisc;
#endif /* PIOCPSINFO */
#ifdef PIOCUSAGE
	prusage_t prUsage;
#endif /* PIOCUSAGE */
#ifdef PIOCACINFO
	struct pracinfo pracInfo;
#endif /* PIOCACINFO */
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	char fileName[ 128 + 8 ];
	int fd, noEntries = 0, quality, status;

	/* Try and open the process info for this process.  We don't use 
	   O_NOFOLLOW because on some Unixen special files can be symlinks and 
	   in any case a system that allows attackers to mess with privileged 
	   filesystems like this is presumably a goner anyway */
	sprintf_s( fileName, 128, "/proc/%d", getpid() );
	if( ( fd = open( fileName, O_RDONLY ) ) == -1 )
		return( 0 );
	if( fd <= 2 )
		{
		/* We've been given a standard I/O handle, something's wrong */
		close( fd );
		return( 0 );
		}
	DEBUG_PRINT_COND( debugRandom, 
					  ( "Reading /proc data via ioctls...\n" ));

	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	ENSURES_EXT( cryptStatusOK( status ), 0 );

	/* Get the process status information, misc information, and resource
	   usage */
#ifdef PIOCSTATUS
	if( ioctl( fd, PIOCSTATUS, &prStatus ) != -1 )
		{
		DEBUG_PRINT_COND( debugRandom, 
						  ( "PIOCSTATUS contributed %d bytes.\n", 
							sizeof( prstatus_t ) ));
		addRandomData( randomState, &prStatus, sizeof( prstatus_t ) );
		noEntries++;
		}
#endif /* PIOCSTATUS */
#ifdef PIOCPSINFO
	if( ioctl( fd, PIOCPSINFO, &prMisc ) != -1 )
		{
		DEBUG_PRINT_COND( debugRandom,
						  ( "PIOCPSINFO contributed %d bytes.\n",
							sizeof( prpsinfo_t ) ));
		addRandomData( randomState, &prMisc, sizeof( prpsinfo_t ) );
		noEntries++;
		}
#endif /* PIOCPSINFO */
#ifdef PIOCUSAGE
	if( ioctl( fd, PIOCUSAGE, &prUsage ) != -1 )
		{
		DEBUG_PRINT_COND( debugRandom, 
						  ( "PIOCUSAGE contributed %d bytes.\n",
							sizeof( prusage_t ) ));
		addRandomData( randomState, &prUsage, sizeof( prusage_t ) );
		noEntries++;
		}
#endif /* PIOCUSAGE */

#ifdef PIOCACINFO
	if( ioctl( fd, PIOCACINFO, &pracInfo ) != -1 )
		{
		DEBUG_PRINT_COND( debugRandom, 
						  ( "PIOCACINFO contributed %d bytes.\n",
							sizeof( struct pracinfo ) ));
		addRandomData( randomState, &pracInfo, sizeof( struct pracinfo ) );
		noEntries++;
		}
#endif /* PIOCACINFO */
	close( fd );

	/* Flush any remaining data through and produce an estimate of its
	   value.  We require that at least two of the sources exist and accesses
	   to them succeed, and give them a relatively low value */
	quality = ( noEntries > 2 ) ? 5 : 0;
	endRandomData( randomState, quality );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "proc (via ioctls) contributed %d value.\n", 
					    quality ));

	return( quality );
	}
#endif /* Slowaris || OSF/1 */

/* Linux-specific polling using perf_event.  This is different to the 
   standard polling mechanisms in that it's called twice, once at the start
   of the slow poll and a second time at the end, since it's being used
   to collect event data over the process of the slow poll */

#if defined( __linux__ ) 

#define USE_PERFEVENT
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#ifndef PERF_FLAG_FD_CLOEXEC
  #define PERF_FLAG_FD_CLOEXEC	0
#endif /* !PERF_FLAG_FD_CLOEXEC */

typedef struct __attribute__((packed)) {
	uint64_t value;				/* Event value */
	uint64_t time_enabled;		/* Time enabled */
	uint64_t time_running;		/* Time running */
	uint64_t id;				/* Event group ID */
	} PERF_READ_INFO;

#define PERF_EVENT_MAX		32
#define PERF_EVEN_LAST_HW	9

#define perf_event_open( hw_event, pid, cpu, group_fd, flags ) \
		syscall( __NR_perf_event_open, hw_event, pid, cpu, group_fd, flags )

CHECK_RETVAL_BOOL \
static BOOLEAN getPerfEventBegin( int *pervEventFDs )
	{
	typedef struct {
		const int type, subType;
		} PERF_TYPE;
	const PERF_TYPE perfType[ PERF_EVENT_MAX ] = {
		/* Hardware perf types.  These typically aren't available from user
		   space, producing ENOENT */
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES }, 
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS }, 
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES }, 
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES }, 
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS }, 
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES }, 
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES },
  #if OSVERSION >= 3
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND }, 
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND },
  #endif /* OSVERSION >= 3 */
  #if OSVERSION >= 4	/* Actually 3.3 but we don't do minor versions */
		{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES },
  #endif /* OSVERSION >= 4 */
		/* Software perf types, less useful than hardware.  In particular
		   usually only time_enabled and time_running contain useful data */
		{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK },
		{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK },
		{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS },
		{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES },
		{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS },
		{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN },
		{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ },
		{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS },
		{ PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS },
		{ -1, -1 }, { -1, -1 } };
	BOOLEAN eventAvailable = FALSE;
	int i;

	/* Clear the event information */
	for( i = 0; i < PERF_EVENT_MAX; i++ )
		pervEventFDs[ i ] = -1;

	/* Set up the perf event information for each event type that we want
	   to record */
	for( i = 0; i < PERF_EVENT_MAX && perfType[ i ].type >= 0; i++ )
		{
		struct perf_event_attr pervEventInfo;

		/* Try and open the performance event data */
		memset( &pervEventInfo, 0, sizeof( struct perf_event_attr ) );
		pervEventInfo.size = sizeof( struct perf_event_attr );
		pervEventInfo.type = perfType[ i ].type;
		pervEventInfo.config = perfType[ i ].subType;
		pervEventInfo.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | \
									PERF_FORMAT_TOTAL_TIME_RUNNING | \
									PERF_FORMAT_ID;
		pervEventFDs[ i ] = perf_event_open( &pervEventInfo, 0, -1, -1, 
											 PERF_FLAG_FD_CLOEXEC );
		if( pervEventFDs[ i ] == -1 )
			{
			DEBUG_PRINT_COND( debugRandom && i == 0, 
							  ( "perf_event_open() for PERF_TYPE_HARDWARE "
							    "got errno %d.\n", errno ) );
			continue;
			}
		eventAvailable = TRUE;
		}

	DEBUG_PRINT_COND( debugRandom && !eventAvailable , 
					  ( "No perf_event data available.\n" ) );

	return( eventAvailable );
	}

CHECK_RETVAL \
static int getPerfEventEnd( const int *pervEventFDs )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	int quality = 0, i, status;

	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	ENSURES_EXT( cryptStatusOK( status ), 0 );

	/* Read the output from each event counter */
	for( i = 0; i < PERF_EVENT_MAX; i++ )
		{
		PERF_READ_INFO eventInfo;
		int count;

		if( pervEventFDs[ i ] == -1 )
			continue;
		ioctl( pervEventFDs[ i ], PERF_EVENT_IOC_DISABLE, 0 );
		count = read( pervEventFDs[ i ], &eventInfo, 
					  sizeof( PERF_READ_INFO ) );
		close( pervEventFDs[ i ] );
		if( count <= 0 )
			continue;
		DEBUG_PRINT_COND( debugRandom, 
						  ( "perf_event %d returned value %ld, time %ld, "
						    "runtime %ld.\n", i, eventInfo.value, 
							eventInfo.time_enabled, eventInfo.time_running ) );
		addRandomData( randomState, &eventInfo, count );
		quality += ( i <= PERF_EVEN_LAST_HW ) ? 2 : 1;
		}

	/* Flush any remaining data through and produce an estimate of its
	   value */
	endRandomData( randomState, quality );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "perf_events contributed %d value.\n", quality ));

	return( quality );
	}
#endif /* Linux */

#ifdef __iOS__ 

#define IOS_BUFSIZE			256

CHECK_RETVAL \
static int getIOSData( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	BYTE secRandomBuffer[ IOS_BUFSIZE + 8 ];
	int quality = 0, status;

	/* Get random data from the iOS system randomness routine.  Although 
	   SecRandomCopyBytes() takes as its first parameter the generator type, 
	   only one is allowed, kSecRandomDefault, after which the call is 
	   passed down to CCRandomCopyBytes(), which does something with one of 
	   the NIST DRBGs, but it's all obfuscated with macros and function 
	   pointers so it's hard to tell what.

	   Initially SecRandomCopyBytes() was just a wrapper for a thread-safe
	   read of Apple's /dev/random (for which see the comment in 
	   getDevRandomData() about the value of this and why we only assign a 
	   quality factor of 60%), however what's in use now isn't a /dev/random
	   read any more but something much more complex (see above).
	   
	   Another possible complication is that if both SecRandomCopyBytes() 
	   and the explicit /dev/random read that we perform ourselves are 
	   getting their input from the same source then we should only count 
	   its value once, however the current Apple code, which seems to be out 
	   of sync with the documentation, appears to do more than that */
	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	ENSURES_EXT( cryptStatusOK( status ), 0 );
	status = SecRandomCopyBytes( kSecRandomDefault, IOS_BUFSIZE, 
								 secRandomBuffer );
	if( status == 0 )
		{
		addRandomData( randomState, secRandomBuffer, 256 );
		quality = 60;
		}
	endRandomData( randomState, quality );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "SecRandomCopyBytes contributed %d value.\n", 
					    quality ));

	return( quality );
	}
#endif /* __iOS__  */

/* TPM poll: "The Linux TPM implementation is Trousers".

   We can check for the existence of a TPM, at least under Linux, by 
   checking /sys/class/misc/hw_random/rng_available and 
   /sys/class/misc/hw_random/rng_current, which will point to "tpm-rng"
   if this is present and/or available */

#ifdef USE_TPM

#include <tss/tspi.h>

CHECK_RETVAL \
static int getTPMData( void )
	{
	TSS_HCONTEXT hTSPIContext;
	TSS_HTPM hTPM;
	BYTE *bufPtr;
	int quality = 0, status;

	/* Create a TSPI context, connect it to the system TPM, and get the TPM
	   handle from it.  The typical error return from Tspi_Context_Connect(),
	   if it fails, is the totally undocumented 0x3011 indicating that tcsd 
	   isn't running (check with "ps aux | grep tcsd", and/or via the
	   "tpm_version" command which will print an error if it can't connect), 
	   in which case it needs to be started with 
	   "sudo /etc/init.d/trousers start" */
	if( Tspi_Context_Create( &hTSPIContext ) != TSS_SUCCESS )
		return( 0 );
	status = Tspi_Context_Connect( hTSPIContext, NULL );
	if( status == 0x3011 )
		DEBUG_DIAG(( "Couldn't connect to TPM, is tcsd running?"));
	if( status != TSS_SUCCESS || \
		Tspi_Context_GetTpmObject( hTSPIContext, &hTPM ) != TSS_SUCCESS )
		{
		Tspi_Context_Close( hTSPIContext );
		return( 0 );
		}
	DEBUG_PRINT_COND( debugRandom, 
					  ( "Opened connection to system TPM.\n" ));

	/* Get random data from the TPM */ 
	if( Tspi_TPM_GetRandom( hTPM, 256, &bufPtr ) == TSS_SUCCESS )
		{
		RANDOM_STATE randomState;
		BYTE buffer[ RANDOM_BUFSIZE + 8 ];
		int status;

		status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
		ENSURES_EXT( cryptStatusOK( status ), 0 );
		addRandomData( randomState, bufPtr, 256 );
		quality = 70;
		endRandomData( randomState, quality );
		zeroise( bufPtr, 256 );
		Tspi_Context_FreeMemory( hTSPIContext, bufPtr );
		}

	/* Clean up */
	Tspi_Context_Close( hTPM );
	Tspi_Context_FreeMemory( hTSPIContext, NULL );
	Tspi_Context_Close( hTSPIContext );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "TPM contributed %d value.\n", quality ));

	return( quality );
	}
#endif /* USE_TPM */

/****************************************************************************
*																			*
*							Universal Slow Poll Sources						*
*																			*
****************************************************************************/

#define HWRNG_BUFSIZE		256

static int getHWRNGNAME( BYTE *buffer, const int bufSize )
	{
	int fd, count;
	
	/* Clear return value */
	memset( buffer, 0, bufSize );

	/* Try and read the HW RNG name.  This may or may not be terminated with
	   a LF, so we strip it if there's one present */
	fd = open( "/sys/class/misc/hw_random/rng_current", O_RDONLY );
	if( fd == -1 )
		return( -1 );
	if( fd <= 2 )
		{
		/* We've been given a standard I/O handle, something's wrong */
		close( fd );
		return( -1 );
		}
	count = read( fd, buffer, bufSize ); 
	close( fd ); 
	if( count < 2 )
		return( -1 );
	if( buffer[ count - 1 ] == '\n' )
		buffer[ count - 1 ] = '\0'; 

	return( 0 );
	}

CHECK_RETVAL \
static int getHWRNGData( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	BYTE hwrngBuffer[ HWRNG_BUFSIZE + 8 ];
	int hwrngFD, count, quality = 0, status;

	/* Open the hardware RNG device, whose nature is documented in 
	   /sys/class/misc/hw_random/rng_current.  This is a real Clayton's 
	   device because the fact that it can be opened doesn't mean that it 
	   actually exists.  The justification for this is that the underlying 
	   hardware can be altered at runtime, so the device may change at any 
	   point, which means that we can happily open the nonexistent device 
	   but won't be told that it's nonexistent until we try and read from 
	   it.  Because of this we fail silently rather than warning that no 
	   data was read, since it would lead to endless false positives */
	hwrngFD = open( "/dev/hwrng", O_RDONLY );
	if( hwrngFD < 0 )
		return( 0 );
	DEBUG_OP( count = getHWRNGNAME( buffer, 128 ) );
	DEBUG_PRINT_COND( debugRandom && count >= 0, 
					  ( "Hardware RNG is provided via '%s' source.\n", buffer ));
	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	ENSURES_EXT( cryptStatusOK( status ), 0 );
	count = read( hwrngFD, hwrngBuffer, HWRNG_BUFSIZE );
	close( hwrngFD );
	if( count > 0 )
		{
		addRandomData( randomState, hwrngBuffer, HWRNG_BUFSIZE );
		quality = 50;
		}
	else
		{
		DEBUG_PRINT_COND( debugRandom, 
						  ( "Hardware RNG via /dev/hwrng is available but "
						    "not readable,\n  this is broken but by-design "
							"functionality.\n" ));
		}
	endRandomData( randomState, quality );

	DEBUG_PRINT_COND( debugRandom && quality > 0, 
					  ( "Hardware RNG read via /dev/hwrng contributed %d "
					    "value.\n", quality ));

	return( quality );
	}

/* Named process information via the /procfs interface.  Each source is 
   given a weighting of 1-3, with 1 being a static (although unpredictable) 
   source, 2 being a slowly-changing source, and 3 being a rapidly-changing
   source */

#define PROCS_BUFSIZE		1024

CHECK_RETVAL \
static int getProcFSdata( void )
	{
	typedef struct {
		const char *source;
		const int value;
		} PROCSOURCE_INFO;
	static const PROCSOURCE_INFO procSources[] = {
		{ "diskstats", 2 }, { "interrupts", 3 }, { "loadavg", 2 }, 
		{ "locks", 1 }, { "meminfo", 3 }, { "net/dev", 2 },
		{ "net/ipx", 2 }, { "modules", 1 }, { "mounts", 1 }, 
		{ "net/netstat", 2 }, { "net/rt_cache", 1 }, 
		{ "net/rt_cache_stat", 3 }, { "net/snmp", 2 }, 
		{ "net/softnet_stat", 2 }, { "net/stat/arp_cache", 3 }, 
		{ "net/stat/ndisc_cache", 2 }, { "net/stat/rt_cache", 3 }, 
		{ "net/tcp", 3 }, { "net/udp", 2 }, { "net/wireless", 2 },
		{ "slabinfo", 3 }, { "stat", 3 }, { "sys/fs/inode-state", 1 }, 
		{ "sys/fs/file-nr", 1 }, { "sys/fs/dentry-state", 1 }, 
		{ "sysvipc/msg", 1 }, { "sysvipc/sem", 1 }, { "sysvipc/shm", 1 },
		{ "zoneinfo", 3 },
/*		{ "/sys/devices/system/node/node0/numastat", 2 }, */
		{ NULL, 0 }, { NULL, 0 }
		};
	MESSAGE_DATA msgData;
	BYTE buffer[ PROCS_BUFSIZE + 8 ];
	int procIndex, procFD, procValue = 0, quality;

	/* Read the first 1K of data from some of the more useful sources (most
	   of these produce far less than 1K output) */
	for( procIndex = 0; 
		 procSources[ procIndex ].source != NULL && \
			procIndex < FAILSAFE_ARRAYSIZE( procSources, PROCSOURCE_INFO );
		 procIndex++ )
		{
		char fileName[ 128 + 8 ];
		int count, status;

		/* Try and open the data source.  We don't use O_NOFOLLOW because on 
		   some Unixen special files can be symlinks and in any case a 
		   system that allows attackers to mess with privileged filesystems 
		   like this is presumably a goner anyway */
		sprintf_s( fileName, 128, "/proc/%s", 
				   procSources[ procIndex ].source );
		procFD = open( fileName, O_RDONLY );
		if( procFD < 0 )
			continue;
		if( procFD <= 2 )
			{
			/* We've been given a standard I/O handle, something's wrong */
			DEBUG_DIAG(( "/proc/%s open returned file handle %d, something "
						 "is seriously wrong", fileName, procFD ));
			close( procFD );
			return( 0 );
			}

		/* Read data from the source */
		count = read( procFD, buffer, PROCS_BUFSIZE );
		if( count > 16 )
			{
			DEBUG_PRINT_COND( debugRandom, 
							  ( "/proc/%s contributed %d bytes.\n",
								procSources[ procIndex ].source, count ));
			setMessageData( &msgData, buffer, count );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_SETATTRIBUTE_S, &msgData,
									  CRYPT_IATTRIBUTE_ENTROPY );
			if( cryptStatusOK( status ) )
				procValue += procSources[ procIndex ].value;
			}
		close( procFD );
		}
	ENSURES( procIndex < FAILSAFE_ARRAYSIZE( procSources, PROCSOURCE_INFO ) );
	zeroise( buffer, PROCS_BUFSIZE );

	/* Produce an estimate of the data's value.  We require that we get a
	   quality value of at least 5 and limit it to a maximum value of 50 to 
	   ensure that some data is still coming from other sources */
	if( procValue < 5 )
		return( 0 );
	quality = min( procValue, 50 );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
					 ( MESSAGE_CAST ) &quality, 
					 CRYPT_IATTRIBUTE_ENTROPY_QUALITY );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "procfs reads contributed %d entropy quality.\n", 
						quality ));

	return( quality );
	}

/* System information via the sysfs interface.  These often provide very 
   small amounts of data each, although they're also somewhat schizophrenic, 
   some provide a single numeric value or string while others provide 
   formatted human-readable-only output */

#define SYSFS_BUFSIZE		1024

CHECK_RETVAL \
static int getSysFSdata( void )
	{
	static const char *sysfsSources[] = {
		/* "SCSI" disk, using a very weird definition of SCSI */
		"block/sda0/stat", "block/sda1/stat", "block/sda2/stat", 
		"block/sda3/stat",
		/* Xen virtual disk */
		"block/xvda1/stat", "block/xvda2/stat", "block/xvda3/stat",
		"block/xvda4/stat",
		/* MMC block device */
		"block/mmcblk0/stat", "block/mmcblk1/stat",
		/* Device mapper */
		"block/dm-0/stat", "block/dm-1/stat", "block/dm-2/stat", 
		"block/dm-3/stat",
		/* Device-specific information */
		"class/devfreq/devfreq0/cur_freq", "class/devfreq/devfreq0/trans_stat",
		"class/devfreq/devfreq1/cur_freq", "class/devfreq/devfreq1/trans_stat",
		"class/devfreq/devfreq2/cur_freq", "class/devfreq/devfreq2/trans_stat",
		"class/devfreq/devfreq3/cur_freq", "class/devfreq/devfreq3/trans_stat",
		"class/devfreq/devfreq4/cur_freq", "class/devfreq/devfreq4/trans_stat",
		"class/devfreq/devfreq5/cur_freq", "class/devfreq/devfreq5/trans_stat",
		"class/devfreq/devfreq6/cur_freq", "class/devfreq/devfreq6/trans_stat",
		"class/devfreq/devfreq7/cur_freq", "class/devfreq/devfreq7/trans_stat",
		"class/dma/dma0chan0/bytes_transferred", "class/dma/dma0chan0/memcpy_count",
		"class/dma/dma0chan1/bytes_transferred", "class/dma/dma0chan1/memcpy_count",
		"class/dma/dma1chan0/bytes_transferred", "class/dma/dma1chan0/memcpy_count",
		"class/dma/dma1chan1/bytes_transferred", "class/dma/dma1chan1/memcpy_count",
		"class/hwmon/hwmon0/fan_speed", "class/hwmon/hwmon0/pwm1", 
		"class/hwmon/hwmon1/fan_speed", "class/hwmon/hwmon1/pwm1", 
		"class/input/event0/dev", "class/input/event1/dev", 
		"class/input/event2/dev", "class/input/event3/dev", 
		"class/input/input0/modalias", "class/input/input1/modalias", 
		"class/input/input2/modalias", "class/input/input3/modalias", 
		"class/net/eth0/carrier_changes", "class/net/eth0/statistics/rx_bytes", 
		"class/net/eth0/statistics/rx_dropped", "class/net/eth0/statistics/rx_packets", 
		"class/net/eth0/statistics/tx_bytes", "class/net/eth0/statistics/tx_dropped", 
		"class/net/eth0/statistics/tx_packets",
		"class/net/eth1/carrier_changes", "class/net/eth1/statistics/rx_bytes",
		"class/net/eth1/statistics/rx_dropped", "class/net/eth1/statistics/rx_packets",
		"class/net/eth1/statistics/tx_bytes", "class/net/eth1/statistics/tx_dropped",
		"class/net/eth1/statistics/tx_packets", 
		"class/net/lo/statistics/rx_bytes", "class/net/lo/statistics/rx_dropped", 
		"class/net/lo/statistics/rx_packets", "class/net/lo/statistics/tx_bytes", 
		"class/net/lo/statistics/tx_dropped", "class/net/lo/statistics/tx_packets", 
		"class/net/wlan0/statistics/rx_bytes", "class/net/wlan0/statistics/rx_dropped", 
		"class/net/wlan0/statistics/rx_packets", "class/net/wlan0/statistics/tx_bytes", 
		"class/net/wlan0/statistics/tx_dropped", "class/net/wlan0/statistics/tx_packets", 
		"class/net/wlan1/statistics/rx_bytes", "class/net/wlan1/statistics/rx_dropped", 
		"class/net/wlan1/statistics/rx_packets", "class/net/wlan1/statistics/tx_bytes", 
		"class/net/wlan1/statistics/tx_dropped", "class/net/wlan1/statistics/tx_packets", 
			/* systemd insanity */
		"class/net/eno1/statistics/rx_bytes", "class/net/eno1/statistics/rx_dropped", 
		"class/net/eno1/statistics/rx_packets", "class/net/eno1/statistics/tx_bytes", 
		"class/net/eno1/statistics/tx_dropped", "class/net/eno1/statistics/tx_packets", 
		"class/net/eno2/statistics/rx_bytes", "class/net/eno2/statistics/rx_dropped", 
		"class/net/eno2/statistics/rx_packets", "class/net/eno2/statistics/tx_bytes", 
		"class/net/eno2/statistics/tx_dropped", "class/net/eno2/statistics/tx_packets", 
		"class/net/ens1/statistics/rx_bytes", "class/net/ens1/statistics/rx_dropped", 
		"class/net/ens1/statistics/rx_packets", "class/net/ens1/statistics/tx_bytes", 
		"class/net/ens1/statistics/tx_dropped", "class/net/ens1/statistics/tx_packets", 
		"class/net/ens2/statistics/rx_bytes", "class/net/ens2/statistics/rx_dropped", 
		"class/net/ens2/statistics/rx_packets", "class/net/ens2/statistics/tx_bytes", 
		"class/net/ens2/statistics/tx_dropped", "class/net/ens2/statistics/tx_packets", 
		"class/regulator/regulator.0/microvolts", "class/regulator/regulator.1/microvolts", 
		"class/regulator/regulator.2/microvolts", "class/regulator/regulator.3/microvolts", 
		"class/regulator/regulator.4/microvolts", "class/regulator/regulator.5/microvolts", 
		"class/regulator/regulator.6/microvolts", "class/regulator/regulator.7/microvolts", 
		"class/rtc/rtc0/since_epoch", "class/rtc/rtc1/since_epoch",
		"class/spi_master/spi0/statistics/bytes_rx", 
		"class/spi_master/spi0/statistics/bytes_tx",
		"class/spi_master/spi0/statistics/messages",
		"class/spi_master/spi0/statistics/transfers",
		"class/spi_master/spi1/statistics/bytes_rx",
		"class/spi_master/spi1/statistics/bytes_tx",
		"class/spi_master/spi1/statistics/messages",
		"class/spi_master/spi1/statistics/transfers",
		"class/thermal/thermal_zone0/temp", "class/thermal/thermal_zone1/temp",
		"class/thermal/thermal_zone2/temp", "class/thermal/thermal_zone3/temp",
		"class/thermal/thermal_zone4/temp", "class/thermal/thermal_zone5/temp",
		"class/thermal/thermal_zone6/temp", "class/thermal/thermal_zone7/temp",
		/* System hardware stats */
		"kernel/debug/gpio", "kernel/debug/irq_domain_mapping", 
		"kernel/debug/pwm", "kernel/debug/suspend_stats", 
		"kernel/debug/wakeup_sources", "kernel/debug/asoc/codecs",
		"kernel/debug/asoc/dais", "kernel/debug/asoc/platforms",
		"kernel/debug/clk/clk_dump", "kernel/debug/clk/clk_summary",
		"kernel/debug/memblock/reserved", "kernel/debug/mmc0/ios",
		"kernel/debug/mmc0/regs", "kernel/debug/mmc1/ios", 
		"kernel/debug/mmc1/regs", "kernel/debug/pinctrl/pinctrl-handles",
		"kernel/debug/pinctrl/pinctrl-maps",
		"kernel/debug/regulator/regulator_summary",
		"kernel/debug/usb/devices",
		"power/wakeup_count",
		NULL, NULL
		};
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ], sysfsBuffer[ SYSFS_BUFSIZE + 8 ];
	int sysfsIndex, sysfsFD, sysfsCount = 0, quality, status;

	status = initRandomData( randomState, buffer, RANDOM_BUFSIZE );
	ENSURES_EXT( cryptStatusOK( status ), 0 );

	/* Read the first 1K of data from some of the more useful sources (most
	   of these produce far less than 1K output) */
	for( sysfsIndex = 0; 
		 sysfsSources[ sysfsIndex ] != NULL && \
			sysfsIndex < FAILSAFE_ARRAYSIZE( sysfsSources, char * );
		 sysfsIndex++ )
		{
		char fileName[ 128 + 8 ];
		int count;

		/* Try and open the data source.  We don't use O_NOFOLLOW because 
		   sysfs is typically symlinked all over itself, with links from
		   /sys/whatever going into the system-specific /sys/devices tree
		   (as well as circular links) */
		sprintf_s( fileName, 128, "/sys/%s", sysfsSources[ sysfsIndex ] );
		sysfsFD = open( fileName, O_RDONLY );
		if( sysfsFD < 0 )
			continue;
		if( sysfsFD <= 2 )
			{
			/* We've been given a standard I/O handle, something's wrong */
			DEBUG_DIAG(( "/sys/%s open returned file handle %d, something "
						 "is seriously wrong", fileName, sysfsFD ));
			close( sysfsFD );
			return( 0 );
			}

		/* Read data from the source */
		count = read( sysfsFD, sysfsBuffer, SYSFS_BUFSIZE );
		close( sysfsFD );
		if( count <= 0 )
			continue;

		/* Some sysfs reads return a value as "<value>" and some return it
		   as "<value>\n", so before we continue we have to strip trailing
		   '\n's */
		if( sysfsBuffer[ count - 1 ] == '\n' )
			{
			count--;
			if( count <= 0 )
				continue;
			}

		/* We got something, check whether it's a dummy value ('0') or 
		   something static ('0'/'1') */
		if( count <= 1 && \
			( sysfsBuffer[ 0 ] == '0' || sysfsBuffer[ 0 ] == '1' ) )
			continue;

		DEBUG_PRINT_COND( debugRandom, 
						  ( "/sys/%s contributed %d %s.\n",
							sysfsSources[ sysfsIndex ], count,
							( count > 1 ) ? "bytes" : "byte" ));
		addRandomData( randomState, sysfsBuffer, count );
		sysfsCount++;
		}
	ENSURES( sysfsIndex < FAILSAFE_ARRAYSIZE( sysfsSources, char * ) );
	zeroise( buffer, SYSFS_BUFSIZE );

	/* Produce an estimate of the data's value.  We require that we get 
	   at least 20 results to get the basic quality value */
	quality = ( sysfsCount >= 50 ) ? 20 : ( sysfsCount >= 20 ) ? 10 : 0;
	endRandomData( randomState, quality );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "sysfs reads contributed %d entropy quality.\n", 
						quality ));

	return( quality );
	}

/* /dev/random interface */

#define DEVRANDOM_BUFSIZE	128

CHECK_RETVAL \
static int getDevRandomData( void )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ DEVRANDOM_BUFSIZE + 8 ];
#if defined( __APPLE__ ) || ( defined( __FreeBSD__ ) && OSVERSION >= 5 )
	static const int quality = 60;	/* See comment below */
#else
	static const int quality = 80;
#endif /* Mac OS X || FreeBSD 5.x */
	int randFD, noBytes = 0, status;

	/* Some OSes include a call to access the system randomness data 
	   directly rather than having to go via a pseudo-device, if this 
	   capability is available then we use a direct read.  This currently 
	   applies to OpenBSD (added in 5.6, conveniently about the time that 
	   LibreSSL needed it) and Linux kernels from 3.17 (in response to 
	   prodding from LibreSSL and the appearance of the OpenBSD function).
	   
	   OpenBSD also has the older arc4random_buf(), which used to use
	   RC4 (ugh) until OpenBSD 5.5 when it was replaced by ChaCha20, which
	   postprocess the entropy data through the given stream cipher.  Since 
	   we're feeding the output into our own PRNG and since this function
	   is a general interface to the system randomness data, we use 
	   getentropy() rather than arc4random() */
#if defined( __linux__ ) && defined( GRND_NONBLOCK ) && \
	defined( SYS_getrandom )
	/* getrandom() was defined in kernel 3.17 and above but is rarely
	   supported in libc ("if we add support for it then people might use it
	   and things won't work any more with older libc versions").  In some
	   cases it's possible to access it via syscall() with SYS_getrandom,
	   so the best that we can do is use that if it's available.
	   
	   Even then, getrandom() is somewat risky because its behaviour, at
	   least as originally designed, was to block until sufficient entropy 
	   (128 bits) was available and then become nonblocking, see
	   https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=c6e9d6f3.
	   This means that the call can block at random, presumably if it's 
	   called early in the boot process.  Since cryptlib probably won't
	   be running as a kernel-mode implementation at early boot, this 
	   should be safe... */
	#include <sys/syscall.h>
	noBytes = syscall( SYS_getrandom, buffer, DEVRANDOM_BUFSIZE, 
					   GRND_NONBLOCK );
	DEBUG_PRINT_COND( debugRandom, 
					  ( "getrandom() (via syscall) contributed %d bytes.\n", 
						noBytes ));
#elif defined( __OpenBSD__ ) && OpenBSD > 201412
	/* See the comment at the start for why we use 'OpenBSD' for the version 
	   number */
	noBytes = getentropy( buffer, DEVRANDOM_BUFSIZE );
	if( noBytes == 0 )
		{
		/* OpenBSD's semantics for the call differ from the read()-alike
		   functionality of the Linux version and simply return 0 or -1
		   for success or failure, so we convert the former into a byte
		   count */
		noBytes = DEVRANDOM_BUFSIZE;
		}
	DEBUG_PRINT_COND( debugRandom, 
					  ( "getentropy() contributed %d bytes.\n", noBytes ));
#elif ( defined( __FreeBSD__ ) || defined( __NetBSD__ ) || \
		defined( __OpenBSD__ ) || defined( __APPLE__ ) ) && \
	  defined( KERN_ARND )
	{
	static const int mib[] = { CTL_KERN, KERN_ARND };
	size_t size = DEVRANDOM_BUFSIZE;
	int status;

	/* Alternative to getentropy() if it's not present, supported by some 
	   BSDs */
	if( sysctl( mib, 2, buffer, &size, NULL, 0 ) == 0 )
		noBytes = size;
	DEBUG_PRINT_COND( debugRandom, 
					  ( "sysctl( KERN_ARND ) contributed %d bytes.\n", 
						noBytes ));
	}
#else
	/* Check whether there's a /dev/random present.  We don't use O_NOFOLLOW 
	   because on some Unixen special files can be symlinks and in any case 
	   a system that allows attackers to mess with privileged filesystems 
	   like this is presumably a goner anyway */
	if( ( randFD = open( "/dev/urandom", O_RDONLY ) ) < 0 )
		return( 0 );
	if( randFD <= 2 )
		{
		/* We've been given a standard I/O handle, something's wrong */
		DEBUG_DIAG(( "/dev/urandom open returned file handle %d, something"
					 "is seriously wrong", randFD ));
		close( randFD );
		return( 0 );
		}

	/* Read data from /dev/urandom, which won't block (although the quality
	   of the noise is arguably slightly less).  We only assign this a 75% 
	   quality factor to ensure that we still get randomness from other 
	   sources as well.  
	   
	   Under FreeBSD 5.x and OS X, the /dev/random implementation is broken, 
	   using a pretend dev-random implemented with Yarrow and a 160-bit pool 
	   (animi sub vulpe latent) so we only assign a 60% quality factor.  
	   These generators also lie about entropy, with both /random and 
	   /urandom being the same PRNG-based implementation.
	   
	   The AIX /dev/random isn't an original /dev/random either but merely 
	   provides a compatible interface, taking its input from interrupt 
	   timings of a very small number of sources such as ethernet and SCSI 
	   adapters and postprocessing them with Yarrow.  This implementation is 
	   also a lot more conservative about its entropy estimation, such that 
	   the blocking interface blocks a lot more often than the original 
	   /dev/random implementation would.  In addition it stops gathering 
	   entropy (from interrupts) when it thinks it has enough, and only 
	   resumes when the value falls below a certain value.  
	   
	   OTOH this may still be better than the Linux implementation, which 
	   in 2009 stopped gathering any information from interrupts at all by 
	   removing the IRQF_SAMPLE_RANDOM flag.  The apparent intention was to 
	   replace it with something else, but after much bikeshedding this 
	   never happened, which makes the Linux /dev/random particularly bad 
	   on headless and embedded systems */
	noBytes = read( randFD, buffer, DEVRANDOM_BUFSIZE );
	close( randFD );
#endif /* OSes without API support for the randomness device */
	if( noBytes < 1 )
		{
		if( noBytes == 0 )
			{
			/* This can actually happen */
			DEBUG_DIAG(( "/dev/urandom read returned EOF (bytes read == 0), "
						 "randomness system is broken" ));
			}
		else
			{
			DEBUG_DIAG(( "/dev/urandom read failed, status %d, errno = %d", 
						 noBytes, errno ));
			}
		return( 0 );
		}
	setMessageData( &msgData, buffer, noBytes );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_ENTROPY );
	zeroise( buffer, DEVRANDOM_BUFSIZE );
	if( cryptStatusError( status ) || noBytes < DEVRANDOM_BUFSIZE )
		return( 0 );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
					 ( MESSAGE_CAST ) &quality, 
					 CRYPT_IATTRIBUTE_ENTROPY_QUALITY );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "/dev/urandom contributed %d entropy quality.\n", 
					  quality ));

	return( quality );
	}

/* egd/prngd interface */

CHECK_RETVAL \
static int getEGDdata( void )
	{
	static const char *egdSources[] = {
		"/var/run/egd-pool", "/dev/egd-pool", "/etc/egd-pool", NULL, NULL };
	MESSAGE_DATA msgData;
	BYTE buffer[ DEVRANDOM_BUFSIZE + 8 ];
	static const int quality = 75;
	int egdIndex, sockFD, noBytes = CRYPT_ERROR, status;

	/* Look for the egd/prngd output.  We re-search each time we're called 
	   because, unlike /dev/random, it's both a user-level process and a 
	   movable feast, so it can in theory disappear and reappear at a 
	   different location between runs */
	sockFD = socket( AF_UNIX, SOCK_STREAM, 0 );
	if( sockFD < 0 )
		return( 0 );
	for( egdIndex = 0; egdSources[ egdIndex ] != NULL && \
					   egdIndex < FAILSAFE_ARRAYSIZE( egdSources, char * ); 
		 egdIndex++ )
		{
		struct sockaddr_un sockAddr;

		memset( &sockAddr, 0, sizeof( struct sockaddr_un ) );
		sockAddr.sun_family = AF_UNIX;
		strlcpy_s( sockAddr.sun_path, sizeof( sockAddr.sun_path ), 
				   egdSources[ egdIndex ] );
		if( connect( sockFD, ( struct sockaddr * ) &sockAddr,
					 sizeof( struct sockaddr_un ) ) >= 0 )
			break;
		}
	ENSURES( egdIndex < FAILSAFE_ARRAYSIZE( egdSources, char * ) );
	if( egdSources[ egdIndex ] == NULL )
		{
		close( sockFD );
		return( 0 );
		}

	/* Read up to 128 bytes of data from the source:
		write:	BYTE 1 = read data nonblocking
				BYTE DEVRANDOM_BUFSIZE = count
		read:	BYTE returned bytes
				BYTE[] data
	   As with /dev/random we only assign this a 75% quality factor to
	   ensure that we still get randomness from other sources as well */
	buffer[ 0 ] = 1;
	buffer[ 1 ] = DEVRANDOM_BUFSIZE;
	status = write( sockFD, buffer, 2 );
	if( status == 2 )
		{
		status = read( sockFD, buffer, 1 );
		noBytes = buffer[ 0 ];
		if( status != 1 || noBytes < 0 || noBytes > DEVRANDOM_BUFSIZE )
			status = -1;
		else
			status = read( sockFD, buffer, noBytes );
		}
	close( sockFD );
	if( ( status < 0 ) || ( status != noBytes ) )
		{
		DEBUG_DIAG(( "EGD (%s) read failed", egdSources[ egdIndex ] ));
		return( 0 );
		}

	/* Send the data to the pool */
	setMessageData( &msgData, buffer, noBytes );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_ENTROPY );
	zeroise( buffer, DEVRANDOM_BUFSIZE );
	if( cryptStatusError( status ) || noBytes < DEVRANDOM_BUFSIZE )
		return( 0 );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
					 ( MESSAGE_CAST ) &quality, 
					 CRYPT_IATTRIBUTE_ENTROPY_QUALITY );

	DEBUG_PRINT_COND( debugRandom, 
					  ( "EGD (%s) contributed %d entropy quality.\n", 
						egdSources[ egdIndex ], quality ));

	return( quality );
	}

/****************************************************************************
*																			*
*						Last-resort External-Source Polling					*
*																			*
****************************************************************************/

#ifndef NO_SYSV_SHAREDMEM

/* The structure containing information on random-data sources.  Each record 
   contains the source and a relative estimate of its usefulness (weighting) 
   which is used to scale the number of kB of output from the source (total 
   = data_bytes / usefulness).  Usually the weighting is in the range 1-3 
   (or 0 for especially useless sources), resulting in a usefulness rating 
   of 1...3 for each kB of source output (or 0 for the useless sources).

   If the source is constantly changing (certain types of network statistics
   have this characteristic) but the amount of output is small, the weighting
   is given as a negative value to indicate that the output should be treated
   as if a minimum of 1K of output had been obtained.  If the source produces
   a lot of output then the scale factor is fractional, resulting in a
   usefulness rating of < 1 for each kB of source output.

   In order to provide enough randomness to satisfy the requirements for a
   slow poll, we need to accumulate at least 20 points of usefulness (a
   typical system should get about 30 points).  This is useful not only for 
   use on source-starved systems but also in special circumstances like 
   running in a *BSD jail, in which only other processes running in the jail
   are visible, severely restricting the amount of entropy that can be 
   collected.

   Some potential options are missed out because of special considerations.
   pstat -i and pstat -f can produce amazing amounts of output (the record is
   600K on an Oracle server) that floods the buffer and doesn't yield
   anything useful (apart from perhaps increasing the entropy of the vmstat
   output a bit), so we don't bother with this.  pstat in general produces
   quite a bit of output but it doesn't change much over time, so it gets
   very low weightings.  netstat -s produces constantly-changing output but
   also produces quite a bit of it, so it only gets a weighting of 2 rather
   than 3.  The same holds for netstat -in, which gets 1 rather than 2.  In
   addition some of the lower-ranked sources are either rather heavyweight
   or of low value, and frequently duplicate information obtained by other
   means like kstat or /procfs.  To avoid waiting for a long time for them
   to produce output, we only use them if earlier, quicker sources aren't
   available.

   Some binaries are stored in different locations on different systems so
   alternative paths are given for them.  The code sorts out which one to run
   by itself, once it finds an exectable somewhere it moves on to the next
   source.  The sources are arranged roughly in their order of usefulness,
   occasionally sources that provide a tiny amount of relatively useless
   data are placed ahead of ones that provide a large amount of possibly
   useful data because another 100 bytes can't hurt, and it means the buffer
   won't be swamped by one or two high-output sources. All the high-output
   sources are clustered towards the end of the list for this reason.  Some
   binaries are checked for in a certain order, for example under Slowaris
   /usr/ucb/ps understands aux as an arg, but the others don't.  Some systems
   have conditional defines enabling alternatives to commands that don't
   understand the usual options but will provide enough output (in the form
   of error messages) to look like they're the real thing, causing
   alternative options to be skipped (we can't check the return either
   because some commands return peculiar, non-zero status values even when 
   they're working correctly).

   In order to maximise use of the buffer, the code performs a form of run-
   length compression on its input where a repeated sequence of bytes is
   replaced by the occurrence count mod 256.  Some commands output an awful
   lot of whitespace, this measure greatly increases the amount of data that 
   we can fit into the buffer.

   When we scale the weighting using the SC() macro, some preprocessors may
   give a division by zero warning for the most obvious expression 'weight ?
   1024 / weight : 0' (and gcc 2.7.2.2 dies with a division by zero trap), so
   we define a value SC_0 that evaluates to zero when fed to '1024 / SC_0' */

#define SC( weight )	( int ) ( 1024 / weight )	/* Scale factor */
#define SC_0			16384				/* SC( SC_0 ) evalutes to 0 */

typedef struct {
	const char *path;		/* Path to check for existence of source */
	const char *arg;		/* Args for source */
	const int usefulness;	/* Usefulness of source */
	FILE *pipe;				/* Pipe to source as FILE * */
	int pipeFD;				/* Pipe to source as FD */
	pid_t pid;				/* pid of child for waitpid() */
	int length;				/* Quantity of output produced */
	const BOOLEAN hasAlternative;	/* Whether source has alt.location */
	} DATA_SOURCE_INFO;

static DATA_SOURCE_INFO dataSources[] = {
	/* Sources that are always polled */
	{ "/bin/vmstat", "-s", SC( -3 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/vmstat", "-s", SC( -3 ), NULL, 0, 0, 0, FALSE },
	{ "/bin/vmstat", "-c", SC( -3 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/vmstat", "-c", SC( -3 ), NULL, 0, 0, 0, FALSE },
#if defined( __APPLE__ )
	{ "/usr/bin/vm_stat", NULL, SC( -3 ), NULL, 0, 0, 0, FALSE },
#endif /* Mach version of vmstat */
	{ "/usr/bin/pfstat", NULL, SC( -2 ), NULL, 0, 0, 0, FALSE },
	{ "/bin/vmstat", "-i", SC( -2 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/vmstat", "-i", SC( -2 ), NULL, 0, 0, 0, FALSE },
	{ "/bin/vmstat", "-d", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/vmstat", "-d", SC( -1 ), NULL, 0, 0, 0, FALSE },
	{ "/bin/vmstat", "-a", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/vmstat", "-a", SC( -1 ), NULL, 0, 0, 0, FALSE },
#if defined( _AIX ) || defined( __SCO_VERSION__ )
	{ "/usr/bin/vmstat", "-f", SC( -1 ), NULL, 0, 0, 0, FALSE },
#endif /* OS-specific extensions to vmstat */
	{ "/usr/ucb/netstat", "-s", SC( 2 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/netstat", "-s", SC( 2 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/sbin/netstat", "-s", SC( 2 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/netstat", "-s", SC( 2 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/etc/netstat", "-s", SC( 2 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/nfsstat", NULL, SC( 2 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/ucb/netstat", "-m", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/netstat", "-m", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/sbin/netstat", "-m", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/netstat", "-m", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/etc/netstat", "-m", SC( -1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/ucb/netstat", "-in", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/netstat", "-in", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/sbin/netstat", "-in", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/netstat", "-in", SC( -1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/etc/netstat", "-in", SC( -1 ), NULL, 0, 0, 0, FALSE },
#ifndef __SCO_VERSION__
	{ "/usr/sbin/snmp_request", "localhost public get 1.3.6.1.2.1.7.1.0", SC( -1 ), NULL, 0, 0, 0, FALSE }, /* UDP in */
	{ "/usr/sbin/snmp_request", "localhost public get 1.3.6.1.2.1.7.4.0", SC( -1 ), NULL, 0, 0, 0, FALSE }, /* UDP out */
	{ "/usr/sbin/snmp_request", "localhost public get 1.3.6.1.2.1.4.3.0", SC( -1 ), NULL, 0, 0, 0, FALSE }, /* IP ? */
	{ "/usr/sbin/snmp_request", "localhost public get 1.3.6.1.2.1.6.10.0", SC( -1 ), NULL, 0, 0, 0, FALSE }, /* TCP ? */
	{ "/usr/sbin/snmp_request", "localhost public get 1.3.6.1.2.1.6.11.0", SC( -1 ), NULL, 0, 0, 0, FALSE }, /* TCP ? */
	{ "/usr/sbin/snmp_request", "localhost public get 1.3.6.1.2.1.6.13.0", SC( -1 ), NULL, 0, 0, 0, FALSE }, /* TCP ? */
#else
	{ "/usr/sbin/snmpstat", "-an localhost public", SC( SC_0 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/sbin/snmpstat", "-in localhost public", SC( SC_0 ), NULL, 0, 0, 0, FALSE }, /* Subset of netstat info */
	{ "/usr/sbin/snmpstat", "-Sn localhost public", SC( SC_0 ), NULL, 0, 0, 0, FALSE },
#endif /* SCO/UnixWare vs.everything else */
	{ "/usr/bin/smartctl", "-A /dev/hda" , SC( 1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/sbin/smartctl", "-A /dev/hda" , SC( 1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/smartctl", "-A /dev/hdb" , SC( 1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/sbin/smartctl", "-A /dev/hdb" , SC( 1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/mpstat", NULL, SC( 1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/w", NULL, SC( 1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bsd/w", NULL, SC( 1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/df", NULL, SC( 1 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/df", NULL, SC( 1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/sbin/portstat", NULL, SC( 1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/iostat", NULL, SC( SC_0 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/uptime", NULL, SC( SC_0 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bsd/uptime", NULL, SC( SC_0 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/vmstat", "-f", SC( SC_0 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/vmstat", "-f", SC( SC_0 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/ucb/netstat", "-n", SC( 0.5 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/netstat", "-n", SC( 0.5 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/sbin/netstat", "-n", SC( 0.5 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/netstat", "-n", SC( 0.5 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/etc/netstat", "-n", SC( 0.5 ), NULL, 0, 0, 0, FALSE },

	/* End-of-lightweight-sources section marker */
	{ "", NULL, SC( SC_0 ), NULL, 0, 0, 0, FALSE },

	/* Potentially heavyweight or low-value sources that are only polled if
	   alternative sources aren't available.  ntptrace is somewhat
	   problematic in that some versions don't support -r and -t, made worse
	   by the fact that various servers in the commonly-used pool.ntp.org
	   cluster refuse to answer ntptrace.  As a result, using this can hang
	   for quite awhile before it times out, so we treat it as a heavyweight
	   source even though in theory it's a nice high-entropy lightweight
	   one */
	{ "/usr/sbin/ntptrace", "-r2 -t1 -nv", SC( -1 ), NULL, 0, 0, 0, FALSE },
#if defined( __sgi ) || defined( __hpux )
	{ "/bin/ps", "-el", SC( 0.3 ), NULL, 0, 0, 0, TRUE },
#endif /* SGI || PHUX */
	{ "/usr/ucb/ps", "aux", SC( 0.3 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/ps", "aux", SC( 0.3 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/ps", "aux", SC( 0.3 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/ipcs", "-a", SC( 0.5 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/ipcs", "-a", SC( 0.5 ), NULL, 0, 0, 0, FALSE },
							/* Unreliable source, depends on system usage */
	{ "/etc/pstat", "-p", SC( 0.5 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/pstat", "-p", SC( 0.5 ), NULL, 0, 0, 0, FALSE },
	{ "/etc/pstat", "-S", SC( 0.2 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/pstat", "-S", SC( 0.2 ), NULL, 0, 0, 0, FALSE },
	{ "/etc/pstat", "-v", SC( 0.2 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/pstat", "-v", SC( 0.2 ), NULL, 0, 0, 0, FALSE },
	{ "/etc/pstat", "-x", SC( 0.2 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/pstat", "-x", SC( 0.2 ), NULL, 0, 0, 0, FALSE },
	{ "/etc/pstat", "-t", SC( 0.1 ), NULL, 0, 0, 0, TRUE },
	{ "/bin/pstat", "-t", SC( 0.1 ), NULL, 0, 0, 0, FALSE },
							/* pstat is your friend */
#ifndef __SCO_VERSION__
	{ "/usr/sbin/sar", "-AR", SC( 0.05 ), NULL, 0, 0, 0, FALSE }, /* Only updated hourly */
#endif /* SCO/UnixWare */
	{ "/usr/bin/last", "-n 50", SC( 0.3 ), NULL, 0, 0, 0, TRUE },
#ifdef __sgi
	{ "/usr/bsd/last", "-50", SC( 0.3 ), NULL, 0, 0, 0, FALSE },
#endif /* SGI */
#ifdef __hpux
	{ "/etc/last", "-50", SC( 0.3 ), NULL, 0, 0, 0, FALSE },
#endif /* PHUX */
	{ "/usr/bsd/last", "-n 50", SC( 0.3 ), NULL, 0, 0, 0, FALSE },
#ifdef sun
	{ "/usr/bin/showrev", "-a", SC( 0.1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/sbin/swap", "-l", SC( SC_0 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/sbin/prtconf", "-v", SC( SC_0 ), NULL, 0, 0, 0, FALSE },
#endif /* SunOS/Slowaris */
	{ "/usr/sbin/psrinfo", NULL, SC( SC_0 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/lsof", "-blnwP", SC( 0.3 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/local/bin/lsof", "-blnwP", SC( 0.3 ), NULL, 0, 0, 0, FALSE },
							/* Output is very system and version-dependent
							   and can also be extremely voluminous */
	{ "/usr/sbin/snmp_request", "localhost public get 1.3.6.1.2.1.5.1.0", SC( 0.1 ), NULL, 0, 0, 0, FALSE }, /* ICMP ? */
	{ "/usr/sbin/snmp_request", "localhost public get 1.3.6.1.2.1.5.3.0", SC( 0.1 ), NULL, 0, 0, 0, FALSE }, /* ICMP ? */
	{ "/etc/arp", "-a", SC( 0.1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/etc/arp", "-a", SC( 0.1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/arp", "-a", SC( 0.1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/sbin/arp", "-a", SC( 0.1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/sbin/ripquery", "-nw 1 127.0.0.1", SC( 0.1 ), NULL, 0, 0, 0, FALSE },
	{ "/bin/lpstat", "-t", SC( 0.1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/bin/lpstat", "-t", SC( 0.1 ), NULL, 0, 0, 0, TRUE },
	{ "/usr/ucb/lpstat", "-t", SC( 0.1 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/bin/tcpdump", "-c 5 -efvvx", SC( 1 ), NULL, 0, 0, 0, FALSE },
							/* This is very environment-dependant.  If
							   network traffic is low, it'll probably time
							   out before delivering 5 packets, which is OK
							   because it'll probably be fixed stuff like ARP
							   anyway */
	{ "/usr/sbin/advfsstat", "-b usr_domain", SC( SC_0 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/sbin/advfsstat", "-l 2 usr_domain", SC( 0.5 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/sbin/advfsstat", "-p usr_domain", SC( SC_0 ), NULL, 0, 0, 0, FALSE },
							/* This is a complex and screwball program.  Some
							   systems have things like rX_dmn, x = integer,
							   for RAID systems, but the statistics are
							   pretty dodgy */
#if 0
	/* The following aren't enabled since they're somewhat slow and not very
	   unpredictable, however they give an indication of the sort of sources
	   you can use (for example the finger might be more useful on a
	   firewalled internal network) */
	{ "/usr/bin/finger", "@ml.media.mit.edu", SC( 0.9 ), NULL, 0, 0, 0, FALSE },
	{ "/usr/local/bin/wget", "-O - http://lavarand.sgi.com/block.html", SC( 0.9 ), NULL, 0, 0, 0, FALSE },
	{ "/bin/cat", "/usr/spool/mqueue/syslog", SC( 0.9 ), NULL, 0, 0, 0, FALSE },
#endif /* 0 */

	/* End-of-sources marker */
	{ NULL, NULL, 0, NULL, 0, 0, 0, FALSE }, 
		{ NULL, NULL, 0, NULL, 0, 0, 0, FALSE }
	};

/* Variables to manage the child process that fills the buffer */

static pid_t gathererProcess = 0;/* The child process that fills the buffer */
static BYTE *gathererBuffer;	/* Shared buffer for gathering random noise */
static int gathererMemID;		/* ID for shared memory */
static int gathererBufSize;		/* Size of the shared memory buffer */
static struct sigaction gathererOldHandler;	/* Previous signal handler */
#ifdef USE_THREADS
  static pthread_mutex_t gathererMutex;	/* Mutex to protect the polling */
#endif /* USE_THREADS */

/* The struct at the start of the shared memory buffer used to communicate
   information from the child to the parent */

typedef struct {
	int usefulness;				/* Usefulness of data in buffer */
	int noBytes;				/* No.of bytes in buffer */
	} GATHERER_INFO;

#if defined( __MVS__ ) || defined( __hpux ) || defined( __Android__ )

/* MVS USS, PHUX, and the Android version of Linux don't have wait4() so we 
   emulate it with waitpid() and getrusage() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4 ) ) \
pid_t wait4( pid_t pid, int *status, int options, struct rusage *rusage )
	{
	const pid_t waitPid = waitpid( pid, status, options );

	getrusage( RUSAGE_CHILDREN, rusage );
	return( waitPid );
	}
#endif /* MVS USS || PHUX || Android */

/* Cray Unicos and QNX 4.x have neither wait4() nor getrusage, so we fake
   it */

#if defined( _CRAY ) || ( defined( __QNX__ ) && OSVERSION <= 4 )

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4 ) ) \
pid_t wait4( pid_t pid, int *status, int options, struct rusage *rusage )
	{
	memset( rusage, 0, sizeof( struct rusage ) );
	return( waitpid( pid, status, options ) );
	}
#endif /* Cray Unicos || QNX 4.x */

#if !defined( _POSIX_PRIORITY_SCHEDULING ) || ( _POSIX_PRIORITY_SCHEDULING < 0 )
  /* No sched_yield() or sched_yield() not supported */
  #define sched_yield()
#elif ( _POSIX_PRIORITY_SCHEDULING == 0 )

static void my_sched_yield( void )
	{
	/* sched_yield() is only supported if sysconf() tells us that it is */
	if( sysconf( _SC_PRIORITY_SCHEDULING ) > 0 )
		sched_yield();
	}
#define sched_yield				my_sched_yield

#endif /* Systems without sched_yield() */

/* When exiting from the child, we call _exit() rather than exit() since 
   this skips certain cleanup operations (atexit() and signal handlers).
   More technically, _exit() is a system call that performs kernel-space
   cleanup while exit() also performs user-space cleanup */
	  
#define CHILD_EXIT( status )	_exit( status )

/* A special form of the ENSURES() predicate used in the forked child 
   process, which calls CHILD_EXIT() rather than returning */

#define ENSURES_EXIT( x ) \
		if( !( x ) ) { assert( INTERNAL_ERROR ); CHILD_EXIT( -1 ); }

/* Under SunOS 4.x popen() doesn't record the pid of the child process.  When
   pclose() is called, instead of calling waitpid() for the correct child, it
   calls wait() repeatedly until the right child is reaped.  The problem with
   this behaviour is that this reaps any other children that happen to have
   died at that moment, and when their pclose() comes along the process hangs
   forever.

   This behaviour may be related to older SVR3-compatible SIGCLD handling in
   which, under the SIG_IGN disposition, the status of the child was discarded
   (i.e. no zombies were generated) so that when the parent called wait() it
   would block until all children terminated, whereupon wait() would return -1
   with errno set to ECHILD.

   The fix for this problem is to use a wrapper for popen()/pclose() that
   saves the pid in the dataSources structure (code adapted from GNU-libc's
   popen() call).  Doing our own popen() has other advantages as well, for
   example we use the more secure execl() to run the child instead of the
   dangerous system().

   Aut viam inveniam aut faciam */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static FILE *my_popen( INOUT DATA_SOURCE_INFO *entry )
	{
	static uid_t gathererUID = ( uid_t ) -1, gathererGID = ( uid_t ) -1;
	int pipedes[ 2 + 8 ];
	FILE *stream;

	/* Sanity check for stdin and stdout */
	REQUIRES_N( STDIN_FILENO <= 1 && STDOUT_FILENO <= 1 );

	/* If we're root, get UID/GID information so that we can give up our 
	   permissions to make sure that we don't inadvertently read anything 
	   sensitive.  We do it at this point in order to avoid calling 
	   getpwnam() between the fork() and the execl() since some getpwnam()s
	   may call malloc() which could be locked by another thread which the
	   child won't have access to */
	if( geteuid() == 0 && gathererUID == ( uid_t ) -1 )
		{
		struct passwd *passwd;

		passwd = getpwnam( "nobody" );
		if( passwd != NULL )
			{
			gathererUID = passwd->pw_uid;
			gathererGID = passwd->pw_gid;
			}
		else
			{
			assert( DEBUG_WARN );
			}
		}

	/* Create the pipe.  Note that under QNX the pipe resource manager
	   'pipe' must be running in order to use pipes */
	if( pipe( pipedes ) < 0 )
		return( NULL );

	/* Fork off the child.  In theory we could use vfork() ("vfork() is like 
	   an OS orgasm.  All OSes want to do it, but most just end up faking 
	   it" - Chris Wedgwood), but most modern Unixen use copy-on-write for 
	   forks anyway (with vfork() being just an alias for fork()), so we get 
	   most of the benefits of vfork() with a plain fork().  There is 
	   however another problem with fork that isn't fixed by COW.  Any large 
	   program, when forked, requires (at least temporarily) a lot of 
	   address space.  That is, when the process is forked the system needs 
	   to allocate many virtual pages (backing store) even if those pages 
	   are never used.  If the system doesn't have enough swap space 
	   available to support this, the fork() will fail when the system tries 
	   to reserve backing store for pages that are never touched.  Even in 
	   non-large processes this can cause problems when (as with the 
	   randomness-gatherer) many children are forked at once.  However this 
	   is a rather unlikely situation, and since the fork()-based entropy-
	   gathering is used only as a last resort when all other methods have
	   failed, we rarely get to this code, let alone run it in a situation
	   where the problem described above crops up.

	   In the absence of threads the use of pcreate() (which only requires
	   backing store for the new processes' stack, not the entire process)
	   would do the trick, however pcreate() isn't compatible with threads,
	   which makes it of little use for the default thread-enabled cryptlib
	   build */
	entry->pid = fork();
	if( entry->pid == ( pid_t ) -1 )
		{
		/* The fork failed */
		close( pipedes[ 0 ] );
		close( pipedes[ 1 ] );
		return( NULL );
		}

	if( entry->pid == ( pid_t ) 0 )
		{
		int fd;

		/* We're the child, connect the read side of the pipe to stdout and
		   unplug stdin and stderr */
		if( dup2( pipedes[ STDOUT_FILENO ], STDOUT_FILENO ) < 0 )
			CHILD_EXIT( 127 );
		if( ( fd = open( "/dev/null", O_RDWR ) ) > 0 )
			{
			dup2( fd, STDIN_FILENO );
			dup2( fd, STDERR_FILENO );
			close( fd );
			}

		/* Give up our permissions if required to make sure that we don't 
		   inadvertently read anything sensitive.  We don't check whether 
		   the change succeeds since it's not a major security problem but 
		   just a precaution (in theory an attacker could do something like 
		   fork()ing until RLIMIT_NPROC is reached, at which point it'd 
		   fail, but that doesn't really give them anything) */
		if( gathererUID != ( uid_t ) -1 )
			{
#if 0		/* Not available on some OSes */
			( void ) setuid( gathererUID );
			( void ) seteuid( gathererUID );
			( void ) setgid( gathererGID );
			( void ) setegid( gathererGID );
#else
  #if( defined( __linux__ ) || ( defined( __FreeBSD__ ) && OSVERSION >= 5 ) || \
	   ( defined( __hpux ) && OSVERSION >= 11 ) )
			( void ) setresuid( gathererUID, gathererUID, gathererUID );
			( void ) setresgid( gathererGID, gathererGID, gathererGID );
  #else
			( void ) setreuid( gathererUID, gathererUID );
			( void ) setregid( gathererGID, gathererGID );
  #endif /* OSses with setresXid() */
#endif /* 0 */
			}

		/* Close the pipe descriptors */
		close( pipedes[ STDIN_FILENO ] );
		close( pipedes[ STDOUT_FILENO ] );

		/* Try and exec the program */
		execl( entry->path, entry->path, entry->arg, NULL );

		/* Die if the exec failed */
		CHILD_EXIT( 127 );
		}

	/* We're the parent, close the irrelevant side of the pipe and open the
	   relevant side as a new stream.  Mark our side of the pipe to close on
	   exec, so new children won't see it (if this call fails there's not 
	   much that we can do, and it's mostly a hygiene thing so we don't fail
	   fatally on it) */
	close( pipedes[ STDOUT_FILENO ] );
	( void ) fcntl( pipedes[ STDIN_FILENO ], F_SETFD, FD_CLOEXEC );
	stream = fdopen( pipedes[ STDIN_FILENO ], "r" );
	if( stream == NULL )
		{
		int savedErrno = errno;

		/* The stream couldn't be opened or the child structure couldn't be
		   allocated.  Kill the child and close the other side of the pipe */
		kill( entry->pid, SIGKILL );
		close( pipedes[ STDOUT_FILENO ] );
		waitpid( entry->pid, NULL, 0 );
		entry->pid = 0;
		errno = savedErrno;
		return( NULL );
		}

	return( stream );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int my_pclose( INOUT DATA_SOURCE_INFO *entry, 
					  INOUT struct rusage *rusage )
	{
	pid_t pid;
	int iterationCount = 0, status = 0;

	/* Close the pipe */
	fclose( entry->pipe );
	entry->pipe = NULL;

	/* Wait for the child to terminate, ignoring the return value from the
	   process because some programs return funny values that would result
	   in the input being discarded even if they executed successfully.
	   This isn't a problem because the result data size threshold will
	   filter out any programs that exit with a usage message without
	   producing useful output */
	do
		{
		/* We use wait4() instead of waitpid() to get the last bit of
		   entropy data, the resource usage of the child */
		errno = 0;
		pid = wait4( entry->pid, NULL, 0, rusage );
		}
	while( pid == -1 && errno == EINTR && \
		   iterationCount++ < FAILSAFE_ITERATIONS_SMALL );
	if( pid != entry->pid )
		status = -1;
	entry->pid = 0;

	return( status );
	}

/* Get data from an external entropy source */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int getEntropySourceData( INOUT DATA_SOURCE_INFO *dataSource, 
								 OUT_BUFFER( bufSize, *bufPos ) BYTE *bufPtr, 
								 IN_DATALENGTH const int bufSize, 
								 OUT_DATALENGTH_Z int *bufPos )
	{
	int bufReadPos = 0, bufWritePos = 0;
	size_t noBytes;

	/* Try and get more data from the source.  If we get zero bytes then the
	   source has sent us all it has */
	if( ( noBytes = fread( bufPtr, 1, bufSize, dataSource->pipe ) ) <= 0 )
		{
		struct rusage rusage;
		int total = 0;

		/* If there's a problem, exit */
		if( my_pclose( dataSource, &rusage ) != 0 )
			return( 0 );

		/* Try and estimate how much entropy we're getting from a data
		   source */
		if( dataSource->usefulness != 0 )
			{
			if( dataSource->usefulness < 0 )
				{
				/* Absolute rating, 1024 / -n */
				total = 1025 / -dataSource->usefulness;
				}
			else
				{
				/* Relative rating, 1024 * n */
				total = dataSource->length / dataSource->usefulness;
				}
			}
		DEBUG_PRINT_COND( debugRandom, 
						  ( "%s %s contributed %d bytes (compressed), "
							"usefulness = %d.\n", dataSource->path,
							( dataSource->arg != NULL ) ? dataSource->arg : "",
							dataSource->length, total ));

		/* Copy in the last bit of entropy data, the resource usage of the
		   popen()ed child */
		if( sizeof( struct rusage ) < bufSize )
			{
			memcpy( bufPtr, &rusage, sizeof( struct rusage ) );
			*bufPos += sizeof( struct rusage );
			}

		return( total );
		}

	/* Run-length compress the input byte sequence */
	while( bufReadPos < noBytes )
		{
		const int ch = byteToInt( bufPtr[ bufReadPos ] );

		/* If it's a single byte or we're at the end of the buffer, just
		   copy it over */
		if( bufReadPos >= bufSize - 1 || ch != bufPtr[ bufReadPos + 1 ] )
			{
			bufPtr[ bufWritePos++ ] = intToByte( ch );
			bufReadPos++;
			}
		else
			{
			int count = 0;

			/* It's a run of repeated bytes, replace them with the byte
			   count mod 256 */
			while( bufReadPos < noBytes && ( ch == bufPtr[ bufReadPos ] ) )
				{
				count++;
				bufReadPos++;
				}
			bufPtr[ bufWritePos++ ] = intToByte( count );
			}
		}

	/* Remember the number of (compressed) bytes of input that we obtained */
	*bufPos += bufWritePos;
	dataSource->length += noBytes;

	return( 0 );
	}

/* The child process that performs the polling, forked from slowPoll() */

#define SLOWPOLL_TIMEOUT	30		/* Time out after 30 seconds */

static void childPollingProcess( const int existingEntropy )
	{
	GATHERER_INFO *gathererInfo;
	MONOTIMER_INFO timerInfo;
	BOOLEAN moreSources;
	struct timeval tv;
	struct rlimit rl = { 0, 0 };
	fd_set fds;
#ifdef _SC_OPEN_MAX
	const int fdTableSize = sysconf( _SC_OPEN_MAX );
#else
	const int fdTableSize = getdtablesize();
#endif /* System-specific ways to get the file descriptor tbl.size */
#if defined( __hpux )
	size_t maxFD = 0;
#else
	int maxFD = 0;
#endif /* OS-specific brokenness */
	int usefulness = 0, fdIndex, bufPos, i, iterationCount, status;

	/* General housekeeping: Make sure that we can never dump core, and close
	   all inherited file descriptors.  We need to do this because if we
	   don't and the calling app has FILE's open, these will be flushed if 
	   we call exit() in the child and again when the parent writes to them 
	   or closes them, resulting in everything that was present in the
	   FILE * buffer at the time of the fork() being written twice.  An
	   alternative solution would be to call _exit() instead of exit() 
	   (which is the default action for the CHILD_EXIT() macro), but this is 
	   somewhat system-dependant and isn't present in all cases so we still
	   take the precaution of exit()-proofing the code.
	   
	   Note that we don't close any of the standard handles because this 
	   could lead to the next file being opened being given the stdin/stdout/
	   stderr handle, which in general is just a nuisance but then some 
	   older kernels didn't check handles when running a setuid program so 
	   that it was actually an exploitable flaw.  In addition some later 
	   kernels (e.g. NetBSD) overreact to the problem a bit and complain 
	   when they see a setuid program with stdin/stdout/stderr closed, so 
	   it's a good idea to leave these open.  We could in theory close them
	   anyway and reopen them to /dev/null, but it's not clear whether this
	   really buys us anything.

	   In addition to this we should in theory call cryptEnd() since we
	   don't need any cryptlib objects beyond this point and it'd be a good
	   idea to clean them up to get rid of sensitive data held in memory.
	   However in some cases when using a crypto device or network interface
	   or similar item the shutdown in the child will also shut down the
	   parent item because while cryptlib objects are reference-counted the
	   external items aren't (they're beyond the control of cryptlib).  Even
	   destroying just contexts with keys loaded isn't possible because they
	   may be tied to a device that will propagate the shutdown from the
	   child to the parent via the device.

	   In general the child will be short-lived, and the fact that any modern 
	   fork() will have copy-on-write semantics will mean that cryptlib 
	   memory is never copied to the child and further children.  It would, 
	   however, be better if there were some way to perform a neutron-bomb 
	   type shutdown that only zeroises senstive information while leaving 
	   structures intact */
	setrlimit( RLIMIT_CORE, &rl );
	for( fdIndex = fdTableSize - 1; fdIndex > STDOUT_FILENO; fdIndex-- )
		close( fdIndex );
	fclose( stderr );	/* Arrghh!!  It's Stuart code!! */

	/* Fire up each randomness source */
	FD_ZERO( &fds );
	for( i = 0; dataSources[ i ].path != NULL && \
				i < FAILSAFE_ARRAYSIZE( dataSources, DATA_SOURCE_INFO ); i++ )
		{
		/* Check for the end-of-lightweight-sources marker */
		if( dataSources[ i ].path[ 0 ] == '\0' )
			{
			/* If we're only polling lightweight sources because we've
			   already obtained entropy from additional sources, we're
			   done */
			if( existingEntropy >= 50 )
				{
				DEBUG_PRINT_COND( debugRandom, 
								  ( "All lightweight sources polled, exiting "
									"without polling heavyweight ones.\n" ));
				break;
				}

			/* We're polling all sources, continue with the heavyweight
			   ones */
			continue;
			}

		/* Since popen() is a fairly heavyweight function, we check to see 
		   whether the executable exists before we try to run it */
		if( access( dataSources[ i ].path, X_OK ) )
			{
			DEBUG_PRINT_COND( debugRandom, 
							  ( "%s not present%s.\n", dataSources[ i ].path,
								dataSources[ i ].hasAlternative ? \
									", has alternatives" : "" ));
			dataSources[ i ].pipe = NULL;
			}
		else
			dataSources[ i ].pipe = my_popen( &dataSources[ i ] );
		if( dataSources[ i ].pipe == NULL )
			continue;
		if( fileno( dataSources[ i ].pipe ) >= FD_SETSIZE )
			{
			/* The fd is larger than what can be fitted into an fd_set, don't
			   try and use it.  This can happen if the calling app opens a
			   large number of files, since most FD_SET() macros don't
			   perform any safety checks this can cause segfaults and other
			   problems if we don't perform the check ourselves */
			fclose( dataSources[ i ].pipe );
			dataSources[ i ].pipe = NULL;
			continue;
			}

		/* Set up the data source information */
		dataSources[ i ].pipeFD = fileno( dataSources[ i ].pipe );
		if( dataSources[ i ].pipeFD > maxFD )
			maxFD = dataSources[ i ].pipeFD;
		fcntl( dataSources[ i ].pipeFD, F_SETFL, O_NONBLOCK );
		FD_SET( dataSources[ i ].pipeFD, &fds );
		dataSources[ i ].length = 0;

		/* If there are alternatives for this command, don't try and execute
		   them */
		iterationCount = 0;
		while( dataSources[ i ].path != NULL && \
			   dataSources[ i ].hasAlternative && \
			   i < FAILSAFE_ARRAYSIZE( dataSources, DATA_SOURCE_INFO ) && \
			   iterationCount++ < FAILSAFE_ITERATIONS_MED ) 
			{
			DEBUG_PRINT_COND( debugRandom, 
							  ( "Skipping %s.\n", 
								dataSources[ i + 1 ].path ));
			i++;
			}
		ENSURES_EXIT( iterationCount < FAILSAFE_ITERATIONS_MED );
					  /* i is checked as part of the loop control */
		}
	ENSURES_EXIT( i < FAILSAFE_ARRAYSIZE( dataSources, DATA_SOURCE_INFO ) );
	gathererInfo = ( GATHERER_INFO * ) gathererBuffer;
	bufPos = sizeof( GATHERER_INFO );	/* Start of buf.has status info */

	/* Suck up all of the data that we can get from each of the sources */
	status = setMonoTimer( &timerInfo, SLOWPOLL_TIMEOUT );
	ENSURES_EXIT( cryptStatusOK( status ) );
	for( moreSources = TRUE, iterationCount = 0;
		 moreSources && bufPos < gathererBufSize && \
			iterationCount < FAILSAFE_ITERATIONS_MAX; 
		 iterationCount++ )
		{
		/* Wait for data to become available from any of the sources, with a
		   timeout of 10 seconds.  This adds even more randomness since data
		   becomes available in a nondeterministic fashion.  Kudos to HP's QA
		   department for managing to ship a select() that breaks its own
		   prototype */
		tv.tv_sec = 10;
		tv.tv_usec = 0;
#if defined( __hpux ) && ( OSVERSION == 9 || OSVERSION == 0 )
		if( select( maxFD + 1, ( int * ) &fds, NULL, NULL, &tv ) == -1 )
#else
		if( select( maxFD + 1, &fds, NULL, NULL, &tv ) == -1 )
#endif /* __hpux */
			break;

		/* One of the sources has data available, read it into the buffer */
		for( i = 0; dataSources[ i ].path != NULL && \
					i < FAILSAFE_ARRAYSIZE( dataSources, DATA_SOURCE_INFO ); 
			 i++ )
			{
			if( dataSources[ i ].pipe != NULL && \
				FD_ISSET( dataSources[ i ].pipeFD, &fds ) )
				{
				usefulness += getEntropySourceData( &dataSources[ i ],
													gathererBuffer + bufPos,
													gathererBufSize - bufPos,
													&bufPos );
				}
			}
		ENSURES_EXIT( i < FAILSAFE_ARRAYSIZE( dataSources, DATA_SOURCE_INFO ) );

		/* Check if there's more input available on any of the sources */
		moreSources = FALSE;
		FD_ZERO( &fds );
		for( i = 0; dataSources[ i ].path != NULL && \
					i < FAILSAFE_ARRAYSIZE( dataSources, DATA_SOURCE_INFO ); 
			 i++ )
			{
			if( dataSources[ i ].pipe != NULL )
				{
				FD_SET( dataSources[ i ].pipeFD, &fds );
				moreSources = TRUE;
				}
			}
		ENSURES_EXIT( i < FAILSAFE_ARRAYSIZE( dataSources, DATA_SOURCE_INFO ) );

		/* If we've gone over our time limit, kill anything still hanging
		   around and exit.  This prevents problems with input from blocked
		   sources */
		if( checkMonoTimerExpired( &timerInfo ) )
			{
			for( i = 0; dataSources[ i ].path != NULL && \
						i < FAILSAFE_ARRAYSIZE( dataSources, DATA_SOURCE_INFO ); 
				 i++ )
				{
				if( dataSources[ i ].pipe != NULL )
					{
					DEBUG_DIAG(( "Aborting read of %s due to timeout", 
								 dataSources[ i ].path ));
					fclose( dataSources[ i ].pipe );
					kill( dataSources[ i ].pid, SIGKILL );
					dataSources[ i ].pipe = NULL;
					dataSources[ i ].pid = 0;
					}
				}
			ENSURES_EXIT( i < FAILSAFE_ARRAYSIZE( dataSources, DATA_SOURCE_INFO ) );
			moreSources = FALSE;
			DEBUG_DIAG(( "Poll timed out, probably due to blocked data "
						 "source" ));
			}
		}
	ENSURES_EXIT( iterationCount < FAILSAFE_ITERATIONS_MAX );
	gathererInfo->usefulness = usefulness;
	gathererInfo->noBytes = bufPos;

	DEBUG_PRINT_COND( debugRandom, 
					  ( "Got %d bytes, usefulness = %d.\n", bufPos, 
						usefulness ));

	/* "Thou child of the daemon, ... wilt thou not cease...?"
	   -- Acts 13:10 */
	CHILD_EXIT( 0 );
	}

/* Unix external-source slow poll.  If a few of the randomness sources 
   create a large amount of output then the slowPoll() stops once the buffer 
   has been filled (but before all of the randomness sources have been 
   sucked dry) so that the 'usefulness' factor remains below the threshold.  
   For this reason the gatherer buffer has to be fairly sizeable on 
   moderately loaded systems.

   An alternative strategy, suggested by Massimo Squillace, is to use a 
   chunk of shared memory protected by a semaphore, with the first 
   sizeof( int ) bytes at the start serving as a high-water mark.  The 
   process forks and waitpid()'s for the child's pid.  The child forks all 
   the entropy-gatherers and exits, allowing the parent to resume execution. 
   The child's children are inherited by init (double-fork paradigm), when 
   each one is finished it takes the semaphore, writes data to the shared 
   memory segment at the given offset, updates the offset, releases the 
   semaphore again, and exits, to be reaped by init.

   The parent monitors the shared memory offset and when enough data is 
   available takes the semaphore, grabs the data, and releases the shared 
   memory area and semaphore.  If any children are still running they'll get 
   errors when they try to access the semaphore or shared memory and 
   silently exit.

   This approach has the advantage that all of the forked processes are 
   managed by init rather than having the parent have to wait for them, but 
   the disadvantage that the end-of-job handling is rather less rigorous. 
   An additional disadvantage is that the existing code has had a lot of 
   real-world testing and adaptation to system-specific quirks, which would 
   have to be repeated for any new version */

#define SHARED_BUFSIZE		49152	/* Usually about 25K are filled */

static void externalSourcesPoll( const int existingEntropy )
	{
	const int pageSize = getSysVar( SYSVAR_PAGESIZE );

	/* Check whether a non-default SIGCHLD handler is present.  This is 
	   necessary because if the program that cryptlib is a part of installs 
	   its own handler it will end up reaping the cryptlib children before
	   cryptlib can.  As a result my_pclose() will call waitpid() on a
	   process that has already been reaped by the installed handler and
	   return an error, so the read data won't be added to the randomness
	   pool */
	if( sigaction( SIGCHLD, NULL, &gathererOldHandler ) < 0 )
		{
		/* This assumes that stderr is open, i.e. that we're not a daemon.
		   This should be the case at least during the development/debugging
		   stage */
		DEBUG_DIAG(( "sigaction() failed, errno = %d", errno ));
		fprintf( stderr, "cryptlib: sigaction() failed, errno = %d, "
				 "file " __FILE__ ", line %d.\n", errno, __LINE__ );
		abort();
		}

	/* Check for handler override */
	if( gathererOldHandler.sa_handler != SIG_DFL && \
		gathererOldHandler.sa_handler != SIG_IGN )
		{
		/* We overwrote the caller's handler, warn them about this */
		DEBUG_DIAG(( "Conflicting SIGCHLD handling detected in randomness "
					 "polling code,\nsee the source code for more "
					 "information" ));
		}

	/* If a non-default handler is present, replace it with the default 
	   handler */
	if( gathererOldHandler.sa_handler != SIG_DFL )
		{
		struct sigaction newHandler;

		memset( &newHandler, 0, sizeof( newHandler ) );
		newHandler.sa_handler = SIG_DFL;
		sigemptyset( &newHandler.sa_mask );
		sigaction( SIGCHLD, &newHandler, NULL );
		}

	/* Determine how much memory we want to set up.  This can get a bit 
	   tricky, implementations typically round the amount passed to shmget()
	   up to the system page size, typically 4kB, but some systems have
	   variable-size pages that can, in theory, go up to 4GB or more.  To
	   deal with this, we fit the amount that we're requesting into the page 
	   size if pages are more or less standard-sized, but don't try and do 
	   anything if pages are huge */
	gathererBufSize = ( pageSize <= SHARED_BUFSIZE ) ? \
					  ( ( SHARED_BUFSIZE + pageSize - 1 ) / pageSize ) * pageSize : \
					  SHARED_BUFSIZE;

	/* Set up the shared memory */
	errno = 0;
	if( ( gathererMemID = shmget( IPC_PRIVATE, gathererBufSize,
								  IPC_CREAT | 0600 ) ) == -1 || \
		( gathererBuffer = ( BYTE * ) shmat( gathererMemID,
											 NULL, 0 ) ) == ( BYTE * ) -1 )
		{
		/* There was a problem obtaining the shared memory, warn the user
		   and exit */
#ifdef _CRAY
		if( errno == ENOSYS )
			{
			/* Unicos supports shmget/shmat, but older Crays don't implement
			   it and return ENOSYS */
			fprintf( stderr, "cryptlib: SYSV shared memory required for "
					 "random number gathering isn't\n  supported on this "
					 "type of Cray hardware (ENOSYS),\n  file " __FILE__ 
					 ", line %d.\n", __LINE__ );
			}
#endif /* Cray */
		DEBUG_DIAG(( "shmget()/shmat() failed, errno = %d", errno ));
		if( gathererMemID != -1 )
			shmctl( gathererMemID, IPC_RMID, NULL );
		if( gathererOldHandler.sa_handler != SIG_DFL )
			sigaction( SIGCHLD, &gathererOldHandler, NULL );
		unlockPollingMutex();
		return; /* Something broke */
		}

	/* At this point we have a possible race condition since we need to set
	   the gatherer PID value inside the mutex but messing with mutexes
	   across a fork() is somewhat messy.  To resolve this, we set the PID
	   to a nonzero value (which marks it as busy) and exit the mutex, then
	   overwrite it with the real PID (also nonzero) from the fork */
	gathererProcess = -1;
	unlockPollingMutex();

	/* Fork off the gatherer, the parent process returns to the caller.  
	
	   Programs using the OS X Core Foundation framework will complain if a 
	   program calls fork() and doesn't follow it up with an exec() (which 
	   really screws up daemonization ), printing an error message:

		The process has forked and you cannot use this CoreFoundation \
		functionality safely. You MUST exec().

	   There are workarounds possible but they're daemon-specific and 
	   involve running under launchd, which isn't really an option here.
	   For now this is left as an OS X framework problem, there doesn't
	   seem to be anything that we can do to fix it here */
	if( ( gathererProcess = fork() ) != 0 )
		{
		/* If we're the parent, we're done */
		if( gathererProcess != -1 )
			return;

		/* The fork() failed, clean up and reset the gatherer PID to make 
		   sure that we're not locked out of retrying the poll later */
		DEBUG_DIAG(( "fork() failed, errno = %d", errno ));
		lockPollingMutex();
		shmctl( gathererMemID, IPC_RMID, NULL );
		if( gathererOldHandler.sa_handler != SIG_DFL )
			sigaction( SIGCHLD, &gathererOldHandler, NULL );
		gathererProcess = 0;
		unlockPollingMutex();
		return;
		}

	/* Make the child an explicitly distinct function */
	childPollingProcess( existingEntropy );
	}

static int externalSourcesPollComplete( const BOOLEAN force )
	{
	MESSAGE_DATA msgData;
	GATHERER_INFO *gathererInfo = ( GATHERER_INFO * ) gathererBuffer;
	pid_t pid;
	int quality, waitPidStatus, iterationCount = 0;

	lockPollingMutex();
	if( gathererProcess	<= 0 )
		{
		/* There's no gatherer running, exit */
		unlockPollingMutex();

		return( CRYPT_OK );
		}

	/* If this is a forced shutdown, be fairly assertive with the gathering 
	   process */
	if( force )
		{
		/* Politely ask the the gatherer to shut down and (try and) yield 
		   our timeslice a few times so that the shutdown can take effect. 
		   This is unfortunately somewhat implementation-dependant in that 
		   in some cases it'll only yield the current thread's timeslice 
		   rather than the overall process' timeslice, or if it's a high-
		   priority thread it'll be scheduled again before any lower-
		   priority threads get to run */
		kill( gathererProcess, SIGTERM );
		sched_yield();
		sched_yield();
		sched_yield();	/* Well, sync is done three times too... */

		/* If the gatherer is still running, ask again, less politely this 
		   time */
		errno = 0;
		if( kill( gathererProcess, 0 ) != -1 || errno != ESRCH )
			kill( gathererProcess, SIGKILL );
		}

	/* Wait for the gathering process to finish and, if it was sucessful, 
	   add the randomness that it's gathered */
	do
		{
		errno = 0;
		pid = waitpid( gathererProcess, &waitPidStatus, 0 );
		}
	while( pid == -1 && errno == EINTR && \
		   iterationCount++ < FAILSAFE_ITERATIONS_SMALL );
	if( pid == gathererProcess && WIFEXITED( waitPidStatus ) )
		{
		/* The child terminated normally, forward its output to the system 
		   device */
		if( gathererInfo->noBytes > 0 && !force )
			{
			int status;

			quality = min( gathererInfo->usefulness * 5, 100 );	/* 0-20 -> 0-100 */
			setMessageData( &msgData, gathererBuffer, gathererInfo->noBytes );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_SETATTRIBUTE_S,
									  &msgData, CRYPT_IATTRIBUTE_ENTROPY );
			assert( cryptStatusOK( status ) );
			if( quality > 0 )
				{
				/* On some very cut-down embedded systems the entropy 
				   quality can be zero so we only send a quality estimate if 
				   there's actually something there */
				status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
										  IMESSAGE_SETATTRIBUTE, &quality,
										  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
				assert( cryptStatusOK( status ) );
				}

			DEBUG_PRINT_COND( debugRandom,
							  ( "Polling parent added %d bytes, "
								"quality = %d.\n", gathererInfo->noBytes, 
								quality ));
			}
		}
	else
		{
		/* There was a problem with the gatherer process, let the user 
		   know */
		DEBUG_DIAG(( "Polling parent failed to get data from child,\n  "
					 "waitpid() status value %X, errno = %d", waitPidStatus, 
					 errno ));
		DEBUG_PRINT_COND( debugRandom && WIFSIGNALED( waitPidStatus ),
						  ( "  Child was terminated due to signal %d.\n",
						    WTERMSIG( waitPidStatus ) ));
		}
	zeroise( gathererBuffer, gathererBufSize );

	/* Detach and delete the shared memory (the latter is necessary because 
	   otherwise the unused ID hangs around until the process terminates) 
	   and restore the original signal handler if we replaced someone else's 
	   one */
	shmdt( gathererBuffer );
	shmctl( gathererMemID, IPC_RMID, NULL );
	if( gathererOldHandler.sa_handler != SIG_DFL )
		{
		struct sigaction oact;

		/* We replaced someone else's handler for the slow poll, reinstate 
		   the original one.  Since someone else could have in turn replaced 
		   our handler, we check for this and warn the user if necessary */
		sigaction( SIGCHLD, NULL, &oact );
		if( oact.sa_handler != SIG_DFL )
			{
			/* The current handler isn't the one that we installed, warn the 
			   user */
			DEBUG_DIAG(( "SIGCHLD handler was replaced while slow poll was "
						 "in progress,\nsee the source code for more "
						 "information" ));
			}
		else
			{
			/* Our handler is still in place, replace it with the original 
			   one */
			sigaction( SIGCHLD, &gathererOldHandler, NULL );
			}
		}
	gathererProcess = 0;
	unlockPollingMutex();

	return( CRYPT_OK );
	}
#endif /* NO_SYSV_SHAREDMEM */

/****************************************************************************
*																			*
*									Slow Poll								*
*																			*
****************************************************************************/

/* Slow poll.  We try for sources that are directly accessible 
   programmatically and only fall back to the external-sources poll as a 
   last resort (this is rarely, if ever, required) */

void slowPoll( void )
	{
#ifdef USE_PERFEVENT
	int perfEventFDs[ PERF_EVENT_MAX ];
	BOOLEAN perfEventsOK = FALSE;
#endif /* USE_PERFEVENT */
	int entropy = 0;

	DEBUG_PRINT_COND( debugRandom, 
					  ( "Attempting to get entropy from /dev/urandom" ));
	DEBUG_PRINT_COND( debugRandom && !access( "/dev/hwrng", R_OK ), 
					  ( ", /dev/hwrng" ));
	DEBUG_PRINT_COND( debugRandom && !access( "/proc/interrupts", R_OK ), 
					  ( ", procfs" ));
	DEBUG_PRINT_COND( debugRandom && !access( "/sys/block", R_OK ), 
					  ( ", sysfs" ));
#ifdef USE_KSTAT
	DEBUG_PRINT_COND( debugRandom, ( ", kstat" ));
#endif /* USE_KSTAT */
#ifdef USE_PROC
	DEBUG_PRINT_COND( debugRandom, ( ", proc (via ioctls)" ));
#endif /* USE_PROC */
#ifdef USE_SYSCTL
	DEBUG_PRINT_COND( debugRandom, ( ", sysctl" ));
#endif /* USE_SYSCTL */
#ifdef USE_GETIFADDRS
	DEBUG_PRINT_COND( debugRandom, ( ", getifaddrs" ));
#endif /* USE_GETIFADDRS */
#ifdef USE_GETFSSTAT
	DEBUG_PRINT_COND( debugRandom, ( ", getfsstat" ));
#endif /* USE_GETFSSTAT */
#ifdef USE_PERFEVENT
	DEBUG_PRINT_COND( debugRandom && !access( "/proc/sys/kernel/perf_event_paranoid", R_OK ),
					  ( ", perf_event" ));
#endif /* USE_PERFEVENT */
#ifdef __iOS__ 
	DEBUG_PRINT_COND( debugRandom, ( ", iOS SecRandomCopyBytes()" ));
#endif /* __iOS__  */
#ifdef USE_TPM
	DEBUG_PRINT_COND( debugRandom, ( ", system TPM" ));
#endif /* USE_TPM */
	DEBUG_PRINT_COND( debugRandom, ( ".\n" ));

	/* If we can detect a TPM and its use isn't enabled, notify that it 
	   could be used */
#ifndef USE_TPM
	DEBUG_OP( { BYTE buffer[ 128 ]; int count; 
				 count = getHWRNGNAME( buffer, 128 ) );
	DEBUG_PRINT_COND( count >= 0 && !memcmp( buffer, "tpm-rng", 7 ),
					  ( "System contains a TPM, consider enabling TPM use "
					    "via TSPI libraries.\n" ));
	DEBUG_OP( } );
#endif /* USE_TPM */

	/* Make sure that we don't start more than one slow poll at a time.  The
	   gathererProcess value may be positive (a PID) or -1 (error), so we
	   compare it to the specific value 0 (= not-used) in the check */
	lockPollingMutex();
	if( gathererProcess != 0 )
		{
		unlockPollingMutex();
		return;
		}

	/* Begin performance event polling.  This brackets the other polling 
	   types since it measures system performance around the duration of the
	   other polls */
#ifdef USE_PERFEVENT
	if( !access( "/proc/sys/kernel/perf_event_paranoid", R_OK ) )
		perfEventsOK = getPerfEventBegin( perfEventFDs );
#endif /* USE_PERFEVENT */

	/* The popen()-level slow poll is the screen-scraping interface of last
	   resort that we use only if we can't get the entropy in any other
	   way.  If the system provides entropy from alternate sources, we don't 
	   have have to try the screen-scraping slow poll (a number of these 
	   additional sources, things like procfs and kstats, duplicate the 
	   sources polled in the slow poll anyway, so we're not adding much by 
	   polling these extra sources if we've already got the data directly) */
	entropy += getDevRandomData();
	if( !access( "/dev/hwrng", R_OK ) )
		entropy += getHWRNGData();
	if( !access( "/proc/interrupts", R_OK ) )
		entropy += getProcFSdata();
	if( !access( "/sys/block", R_OK ) )
		entropy += getSysFSdata();
	entropy += getEGDdata();
#ifdef USE_KSTAT
	entropy += getKstatData();
#endif /* USE_KSTAT */
#ifdef USE_PROC
	entropy += getProcData();
#endif /* USE_PROC */
#ifdef USE_SYSCTL
	entropy += getSysctlData();
#endif /* USE_SYSCTL */
#ifdef USE_GETIFADDRS
	entropy += getIfaddrsData();
#endif /* USE_GETIFADDRS */
#ifdef USE_GETFSSTAT
	entropy += getFsstatData();
#endif /* USE_GETFSSTAT */
#ifdef __iOS__ 
	entropy += getIOSData();
#endif /* __iOS__  */
#ifdef USE_TPM
	entropy += getTPMData();
#endif /* USE_TPM */
#ifdef USE_PERFEVENT
	if( perfEventsOK )
		entropy += getPerfEventEnd( perfEventFDs );
#endif /* USE_PERFEVENT */
	DEBUG_PRINT_COND( debugRandom, 
					  ( "Got %d entropy from direct sources.\n", entropy ));
	if( entropy >= 100 )
		{
		DEBUG_PRINT_COND( debugRandom, 
						  ( "  Skipping full slowpoll since sufficient "
							"entropy is available.\n" ));
		}
	else
		{
		DEBUG_PRINT_COND( debugRandom, 
						  ( "  Continuing to full slowpoll since sufficient "
							"entropy wasn't gathered.\n" ));
		}
	if( entropy >= 100 )
		{
		/* We got enough entropy from the additional sources, we don't
		   have to go through with the full external-sources poll */
		unlockPollingMutex();
		return;
		}

	/* A few systems don't support SYSV shared memory so we can't go beyond 
	   this point, all that we can do is warn the user that they'll have to 
	   use the entropy mechanisms intended for embedded systems without 
	   proper entropy sources */
#ifdef NO_SYSV_SHAREDMEM
	fprintf( stderr, "cryptlib: This system doesn't contain the OS "
			 "mechanisms required to provide\n          system entropy "
			 "sources that can be used for key generation.  In\n"
			 "          order to use cryptlib in this environment, you "
			 "need to apply the\n          randomness mechanisms for "
			 "embedded systems described in the\n          cryptlib "
			 "manual.\n" );
	unlockPollingMutex();
#else
	externalSourcesPoll( entropy );
#endif /* NO_SYSV_SHAREDMEM */
	}

/* Wait for the randomness gathering to finish */

CHECK_RETVAL \
int waitforRandomCompletion( const BOOLEAN force )
	{
#ifdef NO_SYSV_SHAREDMEM
	return( CRYPT_OK );
#else
	return( externalSourcesPollComplete( force ) );
#endif /* NO_SYSV_SHAREDMEM */
	}

/* Check whether we've forked and we're the child.  The mechanism used varies
   depending on whether we're running in a single-threaded or multithreaded
   environment, for single-threaded we check whether the pid has changed
   since the last check, for multithreaded environments this isn't reliable
   since some systems have per-thread pid's so we need to use
   pthread_atfork() as a trigger to set the pid-changed flag.
   
   This is complicated by the fact that some threads implementations don't 
   call pthread_atfork() on a vfork() (see the note about OS X below) while 
   others do, so that with sufficiently obstreperous use of vfork() (doing 
   stuff other than calling one of the exec() functions) it's possible to 
   end up with duplicate pools.  OTOH this behaviour is explicitly advised 
   against in vfork() manpages and documented as leading to undefined 
   behaviour, so anyone who does this kinda gets what they deserve.

   In terms of OS-specific issues, under Aches calling pthread_atfork() with 
   any combination of arguments or circumstances produces a segfault, so we 
   disable its use and fall back to the getpid()-based fork detection.  In 
   addition some other environments don't support the call, so we exclude 
   those as well.  FreeBSD is a particular pain because of its highly 
   confusing use of -RELEASE, -STABLE (experimental), and -CURRENT (also 
   experimental but differently so) while maintaining the same version, it's 
   present in 5.x-CURRENT but not 5.x-RELEASE or -STABLE, so we have to 
   exclude it for all 5.x to be safe.  OS X is also a bit of a pain, support 
   was added after 10.4 (Tiger) but OS X uses vfork() internally so the 
   atfork handler doesn't get called because parent and child are still 
   sharing the same address space, so we also rely on the getpid()-based 
   fork detection mechanism.
   
   Another problem, specific to glibc and shared libraries, is that 
   pthread_atfork() isn't actually present in libpthread.so but in a static
   library libpthread_nonshared.a, with libpthread.so actually being a 
   linker script that pulls in libpthread.so.0 and libpthread_nonshared.a.
   If something goes wrong with this hack, it produces the gcc gibberish
   "hidden symbol `pthread_atfork' in XXX is referenced by DSO", meaning 
   that there's a problem with linking in pthread_atfork() from 
   libpthread_nonshared.a.
   
   There are other possible fork-checking mechanisms, but they're even less
   portable, and considerably more ugly, than pthread_atfork().  For example 
   some recent open-source Unix systems implement an INHERIT_ZERO flag for 
   minherit() which causes a page to be zeroed on fork, present in OpenBSD 
   5.6 and newer and FreeBSD 12.0 and newer, with the Linux version being 
   MADV_WIPEONFORK passed to madvise() in Linux 4.14 and newer.  One way to 
   implement this would be to mmap() a page, fill it with 0xFF, set 
   INHERIT_ZERO/MADV_WIPEONFORK as required, and treat it as a has-forked 
   boolean, but the lack of portability and general clunkiness makes it far 
   less useful than pthread_atfork() */

#if defined( USE_THREADS ) && \
	( defined( _AIX ) || defined( __Android__ ) || defined( _CRAY ) || \
	  defined( __MVS__ ) || defined( _MPRAS ) || defined( __APPLE__ ) || \
	  ( defined( __FreeBSD__ ) && OSVERSION <= 5 ) )
  #define NO_PTHREAD_ATFORK
#endif /* USE_THREADS && OSes without pthread_atfork() */

#if defined( USE_THREADS ) && !defined( NO_PTHREAD_ATFORK )

static BOOLEAN forked = FALSE;
static pthread_mutex_t forkedMutex;

BOOLEAN forkCheck( const BOOLEAN checkForked )
	{
	BOOLEAN hasForked;

	/* Read the forked-t flag in a thread-safe manner */
	pthread_mutex_lock( &forkedMutex );
	if( !checkForked )
		hasForked = forked = FALSE;
	else
		{
		hasForked = forked;
		forked = FALSE;
		}
	pthread_mutex_unlock( &forkedMutex );

	return( hasForked );
	}

void setForked( void )
	{
	/* Set the forked-t flag in a thread-safe manner */
	pthread_mutex_lock( &forkedMutex );
	forked = TRUE;
	pthread_mutex_unlock( &forkedMutex );
	}

#else

BOOLEAN forkCheck( const BOOLEAN checkForked )
	{
	static pid_t originalPID = -1;

	/* If this is an init, remember the current PID */
	if( !checkForked )
		originalPID = getpid();
	else
		{
		/* This is a fork check, if the pid has changed then we've forked 
		   and we're the child, remember the new pid */
		if( getpid() != originalPID )
			{
			originalPID = getpid();
			return( TRUE );
			}
		}

	return( FALSE );
	}
#endif /* USE_THREADS && !NO_PTHREAD_ATFORK */

/* Initialise and clean up any auxiliary randomness-related objects */

void initRandomPolling( void )
	{
	/* Hardcoding in the Posix function name at this point is safe because 
	   it also works for Solaris threads */
#ifdef USE_THREADS
	pthread_mutex_init( &gathererMutex, NULL );

	/* If it's multithreaded code then we need to ensure that we're 
	   signalled if another thread calls fork().  We set the forked flag 
	   in both the child and the parent to ensure that both sides remix the 
	   pool thoroughly */
  #ifndef NO_PTHREAD_ATFORK
	pthread_mutex_init( &forkedMutex, NULL );
	pthread_atfork( NULL, setForked, setForked );
  #endif /* NO_PTHREAD_ATFORK */
#endif /* USE_THREADS */
	}

void endRandomPolling( void )
	{
#ifdef USE_THREADS
	pthread_mutex_destroy( &gathererMutex );

  #ifndef NO_PTHREAD_ATFORK
	pthread_mutex_destroy( &forkedMutex );
  #endif /* NO_PTHREAD_ATFORK */
#endif /* USE_THREADS */
	}
