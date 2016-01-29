/****************************************************************************
*																			*
*					 cryptlib PGP Enveloping Routines						*
*					 Copyright Peter Gutmann 1996-2008						*
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
	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Check the general envelope state */
	if( !envelopeSanityCheck( envelopeInfoPtr ) )
		return( FALSE );

	/* Make sure that general envelope state is in order */
	if( envelopeInfoPtr->type != CRYPT_FORMAT_PGP || \
		( envelopeInfoPtr->flags & ENVELOPE_ISDEENVELOPE ) )
		return( FALSE );

	/* Make sure that the buffer position is within bounds */
	if( envelopeInfoPtr->buffer == NULL || \
		envelopeInfoPtr->bufSize < MIN_BUFFER_SIZE || \
		envelopeInfoPtr->bufSize >= MAX_BUFFER_SIZE )
		return( FALSE );

	/* If the auxBuffer isn't being used, make sure that all values related 
	   to it are clear */
	if( envelopeInfoPtr->auxBuffer != NULL || \
		envelopeInfoPtr->auxBufPos != 0 || envelopeInfoPtr->auxBufSize != 0 )
		return( FALSE );

	return( TRUE );
	}

/* Check that a requested algorithm type is valid with PGP data */

CHECK_RETVAL_BOOL \
BOOLEAN pgpCheckAlgo( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo, 
					  IN_MODE_OPT const CRYPT_MODE_TYPE cryptMode )
	{
	int dummy;

	REQUIRES_B( cryptAlgo > CRYPT_ALGO_NONE && \
				cryptAlgo < CRYPT_ALGO_LAST_EXTERNAL );
	REQUIRES_B( ( cryptMode == CRYPT_MODE_NONE ) || \
				( cryptMode > CRYPT_MODE_NONE && \
				  cryptMode < CRYPT_MODE_LAST ) );

	if( cryptStatusError( cryptlibToPgpAlgo( cryptAlgo, &dummy ) ) )
		return( FALSE );
	if( isConvAlgo( cryptAlgo ) )
		{
		if( cryptMode != CRYPT_MODE_CFB )
			return( FALSE );
		}
	else
		{
		if( cryptMode != CRYPT_MODE_NONE )
			return( FALSE );
		}

	return( TRUE );
	}

/* Unlike PKCS #7/CMS/SMIME, PGP doesn't contain truly nested messages (with 
   the outer wrapper identifying what's in the inner content) but just puts 
   one lot of data inside the other.  This means that when the caller 
   specifies an inner content type we then have to burrow into the data that 
   they push in order to determine that it actually matches what they've 
   specified as the content type.
   
   The following alternative to copyToEnvelope() is enabled when the inner 
   content type isn't plain data.  It checks the data being pushed (as far 
   as it's possible to do so) to ensure that it matches the declared content 
   type */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int copyToEnvelopeAlt( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							  IN_BUFFER_OPT( length ) const BYTE *buffer, 
							  IN_LENGTH_Z const int length )
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
	STREAM stream;
	long contentLength;
	int ctb DUMMY_INIT, version, packetType, value, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( length == 0 || isReadPtr( buffer, length ) );

	REQUIRES( ( buffer == NULL && length == 0 ) || \
			  ( buffer != NULL && length >= 0 && length < MAX_BUFFER_SIZE ) );

	/* If it's a flush then it's always an error, since there must be nested 
	   content */
	if( length <= 0 )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, ENVELOPE_ERRINFO,
				  "Envelope marked as having nested content type %d can't "
				  "contain no content", envelopeInfoPtr->contentType ) );
		}

	/* Examine the start of the data to try and make sure that it matches 
	   the content-type that's been set by the user */
	if( length > 1 )
		{
		sMemConnect( &stream, buffer, length );
		status = pgpReadPacketHeaderI( &stream, &ctb, &contentLength, 1 );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			{
			/* If we encountered an error (other than running out of input)
			   then what's being pushed isn't PGP nested content */
			if( status != CRYPT_ERROR_UNDERFLOW )
				{
				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, ENVELOPE_ERRINFO,
						  "Data for envelope marked as having content type "
						  "%d doesn't appear to be PGP content", 
						  envelopeInfoPtr->contentType ) );
				}
			
			/* We ran out of data to read, only look at the CTB */
			ctb = byteToInt( *buffer );
			}
		}
	else
		{
		REQUIRES( length == 1 );

		/* There's too little data to read a full PGP header, just get the 
		   CTB */
		ctb = byteToInt( *buffer );
		}
	version = pgpGetPacketVersion( ctb );
	packetType = pgpGetPacketType( ctb );
	if( ( version != PGP_VERSION_2 && version != PGP_VERSION_OPENPGP ) || \
		( packetType <= PGP_PACKET_NONE || packetType >= PGP_PACKET_LAST ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, ENVELOPE_ERRINFO,
				  "Data for envelope marked as having content type %d "
				  "doesn't appear to be PGP content", 
				  envelopeInfoPtr->contentType ) );
		}

	/* Make sure that what we've got matches the declared content-type */
	status = mapValue( packetType, &value, typeMapTbl, 
					   FAILSAFE_ARRAYSIZE( typeMapTbl, MAP_TABLE ) );
	if( cryptStatusError( status ) )
		{
		/* Disallowed content type, set a dummy value to fail the following
		   test while still allowing for error reporting */
		value = 0;
		}
	if( value != envelopeInfoPtr->contentType || \
		version < PGP_VERSION_2 || version > PGP_VERSION_OPENPGP )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, ENVELOPE_ERRINFO,
				  "Data for envelope marked as having content type "
				  "%d appears to actually be of content type %d, "
				  "version %d", envelopeInfoPtr->contentType, 
				  value, version ) );
		}

	/* Reset the envelope data processing to the standard mechanism and pass 
	   the data on to the standard function */
	initEnvelopeStreaming( envelopeInfoPtr );
	return( envelopeInfoPtr->copyToEnvelopeFunction( envelopeInfoPtr,
										buffer, length ) );
	}

/****************************************************************************
*																			*
*						Write Key Exchange/Signature Packets				*
*																			*
****************************************************************************/

/* One-pass signature info:

	byte	version = 3
	byte	sigType
	byte	hashAlgo
	byte	sigAlgo
	byte[8]	keyID
	byte	1 

   This is additional header data written at the start of a block of signed
   data rather than a standard PGP packet so we can't write it using the 
   normal PGP packet read/write routines */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeSignatureInfoPacket( INOUT STREAM *stream, 
									 IN_HANDLE const CRYPT_CONTEXT iSignContext,
									 IN_HANDLE const CRYPT_CONTEXT iHashContext )
	{
	BYTE keyID[ PGP_KEYID_SIZE + 8 ];
	int hashAlgo, signAlgo DUMMY_INIT;	/* int vs.enum */
	int pgpHashAlgo, pgpCryptAlgo DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );

	/* Get the signature information */
	status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE, 
							  &hashAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iSignContext, IMESSAGE_GETATTRIBUTE, 
								  &signAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, keyID, PGP_KEYID_SIZE );
		status = krnlSendMessage( iSignContext, IMESSAGE_GETATTRIBUTE_S, 
								  &msgData, CRYPT_IATTRIBUTE_KEYID_OPENPGP );
		}
	if( cryptStatusError( status ) )
		return( status );
	status = cryptlibToPgpAlgo( hashAlgo, &pgpHashAlgo );
	if( cryptStatusOK( status ) )
		status = cryptlibToPgpAlgo( signAlgo, &pgpCryptAlgo );
	ENSURES( cryptStatusOK( status ) );

	/* Write the signature info packet.  Note that the version 3 value is 
	   normally used to identify a legal-kludged PGP 2.0 but in this case it 
	   denotes OpenPGP, which usually has the version 4 value rather than 3 */
	pgpWritePacketHeader( stream, PGP_PACKET_SIGNATURE_ONEPASS, \
						  PGP_VERSION_SIZE + 1 + PGP_ALGOID_SIZE + \
							PGP_ALGOID_SIZE + PGP_KEYID_SIZE + 1 );
	sputc( stream, 3 );		/* Version = 3 (OpenPGP) */
	sputc( stream, 0 );		/* Binary document signature */
	sputc( stream, pgpHashAlgo );
	sputc( stream, pgpCryptAlgo );
	swrite( stream, keyID, PGP_KEYID_SIZE );
	return( sputc( stream, 1 ) );
	}

/****************************************************************************
*																			*
*								Write Header Packets						*
*																			*
****************************************************************************/

/* Write the data header packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeHeaderPacket( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	STREAM stream;
	int status = CRYPT_OK;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( envelopeInfoPtr->envState == ENVSTATE_HEADER );

	/* If we're encrypting, set up the encryption-related information.  
	   Since PGP doesn't perform a key exchange of a session key when 
	   conventionally-encrypting data the encryption information could be 
	   coming from either an encryption action (derived from a password) or 
	   a conventional key exchange action that results in the direct 
	   creation of a session encryption key */
	if( envelopeInfoPtr->usage == ACTION_CRYPT )
		{
		status = initEnvelopeEncryption( envelopeInfoPtr,
								envelopeInfoPtr->actionList->iCryptHandle, 
								CRYPT_ALGO_NONE, CRYPT_MODE_NONE, NULL, 0, 
								FALSE );
		if( cryptStatusError( status ) )
			return( status );

		/* Prepare to start emitting the key exchange (PKC-encrypted) or 
		   session key (conventionally encrypted) actions */
		envelopeInfoPtr->lastAction = \
							findAction( envelopeInfoPtr->preActionList,
										ACTION_KEYEXCHANGE_PKC );
		if( envelopeInfoPtr->lastAction == NULL )
			{
			/* There's no key exchange action, we're using a raw session key 
			   derived from a password */
			envelopeInfoPtr->lastAction = envelopeInfoPtr->actionList;
			}
		envelopeInfoPtr->envState = ENVSTATE_KEYINFO;

		ENSURES( envelopeInfoPtr->lastAction != NULL );

		return( CRYPT_OK );
		}

	/* If we're not encrypting data (i.e. there's only a single packet 
	   present rather than a packet preceded by a pile of key exchange 
	   actions) we write the appropriate PGP header based on the envelope 
	   usage */
	sMemOpen( &stream, envelopeInfoPtr->buffer, envelopeInfoPtr->bufSize );
	switch( envelopeInfoPtr->usage )
		{
		case ACTION_SIGN:
			if( !( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG ) )
				{
				status = writeSignatureInfoPacket( &stream, 
								envelopeInfoPtr->postActionList->iCryptHandle,
								envelopeInfoPtr->actionList->iCryptHandle );
				if( cryptStatusError( status ) )
					break;
				}

			/* Since we can only sign literal data we need to explicitly 
			   write an inner data header */
			REQUIRES( envelopeInfoPtr->contentType == CRYPT_CONTENT_DATA );
			envelopeInfoPtr->envState = ENVSTATE_DATA;
			break;

		case ACTION_NONE:
			/* Write the header followed by an indicator that we're using 
			   opaque content, a zero-length filename, and no date */
			pgpWritePacketHeader( &stream, PGP_PACKET_DATA, 
								  envelopeInfoPtr->payloadSize + \
									PGP_DATA_HEADER_SIZE );
			status = swrite( &stream, PGP_DATA_HEADER, PGP_DATA_HEADER_SIZE );

			/* The header state remains at ENVSTATE_HEADER, which means that
			   it'll be finalised to ENVSTATE_DONE at the end of this 
			   function as no further processing is necessary */
			break;

		case ACTION_COMPRESS:
			/* Compressed data packets use a special unkown-length encoding 
			   that doesn't work like any other PGP packet type so we can't 
			   use pgpWritePacketHeader() for this packet type but have to 
			   hand-assemble the header ourselves */
			sputc( &stream, PGP_CTB_COMPRESSED );
			status = sputc( &stream, PGP_ALGO_ZLIB );
			if( cryptStatusError( status ) )
				break;
			if( envelopeInfoPtr->contentType == CRYPT_CONTENT_DATA )
				{
				/* If there's no inner content type we need to explicitly 
				   write an inner data header */
				envelopeInfoPtr->envState = ENVSTATE_DATA;
				}
			break;
	
		default:
			retIntError();
		}
	if( cryptStatusOK( status ) )
		envelopeInfoPtr->bufPos = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Reset the segmentation state.  Although PGP doesn't segment the 
	   payload we still have to reset the state to synchronise things like 
	   payload hashing and encryption.  We also set the block size mask to 
	   all ones if we're not encrypting since we can begin and end data 
	   segments on arbitrary boundaries */
	envelopeInfoPtr->dataFlags |= ENVDATA_SEGMENTCOMPLETE;
	if( envelopeInfoPtr->usage != ACTION_CRYPT )
		envelopeInfoPtr->blockSizeMask = -1;
	envelopeInfoPtr->lastAction = NULL;

	/* If we're not emitting any inner header, we're done */
	if( envelopeInfoPtr->envState == ENVSTATE_HEADER || \
		( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG ) )
		envelopeInfoPtr->envState = ENVSTATE_DONE;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Header/Trailer Processing Routines					*
*																			*
****************************************************************************/

/* Write key exchange actions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeKeyex( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	ACTION_LIST *lastActionPtr;
	int iterationCount, status = CRYPT_OK;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Export the session key using each of the PKC keys, or write the 
	   derivation information needed to recreate the session key */
	for( lastActionPtr = envelopeInfoPtr->lastAction, iterationCount = 0; 
		 lastActionPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_MED;
		 lastActionPtr = lastActionPtr->next, iterationCount++ )
		{
		void *bufPtr = envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos;
		const int dataLeft = min( envelopeInfoPtr->bufSize - \
									envelopeInfoPtr->bufPos, 
								  MAX_INTLENGTH_SHORT - 1 );
		int keyexSize = 0;

		/* Make sure that there's enough room to emit this key exchange 
		   action */
		if( lastActionPtr->encodedSize + 128 > dataLeft )
			{
			status = CRYPT_ERROR_OVERFLOW;
			break;
			}

		/* Emit the key exchange action.  The "key exchange" for 
		   conventional-encryption actions isn't actually a key exchange 
		   since PGP derives the session key directly from the password, 
		   so all it does is write the key-derivation parameters and
		   exit.

		   If we're encrypting with an MDC there's a third type of action 
		   present, an ACTION_HASH, but since this is unkeyed there's no
		   key exchange action to be taken for it */
		if( lastActionPtr->action == ACTION_KEYEXCHANGE_PKC )
			{
			status = iCryptExportKey( bufPtr, dataLeft, &keyexSize, 
									  CRYPT_FORMAT_PGP, 
									  envelopeInfoPtr->iCryptContext,
									  lastActionPtr->iCryptHandle );
			}
		else
			{
			if( lastActionPtr->action == ACTION_CRYPT )
				{
				status = iCryptExportKey( bufPtr, dataLeft, &keyexSize, 
										  CRYPT_FORMAT_PGP, CRYPT_UNUSED, 
										  envelopeInfoPtr->iCryptContext );
				}
			}
		if( cryptStatusError( status ) )
			break;
		envelopeInfoPtr->bufPos += keyexSize;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED )
	envelopeInfoPtr->lastAction = lastActionPtr;

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeEncryptedContentHeader( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	CRYPT_CONTEXT iMdcContext = CRYPT_UNUSED;
	const BOOLEAN hasMDC = ( envelopeInfoPtr->flags & ENVELOPE_AUTHENC ) ? \
						   TRUE : FALSE;
	STREAM stream;
	BYTE ivInfoBuffer[ ( CRYPT_MAX_IVSIZE + 2 ) + 8 ];
	const int packetType = hasMDC ? PGP_PACKET_ENCR_MDC : PGP_PACKET_ENCR;
	const int payloadDataSize = PGP_DATA_HEADER_SIZE + \
							    envelopeInfoPtr->payloadSize + \
								( hasMDC ? PGP_MDC_PACKET_SIZE : 0 );
	const int dataLeft = min( envelopeInfoPtr->bufSize - \
								envelopeInfoPtr->bufPos, 
							  MAX_INTLENGTH_SHORT - 1 );
	int ivSize, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Get the IV size and make sure that there's enough room to emit the 
	   encrypted content header (+8 for slop space) */
	status = krnlSendMessage( envelopeInfoPtr->iCryptContext,
							  IMESSAGE_GETATTRIBUTE, &ivSize, 
							  CRYPT_CTXINFO_IVSIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( dataLeft < PGP_MAX_HEADER_SIZE + ( ivSize + 2 ) + \
									( hasMDC ? 1 : 0 ) + 8 )
		return( CRYPT_ERROR_OVERFLOW );

	/* If we're using an MDC, we need to hash the IV information before it's
	   processed in order to create the sort-of-keyed hash */
	if( hasMDC )
		{
		ACTION_LIST *actionListPtr;

		actionListPtr = findAction( envelopeInfoPtr->actionList, ACTION_HASH );
		ENSURES( actionListPtr != NULL );
		iMdcContext = actionListPtr->iCryptHandle;
		}

	/* Set up the PGP IV information */
	status = pgpProcessIV( envelopeInfoPtr->iCryptContext, 
						   ivInfoBuffer, ivSize + 2, ivSize, 
						   iMdcContext, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the encrypted content header, with the length being the size of 
	   the optional MDC indicator, the inner data CTB and length, and the
	   combined inner data header, payload, and optional MDC */
	sMemOpen( &stream, envelopeInfoPtr->buffer + envelopeInfoPtr->bufPos, 
			  dataLeft );
	pgpWritePacketHeader( &stream, packetType, 
						  ( hasMDC ? 1 : 0 ) + ( ivSize + 2 ) + \
						  1 + pgpSizeofLength( payloadDataSize ) + \
						  payloadDataSize );
	if( hasMDC )
		{
		/* MDC-encrypted data has a version number before the data */
		sputc( &stream, 1 );
		}
	status = swrite( &stream, ivInfoBuffer, ivSize + 2 );
	if( cryptStatusOK( status ) )
		envelopeInfoPtr->bufPos += stell( &stream );
	sMemDisconnect( &stream );

	return( status );
	}

/****************************************************************************
*																			*
*							Header Processing Routines						*
*																			*
****************************************************************************/

/* Create a session key to encrypt the envelope contents */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int createSessionKey( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	CRYPT_CONTEXT iSessionKeyContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	static const int mode = CRYPT_MODE_CFB;	/* int vs.enum */
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Create a default encryption action */
	setMessageCreateObjectInfo( &createInfo, envelopeInfoPtr->defaultAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iSessionKeyContext = createInfo.cryptHandle;
	status = krnlSendMessage( iSessionKeyContext, IMESSAGE_SETATTRIBUTE, 
							  ( MESSAGE_CAST ) &mode, CRYPT_CTXINFO_MODE );
	if( cryptStatusOK( status ) )
		status = krnlSendNotifier( iSessionKeyContext, IMESSAGE_CTX_GENKEY );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iSessionKeyContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Add the session-key action to the action list */
	status = addAction( &envelopeInfoPtr->actionList, 
						envelopeInfoPtr->memPoolState, ACTION_CRYPT, 
						iSessionKeyContext );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iSessionKeyContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	return( CRYPT_OK );
	}

/* Perform any final initialisation actions before starting the enveloping
   process */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int preEnvelopeEncrypt( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	CRYPT_DEVICE iCryptDevice = CRYPT_ERROR;
	ACTION_LIST *actionListPtr;
	int iterationCount, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( envelopeInfoPtr->usage == ACTION_CRYPT );
	REQUIRES( findAction( envelopeInfoPtr->preActionList, \
						  ACTION_KEYEXCHANGE_PKC ) != NULL );

	/* Create the session key if necessary */
	if( envelopeInfoPtr->actionList == NULL )
		{
		status = createSessionKey( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		/* If the session key context is tied to a device, get its handle so 
		   we can check that all key exchange objects are also in the same 
		   device */
		status = krnlSendMessage( envelopeInfoPtr->actionList->iCryptHandle, 
								  MESSAGE_GETDEPENDENT, &iCryptDevice, 
								  OBJECT_TYPE_DEVICE );
		if( cryptStatusError( status ) )
			iCryptDevice = CRYPT_ERROR;
		}

	/* Notify the kernel that the session key context is attached to the 
	   envelope.  This is an internal object used only by the envelope so we
	   tell the kernel not to increment its reference count when it attaches
	   it */
	status = krnlSendMessage( envelopeInfoPtr->objectHandle, 
							  IMESSAGE_SETDEPENDENT, 
							  &envelopeInfoPtr->actionList->iCryptHandle, 
							  SETDEP_OPTION_NOINCREF );
	if( cryptStatusError( status ) )
		return( status );

	/* Now walk down the list of key exchange actions connecting each one to 
	   the session key action. The caller has already guaranteed that there's 
	   at least one PKC keyex action present */
	for( actionListPtr = findAction( envelopeInfoPtr->preActionList,
									 ACTION_KEYEXCHANGE_PKC ), \
			iterationCount = 0;
		 actionListPtr != NULL && \
			actionListPtr->action == ACTION_KEYEXCHANGE_PKC && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 actionListPtr = actionListPtr->next, iterationCount++ )
		{
		/* If the session key context is tied to a device, make sure that 
		   the key exchange object is in the same device */
		if( iCryptDevice != CRYPT_ERROR )
			{
			CRYPT_DEVICE iKeyexDevice;

			status = krnlSendMessage( actionListPtr->iCryptHandle, 
									  MESSAGE_GETDEPENDENT, &iKeyexDevice, 
									  OBJECT_TYPE_DEVICE );
			if( cryptStatusError( status ) || iCryptDevice != iKeyexDevice )
				return( CRYPT_ERROR_INVALID );
			}

		/* Remember that we now have a controlling action and connect the
		   controller to the subject */
		envelopeInfoPtr->actionList->flags &= ~ACTION_NEEDSCONTROLLER;
		actionListPtr->associatedAction = envelopeInfoPtr->actionList;

		/* Evaluate the size of the exported action.  We only get PKC 
		   actions at this point so we don't have to provide any special-
		   case handling for other key exchange types */
		status = iCryptExportKey( NULL, 0, &actionListPtr->encodedSize, 
								CRYPT_FORMAT_PGP, 
								envelopeInfoPtr->actionList->iCryptHandle,
								actionListPtr->iCryptHandle );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int preEnvelopeSign( const ENVELOPE_INFO *envelopeInfoPtr )
	{
	ACTION_LIST *actionListPtr = envelopeInfoPtr->postActionList;
	SIGPARAMS sigParams;

	assert( isReadPtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( envelopeInfoPtr->usage == ACTION_SIGN );

	/* Make sure that there's at least one signing action present */
	if( actionListPtr == NULL )
		return( CRYPT_ERROR_NOTINITED );

	assert( isWritePtr( actionListPtr, sizeof( ACTION_LIST ) ) );
	
	REQUIRES( actionListPtr->associatedAction != NULL );

	/* Evaluate the size of the signature action */
	initSigParamsPGP( &sigParams, PGP_SIG_DATA, NULL, 0 );
	return( iCryptCreateSignature( NULL, 0, &actionListPtr->encodedSize, 
							CRYPT_FORMAT_PGP, actionListPtr->iCryptHandle, 
							actionListPtr->associatedAction->iCryptHandle,
							&sigParams ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int preEnvelopeInit( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	int status = CRYPT_OK;

	/* If there's no nested content type set, default to plain data */
	if( envelopeInfoPtr->contentType == CRYPT_CONTENT_NONE )
		envelopeInfoPtr->contentType = CRYPT_CONTENT_DATA;

	/* Remember the length information for when we copy in data */
	envelopeInfoPtr->segmentSize = envelopeInfoPtr->payloadSize;

	/* Perform any remaining initialisation.  Since PGP derives the session 
	   key directly from the user password we only perform the encryption 
	   initialisation if there are PKC key exchange actions present */
	if( envelopeInfoPtr->usage == ACTION_CRYPT && \
		findAction( envelopeInfoPtr->preActionList,
					ACTION_KEYEXCHANGE_PKC ) != NULL )
		status = preEnvelopeEncrypt( envelopeInfoPtr );
	else
		{
		if( envelopeInfoPtr->usage == ACTION_SIGN )
			status = preEnvelopeSign( envelopeInfoPtr );
		}
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, ENVELOPE_ERRINFO,
				  "Couldn't perform final %s initialisation prior to "
				  "enveloping data", 
				  ( envelopeInfoPtr->usage == ACTION_SIGN ) ? \
					"signing" : "encryption" ) );
		}

	/* If we're performing authenticated encryption, create the hash object 
	   that's used to sort-of-MAC the data */
	if( envelopeInfoPtr->flags & ENVELOPE_AUTHENC )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;
		CRYPT_CONTEXT iHashContext;

		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA1 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( status );
		iHashContext = createInfo.cryptHandle;
		status = addAction( &envelopeInfoPtr->actionList, 
							envelopeInfoPtr->memPoolState, ACTION_HASH, 
							iHashContext );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		envelopeInfoPtr->dataFlags |= ENVDATA_HASHACTIONSACTIVE;

		/* Since the MDC packet is tacked onto the end of the payload, we 
		   need to increase the effect data size by the size of the MDC
		   packet data */
		envelopeInfoPtr->segmentSize += PGP_MDC_PACKET_SIZE;
		}

	/* Delete any orphaned actions such as automatically-added hash actions 
	   that were overridden with user-supplied alternate actions */
	status = deleteUnusedActions( envelopeInfoPtr );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( checkActions( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Trailer Processing Routines						*
*																			*
****************************************************************************/

/* Process an MDC packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int emitMDC( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	CRYPT_CONTEXT iMdcContext;
	ACTION_LIST *actionListPtr;
	MESSAGE_DATA msgData;
	BYTE mdcBuffer[ 2 + CRYPT_MAX_HASHSIZE + 8 ];
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Make sure that there's enough room left to emit the MDC packet */
	if( envelopeInfoPtr->bufSize - \
			envelopeInfoPtr->bufPos < PGP_MDC_PACKET_SIZE )
		return( CRYPT_ERROR_OVERFLOW );

	/* Hash the trailer bytes (the start of the MDC packet, 0xD3 0x14) and 
	   wrap up the hashing */
	actionListPtr = findAction( envelopeInfoPtr->actionList, ACTION_HASH );
	ENSURES( actionListPtr != NULL );
	iMdcContext = actionListPtr->iCryptHandle;
	memcpy( mdcBuffer, "\xD3\x14", 2 );
	status = krnlSendMessage( iMdcContext, IMESSAGE_CTX_HASH, mdcBuffer, 2 );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iMdcContext, IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusError( status ) )
		return( status );
	envelopeInfoPtr->dataFlags &= ~ENVDATA_HASHACTIONSACTIVE;

	/* Append the MDC packet to the payload data */
	setMessageData( &msgData, mdcBuffer + 2, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iMdcContext, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );
	return( envelopeInfoPtr->copyToEnvelopeFunction( envelopeInfoPtr,
										mdcBuffer, PGP_MDC_PACKET_SIZE ) );
	}

/****************************************************************************
*																			*
*						Emit Envelope Preamble/Postamble					*
*																			*
****************************************************************************/

/* Output as much of the preamble as possible into the envelope buffer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int emitPreamble( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	int status = CRYPT_OK;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* If we've finished processing the header information, don't do
	   anything */
	if( envelopeInfoPtr->envState == ENVSTATE_DONE )
		return( CRYPT_OK );

	/* If we haven't started doing anything yet, perform various final
	   initialisations */
	if( envelopeInfoPtr->envState == ENVSTATE_NONE )
		{
		status = preEnvelopeInit( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );

		/* We're ready to go, prepare to emit the outer header */
		envelopeInfoPtr->envState = ENVSTATE_HEADER;
		}

	/* Emit the outer header.  This always follows directly from the final
	   initialisation step but we keep the two logically distinct to 
	   emphasise the fact that the former is merely finalised enveloping 
	   actions without performing any header processing while the latter is 
	   the first stage that actually emits header data */
	if( envelopeInfoPtr->envState == ENVSTATE_HEADER )
		{
		status = writeHeaderPacket( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't create envelope header" ) );
			}
		ENSURES( envelopeInfoPtr->envState != ENVSTATE_HEADER );
		}

	/* Handle key export actions */
	if( envelopeInfoPtr->envState == ENVSTATE_KEYINFO )
		{
		status = writeKeyex( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't emit key exchange actions to envelope "
					  "header" ) );
			}

		/* Move on to the next state */
		envelopeInfoPtr->envState = ENVSTATE_ENCRINFO;
		}

	/* Handle encrypted content information */
	if( envelopeInfoPtr->envState == ENVSTATE_ENCRINFO )
		{
		/* Write the encrypted content header */
		status = writeEncryptedContentHeader( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't emit encrypted content header to envelope "
					  "header" ) );
			}

		/* Make sure that we start a new segment if we try to add any data */
		envelopeInfoPtr->dataFlags |= ENVDATA_SEGMENTCOMPLETE;

		/* If the content type is plain data then we have to push in the 
		   inner data header before we exit */
		if( envelopeInfoPtr->contentType == CRYPT_CONTENT_DATA )
			envelopeInfoPtr->envState = ENVSTATE_DATA;
		else
			{
			/* We've processed the header, if this is signed data then we 
			   start hashing from this point.  The PGP RFCs are wrong in 
			   this regard in that only the payload is hashed and not the 
			   entire packet */
			if( envelopeInfoPtr->usage == ACTION_SIGN )
				envelopeInfoPtr->dataFlags |= ENVDATA_HASHACTIONSACTIVE;

			/* We're finished */
			envelopeInfoPtr->envState = ENVSTATE_DONE;
			}
		}

	/* Handle data payload information */
	if( envelopeInfoPtr->envState == ENVSTATE_DATA )
		{
		STREAM stream;
		BYTE headerBuffer[ 64 + 8 ];

		/* Make sure that there's enough room to emit the data header (+8 
		   for slop space) */
		if( envelopeInfoPtr->bufPos + PGP_MAX_HEADER_SIZE + \
				PGP_DATA_HEADER_SIZE + 8 >= envelopeInfoPtr->bufSize )
			return( CRYPT_ERROR_OVERFLOW );

		/* Write the payload header.  Since this may be encrypted we have to
		   do it indirectly via copyToEnvelope() */
		sMemOpen( &stream, headerBuffer, 64 );
		pgpWritePacketHeader( &stream, PGP_PACKET_DATA, 
						PGP_DATA_HEADER_SIZE + envelopeInfoPtr->payloadSize );
		status = swrite( &stream, PGP_DATA_HEADER, PGP_DATA_HEADER_SIZE );
		if( cryptStatusOK( status ) )
			{
			/* Adjust the running total count by the size of the additional 
			   header that's been prepended and copy the header to the
			   envelope */
			envelopeInfoPtr->segmentSize += stell( &stream );
			status = envelopeInfoPtr->copyToEnvelopeFunction( envelopeInfoPtr,
											headerBuffer, stell( &stream ) );
			}
		sMemClose( &stream );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't emit data header into envelope header" ) );
			}

		/* We've processed the header, if this is signed data then we start 
		   hashing from this point.  The PGP RFCs are wrong in this regard 
		   in that only the payload is hashed and not the entire packet */
		if( envelopeInfoPtr->usage == ACTION_SIGN )
			envelopeInfoPtr->dataFlags |= ENVDATA_HASHACTIONSACTIVE;

		/* We're finished */
		envelopeInfoPtr->envState = ENVSTATE_DONE;
		}

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	/* If we're processing a nested content-type that isn't plain data, 
	   temporarily enable an alternate processing function that deals with 
	   PGP's way of handling this */
	if( envelopeInfoPtr->contentType != CRYPT_CONTENT_DATA )
		envelopeInfoPtr->copyToEnvelopeFunction = copyToEnvelopeAlt;

	return( CRYPT_OK );
	}

/* Output as much of the postamble as possible into the envelope buffer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int emitPostamble( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						  STDC_UNUSED const BOOLEAN dummy )
	{
	SIGPARAMS sigParams;
	int sigBufSize, sigSize, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( sanityCheck( envelopeInfoPtr ) );

	/* Before we can emit the trailer we need to flush any remaining data
	   from internal buffers */
	if( envelopeInfoPtr->envState == ENVSTATE_NONE )
		{
		/* If we're using MDC encryption, append the MDC packet to the 
		   payload data */
		if( envelopeInfoPtr->flags & ENVELOPE_AUTHENC )
			{
			status = emitMDC( envelopeInfoPtr );
			if( cryptStatusError( status ) )
				return( status );
			}

		/* Flush the data through */
		status = envelopeInfoPtr->copyToEnvelopeFunction( envelopeInfoPtr, 
														  NULL, 0 );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, ENVELOPE_ERRINFO,
					  "Couldn't flush remaining data into envelope "
					  "buffer" ) );
			}
		envelopeInfoPtr->envState = ENVSTATE_FLUSHED;
		}

	/* The only PGP packet that has a trailer is signed data using the new
	   (post-2.x) one-pass signature packet, if we're not signing data we can
	   exit now */
	if( envelopeInfoPtr->usage != ACTION_SIGN )
		{
		/* We're done */
		envelopeInfoPtr->envState = ENVSTATE_DONE;

		ENSURES( sanityCheck( envelopeInfoPtr ) );

		return( CRYPT_OK );
		}

	/* Check whether there's enough room left in the buffer to emit the 
	   signature directly into it.  Since signatures are fairly small (a few 
	   hundred bytes) we always require enough room in the buffer and don't 
	   bother with any overflow handling via the auxBuffer */
	sigBufSize = min( envelopeInfoPtr->bufSize - envelopeInfoPtr->bufPos, 
					  MAX_INTLENGTH_SHORT - 1 );
	if( envelopeInfoPtr->postActionList->encodedSize + 64 > sigBufSize )
		return( CRYPT_ERROR_OVERFLOW );

	/* Sign the data */
	initSigParamsPGP( &sigParams, PGP_SIG_DATA, NULL, 0 );
	status = iCryptCreateSignature( envelopeInfoPtr->buffer + \
					envelopeInfoPtr->bufPos, sigBufSize, &sigSize, 
					CRYPT_FORMAT_PGP, 
					envelopeInfoPtr->postActionList->iCryptHandle, 
					envelopeInfoPtr->actionList->iCryptHandle, &sigParams );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, ENVELOPE_ERRINFO,
				  "Couldn't emit signature to envelope trailer" ) );
		}
	envelopeInfoPtr->bufPos += sigSize;

	/* Now that we've written the final data, set the end-of-segment-data 
	   pointer to the end of the data in the buffer so that 
	   copyFromEnvelope() can copy out the remaining data */
	envelopeInfoPtr->segmentDataEnd = envelopeInfoPtr->bufPos;
	envelopeInfoPtr->envState = ENVSTATE_DONE;

	ENSURES( sanityCheck( envelopeInfoPtr ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Envelope Access Routines						*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initPGPEnveloping( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	int algorithm, dummy, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES_V( !( envelopeInfoPtr->flags & ENVELOPE_ISDEENVELOPE ) );

	/* Set the access method pointers */
	envelopeInfoPtr->processPreambleFunction = emitPreamble;
	envelopeInfoPtr->processPostambleFunction = emitPostamble;
	envelopeInfoPtr->checkAlgo = pgpCheckAlgo;

	/* Set up the processing state information */
	envelopeInfoPtr->envState = ENVSTATE_NONE;

	/* Remember the current default settings for use with the envelope.  
	   Since the PGP algorithms represent only a subset of what's available 
	   we have to drop back to fixed values if the caller has selected 
	   something exotic */
	status = krnlSendMessage( envelopeInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &algorithm, 
							  CRYPT_OPTION_ENCR_HASH );
	if( cryptStatusError( status ) || \
		cryptStatusError( cryptlibToPgpAlgo( algorithm, &dummy ) ) )
		envelopeInfoPtr->defaultHash = CRYPT_ALGO_SHA1;
	else
		envelopeInfoPtr->defaultHash = algorithm;	/* int vs.enum */
	status = krnlSendMessage( envelopeInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &algorithm, 
							  CRYPT_OPTION_ENCR_ALGO );
	if( cryptStatusError( status ) || \
		cryptStatusError( cryptlibToPgpAlgo( algorithm, &dummy ) ) )
		envelopeInfoPtr->defaultAlgo = CRYPT_ALGO_AES;
	else
		envelopeInfoPtr->defaultAlgo = algorithm;	/* int vs.enum */
	envelopeInfoPtr->defaultMAC = CRYPT_ALGO_NONE;

	/* Turn off segmentation of the envelope payload.  PGP has a single 
	   length at the start of the data and doesn't segment the payload */
	envelopeInfoPtr->dataFlags |= ENVDATA_NOSEGMENT;
	}
#endif /* USE_PGP */
