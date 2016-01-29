/****************************************************************************
*																			*
*					Certificate Signature Checking Routines					*
*						Copyright Peter Gutmann 1997-2013					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
#else
  #include "cert/cert.h"
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
	CRYPT_CERTIFICATE cryptResponse DUMMY_INIT;
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

/* Check a certificate object against a signing certificate object, 
   typically a certificate against an issuer certificate but also a self-
   signed object like a certificate request against itself.

   The trustAnchorCheck flag is used when we're checking an explicit trust 
   anchor, for which we only need to check the signature if it's self-signed.  
   The shortCircuitCheck flag is used when checking subject:issuer pairs 
   inside certificate chains, which have already been checked by the chain-
   handling code so a full (re-)check isn't necessary any more.  The 
   basicCheckDone flag is used if checkCertBasic() has already been done by 
   the caller (if we're coming from the certificate-chain code then we're 
   processing a certificate in the middle of the chain and it hasn't been 
   done yet, if we're coming from the standalone certificate-checking code 
   then it's already been done) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 8, 9 ) ) \
int checkCertDetails( INOUT CERT_INFO *subjectCertInfoPtr,
					  INOUT_OPT CERT_INFO *issuerCertInfoPtr,
					  IN_HANDLE_OPT const CRYPT_CONTEXT iIssuerPubKey,
					  IN_OPT const X509SIG_FORMATINFO *formatInfo,
					  const BOOLEAN trustAnchorCheck,
					  const BOOLEAN shortCircuitCheck,
					  const BOOLEAN basicCheckDone,
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

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Perform a basic check for obvious invalidity issues if required */
	if( !basicCheckDone && \
		( subjectCertInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
		  subjectCertInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
		  subjectCertInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN ) )
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
		status = krnlSendMessage( subjectCertInfoPtr->ownerHandle, 
								  IMESSAGE_USER_TRUSTMGMT,
								  &subjectCertInfoPtr->objectHandle,
								  MESSAGE_TRUSTMGMT_CHECK );
		if( cryptStatusOK( status ) )
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
							   TRUE, &certInfoPtr->errorLocus, 
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

/* Check the validity of a certificate object against another object, 
   typically an issuing key or certificate but also a CRL or RTCS/OCSP
   session.

   Working with certificate chains is different from working with any other 
   type of certificate object because of the presence of multiple 
   certificates in both the item to be checked (the left-hand argument) and 
   the item to use for the checking (the right-hand argument).  Consider the 
   following combinations of objects that we could encounter in terms of 
   verification:

	Left		Right		Allowed		Configuration
	----		-----		-------		-------------
	Leaf		Issuer		  Y			L: A
										R:	   B

	Chain		-			  Y			L: A - B - C
										R: -

	Chain		Root		  Y			L: A - B - C
										R:		   C

	Leaf		Chain		  N			L: A
										R: A - B - C

	Leaf		Chain - Lf.	  N			L: A
										R:	   B - C

	Chain - Rt.	Root		  Y			L: A - B
										R:		   C

	Chain - Rt.	Chain - Lf.	  N			L: A - B
										R:	   B - C

   In the worst case we could run into something like:

	L: A - B - C - D
	R: X - Y - C - Z

   Now although some of the certificates on the left and the right appear to 
   be identical, all this means is that the DNs and keys (and optionally 
   keyIDs) match, they could otherwise be completely different certificates 
   (different key usage, policies, altNames, and so on).  Because of this we 
   can't use one chain to verify another chain since although they may 
   logically verify, the principle of least surprise (to the user) indicates 
   that we shouldn't make any security statement about it.  For example in 
   the case of the A - B chain verified by the X - Y chain above, if we 
   report a successful verification then the user could be quite justified 
   in thinking that A - B - C - D was verified rather than A - B - C - Z, or 
   even that both the A - B and the X - Y chains are valid.

   An informal survey of users indicated that no-one was really sure what a 
   success (or failure) status would indicate in this case, or more 
   specifically that everyone had some sort of guess but there was little 
   overlap between them, and people changed their views when questioned 
   about individual details.

   Because of this issue of virtually-guaranteed unexpected results from the 
   user's point of view we don't allow a certificate chain as a right-hand 
   argument for checking any kind of certificate object.  In addition we 
   don't allow any overlap between the certificates on the left and the 
   certificates on the right, for example if the left-hand argument is a 
   chain from leaf to root and the right-hand argument is another copy of 
   the root */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCertValidity( INOUT CERT_INFO *certInfoPtr, 
					   IN_HANDLE_OPT const CRYPT_HANDLE iSigCheckObject )
	{
	CRYPT_CONTEXT iCryptContext;
	CERT_INFO *issuerCertInfoPtr = NULL;
	X509SIG_FORMATINFO formatInfo, *formatInfoPtr = NULL;
	BOOLEAN issuerCertAcquired = FALSE;
	int checkObjectType, checkObjectSubtype = CRYPT_CERTTYPE_NONE;
	int status;

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
							  &checkObjectType, CRYPT_IATTRIBUTE_TYPE );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iSigCheckObject, IMESSAGE_GETATTRIBUTE,
								  &checkObjectSubtype, 
								  CRYPT_IATTRIBUTE_SUBTYPE );
		}
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_VALUE : status );

	/* Perform a general validity check on the object being checked and the
	   associated verification object.  This is somewhat more strict than
	   the kernel checks since the kernel only knows about valid subtypes
	   but not that some subtypes are only valid in combination with some
	   types of object being checked */
	switch( checkObjectType )
		{
		case OBJECT_TYPE_CERTIFICATE:
			REQUIRES( checkObjectSubtype == SUBTYPE_CERT_CERT || \
					  checkObjectSubtype == SUBTYPE_CERT_CRL || \
					  checkObjectSubtype == SUBTYPE_CERT_RTCS_RESP || \
					  checkObjectSubtype == SUBTYPE_CERT_OCSP_RESP );
			break;

		case OBJECT_TYPE_CONTEXT:
			REQUIRES( checkObjectSubtype == SUBTYPE_CTX_PKC );
			break;

		case OBJECT_TYPE_KEYSET:
			/* A keyset can only be used as a source of revocation
			   information for checking a certificate or to populate the
			   status fields of an RTCS/OCSP response */
			REQUIRES( checkObjectSubtype == SUBTYPE_KEYSET_DBMS || \
					  checkObjectSubtype == SUBTYPE_KEYSET_DBMS_STORE );
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
			REQUIRES( checkObjectSubtype == SUBTYPE_SESSION_RTCS || \
					  checkObjectSubtype == SUBTYPE_SESSION_OCSP );
			if( certInfoPtr->type != CRYPT_CERTTYPE_CERTIFICATE && \
				certInfoPtr->type != CRYPT_CERTTYPE_ATTRIBUTE_CERT && \
				certInfoPtr->type != CRYPT_CERTTYPE_CERTCHAIN )
				return( CRYPT_ARGERROR_VALUE );
			break;

		default:
			return( CRYPT_ARGERROR_VALUE );
		}

	/* There's one special-case situation in which we don't perform a 
	   standard check and that's when the object to be checked is a 
	   certificate chain.  Normally the checking object would be 
	   CRYPT_UNUSED, but the caller may also pass in a root certificate
	   as the checking object for a chain that's missing the root */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
		{
		CERT_CERT_INFO *certChainInfo = certInfoPtr->cCertCert;

		/* If we're checking a certificate chain then the only type of 
		   checking object that's valid is a certificate.  This isn't 
		   strictly true since the CRL-checking code will check every
		   certificate in the chain against the CRL, but the code for OCSP, 
		   RTCS, and keysets doesn't yet do this so until someone requests 
		   it we don't allow entire chains to be checked in this manner */
		if( checkObjectType != OBJECT_TYPE_CERTIFICATE || \
			checkObjectSubtype != SUBTYPE_CERT_CERT )
			return( CRYPT_ARGERROR_VALUE );

		/* At this point things get a bit deceptive (for the caller), the
		   only way in which the check that's being performed here can 
		   succeed is if the certificate being used as the checking object 
		   is a trusted CA certificate that's at the top of the chain.  In 
		   this case there's no need to explicitly make it part of the check 
		   since it'll already be present in the trust store, however for
		   consistency we verify that it's a CA certificate, that it's
		   trusted, and that it chains up from the certificate at the top of
		   the chain.  Once this check has passed we call checkCertChain()
		   as if the checking object had been specified as CRYPT_UNUSED */
		status = krnlSendMessage( iSigCheckObject, IMESSAGE_CHECK, NULL, 
								  MESSAGE_CHECK_CA );
		if( cryptStatusError( status ) )
			return( CRYPT_ARGERROR_VALUE );
		status = krnlSendMessage( certInfoPtr->ownerHandle, 
								  IMESSAGE_USER_TRUSTMGMT, 
								  ( MESSAGE_CAST ) &iSigCheckObject, 
								  MESSAGE_TRUSTMGMT_CHECK );
		if( cryptStatusError( status ) )
			return( CRYPT_ARGERROR_VALUE );
		REQUIRES( certChainInfo->chainEnd > 0 && \
				  certChainInfo->chainEnd <= MAX_CHAINLENGTH );
		status = krnlSendMessage( certChainInfo->chain[ certChainInfo->chainEnd - 1 ], 
								  IMESSAGE_CRT_SIGCHECK, NULL,
								  iSigCheckObject );
		if( cryptStatusError( status ) )
			return( CRYPT_ARGERROR_VALUE );

		/* The checking object is a trusted CA certificate that connects to
		   the top of the chain, check the rest of the chain with the 
		   implicitly-present trusted certificate as the root */
		return( checkCertChain( certInfoPtr ) );
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

	/* If the checking object is a CRL or CRL-equivalent like an RTCS or
	   OCSP response, a keyset that may contain a CRL, or an RTCS or OCSP 
	   session then this is a validity/revocation check that works rather 
	   differently from a straight signature check */
#if defined( USE_CERTREV ) || defined( USE_CERTVAL )
	if( checkObjectType == OBJECT_TYPE_CERTIFICATE )
		{
#if defined( USE_CERTREV )
		if( checkObjectSubtype == SUBTYPE_CERT_CRL || \
			checkObjectSubtype == SUBTYPE_CERT_OCSP_RESP )
			{
			return( checkCRL( certInfoPtr, iSigCheckObject ) );
			}
#endif /* USE_CERTREV */
#if defined( USE_CERTVAL )
		if( checkObjectSubtype == SUBTYPE_CERT_RTCS_RESP )
			{
			/* This functionality isn't enabled yet, since RTCS is a 
			   straight go/no go protocol it seems unlikely that it'll
			   ever be needed, and no-one's ever requested it in more
			   than a decade of use */
			retIntError();
			}
#endif /* USE_CERTVAL */
		}
#endif /* USE_CERTREV || USE_CERTVAL */
	if( checkObjectType == OBJECT_TYPE_KEYSET )
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
	if( checkObjectType == OBJECT_TYPE_SESSION )
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
		if( checkObjectType == OBJECT_TYPE_CERTIFICATE )
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
	if( checkObjectType == OBJECT_TYPE_CERTIFICATE )
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
							   TRUE, &certInfoPtr->errorLocus, 
							   &certInfoPtr->errorType );
	if( issuerCertAcquired )
		krnlReleaseObject( issuerCertInfoPtr->objectHandle );
	return( cryptArgError( status ) ? CRYPT_ARGERROR_VALUE : status );
	}
#endif /* USE_CERTIFICATES */
