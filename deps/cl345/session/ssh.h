/****************************************************************************
*																			*
*						  SSH Definitions Header File						*
*						Copyright Peter Gutmann 1998-2015					*
*																			*
****************************************************************************/

#ifndef _SSH_DEFINED

#define _SSH_DEFINED

#if defined( USE_SSH_EXTENDED ) && defined( _MSC_VER )
  #pragma message( "  Building with extended SSH facilities enabled." )
#endif /* USE_SSH_EXTENDED with VC++ */

#ifdef USE_SESSIONS

/****************************************************************************
*																			*
*								SSH Constants								*
*																			*
****************************************************************************/

/* Default SSH port */

#define SSH_PORT				22

/* Various SSH constants */

#define ID_SIZE					1	/* ID byte */
#define LENGTH_SIZE				4	/* Size of packet length field */
#define UINT_SIZE				4	/* Size of integer value */
#define PADLENGTH_SIZE			1	/* Size of padding length field */
#define BOOLEAN_SIZE			1	/* Size of boolean value */

#define SSH2_COOKIE_SIZE		16	/* Size of SSHv2 cookie */
#define SSH2_HEADER_SIZE		5	/* Size of SSHv2 packet header */
#define SSH2_MIN_ALGOID_SIZE	4	/* Size of shortest SSHv2 algo.name */
#define SSH2_MIN_PADLENGTH_SIZE	4	/* Minimum amount of padding for packets */
#define SSH2_PAYLOAD_HEADER_SIZE 9	/* Size of SSHv2 inner payload header */
#define SSH2_FIXED_KEY_SIZE		16	/* Size of SSHv2 fixed-size keys */
#ifdef USE_DH1024
  #define SSH2_DEFAULT_KEYSIZE	128	/* Size of SSHv2 default DH key */
#else
  #define SSH2_DEFAULT_KEYSIZE	192	/* Size of SSHv2 default DH key */
#endif /* USE_DH1024 */

/* SSH packet/buffer size information.  The extra packet data is for
   additional non-payload information including the header, MAC, and up to
   256 bytes of padding */

#define MAX_PACKET_SIZE			262144L
#define EXTRA_PACKET_SIZE		512
#define DEFAULT_PACKET_SIZE		16384
#define MAX_WINDOW_SIZE			( MAX_INTLENGTH - 8192 )

/* By default cryptlib uses DH key agreement, which is supported by all 
   servers.  It's also possible to use ECDH key agreement, however due to
   SSH's braindamaged way of choosing algorithms the peer will always go
   for ECDH if it can handle it (see the comment in sesion/ssh2_svr.c for
   more on this), so we disable ECDH by default.  To use ECDH key agreement 
   in preference to DH, uncomment the following */

/* #define PREFER_ECC */
#if defined( PREFER_ECC ) && defined( _MSC_VER )
  #pragma message( "  Building with ECC preferred for SSH." )
#endif /* PREFER_ECC && Visual C++ */
#if defined( PREFER_ECC ) && \
	!( defined( USE_ECDH ) && defined( USE_ECDSA ) )
  #error PREFER_ECC can only be used with ECDH and ECDSA enabled
#endif /* PREFER_ECC && !( USE_ECDH && USE_ECDSA ) */

/* SSH protocol-specific flags that encode details of implementation bugs 
   that we need to work around */

#define SSH_PFLAG_NONE			0x00000/* No protocol-specific flags */
#define SSH_PFLAG_HMACKEYSIZE	0x00001/* Peer uses short HMAC keys */
#define SSH_PFLAG_SIGFORMAT		0x00002/* Peer omits signature algo name */
#define SSH_PFLAG_NOHASHSECRET	0x00004/* Peer omits secret in key derive */
#define SSH_PFLAG_NOHASHLENGTH	0x00008/* Peer omits length in exchange hash */
#define SSH_PFLAG_RSASIGPAD		0x00010/* Peer requires zero-padded RSA sig.*/
#define SSH_PFLAG_WINDOWSIZE	0x00020/* Peer mishandles large window sizes */
#define SSH_PFLAG_TEXTDIAGS		0x00040/* Peer dumps text diagnostics on error */
#define SSH_PFLAG_PAMPW			0x00080/* Peer chokes on "password" as PAM submethod */
#define SSH_PFLAG_DUMMYUSERAUTH	0x00100/* Peer requires dummy userAuth message */
#define SSH_PFLAG_EMPTYUSERAUTH	0x00200/* Peer sends empty userauth-failure response */
#define SSH_PFLAG_ZEROLENIGNORE	0x00400/* Peer sends zero-length SSH_IGNORE */
#define SSH_PFLAG_ASYMMCOPR		0x00800/* Peer sends asymmetric compression algos */
#define SSH_PFLAG_EMPTYSVCACCEPT 0x01000/* Peer sends empty SSH_SERVICE_ACCEPT */
#define SSH_PFLAG_CHECKSPKOK	0x02000/* Peer checks SSH_MSG_USERAUTH_PK_OK */
#define SSH_PFLAG_NOMTI			0x04000/* Peer doesn't support any MTI algorithms */
#define SSH_PFLAG_OLDGEX		0x08000/* Peer requires old-style KEX_DH_GEX_REQ */
#define SSH_PFLAG_CUTEFTP		0x10000/* CuteFTP, drops conn.during handshake */
#define SSH_PFLAG_CTR			0x20000/* Use CTR mode synthesised from ECB */
#define SSH_PFLAG_MAX			0x3FFFF/* Maximum possible flag value */

/* Various data sizes used for read-ahead and buffering.  The minimum SSH
   packet size is used to determine how much data we can read when reading
   a packet header, the SSH header remainder size is how much data we've
   got left once we've extracted just the length but no other data */

#define MIN_PACKET_SIZE			16
#define SSH_HEADER_REMAINDER_SIZE ( MIN_PACKET_SIZE - LENGTH_SIZE )

/* SSH ID information */

#define SSH_ID					"SSH-"		/* Start of SSH ID */
#define SSH_ID_SIZE				4	/* Size of SSH ID */
#define SSH_VERSION_SIZE		4	/* Size of SSH version */
#define SSH_ID_MAX_SIZE			255	/* Max.size of SSH ID string */
#define SSH_ID_STRING			"SSH-2.0-cryptlib"	/* cryptlib SSH ID strings */
#define SSH_ID_STRING_SIZE		16	/* Size of ID strings */

/* SSH packet types.  Note that the keyex (static DH keys), keyex_gex 
   (ephemeral DH keys), and keyex_ecdh (static ECDH keys) message types 
   overlap */

#define SSH_MSG_DISCONNECT		1	/* Disconnect session */
#define SSH_MSG_IGNORE			2	/* No-op */
#define SSH_MSG_UNIMPLEMENTED	3	/* Treat as disconnect */
#define SSH_MSG_DEBUG			4	/* No-op */
#define SSH_MSG_SERVICE_REQUEST 5	/* Request authentiction */
#define SSH_MSG_SERVICE_ACCEPT	6	/* Acknowledge request */
#define SSH_MSG_EXT_INFO		7	/* Extension info */
#define SSH_MSG_KEXINIT			20	/* Hello */
#define SSH_MSG_NEWKEYS			21	/* Change cipherspec */
#define SSH_MSG_KEXDH_INIT		30	/* DH, phase 1 */
#define SSH_MSG_KEXDH_REPLY		31	/* DH, phase 2 */
#define SSH_MSG_KEX_DH_GEX_REQUEST_OLD 30 /* Ephem.DH key request */
#define SSH_MSG_KEX_DH_GEX_GROUP 31	/* Ephem.DH key response */
#define SSH_MSG_KEX_DH_GEX_INIT	32	/* Ephem.DH, phase 1 */
#define SSH_MSG_KEX_DH_GEX_REPLY 33	/* Ephem.DH, phase 2 */
#define SSH_MSG_KEX_DH_GEX_REQUEST 34 /* Ephem.DH key request */
#define SSH_MSG_KEX_ECDH_INIT	30	/* ECDH, phase 1 */
#define SSH_MSG_KEX_ECDH_REPLY	31	/* ECDH, phase 2 */
#define SSH_MSG_USERAUTH_REQUEST 50 /* Request authentication */
#define SSH_MSG_USERAUTH_FAILURE 51 /* Authentication failed */
#define SSH_MSG_USERAUTH_SUCCESS 52 /* Authentication succeeded */
#define SSH_MSG_USERAUTH_BANNER 53	/* No-op */
#define SSH_MSG_USERAUTH_INFO_REQUEST 60 /* Generic auth.svr.request */
#define SSH_MSG_USERAUTH_INFO_RESPONSE 61 /* Generic auth.cli.response */
#define SSH_MSG_USERAUTH_PK_OK	60	/* Pubkey partial auth.continue */
#define SSH_MSG_GLOBAL_REQUEST	80	/* Perform a global ioctl */
#define SSH_MSG_GLOBAL_SUCCESS	81	/* Global request succeeded */
#define SSH_MSG_GLOBAL_FAILURE	82	/* Global request failed */
#define	SSH_MSG_CHANNEL_OPEN	90	/* Open a channel over an SSH link */
#define	SSH_MSG_CHANNEL_OPEN_CONFIRMATION 91	/* Channel open succeeded */
#define SSH_MSG_CHANNEL_OPEN_FAILURE 92	/* Channel open failed */
#define	SSH_MSG_CHANNEL_WINDOW_ADJUST 93	/* No-op */
#define SSH_MSG_CHANNEL_DATA	94	/* Data */
#define SSH_MSG_CHANNEL_EXTENDED_DATA 95	/* Out-of-band data */
#define SSH_MSG_CHANNEL_EOF		96	/* EOF */
#define SSH_MSG_CHANNEL_CLOSE	97	/* Close the channel */
#define SSH_MSG_CHANNEL_REQUEST 98	/* Perform a channel ioctl */
#define SSH_MSG_CHANNEL_SUCCESS 99	/* Channel request succeeded */
#define SSH_MSG_CHANNEL_FAILURE 100/* Channel request failed */

/* Special-case expected-packet-type values that are passed to
   readHSPacketSSH2()/readAuthPacketSSH2() to handle situations where more 
   than one packet type can be received (see the code in readHSPacket() for 
   details):
   
	MSG_SPECIAL_USERAUTH can return MSG_USERAUTH_FAILURE which indicates 
		a wrong password used iff it's a response to the client sending a 
		password, or MSG_EXT_INFO if the peer is using extensions.

	MSG_SPECIAL_USERAUTH_PAM can return the same values as 
		MSG_SPECIAL_USERAUTH, but can also return MSG_USERAUTH_INFO_REQUEST 
		as part of the multi-iteration PAM process.

	MSG_SPECIAL_CHANNEL can return SSH_MSG_CHANNEL_OPEN_FAILURE to indicate
		that the request was successfully processed but the channel wasn't
		opened.

	MSG_SPECIAL_REQUEST can return either MSG_GLOBAL_REQUEST or 
		MSG_CHANNEL_REQUEST when the other side sends a general request.

	MSG_SPECIAL_SERVICEACCEPT can return either MSG_SERVICEACCEPT or 
		MSG_EXT_INFO if the peer is using extensions */

#define SSH_MSG_SPECIAL_FIRST		500	/* Boundary for _SPECIAL types */
#define SSH_MSG_SPECIAL_USERAUTH	501	/* Value to handle SSH combined auth.*/
#define SSH_MSG_SPECIAL_USERAUTH_PAM 502 /* Value to handle SSH PAM auth.*/
#define SSH_MSG_SPECIAL_CHANNEL		503	/* Value to handle channel open */
#define SSH_MSG_SPECIAL_REQUEST		504	/* Value to handle SSH global/channel req.*/
#define SSH_MSG_SPECIAL_SERVICEACCEPT 505	/* Value to handle optional ext.info */
#define SSH_MSG_SPECIAL_LAST		506	/* Last valid _SPECIAL type */

/* SSH disconnection codes */

enum { SSH_DISCONNECT_NONE, SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT, 
	   SSH_DISCONNECT_PROTOCOL_ERROR, SSH_DISCONNECT_KEY_EXCHANGE_FAILED, 
	   SSH_DISCONNECT_RESERVED, SSH_DISCONNECT_MAC_ERROR, 
	   SSH_DISCONNECT_COMPRESSION_ERROR, SSH_DISCONNECT_SERVICE_NOT_AVAILABLE, 
	   SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED, 
	   SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE, SSH_DISCONNECT_CONNECTION_LOST,
	   SSH_DISCONNECT_BY_APPLICATION, SSH_DISCONNECT_TOO_MANY_CONNECTIONS,
	   SSH_DISCONNECT_AUTH_CANCELLED_BY_USER, 
	   SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE, 
	   SSH_DISCONNECT_ILLEGAL_USER_NAME, SSH_DISCONNECT_LAST };

/* SSH channel open failure codes */

#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED			1
#define SSH_OPEN_CONNECT_FAILED							2
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE					3
#define SSH_OPEN_RESOURCE_SHORTAGE						4

/* The status of the SSH protocol negotiation, which determines which packet 
   types are valid at this point */

typedef enum {
	SSH_PROTOSTATE_NONE,	/* No protocol state */
	SSH_PROTOSTATE_HANDSHAKE, /* Handshake */
	SSH_PROTOSTATE_AUTH,	/* Authentication */
	SSH_PROTOSTATE_DATA,	/* Data transfer */
	SSH_PROTOSTATE_LAST		/* Last possible protocol state */
	} SSH_PROTOCOL_STATE;

/* An indicator of any additional information that we may need to stuff into
   the SSH handshake when we write algorithm ID information */

typedef enum {
	SSH_ALGOSTRINGINFO_NONE,/* No addition information */
	SSH_ALGOSTRINGINFO_EXTINFO,	/* Client extension-neg.indicator */
	SSH_ALGOSTRINGINFO_EXTINFO_ALTDHALGOS,	/* EXTINFO + alt.DH algos */
	SSH_ALGOSTRINGINFO_LAST	/* Last possible protocol state */
	} SSH_ALGOSTRINGINFO_TYPE;

/* SSH requires the use of a number of additional (pseudo)-algorithm
   types that don't correspond to normal cryptlib algorithms.  To handle
   these, we define pseudo-algoID values that fall within the range of
   the normal algorithm ID types but that aren't normal algorithm IDs.
   
   In addition to the standard (pseudo)-algorithm types, SSH's 
   schizophrenic error reporting can also indicate that no particular
   algorithm is reuqired (or at least any algorithm is indicated and
   it's up to the other side to guess which), so we define a special 
   value for that as well */

#define CRYPT_PSEUDOALGO_AMBIGUOUS	( CRYPT_ALGO_LAST_CONVENTIONAL - 3 )
#define CRYPT_PSEUDOALGO_PASSWORD	( CRYPT_ALGO_LAST_CONVENTIONAL - 2 )
#define CRYPT_PSEUDOALGO_PAM		( CRYPT_ALGO_LAST_CONVENTIONAL - 1 )
#define CRYPT_PSEUDOALGO_LAST		( CRYPT_ALGO_LAST_CONVENTIONAL )

#define MK_ALGO( value )			( CRYPT_ALGO_TYPE ) ( CRYPT_##value )

/* The size of the encoded DH keyex value and the requested DHE key size, 
   which we have to store in encoded form so that we can hash them later in 
   the handshake */

#define MAX_ENCODED_KEYEXSIZE		( CRYPT_MAX_PKCSIZE + 16 ) 
#define ENCODED_REQKEYSIZE			( UINT_SIZE * 3 )

/* Check whether an algorithm ID is one of the above pseudo-algorithm
   types */

#define isPseudoAlgo( algorithm ) \
		( ( algorithm ) >= CRYPT_PSEUDOALGO_PASSWORD && \
		  ( algorithm ) <= CRYPT_PSEUDOALGO_PAM )

/* SSH algorithms are grouped into classes such as keyex algorithms or MAC
   algorithms, the following type identifies the different algorithm 
   classes */

typedef enum {
	SSH_ALGOCLASS_NONE,		/* No algorithm class */
	SSH_ALGOCLASS_KEYEX,	/* Keyex algorithms */
	SSH_ALGOCLASS_KEYEX_NOECC,/* Keyex algorithms limited to non-ECC algos */
	SSH_ALGOCLASS_ENCR,		/* Encryption algorithms */
	SSH_ALGOCLASS_SIGN,		/* Signature algorithms */
	SSH_ALGOCLASS_MAC,		/* MAC algorithms */
	SSH_ALGOCLASS_COPR,		/* Compression algorithms */
	SSH_ALGOCLASS_LAST		/* Last possible algorithm class */
	} SSH_ALGOCLASS_TYPE;

/* Values for working with SSH channels.  Channels have a 32-bit ID
   (although no sane implementation uses very large values), to deal with
   range checking for these we limit them to LONG_MAX in 32-bit systems.

   SSH channels have a number of SSH-internal attributes that aren't exposed 
   as cryptlib-wide attribute types.  The following values are used to 
   access SSH-internal channel attributes */

#ifdef SYSTEM_64BIT
  #define CHANNEL_MAX				0x0FFFFFFFFUL
#else
  #define CHANNEL_MAX				LONG_MAX
#endif /* 32- vs 64-bit systems */

typedef enum {
	SSH_ATTRIBUTE_NONE,						/* No channel attribute */
	SSH_ATTRIBUTE_ACTIVE,					/* Channel is active */
	SSH_ATTRIBUTE_WINDOWCOUNT,				/* Data window count */
	SSH_ATTRIBUTE_WINDOWSIZE,				/* Data window size */
	SSH_ATTRIBUTE_ALTCHANNELNO,				/* Secondary channel no. */
	SSH_ATTRIBUTE_LAST						/* Last channel attribute */
	} SSH_ATTRIBUTE_TYPE;

/* Check whether a DH/ECDH value is valid for a given server key size.  The 
   check is slightly different for the ECC version because the value is
   a composite ECC point with two coordinates, so we have to divide the 
   length by two to get the size of a single coordinate.  
   
   In addition when we print an error message based on the check we need to 
   extract the underlying size from the overall data item.  The reason for 
   masking the LSB for the DH extraction is because the value can have a
   leading zero byte that isn't counted as part of the length */

#define isValidDHsize( value, serverKeySize, extraLength ) \
		( ( value ) > ( ( serverKeySize ) - 8 ) + ( extraLength ) && \
		  ( value ) < ( ( serverKeySize ) + 2 ) + ( extraLength ) )
#define extractDHsize( value, extraLength ) \
		( ( value & ~1 ) - extraLength )
#define isValidECDHsize( value, serverKeySize, extraLength ) \
		( ( value ) / 2 > ( ( serverKeySize ) - 8 ) + ( extraLength ) && \
		  ( value ) / 2 < ( ( serverKeySize ) + 2 ) + ( extraLength ) )
#define extractECDHsize( value, extraLength ) \
		( ( value - ( extraLength + 1 ) ) / 2 )

/* The following macro can be used to enable dumping of PDUs to disk.  As a
   safeguard, this only works in the Win32 debug version to prevent it from
   being accidentally enabled in any release version */

#if defined( __WIN32__ ) && !defined( NDEBUG )
  #define DEBUG_DUMP_SSH( buffer, length, isRead ) \
		  debugDumpSSH( sessionInfoPtr, buffer, length, isRead )

  STDC_NONNULL_ARG( ( 1, 2 ) ) \
  void debugDumpSSH( const SESSION_INFO *sessionInfoPtr,
					 IN_BUFFER( length ) const void *buffer, 
					 IN_LENGTH_SHORT const int length,
					 const BOOLEAN isRead );
#else
  #define DEBUG_DUMP_SSH( buffer, length, isRead )
#endif /* Win32 debug */

/****************************************************************************
*																			*
*								SSH Structures								*
*																			*
****************************************************************************/

/* Mapping of SSH algorithm names to cryptlib algorithm IDs, in preferred
   algorithm order.  Some of the algorithms are pure algorithms while others
   are more like cipher suites, in order to check whether they're available
   for use we have to map the suite pseudo-value into one or more actual
   algorithms, which are given via the checkXXXAlgo values */

typedef struct {
	/* Mapping from algorithm name to cryptlib algorithm ID.  The
	   "encryption" algorithm may not be a real cryptlib algorithm type but 
	   an SSH-specific pseudo-algorithm in the CRYPT_PSEUDOALGO_xxx 
	   range, for example CRYPT_PSEUDOALGO_PASSWORD */
	BUFFER_FIXED( nameLen ) \
	const char *name;						/* Algorithm name */
	const int nameLen;
	const CRYPT_ALGO_TYPE algo;				/* Algorithm ID */

	/* Optional parameters needed when the algorithm actually represents a 
	   cipher suite.  The subAlgo is typically an algorithm but may be a 
	   mode for conventional ciphers, the parameter is typically the key
	   size */
	const int subAlgo;
	const int parameter;
	} ALGO_STRING_INFO;

/* SSH handshake state information.  This is passed around various
   subfunctions that handle individual parts of the handshake */

#ifdef SH
  /* VxWorks defines 'SH' (for SuperH CPUs), so we have to undefine it to
     allow the following struct to be declared.  The VxWorks include file
	 order is such that it comes after osspec.h is included, so we can't
	 cover up the problem in that */
  #undef SH
#endif /* SH */

struct SH;

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *SSH_HANDSHAKE_FUNCTION )( INOUT SESSION_INFO *sessionInfoPtr,
									 INOUT struct SH *handshakeInfo );

typedef struct SH {
	/* SSH exchange hash */
	BUFFER_FIXED( SSH2_COOKIE_SIZE ) \
	BYTE cookie[ SSH2_COOKIE_SIZE + 8 ];	/* Anti-spoofing cookie */
	BUFFER_FIXED( CRYPT_MAX_HASHSIZE ) \
	BYTE sessionID[ CRYPT_MAX_HASHSIZE + 8 ];/* Session ID/exchange hash */
	int sessionIDlength;
	CRYPT_ALGO_TYPE exchangeHashAlgo;		/* Exchange hash algorithm */
	CRYPT_CONTEXT iExchangeHashContext, iExchangeHashAltContext;
											/* Hash of exchanged information */

	/* Information needed to compute the session ID.  SSH requires the 
	   client and server DH/ECDH values (along with various other things, 
	   but these are hashed inline).  The SSH values are in MPI-encoded 
	   form so we need to reserve a little extra room for the length and 
	   leading zero-padding */
	BUFFER( MAX_ENCODED_KEYEXSIZE, clientKeyexValueLength ) \
	BYTE clientKeyexValue[ MAX_ENCODED_KEYEXSIZE + 8 ];
	BUFFER( MAX_ENCODED_KEYEXSIZE, serverKeyexValueLength ) \
	BYTE serverKeyexValue[ MAX_ENCODED_KEYEXSIZE + 8 ];
	int clientKeyexValueLength, serverKeyexValueLength;

	/* Encryption algorithm and key information */
	CRYPT_ALGO_TYPE pubkeyAlgo;				/* Host signature algo */
	CRYPT_ALGO_TYPE hashAlgo;				/* Host signature hash algo */
	int cryptKeysize;						/* Size of session key */
	BUFFER( CRYPT_MAX_PKCSIZE, secretValueLength ) \
	BYTE secretValue[ CRYPT_MAX_PKCSIZE + 8 ];	/* Shared secret value */
	int secretValueLength;

	/* DH/ECDH key agreement context and the client requested DH key size 
	   for the key exchange.  Alongside the actual key size we also store 
	   the original encoded form, which has to be hashed as part of the 
	   exchange hash.  The long-term host key is stored as the session 
	   information iKeyexCryptContext for the client and privateKey for the 
	   server.   Since ECDH doesn't just entail a new algorithm but an 
	   entire cipher suite, we provide a flag to make checking for this 
	   easier */
	CRYPT_ALGO_TYPE keyexAlgo;				/* Keyex algo */
	CRYPT_CONTEXT iServerCryptContext;
	int serverKeySize, requestedServerKeySize;
	BUFFER( ENCODED_REQKEYSIZE, encodedReqKeySizesLength ) \
	BYTE encodedReqKeySizes[ ENCODED_REQKEYSIZE + 8 ];
	int encodedReqKeySizesLength;
	BOOLEAN isFixedDH;						/* Whether DH is fixed or negotiated */
	BOOLEAN isECDH;							/* Use of ECC cipher suite */

	/* Miscellaneous information */
	BOOLEAN sendExtInfo;					/* Whether to send extended info */

	/* Table mapping SSH algorithm names to cryptlib algorithm IDs.  This 
	   serves two purposes, firstly by declaring it once in ssh2.c and 
	   referring to it via pointers we can make the data static const, which 
	   is necessary in some environments to get them into the read-only 
	   segment, and secondly for the server where we advertise algorithm X 
	   to the client it allows us to switch to a restricted table that only
	   allows algorithm X in return from the client */
	const ALGO_STRING_INFO *algoStringPubkeyTbl;
	int algoStringPubkeyTblNoEntries;

	/* Function pointers to handshaking functions.  These are set up as
	   required depending on whether the session is client or server */
	FNPTR beginHandshake, exchangeKeys, completeHandshake;
	} SSH_HANDSHAKE_INFO;

/* Channel number and ID used to mark an unused channel */

#define UNUSED_CHANNEL_NO	CRYPT_ERROR
#define UNUSED_CHANNEL_ID	0

/****************************************************************************
*																			*
*								SSH Functions								*
*																			*
****************************************************************************/

/* Unlike SSL, SSH only hashes portions of the handshake, and even then not
   complete packets but arbitrary bits and pieces.  In order to perform the
   hashing, we have to be able to bookmark positions in a stream to allow
   the data at that point to be hashed once it's been encoded or decoded.  
   The following macros set and complete a bookmark.

   When we create or continue a packet stream, the packet type is written
   before we can set the bookmark.  To handle this, we also provide a macro
   that sets the bookmark for a full packet by adjusting for the packet type
   that's already been written */

#define streamBookmarkSet( stream, offset ) \
		offset = stell( stream )
#define streamBookmarkSetFullPacket( stream, offset ) \
		offset = stell( stream ) - ID_SIZE
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int streamBookmarkComplete( INOUT STREAM *stream, 
							OUT_OPT_PTR void **dataPtrPtr, 
							OUT_DATALENGTH_Z int *length, 
							IN_DATALENGTH const int position );

/* Prototypes for functions in ssh.c */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckSessionSSH( IN const SESSION_INFO *sessionInfoPtr );

/* Prototypes for functions in ssh2.c */

STDC_NONNULL_ARG( ( 1 ) ) \
void initHandshakeCrypt( INOUT SSH_HANDSHAKE_INFO *handshakeInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int hashHandshakeStrings( INOUT SSH_HANDSHAKE_INFO *handshakeInfo,
						  IN_BUFFER( clientStringLength ) \
								const void *clientString,
						  IN_LENGTH_SHORT const int clientStringLength,
						  IN_BUFFER( serverStringLength ) \
								const void *serverString,
						  IN_LENGTH_SHORT const int serverStringLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 6 ) ) \
int readAlgoString( INOUT STREAM *stream, 
					IN_ARRAY( noAlgoStringEntries ) \
						const ALGO_STRING_INFO *algoInfo,
					IN_RANGE( 1, 100 ) const int noAlgoStringEntries, 
					OUT_ALGO_Z CRYPT_ALGO_TYPE *algo, 
					const BOOLEAN useFirstMatch, 
					INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoString( INOUT STREAM *stream, 
					 IN_ALGO const CRYPT_ALGO_TYPE algo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoStringEx( INOUT STREAM *stream, 
					   IN_ALGO const CRYPT_ALGO_TYPE algo,
					   IN_INT_SHORT_Z const int subAlgo,
					   IN_INT_SHORT_Z const int parameter,
					   IN_ENUM_OPT( SSH_ALGOSTRINGINFO ) \
							const SSH_ALGOSTRINGINFO_TYPE algoStringInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeAlgoList( INOUT STREAM *stream, 
				   IN_ARRAY( noAlgoStringInfoEntries ) \
						const ALGO_STRING_INFO *algoStringInfoTbl,
				   IN_RANGE( 1, 10 ) const int noAlgoStringInfoEntries );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoClassList( INOUT STREAM *stream, 
						IN_ENUM( SSH_ALGOCLASS ) \
							const SSH_ALGOCLASS_TYPE algoClass );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int processHelloSSH( INOUT SESSION_INFO *sessionInfoPtr,
					 INOUT SSH_HANDSHAKE_INFO *handshakeInfo, 
					 OUT_LENGTH_SHORT_Z int *keyexLength,
					 const BOOLEAN isServer );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readExtensionsSSH( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeExtensionsSSH( INOUT SESSION_INFO *sessionInfoPtr,
					    INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int checkReadPublicKey( INOUT STREAM *stream,
						OUT_ALGO_Z CRYPT_ALGO_TYPE *pubkeyAlgo,
						OUT_INT_SHORT_Z int *keyDataStart,
						INOUT ERROR_INFO *errorInfo );

/* Prototypes for functions in ssh2_authc.c/ssh2_auths.c  */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processClientAuth( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT SSH_HANDSHAKE_INFO *handshakeInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processServerAuth( INOUT SESSION_INFO *sessionInfoPtr,
					   const SSH_HANDSHAKE_INFO *handshakeInfo, 
					   const BOOLEAN userInfoPresent );

/* Prototypes for functions in ssh2_chn.c */

typedef enum { CHANNEL_NONE, CHANNEL_READ, CHANNEL_WRITE,
			   CHANNEL_BOTH, CHANNEL_LAST } CHANNEL_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createChannel( INOUT SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int addChannel( INOUT SESSION_INFO *sessionInfoPtr, 
				IN const long channelNo,
				IN_LENGTH_MIN( PACKET_SIZE_MIN ) const int maxPacketSize, 
				IN_BUFFER( typeLen ) const void *type,
				IN_LENGTH_SHORT const int typeLen, 
				IN_BUFFER_OPT( arg1Len ) const void *arg1, 
				IN_LENGTH_SHORT_Z const int arg1Len );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteChannel( INOUT SESSION_INFO *sessionInfoPtr, 
				   IN const long channelNo,
				   IN_ENUM( CHANNEL ) const CHANNEL_TYPE channelType,
				   const BOOLEAN deleteLastChannel );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int selectChannel( INOUT SESSION_INFO *sessionInfoPtr, 
				   IN const long channelNo,
				   IN_ENUM_OPT( CHANNEL ) const CHANNEL_TYPE channelType );
CHECK_RETVAL_RANGE_NOERROR( UNUSED_CHANNEL_NO, CHANNEL_MAX ) STDC_NONNULL_ARG( ( 1 ) ) \
long getCurrentChannelNo( const SESSION_INFO *sessionInfoPtr,
						  IN_ENUM( CHANNEL ) const CHANNEL_TYPE channelType );
CHECK_RETVAL_ENUM( CHANNEL ) STDC_NONNULL_ARG( ( 1 ) ) \
CHANNEL_TYPE getChannelStatusByChannelNo( const SESSION_INFO *sessionInfoPtr,
										  IN const long channelNo );
CHECK_RETVAL_ENUM( CHANNEL ) STDC_NONNULL_ARG( ( 1 ) ) \
CHANNEL_TYPE getChannelStatusByAddr( const SESSION_INFO *sessionInfoPtr,
									 IN_BUFFER( addrInfoLen ) const char *addrInfo,
									 IN_LENGTH_SHORT const int addrInfoLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getChannelAttribute( const SESSION_INFO *sessionInfoPtr,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						 OUT_INT_Z int *value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getChannelAttributeS( const SESSION_INFO *sessionInfoPtr,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						  OUT_BUFFER_OPT( dataMaxLength, *dataLength ) \
								void *data, 
						  IN_LENGTH_SHORT_Z const int dataMaxLength, 
						  OUT_LENGTH_BOUNDED_Z( dataMaxLength ) \
								int *dataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getChannelExtAttribute( const SESSION_INFO *sessionInfoPtr,
							IN_ENUM( SSH_ATTRIBUTE ) \
								const SSH_ATTRIBUTE_TYPE attribute,
							OUT_INT_Z int *value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setChannelAttribute( INOUT SESSION_INFO *sessionInfoPtr,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						 IN_INT_SHORT const int value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int setChannelAttributeS( INOUT SESSION_INFO *sessionInfoPtr,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						  IN_BUFFER( dataLength ) const void *data, 
						  IN_LENGTH_TEXT const int dataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setChannelExtAttribute( const SESSION_INFO *sessionInfoPtr,
							IN_ATTRIBUTE const SSH_ATTRIBUTE_TYPE attribute,
							IN_INT_Z const int value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int enqueueResponse( INOUT SESSION_INFO *sessionInfoPtr, 
					 IN_RANGE( 1, 255 ) const int type,
					 IN_RANGE( 0, 4 ) const int noParams, 
					 IN const long channelNo,
					 const int param1, const int param2, const int param3 );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sendEnqueuedResponse( INOUT SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int enqueueChannelData( INOUT SESSION_INFO *sessionInfoPtr, 
						IN_RANGE( 1, 255 ) const int type,
						IN const long channelNo, 
						const int param );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int appendChannelData( INOUT SESSION_INFO *sessionInfoPtr, 
					   IN_LENGTH_SHORT_Z const int offset );

/* Prototypes for functions in ssh2_msg.c */

CHECK_RETVAL_RANGE_NOERROR( 10000, MAX_WINDOW_SIZE ) STDC_NONNULL_ARG( ( 1 ) ) \
int getWindowSize( const SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int closeChannel( INOUT SESSION_INFO *sessionInfoPtr,
				  const BOOLEAN closeAllChannels );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processChannelControlMessage( INOUT SESSION_INFO *sessionInfoPtr,
								  INOUT STREAM *stream );

/* Prototypes for functions in ssh2_msgc.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sendChannelOpen( INOUT SESSION_INFO *sessionInfoPtr );

/* Prototypes for functions in ssh2_msgs.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processChannelOpen( INOUT SESSION_INFO *sessionInfoPtr, 
						INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processChannelRequest( INOUT SESSION_INFO *sessionInfoPtr,
						   INOUT STREAM *stream, 
						   IN const long prevChannelNo );

/* Prototypes for functions in ssh2_cry.c */

typedef enum { MAC_NONE, MAC_START, MAC_END, MAC_LAST } MAC_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initSecurityInfo( INOUT SESSION_INFO *sessionInfoPtr,
					  INOUT SSH_HANDSHAKE_INFO *handshakeInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initSecurityContextsSSH( INOUT SESSION_INFO *sessionInfoPtr,
							 INOUT SSH_HANDSHAKE_INFO *handshakeInfo );
STDC_NONNULL_ARG( ( 1 ) ) \
void destroySecurityContextsSSH( INOUT SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initDHcontextSSH( OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
					  OUT_LENGTH_SHORT_Z int *keySize,
					  IN_BUFFER_OPT( keyDataLength ) const void *keyData, 
					  IN_LENGTH_SHORT_Z const int keyDataLength,
					  IN_LENGTH_SHORT_OPT const int requestedKeySize );
#ifdef USE_ECDH
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initECDHcontextSSH( OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
						OUT_LENGTH_SHORT_Z int *keySize,
						IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo );
#endif /* USE_ECDH */
#ifdef USE_SSH_CTR
CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4 ) ) \
int ctrModeCrypt( IN_HANDLE const CRYPT_CONTEXT iCryptContext,
				  INOUT_BUFFER_FIXED( blockSize ) void *counter,
				  IN_LENGTH_IV const int counterSize,
				  INOUT_BUFFER_FIXED( dataLength ) void *data,
				  IN_LENGTH const int dataLength );
#endif /* USE_SSH_CTR */
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int completeKeyex( INOUT SESSION_INFO *sessionInfoPtr,
				   INOUT SSH_HANDSHAKE_INFO *handshakeInfo,
				   const BOOLEAN isServer );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int hashAsString( IN_HANDLE const CRYPT_CONTEXT iHashContext,
				  IN_BUFFER( dataLength ) const BYTE *data, 
				  IN_LENGTH_SHORT const int dataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int hashAsMPI( IN_HANDLE const CRYPT_CONTEXT iHashContext, 
			   IN_BUFFER( dataLength ) const BYTE *data, 
			   IN_LENGTH_SHORT const int dataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int checkMacSSH( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
				 IN_INT const long seqNo,
				 IN_BUFFER( dataMaxLength ) const BYTE *data, 
				 IN_DATALENGTH const int dataMaxLength, 
				 IN_DATALENGTH_Z const int dataLength, 
				 IN_RANGE( 16, CRYPT_MAX_HASHSIZE ) const int macLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int checkMacSSHIncremental( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
							IN_INT_Z const long seqNo,
							IN_BUFFER( dataMaxLength ) const BYTE *data, 
							IN_DATALENGTH const int dataMaxLength, 
							IN_DATALENGTH_Z const int dataLength, 
							IN_DATALENGTH_Z const int packetDataLength, 
							IN_ENUM( MAC ) const MAC_TYPE macType, 
							IN_RANGE( 16, CRYPT_MAX_HASHSIZE ) const int macLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int createMacSSH( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
				  IN_INT const long seqNo,
				  IN_BUFFER( dataMaxLength ) BYTE *data, 
				  IN_DATALENGTH const int dataMaxLength, 
				  IN_DATALENGTH const int dataLength );

/* Prototypes for functions in ssh2_rd.c */

#ifdef USE_ERRMSGS
CHECK_RETVAL_PTR_NONNULL \
const char *getSSHPacketName( IN_RANGE( 0, 255 ) const int packetType );
#else
#define getSSHPacketName( packetType )	"unknown"
#endif /* USE_ERRMSGS */
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int readPacketHeaderSSH2( INOUT SESSION_INFO *sessionInfoPtr,
						  IN_RANGE( SSH_MSG_DISCONNECT, \
									SSH_MSG_SPECIAL_REQUEST ) \
								const int expectedType, 
						  OUT_LENGTH_Z int *packetLength,
						  OUT_DATALENGTH_Z int *packetExtraLength,
						  INOUT SSH_INFO *sshInfo,
						  INOUT READSTATE_INFO *readInfo,
						  const SSH_PROTOCOL_STATE protocolState );
CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1 ) ) \
int readHSPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
					  IN_RANGE( SSH_MSG_DISCONNECT, \
								SSH_MSG_SPECIAL_REQUEST ) \
							int expectedType,
					  IN_RANGE( 1, 1024 ) const int minPacketSize );
CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1 ) ) \
int readAuthPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
						IN_RANGE( SSH_MSG_DISCONNECT, \
								  SSH_MSG_SPECIAL_REQUEST ) \
							int expectedType,
						IN_RANGE( 1, 1024 ) const int minPacketSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getDisconnectInfo( INOUT SESSION_INFO *sessionInfoPtr, 
					   INOUT STREAM *stream );

/* Prototypes for functions in ssh2_wr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int openPacketStreamSSH( OUT STREAM *stream, 
						 const SESSION_INFO *sessionInfoPtr,
						 IN_RANGE( SSH_MSG_DISCONNECT, \
								   SSH_MSG_CHANNEL_FAILURE ) 
							const int packetType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int openPacketStreamSSHEx( OUT STREAM *stream, 
						   const SESSION_INFO *sessionInfoPtr,
						   IN_DATALENGTH const int bufferSize, 
						   IN_RANGE( SSH_MSG_DISCONNECT, \
									 SSH_MSG_CHANNEL_FAILURE ) 
								const int packetType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int continuePacketStreamSSH( INOUT STREAM *stream, 
							 IN_RANGE( SSH_MSG_DISCONNECT, \
									   SSH_MSG_CHANNEL_FAILURE ) \
								const int packetType,
							 OUT_DATALENGTH_Z int *packetOffset );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int wrapPlaintextPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
							 INOUT STREAM *stream,
							 IN_DATALENGTH_Z const int offset );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int wrapPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT STREAM *stream,
					IN_DATALENGTH_Z const int offset, 
					const BOOLEAN useQuantisedPadding );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int wrapSendPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
						INOUT STREAM *stream );

/* Prototypes for session mapping functions */

STDC_NONNULL_ARG( ( 1 ) ) \
void initSSH2processing( INOUT SESSION_INFO *sessionInfoPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void initSSH2clientProcessing( INOUT SSH_HANDSHAKE_INFO *handshakeInfo );
STDC_NONNULL_ARG( ( 1 ) ) \
void initSSH2serverProcessing( INOUT SSH_HANDSHAKE_INFO *handshakeInfo );
#endif /* USE_SESSIONS */
#endif /* _SSH_DEFINED */
