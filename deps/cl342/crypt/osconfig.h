#ifndef _OSCONFIG_DEFINED
#define _OSCONFIG_DEFINED

/* Pull in cryptlib-wide configuration options */

#include "misc/config.h"

/* OpenSSL-specific defines */

#define OPENSSL_EXTERN	extern
#define OPENSSL_GLOBAL
#if defined( _WINDOWS ) && !defined( WINDOWS )	/* Windows */
  #define WINDOWS				/* Old format */
  #define OPENSSL_SYS_WINDOWS	/* New fomat */
#endif /* OpenSSL Windows not defined */
#if defined( _WIN32 )			/* Win32 and WinCE */
  #ifndef WIN32
	#define WIN32				/* Old format OpenSSL Win32 identifier */
  #endif /* WIN32 */
  #define OPENSSL_SYS_WIN32		/* New format OpenSSL Win32 identifier */
  /* Note that the following asm defines are duplicated in misc/os_spec.h, 
	 because the OpenSSL headers are non-orthogonal to the cryptlib ones. 
	 Any changes made here need to be reflected in os_spec.h */
  #if !( defined( _WIN32_WCE ) || defined( _M_X64 ) || \
		 defined( __BORLANDC__ ) || defined( NO_ASM ) )
	#define USE_ASM				/* Always enabled for x86 Win32 */
  #endif /* WinCE || x86-64 || Borland compilers */
#endif /* OpenSSL Win32 not defined */
#include <stdlib.h>			/* For malloc() */
#include <string.h>			/* For memset() */
#ifdef USE_ASM				/* Defined via makefile for Unix systems */
  #define BN_ASM
  #define MD5_ASM
  #define RMD160_ASM
  #define SHA1_ASM
#endif /* USE_ASM */
#if defined( USE_ASM ) && defined( __WATCOMC__ )
  #define ASM_EXPORT	__cdecl
#else
  #define ASM_EXPORT
#endif /* System-specific interface to ASM files */

/* General defines.  A generic config from the original OpenSSL version can 
   be found at http://lists.alioth.debian.org/pipermail/pkg-openssl-changes/-
   2005-October/000012.html */

#include <limits.h>
#if ULONG_MAX > 0xFFFFFFFFUL
  #define SIXTY_FOUR_BIT
#else
  #define THIRTY_TWO_BIT
#endif /* Machine word size */

#if defined( _MSC_VER )
  /* cryptlib is built with the highest warning level, disable some of the
     more irritating warnings produced by the OpenSSL code */
  #pragma warning( disable: 4244 )	/* int <-> unsigned char/short */
  #pragma warning( disable: 4100 )	/* Unreferenced parameter */
  #pragma warning( disable: 4127 )	/* Conditional is constant: while( TRUE ) */
#endif /* Visual C++ */

/* Aches */
#ifdef _AIX
  #define B_ENDIAN
  #define BN_LLONG
  #define RC4_CHAR
#endif /* AIX */

/* Alpha */
#if defined( __osf__ ) || defined( __alpha__ )
  #define L_ENDIAN
  #define SIXTY_FOUR_BIT_LONG
  #define DES_INT
  #define DES_UNROLL
  #define DES_RISC1
  #define RC4_CHUNK
#endif /* Alpha */

/* BeOS */
#ifdef __BEOS__
  #if defined( __i386__ )
	#define L_ENDIAN
	#define BN_LLONG
	#define DES_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_INDEX
  #elif defined( __ppc__ )
	#define B_ENDIAN
	#define BN_LLONG
	#define BF_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_CHAR
	#define RC4_CHUNK
  #else
	#error Need to define CPU type for non-x86/non-PPC BeOS
  #endif /* BeoS variants */
#endif /* BeOS */

/* The BSDs and Linux.  For low-level code-generation purposes these are 
   identical, even if they differ at a higher level */
#if defined( __FreeBSD__ ) || defined( __bsdi__ ) || \
	defined( __OpenBSD__ ) || defined( __NetBSD__ ) || \
	defined( __linux__ )
  #if defined( __x86_64__ ) || defined( __amd64__ )
	/* 64-bit x86 has both 'long' and 'long long' as 64 bits.  In addition
	   we use DES_INT since int's are 64-bit.  We have to check for the
	   64-bit x86 variants before the generic ones because they're a
	   variation on the generics (e.g. AMD64 defines both __athlon__ and
	   __x86_64__, so if we checked for __athlon__ first we'd identify it
	   as a generic rather than 64-bit build) */
	#define L_ENDIAN
	#undef SIXTY_FOUR_BIT
	#define SIXTY_FOUR_BIT_LONG
	#define DES_INT
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_INDEX
  #elif defined( __i386__ )
	#define L_ENDIAN
	#define BN_LLONG
	#define DES_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_INDEX
  #elif defined( __arm ) || defined( __arm__ )
	#ifdef DATA_BIGENDIAN
	  #define B_ENDIAN
	#else
	  #define L_ENDIAN
	#endif	/* Usually little-endian but may be big-endian */
	#define BN_LLONG
	#define DES_RISC1
  #elif defined( __mips__ )
	#ifdef DATA_BIGENDIAN
	  #define B_ENDIAN
	#else
	  #define L_ENDIAN
	#endif	/* Usually little-endian but may be big-endian */
	#define BN_LLONG
	#define BF_PTR
	#define DES_RISC2
	#define DES_PTR
	#define DES_UNROLL
	#define RC4_INDEX
	#define RC4_CHAR
	#define RC4_CHUNK
  #elif defined( __ppc__ ) || defined( __powerpc__ )
	#ifdef DATA_LITTLEENDIAN
	  #define L_ENDIAN
	#else
	  #define B_ENDIAN
	#endif	/* Usually big-endian but may be little-endian */
	#define BN_LLONG
	#define BF_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_CHAR
	#define RC4_CHUNK
  #elif defined( __hppa__ )
	#define B_ENDIAN
	#define BN_DIV2W
	#define BN_LLONG
	#define DES_PTR
	#define DES_UNROLL
	#define DES_RISC1
	#define MD32_XARRAY
  #elif defined( __sparc__ )
	#define B_ENDIAN
	#define BN_DIV2W
	#define BN_LLONG
	#define BF_PTR
	#define DES_UNROLL
	#define RC4_CHAR
	#define RC4_CHUNK
  #elif defined( __sh__ )
	/* Super-H has defines for subtypes, __sh1__ to __sh3__ and then 
	   __SH3__ to __SH5__, but we treat them all as the same general 
	   architecture.
	   
	   There isn't any official config for Super-H (specifically SH4), the 
	   following is the config for MIPS which seems to work OK (the only
	   one that really matters is BN_LLONG which is generic for any 32-bit
	   CPU, Blowfish and RC4 are disabled by default and 3DES isn't used
	   much any more */
	#ifdef DATA_BIGENDIAN
	  #define B_ENDIAN
	#else
	  #define L_ENDIAN
	#endif	/* Usually little-endian but may be big-endian */
	#define BN_LLONG
	#define BF_PTR
	#define DES_RISC2
	#define DES_PTR
	#define DES_UNROLL
	#define RC4_INDEX
	#define RC4_CHAR
	#define RC4_CHUNK
  #else
	#error Need to define CPU type for non-x86/Arm/MIPS/PA-Risc/PPC/Sparc Linux
  #endif /* *BSD/Linux variants */
#endif /* *BSD/Linux */
#if defined( __LINUX__ ) && defined( __WATCOMC__ )
  #define L_ENDIAN
  #define BN_LLONG
  #define RC4_INDEX
#endif /* Linux */

/* Cray Unicos */
#ifdef _CRAY
  /* Crays are big-endian, but if B_ENDIAN is defined the code implicitly
     assumes 32-bit ints whereas Crays have 64-bit ints and longs.  However,
     the non-B/L_ENDIAN code happens to work, so we don't define either */
  #undef SIXTY_FOUR_BIT
  #define SIXTY_FOUR_BIT_LONG
  #define DES_INT
#endif /* Cray Unicos */

/* DGUX */
#ifdef __dgux
  #define L_ENDIAN
  #define RC4_INDEX
  #define DES_UNROLL
#endif /* DGUX */

/* DOS */
#if defined( MSDOS ) || defined( __MSDOS__ )
  #if defined(__WATCOMC__)
	/* 32-bit DOS */
	#define L_ENDIAN
	#define BN_LLONG
	#define RC4_INDEX
  #else
	/* 16-bit DOS */
	#define L_ENDIAN
	#define BN_LLONG
	#define MD2_CHAR
	#define DES_UNROLL
	#define DES_PTR
	#define RC4_INDEX
	#undef THIRTY_TWO_BIT
	#define SIXTEEN_BIT
  #endif /* 16- vs.32-bit DOS */
#endif /* DOS */

/* Irix */
#ifdef __sgi

  /* Irix 5.x and lower */
  #if ( OSVERSION <= 5 )
	#define B_ENDIAN
	#define BN_LLONG
	#define BF_PTR
	#define DES_RISC2
	#define DES_PTR
	#define DES_UNROLL
	#define RC4_INDEX
	#define RC4_CHAR
	#define RC4_CHUNK
	#define MD2_CHAR

  /* Irix 6.x and higher */
  #else
	#define B_ENDIAN
	#define BN_DIV3W
	#define MD2_CHAR
	#define RC4_INDEX
	#define RC4_CHAR
	#define RC4_CHUNK_LL
	#define DES_UNROLL
	#define DES_RISC2
	#define DES_PTR
	#define BF_PTR
	#define SIXTY_FOUR_BIT
	/* Pure 64-bit should also define SIXTY_FOUR_BIT_LONG */
  #endif /* Irix versions */
#endif /* Irix */

/* Mac */
#if defined( __MWERKS__ ) || defined( SYMANTEC_C ) || defined( __MRC__ )
  #define B_ENDIAN
  #define BN_LLONG
  #define BF_PTR
  #define DES_UNROLL
  #define RC4_CHAR
  #define RC4_CHUNK
#endif /* Mac */

/* Mac OS X / iOS */
#if defined( __APPLE__ ) && !defined( __MAC__ )
  #include <TargetConditionals.h>
  #if defined( TARGET_OS_IPHONE ) || defined( TARGET_IPHONE_SIMULATOR )
	#define L_ENDIAN
	#define BN_LLONG
	#define DES_RISC1
  #elif defined( __ppc__ )
	#define B_ENDIAN
	#define BN_LLONG
	#define BF_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_CHAR
	#define RC4_CHUNK
  #else
	#define L_ENDIAN
	#define BN_LLONG
	#define DES_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_INDEX
  #endif /* Mac OS variants */
#endif /* Mac OS X / iOS */

/* MSDOS */
#ifdef __MSDOS__
  #define L_ENDIAN
  #define BN_LLONG
  #define MD2_CHAR
  #define DES_UNROLL
  #define DES_PTR
  #define RC4_INDEX
  #undef THIRTY_TWO_BIT
  #define SIXTEEN_BIT
#endif /* __MSDOS__ */

/* MVS */
#ifdef __MVS__
  #define B_ENDIAN
#endif /* MVS */

/* NCR MP-RAS */
#ifdef __UNIX_SV__
  #define L_ENDIAN
  #define BN_LLONG
  #define DES_PTR
  #define DES_RISC1
  #define DES_UNROLL
  #define RC4_INDEX
#endif /* UNIX_SV */

/* Nucleus */
#if defined( __Nucleus__ )
  #ifdef DATA_BIGENDIAN
	#define B_ENDIAN
  #else
	#define L_ENDIAN
  #endif /* Big vs.little-endian */
  #define BN_LLONG
  #define DES_RISC1
#endif /* __Nucleus__ */

/* Palm OS: ARM */
#if defined( __PALMSOURCE__ )
  #if defined( __arm ) || defined( __arm__ )
	#define L_ENDIAN
	#define BN_LLONG
	#define DES_RISC1
  #else
	#error Need to define architecture-specific values for crypto code
  #endif /* Palm OS variants */
#endif /* Palm OS */

/* PHUX */
#ifdef __hpux

  /* PHUX 9.x (some versions report it as 09 so we also check for 0) */
  #if ( OSVERSION == 0 || OSVERSION == 9 )
	#define B_ENDIAN
	#define BN_DIV2W
	#define BN_LLONG
	#define DES_PTR
	#define DES_UNROLL
	#define DES_RISC1
	#define MD32_XARRAY

  /* PHUX 10.x, 11.x */
  #else
	#define B_ENDIAN
	#define BN_DIV2W
	#define BN_LLONG
	#define DES_PTR
	#define DES_UNROLL
	#define DES_RISC1
	#define MD32_XARRAY
	/* Pure 64-bit should also define SIXTY_FOUR_BIT_LONG MD2_CHAR RC4_INDEX
	   RC4_CHAR DES_INT */
  #endif /* PHUX versions */
#endif /* PHUX */

/* QNX */
#ifdef __QNX__
  #define L_ENDIAN
  #define BN_LLONG
  #define DES_PTR
  #define DES_RISC1
  #define DES_UNROLL
  #define RC4_INDEX
  #if OSVERSION <= 4
	/* The Watcom compiler can't handle 64-bit ints even though the hardware
	   can, so we have to build it as 16-bit code with 16x16 -> 32 multiplies
	   rather than 32x32 -> 64 */
	#undef THIRTY_TWO_BIT
	#define SIXTEEN_BIT
  #endif /* QNX 4.x */
#endif /* QNX */

/* SCO/UnixWare */
#ifdef __SCO_VERSION__

  /* SCO gcc */
  #if defined( __GNUC__ )
	#define L_ENDIAN
	#define BN_LLONG
	#define DES_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_INDEX

  /* SCO cc */
  #else
    #define L_ENDIAN
	#define BN_LLONG
	#define DES_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_INDEX
	#define MD2_CHAR
  #endif /* SCO	gcc/cc */
#endif /* SCO */

/* Solaris */
#ifdef sun

  /* Solaris Sparc */
  #ifdef sparc

	/* Solaris Sparc gcc */
	#if defined( __GNUC__ )
	  #define B_ENDIAN
	  #define BN_DIV2W
	  #define BN_LLONG
	  #define BF_PTR
	  #define DES_UNROLL
	  #define RC4_CHAR
	  #define RC4_CHUNK

	/* Solaris Sparc Sun C */
	#else
	  #define B_ENDIAN
	  #define BN_DIV2W
	  #define BN_LLONG
	  #define BF_PTR
	  #define DES_PTR
	  #define DES_RISC1
	  #define DES_UNROLL
	  #define RC4_CHAR
	  #define RC4_CHUNK
	  /* Pure 64-bit should also define SIXTY_FOUR_BIT_LONG and DES_INT */
	#endif /* Solaris Sparc */

  /* Solaris x86 */
  #else

	/* Solaris x86 gcc */
	#if defined( __GNUC__ )
	  #define L_ENDIAN
	  #define BN_LLONG
	  #define DES_PTR
	  #define DES_RISC1
	  #define DES_UNROLL
	  #define RC4_INDEX

	/* Solaris x86 Sun C */
	#else
	  #define L_ENDIAN
	  #define BN_LLONG
	  #define BF_PTR
	  #define DES_PTR
	  #define DES_UNROLL
	  #define RC4_CHAR
	  #define RC4_CHUNK
	#endif /* Solaris x86 */
  #endif /* Solaris Sparc vs x86 */
#endif /* Slowaris */

/* Symbian OS: Usually ARM, but we may be running under the x86 emulator */
#if defined( __SYMBIAN32__ )
  #if defined( __MARM__ )
	#define L_ENDIAN
	#define BN_LLONG
	#define DES_RISC1
  #elif defined( __EMU_SYMBIAN_OS__ )
	#define L_ENDIAN
	#define BN_LLONG
	#define BF_PTR
	#define DES_PTR
	#define DES_UNROLL
	#define RC4_INDEX
  #else
	#error Need to define architecture-specific values for crypto code
  #endif /* Symbian OS variants */
#endif /* Symbian OS */

/* Tandem NSK/OSS */
#ifdef __TANDEM
  #define B_ENDIAN
  #define BF_PTR
  #define DES_RISC2
  #define DES_PTR
  #define DES_UNROLL
  #define RC4_INDEX
  #define RC4_CHAR
  #define RC4_CHUNK
  #define MD2_CHAR
#endif /* Tandem */

/* Ultrix */
#ifdef __ultrix__
  #define L_ENDIAN
  #define DES_PTR
  #define DES_RISC2
  #define DES_UNROLL
#endif /* Ultrix */

/* VM/CMS */
#ifdef __VMCMS__
  #define B_ENDIAN
#endif /* VM/CMS */

/* Windows */
#if ( defined( _WINDOWS ) || defined( WIN32 ) || defined( _WIN32 ) )
  #define L_ENDIAN

  /* VC++ */
  #if defined( _MSC_VER )

	/* VS 64-bit */
	#if defined( _M_X64 )
	  /* Win64's ULONG_MAX (via limits.h) is 32 bits so the system isn't
	     detected as a 64-bit one, to fix this we manually override the
	     detected machine word size here */
	  #undef THIRTY_TWO_BIT
	  #define SIXTY_FOUR_BIT 
	  #define RC4_CHUNK_LL 
	  #define DES_INT 

	/* VC++ 32-bit */
	#elif ( _MSC_VER >= 1000 )
	  #define BN_LLONG
	  #define RC4_INDEX

	/* VC++ 16-bit */
	#else
	  #define BN_LLONG
	  #define MD2_CHAR
	  #define DES_UNROLL
	  #define DES_PTR
	  #define RC4_INDEX
	  #undef THIRTY_TWO_BIT
	  #define SIXTEEN_BIT
	#endif /* VC++ 32 vs 16-bit */

  /* BC++ */
  #elif defined( __BORLANDC__ )
	#define BN_LLONG
	#define DES_PTR
	#define RC4_INDEX

  /* gcc */
  #else
	#define BN_LLONG
	#define DES_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_INDEX
  #endif /* Assorted Windows compilers */
#endif /* Windows */
#ifdef __CYGWIN__
  #define L_ENDIAN
  #define BN_LLONG
  #define DES_PTR
  #define DES_RISC1
  #define DES_UNROLL
  #define RC4_INDEX
#endif /* gcc native under Cygwin (i.e. not a Cygwin-hosted
		  cross-development toolchain */

/* Embeded OSes get a bit complicated because they're usually cross-
   compiled, first we try for OS-specific options, then we try for the most 
   obvious generic options like the GNU toolchain, and finally if we can't 
   find anything we bail out with an error message */

#if defined( USE_EMBEDDED_OS )

  /* Xilinx XMK */
  #if defined ( _XMK ) || defined( __XMK__ )
	#if defined( __mb__ )
	  #define B_ENDIAN
	  /* Not sure what other options the MicroBlaze build should enable... */
	#elif defined( __ppc__ )
	  #ifdef DATA_LITTLEENDIAN
		#define L_ENDIAN
	  #else
		#define B_ENDIAN
	  #endif	/* Usually big-endian but may be little-endian */
	  #define BN_LLONG
	  #define BF_PTR
	  #define DES_RISC1
	  #define DES_UNROLL
	  #define RC4_CHAR
	  #define RC4_CHUNK
	#else
	  #error Need to define CPU type for non-MicroBlaze/non-PPC XMK.
	#endif /* XMK target variants */

  /* Generic gcc */
  #elif defined( __i386__ )
	#define L_ENDIAN
	#define BN_LLONG
	#define DES_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_INDEX
  #elif defined( __ppc__ ) || defined( __powerpc__ )
	#ifdef DATA_LITTLEENDIAN
	  #define L_ENDIAN
	#else
	  #define B_ENDIAN
	#endif	/* Usually big-endian but may be little-endian */
	#define BN_LLONG
	#define BF_PTR
	#define DES_RISC1
	#define DES_UNROLL
	#define RC4_CHAR
	#define RC4_CHUNK
  #elif defined( __arm ) || defined( __arm__ )
	#define L_ENDIAN
	#define BN_LLONG
	#define DES_RISC1

  /* Generic 68K */
  #elif defined( __m68k__  )
	/* This one is CISC-y enough that any of the (mostly) RISC-specific
	   optimisations won't have much effect, so the generic code is as good
	   as any */
	#define B_ENDIAN

  /* We need the developer's help to sort it out */
  #else
	#error Need to configure the crypto build options for your toolchain.
  #endif /* Embedded OS variants */
#endif /* Embedded OSes */

/* RC4_CHUNK is actually a data type rather than a straight define so we
   redefine it as a data type if it's been defined */

#ifdef RC4_CHUNK
  #undef RC4_CHUNK
  #define RC4_CHUNK	unsigned long
#endif /* RC4_CHUNK */

/* Make sure that we weren't missed out.  See the comment in the Cray 
   section for the exception for Crays */

#if !defined( _CRAY ) && !defined( L_ENDIAN ) && !defined( B_ENDIAN )
  #error You need to add system-specific configuration settings to osconfig.h.
#endif /* Endianness not defined */
#if defined( L_ENDIAN ) && defined( B_ENDIAN )
  #error Incorrect endianness detection in osconfig.h, both L_ENDIAN and B_ENDIAN are defined.
#endif /* Endianness defined erratically */
#if defined( CHECK_ENDIANNESS ) && !defined( OSX_UNIVERSAL_BINARY )
  /* One-off check in des_enc.c, however for OS X universal (fat) binaries
	 we're effectively cross-compiling for multiple targets so we don't
	 perform the check, which would yield false positives */
  #undef _CONFIG_DEFINED
	/* Including crypt.h at this point violates the normal include order 
	   because we've already included config.h which normally depends on 
	   settings in crypt.h, however for this one-off check it isn't a 
	   problem so we fake out the include-order check in config.h */
  #include "crypt.h"
  #if defined( DATA_LITTLEENDIAN ) && defined( DATA_BIGENDIAN )
	#error Incorrect endianness detection in crypt.h, 
	#error both DATA_LITTLEENDIAN and DATA_BIGENDIAN are defined.
  #endif /* Global endianness defined erratically */
  #if ( defined( L_ENDIAN ) && !defined( DATA_LITTLEENDIAN ) )
	#error You need to synchronise the endianness configuration settings 
	#error in osconfig.h and crypt.h.  The cryptlib config is set to 
	#error DATA_BIGENDIAN but osconfig.h has detected L_ENDIAN.
  #endif /* L_ENDIAN && !DATA_LITTLEENDIAN */
  #if ( defined( B_ENDIAN ) && !defined( DATA_BIGENDIAN ) )
	#error You need to synchronise the endianness configuration settings 
	#error in osconfig.h and crypt.h.  The cryptlib config is set to 
	#error DATA_LITTLEENDIAN but osconfig.h has detected B_ENDIAN.
  #endif /* B_ENDIAN && !DATA_BIGENDIAN */
#endif /* One-off check */

#endif /* _OSCONFIG_DEFINED */
