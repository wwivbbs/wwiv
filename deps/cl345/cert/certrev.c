/****************************************************************************
*																			*
*						Certificate Revocation Routines						*
*						Copyright Peter Gutmann 1996-2015					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

/* The maximum length of ID that can be stored in a REVOCATION_INFO entry.
   Larger IDs require external storage */

#define MAX_ID_SIZE		128

/* Usually when we add revocation information we perform various checks such
   as making sure that we're not adding duplicate information, however when
   processing the mega-CRLs from some CAs this becomes prohibitively
   expensive.  To solve this problem we perform checking up to a certain
   number of entries and after that just drop in any further entries as is
   in order to provide same-day service.  The following value defines the
   number of CRL entries at which we stop performing checks when we add new 
   entries */

#define CRL_SORT_LIMIT	1024

/* Context-specific tags for OCSP certificate identifier types */

enum { CTAG_OI_CERTIFICATE, CTAG_OI_CERTIDWITHSIG, CTAG_OI_RTCS };

/* OCSP certificate status values */

enum { OCSP_STATUS_NOTREVOKED, OCSP_STATUS_REVOKED, OCSP_STATUS_UNKNOWN };

#ifdef USE_CERTREV

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the revocation info */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckRevInfo( IN const REVOCATION_INFO *revocationInfo )
	{
	assert( isReadPtr( revocationInfo, sizeof( REVOCATION_INFO ) ) );

	if( revocationInfo == NULL )
		{
		DEBUG_PUTS(( "sanityCheckRevInfo: Missing revocation info" ));
		return( FALSE );
		}

	/* Check ID type */
	if( revocationInfo->idType != CRYPT_KEYID_NONE && \
		revocationInfo->idType != CRYPT_IKEYID_CERTID && \
		revocationInfo->idType != CRYPT_IKEYID_ISSUERANDSERIALNUMBER && \
		revocationInfo->idType != CRYPT_IKEYID_ISSUERID )
		{
		DEBUG_PUTS(( "sanityCheckRevInfo: ID type" ));
		return( FALSE );
		}

	/* Check ID data */
	if( revocationInfo->id == NULL || \
		!isShortIntegerRange( revocationInfo->idLength ) || \
		checksumData( revocationInfo->id, 
					  revocationInfo->idLength ) != revocationInfo->idCheck )
		{
		DEBUG_PUTS(( "sanityCheckRevInfo: ID" ));
		return( FALSE );
		}

	/* Check safe pointers */
	if( !DATAPTR_ISVALID( revocationInfo->attributes ) || \
		!DATAPTR_ISVALID( revocationInfo->prev ) || \
		!DATAPTR_ISVALID( revocationInfo->next ) )
		{
		DEBUG_PUTS(( "sanityCheckRevInfo: Safe pointers" ));
		return( FALSE );
		}


	return( TRUE );
	}

/****************************************************************************
*																			*
*					Add/Delete/Check Revocation Information					*
*																			*
****************************************************************************/

/* Find an entry in a revocation list.  This is done using a linear search,
   which isn't very optimal but anyone trying to do anything useful with
   mega-CRLs (or with CRLs in general) is in more trouble than basic search
   algorithm choice.  In other words it doesn't really make much difference
   whether we have an optimal or suboptimal implementation of a
   fundamentally broken mechanism like CRLs.

   The value is either a serialNumber or a hash of some form (issuerID,
   certHash), we don't bother distinguishing the exact type since the
   chances of a hash collision are virtually nonexistant */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
static int findRevocationEntry( IN const DATAPTR listHead,
								OUT_PTR_OPT REVOCATION_INFO **insertPoint,
								IN_BUFFER( valueLength ) const void *value, 
								IN_LENGTH_SHORT const int valueLength,
								const BOOLEAN sortEntries )
	{
	const REVOCATION_INFO *listPtr, *prevElement = NULL;
	const int idCheck = checksumData( value, valueLength );
	int LOOP_ITERATOR;

	assert( isWritePtr( insertPoint, sizeof( REVOCATION_INFO * ) ) );
	assert( isReadPtrDynamic( value, valueLength ) );

	REQUIRES( isShortIntegerRangeNZ( valueLength ) );
	REQUIRES( sortEntries == TRUE || sortEntries == FALSE );

	/* Clear return value */
	*insertPoint = NULL;

	/* Find the correct place in the list to insert the new element and check
	   for duplicates.  If requested we sort the entries by serial number
	   (or more generally data value) for no adequately explored reason 
	   (some implementations can optimise the searching of CRLs based on
	   this but since there's no agreement on whether to do it or not you 
	   can't tell whether it's safe to rely on it).  In addition we bound 
	   the loop with FAILSAFE_ITERATIONS_MAX rather than the more usual
	   FAILSAFE_ITERATIONS_LARGE since CRLs can grow enormous */
	LOOP_MAX( listPtr = DATAPTR_GET( listHead ),
			  listPtr != NULL, 
			  listPtr = DATAPTR_GET( listPtr->next ) )
		{
		REQUIRES( sanityCheckRevInfo( listPtr ) );

		if( ( sortEntries || idCheck == listPtr->idCheck ) && \
			listPtr->idLength == valueLength )
			{
			const int compareStatus = memcmp( listPtr->id,
											  value, valueLength );

			if( !compareStatus )
				{
				/* We found a matching entry, tell the caller which one it
				   is */
				*insertPoint = ( REVOCATION_INFO * ) listPtr;
				return( CRYPT_OK );
				}
			if( sortEntries && compareStatus > 0 )
				break;					/* Insert before this point */
			}
		else
			{
			if( sortEntries && listPtr->idLength > valueLength )
				break;					/* Insert before this point */
			}

		prevElement = listPtr;
		}
	ENSURES( LOOP_BOUND_OK );

	/* We can't find a matching entry, return the revocation entry that we 
	   should insert the new value after */
	*insertPoint = ( REVOCATION_INFO * ) prevElement;

	return( CRYPT_ERROR_NOTFOUND );
	}

/* Add an entry to a revocation list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int addRevocationEntry( INOUT_PTR DATAPTR *listHeadPtr,
						OUT_OPT_PTR_COND REVOCATION_INFO **newEntryPosition,
						IN_KEYID_OPT const CRYPT_KEYID_TYPE valueType,
						IN_BUFFER( valueLength ) const void *value, 
						IN_LENGTH_SHORT const int valueLength,
						const BOOLEAN noCheck )
	{
	DATAPTR listHead = *listHeadPtr;
	REVOCATION_INFO *newElement, *insertPoint DUMMY_INIT_PTR;

	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR ) ) );
	assert( isWritePtr( newEntryPosition, sizeof( REVOCATION_INFO * ) ) );
	assert( isReadPtrDynamic( value, valueLength ) );
	
	REQUIRES( valueType == CRYPT_KEYID_NONE || \
			  valueType == CRYPT_IKEYID_CERTID || \
			  valueType == CRYPT_IKEYID_ISSUERID || \
			  valueType == CRYPT_IKEYID_ISSUERANDSERIALNUMBER );
	REQUIRES( isShortIntegerRangeNZ( valueLength ) );
	REQUIRES( noCheck == TRUE || noCheck == FALSE );

	/* Clear return value */
	*newEntryPosition = NULL;

	/* Find the insertion point for the new entry unless we're reading data
	   from a large pre-encoded CRL (indicated by the caller setting the 
	   noCheck flag), in which case we just drop it in at the start.  The 
	   absence of checking is necessary in order to provide same-day service 
	   for large CRLs */
	if( !noCheck && DATAPTR_ISSET( listHead ) )
		{
		int status;

		status = findRevocationEntry( listHead, &insertPoint, value, 
									  valueLength, TRUE );
		if( cryptStatusOK( status ) )
			{
			/* If we get an OK status it means that we've found an existing
			   entry that matches the one being added, we can't add it 
			   again */
			return( CRYPT_ERROR_DUPLICATE );
			}
		}
	else
		{
		/* Insert the new element at the start */
		insertPoint = NULL;
		}

	/* Allocate memory for the new element and copy the information across */
	REQUIRES( rangeCheck( valueLength, 1, MAX_INTLENGTH_SHORT ) );
	if( ( newElement = ( REVOCATION_INFO * ) \
			clAlloc( "addRevocationEntry", \
					 sizeof( REVOCATION_INFO ) + valueLength ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	initVarStruct( newElement, REVOCATION_INFO, valueLength, id );
	newElement->idType = valueType;
	memcpy( newElement->id, value, valueLength );
	newElement->idLength = valueLength;
	newElement->idCheck = checksumData( value, valueLength );
	DATAPTR_SET( newElement->attributes, NULL );
	DATAPTR_SET( newElement->prev, NULL );
	DATAPTR_SET( newElement->next, NULL );
	REQUIRES( sanityCheckRevInfo( newElement ) );

	/* Insert the new element into the list */
	insertDoubleListElement( listHeadPtr, insertPoint, newElement, 
							 REVOCATION_INFO );
	*newEntryPosition = newElement;

	return( CRYPT_OK );
	}

/* Delete a revocation list */

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteRevocationEntries( INOUT_PTR DATAPTR *listHeadPtr )
	{
	DATAPTR listHead = *listHeadPtr;
	REVOCATION_INFO *revocationListCursor;
	int LOOP_ITERATOR;

	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR ) ) );

	/* If the list was empty, return now */
	if( DATAPTR_ISNULL( listHead ) )
		return;

	/* Destroy any remaining list items */
	LOOP_MAX_INITCHECK( revocationListCursor = DATAPTR_GET( listHead ),
						revocationListCursor != NULL )
		{
		REVOCATION_INFO *itemToFree = revocationListCursor;

		REQUIRES_V( sanityCheckRevInfo( itemToFree ) );

		revocationListCursor = DATAPTR_GET( revocationListCursor->next );
		if( DATAPTR_ISSET( itemToFree->attributes ) )
			deleteAttributes( &itemToFree->attributes );
		zeroise( itemToFree, sizeof( REVOCATION_INFO ) );
		clFree( "deleteRevocationEntries", itemToFree );
		}
	ENSURES_V( LOOP_BOUND_OK );

	DATAPTR_SET_PTR( listHeadPtr, NULL );
	}

/* Prepare the entries in a revocation list prior to encoding them */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5, 6 ) ) \
int prepareRevocationEntries( IN_DATAPTR_OPT const DATAPTR listHead, 
							  const time_t defaultTime,
							  OUT_PTR_xCOND REVOCATION_INFO **errorEntry,
							  const BOOLEAN isSingleEntry,
							  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType )
	{
	REVOCATION_INFO *revocationEntry;
	const time_t currentTime = ( defaultTime > MIN_TIME_VALUE ) ? \
							   defaultTime : getApproxTime();
	int value, LOOP_ITERATOR, status;

	assert( isWritePtr( errorEntry, sizeof( REVOCATION_INFO * ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISVALID( listHead ) );
	REQUIRES( isSingleEntry == TRUE || isSingleEntry == FALSE );

	/* Clear return values */
	*errorEntry = NULL;
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* If the revocation list is empty there's nothing to do */
	if( DATAPTR_ISNULL( listHead ) )
		return( CRYPT_OK );

	/* Set the revocation time if this hasn't already been set.  If there's a
	   default time set we use that otherwise we use the current time */
	LOOP_LARGE( revocationEntry = DATAPTR_GET( listHead ), 
				revocationEntry != NULL,
				revocationEntry = DATAPTR_GET( revocationEntry->next ) )
		{
		REQUIRES( sanityCheckRevInfo( revocationEntry ) );

		if( revocationEntry->revocationTime <= MIN_TIME_VALUE )
			revocationEntry->revocationTime = currentTime;

		/* Check whether the certificate was revoked with a reason of 
		   neverValid, which requires special handling of dates because 
		   X.509 doesn't formally define a neverValid reason, assuming that 
		   all CAs are perfect and never issue certificates in error.  The 
		   general idea is to set the two to the same value with the 
		   invalidity date (which should be earlier than the revocation date, 
		   at least in a sanely-run CA) taking precedence.  A revocation 
		   with this reason code will in general only be issued by the 
		   cryptlib CA (where it's required to handle problems in the CMP 
		   protocol) and this always sets the invalidity date so in almost 
		   all cases we'll be setting the revocation date to the 
		   (CA-specified) invalidity date, which is the date of issue of the 
		   certificate being revoked */
		status = getAttributeFieldValue( revocationEntry->attributes,
										 CRYPT_CERTINFO_CRLREASON,
										 CRYPT_ATTRIBUTE_NONE, &value );
		if( cryptStatusOK( status ) && value == CRYPT_CRLREASON_NEVERVALID )
			{
			time_t invalidityDate;

			/* The certificate was revoked with the neverValid code, see if 
			   there's an invalidity date present */
			status = getAttributeFieldTime( revocationEntry->attributes,
											CRYPT_CERTINFO_INVALIDITYDATE,
											CRYPT_ATTRIBUTE_NONE, 
											&invalidityDate );
			if( cryptStatusError( status ) )
				{
				/* There's no invalidity date present, set it to the same as
				   the revocation date */
				status = addAttributeFieldString( &revocationEntry->attributes,
												  CRYPT_CERTINFO_INVALIDITYDATE,
												  CRYPT_ATTRIBUTE_NONE,
												  &revocationEntry->revocationTime,
												  sizeof( time_t ), 0,
												  errorLocus, errorType );
				if( cryptStatusError( status ) )
					{
					/* Remember the entry that caused the problem */
					*errorEntry = revocationEntry;
					return( status );
					}
				}
			else
				{
				/* There's an invalidity date present, make sure that the 
				   revocation date is the same as the invalidity date */
				revocationEntry->revocationTime = invalidityDate;
				}
			}

		/* If we're only processing a single CRL entry rather than an 
		   entire revocation list we're done */
		if( isSingleEntry )
			break;
		}
	ENSURES( LOOP_BOUND_OK );

	/* Check the attributes for each entry in a revocation list */
	LOOP_LARGE( revocationEntry = DATAPTR_GET( listHead ), 
				revocationEntry != NULL,
				revocationEntry = DATAPTR_GET( revocationEntry->next ) )
		{
		REQUIRES( sanityCheckRevInfo( revocationEntry ) );

		if( DATAPTR_ISSET( revocationEntry->attributes ) )
			{
			status = checkAttributes( ATTRIBUTE_CERTIFICATE,
									  revocationEntry->attributes,
									  errorLocus, errorType );
			if( cryptStatusError( status ) )
				{
				/* Remember the entry that caused the problem */
				*errorEntry = revocationEntry;
				return( status );
				}
			}

		/* If we're only processing a single CRL entry rather than an 
		   entire revocation list we're done */
		if( isSingleEntry )
			break;
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Revocation-Checking Functions						*
*																			*
****************************************************************************/

/* Check whether a certificate has been revoked */

typedef CHECK_RETVAL int ( *CHECKREVOCATIONFUNCTION ) \
							( const CERT_INFO *certInfoPtr, \
							  INOUT CERT_INFO *revocationInfoPtr ) \
								STDC_NONNULL_ARG( ( 1, 2 ) );

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkRevocationCRL( IN const CERT_INFO *certInfoPtr, 
							   INOUT CERT_INFO *revocationInfoPtr )
	{
	CERT_REV_INFO *certRevInfo = revocationInfoPtr->cCertRev;
	REVOCATION_INFO *revocationEntry;
	int status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( revocationInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( revocationInfoPtr->type == CRYPT_CERTTYPE_CRL );

	/* If there's no revocation information present then the certificate 
	   can't have been revoked */
	if( DATAPTR_ISNULL( certRevInfo->revocations ) )
		return( CRYPT_OK );

	/* If the issuers differ then the certificate can't be in this CRL */
	if( ( revocationInfoPtr->issuerDNsize != certInfoPtr->issuerDNsize || \
		memcmp( revocationInfoPtr->issuerDNptr, certInfoPtr->issuerDNptr,
				revocationInfoPtr->issuerDNsize ) ) )
		return( CRYPT_OK );

	/* Check whether there's an entry for this certificate in the list */
	status = findRevocationEntry( certRevInfo->revocations, &revocationEntry,
								  certInfoPtr->cCertCert->serialNumber,
								  certInfoPtr->cCertCert->serialNumberLength,
								  FALSE );
	if( cryptStatusError( status ) )
		{
		/* No CRL entry, the certificate is OK */
		return( CRYPT_OK );
		}
	ENSURES( revocationEntry != NULL );
	REQUIRES( sanityCheckRevInfo( revocationEntry ) );

	/* Select the entry that contains the revocation information and return
	   the certificate's status */
	DATAPTR_SET( certRevInfo->currentRevocation, revocationEntry );

	return( CRYPT_ERROR_INVALID );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkRevocationOCSP( IN const CERT_INFO *certInfoPtr, 
							    INOUT CERT_INFO *revocationInfoPtr )
	{
	CERT_REV_INFO *certRevInfo = revocationInfoPtr->cCertRev;
	REVOCATION_INFO *revocationEntry;
	int status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( revocationInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( revocationInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE );

	/* If there's no revocation information present then the certificate 
	   can't have been revoked */
	if( DATAPTR_ISNULL( certRevInfo->revocations ) )
		return( CRYPT_OK );

	/* Check whether there's an entry for this certificate in the list */
	status = findRevocationEntry( certRevInfo->revocations, &revocationEntry,
								  certInfoPtr->cCertCert->serialNumber,
								  certInfoPtr->cCertCert->serialNumberLength,
								  FALSE );
	if( cryptStatusError( status ) )
		{
		/* No revocation entry, the certificate is OK */
		return( CRYPT_OK );
		}
	ENSURES( revocationEntry != NULL );
	REQUIRES( sanityCheckRevInfo( revocationEntry ) );

	/* Select the entry that contains the revocation information and return
	   the certificate's revocation status.  Because of the inability of a
	   blacklist to return a proper status, we have to map anything other
	   than "revoked" to "OK" */
	DATAPTR_SET( certRevInfo->currentRevocation, revocationEntry );
	return( ( revocationEntry->status == CRYPT_OCSPSTATUS_REVOKED ) ? \
			CRYPT_ERROR_INVALID : CRYPT_OK );
	}

/* Check a certificate against a CRL or OCSP response (effectively the same 
   thing, an OCSP response is just a custom CRL) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkCertAgainstCRL( INOUT CERT_INFO *certInfoPtr, 
								INOUT CERT_INFO *crlInfoPtr )
	{
	CHECKREVOCATIONFUNCTION checkRevocationFunction;
	int i, LOOP_ITERATOR, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( crlInfoPtr, sizeof( CERT_INFO ) ) );

	/* Check that the CRL/OCSP response is a complete signed CRL/response 
	   and not just a newly-created object */
	if( crlInfoPtr->certificate == NULL )
		return( CRYPT_ERROR_NOTINITED );

	checkRevocationFunction = ( crlInfoPtr->type == CRYPT_CERTTYPE_CRL ) ? \
							  checkRevocationCRL : checkRevocationOCSP;

	/* Check the base certificate against the CRL/OCSP response.  If it's 
	   been revoked or there's only a single certificate present, exit */
	status = checkRevocationFunction( certInfoPtr, crlInfoPtr );
	if( cryptStatusError( status ) || \
		certInfoPtr->type != CRYPT_CERTTYPE_CERTCHAIN )
		return( status );

	/* It's a certificate chain, check every remaining certificate in the 
	   chain against the CRL/OCSP response.  In theory this is pointless 
	   because a CRL can only contain information for a single certificate 
	   in the chain, however the caller may have passed us a CRL for an 
	   intermediate certificate (in which case the check for the leaf 
	   certificate was pointless).  In any case it's easier to just do the 
	   check for all certificates than to determine which certificate the 
	   CRL applies to so we check for all certificates */
	LOOP_EXT( i = 0, i < certInfoPtr->cCertCert->chainEnd, i++, 
			  MAX_CHAINLENGTH )
		{
		CERT_INFO *certChainInfoPtr;

		/* Check this certificate against the CRL/OCSP response */
		status = krnlAcquireObject( certInfoPtr->cCertCert->chain[ i ],
									OBJECT_TYPE_CERTIFICATE,
									( MESSAGE_PTR_CAST ) &certChainInfoPtr,
									CRYPT_ERROR_SIGNALLED );
		if( cryptStatusOK( status ) )
			{
			REQUIRES_OBJECT( sanityCheckCert( certChainInfoPtr ), 
							 certChainInfoPtr->objectHandle );
			status = checkRevocationFunction( certChainInfoPtr, crlInfoPtr );
			krnlReleaseObject( certChainInfoPtr->objectHandle );
			}

		/* If the certificate has been revoked remember which one is the 
		   revoked certificate and exit */
		if( cryptStatusError( status ) )
			{
			certInfoPtr->cCertCert->chainPos = i;
			break;
			}
		}
	ENSURES( i < MAX_CHAINLENGTH );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCRL( INOUT CERT_INFO *certInfoPtr, 
			  IN_HANDLE const CRYPT_CERTIFICATE iCryptCRL )
	{
	CERT_INFO *crlInfoPtr;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( sanityCheckCert( certInfoPtr ) );
	REQUIRES( isHandleRangeValid( iCryptCRL ) );

	/* Get the CRL/OCSP response and use it to check the certificate */
	status = krnlAcquireObject( iCryptCRL, OBJECT_TYPE_CERTIFICATE,
								( MESSAGE_PTR_CAST ) &crlInfoPtr,
								CRYPT_ARGERROR_VALUE );
	if( cryptStatusError( status ) )
		return( status );
	REQUIRES_OBJECT( sanityCheckCert( crlInfoPtr ), 
					 crlInfoPtr->objectHandle );
	status = checkCertAgainstCRL( certInfoPtr, crlInfoPtr );
	krnlReleaseObject( crlInfoPtr->objectHandle );
	return( status );
	}

/****************************************************************************
*																			*
*							Read/write CRL Information						*
*																			*
****************************************************************************/

/* Read CRL entries:

	RevokedCert ::= SEQUENCE {
			userCertificate		CertificalSerialNumber,
			revocationDate		UTCTime,
			extensions			Extensions OPTIONAL
			} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int readCRLentry( INOUT STREAM *stream, 
				  INOUT_PTR DATAPTR *listHeadPtr,
				  IN_LENGTH_Z const int entryNo,
				  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
					CRYPT_ATTRIBUTE_TYPE *errorLocus,
				  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
					CRYPT_ERRTYPE_TYPE *errorType )
	{
	REVOCATION_INFO *currentEntry;
	BYTE serialNumber[ MAX_SERIALNO_SIZE + 8 ];
	int serialNumberLength, endPos, length, status;
	time_t revocationTime;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( isIntegerRange( entryNo ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Determine the overall size of the entry */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;

	/* Read the integer component of the serial number (limited to a sane
	   length) and the revocation time */
	readInteger( stream, serialNumber, MAX_SERIALNO_SIZE,
				 &serialNumberLength );
	status = readUTCTime( stream, &revocationTime );
	if( cryptStatusError( status ) )
		return( status );

	/* Add the entry to the revocation information list.  The ID type isn't
	   quite an issueAndSerialNumber but the checking code eventually
	   converts it into this form using the supplied issuer certificate DN */
	status = addRevocationEntry( listHeadPtr, &currentEntry,
								 CRYPT_IKEYID_ISSUERANDSERIALNUMBER,
								 serialNumber, serialNumberLength,
								 ( entryNo > CRL_SORT_LIMIT ) ? TRUE : FALSE );
	if( cryptStatusError( status ) )
		return( status );
	currentEntry->revocationTime = revocationTime;

	/* Read the extensions if there are any present.  Since these are per-
	   entry extensions we read the extensions themselves as
	   CRYPT_CERTTYPE_NONE rather than CRYPT_CERTTYPE_CRL to make sure
	   that they're processed as required */
	if( stell( stream ) <= endPos - MIN_ATTRIBUTE_SIZE )
		{
		status = readAttributes( stream, &currentEntry->attributes,
								 CRYPT_CERTTYPE_NONE, length,
								 errorLocus, errorType );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int readCRLentries( INOUT STREAM *stream, 
					INOUT_PTR DATAPTR *listHeadPtr,
					OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
					OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType )
	{
	long length;
	int noCrlEntries, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Read the outer wrapper.  Since CRLs can be empty (no certificates 
	   have been revoked in the time the CRL was issued), we need to use
	   readLongGenericHoleZ() rather than readLongSequence() to allow for a 
	   zero-length SEQUENCE */
	status = readLongGenericHoleZ( stream, &length, BER_SEQUENCE );
	if( cryptStatusError( status ) )
		return( status );
	if( length == CRYPT_UNUSED )
		{
		/* If it's an (invalid) indefinite-length encoding 
		   (readLongSequence() accepts those alongside long lengths so we 
		   have to check for this special-case here) then we can't do 
		   anything with it */
		return( CRYPT_ERROR_BADDATA );
		}
	if( length <= 0 )
		{
		/* It's an empty CRL */
		DATAPTR_SET_PTR( listHeadPtr, NULL );

		return( CRYPT_OK );
		}

	/* The following loop is a bit tricky to failsafe because it's actually 
	   possible to encounter real-world 100MB CRLs that the failsafe would 
	   otherwise identify as an error.  Because CRLs can range so far 
	   outside what would be considered sane we can't really bound the loop 
	   in any way except at a fairly generic maximum-integer value */
	LOOP_MAX( noCrlEntries = 0,
			  length > 0 && noCrlEntries < 10000,
			  noCrlEntries++ )
		{
		const long innerStartPos = stell( stream );

		REQUIRES( isIntegerRangeNZ( innerStartPos ) );

		status = readCRLentry( stream, listHeadPtr, noCrlEntries, 
							   errorLocus, errorType );
		if( cryptStatusError( status ) )
			return( status );
		length -= stell( stream ) - innerStartPos;
		}
	ENSURES( LOOP_BOUND_OK );
	if( noCrlEntries >= 10000 )
		{
		/* We allow longish CRLs, but one with more than ten thousand 
		   entries has something wrong with it */
		DEBUG_DIAG(( "CRL contains more than 10,000 entries" ));
		assert_nofuzz( DEBUG_WARN );
		return( CRYPT_ERROR_OVERFLOW );
		}

	return( CRYPT_OK );
	}

/* Write CRL entries */

CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1 ) ) \
static int sizeofCRLentry( INOUT REVOCATION_INFO *crlEntry )
	{
	int status;

	assert( isWritePtr( crlEntry, sizeof( REVOCATION_INFO ) ) );

	/* Remember the encoded attribute size for later when we write the
	   attributes */
	status = crlEntry->attributeSize = \
					sizeofAttributes( crlEntry->attributes,
									  CRYPT_CERTTYPE_NONE );
	if( cryptStatusError( status ) )
		return( status );

	return( sizeofShortObject( \
				sizeofInteger( crlEntry->id, crlEntry->idLength ) + \
				sizeofUTCTime() + \
				( ( crlEntry->attributeSize > 0 ) ? \
					sizeofShortObject( crlEntry->attributeSize ) : 0 ) ) );
	}

CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 2 ) ) \
int sizeofCRLentries( IN const DATAPTR crlEntries,
					  OUT_BOOL BOOLEAN *isV2CRL )
	{
	REVOCATION_INFO *revocationInfo;
	int revocationInfoLength = 0, status, LOOP_ITERATOR;

	assert( isWritePtr( isV2CRL, sizeof( BOOLEAN ) ) );

	REQUIRES( DATAPTR_ISVALID( crlEntries ) );

	/* Clear return value */
	*isV2CRL = FALSE;

	LOOP_MAX( revocationInfo = DATAPTR_GET( crlEntries ), 
			  revocationInfo != NULL,
			  revocationInfo = DATAPTR_GET( revocationInfo->next ) )
		{
		int crlEntrySize;
		
		ENSURES( sanityCheckRevInfo( revocationInfo ) );

		status = crlEntrySize = sizeofCRLentry( revocationInfo );
		if( cryptStatusError( status ) )
			return( status );
		revocationInfoLength += crlEntrySize;

		/* If there are per-entry extensions present then it's a v2 CRL */
		if( DATAPTR_ISSET( revocationInfo->attributes ) )
			*isV2CRL = TRUE;
		}
	ENSURES( LOOP_BOUND_OK );

	return( revocationInfoLength );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCRLentry( INOUT STREAM *stream, 
				   IN const REVOCATION_INFO *crlEntry )
	{
	const int revocationLength = \
				sizeofInteger( crlEntry->id, crlEntry->idLength ) + \
				sizeofUTCTime() + \
				( ( crlEntry->attributeSize > 0 ) ? \
					sizeofShortObject( crlEntry->attributeSize ) : 0 );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( crlEntry, sizeof( REVOCATION_INFO ) ) );

	REQUIRES( sanityCheckRevInfo( crlEntry ) );

	/* Write the CRL entry */
	writeSequence( stream, revocationLength );
	writeInteger( stream, crlEntry->id, crlEntry->idLength, DEFAULT_TAG );
	status = writeUTCTime( stream, crlEntry->revocationTime, DEFAULT_TAG );
	if( cryptStatusError( status ) || crlEntry->attributeSize <= 0 )
		return( status );

	/* Write the per-entry extensions.  Since these are per-entry extensions
	   rather than overall CRL extensions we write them as CRYPT_CERTTYPE_NONE 
	   rather than CRYPT_CERTTYPE_CRL to make sure that they're processed as 
	   required */
	return( writeAttributes( stream, crlEntry->attributes,
							 CRYPT_CERTTYPE_NONE, crlEntry->attributeSize ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeCRLentries( INOUT STREAM *stream, 
					 IN const DATAPTR crlEntries )
	{
	REVOCATION_INFO *revocationInfo;
	int status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Write the SEQUENCE OF revocation information */
	LOOP_MAX( revocationInfo = DATAPTR_GET( crlEntries ), 
			  revocationInfo != NULL,
			  revocationInfo = DATAPTR_GET( revocationInfo->next ) )
		{
		ENSURES( sanityCheckRevInfo( revocationInfo ) );

		status = writeCRLentry( stream, revocationInfo );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							OCSP-specific Functions							*
*																			*
****************************************************************************/

/* Copy a revocation list from an OCSP request to a response */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyRevocationEntries( INOUT_PTR DATAPTR *destListHeadPtr,
						   IN const DATAPTR srcList )
	{
	const REVOCATION_INFO *srcListCursor;
	REVOCATION_INFO *destListCursor DUMMY_INIT_PTR;
	int LOOP_ITERATOR;

	assert( isWritePtr( destListHeadPtr, sizeof( DATAPTR ) ) );

	REQUIRES( DATAPTR_ISSET( srcList ) );

	/* Copy all revocation entries from source to destination */
	LOOP_LARGE( srcListCursor = DATAPTR_GET( srcList ), 
				srcListCursor != NULL, 
				srcListCursor = DATAPTR_GET( srcListCursor->next ) )
		{
		REVOCATION_INFO *newElement;

		REQUIRES( sanityCheckRevInfo( srcListCursor ) );

		/* Allocate the new entry and copy the data from the existing one
		   across.  We don't copy the attributes because there aren't any
		   that should be carried from request to response */
		REQUIRES( rangeCheck( srcListCursor->idLength, 
							  1, MAX_INTLENGTH_SHORT ) );
		if( ( newElement = ( REVOCATION_INFO * ) \
					clAlloc( "copyRevocationEntries",
							 sizeof( REVOCATION_INFO ) + \
								srcListCursor->idLength ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		copyVarStruct( newElement, srcListCursor, REVOCATION_INFO, id );
		DATAPTR_SET( newElement->attributes, NULL );
		DATAPTR_SET( newElement->prev, NULL );
		DATAPTR_SET( newElement->next, NULL );

		/* Set the status to 'unknown' by default, this means that any
		   entries that we can't do anything with automatically get the
		   correct status associated with them */
		newElement->status = CRYPT_OCSPSTATUS_UNKNOWN;

		ENSURES( sanityCheckRevInfo( newElement ) );

		/* Link the new element into the list */
		insertDoubleListElement( destListHeadPtr, destListCursor, newElement, 
								 REVOCATION_INFO );
		destListCursor = newElement;
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

/* Check the entries in an OCSP response object against a certificate store.  
   The semantics for this one are a bit odd, the source information for the 
   check is from a request but the destination information is in a response.  
   Since we don't have a copy-and-verify function we do the checking from 
   the response even though technically it's the request data that's being 
   checked */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkOCSPResponse( INOUT CERT_INFO *certInfoPtr,
					   IN_HANDLE const CRYPT_KEYSET iCryptKeyset )
	{
	const CERT_REV_INFO *certRevInfo = certInfoPtr->cCertRev;
	REVOCATION_INFO *revocationInfo;
	BOOLEAN isRevoked = FALSE;
	int LOOP_ITERATOR;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( sanityCheckCert( certInfoPtr ) );
	REQUIRES( isHandleRangeValid( iCryptKeyset ) );

	/* Walk down the list of revocation entries fetching status information
	   on each one from the certificate store */
	LOOP_LARGE( revocationInfo = DATAPTR_GET( certRevInfo->revocations ), 
				revocationInfo != NULL, 
				revocationInfo = DATAPTR_GET( revocationInfo->next ) )
		{
		CRYPT_KEYID_TYPE idType;
		MESSAGE_KEYMGMT_INFO getkeyInfo;
		CERT_INFO *crlEntryInfoPtr;
		const BYTE *id;
		int status;

		REQUIRES( sanityCheckRevInfo( revocationInfo ) );
		REQUIRES( revocationInfo->idType == CRYPT_KEYID_NONE || \
				  revocationInfo->idType == CRYPT_IKEYID_CERTID || \
				  revocationInfo->idType == CRYPT_IKEYID_ISSUERID );

		/* If it's an OCSPv1 ID and there's no alternate ID information 
		   present we can't really do anything with it because the one-way 
		   hashing process required by the standard destroys the certificate
		   identifying information */
		if( revocationInfo->idType == CRYPT_KEYID_NONE )
			{
			if( revocationInfo->altIdType == CRYPT_KEYID_NONE )
				{
				revocationInfo->status = CRYPT_OCSPSTATUS_UNKNOWN;
				continue;
				}

			/* There's an alternate ID present, use that instead */
			idType = revocationInfo->altIdType;
			id = revocationInfo->altID;
			}
		else
			{
			/* It's a usable ID, either a certID or an issuerID */
			idType = revocationInfo->idType;
			id = revocationInfo->id;
			}

		/* Determine the revocation status of the object.  Unfortunately
		   because of the way OCSP returns status information we can't just
		   return a yes/no response but have to perform multiple queries to
		   determine whether a certificate is not revoked, revoked, or 
		   unknown.  Optimising the query strategy is complicated by the 
		   fact that although in theory the most common status will be 
		   not-revoked we could also get a large number of status-unknown 
		   queries, for example if a widely-deployed implementation which is 
		   pointed at a cryptlib-based server gets its ID-hashing wrong and 
		   submits huge numbers of queries with IDs that match no known 
		   certificate.  The best that we can do is assume that a not-
		   revoked status will be the most common and if that fails fall 
		   back to a revoked status check */
		setMessageKeymgmtInfo( &getkeyInfo, idType, id, KEYID_SIZE, 
							   NULL, 0, KEYMGMT_FLAG_CHECK_ONLY );
		status = krnlSendMessage( iCryptKeyset, IMESSAGE_KEY_GETKEY,
								  &getkeyInfo, KEYMGMT_ITEM_PUBLICKEY );
		if( cryptStatusOK( status ) )
			{
			/* The certificate is present and not revoked/OK, we're done */
			revocationInfo->status = CRYPT_OCSPSTATUS_NOTREVOKED;
			continue;
			}

		/* The certificate isn't a currently active one, if it weren't for 
		   the need to return the CRL-based OCSP status values we could just 
		   return not-OK now but as it is we have to differentiate between 
		   revoked and unknown so we perform a second query, this time of 
		   the revocation information */
		setMessageKeymgmtInfo( &getkeyInfo, idType, id, KEYID_SIZE, 
							   NULL, 0, KEYMGMT_FLAG_NONE );
		status = krnlSendMessage( iCryptKeyset, IMESSAGE_KEY_GETKEY,
								  &getkeyInfo, KEYMGMT_ITEM_REVOCATIONINFO );
		if( cryptStatusError( status ) )
			{
			/* No revocation information found, status is unknown */
			revocationInfo->status = CRYPT_OCSPSTATUS_UNKNOWN;
			continue;
			}

		/* The certificate has been revoked, copy the revocation information 
		   across from the CRL entry.  Error handling here gets a bit 
		   complicated because we're supposed to be a (relatively) reliable 
		   server, in the (highly unlikely) event that the call fails it's 
		   not clear that we should abort the entire operation just because 
		   we can't get the specific details for a single object (we already 
		   know its overall status, which is 'revoked') so we skip reporting
		   the low-level details if there's a problem and continue */
		status = krnlAcquireObject( getkeyInfo.cryptHandle,
									OBJECT_TYPE_CERTIFICATE,
									( MESSAGE_PTR_CAST ) &crlEntryInfoPtr,
									CRYPT_ERROR_SIGNALLED );
		if( cryptStatusOK( status ) )
			{
			const REVOCATION_INFO *crlRevocationInfo;

			REQUIRES_OBJECT( sanityCheckCert( crlEntryInfoPtr ),
							 crlEntryInfoPtr->objectHandle );

			crlRevocationInfo = DATAPTR_GET( certRevInfo->revocations );
			if( crlRevocationInfo != NULL )
				{
				revocationInfo->revocationTime = \
									crlRevocationInfo->revocationTime;
				if( DATAPTR_ISSET( crlRevocationInfo->attributes ) )
					{
					/* We don't check for problems in copying the attributes 
					   since bailing out at this late stage is worse than 
					   missing a few obscure annotations to the revocation */
					( void ) copyRevocationAttributes( &revocationInfo->attributes,
													   crlRevocationInfo->attributes );
					}
				}
			krnlReleaseObject( crlEntryInfoPtr->objectHandle );
			}
		krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DECREFCOUNT );

		/* Record the fact that we've seen at least one revoked certificate */
		revocationInfo->status = CRYPT_OCSPSTATUS_REVOKED;
		isRevoked = TRUE;
		}
	ENSURES( LOOP_BOUND_OK );

	/* If at least one certificate was revoked indicate this to the caller.  
	   Note that if there are multiple certificates present in the query then 
	   it's up to the caller to step through the list to find out which ones 
	   were revoked */
	return( isRevoked ? CRYPT_ERROR_INVALID : CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Read/write OCSP IDs								*
*																			*
****************************************************************************/

/* Read/write an OCSP certificate ID:

	CertID ::=	CHOICE {
		certID			SEQUENCE {
			hashAlgo	AlgorithmIdentifier,
			iNameHash	OCTET STRING,	-- Hash of issuerName
			iKeyHash	OCTET STRING,	-- Hash of issuer SPKI w/o tag+len
			serialNo	INTEGER
				},
		certificate	[0]	EXPLICIT [0] EXPLICIT Certificate,
		certIdWithSignature
					[1]	EXPLICIT SEQUENCE {
			iAndS		IssuerAndSerialNumber,
			tbsCertHash	BIT STRING,
			certSig		SEQUENCE {
				sigAlgo	AlgorithmIdentifier,
				sigVal	BIT STRING
				}
			}
		} */

CHECK_RETVAL_RANGE( 0, 1024 ) STDC_NONNULL_ARG( ( 1 ) ) \
static int sizeofOcspID( IN const REVOCATION_INFO *ocspEntry )
	{
	assert( isReadPtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );

	REQUIRES( ocspEntry->idType == CRYPT_KEYID_NONE );

	/* For now we don't try and handle anything except the v1 ID since the
	   status of v2 is uncertain (it doesn't add anything to v1 except even
	   more broken IDs) */
	return( ocspEntry->idLength );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
static int readOcspID( INOUT STREAM *stream, 
					   OUT_ENUM_OPT( CRYPT_KEYID ) CRYPT_KEYID_TYPE *idType,
					   OUT_BUFFER( idMaxLen, *idLen ) BYTE *id, 
					   IN_LENGTH_SHORT_MIN( 16 ) const int idMaxLen,
					   OUT_LENGTH_BOUNDED_Z( idMaxLen ) int *idLen )
	{
	HASH_FUNCTION_ATOMIC hashFunctionAtomic;
	void *dataPtr DUMMY_INIT_PTR;
	const int hashedIDlen = min( idMaxLen, KEYID_SIZE );
	int length DUMMY_INIT, tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( idType, sizeof( CRYPT_KEYID_TYPE ) ) );
	assert( isWritePtrDynamic( id, idMaxLen ) );
	assert( isWritePtr( idLen, sizeof( int ) ) );

	REQUIRES( idMaxLen >= 16 && idMaxLen < MAX_INTLENGTH_SHORT );

	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, NULL );

	/* Clear return values */
	*idType = CRYPT_KEYID_NONE;
	memset( id, 0, min( 16, idMaxLen ) );
	*idLen = 0;

	/* Read the ID */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
	switch( tag )
		{
		case BER_SEQUENCE:
			/* We can't really do anything with v1 IDs since the one-way
			   hashing process destroys any chance of being able to work
			   with them and the fact that no useful certificate information 
			   is hashed means that we can't use them to identify a 
			   certificate.  As a result the following ID type will always 
			   produce a result of "unknown" */
			*idType = CRYPT_KEYID_NONE;
			status = getStreamObjectLength( stream, &length );
			if( cryptStatusError( status ) )
				return( status );
			if( length < 8 )
				return( CRYPT_ERROR_UNDERFLOW );
			if( length > idMaxLen )
				return( CRYPT_ERROR_OVERFLOW );
			*idLen = length;
			return( sread( stream, id, length ) );

		case MAKE_CTAG( CTAG_OI_CERTIFICATE ):
			/* Convert the certificate to a certID */
			*idType = CRYPT_IKEYID_CERTID;
			*idLen = hashedIDlen;
			readConstructed( stream, NULL, CTAG_OI_CERTIFICATE );
			status = readConstructed( stream, &length, 0 );
			if( cryptStatusOK( status ) && length <= MIN_CRYPT_OBJECTSIZE )
				status = CRYPT_ERROR_BADDATA;
			if( cryptStatusOK( status ) )
				status = sMemGetDataBlock( stream, &dataPtr, length );
			if( cryptStatusError( status ) )
				return( status );
			ANALYSER_HINT( dataPtr != NULL );
			hashFunctionAtomic( id, hashedIDlen, dataPtr, length );
			return( readUniversal( stream ) );

		case MAKE_CTAG( CTAG_OI_CERTIDWITHSIG ):
			/* A bizarro ID dreamed up by Denis Pinkas that manages to carry
			   over all the problems of the v1 ID without being compatible
			   with it.  It's almost as unworkable as the v1 original but we 
			   can convert the iAndS to an issuerID and use that */
			*idType = CRYPT_IKEYID_ISSUERID;
			*idLen = hashedIDlen;
			readConstructed( stream, NULL, CTAG_OI_CERTIDWITHSIG );
			status = readSequence( stream, NULL );
			if( cryptStatusOK( status ) )
				status = getStreamObjectLength( stream, &length );
			if( cryptStatusOK( status ) && length <= 16 )
				status = CRYPT_ERROR_BADDATA;
			if( cryptStatusOK( status ) )
				status = sMemGetDataBlock( stream, &dataPtr, length );
			if( cryptStatusError( status ) )
				return( status );
			ANALYSER_HINT( dataPtr != NULL );
			hashFunctionAtomic( id, hashedIDlen, dataPtr, length );
			sSkip( stream, length, MAX_INTLENGTH_SHORT );	/* issuerAndSerialNumber */
			readUniversal( stream );			/* tbsCertificateHash */
			return( readUniversal( stream ) );	/* certSignature */
		}

	return( CRYPT_ERROR_BADDATA );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeOcspID( INOUT STREAM *stream, 
						IN const REVOCATION_INFO *ocspEntry )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );

	return( swrite( stream, ocspEntry->id, ocspEntry->idLength ) );
	}

/****************************************************************************
*																			*
*							Read/write OCSP Request							*
*																			*
****************************************************************************/

/* Read OCSP request entries:

	Entry ::= SEQUENCE {				-- Request
		certID			CertID,
		extensions	[0]	EXPLICIT Extensions OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readOcspRequestEntry( INOUT STREAM *stream, 
								 INOUT_PTR DATAPTR *listHeadPtr,
								 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus,
								 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	DATAPTR_ATTRIBUTE attributePtr;
	REVOCATION_INFO *currentEntry;
	STREAM certIdStream;
	BYTE idBuffer[ MAX_ID_SIZE + 8 ];
	CRYPT_KEYID_TYPE idType;
	void *certIdPtr;
	int endPos, certIdLength, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Determine the overall size of the entry */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;

	/* Read the ID information */
	status = readOcspID( stream, &idType, idBuffer, MAX_ID_SIZE, &length );
	if( cryptStatusError( status ) )
		return( status );

	/* Add the entry to the revocation information list */
	status = addRevocationEntry( listHeadPtr, &currentEntry, idType,
								 idBuffer, length, FALSE );
	if( cryptStatusError( status ) || \
		stell( stream ) > endPos - MIN_ATTRIBUTE_SIZE )
		return( status );

	/* Read the extensions.  Since these are per-entry extensions rather 
	   than overall OCSP extensions we read the wrapper here and read the 
	   extensions themselves as CRYPT_CERTTYPE_NONE rather than 
	   CRYPT_CERTTYPE_OCSP to make sure that they're processed as required.  
	   Note that these are per-request-entry extensions rather than overall 
	   per-request extensions so the tag is CTAG_OR_SR_EXTENSIONS rather 
	   than CTAG_OR_EXTENSIONS */
	status = readConstructed( stream, &length, CTAG_OR_SR_EXTENSIONS );
	if( cryptStatusOK( status ) && length > 0 )
		{
		status = readAttributes( stream, &currentEntry->attributes,
								 CRYPT_CERTTYPE_NONE, length, 
								 errorLocus, errorType );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* OCSPv1 uses a braindamaged certificate identification method that 
	   breaks the certificate information up into bits and pieces and hashes 
	   some while leaving others intact, making it impossible to identify 
	   the certificate from it.  To try and fix this, if the request 
	   includes an ESSCertID we use that to make it look like there was a 
	   proper ID present */
	if( currentEntry->idType != CRYPT_KEYID_NONE )
		return( CRYPT_OK );		/* Proper ID present, we're done */
	attributePtr = findAttribute( currentEntry->attributes, 
								  CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID, 
								  TRUE );
	if( DATAPTR_ISNULL( attributePtr ) )
		return( CRYPT_OK );		/* No ESSCertID present, can't continue */

	/* Extract the ID information from the ESSCertID and save it alongside
	   the OCSP ID, which we need to retain for use in the response:

		essCertID SEQUENCE {
			certHash			OCTET STRING SIZE(20)
			} */
	status = getAttributeDataPtr( attributePtr, &certIdPtr, &certIdLength );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( certIdPtr != NULL );
	sMemConnect( &certIdStream, certIdPtr, certIdLength );
	readSequence( &certIdStream, NULL );
	status = readOctetString( &certIdStream, idBuffer, &length, KEYID_SIZE, 
							  KEYID_SIZE );
	if( cryptStatusOK( status ) )
		{
		ENSURES( length == KEYID_SIZE );

		currentEntry->altIdType = CRYPT_IKEYID_CERTID;
		REQUIRES( rangeCheck( length, 1, KEYID_SIZE ) );
		memcpy( currentEntry->altID, idBuffer, length );
		}
	sMemDisconnect( &certIdStream );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int readOcspRequestEntries( INOUT STREAM *stream, 
							INOUT_PTR DATAPTR *listHeadPtr,
							OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType )
	{
	int length, noRequestEntries, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Read the outer wrapper */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the SEQUENCE OF request information */
	LOOP_LARGE( noRequestEntries = 0,
				length > 0 && noRequestEntries < 100,
				noRequestEntries++ )
		{
		const int innerStartPos = stell( stream );

		REQUIRES( isIntegerRangeNZ( innerStartPos ) );

		status = readOcspRequestEntry( stream, listHeadPtr,
									   errorLocus, errorType );
		if( cryptStatusError( status ) )
			return( status );
		length -= stell( stream ) - innerStartPos;
		}
	ENSURES( LOOP_BOUND_OK );
	if( noRequestEntries >= 100 )
		{
		/* We allow reasonable numbers of entries in a request, but one with 
		   more than a hundred entries has something wrong with it */
		DEBUG_DIAG(( "OCSP request contains more than 100 entries" ));
		assert_nofuzz( DEBUG_WARN );
		return( CRYPT_ERROR_OVERFLOW );
		}

	return( CRYPT_OK );
	}

/* Write OCSP request entries */

CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1 ) ) \
static int sizeofOcspRequestEntry( INOUT REVOCATION_INFO *ocspEntry )
	{
	int ocspIDsize, attributeSize = 0, status;

	assert( isWritePtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );
	
	REQUIRES( ocspEntry->idType == CRYPT_KEYID_NONE );

	/* Get the OCSP ID size */
	status = ocspIDsize = sizeofOcspID( ocspEntry );
	if( cryptStatusError( status ) )
		return( status );

	/* Remember the encoded attribute size for later when we write the
	   attributes */
	status = \
		ocspEntry->attributeSize = sizeofAttributes( ocspEntry->attributes,
													 CRYPT_CERTTYPE_NONE );
	if( cryptStatusError( status ) )
		return( status );
	if( ocspEntry->attributeSize > 0 )
		{
		attributeSize = sizeofShortObject( \
							sizeofShortObject( ocspEntry->attributeSize ) );
		}

	return( sizeofShortObject( ocspIDsize + attributeSize ) );
	}

CHECK_RETVAL_LENGTH_SHORT \
int sizeofOcspRequestEntries( IN const DATAPTR ocspEntries )
	{
	REVOCATION_INFO *revocationInfo;
	int requestInfoLength = 0, status, LOOP_ITERATOR;

	REQUIRES( DATAPTR_ISVALID( ocspEntries ) );

	/* Determine how big the encoded OCSP request will be */
	LOOP_LARGE( revocationInfo = DATAPTR_GET( ocspEntries ), 
				revocationInfo != NULL,
				revocationInfo = DATAPTR_GET( revocationInfo->next ) )
		{
		int requestEntrySize;

		REQUIRES( sanityCheckRevInfo( revocationInfo ) );
		
		status = requestEntrySize = \
						sizeofOcspRequestEntry( revocationInfo );
		if( cryptStatusError( status ) )
			return( status );
		requestInfoLength += requestEntrySize;
		}
	ENSURES( LOOP_BOUND_OK );

	return( requestInfoLength );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeOcspRequestEntry( INOUT STREAM *stream, 
								  IN const REVOCATION_INFO *ocspEntry )
	{
	int ocspIDsize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );

	/* Get the OCSP ID size */
	status = ocspIDsize = sizeofOcspID( ocspEntry );
	if( cryptStatusError( status ) )
		return( status );

	/* If there are no attributes, we just write the ID */
	if( ocspEntry->attributeSize <= 0 )
		{
		writeSequence( stream, ocspIDsize );
		return( writeOcspID( stream, ocspEntry ) );
		}

	/* Write the header and ID information */
	writeSequence( stream, 
				   ocspIDsize + sizeofShortObject( \
									sizeofShortObject( ocspEntry->attributeSize ) ) );
	status = writeOcspID( stream, ocspEntry );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the per-entry extensions.  Since these are per-entry extensions 
	   rather than overall OCSP extensions we write them as 
	   CRYPT_CERTTYPE_NONE rather than CRYPT_CERTTYPE_OCSP to make sure that 
	   they're processed as required.  Note that these are per-request-entry 
	   extensions rather than overall per-request extensions so the tag is 
	   CTAG_OR_SR_EXTENSIONS rather than CTAG_OR_EXTENSIONS */
	status = writeConstructed( stream, 
							   sizeofObject( ocspEntry->attributeSize ), 
							   CTAG_OR_SR_EXTENSIONS );
	if( cryptStatusOK( status ) )
		{
		status = writeAttributes( stream, ocspEntry->attributes,
								  CRYPT_CERTTYPE_NONE, 
								  ocspEntry->attributeSize );
		}
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeOcspRequestEntries( INOUT STREAM *stream, 
							 IN const DATAPTR ocspEntries )
	{
	REVOCATION_INFO *revocationInfo;
	int status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Write the SEQUENCE OF validity information */
	LOOP_LARGE( revocationInfo = DATAPTR_GET( ocspEntries ), 
				revocationInfo != NULL,
				revocationInfo = DATAPTR_GET( revocationInfo->next ) )
		{
		REQUIRES( sanityCheckRevInfo( revocationInfo ) );

		status = writeOcspRequestEntry( stream, revocationInfo );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Read/write OCSP Responses						*
*																			*
****************************************************************************/

/* Read OCSP response entries:

	Entry ::= SEQUENCE {
		certID			CertID,
		certStatus		CHOICE {
			notRevd	[0]	IMPLICIT NULL,
			revd	[1]	SEQUENCE {
				revTime	GeneralizedTime,
				revReas	[0] EXPLICIT CRLReason Optional
							},
			unknown	[2] IMPLICIT NULL
						},
		thisUpdate		GeneralizedTime,
		nextUpdate	[0]	EXPLICIT GeneralizedTime OPTIONAL,
		extensions	[1]	EXPLICIT Extensions OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int readOcspResponseEntry( INOUT STREAM *stream, 
								  INOUT_PTR DATAPTR *listHeadPtr,
								  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus,
								  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	DATAPTR_ATTRIBUTE attributePtr;
	REVOCATION_INFO *currentEntry;
	BYTE idBuffer[ MAX_ID_SIZE + 8 ];
	CRYPT_KEYID_TYPE idType;
	time_t dummyTime;
	int endPos, length, crlReason = 0, tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Determine the overall size of the entry */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;

	/* Read the ID information */
	status = readOcspID( stream, &idType, idBuffer, MAX_ID_SIZE, &length );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're reading an OCSPv1 ID then the ability to recover any useful 
	   elements in it apart from the serial number has been destroyed by 
	   passing them through a one-way hash function, so we dig down into the
	   ID to extract just the serial number.  See the comment in 
	   checkRevocationOCSP() on the implications of this */
	if( idType == CRYPT_KEYID_NONE )
		{
		STREAM memStream;
		BYTE integer[ MAX_SERIALNO_SIZE + 8 ];
		int integerLength DUMMY_INIT;

		static_assert( MAX_SERIALNO_SIZE <= MAX_ID_SIZE, "Buffer size" );

		/* Dig down into the ID to extract the serial number and replace the
		   overall ID with that */
		sMemConnect( &memStream, idBuffer, length );
		readSequence( &memStream, NULL );
		readUniversal( &memStream );			/* AlgoID */
		readUniversal( &memStream );			/* Hashed issuer DN */
		status = readUniversal( &memStream );	/* Hashed partial issuer key */
		if( cryptStatusOK( status ) )
			{
			status = readInteger( &memStream, integer, MAX_SERIALNO_SIZE, 
								  &integerLength );
			}
		sMemDisconnect( &memStream );
		if( cryptStatusError( status ) )
			return( status );
		if( integerLength <= 0 )
			{
			/* Some certificates may have a serial number of zero, which is 
			   turned into a zero-length integer by the ASN.1 read code 
			   since it truncates leading zeroes that are added due to ASN.1 
			   encoding requirements.  If we get a zero-length integer we 
			   turn it into a single zero byte */
			idBuffer[ 0 ] = 0;
			length = 1;
			}
		else
			{
			REQUIRES( rangeCheck( integerLength, 1, MAX_ID_SIZE ) );
			memcpy( idBuffer, integer, integerLength );
			length = integerLength;
			}
		}

	/* Add the entry to the revocation information list */
	status = addRevocationEntry( listHeadPtr, &currentEntry, idType,
								 idBuffer, length, FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the status information */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
	switch( tag )
		{
		case MAKE_CTAG_PRIMITIVE( OCSP_STATUS_NOTREVOKED ):
			currentEntry->status = CRYPT_OCSPSTATUS_NOTREVOKED;
			status = readUniversal( stream );
			break;

		case MAKE_CTAG( OCSP_STATUS_REVOKED ):
			currentEntry->status = CRYPT_OCSPSTATUS_REVOKED;
			readConstructed( stream, NULL, OCSP_STATUS_REVOKED );
			status = readGeneralizedTime( stream, 
										  &currentEntry->revocationTime );
			if( checkStatusPeekTag( stream, status, tag ) && \
				tag == MAKE_CTAG( 0 ) )
				{
				/* Remember the crlReason for later */
				readConstructed( stream, NULL, 0 );
				status = readEnumerated( stream, &crlReason );
				}
			break;

		case MAKE_CTAG_PRIMITIVE( OCSP_STATUS_UNKNOWN ):
			currentEntry->status = CRYPT_OCSPSTATUS_UNKNOWN;
			status = readUniversal( stream );
			break;

		default:
			return( CRYPT_ERROR_BADDATA );
		}
	if( cryptStatusError( status ) )
		return( status );
	status = readGeneralizedTime( stream, &dummyTime );	/* thisUpdate */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( 0 ) )
		{
		readConstructed( stream, NULL, 0 );				/* nextUpdate */
		status = readGeneralizedTime( stream, &dummyTime );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Read the extensions if there are any present.  Since these are per-
	   entry extensions rather than overall OCSP extensions we read the 
	   wrapper here and read the extensions themselves as CRYPT_CERTTYPE_NONE 
	   rather than CRYPT_CERTTYPE_OCSP to make sure that they're processed 
	   as required */
	if( stell( stream ) <= endPos - MIN_ATTRIBUTE_SIZE )
		{
		status = readConstructed( stream, &length, CTAG_OP_EXTENSIONS );
		if( cryptStatusOK( status ) && length > 0 )
			{
			status = readAttributes( stream, &currentEntry->attributes,
									 CRYPT_CERTTYPE_NONE, length,
									 errorLocus, errorType );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If there's a crlReason present in the response and none as an
	   extension add it as an extension (OCSP allows the same information
	   to be specified in two different places, to make it easier we always
	   return it as a crlReason extension, however some implementations
	   return it in both places so we have to make sure that we don't try and
	   add it a second time) */
	attributePtr = findAttributeField( currentEntry->attributes,
									   CRYPT_CERTINFO_CRLREASON,
									   CRYPT_ATTRIBUTE_NONE );
	if( DATAPTR_ISNULL( attributePtr ) )
		{
		status = addAttributeField( &currentEntry->attributes,
									CRYPT_CERTINFO_CRLREASON, 
									CRYPT_ATTRIBUTE_NONE, crlReason, 0,
									errorLocus, errorType );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int readOcspResponseEntries( INOUT STREAM *stream, 
							 INOUT_PTR DATAPTR *listHeadPtr,
							 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType )
	{
	int length, noResponseEntries, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtr, sizeof( DATAPTR ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Read the outer wrapper */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the SEQUENCE OF response information */
	LOOP_LARGE( noResponseEntries = 0,
				length > 0 && noResponseEntries < 100,
				noResponseEntries++ )
		{
		const int innerStartPos = stell( stream );

		REQUIRES( isIntegerRangeNZ( innerStartPos ) );

		status = readOcspResponseEntry( stream, listHeadPtr,
										errorLocus, errorType );
		if( cryptStatusError( status ) )
			return( status );
		length -= stell( stream ) - innerStartPos;
		}
	ENSURES( LOOP_BOUND_OK );
	if( noResponseEntries >= 100 )
		{
		/* We allow reasonable numbers of entries in a response, but one with 
		   more than a hundred entries has something wrong with it */
		DEBUG_DIAG(( "OCSP response contains more than 100 entries" ));
		assert_nofuzz( DEBUG_WARN );
		return( CRYPT_ERROR_OVERFLOW );
		}

	return( CRYPT_OK );
	}

/* Write OCSP response entries */

CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1 ) ) \
static int sizeofOcspResponseEntry( INOUT REVOCATION_INFO *ocspEntry )
	{
	int ocspIDsize, certStatusSize = 0, attributeSize = 0, status;

	assert( isWritePtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );
	
	/* Get the OCSP ID size */
	status = ocspIDsize = sizeofOcspID( ocspEntry );
	if( cryptStatusError( status ) )
		return( status );

	/* Remember the encoded attribute size for later when we write the
	   attributes */
	status = ocspEntry->attributeSize = \
					sizeofAttributes( ocspEntry->attributes,
									  CRYPT_CERTTYPE_NONE );
	if( cryptStatusError( status ) )
		return( status );
	if( ocspEntry->attributeSize > 0 )
		attributeSize = sizeofShortObject( ocspEntry->attributeSize );

	/* Determine the size of the certificate status field */
	certStatusSize = ( ocspEntry->status != CRYPT_OCSPSTATUS_REVOKED ) ? \
					 sizeofNull() : sizeofShortObject( sizeofGeneralizedTime() );

	return( sizeofShortObject( ocspIDsize + certStatusSize + \
							   sizeofGeneralizedTime() + attributeSize ) );
	}

CHECK_RETVAL_LENGTH_SHORT \
int sizeofOcspResponseEntries( IN const DATAPTR ocspEntries )
	{
	REVOCATION_INFO *revocationInfo;
	int responseInfoLength = 0, status, LOOP_ITERATOR;

	REQUIRES( DATAPTR_ISVALID( ocspEntries ) );

	/* Determine how big the encoded OCSP response will be */
	LOOP_LARGE( revocationInfo = DATAPTR_GET( ocspEntries ), 
				revocationInfo != NULL,
				revocationInfo = DATAPTR_GET( revocationInfo->next ) )
		{
		int responseEntrySize;

		REQUIRES( sanityCheckRevInfo( revocationInfo ) );
		
		status = responseEntrySize = \
						sizeofOcspResponseEntry( revocationInfo );
		if( cryptStatusError( status ) )
			return( status );
		responseInfoLength += responseEntrySize;
		}
	ENSURES( LOOP_BOUND_OK );

	return( responseInfoLength );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeOcspResponseEntry( INOUT STREAM *stream, 
								   IN const REVOCATION_INFO *ocspEntry,
								   const time_t entryTime )
	{
	int ocspIDsize, certStatusSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );

	/* Get the OCSP ID size */
	status = ocspIDsize = sizeofOcspID( ocspEntry );
	if( cryptStatusError( status ) )
		return( status );

	/* Determine the size of the certificate status field */
	certStatusSize = ( ocspEntry->status != CRYPT_OCSPSTATUS_REVOKED ) ? \
					 sizeofNull() : sizeofShortObject( sizeofGeneralizedTime() );

	/* Write the header and ID information */
	writeSequence( stream, ocspIDsize + \
				   certStatusSize + sizeofGeneralizedTime() + \
				   ( ( ocspEntry->attributeSize > 0 ) ? \
						sizeofShortObject( ocspEntry->attributeSize ) : 0 ) );
	status = writeOcspID( stream, ocspEntry );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the certificate status */
	if( ocspEntry->status == CRYPT_OCSPSTATUS_REVOKED )
		{
		writeConstructed( stream, sizeofGeneralizedTime(),
						  CRYPT_OCSPSTATUS_REVOKED );
		status = writeGeneralizedTime( stream, ocspEntry->revocationTime,
									   DEFAULT_TAG );
		}
	else
		{
		/* An other-than-revoked status is communicated as a tagged NULL
		   value.  For no known reason this portion of OCSP uses implicit
		   tagging, since it's the one part of the PDU in which an
		   explicit tag would actually make sense */
		status = writeNull( stream, ocspEntry->status );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Write the current update time, which should be the current time.
	   Since new status information is always available we don't write a
	   nextUpdate time (in fact there is some disagreement over whether 
	   these times are based on CRL information, responder information, the 
	   response dispatch time, or a mixture of the above, implementations 
	   can be found that return all manner of peculiar values here) */
	status = writeGeneralizedTime( stream, entryTime, DEFAULT_TAG );
	if( cryptStatusError( status ) || ocspEntry->attributeSize <= 0 )
		return( status );

	/* Write the per-entry extensions.  Since these are per-entry extensions
	   rather than overall OCSP extensions we write them as 
	   CRYPT_CERTTYPE_NONE rather than CRYPT_CERTTYPE_OCSP to make sure that 
	   they're processed as required */
	return( writeAttributes( stream, ocspEntry->attributes,
							 CRYPT_CERTTYPE_NONE, ocspEntry->attributeSize ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeOcspResponseEntries( INOUT STREAM *stream, 
							  IN const DATAPTR ocspEntries,
							  const time_t entryTime )
	{
	REVOCATION_INFO *revocationInfo;
	int status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Write the SEQUENCE OF validity information */
	LOOP_LARGE( revocationInfo = DATAPTR_GET( ocspEntries ), 
				revocationInfo != NULL,
				revocationInfo = DATAPTR_GET( revocationInfo->next ) )
		{
		REQUIRES( sanityCheckRevInfo( revocationInfo ) );

		status = writeOcspResponseEntry( stream, revocationInfo, 
										 entryTime );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

#endif /* USE_CERTREV */
