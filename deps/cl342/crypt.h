/****************************************************************************
*																			*
*					  cryptlib Internal General Header File 				*
*						Copyright Peter Gutmann 1992-2014					*
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

/* If we're on a new enough version of VC++ or Metrowerks, set a flag to
   only include header files once */

#if ( defined( _MSC_VER ) && ( _MSC_VER >= 1000 ) ) || defined ( __MWERKS__ )
  #pragma once
#endif /* VC++ 5.0 or higher, Metrowerks */

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

/* Global headers used in almost every module */

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

/* Pull in the system and compiler-specific defines and values */

#if defined( INC_ALL )
  #include "os_spec.h"
#else
  #include "misc/os_spec.h"
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
   various USE_xxx defines that enable different cryptlib features.  Note
   that this *must* be included after os_spec.h, which performs OS detection
   used by config.h to enable/disable various code features */

#if defined( INC_ALL )
  #include "config.h"
#else
  #include "misc/config.h"
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

/* Read/write values as 16- and 32-bit big-endian data in cases where we're
   not dealing with a stream.  Usage:
   
	mget/putWord(): 
		SSHv1 (disabled by default).
		SOCKS wrapper in network code (disabled by default).

	mget/putLong(): 
		SSHv1 (disabled by default).
		Sampling data from the crypto RNG to detect stuck-at faults.
		Debug version of clAlloc() */

#if defined( USE_SSH1 ) 

#define mgetWord( memPtr ) \
		( ( ( unsigned int ) memPtr[ 0 ] << 8 ) | \
			( unsigned int ) memPtr[ 1 ] ); \
		memPtr += 2

#define mputWord( memPtr, data ) \
		memPtr[ 0 ] = ( BYTE ) ( ( ( data ) >> 8 ) & 0xFF ); \
		memPtr[ 1 ] = ( BYTE ) ( ( data ) & 0xFF ); \
		memPtr += 2
#endif /* USE_SSH1 */

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
	BUFFER( 128, authEncParamLength ) \
	BYTE authEncParamData[ 128 + 8 ];
	VALUE( 0, 128 ) \
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
   as deas stores, have distinct memory zeroisation support, so if available 
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
		( ( handle ) > NO_SYSTEM_OBJECTS - 1 && ( handle ) < MAX_OBJECTS )

/* A macro to check whether an encryption mode needs an IV or not */

#define needsIV( mode )	( ( mode ) == CRYPT_MODE_CBC || \
						  ( mode ) == CRYPT_MODE_CFB || \
						  ( mode ) == CRYPT_MODE_GCM )

/* A macro to check whether an algorithm is a pure stream cipher (that is,
   a real stream cipher rather than just a block cipher run in a stream
   mode) */

#define isStreamCipher( algorithm )		( ( algorithm ) == CRYPT_ALGO_RC4 )

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

/* Perform a range check on a block of memory, checking that 
   { start, length } falls within { 0, totalLength }.  There are two 
   versions of this, the default which requires a nonzero start offset and 
   the special-case variant which allows a zero start offset, which is used 
   for situations like optionally MIME-wrapped data which have a nonzero 
   offset if there's a MIME header to be skipped but a zero offset if it's 
   unencapsulated data */

#define rangeCheck( start, length, totalLength ) \
		( ( start ) <= 0 || ( length ) < 1 || \
		  ( start ) + ( length ) > ( totalLength ) ) ? FALSE : TRUE
#define rangeCheckZ( start, length, totalLength ) \
		( ( start ) < 0 || ( length ) < 1 || \
		  ( start ) + ( length ) > ( totalLength ) ) ? FALSE : TRUE

/* Check the validity of a pointer passed to a cryptlib function.  Usually
   the best that we can do is check that it's not null, but some OSes allow
   for better checking than this, for example that it points to a block of
   readable or writeable memory.  Under Windows IsBadReadPtr() will always
   succeed if the size is 0, so we have to add a separate check to make sure
   that it's non-NULL.

   There are additional caveats with the use of the Windows memory-checking
   functions.  In theory these would be implemented via VirtualQuery(),
   however this is quite slow, requiring a kernel transition and poking
   around with the page protection mechanisms.  Instead, they try and read
   or write the memory, with an exception handler wrapped around the access.
   If the exception is thrown, they fail.  The problem with this way of
   doing things is that if the memory address is a stack guard page used to
   grow the stack (when the system-level exception handler sees an access to
   the bottom-of-stack guard page, it knows that it has to grow the stack)
   *and* the guard page is owned by another thread, IsBadXxxPtr() will catch 
   the exception and the system will never see it, so it can't grow the 
   stack past the current limit (note that this only occurs if the guard 
   page that we hit is owned by a different thread; if we own in then the
   kernel will catch the STATUS_GUARD_PAGE_VIOLATION exception and grow the
   stack as required).  In addition if it's the last guard page then instead 
   of getting an "out of stack" exception, it's turned into a no-op.  The 
   second time the last guard page is hit, the application is terminated by 
   the system, since it's passed its first-chance exception.

   A variation of this is that the calling app could be deliberately passing
   a pointer to a guard page and catching the guard page exception in order
   to dynamically generate the data that would fill the page (this can 
   happen for example when simulating a large address space with pointer 
   swizzling), but this is a pretty weird programming technique that's 
   unlikely to be used with a crypto library.

   A lesser problem is that there's a race condition in the checking, since
   the memory can be unmapped between the IsBadXxxPtr() check and the actual
   access, but you'd pretty much have to be trying to actively subvert the
   checks to do something like this.

   For these reasons we use these functions mostly for debugging, wrapping
   them up in assert()s in most cases where they're used.  Under Windows 
   Vista they've actually been turned into no-ops because of the above 
   problems, although it's probable that they'll be replaced by code to 
   check for NULL pointers, since Microsoft's docs indicate that this much 
   checking will still be done.  If necessary we could also replace the 
   no-op'd out versions with the equivalent code:

	inline BOOL IsBadReadPtr( const VOID *lp, UINT_PTR ucb )
		{
		__try { memcmp( p, p, cb ); }
		__except( EXCEPTION_EXECUTE_HANDLER ) { return( FALSE ); }
		return( TRUE );
		}

	inline BOOL IsBadWritePtr( LPVOID lp, UINT_PTR ucb )
		{
		__try { memset( p, 0, cb ); }
		__except( EXCEPTION_EXECUTE_HANDLER ) { return( FALSE ); }
		return( TRUE );
		} 

   In a number of cases the code is called as 
   isXXXPtr( ptr, sizeof( ptrObject ) ), which causes warnings about 
   constant expressions, to avoid this we define a separate version 
   that avoids the size check.
   
   Under Unix we could in theory check against _etext but this is too 
   unreliable to use, with shared libraries the single shared image can be 
   mapped pretty much anywhere into the process' address space and there can 
   be multiple _etext's present, one per shared library, it fails with 
   SELinux (which is something you'd expect to see used in combination with 
   code that's been carefully written to do things like perform pointer 
   checking), and who knows what it'll do in combination with different 
   approaches to ASLR.  Because of its high level of nonportability (even on 
   the same system it can break depending on whether something like SELinux 
   is enabled or not) it's too dangerous to enable its use */

#if defined( __WIN32__ ) || defined( __WINCE__ )
  /* The use of code analysis complicates the pointer-checking macros
	 because they read memory that's uninitialised at that point.  This is
	 fine because we're only checking for readability/writeability, but the
	 analyser doesn't know this and flags it as an error.  To avoid this,
	 we remove the read/write calls when running the analyser */
  #ifdef _PREFAST_
	#define isReadPtr( ptr, size )	( ( ptr ) != NULL && ( size ) > 0 )
	#define isWritePtr( ptr, size )	( ( ptr ) != NULL && ( size ) > 0 )
	#define isReadPtrConst( ptr, size ) \
									( ( ptr ) != NULL )
	#define isWritePtrConst( ptr, size ) \
									( ( ptr ) != NULL )
  #else
	#define isReadPtr( ptr, size )	( ( ptr ) != NULL && ( size ) > 0 && \
									  !IsBadReadPtr( ( ptr ), ( size ) ) )
	#define isWritePtr( ptr, size )	( ( ptr ) != NULL && ( size ) > 0 && \
									  !IsBadWritePtr( ( ptr ), ( size ) ) )
	#define isReadPtrConst( ptr, size ) \
									( ( ptr ) != NULL && \
									  !IsBadReadPtr( ( ptr ), ( size ) ) )
	#define isWritePtrConst( ptr, size ) \
									( ( ptr ) != NULL && \
									  !IsBadWritePtr( ( ptr ), ( size ) ) )
  #endif /* _PREFAST_ */
#elif defined( __UNIX__ ) && 0		/* See comment above */
  extern int _etext;

  #define isReadPtr( ptr, size )	( ( ptr ) != NULL && \
									  ( void * ) ( ptr ) > ( void * ) &_etext && \
									  ( size ) > 0 )
  #define isWritePtr( ptr, size )	( ( ptr ) != NULL && \
									  ( void * ) ( ptr ) > ( void * ) &_etext && \
									  ( size ) > 0 )
  #define isReadPtrConst( ptr, size ) \
									( ( ptr ) != NULL && \
									  ( void * ) ( ptr ) > ( void * ) &_etext )
  #define isWritePtrConst( ptr, size ) \
									( ( ptr ) != NULL && \
									  ( void * ) ( ptr ) > ( void * ) &_etext )
#else
  #define isReadPtr( ptr, size )	( ( ptr ) != NULL && ( size ) > 0 )
  #define isWritePtr( ptr, size )	( ( ptr ) != NULL && ( size ) > 0 )
  #define isReadPtrConst( ptr, type ) \
									( ( ptr ) != NULL )
  #define isWritePtrConst( ptr, type ) \
									( ( ptr ) != NULL )
#endif /* Pointer check macros */

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
  #include "int_api.h"
#else
  #include "misc/int_api.h"
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
