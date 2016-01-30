/****************************************************************************
*																			*
*						Certificate Routines Header File 					*
*						Copyright Peter Gutmann 1996-2008					*
*																			*
****************************************************************************/

#ifndef _CERT_DEFINED

#define _CERT_DEFINED

#include <time.h>
#ifndef _STREAM_DEFINED
  #if defined( INC_ALL )
	#include "stream.h"
  #else
	#include "io/stream.h"
  #endif /* Compiler-specific includes */
#endif /* _STREAM_DEFINED */

/* The minimum size of an attribute, SEQUENCE (2), OID (5),
   OCTET STRING (2+3 for payload).  This is the amount of slop to allow when
   reading attributes.  Some software gets the length encoding wrong by a few
   bytes, if what's left at the end of an encoded object is >= this value
   then we look for attributes */

#define MIN_ATTRIBUTE_SIZE		12

/* The maximum size of a PKCS #7 certificate chain.  The built-in bounds
   value FAILSAFE_ITERATIONS_MED is used as a safety check for an upper
   limit on chain lengths (that is, if we hit FAILSAFE_ITERATIONS_MED on
   processing a certificate chain it's an internal error), so it has to
   be larger than MAX_CHAINLENGTH */

#define MAX_CHAINLENGTH			16

#if MAX_CHAINLENGTH > FAILSAFE_ITERATIONS_MED
  #error FAILSAFE_ITERATIONS_MED must be larger than the maximum certificate chain length
#endif /* MAX_CHAINLENGTH > FAILSAFE_ITERATIONS_MED */

/* The default size of the serial number, size of the built-in serial number
   buffer (anything larger than this uses a dynamically-allocated buffer)
   and the maximum size in bytes of a serial number (for example in a
   certificate or CRL).  Technically values of any size are allowed but
   anything larger than this is probably an error */

#define DEFAULT_SERIALNO_SIZE	8
#define SERIALNO_BUFSIZE		32
#define MAX_SERIALNO_SIZE		256

/* The size of the PKI user binary authenticator information before
   checksumming and encoding and the size of the encrypted user info:
   sizeofObject( 2 * sizeofObject( PKIUSER_AUTHENTICATOR_SIZE ) ) + PKCS #5
   padding = 2 + ( 2 + 12 + 2 + 12 ) = 30 + 2 = 32.  This works for both 64-
   and 128-bit block ciphers */

#define PKIUSER_AUTHENTICATOR_SIZE		12
#define PKIUSER_ENCR_AUTHENTICATOR_SIZE	32

/* The size of the FIFO used to encode nested SEQUENCEs */

#define ENCODING_FIFO_SIZE		10

/* Normally we check for a valid time by making sure that it's more recent 
   than MIN_TIME_VALUE, however when reading a certificate the time can be 
   much earlier than this if it's an old certificate.  To handle this we 
   define a certificate-specific time value that we use as the oldest valid 
   time value */

#define MIN_CERT_TIME_VALUE		( ( 1996 - 1970 ) * 365 * 86400L )

/* Certificate information flags.  These are:

	FLAG_CERTCOLLECTION: Indicates that a certificate chain object contains 
			only an unordered collection of (non-duplicate) certificates 
			rather than a true certificate chain.  Note that this is a pure 
			container object for which only the certificate chain member 
			contains certificates, the base certificate object doesn't 
			correspond to an actual certificate.

	FLAG_CRLENTRY: The CRL object contains the data from a single CRL entry
			rather than being a complete CRL.

	FLAG_DATAONLY: Indicates a pure data object with no attached context.

	FLAG_PATHKLUDGE: Indicates that although the certificate appears to be a 
			self-signed (CA root) certificate it's actually a PKIX path 
			kludge certificate that's used to tie a re-issued CA certificate 
			(with a new CA key) to existing issued certificates signed with 
			the old CA key.  This kludge requires that issuer DN == subject 
			DN, which would denote a CA root certificate under normal 
			circumstances.

	FLAG_SELFSIGNED: Indicates that the certificate is self-signed.

	FLAG_SIGCHECKED: Caches the check of the certificate signature.  This is 
			done because it's only necessary to perform this once when the 
			certificate is checked for the first time.  Checking of 
			certificate fields that aren't affected by the issuer 
			certificate is also cached, but this is handled by the 
			compliance-level check value rather than a simple boolean flag 
			since a certificate can be checked at various levels of 
			standards-compliance */

#define CERT_FLAG_NONE			0x00	/* No flag */
#define CERT_FLAG_SELFSIGNED	0x01	/* Certificate is self-signed */
#define CERT_FLAG_SIGCHECKED	0x02	/* Signature has been checked */
#define CERT_FLAG_DATAONLY		0x04	/* Certificate is data-only (no context) */
#define CERT_FLAG_CRLENTRY		0x08	/* CRL is a standalone single entry */
#define CERT_FLAG_CERTCOLLECTION 0x10	/* Certificate chain is unordered collection */
#define CERT_FLAG_PATHKLUDGE	0x20	/* Certificate is a PKIX path kludge */
#define CERT_FLAG_MAX			0x3F	/* Maximum possible flag value */

/* When creating RTCS responses from a request there are several subtypes
   that we can use based on a format specifier in the request.  When we turn
   the request into a response we check the format specifiers and record the
   response format as being one of the following */

typedef enum {
	RTCSRESPONSE_TYPE_NONE,				/* No response type */
	RTCSRESPONSE_TYPE_BASIC,			/* Basic response */
	RTCSRESPONSE_TYPE_EXTENDED,			/* Extended response */
	RTCSRESPONSE_TYPE_LAST				/* Last valid response type */
	} RTCSRESPONSE_TYPE;

/* Set the error locus and type.  This is used for certificate checking 
   functions that need to return extended error information but can't modify 
   the certificate information struct due to it either being a const 
   parameter or only being available via a handle so that setErrorInfo() 
   can't be used */

#define setErrorValues( locus, type ) \
		*errorLocus = ( locus ); *errorType = ( type )

/* The are several different classes of attributes that can be used 
   depending on the object that they're associated with.  The following 
   values are used to select the class of attribute that we want to work 
   with */

typedef enum {
	ATTRIBUTE_CERTIFICATE,				/* Certificate attributes */
	ATTRIBUTE_CMS,						/* CMS / S/MIME attributes */
	ATTRIBUTE_LAST						/* Last valid attribute type */
	} ATTRIBUTE_TYPE;

/* When checking policy constraints there are several different types of
   checking that we can apply depending on the presence of other constraints 
   in the issuing certificate(s) and the level of checking that we're 
   performing.  Policies can be optional, required, or a specific-policy 
   check that disallows the wildcard anyPolicy as a matching policy */

typedef enum {							/* Issuer		Subject		*/
	POLICY_NONE,						/*	 -			 -			*/
	POLICY_NONE_SPECIFIC,				/*	 -,  !any	 -,  !any	*/
	POLICY_SUBJECT,						/*	 -			yes			*/
	POLICY_SUBJECT_SPECIFIC,			/*	 -			yes, !any	*/
	POLICY_BOTH,						/*	yes			yes			*/
	POLICY_BOTH_SPECIFIC,				/*	yes, !any	yes, !any	*/
	POLICY_LAST							/* Last valid policy type */
	} POLICY_TYPE;

/* Selection options when working with DNs/GeneralNames in extensions.  These
   are used internally when handling user get/set/delete DN/GeneralName
   requests */

typedef enum {
	SELECTION_OPTION_NONE,	/* No selection option type */
	MAY_BE_ABSENT,			/* Component may be absent */
	MUST_BE_PRESENT,		/* Component must be present */
	CREATE_IF_ABSENT,		/* Create component if absent */
	SELECTION_OPTION_LAST	/* Last valid selection option type */
	} SELECTION_OPTION;

/* Certificate key check flags, used by checkKeyUsage().  These are:

	FLAG_NONE: No specific check.

	FLAG_CA: Certificate must contain a CA key.

	FLAG_PRIVATEKEY: Check for constraints on the corresponding private
			key's usage, not just the public key usage.

	FLAG_GENCHECK: Perform a general check that the key usage details are
			in order without checking for a particular usage */

#define CHECKKEY_FLAG_NONE			0x00	/* No specific checks */
#define CHECKKEY_FLAG_CA			0x01	/* Must be CA key */
#define CHECKKEY_FLAG_PRIVATEKEY	0x02	/* Check priv.key constraints */
#define CHECKKEY_FLAG_GENCHECK		0x04	/* General details check */
#define CHECKKEY_FLAG_MAX			0x07	/* Maximum possible flag value */

/* Before we encode a certificate object we have to perform various final 
   setup actions and check that the object is ready for encoding.  The 
   following setup operations can be requested by the caller via
   preEncodeCertificate():

	SET_ISSUERATTR: Copy issuer attributes to subject.

	SET_ISSUERDN: Copy issuer DN to subject.

	SET_REVINFO: Set up revocation information.

	SET_STANDARDATTR: Set up standard extensions/attributes.

	SET_VALIDITYPERIOD: Constrain subject validity to issuer validity.

	SET_VALINFO: Set up validity information */

#define PRE_SET_NONE			0x0000	/* No setup actions */
#define PRE_SET_STANDARDATTR	0x0001	/* Set up standard extensions */
#define PRE_SET_ISSUERATTR		0x0002	/* Copy issuer attr.to subject */
#define PRE_SET_ISSUERDN		0x0004	/* Copy issuer DN to subject */
#define PRE_SET_VALIDITYPERIOD	0x0008	/* Constrain subj.val.to issuer val.*/
#define PRE_SET_VALINFO			0x0010	/* Set up validity information */
#define PRE_SET_REVINFO			0x0020	/* Set up revocation information */

#define PRE_SET_FLAG_NONE		0x0000	/* No setup actions */
#define PRE_SET_FLAG_MAX		0x003F	/* Maximum possible flag value */

/* The following checks can be requested by the caller via 
   preCheckCertificate():

	CHECK_DN: Full subject DN is present.

	CHECK_DN_PARTIAL: Partial subject DN is present.  This is a DN template
		so the full DN doesn't have to be present (the CA can fill in the 
		rest later), only the CommonName.

	CHECK_ISSUERDN: Issuer DN is present.

	CHECK_ISSUERCERTDN: Issuer certificate's subject DN == subject 
		certificate's issuer DN.

	CHECK_NONSELFSIGNEDDN: Certificate's subject DN != certificate's issuer 
		DN, which would make it appear to be a self-signed certificate.

	CHECK_REVENTRIES: At least one revocation entry is present.

	CHECK_SERIALNO: Serial number is present.

	CHECK_SPKI: SubjectPublicKeyInfo is present.

	CHECK_VALENTRIES: At least one validity entry is present */

#define PRE_CHECK_NONE			0x0000	/* No check actions */
#define PRE_CHECK_SPKI			0x0001	/* SPKI present */
#define PRE_CHECK_DN			0x0002	/* Subject DN present */
#define PRE_CHECK_DN_PARTIAL	0x0004	/* Partial subject DN present */
#define PRE_CHECK_ISSUERDN		0x0008	/* Issuer DN present */
#define PRE_CHECK_ISSUERCERTDN	0x0010	/* Issuer cert DN == subj.issuer DN */
#define PRE_CHECK_NONSELFSIGNED_DN 0x0020	/* Issuer DN != subject DN */
#define PRE_CHECK_SERIALNO		0x0040	/* SerialNo present */
#define PRE_CHECK_VALENTRIES	0x0080	/* Validity entries present */
#define PRE_CHECK_REVENTRIES	0x0100	/* Revocation entries present */

#define PRE_CHECK_FLAG_NONE		0x0000	/* No check actions */
#define PRE_CHECK_FLAG_MAX		0x01FF	/* Maximum possible flag value */

/* Additional flags that control the checking operations indicated above */

#define PRE_FLAG_NONE			0x0000	/* No special control options */
#define PRE_FLAG_DN_IN_ISSUERCERT 0x0001/* Issuer DN is in issuer cert */
#define PRE_FLAG_MAX			0x0001	/* Maximum possible flag value */

/* The following checks can be requested by the caller via checkDN() */

#define CHECKDN_FLAG_NONE			0x00	/* No DN check */
#define CHECKDN_FLAG_COUNTRY		0x01	/* Check DN has C */
#define CHECKDN_FLAG_COMMONNAME		0x02	/* Check DN has CN */
#define CHECKDN_FLAG_WELLFORMED		0x04	/* Check DN is well-formed */
#define CHECKDN_FLAG_MAX			0x0F	/* Maximum possible flag value */

/****************************************************************************
*																			*
*							Certificate Element Tags						*
*																			*
****************************************************************************/

/* Context-specific tags for certificates */

enum { CTAG_CE_VERSION, CTAG_CE_ISSUERUNIQUEID, CTAG_CE_SUBJECTUNIQUEID,
	   CTAG_CE_EXTENSIONS };

/* Context-specific tags for attribute certificates */

enum { CTAG_AC_HOLDER_BASECERTIFICATEID, CTAG_AC_HOLDER_ENTITYNAME,
	   CTAG_AC_HOLDER_OBJECTDIGESTINFO };
enum { CTAG_AC_ISSUER_BASECERTIFICATEID, CTAG_AC_ISSUER_OBJECTDIGESTINFO };

/* Context-specific tags for certification requests */

enum { CTAG_CR_ATTRIBUTES };

/* Context-specific tags for CRLs */

enum { CTAG_CL_EXTENSIONS };

/* Context-specific tags for CRMF certification requests.  The second set of 
   tags is for POP of the private key */

enum { CTAG_CF_VERSION, CTAG_CF_SERIALNUMBER, CTAG_CF_SIGNINGALG,
	   CTAG_CF_ISSUER, CTAG_CF_VALIDITY, CTAG_CF_SUBJECT, CTAG_CF_PUBLICKEY,
	   CTAG_CF_ISSUERUID, CTAG_CF_SUBJECTUID, CTAG_CF_EXTENSIONS };
enum { CTAG_CF_POP_NONE, CTAG_CF_POP_SIGNATURE, CTAG_CF_POP_ENCRKEY };

/* Context-specific tags for RTCS responses */

enum { CTAG_RP_EXTENSIONS };

/* Context-specific tags for OCSP requests.  The second set of tags
   is for each request entry in an overall request */

enum { CTAG_OR_VERSION, CTAG_OR_DUMMY, CTAG_OR_EXTENSIONS };
enum { CTAG_OR_SR_EXTENSIONS };

/* Context-specific tags for OCSP responses */

enum { CTAG_OP_VERSION, CTAG_OP_EXTENSIONS };

/* Context-specific tags for CMS attributes */

enum { CTAG_SI_AUTHENTICATEDATTRIBUTES };

/****************************************************************************
*																			*
*							Certificate Data Structures						*
*																			*
****************************************************************************/

/* Symbolic defines for data types handled in the certificate attribute and
   DN handling code.  The ATTRIBUTE_PTR_STORAGE value is used in debug 
   macros that check that an ATTRIBUTE_PTR points to at least 
   ATTRIBUTE_PTR_STORAGE  bytes of readable/writeable memory */

#define ATTRIBUTE_PTR			void
#define ATTRIBUTE_PTR_STORAGE	char[ 64 ]
#define DN_PTR					void
#define DN_PTR_STORAGE			char[ 32 ]

/* The structure to hold information on the current selection of attribute/
   GeneralName/DN data used when adding/reading/deleting certificate 
   components.  The usage of this information is too complex to explain 
   here, see the comments at the start of comp_get.c for more details.
   
   Note that the DN pointer stores a pointer to the head of the list of DN
   elements rather than storing the list head itself since we need to pass 
   it to functions that may modify the list head.  So dnPtr may contain
   (for example) &certInfoPtr->subjectDN so that it can be passed to 
   functions like deleteDnComponent() which may have to modify the list 
   head if that's the entry that they're being asked to delete.  Storing a
   copy of certInfoPtr->subjectDN in dnPtr and then passing &dnPtr to
   deleteDnComponent() would update dnPtr but leave certInfoPtr->subjectDN 
   with a dangling reference to the new-deleted DN element */

typedef struct {
	DN_PTR **dnPtr;						/* Pointer to address of current DN */
	CRYPT_ATTRIBUTE_TYPE generalName;	/* Selected GN */
	CRYPT_ATTRIBUTE_TYPE dnComponent;	/* Selected component of DN */
	int dnComponentCount;				/* Iterator for DN components */
	BOOLEAN dnInExtension;				/* Whether DN is in extension */
	BOOLEAN updateCursor;				/* Whether to upate attr.cursor */
	} SELECTION_INFO;

#define initSelectionInfo( certInfoPtr ) \
	memset( &( certInfoPtr )->currentSelection, 0, sizeof( SELECTION_INFO ) ); \
	( certInfoPtr )->currentSelection.dnPtr = &( ( certInfoPtr )->subjectName )

/* Sometimes we need to manipulate an internal component which is addressed
   indirectly as a side-effect of some other processing operation.  We can't
   change the selection information for the certificate object since this will 
   affect any future operations that the user performs so we provide the 
   following macros to save and restore the selection state around these 
   operations */

typedef struct {
	int savedChainPos;					/* Current cert.chain position */
	SELECTION_INFO savedSelectionInfo;	/* Current DN/GN selection info */
	ATTRIBUTE_PTR *savedAttributeCursor;/* Atribute cursor pos.*/
	} SELECTION_STATE;

#define saveSelectionState( savedState, certInfoPtr ) \
	{ \
	memset( &( savedState ), 0, sizeof( SELECTION_STATE ) ); \
	if( ( certInfoPtr )->type == CRYPT_CERTTYPE_CERTCHAIN ) \
		( savedState ).savedChainPos = ( certInfoPtr )->cCertCert->chainPos; \
	( savedState ).savedSelectionInfo = ( certInfoPtr )->currentSelection; \
	( savedState ).savedAttributeCursor = ( certInfoPtr )->attributeCursor; \
	}

#define restoreSelectionState( savedState, certInfoPtr ) \
	{ \
	if( ( certInfoPtr )->type == CRYPT_CERTTYPE_CERTCHAIN ) \
		( certInfoPtr )->cCertCert->chainPos = ( savedState ).savedChainPos; \
	( certInfoPtr )->currentSelection = ( savedState ).savedSelectionInfo; \
	( certInfoPtr )->attributeCursor = ( savedState ).savedAttributeCursor; \
	}

/* The structure used to store the current policy set if we're doing full
   policy checking, enabled for CERTLEVEL_PKIX_FULL.  The MAX_POLICY_SIZE
   value corresponds to MAX_OID_SIZE, unfortunately this define isn't 
   visible at this level so we have to specify the value explicitly (the 
   length is checked on read so there's no chance of an overflow).
   
   When we store the policy value (which is just the encoded policy OID) we
   also store the level in the certificate chain at which it was added
   (since a policy is only valid below the point at which it was added) and
   an indication of whether the policy is explicit or mapped, since the 
   latter can be disabled by the inhibitPolicyMapping value */

#if defined( USE_CERTLEVEL_PKIX_FULL )

#define MAX_POLICIES			16
#define MAX_POLICY_SIZE			32

typedef struct {
	BYTE data[ MAX_POLICY_SIZE + 8 ];
	int length;					/* Policy value and length */
	int level;					/* Pos.in chain at which pol.becomes valid */
	BOOLEAN isMapped;			/* Whether this is a mapped policy */
	} POLICY_DATA;

typedef struct {
	POLICY_DATA policies[ MAX_POLICIES + 4 ];
	int noPolicies;
	} POLICY_INFO;
#endif /* USE_CERTLEVEL_PKIX_FULL */

#ifdef USE_CERTVAL

/* The structure to hold a validity information entry */

typedef struct VI {
	/* Certificate ID information */
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE data[ KEYID_SIZE + 8 ];
	int dCheck;						/* Data checksum for quick match */

	/* Validity information */
	BOOLEAN status;					/* Valid/not valid */
	int extStatus;					/* Extended validity status */
	time_t invalidityTime;			/* Certificate invalidity time */

	/* Per-entry attributes.  These are a rather ugly special case for the
	   user because unlike the attributes for all other certificate objects 
	   where cryptlib can provide the illusion of a flat type<->value 
	   mapping there can be multiple sets of identical per-entry attributes 
	   present if there are multiple RTCS entries present */
	ATTRIBUTE_PTR *attributes;		/* RTCS entry attributes */
	int attributeSize;				/* Encoded size of attributes */

	/* The next element in the linked list of elements */
	struct VI *next;
	} VALIDITY_INFO;

typedef struct {
	/* A list of RTCS request or response entries and a pointer to the
	   request/response which is currently being accessed */
	VALIDITY_INFO *validityInfo;	/* List of validity info */
	VALIDITY_INFO *currentValidity;	/* Currently selected validity info */

	/* The URL for the RTCS responder */
	BUFFER_OPT_FIXED( responderUrlSize ) \
	char *responderUrl;				/* RTCS responder URL */
	int responderUrlSize;

	/* Since RTCS allows for a variety of response types, we include an
	   indication of the request/response format */
	RTCSRESPONSE_TYPE responseType;	/* Request/response format */
	} CERT_VAL_INFO;
#endif /* USE_CERTVAL */

#ifdef USE_CERTREV

/* The structure to hold a revocation information entry, either a CRL entry
   or OCSP request/response information */

typedef struct RI {
	/* Certificate ID information, either a serial number (for CRLs) or a
	   certificate hash or issuerID (for OCSP requests/responses).  In 
	   addition this could also be a pre-encoded OCSP certID, which is 
	   treated as an opaque blob of type CRYPT_ATTRIBUTE_NONE since it can't 
	   be used in any useful way.  If we're using OCSP and an alternative ID 
	   is supplied as an ESSCertID we point to this value (inside the 
	   ESSCertID) in the altIdPtr field.  Since we store this as a varstruct
	   whose data is expected to be held in a field called 'value' we define
	   a field of that name alongside the more obvious 'id' and set 'id' to
	   'value' when we initialise the structure */
	CRYPT_KEYID_TYPE idType;		/* ID type */
	BUFFER_OPT_FIXED( idLength ) \
	BYTE *id, *value;				/* ID information stored in varstruct */
	int idLength;					/*  (see comment above for 'value') */
	int idCheck;					/* Data checksum for quick match */
	CRYPT_KEYID_TYPE altIdType;		/* Alt.ID type for OCSP */
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE altID[ KEYID_SIZE + 8 ];	/* Alt.ID for OCSP */

	/* Revocation information */
	int status;						/* OCSP revocation status */
	time_t revocationTime;			/* Certificate revocation time */

	/* Per-entry attributes.  These are a rather ugly special case for the
	   user because unlike the attributes for all other certificate objects 
	   where cryptlib can provide the illusion of a flat type<->value 
	   mapping there can be multiple sets of identical per-entry attributes 
	   present if there are multiple CRL/OCSP entries present */
	ATTRIBUTE_PTR *attributes;		/* CRL/OCSP entry attributes */
	int attributeSize;				/* Encoded size of attributes */

	/* The next element in the linked list of elements */
	struct RI *next;

	/* Variable-length storage for the ID information */
	DECLARE_VARSTRUCT_VARS;
	} REVOCATION_INFO;

typedef struct {
	/* The list of revocations for a CRL or a list of OCSP request or response
	   entries, and a pointer to the revocation/request/response which is
	   currently being accessed */
	REVOCATION_INFO *revocations;	/* List of revocations */
	REVOCATION_INFO *currentRevocation;	/* Currently selected revocation */

	/* The default revocation time for a CRL, used for if no explicit time
	   is set for a revocation */
	time_t revocationTime;			/* Default certificate revocation time */

	/* The URL for the OCSP responder */
	BUFFER_OPT_FIXED( responderUrlSize ) \
	char *responderUrl;
	int responderUrlSize;			/* OCSP responder URL */

	/* The hash algorithm used to sign the certificate.  Although it's a 
	   part of the signature, a second copy of the algorithm ID is embedded 
	   inside the signed certificate data because of a theoretical attack 
	   that doesn't actually work with any standard signature padding
	   technique.  Because of this we have to record the hash algorithm ID
	   when we read/write the certificate so that we can write it again when
	   we re-encode the certificate */
	CRYPT_ALGO_TYPE hashAlgo;

	/* Signed OCSP requests can include varying levels of detail in the
	   signature.  The following value determines how much information is
	   included in the signature */
	CRYPT_SIGNATURELEVEL_TYPE signatureLevel;
	} CERT_REV_INFO;
#endif /* USE_CERTREV */

#ifdef USE_CERTREQ

typedef struct {
	/* The certificate serial number, used when requesting a revocation by 
	   issuerAndSerialNumber.  This is stored in the buffer if it fits (it
	   almost always does), otherwise in a dynamically-allocated buffer */
	BUFFER( SERIALNO_BUFSIZE, serialNumberLength ) \
	BYTE serialNumberBuffer[ SERIALNO_BUFSIZE + 8 ];
	BUFFER_OPT_FIXED( serialNumberLength ) \
	void *serialNumber;
	int serialNumberLength;			/* Certificate serial number */

	/* The certificate ID of the PKI user or certificate that authorised 
	   this request.  This is from an external source, supplied when the 
	   request is used as part of the CMP protocol */
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE authCertID[ KEYID_SIZE + 8 ];
	} CERT_REQ_INFO;
#endif /* USE_CERTREQ */

#ifdef USE_PKIUSER

typedef struct {
	/* The authenticator used for authenticating certificate issue and
	   revocation requests */
	BUFFER_FIXED( 16 ) \
	BYTE pkiIssuePW[ 16 + 8 ];
	BUFFER_FIXED( 16 ) \
	BYTE pkiRevPW[ 16 + 8 ];
	} CERT_PKIUSER_INFO;
#endif /* USE_PKIUSER */

/* The internal fields in a certificate that hold subtype-specific data for 
   the various certificate object types */

typedef struct {
	/* The certificate serial number.  This is stored in the buffer if it 
	   fits (it almost always does), otherwise in a dynamically-allocated 
	   buffer */
	BUFFER( SERIALNO_BUFSIZE, serialNumberLength ) \
	BYTE serialNumberBuffer[ SERIALNO_BUFSIZE + 8 ];
	BUFFER_OPT_FIXED( serialNumberLength ) \
	void *serialNumber;
	int serialNumberLength;			/* Certificate serial number */

	/* The highest compliance level at which a certificate has been checked.
	   We have to record this high water-mark level because increasing the
	   compliance level may invalidate an earlier check performed at a lower
	   level */
	int maxCheckLevel;

	/* The allowed usage for a certificate can be further controlled by the
	   user.  The trustedUsage value is a mask which is applied to the key
	   usage extension to further constrain usage, alongside this there is
	   an additional implicit trustImplicit value that acts as a boolean 
	   flag and indicates whether the user implicitly trusts this 
	   certificate (without requiring further checking upstream).  This 
	   value isn't stored with the certificate since it's a property of any 
	   instantiation of the certificate rather than just the current one so 
	   when the user queries it it's obtained dynamically from the trust 
	   manager */
	int trustedUsage;

	/* Certificate chains are a special variant of standard certificates, 
	   being complex container objects that contain further certificates 
	   leading up to a CA root certificate.  The reason why they're combined 
	   with standard certificates is because when we're building a chain 
	   from a certificate collection or assembling it from a certificate 
	   source we can't tell at the time of certificate creation which 
	   certificate will be the leaf certificate so that any certificate 
	   potentially has to be able to act as the chain container (another way 
	   of looking at this is that all standard certificates are a special 
	   case of a chain with a length of one).

	   A possible alternative to this way of handling chains is to make the
	   chain object a pure container object used only to hold pointers to
	   the actual certificates, but this requires an extra level of 
	   indirection every time that a certificate chain object is used since 
	   in virtually all cases what'll be used is the leaf certificate with 
	   which the chain-as-standard-certificate model is the default 
	   certificate but with the chain-as-container model requires an extra 
	   object dereference to obtain.

	   In theory we should use a linked list to store chains but since the
	   longest chain ever seen in the wild has a length of 4 using a fixed
	   maximum length several times this size shouldn't be a problem.  The
	   certificates in the chain are ordered from the parent of the leaf 
	   certificate up to the root certificate with the leaf certificate 
	   corresponding to the [-1]th entry in the list.  We also maintain a 
	   current position in the certificate chain that denotes the 
	   certificate in the chain that will be accessed by the 
	   component-manipulation functions.  This is set to CRYPT_ERROR if the 
	   current certificate is the leaf certificate */
	ARRAY( MAX_CHAINLENGTH, chainEnd ) \
	CRYPT_CERTIFICATE chain[ MAX_CHAINLENGTH + 8 ];
	int chainEnd;					/* Length of certificate chain */
	int chainPos;					/* Currently selected certificate in chain */

	/* The hash algorithm used to sign the certificate.  Although it's a 
	   part of the signature, a second copy of the algorithm ID is embedded 
	   inside the signed certificate data because of a theoretical attack 
	   that doesn't actually work with any standard signature padding
	   technique */
	CRYPT_ALGO_TYPE hashAlgo;

#ifdef USE_CERT_OBSOLETE 
	/* The (deprecated) X.509v2 unique ID */
	BUFFER_OPT_FIXED( issuerUniqueIDlength ) \
	void *issuerUniqueID;
	BUFFER_OPT_FIXED( subjectUniqueIDlength ) \
	void *subjectUniqueID;
	int issuerUniqueIDlength, subjectUniqueIDlength;
#endif /* USE_CERT_OBSOLETE */
	} CERT_CERT_INFO;

/* Defines to make access to the union fields less messy */

#define cCertCert		certInfo.certInfo
#define cCertReq		certInfo.reqInfo
#define cCertRev		certInfo.revInfo
#define cCertVal		certInfo.valInfo
#define cCertUser		certInfo.pkiUserInfo

/* The structure that stores information on a certificate object */

typedef struct {
	/* General certificate information */
	CRYPT_CERTTYPE_TYPE type;		/* Certificate type */
	int flags;						/* Certificate flags */
	int version;					/* Certificate format version */

	/* Certificate type-specific information */
	union {
		CERT_CERT_INFO *certInfo;
#ifdef USE_CERTREQ
		CERT_REQ_INFO *reqInfo;
#endif /* USE_CERTREQ */
#ifdef USE_CERTREV
		CERT_REV_INFO *revInfo;
#endif /* USE_CERTREV */
#ifdef USE_CERTVAL
		CERT_VAL_INFO *valInfo;
#endif /* USE_CERTVAL */
#ifdef USE_PKIUSER
		CERT_PKIUSER_INFO *pkiUserInfo;
#endif /* USE_PKIUSER */
		} certInfo;

	/* The encoded certificate object.  We save this when we import it
	   because there are many different interpretations of how a certificate 
	   should be encoded and if we parse and then try to re-encode the 
	   object the signature check would most likely fail */
	BUFFER_OPT_FIXED( certificateSize ) \
	void *certificate;
	int certificateSize;

	/* The public key associated with the certificate.  When the 
	   certificate is in the low (unsigned state) this consists of the 
	   encoded public-key data and associated attributes.  When the 
	   certificate is in the high (signed) state, either by being imported 
	   from an external source or by being signed by cryptlib, this consists 
	   of a public-key context.  In addition some certificates are imported 
	   as data-only certificates, denoted by CERT_FLAG_DATAONLY being set.  
	   These constitute a container object that contain no public-key context 
	   and are used for certificate chains (when read from a trusted source) 
	   and to store certificate information associated with a private-key 
	   context.  Since it's not known during the import stage whether a 
	   certificate in a chain will be a data-only or standard certificate 
	   (it's not known which certificate is the leaf certificate until the 
	   entire chain has been processed) certificate chains from a trusted 
	   source are imported as data-only certificates and then the leaf has 
	   its context instantiated */
	CRYPT_CONTEXT iPubkeyContext;	/* Public-key context */
	CRYPT_ALGO_TYPE publicKeyAlgo;	/* Key algorithm */
	int publicKeyFeatures;			/* Key features flags */
	BUFFER_OPT_FIXED( publicKeyInfoSize ) \
	void *publicKeyInfo;			/* Encoded key information */
	int publicKeyInfoSize;
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE publicKeyID[ KEYID_SIZE + 8 ];	/* Key ID */

	/* General certificate object information */
	DN_PTR *issuerName;				/* Issuer name */
	DN_PTR *subjectName;			/* Subject name */
	time_t startTime;				/* Validity start or update time */
	time_t endTime;					/* Validity end or next update time */

	/* In theory we can just copy the subject DN of a CA certificate into 
	   the issuer DN of a subject certificate, however due to broken 
	   implementations this will break chaining if we correct any problems 
	   in the DN.  Because of this we need to preserve a copy of the 
	   certificate's subject DN so that we can write it as a blob to the 
	   issuer DN field of any certificates that it signs.  We also need to 
	   remember the encoded issuer DN so that we can chain upwards.  The 
	   following fields identify the size and location of the encoded DNs 
	   inside the encoded certificate object */
	BUFFER_OPT_FIXED( subjectDNsize ) \
	void *subjectDNptr;
	BUFFER_OPT_FIXED( issuerDNsize ) \
	void *issuerDNptr;					/* Pointer to encoded DN blobs */
	int subjectDNsize, issuerDNsize;	/* Size of encoded DN blobs */

	/* For some objects the public key and/or subject DN and/or issuer DN are
	   copied in from an external source before the object is signed so we
	   can't just point the issuerDNptr at the encoded object, we have to
	   allocate a separate data area to copy the DN into.  This is used in
	   cases where we don't copy in a full subject/issuerName but only use
	   an encoded DN blob for the reasons described above */
	BUFFER_OPT_FIXED( publicKeyInfoSize ) \
	void *publicKeyData;
	BUFFER_OPT_FIXED( subjectDNsize ) \
	void *subjectDNdata;
	BUFFER_OPT_FIXED( issuerDNsize ) \
	void *issuerDNdata;

	/* The certificate hash/fingerprint/oobCertID/thumbprint/whatever.  This
	   is used so frequently that it's cached here for future re-use */
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE certHash[ KEYID_SIZE + 8 ];/* Cached certificate hash */
	BOOLEAN certHashSet;			/* Whether hash has been set */

	/* Certificate object attributes and a cursor into the attribute list.
	   This can be moved by the user on a per-attribute, per-field, and per-
	   component basis */
	ATTRIBUTE_PTR *attributes, *attributeCursor;

	/* The currently selected GeneralName and DN.  A certificate can contain 
	   multiple GeneralNames and DNs that can be selected by their field 
	   types after which adding DN components will affected the currently 
	   selected DN.  This value contains the details of the currently 
	   selected GeneralName and DN */
	SELECTION_INFO currentSelection;

	/* Save area for the currently selected GeneralName and DN, and position
	   in the certificate chain.  The current values are saved to this area 
	   when the object receives a lock object message and restored when the 
	   object receives the corresponding unlock message.  This guarantees 
	   that any changes made during processing while the certificate is 
	   locked don't get reflected back to external users */
	SELECTION_STATE selectionState;

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
	} CERT_INFO;

/* Certificate read/write methods for the different format types.  
   Specifying input ranges for the process gets a bit complicated because 
   the functions are polymorphic so we have to use the lowest common 
   denominator of all of the functions */

typedef CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *READCERT_FUNCTION )( INOUT STREAM *stream, 
									INOUT CERT_INFO *certInfoPtr );
typedef CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *WRITECERT_FUNCTION )( INOUT STREAM *stream, 
									 INOUT CERT_INFO *subjectCertInfoPtr,
									 IN_OPT const CERT_INFO *issuerCertInfoPtr,
									 IN_HANDLE_OPT \
										const CRYPT_CONTEXT iIssuerCryptContext );

CHECK_RETVAL_PTR \
READCERT_FUNCTION getCertReadFunction( IN_ENUM( CRYPT_CERTTYPE ) \
										const CRYPT_CERTTYPE_TYPE certType );
CHECK_RETVAL_PTR \
WRITECERT_FUNCTION getCertWriteFunction( IN_ENUM( CRYPT_CERTTYPE ) \
											const CRYPT_CERTTYPE_TYPE certType );

/****************************************************************************
*																			*
*							Attribute Selection Macros						*
*																			*
****************************************************************************/

/* Determine whether a component which is being added to a certificate is a 
   special-case DN selection component that selects the current DN without 
   changing the certificate itself or a GeneralName selection component.  
   The latter is sufficiently complex (GeneralNames are used almost 
   everywhere in certificates where a basic text string would do) that we
   break it out into a custom function */

#define isDNSelectionComponent( certInfoType ) \
	( ( certInfoType ) == CRYPT_CERTINFO_ISSUERNAME || \
	  ( certInfoType ) == CRYPT_CERTINFO_SUBJECTNAME || \
	  ( certInfoType ) == CRYPT_CERTINFO_DIRECTORYNAME )

CHECK_RETVAL_BOOL \
BOOLEAN isGeneralNameSelectionComponent( IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE certInfoType );

/* Determine whether a component which is being added is a DN or GeneralName
   component */

#define isDNComponent( certInfoType ) \
	( ( certInfoType ) >= CRYPT_CERTINFO_FIRST_DN && \
	  ( certInfoType ) <= CRYPT_CERTINFO_LAST_DN )

#define isGeneralNameComponent( certInfoType ) \
	( ( certInfoType ) >= CRYPT_CERTINFO_FIRST_GENERALNAME && \
	  ( certInfoType ) <= CRYPT_CERTINFO_LAST_GENERALNAME )

/* Determine whether a component which is being added is pseudo-information
   that corresponds to certificate control information rather than a normal
   certificate attribute */

#define isPseudoInformation( certInfoType ) \
	( ( certInfoType ) >= CRYPT_CERTINFO_FIRST_PSEUDOINFO && \
	  ( certInfoType ) <= CRYPT_CERTINFO_LAST_PSEUDOINFO )

/* Determine whether a component which is being added to a validity/
   revocation check request/response is a standard attribute or a per-entry
   attribute */

#define isRevocationEntryComponent( certInfoType ) \
	( ( certInfoType ) == CRYPT_CERTINFO_CRLREASON || \
	  ( certInfoType ) == CRYPT_CERTINFO_HOLDINSTRUCTIONCODE || \
	  ( certInfoType ) == CRYPT_CERTINFO_INVALIDITYDATE )

/* Check whether an entry in an attribute list is valid.  This checks 
   whether the entry has a non-zero attribute ID, denoting a non blob-type 
   attribute */

#define isValidAttributeField( attributePtr ) \
		( ( attributePtr )->attributeID > 0 )

/****************************************************************************
*																			*
*							Certificate Functions 							*
*																			*
****************************************************************************/

/* The huge complexity of the certificate management code means that there
   are a sufficient number of functions required that we confine the
   prototypes to their own file */

#if defined( INC_ALL )
  #include "certfn.h"
#else
  #include "cert/certfn.h"
#endif /* Compiler-specific includes */

#endif /* _CERT_DEFINED */
