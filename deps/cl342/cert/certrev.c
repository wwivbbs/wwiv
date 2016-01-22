/****************************************************************************
*																			*
*						Certificate Revocation Routines						*
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int findRevocationEntry( const REVOCATION_INFO *listPtr,
								OUT_OPT_PTR REVOCATION_INFO **insertPoint,
								IN_BUFFER( valueLength ) const void *value, 
								IN_LENGTH_SHORT const int valueLength,
								const BOOLEAN sortEntries )
	{
	const REVOCATION_INFO *prevElement = NULL;
	const int idCheck = checksumData( value, valueLength );
	int iterationCount;

	assert( isReadPtr( listPtr, sizeof( REVOCATION_INFO ) ) );
	assert( isWritePtr( insertPoint, sizeof( REVOCATION_INFO * ) ) );
	assert( isReadPtr( value, valueLength ) );

	REQUIRES( valueLength > 0 && valueLength < MAX_INTLENGTH_SHORT );

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
	for( iterationCount = 0;
		 listPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_MAX;
		 listPtr = listPtr->next, iterationCount++ )
		{
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
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );

	/* We can't find a matching entry, return the revocation entry that we 
	   should insert the new value after */
	*insertPoint = ( REVOCATION_INFO * ) prevElement;
	return( CRYPT_ERROR_NOTFOUND );
	}

/* Add an entry to a revocation list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int addRevocationEntry( INOUT_PTR REVOCATION_INFO **listHeadPtrPtr,
						OUT_OPT_PTR REVOCATION_INFO **newEntryPosition,
						IN_KEYID const CRYPT_KEYID_TYPE valueType,
						IN_BUFFER( valueLength ) const void *value, 
						IN_LENGTH_SHORT const int valueLength,
						const BOOLEAN noCheck )
	{
	REVOCATION_INFO *newElement, *insertPoint;

	assert( isWritePtr( listHeadPtrPtr, sizeof( REVOCATION_INFO * ) ) );
	assert( isWritePtr( newEntryPosition, sizeof( REVOCATION_INFO * ) ) );
	assert( isReadPtr( value, valueLength ) );
	
	REQUIRES( valueType == CRYPT_KEYID_NONE || \
			  valueType == CRYPT_IKEYID_CERTID || \
			  valueType == CRYPT_IKEYID_ISSUERID || \
			  valueType == CRYPT_IKEYID_ISSUERANDSERIALNUMBER );
	REQUIRES( valueLength > 0 && valueLength < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*newEntryPosition = NULL;

	/* Find the insertion point for the new entry unless we're reading data
	   from a large pre-encoded CRL (indicated by the caller setting the 
	   noCheck flag), in which case we just drop it in at the start.  The 
	   absence of checking is necessary in order to provide same-day service 
	   for large CRLs */
	if( !noCheck && *listHeadPtrPtr != NULL && \
		cryptStatusOK( \
			findRevocationEntry( *listHeadPtrPtr, &insertPoint, value,
								  valueLength, TRUE ) ) )
		{
		/* If we get an OK status it means that we've found an existing
		   entry that matches the one being added, we can't add it again */
		return( CRYPT_ERROR_DUPLICATE );
		}
	else
		{
		/* Insert the new element at the start */
		insertPoint = NULL;
		}

	/* Allocate memory for the new element and copy the information across */
	if( ( newElement = ( REVOCATION_INFO * ) \
			clAlloc( "addRevocationEntry", \
					 sizeof( REVOCATION_INFO ) + valueLength ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	initVarStruct( newElement, REVOCATION_INFO, valueLength );
	newElement->id = newElement->value;	/* Varstruct expects field name 'value' */
	newElement->idType = valueType;
	memcpy( newElement->id, value, valueLength );
	newElement->idLength = valueLength;
	newElement->idCheck = checksumData( value, valueLength );

	/* Insert the new element into the list */
	insertSingleListElement( listHeadPtrPtr, insertPoint, newElement );
	*newEntryPosition = newElement;

	return( CRYPT_OK );
	}

/* Delete a revocation list */

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteRevocationEntries( INOUT_PTR REVOCATION_INFO **listHeadPtrPtr )
	{
	REVOCATION_INFO *entryListPtr = *listHeadPtrPtr;
	int iterationCount;

	assert( isWritePtr( listHeadPtrPtr, sizeof( REVOCATION_INFO * ) ) );

	*listHeadPtrPtr = NULL;

	/* Destroy any remaining list items */
	for( iterationCount = 0;
		 entryListPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_MAX;
		 iterationCount++ )
		{
		REVOCATION_INFO *itemToFree = entryListPtr;

		entryListPtr = entryListPtr->next;
		if( itemToFree->attributes != NULL )
			deleteAttributes( &itemToFree->attributes );
		zeroise( itemToFree, sizeof( REVOCATION_INFO ) );
		clFree( "deleteRevocationEntries", itemToFree );
		}
	}

/* Prepare the entries in a revocation list prior to encoding them */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5, 6 ) ) \
int prepareRevocationEntries( INOUT_OPT REVOCATION_INFO *listPtr, 
							  const time_t defaultTime,
							  OUT_OPT_PTR REVOCATION_INFO **errorEntry,
							  const BOOLEAN isSingleEntry,
							  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType )
	{
	REVOCATION_INFO *revocationEntry;
	const time_t currentTime = ( defaultTime > MIN_TIME_VALUE ) ? \
							   defaultTime : getApproxTime();
	int value, iterationCount, status;

	assert( listPtr == NULL || \
			isReadPtr( listPtr, sizeof( REVOCATION_INFO ) ) );
	assert( isWritePtr( errorEntry, sizeof( REVOCATION_INFO * ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Clear return value */
	*errorEntry = NULL;

	/* If the revocation list is empty there's nothing to do */
	if( listPtr == NULL )
		return( CRYPT_OK );

	/* Set the revocation time if this hasn't already been set.  If there's a
	   default time set we use that otherwise we use the current time */
	for( revocationEntry = listPtr, iterationCount = 0; 
		 revocationEntry != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 revocationEntry = revocationEntry->next, iterationCount++ )
		{
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
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );

	/* Check the attributes for each entry in a revocation list */
	for( revocationEntry = listPtr, iterationCount = 0; 
		 revocationEntry != NULL && iterationCount < FAILSAFE_ITERATIONS_MAX; 
		 revocationEntry = revocationEntry->next, iterationCount++ )
		{
		if( revocationEntry->attributes != NULL )
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
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							CRL-specific Functions							*
*																			*
****************************************************************************/

/* Check whether a certificate has been revoked */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkRevocationCRL( const CERT_INFO *certInfoPtr, 
							   INOUT CERT_INFO *revocationInfoPtr )
	{
	CERT_REV_INFO *certRevInfo = revocationInfoPtr->cCertRev;
	REVOCATION_INFO *revocationEntry;
	int status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( revocationInfoPtr, sizeof( CERT_INFO ) ) );

	/* If there's no revocation information present then the certificate 
	   can't have been revoked */
	if( certRevInfo->revocations == NULL )
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

	/* Select the entry that contains the revocation information and return
	   the certificate's status */
	certRevInfo->currentRevocation = revocationEntry;
	return( CRYPT_ERROR_INVALID );
	}

/* Check a certificate against a CRL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCRL( INOUT CERT_INFO *certInfoPtr, 
			  IN_HANDLE const CRYPT_CERTIFICATE iCryptCRL )
	{
	CERT_INFO *crlInfoPtr;
	int i, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptCRL ) );

	/* Check that the CRL is a complete signed CRL and not just a 
	   newly-created CRL object */
	status = krnlAcquireObject( iCryptCRL, OBJECT_TYPE_CERTIFICATE,
								( void ** ) &crlInfoPtr,
								CRYPT_ARGERROR_VALUE );
	if( cryptStatusError( status ) )
		return( status );
	if( crlInfoPtr->certificate == NULL )
		{
		krnlReleaseObject( crlInfoPtr->objectHandle );
		return( CRYPT_ERROR_NOTINITED );
		}
	ANALYSER_HINT( crlInfoPtr != NULL );

	/* Check the base certificate against the CRL.  If it's been revoked or 
	   there's only a single certificate present, exit */
	status = checkRevocationCRL( certInfoPtr, crlInfoPtr );
	if( cryptStatusError( status ) || \
		certInfoPtr->type != CRYPT_CERTTYPE_CERTCHAIN )
		{
		krnlReleaseObject( crlInfoPtr->objectHandle );
		return( status );
		}

	/* It's a certificate chain, check every remaining certificate in the 
	   chain against the CRL.  In theory this is pointless because a CRL can 
	   only contain information for a single certificate in the chain, 
	   however the caller may have passed us a CRL for an intermediate 
	   certificate (in which case the check for the leaf certificate was 
	   pointless).  In any case it's easier to just do the check for all 
	   certificates than to determine which certificate the CRL applies to 
	   so we check for all certificates */
	for( i = 0; i < certInfoPtr->cCertCert->chainEnd && \
				i < MAX_CHAINLENGTH; i++ )
		{
		CERT_INFO *certChainInfoPtr;

		/* Check this certificate against the CRL */
		status = krnlAcquireObject( certInfoPtr->cCertCert->chain[ i ],
									OBJECT_TYPE_CERTIFICATE,
									( void ** ) &certChainInfoPtr,
									CRYPT_ERROR_SIGNALLED );
		if( cryptStatusOK( status ) )
			{
			status = checkRevocationCRL( certChainInfoPtr, crlInfoPtr );
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

	krnlReleaseObject( crlInfoPtr->objectHandle );
	return( status );
	}

/****************************************************************************
*																			*
*							Read/write CRL Information						*
*																			*
****************************************************************************/

/* Read/write CRL entries:

	RevokedCert ::= SEQUENCE {
			userCertificate		CertificalSerialNumber,
			revocationDate		UTCTime,
			extensions			Extensions OPTIONAL
			} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofCRLentry( INOUT REVOCATION_INFO *crlEntry )
	{
	int status;

	assert( isWritePtr( crlEntry, sizeof( REVOCATION_INFO ) ) );

	/* Remember the encoded attribute size for later when we write the
	   attributes */
	status = \
		crlEntry->attributeSize = sizeofAttributes( crlEntry->attributes );
	if( cryptStatusError( status ) )
		return( status );

	return( ( int ) sizeofObject( \
						sizeofInteger( crlEntry->id, crlEntry->idLength ) + \
						sizeofUTCTime() + \
						( ( crlEntry->attributeSize > 0 ) ? \
							( int ) sizeofObject( crlEntry->attributeSize ) : 0 ) ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int readCRLentry( INOUT STREAM *stream, 
				  INOUT_PTR REVOCATION_INFO **listHeadPtrPtr,
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
	assert( isWritePtr( listHeadPtrPtr, sizeof( REVOCATION_INFO * ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( entryNo >= 0 && entryNo < MAX_INTLENGTH );

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
	status = addRevocationEntry( listHeadPtrPtr, &currentEntry,
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

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCRLentry( INOUT STREAM *stream, 
				   const REVOCATION_INFO *crlEntry )
	{
	const int revocationLength = \
				sizeofInteger( crlEntry->id, crlEntry->idLength ) + \
				sizeofUTCTime() + \
				( ( crlEntry->attributeSize > 0 ) ? \
					( int ) sizeofObject( crlEntry->attributeSize ) : 0 );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( crlEntry, sizeof( REVOCATION_INFO ) ) );

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

/****************************************************************************
*																			*
*							OCSP-specific Functions							*
*																			*
****************************************************************************/

#if 0	/* 28/8/08 Doesn't seem to be used */

/* Check whether a certificate has been revoked */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int checkRevocationOCSP( const CERT_INFO *certInfoPtr, 
						 INOUT CERT_INFO *revocationInfoPtr )
	{
	CERT_REV_INFO *certRevInfo = revocationInfoPtr->cCertRev;
	REVOCATION_INFO *revocationEntry = DUMMY_INIT_PTR;
	BYTE certHash[ CRYPT_MAX_HASHSIZE + 8 ];
	int certHashLength, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( revocationInfoPtr, sizeof( CERT_INFO ) ) );

	/* If there's no revocation information present then the certificate 
	   can't have been revoked */
	if( certRevInfo->revocations == NULL )
		return( CRYPT_OK );

	/* Get the certificate hash and use it to check whether there's an entry 
	   for this certificate in the list.  We read the certificate hash 
	   indirectly since it's computed on demand and may not have been 
	   evaluated yet */
	status = getCertComponentString( ( CERT_INFO * ) certInfoPtr,
									 CRYPT_CERTINFO_FINGERPRINT_SHA1,
									 certHash, CRYPT_MAX_HASHSIZE, 
									 &certHashLength );
	if( cryptStatusOK( status ) )
		{
		status = findRevocationEntry( certRevInfo->revocations,
									  &revocationEntry, certHash,
									  certHashLength, FALSE );
		}
	if( cryptStatusError( status ) )
		{
		/* No entry, either good or bad, we can't report anything about the 
		   certificate */
		return( status );
		}
	ENSURES( revocationEntry != NULL );

	/* Select the entry that contains the revocation information and return
	   the certificate's status.  The unknown status is a bit difficult to 
	   report, the best that we can do is report notfound although the 
	   notfound occurred at the responder rather than here */
	certRevInfo->currentRevocation = revocationEntry;
	return( ( revocationEntry->status == CRYPT_OCSPSTATUS_NOTREVOKED ) ? \
				CRYPT_OK : \
			( revocationEntry->status == CRYPT_OCSPSTATUS_REVOKED ) ? \
				CRYPT_ERROR_INVALID : CRYPT_ERROR_NOTFOUND );
	}
#endif /* 0 */

/* Copy a revocation list from an OCSP request to a response */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyRevocationEntries( INOUT_PTR REVOCATION_INFO **destListHeadPtrPtr,
						   const REVOCATION_INFO *srcListPtr )
	{
	const REVOCATION_INFO *srcListCursor;
	REVOCATION_INFO *destListCursor = DUMMY_INIT_PTR;
	int iterationCount;

	assert( isWritePtr( destListHeadPtrPtr, sizeof( REVOCATION_INFO * ) ) );
	assert( isReadPtr( srcListPtr, sizeof( REVOCATION_INFO ) ) );

	/* Sanity check to make sure that the destination list doesn't already 
	   exist, which would cause the copy loop below to fail */
	REQUIRES( *destListHeadPtrPtr == NULL );

	/* Copy all revocation entries from source to destination */
	for( srcListCursor = srcListPtr, iterationCount = 0; 
		 srcListCursor != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 srcListCursor = srcListCursor->next, iterationCount++ )
		{
		REVOCATION_INFO *newElement;

		/* Allocate the new entry and copy the data from the existing one
		   across.  We don't copy the attributes because there aren't any
		   that should be carried from request to response */
		if( ( newElement = ( REVOCATION_INFO * ) \
					clAlloc( "copyRevocationEntries",
							 sizeof( REVOCATION_INFO ) + \
								srcListCursor->idLength ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		copyVarStruct( newElement, srcListCursor, REVOCATION_INFO );
		newElement->id = newElement->value;	/* Varstruct expects field name 'value' */
		newElement->attributes = NULL;
		newElement->next = NULL;

		/* Set the status to 'unknown' by default, this means that any
		   entries that we can't do anything with automatically get the
		   correct status associated with them */
		newElement->status = CRYPT_OCSPSTATUS_UNKNOWN;

		/* Link the new element into the list */
		insertSingleListElement( destListHeadPtrPtr, destListCursor, 
								 newElement );
		destListCursor = newElement;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

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
	REVOCATION_INFO *revocationInfo;
	BOOLEAN isRevoked = FALSE;
	int iterationCount;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptKeyset ) );

	/* Walk down the list of revocation entries fetching status information
	   on each one from the certificate store */
	for( revocationInfo = certInfoPtr->cCertRev->revocations, 
			iterationCount = 0;
		 revocationInfo != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 revocationInfo = revocationInfo->next, iterationCount++ )
		{
		CRYPT_KEYID_TYPE idType = revocationInfo->idType;
		MESSAGE_KEYMGMT_INFO getkeyInfo;
		CERT_INFO *crlEntryInfoPtr;
		const BYTE *id = revocationInfo->id;
		int status;

		REQUIRES( revocationInfo->idType == CRYPT_KEYID_NONE || \
				  revocationInfo->idType == CRYPT_IKEYID_CERTID || \
				  revocationInfo->idType == CRYPT_IKEYID_ISSUERID );

		/* If it's an OCSPv1 ID and there's no alternate ID information 
		   present we can't really do anything with it because the one-way 
		   hashing process required by the standard destroys the certificate
		   identifying information */
		if( idType == CRYPT_KEYID_NONE )
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
		   certificate.  The best we can do is assume that a not-revoked 
		   status will be the most common and if that fails fall back to a 
		   revoked status check */
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
									( void ** ) &crlEntryInfoPtr,
									CRYPT_ERROR_SIGNALLED );
		if( cryptStatusOK( status ) )
			{
			const REVOCATION_INFO *crlRevocationInfo;

			crlRevocationInfo = crlEntryInfoPtr->cCertRev->revocations;
			if( crlRevocationInfo != NULL )
				{
				revocationInfo->revocationTime = \
									crlRevocationInfo->revocationTime;
				if( crlRevocationInfo->attributes != NULL )
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
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* If at least one certificate was revoked indicate this to the caller.  
	   Note that if there are multiple certificates present in the query then 
	   it's up to the caller to step through the list to find out which ones 
	   were revoked */
	return( isRevoked ? CRYPT_ERROR_INVALID : CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Read/write OCSP Information						*
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

CHECK_RETVAL_RANGE( MAX_ERROR, 1024 ) STDC_NONNULL_ARG( ( 1 ) ) \
static int sizeofOcspID( const REVOCATION_INFO *ocspEntry )
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
					   OUT_LENGTH_SHORT_Z int *idLen )
	{
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	void *dataPtr = DUMMY_INIT_PTR;
	int length, tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( idType, sizeof( CRYPT_KEYID_TYPE ) ) );
	assert( isWritePtr( id, idMaxLen ) );
	assert( isWritePtr( idLen, sizeof( int ) ) );

	REQUIRES( idMaxLen >= 16 && idMaxLen < MAX_INTLENGTH_SHORT );

	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, NULL );

	/* Clear return values */
	*idType = CRYPT_KEYID_NONE;
	memset( id, 0, min( 16, idMaxLen ) );
	*idLen = 0;

	/* Read the ID */
	tag = peekTag( stream );
	if( cryptStatusError( tag ) )
		return( tag );
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
			*idLen = KEYID_SIZE;
			readConstructed( stream, NULL, CTAG_OI_CERTIFICATE );
			status = readConstructed( stream, &length, 0 );
			if( cryptStatusOK( status ) )
				status = sMemGetDataBlock( stream, &dataPtr, length );
			if( cryptStatusError( status ) )
				return( status );
			hashFunctionAtomic( id, KEYID_SIZE, dataPtr, length );
			return( readUniversal( stream ) );

		case MAKE_CTAG( CTAG_OI_CERTIDWITHSIG ):
			/* A bizarro ID dreamed up by Denis Pinkas that manages to carry
			   over all the problems of the v1 ID without being compatible
			   with it.  It's almost as unworkable as the v1 original but we 
			   can convert the iAndS to an issuerID and use that */
			*idType = CRYPT_IKEYID_ISSUERID;
			*idLen = KEYID_SIZE;
			readConstructed( stream, NULL, CTAG_OI_CERTIDWITHSIG );
			readSequence( stream, NULL );
			status = getStreamObjectLength( stream, &length );
			if( cryptStatusOK( status ) )
				status = sMemGetDataBlock( stream, &dataPtr, length );
			if( cryptStatusError( status ) )
				return( status );
			hashFunctionAtomic( id, KEYID_SIZE, dataPtr, length );
			sSkip( stream, length );			/* issuerAndSerialNumber */
			readUniversal( stream );			/* tbsCertificateHash */
			return( readUniversal( stream ) );	/* certSignature */
		}

	return( CRYPT_ERROR_BADDATA );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeOcspID( INOUT STREAM *stream, 
						const REVOCATION_INFO *ocspEntry )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );
	
	return( swrite( stream, ocspEntry->id, ocspEntry->idLength ) );
	}

/* Read/write an OCSP request entry:

	Entry ::= SEQUENCE {				-- Request
		certID			CertID,
		extensions	[0]	EXPLICIT Extensions OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofOcspRequestEntry( INOUT REVOCATION_INFO *ocspEntry )
	{
	int status;

	assert( isWritePtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );
	
	REQUIRES( ocspEntry->idType == CRYPT_KEYID_NONE );

	/* Remember the encoded attribute size for later when we write the
	   attributes */
	status = \
		ocspEntry->attributeSize = sizeofAttributes( ocspEntry->attributes );
	if( cryptStatusError( status ) )
		return( status );

	return( ( int ) \
			sizeofObject( sizeofOcspID( ocspEntry ) + \
						  ( ( ocspEntry->attributeSize > 0 ) ? \
							( int ) \
							sizeofObject( \
								sizeofObject( ocspEntry->attributeSize ) ) : 0 ) ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readOcspRequestEntry( INOUT STREAM *stream, 
						  INOUT_PTR REVOCATION_INFO **listHeadPtrPtr,
						  INOUT CERT_INFO *certInfoPtr )
	{
	const ATTRIBUTE_PTR *attributePtr;
	REVOCATION_INFO *currentEntry;
	STREAM certIdStream;
	BYTE idBuffer[ MAX_ID_SIZE + 8 ];
	CRYPT_KEYID_TYPE idType;
	void *certIdPtr;
	int endPos, certIdLength, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtrPtr, sizeof( REVOCATION_INFO * ) ) );
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

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
	status = addRevocationEntry( listHeadPtrPtr, &currentEntry, idType,
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
	if( cryptStatusOK( status ) )
		{
		status = readAttributes( stream, &currentEntry->attributes,
								 CRYPT_CERTTYPE_NONE, length,
								 &certInfoPtr->errorLocus,
								 &certInfoPtr->errorType );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* OCSPv1 uses a braindamaged certificate identification method that 
	   breaks the certificate information up into bits and hashes some while 
	   leaving others intact, making it impossible to identify the 
	   certificate from it.  To try and fix this, if the request includes an 
	   ESSCertID we use that to make it look like there was a proper ID 
	   present */
	if( currentEntry->idType != CRYPT_KEYID_NONE )
		return( CRYPT_OK );		/* Proper ID present, we're done */
	attributePtr = findAttribute( currentEntry->attributes, 
								  CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID, 
								  TRUE );
	if( attributePtr == NULL )
		return( CRYPT_OK );		/* No ESSCertID present, can't continue */

	/* Extract the ID information from the ESSCertID and save it alongside
	   the OCSP ID, which we need to retain for use in the response */
	status = getAttributeDataPtr( attributePtr, &certIdPtr, &certIdLength );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &certIdStream, certIdPtr, certIdLength );
	readSequence( &certIdStream, NULL );
	status = readOctetString( &certIdStream, idBuffer, &length, KEYID_SIZE, 
							  KEYID_SIZE );
	if( cryptStatusOK( status ) )
		{
		currentEntry->altIdType = CRYPT_IKEYID_CERTID;
		memcpy( currentEntry->altID, idBuffer, length );
		}
	sMemDisconnect( &certIdStream );

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeOcspRequestEntry( INOUT STREAM *stream, 
						   const REVOCATION_INFO *ocspEntry )
	{
	const int attributeSize = ( ocspEntry->attributeSize > 0 ) ? \
					( int ) sizeofObject( \
								sizeofObject( ocspEntry->attributeSize ) ) : 0;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );

	/* Write the header and ID information */
	writeSequence( stream, sizeofOcspID( ocspEntry ) + attributeSize );
	status = writeOcspID( stream, ocspEntry );
	if( cryptStatusError( status ) || ocspEntry->attributeSize <= 0 )
		return( status );

	/* Write the per-entry extensions.  Since these are per-entry extensions 
	   rather than overall OCSP extensions we write them as 
	   CRYPT_CERTTYPE_NONE rather than CRYPT_CERTTYPE_OCSP to make sure that 
	   they're processed as required.  Note that these are per-request-entry 
	   extensions rather than overall per-request extensions so the tag is 
	   CTAG_OR_SR_EXTENSIONS rather than CTAG_OR_EXTENSIONS */
	writeConstructed( stream, sizeofObject( ocspEntry->attributeSize ), 
					  CTAG_OR_SR_EXTENSIONS );
	return( writeAttributes( stream, ocspEntry->attributes,
							 CRYPT_CERTTYPE_NONE, ocspEntry->attributeSize ) );
	}

/* Read/write an OCSP response entry:

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
		extensions	[1]	EXPLICIT Extensions OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofOcspResponseEntry( INOUT REVOCATION_INFO *ocspEntry )
	{
	int certStatusSize = 0, status;

	assert( isWritePtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );

	/* Remember the encoded attribute size for later when we write the
	   attributes */
	status = \
		ocspEntry->attributeSize = sizeofAttributes( ocspEntry->attributes );
	if( cryptStatusError( status ) )
		return( status );

	/* Determine the size of the certificate status field */
	certStatusSize = ( ocspEntry->status != CRYPT_OCSPSTATUS_REVOKED ) ? \
					 sizeofNull() : ( int ) sizeofObject( sizeofGeneralizedTime() );

	return( ( int ) \
			sizeofObject( sizeofOcspID( ocspEntry ) + \
						  certStatusSize + sizeofGeneralizedTime() ) + \
						  ( ( ocspEntry->attributeSize > 0 ) ? \
							( int ) sizeofObject( ocspEntry->attributeSize ) : 0 ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readOcspResponseEntry( INOUT STREAM *stream, 
						   INOUT_PTR REVOCATION_INFO **listHeadPtrPtr,
						   INOUT CERT_INFO *certInfoPtr )
	{
	REVOCATION_INFO *currentEntry;
	BYTE idBuffer[ MAX_ID_SIZE + 8 ];
	CRYPT_KEYID_TYPE idType;
	int endPos, length, crlReason = 0, tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtrPtr, sizeof( REVOCATION_INFO * ) ) );
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

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
	status = addRevocationEntry( listHeadPtrPtr, &currentEntry, idType,
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
			if( cryptStatusOK( status ) && \
				peekTag( stream ) == MAKE_CTAG( 0 ) )
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
	status = readGeneralizedTime( stream, &certInfoPtr->startTime );
	if( cryptStatusOK( status ) && peekTag( stream ) == MAKE_CTAG( 0 ) )
		{
		readConstructed( stream, NULL, 0 );
		status = readGeneralizedTime( stream, &certInfoPtr->endTime );
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
		if( cryptStatusOK( status ) )
			{
			status = readAttributes( stream, &currentEntry->attributes,
						CRYPT_CERTTYPE_NONE, length,
						&certInfoPtr->errorLocus, &certInfoPtr->errorType );
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
	if( findAttributeField( currentEntry->attributes,
							CRYPT_CERTINFO_CRLREASON,
							CRYPT_ATTRIBUTE_NONE ) == NULL )
		{
		status = addAttributeField( &currentEntry->attributes,
									CRYPT_CERTINFO_CRLREASON, 
									CRYPT_ATTRIBUTE_NONE, crlReason, 0,
									&certInfoPtr->errorLocus, 
									&certInfoPtr->errorType );
		}

	return( status );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeOcspResponseEntry( INOUT STREAM *stream, 
							const REVOCATION_INFO *ocspEntry,
							const time_t entryTime )
	{
	int certStatusSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( ocspEntry, sizeof( REVOCATION_INFO ) ) );

	/* Determine the size of the certificate status field */
	certStatusSize = ( ocspEntry->status != CRYPT_OCSPSTATUS_REVOKED ) ? \
					 sizeofNull() : ( int ) sizeofObject( sizeofGeneralizedTime() );

	/* Write the header and ID information */
	writeSequence( stream, sizeofOcspID( ocspEntry ) + \
				   certStatusSize + sizeofGeneralizedTime() + \
				   ( ( ocspEntry->attributeSize > 0 ) ? \
						( int ) sizeofObject( ocspEntry->attributeSize ) : 0 ) );
	status = writeOcspID( stream, ocspEntry );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the certificate status */
	if( ocspEntry->status == CRYPT_OCSPSTATUS_REVOKED )
		{
		writeConstructed( stream, sizeofGeneralizedTime(),
						  CRYPT_OCSPSTATUS_REVOKED );
		writeGeneralizedTime( stream, ocspEntry->revocationTime,
							  DEFAULT_TAG );
		}
	else
		{
		/* An other-than-revoked status is communicated as a tagged NULL
		   value.  For no known reason this portion of OCSP uses implicit
		   tagging, since it's the one part of the PDU in which an
		   explicit tag would actually make sense */
		writeNull( stream, ocspEntry->status );
		}

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
#endif /* USE_CERTREV */
