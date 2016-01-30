/****************************************************************************
*																			*
*						cryptlib Configuration Settings  					*
*					   Copyright Peter Gutmann 1992-2012					*
*																			*
****************************************************************************/

#ifndef _CONFIG_DEFINED

#define _CONFIG_DEFINED

/* The following defines can be used to enable specific specific cryptlib 
   profiles that only enable the functionality needed for one particular
   application:

	#define CONFIG_PROFILE_SMIME
	#define CONFIG_PROFILE_SSL
	#define CONFIG_PROFILE_SSH

   The configuration is set up in the section "Application Profiles" at the
   end of this file.

   Note that VC 7.1 allows selective inheritance of defines set at the top
   level into source files within projects.  For some bizarre reason this
   defaults to 'none' so that setting USE_xxx values at the project level
   doesn't filter down to any of the source files unless it's manually
   enabled in the compiler config options.

   In addition to the above profile defines, the following can be used to
   remove entire blocks of cryptlib functionality.  Note that this sort of
   thing would normally be done by the build command (e.g. in a makefile), 
   the following is mostly intended for debugging */

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

/* General capabilities that affect further config options */

#if defined( __BEOS__ ) || defined( __CHORUS__ ) || \
	( defined( __ECOS__ ) && defined( CYGPKG_NET ) ) || \
	defined( __MVS__ ) || defined( __PALMOS__ ) || defined( __RTEMS__ ) || \
	defined( __SYMBIAN32__ ) || defined( __TANDEM_NSK__ ) || \
	defined( __TANDEM_OSS__ ) || defined( __UNIX__ ) || \
	defined( __WINDOWS__ )
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

/****************************************************************************
*																			*
*									Contexts								*
*																			*
****************************************************************************/

/* The umbrella define USE_PATENTED_ALGORITHMS can be used to drop all
   patented algorithms (currently only RC5 is still left),
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
  #define USE_RC5
#endif /* Use of patented algorithms */

/* Obsolete and/or weak algorithms, disabled by default.  There are some 
   algorithms that are never enabled, among them KEA (which never gained any 
   real acceptance, and in any case when it was finally analysed by Kristin 
   Lauter and Anton Mityagin was found to have a variety of problems) and 
   MD2, MD4, and CAST (which are either completely broken or obsolete/never
   used any more) */

#ifdef USE_DEPRECATED_ALGORITHMS
  #define USE_RC2
  #define USE_RC4
#endif /* Obsolete and/or weak algorithms */

/* Obscure algorithms */

#ifdef USE_OBSCURE_ALGORITHMS
  #define USE_ELGAMAL
  #define USE_HMAC_MD5
  #define USE_HMAC_RIPEMD160
  #define USE_IDEA
  #define USE_RIPEMD160
#endif /* Obscure algorithms */

/* Obscure algorithms and modes not supported by most other implementations.  
   Note that AES-GCM uses eight times as much memory as straight AES, and 
   that's for the variant with the small lookup tables */

#if !defined( NDEBUG ) && 0
  #define USE_ECDH
  #define USE_ECDSA
  #define USE_GCM
  #define USE_SHA2_EXT
#endif /* Win32 debug */

/* Other algorithms.  Note that DES/3DES and SHA1 are always enabled as
   they're used internally by cryptlib */

#define USE_AES
#define USE_BLOWFISH
#define USE_DH
#define USE_DSA
#define USE_MD5
#define USE_RSA
#define USE_SHA2
#define USE_HMAC_SHA2
#if defined( __UNIX__ ) && defined( _CRAY )
  /* The AES and SHA-2 reference code require a 32-bit data type, but Crays
	 only have 8-bit and 64-bit types */
  #undef USE_AES
  #undef USE_SHA2
  #undef USE_HMAC_SHA2
#endif /* Crays */
#if defined( __MSDOS__ )
  /* Remove some of the more memory-intensive or unlikely-to-be-used-under-DOS
	 algorithms */
  #undef USE_BLOWFISH
  #undef USE_DH
  #undef USE_MD5
  #undef USE_SHA2
  #undef USE_HMAC_SHA2

  /* Remove further algorithms to save space */
  #undef USE_DSA
#endif /* DOS */

/* General PKC context usage */

#if defined( USE_DH ) || defined( USE_DSA ) || defined( USE_ELGAMAL ) || \
	defined( USE_RSA ) || defined( USE_ECDH ) || defined( USE_ECDSA )
  #define USE_PKC
#endif /* PKC types */

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

#if defined( USE_CERTIFICATES ) && !defined( USE_PKC )
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
  #ifndef __BORLANDC__
	#define USE_PKCS11
  #endif /* Borland C can't handle PKCS #11 headers */
  #if !defined( NDEBUG )
	#define USE_HARDWARE
  #endif /* Windows debug mode only */
#endif /* __WIN32__ */
#ifdef HAS_PKCS11
  #define USE_PKCS11
#endif /* PKCS #11 under Unix autoconfig */

/* General device usage */

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

/* CMS envelopes */

#define USE_CMS
#if !defined( __MSDOS__ ) && !defined( __WIN16__ )
  #define USE_COMPRESSION
#endif /* __MSDOS__ || __WIN16__ */

/* PGP envelopes.  Note that we don't force USE_IDEA for PGP (even though 
   the patents have expired and it's freely usable) since this should now 
   hopefully be extinct */

#define USE_PGP
#if defined( USE_PGP )
  #ifndef USE_ELGAMAL
	#define USE_ELGAMAL
  #endif /* OpenPGP requires ElGamal */
  #ifndef USE_CAST
	#define USE_CAST
  #endif /* Some OpenPGP implementations still (!!) default to CAST5 */
#endif /* OpenPGP-specific algorithms */

/* General envelope usage */

#if defined( USE_CMS ) || defined( USE_PGP )
  #define USE_ENVELOPES
#endif /* Enveloping types */

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

/* Network keysets.  LDAP can also be enabled under Unix by the auto-config 
   mechanism, which sets HAS_LDAP if LDAP support is available.

   Note that LDAP is disabled by default because of its very large attack 
   surface, you should only enable this if it's absolutely essential, and 
   your security guarantee is void when using it */

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

/* File keysets */

/* By uncommenting the following PKCS #12 define or enabling equivalent
   functionality in any other manner you acknowledge that you are disabling
   safety features in the code and take full responbility for any
   consequences arising from this action.  You also indemnify the cryptlib
   authors against all actions, claims, losses, costs, and expenses that
   may be suffered or incurred and that may have arisen directly or
   indirectly as a result of any use of cryptlib with this change made.  If
   you receive the code with the safety features already disabled, you must
   obtain an original, unmodified version */
/* #define USE_PKCS12 */
#ifdef USE_PKCS12
  /* If we use PKCS #12 then we have to enable RC2 in order to handle 
	 Microsoft's continuing use of RC2-40 */
  #define USE_RC2
#endif /* USE_PKCS12 */

#define USE_PGPKEYS
#define USE_PKCS15
#if defined( USE_PGPKEYS ) || defined( USE_PKCS15 )
  #ifndef USE_PKC
	#error Use of PGP/PKCS #15 keysets requires use of PKC algorithms to be enabled
  #endif /* USE_PKC */
#endif /* USE_PGPKEYS || USE_PKCS15 */
#if defined( USE_PGPKEYS ) && !defined( USE_CAST )
  #define USE_CAST
#endif /* Some OpenPGP implementations still (!!) default to CAST5 */

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

/* SSHv1 is explicitly disabled (or at least not enabled), you should only
   enable this if there's a very good reason to use it since this code is 
   very old, unmaintained, and hasn't been subject to ongoing security
   audits.  Enabling it here will also produce a double-check warning in 
   ssh1.c that needs to be turned off to allow the code to build */

#ifdef USE_TCP
  #define USE_CERTSTORE
  #define USE_CMP
  #define USE_RTCS
  #define USE_OCSP
  #define USE_SCEP
  #define USE_SSH
  #define USE_SSL
  #define USE_TSP
#endif /* USE_TCP */

/* Make sure that prerequisites are met for sessions that require 
   certificate components */

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

/* General session usage */

#if defined( USE_CMP ) || defined( USE_RTCS ) || defined( USE_OCSP ) || \
	defined( USE_SCEP ) || defined( USE_SSH1 ) || defined( USE_SSH ) || \
	defined( USE_SSL ) || defined( USE_TSP )
  #define USE_SESSIONS
#endif /* Session types */

#if defined( USE_SESSIONS ) && !defined( USE_PKC )
  #error Use of secure sessions requires use of PKC algorithms to be enabled
#endif /* USE_SESSIONS && !USE_PKC */

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

#if defined( __AMX__  ) || defined( __BEOS__ ) || defined( __CHORUS__ ) || \
	defined( __ECOS__ ) || defined( __EmbOS__ ) || defined( __FreeRTOS__ ) || \
	defined( __ITRON__ ) || defined( __MQX__ ) || defined( __Nucleus__ ) || \
	defined( __OS2__ ) || defined( __PALMOS__ ) || defined( __RTEMS__ ) || \
	defined( __ThreadX__ ) || defined( __TKernel__ ) || defined( __UCOS__ ) || \
	defined( __VDK__ ) || defined( __VXWORKS__ ) || defined( __WIN32__ ) || \
	defined( __WINCE__ ) || defined( __XMK__ ) || defined( __VXWORKS__ )
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

#if defined( __BEOS__ ) || defined( __ECOS__ ) || defined( __MSDOS32__ ) || \
	defined( __OS2__ ) || defined( __RTEMS__ ) || \
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

#if defined( __AMX__ ) || defined( __CHORUS__ ) || defined( __ECOS__ ) || \
	defined( __EmbOS__ ) || defined( __FreeRTOS__ ) || defined( __ITRON__ ) || \
	defined( __MQX__ ) || defined( __RTEMS__ ) || defined( __ThreadX__ ) || \
	defined( __TKernel__ ) || defined( __UCOS__ ) || defined( __VDK__ ) || \
	defined( __VXWORKS__ ) || defined( __XMK__ )
  #define USE_EMBEDDED_OS
#endif /* Embedded OSes */

/* If it's an embededd OS there probably won't be much in the way of entropy
   sources available so we enable the use of the random seed file by 
   default */

#ifdef USE_EMBEDDED_OS
  #define CONFIG_RANDSEED
#endif /* USE_EMBEDDED_OS */

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
   metric we define all 16-bit CPUs to be slow and all 64-bit CPUs to be 
   fast, which is a reasonable approximation */

#if defined( SYSTEM_16BIT )
  #define CONFIG_SLOW_CPU
#elif defined( SYSTEM_64BIT )
  #define CONFIG_FAST_CPU
#endif /* Approximation of CPU speeds */

/****************************************************************************
*																			*
*							Application Profiles							*
*																			*
****************************************************************************/

/* The following profiles can be used to enable specific functionality for
   applications like SSL, SSH, and S/MIME */

#if defined( CONFIG_PROFILE_SMIME ) || defined( CONFIG_PROFILE_SSH ) || \
	defined( CONFIG_PROFILE_SSL )

	/* Contexts */
	#undef USE_BLOWFISH
	#undef USE_ELGAMAL
	#undef USE_HMAC_MD5
	#undef USE_HMAC_RIPEMD160
	#undef USE_IDEA
	#undef USE_RC2
	#undef USE_RC4
	#undef USE_RC5
	#undef USE_RIPEMD160

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

	/* Sessions */
	#undef USE_CERTSTORE
	#undef USE_CMP
	#undef USE_OCSP
	#undef USE_RTCS
	#undef USE_SCEP
	#undef USE_SSH1
	#undef USE_TSP
	#undef USE_DNSSRV

#endif /* Application-specific profiles */

#ifdef CONFIG_PROFILE_SSL
	/* Contexts */
	#undef USE_DSA

	/* Certificates */
	#undef USE_CMSATTR

	/* Envelopes */
	#undef USE_PGP
	#undef USE_COMPRESSION

	/* Keysets */
	#undef USE_PGPKEYS

	/* Sessions */
	#undef USE_SSH

#endif /* CONFIG_PROFILE_SSL */

#ifdef CONFIG_PROFILE_SSH
	/* Contexts */
	#undef USE_DSA

	/* Certificates */
	#undef USE_CERTIFICATES
	#undef USE_CMSATTR

	/* Envelopes */
	#undef USE_PGP
	#undef USE_COMPRESSION

	/* Keysets */
	#undef USE_PGPKEYS

	/* Sessions */
	#undef USE_SSL

#endif /* CONFIG_PROFILE_SSH */

#ifdef CONFIG_PROFILE_SMIME

	/* Contexts */
	#undef USE_DH
	#undef USE_DSA
	#undef USE_HMAC_SHA2
	#undef USE_SHA2

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
	   defined( CONFIG_PROFILE_SSH ) || defined( CONFIG_PROFILE_SSL ) )
  #define USE_CERT_DNSTRING
  #define USE_CRYPTOAPI
  #define USE_ECDH
  #define USE_ECDSA
  #define USE_GCM
  #define USE_LDAP
  #define USE_OAEP
  #define USE_PKCS12
  #define USE_RC2		/* Needed for PKCS #12 */
  #ifdef USE_TCP
	#define USE_SSH_EXTENDED
	#define USE_DNSSRV
  #endif /* USE_TCP */
#endif /* Win32 debug build under VC++ 6.0 */

/* If we're using a static analyser then we also enable some additional 
   functionality to allow the analyser to check it */

#if ( defined( _MSC_VER ) && defined( _PREFAST_ ) ) || \
	( defined( __clang_analyzer__ ) )
  #define USE_CERT_DNSTRING
  #define USE_DNSSRV
  #define USE_ECDH
  #define USE_ECDSA
  #define USE_GCM
  #define USE_LDAP
  #define USE_OAEP
  #define USE_PKCS12
  #define USE_SSH_EXTENDED
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
  #undef USE_SSH1
  #undef USE_SSH
  #undef USE_SSL
  #undef USE_TSP
  #undef USE_SESSIONS
#endif /* 0 */
#if 0	/* Verbose error messages */
  #undef USE_ERRMSGS
#endif /* 0 */

#endif /* _CONFIG_DEFINED */
