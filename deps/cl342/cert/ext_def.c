/****************************************************************************
*																			*
*						Certificate Attribute Definitions					*
*						Copyright Peter Gutmann 1996-2011					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

/* The following certificate extensions are currently supported.  If
   'Enforced' is set to 'Yes' then they're constraint extensions that are 
   enforced by the certificate checking code; if set to '-' then they're
   informational extensions for which enforcement doesn't apply; if set to 
   'No' they need to be handled by the user (this only applies for 
   certificate policies for which the user has to decide whether a given 
   policy is acceptable or not).  The Yes/No in policyConstraints means 
   that everything except the policy mapping constraint is enforced 
   (because policyMappings itself isn't enforced).  The 'level' value 
   indicates the compliance level at which the extension is decoded and
   enforced.  The 'conditions' value indicates any further conditions under
   which the extension may be applied or skipped.  PKIX = 'Full' overrides
   Conditions = 'Obscure', so that setting PKIX compliance to Full enables
   handling of all PKIX/X.509 extensions no matter how obscure and strange
   they are.

									Enforced	Level		Conditions
									--------	-----		----------
	aaIssuingDistributionPoint		   -		 Full		   Rev
	additionalInformation (SigG)	   -		Partial		 Obscure
	admissions (SigG)				   -		Partial		 Obscure
	authorityInfoAccess				   -		  -				-
	authorityKeyIdentifier			   -		Partial			-
	autonomousSysIds (RPKI)			   -		  -				-
	basicConstraints				  Yes		  -				-
	biometricInfo (QualifiedCert)	   -		 Full			-
	certCardRequired (SET)			   -		Partial		 Obsolete
	certHash (SigG, OCSP)			   -		Partial		 Obscure, Rev
	certificateIssuer				   -		 Full		   Rev
	certificatePolicies				  Yes		  -				-
	certificateType (SET)			   -		Partial		 Obsolete
	challengePassword (SCEP)		   -		  -			   Req
	cRLDistributionPoints			   -		  -				-
	cRLNumber						   -		Partial		   Rev
	cRLReason						   -		  -			Rev | Req
	cRLExtReason					   -		  -			   Rev
	crlStreamIdentifier				   -		 Full		   Rev
	dateOfCertGen (SigG)			   -		Partial		 Obscure
	declarationOfMajority (SigG)	   -		Partial		 Obscure
	deltaCRLIndicator				   -		Partial		   Rev
	deltaInfo						   -		 Full		   Rev
	expiredCertsOnCRL				   -		 Full		   Rev
	extKeyUsage						  Yes		  -				-
	freshestCRL						   -		 Full		   Rev
	hashedRootKey (SET)				   -		Partial		 Obsolete
	holdInstructionCode				   -		Partial		Rev | Req
	inhibitAnyPolicy				  Yes		 Full			-
	invalidityDate					   -		  -			Rev | Req
	ipAddrBlocks (RPKI)				   -		  -				-
	issuerAltName					   -		  -				-
	issuingDistributionPoint		   -		Partial		   Rev
	keyFeatures						   -		  -				-
	keyUsage						  Yes		  -				-
	monetaryLimit (SigG)			   -		Partial		 Obscure
	nameConstraints					  Yes		 Full			-
	netscape-cert-type				  Yes		  -			 Obsolete
	netscape-base-url				   -		  -			 Obsolete
	netscape-revocation-url			   -		  -			 Obsolete
	netscape-ca-revocation-url		   -		  -			 Obsolete
	netscape-cert-renewal-url		   -		  -			 Obsolete
	netscape-ca-policy-url			   -		  -			 Obsolete
	netscape-ssl-server-name		   -		  -			 Obsolete
	netscape-comment				   -		  -			 Obsolete
	merchantData (SET)				   -		Partial		 Obsolete
	ocspAcceptableResponse (OCSP)	   -		Partial		   Rev
	ocspArchiveCutoff (OCSP)		   -		Partial		   Rev
	ocspNoCheck (OCSP)				   -		Partial		   Rev
	ocspNonce (OCSP)				   -		  -			   Rev
	orderedList						   -		 Full		   Rev
	policyConstraints				 Yes/No		 Full			-
	policyMappings					  No		 Full			-
	privateKeyUsagePeriod			  Yes		Partial			-
	procuration (SigG)				   -		Partial		 Obscure
	qcStatements (QualifiedCert)	   -		 Full			-
	restriction (SigG)				   -		Partial		 Obscure
	revokedGroups					   -		 Full		   Rev
  [	signingCertificate				   -		  -			   Rev ]
	strongExtranet (Thawte)			   -		Partial		 Obsolete
	subjectAltName					   -		  -				-
	subjectDirectoryAttributes		   -		Partial			-
	subjectInfoAccess				   -		  -				-
	subjectKeyIdentifier			   -		  -				-
	toBeRevoked						   -		 Full		   Rev
	tunneling (SET)					   -		Partial		 Obsolete

   Of these extensions, only a very small number are permitted in certificate 
   requests for security reasons, see the code comment for 
   sanitiseCertAttributes() in comp_set.c before changing any of these 
   values.

   Since some extensions fields are tagged the fields as encoded differ from
   the fields as defined by the tagging, the following macro is used to turn
   a small integer into a context-specific tag.  By default the tag is
   implicit as per X.509v3, to make it an explicit tag we need to set the
   FL_EXPLICIT flag for the field */

#define CTAG( x )			( x | BER_CONTEXT_SPECIFIC )

/* Symbolic defines to make the encoded forms more legible */

#define ENCODING( tag )		BER_##tag, CRYPT_UNUSED
#define ENCODING_SPECIAL( value ) \
							FIELDTYPE_##value, CRYPT_UNUSED
#define ENCODING_TAGGED( tag, outerTag ) \
							BER_##tag, outerTag
#define ENCODING_SPECIAL_TAGGED( tag, outerTag ) \
							FIELDTYPE_##tag, outerTag
#define RANGE( min, max )	min, max, 0, NULL
#define RANGE_ATTRIBUTEBLOB	1, MAX_ATTRIBUTE_SIZE, 0, NULL
#define RANGE_BLOB			16, MAX_ATTRIBUTE_SIZE, 0, NULL
#define RANGE_BOOLEAN		FALSE, TRUE, FALSE, NULL 
#define RANGE_NONE			0, 0, 0, NULL
#define RANGE_OID			MIN_OID_SIZE, MAX_OID_SIZE, 0, NULL
#define RANGE_TEXTSTRING	1, CRYPT_MAX_TEXTSIZE, 0, NULL
#define RANGE_TIME			sizeof( time_t ), sizeof( time_t ), 0, NULL
#define RANGE_UNUSED		CRYPT_UNUSED, CRYPT_UNUSED, 0, NULL
#define ENCODED_OBJECT( altEncodingTable ) \
							0, 0, 0, ( void * ) altEncodingTable
#define CHECK_DNS			MIN_DNS_SIZE, MAX_DNS_SIZE, 0, ( void * ) checkDNS
#define CHECK_HTTP			MIN_URL_SIZE, MAX_URL_SIZE, 0, ( void * ) checkHTTP
#define CHECK_RFC822		MIN_RFC822_SIZE, MAX_RFC822_SIZE, 0, ( void * ) checkRFC822
#define CHECK_URL			MIN_URL_SIZE, MAX_URL_SIZE, 0, ( void * ) checkURL
#define CHECK_X500			0, 0, 0, ( void * ) checkDirectoryName

/* Symbolic defines for attribute type information */

#define ATTR_TYPEINFO( type, level ) \
							( FL_LEVEL_##level | FL_VALID_##type | \
							  FL_ATTR_ATTRSTART )
#define ATTR_TYPEINFO2( type1, type2, level ) \
							( FL_LEVEL_##level | FL_VALID_##type1 | \
							  FL_VALID_##type2 | FL_ATTR_ATTRSTART )
#define ATTR_TYPEINFO3( type1, type2, type3, level ) \
							( FL_LEVEL_##level | FL_VALID_##type1 | \
							  FL_VALID_##type2 | FL_VALID_##type3 | \
							  FL_ATTR_ATTRSTART )
#define ATTR_TYPEINFO_CRITICAL( type, level ) \
							( FL_LEVEL_##level | FL_VALID_##type | \
							  FL_ATTR_CRITICAL | FL_ATTR_ATTRSTART )
#define ATTR_TYPEINFO2_CRITICAL( type1, type2, level ) \
							( FL_LEVEL_##level | FL_VALID_##type1 | \
							  FL_VALID_##type2 | FL_ATTR_CRITICAL | \
							  FL_ATTR_ATTRSTART )
#define ATTR_TYPEINFO3_CRITICAL( type1, type2, type3, level ) \
							( FL_LEVEL_##level | FL_VALID_##type1 | \
							  FL_VALID_##type2 | FL_VALID_##type3 | \
							  FL_ATTR_CRITICAL | FL_ATTR_ATTRSTART )
#define ATTR_TYPEINFO_CMS	( FL_ATTR_ATTRSTART )

/* Extended checking functions */

CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkRFC822( const ATTRIBUTE_LIST *attributeListPtr );
CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDNS( const ATTRIBUTE_LIST *attributeListPtr );
CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkURL( const ATTRIBUTE_LIST *attributeListPtr );
#ifdef USE_CERT_OBSOLETE
CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkHTTP( const ATTRIBUTE_LIST *attributeListPtr );
#endif /* USE_CERT_OBSOLETE */
CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDirectoryName( const ATTRIBUTE_LIST *attributeListPtr );

/* Forward declarations for alternative encoding tables used by the main 
   tables.  These are declared in a somewhat peculiar manner because there's 
   no clean way in C to forward declare a static array.  Under VC++ with the 
   highest warning level enabled this produces a compiler warning, so we 
   turn the warning off for this module.  In addition there the usual 
   problems that crop up with gcc 4.x */

#if defined( __GNUC__ ) && ( __GNUC__ >= 4 )
  static const ATTRIBUTE_INFO FAR_BSS generalNameInfo[];
  static const ATTRIBUTE_INFO FAR_BSS holdInstructionInfo[];
  static const ATTRIBUTE_INFO FAR_BSS contentTypeInfo[];
#else
  extern const ATTRIBUTE_INFO FAR_BSS generalNameInfo[];
  extern const ATTRIBUTE_INFO FAR_BSS holdInstructionInfo[];
  extern const ATTRIBUTE_INFO FAR_BSS contentTypeInfo[];
#endif /* gcc 4.x */

#if defined( _MSC_VER )
  #pragma warning( disable: 4211 )
#endif /* VC++ */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*						Certificate Extension Definitions					*
*																			*
****************************************************************************/

/* Certificate extensions are encoded using the following table */

static const ATTRIBUTE_INFO FAR_BSS extensionInfo[] = {
#ifdef USE_CERTREQ
	/* challengePassword:

		OID = 1 2 840 113549 1 9 7
		DirectoryString.

	   This is here even though it's a CMS attribute because SCEP stuffs it 
	   into PKCS #10 requests */
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x07" ), CRYPT_CERTINFO_CHALLENGEPASSWORD,
	  DESCRIPTION( "challengePassword" )
	  ENCODING_SPECIAL( TEXTSTRING ),
	  ATTR_TYPEINFO( CERTREQ, STANDARD ) | FL_ATTR_ATTREND | FL_ATTR_NOCOPY, 
	  FL_SPECIALENCODING, RANGE_TEXTSTRING },
#endif /* USE_CERTREQ */

#if defined( USE_CERTREV )
	/* signingCertificate:

		OID = 1 2 840 113549 1 9 16 2 12
		SEQUENCE {
			SEQUENCE OF ESSCertID {
				certHash		OCTET STRING
				},							-- SIZE(1)
			SEQUENCE OF { ... } OPTIONAL	-- ABSENT
			} 

	   This is here even though it's a CMS attribute because it's required 
	   in order to make OCSP work (a second copy is present with the CMS
	   attributes, see the remainder of this comment below).  Since OCSP 
	   breaks up the certificate identification information into bits and 
	   pieces and hashes some while leaving others intact, there's no way to 
	   map what arrives at the responder back into a certificate without an 
	   ability to reverse the cryptographic hash function.  To work around 
	   this we include an ESSCertID in the request that properly identifies 
	   the certificate being queried.  Since it's a limited-use version that 
	   only identifies the certificate we don't allow a full 
	   signingCertificate extension but only a single ESSCertID.  
	   
	   Note that having this attribute here is somewhat odd in that type-
	   checking is done via the equivalent entry in the CMS attributes 
	   because the fieldID identifies it as a CMS attribute but decoding is 
	   done via this entry because the decoder loops through the certificate 
	   attribute entries to find the decoding information.  For this reason 
	   if OCSP use is enabled then both this entry and the one in the CMS 
	   attributes must be enabled */
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x0C" ), CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE,
	  DESCRIPTION( "signingCertificate" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( OCSPREQ /*Per-entry*/, STANDARD ), 
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "signingCertificate.certs" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID,
	  DESCRIPTION( "signingCertificate.certs.essCertID" )
	  ENCODING_SPECIAL( BLOB_SEQUENCE ),
	  FL_ATTR_ATTREND, FL_SEQEND_2 /*FL_SEQEND*/, RANGE_BLOB },

	/* cRLExtReason:

		OID = 1 3 6 1 4 1 3029 3 1 4
		ENUMERATED */
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x03\x01\x04" ), CRYPT_CERTINFO_CRLEXTREASON,
	  DESCRIPTION( "cRLExtReason" )
	  ENCODING( ENUMERATED ),
	  ATTR_TYPEINFO2( REVREQ /*Per-entry*/, CRL, STANDARD ) | FL_ATTR_ATTREND, 
	  0, RANGE( 0, CRYPT_CRLEXTREASON_LAST ) },
#endif /* USE_CERTREV */

	/* keyFeatures:

		OID = 1 3 6 1 4 1 3029 3 1 5
		BITSTRING */
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x03\x01\x05" ), CRYPT_CERTINFO_KEYFEATURES,
	  DESCRIPTION( "keyFeatures" )
	  ENCODING( BITSTRING ),
	  ATTR_TYPEINFO2( CERT, CERTREQ, STANDARD ) | FL_ATTR_ATTREND, 
	  0, RANGE( 0, 7 ) },

	/* authorityInfoAccess:

		OID = 1 3 6 1 5 5 7 1 1
		SEQUENCE SIZE (1...MAX) OF {
			SEQUENCE {
				accessMethod	OBJECT IDENTIFIER,
				accessLocation	GeneralName
				}
			} */
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x01\x01" ), CRYPT_CERTINFO_AUTHORITYINFOACCESS,
	  DESCRIPTION( "authorityInfoAccess" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, STANDARD ), 
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "authorityInfoAccess.accessDescription (rtcs)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x03\x01\x07" ), 0,
	  DESCRIPTION( "authorityInfoAccess.rtcs (1 3 6 1 4 1 3029 3 1 7)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AUTHORITYINFO_RTCS,
	  DESCRIPTION( "authorityInfoAccess.accessDescription.accessLocation (rtcs)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "authorityInfoAccess.accessDescription (ocsp)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x30\x01" ), 0,
	  DESCRIPTION( "authorityInfoAccess.ocsp (1 3 6 1 5 5 7 48 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AUTHORITYINFO_OCSP,
	  DESCRIPTION( "authorityInfoAccess.accessDescription.accessLocation (ocsp)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "authorityInfoAccess.accessDescription (caIssuers)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x30\x02" ), 0,
	  DESCRIPTION( "authorityInfoAccess.caIssuers (1 3 6 1 5 5 7 48 2)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS,
	  DESCRIPTION( "authorityInfoAccess.accessDescription.accessLocation (caIssuers)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "authorityInfoAccess.accessDescription (httpCerts)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x30\x06" ), 0,
	  DESCRIPTION( "authorityInfoAccess.httpCerts (1 3 6 1 5 5 7 48 6)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE,
	  DESCRIPTION( "authorityInfoAccess.accessDescription.accessLocation (httpCerts)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "authorityInfoAccess.accessDescription (httpCRLs)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x30\x07" ), 0,
	  DESCRIPTION( "authorityInfoAccess.httpCRLs (1 3 6 1 5 5 7 48 7)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AUTHORITYINFO_CRLS,
	  DESCRIPTION( "authorityInfoAccess.accessDescription.accessLocation (httpCRLs)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "authorityInfoAccess.accessDescription (catchAll)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "authorityInfoAccess.catchAll" )
	  ENCODING_SPECIAL( BLOB_ANY ),	/* Match anything and ignore it */
	  FL_ATTR_ATTREND, FL_NONENCODING | FL_SEQEND_2 /*FL_SEQEND*/, RANGE_NONE },

#ifdef USE_CERTLEVEL_PKIX_FULL
	/* biometricInfo (QualifiedCert):

		OID = 1 3 6 1 5 5 7 1 2
		SEQUENCE OF {
			SEQUENCE {
				typeOfData		INTEGER,
				hashAlgorithm	OBJECT IDENTIFIER,
				dataHash		OCTET STRING,
				sourceDataUri	IA5String OPTIONAL
				}
			} */
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x01\x02" ), CRYPT_CERTINFO_BIOMETRICINFO,
	  DESCRIPTION( "biometricInfo" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_FULL ), 
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "biometricInfo.biometricData" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_BIOMETRICINFO_TYPE,
	  DESCRIPTION( "biometricInfo.biometricData.typeOfData" )
	  ENCODING( INTEGER ),
	  0, FL_MULTIVALUED, RANGE( 0, 1 ) },
	{ NULL, CRYPT_CERTINFO_BIOMETRICINFO_HASHALGO,
	  DESCRIPTION( "biometricInfo.biometricData.hashAlgorithm" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, FL_MULTIVALUED, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_BIOMETRICINFO_HASH,
	  DESCRIPTION( "biometricInfo.biometricData.dataHash" )
	  ENCODING( OCTETSTRING ),
	  0, FL_MULTIVALUED, RANGE( 16, CRYPT_MAX_HASHSIZE ) },
	{ NULL, CRYPT_CERTINFO_BIOMETRICINFO_URL,
	  DESCRIPTION( "biometricInfo.biometricData.sourceDataUri" )
	  ENCODING( STRING_IA5 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2 /*FL_SEQEND*/, CHECK_URL },

	/* qcStatements (QualifiedCert):

		OID = 1 3 6 1 5 5 7 1 3
		critical = TRUE
		SEQUENCE OF {
			SEQUENCE {
				statementID		OBJECT IDENTIFIER,
				statementInfo	SEQUENCE {
					semanticsIdentifier	OBJECT IDENTIFIER OPTIONAL,
					nameRegistrationAuthorities SEQUENCE OF GeneralName
				}
			}

		There are two versions of the statementID OID, one for RFC 3039 and
		the other for RFC 3739 (which are actually identical except where
		they're not).  To handle this we preferentially encode the RFC 3739
		(v2) OID but allow the v1 OID as a fallback by marking both as
		optional */
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x01\x03" ), CRYPT_CERTINFO_QCSTATEMENT,
	  DESCRIPTION( "qcStatements" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CRITICAL( CERT, PKIX_FULL ), 
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "qcStatements.qcStatement (statementID)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x0B\x02" ), 0,
	  DESCRIPTION( "qcStatements.qcStatement.statementID (1 3 6 1 5 5 7 11 2)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x0B\x01" ), 0,
	  DESCRIPTION( "qcStatements.qcStatement.statementID (Backwards-compat.) (1 3 6 1 5 5 7 11 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "qcStatements.qcStatement.statementInfo (statementID)" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_QCSTATEMENT_SEMANTICS,
	  DESCRIPTION( "qcStatements.qcStatement.statementInfo.semanticsIdentifier (statementID)" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE_OID },
	{ NULL, 0,
	  DESCRIPTION( "qcStatements.qcStatement.statementInfo.nameRegistrationAuthorities (statementID)" )
	  ENCODING( SEQUENCE ),
	  0, FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY,
	  DESCRIPTION( "qcStatements.qcStatement.statementInfo.nameRegistrationAuthorities.generalNames" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  FL_ATTR_ATTREND, FL_MULTIVALUED | FL_SEQEND_3 /* Really _4*/, ENCODED_OBJECT( generalNameInfo ) },
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* ipAddrBlocks (RPKI):

		OID = 1 3 6 1 5 5 7 1 7
		critical = TRUE
		SEQUENCE OF {
			SEQUENCE {
				addressFamily	OCTET STRING (SIZE(2)),
				addressesOrRanges SEQUENCE OF {
					CHOICE {
					addressPrefix
								BITSTRING,		-- Treated as blob, see below
					addressRange	SEQUENCE {
						min		BITSTRING,		-- Treated as blob, see below
						max		BITSTRING		-- Treated as blob, see below
						}
						}
					}
				}
			} 

	   The length range for the BIT STRING is 3 bytes (zero-length string 
	   for all IP address blocks) up to 19 bytes (16-byte IPv6 address plus 
	   3-byte BIT STRING header) */
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x01\x07" ), CRYPT_CERTINFO_IPADDRESSBLOCKS,
	  DESCRIPTION( "ipAddrBlocks" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ), 
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "ipAddrBlocks.ipAddressFamily" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_IPADDRESSBLOCKS_ADDRESSFAMILY,
	  DESCRIPTION( "ipAddrBlocks.ipAddressFamily.addressFamily" )
	  ENCODING( OCTETSTRING ),
	  0, FL_MULTIVALUED, RANGE( 2, 2 ) },
	{ NULL, 0,
	  DESCRIPTION( "ipAddrBlocks.ipAddressFamily.addressesOrRanges" )
	  ENCODING( SEQUENCE ),
	  0, FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX,
	  DESCRIPTION( "ipAddrBlocks.ipAddressFamily.addressesOrRanges.addressPrefix" )
	  ENCODING_SPECIAL( BLOB_BITSTRING ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, 3, 19 },
	{ NULL, 0,
	  DESCRIPTION( "ipAddrBlocks.ipAddressFamily.addressesOrRanges.addressRange" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_IPADDRESSBLOCKS_MIN,
	  DESCRIPTION( "ipAddrBlocks.ipAddressFamily.addressesOrRanges.addressRange.min" )
	  ENCODING_SPECIAL( BLOB_BITSTRING ),
	  0, FL_MULTIVALUED, 3, 19 },
	{ NULL, CRYPT_CERTINFO_IPADDRESSBLOCKS_MAX,
	  DESCRIPTION( "ipAddrBlocks.ipAddressFamily.addressesOrRanges.addressRange.max" )
	  ENCODING_SPECIAL( BLOB_BITSTRING ),
	  FL_ATTR_ATTREND, FL_SEQEND_4 | FL_MULTIVALUED, 3, 19 },

	/* autonomousSysIds (RPKI):

		OID = 1 3 6 1 5 5 7 1 8
		critical = TRUE
		SEQUENCE { 
			[ 0 ] EXPLICIT SEQUENCE OF {
				CHOICE {
				asId			INTEGER,
				asRange SEQUENCE {
					min			INTEGER,
					max			INTEGER
					}
					}
				}
			} 

		The AS ranges are a bit difficult to provide sensible restrictions
		for, traditionally they were 16-bit numbers but then RFC 4893 
		extended them to 32-bits, of which subranges up to about 400K have
		been allocated (with wierd huge gaps in between the small set of
		subranges in use), see http://www.iana.org/assignments/as-numbers/ 
		for current assignments.  We set a limit of 500K to allow some 
		sanity-checking while also allowing room for future expansion */
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x01\x08" ), CRYPT_CERTINFO_AUTONOMOUSSYSIDS,
	  DESCRIPTION( "autonomousSysIds" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ), 
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "autonomousSysIds.asnum" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_SETOF | FL_EXPLICIT, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_ID,
	  DESCRIPTION( "autonomousSysIds.asnum.asIdsOrRanges.asId" )
	  ENCODING( INTEGER ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, 1, 500000 },
	{ NULL, 0,
	  DESCRIPTION( "autonomousSysIds.asnum.asIdsOrRanges.asRange" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MIN,
	  DESCRIPTION( "autonomousSysIds.asnum.asIdsOrRanges.asRange.min" )
	  ENCODING( INTEGER ),
	  0, FL_MULTIVALUED, 1, 500000 },
	{ NULL, CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MAX,
	  DESCRIPTION( "autonomousSysIds.asnum.asIdsOrRanges.asRange.max" )
	  ENCODING( INTEGER ),
	  FL_ATTR_ATTREND, FL_SEQEND_3 | FL_MULTIVALUED, 1, 500000 },

#ifdef USE_CERTREV
	/* ocspNonce:

		OID = 1 3 6 1 5 5 7 48 1 2
		nonce		OCTET STRING

	   This value was apparently supposed to be an OCTET STRING (although it 
	   may also have been a cargo-cult design version of TSP's INTEGER), 
	   however while specifying a million pieces of uneecessary braindamage 
	   OCSP forgot to actually define this anywhere in the spec.  Because of 
	   this it's possible to get other stuff here as well, the worst-case 
	   being OpenSSL 0.9.6/0.9.7a-c which just dumps a raw blob (not even 
	   valid ASN.1 data) in here.  We can't do anything with this since we 
	   need at least something DER-encoded to be able to read it.  OpenSSL 
	   0.9.7d and later used an OCTET STRING.
	   
	   We set the en/decoding level to FL_LEVEL_OBLIVIOUS to make sure that 
	   it's still encoded even in oblivious mode, if we don't do this then a 
	   nonce in a request won't be returned in the response if the user is 
	   running at a reduced compliance level */
	{ MKOID( "\x06\x09\x2B\x06\x01\x05\x05\x07\x30\x01\x02" ), CRYPT_CERTINFO_OCSP_NONCE,
	  DESCRIPTION( "ocspNonce" )
	  ENCODING( OCTETSTRING ),
	  ATTR_TYPEINFO2( OCSPREQ, OCSPRESP, OBLIVIOUS ) | FL_ATTR_ATTREND, 
	  0, RANGE( 1, 64 ) },

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* ocspAcceptableResponses:

		OID = 1 3 6 1 5 5 7 48 1 4
		SEQUENCE {
			oidInstance1 OPTIONAL,
			oidInstance2 OPTIONAL,
				...
			oidInstanceN OPTIONAL
			} */
	{ MKOID( "\x06\x09\x2B\x06\x01\x05\x05\x07\x30\x01\x04" ), CRYPT_CERTINFO_OCSP_RESPONSE,
	  DESCRIPTION( "ocspAcceptableResponses" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( OCSPREQ, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ MKOID( "\x06\x09\x2B\x06\x01\x05\x05\x07\x30\x01\x01" ), CRYPT_CERTINFO_OCSP_RESPONSE_OCSP,
	  DESCRIPTION( "ocspAcceptableResponses.ocsp (1 3 6 1 5 5 7 48 1 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_SEQEND /*NONE*/, RANGE_NONE },

	/* ocspNoCheck:

		OID = 1 3 6 1 5 5 7 48 1 5
		critical = FALSE
		NULL

	   ocspNoCheck is a certificate extension rather than an OCSP request 
	   extension, it's used by the CA in yet another of OCSP's schizophrenic 
	   trust models to indicate that an OCSP responder certificate shouldn't 
	   be checked until it expires naturally.  The value is treated as a 
	   pseudo-numeric value that must be CRYPT_UNUSED when written and is 
	   explicitly set to CRYPT_UNUSED when read */
	{ MKOID( "\x06\x09\x2B\x06\x01\x05\x05\x07\x30\x01\x05" ), CRYPT_CERTINFO_OCSP_NOCHECK,
	  DESCRIPTION( "ocspNoCheck" )
	  ENCODING( NULL ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ) | FL_ATTR_ATTREND,
	  FL_NONENCODING, RANGE_UNUSED },

	/* ocspArchiveCutoff:

		OID = 1 3 6 1 5 5 7 48 1 6
		archiveCutoff	GeneralizedTime */
	{ MKOID( "\x06\x09\x2B\x06\x01\x05\x05\x07\x30\x01\x06" ), CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF,
	  DESCRIPTION( "ocspArchiveCutoff" )
	  ENCODING( TIME_GENERALIZED ),
	  ATTR_TYPEINFO( OCSPRESP, PKIX_PARTIAL ) | FL_ATTR_ATTREND, 
	  0, RANGE_TIME },
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
#endif /* USE_CERTREV */

	/* subjectInfoAccess:

		OID = 1 3 6 1 5 5 7 1 11
		SEQUENCE SIZE (1...MAX) OF {
			SEQUENCE {
				accessMethod	OBJECT IDENTIFIER,
				accessLocation	GeneralName
				}
			} */
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x01\x0B" ), CRYPT_CERTINFO_SUBJECTINFOACCESS,
	  DESCRIPTION( "subjectInfoAccess" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, STANDARD ), 
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "subjectInfoAccess.accessDescription (timeStamping)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x30\x03" ), 0,
	  DESCRIPTION( "subjectInfoAccess.timeStamping (1 3 6 1 5 5 7 48 3)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING,
	  DESCRIPTION( "subjectInfoAccess.accessDescription.accessLocation (timeStamping)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "subjectInfoAccess.accessDescription (caRepository)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x30\x05" ), 0,
	  DESCRIPTION( "subjectInfoAccess.caRepository (1 3 6 1 5 5 7 48 5)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY,
	  DESCRIPTION( "subjectInfoAccess.accessDescription.accessLocation (caRepository)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "subjectInfoAccess.accessDescription (signedObjectRepository)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x30\x09" ), 0,
	  DESCRIPTION( "subjectInfoAccess.signedObjectRepository (1 3 6 1 5 5 7 48 9)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY,
	  DESCRIPTION( "subjectInfoAccess.accessDescription.accessLocation (signedObjectRepository)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "subjectInfoAccess.accessDescription (rpkiManifest)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x30\x0A" ), 0,
	  DESCRIPTION( "subjectInfoAccess.rpkiManifest (1 3 6 1 5 5 7 48 10)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST,
	  DESCRIPTION( "subjectInfoAccess.accessDescription.accessLocation (rpkiManifest)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "subjectInfoAccess.accessDescription (signedObject)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x30\x0B" ), 0,
	  DESCRIPTION( "subjectInfoAccess.signedObject (1 3 6 1 5 5 7 48 11)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT,
	  DESCRIPTION( "subjectInfoAccess.accessDescription.accessLocation (signedObject)" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "subjectInfoAccess.accessDescription (catchAll)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "subjectInfoAccess.catchAll" )
	  ENCODING_SPECIAL( BLOB_ANY ),	/* Match anything and ignore it */
	  FL_ATTR_ATTREND, FL_NONENCODING | FL_SEQEND_2 /*FL_SEQEND*/, RANGE_NONE },

#if defined( USE_CERT_OBSCURE ) && defined( USE_CERTLEVEL_PKIX_PARTIAL )
	/* dateOfCertGen (SigG)

		OID = 1 3 36 8 3 1
		dateOfCertGen				GeneralizedTime */
	{ MKOID( "\x06\x05\x2B\x24\x08\x03\x01" ), CRYPT_CERTINFO_SIGG_DATEOFCERTGEN,
	  DESCRIPTION( "dateOfCertGen" )
	  ENCODING( TIME_GENERALIZED ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ) | FL_ATTR_ATTREND, 
	  0, RANGE_TIME },

	/* procuration (SigG)

		OID = 1 3 36 8 3 2
		SEQUENCE OF {
			country			  [ 1 ]	EXPLICIT PrintableString SIZE(2) OPTIONAL,
			typeOfSubstitution[ 2 ]	EXPLICIT PrintableString OPTIONAL,
			signingFor		  [ 3 ]	EXPLICIT GeneralName
			} */
	{ MKOID( "\x06\x05\x2B\x24\x08\x03\x02" ), CRYPT_CERTINFO_SIGG_PROCURATION,
	  DESCRIPTION( "procuration" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ),
	  FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY,
	  DESCRIPTION( "procuration.country" )
	  ENCODING_TAGGED( STRING_PRINTABLE, 1 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_EXPLICIT, RANGE( 2, 2 ) },
	{ NULL, CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION,
	  DESCRIPTION( "procuration.typeOfSubstitution" )
	  ENCODING_TAGGED( STRING_PRINTABLE, 2 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_EXPLICIT, RANGE( 1, 128 ) },
	{ NULL, CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR,
	  DESCRIPTION( "procuration.signingFor.thirdPerson" )
	  ENCODING_SPECIAL_TAGGED( SUBTYPED, 3 ),
	  FL_ATTR_ATTREND, FL_MULTIVALUED | FL_EXPLICIT | FL_SEQEND /*NONE*/, ENCODED_OBJECT( generalNameInfo ) },

	/* admissions (SigG)

		OID = 1 3 36 8 3 3
		SEQUENCE {
			authority			GeneralName OPTIONAL,
			content				SEQUENCE OF {
				namingAuthority	[ 1 ] EXPLICIT SEQUENCE {
					namingAuthID		OBJECT IDENTIFIER OPTIONAL,
					namingAuthURL		IA5String OPTIONAL,
					namingAuthText		PrintableString OPTIONAL
					} OPTIONAL,
				professionInfo SEQUENCE OF {
					professionItems SEQUENCE OF {
						professionItem	PrintableString
						},
					professionOIDs SEQUENCE OF {
						professionOID	OBJECT IDENTIFIER
						} OPTIONAL,
					registrationNumber	PrintableString OPTIONAL
					}
				}
			} 

		This is a weird extension with several fields (admissionAuthority, namingAuthority)
		duplicated in two places and only a vague indication in the spec as to which one
		is used under which conditions, the above is an attempt to create a meaningful
		interpretation of the definition */
	{ MKOID( "\x06\x05\x2B\x24\x08\x03\x03" ), CRYPT_CERTINFO_SIGG_ADMISSIONS,
	  DESCRIPTION( "admissions" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY,
	  DESCRIPTION( "admissions.authority" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "admissions.content" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "admissions.content.namingAuthority" )
	  ENCODING_TAGGED( SEQUENCE, 1 ),
	  0, FL_OPTIONAL | FL_EXPLICIT, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHID,
	  DESCRIPTION( "admissions.content.namingAuthority.namingAuthID" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHURL,
	  DESCRIPTION( "admissions.content.namingAuthority.namingAuthURL" )
	  ENCODING( STRING_IA5 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE_TEXTSTRING },
	{ NULL, CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHTEXT,
	  DESCRIPTION( "admissions.content.namingAuthority.namingAuthText" )
	  ENCODING( STRING_PRINTABLE ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, RANGE_TEXTSTRING },
	{ NULL, 0,
	  DESCRIPTION( "admissions.professionInfo" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "admissions.professionInfo.professionItems" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONITEM,
	  DESCRIPTION( "admissions.professionInfo.professionItems.professionItem" )
	  ENCODING( STRING_PRINTABLE ),
	  0, FL_MULTIVALUED | FL_SEQEND, RANGE_TEXTSTRING },
	{ NULL, 0,
	  DESCRIPTION( "admissions.professionInfo.professionOIDs" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONOID,
	  DESCRIPTION( "admissions.professionInfo.professionOIDs.professionOID" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_SIGG_ADMISSIONS_REGISTRATIONNUMBER,
	  DESCRIPTION( "admissions.professionInfo.professionItems.registrationNumber" )
	  ENCODING( STRING_PRINTABLE ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_3, RANGE_TEXTSTRING },

	/* monetaryLimit (SigG)

		OID = 1 3 36 8 3 4
		SEQUENCE {
			currency			PrintableString SIZE(3),
			amount				INTEGER,
			exponent			INTEGER
			} */
	{ MKOID( "\x06\x05\x2B\x24\x08\x03\x04" ), CRYPT_CERTINFO_SIGG_MONETARYLIMIT,
	  DESCRIPTION( "monetaryLimit" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY,
	  DESCRIPTION( "monetaryLimit.currency" )
	  ENCODING( STRING_PRINTABLE ),
	  0, 0, RANGE( 3, 3 ) },
	{ NULL, CRYPT_CERTINFO_SIGG_MONETARY_AMOUNT,
	  DESCRIPTION( "monetaryLimit.amount" )
	  ENCODING( INTEGER ),
	  0, 0, RANGE( 1, 255 ) },	/* That's what the spec says */
	{ NULL, CRYPT_CERTINFO_SIGG_MONETARY_EXPONENT,
	  DESCRIPTION( "monetaryLimit.exponent" )
	  ENCODING( INTEGER ),
	  FL_ATTR_ATTREND, FL_SEQEND /*NONE*/, RANGE( 0, 255 ) },

	/* declarationOfMajority (SigG)

		OID = 1 3 36 8 3 5
		CHOICE {
			fullAgeAtCountry [ 1 ] SEQUENCE {
				fullAge			DEFAULT TRUE,
				country			PrintableString (SIZE(2))
				},
			dateOfBirth	  [ 2 ]	GeneralizedTime
			} 

		This is a rather problematic extension because it uses a CHOICE at 
		the top level so there's no easy way to work with it because, 
		depending on whether the fullAgeAtCountry or dateOfBirth is used, 
		the extension has a different name.  To deal with this we rely on
		the fact that in privacy-conscious Europe where this is unlikely
		to be used (as opposed to everywhere else, where it definitely
		won't be used) it's unlikely that people will want personally
		identifiable information like birth dates in certificates and so
		what'll be used in practice is the fullAgeAtCountry.

		The fullAgeAtCountry has an additional complication in that in 
		theory there's a fullAge field that precedes the country field but
		since it's declared as DEFAULT TRUE and a declaration of majority
		can't be false it can never occur in any (sane) implementation.

		The resulting extension is therefore:

		declarationOfMajority [ 1 ] EXPLICIT SEQUENCE {
				country			PrintableString (SIZE(2))
				} */
	{ MKOID( "\x06\x05\x2B\x24\x08\x03\x05" ), CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY,
	  DESCRIPTION( "declarationOfMajority" )
	  ENCODING_TAGGED( SEQUENCE, 1 ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ),
	  FL_EXPLICIT, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY_COUNTRY,
	  DESCRIPTION( "declarationOfMajority.fullAgeAtCountry.country" )
	  ENCODING( STRING_PRINTABLE ),
	  FL_ATTR_ATTREND, FL_SEQEND, RANGE( 2, 2 ) },

	/* restriction (SigG)

		OID = 1 3 36 8 3 8
		restriction				PrintableString */
	{ MKOID( "\x06\x05\x2B\x24\x08\x03\x08" ), CRYPT_CERTINFO_SIGG_RESTRICTION,
	  DESCRIPTION( "restriction" )
	  ENCODING( STRING_PRINTABLE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ) | FL_ATTR_ATTREND,
	  0, RANGE( 1, 128 ) },

#ifdef USE_CERTREV
	/* certHash  (SigG, OCSP)

		OID = 1 3 36 8 3 13
		SEQUENCE {
			hashAlgo	AlgorithmIdentifier,
			certHash	OCTET STRING
			} */
	{ NULL, CRYPT_CERTINFO_SIGG_CERTHASH,
	  DESCRIPTION( "certhash" )
	  ENCODING_SPECIAL( BLOB_SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ) | FL_ATTR_ATTREND,
	  0, RANGE_BLOB },
#endif /* USE_CERTREV */

	/* additionalInformation (SigG)

		OID = 1 3 36 8 3 15
		additionalInformation	PrintableString */
	{ MKOID( "\x06\x05\x2B\x24\x08\x03\x0F" ), CRYPT_CERTINFO_SIGG_ADDITIONALINFORMATION,
	  DESCRIPTION( "additionalInformation" )
	  ENCODING( STRING_PRINTABLE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ) | FL_ATTR_ATTREND,
	  0, RANGE( 1, 128 ) },
#endif /* USE_CERT_OBSCURE && USE_CERTLEVEL_PKIX_PARTIAL */

#ifdef USE_CERT_OBSOLETE
	/* strongExtranet:

		OID = 1 3 101 1 4 1
		SEQUENCE {
			version				INTEGER (0),
			SEQUENCE OF {
				SEQUENCE {
					zone		INTEGER,
					id			OCTET STRING (SIZE(1..64))
					}
				}
			} */
	{ MKOID( "\x06\x05\x2B\x65\x01\x04\x01" ), CRYPT_CERTINFO_STRONGEXTRANET,
	  DESCRIPTION( "strongExtranet" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "strongExtranet.version" )
	  ENCODING_SPECIAL( BLOB_ANY ),	/* INTEGER 0 */
	  0, FL_NONENCODING, 0, 0, 3, "\x02\x01\x00" },
	{ NULL, 0,
	  DESCRIPTION( "strongExtranet.sxNetIDList" )
	  ENCODING( SEQUENCE ),
	  0, FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "strongExtranet.sxNetIDList.sxNetID" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_STRONGEXTRANET_ZONE,
	  DESCRIPTION( "strongExtranet.sxNetIDList.sxNetID.zone" )
	  ENCODING( INTEGER ),
	  0, 0, RANGE( 0, MAX_INTLENGTH ) },
	{ NULL, CRYPT_CERTINFO_STRONGEXTRANET_ID,
	  DESCRIPTION( "strongExtranet.sxNetIDList.sxnetID.id" )
	  ENCODING( OCTETSTRING ),
	  FL_ATTR_ATTREND, FL_SEQEND_3 /*FL_SEQEND_2*/, RANGE( 1, 64 ) },
#endif /* USE_CERT_OBSOLETE */

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* subjectDirectoryAttributes:

		OID = 2 5 29 9
		SEQUENCE SIZE (1..MAX) OF {
			SEQUENCE {
				type			OBJECT IDENTIFIER,
				values			SET OF ANY			-- SIZE (1)
				} */
	{ MKOID( "\x06\x03\x55\x1D\x09" ), CRYPT_CERTINFO_SUBJECTDIRECTORYATTRIBUTES,
	  DESCRIPTION( "subjectDirectoryAttributes" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ),
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "subjectDirectoryAttributes.attribute" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SUBJECTDIR_TYPE,
	  DESCRIPTION( "subjectDirectoryAttributes.attribute.type" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, FL_MULTIVALUED, RANGE_OID },
	{ NULL, 0,
	  DESCRIPTION( "subjectDirectoryAttributes.attribute.values" )
	  ENCODING( SET ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SUBJECTDIR_VALUES,
	  DESCRIPTION( "subjectDirectoryAttributes.attribute.values.value" )
	  ENCODING_SPECIAL( BLOB_ANY ),
	  FL_ATTR_ATTREND, FL_MULTIVALUED | FL_SEQEND_2 /*SEQEND*/, RANGE_ATTRIBUTEBLOB },
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

	/* subjectKeyIdentifier:

		OID = 2 5 29 14
		OCTET STRING

	   In theory this should only be processed at level 
	   CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL but the sKID is a universal 
	   identifier that's often used in place of the DN to identify a
	   certificate so unlike the authorityKeyIdentifier we process it even 
	   in standard mode */
	{ MKOID( "\x06\x03\x55\x1D\x0E" ), CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER,
	  DESCRIPTION( "subjectKeyIdentifier" )
	  ENCODING( OCTETSTRING ),
	  ATTR_TYPEINFO( CERT, STANDARD ) | FL_ATTR_ATTREND,
	  0, RANGE( 1, 64 ) },

	/* keyUsage:

		OID = 2 5 29 15
		critical = TRUE
		BITSTRING */
	{ MKOID( "\x06\x03\x55\x1D\x0F" ), CRYPT_CERTINFO_KEYUSAGE,
	  DESCRIPTION( "keyUsage" )
	  ENCODING( BITSTRING ),
	  ATTR_TYPEINFO2_CRITICAL( CERTREQ, CERT, REDUCED ) | FL_ATTR_ATTREND,
	  0, 0, CRYPT_KEYUSAGE_LAST, 0, NULL },

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* privateKeyUsagePeriod:

		OID = 2 5 29 16
		SEQUENCE {
			notBefore	  [ 0 ]	GeneralizedTime OPTIONAL,
			notAfter	  [ 1 ]	GeneralizedTime OPTIONAL
			} */
	{ MKOID( "\x06\x03\x55\x1D\x10" ), CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD,
	  DESCRIPTION( "privateKeyUsagePeriod" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE,
	  DESCRIPTION( "privateKeyUsagePeriod.notBefore" )
	  ENCODING_TAGGED( TIME_GENERALIZED, 0 ),
	  0, FL_OPTIONAL, RANGE_TIME },
	{ NULL, CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER,
	  DESCRIPTION( "privateKeyUsagePeriod.notAfter" )
	  ENCODING_TAGGED( TIME_GENERALIZED, 1 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_SEQEND /*NONE*/, RANGE_TIME },
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

	/* subjectAltName:

		OID = 2 5 29 17
		SEQUENCE OF GeneralName */
	{ MKOID( "\x06\x03\x55\x1D\x11" ), FIELDID_FOLLOWS,
	  DESCRIPTION( "subjectAltName" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO2( CERTREQ, CERT, STANDARD ),
	  FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SUBJECTALTNAME,
	  DESCRIPTION( "subjectAltName.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  FL_ATTR_ATTREND, FL_MULTIVALUED | FL_SEQEND /*NONE*/, ENCODED_OBJECT( generalNameInfo ) },

	/* issuerAltName:

		OID = 2 5 29 18
		SEQUENCE OF GeneralName */
	{ MKOID( "\x06\x03\x55\x1D\x12" ), FIELDID_FOLLOWS,
	  DESCRIPTION( "issuerAltName" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO2( CERT, CRL, STANDARD ),
	  FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_ISSUERALTNAME,
	  DESCRIPTION( "issuerAltName.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  FL_ATTR_ATTREND, FL_MULTIVALUED | FL_SEQEND /*NONE*/, ENCODED_OBJECT( generalNameInfo ) },

	/* basicConstraints:

		OID = 2 5 29 19
		critical = TRUE
		SEQUENCE {
			cA					BOOLEAN DEFAULT FALSE,
			pathLenConstraint	INTEGER (0..64) OPTIONAL
			} */
	{ MKOID( "\x06\x03\x55\x1D\x13" ), CRYPT_CERTINFO_BASICCONSTRAINTS,
	  DESCRIPTION( "basicConstraints" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO2_CRITICAL( CERT, ATTRCERT, REDUCED ),
	  FL_EMPTYOK, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CA,
	  DESCRIPTION( "basicConstraints.cA" )
	  ENCODING( BOOLEAN ),
	  0, FL_OPTIONAL | FL_DEFAULT, RANGE_BOOLEAN },
	{ NULL, CRYPT_CERTINFO_PATHLENCONSTRAINT,
	  DESCRIPTION( "basicConstraints.pathLenConstraint" )
	  ENCODING( INTEGER ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_SEQEND /*NONE*/, RANGE( 0, 64 ) },

#if defined( USE_CERTREV ) && defined( USE_CERTLEVEL_PKIX_PARTIAL )
	/* cRLNumber:

		OID = 2 5 29 20
		INTEGER */
	{ MKOID( "\x06\x03\x55\x1D\x14" ), CRYPT_CERTINFO_CRLNUMBER,
	  DESCRIPTION( "cRLNumber" )
	  ENCODING( INTEGER ),
	  ATTR_TYPEINFO( CRL, PKIX_PARTIAL ) | FL_ATTR_ATTREND,
	  0, RANGE( 0, MAX_INTLENGTH ) },
#endif /* USE_CERTREV && USE_CERTLEVEL_PKIX_PARTIAL */

#if defined( USE_CERTREV ) || defined( USE_CERTREQ )
	/* cRLReason:

		OID = 2 5 29 21
		ENUMERATED */
	{ MKOID( "\x06\x03\x55\x1D\x15" ), CRYPT_CERTINFO_CRLREASON,
	  DESCRIPTION( "cRLReason" )
	  ENCODING( ENUMERATED ),
	  ATTR_TYPEINFO2( CRL, REVREQ /*Per-entry*/, REDUCED ) | FL_ATTR_ATTREND,
	  0, RANGE( 0, CRYPT_CRLREASON_LAST ) },
#endif /* USE_CERTREV || USE_CERTREQ */

#if ( defined( USE_CERTREV ) || defined( USE_CERTREQ ) ) && \
	defined( USE_CERTLEVEL_PKIX_FULL )
	/* holdInstructionCode:

		OID = 2 5 29 23
		OBJECT IDENTIFIER */
	{ MKOID( "\x06\x03\x55\x1D\x17" ), CRYPT_CERTINFO_HOLDINSTRUCTIONCODE,
	  DESCRIPTION( "holdInstructionCode" )
	  ENCODING_SPECIAL( CHOICE ),
	  ATTR_TYPEINFO2( CRL, REVREQ /*Per-entry*/, PKIX_FULL ) | FL_ATTR_ATTREND,
	  0, CRYPT_HOLDINSTRUCTION_NONE, CRYPT_HOLDINSTRUCTION_LAST, 0, ( void * ) holdInstructionInfo },
#endif /* ( USE_CERTREV || USE_CERTREQ ) && USE_CERTLEVEL_PKIX_FULL */

#if defined( USE_CERTREV ) || defined( USE_CERTREQ )
	/* invalidityDate:

		OID = 2 5 29 24
		GeneralizedTime */
	{ MKOID( "\x06\x03\x55\x1D\x18" ), CRYPT_CERTINFO_INVALIDITYDATE,
	  DESCRIPTION( "invalidityDate" )
	  ENCODING( TIME_GENERALIZED ),
	  ATTR_TYPEINFO2( CRL, REVREQ /*Per-entry*/, STANDARD ) | FL_ATTR_ATTREND,
	  0, RANGE_TIME },
#endif /* USE_CERTREV || USE_CERTREQ */

#if defined( USE_CERTREV ) && defined( USE_CERTLEVEL_PKIX_PARTIAL )
	/* deltaCRLIndicator:

		OID = 2 5 29 27
		critical = TRUE
		INTEGER */
	{ MKOID( "\x06\x03\x55\x1D\x1B" ), CRYPT_CERTINFO_DELTACRLINDICATOR,
	  DESCRIPTION( "deltaCRLIndicator" )
	  ENCODING( INTEGER ),
	  ATTR_TYPEINFO_CRITICAL( CRL, PKIX_PARTIAL ) | FL_ATTR_ATTREND,
	  0, RANGE( 0, MAX_INTLENGTH ) },

	/* issuingDistributionPoint:

		OID = 2 5 29 28
		critical = TRUE
		SEQUENCE {
			distributionPoint [ 0 ]	{
				fullName	  [ 0 ]	{				-- CHOICE { ... }
					SEQUENCE OF GeneralName			-- GeneralNames
					}
				} OPTIONAL,
			onlyContainsUserCerts
							  [ 1 ]	BOOLEAN DEFAULT FALSE,
			onlyContainsCACerts
							  [ 2 ]	BOOLEAN DEFAULT FALSE,
			onlySomeReasons	  [ 3 ]	BITSTRING OPTIONAL,
			indirectCRL		  [ 4 ]	BOOLEAN DEFAULT FALSE
		} */
	{ MKOID( "\x06\x03\x55\x1D\x1C" ), CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT,
	  DESCRIPTION( "issuingDistributionPoint" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CRITICAL( CRL, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "issuingDistributionPoint.distributionPoint" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "issuingDistributionPoint.distributionPoint.fullName" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, 0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "issuingDistributionPoint.distributionPoint.fullName.generalNames" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_ISSUINGDIST_FULLNAME,
	  DESCRIPTION( "issuingDistributionPoint.distributionPoint.fullName.generalNames.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_3, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY,
	  DESCRIPTION( "issuingDistributionPoint.onlyContainsUserCerts" )
	  ENCODING_TAGGED( BOOLEAN, 1 ),
	  0, FL_OPTIONAL | FL_DEFAULT, RANGE_BOOLEAN },
	{ NULL, CRYPT_CERTINFO_ISSUINGDIST_CACERTSONLY,
	  DESCRIPTION( "issuingDistributionPoint.onlyContainsCACerts" )
	  ENCODING_TAGGED( BOOLEAN, 2 ),
	  0, FL_OPTIONAL | FL_DEFAULT, RANGE_BOOLEAN },
	{ NULL, CRYPT_CERTINFO_ISSUINGDIST_SOMEREASONSONLY,
	  DESCRIPTION( "issuingDistributionPoint.onlySomeReasons" )
	  ENCODING_TAGGED( BITSTRING, 3 ),
	  0, FL_OPTIONAL, RANGE( 0, CRYPT_CRLREASONFLAG_LAST ) },
	{ NULL, CRYPT_CERTINFO_ISSUINGDIST_INDIRECTCRL,
	  DESCRIPTION( "issuingDistributionPoint.indirectCRL" )
	  ENCODING_TAGGED( BOOLEAN, 4 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_DEFAULT | FL_SEQEND /*NONE*/, RANGE_BOOLEAN },
#endif /* USER_CERTREV && USE_CERTLEVEL_PKIX_PARTIAL */

#if defined( USE_CERTREV ) && defined( USE_CERTLEVEL_PKIX_FULL )
	/* certificateIssuer:

		OID = 2 5 29 29
		critical = TRUE
		certificateIssuer SEQUENCE OF GeneralName */
	{ MKOID( "\x06\x03\x55\x1D\x1D" ), FIELDID_FOLLOWS,
	  DESCRIPTION( "certificateIssuer" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CRITICAL( CRL, PKIX_FULL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CERTIFICATEISSUER,
	  DESCRIPTION( "certificateIssuer.generalNames" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  FL_ATTR_ATTREND, FL_MULTIVALUED, ENCODED_OBJECT( generalNameInfo ) },
#endif /* USE_CERTREV && USE_CERTLEVEL_PKIX_FULL */

#ifdef USE_CERTLEVEL_PKIX_FULL
	/* nameConstraints

		OID = 2 5 29 30
		critical = TRUE
		SEQUENCE {
			permittedSubtrees [ 0 ]	SEQUENCE OF {
				SEQUENCE { GeneralName }
				} OPTIONAL,
			excludedSubtrees  [ 1 ]	SEQUENCE OF {
				SEQUENCE { GeneralName }
				} OPTIONAL,
			}

		RFC 3280 extended this by adding two additional fields after the
		GeneralName (probably from X.509v4) but mitigated it by requiring
		that they never be used so we leave the definition as is */
	{ MKOID( "\x06\x03\x55\x1D\x1E" ), CRYPT_CERTINFO_NAMECONSTRAINTS,
	  DESCRIPTION( "nameConstraints" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO2( CERT, ATTRCERT, PKIX_FULL ),
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "nameConstraints.permittedSubtrees" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "nameConstraints.permittedSubtrees.sequenceOf" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_PERMITTEDSUBTREES,
	  DESCRIPTION( "nameConstraints.permittedSubtrees.sequenceOf.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "nameConstraints.excludedSubtrees" )
	  ENCODING_TAGGED( SEQUENCE, 1 ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "nameConstraints.excludedSubtrees.sequenceOf" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_EXCLUDEDSUBTREES,
	  DESCRIPTION( "nameConstraints.excludedSubtrees.sequenceOf.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2 /*or _3*/, ENCODED_OBJECT( generalNameInfo ) },
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* cRLDistributionPoints:

		OID = 2 5 29 31
		SEQUENCE OF {
			SEQUENCE {
				distributionPoint
							  [ 0 ]	{				-- CHOICE { ... }
					fullName  [ 0 ]	SEQUENCE OF GeneralName
					} OPTIONAL,
				reasons		  [ 1 ]	BIT STRING OPTIONAL,
				cRLIssuer	  [ 2 ]	SEQUENCE OF GeneralName OPTIONAL
				}
			} */
	{ MKOID( "\x06\x03\x55\x1D\x1F" ), CRYPT_CERTINFO_CRLDISTRIBUTIONPOINT,
	  DESCRIPTION( "cRLDistributionPoints" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO2( CERT, ATTRCERT, STANDARD ),
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "cRLDistributionPoints.distPoint" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "cRLDistributionPoints.distPoint.distPoint" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "cRLDistributionPoints.distPoint.distPoint.fullName" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CRLDIST_FULLNAME,
	  DESCRIPTION( "cRLDistributionPoints.distPoint.distPoint.fullName.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, CRYPT_CERTINFO_CRLDIST_REASONS,
	  DESCRIPTION( "cRLDistributionPoints.distPoint.reasons" )
	  ENCODING_TAGGED( BITSTRING, 1 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE( 0, CRYPT_CRLREASONFLAG_LAST ) },
	{ NULL, 0,
	  DESCRIPTION( "cRLDistributionPoints.distPoint.cRLIssuer" )
	  ENCODING_TAGGED( SEQUENCE, 2 ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CRLDIST_CRLISSUER,
	  DESCRIPTION( "cRLDistributionPoints.distPoint.cRLIssuer.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2 /*or _3*/, ENCODED_OBJECT( generalNameInfo ) },

	/* certificatePolicies:

		OID = 2 5 29 32
		SEQUENCE SIZE (1..64) OF {
			SEQUENCE {
				policyIdentifier	OBJECT IDENTIFIER,
				policyQualifiers	SEQUENCE SIZE (1..64) OF {
									SEQUENCE {
					policyQualifierId
									OBJECT IDENTIFIER,
					qualifier		ANY DEFINED BY policyQualifierID
						} OPTIONAL
					}
				}
			}

		CPSuri ::= IA5String						-- OID = cps

		UserNotice ::= SEQUENCE {					-- OID = unotice
			noticeRef		SEQUENCE {
				organization	DisplayText,
				noticeNumbers	SEQUENCE OF INTEGER	-- SIZE (1)
				} OPTIONAL,
			explicitText	DisplayText OPTIONAL
			}

	   Note that although this extension is decoded at
	   CRYPT_COMPLIANCELEVEL_STANDARD policy constraints are only enforced
	   at CRYPT_COMPLIANCELEVEL_PKIX_FULL due to the totally bizarre
	   requirements that some of them have (see comments in chk_*.c for more
	   on this) */
	{ MKOID( "\x06\x03\x55\x1D\x20" ), CRYPT_CERTINFO_CERTIFICATEPOLICIES,
	  DESCRIPTION( "certPolicies" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, STANDARD ),
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "certPolicies.policyInfo" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CERTPOLICYID,
	  DESCRIPTION( "certPolicies.policyInfo.policyIdentifier" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, FL_MULTIVALUED, RANGE_OID },
	{ NULL, 0,
	  DESCRIPTION( "certPolicies.policyInfo.policyQualifiers" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x02\x01" ), 0,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual.cps (1 3 6 1 5 5 7 2 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CERTPOLICY_CPSURI,
	  DESCRIPTION( "certPolicies.policyInfo.policyQuals.qualifier.cPSuri" )
	  ENCODING( STRING_IA5 ),
	  0, FL_MULTIVALUED | FL_SEQEND /*FL_SEQEND_2*/, CHECK_URL },
	{ NULL, 0,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x02\x02" ), 0,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual.unotice (1 3 6 1 5 5 7 2 2)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual.userNotice" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual.userNotice.noticeRef" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual.userNotice.noticeRef.organization" )
	  ENCODING_SPECIAL( TEXTSTRING ),
	  0, FL_MULTIVALUED, RANGE( 1, 200 ) },
	{ NULL, 0,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual.userNotice.noticeRef.noticeNumbers" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual.userNotice.noticeRef.noticeNumbers" )
	  ENCODING( INTEGER ),
	  0, FL_MULTIVALUED | FL_SEQEND_2, RANGE( 1, 1000 ) },
	{ NULL, CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT,
	  DESCRIPTION( "certPolicies.policyInfo.policyQual.userNotice.explicitText" )
	  ENCODING_SPECIAL( TEXTSTRING ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_3 /*FL_SEQEND, or _4 (CPS) or _5 or _7 (uNotice), */, RANGE( 1, 200 ) },

#ifdef USE_CERTLEVEL_PKIX_FULL
	/* policyMappings:

		OID = 2 5 29 33
		SEQUENCE SIZE (1..MAX) OF {
			SEQUENCE {
				issuerDomainPolicy	OBJECT IDENTIFIER,
				subjectDomainPolicy	OBJECT IDENTIFIER
				}
			} */
	{ MKOID( "\x06\x03\x55\x1D\x21" ), CRYPT_CERTINFO_POLICYMAPPINGS,
	  DESCRIPTION( "policyMappings" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_FULL ),
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "policyMappings.sequenceOf" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_ISSUERDOMAINPOLICY,
	  DESCRIPTION( "policyMappings.sequenceOf.issuerDomainPolicy" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, FL_MULTIVALUED, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_SUBJECTDOMAINPOLICY,
	  DESCRIPTION( "policyMappings.sequenceOf.subjectDomainPolicy" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  FL_ATTR_ATTREND, FL_MULTIVALUED | FL_SEQEND_2 /*FL_SEQEND_3*/, RANGE_OID },
#endif /* USE_CERTLEVEL_PKIX_FULL */

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* authorityKeyIdentifier:

		OID = 2 5 29 35
		SEQUENCE {
			keyIdentifier [ 0 ]	OCTET STRING OPTIONAL,
			authorityCertIssuer						-- Neither or both
						  [ 1 ] SEQUENCE OF GeneralName OPTIONAL
			authorityCertSerialNumber				-- of these must
						  [ 2 ] INTEGER OPTIONAL	-- be present
			}

	   Although the serialNumber should be an INTEGER it's really an INTEGER 
	   equivalent of an OCTET STRING hole so we call it an OCTET STRING to 
	   make sure that it gets handled appropriately */
	{ MKOID( "\x06\x03\x55\x1D\x23" ), CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER,
	  DESCRIPTION( "authorityKeyIdentifier" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO2( CERT, CRL, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER,
	  DESCRIPTION( "authorityKeyIdentifier.keyIdentifier" )
	  ENCODING_TAGGED( OCTETSTRING, 0 ),
	  0, FL_OPTIONAL, RANGE( 1, 64 ) },
	{ NULL, 0,
	  DESCRIPTION( "authorityKeyIdentifier.authorityCertIssuer" )
	  ENCODING_TAGGED( SEQUENCE, 1 ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AUTHORITY_CERTISSUER,
	  DESCRIPTION( "authorityKeyIdentifier.authorityCertIssuer.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, CRYPT_CERTINFO_AUTHORITY_CERTSERIALNUMBER,
	  DESCRIPTION( "authorityKeyIdentifier.authorityCertSerialNumber" )
	  ENCODING_TAGGED( OCTETSTRING, 2 ),	/* Actually an INTEGER hole */
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_SEQEND /*NONE*/, RANGE( 1, 64 ) },
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

#ifdef USE_CERTLEVEL_PKIX_FULL
	/* policyConstraints:

		OID = 2 5 29 36
		SEQUENCE {
			requireExplicitPolicy [ 0 ]	INTEGER OPTIONAL,
			inhibitPolicyMapping  [ 1 ]	INTEGER OPTIONAL
			} */
	{ MKOID( "\x06\x03\x55\x1D\x24" ), CRYPT_CERTINFO_POLICYCONSTRAINTS,
	  DESCRIPTION( "policyConstraints" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_FULL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_REQUIREEXPLICITPOLICY,
	  DESCRIPTION( "policyConstraints.requireExplicitPolicy" )
	  ENCODING_TAGGED( INTEGER, 0 ),
	  0, FL_OPTIONAL, RANGE( 0, 64 ) },
	{ NULL, CRYPT_CERTINFO_INHIBITPOLICYMAPPING,
	  DESCRIPTION( "policyConstraints.inhibitPolicyMapping" )
	  ENCODING_TAGGED( INTEGER, 1 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_SEQEND /*NONE*/, RANGE( 0, 64 ) },
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* extKeyUsage:

		OID = 2 5 29 37
		SEQUENCE {
			oidInstance1 OPTIONAL,
			oidInstance2 OPTIONAL,
				...
			oidInstanceN OPTIONAL
			} */
	{ MKOID( "\x06\x03\x55\x1D\x25" ), CRYPT_CERTINFO_EXTKEYUSAGE,
	  DESCRIPTION( "extKeyUsage" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO2( CERTREQ, CERT, STANDARD ),
	  0, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x15" ), CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING,
	  DESCRIPTION( "extKeyUsage.individualCodeSigning (1 3 6 1 4 1 311 2 1 21)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x16" ), CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING,
	  DESCRIPTION( "extKeyUsage.commercialCodeSigning (1 3 6 1 4 1 311 2 1 22)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x0A\x03\x01" ), CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING,
	  DESCRIPTION( "extKeyUsage.certTrustListSigning (1 3 6 1 4 1 311 10 3 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x0A\x03\x02" ), CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING,
	  DESCRIPTION( "extKeyUsage.timeStampSigning (1 3 6 1 4 1 311 10 3 2)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x0A\x03\x03" ), CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO,
	  DESCRIPTION( "extKeyUsage.serverGatedCrypto (1 3 6 1 4 1 311 10 3 3)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x0A\x03\x04" ), CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM,
	  DESCRIPTION( "extKeyUsage.encrypedFileSystem (1 3 6 1 4 1 311 10 3 4)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x03\x01" ), CRYPT_CERTINFO_EXTKEY_SERVERAUTH,
	  DESCRIPTION( "extKeyUsage.serverAuth (1 3 6 1 5 5 7 3 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x03\x02" ), CRYPT_CERTINFO_EXTKEY_CLIENTAUTH,
	  DESCRIPTION( "extKeyUsage.clientAuth (1 3 6 1 5 5 7 3 2)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x03\x03" ), CRYPT_CERTINFO_EXTKEY_CODESIGNING,
	  DESCRIPTION( "extKeyUsage.codeSigning (1 3 6 1 5 5 7 3 3)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x03\x04" ), CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION,
	  DESCRIPTION( "extKeyUsage.emailProtection (1 3 6 1 5 5 7 3 4)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x03\x05" ), CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM,
	  DESCRIPTION( "extKeyUsage.ipsecEndSystem (1 3 6 1 5 5 7 3 5)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x03\x06" ), CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL,
	  DESCRIPTION( "extKeyUsage.ipsecTunnel (1 3 6 1 5 5 7 3 6)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x03\x07" ), CRYPT_CERTINFO_EXTKEY_IPSECUSER,
	  DESCRIPTION( "extKeyUsage.ipsecUser (1 3 6 1 5 5 7 3 7)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x03\x08" ), CRYPT_CERTINFO_EXTKEY_TIMESTAMPING,
	  DESCRIPTION( "extKeyUsage.timeStamping (1 3 6 1 5 5 7 3 8)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x03\x09" ), CRYPT_CERTINFO_EXTKEY_OCSPSIGNING,
	  DESCRIPTION( "extKeyUsage.ocspSigning (1 3 6 1 5 5 7 3 9)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x05\x2B\x24\x08\x02\x01" ), CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE,
	  DESCRIPTION( "extKeyUsage.directoryService (1 3 36 8 2 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x04\x55\x1D\x25\x00" ), CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE,
	  DESCRIPTION( "extKeyUsage.anyExtendedKeyUsage(2 5 29 37 0)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x04\x01" ), CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO,
	  DESCRIPTION( "extKeyUsage.serverGatedCrypto (2 16 840 1 113730 4 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0A\x60\x86\x48\x01\x86\xF8\x45\x01\x08\x01" ), CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA,
	  DESCRIPTION( "extKeyUsage.serverGatedCryptoCA (2 16 840 1 113733 1 8 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "extKeyUsage.catchAll" )
	  ENCODING_SPECIAL( BLOB_ANY ),	/* Match anything and ignore it */
	  FL_ATTR_ATTREND, FL_NONENCODING | FL_SEQEND /*NONE*/, RANGE_NONE },

#ifdef USE_CERTLEVEL_PKIX_FULL
#ifdef USE_REV
	/* crlStreamIdentifier:

		OID = 2 5 29 40
		INTEGER */
	{ MKOID( "\x06\x03\x55\x1D\x28" ), CRYPT_CERTINFO_CRLSTREAMIDENTIFIER,
	  DESCRIPTION( "crlStreamIdentifier" )
	  ENCODING( INTEGER ),
	  ATTR_TYPEINFO( CRL, PKIX_FULL ) | FL_ATTR_ATTREND,
	  0, RANGE( 0, 64 ) },
#endif /* USE_REV */

	/* statusReferrals:

		OID = 2 5 29 46
		critical = TRUE
		SEQUENCE OF {
			cRLReferral [ 0 ] EXPLICIT SEQUENCE {
				issuer					  [ 0 ]	GeneralName OPTIONAL,
				location				  [ 1 ]	GeneralName OPTIONAL,
				deltaRefInfo [ 2 ] SEQUENCE {
					deltaLocation				GeneralName,
					lastDelta					GeneralizedTime OPTIONAL
					} OPTIONAL,
				cRLScope SEQUENCE OF {
					SEQUENCE {
						authorityName	  [ 0 ]	GeneralName OPTIONAL,
						distributionPoint [ 1 ][ 0 ] EXPLICIT GeneralName OPTIONAL,
						onlyContains	  [ 2 ]	BIT STRING OPTIONAL,	-- 3 bits
						onlySomeReasons   [ 4 ]	BIT STRING OPTIONAL, -- 9 bits
						serialNumberRange [ 5 ] SEQUENCE {
							startingNumber[ 0 ]	INTEGER OPTIONAL,
							endingNumber  [ 1 ]	INTEGER OPTIONAL
							} OPTIONAL,
						subjectKeyIdRange [ 6 ] SEQUENCE {
							startingKeyId [ 0 ]	INTEGER OPTIONAL,
							endingKeyId	  [ 1 ]	INTEGER OPTIONAL
							} OPTIONAL,
						nameSubtrees	  [ 7 ] SEQUENCE OF GeneralName OPTIONAL,
						baseRevocationInfo [ 9 ] SEQUENCE {
							cRLStatusIndicator [ 0 ] INTEGER OPTIONAL,
							cRLNumber		   [ 1 ] INTEGER,
							baseThisUpdate	   [ 2 ] GeneralizedTime
							}
						}
					}
				lastUpdate [ 3 ] GeneralizedTime OPTIONAL,
				lastChangedCRL [ 4 ] GeneralizedTime OPTIONAL
				}
			}

		To think that someone actually did this on purpose!

		This is a crazy extension that, in effect, says that the CRL that 
		you're holding in your hands isn't really a CRL at all but a pointer
		to locations where actual CRL information may be found, assuming that
		the processing application knows about this bizarre interpretation.
		Since the likelihood of this happening is essentially zero (there are 
		no known implementations of this at present), we don't use it */

	/* freshestCRL:

		OID = 2 5 29 46
		SEQUENCE OF {
			SEQUENCE {
				distributionPoint
							  [ 0 ]	{				-- CHOICE { ... }
					fullName  [ 0 ]	SEQUENCE OF GeneralName
					} OPTIONAL,
				reasons		  [ 1 ]	BIT STRING OPTIONAL,
				cRLIssuer	  [ 2 ]	SEQUENCE OF GeneralName OPTIONAL
				}
			} */
	{ MKOID( "\x06\x03\x55\x1D\x2E" ), CRYPT_CERTINFO_FRESHESTCRL,
	  DESCRIPTION( "freshestCRL" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO2( CERT, ATTRCERT, PKIX_FULL ),
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "freshestCRL.distributionPoint" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "freshestCRL.distributionPoint.distributionPoint" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "freshestCRL.distributionPoint.distributionPoint.fullName" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_FRESHESTCRL_FULLNAME,
	  DESCRIPTION( "freshestCRL.distributionPoint.distributionPoint.fullName.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, CRYPT_CERTINFO_FRESHESTCRL_REASONS,
	  DESCRIPTION( "freshestCRL.distributionPoint.reasons" )
	  ENCODING_TAGGED( BITSTRING, 1 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE( 0, CRYPT_CRLREASONFLAG_LAST ) },
	{ NULL, 0,
	  DESCRIPTION( "freshestCRL.distributionPoint.cRLIssuer" )
	  ENCODING_TAGGED( SEQUENCE, 2 ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER,
	  DESCRIPTION( "freshestCRL.distributionPoint.cRLIssuer.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2 /*or _3*/, ENCODED_OBJECT( generalNameInfo ) },

#ifdef USE_REV
	/* orderedList:

		OID = 2 5 29 47
		ENUMERATED */
	{ MKOID( "\x06\x03\x55\x1D\x2F" ), CRYPT_CERTINFO_ORDEREDLIST,
	  DESCRIPTION( "orderedList" )
	  ENCODING( ENUMERATED ),
	  ATTR_TYPEINFO( CRL, PKIX_FULL ) | FL_ATTR_ATTREND,
	  0, RANGE( 0, 1 ) },

	/* baseUpdateTime:

		OID = 2 5 29 51
		GeneralizedTime */
	{ MKOID( "\x06\x03\x55\x1D\x33" ), CRYPT_CERTINFO_BASEUPDATETIME,
	  DESCRIPTION( "baseUpdateTime" )
	  ENCODING( TIME_GENERALIZED ),
	  ATTR_TYPEINFO( CRL, PKIX_FULL ),
	  FL_ATTR_ATTREND, RANGE_TIME },

	/* deltaInfo:

		OID = 2 5 29 53
		SEQUENCE {
			deltaLocation		GeneralName,
			nextDelta			GeneralizedTime OPTIONAL
			} */
	{ MKOID( "\x06\x03\x55\x1D\x35" ), CRYPT_CERTINFO_DELTAINFO,
	  DESCRIPTION( "deltaInfo" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CRL, PKIX_FULL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_DELTAINFO_LOCATION,
	  DESCRIPTION( "deltaInfo.deltaLocation" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, 0, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, CRYPT_CERTINFO_DELTAINFO_NEXTDELTA,
	  DESCRIPTION( "deltaInfo.nextDelta" )
	  ENCODING( TIME_GENERALIZED ),
	  FL_ATTR_ATTREND, FL_SEQEND, RANGE_TIME },
#endif /* USE_REV */

	/* inhibitAnyPolicy:

		OID = 2 5 29 54
		INTEGER */
	{ MKOID( "\x06\x03\x55\x1D\x36" ), CRYPT_CERTINFO_INHIBITANYPOLICY,
	  DESCRIPTION( "inhibitAnyPolicy" )
	  ENCODING( INTEGER ),
	  ATTR_TYPEINFO( CERT, PKIX_FULL ) | FL_ATTR_ATTREND,
	  0, RANGE( 0, 64 ) },

#ifdef USE_REV
	/* toBeRevoked:

		OID = 2 5 29 58
		SEQUENCE OF SEQUENCE {
			certificateIssuer [ 0 ]	GeneralName OPTIONAL,
			reasonInfo		  [ 1 ] SEQUENCE {
				reasonCode			ENUMERATED
				} OPTIONAL,
			revocationTime			GeneralizedTime,
			certificateGroup  [ 0 ]	EXPLICIT SEQUENCE OF {
				certificateSerialNumber 
									INTEGER
				}
			} 

	   Although the certificateSerialNumber should be an INTEGER it's really 
	   an INTEGER equivalent of an OCTET STRING hole so we call it an OCTET 
	   STRING to make sure that it gets handled appropriately */
	{ MKOID( "\x06\x03\x55\x1D\x3A" ), CRYPT_CERTINFO_TOBEREVOKED,
	  DESCRIPTION( "toBeRevoked" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CRL, PKIX_FULL ),
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "toBeRevoked.group" )
	  ENCODING( SEQUENCE ),
	  0, FL_MULTIVALUED, RANGE_NONE }
	{ NULL, CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER,
	  DESCRIPTION( "toBeRevoked.group.certificateIssuer" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "toBeRevoked.group.reasonInfo" )
	  ENCODING_TAGGED( SEQUENCE, 1 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE_NONE }
	{ NULL, CRYPT_CERTINFO_TOBEREVOKED_REASONCODE,
	  DESCRIPTION( "toBeRevoked.group.reasonInfo.reasonCode" )
	  ENCODING( ENUMERATED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, RANGE( 0, CRYPT_CRLREASON_LAST ) },
	{ 0, CRYPT_CERTINFO_TOBEREVOKED_REVOCATIONTIME,
	  DESCRIPTION( "toBeRevoked.group.revocationTime" )
	  ENCODING( TIME_UTC ),
	  0, FL_MULTIVALUED, RANGE_TIME },
	{ NULL, 0,
	  DESCRIPTION( "toBeRevoked.group.certificateGroup" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_EXPLICIT, RANGE_NONE }
	{ NULL, CRYPT_CERTINFO_TOBEREVOKED_CERTSERIALNUMBER,
	  DESCRIPTION( "toBeRevoked.group.certificateGroup.certificateSerialNumber" )
	  ENCODING_TAGGED( OCTETSTRING, BER_INTEGER ),	/* Actually an INTEGER hole */
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_3, RANGE( 1, 64 ) },

	/* revokedGroups:

		OID = 2 5 29 59
		SEQUENCE {
			certificateIssuer [ 0 ]	GeneralName OPTIONAL,
			reasonInfo		  [ 1 ] SEQUENCE {
				reasonCode			ENUMERATED
				} OPTIONAL,
			invalidityDate	  [ 2 ]	GeneralizedTime OPTIONAL,
			revokedCertificateGroup [ 3 ] EXPLICIT SEQUENCE {
				startingNumber[ 0 ]	INTEGER,
				endingNumber  [ 1 ] INTEGER
				}
			} 

	   Although the revokedCertificateGroup serial numbers should INTEGERs 
	   they're really the INTEGER equivalent of OCTET STRING holes so we 
	   call them OCTET STRINGs to make sure that they get handled 
	   appropriately */
	{ MKOID( "\x06\x03\x55\x1D\x3B" ), CRYPT_CERTINFO_REVOKEDGROUPS,
	  DESCRIPTION( "revokedGroups" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CRL, PKIX_FULL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER,
	  DESCRIPTION( "revokedGroups.certificateIssuer" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "revokedGroups.reasonInfo" )
	  ENCODING_TAGGED( SEQUENCE, 1 ),
	  0, FL_OPTIONAL, RANGE_NONE }
	{ NULL, CRYPT_CERTINFO_REVOKEDGROUPS_REASONCODE,
	  DESCRIPTION( "revokedGroups.reasonCode" )
	  ENCODING( ENUMERATED ),
	  0, FL_OPTIONAL | FL_SEQEND, RANGE( 0, CRYPT_CRLREASON_LAST ) },
	{ 0, CRYPT_CERTINFO_REVOKEDGROUPS_INVALIDITYDATE,
	  DESCRIPTION( "revokedGroups.revocationTime" )
	  ENCODING_TAGGED( TIME_GENERALIZED, 2 ),
	  0, FL_OPTIONAL, RANGE_TIME },
	{ NULL, 0,
	  DESCRIPTION( "revokedGroups.revokedCertificateGroup" )
	  ENCODING_TAGGED( SEQUENCE, 3 ),
	  0, FL_EXPLICIT, RANGE_NONE }
	{ NULL, CRYPT_CERTINFO_REVOKEDGROUPS_STARTINGNUMBER,
	  DESCRIPTION( "revokedGroups.revokedCertificateGroup.startingNumber" )
	  ENCODING_TAGGED( OCTETSTRING, BER_INTEGER ),	/* Actually an INTEGER hole */
	  0, 0, RANGE( 1, 64 ) },
	{ NULL, CRYPT_CERTINFO_REVOKEDGROUPS_ENDINGNUMBER,
	  DESCRIPTION( "revokedGroups.revokedCertificateGroup.endingNumber" )
	  ENCODING_TAGGED( OCTETSTRING, BER_INTEGER ),	/* Actually an INTEGER hole */
	  FL_ATTR_ATTREND, FL_SEQEND_2, RANGE( 1, 64 ) },

	/* expiredCertsOnCRL:

		OID = 2 5 29 60
		GeneralizedTime */
	{ MKOID( "\x06\x03\x55\x1D\x3C" ), CRYPT_CERTINFO_EXPIREDCERTSONCRL,
	  DESCRIPTION( "expiredCertsOnCRL" )
	  ENCODING( TIME_GENERALIZED ),
	  ATTR_TYPEINFO( CRL, PKIX_FULL ),
	  FL_ATTR_ATTREND, RANGE_TIME },

	/* aaIssuingDistributionPoint:

		OID = 2 5 29 63
		critical = TRUE
		SEQUENCE {
			distributionPoint [ 0 ]	{
				fullName	  [ 0 ]	EXPLICIT GeneralName	-- CHOICE ...
				} OPTIONAL,
			onlySomeReasons	  [ 1 ]	BITSTRING OPTIONAL,
			indirectCRL		  [ 2 ]	BOOLEAN DEFAULT FALSE
			containsUserAttributeCerts
							  [ 3 ]	BOOLEAN DEFAULT TRUE,
			containsAACerts	  [ 4 ]	BOOLEAN DEFAULT TRUE,
			containsSOAPublicKeyCerts	  
							  [ 5 ]	BOOLEAN DEFAULT TRUE
		} */
	{ MKOID( "\x06\x03\x55\x1D\x3F" ), CRYPT_CERTINFO_AAISSUINGDISTRIBUTIONPOINT,
	  DESCRIPTION( "aaIssuingDistributionPoint" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CRITICAL( CRL, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "aaIssuingDistributionPoint.distributionPoint" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "aaIssuingDistributionPoint.distributionPoint.fullName" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME,
	  DESCRIPTION( "aaIssuingDistributionPoint.distributionPoint.fullName.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_3, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, CRYPT_CERTINFO_AAISSUINGDIST_SOMEREASONSONLY,
	  DESCRIPTION( "aaIssuingDistributionPoint.onlySomeReasons" )
	  ENCODING_TAGGED( BITSTRING, 1 ),
	  0, FL_OPTIONAL, RANGE( 0, CRYPT_CRLREASONFLAG_LAST ) },
	{ NULL, CRYPT_CERTINFO_AAISSUINGDIST_INDIRECTCRL,
	  DESCRIPTION( "aaIssuingDistributionPoint.indirectCRL" )
	  ENCODING_TAGGED( BOOLEAN, 2 ),
	  0, FL_OPTIONAL | FL_DEFAULT, RANGE_BOOLEAN },
	{ NULL, CRYPT_CERTINFO_AAISSUINGDIST_USERATTRCERTS,
	  DESCRIPTION( "aaIssuingDistributionPoint.containsUserAttributeCerts" )
	  ENCODING_TAGGED( BOOLEAN, 3 ),
	  0, FL_OPTIONAL | FL_DEFAULT, RANGE_BOOLEAN },
	{ NULL, CRYPT_CERTINFO_AAISSUINGDIST_AACERTS,
	  DESCRIPTION( "aaIssuingDistributionPoint.containsAACerts" )
	  ENCODING_TAGGED( BOOLEAN, 4 ),
	  0, FL_OPTIONAL | FL_DEFAULT, RANGE_BOOLEAN },
	{ NULL, CRYPT_CERTINFO_AAISSUINGDIST_SOACERTS,
	  DESCRIPTION( "aaIssuingDistributionPoint.containsSOAPublicKeyCerts" )
	  ENCODING_TAGGED( BOOLEAN, 5 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_DEFAULT | FL_SEQEND /*NONE*/, RANGE_BOOLEAN },
#endif /* USE_REV */
#endif /* USE_CERTLEVEL_PKIX_FULL */

#ifdef USE_CERT_OBSOLETE
	/* netscape-cert-type:

		OID = 2 16 840 1 113730 1 1
		BITSTRING */
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x01\x01" ), CRYPT_CERTINFO_NS_CERTTYPE,
	  DESCRIPTION( "netscape-cert-type" )
	  ENCODING( BITSTRING ),
	  ATTR_TYPEINFO( CERT, STANDARD ) | FL_ATTR_ATTREND,
	  0, RANGE( 0, CRYPT_NS_CERTTYPE_LAST ) },

	/* netscape-base-url:

		OID = 2 16 840 1 113730 1 2
		IA5String */
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x01\x02" ), CRYPT_CERTINFO_NS_BASEURL,
	  DESCRIPTION( "netscape-base-url" )
	  ENCODING( STRING_IA5 ),
	  ATTR_TYPEINFO( CERT, STANDARD ) | FL_ATTR_ATTREND,
	  0, CHECK_HTTP },

	/* netscape-revocation-url:

		OID = 2 16 840 1 113730 1 3
		IA5String */
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x01\x03" ), CRYPT_CERTINFO_NS_REVOCATIONURL,
	  DESCRIPTION( "netscape-revocation-url" )
	  ENCODING( STRING_IA5 ),
	  ATTR_TYPEINFO( CERT, STANDARD ) | FL_ATTR_ATTREND,
	  0, CHECK_HTTP },

	/* netscape-ca-revocation-url:

		OID = 2 16 840 1 113730 1 3
		IA5String */
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x01\x04" ), CRYPT_CERTINFO_NS_CAREVOCATIONURL,
	  DESCRIPTION( "netscape-ca-revocation-url" )
	  ENCODING( STRING_IA5 ),
	  ATTR_TYPEINFO( CERT, STANDARD ) | FL_ATTR_ATTREND,
	  0, CHECK_HTTP },

	/* netscape-ca-revocation-url:

		OID = 2 16 840 1 113730 11 7
		IA5String */
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x01\x07" ), CRYPT_CERTINFO_NS_CERTRENEWALURL,
	  DESCRIPTION( "netscape-ca-revocation-url" )
	  ENCODING( STRING_IA5 ),
	  ATTR_TYPEINFO( CERT, STANDARD ) | FL_ATTR_ATTREND,
	  0, CHECK_HTTP },

	/* netscape-ca-policy-url:

		OID = 2 16 840 1 113730 1 8
		IA5String */
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x01\x08" ), CRYPT_CERTINFO_NS_CAPOLICYURL,
	  DESCRIPTION( "netscape-ca-policy-url" )
	  ENCODING( STRING_IA5 ),
	  ATTR_TYPEINFO( CERT, STANDARD ) | FL_ATTR_ATTREND,
	  0, CHECK_HTTP },

	/* netscape-ssl-server-name:

		OID = 2 16 840 1 113730 1 12
		IA5String */
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x01\x0C" ), CRYPT_CERTINFO_NS_SSLSERVERNAME,
	  DESCRIPTION( "netscape-ssl-server-name" )
	  ENCODING( STRING_IA5 ),
	  ATTR_TYPEINFO( CERT, STANDARD ) | FL_ATTR_ATTREND,
	  0, CHECK_DNS },

	/* netscape-comment:

		OID = 2 16 840 1 113730 1 13
		IA5String */
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x01\x0D" ), CRYPT_CERTINFO_NS_COMMENT,
	  DESCRIPTION( "netscape-comment" )
	  ENCODING( STRING_IA5 ),
	  ATTR_TYPEINFO( CERT, STANDARD ) | FL_ATTR_ATTREND,
	  0, RANGE_ATTRIBUTEBLOB },
#endif /* USE_CERT_OBSOLETE */

#ifdef USE_CERT_OBSOLETE
	/* hashedRootKey (SET):

		OID = 2 23 42 7 0
		critical = TRUE
		SEQUENCE {
			rootKeyThumbprint	DigestedData		-- PKCS #7-type wrapper
			} */
	{ MKOID( "\x06\x04\x67\x2A\x07\x00" ), CRYPT_CERTINFO_SET_HASHEDROOTKEY,
	  DESCRIPTION( "hashedRootKey" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CRITICAL( CERT, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "hashedRootKey.rootKeyThumbprint" )
	  ENCODING_SPECIAL( BLOB_SEQUENCE ),		/* PKCS #7-type wrapper */
	  0, FL_NONENCODING, 0, 0, 25,
	  "\x30\x2D\x02\x01\x00\x30\x09\x06\x05\x2B\x0E\x03\x02\x1A\x05\x00\x30\x07\x06\x05\x67\x2A\x03\x00\x00" },
	{ NULL, CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT,
	  DESCRIPTION( "hashedRootKey.rootKeyThumbprint.hashData" )
	  ENCODING( OCTETSTRING ),
	  FL_ATTR_ATTREND, FL_SEQEND /*NONE*/, RANGE( 20, 20 ) },

	/* certificateType (SET):

		OID = 2 23 42 7 1
		critical = TRUE
		BIT STRING */
	{ MKOID( "\x06\x04\x67\x2A\x07\x01" ), CRYPT_CERTINFO_SET_CERTIFICATETYPE,
	  DESCRIPTION( "certificateType" )
	  ENCODING( BITSTRING ),
	  ATTR_TYPEINFO_CRITICAL( CERT, PKIX_PARTIAL ) | FL_ATTR_ATTREND,
	  0, RANGE( 0, CRYPT_SET_CERTTYPE_LAST ) },

	/* merchantData (SET):

		OID = 2 23 42 7 2
		SEQUENCE {
			merID				SETString SIZE(1..30),
			merAcquirerBIN		NumericString SIZE(6),
			merNameSeq			SEQUENCE OF MerNames,
			merCountry			INTEGER (1..999),
			merAuthFlag			BOOLEAN DEFAULT TRUE
			}

		MerNames ::= SEQUENCE {
			language	  [ 0 ] VisibleString SIZE(1..35),
			name		  [ 1 ]	EXPLICIT SETString SIZE(1..50),
			city		  [ 2 ]	EXPLICIT SETString SIZE(1..50),
			stateProvince [ 3 ] EXPLICIT SETString SIZE(1..50) OPTIONAL,
			postalCode	  [ 4 ] EXPLICIT SETString SIZE(1..14) OPTIONAL,
			countryName	  [ 5 ]	EXPLICIT SETString SIZE(1..50)
			} */
	{ MKOID( "\x06\x04\x67\x2A\x07\x02" ), CRYPT_CERTINFO_SET_MERCHANTDATA,
	  DESCRIPTION( "merchantData" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SET_MERID,
	  DESCRIPTION( "merchantData.merID" )
	  ENCODING( STRING_ISO646 ),
	  0, 0, RANGE( 1, 30 ) },
	{ NULL, CRYPT_CERTINFO_SET_MERACQUIRERBIN,
	  DESCRIPTION( "merchantData.merAcquirerBIN" )
	  ENCODING( STRING_NUMERIC ),
	  0, 0, RANGE( 6, 6 ) },
	{ NULL, 0,
	  DESCRIPTION( "merchantData.merNameSeq" )
	  ENCODING( SEQUENCE ),
	  0, FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "merchantData.merNameSeq.merNames" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SET_MERCHANTLANGUAGE,
	  DESCRIPTION( "merchantData.merNameSeq.merNames.language" )
	  ENCODING_TAGGED( STRING_ISO646, 0 ),
	  0, FL_MULTIVALUED, RANGE( 1, 35 ) },
	{ NULL, CRYPT_CERTINFO_SET_MERCHANTNAME,
	  DESCRIPTION( "merchantData.merNameSeq.merNames.name" )
	  ENCODING_TAGGED( STRING_ISO646, 1 ),
	  0, FL_MULTIVALUED | FL_EXPLICIT, RANGE( 1, 50 ) },
	{ NULL, CRYPT_CERTINFO_SET_MERCHANTCITY,
	  DESCRIPTION( "merchantData.merNameSeq.merNames.city" )
	  ENCODING_TAGGED( STRING_ISO646, 2 ),
	  0, FL_MULTIVALUED | FL_EXPLICIT, RANGE( 1, 50 ) },
	{ NULL, CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE,
	  DESCRIPTION( "merchantData.merNameSeq.merNames.stateProvince" )
	  ENCODING_TAGGED( STRING_ISO646, 3 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_EXPLICIT, RANGE( 1, 50 ) },
	{ NULL, CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE,
	  DESCRIPTION( "merchantData.merNameSeq.merNames.postalCode" )
	  ENCODING_TAGGED( STRING_ISO646, 4 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_EXPLICIT, RANGE( 1, 50 ) },
	{ NULL, CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME,
	  DESCRIPTION( "merchantData.merNameSeq.merNames.countryName" )
	  ENCODING_TAGGED( STRING_ISO646, 5 ),
	  0, FL_MULTIVALUED | FL_EXPLICIT | FL_SEQEND_2, RANGE( 1, 50 ) },
	{ NULL, CRYPT_CERTINFO_SET_MERCOUNTRY,
	  DESCRIPTION( "merchantData.merCountry" )
	  ENCODING( INTEGER ),
	  0, 0, RANGE( 1, 999 ) },
	{ NULL, CRYPT_CERTINFO_SET_MERAUTHFLAG,
	  DESCRIPTION( "merchantData.merAuthFlag" )
	  ENCODING( BOOLEAN ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_DEFAULT | FL_SEQEND /*NONE*/, RANGE_BOOLEAN },

	/* certCardRequired (SET):

		OID = 2 23 42 7 3
		BOOLEAN */
	{ MKOID( "\x06\x04\x67\x2A\x07\x03" ), CRYPT_CERTINFO_SET_CERTCARDREQUIRED,
	  DESCRIPTION( "certCardRequired" )
	  ENCODING( BOOLEAN ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ) | FL_ATTR_ATTREND,
	  0, RANGE_BOOLEAN },

	/* tunneling (SET):

		OID = 2 23 42 7 4
		SEQUENCE {
			tunneling 		DEFAULT TRUE,
			tunnelAlgIDs	SEQUENCE OF OBJECT IDENTIFIER
			} */
	{ MKOID( "\x06\x04\x67\x2A\x07\x04" ), CRYPT_CERTINFO_SET_TUNNELING,
	  DESCRIPTION( "tunneling" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO( CERT, PKIX_PARTIAL ),
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SET_TUNNELINGFLAG,
	  DESCRIPTION( "tunneling.tunneling" )
	  ENCODING( BOOLEAN ),
	  0, FL_OPTIONAL | FL_DEFAULT, FALSE, TRUE, TRUE },
	{ NULL, 0,
	  DESCRIPTION( "tunneling.tunnelingAlgIDs" )
	  ENCODING( SEQUENCE ),
	  0, FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_SET_TUNNELINGALGID,
	  DESCRIPTION( "tunneling.tunnelingAlgIDs.tunnelingAlgID" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  FL_ATTR_ATTREND, FL_MULTIVALUED | FL_SEQEND, RANGE_OID },
#endif /* USE_CERT_OBSOLETE */

	{ NULL, CRYPT_IATTRIBUTE_LAST }, { NULL, CRYPT_IATTRIBUTE_LAST }
	};

#if ( defined( USE_CERTREV ) || defined( USE_CERTREQ ) ) && \
	defined( USE_CERTLEVEL_PKIX_FULL )

/* Subtable for encoding the holdInstructionCode */

STATIC_DATA const ATTRIBUTE_INFO FAR_BSS holdInstructionInfo[] = {
	{ MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x02\x01" ), CRYPT_HOLDINSTRUCTION_NONE,
	  DESCRIPTION( "holdInstructionCode.holdinstruction-none (1 2 840 10040 2 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  FL_ATTR_ATTRSTART, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x02\x02" ), CRYPT_HOLDINSTRUCTION_CALLISSUER,
	  DESCRIPTION( "holdInstructionCode.holdinstruction-callissuer (1 2 840 10040 2 2)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x02\x03" ), CRYPT_HOLDINSTRUCTION_REJECT,
	  DESCRIPTION( "holdInstructionCode.holdinstruction-reject (1 2 840 10040 2 3)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x02\x04" ), CRYPT_HOLDINSTRUCTION_PICKUPTOKEN,
	  DESCRIPTION( "holdInstructionCode.holdinstruction-pickupToken (1 2 840 10040 2 4)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  FL_ATTR_ATTREND, FL_OPTIONAL, RANGE_NONE },

	{ NULL, CRYPT_IATTRIBUTE_LAST }, { NULL, CRYPT_IATTRIBUTE_LAST }
	};
#endif /* ( USE_CERTREV || USE_CERTREQ ) && USE_CERTLEVEL_PKIX_FULL */

/****************************************************************************
*																			*
*								GeneralName Definition						*
*																			*
****************************************************************************/

/* Encoding and decoding of GeneralNames is performed with the following
   subtable:

	otherName		  [ 0 ]	SEQUENCE {
		type-id				OBJECT IDENTIFIER,
		value		  [ 0 ]	EXPLICIT ANY DEFINED BY type-id
		} OPTIONAL,
	rfc822Name		  [ 1 ]	IA5String OPTIONAL,
	dNSName			  [ 2 ]	IA5String OPTIONAL,
	x400Address		  [ 3 ] ITU-BrainDamage OPTIONAL
	directoryName	  [ 4 ]	EXPLICIT Name OPTIONAL,
	ediPartyName 	  [ 5 ]	SEQUENCE {
		nameAssigner  [ 0 ]	EXPLICIT DirectoryString OPTIONAL,
		partyName	  [ 1 ]	EXPLICIT DirectoryString
		} OPTIONAL,
	uniformResourceIdentifier
					  [ 6 ]	IA5String OPTIONAL,
	iPAddress		  [ 7 ]	OCTET STRING OPTIONAL,
	registeredID	  [ 8 ]	OBJECT IDENTIFIER OPTIONAL

	ITU-Braindamge ::= SEQUENCE {
		built-in-standard-attributes		SEQUENCE {
			country-name  [ APPLICATION 1 ]	CHOICE {
				x121-dcc-code				NumericString,
				iso-3166-alpha2-code		PrintableString
				},
			administration-domain-name
						  [ APPLICATION 2 ]	CHOICE {
				numeric						NumericString,
				printable					PrintableString
				},
			network-address			  [ 0 ]	NumericString OPTIONAL,
			terminal-identifier		  [ 1 ]	PrintableString OPTIONAL,
			private-domain-name		  [ 2 ]	CHOICE {
				numeric						NumericString,
				printable					PrintableString
				} OPTIONAL,
			organization-name		  [ 3 ]	PrintableString OPTIONAL,
			numeric-use-identifier	  [ 4 ]	NumericString OPTIONAL,
			personal-name			  [ 5 ]	SET {
				surname				  [ 0 ]	PrintableString,
				given-name			  [ 1 ]	PrintableString,
				initials			  [ 2 ]	PrintableString,
				generation-qualifier  [ 3 ]	PrintableString
				} OPTIONAL,
			organizational-unit-name  [ 6 ]	PrintableString OPTIONAL,
			}
		built-in-domain-defined-attributes	SEQUENCE OF {
			type							PrintableString SIZE(1..64),
			value							PrintableString SIZE(1..64)
			} OPTIONAL
		extensionAttributes					SET OF SEQUENCE {
			extension-attribute-type  [ 0 ]	INTEGER,
			extension-attribute-value [ 1 ]	ANY DEFINED BY extension-attribute-type
			} OPTIONAL
		}

   Needless to say X.400 addresses aren't supported (for readers who've
   never seen one before you now know why they've been so enormously
   successful).

   Note the special-case encoding of the DirectoryName and EDIPartyName.
   This is required because (for the DirectoryName) a Name is actually a
   CHOICE { RDNSequence } and if the tagging were implicit then there'd be
   no way to tell which of the CHOICE options was being used:

	directoryName	  [ 4 ]	Name OPTIONAL

   becomes:

	directoryName	  [ 4 ]	CHOICE { RDNSequence } OPTIONAL

   which, if implicit tagging is used, would replace the RDNSequence tag with
   the [4] tag, making it impossible to determine which of the Name choices
   was used (actually there's only one possibility and it's unlikely that
   there'll ever be more but that's what the encoding rules require - X.208,
   section 26.7c).

   The same applies to the EDIPartyName, this is a DirectoryString which is
   a CHOICE of several possible string types.  The end result is that:

	[ 0 ] DirectoryString

   ends up looking like:

	[ 0 ] CHOICE { PrintableString | T61String | UTF8String | BMPString }

   In addition to the above complications, newer versions of the PKIX core 
   RFC allow the use of 8- and 32-byte CIDR forms for 4- and 16-byte IP 
   addresses in some instances when they're being used as constraints.  We 
   can add support for this if anyone ever asks for it */

STATIC_DATA const ATTRIBUTE_INFO FAR_BSS generalNameInfo[] = {
	/* otherName */
	{ NULL, FIELDID_FOLLOWS,
	  DESCRIPTION( "generalName.otherName" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  FL_ATTR_ATTRSTART, FL_OPTIONAL, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_OTHERNAME_TYPEID,
	  DESCRIPTION( "generalName.otherName.type-id" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_OTHERNAME_VALUE,
	  DESCRIPTION( "generalName.otherName.value" )
	  ENCODING_SPECIAL_TAGGED( BLOB_ANY, 0 ),
	  0, FL_OPTIONAL | FL_EXPLICIT | FL_SEQEND, RANGE( 3, MAX_ATTRIBUTE_SIZE ) },
	
	/* rfc822Name */
	{ NULL, CRYPT_CERTINFO_RFC822NAME,
	  DESCRIPTION( "generalName.rfc822Name" )
	  ENCODING_TAGGED( STRING_IA5, 1 ),
	  0, FL_OPTIONAL, CHECK_RFC822 },
	
	/* dNSName */
	{ NULL, CRYPT_CERTINFO_DNSNAME,
	  DESCRIPTION( "generalName.dNSName" )
	  ENCODING_TAGGED( STRING_IA5, 2 ),
	  0, FL_OPTIONAL, CHECK_DNS },
	
	/* directoryName */
	{ NULL, CRYPT_CERTINFO_DIRECTORYNAME,
	  DESCRIPTION( "generalName.directoryName" )
	  ENCODING_SPECIAL_TAGGED( DN, 4 ),
	  0, FL_OPTIONAL | FL_EXPLICIT, CHECK_X500 },

	/* ediPartyName */
	{ NULL, 0,
	  DESCRIPTION( "generalName.ediPartyName" )
	  ENCODING_TAGGED( SEQUENCE, 5 ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "generalName.ediPartyName.nameAssigner" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_OPTIONAL, RANGE_TEXTSTRING },
	  /* See note above on why the extra SEQUENCE is present */
	{ NULL, CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER,
	  DESCRIPTION( "generalName.ediPartyName.nameAssigner.directoryName" )
	  ENCODING_SPECIAL( TEXTSTRING ),
	  0, FL_OPTIONAL | FL_SEQEND, RANGE_TEXTSTRING },
	{ NULL, 0,
	  DESCRIPTION( "generalName.ediPartyName.partyName" )
	  ENCODING_TAGGED( SEQUENCE, 1 ),
	  0, FL_OPTIONAL, RANGE_TEXTSTRING },
	{ NULL, CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME,
	  DESCRIPTION( "generalName.ediPartyName.partyName.directoryName" )
	  ENCODING_SPECIAL( TEXTSTRING ),
	  0, FL_OPTIONAL | FL_SEQEND_2, RANGE_TEXTSTRING },
	
	/* uniformResourceIdentifier */
	{ NULL, CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER,
	  DESCRIPTION( "generalName.uniformResourceIdentifier" )
	  ENCODING_TAGGED( STRING_IA5, 6 ),
	  0, FL_OPTIONAL, CHECK_URL },
	
	/* iPAddress */
	{ NULL, CRYPT_CERTINFO_IPADDRESS,
	  DESCRIPTION( "generalName.iPAddress" )
	  ENCODING_TAGGED( OCTETSTRING, 7 ),
	  0, FL_OPTIONAL, RANGE( 4, 16 ) },

	/* registeredID */
	{ NULL, CRYPT_CERTINFO_REGISTEREDID,
	  DESCRIPTION( "generalName.registeredID" )
	  ENCODING_TAGGED( OBJECT_IDENTIFIER, 8 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL, RANGE_OID },

	{ NULL, CRYPT_IATTRIBUTE_LAST }, { NULL, CRYPT_IATTRIBUTE_LAST }
	};

/****************************************************************************
*																			*
*							CMS Attribute Definitions						*
*																			*
****************************************************************************/

/* CMS attributes are encoded using the following table */

static const ATTRIBUTE_INFO FAR_BSS cmsAttributeInfo[] = {
	/* contentType:

		OID = 1 2 840 113549 1 9 3
		OBJECT IDENTIFIER */
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x03" ), CRYPT_CERTINFO_CMS_CONTENTTYPE,
	  DESCRIPTION( "contentType" )
	  ENCODING_SPECIAL( CHOICE ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, CRYPT_CONTENT_DATA, CRYPT_CONTENT_LAST, 0, ( void * ) contentTypeInfo },

	/* messageDigest:

		OID = 1 2 840 113549 1 9 4
		OCTET STRING */
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x04" ), CRYPT_CERTINFO_CMS_MESSAGEDIGEST,
	  DESCRIPTION( "messageDigest" )
	  ENCODING( OCTETSTRING ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE( 16, CRYPT_MAX_HASHSIZE ) },

	/* signingTime:

		OID = 1 2 840 113549 1 9 5
		CHOICE {
			utcTime			UTCTime,				-- Up to 2049
			generalizedTime	GeneralizedTime
			} */
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x05" ), CRYPT_CERTINFO_CMS_SIGNINGTIME,
	  DESCRIPTION( "signingTime" )
	  ENCODING( TIME_UTC ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE_TIME },

#ifdef USE_CMSATTR_OBSCURE
	/* counterSignature:

		OID = 1 2 840 113549 1 9 6
		CHOICE {
			utcTime			UTCTime,				-- Up to 2049
			generalizedTime	GeneralizedTime
			}

	   This field isn't an authenticated attribute so it isn't used */
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x06" ), CRYPT_CERTINFO_CMS_COUNTERSIGNATURE,
	  DESCRIPTION( "counterSignature" )
	  ENCODING( NULL ),		/* Dummy value */
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE_NONE },

  	/* signingDescription:

		OID = 1 2 840 113549 1 9 13
		UTF8String */
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x0D" ), CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION,
	  DESCRIPTION( "signingDescription" )
	  ENCODING( STRING_UTF8 ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE_ATTRIBUTEBLOB },
#endif /* USE_CMSATTR_OBSCURE */

	/* sMIMECapabilities:

		OID = 1 2 840 113549 1 9 15
		SEQUENCE OF {
			SEQUENCE {
				capabilityID	OBJECT IDENTIFIER,
				parameters		ANY DEFINED BY capabilityID
				}
			} */
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x0F" ), CRYPT_CERTINFO_CMS_SMIMECAPABILITIES,
	  DESCRIPTION( "sMIMECapabilities" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (des-EDE3-CBC)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x07" ), CRYPT_CERTINFO_CMS_SMIMECAP_3DES,
	  DESCRIPTION( "sMIMECapabilities.capability.des-EDE3-CBC" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (aes128-CBC)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x02" ), CRYPT_CERTINFO_CMS_SMIMECAP_AES,
	  DESCRIPTION( "sMIMECapabilities.capability.aes128-CBC" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (cast5CBC)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF6\x7D\x07\x42\x0A" ), CRYPT_CERTINFO_CMS_SMIMECAP_CAST128,
	  DESCRIPTION( "sMIMECapabilities.capability.cast5CBC" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability.cast5CBC.parameter" )
	  ENCODING_SPECIAL( BLOB_ANY ),	/* 128-bit key */
	  0, FL_NONENCODING | FL_SEQEND, 0, 0, 4, "\x02\x02\x00\x80" },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (sha-ng)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "Unknown" ), CRYPT_CERTINFO_CMS_SMIMECAP_SHAng,
	  DESCRIPTION( "sMIMECapabilities.capability.sha-ng" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (sha2-256)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01" ), CRYPT_CERTINFO_CMS_SMIMECAP_SHA2,
	  DESCRIPTION( "sMIMECapabilities.capability.sha2-256" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (sha1)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x05\x2B\x0E\x03\x02\x1A" ), CRYPT_CERTINFO_CMS_SMIMECAP_SHA1,
	  DESCRIPTION( "sMIMECapabilities.capability.sha1" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (hmac-sha-ng)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "Unknown" ), CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng,
	  DESCRIPTION( "sMIMECapabilities.capability.hmac-sha-ng" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (hmac-sha2-256)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x09" ), CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2,
	  DESCRIPTION( "sMIMECapabilities.capability.hmac-sha2-256" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (hmac-sha1)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x08\x01\x02" ), CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1,
	  DESCRIPTION( "sMIMECapabilities.capability.hmac-sha1" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (authEnc256)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x03\x10" ), CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256,
	  DESCRIPTION( "sMIMECapabilities.capability.authEnc256" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (authEnc128)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x03\x0F" ), CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128,
	  DESCRIPTION( "sMIMECapabilities.capability.authEnc128" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (rsaWithSHAng)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "Unknown" ), CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng,
	  DESCRIPTION( "sMIMECapabilities.capability.rsaWithSHAng" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (rsaWithSHA2-256)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0B" ), CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2,
	  DESCRIPTION( "sMIMECapabilities.capability.rsaWithSHA2-256" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (rsaWithSHA1)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05" ), CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1,
	  DESCRIPTION( "sMIMECapabilities.capability.rsaWithSHA1" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (dsaWithSHA1)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x04\x03" ), CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1,
	  DESCRIPTION( "sMIMECapabilities.capability.dsaWithSHA1" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (ecdsaWithSHA-ng)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "Unknown" ), CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng,
	  DESCRIPTION( "sMIMECapabilities.capability.ecdsaWithSHA-ng" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (ecdsaWithSHA2-256)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x02" ), CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2,
	  DESCRIPTION( "sMIMECapabilities.capability.ecdsaWithSHA2-256" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (ecdsaWithSHA1)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x04\x01" ), CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1,
	  DESCRIPTION( "sMIMECapabilities.capability.ecdsaWithSHA1" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (preferSignedData)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x09\x0F\x01" ), CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA,
	  DESCRIPTION( "sMIMECapabilities.capability.preferSignedData" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (canNotDecryptAny)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x09\x0F\x02" ), CRYPT_CERTINFO_CMS_SMIMECAP_CANNOTDECRYPTANY,
	  DESCRIPTION( "sMIMECapabilities.capability.canNotDecryptAny" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (preferBinaryInside)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x0B\x01" ), CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE,
	  DESCRIPTION( "sMIMECapabilities.capability.preferBinaryInside" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_NONENCODING | FL_SEQEND, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability (catchAll)" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "sMIMECapabilities.capability.catchAll" )
	  ENCODING_SPECIAL( BLOB_ANY ),	/* Match anything and ignore it */
	  FL_ATTR_ATTREND, FL_NONENCODING | FL_SEQEND_2 /*FL_SEQEND*/, RANGE_NONE },

#ifdef USE_CMSATTR_OBSCURE
	/* receiptRequest:

		OID = 1 2 840 113549 1 9 16 2 1
		SEQUENCE {
			contentIdentifier	OCTET STRING,
			receiptsFrom  [ 0 ]	INTEGER (0..1),
			receiptsTo			SEQUENCE {
				SEQUENCE OF GeneralName
				}
			} */
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x01" ), CRYPT_CERTINFO_CMS_RECEIPTREQUEST,
	  DESCRIPTION( "receiptRequest" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_RECEIPT_CONTENTIDENTIFIER,
	  DESCRIPTION( "receiptRequest.contentIdentifier" )
	  ENCODING( OCTETSTRING ),
	  0, 0, RANGE( 16, 64 ) },
	{ NULL, CRYPT_CERTINFO_CMS_RECEIPT_FROM,
	  DESCRIPTION( "receiptRequest.receiptsFrom" )
	  ENCODING_TAGGED( INTEGER, 0 ),
	  0, 0, RANGE( 0, 1 ) },
	{ NULL, 0,
	  DESCRIPTION( "receiptRequest.receiptsTo" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "receiptRequest.receiptsTo.generalNames" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_RECEIPT_TO,
	  DESCRIPTION( "receiptRequest.receiptsTo.generalNames.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  FL_ATTR_ATTREND, FL_MULTIVALUED | FL_SEQEND_3 /*FL_SEQEND_2*/, ENCODED_OBJECT( generalNameInfo ) },

	/* essSecurityLabel:

		OID = 1 2 840 113549 1 9 16 2 2
		SET {
			policyIdentifier	OBJECT IDENTIFIER,
			classification		INTEGER (0..5+6..255) OPTIONAL,
			privacyMark			PrintableString OPTIONAL,
			categories			SET OF {
				SEQUENCE {
					type  [ 0 ]	OBJECT IDENTIFIER,
					value [ 1 ]	ANY DEFINED BY type
					}
				} OPTIONAL
			}

		Because this is a SET we don't order the fields in the sequence
		given in the above ASN.1 but in the order of encoded size to follow
		the DER SET encoding rules */
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x02" ), CRYPT_CERTINFO_CMS_SECURITYLABEL,
	  DESCRIPTION( "essSecurityLabel" )
	  ENCODING( SET ),
	  ATTR_TYPEINFO_CMS, 
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SECLABEL_POLICY,
	  DESCRIPTION( "essSecurityLabel.securityPolicyIdentifier" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, 0, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION,
	  DESCRIPTION( "essSecurityLabel.securityClassification" )
	  ENCODING( INTEGER ),
	  0, FL_OPTIONAL, RANGE( CRYPT_CLASSIFICATION_UNMARKED, CRYPT_CLASSIFICATION_LAST ) },
	{ NULL, CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK,
	  DESCRIPTION( "essSecurityLabel.privacyMark" )
	  ENCODING( STRING_PRINTABLE ),
	  0, FL_OPTIONAL, RANGE( 1, 64 ) },
	{ NULL, 0,
	  DESCRIPTION( "essSecurityLabel.securityCategories" )
	  ENCODING( SET ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "essSecurityLabel.securityCategories.securityCategory" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE,
	  DESCRIPTION( "essSecurityLabel.securityCategories.securityCategory.type" )
	  ENCODING_TAGGED( OBJECT_IDENTIFIER, 0 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE,
	  DESCRIPTION( "essSecurityLabel.securityCategories.securityCategory.value" )
	  ENCODING_SPECIAL_TAGGED( BLOB_ANY, 1 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND /*FL_SEQEND_2, or _3*/, RANGE_ATTRIBUTEBLOB },

	/* mlExpansionHistory:

		OID = 1 2 840 113549 1 9 16 2 3
		SEQUENCE OF {
			SEQUENCE {
				entityIdentifier IssuerAndSerialNumber,	-- Treated as blob
				expansionTime	GeneralizedTime,
				mlReceiptPolicy	CHOICE {
					none		  [ 0 ]	NULL,
					insteadOf	  [ 1 ]	SEQUENCE OF {
						SEQUENCE OF GeneralName		-- GeneralNames
						}
					inAdditionTo  [ 2 ]	SEQUENCE OF {
						SEQUENCE OF GeneralName		-- GeneralNames
						}
					}
				}
			} */
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x03" ), CRYPT_CERTINFO_CMS_MLEXPANSIONHISTORY,
	  DESCRIPTION( "mlExpansionHistory" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "mlExpansionHistory.mlData" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_MLEXP_ENTITYIDENTIFIER,
	  DESCRIPTION( "mlExpansionHistory.mlData.mailListIdentifier.issuerAndSerialNumber" )
	  ENCODING_SPECIAL( BLOB_SEQUENCE ),
	  0, FL_MULTIVALUED, RANGE_ATTRIBUTEBLOB },
	{ NULL, CRYPT_CERTINFO_CMS_MLEXP_TIME,
	  DESCRIPTION( "mlExpansionHistory.mlData.expansionTime" )
	  ENCODING( TIME_GENERALIZED ),
	  0, FL_MULTIVALUED, RANGE_TIME },
	{ NULL, CRYPT_CERTINFO_CMS_MLEXP_NONE,
	  DESCRIPTION( "mlExpansionHistory.mlData.mlReceiptPolicy.none" )
	  ENCODING_TAGGED( NULL, 0 ),
	  0, FL_MULTIVALUED, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "mlExpansionHistory.mlData.mlReceiptPolicy.insteadOf" )
	  ENCODING_TAGGED( SEQUENCE, 1 ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "mlExpansionHistory.mlData.mlReceiptPolicy.insteadOf.generalNames" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF,
	  DESCRIPTION( "mlExpansionHistory.mlData.mlReceiptPolicy.insteadOf.generalNames.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  0, FL_OPTIONAL | FL_SEQEND_2 | FL_MULTIVALUED, ENCODED_OBJECT( generalNameInfo ) },
	{ NULL, 0,
	  DESCRIPTION( "mlExpansionHistory.mlData.mlReceiptPolicy.inAdditionTo" )
	  ENCODING_TAGGED( SEQUENCE, 2 ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "mlExpansionHistory.mlData.mlReceiptPolicy.inAdditionTo.generalNames" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO,
	  DESCRIPTION( "mlExpansionHistory.mlData.mlReceiptPolicy.inAdditionTo.generalNames.generalName" )
	  ENCODING_SPECIAL( SUBTYPED ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_SEQEND_2 /*FL_SEQEND_3, or _4*/ | FL_MULTIVALUED, ENCODED_OBJECT( generalNameInfo ) },

	/* contentHints:

		OID = 1 2 840 113549 1 9 16 2 4
		SEQUENCE {
			contentDescription	UTF8String,
			contentType			OBJECT IDENTIFIER
			} */
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x04" ), CRYPT_CERTINFO_CMS_CONTENTHINTS,
	  DESCRIPTION( "contentHints" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION,
	  DESCRIPTION( "contentHints.contentDescription" )
	  ENCODING( STRING_UTF8 ),
	  0, FL_OPTIONAL, RANGE_TEXTSTRING },
	{ NULL, CRYPT_CERTINFO_CMS_CONTENTHINT_TYPE,
	  DESCRIPTION( "contentHints.contentType" )
	  ENCODING_SPECIAL( CHOICE ),
	  FL_ATTR_ATTREND, FL_SEQEND /*NONE*/, CRYPT_CONTENT_DATA, CRYPT_CONTENT_LAST, 0, ( void * ) contentTypeInfo },

	/* equivalentLabels:

		OID = 1 2 840 113549 1 9 16 2 9
		SEQUENCE OF {
			SET {
				policyIdentifier OBJECT IDENTIFIER,
				classification	INTEGER (0..5) OPTIONAL,
				privacyMark		PrintableString OPTIONAL,
				categories		SET OF {
					SEQUENCE {
						type  [ 0 ]	OBJECT IDENTIFIER,
						value [ 1 ]	ANY DEFINED BY type
						}
					} OPTIONAL
				}
			}

		Because this is a SET we don't order the fields in the sequence
		given in the above ASN.1 but in the order of encoded size to follow
		the DER SET encoding rules */
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x09" ), CRYPT_CERTINFO_CMS_EQUIVALENTLABEL,
	  DESCRIPTION( "equivalentLabels" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "equivalentLabels.set" )
	  ENCODING( SET ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION,
	  DESCRIPTION( "equivalentLabels.set.securityClassification" )
	  ENCODING( INTEGER ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, CRYPT_CLASSIFICATION_UNMARKED, CRYPT_CLASSIFICATION_LAST, 0, NULL },
	{ NULL, CRYPT_CERTINFO_CMS_EQVLABEL_POLICY,
	  DESCRIPTION( "equivalentLabels.set.securityPolicyIdentifier" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, FL_MULTIVALUED, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK,
	  DESCRIPTION( "equivalentLabels.set.privacyMark" )
	  ENCODING( STRING_PRINTABLE ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE_TEXTSTRING },
	{ NULL, 0,
	  DESCRIPTION( "equivalentLabels.set.securityCategories" )
	  ENCODING( SET ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "equivalentLabels.set.securityCategories.securityCategory" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE,
	  DESCRIPTION( "equivalentLabels.set.securityCategories.securityCategory.type" )
	  ENCODING_TAGGED( OBJECT_IDENTIFIER, 0 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE,
	  DESCRIPTION( "equivalentLabels.set.securityCategories.securityCategory.value" )
	  ENCODING_SPECIAL_TAGGED( BLOB_ANY, 1 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2 /*or _4*/, RANGE_ATTRIBUTEBLOB },
#endif /* USE_CMSATTR_OBSCURE */

#if defined( USE_CMSATTR_OBSCURE ) || defined( USE_CERTREV ) || defined( USE_TSP )
	/* signingCertificate:

		OID = 1 2 840 113549 1 9 16 2 12
		SEQUENCE {
			SEQUENCE OF ESSCertID {
				certHash		OCTET STRING
				},
			SEQUENCE OF {
				SEQUENCE {
					policyIdentifier	OBJECT IDENTIFIER
					}
				} OPTIONAL
			}

	   See the long comment in the certificate attributes for why this 
	   attribute has to be enabled if USE_CERTREV is defined */
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x0C" ), CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE,
	  DESCRIPTION( "signingCertificate" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "signingCertificate.certs" )
	  ENCODING( SEQUENCE ),
	  0, FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID,
	  DESCRIPTION( "signingCertificate.certs.essCertID" )
	  ENCODING_SPECIAL( BLOB_SEQUENCE ),
	  0, FL_MULTIVALUED | FL_SEQEND, RANGE_BLOB },
	{ NULL, 0,
	  DESCRIPTION( "signingCertificate.policies" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "signingCertificate.policies.policyInfo" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES,
	  DESCRIPTION( "signingCertificate.policies.policyInfo.policyIdentifier" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND /*or _3*/, RANGE_OID },
#endif /* USE_CMSATTR_OBSCURE || USE_CERTREV || USE_TSP */

#ifdef USE_CMSATTR_OBSCURE
	/* signingCertificateV2:

		OID = 1 2 840 113549 1 9 16 2 47
		SEQUENCE {
			SEQUENCE OF ESSCertIDv2 {
					hashAlgorithm		AlgorithmIdentifier DEFAULT {SHA-256},
					certHash			OCTET STRING
					},
			SEQUENCE OF {
				SEQUENCE {
					policyIdentifier	OBJECT IDENTIFIER
					}
				} OPTIONAL
			} */
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x2F" ), CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATEV2,
	  DESCRIPTION( "signingCertificate" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "signingCertificate.certs" )
	  ENCODING( SEQUENCE ),
	  0, FL_SETOF, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SIGNINGCERTV2_ESSCERTIDV2,
	  DESCRIPTION( "signingCertificate.certs.essCertID" )
	  ENCODING_SPECIAL( BLOB_SEQUENCE ),
	  0, FL_MULTIVALUED | FL_SEQEND, RANGE_BLOB },
	{ NULL, 0,
	  DESCRIPTION( "signingCertificate.policies" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "signingCertificate.policies.policyInfo" )
	  ENCODING( SEQUENCE ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SIGNINGCERTV2_POLICIES,
	  DESCRIPTION( "signingCertificate.policies.policyInfo.policyIdentifier" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND /*or _3*/, RANGE_OID },

	/* signaturePolicyID:

		OID = 1 2 840 113549 1 9 16 2 15
		SEQUENCE {
			sigPolicyID					OBJECT IDENTIFIER,
			sigPolicyHash				OtherHashAlgAndValue,
			sigPolicyQualifiers			SEQUENCE OF {
										SEQUENCE {
				sigPolicyQualifierID	OBJECT IDENTIFIER,
				sigPolicyQualifier		ANY DEFINED BY sigPolicyQualifierID
					}
				} OPTIONAL
			}

		CPSuri ::= IA5String						-- OID = cps

		UserNotice ::= SEQUENCE {					-- OID = unotice
			noticeRef		SEQUENCE {
				organization	UTF8String,
				noticeNumbers	SEQUENCE OF INTEGER	-- SIZE (1)
				} OPTIONAL,
			explicitText	UTF8String OPTIONAL
			} */
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x0F" ), CRYPT_CERTINFO_CMS_SIGNATUREPOLICYID,
	  DESCRIPTION( "signaturePolicyID" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SIGPOLICYID,
	  DESCRIPTION( "signaturePolicyID.sigPolicyID" )
	  ENCODING( OBJECT_IDENTIFIER ),
	  0, 0, RANGE_OID },
	{ NULL, CRYPT_CERTINFO_CMS_SIGPOLICYHASH,
	  DESCRIPTION( "signaturePolicyID.sigPolicyHash" )
	  ENCODING_SPECIAL( BLOB_SEQUENCE ),
	  0, 0, RANGE_BLOB },
	{ NULL, 0,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL | FL_SETOF, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x05\x01" ), 0,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.cps (1 2 840 113549 1 9 16 5 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SIGPOLICY_CPSURI,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.cPSuri" )
	  ENCODING( STRING_IA5 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2, CHECK_URL },
	{ NULL, 0,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier" )
	  ENCODING( SEQUENCE ),
	  0, FL_IDENTIFIER, RANGE_NONE },
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x05\x02" ), 0,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.unotice (1 2 840 113549 1 9 16 5 2)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, 0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.userNotice" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.organization" )
	  ENCODING( STRING_UTF8 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE( 1, 200 ) },
	{ NULL, CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION,	/* Backwards-compat.handling for VisibleString */
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.organization" )
	  ENCODING( STRING_ISO646 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED, RANGE( 1, 200 ) },
	{ NULL, 0,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.noticeNumbers" )
	  ENCODING( SEQUENCE ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SIGPOLICY_NOTICENUMBERS,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.noticeNumbers" )
	  ENCODING( INTEGER ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND_2, RANGE( 1, 1000 ) },
	{ NULL, CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT,
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.userNotice.explicitText" )
	  ENCODING( STRING_UTF8 ),
	  0, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND, RANGE( 1, 200 ) },
	{ NULL, CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT,	/* Backwards-compat.handling for VisibleString */
	  DESCRIPTION( "signaturePolicyID.sigPolicyQualifiers.sigPolicyQualifier.userNotice.explicitText" )
	  ENCODING( STRING_ISO646 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_MULTIVALUED | FL_SEQEND /* or ... _5 */, RANGE( 1, 200 ) },

	/* signatureTypeIdentifier:

		OID = 1 2 840 113549 1 9 16 9
		SEQUENCE {
			oidInstance1 OPTIONAL,
			oidInstance2 OPTIONAL,
				...
			oidInstanceN OPTIONAL
			} */
	{ MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x09" ), CRYPT_CERTINFO_CMS_SIGTYPEIDENTIFIER,
	  DESCRIPTION( "signatureTypeIdentifier" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  0, RANGE_NONE },
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x09\x01" ), CRYPT_CERTINFO_CMS_SIGTYPEID_ORIGINATORSIG,
	  DESCRIPTION( "signatureTypeIdentifier.originatorSig (1 2 840 113549 1 9 16 9 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x09\x02" ), CRYPT_CERTINFO_CMS_SIGTYPEID_DOMAINSIG,
	  DESCRIPTION( "signatureTypeIdentifier.domainSig (1 2 840 113549 1 9 16 9 2)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x09\x03" ), CRYPT_CERTINFO_CMS_SIGTYPEID_ADDITIONALATTRIBUTES,
	  DESCRIPTION( "signatureTypeIdentifier.additionalAttributesSig (1 2 840 113549 1 9 16 9 3)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x09\x04" ), CRYPT_CERTINFO_CMS_SIGTYPEID_REVIEWSIG,
	  DESCRIPTION( "signatureTypeIdentifier.reviewSig (1 2 840 113549 1 9 16 9 4)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_SEQEND /*NONE*/, RANGE_NONE },
#endif /* USE_CMSATTR_OBSCURE */

#ifdef USE_CERTVAL
	/* randomNonce:

		OID = 1 2 840 113549 1 9 25 3
		OCTET STRING
	
	   This is used by RTCS for its message nonce */
	{ MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x09\x19\x03" ), CRYPT_CERTINFO_CMS_NONCE,
	  DESCRIPTION( "randomNonce" )
	  ENCODING( OCTETSTRING ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE( 4, CRYPT_MAX_HASHSIZE ) },
#endif /* USE_CERTVAL */

#ifdef USE_SCEP
	/* SCEP attributes:

		messageType:
			OID = 2 16 840 1 113733 1 9 2
			PrintableString
		pkiStatus
			OID = 2 16 840 1 113733 1 9 3
			PrintableString
		failInfo
			OID = 2 16 840 1 113733 1 9 4
			PrintableString
		senderNonce
			OID = 2 16 840 1 113733 1 9 5
			OCTET STRING
		recipientNonce
			OID = 2 16 840 1 113733 1 9 6
			OCTET STRING
		transID
			OID = 2 16 840 1 113733 1 9 7
			PrintableString */
	{ MKOID( "\x06\x0A\x60\x86\x48\x01\x86\xF8\x45\x01\x09\x02" ), CRYPT_CERTINFO_SCEP_MESSAGETYPE,
	  DESCRIPTION( "messageType" )
	  ENCODING( STRING_PRINTABLE ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE( 1, 2 ) },
	{ MKOID( "\x06\x0A\x60\x86\x48\x01\x86\xF8\x45\x01\x09\x03" ), CRYPT_CERTINFO_SCEP_PKISTATUS,
	  DESCRIPTION( "pkiStatus" )
	  ENCODING( STRING_PRINTABLE ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE( 1, 1 ) },
	{ MKOID( "\x06\x0A\x60\x86\x48\x01\x86\xF8\x45\x01\x09\x04" ), CRYPT_CERTINFO_SCEP_FAILINFO,
	  DESCRIPTION( "failInfo" )
	  ENCODING( STRING_PRINTABLE ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE( 1, 1 ) },
	{ MKOID( "\x06\x0A\x60\x86\x48\x01\x86\xF8\x45\x01\x09\x05" ), CRYPT_CERTINFO_SCEP_SENDERNONCE,
	  DESCRIPTION( "senderNonce" )
	  ENCODING( OCTETSTRING ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE( 8, CRYPT_MAX_HASHSIZE ) },
	{ MKOID( "\x06\x0A\x60\x86\x48\x01\x86\xF8\x45\x01\x09\x06" ), CRYPT_CERTINFO_SCEP_RECIPIENTNONCE,
	  DESCRIPTION( "recipientNonce" )
	  ENCODING( OCTETSTRING ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE( 8, CRYPT_MAX_HASHSIZE ) },
	{ MKOID( "\x06\x0A\x60\x86\x48\x01\x86\xF8\x45\x01\x09\x07" ), CRYPT_CERTINFO_SCEP_TRANSACTIONID,
	  DESCRIPTION( "transID" )
	  ENCODING( STRING_PRINTABLE ),
	  ATTR_TYPEINFO_CMS | FL_ATTR_ATTREND, 
	  0, RANGE( 2, CRYPT_MAX_TEXTSIZE ) },
#endif /* USE_SCEP */

#ifdef USE_CMSATTR_OBSCURE
	/* spcAgencyInfo:

		OID = 1 3 6 1 4 1 311 2 1 10
		SEQUENCE {
			[ 0 ] {
				??? (= [ 0 ] IA5String )
				}
			}

	   The format for this attribute is unknown but it seems to be an
	   unnecessarily nested URL which is probably an IA5String */
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x0A" ), CRYPT_CERTINFO_CMS_SPCAGENCYINFO,
	  DESCRIPTION( "spcAgencyInfo" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "spcAgencyInfo.vendorInfo" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, 0, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SPCAGENCYURL,
	  DESCRIPTION( "spcAgencyInfo..vendorInfo.url" )
	  ENCODING_TAGGED( STRING_IA5, 0 ),
	  FL_ATTR_ATTREND, FL_SEQEND /*NONE*/, CHECK_HTTP },

	/* spcStatementType:

		OID = 1 3 6 1 4 1 311 2 1 11
		SEQUENCE {
			oidInstance1 OPTIONAL,
			oidInstance2 OPTIONAL,
				...
			oidInstanceN OPTIONAL
			} */
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x0B" ), CRYPT_CERTINFO_CMS_SPCSTATEMENTTYPE,
	  DESCRIPTION( "spcStatementType" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  FL_SETOF, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x15" ), CRYPT_CERTINFO_CMS_SPCSTMT_INDIVIDUALCODESIGNING,
	  DESCRIPTION( "spcStatementType.individualCodeSigning (1 3 6 1 4 1 311 2 1 21)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x16" ), CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING,
	  DESCRIPTION( "spcStatementType.commercialCodeSigning (1 3 6 1 4 1 311 2 1 22)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_SEQEND /*NONE*/, RANGE_NONE },

	/* spcOpusInfo:

		OID = 1 3 6 1 4 1 311 2 1 12
		SEQUENCE {
			[ 0 ] {
				??? (= [ 0 ] BMPString )
				}
			[ 1 ] {
				??? (= [ 0 ] IA5String )
				}
			}

	   The format for this attribute is unknown but it seems to be either an
	   empty sequence or some nested set of tagged fields that eventually
	   end up as text strings */
	{ MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x0C" ), CRYPT_CERTINFO_CMS_SPCOPUSINFO,
	  DESCRIPTION( "spcOpusInfo" )
	  ENCODING( SEQUENCE ),
	  ATTR_TYPEINFO_CMS, 
	  0, RANGE_NONE },
	{ NULL, 0,
	  DESCRIPTION( "spcOpusInfo.programInfo" )
	  ENCODING_TAGGED( SEQUENCE, 0 ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME,
	  DESCRIPTION( "spcOpusInfo.programInfo.name" )
	  ENCODING_TAGGED( STRING_BMP, 0 ),
	  0, FL_OPTIONAL | FL_SEQEND, RANGE( 2, 128 ) },
	{ NULL, 0,
	  DESCRIPTION( "spcOpusInfo.vendorInfo" )
	  ENCODING_TAGGED( SEQUENCE, 1 ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ NULL, CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL,
	  DESCRIPTION( "spcOpusInfo.vendorInfo.url" )
	  ENCODING_TAGGED( STRING_IA5, 0 ),
	  FL_ATTR_ATTREND, FL_OPTIONAL | FL_SEQEND, CHECK_HTTP },
#endif /* USE_CMSATTR_OBSCURE */

	{ NULL, CRYPT_IATTRIBUTE_LAST }, { NULL, CRYPT_IATTRIBUTE_LAST }
	};

/* Subtable for encoding the contentType.  Since the fieldID that we're 
   using for this subtable is a CRYPT_CONTENT_TYPE rather than the 
   CRYPT_ATTRIBUTE_TYPE that's used for the standard encoding tables, we 
   define a macro to convert to the appropriate type */

#define MK_FIELDID( value )		( CRYPT_ATTRIBUTE_TYPE ) ( value )

STATIC_DATA const ATTRIBUTE_INFO FAR_BSS contentTypeInfo[] = {
	{ OID_CMS_DATA, MK_FIELDID( CRYPT_CONTENT_DATA ),
	  DESCRIPTION( "contentType.data (1 2 840 113549 1 7 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  FL_ATTR_ATTRSTART, FL_OPTIONAL, RANGE_NONE },
	{ OID_CMS_SIGNEDDATA, MK_FIELDID( CRYPT_CONTENT_SIGNEDDATA ),
	  DESCRIPTION( "contentType.signedData (1 2 840 113549 1 7 2)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ OID_CMS_ENVELOPEDDATA, MK_FIELDID( CRYPT_CONTENT_ENVELOPEDDATA ),
	  DESCRIPTION( "contentType.envelopedData (1 2 840 113549 1 7 3)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
#ifdef USE_CMSATTR_OBSCURE
	{ MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x07\x04" ), MK_FIELDID( CRYPT_CONTENT_SIGNEDANDENVELOPEDDATA ),
	  DESCRIPTION( "contentType.signedAndEnvelopedData (1 2 840 113549 1 7 4)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
#endif /* USE_CMSATTR_OBSCURE */
	{ OID_CMS_DIGESTEDDATA, MK_FIELDID( CRYPT_CONTENT_DIGESTEDDATA ),
	  DESCRIPTION( "contentType.digestedData (1 2 840 113549 1 7 5)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ OID_CMS_ENCRYPTEDDATA, MK_FIELDID( CRYPT_CONTENT_ENCRYPTEDDATA ),
	  DESCRIPTION( "contentType.encryptedData (1 2 840 113549 1 7 6)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
#ifdef USE_COMPRESSION
	{ OID_CMS_COMPRESSEDDATA, MK_FIELDID( CRYPT_CONTENT_COMPRESSEDDATA ),
	  DESCRIPTION( "contentType.compressedData (1 2 840 113549 1 9 16 1 9)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
#endif /* USE_COMPRESSION */
#ifdef USE_TSP
	{ OID_CMS_TSTOKEN, MK_FIELDID( CRYPT_CONTENT_TSTINFO ),
	  DESCRIPTION( "contentType.tstInfo (1 2 840 113549 1 9 16 1 4)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
#endif /* USE_TSP */
#ifdef USE_CMSATTR_OBSCURE
	{ OID_MS_SPCINDIRECTDATACONTEXT, MK_FIELDID( CRYPT_CONTENT_SPCINDIRECTDATACONTEXT ),
	  DESCRIPTION( "contentType.spcIndirectDataContext (1 3 6 1 4 1 311 2 1 4)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
#endif /* USE_CMSATTR_OBSCURE */
#ifdef USE_CERTVAL
	{ OID_CRYPTLIB_RTCSREQ, MK_FIELDID( CRYPT_CONTENT_RTCSREQUEST ),
	  DESCRIPTION( "contentType.rtcsRequest (1 3 6 1 4 1 3029 4 1 4)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ OID_CRYPTLIB_RTCSRESP, MK_FIELDID( CRYPT_CONTENT_RTCSRESPONSE ),
	  DESCRIPTION( "contentType.rtcsResponse (1 3 6 1 4 1 3029 4 1 5)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
	{ OID_CRYPTLIB_RTCSRESP_EXT, MK_FIELDID( CRYPT_CONTENT_RTCSRESPONSE_EXT ),
	  DESCRIPTION( "contentType.rtcsResponseExt (1 3 6 1 4 1 3029 4 1 6)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  0, FL_OPTIONAL, RANGE_NONE },
#endif /* USE_CERTVAL */
	{ MKOID( "\x06\x06\x67\x81\x08\x01\x01\x01" ), MK_FIELDID( CRYPT_CONTENT_MRTD ),
	  DESCRIPTION( "contentType.mRTD (2 23 136 1 1 1)" )
	  ENCODING_SPECIAL( IDENTIFIER ),
	  FL_ATTR_ATTREND, FL_OPTIONAL, RANGE_NONE },
	{ NULL, CRYPT_IATTRIBUTE_LAST }, { NULL, CRYPT_IATTRIBUTE_LAST }
	};

/* Select the appropriate attribute information table for encoding/type 
   checking */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int getAttributeInfo( IN_ENUM( ATTRIBUTE ) const ATTRIBUTE_TYPE attributeType,
					  OUT const ATTRIBUTE_INFO **attributeInfoPtrPtr,
					  OUT_INT_Z int *noAttributeEntries )
	{
	assert( isReadPtr( attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO * ) ) );
	assert( isWritePtr( noAttributeEntries, sizeof( int ) ) );

	REQUIRES( attributeType == ATTRIBUTE_CERTIFICATE || \
			  attributeType == ATTRIBUTE_CMS );
	
	if( attributeType == ATTRIBUTE_CMS )
		{
		*attributeInfoPtrPtr = cmsAttributeInfo;
		*noAttributeEntries = FAILSAFE_ARRAYSIZE( cmsAttributeInfo, \
												  ATTRIBUTE_INFO );
		}
	else
		{
		*attributeInfoPtrPtr = extensionInfo;
		*noAttributeEntries = FAILSAFE_ARRAYSIZE( extensionInfo, \
												  ATTRIBUTE_INFO );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Init/Shutdown Functions							*
*																			*
****************************************************************************/

/* Check the validity of the encoding information for an individual 
   extension */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkExtension( IN_ARRAY( noAttributeInfoEntries ) \
								const ATTRIBUTE_INFO *attributeInfoPtr,
							   IN_LENGTH_SHORT const int noAttributeInfoEntries,
							   IN_ATTRIBUTE CRYPT_ATTRIBUTE_TYPE firstAttributeID,
							   const BOOLEAN isStandardAttribute, 
							   const BOOLEAN isSubTable )
	{
	CRYPT_ATTRIBUTE_TYPE prevFieldID = CRYPT_ATTRIBUTE_NONE;
	BOOLEAN seenFirstField = FALSE;
	int nestingLevel = 0, iterationCount;

	assert( isReadPtr( attributeInfoPtr, \
					   noAttributeInfoEntries * sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES_B( noAttributeInfoEntries > 0 && \
				noAttributeInfoEntries < MAX_INTLENGTH_SHORT );
	REQUIRES_B( ( !isSubTable && \
				  firstAttributeID >= CRYPT_CERTINFO_FIRST && \
				  firstAttributeID < CRYPT_CERTINFO_LAST ) || \
				( isSubTable && \
				  firstAttributeID >= 0 && firstAttributeID < 50 ) );
				/* Subtables are indexed by small integer values 
				   (CRYPT_CONTENT_TYPE, CRYPT_HOLDINSTRUCTION_TYPE) rather
				   than CRYPT_ATTRIBUTE_TYPE so the values are outside the
				   usual CRYPT_CERTINFO_xxx range */

	/* The first entry must be marked as the attribute start and must have an
	   associated OID */
	if( !( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTRSTART ) )
		return( FALSE );
	if( isStandardAttribute && attributeInfoPtr->oid == NULL )
		return( FALSE );

	/* The overall attribute can't have a lower attribute ID than any 
	   preceding attribute */
	if( attributeInfoPtr->fieldID == FIELDID_FOLLOWS )
		{
		if( attributeInfoPtr[ 1 ].fieldID < firstAttributeID )
			return( FALSE );
		}
	else
		{
		if( attributeInfoPtr->fieldID < firstAttributeID )
			return( FALSE );
		}

	for( iterationCount = 0;
		 !isAttributeTableEnd( attributeInfoPtr ) && \
			iterationCount < noAttributeInfoEntries;
		 attributeInfoPtr++, iterationCount++ )
		{
		const int nestingLevelDelta = \
					decodeNestingLevel( attributeInfoPtr->encodingFlags );

		/* Make sure that various ranges are valid */
		if( nestingLevelDelta < 0 || nestingLevelDelta > 5 )
			return( FALSE );
		if( decodeComplianceLevel( attributeInfoPtr->typeInfoFlags ) < \
									CRYPT_COMPLIANCELEVEL_OBLIVIOUS || \
			decodeComplianceLevel( attributeInfoPtr->typeInfoFlags ) >= \
									CRYPT_COMPLIANCELEVEL_LAST )
			return( FALSE );

		/* Make sure that the fieldIDs (if present for this entry) are 
		   sorted.  We can't require that they be monotone increasing 
		   because handling of some extensions may have been disabled 
		   through the use of configuration options */
		if( attributeInfoPtr->fieldID != CRYPT_ATTRIBUTE_NONE && \
			attributeInfoPtr->fieldID != FIELDID_FOLLOWS && \
			attributeInfoPtr->fieldID <= prevFieldID )
			{
			/* There are a few special-case situations in which the fields
			   don't appear sorted:

				equivalentLabels is for some unknown reason it's defined as 
				a SET rather than a SEQUENCE, so the fields are given in 
				their encoding order rather than their sorting order.  This 
				means in practice that the two fields 
				CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION and
				CRYPT_CERTINFO_CMS_EQVLABEL_POLICY are reversed.

				signaturePolicyID has changed over time to use a different
				encoding form for its text strings, which is handled in a
				transparent manner by having the 
				CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION and 
				CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT entries present 
				twice, first with the current (preferred) format and then 
				again with the legacy one */
			if( !( ( prevFieldID == CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION && \
					 attributeInfoPtr->fieldID == CRYPT_CERTINFO_CMS_EQVLABEL_POLICY ) || \
				   ( prevFieldID == CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION && \
				     attributeInfoPtr->fieldID == CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION ) || \
				   ( prevFieldID == CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT && \
					 attributeInfoPtr->fieldID == CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT ) ) )
				return( FALSE );
			}
		if( attributeInfoPtr->fieldID > CRYPT_ATTRIBUTE_NONE && \
			attributeInfoPtr->fieldID < CRYPT_ATTRIBUTE_LAST )
			{
			/* This may be a field code or unused, so we only update it if
			   it's an attribute value */
			prevFieldID = attributeInfoPtr->fieldID;
			}

		/* Make sure that the field entries are consistent with the field 
		   type */
		if( attributeInfoPtr->fieldType < FIELDTYPE_LAST || \
			attributeInfoPtr->fieldType >= MAX_TAG )
			return( FALSE ); 
		if( attributeInfoPtr->fieldType == FIELDTYPE_CHOICE && \
			attributeInfoPtr->extraData == NULL )
			{
			/* CHOICE must have a CHOICE encoding table */
			return( FALSE );
			}
		if( attributeInfoPtr->fieldType == FIELDTYPE_DN && \
			attributeInfoPtr->extraData == NULL )
			{
			/* DN must have a DN entry checking function */
			return( FALSE );
			}
		if( ( attributeInfoPtr->fieldType == FIELDTYPE_DN || \
			  attributeInfoPtr->fieldType == FIELDTYPE_BLOB_ANY ) && \
			  ( attributeInfoPtr->encodingFlags & FL_OPTIONAL ) && \
			  !( attributeInfoPtr->encodingFlags & FL_EXPLICIT ) )
			{
			/* A FIELDTYPE_BLOB_ANY or FIELDTYPE_DN can't be optional fields 
			   (unless they're explicitly tagged) because with no type 
			   information for them available there's no way to check 
			   whether we've encountered them or not */
			return( FALSE );
			}
		if( attributeInfoPtr->fieldType == FIELDTYPE_SUBTYPED )
			{
			/* Subtyped field must have a subtype encoding table, currently 
			   this can only be the GeneralName table */
			if( attributeInfoPtr->extraData != generalNameInfo )
				return( FALSE );

			/* Make sure that the field really is a GeneralName */
			if( !isGeneralNameSelectionComponent( attributeInfoPtr->fieldID ) )
				return( FALSE );
			}
		if( attributeInfoPtr->fieldType == FIELDID_FOLLOWS )
			{
			const ATTRIBUTE_INFO *nextAttributeInfoPtr = attributeInfoPtr + 1;

			/* FIELDID_FOLLOWS entry must be at the start of the attribute 
			   and must followed by the one that contains the actual field 
			   ID */
			if( !( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTRSTART ) )
				return( FALSE );
			if( nextAttributeInfoPtr->fieldID < CRYPT_CERTINFO_FIRST_EXTENSION || \
				nextAttributeInfoPtr->fieldID > CRYPT_CERTINFO_LAST_EXTENSION )
				return( FALSE ); 
			}

		/* We shouldn't be finding another attribute-start flag in the 
		   middle of the current attribute */
		if( seenFirstField && \
			( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTRSTART ) )
			return( FALSE );

		/* Make sure that identifier fields are set up correctly */
		if( attributeInfoPtr->encodingFlags & FL_IDENTIFIER )
			{
			/* Each identifier field must be followed by the OID that 
			   identifies it (required in ext_chk.c and ext_rd.c) unless
			   it's the final catch-all blob entry */
			if( attributeInfoPtr[ 1 ].fieldType == FIELDTYPE_BLOB_ANY )
				{
				if( attributeInfoPtr[ 1 ].oid != NULL )
					return( FALSE );
				}
			else
				{
				if( attributeInfoPtr[ 1 ].oid == NULL )
					return( FALSE );
				}

			/* If there's optional parameters following the OID then they 
			   have to be part of the current extension (required in 
			   ext_chk.c) */
			if( ( attributeInfoPtr[ 1 ].encodingFlags & FL_NONENCODING ) && \
				( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) )
				return( FALSE );
			}

		/* Make sure that fields with default values are booleans.  This 
		   isn't technically required (ext.c simply users the value stored 
		   in the 'defaultValue' field) but currently all default-value 
		   fields are booleans so we add it as a safety check */
		if( ( attributeInfoPtr->encodingFlags & FL_DEFAULT ) && \
			attributeInfoPtr->fieldType != BER_BOOLEAN )
			return( FALSE );

		/* At the moment only complete-attribute SET/SEQUENCEs can be marked
		   with FL_EMPTYOK, see the comment in certattr.h for details */
		if( ( attributeInfoPtr->encodingFlags & FL_EMPTYOK ) && \
			attributeInfoPtr->fieldID != CRYPT_CERTINFO_BASICCONSTRAINTS )
			return( FALSE );

		/* If it's a CHOICE field then it must be associated with an 
		   encoding subtable for which each entry has the type
		   FIELDTYPE_IDENTIFIER.  The presence of the subtable has already
		   been checked in the general field-entry checks above */
		if( attributeInfoPtr->fieldType == FIELDTYPE_CHOICE )
			{
			const ATTRIBUTE_INFO *subTableInfoPtr;
			int innerIterationCount;

			for( subTableInfoPtr = attributeInfoPtr->extraData, \
					innerIterationCount = 0;
				 !isAttributeTableEnd( subTableInfoPtr ) && \
					innerIterationCount < FAILSAFE_ITERATIONS_MED;
				 subTableInfoPtr++, innerIterationCount++ )
				{
				if( subTableInfoPtr->fieldType != FIELDTYPE_IDENTIFIER )
					return( FALSE );
				}
			ENSURES_B( innerIterationCount < FAILSAFE_ITERATIONS_MED );
			}

		/* If it's a sequence/set, increment the nesting level; if it's an 
		   end-of-constructed-item marker, decrement it by the appropriate 
		   amount */
		if( attributeInfoPtr->fieldType == BER_SEQUENCE || \
			attributeInfoPtr->fieldType == BER_SET )
			nestingLevel++;
		nestingLevel -= nestingLevelDelta;

		/* Make sure that the encoding information is valid */
		if( !( attributeInfoPtr->fieldEncodedType == CRYPT_UNUSED || \
			   ( attributeInfoPtr->fieldEncodedType >= 0 && \
				 attributeInfoPtr->fieldEncodedType < MAX_TAG_VALUE ) ) )
			return( FALSE );

		/* If it's explicitly tagged make sure that it's a constructed tag 
		   in the correct range */
		if( attributeInfoPtr->encodingFlags & FL_EXPLICIT )
			{
			if( ( attributeInfoPtr->fieldEncodedType < 0 ) || \
				( attributeInfoPtr->fieldEncodedType >= MAX_TAG ) )
				return( FALSE );
			}

		/* Check any remaining miscellaneous conditions */
		if( attributeInfoPtr->encodingFlags & FL_SPECIALENCODING )
			{
			/* This flag is only valid for certificate requests, and the 
			   attribute that it applies to can only have a single field */
			if( ( attributeInfoPtr->typeInfoFlags & \
								FL_VALID_MASK ) != FL_VALID_CERTREQ || \
				!( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) )
				return( FALSE );
			}

		/* If we've reached the end of the extension, we're done */
		if( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND )
			break;

		/* Remember that we've seen the first field, from now on we 
		   shouldn't be seeing any start-of-attribute fields */
		seenFirstField = TRUE;
		}
	REQUIRES_B( iterationCount < noAttributeInfoEntries );

	/* The last entry must be marked as the attribute end */
	if( !( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) )
		return( FALSE );

	/* Make sure that the nesting is correct.  Unfortunately this isn't a 
	   very good check because we can exit with a nesting level between zero 
	   and four, see the long comment in cert/certattr.h on the complexities 
	   of deeply nested SEQUENCEs with optional components */
	if( nestingLevel < 0 || nestingLevel > 4 )
		return( FALSE );

	return( TRUE );
	}

/* Check the validity of each extension in an encoding table */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkExtensionTable( IN_ARRAY( noAttributeInfoEntries ) \
										const ATTRIBUTE_INFO *attributeInfoPtr,
									IN_LENGTH_SHORT const int noAttributeInfoEntries,
									const BOOLEAN isStandardAttribute, 
									const BOOLEAN isSubTable )
	{
	CRYPT_ATTRIBUTE_TYPE baseAttributeID;
	int index;

	assert( isReadPtr( attributeInfoPtr, \
					   noAttributeInfoEntries * sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES_B( noAttributeInfoEntries > 0 && \
				noAttributeInfoEntries < MAX_INTLENGTH_SHORT );

	baseAttributeID = ( attributeInfoPtr->fieldID == FIELDID_FOLLOWS ) ? \
					  attributeInfoPtr[ 1 ].fieldID : attributeInfoPtr->fieldID;
	for( index = 0;
		 !isAttributeTableEnd( attributeInfoPtr ) && \
			index < noAttributeInfoEntries;
		 attributeInfoPtr++, index++ )
		{
		int iterationCount;

		if( !checkExtension( attributeInfoPtr, \
								noAttributeInfoEntries - index,
							 baseAttributeID, isStandardAttribute, 
							 isSubTable ) )
			{
			DEBUG_DIAG(( "Extension '%s' definition consistency check failed",
						 ( attributeInfoPtr->description != NULL ) ? \
							attributeInfoPtr->description : "<Unknown>" ));
			return( FALSE );
			}

		/* Remember the base attribute ID, which all subsequent IDs have to
		   be equal to or higher.  The only exception to this is the ID for
		   the ESSCertID attribute, which also applies to certificates and 
		   so appears in a out-of-place location in the certificate 
		   extensions */ 
		if( attributeInfoPtr->fieldID != CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE && \
			attributeInfoPtr->fieldID != CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID )
			{
			baseAttributeID = ( attributeInfoPtr->fieldID == FIELDID_FOLLOWS ) ? \
								attributeInfoPtr[ 1 ].fieldID + 1 : \
								attributeInfoPtr->fieldID + 1;
			}

		/* Skip the remainder of this attribute */
		for( iterationCount = 0;
			 !isAttributeTableEnd( attributeInfoPtr ) && \
				!( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) && \
				iterationCount < noAttributeInfoEntries;
			 attributeInfoPtr++, iterationCount++ );
		ENSURES_B( iterationCount < noAttributeInfoEntries );
		}
	ENSURES_B( index < noAttributeInfoEntries );

	return( TRUE );
	}

/* Check the validity of the encoding tables */

CHECK_RETVAL_BOOL \
BOOLEAN checkExtensionTables( void )
	{
	static const MAP_TABLE checkNestingTbl[] = {
		{ FL_SEQEND, 1 },
		{ FL_SEQEND_2, 2 },
		{ FL_SEQEND_3, 3 },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	static const MAP_TABLE checkComplianceTbl[] = {
		{ FL_LEVEL_OBLIVIOUS, CRYPT_COMPLIANCELEVEL_OBLIVIOUS },
		{ FL_LEVEL_REDUCED, CRYPT_COMPLIANCELEVEL_REDUCED },
		{ FL_LEVEL_STANDARD, CRYPT_COMPLIANCELEVEL_STANDARD },
		{ FL_LEVEL_PKIX_PARTIAL, CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL },
		{ FL_LEVEL_PKIX_FULL, CRYPT_COMPLIANCELEVEL_PKIX_FULL },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	int i;

	/* Sanity checks on various encoded attribute information flags.  These 
	   evaluate to constant expressions at compile-time, which both (at 
	   least somewhat) defeats the point of the check and can also lead to
	   compiler warnings with some compilers due to the expressions being
	   constant, so we evaluate them from a table in order to ensure that 
	   they're actually evaluated and to get rid of compiler warnings */
	for( i = 0; checkNestingTbl[ i ].source != CRYPT_ERROR && \
				i < FAILSAFE_ARRAYSIZE( checkNestingTbl, \
										sizeof( MAP_TABLE ) ); i++ )
		{
		ENSURES_B( decodeNestingLevel( checkNestingTbl[ i ].source ) == \
				   checkNestingTbl[ i ].destination );
		}
	ENSURES_B( i < FAILSAFE_ARRAYSIZE( checkNestingTbl, \
									   sizeof( MAP_TABLE ) ) );
	for( i = 0; checkComplianceTbl[ i ].source != CRYPT_ERROR && \
				i < FAILSAFE_ARRAYSIZE( checkComplianceTbl, \
										sizeof( MAP_TABLE ) ); i++ )
		{
		ENSURES_B( decodeComplianceLevel( checkComplianceTbl[ i ].source ) == \
				   checkComplianceTbl[ i ].destination );
		}
	ENSURES_B( i < FAILSAFE_ARRAYSIZE( checkComplianceTbl, \
									   sizeof( MAP_TABLE ) ) );

	/* Check each encoding table */
	if( !checkExtensionTable( extensionInfo, 
							  FAILSAFE_ARRAYSIZE( extensionInfo, \
												  ATTRIBUTE_INFO ), 
							  TRUE, FALSE ) )
		{
		DEBUG_DIAG(( "Certificate extension definition consistency check failed" ));
		retIntError_Boolean();
		}
	if( !checkExtensionTable( cmsAttributeInfo,
							  FAILSAFE_ARRAYSIZE( cmsAttributeInfo, \
												  ATTRIBUTE_INFO ), 
							  TRUE, FALSE ) )
		{
		DEBUG_DIAG(( "CMS attribute definition consistency check failed" ));
		retIntError_Boolean();
		}
	if( !checkExtensionTable( generalNameInfo,
							  FAILSAFE_ARRAYSIZE( generalNameInfo, \
												  ATTRIBUTE_INFO ), 
							  FALSE, FALSE ) )
		{
		DEBUG_DIAG(( "GeneralName definition consistency check failed" ));
		retIntError_Boolean();
		}
#if ( defined( USE_CERTREV ) || defined( USE_CERTREQ ) ) && \
	defined( USE_CERTLEVEL_PKIX_FULL )
	if( !checkExtensionTable( holdInstructionInfo,
							  FAILSAFE_ARRAYSIZE( holdInstructionInfo, \
												  ATTRIBUTE_INFO ), 
							  TRUE, TRUE ) )
		{
		DEBUG_DIAG(( "HoldInstructionInfo definition consistency check failed" ));
		retIntError_Boolean();
		}
#endif /* ( USE_CERTREV || USE_CERTREQ ) && USE_CERTLEVEL_PKIX_FULL */
	if( !checkExtensionTable( contentTypeInfo,
							  FAILSAFE_ARRAYSIZE( contentTypeInfo, \
												  ATTRIBUTE_INFO ), 
							  TRUE, TRUE ) )
		{
		DEBUG_DIAG(( "ContentTypeInfo definition consistency check failed" ));
		retIntError_Boolean();
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Extended Validity Checking Functions				*
*																			*
****************************************************************************/

/* Determine whether a variety of URIs are valid and return a 
   CRYPT_ERRTYPE_TYPE describing the type of error if there's a problem.  
   The PKIX RFC refers to a pile of complex parsing rules for various URI 
   forms, since cryptlib is neither a resolver nor an MTA nor a web browser 
   it leaves it up to the calling application to decide whether a particular 
   form is acceptable to it or not.  We do however perform a few basic 
   checks to weed out obviously-incorrect forms here.
   
   In theory we could use sNetParseUrl() for this but the code won't be
   included if cryptlib is built without networking support, and in any case 
   we also need to perform processing for URLs that aren't network URLs */

typedef enum {
	URL_NONE,				/* No URL */
	URL_RFC822,				/* Email address */
	URL_DNS,				/* FQDN */
	URL_HTTP,				/* HTTP URL */
	URL_ANY,				/* Generic URL */
	URL_LAST				/* Last possible URL type */
	} URL_CHECK_TYPE;

CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkURLString( IN_BUFFER( urlLength ) const char *url, 
						   IN_LENGTH_DNS const int urlLength,
						   IN_ENUM( URL ) const URL_CHECK_TYPE urlType )
	{
	const char *schema = NULL;
	int schemaLength = 0, length = urlLength, offset, i;

	assert( isReadPtr( url, urlLength ) );

	REQUIRES_EXT( urlLength >= MIN_RFC822_SIZE && urlLength < MAX_URL_SIZE,
				  CRYPT_ERRTYPE_ATTR_VALUE );
				  /* MIN_RFC822_SIZE is the shortest value allow, 
				     MAX_URL_SIZE is the largest */
	REQUIRES_EXT( urlType > URL_NONE && urlType < URL_LAST,
				  CRYPT_ERRTYPE_ATTR_VALUE );

	/* Make a first pass over the URL checking that it follows the RFC 1738 
	   rules for valid characters.  Because of the use of wildcards in 
	   certificates we can't check for '*' at this point but have to make a
	   second pass after we've performed URL-specific processing */
	for( i = 0; i < urlLength; i++ )
		{
		const int ch = byteToInt( url[ i ] );

		if( !isValidTextChar( ch ) || \
			ch == ' ' || ch == '<' || ch == '>' || ch == '"' || \
			ch == '{' || ch == '}' || ch == '|' || ch == '\\' || \
			ch == '^' || ch == '[' || ch == ']' || ch == '`' )
			return( CRYPT_ERRTYPE_ATTR_VALUE );
		}

	/* Check for a schema separator.  This get a bit complicated because
	   some URLs use "://" (HTTP, FTP, LDAP) and others just use ":" (SMTP, 
	   SIP), so we have to check for both.  We can't check for a possibly-
	   malformed ":/" because this could be something like 
	   "file:/dir/filename", which is valid.  The pattern that we check for
	   is '(2...8 chars) ":" ["//" ] (3...n chars)' */
	if( ( offset = strFindStr( url, urlLength, "://", 3 ) ) >= 0 )
		{
		/* Extract the URI schema */
		if( offset < 2 || offset > 8 || offset >= urlLength - 3 )
			return( CRYPT_ERRTYPE_ATTR_SIZE );
		offset += 3;	/* Adjust for "://" */
		}
	else
		{
		if( ( offset = strFindCh( url, urlLength, ':' ) ) >= 0 )
			{
			/* Extract the URI schema */
			if( offset < 2 || offset > 8 || offset >= urlLength - 3 )
				return( CRYPT_ERRTYPE_ATTR_SIZE );
			offset++;	/* Adjust for ":" */
			}
		}
	if( offset > 0 )
		{
		schema = url;
		schemaLength = offset;
		url += offset;
		length = urlLength - offset;
		}
	ENSURES_EXT( rangeCheckZ( schemaLength, length, urlLength ),
				 CRYPT_ERRTYPE_ATTR_VALUE );

	/* Make sure that the start of the URL looks valid.  Note that this 
	   simply checks for obvious mistakes (e.g. setting an IP address for a 
	   DNS name), not for every possible way of disguising one kind of URL 
	   as another.

	   The lengths have already been checked by the kernel but we check them 
	   again here to be sure */
	switch( urlType )
		{
		case URL_DNS:
			if( urlLength < MIN_DNS_SIZE || urlLength > MAX_DNS_SIZE )
				return( CRYPT_ERRTYPE_ATTR_SIZE );
			if( schema != NULL )
				{
				/* It's a URL, not a DNS name */
				return( CRYPT_ERRTYPE_ATTR_VALUE );
				}
			if( ( isDigit( url[ 0 ] && isDigit( url[ 1 ] ) ) ) || \
				( url[ 0 ] == '[' && \
				  ( url[ 1 ] == ':' || isDigit( url[ 1 ] ) ) ) )
				{
				/* It's an IPv4 or IPv6 address, not a DNS name */
				return( CRYPT_ERRTYPE_ATTR_VALUE );
				}
			if( !strCompare( url, "*.", 2 ) )
				{
				url += 2;	/* Skip wildcard */
				length -= 2;
				}
			break;

		case URL_RFC822:
			if( urlLength < MIN_RFC822_SIZE || urlLength > MAX_RFC822_SIZE )
				return( CRYPT_ERRTYPE_ATTR_SIZE );
			if( schema != NULL )
				{
				/* Catch erroneous use of URL */
				return( CRYPT_ERRTYPE_ATTR_VALUE );
				}
			if( !strCompare( url, "*@", 2 ) )
				{
				url += 2;	/* Skip wildcard */
				length -= 2;
				}
			break;

		case URL_HTTP:
			if( urlLength < MIN_URL_SIZE || urlLength > MAX_URL_SIZE )
				return( CRYPT_ERRTYPE_ATTR_SIZE );
			if( schema == NULL || \
				( strCompare( schema, "http://", 7 ) && \
				  strCompare( schema, "https://", 8 ) ) )
				return( CRYPT_ERRTYPE_ATTR_VALUE );
			if( !strCompare( url, "*.", 2 ) )
				{
				url += 2;	/* Skip wildcard */
				length -= 2;
				}
			break;

		case URL_ANY:
			if( schema == NULL || length < 3 || length > MAX_URL_SIZE )
				return( CRYPT_ERRTYPE_ATTR_VALUE );
			break;

		default:
			retIntError();
		}

	/* Make a second pass over the URL checking for any remaining invalid 
	   characters.  Since we've stripped any permitted wildcards earlier on,
	   the wildcard is now an invalid character */
	for( i = 0; i < length; i++ )
		{
		const int ch = byteToInt( url[ i ] );

		if( ch == '*' )
			return( CRYPT_ERRTYPE_ATTR_VALUE );
		}

	return( CRYPT_ERRTYPE_NONE );
	}

CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkRFC822( const ATTRIBUTE_LIST *attributeListPtr )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	return( checkURLString( attributeListPtr->value,
							attributeListPtr->valueLength, URL_RFC822 ) );
	}

CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDNS( const ATTRIBUTE_LIST *attributeListPtr )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	return( checkURLString( attributeListPtr->value,
							attributeListPtr->valueLength, URL_DNS ) );
	}

CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkURL( const ATTRIBUTE_LIST *attributeListPtr )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	return( checkURLString( attributeListPtr->value,
							attributeListPtr->valueLength, URL_ANY ) );
	}

#ifdef USE_CERT_OBSOLETE

CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkHTTP( const ATTRIBUTE_LIST *attributeListPtr )
	{
	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	return( checkURLString( attributeListPtr->value,
							attributeListPtr->valueLength, URL_HTTP ) );
	}
#endif /* USE_CERT_OBSOLETE */

/* Determine whether a DN (either a complete DN or a DN subtree) is valid.
   Most attribute fields require a full DN but some fields (which act as
   filters) are allowed a partial DN */

CHECK_RETVAL_ENUM( CRYPT_ERRTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDirectoryName( const ATTRIBUTE_LIST *attributeListPtr )
	{
	CRYPT_ATTRIBUTE_TYPE dummy;
	CRYPT_ERRTYPE_TYPE errorType;
	int status;

	assert( isReadPtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );

	if( attributeListPtr->fieldID == CRYPT_CERTINFO_EXCLUDEDSUBTREES || \
		attributeListPtr->fieldID == CRYPT_CERTINFO_PERMITTEDSUBTREES )
		{
		status = checkDN( attributeListPtr->value, CHECKDN_FLAG_COUNTRY,
						  &dummy, &errorType );
		}
	else
		{
		status = checkDN( attributeListPtr->value, 
						  CHECKDN_FLAG_COUNTRY | CHECKDN_FLAG_COMMONNAME,
						  &dummy, &errorType );
		}
	if( cryptStatusError( status ) )
		return( errorType );

	return( CRYPT_ERRTYPE_NONE );
	}

/****************************************************************************
*																			*
*							Miscellaneous Functions							*
*																			*
****************************************************************************/

/* Since many of the attributes can be disabled to save space and reduce 
   complexity, we may need to check that an attribute that we want to use is
   actually available, for example if we're about to create it as part of an
   internal operation for which we don't want to present an unexpected error
   status to the caller.  The following function checks whether an attribute
   is enabled for use */

CHECK_RETVAL_BOOL \
BOOLEAN checkAttributeAvailable( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID )
	{
	REQUIRES_B( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				fieldID <= CRYPT_CERTINFO_LAST );

	if( fieldID <= CRYPT_CERTINFO_LAST_EXTENSION )
		{
		return( fieldIDToAttribute( ATTRIBUTE_CERTIFICATE, fieldID, 
									CRYPT_ATTRIBUTE_NONE, NULL ) != NULL ? \
				TRUE : FALSE );
		}
	return( fieldIDToAttribute( ATTRIBUTE_CMS, fieldID, 
								CRYPT_ATTRIBUTE_NONE, NULL ) != NULL ? \
			TRUE : FALSE );
	}

/* Get the encoded tag value for a field */

CHECK_RETVAL_RANGE( MAKE_CTAG_PRIMITIVE( 0 ), MAX_TAG ) STDC_NONNULL_ARG( ( 1 ) ) \
int getFieldEncodedTag( const ATTRIBUTE_INFO *attributeInfoPtr )
	{
	int tag;

	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( attributeInfoPtr->fieldEncodedType == CRYPT_UNUSED || \
			  ( attributeInfoPtr->fieldEncodedType >= 0 && \
				attributeInfoPtr->fieldEncodedType < MAX_TAG_VALUE ) );

	/* If it's a non-tagged field, we're done */
	if( attributeInfoPtr->fieldEncodedType == CRYPT_UNUSED )
		return( OK_SPECIAL );

	/* It's a tagged field, the actual tag is stored as the encoded-type 
	   value.  If it's explicitly tagged or an implictly tagged SET/SEQUENCE 
	   then it's constructed, otherwise it's primitive */
	if( ( attributeInfoPtr->fieldType == BER_SEQUENCE ||
		  attributeInfoPtr->fieldType == BER_SET ||
		  attributeInfoPtr->fieldType == FIELDTYPE_DN ||
		  ( attributeInfoPtr->encodingFlags & FL_EXPLICIT ) ) )
		tag = MAKE_CTAG( attributeInfoPtr->fieldEncodedType );
	else
		tag = MAKE_CTAG_PRIMITIVE( attributeInfoPtr->fieldEncodedType );

	ENSURES( ( tag >= MAKE_CTAG_PRIMITIVE( 0 ) ) && \
			 ( tag <= MAX_TAG ) );

	return( tag );
	}
#endif /* USE_CERTIFICATES */
