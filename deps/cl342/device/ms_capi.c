/****************************************************************************
*																			*
*							cryptlib CryptoAPI Routines						*
*						Copyright Peter Gutmann 1998-2006					*
*																			*
****************************************************************************/

/* The following code is purely a test framework used to test the ability to
   work with CryptoAPI keys.  Much of the code is only present as a rough
   sketch.  It's not part of cryptlib, and shouldn't be used as a cryptlib
   component */

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "device.h"
  #include "dev_mech.h"
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "device/device.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "mechs/dev_mech.h"
#endif /* Compiler-specific includes */

/* The size of the (packed) header used for key blobs */

#define BLOBHEADER_SIZE			8

#ifdef USE_CRYPTOAPI

#if defined( _MSC_VER )
  #pragma message( "  Building with CAPI device interface enabled." )
#endif /* Warn with VC++ */

/* The following define is needed to enable crypto functions in the include
   file.  This would probably be defined by the compiler since it's not 
   defined in any header file, but it doesn't seem to be enabled by 
   default */

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT	0x0500
#endif /* _WIN32_WINNT */

/* cryptlib.h includes a trap for inclusion of wincrypt.h before cryptlib.h
   which results in a compiler error if both files are included.  To disable 
   this, we need to undefine the CRYPT_MODE_ECB defined in cryptlib.h */

#undef CRYPT_MODE_ECB

#include <wincrypt.h>

/* CryptoAPI uses the same mode names as cryptlib but different values, 
   fortunately this is done with #defines so we can remove them at this
   point */

#undef CRYPT_MODE_ECB
#undef CRYPT_MODE_CBC
#undef CRYPT_MODE_CFB
#undef CRYPT_MODE_CTR

/* Symbolic defines to represent non-initialised values */

#define CALG_NONE			0
#define HCRYPTPROV_NONE		0

/* Some parts of CryptoAPI (inconsistently) require the use of Unicode,
   winnls.h was already included via the global include of windows.h however
   it isn't needed for any other part of cryptlib so it was disabled via
   NONLS.  Since winnls.h is now locked out, we have to un-define the guards
   used earlier to get it included */

#undef _WINNLS_
#undef NONLS
#include <winnls.h>

/* Older versions of wincrypt.h don't contain defines and typedefs that we
   require.  Defines can be detected with #ifdef but typedefs can't, to 
   handle this we rely on checking for values that aren't defined in older 
   versions of wincrypt.h, which only go up to KP_PUB_EX_VAL */

#ifndef KP_ADMIN_PIN
  /* Misc values */
  #define HCERTCHAINENGINE				void *
  #define USAGE_MATCH_TYPE_AND			0
  #define USAGE_MATCH_TYPE_OR			1
  #define CERT_COMPARE_KEY_IDENTIFIER	15
  #define CERT_FIND_KEY_IDENTIFIER		( CERT_COMPARE_KEY_IDENTIFIER << CERT_COMPARE_SHIFT )
  #define CERT_CHAIN_CACHE_END_CERT					0x00000001
  #define CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY	0x80000000
  #define CRYPT_ACQUIRE_CACHE_FLAG					0x00000001

  /* Certificate chain match information */
  typedef struct {
	DWORD dwType;
	CERT_ENHKEY_USAGE Usage;
	} CERT_USAGE_MATCH;
  typedef struct {
	DWORD cbSize;
	CERT_USAGE_MATCH RequestedUsage;
	CERT_USAGE_MATCH RequestedIssuancePolicy;
	DWORD dwUrlRetrievalTimeout;
	BOOL fCheckRevocationFreshnessTime;
	DWORD dwRevocationFreshnessTime;
	} CERT_CHAIN_PARA, *PCERT_CHAIN_PARA;

  /* Certificate chain information */
  typedef struct {
	DWORD dwErrorStatus;
	DWORD dwInfoStatus;
	} CERT_TRUST_STATUS, *PCERT_TRUST_STATUS;
  typedef struct {
	DWORD cbSize;
	PCCERT_CONTEXT pCertContext;
	CERT_TRUST_STATUS TrustStatus;
	void *pRevocationInfo;		// PCERT_REVOCATION_INFO pRevocationInfo;
	void *pIssuanceUsage;		// PCERT_ENHKEY_USAGE pIssuanceUsage;
	void *pApplicationUsage;	// PCERT_ENHKEY_USAGE pApplicationUsage;
	LPCWSTR pwszExtendedErrorInfo;
	} CERT_CHAIN_ELEMENT, *PCERT_CHAIN_ELEMENT;
  typedef struct {
	DWORD cbSize;
	PCTL_ENTRY pCtlEntry;
	PCCTL_CONTEXT pCtlContext;
	} CERT_TRUST_LIST_INFO, *PCERT_TRUST_LIST_INFO;
  typedef struct {
	DWORD cbSize;
	CERT_TRUST_STATUS TrustStatus;
	DWORD cElement;
	PCERT_CHAIN_ELEMENT *rgpElement;
	PCERT_TRUST_LIST_INFO pTrustListInfo;
	BOOL fHasRevocationFreshnessTime;
	DWORD dwRevocationFreshnessTime;
	} CERT_SIMPLE_CHAIN, *PCERT_SIMPLE_CHAIN;
  typedef struct CCC {
	DWORD cbSize;
	CERT_TRUST_STATUS TrustStatus;
	DWORD cChain;
	PCERT_SIMPLE_CHAIN *rgpChain;
	DWORD cLowerQualityChainContext;
	struct CCC **rgpLowerQualityChainContext;
	BOOL fHasRevocationFreshnessTime;
	DWORD dwRevocationFreshnessTime;
	} CERT_CHAIN_CONTEXT;
  typedef const CERT_CHAIN_CONTEXT *PCCERT_CHAIN_CONTEXT;
#endif /* Pre-1999 wincrypt.h */

/****************************************************************************
*																			*
*						 		Init/Shutdown Routines						*
*																			*
****************************************************************************/

/* Global function pointers.  These are necessary because the functions need
   to be dynamically linked since not all systems contain the necessary
   DLL's.  Explicitly linking to them will make cryptlib unloadable on some
   systems */

#define NULL_HINSTANCE	( HINSTANCE ) NULL

static HINSTANCE hCryptoAPI = NULL_HINSTANCE;
static HINSTANCE hAdvAPI32 = NULL_HINSTANCE;

typedef BOOL ( WINAPI *CERTADDCERTIFICATETOSTORE )( HCERTSTORE hCertStore,
					PCCERT_CONTEXT pCertContext, DWORD dwAddDisposition,
					PCCERT_CONTEXT *ppStoreContext );
typedef BOOL ( WINAPI *CERTADDENCODEDCERTIFICATETOSTORE )( HCERTSTORE hCertStore,
					DWORD dwCertEncodingType, const BYTE *pbCertEncoded, 
					DWORD cbCertEncoded, DWORD dwAddDisposition, 
					PCCERT_CONTEXT *ppCertContext );
typedef BOOL ( WINAPI *CERTCLOSESTORE )( HCERTSTORE hCertStore, DWORD dwFlags );
typedef PCCERT_CONTEXT ( WINAPI *CERTCREATECERTIFICATECONTEXT )( DWORD dwCertEncodingType,
					const BYTE *pbCertEncoded, DWORD cbCertEncoded );
typedef BOOL ( WINAPI *CERTDELETECERTIFICATEFROMSTORE )( PCCERT_CONTEXT pCertContext );
typedef PCCERT_CONTEXT ( WINAPI *CERTFINDCERTIFICATEINSTORE )( HCERTSTORE hCertStore,
					DWORD dwCertEncodingType, DWORD dwFindFlags, 
					DWORD dwFindType, const void *pvFindPara, 
					PCCERT_CONTEXT pPrevCertContext );
typedef VOID ( WINAPI *CERTFREECERTIFICATECHAIN )( PCCERT_CHAIN_CONTEXT pChainContext );
typedef BOOL ( WINAPI *CERTFREECERTIFICATECONTEXT )( PCCERT_CONTEXT pCertContext );
typedef BOOL ( WINAPI *CERTGETCERTIFICATECHAIN )( HCERTCHAINENGINE hChainEngine,
					PCCERT_CONTEXT pCertContext, LPFILETIME pTime, 
					HCERTSTORE hAdditionalStore, PCERT_CHAIN_PARA pChainPara,
					DWORD dwFlags, LPVOID pvReserved, 
					PCCERT_CHAIN_CONTEXT *ppChainContext );
typedef BOOL ( WINAPI *CERTGETCERTIFICATECONTEXTPROPERTY )( PCCERT_CONTEXT pCertContext,
					DWORD dwPropId, void *pvData, DWORD *pcbData );
typedef PCCERT_CONTEXT ( WINAPI *CERTGETSUBJECTCERTIFICATEFROMSTORE )( HCERTSTORE hCertStore,
					DWORD dwCertEncodingType, PCERT_INFO pCertId );
typedef BOOL ( WINAPI *CERTSETCERTIFICATEPROPERTY )( PCCERT_CONTEXT pCertContext,
					DWORD dwPropId, DWORD dwFlags, const void *pvData );
typedef HCERTSTORE ( WINAPI *CERTOPENSYSTEMSTORE )( HCRYPTPROV hprov,
					LPCSTR szSubsystemProtocol );

typedef BOOL ( WINAPI *CRYPTACQUIRECERTIFICATEPRIVATEKEY )( PCCERT_CONTEXT pCert,
					DWORD dwFlags, void *pvReserved, HCRYPTPROV *phCryptProv, 
					DWORD *pdwKeySpec, BOOL *pfCallerFreeProv );
typedef BOOL ( WINAPI *CRYPTACQUIRECONTEXTA )( HCRYPTPROV *phProv, LPCSTR pszContainer,
					LPCSTR pszProvider, DWORD dwProvType, DWORD dwFlags );
typedef BOOL ( WINAPI *CRYPTCREATEHASH )( HCRYPTPROV hProv, ALG_ID Algid, 
					HCRYPTKEY hKey, DWORD dwFlags, HCRYPTHASH* phHash );
typedef BOOL ( WINAPI *CRYPTDECRYPT )( HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
					DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen );
typedef BOOL ( WINAPI *CRYPTDESTROYHASH )( HCRYPTHASH hHash );
typedef BOOL ( WINAPI *CRYPTDESTROYKEY )( HCRYPTKEY hKey );
typedef BOOL ( WINAPI *CRYPTENCRYPT )( HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
					DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen, DWORD dwBufLen );
typedef BOOL ( WINAPI *CRYPTEXPORTKEY )( HCRYPTKEY hKey, HCRYPTKEY hExpKey,
					DWORD dwBlobType, DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen );
typedef BOOL ( WINAPI *CRYPTFINDCERTIFICATEKEYPROVINFO )( PCCERT_CONTEXT pCert,
					DWORD dwFlags, void *pvReserved );
typedef BOOL ( WINAPI *CRYPTGENKEY )( HCRYPTPROV hProv, ALG_ID Algid, DWORD dwFlags,
					HCRYPTKEY *phKey );
typedef BOOL ( WINAPI *CRYPTGENRANDOM )( HCRYPTPROV hProv, DWORD dwLen, BYTE *pbBuffer );
typedef BOOL ( WINAPI *CRYPTGETKEYPARAM )( HCRYPTKEY hKey, DWORD dwParam, BYTE* pbData,
					DWORD* pdwDataLen, DWORD dwFlags );
typedef BOOL ( WINAPI *CRYPTGETPROVPARAM )( HCRYPTPROV hProv, DWORD dwParam, 
					BYTE *pbData, DWORD *pdwDataLen, DWORD dwFlags );
typedef BOOL ( WINAPI *CRYPTGETUSERKEY )( HCRYPTPROV hProv, DWORD dwKeySpec, 
					HCRYPTKEY* phUserKey );
typedef BOOL ( WINAPI *CRYPTHASHDATA )( HCRYPTHASH hHash, BYTE *pbData, DWORD dwDataLen,
					DWORD dwFlags );
typedef BOOL ( WINAPI *CRYPTIMPORTKEY )( HCRYPTPROV hProv, CONST BYTE *pbData,
					DWORD dwDataLen, HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY *phKey );
typedef BOOL ( WINAPI *CRYPTRELEASECONTEXT )( HCRYPTPROV hProv, DWORD dwFlags );
typedef BOOL ( WINAPI *CRYPTSETHASHPARAM )( HCRYPTHASH hHash, DWORD dwParam, 
					BYTE* pbData, DWORD dwFlags );
typedef BOOL ( WINAPI *CRYPTSETKEYPARAM )( HCRYPTKEY hKey, DWORD dwParam, 
					BYTE *pbData, DWORD dwFlags );
typedef BOOL ( WINAPI *CRYPTSIGNHASH )( HCRYPTHASH hHash, DWORD dwKeySpec, 
					LPCTSTR sDescription, DWORD dwFlags, BYTE* pbSignature, 
					DWORD* pdwSigLen );

static CERTADDCERTIFICATETOSTORE pCertAddCertificateContextToStore = NULL;
static CERTADDENCODEDCERTIFICATETOSTORE pCertAddEncodedCertificateToStore = NULL;
static CERTCREATECERTIFICATECONTEXT pCertCreateCertificateContext = NULL;
static CERTDELETECERTIFICATEFROMSTORE pCertDeleteCertificateFromStore = NULL;
static CERTCLOSESTORE pCertCloseStore = NULL;
static CERTFINDCERTIFICATEINSTORE pCertFindCertificateInStore = NULL;
static CERTFREECERTIFICATECHAIN pCertFreeCertificateChain = NULL;
static CERTFREECERTIFICATECONTEXT pCertFreeCertificateContext = NULL;
static CERTGETCERTIFICATECHAIN pCertGetCertificateChain = NULL;
static CERTGETCERTIFICATECONTEXTPROPERTY pCertGetCertificateContextProperty = NULL;
static CERTGETSUBJECTCERTIFICATEFROMSTORE pCertGetSubjectCertificateFromStore = NULL;
static CERTSETCERTIFICATEPROPERTY pCertSetCertificateContextProperty = NULL;
static CERTOPENSYSTEMSTORE pCertOpenSystemStore = NULL;

static CRYPTACQUIRECERTIFICATEPRIVATEKEY pCryptAcquireCertificatePrivateKey = NULL;
static CRYPTACQUIRECONTEXTA pCryptAcquireContextA = NULL;
static CRYPTCREATEHASH pCryptCreateHash = NULL;
static CRYPTDECRYPT pCryptDecrypt = NULL;
static CRYPTDESTROYHASH pCryptDestroyHash = NULL;
static CRYPTDESTROYKEY pCryptDestroyKey = NULL;
static CRYPTENCRYPT pCryptEncrypt = NULL;
static CRYPTEXPORTKEY pCryptExportKey = NULL;
static CRYPTFINDCERTIFICATEKEYPROVINFO pCryptFindCertificateKeyProvInfo = NULL;
static CRYPTGENKEY pCryptGenKey = NULL;
static CRYPTGENRANDOM pCryptGenRandom = NULL;
static CRYPTGETKEYPARAM pCryptGetKeyParam = NULL;
static CRYPTGETPROVPARAM pCryptGetProvParam = NULL;
static CRYPTGETUSERKEY pCryptGetUserKey = NULL;
static CRYPTHASHDATA pCryptHashData = NULL;
static CRYPTIMPORTKEY pCryptImportKey = NULL;
static CRYPTRELEASECONTEXT pCryptReleaseContext = NULL;
static CRYPTSETHASHPARAM pCryptSetHashParam = NULL;
static CRYPTSETKEYPARAM pCryptSetKeyParam = NULL;
static CRYPTSIGNHASH pCryptSignHash = NULL;

/* Dynamically load and unload any necessary DBMS libraries */

int deviceInitCryptoAPI( void )
	{
	/* If the CryptoAPI module is already linked in, don't do anything */
	if( hCryptoAPI != NULL_HINSTANCE )
		return( CRYPT_OK );

	/* Obtain handles to the modules containing the CryptoAPI functions */
	if( ( hAdvAPI32 = GetModuleHandle( "AdvAPI32.DLL" ) ) == NULL )
		return( CRYPT_ERROR );
	if( ( hCryptoAPI = DynamicLoad( "Crypt32.dll" ) ) == NULL_HINSTANCE )
		return( CRYPT_ERROR );

	/* Get pointers to the crypt functions */
	pCryptAcquireCertificatePrivateKey = ( CRYPTACQUIRECERTIFICATEPRIVATEKEY ) GetProcAddress( hCryptoAPI, "CryptAcquireCertificatePrivateKey" );
	pCryptAcquireContextA = ( CRYPTACQUIRECONTEXTA ) GetProcAddress( hAdvAPI32, "CryptAcquireContextA" );
	pCryptCreateHash = ( CRYPTCREATEHASH ) GetProcAddress( hAdvAPI32, "CryptCreateHash" );
	pCryptDecrypt = ( CRYPTDECRYPT ) GetProcAddress( hAdvAPI32, "CryptDecrypt" );
	pCryptDestroyHash = ( CRYPTDESTROYHASH ) GetProcAddress( hAdvAPI32, "CryptDestroyHash" );
	pCryptDestroyKey = ( CRYPTDESTROYKEY ) GetProcAddress( hAdvAPI32, "CryptDestroyKey" );
	pCryptEncrypt = ( CRYPTENCRYPT ) GetProcAddress( hAdvAPI32, "CryptEncrypt" );
	pCryptExportKey = ( CRYPTEXPORTKEY ) GetProcAddress( hAdvAPI32, "CryptExportKey" );
	pCryptFindCertificateKeyProvInfo = ( CRYPTFINDCERTIFICATEKEYPROVINFO ) GetProcAddress( hCryptoAPI, "CryptFindCertificateKeyProvInfo" );
	pCryptGenKey = ( CRYPTGENKEY ) GetProcAddress( hAdvAPI32, "CryptGenKey" );
	pCryptGenRandom = ( CRYPTGENRANDOM ) GetProcAddress( hAdvAPI32, "CryptGenRandom" );
	pCryptGetKeyParam = ( CRYPTGETKEYPARAM ) GetProcAddress( hAdvAPI32, "CryptGetKeyParam" );
	pCryptGetProvParam = ( CRYPTGETPROVPARAM ) GetProcAddress( hAdvAPI32, "CryptGetProvParam" );
	pCryptGetUserKey = ( CRYPTGETUSERKEY ) GetProcAddress( hAdvAPI32, "CryptGetUserKey" );
	pCryptHashData = ( CRYPTHASHDATA ) GetProcAddress( hAdvAPI32, "CryptHashData" );
	pCryptImportKey = ( CRYPTIMPORTKEY ) GetProcAddress( hAdvAPI32, "CryptImportKey" );
	pCryptReleaseContext = ( CRYPTRELEASECONTEXT ) GetProcAddress( hAdvAPI32, "CryptReleaseContext" );
	pCryptSetHashParam = ( CRYPTSETHASHPARAM ) GetProcAddress( hAdvAPI32, "CryptSetHashParam" );
	pCryptSetKeyParam = ( CRYPTSETKEYPARAM ) GetProcAddress( hAdvAPI32, "CryptSetKeyParam" );
	pCryptSignHash = ( CRYPTSIGNHASH ) GetProcAddress( hAdvAPI32, "CryptSignHashA" );

	/* Get pointers to the certificate functions */
	pCertAddCertificateContextToStore = ( CERTADDCERTIFICATETOSTORE ) GetProcAddress( hCryptoAPI, "CertAddCertificateContextToStore" );
	pCertAddEncodedCertificateToStore = ( CERTADDENCODEDCERTIFICATETOSTORE ) GetProcAddress( hCryptoAPI, "CertAddEncodedCertificateToStore" );
	pCertCreateCertificateContext = ( CERTCREATECERTIFICATECONTEXT ) GetProcAddress( hCryptoAPI, "CertCreateCertificateContext" );
	pCertDeleteCertificateFromStore = ( CERTDELETECERTIFICATEFROMSTORE ) GetProcAddress( hCryptoAPI, "CertDeleteCertificateFromStore" );
	pCertCloseStore = ( CERTCLOSESTORE ) GetProcAddress( hCryptoAPI, "CertCloseStore" );
	pCertFindCertificateInStore = ( CERTFINDCERTIFICATEINSTORE ) GetProcAddress( hCryptoAPI, "CertFindCertificateInStore" );
	pCertFreeCertificateChain = ( CERTFREECERTIFICATECHAIN ) GetProcAddress( hCryptoAPI, "CertFreeCertificateChain" );
	pCertFreeCertificateContext = ( CERTFREECERTIFICATECONTEXT )  GetProcAddress( hCryptoAPI, "CertFreeCertificateContext" );
	pCertGetCertificateChain = ( CERTGETCERTIFICATECHAIN ) GetProcAddress( hCryptoAPI, "CertGetCertificateChain" );
	pCertGetCertificateContextProperty = ( CERTGETCERTIFICATECONTEXTPROPERTY ) GetProcAddress( hCryptoAPI, "CertGetCertificateContextProperty" );
	pCertGetSubjectCertificateFromStore = ( CERTGETSUBJECTCERTIFICATEFROMSTORE ) GetProcAddress( hCryptoAPI, "CertGetSubjectCertificateFromStore" );
	pCertSetCertificateContextProperty = ( CERTSETCERTIFICATEPROPERTY ) GetProcAddress( hCryptoAPI, "CertSetCertificateContextProperty" );
	pCertOpenSystemStore = ( CERTOPENSYSTEMSTORE ) GetProcAddress( hCryptoAPI, "CertOpenSystemStoreA" );

	/* Make sure that we got valid pointers for every CryptoAPI function */
	if( pCertAddCertificateContextToStore == NULL || \
		pCertAddEncodedCertificateToStore == NULL || 
		pCertCreateCertificateContext == NULL ||
		pCertDeleteCertificateFromStore == NULL ||
		pCertCloseStore == NULL || pCertFindCertificateInStore == NULL ||
		pCertFreeCertificateChain == NULL || 
		pCertFreeCertificateContext == NULL || 
		pCertGetCertificateChain == NULL ||
		pCertGetCertificateContextProperty  == NULL ||
		pCertGetSubjectCertificateFromStore == NULL || 
		pCertSetCertificateContextProperty == NULL ||
		pCertOpenSystemStore == NULL || 
		pCryptAcquireCertificatePrivateKey == NULL ||
		pCryptAcquireContextA == NULL || pCryptCreateHash == NULL || 
		pCryptDecrypt == NULL || pCryptEncrypt == NULL || 
		pCryptExportKey == NULL || pCryptDestroyHash == NULL || 
		pCryptDestroyKey == NULL || 
		pCryptFindCertificateKeyProvInfo == NULL || pCryptGenKey == NULL || 
		pCryptGenRandom == NULL || pCryptGetKeyParam == NULL || 
		pCryptGetProvParam == NULL || pCryptGetUserKey == NULL || 
		pCryptHashData == NULL || pCryptImportKey == NULL || 
		pCryptReleaseContext == NULL || pCryptSetHashParam == NULL || 
		pCryptSetKeyParam == NULL || pCryptSignHash == NULL )
		{
		/* Free the library reference and reset the handle */
		DynamicUnload( hCryptoAPI );
		hCryptoAPI = NULL_HINSTANCE;
		return( CRYPT_ERROR );
		}

	return( CRYPT_OK );
	}

void deviceEndCryptoAPI( void )
	{
	if( hCryptoAPI != NULL_HINSTANCE )
		DynamicUnload( hCryptoAPI );
	hCryptoAPI = NULL_HINSTANCE;
	}

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Get access to the PKCS #11 device associated with a context */

static int getContextDeviceInfo( const CRYPT_HANDLE iCryptContext,
								 CRYPT_DEVICE *iCryptDevice, 
								 CRYPTOAPI_INFO **cryptoapiInfoPtrPtr )
	{
	CRYPT_DEVICE iLocalDevice;
	DEVICE_INFO *deviceInfo;
	int cryptStatus;

	/* Clear return values */
	*iCryptDevice = CRYPT_ERROR;
	*cryptoapiInfoPtrPtr = NULL;

	/* Get the the device associated with this context */
	cryptStatus = krnlSendMessage( iCryptContext, IMESSAGE_GETDEPENDENT, 
								   &iLocalDevice, OBJECT_TYPE_DEVICE );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Get the CryptoAPI information from the device information */
	cryptStatus = krnlAcquireObject( iLocalDevice, OBJECT_TYPE_DEVICE, 
									 ( void ** ) &deviceInfo, 
									 CRYPT_ERROR_SIGNALLED );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	*iCryptDevice = iLocalDevice;
	*cryptoapiInfoPtrPtr = deviceInfo->deviceCryptoAPI;

	return( CRYPT_OK );
	}

/* Map a CryptoAPI-specific error to a cryptlib error */

static int mapError( CRYPTOAPI_INFO *cryptoapiInfo, const int defaultError )
	{
#ifdef USE_ERRMSGS
	ERROR_INFO *errorInfo = &cryptoapiInfo->errorInfo;
	int messageLength;
#endif /* USE_ERRMSGS */
	const DWORD errorCode = GetLastError();

	/* Get the error message for this error.  FormatMessage() adds EOL 
	   terminators so we have to strip those before we pass the string back 
	   to the caller.  There's an incredibly arcane way of telling 
	   FormatMessage() to do this via escape codes passed in as part of a 
	   va_arg argument list, but aside from being complex to set up this 
	   also means that the function will try and insert information such as 
	   filenames from the argument list when required (there's no way to 
	   tell in advance which arguments are required), so this is more 
	   trouble than it's worth */
#ifdef USE_ERRMSGS
	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorCode, 0,
				   errorInfo->errorString, MAX_ERRMSG_SIZE - 1, NULL );
	for( messageLength = strlen( errorInfo->errorString );
		 messageLength > 0 && \
			( errorInfo->errorString[ messageLength - 1 ] == '\n' || \
			  errorInfo->errorString[ messageLength - 1 ] == '\r' );
		 messageLength-- );
	errorInfo->errorStringLength = messageLength;
#endif /* USE_ERRMSGS */

	/* Translate the CAPI error code into the cryptlib equivalent */
	switch( errorCode )
		{
		case CRYPT_E_UNKNOWN_ALGO:
			return( CRYPT_ERROR_NOTAVAIL );

		case ERROR_BUSY:
			return( CRYPT_ERROR_TIMEOUT );

		case ERROR_MORE_DATA:
			return( CRYPT_ERROR_OVERFLOW );

		case ERROR_NO_MORE_ITEMS:
			return( CRYPT_ERROR_COMPLETE );

		case NTE_BAD_DATA:
			return( CRYPT_ERROR_BADDATA );

		case CRYPT_E_EXISTS:
		case NTE_EXISTS:
			return( CRYPT_ERROR_DUPLICATE );

		case ERROR_NOT_ENOUGH_MEMORY:
		case NTE_NO_MEMORY:
			return( CRYPT_ERROR_MEMORY );

		case CRYPT_E_SECURITY_SETTINGS:
		case NTE_PERM:
			return( CRYPT_ERROR_PERMISSION );

		case NTE_BAD_SIGNATURE:
			return( CRYPT_ERROR_SIGNATURE );

		case CRYPT_E_NO_MATCH:
		case CRYPT_E_NOT_FOUND:
		case NTE_KEYSET_NOT_DEF:
		case NTE_NOT_FOUND:
		case NTE_PROV_DLL_NOT_FOUND:
		case NTE_PROV_TYPE_NO_MATCH:
		case NTE_PROV_TYPE_NOT_DEF:
			return( CRYPT_ERROR_NOTFOUND );
		}

	return( defaultError );
	}

static int mapDeviceError( CONTEXT_INFO *contextInfoPtr, const int defaultError )
	{
	CRYPT_DEVICE iCryptDevice;
	CRYPTOAPI_INFO *cryptoapiInfo;
	int status;

	/* Get the device associated with this context, set the error information
	   in it, and exit */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &cryptoapiInfo );
	if( cryptStatusError( status ) )
		return( status );
	status = mapError( cryptoapiInfo, defaultError );
	krnlReleaseObject( iCryptDevice );
	return( status );
	}

/* Map cryptlib to/from CryptoAPI algorithm IDs */

static const MAP_TABLE algoMapTbl[] = {
	/* PKC algorithms */
	{ CRYPT_ALGO_RSA, CALG_RSA_SIGN },
	{ CRYPT_ALGO_RSA, CALG_RSA_KEYX },
	{ CRYPT_ALGO_DSA, CALG_DSS_SIGN },

	/* Encryption algorithms */
	{ CRYPT_ALGO_DES, CALG_DES },
	{ CRYPT_ALGO_3DES, CALG_3DES },
	{ CRYPT_ALGO_RC2, CALG_RC2 },
	{ CRYPT_ALGO_RC4, CALG_RC4 },

	/* Hash algorithms */
	{ CRYPT_ALGO_MD5, CALG_MD5 },
	{ CRYPT_ALGO_SHA1, CALG_SHA },

	{ CRYPT_ALGO_NONE, 0 }, { CRYPT_ALGO_NONE, 0 }
	};

static ALG_ID cryptlibToCapiID( const CRYPT_ALGO_TYPE cryptAlgo )
	{
	int value, status;

	status = mapValue( cryptAlgo, &value, algoMapTbl,
					   FAILSAFE_ARRAYSIZE( algoMapTbl, MAP_TABLE ) );
	ENSURES_EXT( cryptStatusOK( status ), CALG_NONE );
	
	return( value );
	}

static CRYPT_ALGO_TYPE capiToCryptlibID( const ALG_ID algID )
	{
	int i;

	for( i = 0; algoMapTbl[ i ].source != CRYPT_ALGO_NONE && \
				i < FAILSAFE_ARRAYSIZE( algoMapTbl, MAP_TABLE ); i++ )
		{
		if( ( ALG_ID ) algoMapTbl[ i ].destination == algID )
			break;
		}
	if( i >= FAILSAFE_ARRAYSIZE( algoMapTbl, MAP_TABLE ) )
		retIntError_Ext( CRYPT_ALGO_NONE );
	if( algoMapTbl[ i ].source == CRYPT_ALGO_NONE )
		return( CRYPT_ALGO_NONE );
	return( algoMapTbl[ i ].source );
	}

/* Copy an MPI into the little-endian order required by CryptoAPI, returning
   the end position of the copied MPI */

static BYTE *copyMPI( BYTE *dest, const BYTE *src, const int srcLen,
					  const int srcRequiredLen )
	{
	int i;

	dest += srcLen - 1;
	for( i = 0; i < srcLen; i++ )
		*dest-- = *src++;
	dest += srcLen + 1;
	if( srcLen < srcRequiredLen )
		{
		/* CryptoAPI blobs don't contain any length information but 
		   implicitly specify all lengths in terms of the size of the
		   main MPI component, so if the actual length is less than the
		   assumed length we pad the remainder out with zeroes */
		for( i = 0; i < srcRequiredLen - srcLen; i++ )
			*dest++ = 0;
		}
	
	return( dest );
	}

/* Create the special-case RSA key with e=1 that's needed to allow direct 
   key import and export */

static int createExportKey( const HCRYPTPROV hProv, HCRYPTKEY *hPrivateKey, 
							int *privateKeySize )
	{
	BLOBHEADER *blobHeaderPtr;
	RSAPUBKEY *pubKeyPtr;
	BYTE keyBlob[ 1024 + 8 ], *keyBlobPtr;
	DWORD keyBlobLen = 1024;
	BOOL result;
	int bitLen16;

	/* Generate a private key and export it as a private key blob:

		Ofs	Value

		  0	BLOBHEADER blobheader {
			  0	BYTE bType;
			  1	BYTE bVersion;
			  2 WORD reserved;
			  4	ALG_ID aiKeyAlg; }
		  8	RSAPUBKEY rsapubkey {
			  8 DWORD magic;
			 12	DWORD bitlen;
			 16	DWORD pubexp; }
		 20	BYTE modulus[ rsapubkey.bitlen / 8 ];
			BYTE prime1[ rsapubkey.bitlen / 16 ];
			BYTE prime2[ rsapubkey.bitlen / 16 ];
			BYTE exponent1[ rsapubkey.bitlen / 16 ];
			BYTE exponent2[ rsapubkey.bitlen / 16 ];
			BYTE coefficient[ rsapubkey.bitlen / 16 ];
			BYTE privateExponent[ rsapubkey.bitlen / 8 ]; */
	if( !pCryptGenKey( hProv, AT_KEYEXCHANGE, CRYPT_EXPORTABLE, hPrivateKey ) || \
		!pCryptExportKey( *hPrivateKey, 0, PRIVATEKEYBLOB, 0, keyBlob, &keyBlobLen ) || \
		!pCryptDestroyKey( *hPrivateKey ) )
		return( CRYPT_ERROR );

	/* Perform a general sanity check on the returned data */
	blobHeaderPtr = ( BLOBHEADER * ) keyBlob;
	if( blobHeaderPtr->bType != PRIVATEKEYBLOB || \
		blobHeaderPtr->bVersion != CUR_BLOB_VERSION || \
		blobHeaderPtr->aiKeyAlg != CALG_RSA_KEYX )
		{
		pCryptDestroyKey( *hPrivateKey );
		return( CRYPT_ERROR );
		}

	/* Set the public exponent to 1 (little-endian 32-bit value) and skip to 
	   the private exponents */
	pubKeyPtr = ( RSAPUBKEY * ) ( keyBlob + BLOBHEADER_SIZE );
	bitLen16 = ( pubKeyPtr->bitlen / 16 );
	pubKeyPtr->pubexp = 1;
	keyBlobPtr = keyBlob + 20 + ( pubKeyPtr->bitlen / 8 ) + bitLen16 + bitLen16;

	/* Set the two exponents to 1 */
	*keyBlobPtr++ = 1;
	memset( keyBlobPtr, 0, bitLen16 - 1 );
	keyBlobPtr += bitLen16 - 1;
	*keyBlobPtr++ = 1;
	memset( keyBlobPtr, 0, bitLen16 - 1 );
	keyBlobPtr += bitLen16 - 1;

	/* Set the private exponent to 1 */
	keyBlobPtr += bitLen16;		/* Skip coefficient */
	*keyBlobPtr++ = 1;
	memset( keyBlobPtr, 0, bitLen16 - 1 );
	keyBlobPtr += bitLen16 - 1;

	/* Finally, re-import the hacked key and clean up */
	result = pCryptImportKey( hProv, keyBlob, keyBlobLen, 0, 0, hPrivateKey );
	if( result )
		*privateKeySize = pubKeyPtr->bitlen / 8;
	else
		*hPrivateKey = 0;
	zeroise( keyBlob, keyBlobLen );

	return( result ? CRYPT_OK : CRYPT_ERROR );
	}

/* Import a raw session key using the exponent-one RSA key */

static int importPlainKey( const HCRYPTPROV hProv, 
						   const HCRYPTKEY hPrivateKey, 
						   const int privateKeySize, HCRYPTKEY *hSessionKey, 
						   const CRYPT_ALGO_TYPE cryptAlgo, const BYTE *keyData, 
						   const int keyDataSize, void *errorInfoPtr )
	{
	BLOBHEADER *blobHeaderPtr;
	BYTE keyBlob[ 1024 + 8 ], *keyBlobPtr;
	ALG_ID algID;
	DWORD *dwPtr;
	BOOL result;
	const int blobSize = sizeof( BLOBHEADER ) + sizeof( ALG_ID ) + privateKeySize;
	int i;

	/* Set up a SIMPLEBLOB:

		Ofs	Value
		  0	BLOBHEADER blobheader {
			  0	BYTE bType;
			  1	BYTE bVersion;
			  2 WORD reserved;
			  4	ALG_ID aiKeyAlg; }
		  8	ALG_ID algid;
		 12	BYTE encryptedkey[ rsapubkey.bitlen/8 ]; */
	memset( keyBlob, 0, 1024 );
	algID = cryptlibToCapiID( cryptAlgo );
	if( algID == 0 )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Set up the BLOBHEADER part of the blob */
	blobHeaderPtr = ( BLOBHEADER * ) keyBlob;
    blobHeaderPtr->bType = SIMPLEBLOB;
	blobHeaderPtr->bVersion = CUR_BLOB_VERSION;
	blobHeaderPtr->aiKeyAlg = algID;

	/* Set up the private-key algorithm ID */
	dwPtr = ( DWORD * )( keyBlob + BLOBHEADER_SIZE );
	*dwPtr = CALG_RSA_KEYX;

	/* Store the key as byte-reversed PKCS #1 padded data (or at least close 
	   enough to it to work for the import) */
	keyBlobPtr = keyBlob + 12;
	for( i = keyDataSize - 1; i >= 0; i-- )
		*keyBlobPtr++ = keyData[ i ];
	*keyBlobPtr++ = 0;
	memset( keyBlobPtr, 2, privateKeySize - ( keyDataSize + 2 ) );

	/* Import the key from the faked PKCS #1 wrapped form */
	result = pCryptImportKey( hProv, keyBlob, blobSize, hPrivateKey, 0, hSessionKey );
	zeroise( keyBlob, blobSize );
	if( !result )
		return( mapDeviceError( errorInfoPtr, CRYPT_ERROR_FAILED ) );

	return( CRYPT_OK );
	}

/* Load a CryptoAPI public key into a cryptlib native context */

static int getPubkeyComponents( CRYPTOAPI_INFO *cryptoapiInfo,
								const HCRYPTKEY hKey, BYTE *n, int *nLen, 
								BYTE *e, int *eLen )
	{
	BLOBHEADER *blobHeaderPtr;
	RSAPUBKEY *pubKeyPtr;
	BYTE keyBlob[ 1024 + CRYPT_MAX_PKCSIZE + 8 ], *nPtr;
	BYTE buffer[ 16 + 8 ], *bufPtr = buffer;
	DWORD keyBlobLen = 1024 + CRYPT_MAX_PKCSIZE;
	int exponent, length;

	/* Clear return values */
	memset( n, 0, 8 );
	memset( e, 0, 8 );
	*nLen = *eLen = 0;

	/* Get the public key components */
	if( !pCryptExportKey( hKey, 0, PUBLICKEYBLOB, 0, keyBlob, &keyBlobLen ) )
		return( mapError( cryptoapiInfo, CRYPT_ERROR_FAILED ) );

	/* Perform a general sanity check on the returned data */
	blobHeaderPtr = ( BLOBHEADER * ) keyBlob;
	if( blobHeaderPtr->bType != PUBLICKEYBLOB || \
		blobHeaderPtr->bVersion != CUR_BLOB_VERSION )
		return( CRYPT_ERROR_FAILED );

	/* Extract the public key components */
	pubKeyPtr = ( RSAPUBKEY * ) ( keyBlob + BLOBHEADER_SIZE );
	exponent = pubKeyPtr->pubexp;
	nPtr = keyBlob + 20;
	length = pubKeyPtr->bitlen / 8;
	while( length > 0 && nPtr[ length ] == 0 )
		length--;
	copyMPI( n, nPtr, length, length );
	*nLen = length;
	length = ( exponent <= 0xFF ) ? 1 : \
			 ( exponent <= 0xFFFF ) ? 2 : \
			 ( exponent <= 0xFFFFFFL ) ? 3 : 4;
	mputLong( bufPtr, exponent );
	memcpy( e, buffer + ( 4 - length ), length );
	*eLen = length;

	return( CRYPT_OK );
	}

static int capiToCryptlibContext( CRYPTOAPI_INFO *cryptoapiInfo,
								  const HCRYPTKEY hKey, 
								  CRYPT_CONTEXT *cryptContextPtr )
	{
	CRYPT_PKCINFO_RSA rsaKey;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	BYTE n[ CRYPT_MAX_PKCSIZE + 8 ], e[ CRYPT_MAX_PKCSIZE + 8 ];
	int nLen, eLen, status;

	/* Clear return value */
	*cryptContextPtr = CRYPT_ERROR;

	/* Extract the public-key components from the CryptoAPI context */
	status = getPubkeyComponents( cryptoapiInfo, hKey, n, &nLen, e, &eLen );
	if( cryptStatusError( status ) )
		return( status );

	/* Copy the public-key components into the cryptlib format */
	cryptInitComponents( &rsaKey, CRYPT_KEYTYPE_PUBLIC );
	cryptSetComponent( ( &rsaKey )->n, n, bytesToBits( nLen ) );
	cryptSetComponent( ( &rsaKey )->e, e, bytesToBits( eLen ) );
	zeroise( n, CRYPT_MAX_PKCSIZE );
	zeroise( e, CRYPT_MAX_PKCSIZE );

	/* Create the RSA context */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_RSA );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		{
		cryptDestroyComponents( &rsaKey );
		return( status );
		}

	/* Load the key into the context */
	setMessageData( &msgData, "CryptoAPI RSA key", 17 );
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_LABEL );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, &rsaKey, sizeof( CRYPT_PKCINFO_RSA ) );
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE_S, &msgData,
								  CRYPT_CTXINFO_KEY_COMPONENTS );
		}
	cryptDestroyComponents( &rsaKey );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Failed to load CryptoAPI public key data" ));
		assert( DEBUG_WARN );
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	*cryptContextPtr = createInfo.cryptHandle;
	return( CRYPT_OK );
	}

/* Compare a CryptoAPI private key with a certificate to check whether the
   certificate corresponds to the key */

static BOOLEAN isCertKey( const CRYPTOAPI_INFO *cryptoapiInfo,
						  const HCRYPTKEY hKey, 
						  const CRYPT_CERTIFICATE iCryptCert )
	{
	return( TRUE );
	}

/* Get a certificate using a key/certificate identifier */

static int getCertificate( const CRYPTOAPI_INFO *cryptoapiInfo,
						   const CRYPT_KEYID_TYPE keyIDtype,
						   const void *keyID, const int keyIDlength,
						   PCCERT_CONTEXT *pCertContextPtr )
	{
	PCCERT_CONTEXT pCertContext = NULL;

	switch( keyIDtype )
		{
		case CRYPT_KEYID_NAME:
			{
			CERT_RDN certRDN;
			CERT_RDN_ATTR certRDNAttr;

			/* Find a certificate by CN */
			memset( &certRDNAttr, 0, sizeof( CERT_RDN_ATTR ) );
			certRDNAttr.pszObjId = szOID_COMMON_NAME;
			certRDNAttr.dwValueType = CERT_RDN_ANY_TYPE;
			certRDNAttr.Value.pbData = ( void * ) keyID;
			certRDNAttr.Value.cbData = keyIDlength;
			memset( &certRDN, 0, sizeof( CERT_RDN ) );
			certRDN.rgRDNAttr = &certRDNAttr;
			certRDN.cRDNAttr = 1;
			pCertContext = \
				pCertFindCertificateInStore( cryptoapiInfo->hCertStore,
							X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_ATTR,
							&certRDN, NULL );
			break;
			}

		case CRYPT_KEYID_URI:
			{
			CERT_RDN certRDN;
			CERT_RDN_ATTR certRDNAttr;

			/* There doesn't appear to be any way to locate a certificate 
			   using the email address in an altName, so we have to restrict 
			   ourselves to the most commonly-used OID for certificates in 
			   DNs */
			memset( &certRDNAttr, 0, sizeof( CERT_RDN_ATTR ) );
			certRDNAttr.pszObjId = szOID_RSA_emailAddr ;
			certRDNAttr.dwValueType = CERT_RDN_ANY_TYPE;
			certRDNAttr.Value.pbData = ( void * ) keyID;
			certRDNAttr.Value.cbData = keyIDlength;
			memset( &certRDN, 0, sizeof( CERT_RDN ) );
			certRDN.rgRDNAttr = &certRDNAttr;
			certRDN.cRDNAttr = 1;
			pCertContext = \
				pCertFindCertificateInStore( cryptoapiInfo->hCertStore,
							X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_ATTR,
							&certRDN, NULL );
			break;
			}

		case CRYPT_IKEYID_CERTID:
			{
			CRYPT_DATA_BLOB cryptDataBlob;

			memset( &cryptDataBlob, 0, sizeof( CRYPT_DATA_BLOB ) );
			cryptDataBlob.pbData = ( void * ) keyID;
			cryptDataBlob.cbData = keyIDlength;
			pCertContext = \
				pCertFindCertificateInStore( cryptoapiInfo->hCertStore,
							X509_ASN_ENCODING, 0, CERT_FIND_SHA1_HASH,
							&cryptDataBlob, NULL );
			break;
			}

		case CRYPT_IKEYID_KEYID:
			{
#if 0
			CERT_ID certID;

			certID.dwIdChoice = CERT_ID_KEY_IDENTIFIER;
			certID.KeyId.pbData = ( void * ) keyID;
			certID.KeyId.cbData = keyIDlength;
			pCertContext = \
				pCertFindCertificateInStore( cryptoapiInfo->hCertStore,
							X509_ASN_ENCODING, 0, CERT_FIND_CERT_ID,
							&certID, NULL );
#else
			CRYPT_HASH_BLOB hashBlob;

			hashBlob.pbData = ( void * ) keyID;
			hashBlob.cbData = keyIDlength;
			pCertContext = \
				pCertFindCertificateInStore( cryptoapiInfo->hCertStore,
							X509_ASN_ENCODING, 0, CERT_FIND_KEY_IDENTIFIER,
							&hashBlob, NULL );
#endif /* 0 */
			break;
			}

		case CRYPT_IKEYID_ISSUERANDSERIALNUMBER:
			{
			CERT_INFO certInfo;
			STREAM stream;
			void *dataPtr DUMMY_INIT_PTR;
			int length DUMMY_INIT, status;

			memset( &certInfo, 0, sizeof( CERT_INFO ) );
			sMemConnect( &stream, keyID, keyIDlength );
			status = readSequence( &stream, NULL );
			if( cryptStatusOK( status ) )
				status = getStreamObjectLength( &stream, &length );
			if( cryptStatusOK( status ) )
				status = sMemGetDataBlock( &stream, &dataPtr, length );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( &stream );
				return( status );
				}
			certInfo.Issuer.pbData = dataPtr;		/* Issuer DN */
			certInfo.Issuer.cbData = length;
			status = sSkip( &stream, length, MAX_INTLENGTH_SHORT );
			if( cryptStatusOK( status ) )
				status = getStreamObjectLength( &stream, &length );
			if( cryptStatusOK( status ) )
				status = sMemGetDataBlock( &stream, &dataPtr, length );
			if( cryptStatusError( status ) )
				return( status );
			certInfo.SerialNumber.pbData = dataPtr;	/* Serial number */
			certInfo.SerialNumber.cbData = length;
			status = sSkip( &stream, length, MAX_INTLENGTH_SHORT );
			assert( sStatusOK( &stream ) );
			sMemDisconnect( &stream );
			if( cryptStatusError( status ) )
				return( status );
			pCertContext = \
				pCertGetSubjectCertificateFromStore( cryptoapiInfo->hCertStore,
							X509_ASN_ENCODING, &certInfo );
			}

		default:
			retIntError();
		}

	if( pCertContext == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	*pCertContextPtr = pCertContext;
	return( CRYPT_OK );
	}

/* Get a certificate chain from a leaf certificate */

static int getCertificateChain( CRYPTOAPI_INFO *cryptoapiInfo,
								const PCCERT_CONTEXT pCertContext,
								PCCERT_CHAIN_CONTEXT *pChainContextPtr )
	{
	CERT_CHAIN_PARA chainPara;
	CERT_USAGE_MATCH certUsage;
	CERT_ENHKEY_USAGE enhkeyUsage;
	PCCERT_CHAIN_CONTEXT pChainContext;

	/* Clear return value */
	*pChainContextPtr = NULL;

	/* Get the chain from the supplied certificate up to a root 
	   certificate */
	memset( &enhkeyUsage, 0, sizeof( CERT_ENHKEY_USAGE ) );
	enhkeyUsage.cUsageIdentifier = 0;
	enhkeyUsage.rgpszUsageIdentifier = NULL;
	memset( &certUsage, 0, sizeof( CERT_USAGE_MATCH ) );
	certUsage.dwType = USAGE_MATCH_TYPE_AND;
	certUsage.Usage = enhkeyUsage;
	memset( &chainPara, 0, sizeof( CERT_CHAIN_PARA ) );
	chainPara.cbSize = sizeof( CERT_CHAIN_PARA );
	chainPara.RequestedUsage = certUsage;
	if( !pCertGetCertificateChain( NULL, pCertContext, NULL, NULL, &chainPara,
								   CERT_CHAIN_CACHE_END_CERT | \
								   CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY, 
								   NULL, &pChainContext ) )
		return( mapError( cryptoapiInfo, CRYPT_ERROR_NOTFOUND ) );
	*pChainContextPtr = pChainContext;
	return( CRYPT_OK );
	}

/* Get a certificate from a public/private key and vice versa */

static int getCertificateFromKey( CRYPTOAPI_INFO *cryptoapiInfo,
								  const HCRYPTKEY hKey,
								  const BOOLEAN isSigningKey,
								  PCCERT_CONTEXT *pCertContextPtr )
	{
	PCCERT_CONTEXT pCertContext;
	CERT_PUBLIC_KEY_INFO pubKeyInfo;
	BYTE keyBlob[ 1024 + CRYPT_MAX_PKCSIZE + 8 ];
	DWORD keyBlobLen = 1024 + CRYPT_MAX_PKCSIZE;

	/* Clear return value */
	*pCertContextPtr = NULL;

	/* Extract the public-key components from the public or private key */
	if( !pCryptExportKey( hKey, 0, PUBLICKEYBLOB, 0, keyBlob, &keyBlobLen ) )
		{
		pCryptDestroyKey( hKey );
		return( CRYPT_ERROR_NOTFOUND );
		}

	/* Get the certificate for the context's public key */
	memset( &pubKeyInfo, 0, sizeof( CERT_PUBLIC_KEY_INFO ) );
	pubKeyInfo.Algorithm.pszObjId = isSigningKey ? \
									CERT_DEFAULT_OID_PUBLIC_KEY_SIGN : \
									CERT_DEFAULT_OID_PUBLIC_KEY_XCHG;
	pubKeyInfo.PublicKey.pbData = keyBlob;
	pubKeyInfo.PublicKey.cbData = keyBlobLen;
	pCertContext = \
		pCertFindCertificateInStore( cryptoapiInfo->hCertStore,
						X509_ASN_ENCODING, 0, CERT_FIND_PUBLIC_KEY,
						&pubKeyInfo, NULL );
	if( pCertContext == NULL )
		return( mapError( cryptoapiInfo, CRYPT_ERROR_NOTFOUND ) );
	*pCertContextPtr = pCertContext;
	return( CRYPT_OK );
	}

#if 0

static int getPrivKeyFromCertificate( CRYPTOAPI_INFO *cryptoapiInfo,
									  const PCCERT_CONTEXT pCertContext,
									  HCRYPTKEY *hKeyPtr )
	{
	HCRYPTPROV hProv;
	HCRYPTKEY hKey;
	DWORD dwKeySpec;
	BOOL fCallerFreeProv;

	/* Clear return value */
	*hKeyPtr = 0;

	/* Get the provider and key-type from the certificate and use that to 
	   get the key */
	if( !pCryptAcquireCertificatePrivateKey( pCertContext, 
									CRYPT_ACQUIRE_CACHE_FLAG, NULL,
									&hProv, &dwKeySpec, &fCallerFreeProv ) )
		return( mapError( cryptoapiInfo, CRYPT_ERROR_NOTFOUND ) );
	if( !pCryptGetUserKey( hProv, dwKeySpec, &hKey ) )
		return( mapError( cryptoapiInfo, CRYPT_ERROR_NOTFOUND ) );
	*hKeyPtr = hKey;
	return( CRYPT_OK );
	}
#endif /* 0 */

/* Create a private-key context using a CryptoAPI native key */

static int createPrivkeyContext( DEVICE_INFO *deviceInfo,
								 CRYPT_CONTEXT *iCryptContext,
								 CRYPT_ALGO_TYPE *cryptAlgo,
								 const HCRYPTKEY hKey,
								 const char *label )
	{
	ALG_ID algID;
	DWORD dwDataLen = sizeof( ALG_ID );
	const CAPABILITY_INFO *capabilityInfoPtr = NULL;
	MESSAGE_DATA msgData;
	int status;

	/* Clear return values */
	*iCryptContext = CRYPT_ERROR;
	*cryptAlgo = CRYPT_ALGO_NONE;

	/* Get the algorithm type and look up the corresponding capability 
	   information */
	if( !pCryptGetKeyParam( hKey, KP_ALGID, ( BYTE * ) &algID, &dwDataLen, 
							0 ) || \
		( *cryptAlgo = capiToCryptlibID( algID ) ) == CRYPT_ALGO_NONE )
		return( CRYPT_ERROR_NOTAVAIL );
	capabilityInfoPtr = findCapabilityInfo( deviceInfo->capabilityInfoList, 
											*cryptAlgo );
	if( capabilityInfoPtr == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Create a dummy context for the key, remember the device it's 
	   contained in, the object's label, and the handle for the device-
	   internal key, and mark it as initialised (i.e. with a key loaded) */
	status = createContextFromCapability( iCryptContext, 
								deviceInfo->ownerHandle, capabilityInfoPtr, 
								CREATEOBJECT_FLAG_DUMMY | \
								CREATEOBJECT_FLAG_PERSISTENT );
	if( cryptStatusError( status ) )
		return( status );
	krnlSendMessage( *iCryptContext, IMESSAGE_SETDEPENDENT,
					 &deviceInfo->objectHandle, SETDEP_OPTION_INCREF );
	krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE, 
					 ( MESSAGE_CAST ) &hKey, CRYPT_IATTRIBUTE_DEVICEOBJECT );
	setMessageData( &msgData, ( MESSAGE_CAST ) label, 
					min( strlen( label ), CRYPT_MAX_TEXTSIZE ) );
#if 0
	if( cryptAlgo == CRYPT_ALGO_RSA )
		/* Send the keying information to the context.  This is only 
		   possible for RSA keys since it's not possible to read y from a 
		   DSA private key object (see the comments in the DSA code for more 
		   on this), however the only time this is necessary is when a 
		   certificate is being generated for a key that was pre-generated 
		   in the device by someone else, which is typically done in Europe 
		   where DSA isn't used so this shouldn't be a problem */
		// Use getPubkeyComponents()
		cryptStatus = rsaSetPublicComponents( deviceInfo, *iCryptContext, 
											  hObject );
	else
		cryptStatus = krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE, 
									   &keySize, CRYPT_IATTRIBUTE_KEYSIZE );
#endif
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_LABEL );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( *iCryptContext, IMESSAGE_SETATTRIBUTE,
								   MESSAGE_VALUE_UNUSED, 
								   CRYPT_IATTRIBUTE_INITIALISED );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( *iCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Device Init/Shutdown/Device Control Routines			*
*																			*
****************************************************************************/

/* Prototypes for functions to get and free device capability information */

static int getCapabilities( DEVICE_INFO *deviceInfo );
static void freeCapabilities( DEVICE_INFO *deviceInfo );

/* Close a previously-opened session with the device.  We have to have this
   before the initialisation function since it may be called by it if the 
   initialisation process fails */

static void shutdownFunction( DEVICE_INFO *deviceInfo )
	{
	CRYPTOAPI_INFO *cryptoapiInfo = deviceInfo->deviceCryptoAPI;

	/* Log out and close the session with the device */
	if( deviceInfo->flags & DEVICE_LOGGEDIN )
		{
		if( cryptoapiInfo->hPrivateKey )
			pCryptDestroyKey( cryptoapiInfo->hPrivateKey );
		pCryptReleaseContext( cryptoapiInfo->hProv, 0 );
		}
	if( cryptoapiInfo->hCertStore != NULL )
		{
		pCertCloseStore( cryptoapiInfo->hCertStore, 0 );
		cryptoapiInfo->hCertStore = 0;
		}
	cryptoapiInfo->hProv = HCRYPTPROV_NONE;
	deviceInfo->flags &= ~( DEVICE_ACTIVE | DEVICE_LOGGEDIN );

	/* Free the device capability information */
	freeCapabilities( deviceInfo );
	}

/* Open a session with the device */

static int initFunction( DEVICE_INFO *deviceInfo, const char *name,
						 const int nameLength )
	{
	CRYPTOAPI_INFO *cryptoapiInfo = deviceInfo->deviceCryptoAPI;
	HCRYPTPROV hProv;
	HCERTSTORE hCertStore;
	char providerNameBuffer[ CRYPT_MAX_TEXTSIZE + 1 + 8 ];
	char keysetNameBuffer[ CRYPT_MAX_TEXTSIZE + 1 + 8 ];
	const char *keysetName = NULL;
	DWORD value;
	int i, driverNameLength = nameLength, status;

	/* Check whether a keyset name has been specified */
	strlcpy_s( keysetNameBuffer, CRYPT_MAX_TEXTSIZE, "MY" );/* Default keyset */
	for( i = 1; i < nameLength - 1; i++ )
		{
		if( name[ i ] == ':' && name[ i + 1 ] == ':' )
			{
			const int keysetNameLength = nameLength - ( i + 2 );

			if( i > CRYPT_MAX_TEXTSIZE || keysetNameLength <= 0 || \
				keysetNameLength > CRYPT_MAX_TEXTSIZE )
				return( CRYPT_ARGERROR_STR1 );

			/* We've got a keyset name appended to the provider name, break 
			   out the provider and keyset names */
			memcpy( providerNameBuffer, name, i );
			providerNameBuffer[ i ] = '\0';
			memcpy( keysetNameBuffer, name + i + 2, keysetNameLength );
			keysetNameBuffer[ keysetNameLength ] = '\0';
			name = providerNameBuffer;
			keysetName = keysetNameBuffer;
			break;
			}
		}

	/* If we're auto-detecting the device, try various choices */
	if( driverNameLength == 12 && \
		!strnicmp( "[Autodetect]", name, driverNameLength ) )
		{
		if( CryptAcquireContextA( &hProv, keysetName, MS_ENHANCED_PROV, 
								  PROV_RSA_FULL, 0 ) )
			cryptoapiInfo->hProv = hProv;
		else
			{
			if( CryptAcquireContextA( &hProv, keysetName, MS_DEF_PROV, 
									  PROV_RSA_FULL, 0 ) )
				cryptoapiInfo->hProv = hProv;
			else
				return( mapError( cryptoapiInfo, CRYPT_ERROR_NOTFOUND ) );
			}
		}
	else
		{
		/* Try and find a specific provider */
		if( CryptAcquireContextA( &hProv, keysetName, name, PROV_RSA_FULL, 0 ) )
			cryptoapiInfo->hProv = hProv;
		}

	/* Get information on device-specific capabilities */
	value = CRYPT_MAX_TEXTSIZE + 1;
	if( !CryptGetProvParam( cryptoapiInfo->hProv, PP_NAME, 
							cryptoapiInfo->label, &value, 0 ) )
		return( mapError( cryptoapiInfo, CRYPT_ERROR_NOTFOUND ) );
	cryptoapiInfo->labelLen = value;
	deviceInfo->label = cryptoapiInfo->label;
	deviceInfo->labelLen = cryptoapiInfo->labelLen;
	deviceInfo->flags |= DEVICE_ACTIVE;

	/* Set up the capability information for this device */
	status = getCapabilities( deviceInfo );
	if( cryptStatusError( status ) )
		{
		shutdownFunction( deviceInfo );
		return( ( status == CRYPT_ERROR ) ? CRYPT_ERROR_OPEN : status );
		}

	/* Create the special-purpose key needed to allow symmetric key loads */
	status = createExportKey( cryptoapiInfo->hProv, 
							  &cryptoapiInfo->hPrivateKey, 
							  &cryptoapiInfo->privateKeySize );
	if( cryptStatusError( status ) )
		{
		shutdownFunction( deviceInfo );
		return( status );
		}

	/* Open the certificate store used to store/retrieve certificates */
	hCertStore = pCertOpenSystemStore( cryptoapiInfo->hProv, 
									   keysetNameBuffer );
	if( hCertStore == NULL )
		{
		shutdownFunction( deviceInfo );
		return( CRYPT_ERROR_OPEN );
		}
	cryptoapiInfo->hCertStore = hCertStore;

	return( CRYPT_OK );
	}

/* Handle device control functions */

static int controlFunction( DEVICE_INFO *deviceInfo,
							const CRYPT_ATTRIBUTE_TYPE type,
							const void *data, const int dataLength,
							MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	retIntError();
	}

/****************************************************************************
*																			*
*						 	Misc.Device Interface Routines					*
*																			*
****************************************************************************/

/* Get random data from the device */

static int getRandomFunction( DEVICE_INFO *deviceInfo, void *buffer,
							  const int length,
							  MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	CRYPTOAPI_INFO *cryptoapiInfo = deviceInfo->deviceCryptoAPI;

	if( pCryptGenRandom( cryptoapiInfo->hProv, length, buffer ) )
		return( CRYPT_OK );
	return( mapError( cryptoapiInfo, CRYPT_ERROR_FAILED ) );
	}

/* Instantiate an object in a device.  This works like the create context
   function but instantiates a cryptlib object using data already contained
   in the device (for example a stored private key or certificate).  If the
   value being read is a public key and there's a certificate attached, the
   instantiated object is a native cryptlib object rather than a device
   object with a native certificate object attached because there doesn't 
   appear to be any good reason to create the public-key object in the 
   device.

   CryptoAPI doesn't have any concept of multiple private keys, only a 
   default encryption + signature key for the provider as a whole, and an 
   optional additional signature key to complement the encryption (and 
   signature if necessary) one.  In addition the ties between a private key 
   and its associated certificate(s) are rather tenuous, requiring jumping 
   through several levels of indirection in order to get from one to the 
   other.  To handle this, we have to use a meet-in-the-middle approach 
   where we try to go from private key to certificate if the identity of the 
   private key is obvious (the user has specifically asked for a private 
   decryption or signature key), or from certificate to private key in all 
   other cases */

static int getItemFunction( DEVICE_INFO *deviceInfo,
							CRYPT_CONTEXT *iCryptContext,
							const KEYMGMT_ITEM_TYPE itemType,
							const CRYPT_KEYID_TYPE keyIDtype,
							const void *keyID, const int keyIDlength,
							void *auxInfo, int *auxInfoLength, 
							const int flags )
	{
	CRYPTOAPI_INFO *cryptoapiInfo = deviceInfo->deviceCryptoAPI;
//	CRYPT_CERTIFICATE iCryptCert;
	HCRYPTKEY hKey = 0;
	PCCERT_CONTEXT pCertContext = NULL;
	PCCERT_CHAIN_CONTEXT pChainContext = NULL;
	DWORD dwKeySpec = 0;
	CRYPT_CERTIFICATE iCryptCert;
	CRYPT_ALGO_TYPE cryptAlgo = CRYPT_ALGO_NONE;
	const char *label = "Private key";
	int status;

	assert( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			itemType == KEYMGMT_ITEM_PRIVATEKEY );

	/* If we're searching for the key by label and the user has specified 
	   one of the special-case key descriptions, get the appropriate key */
	if( keyIDtype == CRYPT_KEYID_NAME )
		{
		if( keyIDlength == 13 && \
			!memcmp( keyID, "Signature key", 13 ) )
			dwKeySpec = AT_SIGNATURE;
		else
			if( keyIDlength == 14 && \
				!memcmp( keyID, "Encryption key", 14 ) )
				dwKeySpec = AT_KEYEXCHANGE;
		}

	/* If we haven't got a key type from the label and we're looking for a 
	   particular usage, get the appropriate key */
	if( dwKeySpec == 0 && itemType == KEYMGMT_ITEM_PRIVATEKEY )
		{
		if( flags & KEYMGMT_FLAG_USAGE_SIGN ) 
			dwKeySpec = AT_SIGNATURE;
		else
			if( flags & KEYMGMT_FLAG_USAGE_CRYPT ) 
				dwKeySpec = AT_KEYEXCHANGE;
		}

	/* If we still haven't got a key type, try and get the certificate for
	   the given ID */
	if( dwKeySpec != 0 )
		{
		/* Get the required key type */
		if( !pCryptGetUserKey( cryptoapiInfo->hProv, dwKeySpec, &hKey ) )
			return( CRYPT_ERROR_NOTFOUND );
		label = ( dwKeySpec == AT_SIGNATURE ) ? \
				"Signature key" : "Encryption key";

		/* If we're only doing a presence check, we don't need the key and 
		   can exit */
		if( flags & KEYMGMT_FLAG_CHECK_ONLY )
			{
			pCryptDestroyKey( hKey ); 
			return( CRYPT_OK );
			}

		/* Since CryptoAPI doesn't have any concept of key labels, the best 
		   that we can do is provide a generic description of the intended 
		   key usage as a form of pseudo-label */
		if( flags & KEYMGMT_FLAG_LABEL_ONLY )
			{
			strlcpy_s( auxInfo, *auxInfoLength, label );
			*auxInfoLength = strlen( label );
			pCryptDestroyKey( hKey ); 
			return( CRYPT_OK );
			}

		/* We've got the key, try and get the associated certificate */
		status = getCertificateFromKey( cryptoapiInfo, hKey, 
										( flags & KEYMGMT_FLAG_USAGE_SIGN ) ? \
											TRUE : FALSE, &pCertContext );
		if( cryptStatusError( status ) && \
			itemType == KEYMGMT_ITEM_PUBLICKEY ) 
			{
			/* We couldn't get a certificate for the key, if we're after a 
			   public key return it as a native context */
			status = capiToCryptlibContext( cryptoapiInfo, hKey, 
											iCryptContext );
			pCryptDestroyKey( hKey ); 
			return( status );
			}
		}
	else
		{
		assert( !( flags & KEYMGMT_FLAG_LABEL_ONLY ) );

		/* We don't have a definite private key ID indicator, go via the
		   certificate instead */
		status = getCertificate( cryptoapiInfo, keyIDtype, keyID, keyIDlength,
								 &pCertContext );
		if( cryptStatusError( status ) )
			return( status );

		/* If we're only doing a presence check, we don't need the key and 
		   can exit */
		if( flags & KEYMGMT_FLAG_CHECK_ONLY )
			{
			pCertFreeCertificateContext( pCertContext );
			return( CRYPT_OK );
			}

		/* If we're after a private key, try and find the one corresponding
		   to the certificate */
		if( itemType == KEYMGMT_ITEM_PRIVATEKEY )
			{
#if 1
			CERT_KEY_CONTEXT certKeyContext;
			DWORD certKeyContextSize = sizeof( CERT_KEY_CONTEXT );

			if( !pCertGetCertificateContextProperty( pCertContext, 
									CERT_KEY_CONTEXT_PROP_ID,
									&certKeyContext, &certKeyContextSize ) || \
				!pCryptGetUserKey( certKeyContext.hCryptProv, 
								   certKeyContext.dwKeySpec, &hKey ) )
				return( mapError( cryptoapiInfo, CRYPT_ERROR_NOTFOUND ) );

#else		/* Need to uncomment gPKFC() above */
			status = getPrivKeyFromCertificate( cryptoapiInfo, pCertContext, 
												&hKey );
#endif
			if( cryptStatusError( status ) )
				{
				pCertFreeCertificateContext( pCertContext );
				return( status );
				}
			}
		}

	/* If we've got a priavte key, create a device context for it */
	if( hKey != 0 )
		{
		status = createPrivkeyContext( deviceInfo, iCryptContext, 
									   &cryptAlgo, hKey, label );
		if( cryptStatusError( status ) )
			{
			if( pCertContext != NULL )
				pCertFreeCertificateContext( pCertContext );
			return( status );
			}
		}

	/* If there's no certificate available for the key, we're done */
	if( pCertContext == NULL )
		return( CRYPT_OK );

	/* We've got a key and certificate, try and get the rest of the chain 
	   for the certificate */
{
BOOLEAN publicComponentsOnly = FALSE;

	status = iCryptImportCertIndirect( &iCryptCert,
								deviceInfo->objectHandle, keyIDtype, keyID,
								keyIDlength, publicComponentsOnly ? \
									KEYMGMT_FLAG_NONE : \
									KEYMGMT_FLAG_DATAONLY_CERT );
}

	if( cryptStatusOK( status ) )
		{
		/* If we're getting a public key, the returned information is the 
		   certificate chain */
		if( itemType == KEYMGMT_ITEM_PUBLICKEY )
			*iCryptContext = iCryptCert;
		else
			{
			/* If we're getting a private key, attach the certificate chain 
			   to the context.  The certificate is an internal object used 
			   only by the context so we tell the kernel to mark it as owned 
			   by the context only */
			status = krnlSendMessage( *iCryptContext, IMESSAGE_SETDEPENDENT, 
									   &iCryptCert, SETDEP_OPTION_NOINCREF );
			}
		}
	
	/* Clean up */
	pCertFreeCertificateContext( pCertContext );
	pCertFreeCertificateChain( pChainContext );
	return( status );

#if 0
		{
		const int keyTemplateCount = ( keyID == NULL ) ? 1 : 2;

		/* Try and find the object with the given label/ID, or the first 
		   object of the given class if no ID is given */
		keyTemplate[ 0 ].pValue = ( CK_VOID_PTR ) \
								  ( ( itemType == KEYMGMT_ITEM_PUBLICKEY ) ? \
								  &pubkeyClass : &privkeyClass );
		if( keyIDtype != CRYPT_KEYID_NONE )
			{
			if( keyIDtype == CRYPT_IKEYID_KEYID )
				keyTemplate[ 1 ].type = CKA_ID;
			keyTemplate[ 1 ].pValue = ( CK_VOID_PTR ) keyID;
			keyTemplate[ 1 ].ulValueLen = keyIDlength;
			}
		cryptStatus = findObject( deviceInfo, &hObject, keyTemplate, 
								  keyTemplateCount );
		if( cryptStatus == CRYPT_ERROR_NOTFOUND && \
			itemType == KEYMGMT_ITEM_PUBLICKEY )
			{
			/* Some devices may only contain private key objects with 
			   associated certificates that can't be picked out of the other 
			   cruft that's present without going via the private key, so if 
			   we're looking for a public key and don't find one, we try 
			   again for a private key whose sole function is to point to an 
			   associated certificate */
			keyTemplate[ 0 ].pValue = ( CK_VOID_PTR ) &privkeyClass;
			cryptStatus = findObject( deviceInfo, &hObject, keyTemplate, 
									  keyTemplateCount );
			if( cryptStatusError( cryptStatus ) )
				return( cryptStatus );
		
			/* Remember that although we've got a private key object, we only 
			   need it to find the associated certificate and not finding an 
			   associated certificate is an error */
			certViaPrivateKey = TRUE;
			}
		}

	/* If we're looking for any kind of private key and we either have an
	   explicit certificate ID but couldn't find a certificate for it or we 
	   don't have a proper ID to search on and a generic search found more 
	   than one matching object, chances are we're after a generic decrypt 
	   key.  The former only occurs in misconfigured or limited-memory 
	   tokens, the latter only in rare tokens that store more than one 
	   private key, typically one for signing and one for verification.  
	   
	   If either of these cases occur we try again looking specifically for 
	   a decryption key.  Even this doesn't always work, there's are some
	   >1-key tokens that mark a signing key as a decryption key so we still 
	   get a CRYPT_ERROR_DUPLICATE error.
	   
	   Finally, if we can't find a decryption key either, we look for an
	   unwrapping key.  This may or may not work, depending on whether we 
	   have a decryption key marked as valid for unwrapping but not 
	   decryption, or a key that's genuinely only valid for unwrapping, but
	   at this point we're ready to try anything */
	if( itemType == KEYMGMT_ITEM_PRIVATEKEY && \
		( keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER && \
		  cryptStatus == CRYPT_ERROR_NOTFOUND ) || \
		( cryptStatus == CRYPT_ERROR_DUPLICATE ) )
		{
		static const CK_BBOOL bTrue = TRUE;
		CK_ATTRIBUTE decryptKeyTemplate[] = {
			{ CKA_CLASS, ( CK_VOID_PTR ) &privkeyClass, sizeof( CK_OBJECT_CLASS ) },
			{ CKA_DECRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) }
			};

		cryptStatus = findObject( deviceInfo, &hObject, 
								  decryptKeyTemplate, 2 );
		if( cryptStatusError( cryptStatus ) )
			{
			decryptKeyTemplate[ 1 ].type = CKA_UNWRAP;
			cryptStatus = findObject( deviceInfo, &hObject, 
									  decryptKeyTemplate, 2 );
			}
		}
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* If we're just checking whether an object exists, return now.  If all 
	   we want is the key label, copy it back to the caller and exit */
	if( flags & KEYMGMT_FLAG_CHECK_ONLY )
		return( CRYPT_OK );
	if( flags & KEYMGMT_FLAG_LABEL_ONLY )
		return( getObjectLabel( deviceInfo, hObject, auxInfo, 
								auxInfoLength ) );

	/* We found something, map the key type to a cryptlib algorithm ID,
	   determine the key size, and find its capabilities */
	keyTypeTemplate.pValue = &keyType;
	C_GetAttributeValue( cryptoapiInfo->hProv, hObject, 
						 &keyTypeTemplate, 1 );
	switch( ( int ) keyType )
		{
		case CKK_RSA:
			cryptAlgo = CRYPT_ALGO_RSA;
			keySizeTemplate.type = CKA_MODULUS;
			break;
		case CKK_DSA:
			cryptAlgo = CRYPT_ALGO_DSA;
			keySizeTemplate.type = CKA_PRIME;
			break;
		case CKK_DH:
			cryptAlgo = CRYPT_ALGO_DH;
			keySizeTemplate.type = CKA_PRIME;
			break;
		default:
			return( CRYPT_ERROR_NOTAVAIL );
		}
	C_GetAttributeValue( cryptoapiInfo->hProv, hObject, 
						 &keySizeTemplate, 1 );
	keySize = keySizeTemplate.ulValueLen;
	capabilityInfoPtr = findCapabilityInfo( deviceInfo->capabilityInfo, 
											cryptAlgo );
	if( capabilityInfoPtr == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Try and find a certificate which matches the key.  The process is as
	   follows:

		if certificate object found in issuerAndSerialNumber search
			create native data-only certificate object
			attach certificate object to key
		else
			if public key
				if certificate
					create native certificate (+context) object
				else
					create device pubkey object, mark as "key loaded"
			else
				create device privkey object, mark as "key loaded"
				if certificate
					create native data-only certificate object
					attach certificate object to key

	   The reason for doing things this way is given in the comments earlier
	   on in this function */
	if( privateKeyViaCert )
		{
		/* We've already got the certificate object handle, instantiate a 
		   native data-only certificate from it */
		cryptStatus = instantiateCert( deviceInfo, hCertificate, 
									   &iCryptCert, FALSE );
		if( cryptStatusError( cryptStatus ) )
			return( cryptStatus );
		certPresent = TRUE;
		}
	else
		{
		cryptStatus = findCertFromObject( deviceInfo, hObject, &iCryptCert, 
										  ( itemType == KEYMGMT_ITEM_PUBLICKEY ) ? \
										  FINDCERT_NORMAL : FINDCERT_DATAONLY );
		if( cryptStatusError( cryptStatus ) )
			{
			/* If we get a CRYPT_ERROR_NOTFOUND this is OK since it means 
			   there's no certificate present, however anything else is an 
			   error.  In addition if we've got a private key whose only 
			   function is to point to an associated certificate then not 
			   finding anything is also an error */
			if( cryptStatus != CRYPT_ERROR_NOTFOUND || certViaPrivateKey )
				return( cryptStatus );
			}
		else
			{
			/* We got the certificate, if we're being asked for a public key 
			   then we've created a native object to contain it so we return 
			   that */
			certPresent = TRUE;
			if( itemType == KEYMGMT_ITEM_PUBLICKEY )
				{
				*iCryptContext = iCryptCert;
				return( CRYPT_OK );
				}
			}
		}
#endif
	}

/* Update a device with a certificate */

static int setItemFunction( DEVICE_INFO *deviceInfo, 
							const CRYPT_HANDLE iCryptHandle )
	{
	CRYPTOAPI_INFO *cryptoapiInfo = deviceInfo->deviceCryptoAPI;
	HCRYPTKEY hKey;
	DWORD dwKeySpec = 0;
	CRYPT_CERTIFICATE iCryptCert;
	BOOLEAN seenNonDuplicate = FALSE;
	int iterationCount = 0, status;

	/* Lock the certificate for our exclusive use (in case it's a 
	   certificate chain, we also select the first certificate in the 
	   chain), update the device with the certificate, and unlock it to 
	   allow others access */
	krnlSendMessage( iCryptHandle, IMESSAGE_GETDEPENDENT, &iCryptCert, 
					 OBJECT_TYPE_CERTIFICATE );
	krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
					 MESSAGE_VALUE_CURSORFIRST, 
					 CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
							  MESSAGE_VALUE_TRUE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		return( status );

	/* Check whether the leaf certificate matches any of the user's private 
	   keys */
	if( pCryptGetUserKey( cryptoapiInfo->hProv, AT_KEYEXCHANGE, &hKey ) )
		{
		if( isCertKey( cryptoapiInfo, hKey, iCryptCert ) )
			dwKeySpec = AT_KEYEXCHANGE;
		pCryptDestroyKey( hKey ); 
		}
	else
		{
		if( pCryptGetUserKey( cryptoapiInfo->hProv, AT_SIGNATURE, &hKey ) )
			{
			if( isCertKey( cryptoapiInfo, hKey, iCryptCert ) )
				dwKeySpec = AT_SIGNATURE;
			pCryptDestroyKey( hKey ); 
			}
		}

	/* Add each certificate in the chain to the store */
	do
		{
		DYNBUF certDB;
		BOOL result = FALSE;

		/* Get the certificate data and add it to the store */
		status = dynCreate( &certDB, iCryptHandle, 
							CRYPT_CERTFORMAT_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( status );

		/* If the certificate corresponds to one of the user's private keys 
		   and a binding between the key and a certificate isn't already 
		   established, establish one now */
		if( dwKeySpec != 0 )
			{
			PCCERT_CONTEXT pCertContext;
			CERT_KEY_CONTEXT certKeyContext;
			DWORD certKeyContextSize = sizeof( CERT_KEY_CONTEXT );

			/* Check whether the certificate is already bound to a key and 
			   if not, bind it to the appropriate private key */
			pCertContext = pCertCreateCertificateContext( X509_ASN_ENCODING, 
									dynData( certDB ), dynLength( certDB ) );
			if( pCertContext != NULL && \
				!pCertGetCertificateContextProperty( pCertContext, 
									CERT_KEY_CONTEXT_PROP_ID,
									&certKeyContext, &certKeyContextSize ) )
				{
#if 1
				/* Check the certificate stores synchronising the 
				   certificate's CERT_KEY_PROV_INFO_PROP_ID with any present 
				   private keys if required */
				pCryptFindCertificateKeyProvInfo( pCertContext, 0, NULL );
#elif 0
				CRYPT_KEY_PROV_INFO keyProvInfo;
				BYTE buffer[ 256 + 8 ];
				DWORD length = 256;
				wchar_t provName[ 256 + 8 ], container[ 256 + 8 ];

				if( pCryptGetProvParam( cryptoapiInfo->hProv, PP_NAME,
										buffer, &length, 0 ) )
					MultiByteToWideChar( GetACP(), 0, buffer, length + 1, 
										 provName, 256 );
				if( pCryptGetProvParam( cryptoapiInfo->hProv, PP_CONTAINER,
										buffer, &length, 0 ) )
					MultiByteToWideChar( GetACP(), 0, buffer, length + 1, 
										 container, 256 );
				memset( &keyProvInfo, 0, sizeof( CRYPT_KEY_PROV_INFO ) );
				keyProvInfo.pwszContainerName = container;
				keyProvInfo.pwszProvName = provName;
				keyProvInfo.dwProvType = PROV_RSA_FULL;
				keyProvInfo.cProvParam = 0;
				keyProvInfo.dwKeySpec = dwKeySpec;
				pCertSetCertificateContextProperty( pCertContext, 
									CERT_KEY_PROV_HANDLE_PROP_ID, 0,
									&keyProvInfo );
#elif 0
				memset( &certKeyContext, 0, sizeof( CERT_KEY_CONTEXT ) );
				certKeyContext.cbSize = sizeof( CERT_KEY_CONTEXT );
				certKeyContext.dwKeySpec = dwKeySpec;
				certKeyContext.hCryptProv = cryptoapiInfo->hProv;
				pCertSetCertificateContextProperty( pCertContext, 
									CERT_KEY_CONTEXT_PROP_ID, 0,
									&certKeyContext );
#endif
				}

			/* Add the certificate to the store */
			if( pCertContext != NULL )
				{
				result = pCertAddCertificateContextToStore( cryptoapiInfo->hCertStore,
								pCertContext, CERT_STORE_ADD_NEW, NULL );
				pCertFreeCertificateContext( pCertContext );
				}

			/* We've now added a bound certificate, don't bind the key to 
			   any more certificates */
			dwKeySpec = 0;
			}
		else
			{
			/* Add the certificate to the store */
			result = pCertAddEncodedCertificateToStore( cryptoapiInfo->hCertStore,
							X509_ASN_ENCODING, dynData( certDB ), 
							dynLength( certDB ), CERT_STORE_ADD_NEW, NULL );
			}
		dynDestroy( &certDB );
		if( result )
			seenNonDuplicate = TRUE;
		}
	while( krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
							MESSAGE_VALUE_CURSORNEXT,
							CRYPT_CERTINFO_CURRENT_CERTIFICATE ) == CRYPT_OK && \
		   iterationCount++ < FAILSAFE_ITERATIONS_MED );
	if( iterationCount >= FAILSAFE_ITERATIONS_MED )
		retIntError();

	krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, MESSAGE_VALUE_FALSE, 
					 CRYPT_IATTRIBUTE_LOCKED );

	return( seenNonDuplicate ? CRYPT_OK : CRYPT_ERROR_DUPLICATE );
	}

/* Delete an object in a device */

static int deleteItemFunction( DEVICE_INFO *deviceInfo,
							   const KEYMGMT_ITEM_TYPE itemType,
							   const CRYPT_KEYID_TYPE keyIDtype,
							   const void *keyID, const int keyIDlength )
	{
#if 0
	static const CK_OBJECT_CLASS pubkeyClass = CKO_PUBLIC_KEY;
	static const CK_OBJECT_CLASS privkeyClass = CKO_PRIVATE_KEY;
	static const CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
	static const CK_CERTIFICATE_TYPE certType = CKC_X_509;
	CK_ATTRIBUTE certTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &certClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_CERTIFICATE_TYPE, ( CK_VOID_PTR ) &certType, sizeof( CK_CERTIFICATE_TYPE ) },
		{ CKA_LABEL, ( CK_VOID_PTR ) keyID, keyIDlength }
		};
	CK_ATTRIBUTE keyTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &pubkeyClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_LABEL, ( CK_VOID_PTR ) keyID, keyIDlength }
		};
	CK_OBJECT_HANDLE hPrivkey = CRYPT_ERROR, hCertificate = CRYPT_ERROR;
	CK_OBJECT_HANDLE hPubkey = CRYPT_ERROR;
	CK_RV status;
	int cryptStatus;

	assert( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			itemType == KEYMGMT_ITEM_PRIVATEKEY );
	assert( keyIDtype == CRYPT_KEYID_NAME );
	assert( keyIDlength > 0 && keyIDlength <= CRYPT_MAX_TEXTSIZE );

	/* Find the object to delete based on the label.  Since we can have 
	   multiple related objects (e.g. a key and a certificate) with the same 
	   label, a straight search for all objects with a given label could 
	   return CRYPT_ERROR_DUPLICATE so we search for the objects by type as 
	   well as label.  In addition even a search for specific objects can 
	   return CRYPT_ERROR_DUPLICATE so we use the Ex version of findObject() 
	   to make sure we don't get an error if multiple objects exist.  
	   Although cryptlib won't allow more than one object with a given label 
	   to be created, other applications might create duplicate labels.  The 
	   correct behaviour in these circumstances is uncertain, what we do for 
	   now is delete the first object we find that matches the label.
	   
	   First we try for a certificate and use that to find associated keys */
	cryptStatus = findObjectEx( deviceInfo, &hCertificate, certTemplate, 3 );
	if( cryptStatusOK( cryptStatus ) )
		{
		/* We got a certificate, if there are associated keys delete them as 
		   well */
		cryptStatus = findObjectFromObject( deviceInfo, hCertificate, 
											CKO_PUBLIC_KEY, &hPubkey );
		if( cryptStatusError( cryptStatus ) )
			hPubkey = CRYPT_ERROR;
		cryptStatus = findObjectFromObject( deviceInfo, hCertificate, 
											CKO_PRIVATE_KEY, &hPrivkey );
		if( cryptStatusError( cryptStatus ) )
			hPrivkey = CRYPT_ERROR;
		}
	else
		{
		/* We didn't find a certificate with the given label, try for public 
		   and private keys */
		cryptStatus = findObjectEx( deviceInfo, &hPubkey, keyTemplate, 2 );
		if( cryptStatusError( cryptStatus ) )
			hPubkey = CRYPT_ERROR;
		keyTemplate[ 0 ].pValue = ( CK_VOID_PTR ) &privkeyClass;
		cryptStatus = findObjectEx( deviceInfo, &hPrivkey, keyTemplate, 2 );
		if( cryptStatusError( cryptStatus ) )
			hPrivkey = CRYPT_ERROR;

		/* There may be an unlabelled certificate present, try and find it by 
		   looking for a certificate matching the key ID */
		if( hPubkey != CRYPT_ERROR || hPrivkey != CRYPT_ERROR )
			{
			cryptStatus = findObjectFromObject( deviceInfo, 
							( hPrivkey != CRYPT_ERROR ) ? hPrivkey : hPubkey, 
							CKO_CERTIFICATE, &hCertificate );
			if( cryptStatusError( cryptStatus ) )
				hCertificate = CRYPT_ERROR;
			}
		}

	/* If we found a public key with a given label but no private key, try 
	   and find a matching private key by ID, and vice versa */
	if( hPubkey != CRYPT_ERROR && hPrivkey == CRYPT_ERROR )
		{
		cryptStatus = findObjectFromObject( deviceInfo, hPubkey, 
											CKO_PRIVATE_KEY, &hPrivkey );
		if( cryptStatusError( cryptStatus ) )
			hPrivkey = CRYPT_ERROR;
		}
	if( hPrivkey != CRYPT_ERROR && hPubkey == CRYPT_ERROR )
		{
		cryptStatus = findObjectFromObject( deviceInfo, hPrivkey, 
											CKO_PUBLIC_KEY, &hPubkey );
		if( cryptStatusError( cryptStatus ) )
			hPubkey = CRYPT_ERROR;
		}
	if( hPrivkey == CRYPT_ERROR && hPubkey == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTFOUND );

	/* Reset the status values, which may contain error values due to not 
	   finding various objects to delete above */
	cryptStatus = CRYPT_OK;
	status = CKR_OK;

	/* Delete the objects */
	if( hCertificate != CRYPT_ERROR )
		status = C_DestroyObject( cryptoapiInfo->hProv, hCertificate );
	if( hPubkey != CRYPT_ERROR )
		{
		int status2;

		status2 = C_DestroyObject( cryptoapiInfo->hProv, hPubkey );
		if( status2 != CKR_OK && status == CKR_OK )
			status = status2;
		}
	if( hPrivkey != CRYPT_ERROR )
		{
		int status2;

		status2 = C_DestroyObject( cryptoapiInfo->hProv, hPrivkey );
		if( status2 != CKR_OK && status == CKR_OK )
			status = status2;
		}
	if( status != CKR_OK )
		cryptStatus = mapError( deviceInfo, status, CRYPT_ERROR_FAILED );
	return( cryptStatus );
#endif

	return( CRYPT_ERROR );
	}

/* Get the sequence of certificates in a chain from a device.  Unfortunately 
   we can't really make the certificate chain fetch stateless without re-
   doing the read of the entire chain from the CryptoAPI certificate store 
   for each fetch.  To avoid this performance hit we cache the chain in the 
   device information and pick out each certificate in turn during the 
   findNext */

static int getFirstItemFunction( DEVICE_INFO *deviceInfo, 
								 CRYPT_CERTIFICATE *iCertificate,
								 int *stateInfo, 
								 const CRYPT_KEYID_TYPE keyIDtype,
								 const void *keyID, const int keyIDlength,
								 const KEYMGMT_ITEM_TYPE itemType, 
								 const int options )
	{
	CRYPTOAPI_INFO *cryptoapiInfo = deviceInfo->deviceCryptoAPI;
	PCCERT_CONTEXT pCertContext;
	PCCERT_CHAIN_CONTEXT pChainContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( keyIDtype == CRYPT_IKEYID_KEYID );
	assert( isReadPtr( keyID, keyIDlength ) );
	assert( keyIDlength > 4 && keyIDlength < MAX_INTLENGTH_SHORT );
	assert( isWritePtr( stateInfo, sizeof( int ) ) );
	assert( itemType == KEYMGMT_ITEM_PUBLICKEY );

	/* Clear return values */
	*iCertificate = CRYPT_ERROR;
	*stateInfo = CRYPT_ERROR;
	cryptoapiInfo->hCertChain = NULL;

	/* Try and find the certificate with the given ID.  This should work 
	   because we've just read the ID for the indirect-import that lead to 
	   the getFirst call */
	status = getCertificate( cryptoapiInfo, keyIDtype, keyID, keyIDlength,
							 &pCertContext );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Failed to load CryptoAPI certificate data" ));
		assert( DEBUG_WARN );
		return( status );
		}

	/* Now try and get the chain from the certificate.  If this fails, we 
	   just use the standalone certificate */
	status = getCertificateChain( cryptoapiInfo, pCertContext, 
								  &pChainContext );
	if( cryptStatusOK( status ) )
		cryptoapiInfo->hCertChain = pChainContext;

	/* Import the leaf certificate as a cryptlib object */
	setMessageCreateObjectIndirectInfo( &createInfo, 
										pCertContext->pbCertEncoded, 
										pCertContext->cbCertEncoded,
										CRYPT_CERTTYPE_CERTIFICATE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	pCertFreeCertificateContext( pCertContext );
	if( cryptStatusError( status ) )
		{
		if( cryptoapiInfo->hCertChain != NULL )
			pCertFreeCertificateChain( cryptoapiInfo->hCertChain );
		return( status );
		}

	/* Remember that we've got the leaf certificate in the chain */
	assert( pChainContext->cChain == 1 );
	*stateInfo = 1;
	return( CRYPT_OK );
	}

static int getNextItemFunction( DEVICE_INFO *deviceInfo, 
								CRYPT_CERTIFICATE *iCertificate,
								int *stateInfo, const int options )
	{
	CRYPTOAPI_INFO *cryptoapiInfo = deviceInfo->deviceCryptoAPI;
	PCCERT_CHAIN_CONTEXT pChainContext = cryptoapiInfo->hCertChain;
	PCERT_SIMPLE_CHAIN pCertSimpleChain;
	PCERT_CHAIN_ELEMENT pCertChainElement;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isWritePtr( stateInfo, sizeof( int ) ) );
	assert( *stateInfo == CRYPT_ERROR || *stateInfo >= 1 );

	/* Clear return value */
	*iCertificate = CRYPT_ERROR;

	/* If the previous certificate was the last one, there's nothing left to 
	   fetch */
	if( cryptoapiInfo->hCertChain == NULL )
		{
		*stateInfo = CRYPT_ERROR;
		return( CRYPT_ERROR_NOTFOUND );
		}
	pCertSimpleChain = pChainContext->rgpChain[ 0 ];
	if( *stateInfo < 1 || *stateInfo > pCertSimpleChain->cElement )
		{
		pCertFreeCertificateChain( cryptoapiInfo->hCertChain );
		cryptoapiInfo->hCertChain = NULL;
		*stateInfo = CRYPT_ERROR;
		return( CRYPT_ERROR_NOTFOUND );
		}

	/* Get the next certificate in the chain */
	pCertChainElement = pCertSimpleChain->rgpElement[ *stateInfo ];
	setMessageCreateObjectIndirectInfo( &createInfo, 
							pCertChainElement->pCertContext->pbCertEncoded, 
							pCertChainElement->pCertContext->cbCertEncoded,
							CRYPT_CERTTYPE_CERTIFICATE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		pCertFreeCertificateChain( cryptoapiInfo->hCertChain );
		cryptoapiInfo->hCertChain = NULL;
		*stateInfo = CRYPT_ERROR;
		return( status );
		}
	( *stateInfo )++;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Capability Interface Routines					*
*																			*
****************************************************************************/

/* Clean up the object associated with a context */

static int genericEndFunction( CONTEXT_INFO *contextInfoPtr )
	{
	/* Since the device object that corresponds to the cryptlib object is
	   created on-demand, it may not exist yet if the action that triggers
	   the on-demand creation hasn't been taken yet.  If no device object
	   exists, we're done */
	if( contextInfoPtr->deviceObject == CRYPT_ERROR )
		return( CRYPT_OK );

	/* Destroy the object */
	if( contextInfoPtr->capabilityInfo->keySize > 0 )
		pCryptDestroyKey( contextInfoPtr->deviceObject );
	else
		pCryptDestroyHash( contextInfoPtr->deviceObject );
	return( CRYPT_OK );
	}

/* RSA algorithm-specific mapping functions.  Since CryptoAPI adds its own
   PKCS #1 padding, we add/remove the cryptlib-added padding to fake out the 
   presence of a raw RSA mechanism */

static int rsaSetKeyInfo( CRYPTOAPI_INFO *cryptoapiInfo,
						  CONTEXT_INFO *contextInfoPtr )
	{
	BYTE n[ CRYPT_MAX_PKCSIZE + 8 ], e[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE keyDataBuffer[ ( CRYPT_MAX_PKCSIZE * 2 ) + 8 ];
	MESSAGE_DATA msgData;
	int nLen, eLen, keyDataSize, status;

	/* Extract the public-key components from the CryptoAPI context */
	status = getPubkeyComponents( cryptoapiInfo, 
								  contextInfoPtr->deviceObject, 
								  n, &nLen, e, &eLen );
	if( cryptStatusError( status ) )
		return( status );

	/* Send the public key data to the context.  We send the keying 
	   information as CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL rather than 
	   CRYPT_IATTRIBUTE_KEY_SPKI since the latter transitions the context 
	   into the high state.  We don't want to do this because we're already 
	   in the middle of processing a message that does this on completion, 
	   all we're doing here is sending in encoded public key data for use by 
	   objects such as certificates */
	status = writeFlatPublicKey( keyDataBuffer, CRYPT_MAX_PKCSIZE * 2,
								 &keyDataSize, CRYPT_ALGO_RSA, 0,
								 n, nLen, e, eLen, NULL, 0, NULL, 0 );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, keyDataBuffer, keyDataSize );
		status = krnlSendMessage( contextInfoPtr->objectHandle, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL );
		}
	return( status );
	}

static int rsaInitKey( CONTEXT_INFO *contextInfoPtr, const void *key, 
					   const int keyLength )
	{
	CRYPT_DEVICE iCryptDevice;
	CRYPTOAPI_INFO *cryptoapiInfo;
	CRYPT_PKCINFO_RSA *rsaKey = ( CRYPT_PKCINFO_RSA * ) key;
	HCRYPTKEY hKey;
	BLOBHEADER *blobHeaderPtr;
	RSAPUBKEY *pubKeyPtr;
	BYTE keyBlob[ ( CRYPT_MAX_PKCSIZE * 8 ) + 8 ], *keyBlobPtr;
	DWORD exponent = 0L;
	BOOL result;
	const int nLen = bitsToBytes( rsaKey->nLen );
	int i, status;

	/* CryptoAPI sets some awkward constraints on key formats, only allowing
	   exponents that can be represented as 32-bit ints and requiring that
	   the lengths of all MPIs be explicitly specified by the modulus size,
	   so we have to check whether the key components are in this form 
	   before we can try and load the key */
	if( bitsToBytes( rsaKey->eLen ) > sizeof( DWORD ) )
		return( CRYPT_ERROR_BADDATA );
	if( !rsaKey->isPublicKey && \
		( ( bitsToBytes( rsaKey->pLen ) > nLen / 2 ) || \
		  ( bitsToBytes( rsaKey->qLen ) > nLen / 2 ) || \
		  ( bitsToBytes( rsaKey->e1Len ) > nLen / 2 ) || \
		  ( bitsToBytes( rsaKey->e2Len ) > nLen / 2 ) || \
		  ( bitsToBytes( rsaKey->uLen ) > nLen / 2 ) || \
		  ( bitsToBytes( rsaKey->dLen ) > nLen ) ) )
		return( CRYPT_ERROR_BADDATA );

	/* Get the exponent as a DWORD */
	for( i = 0; i < bitsToBytes( rsaKey->eLen ); i++ )
		exponent = ( exponent << 8 ) | rsaKey->e[ i ];

	/* Get the information for the device associated with this context */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &cryptoapiInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Set up the blob header:

		Ofs	Value
		  0	BLOBHEADER blobheader {
			  0	BYTE bType;
			  1	BYTE bVersion;
			  2 WORD reserved;
			  4	ALG_ID aiKeyAlg; }
		  8	RSAPUBKEY rsapubkey {
			  8 DWORD magic;
			 12	DWORD bitlen;
			 16	DWORD pubexp; }
		 20	BYTE modulus[ rsapubkey.bitlen / 8 ];
			BYTE prime1[ rsapubkey.bitlen / 16 ];
			BYTE prime2[ rsapubkey.bitlen / 16 ];
			BYTE exponent1[ rsapubkey.bitlen / 16 ];
			BYTE exponent2[ rsapubkey.bitlen / 16 ];
			BYTE coefficient[ rsapubkey.bitlen / 16 ];
			BYTE privateExponent[ rsapubkey.bitlen / 8 ]; */
	memset( keyBlob, 0, CRYPT_MAX_PKCSIZE * 8 );
	blobHeaderPtr = ( BLOBHEADER * ) keyBlob;
	blobHeaderPtr->bType = intToByte( ( rsaKey->isPublicKey ) ? \
									  PUBLICKEYBLOB : PRIVATEKEYBLOB );
	blobHeaderPtr->bVersion = CUR_BLOB_VERSION;
	blobHeaderPtr->aiKeyAlg = CALG_RSA_KEYX;

	/* Set up the public-key components.  CryptoAPI requires that the 
	   modulus length be a multiple of 8 bits, so we have to round up the 
	   length to the nearest byte boundary */
	pubKeyPtr = ( RSAPUBKEY * ) ( keyBlob + BLOBHEADER_SIZE );
	pubKeyPtr->magic = ( rsaKey->isPublicKey ) ? 0x31415352 : 0x32415352;
	pubKeyPtr->bitlen = roundUp( rsaKey->nLen, 8 );
	pubKeyPtr->pubexp = exponent;
	keyBlobPtr = keyBlob + BLOBHEADER_SIZE + ( 3 * sizeof( DWORD ) );
	keyBlobPtr = copyMPI( keyBlobPtr, rsaKey->n, nLen, nLen );

	/* Set up the private-key components if necessary */
	if( !rsaKey->isPublicKey )
		{
		keyBlobPtr = copyMPI( keyBlobPtr, rsaKey->p, 
							  bitsToBytes( rsaKey->pLen ), nLen / 2 );
		keyBlobPtr = copyMPI( keyBlobPtr, rsaKey->q, 
							  bitsToBytes( rsaKey->qLen ), nLen / 2 );
		keyBlobPtr = copyMPI( keyBlobPtr, rsaKey->e1, 
							  bitsToBytes( rsaKey->e1Len ), nLen / 2 );
		keyBlobPtr = copyMPI( keyBlobPtr, rsaKey->e2, 
							  bitsToBytes( rsaKey->e2Len ), nLen / 2 );
		keyBlobPtr = copyMPI( keyBlobPtr, rsaKey->u, 
							  bitsToBytes( rsaKey->uLen ), nLen / 2 );
		keyBlobPtr = copyMPI( keyBlobPtr, rsaKey->d, 
							  bitsToBytes( rsaKey->dLen ), nLen );
		}

	/* Import the key blob and clean up */
	result = pCryptImportKey( cryptoapiInfo->hProv, keyBlob, 
							  keyBlobPtr - keyBlob, 0, 0, &hKey );
	if( result )
		{
		/* Note that the following will break under Win64 since the hKey is
		   a 64-bit pointer while the deviceObject is a 32-bit unsigned 
		   value, this code is experimental and only enabled for Win32 debug 
		   so this isn't a problem at the moment */
		contextInfoPtr->deviceObject = ( long ) hKey;
		}
	else
		status = mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED );
	zeroise( keyBlob, CRYPT_MAX_PKCSIZE * 8 );

	/* Send the keying information to the context and set up the key ID 
	   information */
	if( cryptStatusOK( status ) )
		status = rsaSetKeyInfo( cryptoapiInfo, contextInfoPtr );

	krnlReleaseObject( iCryptDevice );
	return( status );
	}

static int rsaGenerateKey( CONTEXT_INFO *contextInfoPtr, const int keysizeBits )
	{
	CRYPT_DEVICE iCryptDevice;
	CRYPTOAPI_INFO *cryptoapiInfo;
	HCRYPTKEY hPrivateKey;
	BOOL result;
	int status;

	/* Get the information for the device associated with this context */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &cryptoapiInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Generate the key.  CryptoAPI kludges the key size information by 
	   masking it into the high bits of the flags parameter.  We make the
	   key exportable, because the ability to hand out keys in PKCS #12 
	   format is the prime (only?) motivation for anyone using CryptoAPI */
	result = pCryptGenKey( cryptoapiInfo->hProv, AT_KEYEXCHANGE, 
						   CRYPT_EXPORTABLE | ( keysizeBits << 16 ), 
						   &hPrivateKey );
	if( result )
		{
		/* Note that the following will break under Win64 since the hKey is
		   a 64-bit pointer while the deviceObject is a 32-bit unsigned 
		   value, this code is experimental and only enabled for Win32 debug 
		   so this isn't a problem at the moment */
		contextInfoPtr->deviceObject = ( long ) hPrivateKey;
		}
	else
		status = mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED );

	/* Send the keying information to the context and set up the key ID 
	   information */
	if( cryptStatusOK( status ) )
		status = rsaSetKeyInfo( cryptoapiInfo, contextInfoPtr );

	krnlReleaseObject( iCryptDevice );
	return( status );
	}

static int rsaSign( CONTEXT_INFO *contextInfoPtr, void *buffer, int length )
	{
	CRYPT_DEVICE iCryptDevice;
	CRYPTOAPI_INFO *cryptoapiInfo;
	CRYPT_ALGO_TYPE cryptAlgo;
	STREAM stream;
	HCRYPTHASH hHash;
	BYTE tempBuffer[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE hashBuffer[ CRYPT_MAX_HASHSIZE + 8 ], *bufPtr = buffer;
	ALG_ID algID;
	DWORD resultLength = length;
	BOOL result;
	int i, status;

	/* CryptoAPI adds its own PKCS #1 padding, so we have to reverse-engineer
	   the encoding and padding that's already been added */
	assert( bufPtr[ 0 ] == 0 && bufPtr[ 1 ] == 1 );
	for( i = 2; i < length; i++ )
		{
		if( bufPtr[ i ] == 0 )
			break;
		}
	i++;	/* Skip final 0 byte */
	sMemConnect( &stream, bufPtr + i, length - i );
	status = readMessageDigest( &stream, &cryptAlgo, hashBuffer, 
								CRYPT_MAX_HASHSIZE, &i );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	algID = cryptlibToCapiID( cryptAlgo );
	if( algID == 0 )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Get the information for the device associated with this context */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &cryptoapiInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* CryptoAPI can only sign hash values inside hash objects, luckily 
	   there's a function whose sole purpose is to allow hash values to be 
	   loaded directly into hash objects that we can use to create the hash
	   to be signed.
	   
	   Once we've got the hash though, things get tricky.  CryptSignHash() 
	   doesn't let you specify which key you want to use for signing, but it 
	   does let you indicate that you want to use the default encryption-only 
	   key as your signature key.  This is sort of like building a car where 
	   the steering wheel won't turn, but in compensation the fuel tank will
	   explode if you hit the horn */
	result = pCryptCreateHash( cryptoapiInfo->hProv, algID, 0, 0, &hHash );
	if( !result )
		{
		status = mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED );
		krnlReleaseObject( iCryptDevice );
		return( status );
		}
	result = pCryptSetHashParam( hHash, HP_HASHVAL, hashBuffer, 0 );
	if( result )
		result = pCryptSignHash( hHash, AT_KEYEXCHANGE, NULL, 0, 
								 tempBuffer, &resultLength );
	pCryptDestroyHash( hHash );
	if( result )
		{
		copyMPI( buffer, tempBuffer, resultLength, resultLength );
		zeroise( tempBuffer, CRYPT_MAX_PKCSIZE + 8 );

		/* Restore any truncated leading zeroes if necessary */
		if( resultLength < length )
			{
			const int delta = length - resultLength;
	
			memmove( ( BYTE * ) buffer + delta, tempBuffer, resultLength );
			memset( buffer, 0, delta );
			}
		}
	if( !result )
		status = mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED );

	krnlReleaseObject( iCryptDevice );
	return( status );
	}

static int rsaVerify( CONTEXT_INFO *contextInfoPtr, void *buffer, int length )
	{
	/* This function is present but isn't used as part of any normal 
	   operation because cryptlib does the same thing much faster in 
	   software and more importanyly because there's no way to get at the
	   hash information that's needed to create the hash object, so that
	   the trick used in rsaSign() won't work */
	retIntError();
	}

static int rsaEncrypt( CONTEXT_INFO *contextInfoPtr, void *buffer, int length )
	{
	BYTE tempBuffer[ CRYPT_MAX_PKCSIZE + 8 ], *tempPtr, *bufPtr = buffer;
	DWORD resultLength;
	int i;

	/* CryptoAPI adds its own PKCS #1 padding, so we have to undo the 
	   padding that's already been added */
	assert( bufPtr[ 0 ] == 0 && bufPtr[ 1 ] == 2 );
	for( i = 2; i < length; i++ )
		{
		if( bufPtr[ i ] == 0 )
			break;
		}
	i++;	/* Skip final 0 byte */

	/* Change the data into the little-endian order required by CryptoAPI, 
	   encrypt it, reverse the order again */
	tempPtr = copyMPI( tempBuffer, bufPtr + i, length - i, length - i );
	resultLength = tempPtr - tempBuffer;
	if( !pCryptEncrypt( contextInfoPtr->deviceObject, 0, TRUE, 0, 
						tempBuffer, &resultLength, length ) )
		{
		zeroise( tempBuffer, CRYPT_MAX_PKCSIZE + 8 );
		return( mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED ) );
		}
	copyMPI( buffer, tempBuffer, resultLength, resultLength );
	zeroise( tempBuffer, CRYPT_MAX_PKCSIZE + 8 );

	/* Restore any truncated leading zeroes if necessary */
	if( resultLength < length )
		{
		const int delta = length - resultLength;

		memmove( ( BYTE * ) buffer + delta, tempBuffer, resultLength );
		memset( buffer, 0, delta );
		}
	return( CRYPT_OK );
	}

static int rsaDecrypt( CONTEXT_INFO *contextInfoPtr, void *buffer, int length )
	{
	BYTE tempBuffer[ CRYPT_MAX_PKCSIZE + 8 ], *tempPtr, *bufPtr = buffer;
	DWORD resultLength;
	int i;

	/* Change the data into the little-endian order required by CryptoAPI, 
	   decrypt it, and reverse the order again */
	tempPtr = copyMPI( tempBuffer, bufPtr, length, length );
	resultLength = tempPtr - tempBuffer;
	if( !pCryptDecrypt( contextInfoPtr->deviceObject, 0, FALSE, 0, 
						tempBuffer, &resultLength ) )
		{
		zeroise( tempBuffer, CRYPT_MAX_PKCSIZE + 8 );
		return( mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED ) );
		}
	copyMPI( buffer, tempBuffer, resultLength, resultLength );
	zeroise( tempBuffer, CRYPT_MAX_PKCSIZE + 8 );

	/* Redo the PKCS #1 padding that was stripped by CryptoAPI */
	memmove( bufPtr + length - resultLength, bufPtr, resultLength );
	bufPtr[ 0 ] = 0;
	bufPtr[ 1 ] = 2;
	for( i = 2; i < length - resultLength - 1; i++ )
		bufPtr[ i ] = 0xA5;
	bufPtr[ i ] = 0;
	assert( i + 1 + resultLength == length );
	return( CRYPT_OK );
	}

/* DSA algorithm-specific mapping functions */

#if 0

static int dsaSetKeyInfo( DEVICE_INFO *deviceInfo, CONTEXT_INFO *contextInfoPtr, 
//						  const CK_OBJECT_HANDLE hPrivateKey,
//						  const CK_OBJECT_HANDLE hPublicKey,
						  const void *p, const int pLen,
						  const void *q, const int qLen,
						  const void *g, const int gLen,
						  const void *y, const int yLen )
	{
	MESSAGE_DATA msgData;
	BYTE keyDataBuffer[ ( CRYPT_MAX_PKCSIZE * 2 ) + 8 ];
	BYTE idBuffer[ KEYID_SIZE + 8 ];
	int keyDataSize, cryptStatus;

	/* Send the public key data to the context.  We send the keying 
	   information as CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL rather than 
	   CRYPT_IATTRIBUTE_KEY_SPKI since the latter transitions the context 
	   into the high state.  We don't want to do this because we're already 
	   in the middle of processing a message that does this on completion, 
	   all we're doing here is sending in encoded public key data for use by 
	   objects such as certificates */
	cryptStatus = keyDataSize = writeFlatPublicKey( NULL, 0, CRYPT_ALGO_DSA, 0,
													p, pLen, q, qLen, 
													g, gLen, y, yLen );
	if( !cryptStatusError( cryptStatus ) )
		cryptStatus = writeFlatPublicKey( keyDataBuffer, CRYPT_MAX_PKCSIZE * 2,
										  CRYPT_ALGO_DSA, 0, p, pLen, q, qLen, 
										  g, gLen, y, yLen );
	if( !cryptStatusError( cryptStatus ) )
		{
		setMessageData( &msgData, keyDataBuffer, keyDataSize );
		cryptStatus = krnlSendMessage( contextInfoPtr->objectHandle, 
									   IMESSAGE_SETATTRIBUTE_S, &msgData, 
									   CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL );
		}
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Remember what we've set up */
	krnlSendMessage( contextInfoPtr->objectHandle, IMESSAGE_SETATTRIBUTE,
					 ( MESSAGE_CAST ) &hPrivateKey, 
					 CRYPT_IATTRIBUTE_DEVICEOBJECT );

	/* Get the key ID from the context and use it as the object ID.  Since 
	   some objects won't allow after-the-even ID updates, we don't treat a
	   failure to update as an error */
	setMessageData( &msgData, idBuffer, KEYID_SIZE );
	cryptStatus = krnlSendMessage( contextInfoPtr->objectHandle, 
								   IMESSAGE_GETATTRIBUTE_S, &msgData, 
								   CRYPT_IATTRIBUTE_KEYID );
	if( cryptStatusOK( cryptStatus ) )
		{
		CK_ATTRIBUTE idTemplate = { CKA_ID, msgData.data, msgData.length };

		if( hPublicKey != CRYPT_UNUSED )
			{
			C_SetAttributeValue( cryptoapiInfo->hProv, hPublicKey, 
								 &idTemplate, 1 );
			}
		C_SetAttributeValue( cryptoapiInfo->hProv, hPrivateKey, 
							 &idTemplate, 1 );
		}
	
	return( cryptStatus );
	return( CRYPT_ERROR );
	}
#endif /* 0 */

static int dsaInitKey( CONTEXT_INFO *contextInfoPtr, const void *key, 
					   const int keyLength )
	{
	CRYPT_DEVICE iCryptDevice;
	CRYPTOAPI_INFO *cryptoapiInfo;
	CRYPT_PKCINFO_DLP *dlpKey = ( CRYPT_PKCINFO_DLP * ) key;
	HCRYPTKEY hKey;
	BLOBHEADER *blobHeaderPtr;
	DSSPUBKEY *pubKeyPtr;
	BYTE keyBlob[ ( CRYPT_MAX_PKCSIZE * 4 ) + 8 ], *keyBlobPtr;
	BOOL result;
	const int pLen = bitsToBytes( dlpKey->pLen );
	int status;

	/* CryptoAPI sets some awkward constraints on key formats, only allowing
	   a prime size up to 1024 bits, so we have to check whether the key 
	   components are in this form before we can try and load the key */
	if( dlpKey->pLen > 1024 )
		return( CRYPT_ERROR_BADDATA );

	/* Get the information for the device associated with this context */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &cryptoapiInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Set up the blob header:

		Ofs	Value
		  0	BLOBHEADER blobheader {
			  0	BYTE bType;
			  1	BYTE bVersion;
			  2 WORD reserved;
			  4	ALG_ID aiKeyAlg; }
		  8	DSSPUBKEY_VER3 dsspubkey {
			  8 DWORD magic;
			 12	DWORD bitlen; }
		 16	BYTE p[ 128 ];
			BYTE q[ 20 ];
			BYTE g[ 128 ];
			BYTE y[ 128 ]; */
	memset( keyBlob, 0, CRYPT_MAX_PKCSIZE * 4 );
	blobHeaderPtr = ( BLOBHEADER * ) keyBlob;
	blobHeaderPtr->bType = intToByte( ( dlpKey->isPublicKey ) ? \
									  PUBLICKEYBLOB : PRIVATEKEYBLOB );
	blobHeaderPtr->bVersion = CUR_BLOB_VERSION;
	blobHeaderPtr->aiKeyAlg = CALG_DSS_SIGN;

	/* Set up the public-key components.  CryptoAPI requires that the 
	   modulus length be a multiple of 8 bits, so we have to round up the 
	   length to the nearest byte boundary */
	pubKeyPtr = ( DSSPUBKEY * ) ( keyBlob + BLOBHEADER_SIZE );
	pubKeyPtr->magic = 0x31535344;
	pubKeyPtr->bitlen = roundUp( dlpKey->pLen, 8 );
	keyBlobPtr = keyBlob + BLOBHEADER_SIZE + ( 2 * sizeof( DWORD ) );
	keyBlobPtr = copyMPI( keyBlobPtr, dlpKey->p, pLen, 128 );
	keyBlobPtr = copyMPI( keyBlobPtr, dlpKey->q, 
						  bitsToBytes( dlpKey->qLen ), 20 );
	keyBlobPtr = copyMPI( keyBlobPtr, dlpKey->g, 
						  bitsToBytes( dlpKey->gLen ), 128 );
	keyBlobPtr = copyMPI( keyBlobPtr, dlpKey->y, 
						  bitsToBytes( dlpKey->yLen ), 128 );

	/* Set up the private-key components if necessary */
	if( !dlpKey->isPublicKey )
		{
		}

	/* Import the key blob and clean up */
	result = pCryptImportKey( cryptoapiInfo->hProv, keyBlob, 
							  keyBlobPtr - keyBlob, 0, 0, &hKey );
	if( result )
		{
		/* Note that the following will break under Win64 since the hKey is
		   a 64-bit pointer while the deviceObject is a 32-bit unsigned 
		   value, this code is experimental and only enabled for Win32 debug 
		   so this isn't a problem at the moment */
		contextInfoPtr->deviceObject = ( long ) hKey;
		}
	else
		status = mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED );
	zeroise( keyBlob, CRYPT_MAX_PKCSIZE * 4 );

	krnlReleaseObject( iCryptDevice );
	return( status );
	}

static int dsaGenerateKey( CONTEXT_INFO *contextInfoPtr, const int keysizeBits )
	{
#if 0
	static const CK_MECHANISM mechanism = { CKM_DSA_KEY_PAIR_GEN, NULL_PTR, 0 };
	static const CK_BBOOL bTrue = TRUE;
	const CK_ULONG modulusBits = keysizeBits;
	CK_ATTRIBUTE privateKeyTemplate[] = {
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SENSITIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_SIGN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		};
	CK_ATTRIBUTE publicKeyTemplate[] = {
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_VERIFY, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_PRIME, NULL, 0 },
		{ CKA_SUBPRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 },
		};
	CK_ATTRIBUTE yValueTemplate = { CKA_VALUE, NULL, CRYPT_MAX_PKCSIZE * 2 };
	CK_OBJECT_HANDLE hPublicKey, hPrivateKey;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	CRYPT_DEVICE iCryptDevice;
	BYTE pubkeyBuffer[ ( CRYPT_MAX_PKCSIZE * 2 ) + 8 ], label[ 8 + 8 ];
	CK_RV status;
	STREAM stream;
	long length;
	int keyLength = bitsToBytes( keysizeBits ), cryptStatus;

	/* CKM_DSA_KEY_PAIR_GEN is really a Clayton's key generation mechanism 
	   since it doesn't actually generate the p, q, or g values (presumably 
	   it dates back to the original FIPS 186 shared domain parameters idea).
	   Because of this we'd have to generate half the key ourselves in a 
	   native context, then copy portions from the native context over in 
	   flat form and complete the keygen via the device.  The easiest way to
	   do this is to create a native DSA context, generate a key, grab the
	   public portions, and destroy the context again (i.e. generate a full
	   key on a superscalar 2GHz RISC CPU, then throw half of it away, and 
	   regenerate it on a 5MHz 8-bit tinkertoy).  Since the keygen can take 
	   awhile and doesn't require the device, we do it before we grab the 
	   device */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_DSA );
	cryptStatus = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								   IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
								   OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	setMessageData( &msgData, label, 8 );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
					 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE_S,
					 &msgData, CRYPT_CTXINFO_LABEL );
	krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
					 ( int * ) &keyLength, CRYPT_CTXINFO_KEYSIZE );
	cryptStatus = krnlSendMessage( createInfo.cryptHandle, 
								   IMESSAGE_CTX_GENKEY, NULL, FALSE );
	if( cryptStatusOK( cryptStatus ) )
		{
		setMessageData( &msgData, pubkeyBuffer, CRYPT_MAX_PKCSIZE * 2 );
		cryptStatus = krnlSendMessage( createInfo.cryptHandle, 
									   IMESSAGE_GETATTRIBUTE_S, &msgData, 
									   CRYPT_IATTRIBUTE_KEY_SPKI );
		}
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Set up the public key information by extracting the flat values from 
	   the SubjectPublicKeyInfo.  Note that the data used is represented in
	   DER-canonical form, there may be PKCS #11 implementations that
	   can't handle this (for example they may require q to be zero-padded
	   to make it exactly 20 bytes rather than (say) 19 bytes if the high
	   byte is zero) */
	sMemConnect( &stream, pubkeyBuffer, msgData.length );
	readSequence( &stream, NULL );				/* SEQUENCE */
	readSequence( &stream, NULL );					/* SEQUENCE */
	readUniversal( &stream );							/* OID */
	readSequence( &stream, NULL );						/* SEQUENCE */
	readGenericHole( &stream, &length );					/* p */
	publicKeyTemplate[ 3 ].pValue = sMemBufPtr( &stream );
	publicKeyTemplate[ 3 ].ulValueLen = length;
	sSkip( &stream, length, MAX_INTLENGTH_SHORT );
	readGenericHole( &stream, &length );					/* q */
	publicKeyTemplate[ 4 ].pValue = sMemBufPtr( &stream );
	publicKeyTemplate[ 4 ].ulValueLen = length;
	sSkip( &stream, length, MAX_INTLENGTH_SHORT );
	readGenericHole( &stream, &length );					/* g */
	publicKeyTemplate[ 5 ].pValue = sMemBufPtr( &stream );
	publicKeyTemplate[ 5 ].ulValueLen = length;
	assert( sStatusOK( &stream ) );
	sMemDisconnect( &stream );

	/* Get the information for the device associated with this context */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &cryptoapiInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Generate the keys */
	status = C_GenerateKeyPair( cryptoapiInfo->hProv,
								( CK_MECHANISM_PTR ) &mechanism,
								( CK_ATTRIBUTE_PTR ) publicKeyTemplate, 5,
								( CK_ATTRIBUTE_PTR ) privateKeyTemplate, 4,
								&hPublicKey, &hPrivateKey );
	cryptStatus = mapError( deviceInfo, status, CRYPT_ERROR_FAILED );
	if( cryptStatusError( cryptStatus ) )
		{
		krnlReleaseObject( iCryptDevice );
		return( cryptStatus );
		}

	/* Read back the generated y value, send the public key information to 
	   the context, and set up the key ID info.  The odd two-phase y value 
	   read is necessary for buggy implementations that fail if the given 
	   size isn't exactly the same as the data size */
	status = C_GetAttributeValue( cryptoapiInfo->hProv, hPublicKey,
								  &yValueTemplate, 1 );
	if( status == CKR_OK )
		{
		yValueTemplate.pValue = pubkeyBuffer;
		status = C_GetAttributeValue( cryptoapiInfo->hProv, hPublicKey, 
									  &yValueTemplate, 1 );
		}
	cryptStatus = mapError( deviceInfo, status, CRYPT_ERROR_FAILED );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = dsaSetKeyInfo( deviceInfo, contextInfoPtr, 
			hPrivateKey, hPublicKey,
			publicKeyTemplate[ 3 ].pValue, publicKeyTemplate[ 3 ].ulValueLen, 
			publicKeyTemplate[ 4 ].pValue, publicKeyTemplate[ 4 ].ulValueLen, 
			publicKeyTemplate[ 5 ].pValue, publicKeyTemplate[ 5 ].ulValueLen,
			yValueTemplate.pValue, yValueTemplate.ulValueLen );
	if( cryptStatusError( cryptStatus ) )
		{
		C_DestroyObject( cryptoapiInfo->hProv, hPublicKey );
		C_DestroyObject( cryptoapiInfo->hProv, hPrivateKey );
		}

	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
#endif /* 0 */
	return( CRYPT_ERROR );
	}

static int dsaSign( CONTEXT_INFO *contextInfoPtr, void *buffer, int length )
	{
#if 0
	static const CK_MECHANISM mechanism = { CKM_DSA, NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) buffer;
	BIGNUM *r, *s;
	BYTE signature[ 40 + 8 ];
	int cryptStatus;

	assert( length == sizeof( DLP_PARAMS ) );
	assert( dlpParams->inParam1 != NULL && \
			dlpParams->inLen1 == 20 );
	assert( dlpParams->inParam2 == NULL && dlpParams->inLen2 == 0 );
	assert( dlpParams->outParam != NULL && \
			dlpParams->outLen >= ( 2 + 20 ) * 2 );

	/* Get the information for the device associated with this context */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &cryptoapiInfo );
	if( cryptStatusError( status ) )
		return( status );
	cryptStatus = genericSign( deviceInfo, contextInfoPtr, &mechanism, 
							   dlpParams->inParam1, dlpParams->inLen1,
							   signature, 40 );
	krnlReleaseObject( iCryptDevice );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Encode the result as a DL data block.  We have to do this as via 
	   bignums, but this isn't a big deal since DSA signing via tokens is
	   almost never used */
	r = BN_new();
	s = BN_new();
	if( r != NULL && s != NULL )
		{
		BN_bin2bn( signature, 20, r );
		BN_bin2bn( signature + 20, 20, s );
		cryptStatus = encodeDLValues( dlpParams->outParam, dlpParams->outLen, 
									  r, s, dlpParams->formatType );
		if( !cryptStatusError( cryptStatus ) )
			{
			dlpParams->outLen = cryptStatus;
			cryptStatus = CRYPT_OK;	/* encodeDLValues() returns a byte count */
			}
		BN_clear_free( s );
		BN_clear_free( r );
		}
	return( cryptStatus );
#endif /* 0 */
	return( CRYPT_ERROR );
	}

static int dsaVerify( CONTEXT_INFO *contextInfoPtr, void *buffer, int length )
	{
#if 0
	static const CK_MECHANISM mechanism = { CKM_DSA, NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) buffer;
	BIGNUM *r, *s;
	BYTE signature[ 40 + 8 ];
	int cryptStatus;

	/* This function is present but isn't used as part of any normal 
	   operation because cryptlib does the same thing much faster in 
	   software and because some tokens don't support public-key 
	   operations */

	assert( length == sizeof( DLP_PARAMS ) );
	assert( dlpParams->inParam1 != NULL && dlpParams->inLen1 == 20 );
	assert( dlpParams->inParam2 != NULL && \
			( ( dlpParams->formatType == CRYPT_FORMAT_CRYPTLIB && \
				dlpParams->inLen2 >= 46 ) || \
			  ( dlpParams->formatType == CRYPT_FORMAT_PGP && \
				dlpParams->inLen2 == 44 ) || \
				( dlpParams->formatType == CRYPT_IFORMAT_SSH && \
				dlpParams->inLen2 == 40 ) ) );
	assert( dlpParams->outParam == NULL && dlpParams->outLen == 0 );

	/* Decode the values from a DL data block and make sure r and s are
	   valid */
	cryptStatus = decodeDLValues( dlpParams->inParam2, dlpParams->inLen2, 
								  &r, &s, dlpParams->formatType );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* This code can never be called, since DSA public-key contexts are 
	   always native contexts */
	retIntError();

	/* Get the information for the device associated with this context */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &cryptoapiInfo );
	if( cryptStatusError( status ) )
		return( status );
	cryptStatus = genericVerify( deviceInfo, contextInfoPtr, &mechanism, buffer,
								 20, signature, 40 );
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
#endif /* 0 */
	return( CRYPT_ERROR );
	}

/* Conventional cipher-specific mapping functions */

static int cipherInitKey( CONTEXT_INFO *contextInfoPtr, const void *key, 
						  const int keyLength )
	{
	CRYPT_DEVICE iCryptDevice;
	CRYPTOAPI_INFO *cryptoapiInfo;
	HCRYPTKEY hSessionKey;
	int keySize = keyLength, status;

	/* Get the information for the device associated with this context */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &cryptoapiInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Copy the key to internal storage */
	if( contextInfoPtr->ctxConv->userKey != key )
		memcpy( contextInfoPtr->ctxConv->userKey, key, keyLength );
	contextInfoPtr->ctxConv->userKeyLength = keyLength;

	/* Special-case handling for 2-key vs.3-key 3DES */
	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_3DES )
		{
		/* If the supplied key contains only two DES keys, adjust the key to
		   make it the equivalent of 3-key 3DES.  In addition since the
		   nominal keysize is for 2-key 3DES, we have to make the actual size
		   the maximum size, corresponding to 3-key 3DES */
		if( keyLength <= bitsToBytes( 64 * 2 ) )
			memcpy( contextInfoPtr->ctxConv->userKey + bitsToBytes( 64 * 2 ),
					contextInfoPtr->ctxConv->userKey, bitsToBytes( 64 ) );
		keySize = contextInfoPtr->capabilityInfo->maxKeySize;
		}

	/* Import the key via the hideous decrypt-with-exponent-one RSA key 
	   kludge */
	status = importPlainKey( cryptoapiInfo->hProv, 
							 cryptoapiInfo->hPrivateKey, 
							 cryptoapiInfo->privateKeySize, &hSessionKey, 
							 contextInfoPtr->capabilityInfo->cryptAlgo, 
							 key, keySize, contextInfoPtr );
	if( cryptStatusOK( status ) )
		{
		/* Note that the following will break under Win64 since the hKey is
		   a 64-bit pointer while the deviceObject is a 32-bit unsigned 
		   value, this code is experimental and only enabled for Win32 debug 
		   so this isn't a problem at the moment */
		contextInfoPtr->deviceObject = ( long ) hSessionKey;
		}

	krnlReleaseObject( iCryptDevice );
	return( status );
	}

/* Set up algorithm-specific encryption parameters */

static int initCryptParams( CONTEXT_INFO *contextInfoPtr )
	{
	enum { CAPI_CRYPT_MODE_NONE, CAPI_CRYPT_MODE_CBC, 
		   CAPI_CRYPT_MODE_ECB, CAPI_CRYPT_MODE_CTR,
		   CAPI_CRYPT_MODE_CFB };
	const CRYPT_MODE_TYPE mode = contextInfoPtr->ctxConv->mode;
	DWORD dwMode;

	/* If it's a native stream cipher (rather than a block cipher being run
	   in stream cipher mode), there's nothing to do */
	if( isStreamCipher( contextInfoPtr->capabilityInfo->cryptAlgo ) )
		return( CRYPT_OK );

	/* Make sure that the values from the CryptoAPI header aren't 
	   overriding the cryptlib values */
	assert( CRYPT_MODE_ECB == 1 );
	assert( CRYPT_MODE_CBC == 2 );
	assert( CRYPT_MODE_CFB == 3 );
	assert( CRYPT_MODE_GCM == 4 );

	/* CryptoAPI uses the same mode names as cryptlib but different values, 
	   so we have to override the naming with our own names here and then 
	   map the cryptlib values to the CryptoAPI ones */
	switch( mode )
		{
		case CRYPT_MODE_ECB:
			dwMode = CAPI_CRYPT_MODE_ECB;
			break;
		case CRYPT_MODE_CBC:
			dwMode = CAPI_CRYPT_MODE_CBC;
			break;
		case CRYPT_MODE_CFB:
			dwMode = CAPI_CRYPT_MODE_CFB;
			break;
		default:
			retIntError();
		}
		
	/* Set the mode parameter for the CryptoAPI object */
	if( !CryptSetKeyParam( contextInfoPtr->deviceObject, KP_MODE,
						   ( BYTE * ) &dwMode, 0 ) )
		return( mapDeviceError( contextInfoPtr, CRYPT_ERROR_NOTAVAIL ) );
	if( mode == CRYPT_MODE_CFB )
		{
		const DWORD dwModeBits = contextInfoPtr->capabilityInfo->blockSize * 8;

		/* CryptoAPI defaults to 8-bit feedback for CFB and OFB (!!) so we 
		   have to fix the feedback amount if we're using a stream mode */
		if( !CryptSetKeyParam( contextInfoPtr->deviceObject, KP_MODE_BITS,
							   ( BYTE * ) &dwModeBits, 0 ) )
			return( mapDeviceError( contextInfoPtr, CRYPT_ERROR_NOTAVAIL ) );
		}

	/* If there's no IV present, we're done */
	if( mode == CRYPT_MODE_ECB )
		return( CRYPT_OK );

	/* Set the IV parameter for the CryptoAPI object */
	if( !CryptSetKeyParam( contextInfoPtr->deviceObject, KP_IV,
						   contextInfoPtr->ctxConv->currentIV, 0 ) )
		return( mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED ) );

	return( CRYPT_OK );
	}

/* En/decrypt/hash data */

static int cipherEncrypt( CONTEXT_INFO *contextInfoPtr, void *buffer, int length )
	{
	DWORD resultLength = length;

	/* Finalise the encryption parameters if necessary.  We couldn't do this
	   earlier because the device-level object isn't instantiated until the 
	   key is loaded */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY_INITED ) )
		{
		int status;

		status = initCryptParams( contextInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		contextInfoPtr->flags |= CONTEXT_FLAG_DUMMY_INITED;
		}

	/* Encrypt the data.  We always set the bFinal flag to FALSE since 
	   setting it to TRUE tries to apply message padding, resets the IV, and 
	   various other unwanted side-effects */
	if( !pCryptEncrypt( contextInfoPtr->deviceObject, 0, FALSE, 0, buffer, 
						&resultLength, length ) )
		return( mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED ) );
	return( CRYPT_OK );
	}

static int cipherDecrypt( CONTEXT_INFO *contextInfoPtr, void *buffer, int length )
	{
	DWORD resultLength = length;

	/* Finalise the encryption parameters if necessary.  We couldn't do this
	   earlier because the device-level object isn't instantiated until the 
	   key is loaded */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY_INITED ) )
		{
		int status;

		status = initCryptParams( contextInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		contextInfoPtr->flags |= CONTEXT_FLAG_DUMMY_INITED;
		}

	/* Decrypt the data.  We always set the bFinal flag to FALSE since 
	   setting it to TRUE tries to process message padding, resets the IV, 
	   and various other unwanted side-effects */
	if( !pCryptDecrypt( contextInfoPtr->deviceObject, 0, FALSE, 0, buffer, 
						&resultLength ) )
		return( mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED ) );
	return( CRYPT_OK );
	}

#if 0	/* Not used, see the comment in the capability information */

static int hashFunction( CONTEXT_INFO *contextInfoPtr, void *buffer, int length )
	{
	if( !pCryptHashData( contextInfoPtr->deviceObject, buffer, length, 0 ) )
		return( mapDeviceError( contextInfoPtr, CRYPT_ERROR_FAILED ) );
	return( CRYPT_OK );
	}
#endif /* 0 */

/****************************************************************************
*																			*
*						 	Device Capability Routines						*
*																			*
****************************************************************************/

/* Templates for the various capabilities.  These contain only basic 
   information, the remaining fields are filled in when the capability is 
   set up */

static CAPABILITY_INFO FAR_BSS capabilityTemplates[] = {
	/* Encryption capabilities */
	{ CRYPT_ALGO_3DES, bitsToBytes( 64 ), "3DES", 4,
		bitsToBytes( 64 + 8 ), bitsToBytes( 128 ), bitsToBytes( 192 ) },
	{ CRYPT_ALGO_RC2, bitsToBytes( 64 ), "RC2", 3,
		bitsToBytes( 40 ), bitsToBytes( 128 ), bitsToBytes( 1024 ) },
	{ CRYPT_ALGO_RC4, bitsToBytes( 8 ), "RC4", 3,
		bitsToBytes( 40 ), bitsToBytes( 128 ), 256 },
	{ CRYPT_ALGO_AES, bitsToBytes( 128 ), "AES", 3,
		bitsToBytes( 128 ), bitsToBytes( 128 ), bitsToBytes( 256 ) },

	/* Hash capabilities */
	{ CRYPT_ALGO_MD5, bitsToBytes( 128 ), "MD5", 3,
		bitsToBytes( 0 ), bitsToBytes( 0 ), bitsToBytes( 0 ) },
	{ CRYPT_ALGO_SHA1, bitsToBytes( 160 ), "SHA1", 3,
		bitsToBytes( 0 ), bitsToBytes( 0 ), bitsToBytes( 0 ) },

	/* Public-key capabilities */
	{ CRYPT_ALGO_RSA, bitsToBytes( 0 ), "RSA", 3,
		bitsToBytes( 512 ), bitsToBytes( 1024 ), CRYPT_MAX_PKCSIZE },
	{ CRYPT_ALGO_DSA, bitsToBytes( 0 ), "DSA", 3,
		bitsToBytes( 512 ), bitsToBytes( 1024 ), CRYPT_MAX_PKCSIZE },

	/* Hier ist der Mast zu ende */
	{ CRYPT_ERROR }, { CRYPT_ERROR }
	};

/* Mapping of CryptoAPI provider capabilities to cryptlib capabilities */

typedef struct {
	/* Mapping information from CryptoAPI to cryptlib algorithms.  For some
	   PKC algorithms CryptoAPI creates two virtual algorithm types (badly,
	   it's easily confused between the two), one for signing and one for 
	   encryption.  The first algorithm type is always the one with 
	   encryption capability, if there's one with signature capability or 
	   it's a signature-only algorithm we specify it as the optional 
	   alternative algorithm type */
	const ALG_ID algoID;				/* CryptoAPI algorithm type */
	const ALG_ID altAlgoID;				/* CryptoAPI alt.algorithm type */
	const CRYPT_ALGO_TYPE cryptAlgo;	/* cryptlib algo and mode */
	const CRYPT_MODE_TYPE cryptMode;

	/* Function pointers */
	int ( *endFunction )( CONTEXT_INFO *contextInfoPtr );
	int ( *initKeyFunction )( CONTEXT_INFO *contextInfoPtr, const void *key, const int keyLength );
	int ( *generateKeyFunction )( CONTEXT_INFO *contextInfoPtr, const int keySizeBits );
	int ( *encryptFunction )( CONTEXT_INFO *contextInfoPtr, void *buffer, int length );
	int ( *decryptFunction )( CONTEXT_INFO *contextInfoPtr, void *buffer, int length );
	int ( *signFunction )( CONTEXT_INFO *contextInfoPtr, void *buffer, int length );
	int ( *sigCheckFunction )( CONTEXT_INFO *contextInfoPtr, void *buffer, int length );
	} MECHANISM_INFO;

static const MECHANISM_INFO mechanismInfo[] = {
	{ CALG_RSA_KEYX, CALG_RSA_SIGN, CRYPT_ALGO_RSA, CRYPT_MODE_NONE, 
	  NULL, rsaInitKey, rsaGenerateKey, 
	  rsaEncrypt, rsaDecrypt, rsaSign, rsaVerify },
	{ CALG_NONE, CALG_DSS_SIGN, CRYPT_ALGO_DSA, CRYPT_MODE_NONE, 
	  NULL, dsaInitKey, dsaGenerateKey, 
	  NULL, NULL, dsaSign, dsaVerify },
	{ CALG_DES, CALG_NONE, CRYPT_ALGO_DES, CRYPT_MODE_ECB, 
	  genericEndFunction, cipherInitKey, NULL, 
	  cipherEncrypt, cipherDecrypt, NULL, NULL },
	{ CALG_DES, CALG_NONE, CRYPT_ALGO_DES, CRYPT_MODE_CBC, 
	  genericEndFunction, cipherInitKey, NULL, 
	  cipherEncrypt, cipherDecrypt, NULL, NULL },
	{ CALG_3DES, CALG_NONE, CRYPT_ALGO_3DES, CRYPT_MODE_ECB, 
	  genericEndFunction, cipherInitKey, NULL, 
	  cipherEncrypt, cipherDecrypt, NULL, NULL },
	{ CALG_3DES, CALG_NONE, CRYPT_ALGO_3DES, CRYPT_MODE_CBC, 
	  genericEndFunction, cipherInitKey, NULL, 
	  cipherEncrypt, cipherDecrypt, NULL, NULL },
	{ CALG_RC2, CALG_NONE, CRYPT_ALGO_RC2, CRYPT_MODE_ECB, 
	  genericEndFunction, cipherInitKey, NULL, 
	  cipherEncrypt, cipherDecrypt, NULL, NULL },
	{ CALG_RC2, CALG_NONE, CRYPT_ALGO_RC2, CRYPT_MODE_CBC, 
	  genericEndFunction, cipherInitKey, NULL, 
	  cipherEncrypt, cipherDecrypt, NULL, NULL },
	{ CALG_RC4, CALG_NONE, CRYPT_ALGO_RC4, CRYPT_MODE_CFB, 
	  genericEndFunction, cipherInitKey, NULL, 
	  cipherEncrypt, cipherDecrypt, NULL, NULL },
#if 0	/* Although CAPI supports the hash mechanisms, as with PKCS #11
		   we always use cryptlib native contexts for this */
	{ CALG_MD5, CALG_NONE, CRYPT_ALGO_MD5, CRYPT_MODE_NONE,
	  genericEndFunction, NULL, NULL, 
	  hashFunction, hashFunction, NULL, NULL },
	{ CALG_SHA1, CALG_NONE, CRYPT_ALGO_SHA1, CRYPT_MODE_NONE,
	  genericEndFunction, NULL, NULL, 
	  hashFunction, hashFunction, NULL, NULL },
#endif /* 0 */
	{ CALG_NONE, CALG_NONE, CRYPT_ALGO_NONE, CRYPT_MODE_NONE },
		{ CALG_NONE, CALG_NONE, CRYPT_ALGO_NONE, CRYPT_MODE_NONE }
	};

/* Fill out a capability information based on CryptoAPI algorithm 
   information */

static CAPABILITY_INFO *addCapability( const DEVICE_INFO *deviceInfo,
									   const PROV_ENUMALGS_EX *capiAlgoInfo,
									   const MECHANISM_INFO *mechanismInfoPtr,
									   const CAPABILITY_INFO *existingCapabilityInfo )
	{
	VARIABLE_CAPABILITY_INFO *capabilityInfo = \
					( VARIABLE_CAPABILITY_INFO * ) existingCapabilityInfo;
	int i;

	/* If it's a new capability, copy across the template for this 
	   capability */
	if( capabilityInfo == NULL )
		{
		if( ( capabilityInfo = \
				clAlloc( "addCapability", sizeof( CAPABILITY_INFO ) ) ) == NULL )
			return( NULL );
		for( i = 0; 
			 capabilityTemplates[ i ].cryptAlgo != mechanismInfoPtr->cryptAlgo && \
				capabilityTemplates[ i ].cryptAlgo != CRYPT_ALGO_NONE && \
				i < FAILSAFE_ARRAYSIZE( capabilityTemplates, CAPABILITY_INFO ); \
			 i++ );
		if( i >= FAILSAFE_ARRAYSIZE( capabilityTemplates, CAPABILITY_INFO ) )
			retIntError_Null();
		assert( i < sizeof( capabilityTemplates ) / sizeof( CAPABILITY_INFO ) && \
				capabilityTemplates[ i ].cryptAlgo != CRYPT_ALGO_NONE );
		memcpy( capabilityInfo, &capabilityTemplates[ i ],
				sizeof( CAPABILITY_INFO ) );
		}

	/* Set up the keysize information, limiting the maximum key size to 
	   match the cryptlib native max.key size, both for consistency and 
	   because cryptlib performs buffer allocation based on the maximum 
	   native buffer size.  Since CryptoAPI specifies key sizes for unkeyed 
	   hash algorithms, we only set the keysize if there's really a key
	   present.  In addition it indicates the number of bits involved in 
	   keying rather than the nominal key size, so we have to adjust the
	   reported size to match the conventionally-used value */
	if( capabilityInfo->keySize > 0 )
		{
		int minKeySize = bitsToBytes( capiAlgoInfo->dwMinLen );
		int maxKeySize = bitsToBytes( capiAlgoInfo->dwMaxLen );

		if( mechanismInfoPtr->cryptAlgo == CRYPT_ALGO_DES && \
			minKeySize == 7 )
			{
			/* Adjust 56 bits -> 8 bytes */
			minKeySize = maxKeySize = 8;
			}
		if( mechanismInfoPtr->cryptAlgo == CRYPT_ALGO_3DES && \
			minKeySize == 21 )
			{
			/* Adjust 168 bits -> 24 bytes */
			minKeySize = maxKeySize = 24;
			}
		if( minKeySize > capabilityInfo->minKeySize )
			capabilityInfo->minKeySize = minKeySize;
		if( capabilityInfo->keySize < capabilityInfo->minKeySize )
			capabilityInfo->keySize = capabilityInfo->minKeySize;
		capabilityInfo->maxKeySize = min( maxKeySize, 
										  capabilityInfo->maxKeySize );
		if( capabilityInfo->keySize > capabilityInfo->maxKeySize )
			capabilityInfo->keySize = capabilityInfo->maxKeySize;
		capabilityInfo->endFunction = genericEndFunction;
		}

	/* Set up the device-specific handlers */
	capabilityInfo->getInfoFunction = getDefaultInfo;
	if( mechanismInfoPtr->cryptAlgo != CRYPT_ALGO_RSA && \
		mechanismInfoPtr->cryptAlgo != CRYPT_ALGO_DSA )
		capabilityInfo->initParamsFunction = initGenericParams;
	capabilityInfo->endFunction = mechanismInfoPtr->endFunction;
	capabilityInfo->initKeyFunction = mechanismInfoPtr->initKeyFunction;
	capabilityInfo->generateKeyFunction = mechanismInfoPtr->generateKeyFunction;
	if( mechanismInfoPtr->algoID == capiAlgoInfo->aiAlgid )
		{
		if( mechanismInfoPtr->cryptMode == CRYPT_MODE_CFB )
			{
			/* Stream ciphers have an implicit mode of CFB */
			capabilityInfo->encryptCFBFunction = mechanismInfoPtr->encryptFunction;
			}
		else
			capabilityInfo->encryptFunction = mechanismInfoPtr->encryptFunction;
		if( mechanismInfoPtr->cryptMode == CRYPT_MODE_CFB )
			{
			/* Stream ciphers have an implicit mode of CFB */
			capabilityInfo->decryptCFBFunction = mechanismInfoPtr->decryptFunction;
			}
		else
			capabilityInfo->decryptFunction = mechanismInfoPtr->decryptFunction;
		if( mechanismInfoPtr->cryptMode != CRYPT_MODE_NONE )
			{
			capabilityInfo->encryptCBCFunction = \
										mechanismInfoPtr->encryptFunction;
			capabilityInfo->decryptCBCFunction = \
										mechanismInfoPtr->decryptFunction;
#if 0		/* CAPI requires the encryption of full blocks even in a stream 
			   cipher mode, which doesn't match the standard cryptlib 
			   behaviour.  To avoid this problem, we mark the stream cipher
			   modes as not available */
			capabilityInfo->encryptCFBFunction = \
										mechanismInfoPtr->encryptFunction;
			capabilityInfo->decryptCFBFunction = \
										mechanismInfoPtr->decryptFunction;
			capabilityInfo->encryptCTRFunction = \
										mechanismInfoPtr->encryptFunction;
			capabilityInfo->decryptCTRFunction = \
										mechanismInfoPtr->decryptFunction;
#endif /* 0 */
			}
		}
	if( mechanismInfoPtr->altAlgoID == capiAlgoInfo->aiAlgid )
		{
		capabilityInfo->signFunction = mechanismInfoPtr->signFunction;
		capabilityInfo->sigCheckFunction = mechanismInfoPtr->sigCheckFunction;
		}

	return( ( CAPABILITY_INFO * ) capabilityInfo );
	}

/* Set the capability information based on device capabilities.  Since
   CryptoAPI devices can have assorted capabilities, we have to build this 
   up on the fly rather than using a fixed table like the built-in 
   capabilities */

static void freeCapabilities( DEVICE_INFO *deviceInfo )
	{
	CAPABILITY_INFO_LIST *capabilityInfoListPtr = \
				( CAPABILITY_INFO_LIST * ) deviceInfo->capabilityInfoList;

	/* If the list was empty, return now */
	if( capabilityInfoListPtr == NULL )
		return;
	deviceInfo->capabilityInfoList = NULL;

	while( capabilityInfoListPtr != NULL )
		{
		CAPABILITY_INFO_LIST *listItemToFree = capabilityInfoListPtr;
		CAPABILITY_INFO *itemToFree = ( CAPABILITY_INFO * ) listItemToFree->info;

		capabilityInfoListPtr = capabilityInfoListPtr->next;
		zeroise( itemToFree, sizeof( CAPABILITY_INFO ) );
		clFree( "freeCapabilities", itemToFree );
		zeroise( listItemToFree, sizeof( CAPABILITY_INFO_LIST ) );
		clFree( "freeCapabilities", listItemToFree );
		}
	}

static int getCapabilities( DEVICE_INFO *deviceInfo )
	{
	CRYPTOAPI_INFO *cryptoapiInfo = deviceInfo->deviceCryptoAPI;
	CAPABILITY_INFO_LIST *capabilityInfoListTail = \
				( CAPABILITY_INFO_LIST * ) deviceInfo->capabilityInfoList;
	PROV_ENUMALGS_EX capiAlgoInfo;
	DWORD length = sizeof( PROV_ENUMALGS_EX );
	int iterationCount = 0;

	assert( sizeof( CAPABILITY_INFO ) == sizeof( VARIABLE_CAPABILITY_INFO ) );

	/* Step through each available CryptoAPI algorithm type adding the 
	   appropriate cryptlib capability for it */
	if( !pCryptGetProvParam( cryptoapiInfo->hProv, PP_ENUMALGS_EX, 
							 ( BYTE * ) &capiAlgoInfo, &length, CRYPT_FIRST ) )
		return( CRYPT_ERROR );
	do
		{
		CAPABILITY_INFO_LIST *newCapabilityList, *capabilityInfoListPtr;
		CAPABILITY_INFO *newCapability;
		CRYPT_ALGO_TYPE cryptAlgo;
		int i;

		/* Check whether this algorithm type corresponds to a cryptlib
		   capability */
		for( i = 0; mechanismInfo[ i ].cryptAlgo != CRYPT_ALGO_NONE && \
					i < FAILSAFE_ARRAYSIZE( mechanismInfo, MECHANISM_INFO ); 
			 i++ )
			{
			if( mechanismInfo[ i ].algoID == capiAlgoInfo.aiAlgid || \
				( mechanismInfo[ i ].altAlgoID != CALG_NONE && \
				  mechanismInfo[ i ].altAlgoID == capiAlgoInfo.aiAlgid ) )
				break;
			}
		if( i >= FAILSAFE_ARRAYSIZE( mechanismInfo, MECHANISM_INFO ) )
			retIntError();
		if( mechanismInfo[ i ].cryptAlgo == CRYPT_ALGO_NONE )
			continue;
		cryptAlgo = mechanismInfo[ i ].cryptAlgo;

		/* Check whether this is a variation of an existing capability */
		for( capabilityInfoListPtr = ( CAPABILITY_INFO_LIST * ) \
									 deviceInfo->capabilityInfoList; 
			 capabilityInfoListPtr != NULL && \
				capabilityInfoListPtr->info->cryptAlgo != cryptAlgo; 
			 capabilityInfoListPtr = capabilityInfoListPtr->next );
		if( capabilityInfoListPtr != NULL )
			{
			addCapability( deviceInfo, &capiAlgoInfo, &mechanismInfo[ i ], 
						   capabilityInfoListPtr->info );
			continue;
			}

		/* Add capabilities for all mechanisms corresponding to the current
		   CryptoAPI algorithm type.  If the assertion below triggers then 
		   the CryptoAPI provider is broken since it's returning 
		   inconsistent information such as illegal key length data, 
		   conflicting algorithm information, etc etc.  This assertion is 
		   included here to detect buggy drivers early on rather than 
		   forcing users to step through the CryptoAPI glue code to find out 
		   why an operation is failing.
		   
		   Because some providers mapped down to tinkertoy smart cards 
		   support only the bare minimum functionality (e.g.RSA private key 
		   ops and nothing else), we allow asymmetric functionality for 
		   PKCs */
		newCapability = addCapability( deviceInfo, &capiAlgoInfo, 
									   &mechanismInfo[ i ], NULL );
		if( newCapability == NULL )
			break;
		REQUIRES( sanityCheckCapability( newCapability ) );
		if( ( newCapabilityList = \
						clAlloc( "getCapabilities", \
								 sizeof( CAPABILITY_INFO_LIST ) ) ) == NULL )
			{
			clFree( "getCapabilities", newCapability );
			continue;
			}
		newCapabilityList->info = newCapability;
		newCapabilityList->next = NULL;
		if( deviceInfo->capabilityInfoList == NULL )
			deviceInfo->capabilityInfoList = newCapabilityList;
		else
			capabilityInfoListTail->next = newCapabilityList;
		capabilityInfoListTail = newCapabilityList;
		}
	while( pCryptGetProvParam( cryptoapiInfo->hProv, PP_ENUMALGS_EX, 
							   ( BYTE * ) &capiAlgoInfo, &length, 0 ) && \
		   iterationCount++ < FAILSAFE_ITERATIONS_LARGE );
	if( iterationCount >= FAILSAFE_ITERATIONS_LARGE )
		retIntError();

	return( ( deviceInfo->capabilityInfoList == NULL ) ? CRYPT_ERROR : CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Device Access Routines							*
*																			*
****************************************************************************/

/* Mechanisms supported by CryptoAPI devices.  These are actually cryptlib 
   native mechanisms since many aren't supported by CryptoAPI, but not the 
   full set supported by the system device since functions like private key 
   export aren't available except in the nonstandard blob format invented 
   by Microsoft.  The list is sorted in order of frequency of use in order 
   to make lookups a bit faster */

static const FAR_BSS MECHANISM_FUNCTION_INFO mechanismFunctions[] = {
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_PKCS1, ( MECHANISM_FUNCTION ) exportPKCS1 },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_PKCS1, ( MECHANISM_FUNCTION ) importPKCS1 },
	{ MESSAGE_DEV_SIGN, MECHANISM_SIG_PKCS1, ( MECHANISM_FUNCTION ) signPKCS1 },
	{ MESSAGE_DEV_SIGCHECK, MECHANISM_SIG_PKCS1, ( MECHANISM_FUNCTION ) sigcheckPKCS1 },
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_PKCS1_RAW, ( MECHANISM_FUNCTION ) exportPKCS1 },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_PKCS1_RAW, ( MECHANISM_FUNCTION ) importPKCS1 },
#ifdef USE_PGP
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_PKCS1_PGP, ( MECHANISM_FUNCTION ) exportPKCS1PGP },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_PKCS1_PGP, ( MECHANISM_FUNCTION ) importPKCS1PGP },
#endif /* USE_PGP */
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_CMS, ( MECHANISM_FUNCTION ) exportCMS },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_CMS, ( MECHANISM_FUNCTION ) importCMS },
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PKCS5, ( MECHANISM_FUNCTION ) derivePKCS5 },
#if defined( USE_PGP ) || defined( USE_PGPKEYS )
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PGP, ( MECHANISM_FUNCTION ) derivePGP },
#endif /* USE_PGP || USE_PGPKEYS */
#ifdef USE_SSL
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_SSL, ( MECHANISM_FUNCTION ) deriveSSL },
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_TLS, ( MECHANISM_FUNCTION ) deriveTLS },
	{ MESSAGE_DEV_SIGN, MECHANISM_SIG_SSL, ( MECHANISM_FUNCTION ) signSSL },
	{ MESSAGE_DEV_SIGCHECK, MECHANISM_SIG_SSL, ( MECHANISM_FUNCTION ) sigcheckSSL },
#endif /* USE_SSL */
#ifdef USE_CMP
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_CMP, ( MECHANISM_FUNCTION ) deriveCMP },
#endif /* USE_CMP */
#ifdef USE_PKCS12
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PKCS12, ( MECHANISM_FUNCTION ) derivePKCS12 },
#endif /* USE_PKCS12 */
	{ MESSAGE_NONE, MECHANISM_NONE, NULL }, { MESSAGE_NONE, MECHANISM_NONE, NULL }
	};

/* Set up the function pointers to the device methods */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setDeviceCryptoAPI( INOUT DEVICE_INFO *deviceInfo )
	{
	/* Make sure that the CryptoAPI driver DLL is loaded */
	if( hCryptoAPI == NULL_HINSTANCE )
		return( CRYPT_ERROR_OPEN );

	deviceInfo->initFunction = initFunction;
	deviceInfo->shutdownFunction = shutdownFunction;
	deviceInfo->controlFunction = controlFunction;
	deviceInfo->getItemFunction = getItemFunction;
	deviceInfo->setItemFunction = setItemFunction;
	deviceInfo->deleteItemFunction = deleteItemFunction;
	deviceInfo->getFirstItemFunction = getFirstItemFunction;
	deviceInfo->getNextItemFunction = getNextItemFunction;
	deviceInfo->getRandomFunction = getRandomFunction;
	deviceInfo->mechanismFunctions = mechanismFunctions;
	deviceInfo->mechanismFunctionCount = \
		FAILSAFE_ARRAYSIZE( mechanismFunctions, MECHANISM_FUNCTION_INFO );

	return( CRYPT_OK );
	}
#endif /* USE_CRYPTOAPI */
