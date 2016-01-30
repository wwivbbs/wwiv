/****************************************************************************
*																			*
*				ASN.1 Supplementary Constants and Structures				*
*						Copyright Peter Gutmann 1992-2011					*
*																			*
****************************************************************************/

#ifndef _ASN1OID_DEFINED

#define _ASN1OID_DEFINED

/* The cryptlib (strictly speaking DDS) OID arc is as follows:

	1 3 6 1 4 1 3029 = dds
					 1 = algorithm
					   1 = symmetric encryption
						 1 = blowfishECB
						 2 = blowfishCBC
						 3 = blowfishCFB
						 4 = blowfishOFB
					   2 = public-key encryption
						 1 = elgamal
					   3 = hash
					   4 = MAC
					 2 = mechanism
					 3 = attribute
					   1 = PKIX fixes
						 1 = cryptlibPresenceCheck
						 2 = pkiBoot
						 (3 unused)
						 4 = cRLExtReason
						 5 = keyFeatures
					 4 = content-type
					   1 = cryptlib
						 1 = cryptlibConfigData
						 2 = cryptlibUserIndex
						 3 = cryptlibUserInfo
						 4 = cryptlibRtcsRequest
						 5 = cryptlibRtcsResponse
						 6 = cryptlibRtcsResponseExt
					 x36\xDD\x24\x36 = TSA policy ('snooze policy, "Anything 
									   that arrives, we sign").
					 x58 x59 x5A x5A x59 = XYZZY cert policy */

/* Attribute OIDs */

#define OID_CRYPTLIB_PRESENCECHECK	MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x03\x01\x01" )
#define OID_ESS_CERTID			MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x0C" )
#define OID_TSP_TSTOKEN			MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x0E" )
#define OID_PKCS9_FRIENDLYNAME	MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x14" )
#define OID_PKCS9_LOCALKEYID	MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x15" )
#define OID_PKCS9_X509CERTIFICATE MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x09\x16\x01" )

/* The PKCS #9 OID for cert extensions in a certification request, from the
   CMMF draft.  Naturally MS had to define their own incompatible OID for
   this, so we check for this as well */

#define OID_PKCS9_EXTREQ		MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x0E" )
#define OID_MS_EXTREQ			MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x0E" )

/* Content-type OIDs */

#define OID_CMS_DATA			MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x07\x01" )
#define OID_CMS_SIGNEDDATA		MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x07\x02" )
#define OID_CMS_ENVELOPEDDATA	MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x07\x03" )
#define OID_CMS_DIGESTEDDATA	MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x07\x05" )
#define OID_CMS_ENCRYPTEDDATA	MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x07\x06" )
#define OID_CMS_AUTHDATA		MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x01\x02" )
#define OID_CMS_TSTOKEN			MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x01\x04" )
#define OID_CMS_COMPRESSEDDATA	MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x01\x09" )
#define OID_CMS_AUTHENVDATA		MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x01\x17" )
#define OID_CRYPTLIB_CONTENTTYPE MKOID( "\x06\x09\x2B\x06\x01\x04\x01\x97\x55\x04\x01" )
#define OID_CRYPTLIB_CONFIGDATA	MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x04\x01\x01" )
#define OID_CRYPTLIB_USERINDEX	MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x04\x01\x02" )
#define OID_CRYPTLIB_USERINFO	MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x04\x01\x03" )
#define OID_CRYPTLIB_RTCSREQ	MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x04\x01\x04" )
#define OID_CRYPTLIB_RTCSRESP	MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x04\x01\x05" )
#define OID_CRYPTLIB_RTCSRESP_EXT	MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x04\x01\x06" )
#define OID_MS_SPCINDIRECTDATACONTEXT MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x04" )
#define OID_NS_CERTSEQ			MKOID( "\x06\x09\x60\x86\x48\x01\x86\xF8\x42\x02\x05" )
#define OID_OCSP_RESPONSE_OCSP MKOID( "\x06\x09\x2B\x06\x01\x05\x05\x07\x30\x01\x01" )
#define OID_PKIBOOT				MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x03\x01\x02" )
#define OID_PKCS12_SHROUDEDKEYBAG MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x0C\x0A\x01\x02" )
#define OID_PKCS12_CERTBAG		MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x0C\x0A\x01\x03" )
#define OID_PKCS15_CONTENTTYPE	MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x0F\x03\x01" )

/* Misc OIDs */

#define OID_ANYPOLICY			MKOID( "\x06\x04\x55\x1D\x20\x00" )
#define OID_TSP_POLICY			MKOID( "\x06\x0B\x2B\x06\x01\x04\x01\x97\x55\x36\xDD\x24\x36" )
#define OID_CRYPTLIB_XYZZYCERT	MKOID( "\x06\x0C\x2B\x06\x01\x04\x01\x97\x55\x58\x59\x5A\x5A\x59" )
#define OID_PKCS12_PBEWITHSHAAND3KEYTRIPLEDESCBC MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x0C\x01\x03" )
#define OID_PKCS12_PBEWITHSHAAND2KEYTRIPLEDESCBC MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x0C\x01\x04" )
#define OID_PKCS12_PBEWITHSHAAND40BITRC2CBC MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x0C\x01\x06" )
#define OID_RPKI_POLICY			MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x07\x0E\x02" )
#define OID_ZLIB				MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x03\x08" )

/* Additional information required when reading a CMS header.  This is
   pointed to by the extraInfo member of the ASN.1 OID_INFO structure and
   contains CMS version number information */

typedef struct {
	const int minVersion;	/* Minimum version number for content type */
	const int maxVersion;	/* Maximum version number for content type */
	} CMS_CONTENT_INFO;

/* AlgorithmIdentifier routines.  The reason for the apparently redundant 
   CHECK_RETVAL specifiers on some of the write functions is because they 
   won't necessarily set the stream error state if they encounter an error
   obtaining algorithm parameters or during some other non-stream-related
   operation.

   The difference between read/writeAlgoID() and read/writeAlgoIDparam() is 
   that the latter take an additional length parameter for when the 
   AlgorithmIdentifier contains additional parameters beyond the OID */

typedef enum {
	ALGOID_CLASS_NONE,		/* No AlgoID class */
	ALGOID_CLASS_CRYPT,		/* Encryption algorithms */
	ALGOID_CLASS_HASH,		/* Hash/MAC algorithm */
	ALGOID_CLASS_AUTHENC,	/* Authenticated-encryption algorithm */
	ALGOID_CLASS_PKC,		/* Generic PKC algorithm */
	ALGOID_CLASS_PKCSIG,	/* PKC signature algorithm (+ hash algorithm) */
	ALGOID_CLASS_LAST		/* Last possible AlgoID class */
	} ALGOID_CLASS_TYPE;

CHECK_RETVAL_BOOL \
BOOLEAN checkAlgoID( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
					 IN_MODE const CRYPT_MODE_TYPE cryptMode );
CHECK_RETVAL_LENGTH \
int sizeofAlgoID( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo );
CHECK_RETVAL_LENGTH \
int sizeofAlgoIDex( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
					IN_RANGE( 0, 999 ) const int parameter, 
					IN_LENGTH_SHORT_Z const int extraLength );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoID( INOUT STREAM *stream, 
				 IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoIDex( INOUT STREAM *stream, 
				   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
				   IN_RANGE( 0, 999 ) const int parameter, 
				   IN_LENGTH_SHORT_Z const int extraLength );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoIDparam( INOUT STREAM *stream, 
					  IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
					  IN_LENGTH_SHORT_Z const int extraLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2) ) \
int readAlgoID( INOUT STREAM *stream, 
				OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo,
				IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readAlgoIDex( INOUT STREAM *stream, 
				  OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo,
				  OUT_OPT_ALGO_Z CRYPT_ALGO_TYPE *altCryptAlgo,
				  OUT_INT_Z int *parameter,
				  IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readAlgoIDparam( INOUT STREAM *stream, 
					 OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo, 
					 OUT_LENGTH_SHORT_Z int *extraLength,
					 IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type );

/* Alternative versions that read/write various algorithm ID types (algo and
   mode only or full details depending on the option parameter) from contexts */

CHECK_RETVAL_LENGTH \
int sizeofContextAlgoID( IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						 IN_RANGE( 0, 999 ) const int parameter );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readContextAlgoID( INOUT STREAM *stream, 
					   OUT_OPT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
					   OUT_OPT QUERY_INFO *queryInfo, 
					   IN_TAG const int tag,
					   IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeContextAlgoID( INOUT STREAM *stream, 
						IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						IN_ALGO_OPT const int associatedAlgo );
CHECK_RETVAL_LENGTH \
int sizeofCryptContextAlgoID( IN_HANDLE const CRYPT_CONTEXT iCryptContext );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeCryptContextAlgoID( INOUT STREAM *stream,
							 IN_HANDLE const CRYPT_CONTEXT iCryptContext );

/* Another alternative that reads/writes a non-crypto algorithm identifier,
   used for things like content types.  This just wraps the given OID up
   in the AlgorithmIdentifier and writes it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readGenericAlgoID( INOUT STREAM *stream, 
					   IN_BUFFER( oidLength ) \
					   const BYTE *oid, 
					   IN_LENGTH_OID const int oidLength );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeGenericAlgoID( INOUT STREAM *stream, 
						IN_BUFFER( oidLength ) \
						const BYTE *oid, 
						IN_LENGTH_OID const int oidLength );

/* Read/write a message digest */

CHECK_RETVAL_LENGTH \
int sizeofMessageDigest( IN_ALGO const CRYPT_ALGO_TYPE hashAlgo, 
						 IN_LENGTH_HASH const int hashSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int readMessageDigest( INOUT STREAM *stream, 
					   OUT_ALGO_Z CRYPT_ALGO_TYPE *hashAlgo,
					   OUT_BUFFER( hashMaxLen, *hashSize ) void *hash, 
					   IN_LENGTH_HASH const int hashMaxLen, 
					   OUT_LENGTH_SHORT_Z int *hashSize );
RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int writeMessageDigest( INOUT STREAM *stream, 
						IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
						IN_BUFFER( hashSize ) \
						const void *hash, IN_LENGTH_HASH const int hashSize );

/* Read/write CMS headers.  The readCMSheader() flags are:

	READCMS_FLAG_AUTHENC: The content uses authenticated encryption, which
			has a different set of permitted content-encryption algorithms 
			than standard encryption.

	READCMS_FLAG_DEFINITELENGTH: Try and obtain a definite length from 
			somewhere in the CMS header rather than returning CRYPT_UNUSED
			for the length, return an error if there's no definite length
			available.  Note that this changes processing in the calling
			code because it can no longer use the length to determine 
			whether it should perform EOC checks if there's an indefinite
			length somwwhere in the header.

	READCMS_FLAG_DEFINITELENGTH_OPT: As READCMS_FLAG_DEFINITELENGTH but 
			return a length of CRYPT_UNUSED if there's no definite length
			information available.

	READCMS_FLAG_INNERHEADER: This is an inner header, the content wrapper
			can be an OCTET STRING as well as the more usual SEQUENCE.

	READCMS_FLAG_WRAPPERONLY: Only read the outer SEQUENCE, OID, [0] wrapper
			without reading the final layer of inner encapsulation, used
			when one CMS content type is redundantly nested directly inside 
			another (Microsoft did this for PKCS #12) */

#define READCMS_FLAG_NONE			0x00	/* No CMS read flag */
#define READCMS_FLAG_INNERHEADER	0x01	/* Inner CMS header */
#define READCMS_FLAG_AUTHENC		0x02	/* Content uses auth.enc */
#define READCMS_FLAG_WRAPPERONLY	0x04	/* Only read wrapper */
#define READCMS_FLAG_DEFINITELENGTH	0x08	/* Try and get definte len */
#define READCMS_FLAG_DEFINITELENGTH_OPT 0x10/* Opt.try and get def.len */
#define READCMS_FLAG_MAX			0x1F	/* Maximum possible flag value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readCMSheader( INOUT STREAM *stream, 
				   IN_ARRAY( noOidInfoEntries ) \
				   const OID_INFO *oidInfo, 
				   IN_RANGE( 1, 50 ) const int noOidInfoEntries, 
				   OUT_OPT_LENGTH_INDEF long *dataSize, 
				   IN_FLAGS_Z( READCMS ) const int flags );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCMSheader( INOUT STREAM *stream, 
					IN_BUFFER( contentOIDlength ) \
					const BYTE *contentOID, 
					IN_LENGTH_OID const int contentOIDlength,
					IN_LENGTH_INDEF const long dataSize, 
					const BOOLEAN isInnerHeader );
CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofCMSencrHeader( IN_BUFFER( contentOIDlength ) \
						 const BYTE *contentOID, 
						 IN_LENGTH_OID const int contentOIDlength,
						 IN_LENGTH_INDEF const long dataSize, 
						 IN_HANDLE const CRYPT_CONTEXT iCryptContext );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readCMSencrHeader( INOUT STREAM *stream, 
					   IN_ARRAY( noOidInfoEntries ) \
					   const OID_INFO *oidInfo,
					   IN_RANGE( 1, 50 ) const int noOidInfoEntries, 
					   OUT_OPT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext, 
					   OUT_OPT QUERY_INFO *queryInfo,
					   IN_FLAGS_Z( READCMS ) const int flags );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCMSencrHeader( INOUT STREAM *stream, 
						IN_BUFFER( contentOIDlength ) \
						const BYTE *contentOID, 
						IN_LENGTH_OID const int contentOIDlength,
						IN_LENGTH_INDEF const long dataSize,
						IN_HANDLE const CRYPT_CONTEXT iCryptContext );

#endif /* _ASN1OID_DEFINED */
