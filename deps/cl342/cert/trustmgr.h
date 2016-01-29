/****************************************************************************
*																			*
*						Certificate Trust Manger Interface 					*
*						Copyright Peter Gutmann 1998-2005					*
*																			*
****************************************************************************/

#ifndef _TRUSTMGR_DEFINED

#define _TRUSTMGR_DEFINED

/* Prototypes for certificate trust managemer functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initTrustInfo( OUT_PTR_COND void **trustInfoPtrPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void endTrustInfo( IN void *trustInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addTrustEntry( INOUT void *trustInfoPtr, 
				   IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptCert,
				   IN_BUFFER_OPT( certObjectLength ) const void *certObject, 
				   IN_LENGTH_SHORT_Z const int certObjectLength,
				   const BOOLEAN addSingleCert );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void deleteTrustEntry( INOUT void *trustInfoPtr, 
					   IN void *entryToDeletePtr );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
void *findTrustEntry( INOUT void *trustInfoPtr, 
					  IN_HANDLE const CRYPT_CERTIFICATE iCryptCert,
					  const BOOLEAN getIssuerEntry );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getTrustedCert( INOUT void *trustInfoPtrPtr,
					OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN trustedCertsPresent( TYPECAST( TRUST_INFO ** ) const void *trustInfoPtrPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int enumTrustedCerts( INOUT void *trustInfoPtr, 
					  IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptCtl,
					  IN_HANDLE_OPT const CRYPT_KEYSET iCryptKeyset );

/* If certificates aren't available, we have to no-op out the cert trust
   manager functions */

#ifndef USE_CERTIFICATES

#define initTrustInfo( trustInfoPtrPtr )	CRYPT_OK
#define endTrustInfo( trustInfoPtr )
#define addTrustEntry( trustInfoPtr, iCryptCert, certObject, \
					   certObjectLength, addSingleCert ) \
		CRYPT_ERROR_NOTAVAIL
#define deleteTrustEntry( trustInfoPtr, entryToDelete )
#define findTrustEntry( trustInfoPtr, cryptCert, getIssuerEntry ) \
		NULL
#define getTrustedCert( trustInfoPtr )		CRYPT_ERROR_NOTFOUND
#define enumTrustedCerts( trustInfoPtr, iCryptCtl, iCryptKeyset ) \
		CRYPT_ERROR_NOTFOUND

#endif /* USE_CERTIFICATES */

#endif /* _TRUSTMGR_DEFINED */
