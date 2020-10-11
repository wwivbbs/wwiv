/****************************************************************************
*																			*
*							File Stream I/O Header							*
*						Copyright Peter Gutmann 1993-2015					*
*																			*
****************************************************************************/

#ifndef _STRFILE_DEFINED

#define _STRFILE_DEFINED

/****************************************************************************
*																			*
*						 				AMX									*
*																			*
****************************************************************************/

#if defined( __AMX__ )

#include <fjzzz.h>

/****************************************************************************
*																			*
*						 			DOS/Win16								*
*																			*
****************************************************************************/

#elif defined( __MSDOS16__ ) || defined( __WIN16__ )

#include <io.h>
#include <errno.h>			/* Needed for access() check */

/****************************************************************************
*																			*
*						 			IBM 4758								*
*																			*
****************************************************************************/

#elif defined( __IBM4758__ )

#include <scc_err.h>
#include <scc_int.h>

/****************************************************************************
*																			*
*						 			Macintosh								*
*																			*
****************************************************************************/

#elif defined( __MAC__ )

#include <Script.h>
#if defined __MWERKS__
  #pragma mpwc_relax off
  #pragma extended_errorcheck on
#endif /* __MWERKS__ */

/****************************************************************************
*																			*
*						 		Mongoose OS									*
*																			*
****************************************************************************/

#elif defined( __MGOS__ )

#include <mgos_vfs.h>

/* The Mongoose VFS header defines the Unix-style I/O functions, but with 
   the prefix "mgos_vfs_", so we map them back to standard names.  We have 
   to be careful with mapping stat() because there's also a struct stat */

#define open			mgos_vfs_open
#define close			mgos_vfs_close
#define read			mgos_vfs_read
#define write			mgos_vfs_write
#define stat( a, b )	mgos_vfs_stat( a, b )
#define fstat			mgos_vfs_fstat
#define lseek			mgos_vfs_lseek
#define unlink			mgos_vfs_unlink
#define rename			mgos_vfs_rename

/****************************************************************************
*																			*
*						 				MQX									*
*																			*
****************************************************************************/

#elif defined( __MQXRTOS__ )

/* The MQX fio.h redefines every stdio function to MQX_internal names, if 
   we're cross-compiling and stdio functions are already present as macros 
   then we have to undefine them to let MQX re-define them.  If they're 
   present as typedefs then we have to map them to something harmless.  
   
   Finally, we have to define __STDC__ to remove some hacks that are enabled
   in the MQX headers if this isn't defined */

#ifdef _MSC_VER
  #ifdef feof
	#undef feof
	#undef ferror
	#undef stdin
	#undef stdout
	#undef stderr
	#define FILE		__FILE
  #endif /* Conflicting defines */
  #define __STDC__		1
#endif

#include <mfs.h>

/****************************************************************************
*																			*
*						 			Nucleus									*
*																			*
****************************************************************************/

#elif defined( __Nucleus__ )

#include <pcdisk.h>

/****************************************************************************
*																			*
*						 				OS/2								*
*																			*
****************************************************************************/

#elif defined( __OS2__ )

#define INCL_DOSFILEMGR		/* DosQueryPathInfo(),DosSetFileSize(),DosSetPathInfo */
#define INCL_DOSMISC		/* DosQuerySysInfo() */
#include <os2.h>			/* FILESTATUS */
#include <io.h>
#include <errno.h>			/* Needed for access() check */

/****************************************************************************
*																			*
*						 			Quadros									*
*																			*
****************************************************************************/

#elif defined( __Quadros__ )

#include <udefs_s.h>
#include <api_s.h>

/****************************************************************************
*																			*
*						 				SMX									*
*																			*
****************************************************************************/

#elif defined( __SMX__ )

#include <smx.h>
#include <smxfs.h>

/****************************************************************************
*																			*
*						 			Telit									*
*																			*
****************************************************************************/

#elif defined( __Telit__ )

#include <m2m_fs_api.h>

/****************************************************************************
*																			*
*						 		ThreadX (via FileX)							*
*																			*
****************************************************************************/

#elif defined( __FileX__ )

#include <fx_api.h>

/****************************************************************************
*																			*
*						 			TI RTS									*
*																			*
****************************************************************************/

#elif defined( __TI_COMPILER_VERSION__ )

#include <errno.h>

/****************************************************************************
*																			*
*						 			uITRON									*
*																			*
****************************************************************************/

#elif defined( __ITRON__ )

/* uITRON has a file API (ITRON/FILE) derived from the BTRON persistent
   object store interface, but the only documentationm for this is for BTRON
   and it's only available in Japanese.  Because of the inability to obtain
   either documentation or an implementation to code against, anyone with
   access to the required documentation/implementation will need to fill in
   the required headers and functions here */

#error You need to set up the ITRON/FILE headers and interface in str_file.c

/****************************************************************************
*																			*
*						 		Unix/Unix-like Systems						*
*																			*
****************************************************************************/

#elif defined( __Android__ ) || defined( __BEOS__ ) || \
	  defined( __ECOS__ ) || defined( __iOS__ ) || defined( __MVS__ ) || \
	  defined( __RTEMS__ ) || defined( __SYMBIAN32__ ) || \
	  defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ ) || \
	  defined( __UNIX__ )

#if defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ )
  /* Needed for lstat() in sys/lstat.h */
  #define _XOPEN_SOURCE_EXTENDED	1
#endif /* Tandem */
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#if !( defined( __ECOS__ ) || defined( __TANDEM_NSK__ ) || \
	   defined( __TANDEM_OSS__ ) )
  #include <sys/file.h>
#endif /* Tandem */
#include <sys/stat.h>
#if defined( _AIX ) || defined( __alpha__ ) || defined( _MPRAS ) || \
	defined( __osf__ ) || defined( __SCO_VERSION__ )
  #include <sys/mode.h>
#endif /* Vaguely SYSV-ish systems */
#include <unistd.h>
#if defined( _AIX ) || defined( __alpha__ ) || defined( __BEOS__ ) || \
	defined( __bsdi__ ) || defined( _CRAY ) || defined( __FreeBSD__ ) || \
	defined( __iOS__ ) || defined( __linux__ ) || defined( _MPRAS ) || \
	defined( __MVS__ ) || defined( _M_XENIX ) || defined( __NetBSD__ ) || \
	defined( __OpenBSD__ ) || defined( __osf__ ) || defined( __QNX__ ) || \
	defined( __SCO_VERSION__ ) || defined( sun ) || \
	defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ )
  #include <utime.h>			/* It's a SYSV thing... */
#endif /* SYSV Unixen */
#if defined( __APPLE__ ) || defined( __linux__ )
  #include <sys/time.h>			/* For futimes() */
#endif /* OS X || Linux */
#ifdef __CYGWIN__
  #include <sys/utime.h>
#endif /* __CYGWIN__ */

/* By default we try and use flock()-locking, if this isn't available we
   fall back to fcntl() locking (see the long comment further on).  Actually
   Slowaris does have flock() but there are lots of warnings in the manpage
   about using it only on BSD platforms and it requires the BSD libraries to
   work.  SunOS did support it without any problems, it's only the SVR4 
   Slowaris that breaks it - the Solaris flock() is really just a 
   compatibility hack around fcntl() locking, even up to the very latest 
   versions (Solaris 10), and there are various weird side-effects and
   problems that make it too dangerous to use.  In addition UnixWare 
   (== SCO) supports something called flockfile() but this only provides 
   thread-level locking that isn't useful */

#if defined( _AIX ) || defined( __BEOS__ ) || defined( __CYGWIN__ ) || \
	defined( __hpux ) || defined( _MPRAS ) || defined( __MVS__ ) || \
	defined( _M_XENIX ) || defined( __SCO_VERSION__ ) || \
	( defined( sun ) && ( OSVERSION >= 5 ) ) || \
	defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ )
  #define USE_FCNTL_LOCKING
#endif /* Some older SYSV-ish systems */

/* Older versions of SCO didn't have ftruncate() but did have the equivalent
   function chsize() */

#if ( defined( _M_XENIX ) && ( OSVERSION == 3 ) )
  #define ftruncate( a, b )	chsize( a, b )
#endif /* SCO */

/* SunOS 4 doesn't have memmove(), but Solaris does, so we define memmove()
   to bcopy() under 4.  In addition SunOS doesn't define the fseek()
   position indicators so we define these as well */

#if defined( __UNIX__ ) && defined( sun ) && ( OSVERSION == 4 )
  #define memmove				bcopy

  #define SEEK_SET				0
  #define SEEK_CUR				1
  #define SEEK_END				2
#endif /* SunOS 4 */

/* Some versions of Cygwin don't define the locking constants */

#if defined( __CYGWIN__ ) && !defined( LOCK_SH )
  #define LOCK_SH		1
  #define LOCK_EX		2
  #define LOCK_NB		4
  #define LOCK_UN		8
#endif /* Cygwin */

/****************************************************************************
*																			*
*						 			VxWorks									*
*																			*
****************************************************************************/

#elif defined( __VxWorks__ )

#include <vxWorks.h>
#include <ioLib.h>
#include <errno.h>
#include <ioctl.h>
#include <sys/stat.h>

/****************************************************************************
*																			*
*						 			Xilinx XMK								*
*																			*
****************************************************************************/

#elif defined( __XMK__ )

#include <xilmfs.h>

#endif /* OS-specific includes and defines */

#endif /* _STRFILE_DEFINED */
