/****************************************************************************
*																			*
*				cryptlib OS-Specific Config/Detection Header File 			*
*						Copyright Peter Gutmann 1992-2017					*
*																			*
****************************************************************************/

#ifndef _OSDETECT_DEFINED

#define _OSDETECT_DEFINED

/* os_detect.h performs OS and compiler detection that's used by config.h, so
   this file must be applied before config.h */

#ifdef _CONFIG_DEFINED
  #error "os_detect.h must be included before config.h"
#endif /* _CONFIG_DEFINED */

/****************************************************************************
*																			*
*									OS Detection							*
*																			*
****************************************************************************/

/* Try and figure out if we're running under Windows and Win16/Win32/WinCE.
   We have to jump through all sorts of hoops later on, not helped by the
   fact that the method of detecting Windows at compile time changes with
   different versions of Visual C (it's different for each of VC 2.0, 2.1,
   4.0, and 4.1.  It actually remains the same after 4.1) */

#ifndef __WINDOWS__
  #if defined( _Windows ) || defined( _WINDOWS )
	#define __WINDOWS__
  #endif /* Older Windows compilers */
  #ifdef __MINGW32__
	#define __WINDOWS__
  #endif /* MinGW */
#endif /* Windows */
#if !defined( __WIN32__ ) && ( defined( WIN32 ) || defined( _WIN32 ) )
  #ifndef __WINDOWS__
	#define __WINDOWS__		/* Win32 or WinCE */
  #endif /* __WINDOWS__ */
  #ifdef _WIN32_WCE
	#define __WINCE__
  #else
	#define __WIN32__
  #endif /* WinCE vs. Win32 */
  #if defined( _M_X64 )
	#define __WIN64__
  #endif /* Win64 */
#endif /* Win32 or WinCE */
#if defined( __WINDOWS__ ) && \
	!( defined( __WIN32__ ) || defined( __WINCE__ ) )
  #define __WIN16__
#endif /* Windows without Win32 or WinCE */

/* If we're using a DOS compiler and it's not a 32-bit one, record this.
   __MSDOS__ is predefined by a number of compilers, so we use __MSDOS16__
   for stuff that's 16-bit DOS specific, and __MSDOS32__ for stuff that's
   32-bit DOS specific */

#if defined( __MSDOS__ ) && !defined( __MSDOS32__ )
  #define __MSDOS16__
#endif /* 16-bit DOS */
#if defined( __WATCOMC__ ) && defined( __DOS__ )
  #ifndef __MSDOS__
	#define __MSDOS__
  #endif /* 16- or 32-bit DOS */
  #if defined( __386__ ) && !defined( __MSDOS32__ )
	#define __MSDOS32__
  #endif /* 32-bit DOS */
#endif /* Watcom C under DOS */

/* Make the defines for various OSes look a bit more like the usual ANSI 
  defines that are used to identify the other OS types */

#ifdef __TANDEM
  #if defined( _OSS_TARGET )
	#define __TANDEM_OSS__
  #elif defined( _GUARDIAN_TARGET )
	#define __TANDEM_NSK__
  #else
	#error "Can't determine Tandem OS target type (NSK or OSS)"
  #endif /* Tandem OSS vs. NSK */
#endif /* Tandem */

#if defined( __MWERKS__ ) || defined( SYMANTEC_C ) || defined( __MRC__ )
  #define __MAC__
#endif /* Macintosh */

#if defined( __OS400__ ) || defined( __ILEC400__ )
  #define __AS400__
#endif /* AS/400 */

#ifdef __PALMSOURCE__
  #define __PALMOS__
#endif /* Palm OS */

#ifdef __VMS
  #define __VMS__
#endif /* VMS */

#if defined( __APPLE__ )
  /* Apple provides an environment-specific file that provides detailed
	 information about the target enviroment, defining TARGET_OS_xxx to 1
	 for a given target environment */
  #include <TargetConditionals.h>
  #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
	#define __iOS__
  #elif TARGET_OS_WATCH
	/* It's a bit unclear what the OS for Apple's watch will eventually end 
	   up as, for now we treat it as iOS since we're only accessing the low-
	   level functionality */
	#define __iOS__
  #endif /* iOS aliases */
#endif /* __APPLE__ */

/* In some cases we're using a Windows system as an emulated cross-
   development platform, in which case we are we add extra defines to turn 
   off some Windows-specific features.  The override for BOOLEAN is required 
   because once __WIN32__ is turned off we try and typedef BOOLEAN, but 
   under Windows it's already typedef'd which leads to error messages */

#if defined( __WIN32__ ) && ( _MSC_VER == 1200 ) && defined( CROSSCOMPILE )
  /* Embedded OS variant.  Remember to change Project | Settings | C/C++ |
	 Preprocessor | Additional include directories as per the code 
	 comments, and add the new OS to the USE_THREADS and USE_EMBEDDED_OS 
	 defines in config.h */
//	#define __ARINC653__	/* Extra include: ./,./embedded/arinc653 */
//	#define __CMSIS__		/* Extra include: ./,./embedded/cmsis */
//	#define __embOS__		/* Extra include: ./,./embedded/embos */
//	#define __FreeRTOS__	/* Extra include: ./,./embedded/freertos */
//	#define __ITRON__		/* Extra include: ./,./embedded/itron */
	#define __MGOS__		/* Extra include: ./,./embedded/mgos,./embedded */
//	#define __MQXRTOS__		/* Extra include: ./,./embedded/mqx */
//	#define __Nucleus__		/* Extra include: ./,./embedded/nucleus */
//	#define __OSEK__		/* Extra include: ./,./embedded/osek */
//	#define __Quadros__		/* Extra include: ./,./embedded/quadros */
//	#define __RTEMS__		/* Extra include: ./,./embedded/rtems */
//	#define __SMX__			/* Extra include: ./,./embedded/smx/xsmx,./embedded/smx/xfs */
//	#define __Telit__		/* Extra include: ./,./embedded/telit */
//	#define __ThreadX__		/* Extra include: ./,./embedded/threadx */
//	#define __TKernel__		/* Extra include: ./,./embedded/tk */
//  #define __UCOS__		/* Extra include: ./,./embedded/ucos */
//	#define __VxWorks__		/* Extra include: ./,./embedded/vxworks/,./embedded/vxworks/wrn/coreip/ */

  /* Embedded OS additions (filesystems, networking).  Include directory 
     changes as before */
  #if defined( __MGOS__ )
	#define USE_LWIP		/* Extra include: ...,./embedded */
							/* LWIP uses absolute paths, so the 'lwip' in 
							   the path is part of the #include */
  #endif /* OSes that support LWIP */

  /* If we're using an embedded OS without support for a particular feature,
     disable it */
  #if defined( __ARINC653__ ) || defined( __ITRON__ ) || defined( __OSEK__ ) 
	#define CONFIG_NO_SESSIONS
  #endif /* Embedded OSes without built-in networking support */

  /* Undo Windows defines */
  #undef __WINDOWS__
  #undef __WIN32__
  #if !defined( __Nucleus__ ) && !defined( __SMX__ ) && !defined( __UCOS__ )
	#define BOOLEAN			FNORDIAN
  #endif /* Systems that typedef BOOLEAN */
  #ifdef __Nucleus__
	#undef FAR
  #endif /* Systems that define FAR */

  /* Embedded SDK-specific additional defines */
  #if defined( __VxWorks__ ) && !defined( _WRS_KERNEL )
	#define _WRS_KERNEL		1
  #endif /* SDK-specific defines */
  #if defined( __OSEK__ )
	/* OSEK uses statically-defined mutex IDs, these are normally set via 
	   the config tool but aren't available for cross-compile builds */
	#define initialisationMutex		0x01
	#define objectTableMutex		0x02
	#define semaphoreMutex			0x03
	#define mutex1Mutex				0x04
	#define mutex2Mutex				0x05
	#define mutex3Mutex				0x06
	#define allocationMutex			0x07
  #endif /* __OSEK__ */

  /* In addition '__i386__' (assuming gcc with an x86 target) needs to be 
     defined globally via Project Settings | C/C++ | Preprocessor.  This
	 is already defined for the 'Crosscompile' build configuration */
#endif /* Windows emulated cross-compile environment */

#ifdef _SCCTK
  #define __IBM4758__
#endif /* IBM 4758 cross-compiled under Windows */

/****************************************************************************
*																			*
*						Compiler Detection and Configuration				*
*																			*
****************************************************************************/

/* Visual C++ capabilities have changed somewhat over the years, the 
   following defines make explicit what we're testing for in a check of 
   _MSC_VER.

	Visual C++ 1.5 _MSC_VER = 800
	Visual C++ 2.0 _MSC_VER = 900
	Visual C++ 4.0 _MSC_VER = 1000
	Visual C++ 5.0 _MSC_VER = 1100
	Visual C++ 6.0 _MSC_VER = 1200
	Visual C++ 7.0 (VC++.NET/2002) _MSC_VER = 1300
	Visual C++ 7.1 (VC++.NET/2003) _MSC_VER = 1310
	Visual C++ 8.0 (VS2005) _MSC_VER = 1400 
	Visual C++ 9.0 (VS2008) _MSC_VER = 1500
	Visual C++ 10.0 (VS2010) _MSC_VER = 1600 
	Visual C++ 11.0 (VS2012) _MSC_VER = 1700
	Visual C++ 12.0 (VS2013) _MSC_VER = 1800 
	Visual C++ 14.0 (VS2015) _MSC_VER = 1900
	Visual C++ 14.1x (VS2017) _MSC_VER = 191x

   Starting with VS2017 and in line with the mainline Windows 10 update 
   cretinism, the version number is incremented from 1900 for every minor 
   update, starting at 1910 for Visual C++ 14.1 (VS2017) */

#ifdef _MSC_VER
  #define VC_16BIT( version )		( version <= 800 )
  #define VC_LE_VC6( version )		( version <= 1200 )
  #define VC_GE_2002( version )		( version >= 1300 )
  #define VC_LT_2005( version )		( version < 1400 )
  #define VC_GE_2005( version )		( version >= 1400 )
  #define VC_GE_2008( version )		( version >= 1500 )
  #define VC_LT_2010( version )		( version < 1600 )
  #define VC_GE_2010( version )		( version >= 1600 )
  #define VC_GE_2012( version )		( version >= 1700 )
  #define VC_GE_2013( version )		( version >= 1800 )
  #define VC_GE_2015( version )		( version >= 1900 )
  #define VC_LT_2017( version )		( version < 1910 )
  #define VC_GE_2017( version )		( version >= 1910 )
#else
  /* These aren't specifically required on non-VC++ systems, but some 
     preprocessors get confused if they aren't defined */
  #define VC_16BIT( version )		0
  #define VC_LE_VC6( version )		0
  #define VC_GE_2002( version )		0
  #define VC_LT_2005( version )		0
  #define VC_GE_2005( version )		0
  #define VC_GE_2008( version )		0
  #define VC_GE_2010( version )		0
  #define VC_GE_2012( version )		0
  #define VC_GE_2013( version )		0
  #define VC_GE_2015( version )		0
  #define VC_LT_2017( version )		0
  #define VC_GE_2017( version )		0
#endif /* Visual C++ */

/* If we're compiling under VC++ with the maximum level of warnings, turn
   off some of the more irritating warnings */

#if defined( _MSC_VER )
  #if VC_16BIT( _MSC_VER )
	#pragma warning( disable: 4135 )/* Conversion bet.diff.integral types */
	#pragma warning( disable: 4761 )/* Integral size mismatch in argument */
  #endif /* 16-bit VC++ */

  /* Warning level 3:
  
	 4018: Comparing signed <-> unsigned value.  The compiler has to convert 
		   the signed value to unsigned to perform the comparison.  This 
		   leads to around 25 false-positive warnings.  Note that this is
		   a variant of the VC++ 2005-only warning 4267, this one warns
		   about comparing the result of a sizeof() operation to an int and
		   4267 warns about size_t types in general */
  #pragma warning( disable: 4018 )	/* Comparing signed <-> unsigned value */

  /* Warning level 4:

	 4054, 4055: Cast from function pointer -> generic (data) pointer, cast 
		   from generic (data) pointer -> function pointer.  These are 
		   orthogonal and impossible to disable as they override the 
		   universal 'void *' pointer type.

	 4057: Different types via indirection.  An annoying dual-purpose 
		   warning that leads to huge numbers of false positives for 
		   'char *' vs. 'unsigned char *' (for example due to a PKCS #11 
		   token label, declared as 'unsigned char *', being passed to a 
		   string function, these are pretty much un-fixable as 'char'
		   vs. 'unsigned char's percolate up and down the code tree), 
		   but that also provides useful warnings of potential problems 
		   (for example 'int *' passed to function expecting 'long *').

	 4204, 4221: Struct initialised with non-const value, struct initialised 
		   with address of automatic variable.  Standards extensions that 
		   the struct STATIC_INIT macros manage for us.

	 4206: Empty C module due to #ifdef'd out code.  Annoying noise caused 
		   by empty modules due to disabled functionality.

	 The only useful ones are 4057, which can be turned off on a one-off 
	 basis to identify new true-positive issues before being disabled again 
	 to avoid all of the false-positives, currently 100 for 4057 */
  #pragma warning( disable: 4054 )	/* Cast from fn.ptr -> generic (data) ptr.*/
  #pragma warning( disable: 4055 )	/* Cast from generic (data) ptr. -> fn.ptr.*/
  #pragma warning( disable: 4057 )	/* Different types via indirection */
  #pragma warning( disable: 4204 )	/* Struct initialised with non-const value */
  #pragma warning( disable: 4206 )	/* Empty C module due to #ifdef'd out code */
  #pragma warning( disable: 4221 )	/* Struct initialised with addr.of auto.var */
  #if VC_GE_2005( _MSC_VER )
	#pragma warning( disable: 4267 )/* int <-> size_t */
  #endif /* VC++ 2005 or newer */

  /* Different versions of VC++ generates extra warnings at level 4 due to 
	 problems in VC++/Platform SDK headers */
  #pragma warning( disable: 4201 )/* Nameless struct/union in SQL/networking hdrs*/
  #if VC_GE_2005( _MSC_VER )
	#pragma warning( disable: 4214 )/* bit field types other than int */
  #endif /* VC++ 2005 or newer */

  /* Code analysis generates even more warnings.  C6011 is particularly 
	 problematic, it's issued whenever a pointer is derefenced without first
	 checking that it's not NULL, which makes it more or less unusable */
  #if defined( _MSC_VER ) && defined( _PREFAST_ ) 
	#pragma warning( disable: 6011 )/* Deferencing NULL pointer */
  #endif /* VC++ with source analysis enabled */

  /* Windows DDK free builds treat warnings as errors and the DDK headers 
	 have some problems so we have to disable additional warnings */
  #ifdef WIN_DDK
	#pragma warning( disable: 4242 )/* MS-only bit field type used */
	#pragma warning( disable: 4731 )/* Frame pointer modified by inline asm */
  #endif /* WIN_DDK */

  /* gcc -wall type warnings.  The highest warning level generates large
	 numbers of spurious warnings (including ones in VC++ headers), so it's
	 best to only enable them for one-off test builds requiring manual
	 checking for real errors */
  #pragma warning( disable: 4100 )	/* Unreferenced parameter */
#endif /* Visual C++ */

/* Under VC++/VS a number of warnings are disabled by default, including 
   some potentially useful ones, so we re-enable them.  The warnings are
   listed as "Compiler Warnings That Are Off by Default", currently at
   https://msdn.microsoft.com/en-us/library/23k5d385.aspx.

	C4242 'identifier': conversion from 'type1' to 'type2', possible loss of 
		  data.
	C4255 'function': no function prototype given: converting '()' to 
		  '(void)'.
	C4287 'operator': unsigned/negative constant mismatch.
	C4296 'operator': expression is always false.
	C4302 'conversion' : truncation from 'type 1' to 'type 2'.
	C4311 'variable' : pointer truncation from 'type' to 'type'.
	C4312 'operation' : conversion from 'type1' to 'type2' of greater size,
		  assigning a 32-bit type to a 64-bit pointer.
	C4431 missing type specifier - int assumed.
	C4545 expression before comma evaluates to a function which is missing 
		  an argument list.
	C4546 function call before comma missing argument list.
	C4547 'operator' : operator before comma has no effect; expected 
		  operator with side-effect.
	C4548 expression before comma has no effect; expected expression with 
		  side-effect.
	C4549 'operator' : operator before comma has no effect; did you intend 
		  'operator'?
	C4555 expression has no effect; expected expression with side-effect.
	C4557 '__assume' contains effect 'effect'.
	C4574 'identifier' is defined to be '0': did you mean to use '#if 
		  identifier'?
	C4619 #pragma warning : there is no warning number 'number'.
	C4628 digraphs not supported with -Ze. Character sequence 'digraph' 
		  not interpreted as alternate token for 'char'.
	C4668 'symbol' is not defined as a preprocessor macro, replacing with 
		  '0' for 'directives'.  
		  Note that enabling this check causes warnings in Windows header 
		  files.
	C4826 Conversion from 'type1 ' to 'type2' is sign-extended.
	C4837 trigraph detected: '??%c' replaced by '%c'.

   These are all defined only for newer versions of VC++ (2005 and up) so 
   they need a recent compiler in order to be evaluated.

   These versions also have the potentially useful warning 'C4390 empty 
   controlled statement found' (e.g. 'if( i );'), on by default for W3 and 
   above, however this warning has no effect in either VS 2005 or VS 2010,
   it seems to be triggered by random misplaced semicolons rather than the
   presence of empty controlled statements, e.g. 'if( foo; )' */

#if defined( _MSC_VER )
  #pragma warning( 3: 4242 )
  #pragma warning( 3: 4255 )
  #pragma warning( 3: 4287 )
  #pragma warning( 3: 4296 )
  #pragma warning( 3: 4302 )
  #pragma warning( 3: 4311 )
  #pragma warning( 3: 4312 )
  #pragma warning( 3: 4431 )
  #pragma warning( 3: 4545 )
  #pragma warning( 3: 4546 )
  #pragma warning( 3: 4547 )
  #pragma warning( 3: 4548 )
  #pragma warning( 3: 4549 )
  #pragma warning( 3: 4555 )
  #pragma warning( 3: 4557 )
  #pragma warning( 3: 4574 )
  #pragma warning( 3: 4619 )
  #pragma warning( 3: 4628 )
  #pragma warning( 3: 4668 )
  #pragma warning( 3: 4826 )
  #pragma warning( 3: 4837 )
#endif /* Visual C++ */

/* VC++ 2005 implements the TR 24731 security extensions but doesn't yet 
   define __STDC_LIB_EXT1__, so if we detect this version of the compiler we 
   define it ourselves */

#if defined( _MSC_VER ) && VC_GE_2005( _MSC_VER ) && \
	!defined( __STDC_LIB_EXT1__ )
  #define __STDC_LIB_EXT1__
#endif /* VC++ 2005 without __STDC_LIB_EXT1__ defined */

/* The ability to modify warnings via the project file in BC++ 5.0x is
   completely broken, the only way to do this is via pragmas in the source
   code */

#if defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x550 )
  /* Spurious warnings to disable */
  #pragma warn -aus						/* Assigned but never used.  This is
										   frequently misreported even when
										   the value is quite obviously used */
  #pragma warn -csu						/* Comparing signed/unsigned value */
  #pragma warn -par						/* Parameter is never used	*/
  #pragma warn -sig						/* Conversion may lose significant digits */
  #pragma warn -ucp						/* Signed/unsigned char assignment */

  /* Useful warnings to enable */
  #pragma warn +amb						/* Ambiguous operators need parentheses */
  #pragma warn +amp						/* Superfluous & with function */
  #pragma warn +asm						/* Unknown assembler instruction */
  #pragma warn +ccc						/* Condition is always true/false */
  #pragma warn +cln						/* Constant is long */
  #pragma warn +def						/* Use of ident before definition */
  #pragma warn +stv						/* Structure passed by value */
#endif /* Broken BC++ 5.0x warning handling */

/* The TI compiler needs to have a few annoying warnings disabled */

#if defined( __TI_COMPILER_VERSION__ )
  #pragma diag_suppress 190				/* enum vs. int */
#endif /* TI compiler */

/* The IAR compiler warns about all manner of pointless stuff that seems to
   be motivated more by the compiler developers' desire to show off how 
   smart they are than any real help it provides with code development.  
   This includes things that don't have any effect on compiling (e.g. CRLF 
   vs. LF line endings) or are a mandatory part of the C standard (type 1 
   promoted to type 2).  As an added benefit it includes warnings that can 
   never be disabled even if it seems that they can.  
   
   We try and disable the more annoying and pointless ones here, but the 
   fact that there's a diag_suppress for a particular warning doesn't mean 
   that it'll actually get suppressed */

#ifdef __IAR_SYSTEMS_ICC__
  #pragma diag_suppress=Pa050			/* LF vs. CRLF line endings */
  #pragma diag_suppress=Pa084			/* Result of comparison always true */
  #pragma diag_suppress=Pa118			/* Mixing bool.and non-bool in a comparison */
  #pragma diag_suppress=Pe167			/* char * vs. unsigned char * */
  #pragma diag_suppress=Pe186			/* Compare unsigned using '>= 0' */
  #pragma diag_suppress=Pe188			/* int vs. enum */
#endif /* IAR */

/* All Windows CE functions are Unicode-only, this was an attempt to clean
   up the ASCII vs. Unicode kludges in Win32 but unfortunately was made just
   before UTF8 took off.  Because UTF8 allows everyone to keep using their
   old ASCII stuff while being nominally Unicode-aware, it's unlikely that
   any new Unicode-only systems will appear in the future, leaving WinCE's
   Unicode-only API an orphan.  The easiest way to handle this is to convert
   all strings to ASCII/8 bit as they come in from the external cryptlib API
   and convert them back to Unicode as required when they're passed to WinCE
   OS functions.  In other words Unicode is treated just like EBCDIC and
   pushed out to the edges of cryptlib.  This requires the minimum amount of
   conversion and special-case handling internally */

#ifdef __WINCE__
  #define UNICODE_CHARS
#endif /* WinCE */

/* Symbian has rather inconsistent defines depending in which toolchain 
   we're using, with the original ARM tools the define was __SYMBIAN32__
   with __MARM__ for the ARM architecture, with the ex-Metrowerks Nokia
   compiler the define is __EMU_SYMBIAN_OS__ for the emulated environment
   and who knows what for the gcc toolchain.  To make checking easier we
   require __SYMBIAN32__ for all environments, with __MARM__ vs.
   __EMU_SYMBIAN_OS__ distinguishing between ARM and x86 emulator */

#if defined( __EMU_SYMBIAN_OS__ ) && !defined( __SYMBIAN32__ )
  #error Need to define '__SYMBIAN32__' for the Symbian build
#endif /* __EMU_SYMBIAN_OS__ && !__SYMBIAN32__ */
#if defined( __SYMBIAN32__ ) && \
	!( defined( __MARM__ ) || defined( __EMU_SYMBIAN_OS__ ) )
  #error Need to define a Symbian target architecture type, e.g. ARM or x86
#endif /* __SYMBIAN32__ && !( __MARM__ || __EMU_SYMBIAN_OS__ ) */

/* Some systems (typically 16-bit or embedded ones) have rather limited
   amounts of memory available, if we're building on one of these we limit
   the size of some of the buffers that we use and the size of the object
   table */

#if defined( __MSDOS16__ ) || defined( __uClinux__ )
  #define CONFIG_CONSERVE_MEMORY
  #define CONFIG_NUM_OBJECTS		128
#endif /* Memory-starved systems */

/* Since the Win32 randomness-gathering uses a background randomness polling
   thread, we can't build a Win32 version with NO_THREADS */

#if defined( __WIN32__ ) && defined( NO_THREADS )
  #error The Win32 version of cryptlib must have threading enabled
#endif /* Win32 without threading */

/* Enable use of assembly-language alternatives to C functions if possible.
   Note that the following asm defines are duplicated in crypt/osconfig.h,
   because the OpenSSL headers are non-orthogonal to the cryptlib ones.  Any 
   changes made here need to be reflected in osconfig.h */

#if defined( __WIN32__ ) && \
	!( defined( __WIN64__ ) || defined( __BORLANDC__ ) || defined( NO_ASM ) )
  /* Enable use of the AES ASM code */
  #define AES_ASM
#endif /* Win32 */

/* Alongside the general crypto asm code there's also inline asm to handle 
   things like CPU hardware features, if we're running under Win64 we have 
   to disable this as well */

#if defined( __WIN64__ )
  #define NO_ASM
#endif /* Win64 */

/* On systems that support dynamic loading, we bind various drivers and
   libraries at runtime rather than at compile time.  Under Windows this is
   fairly easy but under Unix it's supported somewhat selectively and may be
   buggy or platform-specific */

#if defined( __WINDOWS__ ) || \
	( defined( __UNIX__ ) && \
	  ( ( defined( sun ) && OSVERSION > 4 ) || defined( __linux__ ) || \
		defined( _AIX ) || ( defined( __APPLE__ ) && !defined( __MAC__ ) ) ) ) || \
	defined( __ANDROID__ )
  #define DYNAMIC_LOAD
#endif /* Systems that support dynamic loading */

/****************************************************************************
*																			*
*								Endianness Defines							*
*																			*
****************************************************************************/

/* If the endianness isn't predefined and the compiler can tell us what
   endianness we've got, use this in preference to all other methods.  This
   is only really necessary on non-Unix systems since the makefile runtime
   test will tell us the endianness under Unix */

#ifdef __GNUC__
  /* Apple and NetBSD do it differently to everyone else in the universe */
  #if defined( __APPLE__ )
	#include <machine/endian.h>
  #elif defined( __NetBSD__ )
	#include <sys/endian.h>
  #else
	#include <endian.h>
  #endif /* Apple vs. everyone else */
#endif /* GCC */

#if defined( CONFIG_DATA_LITTLEENDIAN ) || defined( CONFIG_DATA_BIGENDIAN )
  /* If we're cross-compiling for another system, the endianness auto-
	 detection will have been overridden.  In this case we force it to be
	 what the user has specified rather than what we've auto-detected */
  #undef DATA_LITTLEENDIAN
  #undef DATA_BIGENDIAN
  #ifdef CONFIG_DATA_LITTLEENDIAN
	#define DATA_LITTLEENDIAN
  #else
	#define DATA_BIGENDIAN
  #endif /* CONFIG_DATA_LITTLEENDIAN */
#endif /* Forced big vs.little-endian */

#if !defined( DATA_LITTLEENDIAN ) && !defined( DATA_BIGENDIAN )
  #if defined( BIG_ENDIAN ) && defined( LITTLE_ENDIAN ) && defined( BYTE_ORDER )
	/* Some systems define both BIG_ENDIAN and LITTLE_ENDIAN, then define
	   BYTE_ORDER to the appropriate one, so we check this and define the
	   appropriate value */
	#if ( BYTE_ORDER == BIG_ENDIAN ) && !defined( DATA_BIGENDIAN )
	  #define DATA_BIGENDIAN
	#elif ( BYTE_ORDER == LITTLE_ENDIAN ) && !defined( DATA_LITTLEENDIAN )
	  #define DATA_LITTLEENDIAN
	#else
	  #error BYTE_ORDER is neither BIG_ENDIAN nor LITTLE_ENDIAN
	#endif /* BYTE_ORDER-specific define */
  #elif defined( __BYTE_ORDER__ ) && \
		( __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ )
	#define DATA_BIGENDIAN
  #elif defined( __BYTE_ORDER__ ) && \
		( __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )
	#define DATA_LITTLEENDIAN
  #elif defined( __LITTLE_ENDIAN__ )
	#if __LITTLE_ENDIAN__ == 1
	  #define DATA_LITTLEENDIAN
	#else
	  #define DATA_BIGENDIAN
	#endif /* __LITTLE_ENDIAN__ */
  #elif defined( __i386 ) || defined( __i386__ ) || defined( __i486__ ) || \
		defined( __i586__ ) || defined( __i686__ ) || defined( _M_I86 ) || \
		defined( _M_IX86 ) || defined( _M_X64 ) || defined( __amd64__ ) || \
		defined( __x86_64__ ) || defined( _M_AMD64 ) || \
		defined( __TURBOC__ ) || defined( __OS2__ )
	#define DATA_LITTLEENDIAN	/* Intel architecture always little-endian */
  #elif defined( __WINCE__ )
	/* For WinCE it can get a bit complicated, however because of x86 cargo
	   cult programming WinCE systems always tend to be set up in little-
	   endian mode */
	#define DATA_LITTLEENDIAN	/* Intel architecture always little-endian */
  #elif defined( __sparc ) || defined( __sparc__ ) 
	#define DATA_BIGENDIAN		/* Sparc always big-endian */
  #elif defined( _ARCH_PPC ) || defined( _ARCH_PPC64 ) || \
		defined( __powerpc ) || defined( __powerpc__ ) || \
		defined( __ppc__ )
	#define DATA_BIGENDIAN		/* PowerPC always big-endian */
  #elif defined( __AARCH64EB__ ) || defined( __ARMEB__ ) || \
		defined( __MIPSEB ) || defined( __MIPSEB__ ) || \
		defined( _MIPSEB ) || defined( __THUMBEB__ )
	#define DATA_BIGENDIAN		/* ARM/MIPS explicit big-endian */
  #elif defined( __AARCH64EL__ ) || defined( __ARMEL__ ) || \
		defined( __MIPSEL ) || defined( __MIPSEL__ ) || \
		defined( _MIPSEL ) || defined( __THUMBEL__ )
	#define DATA_LITTLEENDIAN	/* ARM/MPIS explicit little-endian */
  #elif defined( AMIGA ) || defined( __MWERKS__ ) || defined( SYMANTEC_C ) || \
		defined( THINK_C ) || defined( applec ) || defined( __MRC__ )
	#define DATA_BIGENDIAN		/* Motorola architecture always big-endian */
  #elif defined( VMS ) || defined( __VMS )
	#define DATA_LITTLEENDIAN	/* VAX architecture always little-endian */
  #elif defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ )
	#define DATA_BIGENDIAN		/* Tandem architecture always big-endian */
  #elif defined( __AS400__ ) || defined( __VMCMS__ ) || defined( __MVS__ )
	#define DATA_BIGENDIAN		/* IBM big iron always big-endian */
  #elif defined( __SYMBIAN32__ ) && \
		( defined( __MARM__ ) || defined( __EMU_SYMBIAN_OS__ ) )
	#define DATA_LITTLEENDIAN	/* Symbian on ARM/x86 always little-endian */
  #elif defined( __Nucleus__ ) && defined( __RVCT2_1__ )
	#if defined( __BIG_ENDIAN )	/* Realview for Nucleus */
	  #define DATA_BIGENDIAN
	#else
	  #define DATA_LITTLEENDIAN
	#endif /* Big vs.little-endian */
  #elif defined( __m68k__  )
	#define DATA_BIGENDIAN		/* 68K always big-endian */
  #elif defined( __TI_COMPILER_VERSION__ )
	/* The TI compiler can masquerade as gcc so we need to check for it 
	   before we check for Gnu indicators */
	#if CPU_BYTE_ORDER == LOW_BYTE_FIRST
	  #define DATA_LITTLEENDIAN
	#elif CPU_BYTE_ORDER == HIGH_BYTE_FIRST
	  #define DATA_BIGENDIAN
	#else
	  #error Couldnt get endianness from CPU_BYTE_ORDER
	#endif /* TI compiler endianness detection */
  #elif defined __GNUC__
	#ifdef BYTES_BIG_ENDIAN
	  #define DATA_BIGENDIAN	/* Big-endian byte order */
	#else
	  #define DATA_LITTLEENDIAN	/* Undefined = little-endian byte order */
	#endif /* __GNUC__ */
  #endif /* Compiler-specific endianness checks */
#endif /* !( DATA_LITTLEENDIAN || DATA_BIGENDIAN ) */

/* The last-resort method.  Thanks to Shawn Clifford
   <sysop@robot.nuceng.ufl.edu> for this trick.

   NB: A number of compilers aren't tough enough for this test */

#if !defined( DATA_LITTLEENDIAN ) && !defined( DATA_BIGENDIAN )
  #if ( ( ( unsigned short ) ( 'AB' ) >> 8 ) == 'B' )
	#define DATA_LITTLEENDIAN
  #elif ( ( ( unsigned short ) ( 'AB' ) >> 8 ) == 'A' )
	#define DATA_BIGENDIAN
  #else
	#error "Cannot determine processor endianness.  Edit misc/os_spec.h and recompile"
  #endif /* Endianness test */
#endif /* !( DATA_LITTLEENDIAN || DATA_BIGENDIAN ) */

/* Sanity check to catch both values being defined */

#if defined( DATA_LITTLEENDIAN ) && defined( DATA_BIGENDIAN )
  #error Both DATA_LITTLEENDIAN and DATA_BIGENDIAN are defined
#endif /* DATA_LITTLEENDIAN && DATA_BIGENDIAN */

#endif /* _OSDETECT_DEFINED */
