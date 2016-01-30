/****************************************************************************
*																			*
*						cryptlib Configuration Settings  					*
*					   Copyright Peter Gutmann 1992-2014					*
*																			*
****************************************************************************/

#ifndef _CONFIG_DEFINED

#define _CONFIG_DEFINED

/****************************************************************************
*																			*
*						Custom Configuration Profiles						*
*																			*
****************************************************************************/

/* The following defines can be used to enable specific specific cryptlib 
   profiles that only enable the functionality needed for one particular
   application:

	#define CONFIG_PROFILE_SMIME
	#define CONFIG_PROFILE_PGP
	#define CONFIG_PROFILE_SSL
	#define CONFIG_PROFILE_SSH

   The configuration is set up in the section "Application Profiles" at the
   end of this file.  Note that this sort of thing would normally be done by 
   the build command (e.g. in a makefile), the following is mostly intended 
   for debugging */

#if 0
  #define CONFIG_NO_CERTIFICATES
  #define CONFIG_NO_DEVICES
  #define CONFIG_NO_ENVELOPES
  #define CONFIG_NO_KEYSETS
  #define CONFIG_NO_SESSIONS
  #if defined( _WIN32 ) && defined( _MSC_VER ) && ( _MSC_VER == 1200 )
	#define NO_OBSCURE_FEATURES
  #endif /* Exception for testing rarely-used facilities under VC++ 6.0 */
#endif /* 0 */

/* The following configuration options can be used for custom builds of
   cryptlib to fit constrained environments.  Note that these builds 
   severely constrain the options available for cryptlib use, for example 
   removing certificate support and using CONFIG_USE_PSEUDOCERTIFICATES in 
   combination with CONFIG_PROFILE_SSL requires using a pre-encoded SSL/TLS 
   certificate chain with cryptCreateAttachedCert() to create the server's 
   key, since no certificate import or export capabilities are present */

#if 0	/* Embedded SSL/TLS server */
#define CONFIG_PROFILE_SSL
#define CONFIG_CONSERVE_MEMORY
#define CONFIG_NO_KEYSETS
#define CONFIG_NO_CERTIFICATES
#define CONFIG_NO_DEVICES 
#define CONFIG_NO_ENVELOPES
#define CONFIG_USE_PSEUDOCERTIFICATES
#endif /* 0 */

#if 0	/* Embedded SSL/TLS server with SCEP */
#define CONFIG_CONSERVE_MEMORY
#define CONFIG_NO_CONTEXTS		/* Only a few algorithms */
#define USE_AES
#define USE_DH
#define USE_MD5
#define USE_RSA
#define USE_PKC
#define CONFIG_NO_CERTIFICATES	/* Only basic certificates */
#define USE_CERTIFICATES 
#define USE_CERTLEVEL_STANDARD
#define USE_INT_ASN1
#define CONFIG_NO_DEVICES 
#define CONFIG_NO_ENVELOPES		/* Only CMS envelopes */
#define USE_ENVELOPES
#define USE_CMS
#define USE_INT_CMS
#define CONFIG_NO_KEYSETS		/* Only PKCS #15 keysets */
//#define USE_KEYSETS
//#define USE_PKCS15
//#define USE_FILES
#define CONFIG_NO_SESSIONS		/* Only SCEP sessions */
#define USE_SESSIONS
#define USE_SSL
#define USE_SCEP
#endif /* 0 */

/* The blanket low-memory configuration option changes other configuration 
   settings as well */

#ifdef CONFIG_CONSERVE_MEMORY
  #ifndef CONFIG_NUM_OBJECTS
	#define CONFIG_NUM_OBJECTS		128
  #endif /* CONFIG_NUM_OBJECTS */
  #ifndef CONFIG_PKC_ALLOCSIZE
	#define CONFIG_PKC_ALLOCSIZE	256
  #endif /* CONFIG_PKC_ALLOCSIZE */
#endif /* CONFIG_CONSERVE_MEMORY */

/* Some standard cryptlib settings can be overridden by user-set 
   configuration options */

#ifdef CONFIG_PKC_ALLOCSIZE
  #if CONFIG_PKC_ALLOCSIZE < 128 || CONFIG_PKC_ALLOCSIZE > 512
	#error CONFIG_PKC_ALLOCSIZE must be between 128 and 512 (1024 to 4096 bits).
  #endif /* CONFIG_PKC_ALLOCSIZE range check */
  #undef CRYPT_MAX_PKCSIZE
  #define CRYPT_MAX_PKCSIZE			CONFIG_PKC_ALLOCSIZE
#endif /* CONFIG_PKC_ALLOCSIZE */

/****************************************************************************
*																			*
*								General Capabilities						*
*																			*
****************************************************************************/

/* General capabilities that affect further config options */

#if defined( __BEOS__ ) || defined( __CHORUS__ ) || \
	( defined( __ECOS__ ) && defined( CYGPKG_NET ) ) || \
	defined( __MVS__ ) || defined( __Nucleus__ ) || \
	defined( __PALMOS__ ) || defined( __RTEMS__ ) || \
	defined( __SYMBIAN32__ ) || defined( __TANDEM_NSK__ ) || \
	defined( __TANDEM_OSS__ ) || defined( __UNIX__ ) || \
	defined( __VxWorks__ ) || defined( _WIN32 ) || \
	defined( _WIN64 ) || defined( _WIN32_WCE )
  #define USE_TCP
#endif /* Systems with TCP/IP networking available */

/* Whether to use the RPC API or not.  This provides total isolation of
   input and output data, at the expense of some additional overhead due
   to marshalling and unmarshalling */

/* #define USE_RPCAPI */

/* Whether to use FIPS 140 ACLs or not.  Enabling this setting disables
   all plaintext key loads.  Note that this will cause several of the
   self-tests, which assume that they can load keys directly, to fail */

/* #define USE_FIPS140 */

/* Whether to build the Java/JNI interface or not */

/* #define USE_JAVA */

/* Whether to provide descriptive text messages for errors or not.
   Disabling these can reduce code size, at the expense of making error
   diagnosis reliant solely on error codes */

#ifndef CONFIG_CONSERVE_MEMORY
  #define USE_ERRMSGS
#endif /* Low-memory builds */

/* When certificates are disabled in order to reduce code size it may still
   be necessary to be able to at least send out certificates in things like 
   SSL/TLS handshakes.  The following define enables support for pseudo-
   certificate objects, objects that support just enough of the required
   certificate functionality to act as storage containers for encoded
   certificate data that can be attached to messages */

#ifdef CONFIG_USE_PSEUDOCERTIFICATES
  #define USE_PSEUDOCERTIFICATES
#endif /* CONFIG_USE_PSEUDOCERTIFICATES */

/****************************************************************************
*																			*
*									Contexts								*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_CONTEXTS

/* The umbrella define USE_PATENTED_ALGORITHMS can be used to drop all
   patented algorithms (currently no patented algorithms are left), 
   USE_DEPRECATED_ALGORITHMS can be used to drop deprecated (obsolete or 
   weak) algorithms, and USE_OBSCURE_ALGORITHMS can be used to drop little-
   used algorithms.  Technically both DES and MD5 are also deprecated but 
   they're still so widely used that it's not really possible to drop them */

#if 0
  #define USE_DEPRECATED_ALGORITHMS
#endif /* 0 */
#ifndef CONFIG_CONSERVE_MEMORY
  #define USE_PATENTED_ALGORITHMS
  #define USE_OBSCURE_ALGORITHMS
#endif /* Low-memory builds */

/* Patented algorithms */

#ifdef USE_PATENTED_ALGORITHMS
#endif /* Use of patented algorithms */

/* Obsolete and/or weak algorithms, disabled by default.  There are some 
   algorithms that are never enabled, among them KEA (which never gained any 
   real acceptance, and in any case when it was finally analysed by Kristin 
   Lauter and Anton Mityagin was found to have a variety of problems) and 
   MD2 and MD4 (which are either completely broken or obsolete/never used 
   any more) */

#ifdef USE_DEPRECATED_ALGORITHMS
  #define USE_DES
  #define USE_RC2
  #define USE_RC4
#endif /* Obsolete and/or weak algorithms */

/* Obscure algorithms */

#ifdef USE_OBSCURE_ALGORITHMS
  #define USE_ELGAMAL
  #define USE_IDEA
#endif /* Obscure algorithms */

/* Problematic algorithms that can cause issues due to memory/code size (for
   example AES-GCM uses eight times as much memory as straight AES, and 
   that's for the variant with the small lookup tables, and the ECC 
   algorithms have a sizeable code and memory footprint) or because the 
   cryptosystems are brittle and problematic (the ECC algorithms again) */

#ifdef USE_PROBLEMATIC_ALGORITHMS
  #define USE_ECDH
  #define USE_ECDSA
  #define USE_GCM
  #define USE_SHA2_EXT
#endif /* Problematic algorithms */

/* Other algorithms.  Note that DES/3DES, AES, SHA1 and SHA2 are always 
   enabled as they're either used internally by cryptlib or used by all
   cryptlib protocols/mechanisms */

#define USE_DH
#define USE_DSA
#define USE_MD5
#define USE_RSA

/* General PKC context usage */

#if defined( USE_DH ) || defined( USE_DSA ) || defined( USE_ELGAMAL ) || \
	defined( USE_RSA ) || defined( USE_ECDH ) || defined( USE_ECDSA )
  #define USE_PKC
#endif /* PKC types */

#endif /* CONFIG_NO_CONTEXTS */

/****************************************************************************
*																			*
*									Certificates							*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_CERTIFICATES

/* The certificate-processing code is so deeply intertwingled (almost all of 
   the code to manipulate certificate attributes is shared, with only a few 
   certificate type-specific routines) that it's quite difficult to separate 
   out individual sections so all that we can provide is the ability to 
   enable/disable general classes of certificate object */

#define USE_CERTIFICATES
#define USE_CERTREV			/* CRL, OCSP */
#define USE_CERTVAL			/* RTCS */
#define USE_CERTREQ			/* PKCS #10, CRMF */
#define USE_CMSATTR			/* CMS attributes */
#define USE_PKIUSER			/* pkiUser */

/* Another side-effect of the complexity of the certificate-handling code is
   that it carries around a large amount of code that's required in order to 
   support processing of bizarro attributes that no-one ever uses and whose
   sole effect is to weaken the overall code by vastly increasing its attack
   surface.  The following defines can be used to control the maximum level 
   of compliance in the certificate-handling code.  To enable use at 
   compliance level n it's necessary to have the values for level 0...n-1 
   defined as well, this makes checking in the code cleaner.  Compliance
   levels _OBLIVIOUS, _REDUCED, and _STANDARD are assumed by default, levels
   _PKIX_PARTIAL and _PKIX_FULL need to be explicitly enabled.  _PKIX_PARTIAL
   enables a few extra attributes and extra checking that are skipped in
   _STANDARD, in most cases there'll be no noticeable difference between
   _STANDARD and _PKIX_PARTIAL so unless you specifically need it you can
   disable it to save space and code complexity.  _PKIX_FULL on the other 
   hand enables a large number of additional attributes and checks, 
   including ones that enforce downright bizarre requirements set by the 
   standards.  Unless you understand the implications of this (or you need 
   to pass some sort of external compliance test) you shouldn't enable this 
   level of processing since all it does is increase the code size and 
   attack surface, complicate processing, and (if triggered) produce results
   that can be quite counterintuitive unless you really understand the
   peculiarities in the standards */

#if !defined( USE_CERTLEVEL_PKIX_FULL ) && \
	!defined( USE_CERTLEVEL_PKIX_PARTIAL ) && \
	!defined( USE_CERTLEVEL_STANDARD )
  #define USE_CERTLEVEL_PKIX_PARTIAL	/* Default level is PKIX_PARTIAL */
#endif /* PKIX compliance level */
#if defined( USE_CERTLEVEL_PKIX_FULL ) && !defined( USE_CERTLEVEL_PKIX_PARTIAL )
  /* USE_CERTLEVEL_PKIX_FULL implies USE_CERTLEVEL_PKIX_PARTIAL */
  #define USE_CERTLEVEL_PKIX_PARTIAL
#endif /* USE_CERTLEVEL_PKIX_FULL && !USE_CERTLEVEL_PKIX_PARTIAL */

/* Certificates can be given to us in base64-encoded form, so we need to 
   enable base64 decoding to deal with them */

#define USE_BASE64

/* Certificates need ASN.1 support */

#if defined( USE_CERTIFICATES ) && !defined( USE_INT_ASN1 )
  #define USE_INT_ASN1
#endif /* USE_CERTIFICATES && !USE_INT_ASN1 */

/* If we're using pseudo-certificates then we can't also use full 
   certificates */

#if defined( USE_PSEUDOCERTIFICATES ) && defined( USE_CERTIFICATES )
  #error Cant use both full certificates and pseudocertificates at the same time
#endif /* USE_PSEUDOCERTIFICATES && USE_CERTIFICATES */

/* The following are used to control handling of obscure certificate and CMS 
   attributes like qualified certificates, SigG certificates, CMS receipts, 
   security labels, and AuthentiCode, and completely obsolete certificate 
   attributes like the old Thawte and Netscape certificate extensions.  These
   are disabled by default */

#if 0
  #define USE_CERT_OBSCURE
  #define USE_CMSATTR_OBSCURE
  #define USE_CERT_OBSOLETE
#endif /* 0 */

/* Finally, we provide the ability to disable various complex and therefore
   error-prone mechanisms that aren't likely to see much use.  By default 
   these are disabled */

#if 0
  #define USE_CERT_DNSTRING
#endif /* 0 */

#if ( defined( USE_CERTIFICATES ) || defined( USE_PSEUDOCERTIFICATES ) ) && \
	!defined( USE_PKC )
  #error Use of certificates requires use of PKC algorithms to be enabled
#endif /* USE_CERTIFICATES && !USE_PKC */

#endif /* CONFIG_NO_CERTIFICATES */

/****************************************************************************
*																			*
*									Devices									*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_DEVICES

/* Device types.  PKCS #11 can also be enabled under Unix by the auto-config 
   mechanism, which sets HAS_PKCS11 if PKCS #11 support is available */

#if defined( __WIN32__ )
  #define USE_PKCS11
  #if !defined( NDEBUG )
	#define USE_HARDWARE
  #endif /* Windows debug mode only */
#endif /* __WIN32__ */
#ifdef HAS_PKCS11
  #define USE_PKCS11
#endif /* PKCS #11 under Unix autoconfig */
#if defined( USE_PKCS11 ) || defined( USE_CRYPTOAPI )
  #define USE_DEVICES
#endif /* Device types */

#endif /* CONFIG_NO_DEVICES */

/****************************************************************************
*																			*
*									Enveloping								*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_ENVELOPES

#define USE_CMS
#define USE_PGP

#if defined( USE_CMS ) || defined( USE_PGP )
  #define USE_ENVELOPES
#endif /* Enveloping types */

/* CMS envelopes require CMS data formats and compression support */

#if defined( USE_CMS ) && !defined( USE_INT_CMS )
  /* CMS enveloping requires CMS data format support */
  #define USE_INT_CMS
  #define USE_COMPRESSION
#endif /* USE_CMS */

/* PGP envelopes require Elgamal, CAST, and compression support.  Note that 
   we don't force USE_IDEA for PGP (even though the patents have expired and 
   it's freely usable) since this should now hopefully be extinct */

#if defined( USE_PGP )
  #ifndef USE_ELGAMAL
	/* OpenPGP requires ElGamal */
	#define USE_ELGAMAL
  #endif /* !USE_ELGAMAL */
  #ifndef USE_CAST
	/* Some OpenPGP implementations still (!!) default to CAST5 */
	#define USE_CAST
  #endif /* !USE_CAST */
  #ifndef USE_COMPRESSION
	/* Decoding PGP messages from other implementations requires 
	   compression support */
	#define USE_COMPRESSION
  #endif /* !USE_COMPRESSION */
#endif /* OpenPGP-specific algorithms */

/* Envelopes require PKC algorithms (they can be done with symmetric 
   algorithms only, but it's rather unikely that anyone will be doing 
   this) */

#if defined( USE_ENVELOPES ) && !defined( USE_PKC )
  #error Use of envelopes requires use of PKC algorithms to be enabled
#endif /* USE_ENVELOPES && !USE_PKC */

#endif /* CONFIG_NO_ENVELOPES */

/****************************************************************************
*																			*
*									Keysets									*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_KEYSETS

/* File keysets */

/* By uncommenting the following PKCS #12 define or enabling equivalent
   functionality in any other manner you acknowledge that you are disabling
   safety features in the code and take full responbility for any
   consequences arising from this action.  You also indemnify the cryptlib
   authors against all actions, claims, losses, costs, and expenses that
   may be suffered or incurred and that may have arisen directly or
   indirectly as a result of any use of cryptlib with this change made.  If
   you receive the code with the safety features already disabled, you must
   immediately obtain and use an original, unmodified version */
/* #define USE_PKCS12 */

#define USE_PKCS15
#define USE_PGPKEYS

#if defined( USE_PKCS15 ) && !defined( USE_INT_CMS )
  /* PKCS #15 needs CMS support for iCryptImport/ExportKey() */
  #define USE_INT_CMS
#endif /* USE_PKCS15 && !USE_INT_CMS */
#if defined( USE_PGPKEYS ) && !defined( USE_CAST )
  /* Some OpenPGP implementations still (!!) default to CAST5 */
  #define USE_CAST
#endif /* USE_PGPKEYS && !USE_CAST */
#ifdef USE_PKCS12
  /* If we use PKCS #12 then we have to enable RC2 in order to handle 
	 Microsoft's continuing use of RC2-40 */
  #define USE_RC2
#endif /* USE_PKCS12 */
#if ( defined( USE_PKCS15 ) || defined( USE_PGPKEYS ) ) && \
	!defined( USE_FILES )
  /* PKCS #15/PGP keysets need file I/O support */
  #define USE_FILES
#endif /* ( USE_PKCS15 || USE_PGPKEYS ) && !USE_FILES */

#if defined( USE_PGPKEYS ) || defined( USE_PKCS15 )
  #ifndef USE_PKC
	#error Use of PGP/PKCS #15 keysets requires use of PKC algorithms to be enabled
  #endif /* USE_PKC */
#endif /* USE_PGPKEYS || USE_PKCS15 */

/* Database keysets.  ODBC can also be enabled under Unix by the auto-config 
   mechanism, which sets HAS_ODBC if ODBC support is available */

#if defined( __WIN32__ ) && !defined( NT_DRIVER )
  #if !( defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x550 ) )
	#define USE_ODBC
  #endif /* Old Borland C++ */
#endif /* Windows */
#ifdef HAS_ODBC
  #define USE_ODBC
#endif /* ODBC under Unix autoconfig */
#if defined( USE_ODBC ) || defined( USE_DATABASE ) || \
						   defined( USE_DATABASE_PLUGIN )
  #define USE_DBMS
#endif /* RDBMS types */

/* If we're using a database keyset then we need to be able to encode binary
   identifiers as text, which requires the use of base64 encoding */

#if defined( USE_DBMS )
  #define USE_BASE64
#endif /* USE_BASE64 */

/* Network keysets.  LDAP can also be enabled under Unix by the auto-config 
   mechanism, which sets HAS_LDAP if LDAP support is available.

   Note that LDAP is disabled by default because of its very large attack 
   surface, you should only enable this if it's absolutely essential.  Your 
   security guarantee is void when you do this */

#if defined( __WIN32__ ) && \
	!( defined( NT_DRIVER ) || defined( WIN_DDK ) || \
	   defined( __BORLANDC__ ) ) && 0
  #define USE_LDAP
#endif /* Windows */
#if defined( HAS_LDAP ) && 0
  #define USE_LDAP
#endif /* LDAP under Unix autoconfig */
#ifdef USE_TCP
  #define USE_HTTP
#endif /* TCP/IP networking */

/* General keyset usage */

#if defined( USE_DBMS ) || defined( USE_HTTP ) || defined( USE_LDAP ) || \
	defined( USE_PGPKEYS ) || defined( USE_PKCS12 ) || defined( USE_PKCS15 )
  #define USE_KEYSETS
#endif /* Keyset types */

#endif /* CONFIG_NO_KEYSETS */

/****************************************************************************
*																			*
*									Sessions								*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SESSIONS

#define USE_CERTSTORE
#define USE_CMP
#define USE_RTCS
#define USE_OCSP
#define USE_SCEP
#define USE_SSH
#define USE_SSL
#define USE_TSP

#if defined( USE_CERTSTORE ) || defined( USE_CMP ) || defined( USE_RTCS ) || \
	defined( USE_OCSP ) || defined( USE_SCEP ) || defined( USE_SSH ) || \
	defined( USE_SSL ) || defined( USE_TSP )
  #define USE_SESSIONS
#endif /* Session types */

/* We can't use secure sessions if there's no networking available */

#ifndef USE_TCP
  #error Use of secure sessions requires the use of TCP/IP
#endif /* !USE_TCP */

/* Make sure that prerequisites are met for sessions that require 
   certificate components.  We can only check these if there's no specific
   session-based profile defined, because a profile for (for example) SSH
   enables sessions, but only the SSH session and not any others, which
   means that the dependency checks will produce false positives */

#if !defined( CONFIG_PROFILE_SSH ) && !defined( CONFIG_PROFILE_SSL )
  #if defined( USE_CERTSTORE ) && !defined( USE_CERTIFICATES )
	#error Use of a certificate store requires use of certificates to be enabled
  #endif /* USE_CERTSTORE && !USE_CERTIFICATES */
  #if defined( USE_CMP ) && !defined( USE_CERTREQ )
	#error Use of CMP requires use of certificate requests to be enabled
  #endif /* USE_CMP && !USE_CERTREQ */
  #if defined( USE_RTCS ) && !( defined( USE_CERTVAL ) && defined( USE_CMSATTR ) )
	/* RTCS needs CRYPT_CERTINFO_CMS_NONCE */
	#error Use of RTCS requires use of certificate validation and CMS attributes to be enabled
  #endif /* USE_RTCS && !( USE_CERTVAL && USE_CMSATTR ) */
  #if defined( USE_OCSP ) && !defined( USE_CERTREV )
	#error Use of OCSP requires use of certificate revocation to be enabled
  #endif /* USE_OCSP && !USE_CERTREV */
  #if defined( USE_SCEP ) && !( defined( USE_CERTREQ ) && defined( USE_CMSATTR ) )
	/* SCEP needs CRYPT_CERTINFO_CHALLENGEPASSWORD for PKCS #10 requests and 
	   CRYPT_CERTINFO_SCEP_xyz for CMS attributes */
	#error Use of SCEP requires use of certificate requests and CMS attributes to be enabled
  #endif /* USE_SCEP && !( USE_CERTREQ && USE_CMSATTR ) */
  #if defined( USE_SSL ) && !defined( USE_CERTIFICATES )
	#error Use of SSL requires use of certificates to be enabled
  #endif /* USE_SSL && !USE_CERTIFICATES */
  #if defined( USE_TSP ) && !defined( USE_CMSATTR )
	/* TSP requires CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID */
	#error Use of TSP requires use of CMS attributes to be enabled
  #endif /* USE_TSP && !USE_CERTIFICATES */
#endif /* !CONFIG_PROFILE_SSH && !CONFIG_PROFILE_SSL */

/* General session usage */

#if defined( USE_SESSIONS ) && !defined( USE_PKC )
  #error Use of secure sessions requires use of PKC algorithms to be enabled
#endif /* USE_SESSIONS && !USE_PKC */

/* If we're using SCEP then we need to deal with broken servers that require 
   the use of a POST disguised as a GET, for which we need to base64-encode 
   the binary data that we're sending */

#if defined( USE_SCEP )
  #define USE_BASE64
#endif /* USE_SCEP */

/* Finally, we provide the ability to disable various complex and therefore
   error-prone mechanisms that aren't likely to see much use.  By default 
   these are disabled */

#if 0
  #define USE_SSH_EXTENDED
#endif /* 0 */

#endif /* CONFIG_NO_SESSIONS */

/****************************************************************************
*																			*
*							OS Services and Resources						*
*																			*
****************************************************************************/

/* Threads */

#if defined( __AMX__  ) || defined( __ARINC653__ ) || defined( __BEOS__ ) || \
	defined( __CHORUS__ ) || defined( __CMSIS__ ) || defined( __ECOS__ ) || \
	defined( __EmbOS__ ) || defined( __FreeRTOS__ ) || defined( __ITRON__ ) || \
	defined( __MQX__ ) || defined( __Nucleus__ ) || defined( __OS2__ ) || \
	defined( __PALMOS__ ) || defined( __RTEMS__ ) || defined( __SMX__ ) || \
	defined( __ThreadX__ ) || defined( __TKernel__ ) || defined( __UCOS__ ) || \
	defined( __VDK__ ) || defined( __VxWorks__ ) || defined( __WIN32__ ) || \
	defined( __WINCE__ ) || defined( __XMK__ ) 
  #define USE_THREADS
#endif /* Non-Unix systems with threads */

#ifdef __UNIX__
  #if !( ( defined( __QNX__ ) && ( OSVERSION <= 4 ) ) || \
		 ( defined( sun ) && ( OSVERSION <= 4 ) ) || defined( __TANDEM ) )
	#define USE_THREADS
  #endif
#endif /* Unix systems with threads */

#ifdef NO_THREADS
  /* Allow thread use to be overridden by the user if required */
  #undef USE_THREADS
#endif /* NO_THREADS */

/* Widechars */

#if defined( __Android__ ) || defined( __BEOS__ ) || defined( __ECOS__ ) || \
	defined( __MSDOS32__ ) || defined( __OS2__ ) || defined( __RTEMS__ ) || \
	( ( defined( __WIN32__ ) || defined( __WINCE__ ) ) && \
	  !( defined( __BORLANDC__ ) && ( __BORLANDC__ < 0x500 ) ) ) || \
	defined( __XMK__ )
  #define USE_WIDECHARS
#endif /* Non-Unix systems with widechars */

#ifdef __UNIX__
  #if !( ( defined( __APPLE__ ) && OSVERSION < 7 ) || \
		 defined( __bsdi__ ) || defined( __OpenBSD__ ) || \
		 ( defined( __SCO_VERSION__ ) && OSVERSION < 5 ) || \
		 ( defined( sun ) && OSVERSION < 5 ) || \
		 defined( __SYMBIAN32__ ) )
	#define USE_WIDECHARS
  #endif
#endif /* Unix systems with widechars */

/* Embedded OSes are a bit of a special case in that they're usually cross-
   compiled, which means that we can't just pick up the native defines and 
   headers and go with those.  To help handle things like conditional 
   includes we define a special symbol used to indicate a non-native cross-
   compile.  Note that this is distinct from specific embedded  OSes like 
   WinCE and PalmOS which are treated as OSes in their own right, while all 
   USE_EMBEDDED_OS OSes are a single amorphous blob */

#if defined( __AMX__ ) || defined( __Android__ ) || defined( __ARINC653__ ) || \
	defined( __CHORUS__ ) || defined( __CMSIS__ ) || defined( __ECOS__ ) || \
	defined( __EmbOS__ ) || defined( __FreeRTOS__ ) || defined( __ITRON__ ) || \
	defined( __MQX__ ) || defined( __RTEMS__ ) || defined( __ThreadX__ ) || \
	defined( __TKernel__ ) || defined( __UCOS__ ) || defined( __VDK__ ) || \
	defined( __VxWorks__ ) || defined( __XMK__ )
  #define USE_EMBEDDED_OS
#endif /* Embedded OSes */

/* If it's an embededd OS there probably won't be much in the way of entropy
   sources available so we enable the use of the random seed file by 
   default */

#if defined( USE_EMBEDDED_OS ) && !defined( CONFIG_RANDSEED )
  #define CONFIG_RANDSEED
#endif /* USE_EMBEDDED_OS && !CONFIG_RANDSEED */

/* Networking.  DNS SRV is very rarely used and somewhat risky to leave 
   enabled by default because the high level of complexity of DNS packet 
   parsing combined with the primitiveness of some of the APIs (specifically
   the Unix ones) make it a bit risky to leave enabled by default, so we
   disabled it by default for attack surface reduction */

#if defined( USE_TCP ) && \
	( defined( __WINDOWS__ ) || defined( __UNIX__ ) ) && 0
  #define USE_DNSSRV
#endif /* Windows || Unix */

/* If we're on a particularly slow or fast CPU we disable or enable certain 
   processor-intensive operations.  In the absence of any easy compile-time 
   metric we define the following:

	Slow: All 16-bit CPUs 
	Fast: All 64-bit CPUs.  Windows PCs.

   This isn't perfect, but is a reasonable approximation */

#if defined( SYSTEM_16BIT )
  #define CONFIG_SLOW_CPU
#elif defined( SYSTEM_64BIT ) || defined( __WINDOWS__ )
  #define CONFIG_FAST_CPU
#endif /* Approximation of CPU speeds */

/****************************************************************************
*																			*
*						Internal/Low-level Formats							*
*																			*
****************************************************************************/

/* The CMS data format requires the use of ASN.1 */

#if defined( USE_INT_CMS ) && !defined( USE_INT_ASN1 )
  #define USE_INT_ASN1
#endif /* USE_INT_CMS && !USE_INT_ASN1 */

/****************************************************************************
*																			*
*							Application Profiles							*
*																			*
****************************************************************************/

/* The following profiles can be used to enable specific functionality for
   applications like SSL, SSH, and S/MIME */

#if defined( CONFIG_PROFILE_SMIME ) || defined( CONFIG_PROFILE_PGP ) || \
	defined( CONFIG_PROFILE_SSH ) || defined( CONFIG_PROFILE_SSL )

	/* Contexts */
	#undef USE_CAST
	#undef USE_DES
	#undef USE_ELGAMAL
	#undef USE_IDEA
	#undef USE_RC2
	#undef USE_RC4

	/* Certificates */
	#undef USE_CERTREV
	#undef USE_CERTVAL
	#undef USE_CERTREQ
	#undef USE_PKIUSER

	/* Devices */
	#undef USE_CRYPTOAPI
	#undef USE_HARDWARE
	#undef USE_PKCS11
	#undef USE_DEVICES

	/* Keysets */
	#undef USE_DBMS
	#undef USE_HTTP
	#undef USE_LDAP
	#undef USE_ODBC

	/* Misc */
	#undef USE_BASE64

	/* Sessions */
	#undef USE_CERTSTORE
	#undef USE_CMP
	#undef USE_OCSP
	#undef USE_RTCS
	#undef USE_SCEP
	#undef USE_TSP
	#undef USE_DNSSRV

#endif /* Application-specific profiles */

#ifdef CONFIG_PROFILE_SSL
	/* Contexts */
	#undef USE_DSA

	/* Certificates */
	#undef USE_CMSATTR

	/* Envelopes */
	#undef USE_ENVELOPES
	#undef USE_CMS
	#undef USE_PGP
	#undef USE_COMPRESSION

	/* Keysets */
	#undef USE_PGPKEYS

	/* Sessions */
	#undef USE_SSH

	/* Internal data formats */
	#undef USE_INT_CMS

#endif /* CONFIG_PROFILE_SSL */

#ifdef CONFIG_PROFILE_SSH
	/* Contexts */
	#undef USE_DSA

	/* Certificates */
	#undef USE_CERTIFICATES
	#undef USE_CMSATTR

	/* Envelopes */
	#undef USE_ENVELOPES
	#undef USE_CMS
	#undef USE_PGP
	#undef USE_COMPRESSION

	/* Keysets */
	#undef USE_PGPKEYS

	/* Sessions */
	#undef USE_SSL

	/* Internal data formats */
	#undef USE_INT_CMS

#endif /* CONFIG_PROFILE_SSH */

#ifdef CONFIG_PROFILE_SMIME

	/* Contexts */
	#undef USE_DH
	#undef USE_DSA

	/* Envelopes */
	#undef USE_PGP

	/* Keysets */
	#undef USE_PGPKEYS

	/* Sessions */
	#undef USE_SSH
	#undef USE_SSL
	#undef USE_TCP
	#undef USE_SESSIONS

#endif /* CONFIG_PROFILE_SSH */

#ifdef CONFIG_PROFILE_PGP

	/* Contexts */
	#undef USE_DH
	#define USE_ELGAMAL	/* Re-enable algorithms that nothing else uses */
	#define USE_CAST

	/* Certificages */
	#undef USE_CERTIFICATES

	/* Envelopes */
	#undef USE_CMS

	/* Sessions */
	#undef USE_SSH
	#undef USE_SSL
	#undef USE_TCP
	#undef USE_SESSIONS

	/* Internal data formats */
	#undef USE_INT_CMS
	#undef USE_INT_ASN1

#endif /* CONFIG_PROFILE_SSH */

/****************************************************************************
*																			*
*							Fixups for Build Dependencies					*
*																			*
****************************************************************************/

/* Some of the high-level configuration options above retroactively affect 
   the availability of low-level options.  For example USE_SESSIONS can't be 
   enabled unless USE_TCP is already defined, but then if USE_SESSIONS 
   isn't defined then there's no need for USE_TCP any more.  The following
   fixups handle situations like this */

/* If use of ASN.1 isn't enabled then we can't use the DLP signature 
   algorithms, which need ASN.1 support to write the signature format.
   Technically this isn't quite true since we could be writing the signature
   in SSL/TLS or SSH format, but the self-test uses the ASN.1 format, and we
   assume that a build that includes SSL/TLS or SSH will also include the
   ASN.1 capabilities for things like private key storage */

#ifndef USE_INT_ASN1
  #ifdef USE_DSA
	#undef USE_DSA
  #endif /* USE_DSA */
  #ifdef USE_ELGAMAL
	#undef USE_ELGAMAL
  #endif /* USE_ELGAMAL */
  #ifdef USE_ECDSA
	#undef USE_ECDSA
  #endif /* USE_ECDSA */
#endif /* USE_INT_ASN1 */

/* If sessions or HTTP keysets aren't being used then there's no need for 
   TCP networking */

#if !defined( USE_SESSIONS ) && !( defined( USE_KEYSETS ) && defined( USE_HTTP ) )
  #ifdef USE_TCP
	#undef USE_TCP
  #endif /* USE_TCP */
#endif /* USE_SESSIONS */

/****************************************************************************
*																			*
*						Defines for Testing and Custom Builds				*
*																			*
****************************************************************************/

/* Unsafe or obsolete facilities that are disabled by default, except in the 
   Win32 debug build under VC++ 6.0.  We have to be careful with the 
   preprocessor checks because the high-level feature-checking defines and
   macros are only available if osspec.h is included, which it won't be at
   this level */

#if defined( _WIN32 ) && defined( _MSC_VER ) && ( _MSC_VER == 1200 ) && \
	!defined( NDEBUG ) && !defined( NO_OBSCURE_FEATURES ) && \
	!( defined( __WINCE__ ) || defined( CONFIG_PROFILE_SMIME ) || \
	   defined( CONFIG_PROFILE_PGP ) || defined( CONFIG_PROFILE_SSH ) || \
	   defined( CONFIG_PROFILE_SSL ) )
  #define USE_CERT_DNSTRING
  #define USE_CRYPTOAPI
  #define USE_ECDH
  #define USE_ECDSA
  #define USE_GCM
  #define USE_SHA2_EXT
  #define USE_LDAP
  #define USE_OAEP
  #define USE_PKCS12
  #define USE_RC2		/* Needed for PKCS #12 */
  #ifdef USE_TCP
	#define USE_SSL3
	#define USE_SSH_EXTENDED
	#define USE_DNSSRV
  #endif /* USE_TCP */
  #define USE_PGP2
#endif /* Win32 debug build under VC++ 6.0 */

/* If we're using a static analyser then we also enable some additional 
   functionality to allow the analyser to check it */

#if ( defined( _MSC_VER ) && defined( _PREFAST_ ) ) || \
	( defined( __clang_analyzer__ ) ) || \
	( defined( USE_ANALYSER ) )
  #define USE_CERT_DNSTRING
  #define USE_DNSSRV
  #define USE_ECDH
  #define USE_ECDSA
  #define USE_GCM
  #if defined( _PREFAST_ ) || defined( __clang_analyzer__ )
	#define USE_LDAP
  #endif /* Analysers on development machines */
  #define USE_OAEP
  #define USE_PKCS12
  #define USE_RC2
  #define USE_SSH_EXTENDED
  #define USE_DNSSRV
  #define USE_PGP2
#endif /* Static analyser builds */

/* If we're using Suite B we have to explicitly enable certain algorithms, 
   including extended forms of SHA-2 */

#if defined( CONFIG_SUITEB_TESTS ) && !defined( CONFIG_SUITEB )
  #define CONFIG_SUITEB 
#endif /* CONFIG_SUITEB_TESTS && !CONFIG_SUITEB */
#if defined( CONFIG_SUITEB )
  #define USE_ECDH
  #define USE_ECDSA
  #define USE_GCM
  #define USE_SHA2_EXT
#endif /* Suite B */

/* Rather than making everything even more complex and conditional than it
   already is, it's easier to undefine the features that we don't want in
   one place rather than trying to conditionally enable them */

#if 0	/* Devices */
  #undef USE_PKCS11
  #undef USE_CRYPTOAPI
#endif /* 0 */
#if 0	/* Heavyweight keysets */
  #undef USE_HTTP
  #undef USE_LDAP
  #undef USE_ODBC
  #undef USE_DBMS
#endif /* 0 */
#if 0	/* Networking */
  #undef USE_CERTSTORE
  #undef USE_TCP
  #undef USE_CMP
  #undef USE_HTTP
  #undef USE_RTCS
  #undef USE_OCSP
  #undef USE_SCEP
  #undef USE_SSH
  #undef USE_SSL
  #undef USE_TSP
  #undef USE_SESSIONS
#endif /* 0 */
#if 0	/* Verbose error messages */
  #undef USE_ERRMSGS
#endif /* 0 */

#endif /* _CONFIG_DEFINED */
