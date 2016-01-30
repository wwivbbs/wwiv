/****************************************************************************
*																			*
*							Set Certificate Components						*
*						Copyright Peter Gutmann 1997-2012					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/* Determine whether anyone ever uses the higher compliance levels */

#if defined( USE_CERTLEVEL_PKIX_FULL )
  #error If you see this message, please let the cryptlib developers know,
  #error then remove the error directive that caused it and recompile as normal.
#endif /* USE_CERTLEVEL_PKIX_FULL */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

#if defined( USE_CERTREQ ) || defined( USE_CERTREV )

/* Copy the encoded issuer DN */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyIssuerDnData( INOUT CERT_INFO *destCertInfoPtr,
							 const CERT_INFO *srcCertInfoPtr )
	{
	void *dnDataPtr;

	assert( isWritePtr( destCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( srcCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( destCertInfoPtr->issuerDNptr == NULL );
	REQUIRES( srcCertInfoPtr->issuerDNptr != NULL );

	if( ( dnDataPtr = clAlloc( "copyIssuerDnData",
							   srcCertInfoPtr->issuerDNsize ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memcpy( dnDataPtr, srcCertInfoPtr->issuerDNptr,
			srcCertInfoPtr->issuerDNsize );
	destCertInfoPtr->issuerDNptr = destCertInfoPtr->issuerDNdata = dnDataPtr;
	destCertInfoPtr->issuerDNsize = srcCertInfoPtr->issuerDNsize;

	return( CRYPT_OK );
	}

/* Copy revocation information into a CRL or revocation request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyRevocationInfo( INOUT CERT_INFO *certInfoPtr,
							   const CERT_INFO *revInfoPtr )
	{
	const void *serialNumberPtr;
	int serialNumberLength, status = CRYPT_OK;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( revInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( revInfoPtr->issuerDNptr, 
					   revInfoPtr->issuerDNsize ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CRL || \
			  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION );
	REQUIRES( revInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  revInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  revInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN || \
			  revInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION );
	REQUIRES( revInfoPtr->issuerDNptr != NULL );

	/* If there's an issuer name recorded make sure that it matches the one
	   in the certificate that's being added */
	if( certInfoPtr->issuerDNptr != NULL )
		{
		if( certInfoPtr->issuerDNsize != revInfoPtr->issuerDNsize || \
			memcmp( certInfoPtr->issuerDNptr, revInfoPtr->issuerDNptr,
					certInfoPtr->issuerDNsize ) )
			{
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_ISSUERNAME,
						  CRYPT_ERRTYPE_ATTR_VALUE );
			status = CRYPT_ERROR_INVALID;
			}
		}
	else
		{
		/* There's no issuer name present yet, set the CRL issuer name to
		   the certificate's issuer to make sure that we can't add 
		   certificates or sign the CRL with a different issuer.  We do this 
		   here rather than after setting the revocation list entry because 
		   of the difficulty of undoing the revocation entry addition */
		status = copyIssuerDnData( certInfoPtr, revInfoPtr );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Add the certificate information to the revocation list and make it 
	   the currently selected entry.  The ID type isn't quite an
	   issuerAndSerialNumber but the checking code eventually converts it 
	   into this form using the supplied issuer certificate DN */
	if( revInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION )
		{
		serialNumberPtr = revInfoPtr->cCertReq->serialNumber;
		serialNumberLength = revInfoPtr->cCertReq->serialNumberLength;
		}
	else
		{
		serialNumberPtr = revInfoPtr->cCertCert->serialNumber;
		serialNumberLength = revInfoPtr->cCertCert->serialNumberLength;
		}
	status = addRevocationEntry( &certInfoPtr->cCertRev->revocations,
								 &certInfoPtr->cCertRev->currentRevocation,
								 CRYPT_IKEYID_ISSUERANDSERIALNUMBER,
								 serialNumberPtr, serialNumberLength, FALSE );
	if( status == CRYPT_ERROR_DUPLICATE )
		{
		/* If this certificate is already present in the list set the 
		   extended error code for it */
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_CERTIFICATE,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		}
	return( status );
	}
#endif /* USE_CERTREQ || USE_CERTREV */

#if defined( USE_CERTREV ) || defined( USE_CERTVAL )

/* Copy an RTCS or OCSP responder URL from a certificate into a request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyResponderURL( INOUT CERT_INFO *requestInfoPtr,
							 INOUT CERT_INFO *userCertInfoPtr )
	{
	const CRYPT_ATTRIBUTE_TYPE aiaAttribute = \
				( requestInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST ) ? \
				CRYPT_CERTINFO_AUTHORITYINFO_RTCS : \
				CRYPT_CERTINFO_AUTHORITYINFO_OCSP;
	SELECTION_STATE savedState;
	void *responderUrl;
	int urlSize DUMMY_INIT, status;

	REQUIRES( requestInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST || \
			  requestInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST );
	REQUIRES( userCertInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  userCertInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	/* There's no responder URL set, check whether the user certificate 
	   contains a responder URL in the RTCS/OCSP authorityInfoAccess 
	   GeneralName */
	saveSelectionState( savedState, userCertInfoPtr );
	status = selectGeneralName( userCertInfoPtr, aiaAttribute,
								MAY_BE_ABSENT );
	if( cryptStatusOK( status ) )
		{
		status = selectGeneralName( userCertInfoPtr,
									CRYPT_ATTRIBUTE_NONE,
									MUST_BE_PRESENT );
		}
	if( cryptStatusOK( status ) )
		{
		status = getCertComponentString( userCertInfoPtr,
								CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER,
								NULL, 0, &urlSize );
		}
	if( cryptStatusError( status ) )
		{
		/* If there's no responder URL present then it's not a (fatal) 
		   error */
		restoreSelectionState( savedState, userCertInfoPtr );
		return( CRYPT_OK );
		}

	/* There's a responder URL present, copy it to the request */
	if( ( responderUrl = clAlloc( "copyResponderURL", urlSize ) ) == NULL )
		{
		restoreSelectionState( savedState, userCertInfoPtr );
		return( CRYPT_ERROR_MEMORY );
		}
	status = getCertComponentString( userCertInfoPtr,
									 CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER,
									 responderUrl, urlSize, &urlSize );
	if( cryptStatusOK( status ) )
		{
		if( requestInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST )
			{
			REQUIRES( requestInfoPtr->cCertVal->responderUrl == NULL );

			requestInfoPtr->cCertVal->responderUrl = responderUrl;
			requestInfoPtr->cCertVal->responderUrlSize = urlSize;
			}
		else
			{
			REQUIRES( requestInfoPtr->cCertRev->responderUrl == NULL );

			requestInfoPtr->cCertRev->responderUrl = responderUrl;
			requestInfoPtr->cCertRev->responderUrlSize = urlSize;
			}
		}
	else
		{
		clFree( "copyResponderURL", responderUrl );
		}
	restoreSelectionState( savedState, userCertInfoPtr );

	return( status );
	}
#endif /* USE_CERTREV || USE_CERTVAL */

/****************************************************************************
*																			*
*							Copy Certificate Data							*
*																			*
****************************************************************************/

/* Check that the key in a XYZZY certificate is signature-capable (or at 
   least sig-check capable, since it may be just the public key that's being 
   used), since XYZZY certificates are self-signed.  Then set the 
   appropriate signature keyUsage and optional encryption usage if the key 
   can also do that */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setXyzzyKeyUsage( INOUT CERT_INFO *certInfoPtr,
							 IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	int keyUsage = KEYUSAGE_SIGN | KEYUSAGE_CA, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Make sure that the key is signature-capable.  We have to check for a 
	   capability to either sign or sig-check since a pure public key will 
	   only have a sig-check capability while a private key held in a device 
	   (from which we're going to extract the public-key components) may be 
	   only signature-capable */
	status = krnlSendMessage( iCryptContext, IMESSAGE_CHECK, NULL,
							  MESSAGE_CHECK_PKC_SIGCHECK );
	if( cryptStatusError( status ) )
		status = krnlSendMessage( iCryptContext, IMESSAGE_CHECK, NULL,
								  MESSAGE_CHECK_PKC_SIGN );
	if( cryptStatusError( status ) )
		{
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE,
					  CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ERROR_INVALID );
		}

	/* If the key is encryption-capable (with the same caveat as for the 
	   signature check above), enable that usage as well */
	status = krnlSendMessage( iCryptContext, IMESSAGE_CHECK, NULL,
							  MESSAGE_CHECK_PKC_ENCRYPT );
	if( cryptStatusError( status ) )
		status = krnlSendMessage( iCryptContext, IMESSAGE_CHECK, NULL,
								  MESSAGE_CHECK_PKC_DECRYPT );
	if( cryptStatusOK( status ) )
		keyUsage |= CRYPT_KEYUSAGE_KEYENCIPHERMENT;

	/* Clear the existing usage and replace it with our usage.  See the 
	   comment in setXyzzyInfo() for why it's done this way */
	( void ) deleteCertComponent( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE );
	return( addCertComponent( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE, 
							  keyUsage ) );
	}

/* Copy public key data into a certificate object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int copyKeyFromCertificate( INOUT CERT_INFO *destCertInfoPtr,
								   const CERT_INFO *srcCertInfoPtr )
	{
	void *publicKeyInfoPtr;

	assert( isWritePtr( destCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( srcCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( destCertInfoPtr->publicKeyInfo == NULL );
	REQUIRES( srcCertInfoPtr->publicKeyInfo != NULL );

	if( ( publicKeyInfoPtr = \
				clAlloc( "copyPublicKeyInfo", 
						 srcCertInfoPtr->publicKeyInfoSize ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memcpy( publicKeyInfoPtr, srcCertInfoPtr->publicKeyInfo, 
			srcCertInfoPtr->publicKeyInfoSize );
	destCertInfoPtr->publicKeyData = destCertInfoPtr->publicKeyInfo = \
		publicKeyInfoPtr;
	destCertInfoPtr->publicKeyInfoSize = srcCertInfoPtr->publicKeyInfoSize;
	destCertInfoPtr->publicKeyAlgo = srcCertInfoPtr->publicKeyAlgo;
	destCertInfoPtr->publicKeyFeatures = srcCertInfoPtr->publicKeyFeatures;
	memcpy( destCertInfoPtr->publicKeyID, srcCertInfoPtr->publicKeyID, 
			KEYID_SIZE );
	destCertInfoPtr->flags |= CERT_FLAG_DATAONLY;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int copyKeyFromContext( INOUT CERT_INFO *destCertInfoPtr,
							   IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	MESSAGE_DATA msgData;
	void *publicKeyInfoPtr;
	int length, status;

	assert( isWritePtr( destCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( destCertInfoPtr->publicKeyInfo == NULL );
	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Get the key metadata from the context */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &destCertInfoPtr->publicKeyAlgo,
							  CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
								  &destCertInfoPtr->publicKeyFeatures,
								  CRYPT_IATTRIBUTE_KEYFEATURES );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, destCertInfoPtr->publicKeyID, KEYID_SIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Copy over the public-key data.  We perform a copy rather than keeping 
	   a reference to the context for two reasons.  Firstly, when the 
	   certificate is transitioned into the high state it will constrain the 
	   attached context so a context shared between two certificates could 
	   be constrained in unexpected ways.  Secondly, the context could be a 
	   private-key context and attaching that to a certificate would be 
	   rather inappropriate.  Furthermore, the constraint issue is even more 
	   problematic in that a context constrained by an encryption-only 
	   request could then no longer be used to sign the request or a PKI 
	   protocol message containing the request */
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_KEY_SPKI );
	if( cryptStatusError( status ) )
		return( status );
	length = msgData.length;
	if( ( publicKeyInfoPtr = clAlloc( "copyPublicKeyInfo", 
									  length ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	setMessageData( &msgData, publicKeyInfoPtr, length );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_KEY_SPKI );
	if( cryptStatusError( status ) )
		return( status );
	destCertInfoPtr->publicKeyData = destCertInfoPtr->publicKeyInfo = \
		publicKeyInfoPtr;
	destCertInfoPtr->publicKeyInfoSize = length;
	destCertInfoPtr->flags |= CERT_FLAG_DATAONLY;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyPublicKeyInfo( INOUT CERT_INFO *certInfoPtr,
					   IN_HANDLE_OPT const CRYPT_HANDLE cryptHandle,
					   IN_OPT const CERT_INFO *srcCertInfoPtr )
	{
	CRYPT_CONTEXT iCryptContext;
	int isXyzzyCert, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( isHandleRangeValid( cryptHandle ) && \
			  srcCertInfoPtr == NULL ) || \
			( cryptHandle == CRYPT_UNUSED && \
			  isReadPtr( srcCertInfoPtr, sizeof( CERT_INFO ) ) ) );

	REQUIRES( ( isHandleRangeValid( cryptHandle ) && \
				srcCertInfoPtr == NULL ) || \
			  ( cryptHandle == CRYPT_UNUSED && \
			    srcCertInfoPtr != NULL ) );
	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
			  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );
	REQUIRES( srcCertInfoPtr == NULL || \
			  srcCertInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
			  srcCertInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );

	/* Make sure that we haven't already got a public key present */
	if( certInfoPtr->iPubkeyContext != CRYPT_ERROR || \
		certInfoPtr->publicKeyInfo != NULL )
		{
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		return( CRYPT_ERROR_INITED );
		}

	/* If we've been given a data-only certificate then all that we need to 
	   do is copy over the public key data */
	if( srcCertInfoPtr != NULL )
		return( copyKeyFromCertificate( certInfoPtr, srcCertInfoPtr ) );

	/* Get the context handle.  All other checking has already been 
	   performed by the kernel */
	status = krnlSendMessage( cryptHandle, IMESSAGE_GETDEPENDENT, 
							  &iCryptContext, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		{
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO,
					  CRYPT_ERRTYPE_ATTR_VALUE );
		return( status );
		}

	/* If it's a XYZZY certificate then we need to perform additional
	   processing for XYZZY-specific key usage requirements */
	status = getCertComponent( certInfoPtr, CRYPT_CERTINFO_XYZZY, 
							   &isXyzzyCert );
	if( cryptStatusOK( status ) && isXyzzyCert )
		{
		status = setXyzzyKeyUsage( certInfoPtr, iCryptContext);
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Copy the public-key data from the encryption context */
	return( copyKeyFromContext( certInfoPtr, iCryptContext ) );
	}

#ifdef USE_DBMS	/* Only used by CA code */

/* Sanitise certificate attributes based on a user-supplied template.  This 
   is used to prevent a user from supplying potentially dangerous attributes 
   in a certificate request, for example to request a CA certificate by 
   setting the basicConstraints/keyUsage = CA extensions in the request in a 
   manner that would result in the creation of a CA certificate when the 
   request is processed.  
   
   There are two ways to do this, either with a whitelist or with a 
   blacklist.  Obviously the preferred option is the whitelist one, but the
   problem with this is that without any knowledge of the environment into
   which it's being deployed we have no idea what should be permitted.  For
   example an RA, which acts as a trusted agent for the CA, probably wants
   fuller control over the form of the certificate with a wider selection of 
   attributes permitted while a CA dealing directly with the public probably 
   wants few or no attributes permitted.  This is a variation of the 
   firewall configuration problem, lock it down by default and users will 
   complain, leave it open by default and users are happy but it's not secure.

   What we use here is in effect a blend of whitelist and blacklist.  On
   the whitelist side the only attributes that are permitted in certificate 
   requests are:

	CRYPT_CERTINFO_CHALLENGEPASSWORD: Needed for SCEP.
	CRYPT_CERTINFO_KEYFEATURES: Only used by cryptlib for its own purposes.
	CRYPT_CERTINFO_KEYUSAGE
	CRYPT_CERTINFO_SUBJECTALTNAME
	CRYPT_CERTINFO_EXTKEYUSAGE

   For revocation requests we allow rather more values since they're 
   required by the client to inform the CA why the certificate is being
   revoked.  Except for the invalidity date they're not so much of a 
   security threat, and even for the date the consistency vs. accuracy issue 
   for CRLs debate means the CA will put who knows what date in the CRL 
   anyway:

	CRYPT_CERTINFO_CRLEXTREASON
	CRYPT_CERTINFO_CRLREASON
	CRYPT_CERTINFO_HOLDINSTRUCTIONCODE
	CRYPT_CERTINFO_INVALIDITYDATE
   
   Everything else, including the all-important basicConstraints, has to be 
   set explicitly by the CA and can't be specified in a request.

   On the blacklist side, multi-AVA RDNs are disallowed in order to prevent
   an attacker from playing games with DN forms.  This should be safe 
   because the only things that create such weird DNs tend to be European
   in-house CAs following (or justifying their bugs through) peculiar 
   requirements in digital signature legislation, and those guys will be 
   creating their own certificates from scratch themselves rather than using 
   cryptlib for it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkCertWellFormed( INOUT CERT_INFO *certInfoPtr )
	{
	ATTRIBUTE_PTR *attributePtr;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* Check that the subject DN is well-formed */
	status = checkDN( certInfoPtr->subjectName, 
					  CHECKDN_FLAG_COUNTRY | CHECKDN_FLAG_COMMONNAME | \
							CHECKDN_FLAG_WELLFORMED,
					  &certInfoPtr->errorLocus,
					  &certInfoPtr->errorType );
	if( cryptStatusError( status ) )
		return( status );

	/* There can potentially be any number of other DNs in a certificate 
	   (typically hidden in GeneralName fields), it's not really certain 
	   what we should do with these or even how we can effectively track 
	   them all down.  Since the reason why we're doing this in the first 
	   place is to avoid shenanigans due to the more or less arbitrary 
	   handling of complex DNs by implementations, and the only one that 
	   anyone really pays any attention to is the subject name, we check 
	   the subject name and at least have a go at the subjectAltName, but 
	   leave the rest (it's not even certain what a DN in, say, an AIA
	   would mean, let alone how to check it) */
	attributePtr = findAttribute( certInfoPtr->attributes,
								  CRYPT_CERTINFO_SUBJECTALTNAME, TRUE );
	if( attributePtr != NULL )
		attributePtr = findAttributeField( attributePtr, 
										   CRYPT_CERTINFO_SUBJECTALTNAME,
										   CRYPT_CERTINFO_DIRECTORYNAME );
	if( attributePtr != NULL )
		{
		CRYPT_ATTRIBUTE_TYPE dummy1;
		CRYPT_ERRTYPE_TYPE dummy2;
		DN_PTR **dnPtr;

		status = getAttributeDataDN( attributePtr, &dnPtr );
		if( cryptStatusOK( status ) )
			{
			status = checkDN( dnPtr, CHECKDN_FLAG_COUNTRY | \
									 CHECKDN_FLAG_COMMONNAME | \
									 CHECKDN_FLAG_WELLFORMED,
							  &dummy1, &dummy2 );
			if( cryptStatusError( status ) )
				{
				/* Reporting this one is a bit complicated because we're 
				   dealing with a complex nested attribute, the best that we 
				   can do is report a general problem with the altName */
				setErrorInfo( certInfoPtr, 
							  CRYPT_CERTINFO_SUBJECTALTNAME,
							  CRYPT_ERRTYPE_ATTR_VALUE );
				return( status );
				}
			}
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sanitiseCertAttributes( INOUT CERT_INFO *certInfoPtr,
								   IN_OPT const ATTRIBUTE_PTR *templateListPtr )
	{
	const ATTRIBUTE_PTR *templateAttributeCursor;
	ATTRIBUTE_ENUM_INFO attrEnumInfo;
	int iterationCount;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( templateListPtr == NULL || \
			( isReadPtr( templateListPtr, sizeof( ATTRIBUTE_PTR_STORAGE ) ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	/* If there are no attributes present or there's no disallowed-attribute 
	   template, we just need to apply the well-formedness check on the way
	   out */
	if( certInfoPtr->attributes == NULL || templateListPtr == NULL )
		return( checkCertWellFormed( certInfoPtr ) );

	/* Walk down the template attribute list applying each one in turn to
	   the certificate attributes */
	for( templateAttributeCursor = \
			getFirstAttribute( &attrEnumInfo, templateListPtr, \
							   ATTRIBUTE_ENUM_NONBLOB ), \
			iterationCount = 0;
		 templateAttributeCursor != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MAX; 
		 templateAttributeCursor = getNextAttribute( &attrEnumInfo ), \
			iterationCount++ )
		{
		CRYPT_ATTRIBUTE_TYPE fieldID, subFieldID;
		ATTRIBUTE_PTR *certAttributePtr;
		int status;

		/* Check to see whether there's a constrained attribute present in
		   the certificate attributes and if it is, whether it conflicts 
		   with the constraining attribute */
		status = getAttributeIdInfo( templateAttributeCursor, NULL, 
									 &fieldID, &subFieldID );
		if( cryptStatusError( status ) )
			return( status );
		certAttributePtr = findAttributeField( certInfoPtr->attributes,
											   fieldID, subFieldID );
		if( certAttributePtr == NULL )
			{
			/* There's nothing to constrain present in the certificate, 
			   continue */
			continue;
			}

		/* If the certificate attribute was provided through the application 
		   of PKI user data (indicated by it having the locked flag set), 
		   allow it even if it conflicts with the constraining attribute.  
		   This is permitted because the PKI user data was explicitly set by 
		   the issuing CA rather than being user-supplied in the certificate 
		   request so it has to be OK, or at least CA-approved */
		if( checkAttributeProperty( certAttributePtr, 
									ATTRIBUTE_PROPERTY_LOCKED ) )
			continue;

		/* There are conflicting attributes present, disallow the 
		   certificate issue */
		status = getAttributeIdInfo( certAttributePtr, NULL, &fieldID, 
									 NULL );
		if( cryptStatusError( status ) )
			return( status );
		if( fieldID == CRYPT_CERTINFO_KEYUSAGE )
			{
			int constrainedAttributeValue, constrainingAttributeValue;

			/* There is one special case in which conflicting attributes 
			   are allowed and that's for keyUsage (or more generally 
			   bitfields), since we can selectively disallow dangerous bit 
			   values while allowing safe ones */
			status = getAttributeDataValue( certAttributePtr, 
											&constrainedAttributeValue );
			if( cryptStatusError( status ) )
				return( status );
			status = getAttributeDataValue( templateAttributeCursor, 
											&constrainingAttributeValue );
			if( cryptStatusError( status ) )
				return( status );
			if( !( constrainedAttributeValue & constrainingAttributeValue ) )
				continue;
			}
		certInfoPtr->errorLocus = fieldID;
		certInfoPtr->errorType = CRYPT_ERRTYPE_ATTR_VALUE;
		return( CRYPT_ERROR_INVALID );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( checkCertWellFormed( certInfoPtr ) );
	}
#endif /* USE_DBMS*/

/****************************************************************************
*																			*
*						Copy Certificate Request Data						*
*																			*
****************************************************************************/

#ifdef USE_CERTREQ

/* Copy certificate request information into a certificate object.  This 
   copies the public key context, the DN, any valid attributes, and any other 
   relevant bits and pieces if it's a CRMF request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyCertReqToCert( INOUT CERT_INFO *certInfoPtr,
							  INOUT CERT_INFO *certRequestInfoPtr )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( certRequestInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );
	REQUIRES( certInfoPtr->subjectName == NULL );
	REQUIRES( certRequestInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
			  certRequestInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );

	/* Copy the public key context, the DN, and the attributes.  Type
	   checking has already been performed by the kernel.  We copy the
	   attributes across after the DN because that copy is the hardest to
	   undo: If there are already attributes present then the copied 
	   attributes would be mixed in among them so it's not really possible 
	   to undo the copy later without performing a complex selective 
	   delete */
	status = copyDN( &certInfoPtr->subjectName,
					 certRequestInfoPtr->subjectName );
	if( cryptStatusError( status ) )
		return( status );
	if( certRequestInfoPtr->flags & CERT_FLAG_DATAONLY )
		{
		status = copyPublicKeyInfo( certInfoPtr, CRYPT_UNUSED,
									certRequestInfoPtr );
		}
	else
		{
		status = copyPublicKeyInfo( certInfoPtr,
									certRequestInfoPtr->iPubkeyContext,
									NULL );
		}
	if( cryptStatusOK( status ) && \
		certRequestInfoPtr->attributes != NULL )
		{
		status = copyAttributes( &certInfoPtr->attributes,
								 certRequestInfoPtr->attributes,
								 &certInfoPtr->errorLocus,
								 &certInfoPtr->errorType );
		}
	if( cryptStatusError( status ) )
		{
		deleteDN( &certInfoPtr->subjectName );
		return( status );
		}

	/* If it's a CRMF request there could also be a validity period
	   specified */
	if( certRequestInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT )
		{
		const time_t currentTime = getApproxTime();

		/* We don't allow start times backdated by more than a year or end
		   times before the start time.  Since these are trivial things we
		   don't abort if there's a problem but just quietly fix the value */
		if( certRequestInfoPtr->startTime > MIN_TIME_VALUE && \
			certRequestInfoPtr->startTime > currentTime - ( 86400L * 365 ) )
			certInfoPtr->startTime = certRequestInfoPtr->startTime;
		if( certRequestInfoPtr->endTime > MIN_TIME_VALUE && \
			certRequestInfoPtr->endTime > certInfoPtr->startTime )
			certInfoPtr->endTime = certRequestInfoPtr->endTime;
		}

	return( CRYPT_OK );
	}

/* Copy what we need to identify the certificate to be revoked and any 
   revocation information into a certificate object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyRevReqToCert( INOUT CERT_INFO *certInfoPtr,
							 INOUT CERT_INFO *revRequestInfoPtr )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( revRequestInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CRL );
	REQUIRES( revRequestInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
			  revRequestInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION );

	status = copyRevocationInfo( certInfoPtr, revRequestInfoPtr );
	if( cryptStatusError( status ) || \
		revRequestInfoPtr->attributes == NULL )
		return( status );
	return( copyRevocationAttributes( &certInfoPtr->attributes,
									  revRequestInfoPtr->attributes ) );
	}

/* Copy the public key, DN, and any attributes that need to be copied across.  
   We copy the full DN rather than just the encoded form in case the user 
   wants to query the request details after creating it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyCertToRequest( INOUT CERT_INFO *crmfRequestInfoPtr,
							  INOUT CERT_INFO *certInfoPtr,
							  const CRYPT_HANDLE iCryptHandle )
	{
	int status;

	assert( isWritePtr( crmfRequestInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( crmfRequestInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );
	REQUIRES( crmfRequestInfoPtr->subjectName == NULL );
	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	status = copyDN( &crmfRequestInfoPtr->subjectName,
					 certInfoPtr->subjectName );
	if( cryptStatusError( status ) )
		return( status );
	if( crmfRequestInfoPtr->iPubkeyContext == CRYPT_ERROR && \
		crmfRequestInfoPtr->publicKeyInfo == NULL )
		{
		/* Only copy the key across if a key hasn't already been added 
		   earlier as CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO.  Checking for 
		   this special case (rather than returning an error) allows the DN 
		   information from an existing certificate to be copied into a 
		   request for a new key */
		status = copyPublicKeyInfo( crmfRequestInfoPtr, iCryptHandle, NULL );
		}
	if( cryptStatusOK( status ) )
		{
		/* We copy the attributes across after the DN because that copy is 
		   the hardest to undo: If there are already attributes present then 
		   the copied attributes will be mixed in among them so it's not 
		   really possible to undo the copy later without performing a 
		   complex selective delete */
		status = copyCRMFRequestAttributes( &crmfRequestInfoPtr->attributes,
											certInfoPtr->attributes );
		}
	if( cryptStatusError( status ) )
		deleteDN( &crmfRequestInfoPtr->subjectName );

	return( status );
	}

/* Copy across the issuer and subject DN and serial number */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyCertToRevRequest( INOUT CERT_INFO *crmfRevRequestInfoPtr,
								 INOUT CERT_INFO *certInfoPtr )
	{
	int status;

	assert( isWritePtr( crmfRevRequestInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( crmfRevRequestInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION );
	REQUIRES( crmfRevRequestInfoPtr->subjectName == NULL );
	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	/* If the information is already present then we can't add it again */
	if( crmfRevRequestInfoPtr->issuerName != NULL )
		{
		setErrorInfo( crmfRevRequestInfoPtr, CRYPT_CERTINFO_CERTIFICATE,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		return( CRYPT_ERROR_INITED );
		}

	/* Copy across the issuer name and allocate the storage that we need to 
	   copy the subject name.  We don't care about any internal structure of 
	   the DNs so we just copy the pre-encoded form, we could in theory copy 
	   the full DN but it isn't really the issuer (creator) of the object so 
	   it's better if it appears to have no issuer DN than a misleading one */
	status = copyIssuerDnData( crmfRevRequestInfoPtr, certInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	status = setSerialNumber( crmfRevRequestInfoPtr,
							  certInfoPtr->cCertCert->serialNumber,
							  certInfoPtr->cCertCert->serialNumberLength );
	if( cryptStatusOK( status ) && \
		( crmfRevRequestInfoPtr->subjectDNdata = \
				  clAlloc( "copyCertToRevRequest",
						   certInfoPtr->subjectDNsize ) ) == NULL )
		status = CRYPT_ERROR_MEMORY;
	if( cryptStatusError( status ) )
		{
		clFree( "copyCertToRevRequest", 
				crmfRevRequestInfoPtr->issuerDNdata );
		crmfRevRequestInfoPtr->issuerDNptr = \
			crmfRevRequestInfoPtr->issuerDNdata = NULL;
		crmfRevRequestInfoPtr->issuerDNsize = 0;
		if( crmfRevRequestInfoPtr->cCertCert->serialNumber != NULL && \
			crmfRevRequestInfoPtr->cCertCert->serialNumber != \
				crmfRevRequestInfoPtr->cCertCert->serialNumberBuffer )
			{
			clFree( "copyCertToRevRequest",
					crmfRevRequestInfoPtr->cCertCert->serialNumber );
			}
		crmfRevRequestInfoPtr->cCertCert->serialNumber = NULL;
		crmfRevRequestInfoPtr->cCertCert->serialNumberLength = 0;
		return( status );
		}

	/* Copy the subject DN for use in CMP */
	memcpy( crmfRevRequestInfoPtr->subjectDNdata, certInfoPtr->subjectDNptr,
			certInfoPtr->subjectDNsize );
	crmfRevRequestInfoPtr->subjectDNptr = crmfRevRequestInfoPtr->subjectDNdata;
	crmfRevRequestInfoPtr->subjectDNsize = certInfoPtr->subjectDNsize;

	return( CRYPT_OK );
	}
#endif /* USE_CERTREQ */

/****************************************************************************
*																			*
*						Copy Certificate Revocation Data					*
*																			*
****************************************************************************/

#ifdef USE_CERTREV

/* The OCSP ID doesn't contain any usable fields so we pre-encode it when 
   the certificate is added to the OCSP request and treat it as a blob 
   thereafter */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeOCSPID( INOUT STREAM *stream, 
						const CERT_INFO *certInfoPtr,
						IN_BUFFER( issuerKeyHashLength ) \
							const void *issuerKeyHash,
						IN_LENGTH_FIXED( KEYID_SIZE ) \
							const int issuerKeyHashLength )
	{
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	BYTE hashBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int hashSize;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( issuerKeyHash, issuerKeyHashLength ) );

	REQUIRES( issuerKeyHashLength == KEYID_SIZE );
	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );
	REQUIRES( certInfoPtr->issuerDNptr != NULL );
	REQUIRES( certInfoPtr->cCertCert->serialNumber != NULL );

	/* Get the issuerName hash */
	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, 
							 &hashSize );
	hashFunctionAtomic( hashBuffer, CRYPT_MAX_HASHSIZE,
						certInfoPtr->issuerDNptr,
						certInfoPtr->issuerDNsize );

	/* Write the request data */
	writeSequence( stream,
			sizeofAlgoID( CRYPT_ALGO_SHA1 ) + \
			sizeofObject( hashSize ) + sizeofObject( hashSize ) + \
			sizeofInteger( certInfoPtr->cCertCert->serialNumber,
						   certInfoPtr->cCertCert->serialNumberLength ) );
	writeAlgoID( stream, CRYPT_ALGO_SHA1 );
	writeOctetString( stream, hashBuffer, hashSize, DEFAULT_TAG );
	writeOctetString( stream, issuerKeyHash, issuerKeyHashLength, 
					  DEFAULT_TAG );
	return( writeInteger( stream, certInfoPtr->cCertCert->serialNumber,
						  certInfoPtr->cCertCert->serialNumberLength,
						  DEFAULT_TAG ) );
	}

/* Copy revocation information from an OCSP request to a response */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyOcspReqToResp( INOUT CERT_INFO *certInfoPtr,
							  INOUT CERT_INFO *ocspRequestInfoPtr )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( ocspRequestInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE );
	REQUIRES( certInfoPtr->cCertRev->revocations == NULL );
	REQUIRES( ocspRequestInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST );

	/* Copy the revocation information and extensions */
	status = copyRevocationEntries( &certInfoPtr->cCertRev->revocations,
									ocspRequestInfoPtr->cCertRev->revocations );
	if( cryptStatusOK( status ) )
		status = copyOCSPRequestAttributes( &certInfoPtr->attributes,
											ocspRequestInfoPtr->attributes );
	if( cryptStatusError( status ) )
		return( status );

	return( CRYPT_OK );
	}

/* Copy the certificate information to the revocation list.  First we make 
   sure that the CA certificate hash (needed for OCSP's weird certificate 
   ID) is present.  We add the necessary information as a pre-encoded blob 
   since we can't do much with the ID fields */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyCertToOCSPRequest( INOUT CERT_INFO *ocspRequestInfoPtr,
								  INOUT CERT_INFO *certInfoPtr )
	{
	CERT_REV_INFO *revInfoPtr = ocspRequestInfoPtr->cCertRev;
	STREAM stream;
	DYNBUF essCertDB;
	BYTE idBuffer[ 256 + 8 ], *idBufPtr = idBuffer;
	int idLength DUMMY_INIT, status;

	assert( isWritePtr( ocspRequestInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( ocspRequestInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST );
	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	/* Make sure that there's a CA certificate hash present */
	if( !ocspRequestInfoPtr->certHashSet )
		{
		setErrorInfo( ocspRequestInfoPtr, CRYPT_CERTINFO_CACERTIFICATE,
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_NOTINITED );
		}

	/* Generate the OCSP certificate ID */
	sMemNullOpen( &stream );
	status = writeOCSPID( &stream, certInfoPtr, 
						  ocspRequestInfoPtr->certHash, KEYID_SIZE );
	if( cryptStatusOK( status ) )
		idLength = stell( &stream );
	sMemClose( &stream );
	if( cryptStatusError( status ) )
		return( status );
	if( idLength > 256 )
		{
		/* Allocate a buffer for an oversize ID */
		if( ( idBufPtr = clDynAlloc( "copyCertToOCSPRequest", \
									 idLength ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		}
	sMemOpen( &stream, idBufPtr, idLength );
	status = writeOCSPID( &stream, certInfoPtr, 
						  ocspRequestInfoPtr->certHash, KEYID_SIZE );
	sMemDisconnect( &stream );
	if( cryptStatusOK( status ) )
		{
		status = addRevocationEntry( &revInfoPtr->revocations,
									 &revInfoPtr->currentRevocation,
									 CRYPT_KEYID_NONE, idBufPtr,
									 idLength, FALSE );
		}
	if( idBufPtr != idBuffer )
		clFree( "copyCertToOCSPRequest", idBufPtr );
	if( status == CRYPT_ERROR_DUPLICATE )
		{
		/* If this certificate is already present in the list, set the 
		   extended error code for it */
		setErrorInfo( ocspRequestInfoPtr, CRYPT_CERTINFO_CERTIFICATE,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Add the certificate information again as an ESSCertID extension to 
	   work around the problems inherent in OCSP IDs */
	status = dynCreate( &essCertDB, certInfoPtr->objectHandle, 
						CRYPT_IATTRIBUTE_ESSCERTID );
	if( cryptStatusOK( status ) )
		{
		CRYPT_ATTRIBUTE_TYPE dummy1;
		CRYPT_ERRTYPE_TYPE dummy2;

		/* Since this isn't a critical extension (the ESSCertID is just a 
		   backup for the main, albeit not very useful, ID) we continue if 
		   there's a problem adding it */
		( void ) addAttributeFieldString( \
								&revInfoPtr->currentRevocation->attributes,
								CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID, 
								CRYPT_ATTRIBUTE_NONE, dynData( essCertDB ), 
								dynLength( essCertDB ), ATTR_FLAG_NONE, 
								&dummy1, &dummy2 );
		dynDestroy( &essCertDB );
		}
	return( CRYPT_OK );
	}

/* Get the hash of the public key (for an OCSP request), possibly
   overwriting a previous hash if there are multiple entries in the
   request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyCaCertToOCSPReq( INOUT CERT_INFO *certInfoPtr,
								INOUT CERT_INFO *caCertInfoPtr )
	{
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	STREAM stream;
	void *dataPtr DUMMY_INIT_PTR;
	int length, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( caCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST );
	REQUIRES( caCertInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  caCertInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );
	REQUIRES( caCertInfoPtr->publicKeyInfo != NULL );

	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, NULL );

	/* Dig down into the encoded key data to find the weird bits of key that
	   OCSP requires us to hash.  We store the result as the certificate 
	   hash, this is safe because this value isn't used for an OCSP request 
	   so it can't be accessed externally */
	sMemConnect( &stream, caCertInfoPtr->publicKeyInfo,
				 caCertInfoPtr->publicKeyInfoSize );
	readSequence( &stream, NULL );	/* Wrapper */
	readUniversal( &stream );		/* AlgoID */
	status = readBitStringHole( &stream, &length, 16, DEFAULT_TAG );
	if( cryptStatusOK( status ) )	/* BIT STRING wrapper */
		status = sMemGetDataBlock( &stream, &dataPtr, length );
	if( cryptStatusError( status ) )
		{
		/* There's a problem with the format of the key */
		DEBUG_DIAG(( "Invalid certificate data format when hashing "
					 "certificate to create OCSP ID" ));
		assert( DEBUG_WARN );
		sMemDisconnect( &stream );
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_CACERTIFICATE,
					  CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ERROR_INVALID );
		}
	hashFunctionAtomic( certInfoPtr->certHash, KEYID_SIZE, dataPtr, length );
	certInfoPtr->certHashSet = TRUE;
	sMemDisconnect( &stream );

	return( CRYPT_OK );
	}
#endif /* USE_CERTREV */

/****************************************************************************
*																			*
*						Copy Certificate Validation Data					*
*																			*
****************************************************************************/

#ifdef USE_CERTVAL

/* Copy validity information from an RTCS request to a response */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyRtcsReqToResp( INOUT CERT_INFO *certInfoPtr,
							  INOUT CERT_INFO *rtcsRequestInfoPtr )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( rtcsRequestInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE );
	REQUIRES( certInfoPtr->cCertVal->validityInfo == NULL );
	REQUIRES( rtcsRequestInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST );

	/* Copy the certificate validity information and extensions */
	status = copyValidityEntries( &certInfoPtr->cCertVal->validityInfo,
								  rtcsRequestInfoPtr->cCertVal->validityInfo );
	if( cryptStatusOK( status ) )
		status = copyRTCSRequestAttributes( &certInfoPtr->attributes,
											rtcsRequestInfoPtr->attributes );
	return( status );
	}

/* Copy the certificate hash.  We read the value indirectly since it's 
   computed on demand and may not have been evaluated yet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyCertToRTCSRequest( INOUT CERT_INFO *rtcsRequestInfoPtr,
								  INOUT CERT_INFO *certInfoPtr )
	{
	CERT_VAL_INFO *valInfoPtr = rtcsRequestInfoPtr->cCertVal;
	BYTE certHash[ CRYPT_MAX_HASHSIZE + 8 ];
	int certHashLength, status;

	assert( isWritePtr( rtcsRequestInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( rtcsRequestInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST );
	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	status = getCertComponentString( certInfoPtr,
									 CRYPT_CERTINFO_FINGERPRINT_SHA1, 
									 certHash, CRYPT_MAX_HASHSIZE, 
									 &certHashLength );
	if( cryptStatusOK( status ) )
		{
		status = addValidityEntry( &valInfoPtr->validityInfo,
								   &valInfoPtr->currentValidity,
								   certHash, certHashLength );
		}
	if( status == CRYPT_ERROR_DUPLICATE )
		{
		/* If this certificate is already present in the list, set the 
		   extended error code for it */
		setErrorInfo( rtcsRequestInfoPtr, CRYPT_CERTINFO_CERTIFICATE,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		}
	return( status );
	}
#endif /* USE_CERTVAL */

/****************************************************************************
*																			*
*				Certificate Information Copy External Interface				*
*																			*
****************************************************************************/

/* Copy user certificate information into a certificate object, either a
   CRL, a certification/revocation request, or an RTCS/OCSP request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyUserCertInfo( INOUT CERT_INFO *certInfoPtr,
							 INOUT CERT_INFO *userCertInfoPtr,
							 IN_HANDLE const CRYPT_HANDLE iCryptHandle )
	{
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( userCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( userCertInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  userCertInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );
	REQUIRES( userCertInfoPtr->certificate != NULL );

	/* If it's an RTCS or OCSP request, remember the responder URL if there's
	   one present.  We can't leave it to be read out of the certificate 
	   because authorityInfoAccess isn't a valid attribute for RTCS/OCSP 
	   requests */
#ifdef USE_CERTREV
	if( certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST && \
		certInfoPtr->cCertRev->responderUrl == NULL )
		{
		int status;

		status = copyResponderURL( certInfoPtr, userCertInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
#endif /* USE_CERTREV */
#ifdef USE_CERTVAL
	if( certInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST && \
		certInfoPtr->cCertVal->responderUrl == NULL )
		{
		int status;

		status = copyResponderURL( certInfoPtr, userCertInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
#endif /* USE_CERTVAL */

	/* Copy the required information across to the certificate */
	switch( certInfoPtr->type )
		{
#ifdef USE_CERTREQ
		case CRYPT_CERTTYPE_REQUEST_CERT:
			return( copyCertToRequest( certInfoPtr, userCertInfoPtr,
									   iCryptHandle ) );

		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			return( copyCertToRevRequest( certInfoPtr, userCertInfoPtr ) );
#endif /* USE_CERTREQ */

#ifdef USE_CERTREV
		case CRYPT_CERTTYPE_CRL:
			return( copyRevocationInfo( certInfoPtr, userCertInfoPtr ) );

		case CRYPT_CERTTYPE_OCSP_REQUEST:
			return( copyCertToOCSPRequest( certInfoPtr, userCertInfoPtr ) );
#endif /* USE_CERTREV */

#ifdef USE_CERTVAL
		case CRYPT_CERTTYPE_RTCS_REQUEST:
			return( copyCertToRTCSRequest( certInfoPtr, userCertInfoPtr ) );
#endif /* USE_CERTVAL */
		}

	retIntError();
	}

/* A general dispatch function for copying information from one certificate 
   object to another.  This is a wrapper that acquires the source object's 
   data and then passes a pointer to it to the actual copy function that
   copies any necessary information to the destination object, releasing the
   source object after the copy */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyCertObject( INOUT CERT_INFO *certInfoPtr,
					IN_HANDLE const CRYPT_CERTIFICATE addedCert,
					IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
					IN const int certInfo )
	{
	CERT_INFO *addedCertInfoPtr;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isAttribute( certInfoType ) || \
			  isInternalAttribute( certInfoType ) );
	REQUIRES( isHandleRangeValid( addedCert ) );

	status = krnlAcquireObject( addedCert, OBJECT_TYPE_CERTIFICATE,
								( void ** ) &addedCertInfoPtr,
								CRYPT_ARGERROR_NUM1 );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( addedCertInfoPtr != NULL );
	switch( certInfoType )
		{
		case CRYPT_CERTINFO_CERTIFICATE:
			status = copyUserCertInfo( certInfoPtr, addedCertInfoPtr, 
									   certInfo );
			break;

#ifdef USE_CERTREV
		case CRYPT_CERTINFO_CACERTIFICATE:
			status = copyCaCertToOCSPReq( certInfoPtr, addedCertInfoPtr );
			break;
#endif /* USE_CERTREV */

#ifdef USE_CERTREQ
		case CRYPT_CERTINFO_CERTREQUEST:
			status = copyCertReqToCert( certInfoPtr, addedCertInfoPtr );
			break;
#endif /* USE_CERTREQ */

#ifdef USE_CERTVAL
		case CRYPT_IATTRIBUTE_RTCSREQUEST:
			status = copyRtcsReqToResp( certInfoPtr, addedCertInfoPtr );
			break;
#endif /* USE_CERTVAL */

#ifdef USE_CERTREV
		case CRYPT_IATTRIBUTE_OCSPREQUEST:
			status = copyOcspReqToResp( certInfoPtr, addedCertInfoPtr );
			break;

		case CRYPT_IATTRIBUTE_REVREQUEST:
			status = copyRevReqToCert( certInfoPtr, addedCertInfoPtr );
			break;
#endif /* USE_CERTREV */

#ifdef USE_PKIUSER
		case CRYPT_IATTRIBUTE_PKIUSERINFO:
			status = copyPkiUserToCertReq( certInfoPtr, addedCertInfoPtr );
			break;
#endif /* USE_PKIUSER */

#ifdef USE_DBMS	/* Only used by CA code */
		case CRYPT_IATTRIBUTE_BLOCKEDATTRS:
			status = sanitiseCertAttributes( certInfoPtr,
											 addedCertInfoPtr->attributes );
			break;
#endif /* USE_DBMS */

		default:
			retIntError();
		}
	krnlReleaseObject( addedCertInfoPtr->objectHandle );
	return( status );
	}
#endif /* USE_CERTIFICATES */
