/****************************************************************************
*																			*
*						Secure Session Routines Header File					*
*						 Copyright Peter Gutmann 1998-2004					*
*																			*
****************************************************************************/

#ifndef _SES_DEFINED

#define _SES_DEFINED

#ifndef _STREAM_DEFINED
  #if defined( INC_ALL )
	#include "stream.h"
  #else
	#include "io/stream.h"
  #endif /* Compiler-specific includes */
#endif /* _STREAM_DEFINED */

/****************************************************************************
*																			*
*							Session Types and Constants						*
*																			*
****************************************************************************/

/* Session information flags.  These are:

	SESSION_ISOPEN: The session is active.

	SESSION_PARTIALOPEN: The session is partially active pending 
			confirmation of credentials such as a username and password
			or certificate.  This means that the session remains in the
			handshake state, with the handshake being completed once the 
			credentials have been confirmed.

	SESSION_SENDCLOSED: The remote system has closed its receive channel, 
			which means that no more data can be sent to it.  This does not 
			however mean that no more data can be received on our receive 
			channel.

	SESSION_ISCLOSINGDOWN: The session is in the shutdown stage, if further
			requests from the remote system arrive they should be NACK'd or
			ignored.

	SESSION_NOREPORTERROR: Don't update the extended error information if
			an error occurs, since this has already been set.  This is
			typically used when performing shutdown actions in response to
			a protocol error, when a network error such as the other side
			closing the connection would overwrite the details of the
			error that caused the shutdown to be performed.

	SESSION_ISSERVER: The session is a server session.

	SESSION_ISSECURE_READ:  The read/write channel is in the secure state, 
	SESSION_ISSECURE_WRITE: for secure data transport sessions.  In other
			words the session has passed the initial handshake stage and all 
			data is now being encrypted/MACd/whatever.

	SESSION_ISCRYPTLIB: The peer is also running cryptlib, which means that 
			we can apply cryptlib-specific optimistions and security 
			enhancements.

	SESSION_ISHTTPTRANSPORT: The session is using HTTP transport, for 
			request/response sessions */
	
#define SESSION_NONE				0x0000	/* No session flags */
#define SESSION_ISOPEN				0x0001	/* Session is active */
#define SESSION_PARTIALOPEN			0x0002	/* Session is partially active */
#define SESSION_SENDCLOSED			0x0004	/* Send channel is closed */
#define SESSION_ISCLOSINGDOWN		0x0008	/* Session is in process of shutdown */
#define SESSION_NOREPORTERROR		0x0010	/* Don't report network-level errors */
#define SESSION_ISSERVER			0x0020	/* Session is server session */
#define SESSION_ISSECURE_READ		0x0040	/* Session read ch.in secure state */
#define SESSION_ISSECURE_WRITE		0x0080	/* Session write ch.in secure state */
#define SESSION_ISCRYPTLIB			0x0100	/* Peer is running cryptlib */
#define SESSION_ISHTTPTRANSPORT		0x0200	/* Session using HTTP transport */

/* Needed-information flags used by protocol-specific handlers to indicate
   that the caller must set the given attributes in the session information
   before the session can be activated.  This allows it to be checked at the
   general cryptses.c level rather than at the per-protocol level.
   
   Some session types have private keys optional but if present they must 
   meet certain requirements, this is indicated by omitting the presence-
   check SESSION_NEEDS_PRIVATEKEY but specifying one or more of the 
   SESSION_NEEDS_PRIVKEYxxx options */

#define SESSION_NEEDS_USERID		0x0001	/* Must have userID */
#define SESSION_NEEDS_PASSWORD		0x0002	/* Must have password */
#define SESSION_NEEDS_PRIVATEKEY	0x0004	/* Must have private key */
#define SESSION_NEEDS_PRIVKEYCRYPT	0x0008	/* Priv.key must have certificate */
#define SESSION_NEEDS_PRIVKEYSIGN	0x0010	/* Priv.key must have sig.capabil.*/
#define SESSION_NEEDS_PRIVKEYCERT	0x0020	/* Priv.key must have crypt capabil.*/
#define SESSION_NEEDS_PRIVKEYCACERT	0x0040	/* Priv key must have CA certificate */
#define SESSION_NEEDS_KEYORPASSWORD	0x0080	/* PW can be used in place of privK */
#define SESSION_NEEDS_REQUEST		0x0100	/* Must have request obj.*/
#define SESSION_NEEDS_KEYSET		0x0200	/* Must have certificate keyset */
#define SESSION_NEEDS_CERTSTORE		0x0400	/* Keyset must be certificate store */

/* The minimum- and maximum-length fixed headers that we should see in 
   header-read code */

#define FIXED_HEADER_MIN			5		/* SSL header */
#define FIXED_HEADER_MAX			21		/* TLS 1.1+ header with explicit 
											   AES IV */

/* When reading packets for a secure session protocol, we need to 
   communicate read state information which is more complex than the usual 
   length or error code.  The following values modify the standard return
   value (either a positive or zero byte count or a negative error value) 
   with additional context-specific information */

typedef enum {
	READINFO_NONE,						/* No special handling */
	READINFO_HEADERPAYLOAD,				/* Header read got some payload data */
	READINFO_NOOP,						/* Packet was no-op, try again */
	READINFO_PARTIAL,					/* Partial packet, try again */
	READINFO_FATAL,						/* Treat errors as fatal */
	READINFO_LAST						/* Last possible read information */
	} READSTATE_INFO;

/* The control mechanism for authorisation in interactive sessions.  
   Normally when the server gets a user-authorisation request it'll return a 
   CRYPT_ENVELOPE_RESOURCE to tell the caller that they need to decide what 
   to do with the request.  If they set it to AUTHRESPONSE_SUCCESS, we allow 
   the client authorisation, if they set it to AUTHRESPONSE_FAILURE we 
   disallow it and the client gets another go at authorising themselves.  
   The default setting of AUTHRESPONSE_NONE means we ask the user for 
   instructions */

typedef enum {
	AUTHRESPONSE_NONE,					/* No authorisation response */
	AUTHRESPONSE_SUCCESS,				/* Allow authorisation */
	AUTHRESPONSE_FAILURE,				/* Disallow authorisation */
	AUTHRESPONSE_LAST					/* Last possible authorisation response */
	} AUTHRESPONSE_TYPE;

/****************************************************************************
*																			*
*								Session Structures							*
*																			*
****************************************************************************/

/* Protocol-specific information for each session */

typedef struct {
	/* Information required for all sessions: Whether this is a secure
	   session or request/response protocol, protocol-specific flags, the
	   default port for the protocol, flags for attributes required before
	   the session can be activated, the default protocol version and lowest
	   and highest allowed versions, and the transport-protocol client and 
	   server content-types */
	const BOOLEAN isReqResp;			/* Whether session is req/resp session */
	const int flags;					/* Protocol flags */
	const int port;						/* Default port */
	const int clientReqAttrFlags, serverReqAttrFlags; /* Required attributes */
	const int version, minVersion, maxVersion;/* Protocol version/subtype */

	/* Session type-specific information: The send and receive buffer size,
	   the alternative transport protocol for request/response sessions if
	   HTTP isn't being used, the minimum allowed size for the server's
	   private key */
	const int bufSize;					/* Send/receive buffer sizes */
	const int sendBufStartOfs;			/* Payload data start */
	const int maxPacketSize;			/* Maximum packet (payload data) size */
	} PROTOCOL_INFO;

/* A value to initialise the session type-specific buffer size values to
   default settings for request/response protocols */

#define BUFFER_SIZE_DEFAULT		0, 0, 0

/* Attribute flags.  These are:

	FLAG_COMPOSITE: Composite attribute containing sub-attribute data in the 
			{ value, valueLength } buffer.  The attribute cursor can be 
			moved within the attribute using the internal virtual cursor.
			
	FLAG_CURSORMOVED: The attribute (group) cursor has moved, so the virtual 
			cursor within the attribute needs to be reset the next time that 
			it's referenced.  This is used with composite attributes, whose 
			internal structure is opaque to the general session code.

	FLAG_ENCODEDVALUE: The attribute value is stored in cryptlib 
			XXXXX-XXXXX-... style encoding and needs to be converted to 
			binary form before use.

	FLAG_EPHEMERAL: The attribute is only valid for the current session 
			activation and is cleared between session re-activations.
		
	FLAG_MULTIVALUED: Multiple instances of the attribute are permitted.  
			This complements ATTR_FLAG_OVERWRITE in that instead of 
			overwriting the single existing instance, another instance is 
			created */

#define ATTR_FLAG_NONE			0x00	/* No attribute flag */
#define ATTR_FLAG_ENCODEDVALUE	0x01	/* Value uses XXX-XXX encoding */
#define ATTR_FLAG_MULTIVALUED	0x02	/* Multiple instances permitted */
#define ATTR_FLAG_COMPOSITE		0x04	/* Composite attribute */
#define ATTR_FLAG_CURSORMOVED	0x08	/* Attribute virtual cursor reset */
#define ATTR_FLAG_EPHEMERAL		0x10	/* Only valid for current sess.act.*/
#define ATTR_FLAG_MAX			0x1F	/* Maximum possible flag value */

/* The helper function used to access session subtype-specific internal
   attributes within an attribute list entry */

struct AL;	/* Forward declaration for attribute-list access function */

typedef CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 3 ) ) \
		int ( *ATTRACCESSFUNCTION )( INOUT struct AL *attributeListPtr,
									 IN_ENUM( ATTR ) const ATTR_TYPE attrGetType,
									 OUT_INT_Z int *value );

/* An attribute list used to store session-related attributes such as 
   user names, passwords, and public keys.  Since some of these can be
   composite attributes (with information stored in the { value, 
   valueLength } buffer), we implement a virtual cursor that points to the 
   currently-selected sub-attribute within the composite attribute */

typedef struct AL {
	/* Identification and other information for this attribute */
	CRYPT_ATTRIBUTE_TYPE groupID, attributeID;	/* Attribute group and type */
	ATTRACCESSFUNCTION accessFunction;	/* Internal attribute access fn.*/
	int flags;							/* Attribute data flags */

	/* The data payload for this attribute.  If it's numeric data such as 
	   a small integer or context, we store it in the intValue member.  If 
	   it's a string or composite attribute data, we store it in the 
	   variable-length buffer */
	long intValue;						/* Integer value for simple types */
	BUFFER_OPT_FIXED( valueLength ) \
	void *value;						/* Attribute value */
	int valueLength;					/* Attribute value length */

	/* The previous and next list element in the linked list of elements */
	struct AL *prev, *next;				/* Prev, next item in the list */

	/* Variable-length storage for the attribute data */
	DECLARE_VARSTRUCT_VARS;
	} ATTRIBUTE_LIST;

/* Deferred response information.  When we get a request, we may be in the 
   middle of assembling or sending a data packet, so the response has to be 
   deferred until after the data packet has been completed and sent.  The
   following structure is used to hold the response data until the send
   channel is clear */

#define SSH_MAX_RESPONSESIZE	16		/* 2 * channelNo + 2 * param */

typedef struct {
	int type;							/* Response type */
	BUFFER( SSH_MAX_RESPONSESIZE, dataLen ) \
	BYTE data[ SSH_MAX_RESPONSESIZE + 8 ];	/* Encoded response data */
	int dataLen;
	} SSH_RESPONSE_INFO;

/* The internal fields in a session that hold data for the various session
   types */

typedef struct {
	/* Session state information */
	int sessionCacheID;					/* Session cache ID for this session */
	int minVersion;						/* Minimum acceptable protocol version */
	int ivSize;							/* Explicit IV size for TLS 1.1+ */

	/* The incoming and outgoing packet sequence number, for detecting 
	   insertion/deletion attacks */
	long readSeqNo, writeSeqNo;

	/* The SSL MAC read/write secrets are required because SSL 3.0 uses a 
	   proto-HMAC that isn't handled by cryptlib.  We leave the data in 
	   normal memory because it's only usable for an active attack, which 
	   means that recovering it from swap afterwards isn't a problem */
	BUFFER_FIXED( CRYPT_MAX_HASHSIZE ) \
	BYTE macReadSecret[ CRYPT_MAX_HASHSIZE + 8 ];
	BUFFER_FIXED( CRYPT_MAX_HASHSIZE ) \
	BYTE macWriteSecret[ CRYPT_MAX_HASHSIZE + 8 ];

	/* TLS 1.2+ with GCM modes breaks the IV down into two parts, an 
	   explicit portion that's sent with every packet and an implicit
	   portion that's derived from the master secret.  To save space
	   we store this in the data block for the SSL 3.0 MAC read/write 
	   secrets */
	#define gcmReadSalt		macReadSecret
	#define gcmWriteSalt	macWriteSecret
	int gcmSaltSize;

	/* The session scoreboard, used for the SSL session cache */
	void *scoreboardInfoPtr;			/* Session scoreboard */

	/* A buffer for the SSL packet header, which is read out-of-band */
	BUFFER_FIXED( 8 + CRYPT_MAX_IVSIZE ) \
	BYTE headerBuffer[ 8 + CRYPT_MAX_IVSIZE + 8 ];
	} SSL_INFO;

typedef struct {
	/* The packet type and padding length, which are extracted from the 
	   packet header during header processing */
	int packetType, padLength;

	/* The incoming and outgoing packet sequence number, for detecting 
	   insertion/deletion attacks */
	long readSeqNo, writeSeqNo;

	/* Per-channel state information */
	int currReadChannel, currWriteChannel; /* Current active R/W channels */
	int nextChannelNo;					/* Next SSH channel no.to use */
	int channelIndex;					/* Current cryptlib unique channel ID */

	/* Deferred response data, used to enqueue responses when unwritten data 
	   remains in the send buffer */
	SSH_RESPONSE_INFO response;

	/* Whether an SSH user authentication packet has been read ready for the
	   server to act on */
	BOOLEAN authRead;

	/* A buffer for the SSH packet header, which we receive in encrypted 
	   form so that it needs to be decrypted, parts discarded, and the 
	   remainder copied into the main buffer as payload data (ugh) */
	BUFFER_FIXED( CRYPT_MAX_IVSIZE ) \
	BYTE headerBuffer[ CRYPT_MAX_IVSIZE + 8 ];

	/* To keep track of the partially-processed data we need to augment the
	   standard pendingPacket indicators with an additional value that 
	   tracks how much of the pending packet has already been processed as 
	   part of the header read */
	int partialPacketDataLength;		/* Length of data already processed */

	/* The SSH spec allows authentication to be performed in lots of little 
	   bits and pieces, which give an attacker lots of leeway to fiddle with
	   the credentials being submitted on different passes of the 
	   authentication to try and confuse the server.  To avoid this problem
	   we require that the userID and authentication method remain constant
	   over different iterations of authentication, which unfortunately 
	   means recording a pile of server-side authentication state */
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE authUserNameHash[ KEYID_SIZE + 8 ];	/* Hashed userID */
	int authType;						/* Authentication method */
	} SSH_INFO;

typedef struct {
	/* The message imprint (hash) algorithm and hash value */
	CRYPT_ALGO_TYPE imprintAlgo;
	BUFFER( CRYPT_MAX_HASHSIZE, imprintSize ) \
	BYTE imprint[ CRYPT_MAX_HASHSIZE ];
	int imprintSize;
	} TSP_INFO;

typedef struct {
	/* CMP request subtype, user information and protocol flags */
	int requestType;					/* CMP request subtype */
	CRYPT_CERTIFICATE userInfo;			/* PKI user information */
	int flags;							/* Protocol flags */

	/* The saved MAC context from a previous transaction (if any).  This is
	   saved across transactions in case the same user information is used
	   for subsequent transactions, see the comment in cmp.h for details */
	CRYPT_CONTEXT savedMacContext;		/* MAC context from prev.trans */
	} CMP_INFO;

typedef struct {
	/* SCEP protocol flags */
	int flags;							/* Protocol flags */
	} SCEP_INFO;

/* Defines to make access to the union fields less messy */

#define sessionSSH		sessionInfo.sshInfo
#define sessionSSL		sessionInfo.sslInfo
#define sessionTSP		sessionInfo.tspInfo
#define sessionCMP		sessionInfo.cmpInfo
#define sessionSCEP		sessionInfo.scepInfo

/* The structure that stores the information on a session */

typedef struct SI {
	/* Control and status information */
	CRYPT_SESSION_TYPE type;			/* Session type */
	const PROTOCOL_INFO *protocolInfo;	/* Session subtype information */
	int version;						/* Protocol version/subtype */
	CRYPT_ALGO_TYPE cryptAlgo;			/* Negotiated encryption algo */
	CRYPT_ALGO_TYPE integrityAlgo;		/* Negotiated integrity prot.algo */
	int flags, protocolFlags;			/* Session information, protocol-specific flags */
	AUTHRESPONSE_TYPE authResponse;		/* Response to user-auth request */

	/* Session type-specific information */
	union {
		SSL_INFO *sslInfo;
		SSH_INFO *sshInfo;
		TSP_INFO *tspInfo;
		CMP_INFO *cmpInfo;
		SCEP_INFO *scepInfo;
		} sessionInfo;

	/* When we add generic attributes to the session, we occasionally need to
	   perform protocol-specific checking of the attributes being added.  The
	   following values are used to tell the generic cryptses.c code which
	   checks need to be performed */
	int clientReqAttrFlags, serverReqAttrFlags; /* Required attributes */

	/* The overall session status.  If we run into a nonrecoverable error
	   (which for the encrypted session types means just about anything,
	   once we lose sync we're toast) we remember the status here so that
	   any further attempts to work with the session will return this
	   status.  Since an error on one side of the channel (e.g. bad data on
	   read) doesn't necessarily affect the operation of the other side, we
	   keep track of the two sides independantly, and only set the error
	   state for both sides for network-related errors.

	   In many cases there'll still be data in the internal buffer that the
	   user can read/write without triggering an error response so before we 
	   set the error state we set the pending error state and only move the
	   pending state into the current state once all data still present in
	   the buffer has been read */
	int readErrorState, writeErrorState;/* Current error state */
	int pendingReadErrorState, pendingWriteErrorState;
										/* Error state when buffer emptied */

	/* Data buffer information.  In protocols that consist of single
	   messages sent back and forth only the receive buffer is used for
	   sending and receiving data, this buffer is somewhat more flexible
	   since it's associated with extra variables for handling the current
	   position in the buffer (bufPos) vs.the total amount of data present
	   (bufEnd) */
	BUFFER( sendBufSize, sendBufPos ) \
	BYTE *sendBuffer;
	BUFFER_OPT( receiveBufSize, receiveBufEnd ) \
	BYTE *receiveBuffer;				/* Data buffer */
	int sendBufSize, receiveBufSize;	/* Total buffer size */
	int sendBufPos, receiveBufPos;		/* Current position in buffer */
	int sendBufStartOfs, receiveBufStartOfs; /* Space for header in buffer */
	int receiveBufEnd;					/* Total data in buffer */
	int maxPacketSize;					/* Maximum packet (payload data) size */

	/* When reading encrypted data packets we typically end up with a partial
	   packet in the read buffer that we can't process until the remainder
	   arrives, the following variables holds the eventual length of the
	   pending data packet and the amount of data remaining to be read */
	int pendingPacketLength;			/* Lending of pending data packet */
	int pendingPacketRemaining;			/* Bytes remaining to be read */

	/* Unlike payload data, the packet header can't be read in sections but
	   must be read completely since all of the header information needs to
	   be processed at once.  The following value is usually zero, if it's
	   nonzero it records how much of the header remains to be read */
	int partialHeaderRemaining;			/* Header bytes still to read */

	/* When sending data we can also end up with partially-processed packets
	   in the send buffer, but for sending we prevent further packets from
	   being added until the current one is flushed.  To handle this all we
	   need is a simple high-water-mark indicator that indicates the start 
	   position of any yet-to-be-written data */
	BOOLEAN partialWrite;				/* Unwritten data remains in buffer */
	int sendBufPartialBufPos;			/* Progress point of partial write */

	/* The session generally has various ephemeral contexts associated with
	   it, some short-term (e.g.public-key contexts used to establish the
	   session) and some long-term (e.g.encryption contexts used to perform
	   bulk data encryption).  These contexts are ephemeral ones that are
	   created as part of the session, long-term ones (e.g.signature keys
	   used for authentication) are held elsewhere */
	CRYPT_CONTEXT iKeyexCryptContext;	/* Key exchange encryption */
	CRYPT_CONTEXT iKeyexAuthContext;	/* Key exchange authentication */
	CRYPT_CONTEXT iCryptInContext, iCryptOutContext;
										/* In/outgoing data encryption */
	CRYPT_CONTEXT iAuthInContext, iAuthOutContext;
										/* In/outgoing auth/integrity */
	CRYPT_CERTIFICATE iCertRequest, iCertResponse;
										/* Certificate request/response */
	int cryptBlocksize, authBlocksize;	/* Block size of crypt, auth.algos */

	/* The private key, which is required to authenticate the client or 
	   server in some protocols */
	CRYPT_CONTEXT privateKey;			/* Authentication private key */

	/* Certificate store for certificate management protocols like OCSP and 
	   CMP and private-key keyset for PnP PKI protocols */
	CRYPT_KEYSET cryptKeyset;			/* Certificate store */
	CRYPT_HANDLE privKeyset;			/* Private-key keyset/device */

	/* Session-related attributes, a current-position cursor in the 
	   attribute list */
	ATTRIBUTE_LIST *attributeList, *attributeListCurrent;

	/* Network connection information.  The reason why the client and server
	   information require separate storage is that (on the server) we may 
	   be binding to a specific interface (requiring a server name) and we 
	   need to record where the remote system's connection is coming from 
	   (requiring a client name) */
	CRYPT_SESSION transportSession;		/* Transport mechanism */
	int networkSocket;					/* User-supplied network socket */
	int readTimeout, writeTimeout, connectTimeout;
										/* Connect and data xfer.timeouts */
	STREAM stream;						/* Network I/O stream */

	/* Low-level error information */
	ERROR_INFO errorInfo;

	/* Pointers to session access methods.  Stateful sessions use the read/
	   write functions, stateless ones use the transact function */
	STDC_NONNULL_ARG( ( 1 ) ) \
	void ( *shutdownFunction )( INOUT struct SI *sessionInfoPtr );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *connectFunction )( INOUT struct SI *sessionInfoPtr );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *getAttributeFunction )( INOUT struct SI *sessionInfoPtr, 
								   OUT void *data,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *setAttributeFunction )( INOUT struct SI *sessionInfoPtr, 
								   IN const void *data,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *checkAttributeFunction )( INOUT struct SI *sessionInfoPtr,
									 IN const void *data,
									 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *transactFunction )( INOUT struct SI *sessionInfoPtr );
	CHECK_RETVAL_LENGTH_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *readHeaderFunction )( INOUT struct SI *sessionInfoPtr,
								 INOUT_ENUM( READSTATE ) READSTATE_INFO *readInfo );
	CHECK_RETVAL_LENGTH_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *processBodyFunction )( INOUT struct SI *sessionInfoPtr,
								  INOUT_ENUM( READSTATE ) READSTATE_INFO *readInfo );
	CHECK_RETVAL_LENGTH_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *preparePacketFunction )( INOUT struct SI *sessionInfoPtr );

	/* Error information */
	CRYPT_ATTRIBUTE_TYPE errorLocus;/* Error locus */
	CRYPT_ERRTYPE_TYPE errorType;	/* Error type */

	/* The object's handle and the handle of the user who owns this object.
	   The former is used when sending messages to the object when only the
	   xxx_INFO is available, the latter is used to avoid having to fetch the
	   same information from the system object table */
	CRYPT_HANDLE objectHandle;
	CRYPT_USER ownerHandle;

	/* Variable-length storage for the type-specific data */
	DECLARE_VARSTRUCT_VARS;
	} SESSION_INFO;

/****************************************************************************
*																			*
*								Session Functions							*
*																			*
****************************************************************************/

/* Macros to make handling of error reporting on shutdown a bit more 
   obvious */

#define disableErrorReporting( sessionInfoPtr )	\
		( sessionInfoPtr )->flags |= SESSION_NOREPORTERROR
#define enableErrorReporting( sessionInfoPtr )	\
		( sessionInfoPtr )->flags &= ~SESSION_NOREPORTERROR

/* The SESSION_ISSERVER flag is checked so often that we define a macro to 
   handle it */

#define isServer( sessionInfoPtr ) \
		( sessionInfoPtr->flags & SESSION_ISSERVER )

/* Session attribute handling functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getSessionAttribute( INOUT SESSION_INFO *sessionInfoPtr,
						 OUT_INT_Z int *valuePtr, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getSessionAttributeS( INOUT SESSION_INFO *sessionInfoPtr,
						  INOUT MESSAGE_DATA *msgData, 
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setSessionAttribute( INOUT SESSION_INFO *sessionInfoPtr,
						 IN_INT_Z const int value, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setSessionAttributeS( INOUT SESSION_INFO *sessionInfoPtr,
						  IN_BUFFER( dataLength ) const void *data,
						  IN_LENGTH const int dataLength,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteSessionAttribute( INOUT SESSION_INFO *sessionInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );

/* Session-specific attribute management functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addSessionInfo( INOUT_PTR ATTRIBUTE_LIST **listHeadPtr,
					IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
					IN_INT_Z const int value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int addSessionInfoS( INOUT_PTR ATTRIBUTE_LIST **listHeadPtr,
					IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
					IN_BUFFER( dataLength ) const void *data, 
					IN_LENGTH_SHORT const int dataLength );

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int addSessionInfoEx( INOUT_PTR ATTRIBUTE_LIST **listHeadPtr,
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
					  IN_BUFFER( dataLength ) const void *data, 
					  IN_LENGTH_SHORT const int dataLength, 
					  IN_FLAGS( ATTR ) const int flags );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int addSessionInfoComposite( INOUT_PTR ATTRIBUTE_LIST **listHeadPtr,
							 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
							 const ATTRACCESSFUNCTION accessFunction, 
							 IN_BUFFER( dataLength ) const void *data, 
							 IN_LENGTH_SHORT const int dataLength,
							 IN_FLAGS( ATTR ) const int flags );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int updateSessionInfo( INOUT_PTR ATTRIBUTE_LIST **listHeadPtr,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
					   IN_BUFFER( dataLength ) const void *data, 
					   IN_LENGTH_SHORT const int dataLength,
					   IN_LENGTH_SHORT const int dataMaxLength, 
					   IN_FLAGS( ATTR ) const int flags );
CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
int getSessionAttributeCursor( IN_OPT ATTRIBUTE_LIST *attributeListHead,
							   IN_OPT ATTRIBUTE_LIST *attributeListCursor, 
							   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE sessionInfoType,
							   OUT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *valuePtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int setSessionAttributeCursor( IN_OPT const ATTRIBUTE_LIST *attributeListHead,
							   INOUT_PTR ATTRIBUTE_LIST **attributeListCursorPtr, 
							   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE sessionInfoType,
							   IN_RANGE( CRYPT_CURSOR_LAST, \
										 CRYPT_CURSOR_FIRST ) /* Values are -ve */
									const int position );
CHECK_RETVAL_PTR \
const ATTRIBUTE_LIST *findSessionInfo( IN_OPT const ATTRIBUTE_LIST *attributeListPtr,
									   IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE attributeID );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 3 ) ) \
const ATTRIBUTE_LIST *findSessionInfoEx( IN_OPT const ATTRIBUTE_LIST *attributeListPtr,
										 IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE attributeID,
										 IN_BUFFER( valueLength ) const void *value, 
										 IN_LENGTH_SHORT const int valueLength );
STDC_NONNULL_ARG( ( 1 ) ) \
void lockEphemeralAttributes( INOUT ATTRIBUTE_LIST *attributeListHead );
STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int deleteSessionInfo( INOUT ATTRIBUTE_LIST **attributeListHead,
					   INOUT ATTRIBUTE_LIST **attributeListCurrent,
					   INOUT ATTRIBUTE_LIST *attributeListPtr );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void deleteSessionInfoAll( INOUT ATTRIBUTE_LIST **attributeListHead,
						   INOUT ATTRIBUTE_LIST **attributeListCurrent );
CHECK_RETVAL_ENUM( CRYPT_ATTRIBUTE ) \
CRYPT_ATTRIBUTE_TYPE checkMissingInfo( IN_OPT const ATTRIBUTE_LIST *attributeListHead,
									   const BOOLEAN isServer );

/* Prototypes for functions in sess_rw.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readFixedHeaderAtomic( INOUT SESSION_INFO *sessionInfoPtr, 
						   OUT_BUFFER_FIXED( headerLength ) void *headerBuffer, 
						   IN_LENGTH_SHORT_MIN( FIXED_HEADER_MIN ) \
								const int headerLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readFixedHeader( INOUT SESSION_INFO *sessionInfoPtr, 
					 OUT_BUFFER_FIXED( headerLength ) void *headerBuffer, 
					 IN_LENGTH_SHORT_MIN( FIXED_HEADER_MIN ) \
							const int headerLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int getSessionData( INOUT SESSION_INFO *sessionInfoPtr, 
					OUT_BUFFER( dataMaxLength, *bytesCopied ) void *data, 
					IN_LENGTH const int dataMaxLength, 
					OUT_LENGTH_Z int *bytesCopied );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int putSessionData( INOUT SESSION_INFO *sessionInfoPtr, 
					IN_BUFFER_OPT( dataLength ) const void *data,
					IN_LENGTH_Z const int dataLength, 
					OUT_LENGTH_Z int *bytesCopied );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readPkiDatagram( INOUT SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writePkiDatagram( INOUT SESSION_INFO *sessionInfoPtr, 
					  IN_BUFFER_OPT( contentTypeLen ) \
							const char *contentType, 
					  IN_LENGTH_TEXT_Z const int contentTypeLen );

/* Prototypes for functions in session.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initSessionIO( INOUT SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initSessionNetConnectInfo( const SESSION_INFO *sessionInfoPtr,
							   OUT NET_CONNECT_INFO *connectInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkAttributesConsistent( INOUT SESSION_INFO *sessionInfoPtr,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int checkServerCertValid( const CRYPT_CERTIFICATE iServerCert,
						  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
						  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int activateSession( INOUT SESSION_INFO *sessionInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sendCloseNotification( INOUT SESSION_INFO *sessionInfoPtr,
						   IN_BUFFER_OPT( length ) const void *data, 
						   IN_LENGTH_SHORT_Z const int length );

/* Prototypes for session mapping functions */

#ifdef USE_CERTSTORE
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodCertstore( INOUT SESSION_INFO *sessionInfoPtr );
#else
  #define setAccessMethodCertstore( x )	CRYPT_ARGERROR_NUM1
#endif /* USE_CERTSTORE */
#ifdef USE_CMP
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodCMP( INOUT SESSION_INFO *sessionInfoPtr );
#else
  #define setAccessMethodCMP( x )	CRYPT_ARGERROR_NUM1
#endif /* USE_CMP */
#ifdef USE_RTCS
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodRTCS( INOUT SESSION_INFO *sessionInfoPtr );
#else
  #define setAccessMethodRTCS( x )	CRYPT_ARGERROR_NUM1
#endif /* USE_RTCS */
#ifdef USE_OCSP
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodOCSP( INOUT SESSION_INFO *sessionInfoPtr );
#else
  #define setAccessMethodOCSP( x )	CRYPT_ARGERROR_NUM1
#endif /* USE_OCSP */
#ifdef USE_SCEP
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodSCEP( INOUT SESSION_INFO *sessionInfoPtr );
#else
  #define setAccessMethodSCEP( x )	CRYPT_ARGERROR_NUM1
#endif /* USE_SCEP */
#if defined( USE_SSH ) || defined( USE_SSH1 )
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodSSH( INOUT SESSION_INFO *sessionInfoPtr );
#else
  #define setAccessMethodSSH( x )	CRYPT_ARGERROR_NUM1
#endif /* USE_SSH || USE_SSH1 */
#ifdef USE_SSL
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodSSL( INOUT SESSION_INFO *sessionInfoPtr );
#else
  #define setAccessMethodSSL( x )	CRYPT_ARGERROR_NUM1
#endif /* USE_SSL */
#ifdef USE_TSP
  CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
  int setAccessMethodTSP( INOUT SESSION_INFO *sessionInfoPtr );
#else
  #define setAccessMethodTSP( x )	CRYPT_ARGERROR_NUM1
#endif /* USE_TCP */
#endif /* _SES_DEFINED */
