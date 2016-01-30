/****************************************************************************
*																			*
*						PKCS #15 Definitions Header File					*
*						Copyright Peter Gutmann 1996-2014					*
*																			*
****************************************************************************/

#ifndef _PKCS15_DEFINED

#define _PKCS15_DEFINED

/* The format used to protect the private key components is a standard
   cryptlib envelope, however for various reasons the required enveloping
   functionality (which in practice is just minimal code to process a
   PasswordRecipientInfo at the start of the data) is duplicated here
   because:

	1. It's somewhat inelegant to use the heavyweight enveloping routines to
	   wrap up 100 bytes of data.

	2. The enveloping code is enormous and complex, especially when extra
	   sections like zlib and PGP and S/MIME support are factored in.  This
	   makes it difficult to compile a stripped-down version of cryptlib,
	   since private key storage will require all of the enveloping code to 
	   be included.

	3. Since the enveloping code is general-purpose, it doesn't allow very
	   precise control over the data being processed.  Specifically, it's
	   necessary to write the private key components to a buffer in plaintext
	   form, which isn't permitted by the cryptlib kernel.

   For these reasons the PKCS #15 modules include the code to process minimal
   (password-encrypted data) envelopes */

/* Alongside per-key protection, PKCS #15 also allows for an entire keyset 
   to be integrity-protected, but the key management makes this more or less 
   impossible to deploy using a general-purpose API.  If only a single key 
   is present in the file then it's partly feasible, except that once a 
   keyset is cast in stone no further updates can be made without requiring 
   passwords at odd times.  For example a certificate update would require a 
   (private-key) password in order to recalculate the integrity-check value 
   once the certificate has been added.  In addition the password would need 
   to be supplied at file open, when the whole file is parsed, and then 
   again when the private key is read, which leads to a rather puzzling API 
   for anyone not in the know.

   In the presence of multiple keys this becomes even more awkward since the 
   per-key password isn't necessarily the same as the per-file password, so 
   you'd need to provide a password to open the file and then possibly 
   another one to get the key, unless it was the same as the per-file 
   password.  If there was one per-file password and multiple per-key 
   passwords then you would't know which key or keys used the per-file 
   password unless you opportunistically tried on each per-key access, and 
   the chances of that successfully making it up into any UI that people 
   create seem slim.  In addition updates are even more problematic than for 
   the one file, one key case because whoever last added a key would end up 
   with their per-key password authenticating the whole file */

/****************************************************************************
*																			*
*								PKCS #15 Constants							*
*																			*
****************************************************************************/

/* Each PKCS #15 keyset can contain information for multiple personalities 
   (although it's extremely unlikely to contain more than one or two), we 
   allow a maximum of MAX_PKCS15_OBJECTS per keyset in order to discourage 
   them from being used as general-purpose public-key keysets, which they're 
   not supposed to be.  A setting of 16 objects consumes ~2K of memory 
   (16 x ~128) and seems like a sensible upper bound so we choose that as 
   the limit */

#ifdef CONFIG_CONSERVE_MEMORY
  #define MAX_PKCS15_OBJECTS	8
#else
  #define MAX_PKCS15_OBJECTS	16
#endif /* CONFIG_CONSERVE_MEMORY */

/* Usually a PKCS #15 personality consists of a collection of related PKCS
   #15 objects (typically a public and private key and a certificate), but 
   sometimes we have personalities that consist only of a certificate and 
   little other information (for example a trusted CA root certificate, 
   which contains no user-supplied information such as a label).  In 
   addition there's an unrecognised data-type for which we interpret the
   metadata but can't perform any other operations with */

typedef enum {
	PKCS15_SUBTYPE_NONE,			/* Non-personality */
	PKCS15_SUBTYPE_NORMAL,			/* Standard personality, keys+optional cert */
	PKCS15_SUBTYPE_CERT,			/* Standalone certificate */
	PKCS15_SUBTYPE_SECRETKEY,		/* Secret key */
	PKCS15_SUBTYPE_DATA,			/* Pre-encoded cryptlib-specific data */
	PKCS15_SUBTYPE_UNRECOGNISED,	/* Unrecognised data type */
	PKCS15_SUBTYPE_LAST
	} PKCS15_SUBTYPE;

/* The types of object that we can find in a PKCS #15 file */

typedef enum { PKCS15_OBJECT_NONE, PKCS15_OBJECT_PUBKEY, 
			   PKCS15_OBJECT_PRIVKEY, PKCS15_OBJECT_CERT, 
			   PKCS15_OBJECT_SECRETKEY, PKCS15_OBJECT_DATA, 
			   PKCS15_OBJECT_UNRECOGNISED, PKCS15_OBJECT_LAST 
			   } PKCS15_OBJECT_TYPE;

/* The types of key identifiers that we can find attached to an object */

enum { PKCS15_KEYID_NONE, PKCS15_KEYID_ISSUERANDSERIALNUMBER,
	   PKCS15_KEYID_SUBJECTKEYIDENTIFIER, PKCS15_KEYID_ISSUERANDSERIALNUMBERHASH,
	   PKCS15_KEYID_SUBJECTKEYHASH, PKCS15_KEYID_ISSUERKEYHASH,
	   PKCS15_KEYID_ISSUERNAMEHASH, PKCS15_KEYID_SUBJECTNAMEHASH,
	   PKCS15_KEYID_PGP2, PKCS15_KEYID_OPENPGP, PKCS15_KEYID_LAST };

/* PKCS #15 key usage flags, a complex mixture of PKCS #11 and some bits of
   X.509 */

#define PKCS15_USAGE_ENCRYPT		0x0001
#define PKCS15_USAGE_DECRYPT		0x0002
#define PKCS15_USAGE_SIGN			0x0004
#define PKCS15_USAGE_SIGNRECOVER	0x0008
#define PKCS15_USAGE_WRAP			0x0010
#define PKCS15_USAGE_UNWRAP			0x0020
#define PKCS15_USAGE_VERIFY			0x0040
#define PKCS15_USAGE_VERIFYRECOVER	0x0080
#define PKCS15_USAGE_DERIVE			0x0100
#define PKCS15_USAGE_NONREPUDIATION	0x0200

/* Symbolic values for range checking of the usage flags */

#define PKSC15_USAGE_FLAG_NONE		0x0000
#define PKCS15_USAGE_FLAG_MAX		0x03FF

/* PKCS #15 flags that can't be set for public keys.  We use this as a mask
   to derive public-key flags from private key ones */

#define PUBKEY_USAGE_MASK	~( PKCS15_USAGE_DECRYPT | PKCS15_USAGE_SIGN | \
							   PKCS15_USAGE_SIGNRECOVER | PKCS15_USAGE_UNWRAP )

/* PKCS #15 usage types for encryption and signature keys.  We use these when
   looking specifically for signing or encryption keys */

#define ENCR_USAGE_MASK		( PKCS15_USAGE_ENCRYPT | PKCS15_USAGE_DECRYPT | \
							  PKCS15_USAGE_WRAP | PKCS15_USAGE_UNWRAP )
#define SIGN_USAGE_MASK		( PKCS15_USAGE_SIGN | PKCS15_USAGE_SIGNRECOVER | \
							  PKCS15_USAGE_VERIFY | PKCS15_USAGE_VERIFYRECOVER | \
							  PKCS15_USAGE_NONREPUDIATION )

/* The access flags for various types of key objects.  For a public key we
   set 'extractable', for a private key we set 'sensitive',
   'alwaysSensitive', and 'neverExtractable' */

#define KEYATTR_ACCESS_PUBLIC	0x02	/* 00010b */
#define KEYATTR_ACCESS_PRIVATE	0x0D	/* 01101b */

/* When adding a public key and/or certificate to a PKCS #15 collection we 
   have to be able to cleanly handle the addition of arbitrary collections of 
   potentially overlapping objects.  Since a public key is a subset of the 
   data in a certificate, if we're adding a certificate + public key pair 
   the certificate data overrides the public key, which isn't added at all.  
   This leads to some rather convoluted logic for deciding what needs 
   updating and under which conditions.  The actions taken are:

	key only:	if present
					return( CRYPT_ERROR_DUPLICATE )
				else
					add key;
	cert only:	if present
					return( CRYPT_ERROR_DUPLICATE );
				elif( matching key present )
					add, delete key data;
				elif( trusted certificate )
					add as trusted certificate;
				else
					error;
	key+cert:	if key present and certificate present
					return( CRYPT_ERROR_DUPLICATE );
				delete key;
				if certificate present -> don't add certificate;

   The following values specify the action to be taken when adding a 
   certificate */

typedef enum {
	CERTADD_NONE,			/* No certificate add action */
	CERTADD_UPDATE_EXISTING,/* Update existing key info with a certificate */
	CERTADD_NORMAL,			/* Add a certificate for which no key info present */
	CERTADD_STANDALONE_CERT,/* Add a standalone certificate not assoc'd with a key */
	CERTADD_LAST			/* Last valid certificate add action */
	} CERTADD_TYPE;

/* Since PKCS #15 uses more key ID types than are used by the rest of
   cryptlib, we extend the standard range with PKCS15-only types */

#define CRYPT_KEYIDEX_ID				CRYPT_KEYID_NONE

/* The minimum size of an object in a keyset, used for sanity-checking when
   reading a keyset */

#define MIN_OBJECT_SIZE		16

/* When writing attributes it's useful to have a fixed-size buffer rather
   than having to mess around with all sorts of variable-length structures,
   the following value defines the maximum size of the attribute data that
   we can write (that is, the I/O stream is opened with this size and
   generates a CRYPT_ERROR_OVERFLOW if we go beyond this).  The maximum-
   length buffer contents are two CRYPT_MAX_TEXTSIZE strings, about half a
   dozen hashes (as IDs for subject, issuer, and so on), and a few odd bits 
   and pieces so this is plenty */

#define KEYATTR_BUFFER_SIZE	512

/****************************************************************************
*																			*
*							PKCS #15 Types and Structures					*
*																			*
****************************************************************************/

/* The following structure contains the the information for one personality,
   which covers one or more of a private key, public key, and certificate */

typedef struct {
	/* General information on the personality: The subtype, a local unique
	   identifier which is easier to manage than the iD (this is used when
	   enumerating PKCS #15 items in a keyset, the last-read-item entry is 
	   set to the index value), the PKCS #15 object label, and the PKCS #15 
	   object ID and key ID (which is usually the same as the object ID) */
	PKCS15_SUBTYPE type;			/* Personality subtype */
	int index;						/* Unique value for this personality */
	BUFFER( CRYPT_MAX_TEXTSIZE, labelLength ) \
	char label[ CRYPT_MAX_TEXTSIZE + 8 ];/* PKCS #15 object label */
	int labelLength;
	BUFFER( CRYPT_MAX_HASHSIZE, iDlength ) \
	BYTE iD[ CRYPT_MAX_HASHSIZE + 8 ];
	BUFFER( CRYPT_MAX_HASHSIZE, keyIDlength ) \
	BYTE keyID[ CRYPT_MAX_HASHSIZE + 8 ];
	int iDlength, keyIDlength;		/* PKCS #15 object ID and key ID */

	/* Certificate-related ID information: Hash of the issuer name, subject
	   name, and issuerAndSerialNumber, and PGP key IDs */
	BUFFER( KEYID_SIZE, iAndSIDlength ) \
	BYTE iAndSID[ KEYID_SIZE + 8 ];
	BUFFER( KEYID_SIZE, subjectNameIDlength ) \
	BYTE subjectNameID[ KEYID_SIZE + 8 ];
	BUFFER( KEYID_SIZE, issuerNameIDlength ) \
	BYTE issuerNameID[ KEYID_SIZE + 8 ];
	BUFFER( KEYID_SIZE, pgp2KeyIDlength ) \
	BYTE pgp2KeyID[ PGP_KEYID_SIZE + 8 ];
	BUFFER( KEYID_SIZE, openPGPKeyIDlength ) \
	BYTE openPGPKeyID[ PGP_KEYID_SIZE + 8 ];
	int iAndSIDlength, subjectNameIDlength, issuerNameIDlength;
	int pgp2KeyIDlength, openPGPKeyIDlength;

	/* Key/certificate object data */
	BUFFER_OPT_FIXED( pubKeyDataSize ) \
	void *pubKeyData;
	BUFFER_OPT_FIXED( privKeyDataSize ) \
	void *privKeyData;
	BUFFER_OPT_FIXED( certDataSize ) \
	void *certData;					/* Encoded object data */
	int pubKeyDataSize, privKeyDataSize, certDataSize;
	int pubKeyOffset, privKeyOffset, certOffset;
									/* Offset of payload in data */
	int pubKeyUsage, privKeyUsage;	/* Permitted usage for the key */
	int trustedUsage;				/* Usage which key is trusted for */
	BOOLEAN implicitTrust;			/* Whether certificate is implicitly trusted */
	time_t validFrom, validTo;		/* Key/certificate validity information */

	/* Secret-key object data */
	BUFFER_OPT_FIXED( secretKeyDataSize ) \
	void *secretKeyData;			/* Encoded object data */
	int secretKeyDataSize, secretKeyOffset;

	/* Data object data */
	CRYPT_ATTRIBUTE_TYPE dataType;	/* Type of the encoded object data */
	BUFFER_OPT_FIXED( dataDataSize ) \
	void *dataData;					/* Encoded object data */
	int dataDataSize, dataOffset;
	} PKCS15_INFO;

/****************************************************************************
*																			*
*								PKCS #15 ASN.1 Tags							*
*																			*
****************************************************************************/

/* Context-specific tags for object types */

enum { CTAG_PO_PRIVKEY, CTAG_PO_PUBKEY, CTAG_PO_TRUSTEDPUBKEY,
	   CTAG_PO_SECRETKEY, CTAG_PO_CERT, CTAG_PO_TRUSTEDCERT,
	   CTAG_PO_USEFULCERT, CTAG_PO_DATA, CTAG_PO_AUTH, CTAG_PO_LAST };

/* Context-specific tags for the PublicKeyInfo record */

enum { CTAG_PK_CERTIFICATE, CTAG_PK_CERTCHAIN, CTAG_PK_LAST };

/* Context-specific tags for the object record */

enum { CTAG_OB_SUBCLASSATTR, CTAG_OB_TYPEATTR, CTAG_OB_LAST };

/* Context-specific tags for the object value record */

enum { CTAG_OV_DIRECT, CTAG_OV_DUMMY, CTAG_OV_DIRECTPROTECTED, 
	   CTAG_OV_DUMMY_DIRECTPROTECTED_EXT, CTAG_OV_DIRECTPROTECTED_EXT,
	   CTAG_OV_LAST };

/* Context-specific tags for the class attributes record */

enum { CTAG_KA_VALIDTO, CTAG_KA_LAST };
enum { CTAG_CA_DUMMY, CTAG_CA_TRUSTED_USAGE, CTAG_CA_IDENTIFIERS,
	   CTAG_CA_TRUSTED_IMPLICIT, CTAG_CA_VALIDTO, CTAG_CA_LAST };
enum { CTAG_PK_IDENTIFIERS };

/* Context-specific tags for the public/private key objects record */

enum { CTAG_PK_ECC, CTAG_PK_DH, CTAG_PK_DSA, CTAG_PK_KEA };

/* Context-specific tags for the data objects record */

enum { CTAG_DO_EXTERNALDO, CTAG_DO_OIDDO, CTAG_DO_LAST };

/****************************************************************************
*																			*
*								PKCS #15 Functions							*
*																			*
****************************************************************************/

/* Utility functions in pkcs15.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5 ) ) \
int getCertID( IN_HANDLE const CRYPT_HANDLE iCryptHandle, 
			   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE nameType, 
			   OUT_BUFFER( nameIdMaxLen, *nameIdLen ) BYTE *nameID, 
			   IN_LENGTH_SHORT_MIN( KEYID_SIZE ) const int nameIdMaxLen,
			   OUT_LENGTH_BOUNDED_Z( nameIdMaxLen ) int *nameIdLen );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
PKCS15_INFO *findEntry( IN_ARRAY( noPkcs15objects ) const PKCS15_INFO *pkcs15info,
						IN_LENGTH_SHORT const int noPkcs15objects,
						IN_KEYID_OPT const CRYPT_KEYID_TYPE keyIDtype,
							/* CRYPT_KEYIDEX_ID maps to CRYPT_KEYID_NONE */
						IN_BUFFER_OPT( keyIDlength ) const void *keyID,
						IN_LENGTH_KEYID_Z const int keyIDlength,
						IN_FLAGS_Z( KEYMGMT ) const int requestedUsage,
						const BOOLEAN isWildcardMatch );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
PKCS15_INFO *findFreeEntry( IN_ARRAY( noPkcs15objects ) \
								const PKCS15_INFO *pkcs15info,
							IN_LENGTH_SHORT const int noPkcs15objects, 
							OUT_OPT_INDEX( noPkcs15objects ) int *index );
STDC_NONNULL_ARG( ( 1 ) ) \
void pkcs15freeEntry( INOUT PKCS15_INFO *pkcs15info );
STDC_NONNULL_ARG( ( 1 ) ) \
void pkcs15Free( INOUT_ARRAY( noPkcs15objects ) PKCS15_INFO *pkcs15info, 
				 IN_RANGE( 1, MAX_PKCS15_OBJECTS ) const int noPkcs15objects );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getValidityInfo( INOUT PKCS15_INFO *pkcs15info,
					 IN_HANDLE const CRYPT_HANDLE cryptHandle );

/* Prototypes for functions in pkcs15_add.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int getKeyTypeTag( IN_HANDLE_OPT const CRYPT_CONTEXT cryptContext,
				   IN_ALGO_OPT const CRYPT_ALGO_TYPE cryptAlgo,
				   OUT int *tag );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int addConfigData( IN_ARRAY( noPkcs15objects ) PKCS15_INFO *pkcs15info, 
				   IN_LENGTH_SHORT const int noPkcs15objects, 
				   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE dataType,
				   IN_BUFFER( dataLength ) const char *data, 
				   IN_LENGTH_SHORT const int dataLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addSecretKey( IN_ARRAY( noPkcs15objects ) PKCS15_INFO *pkcs15info, 
				  IN_LENGTH_SHORT const int noPkcs15objects,
				  IN_HANDLE const CRYPT_HANDLE iCryptContext );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 11 ) ) \
int pkcs15AddKey( INOUT PKCS15_INFO *pkcs15infoPtr, 
				  IN_HANDLE const CRYPT_HANDLE iCryptHandle,
				  IN_BUFFER_OPT( passwordLength ) const void *password, 
				  IN_LENGTH_NAME_Z const int passwordLength,
				  IN_HANDLE const CRYPT_USER iOwnerHandle, 
				  const BOOLEAN privkeyPresent, const BOOLEAN certPresent, 
				  const BOOLEAN doAddCert, const BOOLEAN pkcs15keyPresent,
				  const BOOLEAN isStorageObject, 
				  INOUT ERROR_INFO *errorInfo );

/* Prototypes for functions in pkcs15_adpb.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
int pkcs15AddCert( INOUT PKCS15_INFO *pkcs15infoPtr, 
				   IN_HANDLE const CRYPT_CERTIFICATE iCryptCert,
				   IN_BUFFER_OPT( privKeyAttributeSize ) \
					const void *privKeyAttributes, 
				   IN_LENGTH_SHORT_Z const int privKeyAttributeSize,
				   IN_ENUM( CERTADD ) const CERTADD_TYPE certAddType, 
				   INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int pkcs15AddCertChain( INOUT PKCS15_INFO *pkcs15info, 
						IN_LENGTH_SHORT const int noPkcs15objects,
						IN_HANDLE const CRYPT_CERTIFICATE iCryptCert, 
						INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 8 ) ) \
int pkcs15AddPublicKey( INOUT PKCS15_INFO *pkcs15infoPtr, 
						IN_HANDLE const CRYPT_HANDLE iCryptContext, 
						IN_BUFFER( pubKeyAttributeSize ) \
							const void *pubKeyAttributes, 
						IN_LENGTH_SHORT const int pubKeyAttributeSize,
						IN_ALGO const CRYPT_ALGO_TYPE pkcCryptAlgo, 
						IN_LENGTH_PKC const int modulusSize, 
						const BOOLEAN isStorageObject, 
						INOUT ERROR_INFO *errorInfo );

/* Prototypes for functions in pkcs15_adpr.c.  The private-key attribute 
   functions have to be accessible externally because adding or changing a 
   certificate for a private key can update the private-key attributes */

STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
void updatePrivKeyAttributes( INOUT PKCS15_INFO *pkcs15infoPtr,
							  OUT_BUFFER_FIXED( newPrivKeyDataSize ) \
								void *newPrivKeyData, 
							  IN_LENGTH_SHORT_MIN( 16 ) \
								const int newPrivKeyDataSize,
							  IN_BUFFER( privKeyAttributeSize ) \
								const void *privKeyAttributes, 
							  IN_LENGTH_SHORT const int privKeyAttributeSize, 
							  IN_LENGTH_SHORT const int privKeyInfoSize, 
							  IN_TAG const int keyTypeTag );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int calculatePrivkeyStorage( OUT_BUFFER_ALLOC_OPT( *newPrivKeyDataSize ) \
								void **newPrivKeyDataPtr, 
							 OUT_LENGTH_SHORT_Z int *newPrivKeyDataSize, 
							 IN_BUFFER_OPT( origPrivKeyDataSize ) \
								const void *origPrivKeyData,
							 IN_LENGTH_SHORT_Z const int origPrivKeyDataSize,
							 IN_LENGTH_SHORT const int privKeySize,
							 IN_LENGTH_SHORT const int privKeyAttributeSize,
							 IN_LENGTH_SHORT_Z const int extraDataSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6, 11 ) ) \
int pkcs15AddPrivateKey( INOUT PKCS15_INFO *pkcs15infoPtr, 
						 IN_HANDLE const CRYPT_HANDLE iPrivKeyContext,
						 IN_HANDLE const CRYPT_HANDLE iCryptOwner,
						 IN_BUFFER_OPT( passwordLength ) const char *password, 
						 IN_LENGTH_NAME_Z const int passwordLength,
						 IN_BUFFER( privKeyAttributeSize ) \
							const void *privKeyAttributes, 
						 IN_LENGTH_SHORT const int privKeyAttributeSize,
						 IN_ALGO const CRYPT_ALGO_TYPE pkcCryptAlgo, 
						 IN_LENGTH_PKC const int modulusSize, 
						 const BOOLEAN isStorageObject, 
						 INOUT ERROR_INFO *errorInfo );

/* Prototypes for functions in pkcs15_atrd.c/pkcs15_atwr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 6, 7 ) ) \
int writeKeyAttributes( OUT_BUFFER( privKeyAttributeMaxLen, \
									*privKeyAttributeSize ) 
							void *privKeyAttributes, 
						IN_LENGTH_SHORT_MIN( 16 ) \
							const int privKeyAttributeMaxLen,
						OUT_LENGTH_BOUNDED_Z( privKeyAttributeMaxLen ) \
							int *privKeyAttributeSize, 
						OUT_BUFFER( pubKeyAttributeMaxLen, \
									*pubKeyAttributeSize ) \
							void *pubKeyAttributes, 
						IN_LENGTH_SHORT_MIN( 16 ) \
							const int pubKeyAttributeMaxLen,
						OUT_LENGTH_BOUNDED_Z( pubKeyAttributeMaxLen ) \
							int *pubKeyAttributeSize, 
						INOUT PKCS15_INFO *pkcs15infoPtr, 
						IN_HANDLE const CRYPT_HANDLE iCryptContext,
						const BOOLEAN writeKeyIDs );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int writeCertAttributes( OUT_BUFFER( certAttributeMaxLen, *certAttributeSize ) \
							void *certAttributes, 
						 IN_LENGTH_SHORT_MIN( 16 ) const int certAttributeMaxLen,
						 OUT_LENGTH_BOUNDED_Z( certAttributeMaxLen ) \
							int *certAttributeSize, 
						 INOUT PKCS15_INFO *pkcs15infoPtr, 
						 IN_HANDLE const CRYPT_HANDLE iCryptCert );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readObjectAttributes( INOUT STREAM *stream, 
						  INOUT PKCS15_INFO *pkcs15infoPtr,
						  IN_ENUM( PKCS15_OBJECT ) const PKCS15_OBJECT_TYPE type, 
						  INOUT ERROR_INFO *errorInfo );

/* Prototypes for functions in pkcs15_get/set.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initPKCS15get( INOUT KEYSET_INFO *keysetInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initPKCS15set( INOUT KEYSET_INFO *keysetInfoPtr );

/* Prototypes for functions in pkcs15_wr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int pkcs15Flush( INOUT STREAM *stream, 
				 IN_ARRAY( noPkcs15objects ) const PKCS15_INFO *pkcs15info, 
				 IN_LENGTH_SHORT const int noPkcs15objects );

/* Prototypes for functions in pkcs15_rd.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 8, 9, 10, 11, 12 ) ) \
int readPublicKeyComponents( const PKCS15_INFO *pkcs15infoPtr,
							 IN_HANDLE const CRYPT_KEYSET iCryptKeysetCallback,
							 IN_ENUM( CRYPT_KEYID ) \
								const CRYPT_KEYID_TYPE keyIDtype,
							 IN_BUFFER( keyIDlength ) const void *keyID, 
							 IN_LENGTH_KEYID const int keyIDlength,
							 const BOOLEAN publicComponentsOnly,
							 IN_HANDLE const CRYPT_DEVICE iDeviceObject, 
							 OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContextPtr,
							 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iDataCertPtr,
							 OUT_FLAGS_Z( ACTION_PERM ) int *pubkeyActionFlags, 
							 OUT_FLAGS_Z( ACTION_PERM ) int *privkeyActionFlags, 
							 INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
int readPrivateKeyComponents( const PKCS15_INFO *pkcs15infoPtr,
							  IN_HANDLE const CRYPT_CONTEXT iPrivKeyContext,
							  IN_BUFFER_OPT( passwordLength ) \
									const void *password, 
							  IN_LENGTH_NAME_Z const int passwordLength, 
							  const BOOLEAN isStorageObject, 
							  INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
int readPkcs15Keyset( INOUT STREAM *stream, 
					  OUT_ARRAY( maxNoPkcs15objects ) PKCS15_INFO *pkcs15info, 
					  IN_LENGTH_SHORT const int maxNoPkcs15objects, 
					  IN_LENGTH const long endPos,
					  INOUT ERROR_INFO *errorInfo );

#endif /* _PKCS15_DEFINED */
