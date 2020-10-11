/****************************************************************************
*																			*
*						Certificate Sanity-check Routines					*
*						Copyright Peter Gutmann 1996-2016					*
*																			*
****************************************************************************/

#include "crypt.h"
#ifdef INC_ALL
  #include "cert.h"
#else
  #include "cert/cert.h"
#endif /* Compiler-specific includes */

#if defined( USE_CERTIFICATES )

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Check serial number storage */

CHECK_RETVAL_BOOL \
static BOOLEAN checkSerialNumber( IN_OPT const void *serialNumber,
								  IN_OPT const void *serialNumberBuffer,
								  IN_LENGTH_SHORT_Z const int serialNumberSize )
	{
	ENSURES_B( isShortIntegerRange( serialNumberSize ) );

	/* If there's no serial number present, we're done */
	if( serialNumber == NULL )
		return( ( serialNumberSize == 0 ) ? TRUE : FALSE );

	/* The serial number has to have a nonzero length */
	if( serialNumberSize <= 0 )
		return( FALSE );

	/* If the serial number fits into the internal buffer then it has to be 
	   stored there */
	if( serialNumberSize <= SERIALNO_BUFSIZE )
		return( ( serialNumber == serialNumberBuffer ) ? TRUE : FALSE );

	/* It's an over-long serial number, it's held in dynamically-allocated 
	   storage, not the internal buffer */
	if( serialNumber == serialNumberBuffer || \
		serialNumberSize > MAX_SERIALNO_SIZE )
		return( FALSE );

	return( TRUE );
	}

/* Check data pointers within objects */

CHECK_RETVAL_BOOL \
static BOOLEAN checkDataPointers( IN_OPT const void *certificatePtr,
								  IN_LENGTH_Z const int certificateSize,
								  IN_OPT const void *dataPtr,
								  IN_OPT const void *dataStoragePtr,
								  IN_LENGTH_SHORT_Z const int dataSize,
								  IN const DATAPTR dataObject )
	{
	assert( ( certificatePtr == NULL && certificateSize == 0 ) || \
			isReadPtr( certificatePtr, certificateSize ) );
	assert( ( dataPtr == NULL ) || isReadPtr( dataPtr, dataSize ) );
			/* dataPtr may be NULL with dataSize non-zero if it's a DN stored
			   in the data object */

	REQUIRES_B( isIntegerRange( certificateSize ) );
	REQUIRES_B( isShortIntegerRange( dataSize ) );

	/* Checking for data pointers gets somewhat complicated, we can always 
	   have the value present as an object reference (a pointer to a decoded 
	   DN or a context handle) but alongside this we can also have a pointer 
	   to encoded certificate data (dataPtr) or in separately-allocated 
	   memory (dataStoragePtr).  The latter is only required when there's no 
	   certificate data present, once this has been instantiated the dataPtr
	   points into the certificate data and the dataStoragePtr is freed:

		Certificate		dataPtr			dataStoragePtr		Object ref.
		-----------		-------			--------------		-----------
		  NULL			  NULL				NULL			  ---
		 Present		Inside cert.		NULL			  ---
		  NULL			== dataStgePtr	  Present			  --- */

	/* If there's no certificate present then the information has to be 
	   either stored in the dataStoragePtr or as an object reference */
	if( certificatePtr == NULL )
		{
		/* If there's no storage allocated for it, all values have to be 
		   zero */
		if( dataStoragePtr == NULL )
			{
			/* If there's a non-zero data size present then it's due to the
			   encoding of a DN stored in the data object, so there must be
			   data present in the data object */
			if( dataSize > 0 )
				{
				if( !DATAPTR_ISSET( dataObject ) )
					return( FALSE );
				}
			else
				{
				/* This is a redundant check, the precondition was that
				   dataSize >= 0, so if it's not > 0 as per the check above
				   then it must be zero, but we leave the check in to make
				   this explicit */
				ENSURES_B( dataSize == 0 );
				}
			if( dataPtr != NULL )
				return( FALSE );

			return( TRUE );
			}
		
		/* There's storage allocated, it has to match the dataPtr */
		if( dataPtr != dataStoragePtr || \
			!isShortIntegerRangeNZ( dataSize ) )
			return( FALSE );

		return( TRUE );
		}

	/* There's certificate data present, the data pointer has to be 
	   contained within it */
	if( !pointerBoundsCheck( certificatePtr, certificateSize,
							 dataPtr, dataSize ) )
		return( FALSE );
	if( dataStoragePtr != NULL )
		return( FALSE );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Check Certifcate Subtype Data						*
*																			*
****************************************************************************/

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckCertificate( const CERT_INFO *certInfoPtr )
	{
	const CERT_CERT_INFO *certInfo = certInfoPtr->cCertCert;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( certInfo, sizeof( CERT_CERT_INFO ) ) );

	/* Check certificate data */
	if( !checkSerialNumber( certInfo->serialNumber, 
							certInfo->serialNumberBuffer,
							certInfo->serialNumberLength ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Serial number" ));
		return( FALSE );
		}
	if( certInfo->maxCheckLevel < CRYPT_COMPLIANCELEVEL_OBLIVIOUS || \
		certInfo->maxCheckLevel >= CRYPT_COMPLIANCELEVEL_LAST )
		{
		DEBUG_PUTS(( "sanityCheckCert: Compliance level" ));
		return( FALSE );
		}
	if( certInfo->chainEnd != 0 || certInfo->chainPos != CRYPT_ERROR )
		{
		/* It's a certificate chain, check that the chain information is in 
		   order.  A chainPos of -1 is valid for the leaf certificate, which 
		   is past the end of the chain */
		if( certInfo->chainEnd < 0 || \
			certInfo->chainEnd >= MAX_CHAINLENGTH || \
			certInfo->chainPos < -1 || \
			certInfo->chainPos > certInfo->chainEnd )
			{
			DEBUG_PUTS(( "sanityCheckCert: Chain info" ));
			return( FALSE );
			}
		}
	if( certInfo->hashAlgo != CRYPT_ALGO_NONE && \
		!isHashAlgo( certInfo->hashAlgo ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Hash algorithm" ));
		return( FALSE );
		}

	return( TRUE );
	}

#ifdef USE_CERTREQ

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckCertReq( const CERT_INFO *certInfoPtr )
	{
	const CERT_REQ_INFO *reqInfo = certInfoPtr->cCertReq;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( reqInfo, sizeof( CERT_REQ_INFO ) ) );

	/* Check request data */
	if( !checkSerialNumber( reqInfo->serialNumber, 
							reqInfo->serialNumberBuffer,
							reqInfo->serialNumberLength ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Serial number" ));
		return( FALSE );
		}
	if( reqInfo->requestFromRA != TRUE && \
		reqInfo->requestFromRA != FALSE )
		{
		DEBUG_PUTS(( "sanityCheckCert: RA flag" ));
		return( FALSE );
		}

	return( TRUE );
	}
#endif /* USE_CERTREQ */

#ifdef USE_CERTREV

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckCertRev( const CERT_INFO *certInfoPtr )
	{
	const CERT_REV_INFO *revInfo = certInfoPtr->cCertRev;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( revInfo, sizeof( CERT_REV_INFO ) ) );

	/* Check revocation data */
	if( !( ( revInfo->responderUrl == NULL && \
			 revInfo->responderUrlSize == 0 ) || \
		   ( revInfo->responderUrl != NULL && \
			 revInfo->responderUrlSize >= MIN_URL_SIZE && \
			 revInfo->responderUrlSize <= MAX_URL_SIZE ) ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Revocation responder URL" ));
		return( FALSE );
		}
	if( revInfo->hashAlgo != CRYPT_ALGO_NONE && \
		!isHashAlgo( revInfo->hashAlgo ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Revocation hash algorithm" ));
		return( FALSE );
		}
	if( !isEnumRangeOpt( revInfo->signatureLevel, CRYPT_SIGNATURELEVEL ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Revocation signature level" ));
		return( FALSE );
		}
	if( !DATAPTR_ISVALID( revInfo->revocations ) || \
		!DATAPTR_ISVALID( revInfo->currentRevocation ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Revocation safe pointers" ));
		return( FALSE );
		}
	if( DATAPTR_ISSET( revInfo->revocations ) )
		{
		const REVOCATION_INFO *revInfoPtr;

		revInfoPtr = DATAPTR_GET( revInfo->revocations );
		ENSURES_B( revInfoPtr != NULL );
		if( !sanityCheckRevInfo( revInfoPtr ) )
			{
			DEBUG_PUTS(( "sanityCheckCert: Revocation info" ));
			return( FALSE );
			}
		}
	if( DATAPTR_ISSET( revInfo->currentRevocation ) )
		{
		const REVOCATION_INFO *revInfoPtr;

		revInfoPtr = DATAPTR_GET( revInfo->currentRevocation );
		ENSURES_B( revInfoPtr != NULL );
		if( !sanityCheckRevInfo( revInfoPtr ) )
			{
			DEBUG_PUTS(( "sanityCheckCert: Revocation current revocation" ));
			return( FALSE );
			}
		}

	return( TRUE );
	}
#endif /* USE_CERTREV */

#ifdef USE_CERTVAL

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckCertVal( const CERT_INFO *certInfoPtr )
	{
	const CERT_VAL_INFO *valInfo = certInfoPtr->cCertVal;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( valInfo, sizeof( CERT_VAL_INFO ) ) );

	/* Check RTCS data */
	if( !( ( valInfo->responderUrl == NULL && \
			 valInfo->responderUrlSize == 0 ) || \
		   ( valInfo->responderUrl != NULL && \
			 valInfo->responderUrlSize >= MIN_URL_SIZE && \
			 valInfo->responderUrlSize <= MAX_URL_SIZE ) ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Validity responder URL" ));
		return( FALSE );
		}
	if( !isEnumRangeOpt( valInfo->responseType, RTCSRESPONSE_TYPE ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Validity response type" ));
		return( FALSE );
		}
	if( !DATAPTR_ISVALID( valInfo->validityInfo ) || \
		!DATAPTR_ISVALID( valInfo->currentValidity ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Validity safe pointers" ));
		return( FALSE );
		}
	if( DATAPTR_ISSET( valInfo->validityInfo ) )
		{
		const VALIDITY_INFO *valInfoPtr;

		valInfoPtr = DATAPTR_GET( valInfo->validityInfo );
		ENSURES_B( valInfoPtr != NULL );
		if( !sanityCheckValInfo( valInfoPtr ) )
			{
			DEBUG_PUTS(( "sanityCheckCert: Validity info" ));
			return( FALSE );
			}
		}
	if( DATAPTR_ISSET( valInfo->currentValidity ) )
		{
		const VALIDITY_INFO *valInfoPtr;

		valInfoPtr = DATAPTR_GET( valInfo->currentValidity );
		ENSURES_B( valInfoPtr != NULL );
		if( !sanityCheckValInfo( valInfoPtr ) )
			{
			DEBUG_PUTS(( "sanityCheckCert: Validity current validity" ));
			return( FALSE );
			}
		}

	return( TRUE );
	}
#endif /* USE_CERTVAL */

#ifdef USE_PKIUSER

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckCertPKIUser( const CERT_INFO *certInfoPtr )
	{
	const CERT_PKIUSER_INFO *pkiUserInfo = certInfoPtr->cCertUser;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( pkiUserInfo, sizeof( CERT_PKIUSER_INFO ) ) );

	/* Check PKI user data.  The only thing that we can really check is a 
	   single boolean */
	if( pkiUserInfo->isRA != TRUE && pkiUserInfo->isRA != FALSE )
		{
		DEBUG_PUTS(( "sanityCheckCert: pkiUser RA flag" ));
		return( FALSE );
		}

	return( TRUE );
	}
#endif /* USE_PKIUSER */

/****************************************************************************
*																			*
*							Check Certifcate Data							*
*																			*
****************************************************************************/

/* Sanity-check a certificate */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckCert( IN const CERT_INFO *certInfoPtr )
	{
	BOOLEAN subjectNameSpecialCheck = FALSE;
#ifdef USE_CERTLEVEL_PKIX_FULL
	BOOLEAN nullDN = FALSE;
#endif /* USE_CERTLEVEL_PKIX_FULL */

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* Check general certificate data */
	if( !isEnumRange( certInfoPtr->type, CRYPT_CERTTYPE ) || \
		!CHECK_FLAGS( certInfoPtr->flags, CERT_FLAG_NONE, 
					  CERT_FLAG_MAX ) || \
		certInfoPtr->version < 0 || \
		certInfoPtr->version > 3 )
		{
		DEBUG_PUTS(( "sanityCheckCert: General info" ));
		return( FALSE );
		}
	if( certInfoPtr->certificate == NULL )
		{
		if( certInfoPtr->certificateSize != 0 )
			{
			DEBUG_PUTS(( "sanityCheckCert: Empty certificate data" ));
			return( FALSE );
			}
		}
	else
		{
		/* We allow a minimum size of 48 rather than MIN_CRYPT_OBJECTSIZE 
		   since RTCS requests/responses are very compact */
		if( certInfoPtr->certificateSize < 48 || \
			certInfoPtr->certificateSize >= MAX_INTLENGTH )
			{
			DEBUG_PUTS(( "sanityCheckCert: Certificate data" ));
			return( FALSE );
			}
		}

	/* Check safe pointers */
	if( !DATAPTR_ISVALID( certInfoPtr->subjectName ) || \
		!DATAPTR_ISVALID( certInfoPtr->issuerName ) || \
		!DATAPTR_ISVALID( certInfoPtr->attributes ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Safe pointers" ));
		return( FALSE );
		}

	/* Check key data */
	if( certInfoPtr->iPubkeyContext != CRYPT_ERROR && \
		!isHandleRangeValid( certInfoPtr->iPubkeyContext ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Public key" ));
		return( FALSE );
		}
	if( certInfoPtr->publicKeyAlgo != CRYPT_ALGO_NONE && \
		!isPkcAlgo( certInfoPtr->publicKeyAlgo ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Public key algorithm" ));
		return( FALSE );
		}
	if( certInfoPtr->publicKeyInfo == NULL )
		{
		if( certInfoPtr->publicKeyInfoSize != 0 )
			{
			DEBUG_PUTS(( "sanityCheckCert: Spurious public key info" ));
			return( FALSE );
			}
		}
	else
		{
		if( certInfoPtr->publicKeyInfoSize <= MIN_PKCSIZE_ECC || \
			certInfoPtr->publicKeyInfoSize >= MAX_INTLENGTH_SHORT )
			{
			DEBUG_PUTS(( "sanityCheckCert: Public key info" ));
			return( FALSE );
			}
		}

	/* Check DNs */
	if( certInfoPtr->subjectDNsize == 0 )
		{
		if( certInfoPtr->subjectDNptr != NULL || \
			certInfoPtr->subjectDNdata != NULL )
			{
			DEBUG_PUTS(( "sanityCheckCert: Empty subject DN" ));
			return( FALSE );
			}
		}
	else
		{
#ifdef USE_CERTLEVEL_PKIX_FULL
		/* Full PKIX allows null DNs (!!), so we have to handle the
		   situation where there's no DN given, with the size of a zero-
		   length SEQUENCE.
		   
		   There's an extra combination here in that once the certificate is 
		   created, we save a pointer to its encoded form, so it appears 
		   that there's a DN present even though it's a null DN.  To deal
		   with this we check for the special case of there being a pointer
		   to the encoded DN data that corresponds to a zero-length 
		   SEQUENCE */
		if( DATAPTR_ISNULL( certInfoPtr->subjectName ) && \
			certInfoPtr->subjectDNptr == NULL )
			{
			if( certInfoPtr->subjectDNsize != 2 )
				{
				DEBUG_PUTS(( "sanityCheckCert: Null subject DN" ));
				return( FALSE );
				}
			nullDN = TRUE;
			}
		if( certInfoPtr->subjectDNsize < MIN_DN_SIZE || \
			certInfoPtr->subjectDNsize >= MAX_INTLENGTH_SHORT )
			{
			if( !nullDN && DATAPTR_ISNULL( certInfoPtr->subjectName ) && \
				certInfoPtr->subjectDNptr != NULL && \
				!memcmp( certInfoPtr->subjectDNptr, "\x30\x00", 2 ) )
				nullDN = TRUE;
			if( !nullDN )
				{
				DEBUG_PUTS(( "sanityCheckCert: Non-NULL DN" ));
				return( FALSE );
				}
			}
#else
		if( DATAPTR_ISNULL( certInfoPtr->subjectName ) && \
			certInfoPtr->subjectDNptr == NULL )
			{
			DEBUG_PUTS(( "sanityCheckCert: Absent subject DN" ));
			return( FALSE );
			}
		if( certInfoPtr->subjectDNsize < MIN_DN_SIZE || \
			certInfoPtr->subjectDNsize >= MAX_INTLENGTH_SHORT )
			{
			DEBUG_PUTS(( "sanityCheckCert: Subject DN size" ));
			return( FALSE );
			}
#endif /* USE_CERTLEVEL_PKIX_FULL */
		}
	if( certInfoPtr->issuerDNsize == 0 )
		{
		if( certInfoPtr->issuerDNptr != NULL || \
			certInfoPtr->issuerDNdata != NULL )
			{
			DEBUG_PUTS(( "sanityCheckCert: Empty issuer DN" ));
			return( FALSE );
			}
		}
	else
		{
		if( DATAPTR_ISNULL( certInfoPtr->issuerName ) && \
			certInfoPtr->issuerDNptr == NULL )
			{
			DEBUG_PUTS(( "sanityCheckCert: Absent issuer DN" ));
			return( FALSE );
			}
		if( certInfoPtr->issuerDNsize < MIN_DN_SIZE || \
			certInfoPtr->issuerDNsize >= MAX_INTLENGTH_SHORT )
			{
			DEBUG_PUTS(( "sanityCheckCert: Issuer DN size" ));
			return( FALSE );
			}
		}

	/* Make sure that data pointers within the certificate are valid: If the 
	   public key or subject or issuer DN data is dynamically allocated then 
	   the encoded-value pointers have to point to the same memory location 
	   as the dynamic pointers, and if encoded certificate data is present 
	   then the encoded-value pointers have to point within it.

	   There are some special-case conditions in which these rules don't 
	   apply, both caused by CRMF, or CRMF in combination with CMP.  For a
	   CRMF request we can have both certificate data and a distinct subject 
	   DN present when we've received a CRMF request that doesn't have a DN 
	   included and for which the DN has been supplied externally by the CA, 
	   typically by copying it in from the PKI user info.  The arrangement
	   here is:

		Certificate	subjDNdata	subjDNptr		issDNdata	issDNptr
		-----------	---------	----------		---------	---------
		Present		Present		== subjDNdata	---			---

	   For CRMF revocation requests it gets even more awkward, in this case
	   we have a standalone subject DN that exists independently of the 
	   request because it's needed for CMP use (see the long comment in 
	   copyCertToRevRequest() in comp_cert.c).  The arrangement here is:

		Certificate	subjDNdata	subjDNptr		issDNdata	issDNptr
		-----------	---------	----------		---------	---------
		Present		Present		== subjDNdata	NULL		== certificate */
	if( ( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT || \
		  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION ) && \
		certInfoPtr->certificate != NULL )
		{
		/* It's a CRMF request with certificate data present, if there's a 
		   distinct subject DN present then don't perform sanity checks on
		   it since it doesn't work like standard subject DNs */
		if( certInfoPtr->subjectDNdata != NULL && \
			certInfoPtr->subjectDNptr == certInfoPtr->subjectDNdata )
			subjectNameSpecialCheck = TRUE;
		}
#ifdef USE_CERTLEVEL_PKIX_FULL
	if( !subjectNameSpecialCheck && !nullDN && \
		!checkDataPointers( certInfoPtr->certificate, certInfoPtr->certificateSize,
							certInfoPtr->subjectDNptr, certInfoPtr->subjectDNdata,
							certInfoPtr->subjectDNsize, certInfoPtr->subjectName ) )
#else
	if( !subjectNameSpecialCheck && \
		!checkDataPointers( certInfoPtr->certificate, certInfoPtr->certificateSize,
							certInfoPtr->subjectDNptr, certInfoPtr->subjectDNdata,
							certInfoPtr->subjectDNsize, certInfoPtr->subjectName ) )
#endif /* USE_CERTLEVEL_PKIX_FULL */
		{
		DEBUG_PUTS(( "sanityCheckCert: Subject DN data pointers" ));
		return( FALSE );
		}
	if( !checkDataPointers( certInfoPtr->certificate, certInfoPtr->certificateSize,
							certInfoPtr->issuerDNptr, certInfoPtr->issuerDNdata,
							certInfoPtr->issuerDNsize, certInfoPtr->issuerName ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Issuer DN data pointers" ));
		return( FALSE );
		}
	if( !checkDataPointers( certInfoPtr->certificate, certInfoPtr->certificateSize,
							certInfoPtr->publicKeyInfo, certInfoPtr->publicKeyData,
							certInfoPtr->publicKeyInfoSize, DATAPTR_NULL ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Public key data pointers" ));
		return( FALSE );
		}
	if( certInfoPtr->certificate != NULL && !subjectNameSpecialCheck )
		{
		if( 16 + certInfoPtr->subjectDNsize + certInfoPtr->issuerDNsize + \
				 certInfoPtr->publicKeyInfoSize > certInfoPtr->certificateSize )
			{
			DEBUG_PUTS(( "sanityCheckCert: Certificate component size" ));
			return( FALSE );
			}
		}

	/* Check associated handles */
	if( !isHandleRangeValid( certInfoPtr->objectHandle ) || \
		!( certInfoPtr->ownerHandle == DEFAULTUSER_OBJECT_HANDLE || \
		   isHandleRangeValid( certInfoPtr->ownerHandle ) ) )
		{
		DEBUG_PUTS(( "sanityCheckCert: Object handles" ));
		return( FALSE );
		}

	/* Check subtype-specific data */
	switch( certInfoPtr->type )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
#ifdef USE_ATTRCERT
		case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
#endif /* USE_ATTRCERT */
		case CRYPT_CERTTYPE_CERTCHAIN:
		case CRYPT_ICERTTYPE_CMS_CERTSET:
		case CRYPT_ICERTTYPE_SSL_CERTCHAIN:
			if( !sanityCheckCertificate( certInfoPtr ) )
				return( FALSE );
			break;

#ifdef USE_CERTREQ
		case CRYPT_CERTTYPE_REQUEST_CERT:
		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			if( !sanityCheckCertReq( certInfoPtr ) )
				return( FALSE );
			break;
#endif /* USE_CERTREQ */

#ifdef USE_CERTREV
		case CRYPT_CERTTYPE_CRL:
		case CRYPT_CERTTYPE_OCSP_REQUEST:
		case CRYPT_CERTTYPE_OCSP_RESPONSE:
		case CRYPT_ICERTTYPE_REVINFO:
			if( !sanityCheckCertRev( certInfoPtr ) )
				return( FALSE );
			break;
#endif /* USE_CERTREV */

#ifdef USE_CERTVAL
		case CRYPT_CERTTYPE_RTCS_REQUEST:
		case CRYPT_CERTTYPE_RTCS_RESPONSE:
			if( !sanityCheckCertVal( certInfoPtr ) )
				return( FALSE );
			break;
#endif /* USE_CERTVAL */

#ifdef USE_PKIUSER
		case CRYPT_CERTTYPE_PKIUSER:
			if( !sanityCheckCertPKIUser( certInfoPtr ) )
				return( FALSE );
			break;
#endif /* USE_PKIUSER */

#if defined( USE_CERTREQ ) || defined( USE_CMSATTR )
		case CRYPT_CERTTYPE_CERTREQUEST:
		case CRYPT_CERTTYPE_CMS_ATTRIBUTES:
			if( certInfoPtr->cCertCert != NULL )
				{
				DEBUG_PUTS(( "sanityCheckCert: Spurious certificate info" ));
				return( FALSE );
				}
			break;
#endif /* USE_CERTREQ || USE_CMSATTR */

		default:
			retIntError_Boolean();
		}

	/* The SELECTION_INFO and SELECTION_STATE are only used in 
	   cert/comp_curs.c and are checked there */

	return( TRUE );
	}
#endif /* USE_CERTIFICATES */
