/****************************************************************************
*																			*
*					cryptlib De-enveloping Information Management			*
*						Copyright Peter Gutmann 1996-2016					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "envelope.h"
  #include "pgp.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "envelope/envelope.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

/* The maximum number of content items that we can add to a content list.
   Encrypted messages sent to very large distribution lists can potentially 
   have a large number of per-recipient wrapped keys, although this stuff is
   so rarely used that way that there's no hard data on it.  So far a bound
   of 50 items seems to be a pretty safe bet */

#define MAX_CONTENT_ITEMS	50

#ifdef USE_ENVELOPES

/****************************************************************************
*																			*
*						Content List Management Functions					*
*																			*
****************************************************************************/

/* Sanity-check a content-list entry */

CHECK_RETVAL_BOOL \
static BOOLEAN checkDataPointer( IN const void *data, 
								 IN const int dataLength,
								 IN_LENGTH_SHORT const int dataMinLength, 
								 IN_LENGTH_SHORT const int dataMaxLength )
	{
	/* We don't perform any precondition checks on the to-be-checked input 
	   values because the purpose of this function is to check potentially 
	   invalid values, so we can't assume they have any particular value */
	REQUIRES_B( isShortIntegerRangeNZ( dataMinLength ) );
	REQUIRES_B( dataMaxLength > 0 && dataMaxLength < MAX_INTLENGTH_SHORT && \
				dataMinLength < dataMaxLength );

	if( data == NULL )
		{
		if( dataLength != 0 )
			return( FALSE );

		return( TRUE );
		}
	if( dataLength < dataMinLength || dataLength > dataMaxLength )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckContentCrypt( IN const CONTENT_LIST *contentListPtr )
	{
	const CONTENT_ENCR_INFO *contentEncrInfo = &contentListPtr->clEncrInfo;
	const int maxIterations = \
				( contentListPtr->formatType == CRYPT_FORMAT_PGP ) ? \
				MAX_KEYSETUP_HASHSPECIFIER : MAX_KEYSETUP_ITERATIONS;

	assert( isReadPtr( contentListPtr, sizeof( CONTENT_LIST ) ) );
	assert( isReadPtr( contentEncrInfo, sizeof( CONTENT_ENCR_INFO ) ) );

	/* Check crypto parameters */
	if( ( contentEncrInfo->cryptAlgo != CRYPT_ALGO_NONE && \
		  !isConvAlgo( contentEncrInfo->cryptAlgo ) && \
		  !isPkcAlgo( contentEncrInfo->cryptAlgo ) ) || \
		( contentEncrInfo->cryptMode < CRYPT_MODE_NONE || \
		  contentEncrInfo->cryptMode >= CRYPT_MODE_LAST ) ) 
		{
		DEBUG_PUTS(( "sanityCheckContentList: Crypto algorithm/mode" ));
		return( FALSE );
		}
	if( contentEncrInfo->saltOrIVsize < 0 || \
		contentEncrInfo->saltOrIVsize > CRYPT_MAX_HASHSIZE || \
		contentEncrInfo->keySetupAlgo < CRYPT_ALGO_NONE || \
		contentEncrInfo->keySetupAlgo > CRYPT_ALGO_LAST_EXTERNAL || \
		contentEncrInfo->keySetupIterations < 0 || \
		contentEncrInfo->keySetupIterations > maxIterations || \
		contentEncrInfo->keySize < 0 || \
		contentEncrInfo->keySize > CRYPT_MAX_KEYSIZE )
		{
		DEBUG_PUTS(( "sanityCheckContentList: Crypto parameters" ));
		return( FALSE );
		}

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckContentSig( IN const CONTENT_LIST *contentListPtr,
									  IN_OPT const void *objectPtr )
	{
	const CONTENT_SIG_INFO *contentSigInfo = &contentListPtr->clSigInfo;

	assert( isReadPtr( contentListPtr, sizeof( CONTENT_LIST ) ) );
	assert( isReadPtr( contentSigInfo, sizeof( CONTENT_SIG_INFO ) ) );
	assert( objectPtr == NULL || isReadPtr( objectPtr, 2 ) );

	/* Check signing parameters */
	if( ( contentSigInfo->hashAlgo != CRYPT_ALGO_NONE && \
		  !isHashAlgo( contentSigInfo->hashAlgo ) ) || \
		( contentSigInfo->hashAlgoParam != 0 && \
		  ( contentSigInfo->hashAlgoParam < MIN_HASHSIZE || \
			contentSigInfo->hashAlgoParam > CRYPT_MAX_HASHSIZE ) ) || \
		( contentSigInfo->iSigCheckKey != CRYPT_ERROR && \
		  !isHandleRangeValid( contentSigInfo->iSigCheckKey ) ) )
		{
		DEBUG_PUTS(( "sanityCheckContentList: Signature algorithm info" ));
		return( FALSE );
		}

	/* Check data pointers */
	if( !checkDataPointer( contentSigInfo->extraData,
						   contentSigInfo->extraDataLength, 
						   1, MAX_INTLENGTH_SHORT - 1 ) || \
		( contentSigInfo->iTimestamp != CRYPT_ERROR && \
		  !isHandleRangeValid( contentSigInfo->iTimestamp ) ) || \
		!checkDataPointer( contentSigInfo->extraData2,
						   contentSigInfo->extraData2Length, 
						   1, MAX_INTLENGTH_SHORT - 1 ) )
		{
		DEBUG_PUTS(( "sanityCheckContentList: Signature attribute info" ));
		return( FALSE );
		}
	if( objectPtr != NULL )
		{
		if( !pointerBoundsCheck( objectPtr, contentListPtr->objectSize,
								 contentSigInfo->extraData,
								 contentSigInfo->extraDataLength ) || \
			!pointerBoundsCheck( objectPtr, contentListPtr->objectSize,
								 contentSigInfo->extraData2,
								 contentSigInfo->extraData2Length ) )
			{
			DEBUG_PUTS(( "sanityCheckContentList: Signature object data pointers" ));
			return( FALSE );
			}
		}

	/* Check miscellaneous items */
	if( !isEnumRangeOpt( contentSigInfo->attributeCursorEntry, CRYPT_ATTRIBUTE ) )
		{
		DEBUG_PUTS(( "sanityCheckContentList: Signature attribute cursor" ));
		return( FALSE );
		}

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckContentAuthenc( IN const CONTENT_LIST *contentListPtr )
	{
	const CONTENT_AUTHENC_INFO *contentAuthEncInfo = &contentListPtr->clAuthEncInfo;

	assert( isReadPtr( contentListPtr, sizeof( CONTENT_LIST ) ) );
	assert( isReadPtr( contentAuthEncInfo, sizeof( CONTENT_AUTHENC_INFO ) ) );

	/* If the content-list entry hasn't been set up yet then all parameters 
	   must be clear */
	if( contentAuthEncInfo->authEncAlgo == CRYPT_ALGO_NONE )
		{
		if( contentAuthEncInfo->authEncParamLength != 0 || \
			contentAuthEncInfo->kdfData != NULL || \
			contentAuthEncInfo->kdfDataLength != 0 || \
			contentAuthEncInfo->encParamData != NULL || \
			contentAuthEncInfo->encParamDataLength != 0 || \
			contentAuthEncInfo->macParamData != NULL || \
			contentAuthEncInfo->macParamDataLength != 0 )
			{
			DEBUG_PUTS(( "sanityCheckContentList: Authenc spurious parameters" ));
			return( FALSE );
			}

		return( TRUE );
		}

	/* Check authenc parameters */
	if( !isSpecialAlgo( contentAuthEncInfo->authEncAlgo ) ) 
		{
		DEBUG_PUTS(( "sanityCheckContentList: Authenc algorithm" ));
		return( FALSE );
		}
	if( contentAuthEncInfo->authEncParamLength <= 0 || \
		contentAuthEncInfo->authEncParamLength > 128 )
		{
		DEBUG_PUTS(( "sanityCheckContentList: Authenc parameter size" ));
		return( FALSE );
		}

	/* Check data pointers */
	if( !pointerBoundsCheck( contentAuthEncInfo->authEncParamData, 
							 contentAuthEncInfo->authEncParamLength,
							 contentAuthEncInfo->kdfData,
							 contentAuthEncInfo->kdfDataLength ) || \
		!pointerBoundsCheck( contentAuthEncInfo->authEncParamData, 
							 contentAuthEncInfo->authEncParamLength,
							 contentAuthEncInfo->encParamData,
							 contentAuthEncInfo->encParamDataLength ) || \
		!pointerBoundsCheck( contentAuthEncInfo->authEncParamData, 
							 contentAuthEncInfo->authEncParamLength,
							 contentAuthEncInfo->macParamData,
							 contentAuthEncInfo->macParamDataLength ) )
		{
		DEBUG_PUTS(( "sanityCheckContentList: Authenc object data pointers" ));
		return( FALSE );
		}

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckContentList( IN const CONTENT_LIST *contentListPtr )
	{
	const void *objectPtr = NULL;

	assert( isReadPtr( contentListPtr, sizeof( CONTENT_LIST ) ) );

	/* Check overall content list data */
	if( !isEnumRange( contentListPtr->type, CONTENT ) || \
		( contentListPtr->envInfo != CRYPT_ATTRIBUTE_NONE && \
		  ( contentListPtr->envInfo <= CRYPT_ENVINFO_FIRST || \
			contentListPtr->envInfo >= CRYPT_ENVINFO_LAST ) ) || \
		contentListPtr->formatType <= CRYPT_FORMAT_NONE || \
		contentListPtr->formatType >= CRYPT_FORMAT_LAST_EXTERNAL || \
		!CHECK_FLAGS( contentListPtr->flags, CONTENT_FLAG_NONE, 
					  CONTENT_FLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckContentList: General info" ));
		return( FALSE );
		}

	/* Check safe pointers.  We don't have to check the function pointers
	   because they're validated each time they're dereferenced */
	if( !DATAPTR_ISVALID( contentListPtr->prev ) || \
		!DATAPTR_ISVALID( contentListPtr->next ) || \
		!DATAPTR_ISVALID( contentListPtr->object ) )
		{
		DEBUG_PUTS(( "sanityCheckContentList: Safe pointers" ));
		return( FALSE );
		}

	/* Check buffer values */
	if( DATAPTR_ISNULL( contentListPtr->object ) )
		{
		if( contentListPtr->objectSize != 0 || \
			contentListPtr->issuerAndSerialNumber != NULL || \
			contentListPtr->issuerAndSerialNumberSize != 0 || \
			contentListPtr->payload != NULL || \
			contentListPtr->payloadSize != 0 )
			{
			DEBUG_PUTS(( "sanityCheckContentList: Spurious object data" ));
			return( FALSE );
			}
		}
	else
		{
		/* Get the object pointer.  We know that this is valid because it's
		   been checked above */
		objectPtr = DATAPTR_GET( contentListPtr->object );
		REQUIRES_B( objectPtr != NULL );

		if( contentListPtr->objectSize < 8 || \
			contentListPtr->objectSize >= MAX_INTLENGTH_SHORT || \
			!checkDataPointer( contentListPtr->issuerAndSerialNumber,
							   contentListPtr->issuerAndSerialNumberSize, 
							   1, MAX_INTLENGTH_SHORT - 1 ) || \
			!checkDataPointer( contentListPtr->payload,
							   contentListPtr->payloadSize, 
							   1, MAX_INTLENGTH_SHORT - 1 ) || \
			contentListPtr->keyIDsize < 0 || \
			contentListPtr->keyIDsize > CRYPT_MAX_HASHSIZE )
			{
			DEBUG_PUTS(( "sanityCheckContentList: Object data" ));
			return( FALSE );
			}
		if( !pointerBoundsCheck( objectPtr, contentListPtr->objectSize,
								 contentListPtr->issuerAndSerialNumber,
								 contentListPtr->issuerAndSerialNumberSize ) || \
			!pointerBoundsCheck( objectPtr, contentListPtr->objectSize,
								 contentListPtr->payload,
								 contentListPtr->payloadSize ) )
			{
			DEBUG_PUTS(( "sanityCheckContentList: Signature object data pointers" ));
			return( FALSE );
			}
		}

	/* Check subtype-specific data */
	switch( contentListPtr->type )
		{
		case CONTENT_CRYPT:
			if( !sanityCheckContentCrypt( contentListPtr ) )
				return( FALSE );
			break;

		case CONTENT_SIGNATURE:
			if( !sanityCheckContentSig( contentListPtr, objectPtr ) )
				return( FALSE );
			break;

		case CONTENT_AUTHENC:
			if( !sanityCheckContentAuthenc( contentListPtr ) )
				return( FALSE );
			break;

		default:
			retIntError_Boolean();
		}

	return( TRUE );
	}

/* Check whether more content items can be added to a content list */

CHECK_RETVAL_BOOL \
BOOLEAN moreContentItemsPossible( IN_OPT const CONTENT_LIST *contentListPtr )
	{
	int contentListCount, LOOP_ITERATOR;

	assert( contentListPtr == NULL || \
			isReadPtr( contentListPtr, sizeof( CONTENT_LIST ) ) );

	REQUIRES_B( contentListPtr == NULL || \
				sanityCheckContentList( contentListPtr ) );

	LOOP_EXT( contentListCount = 0,
			  contentListPtr != NULL && \
					contentListCount < MAX_CONTENT_ITEMS,
			  ( contentListPtr = DATAPTR_GET( contentListPtr->next ),
					contentListCount++ ), 
			  MAX_CONTENT_ITEMS + 1 );
	ENSURES_B( LOOP_BOUND_OK );

	return( ( contentListCount < MAX_CONTENT_ITEMS ) ? TRUE : FALSE );
	}

/* Create a content list item */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int createContentListItem( OUT_BUFFER_ALLOC_OPT( sizeof( CONTENT_LIST ) ) \
								CONTENT_LIST **newContentListItemPtrPtr,
						   INOUT MEMPOOL_STATE memPoolState, 
						   IN_ENUM( CONTENT ) const CONTENT_TYPE type,
						   IN_ENUM( CRYPT_FORMAT ) \
								const CRYPT_FORMAT_TYPE formatType,
						   IN_BUFFER_OPT( objectSize ) const void *object, 
						   IN_LENGTH_Z const int objectSize )
	{
	CONTENT_LIST *newItem;

	assert( isWritePtr( newContentListItemPtrPtr, \
						sizeof( CONTENT_LIST * ) ) );
	assert( isWritePtr( memPoolState, sizeof( MEMPOOL_STATE ) ) );
	assert( objectSize == 0 || isReadPtrDynamic( object, objectSize ) );

	REQUIRES( isEnumRange( type, CONTENT ) );
	REQUIRES( isEnumRange( formatType, CRYPT_FORMAT ) );
	REQUIRES( ( object == NULL && objectSize == 0 ) || \
			  ( object != NULL && \
				objectSize > 0 && objectSize < MAX_BUFFER_SIZE ) );

	/* Clear return value */
	*newContentListItemPtrPtr = NULL;

	if( ( newItem = getMemPool( memPoolState, \
								sizeof( CONTENT_LIST ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( newItem, 0, sizeof( CONTENT_LIST ) );
	newItem->type = type;
	newItem->formatType = formatType;
	INIT_FLAGS( newItem->flags, CONTENT_FLAG_NONE );
	DATAPTR_SET( newItem->object, ( void * ) object );
	newItem->objectSize = objectSize;
	DATAPTR_SET( newItem->prev, NULL );
	DATAPTR_SET( newItem->next, NULL );
	if( type == CONTENT_SIGNATURE )
		{
		newItem->clSigInfo.iSigCheckKey = CRYPT_ERROR;
		newItem->clSigInfo.iExtraData = CRYPT_ERROR;
		newItem->clSigInfo.iTimestamp = CRYPT_ERROR;
		}
	*newContentListItemPtrPtr = newItem;

	REQUIRES( sanityCheckContentList( newItem ) );

	return( CRYPT_OK );
	}

/* Add an item to the content list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int appendContentListItem( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						   INOUT CONTENT_LIST *contentListItem )
	{
	CONTENT_LIST *contentListPtr = \
					DATAPTR_GET( envelopeInfoPtr->contentList );

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( contentListPtr == NULL || \
			isWritePtr( contentListPtr, sizeof( CONTENT_LIST ) ) );

	REQUIRES( sanityCheckEnvelope( envelopeInfoPtr ) );

	/* Find the end of the list and add the new item */
	if( contentListPtr != NULL )
		{
		CONTENT_LIST *prevElementPtr;
		int LOOP_ITERATOR;
		
		LOOP_LARGE( prevElementPtr = NULL,
					contentListPtr != NULL,
					( prevElementPtr = contentListPtr,
					  contentListPtr = DATAPTR_GET( contentListPtr->next ) ) );
		ENSURES( LOOP_BOUND_OK );
		contentListPtr = prevElementPtr;
		}
	insertDoubleListElement( &envelopeInfoPtr->contentList, contentListPtr, 
							 contentListItem, CONTENT_LIST );

	return( CRYPT_OK );
	}

/* Delete a content list */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void deleteContentListItem( INOUT MEMPOOL_STATE memPoolState,
							INOUT CONTENT_LIST *contentListItem )
	{
	assert( isWritePtr( memPoolState, sizeof( MEMPOOL_STATE ) ) );
	assert( isWritePtr( contentListItem, sizeof( CONTENT_LIST ) ) );

	REQUIRES_V( sanityCheckContentList( contentListItem ) );

	/* Destroy any attached objects if necessary */
	if( contentListItem->type == CONTENT_SIGNATURE )
		{
		CONTENT_SIG_INFO *sigInfo = &contentListItem->clSigInfo;

		if( sigInfo->iSigCheckKey != CRYPT_ERROR )
			krnlSendNotifier( sigInfo->iSigCheckKey, IMESSAGE_DECREFCOUNT );
		if( sigInfo->iExtraData != CRYPT_ERROR )
			krnlSendNotifier( sigInfo->iExtraData, IMESSAGE_DECREFCOUNT );
		if( sigInfo->iTimestamp != CRYPT_ERROR )
			krnlSendNotifier( sigInfo->iTimestamp, IMESSAGE_DECREFCOUNT );
		}

	/* Free any object data */
	if( DATAPTR_ISSET( contentListItem->object ) )
		{
		void *objectPtr = ( void * ) DATAPTR_GET( contentListItem->object );
			 /* Although the data is declared 'const' since it can't be 
			    modified, we still have to be able to zeroise it on free so 
				we override the const for this */

		REQUIRES_V( objectPtr != NULL );

		zeroise( objectPtr, contentListItem->objectSize );
		clFree( "deleteContentListItem", objectPtr );
		}

	/* Free the item itself */
	zeroise( contentListItem, sizeof( CONTENT_LIST ) );
	freeMemPool( memPoolState, contentListItem );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
int deleteContentList( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	CONTENT_LIST *contentListCursor;
	int LOOP_ITERATOR;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheckEnvelope( envelopeInfoPtr ) );

	LOOP_LARGE_INITCHECK( contentListCursor = DATAPTR_GET( envelopeInfoPtr->contentList ), 
						  contentListCursor != NULL )
		{
		CONTENT_LIST *contentListItem = contentListCursor;

		REQUIRES( sanityCheckContentList( contentListCursor ) );

		contentListCursor = DATAPTR_GET( contentListCursor->next );

		/* Erase and free the object buffer if necessary */
		deleteDoubleListElement( &envelopeInfoPtr->contentList, 
								 contentListItem, CONTENT_LIST );
		deleteContentListItem( envelopeInfoPtr->memPoolState, 
							   contentListItem );
		}
	ENSURES( LOOP_BOUND_OK );

	/* Clear the pointers to the content list */
	DATAPTR_SET( envelopeInfoPtr->contentList, NULL );
	DATAPTR_SET( envelopeInfoPtr->contentListCurrent, NULL );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Process Signature Data 							*
*																			*
****************************************************************************/

#ifdef USE_CMS

/* Process timestamps */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processTimestamp( INOUT CONTENT_LIST *contentListPtr, 
							 IN_BUFFER( timestampLength ) const void *timestamp, 
							 IN_LENGTH_MIN( MIN_CRYPT_OBJECTSIZE ) \
								const int timestampLength )
	{
	CRYPT_ENVELOPE iTimestampEnvelope;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	const int bufSize = max( timestampLength + 128, MIN_BUFFER_SIZE );
	int status;

	assert( isWritePtr( contentListPtr, sizeof( CONTENT_LIST ) ) );
	assert( isReadPtrDynamic( timestamp, timestampLength ) );

	REQUIRES( timestampLength >= MIN_CRYPT_OBJECTSIZE && \
			  timestampLength < MAX_BUFFER_SIZE );

	/* Create an envelope to contain the timestamp data.  We can't use the
	   internal enveloping API for this because we want to retain the final
	   envelope and not just recover the data contents (which for a 
	   timestamp will be empty anyway) */
	setMessageCreateObjectInfo( &createInfo, CRYPT_FORMAT_AUTO );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_ENVELOPE );
	if( cryptStatusError( status ) )
		return( status );
	iTimestampEnvelope = createInfo.cryptHandle;

	/* Push in the timestamp data */
	status = krnlSendMessage( iTimestampEnvelope, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &bufSize, 
							  CRYPT_ATTRIBUTE_BUFFERSIZE );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) timestamp, \
						timestampLength );
		status = krnlSendMessage( iTimestampEnvelope, IMESSAGE_ENV_PUSHDATA,
								  &msgData, 0 );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( iTimestampEnvelope, IMESSAGE_ENV_PUSHDATA, 
								  &msgData, 0 );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iTimestampEnvelope, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* We've got the timestamp info in a sub-envelope, remember it for
	   later */
	contentListPtr->clSigInfo.iTimestamp = iTimestampEnvelope;

	return( CRYPT_OK );
	}

/* Process CMS unauthenticated attributes.  We can't handle these as
   standard CMS attributes since the only thing that we're likely to see 
   here is a countersignature, which isn't an attribute in the normal 
   sense */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processUnauthAttributes( INOUT CONTENT_LIST *contentListPtr,
									IN_BUFFER( unauthAttrLength ) \
										const void *unauthAttr,
									IN_LENGTH_MIN( MIN_CRYPT_OBJECTSIZE ) \
										const int unauthAttrLength )
	{
	STREAM stream;
	int status, LOOP_ITERATOR;

	assert( isWritePtr( contentListPtr, sizeof( CONTENT_LIST ) ) );
	assert( isReadPtrDynamic( unauthAttr, unauthAttrLength ) );

	REQUIRES( unauthAttrLength >= MIN_CRYPT_OBJECTSIZE && \
			  unauthAttrLength < MAX_BUFFER_SIZE );

	/* Make sure that the unauthenticated attributes are OK.  Normally this
	   is done when we import the attributes but since we can't import
	   them we have to perform the check explicitly here */
	status = checkCertObjectEncoding( unauthAttr, unauthAttrLength );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_BADDATA );

	/* Process each attribute */
	sMemConnect( &stream, unauthAttr, unauthAttrLength );
	status = readConstructed( &stream, NULL, 1 );
	LOOP_LARGE_CHECK( cryptStatusOK( status ) && \
					  sMemDataLeft( &stream ) >= MIN_CRYPT_OBJECTSIZE )
		{
		BYTE oid[ MAX_OID_SIZE + 8 ];
		void *dataPtr;
		int oidLength, length DUMMY_INIT;

		/* See what we've got */
		readSequence( &stream, NULL );
		status = readEncodedOID( &stream, oid, MAX_OID_SIZE, &oidLength, 
								 BER_OBJECT_IDENTIFIER );
		if( cryptStatusOK( status ) )
			status = readSet( &stream, &length );
		if( cryptStatusError( status ) )
			break;

		/* If it's something that we don't recognise, skip it and continue */
		if( oidLength != sizeofOID( OID_TSP_TSTOKEN ) || \
			memcmp( oid, OID_TSP_TSTOKEN, oidLength ) )
			{
			status = readUniversal( &stream );
			continue;
			}

		/* We've got a timestamp.  We can't really do much with this at the 
		   moment since although it quacks like a countersignature, in the 
		   PKIX tradition it's subtly (and gratuitously) incompatible in 
		   various ways so that it can't be verified as a standard 
		   countersignature (video meliora proboque deteriora sequor).  
		   Amusingly, the RFC actually states that this is a stupid way to 
		   do things.  Specifically, instead of using the normal MUST/SHOULD 
		   it first states that the sensible solution to the problem is to 
		   use a countersignature, and then goes on to mandate something 
		   that isn't a countersignature.  Since this isn't the sensible 
		   solution, it's obviously the stupid one.  QED */
		if( length < MIN_CRYPT_OBJECTSIZE )
			{
			/* It's too short to be a valid timestamp */
			status = CRYPT_ERROR_UNDERFLOW;
			continue;
			}
		status = sMemGetDataBlock( &stream, &dataPtr, length );
		if( cryptStatusOK( status ) )
			status = sSkip( &stream, length, MAX_INTLENGTH_SHORT );
		if( cryptStatusOK( status ) )
			status = processTimestamp( contentListPtr, dataPtr, length );
		/* Continue the loop with the cryptStatusOK() check */
		}
	ENSURES( LOOP_BOUND_OK );
	sMemDisconnect( &stream );

	return( status );
	}

/* Perform additional checks beyond those performed for a standard 
   signature as required by CMS signatures */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
static int checkCmsSignatureInfo( INOUT CONTENT_LIST *contentListPtr, 
								  IN_HANDLE const CRYPT_HANDLE iHashContext,
								  IN_HANDLE const CRYPT_HANDLE iSigCheckContext,
								  IN_ENUM( CRYPT_CONTENT ) \
									const CRYPT_CONTENT_TYPE contentType,
								  INOUT ERROR_INFO *errorInfo )
	{
	CONTENT_SIG_INFO *sigInfo = &contentListPtr->clSigInfo;
	const void *objectPtr = DATAPTR_GET( contentListPtr->object );
	int status;

	assert( isWritePtr( contentListPtr, sizeof( CONTENT_LIST ) ) );
	assert( isWritePtr( errorInfo, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( isHandleRangeValid( iSigCheckContext ) );
	REQUIRES( isEnumRange( contentType, CRYPT_CONTENT ) );
	REQUIRES( objectPtr != NULL );

	/* If it's CMS signed data then the signature check key should be 
	   included with the signed data as a certificate chain, however it's 
	   possible (though unlikely) that the certificates may be unrelated to 
	   the signature, in which case the caller will have provided the 
	   signature check key from an external source */
	status = iCryptCheckSignature( objectPtr, contentListPtr->objectSize, 
								   CRYPT_FORMAT_CMS,
								   ( sigInfo->iSigCheckKey == CRYPT_ERROR ) ? \
									iSigCheckContext : sigInfo->iSigCheckKey,
								   iHashContext, CRYPT_UNUSED,
								   &sigInfo->iExtraData );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, errorInfo, "Signature verification failed" ) );
		}

	/* If there are authenticated attributes present we have to perform an 
	   extra check to make sure that the content-type specified in the 
	   authenticated attributes matches the actual data content type */
	if( sigInfo->iExtraData != CRYPT_ERROR )
		{
		int signatureContentType;

		status = krnlSendMessage( sigInfo->iExtraData, IMESSAGE_GETATTRIBUTE, 
								  &signatureContentType, 
								  CRYPT_CERTINFO_CMS_CONTENTTYPE );
		if( cryptStatusError( status ) || \
			signatureContentType != contentType )
			{
			retExt( CRYPT_ERROR_SIGNATURE,
					( CRYPT_ERROR_SIGNATURE, errorInfo, 
					  "Content-type in authenticated attributes doesn't "
					  "match actual content type" ) );
			}
		}

	/* If there are unauthenticated attributes present, process them */
	if( sigInfo->extraData2 != NULL )
		{
		status = processUnauthAttributes( contentListPtr, sigInfo->extraData2,
										  sigInfo->extraData2Length );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Invalid unauthenticated attribute data") );
			}
		}

	return( CRYPT_OK );
	}
#endif /* USE_CMS */

/****************************************************************************
*																			*
*							Process Encryption Data							*
*																			*
****************************************************************************/

/* Add a new encryption or MAC action to the envelope's action list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int addActionToList( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							IN_ENUM( ACTION ) const ACTION_TYPE action )
	{
	ACTION_RESULT actionResult;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( action == ACTION_CRYPT || action == ACTION_MAC );

	/* Add the action to the envelope action list */
	REQUIRES( DATAPTR_ISVALID( envelopeInfoPtr->actionList ) );
	actionResult = checkAction( DATAPTR_GET( envelopeInfoPtr->actionList ), 
								action, iCryptContext );
	if( actionResult == ACTION_RESULT_ERROR || \
		actionResult == ACTION_RESULT_INITED )
		return( CRYPT_ERROR_INITED );
	return( addAction( envelopeInfoPtr, action, iCryptContext ) );
	}

#ifdef USE_CMS

/* Initialise a recovered encryption key, either directly if it's a session 
   key or indirectly if it's a generic-secret key used to derive encryption
   and MAC contexts and keys */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int initKeys( INOUT ENVELOPE_INFO *envelopeInfoPtr,
					 IN_HANDLE const CRYPT_CONTEXT iSessionKeyContext,
					 OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
					 OUT_HANDLE_OPT CRYPT_CONTEXT *iMacContext )
	{
	CRYPT_CONTEXT iAuthEncCryptContext, iAuthEncMacContext;
	CRYPT_ALGO_TYPE kdfAlgo DUMMY_INIT;
	const CONTENT_LIST *contentListPtr;
	const CONTENT_ENCR_INFO *encrInfo;
	const CONTENT_AUTHENC_INFO *authEncInfo;
	MECHANISM_KDF_INFO mechanismInfo;
	CONTENT_ENCR_INFO localEncrInfo;
	STREAM stream;
	int value, kdfAlgoParam DUMMY_INIT, status, LOOP_ITERATOR;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( iMacContext, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( isHandleRangeValid( iSessionKeyContext ) );

	/* Clear return values.  Note that we set the returned context to the
	   passed-in context since this is the default (identity) transformation,
	   it's only when using authenticated encryption that it gets set to a
	   new context */
	*iCryptContext = iSessionKeyContext;
	*iMacContext = CRYPT_ERROR;

	/* Check whether we got as far as the encrypted data, which will be 
	   indicated by the fact that there's content information present from 
	   which we can set up the decryption */
	LOOP_LARGE( contentListPtr = DATAPTR_GET( envelopeInfoPtr->contentList ), 
				contentListPtr != NULL && \
					contentListPtr->envInfo != CRYPT_ENVINFO_SESSIONKEY,
				contentListPtr = DATAPTR_GET( contentListPtr->next ) );
	ENSURES( LOOP_BOUND_OK );
	if( contentListPtr == NULL )
		{
		/* We didn't get to the encrypted data, the decryption will be set 
		   up by the de-enveloping code when we reach the data */
		return( CRYPT_OK );
		}

	/* If we're using standard (non-authenticated) encryption, the 
	   encryption parameters have been provided directly as part of the 
	   content information so we can set up the decryption and exit */
	if( contentListPtr->type != CONTENT_AUTHENC )
		{
		encrInfo = &contentListPtr->clEncrInfo;
		return( initEnvelopeEncryption( envelopeInfoPtr, iSessionKeyContext, 
								encrInfo->cryptAlgo, encrInfo->cryptMode, 
								( encrInfo->saltOrIVsize > 0 ) ? \
									encrInfo->saltOrIV : NULL, 
								encrInfo->saltOrIVsize, FALSE ) );
		}

	/* We're using authenticated encryption, in which case the "session key" 
	   that we've been given is actually a generic-secret context from which 
	   the encryption and MAC contexts and keys have to be derived */
	authEncInfo = &contentListPtr->clAuthEncInfo;

	/* Recover the KDF information if it's present */
	if( authEncInfo->kdfDataLength > 0 )
		{
		sMemConnect( &stream, authEncInfo->kdfData, 
					 authEncInfo->kdfDataLength );
		readConstructed( &stream, NULL, 0 );
		readUniversal( &stream );
		status = readShortInteger( &stream, NULL );
		if( cryptStatusOK( status ) )
			{
			status = readAlgoIDex( &stream, &kdfAlgo, NULL, &kdfAlgoParam, 
								   ALGOID_CLASS_HASH );
			}
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		/* The PBKDF2 default KDF is HMAC-SHA1 */
		kdfAlgo = CRYPT_ALGO_HMAC_SHA1;
		kdfAlgoParam = 0;
		}

	/* Recreate the encryption and MAC contexts used for the authenticated 
	   encryption from the algorithm parameter data stored with the generic-
	   secret context */
	sMemConnect( &stream, authEncInfo->encParamData, 
				 authEncInfo->encParamDataLength );
	status = readContextAlgoID( &stream, &iAuthEncCryptContext, NULL, 
								DEFAULT_TAG, ALGOID_CLASS_CRYPT );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, authEncInfo->macParamData, 
				 authEncInfo->macParamDataLength );
	status = readContextAlgoID( &stream, &iAuthEncMacContext, NULL, 
								DEFAULT_TAG, ALGOID_CLASS_HASH );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Set up the encryption parameters using the parameter data recovered
	   from the generic-secret context */
	memset( &localEncrInfo, 0, sizeof( CONTENT_ENCR_INFO ) );
	status = krnlSendMessage( iAuthEncCryptContext, IMESSAGE_GETATTRIBUTE, 
							  &value, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		{
		localEncrInfo.cryptAlgo = value;	/* int vs.enum */
		status = krnlSendMessage( iAuthEncCryptContext, IMESSAGE_GETATTRIBUTE, 
								  &value, CRYPT_CTXINFO_MODE );
		}
	if( cryptStatusOK( status ) )
		{
		MESSAGE_DATA msgData;

		localEncrInfo.cryptMode = value;	/* int vs.enum */
		setMessageData( &msgData, localEncrInfo.saltOrIV, 
						CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( iAuthEncCryptContext, IMESSAGE_GETATTRIBUTE_S, 
								  &msgData, CRYPT_CTXINFO_IV );
		if( cryptStatusOK( status ) )
			localEncrInfo.saltOrIVsize = msgData.length;
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iAuthEncMacContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	encrInfo = &localEncrInfo;

	/* Derive the encryption and MAC keys from the generic-secret key */
	setMechanismKDFInfo( &mechanismInfo, iAuthEncCryptContext, 
						 iSessionKeyContext, kdfAlgo, "encryption", 10 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_KDF,
							  &mechanismInfo, MECHANISM_DERIVE_PBKDF2 );
	if( cryptStatusOK( status ) )
		{
		setMechanismKDFInfo( &mechanismInfo, iAuthEncMacContext, 
							 iSessionKeyContext, kdfAlgo, "authentication", 14 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_KDF,
								  &mechanismInfo, MECHANISM_DERIVE_PBKDF2 );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iAuthEncMacContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* We've got the encryption context and information ready, set up the 
	   decryption */
	status = initEnvelopeEncryption( envelopeInfoPtr, 
							iAuthEncCryptContext, encrInfo->cryptAlgo, 
							encrInfo->cryptMode, encrInfo->saltOrIV, 
							encrInfo->saltOrIVsize, FALSE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iAuthEncMacContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* MAC the EncryptedContentInfo.ContentEncryptionAlgorithmIdentifier 
	   information alongside the payload data to prevent an attacker from 
	   manipulating the algorithm parameters to cause corruption that won't 
	   be detected by the MAC on the payload data */
	status = krnlSendMessage( iAuthEncMacContext, IMESSAGE_CTX_HASH,
							  ( MESSAGE_CAST ) authEncInfo->authEncParamData,
							  authEncInfo->authEncParamLength );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iAuthEncCryptContext, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iAuthEncMacContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	*iCryptContext = iAuthEncCryptContext;
	*iMacContext = iAuthEncMacContext;

	/* We're now MACing the data via a level of indirection (in other words
	   we haven't gone directly via a MACAlgorithmIdentifier in the envelope
	   header) so we need to explicitly turn on hashing */
	SET_FLAG( envelopeInfoPtr->dataFlags, 
			  ENVDATA_FLAG_AUTHENCACTIONSACTIVE );

	/* Let the caller know that the contexts have been switched */
	return( OK_SPECIAL );
	}
#endif /* USE_CMS */

/* Set up the envelope decryption using an added or recovered session key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initSessionKeyDecryption( INOUT ENVELOPE_INFO *envelopeInfoPtr,
									 IN_HANDLE \
										const CRYPT_CONTEXT iSessionKeyContext,
									 const BOOLEAN isRecoveredSessionKey )
	{
	CRYPT_CONTEXT iCryptContext = iSessionKeyContext;
	CRYPT_CONTEXT iMacContext = CRYPT_ERROR;
	BOOLEAN isAuthEnc = FALSE;
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( isHandleRangeValid( iSessionKeyContext ) );
	REQUIRES( isRecoveredSessionKey == TRUE || isRecoveredSessionKey == FALSE );

	/* If we recovered the session key from a key exchange action rather 
	   than having it passed directly to us by the user, try and set up the 
	   decryption */
	if( isRecoveredSessionKey )
		{
		status = initKeys( envelopeInfoPtr, iSessionKeyContext,
						   &iCryptContext, &iMacContext );
		if( cryptStatusError( status ) )
			{
			if( status != OK_SPECIAL )
				return( status );

			/* A return status of OK_SPECIAL means that the context has
			   changed from a single 'session-key' context containing a
			   generic secret to two new contexts, one for encryption and
			   the other for authentication */
			isAuthEnc = TRUE;
			}
		}

	/* Add the recovered session encryption action to the action list */
	status = addActionToList( envelopeInfoPtr, iCryptContext, ACTION_CRYPT );
	if( cryptStatusOK( status ) && isAuthEnc )
		status = addActionToList( envelopeInfoPtr, iMacContext, ACTION_MAC );
	if( cryptStatusError( status ) )
		{
		if( isAuthEnc )
			{
			krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
			krnlSendNotifier( iMacContext, IMESSAGE_DECREFCOUNT );
			}
		return( status );
		}

	/* If we're using authenticated encryption (via CMS's encrypt-then-MAC
	   rather than PGP's encrypted sort-of-keyed hash) then the generic-
	   secret context that was recovered as the 'session-key' has been 
	   turned into two new contexts, one for encryption and the other for 
	   authentication, and we can destroy it */
	if( envelopeInfoPtr->usage == ACTION_CRYPT && \
		TEST_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_AUTHENC ) && \
		envelopeInfoPtr->type != CRYPT_FORMAT_PGP )
		{
		REQUIRES( iSessionKeyContext != iCryptContext );

		krnlSendNotifier( iSessionKeyContext, IMESSAGE_DECREFCOUNT );
		}

	/* Notify the kernel that the encryption/MAC context is attached to the 
	   envelope.  This is an internal object used only by the envelope so we 
	   tell the kernel not to increment its reference count when it attaches 
	   it */
	return( krnlSendMessage( envelopeInfoPtr->objectHandle, 
							 IMESSAGE_SETDEPENDENT, 
							 ( MESSAGE_CAST ) &iCryptContext, 
							 SETDEP_OPTION_NOINCREF ) );
	}

/* Import a wrapped session key (optionally a generic-secret key if we're
   going via an intermediate step for authenticated encryption) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int importSessionKey( IN const CONTENT_LIST *contentListPtr,
							 IN_HANDLE const CRYPT_CONTEXT iImportContext,
							 OUT_HANDLE_OPT CRYPT_CONTEXT *iSessionKeyContext,
							 INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CONTEXT iSessionKey;
	const CONTENT_LIST *sessionKeyInfoPtr;
	const void *objectPtr = DATAPTR_GET( contentListPtr->object );
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status, LOOP_ITERATOR;

	assert( isReadPtr( contentListPtr, sizeof( CONTENT_LIST ) ) );
	assert( isWritePtr( iSessionKeyContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( isHandleRangeValid( iImportContext ) );
	REQUIRES( objectPtr != NULL );

	/* Clear return value */
	*iSessionKeyContext = CRYPT_ERROR;

#ifdef USE_PGP
	/* PGP doesn't provide separate session key information with the
	   encrypted data but wraps it up alongside the encrypted key so we
	   can't import the wrapped key into a context via the standard key
	   import functions but instead have to create the context as part of
	   the unwrap process */
	if( contentListPtr->formatType == CRYPT_FORMAT_PGP )
		{
		return( iCryptImportKey( objectPtr, contentListPtr->objectSize,
								 CRYPT_FORMAT_PGP, iImportContext,
								 CRYPT_UNUSED, iSessionKeyContext ) );
		}
#endif /* USE_PGP */

	/* Look for the information required to recreate the session key (or
	   generic-secret) context */
	LOOP_LARGE( sessionKeyInfoPtr = contentListPtr, 
				sessionKeyInfoPtr != NULL && \
					sessionKeyInfoPtr->envInfo != CRYPT_ENVINFO_SESSIONKEY,
				sessionKeyInfoPtr = DATAPTR_GET( sessionKeyInfoPtr->next ) );
	ENSURES( LOOP_BOUND_OK );
	if( sessionKeyInfoPtr == NULL )
		{
		/* We need to read more data before we can recreate the session key */
		return( CRYPT_ERROR_UNDERFLOW );
		}

	/* Create the session/generic-secret key context */
	if( sessionKeyInfoPtr->type == CONTENT_CRYPT )
		{
		const CONTENT_ENCR_INFO *encrInfo = &sessionKeyInfoPtr->clEncrInfo;

		/* It's conventional encrypted data, import the session key and
		   set the encryption mode */
		setMessageCreateObjectInfo( &createInfo, encrInfo->cryptAlgo );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, errorInfo,
					  "Couldn't create decryption context for algorithm %s",
					  getAlgoName( encrInfo->cryptAlgo ) ) );
			}
		if( !isStreamCipher( encrInfo->cryptAlgo ) )
			{
			const int mode = encrInfo->cryptMode;	/* int vs.enum */

			status = krnlSendMessage( createInfo.cryptHandle, 
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &mode,
									  CRYPT_CTXINFO_MODE );
			if( cryptStatusError( status ) )
				{
				krnlSendNotifier( createInfo.cryptHandle, 
								  IMESSAGE_DECREFCOUNT );
				return( status );
				}
			}
		}
	else
		{
		const CONTENT_AUTHENC_INFO *authEncInfo = \
							&sessionKeyInfoPtr->clAuthEncInfo;

		/* It's authenticated-encrypted data, import the generic-secret
		   context used to create the encryption and MAC contexts */
		setMessageCreateObjectInfo( &createInfo, authEncInfo->authEncAlgo );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, errorInfo,
					  "Couldn't create decryption context for algorithm %s",
					  getAlgoName( authEncInfo->authEncAlgo ) ) );
			}
		}
	iSessionKey = createInfo.cryptHandle;

	/* Import the wrapped session/generic-secret key */
	status = iCryptImportKey( objectPtr, contentListPtr->objectSize,
							  contentListPtr->formatType, iImportContext, 
							  iSessionKey, NULL );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iSessionKey, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iSessionKeyContext = iSessionKey;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Add De-enveloping Information 					*
*																			*
****************************************************************************/

/* Add signature verification information.  Note that the hashAlgo parameter
   is an int instead of the more obvious CRYPT_ALGO_TYPE because this 
   function is a function parameter of type CHECKACTIONFUNCTION for which 
   the second argument is a generic integer parameter */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int findHashActionFunction( IN const ACTION_LIST *actionListPtr,
								   IN_ALGO const int hashAlgo )
	{
	int actionCryptAlgo, status;

	assert( isReadPtr( actionListPtr, sizeof( ACTION_LIST ) ) );

	REQUIRES( isHashAlgo( hashAlgo ) );

	/* Check to see if it's the action that we want */
	status = krnlSendMessage( actionListPtr->iCryptHandle,
							  IMESSAGE_GETATTRIBUTE, &actionCryptAlgo,
							  CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR );
	return( ( actionCryptAlgo == hashAlgo ) ? CRYPT_OK : CRYPT_ERROR );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int addSignatureInfo( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							 INOUT CONTENT_LIST *contentListPtr,
							 IN_HANDLE const CRYPT_HANDLE sigCheckContext,
							 const BOOLEAN isExternalKey )
	{
	const ACTION_LIST *actionListPtr = \
						DATAPTR_GET( envelopeInfoPtr->actionList );
	CONTENT_SIG_INFO *sigInfo = &contentListPtr->clSigInfo;
	const void *objectPtr = DATAPTR_GET( contentListPtr->object );
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( contentListPtr, sizeof( CONTENT_LIST ) ) );

	REQUIRES( isHandleRangeValid( sigCheckContext ) );
	REQUIRES( isExternalKey == TRUE || isExternalKey == FALSE );
	REQUIRES( actionListPtr != NULL );
	REQUIRES( objectPtr != NULL );

	/* If we've already processed this entry, return the cached processing 
	   result */
	if( TEST_FLAG( contentListPtr->flags, CONTENT_FLAG_PROCESSED ) )
		return( sigInfo->processingResult );

	/* Find the hash action that we need to check this signature.  If we 
	   can't find one, return a bad signature error since something must 
	   have altered the algorithm ID for the hash */
	actionListPtr = findActionIndirect( actionListPtr, findHashActionFunction,
										sigInfo->hashAlgo );
	if( actionListPtr == NULL || actionListPtr->action != ACTION_HASH )
		{
		SET_FLAG( contentListPtr->flags, CONTENT_FLAG_PROCESSED );
		sigInfo->processingResult = CRYPT_ERROR_SIGNATURE;
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, ENVELOPE_ERRINFO,
				  "Signature hash algorithm doesn't match hash algorithm "
				  "applied to enveloped data" ) );
		}

	/* Check the signature.  In theory there's an additional check that we 
	   need to apply at this point that defends against a (hypothesised) 
	   attack in which, if there are multiple signatures present and they 
	   use different-strength hash algorithms and an attacker manages to 
	   break one of them, the attacker can strip the stronger-algorithm 
	   signature(s) and leave only the weaker-algorithm one(s), allowing 
	   them to modify the signed data.  The way to handle this is to include 
	   a reference to every other signature in the current signature.  
	   However there are (currently) no known implementations of this, which 
	   makes testing somewhat difficult.  In addition the handling gets very 
	   tricky, for example if the recipient supports only the weak algorithm 
	   should they reject the message or accept it?  (The RFC that covers 
	   this, RFC 5750, says that no matter what occurs in terms of absent or 
	   present strong or weak-algorithm signatures, the recipient MAY 
	   consider them valid).  Until both implementations, and more 
	   importantly users who can specify how they want this handled, appear, 
	   we leave it for future implementation */
#ifdef USE_CMS
	if( contentListPtr->formatType == CRYPT_FORMAT_CMS )
		{
		status = checkCmsSignatureInfo( contentListPtr, 
										actionListPtr->iCryptHandle,
										sigCheckContext,
										envelopeInfoPtr->contentType,
										ENVELOPE_ERRINFO );
		}
	else
#endif /* USE_CMS */
		{
		status = iCryptCheckSignature( objectPtr, contentListPtr->objectSize,
								contentListPtr->formatType, sigCheckContext,
								actionListPtr->iCryptHandle, CRYPT_UNUSED, 
								NULL );
		if( cryptStatusError( status ) )
			{
			/* We need to do this explicitly here since it's not set by
			   iCryptCheckSignature() as it is for checkCmsSignatureInfo() */
			setErrorString( ENVELOPE_ERRINFO, 
							"Signature verification failed", 29 );
			}

		/* If it's a format that includes signing key information remember 
		   the key that was used to check the signature in case the user 
		   wants to query it later */
		if( contentListPtr->formatType != CRYPT_FORMAT_PGP )
			{
			krnlSendNotifier( sigCheckContext, IMESSAGE_INCREFCOUNT );
			sigInfo->iSigCheckKey = sigCheckContext;
			if( isExternalKey )
				SET_FLAG( contentListPtr->flags, CONTENT_FLAG_EXTERNALKEY );
			}
		}

	/* There are a few special-case situations in which a failure at this
	   point isn't necessarily fatal, but it's hard to predict in advance 
	   exactly what all of these could be.  The one obvious one is with a
	   CRYPT_ERROR_WRONGKEY, which means that the caller can simply retry 
	   with a different key, so we make this error non-persistent */
	if( status == CRYPT_ERROR_WRONGKEY )
		{
		retExt( CRYPT_ERROR_WRONGKEY,
				( CRYPT_ERROR_WRONGKEY, ENVELOPE_ERRINFO, 
				  "Incorrect key used to verify signature" ) );
		}

	/* Remember the processing result so that we don't have to repeat the 
	   processing if queried again.  Since we don't need the encoded 
	   signature data any more after this point we can free it.  This is
	   rather more complex than it seems because the content list contains 
	   various pointers to ID and attribute information inside the object
	   data, to deal with this we have to go through and clear every pointer
	   value that pointed to the now-freed object data */
	clFree( "addSignatureInfo", ( void * ) objectPtr );
	DATAPTR_SET( contentListPtr->object, NULL );
	contentListPtr->objectSize = 0;
	contentListPtr->issuerAndSerialNumber = \
			contentListPtr->payload = NULL;
	contentListPtr->issuerAndSerialNumberSize = \
			contentListPtr->payloadSize = 0;
	switch( contentListPtr->type )
		{
		case CONTENT_CRYPT:
			/* All information is stored in the encryption content 
			   structure */
			break;

		case CONTENT_SIGNATURE:
			{
			CONTENT_SIG_INFO *contentSigInfo = &contentListPtr->clSigInfo;

			contentSigInfo->extraData = \
					contentSigInfo->extraData2 = NULL;
			contentSigInfo->extraDataLength = \
					contentSigInfo->extraData2Length = 0;
			break;
			}

		case CONTENT_AUTHENC:
			{
			CONTENT_AUTHENC_INFO *contentAuthEncInfo = \
											&contentListPtr->clAuthEncInfo;

			contentAuthEncInfo->kdfData = \
					contentAuthEncInfo->encParamData = \
					contentAuthEncInfo->macParamData = NULL;
			contentAuthEncInfo->kdfDataLength = \
					contentAuthEncInfo->encParamDataLength = \
					contentAuthEncInfo->macParamDataLength = 0;
			break;
			}

		default:
			retIntError_Boolean();
		}
	SET_FLAG( contentListPtr->flags, CONTENT_FLAG_PROCESSED );
	sigInfo->processingResult = cryptArgError( status ) ? \
								CRYPT_ERROR_SIGNATURE : status;

	ENSURES( sanityCheckContentList( contentListPtr ) );

	return( status );
	}

/* Add a password for decryption of a private key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int addPrivkeyPasswordInfo( INOUT ENVELOPE_INFO *envelopeInfoPtr,
								   IN const CONTENT_LIST *contentListPtr,
								   IN_BUFFER( passwordLength ) const void *password, 
								   IN_LENGTH_TEXT const int passwordLength )
	{
	const ENV_ADDINFO_FUNCTION addInfoFunction = \
				( ENV_ADDINFO_FUNCTION ) \
				FNPTR_GET( envelopeInfoPtr->addInfoFunction );
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	int type, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( contentListPtr, sizeof( CONTENT_LIST ) ) );
	assert( isReadPtrDynamic( password, passwordLength ) );

	REQUIRES( passwordLength > 0 && passwordLength <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( addInfoFunction != NULL );

	/* Make sure that there's a keyset available to pull the key from */
	if( envelopeInfoPtr->iDecryptionKeyset == CRYPT_ERROR )
		{
		setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_KEYSET_DECRYPT,
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_NOTINITED );
		}

	/* Make sure that we're trying to send the password to something for
	   which it makes sense.  Private-key sources aren't just keysets but
	   can also be devices, but if we're trying to send a password to a 
	   device to get a private key then something's gone wrong since it 
	   should be retrieved automatically from the device, which was unlocked
	   via a PIN or password when a session with it was established */
	status = krnlSendMessage( envelopeInfoPtr->iDecryptionKeyset,
							  IMESSAGE_GETATTRIBUTE, &type,
							  CRYPT_IATTRIBUTE_TYPE );
	if( cryptStatusError( status ) || type != OBJECT_TYPE_KEYSET )
		{
		/* This one is very difficult to report appropriately, the best that
		   we can do is report the wrong key for this type of object */
		setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_KEYSET_DECRYPT,
					  CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ERROR_WRONGKEY );
		}

	/* Try and get the key information */
	if( contentListPtr->issuerAndSerialNumber == NULL )
		{
		setMessageKeymgmtInfo( &getkeyInfo,
				( contentListPtr->formatType == CRYPT_FORMAT_PGP ) ? \
				CRYPT_IKEYID_PGPKEYID : CRYPT_IKEYID_KEYID,
				contentListPtr->keyID, contentListPtr->keyIDsize,
				( MESSAGE_CAST ) password, passwordLength, 
				KEYMGMT_FLAG_USAGE_CRYPT );
		}
	else
		{
		setMessageKeymgmtInfo( &getkeyInfo,
				CRYPT_IKEYID_ISSUERANDSERIALNUMBER,
				contentListPtr->issuerAndSerialNumber,
				contentListPtr->issuerAndSerialNumberSize,
				( MESSAGE_CAST ) password, passwordLength, 
				KEYMGMT_FLAG_USAGE_CRYPT );
		}
	status = krnlSendMessage( envelopeInfoPtr->iDecryptionKeyset,
							  IMESSAGE_KEY_GETKEY, &getkeyInfo,
							  KEYMGMT_ITEM_PRIVATEKEY );
	if( cryptStatusError( status ) )
		{
		retExtObj( status,
				   ( status, ENVELOPE_ERRINFO,
				     envelopeInfoPtr->iDecryptionKeyset,
					 "Couldn't retrieve private key from decryption "
					 "keyset/device" ) );
		}

	/* We managed to get the private key, push it into the envelope.  If the
	   call succeeds this will import the session key and delete the 
	   required-information list, after which we don't need the private key
	   any more */
	status = addInfoFunction( envelopeInfoPtr, CRYPT_ENVINFO_PRIVATEKEY,
							  getkeyInfo.cryptHandle );
	krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	return( status );
	}

/* Add a decryption password */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 7 ) ) \
static int addPasswordInfo( IN const CONTENT_LIST *contentListPtr,
							IN_BUFFER( passwordLength ) const void *password, 
							IN_RANGE( 1, CRYPT_MAX_TEXTSIZE ) \
								const int passwordLength,
							IN_HANDLE_OPT const CRYPT_CONTEXT iMacContext,
							OUT_OPT_HANDLE_OPT CRYPT_CONTEXT *iNewContext,
							IN_ENUM( CRYPT_FORMAT ) \
								const CRYPT_FORMAT_TYPE formatType,
							INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CONTEXT iCryptContext;
	const CONTENT_ENCR_INFO *encrInfo = &contentListPtr->clEncrInfo;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int mode, status;

	assert( isReadPtr( contentListPtr, sizeof( CONTENT_LIST ) ) );
	assert( isReadPtrDynamic( password, passwordLength ) );
	assert( ( isHandleRangeValid( iMacContext ) && iNewContext == NULL ) || \
			( iMacContext == CRYPT_UNUSED && \
			  isWritePtr( iNewContext, sizeof( CRYPT_CONTEXT ) ) ) );
	assert( isWritePtr( errorInfo, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( ( isHandleRangeValid( iMacContext ) && iNewContext == NULL ) || \
			  ( iMacContext == CRYPT_UNUSED && iNewContext != NULL ) );
	REQUIRES( isEnumRange( formatType, CRYPT_FORMAT ) );
	REQUIRES( formatType != CRYPT_FORMAT_PGP || iNewContext != NULL );
			  /* PGP can't perform MACing, only encryption */

	/* Clear return value */
	if( iNewContext != NULL )
		*iNewContext = CRYPT_ERROR;

	/* Create the appropriate encryption context and derive the key into it */
	setMessageCreateObjectInfo( &createInfo, encrInfo->cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT, 
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iCryptContext = createInfo.cryptHandle;
	mode = encrInfo->cryptMode;	/* int vs.enum */
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &mode, 
							  CRYPT_CTXINFO_MODE );
	if( cryptStatusOK( status ) )
		{
#ifdef USE_PGP
		if( formatType == CRYPT_FORMAT_PGP )
			{
			status = pgpPasswordToKey( iCryptContext, CRYPT_UNUSED,
							password, passwordLength, encrInfo->keySetupAlgo,
							( encrInfo->saltOrIVsize > 0 ) ? \
								encrInfo->saltOrIV : NULL, encrInfo->saltOrIVsize,
							encrInfo->keySetupIterations );
			}
		else
#endif /* USE_PGP */
			{
			MESSAGE_DATA msgData;

			/* Load the derivation information into the context */
			if( encrInfo->keySetupAlgo != CRYPT_ALGO_NONE )
				{
				const int algorithm = encrInfo->keySetupAlgo;	/* int vs.enum */
				status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &algorithm,
										  CRYPT_CTXINFO_KEYING_ALGO );
				}
			if( cryptStatusOK( status ) )
				{
				status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &encrInfo->keySetupIterations,
										  CRYPT_CTXINFO_KEYING_ITERATIONS );
				}
			if( cryptStatusOK( status ) && encrInfo->keySetupAlgoParam > 0 )
				{
				status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &encrInfo->keySetupAlgoParam,
										  CRYPT_IATTRIBUTE_KEYING_ALGO_PARAM );
				}
			if( cryptStatusOK( status ) && encrInfo->keySize > 0 )
				{
				status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &encrInfo->keySize,
										  CRYPT_CTXINFO_KEYSIZE );
				}
			if( cryptStatusOK( status ) )
				{
				setMessageData( &msgData, ( MESSAGE_CAST ) encrInfo->saltOrIV,
								encrInfo->saltOrIVsize );
				status = krnlSendMessage( iCryptContext,
									IMESSAGE_SETATTRIBUTE_S, &msgData,
									CRYPT_CTXINFO_KEYING_SALT );
				}
			if( cryptStatusOK( status ) )
				{
				setMessageData( &msgData, ( MESSAGE_CAST ) password, 
								passwordLength );
				status = krnlSendMessage( iCryptContext,
									IMESSAGE_SETATTRIBUTE_S, &msgData,
									CRYPT_CTXINFO_KEYING_VALUE );
				}
			}
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		retExt( status,
				( status, errorInfo,
				  "Couldn't derive key-import key from password" ) );
		}

	/* In PGP there isn't any encrypted session key so the context created 
	   from the password becomes the bulk encryption context and we're done */
	if( formatType == CRYPT_FORMAT_PGP )
		{
		*iNewContext = iCryptContext;

		return( CRYPT_OK );
		}

	/* Recover the session key using the password context and destroy it 
	   when we're done with it */
	if( iNewContext == NULL )
		{
		const void *objectPtr = DATAPTR_GET( contentListPtr->object );

		REQUIRES( objectPtr != NULL );

		/* The target is a MAC context (which has already been set up), load
		   the session key directly into it */
		status = iCryptImportKey( objectPtr, contentListPtr->objectSize,
								  contentListPtr->formatType, 
								  iCryptContext, iMacContext, NULL );
		}
	else
		{
		/* The target is an encryption context, recreate it from the 
		   encrypted session key information */
		status = importSessionKey( contentListPtr, iCryptContext, 
								   iNewContext, errorInfo );
		}
	krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, errorInfo,
				  "Couldn't recover wrapped session key" ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					De-enveloping Information Management Functions			*
*																			*
****************************************************************************/

/* Try and match what's being added to an information object in the content 
   list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int matchInfoObject( OUT_PTR_OPT CONTENT_LIST **contentListPtrPtr,
							IN const ENVELOPE_INFO *envelopeInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE envInfo )
	{
	CONTENT_LIST *contentListPtr;
	const BOOLEAN privateKeyFetch = \
		( envInfo == CRYPT_ENVINFO_PASSWORD && \
		  envelopeInfoPtr->iDecryptionKeyset != CRYPT_ERROR ) ? TRUE : FALSE;
	int LOOP_ITERATOR;

	assert( isWritePtr( contentListPtrPtr, sizeof( CONTENT_LIST * ) ) );
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( envInfo == CRYPT_IATTRIBUTE_ATTRONLY || \
			  ( envInfo > CRYPT_ENVINFO_FIRST && \
				envInfo < CRYPT_ENVINFO_LAST ) );

	/* Clear return value */
	*contentListPtrPtr = NULL;

	/* If we're adding meta-information there's nothing to check */
	if( envInfo == CRYPT_IATTRIBUTE_ATTRONLY || \
		envInfo == CRYPT_ENVINFO_DETACHEDSIGNATURE || \
		envInfo == CRYPT_ENVINFO_KEYSET_SIGCHECK || \
		envInfo == CRYPT_ENVINFO_KEYSET_ENCRYPT || \
		envInfo == CRYPT_ENVINFO_KEYSET_DECRYPT || \
		envInfo == CRYPT_ENVINFO_HASH )
		return( CRYPT_OK );

	/* If there's already a content-list item selected, make sure that the 
	   information that we're adding matches the current information object.  
	   The one exception to this is that we can be passed password 
	   information when we require a private key if the private key is 
	   encrypted */
	REQUIRES( DATAPTR_ISVALID( envelopeInfoPtr->contentListCurrent ) );
	contentListPtr = DATAPTR_GET( envelopeInfoPtr->contentListCurrent );
	if( contentListPtr != NULL )
		{
		if( contentListPtr->envInfo != envInfo && \
			!( contentListPtr->envInfo == CRYPT_ENVINFO_PRIVATEKEY && \
			   privateKeyFetch ) )
			return( CRYPT_ARGERROR_VALUE );

		*contentListPtrPtr = contentListPtr;
		return( CRYPT_OK );
		}

	/* Look for the first information object that matches the supplied 
	   information */
	LOOP_LARGE( contentListPtr = DATAPTR_GET( envelopeInfoPtr->contentList ), 
				contentListPtr != NULL && contentListPtr->envInfo != envInfo,
				contentListPtr = DATAPTR_GET( contentListPtr->next ) );
	ENSURES( LOOP_BOUND_OK );
	if( contentListPtr == NULL && privateKeyFetch )
		{
		/* If we didn't find a direct match and we've been given a password, 
		   check for a private key that can (potentially) be decrypted using 
		   the password.  This requires both a keyset/device to fetch the 
		   key from and a private key as the required info type */
		LOOP_LARGE( contentListPtr = DATAPTR_GET( envelopeInfoPtr->contentList ), 
					contentListPtr != NULL && \
						contentListPtr->envInfo != CRYPT_ENVINFO_PRIVATEKEY,
					contentListPtr = DATAPTR_GET( contentListPtr->next ) );
		ENSURES( LOOP_BOUND_OK );
		}
	if( contentListPtr == NULL )
		return( CRYPT_ARGERROR_VALUE );

	ENSURES( sanityCheckContentList( contentListPtr ) );

	*contentListPtrPtr = contentListPtr;
	return( CRYPT_OK );
	}

/* Complete the addition of information to an envelope */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int completeEnvelopeInfoUpdate( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Destroy the content list, which at this point will contain only (now-
	   irrelevant) key exchange items */
	if( DATAPTR_ISSET( envelopeInfoPtr->contentList ) )
		deleteContentList( envelopeInfoPtr );

	/* If the only error was an information required error, we've now
	   resolved the problem and can continue */
	if( envelopeInfoPtr->errorState == CRYPT_ENVELOPE_RESOURCE )
		{
		envelopeInfoPtr->errorState = CRYPT_OK;

		/* The envelope is ready to process data, move it into the high
		   state.  Normally this is handled in the data-push code but this
		   leads to a race condition when all the data being pushed is
		   buffered inside the envelope, requiring only that processing be
		   enabled by adding a resource.  Once the resource is added the
		   only action left for the caller to perform is a flush, so they 
		   expect that high-state actions should succeed even though the
		   envelope state machine wouldn't move the envelope into the high
		   state until some data action (in this case a flush) is 
		   initiated.  To avoid this problem we move the envelope into the
		   high state as soon as the resource blockage has been cleared,
		   since any high-state-only information (for example the nested
		   content type) will now be available */
		return( krnlSendMessage( envelopeInfoPtr->objectHandle, 
								 IMESSAGE_SETATTRIBUTE, MESSAGE_VALUE_UNUSED, 
								 CRYPT_IATTRIBUTE_INITIALISED ) );
		}

	return( CRYPT_OK );
	}

/* Add de-enveloping information to an envelope */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int addDeenvelopeInfo( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE envInfo,
							  IN_INT_Z const int value )
	{
	CRYPT_HANDLE cryptHandle = ( CRYPT_HANDLE ) value;
	CRYPT_CONTEXT iNewContext DUMMY_INIT;
	CRYPT_ATTRIBUTE_TYPE localEnvInfo = envInfo;
	const ACTION_LIST *actionListPtr = \
						DATAPTR_GET( envelopeInfoPtr->actionList );
	CONTENT_LIST *contentListPtr;
	BOOLEAN isExternalKey = TRUE;
	int status = CRYPT_OK;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheckEnvelope( envelopeInfoPtr ) );
	REQUIRES( envInfo == CRYPT_IATTRIBUTE_ATTRONLY || \
			  ( envInfo > CRYPT_ENVINFO_FIRST && \
				envInfo < CRYPT_ENVINFO_LAST ) );

	/* A signature-check object can be passed in as a special-case type
	   CRYPT_ENVINFO_SIGNATURE_RESULT to indicate that the object was 
	   obtained internally (for example by instantiating it from an attached 
	   certificate chain) and doesn't require various special-case 
	   operations that are applied to user-supplied objects.  If this is the 
	   case then we convert it to a standard CRYPT_ENVINFO_SIGNATURE and 
	   remember that it's an internally-supplied key */
	if( envInfo == CRYPT_ENVINFO_SIGNATURE_RESULT )
		{
		localEnvInfo = CRYPT_ENVINFO_SIGNATURE;
		isExternalKey = FALSE;
		}

	/* Since we can add one of a multitude of necessary information types 
	   we need to check to make sure that what we're adding is appropriate.  
	   We do this by trying to match what's being added to the first 
	   information object of the correct type */
	status = matchInfoObject( &contentListPtr, envelopeInfoPtr, 
							  localEnvInfo );
	if( cryptStatusError( status ) )
		{
		retExtArg( status,
				   ( status, ENVELOPE_ERRINFO,
					 "Added item doesn't match %s envelope information "
					 "object",
					 ( DATAPTR_ISSET( envelopeInfoPtr->contentListCurrent ) ) ? \
						"currently selected" : "any" ) );
		}

	/* Process non-encryption-related enveloping info */
	switch( localEnvInfo )
		{
		case CRYPT_IATTRIBUTE_ATTRONLY:
			/* This is off by default so we should only be turning it on */
			REQUIRES( value == TRUE );

			SET_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_ATTRONLY );
			return( CRYPT_OK );

		case CRYPT_ENVINFO_DETACHEDSIGNATURE:
			SET_FLAG( envelopeInfoPtr->flags, ENVELOPE_FLAG_DETACHED_SIG );
			return( CRYPT_OK );

		case CRYPT_ENVINFO_KEYSET_SIGCHECK:
		case CRYPT_ENVINFO_KEYSET_ENCRYPT:
		case CRYPT_ENVINFO_KEYSET_DECRYPT:
			/* It's keyset information, keep a record of it for later use */
			return( addKeysetInfo( envelopeInfoPtr, localEnvInfo, 
								   cryptHandle ) );

		case CRYPT_ENVINFO_HASH:
			/* The user is checking a detached signature, remember the hash 
			   for later.  In theory we should also check the state of the 
			   hash context, however PGP requires that it not be completed 
			   (since it needs to hash further data) and everything else 
			   requires that it be completed, but we don't know at this 
			   point whether we're processing PGP or non-PGP data so we 
			   can't perform any checking here */
			if( actionListPtr != NULL )
				{
				/* There's already a hash action present, we can't add 
				   anything further */
				setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_HASH,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}

			/* In theory at this point we should call moreActionsPossible(), 
			   however since we know that actionListPtr == NULL we can 
			   always add more actions */

			/* Add the hash as an action list item */
			status = addAction( envelopeInfoPtr, ACTION_HASH, cryptHandle );
			if( cryptStatusError( status ) )
				return( status );
			return( krnlSendNotifier( cryptHandle, IMESSAGE_INCREFCOUNT ) );

		case CRYPT_ENVINFO_SIGNATURE:
			/* It's a signature object, check the signature and exit */
			REQUIRES( contentListPtr != NULL );
			return( addSignatureInfo( envelopeInfoPtr, contentListPtr,
									  cryptHandle, isExternalKey ) );
		}

	/* Make sure that we can still add another action */
	if( !moreActionsPossible( actionListPtr ) )
		return( CRYPT_ERROR_OVERFLOW );

	/* Since we're performing envelope decryption, there must be content 
	   info present */
	REQUIRES( contentListPtr != NULL );

	/* Anything that's left at this point related to envelope decryption */
	switch( localEnvInfo )
		{
		case CRYPT_ENVINFO_PRIVATEKEY:
		case CRYPT_ENVINFO_KEY:
			/* Import the session key using the KEK */
			status = importSessionKey( contentListPtr, cryptHandle, 
									   &iNewContext, ENVELOPE_ERRINFO );
			break;

		case CRYPT_ENVINFO_SESSIONKEY:
			{
			/* If we've been given the session key directly then we must 
			   have reached the encrypted data so we take a copy and set 
			   up the decryption with it */
			const CONTENT_ENCR_INFO *encrInfo = &contentListPtr->clEncrInfo;

			status = initEnvelopeEncryption( envelopeInfoPtr, cryptHandle,
							encrInfo->cryptAlgo, encrInfo->cryptMode,
							encrInfo->saltOrIV, encrInfo->saltOrIVsize, TRUE );
			if( cryptStatusOK( status ) )
				{
				/* The session key context is the newly-created internal 
				   one */
				iNewContext = envelopeInfoPtr->iCryptContext;
				}
			break;
			}

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );

	/* We've now got the session key, if we recovered it from a key exchange
	   action (rather than having it passed directly to us by the user) try
	   and set up the decryption */
	if( envelopeInfoPtr->usage != ACTION_MAC )
		{
		status = initSessionKeyDecryption( envelopeInfoPtr, iNewContext,
							( localEnvInfo != CRYPT_ENVINFO_SESSIONKEY ) ? \
								TRUE : FALSE );
		if( cryptStatusError( status ) )
			{
			if( localEnvInfo != CRYPT_ENVINFO_SESSIONKEY )
				krnlSendNotifier( iNewContext, IMESSAGE_DECREFCOUNT );
			if( status == CRYPT_ERROR_INITED )
				{
				/* If the attribute that we added to recover the session key 
				   is already present, provide extended error information */
				setErrorInfo( envelopeInfoPtr, localEnvInfo, 
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				}
			return( status );
			}
		}

	/* Complete the envelope information update */
	return( completeEnvelopeInfoUpdate( envelopeInfoPtr ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int addDeenvelopeInfoString( INOUT ENVELOPE_INFO *envelopeInfoPtr,
									IN_RANGE( CRYPT_ENVINFO_PASSWORD, \
											  CRYPT_ENVINFO_PASSWORD ) \
										const CRYPT_ATTRIBUTE_TYPE envInfo,
									IN_BUFFER( valueLength ) const void *value, 
									IN_RANGE( 1, CRYPT_MAX_TEXTSIZE ) \
										const int valueLength )
	{
	CRYPT_CONTEXT iNewContext DUMMY_INIT;
	const ACTION_LIST *actionListPtr = \
						DATAPTR_GET( envelopeInfoPtr->actionList );
	CONTENT_LIST *contentListPtr;
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtrDynamic( value, valueLength ) );

	REQUIRES( sanityCheckEnvelope( envelopeInfoPtr ) );
	REQUIRES( envInfo == CRYPT_ENVINFO_PASSWORD );
	REQUIRES( valueLength > 0 && valueLength < MAX_ATTRIBUTE_SIZE );

	/* Since we can add one of a multitude of necessary information types, 
	   we need to check to make sure that what we're adding is appropriate.  
	   We do this by trying to match what's being added to the first 
	   information object of the correct type */
	status = matchInfoObject( &contentListPtr, envelopeInfoPtr, envInfo );
	if( cryptStatusError( status ) )
		{
		retExtArg( status,
				   ( status, ENVELOPE_ERRINFO,
					 "Added item doesn't match any envelope information "
					 "object" ) );
		}
	ENSURES( sanityCheckContentList( contentListPtr ) );

	/* If we've been given a password and we need private key information, 
	   it's the password required to decrypt the key so we treat this 
	   specially.  This action recursively calls addDeenvelopeInfo() with 
	   the processed private key so we don't have to fall through to the 
	   session-key processing code below like the other key-handling 
	   actions */
	if( contentListPtr->envInfo == CRYPT_ENVINFO_PRIVATEKEY )
		{
		return( addPrivkeyPasswordInfo( envelopeInfoPtr, contentListPtr,
										value, valueLength ) );
		}

	/* Make sure that we can still add another action */
	if( envelopeInfoPtr->usage != ACTION_MAC && \
		!moreActionsPossible( actionListPtr ) )
		return( CRYPT_ERROR_OVERFLOW );

	/* We've been given a standard decryption password, create a decryption 
	   context for it, derive the key from the password, and use it to 
	   import the session/MAC key */
	if( envelopeInfoPtr->usage == ACTION_MAC )
		{
		if( actionListPtr == NULL )
			return( CRYPT_ERROR_NOTINITED );
		status = addPasswordInfo( contentListPtr, value, valueLength, 
								  actionListPtr->iCryptHandle, NULL, 
								  envelopeInfoPtr->type, ENVELOPE_ERRINFO );
		}
	else
		{
		status = addPasswordInfo( contentListPtr, value, valueLength, 
								  CRYPT_UNUSED, &iNewContext,
								  envelopeInfoPtr->type, ENVELOPE_ERRINFO );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* We've recovered the session key, try and set up the decryption */
	if( envelopeInfoPtr->usage != ACTION_MAC )
		{
		status = initSessionKeyDecryption( envelopeInfoPtr, iNewContext, 
										   TRUE );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iNewContext, IMESSAGE_DECREFCOUNT );
			if( status == CRYPT_ERROR_INITED )
				{
				/* If the attribute that we added to recover the session key 
				   is already present, provide extended error information */
				setErrorInfo( envelopeInfoPtr, envInfo, 
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				}
			return( status );
			}
		}

	/* Complete the envelope information update */
	return( completeEnvelopeInfoUpdate( envelopeInfoPtr ) );
	}

/****************************************************************************
*																			*
*							Envelope Access Routines						*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initDenvResourceHandling( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	
	REQUIRES_V( TEST_FLAG( envelopeInfoPtr->flags, 
						   ENVELOPE_FLAG_ISDEENVELOPE ) );

	/* Set the access method pointers */
	FNPTR_SET( envelopeInfoPtr->addInfoFunction, addDeenvelopeInfo );
	FNPTR_SET( envelopeInfoPtr->addInfoStringFunction, addDeenvelopeInfoString );
	}
#endif /* USE_ENVELOPES */
