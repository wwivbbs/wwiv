/****************************************************************************
*																			*
*					cryptlib Encryption Capability Header File 				*
*						Copyright Peter Gutmann 1992-2005					*
*																			*
****************************************************************************/

#ifndef _CRYPTCAP_DEFINED

#define _CRYPTCAP_DEFINED

/* The information processed by the initParams() function */

typedef enum {
	KEYPARAM_NONE,			/* No crypto paramter type */
	KEYPARAM_MODE,			/* Encryption mode */
	KEYPARAM_IV,			/* Initialisation vector */
	KEYPARAM_BLOCKSIZE,		/* Hash/MAC output size */
	KEYPARAM_AAD,			/* AAD for authenticated-encr.modes */
	KEYPARAM_LAST			/* Last possible crypto parameter type */
	} KEYPARAM_TYPE;

/* The CONTEXT_INFO structure is only visible inside modules that have access
   to context internals, if we use it anywhere else we just treat it as a
   generic void *.  In addition since the CONTEXT_INFO contains the 
   capability info struct, it can't be declared yet at this point so we have 
   to provide a forward declaration for it */

#ifdef _CRYPTCTX_DEFINED
  struct CI;
  #define CI_STRUCT		struct CI
#else
  #define CI_STRUCT		void
#endif /* _CRYPTCTX_DEFINED */

/* The information returned by the capability get-info function */

typedef enum {
	CAPABILITY_INFO_NONE,			/* No info */
	CAPABILITY_INFO_STATESIZE,		/* Size of algorithm state info */
	CAPABILITY_INFO_STATEALIGNTYPE,	/* Alignment requirements for state info */
	CAPABILITY_INFO_ICV,			/* ICV for authenticated-encr.modes */
	CAPABILITY_INFO_LAST			/* Last possible capability info type */
	} CAPABILITY_INFO_TYPE;

/****************************************************************************
*																			*
*								Data Structures								*
*																			*
****************************************************************************/

/* The structure used to store information about the crypto capabilities */

typedef struct CA {
	/* Basic identification information for the algorithm */
	const CRYPT_ALGO_TYPE cryptAlgo;/* The encryption algorithm */
	const int blockSize;			/* The basic block size of the algorithm */
	BUFFER_FIXED( algoNameLen ) \
	const char *algoName;			/* Algorithm name */
	const int algoNameLen;			/* Algorithm name length */

	/* Keying information.  Note that the maximum sizes may vary (for
	   example for two-key triple DES vs.three-key triple DES) so the
	   crypt query functions should be used to determine the actual size
	   for a particular context rather than just using maxKeySize */
	const int minKeySize;			/* Minimum key size in bytes */
	const int keySize;				/* Recommended key size in bytes */
	const int maxKeySize;			/* Maximum key size in bytes */

	/* The functions for implementing the algorithm */
	CHECK_RETVAL_FNPTR \
	int ( *selfTestFunction )( void );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 3 ) ) \
	int ( *getInfoFunction )( IN_ENUM( CAPABILITY_INFO ) \
								const CAPABILITY_INFO_TYPE type, 
							  INOUT_OPT CI_STRUCT *contextInfoPtr, 
							  OUT void *data, 
							  IN_INT_Z const int length );
	STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *endFunction )( INOUT CI_STRUCT *contextInfoPtr );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *initParamsFunction) ( INOUT CI_STRUCT *contextInfoPtr, 
								 IN_ENUM( KEYPARAM ) \
									const KEYPARAM_TYPE paramType,
								 IN_OPT const void *data, 
								 IN_INT const int dataLength );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *initKeyFunction )( INOUT CI_STRUCT *contextInfoPtr, 
							  IN_BUFFER_OPT( keyLength ) const void *key, 
							  IN_LENGTH_SHORT_Z const int keyLength );
							  /* The key data can be NULL if it's a PKC 
							     context whose key components have been read 
								 directly into the context, which is also 
								 why we can't use IN_LENGTH_KEY for the 
								 length */
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *generateKeyFunction )( INOUT CI_STRUCT *contextInfoPtr, \
								  IN_RANGE( bytesToBits( MIN_KEYSIZE ),
											bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
									const int keySizeBits );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptFunction )( INOUT CI_STRUCT *contextInfoPtr, 
							  INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							  IN_LENGTH_Z int length );
							  /* Length may be zero for hash functions */
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptFunction )( INOUT CI_STRUCT *contextInfoPtr, 
							  INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							  IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptCBCFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptCBCFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptCFBFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptCFBFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptOFBFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptOFBFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptGCMFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptGCMFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *signFunction )( INOUT CI_STRUCT *contextInfoPtr, 
						   INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
						   IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *sigCheckFunction )( INOUT CI_STRUCT *contextInfoPtr, 
							   INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							   IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) int length );

	/* Non-native implementations may require extra parameters (for example
	   to specify the algorithm and mode in the manner required by the
	   non-native implementation), the following values can be used to store
	   these parameters */
	const int param1, param2, param3, param4;
	} CAPABILITY_INFO;

/* An encapsulating list type for the list of capabilities */

typedef struct CL {
	const CAPABILITY_INFO *info;
	struct CL *next;
	}  CAPABILITY_INFO_LIST;

/* Since cryptlib's CAPABILITY_INFO is fixed, all of the fields are declared
   const so that they'll be allocated in the code segment.  This doesn't quite 
   work for some types of crypto devices since things like the available key 
   lengths can vary depending on the underlying hardware or software, so we 
   provide an equivalent structure that makes the variable fields non-const.  
   Once the fields are set up, the result is copied into a dynamically-
   allocated CAPABILITY_INFO block at which point the fields are treated as 
   const by the code */

typedef struct {
	const CRYPT_ALGO_TYPE cryptAlgo;
	const int blockSize;
	BUFFER_FIXED( algoNameLen ) \
	const char *algoName;
	const int algoNameLen;

	int minKeySize;						/* Non-const */
	int keySize;						/* Non-const */
	int maxKeySize;						/* Non-const */

	CHECK_RETVAL_FNPTR \
	int ( *selfTestFunction )( void );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 3 ) ) \
	int ( *getInfoFunction )( IN_ENUM( CAPABILITY_INFO ) \
								const CAPABILITY_INFO_TYPE type, 
							  INOUT_OPT CI_STRUCT *contextInfoPtr, 
							  OUT void *data, 
							  IN_INT_Z const int length );
	STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *endFunction )( INOUT CI_STRUCT *contextInfoPtr );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *initParamsFunction) ( INOUT CI_STRUCT *contextInfoPtr, 
								 IN_ENUM( KEYPARAM ) \
									const KEYPARAM_TYPE paramType,
								 IN_OPT const void *data, 
								 IN_INT const int dataLength );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *initKeyFunction )( INOUT CI_STRUCT *contextInfoPtr, 
							  IN_BUFFER_OPT( keyLength ) const void *key, 
							  IN_LENGTH_SHORT_Z const int keyLength );
							  /* The key data can be NULL if it's a PKC 
							     context whose key components have been read 
								 directly into the context */
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
	int ( *generateKeyFunction )( INOUT CI_STRUCT *contextInfoPtr, \
								  IN_LENGTH_SHORT const int keySizeBits );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptFunction )( INOUT CI_STRUCT *contextInfoPtr, 
							  INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							  IN_LENGTH_Z int length );
							  /* Length may be zero for hash functions */
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptFunction )( INOUT CI_STRUCT *contextInfoPtr, 
							  INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							  IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptCBCFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptCBCFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptCFBFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptCFBFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptOFBFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptOFBFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *encryptGCMFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *decryptGCMFunction )( INOUT CI_STRUCT *contextInfoPtr, 
								 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								 IN_LENGTH int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *signFunction )( INOUT CI_STRUCT *contextInfoPtr, 
						   INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
						   IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) int length );
	CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int ( *sigCheckFunction )( INOUT CI_STRUCT *contextInfoPtr, 
							   INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							   IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) int length );

	int param1, param2, param3, param4;	/* Non-const */
	} VARIABLE_CAPABILITY_INFO;

/****************************************************************************
*																			*
*								Internal API Functions						*
*																			*
****************************************************************************/

/* Prototypes for capability access functions */

typedef const CAPABILITY_INFO * ( *GETCAPABILITY_FUNCTION )( void );

const CAPABILITY_INFO *get3DESCapability( void );
const CAPABILITY_INFO *getAESCapability( void );
const CAPABILITY_INFO *getBlowfishCapability( void );
const CAPABILITY_INFO *getCASTCapability( void );
const CAPABILITY_INFO *getDESCapability( void );
const CAPABILITY_INFO *getIDEACapability( void );
const CAPABILITY_INFO *getRC2Capability( void );
const CAPABILITY_INFO *getRC4Capability( void );
const CAPABILITY_INFO *getRC5Capability( void );

const CAPABILITY_INFO *getMD5Capability( void );
const CAPABILITY_INFO *getRipemd160Capability( void );
const CAPABILITY_INFO *getSHA1Capability( void );
const CAPABILITY_INFO *getSHA2Capability( void );

const CAPABILITY_INFO *getHmacMD5Capability( void );
const CAPABILITY_INFO *getHmacRipemd160Capability( void );
const CAPABILITY_INFO *getHmacSHA1Capability( void );
const CAPABILITY_INFO *getHmacSHA2Capability( void );

const CAPABILITY_INFO *getDHCapability( void );
const CAPABILITY_INFO *getDSACapability( void );
const CAPABILITY_INFO *getElgamalCapability( void );
const CAPABILITY_INFO *getRSACapability( void );
const CAPABILITY_INFO *getECDSACapability( void );
const CAPABILITY_INFO *getECDHCapability( void );

const CAPABILITY_INFO *getGenericSecretCapability( void );

/* Prototypes for functions in cryptctx.c, used by devices to create native 
   contexts */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int createContext( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				   IN const void *auxDataPtr, 
				   IN_FLAGS_Z( CREATEOBJECT ) const int auxValue );

/* Prototypes for capability functions in context/ctx_misc.c */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const CAPABILITY_INFO FAR_BSS *findCapabilityInfo(
						const CAPABILITY_INFO_LIST *capabilityInfoList,
						IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void getCapabilityInfo( OUT CRYPT_QUERY_INFO *cryptQueryInfo,
						const CAPABILITY_INFO FAR_BSS *capabilityInfoPtr );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckCapability( const CAPABILITY_INFO *capabilityInfoPtr,
							   const BOOLEAN asymmetricOK );
		/* The asymmetricOK flag indicates that the capabilities can have 
		   asymmetric functionality, for example sign is supported but sig.
		   check isn't (this is required for some tinkertoy implementations 
		   in crypto tokens which support bare-minimum functionality such as 
		   RSA private-key ops and nothing else) */

/* Fallback functions to handle context-specific information that isn't 
   specific to a particular context.  The initial request goes to the 
   context, if that doesn't want to handle it it passes it on to the default 
   handler */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int getDefaultInfo( IN_ENUM( CAPABILITY_INFO ) const CAPABILITY_INFO_TYPE type, 
					INOUT_OPT CI_STRUCT *contextInfoPtr,
					OUT void *data, 
					IN_INT_Z const int length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initGenericParams( INOUT CI_STRUCT *contextInfoPtr, 
					   IN_ENUM( KEYPARAM ) const KEYPARAM_TYPE paramType,
					   IN_OPT const void *data, 
					   IN_INT const int dataLength );

#endif /* _CRYPTCAP_DEFINED */
