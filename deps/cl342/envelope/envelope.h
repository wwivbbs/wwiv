/****************************************************************************
*																			*
*						 Enveloping Routines Header File					*
*						Copyright Peter Gutmann 1996-2011					*
*																			*
****************************************************************************/

#ifndef _ENV_DEFINED

#define _ENV_DEFINED

#ifndef _STREAM_DEFINED
  #if defined( INC_ALL )
	#include "stream.h"
  #else
	#include "io/stream.h"
  #endif /* Compiler-specific includes */
#endif /* _STREAM_DEFINED */
#ifdef USE_COMPRESSION
  #if defined( INC_ALL )
	#include "zlib.h"
  #else
	#include "zlib/zlib.h"
  #endif /* Compiler-specific includes */
#endif /* USE_COMPRESSION */

/****************************************************************************
*																			*
*								Envelope Actions							*
*																			*
****************************************************************************/

/* Types of actions that can be performed on a piece of envelope data.  The 
   two key exchange actions are handled identically, but are given different 
   tags because we place PKC-based key exchange actions (which may be 
   handled automatically on de-enveloping) before conventional key exchange 
   actions (which usually require manual intervention for passphrases).  For 
   this reason the actions are given in their sort order (i.e. 
   ACTION_KEYEXCHANGE_PKC precedes ACTION_KEYEXCHANGE in the action list).
   The same occurs for the crypto actions ACTION_xxx, ACTION_CRYPT, and
   ACTION_MAC, which when used for authenticated encryption are applied in
   that order */

typedef enum {
	ACTION_NONE,					/* Non-action */

	/* Pre-actions */
	ACTION_KEYEXCHANGE_PKC,			/* Generate/read PKC exchange information */
	ACTION_KEYEXCHANGE,				/* Generate/read key exchange information */

	/* Actions */
	ACTION_xxx,						/* Generic-secret XXX action */
	ACTION_CRYPT,					/* En/decrypt */
	ACTION_MAC,						/* MAC */
	ACTION_COMPRESS,				/* Compress */
	ACTION_HASH,					/* Hash */

	/* Post-actions */
	ACTION_SIGN,					/* Generate/check signature */

	ACTION_LAST						/* Last valid action type */
	} ACTION_TYPE;

/* An 'action list' that defines what we need to do to the content when
   enveloping data.  There are three action lists, one for actions to perform
   before enveloping data, one to perform during enveloping (which is 
   actually just a single action rather than a list), and one to perform 
   after enveloping.  ACTION_KEYEXCHANGE and ACTION_KEYEXCHANGE_PKC are found 
   in the pre-enveloping list, ACTION_SIGN in the post-enveloping list, and 
   everything else in the during-enveloping list.

   Some actions are many-to-one, in which a number of controlling actions in
   one list may act on a single subject action in another list (for example a
   number of signature actions may sign the output from a single hash
   action).  This is handled by having the controlling actions maintain
   pointers to the subject action, for example a number of key export actions
   would point to one encryption action, with the export actions exporting
   the session key for the encryption action.

   The flags are:

	ACTION_ADDEDAUTOMATICALLY: Whether a subject action was added 
			automatically and invisibly to the caller as a result of adding 
			a controlling action, for example a hash action created when a 
			signing action is added.  This is to ensure that we don't return 
			an error the first time the caller adds an action which is 
			identical to an automatically added action.

	ACTION_HASHCOMPLETE: Whether the hashing for a hash action has already
			been completed and we don't need to do it ourselves.  If we're 
			using a detached signature then the hash value will be supplied
			externally and there's no need to complete the hashing.
   
	ACTION_NEEDSCONTROLLER: Whether this is a subject action that still 
			requires a controlling action.  This allows us to identify 
			unused subject actions more easily than by scanning all 
			controller->subject relationships */

#define ACTION_NEEDSCONTROLLER	0x01	/* Whether action reqs.controller */
#define ACTION_ADDEDAUTOMATICALLY 0x02	/* Whether action added automat.*/
#define ACTION_HASHCOMPLETE		0x04	/* Whether hash is complete */

typedef struct AI {
	/* Control and status information */
	ACTION_TYPE action;				/* Type of action to perform */
	int flags;						/* Action flags */
	struct AI *next;				/* Next item in the list */

	/* The controlling/subject action.  This points to the subject action 
	   associated with a controlling action if this is a controlling 
	   action */
	struct AI *associatedAction;	/* Associated action */

	/* Information related to the action.  These fields contain various
	   pieces of information required as part of the action.  The crypt
	   handle contains the encryption context needed to perform the action 
	   (e.g. encryption, hashing, signing).  If we're generating CMS 
	   signatures, there may be extra attribute data present which is 
	   included in the signature, and we may also have countersignature 
	   information present, typically a timestamping session object */
	CRYPT_CONTEXT iCryptHandle;		/* Encryption handle for action */
	CRYPT_CERTIFICATE iExtraData;	/* Extra attribute data for CMS sigs.*/
	CRYPT_SESSION iTspSession;		/* Timestamping session object */
	int encodedSize;				/* The encoded size of the action */
	} ACTION_LIST;

/* Result codes for the checkAction() function when adding an action to
   an action list.  The two 'action present' results are for the case where 
   the action is already present and shouldn't be added again, and where the 
   action is present from being added as an (invisible to the user) side-
   effect of another action being added, so that this attempt to add it 
   should be reported as CRYPT_OK rather than CRYPT_INITED */

typedef enum {
	ACTION_RESULT_OK,				/* Action not present, can be added */
	ACTION_RESULT_EMPTY,			/* Action list is empty */
	ACTION_RESULT_INITED,			/* Action present (CRYPT_ERROR_INITED) */
	ACTION_RESULT_PRESENT,			/* Action present (CRYPT_OK) */
	ACTION_RESULT_ERROR,			/* Arg.error (CRYPT_ARGERROR_NUM1) */
	ACTION_RESULT_LAST				/* Last valid action result type */
	} ACTION_RESULT;

/* Envelope content information types */

typedef enum {
	CONTENT_NONE,					/* No content type */
	CONTENT_CRYPT,					/* Encrypted content info */
	CONTENT_AUTHENC,				/* Auth-enc content info */
	CONTENT_SIGNATURE,				/* Signed content info */
	CONTENT_LAST					/* Last valid content type */
	} CONTENT_TYPE;

/* Content information flags.  These are:

	CONTENTLIST_PROCESSED: The signature object has been processed by having
			the signature verified (or at least attempted to be verified).
			The verification result is stored/cached with the content info 
			for later use.

	CONTENTLIST_EXTERNALKEY: The signature-check key was supplied by the user
			rather than being instantiated internally from certificates
			associated with the signature */

#define CONTENTLIST_PROCESSED	0x01	/* Whether object has been processed */
#define CONTENTLIST_EXTERNALKEY	0x02	/* Whether key was added externally */

/* A 'content list' which is used to store objects found in the non-data
   portion of the envelope until we can do something with them when de-
   enveloping data */

#define clEncrInfo		contentInfo.contentEncrInfo
#define clSigInfo		contentInfo.contentSigInfo
#define clAuthEncInfo	contentInfo.contentAuthEncInfo

typedef struct {
	/* Signature algorithm/key information */
	CRYPT_ALGO_TYPE hashAlgo;		/* Hash algo.for signed data */
	int hashAlgoParam;				/* Optional algorithm parameter */
	CRYPT_HANDLE iSigCheckKey;		/* Signature check key */

	/* Authenticated/unauthenticated attribute information */
	CRYPT_CERTIFICATE iExtraData;	/* Authent.attrib.in CMS signatures */
	BUFFER_OPT_FIXED( extraDataLength ) \
	const void *extraData;			/* Authent.attrib.in PGP signatures */
	int extraDataLength;
	CRYPT_ENVELOPE iTimestamp;		/* Unauth.attrib.in CMS signatures */
	BUFFER_OPT_FIXED( extraData2Length ) \
	const void *extraData2;			/* Unauthenticated attributes */
	int extraData2Length;

	/* We only need to process a signed object once, once we've done this we 
	   store the processing result so that any further attempts to process 
	   the object will return the previously obtained result (an object can 
	   be processed multiple times if the user wanders up and down the 
	   content list using the cursor management capabilities) */
	int processingResult;			/* Result of processing */
	
	/* To allow positioning of the cursor within this item (== attribute 
	   group), we have to keep track of the virtual position within the
	   group.  The virtual attribute order is result -> key -> auth.
	   attr -> timestamp */
	CRYPT_ATTRIBUTE_TYPE attributeCursorEntry;
	} CONTENT_SIG_INFO;

typedef struct {
	/* Encryption algorithm/key information */
	CRYPT_ALGO_TYPE cryptAlgo;		/* Encryption algo.for this object */
	CRYPT_MODE_TYPE cryptMode;		/* Encrytion mode for this object */

	/* Encryption key setup information */
	BUFFER( CRYPT_MAX_HASHSIZE, saltOrIVsize ) \
	BYTE saltOrIV[ CRYPT_MAX_HASHSIZE + 8 ];/* Salt for password-derived key or */
	int saltOrIVsize;				/*	   IV for session encr.context */
	CRYPT_ALGO_TYPE keySetupAlgo;	/* Hash algo.for pw-derived key */
	int keySetupIterations;			/* Iterations for pw-derived key */
	int keySize;					/* Key size (if not implicit) */
	} CONTENT_ENCR_INFO;	

typedef struct {
	/* Authenticated-encryption algorithm information */
	CRYPT_ALGO_TYPE authEncAlgo;	/* AuthEnc algo.for this object */

	/* Authenticated encryption algorithm parameter data */
	BUFFER( 128, authEncParamLength ) \
	BYTE authEncParamData[ 128 + 8 ];
	int authEncParamLength;			/* AuthEnc parameter data */
	BUFFER_FIXED( kdfDataLength ) \
	void *kdfData;
	int kdfDataLength;				/* Opt.KDF algorithm params */
	BUFFER_FIXED( encParamDataLength ) \
	void *encParamData;
	int encParamDataLength;			/* Encryption algorithm params */
	BUFFER_FIXED( macParamDataLength ) \
	void *macParamData;
	int macParamDataLength;			/* MAC algorithm params */
	} CONTENT_AUTHENC_INFO;

typedef struct CL {
	/* Control and status information */
	CONTENT_TYPE type;				/* Content type enc/authenc/sig */
	CRYPT_ATTRIBUTE_TYPE envInfo;	/* Env.info required to continue */
	CRYPT_FORMAT_TYPE formatType;	/* Data format */
	int flags;						/* Item flags */
	struct CL *prev, *next;			/* Prev, next items in the list */

	/* The object contained in this list element.  All object type-specific
	   pointers in the xxxInfo data point into fields inside this data */
	BUFFER_FIXED( objectSize ) \
	const void *object;				/* The object data */
	int objectSize;					/* Size of the object */

	/* Details on the object.  Here we store whatever is required to process
	   the object without having to call queryXXXObject() for the details */
	BUFFER( CRYPT_MAX_HASHSIZE, keyIDsize ) \
	BYTE keyID[ CRYPT_MAX_HASHSIZE + 8 ];/* cryptlib key ID */
	int keyIDsize;
	BUFFER_OPT_FIXED( issuerAndSerialNumberSize ) \
	const void *issuerAndSerialNumber;/* CMS key ID */
	int issuerAndSerialNumberSize;
	BUFFER_OPT_FIXED( payloadSize ) \
	const void *payload;			/* Payload data (e.g. encr.key) */
	int payloadSize;
	union {
		CONTENT_ENCR_INFO contentEncrInfo;	/* Encryption obj-specific infor.*/
		CONTENT_SIG_INFO contentSigInfo;	/* Signature obj-specific info.*/
		CONTENT_AUTHENC_INFO contentAuthEncInfo;/* Auth-enc obj-spec.info */
		} contentInfo;
	} CONTENT_LIST;

/****************************************************************************
*																			*
*								De-envelope Actions							*
*																			*
****************************************************************************/

/* The current state of the (de)enveloping.  The states are the predata state
   (when we're performing final setup steps and handling header information
   in the envelope), the data state (when we're enveloping data), the
   postdata state (when we're handling trailer information), and the
   extradata state (when we're processing out-of-band data such as the data
   associated with detached signatures) */

typedef enum {
	ENVELOPE_STATE_NONE,			/* No envelope state */
	ENVELOPE_STATE_PREDATA,			/* Emitting header information */
	ENVELOPE_STATE_DATA,			/* During (de)enveloping of data */
	ENVELOPE_STATE_POSTDATA,		/* After (de)enveloping of data */
	ENVELOPE_STATE_EXTRADATA,		/* Additional out-of-band data */
	ENVELOPE_STATE_FINISHED,		/* Finished processing */
	ENVELOPE_STATE_LAST				/* Last valid enveloping state */
	} ENVELOPE_STATE;

/* The current state of the processing of CMS headers that contain non-data
   during the enveloping process.  Before the enveloping of data begins, the
   user pushes in a variety of enveloping information, which in turn might
   trigger the creation of more internal information objects.  Once the
   enveloping begins, this information is encoded as ASN.1 structures and 
   written into the envelope buffer.  The encoding process can be interrupted 
   at any point when the envelope buffer fills up, so we break it down into a 
   series of atomic states between which the enveloping process can be 
   interrupted by the caller removing data from the envelope.

   There are two sets of states, the first set that covers the encoding of
   the header information at the start of the envelope (only key exchange
   information requires this), and the second that covers the information at
   the end of the envelope (only signatures require this) */

typedef enum {
	ENVSTATE_NONE,					/* No header processing/before header */

	/* Header state information */
	ENVSTATE_HEADER,				/* Emitting header */
	ENVSTATE_KEYINFO,				/* Emitting key exchange information */
	ENVSTATE_ENCRINFO,				/* Emitting EncrContentInfo information */

	ENVSTATE_DATA,					/* Emitting data payload information */

	/* Trailer state information */
	ENVSTATE_FLUSHED,				/* Data flushed through into buffer */
	ENVSTATE_SIGNATURE,				/* Emitting signatures */

	ENVSTATE_DONE,					/* Finished processing header/trailer */

	ENVSTATE_LAST					/* Last valid enveloping state */
	} ENV_STATE;

/* The current state of the processing of CMS headers that contain non-data
   in the envelope during the de-enveloping process.  This is implemented as 
   a somewhat complex FSM because the enveloping routines give the user the
   ability to push in arbitrary amounts of data corresponding to various
   enveloping structures and simultaneously pop out data/information based 
   on decoding them.  A typical complex enveloped type might contain a 
   number of headers, a session key encrypted with 18 different public keys, 
   five varieties of signature type, and God knows what else, of which the 
   caller might feed us 500 bytes - a small fraction of the total data - and 
   then ask for information on what they've just fed us.  We have to 
   remember how far we got (halfway through an RSA-encrypted DES key fifteen 
   levels of nesting down in an ASN.1 structure), process everything that we 
   can, and then get back to them on what we found.  Then they feed us 
   another few hundred bytes and the whole thing starts anew.

   The state machine works by processing one complete object or part of an
   object at a time and then moving on to the next state that corresponds to
   handling another part of the object or another object.  If there isn't
   enough data present to process a part or subpart, we return an underflow
   error and try again when more data is added */

typedef enum {
	DEENVSTATE_NONE,				/* No header processing/before header */

	/* Header state information */
	DEENVSTATE_SET_ENCR,			/* Processing start of SET OF EncrKeyInfo */
	DEENVSTATE_ENCR,				/* Processing EncrKeyInfo records */
	DEENVSTATE_ENCRCONTENT,			/* Processing EncrContentInfo */

	DEENVSTATE_SET_HASH,			/* Processing start of SET OF DigestAlgoID */
	DEENVSTATE_HASH,				/* Processing DigestAlgoID (for hash) records */
	DEENVSTATE_MAC,					/* Processing DigestAlgoID (for MAC) records */
	
	DEENVSTATE_CONTENT,				/* Processing ContentInfo */
	DEENVSTATE_DATA,				/* Processing data payload */

	/* Trailer state information */
	DEENVSTATE_CERTSET,				/* Processing optional cert chain */
	DEENVSTATE_SET_SIG,				/* Processing start of SET OF Signature */
	DEENVSTATE_SIG,					/* Processing Signature records */
	DEENVSTATE_EOC,					/* Processing end-of-contents octets */

	DEENVSTATE_DONE,				/* Finished processing header/trailer */

	DEENVSTATE_LAST					/* Last valid de-enveloping state */
	} DEENV_STATE;

/* The current state of processing of PGP headers that contain non-data in 
   the envelope during the de-enveloping process.  These are somewhat 
   different to the ASN.1-encoded objects used by cryptlib in that many of 
   the objects are emitted as discrete packets rather than the nested 
   objects used in ASN.1 objects.  This makes some parts of the processing
   much easier (less length information to track) and some parts much harder
   (since just about anything could appear next, you need to maintain a
   lookahead to figure out what to do next, but you may run out of data
   before you can determine which state is next).  The handling of content
   inside encrypted data is particularly messy since there's a plain-data
   header that has to be removed in a manner which is transparent to the
   user.
   
   The two de-enveloping encrypted-data states are almost identical except 
   for the fact that one performs PGP's odd IV resync while the other 
   doesn't, a requirement buried in the depths of two otherwise identical 
   text blocks in the RFC */

typedef enum {
	PGP_DEENVSTATE_NONE,			/* No message processing/before message */

	/* Header state information */
	PGP_DEENVSTATE_ENCR_HDR,		/* PKE/SKE packet */
	PGP_DEENVSTATE_ENCR,			/* Encrypted data packet */
	PGP_DEENVSTATE_ENCR_MDC,		/* Encrypted data with MDC */

	PGP_DEENVSTATE_DATA,			/* Data */
	PGP_DEENVSTATE_DATA_HEADER,		/* Out-of-band data inside compressed data */

	PGP_DEENVSTATE_DONE,			/* Finished processing message */

	PGP_DEENVSTATE_LAST				/* Last valid de-enveloping state */
	} PGP_DEENV_STATE;

/* Envelope information flags.  These are:

	ENVELOPE_ISDEENVELOPE: The envelope is a de-enveloping envelope.

	ENVELOPE_DETACHED_SIG: The (signed data) envelope should generate a
			standalone detached signature rather than signed enveloped data.

	ENVELOPE_NOSIGNINGCERTS: When generating a S/MIME signed data, don't
			include the signing certificates with the data.

	ENVELOPE_ATTRONLY: The (signed data) envelope only contains authenticated
			attributes, but not actual data.  This is required by SCEP.

	ENVELOPE_ZSTREAMINITED: Whether the zlib compression/decompression stream
			has been initialised.
	
	ENVELOPE_AUTHENC: Use authenticated encryption, which adds a MAC tag to
			the encrypted data */

#define ENVELOPE_ISDEENVELOPE	0x0001	/* De-enveloping envelope */
#define ENVELOPE_DETACHED_SIG	0x0002	/* Generate detached signature */
#define ENVELOPE_NOSIGNINGCERTS	0x0004	/* Don't include signing certs */
#define ENVELOPE_ATTRONLY		0x0008	/* Env.contains only auth'd attrs.*/
#define ENVELOPE_ZSTREAMINITED	0x0010	/* Whether zlib stream has been inited */
#define ENVELOPE_AUTHENC		0x0020	/* Use authenticated encryption */

/* Envelope data processing flags.  These are:

	ENVDATA_AUTHENACTIONSACTIVE: The (authenticated-encrypted) envelope is
			currently hashing payload data.  This differs from 
			HASHACTIONSACTIVE in that it hashes ciphertext, not plaintext.

	ENVDATA_HASINDEFTRAILER: The (signed) envelope trailer has an indefinite
			length due to the use of signature algorithms that produce 
			variable-length output that can't be determined in advance.

	ENVDATA_HASHACTIONSACTIVE: The (signed) envelope is currently hashing
			payload data.  This differs from AUTHENCACTIONSACTIVE in that
			it hashes plaintext, not ciphertext.

	ENVDATA_NOLENGTHINFO: The payload uses neither a definite- nor 
			indefinite-length encoding but continues until the caller tells
			us it's finished.  This is used to handle PGP 2.x compressed
			data, which just continues until EOF with no length information
			provided.

	ENVDATA_NOSEGMENT: The payload data shouldn't be segmented because a
			definite-length encoding is being used.

	ENVDATA_NOFIRSTSEGMENT: The payload data is segmented but the first 
			segment doesn't have an explicit length, being defined as 
			"whatever's left after the data at the start has been 
			processed".  This is used to handle PGP's indefinite-length
			encoding, which unlike ASN.1's 
			
				[indef.marker][segment][segment]...[EOC]

			is encoded as:

				[length+cont,segment'][length+cont,segment]...[length,segment]

			so the initial length has already been read when the packet
			header was read.  We can't undo the read of the start of the 
			first packet and treat it as a standard segement because the 
			encoding for the first segment (denoted by segment' in the 
			diagram above) and the remaining segments is different so an 
			attempt to treat them identically will lead to a decoding error.

	ENVDATA_SEGMENTCOMPLETE: An indefinite-length segment has been completed
			an another one must be begun before more payload data can be
			emitted.

	ENVDATA_ENDOFCONTENTS: The EOC octets on indefinite payload data have 
			been reached.

	ENVDATA_NEEDSPADDING: Before (encrypted) enveloping can been completed 
			the payload data needs PKCS #5 padding added to it.

	ENVDATA_HASATTACHEDOOB: The envelope has out-of-band additional data
			attached to the payload data.  This is used by OpenPGP to tack
			an MDC packet onto the end of encrypted data.

   The handling of several of these flags is quite complex, more details can 
   be found in encode/decode.c */

#define ENVDATA_HASINDEFTRAILER	0x0001	/* Whether trailer size is indefinite */
#define ENVDATA_HASHACTIONSACTIVE 0x0002 /* Payload hashing is active */
#define ENVDATA_AUTHENCACTIONSACTIVE 0x0004	/* Payload ciphertext hashing active */
#define ENVDATA_NOLENGTHINFO	0x0008	/* No length info for payload avail.*/
#define ENVDATA_NOSEGMENT		0x0010	/* Don't segment payload data */
#define ENVDATA_NOFIRSTSEGMENT	0x0020	/* No first segment length */
#define ENVDATA_SEGMENTCOMPLETE	0x0040	/* Current segment has been completed */
#define ENVDATA_ENDOFCONTENTS	0x0080	/* EOC reached */
#define ENVDATA_NEEDSPADDING	0x0100	/* Whether to add PKCS #5 padding */
#define ENVDATA_HASATTACHEDOOB	0x0200	/* Whether data has attached OOB extra */

/* Envelope data-copy flags.  These are:

	ENVCOPY_FLAG_NONE: Perform no special processing.

	ENVCOPY_FLAG_OOBDATA: Data is out-of-band data that isn't part of the 
			normal data payload */

#define ENVCOPY_FLAG_NONE		0x00	/* No special action */
#define ENVCOPY_FLAG_OOBDATA	0x01	/* Data is OOB rather than payload data */
#define ENVCOPY_FLAG_MAX		0x01	/* Maximum possible flag value */

/* The size of the buffers used to handle read-ahead into out-of-band data 
   at  the start of the payload, and to buffer leftover bytes when the data 
   that we're given is split exactly over a header (OCTET STRING + length
   for CMS, partial length for PGP) that we can't leave with the payload 
   data but have to store until we get fed the rest of the header.  See
   the longer comments where the variables are declared for details */

#define OOB_BUFFER_SIZE			8
#define PARTIAL_BUFFER_SIZE		16

/* The structure that stores the information on an envelope */

struct EI;

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *ENV_ADDINFOFUNCTION )( INOUT struct EI *envelopeInfoPtr,
									  IN_ATTRIBUTE \
										const CRYPT_ATTRIBUTE_TYPE envInfo,
									  IN_INT_Z const int value );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
		int ( *ENV_ADDINFOSTRINGFUNCTION )( INOUT struct EI *envelopeInfoPtr,
											IN_RANGE( CRYPT_ENVINFO_PASSWORD, \
													  CRYPT_ENVINFO_PASSWORD ) \
												const CRYPT_ATTRIBUTE_TYPE envInfo,
											IN_BUFFER( valueLength ) \
												const void *value, 
											IN_RANGE( 1, CRYPT_MAX_TEXTSIZE ) \
												const int valueLength );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *ENV_CHECKMISSINGINFOFUNCTION )( INOUT struct EI *envelopeInfoPtr,
											   const BOOLEAN isFlush );
typedef CHECK_RETVAL \
		BOOLEAN ( *ENV_CHECKALGOFUNCTION )( IN_ALGO \
												const CRYPT_ALGO_TYPE cryptAlgo, 
											IN_MODE_OPT \
												const CRYPT_MODE_TYPE cryptMode );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *ENV_PROCESSPREAMBLEFUNCTION )( INOUT struct EI *envelopeInfoPtr );
typedef CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *ENV_PROCESSPOSTAMBLEFUNCTION )( INOUT struct EI *envelopeInfoPtr,
											   const BOOLEAN isFlush );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *ENV_COPYTOENVELOPEFUNCTION )( INOUT struct EI *envelopeInfoPtr, 
											 IN_BUFFER_OPT( length ) \
												const BYTE *buffer, 
											 IN_DATALENGTH_Z const int length );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
		int ( *ENV_COPYFROMENVELOPEFUNCTION )( INOUT struct EI *envelopeInfoPtr, 
											   OUT_BUFFER( maxLength, *length ) \
													BYTE *buffer, 
											   IN_DATALENGTH const int maxLength, 
											   OUT_DATALENGTH_Z int *length, 
											   IN_FLAGS_Z( ENVCOPY ) \
													const int flags );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *ENV_PROCESSEXTRADATAFUNCTION )( INOUT struct EI *envelopeInfoPtr, 
											   IN_BUFFER( length ) \
													const void *buffer, 
											   IN_DATALENGTH_Z const int length );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *ENV_SYNCDEENVELOPEDATAFUNCTION )( INOUT struct EI *envelopeInfoPtr, 
												 INOUT STREAM *stream );

typedef struct EI {
	/* Control and status information */
	CRYPT_FORMAT_TYPE type;			/* Envelope type */
	CRYPT_CONTENT_TYPE contentType;	/* Inner content type */
	ACTION_TYPE usage;				/* Usage (signing, encryption, etc) */
	int version;					/* Protocol version/subtype */
	int flags;						/* Envelope information flags */
	int dataFlags;					/* Envelope data processing flags */

	/* The list of actions to perform on the data.  There are three sets of
	   actions, the preActions (key exchange), the main actions (encryption 
	   or hashing), and the postActions (signing) */
	ACTION_LIST *preActionList;
	ACTION_LIST *actionList;
	ACTION_LIST *postActionList;

	/* Several action groups produce information which is prepended or
	   appended to the data.  The following variables record the encoded size
	   of this information.  In some cases the size of the appended
	   information isn't known when the enveloping is started so we have to
	   use an indefinite-length encoding for the outermost wrapper, if this
	   is the case then we set the ENVDATA_HASINDEFTRAILER flag to indicate 
	   that a definite-length encoding shouldn't be used even if the payload 
	   size is known */
	int cryptActionSize;			/* Size of key exchange actions */
	int signActionSize;				/* Size of signatures */
	int extraDataSize;				/* Size of any extra data */

	/* When prepending or appending header or trailer information to an
	   envelope we need to record the current position in the action list so
	   that we can continue later if we run out of room */
	ACTION_LIST *lastAction;

	/* When de-enveloping we may have objects present that can't be used
	   until user-supplied de-enveloping information is added to the
	   envelope.  We store these in a linked list in memory until the
	   information needed to work with them is present.  We also store a
	   pointer to the current position in the list, which is used when
	   traversing the list */
	CONTENT_LIST *contentList, *contentListCurrent;

	/* The public-key encryption/private-key decryption and signature-check
	   keysets that are used to look up any keys required during the 
	   enveloping/de-enveloping process */
	CRYPT_KEYSET iDecryptionKeyset;
	CRYPT_KEYSET iEncryptionKeyset;
	CRYPT_KEYSET iSigCheckKeyset;

	/* When we're encrypting/decrypting the envelope payload, the one action 
	   that we'll be performing constantly is encryption.  Similarly, when
	   we're signing/sig.checking, we'll be hashing the payload.  In order 
	   to make this process more efficientm, we record the encryption and 
	   hashing info here to save having to pull it out of the action list 
	   whenever it's needed.
	   
	   The encryption context, of which there's only one, is stored here.
	   Note that since there is a second reference held in the action list, 
	   there's no need to explicitly delete this when we destroy the 
	   envelope object since it's already been destroyed when the action 
	   list is destroyed.

	   For hashing, we may have multiple hash contexts active, so we still
	   use the action list for these.  However it's convenient to have 
	   direct confirmation that we have to hash the payload data to save 
	   having to check the action list each time, so we set the
	   ENVDATA_HASHACTIONSACTIVE flag to indicate that hashing is taking 
	   place.  This is only set while the hashing is taking place, once 
	   there's no more data to be hashed it's cleared to prevent out-of-band 
	   data from being hashed as well */
	CRYPT_CONTEXT iCryptContext;

	/* Some types of key management/signature require additional certs.
	   For encryption, there may be originator certs present for use with
	   key agreement algorithms; for signing, there may be a signing-cert
	   meta-object present to contain the union of multiple sets of signing
	   certs if multiple signatures are present.  These are held in the 
	   following certificate object */
	CRYPT_CERTIFICATE iExtraCertChain;

	/* The encryption/hashing/signature defaults for this envelope.  These
	   are recorded here when the envelope is created (so that a later 
	   change of the default value won't affect the enveloping process) but 
	   can be changed explicitly on a per-envelope basis by setting the 
	   options just for the envelope rather than for all of cryptlib */
	CRYPT_ALGO_TYPE defaultHash;		/* Default hash algorithm */
	CRYPT_ALGO_TYPE defaultAlgo;		/* Default encryption algorithm */
	CRYPT_ALGO_TYPE defaultMAC;			/* Default MAC algorithm */

#ifdef USE_COMPRESSION
	/* zlib stream compression data structure used to hold the compression/
	   decompression state */
	z_stream zStream;				/* zlib state variable */
#endif /* USE_COMPRESSION */

	/* Buffer information */
	BUFFER( bufSize, bufPos ) \
	BYTE *buffer;					/* Data buffer */
	int bufSize;					/* Total buffer size */
	int bufPos;						/* Last data position in buffer */

	/* Auxiliary buffer used as a staging area for holding signature data 
	   that may not currently fit into the main buffer.  These are generated 
	   into the auxiliary buffer and then copied into the main buffer as 
	   required */
	BUFFER_OPT( auxBufSize, auxBufPos ) \
	BYTE *auxBuffer;				/* Buffer for sig.data */
	int auxBufSize, auxBufPos;		/* Current pos and total aux.buf.size */

	/* When the caller knows in advance how large the payload will be, they 
	   can advise the enveloping code of this, which allows a more efficient 
	   encoding of the data.  The following variable records the payload 
	   size */
	long payloadSize;

	/* The current state of header processing.  The cryptlib/CMS and PGP
	   processing states are kept separate (although they could actually be 
	   merged into the same variable) because they are conceptually separate 
	   and shouldn't really be treated as the same thing */
	ENVELOPE_STATE state;			/* Current state of processing */
	ENV_STATE envState;				/* Current state of env.non-data proc.*/
	DEENV_STATE deenvState;			/* Current state of de-env.non-data proc.*/
#ifdef USE_PGP
	PGP_DEENV_STATE pgpDeenvState;	/* Current state of PGP de-env.n-d proc.*/
#endif /* USE_PGP */
	long hdrSetLength;				/* Remaining bytes in SET OF EKeyInfo */

	/* When we pushe data into an envelope it may end up breaking the push 
	   over an intermediate header.  This occurs if we're pushing indefinite-
	   length data and stop halfway through a tag, either an EOC or an OCTET 
	   STRING segment (or its PGP equivalent):

		+-------+-----------+-----+---------+
		| Header|	Body	|00 00| Trailer |
		+-------+-----------+-----+---------+
							   ^	
							   |
							 bufPos
							   +-------+
									   v
		+-------+--------+----------+--------+----------+-----+---------+
		| Header|04 xx xx|	Body	|04 xx xx|	Body	|00 00|	Trailer |
		+-------+--------+----------+--------+----------+-----+---------+

	   In this case we can't decode the intermediate header and have to
	   buffer the data somewhere until the next push.  We can't report the
	   remainder to the caller as un-consumed data because this may be an
	   implicit push, for example when we add a keying resource to an 
	   encrypted envelope, which continues processing with previously-pushed 
	   data when it initialises the cryptovariables from the data.  In this
	   case since no data is being pushed, there's no way to report that 
	   some of the data was unconsumed.
	   
	   Because of this we have to buffer any partial information until the
	   next data push.  This can only occur when we break across a partial
	   header, so the amount of data to be buffered is minimal.
	   
	   (It also occurs extremely rarely since it requires indefinite-length 
	   data and there's usually only a single byte location where this can 
	   occur, this situation almost never occurs) */
	BUFFER( PARTIAL_BUFFER_SIZE, partialBufPos ) \
	BYTE partialBuffer[ PARTIAL_BUFFER_SIZE + 8 ];	/* Buffered partial header data */
	int partialBufPos;

	/* Some data formats place out-of-band data at the start of the payload
	   rather than putting it in the header, so we need to be able to peek
	   ahead into the processed (e.g. decrypted) data via a lookahead read 
	   in order to determine what to do next.  The out-of-band (OOB) buffer 
	   and associated variables take care of this.
	   
	   The OOB data-left variable keeps track of how many bytes of data 
	   still need to be removed before we can return actual payload data to 
	   the caller.  In some cases the amount of data can't be specified as a 
	   simple byte count but involves format-specific events (e.g. the 
	   presence of a flag or data count in the OOB data that indicates that 
	   there's more data present), the OOB event-count variable records how 
	   many of these events still need to be handled before we can return 
	   data to the caller.
	   
	   Finally, some content types (e.g. compressed data) don't allow 
	   lookahead reads, which are necessary in some cases to determine how 
	   the payload needs to be handled.  To handle this we have to remember 
	   the returned OOB data (if it's read via a lookahead read) so that we 
	   can reinsert it into the output stream on the next read call */
	int oobDataLeft;				/* Remaining out-of-band data in payload */
	int oobEventCount;				/* No.events left to process */
	BUFFER( OOB_BUFFER_SIZE, oobBufSize ) \
	BYTE oobBuffer[ OOB_BUFFER_SIZE + 8 ];	/* Buffered OOB data */
	int oobBufSize;

	/* Information on the current OCTET STRING segment in the buffer during
	   the de-enveloping process.  We keep track of the segment start point 
	   (the byte after the OCTET STRING tag), the segment data start point 
	   (which may move when the segment is terminated if the length encoding 
	   shrinks due to a short segment), and whether we've just completed a 
	   segment (which we need to do before we can pop any data from the 
	   envelope, this is tracked via the ENVDATA_SEGMENTCOMPLETE flag).  In 
	   addition we track where the last completed segment ends, as the buffer 
	   may contain one or more completed segments followed by an incomplete 
	   segment, and any attempt to read into the incomplete segment will 
	   require it to be completed first */
	int segmentStart;				/* Segment len+data start point */
	int segmentDataStart;			/* Segment data start point */
	int segmentDataEnd;				/* End of completed data */

	/* The remaining data in the current OCTET STRING segment, explicitly 
	   declared as a long since we may be processing data that came from a 
	   32-bit machine on a 16-bit machine.  During enveloping this is used
	   to track the amount of data left from the user-declared payloadSize
	   that can still be added to the envelope.  During de-enveloping, it's 
	   used to record how much data is left in the current segment */
	long segmentSize;				/* Remaining data in segment */

	/* Once the low-level segment-processing code sees the end-of-contents
	   octets for the payload, we need to notify the higher-level code that
	   anything that follows is out-of-band data that needs to be processed
	   at a higher level.  We do this by setting the ENVDATA_ENDOFCONTENTS
	   flag to record the fact that we've seen the payload EOC and have 
	   moved to trailing out-of-band data.  Since the post-payload data
	   isn't directly retrievable by the user, we use the dataLeft variable 
	   to keep track of how much of what's left in the envelope is payload 
	   that can be copied out */
	int dataLeft;

	/* Block cipher buffer for leftover bytes.  This contains any bytes
	   remaining to be en/decrypted when the input data size isn't a
	   multiple of the cipher block size */
	BUFFER( CRYPT_MAX_IVSIZE, blockBufferPos ) \
	BYTE blockBuffer[ CRYPT_MAX_IVSIZE + 8 ];/* Leftover byte buffer */
	int blockBufferPos;				/* Position in buffer */
	int blockSize;					/* Cipher block size */
	int blockSizeMask;				/* Mask for blockSize-sized blocks */

	/* The overall envelope status.  Some error states (underflow/overflow
	   enveloping information errors, randomness errors, and a few others)
	   are recoverable whereas other states (bad data) aren't recoverable.
	   If we run into a nonrecoverable error, we remember the status here so
	   that any further attempts to work with the envelope will return this
	   status */
	int errorState;

	/* Error information */
	CRYPT_ATTRIBUTE_TYPE errorLocus;/* Error locus */
	CRYPT_ERRTYPE_TYPE errorType;	/* Error type */

	/* Low-level error information */
	ERROR_INFO errorInfo;

	/* Pointers to the enveloping/de-enveloping functions */
	ENV_ADDINFOFUNCTION addInfo;
	ENV_ADDINFOSTRINGFUNCTION addInfoString;
	ENV_CHECKMISSINGINFOFUNCTION checkMissingInfo;
	ENV_CHECKALGOFUNCTION checkAlgo;
	ENV_PROCESSPREAMBLEFUNCTION processPreambleFunction;
	ENV_PROCESSPOSTAMBLEFUNCTION processPostambleFunction;
	ENV_COPYTOENVELOPEFUNCTION copyToEnvelopeFunction;
	ENV_COPYFROMENVELOPEFUNCTION copyFromEnvelopeFunction;
	ENV_PROCESSEXTRADATAFUNCTION processExtraData;
	ENV_SYNCDEENVELOPEDATAFUNCTION syncDeenvelopeData;

	/* The object's handle and the handle of the user who owns this object.
	   The former is used when sending messages to the object when only the 
	   xxx_INFO is available, the latter is used to avoid having to fetch the
	   same information from the system object table */
	CRYPT_HANDLE objectHandle;
	CRYPT_USER ownerHandle;

	/* The local memory pool used to allocate small data objects attached to 
	   the envelope */
	MEMPOOL_STATE memPoolState;

	/* Variable-length storage for the type-specific data */
	DECLARE_VARSTRUCT_VARS;
	} ENVELOPE_INFO;

/****************************************************************************
*																			*
*								Enveloping Functions						*
*																			*
****************************************************************************/

/* Envelope attribute handling functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getEnvelopeAttribute( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						  OUT_INT_Z int *valuePtr, 
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getEnvelopeAttributeS( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						   INOUT MESSAGE_DATA *msgData, 
						   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setEnvelopeAttribute( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						  IN_INT_Z const int value, 
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setEnvelopeAttributeS( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						   IN_BUFFER( dataLength ) const void *data,
						   IN_LENGTH const int dataLength,
						   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );


/* Prototypes for envelope action management functions */

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *CHECKACTIONFUNCTION )( const ACTION_LIST *actionListPtr,
									  IN_INT_Z const int intParam );

CHECK_RETVAL_BOOL \
BOOLEAN moreActionsPossible( IN_OPT const ACTION_LIST *actionListPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int addAction( INOUT_PTR ACTION_LIST **actionListHeadPtrPtr,
			   INOUT MEMPOOL_STATE memPoolState,
			   IN_ENUM( ACTION ) const ACTION_TYPE actionType,
			   IN_HANDLE const CRYPT_HANDLE cryptHandle );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int addActionEx( OUT_OPT_PTR_COND ACTION_LIST **newActionPtrPtr,
				 INOUT_PTR ACTION_LIST **actionListHeadPtrPtr,
				 INOUT MEMPOOL_STATE memPoolState,
				 IN_ENUM( ACTION ) const ACTION_TYPE actionType,
				 IN_HANDLE const CRYPT_HANDLE cryptHandle );
STDC_NONNULL_ARG( ( 1 ) ) \
int replaceAction( INOUT ACTION_LIST *actionListItem,
				   IN_HANDLE const CRYPT_HANDLE cryptHandle );
CHECK_RETVAL_ENUM( ACTION ) \
ACTION_RESULT checkAction( IN_OPT const ACTION_LIST *actionListStart,
						   IN_ENUM( ACTION ) const ACTION_TYPE actionType, 
						   IN_HANDLE const CRYPT_HANDLE cryptHandle );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int checkActionIndirect( const ACTION_LIST *actionListStart,
						 IN CHECKACTIONFUNCTION checkActionFunction,
						 IN_INT_Z const int intParam );
CHECK_RETVAL_PTR \
ACTION_LIST *findAction( IN_OPT const ACTION_LIST *actionListPtr,
						 IN_ENUM( ACTION ) const ACTION_TYPE actionType );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ACTION_LIST *findLastAction( const ACTION_LIST *actionListPtr,
							 IN_ENUM( ACTION ) \
							 const ACTION_TYPE actionType );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
ACTION_LIST *findActionIndirect( const ACTION_LIST *actionListStart,
								 IN CHECKACTIONFUNCTION checkActionFunction,
								 IN_INT_Z const int intParam );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void deleteActionList( INOUT MEMPOOL_STATE memPoolState,
					   INOUT ACTION_LIST *actionListPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
int deleteUnusedActions( INOUT ENVELOPE_INFO *envelopeInfoPtr );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkActions( INOUT ENVELOPE_INFO *envelopeInfoPtr );

/* Prototypes for content list management functions */

CHECK_RETVAL_BOOL \
BOOLEAN moreContentItemsPossible( IN_OPT const CONTENT_LIST *contentListPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int createContentListItem( OUT_BUFFER_ALLOC_OPT( sizeof( CONTENT_LIST ) ) \
								CONTENT_LIST **newContentListItemPtrPtr,
						   INOUT MEMPOOL_STATE memPoolState,
						   IN_ENUM( CONTENT ) const CONTENT_TYPE type,
						   IN_ENUM( CRYPT_FORMAT ) \
								const CRYPT_FORMAT_TYPE formatType,
						   IN_BUFFER_OPT( objectSize ) const void *object, 
						   IN_LENGTH_Z const int objectSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int appendContentListItem( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						   INOUT CONTENT_LIST *contentListItem );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
int deleteContentList( INOUT MEMPOOL_STATE memPoolState,
					   INOUT_PTR CONTENT_LIST **contentListHeadPtrPtr );

/* Prototypes for misc.management functions */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN envelopeSanityCheck( const ENVELOPE_INFO *envelopeInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addKeysetInfo( INOUT ENVELOPE_INFO *envelopeInfoPtr,
				   IN_RANGE( CRYPT_ENVINFO_KEYSET_SIGCHECK, \
							 CRYPT_ENVINFO_KEYSET_DECRYPT ) \
					const CRYPT_ATTRIBUTE_TYPE keysetFunction,
				   IN_HANDLE const CRYPT_KEYSET keyset );
CHECK_RETVAL_BOOL \
BOOLEAN cmsCheckAlgo( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
					  IN_MODE_OPT const CRYPT_MODE_TYPE cryptMode );
CHECK_RETVAL_BOOL \
BOOLEAN pgpCheckAlgo( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo, 
					  IN_MODE_OPT const CRYPT_MODE_TYPE cryptMode );

/* Prepare the envelope for data en/decryption */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initEnvelopeEncryption( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							IN_HANDLE const CRYPT_CONTEXT cryptContext,
							IN_ALGO_OPT const CRYPT_ALGO_TYPE algorithm, 
							IN_MODE_OPT const CRYPT_MODE_TYPE mode,
							IN_BUFFER_OPT( ivLength ) const BYTE *iv, 
							IN_LENGTH_IV_Z const int ivLength,
							const BOOLEAN copyContext );

/* Prototypes for enveloping internal functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int cmsPreEnvelopeEncrypt( INOUT ENVELOPE_INFO *envelopeInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int cmsPreEnvelopeSign( INOUT ENVELOPE_INFO *envelopeInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int hashEnvelopeData( const ACTION_LIST *hashActionPtr,
					  IN_BUFFER( dataLength ) const void *data, 
					  IN_LENGTH_Z const int dataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int cmsInitSigParams( const ACTION_LIST *actionListPtr,
					  IN_ENUM( CRYPT_FORMAT ) const CRYPT_FORMAT_TYPE formatType,
					  IN_HANDLE const CRYPT_USER iCryptOwner,
					  OUT SIGPARAMS *sigParams );

/* Prototypes for envelope mapping functions */

#ifdef USE_CMS
  STDC_NONNULL_ARG( ( 1 ) ) \
  void initCMSEnveloping( INOUT ENVELOPE_INFO *envelopeInfoPtr );
  STDC_NONNULL_ARG( ( 1 ) ) \
  void initCMSDeenveloping( INOUT ENVELOPE_INFO *envelopeInfoPtr );
#else
  #define initCMSEnveloping( envelopeInfoPtr )
  #define initCMSDeenveloping( envelopeInfoPtr )
#endif /* USE_CMS */
#ifdef USE_PGP
  STDC_NONNULL_ARG( ( 1 ) ) \
  void initPGPEnveloping( INOUT ENVELOPE_INFO *envelopeInfoPtr );
  STDC_NONNULL_ARG( ( 1 ) ) \
  void initPGPDeenveloping( INOUT ENVELOPE_INFO *envelopeInfoPtr );
#else
  #define initPGPEnveloping( envelopeInfoPtr )
  #define initPGPDeenveloping( envelopeInfoPtr )
#endif /* USE_PGP */
STDC_NONNULL_ARG( ( 1 ) ) \
void initEnvelopeStreaming( INOUT ENVELOPE_INFO *envelopeInfoPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void initEnvResourceHandling( INOUT ENVELOPE_INFO *envelopeInfoPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void initDeenvelopeStreaming( INOUT ENVELOPE_INFO *envelopeInfoPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void initDenvResourceHandling( INOUT ENVELOPE_INFO *envelopeInfoPtr );

#endif /* _ENV_DEFINED */
