/****************************************************************************
*																			*
*						Certificate Trust Manger Interface 					*
*						Copyright Peter Gutmann 1998-2015					*
*																			*
****************************************************************************/

#ifndef _TRUSTMGR_DEFINED

#define _TRUSTMGR_DEFINED

/* Prototypes for certificate trust management functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initTrustInfo( OUT_DATAPTR DATAPTR *trustInfoPtr );
void endTrustInfo( IN const DATAPTR trustInfo );
CHECK_RETVAL \
int addTrustEntry( IN const DATAPTR trustInfo, 
				   IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptCert,
				   IN_BUFFER_OPT( certObjectLength ) const void *certObject, 
				   IN_LENGTH_SHORT_Z const int certObjectLength,
				   const BOOLEAN addSingleCert );
RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deleteTrustEntry( IN const DATAPTR trustInfo, 
					  IN void *entryToDeletePtr );
CHECK_RETVAL_PTR \
void *findTrustEntry( IN const DATAPTR trustInfo, 
					  IN_HANDLE const CRYPT_CERTIFICATE iCryptCert,
					  const BOOLEAN getIssuerEntry );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getTrustedCert( INOUT void *trustInfoPtr,
					OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate );
CHECK_RETVAL_BOOL \
BOOLEAN trustedCertsPresent( IN const DATAPTR trustInfo );
CHECK_RETVAL \
int enumTrustedCerts( IN const DATAPTR trustInfo, 
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
