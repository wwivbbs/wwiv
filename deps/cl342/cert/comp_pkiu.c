/****************************************************************************
*																			*
*							Set PKI User Components							*
*						Copyright Peter Gutmann 1997-2012					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
#endif /* Compiler-specific includes */

#if defined( USE_CERTIFICATES ) && defined( USE_PKIUSER )

/****************************************************************************
*																			*
*							PKI User Attribute Management					*
*																			*
****************************************************************************/

/* Perform any special-case adjustments on attributes that may be required 
   after they've been copied from the PKI user to the certificate request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int adjustPkiUserAttributes( INOUT CERT_INFO *certInfoPtr )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* The SCEP protocol requires that the SCEP challenge password be placed 
	   in the PKCS #10 request instead of being included in the SCEP 
	   metadata.  This shouldn't have been copied across to the certificate 
	   request since the NOCOPY flag was set for the attribute, but to make 
	   absolutely certain we try and delete it anyway.  Since this could in 
	   theory be present in non-SCEP requests as well (the request-
	   processing code just knows about PKCS #10 requests as a whole, not 
	   requests from a SCEP source vs. requests not from a SCEP source), we 
	   make the delete unconditional */
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

/* Populate the certificate request with additional attributes from the PKI 
   user information.  The attributes that can be specified in requests are 
   severely limited (see the long comment for sanitiseCertAttributes() in 
   comp_cert.c) so the only ones that we really need to handle are the 
   altName and a special-case keyUsage check for CA key usages */

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

		/* The two altNames are identical, delete the one in the request in 
		   order to allow the one from the PKI user data to be copied across 
		   when we call copyAttributes() */
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

	/* Perform any special-case adjustments on attributes that may be 
	   required after they've been copied from the PKI user to the 
	   certificate request */
	return( adjustPkiUserAttributes( certInfoPtr ) );
	}

/****************************************************************************
*																			*
*								PKI User DN Management						*
*																			*
****************************************************************************/

/* Assemble the certificate DN from information in the request and the PKI 
   user data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int assemblePkiUserDN( INOUT CERT_INFO *certInfoPtr,
							  const DN_PTR *pkiUserSubjectName,
							  IN_BUFFER( commonNameLength ) const void *commonName, 
							  IN_LENGTH_SHORT const int commonNameLength,
							  const BOOLEAN replaceCN )
	{
	STREAM stream;
	void *tempDN = NULL, *tempDNdata;
	int tempDNsize DUMMY_INIT, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( commonName, commonNameLength ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
			  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );
	REQUIRES( pkiUserSubjectName != NULL );
	REQUIRES( commonNameLength > 0 && \
			  commonNameLength < MAX_INTLENGTH_SHORT );

	/* Copy the DN template, delete the original CN if necessary (required 
	   if the template is from an RA, for which the user being authorised 
	   has a different CN), append the user-supplied CN, and allocate room 
	   for the encoded form */
	status = copyDN( &tempDN, pkiUserSubjectName );
	if( cryptStatusError( status ) )
		return( status );
	if( replaceCN )
		{
		/* We're replacing the CN with a new one, delete the current one */
		( void ) deleteDNComponent( &tempDN, CRYPT_CERTINFO_COMMONNAME, 
									NULL, 0 );
		}
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
	   the request was read from its encoded form, with the subject DN 
	   pointer pointing into the encoded request data (if the request was 
	   created from scratch then there's no DN present).  We replace the 
	   (possible) existing pointer into the certificate data with a pointer 
	   to the updated encoded DN written to our newly-allocated block of 
	   memory */
	deleteDN( &certInfoPtr->subjectName );
	certInfoPtr->subjectName = tempDN;
	sMemOpen( &stream, tempDNdata, tempDNsize );
	status = writeDN( &stream, tempDN, DEFAULT_TAG );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );
	certInfoPtr->subjectDNdata = certInfoPtr->subjectDNptr = tempDNdata;
	certInfoPtr->subjectDNsize = tempDNsize;

	return( CRYPT_OK );
	}

/* Process the DN in a certificate request, reconciling it with the DN 
   information from the PKI user data.  This is a complex operation, see the 
   comments in the code below for more */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processReqDN( INOUT CERT_INFO *certInfoPtr,
						 const DN_PTR *pkiUserSubjectName,
						 IN_BUFFER( pkiUserDNsize ) const void *pkiUserDNptr, 
						 IN_LENGTH_SHORT const int pkiUserDNsize,
						 const BOOLEAN requestFromRA )
	{
	CRYPT_ATTRIBUTE_TYPE dnComponentType;
	DN_PTR *requestDNSubset, *pkiUserDNSubset;
	BOOLEAN replaceCN = FALSE, dnContinues;
	int dummy, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( pkiUserSubjectName, sizeof( DN_PTR * ) ) );
	assert( isReadPtr( pkiUserDNptr, sizeof( pkiUserDNsize ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
			  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );
	REQUIRES( pkiUserDNsize > 0 && pkiUserDNsize < MAX_INTLENGTH_SHORT );

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
	if( certInfoPtr->subjectName == NULL && pkiUserSubjectName == NULL )
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
		( pkiUserSubjectName == NULL || \
		  cryptStatusError( \
				getDNComponentValue( pkiUserSubjectName, 
									 CRYPT_CERTINFO_COMMONNAME, 0, NULL, 0, 
									 &dummy ) ) ) )
		{
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_COMMONNAME,
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If there's no DN present in the request then one has been supplied by 
	   the CA in the PKI user information, copy over the DN and its encoded 
	   form from the user information.  
	   
	   Note that this case also applies for when the PKI user is an RA and
	   they're obtaining their own certificate that's then used to authorise 
	   further certificate issuance */
	if( certInfoPtr->subjectName == NULL )
		{
		REQUIRES( pkiUserDNptr != NULL );

		status = copyDN( &certInfoPtr->subjectName, pkiUserSubjectName );
		if( cryptStatusError( status ) )
			return( status );
		if( ( certInfoPtr->subjectDNdata = \
				clAlloc( "processReqDN", pkiUserDNsize ) ) == NULL )
			{
			deleteDN( &certInfoPtr->subjectName );
			return( CRYPT_ERROR_MEMORY );
			}
		memcpy( certInfoPtr->subjectDNdata, pkiUserDNptr, pkiUserDNsize );
		certInfoPtr->subjectDNptr = certInfoPtr->subjectDNdata;
		certInfoPtr->subjectDNsize = pkiUserDNsize;

		return( CRYPT_OK );
		}

	/* If there's no PKI user DN with the potential to conflict with the one
	   in the request present, we're done */
	if( pkiUserSubjectName == NULL )
		return( CRYPT_OK );

	/* If there are full DNs present in both objects and they match then 
	   there's nothing else to do */
	if( compareDN( certInfoPtr->subjectName, pkiUserSubjectName, \
				   FALSE, NULL ) )
		return( CRYPT_OK );

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

	   If this is an RA user then things work slightly differently.  In 
	   order to avoid an RA being able to approve the issuance of 
	   certificates for any identity, we require that all portions of the DN 
	   except the CN match the RA's DN, meaning that the request has to have 
	   a full DN that matches (apart from the CN) or contains only a CN, 
	   with the rest filled in from the RA's DN.

	   The set of DNs with a prefix-match-result of TRUE (in other words the
	   request contains a subset of the PKI user's DN) are trivially 
	   invalid, so we can reject them immediately */
	if( compareDN( certInfoPtr->subjectName, pkiUserSubjectName, TRUE, 
				   &requestDNSubset ) )
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
	if( !compareDN( pkiUserSubjectName, certInfoPtr->subjectName, TRUE, 
					&pkiUserDNSubset ) )
		{
		ANALYSER_HINT( pkiUserDNSubset != NULL );
		status = getDNComponentInfo( pkiUserDNSubset, &dnComponentType, 
									 &dnContinues );
		if( cryptStatusError( status ) )
			{
			/* There's some sort of nonspecific error, treat it as a problem 
			   with a missing/mismatched CN */
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_COMMONNAME,
						  CRYPT_ERRTYPE_ISSUERCONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		if( dnComponentType == CRYPT_CERTINFO_COMMONNAME )
			{
			/* There is one situation in which a mismatched CN is valid and 
			   that's where the PKI user is an RA, in which case the CN is
			   for the user being authorised by the RA rather than the RA 
			   user themselves.  In this case we replace any possibly-
			   present RA CN with the one in the request */
			if( dnContinues || !requestFromRA )
				{
				setErrorInfo( certInfoPtr, CRYPT_CERTINFO_COMMONNAME,
							  CRYPT_ERRTYPE_ISSUERCONSTRAINT );
				return( CRYPT_ERROR_INVALID );
				}
			replaceCN = TRUE;
			}
		}

	/* If the request DN consists only of a CN, replace the request DN with 
	   the merged DN prefix from the PKI user DN and the CN from the request 
	   DN */
	if( certInfoPtr->subjectName == requestDNSubset || replaceCN )
		{
		char commonName[ CRYPT_MAX_TEXTSIZE + 8 ];
		int commonNameLength;

		status = getDNComponentValue( requestDNSubset, 
									  CRYPT_CERTINFO_COMMONNAME, 0, 
									  commonName, CRYPT_MAX_TEXTSIZE, 
									  &commonNameLength );
		if( cryptStatusOK( status ) )
		return( assemblePkiUserDN( certInfoPtr, pkiUserSubjectName,
								   commonName, commonNameLength, 
								   replaceCN ) );
		}

	/* The request DN contains a full DN including a CN that matches the 
	   full PKI user DN and there's nothing to do beyond the checks that 
	   we've already performed above */
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Copy PKI User Data								*
*																			*
****************************************************************************/

/* Set or modify data in a certificate request based on the PKI user 
   information.  This is rather more complicated than the standard copy
   operations because we potentially have to merge information already
   present in the request with information in the PKI user object, checking
   for consistency/conflicts in the process */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyPkiUserToCertReq( INOUT CERT_INFO *certInfoPtr,
						  INOUT CERT_INFO *pkiUserInfoPtr )
	{
	BOOLEAN requestFromRA = FALSE;
	int value, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( pkiUserInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
			  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );
	REQUIRES( pkiUserInfoPtr->type == CRYPT_CERTTYPE_PKIUSER );
	REQUIRES( pkiUserInfoPtr->certificate != NULL );

	/* If the request came via an RA then this changes the DN constraint 
	   handling.  Only CRMF requests support RA functionality so there's no 
	   check for standard PKSC #10 requests */
	if( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT )
		{
		status = getCertComponent( certInfoPtr, CRYPT_IATTRIBUTE_REQFROMRA, 
								   &value );
		if( cryptStatusOK( status ) && value == TRUE )
			requestFromRA = TRUE;
		}

	/* Process the DN in the request and make sure that it's consistent with 
	   the information for the PKI user */
	status = processReqDN( certInfoPtr, pkiUserInfoPtr->subjectName,
						   pkiUserInfoPtr->subjectDNptr, 
						   pkiUserInfoPtr->subjectDNsize, requestFromRA );
	if( cryptStatusError( status ) )
		return( status );

	/* Copy any additional attributes across */
	return( copyPkiUserAttributes( certInfoPtr, 
								   pkiUserInfoPtr->attributes ) );
	}
#endif /* USE_CERTIFICATES && USE_PKIUSER */
