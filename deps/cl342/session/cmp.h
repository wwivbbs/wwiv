/****************************************************************************
*																			*
*							CMP Definitions Header File						*
*						Copyright Peter Gutmann 1999-2011					*
*																			*
****************************************************************************/

#ifndef _CMP_DEFINED

#define _CMP_DEFINED

/****************************************************************************
*																			*
*							Implementation Notes							*
*																			*
****************************************************************************/

/* CMP requires a variety of authentication contexts, which are mapped to 
   session information contexts as follows:

			|	iAuthIn			|	iAuthOut
	--------+-------------------+-----------------------
	Client	|	CA cert			|	Client privKey or
			|					|		none if MAC
	Server	|	Client cert or	|	CA privKey
			|		none if MAC	|

   This is complicated somewhat by the fact the iAuthOut can be one of three 
   objects, either the client or server's private key if it's signature-
   capable, or a separate signing key on the client if it's requesting a
   certificate for an encryption-only key, or a MAC context if no shared
   public keys are available yet.  To avoid having to select the appropriate
   private-key object each time that it's used we maintain a reference to the
   appropriate one in the protocolInfo's authContext.  If we're using MAC
   authentication then we store it in the protocolInfo's iMacContext.  Since
   this is destroyed each time that the session is recycled we copy it to 
   and from the session state if required.  This is a little awkward but 
   keeping the sole copy in the session state is even worse because all of 
   the ephemeral MAC-related data is stored in the protocolInfo, and storing
   the MAC context with the session state would require schlepping it around
   alongside the protocolInfo to each location where it's needed.

   The CMP message header includes a large amount of ambiguous, confusing, 
   and redundant information, we remove all of the unnecessary junk that's 
   required by the CMP spec by only sending the fields that are actually 
   useful (malum est consilium quod mutari non potest - Syrus).  Fields that 
   are completely pointless or can't be provided (sender and recipient DN, 
   nonces) are omitted entirely, fields that remain static throughout an 
   exchange (user ID information) are only sent in the first message and are 
   assumed to be the same as for the previous message if absent.  The 
   general schema for message fields during various sample exchanges is:

			transID	protKeyID	protAlgo	-- generalInfo data --
			------------------------------------------------------

	ir:		transID	userID-user	mac+param	clibID
	ip:		transID				mac			clibID

	cr:		transID				sig			clibID	certID-user
	cp:		transID				sig			clibID	certID-CA

	ir:		transID	userID-user	mac+param	clibID
	ip:		transID				mac			clibID
	cr:		transID2			sig					certID-user
	cp:		transID2			sig					certID-CA

	genm:	transID	userID-user	mac+param	clibID
	genp:	transID				mac			clibID	certID-CA
	ir:		transID2			mac
	ip:		transID2			mac
	cr:		transID3			sig					certID-user
	cp:		transID3			sig

   The transID (= nonce) is sent in all messages and is constant during a
   single transaction but not across a sequence of transactions.  At the
   start of each new transaction the client generates a fresh transID and
   sends it to the server, verifying that the same value is returned in 
   the server's response.  On the first message read in a transaction the
   server records the client's transID and verifies it in subsequent 
   messages.
   
   The user ID (= protKeyID) is sent once, if absent it's assumed to be 
   "same as previous".  The user credentials (if MAC authentication is being 
   used) are tied to a MAC context which is maintained as part of the 
   protocol state information.  Since this is recycled on each transaction, 
   it's copied to the savedMacContext value in the session state in case 
   it's required in the following transaction.  This is necessary because 
   the creation of a MAC context is triggered by a new user ID being seen,
   if it's "same as previous" then no new MAC context will be created.

   (An alternative option would be to disable the user ID and MAC context
   cacheing, but the main case where MAC authentication is used is in 
   PnPPKI where a PKIBoot is always followed by an ir, so it saves CMP 
   messaging, certificate store lookup, and context setup overhead by 
   cacheing the credentials across transactions).
   
   The certificate ID and MAC parameters are also sent once and if absent 
   they're similarly assumed to be "same as previous" (in the case of the 
   MAC parameters we simply send the MAC OID with NULL parameters to 
   indicate no change).  The cryptlib ID, a magic value used to indicate 
   that the other side is running cryptlib, is sent in the first message 
   only.

   The sending of the CA certificate ID in the PKIBoot response even though 
   the response is MAC'd is necessary because we need this value to identify 
   which of the certificates in the CTL is the CA/RA certificate to be used 
   for further exchanges.  There are actually several ways in which we can 
   identify the certificate:

	1. PKIBoot response is a CTL, userID identifies the CA certificate.

		Issues: The userID is probably only meant to identify the 
		authenticator of the message (the spec is, as usual, unclear on 
		this), not a random certificate located elsewhere.

	2. PKIBoot response is a CTL, certID identifies the CA certificate.

		Issues: A less serious version of the above, we're overloading the 
		certID to some extent but since it doesn't affect CMP messages as a
		whole (as overloading the userID would) this is probably OK.	

	3. PKIBoot response is SignedData, signer is CA certificate.

		Issues: Mostly nitpicks, we should probably only be sending a pure 
		CTL rather than signed data, and the means of identifying the CA 
		certificate seems a bit clunky.  On one hand it provides POP of the 
		CA key at the PKIBoot stage, but on the other it requires a CA 
		signing operation for every PKIBoot exchange, which is both rather 
		heavyweight if clients use it in a DHCP-like manner every time they
		start up and a potential security issue if the CA signing key has to
		be available at the CMP server.  In addition it requires a general-
		purpose signature-capable CA key, which often isn't the case if it's 
		reserved specifically for certificate and CRL signing */

/****************************************************************************
*																			*
*								CMP Constants								*
*																			*
****************************************************************************/

/* CMP version and the default port for the "TCP transport" kludge */

#define CMP_VERSION				2		/* CMP version */

/* Various CMP constants.  Since the cryptlib MAC keys are high-entropy 
   machine-generated values we don't have to use as many hashing iterations 
   as we would for generic passwords which are often weak, easily-guessed 
   values */

#define CMP_NONCE_SIZE			16		/* Size of nonces */
#define CMP_PW_ITERATIONS_CLIB	500		/* PW hashing iter.for clib pws */
#define CMP_PW_ITERATIONS_OTHER	2000	/* PW hashing iter.for non-clib pws */
#define CMP_MAX_PW_ITERATIONS	10000	/* Max allowable iterations */

/* The CMP HTTP content type */

#define CMP_CONTENT_TYPE		"application/pkixcmp"
#define CMP_CONTENT_TYPE_LEN	19

/* The CMP spec never defines any key size for the CMP/Entrust MAC, but
   everyone seems to use 160 bits for this */

#define CMP_HMAC_KEYSIZE		20

/* CMP protocol-specific flags that augment the general session flags.  If
   we're running a PnP PKI session, we leave the client connection active 
   to allow for further transactions.  In order to minimise the mount of 
   junk in headers (see the comment in the implementation notes above) we 
   record when various identifiers have been sent and don't send them again 
   in subsequent messages */

#define CMP_PFLAG_NONE			0x00	/* No protocol-specific flags */
#define CMP_PFLAG_RETAINCONNECTION 0x01	/* Leave conn.open for further trans.*/
#define CMP_PFLAG_CLIBIDSENT	0x02	/* cryptlib ID sent */
#define CMP_PFLAG_USERIDSENT	0x04	/* User ID sent */
#define CMP_PFLAG_CERTIDSENT	0x08	/* Certificate ID sent */
#define CMP_PFLAG_MACINFOSENT	0x10	/* MAC parameters sent */
#define CMP_PFLAG_PNPPKI		0x20	/* Session is PnP PKI-capable */

/* Context-specific tags for the PKIHeader record */

enum { CTAG_PH_MESSAGETIME, CTAG_PH_PROTECTIONALGO, CTAG_PH_SENDERKID,
	   CTAG_PH_RECIPKID, CTAG_PH_TRANSACTIONID, CTAG_PH_SENDERNONCE,
	   CTAG_PH_RECIPNONCE, CTAG_PH_FREETEXT, CTAG_PH_GENERALINFO };

/* Context-specific tags for the PKIBody wrapper.  These are both context-
   specific tags and message types so there are also extra values to handle
   message-type indicators that don't correspond to tags.   At the moment
   this is just a read-any indicator used by the server as a wildcard to 
   indicate that it'll accept any message that's valid as an initial client
   message */

typedef enum { 
	CTAG_PB_IR, CMP_MESSAGE_NONE = CTAG_PB_IR, /* _IR = 0 = _NONE */
	CTAG_PB_IP, CTAG_PB_CR, CTAG_PB_CP, CTAG_PB_P10CR, CTAG_PB_POPDECC, 
	CTAG_PB_POPDECR, CTAG_PB_KUR, CTAG_PB_KUP, CTAG_PB_KRR, CTAG_PB_KRP, 
	CTAG_PB_RR, CTAG_PB_RP, CTAG_PB_CCR, CTAG_PB_CCP, CTAG_PB_CKUANN, 
	CTAG_PB_CANN, CTAG_PB_RANN, CTAG_PB_CRLANN, CTAG_PB_PKICONF, 
	CTAG_PB_NESTED, CTAG_PB_GENM, CTAG_PB_GENP, CTAG_PB_ERROR, 
	CTAG_PB_CERTCONF, 
	CTAG_PB_READ_ANY,	/* Special-case for match-any message */
	CTAG_PB_LAST, CMP_MESSAGE_LAST = CTAG_PB_LAST	/* Last possible type */
	} CMP_MESSAGE_TYPE;

/* Context-specific tags for the PKIMessage */

enum { CTAG_PM_PROTECTION, CTAG_PM_EXTRACERTS };

/* Context-specific tags for the CertifiedKeyPair in the PKIMessage */

enum { CTAG_CK_CERT, CTAG_CK_ENCRYPTEDCERT, CTAG_CK_NEWENCRYPTEDCERT };

/* Context-specific tags for the EncryptedValue in the CertifiedKeyPair */

enum { CTAG_EV_DUMMY1, CTAG_EV_CEKALGO, CTAG_EV_ENCCEK, CTAG_EV_DUMMY2,
	   CTAG_EV_DUMMY3 };

/* PKIStatus values */

enum { PKISTATUS_OK, PKISTATUS_OK_WITHINFO, PKISTATUS_REJECTED,
	   PKISTATUS_WAITING, PKISTATUS_REVOCATIONIMMINENT,
	   PKISTATUS_REVOCATION, PKISTATUS_KEYUPDATE };

/* PKIFailureInfo values */

#define CMPFAILINFO_NONE				0x00000000L
#define CMPFAILINFO_OK					0x00000000L
#define CMPFAILINFO_BADALG				0x00000001L
#define CMPFAILINFO_BADMESSAGECHECK		0x00000002L
#define CMPFAILINFO_BADREQUEST			0x00000004L
#define CMPFAILINFO_BADTIME				0x00000008L
#define CMPFAILINFO_BADCERTID			0x00000010L
#define CMPFAILINFO_BADDATAFORMAT		0x00000020L
#define CMPFAILINFO_WRONGAUTHORITY		0x00000040L
#define CMPFAILINFO_INCORRECTDATA		0x00000080L
#define CMPFAILINFO_MISSINGTIMESTAMP	0x00000100L
#define CMPFAILINFO_BADPOP				0x00000200L
#define CMPFAILINFO_CERTREVOKED			0x00000400L
#define CMPFAILINFO_CERTCONFIRMED		0x00000800L
#define CMPFAILINFO_WRONGINTEGRITY		0x00001000L
#define CMPFAILINFO_BADRECIPIENTNONCE	0x00002000L
#define CMPFAILINFO_TIMENOTAVAILABLE	0x00004000L
#define CMPFAILINFO_UNACCEPTEDPOLICY	0x00008000L
#define CMPFAILINFO_UNACCEPTEDEXTENSION	0x00010000L
#define CMPFAILINFO_ADDINFONOTAVAILABLE	0x00020000L
#define CMPFAILINFO_BADSENDERNONCE		0x00040000L
#define CMPFAILINFO_BADCERTTEMPLATE		0x00080000L
#define CMPFAILINFO_SIGNERNOTTRUSTED	0x00100000L
#define CMPFAILINFO_TRANSACTIONIDINUSE	0x00200000L
#define CMPFAILINFO_UNSUPPORTEDVERSION	0x00400000L
#define CMPFAILINFO_NOTAUTHORIZED		0x00800000L
#define CMPFAILINFO_SYSTEMUNAVAIL		0x01000000L
#define CMPFAILINFO_SYSTEMFAILURE		0x02000000L
#define CMPFAILINFO_DUPLICATECERTREQ	0x04000000L
#define CMPFAILINFO_LAST				0x08000000L

/* Flags for initialising the protocol state information */

#define CMP_INIT_FLAG_NONE			0x00
#define CMP_INIT_FLAG_USERID		0x01
#define CMP_INIT_FLAG_TRANSID		0x02
#define CMP_INIT_FLAG_MACINFO		0x04
#define CMP_INIT_FLAG_MACCTX		0x08
#define CMP_INIT_FLAG_ALL			( CMP_INIT_FLAG_USERID | \
									  CMP_INIT_FLAG_TRANSID | \
									  CMP_INIT_FLAG_MACINFO | \
									  CMP_INIT_FLAG_MACCTX )
#define CMP_INIT_FLAG_MAX			0x0F

/* The OID for the Entrust MAC */

#define OID_ENTRUST_MAC	MKOID( "\x06\x09\x2A\x86\x48\x86\xF6\x7D\x07\x42\x0D" )

/* When we're writing the payload of a CMP message, we use a shared function
   for most payload types because they're all pretty similar.  The following
   values distinguish between the message classes that can be handled by a
   single write function */

typedef enum {
	CMPBODY_NONE, CMPBODY_NORMAL, CMPBODY_CONFIRMATION, CMPBODY_ACK, 
	CMPBODY_GENMSG, CMPBODY_ERROR, CMPBODY_LAST
	} CMPBODY_TYPE;

/* CMP uses so many unnecessary EXPLICIT tags that we define a macro to
   make it easier to evaluate the encoded sizes of objects tagged in this
   manner */

#define objSize( length )	( ( int ) sizeofObject( length ) )

/* The following macro can be used to enable dumping of PDUs to disk.  As a
   safeguard, this only works in the Win32 debug version to prevent it from
   being accidentally enabled in any release version */

#if defined( __WIN32__ ) && !defined( NDEBUG )
  #define DEBUG_DUMP_CMP( type, level, sessionInfo ) \
		  debugDumpCMP( type, level, sessionInfo )

  STDC_NONNULL_ARG( ( 3 ) ) \
  void debugDumpCMP( IN_ENUM( CMP_MESSAGE ) const CMP_MESSAGE_TYPE type, 
					 IN_RANGE( 1, 4 ) const int phase,
					 const SESSION_INFO *sessionInfoPtr );
#else
  #define DEBUG_DUMP_CMP( type, level, sessionInfo )
#endif /* Win32 debug */

/****************************************************************************
*																			*
*								CMP Structures								*
*																			*
****************************************************************************/

/* CMP protocol state information.  This is passed around various
   subfunctions that handle individual parts of the protocol */

typedef struct {
	/* Session state information.  We record the operation being carried
	   out so that we can make decisions about message validity and contents 
	   when reading/writing fields, and whether the other side is a cryptlib 
	   implementation, which both allows us to work around some of the 
	   braindamage in CMP since we know that the other side will do the 
	   right thing and lets us include extra information required to avoid 
	   CMP shortcomings.  We also record whether we're the server or the 
	   client for use in situations where the overall session state isn't 
	   available, this is used for diagnostic purposes to allow the display
	   of more specific error messages */
	int operation;							/* genm/ir/cr/kur/rr */
	BOOLEAN isCryptlib;						/* Whether peer is cryptlib */
	BOOLEAN isServer;						/* Whether we're the server */

	/* Identification/state variable information.  The userID is either the
	   CA-supplied user ID value (for MAC'd messages) or the 
	   subjectKeyIdentifier (for signed messages).  The sender and recipient
	   nonces change roles each at message turnaround (even though this is
	   totally unnecessary), so as we go through the various portions of the
	   protocol the different nonces slowly shift through the two values.
	   In order to accommodate nonstandard implementations, we allow for
	   nonces that are slightly larger than the required size.
	   
	   When using a persistent connection, the user information can change 
	   over successive transactions.  If a new transaction arrives whose 
	   user ID or certID differs from the previous one, we set the 
	   userID/certID-changed flag to tell the higher-level code to update 
	   the information that it has stored.
	   
	   If we encounter an error when reading the peer's PKI header, we can't
	   easily send a CMP error response because the information that we need
	   to create the CMP message may not have been read properly.  To handle
	   this we set the headerRead flag if the header has been successfully
	   read and if not, bail out rather than trying to create a response
	   based on incomplete information */
	BUFFER( CRYPT_MAX_TEXTSIZE, userIDsize ) \
	BYTE userID[ CRYPT_MAX_TEXTSIZE + 8 ];	/* User ID */
	BUFFER( CRYPT_MAX_TEXTSIZE, transIDsize ) \
	BYTE transID[ CRYPT_MAX_HASHSIZE + 8 ];	/* Transaction nonce */
	BUFFER( CRYPT_MAX_TEXTSIZE, certIDsize ) \
	BYTE certID[ CRYPT_MAX_HASHSIZE + 8 ];	/* Sender certificate ID */
	BUFFER( CRYPT_MAX_TEXTSIZE, senderNonceSize ) \
	BYTE senderNonce[ CRYPT_MAX_HASHSIZE + 8 ];	/* Sender nonce */
	BUFFER( CRYPT_MAX_TEXTSIZE, recipNonceSize ) \
	BYTE recipNonce[ CRYPT_MAX_HASHSIZE + 8 ];	/* Recipient nonce */
	int userIDsize, transIDsize, certIDsize, senderNonceSize, recipNonceSize;
	BOOLEAN userIDchanged, certIDchanged;	/* Whether ID information same as prev.*/
	BOOLEAN headerRead;						/* Whether header read successfully */

	/* Usually the key that we're getting a certificate for is signature-
	   capable but sometimes we need to certify an encryption-only key.  In 
	   this case we can't use the private key that created the request to 
	   authenticate it but have to use a separate signing key, typically one 
	   that we had certified before getting the encryption-only key certified.  
	   To keep things simple we keep a reference to whichever object is 
	   being used for authentication to avoid having to explicitly select it
	   each time that it's used */
	BOOLEAN cryptOnlyKey;					/* Whether key being cert'd is encr-only */
	CRYPT_CONTEXT authContext;

	/* When processing CMP data, we need to remember the last cryptlib error
	   status value we encountered and the last CMP extended failure value so
	   that we can send it to the remote client/server in an error response */
	int status;								/* Last error status */
	long pkiFailInfo;						/* Last extended failure status */

	/* The information needed to verify message integrity.  Typically we
	   use a MAC, however in some cases the data isn't MAC'd but signed by
	   the user or CA, in which case we use the user private key to sign or
	   CA certificate to verify instead of MAC'ing it.  If we're signing,
	   we clear the useMACsend flag, if we're verifying we clear the
	   useMACreceive flag (in theory the two should match, but there are
	   implementations that use MACs one way and sigs the other).
	   
	   In addition to the standard MAC password there's also an alternative
	   MAC password that may be used to authenticated revocations.  Since 
	   this doesn't fit into the conventional username+password model, we 
	   keep a copy here in case it's needed to process a MAC'd revocation
	   request */
	CRYPT_ALGO_TYPE hashAlgo;				/* Hash algo for signature */
	int hashParam;							/* Optional parameter for hash algo.*/
	CRYPT_CONTEXT iMacContext;				/* MAC context */
	BUFFER( CRYPT_MAX_HASHSIZE, saltSize ) \
	BYTE salt[ CRYPT_MAX_HASHSIZE + 8 ];	/* MAC password salt  */
	int saltSize;
	int iterations;							/* MAC password iterations */
	BYTE altMacKey[ CRYPT_MAX_HASHSIZE + 8 ];
	int altMacKeySize;						/* Alt.MAC key for revocations */
	BOOLEAN useMACsend, useMACreceive;		/* Use MAC to verify integrity */

	/* Whether the certificate issue is being authorised by an RA user 
	   rather than a standard user */
	BOOLEAN userIsRA;

	/* Other protocol information.  CMP uses an extremely clunky 
	   confirmation mechanism in which a certificate conf uses as its hash 
	   algorithm the algorithm that was used in a previous message by the CA 
	   to sign the certificate, which means implementations will break each 
	   time a new certificate format or CA hash algorithm is added since the 
	   CMP transport-level code is now dependent on the format of the data 
	   that it carries.  In order to support this content-coupling of 
	   protocol and data we record the hash algorithm when we receive the 
	   CA's reply so that it can be used later */
	CRYPT_ALGO_TYPE confHashAlgo;			/* Certificate conf.hash algo */

	/* Pointers to parsed data in the current message, required to work 
	   around CMP design flaws.  This is used by lower-level decoding 
	   routines to return information needed by higher-level ones.  The MAC 
	   information position records the position of the MAC information (we 
	   can't set up the MAC information until we've read the sender key ID, 
	   but the MAC information is sent first, so we have to go back and 
	   re-process it once we've got the sender key ID).  The sender DN 
	   pointer records the DN of the key used to sign the message if we're 
	   not talking to a cryptlib peer (the DN is ambiguous and can't 
	   properly identify the sender, so we only use it if there's no 
	   alternative) */
	int macInfoPos;							/* Position of MAC information in stream */
	BUFFER_OPT_FIXED( senderDNlength ) \
	void *senderDNPtr;
	int senderDNlength;						/* Position of auth.key ID in stream */
	} CMP_PROTOCOL_INFO;

/****************************************************************************
*																			*
*								CMP Functions								*
*																			*
****************************************************************************/

/* CMP message read/write methods for the different message types */

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
		int ( *READMESSAGE_FUNCTION )( INOUT STREAM *stream, 
									   INOUT SESSION_INFO *sessionInfoPtr,
									   INOUT CMP_PROTOCOL_INFO *protocolInfo,
									   IN_ENUM_OPT( CMP_MESSAGE ) \
											const CMP_MESSAGE_TYPE messageType,
									   IN_LENGTH_SHORT const int messageLength );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
		int ( *WRITEMESSAGE_FUNCTION )( INOUT STREAM *stream, 
										INOUT SESSION_INFO *sessionInfoPtr,
										const CMP_PROTOCOL_INFO *protocolInfo );

CHECK_RETVAL_PTR \
READMESSAGE_FUNCTION getMessageReadFunction( IN_ENUM_OPT( CMP_MESSAGE ) \
									const CMP_MESSAGE_TYPE messageType );
CHECK_RETVAL_PTR \
WRITEMESSAGE_FUNCTION getMessageWriteFunction( IN_ENUM( CMPBODY ) \
									const CMPBODY_TYPE bodyType,
									const BOOLEAN isServer );

/* Prototypes for functions in cmp.c */

CHECK_RETVAL_RANGE( CMP_MESSAGE_NONE, CMP_MESSAGE_LAST - 1 ) \
int reqToResp( IN_ENUM_OPT( CMP_MESSAGE ) const CMP_MESSAGE_TYPE reqType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setCMPprotocolInfo( INOUT CMP_PROTOCOL_INFO *protocolInfo, 
						IN_BUFFER_OPT( userIDlength ) const void *userID, 
						IN_LENGTH_SHORT_Z const int userIDlength, 
						IN_FLAGS_Z( CMP_INIT ) const int flags,
						const BOOLEAN isCryptlib );
STDC_NONNULL_ARG( ( 1 ) ) \
void initCMPprotocolInfo( OUT CMP_PROTOCOL_INFO *protocolInfo, 
						  const BOOLEAN isCryptlib,
						  const BOOLEAN isServer );
STDC_NONNULL_ARG( ( 1 ) ) \
void destroyCMPprotocolInfo( INOUT CMP_PROTOCOL_INFO *protocolInfo );

/* Prototypes for functions in cmp_cli/cmp_svr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initServerAuthentMAC( INOUT SESSION_INFO *sessionInfoPtr, 
						  INOUT CMP_PROTOCOL_INFO *protocolInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initServerAuthentSign( INOUT SESSION_INFO *sessionInfoPtr, 
						   INOUT CMP_PROTOCOL_INFO *protocolInfo );
STDC_NONNULL_ARG( ( 1 ) ) \
void initCMPserverProcessing( SESSION_INFO *sessionInfoPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void initCMPclientProcessing( SESSION_INFO *sessionInfoPtr );

/* Prototypes for functions in cmp_cry.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int hashMessageContents( IN_HANDLE const CRYPT_CONTEXT iHashContext,
						 IN_BUFFER( length ) const void *data, 
						 IN_LENGTH_SHORT const int length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4 ) ) \
int initMacInfo( IN_HANDLE const CRYPT_CONTEXT iMacContext, 
				 IN_BUFFER( passwordLength ) const void *password, 
				 IN_LENGTH_SHORT const int passwordLength, 
				 IN_BUFFER( saltLength ) const void *salt, 
				 IN_LENGTH_SHORT const int saltLength, 
				 IN_RANGE( 1, CMP_MAX_PW_ITERATIONS ) const int iterations );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int readMacInfo( INOUT STREAM *stream, 
				 INOUT CMP_PROTOCOL_INFO *protocolInfo,
				 IN_BUFFER( passwordLength ) const void *password, 
				 IN_LENGTH_SHORT const int passwordLength,
				 INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeMacInfo( INOUT STREAM *stream,
				  const CMP_PROTOCOL_INFO *protocolInfo,
				  const BOOLEAN sendFullInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int checkMessageMAC( INOUT STREAM *stream, 
					 INOUT CMP_PROTOCOL_INFO *protocolInfo,
					 IN_BUFFER( messageLength ) const void *message,
					 IN_DATALENGTH const int messageLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int checkMessageSignature( INOUT CMP_PROTOCOL_INFO *protocolInfo,
						   IN_BUFFER( messageLength ) const void *message,
						   IN_DATALENGTH const int messageLength,
						   IN_BUFFER( signatureLength ) const void *signature,
						   IN_LENGTH_SHORT const int signatureLength,
						   IN_HANDLE const CRYPT_HANDLE iAuthContext );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4, 6 ) ) \
int writeMacProtinfo( IN_HANDLE const CRYPT_CONTEXT iMacContext,
					  IN_BUFFER( messageLength ) const void *message, 
					  IN_LENGTH_SHORT const int messageLength,
					  OUT_BUFFER( protInfoMaxLength, *protInfoLength ) \
							void *protInfo, 
					  IN_LENGTH_SHORT_MIN( 16 ) const int protInfoMaxLength,
					  OUT_LENGTH_BOUNDED_Z( protInfoMaxLength ) \
							int *protInfoLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 4, 6, 8 ) ) \
int writeSignedProtinfo( IN_HANDLE const CRYPT_CONTEXT iSignContext,
						 IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
						 IN_RANGE( 0, 999 ) const int hashParam,
						 IN_BUFFER( messageLength ) const void *message, 
						 IN_LENGTH_SHORT const int messageLength,
						 OUT_BUFFER( protInfoMaxLength, *protInfoLength ) \
								void *protInfo, 
						  IN_LENGTH_SHORT_MIN( 32 ) const int protInfoMaxLength,
						  OUT_LENGTH_BOUNDED_Z( protInfoMaxLength ) \
								int *protInfoLength );

/* Prototypes for functions in cmp_err.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readPkiStatusInfo( INOUT STREAM *stream, 
					   const BOOLEAN isServer,
					   INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL_LENGTH_SHORT_NOERROR \
int sizeofPkiStatusInfo( IN_STATUS const int pkiStatus,
						 IN_ENUM_OPT( CMPFAILINFO ) const long pkiFailureInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writePkiStatusInfo( INOUT STREAM *stream, 
						IN_STATUS const int pkiStatus,
						IN_ENUM_OPT( CMPFAILINFO ) const long pkiFailureInfo );

/* Prototypes for functions in cmp_rd.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readPkiMessage( INOUT SESSION_INFO *sessionInfoPtr,
					INOUT CMP_PROTOCOL_INFO *protocolInfo,
					IN_ENUM_OPT( CMP_MESSAGE ) CMP_MESSAGE_TYPE messageType );

/* Prototypes for functions in cmp_wr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writePkiMessage( INOUT SESSION_INFO *sessionInfoPtr,
					 INOUT CMP_PROTOCOL_INFO *protocolInfo,
					 IN_ENUM( CMPBODY ) const CMPBODY_TYPE bodyType );

/* Prototypes for functions in pnppki.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pnpPkiSession( INOUT SESSION_INFO *sessionInfoPtr );

#endif /* _CMP_DEFINED */
