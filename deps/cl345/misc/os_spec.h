/****************************************************************************
*																			*
*					  cryptlib OS-Specific Interface Header File 			*
*						Copyright Peter Gutmann 1992-2017					*
*																			*
****************************************************************************/

#ifndef _OSSPEC_DEFINED

#define _OSSPEC_DEFINED

/* To build the static .LIB under Win32, uncomment the following define (this
   it not recommended since the init/shutdown is no longer completely thread-
   safe).  In theory it should be possible to detect the build of a DLL vs.a
   LIB with the _DLL define which is set when the /MD (multithreaded DLL)
   option is used, however VC++ only defines _DLL when /MD is used *and*
   it's linked with the MT DLL runtime.  If it's linked with the statically
   linked runtime, _DLL isn't defined, which would result in the unsafe LIB
   version being built as a DLL */

/* #define STATIC_LIB */

/* os_spec.h uses settings from config.h, so this file must be applied after 
   config.h */

#ifndef _CONFIG_DEFINED
  #error "os_spec.h must be included after config.h"
#endif /* _CONFIG_DEFINED */

/****************************************************************************
*																			*
*						OS-Specific Compiler Configuration					*
*																			*
****************************************************************************/

/* Include stdint.h if it's available, since this greatly simplifies the
   handling of data types in a portable manner */

#if defined( __STDC_VERSION__ ) && ( __STDC_VERSION__ >= 199901L ) 
  /* stdint.h via C99 compatibility */
  #include <stdint.h>
#elif defined( __GNUC__ ) && ( __GNUC__ > 3 ) 
  #if defined( __VxWorks__ ) && \
	  ( ( _WRS_VXWORKS_MAJOR < 6 ) || \
		( _WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 6 ) )
	/* Older versions of Wind River's toolchain don't include stdint.h, it 
	   was added somewhere around VxWorks 6.6 */
	typedef unsigned int		uintptr_t;
	typedef int					intptr_t;
  #else
	/* stdint.h via GNU headers */
	#include <stdint.h>
  #endif /* Nonstandard gcc environments */
#elif defined( __SUNPRO_C )
  /* Older versions of Slowaris, which won't be caught by the STDC check 
     above, use inttypes.h */
  #include <inttypes.h>
#elif defined( __HAIKU__ )
  /* Ancient gcc but with stdint.h */
  #include <stdint.h>
#elif defined( _MSC_VER ) && VC_GE_2015( _MSC_VER )
  /* stdint.h via Visual Studio */
  #include <stdint.h>
#elif defined( _MSC_VER )
  typedef INT_PTR				intptr_t;
  typedef UINT_PTR				uintptr_t;
#else
  #if ULONG_MAX > 0xFFFFFFFFUL
	typedef long long			intptr_t;
	typedef unsigned long long	uintptr_t;
  #else
	typedef long				intptr_t;
	typedef unsigned long		uintptr_t;
  #endif /* 64- vs 32-bit systems */
#endif /* Various stdint.h options */

/* Try and figure out how to get the current function name.  Pre-C99 used
   __FUNCTION__ instead of __func__ and treated it like the other 
   preprocessor macros __FILE__, __TIME__, etc, so if we find that we map
   it to the C99 __func__.  In addition because __func__ is a predefined 
   string literal rather than a predefined macro/preprocessor symbol, we
   can't check for #ifdef __func__ because it's usually not visible to
   the preprocessor */

#if defined( __FUNCTION__ ) && \
	!( defined( __STDC_VERSION__ ) && ( __STDC_VERSION__ >= 199901L ) )
  #define __func__		__FUNCTION__
#endif /* Map old-style __FUNCTION__ to C99 equivalent */
#if defined( __arm__ ) || defined( __ghs__ )
  #define __func__		__FUNCTION__
#endif /* Compilers with __FUNCTION__ but not __func__, and as a predefined 
		  string literal not a predefined macro, Arm cc and Green Hills cc */

#if ( defined( __STDC_VERSION__ ) && ( __STDC_VERSION__ >= 199901L ) ) || \
	defined( __func__ ) || \
	defined( __GNUC__ ) || defined( __clang__ ) || defined( __arm__ ) || \
	defined( __IAR_SYSTEMS_ICC__ ) || defined( __INTEL_COMPILER ) || \
	defined( __SUNPRO_C ) 
  /* The compilers above have __func__, or __FUNCTION__ mapped to __func__.  
     For all others we generate a compile error to allow explicit fixup of 
	 the appropriate macro or identifier, unless the compilers are excluded
	 below */
  #define HAS_FUNC
#endif /* Compilers with __func__ support */
#if !defined( HAS_FUNC ) && \
	( defined( _MSC_VER ) && VC_LT_2005( _MSC_VER ) )
  #define __func__				"(unknown)"
#endif /* HAS_FUNC */

/* If we're compiling on the AS/400, make enums a fixed size rather than
   using the variable-length values that IBM compilers default to, and force
   strings into a read-only segment (by default they're writeable) */

#ifdef __AS400__
  #pragma enumsize( 4 )
  #pragma strings( readonly )
  #define EBCDIC_CHARS
#endif /* AS/400 */

/* If we're compiling under MVS or VM/CMS, make enums a fixed size rather
   than using the variable-length values that IBM compilers default to */

#if defined( __MVS__ ) || defined( __VMCMS__ )
  #pragma enum( 4 )
  #define USE_ETOA		/* Use built-in ASCII <-> EBCDIC conversion */
  #define EBCDIC_CHARS
#endif /* __MVS__ */

/* If we're compiling under QNX, make enums a fixed size rather than using
   the variable-length values that the Watcom compiler defaults to */

#if defined( __QNX__ ) && defined( __WATCOMC__ )
  #pragma enum int
#endif /* QNX and Watcom C */

/* A few rare operations are word-size-dependant.  This isn't as simple as 
   checking LONG_MAX because it won't work correctly with LLP64 systems, 
   luckily this is only Win64 which we can check for explicitly */

#if INT_MAX <= 32768L
  #define SYSTEM_16BIT
#elif ( ULONG_MAX > 0xFFFFFFFFUL ) || defined( __WIN64__ )
  #define SYSTEM_64BIT
#else
  #define SYSTEM_32BIT
#endif /* 16- vs.32- vs.64-bit system */

/* Useful data types.  Newer compilers provide a 'bool' datatype via 
   stdbool.h, but in a fit of braindamage generally make this a char instead 
   of an int.  While Microsoft's use of char for BOOLEAN in the early 1980s 
   with 8/16-bit 8086s and 128K of RAM makes sense, it's a pretty stupid 
   choice for 32- or 64-bit CPUs because alignment issues mean that it'll 
   generally still require 32 or 64 bits of storage (except for special 
   cases like an array of bool), but then the difficulty or even inability 
   of many CPUs and/or architectures to perform byte-level accesses means 
   that in order to update a boolean the system has to fetch a full machine 
   word, mask out the byte data, or/and in the value, and write the word 
   back out.  So 'bool' = 'char' combines most of the worst features of both 
   char and int.
   
   It also leads to really hard-to-find implementation bugs due to the fact 
   that '(bool) int = true' produces different results to 
   '*(bool *) intptr = true', something that was resolved years ago in enums 
   without causing such breakage.  
   
   Quite apart from those issues, the fact that we use fault-immune values
   for booleans (see misc/safety.h) means that we can't use a bool because 
   it's only defined for the values 0 and 1, a single fault away from each 
   other.

   Because of this we avoid the use of bool and just define it to int */

typedef unsigned char		BYTE;
#if defined( __STDC_VERSION__ ) && ( __STDC_VERSION__ >= 199901L ) && 0
  #include <stdbool.h>
  typedef bool              BOOLEAN;
#elif defined( __WIN32__ ) || defined( __WINCE__ )
  /* VC++ typedefs BOOLEAN so we need to use the preprocessor to override it */
  #define BOOLEAN			int
#elif defined( __UCOS__ ) || defined( __SMX__ )
  /* Some OSes typedef BOOLEAN themselves so we set it as a #define, which 
	 means that we can then work around the typedef by undefining and
	 redefining it around the include of the OS-specific headers:

		#undef BOOLEAN
		#include <smx.h>
		#define BOOLEAN int */
  #if defined( __SMX__ ) && !defined( _MSC_VER )
	typedef int				BOOLEAN;
  #else
	#define BOOLEAN			int
  #endif /* OS-specific BOOLEAN juggling */
#elif defined( __Nucleus__ )
  /* Nucleus defines BOOLEAN as 'unsigned char' so we override it to be an
     int */
  #undef  BOOLEAN
  #define BOOLEAN			int
#else
  typedef int				BOOLEAN;
#endif /* Boolean data type on different platforms */

/* Sometimes we need to pass a BOOLEAN by reference to a function that 
   treats its parameter as a generic integer value.  In order to make 
   explicit that this is still a BOOLEAN even though it's of type int,
   we define a symbolic BOOLEAN_INT */

typedef int					BOOLEAN_INT;

/* If we're building the Windows kernel driver version, include the DDK
   headers */

#if defined( __WIN32__ ) && defined( NT_DRIVER )
  #include <ntddk.h>
#endif /* NT kernel driver */

/* If we're using eCOS, include the system config file that tells us which 
   parts of the eCOS kernel are available */

#ifdef __ECOS__
  #include <pkgconf/system.h>
#endif /* __ECOS__ */

/* The VxWorks SDK defines the value 'SH' (to indicate the use of the SuperH 
   CPU family) in vxCpu.h which conflicts with the 'struct SH' in ssh.h. 
   To fix this we undefine it, this shouldn't be a problem since the define 
   SH32 is set to the same value as SH and presumably no-one will be using 
   the basic 20-year-old SuperH any more.

   VxWorks also uses some global symbols that clash with cryptlib's ones, 
   to resolve this we redefine the cryptlib ones to have a 'cl_' prefix */

#ifdef __VxWorks__
  /* Correct the use of the VxWorks preprocessor define 'SH' overriding 
     'struct SH' in ssh.h */
  #if defined( SH )
	#undef SH
  #endif /* SH */

  /* Correct clashing global symbols in VxWorks */
  #define setSerialNumber		cl_setSerialNumber
  #define inflate				cl_inflate
  #define addAction				cl_addAction
  #define inflate_copyright		cl_inflate_copyright
  #define zlibVersion			cl_zlibVersion
#endif /* __VxWorks__ */

/* Some versions of the WinCE SDK define 'interface' as part of a complex 
   series of kludges for OLE support, made even more amusing by the fact 
   that 'interface' is an optional keyword in eVC++ which may or may not 
   be recognised as such by the compiler.  To avoid conflicts we undefine 
   it if it's defined since we're not using any OLE functionality */

#if defined( __WINCE__ ) && defined( interface )
  #undef interface
#endif /* WinCE SDK */

/* cryptlib contains a few locations that require forward declarations for
   static data:

	extern const int foo[];

	foo[ i ] = bar;

	static const int foo[] = { ... };

   Compiler opinions on how to handle this vary.  Some compile it as is
   (i.e. 'static const ...'), some don't allow the 'static', some allow both
   variants, and some produce warnings with both but allow them anyway
   (there are probably more variants with further compilers).  To get around
   this, we use the following define and then vary it for broken compilers
   (the following is the minimum required to get it to compile, other broken
   compilers will still produce warnings) */

#if ( defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x550 ) ) || \
	defined( __VMCMS__ ) || defined( __MVS__ ) || defined( __MRC__ ) || \
	defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ ) || \
	( defined( __UNIX__ ) && ( defined( __HP_cc ) || defined( __HP_aCC ) ) ) || \
	( defined( __UNIX__ ) && defined( _MPRAS ) )
  #define STATIC_DATA
#else
  #define STATIC_DATA	static
#endif /* Fn.prototyping workarounds for borken compilers */

/* When implementing safe pointers, cryptlib returns structs that contain 
   fat pointers, some of which are const.  A few compilers don't like a
   const struct as a returned value, so we no-op it out */

#if defined( __xlc__ ) || defined( __IBMC__ )
  #define CONST_RETURN 
#else
  #define CONST_RETURN	const
#endif /* IBM mainframe compilers */

/* A few compilers won't allow initialisation of a struct at runtime, so
   we have to kludge the init with macros.  This is rather ugly since
   instead of saying "struct = { a, b, c }" we have to set each field
   individually by name.  The real reason for doing this though is that
   if the compiler can initialise the struct directly, we can make the
   fields const for better usage checking by the compiler.

   There are two forms of this, one for simple structs and one for arrays
   of structs.  At the moment the only use for the array-init is for the
   situation where the array represents a sequence of search options with
   the last one being a terminator entry, so we provide a simplified form
   that only sets the required fields.
   
   The value of __SUNPRO_C bears no relation whatsoever to the actual 
   version number of the compiler and even Sun's docs give different values 
   in different places for the same compiler version, but 0x570 seems to 
   work */

#if ( defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x550 ) ) || \
	defined( _CRAY ) || \
	( defined( __hpux ) && !defined( __GNUC__ ) ) || \
	( defined( __QNX__ ) && ( OSVERSION <= 4 ) ) || \
	defined( __RVCT2_1__ ) || \
	( defined( __SUNPRO_C ) && ( __SUNPRO_C <= 0x570 ) ) || \
	defined( __SCO_VERSION__ ) || defined(  __TANDEM )
  #define CONST_INIT
  #define CONST_INIT_STRUCT_3( decl, init1, init2, init3 ) \
		  decl
  #define CONST_INIT_STRUCT_4( decl, init1, init2, init3, init4 ) \
		  decl
  #define CONST_INIT_STRUCT_5( decl, init1, init2, init3, init4, init5 ) \
		  decl
  #define CONST_SET_STRUCT( init ) \
		  init

  #define CONST_INIT_STRUCT_A2( decl, init1, init2 ) \
		  decl
  #define CONST_SET_STRUCT_A( init ) \
		  init
#else
  #define CONST_INIT	const
  #define CONST_INIT_STRUCT_3( decl, init1, init2, init3 ) \
		  decl = { init1, init2, init3 }
  #define CONST_INIT_STRUCT_4( decl, init1, init2, init3, init4 ) \
		  decl = { init1, init2, init3, init4 }
  #define CONST_INIT_STRUCT_5( decl, init1, init2, init3, init4, init5 ) \
		  decl = { init1, init2, init3, init4, init5 }
  #define CONST_SET_STRUCT( init )

  #define CONST_INIT_STRUCT_A2( decl, init1, init2 ) \
		  const decl = { { init1, 0 }, { init2, 0 } }
  #define CONST_SET_STRUCT_A( init )
#endif /* Watcom C || SunPro C || SCO C */

/* The Tandem mktime() is broken and can't convert dates beyond 2023, so we
   replace it with our own version which can */

#if defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ )
  #define mktime( timeptr )		my_mktime( timeptr )
#endif /* __TANDEM_NSK__ || __TANDEM_OSS__ */

/* MQX is even worse, its struct tm is broken so we need to override both
   mktime() and gmtime() to allow fixups for the struct tm fields */

#if defined( __MQXRTOS__ )
  #define mktime( timeptr )				my_mktime( timeptr )
  #define gmtime_r( time, result )		my_gmtime_r( time, result )
#endif /* __MQXRTOS__ */

/* In some environments va_list is a scalar, so it can't be compared with 
   NULL in order to verify its validity.  This was particularly problematic 
   with the ARM ABI, which changed the type in late 2009 to 
   'struct __va_list { void *__ap; }', breaking compatibility with all 
   existing code.  We can detect this by taking advantage of the fact that 
   support for the change was added in gcc 4.4, so any newer version with 
   ARM_EABI defined will have a scalar va_list.

   The Arm64 ABI gets even more complicated, with a va_list being 
   struct __va_list { void *__stack; void *__gr_top; void *__vr_top; 
   int __gr_offs; int __vr_offs; } so we no-op it out there as well.
   
   Super-H variants also have scalar va_lists, the Super-H varargs header 
   va-sh.h uses a complex structure to handle va_lists for all SH3 and SH4 
   variants, this presumably extends to SH5 as well so we treat va_lists on 
   Super-H as scalars */

#if defined( __GNUC__ )
  #if( defined( __ARM_EABI__ ) && \
	   ( __GNUC__ == 4 && __GNUC_MINOR__ >= 4 ) || ( __GNUC__ > 4 ) )
	/* In theory we could check __ap but in practice it's too risky to rely 
	   on the type and state of hidden internal fields, and in any case it's 
	   only a sanity check, not a hard requirement, so we just no-op the 
	   check out */
	#define verifyVAList( x ) TRUE
  #elif defined( __aarch64__ ) || defined( __arm64 )
	#define verifyVAList( x ) TRUE
  #elif defined( __sh__ )
	#define verifyVAList( x ) TRUE
  #endif /* Architecture-specific scalar va_lists */
#elif defined( __RVCT2_1__ ) || defined( __IAR_SYSTEMS_ICC__ )
  /* The RealView and IAR compilers have the same issue */
  #define verifyVAList( x ) TRUE
#endif /* Nonstandard va_list types */
#ifndef verifyVAList
  #define verifyVAList( x ) ( ( x ) != NULL )
#endif /* Verify function for vector arg lists */

/* cryptlib has many code sequences of the form:

	status = foo();
	if( cryptStatusOK( status ) )
		status = bar();
	if( cryptStatusOK( status ) )
		status = baz();
	if( cryptStatusOK( status ) )
		...

   These can be made more efficient when the compiler can assume that the
   majority case has 'status == CRYPT_OK'.  gcc provides a means of doing 
   this via __builtin_expect().  As usual for gcc the documentation for this 
   is quite confusing:

     "if( __builtin_expect( x, 0 ) ) foo (); would indicate that we do not 
	 expect to call foo, since we expect x to be zero"

   In this case the test is actually the expression 'x', which is evaluated
   as 'x != 0', with the second parameter only taking values 0 (to mean 'not
   likely') or 1 (to mean 'likely').  So the appropriate usage is 
   "__builtin_expect( expr, 0 )" to mean that we don't expect something and 
   "__builtin_expect( expr, 1 )" to mean that we do expect it.  The 
   following forms of cryptStatusError() and cryptStatusOK() assume that in 
   the majority of situations we won't encounter the error case */

#if defined( __GNUC__ ) && ( __GNUC__ >= 3 )
  #undef cryptStatusError
  #undef cryptStatusOK
  #define cryptStatusError( status ) \
		  __builtin_expect( ( status ) < CRYPT_OK, 0 )
  #define cryptStatusOK( status ) \
		  __builtin_expect( ( status ) == CRYPT_OK, 1 )
#endif /* gcc 3.x and newer */

/* Nucleus has it's own functions for allocating and freeing memory, so
   we provide wrappers for them that override the default clAlloc()/clFree()
   mappings */

#ifdef __Nucleus__ 
  #define clAlloc( string, size )		clAllocFn( size )
  #define clFree( string, memblock )	clFreeFn( memblock )
  void *clAllocFn( size_t size );
  void clFreeFn( void *memblock );
#endif /* __Nucleus__ */

/****************************************************************************
*																			*
*								Dynamic Loading Support						*
*																			*
****************************************************************************/

/* On systems that support dynamic loading, we bind various drivers and
   libraries at runtime rather than at compile time.  Under Windows this is
   fairly easy but under Unix it's supported somewhat selectively and may be
   buggy or platform-specific */

#ifdef DYNAMIC_LOAD
  /* Macros to map OS-specific dynamic-load values to generic ones */
  #if defined( __WINDOWS__ )
	#define INSTANCE_HANDLE		HINSTANCE
	#define NULL_INSTANCE		( HINSTANCE ) NULL
	#ifdef __WINCE__
	  #define DynamicLoad( name ) LoadLibrary( name )
	#else
	  #define DynamicLoad( name ) SafeLoadLibrary( name )
	#endif /* Win32 vs. WinCE */
	#define DynamicUnload		FreeLibrary
	#define DynamicBind			GetProcAddress
  #elif defined( __UNIX__ ) || defined( __ANDROID__ )
    /* Older versions of OS X didn't have dlopen() support but required
	   the use of the rather painful low-level dyld() interface.  If you're
	   running an older version of OS X and don't have the dlcompat wrapper
	   installed, get Peter O'Gorman's dlopen() implementation, which wraps
	   the dyld() interface */
	#include <dlfcn.h>
	#define INSTANCE_HANDLE		void *
	#define NULL_INSTANCE		NULL
	#define DynamicLoad( name )	dlopen( name, RTLD_LAZY )
	#define DynamicUnload		dlclose
	#define DynamicBind			dlsym
  #elif defined __VMCMS__
	#include <dll.h>

	#define INSTANCE_HANDLE		dllhandle *
	#define NULL_INSTANCE		NULL
	#define DynamicLoad( name )	dllload( name, RTLD_LAZY )
	#define DynamicUnload		dllfree
	#define DynamicBind			dlqueryfn
  #endif /* OS-specific instance handles */
#endif /* DYNAMIC_LOAD */

/****************************************************************************
*																			*
*									Charset Support							*
*																			*
****************************************************************************/

/* Widechar handling.  Most systems now support this, the only support that
   we require is the wchar_t type define */

#ifdef USE_WIDECHARS
  #if !( defined( __ECOS__ ) || \
		 ( defined( __QNX__ ) && ( OSVERSION <= 4 ) ) || \
		 ( defined( __WIN32__ ) && defined( __BORLANDC__ ) ) || \
		 ( defined( __WINCE__ ) && _WIN32_WCE < 400 ) || \
		 defined( __XMK__ ) )
	#include <wchar.h>
  #endif /* Systems with widechar support in stdlib.h */
#else
  /* No native widechar support, define the necesary types ourselves unless
	 we're running under older OS X (Darwin 6.x), which defines wchar_t in
	 stdlib.h even though there's no wchar support present, or PalmOS, which
	 defines it in wchar.h but then defines it differently in stddef.h, and
	 in any case has no wchar support present */
  #if !( defined( __APPLE__ ) || defined( __MVS__ ) || \
		 defined( __OpenBSD__ ) || defined( __PALMOS__ ) || \
		 defined( __SMX__ ) )
	typedef unsigned short int wchar_t;
  #endif /* __APPLE__ */
#endif /* USE_WIDECHARS */
#define WCSIZE					( sizeof( wchar_t ) )
#ifndef WCHAR_MAX
  #define WCHAR_MAX				( ( wchar_t ) -1 )
#endif /* !WCHAR_MAX */

/* The EOL convention used when outputting text.  Technically speaking 
   Nucleus, SMX, and XMK don't use any particular EOL convention, but since 
   the typical development environment is debug output sent to a Windows 
   terminal emulator, we use CRLF */

#if defined( __MSDOS16__ ) || defined( __MSDOS32__ ) || \
	defined( __Nucleus__ ) || defined( __OS2__ ) || \
	defined( __SMX__ ) || defined( __SYMBIAN32__ ) || \
	defined( __WINDOWS__ ) || defined( __XMK__ )
  #define EOL					"\r\n"
  #define EOL_LEN				2
#elif ( defined( __APPLE__ ) && !defined( __MAC__ ) ) || \
	  defined( __BEOS__ ) || defined( __IBM4758__ ) || \
	  defined( __MVS__ ) || defined( __PALMOS__ ) || \
	  defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ ) || \
	  defined( __UNIX__ ) || defined( __VMCMS__ )
  #define EOL					"\n"
  #define EOL_LEN				1
#elif defined( __MAC__ )
  #define EOL					"\r"
  #define EOL_LEN				1
#elif defined( USE_EMBEDDED_OS )
  /* For embedded OSes we assume a generic Unix-like text environment, these 
	 aren't exactly used for interactive operations like text editing so 
	 there's usually no fixed text format, and many will handle both CRLF 
	 and LF-only text, with the lowest common denominator being the Unix-
	 style LF-only */
  #define EOL					"\n"
  #define EOL_LEN				1
#else
  #error "You need to add the OS-specific define to enable end-of-line handling"
#endif /* OS-specific EOL markers */

/* If we're compiling on IBM mainframes, enable EBCDIC <-> ASCII string
   conversion.  Since cryptlib uses ASCII internally for all strings, we
   need to check to make sure it's been built with ASCII strings enabled
   before we go any further */

#ifdef EBCDIC_CHARS
  #if 'A' != 0x41
	#error cryptlib must be compiled with ASCII literals
  #endif /* Check for use of ASCII */

  int asciiToEbcdic( char *dest, const char *src, const int length );
  int ebcdicToAscii( char *dest, const char *src, const int length );
  char *bufferToEbcdic( char *buffer, const char *string );
  char *bufferToAscii( char *buffer, const char *string );
#endif /* IBM mainframes */

/* If we're compiling on Windows CE, enable Unicode <-> ASCII string
   conversion */

#ifdef UNICODE_CHARS
  int asciiToUnicode( wchar_t *dest, const int destMaxLen, 
					  const char *src, const int length );
  int unicodeToAscii( char *dest, const int destMaxLen, 
					  const wchar_t *src, const int length );
#endif /* Windows CE */

/* Since cryptlib uses ASCII internally, we have to force the use of
   ASCII-compatible versions of system library functions if the system
   uses EBCDIC */

#ifdef EBCDIC_CHARS
  #include <stdarg.h>

  #define ASCII_ALPHA			0x01
  #define ASCII_LOWER			0x02
  #define ASCII_NUMERIC			0x04
  #define ASCII_SPACE			0x08
  #define ASCII_UPPER			0x10
  #define ASCII_HEX				0x20
  extern const BYTE asciiCtypeTbl[];

  #define isAlnum( ch ) \
		  ( asciiCtypeTbl[ byteToInt( ch ) ] & ( ASCII_ALPHA | ASCII_NUMERIC ) )
  #define isAlpha( ch ) \
		  ( asciiCtypeTbl[ byteToInt( ch ) ] & ASCII_ALPHA )
  #define isDigit( ch ) \
		  ( asciiCtypeTbl[ byteToInt( ch ) ] & ASCII_NUMERIC )
  #define isPrint( ch ) \
		  ( ( byteToInt( ch ) ) >= 0x20 && ( byteToInt( ch ) ) <= 0x7E )
  #define isXDigit( ch ) \
		  ( asciiCtypeTbl[ byteToInt( ch ) ] & ASCII_HEX )
  #define toLower( ch ) \
		  ( ( asciiCtypeTbl[ byteToInt( ch ) ] & ASCII_UPPER ) ? \
			( byteToInt( ch ) ) + 32 : ( byteToInt( ch ) ) )
  #define toUpper( ch ) \
		  ( ( asciiCtypeTbl[ byteToInt( ch ) ] & ASCII_LOWER ) ? \
			( byteToInt( ch ) ) - 32 : ( byteToInt( ch ) ) )

  /* We can't annotate these functions because os_spec.h is pulled in before
     analyse.h, so the required annotations don't exist yet */
  int strCompare( const char *src, const char *dest, const int length );
  int strCompareZ( const char *src, const char *dest );
  int sPrintf_s( char *buffer, const int bufSize, const char *format, ... );
  int vsPrintf_s( char *buffer, const int bufSize, const char *format, 
				  va_list argPtr );
  #define sprintf_s				sPrintf_s
  #define vsprintf_s			vsPrintf_s
#else
  #if defined( __Nucleus__ )
	#include <nu_ctype.h>
	#include <nu_string.h>
  #else
	#include <ctype.h>
  #endif /* OS-specific includes */

  #define isAlnum( ch )			isalnum( byteToInt( ch ) )
  #define isAlpha( ch )			isalpha( byteToInt( ch ) )
  #define isDigit( ch )			isdigit( byteToInt( ch ) )
  #define isPrint( ch )			isprint( byteToInt( ch ) )
  #define isXDigit( ch )		isxdigit( byteToInt( ch ) )
  #define toLower( ch )			tolower( byteToInt( ch ) )
  #define toUpper( ch )			toupper( byteToInt( ch ) )
  #define strCompareZ( str1, str2 )	\
								stricmp( str1, str2 )
  #define strCompare( str1, str2, len )	\
								strnicmp( str1, str2, len )
#endif /* EBCDIC_CHARS */

/* SunOS and older Slowaris have broken sprintf() handling.  In SunOS 4.x
   this was documented as returning a pointer to the output data as per the
   Berkeley original.  Under Slowaris the manpage was changed so that it
   looks like any other sprintf(), but it still returns the pointer to the
   output buffer in some versions so we use a wrapper that checks at
   runtime to see what we've got and adjusts its behaviour accordingly */

#if defined( sun ) && ( OSVERSION <= 5 )
  int fixedSprintf( char *buffer, const int bufSize,
					const char *format, ... );

  #undef sPrintf_s
  #define sPrintf_s				fixedSprintf
#endif /* Old SunOS */

/* Borland C++ before 5.50 doesn't have snprintf() or vsnprintf() */

#if defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x550 )
  #include <stdarg.h>

  int bcSnprintf( char *buffer, const int bufSize,
				  const char *format, ... );
  int bcVsnprintf( char *buffer, const int bufSize,
				   const char *format, va_list argPtr );
#endif /* BC++ before 5.50 */

/****************************************************************************
*																			*
*						TR 24731 Safe stdlib Extensions						*
*																			*
****************************************************************************/

/* ISO/IEC TR 24731 defines alternative stdlib functions designed to perform
   additional parameter checking and avoid some types of common buffer
   overflows.  We use these if possible, if they're not available we map
   them down to the traditional stdlib equivalents, via the preprocessor if
   possible or using wrapper functions if not.  In addition we use the 
   OpenBSD et al strlcpy()/strlcat() functions, whose truncation semantics 
   make them more useful than the TR 24731 equivalents (for example 
   strcpy_s() does nothing on overflow while the equivalent strlcpy() copies
   with truncation).  Microsoft recognise this as well, implementing them in
   TR 24731 by allowing the caller to specify _TRUNCATE semantics */

#ifdef __STDC_LIB_EXT1__
  #if defined( _MSC_VER ) && VC_GE_2005( _MSC_VER )
	/* The VC++ implementation of TR 24731 is based on preliminary versions 
	   of the design for the spec, and in some cases needs re-mapping onto 
	   the final versions.  Instances of this are:
   
		TR 24731: struct tm *gmtime_s( const time_t *timer, struct tm *result );
		VC++: errno_t gmtime_s( struct tm *result, const time_t timer );

	   Because this could potentially result in a circular definition, we 
	   have to kludge in an intermediate layer by renaming the call to 
	   gmTime_s(), which we then remap to the VC++ gmtime_s() */
	#define gmTime_s( timer, result )	\
			( ( gmtime_s( result, timer ) == 0 ) ? result : NULL )

	/* Complicating things further, the Windows DDK doesn't have gmtime_s(),
	   although it does have all of the other TR 24731 functions.  To handle
	   this, we use the same workaround as for the non-TR 24731 libcs */
	#ifdef WIN_DDK
	  #undef gmTime_s
	  #define gmTime_s( timer, result )	gmtime( timer )
	#endif /* WIN_DDK */

	/* MS implements strlcpy/strlcat-equivalents via the TR 24731 
	   functions */
	#define strlcpy_s( s1, s1max, s2 )	strncpy_s( s1, s1max, s2, _TRUNCATE )
	#define strlcat_s( s1, s1max, s2 )	strncat_s( s1, s1max, s2, _TRUNCATE )
  #else
	#define gmTime_s					gmtime_s
  #endif /* VC++ >= 2005 */
#else
  /* String functions.  The OpenBSD strlcpy()/strlcat() functions with their
     truncation semantics are quite useful so we use these as well, 
	 overlaying them with a macro that make them match the TR 24731 look 
	 and feel */
  #define strcpy_s( s1, s1max, s2 )		strcpy( s1, s2 )
  #if defined( __UNIX__ ) && \
	  ( defined( __APPLE__ ) || defined( __FreeBSD__ ) || \
		defined( __NetBSD__ ) || defined( __OpenBSD__ ) || \
		( defined( sun ) && OSVERSION >= 7 ) )
	/* Despite the glibc maintainer's pigheaded opposition to these 
	   functions, some Unix OSes support them via custom libc patches */
	#define strlcpy_s( s1, s1max, s2 )	strlcpy( s1, s2, s1max )	
	#define strlcat_s( s1, s1max, s2 )	strlcat( s1, s2, s1max )
  #else
	int strlcpy_s( char *dest, const int destLen, const char *src );
	int strlcat_s( char *dest, const int destLen, const char *src );
	#define NO_NATIVE_STRLCPY
  #endif /* OpenBSD safe string functions */

  /* Widechar functions */
  int mbstowcs_s( size_t *retval, wchar_t *dst, size_t dstmax, 
				  const char *src, size_t len );
  int wcstombs_s( size_t *retval, char *dst, size_t dstmax, 
				  const wchar_t *src, size_t len );
  #if defined( USE_EMBEDDED_OS )
    /* Support for the thread-safe mbtowc() is practially nonexistent in
	   embedded OSes, but in any case is unlikely to be necessary since
	   there'll likely only be a single task dealing with crypto */
	#define mbstate_t		int
	#define mbrtowc( wideChar, char, size, state ) \
			mbtowc( wideChar, char, size )
  #elif defined( _MSC_VER ) && VC_LE_VC6( _MSC_VER ) 
	/* VC++ 6.0 has mbrtowc() in wchar.h but not in libc, it's present in
	   the deprecated libcp/msvcp libraries but it's better to just map
	   it to mbtowc() since this is thread-safe under Windows anyway */
	#define mbrtowc( wideChar, char, size, state ) \
			mbtowc( wideChar, char, size )
  #endif /* Compiler-specific thread-safe mbtowc() support */

  /* printf() */
  #if defined( _MSC_VER ) && VC_LT_2005( _MSC_VER )
    #include <stdio.h>

	#define sprintf_s					_snprintf
	#define vsprintf_s					_vsnprintf
  #elif defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x550 )
	#define sprintf_s					bcSnprintf
	#define vsprintf_s					bcVsnprintf
  #elif defined( __QNX__ ) && ( OSVERSION <= 4 )
	/* snprintf() exists under QNX 4.x but causes a SIGSEGV when called */
	#define sprintf_s					_bprintf
	#define vsnprintf					_vbprintf
  #elif defined( EBCDIC_CHARS )
	/* We provide our own replacements for these functions which handle 
	   output in ASCII (rather than EBCDIC) form */
  #else
    #include <stdio.h>

	#define sprintf_s					snprintf
	#define vsprintf_s					vsnprintf
  #endif /* Compiler-specific safe printf() support */

  /* Misc.functions.  gmtime() is an ugly non-thread-safe function that runs 
     into the same problems as gethostbyname() (see the long comment in 
	 io/tcp.h), to deal with this as best we can we map it to the reentrant 
	 gmtime_r() if it's available.  In addition some OSes use TLS for the
	 result value so it's handled automatically, see again the comments in 
	 io/tcp.h for more on this */
  #if defined( USE_THREADS ) && defined( __GLIBC__ ) && ( __GLIBC__ >= 2 ) 
	#define gmTime_s					gmtime_r
  #elif defined( __MQXRTOS__ )
	#define gmTime_s					gmtime_r
  #else
	#define gmTime_s( timer, result )	gmtime( timer )
  #endif /* USE_THREADS and libraries that provide gmtime_r() */
#endif /* TR 24731 safe stdlib extensions */

/****************************************************************************
*																			*
*				Miscellaneous System-specific Support Functions				*
*																			*
****************************************************************************/

/* Perform various operations on pointers */

void *ptr_align( const void *ptr, const int units );
int ptr_diff( const void *ptr1, const void *ptr2 );

/* Check whether a pointer is aligned to a particular value, used by some
   low-level functions that check for potentially unaligned accesses and 
   clean them up if possible.  This is mostly a hygiene check in that if it
   can't be easily implemented we continue anyway but with the possible 
   overhead of an unaligned-access fixup, thus the _OPT qualifier.
   
   The apparently redundant cast to void * before the uintptr_t is necessary 
   because the conversion is only guaranteed for a void *, so if it's some 
   other type of pointer then we have to cast it to a void * first */

#if ( defined( __STDC_VERSION__ ) && ( __STDC_VERSION__ >= 199901L ) ) || \
	( defined( _MSC_VER ) && VC_GE_2005( _MSC_VER ) )
  #define IS_ALIGNED_OPT( pointer, value ) \
		  ( ( ( uintptr_t )( const void * )( pointer ) ) % ( value ) == 0 )
#else
  #define IS_ALIGNED_OPT( pointer, value )	TRUE
#endif /* C99 check */

/* Align a piece of data on a given boundary.  This is needed on some 
   architectures that fault on unaligned accesses if the fault handler 
   doesn't fix up the access.  This is only used in a handful of locations
   where we're adding cookies to buffers, which in most cases are aligned
   anyway due to the additional data surrounding them, the code doesn't 
   depend on it */

#if defined( __GNUC__ ) || defined( __clang__ )
  #ifdef SYSTEM_64BIT
	#define STACK_ALIGN_DATA	__attribute__(( aligned( 8 ) ))
  #else
	#define STACK_ALIGN_DATA	__attribute__(( aligned( 4 ) ))
  #endif /* 32- vs. 64-bit alignment */
#else
  #define STACK_ALIGN_DATA
#endif /* Data-alignment defines */

#endif /* _OSSPEC_DEFINED */
