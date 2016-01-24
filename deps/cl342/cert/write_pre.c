/****************************************************************************
*																			*
*					Certificate Write Preparation Routines					*
*						Copyright Peter Gutmann 1996-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

/* Before we encode a certificate object we have to perform various final 
   setup actions and check that the object is ready for encoding.  The setup 
   operations and checks for the different object types are:

				|  Cert	|  Attr	|  P10	|Cr.Req	|Rv.Req	
	------------+-------+-------+-------+-------+-------+
	STDATTR		|	X[1]|		|		|		|		| Setup 
	ISSUERATTR	|	X	|	X	|		|		|		| action
	ISSUERDN	|	X	|	X	|		|		|		|
	VALPERIOD	|	X	|	X	|		|		|		|
	VALINFO		|		|		|		|		|		|
	REVINFO		|		|		|		|		|		|
	------------+-------+-------+-------+-------+-------+
	SPKI		|	X	|		|	X	|	X	|		| Check
	DN			|	X	|	X	|		|		|		|
	DN_PART		|		|		|	X	|	X	|		|
	ISSUERDN	|	X	|	X	|		|		|	X	|
	ISSUERCRTDN	|		|		|		|		|		|
	NON_SELFSD	|	X	|	X	|		|		|		|
	SERIALNO	|	X	|	X	|		|		|	X	|
	REVENTRIES	|		|		|		|		|		|
	------------+-------+-------+-------+-------+-------+

				|RTCS Rq|RTCS Rs|OCSP Rq|OCSP Rs|  CRL	|CRLentr|
	------------+-------+-------+-------+-------+-------+-------+
	STDATTR		|		|		|		|		|		|		| Setup 
	ISSUERATTR	|		|		|		|		|	X	|		| action
	ISSUERDN	|		|		|		|		|	X	|		|
	VALPERIOD	|		|		|		|		|		|		|
	VALINFO		|	X	|		|		|		|		|		|
	REVINFO		|		|		|	X	|		|	X	|	X	|
	------------+-------+-------+-------+-------+-------+-------+
	SPKI		|		|		|		|		|		|		| Check
	DN			|		|		|		|	X	|		|		|
	DN_PART		|		|		|		|		|		|		|
	ISSUERDN	|		|		|		|		|	X	|		|
	ISSUERCRTDN	|		|		|		|		|	X	|		|
	NON_SELFSD	|		|		|		|		|		|		|
	SERIALNO	|		|		|		|		|		|		|
	VALENTRIES	|	X	|		|		|		|		|		|
	REVENTRIES	|		|		|	X	|	X	|		|		|
	------------+-------+-------+-------+-------+-------+-------+ 

   We have to be careful here to avoid race conditions when some of the 
   checks depend on setup actions having been performed first but some of
   the setup actions require that checks be performed first.  The noted
   exceptions are:

	[1] Requires that the SPKI check be performed first since STDATTR
		evaluates keyUsage from the SPKI */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Add standard X.509v3 extensions to a certificate if they're not already 
   present.  This function simply adds the required extensions, it doesn't 
   check for consistency with existing extensions which is done later by 
   checkAttributes() and checkCert() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int addStandardExtensions( INOUT CERT_INFO *certInfoPtr )
	{
	BOOLEAN isCA = FALSE;
	int keyUsage, extKeyUsage, value, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* Get the implicit keyUsage flags (based on any extended key usage 
	   extensions present) and explicit key usage flags, which we use to 
	   extend the basic keyUsage flags if required */
	status = getKeyUsageFromExtKeyUsage( certInfoPtr, &extKeyUsage,
						&certInfoPtr->errorLocus, &certInfoPtr->errorType );
	if( cryptStatusError( status ) )
		return( status );
	status = getAttributeFieldValue( certInfoPtr->attributes,
									 CRYPT_CERTINFO_KEYUSAGE,
									 CRYPT_ATTRIBUTE_NONE, &keyUsage );
	if( cryptStatusError( status ) )
		{
		if( status != CRYPT_ERROR_NOTFOUND )
			return( status );

		/* There's no keyUsage attribute present, mark the value as being 
		   not set so that we explicitly set it later */
		keyUsage = CRYPT_ERROR;
		}

	/* If there's an explicit key usage present, make sure that it's
	   consistent with the implicit key usage flags derived from the 
	   extended key usage.  We mask out the nonRepudiation bit for reasons 
	   given in chk_cert.c.

	   This check is also performed by checkCert(), however we need to
	   explicitly perform it here as well since we need to add a key usage 
	   to match the extKeyUsage before calling checkCert() if one wasn't
	   explicitly set or checkCert() will reject the certificate because of 
	   the inconsistent keyUsage */
	if( keyUsage > 0 )
		{
		const int effectiveKeyUsage = \
						extKeyUsage & ~CRYPT_KEYUSAGE_NONREPUDIATION;

		if( ( keyUsage & effectiveKeyUsage ) != effectiveKeyUsage )
			{
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE,
						  CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* Check whether this is a CA certificate.  If there's no 
	   basicConstraints attribute present, add one and make it a non-CA 
	   certificate */
	status = getAttributeFieldValue( certInfoPtr->attributes,
									 CRYPT_CERTINFO_CA, CRYPT_ATTRIBUTE_NONE,
									 &value );
	if( cryptStatusOK( status ) )
		isCA = ( value > 0 ) ? TRUE : FALSE;
	else
		{
		status = addCertComponent( certInfoPtr, CRYPT_CERTINFO_CA, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If there's no explicit keyUsage information present add it based on
	   various implicit information.  We also add key feature information
	   which is used to help automate key management, for example to inhibit
	   speculative reads of keys held in removable tokens, which can result
	   in spurious insert-token dialogs being presented to the user outside
	   the control of cryptlib if the token isn't present */
	if( keyUsage <= 0 )
		{
		/* If there's no implicit key usage present and it's not a CA (for 
		   which we don't want to set things like encryption flags for the
		   CA certificate), set the key usage flags based on the 
		   capabilities of the associated context.  Because no-one can 
		   figure out what the nonRepudiation flag signifies we don't set 
		   this, if the user wants it they have to specify it explicitly.  
		   Similarly we don't try and set the keyAgreement encipher/decipher-
		   only flags, which were tacked on as variants of keyAgreement long 
		   after the basic keyAgreement flag was defined */
		if( extKeyUsage <= 0 && !isCA )
			{
			keyUsage = 0;	/* Reset key usage */
			if( certInfoPtr->iPubkeyContext != CRYPT_ERROR )
				{
				/* There's a context present, check its capabilities.  This
				   has the advantage that it takes into account any ACLs
				   that may exist for the key */
				if( cryptStatusOK( \
						krnlSendMessage( certInfoPtr->iPubkeyContext, 
										 IMESSAGE_CHECK, NULL, 
										 MESSAGE_CHECK_PKC_SIGCHECK ) ) )
					keyUsage = CRYPT_KEYUSAGE_DIGITALSIGNATURE;
				if( cryptStatusOK( \
						krnlSendMessage( certInfoPtr->iPubkeyContext, 
										 IMESSAGE_CHECK, NULL, 
										 MESSAGE_CHECK_PKC_ENCRYPT ) ) )
					keyUsage |= CRYPT_KEYUSAGE_KEYENCIPHERMENT;
				if( cryptStatusOK( \
						krnlSendMessage( certInfoPtr->iPubkeyContext, 
										 IMESSAGE_CHECK, NULL, 
										 MESSAGE_CHECK_PKC_KA_EXPORT ) ) || \
					cryptStatusOK( \
						krnlSendMessage( certInfoPtr->iPubkeyContext, 
										 IMESSAGE_CHECK, NULL, 
										 MESSAGE_CHECK_PKC_KA_IMPORT ) ) )
					keyUsage |= CRYPT_KEYUSAGE_KEYAGREEMENT;
				}
			else
				{
				/* There's no context present (the key is present as encoded
				   data), assume we can do whatever the algorithm allows */
				if( isSigAlgo( certInfoPtr->publicKeyAlgo ) )
					keyUsage = CRYPT_KEYUSAGE_DIGITALSIGNATURE;
				if( isCryptAlgo( certInfoPtr->publicKeyAlgo ) )
					keyUsage |= CRYPT_KEYUSAGE_KEYENCIPHERMENT;
				if( isKeyxAlgo( certInfoPtr->publicKeyAlgo ) )
					keyUsage |= CRYPT_KEYUSAGE_KEYAGREEMENT;
				}
			}
		else
			{
			/* There's an extended key usage set but no basic keyUsage, make 
			   the keyUsage consistent with the usage flags derived from the 
			   extended usage */
			keyUsage = extKeyUsage;

			/* If it's a CA key, make sure that it's a signing key and
			   enable its use for certification-related purposes*/
			if( isCA )
				{
				BOOLEAN usageOK;

				if( certInfoPtr->iPubkeyContext != CRYPT_ERROR )
					{
					usageOK = cryptStatusOK( \
								krnlSendMessage( certInfoPtr->iPubkeyContext, 
												 IMESSAGE_CHECK, NULL, 
												 MESSAGE_CHECK_PKC_SIGCHECK ) );
					}
				else
					usageOK = isSigAlgo( certInfoPtr->publicKeyAlgo );
				if( !usageOK )
					{
					setErrorInfo( certInfoPtr, CRYPT_CERTINFO_CA,
								  CRYPT_ERRTYPE_CONSTRAINT );
					return( CRYPT_ERROR_INVALID );
					}
				keyUsage |= KEYUSAGE_CA;
				}
			}
		ENSURES( keyUsage > CRYPT_KEYUSAGE_NONE && \
				 keyUsage < CRYPT_KEYUSAGE_LAST );
		status = addCertComponent( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE,
								   keyUsage );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( certInfoPtr->publicKeyFeatures > 0 )
		{
		/* This is a bitstring so we only add it if there are feature flags
		   present to avoid writing zero-length values */
		status = addCertComponent( certInfoPtr, CRYPT_CERTINFO_KEYFEATURES,
								   certInfoPtr->publicKeyFeatures );
		if( cryptStatusError( status ) && status != CRYPT_ERROR_INITED )
			return( status );
		}

	/* Add the subjectKeyIdentifier */
	return( addCertComponentString( certInfoPtr, 
									CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER,
									certInfoPtr->publicKeyID, KEYID_SIZE ) );
	}

/****************************************************************************
*																			*
*							Pre-encode Checking Functions					*
*																			*
****************************************************************************/

/* Check whether an empty DN is permitted in a certificate.  This is a PKIX 
   peculiarity that causes severe problems for virtually all certificate-
   using protocols so we only allow it at a compliance level of 
   CRYPT_COMPLIANCELEVEL_PKIX_FULL */

#ifdef USE_CERTLEVEL_PKIX_FULL

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkEmptyDnOK( INOUT CERT_INFO *subjectCertInfoPtr )
	{
	ATTRIBUTE_PTR *attributePtr;
	int value, complianceLevel, status;

	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );

	/* PKIX allows empty subject DNs if a subject altName is present, 
	   however creating certificates like this breaks every certificate-
	   using protocol supported by cryptlib so we only allow it at the 
	   highest compliance level */
	if( cryptStatusError( \
			krnlSendMessage( subjectCertInfoPtr->ownerHandle,
							 IMESSAGE_GETATTRIBUTE, &complianceLevel,
							 CRYPT_OPTION_CERT_COMPLIANCELEVEL ) ) || \
		complianceLevel < CRYPT_COMPLIANCELEVEL_PKIX_FULL )
		{
		/* We only allow this behaviour at the highest compliance level */
		return( FALSE );
		}
	   
	/* We also have to be very careful to ensure that the empty subject 
	   DN can't end up becoming an empty issuer DN, which can occur if it's 
	   a self-signed certificate */
	if( subjectCertInfoPtr->flags & CERT_FLAG_SELFSIGNED )
		{
		/* We can't have an empty issuer (== subject) DN */
		return( FALSE );
		}

	/* In addition if it's a CA certificate then the subject DN can't be 
	   empty, for obvious reasons */
	status = getAttributeFieldValue( subjectCertInfoPtr->attributes,
									 CRYPT_CERTINFO_CA, CRYPT_ATTRIBUTE_NONE,
									 &value );
	if( cryptStatusOK( status ) && value > 0 )
		{
		/* It's a CA certificate, the subject DN can't be empty */
		return( FALSE );
		}

	/* Finally, if there's no subject DN present then there has to be an 
	   altName present to take its place */
	attributePtr = findAttributeField( subjectCertInfoPtr->attributes,
									   CRYPT_CERTINFO_SUBJECTALTNAME,
									   CRYPT_ATTRIBUTE_NONE );
	if( attributePtr == NULL )
		{
		/* Either a subject DN or subject altName must be present */
		return( FALSE );
		}

	/* There's a subject altName present but no subject DN, mark the altName 
	   as critical */
	setAttributeProperty( attributePtr, ATTRIBUTE_PROPERTY_CRITICAL, 0 );

	return( TRUE );
	}
#endif /* USE_CERTLEVEL_PKIX_FULL */

/* Perform any final setup actions that add default and issuer-contributed 
   attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int preEncodeCertificate( INOUT CERT_INFO *subjectCertInfoPtr,
						  IN_OPT const CERT_INFO *issuerCertInfoPtr,
						  IN_FLAGS( PRE_SET ) const int actions )
	{
	int status;

	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( issuerCertInfoPtr == NULL ) || \
			isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( actions >= PRE_SET_NONE && \
			  actions <= PRE_SET_FLAG_MAX );
	REQUIRES( ( ( actions & ( PRE_SET_ISSUERATTR | PRE_SET_ISSUERDN | \
							  PRE_SET_VALIDITYPERIOD ) ) && \
				issuerCertInfoPtr != NULL ) || \
			  !( actions & ( PRE_SET_ISSUERATTR | PRE_SET_ISSUERDN | \
							 PRE_SET_VALIDITYPERIOD ) ) );

	/* If it's a >= v3 certificate add the standard X.509v3 extensions if 
	   these aren't already present */
	if( actions & PRE_SET_STANDARDATTR )
		{
		/* Setting the standard attributes requires the presence of a public
		   key to get keyUsage information from, so we have to check this
		   before we can add any attributes.  This would normally be checked 
		   as part of the range of checking performedin 
		   preCheckCertificate(), but that isn't called until the pre-
		   encoding functions here have been performed */
		if( subjectCertInfoPtr->publicKeyInfo == NULL )
			{
			setErrorInfo( subjectCertInfoPtr, 
						  CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}

		/* Attributes are only allowed with version 3 certificates */
		if( subjectCertInfoPtr->version >= 3 )
			{
			status = addStandardExtensions( subjectCertInfoPtr );
			if( cryptStatusError( status ) )
				return( status );
			}
		}

	/* Copy any required extensions from the issuer to the subject 
	   certificate if necessary */
	if( actions & PRE_SET_ISSUERATTR )
		{
		ANALYSER_HINT( issuerCertInfoPtr != NULL );

		if( !( subjectCertInfoPtr->flags & CERT_FLAG_SELFSIGNED ) )
			{
			status = copyIssuerAttributes( &subjectCertInfoPtr->attributes,
										   issuerCertInfoPtr->attributes,
										   subjectCertInfoPtr->type,
										   &subjectCertInfoPtr->errorLocus,
										   &subjectCertInfoPtr->errorType );
			if( cryptStatusError( status ) )
				return( status );
			}
		}

	/* Copy the issuer DN if this isn't already present */
	if( actions & PRE_SET_ISSUERDN )
		{
		ANALYSER_HINT( issuerCertInfoPtr != NULL );

		if( subjectCertInfoPtr->issuerName == NULL )
			{
			status = copyDN( &subjectCertInfoPtr->issuerName,
							 issuerCertInfoPtr->subjectName );
			if( cryptStatusError( status ) )
				return( status );
			}
		}

	/* Constrain the subject validity period to be within the issuer 
	   validity period */
	if( actions & PRE_SET_VALIDITYPERIOD )
		{
		ANALYSER_HINT( issuerCertInfoPtr != NULL );

		if( subjectCertInfoPtr->startTime < issuerCertInfoPtr->startTime )
			subjectCertInfoPtr->startTime = issuerCertInfoPtr->startTime;
		if( subjectCertInfoPtr->endTime > issuerCertInfoPtr->endTime )
			subjectCertInfoPtr->endTime = issuerCertInfoPtr->endTime;
		}

#ifdef USE_CERTVAL
	/* If it's an RTCS response, prepare the certificate status list entries 
	   prior to encoding them */
	if( actions & PRE_SET_VALINFO )
		{
		status = prepareValidityEntries( subjectCertInfoPtr->cCertVal->validityInfo,
										 &subjectCertInfoPtr->cCertVal->currentValidity,
										 &subjectCertInfoPtr->errorLocus,
										 &subjectCertInfoPtr->errorType );
		if( cryptStatusError( status ) )
			return( status );
		}
#endif /* USE_CERTVAL */

#ifdef USE_CERTREV
	/* If it's a CRL or OCSP response, prepare the revocation list entries 
	   prior to encoding them */
	if( actions & PRE_SET_REVINFO )
		{
		REVOCATION_INFO *revocationErrorEntry;
		const BOOLEAN isCrlEntry = ( actions & PRE_SET_ISSUERDN ) ? FALSE : TRUE;

		status = prepareRevocationEntries( subjectCertInfoPtr->cCertRev->revocations,
										   subjectCertInfoPtr->cCertRev->revocationTime,
										   &revocationErrorEntry, isCrlEntry,
										   &subjectCertInfoPtr->errorLocus,
										   &subjectCertInfoPtr->errorType );
		if( cryptStatusError( status ) )
			{
			/* If there was an error and we're processing an entire 
			   revocation list, select the entry that caused the problem */
			if( !isCrlEntry )
				{
				subjectCertInfoPtr->cCertRev->currentRevocation = \
													revocationErrorEntry;
				}
			return( status );
			}
		}
#endif /* USE_CERTREV */

	return( CRYPT_OK );
	}

/* Check that a certificate object is reading for encoding */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int preCheckCertificate( INOUT CERT_INFO *subjectCertInfoPtr,
						 IN_OPT const CERT_INFO *issuerCertInfoPtr,
						 IN_FLAGS( PRE_CHECK ) const int actions, 
						 IN_FLAGS( PRE ) const int flags )
	{
	int status;

	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( issuerCertInfoPtr == NULL ) || \
			isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( actions >= PRE_CHECK_NONE && \
			  actions <= PRE_CHECK_FLAG_MAX );
	REQUIRES( flags == PRE_FLAG_NONE || \
			  flags == PRE_FLAG_DN_IN_ISSUERCERT );
	REQUIRES( ( ( actions & ( PRE_CHECK_ISSUERCERTDN | \
							  PRE_CHECK_NONSELFSIGNED_DN ) ) && \
				issuerCertInfoPtr != NULL ) || \
			  !( actions & ( PRE_CHECK_ISSUERCERTDN | \
							 PRE_CHECK_NONSELFSIGNED_DN ) ) );
			  /* We can't impose a complete set of preconditions on the
			     issuer certificate because some issuer attributes like the 
				 issuer DN may already be present in the subject 
				 certificate */

	/* Make sure that there's public-key information present */
	if( actions & PRE_CHECK_SPKI )
		{
		if( subjectCertInfoPtr->publicKeyInfo == NULL )
			{
			setErrorInfo( subjectCertInfoPtr, 
						  CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}
		}

	/* Make sure that there's a full DN present */
	if( actions & PRE_CHECK_DN )
		{
		status = checkDN( subjectCertInfoPtr->subjectName, 
						  CHECKDN_FLAG_COUNTRY | CHECKDN_FLAG_COMMONNAME,
						  &subjectCertInfoPtr->errorLocus,
						  &subjectCertInfoPtr->errorType );
		if( cryptStatusError( status ) )
			{
#ifdef USE_CERTLEVEL_PKIX_FULL
			/* In some very special cases an empty DN is permitted so we
			   only return an error if this really isn't allowed */
			if( status != CRYPT_ERROR_NOTINITED || \
				!checkEmptyDnOK( subjectCertInfoPtr ) )
#endif /* USE_CERTLEVEL_PKIX_FULL */
				return( status );
			}
		}

	/* Make sure that there's at least a partial DN present (some CA's will 
	   fill the upper portion of the DN themselves so at a minimum all that 
	   we really need is a CommonName) */
	if( actions & PRE_CHECK_DN_PARTIAL )
		{
		status = checkDN( subjectCertInfoPtr->subjectName, 
						  CHECKDN_FLAG_COMMONNAME,
						  &subjectCertInfoPtr->errorLocus,
						  &subjectCertInfoPtr->errorType );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Make sure that there's an issuer DN present */
	if( actions & PRE_CHECK_ISSUERDN )
		{
		if( flags & PRE_FLAG_DN_IN_ISSUERCERT )
			{
			if( issuerCertInfoPtr == NULL || \
				issuerCertInfoPtr->subjectDNptr == NULL || \
				issuerCertInfoPtr->subjectDNsize < 1 )
				{
				setErrorInfo( subjectCertInfoPtr, CRYPT_CERTINFO_ISSUERNAME,
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				return( CRYPT_ERROR_NOTINITED );
				}
			}
		else
			{
			/* The issuer DN can be present either in pre-encoded form (if
			   it was copied from an issuer certificate) or as a full DN (if 
			   it's a self-signed certificate), so we check for the presence 
			   of either */
			if( ( subjectCertInfoPtr->issuerName == NULL ) && 
				( subjectCertInfoPtr->issuerDNptr == NULL || \
				  subjectCertInfoPtr->issuerDNsize < 1 ) )
				{
				setErrorInfo( subjectCertInfoPtr, CRYPT_CERTINFO_ISSUERNAME,
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				return( CRYPT_ERROR_NOTINITED );
				}
			}
		}

	/* If it's a CRL, compare the revoked certificate issuer DN and signer 
	   DN to make sure that we're not trying to revoke someone else's 
	   certificates, and prepare the revocation entries */
	if( actions & PRE_CHECK_ISSUERCERTDN )
		{
		ANALYSER_HINT( issuerCertInfoPtr != NULL );

		if( !compareDN( subjectCertInfoPtr->issuerName,
						issuerCertInfoPtr->subjectName, FALSE, NULL ) )
			{
			setErrorInfo( subjectCertInfoPtr, CRYPT_CERTINFO_ISSUERNAME,
						  CRYPT_ERRTYPE_ATTR_VALUE );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* If we're creating a non-self-signed certificate check whether the 
	   subject's DN is the same as the issuer's DN.  If this is the case 
	   then the resulting object would appear to be self-signed so we 
	   disallow it */
	if( actions & PRE_CHECK_NONSELFSIGNED_DN )
		{
		ANALYSER_HINT( issuerCertInfoPtr != NULL );

		if( compareDN( issuerCertInfoPtr->subjectName,
					   subjectCertInfoPtr->subjectName, FALSE, NULL ) )
			{
			setErrorInfo( subjectCertInfoPtr, CRYPT_CERTINFO_SUBJECTNAME,
						  CRYPT_ERRTYPE_ISSUERCONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* Check that the serial number is present */
	if( actions & PRE_CHECK_SERIALNO )
		{
#ifdef USE_CERTREQ
		if( subjectCertInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION )
			{
			if( subjectCertInfoPtr->cCertReq->serialNumberLength <= 0 )
				{
				setErrorInfo( subjectCertInfoPtr, CRYPT_CERTINFO_SERIALNUMBER,
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				return( CRYPT_ERROR_NOTINITED );
				}
			}
		else
#endif /* USE_CERTREQ */
			{
			if( subjectCertInfoPtr->cCertCert->serialNumberLength <= 0 )
				{
				setErrorInfo( subjectCertInfoPtr, CRYPT_CERTINFO_SERIALNUMBER,
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				return( CRYPT_ERROR_NOTINITED );
				}
			}
		}

	/* Check that the validity/revocation information is present */
#ifdef USE_CERTVAL
	if( actions & PRE_CHECK_VALENTRIES )
		{
		if( subjectCertInfoPtr->cCertVal->validityInfo == NULL )
			{
			setErrorInfo( subjectCertInfoPtr, CRYPT_CERTINFO_CERTIFICATE,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}
		}
#endif /* USE_CERTVAL */
#ifdef USE_CERTREV
	if( actions & PRE_CHECK_REVENTRIES )
		{
		if( subjectCertInfoPtr->cCertRev->revocations == NULL )
			{
			setErrorInfo( subjectCertInfoPtr, CRYPT_CERTINFO_CERTIFICATE,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}
		}
#endif /* USE_CERTREV */

	/* Now that we've set up the attributes, perform the remainder of the
	   checks.  Because RTCS is a CMS standard rather than PKIX the RTCS
	   attributes are CMS rather than certificate attributes */
	if( subjectCertInfoPtr->attributes != NULL )
		{
		status = checkAttributes( ( subjectCertInfoPtr->type == \
									CRYPT_CERTTYPE_RTCS_REQUEST ) ? \
								  ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE,
								  subjectCertInfoPtr->attributes,
								  &subjectCertInfoPtr->errorLocus,
								  &subjectCertInfoPtr->errorType );
		if( cryptStatusError( status ) )
			return( status );
		}
	status = checkCert( subjectCertInfoPtr, issuerCertInfoPtr, FALSE,
						&subjectCertInfoPtr->errorLocus,
						&subjectCertInfoPtr->errorType );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a certificate or certificate chain remember that it's been 
	   checked at full compliance level (or at least as full as we're
	   configured for).  This short-circuits the need to perform excessive 
	   levels of checking if the caller wants to re-check it after it's 
	   been signed */
	if( subjectCertInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
		subjectCertInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
		{
		subjectCertInfoPtr->cCertCert->maxCheckLevel = \
									CRYPT_COMPLIANCELEVEL_PKIX_FULL;
		}

	return( status );
	}
#endif /* USE_CERTIFICATES */
