/****************************************************************************
*																			*
*					  Certificate Chain Checking Routines					*
*						Copyright Peter Gutmann 1996-2009					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
#else
  #include "cert/cert.h"
#endif /* Compiler-specific includes */

/* This module and chk_cert.c implement the following PKIX checks.  For
   simplicity we use the more compact form of RFC 2459 rather than the 18
   page long one from RFC 3280.

	General:

	(a) Verify the basic certificate information:
		(1) The certificate signature is valid.
		(2a) The certificate has not expired.
		(2b) If present, the private key usage period is satisfied.
		(3) The certificate has not been revoked.
		(4a) The subject and issuer name chains correctly.
		(4b) If present, the subjectAltName and issuerAltName chains
			 correctly.

	NameConstraints:

	(b) Verify that the subject name or critical subjectAltName is consistent
		with the constrained subtrees.

	(c) Verify that the subject name or critical subjectAltName is consistent
		with the excluded subtrees.

	Policy Constraints:

	(d) Verify that policy information is consistent with the initial policy 
		set:
		(1) If the require explicit policy state variable is less than or 
			equal to n, a policy identifier in the certificate must be in 
			the initial policy set.
		(2) If the policy mapping state variable is less than or equal to n, 
			the policy identifier may not be mapped.
		(3) RFC 3280 addition: If the inhibitAnyPolicy state variable is 
			less than or equal to n, the anyPolicy policy is no longer 
			considered a match (this also extends into (e) and (g) below).

	(e) Verify that policy information is consistent with the acceptable policy 
		set:
		(1) If the policies extension is marked critical, the policies
			extension must lie within the acceptable policy set.
		(2) The acceptable policy set is assigned the resulting intersection
			as its new value.

	(g) Verify that the intersection of the acceptable policy set and the
		initial policy set is non-null (this is covered by chaining of e(1)).

	Other Constraints:

	(f) Step (f) is missing in the original, it should probably be: Verify 
		that the current path length is less than the path length constraint.  
		If a path length constraint is present in the certificate, update it 
		as for policy constraints in (l).  RFC 3280 addition: If the 
		certificate is a PKIX path kludge certificate it doesn't count for 
		path length constraint purposes.

	(h) Recognize and process any other critical extension present in the
		certificate.

	(i) Verify that the certificate is a CA certificate.

	Update of state:

	(j) If permittedSubtrees is present in the certificate, set the
		constrained subtrees state variable to the intersection of its
		previous value and the value indicated in the extension field.

	(k) If excludedSubtrees is present in the certificate, set the excluded
		subtrees state variable to the union of its previous value and the
		value indicated in the extension field.

	(l) If a policy constraints extension is included in the certificate,
		modify the explicit policy and policy mapping state variables as
		follows:

		For any of { requireExplicitPolicy, inhibitPolicyMapping, 
		inhibitAnyPolicy }, if the constraint value is present and has value 
		r, the state variable is set to the minimum of (a) its current value 
		and (b) the sum of r and n (the current certificate in the 
		sequence) 

	(m) If a key usage extension is marked critical, ensure that the 
		keyCertSign bit is set */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Get certificate information for a certificate in the chain.  The index 
   value can go to -1, indicating the leaf certificate itself rather than a
   certificate in the chain */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getCertInfo( const CERT_INFO *certInfoPtr,
						INOUT_PTR CERT_INFO **certChainPtr, 
						IN_RANGE( -1, MAX_CHAINLENGTH - 1 ) const int certChainIndex )
	{
	CERT_CERT_INFO *certChainInfo = certInfoPtr->cCertCert;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( certChainPtr, sizeof( CERT_INFO * ) ) );

	REQUIRES( certChainIndex >= -1 && \
			  certChainIndex < certChainInfo->chainEnd && \
			  certChainIndex < MAX_CHAINLENGTH );

	/* Clear return value */
	*certChainPtr = NULL;

	/* If it's an index into the certificate chain, return information for 
	   the certificate at that position */
	if( certChainIndex >= 0 && certChainIndex < certChainInfo->chainEnd )
		{
		return( krnlAcquireObject( certChainInfo->chain[ certChainIndex ], 
								   OBJECT_TYPE_CERTIFICATE, 
								   ( void ** ) certChainPtr, 
								   CRYPT_ERROR_SIGNALLED ) );
		}

	/* The -1th certificate is the leaf itself */
	if( certChainIndex == -1 )
		{
		*certChainPtr = ( CERT_INFO * ) certInfoPtr;
		return( CRYPT_OK );
		}

	/* We've reached the end of the chain */
	*certChainPtr = NULL;
	return( CRYPT_ERROR_NOTFOUND );
	}

/* When checking a particular certificate in a chain via a message sent to 
   the encompassing object we have to explicitly select the certificate that
   we want to operate on, otherwise all messages will act on the currently-
   selected certificate in the chain.  To do this we temporarily select the
   target certificate in the chain and then restore the selection state when
   we're done */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int performAbsTrustOperation( INOUT CERT_INFO *certInfoPtr, 
									 IN_ENUM( MESSAGE_TRUSTMGMT ) \
											const MESSAGE_TRUSTMGMT_TYPE operation,
									 IN_RANGE( -1, MAX_CHAINLENGTH - 1 ) \
											const int certChainIndex,
									 OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iIssuerCert )
	{
	CRYPT_CERTIFICATE iLocalCert;
	CERT_CERT_INFO *certChainInfo = certInfoPtr->cCertCert;
	SELECTION_STATE savedState;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( iIssuerCert == NULL || \
			isWritePtr( iIssuerCert, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( certChainIndex >= -1 && \
			  certChainIndex < certChainInfo->chainEnd && \
			  certChainIndex < MAX_CHAINLENGTH );

	/* Clear return value */
	if( iIssuerCert != NULL )
		*iIssuerCert = CRYPT_ERROR;

	/* Perform the required operation at an absolute chain position */
	saveSelectionState( savedState, certInfoPtr );
	certChainInfo->chainPos = certChainIndex;
	if( certChainIndex == -1 )
		{
		/* The -1th certificate is the leaf itself */
		iLocalCert = certInfoPtr->objectHandle;
		}
	else
		{
		/* It's an index into the certificate chain, use the certificate at 
		   that position */
		iLocalCert = certChainInfo->chain[ certChainIndex ];
		}
	status = krnlSendMessage( certInfoPtr->ownerHandle, 
							  IMESSAGE_USER_TRUSTMGMT, &iLocalCert, 
							  operation );
	restoreSelectionState( savedState, certInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	if( iIssuerCert != NULL )
		*iIssuerCert = iLocalCert;
	return( CRYPT_OK );
	}

/* Find the trust anchor in a certificate chain.  The definition of a 
   "trusted certificate" is somewhat ambiguous and can have at least two 
   different interpretations:

	1. Trust the identified certificate in the chain and only verify from 
	   there on down.

	2. Trust the root of the chain that contains the identified certificate 
	   (for the purposes of verifying that particular chain only) and verify 
	   the whole chain.

   Situation 1 is useful where there's a requirement that things go up to an
   external CA somewhere but no-one particularly cares about (or trusts) the
   external CA.  In this case the end user can choose to trust the path at 
   the point where it comes under their control (a local CA or directly 
   trusting the leaf certificates) without having to bother about the 
   external CA.

   Situation 2 is useful where there's a requirement to use the full PKI 
   model.  This can be enabled by having the user mark the root CA as
   trusted, although this means that all certificates issued by that CA also 
   have to be trusted, removing user control over certificate use.  This is 
   required by orthodox PKI theology, followed by all manner of hacks and
   kludges down the chain to limit what can actually be done with the 
   certificate(s).

   Est autem fides credere quod nondum vides; cuius fidei merces est videre 
   quod credis (St. Augustine) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int findTrustAnchor( INOUT CERT_INFO *certInfoPtr, 
							OUT_RANGE( -1, MAX_CHAINLENGTH - 1 ) \
								int *trustAnchorIndexPtr, 
							OUT_HANDLE_OPT CRYPT_CERTIFICATE *trustAnchorCert )
	{
	CRYPT_CERTIFICATE iIssuerCert = DUMMY_INIT;
	CERT_CERT_INFO *certChainInfo = certInfoPtr->cCertCert;
	int trustAnchorIndex, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( trustAnchorIndexPtr, sizeof( int ) ) );
	assert( isWritePtr( trustAnchorCert, sizeof( CRYPT_CERTIFICATE ) ) );

	/* Clear return value */
	*trustAnchorIndexPtr = CRYPT_ERROR;
	*trustAnchorCert = CRYPT_ERROR;

	/* If the leaf certificate is implicitly trusted, exit.  Since this is a
	   certificate chain we have to explicitly perform the operation at an
	   absolute position in the chain rather than the currently-selected 
	   certificate, specifically the leaf at position -1 */
	status = performAbsTrustOperation( certInfoPtr, MESSAGE_TRUSTMGMT_CHECK,
									   -1, NULL );
	if( cryptStatusOK( status ) )
		{
		/* Indicate that the leaf is trusted and there's nothing further to 
		   do */
		return( OK_SPECIAL );
		}

	/* Walk up the chain looking for the trusted certificate that issued the
	   current one.  The evaluated trust anchor certificate position is one 
	   past the current position since we're looking for the issuer of the 
	   current certificate at position n, which will be located at position 
	   n+1:
	   
		trustAnchorIndex	Cert queried	trustAnchorCert
		----------------	------------	---------------
				0			  cert #-1		 issuer( #-1 )
				1			  cert #0		 issuer( #0 )
				2			  cert #1		 issuer( #1 )
	   
	   This means that trustAnchorIndex may end up referencing a certificate 
	   past the end of the chain if the trust anchor is present in the trust 
	   database but not in the chain */
	REQUIRES( certChainInfo->chainEnd >= 0 );
	for( trustAnchorIndex = 0;
		 trustAnchorIndex <= certChainInfo->chainEnd && \
			trustAnchorIndex < MAX_CHAINLENGTH; 
		 trustAnchorIndex++ )
		{
		status = performAbsTrustOperation( certInfoPtr, 
										   MESSAGE_TRUSTMGMT_GETISSUER, 
										   trustAnchorIndex - 1, 
										   &iIssuerCert );
		if( cryptStatusOK( status ) )
			break;
		}
	ENSURES( trustAnchorIndex < MAX_CHAINLENGTH );
	if( cryptStatusError( status ) || \
		trustAnchorIndex > certChainInfo->chainEnd )
		return( CRYPT_ERROR_NOTFOUND );
	*trustAnchorIndexPtr = trustAnchorIndex;
	*trustAnchorCert = iIssuerCert;

	/* If there are more certificates in the chain beyond the one that we 
	   stopped at check to see whether the next certificate is the same as 
	   the trust anchor.  If it is then we use the copy of the certificate 
	   in the chain rather than the external one from the trust database */
	if( trustAnchorIndex < certChainInfo->chainEnd - 1 )
		{
		status = krnlSendMessage( certChainInfo->chain[ trustAnchorIndex ],
								  IMESSAGE_COMPARE, &iIssuerCert, 
								  MESSAGE_COMPARE_CERTOBJ );
		if( cryptStatusOK( status ) )
			*trustAnchorCert = certChainInfo->chain[ trustAnchorIndex ];
		}
	return( CRYPT_OK );
	}

/* Set error information for the certificate that caused problems when 
   looking for a trust anchor */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setTrustAnchorErrorInfo( INOUT CERT_INFO *certInfoPtr )
	{
	CRYPT_ATTRIBUTE_TYPE attributeType;
	CERT_CERT_INFO *certChainInfo = certInfoPtr->cCertCert;
	CERT_INFO *subjectCertInfoPtr;
	const int lastCertIndex = certChainInfo->chainEnd - 1;
	int value, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	ENSURES( lastCertIndex >= 0 && lastCertIndex < certChainInfo->chainEnd );

	/* Select the certificate that caused the problem, which is the highest-
	   level certificate in the chain */
	certChainInfo->chainPos = lastCertIndex;

	/* We couldn't find a trust anchor, either there's a missing link in the 
	   chain (CRYPT_ERROR_STUART) and it was truncated before we got to a 
	   trusted certificate or it goes to a root certificate but it isn't 
	   trusted.  Returning error information on this is a bit complex since 
	   we've selected the certificate that caused the problem, which means 
	   that any attempt to read error status information will read it from 
	   this certificate rather than the encapsulating chain object.  To get 
	   around this we set the error information for the selected certificate 
	   rather than the chain */
	status = krnlSendMessage( certChainInfo->chain[ lastCertIndex ], 
							  IMESSAGE_GETATTRIBUTE, &value, 
							  CRYPT_CERTINFO_SELFSIGNED );
	if( cryptStatusOK( status ) && value > 0 )
		{
		/* We got a root certificate but it's not trusted */
		attributeType = CRYPT_CERTINFO_TRUSTED_IMPLICIT;
		}
	else
		{
		/* There's a missing link in the chain and it stops at this 
		   certificate */
		attributeType = CRYPT_CERTINFO_CERTIFICATE;
		}
	status = getCertInfo( certInfoPtr, &subjectCertInfoPtr, 
						  lastCertIndex );
	if( cryptStatusOK( status ) )
		{
		setErrorInfo( subjectCertInfoPtr, attributeType, 
					  CRYPT_ERRTYPE_ATTR_ABSENT );

		/* If we're not at the leaf certificate then we have to unlock the 
		   certificate that getCertInfo() provided us */
		if( certInfoPtr != subjectCertInfoPtr )
			krnlReleaseObject( subjectCertInfoPtr->objectHandle );
		}

	return( CRYPT_ERROR_INVALID );
	}

/****************************************************************************
*																			*
*							Policy Management Functions						*
*																			*
****************************************************************************/

#ifdef USE_CERTLEVEL_PKIX_FULL

/* Check whether a policy is already present in the policy set */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static BOOLEAN isPolicyPresent( const POLICY_DATA *policyData,
								IN_RANGE( 0, MAX_POLICIES ) const int policyCount,
								IN_BUFFER( policyValueLength ) const void *policyValue,
								IN_LENGTH_OID const int policyValueLength )
	{
	int i;

	assert( isReadPtr( policyData, sizeof( POLICY_DATA ) ) );
	assert( isReadPtr( policyValue, policyValueLength ) );

	REQUIRES_B( policyCount >= 0 && policyCount < MAX_POLICIES );
	REQUIRES_B( policyValueLength > 0 && policyValueLength < MAX_POLICY_SIZE );

	/* Check whether the given policy is already present in the set of 
	   acceptable policies */
	for( i = 0; i < policyCount && i < FAILSAFE_ITERATIONS_MED; i++ )
		{
		const POLICY_DATA *policyDataPtr = &policyData[ i ];

		if( policyDataPtr->length == policyValueLength && \
			!memcmp( policyDataPtr->data, policyValue, policyValueLength ) )
			return( TRUE );
		}
	ENSURES_B( i < FAILSAFE_ITERATIONS_MED );

	return( FALSE );
	}

/* Add a policy to the policy set */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int addPolicy( INOUT POLICY_DATA *policyData,
					  IN_RANGE( 0, MAX_POLICIES ) const int policyCount,
					  const ATTRIBUTE_PTR *policyAttributePtr,
					  IN_RANGE( -1, MAX_CHAINLENGTH - 1 ) \
							const int certChainIndex,
					  const BOOLEAN isMapped )
	{
	POLICY_DATA *policyDataPtr;
	void *policyValuePtr;
	int policyValueLength, status;

	assert( isWritePtr( policyData, sizeof( POLICY_DATA ) ) );
	assert( isReadPtr( policyAttributePtr, 
					   sizeof( ATTRIBUTE_PTR_STORAGE ) ) );

	REQUIRES( policyCount >= 0 && policyCount < MAX_POLICIES );
	REQUIRES( certChainIndex >= -1 && certChainIndex < MAX_CHAINLENGTH );

	/* Get the policy value */
	status = getAttributeDataPtr( policyAttributePtr, &policyValuePtr,
								  &policyValueLength );	
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that this policy isn't already present in the policy set.
	   Since policies are asserted all the way up and down a chain we're
	   going to find more copies in subsequent certificates so duplicates
	   aren't a problem */
	if( isPolicyPresent( policyData, policyCount, policyValuePtr, 
						 policyValueLength ) )
		return( OK_SPECIAL );

	/* Copy the policy data to the next empty slot.  The policy level is 
	   counted from 0 (the EE certificate) to n (the root certificate) so we 
	   have to adjust the chain-position indicator by one since it denotes 
	   the EE, the containing certificate, with the virtual position -1 and 
	   the remainder of the certificates in the chain with positions 0...n */
	policyDataPtr = &policyData[ policyCount ];
	memset( policyDataPtr, 0, sizeof( POLICY_DATA ) );
	policyDataPtr->level = certChainIndex + 1;
	policyDataPtr->isMapped = isMapped;
	return( attributeCopyParams( policyDataPtr->data, MAX_POLICY_SIZE,
								 &policyDataPtr->length, policyValuePtr, 
								 policyValueLength ) );
	}

/* Add explicit policies to the policy set */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int addExplicitPolicies( INOUT POLICY_INFO *policyInfo,
								const ATTRIBUTE_PTR *attributes,
								IN_RANGE( -1, MAX_CHAINLENGTH - 1 ) \
									const int certChainIndex )
	{
	const ATTRIBUTE_PTR *attributeCursor;
	int policyCount = policyInfo->noPolicies, iterationCount, status;

	assert( isWritePtr( policyInfo, sizeof( POLICY_INFO ) ) );
	assert( isReadPtr( attributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );

	REQUIRES( certChainIndex >= -1 && certChainIndex < MAX_CHAINLENGTH );

	/* Add all policies to the policy set */
	for( attributeCursor = findAttributeField( attributes, 
				CRYPT_CERTINFO_CERTPOLICYID, CRYPT_ATTRIBUTE_NONE ), 
			iterationCount = 0;
		 attributeCursor != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 attributeCursor = findNextFieldInstance( attributeCursor ), 
			iterationCount++ )
		{
		if( policyCount >= MAX_POLICIES )
			return( CRYPT_ERROR_OVERFLOW );
		status = addPolicy( policyInfo->policies, policyCount,
							attributeCursor, certChainIndex, FALSE );
		if( status == OK_SPECIAL )
			{
			/* This policy is already present, there's nothing further to 
			   do */
			continue;
			}
		if( cryptStatusError( status ) )
			return( status );
		policyCount++;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	policyInfo->noPolicies = policyCount;

	return( CRYPT_OK );
	}

/* Add mapped policies to the policy set */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int addMappedPolicies( INOUT POLICY_INFO *policyInfo,
							  const ATTRIBUTE_PTR *attributes,
							  IN_RANGE( -1, MAX_CHAINLENGTH - 1 ) \
									const int certChainIndex )
	{
	const ATTRIBUTE_PTR *sourcePolicyAttributeCursor = \
					findAttributeField( attributes, 
										CRYPT_CERTINFO_ISSUERDOMAINPOLICY,
										CRYPT_ATTRIBUTE_NONE );
	const ATTRIBUTE_PTR *destPolicyAttributeCursor = \
					findAttributeField( attributes, 
										CRYPT_CERTINFO_SUBJECTDOMAINPOLICY,
										CRYPT_ATTRIBUTE_NONE );
	int policyCount = policyInfo->noPolicies, iterationCount;
	int status = CRYPT_OK;

	assert( isWritePtr( policyInfo, sizeof( POLICY_INFO ) ) );
	assert( isReadPtr( attributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );

	REQUIRES( certChainIndex >= -1 && certChainIndex < MAX_CHAINLENGTH );

	/* If there are no mapped policies, we're done */
	if( sourcePolicyAttributeCursor == NULL )
		return( CRYPT_OK );

	/* Add all mapped policies to the policy set */
	for( iterationCount = 0;
		 sourcePolicyAttributeCursor != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 sourcePolicyAttributeCursor = \
				findNextFieldInstance( sourcePolicyAttributeCursor ), \
			destPolicyAttributeCursor = \
				findNextFieldInstance( destPolicyAttributeCursor ), \
			iterationCount++ )
		{
		void *policyValuePtr;
		int policyValueLength;

		REQUIRES( sourcePolicyAttributeCursor != NULL && \
				  destPolicyAttributeCursor != NULL );

		/* Make sure that we're not trying to map from or to the special-case 
		   anyPolicy policy */
		if( isAnyPolicy( sourcePolicyAttributeCursor ) || \
			isAnyPolicy( destPolicyAttributeCursor ) )
			return( CRYPT_ERROR_INVALID );

		/* Get the source policy and check whether it's present in the 
		   policy set */
		status = getAttributeDataPtr( sourcePolicyAttributeCursor, &policyValuePtr,
									  &policyValueLength );	
		if( cryptStatusError( status ) )
			continue;
		if( !isPolicyPresent( policyInfo->policies, policyCount, 
							  policyValuePtr, policyValueLength ) )
			continue;

		/* The source policy is present, add the corresponding destination
		   policy to the policy set */
		if( policyCount >= MAX_POLICIES )
			return(  CRYPT_ERROR_OVERFLOW );
		status = addPolicy( policyInfo->policies, policyCount,
							destPolicyAttributeCursor, certChainIndex, TRUE );
		if( status == OK_SPECIAL )
			{
			/* This destination policy is already present, there's nothing 
			   further to do.  This can happen if there's two different 
			   source policies mapped to a single destination policy */
			continue;
			}
		if( cryptStatusError( status ) )
			return( status );
		policyCount++;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	policyInfo->noPolicies = policyCount;

	return( CRYPT_OK );
	}

/* Create a certificate policy set from a certificate chain */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int createPolicySet( OUT POLICY_INFO *policyInfo,
							IN_OPT const ATTRIBUTE_PTR *trustAnchorAttributes,
							INOUT CERT_INFO *certInfoPtr,
							IN_RANGE( -1, MAX_CHAINLENGTH - 1 ) \
								const int startCertIndex )
	{
	BOOLEAN addImplicitPolicy = FALSE;
	int certIndex = startCertIndex, iterationCount, status;

	assert( isWritePtr( policyInfo, sizeof( POLICY_INFO ) ) );
	assert( ( trustAnchorAttributes == NULL ) || \
			isReadPtr( trustAnchorAttributes, 
					   sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( startCertIndex >= -1 && startCertIndex < MAX_CHAINLENGTH );

	/* Clear return value */
	memset( policyInfo, 0, sizeof( POLICY_INFO ) );

	/* Add trust anchor explicit and mapped policies if required */
	if( trustAnchorAttributes != NULL && \
		checkAttributeFieldPresent( trustAnchorAttributes, 
									CRYPT_CERTINFO_CERTPOLICYID ) )
		{
		status = addExplicitPolicies( policyInfo, trustAnchorAttributes, 
									  certIndex );
		if( cryptStatusError( status ) )
			return( status );
		status = addMappedPolicies( policyInfo, trustAnchorAttributes,
									certIndex );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* We've checked the trust anchor, move on to the next certificate */
	certIndex--;

	/* If there are no policies in the trust anchor, pick up the policies in
	   the first certificate we get to that has any.  This is a bit of an
	   ugly hack that's required to deal with things like self-signed CA 
	   roots in X.509v1 format where the CA policy doesn't appear until the
	   second certficiate in the chain */
	if( policyInfo->noPolicies <= 0 )
		addImplicitPolicy = TRUE;

	/* Add mapped policies from the remainder of the certificate chain.  
	   Note that we don't go all the way down to the EE certificate 
	   (certIndex == -1) because any mapping at the end of the chain won't
	   be used any more */
	for( iterationCount = 0;
		 certIndex >= 0 && iterationCount < MAX_CHAINLENGTH; 
		 certIndex--, iterationCount++ )
		{
		CERT_INFO *subjectCertInfoPtr;

		/* Get information for the current certificate in the chain */
		status = getCertInfo( certInfoPtr, &subjectCertInfoPtr, certIndex );
		if( cryptStatusError( status ) )
			break;

		if( addImplicitPolicy && \
			checkAttributeFieldPresent( subjectCertInfoPtr->attributes, 
										CRYPT_CERTINFO_CERTPOLICYID ) )
			{
			status = addExplicitPolicies( policyInfo, 
										  subjectCertInfoPtr->attributes,
										  certIndex );
			if( cryptStatusError( status ) )
				return( status );
			if( policyInfo->noPolicies > 0 )
				addImplicitPolicy = FALSE;
			}

		/* Add any mapped policies present in the current certificate */
		status = addMappedPolicies( policyInfo, 
									subjectCertInfoPtr->attributes, 
									certIndex );
		if( cryptStatusError( status ) )
			{
			krnlReleaseObject( subjectCertInfoPtr->objectHandle );
			return( status );
			}

		/* Release the certificate again.  We don't have to check for it 
		   being the chain certificate as we normally would because we never
		   process the certificate at the end of the chain */
		krnlReleaseObject( subjectCertInfoPtr->objectHandle );
		}
	ENSURES( iterationCount < MAX_CHAINLENGTH );

	return( CRYPT_OK );
	}
#endif /* USE_CERTLEVEL_PKIX_FULL */

/****************************************************************************
*																			*
*					Verify Constraints on a Certificate Chain				*
*																			*
****************************************************************************/

#ifdef USE_CERTLEVEL_PKIX_FULL

/* Check constraints along a certificate chain in certInfoPtr from 
   startCertIndex on down, checked if complianceLevel >= 
   CRYPT_COMPLIANCELEVEL_PKIX_FULL.  There are three types of constraints 
   that can cover multiple certificates: path constraints, name constraints, 
   and policy constraints.

   Path constraints are the easiest to check, just make sure that the number 
   of certificates from the issuer to the leaf is less than the constraint 
   length with special handling for PKIX path-kludge certificates.

   Name constraints are a bit more difficult, the abstract description
   requires building and maintaining a (potentially enormous) name 
   constraint tree which is applied to each certificate in turn as it's 
   processed.  Since name constraints are practically nonexistant and chains 
   are short it's more efficient to walk down the certificate chain when a 
   constraint is encountered and check each certificate in turn, which 
   avoids having to maintain massive amounts of state information and is no 
   less efficient than a single monolithic state comparison.  Again there's 
   special handling for PKIX path-kludge certificates, see chk_cert.c for 
   details.

   Policy constraints are the hardest of all to deal with because, with the 
   complex mishmash of policies, policy constraints, qualifiers, and mappings 
   it turns out that no-one actually knows how to apply them and even if 
   people could agree, with the de facto use of the policy extension as the 
   kitchenSink extension it's uncertain how to apply the constraints to 
   typical kitchenSink constructs.  The ambiguity of name constraints when 
   applied to altNames is bad enough, with a 50/50 split in PKIX about 
   whether it should be an AND or OR operation and whether a DN constraint 
   applies to a subjectName or altName or both.  In the absence of any 
   consensus on the issue the latter was fixed in the final version of RFC 
   2459 by somewhat arbitrarily requiring an AND rather than an OR although 
   how many implementations follow exactly this version rather than the 
   dozen earlier drafts or any other profile or interpretation is unknown.  
   With policy constraints it's even worse and no-one seems to be able to 
   agree on what to do with them, or more specifically the people who write 
   the standards don't seem to be aware that there are ambiguities and 
   inconsistencies in the handling of these extensions.  Anyone who doesn't 
   believe this is invited to try implementing the path-processing algorithm 
   in RFC 3280 as described by the pseudocode there.
   
   For example the various policy constraints in effect act as conditional 
   modifiers on the critical flag of the policies extension and/or the 
   various blah-policy-set settings in the path-processing algorithm so 
   that under various conditions imposed by the constraints the extension 
   goes from being non-critical to being (effectively) critical.  In addition 
   the constraint extensions can have their own critical flags which means 
   that we can end up having to chain back through multiple layers of 
   interacting constraint extensions spread across multiple certificates to 
   see what the current interpretation of a particular extension is.  
   Finally, the presence of PKIX path-kludge certificates can turn 
   enforcement of constraints on and off at various stages of path 
   processing with extra special cases containing exceptions to the 
   exceptions.  In addition the path-kludge exceptions apply to some 
   constraint types but not to others although the main body of the spec 
   and the pseudocode path-processing algorithm disagree on which ones and 
   when they're in effect (this implementation assumes that the body of the 
   spec is authoritative and the pseudocode represents a buggy attempt to 
   implement the spec rather than the other way round).  Since the 
   virtual-criticality can switch itself on and off across certificates 
   depending on where in the path they are, the handling of policy 
   constraints is reduced to complete chaos if we try and interpret them as 
   required by the spec - an independent evaluation of the spec that tried to 
   implement the logic using decision tables ended up with expressions of 
   more than a dozen variables (and that was at a pre-3280 stage before the
   state space explosion that occurred after that point) which indicates 
   that the issue is more or less incomprehensible.  However since it's only 
   applied at the CRYPT_COMPLIANCELEVEL_PKIX_FULL compliance level it's 
   reasonably safe since users should be expecting all sorts of wierd 
   behaviour at this level anyway. 

   The requireExplicitPolicy constraint is particularly bizarre, it 
   specifies the number of additional certificates that can be present in 
   the path before the entire path needs to have policies present.  In other 
   words unlike all other length-based constraints (pathLenConstraint, 
   inhibitPolicyMapping, inhibitAnyPolicy) this works both forwards and
   *backwards* up and down the path, making it the PKI equivalent of a COME
   FROM in that at some random point down the path a constraint placed who
   knows where can suddenly retroactively render the previously-valid path 
   invalid.  No-one seems to know why it runs backwards or what the purpose
   of the retroactive triggering after n certificates is, for now we only check
   forwards down the path in the manner of all the other length-based 
   constraints.

   Massa make big magic, gunga din */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int checkConstraints( INOUT CERT_INFO *certInfoPtr, 
							 IN_RANGE( -1, MAX_CHAINLENGTH - 1 ) \
								const int startCertIndex,
							 const ATTRIBUTE_PTR *issuerAttributes,
							 OUT_RANGE( -1, MAX_CHAINLENGTH - 1 ) \
								int *errorCertIndex, 
							 const POLICY_INFO *policyInfo,
							 const BOOLEAN explicitPolicy )
	{
	const ATTRIBUTE_PTR *nameConstraintPtr = NULL, *policyConstraintPtr = NULL;
	const ATTRIBUTE_PTR *inhibitPolicyPtr = NULL, *attributePtr;
	BOOLEAN hasExcludedSubtrees = FALSE, hasPermittedSubtrees = FALSE;
	BOOLEAN hasPolicy = FALSE, hasPathLength = FALSE;
	BOOLEAN hasExplicitPolicy = FALSE, hasInhibitPolicyMap = FALSE;
	BOOLEAN hasInhibitAnyPolicy = FALSE;
	int requireExplicitPolicyLevel, inhibitPolicyMapLevel;
	int inhibitAnyPolicyLevel;
	int pathLength = DUMMY_INIT, certIndex = startCertIndex;
	int value, iterationCount, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( issuerAttributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isWritePtr( errorCertIndex, sizeof( int ) ) );
	assert( isReadPtr( policyInfo, sizeof( POLICY_INFO ) ) );

	REQUIRES( startCertIndex >= -1 && startCertIndex < MAX_CHAINLENGTH );

	/* Clear return value */
	*errorCertIndex = CRYPT_ERROR;

	/* Check for path constraints */
	status = getAttributeFieldValue( issuerAttributes,
									 CRYPT_CERTINFO_PATHLENCONSTRAINT, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) )
		{
		pathLength = value;
		hasPathLength = TRUE;
		}

	/* Check for policy constraints */
	if( explicitPolicy && \
		checkAttributePresent( issuerAttributes, 
							   CRYPT_CERTINFO_CERTIFICATEPOLICIES ) )
		{
		/* Policy chaining purely from the presence of a policy extension
		   is only enforced if the explicit-policy option is set */
		hasPolicy = TRUE;
		}
	attributePtr = findAttribute( issuerAttributes, 
								  CRYPT_CERTINFO_POLICYCONSTRAINTS, FALSE );
	if( attributePtr != NULL )
		policyConstraintPtr = attributePtr;
	attributePtr = findAttribute( issuerAttributes, 
								  CRYPT_CERTINFO_INHIBITANYPOLICY, TRUE );
	if( attributePtr != NULL )
		inhibitPolicyPtr = attributePtr;

	/* Check for name constraints */
	attributePtr = findAttribute( issuerAttributes, 
								  CRYPT_CERTINFO_NAMECONSTRAINTS, FALSE );
	if( attributePtr != NULL )
		{
		nameConstraintPtr = attributePtr;
		hasExcludedSubtrees = \
			checkAttributeFieldPresent( nameConstraintPtr, 
										CRYPT_CERTINFO_EXCLUDEDSUBTREES );
		hasPermittedSubtrees = \
			checkAttributeFieldPresent( nameConstraintPtr, 
										CRYPT_CERTINFO_PERMITTEDSUBTREES );
		}

	/* If there aren't any critical policies or constraints present (the 
	   most common case), we're done */
	if( !hasPolicy && !hasPathLength && \
		policyConstraintPtr == NULL && inhibitPolicyPtr == NULL && \
		nameConstraintPtr == NULL )
		return( CRYPT_OK );

	/* Check whether there are requireExplicitPolicy, inhibitPolicyMapping, or 
	   inhibitAnyPolicy attributes, which act as conditional modifiers on the
	   criticality and contents of the policies extension */
	requireExplicitPolicyLevel = inhibitPolicyMapLevel = inhibitAnyPolicyLevel = 0;
	status = getAttributeFieldValue( policyConstraintPtr,
									 CRYPT_CERTINFO_REQUIREEXPLICITPOLICY, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) )
		{
		requireExplicitPolicyLevel = value;
		hasExplicitPolicy = TRUE;
		}
	status = getAttributeFieldValue( policyConstraintPtr,
									 CRYPT_CERTINFO_INHIBITPOLICYMAPPING, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) )
		{
		inhibitPolicyMapLevel = value;
		hasInhibitPolicyMap = TRUE;
		}
	if( inhibitPolicyPtr != NULL )
		{
		status = getAttributeDataValue( inhibitPolicyPtr, 
										&inhibitAnyPolicyLevel );
		if( cryptStatusError( status ) )
			return( status );
		hasInhibitAnyPolicy = TRUE;
		}
	status = CRYPT_OK;

	/* Walk down the chain checking each certificate against the issuer */
	for( certIndex = startCertIndex, iterationCount = 0;
		 cryptStatusOK( status ) && certIndex >= -1 && \
			iterationCount < MAX_CHAINLENGTH; 
		 certIndex--, iterationCount++ )
		{
		CERT_INFO *subjectCertInfoPtr;
		POLICY_TYPE policyType;
		int policyLevel;

		/* Get information for the current certificate in the chain */
		status = getCertInfo( certInfoPtr, &subjectCertInfoPtr, certIndex );
		if( cryptStatusError( status ) )
			break;

		/* Check for the presence of further policy constraints.  The path 
		   length value can only ever be decremented once set so if we find 
		   a further value for the length constraint we set the overall 
		   value to the smaller of the two */
		status = getAttributeFieldValue( subjectCertInfoPtr->attributes,
										 CRYPT_CERTINFO_REQUIREEXPLICITPOLICY, 
										 CRYPT_ATTRIBUTE_NONE, &policyLevel );
		if( cryptStatusOK( status ) )
			{
			if( !hasExplicitPolicy || policyLevel < requireExplicitPolicyLevel )
				requireExplicitPolicyLevel = policyLevel;
			hasExplicitPolicy = TRUE;
			}
		status = getAttributeFieldValue( subjectCertInfoPtr->attributes,
										 CRYPT_CERTINFO_INHIBITPOLICYMAPPING, 
										 CRYPT_ATTRIBUTE_NONE, &policyLevel );
		if( cryptStatusOK( status ) )
			{
			if( !hasInhibitPolicyMap || policyLevel < inhibitPolicyMapLevel )
				inhibitPolicyMapLevel = policyLevel;
			hasInhibitPolicyMap = TRUE;
			}
		status = getAttributeFieldValue( subjectCertInfoPtr->attributes,
										 CRYPT_CERTINFO_INHIBITANYPOLICY, 
										 CRYPT_ATTRIBUTE_NONE, &policyLevel );
		if( cryptStatusOK( status ) )
			{
			if( !hasInhibitAnyPolicy || policyLevel < inhibitAnyPolicyLevel )
				inhibitAnyPolicyLevel = policyLevel;
			hasInhibitAnyPolicy = TRUE;
			}
		status = CRYPT_OK;

		/* If any of the policy constraints have triggered then the policy 
		   extension is now treated as critical even if it wasn't before */
		if( ( hasExplicitPolicy && requireExplicitPolicyLevel <= 0 ) || \
			( hasInhibitAnyPolicy && inhibitAnyPolicyLevel <= 0 ) )
			hasPolicy = TRUE;

		/* Determine the necessary policy check type based on the various
		   policy constraints */
		policyType = POLICY_NONE;
		if( hasPolicy )
			{
			const BOOLEAN inhibitAnyPolicy = \
				( hasInhibitAnyPolicy && inhibitAnyPolicyLevel <= 0 ) ? \
				TRUE : FALSE;

			if( hasExplicitPolicy )
				{
				if( requireExplicitPolicyLevel > 0 )
					policyType = inhibitAnyPolicy ? \
								 POLICY_NONE_SPECIFIC : POLICY_NONE;
				else
				if( requireExplicitPolicyLevel == 0 )
					policyType = inhibitAnyPolicy ? \
								 POLICY_SUBJECT_SPECIFIC : POLICY_SUBJECT;
				else
				if( requireExplicitPolicyLevel < 0 )
					policyType = inhibitAnyPolicy ? \
								 POLICY_BOTH_SPECIFIC : POLICY_BOTH;
				}
			else
				policyType = inhibitAnyPolicy ? \
							 POLICY_NONE_SPECIFIC : POLICY_NONE;
			}

		/* Check that the current certificate in the chain obeys the 
		   constraints set by the overall issuer, possibly modified by other 
		   certificates in the chain */
		if( hasExcludedSubtrees )
			{
			status = checkNameConstraints( subjectCertInfoPtr,
										   nameConstraintPtr, TRUE,
										   &subjectCertInfoPtr->errorLocus, 
										   &subjectCertInfoPtr->errorType );
			}
		if( cryptStatusOK( status ) && hasPermittedSubtrees )
			{
			status = checkNameConstraints( subjectCertInfoPtr,
										   nameConstraintPtr, FALSE,
										   &subjectCertInfoPtr->errorLocus, 
										   &subjectCertInfoPtr->errorType );
			}
		if( cryptStatusOK( status ) && hasPolicy )
			{
			/* When we specify the certificate position we have to add one
			   to it because the policy level is counted from 0 (the EE 
			   certificate) to n (the root certificate) while the chain-
			   position indicator statrs from the virtual position -1 for 
			   the EE certificate that contains the chain */
			status = checkPolicyConstraints( subjectCertInfoPtr, 
											 issuerAttributes, policyType, 
											 policyInfo, certIndex + 1,
											 ( hasInhibitPolicyMap && \
											   inhibitPolicyMapLevel <= 0 ) ? \
												FALSE : TRUE,
											 &subjectCertInfoPtr->errorLocus, 
											 &subjectCertInfoPtr->errorType );
			}
		if( cryptStatusOK( status ) && hasPathLength )
			{
			status = checkPathConstraints( subjectCertInfoPtr, pathLength, 
										   &subjectCertInfoPtr->errorLocus, 
										   &subjectCertInfoPtr->errorType );
			}
		if( cryptStatusError( status ) )
			{
			/* Remember which certificate caused the problem */
			*errorCertIndex = certIndex;
			}

		/* If there are length constraints, decrement them for each 
		   certificate.  At this point we run into another piece of PKIX 
		   weirdness: If there's a path-kludge certificate present it's not 
		   counted for path-length constraint purposes but the exception 
		   only holds for path-length constraint purposes and not for 
		   require/inhibit policy constraint purposes.  This is an error in 
		   the spec, sections 4.2.1.12 (policy constraints) and 4.2.1.15 
		   (path constraints) don't permit path-kludge certificate 
		   exceptions while section 6.1.4(h) does.  On the other hand given 
		   the confusion in the pseudocode and the fact that it diverges 
		   from the body of the spec in other places as well we treat it as 
		   an error in the (non-authoritative) pseudocode rather than the 
		   (authoritative) spec.
		    
		   Unfortunately there's no easy way to tell just from looking at a 
		   certificate whether it's one of these kludge certificates or not 
		   because it looks identical to a CA root certificate (even the 
		   path-building code has to handle this speculatively, falling back 
		   to alternatives if the initial attempt to construct a path fails).

		   However, for chain-internal kludge certificates the 
		   chain-assembly code can determine whether it's a path-kludge by 
		   the presence of further certificates higher up in the chain 
		   (although it can't tell whether the chain ends in a path-kludge 
		   or a true CA root certificate because they appear identical).  In 
		   the case where the chain-assembly code has been able to identify 
		   the certificate as a path-kludge we can skip it for path length 
		   constraint purposes */
		if( hasPathLength && \
			( !( subjectCertInfoPtr->flags & CERT_FLAG_PATHKLUDGE ) ) )
			pathLength--;
		if( hasExplicitPolicy )
			requireExplicitPolicyLevel--;
		if( hasInhibitPolicyMap )
			inhibitPolicyMapLevel--;
		if( hasInhibitAnyPolicy )
			inhibitAnyPolicyLevel--;

		/* Release the certificate again unless it's the chain certificate 
		   itself, which is returned by getCertInfo() as the last 
		   certificate in the chain */
		if( certInfoPtr != subjectCertInfoPtr )
			krnlReleaseObject( subjectCertInfoPtr->objectHandle );
		}
	ENSURES( iterationCount < MAX_CHAINLENGTH );

	return( status );
	}
#endif /* USE_CERTLEVEL_PKIX_FULL */

/****************************************************************************
*																			*
*							Verify a Certificate Chain						*
*																			*
****************************************************************************/

/* Walk down a chain checking each certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCertChain( INOUT CERT_INFO *certInfoPtr )
	{
	CRYPT_CERTIFICATE iIssuerCert;
	CERT_CERT_INFO *certChainInfo = certInfoPtr->cCertCert;
	CERT_INFO *issuerCertInfoPtr, *subjectCertInfoPtr;
#ifdef USE_CERTLEVEL_PKIX_FULL
	POLICY_INFO policyInfo;
	BOOLEAN explicitPolicy = TRUE;
#endif /* USE_CERTLEVEL_PKIX_FULL */
	int certIndex, complianceLevel, iterationCount, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* Determine how much checking we need to perform */
	status = krnlSendMessage( certInfoPtr->ownerHandle, IMESSAGE_GETATTRIBUTE,
							  &complianceLevel, 
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		return( status );
#ifdef USE_CERTLEVEL_PKIX_FULL
	if( complianceLevel >= CRYPT_COMPLIANCELEVEL_PKIX_FULL )
		{
		int value;

		status = krnlSendMessage( certInfoPtr->ownerHandle, 
								  IMESSAGE_GETATTRIBUTE, &value, 
								  CRYPT_OPTION_CERT_REQUIREPOLICY );
		if( cryptStatusOK( status ) && !value )
			explicitPolicy = FALSE;
		}
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* Try and find a trust anchor for the chain */
	status = findTrustAnchor( certInfoPtr, &certIndex, &iIssuerCert );
	if( status == OK_SPECIAL )
		{
		/* The leaf is implicitly trusted, there's nothing more to do */
		return( CRYPT_OK );
		}
	if( cryptStatusError( status ) )
		return( setTrustAnchorErrorInfo( certInfoPtr ) );

	status = krnlAcquireObject( iIssuerCert, OBJECT_TYPE_CERTIFICATE, 
								( void ** ) &issuerCertInfoPtr, 
								CRYPT_ERROR_SIGNALLED );
	if( cryptStatusError( status ) )
		return( status );

	/* Add policies (both native and mapped) from the trust anchor to the 
	   policy set */
#ifdef USE_CERTLEVEL_PKIX_FULL
	status = createPolicySet( &policyInfo, issuerCertInfoPtr->attributes,
							  certInfoPtr, certIndex );
	if( cryptStatusError( status ) )
		{
		krnlReleaseObject( issuerCertInfoPtr->objectHandle );
		return( status );
		}
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* Check the trust anchor.  Since this is the start of the chain there 
	   aren't any constraints placed on it by higher-level certificates so 
	   all that we need to check at this point is the certificate itself and 
	   its signature if it's self-signed */
	if( certIndex >= certChainInfo->chainEnd )
		{
		CRYPT_ATTRIBUTE_TYPE dummyLocus;
		CRYPT_ERRTYPE_TYPE dummyType;

		/* The issuer certificate information is coming from the certificate 
		   trust database, don't modify its state when we check it */
		status = checkCertDetails( issuerCertInfoPtr, issuerCertInfoPtr, 
						( issuerCertInfoPtr->iPubkeyContext != CRYPT_ERROR ) ? \
							issuerCertInfoPtr->iPubkeyContext : CRYPT_UNUSED,
						NULL, TRUE, TRUE, &dummyLocus, &dummyType );

		}
	else
		{
		/* The issuer certificate is contained in the chain, update its state 
		   when we check it */
		status = checkCertDetails( issuerCertInfoPtr, issuerCertInfoPtr, 
						( issuerCertInfoPtr->iPubkeyContext != CRYPT_ERROR ) ? \
							issuerCertInfoPtr->iPubkeyContext : CRYPT_UNUSED,
						NULL, TRUE, TRUE, &issuerCertInfoPtr->errorLocus, 
						&issuerCertInfoPtr->errorType );
		}
	if( cryptStatusError( status ) )
		{
		krnlReleaseObject( issuerCertInfoPtr->objectHandle );
		if( certIndex < certChainInfo->chainEnd )
			{
			/* Remember which certificate caused the problem */
			certChainInfo->chainPos = certIndex;
			}
		return( status );
		}

	/* We've checked the trust anchor, move on to the next certificate */
	certIndex--;

	/* Walk down the chain from the trusted certificate checking each link 
	   in turn */
	for( iterationCount = 0;
		 cryptStatusOK( status ) && certIndex >= -1 && \
			( status = getCertInfo( certInfoPtr, &subjectCertInfoPtr,
									certIndex ) ) == CRYPT_OK && \
			iterationCount < MAX_CHAINLENGTH;
		 certIndex--, iterationCount++ )
		{
		/* Check the certificate details and signature */
		status = checkCertDetails( subjectCertInfoPtr, issuerCertInfoPtr, 
						( issuerCertInfoPtr->iPubkeyContext != CRYPT_ERROR ) ? \
							issuerCertInfoPtr->iPubkeyContext : CRYPT_UNUSED,
						NULL, FALSE, TRUE, &subjectCertInfoPtr->errorLocus, 
						&subjectCertInfoPtr->errorType );
		if( cryptStatusError( status ) )
			{
			if( cryptArgError( status ) )
				{
				/* If there's a problem with the issuer's public key we'll 
				   get a parameter error, the most appropriate standard 
				   error code that we can translate this to is a standard 
				   signature error */
				status = CRYPT_ERROR_SIGNATURE;
				}
			break;
			}

#ifdef USE_CERTLEVEL_PKIX_FULL
		/* Check any constraints that the issuer certificate may place on 
		   the rest of the chain */
		if( complianceLevel >= CRYPT_COMPLIANCELEVEL_PKIX_FULL )
			{
			int errorCertIndex = DUMMY_INIT;	/* Needed for gcc */

			status = checkConstraints( certInfoPtr, certIndex, 
									   issuerCertInfoPtr->attributes, 
									   &errorCertIndex, &policyInfo, 
									   explicitPolicy );
			if( cryptStatusError( status ) )
				{
				certIndex = errorCertIndex;
				break;
				}
			}
#endif /* USE_CERTLEVEL_PKIX_FULL */

		/* Move on to the next certificate */
		krnlReleaseObject( issuerCertInfoPtr->objectHandle );
		issuerCertInfoPtr = subjectCertInfoPtr;
		}
	ENSURES( iterationCount < MAX_CHAINLENGTH );

	/* If we stopped before we processed all of the certificates in the 
	   chain, select the one that caused the problem.  We also have to 
	   unlock the last certificate that we got to if it wasn't the leaf, 
	   which corresponds to the chain itself */
	if( cryptStatusError( status ) )
		{
		certChainInfo->chainPos = certIndex ;
		if( issuerCertInfoPtr != certInfoPtr )
			krnlReleaseObject( issuerCertInfoPtr->objectHandle );
		}

	return( status );
	}
#endif /* USE_CERTIFICATES */
