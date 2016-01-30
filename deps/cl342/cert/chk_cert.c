/****************************************************************************
*																			*
*						  Certificate Checking Routines						*
*						Copyright Peter Gutmann 1997-2010					*
*																			*
****************************************************************************/

#include <ctype.h>
#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

#ifdef USE_CERTLEVEL_PKIX_PARTIAL

/* Check whether disallowed CA-only attributes are present in a (non-CA) 
   attribute list.  We report the error as a constraint derived from the CA
   flag rather than the attribute itself since it's the absence of the flag 
   that renders the presence of the attribute invalid */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static BOOLEAN invalidAttributesPresent( const ATTRIBUTE_PTR *attributePtr,
										 const BOOLEAN isIssuer,
										 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
											CRYPT_ATTRIBUTE_TYPE *errorLocus, 
										 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
											CRYPT_ERRTYPE_TYPE *errorType )
	{
	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Check for entire disallowed attributes */
#ifdef USE_CERTLEVEL_PKIX_FULL
	if( checkAttributePresent( attributePtr, \
							   CRYPT_CERTINFO_NAMECONSTRAINTS ) || \
		checkAttributePresent( attributePtr, \
							   CRYPT_CERTINFO_POLICYCONSTRAINTS ) || \
		checkAttributePresent( attributePtr, \
							   CRYPT_CERTINFO_INHIBITANYPOLICY ) || \
		checkAttributePresent( attributePtr, \
							   CRYPT_CERTINFO_POLICYMAPPINGS ) )
		{
		setErrorValues( CRYPT_CERTINFO_CA, isIssuer ? \
							CRYPT_ERRTYPE_ISSUERCONSTRAINT : \
							CRYPT_ERRTYPE_CONSTRAINT );
		return( TRUE );
		}
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* Check for a particular field of an attribute that's invalid rather 
	   than the entire attribute (the specific exclusion of path-length 
	   constraints in basicConstraints was introduced in RFC 3280) */
	if( checkAttributeFieldPresent( attributePtr, \
									CRYPT_CERTINFO_PATHLENCONSTRAINT ) )
		{
		setErrorValues( CRYPT_CERTINFO_CA, isIssuer ? \
							CRYPT_ERRTYPE_ISSUERCONSTRAINT : \
							CRYPT_ERRTYPE_CONSTRAINT );
		return( TRUE );
		}

	return( FALSE );
	}
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

#ifdef USE_CERTLEVEL_PKIX_FULL

/* Check whether a certificate is a PKIX path-kludge certificate, which 
   allows extra certificates to be kludged into the path without violating 
   any constraints */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isPathKludge( const CERT_INFO *certInfoPtr )
	{
	int value, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* Perform a quick-reject check for certificates that haven't been 
	   identified by the certificate chain processing code as path-kludge 
	   certificates */
	if( !( certInfoPtr->flags & CERT_FLAG_PATHKLUDGE ) )
		return( FALSE );

	/* Only CA path-kludge certificates are exempt from constraint 
	   enforcement.  Non-CA path kludges shouldn't ever occur but who knows 
	   what other weirdness future RFCs will dream up, so we perform an 
	   explicit check here */
	status = getAttributeFieldValue( certInfoPtr->attributes, 
									 CRYPT_CERTINFO_CA, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	return( ( cryptStatusOK( status ) && value ) ? TRUE : FALSE );
	}
#endif /* USE_CERTLEVEL_PKIX_FULL */

/****************************************************************************
*																			*
*								Check Name Constraints						*
*																			*
****************************************************************************/

#ifdef USE_CERTLEVEL_PKIX_FULL

/* Perform a wildcarded compare of two strings in attributes.  Certificates
   don't use standard ? and * regular-expression wildcards but instead 
   specify the constraint as a form of longest-suffix filter that's applied 
   to the string (with the usual pile of special-case exceptions that apply 
   to any certificate-related rules) so that e.g. www.foo.com would be 
   constrained using foo.com (or more usually .foo.com to avoid erroneous 
   matches for strings like www.barfoo.com) */

typedef enum {
	MATCH_NONE,		/* No special-case matching rules */
	MATCH_EMAIL,	/* Match using email address mailbox exception */
	MATCH_URI,		/* Match only DNS name portion of URI */
	MATCH_LAST		/* Last valid match rule type */
	} MATCH_TYPE;

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN wildcardMatch( const ATTRIBUTE_PTR *constrainedAttribute,
							  const ATTRIBUTE_PTR *constrainingAttribute,
							  IN_ENUM_OPT( MATCH ) const MATCH_TYPE matchType )
	{
	const BYTE *constrainingString, *constrainedString;
	int constrainingStringLength, constrainedStringLength;
	BOOLEAN isWildcardMatch;
	int startPos, status;

	assert( isReadPtr( constrainedAttribute, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isReadPtr( constrainingAttribute, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );

	REQUIRES_B( matchType >= MATCH_NONE && matchType < MATCH_LAST );

	status = getAttributeDataPtr( constrainingAttribute, 
								  ( void ** ) &constrainingString, 
								  &constrainingStringLength );
	if( cryptStatusError( status ) )
		return( FALSE );
	status = getAttributeDataPtr( constrainedAttribute, 
								  ( void ** ) &constrainedString, 
								  &constrainedStringLength );
	if( cryptStatusError( status ) )
		return( FALSE );
	isWildcardMatch = ( *constrainingString == '.' ) ? TRUE : FALSE;

	/* Determine the start position of the constraining string within the
	   constrained string: 

		xxxxxyyyyy	- Constrained string
			 yyyyy	- Constraining string
			^
			|
		startPos
	   
	   If the constraining string is longer than the constrained string 
	   (making startPos negative), it can never match */
	startPos = constrainedStringLength - constrainingStringLength;
	if( startPos < 0 || startPos > MAX_INTLENGTH_SHORT )
		return( FALSE );

	/* Handle special-case match requirements (PKIX section 4.2.1.11) */
	switch( matchType )
		{
		case MATCH_EMAIL:
			/* Email addresses have a special-case requirement where the 
			   absence of a wildcard-match indicator (the leading dot)
			   indicates that the mailbox has to be located directly on the 
			   constraining hostname rather than merely within that domain, 
			   i.e. user@foo.bar.com is a valid match for .bar.com but not 
			   for bar.com, which would require user@bar.com to match */
			ENSURES_B( startPos <= constrainedStringLength );
			if( !isWildcardMatch && \
				( startPos < 1 || constrainedString[ startPos - 1 ] != '@' ) )
				return( FALSE );
			break;

		case MATCH_URI:
			{
			URL_INFO urlInfo;

			/* URIs can contain trailing location information that isn't 
			   regarded as part of the URI for matching purposes so before 
			   performing the match we have to parse the URL and only use 
			   the DNS name portion */
			status = sNetParseURL( &urlInfo, constrainedString, 
								   constrainedStringLength, URL_TYPE_NONE );
			if( cryptStatusError( status ) )
				{
				/* Exactly what to do in the case of a URL parse error is a
				   bit complicated.  The standard action is to fail closed, 
				   otherwise anyone who creates a URL that the certificate 
				   software can't parse but that's still accepted by other 
				   apps (who in general will bend over backwards to try and 
				   accept almost any malformed URI, if they didn't do this 
				   then half the Internet would stop working) would be able 
				   to bypass the name constraint.  However this mode of 
				   handling is complicated by the fact that to report a 
				   failure at this point we need to report a match for 
				   excluded subtrees but a non-match for permitted subtrees.  
				   Since it's more likely that we'll encounter a permitted-
				   subtrees whitelist we report the constraint as being not 
				   matched which will reject the certificate for permitted-
				   subtrees (who in their right mind would trust something as 
				   flaky as PKI software to reliably apply an excluded-
				   subtrees blacklist?  Even something as trivial as 
				   "ex%41mple.com", let alone "ex%u0041mple.com", 
				   "ex&#x41;mple.com", or "ex%EF%BC%A1mpple.com", is likely 
				   to trivially fool all certificate software in existence, 
				   so permitted-subtrees will never work anyway).  In 
				   addition we throw an exception in debug mode */
				assert( DEBUG_WARN );
				return( FALSE );
				}

			/* Adjust the constrained string information to contain only the 
			   DNS name portion of the URI */
			constrainedString = urlInfo.host;
			startPos = urlInfo.hostLen - constrainingStringLength;
			if( startPos < 0 || startPos > MAX_INTLENGTH_SHORT )
				return( FALSE );
			ENSURES_B( rangeCheckZ( startPos, constrainingStringLength, \
									urlInfo.hostLen ) );

			/* URIs have a special-case requirement where the absence of a
			   wildcard-match indicator (the leading dot) indicates that the
			   constraining DNS name is for a standalone host and not a 
			   portion of the constrained string's DNS name.  This means
			   that the DNS-name portion of the URI must be an exact match
			   for the constraining string */
			if( !isWildcardMatch && startPos != 0 )
				return( FALSE );
			}
		}
	ENSURES_B( rangeCheckZ( startPos, constrainingStringLength, \
							constrainedStringLength ) );

	/* Check whether the constraining string is a suffix of the constrained
	   string.  For DNS name constraints the rule for RFC 3280 became 
	   "adding to the LHS" as for other constraints, in RFC 2459 it was
	   another special case where it had to be a subdomain as if an 
	   implicit "." was present */
	return( !strCompare( constrainedString + startPos, constrainingString, 
						 constrainingStringLength ) ? TRUE : FALSE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN matchAltnameComponent( const ATTRIBUTE_PTR *constrainedAttribute,
									  const ATTRIBUTE_PTR *constrainingAttribute,
									  IN_ATTRIBUTE \
										const CRYPT_ATTRIBUTE_TYPE attributeType )
	{
	assert( isReadPtr( constrainedAttribute, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isReadPtr( constrainingAttribute, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );

	REQUIRES( attributeType == CRYPT_CERTINFO_DIRECTORYNAME || \
			  attributeType == CRYPT_CERTINFO_RFC822NAME || \
			  attributeType == CRYPT_CERTINFO_DNSNAME || \
			  attributeType == CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER );

	/* If the attribute being matched is a DN, use a DN-specific match */
	if( attributeType == CRYPT_CERTINFO_DIRECTORYNAME )
		{
		void **constrainedDnPtr, **constrainingDnPtr;
		int status;

		status = getAttributeDataDN( constrainedAttribute, 
									 &constrainedDnPtr );
		if( cryptStatusError( status ) )
			return( FALSE );
		status = getAttributeDataDN( constrainingAttribute, &constrainingDnPtr );
		if( cryptStatusError( status ) )
			return( FALSE );
		return( compareDN( *constrainingDnPtr, *constrainedDnPtr, TRUE, 
						   NULL ) );
		}

	/* It's a string name, use a substring match with attribute type-specific
	   special cases */
	return( wildcardMatch( constrainedAttribute, constrainingAttribute, 
					( attributeType == CRYPT_CERTINFO_RFC822NAME ) ? \
						MATCH_EMAIL : \
					( attributeType == CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER ) ? \
						MATCH_URI : \
						MATCH_NONE ) );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN checkAltnameConstraints( const ATTRIBUTE_PTR *subjectAttributes,
										const ATTRIBUTE_PTR *issuerAttributes,
										IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE attributeType,
										const BOOLEAN isExcluded )
	{
	const ATTRIBUTE_PTR *attributePtr, *constrainedAttributePtr;
	int iterationCount;

	assert( isReadPtr( subjectAttributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isReadPtr( issuerAttributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );

	REQUIRES( attributeType == CRYPT_CERTINFO_DIRECTORYNAME || \
			  attributeType == CRYPT_CERTINFO_RFC822NAME || \
			  attributeType == CRYPT_CERTINFO_DNSNAME || \
			  attributeType == CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER );

	/* Check for the presence of constrained or constraining altName 
	   components.  If either are absent, there are no constraints to 
	   apply */
	attributePtr = findAttributeField( issuerAttributes,
									   isExcluded ? \
										CRYPT_CERTINFO_EXCLUDEDSUBTREES : \
										CRYPT_CERTINFO_PERMITTEDSUBTREES,
									   attributeType );
	if( attributePtr == NULL )
		return( TRUE );

	for( constrainedAttributePtr = \
			findAttributeField( subjectAttributes, 
								CRYPT_CERTINFO_SUBJECTALTNAME, attributeType ), \
			iterationCount = 0; 
		constrainedAttributePtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		constrainedAttributePtr = \
			findNextFieldInstance( constrainedAttributePtr ), \
			iterationCount++ )
		{
		const ATTRIBUTE_PTR *attributeCursor;
		int innerIterationCount;
		BOOLEAN isMatch = FALSE;

		/* Step through the constraining attributes checking if any match 
		   the constrained attribute.  If it's an excluded subtree then none 
		   can match, if it's a permitted subtree then at least one must 
		   match */
		for( attributeCursor = attributePtr, \
				innerIterationCount = 0;
			 attributeCursor != NULL && !isMatch && \
				innerIterationCount < FAILSAFE_ITERATIONS_LARGE;
			 attributeCursor = 
				findNextFieldInstance( attributeCursor ), \
				innerIterationCount++ )
			{
			isMatch = matchAltnameComponent( constrainedAttributePtr,
											 attributeCursor,
											 attributeType );
			}
		ENSURES_B( innerIterationCount < FAILSAFE_ITERATIONS_LARGE );
		if( isExcluded == isMatch )
			return( FALSE );
		}
	ENSURES_B( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( TRUE );
	}

/* Check name constraints placed by an issuer, checked if complianceLevel >=
   CRYPT_COMPLIANCELEVEL_PKIX_FULL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int checkNameConstraints( const CERT_INFO *subjectCertInfoPtr,
						  const ATTRIBUTE_PTR *issuerAttributes,
						  const BOOLEAN isExcluded,
						  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
						  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType )
	{
	const ATTRIBUTE_PTR *subjectAttributes = subjectCertInfoPtr->attributes;
	const CRYPT_ATTRIBUTE_TYPE constraintType = isExcluded ? \
		CRYPT_CERTINFO_EXCLUDEDSUBTREES : CRYPT_CERTINFO_PERMITTEDSUBTREES;
	ATTRIBUTE_PTR *attributePtr;
	BOOLEAN isMatch = FALSE;

	assert( isReadPtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( issuerAttributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* If this is a PKIX path-kludge CA certificate then the name 
	   constraints don't apply to it (PKIX section 4.2.1.11).  This is 
	   required in order to allow extra certificates to be kludged into the 
	   path without violating the constraint.  For example with the chain:

		Issuer	Subject		Constraint
		------	-------		----------
		Root	CA			permitted = "EE"
		CA'		CA'
		CA		EE

	   the kludge certificate CA' must be excluded from name constraint 
	   restrictions in order for the path to be valid.  Obviously this is 
	   only necessary for constraints set by the immediate parent but PKIX 
	   says it's for constraints set by all certificates in the chain (!!), 
	   thus making the pathkludge certificate exempt from any name 
	   constraints and not just the one that would cause problems */
	if( isPathKludge( subjectCertInfoPtr ) )
		return( CRYPT_OK );

	/* Check the subject DN if constraints exist.  If it's an excluded 
	   subtree then none can match, if it's a permitted subtree then at 
	   least one must match.  We also check for the special case of an
	   empty subject DN, which acts as a wildcard that matches/doesn't
	   match permitted/excluded as required */
	attributePtr = findAttributeField( issuerAttributes, constraintType, 
									   CRYPT_CERTINFO_DIRECTORYNAME );
	if( attributePtr != NULL && subjectCertInfoPtr->subjectName != NULL )
		{
		int iterationCount;

		for( iterationCount = 0;
			 attributePtr != NULL && !isMatch && \
				iterationCount < FAILSAFE_ITERATIONS_LARGE;
			 iterationCount++ )
			{
			DN_PTR **dnPtrPtr;
			int status;

			status = getAttributeDataDN( attributePtr, &dnPtrPtr );
			if( cryptStatusOK( status ) )
				{
				/* Check whether the constraining DN is a substring of the 
				   subject DN.  For example if the constraining DN is 
				   C=US/O=Foo/OU=Bar and the subject DN is 
				   C=US/O=Foo/OU=Bar/CN=Baz then compareDN() will return 
				   TRUE to indicate that it's a substring */
				isMatch = compareDN( *dnPtrPtr, 
									 subjectCertInfoPtr->subjectName, TRUE, 
									 NULL );
				}
			attributePtr = findNextFieldInstance( attributePtr );
			}
		ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
		if( isExcluded == isMatch )
			{
			setErrorValues( CRYPT_CERTINFO_SUBJECTNAME, 
							CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* DN constraints apply to both the main subject DN and any other DNs 
	   that may be present as subject altNames, so after we've checked the 
	   main DN we check any altName DNs as well */
	if( !checkAltnameConstraints( subjectAttributes, issuerAttributes,
								  CRYPT_CERTINFO_DIRECTORYNAME, isExcluded ) )
		{
		setErrorValues( CRYPT_CERTINFO_SUBJECTALTNAME, 
						CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* Compare the Internet-related names if constraints exist.  We don't
	   have to check for the special case of an email address in the DN 
	   since the certificate import code transparently maps this to the 
	   appropriate altName component */
	if( !checkAltnameConstraints( subjectAttributes, issuerAttributes,
								  CRYPT_CERTINFO_RFC822NAME, isExcluded ) || \
		!checkAltnameConstraints( subjectAttributes, issuerAttributes,
								  CRYPT_CERTINFO_DNSNAME, isExcluded ) || \
		!checkAltnameConstraints( subjectAttributes, issuerAttributes,
								  CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, 
								  isExcluded ) )
		{
		setErrorValues( CRYPT_CERTINFO_SUBJECTALTNAME, 
						CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	return( CRYPT_OK );
	}
#endif /* USE_CERTLEVEL_PKIX_FULL */

/****************************************************************************
*																			*
*							Check Policy Constraints						*
*																			*
****************************************************************************/

#ifdef USE_CERTLEVEL_PKIX_FULL

/* Check whether a policy is the wildcard anyPolicy */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN isAnyPolicy( const ATTRIBUTE_PTR *attributePtr )
	{
	void *policyOidPtr;
	int policyOidLength, status;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );

	status = getAttributeDataPtr( attributePtr, &policyOidPtr, 
								  &policyOidLength );
	if( cryptStatusError( status ) )
		return( FALSE );
	return( ( policyOidLength == sizeofOID( OID_ANYPOLICY ) && \
			  !memcmp( policyOidPtr, OID_ANYPOLICY, 
					   sizeofOID( OID_ANYPOLICY ) ) ) ? TRUE : FALSE );
	}

/* Check whether a set of policies contains an instance of the anyPolicy
   wildcard */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN containsAnyPolicy( const ATTRIBUTE_PTR *attributePtr,
								  IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE attributeType )
	{
	int iterationCount;

	assert( isReadPtr( attributePtr, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );

	REQUIRES_B( attributeType >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				attributeType <= CRYPT_CERTINFO_LAST );
	
	for( attributePtr = findAttributeField( attributePtr, \
									attributeType, CRYPT_ATTRIBUTE_NONE ), \
			iterationCount = 0; 
		 attributePtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 attributePtr = findNextFieldInstance( attributePtr ), \
			iterationCount++ )
		{
		if( isAnyPolicy( attributePtr ) )
			return( TRUE );
		}
	ENSURES_B( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( FALSE );
	}

/* Check the type of policy present in a certificate and make sure that it's 
   valid */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 2, 3 ) ) \
static BOOLEAN checkPolicyType( IN_OPT const ATTRIBUTE_PTR *attributePtr,
								OUT_BOOL BOOLEAN *hasPolicy, 
								OUT_BOOL BOOLEAN *hasAnyPolicy,
								const BOOLEAN inhibitAnyPolicy )
	{
	int iterationCount;

	assert( attributePtr == NULL || \
			isReadPtr( attributePtr, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isWritePtr( hasPolicy, sizeof( BOOLEAN ) ) );
	assert( isWritePtr( hasAnyPolicy, sizeof( BOOLEAN ) ) );

	/* Clear return values */
	*hasPolicy = *hasAnyPolicy = FALSE;

	/* Make sure that there's a policy present and that it's a specific 
	   policy if an explicit policy is required (the ability to disallow the 
	   wildcard policy via inhibitAnyPolicy was introduced in RFC 3280 along 
	   with the introduction of anyPolicy) */
	if( attributePtr == NULL )
		return( FALSE );
	for( iterationCount = 0; 
		 attributePtr != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 attributePtr = findNextFieldInstance( attributePtr ), iterationCount++ )
		{
		if( isAnyPolicy( attributePtr ) )
			*hasAnyPolicy = TRUE;
		else
			*hasPolicy = TRUE;
		}
	ENSURES_B( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	if( inhibitAnyPolicy )
		{
		/* The wildcard anyPolicy isn't valid for the subject, if there's no
		   other policy set then this is an error, otherwise we continue 
		   without the wildcard match allowed */
		if( !*hasPolicy )
			return( FALSE );
		*hasAnyPolicy = FALSE;
		}

	return( TRUE );
	}

/* Check whether a given policy is present in a certificate */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN isPolicyPresent( const ATTRIBUTE_PTR *subjectAttributes,
						 IN_BUFFER( issuerPolicyValueLength ) \
								const void *issuerPolicyValue,
						 IN_LENGTH_OID const int issuerPolicyValueLength )
	{
	const ATTRIBUTE_PTR *attributeCursor;
	int iterationCount, status;

	assert( isReadPtr( subjectAttributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isReadPtr( issuerPolicyValue, issuerPolicyValueLength ) );

	REQUIRES_B( issuerPolicyValueLength > 0 && \
				issuerPolicyValueLength < MAX_POLICY_SIZE );

	for( attributeCursor = subjectAttributes, iterationCount = 0;
		 attributeCursor != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 attributeCursor = findNextFieldInstance( attributeCursor ), \
			iterationCount++ )
		{
		void *subjectPolicyValuePtr;
		int subjectPolicyValueLength;

		status = getAttributeDataPtr( attributeCursor, &subjectPolicyValuePtr, 
									  &subjectPolicyValueLength );
		if( cryptStatusError( status ) )
			continue;
		if( issuerPolicyValueLength == subjectPolicyValueLength && \
			!memcmp( issuerPolicyValue, subjectPolicyValuePtr, 
					 issuerPolicyValueLength ) )
			return( TRUE );
			}
	ENSURES_B( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( FALSE );
	}

/* Check policy constraints placed by an issuer, checked if complianceLevel 
   >= CRYPT_COMPLIANCELEVEL_PKIX_FULL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 7, 8 ) ) \
int checkPolicyConstraints( const CERT_INFO *subjectCertInfoPtr,
							const ATTRIBUTE_PTR *issuerAttributes,
							IN_ENUM_OPT( POLICY ) const POLICY_TYPE policyType,
							IN_OPT const POLICY_INFO *policyInfo,
							IN_RANGE( 0, MAX_CHAINLENGTH ) const int policyLevel,
							const BOOLEAN allowMappedPolicies,
							OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType )
	{
	const ATTRIBUTE_PTR *constrainingAttributePtr = \
					findAttributeField( issuerAttributes, 
										CRYPT_CERTINFO_CERTPOLICYID, 
										CRYPT_ATTRIBUTE_NONE );
	const ATTRIBUTE_PTR *constrainedAttributePtr = \
					findAttributeField( subjectCertInfoPtr->attributes, 
										CRYPT_CERTINFO_CERTPOLICYID, 
										CRYPT_ATTRIBUTE_NONE );
	BOOLEAN subjectHasPolicy, issuerHasPolicy;
	BOOLEAN subjectHasAnyPolicy, issuerHasAnyPolicy;
	int iterationCount;

	assert( isReadPtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( issuerAttributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( ( policyInfo == NULL && policyLevel == 0 ) || \
			( isReadPtr( policyInfo, sizeof( POLICY_INFO ) ) && \
			  ( policyLevel >= 0 && policyLevel < MAX_CHAINLENGTH ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( policyType >= POLICY_NONE && policyType < POLICY_LAST );
	REQUIRES( policyLevel >= 0 && policyLevel < MAX_CHAINLENGTH );

	/* If there's a policy mapping present then neither the issuer nor 
	   subject domain policies can be the wildcard anyPolicy (PKIX section 
	   4.2.1.6) */
	if( containsAnyPolicy( issuerAttributes, 
						   CRYPT_CERTINFO_ISSUERDOMAINPOLICY ) || \
		containsAnyPolicy( issuerAttributes, 
						   CRYPT_CERTINFO_SUBJECTDOMAINPOLICY ) )
		{
		setErrorValues( CRYPT_CERTINFO_POLICYMAPPINGS, 
						CRYPT_ERRTYPE_ISSUERCONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If there's no requirement for a policy and there's none set, we're 
	   done */
	if( policyType == POLICY_NONE && constrainedAttributePtr == NULL )
		return( CRYPT_OK );

	/* Check the subject policy */
	if( !checkPolicyType( constrainedAttributePtr, &subjectHasPolicy,
						  &subjectHasAnyPolicy, 
						  ( policyType == POLICY_NONE_SPECIFIC || \
							policyType == POLICY_SUBJECT_SPECIFIC || \
							policyType == POLICY_BOTH_SPECIFIC ) ? \
							TRUE : FALSE ) )
		{
		setErrorValues( CRYPT_CERTINFO_CERTPOLICYID, 
						CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If there's no requirement for an issuer policy and there's none set 
	   by the issuer, we're done */
	if( ( ( policyType == POLICY_SUBJECT ) || \
		  ( policyType == POLICY_SUBJECT_SPECIFIC ) ) && \
		constrainingAttributePtr == NULL )
		return( CRYPT_OK );

	/* Check the issuer policy */
	if( !checkPolicyType( constrainingAttributePtr , &issuerHasPolicy,
						  &issuerHasAnyPolicy, 
						  ( policyType == POLICY_BOTH_SPECIFIC ) ? \
							TRUE : FALSE ) )
		{
		setErrorValues( CRYPT_CERTINFO_CERTPOLICYID, 
						CRYPT_ERRTYPE_ISSUERCONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* Both the issuer and subject have some sort of policy, if either are 
	   anyPolicy wildcards (introduced in RFC 3280 section 4.2.1.5) then 
	   it's considered a match */
	if( subjectHasAnyPolicy || issuerHasAnyPolicy )
		return( CRYPT_OK );

	/* An explicit policy is required, make sure that at least one of the 
	   issuer policies matches at least one of the subject policies.  Note
	   that there's no exception for PKIX path-kludge certificates, this is 
	   an error in the RFC for which the text at this point is unchanged 
	   from RFC 2459.  In fact this contradicts the path-processing 
	   pesudocode but since that in turn contradicts the main text in a 
	   number of places we take the main text as definitive, not the buggy 
	   pseudocode */
	if( policyInfo != NULL )
		{
		const POLICY_DATA *policyData = policyInfo->policies;
		int i;

		for( i = 0; i < policyInfo->noPolicies && \
					i < FAILSAFE_ITERATIONS_MED; i++ )
			{
			if( policyData[ i ].isMapped && !allowMappedPolicies )
				continue;
			if( isPolicyPresent( constrainedAttributePtr, 
								 policyData[ i ].data, 
								 policyData[ i ].length ) )
				return( CRYPT_OK );
			}
		ENSURES( i < FAILSAFE_ITERATIONS_MED );
		}
	else
		{
		ATTRIBUTE_PTR *constrainingAttributeCursor;

		for( constrainingAttributeCursor = \
				( ATTRIBUTE_PTR * ) constrainingAttributePtr, \
				iterationCount = 0;
			 constrainingAttributeCursor != NULL && \
				iterationCount < FAILSAFE_ITERATIONS_LARGE; 
			 constrainingAttributeCursor = \
				findNextFieldInstance( constrainingAttributeCursor ), \
				iterationCount++ )
			{
			void *constrainingPolicyValuePtr;
			int constrainingPolicyValueLength, status;

			status = getAttributeDataPtr( constrainingAttributeCursor, 
										  &constrainingPolicyValuePtr, 
										  &constrainingPolicyValueLength );
			if( cryptStatusError( status ) )
				break;
			if( isPolicyPresent( constrainedAttributePtr, 
								 constrainingPolicyValuePtr, 
								 constrainingPolicyValueLength ) )
				return( CRYPT_OK );
			}
		ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
		}

	/* We couldn't find a matching policy, report an error */
	setErrorValues( CRYPT_CERTINFO_CERTPOLICYID, CRYPT_ERRTYPE_CONSTRAINT );
	return( CRYPT_ERROR_INVALID );
	}
#endif /* USE_CERTLEVEL_PKIX_FULL */

/****************************************************************************
*																			*
*							Check Path Constraints							*
*																			*
****************************************************************************/

#ifdef USE_CERTLEVEL_PKIX_PARTIAL

/* Check path constraints placed by an issuer, checked if complianceLevel 
   >= CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int checkPathConstraints( const CERT_INFO *subjectCertInfoPtr,
						  IN_LENGTH_SHORT_Z const int pathLength,
						  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
						  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType )
	{
	int value, status;

	assert( isReadPtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( pathLength >= 0 && pathLength < MAX_INTLENGTH_SHORT );

#ifdef USE_CERTLEVEL_PKIX_FULL
	/* If this is a PKIX path-kludge certificate then the path length 
	   constraints don't apply to it (PKIX section 4.2.1.10).  This is 
	   required in order to allow extra certificates to be kludged into the 
	   path without violating the name constraint */
	if( isPathKludge( subjectCertInfoPtr ) )
		return( CRYPT_OK );
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* If the path length constraint hasn't been triggered yet we're OK */
	if( pathLength > 0 )
		return( CRYPT_OK );

	/* If the certificate is self-signed (i.e. the certificate is applying 
	   the constraint to itself) then a path length constraint of zero is 
	   valid.  Checking only the subject certificate information is safe 
	   because the calling code has guaranteed that if the certificate is 
	   self-signed then the issuer attributes are the attributes from the 
	   subject certificate */
	if( subjectCertInfoPtr->flags & CERT_FLAG_SELFSIGNED )
		return( CRYPT_OK );

	/* The path length constraint is in effect, the next certificate down 
	   the chain must be an end-entity certificate */
	status = getAttributeFieldValue( subjectCertInfoPtr->attributes, 
									 CRYPT_CERTINFO_CA, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) && value > 0 )
		{
		setErrorValues( CRYPT_CERTINFO_PATHLENCONSTRAINT,
						CRYPT_ERRTYPE_ISSUERCONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	return( CRYPT_OK );
	}
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

/****************************************************************************
*																			*
*							Check RPKI Attributes							*
*																			*
****************************************************************************/

/* Check attributes for a resource RPKI (RPKI) certificate.  This is 
   somewhat ugly in that it entails a number of implicit checks rather
   than just applying constraints specified in the certificate itself,
   but then again it's not much different to the range of checks
   hardcoded into checkCert(), the only real difference is that the latter
   is specified for all certificates while these are only for RPKI
   certificates */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5 ) ) \
static int checkRPKIAttributes( const ATTRIBUTE_PTR *subjectAttributes,
								const BOOLEAN isCA,
								const BOOLEAN isSelfSigned,
								OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus,
								OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	const ATTRIBUTE_PTR *attributePtr;
	void *policyOidPtr;
	int policyOidLength, value, status;

	assert( isReadPtr( subjectAttributes, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE  ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Check that there's a keyUsage present, and that for CA certificates 
	   it's the CA usages and for EE certificates it's digital signature 
	   (RPKI section 3.9.4) */
	status = getAttributeFieldValue( subjectAttributes,
									 CRYPT_CERTINFO_KEYUSAGE, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusError( status ) )
		{
		setErrorValues( CRYPT_CERTINFO_KEYUSAGE, CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_INVALID );
		}
	if( isCA )
		{
		if( value != CRYPT_KEYUSAGE_DIGITALSIGNATURE )
			status = CRYPT_ERROR_INVALID;
		}
	else
		{
		if( value != ( CRYPT_KEYUSAGE_KEYCERTSIGN | \
					   CRYPT_KEYUSAGE_CRLSIGN ) )
			status = CRYPT_ERROR_INVALID;
		}
	if( cryptStatusError( status ) )
		{
		setErrorValues( CRYPT_CERTINFO_KEYUSAGE, CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ERROR_INVALID );
		}

	/* If it's a CA, check that there's no extendedKeyUsage (RPKI section 
	   3.9.5) and there's a caRepository SIA (RPKI section 3.9.8) */
	if( isCA )
		{
		if( checkAttributePresent( subjectAttributes, 
								   CRYPT_CERTINFO_EXTKEYUSAGE ) )
			{
			setErrorValues( CRYPT_CERTINFO_EXTKEYUSAGE, 
							CRYPT_ERRTYPE_ATTR_PRESENT );
			return( CRYPT_ERROR_INVALID );
			}
		if( !checkAttributeFieldPresent( subjectAttributes,
								CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY ) )
			{
			setErrorValues( CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY, 
							CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* If it's not self-signed (i.e. not a root certificate) check that
	   there's a caIssuers AIA (RPKI section 3.9.7) */
	if( !isSelfSigned )
		{
		if( !checkAttributeFieldPresent( subjectAttributes,
								CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS ) )
			{
			setErrorValues( CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS, 
							CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* Check that there's an RPKI policy present (RPKI section 3.9.9) */
	attributePtr = findAttributeField( subjectAttributes,
									   CRYPT_CERTINFO_CERTPOLICYID,
									   CRYPT_ATTRIBUTE_NONE );
	if( attributePtr == NULL )
		{
		setErrorValues( CRYPT_CERTINFO_CERTPOLICYID, 
						CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_INVALID );
		}
	status = getAttributeDataPtr( attributePtr, &policyOidPtr, 
								  &policyOidLength );
	if( cryptStatusError( status ) || \
		policyOidLength != sizeofOID( OID_RPKI_POLICY ) || \
		memcmp( policyOidPtr, OID_RPKI_POLICY, 
				sizeofOID( OID_RPKI_POLICY ) ) )
		{
		setErrorValues( CRYPT_CERTINFO_CERTPOLICYID, 
						CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ERROR_INVALID );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Check a Certificate	Object						*
*																			*
****************************************************************************/

#ifdef USE_CERTREV

/* Check the consistency of a CRL against its issuing certificate.  Note 
   that this is the reverse of the usual form of checking the certificate 
   against the CRL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5 ) ) \
static int checkCrlConsistency( const CERT_INFO *crlInfoPtr,
								IN_OPT const CERT_INFO *issuerCertInfoPtr,
								IN_RANGE( CRYPT_COMPLIANCELEVEL_OBLIVIOUS, \
										  CRYPT_COMPLIANCELEVEL_LAST - 1 ) \
									const int complianceLevel,
								OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus,
								OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	int deltaCRLindicator, status;

	assert( isReadPtr( crlInfoPtr, sizeof( CERT_INFO ) ) );
	assert( issuerCertInfoPtr == NULL || \
			isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );

	/* If it's a delta CRL make sure that the CRL numbers make sense (that 
	   is, that the delta CRL was issued after the full CRL) */
	status = getAttributeFieldValue( crlInfoPtr->attributes,
									 CRYPT_CERTINFO_DELTACRLINDICATOR, 
									 CRYPT_ATTRIBUTE_NONE, 
									 &deltaCRLindicator );
	if( cryptStatusOK( status ) )
		{
		int crlNumber;

		status = getAttributeFieldValue( crlInfoPtr->attributes,
										 CRYPT_CERTINFO_CRLNUMBER, 
										 CRYPT_ATTRIBUTE_NONE, &crlNumber );
		if( cryptStatusOK( status ) && crlNumber >= deltaCRLindicator )
			{
			setErrorValues( CRYPT_CERTINFO_DELTACRLINDICATOR,
							CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* If it's a standalone CRL entry used purely as a container for 
	   revocation data don't try and perform any issuer-based checking */
	if( issuerCertInfoPtr == NULL )
		return( CRYPT_OK );

	/* Make sure that the issuer can sign CRLs and that the issuer 
	   certificate in general is in order */
	return( checkKeyUsage( issuerCertInfoPtr, 
						   CHECKKEY_FLAG_CA | CHECKKEY_FLAG_GENCHECK, 
						   CRYPT_KEYUSAGE_CRLSIGN, complianceLevel, 
						   errorLocus, errorType ) );
	}
#endif /* USE_CERTREV */

/* Perform basic checks on a certificate.  Apart from its use as part of the 
   normal certificate-checking process this is also used to provide a quick
   "is this certificate obviously invalid" check without having to check
   signatures from issuing certificates and other paraphernalia, for example
   when a full certificate check has been performed earlier and all we want
   to do is make sure that the certificate hasn't expired or been declared
   untrusted in the meantime */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCertBasic( INOUT CERT_INFO *certInfoPtr )
	{
	CRYPT_ATTRIBUTE_TYPE *errorLocus = &certInfoPtr->errorLocus;
	CRYPT_ERRTYPE_TYPE *errorType = &certInfoPtr->errorType;
	const time_t currentTime = getTime();
	int complianceLevel, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE  ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	/* Determine how much checking we need to perform */
	status = krnlSendMessage( certInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &complianceLevel, 
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		return( status );

	/* There is one universal case in which a certificate is regarded as 
	   invalid and that's when it's explicitly not trusted */
	if( certInfoPtr->cCertCert->trustedUsage == 0 )
		{
		setErrorValues( CRYPT_CERTINFO_TRUSTED_USAGE,
						CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If we're running in oblivious mode, we're done */
	if( complianceLevel < CRYPT_COMPLIANCELEVEL_REDUCED )
		return( CRYPT_OK );

	/* Check that the validity period is in order.  If we're checking an 
	   existing certificate then the start time has to be valid, if we're 
	   creating a new certificate then it doesn't have to be valid since the 
	   certificate could be created for use in the future */
	if( currentTime <= MIN_TIME_VALUE )
		{
		/* Time is broken, we can't reliably check for expiry times */
		setErrorValues( CRYPT_CERTINFO_VALIDFROM, CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}
	if( certInfoPtr->startTime >= certInfoPtr->endTime || \
		( certInfoPtr->certificate != NULL && \
		  currentTime < certInfoPtr->startTime ) )
		{
		setErrorValues( CRYPT_CERTINFO_VALIDFROM, CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}
	if( currentTime > certInfoPtr->endTime )
		{
		setErrorValues( CRYPT_CERTINFO_VALIDTO, CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	return( CRYPT_OK );
	}

/* Check the validity of a subject certificate based on an issuer 
   certificate with the level of checking performed depending on the 
   complianceLevel setting.  If the shortCircuitCheck flag is set (used for 
   certificate issuer : subject pairs that may already have been checked) 
   we skip the constant-result checks if the combination has already been 
   checked at this compliance level */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5 ) ) \
int checkCert( INOUT CERT_INFO *subjectCertInfoPtr,
			   IN_OPT const CERT_INFO *issuerCertInfoPtr,
			   const BOOLEAN shortCircuitCheck,
			   OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
					CRYPT_ATTRIBUTE_TYPE *errorLocus,
			   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
					CRYPT_ERRTYPE_TYPE *errorType )
	{
	const ATTRIBUTE_PTR *subjectAttributes = subjectCertInfoPtr->attributes;
	const ATTRIBUTE_PTR *issuerAttributes = \
								( issuerCertInfoPtr != NULL ) ? \
								issuerCertInfoPtr->attributes : NULL;
	const ATTRIBUTE_PTR *attributePtr;
	const BOOLEAN subjectSelfSigned = \
					( subjectCertInfoPtr->flags & CERT_FLAG_SELFSIGNED ) ? \
					TRUE : FALSE;
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	BOOLEAN subjectIsCA = FALSE, issuerIsCA = FALSE;
	int value;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
	int complianceLevel, status;

	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( issuerCertInfoPtr == NULL || \
			isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE  ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Determine how much checking we need to perform.  If this is a 
	   currently-under-construction certificate then we use the maximum 
	   compliance level to ensure that cryptlib never produces broken 
	   certificates */
	if( subjectCertInfoPtr->certificate == NULL )
		complianceLevel = CRYPT_COMPLIANCELEVEL_PKIX_FULL;
	else
		{
		status = krnlSendMessage( subjectCertInfoPtr->ownerHandle, 
								  IMESSAGE_GETATTRIBUTE, &complianceLevel, 
								  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If it's some form of certificate request or an OCSP object (which 
	   means that it isn't signed by an issuer in the normal sense) then 
	   there's nothing to check (yet) */
	switch( subjectCertInfoPtr->type )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
		case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
		case CRYPT_CERTTYPE_CERTCHAIN:
			/* It's an issuer-signed object, there must be an issuer 
			   certificate present */
			REQUIRES( issuerCertInfoPtr != NULL );
			if( subjectCertInfoPtr->flags & CERT_FLAG_CERTCOLLECTION )
				{
				/* Certificate collections are pure container objects for 
				   which the base certificate object doesn't correspond to 
				   an actual certificate */
				retIntError();
				}
			break;

		case CRYPT_CERTTYPE_CERTREQUEST:
		case CRYPT_CERTTYPE_REQUEST_CERT:
		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			/* These are merely templates submitted to a CA, there's nothing 
			   to check.  More specifically, the template could contain 
			   constraints that only make sense once the issuer certificate 
			   is incorporated into a chain or a future-dated validity time 
			   or a CA keyUsage for which the CA provides the appropriate 
			   matching basicConstraints value(s) so we can't really perform 
			   much checking here */
			return( CRYPT_OK );

#ifdef USE_CERTREV
		case CRYPT_CERTTYPE_CRL:
			/* There must be an issuer certificate present unless we're 
			   checking a standalone CRL entry that acts purely as a 
			   container for revocation data */
			assert( issuerCertInfoPtr == NULL || \
					isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );

			/* CRL checking is handled specially */
			return( checkCrlConsistency( subjectCertInfoPtr, 
										 issuerCertInfoPtr, complianceLevel, 
										 errorLocus, errorType ) );
#endif /* USE_CERTREV */

		case CRYPT_CERTTYPE_CMS_ATTRIBUTES:
		case CRYPT_CERTTYPE_PKIUSER:
			retIntError();

		case CRYPT_CERTTYPE_RTCS_REQUEST:
		case CRYPT_CERTTYPE_RTCS_RESPONSE:
		case CRYPT_CERTTYPE_OCSP_REQUEST:
		case CRYPT_CERTTYPE_OCSP_RESPONSE:
			/* These aren't normal certificate types, there's nothing to 
			   check - we can't even check the issuer since they're not 
			   normally issued by CAs */
			return( CRYPT_OK );

		default:
			retIntError();
		}
	ENSURES( issuerCertInfoPtr != NULL );
	ENSURES( subjectCertInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			 subjectCertInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			 subjectCertInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	/* Perform a basic check for obvious invalidity issues */
	status = checkCertBasic( subjectCertInfoPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* There is one universal case in which a certificate is regarded as 
	   invalid and that's when the issuing certificate isn't trusted as an 
	   issuer.  We perform the check in oblivious mode to ensure that only 
	   the basic trusted usage gets checked at this point */
	if( issuerCertInfoPtr->cCertCert->trustedUsage != CRYPT_ERROR )
		{
		status = checkKeyUsage( issuerCertInfoPtr, CHECKKEY_FLAG_CA, 
								CRYPT_KEYUSAGE_KEYCERTSIGN,
								CRYPT_COMPLIANCELEVEL_OBLIVIOUS, 
								errorLocus, errorType );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If we're running in oblivious mode, we're done */
	if( complianceLevel < CRYPT_COMPLIANCELEVEL_REDUCED )
		return( CRYPT_OK );

	/* If it's a self-signed certificate or if we're doing a short-circuit 
	   check of a certificate in a chain that's already been checked and 
	   we've already checked it at the appropriate level then there's no 
	   need to perform any further checks */
	if( ( subjectSelfSigned || shortCircuitCheck ) && \
		( subjectCertInfoPtr->cCertCert->maxCheckLevel >= complianceLevel ) )
		return( CRYPT_OK );

	/* If the certificate isn't self-signed, check name chaining */
	if( !subjectSelfSigned )
		{
		/* Check that the subject issuer name and issuer subject name chain
		   properly.  If the DNs are present in pre-encoded form we do a 
		   binary comparison, which is faster than calling compareDN() */
		if( subjectCertInfoPtr->certificate != NULL )
			{
			if( subjectCertInfoPtr->issuerDNsize != \
							issuerCertInfoPtr->subjectDNsize || \
				memcmp( subjectCertInfoPtr->issuerDNptr, 
						issuerCertInfoPtr->subjectDNptr, 
						subjectCertInfoPtr->issuerDNsize ) )
				{
				setErrorValues( CRYPT_CERTINFO_ISSUERNAME, 
								CRYPT_ERRTYPE_CONSTRAINT );
				return( CRYPT_ERROR_INVALID );
				}
			}
		else
			{
			if( !compareDN( subjectCertInfoPtr->issuerName,
							issuerCertInfoPtr->subjectName, FALSE, NULL ) )
				{
				setErrorValues( CRYPT_CERTINFO_ISSUERNAME, 
								CRYPT_ERRTYPE_CONSTRAINT );
				return( CRYPT_ERROR_INVALID );
				}
			}
		}

	/* If we're doing a reduced level of checking, we're done */
	if( complianceLevel < CRYPT_COMPLIANCELEVEL_STANDARD )
		{
		if( subjectCertInfoPtr->cCertCert->maxCheckLevel < complianceLevel )
			subjectCertInfoPtr->cCertCert->maxCheckLevel = complianceLevel;
		return( CRYPT_OK );
		}

	/* Check that the certificate usage flags are present and consistent.  
	   The key usage checking level ranges up to 
	   CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL so we re-do the check even if it's 
	   already been done at a lower level */
	if( subjectCertInfoPtr->cCertCert->maxCheckLevel < CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL && \
		subjectCertInfoPtr->type != CRYPT_CERTTYPE_ATTRIBUTE_CERT )
		{
		status = checkKeyUsage( subjectCertInfoPtr, CHECKKEY_FLAG_GENCHECK, 
								CRYPT_KEYUSAGE_NONE, complianceLevel, 
								errorLocus, errorType );
		if( cryptStatusError( status ) )
			return( status );
        }

	/* If the certificate isn't self-signed check that the issuer is a CA */
	if( !subjectSelfSigned )
		{
		status = checkKeyUsage( issuerCertInfoPtr, CHECKKEY_FLAG_CA, 
								CRYPT_KEYUSAGE_KEYCERTSIGN, complianceLevel, 
								errorLocus, errorType );
		if( cryptStatusError( status ) )
			{
			/* There was a problem with the issuer certificate, convert the 
			   error to an issuer constraint */
			*errorType = CRYPT_ERRTYPE_ISSUERCONSTRAINT;
			return( status );
			}
		}

	/* Check all the blob (unrecognised) attributes to see if any are marked 
	   critical.  We only do this if it's an existing certificate that we've
	   imported rather than one that we've just created since applying this 
	   check to the latter would make it impossible to create certificates 
	   with unrecognised critical extensions */
	if( subjectCertInfoPtr->certificate != NULL )
		{
		ATTRIBUTE_ENUM_INFO attrEnumInfo;
		int iterationCount;

		for( attributePtr = getFirstAttribute( &attrEnumInfo, subjectAttributes,
											   ATTRIBUTE_ENUM_BLOB ), \
				iterationCount = 0;
			 attributePtr != NULL && \
				iterationCount < FAILSAFE_ITERATIONS_LARGE; 
			 attributePtr = getNextAttribute( &attrEnumInfo ), \
				iterationCount++ )
			{
			/* If we've found an unrecognised critical extension, reject the 
			   certificate (PKIX section 4.2).  The one exception to this is 
			   if the attribute was recognised but has been ignored at this 
			   compliance level, in which case it's treated as a blob
			   attribute */
			if( checkAttributeProperty( attributePtr, \
										ATTRIBUTE_PROPERTY_CRITICAL ) && \
				!checkAttributeProperty( attributePtr, \
										 ATTRIBUTE_PROPERTY_IGNORED ) )
				{
				setErrorValues( CRYPT_ATTRIBUTE_NONE, 
								CRYPT_ERRTYPE_CONSTRAINT );
				return( CRYPT_ERROR_INVALID );
				}
			}
		ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
		}

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* If we're not doing at least partial PKIX checking, we're done */
	if( complianceLevel < CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL )
		{
		if( subjectCertInfoPtr->cCertCert->maxCheckLevel < complianceLevel )
			subjectCertInfoPtr->cCertCert->maxCheckLevel = complianceLevel;
		return( CRYPT_OK );
		}

	/* Determine whether the subject or issuer are CA certificates */
	status = getAttributeFieldValue( subjectAttributes, 
									 CRYPT_CERTINFO_CA, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) )
		subjectIsCA = ( value > 0 ) ? TRUE : FALSE;
	status = getAttributeFieldValue( issuerAttributes,
									 CRYPT_CERTINFO_CA, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) )
		issuerIsCA = ( value > 0 ) ? TRUE : FALSE;

	/* Constraints can only be present in CA certificates.  The issuer may 
	   not be a proper CA if it's a self-signed end entity certificate or 
	   an X.509v1 CA certificate, which is why we also check for 
	   !issuerIsCA */
	if( subjectAttributes != NULL )
		{
		if( !subjectIsCA && invalidAttributesPresent( subjectAttributes, FALSE, 
													  errorLocus, errorType ) )
			return( CRYPT_ERROR_INVALID );
		if( !issuerIsCA && invalidAttributesPresent( subjectAttributes, TRUE, 
													 errorLocus, errorType ) )
			return( CRYPT_ERROR_INVALID );
		}

	/*  From this point onwards if we're doing a short-circuit check of 
	    certificates in a chain we don't apply constraint checks.  This is 
		because the certificate-chain code has already performed far more 
		complete checks of the various constraints set by all the 
		certificates in the chain rather than just the current certificate 
		issuer : subject pair */

	/* If there's a path length constraint present, apply it */
	status = getAttributeFieldValue( issuerAttributes,
									 CRYPT_CERTINFO_PATHLENCONSTRAINT, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) && !shortCircuitCheck )
		{
		status = checkPathConstraints( subjectCertInfoPtr, value,
									   errorLocus, errorType );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* In order to dig itself out of a hole caused by a circular definition, 
	   RFC 3280 added a new extKeyUsage anyExtendedKeyUsage (rather than the
	   more obvious fix of removing the problematic definition).  
	   Unfortunately this causes more problems than it solves because the exact
	   semantics of this new usage aren't precisely defined.  To fix this 
	   problem we invent some plausible ones ourselves: If the only eKU is 
	   anyKU we treat the overall extKeyUsage as empty, i.e. there are no
	   particular restrictions on usage.  If any other usage is present then 
	   the extension has become self-contradictory so we treat the anyKU as
	   being absent.  See the comment for getExtendedKeyUsageFlags() for how
	   this is handled */
	attributePtr = findAttributeField( subjectAttributes,
									   CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE, 
									   CRYPT_ATTRIBUTE_NONE );
	if( attributePtr != NULL && \
		checkAttributeProperty( attributePtr, ATTRIBUTE_PROPERTY_CRITICAL ) )
		{
		/* If anyKU is present the extension must be non-critical 
		   (PKIX section 4.2.1.13) */
		setErrorValues( CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE, 
						CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If this is a resource PKI (RPKI) certificate, apply RPKI-specific 
	   checks */
	if( checkAttributePresent( subjectAttributes, \
							   CRYPT_CERTINFO_IPADDRESSBLOCKS ) || \
		checkAttributePresent( subjectAttributes, \
							   CRYPT_CERTINFO_AUTONOMOUSSYSIDS ) )
		{
		ANALYSER_HINT( subjectAttributes != NULL );

		status = checkRPKIAttributes( subjectAttributes, subjectIsCA,
									  subjectSelfSigned, errorLocus,
									  errorType );
		if( cryptStatusError( status ) )
			return( status );
		}

#ifdef USE_CERTLEVEL_PKIX_FULL
	/* If we're not doing full PKIX checking, we're done.  In addition since 
	   all of the remaining checks are constraint checks we can exit at this
	   point if we're doing a short-circuit check */
	if( complianceLevel < CRYPT_COMPLIANCELEVEL_PKIX_FULL || \
		shortCircuitCheck )
		{
		if( subjectCertInfoPtr->cCertCert->maxCheckLevel < complianceLevel )
			subjectCertInfoPtr->cCertCert->maxCheckLevel = complianceLevel;
		return( CRYPT_OK );
		}

	/* If the issuing certificate has name constraints and isn't 
	   self-signed make sure that the subject name and altName fall within 
	   the constrained subtrees.  Since excluded subtrees override permitted 
	   subtrees we check these first */
	if( !subjectSelfSigned )
		{
		attributePtr = findAttributeField( issuerAttributes, 
										   CRYPT_CERTINFO_EXCLUDEDSUBTREES,
										   CRYPT_ATTRIBUTE_NONE );
		if( attributePtr != NULL && \
			cryptStatusError( \
				checkNameConstraints( subjectCertInfoPtr, attributePtr, 
									  TRUE, errorLocus, errorType ) ) )
			return( CRYPT_ERROR_INVALID );
		attributePtr = findAttributeField( issuerAttributes, 
										   CRYPT_CERTINFO_PERMITTEDSUBTREES,
										   CRYPT_ATTRIBUTE_NONE );
		if( attributePtr != NULL && \
			cryptStatusError( \
				checkNameConstraints( subjectCertInfoPtr, attributePtr, 
									  FALSE, errorLocus, errorType ) ) )
			return( CRYPT_ERROR_INVALID );
		}

	/* If there's a policy constraint present and the skip count is set to 
	   zero (i.e. the constraint applies to the current certificate) check 
	   the issuer constraints against the subject */
	status = getAttributeFieldValue( issuerAttributes,
									 CRYPT_CERTINFO_REQUIREEXPLICITPOLICY,
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) && value <= 0 )
		{
		POLICY_TYPE policyType = POLICY_SUBJECT;

		/* Check whether use of the the wildcard anyPolicy has been 
		   disallowed */
		attributePtr = findAttribute( issuerCertInfoPtr->attributes, 
									  CRYPT_CERTINFO_INHIBITANYPOLICY, 
									  TRUE );
		if( attributePtr != NULL && \
			cryptStatusOK( getAttributeDataValue( attributePtr, \
												  &value ) ) && \
			value <= 0 )
			policyType = POLICY_SUBJECT_SPECIFIC;

		/* Apply the appropriate policy constraint */
		status = checkPolicyConstraints( subjectCertInfoPtr,
										 issuerAttributes, policyType,
										 NULL, 0, FALSE, errorLocus, 
										 errorType );
		if( cryptStatusError( status ) )
			return( status );
		}
#endif /* USE_CERTLEVEL_PKIX_FULL */
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

	if( subjectCertInfoPtr->cCertCert->maxCheckLevel < complianceLevel )
		subjectCertInfoPtr->cCertCert->maxCheckLevel = complianceLevel;
	return( CRYPT_OK );
	}
#endif /* USE_CERTIFICATES */
