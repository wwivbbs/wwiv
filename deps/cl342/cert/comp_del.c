/****************************************************************************
*																			*
*							Delete Certificate Components					*
*						Copyright Peter Gutmann 1997-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Find the head of a list of attributes for per-entry attributes, i.e. ones
   for an individual entry in an object like a CRL rather than for the CRL 
   as a whole */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
static ATTRIBUTE_PTR **getEntryAttributeListHead( const CERT_INFO *certInfoPtr )
	{
	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	switch( certInfoPtr->type )
		{
#ifdef USE_CERTVAL
		case CRYPT_CERTTYPE_RTCS_REQUEST:
		case CRYPT_CERTTYPE_RTCS_RESPONSE:
			return( &certInfoPtr->cCertVal->currentValidity->attributes );
#endif /* USE_CERTVAL */

#ifdef USE_CERTREV
		case CRYPT_CERTTYPE_CRL:
		case CRYPT_CERTTYPE_OCSP_REQUEST:
		case CRYPT_CERTTYPE_OCSP_RESPONSE:
			return( &certInfoPtr->cCertRev->currentRevocation->attributes );
#endif /* USE_CERTREV */

		default:
			retIntError_Null();
		}

	retIntError_Null();
	}

/****************************************************************************
*																			*
*							Delete a Certificate Component					*
*																			*
****************************************************************************/

/* Delete a certificate attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int deleteCertAttribute( INOUT CERT_INFO *certInfoPtr,
								IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE certInfoType )
	{
	ATTRIBUTE_PTR **attributeListHeadPtrPtr;
	ATTRIBUTE_PTR *attributePtr;
	const BOOLEAN isRevocationEntry = \
				isRevocationEntryComponent( certInfoType ) ? TRUE : FALSE;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isAttribute( certInfoType ) || \
			  isInternalAttribute( certInfoType ) );

	/* Try and find this attribute in the attribute list */
	attributePtr = findAttributeComponent( certInfoPtr, certInfoType );
	if( attributePtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* If this is a non-present field with a default value in a present 
	   attribute (so that its value can be read but the field itself isn't 
	   really there) there isn't any terribly satisfactory return code to 
	   indicate this.  Returning CRYPT_OK is wrong because the caller can 
	   keep deleting the same field over and over, and returning 
	   CRYPT_ERROR_NOTFOUND is wrong because the caller may have added the 
	   attribute at an earlier date but it was never written because it had 
	   the default value so that to the caller it appears that the field 
	   they added has been lost.  The least unexpected action is probably to 
	   return CRYPT_OK */
	if( checkAttributeProperty( attributePtr, 
								ATTRIBUTE_PROPERTY_DEFAULTVALUE ) )
		return( CRYPT_OK );

	/* If this is a complete attribute (e.g. CRYPT_CERTINFO_SUBJECTINFOACCESS
	   rather than one of its fields like 
	   CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY), delete the entire attribute */
	if( checkAttributeProperty( attributePtr,
								ATTRIBUTE_PROPERTY_COMPLETEATRIBUTE ) )
		{
		ATTRIBUTE_PTR *fieldAttributePtr;

		/* If the certificate has a fleur de lis make sure that it can't be 
		   scraped off */
		fieldAttributePtr = findAttribute( certInfoPtr->attributes,
										   certInfoType, TRUE );
		if( fieldAttributePtr != NULL && \
			checkAttributeProperty( fieldAttributePtr, 
									ATTRIBUTE_PROPERTY_LOCKED ) )
			return( CRYPT_ERROR_PERMISSION );

		/* This is an ID for an entire (constructed) attribute, delete the 
		   attribute */
		if( isRevocationEntry )
			{
			attributeListHeadPtrPtr = getEntryAttributeListHead( certInfoPtr );
			ENSURES( attributeListHeadPtrPtr != NULL );
			}
		else
			attributeListHeadPtrPtr = &certInfoPtr->attributes;
		return( deleteCompleteAttribute( attributeListHeadPtrPtr, 
										 &certInfoPtr->attributeCursor, certInfoType,
										 certInfoPtr->currentSelection.dnPtr ) );
		}

	/* If the certificate has a fleur de lis make sure that it can't be 
	   scraped off */
	if( checkAttributeProperty( attributePtr, ATTRIBUTE_PROPERTY_LOCKED ) )
		return( CRYPT_ERROR_PERMISSION );

	/* It's a single field, delete that */
	if( isRevocationEntry )
		{
		attributeListHeadPtrPtr = getEntryAttributeListHead( certInfoPtr );
		ENSURES( attributeListHeadPtrPtr != NULL );
		}
	else
		attributeListHeadPtrPtr = &certInfoPtr->attributes;
	status = deleteAttributeField( attributeListHeadPtrPtr,
								   &certInfoPtr->attributeCursor, attributePtr,
								   certInfoPtr->currentSelection.dnPtr );

	/* If we've deleted the attribute containing the currently selected DN, 
	   deselect it */
	if( status == OK_SPECIAL )
		{
		certInfoPtr->currentSelection.dnPtr = NULL;
		status = CRYPT_OK;
		}

	return( status );
	}

/* Delete a certificate component */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteCertComponent( INOUT CERT_INFO *certInfoPtr,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isAttribute( certInfoType ) || \
			  isInternalAttribute( certInfoType ) );

	/* If it's a GeneralName or DN component, delete it.  These are 
	   special-case attribute values so they have to come before the 
	   general attribute-handling code */
	if( isGeneralNameSelectionComponent( certInfoType ) )
		{
		/* Check whether this GeneralName is present */
		status = selectGeneralName( certInfoPtr, certInfoType,
									MUST_BE_PRESENT );
		if( cryptStatusError( status ) )
			return( status );

		/* Delete each field in the GeneralName */
		if( deleteCompositeAttributeField( &certInfoPtr->attributes,
					&certInfoPtr->attributeCursor, certInfoPtr->attributeCursor,
					certInfoPtr->currentSelection.dnPtr ) == OK_SPECIAL )
			{
			/* We've deleted the attribute containing the currently selected 
			   DN, deselect it */
			certInfoPtr->currentSelection.dnPtr = NULL;
			}

		return( CRYPT_OK );
		}
	if( isGeneralNameComponent( certInfoType ) )
		{
		SELECTION_STATE selectionState;
		ATTRIBUTE_PTR *attributePtr DUMMY_INIT_PTR;

		/* Find the requested GeneralName component.  Since 
		   selectGeneralNameComponent() changes the current selection within 
		   the GeneralName, we save the selection state around the call */
		saveSelectionState( selectionState, certInfoPtr );
		status = selectGeneralNameComponent( certInfoPtr, certInfoType );
		if( cryptStatusOK( status ) )
			attributePtr = certInfoPtr->attributeCursor;
		restoreSelectionState( selectionState, certInfoPtr );
		if( cryptStatusError( status ))
			return( status );
		ENSURES( attributePtr != NULL );

		/* Delete the field within the GeneralName */
		if( deleteAttributeField( &certInfoPtr->attributes,
						&certInfoPtr->attributeCursor, attributePtr,
						certInfoPtr->currentSelection.dnPtr ) == OK_SPECIAL )
			{
			/* We've deleted the attribute containing the currently selected
			   DN, deselect it */
			certInfoPtr->currentSelection.dnPtr = NULL;
			}
		return( CRYPT_OK );
		}
	if( isDNComponent( certInfoType ) )
		{
		status = selectDN( certInfoPtr, CRYPT_ATTRIBUTE_NONE,
						   MUST_BE_PRESENT );
		if( cryptStatusOK( status ) )
			{
			status = deleteDNComponent( certInfoPtr->currentSelection.dnPtr,
										certInfoType, NULL, 0 );
			}
		return( status );
		}

	/* If it's standard certificate or CMS attribute, delete it */
	if( ( certInfoType >= CRYPT_CERTINFO_FIRST_EXTENSION && \
		  certInfoType <= CRYPT_CERTINFO_LAST_EXTENSION ) || \
		( certInfoType >= CRYPT_CERTINFO_FIRST_CMS && \
		  certInfoType <= CRYPT_CERTINFO_LAST_CMS ) )
		return( deleteCertAttribute( certInfoPtr, certInfoType ) );

	/* If it's anything else, handle it specially */
	switch( certInfoType )
		{
		case CRYPT_CERTINFO_SELFSIGNED:
			if( !( certInfoPtr->flags & CERT_FLAG_SELFSIGNED ) )
				return( CRYPT_ERROR_NOTFOUND );
			certInfoPtr->flags &= ~CERT_FLAG_SELFSIGNED;
			return( CRYPT_OK );

		case CRYPT_CERTINFO_CURRENT_CERTIFICATE:
		case CRYPT_ATTRIBUTE_CURRENT_GROUP:
		case CRYPT_ATTRIBUTE_CURRENT:
		case CRYPT_ATTRIBUTE_CURRENT_INSTANCE:
			if( certInfoPtr->attributeCursor == NULL )
				return( CRYPT_ERROR_NOTFOUND );
			if( certInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP )
				{
				status = deleteAttribute( &certInfoPtr->attributes,
									&certInfoPtr->attributeCursor,
									certInfoPtr->attributeCursor,
									certInfoPtr->currentSelection.dnPtr );
				}
			else
				{
				/* The current component and field are essentially the
				   same thing since a component is one of a set of
				   entries in a multivalued field, thus they're handled
				   identically */
				status = deleteAttributeField( &certInfoPtr->attributes,
									&certInfoPtr->attributeCursor,
									certInfoPtr->attributeCursor,
									certInfoPtr->currentSelection.dnPtr );
				}
			if( status == OK_SPECIAL )
				{
				/* We've deleted the attribute containing the currently 
				   selected DN, deselect it */
				certInfoPtr->currentSelection.dnPtr = NULL;
				}
			return( CRYPT_OK );

		case CRYPT_CERTINFO_TRUSTED_USAGE:
			if( certInfoPtr->cCertCert->trustedUsage == CRYPT_ERROR )
				return( CRYPT_ERROR_NOTFOUND );
			certInfoPtr->cCertCert->trustedUsage = CRYPT_ERROR;
			return( CRYPT_OK );

		case CRYPT_CERTINFO_TRUSTED_IMPLICIT:
			return( krnlSendMessage( certInfoPtr->ownerHandle,
									 IMESSAGE_USER_TRUSTMGMT,
									 &certInfoPtr->objectHandle,
									 MESSAGE_TRUSTMGMT_DELETE ) );

		case CRYPT_CERTINFO_VALIDFROM:
		case CRYPT_CERTINFO_THISUPDATE:
			if( certInfoPtr->startTime <= 0 )
				return( CRYPT_ERROR_NOTFOUND );
			certInfoPtr->startTime = 0;
			return( CRYPT_OK );

		case CRYPT_CERTINFO_VALIDTO:
		case CRYPT_CERTINFO_NEXTUPDATE:
			if( certInfoPtr->endTime <= 0 )
				return( CRYPT_ERROR_NOTFOUND );
			certInfoPtr->endTime = 0;
			return( CRYPT_OK );

		case CRYPT_CERTINFO_SUBJECTNAME:
			if( certInfoPtr->currentSelection.dnPtr == &certInfoPtr->subjectName )
				{
				/* This is the currently selected DN, deselect it before 
				   deleting it */
				certInfoPtr->currentSelection.dnPtr = NULL;
				}
			deleteDN( &certInfoPtr->subjectName );
			return( CRYPT_OK );

#ifdef USE_CERTREV
		case CRYPT_CERTINFO_REVOCATIONDATE:
			{
			time_t *revocationTimePtr = ( time_t * ) \
							getRevocationTimePtr( certInfoPtr );

			if( revocationTimePtr == NULL )
				return( CRYPT_ERROR_NOTFOUND );
			*revocationTimePtr = 0;
			return( CRYPT_OK );
			}
#endif /* USE_CERTREV */

#ifdef USE_PKIUSER
		case CRYPT_CERTINFO_PKIUSER_RA:
			if( !certInfoPtr->cCertUser->isRA )
				return( CRYPT_ERROR_NOTFOUND );
			certInfoPtr->cCertUser->isRA = FALSE;
			return( CRYPT_OK );
#endif /* USE_PKIUSER */
		}

	retIntError();
	}
#endif /* USE_CERTIFICATES */
