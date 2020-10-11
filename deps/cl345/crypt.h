/****************************************************************************
*																			*
*					  cryptlib Internal General Header File 				*
*						Copyright Peter Gutmann 1992-2017					*
*																			*
****************************************************************************/

#ifndef _CRYPT_DEFINED

#define _CRYPT_DEFINED

/* Various compilers handle includes in subdirectories differently.  Most
   will work with paths from a root directory.  Non-OS X Macintoshes don't
   recognise '/'s as path delimiters, but work around it by scanning all
   subdirectories and treating the files as if they were in the same
   directory (INC_ALL).  Microsoft C, in a braindamaged exception to all
   other compilers, treats the subdirectory as the root, unless explicitly
   told to use the project-file directory by setting Project | Settings |
   C/C++ | Preprocessor | Additional include directories to '.\'.  The
   Tandem NSK (Guardian) filesystem doesn't have subdirectories, and the C
   compiler zaps '.'s, truncates filenames to 7 characters, and appends a
   'h' to the name (so that asn1misc.h becomes asn1mish).  This
   unfortunately requires a bit of renaming for header files.  Tandem OSS
   (Unix services) on the other hand is just like Unix, so we explicitly
   distinguish between the two */

#if defined( SYMANTEC_C ) && !defined( INC_ALL )
  #error You need to predefine INC_ALL in your project file
#endif /* Checks for various compiler/OS-dependant include paths */

/* If we're on a compiler that supports it, set a flag to only include 
   header files once.  Note that on IBM systems this is supported by
   xlc (__xlc__) but not c89 (__IBMC__) */

#if defined( __CC_ARM ) || defined( __clang__ ) || \
	( defined( __GNUC__ ) && __GNUC__ >= 4 ) || defined( __HP_cc ) || \
	defined( __IAR_SYSTEMS_ICC__ ) || defined( __INTEL_COMPILER ) || \
	defined( _MSC_VER ) || \
	( defined( __SUNPRO_C ) && ( __SUNPRO_C >= 0x5140 ) ) || \
	defined( __xlc__ ) 
  #pragma once
#endif /* Compilers that support pragma once */

/* Enable optional newer Posix features if they're available.  This is a 
   majorly problematic define to use because although it gives us access to
   newer capabilities that may not be present by default, compiler vendors
   really, really want to use it to show off how clever they are as rules
   lawyers, so that _POSIX_C_SOURCE gives them free reign to abort the 
   compile if you don't navigate the maze of often compiler-specific options
   and defines needed to comply with the requirements of fifteen different
   standards documents, as interpreted by the compiler vendor.  For example
   SunPro C's sys/feature_tests.h performs a complex series of checks to 
   enforce rules such as "if you define this but don't define that and also 
   define the other and it's the second Tuesday of the month, stop with a 
   #error rather than continuing".  In theory we can get past the SunPro 
   checks by defining the Sun-specific _STDC_C99 and _XPG6, but this still 
   produces internal errors in headers due to various things being enabled 
   or not enabled by various defines.

   As a result, we use a whitelist approach and only define _POSIX_C_SOURCE 
   if there's a reasonable chance of the compile being able to continue.  
   For IBM compilers this means restricting ourselves to Posix 2001, for gcc
   we can at least use Posix 2008, but then defining it has the braindamaged 
   effect of disabling the use of all BSD-specific defines and data types, 
   which the networking headers are riddled with.  In order to deal with 
   this additionally define _BSD_SOURCE to re-enable all the defines and 
   types that _POSIX_C_SOURCE takes away.  However, in a further bit of
   braindamage, glibc went from _BSD_SOURCE in 2.19 to _DEFAULT_SOURCE in
   2.20, and the headers complain if they see _BSD_SOURCE defined.  Since
   there's no way to tell whether glibc wants to see _BSD_SOURCE or
   _DEFAULT_SOURCE, we take advantage of the fact that defining 
   _DEFAULT_SOURCE takes precedence over _BSD_SOURCE, so the warnings go
   away.
   
   The value that we can define for _POSIX_C_SOURCE also varies from OS to
   OS, in theory n-1 is a subset of n but some OSes will only accept n while
   others want to see n-1 */

#ifndef _POSIX_C_SOURCE 
  #if defined( __xlc__ ) || defined( __IBMC__ )
	#define _POSIX_C_SOURCE			200112L		/* Posix 2001 */
  #elif defined( __GNUC__ )
	#define _POSIX_C_SOURCE			200809L		/* Posix 2008 */
	#define _DEFAULT_SOURCE			1			/* See note above */
	#define _BSD_SOURCE				1			/* Undo breakage */
  #endif /* Compiler-specific _POSIX_C_SOURCE usage */
#endif /* _POSIX_C_SOURCE  */

/* Enable use of the TR 24731 safe stdlib extensions if they're available */

#if !defined( __STDC_WANT_LIB_EXT1__ )
  #define __STDC_WANT_LIB_EXT1__	1
#endif /* TR 24731 safe stdlib extensions */

/* If we're building under Win32, don't haul in the huge amount of cruft
   that windows.h brings with it.  We need to define these values before
   we include cryptlib.h since this is where windows.h is included */

#if ( defined( _WINDOWS ) || defined( WIN32 ) || defined( _WIN32 ) || \
	  defined( __WIN32__ ) ) && !defined( _SCCTK )
  #define NOATOM			/* Atom Manager routines */
  #define NOMCX				/* Modem Configuration Extensions */
/*#define NOCLIPBOARD		// Clipboard routines, needed for randomness polling */
  #define NOCOLOR			/* Screen colors */
  #define NOCOMM			/* COMM driver routines */
  #define NOCTLMGR			/* Control and Dialog routines */
  #define NODEFERWINDOWPOS	/* DeferWindowPos routines */
  #define NODRAWTEXT		/* DrawText() and DT_* */
  #define NOGDI				/* All GDI defines and routines */
  #define NOGDICAPMASKS		/* CC_*, LC_*, PC_*, CP_*, TC_*, RC_ */
  #define NOHELP			/* Help engine interface */
  #define NOICONS			/* IDI_* */
  #define NOKANJI			/* Kanji support stuff */
  #define NOKEYSTATES		/* MK_* */
  #define NOMB				/* MB_* and MessageBox() */
  #define NOMCX				/* Modem Configuration Extensions */
  #define NOMEMMGR			/* GMEM_*, LMEM_*, GHND, LHND, etc */
  #define NOMENUS			/* MF_* */
  #define NOMETAFILE		/* typedef METAFILEPICT */
  #if defined( _MSC_VER ) && ( _MSC_VER > 800 )
	#define NOMSG			/* typedef MSG and associated routines */
  #endif /* !Win16 */
  #define NONLS				/* NLS routines */
  #define NOPROFILER		/* Profiler interface */
  #define NORASTEROPS		/* Binary and Tertiary raster ops */
  #define NOSCROLL			/* SB_* and scrolling routines */
  #define NOSERVICE			/* All Service Controller routines, SERVICE_* */
  #define NOSHOWWINDOW		/* SW_* */
  #define NOSOUND			/* Sound driver routines */
  #define NOSYSCOMMANDS		/* SC_* */
  #define NOSYSMETRICS		/* SM_* */
  #define NOTEXTMETRIC		/* typedef TEXTMETRIC and associated routines */
  #define NOVIRTUALKEYCODES	/* VK_* */
  #define NOWH				/* SetWindowsHook and WH_* */
  #define NOWINMESSAGES		/* WM_*, EM_*, LB_*, CB_* */
  #define NOWINOFFSETS		/* GWL_*, GCL_*, associated routines */
  #define NOWINSTYLES		/* WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_* */
  #define OEMRESOURCE		/* OEM Resource values */
#endif /* Win32 */

/* The Palm OS SDK compiler tries to make enums as small as possible (8-bit
   unsigned chars if it can, otherwise 16-bit unsigned shorts, otherwise
   ints) for backwards-compatibility with the old 68K-based Palm interface,
   which causes severe problems for code that assumes that enum == int
   (this occurs in a number of places where an integer parameter is used to
   pass a generic value to/from a function).  CodeWarrior allows this enum
   behaviour to be turned off, but pacc doesn't.

   Similarly, the MSDOS-derived (!!) Watcom C compiler used with older
   versions of QNX 4.x uses 16-bit enums (DOS 16-bit ints) if possible, and
   again there's no way to disable this behaviour (there is with newer
   versions, the pragma to fix the problem is used further down).

   To fix this, we take advantage of the fact that every typedef'd enum has
   a _LAST member as the last entry and override it to include an additional
   value that forces the enum range into the 32-bit int range */

#if ( defined( __PALMSOURCE__ ) && defined( _PACC_VER ) ) || \
	( defined( __QNX__ ) && ( OSVERSION <= 4 ) )
  #define NEED_ENUMFIX			/* Remember to undo defines later */

  /* cryptlib.h */
  #define CRYPT_ALGO_LAST		CRYPT_ALGO_LAST, CRYPT_ALGO_ENUM = -50000
  #define CRYPT_MODE_LAST		CRYPT_MODE_LAST, CRYPT_MODE_ENUM = -50000
  #define CRYPT_KEYSET_LAST		CRYPT_KEYSET_LAST, CRYPT_KEYSET_ENUM = -50000
  #define CRYPT_DEVICE_LAST		CRYPT_DEVICE_LAST, CRYPT_DEVICE_ENUM = -50000
  #define CRYPT_CERTTYPE_LAST	CRYPT_CERTTYPE_LAST, CRYPT_CERTTYPE_ENUM = -50000
  #define CRYPT_FORMAT_LAST		CRYPT_FORMAT_LAST, CRYPT_FORMAT_ENUM = -50000
  #define CRYPT_SESSION_LAST	CRYPT_SESSION_LAST, CRYPT_SESSION_ENUM = -50000
  #define CRYPT_USER_LAST		CRYPT_USER_LAST, CRYPT_USER_ENUM = -50000
  #define CRYPT_IATTRIBUTE_LAST	CRYPT_IATTRIBUTE_LAST, CRYPT_IATTRIBUTE_ENUM = -50000
  #define CRYPT_CRLEXTREASON_LAST	CRYPT_CRLEXTREASON_LAST, CRYPT_CRLEXTREASON_ENUM = -50000
  #define CRYPT_CONTENT_LAST	CRYPT_CONTENT_LAST, CRYPT_CONTENT_ENUM = -50000
  #define CRYPT_SIGNATURELEVEL_LAST	CRYPT_SIGNATURELEVEL_LAST, CRYPT_SIGNATURELEVEL_ENUM = -50000
  #define CRYPT_CERTFORMAT_LAST	CRYPT_CERTFORMAT_LAST
  #define CRYPT_REQUESTTYPE_LAST	CRYPT_REQUESTTYPE_LAST, CRYPT_REQUESTTYPE_ENUM = -50000
  #define CRYPT_KEYID_LAST		CRYPT_KEYID_LAST, CRYPT_KEYID_ENUM = -50000
  #define CRYPT_OBJECT_LAST		CRYPT_OBJECT_LAST, CRYPT_OBJECT_ENUM = -50000
  #define CRYPT_ERRTYPE_LAST	CRYPT_ERRTYPE_LAST, CRYPT_ERRTYPE_ENUM = -50000
  #define CRYPT_CERTACTION_LAST	CRYPT_CERTACTION_LAST, CRYPT_CERTACTION_ENUM = -50000
  #define CRYPT_KEYOPT_LAST		CRYPT_KEYOPT_LAST, CRYPT_KEYOPT_ENUM = -50000
  /* crypt.h */
  #define KEYFORMAT_LAST		KEYFORMAT_LAST, KEYFORMAT_ENUM = -50000
  #define CERTFORMAT_LAST		CERTFORMAT_LAST, CERTFORMAT_ENUM = -50000
  #define MANAGEMENT_ACTION_LAST	MANAGEMENT_ACTION_LAST, MANAGEMENT_ACTION_ENUM = -50000
  #define HASH_LAST				HASH_LAST, HASH_ENUM = -50000
  #define ATTR_LAST				ATTR_LAST, ATTR_ENUM = -50000
  /* cryptkrn.h */
  #define MESSAGE_COMPARE_LAST	MESSAGE_COMPARE_LAST, MESSAGE_COMPARE_ENUM = -50000
  #define MESSAGE_CHECK_LAST	MESSAGE_CHECK_LAST, MESSAGE_CHECK_ENUM = -50000
  #define MESSAGE_CHANGENOTIFY_LAST	MESSAGE_CHANGENOTIFY_LAST, MESSAGE_CHANGENOTIFY_ENUM = -50000
  #define MECHANISM_LAST		MECHANISM_LAST, MECHANISM_ENUM = -50000
  #define KEYMGMT_ITEM_LAST		KEYMGMT_ITEM_LAST, KEYMGMT_ITEM_ENUM = -50000
  #define SEMAPHORE_LAST		SEMAPHORE_LAST, SEMAPHORE_ENUM = -50000
  #define MUTEX_LAST			MUTEX_LAST, MUTEX_ENUM = -50000
  /* cert/cert.h */
  #define RTCSRESPONSE_TYPE_LAST	RTCSRESPONSE_TYPE_LAST, RTCSRESPONSE_TYPE_ENUM = -50000
  #define ATTRIBUTE_LAST		ATTRIBUTE_LAST, ATTRIBUTE_ENUM = -50000
  #define POLICY_LAST			POLICY_LAST, POLICY_ENUM = -50000
  #define SELECTION_OPTION_LAST	SELECTION_OPTION_LAST, SELECTION_OPTION_ENUM = -50000
  /* context/context.h */
  #define CONTEXT_LAST			CONTEXT_LAST, CONTEXT_ENUM = -50000
  /* device/capabil.h */
  #define CAPABILITY_INFO_LAST	CAPABILITY_INFO_LAST, CAPABILITY_INFO_ENUM = -50000
  /* enc_dec/asn1.h */
  #define BER_ID_LAST			BER_ID_LAST, BER_ID_ENUM = -50000
  /* envelope/envelope.h */
  #define ACTION_LAST			ACTION_LAST, ACTION_ENUM = -50000
  #define ACTION_RESULT_LAST	ACTION_RESULT_LAST, ACTION_RESULT_ENUM = -50000
  #define STATE_LAST			STATE_LAST, STATE_ENUM = -50000
  #define ENVSTATE_LAST			ENVSTATE_LAST, ENVSTATE_ENUM = -50000
  #define DEENVSTATE_LAST		DEENVSTATE_LAST, DEENVSTATE_ENUM = -50000
  #define PGP_DEENVSTATE_LAST	PGP_DEENVSTATE_LAST, PGP_DEENVSTATE_ENUM = -50000
  #define SEGHDRSTATE_LAST		SEGHDRSTATE_LAST, SEGHDRSTATE_ENUM = -50000
  /* kernel/acl.h */
  #define RANGEVAL_LAST			RANGEVAL_LAST, RANGEVAL_ENUM = -50000
  #define ATTRIBUTE_VALUE_LAST	ATTRIBUTE_VALUE_LAST, ATTRIBUTE_VALUE_ENUM = -50000
  #define PARAM_VALUE_LAST		PARAM_VALUE_LAST, PARAM_VALUE_ENUM = -50000
  /* kernel/kernel.h */
  #define SEMAPHORE_STATE_LAST	SEMAPHORE_STATE_LAST, SEMAPHORE_STATE_ENUM = -50000
  /* keyset/dbms.h */
  #define CERTADD_LAST			CERTADD_LAST, CERTADD_ENUM = -50000
  /* keyset/keyset.h */
  #define KEYSET_SUBTYPE_LAST	KEYSET_SUBTYPE_LAST, KEYSET_SUBTYPE_ENUM = -50000
  #define DBMS_QUERY_LAST		DBMS_QUERY_LAST, DBMS_QUERY_ENUM = -50000
  #define DBMS_UPDATE_LAST		DBMS_UPDATE_LAST, DBMS_UPDATE_ENUM = -50000
  #define DBMS_CACHEDQUERY_LAST	DBMS_CACHEDQUERY_LAST, DBMS_CACHEDQUERY_ENUM = -50000
  /* keyset/pkcs15.h */
  #define PKCS15_SUBTYPE_LAST	PKCS15_SUBTYPE_LAST, PKCS15_SUBTYPE_ENUM = -50000
//  #define PKCS15_OBJECT_LAST	PKCS15_OBJECT_LAST, PKCS15_OBJECT_ENUM = -50000
  #define PKCS15_KEYID_LAST		PKCS15_KEYID_LAST, PKCS15_KEYID_ENUM = -50000
  /* misc/pgp.h */
  #define PGP_ALGOCLASS_LAST	PGP_ALGOCLASS_LAST, PGP_ALGOCLASS_ENUM = -50000
  /* misc/rpc.h */
  #define COMMAND_LAST			COMMAND_LAST, COMMAND_ENUM = -50000
  #define DBX_COMMAND_LAST		DBX_COMMAND_LAST, DBX_COMMAND_ENUM = -50000
  /* io/stream.h */
  #define STREAM_TYPE_LAST		STREAM_TYPE_LAST, STREAM_TYPE_ENUM = -50000
  #define BUILDPATH_LAST		BUILDPATH_LAST, BUILDPATH_ENUM = -50000
  #define STREAM_IOCTL_LAST		STREAM_IOCTL_LAST, STREAM_IOCTL_ENUM = -50000
  #define STREAM_PROTOCOL_LAST	STREAM_PROTOCOL_LAST, STREAM_PROTOCOL_ENUM = -50000
  #define URL_TYPE_LAST			URL_TYPE_LAST, URL_TYPE_ENUM = -50000
  #define NET_OPTION_LAST		NET_OPTION_LAST, NET_OPTION_ENUM = -50000
  /* session/cmp.h */
  #define CMPBODY_LAST			CMPBODY_LAST, CMPBODY_ENUM = -50000
  /* session/session.h */
  #define READINFO_LAST			READINFO_LAST, READINFO_ENUM = -50000
  /* session/ssh.h */
  #define CHANNEL_LAST			CHANNEL_LAST, CHANNEL_ENUM = -50000
  #define MAC_LAST				MAC_LAST, MAC_ENUM = -50000
  #define SSH_ATRIBUTE_LAST		SSH_ATRIBUTE_LAST, SSH_ATRIBUTE_ENUM = -50000
  /* session/ssl.h */
  #define SSL_LAST				SSL_LAST, SSL_ENUM = -50000
  #define TLS_EXT_LAST			TLS_EXT_LAST, TLS_EXT_ENUM = -50000
#endif /* Palm SDK compiler enum fix */

/* Global headers used in almost every module.  Before includng these, we 
   set the magic define that enables safe(r) C library functions, which for
   some unfathomable reason are disabled by default, like disabling the
   seatbelts in a car */

#ifndef __STDC_WANT_LIB_EXT1__
  #define __STDC_WANT_LIB_EXT1__	1
#endif /* __STDC_WANT_LIB_EXT1__ */
#if !defined( NDEBUG) && ( defined( __MVS__ ) || defined( __VMCMS__ ) )
  /* IBM mainframe debug builds need extra functions for diagnostics 
	 support */
  #define _OPEN_SYS_ITOA_EXT
#endif /* IBM big iron debug build */
#if defined( __APPLE__ ) 
  /* Apple headers rely on a pile of BSD-isms that don't get defined unless 
     _DARWIN_C_SOURCE is defined */
  #define _DARWIN_C_SOURCE
#endif /* Apple */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* If the global cryptlib header hasn't been included yet, include it now */

#ifndef _CRYPTLIB_DEFINED
  #include "cryptlib.h"
#endif /* _CRYPTLIB_DEFINED */

/* Since some of the _LAST types are used in the code, we have to undefine
   them again if they've been used in the enum-fix kludge */

#ifdef NEED_ENUMFIX
  #undef CRYPT_ALGO_LAST
  #undef CRYPT_MODE_LAST
  #undef CRYPT_KEYSET_LAST
  #undef CRYPT_DEVICE_LAST
  #undef CRYPT_CERTTYPE_LAST
  #undef CRYPT_FORMAT_LAST
  #undef CRYPT_SESSION_LAST
  #undef CRYPT_USER_LAST
  #undef CRYPT_IATTRIBUTE_LAST
  #undef CRYPT_CRLEXTREASON_LAST
  #undef CRYPT_CONTENT_LAST
  #undef CRYPT_SIGNATURELEVEL_LAST
  #undef CRYPT_CERTFORMAT_LAST
  #undef CRYPT_REQUESTTYPE_LAST
  #undef CRYPT_KEYID_LAST
  #undef CRYPT_OBJECT_LAST
  #undef CRYPT_ERRTYPE_LAST
  #undef CRYPT_CERTACTION_LAST
  #undef CRYPT_KEYOPT_LAST
#endif /* NEED_ENUMFIX */

/****************************************************************************
*																			*
*						System- and Compiler-Specific Defines				*
*																			*
****************************************************************************/

/* Pull in the system and compiler-specific defines and values.  This 
   detects the system config and is used by config.h */

#if defined( INC_ALL )
  #include "os_detect.h"
#else
  #include "misc/os_detect.h"
#endif /* Compiler-specific includes */

/* Pull in the source code analysis header */

#if defined( INC_ALL )
  #include "analyse.h"
#else
  #include "misc/analyse.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Config Options								*
*																			*
****************************************************************************/

/* Pull in the cryptlib initialisation options file, which contains the
   various USE_xxx defines that enable different cryptlib features */

#if defined( INC_ALL )
  #include "config.h"
#else
  #include "misc/config.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						System- and Compiler-Specific Interface				*
*																			*
****************************************************************************/

/* Pull in the system and compiler-specific interface definitions.  This 
   uses the output from config.h to enable/disable system-specific 
   interfaces and options */

#if defined( INC_ALL )
  #include "os_spec.h"
#else
  #include "misc/os_spec.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						Data Size and Crypto-related Constants				*
*																			*
****************************************************************************/

/* Pull in the data-size and crypt-related constants */

#if defined( INC_ALL )
  #include "consts.h"
#else
  #include "misc/consts.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Kernel Interface							*
*																			*
****************************************************************************/

/* Pull in the cryptlib kernel interface defines */

#include "cryptkrn.h"

/****************************************************************************
*																			*
*								Portability Defines							*
*																			*
****************************************************************************/

/* Read/write values as 32-bit big-endian data in cases where we're not 
   dealing with a stream.  Used to sample data from the crypto RNG to detect 
   stuck-at faults and in the debug version of clAlloc() */

#define mgetLong( memPtr ) \
		( ( ( unsigned long ) memPtr[ 0 ] << 24 ) | \
		  ( ( unsigned long ) memPtr[ 1 ] << 16 ) | \
		  ( ( unsigned long ) memPtr[ 2 ] << 8 ) | \
		    ( unsigned long ) memPtr[ 3 ] ); \
		memPtr += 4

#define mputLong( memPtr, data ) \
		memPtr[ 0 ] = ( BYTE ) ( ( ( data ) >> 24 ) & 0xFF ); \
		memPtr[ 1 ] = ( BYTE ) ( ( ( data ) >> 16 ) & 0xFF ); \
		memPtr[ 2 ] = ( BYTE ) ( ( ( data ) >> 8 ) & 0xFF ); \
		memPtr[ 3 ] = ( BYTE ) ( ( data ) & 0xFF ); \
		memPtr += 4

/****************************************************************************
*																			*
*								Data Structures								*
*																			*
****************************************************************************/

/* Information on exported key/signature data.  This is an extended version
   of the data returned by the externally-visible cryptQueryObject() routine */

#define AUTHENCPARAM_MAX_SIZE	128

typedef struct {
	/* Object format and status information */
	CRYPT_FORMAT_TYPE formatType;	/* Object format type */
	CRYPT_OBJECT_TYPE type;			/* Object type */
	long size;						/* Object size */
	VALUE( 0, 10 ) \
	int version;					/* Object format version */

	/* The encryption algorithm and mode */
	CRYPT_ALGO_TYPE cryptAlgo;		/* The encryption algorithm */
	CRYPT_MODE_TYPE cryptMode;		/* The encryption mode */
	int cryptAlgoParam;				/* Optional algorithm parameter */
	int cryptAlgoEncoding;			/* Optional encoding ALGOID_ENCODING_xxx */

	/* The key ID for public key objects */
	BUFFER( CRYPT_MAX_HASHSIZE, keyIDlength ) \
	BYTE keyID[ CRYPT_MAX_HASHSIZE + 8 ];/* PKC key ID */
	VALUE( 0, CRYPT_MAX_HASHSIZE ) \
	int keyIDlength;

	/* The IV for conventionally encrypted data */
	BUFFER( CRYPT_MAX_IVSIZE, ivLength ) \
	BYTE iv[ CRYPT_MAX_IVSIZE + 8 ];/* IV */
	VALUE( 0, CRYPT_MAX_IVSIZE ) \
	int ivLength;

	/* The key derivation algorithm and iteration count for conventionally
	   encrypted keys */
	CRYPT_ALGO_TYPE keySetupAlgo;	/* Key setup algorithm */
	int keySetupAlgoParam;			/* Optional parameter for key setup algo */
	int keySetupIterations;			/* Key setup iteration count */
	int keySize;					/* Key size (if not implicit) */
	BUFFER( CRYPT_MAX_HASHSIZE, saltLength ) \
	BYTE salt[ CRYPT_MAX_HASHSIZE + 8 ];/* Key setup salt */
	VALUE( 0, CRYPT_MAX_HASHSIZE ) \
	int saltLength;

	/* The hash algorithm for signatures */
	CRYPT_ALGO_TYPE hashAlgo;		/* Hash algorithm */
	int hashAlgoParam;				/* Optional algorithm parameter */

	/* The encoded parameter data for authenticated encryption, and the
	   optional KDF and encryption and MAC algorithm parameter data within 
	   that */
	BUFFER( AUTHENCPARAM_MAX_SIZE, authEncParamLength ) \
	BYTE authEncParamData[ AUTHENCPARAM_MAX_SIZE + 8 ];
	VALUE( 0, AUTHENCPARAM_MAX_SIZE ) \
	int authEncParamLength;			/* AuthEnc parameter data */
	int kdfParamStart, kdfParamLength;	/* Position of opt.KDF params */
	int encParamStart, encParamLength;	/* Position of enc.parameters */
	int macParamStart, macParamLength;	/* Position of MAC parameters */

	/* The start and length of the payload data, either the encrypted key or
	   the signature data */
	int dataStart, dataLength;

	/* The start and length of the issuerAndSerialNumber, authenticated 
	   attributes, and unauthenticated attributes for CMS objects */
	int iAndSStart, iAndSLength;
	int attributeStart, attributeLength;
	int unauthAttributeStart, unauthAttributeLength;
	} QUERY_INFO;

/* DLP algorithms require composite parameters when en/decrypting and
   signing/sig checking, so we can't just pass in a single buffer full of
   data as we can with RSA.  In addition the data length changes, for
   example for a DSA sig we pass in a 20-byte hash and get back a ~50-byte
   sig, for sig.checking we pass in a 20-byte hash and ~50-byte sig and get
   back nothing.  Because of this we have to use the following structure to
   pass data to the DLP-based PKCs */

typedef struct {
	BUFFER_FIXED( inLen1 ) \
	const BYTE *inParam1;
	BUFFER_OPT_FIXED( inLen2 ) \
	const BYTE *inParam2;				/* Input parameters */
	BUFFER_FIXED( outLen ) \
	BYTE *outParam;						/* Output parameter */
	int inLen1, inLen2, outLen;			/* Parameter lengths */
	CRYPT_FORMAT_TYPE formatType;		/* Paramter format type */
	} DLP_PARAMS;

#define setDLPParams( dlpDataPtr, dataIn, dataInLen, dataOut, dataOutLen ) \
	{ \
	memset( ( dlpDataPtr ), 0, sizeof( DLP_PARAMS ) ); \
	( dlpDataPtr )->formatType = CRYPT_FORMAT_CRYPTLIB; \
	( dlpDataPtr )->inParam1 = ( dataIn ); \
	( dlpDataPtr )->inLen1 = ( dataInLen ); \
	( dlpDataPtr )->outParam = ( dataOut ); \
	( dlpDataPtr )->outLen = ( dataOutLen ); \
	}

/* When calling key agreement functions we have to pass a mass of cruft
   around instead of the usual flat data (even more than the generic DLP
   parameter information) for which we use the following structure.  The
   public value is the public key value used for the agreement process,
   typically y = g^x mod p for DH-like mechanisms.  The ukm is the user
   keying material, typically something which is mixed into the DH process
   to make the new key unique.  The wrapped key is the output (originator)/
   input(recipient) to the keyagreement process.  The session key context
   contains a context into which the derived key is loaded.  Typical
   examples of use are:

	PKCS #3: publicValue = y
	S/MIME: publicValue = y, ukm = 512-bit nonce, wrappedKey = g^x mod p
	SSH, SSL: publicValue = y, wrappedKey = x */

typedef struct {
	BUFFER( CRYPT_MAX_PKCSIZE, publicValueLen ) \
	BYTE publicValue[ CRYPT_MAX_PKCSIZE + 8 ];
	VALUE( 0, CRYPT_MAX_PKCSIZE ) \
	int publicValueLen;				/* Public key value */
	BUFFER( CRYPT_MAX_PKCSIZE, wrappedKeyLen ) \
	BYTE wrappedKey[ CRYPT_MAX_PKCSIZE + 8 ];
	VALUE( 0, CRYPT_MAX_PKCSIZE ) \
	int wrappedKeyLen;				/* Wrapped key */
	} KEYAGREE_PARAMS;

/****************************************************************************
*																			*
*								Useful General Macros						*
*																			*
****************************************************************************/

/* Reasonably reliable way to get rid of unused argument warnings in a
   compiler-independant manner */

#define UNUSED_ARG( arg )	( ( arg ) = ( arg ) )

/* Although min() and max() aren't in the ANSI standard, most compilers have
   them in one form or another, but just enough don't that we need to define 
   them ourselves in some cases */

#if !defined( min )
  #ifdef MIN
	#define min			MIN
	#define max			MAX
  #else
	#define min( a, b )	( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
	#define max( a, b )	( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
  #endif /* Various min/max macros */
#endif /* !min/max */

/* Macros to convert to and from the bit counts used for some encryption
   parameters */

#define bitsToBytes( bits )			( ( ( bits ) + 7 ) >> 3 )
#define bytesToBits( bytes )		( ( bytes ) << 3 )

/* When initialising a static block of bytes, it's useful to be able to 
   specify it as a character string, however this runs into problems with
   the fact that the default char type is signed.  To get around this the
   following macro declares a byte string as a set of unsigned bytes */

#define MKDATA( x )					( ( BYTE * ) ( x ) )

/* Macro to round a value up to the nearest multiple of a second value,
   with the second value being a power of 2 */

#define roundUp( size, roundSize ) \
	( ( ( size ) + ( ( roundSize ) - 1 ) ) & ~( ( roundSize ) - 1 ) )

/* A macro to clear sensitive data from memory.  This is somewhat easier to
   use than calling memset with the second parameter set to 0 all the time,
   and makes it obvious where sensitive data is being erased.  In addition
   some systems, recognising the problem of compilers removing what they see
   as dead stores, have distinct memory zeroisation support, so if available 
   we use that */

#if defined( _MSC_VER ) && VC_GE_2005( _MSC_VER )
  /* This is just a mapping to RtlSecureZeroMemory() (via WinBase.h) which 
     is implemented as inline code implementing a loop on a pointer declared 
	 volatile, but unlike the corresponding RtlZeroMemory() there's a 
	 contract that this will always zeroise memory even in the face of 
	 compiler changes that would otherwise optimise away the access */
  #define zeroise( memory, size )	SecureZeroMemory( memory, size )
#elif defined( __STDC_LIB_EXT1__ )
  /* C11 defines a function memset_s() that guarantees that it won't be
	 optimised away, although this is quite well obfuscated in the spec,
	 "the memory indicated by [the memset parameters] may be accessible in 
	 the future and therefore must contain the values indicated by [the
	 value to set]", hopefully the implementers will know that this equates
	 to "the memset_s() call can't be optimised away" */
  #define zeroise( memory, size )	memset_s( memory, size, 0, size )
#elif defined( __OpenBSD__ )
  /* The OpenBSD folks defined their own won't-be-optimised-away bzero()
	 function */
  #define zeroise( memory, size )	explicit_bzero( memory, size )
#else
  #define zeroise( memory, size )	memset( memory, 0, size )
#endif /* Systems with distinct zeroise functions */

/* A macro to check that a value is a possibly valid handle.  This doesn't
   check that the handle refers to a valid object, merely that the value is
   in the range for valid handles.  The full function isValidHandle() used
   in the kernel does check that the handle refers to a valid object, being
   more than just a range check */

#define isHandleRangeValid( handle ) \
		( ( handle ) > NO_SYSTEM_OBJECTS - 1 && ( handle ) < MAX_NO_OBJECTS )

/* A macro to check whether an encryption mode needs an IV or not */

#define needsIV( mode )	( ( mode ) == CRYPT_MODE_CBC || \
						  ( mode ) == CRYPT_MODE_CFB || \
						  ( mode ) == CRYPT_MODE_GCM )

/* A macro to check whether an algorithm is a pure stream cipher (that is,
   a real stream cipher rather than just a block cipher run in a stream
   mode) */

#define isStreamCipher( algorithm )		( ( algorithm ) == CRYPT_ALGO_RC4 )

/* Non-stream ciphers consist of { algorithm, mode } pairs, however pure
   stream ciphers don't have any explicit mode, so we define a pseudo-mode
   that can be used as a placeholder.  Using CRYPT_MODE_ECB, which is 
   IV-less, conveniently means that we pass any needsIV() checks */

#define CRYPT_PSEUDOMODE_RC4			CRYPT_MODE_ECB

/* A macro to check whether an algorithm is regarded as being (relatively)
   insecure or not.  This is used by some of the higher-level internal
   routines that normally use the default algorithm set in the configuration
   database if nothing else is explicitly specified, but that specifically
   check for the weaker algorithms and use something stronger instead if a
   weak algorithm is specified.  This is done both for luser-proofing and to
   avoid possible problems from a trojan patching the configuration
   database */

#define isWeakCryptAlgo( algorithm )	( ( algorithm ) == CRYPT_ALGO_DES || \
										  ( algorithm ) == CRYPT_ALGO_RC2 || \
										  ( algorithm ) == CRYPT_ALGO_RC4 )
#define isWeakHashAlgo( algorithm )		( ( algorithm ) == CRYPT_ALGO_MD5 )
#define isWeakMacAlgo( algorithm )		( 0 )
										/* None left with HMAC-MD5 deprecated */

/* Macros to check for membership in overall algorithm classes */

#define isConvAlgo( algorithm ) \
		( ( algorithm ) >= CRYPT_ALGO_FIRST_CONVENTIONAL && \
		  ( algorithm ) <= CRYPT_ALGO_LAST_CONVENTIONAL )
#define isPkcAlgo( algorithm ) \
		( ( algorithm ) >= CRYPT_ALGO_FIRST_PKC && \
		  ( algorithm ) <= CRYPT_ALGO_LAST_PKC )
#define isHashAlgo( algorithm ) \
		( ( algorithm ) >= CRYPT_ALGO_FIRST_HASH && \
		  ( algorithm ) <= CRYPT_ALGO_LAST_HASH )
#define isHashMacExtAlgo( algorithm ) \
		( ( algorithm ) == CRYPT_ALGO_SHA2 || \
		  ( algorithm ) == CRYPT_ALGO_SHAng || \
		  ( algorithm ) == CRYPT_ALGO_HMAC_SHA2 || \
		  ( algorithm ) == CRYPT_ALGO_HMAC_SHAng )
#define isMacAlgo( algorithm ) \
		( ( algorithm ) >= CRYPT_ALGO_FIRST_MAC && \
		  ( algorithm ) <= CRYPT_ALGO_LAST_MAC )
#define isSpecialAlgo( algorithm ) \
		( ( algorithm ) == CRYPT_IALGO_GENERIC_SECRET )

/* Macros to check whether a PKC algorithm is useful for a certain purpose 
   or requires special-case handling.  Note that isDlpAlgo() doesn't include 
   the ECC algorithms, which are also based on the DLP (although in this 
   case the ECDLP and not the standard DLP).  This is a bit ugly but it's 
   used in various places to distinguish DLP-based PKCs from non-DLP-based
   PKCs, while ECDLP-based-PKCs are in a separate class.  This means that
   when checking for the extended class { DLP | ECDLP } it's necessary to
   explicitly include isEccAlgo() alongside isDlpAlgo() */

#define isSigAlgo( algorithm ) \
	( ( algorithm ) == CRYPT_ALGO_RSA || ( algorithm ) == CRYPT_ALGO_DSA || \
	  ( algorithm ) == CRYPT_ALGO_ECDSA )
#define isCryptAlgo( algorithm ) \
	( ( algorithm ) == CRYPT_ALGO_RSA || ( algorithm ) == CRYPT_ALGO_ELGAMAL )
#define isKeyxAlgo( algorithm ) \
	( ( algorithm ) == CRYPT_ALGO_DH || ( algorithm ) == CRYPT_ALGO_ECDH )
#define isDlpAlgo( algorithm ) \
	( ( algorithm ) == CRYPT_ALGO_DSA || ( algorithm ) == CRYPT_ALGO_ELGAMAL || \
	  ( algorithm ) == CRYPT_ALGO_DH )
#define isEccAlgo( algorithm ) \
	( ( algorithm ) == CRYPT_ALGO_ECDSA || ( algorithm ) == CRYPT_ALGO_ECDH )

/* Macros to check whether an algorithm has additional parameters that need 
   to be handled explicitly */

#define isParameterisedConvAlgo( algorithm ) \
	( ( algorithm ) == CRYPT_ALGO_AES )
#define isParameterisedHashAlgo( algorithm ) \
	( ( algorithm ) == CRYPT_ALGO_SHA2 || ( algorithm ) == CRYPT_ALGO_SHAng )
#define isParameterisedMacAlgo( algorithm ) \
	( ( algorithm ) == CRYPT_ALGO_HMAC_SHA2 || \
	  ( algorithm ) == CRYPT_ALGO_HMAC_SHAng )

/* A macro to check whether an error status is related to a data-formatting
   problem or some other problem.  This is used to provide extended string-
   format error information, if it's a data error then the message being
   processed was (probably) invalid, if it's not a data error then it may be
   due to an invalid decryption key being used or something similar that's
   unrelated to the message itself.
   
   The exact definition of what constitutes a "data error" is a bit vague 
   but since it's only used to control what additional error information is
   returned a certain level of fuzziness is permitted */

#define isDataError( status ) \
		( ( status ) == CRYPT_ERROR_OVERFLOW || \
		  ( status ) == CRYPT_ERROR_UNDERFLOW || \
		  ( status ) == CRYPT_ERROR_BADDATA || \
		  ( status ) == CRYPT_ERROR_SIGNATURE || \
		  ( status ) == CRYPT_ERROR_NOTAVAIL || \
		  ( status ) == CRYPT_ERROR_INCOMPLETE || \
		  ( status ) == CRYPT_ERROR_COMPLETE || \
		  ( status ) == CRYPT_ERROR_INVALID )

/* A macro to check whether a public key is too short to be secure.  This
   is a bit more complex than just a range check because any length below 
   about 512 bits is probably a bad data error, while lengths from about
   512 bits to MIN_PKCSIZE (for standard PKCs) or 120 bits to 
   MIN_PKCSIZE_ECC are too-short key errors */

#define isShortPKCKey( keySize ) \
		( ( keySize ) >= MIN_PKCSIZE_THRESHOLD && \
		  ( keySize ) < MIN_PKCSIZE )
#define isShortECCKey( keySize ) \
		( ( keySize ) >= MIN_PKCSIZE_ECC_THRESHOLD && \
		  ( keySize ) < MIN_PKCSIZE_ECC )

/* To avoid problems with signs, for example due to (signed) characters
   being potentially converted to large signed integer values we perform a
   safe conversion by going via an intermediate unsigned value, which in
   the case of char -> int results in 0xFF turning into 0x000000FF rather
   than 0xFFFFFFFF.
   
   For Visual Studio we explicitly mask some values to avoid runtime traps 
   in debug builds */

#define byteToInt( x )				( ( unsigned char ) ( x ) )
#define intToLong( x )				( ( unsigned int ) ( x ) )

#define sizeToInt( x )				( ( unsigned int ) ( x ) )
#if defined( _MSC_VER ) && VC_GE_2010( _MSC_VER )
  #define intToByte( x )			( ( unsigned char ) ( ( x ) & 0xFF ) )
#else
  #define intToByte( x )			( ( unsigned char ) ( x ) )
#endif /* VS 2010 or newer */

/* Clear/set object error information */

#define clearErrorInfo( objectInfoPtr ) \
	{ \
	( objectInfoPtr )->errorLocus = CRYPT_ATTRIBUTE_NONE; \
	( objectInfoPtr )->errorType = CRYPT_OK; \
	}

#define setErrorInfo( objectInfoPtr, locus, type ) \
	{ \
	( objectInfoPtr )->errorLocus = locus; \
	( objectInfoPtr )->errorType = type; \
	}

/****************************************************************************
*																			*
*								Internal API Functions						*
*																			*
****************************************************************************/

/* Pull in the internal API function definitions and prototypes */

#if defined( INC_ALL )
  #include "safety.h"		/* Must be before int_api.h for safe pointers */
  #include "int_api.h"
  #include "list.h"
#else
  #include "misc/safety.h"	/* Must be before int_api.h for safe pointers */
  #include "misc/int_api.h"
  #include "misc/list.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Debugging Functions							*
*																			*
****************************************************************************/

/* Pull in the debugging function definitions and prototypes */

#if defined( INC_ALL )
  #include "debug.h"
  #include "fault.h"
#else
  #include "misc/debug.h"
  #include "misc/fault.h"
#endif /* Compiler-specific includes */

#endif /* _CRYPT_DEFINED */
