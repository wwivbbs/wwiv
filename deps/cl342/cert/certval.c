/****************************************************************************
*																			*
*						Certificate Validity Routines						*
*						Copyright Peter Gutmann 1996-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTVAL

/****************************************************************************
*																			*
*					Add/Delete/Check Validity Information					*
*																			*
****************************************************************************/

/* Find an entry in a validity information list */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static const VALIDITY_INFO *findValidityEntry( const VALIDITY_INFO *listPtr,
											   IN_BUFFER( valueLength ) \
													const void *value,
											   IN_LENGTH_FIXED( KEYID_SIZE ) \
													const int valueLength )
	{
	const int vCheck = checksumData( value, valueLength );
	int iterationCount;

	assert( isReadPtr( listPtr, sizeof( VALIDITY_INFO ) ) );
	assert( isReadPtr( value, valueLength ) );

	REQUIRES_N( valueLength == KEYID_SIZE );

	/* Check whether this entry is present in the list */
	for( iterationCount = 0;
		 listPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 listPtr = listPtr->next, iterationCount++ )
		{
		if( listPtr->dCheck == vCheck && \
			!memcmp( listPtr->data, value, valueLength ) )
			return( listPtr );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( NULL );
	}

#if 0	/* 30/6/08 Doesn't seem to be used by anything */

/* Check whether a certificate is valid */

static int checkValidity( const CERT_INFO *certInfoPtr,
						  CERT_INFO *validityInfoPtr )
	{
	CERT_VAL_INFO *certValInfo = validityInfoPtr->cCertVal;
	VALIDITY_INFO *validityEntry;
	BYTE certHash[ CRYPT_MAX_HASHSIZE + 8 ];
	int certHashLength, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( validityInfoPtr, sizeof( CERT_INFO ) ) );
	
	REQUIRES( validityInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE );

	/* If there's no validity information present we can't say anything
	   about the certificate */
	if( certValInfo->validityInfo == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* Get the certificate hash and use it to check whether there's an entry 
	   for this certificate in the list.  We read the certificate hash 
	   indirectly since it's computed on demand and may not have been 
	   evaluated yet */
	status = getCertComponent( ( CERT_INFO * ) certInfoPtr,
							   CRYPT_CERTINFO_FINGERPRINT_SHA1,
							   certHash, CRYPT_MAX_HASHSIZE, 
							   &certHashLength );
	if( cryptStatusError( status ) )
		return( status );
	validityEntry = findValidityEntry( certValInfo->validityInfo,
									   certHash, certHashLength );
	if( validityEntry == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* Select the entry that contains the validity information and return 
	   the certificate's status */
	certValInfo->currentValidity = validityEntry;
	return( ( validityEntry->status == TRUE ) ? \
			CRYPT_OK : CRYPT_ERROR_INVALID );
	}
#endif /* 0 */

/* Add an entry to a validation list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int addValidityEntry( INOUT_PTR VALIDITY_INFO **listHeadPtrPtr,
					  OUT_OPT_PTR_COND VALIDITY_INFO **newEntryPosition,
					  IN_BUFFER( valueLength ) const void *value, 
					  IN_LENGTH_FIXED( KEYID_SIZE ) const int valueLength )
	{
	VALIDITY_INFO *newElement;

	assert( isWritePtr( listHeadPtrPtr, sizeof( VALIDITY_INFO * ) ) );
	assert( newEntryPosition == NULL || \
			isWritePtr( newEntryPosition, sizeof( VALIDITY_INFO * ) ) );
	assert( isReadPtr( value, valueLength ) );

	REQUIRES( valueLength == KEYID_SIZE );

	/* Clear return value */
	if( newEntryPosition != NULL )
		*newEntryPosition = NULL;

	/* Make sure that this entry isn't already present */
	if( *listHeadPtrPtr != NULL && \
		findValidityEntry( *listHeadPtrPtr, value, valueLength ) != NULL )
		{
		/* If we found an entry that matches the one being added, we can't
		   add it again */
		return( CRYPT_ERROR_DUPLICATE );
		}

	/* Allocate memory for the new element and copy the information across */
	if( ( newElement = ( VALIDITY_INFO * ) \
			clAlloc( "addValidityEntry", sizeof( VALIDITY_INFO ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( newElement, 0, sizeof( VALIDITY_INFO ) );
	memcpy( newElement->data, value, valueLength );
	newElement->dCheck = checksumData( value, valueLength );

	/* Insert the new element into the list */
	insertSingleListElement( listHeadPtrPtr, *listHeadPtrPtr, newElement );
	if( newEntryPosition != NULL )
		*newEntryPosition = newElement;
	return( CRYPT_OK );
	}

/* Delete a validity information list */

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteValidityEntries( INOUT_PTR VALIDITY_INFO **listHeadPtrPtr )
	{
	VALIDITY_INFO *entryListPtr = *listHeadPtrPtr;
	int iterationCount;

	assert( isWritePtr( listHeadPtrPtr, sizeof( VALIDITY_INFO * ) ) );

	*listHeadPtrPtr = NULL;

	/* Destroy any remaining list items */
	for( iterationCount = 0;
		 entryListPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		VALIDITY_INFO *itemToFree = entryListPtr;

		entryListPtr = entryListPtr->next;
		if( itemToFree->attributes != NULL )
			deleteAttributes( &itemToFree->attributes );
		zeroise( itemToFree, sizeof( VALIDITY_INFO ) );
		clFree( "deleteValidityEntries", itemToFree );
		}
	ENSURES_V( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	}

/* Copy a validity information list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyValidityEntries( INOUT_PTR VALIDITY_INFO **destListHeadPtrPtr,
						 const VALIDITY_INFO *srcListPtr )
	{
	const VALIDITY_INFO *srcListCursor;
	VALIDITY_INFO *destListCursor DUMMY_INIT_PTR;
	int iterationCount;

	assert( isWritePtr( destListHeadPtrPtr, sizeof( VALIDITY_INFO * ) ) );
	assert( *destListHeadPtrPtr == NULL );	/* Dest.should be empty */
	assert( isReadPtr( srcListPtr, sizeof( VALIDITY_INFO ) ) );

	/* Sanity check to make sure that the destination list doesn't already 
	   exist, which would cause the copy loop below to fail */
	REQUIRES( *destListHeadPtrPtr == NULL );

	/* Copy all validation entries from source to destination */
	for( srcListCursor = srcListPtr, iterationCount = 0; 
		 srcListCursor != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 srcListCursor = srcListCursor->next, iterationCount++ )
		{
		VALIDITY_INFO *newElement;

		/* Allocate the new entry and copy the data from the existing one
		   across.  We don't copy the attributes because there aren't any
		   that should be carried from request to response */
		if( ( newElement = ( VALIDITY_INFO * ) \
					clAlloc( "copyValidityEntries", \
							 sizeof( VALIDITY_INFO ) ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		memcpy( newElement, srcListCursor, sizeof( VALIDITY_INFO ) );
		newElement->attributes = NULL;
		newElement->next = NULL;

		/* Set the status to invalid/unknown by default, this means that any
		   entries that we can't do anything with automatically get the
		   correct status associated with them */
		newElement->status = FALSE;
		newElement->extStatus = CRYPT_CERTSTATUS_UNKNOWN;

		/* Link the new element into the list */
		insertSingleListElement( destListHeadPtrPtr, destListCursor, 
								 newElement );
		destListCursor = newElement;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( CRYPT_OK );
	}

/* Prepare the entries in a certificate validity list prior to encoding 
   them */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3, 4 ) ) \
int prepareValidityEntries( IN_OPT const VALIDITY_INFO *listPtr, 
							OUT_PTR_xCOND VALIDITY_INFO **errorEntry,
							OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType )
	{
	const VALIDITY_INFO *validityEntry;
	int iterationCount;

	assert( listPtr == NULL || \
			isReadPtr( listPtr, sizeof( VALIDITY_INFO ) ) );
	assert( isWritePtr( errorEntry, sizeof( VALIDITY_INFO * ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Clear return values */
	*errorEntry = NULL;
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* If the validity list is empty there's nothing to do */
	if( listPtr == NULL )
		return( CRYPT_OK );

	/* Check the attributes for each entry in a validation list */
	for( validityEntry = listPtr, iterationCount = 0; 
		 validityEntry != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 validityEntry = validityEntry->next, iterationCount++ )
		{
		int status;

		/* If there's nothing to check, skip this entry */
		if( validityEntry->attributes == NULL )
			continue;

		status = checkAttributes( ATTRIBUTE_CERTIFICATE,
								  validityEntry->attributes,
								  errorLocus, errorType );
		if( cryptStatusError( status ) )
			{
			/* Remember the entry that caused the problem */
			*errorEntry = ( VALIDITY_INFO * ) validityEntry;
			return( status );
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( CRYPT_OK );
	}

/* Check the entries in an RTCS response object against a certificate store.  
   The semantics for this one are a bit odd, the source information for the 
   check is from a request but the destination information is in a response.  
   Since we don't have a copy-and-verify function we do the checking from 
   the response even though technically it's the request data that's being 
   checked */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkRTCSResponse( INOUT CERT_INFO *certInfoPtr,
					   IN_HANDLE const CRYPT_KEYSET iCryptKeyset )
	{
	VALIDITY_INFO *validityInfo;
	BOOLEAN isInvalid = FALSE;
	int iterationCount;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptKeyset ) );

	/* Walk down the list of validity entries fetching status information
	   on each one from the certificate store */
	for( validityInfo = certInfoPtr->cCertVal->validityInfo, 
			iterationCount = 0;
		 validityInfo != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 validityInfo = validityInfo->next, iterationCount++ )
		{
		MESSAGE_KEYMGMT_INFO getkeyInfo;
		int status;

		/* Determine the validity of the object */
		setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_CERTID,
							   validityInfo->data, KEYID_SIZE, NULL, 0,
							   KEYMGMT_FLAG_CHECK_ONLY );
		status = krnlSendMessage( iCryptKeyset, IMESSAGE_KEY_GETKEY,
								  &getkeyInfo, KEYMGMT_ITEM_PUBLICKEY );
		if( cryptStatusOK( status ) )
			{
			/* The certificate is present and OK */
			validityInfo->status = TRUE;
			validityInfo->extStatus = CRYPT_CERTSTATUS_VALID;
			}
		else
			{
			/* The certificate isn't present/OK, record the fact that we've 
			   seen at least one invalid certificate */
			validityInfo->status = FALSE;
			validityInfo->extStatus = CRYPT_CERTSTATUS_NOTVALID;
			isInvalid = TRUE;
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* If at least one certificate was invalid indicate this to the caller.  
	   Note that if there are multiple certificates present in the query 
	   it's up to the caller to step through the list to find out which ones 
	   were invalid */
	return( isInvalid ? CRYPT_ERROR_INVALID : CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Read/write RTCS Information						*
*																			*
****************************************************************************/

/* Read/write an RTCS resquest entry:

	Entry ::= SEQUENCE {
		certHash		OCTET STRING SIZE(20),
		legacyID		IssuerAndSerialNumber OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofRtcsRequestEntry( STDC_UNUSED const VALIDITY_INFO *rtcsEntry )
	{
	assert( isReadPtr( rtcsEntry, sizeof( VALIDITY_INFO ) ) );

	return( ( int ) sizeofObject( sizeofObject( KEYID_SIZE ) ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readRtcsRequestEntry( INOUT STREAM *stream, 
						  INOUT_PTR VALIDITY_INFO **listHeadPtrPtr )
	{
	BYTE idBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int endPos, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtrPtr, sizeof( VALIDITY_INFO * ) ) );

	/* Determine the overall size of the entry */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;

	/* Read the certificate ID and add it to the validity information list */
	status = readOctetString( stream, idBuffer, &length,
							  KEYID_SIZE, KEYID_SIZE );
	if( cryptStatusOK( status ) && \
		stell( stream ) <= endPos - MIN_ATTRIBUTE_SIZE )
		{
		/* Skip the legacy ID */
		status = readUniversal( stream );
		}
	if( cryptStatusOK( status ) )
		status = addValidityEntry( listHeadPtrPtr, NULL, 
								   idBuffer, KEYID_SIZE );
	return( status );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeRtcsRequestEntry( INOUT STREAM *stream, 
						   const VALIDITY_INFO *rtcsEntry )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( rtcsEntry, sizeof( VALIDITY_INFO ) ) );

	/* Write the header and ID information */
	writeSequence( stream, sizeofObject( KEYID_SIZE ) );
	return( writeOctetString( stream, rtcsEntry->data, KEYID_SIZE,
							  DEFAULT_TAG ) );
	}

/* Read/write an RTCS response entry:

	Entry ::= SEQUENCE {				-- Basic response
		certHash		OCTET STRING SIZE(20),
		status			BOOLEAN
		}

	Entry ::= SEQUENCE {				-- Full response
		certHash		OCTET STRING SIZE(20),
		status			ENUMERATED,
		statusInfo		ANY DEFINED BY status OPTIONAL,
		extensions	[0]	Extensions OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofRtcsResponseEntry( INOUT VALIDITY_INFO *rtcsEntry,
							 const BOOLEAN isFullResponse )
	{
	int status;

	assert( isWritePtr( rtcsEntry, sizeof( VALIDITY_INFO ) ) );

	/* If it's a basic response the size is fairly easy to calculate */
	if( !isFullResponse )
		{
		return( ( int ) sizeofObject( sizeofObject( KEYID_SIZE ) + \
									  sizeofBoolean() ) );
		}

	/* Remember the encoded attribute size for later when we write the
	   attributes */
	status = \
		rtcsEntry->attributeSize = sizeofAttributes( rtcsEntry->attributes,
													 CRYPT_CERTTYPE_NONE );
	if( cryptStatusError( status ) )
		return( status );

	return( ( int ) \
			sizeofObject( sizeofObject( KEYID_SIZE ) + sizeofEnumerated( 1 ) + \
						  ( ( rtcsEntry->attributeSize ) ? \
							( int ) sizeofObject( rtcsEntry->attributeSize ) : 0 ) ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readRtcsResponseEntry( INOUT STREAM *stream, 
						   INOUT_PTR VALIDITY_INFO **listHeadPtrPtr,
						   INOUT CERT_INFO *certInfoPtr,
						   const BOOLEAN isFullResponse )
	{
	VALIDITY_INFO *newEntry;
	BYTE idBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int endPos, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( listHeadPtrPtr, sizeof( VALIDITY_INFO * ) ) );
	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	/* Determine the overall size of the entry */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;

	/* Read the ID information */
	status = readOctetString( stream, idBuffer, &length, \
							  KEYID_SIZE, KEYID_SIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Add the entry to the validity information list */
	status = addValidityEntry( listHeadPtrPtr, &newEntry, 
							   idBuffer, KEYID_SIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the status information and record the valid/not-valid status  */
	if( isFullResponse )
		{
		status = readEnumerated( stream, &newEntry->extStatus );
		if( cryptStatusOK( status ) )
			{
			newEntry->status = \
						( newEntry->extStatus == CRYPT_CERTSTATUS_VALID ) ? \
						TRUE : FALSE;
			}
		}
	else
		{
		status = readBoolean( stream, &newEntry->status );
		if( cryptStatusOK( status ) )
			{
			newEntry->extStatus = newEntry->status ? \
						CRYPT_CERTSTATUS_VALID : CRYPT_CERTSTATUS_NOTVALID;
			}
		}
	if( cryptStatusError( status ) || \
		stell( stream ) > endPos - MIN_ATTRIBUTE_SIZE )
		return( status );

	/* Read the extensions.  Since these are per-entry extensions we read
	   the wrapper here and read the extensions themselves as
	   CRYPT_CERTTYPE_NONE rather than CRYPT_CERTTYPE_RTCS to make sure
	   that they're processed as required */
	status = readConstructed( stream, &length, 0 );
	if( cryptStatusOK( status ) && length > 0 )
		{
		status = readAttributes( stream, &newEntry->attributes,
								 CRYPT_CERTTYPE_NONE, length,
								 &certInfoPtr->errorLocus,
								 &certInfoPtr->errorType );
		}
	return( status );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeRtcsResponseEntry( INOUT STREAM *stream, 
						    const VALIDITY_INFO *rtcsEntry,
							const BOOLEAN isFullResponse )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( rtcsEntry, sizeof( VALIDITY_INFO ) ) );
	
	REQUIRES( rtcsEntry->extStatus >= CRYPT_CERTSTATUS_VALID && \
			  rtcsEntry->extStatus <= CRYPT_CERTSTATUS_UNKNOWN );

	/* If it's a basic response it's a straightforward fixed-length
	   object */
	if( !isFullResponse )
		{
		writeSequence( stream, sizeofObject( KEYID_SIZE ) +
							   sizeofBoolean() );
		writeOctetString( stream, rtcsEntry->data, KEYID_SIZE, DEFAULT_TAG );
		return( writeBoolean( stream, rtcsEntry->status, DEFAULT_TAG ) );
		}

	/* Write an extended response */
	writeSequence( stream, sizeofObject( KEYID_SIZE ) + sizeofEnumerated( 1 ) );
	writeOctetString( stream, rtcsEntry->data, KEYID_SIZE, DEFAULT_TAG );
	status = writeEnumerated( stream, rtcsEntry->extStatus, DEFAULT_TAG );
	if( cryptStatusError( status ) || rtcsEntry->attributeSize <= 0 )
		return( status );

	/* Write the per-entry extensions.  Since these are per-entry extensions
	   we write them as CRYPT_CERTTYPE_NONE rather than CRYPT_CERTTYPE_RTCS
	   to make sure that they're processed as required */
	return( writeAttributes( stream, rtcsEntry->attributes,
							 CRYPT_CERTTYPE_NONE, rtcsEntry->attributeSize ) );
	}
#endif /* USE_CERTVAL */
