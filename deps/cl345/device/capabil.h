/****************************************************************************
*																			*
*					cryptlib Encryption Capability Header File 				*
*						Copyright Peter Gutmann 1992-2015					*
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

typedef CHECK_RETVAL \
		int ( *CAP_SELFTEST_FUNCTION )( void );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
		int ( *CAP_GETINFO_FUNCTION )( IN_ENUM( CAPABILITY_INFO ) \
										const CAPABILITY_INFO_TYPE type, 
									   INOUT_OPT CI_STRUCT *contextInfoPtr, 
									   OUT void *data, 
									   IN_INT_Z const int length );
typedef RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *CAP_END_FUNCTION )( INOUT CI_STRUCT *contextInfoPtr );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *CAP_INITPARAMS_FUNCTION )( INOUT CI_STRUCT *contextInfoPtr, 
										  IN_ENUM( KEYPARAM ) \
											const KEYPARAM_TYPE paramType,
										  IN_OPT const void *data, 
										  IN_INT const int dataLength );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *CAP_INITKEY_FUNCTION )( INOUT CI_STRUCT *contextInfoPtr, 
									   IN_BUFFER_OPT( keyLength ) const void *key, 
									   IN_LENGTH_SHORT_Z const int keyLength );
									   /* The key data can be NULL if it's a PKC 
										  context whose key components have been 
										  read directly into the context, which 
										  is also why we can't use IN_LENGTH_KEY 
										  for the length */
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *CAP_GENERATEKEY_FUNCTION )( INOUT CI_STRUCT *contextInfoPtr, \
										   IN_RANGE( bytesToBits( MIN_KEYSIZE ),
													bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
												const int keySizeBits );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *CAP_ENCRYPTSPECIAL_FUNCTION )( INOUT CI_STRUCT *contextInfoPtr, 
											  INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
											  IN_LENGTH_Z int length );
											  /* Length may be zero for hash functions */
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *CAP_ENCRYPT_FUNCTION )( INOUT CI_STRUCT *contextInfoPtr, 
									   INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
									   IN_LENGTH int length );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *CAP_SIGN_FUNCTION )( INOUT CI_STRUCT *contextInfoPtr, 
								    INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								    IN_LENGTH_SHORT_MIN( 32 ) int length );

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
	CAP_SELFTEST_FUNCTION selfTestFunction;
	CAP_GETINFO_FUNCTION getInfoFunction;
	CAP_END_FUNCTION endFunction;
	CAP_INITPARAMS_FUNCTION initParamsFunction;
	CAP_INITKEY_FUNCTION initKeyFunction;
	CAP_GENERATEKEY_FUNCTION generateKeyFunction;
	CAP_ENCRYPTSPECIAL_FUNCTION encryptFunction;
	CAP_ENCRYPT_FUNCTION decryptFunction;
	CAP_ENCRYPT_FUNCTION encryptCBCFunction, decryptCBCFunction;
	CAP_ENCRYPT_FUNCTION encryptCFBFunction, decryptCFBFunction;
	CAP_ENCRYPT_FUNCTION encryptGCMFunction, decryptGCMFunction;
	CAP_SIGN_FUNCTION signFunction, sigCheckFunction;

	/* Non-native implementations may require extra parameters (for example
	   to specify the algorithm and mode in the manner required by the
	   non-native implementation), the following values can be used to store
	   these parameters */
	const int param1, param2, param3, param4;
	} CAPABILITY_INFO;

/* An encapsulating list type for the list of capabilities.  This is a 
   distinct structure that's used to build a linked list of the read-only 
   capability info structures */

typedef struct CL {
	DATAPTR info;
	DATAPTR next;
	} CAPABILITY_INFO_LIST;

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

	CAP_SELFTEST_FUNCTION selfTestFunction;
	CAP_GETINFO_FUNCTION getInfoFunction;
	CAP_END_FUNCTION endFunction;
	CAP_INITPARAMS_FUNCTION initParamsFunction;
	CAP_INITKEY_FUNCTION initKeyFunction;
	CAP_GENERATEKEY_FUNCTION generateKeyFunction;
	CAP_ENCRYPTSPECIAL_FUNCTION encryptFunction;
	CAP_ENCRYPT_FUNCTION decryptFunction;
	CAP_ENCRYPT_FUNCTION encryptCBCFunction, decryptCBCFunction;
	CAP_ENCRYPT_FUNCTION encryptCFBFunction, decryptCFBFunction;
	CAP_ENCRYPT_FUNCTION encryptGCMFunction, decryptGCMFunction;
	CAP_SIGN_FUNCTION signFunction, sigCheckFunction;

	int param1, param2, param3, param4;	/* Non-const */
	} VARIABLE_CAPABILITY_INFO;

/****************************************************************************
*																			*
*								Internal API Functions						*
*																			*
****************************************************************************/

/* Prototypes for capability access functions */

typedef const CAPABILITY_INFO * ( *GETCAPABILITY_FUNCTION )( void );

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *get3DESCapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getAESCapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getCASTCapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getDESCapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getIDEACapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getRC2Capability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getRC4Capability( void );

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getMD5Capability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getSHA1Capability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getSHA2Capability( void );

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getHmacMD5Capability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getHmacSHA1Capability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getHmacSHA2Capability( void );

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getDHCapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getDSACapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getElgamalCapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getRSACapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getECDSACapability( void );
CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getECDHCapability( void );

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getGenericSecretCapability( void );

/* Prototypes for functions in cryptctx.c, used by devices to create native 
   contexts */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int createContext( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				   IN const void *auxDataPtr, 
				   IN_FLAGS_Z( CREATEOBJECT ) const int auxValue );

/* Prototypes for capability functions in context/ctx_misc.c */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const CAPABILITY_INFO *findCapabilityInfo(
						const CAPABILITY_INFO_LIST *capabilityInfoList,
						IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void getCapabilityInfo( OUT CRYPT_QUERY_INFO *cryptQueryInfo,
						const CAPABILITY_INFO *capabilityInfoPtr );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckCapability( const CAPABILITY_INFO *capabilityInfoPtr );

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
