/****************************************************************************
*																			*
*					  cryptlib Encryption Context Header File 				*
*						Copyright Peter Gutmann 1992-2009					*
*																			*
****************************************************************************/

#ifndef _CRYPTCTX_DEFINED

#define _CRYPTCTX_DEFINED

/* Various include files needed by contexts.  Since the bignum and stream
   headers are only needed by PKC contexts, we only apply them in modules 
   that use PKC contexts */

#ifdef PKC_CONTEXT
  #ifndef _STREAM_DEFINED
	#if defined( INC_ALL )
	  #include "stream.h"
	#else
	  #include "io/stream.h"
	#endif /* Compiler-specific includes */
  #endif /* _STREAM_DEFINED */
  #ifndef HEADER_BN_H
	#if defined( INC_ALL )
	  #include "bn.h"
	#else
	  #include "bn/bn.h"
	#endif /* Compiler-specific includes */
  #endif /* HEADER_BN_H */
  #if ( defined( USE_ECDH ) || defined( USE_ECDSA ) ) && !defined( HEADER_EC_H )
	#if defined( INC_ALL )
	  #include "ec_lcl.h"
	#else
	  #include "bn/ec_lcl.h"
	#endif /* Compiler-specific includes */
  #endif /* ( USE_ECDH || USE_ECDSA ) && HEADER_EC_H */
#endif /* Extra eaders needed only for PKC contexts */
#ifndef _CRYPTCAP_DEFINED
  #if defined( INC_ALL )
	#include "capabil.h"
  #else
	#include "device/capabil.h"
  #endif /* Compiler-specific includes */
#endif /* _CRYPTCAP_DEFINED */

/****************************************************************************
*																			*
*							Context Types and Constants						*
*																			*
****************************************************************************/

/* Context information flags.  Most of these flags are context-type-specific,
   and are only used with some context types:

	FLAG_DUMMY: The context is a dummy context with actions handled through 
			an external crypto device.  When a device context is created, it 
			usually isn't instantiated at the device level until the key 
			(and possibly other parameters) are available because most 
			devices use an atomic created-initialised-context operation 
			rather than allowing incremental parameter setting like cryptlib
			does.  To handle this, we first create a dummy context and then
			fill in the details on demand.

	FLAG_DUMMY_INITED: The dummy context has been initialised.  Since the 
			context isn't instantiated until required, this flag is needed 
			to keep track of whether any cached parameters retained from the 
			dummy state need to be set when the context is used.

	FLAG_HASH_INITED: The hash parameters have been inited.
	FLAG_HASH_DONE: The hash operation is complete, no further hashing can 
			be done 

	FLAG_ISPUBLICKEY: The key is a public or private key.
	FLAG_ISPRIVATEKEY:

	FLAG_IV_SET: The IV has been set.
	FLAG_KEY_SET: The key has been initialised.
	FLAG_PGPKEYID_SET: The PGP keyID has been set (this is only valid for 
			RSA keys, and may not be enabled if we're not using PGP).
	FLAG_OPENPGPKEYID_SET: The OpenPGP keyID has been set (this may not be 
			enabled if we're not using PGP).

	FLAG_PERSISTENT: The context is backed by a keyset or crypto device.

	FLAG_SIDECHANNELPROTECTION: The context has side-channel protection
			(additional checking for crypto operations, blinding, and so
			on) enabled.
	
	FLAG_FLAG_STATICCONTEXT: The context data has been instantiated via
			staticInitContext() for internal use and doesn't correspond to 
			an actual cryptlib object */

#define CONTEXT_FLAG_NONE			0x0000	/* No context flag */
#define CONTEXT_FLAG_KEY_SET		0x0001	/* Key has been set */
#define CONTEXT_FLAG_IV_SET			0x0002	/* IV has been set */
#define CONTEXT_FLAG_ISPUBLICKEY	0x0004	/* Key is a public key */
#define CONTEXT_FLAG_ISPRIVATEKEY	0x0008	/* Key is a private key */
#define CONTEXT_FLAG_DUMMY			0x0010	/* Context actions handled externally */
#define CONTEXT_FLAG_DUMMY_INITED	0x0020	/* Dummy context is inited */
#define CONTEXT_FLAG_PERSISTENT		0x0040	/* Context is backed by dev.or keyset */
#define CONTEXT_FLAG_SIDECHANNELPROTECTION \
									0x0080	/* Enabled side-channel prot.in ops */
#define CONTEXT_FLAG_HASH_INITED	0x0100	/* Hash parameters have been inited */
#define CONTEXT_FLAG_HASH_DONE		0x0200	/* Hash operation is complete */
#define CONTEXT_FLAG_PGPKEYID_SET	0x0400	/* PGP keyID is set */
#define CONTEXT_FLAG_OPENPGPKEYID_SET 0x0800 /* OpenPGP keyID is set */
#define CONTEXT_FLAG_STATICCONTEXT	0x1000	/* Static context */
#define CONTEXT_FLAG_MAX			0x1FFF	/* Maximum possible flag value */

/* DLP PKCs require a random value k that's then reduced mod p or q, however
   if we make sizeof( k ) == sizeof( p ) then this introduced a bias into k
   that eventually leaks the private key (see "The Insecurity of the Digital 
   Signature Algorithm with Partially Known Nonces" by Phong Nguyen and Igor 
   Shparlinski).  To get around this we generate a k that's slightly larger
   than required, the following defines the size in bytes of this overflow
   factor */

#define DLP_OVERFLOW_SIZE			bitsToBytes( 32 )

/****************************************************************************
*																			*
*							Context Data Structures							*
*																			*
****************************************************************************/

/* The internal fields in a context that hold data for a conventional,
   public-key, hash, or MAC algorithm.  CONTEXT_CONV and CONTEXT_MAC
   should be allocated in pagelocked memory since they contain the sensitive
   userKey data */

typedef enum { 
	CONTEXT_NONE,					/* No context type */
	CONTEXT_CONV,					/* Conventional encryption context */
	CONTEXT_PKC,					/* PKC context */
	CONTEXT_HASH,					/* Hash context */
	CONTEXT_MAC,					/* MAC context */
	CONTEXT_GENERIC,				/* Generic-secret context */
	CONTEXT_LAST					/* Last valid context type */
	} CONTEXT_TYPE;

#define needsSecureMemory( contextType ) \
		( contextType == CONTEXT_CONV || contextType == CONTEXT_MAC || \
		  contextType == CONTEXT_GENERIC )

typedef struct {
	/* General algorithm information */
	CRYPT_MODE_TYPE mode;			/* Encryption mode being used */

	/* User keying information.  The user key is the unprocessed key as
	   entered by the user (rather than the key in the form used by the
	   algorithm), the IV is the initial IV.  We keep a copy of the
	   unprocessed key because we usually need to wrap it up in a KEK
	   at some point after it's loaded */
	BUFFER( CRYPT_MAX_KEYSIZE, userKeyLength ) \
	BYTE userKey[ CRYPT_MAX_KEYSIZE + 8 ];	/* User encryption key */
	BUFFER( CRYPT_MAX_IVSIZE, ivLength ) \
	BYTE iv[ CRYPT_MAX_IVSIZE + 8 ];/* Initial IV */
	int userKeyLength, ivLength;

	/* Conventional encryption keying information.  The key is the processed
	   encryption key stored in whatever form is required by the algorithm,
	   usually the key-scheduled user key.  The IV is the current working IV.
	   The ivCount is the number of bytes of IV that have been used, and is
	   used when a block cipher is used as a stream cipher */
	void *key;						/* Internal working key */
	BUFFER( CRYPT_MAX_IVSIZE, ivLength ) \
	BYTE currentIV[ CRYPT_MAX_IVSIZE + 8 ];	/* Internal working IV */
	int ivCount;					/* Internal IV count for chaining modes */

	/* Information required when a key suitable for use by this algorithm
	   is derived from a longer user key */
	BUFFER( CRYPT_MAX_HASHSIZE, saltLength ) \
	BYTE salt[ CRYPT_MAX_HASHSIZE + 8 ];/* Salt */
	int saltLength;
	int keySetupIterations;			/* Number of times setup was iterated */
	CRYPT_ALGO_TYPE keySetupAlgorithm; /* Algorithm used for key setup */
	} CONV_INFO;

#ifdef PKC_CONTEXT

typedef struct {
	/* General information on the key: The nominal key size in bits, the key
	   IDs, and key-related meta-info.  Since the OpenPGP key ID can't be
	   calculated directly like the other IDs, we have to keep track of
	   whether it's been set or not with a flag */
	int keySizeBits;				/* Nominal key size in bits */
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE keyID[ KEYID_SIZE + 8 ];	/* Key ID for this key */
	BUFFER_FIXED( PGP_KEYID_SIZE ) \
	BYTE pgp2KeyID[ PGP_KEYID_SIZE + 8 ];/* PGP 2 key ID for this key */
	BUFFER_FIXED( PGP_KEYID_SIZE ) \
	BYTE openPgpKeyID[ PGP_KEYID_SIZE + 8 ];/* OpenPGP key ID for this key */
	time_t pgpCreationTime;			/* Key creation time (for OpenPGP ID) */

	/* Public-key encryption keying information.  Since each algorithm has
	   its own unique parameters, the bignums are given generic names here.
	   The algorithm-specific code refers to them by their actual names,
	   which are implemented as symbolic defines of the form
	   <algo>Param_<param_name>, e.g.rsaParam_e */
	BIGNUM param1;					/* PKC key components */
	BIGNUM param2;
	BIGNUM param3;
	BIGNUM param4;
	BIGNUM param5;
	BIGNUM param6;
	BIGNUM param7;
	BIGNUM param8;
#if defined( USE_ECDH ) || defined( USE_ECDSA ) 
	BIGNUM param9;
	BIGNUM param10;
#endif /* USE_ECDH || USE_ECDSA */
	BN_MONT_CTX montCTX1;			/* Precomputed Montgomery values */
	BN_MONT_CTX montCTX2;
	BN_MONT_CTX montCTX3;
#if defined( USE_ECDH ) || defined( USE_ECDSA ) 
	CRYPT_ECCCURVE_TYPE curveType;	/* Additional info.needed for ECC ctxs.*/
	BOOLEAN isECC;
	EC_GROUP *ecCTX;
	EC_POINT *ecPoint;
#endif /* USE_ECDH || USE_ECDSA */
	int metadataChecksum, checksum;	/* Checksums for key data */

	/* Temporary workspace values used to avoid having to allocate and
	   deallocate them on each PKC operation, and to keep better control
	   over the data in them.  DLP operations that require extensive
	   temporary vars also reuse the last three general-purpose bignums
	   above, since they're not used for keying material */
	BIGNUM tmp1, tmp2, tmp3;
#if defined( USE_ECDH ) || defined( USE_ECDSA ) 
	BIGNUM tmp4, tmp5;
	EC_POINT *tmpPoint;
#endif /* USE_ECDH || USE_ECDSA */
	BN_CTX *bnCTX;
	#define CONTEXT_FLAG_PBO 0x08

	/* If we're using side-channel protection, we also need to store values
	   used to perform extra operations that eliminate timing channels */
	BIGNUM blind1, blind2;

	/* If the context is tied to a device the keying info won't be available,
	   however we generally need the public key information for use in cert
	   requests and whatnot so we save a copy as SubjectPublicKeyInfo when
	   the key is loaded/generated */
	BUFFER_OPT_FIXED( publicKeyInfoSize ) \
	void *publicKeyInfo;			/* X.509 SubjectPublicKeyInfo */
	int publicKeyInfoSize;			/* Key info size */

	/* Pointers to functions to public-key context access methods.  The
	   functions to read and write public and private keys are kept distinct
	   to enforce red/black separation */
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *calculateKeyIDFunction )( INOUT struct CI *contextInfoPtr );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *readPublicKeyFunction )( INOUT STREAM *stream, 
									INOUT struct CI *contextInfoPtr,
									IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *readPrivateKeyFunction )( INOUT STREAM *stream, 
									 INOUT struct CI *contextInfoPtr,
									 IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
	int ( *writePublicKeyFunction )( INOUT STREAM *stream,
									 const struct CI *contextInfoPtr,
									 IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType,
									 IN_BUFFER( accessKeyLen ) \
										const char *accessKey, 
									 IN_LENGTH_SHORT_MIN( 4 ) \
										const int accessKeyLen );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
	int ( *writePrivateKeyFunction )( INOUT STREAM *stream,
									  const struct CI *contextInfoPtr,
									  IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType,
									  IN_BUFFER( accessKeyLen ) \
										const char *accessKey,
									  IN_LENGTH_SHORT_MIN( 4 ) \
										const int accessKeyLen );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
	int ( *encodeDLValuesFunction )( OUT_BUFFER( bufMaxSize, \
												 *bufSize ) BYTE *buffer, 
									 IN_LENGTH_SHORT_MIN( 20 + 20 ) \
										const int bufMaxSize, 
									 OUT_LENGTH_SHORT_Z int *bufSize, 
									 const BIGNUM *value1, 
									 const BIGNUM *value2, 
									 IN_ENUM( CRYPT_FORMAT ) \
										const CRYPT_FORMAT_TYPE formatType );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
	int ( *decodeDLValuesFunction )( IN_BUFFER( bufSize ) const BYTE *buffer, 
									 IN_LENGTH_SHORT_MIN( 32 ) const int bufSize, 
									 OUT BIGNUM *value1, 
									 OUT BIGNUM *value2, 
									 const BIGNUM *maxRange,
									 IN_ENUM( CRYPT_FORMAT_TYPE ) \
										const CRYPT_FORMAT_TYPE formatType );
	} PKC_INFO;
#endif /* PKC_CONTEXT */

typedef struct {
	/* The current state of the hashing and the result from the last
	   completed hash operation */
	void *hashInfo;					/* Current hash state */
	BUFFER_FIXED( CRYPT_MAX_HASHSIZE ) \
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];/* Last hash result */
	} HASH_INFO;

typedef struct {
	/* User keying information.  The user key is the unprocessed key as
	   entered by the user rather than the key in the form used by the
	   algorithm.  We keep a copy of the unprocessed key because we usually 
	   need to wrap it up in a KEK at some point after it's loaded */
	BUFFER( CRYPT_MAX_KEYSIZE, userKeyLength ) \
	BYTE userKey[ CRYPT_MAX_KEYSIZE + 8 ];	/* User MAC key */
	int userKeyLength;

	/* The current state of the MAC'ing and the result from the last
	   completed MAC operation */
	void *macInfo;					/* Current MAC state */
	BUFFER_FIXED( CRYPT_MAX_HASHSIZE ) \
	BYTE mac[ CRYPT_MAX_HASHSIZE + 8 ];	/* Last MAC result */

	/* Information required when a key suitable for use by this algorithm
	   is derived from a longer user key */
	BUFFER( CRYPT_MAX_HASHSIZE, saltLength ) \
	BYTE salt[ CRYPT_MAX_HASHSIZE + 8 ];/* Salt */
	int saltLength;
	int keySetupIterations;			/* Number of times setup was iterated */
	CRYPT_ALGO_TYPE keySetupAlgorithm; /* Algorithm used for key setup */
	} MAC_INFO;

typedef struct {
	/* Generic-secret information */
	BUFFER( CRYPT_MAX_KEYSIZE, genericSecretLength ) \
	BYTE genericSecret[ CRYPT_MAX_KEYSIZE + 8 ];
	int genericSecretLength;

	/* Parameter information for the encryption and MAC contexts that are
	   derived from the generic-secret context */
	BUFFER( CRYPT_MAX_TEXTSIZE, encAlgoParamSize ) \
	BYTE encAlgoParams[ CRYPT_MAX_TEXTSIZE + 8 ];
	int encAlgoParamSize;
	BUFFER( CRYPT_MAX_TEXTSIZE, macAlgoParamSize ) \
	BYTE macAlgoParams[ CRYPT_MAX_TEXTSIZE + 8 ];
	int macAlgoParamSize;
	} GENERIC_INFO;

/* Defines to make access to the union fields less messy */

#define ctxConv		keyingInfo.convInfo
#define ctxPKC		keyingInfo.pkcInfo
#define ctxHash		keyingInfo.hashInfo
#define ctxMAC		keyingInfo.macInfo
#define ctxGeneric	keyingInfo.genericInfo

/* An encryption context */

typedef struct CI {
	/* Control and status information */
	CONTEXT_TYPE type;				/* The context type */
	const CAPABILITY_INFO *capabilityInfo;	/* Encryption capability info */
	int flags;						/* Context information flags */

	/* Context type-specific information */
	union {
		CONV_INFO *convInfo;
#ifdef PKC_CONTEXT
		PKC_INFO *pkcInfo;
#endif /* PKC_CONTEXT */
		HASH_INFO *hashInfo;
		MAC_INFO *macInfo;
		GENERIC_INFO *genericInfo;
		} keyingInfo;

#ifdef USE_DEVICES
	/* If implemented using a crypto device, the object information is
	   usually stored inside the device.  The following value contains the
	   reference to the crypto object inside the device.  In addition some
	   objects (specifically, DH) that aren't really public- or private-key
	   objects but a mixture of both require a second handle to the other 
	   part of the object in the device */
	long deviceObject, altDeviceObject;

  #ifdef USE_HARDWARE
	/* When data used to instantiate a context is stored in a device we may
	   need a unique ID value to locate the data, the following value 
	   contains this storage ID */
	BUFFER_FIXED( KEYID_SIZE ) \
	BYTE deviceStorageID[ KEYID_SIZE + 8 ];
	BOOLEAN deviceStorageIDset;
  #endif /* USE_HARDWARE */
#endif /* USE_DEVICES */

	/* The label for this object, typically used to identify stored keys */
	BUFFER( CRYPT_MAX_TEXTSIZE, labelSize ) \
	char label[ CRYPT_MAX_TEXTSIZE + 8 ];/* Text string identifying key */
	int labelSize;

	/* Pointers to context access methods.  These are somewhat higher-level
	   than the capability info methods and apply to entire classes of
	   context rather than at a per-algorithm level */
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *loadKeyFunction )( INOUT struct CI *contextInfoPtr, 
							  IN_BUFFER_OPT( keyLength ) const void *key, 
							  IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) \
								const int keyLength );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *generateKeyFunction )( INOUT struct CI *contextInfoPtr );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptFunction )( INOUT struct CI *contextInfoPtr, 
							  INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							  IN_LENGTH_Z int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptFunction )( INOUT struct CI *contextInfoPtr, 
							  INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							  IN_LENGTH_Z int length );

	/* Error information */
	CRYPT_ATTRIBUTE_TYPE errorLocus;/* Error locus */
	CRYPT_ERRTYPE_TYPE errorType;	/* Error type */

	/* The object's handle and the handle of the user who owns this object.
	   The former is used when sending messages to the object when only the
	   xxx_INFO is available, the latter is used to avoid having to fetch the
	   same information from the system object table */
	CRYPT_HANDLE objectHandle;
	CRYPT_USER ownerHandle;
	} CONTEXT_INFO;

/* Symbolic defines for the various PKC components for different PKC
   algorithms.  All of the DLP algorithms actually use the same parameters,
   so we define generic DLP names for them */

#define dlpParam_p			param1
#define dlpParam_g			param2
#define dlpParam_q			param3
#define dlpParam_y			param4
#define dlpParam_x			param5
#define dlpTmp1				param6
#define dlpTmp2				param7
#define dlpTmp3				param8		/* More temp.values for DLP PKCs */
#define dhParam_yPrime		param8		/* Special value for DH */
#define dlpParam_mont_p		montCTX1

#define rsaParam_n			param1
#define rsaParam_e			param2
#define rsaParam_d			param3
#define rsaParam_p			param4
#define rsaParam_q			param5
#define rsaParam_u			param6
#define rsaParam_exponent1	param7
#define rsaParam_exponent2	param8
#define rsaParam_blind_k	blind1
#define rsaParam_blind_kInv	blind2
#define rsaParam_mont_n		montCTX1
#define rsaParam_mont_p		montCTX2
#define rsaParam_mont_q		montCTX3

#define eccParam_p			param1
#define eccParam_a			param2
#define eccParam_b			param3
#define eccParam_gx			param4
#define eccParam_gy			param5
#define eccParam_n			param6
#define eccParam_h			param7
#define eccParam_qx			param8
#define eccParam_qy			param9
#define eccParam_d			param10
#define eccParam_mont_p		montCTX1
#define eccParam_mont_n		montCTX2

/* Minimum and maximum permitted lengths for various PKC components.  These
   can be loaded in various ways (read from ASN.1 data, read from 
   PGP/SSH/SSL data, loaded by the user, and so on) so we define permitted
   length values in a central location for use in the different read 
   routines */

#define RSAPARAM_MIN_N		MIN_PKCSIZE
#define RSAPARAM_MAX_N		CRYPT_MAX_PKCSIZE
#define RSAPARAM_MIN_E		1
#define RSAPARAM_MAX_E		4
#define RSAPARAM_MIN_D		MIN_PKCSIZE
#define RSAPARAM_MAX_D		CRYPT_MAX_PKCSIZE
#define RSAPARAM_MIN_P		MIN_PKCSIZE / 2
#define RSAPARAM_MAX_P		CRYPT_MAX_PKCSIZE
#define RSAPARAM_MIN_Q		MIN_PKCSIZE / 2
#define RSAPARAM_MAX_Q		CRYPT_MAX_PKCSIZE
#define RSAPARAM_MIN_U		MIN_PKCSIZE / 2
#define RSAPARAM_MAX_U		CRYPT_MAX_PKCSIZE
#define RSAPARAM_MIN_EXP1	MIN_PKCSIZE / 2
#define RSAPARAM_MAX_EXP1	CRYPT_MAX_PKCSIZE
#define RSAPARAM_MIN_EXP2	MIN_PKCSIZE / 2
#define RSAPARAM_MAX_EXP2	CRYPT_MAX_PKCSIZE

#define DLPPARAM_MIN_P		MIN_PKCSIZE
#define DLPPARAM_MAX_P		CRYPT_MAX_PKCSIZE
#define DLPPARAM_MIN_G		1
#define DLPPARAM_MAX_G		CRYPT_MAX_PKCSIZE
#define DLPPARAM_MIN_Q		bitsToBytes( 128 )
#define DLPPARAM_MAX_Q		CRYPT_MAX_PKCSIZE
#define DLPPARAM_MIN_Y		MIN_PKCSIZE
#define DLPPARAM_MAX_Y		CRYPT_MAX_PKCSIZE
#define DLPPARAM_MIN_X		bitsToBytes( 128 )
#define DLPPARAM_MAX_X		CRYPT_MAX_PKCSIZE

#define DLPPARAM_MIN_SIG_R	DLPPARAM_MIN_Q	/* For DSA sigs */
#define DLPPARAM_MIN_SIG_S	DLPPARAM_MIN_Q	/* For DSA sigs */

#define ECCPARAM_MIN_P		MIN_PKCSIZE_ECC
#define ECCPARAM_MAX_P		CRYPT_MAX_PKCSIZE_ECC
#define ECCPARAM_MIN_A		MIN_PKCSIZE_ECC / 2
#define ECCPARAM_MAX_A		CRYPT_MAX_PKCSIZE_ECC
#define ECCPARAM_MIN_B		MIN_PKCSIZE_ECC / 2
#define ECCPARAM_MAX_B		CRYPT_MAX_PKCSIZE_ECC
#define ECCPARAM_MIN_GX		1
#define ECCPARAM_MAX_GX		CRYPT_MAX_PKCSIZE_ECC
#define ECCPARAM_MIN_GY		1
#define ECCPARAM_MAX_GY		CRYPT_MAX_PKCSIZE_ECC
#define ECCPARAM_MIN_N		MIN_PKCSIZE_ECC
#define ECCPARAM_MAX_N		CRYPT_MAX_PKCSIZE_ECC
#define ECCPARAM_MIN_H		MIN_PKCSIZE_ECC
#define ECCPARAM_MAX_H		CRYPT_MAX_PKCSIZE_ECC
#define ECCPARAM_MIN_QX		MIN_PKCSIZE_ECC / 2
#define ECCPARAM_MAX_QX		CRYPT_MAX_PKCSIZE_ECC
#define ECCPARAM_MIN_QY		MIN_PKCSIZE_ECC / 2
#define ECCPARAM_MAX_QY		CRYPT_MAX_PKCSIZE_ECC
#define ECCPARAM_MIN_D		MIN_PKCSIZE_ECC / 2
#define ECCPARAM_MAX_D		CRYPT_MAX_PKCSIZE_ECC

#define ECCPARAM_MIN_SIG_R	ECCPARAM_MIN_QX	/* For ECDSA sigs */
#define ECCPARAM_MIN_SIG_S	ECCPARAM_MIN_QX	/* For ECDSA sigs */

/* Because there's no really clean way to throw an exception in C and the
   bnlib routines don't carry around state information like cryptlib objects
   do, we need to perform an error check for most of the routines we call.
   To make this slightly less ugly we define the following macro that
   performs the check for us by updating a variable called `bnStatus' with
   the result of a bnlib call, which returns 1 for OK and 0 for error.
   Annoyingly, this interface isn't quite consistent and some calls return
   pointers rather than integer values, so we define a second macro that
   checks for pointer values rather than integers */

#define CK( x )				bnStatus &= x
#define CKPTR( x )			bnStatus &= ( ( x ) == NULL ? 0 : 1 )
#define BN_STATUS			1
#define bnStatusOK( x )		bnStatus
#define bnStatusError( x )	( !bnStatus )
#define getBnStatus( x )	( bnStatus ? CRYPT_OK : CRYPT_ERROR_FAILED )

/****************************************************************************
*																			*
*								Internal API Functions						*
*																			*
****************************************************************************/

/* Determine whether a context needs to have a key loaded */

#define needsKey( contextInfoPtr ) \
		!( ( contextInfoPtr )->flags & CONTEXT_FLAG_KEY_SET )

/* Low-level capability checking and context-creation functions used when
   creating a context in a device */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCapability( const CAPABILITY_INFO FAR_BSS *capabilityInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int createContextFromCapability( OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
								 IN_HANDLE const CRYPT_USER iCryptOwner,
								 const CAPABILITY_INFO *capabilityInfoPtr,
								 IN_FLAGS_Z( CREATEOBJECT ) const int objectFlags );

/* Statically init/destroy a context for the self-check, and perform various
   types of self-check */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int staticInitContext( OUT CONTEXT_INFO *contextInfoPtr, 
					   IN_ENUM( CONTEXT_TYPE ) const CONTEXT_TYPE type, 
					   const CAPABILITY_INFO *capabilityInfoPtr,
					   OUT_BUFFER_FIXED( contextDataSize ) void *contextData, 
					   IN_LENGTH_SHORT_MIN( 32 ) const int contextDataSize,
					   IN_OPT void *keyData );
STDC_NONNULL_ARG( ( 1 ) ) \
void staticDestroyContext( INOUT CONTEXT_INFO *contextInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 6 ) ) \
int testCipher( const CAPABILITY_INFO *capabilityInfo, 
				INOUT void *keyDataStorage, 
				IN_BUFFER( keySize ) const void *key, 
				IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) const int keySize, 
				const void *plaintext,
				const void *ciphertext );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
int testHash( const CAPABILITY_INFO *capabilityInfo, 
			  INOUT void *hashDataStorage, 
			  IN_BUFFER_OPT( dataLength ) const void *data, 
			  IN_LENGTH_SHORT_Z const int dataLength, 
			  const void *hashValue );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 7 ) ) \
int testMAC( const CAPABILITY_INFO *capabilityInfo, 
			 INOUT void *macDataStorage, 
			 IN_BUFFER( keySize ) const void *key, 
			 IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) const int keySize, 
			 IN_BUFFER( dataLength ) const void *data, 
			 IN_LENGTH_SHORT_MIN( 8 ) const int dataLength,
			 const void *hashValue );

/* Context attribute handling functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getContextAttribute( INOUT CONTEXT_INFO *contextInfoPtr,
						 OUT_INT_Z int *valuePtr, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getContextAttributeS( INOUT CONTEXT_INFO *contextInfoPtr,
						  INOUT MESSAGE_DATA *msgData, 
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setContextAttribute( INOUT CONTEXT_INFO *contextInfoPtr,
						 IN_INT_Z const int value, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setContextAttributeS( INOUT CONTEXT_INFO *contextInfoPtr,
						  IN_BUFFER( dataLength ) const void *data,
						  IN_LENGTH const int dataLength,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteContextAttribute( INOUT CONTEXT_INFO *contextInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute );

/* General key load/generation routines */

STDC_NONNULL_ARG( ( 1 ) ) \
void initKeyHandling( INOUT CONTEXT_INFO *contextInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int setEncodedKey( INOUT CONTEXT_INFO *contextInfoPtr, 
				   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE keyType, 
				   IN_BUFFER( keyDataLen ) const void *keyData, 
				   IN_LENGTH_SHORT const int keyDataLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setKeyComponents( INOUT CONTEXT_INFO *contextInfoPtr, 
					  IN_BUFFER( keyDataLen ) const void *keyData, 
					  IN_LENGTH_SHORT_MIN( 32 ) const int keyDataLen );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int deriveKey( INOUT CONTEXT_INFO *contextInfoPtr, 
			   IN_BUFFER( keyValueLen ) const void *keyValue, 
			   IN_LENGTH_SHORT const int keyValueLen );

/* PKC key-generation and related routines */

#ifdef PKC_CONTEXT

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initCheckDLPkey( INOUT CONTEXT_INFO *contextInfoPtr, 
					 const BOOLEAN isDH, const BOOLEAN isPKCS3 );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int generateDLPkey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_LENGTH_SHORT_MIN( MIN_PKCSIZE * 8 ) const int keyBits );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initCheckRSAkey( INOUT CONTEXT_INFO *contextInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int generateRSAkey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_LENGTH_SHORT_MIN( MIN_PKCSIZE * 8 ) const int keyBits );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initCheckECCkey( INOUT CONTEXT_INFO *contextInfoPtr,
					 const BOOLEAN isECDH );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int generateECCkey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_LENGTH_SHORT_MIN( MIN_PKCSIZE_ECC * 8 ) \
						const int keyBits );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int getECCFieldSize( IN_ENUM( CRYPT_ECCCURVE ) const CRYPT_ECCCURVE_TYPE fieldID,
					 OUT_INT_Z int *fieldSize );

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int generateBignum( OUT BIGNUM *bn, 
					IN_LENGTH_SHORT_MIN( 120 ) const int noBits, 
					IN_BYTE const int high, IN_BYTE const int low );
STDC_NONNULL_ARG( ( 1 ) ) \
void clearTempBignums( INOUT PKC_INFO *pkcInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initContextBignums( INOUT PKC_INFO *pkcInfo, 
						IN_RANGE( 0, 3 ) const int sideChannelProtectionLevel,
						const BOOLEAN isECC );
STDC_NONNULL_ARG( ( 1 ) ) \
void freeContextBignums( INOUT PKC_INFO *pkcInfo, 
						 IN_FLAGS( CONTEXT ) const int contextFlags );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int calculateBignumChecksum( INOUT PKC_INFO *pkcInfo, 
							 IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo );
#endif /* PKC_CONTEXT */

/* Key read/write routines */

STDC_NONNULL_ARG( ( 1 ) ) \
void initKeyID( INOUT CONTEXT_INFO *contextInfoPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void initKeyRead( INOUT CONTEXT_INFO *contextInfoPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void initKeyWrite( INOUT CONTEXT_INFO *contextInfoPtr );

/* Internal functions shared across a small number of modules, declared via 
   a header to allow type checking (attributeToFormatType() from keyload.c, 
   hash functions from ctx_XXX.c accessed via the universal interface in 
   ctx_misc.c) */

CHECK_RETVAL \
int attributeToFormatType( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						   OUT_ENUM_OPT( KEYFORMAT ) KEYFORMAT_TYPE *keyformat );

STDC_NONNULL_ARG( ( 1 ) ) \
void md5HashBuffer( INOUT_OPT HASHINFO hashInfo, 
					OUT_BUFFER_OPT_C( outBufMaxLength, 16 ) BYTE *outBuffer, 
					IN_LENGTH_SHORT_Z const int outBufMaxLength,
					IN_BUFFER_OPT( inLength ) const void *inBuffer, 
					IN_LENGTH_SHORT_Z const int inLength,
					IN_ENUM( HASH_STATE ) const HASH_STATE hashState );
STDC_NONNULL_ARG( ( 1 ) ) \
void ripemd160HashBuffer( INOUT_OPT HASHINFO hashInfo, 
						  OUT_BUFFER_OPT_C( outBufMaxLength, 20 ) BYTE *outBuffer, 
						  IN_LENGTH_SHORT_Z const int outBufMaxLength,
						  IN_BUFFER_OPT( inLength ) const void *inBuffer, 
						  IN_LENGTH_SHORT_Z const int inLength,
						  IN_ENUM( HASH_STATE ) const HASH_STATE hashState );
STDC_NONNULL_ARG( ( 1 ) ) \
void shaHashBuffer( INOUT_OPT HASHINFO hashInfo, 
					OUT_BUFFER_OPT_C( outBufMaxLength, 20 ) BYTE *outBuffer, 
					IN_LENGTH_SHORT_Z const int outBufMaxLength,
					IN_BUFFER_OPT( inLength ) const void *inBuffer, 
					IN_LENGTH_SHORT_Z const int inLength,
					IN_ENUM( HASH_STATE ) const HASH_STATE hashState );
STDC_NONNULL_ARG( ( 1 ) ) \
void sha2HashBuffer( INOUT_OPT HASHINFO hashInfo, 
					 OUT_BUFFER_OPT_C( outBufMaxLength, 32 ) BYTE *outBuffer, 
					 IN_LENGTH_SHORT_Z const int outBufMaxLength,
					 IN_BUFFER_OPT( inLength ) const void *inBuffer, 
					 IN_LENGTH_SHORT_Z const int inLength,
					 IN_ENUM( HASH_STATE ) const HASH_STATE hashState );
STDC_NONNULL_ARG( ( 1 ) ) \
void sha2_ExtHashBuffer( INOUT_OPT HASHINFO hashInfo, 
						 OUT_BUFFER_OPT_C( outBufMaxLength, 64 ) BYTE *outBuffer, 
						 IN_LENGTH_SHORT_Z const int outBufMaxLength,
						 IN_BUFFER_OPT( inLength ) const void *inBuffer, 
						 IN_LENGTH_SHORT_Z const int inLength,
						 IN_ENUM( HASH_STATE ) const HASH_STATE hashState );

STDC_NONNULL_ARG( ( 1, 3 ) ) \
void md5HashBufferAtomic( OUT_BUFFER_C( outBufMaxLength, 16 ) BYTE *outBuffer, 
						  IN_LENGTH_SHORT_MIN( 16 ) const int outBufMaxLength,
						  IN_BUFFER( inLength ) const void *inBuffer, 
						  IN_LENGTH_SHORT const int inLength );
STDC_NONNULL_ARG( ( 1, 3 ) ) \
void ripemd160HashBufferAtomic( OUT_BUFFER_C( outBufMaxLength, 20 ) BYTE *outBuffer, 
								IN_LENGTH_SHORT_MIN( 20 ) const int outBufMaxLength,
							    IN_BUFFER( inLength ) const void *inBuffer, 
								IN_LENGTH_SHORT const int inLength );
STDC_NONNULL_ARG( ( 1, 3 ) ) \
void shaHashBufferAtomic( OUT_BUFFER_C( outBufMaxLength, 20 ) BYTE *outBuffer, 
						  IN_LENGTH_SHORT_MIN( 20 ) const int outBufMaxLength,
						  IN_BUFFER( inLength ) const void *inBuffer, 
						  IN_LENGTH_SHORT const int inLength );
STDC_NONNULL_ARG( ( 1, 3 ) ) \
void sha2HashBufferAtomic( OUT_BUFFER_C( outBufMaxLength, 32 ) BYTE *outBuffer, 
						   IN_LENGTH_SHORT_MIN( 32 ) const int outBufMaxLength,
						   IN_BUFFER( inLength ) const void *inBuffer, 
						   IN_LENGTH_SHORT const int inLength );
STDC_NONNULL_ARG( ( 1, 3 ) ) \
void sha2_ExtHashBufferAtomic( OUT_BUFFER_C( outBufMaxLength, 64 ) BYTE *outBuffer, 
							   IN_LENGTH_SHORT_MIN( 64 ) const int outBufMaxLength,
							   IN_BUFFER( inLength ) const void *inBuffer, 
							   IN_LENGTH_SHORT const int inLength );

#endif /* _CRYPTCTX_DEFINED */
