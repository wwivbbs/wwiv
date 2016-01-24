/****************************************************************************
*																			*
*					cryptlib SSL v3/TLS Cipher Suites						*
*					Copyright Peter Gutmann 1998-2010						*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "session.h"
  #include "ssl.h"
#else
  #include "crypt.h"
  #include "session/session.h"
  #include "session/ssl.h"
#endif /* Compiler-specific includes */

#ifdef USE_SSL

/****************************************************************************
*																			*
*							Cipher Suite Definitions						*
*																			*
****************************************************************************/

/* The monster list of cryptlib's SSL/TLS cipher suites (the full list is 
   even longer than this).  There are a pile of DH cipher suites, in 
   practice only DHE is used, DH requires the use of X9.42 DH certificates 
   (there aren't any) and DH_anon uses unauthenticated DH which implementers 
   seem to have an objection to even though it's not much different in 
   effect from the way RSA cipher suites are used in practice.

   To keep things simple for the caller we only allow RSA auth for DH key
   agreement and not DSA, since the former also automatically works for the
   far more common RSA key exchange that's usually used for key setup.
   Similarly we only allow ECDSA for ECDH, since anyone who wants to make 
   the ECC fashion statement isn't going to then fall back to RSA for the 
   server authentication.  In both cases the actions for the unused suites
   are present in the table but commented out.

   We prefer AES-128 to AES-256 since -256 has a weaker key schedule than
   -128, so if anyone's going to attack it they'll go for the key schedule
   rather than the (mostly irrelevant) -128 vs. -256.

   Some buggy older versions of IIS that only support crippled crypto drop 
   the connection when they see a client hello advertising strong crypto 
   rather than sending an alert as they should.  To work around this we 
   advertise a dummy cipher suite SSL_RSA_EXPORT_WITH_RC4_40_MD5 as a canary 
   to force IIS to send back a response that we can then turn into an error 
   message.  The need to do this is somewhat unfortunate since it will 
   appear to an observer that cryptlib will use crippled crypto (in fact it 
   won't even load such a key) but there's no other way to detect the buggy 
   IIS apart from completely restarting the session activation at the 
   session level with crippled-crypto advertised in the restarted session.
   
   The number of suites and different configuration options are sufficiently
   complex that we can't use a fixed table for them but have to dynamically
   build them up at runtime from the following sub-tables */

#define MAX_CIPHERSUITE_TBLSIZE		64

static const CIPHERSUITE_INFO cipherSuiteDH[] = {
	/* 3DES with DH */
	{ TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
	  DESCRIPTION( "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_RSA, CRYPT_ALGO_3DES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 24, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_DH },
/*	{ TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
	  DESCRIPTION( "TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_DSA, CRYPT_ALGO_3DES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 24, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_DH }, */

	/* AES with DH */
	{ TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
	  DESCRIPTION( "TLS_DHE_RSA_WITH_AES_128_CBC_SHA" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 16, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_DH },
	{ TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
	  DESCRIPTION( "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 16, SHA2MAC_SIZE, 
	  CIPHERSUITE_FLAG_DH | CIPHERSUITE_FLAG_TLS12 },
/*	{ TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
	  DESCRIPTION( "TLS_DHE_DSS_WITH_AES_128_CBC_SHA" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_DSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 16, SHA1MAC_SIZE, CIPHERSUITE_FLAG_DH }, 
	{ TLS_DHE_DSS_WITH_AES_128_CBC_SHA256, 
	  DESCRIPTION( "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_DSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 16, SHA2MAC_SIZE, 
	  CIPHERSUITE_FLAG_DH | CIPHERSUITE_FLAG_TLS12 }, */
	{ TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
	  DESCRIPTION( "TLS_DHE_RSA_WITH_AES_256_CBC_SHA" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 32, SHA1MAC_SIZE, CIPHERSUITE_FLAG_DH },
	{ TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
	  DESCRIPTION( "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 32, SHA2MAC_SIZE, 
	  CIPHERSUITE_FLAG_DH | CIPHERSUITE_FLAG_TLS12 },
/*	{ TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
	  DESCRIPTION( "TLS_DHE_DSS_WITH_AES_256_CBC_SHA" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_DSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 32, SHA1MAC_SIZE, CIPHERSUITE_FLAG_DH }, 
	{ TLS_DHE_DSS_WITH_AES_256_CBC_SHA256,
	  DESCRIPTION( "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_DSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 32, SHA2MAC_SIZE, 
	  CIPHERSUITE_FLAG_DH | CIPHERSUITE_FLAG_TLS12 }, */

	/* End-of-list marker */
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE },
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE }
	};

static const CIPHERSUITE_INFO cipherSuiteECC[] = {
	/* ECDH with ECDSA */
	{ TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, 
	  DESCRIPTION( "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_ECDSA, CRYPT_ALGO_3DES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 24, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_ECC },
	{ TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, 
	  DESCRIPTION( "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_ECDSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 32, SHA2MAC_SIZE, 
	  CIPHERSUITE_FLAG_ECC | CIPHERSUITE_FLAG_TLS12 },
/*	{ TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, 
	  DESCRIPTION( "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 32, SHA2MAC_SIZE, 
	  CIPHERSUITE_FLAG_ECC | CIPHERSUITE_FLAG_TLS12 }, */
	{ TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, 
	  DESCRIPTION( "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_ECDSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 16, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_ECC },
	{ TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, 
	  DESCRIPTION( "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_ECDSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 32, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_ECC },
	{ TLS_ECDHE_RSA_WITH_RC4_128_SHA, 
	  DESCRIPTION( "TLS_ECDHE_RSA_WITH_RC4_128_SHA" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_RSA, CRYPT_ALGO_RC4,
	  CRYPT_ALGO_HMAC_SHA1, 0, 16, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_ECC },

	/* ECDH with RSA */
/*	{ TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, 
	  DESCRIPTION( "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_RSA, CRYPT_ALGO_3DES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 24, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_ECC },
	{ TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, 
	  DESCRIPTION( "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 16, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_ECC }, 
	{ TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, 
	  DESCRIPTION( "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 32, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_ECC }, */

	/* End-of-list marker */
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE },
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE }
	};

static const CIPHERSUITE_INFO cipherSuiteGCM[] = {
	/* ECDH with ECDSA and AES-GCM */
	{ TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 
	  DESCRIPTION( "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_ECDSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 16, GCMICV_SIZE, 
	  CIPHERSUITE_FLAG_ECC | CIPHERSUITE_FLAG_GCM | CIPHERSUITE_FLAG_TLS12 },
/*	{ TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 
	  DESCRIPTION( "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256" )
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 16, GCMICV_SIZE, 
	  CIPHERSUITE_FLAG_ECC | CIPHERSUITE_FLAG_GCM | CIPHERSUITE_FLAG_TLS12 }, */

	/* AES-GCM with DH */
	{ TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
	  DESCRIPTION( "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 16, GCMICV_SIZE, 
	  CIPHERSUITE_FLAG_DH | CIPHERSUITE_FLAG_GCM | CIPHERSUITE_FLAG_TLS12 },
/*	{ TLS_DHE_DSS_WITH_AES_128_GCM_SHA256,
	  DESCRIPTION( "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_DSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 16, GCMICV_SIZE, 
	  CIPHERSUITE_FLAG_DH | CIPHERSUITE_FLAG_GCM | CIPHERSUITE_FLAG_TLS12 }, */

	/* AES-GCM with RSA */
	{ TLS_RSA_WITH_AES_128_GCM_SHA256,
	  DESCRIPTION( "TLS_RSA_WITH_AES_128_GCM_SHA256" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 16, GCMICV_SIZE, 
	  CIPHERSUITE_FLAG_GCM | CIPHERSUITE_FLAG_TLS12 },

	/* End-of-list marker */
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE },
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE }
	};

static const CIPHERSUITE_INFO cipherSuiteMisc[] = {
	/* RC4 + RSA */
	{ SSL_RSA_WITH_RC4_128_SHA,
	  DESCRIPTION( "SSL_RSA_WITH_RC4_128_SHA" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_RC4,
	  CRYPT_ALGO_HMAC_SHA1, 0, 16, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_NONE },
	{ SSL_RSA_WITH_RC4_128_MD5,
	  DESCRIPTION( "SSL_RSA_WITH_RC4_128_MD5" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_RC4,
	  CRYPT_ALGO_HMAC_MD5, 0, 16, MD5MAC_SIZE, 
	  CIPHERSUITE_FLAG_NONE },

	/* DES + RSA */
	{ SSL_RSA_WITH_DES_CBC_SHA,
	  DESCRIPTION( "SSL_RSA_WITH_DES_CBC_SHA" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_DES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 8, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_NONE },
	{ TLS_DHE_RSA_WITH_DES_CBC_SHA,
	  DESCRIPTION( "TLS_DHE_RSA_WITH_DES_CBC_SHA" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_RSA, CRYPT_ALGO_DES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 8, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_NONE },
/*	{ TLS_DHE_DSS_WITH_DES_CBC_SHA,
	  DESCRIPTION( "TLS_DHE_DSS_WITH_DES_CBC_SHA" )
	  CRYPT_ALGO_DH, CRYPT_ALGO_DSA, CRYPT_ALGO_DES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 8, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_NONE }, */

	/* End-of-list marker */
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE },
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE }
	};

static const CIPHERSUITE_INFO cipherSuitePSK[] = {
	/* PSK */
	{ TLS_PSK_WITH_3DES_EDE_CBC_SHA,
	  DESCRIPTION( "TLS_PSK_WITH_3DES_EDE_CBC_SHA" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_3DES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 24, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_PSK },
	{ TLS_PSK_WITH_AES_128_CBC_SHA,
	  DESCRIPTION( "TLS_PSK_WITH_AES_128_CBC_SHA" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 16, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_PSK },
	{ TLS_PSK_WITH_AES_256_CBC_SHA,
	  DESCRIPTION( "TLS_PSK_WITH_AES_256_CBC_SHA" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 32, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_PSK },
	{ TLS_PSK_WITH_RC4_128_SHA,
	  DESCRIPTION( "TLS_PSK_WITH_RC4_128_SHA" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_RC4,
	  CRYPT_ALGO_HMAC_SHA1, 0, 16, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_PSK },

	/* End-of-list marker */
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE },
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE }
	};

static const CIPHERSUITE_INFO cipherSuiteRSA[] = {
	/* 3DES with RSA */
	{ SSL_RSA_WITH_3DES_EDE_CBC_SHA,
	  DESCRIPTION( "SSL_RSA_WITH_3DES_EDE_CBC_SHA" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_3DES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 24, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_NONE },

	/* AES with RSA */
	{ TLS_RSA_WITH_AES_128_CBC_SHA,
	  DESCRIPTION( "TLS_RSA_WITH_AES_128_CBC_SHA" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 16, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_NONE },
	{ TLS_RSA_WITH_AES_256_CBC_SHA,
	  DESCRIPTION( "TLS_RSA_WITH_AES_256_CBC_SHA" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA1, 0, 32, SHA1MAC_SIZE, 
	  CIPHERSUITE_FLAG_NONE },
	{ TLS_RSA_WITH_AES_128_CBC_SHA256,
	  DESCRIPTION( "TLS_RSA_WITH_AES_128_CBC_SHA256" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 16, SHA2MAC_SIZE, 
	  CIPHERSUITE_FLAG_TLS12 },
	{ TLS_RSA_WITH_AES_256_CBC_SHA256,
	  DESCRIPTION( "TLS_RSA_WITH_AES_256_CBC_SHA256" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_AES,
	  CRYPT_ALGO_HMAC_SHA2, 0, 32, SHA2MAC_SIZE, 
	  CIPHERSUITE_FLAG_TLS12 },

	/* End-of-list marker */
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE },
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE }
	};

static const CIPHERSUITE_INFO cipherSuiteCanary[] = {
	/* Canary used to detect the use of weak ciphers by the peer (where the
	   peer in this case would be "older versions of IIS") */
	{ SSL_RSA_EXPORT_WITH_RC4_40_MD5, 
	  DESCRIPTION( "SSL_RSA_EXPORT_WITH_RC4_40_MD5" )
	  CRYPT_ALGO_RSA, CRYPT_ALGO_RSA, CRYPT_ALGO_RC4,
	  CRYPT_ALGO_HMAC_MD5, 0, 16, MD5MAC_SIZE, 
	  CIPHERSUITE_FLAG_NONE },

	/* End-of-list marker */
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE },
	{ SSL_NULL_WITH_NULL,
	  DESCRIPTION( "End-of-list marker" )
	  CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	  CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE }
	};

/* Tables defining the arrangement of the above sets of suites into a 
   single monster list.  The order of adding suites is as follows:

	[ Optional special-case suites, for testing or custom configurations ].
	PSK suites, since they provide proper mutual authentication. 
	ECC suites if PREFER_ECC is defined, since anyone wanting to make this 
			particular fashion statement probably wants to actually use the 
			suites rather than having them ignored as the 25th-ranked 
			option.  (GCM with ECC is a variant of this, this is even more 
			of a fashion statement and really only makes sense with ECC).
	DH suites unless PREFER_RSA_TO_DH is defined, since these provide
			PFS, are secure against factoring attacks, and in many cases
			are the same as RSA when employed in anon-DH-equivalent mode.
	RSA suites with strong ciphers.
	Misc RSA suites with also-ran ciphers.
	Canary used to detect broken older version of MSIE */

typedef struct {
	const CIPHERSUITE_INFO *cipherSuites;
	const int cipherSuiteCount;
	} CIPHERSUITES_LIST;

static const CIPHERSUITES_LIST cipherSuitesList[] = {
	{ cipherSuitePSK, FAILSAFE_ARRAYSIZE( cipherSuitePSK, CIPHERSUITE_INFO ) },
#if defined( PREFER_ECC ) && defined( USE_ECDH ) && defined( USE_ECDSA )
  #ifdef USE_GCM
	{ cipherSuiteGCM, FAILSAFE_ARRAYSIZE( cipherSuiteGCM, CIPHERSUITE_INFO ) },
  #endif /* USE_GCM */
	{ cipherSuiteECC, FAILSAFE_ARRAYSIZE( cipherSuiteECC, CIPHERSUITE_INFO ) },
#endif /* PREFER_ECC && USE_ECDH && USE_ECDSA */
#ifdef PREFER_RSA_TO_DH
	{ cipherSuiteRSA, FAILSAFE_ARRAYSIZE( cipherSuiteRSA, CIPHERSUITE_INFO ) },
	{ cipherSuiteDH, FAILSAFE_ARRAYSIZE( cipherSuiteDH, CIPHERSUITE_INFO ) },
#else
	{ cipherSuiteDH, FAILSAFE_ARRAYSIZE( cipherSuiteDH, CIPHERSUITE_INFO ) },
	{ cipherSuiteRSA, FAILSAFE_ARRAYSIZE( cipherSuiteRSA, CIPHERSUITE_INFO ) },
#endif /* PREFER_RSA_TO_DH */
#if !defined( PREFER_ECC ) && defined( USE_ECDH ) && defined( USE_ECDSA )
  #ifdef USE_GCM
	{ cipherSuiteGCM, FAILSAFE_ARRAYSIZE( cipherSuiteGCM, CIPHERSUITE_INFO ) },
  #endif /* USE_GCM */
	{ cipherSuiteECC, FAILSAFE_ARRAYSIZE( cipherSuiteECC, CIPHERSUITE_INFO ) },
#endif /* !PREFER_ECC && USE_ECDH && USE_ECDSA */
	{ cipherSuiteMisc, FAILSAFE_ARRAYSIZE( cipherSuiteMisc, CIPHERSUITE_INFO ) },
	{ cipherSuiteCanary, FAILSAFE_ARRAYSIZE( cipherSuiteCanary, CIPHERSUITE_INFO ) },
	{ NULL, 0 }, { NULL, 0 }
	};

/****************************************************************************
*																			*
*						Cipher Suite Definitions for Suite B				*
*																			*
****************************************************************************/

/* If we're running in a Suite B configuration then we don't bother with any
   of the standard cipher suites but only provide Suite B suites */

#if defined( CONFIG_SUITEB )

#if defined( _MSC_VER )
  #pragma message( "  Building with custom suite: Suite B" )
  #if defined( CONFIG_SUITEB_TESTS )
	#pragma message( "  Building with custom suite: Suite B test suites" )
  #endif /* Suite B special test suites */
#endif /* VC++ */

/* 256-bit Suite B suites */

static const CIPHERSUITE_INFO suiteBP384GCM = { 
	TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, 
	DESCRIPTION( "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384" )
	CRYPT_ALGO_ECDH, CRYPT_ALGO_ECDSA, CRYPT_ALGO_AES,
	CRYPT_ALGO_HMAC_SHA2, bitsToBytes( 384 ), 32, GCMICV_SIZE, 
	CIPHERSUITE_FLAG_ECC | CIPHERSUITE_FLAG_GCM | CIPHERSUITE_FLAG_TLS12 
	};

/* 128-bit Suite B suites */

static const CIPHERSUITE_INFO suiteBP256GCM = { 
	TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 
	DESCRIPTION( "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256" )
	CRYPT_ALGO_ECDH, CRYPT_ALGO_ECDSA, CRYPT_ALGO_AES,
	CRYPT_ALGO_HMAC_SHA2, 0, 16, GCMICV_SIZE, 
	CIPHERSUITE_FLAG_ECC | CIPHERSUITE_FLAG_GCM | CIPHERSUITE_FLAG_TLS12 
	};

/* End-of-list marker */

static const CIPHERSUITE_INFO suiteBEOL = { 
	SSL_NULL_WITH_NULL,
	DESCRIPTION( "End-of-list marker" )
	CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
	CRYPT_ALGO_NONE, 0, 0, 0, CIPHERSUITE_FLAG_NONE 
	};

/* Since the only suites that we're enabling are the Suite B ones, we 
   override the default getCipherSuiteInfo() with our own one */

CHECK_RETVAL \
int getSuiteBCipherSuiteInfo( OUT const CIPHERSUITE_INFO ***cipherSuiteInfoPtrPtrPtr,
							  OUT_INT_Z int *noSuiteEntries,
							  const BOOLEAN isServer,
							  IN_FLAGS_Z( SSL ) const int suiteBinfo )
	{
	static const CIPHERSUITE_INFO *cipherSuite192InfoTbl[] = \
		{ &suiteBP384GCM, &suiteBEOL, &suiteBEOL };
	static const CIPHERSUITE_INFO *cipherSuite128InfoTbl[] = \
		{ &suiteBP256GCM, &suiteBP384GCM, &suiteBEOL, &suiteBEOL };
	BOOLEAN is128bitLevel = ( ( suiteBinfo & SSL_PFLAG_SUITEB ) == \
									SSL_PFLAG_SUITEB_128 ) ? TRUE : FALSE;

	assert( isReadPtr( cipherSuiteInfoPtrPtrPtr, \
					   sizeof( CIPHERSUITE_INFO ** ) ) );
	assert( isWritePtr( noSuiteEntries, sizeof( int ) ) );

	REQUIRES( suiteBinfo >= SSL_PFLAG_NONE && suiteBinfo < SSL_PFLAG_MAX );

	/* Depending on the security level that we're configured for we either 
	   prefer the 128-bit suites or the 192-bit suites */
	if( is128bitLevel )
		{
		*cipherSuiteInfoPtrPtrPtr = ( const CIPHERSUITE_INFO ** ) \
									cipherSuite128InfoTbl;	/* For gcc */
		*noSuiteEntries = FAILSAFE_ARRAYSIZE( cipherSuite128InfoTbl, \
											  CIPHERSUITE_INFO * );
		}
	else
		{
		*cipherSuiteInfoPtrPtrPtr = ( const CIPHERSUITE_INFO ** ) \
									cipherSuite192InfoTbl;	/* For gcc */
		*noSuiteEntries = FAILSAFE_ARRAYSIZE( cipherSuite192InfoTbl, \
											  CIPHERSUITE_INFO * );
		}
	return( CRYPT_OK );
	}

/* Remap the usual getCipherSuiteInfo() into an alternative name that
   doesn't clash with the Suite B replacement */

#undef getCipherSuiteInfo
#define	getCipherSuiteInfo	getCipherSuiteInfoOriginal

#endif /* CONFIG_SUITEB */

/****************************************************************************
*																			*
*							Cipher Suite Functions							*
*																			*
****************************************************************************/

/* Build the single unified list of ciphers suites in preferred-algorithm
   order */

CHECK_RETVAL \
static int addCipherSuiteInfo( INOUT CIPHERSUITE_INFO **cipherSuiteTbl, 
							   IN_RANGE( 0, MAX_CIPHERSUITE_TBLSIZE ) \
									const int cipherSuiteTblCount,
							   OUT_RANGE( 0, MAX_CIPHERSUITE_TBLSIZE ) \
									int *newCipherSuiteTblCount, 
							   const CIPHERSUITE_INFO *cipherSuites,
							   IN_RANGE( 0, MAX_CIPHERSUITE_TBLSIZE / 2 ) \
									const int cipherSuitesCount )
	{
	int srcIndex, destIndex;

	assert( isReadPtr( cipherSuiteTbl, \
					   sizeof( CIPHERSUITE_INFO * ) * MAX_CIPHERSUITE_TBLSIZE ) );
	assert( isWritePtr( newCipherSuiteTblCount, sizeof( int ) ) );
	assert( isReadPtr( cipherSuites,
					   sizeof( CIPHERSUITE_INFO * ) * 2 ) );

	REQUIRES( cipherSuiteTblCount >= 0 && \
			  cipherSuiteTblCount < MAX_CIPHERSUITE_TBLSIZE );
	REQUIRES( cipherSuitesCount >= 0 && \
			  cipherSuitesCount < MAX_CIPHERSUITE_TBLSIZE / 2 );

	/* Clear return value.  Unlike standard practice this doesn't set it to
	   zero but to the existing count, making the call a no-op */
	*newCipherSuiteTblCount = cipherSuiteTblCount;

	/* Add any new suites to the existing table */
	for( srcIndex = 0, destIndex = cipherSuiteTblCount; 
		 cipherSuites[ srcIndex ].cipherSuite != SSL_NULL_WITH_NULL && \
			srcIndex < cipherSuitesCount && destIndex < MAX_CIPHERSUITE_TBLSIZE;
		 srcIndex++, destIndex++ )
		{
		cipherSuiteTbl[ destIndex ] = ( CIPHERSUITE_INFO * ) \
									  &cipherSuites[ srcIndex ];
		}
	ENSURES( srcIndex < cipherSuitesCount );
	ENSURES( destIndex < MAX_CIPHERSUITE_TBLSIZE );

	*newCipherSuiteTblCount = destIndex;

	return( CRYPT_OK );
	}

CHECK_RETVAL \
int getCipherSuiteInfo( OUT const CIPHERSUITE_INFO ***cipherSuiteInfoPtrPtrPtr,
						OUT_INT_Z int *noSuiteEntries,
						STDC_UNUSED const BOOLEAN isServer )
	{
	static CIPHERSUITE_INFO *cipherSuiteInfoTbl[ MAX_CIPHERSUITE_TBLSIZE + 8 ];
	static BOOLEAN cipherSuitInfoInited = FALSE;
	static int cipherSuiteCount = 0;
	int status;

	assert( isReadPtr( cipherSuiteInfoPtrPtrPtr, \
					   sizeof( CIPHERSUITE_INFO ** ) ) );
	assert( isWritePtr( noSuiteEntries, sizeof( int ) ) );

	/* Dynamically set up the monster table of cipher suites.  Note that 
	   this isn't thread-safe, but since it performs the setup in a
	   completely deterministic manner it doesn't matter if the extremely
	   unlikely situation of two threads initialising the array at the same
	   time occurs, since they're initialising it identically */
	if( !cipherSuitInfoInited )
		{
		static const CIPHERSUITE_INFO endOfList = {
			SSL_NULL_WITH_NULL,
			DESCRIPTION( "End-of-list marker" )
			CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 
			CRYPT_ALGO_NONE, 0, 0, CIPHERSUITE_FLAG_NONE };
		int i;

		/* Build the unified list of cipher suites */
		for( i = 0; 
			 cipherSuitesList[ i ].cipherSuites != NULL && \
				i < FAILSAFE_ARRAYSIZE( cipherSuitesList, CIPHERSUITES_LIST );
			 i++ )
			{
			status = addCipherSuiteInfo( cipherSuiteInfoTbl, cipherSuiteCount,
						&cipherSuiteCount, cipherSuitesList[ i ].cipherSuites,
						cipherSuitesList[ i ].cipherSuiteCount );
			if( cryptStatusError( status ) )
				return( status );
			}
		ENSURES( i < FAILSAFE_ARRAYSIZE( cipherSuitesList, CIPHERSUITES_LIST ) );

		/* Add the end-of-list marker suites.  Note that we don't increment 
		   the suite count when the second one is added to match the 
		   behaviour of FAILSAFE_ARRAYSIZE() */
		REQUIRES( cipherSuiteCount + 2 < MAX_CIPHERSUITE_TBLSIZE );
		cipherSuiteInfoTbl[ cipherSuiteCount++ ] = \
								( CIPHERSUITE_INFO * ) &endOfList;
		cipherSuiteInfoTbl[ cipherSuiteCount ] = \
								( CIPHERSUITE_INFO * ) &endOfList;

		cipherSuitInfoInited = TRUE;
		}

	*cipherSuiteInfoPtrPtrPtr = ( const CIPHERSUITE_INFO ** ) \
								cipherSuiteInfoTbl;	/* For gcc */
	*noSuiteEntries = cipherSuiteCount;

	return( CRYPT_OK );
	}
#endif /* USE_SSL */
