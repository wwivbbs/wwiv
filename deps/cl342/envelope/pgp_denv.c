/****************************************************************************
*																			*
*					 cryptlib PGP De-enveloping Routines					*
*					 Copyright Peter Gutmann 1996-2011						*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "pgp_rw.h"
  #include "envelope.h"
#else
  #include "enc_dec/pgp_rw.h"
  #include "envelope/envelope.h"
#endif /* Compiler-specific includes */

#ifdef USE_PGP

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Sanity-check the envelope state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheck( const ENVELOPE_INFO *envelopeInfoPtr )
	{
	/* Make sure that general envelope state is in order */
	if( !( envelopeInfoPtr->flags & ENVELOPE_ISDEENVELOPE ) )
		return( FALSE );
	if( envelopeInfoPtr->pgpDeenvState < PGP_DEENVSTATE_NONE || \
		envelopeInfoPtr->pgpDeenvState >= PGP_DEENVSTATE_LAST )
		return( FALSE );

	/* Make sure that the buffer position is within bounds */
	if( envelopeInfoPtr->buffer == NULL || \
		envelopeInfoPtr->bufPos < 0 || \
		envelopeInfoPtr->bufPos > envelopeInfoPtr->bufSize || \
		envelopeInfoPtr->bufSize < MIN_BUFFER_SIZE || \
		envelopeInfoPtr->bufSize >= MAX_INTLENGTH )
		return( FALSE );

	/* Make sure that the payload size is within bounds */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED && \
		( envelopeInfoPtr->payloadSize < 0 || \
		  envelopeInfoPtr->payloadSize >= MAX_INTLENGTH ) )
		return( FALSE );

	/* Make sure that the out-of-band buffer state is OK.  The oobDataLeft 
	   value is the general size of a data packet header plus the maximum 
	   possible length for the variable-length filename portion */
	if( envelopeInfoPtr->oobEventCount < 0 || \
		envelopeInfoPtr->oobEventCount > 10 || \
		envelopeInfoPtr->oobDataLeft < 0 || \
		envelopeInfoPtr->oobDataLeft >= 32 + 256 )
		return( FALSE );

	/* Make sure that the packet data information is within bounds */
	if( envelopeInfoPtr->segmentSize < 0 || \
		envelopeInfoPtr->segmentSize >= MAX_INTLENGTH || \
		envelopeInfoPtr->dataLeft < 0 || \
		envelopeInfoPtr->dataLeft >= MAX_INTLENGTH )
		return( FALSE );

	return( TRUE );
	}

/* Get information on a PGP data packet.  If the isIndefinite value is 
   present then an indefinite length (i.e. partial packet lengths) is 
   permitted, otherwise it isn't.  If the allowDummyPackets flag is set then
   we allow shorter-than-normal dummy packets (PGP_PACKET_MARKER), otherwise
   we enforce a sensible minimum packet size */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int getPacketInfo( INOUT STREAM *stream, 
						  INOUT ENVELOPE_INFO *envelopeInfoPtr,
						  OUT_ENUM_OPT( PGP_PACKET ) PGP_PACKET_TYPE *packetType, 
						  OUT_LENGTH_Z long *length, 
						  OUT_OPT_BOOL BOOLEAN *isIndefinite,
						  IN_LENGTH_SHORT int minPacketSize )
	{
	int ctb, version, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( packetType, sizeof( PGP_PACKET_TYPE ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );
	assert( isIndefinite == NULL || \
			isWritePtr( isIndefinite, sizeof( BOOLEAN ) ) );

	ENSURES( minPacketSize > 0 && minPacketSize < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*packetType = PGP_PACKET_NONE;
	*length = 0;
	if( isIndefinite != NULL )
		*isIndefinite = FALSE;

	/* Read the packet header and extract information from the CTB.  The 
	   assignment of version numbers is a bit complicated since it's 
	   possible to use PGP 2.x packet headers to wrap up OpenPGP packets, 
	   and in fact a number of apps mix version numbers.  We treat the 
	   version to report as the highest one that we find */
	if( isIndefinite != NULL )
		status = pgpReadPacketHeaderI( stream, &ctb, length, minPacketSize );
	else
		status = pgpReadPacketHeader( stream, &ctb, length, minPacketSize );
	if( cryptStatusError( status ) )
		{
		if( status != OK_SPECIAL )
			return( status );
		ENSURES( isIndefinite != NULL );

		/* Remember that the packet uses an indefinite-length encoding */
		envelopeInfoPtr->dataFlags &= ~ENVDATA_NOSEGMENT;
		*isIndefinite = TRUE;
		}
	version = pgpGetPacketVersion( ctb );
	if( version > envelopeInfoPtr->version )
		envelopeInfoPtr->version = version;
	*packetType = pgpGetPacketType( ctb );

	/* Extract and return the packet type */
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Read Key Exchange/Signature Packets					*
*																			*
****************************************************************************/

/* Add information about an object to an envelope's content information list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int addContentListItem( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							   INOUT_OPT STREAM *stream,
							   const BOOLEAN isContinuedSignature )
	{
	QUERY_INFO queryInfo;
	CONTENT_LIST *contentListItem;
	void *object = NULL;
	int objectSize = 0, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( ( stream == NULL && \
			  envelopeInfoPtr->actionList == NULL && \
			  envelopeInfoPtr->contentList == NULL ) || \
			isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( ( stream == NULL && \
				envelopeInfoPtr->actionList == NULL && \
				envelopeInfoPtr->contentList == NULL ) || \
			  ( stream != NULL ) );

	/* Make sure that there's room to add another list item */
	if( !moreContentItemsPossible( envelopeInfoPtr->contentList ) )
		return( CRYPT_ERROR_OVERFLOW );

	/* PGP 2.x password-encrypted data is detected by the absence of any
	   other keying object rather than by finding a concrete object type so
	   if we're passed a null stream we add a password pseudo-object */
	if( stream == NULL )
		{
		CONTENT_ENCR_INFO *encrInfo;

		status = createContentListItem( &contentListItem, 
										envelopeInfoPtr->memPoolState,
										CONTENT_CRYPT, CRYPT_FORMAT_PGP, 
										NULL, 0 );
		if( cryptStatusError( status ) )
			return( status );
		encrInfo = &contentListItem->clEncrInfo;
		contentListItem->envInfo = CRYPT_ENVINFO_PASSWORD;
		encrInfo->cryptAlgo = CRYPT_ALGO_IDEA;
		encrInfo->cryptMode = CRYPT_MODE_CFB;
		encrInfo->keySetupAlgo = CRYPT_ALGO_MD5;
		status = appendContentListItem( envelopeInfoPtr, contentListItem );
		if( cryptStatusError( status ) )
			{
			clFree( "addContentListItem", contentListItem );
			return( status );
			}
		return( CRYPT_OK );
		}

	/* Find the size of the object, allocate a buffer for it if necessary,
	   and copy it across.  This call verifies that all of the object data 
	   is present in the stream so in theory we don't have to check the 
	   following reads, but we check them anyway just to be sure */
	status = queryPgpObject( stream, &queryInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( queryInfo.type == CRYPT_OBJECT_SIGNATURE && \
		queryInfo.dataStart <= 0 )
		{
		ENSURES( !isContinuedSignature );

		/* It's a one-pass signature packet, the signature information 
		   follows in another packet that will be added later */
		status = sSkip( stream, ( int ) queryInfo.size );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		objectSize = ( int ) queryInfo.size;
		if( ( object = clAlloc( "addContentListItem", \
								objectSize ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		status = sread( stream, object, objectSize );
		if( cryptStatusError( status ) )
			{
			clFree( "addContentListItem", object );
			return( status );
			}
		}

	/* If it's the rest of the signature data from a one-pass signature,
	   locate the first half of the signature info and complete the
	   information.  In theory this could get ugly because there could be
	   multiple one-pass signature packets present but PGP handles multiple 
	   signatures by nesting them so this isn't a problem */
	if( isContinuedSignature )
		{
		int iterationCount;
		
		for( contentListItem = envelopeInfoPtr->contentList, \
				iterationCount = 0;
			 contentListItem != NULL && \
				contentListItem->envInfo != CRYPT_ENVINFO_SIGNATURE && \
				iterationCount < FAILSAFE_ITERATIONS_MED;
			 contentListItem = contentListItem->next, iterationCount++ );
		ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
		ENSURES( contentListItem != NULL && \
				 contentListItem->object == NULL && \
				 contentListItem->objectSize == 0 );

		/* Consistency check, make sure that the hash algorithm and key ID 
		   that we've been working with match what's in the signature */
		if( contentListItem->clSigInfo.hashAlgo != queryInfo.hashAlgo || \
			contentListItem->keyIDsize != queryInfo.keyIDlength || \
			memcmp( contentListItem->keyID, queryInfo.keyID, 
					queryInfo.keyIDlength ) )
			{
			clFree( "addContentListItem", object );
			return( CRYPT_ERROR_BADDATA );
			}

		/* We've got the right content list entry, point it to the newly-
		   acquired signature data */
		contentListItem->object = object;
		contentListItem->objectSize = objectSize;
		}
	else
		{
		/* Allocate memory for the new content list item and copy information
		   on the item across */
		status = createContentListItem( &contentListItem, 
							envelopeInfoPtr->memPoolState, 
							( queryInfo.type == CRYPT_OBJECT_SIGNATURE ) ? \
								CONTENT_SIGNATURE : CONTENT_CRYPT, 
							CRYPT_FORMAT_PGP, object, objectSize );
		if( cryptStatusError( status ) )
			{
			if( object != NULL )
				clFree( "addContentListItem", object );
			return( status );
			}
		}
	if( queryInfo.type == CRYPT_OBJECT_PKCENCRYPTED_KEY || \
		queryInfo.type == CRYPT_OBJECT_SIGNATURE )
		{
		/* Remember details of the enveloping info that we require to 
		   continue */
		if( queryInfo.type == CRYPT_OBJECT_PKCENCRYPTED_KEY )
			{
			CONTENT_ENCR_INFO *encrInfo = &contentListItem->clEncrInfo;

			contentListItem->envInfo = CRYPT_ENVINFO_PRIVATEKEY;
			encrInfo->cryptAlgo = queryInfo.cryptAlgo;
			}
		else
			{
			CONTENT_SIG_INFO *sigInfo = &contentListItem->clSigInfo;

			contentListItem->envInfo = CRYPT_ENVINFO_SIGNATURE;
			sigInfo->hashAlgo = queryInfo.hashAlgo;
			if( queryInfo.attributeStart > 0 )
				{
				REQUIRES( rangeCheck( queryInfo.attributeStart, 
									  queryInfo.attributeLength, 
									  objectSize ) );
				sigInfo->extraData = \
								( BYTE * ) contentListItem->object + \
										   queryInfo.attributeStart;
				sigInfo->extraDataLength = queryInfo.attributeLength;
				}
			if( queryInfo.unauthAttributeStart > 0 )
				{
				REQUIRES( rangeCheck( queryInfo.unauthAttributeStart, 
									  queryInfo.unauthAttributeLength, 
									  objectSize ) );
				sigInfo->extraData2 = \
								( BYTE * ) contentListItem->object + \
										   queryInfo.unauthAttributeStart;
				sigInfo->extraData2Length = queryInfo.unauthAttributeLength;
				}
			}
		REQUIRES( queryInfo.keyIDlength > 0 && \
				  queryInfo.keyIDlength <= CRYPT_MAX_HASHSIZE );
		memcpy( contentListItem->keyID, queryInfo.keyID, 
				queryInfo.keyIDlength );
		contentListItem->keyIDsize = queryInfo.keyIDlength;
		if( queryInfo.iAndSStart > 0 )
			{
			REQUIRES( rangeCheck( queryInfo.iAndSStart, 
								  queryInfo.iAndSLength, objectSize ) );
			contentListItem->issuerAndSerialNumber = \
				( BYTE * ) contentListItem->object + queryInfo.iAndSStart;
			contentListItem->issuerAndSerialNumberSize = queryInfo.iAndSLength;
			}
		}
	if( queryInfo.type == CRYPT_OBJECT_ENCRYPTED_KEY )
		{
		CONTENT_ENCR_INFO *encrInfo = &contentListItem->clEncrInfo;

		/* Remember details of the enveloping info that we require to 
		   continue */
		if( queryInfo.keySetupAlgo != CRYPT_ALGO_NONE )
			{
			contentListItem->envInfo = CRYPT_ENVINFO_PASSWORD;
			encrInfo->keySetupAlgo = queryInfo.keySetupAlgo;
			encrInfo->keySetupIterations = queryInfo.keySetupIterations;
			REQUIRES( queryInfo.saltLength > 0 && \
					  queryInfo.saltLength <= CRYPT_MAX_IVSIZE );
			memcpy( encrInfo->saltOrIV, queryInfo.salt, 
					queryInfo.saltLength );
			encrInfo->saltOrIVsize = queryInfo.saltLength;
			}
		else
			contentListItem->envInfo = CRYPT_ENVINFO_KEY;
		encrInfo->cryptAlgo = queryInfo.cryptAlgo;
		encrInfo->cryptMode = CRYPT_MODE_CFB;
		}
	if( queryInfo.dataStart > 0 )
		{
		REQUIRES( rangeCheck( queryInfo.dataStart, queryInfo.dataLength, 
							  objectSize ) );
		contentListItem->payload = \
			( BYTE * ) contentListItem->object + queryInfo.dataStart;
		contentListItem->payloadSize = queryInfo.dataLength;
		}
	if( queryInfo.version > envelopeInfoPtr->version )
		envelopeInfoPtr->version = queryInfo.version;

	/* If we're completing the read of the data in a one-pass signature
	   packet, we're done */
	if( isContinuedSignature )
		return( CRYPT_OK );

	/* If it's signed data, create a hash action to process it.  Because PGP 
	   only applies one level of signing per packet nesting level we don't 
	   have to worry that this will add redundant hash actions as there'll 
	   only ever be one */
	if( queryInfo.type == CRYPT_OBJECT_SIGNATURE )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		REQUIRES( moreActionsPossible( envelopeInfoPtr->actionList ) );

		/* Append a new hash action to the action list */
		setMessageCreateObjectInfo( &createInfo,
									contentListItem->clSigInfo.hashAlgo );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			{
			deleteContentList( envelopeInfoPtr->memPoolState, 
							   &contentListItem );
			return( status );
			}
		status = addAction( &envelopeInfoPtr->actionList, 
							envelopeInfoPtr->memPoolState, ACTION_HASH,
							createInfo.cryptHandle );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, 
							  IMESSAGE_DECREFCOUNT );
			deleteContentList( envelopeInfoPtr->memPoolState, 
							   &contentListItem );
			return( status );
			}
		}
	status = appendContentListItem( envelopeInfoPtr, contentListItem );
	if( cryptStatusError( status ) )
		{
		deleteContentList( envelopeInfoPtr->memPoolState, 
						   &contentListItem );
		return( status );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Header Processing Routines						*
*																			*
****************************************************************************/

/* Process the header of a packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processPacketHeader( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
								INOUT STREAM *stream, 
								INOUT_ENUM( PGP_DEENVSTATE ) \
									PGP_DEENV_STATE *state )
	{
	const int streamPos = stell( stream );
	PGP_PACKET_TYPE packetType;
	BOOLEAN isIndefinite;
	long packetLength;
	int value, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( state, sizeof( PGP_DEENV_STATE ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( streamPos >= 0 && streamPos < MAX_INTLENGTH );

	/* Read the PGP packet type and figure out what we've got.  If we're at
	   the start of the data we allow noise packets like PGP_PACKET_MARKER
	   (with a length of 3), otherwise we only allow standard packets */
	status = getPacketInfo( stream, envelopeInfoPtr, &packetType, 
							&packetLength, &isIndefinite,
							( *state == PGP_DEENVSTATE_NONE ) ? 3 : 8 );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, ENVELOPE_ERRINFO,
				  "Invalid PGP packet header" ) );
		}

	/* Process as much of the header as we can and move on to the next state.  
	   Since PGP uses sequential discrete packets, if we encounter any of 
	   the non-payload packet types we stay in the initial "none" state 
	   because we don't know what's next */
	switch( packetType )
		{
		case PGP_PACKET_DATA:
			{
			long payloadSize;
			int length;

			/* Skip the content-type, filename, and date */
			sSkip( stream, 1 );
			status = length = sgetc( stream );
			if( !cryptStatusError( status ) )
				status = sSkip( stream, length + 4 );
			if( cryptStatusError( status ) )
				{
				retExt( status,
						( status, ENVELOPE_ERRINFO,
						  "Invalid PGP data packet start" ) );
				}

			/* Remember that this is a pure data packet, record the content 
			   length, and move on to the payload */
			envelopeInfoPtr->contentType = CRYPT_CONTENT_DATA;
			payloadSize = packetLength - ( 1 + 1 + length + 4 );
			if( payloadSize < 1 || payloadSize > MAX_INTLENGTH )
				return( CRYPT_ERROR_BADDATA );
			envelopeInfoPtr->payloadSize = payloadSize;
			if( isIndefinite )
				{
				/* See comment for PGP_PACKET_ENCR */
				}
			*state = PGP_DEENVSTATE_DATA;
			break;
			}

		case PGP_PACKET_COPR:
			if( envelopeInfoPtr->usage != ACTION_NONE )
				return( CRYPT_ERROR_BADDATA );
			envelopeInfoPtr->usage = ACTION_COMPRESS;
#ifdef USE_COMPRESSION
			value = sgetc( stream );
			if( cryptStatusError( value ) )
				return( value );
			switch( value )
				{
				case PGP_ALGO_ZIP:
					/* PGP 2.x has a funny compression level based on DOS 
					   memory limits (13-bit windows) and no zlib header 
					   (because it uses very old InfoZIP code).  Setting the 
					   windowSize to a negative value has the undocumented 
					   effect of not reading zlib headers */
					if( inflateInit2( &envelopeInfoPtr->zStream, -13 ) != Z_OK )
						return( CRYPT_ERROR_MEMORY );
					break;

				case PGP_ALGO_ZLIB:
					/* Standard zlib compression */
					if( inflateInit( &envelopeInfoPtr->zStream ) != Z_OK )
						return( CRYPT_ERROR_MEMORY );
					break;

				default:
					return( CRYPT_ERROR_NOTAVAIL );
				}
			envelopeInfoPtr->flags |= ENVELOPE_ZSTREAMINITED;
			if( packetLength != CRYPT_UNUSED )
				{
				const long payloadSize = packetLength - 1;

				/* All known implementations use the PGP 2.x "keep going 
				   until you run out of data" non-length encoding that's 
				   neither a definite- nor an indefinite length, but it's 
				   possible that something somewhere will use a proper 
				   definite length so we accomodate this here */
				if( payloadSize < 1 || payloadSize > MAX_INTLENGTH )
					return( CRYPT_ERROR_BADDATA );
				envelopeInfoPtr->payloadSize = payloadSize;
				}
			else
				{
				/* Remember that we have no length information available for 
				   the payload */
				envelopeInfoPtr->dataFlags |= ENVDATA_NOLENGTHINFO;
				}
			*state = PGP_DEENVSTATE_DATA;
			break;
#else
			return( CRYPT_ERROR_NOTAVAIL );
#endif /* USE_COMPRESSION */

		case PGP_PACKET_SKE:
		case PGP_PACKET_PKE:
			/* Read the SKE/PKE packet */
			if( envelopeInfoPtr->usage != ACTION_NONE && \
				envelopeInfoPtr->usage != ACTION_CRYPT )
				return( CRYPT_ERROR_BADDATA );
			envelopeInfoPtr->usage = ACTION_CRYPT;
			sseek( stream, streamPos );	/* Reset to start of packet */
			status = addContentListItem( envelopeInfoPtr, stream, FALSE );
			if( cryptStatusError( status ) )
				{
				retExt( status,
						( status, ENVELOPE_ERRINFO,
						  "Invalid PGP %s packet", 
						  ( packetType == PGP_PACKET_SKE ) ? "SKE" : "PKE" ) );
				}
			break;

		case PGP_PACKET_SIGNATURE:
		case PGP_PACKET_SIGNATURE_ONEPASS:
			/* Try and guess whether this is a standalone signature.  This 
			   is rather difficult since unlike S/MIME there's no way to 
			   tell whether a PGP signature packet is part of other data or 
			   a standalone item.  The best that we can do is assume that if 
			   the caller added a hash action and we find a signature then 
			   it's a detached signature.  Unfortunately there's no way to 
			   tell whether a signature packet with no user-supplied hash is 
			   a standalone signature or the start of further signed data so 
			   we can't handle detached signatures where the user doesn't 
			   supply the hash */
			if( envelopeInfoPtr->usage == ACTION_SIGN && \
				envelopeInfoPtr->actionList != NULL && \
				envelopeInfoPtr->actionList->action == ACTION_HASH )
				{
				/* We can't have a detached signature packet as a one-pass 
				   signature */
				if( packetType == PGP_PACKET_SIGNATURE_ONEPASS )
					{
					retExt( CRYPT_ERROR_BADDATA,
							( CRYPT_ERROR_BADDATA, ENVELOPE_ERRINFO,
							  "PGP detached signature can't be a one-pass "
							  "signature packet" ) );
					}
				envelopeInfoPtr->flags |= ENVELOPE_DETACHED_SIG;
				}

			/* Read the signature/signature information packet.  We allow 
			   the usage to be set already if we find a signature packet 
			   since it could have been preceded by a one-pass signature 
			   packet or be a detached signature */
			if( envelopeInfoPtr->usage != ACTION_NONE && \
				!( packetType == PGP_PACKET_SIGNATURE && \
				   envelopeInfoPtr->usage == ACTION_SIGN ) )
				return( CRYPT_ERROR_BADDATA );
			envelopeInfoPtr->usage = ACTION_SIGN;
			sseek( stream, streamPos );	/* Reset to start of packet */
			status = addContentListItem( envelopeInfoPtr, stream, FALSE );
			if( cryptStatusError( status ) )
				{
				retExt( status,
						( status, ENVELOPE_ERRINFO,
						  "Invalid PGP %s signature packet",
						  ( packetType == PGP_PACKET_SIGNATURE_ONEPASS ) ? \
							"one-pass" : "" ) );
				}
			if( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG )
				{
				/* If it's a one-pass signature there's no payload present 
				   so we can go straight to the postdata state */
				envelopeInfoPtr->dataFlags |= ENVDATA_HASHACTIONSACTIVE;
				envelopeInfoPtr->payloadSize = 0;
				*state = PGP_DEENVSTATE_DONE;
				}
			else
				*state = PGP_DEENVSTATE_DATA;
			break;

		case PGP_PACKET_ENCR_MDC:
			/* The encrypted-data-with-MDC packet is preceded by a version 
			   number */
			status = value = sgetc( stream );
			if( !cryptStatusError( status ) && value != 1 )
				status = CRYPT_ERROR_BADDATA;
			if( !cryptStatusError( status ) )
				{
				/* Adjust the length for the version number and make sure 
				   that what's left is valid.  In theory this check isn't
				   necessary because getPacketInfo() has enforced a minimum
				   length, but we do it anyway just to be sure */
				packetLength--;
				if( packetLength <= 0 || packetLength > MAX_INTLENGTH )
					status = CRYPT_ERROR_BADDATA;
				}
			if( cryptStatusError( status ) )
				{
				retExt( status,
						( status, ENVELOPE_ERRINFO,
						  "Invalid MDC packet header" ) );
				}
			envelopeInfoPtr->flags |= ENVELOPE_AUTHENC;
			/* Fall through */

		case PGP_PACKET_ENCR:
			if( envelopeInfoPtr->usage != ACTION_NONE && \
				envelopeInfoPtr->usage != ACTION_CRYPT )
				return( CRYPT_ERROR_BADDATA );
			envelopeInfoPtr->payloadSize = packetLength;
			if( isIndefinite )
				{
				/* This is only a partial length, need to mark it as such */
				/* NB: getPacketInfo() has already cleared the
					   envelopeInfoPtr->dataFlags & ENVDATA_NOSEGMENT setting */
				}
			envelopeInfoPtr->usage = ACTION_CRYPT;
			*state = ( packetType == PGP_PACKET_ENCR_MDC ) ? \
					 PGP_DEENVSTATE_ENCR_MDC : PGP_DEENVSTATE_ENCR;
			break;

		case PGP_PACKET_MARKER:
			/* Obsolete marker packet, skip it */
			return( sSkip( stream, packetLength ) );
		
		default:
			/* Unrecognised/invalid packet type */
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, ENVELOPE_ERRINFO,
					  "Unrecognised PGP packet type %d", packetType ) );
		}

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/* Adjust the envelope data size information based on what we've found in 
   any nested packets that we've dug down to through 
   processPacketDataHeader() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int adjustDataInfo( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						   const STREAM *stream,
						   IN_LENGTH_OPT const long packetLength )
	{
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( stream, sizeof( STREAM ) ) );

	REQUIRES( packetLength == CRYPT_UNUSED || \
			  ( packetLength >= 0 && packetLength < MAX_INTLENGTH ) );
	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* If it's a definite-length packet, use the overall packet size.  This 
	   also skips any MDC packets that may be attached to the end of the 
	   plaintext */
	if( packetLength != CRYPT_UNUSED )
		{
		envelopeInfoPtr->segmentSize = stell( stream ) + packetLength;

		/* If we're using the definite-length encoding (which is the default 
		   for PGP) then the overall payload size is equal to the segment 
		   size */
		if( envelopeInfoPtr->dataFlags & ENVDATA_NOSEGMENT )
			envelopeInfoPtr->payloadSize = envelopeInfoPtr->segmentSize;

		ENSURES( sanityCheck( envelopeInfoPtr ) );

		return( CRYPT_OK );
		}

	/* If it's not a definite-length packet then it can only be compressed 
	   data, for which 'ENSURES( packetType == PGP_PACKET_COPR )' */
	ENSURES( envelopeInfoPtr->payloadSize != CRYPT_UNUSED );

	/* It's an arbitrary-length compressed data packet, use the length that 
	   we got earlier from the outer packet */
	if( envelopeInfoPtr->dataFlags & ENVDATA_ENDOFCONTENTS )
		{
		/* This is a should-never-occur situation, it's unclear what exactly
		   we should be doing at this point */
		DEBUG_DIAG(( "Found EOC for unknown-length compressed data" ));
		assert( DEBUG_WARN );
		
		return( CRYPT_ERROR_BADDATA );
		}
	envelopeInfoPtr->segmentSize = envelopeInfoPtr->payloadSize;

	/* If we've reached the end of the data (i.e. the entire current segment 
	   is contained within the data present in the buffer), remember that 
	   what's left still needs to be processed (e.g. hashed in the case of 
	   signed data) on the way out */
	if( envelopeInfoPtr->segmentSize <= envelopeInfoPtr->bufPos )
		{
		/* If the outer packet has an MDC at the end then we need to adjust 
		   the data size to skip the MDC data */
		if( envelopeInfoPtr->dataFlags & ENVDATA_HASATTACHEDOOB ) 
			{
			if( envelopeInfoPtr->segmentSize < PGP_MDC_PACKET_SIZE )
				{
				retExt( CRYPT_ERROR_SIGNATURE,
						( CRYPT_ERROR_SIGNATURE, ENVELOPE_ERRINFO,
						  "MDC packet is missing or incomplete, expected %d bytes "
						  "but got %ld", PGP_MDC_PACKET_SIZE, 
						  envelopeInfoPtr->segmentSize ) );
				}
			envelopeInfoPtr->segmentSize -= PGP_MDC_PACKET_SIZE;
			}
		envelopeInfoPtr->dataLeft = envelopeInfoPtr->segmentSize;
		envelopeInfoPtr->segmentSize = 0;
		}

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/* PGP doesn't provide any indication of what the content of the packet's 
   encrypted payload is so we have to burrow down into the encrypted data to 
   see whether the payload needs any further processing.  To do this we look 
   ahead into the data to see whether we need to strip the header (for a 
   plain data packet) or inform the user that there's a nested content type.  
   This process is complicated by the fact that there are various ways of 
   representing the length information for both outer and inner packets and 
   the fact that the payload can consist of more than one packet, but we're 
   really only interested in the first one in most cases.  The calculation 
   of the encapsulated payload length is as follows:

	+---+---+---+........................................
	|len|hdr| IV|										: Encrypted data
	+---+---+---+........................................
				:										:
				+---+---+---------------------------+---+
				|len|hdr|		  Payload			| ? | Inner content
				+---+---+---------------------------+---+

   Definite payload length:
		Payload = (inner) length - (inner) hdr.

   Unknown length (only allowed for compressed data): 
		Payload = (leave as is since by definition the compressed data 
				   extends to EOF).

   Indefinite payload length: This gets complicated because when this occurs 
   it's always accompanied by indefinite-length inner content as well, and 
   because of PGP's bizarre fixed-point encoding that only allows power-of-
   two lengths the positions never synchronise:

	+---+---+-------+---+-----------+---+-----------+
	|len| IV|		|len|			|len|			| Encrypted data
	+---+---+-------+---+-----------+---+-----------+
			:
			+---+-----------+---+-----------+---+----
			|len|			|len|			|len|	  Inner content
			+---+-----------+---+-----------+---+----

   Since there's no way to process both the outer and inner indefinite
   lengths in a single pass, we leave the inner content unprocessed.
   This leads to a problem because indicating an inner content type
   of "Data" implies that the caller is getting back raw payload data
   and not PGP-encapsulated data, so we cheat slightly and report it as
   compressed data.  This means that the caller will feed it back to us
   to strip the nested encapsulation, with the "decompression" being
   a straight copy from input to output */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processPacketDataHeader( INOUT ENVELOPE_INFO *envelopeInfoPtr,
									OUT_ENUM_OPT( PGP_DEENVSTATE ) \
										PGP_DEENV_STATE *state )
	{
	static const MAP_TABLE typeMapTbl[] = {
		{ PGP_PACKET_COPR, CRYPT_CONTENT_COMPRESSEDDATA },
		{ PGP_PACKET_ENCR, CRYPT_CONTENT_ENCRYPTEDDATA },
		{ PGP_PACKET_ENCR_MDC, CRYPT_CONTENT_ENCRYPTEDDATA },
		{ PGP_PACKET_SKE, CRYPT_CONTENT_ENCRYPTEDDATA },
		{ PGP_PACKET_PKE, CRYPT_CONTENT_ENVELOPEDDATA },
		{ PGP_PACKET_SIGNATURE, CRYPT_CONTENT_SIGNEDDATA },
		{ PGP_PACKET_SIGNATURE_ONEPASS, CRYPT_CONTENT_SIGNEDDATA },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	STREAM headerStream;
	BYTE buffer[ 32 + 256 + 8 ];	/* Max.data packet header size */
	PGP_PACKET_TYPE packetType;
	long packetLength;
	int value, length, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( state, sizeof( PGP_DEENV_STATE ) ) );
	
	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( envelopeInfoPtr->oobDataLeft < 32 + 256 );

	/* If this is an indefinite-length payload then we pretend that it's
	   compressed data in order to have the caller hand it back to us for
	   processing of the inner indefinite-length content, see the comment
	   at the start of this function for details */
	if( !( envelopeInfoPtr->dataFlags & ENVDATA_NOSEGMENT ) )
		{
		envelopeInfoPtr->contentType = CRYPT_CONTENT_COMPRESSEDDATA;

		/* Don't try and process the content any further */
		envelopeInfoPtr->oobEventCount = envelopeInfoPtr->oobDataLeft = 0;
		*state = PGP_DEENVSTATE_DONE;

		ENSURES( sanityCheck( envelopeInfoPtr ) );

		return( CRYPT_OK );
		}

	/* If we're down to stripping raw header data, remove it from the buffer
	   and exit */
	if( envelopeInfoPtr->oobEventCount <= 0 )
		{
		status = envelopeInfoPtr->copyFromEnvelopeFunction( envelopeInfoPtr,
										buffer, envelopeInfoPtr->oobDataLeft, 
										&length, ENVCOPY_FLAG_NONE );
		if( cryptStatusError( status ) )
			return( status );
		if( length < envelopeInfoPtr->oobDataLeft )
			return( CRYPT_ERROR_UNDERFLOW );

		/* We've successfully stripped all of the out-of-band data, clear the
		   data counter.  If it's compressed data (which doesn't have a 1:1 
		   correspondence between input and output and which has an unknown-
		   length encoding so there's no length information to adjust), 
		   exit */
		envelopeInfoPtr->oobDataLeft = 0;
		if( envelopeInfoPtr->usage == ACTION_COMPRESS )
			{
			*state = PGP_DEENVSTATE_DONE;

			ENSURES( sanityCheck( envelopeInfoPtr ) );

			return( CRYPT_OK );
			}

		/* Adjust the current data count by what we've removed.  The reason 
		   we have to do this is because segmentSize records the amount of
		   data copied in (rather than out, as we've done here) but since it 
		   was copied directly into the envelope buffer as part of the 
		   header-processing rather than via copyToDeenvelope() (which is
		   what usually adjusts segmentSize for us) we have to manually 
		   adjust the value here */
		if( envelopeInfoPtr->segmentSize > 0 )
			{
			envelopeInfoPtr->segmentSize -= length;
			ENSURES( envelopeInfoPtr->segmentSize >= 0 && \
					 envelopeInfoPtr->segmentSize < MAX_INTLENGTH );

			/* If we've reached the end of the data (i.e. the entire current 
			   segment is contained within the data present in the buffer) 
			   remember that what's left still needs to be processed (e.g. 
			   hashed in the case of signed data) on the way out */
			if( envelopeInfoPtr->segmentSize <= envelopeInfoPtr->bufPos )
				{
				envelopeInfoPtr->dataLeft = envelopeInfoPtr->segmentSize;
				envelopeInfoPtr->segmentSize = 0;
				}
			}

		/* We've processed the header, if this is signed data we start 
		   hashing from this point (the PGP RFCs are wrong in this regard, 
		   only the payload is hashed and not the entire packet) */
		if( envelopeInfoPtr->usage == ACTION_SIGN )
			envelopeInfoPtr->dataFlags |= ENVDATA_HASHACTIONSACTIVE;

		/* We're done */
		*state = PGP_DEENVSTATE_DONE;

		ENSURES( sanityCheck( envelopeInfoPtr ) );
		return( CRYPT_OK );
		}

	/* We have to perform all sorts of special-case processing to handle the 
	   out-of-band packet header at the start of the payload.  Initially, we 
	   need to find out how much header data is actually present.  The header 
	   for a plain data packet consists of:

		byte	ctb
		byte[]	length
		byte	type = 'b' | 't'
		byte	filename length
		byte[]	filename
		byte[4]	timestamp

	   The smallest size for this header (1-byte length, no filename) is 
	   1 + 1 + 1 + 1 + 4 = 8 bytes.  This is also just enough to get us to 
	   the filename length for a maximum-size header, which is 1 + 5 + 1 + 1 
	   bytes up to the filename length and covers the type + length range 
	   of every other packet type, which can be from 1 to 1 + 5 bytes.  Thus 
	   we read 8 bytes, setting the OOB data flag to indicate that this is a 
	   read-ahead read that doesn't remove data from the buffer */
	status = envelopeInfoPtr->copyFromEnvelopeFunction( envelopeInfoPtr,
														buffer, 8, &length,
														ENVCOPY_FLAG_OOBDATA );
	if( cryptStatusError( status ) )
		return( status );
	if( length < 8 )
		return( CRYPT_ERROR_UNDERFLOW );

	/* Read the header information and see what we've got */
	sMemConnect( &headerStream, buffer, length );
	status = getPacketInfo( &headerStream, envelopeInfoPtr, &packetType,
							&packetLength, NULL, 8 );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &headerStream );
		return( status );
		}

	/* Remember the total data packet size unless it's compressed data, 
	   which doesn't have a 1:1 correspondence between input and output */
	if( envelopeInfoPtr->usage != ACTION_COMPRESS )
		{
		status = adjustDataInfo( envelopeInfoPtr, &headerStream,
								 packetLength );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If it's a literal data packet, parse it so that we can strip it from 
	   the data that we return to the caller.  We know that the reads can't
	   fail because the readahead read has confirmed that there are at least
	   8 bytes available, but we check anyway just to be sure */
	if( packetType == PGP_PACKET_DATA )
		{
		int extraLen;

		( void ) sgetc( &headerStream );	/* Skip content type */
		status = extraLen = sgetc( &headerStream );
		if( !cryptStatusError( status ) )
			{
			envelopeInfoPtr->oobDataLeft = stell( &headerStream ) + \
										   extraLen + 4;
			}
		sMemDisconnect( &headerStream );
		if( cryptStatusError( status ) )
			return( status );

		/* Remember that this is a pure data packet */
		envelopeInfoPtr->contentType = CRYPT_CONTENT_DATA;

		/* We've processed enough of the header to know what to do next, 
		   move on to the next sub-state where we just consume all of the 
		   input.  This has to be done as a sub-state within the 
		   PGP_DEENVSTATE_DATA_HEADER state since we can encounter a
		   (recoverable) error between reading the out-of-band data header
		   and reading the out-of-band data itself */
		envelopeInfoPtr->oobEventCount--;

		ENSURES( sanityCheck( envelopeInfoPtr ) );

		return( CRYPT_OK );
		}

	sMemDisconnect( &headerStream );

	/* If it's a known packet type, indicate it as the nested content type */
	status = mapValue( packetType, &value, typeMapTbl, 
					   FAILSAFE_ARRAYSIZE( typeMapTbl, MAP_TABLE ) );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_BADDATA );
	envelopeInfoPtr->contentType = value;

	/* Don't try and process the content any further */
	envelopeInfoPtr->oobEventCount = envelopeInfoPtr->oobDataLeft = 0;
	*state = PGP_DEENVSTATE_DONE;

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/* Process the start of an encrypted data packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processEncryptedPacket( INOUT ENVELOPE_INFO *envelopeInfoPtr, 
								   INOUT STREAM *stream, 
								   IN_ENUM( PGP_DEENVSTATE ) \
									const PGP_DEENV_STATE state )
	{
	CRYPT_CONTEXT iMdcContext = CRYPT_UNUSED;
	BYTE ivInfoBuffer[ CRYPT_MAX_IVSIZE + 2 + 8 ];
	int ivSize, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( sanityCheck( envelopeInfoPtr ) );
	REQUIRES( state > PGP_DEENVSTATE_NONE && state < PGP_DEENVSTATE_LAST );

	/* If there aren't any non-session-key keying resource objects present 
	   then we can't go any further until we get a session key */
	if( envelopeInfoPtr->actionList == NULL )
		{
		/* There's no session key object present, add a pseudo-object that 
		   takes the place of the (password-derived) session key object in 
		   the content list.  This can only occur for PGP 2.x conventionally-
		   encrypted data, which didn't encode any algorithm information 
		   with the data, so if we get to this point we know that we've hit 
		   data encrypted with the default IDEA/CFB encryption algorithm 
		   derived from a user password using the default MD5 hash 
		   algorithm */
		if( envelopeInfoPtr->contentList == NULL )
			{
			status = addContentListItem( envelopeInfoPtr, NULL, FALSE );
			if( cryptStatusError( status ) )
				return( status );
			}

		/* We can't continue until we're given some sort of keying resource */
		return( CRYPT_ENVELOPE_RESOURCE );
		}
	ENSURES( envelopeInfoPtr->actionList != NULL && \
			 envelopeInfoPtr->actionList->action == ACTION_CRYPT );

	/* If there's an MDC packet present, prepare a hash action.  We have to 
	   do this before we perform the IV setup because the decrypted form
	   of the IV data is hashed before the payload data is hashed */
	if( state == PGP_DEENVSTATE_ENCR_MDC )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		REQUIRES( moreActionsPossible( envelopeInfoPtr->actionList ) );

		/* Append a hash action to the action list */
		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA1 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT,
								  &createInfo, OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( status );
		iMdcContext = createInfo.cryptHandle;
		status = addAction( &envelopeInfoPtr->actionList, 
							envelopeInfoPtr->memPoolState, ACTION_HASH,
							iMdcContext );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iMdcContext, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		envelopeInfoPtr->dataFlags |= ENVDATA_HASHACTIONSACTIVE;

		/* Remember that the end of the payload data is actually an MDC 
		   packet that's been tacked onto the payload */
		envelopeInfoPtr->dataFlags |= ENVDATA_HASATTACHEDOOB;
		}

	/* Read and process PGP's peculiar two-stage IV */
	status = krnlSendMessage( envelopeInfoPtr->actionList->iCryptHandle,
							  IMESSAGE_GETATTRIBUTE, &ivSize, 
							  CRYPT_CTXINFO_IVSIZE );
	if( cryptStatusOK( status ) )
		status = sread( stream, ivInfoBuffer, ivSize + 2 );
	if( !cryptStatusError( status ) )
		{
		status = pgpProcessIV( envelopeInfoPtr->actionList->iCryptHandle,
							   ivInfoBuffer, ivSize + 2, ivSize, 
							   iMdcContext, FALSE );
		}
	if( cryptStatusError( status ) )
		return( status );
	envelopeInfoPtr->iCryptContext = \
							envelopeInfoPtr->actionList->iCryptHandle;

	/* If we're keeping track of the outer packet size in case there's no 
	   inner size info present, adjust it by the data that we've just 
	   processed */
	if( envelopeInfoPtr->payloadSize != CRYPT_UNUSED )
		envelopeInfoPtr->payloadSize -= stell( stream );

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Trailer Processing Routines						*
*																			*
****************************************************************************/

/* Process an MDC packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processMDC( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	ACTION_LIST *actionListPtr;
	MESSAGE_DATA msgData;
	BYTE *bufPtr = envelopeInfoPtr->buffer + envelopeInfoPtr->dataLeft;
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* Make sure that there's an MDC packet present */
	if( envelopeInfoPtr->bufPos - \
			envelopeInfoPtr->dataLeft < PGP_MDC_PACKET_SIZE )
		{
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, ENVELOPE_ERRINFO,
				  "MDC packet is missing or incomplete, expected %d bytes "
				  "but got %d", PGP_MDC_PACKET_SIZE, 
				  envelopeInfoPtr->bufPos - envelopeInfoPtr->dataLeft ) );
		}

	/* Since this is out-of-band data that follows the payload, we can't 
	   use copyFromDeenvelope() to retrieve it but have to pull it directly 
	   from the envelope buffer */
	if( bufPtr[ 0 ] != 0xD3 || bufPtr[ 1 ] != 0x14 )
		return( CRYPT_ERROR_BADDATA );

	/* Hash the trailer bytes (the start of the MDC packet) and wrap up the 
	   hashing */
	status = envelopeInfoPtr->processExtraData( envelopeInfoPtr, bufPtr, 2 );
	if( cryptStatusOK( status ) )
		status = envelopeInfoPtr->processExtraData( envelopeInfoPtr, "", 0 );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the MDC value matches our calculated hash value */
	setMessageData( &msgData, bufPtr + 2, PGP_MDC_PACKET_SIZE - 2 );
	actionListPtr = findAction( envelopeInfoPtr->actionList, ACTION_HASH );
	ENSURES( actionListPtr != NULL );
	status = krnlSendMessage( actionListPtr->iCryptHandle, IMESSAGE_COMPARE, 
							  &msgData, MESSAGE_COMPARE_HASH );
	if( cryptStatusError( status ) )
		{
		setErrorString( ENVELOPE_ERRINFO, 
						"MDC data doesn't match calculated MDC", 37 );
		status = CRYPT_ERROR_SIGNATURE;
		}

	/* Record the MDC data as having been consumed */
	envelopeInfoPtr->bufPos = envelopeInfoPtr->dataLeft;

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( status );
	}

/****************************************************************************
*																			*
*						Process Envelope Preamble/Postamble					*
*																			*
****************************************************************************/

/* Process the non-data portions of a PGP message.  This is a complex event-
   driven state machine, but instead of reading along a (hypothetical
   Turing-machine) tape someone has taken the tape and cut it into bits and
   keeps feeding them to us and saying "See what you can do with this" (and
   occasionally "Where's the bloody spoons?").  Since PGP uses sequential
   discrete packets rather than the nested objects encountered in the ASN.1-
   encoded data format the parsing code is made slightly simpler because
   (for example) the PKC info is just an unconnected sequence of packets
   rather than a SEQUENCE or SET OF as for cryptlib and PKCS #7/CMS.  OTOH 
   since there's no indication of what's next we have to perform a complex
   lookahead to see what actions we have to take once we get to the payload.
   The end result is that teh code is actually vastly more complex than the
   CMS equivalent */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processPreamble( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	PGP_DEENV_STATE state = envelopeInfoPtr->pgpDeenvState;
	STREAM stream;
	int remainder, streamPos = 0, iterationCount, status = CRYPT_OK;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* If we've finished processing the start of the message, header, don't
	   do anything */
	if( state == PGP_DEENVSTATE_DONE )
		return( CRYPT_OK );

	sMemConnect( &stream, envelopeInfoPtr->buffer, envelopeInfoPtr->bufPos );

	/* Keep consuming information until we run out of input or reach the
	   plaintext data packet */
	for( iterationCount = 0;
		 state != PGP_DEENVSTATE_DONE && iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		/* Read the PGP packet type and figure out what we've got */
		if( state == PGP_DEENVSTATE_NONE )
			{
			status = processPacketHeader( envelopeInfoPtr, &stream, &state );
			if( cryptStatusError( status ) )
				break;

			/* Remember how far we got */
			streamPos = stell( &stream );
			}

		/* Process the start of an encrypted data packet */
		if( state == PGP_DEENVSTATE_ENCR || \
			state == PGP_DEENVSTATE_ENCR_MDC )
			{
			status = processEncryptedPacket( envelopeInfoPtr, &stream, state );
			if( cryptStatusError( status ) )
				{
				/* If it's a resource-needed status then it's not an 
				   error */
				if( status == CRYPT_ENVELOPE_RESOURCE )
					break;

				/* We may get non-data-related errors like 
				   CRYPT_ERROR_WRONGKEY so we only set extended error 
				   information if it's a data-related error */
				if( isDataError( status ) )
					{
					setErrorString( ENVELOPE_ERRINFO, 
									"Invalid PGP encrypted data packet header", 
									40 );
					}
				break;
				}

			/* Remember where we are and move on to the next state */
			streamPos = stell( &stream );
			state = PGP_DEENVSTATE_DATA;
			}

		/* Process the start of a data packet */
		if( state == PGP_DEENVSTATE_DATA )
			{
			const int originalDataFlags = envelopeInfoPtr->dataFlags;

			/* Synchronise the data stream processing to the start of the
			   encapsulated data.  This is made somewhat complex by PGP's
			   awkward packet format (see the comment for 
			   processPacketDataHeader()) which, unlike CMS:

				[ Hdr [ Encaps [ Octet String ] ] ]

			   has:
						   [ Hdr | Octet String ]
				  [ Keyex ][ Hdr | Octet String ]
				[ Onepass ][ Hdr | Octet String ][ Signature ]
				   [ Copr ][ Hdr | Octet String ]

			   This means that if we're not processing a data packet then 
			   the content isn't the payload but a futher set of discrete 
			   packets that we don't want to touch.  To work around this we 
			   temporarily set the ENVDATA_NOLENGTHINFO flag to indicate 
			   that it's a blob to be processed as an opaque unit, at the 
			   same time temporarily clearing any other flags that might 
			   mess up the opaque-blob handling.

			   In addition to this, if we're using the indefinite-length
			   encoding then the initial segment's length has already been 
			   read when the packet header was read.  This is because PGP's
			   wierd indefinite-length encoding works as follows:

				[ Type | Length | Continuation flag | Data ]
				[		 Length | Continuation flag | Data ]
				[		 Length | Continuation flag | Data ]
				...
				[		 Length						| Data ]

			   so we can't simply undo the read of the start of the first 
			   packet and treat it as a standard segement because the 
			   encoding for the first segment and the remaining segments is 
			   different, so an attempt to treat them identically will lead 
			   to a decoding error.  Instead we set the 
			   ENVDATA_NOFIRSTSEGMENT flag to indicate that the first 
			   length-read should be skipped.
			   
			   Finally, when we reset the flags we have to preserve the 
			   ENVDATA_ENDOFCONTENTS flag, since we may have already 
			   encountered the last segment during the sync operation */
			if( envelopeInfoPtr->dataFlags & ENVDATA_NOSEGMENT )
				{
				if( envelopeInfoPtr->usage != ACTION_NONE )
					envelopeInfoPtr->dataFlags |= ENVDATA_NOLENGTHINFO;
				}
			else
				envelopeInfoPtr->dataFlags |= ENVDATA_NOFIRSTSEGMENT;
			status = envelopeInfoPtr->syncDeenvelopeData( envelopeInfoPtr,
														  &stream );
			envelopeInfoPtr->dataFlags = originalDataFlags | \
				( envelopeInfoPtr->dataFlags & ENVDATA_ENDOFCONTENTS );
			if( cryptStatusError( status ) )
				{
				setErrorString( ENVELOPE_ERRINFO, 
								"Couldn't synchronise envelope state prior "
								"to data payload processing", 68 );
				break;
				}
			streamPos = 0;

			/* Move on to the next state.  For plain data we're done,
			   however for other content types we have to either process or
			   strip out the junk that PGP puts at the start of the 
			   content */
			if( envelopeInfoPtr->usage != ACTION_NONE )
				{
				envelopeInfoPtr->oobEventCount = 1;
				state = PGP_DEENVSTATE_DATA_HEADER;
				}
			else
				state = PGP_DEENVSTATE_DONE;
			
			ENSURES( checkActions( envelopeInfoPtr ) );
			}

		/* Burrow down into the encapsulated data to see what's next */
		if( state == PGP_DEENVSTATE_DATA_HEADER )
			{
			/* If there's no out-of-band data left to remove at the start of
			   the payload, we're done.  This out-of-band data handling 
			   sometimes requires two passes, the first time through 
			   oobEventCount is nonzero because it's been set in the 
			   preceding PGP_DEENVSTATE_DATA state and we fall through to 
			   processPacketDataHeader() which decrements the oobEventCount
			   to zero.  However processPacketDataHeader() may need to read 
			   out-of-band data in which case on the second time around 
			   oobDataLeft will be nonzero, resulting in a second call to
			   processPacketDataHeader() to clear the remaining out-of-band
			   data */
			if( envelopeInfoPtr->oobEventCount <= 0 && \
				envelopeInfoPtr->oobDataLeft <= 0 )
				{
				state = PGP_DEENVSTATE_DONE;
				break;
				}

			/* Process the encapsulated data header */
			status = processPacketDataHeader( envelopeInfoPtr, &state );
			if( cryptStatusError( status ) )
				{
				setErrorString( ENVELOPE_ERRINFO, 
								"Invalid PGP encapsulated content header", 
								39 );
				break;
				}
			}
		}
	sMemDisconnect( &stream );
	if( iterationCount >= FAILSAFE_ITERATIONS_MED )
		{
		/* Technically this would be an overflow but that's a recoverable
		   error so we make it a BADDATA, which is really what it is */
		return( CRYPT_ERROR_BADDATA );
		}
	envelopeInfoPtr->pgpDeenvState = state;

	ENSURES( streamPos >= 0 && streamPos < MAX_INTLENGTH && \
			 envelopeInfoPtr->bufPos - streamPos >= 0 );

	/* Consume the input that we've processed so far by moving everything 
	   past the current position down to the start of the envelope buffer */
	remainder = envelopeInfoPtr->bufPos - streamPos;
	REQUIRES( remainder >= 0 && remainder < MAX_INTLENGTH && \
			  streamPos + remainder <= envelopeInfoPtr->bufSize );
	if( remainder > 0 && streamPos > 0 )
		{
		REQUIRES( rangeCheck( streamPos, remainder, 
							  envelopeInfoPtr->bufSize ) );
		memmove( envelopeInfoPtr->buffer, envelopeInfoPtr->buffer + streamPos,
				 remainder );
		}
	envelopeInfoPtr->bufPos = remainder;
	ENSURES( sanityCheck( envelopeInfoPtr ) );
	if( cryptStatusError( status ) )
		return( status );

	/* If all went OK but we're still not out of the header information,
	   return an underflow error */
	return( ( state != PGP_DEENVSTATE_DONE ) ? \
			CRYPT_ERROR_UNDERFLOW : CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processPostamble( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							 STDC_UNUSED const BOOLEAN dummy )
	{
	CONTENT_LIST *contentListPtr;
	int iterationCount, status = CRYPT_OK;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* If that's all there is, return */
	if( envelopeInfoPtr->usage != ACTION_SIGN && \
		!( envelopeInfoPtr->dataFlags & ENVDATA_HASATTACHEDOOB ) )
		return( CRYPT_OK );

	/* If there's an MDC packet present, make sure that the integrity check 
	   matches */
	if( envelopeInfoPtr->dataFlags & ENVDATA_HASATTACHEDOOB )
		return( processMDC( envelopeInfoPtr ) );

	/* Find the signature information in the content list.  In theory this
	   could get ugly because there could be multiple one-pass signature
	   packets present but PGP handles multiple signatures by nesting them 
	   so this isn't a problem */
	for( contentListPtr = envelopeInfoPtr->contentList, iterationCount = 0;
		 contentListPtr != NULL && \
			contentListPtr->envInfo != CRYPT_ENVINFO_SIGNATURE && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 contentListPtr = contentListPtr->next, iterationCount++ );
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	ENSURES( contentListPtr != NULL );

	/* PGP 2.x prepended (!!) signatures to the signed data, OpenPGP fixed 
	   this by splitting the signature into a header with signature info and 
	   a trailer with the actual signature.  If we're processing a PGP 2.x
	   signature we'll already have the signature data present so we only 
	   check for signature data if it's not already available */
	if( contentListPtr->object == NULL )
		{
		STREAM stream;
		long packetLength;
		PGP_PACKET_TYPE packetType;

		/* Make sure that there's enough data left in the stream to do 
		   something with.  We require a minimum of 44 bytes, the size
		   of the DSA signature payload, in the stream */
		if( envelopeInfoPtr->bufPos - \
				envelopeInfoPtr->dataLeft < PGP_MAX_HEADER_SIZE + 44 )
			return( CRYPT_ERROR_UNDERFLOW );

		REQUIRES( moreContentItemsPossible( envelopeInfoPtr->contentList ) );

		/* Read the signature packet at the end of the payload */
		sMemConnect( &stream, envelopeInfoPtr->buffer + envelopeInfoPtr->dataLeft,
					 envelopeInfoPtr->bufPos - envelopeInfoPtr->dataLeft );
		status = getPacketInfo( &stream, envelopeInfoPtr, &packetType, 
								&packetLength, NULL, 8 );
		if( cryptStatusOK( status ) && packetType != PGP_PACKET_SIGNATURE )
			status = CRYPT_ERROR_BADDATA;
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Invalid PGP signature packet header" ) );
			}
		sseek( &stream, 0 );
		status = addContentListItem( envelopeInfoPtr, &stream, TRUE );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Invalid PGP signature packet" ) );
			}
		}

	/* When we reach this point there may still be unhashed data left in the 
	   buffer (it won't have been hashed yet because the hashing is performed 
	   when the data is copied out, after unwrapping and whatnot) so we hash 
	   it before we exit.  Since we don't wrap up the hashing as we do with
	   any other format (PGP hashes in all sorts of odds and ends after 
	   hashing the message body) we have to manually turn off hashing here */
	if( envelopeInfoPtr->dataLeft > 0 )
		{
		status = envelopeInfoPtr->processExtraData( envelopeInfoPtr,
						envelopeInfoPtr->buffer, envelopeInfoPtr->dataLeft );
		}
	envelopeInfoPtr->dataFlags &= ~ENVDATA_HASHACTIONSACTIVE;
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Envelope Access Routines						*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initPGPDeenveloping( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	
	REQUIRES_V( envelopeInfoPtr->flags & ENVELOPE_ISDEENVELOPE );

	/* Set the access method pointers */
	envelopeInfoPtr->processPreambleFunction = processPreamble;
	envelopeInfoPtr->processPostambleFunction = processPostamble;
	envelopeInfoPtr->checkAlgo = pgpCheckAlgo;

	/* Set up the processing state information */
	envelopeInfoPtr->pgpDeenvState = PGP_DEENVSTATE_NONE;

	/* Turn off segmentation of the envelope payload.  PGP has a single 
	   length at the start of the data and doesn't segment the payload */
	envelopeInfoPtr->dataFlags |= ENVDATA_NOSEGMENT;
	}
#endif /* USE_PGP */
