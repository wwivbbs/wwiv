/****************************************************************************
*																			*
*						SSHv1/SSHv2 Definitions Header File					*
*						Copyright Peter Gutmann 1998-2008					*
*																			*
****************************************************************************/

#ifndef _SSH_DEFINED

#define _SSH_DEFINED

#if defined( USE_SSH_EXTENDED ) && defined( _MSC_VER )
  #pragma message( "  Building with extended SSH facilities enabled." )
#endif /* USE_SSH_EXTENDED with VC++ */

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

#define SSH1_COOKIE_SIZE		8	/* Size of SSHv1 cookie */
#define SSH1_HEADER_SIZE		5	/* Size of SSHv1 packet header */
#define SSH1_CRC_SIZE			4	/* Size of CRC value */
#define SSH1_MPI_LENGTH_SIZE	2	/* Size of MPI length field */
#define SSH1_SESSIONID_SIZE		16	/* Size of SSHv1 session ID */
#define SSH1_SECRET_SIZE		32	/* Size of SSHv1 shared secret */
#define SSH1_CHALLENGE_SIZE		32	/* Size of SSHv1 RSA auth.challenge */
#define SSH1_RESPONSE_SIZE		16	/* Size of SSHv1 RSA auth.response */

#define SSH2_COOKIE_SIZE		16	/* Size of SSHv2 cookie */
#define SSH2_HEADER_SIZE		5	/* Size of SSHv2 packet header */
#define SSH2_MIN_ALGOID_SIZE	4	/* Size of shortest SSHv2 algo.name */
#define SSH2_MIN_PADLENGTH_SIZE	4	/* Minimum amount of padding for packets */
#define SSH2_PAYLOAD_HEADER_SIZE 9	/* Size of SSHv2 inner payload header */
#define SSH2_FIXED_KEY_SIZE		16	/* Size of SSHv2 fixed-size keys */
#define SSH2_DEFAULT_KEYSIZE	128	/* Size of SSHv2 default DH key */

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

/* #define PREFER_ECC_SUITES */
#if defined( PREFER_ECC_SUITES ) && defined( _MSC_VER )
  #pragma message( "  Building with ECC preferred for SSH." )
#endif /* PREFER_ECC_SUITES && Visual C++ */
#if defined( PREFER_ECC_SUITES ) && \
	!( defined( USE_ECDH ) && defined( USE_ECDSA ) )
  #error PREFER_ECC_SUITES can only be used with ECDH and ECDSA enabled
#endif /* PREFER_ECC_SUITES && !( USE_ECDH && USE_ECDSA ) */

/* SSH protocol-specific flags that encode details of implementation bugs 
   that we need to work around */

#define SSH_PFLAG_NONE			0x0000/* No protocol-specific flags */
#define SSH_PFLAG_HMACKEYSIZE	0x0001/* Peer uses short HMAC keys */
#define SSH_PFLAG_SIGFORMAT		0x0002/* Peer omits signature algo name */
#define SSH_PFLAG_NOHASHSECRET	0x0004/* Peer omits secret in key derive */
#define SSH_PFLAG_NOHASHLENGTH	0x0008/* Peer omits length in exchange hash */
#define SSH_PFLAG_RSASIGPAD		0x0010/* Peer requires zero-padded RSA sig.*/
#define SSH_PFLAG_WINDOWSIZE	0x0020/* Peer mishandles large window sizes */
#define SSH_PFLAG_TEXTDIAGS		0x0040/* Peer dumps text diagnostics on error */
#define SSH_PFLAG_PAMPW			0x0080/* Peer chokes on "password" as PAM submethod */
#define SSH_PFLAG_DUMMYUSERAUTH	0x0100/* Peer requires dummy userAuth message */
#define SSH_PFLAG_EMPTYUSERAUTH	0x0200/* Peer sends empty userauth-failure response */
#define SSH_PFLAG_ZEROLENIGNORE	0x0400/* Peer sends zero-length SSH_IGNORE */
#define SSH_PFLAG_ASYMMCOPR		0x0800/* Peer sends asymmetric compression algos */
#define SSH_PFLAG_EMPTYSVCACCEPT 0x1000/* Peer sends empty SSH_SERVICE_ACCEPT */
#define SSH_PFLAG_CUTEFTP		0x2000/* CuteFTP, drops conn.during handshake */
#define SSH_PFLAG_MAX			0x3FFF/* Maximum possible flag value */

/* Various data sizes used for read-ahead and buffering.  The minimum SSH
   packet size is used to determine how much data we can read when reading
   a packet header, the SSHv2 header remainder size is how much data we've
   got left once we've extracted just the length but no other data, the
   SSHv2 remainder size is how much data we've got left once we've
   extracted all fixed information values, and the SSHv1 maximum header size
   is used to determine how much space we need to reserve at the start of
   the buffer when encoding SSHv1's variable-length data packets (SSHv2 has
   a fixed header size so this isn't a problem any more) */

#define MIN_PACKET_SIZE			16
#define SSH2_HEADER_REMAINDER_SIZE \
								( MIN_PACKET_SIZE - LENGTH_SIZE )
#define SSH1_MAX_HEADER_SIZE	( LENGTH_SIZE + 8 + ID_SIZE + LENGTH_SIZE )

/* SSH ID information */

#define SSH_ID					"SSH-"		/* Start of SSH ID */
#define SSH_ID_SIZE				4	/* Size of SSH ID */
#define SSH_VERSION_SIZE		4	/* Size of SSH version */
#define SSH_ID_MAX_SIZE			255	/* Max.size of SSHv2 ID string */
#define SSH1_ID_STRING			"SSH-1.5-cryptlib"
#define SSH2_ID_STRING			"SSH-2.0-cryptlib"	/* cryptlib SSH ID strings */
#define SSH_ID_STRING_SIZE		16	/* Size of ID strings */

/* SSHv1 packet types */

#define SSH1_MSG_DISCONNECT		1	/* Disconnect session */
#define SSH1_SMSG_PUBLIC_KEY	2	/* Server public key */
#define SSH1_CMSG_SESSION_KEY	3	/* Encrypted session key */
#define SSH1_CMSG_USER			4	/* User name */
#define SSH1_CMSG_AUTH_RSA		6	/* RSA public key */
#define SSH1_SMSG_AUTH_RSA_CHALLENGE 7	/* RSA challenge from server */
#define SSH1_CMSG_AUTH_RSA_RESPONSE 8	/* RSA response from client */
#define SSH1_CMSG_AUTH_PASSWORD	9	/* Password */
#define SSH1_CMSG_REQUEST_PTY	10	/* Request a pty */
#define SSH1_CMSG_WINDOW_SIZE	11	/* Terminal window size change */
#define SSH1_CMSG_EXEC_SHELL	12	/* Request a shell */
#define SSH1_CMSG_EXEC_CMD		13	/* Request command execution */
#define SSH1_SMSG_SUCCESS		14	/* Success status message */
#define SSH1_SMSG_FAILURE		15	/* Failure status message */
#define SSH1_CMSG_STDIN_DATA	16	/* Data from client stdin */
#define SSH1_SMSG_STDOUT_DATA	17	/* Data from server stdout */
#define SSH1_SMSG_EXITSTATUS	20	/* Exit status of command run on server */
#define SSH1_MSG_IGNORE			32	/* No-op */
#define SSH1_CMSG_EXIT_CONFIRMATION 33 /* Client response to server exitstatus */
#define SSH1_MSG_DEBUG			36	/* Debugging/informational message */
#define SSH1_CMSG_MAX_PACKET_SIZE 38	/* Maximum data packet size */

/* Further SSHv1 packet types that aren't used but which we need to
   recognise */

#define SSH1_CMSG_PORT_FORWARD_REQUEST		28
#define SSH1_CMSG_AGENT_REQUEST_FORWARDING	30
#define SSH1_CMSG_X11_REQUEST_FORWARDING	34
#define SSH1_CMSG_REQUEST_COMPRESSION		37

/* SSHv1 cipher types */

#define SSH1_CIPHER_NONE		0	/* No encryption */
#define SSH1_CIPHER_IDEA		1	/* IDEA/CFB */
#define SSH1_CIPHER_DES			2	/* DES/CBC */
#define SSH1_CIPHER_3DES		3	/* 3DES/inner-CBC (nonstandard) */
#define SSH1_CIPHER_TSS			4	/* Deprecated */
#define SSH1_CIPHER_RC4			5	/* RC4 */
#define SSH1_CIPHER_BLOWFISH	6	/* Blowfish */
#define SSH1_CIPHER_CRIPPLED	7	/* Reserved, from ssh 1.2.x source */

/* SSHv1 authentication types */

#define SSH1_AUTH_RHOSTS		1	/* .rhosts or /etc/hosts.equiv */
#define SSH1_AUTH_RSA			2	/* RSA challenge-response */
#define SSH1_AUTH_PASSWORD		3	/* Password */
#define SSH1_AUTH_RHOSTS_RSA	4	/* .rhosts with RSA challenge-response */
#define SSH1_AUTH_TIS			5	/* TIS authsrv */
#define SSH1_AUTH_KERBEROS		6	/* Kerberos */
#define SSH1_PASS_KERBEROS_TGT	7	/* Kerberos TGT-passing */

/* SSHv2 packet types.  There is some overlap with SSHv1, but an annoying
   number of messages have the same name but different values.  Note also
   that the keyex (static DH keys), keyex_gex (ephemeral DH keys), and
   keyex_ecdh (static ECDH keys) message types overlap */

#define SSH_MSG_DISCONNECT		1	/* Disconnect session */
#define SSH_MSG_IGNORE			2	/* No-op */
#define SSH_MSG_DEBUG			4	/* No-op */
#define SSH_MSG_SERVICE_REQUEST 5	/* Request authentiction */
#define SSH_MSG_SERVICE_ACCEPT	6	/* Acknowledge request */
#define SSH_MSG_KEXINIT			20	/* Hello */
#define SSH_MSG_NEWKEYS			21	/* Change cipherspec */
#define SSH_MSG_KEXDH_INIT		30	/* DH, phase 1 */
#define SSH_MSG_KEXDH_REPLY		31	/* DH, phase 2 */
#define SSH_MSG_KEXDH_GEX_REQUEST_OLD 30 /* Ephem.DH key request */
#define SSH_MSG_KEXDH_GEX_GROUP 31	/* Ephem.DH key response */
#define SSH_MSG_KEXDH_GEX_INIT	32	/* Ephem.DH, phase 1 */
#define SSH_MSG_KEXDH_GEX_REPLY 33	/* Ephem.DH, phase 2 */
#define SSH_MSG_KEXDH_GEX_REQUEST_NEW 34 /* Ephem.DH key request */
#define SSH_MSG_KEX_ECDH_INIT	30	/* ECDH, phase 1 */
#define SSH_MSG_KEX_ECDH_REPLY	31	/* ECDH, phase 2 */
#define SSH_MSG_USERAUTH_REQUEST 50 /* Request authentication */
#define SSH_MSG_USERAUTH_FAILURE 51 /* Authentication failed */
#define SSH_MSG_USERAUTH_SUCCESS 52 /* Authentication succeeded */
#define SSH_MSG_USERAUTH_BANNER 53	/* No-op */
#define SSH_MSG_USERAUTH_INFO_REQUEST 60 /* Generic auth.svr.request */
#define SSH_MSG_USERAUTH_INFO_RESPONSE 61 /* Generic auth.cli.response */
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
   readHSPacketSSH() to handle situations where more than one return value is
   valid.  CMSG_USER can return failure meaning "no password" even if
   there's no actual failure, CMSG_AUTH_PASSWORD can return SMSG_FAILURE
   which indicates a wrong password used iff it's a response to the client
   sending a password, and MSG_USERAUTH_REQUEST can similarly return a
   failure or success response.

   In addition to these types there's an "any" type which is used during the
   setup negotiation which will accept any (non-error) packet type and return
   the type as the return code */

#define SSH1_MSG_SPECIAL_USEROPT	500	/* Value to handle SSHv1 user name */
#define SSH1_MSG_SPECIAL_PWOPT		501	/* Value to handle SSHv1 password */
#define SSH1_MSG_SPECIAL_RSAOPT		502	/* Value to handle SSHv1 RSA challenge */
#define SSH1_MSG_SPECIAL_ANY		503	/* Any SSHv1 packet type */

#define SSH_MSG_SPECIAL_FIRST		500	/* Boundary for _SPECIAL types */
#define SSH_MSG_SPECIAL_USERAUTH	501	/* Value to handle SSHv2 combined auth.*/
#define SSH_MSG_SPECIAL_USERAUTH_PAM 502 /* Value to handle SSHv2 PAM auth.*/
#define SSH_MSG_SPECIAL_CHANNEL		503	/* Value to handle channel open */
#define SSH_MSG_SPECIAL_REQUEST		504	/* Value to handle SSHv2 global/channel req.*/
#define SSH_MSG_SPECIAL_LAST		505	/* Last valid _SPECIAL type */

/* SSHv2 disconnection codes */

#define SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT		1
#define SSH_DISCONNECT_PROTOCOL_ERROR					2
#define SSH_DISCONNECT_KEY_EXCHANGE_FAILED				3
#define SSH_DISCONNECT_RESERVED							4
#define SSH_DISCONNECT_MAC_ERROR						5
#define SSH_DISCONNECT_COMPRESSION_ERROR				6
#define SSH_DISCONNECT_SERVICE_NOT_AVAILABLE			7
#define SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED	8
#define SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE			9
#define SSH_DISCONNECT_CONNECTION_LOST					10
#define SSH_DISCONNECT_BY_APPLICATION					11
#define SSH_DISCONNECT_TOO_MANY_CONNECTIONS				12
#define SSH_DISCONNECT_AUTH_CANCELLED_BY_USER			13
#define SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE	14
#define SSH_DISCONNECT_ILLEGAL_USER_NAME				15

/* SSHv2 channel open failure codes */

#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED			1
#define SSH_OPEN_CONNECT_FAILED							2
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE					3
#define SSH_OPEN_RESOURCE_SHORTAGE						4

/* SSHv2 requires the use of a number of additional (pseudo)-algorithm
   types that don't correspond to normal cryptlib algorithms.  To handle
   these, we define pseudo-algoID values that fall within the range of
   the normal algorithm ID types but that aren't normal algorithm IDs.

   The difference between CRYPT_PSEUDOALGO_DHE and CRYPT_PSEUDOALGO_DHE_ALT
   is that the former uses SHA-1 and the latter uses a hastily-kludged-
   on SHA-256 that was added shortly before the RFC was published, so
   that the PRF uses SHA-256 but all other portions of the protocol still
   use SHA-1.

   The ECC (pseudo-)algorithm types are even messier since they're not just 
   algorithm values but a combination of the algorithm, the key size, and 
   the hash algorithm, with CRYPT_ALGO_ECDH/CRYPT_ALGO_ECDSA being the 
   default P256 curve with SHA-256 and the others being different curve 
   types and hashes.  Because the curve types are tied to the oddball SHA-2 
   hash variants (we can't just use SHA-256 for every curve), we don't 
   support P384 and P512 because we'd have to support an entirely new (and 
   64-bit-only) hash algorithm for each of the curves.  Because of this the 
   values for P384/P512 are defined below, but disabled in the code */

#define CRYPT_PSEUDOALGO_DHE		( CRYPT_ALGO_LAST_CONVENTIONAL - 9 )
#define CRYPT_PSEUDOALGO_DHE_ALT	( CRYPT_ALGO_LAST_CONVENTIONAL - 8 )
#define CRYPT_PSEUDOALGO_ECDH_P384	( CRYPT_ALGO_LAST_CONVENTIONAL - 7 )
#define CRYPT_PSEUDOALGO_ECDH_P521	( CRYPT_ALGO_LAST_CONVENTIONAL - 6 )
#define CRYPT_PSEUDOALGO_ECDSA_P384	( CRYPT_ALGO_LAST_CONVENTIONAL - 5 )
#define CRYPT_PSEUDOALGO_ECDSA_P521	( CRYPT_ALGO_LAST_CONVENTIONAL - 4 )
#define CRYPT_PSEUDOALGO_COPR		( CRYPT_ALGO_LAST_CONVENTIONAL - 3 )
#define CRYPT_PSEUDOALGO_PASSWORD	( CRYPT_ALGO_LAST_CONVENTIONAL - 2 )
#define CRYPT_PSEUDOALGO_PAM		( CRYPT_ALGO_LAST_CONVENTIONAL - 1 )

/* The size of the encoded DH keyex value and the requested DHE key size, 
   which we have to store in encoded form so that we can hash them later in 
   the handshake */

#define MAX_ENCODED_KEYEXSIZE		( CRYPT_MAX_PKCSIZE + 16 ) 
#define ENCODED_REQKEYSIZE			( UINT_SIZE * 3 )

/* Check whether an algorithm ID is one of the above pseudo-algorithm
   types */

#define isPseudoAlgo( algorithm ) \
		( algorithm >= CRYPT_PSEUDOALGO_DHE && \
		  algorithm <= CRYPT_PSEUDOALGO_PAM )

/* When working with SSH channels there are a number of SSH-internal
   attributes that aren't exposed as cryptlib-wide attribute types.  The
   following values are used to access SSH-internal channel attributes */

typedef enum {
	SSH_ATTRIBUTE_NONE,						/* No channel attribute */
	SSH_ATTRIBUTE_ACTIVE,					/* Channel is active */
	SSH_ATTRIBUTE_WINDOWCOUNT,				/* Data window count */
	SSH_ATTRIBUTE_WINDOWSIZE,				/* Data window size */
	SSH_ATTRIBUTE_ALTCHANNELNO,				/* Secondary channel no. */
	SSH_ATRIBUTE_LAST						/* Last channel attribute */
	} SSH_ATTRIBUTE_TYPE;

/* Check whether a DH/ECDH value is valid for a given server key size.  The 
   check is slightly different for the ECC version because the value is
   a composite ECC point with two coordinates, so we have to divide the 
   length by two to get the size of a single coordinate */

#define isValidDHsize( value, serverKeySize, extraLength ) \
		( ( value ) > ( ( serverKeySize ) - 8 ) + ( extraLength ) && \
		  ( value ) < ( ( serverKeySize ) + 2 ) + ( extraLength ) )
#define isValidECDHsize( value, serverKeySize, extraLength ) \
		( ( value ) / 2 > ( ( serverKeySize ) - 8 ) + ( extraLength ) && \
		  ( value ) / 2 < ( ( serverKeySize ) + 2 ) + ( extraLength ) )

/****************************************************************************
*																			*
*								SSH Structures								*
*																			*
****************************************************************************/

/* Mapping of SSHv2 algorithm names to cryptlib algorithm IDs, in preferred
   algorithm order.  Some of the algorithms are pure algorithms while others
   are more like cipher suites, in order to check whether they're available
   for use we have to map the suite pseudo-value into one or more actual
   algorithms, which are given via the checkXXXAlgo values */

typedef struct {
	/* Mapping from algorithm name to cryptlib algorithm ID */
	BUFFER_FIXED( nameLen ) \
	const char FAR_BSS *name;				/* Algorithm name */
	const int nameLen;
	const CRYPT_ALGO_TYPE algo;				/* Algorithm ID */

	/* Optional parameters needed to check for algorithm availability
	   when the algorithm actually represents a cipher suite */
	const CRYPT_ALGO_TYPE checkCryptAlgo, checkHashAlgo;
	} ALGO_STRING_INFO;

/* SSH handshake state information.  This is passed around various
   subfunctions that handle individual parts of the handshake */

typedef struct SH {
	/* SSHv1 session state information/SSHv2 exchange hash */
	BUFFER_FIXED( SSH2_COOKIE_SIZE ) \
	BYTE cookie[ SSH2_COOKIE_SIZE + 8 ];	/* Anti-spoofing cookie */
	BUFFER_FIXED( CRYPT_MAX_HASHSIZE ) \
	BYTE sessionID[ CRYPT_MAX_HASHSIZE + 8 ];/* Session ID/exchange hash */
	int sessionIDlength;
	CRYPT_ALGO_TYPE exchangeHashAlgo;		/* Exchange hash algorithm */
	CRYPT_CONTEXT iExchangeHashContext, iExchangeHashAltContext;
											/* Hash of exchanged information */

	/* Information needed to compute the session ID.  SSHv1 requires the
	   host and server key modulus, SSHv2 requires the client and server 
	   DH/ECDH values (along with various other things, but these are hashed
	   inline).  The SSHv2 values are in MPI-encoded form so we need to
	   reserve a little extra room for the length and leading zero-padding.
	   Since the data fields are rather large and also disjoint, and since
	   SSHv1 is pretty much dead and therefore not worth going out of our 
	   way to accomodate) we alias one to the other to save space */
	BUFFER( MAX_ENCODED_KEYEXSIZE, clientKeyexValueLength ) \
	BYTE clientKeyexValue[ MAX_ENCODED_KEYEXSIZE + 8 ];
	BUFFER( MAX_ENCODED_KEYEXSIZE, serverKeyexValueLength ) \
	BYTE serverKeyexValue[ MAX_ENCODED_KEYEXSIZE + 8 ];
	int clientKeyexValueLength, serverKeyexValueLength;
	#define hostModulus				clientKeyexValue
	#define serverModulus			serverKeyexValue
	#define hostModulusLength		clientKeyexValueLength
	#define serverModulusLength		serverKeyexValueLength

	/* Encryption algorithm and key information */
	CRYPT_ALGO_TYPE pubkeyAlgo;				/* Host signature algo */
	BUFFER( CRYPT_MAX_PKCSIZE, secretValueLength ) \
	BYTE secretValue[ CRYPT_MAX_PKCSIZE + 8 ];	/* Shared secret value */
	int secretValueLength;

	/* Short-term server key (SSHv1) or DH/ECDH key agreement context 
	   (SSHv2), and the client requested DH key size for the SSHv2 key 
	   exchange.  Alongside the actual key size we also store the 
	   original encoded form, which has to be hashed as part of the exchange 
	   hash.  The long-term host key is stored as the session information 
	   iKeyexCryptContext for the client and privateKey for the server. 
	   Since ECDH doesn't just entail a new algorithm but an entire cipher
	   suite, we provide a flag to make checking for this easier */
	CRYPT_ALGO_TYPE keyexAlgo;				/* Keyex algo */
	CRYPT_CONTEXT iServerCryptContext;
	int serverKeySize, requestedServerKeySize;
	BUFFER( ENCODED_REQKEYSIZE, encodedReqKeySizesLength ) \
	BYTE encodedReqKeySizes[ ENCODED_REQKEYSIZE + 8 ];
	int encodedReqKeySizesLength;
	BOOLEAN isECDH;							/* Use of ECC cipher suite */

	/* Tables mapping SSHv2 algorithm names to cryptlib algorithm IDs.
	   These are declared once in ssh2.c and referred to here via pointers
	   to allow them to be static const, which is necessary in some
	   environments to get them into the read-only segment */
	const ALGO_STRING_INFO FAR_BSS *algoStringPubkeyTbl;
	int algoStringPubkeyTblNoEntries;

	/* Function pointers to handshaking functions.  These are set up as
	   required depending on whether the protocol being used is v1 or v2,
	   and the session is client or server */
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *beginHandshake )( INOUT SESSION_INFO *sessionInfoPtr,
							 INOUT struct SH *handshakeInfo );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *exchangeKeys )( INOUT SESSION_INFO *sessionInfoPtr,
						   INOUT struct SH *handshakeInfo );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *completeHandshake )( INOUT SESSION_INFO *sessionInfoPtr,
								INOUT struct SH *handshakeInfo );
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
							OUT_LENGTH_Z int *length, 
							IN_LENGTH const int position );

/* Prototypes for functions in ssh2.c */

CHECK_RETVAL \
int getAlgoStringInfo( OUT const ALGO_STRING_INFO **algoStringInfoPtrPtr,
					   OUT_INT_Z int *noInfoEntries );
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
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int processHelloSSH( INOUT SESSION_INFO *sessionInfoPtr,
					 INOUT SSH_HANDSHAKE_INFO *handshakeInfo, 
					 OUT_LENGTH_SHORT_Z int *keyexLength,
					 const BOOLEAN isServer );

/* Prototypes for functions in ssh2_authc.c/ssh2_auths.c  */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processClientAuth( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT SSH_HANDSHAKE_INFO *handshakeInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int processServerAuth( INOUT SESSION_INFO *sessionInfoPtr,
					   const BOOLEAN userInfoPresent );

/* Prototypes for functions in ssh2_chn.c */

typedef enum { CHANNEL_NONE, CHANNEL_READ, CHANNEL_WRITE,
			   CHANNEL_BOTH, CHANNEL_LAST } CHANNEL_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createChannel( INOUT SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int addChannel( INOUT SESSION_INFO *sessionInfoPtr, 
				IN const long channelNo,
				IN_LENGTH_MIN( 1024 ) const int maxPacketSize, 
				IN_BUFFER( typeLen ) const void *type,
				IN_LENGTH_SHORT const int typeLen, 
				IN_BUFFER_OPT( arg1Len ) const void *arg1, 
				IN_LENGTH_SHORT const int arg1Len );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteChannel( INOUT SESSION_INFO *sessionInfoPtr, 
				   IN const long channelNo,
				   IN_ENUM( CHANNEL ) const CHANNEL_TYPE channelType,
				   const BOOLEAN deleteLastChannel );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int selectChannel( INOUT SESSION_INFO *sessionInfoPtr, 
				   IN const long channelNo,
				   IN_ENUM_OPT( CHANNEL ) const CHANNEL_TYPE channelType );
CHECK_RETVAL_RANGE( 1, LONG_MAX ) STDC_NONNULL_ARG( ( 1 ) ) \
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
						  OUT_LENGTH_SHORT_Z int *dataLength );
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

CHECK_RETVAL_RANGE( 10000, MAX_WINDOW_SIZE ) STDC_NONNULL_ARG( ( 1 ) ) \
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
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initSecurityContextsSSH( INOUT SESSION_INFO *sessionInfoPtr );
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
				 IN_LENGTH const int dataMaxLength, 
				 IN_LENGTH_Z const int dataLength, 
				 IN_RANGE( 16, CRYPT_MAX_HASHSIZE ) const int macLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int checkMacSSHIncremental( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
							IN_INT const long seqNo,
							IN_BUFFER( dataMaxLength ) const BYTE *data, 
							IN_LENGTH const int dataMaxLength, 
							IN_LENGTH_Z const int dataLength, 
							IN_LENGTH const int packetDataLength, 
							IN_ENUM( MAC ) const MAC_TYPE macType, 
							IN_RANGE( 16, CRYPT_MAX_HASHSIZE ) const int macLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int createMacSSH( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
				  IN_INT const long seqNo,
				  IN_BUFFER( dataMaxLength ) BYTE *data, 
				  IN_LENGTH const int dataMaxLength, 
				  IN_LENGTH const int dataLength );

/* Prototypes for functions in ssh2_rd.c */

CHECK_RETVAL_PTR \
const char *getSSHPacketName( IN_RANGE( 0, 255 ) const int packetType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int readPacketHeaderSSH2( INOUT SESSION_INFO *sessionInfoPtr,
						  IN_RANGE( SSH_MSG_DISCONNECT, \
									SSH_MSG_SPECIAL_REQUEST ) \
								const int expectedType, 
						  OUT_LENGTH_Z long *packetLength,
						  OUT_LENGTH_Z int *packetExtraLength,
						  INOUT SSH_INFO *sshInfo,
						  INOUT_OPT READSTATE_INFO *readInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readHSPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
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
						   IN_LENGTH const int bufferSize, 
						   IN_RANGE( SSH_MSG_DISCONNECT, \
									 SSH_MSG_CHANNEL_FAILURE ) 
								const int packetType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int continuePacketStreamSSH( INOUT STREAM *stream, 
							 IN_RANGE( SSH_MSG_DISCONNECT, \
									   SSH_MSG_CHANNEL_FAILURE ) \
								const int packetType,
							 OUT_LENGTH_Z int *packetOffset );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int wrapPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT STREAM *stream,
					IN_LENGTH_Z const int offset, 
					const BOOLEAN useQuantisedPadding,
					const BOOLEAN isWriteableStream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int sendPacketSSH2( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT STREAM *stream,
					const BOOLEAN sendOnly );

/* Prototypes for session mapping functions */

void initSSH1processing( INOUT SESSION_INFO *sessionInfoPtr,
						 INOUT_OPT SSH_HANDSHAKE_INFO *handshakeInfo,
						 const BOOLEAN isServer ) \
						 STDC_NONNULL_ARG( ( 1 ) );
STDC_NONNULL_ARG( ( 1 ) ) \
void initSSH2processing( INOUT SESSION_INFO *sessionInfoPtr,
						 INOUT_OPT SSH_HANDSHAKE_INFO *handshakeInfo,
						 const BOOLEAN isServer );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void initSSH2clientProcessing( STDC_UNUSED SESSION_INFO *sessionInfoPtr,
							   INOUT SSH_HANDSHAKE_INFO *handshakeInfo );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void initSSH2serverProcessing( STDC_UNUSED SESSION_INFO *sessionInfoPtr,
							   INOUT SSH_HANDSHAKE_INFO *handshakeInfo );

#ifndef USE_SSH1
  #define initSSH1processing	initSSH2processing
#endif /* USE_SSH1 */
#endif /* _SSH_DEFINED */
