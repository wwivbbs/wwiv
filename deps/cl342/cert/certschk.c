/****************************************************************************
*																			*
*					Certificate Signature Checking Routines					*
*						Copyright Peter Gutmann 1997-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

#if defined( USE_CERTREV ) || defined( USE_CERTVAL )

/* Generate an issuerID, a SHA-1 hash of the issuerAndSerialNumber needed 
   when storing/retrieving a certificate to/from a database keyset, which 
   can't handle the awkward heirarchical IDs usually used in certificates.  
   This is created by encoding the DN and serial number */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int generateCertID( IN_BUFFER( dnLength ) const void *dn, 
						   IN_LENGTH_SHORT const int dnLength,
						   IN_BUFFER( serialNumberLength ) \
								const void *serialNumber,
						   IN_LENGTH_SHORT const int serialNumberLength, 
						   OUT_BUFFER_FIXED_C( KEYID_SIZE ) BYTE *certID, 
						   IN_LENGTH_FIXED( KEYID_SIZE ) const int certIdLength )
	{
	HASHFUNCTION hashFunction;
	HASHINFO hashInfo;
	STREAM stream;
	BYTE buffer[ MAX_SERIALNO_SIZE + 8 + 8 ];
	int status;

	assert( isReadPtr( dn, dnLength ) );
	assert( isReadPtr( serialNumber, serialNumberLength ) && \
			serialNumberLength > 0 && \
			serialNumberLength <= MAX_SERIALNO_SIZE );
	assert( isWritePtr( certID, certIdLength ) );

	REQUIRES( serialNumberLength > 0 && \
			  serialNumberLength <= MAX_SERIALNO_SIZE );
	REQUIRES( certIdLength == KEYID_SIZE );

	/* Clear return value */
	memset( certID, 0, min( 16, certIdLength ) );

	/* Get the hash algorithm information */
	getHashParameters( CRYPT_ALGO_SHA1, 0, &hashFunction, NULL );

	/* Write the relevant information to a buffer and hash the data to get
	   the ID:

		SEQUENCE {
			issuer		DN,
			serial		INTEGER
			} */
	sMemOpen( &stream, buffer, MAX_SERIALNO_SIZE + 8 );
	status = writeSequence( &stream, 
							dnLength + sizeofInteger( serialNumber, \
													  serialNumberLength ) );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		return( status );
		}
	hashFunction( hashInfo, NULL, 0, buffer, stell( &stream ), 
				  HASH_STATE_START );
	hashFunction( hashInfo, NULL, 0, dn, dnLength, HASH_STATE_CONTINUE );
	sseek( &stream, 0 );
	status = writeInteger( &stream, serialNumber, serialNumberLength,
						   DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		{
		hashFunction( hashInfo, certID, certIdLength, buffer, 
					  stell( &stream ), HASH_STATE_END );
		}
	sMemClose( &stream );

	return( status );
	}
#endif /* USE_CERTREV || USE_CERTVAL */

/****************************************************************************
*																			*
*							Validity/Revocation Checking 					*
*																			*
****************************************************************************/

#if defined( USE_CERTREV ) || defined( USE_CERTVAL )

/* Check a certificate using an RTCS or OCSP responder */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkResponder( INOUT CERT_INFO *certInfoPtr,
						   IN_HANDLE const CRYPT_SESSION iCryptSession )
	{
	CRYPT_CERTIFICATE cryptResponse = DUMMY_INIT;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int sessionType, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptSession) );

	status = krnlSendMessage( iCryptSession, IMESSAGE_GETATTRIBUTE, 
							  &sessionType, CRYPT_IATTRIBUTE_SUBTYPE );
	if( cryptStatusError( status ) )
		return( status );

	REQUIRES( ( sessionType == SUBTYPE_SESSION_RTCS ) || \
			  ( sessionType == SUBTYPE_SESSION_OCSP ) );

	/* Create the request, add the certificate, and add the request to the
	   session */
	setMessageCreateObjectInfo( &createInfo,
							    ( sessionType == SUBTYPE_SESSION_RTCS ) ? \
									CRYPT_CERTTYPE_RTCS_REQUEST : \
									CRYPT_CERTTYPE_OCSP_REQUEST );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
					&certInfoPtr->objectHandle, CRYPT_CERTINFO_CERTIFICATE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptSession, IMESSAGE_SETATTRIBUTE,
					&createInfo.cryptHandle, CRYPT_SESSINFO_REQUEST );
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/* Activate the session and get the response information */
	status = krnlSendMessage( iCryptSession, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, CRYPT_SESSINFO_ACTIVE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptSession, IMESSAGE_GETATTRIBUTE,
								  &cryptResponse, CRYPT_SESSINFO_RESPONSE );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionType == SUBTYPE_SESSION_RTCS )
		{
		int certStatus;

		status = krnlSendMessage( cryptResponse, IMESSAGE_GETATTRIBUTE,
								  &certStatus, CRYPT_CERTINFO_CERTSTATUS );
		if( cryptStatusOK( status ) && \
			( certStatus != CRYPT_CERTSTATUS_VALID ) )
			status = CRYPT_ERROR_INVALID;
		}
	else
		{
		int revocationStatus;

		status = krnlSendMessage( cryptResponse, IMESSAGE_GETATTRIBUTE,
								  &revocationStatus,
								  CRYPT_CERTINFO_REVOCATIONSTATUS );
		if( cryptStatusOK( status ) && \
			( revocationStatus != CRYPT_OCSPSTATUS_NOTREVOKED ) )
			status = CRYPT_ERROR_INVALID;
		}
	krnlSendNotifier( cryptResponse, IMESSAGE_DECREFCOUNT );

	return( status );
	}

/* Check a certificate against a CRL stored in a keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkKeyset( INOUT CERT_INFO *certInfoPtr,
						IN_HANDLE const CRYPT_SESSION iCryptKeyset )
	{
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	BYTE issuerID[ KEYID_SIZE + 8 ];
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );
	REQUIRES( isHandleRangeValid( iCryptKeyset ) );

	/* Generate the issuerID for the certificate */
	status = generateCertID( certInfoPtr->issuerDNptr,
							 certInfoPtr->issuerDNsize,
							 certInfoPtr->cCertCert->serialNumber,
							 certInfoPtr->cCertCert->serialNumberLength,
							 issuerID, KEYID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	
	/* Check whether the object with this issuerID is present in the CRL.  
	   Since all that we're interested in is a yes/no answer we tell the 
	   keyset to perform a check only */
	setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_ISSUERID, issuerID, 
						   KEYID_SIZE, NULL, 0, KEYMGMT_FLAG_CHECK_ONLY );
	status = krnlSendMessage( iCryptKeyset, IMESSAGE_KEY_GETKEY,
							  &getkeyInfo, KEYMGMT_ITEM_REVOCATIONINFO );
	if( cryptStatusOK( status ) )
		{
		/* The certificate is present in the blacklist so it's an invalid
		   certificate */
		return( CRYPT_ERROR_INVALID );
		}
	if( status == CRYPT_ERROR_NOTFOUND )
		{
		/* The certificate isn't present in the blacklist so it's not 
		   revoked (although not necessarily valid either) */
		return( CRYPT_OK );
		}

	/* Some other type of error occurred */
	return( status );
	}
#endif /* USE_CERTREV || USE_CERTVAL */

/****************************************************************************
*																			*
*							Signature Checking Functions					*
*																			*
****************************************************************************/

/* Check a certificate against an issuer certificate.  The trustAnchorCheck 
   flag is used when we're checking an explicit trust anchor, for which we
   only need to check the signature if it's self-signed.  The 
   shortCircuitCheck flag is used when checking subject:issuer pairs inside 
   certificate chains, which have already been checked by the chain-handling 
   code so a full (re-)check isn't necessary any more */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 7, 8 ) ) \
int checkCertDetails( INOUT CERT_INFO *subjectCertInfoPtr,
					  INOUT_OPT CERT_INFO *issuerCertInfoPtr,
					  IN_HANDLE_OPT const CRYPT_CONTEXT iIssuerPubKey,
					  IN_OPT const X509SIG_FORMATINFO *formatInfo,
					  const BOOLEAN trustAnchorCheck,
					  const BOOLEAN shortCircuitCheck,
					  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
					  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType )
	{
	int status;

	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( issuerCertInfoPtr == NULL || \
			isWritePtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( formatInfo == NULL || \
			isReadPtr( formatInfo, sizeof( X509SIG_FORMATINFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( iIssuerPubKey == CRYPT_UNUSED || \
			  isHandleRangeValid( iIssuerPubKey ) );

	/* Perform a basic check for obvious invalidity issues */
	if( subjectCertInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
		subjectCertInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
		subjectCertInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
		{
		status = checkCertBasic( subjectCertInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If there's an issuer certificate present check the validity of the
	   subject certificate based on it.  If it's not present all that we can 
	   do is perform a pure signature check with the context */
	if( issuerCertInfoPtr != NULL )
		{
		status = checkCert( subjectCertInfoPtr, issuerCertInfoPtr, 
							shortCircuitCheck, errorLocus, errorType );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If the signature has already been checked or there's no signature-
	   check key present we're done.  The latter can occur when we're
	   checking a data-only certificate in a certificate chain.  This is 
	   safe because these certificates can only occur when we're reading 
	   them from an (implicitly trusted) private key store */
	if( ( subjectCertInfoPtr->flags & CERT_FLAG_SIGCHECKED ) || \
		iIssuerPubKey == CRYPT_UNUSED )
		return( CRYPT_OK );

	/* If we're checking an explicit trust anchor and the certificate isn't 
	   self-signed there's nothing further left to check */
	if( trustAnchorCheck && issuerCertInfoPtr != NULL && \
		!( issuerCertInfoPtr->flags & CERT_FLAG_SELFSIGNED ) )
		return( CRYPT_OK );

	/* If we're performing a standard check and it's an explicitly-trusted 
	   certificate we're done.  If we're performing a check of a certificate 
	   chain then the chain-handling code will have performed its own 
	   handling of trusted certificates/trust anchors so we don't peform a 
	   second check here */
	if( !shortCircuitCheck )
		{
		if( cryptStatusOK( \
				krnlSendMessage( subjectCertInfoPtr->ownerHandle, 
								 IMESSAGE_USER_TRUSTMGMT,
								 &subjectCertInfoPtr->objectHandle,
								 MESSAGE_TRUSTMGMT_CHECK ) ) )
			return( CRYPT_OK );
		}

	/* Check the signature on the certificate.  If there's a problem with 
	   the issuer's public key it'll be reported as a CRYPT_ARGERROR_NUM1,
	   which the caller has to convert into an appropriate error code */
	status = checkX509signature( subjectCertInfoPtr->certificate, 
								 subjectCertInfoPtr->certificateSize,
								 iIssuerPubKey, formatInfo );
	if( cryptStatusError( status ) )
		{
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
		MESSAGE_DATA msgData;
		BYTE subjectIssuerID[ CRYPT_MAX_HASHSIZE + 8 ];
		BYTE issuerSubjectID[ CRYPT_MAX_HASHSIZE + 8 ];
		int subjectIDlength, issuerIDlength;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

		/* There's one special-case situation in which we can get a 
		   signature-check failure that looks like data corruption and 
		   that's when a CA quietly changes its issuing key without changing 
		   anything else so that the certificates chain but the signature 
		   check produces garbage as output due to the use of the incorrect 
		   key.  Although it could be argued that a CA that does this is 
		   broken, we try and accomodate it by performing a backup check 
		   using keyIDs if the signature check produces garbled output.  
		   Because of the complete chaos present in keyIDs we can't do this 
		   by default (it would result in far too many false positives) but 
		   it's safe as a fallback at this point since we're about to report 
		   an error anyway and the worst that can happen is that we return 
		   a slightly inappropriate error message.
		   
		   If there's no issuer certificate provided we also have to exit at 
		   this point because we can't go any further without it */
		if( status != CRYPT_ERROR_BADDATA || issuerCertInfoPtr == NULL )
			return( status );

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
		/* Get the subject certificate's issuerID and the issuer 
		   certificate's subjectID.  We don't bother with the alternative 
		   awkward DN-based ID since what we're really interested in is the 
		   ID of the signing key and it's not worth the extra pain of dealing 
		   with these awkward cert IDs just to try and fix up a slight 
		   difference in error codes.  Since the overall error status at this 
		   point is CRYPT_ERROR_BADDATA we return that instead of any 
		   ephemeral status values returned from further function calls */
		setMessageData( &msgData, subjectIssuerID, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( subjectCertInfoPtr->objectHandle, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_BADDATA );
		issuerIDlength = msgData.length;
		setMessageData( &msgData, issuerSubjectID, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( issuerCertInfoPtr->objectHandle, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_BADDATA );
		subjectIDlength = msgData.length;

		/* If the keyIDs don't match then it's a signature error due to 
		   false-positive chaining rather than a data corruption error */
		return( ( ( issuerIDlength != subjectIDlength ) || \
				  memcmp( subjectIssuerID, issuerSubjectID, \
						  issuerIDlength ) ) ? \
				CRYPT_ERROR_SIGNATURE : CRYPT_ERROR_BADDATA );
#else
		return( status );
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
		}

	/* The signature is OK, we don't need to check it again.  There is a
	   theoretical situation where this can lead to a false positive which 
	   requires first checking the certificate using the correct issuing CA 
	   (which will set the CERT_FLAG_SIGCHECKED flag) and then checking it 
	   again using a second CA certificate identical to the first but with a 
	   different key.  In other words the issuer DN chains correctly but the 
	   issuer key is different.  The appropriate behaviour here is somewhat 
	   unclear, it could be argued that a CA that uses two otherwise 
	   identical certificates but with different keys is broken and therefore 
	   behaviour in this situation is undefined.  However we need to do 
	   something and returning the result of the check with the correct CA 
	   certificate even if we're later passed a second incorrect certificate 
	   from the CA seems to be the most appropriate action since it has in 
	   the past been validated by a certificate from the same CA.  If we want 
	   to force the check to be done with a specific CA key (rather than just 
	   the issuing CA's certificate in general) we could store the 
	   fingerprint of the signing key alongside the CERT_FLAG_SIGCHECKED 
	   flag */
	subjectCertInfoPtr->flags |= CERT_FLAG_SIGCHECKED;
	
	return( CRYPT_OK );
	}

/* Check a self-signed certificate object like a certificate request or a 
   self-signed certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkSelfSignedCert( INOUT CERT_INFO *certInfoPtr,
								IN_OPT const X509SIG_FORMATINFO *formatInfo )
	{
	CRYPT_CONTEXT iCryptContext = CRYPT_UNUSED;
	CERT_INFO *issuerCertInfoPtr;
	BOOLEAN trustedCertAcquired = FALSE;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( formatInfo == NULL || \
			isReadPtr( formatInfo, sizeof( X509SIG_FORMATINFO ) ) );

	/* Perform a basic check for obvious invalidity issues */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
		certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
		certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
		{
		status = checkCertBasic( certInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Since there's no signer certificate provided it has to be either 
	   explicitly self-signed or signed by a trusted certificate */
	if( certInfoPtr->flags & CERT_FLAG_SELFSIGNED )
		{
		if( certInfoPtr->iPubkeyContext != CRYPT_ERROR )
			iCryptContext = certInfoPtr->iPubkeyContext;
		issuerCertInfoPtr = certInfoPtr;
		}
	else
		{
		CRYPT_CERTIFICATE iCryptCert = certInfoPtr->objectHandle;

		/* If it's a certificate it may be implicitly trusted */
		if( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT )
			{
			if( cryptStatusOK( \
					krnlSendMessage( certInfoPtr->ownerHandle,
									 IMESSAGE_USER_TRUSTMGMT, &iCryptCert,
									 MESSAGE_TRUSTMGMT_CHECK ) ) )
				{
				/* The certificate is implicitly trusted, we're done */
				return( CRYPT_OK );
				}
			}

		/* If it's not self-signed it has to be signed by a trusted 
		   certificate, try and get the trusted certificate */
		status = krnlSendMessage( certInfoPtr->ownerHandle,
								  IMESSAGE_USER_TRUSTMGMT, &iCryptCert,
								  MESSAGE_TRUSTMGMT_GETISSUER );
		if( cryptStatusError( status ) )
			{
			/* There's no trusted signer present, indicate that we need
			   something to check the certificate with */
			return( CRYPT_ARGERROR_VALUE );
			}
		status = krnlAcquireObject( iCryptCert, OBJECT_TYPE_CERTIFICATE,
									( void ** ) &issuerCertInfoPtr,
									CRYPT_ERROR_SIGNALLED );
		if( cryptStatusError( status ) )
			return( status );
		ANALYSER_HINT( issuerCertInfoPtr != NULL );
		iCryptContext = iCryptCert;
		trustedCertAcquired = TRUE;
		}

	/* Check the certificate against the issuing certificate */
	status = checkCertDetails( certInfoPtr, issuerCertInfoPtr, 
							   iCryptContext, formatInfo, FALSE, FALSE,
							   &certInfoPtr->errorLocus, 
							   &certInfoPtr->errorType );
	if( trustedCertAcquired )
		krnlReleaseObject( issuerCertInfoPtr->objectHandle );
	return( cryptArgError( status ) ? CRYPT_ARGERROR_OBJECT : status );
	}

/****************************************************************************
*																			*
*				General Certificate Validity Checking Functions				*
*																			*
****************************************************************************/

/* Check the validity of a certificate object, either against an issuing 
   key/certificate or against a CRL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCertValidity( INOUT CERT_INFO *certInfoPtr, 
					   IN_HANDLE_OPT const CRYPT_HANDLE iSigCheckObject )
	{
	CRYPT_CONTEXT iCryptContext;
	CERT_INFO *issuerCertInfoPtr = NULL;
	X509SIG_FORMATINFO formatInfo, *formatInfoPtr = NULL;
	BOOLEAN issuerCertAcquired = FALSE;
	int sigCheckObjectType, sigCheckKeyType = CRYPT_CERTTYPE_NONE, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->certificate != NULL || \
			  certInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE );
	REQUIRES( iSigCheckObject == CRYPT_UNUSED || \
			  isHandleRangeValid( iSigCheckObject ) );

	/* CRMF and OCSP use a b0rken signature format (the authors couldn't 
	   quite manage a cut & paste of two lines of text) so if it's one of 
	   these we have to use nonstandard formatting */
	if( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT )
		{
		/* [1] SEQUENCE */
		setX509FormatInfo( &formatInfo, 1, FALSE );
		formatInfoPtr = &formatInfo;
		}
	else
		{
		if( certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST ) 
			{
			/* [0] EXPLICIT SEQUENCE */
			setX509FormatInfo( &formatInfo, 0, TRUE );
			formatInfoPtr = &formatInfo;
			}
		}

	/* If there's no signature checking key supplied then the certificate 
	   must be self-signed, either an implicitly self-signed object like a 
	   certificate chain or an explicitly self-signed object like a 
	   certificate request or self-signed certificate */
	if( iSigCheckObject == CRYPT_UNUSED )
		{
		/* If it's a certificate chain it's a (complex) self-signed object 
		   containing more than one certificate so we need a special 
		   function to check the entire chain */
		if( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
			return( checkCertChain( certInfoPtr ) );

		/* It's an explicitly self-signed object */
		return( checkSelfSignedCert( certInfoPtr, formatInfoPtr ) );
		}

	/* Find out what the signature check object is */
	status = krnlSendMessage( iSigCheckObject, IMESSAGE_GETATTRIBUTE, 
							  &sigCheckObjectType, CRYPT_IATTRIBUTE_TYPE );
	if( cryptStatusOK( status ) && \
		sigCheckObjectType == OBJECT_TYPE_CERTIFICATE )
		{
		status = krnlSendMessage( iSigCheckObject, IMESSAGE_GETATTRIBUTE,
								  &sigCheckKeyType, 
								  CRYPT_CERTINFO_CERTTYPE );
		}
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_VALUE : status );

	/* Perform a general validity check on the object being checked and the
	   associated verification object.  This is somewhat more strict than
	   the kernel checks since the kernel only knows about valid subtypes
	   but not that some subtypes are only valid in combination with some
	   types of object being checked */
	switch( sigCheckObjectType )
		{
		case OBJECT_TYPE_CERTIFICATE:
		case OBJECT_TYPE_CONTEXT:
			break;

		case OBJECT_TYPE_KEYSET:
			/* A keyset can only be used as a source of revocation
			   information for checking a certificate or to populate the
			   status fields of an RTCS/OCSP response */
			if( certInfoPtr->type != CRYPT_CERTTYPE_CERTIFICATE && \
				certInfoPtr->type != CRYPT_CERTTYPE_ATTRIBUTE_CERT && \
				certInfoPtr->type != CRYPT_CERTTYPE_CERTCHAIN && \
				certInfoPtr->type != CRYPT_CERTTYPE_RTCS_RESPONSE && \
				certInfoPtr->type != CRYPT_CERTTYPE_OCSP_RESPONSE )
				return( CRYPT_ARGERROR_VALUE );
			break;

		case OBJECT_TYPE_SESSION:
			/* An (RTCS or OCSP) session can only be used as a source of
			   validity/revocation information for checking a certificate */
			if( certInfoPtr->type != CRYPT_CERTTYPE_CERTIFICATE && \
				certInfoPtr->type != CRYPT_CERTTYPE_ATTRIBUTE_CERT && \
				certInfoPtr->type != CRYPT_CERTTYPE_CERTCHAIN )
				return( CRYPT_ARGERROR_VALUE );
			break;

		default:
			return( CRYPT_ARGERROR_VALUE );
		}

	/* Perform a basic check for obvious invalidity issues */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
		certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
		certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
		{
		status = checkCertBasic( certInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If the checking key is a CRL, a keyset that may contain a CRL, or an
	   RTCS or OCSP session then this is a validity/revocation check that 
	   works rather differently from a straight signature check */
#if defined( USE_CERTREV )
	if( sigCheckObjectType == OBJECT_TYPE_CERTIFICATE && \
		sigCheckKeyType == CRYPT_CERTTYPE_CRL )
		return( checkCRL( certInfoPtr, iSigCheckObject ) );
#endif /* USE_CERTREV */
	if( sigCheckObjectType == OBJECT_TYPE_KEYSET )
		{
		/* If it's an RTCS or OCSP response use the certificate store to fill
		   in the status information fields */
#if defined( USE_CERTVAL )
		if( certInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE )
			return( checkRTCSResponse( certInfoPtr, iSigCheckObject ) );
#endif /* USE_CERTVAL */
#if defined( USE_CERTREV )
		if( certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE )
			return( checkOCSPResponse( certInfoPtr, iSigCheckObject ) );

		/* It's a keyset, check the certificate against the CRL blacklist in 
		   the keyset */
		return( checkKeyset( certInfoPtr, iSigCheckObject ) );
#else
		return( CRYPT_ARGERROR_VALUE );
#endif /* USE_CERTREV */
		}
#if defined( USE_CERTREV ) || defined( USE_CERTVAL )
	if( sigCheckObjectType == OBJECT_TYPE_SESSION )
		return( checkResponder( certInfoPtr, iSigCheckObject ) );
#endif /* USE_CERTREV || USE_CERTVAL */

	/* If we've been given a self-signed certificate make sure that the 
	   signature check key is the same as the certificate's key.  To test 
	   this we have to compare both the signing key and if the signature 
	   check object is a certificate, the certificate */
	if( certInfoPtr->flags & CERT_FLAG_SELFSIGNED )
		{
		MESSAGE_DATA msgData;
		BYTE keyID[ KEYID_SIZE + 8 ];

		/* Check that the key in the certificate and the key in the 
		   signature check object are identical */
		setMessageData( &msgData, keyID, KEYID_SIZE );
		status = krnlSendMessage( iSigCheckObject, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( certInfoPtr->objectHandle,
									  IMESSAGE_COMPARE, &msgData,
									  MESSAGE_COMPARE_KEYID );
		if( cryptStatusError( status ) )
			return( CRYPT_ARGERROR_VALUE );

		/* If the signature check object is a certificate (even though 
		   what's being checked is already a self-signed certificate) check 
		   that it's identical to the certificate being checked (which it 
		   must be if the certificate is self-signed).  This may be somewhat 
		   stricter than required but it'll weed out technically valid but 
		   questionable combinations like a certificate request being used 
		   to validate a certificate and misleading ones such as one 
		   certificate chain being used to check a second chain */
		if( sigCheckObjectType == OBJECT_TYPE_CERTIFICATE )
			{
			status = krnlSendMessage( certInfoPtr->objectHandle,
									  IMESSAGE_COMPARE, 
									  ( MESSAGE_CAST ) &iSigCheckObject,
									  MESSAGE_COMPARE_CERTOBJ );
			if( cryptStatusError( status ) )
				return( CRYPT_ARGERROR_VALUE );
			}

		/* If it's a certificate chain it's a (complex) self-signed object
		   containing more than one certificate so we need a special 
		   function to check the entire chain */
		if( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
			return( checkCertChain( certInfoPtr ) );

		return( checkSelfSignedCert( certInfoPtr, formatInfoPtr ) );
		}

	/* The signature check key may be a certificate or a context.  If it's
	   a certificate we get the issuer certificate information and extract 
	   the context from it before continuing */
	if( sigCheckObjectType == OBJECT_TYPE_CERTIFICATE )
		{
		/* Get the context from the issuer certificate */
		status = krnlSendMessage( iSigCheckObject, IMESSAGE_GETDEPENDENT,
								  &iCryptContext, OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( cryptArgError( status ) ? CRYPT_ARGERROR_VALUE : status );

		/* Get the issuer certificate information */
		status = krnlAcquireObject( iSigCheckObject, OBJECT_TYPE_CERTIFICATE,
									( void ** ) &issuerCertInfoPtr,
									CRYPT_ARGERROR_VALUE );
		if( cryptStatusError( status ) )
			return( status );
		ANALYSER_HINT( issuerCertInfoPtr != NULL );
		issuerCertAcquired = TRUE;
		}
	else
		{
		CRYPT_CERTIFICATE localCert;

		iCryptContext = iSigCheckObject;

		/* It's a context, we may have a certificate present with it so we 
		   try to extract that and use it as the issuer certificate if 
		   possible.  If the issuer certificate isn't present this isn't an 
		   error since it could be just a raw context */
		status = krnlSendMessage( iSigCheckObject, IMESSAGE_GETDEPENDENT,
								  &localCert, OBJECT_TYPE_CERTIFICATE );
		if( cryptStatusOK( status ) )
			{
			status = krnlAcquireObject( localCert, OBJECT_TYPE_CERTIFICATE,
										( void ** ) &issuerCertInfoPtr,
										CRYPT_ARGERROR_VALUE );
			if( cryptStatusError( status ) )
				return( status );
			ANALYSER_HINT( issuerCertInfoPtr != NULL );
			issuerCertAcquired = TRUE;
			}
		}

	/* Check the certificate against the issuing certificate */
	status = checkCertDetails( certInfoPtr, issuerCertAcquired ? \
									issuerCertInfoPtr : NULL, 
							   iCryptContext, formatInfoPtr, FALSE, FALSE,
							   &certInfoPtr->errorLocus, 
							   &certInfoPtr->errorType );
	if( issuerCertAcquired )
		krnlReleaseObject( issuerCertInfoPtr->objectHandle );
	return( cryptArgError( status ) ? CRYPT_ARGERROR_VALUE : status );
	}
#endif /* USE_CERTIFICATES */
