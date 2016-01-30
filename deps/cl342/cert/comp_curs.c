/****************************************************************************
*																			*
*					Manage Certificate Attribute Cursors					*
*					  Copyright Peter Gutmann 1997-2009						*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
#endif /* Compiler-specific includes */

/* GeneralNames and DNs are handled via indirect selection.  There are four
   classes of field type that cover these names:

	GNSelection	= EXCLUDEDSUBTREES | ...
	GNValue		= OTHERNAME | ... | DIRECTORYNAME
	DNSelection	= SUBJECTNAME | ISSUERNAME | DIRECTORYNAME
	DNValue		= C | O | OU | CN | ...

   Note that DIRECTORYNAME is present twice since it's both a component of a
   GeneralName and a DN in its own right.  GNSelection and DNSelection
   components merely select a composite component, the primitive elements are
   read and written via the GN and DN values.  The selection process is as
   follows:

	GNSelection --+	(default = subjectAltName)
				  |
				  v
				 GN -+----------------> non-DirectoryName field
					 |
				  +--+ DirectoryName
				  |
	DNSelection --+	(default = subjectName)
				  |
				  v
				 DN ------------------> DN field

   Selecting a component can therefore lead through a complex heirarchy of
   explicit and implicit selections, in the worst case being something like
   subjectAltName -> directoryName -> DN field.  DN and GeneralName
   components may be absent (if we're selecting it in order to create it),
   present (if we're about to read it), or can be created when accessed (if 
   we're about to write to it).  The handling is selected by the 
   SELECTION_OPTION type, if a certificate is in the high state then 
   MAY/CREATE options are implicitly converted to MUST_BE_PRESENT during the 
   selection process.

   The selection is performed as follows:

	set attribute:

	  selectionComponent:
		selectDN	subject | issuer			| MAY_BE_ABSENT
		selectGN	attributeID					| MAY_BE_ABSENT
			- Select prior to use

	  valueComponent:
		selectDN	-							| CREATE_IF_ABSENT
		selectGN	-							| CREATE_IF_ABSENT
			- To create DN/GeneralName before adding DN/GN
			  component/setting DN string

	get attribute:

	  selectionComponent:
		check		subject | issuer | other	| Presence check only
		check		attributeID
			- Return T/F if present

	  valueComponent:
		selectDN	none						| MUST_BE_PRESENT
		selectGN	none						| MUST_BE_PRESENT
			- To get DN/GeneralName component

	delete attribute:

		selectDN	subject | issuers			| MUST_BE_PRESENT
		selectGN	attributeID					| MUST_BE_PRESENT
			- To delete DN/GeneralName component

   This code is cursed */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Check whether the currently selected extension is a GeneralName */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isGeneralNameSelected( const CERT_INFO *certInfoPtr )
	{
	CRYPT_ATTRIBUTE_TYPE fieldID;
	int status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	if( certInfoPtr->attributeCursor == NULL )
		return( FALSE );
	status = getAttributeIdInfo( certInfoPtr->attributeCursor, 
								 NULL, &fieldID, NULL );
	if( cryptStatusError( status ) )
		return( FALSE );
	return( certInfoPtr->attributeCursor != NULL && \
			isGeneralNameSelectionComponent( fieldID ) ? \
			TRUE : FALSE );
	}

/* Sanity-check the selection state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckSelectionInfo( const CERT_INFO *certInfoPtr )
	{
	const SELECTION_INFO *currentSelection = &certInfoPtr->currentSelection;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* If the DN-in-extension flag is set then there must be a DN selected */
	if( currentSelection->dnPtr == NULL && currentSelection->dnInExtension )
		return( FALSE );

	/* If there's a DN selected and it's not in an extension then it must be 
	   the subject or issuer DN */
	if( currentSelection->dnPtr != NULL && \
		!currentSelection->dnInExtension && \
		currentSelection->dnPtr != &certInfoPtr->subjectName && \
		currentSelection->dnPtr != &certInfoPtr->issuerName )
		return( FALSE );

	/* If there's a GeneralName selected there can't also be a saved
	   GeneralName present */
	if( isGeneralNameSelected( certInfoPtr ) && \
		currentSelection->generalName != CRYPT_ATTRIBUTE_NONE )
		return( FALSE );

	/* The DN component count must be consistent with the DN selection 
	   state */
	if( currentSelection->dnComponent == CRYPT_ATTRIBUTE_NONE )
		{
		if( currentSelection->dnComponentCount != 0 )
			return( FALSE );
		}
	else
		{
		if( currentSelection->dnComponentCount < 0 || \
			currentSelection->dnComponentCount > MAX_INTLENGTH_SHORT )
			return( FALSE );
		}

	return( TRUE );
	}

/* Check whether there's a DN in the currently-selected GeneralName and 
   update the various selection values if we find one */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int findDnInGeneralName( INOUT CERT_INFO *certInfoPtr,
								const BOOLEAN updateCursor )
	{
	ATTRIBUTE_PTR *attributePtr;
	DN_PTR **dnPtr;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* We're inside a GeneralName, clear any possible saved selection */
	certInfoPtr->currentSelection.generalName = CRYPT_ATTRIBUTE_NONE;

	REQUIRES( sanityCheckSelectionInfo( certInfoPtr ) );

	/* Search for a DN in the current GeneralName */
	attributePtr = findDnInAttribute( certInfoPtr->attributeCursor );
	if( attributePtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* We found a DN, select it */
	status = getAttributeDataDN( attributePtr, &dnPtr );
	if( cryptStatusError( status ) )
		return( status );
	certInfoPtr->currentSelection.dnPtr = dnPtr;
	if( updateCursor )
		certInfoPtr->attributeCursor = attributePtr;
	certInfoPtr->currentSelection.dnInExtension = TRUE;

	ENSURES( sanityCheckSelectionInfo( certInfoPtr ) );

	return( CRYPT_OK );
	}

/* Reset the current DN selection state.  Note that this only resets the 
   metadata for the selection but not the selected DN itself */

STDC_NONNULL_ARG( ( 1 ) ) \
static void resetDNselection( INOUT SELECTION_INFO *selectionInfoPtr )
	{
	assert( isWritePtr( selectionInfoPtr, sizeof( SELECTION_INFO ) ) );

	selectionInfoPtr->dnInExtension = FALSE;
	selectionInfoPtr->dnComponent = CRYPT_ATTRIBUTE_NONE;
	selectionInfoPtr->dnComponentCount = 0;
	}

/* Synchronise DN/GeneralName selection information after moving the
   extension cursor */

STDC_NONNULL_ARG( ( 1 ) ) \
static void syncSelection( INOUT CERT_INFO *certInfoPtr )
	{
	SELECTION_INFO *currentSelection = &certInfoPtr->currentSelection;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* We don't apply a REQUIRES( sanityCheckSelectionInfo() ) precondition 
	   here because the purpose of syncSelection() is to restore a
	   consistent state after moving the cursor */

	/* We've moved the cursor, clear any saved GeneralName selection */
	currentSelection->generalName = CRYPT_ATTRIBUTE_NONE;

	/* I've we've moved the cursor off the GeneralName or there's no DN in
	   the GeneralName, deselect the DN */
	if( !isGeneralNameSelected( certInfoPtr ) || \
		cryptStatusError( findDnInGeneralName( certInfoPtr, FALSE ) ) )
		{
		resetDNselection( currentSelection );
		currentSelection->dnPtr = NULL;
		}

	ENSURES_V( sanityCheckSelectionInfo( certInfoPtr ) );
	}

/* Move the extension cursor to the given extension field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int moveCursorToField( INOUT CERT_INFO *certInfoPtr,
							  IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE certInfoType )
	{
	const ATTRIBUTE_PTR *attributePtr;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( sanityCheckSelectionInfo( certInfoPtr ) );
	REQUIRES( certInfoType >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			  certInfoType <= CRYPT_CERTINFO_LAST );

	/* Try and locate the given field in the extension */
	attributePtr = findAttributeField( certInfoPtr->attributes,
									   certInfoType, CRYPT_ATTRIBUTE_NONE );
	if( attributePtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* We've found the given field, update the cursor and select the DN within
	   it if it's present */
	certInfoPtr->currentSelection.updateCursor = FALSE;
	certInfoPtr->attributeCursor = ( ATTRIBUTE_PTR * ) attributePtr;
	if( isGeneralNameSelectionComponent( certInfoType ) )
		{
		/* If this is a GeneralName, select the DN within it if there's one
		   present.  Since this is peripheral to the main operation of 
		   moving the cursor we ignore the return status */
		( void ) findDnInGeneralName( certInfoPtr, FALSE );

		/* We've selected the GeneralName (possibly as a side-effect of its
		   on-demand creation), clear the saved GeneralName-to-be-created
		   value */
		certInfoPtr->currentSelection.generalName = CRYPT_ATTRIBUTE_NONE;
		}

	ENSURES( sanityCheckSelectionInfo( certInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						GeneralName Selection Routines						*
*																			*
****************************************************************************/

/* Determine whether a component which is being added to a certificate is a 
   GeneralName selection component */

CHECK_RETVAL_BOOL \
BOOLEAN isGeneralNameSelectionComponent( IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE certInfoType )
	{
	static const CRYPT_ATTRIBUTE_TYPE certGeneralNameTbl[] = {
		CRYPT_CERTINFO_AUTHORITYINFO_RTCS, 
		CRYPT_CERTINFO_AUTHORITYINFO_OCSP,
		CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS, 
		CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE,
		CRYPT_CERTINFO_AUTHORITYINFO_CRLS,
		CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY,
		CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING,
		CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY,
		CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY,
		CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST,
		CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT,
		CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR,
		CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY,
		CRYPT_CERTINFO_SUBJECTALTNAME,
		CRYPT_CERTINFO_ISSUERALTNAME,
		CRYPT_CERTINFO_ISSUINGDIST_FULLNAME,
		CRYPT_CERTINFO_CERTIFICATEISSUER,
		CRYPT_CERTINFO_PERMITTEDSUBTREES,
		CRYPT_CERTINFO_EXCLUDEDSUBTREES,
		CRYPT_CERTINFO_CRLDIST_FULLNAME,
		CRYPT_CERTINFO_CRLDIST_CRLISSUER,
		CRYPT_CERTINFO_AUTHORITY_CERTISSUER,
		CRYPT_CERTINFO_FRESHESTCRL_FULLNAME,
		CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER,
		CRYPT_CERTINFO_DELTAINFO_LOCATION,
		CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER,
		CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER,
		CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME,
		CRYPT_ATTRIBUTE_NONE, CRYPT_ATTRIBUTE_NONE 
		};
	static const CRYPT_ATTRIBUTE_TYPE cmsGeneralNameTbl[] = {
		CRYPT_CERTINFO_CMS_RECEIPT_TO,
		CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF,
		CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO,
		CRYPT_ATTRIBUTE_NONE, CRYPT_ATTRIBUTE_NONE 
		};
	const CRYPT_ATTRIBUTE_TYPE *generalNameTbl;
	int generalNameTblSize, i;

	REQUIRES_B( isAttribute( certInfoType ) || \
				isInternalAttribute( certInfoType ) );

	/* Determine which type of attribute we're dealing with */
	if( certInfoType >= CRYPT_CERTINFO_FIRST_EXTENSION && \
		certInfoType <= CRYPT_CERTINFO_LAST_EXTENSION )
		{
		generalNameTbl = certGeneralNameTbl;
		generalNameTblSize = FAILSAFE_ARRAYSIZE( certGeneralNameTbl, \
												 CRYPT_ATTRIBUTE_TYPE );
		}
	else
		{
		if( certInfoType >= CRYPT_CERTINFO_FIRST_CMS && \
			certInfoType <= CRYPT_CERTINFO_LAST_CMS )
			{
			generalNameTbl = cmsGeneralNameTbl;
			generalNameTblSize = FAILSAFE_ARRAYSIZE( cmsGeneralNameTbl, \
													 CRYPT_ATTRIBUTE_TYPE );
			}
		else
			{
			/* It's neither a certificate nor a CMS attribute extension, it
			   can't be a GeneralName */
			return( FALSE );
			}
		}

	/* Check for membership in the GeneralName set.  In theory we could 
	   divide this further via binary search but we're really reaching the 
	   law of diminishing returns here */
	for( i = 0; i < generalNameTblSize && \
				generalNameTbl[ i ] != CRYPT_ATTRIBUTE_NONE; i++ )
		{
		if( generalNameTbl[ i ] == certInfoType )
			return( TRUE );
		}
	ENSURES_B( i < generalNameTblSize );

	return( FALSE );
	}

/* Handle selection of a GeneralName or GeneralName	component in a certificate 
   extension */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int selectGeneralName( INOUT CERT_INFO *certInfoPtr,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
					   IN_ENUM( SELECTION_OPTION ) const SELECTION_OPTION option )
	{
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( ( option == MAY_BE_ABSENT && \
				isGeneralNameSelectionComponent( certInfoType ) ) || \
			  ( ( option == MUST_BE_PRESENT || option == CREATE_IF_ABSENT ) && \
				certInfoType == CRYPT_ATTRIBUTE_NONE ) );
	REQUIRES( sanityCheckSelectionInfo( certInfoPtr ) );

	/* At this point we may be trying to create or access a GeneralName for an 
	   attribute that's been disabled through configuration options.  This can 
	   happen if the access is indirect, for example by setting 
	   CRYPT_ATTRIBUTE_CURRENT to the GeneralName, which won't be blocked by
	   the kernel ACLs.  Before we can continue we verify that the attribute
	   containing the GeneralName that we want to use is actually available */
	if( option == MAY_BE_ABSENT && !checkAttributeAvailable( certInfoType ) )
		return( CRYPT_ARGERROR_VALUE );

	certInfoPtr->currentSelection.updateCursor = FALSE;

	if( option == MAY_BE_ABSENT )
		{
		/* If the selection is present, update the extension cursor and
		   exit */
		if( cryptStatusOK( moveCursorToField( certInfoPtr, certInfoType ) ) )
			return( CRYPT_OK );

		/* If the certificate is in the high state then the MAY is treated 
		   as a MUST since we can't be selecting something in order to 
		   create it later as we can for a certificate in the low state */
		if( certInfoPtr->certificate != NULL )
			return( CRYPT_ERROR_NOTFOUND );

		/* The selection isn't present, remember it for later without
		   changing any other selection information */
		certInfoPtr->currentSelection.generalName = certInfoType;
		certInfoPtr->attributeCursor = NULL;

		ENSURES( sanityCheckSelectionInfo( certInfoPtr ) );

		return( CRYPT_OK );
		}

	ENSURES( option == MUST_BE_PRESENT || option == CREATE_IF_ABSENT );

	/* If there's no saved GeneralName selection present, the extension
	   cursor must be pointing to a GeneralName */
	if( certInfoPtr->currentSelection.generalName == CRYPT_ATTRIBUTE_NONE )
		{
		if( isGeneralNameSelected( certInfoPtr ) )
			return( CRYPT_OK );

		/* If there's no GeneralName explicitly selected, try for the 
		   default subject altName */
		certInfoPtr->currentSelection.generalName = \
							CRYPT_CERTINFO_SUBJECTALTNAME;
		}

	/* Try and move the cursor to the saved GeneralName selection */
	if( cryptStatusOK( \
			moveCursorToField( certInfoPtr,
							   certInfoPtr->currentSelection.generalName ) ) )
		return( CRYPT_OK );
	if( option == MUST_BE_PRESENT )
		return( CRYPT_ERROR_NOTFOUND );

	/* We're creating the GeneralName extension, deselect the current DN and
	   remember that we have to update the extension cursor when we've done
	   it */
	resetDNselection( &certInfoPtr->currentSelection );
	certInfoPtr->currentSelection.dnPtr = NULL;
	certInfoPtr->currentSelection.updateCursor = TRUE;

	ENSURES( sanityCheckSelectionInfo( certInfoPtr ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int selectGeneralNameComponent( INOUT CERT_INFO *certInfoPtr,
								IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE certInfoType )
	{
	CRYPT_ATTRIBUTE_TYPE generalName;
	ATTRIBUTE_PTR *attributePtr;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isGeneralNameComponent( certInfoType ) );
	REQUIRES( sanityCheckSelectionInfo( certInfoPtr ) );

	/* To select a GeneralName component we first need to have a GeneralName
	   selected */
	status = selectGeneralName( certInfoPtr, CRYPT_ATTRIBUTE_NONE, 
								MUST_BE_PRESENT );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( isGeneralNameSelected( certInfoPtr ) );
			 /* Required for MUST_BE_PRESENT */

	/* We've got the required GeneralName selected, set the cursor to the 
	   field within it */
	status = getAttributeIdInfo( certInfoPtr->attributeCursor, NULL, 
								 &generalName, NULL );
	if( cryptStatusError( status ) )
		return( status );
	attributePtr = findAttributeField( certInfoPtr->attributeCursor, 
									   generalName, certInfoType );
	if( attributePtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	certInfoPtr->currentSelection.updateCursor = FALSE;
	certInfoPtr->attributeCursor = ( ATTRIBUTE_PTR * ) attributePtr;

	ENSURES( sanityCheckSelectionInfo( certInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							DN Selection Routines							*
*																			*
****************************************************************************/

/* Handle selection of DNs.  The subject and issuer DNs are somewhat special 
   in that they can be selected like any other attribute but aren't actually 
   certificate extensions, so that some things that work with attributes-in-
   extensions don't work with attributes-in-a-DN.  The problem is the depth
   of the nesting, for a DN in a GeneralName the CURRENT_GROUP is the
   extension, the CURRENT_ATTRIBUTE is the GeneralName within it, and the
   CURRENT_INSTANCE is the GeneralName component within that, while for the
   standalone subject/issuer DNs there's no CURRENT_GROUP, the 
   CURRENT_ATTRIBUTE is the subject or issuer DN, and the CURRENT_INSTANCE 
   is the DN component.  This means that we can't select repeated instances
   of DN components in a GeneralName (at least not without introducing a
   fourth level of nesting), but the use of DNs in GeneralNames is 
   practically nonexistent so this shouldn't be an issue */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int selectDN( INOUT CERT_INFO *certInfoPtr, 
			  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
			  IN_ENUM( SELECTION_OPTION ) const SELECTION_OPTION option )
	{
	CRYPT_ATTRIBUTE_TYPE generalName = \
							certInfoPtr->currentSelection.generalName;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( ( option == MAY_BE_ABSENT && \
				isDNSelectionComponent( certInfoType ) ) || \
			  ( ( option == MUST_BE_PRESENT || option == CREATE_IF_ABSENT ) && \
				certInfoType == CRYPT_ATTRIBUTE_NONE ) );
	REQUIRES( sanityCheckSelectionInfo( certInfoPtr ) );

	if( option == MAY_BE_ABSENT )
		{
		/* Try and select a DN based on the supplied attribute ID */
		switch( certInfoType )
			{
			case CRYPT_CERTINFO_SUBJECTNAME:
				certInfoPtr->currentSelection.dnPtr = &certInfoPtr->subjectName;
				break;

			case CRYPT_CERTINFO_ISSUERNAME:
				certInfoPtr->currentSelection.dnPtr = &certInfoPtr->issuerName;

				/* If it's a self-signed certificate and the issuer name 
				   isn't explicitly present then it must be implicitly 
				   present as the subject name */
				if( certInfoPtr->issuerName == NULL && \
					( certInfoPtr->flags & CERT_FLAG_SELFSIGNED ) )
					certInfoPtr->currentSelection.dnPtr = &certInfoPtr->subjectName;
				break;

			default:
				retIntError();
			}

		/* We've selected a built-in DN, remember that this isn't one in an
		   (optional) extension and clear the current DN component 
		   selection.  In addition we clear the current extension cursor 
		   since we've implicitly moved it away from the extensions to the
		   non-extension space of the built-in DNs */
		resetDNselection( &certInfoPtr->currentSelection );
		certInfoPtr->attributeCursor = NULL;

		ENSURES( sanityCheckSelectionInfo( certInfoPtr ) );

		return( CRYPT_OK );
		}

	/* If there's a DN already selected, we're done */
	if( certInfoPtr->currentSelection.dnPtr != NULL )
		return( CRYPT_OK );

	ENSURES( option == MUST_BE_PRESENT || option == CREATE_IF_ABSENT );

	/* To select a DN in a GeneralName we first need to have a GeneralName
	   selected */
	status = selectGeneralName( certInfoPtr, CRYPT_ATTRIBUTE_NONE, option );
	if( cryptStatusError( status ) )
		return( status );

	/* If we've now got a GeneralName selected, try and find a DN in it.  
	   The reason why we have to perform the explicit check is because if
	   the CREATE_IF_ABSENT option is used then the GeneralName has been
	   marked for creation when a field within it is added but won't 
	   actually be created until the field is added further down */
	if( isGeneralNameSelected( certInfoPtr ) )
		{
		/* If there's a DN currently selected, we're done */
		if( checkAttributeProperty( certInfoPtr->attributeCursor, \
									ATTRIBUTE_PROPERTY_DN ) )
			{
			DN_PTR **dnPtr;

			status = getAttributeDataDN( certInfoPtr->attributeCursor,
										 &dnPtr );
			if( cryptStatusError( status ) )
				return( status );
			resetDNselection( &certInfoPtr->currentSelection );
			certInfoPtr->currentSelection.dnPtr = dnPtr;
			certInfoPtr->currentSelection.dnInExtension = TRUE;

			ENSURES( sanityCheckSelectionInfo( certInfoPtr ) );

			return( CRYPT_OK );
			}

		/* There's no DN selected, see if there's one present somewhere in
		   the extension */
		if( cryptStatusOK( findDnInGeneralName( certInfoPtr, TRUE ) ) )
			return( CRYPT_OK );

		/* If there's no DN present and we're not about to create one,
		   exit */
		if( option == MUST_BE_PRESENT )
			return( CRYPT_ERROR_NOTFOUND );

		/* Create the DN in the currently selected GeneralName */
		status = getAttributeIdInfo( certInfoPtr->attributeCursor, NULL, 
									 &generalName, NULL );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* We're being asked to instantiate the DN, create the attribute field
	   that contains it */
	status = addAttributeField( &certInfoPtr->attributes, generalName,
								CRYPT_CERTINFO_DIRECTORYNAME, CRYPT_UNUSED, 
								0, &certInfoPtr->errorLocus, 
								&certInfoPtr->errorType );
	if( cryptStatusError( status ) )
		return( status );

	/* Find the field that we've just created.  This is a newly-created
	   attribute so it's the only one present (i.e we don't have to worry
	   about finding one added at the end of the sequence of identical
	   attributes) and we also know that it must be present since we've
	   just created it */
	return( selectGeneralName( certInfoPtr, generalName, MAY_BE_ABSENT ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int selectDNComponent( INOUT CERT_INFO *certInfoPtr,
							  IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE certInfoType )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isDNComponent( certInfoType ) );
	REQUIRES( sanityCheckSelectionInfo( certInfoPtr ) );

	/* To select a DN component we first need to have a DN selected */
	status = selectDN( certInfoPtr, CRYPT_ATTRIBUTE_NONE, MUST_BE_PRESENT );
	if( cryptStatusError( status ) )
		return( status );

	/* Remember the currently selected DN component */
	certInfoPtr->currentSelection.dnComponent = certInfoType;
	certInfoPtr->currentSelection.dnComponentCount = 0;

	ENSURES( sanityCheckSelectionInfo( certInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Certificate Cursor Movement Routines					*
*																			*
****************************************************************************/

/* Set certificate cursor information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setCursorCertChain( INOUT CERT_INFO *certInfoPtr, 
							   IN_RANGE( CRYPT_CURSOR_LAST, \
										 CRYPT_CURSOR_FIRST ) /* Values are -ve */
									const int cursorMoveType )
	{
	CERT_CERT_INFO *certChainInfo = certInfoPtr->cCertCert;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );
	REQUIRES( cursorMoveType >= CRYPT_CURSOR_LAST && \
			  cursorMoveType <= CRYPT_CURSOR_FIRST );	/* Values are -ve */

	switch( cursorMoveType )
		{
		case CRYPT_CURSOR_FIRST:
			/* Set the chain position to -1 (= CRYPT_ERROR) to indicate that 
			   it's at the leaf certificate, which is logically at position 
			   -1 in the chain */
			certChainInfo->chainPos = CRYPT_ERROR;
			break;

		case CRYPT_CURSOR_PREVIOUS:
			/* Adjust the chain position.  Note that the value can go to -1 
			   (= CRYPT_ERROR) to indicate that it's at the leaf certificate, 
			   which is logically at position -1 in the chain */
			if( certChainInfo->chainPos < 0 )
				return( CRYPT_ERROR_NOTFOUND );
			certChainInfo->chainPos--;
			break;

		case CRYPT_CURSOR_NEXT:
			if( certChainInfo->chainPos >= certChainInfo->chainEnd - 1 )
				return( CRYPT_ERROR_NOTFOUND );
			certChainInfo->chainPos++;
			break;

		case CRYPT_CURSOR_LAST:
			certChainInfo->chainPos = certChainInfo->chainEnd - 1;
			break;

		default:
			return( CRYPT_ARGERROR_NUM1 );
		}

	return( CRYPT_OK );
	}

#ifdef USE_CERTVAL

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setCursorValInfo( INOUT CERT_INFO *certInfoPtr, 
							 IN_RANGE( CRYPT_CURSOR_LAST, \
									   CRYPT_CURSOR_FIRST ) /* Values are -ve */
								const int cursorMoveType )
	{
	CERT_VAL_INFO *certValInfo = certInfoPtr->cCertVal;
	VALIDITY_INFO *valInfo = certValInfo->validityInfo;
	int iterationCount;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST || \
			  certInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE );
	REQUIRES( cursorMoveType >= CRYPT_CURSOR_LAST && \
			  cursorMoveType <= CRYPT_CURSOR_FIRST );	/* Values are -ve */

	switch( cursorMoveType )
		{
		case CRYPT_CURSOR_FIRST:
			certValInfo->currentValidity = certValInfo->validityInfo;
			if( certValInfo->currentValidity == NULL )
				return( CRYPT_ERROR_NOTFOUND );
			break;

		case CRYPT_CURSOR_PREVIOUS:
			if( valInfo == NULL || \
				certValInfo->currentValidity == NULL || \
				valInfo == certValInfo->currentValidity )
				{
				/* No validity information or we're already at the start of 
				   the list */
				return( CRYPT_ERROR_NOTFOUND );
				}

			/* Find the previous element in the list */
			for( iterationCount = 0;
				 valInfo != NULL && \
					valInfo->next != certValInfo->currentValidity && \
					iterationCount < FAILSAFE_ITERATIONS_LARGE;
				 valInfo = valInfo->next, iterationCount++ );
			ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
			certValInfo->currentValidity = valInfo;
			break;

		case CRYPT_CURSOR_NEXT:
			if( certValInfo->currentValidity == NULL || \
				certValInfo->currentValidity->next == NULL )
				return( CRYPT_ERROR_NOTFOUND );
			certValInfo->currentValidity = certValInfo->currentValidity->next;
			break;

		case CRYPT_CURSOR_LAST:
			if( valInfo == NULL )
				{
				/* No validity information present */
				return( CRYPT_ERROR_NOTFOUND );
				}

			/* Go to the end of the list */
			for( iterationCount = 0;
				 valInfo->next != NULL && \
					iterationCount < FAILSAFE_ITERATIONS_LARGE;
				 valInfo = valInfo->next, iterationCount++ );
			ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
			certValInfo->currentValidity = valInfo;
			break;

		default:
			return( CRYPT_ARGERROR_NUM1 );
		}

	return( CRYPT_OK );
	}
#endif /* USE_CERTVAL */

#ifdef USE_CERTREV

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setCursorRevInfo( INOUT CERT_INFO *certInfoPtr, 
							 IN_RANGE( CRYPT_CURSOR_LAST, \
									   CRYPT_CURSOR_FIRST ) /* Values are -ve */
								const int cursorMoveType )
	{
	CERT_REV_INFO *certRevInfo = certInfoPtr->cCertRev;
	REVOCATION_INFO *revInfo = certRevInfo->revocations;
	int iterationCount;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CRL || \
			  certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST || \
			  certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE );
	REQUIRES( cursorMoveType >= CRYPT_CURSOR_LAST && \
			  cursorMoveType <= CRYPT_CURSOR_FIRST );	/* Values are -ve */

	switch( cursorMoveType )
		{
		case CRYPT_CURSOR_FIRST:
			certRevInfo->currentRevocation = certRevInfo->revocations;
			if( certRevInfo->currentRevocation == NULL )
				return( CRYPT_ERROR_NOTFOUND );
			break;

		case CRYPT_CURSOR_PREVIOUS:
			if( revInfo == NULL || \
				certRevInfo->currentRevocation == NULL || \
				revInfo == certRevInfo->currentRevocation )
				{
				/* No revocations or we're already at the start of the 
				   list */
				return( CRYPT_ERROR_NOTFOUND );
				}

			/* Find the previous element in the list.  We use  
			   FAILSAFE_ITERATIONS_MAX as the bound because CRLs can become 
			   enormous */
			for( iterationCount = 0;
				 revInfo != NULL && \
					revInfo->next != certRevInfo->currentRevocation && \
					iterationCount < FAILSAFE_ITERATIONS_MAX;
				 revInfo = revInfo->next, iterationCount++ );
			ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );
			certRevInfo->currentRevocation = revInfo;
			break;

		case CRYPT_CURSOR_NEXT:
			if( certRevInfo->currentRevocation == NULL || \
				certRevInfo->currentRevocation->next == NULL )
				return( CRYPT_ERROR_NOTFOUND );
			certRevInfo->currentRevocation = certRevInfo->currentRevocation->next;
			break;

		case CRYPT_CURSOR_LAST:
			if( revInfo == NULL )
				{
				/* No revocations present */
				return( CRYPT_ERROR_NOTFOUND );
				}

			/* Go to the end of the list.  We use FAILSAFE_ITERATIONS_MAX as 
			   the bound because CRLs can become enormous */
			for( iterationCount = 0;
				 revInfo->next != NULL && \
					iterationCount < FAILSAFE_ITERATIONS_MAX;
				revInfo = revInfo->next, iterationCount++ );
			ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );
			certRevInfo->currentRevocation = revInfo;
			break;

		default:
			return( CRYPT_ARGERROR_NUM1 );
		}

	return( CRYPT_OK );
	}
#endif /* USE_CERTREV */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setCertificateCursor( INOUT CERT_INFO *certInfoPtr, 
						  IN_RANGE( CRYPT_CURSOR_LAST, \
									CRYPT_CURSOR_FIRST ) /* Values are -ve */
								const int cursorMoveType )
	{
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( cursorMoveType >= CRYPT_CURSOR_LAST && \
			  cursorMoveType <= CRYPT_CURSOR_FIRST );	/* Values are -ve */

	/* If it's a single certificate, there's nothing to do.  See the 
	   CRYPT_CERTINFO_CURRENT_CERTIFICATE ACL comment for why we 
	   (apparently) allow cursor movement movement in single certificates */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE )
		{
		REQUIRES( certInfoPtr->cCertCert->chainEnd == 0 );

		return( ( cursorMoveType == CRYPT_CURSOR_FIRST || \
				  cursorMoveType == CRYPT_CURSOR_LAST ) ? \
				CRYPT_OK : CRYPT_ERROR_NOTFOUND );
		}
			
	/* Move the cursor in an object-specific manner */
	switch( certInfoPtr->type )
		{
		case CRYPT_CERTTYPE_CERTCHAIN:
			return( setCursorCertChain( certInfoPtr, cursorMoveType ) );

#ifdef USE_CERTVAL
		case CRYPT_CERTTYPE_RTCS_REQUEST:
		case CRYPT_CERTTYPE_RTCS_RESPONSE:
			return( setCursorValInfo( certInfoPtr, cursorMoveType ) );
#endif /* USE_CERTVAL */

#ifdef USE_CERTREV
		case CRYPT_CERTTYPE_CRL:
		case CRYPT_CERTTYPE_OCSP_REQUEST:
		case CRYPT_CERTTYPE_OCSP_RESPONSE:
			return( setCursorRevInfo( certInfoPtr, cursorMoveType ) );
#endif /* USE_CERTREV */
		}

	retIntError();
	}

/****************************************************************************
*																			*
*					Attribute Cursor Movement Routines						*
*																			*
****************************************************************************/

/* Set attribute cursor information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setAttributeCursorDN( INOUT SELECTION_INFO *currentSelection,
								 IN const int moveType )
	{
	const DN_PTR *dnComponentList = *currentSelection->dnPtr;
	int count = 0, iterationCount;

	assert( isWritePtr( currentSelection, sizeof( SELECTION_INFO ) ) );

	REQUIRES( moveType <= CRYPT_CURSOR_FIRST && \
			  moveType >= CRYPT_CURSOR_LAST );

	switch( moveType )
		{
		case CRYPT_CURSOR_FIRST:
			/* Select the first instance of this attribute */
			currentSelection->dnComponentCount = 0;
			break;

		case CRYPT_CURSOR_PREVIOUS:
			/* Adjust the instance selection value */
			if( currentSelection->dnComponentCount <= 0 )
				return( CRYPT_ERROR_NOTFOUND );
			currentSelection->dnComponentCount--;
			break;

		case CRYPT_CURSOR_NEXT:
		case CRYPT_CURSOR_LAST:
			/* Find the number of occurrences of the DN component that we're 
			   enumerating and use that to move the cursor, which is 
			   actually just an iteration count of the number of components 
			   to skip */
			for( iterationCount = 0; 
				 iterationCount < FAILSAFE_ITERATIONS_MED;
				 iterationCount++ )
				{
				int dummy;

				if( cryptStatusError( \
						getDNComponentValue( dnComponentList, 
											 currentSelection->dnComponent,
											 count + 1, NULL, 0, &dummy ) ) )
					break;
				count++;
				}
			ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
			if( moveType == CRYPT_CURSOR_LAST )
				currentSelection->dnComponentCount = count;
			else
				{
				if( currentSelection->dnComponentCount >= count )
					return( CRYPT_ERROR_NOTFOUND );
				currentSelection->dnComponentCount++;
				}
			break;
			
		default:
			retIntError();
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setAttributeCursorRelative( INOUT CERT_INFO *certInfoPtr,
									   IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE certInfoType,
									   IN const int value )
	{
	ATTRIBUTE_PTR *attributeCursor;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP || \
			  certInfoType == CRYPT_ATTRIBUTE_CURRENT || \
			  certInfoType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE );
	REQUIRES( ( value <= CRYPT_CURSOR_FIRST && \
				value >= CRYPT_CURSOR_LAST ) || \
			  ( value >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				value <= CRYPT_CERTINFO_LAST_EXTENSION ) || \
			  ( certInfoType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE && \
				( isDNComponent( value ) || \
				  isGeneralNameComponent( value ) ) ) );
			  /* See comment in setCursorInfo for the odd CRYPT_CURSOR_xxx 
			     comparison */

	/* If we're moving to a field in an extension and there's a saved 
	   GeneralName selection present (which means that it's for a 
	   GeneralName that's not present yet but has been tagged for creation 
	   the next time that an attribute is added) then we can't move to a 
	   field in it since it hasn't been created yet */
	if( certInfoType != CRYPT_ATTRIBUTE_CURRENT_GROUP && \
		certInfoPtr->currentSelection.generalName != CRYPT_ATTRIBUTE_NONE )
		return( CRYPT_ERROR_NOTFOUND );

	/* The subject and issuer DNs aren't standard certificate extensions but 
	   (for cursor-positioning purposes) can be manipulated as such by moving
	   from one instance (e.g. one OU) to the next.  If there's no current 
	   attribute selected but there is a subject or issuer DN and a component 
	   within that DN selected (performing this DN selection inmplicitly de-
	   selects any attribute, otherwise the selected DN could be one that's
	   within a GeneralName in an attribute), then we allow a pseudo-move 
	   to/from identical DN components */
	if( certInfoType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE && \
		certInfoPtr->attributeCursor == NULL && \
		certInfoPtr->currentSelection.dnPtr != NULL && \
		certInfoPtr->currentSelection.dnComponent != CRYPT_ATTRIBUTE_NONE )
		{
		return( setAttributeCursorDN( &certInfoPtr->currentSelection, 
									  value ) );
		}

	/* If it's an absolute positioning code, pre-set the attribute cursor 
	   if required */
	if( value == CRYPT_CURSOR_FIRST || value == CRYPT_CURSOR_LAST )
		{
		if( certInfoPtr->attributes == NULL )
			return( CRYPT_ERROR_NOTFOUND );

		/* It's a full-attribute positioning code, reset the attribute 
		   cursor to the start of the list before we try to move it */
		if( certInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP )
			certInfoPtr->attributeCursor = certInfoPtr->attributes;
		else
			{
			/* It's a field or component positioning code, initialise the 
			   attribute cursor if necessary */
			if( certInfoPtr->attributeCursor == NULL )
				certInfoPtr->attributeCursor = certInfoPtr->attributes;
			}

		/* If there are no attributes present return the appropriate error 
		   code */
		if( certInfoPtr->attributeCursor == NULL )
			{
			return( ( value == CRYPT_CURSOR_FIRST || \
					  value == CRYPT_CURSOR_LAST ) ? \
					 CRYPT_ERROR_NOTFOUND : CRYPT_ERROR_NOTINITED );
			}
		}
	else
		{
		/* It's a relative positioning code, return a not-inited error 
		   rather than a not-found error if the cursor isn't set since there 
		   may be attributes present but the cursor hasn't been initialised 
		   yet by selecting the first or last absolute attribute */
		if( certInfoPtr->attributeCursor == NULL )
			return( CRYPT_ERROR_NOTINITED );
		}

	/* Move the attribute cursor */
	attributeCursor = certMoveAttributeCursor( certInfoPtr->attributeCursor,
											   certInfoType, value );
	if( attributeCursor == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	certInfoPtr->attributeCursor = attributeCursor;
	syncSelection( certInfoPtr );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAttributeCursor( INOUT CERT_INFO *certInfoPtr,
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
						IN const int value )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP || \
			  certInfoType == CRYPT_ATTRIBUTE_CURRENT || \
			  certInfoType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE );
	REQUIRES( ( value <= CRYPT_CURSOR_FIRST && \
				value >= CRYPT_CURSOR_LAST ) || 
			  ( value >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				value <= CRYPT_CERTINFO_LAST_EXTENSION ) || 
			  ( certInfoType == CRYPT_ATTRIBUTE_CURRENT && \
				( value == CRYPT_CERTINFO_ISSUERNAME || \
				  value == CRYPT_CERTINFO_SUBJECTNAME ) ) || 
			  ( certInfoType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE && \
				( isDNComponent( value ) || \
				  isGeneralNameComponent( value ) ) ) );
			  /* See comment below for the odd CRYPT_CURSOR_xxx comparison */

	/* If the new position is specified relative to a previous position, try
	   and move to that position.  Note that the seemingly illogical
	   comparison is used because the cursor positioning codes are negative
	   values */
	if( value <= CRYPT_CURSOR_FIRST && value >= CRYPT_CURSOR_LAST )
		{
		return( setAttributeCursorRelative( certInfoPtr, certInfoType, 
											value ) );
		}

	/* It's a field in an extension, try and move to the start of the
	   extension that contains this field */
	if( certInfoType == CRYPT_ATTRIBUTE_CURRENT_GROUP )
		{
		ATTRIBUTE_PTR *attributePtr;

		attributePtr = findAttribute( certInfoPtr->attributes, value, TRUE );
		if( attributePtr == NULL )
			return( CRYPT_ERROR_NOTFOUND );
		certInfoPtr->attributeCursor = attributePtr;
		syncSelection( certInfoPtr );
		return( CRYPT_OK );
		}

	/* Beyond the standard attribute-selection values there are two special
	   cases that we have to deal with.  The subject and issuer DN aren't
	   standard attributes but can be selected as if they were in order to
	   perform extended operations on them, and GeneralName components are 
	   deeply nested enough that what's a per-instance operation for any 
	   other attribute type becomes an attribute subtype in its own right */
	ENSURES( certInfoType == CRYPT_ATTRIBUTE_CURRENT || \
			 certInfoType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE );
	ENSURES( ( value >= CRYPT_CERTINFO_FIRST_EXTENSION && \
			   value <= CRYPT_CERTINFO_LAST_EXTENSION ) || \
			 ( certInfoType == CRYPT_ATTRIBUTE_CURRENT && \
			   ( value == CRYPT_CERTINFO_ISSUERNAME || \
				 value == CRYPT_CERTINFO_SUBJECTNAME ) ) || 
			 ( certInfoType == CRYPT_ATTRIBUTE_CURRENT_INSTANCE && \
				( isDNComponent( value ) || \
				  isGeneralNameComponent( value ) ) ) );

	/* If it's a GeneralName selection or GeneralName component, locate the 
	   attribute field that it corresponds to.  Note the difference in 
	   selection options, for the GeneralName as a whole we're indicating
	   which GeneralName we want to work with, including potentially 
	   creating it when we set the first field in it while for a 
	   GeneralName component we're merely selecting a field in an already-
	   existing GeneralName.
	   
	   If the returned status is a parameter error then we have to translate 
	   it from the form { certInfoPtr, value } in selectXXX() to 
	   { certInfoPtr, attribute_cursor, value } in this function, so it goes
	   from being a CRYPT_ARGERROR_VALUE to a CRYPT_ARGERROR_NUM1 */
	if( isGeneralNameSelectionComponent( value ) )
		{
		status = selectGeneralName( certInfoPtr, value, MAY_BE_ABSENT );
		return( ( status == CRYPT_ARGERROR_VALUE ) ? \
				CRYPT_ARGERROR_NUM1 : status );
		}
	if( isGeneralNameComponent( value ) )
		{
		status = selectGeneralNameComponent( certInfoPtr, value );
		return( ( status == CRYPT_ARGERROR_VALUE ) ? \
				CRYPT_ARGERROR_NUM1 : status );
		}

	/* If it's a DN, select it.  If it's a DN component, locate the RDN that 
	   it corresponds to */
	if( value == CRYPT_CERTINFO_ISSUERNAME || \
		value == CRYPT_CERTINFO_SUBJECTNAME )
		return( selectDN( certInfoPtr, value, MAY_BE_ABSENT ) );
	if( isDNComponent( value ) )
		return( selectDNComponent( certInfoPtr, value ) );

	/* It's a standard attribute field, try and locate it */
	return( moveCursorToField( certInfoPtr, value ) );
	}
#endif /* USE_CERTIFICATES */
