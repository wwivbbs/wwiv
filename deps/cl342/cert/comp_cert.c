/****************************************************************************
*																			*
*							Set Certificate Components						*
*						Copyright Peter Gutmann 1997-2011					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

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
	int urlSize = DUMMY_INIT, status;

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
		status = selectGeneralName( userCertInfoPtr,
									CRYPT_ATTRIBUTE_NONE,
									MUST_BE_PRESENT );
	if( cryptStatusOK( status ) )
		status = getCertComponentString( userCertInfoPtr,
								CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER,
								NULL, 0, &urlSize );
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
			requestInfoPtr->cCertVal->responderUrl = responderUrl;
			requestInfoPtr->cCertVal->responderUrlSize = urlSize;
			}
		else
			{
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

#if defined( USE_CERTREQ ) || defined( USE_CERTREV )

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

/****************************************************************************
*																			*
*							Copy Certificate Data							*
*																			*
****************************************************************************/

/* Copy public key data into a certificate object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyPublicKeyInfo( INOUT CERT_INFO *certInfoPtr,
					   IN_HANDLE_OPT const CRYPT_HANDLE cryptHandle,
					   IN_OPT const CERT_INFO *srcCertInfoPtr )
	{
	CRYPT_CONTEXT iCryptContext;
	MESSAGE_DATA msgData;
	void *publicKeyInfoPtr;
	int isXyzzyCert, length = DUMMY_INIT, status;

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
		{
		REQUIRES( memcmp( srcCertInfoPtr->publicKeyID,
						  "\x00\x00\x00\x00\x00\x00\x00\x00", 8 ) );

		if( ( publicKeyInfoPtr = \
					clAlloc( "copyPublicKeyInfo", 
							 srcCertInfoPtr->publicKeyInfoSize ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		memcpy( publicKeyInfoPtr, srcCertInfoPtr->publicKeyInfo, 
				srcCertInfoPtr->publicKeyInfoSize );
		certInfoPtr->publicKeyData = certInfoPtr->publicKeyInfo = \
			publicKeyInfoPtr;
		certInfoPtr->publicKeyInfoSize = srcCertInfoPtr->publicKeyInfoSize;
		certInfoPtr->publicKeyAlgo = srcCertInfoPtr->publicKeyAlgo;
		certInfoPtr->publicKeyFeatures = srcCertInfoPtr->publicKeyFeatures;
		memcpy( certInfoPtr->publicKeyID, srcCertInfoPtr->publicKeyID,
				KEYID_SIZE );
		certInfoPtr->flags |= CERT_FLAG_DATAONLY;

		return( CRYPT_OK );
		}

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

	/* Get the key information */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &certInfoPtr->publicKeyAlgo,
							  CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
								  &certInfoPtr->publicKeyFeatures,
								  CRYPT_IATTRIBUTE_KEYFEATURES );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, certInfoPtr->publicKeyID, KEYID_SIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a XYZZY certificate then we need to check that the key is 
	   signature-capable (or at least sig-check capable, since it may be
	   just the public key that's being added), since XYZZY certificates 
	   are self-signed */
	status = getCertComponent( certInfoPtr, CRYPT_CERTINFO_XYZZY, 
							   &isXyzzyCert );
	if( cryptStatusOK( status ) && isXyzzyCert )
		{
		int keyUsage = KEYUSAGE_SIGN | KEYUSAGE_CA;

		/* Make sure that the key is signature-capable.  We have to check 
		   for a capability to either sign or sig-check since a pure public 
		   key will only have a sig-check capability while a private key 
		   held in a device (from which we're going to extract the public-
		   key components) may be only signature-capable */
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

		/* Clear the existing usage and replace it with our usage.  See 
		   the comment in setXyzzyInfo() for why it's done this way */
		( void ) deleteCertComponent( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE );
		status = addCertComponent( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE,
								   keyUsage );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Copy over the public-key data.  We copy the data rather than keeping 
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
	certInfoPtr->publicKeyData = certInfoPtr->publicKeyInfo = \
		publicKeyInfoPtr;
	certInfoPtr->publicKeyInfoSize = length;
	certInfoPtr->flags |= CERT_FLAG_DATAONLY;

	return( CRYPT_OK );
	}

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
   anyway):

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
   requirements in digital signature legislation, who will be creating their 
   own certificates from scratch themselves rather than using cryptlib for 
   it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkCertWellFormed( INOUT CERT_INFO *certInfoPtr )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* Check that the subject DN is well-formed.  There can potentially be 
	   any number of other DNs in a certificate (either as the issuer DN or
	   hidden in GeneralName fields), it's not really certain what we should
	   do with these (or even how we can effectively track them all down).  
	   Since the reason why we're doing this in the first place is to avoid 
	   shenanigans due to the more or less arbitrary handling of complex DNs 
	   by implementations, and the only one that anyone really pays any 
	   attention to is the subject name, we only check the subject name */
	status = checkDN( certInfoPtr->subjectName, 
					  CHECKDN_FLAG_COUNTRY | CHECKDN_FLAG_COMMONNAME | \
							CHECKDN_FLAG_WELLFORMED,
					  &certInfoPtr->errorLocus,
					  &certInfoPtr->errorType );
	if( cryptStatusError( status ) )
		return( status );

#if 0	/* Not really sure what else we should be checking for, or 
		   disallowing, here */
	SELECTION_STATE selectionState;

	/* Check that the subject altName is well-formed.  The reason for 
	   checking just this particular field are as for the subject DN check
	   above */
	saveSelectionState( selectionState, certInfoPtr );
	status = addCertComponent( certInfoPtr, CRYPT_ATTRIBUTE_CURRENT, 
							   CRYPT_CERTINFO_SUBJECTALTNAME );
	if( cryptStatusOK( status ) )
		{
		}
	restoreSelectionState( selectionState, certInfoPtr );
#endif /* 0 */

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

#if 0	/* 10/5/09 The original behaviour of this function was as its name
				   suggests, disallowed attribute values were selectively 
				   disabled and, if any permitted values were still present, 
				   the remainder were allowed (this assumed that the value 
				   was a bitflag, which occurs for the only attribute that's
				   currently handled this way, the keyUsage).  However this
				   probably isn't the right behaviour, if there's any
				   disallowed information present then the certificate-
				   creation process as a whole should be rejected rather 
				   than returning a certificate that's different from what 
				   was requested */
		int constrainedAttributeValue, constrainingAttributeValue;

		/* Get the attribute values and, if there's no conflict, continue */
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

		/* The attribute contains a value that's disallowed by the
		   constraining attribute, correct it if possible */
		value = constrainedAttributeValue & ~constrainingAttributeValue;
		if( value <= 0 )
			{
			/* The attribute contains only invalid bits and can't be
			   permitted */
			status = getAttributeIdInfo( certAttributePtr, NULL, &fieldID, 
										 NULL );
			if( cryptStatusOK( status ) )
				{
				certInfoPtr->errorLocus = fieldID;
				certInfoPtr->errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				}
			return( CRYPT_ERROR_INVALID );
			}
		setAttributeProperty( certAttributePtr, ATTRIBUTE_PROPERTY_VALUE, 
							  value );		/* Set adjusted value */
#else
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
#endif /* 0 */
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( checkCertWellFormed( certInfoPtr ) );
	}

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
		if( cryptStatusError( status ) )
			deleteDN( &certInfoPtr->subjectName );
		}
	if( cryptStatusError( status ) )
		return( status );

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
	int idLength = DUMMY_INIT, status;

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
	void *dataPtr = DUMMY_INIT_PTR;
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
*							Copy PKI User Data								*
*																			*
****************************************************************************/

#ifdef USE_PKIUSER

/* Set or modify data in a certificate request based on the PKI user 
   information.  This is rather more complicated than the standard copy
   operations because we potentially have to merge information already
   present in the request with information in the PKI user object, checking
   for consistency/conflicts in the process.
   
   The attributes that can be specified in requests are severely limited 
   (see the long comment for sanitiseCertAttributes() in comp_set.c) so
   the only ones that we really need to handle are the altName and a special-
   case keyUsage check for CA key usages, along with special handling for 
   the SCEP challenge password (see the comment in the code below) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyPkiUserAttributes( INOUT CERT_INFO *certInfoPtr,
								  INOUT ATTRIBUTE_PTR *pkiUserAttributes )
	{
	ATTRIBUTE_PTR *requestAttrPtr, *pkiUserAttrPtr;
	int value, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( pkiUserAttributes, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
			  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );

	/* If there are altNames present in both the PKI user data and the
	   request, make sure that they match */
	requestAttrPtr = findAttribute( certInfoPtr->attributes,
									CRYPT_CERTINFO_SUBJECTALTNAME, FALSE );
	pkiUserAttrPtr = findAttribute( pkiUserAttributes,
									CRYPT_CERTINFO_SUBJECTALTNAME, FALSE );
	if( requestAttrPtr != NULL && pkiUserAttrPtr != NULL )
		{
		/* Both the certificate request and the PKI user have altNames,
		   make sure that they're identical */
		if( !compareAttribute( requestAttrPtr, pkiUserAttrPtr ) )
			{
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_SUBJECTALTNAME,
						  CRYPT_ERRTYPE_ISSUERCONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}

		/* The two altNames are identical, delete the one in the request to
		   allow the one from the PKI user data to be copied across */
		status = deleteAttribute( &certInfoPtr->attributes,
								  &certInfoPtr->attributeCursor, 
								  requestAttrPtr,
								  certInfoPtr->currentSelection.dnPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* There's one rather ugly special-case situation that we have to handle 
	   which is when the user has submitted a PnP PKI request for a generic 
	   signing certificate but their PKI user information indicates that 
	   they're intended to be a CA user.  The processing flow for this is:

			Read request data from an external source into certificate 
				request object, creating a state=high object;

			Add PKI user information to state=high request;

	   When augmenting the request with the PKI user information the 
	   incoming request will contain a keyUsage of digitalSignature while 
	   the PKI user information will contain a keyUsage of keyCertSign 
	   and/or crlSign.  We can't fix this up at the PnP processing level 
	   because the request object will be in the high state once it's
	   instantiated and no changes to the attributes can be made (the PKI 
	   user information is a special case that can be added to an object in 
	   the high state but which modifies attributes in it as if it were 
	   still in the low state).

	   To avoid the attribute conflict, if we find this situation in the 
	   request/pkiUser combination we delete the keyUsage in the request to 
	   allow it to be replaced by the pkiUser keyUsage.  Hardcoding in this 
	   special case isn't very elegant but it's the only way to make the PnP 
	   PKI issue work without requiring that the user explicitly specify 
	   that they want to be a CA in the request's keyUsage, which makes it 
	   rather non-PnP and would also lead to slightly strange requests since
	   basicConstraints can't be specified in requests while the CA keyUsage
	   can */
	status = getAttributeFieldValue( certInfoPtr->attributes,
									 CRYPT_CERTINFO_KEYUSAGE, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) && value == CRYPT_KEYUSAGE_DIGITALSIGNATURE )
		{
		status = getAttributeFieldValue( pkiUserAttributes, 
										 CRYPT_CERTINFO_KEYUSAGE,
										 CRYPT_ATTRIBUTE_NONE, &value );
		if( cryptStatusOK( status ) && ( value & KEYUSAGE_CA ) )
			{
			/* The certificate contains a digitalSignature keyUsage and the 
			   PKI user information contains a CA usage, delete the 
			   certificate's keyUsage to make way for the PKI user's CA 
			   keyUsage */
			status = deleteCompleteAttribute( &certInfoPtr->attributes,
											  &certInfoPtr->attributeCursor, 
											  CRYPT_CERTINFO_KEYUSAGE, 
											  certInfoPtr->currentSelection.dnPtr );
			if( cryptStatusError( status ) )
				return( status );
			}
		}

	/* Copy the attributes from the PKI user information into the 
	   certificate */
	status = copyAttributes( &certInfoPtr->attributes, pkiUserAttributes,
							 &certInfoPtr->errorLocus,
							 &certInfoPtr->errorType );
	if( cryptStatusError( status ) )
		return( status );

	/* There's another special that we have to handle which occurs for the 
	   SCEP challenge password, which the SCEP protocol requires to be 
	   placed in the PKCS #10 request instead of being included in the SCEP 
	   metadata.  This shouldn't be copied across to the certificate since 
	   the NOCOPY flag is set for the attribute, but to make absolutely 
	   certain we try and delete it anyway.  Since this could in theory be 
	   present in non-SCEP requests as well (the request-processing code 
	   just knows about PKCS #10 requests as a whole, not requests from a 
	   SCEP source vs. requests not from a SCEP source), we make the delete 
	   unconditional */
	if( findAttributeField( certInfoPtr->attributes,
							CRYPT_CERTINFO_CHALLENGEPASSWORD, 
							CRYPT_ATTRIBUTE_NONE ) != NULL )
		{
		status = deleteCompleteAttribute( &certInfoPtr->attributes,
										  &certInfoPtr->attributeCursor, 
										  CRYPT_CERTINFO_CHALLENGEPASSWORD, 
										  certInfoPtr->currentSelection.dnPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* The PKI user information contains an sKID that's used to uniquely 
	   identify the user, this applies to the user information itself rather 
	   than the certificate that'll be issued from it.  Since this will have 
	   been copied over alongside the other attributes we need to explicitly 
	   delete it before we continue */
	if( findAttributeField( certInfoPtr->attributes,
							CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER, 
							CRYPT_ATTRIBUTE_NONE ) != NULL )
		{
		status = deleteCompleteAttribute( &certInfoPtr->attributes,
										  &certInfoPtr->attributeCursor, 
										  CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER, 
										  certInfoPtr->currentSelection.dnPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int assemblePkiUserDN( INOUT CERT_INFO *certInfoPtr,
							  const DN_PTR *pkiUserSubjectName,
							  IN_BUFFER( commonNameLength ) const void *commonName, 
							  IN_LENGTH_SHORT const int commonNameLength )
	{
	STREAM stream;
	void *tempDN = NULL, *tempDNdata;
	int tempDNsize = DUMMY_INIT, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( commonName, commonNameLength ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );
	REQUIRES( pkiUserSubjectName != NULL );
	REQUIRES( commonNameLength > 0 && \
			  commonNameLength < MAX_INTLENGTH_SHORT );

	/* Copy the DN template, append the user-supplied CN, and allocate room 
	   for the encoded form */
	status = copyDN( &tempDN, pkiUserSubjectName );
	if( cryptStatusError( status ) )
		return( status );
	status = insertDNComponent( &tempDN, CRYPT_CERTINFO_COMMONNAME,
								commonName, commonNameLength,
								&certInfoPtr->errorType );
	if( cryptStatusOK( status ) )
		status = tempDNsize = sizeofDN( tempDN );
	if( cryptStatusError( status ) )
		{
		deleteDN( &tempDN );
		return( status );
		}
	if( ( tempDNdata = clAlloc( "assemblePkiUserDN", tempDNsize ) ) == NULL )
		{
		deleteDN( &tempDN );
		return( CRYPT_ERROR_MEMORY );
		}

	/* Replace the existing DN with the new one and set up the encoded 
	   form.  At this point we could already have an encoded DN present if 
	   the request was read from encoded form, with the subject DN pointer
	   pointing into the encoded request data (if the request was created 
	   from scratch then there's no DN present).  We replace the (possible)
	   existing pointer into the certificate data with a pointer to the
	   updated encoded DN written to our newly-allocated block of memory */
	deleteDN( &certInfoPtr->subjectName );
	certInfoPtr->subjectName = tempDN;
	sMemOpen( &stream, tempDNdata, tempDNsize );
	status = writeDN( &stream, tempDN, DEFAULT_TAG );
	ENSURES( cryptStatusOK( status ) );
	sMemDisconnect( &stream );
	certInfoPtr->subjectDNdata = certInfoPtr->subjectDNptr = tempDNdata;
	certInfoPtr->subjectDNsize = tempDNsize;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int copyPkiUserToCertReq( INOUT CERT_INFO *certInfoPtr,
								 INOUT CERT_INFO *pkiUserInfoPtr )
	{
	CRYPT_ATTRIBUTE_TYPE dnComponentType;
	DN_PTR *requestDNSubset, *pkiUserDNSubset;
	BOOLEAN dnContinues;
	int dummy, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( pkiUserInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
			  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );
	REQUIRES( pkiUserInfoPtr->type == CRYPT_CERTTYPE_PKIUSER );
	REQUIRES( pkiUserInfoPtr->certificate != NULL );

	/* Because a DN can be made up of elements from both the certificate 
	   request and the PKI user information, it's possible that neither can
	   contain a DN, expecting it to be supplied by the other side.  
	   Although the lack of a DN would be caught anyway when the certificate
	   is signed, it's better to alert the caller at this early stage rather
	   than later on in the certification process.  First we check for an
	   overall missing DN and then for the more specific case of a missing 
	   CN.

	   Note that both here and in all of the following checks we return an
	   error status of CRYPT_ERROR_INVALID rather than CRYPT_ERROR_NOTINITED
	   since this is a validity check of the certificate request against the
	   PKI user information and not a general object-ready-to-encode check */
	if( certInfoPtr->subjectName == NULL && \
		pkiUserInfoPtr->subjectName == NULL )
		{
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_SUBJECTNAME,
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_INVALID );
		}
	if( ( certInfoPtr->subjectName == NULL || \
		  cryptStatusError( \
				getDNComponentValue( certInfoPtr->subjectName, 
									 CRYPT_CERTINFO_COMMONNAME, 0, NULL, 0, 
									 &dummy ) ) ) && \
		( pkiUserInfoPtr->subjectName == NULL || \
		  cryptStatusError( \
				getDNComponentValue( pkiUserInfoPtr->subjectName, 
									 CRYPT_CERTINFO_COMMONNAME, 0, NULL, 0, 
									 &dummy ) ) ) )
		{
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_COMMONNAME,
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If there's no DN present in the request then one has been supplied by 
	   the CA in the PKI user information, copy over the DN and its encoded 
	   form from the user information */
	if( certInfoPtr->subjectName == NULL )
		{
		status = copyDN( &certInfoPtr->subjectName,
						 pkiUserInfoPtr->subjectName );
		if( cryptStatusError( status ) )
			return( status );
		ENSURES( pkiUserInfoPtr->subjectDNptr != NULL );
		if( ( certInfoPtr->subjectDNdata = \
					clAlloc( "copyPkiUserToCertReq",
							 pkiUserInfoPtr->subjectDNsize ) ) == NULL )
			{
			deleteDN( &certInfoPtr->subjectName );
			return( CRYPT_ERROR_MEMORY );
			}
		memcpy( certInfoPtr->subjectDNdata, pkiUserInfoPtr->subjectDNptr,
				pkiUserInfoPtr->subjectDNsize );
		certInfoPtr->subjectDNptr = certInfoPtr->subjectDNdata;
		certInfoPtr->subjectDNsize = pkiUserInfoPtr->subjectDNsize;

		/* Copy any additional attributes across */
		return( copyPkiUserAttributes( certInfoPtr,
									   pkiUserInfoPtr->attributes ) );
		}

	/* If there's no PKI user DN with the potential to conflict with the one
	   in the request present, copy any additional attributes across and
	   exit */
	if( pkiUserInfoPtr->subjectName == NULL )
		{
		ENSURES( certInfoPtr->subjectName != NULL );
		return( copyPkiUserAttributes( certInfoPtr,
									   pkiUserInfoPtr->attributes ) );
		}

	/* If there are full DNs present in both objects and they match, copy 
	   any additional attributes across and exit */
	if( compareDN( certInfoPtr->subjectName,
				   pkiUserInfoPtr->subjectName, FALSE, NULL ) )
		{
		return( copyPkiUserAttributes( certInfoPtr,
									   pkiUserInfoPtr->attributes ) );
		}

	/* Now things get complicated because there are distinct request DN and 
	   PKI user DNs present and we need to reconcile the two.  Typically the 
	   CA will have provided a partial DN with the user providing the rest:

		pkiUser: A - B - C - D
		request: A - B - C - D - E - F

	   in which case we could just allow the request DN (in theory we're 
	   merging the two, but since the pkiUser DN is a proper subset of the 
	   request DN it just acts as a filter).  The real problem though occurs 
	   when we get a request like:

		pkiUser: A - B - C - D
		request: X

	   where X can be any of:

		'D': Legal, this duplicates a single permitted element
		'E': Legal, this adds a new sub-element
		'D - E': Legal, this duplicates a legal element and adds a new sub-
			element
		'A - B - C - D#' where 'D#' doesn't match 'D', for example it's a CN 
			that differs from the pkiUser CN: Not legal since it violates a
			CA-imposed constraint.
		'A - B - C - D - E - F': Legal, this is the scenario given above 
			where one is a proper subset of the other.
		'D - A': This is somewhat dubious and probably not legal, for 
			example if 'A' is a countryName.
		'D - A - B - C - D': This is almost certainly not legal because in a 
			hierarchy like this 'A' is probably a countryName so an attempt 
			to merge the request DN at point 'D' in the pkiUser DN could 
			lead to a malformed DN.
		'E - C - D - E': This may or may not be legal if the middle elements 
			are things like O's and OU's, which can be mixed up arbitrarily.

	   Handling this really requires human intervention to decide what's 
	   legal or not.  In the absence of an AI capability in the software
	   we restrict ourselves to allowing only two cases:

		1. If the PKI user contains a DN without a CN and the request
		   contains only a CN (typically used where the CA provides a
		   template for the user's DN and the user supplies only their
		   name), we merge the CA-provided DN-without-CN with the user-
		   provided CN.

		2. If the PKI user contains a DN without a CN and the request
		   contains a full DN (a variation of the above where the user
		   knows their full DN), we make sure that the rest of the DN
		   matches.

	   First we check that the user DN contains a CN, optionally preceded
	   by a prefix of the PKI user's DN, by passing them to compareDN(), 
	   which checks whether the first DN is a proper subset of the second.  
	   Some sample DN configurations and their results are:

		request = CN				result = FALSE, mismatchSubset = CN
		pkiUser = C - O - OU

		request = ... X				result = FALSE, mismatchSubset = X
		pkiUser = C - O - OU

		request = C - O - OU - CN	result = FALSE, mismatchSubset = CN
		pkiUser = C - O - OU

		request = C - O - OU - CN1	result = FALSE, mismatchSubset = CN1
		pkiUser = C - O - OU - CN2

		request = C - O - OU		result = TRUE, mismatchSubset = NULL
		pkiUser = C - O - OU - CN

	   The set of DNs with a prefix-match-result of TRUE (in other words the
	   request contains a subset of the PKI user's DN) are trivially 
	   invalid, so we can reject them immediately */
	if( compareDN( certInfoPtr->subjectName, pkiUserInfoPtr->subjectName, 
				   TRUE, &requestDNSubset ) )
		{
		/* The issuer-constraint error is technically accurate if a little 
		   unexpected at this point, since the PKIUser information has been 
		   set as a constraint by the CA */
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_SUBJECTNAME,
					  CRYPT_ERRTYPE_ISSUERCONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}
	ANALYSER_HINT( requestDNSubset != NULL );

	/* The request DN is a prefix (possibly an empty one) of the PKI user 
	   DN, check that the mis-matching part is only a CN */
	status = getDNComponentInfo( requestDNSubset, &dnComponentType, 
								 &dnContinues );
	if( cryptStatusError( status ) )
		{
		/* This is an even more problematic situation to report (since it's
		   a shouldn't-occur type error), the best that we can do is report
		   a generic issuer constraint */
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_SUBJECTNAME,
					  CRYPT_ERRTYPE_ISSUERCONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}
	if( dnComponentType != CRYPT_CERTINFO_COMMONNAME || dnContinues )
		{
		/* Check for the special case of there being no CN at all present in 
		   the request DN, which lets us return a more specific error 
		   indicator (this is a more specific case of the check performed
		   earlier for a CN in the request or the PKI user object) */
		status = getDNComponentValue( requestDNSubset, 
									  CRYPT_CERTINFO_COMMONNAME, 0, NULL, 0, 
									  &dummy );
		if( cryptStatusError( status ) )
			{
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_COMMONNAME,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_INVALID );
			}

		/* The mismatching portion of the request DN isn't just a CN */
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_SUBJECTNAME,
					  CRYPT_ERRTYPE_ISSUERCONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* There's one special case that we have to handle where both DNs 
	   consist of identical prefixes ending in CNs but the CNs differ, which 
	   passes the above check.  We do this by reversing the order of the 
	   match performed above, if what's left is also a CN then there was a 
	   mismatch in the CNs */
	if( !compareDN( pkiUserInfoPtr->subjectName, certInfoPtr->subjectName,
					TRUE, &pkiUserDNSubset ) )
		{
		status = getDNComponentInfo( pkiUserDNSubset, &dnComponentType, 
									 &dnContinues );
		if( cryptStatusError( status ) || \
			dnComponentType == CRYPT_CERTINFO_COMMONNAME )
			{
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_COMMONNAME,
						  CRYPT_ERRTYPE_ISSUERCONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* If the request DN consists only of a CN, replace the request DN with 
	   the merged DN prefix from the PKI user DN and the CN from the request 
	   DN.  Otherwise, the request DN contains a full DN including a CN that
	   matches the full PKI user DN and there's nothing to do beyond the 
	   checks that we've already performed above */
	if( certInfoPtr->subjectName == requestDNSubset )
		{
		char commonName[ CRYPT_MAX_TEXTSIZE + 8 ];
		int commonNameLength;

		status = getDNComponentValue( requestDNSubset, 
									  CRYPT_CERTINFO_COMMONNAME, 0, 
									  commonName, CRYPT_MAX_TEXTSIZE, 
									  &commonNameLength );
		if( cryptStatusOK( status ) )
		status = assemblePkiUserDN( certInfoPtr,
									pkiUserInfoPtr->subjectName,
									commonName, commonNameLength );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Copy any additional attributes across */
	return( copyPkiUserAttributes( certInfoPtr, pkiUserInfoPtr->attributes ) );
	}
#endif /* USE_PKIUSER */

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

		case CRYPT_IATTRIBUTE_BLOCKEDATTRS:
			status = sanitiseCertAttributes( certInfoPtr,
											 addedCertInfoPtr->attributes );
			break;

		default:
			retIntError();
		}
	krnlReleaseObject( addedCertInfoPtr->objectHandle );
	return( status );
	}
#endif /* USE_CERTIFICATES */
