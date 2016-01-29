/****************************************************************************
*																			*
*						cryptlib PKCS #11 API Interface						*
*						Copyright Peter Gutmann 1998-2006					*
*																			*
****************************************************************************/

#ifdef USE_PKCS11

/* Before we can include the PKCS #11 headers we need to define a few OS-
   specific things that are required by the headers */

#ifdef __WINDOWS__
  #ifdef __WIN16__
	#pragma pack( 1 )					/* Struct packing */
	#define CK_PTR	far *				/* Pointer type */
	#define CK_DEFINE_FUNCTION( returnType, name ) \
								returnType __export _far _pascal name
	#define CK_DECLARE_FUNCTION( returnType, name ) \
								 returnType __export _far _pascal name
	#define CK_DECLARE_FUNCTION_POINTER( returnType, name ) \
								returnType __export _far _pascal (* name)
	#define CK_CALLBACK_FUNCTION( returnType, name ) \
								  returnType (_far _pascal * name)
  #else
	#pragma pack( push, cryptoki, 1 )	/* Struct packing */
	#define CK_PTR	*					/* Pointer type */
	#define CK_DEFINE_FUNCTION( returnType, name ) \
								returnType __declspec( dllexport ) name
	#define CK_DECLARE_FUNCTION( returnType, name ) \
								 returnType __declspec( dllimport ) name
	#define CK_DECLARE_FUNCTION_POINTER( returnType, name ) \
								returnType __declspec( dllimport ) (* name)
	#define CK_CALLBACK_FUNCTION( returnType, name ) \
								  returnType (* name)
  #endif /* Win16 vs.Win32 */
#else
  #define CK_PTR	*					/* Pointer type */
  #define CK_DEFINE_FUNCTION( returnType, name ) \
							  returnType name
  #define CK_DECLARE_FUNCTION( returnType, name ) \
							   returnType name
  #define CK_DECLARE_FUNCTION_POINTER( returnType, name ) \
									   returnType (* name)
  #define CK_CALLBACK_FUNCTION( returnType, name ) \
								returnType (* name)
#endif /* __WINDOWS__ */
#ifndef NULL_PTR
  #define NULL_PTR	NULL
#endif /* NULL_PTR */

/* Pull in the PKCS #11 headers */

#if defined( INC_ALL )
  #include "pkcs11.h"
#else
  #include "device/pkcs11.h"
#endif /* Compiler-specific includes */

/* The use of dynamically bound function pointers vs.statically linked
   functions requires a bit of sleight of hand since we can't give the
   pointers the same names as prototyped functions.  To get around this we
   redefine the actual function names to the names of the pointers */

#define C_CloseSession		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_CloseSession
#define C_CreateObject		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_CreateObject
#define C_Decrypt			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_Decrypt
#define C_DecryptInit		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_DecryptInit
#define C_DeriveKey			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_DeriveKey
#define C_DestroyObject		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_DestroyObject
#define C_Digest			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_Digest
#define C_DigestInit		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_DigestInit
#define C_Encrypt			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_Encrypt
#define C_EncryptInit		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_EncryptInit
#define C_Finalize			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_Finalize
#define C_FindObjects		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_FindObjects
#define C_FindObjectsFinal	( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_FindObjectsFinal
#define C_FindObjectsInit	( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_FindObjectsInit
#define C_GenerateKey		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_GenerateKey
#define C_GenerateKeyPair	( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_GenerateKeyPair
#define C_GenerateRandom	( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_GenerateRandom
#define C_GetAttributeValue	( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_GetAttributeValue
#define C_GetMechanismInfo	( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_GetMechanismInfo
#define C_GetInfo			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_GetInfo
#define C_GetSlotInfo		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_GetSlotInfo
#define C_GetSlotList		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_GetSlotList
#define C_GetTokenInfo		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_GetTokenInfo
#define C_Initialize		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_Initialize
#define C_InitPIN			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_InitPIN
#define C_InitToken			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_InitToken
#define C_Login				( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_Login
#define C_Logout			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_Logout
#define C_OpenSession		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_OpenSession
#define C_SetAttributeValue	( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_SetAttributeValue
#define C_SetPIN			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_SetPIN
#define C_Sign				( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_Sign
#define C_SignFinal			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_SignFinal
#define C_SignInit			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_SignInit
#define C_SignUpdate		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_SignUpdate
#define C_Verify			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_Verify
#define C_VerifyInit		( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_VerifyInit
#define C_WrapKey			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_WrapKey
#define C_UnwrapKey			( ( CK_FUNCTION_LIST_PTR )( pkcs11Info->functionListPtr ) )->C_UnwrapKey

/* Mapping of PKCS #11 device capabilities to cryptlib capabilities.  We 
   don't use the hash functions because they're always *much* faster in
   software on the host system (as are the symmetric ciphers in most 
   cases) */

typedef RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *P11_ENDFUNCTION )( INOUT CONTEXT_INFO *contextInfoPtr );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *P11_INITKEYFUNCTION )( INOUT CONTEXT_INFO *contextInfoPtr, 
									  IN_BUFFER( keyLength ) const void *key, 
									  IN_LENGTH_SHORT const int keyLength );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *P11_GENERATEKEYFUNCTION )( INOUT CONTEXT_INFO *contextInfoPtr, \
										  IN_LENGTH_SHORT const int keySizeBits );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *P11_ENCRYPTFUNCTION )( INOUT CONTEXT_INFO *contextInfoPtr, 
									  INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
									  IN_LENGTH_Z int length );
									  /* Length may be zero for hash functions */
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *P11_DECRYPTFUNCTION )( INOUT CONTEXT_INFO *contextInfoPtr, 
									  INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
									  IN_LENGTH int length );
typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
		int ( *P11_SIGNFUNCTION )( INOUT CONTEXT_INFO *contextInfoPtr, 
								   INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
								   IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) int length );

typedef struct {
	/* Mapping information.  The mechanism type is the specific mechanism for
	   this algorithm and mode, for example CKM_DES_CFB64 (= { CRYPT_ALGO_DES,
	   CRYPT_MODE_CFB }, the default mechanism is the default mechanism for 
	   the algorithm (without taking the mode into account), for example
	   CKM_DES_CBC.  The reason why we specify both is that cryptlib records
	   a single capability for each algorithm while PKCS #11 has distinct 
	   parameters for each mode of an algorithm.  By recording the most common
	   algorithm+mode combination (CBC mode), we can short-circuit having to 
	   search the PKCS #11 mechanism table for the common case where that mode
	   is being used */
	const CK_MECHANISM_TYPE mechanism;	/* Mechanism type for this algo/mode */
	const CK_MECHANISM_TYPE keygenMechanism; /* Supplementary keygen mechanism */
	const CK_MECHANISM_TYPE defaultMechanism;/* Default mechanism for this algo */
	const CK_FLAGS requiredFlags;		/* Required flags for this mechanism */
	const CRYPT_ALGO_TYPE cryptAlgo;	/* cryptlib algo and mode */
	const CRYPT_MODE_TYPE cryptMode;

	/* Equivalent PKCS #11 parameters */
	const CK_KEY_TYPE keyType;			/* PKCS #11 key type */

	/* Function pointers */
	P11_ENDFUNCTION endFunction;
	P11_INITKEYFUNCTION initKeyFunction;
	P11_GENERATEKEYFUNCTION generateKeyFunction;
	P11_ENCRYPTFUNCTION encryptFunction;
	P11_DECRYPTFUNCTION decryptFunction;
	P11_SIGNFUNCTION signFunction, sigCheckFunction;
	} PKCS11_MECHANISM_INFO;

/* Encryption contexts can store extra implementation-dependant parameters.
   The following macro maps these generic parameter names to the PKCS #11
   values */

#define paramKeyType			param1
#define paramKeyGen				param2
#define paramDefaultMech		param3

/* Special non-mechanism/object types used as an EOF markers.  Unfortunately 
   PKCS #11 uses all mechanisms down to 0 and it's an unsigned value so 
   there's no facility for a CKM_NONE value along the lines of CRYPT_ERROR, 
   and similarly object handles can also take any value, the best that we 
   can do is use CRYPT_ERROR (all ones) and hope that nothing decides to 
   return an object handle with this value.  With mechanisms we're a bit 
   safer, it'd have to be some weird vendor-defined value to match 
   CKM_NONE */

#define CKM_NONE				( ( CK_MECHANISM_TYPE ) CRYPT_ERROR )
#define CK_OBJECT_NONE			( ( CK_OBJECT_HANDLE ) CRYPT_ERROR )
#define CKA_NONE				( ( CK_ATTRIBUTE_TYPE ) CRYPT_ERROR )
#define CKF_NONE				0

/* The HMAC mechanisms in PKCS #11 don't work because they have to be keyed 
   with generic secret keys (rather than specific HMAC keys) but generic 
   secret keys can't be used with HMAC operations.  There are two possible
   workarounds for this, either ignore the restrictions on generic secret 
   keys so that they can be used with HMAC objects or define vendor-specific 
   HMAC mechanisms and keys.  The latter approach is used by nCipher, who 
   define their own CKA/CKM/CKK values based around a custom magic ID 
   value */

#define VENDOR_NCIPHER			0xDE436972UL
#define CKA_NCIPHER				( CKA_VENDOR_DEFINED | VENDOR_NCIPHER )
#define CKM_NCIPHER				( CKM_VENDOR_DEFINED | VENDOR_NCIPHER )
#define CKK_NCIPHER				( CKK_VENDOR_DEFINED | VENDOR_NCIPHER )

#ifdef NCIPHER_PKCS11
  #define CKK_MD5_HMAC			( CKK_NCIPHER + 2 )
  #define CKK_SHA_1_HMAC		( CKK_NCIPHER + 1 )
  #define CKK_RIPEMD160_HMAC	CKK_GENERIC_SECRET
  #define CKK_SHA256_HMAC		CKK_GENERIC_SECRET
  #define CKM_MD5_HMAC_KEY_GEN	( CKM_NCIPHER + 6 )
  #define CKM_SHA_1_HMAC_KEY_GEN ( CKM_NCIPHER + 3 )
  #define CKM_RIPEMD160_HMAC_KEY_GEN CKK_GENERIC_SECRET
  #define CKM_SHA256_HMAC_KEY_GEN CKK_GENERIC_SECRET
#else
  #define CKK_MD5_HMAC			CKK_GENERIC_SECRET
  #define CKK_SHA_1_HMAC		CKK_GENERIC_SECRET
  #define CKK_RIPEMD160_HMAC	CKK_GENERIC_SECRET
  #define CKK_SHA256_HMAC		CKK_GENERIC_SECRET
  #define CKM_MD5_HMAC_KEY_GEN	CKM_GENERIC_SECRET_KEY_GEN
  #define CKM_SHA_1_HMAC_KEY_GEN CKM_GENERIC_SECRET_KEY_GEN
  #define CKM_RIPEMD160_HMAC_KEY_GEN CKK_GENERIC_SECRET
  #define CKM_SHA256_HMAC_KEY_GEN CKM_GENERIC_SECRET_KEY_GEN
#endif /* NCIPHER_PKCS11 */

/* Prototypes for functions in pkcs11.c */

CHECK_RETVAL \
int pkcs11MapError( const CK_RV errorCode,
					IN_STATUS const int defaultError );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int getContextDeviceInfo( IN_HANDLE const CRYPT_HANDLE iCryptContext,
						  OUT_HANDLE_OPT CRYPT_DEVICE *iCryptDevice, 
						  OUT_PTR_COND PKCS11_INFO **pkcs11InfoPtrPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
time_t getTokenTime( const CK_TOKEN_INFO *tokenInfo );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const PKCS11_MECHANISM_INFO *getMechanismInfoConv( OUT_LENGTH_SHORT int *mechanismInfoSize );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int genericEndFunction( INOUT CONTEXT_INFO *contextInfoPtr );

/* Prototypes for functions in pkcs11_init.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initPKCS11Init( INOUT DEVICE_INFO *deviceInfo, 
					IN_BUFFER( nameLength ) const char *name, 
					IN_LENGTH_SHORT const int nameLength );

/* Prototypes for functions in pkcs11_pkc.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int rsaSetPublicComponents( INOUT PKCS11_INFO *pkcs11Info,
							IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							const CK_OBJECT_HANDLE hRsaKey,
							const BOOLEAN nativeContext );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int dsaSetPublicComponents( INOUT PKCS11_INFO *pkcs11Info,
							IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							const CK_OBJECT_HANDLE hDsaKey,
							const BOOLEAN nativeContext );
#if defined( USE_ECDSA )
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int ecdsaSetPublicComponents( INOUT PKCS11_INFO *pkcs11Info,
							  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							  const CK_OBJECT_HANDLE hEcdsaKey,
							  const BOOLEAN nativeContext );
#endif /* USE_ECDSA */
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const PKCS11_MECHANISM_INFO *getMechanismInfoPKC( OUT int *mechanismInfoSize );

/* Prototypes for functions in pkcs11_rd/wr.c */

CHECK_RETVAL_RANGE( ACTION_PERM_FLAG_NONE, ACTION_PERM_FLAG_MAX ) STDC_NONNULL_ARG( ( 1 ) ) \
int getActionFlags( INOUT PKCS11_INFO *pkcs11Info,
					const CK_OBJECT_HANDLE hObject,
					IN_ENUM( KEYMGMT_ITEM ) const KEYMGMT_ITEM_TYPE itemType,
					IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int addIAndSToTemplate( INOUT_ARRAY_C( 2 ) CK_ATTRIBUTE *certTemplate, 
						IN_BUFFER( iAndSLength ) const void *iAndSPtr, 
						IN_LENGTH_SHORT const int iAndSLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getEccCurveType( INOUT PKCS11_INFO *pkcs11Info, 
					 const CK_OBJECT_HANDLE hObject,
					 OUT_ENUM_OPT( CRYPT_ECCCURVE ) \
						CRYPT_ECCCURVE_TYPE *curveType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int findObject( INOUT PKCS11_INFO *pkcs11Info, 
				OUT CK_OBJECT_HANDLE *hObject,
				IN_ARRAY( templateCount ) \
					const CK_ATTRIBUTE *objectTemplate,
				IN_RANGE( 1, 64 ) const CK_ULONG templateCount );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int findObjectEx( INOUT PKCS11_INFO *pkcs11Info, 
				  OUT CK_OBJECT_HANDLE *hObject,
				  IN_ARRAY( templateCount ) \
					const CK_ATTRIBUTE *objectTemplate,
				  IN_RANGE( 1, 64 ) const CK_ULONG templateCount );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int findObjectFromObject( INOUT PKCS11_INFO *pkcs11Info,
						  const CK_OBJECT_HANDLE hSourceObject, 
						  const CK_OBJECT_CLASS objectClass,
						  OUT CK_OBJECT_HANDLE *hObject );
void initPKCS11Read( INOUT DEVICE_INFO *deviceInfo ) \
					 STDC_NONNULL_ARG( ( 1 ) );
void initPKCS11Write( INOUT DEVICE_INFO *deviceInfo ) \
					  STDC_NONNULL_ARG( ( 1 ) );

#endif /* USE_PKCS11 */
