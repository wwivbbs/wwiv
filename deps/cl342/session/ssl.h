/****************************************************************************
*																			*
*						SSL v3/TLS Definitions Header File					*
*						Copyright Peter Gutmann 1998-2011					*
*																			*
****************************************************************************/

#ifndef _SSL_DEFINED

#define _SSL_DEFINED

#if defined( INC_ALL )
  #include "scorebrd.h"
#else
  #include "session/scorebrd.h"
#endif /* _STREAM_DEFINED */

/****************************************************************************
*																			*
*								SSL Constants								*
*																			*
****************************************************************************/

/* Default SSL/TLS port */

#define SSL_PORT					443

/* SSL and TLS constants */

#define ID_SIZE						1	/* ID byte */
#define LENGTH_SIZE					3	/* 24 bits */
#define SEQNO_SIZE					8	/* 64 bits */
#define VERSIONINFO_SIZE			2	/* 0x03, 0x0n */
#define ALERTINFO_SIZE				2	/* level + description */
#define SSL_HEADER_SIZE				5	/* Type, version, length */
#define SSL_NONCE_SIZE				32	/* Size of client/svr nonce */
#define SSL_SECRET_SIZE				48	/* Size of premaster/master secret */
#define MD5MAC_SIZE					16	/* Size of MD5 proto-HMAC/dual hash */
#define SHA1MAC_SIZE				20	/* Size of SHA-1 proto-HMAC/dual hash */
#define SHA2MAC_SIZE				32	/* Size of SHA-2 HMAC hash */
#define GCMICV_SIZE					16	/* Size of GCM ICV */
#define GCM_SALT_SIZE				4	/* Size of implicit portion of GCM IV */
#define GCM_IV_SIZE					12	/* Overall size of GCM IV */
#define TLS_HASHEDMAC_SIZE			12	/* Size of TLS PRF( MD5 + SHA1 ) */
#define SESSIONID_SIZE				16	/* Size of session ID */
#define MAX_SESSIONID_SIZE			32	/* Max.allowed session ID size */
#define MAX_KEYBLOCK_SIZE			( ( 64 + 32 + 16 ) * 2 )
										/* HMAC-SHA2 + AES-256 key + AES IV */
#define MIN_PACKET_SIZE				4	/* Minimum SSL packet size */
#define MAX_PACKET_SIZE				16384	/* Maximum SSL packet size */
#define MAX_CIPHERSUITES			200	/* Max.allowed cipher suites */

/* SSL/TLS packet/buffer size information.  The extra packet size is 
   somewhat large because it can contains the packet header (5 bytes), IV 
   (0/8/16 bytes), MAC/ICV (12/16/20 bytes), and cipher block padding (up to 
   256 bytes) */

#define EXTRA_PACKET_SIZE			512	

/* We can default to preferring either RSA key transport or DH key 
   agreement, DH has numerous benefits (PFS and resistance to side-channel 
   attacks) but one major disadvantage, it has the same relatively high cost 
   on the client as it does on the server, requiring a DH (pseudo-)private 
   key operation on both client and server while RSA only has the high cost 
   on the server.  The problem with automatically preferring DH to RSA is 
   that the people who least understand how the crypto works will then run a 
   timing test and decide that it's too slow based on the DH timing rather 
   than the much faster RSA timing.  There isn't really any easy way out of 
   this, since the benefits of using DH are rather significant and the 
   client, if it really needs performance over everything, can force the use 
   of RSA, we prefer DH to RSA unless the behaviour is toggled via the 
   following define */

/* #define PREFER_RSA_TO_DH */

#if defined( PREFER_RSA_TO_DH ) && defined( _MSC_VER )
  #pragma message( "  Building with RSA as preferred SSL/TLS cipher suite." )
#endif /* PREFER_RSA_TO_DH with VC++ */

/* SSLv2 was finally removed in MSIE and Firefox in 2008, however some 
   crufty old implementations will still send SSLv2 hellos.  Define the 
   following to enable handling of SSLv2 client hellos.  Note that this 
   code is not maintained, and your warranty is void when you enable SSLv2 
   hello handling */

/* #define ALLOW_SSLV2_HELLO */

/* SSL/TLS protocol-specific flags that augment the general session flags:

	FLAG_ALERTSENT: Whether we've already sent a close-alert.  Keeping track 
		of this is necessary because we're required to send a close alert 
		when shutting down to prevent a truncation attack, however lower-
		level code may have already sent an alert so we have to remember not 
		to send it twice.

	FLAG_DISABLE_CERTVERIFY: Disable checking of the server certificate 
	FLAG_DISABLE_NAMEVERIFY: and/or host name.  By default we check both of
		these, but because of all of the problems surrounding certificates 
		we allow the checking to be disabled.

	FLAG_CHECKREHANDSHAKE: The header-read got a handshake packet instead of 
		a data packet, when the body-read decrypts the payload it should 
		check for a rehandshake request in the payload.

	FLAG_CLIAUTHSKIPPED: The client saw an auth-request from the server and 
		responded with a no-certificates alert, if we later get a close 
		alert from the server then we provide additional error information 
		indicating that this may be due to the lack of a client certificate.

	FLAG_GCM: The encryption used is GCM and not the usual CBC, which 
		unifies encryption and MACing into a single operation.
	
	FLAG_SUITEB_128: Enforce Suite B semantics on top of the standard TLS 
	FLAG_SUITEB_256: 1.2 + ECC + AES-GCM ones.  _128 = P256 + P384, 
		_256 = P384 only */

#define SSL_PFLAG_NONE				0x00	/* No protocol-specific flags */
#define SSL_PFLAG_ALERTSENT			0x01	/* Close alert sent */
#define SSL_PFLAG_CLIAUTHSKIPPED	0x02	/* Client auth-req.skipped */
#define SSL_PFLAG_GCM				0x04	/* Encryption uses GCM, not CBC */
#define SSL_PFLAG_SUITEB_128		0x08	/* Enforce Suite B 128-bit semantics */
#define SSL_PFLAG_SUITEB_256		0x10	/* Enforce Suite B 256-bit semantics */
#define SSL_PFLAG_CHECKREHANDSHAKE	0x20	/* Check decrypted pkt.for rehandshake */
#define SSL_PFLAG_DISABLE_NAMEVERIFY 0x40	/* Disable host name verification */
#define SSL_PFLAG_DISABLE_CERTVERIFY 0x80	/* Disable certificate verification */
#define SSL_PFLAG_MAX				0xFF	/* Maximum possible flag value */

/* Suite B consists of two subclasses, the 128-bit security level (AES-128 
   with P256 and SHA2-256) and the 192-bit security level (AES-256 with P384 
   and SHA2-384), in order to identify generic use of Suite B we provide a
   pseudo-value that combines the 128-bit and 192-bit subclasses */

#define SSL_PFLAG_SUITEB			( SSL_PFLAG_SUITEB_128 | \
									  SSL_PFLAG_SUITEB_256 )

/* SSL/TLS message types */

#define SSL_MSG_CHANGE_CIPHER_SPEC	20
#define SSL_MSG_ALERT				21
#define SSL_MSG_HANDSHAKE			22
#define SSL_MSG_APPLICATION_DATA	23

#define SSL_MSG_FIRST				SSL_MSG_CHANGE_CIPHER_SPEC
#define SSL_MSG_LAST				SSL_MSG_APPLICATION_DATA

/* Special expected packet-type values that are passed to readHSPacketSSL() 
   to handle situations where special-case handling is required for read
   packets.  The first handshake packet from the client or server is treated 
   specially in that the version number information is taken from this 
   packet, and the attempt to read the first encrypted handshake packet may 
   be met with a TCP close from the peer if it handles errors badly, in 
   which case we provide a special-case error message that indicates more
   than just "connection closed" */

#define SSL_MSG_FIRST_HANDSHAKE		0xFE
#define SSL_MSG_FIRST_ENCRHANDSHAKE	0xFF
#define SSL_MSG_LAST_SPECIAL		SSL_MSG_FIRST_ENCRHANDSHAKE
#define SSL_MSG_V2HANDSHAKE			0x80

/* SSL/TLS handshake message subtypes */

#define SSL_HAND_CLIENT_HELLO		1
#define SSL_HAND_SERVER_HELLO		2
#define SSL_HAND_CERTIFICATE		11
#define SSL_HAND_SERVER_KEYEXCHANGE	12
#define SSL_HAND_SERVER_CERTREQUEST	13
#define SSL_HAND_SERVER_HELLODONE	14
#define SSL_HAND_CLIENT_CERTVERIFY	15
#define SSL_HAND_CLIENT_KEYEXCHANGE	16
#define SSL_HAND_FINISHED			20
#define SSL_HAND_SUPPLEMENTAL_DATA	23

#define SSL_HAND_FIRST				SSL_HAND_CLIENT_HELLO
#define SSL_HAND_LAST				SSL_HAND_SUPPLEMENTAL_DATA

/* SSL and TLS alert levels and types */

#define SSL_ALERTLEVEL_WARNING				1
#define SSL_ALERTLEVEL_FATAL				2

#define SSL_ALERT_CLOSE_NOTIFY				0
#define SSL_ALERT_UNEXPECTED_MESSAGE		10
#define SSL_ALERT_BAD_RECORD_MAC			20
#define TLS_ALERT_DECRYPTION_FAILED			21
#define TLS_ALERT_RECORD_OVERFLOW			22
#define SSL_ALERT_DECOMPRESSION_FAILURE		30
#define SSL_ALERT_HANDSHAKE_FAILURE			40
#define SSL_ALERT_NO_CERTIFICATE			41
#define SSL_ALERT_BAD_CERTIFICATE			42
#define SSL_ALERT_UNSUPPORTED_CERTIFICATE	43
#define SSL_ALERT_CERTIFICATE_REVOKED		44
#define SSL_ALERT_CERTIFICATE_EXPIRED		45
#define SSL_ALERT_CERTIFICATE_UNKNOWN		46
#define TLS_ALERT_ILLEGAL_PARAMETER			47
#define TLS_ALERT_UNKNOWN_CA				48
#define TLS_ALERT_ACCESS_DENIED				49
#define TLS_ALERT_DECODE_ERROR				50
#define TLS_ALERT_DECRYPT_ERROR				51
#define TLS_ALERT_EXPORT_RESTRICTION		60
#define TLS_ALERT_PROTOCOL_VERSION			70
#define TLS_ALERT_INSUFFICIENT_SECURITY		71
#define TLS_ALERT_INTERNAL_ERROR			80
#define TLS_ALERT_USER_CANCELLED			90
#define TLS_ALERT_NO_RENEGOTIATION			100
#define TLS_ALERT_UNSUPPORTED_EXTENSION		110
#define TLS_ALERT_CERTIFICATE_UNOBTAINABLE	111
#define TLS_ALERT_UNRECOGNIZED_NAME			112
#define TLS_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE 113
#define TLS_ALERT_BAD_CERTIFICATE_HASH_VALUE 114
#define TLS_ALERT_UNKNOWN_PSK_IDENTITY		115

#define SSL_ALERT_FIRST						SSL_ALERT_CLOSE_NOTIFY
#define SSL_ALERT_LAST						TLS_ALERT_UNKNOWN_PSK_IDENTITY

/* TLS supplemental data subtypes */

#define TLS_SUPPDATA_USERMAPPING			0

/* SSL and TLS cipher suites */

typedef enum {
	/* SSLv3 cipher suites (0-10) */
	SSL_NULL_WITH_NULL, SSL_RSA_WITH_NULL_MD5, SSL_RSA_WITH_NULL_SHA,
	SSL_RSA_EXPORT_WITH_RC4_40_MD5,		/* Non-valid/accapted suites */
	SSL_RSA_WITH_RC4_128_MD5, SSL_FIRST_VALID_SUITE = SSL_RSA_WITH_RC4_128_MD5,
	SSL_RSA_WITH_RC4_128_SHA, SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
	SSL_RSA_WITH_IDEA_CBC_SHA, SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_RSA_WITH_DES_CBC_SHA, SSL_RSA_WITH_3DES_EDE_CBC_SHA,

	/* TLS (RFC 2246) DH cipher suites (11-22) */
	TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA, TLS_DH_DSS_WITH_DES_CBC_SHA,
	TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA, TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
	TLS_DH_RSA_WITH_DES_CBC_SHA, TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA,
	TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA, TLS_DHE_DSS_WITH_DES_CBC_SHA,
	TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA, TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
	TLS_DHE_RSA_WITH_DES_CBC_SHA, TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,

	/* TLS (RFC 2246) anon-DH cipher suites (23-27) */
	TLS_DH_anon_EXPORT_WITH_RC4_40_MD5, TLS_DH_anon_WITH_RC4_128_MD5,
	TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA, TLS_DH_anon_WITH_DES_CBC_SHA,
	TLS_DH_anon_WITH_3DES_EDE_CBC_SHA,

	/* TLS (RFC 2246) reserved cipher suites (28-29, used for Fortezza in
	   SSLv3) */
	TLS_reserved_1, TLS_reserved_2,

	/* TLS with Kerberos (RFC 2712) suites (30-43) */
	TLS_KRB5_WITH_DES_CBC_SHA, TLS_KRB5_WITH_3DES_EDE_CBC_SHA,
	TLS_KRB5_WITH_RC4_128_SHA, TLS_KRB5_WITH_IDEA_CBC_SHA,
	TLS_KRB5_WITH_DES_CBC_MD5, TLS_KRB5_WITH_3DES_EDE_CBC_MD5,
	TLS_KRB5_WITH_RC4_128_MD5, TLS_KRB5_WITH_IDEA_CBC_MD5,
	TLS_KRB5_EXPORT_WITH_DES_CBC_40_SHA, TLS_KRB5_EXPORT_WITH_RC2_CBC_40_SHA,
	TLS_KRB5_EXPORT_WITH_RC4_40_SHA, TLS_KRB5_EXPORT_WITH_DES_CBC_40_MD5,
	TLS_KRB5_EXPORT_WITH_RC2_CBC_40_MD5, TLS_KRB5_EXPORT_WITH_RC4_40_MD5,

	/* Unknown suites (44-46) */

	/* TLS 1.1 (RFC 4346) cipher suites (47-58) */
	TLS_RSA_WITH_AES_128_CBC_SHA = 0x2F, TLS_DH_DSS_WITH_AES_128_CBC_SHA,
	TLS_DH_RSA_WITH_AES_128_CBC_SHA, TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_128_CBC_SHA, TLS_DH_anon_WITH_AES_128_CBC_SHA,
	TLS_RSA_WITH_AES_256_CBC_SHA, TLS_DH_DSS_WITH_AES_256_CBC_SHA,
	TLS_DH_RSA_WITH_AES_256_CBC_SHA, TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_256_CBC_SHA, TLS_DH_anon_WITH_AES_256_CBC_SHA,

	/* TLS 1.2 (RFC 5246) cipher suites (59-61) */
	TLS_RSA_WITH_NULL_SHA256, TLS_RSA_WITH_AES_128_CBC_SHA256,
	TLS_RSA_WITH_AES_256_CBC_SHA256,

	/* TLS 1.2 (RFC 5246) DH cipher suites (62-64), continued at 103 */
	TLS_DH_DSS_WITH_AES_128_CBC_SHA256, TLS_DH_RSA_WITH_AES_128_CBC_SHA256, 
	TLS_DHE_DSS_WITH_AES_128_CBC_SHA256, 

	/* Camellia (RFC 4132) AES-128 suites (65-70) */
	TLS_RSA_WITH_CAMELLIA_128_CBC_SHA, TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA, 
	TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA, TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA, 
	TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA, TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA,

	/* Unknown suites (71-103) */

	/* More TLS 1.2 (RFC 5246) DH cipher suites (103-109) */
	TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 = 103, TLS_DH_DSS_WITH_AES_256_CBC_SHA256,
	TLS_DH_RSA_WITH_AES_256_CBC_SHA256, TLS_DHE_DSS_WITH_AES_256_CBC_SHA256,
	TLS_DHE_RSA_WITH_AES_256_CBC_SHA256, TLS_DH_anon_WITH_AES_128_CBC_SHA256,
	TLS_DH_anon_WITH_AES_256_CBC_SHA256,

	/* Camellia (RFC 4132) AES-256 suites (132-137) */
	TLS_RSA_WITH_CAMELLIA_256_CBC_SHA = 132,
	TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA, TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA,
	TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA, TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
	TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA,

	/* TLS-PSK (RFC 4279) cipher suites (138-149) */
	TLS_PSK_WITH_RC4_128_SHA, TLS_PSK_WITH_3DES_EDE_CBC_SHA, 
	TLS_PSK_WITH_AES_128_CBC_SHA, TLS_PSK_WITH_AES_256_CBC_SHA, 
	TLS_DHE_PSK_WITH_RC4_128_SHA, TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA,
	TLS_DHE_PSK_WITH_AES_128_CBC_SHA, TLS_DHE_PSK_WITH_AES_256_CBC_SHA,
	TLS_RSA_PSK_WITH_RC4_128_SHA, TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA,
	TLS_RSA_PSK_WITH_AES_128_CBC_SHA, TLS_RSA_PSK_WITH_AES_256_CBC_SHA,

	/* Unknown suites (150-155) */

	/* TLS 1.2 (RFC 5288) GCM cipher suites (156-167) */
	TLS_RSA_WITH_AES_128_GCM_SHA256 = 156, TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256, TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DH_RSA_WITH_AES_128_GCM_SHA256, TLS_DH_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DHE_DSS_WITH_AES_128_GCM_SHA256, TLS_DHE_DSS_WITH_AES_256_GCM_SHA384,
    TLS_DH_DSS_WITH_AES_128_GCM_SHA256, TLS_DH_DSS_WITH_AES_256_GCM_SHA384,
    TLS_DH_anon_WITH_AES_128_GCM_SHA256, TLS_DH_anon_WITH_AES_256_GCM_SHA384,

	/* TLS-ECC (RFC 4492) cipher suites.  For some unknown reason these 
	   start above 49152/0xC000, so the range is 49153...49177 */
	TLS_ECDH_ECDSA_WITH_NULL_SHA = 49153, TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
	TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
	TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA, TLS_ECDHE_ECDSA_WITH_NULL_SHA,
	TLS_ECDHE_ECDSA_WITH_RC4_128_SHA, TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
	TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
	TLS_ECDH_RSA_WITH_NULL_SHA, TLS_ECDH_RSA_WITH_RC4_128_SHA,
	TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
	TLS_ECDH_RSA_WITH_AES_256_CBC_SHA, TLS_ECDHE_RSA_WITH_NULL_SHA,
	TLS_ECDHE_RSA_WITH_RC4_128_SHA, TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
	TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
	TLS_ECDH_anon_WITH_NULL_SHA, TLS_ECDH_anon_WITH_RC4_128_SHA,
	TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA, TLS_ECDH_anon_WITH_AES_128_CBC_SHA,
	TLS_ECDH_anon_WITH_AES_256_CBC_SHA,

	/* TLS-SRP (RFC 5054) cipher suites, following the pattern from 
	   above at 49178/0xC01A...49186 */
	TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA, TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA,
	TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA, TLS_SRP_SHA_WITH_AES_128_CBC_SHA,
	TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA, TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA,
	TLS_SRP_SHA_WITH_AES_256_CBC_SHA, TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA,
	TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA,

	/* TLS-ECC (RFC 5289) SHA-2 cipher suites, following the pattern from 
	   above at 49187/0xC023...49194 */
	TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
	TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256, TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384,
	TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
	TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256, TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384,

	/* TLS-ECC (RFC 5289) GCM cipher suites, following the pattern from above
	   at 49195/0xC02B...49202 */
	TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
	TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256, TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384,
	TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
	TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256, TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384,

	/* TLS-ECC (RFC 5489) PSK cipher suites, following the pattern from above
	   at 49203/0xC033...49211 */
	TLS_ECDHE_PSK_WITH_RC4_128_SHA, TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA, TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA,
	TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256, TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_PSK_WITH_NULL_SHA, TLS_ECDHE_PSK_WITH_NULL_SHA256,
    TLS_ECDHE_PSK_WITH_NULL_SHA384,

	SSL_LAST_SUITE
	} SSL_CIPHERSUITE_TYPE;

/* TLS extension types */

typedef enum {
	TLS_EXT_SERVER_NAME,		/* Name of virtual server to contact */
	TLS_EXT_MAX_FRAGMENT_LENTH,	/* Max.fragment length if smaller than 2^14 bytes */
	TLS_EXT_CLIENT_CERTIFICATE_URL,	/* Location for server to find client certificate */
	TLS_EXT_TRUSTED_CA_KEYS,	/* Indication of which CAs clients trust */
	TLS_EXT_TRUNCATED_HMAC,		/* Use 80-bit truncated HMAC */
	TLS_EXT_STATUS_REQUEST,		/* OCSP status request from server */
	TLS_EXT_USER_MAPPING,		/* RFC 4681 mapping of user name to account */
	TLS_EXT_CLIENT_AUTHZ,		/* RFC 5878 authorisation exts */
	TLS_EXT_SERVER_AUTHZ,		/* RFC 5878 authorisation exts */
	TLS_EXT_CERTTYPE,			/* RFC 5081/6091 OpenPGP key support */
	TLS_EXT_ELLIPTIC_CURVES,	/* RFC 4492 ECDH/ECDSA support */
	TLS_EXT_EC_POINT_FORMATS,	/* RFC 4492 ECDH/ECDSA support */
	TLS_EXT_SRP,				/* RFC 5054 SRP support */
	TLS_EXT_SIGNATURE_ALGORITHMS,	/* RFC 5246 TLSv1.2 */
		/* 14...34 unused */
	TLS_EXT_SESSIONTICKET = 35,	/* RFC 4507 session ticket support */
	TLS_EXT_LAST,

	/* The secure-renegotiation extension, for some unknown reason, is given
	   a value of 65281 / 0xFF01, so we defined it outside the usual 
	   extension range in order for the standard range-checking to be a bit
	   more sensible */
	TLS_EXT_SECURE_RENEG = 65281,/* RFC 5746 secure renegotiation */
	} TLS_EXT_TYPE;

/* SSL/TLS certificate types */

typedef enum {
	TLS_CERTTYPE_NONE, TLS_CERTTYPE_RSA, TLS_CERTTYPE_DSA, 
	TLS_CERTTYPE_DUMMY1 /* RSA+DH */, TLS_CERTTYPE_DUMMY2 /* DSA+DH */,
	TLS_CERTTYPE_DUMMY3 /* RSA+EDH */, TLS_CERTTYPE_DUMMY4 /* DSA+EDH */,
	TLS_CERTTYPE_ECDSA = 64, TLS_CERTTYPE_LAST
	} TLS_CERTTYPE_TYPE;

/* TLS signature and hash algorithm identifiers */

typedef enum {
	TLS_SIGALGO_NONE, TLS_SIGALGO_RSA, TLS_SIGALGO_DSA, TLS_SIGALGO_ECDSA,
	TLS_SIGALGO_LAST 
	} TLS_SIGALGO_TYPE;

typedef enum {
	TLS_HASHALGO_NONE, TLS_HASHALGO_MD5, TLS_HASHALGO_SHA1, 
	TLS_HASHALGO_DUMMY1 /* SHA2-224 */, TLS_HASHALGO_SHA2, 
	TLS_HASHALGO_SHA384, TLS_HASHALGO_DUMMY3 /* SHA2-512 */, 
	TLS_HASHALGO_LAST 
	} TLS_HASHALGO_TYPE;

/* TLS ECC curve identifiers */

typedef enum {
	TLS_CURVE_NONE, TLS_CURVE_SECT163K1, TLS_CURVE_SECT163R1, 
	TLS_CURVE_SECT163R2, TLS_CURVE_SECT193R1, TLS_CURVE_SECT193R2, 
	TLS_CURVE_SECT233K1, TLS_CURVE_SECT233R1, TLS_CURVE_SECT239K1, 
	TLS_CURVE_SECT283K1, TLS_CURVE_SECT283R1, TLS_CURVE_SECT409K1, 
	TLS_CURVE_SECT409R1, TLS_CURVE_SECT571K1, TLS_CURVE_SECT571R1, 
	TLS_CURVE_SECP160K1, TLS_CURVE_SECP160R1, TLS_CURVE_SECP160R2, 
	TLS_CURVE_SECP192K1, TLS_CURVE_SECP192R1 /* P192 */, 
	TLS_CURVE_SECP224K1, TLS_CURVE_SECP224R1 /* P224 */, 
	TLS_CURVE_SECP256K1, TLS_CURVE_SECP256R1 /* P256 */, 
	TLS_CURVE_SECP384R1 /* P384 */, TLS_CURVE_SECP521R1 /* P521 */,
	TLS_CURVE_LAST
	} TLS_CURVE_TYPE;

/* SSL and TLS major and minor version numbers */

#define SSL_MAJOR_VERSION		3
#define SSL_MINOR_VERSION_SSL	0
#define SSL_MINOR_VERSION_TLS	1
#define SSL_MINOR_VERSION_TLS11	2
#define SSL_MINOR_VERSION_TLS12	3

/* SSL sender label values for the finished message MAC */

#define SSL_SENDER_CLIENTLABEL	"CLNT"
#define SSL_SENDER_SERVERLABEL	"SRVR"
#define SSL_SENDERLABEL_SIZE	4

/* SSL/TLS cipher suite flags.  These are:

	CIPHERSUITE_PSK: Suite is a TLS-PSK suite and is used only if we're
		using TLS-PSK.

	CIPHERSUITE_DH:	 Suite is a DH suite.

	CIPHERSUITE_ECC: Suite is an ECC suite and is used only if ECC is
		enabled.

	CIPHERSUITE_GCM: Encryption uses GCM instead of the usual CBC.

	CIPHERSUITE_TLS12: Suite is a TLS 1.2 suite and is only sent if
		TLS 1.2 is enabled */

#define CIPHERSUITE_FLAG_NONE	0x00	/* No suite */
#define CIPHERSUITE_FLAG_PSK	0x01	/* TLS-PSK suite */
#define CIPHERSUITE_FLAG_DH		0x02	/* DH suite */
#define CIPHERSUITE_FLAG_ECC	0x04	/* ECC suite */
#define CIPHERSUITE_FLAG_TLS12	0x08	/* TLS 1.2 suite */
#define CIPHERSUITE_FLAG_GCM	0x10	/* GCM instead of CBC */
#define CIPHERSUITE_FLAG_MAX	0x1F	/* Maximum possible flag value */

typedef struct {
	/* The SSL/TLS cipher suite */
	const int cipherSuite;
#ifndef NDEBUG
	const char *description;
#endif /* NDEBUG */

	/* cryptlib algorithms for the cipher suite */
	const CRYPT_ALGO_TYPE keyexAlgo, authAlgo, cryptAlgo, macAlgo;

	/* Auxiliary information for the suite */
	const int macParam, cryptKeySize, macBlockSize;
	const int flags;
	} CIPHERSUITE_INFO;

/* When building the debug version of the code we include a text string 
   describing the cipher suite which is being processed, this makes it 
   easier to track down problems */

#ifndef NDEBUG
  #define DESCRIPTION( text )		text,
#else
  #define DESCRIPTION( text )
#endif /* NDEBUG */

/* If we're configured to only use Suite B algorithms, we override the
   algoAvailable() check to report that only Suite B algorithms are
   available */

#ifdef CONFIG_SUITEB

#if defined( _MSC_VER )
  #pragma message( "  Building with Suite B algorithms only." )
#endif /* VC++ */

#define algoAvailable( algo ) \
		( ( ( algo ) == CRYPT_ALGO_AES || ( algo ) == CRYPT_ALGO_ECDSA || \
			( algo ) == CRYPT_ALGO_ECDH || ( algo ) == CRYPT_ALGO_SHA2 || \
			( algo ) == CRYPT_ALGO_HMAC_SHA2 ) ? TRUE : FALSE )

  /* Special configuration defines to enable nonstandard behaviour for 
     Suite B tests */
  #ifdef CONFIG_SUITEB_TESTS 
	typedef enum {
		SUITEB_TEST_NONE,			/* No special test behaviour */

		/* RFC 5430bis tests */
		SUITEB_TEST_CLIINVALIDCURVE,/* Client sends non-Suite B curve */
		SUITEB_TEST_SVRINVALIDCURVE,/* Server sends non-Suite B curve */
		SUITEB_TEST_BOTHCURVES,		/* Client must send P256 and P384 as supp.curves */
		SUITEB_TEST_BOTHSIGALGOS,	/* Client must send SHA256 and SHA384 as sig.algos */

		SUITEB_TEST_LAST
		} SUITEB_TEST_VALUE;

	extern SUITEB_TEST_VALUE suiteBTestValue;
	extern BOOLEAN suiteBTestClientCert;
  #endif /* CONFIG_SUITEB_TESTS */
#endif /* Suite B algorithms only */

/****************************************************************************
*																			*
*								SSL Structures								*
*																			*
****************************************************************************/

/* SSL/TLS handshake state information.  This is passed around various 
   subfunctions that handle individual parts of the handshake */

typedef struct SL {
	/* Client and server dual-hash/hash contexts */
	CRYPT_CONTEXT md5context, sha1context, sha2context;
#ifdef CONFIG_SUITEB
	CRYPT_CONTEXT sha384context;
#endif /* CONFIG_SUITEB */

	/* Client and server nonces and session ID */
	BUFFER_FIXED( SSL_NONCE_SIZE ) \
	BYTE clientNonce[ SSL_NONCE_SIZE + 8 ];
	BUFFER_FIXED( SSL_NONCE_SIZE ) \
	BYTE serverNonce[ SSL_NONCE_SIZE + 8 ];
	BUFFER( MAX_SESSIONID_SIZE, sessionIDlength ) \
	BYTE sessionID[ MAX_SESSIONID_SIZE + 8 ];
	int sessionIDlength;

	/* Premaster/master secret */
	BUFFER( CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE, premasterSecretSize ) \
	BYTE premasterSecret[ CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE + 8 ];
	int premasterSecretSize;

	/* Encryption/security information.  The encryption algorithm (cryptAlgo)
	   and integrity algorithm (integrityAlgo) are stored with the session
	   information, although the optional integrity-algorithm parameters are
	   stored here */
	CRYPT_CONTEXT dhContext;	/* DH context if DHE is being used */
	CRYPT_CONTEXT certVerifyContext;/* Hash context if client certs used */
	int cipherSuite;			/* Selected cipher suite */
	CRYPT_ALGO_TYPE keyexAlgo, authAlgo;/* Selected cipher suite algos */
	int integrityAlgoParam;		/* Optional param.for integrity algo */
	CRYPT_ALGO_TYPE keyexSigHashAlgo;/* Algo.for keyex authentication */
	int keyexSigHashAlgoParam;	/* Optional params.for keyex hash */
	int cryptKeysize;			/* Size of session key */

	/* Other information */
	int clientOfferedVersion;	/* Prot.vers.originally offered by client */
	int originalVersion;		/* Original version set by the user before
								   it was modified based on what the peer
								   requested */
#ifdef ALLOW_SSLV2_HELLO	/* 28/01/08 Disabled since it's now finally 
							   removed in MSIE and Firefox */
	BOOLEAN isSSLv2;			/* Client hello is SSLv2 */
#endif /* ALLOW_SSLV2_HELLO */
	BOOLEAN hasExtensions;		/* Hello has TLS extensions */
	BOOLEAN needSNIResponse;	/* Server needs to respond to SNI */
	BOOLEAN needRenegResponse;	/* Server needs to respond to reneg.ind.*/

	/* ECC-related information.  Since ECC algorithms have a huge pile of
	   parameters we need to parse any extensions that the client sends in 
	   order to locate any additional information required to handle them.  
	   In the worst case these can retroactively modify the already-
	   negotiated cipher suites, disabling the use of ECC algorithms after 
	   they were agreed on via cipher suites.  To handle this we remember
	   both the preferred mainstream suite and a pointer to the preferred
	   ECC suite in 'eccSuiteInfoPtr', if it later turns out that the use
	   of ECC is OK we reset the crypto parameters using the save ECC suite
	   pointer.
	   
	   If the use of ECC isn't retroactively disabled then the eccCurveID 
	   and sendECCPointExtn values indicate which curve to use and whether 
	   the server needs to respond with a point-extension indicator */
	BOOLEAN disableECC;			/* Extn.disabled use of ECC suites */
	CRYPT_ECCCURVE_TYPE eccCurveID;	/* cryptlib ID of ECC curve to use */
	BOOLEAN sendECCPointExtn;	/* Whether svr.has to respond with ECC point ext.*/
	const void *eccSuiteInfoPtr;	/* ECC suite information */

	/* The packet data stream.  Since SSL can encapsulate multiple handshake
	   packets within a single SSL packet, the stream has to be persistent
	   across the different handshake functions to allow the continuation of
	   packets */
	STREAM stream;				/* Packet data stream */

	/* Function pointers to handshaking functions.  These are set up as 
	   required depending on whether the session is client or server */
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *beginHandshake )( INOUT SESSION_INFO *sessionInfoPtr,
							 struct SL *handshakeInfo );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *exchangeKeys )( INOUT SESSION_INFO *sessionInfoPtr,
						   struct SL *handshakeInfo );
	} SSL_HANDSHAKE_INFO;

/****************************************************************************
*																			*
*							SSL/TLS Functions								*
*																			*
****************************************************************************/

/* Prototypes for functions in ssl.c */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
int readUint24( INOUT STREAM *stream );
STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint24( INOUT STREAM *stream, IN_LENGTH const int length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readEcdhValue( INOUT STREAM *stream,
				   OUT_BUFFER( valueMaxLen, *valueLen ) void *value,
				   IN_LENGTH_SHORT_MIN( 64 ) const int valueMaxLen,
				   OUT_LENGTH_PKC_Z int *valueLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int readSSLCertChain( INOUT SESSION_INFO *sessionInfoPtr, 
					  INOUT SSL_HANDSHAKE_INFO *handshakeInfo, 
					  INOUT STREAM *stream,
					  OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertChain, 
					  const BOOLEAN isServer );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeSSLCertChain( INOUT SESSION_INFO *sessionInfoPtr, 
					   INOUT STREAM *stream );

/* Prototypes for functions in ssl_cry.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int encryptData( const SESSION_INFO *sessionInfoPtr, 
				 INOUT_BUFFER( dataMaxLength, *dataLength ) \
					BYTE *data, 
				 IN_LENGTH const int dataMaxLength,
				 OUT_LENGTH_Z int *dataLength,
				 IN_LENGTH const int payloadLength );
				 /* This one's a bit tricky, the input is 
				    { data, payloadLength } which is padded (if necessary) 
					and the padded length returned in '*dataLength' */
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int decryptData( SESSION_INFO *sessionInfoPtr, 
				 INOUT_BUFFER_FIXED( dataLength ) \
					BYTE *data, 
				 IN_LENGTH const int dataLength, 
				 OUT_LENGTH_Z int *processedDataLength );
				/* This one's also tricky, the entire data block will be 
				   processed but only 'processedDataLength' bytes of result 
				   are valid output */
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int createMacSSL( INOUT SESSION_INFO *sessionInfoPtr, 
				  INOUT_BUFFER( dataMaxLength, *dataLength ) void *data, 
				  IN_LENGTH const int dataMaxLength, 
				  OUT_LENGTH_Z int *dataLength,
				  IN_LENGTH const int payloadLength, 
				  IN_RANGE( 0, 255 ) const int type );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int createMacTLS( INOUT SESSION_INFO *sessionInfoPtr, 
				  OUT_BUFFER( dataMaxLength, *dataLength ) void *data, 
				  IN_LENGTH const int dataMaxLength, 
				  OUT_LENGTH_Z int *dataLength,
				  IN_LENGTH const int payloadLength, 
				  IN_RANGE( 0, 255 ) const int type );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int checkMacSSL( INOUT SESSION_INFO *sessionInfoPtr, 
				 IN_BUFFER( dataLength ) const void *data, 
				 IN_LENGTH const int dataLength, 
				 IN_LENGTH_Z const int payloadLength, 
				 IN_RANGE( 0, 255 ) const int type, 
				 const BOOLEAN noReportError );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int checkMacTLS( INOUT SESSION_INFO *sessionInfoPtr, 
				 IN_BUFFER( dataLength ) const void *data, 
				 IN_LENGTH const int dataLength, 
				 IN_LENGTH_Z const int payloadLength, 
				 IN_RANGE( 0, 255 ) const int type, 
				 const BOOLEAN noReportError );
CHECK_RETVAL \
int macDataTLSGCM( IN_HANDLE const CRYPT_CONTEXT iCryptContext, 
				   IN_INT_Z const long seqNo, 
				   IN_RANGE( SSL_MINOR_VERSION_TLS, \
							 SSL_MINOR_VERSION_TLS12 ) const int version,
				   IN_LENGTH_Z const int payloadLength, 
				   IN_RANGE( 0, 255 ) const int type );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int hashHSPacketRead( const SSL_HANDSHAKE_INFO *handshakeInfo, 
					  INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int hashHSPacketWrite( const SSL_HANDSHAKE_INFO *handshakeInfo, 
					   INOUT STREAM *stream,
					   IN_LENGTH_Z const int offset );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5, 6, 8 ) ) \
int completeSSLDualMAC( IN_HANDLE const CRYPT_CONTEXT md5context,
						IN_HANDLE const CRYPT_CONTEXT sha1context, 
						OUT_BUFFER( hashValuesMaxLen, *hashValuesLen )
							BYTE *hashValues, 
						IN_LENGTH_SHORT_MIN( MD5MAC_SIZE + SHA1MAC_SIZE ) \
							const int hashValuesMaxLen,
						OUT_LENGTH_SHORT_Z int *hashValuesLen,
						IN_BUFFER( labelLength ) const char *label, 
						IN_RANGE( 1, 64 ) const int labelLength, 
						IN_BUFFER( masterSecretLen ) const BYTE *masterSecret, 
						IN_LENGTH_SHORT const int masterSecretLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5, 6, 8 ) ) \
int completeTLSHashedMAC( IN_HANDLE const CRYPT_CONTEXT md5context,
						  IN_HANDLE const CRYPT_CONTEXT sha1context, 
						  OUT_BUFFER( hashValuesMaxLen, *hashValuesLen ) \
								BYTE *hashValues, 
						  IN_LENGTH_SHORT_MIN( TLS_HASHEDMAC_SIZE ) \
								const int hashValuesMaxLen,
						  OUT_LENGTH_SHORT_Z int *hashValuesLen,
						  IN_BUFFER( labelLength ) const char *label, 
						  IN_RANGE( 1, 64 ) const int labelLength, 
						  IN_BUFFER( masterSecretLen ) const BYTE *masterSecret, 
						  IN_LENGTH_SHORT const int masterSecretLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4, 5, 7 ) ) \
int completeTLS12HashedMAC( IN_HANDLE const CRYPT_CONTEXT sha2context,
							OUT_BUFFER( hashValuesMaxLen, *hashValuesLen ) \
								BYTE *hashValues, 
							IN_LENGTH_SHORT_MIN( TLS_HASHEDMAC_SIZE ) \
								const int hashValuesMaxLen,
							OUT_LENGTH_SHORT_Z int *hashValuesLen,
							IN_BUFFER( labelLength ) const char *label, 
							IN_RANGE( 1, 64 ) const int labelLength, 
							IN_BUFFER( masterSecretLen ) const BYTE *masterSecret, 
							IN_LENGTH_SHORT const int masterSecretLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int createCertVerifyHash( const SESSION_INFO *sessionInfoPtr,
						  INOUT SSL_HANDSHAKE_INFO *handshakeInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int createCertVerify( const SESSION_INFO *sessionInfoPtr,
					  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
					  INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int checkCertVerify( const SESSION_INFO *sessionInfoPtr,
					 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
					 INOUT STREAM *stream, 
					 IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
						const int sigLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int createKeyexSignature( INOUT SESSION_INFO *sessionInfoPtr, 
						  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						  INOUT STREAM *stream, 
						  IN_BUFFER( keyDataLength ) const void *keyData, 
						  IN_LENGTH_SHORT const int keyDataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int checkKeyexSignature( INOUT SESSION_INFO *sessionInfoPtr, 
						 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						 INOUT STREAM *stream, 
						 IN_BUFFER( keyDataLength ) const void *keyData, 
						 IN_LENGTH_SHORT const int keyDataLength,
						 const BOOLEAN isECC );

/* Prototypes for functions in ssl_ext.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readExtensions( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
					INOUT STREAM *stream, 
					IN_LENGTH_SHORT const int length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeClientExtensions( INOUT STREAM *stream,
						   INOUT SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeServerExtensions( INOUT STREAM *stream,
						   INOUT SSL_HANDSHAKE_INFO *handshakeInfo );

/* Prototypes for functions in ssl_hs.c/ssl_hsc.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int processHelloSSL( INOUT SESSION_INFO *sessionInfoPtr, 
					 INOUT SSL_HANDSHAKE_INFO *handshakeInfo, 
					 INOUT STREAM *stream, const BOOLEAN isServer );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int completeHandshakeSSL( INOUT SESSION_INFO *sessionInfoPtr,
						  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						  const BOOLEAN isClient,
						  const BOOLEAN isResumedSession );

/* Prototypes for functions in ssl_keymgmt.c */

STDC_NONNULL_ARG( ( 1 ) ) \
void destroySecurityContextsSSL( INOUT SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initHandshakeCryptInfo( INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							const BOOLEAN isTLS12 );
STDC_NONNULL_ARG( ( 1 ) ) \
void destroyHandshakeCryptInfo( INOUT SSL_HANDSHAKE_INFO *handshakeInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int cloneHashContext( IN_HANDLE const CRYPT_CONTEXT hashContext,
					  OUT_HANDLE_OPT CRYPT_CONTEXT *clonedHashContext );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDHcontextSSL( OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
					  IN_BUFFER_OPT( keyDataLength ) const void *keyData, 
					  IN_LENGTH_SHORT_Z const int keyDataLength,
					  IN_HANDLE_OPT const CRYPT_CONTEXT iServerKeyTemplate,
					  IN_ENUM_OPT( CRYPT_ECCCURVE ) \
							const CRYPT_ECCCURVE_TYPE eccParams );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int createSharedPremasterSecret( OUT_BUFFER( premasterSecretMaxLength, \
											 *premasterSecretLength ) \
									void *premasterSecret, 
								 IN_LENGTH_SHORT \
									const int premasterSecretMaxLength, 
								 OUT_LENGTH_SHORT_Z int *premasterSecretLength,
								 IN_BUFFER( sharedSecretLength ) \
									const void *sharedSecret, 
								 IN_LENGTH_SHORT const int sharedSecretLength,
								 const BOOLEAN isEncodedValue );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int wrapPremasterSecret( INOUT SESSION_INFO *sessionInfoPtr,
						 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						 OUT_BUFFER( dataMaxLength, *dataLength ) void *data, 
						 IN_LENGTH_SHORT const int dataMaxLength, 
						 OUT_LENGTH_SHORT_Z int *dataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int unwrapPremasterSecret( INOUT SESSION_INFO *sessionInfoPtr, 
						   INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						   IN_BUFFER( dataLength ) const void *data, 
						   IN_LENGTH_SHORT const int dataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int initCryptoSSL( INOUT SESSION_INFO *sessionInfoPtr,
				   INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
				   OUT_BUFFER_FIXED( masterSecretSize ) void *masterSecret,
				   IN_LENGTH_SHORT const int masterSecretSize,
				   const BOOLEAN isClient,
				   const BOOLEAN isResumedSession );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int loadExplicitIV( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT STREAM *stream, 
					OUT_INT_SHORT_Z int *ivLength );

/* Prototypes for functions in ssl_rd.c */

CHECK_RETVAL_PTR \
const char *getSSLPacketName( IN_RANGE( 0, 255 ) const int packetType );
CHECK_RETVAL_PTR \
const char *getSSLHSPacketName( IN_RANGE( 0, 255 ) const int packetType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processVersionInfo( INOUT SESSION_INFO *sessionInfoPtr, 
						INOUT STREAM *stream, 
						OUT_OPT_INT_Z int *clientVersion );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int checkPacketHeaderSSL( INOUT SESSION_INFO *sessionInfoPtr, 
						  INOUT STREAM *stream, 
						  OUT_LENGTH_Z int *packetLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int checkHSPacketHeader( INOUT SESSION_INFO *sessionInfoPtr, 
						 INOUT STREAM *stream, 
						 OUT_LENGTH_Z int *packetLength, 
						 IN_RANGE( SSL_HAND_FIRST, \
								   SSL_HAND_LAST ) const int packetType, 
						 IN_LENGTH_SHORT_Z const int minSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int unwrapPacketSSL( INOUT SESSION_INFO *sessionInfoPtr, 
					 INOUT_BUFFER( dataMaxLength, \
								   *dataLength ) void *data, 
					 IN_LENGTH const int dataMaxLength, 
					 OUT_LENGTH_Z int *dataLength,
					 IN_RANGE( SSL_HAND_FIRST, \
							   SSL_HAND_LAST ) const int packetType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readHSPacketSSL( INOUT SESSION_INFO *sessionInfoPtr,
					 INOUT_OPT SSL_HANDSHAKE_INFO *handshakeInfo, 
					 OUT_LENGTH_Z int *packetLength, 
					 IN_RANGE( SSL_HAND_FIRST, \
							   SSL_MSG_LAST_SPECIAL ) const int packetType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int refreshHSStream( INOUT SESSION_INFO *sessionInfoPtr, 
					 INOUT SSL_HANDSHAKE_INFO *handshakeInfo );

/* Prototypes for functions in ssl_suites.c */

#ifndef CONFIG_SUITEB

CHECK_RETVAL \
int getCipherSuiteInfo( OUT const CIPHERSUITE_INFO ***cipherSuiteInfoPtrPtrPtr,
						OUT_INT_Z int *noSuiteEntries,
						const BOOLEAN isServer );
#else

#define getCipherSuiteInfo( infoPtr, noEntries, isServer ) \
		getSuiteBCipherSuiteInfo( infoPtr, noEntries, isServer, suiteBinfo )

CHECK_RETVAL \
int getSuiteBCipherSuiteInfo( OUT const CIPHERSUITE_INFO ***cipherSuiteInfoPtrPtrPtr,
							  OUT_INT_Z int *noSuiteEntries,
							  const BOOLEAN isServer,
							  IN_FLAGS_Z( SSL ) const int suiteBinfo );

#endif /* CONFIG_SUITEB */

/* Prototypes for functions in ssl_wr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int wrapPacketSSL( INOUT SESSION_INFO *sessionInfoPtr, 
				   INOUT STREAM *stream, 
				   IN_LENGTH_Z const int offset );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendPacketSSL( INOUT SESSION_INFO *sessionInfoPtr, 
				   INOUT STREAM *stream, const BOOLEAN sendOnly );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int openPacketStreamSSL( OUT STREAM *stream, 
						 const SESSION_INFO *sessionInfoPtr, 
						 IN_LENGTH_OPT const int bufferSize, 
						 IN_RANGE( SSL_HAND_FIRST, \
								   SSL_HAND_LAST ) const int packetType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int continuePacketStreamSSL( INOUT STREAM *stream, 
							 const SESSION_INFO *sessionInfoPtr, 
							 IN_RANGE( SSL_HAND_FIRST, \
									   SSL_HAND_LAST ) const int packetType,
							 OUT_LENGTH_SHORT_Z int *packetOffset );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int completePacketStreamSSL( INOUT STREAM *stream, 
							 IN_LENGTH_Z const int offset );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int continueHSPacketStream( INOUT STREAM *stream, 
							IN_RANGE( SSL_HAND_FIRST, \
									  SSL_HAND_LAST ) const int packetType,
							OUT_LENGTH_SHORT_Z int *packetOffset );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int completeHSPacketStream( INOUT STREAM *stream, 
							IN_LENGTH const int offset );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processAlert( INOUT SESSION_INFO *sessionInfoPtr, 
				  IN_BUFFER( headerLength ) const void *header, 
				  IN_LENGTH const int headerLength );
STDC_NONNULL_ARG( ( 1 ) ) \
void sendCloseAlert( INOUT SESSION_INFO *sessionInfoPtr, 
					 const BOOLEAN alertReceived );
STDC_NONNULL_ARG( ( 1 ) ) \
void sendHandshakeFailAlert( INOUT SESSION_INFO *sessionInfoPtr );

/* Prototypes for session mapping functions */

STDC_NONNULL_ARG( ( 1 ) ) \
void initSSLclientProcessing( INOUT SSL_HANDSHAKE_INFO *handshakeInfo );
STDC_NONNULL_ARG( ( 1 ) ) \
void initSSLserverProcessing( SSL_HANDSHAKE_INFO *handshakeInfo );

#endif /* _SSL_DEFINED */
