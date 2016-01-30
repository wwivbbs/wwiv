/****************************************************************************
*																			*
*						 cryptlib TSP Session Management					*
*						Copyright Peter Gutmann 1999-2011					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "session.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "session/session.h"
#endif /* Compiler-specific includes */

#ifdef USE_TSP

/* TSP constants */

#define TSP_VERSION					1	/* Version number */
#define MIN_MSGIMPRINT_SIZE			( 2 + 10 + 16 )	/* SEQ + MD5 OID + MD5 hash */
#define MAX_MSGIMPRINT_SIZE			( 32 + CRYPT_MAX_HASHSIZE )

/* TSP HTTP content types */

#define TSP_CONTENT_TYPE_REQ		"application/timestamp-query"
#define TSP_CONTENT_TYPE_REQ_LEN	27
#define TSP_CONTENT_TYPE_RESP		"application/timestamp-reply"
#define TSP_CONTENT_TYPE_RESP_LEN	27

/* TSP protocol state information.  This is passed around the various
   subfunctions that handle individual parts of the protocol */

typedef struct {
	/* TSP protocol control information.  The hashAlgo is usually unset (so 
	   it has a value of CRYPT_ALGO_NONE) but may be set if the client has
	   indicated that they want to use a stronger hash algorithm than the 
	   default one */
	CRYPT_ALGO_TYPE hashAlgo;			/* Optional hash algorithm for TSA resp.*/
	BOOLEAN includeSigCerts;			/* Whether to include signer certificates */

	/* TSP request/response data */
	BUFFER( MAX_MSGIMPRINT_SIZE, msgImprintSize ) \
	BYTE msgImprint[ MAX_MSGIMPRINT_SIZE + 8 ];
	int msgImprintSize;					/* Message imprint */
	BUFFER( CRYPT_MAX_HASHSIZE, nonceSize ) \
	BYTE nonce[ CRYPT_MAX_HASHSIZE + 8 ];
	int nonceSize;						/* Nonce (if present) */

	} TSP_PROTOCOL_INFO;

/* Prototypes for functions in cmp_rd.c.  This code is shared due to TSP's use
   of random elements cut & pasted from CMP */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readPkiStatusInfo( INOUT STREAM *stream, 
					   const BOOLEAN isServer,
					   INOUT ERROR_INFO *errorInfo );

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Read a TSP request:

	TSARequest ::= SEQUENCE {
		version				INTEGER (1),
		msgImprint			MessageDigest,
		policy				OBJECT IDENTIFIER OPTIONAL,
												-- Ignored
		nonce				INTEGER OPTIONAL,	-- Copy to output if present
		includeSigCerts		BOOLEAN DEFAULT FALSE,
												-- Include signer certs if set
		extensions		[0]	Extensions OPTIONAL	-- Ignored, see below
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int readTSPRequest( INOUT STREAM *stream, 
						   INOUT TSP_PROTOCOL_INFO *protocolInfo,
						   IN_HANDLE const CRYPT_USER iOwnerHandle, 
						   INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_ALGO_TYPE defaultHashAlgo, msgImprintHashAlgo;
	STREAM msgImprintStream;
	void *dataPtr = DUMMY_INIT_PTR;
	long value;
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( protocolInfo, sizeof( TSP_PROTOCOL_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( iOwnerHandle == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iOwnerHandle ) );

	/* Read the request header and make sure everything is in order */
	readSequence( stream, NULL );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) || value != TSP_VERSION )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Invalid TSP request header" ) );
		}

	/* Read the message imprint.  We don't really care what this is so we
	   just treat it as a blob */
	status = getStreamObjectLength( stream, &length );
	if( cryptStatusOK( status ) )
		status = sMemGetDataBlock( stream, &dataPtr, length );
	if( cryptStatusOK( status ) )
		{
		if( length < MIN_MSGIMPRINT_SIZE || \
			length > MAX_MSGIMPRINT_SIZE || \
			cryptStatusError( sSkip( stream, length ) ) )
			status = CRYPT_ERROR_BADDATA;
		}
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Invalid TSP message imprint data" ) );
		}
	ANALYSER_HINT( dataPtr != NULL );
	memcpy( protocolInfo->msgImprint, dataPtr, length );
	protocolInfo->msgImprintSize = length;

	/* Pick apart the msgImprint:

		msgImprint			SEQUENCE {
			algorithm		AlgorithmIdentifier,
			hash			OCTET STRING
			}

	   to see whether we can use a stronger hash in our response than the 
	   default SHA-1.  This is done on the basis that if the client sends us
	   a message imprint with a stronger hash then they should be able to
	   process a response with a stronger hash as well */
	sMemConnect( &msgImprintStream, protocolInfo->msgImprint, 
				 protocolInfo->msgImprintSize );
	readSequence( &msgImprintStream, NULL );
	status = readAlgoID( &msgImprintStream, &msgImprintHashAlgo, 
						 ALGOID_CLASS_HASH );
	if( cryptStatusOK( status ) )
		status = readOctetStringHole( &msgImprintStream, NULL, 16, 
									  DEFAULT_TAG );
	sMemDisconnect( &msgImprintStream );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Invalid TSP message imprint content" ) );
		}
	status = krnlSendMessage( iOwnerHandle, IMESSAGE_GETATTRIBUTE, 
							  &defaultHashAlgo, CRYPT_OPTION_ENCR_HASH );
	if( cryptStatusOK( status ) && \
		isStrongerHash( msgImprintHashAlgo, defaultHashAlgo ) )
		protocolInfo->hashAlgo = msgImprintHashAlgo;

	/* Check for the presence of the assorted optional fields */
	if( peekTag( stream ) == BER_OBJECT_IDENTIFIER )
		{
		/* This could be anything since it's defined as "by prior agreement"
		   so we ignore it and give them whatever policy we happen to
		   implement, if they don't like it they're free to ignore it */
		status = readUniversal( stream );
		}
	if( cryptStatusOK( status ) && peekTag( stream ) == BER_INTEGER )
		{
		/* For some unknown reason the nonce is encoded as an INTEGER 
		   instead of an OCTET STRING, so in theory we'd have to jump 
		   through all sorts of hoops to handle it because it's really an 
		   OCTET STRING blob dressed up as an INTEGER.  To avoid this mess,
		   we just read it as a blob and memcpy() it back to the output */
		status = readRawObject( stream, protocolInfo->nonce,
								CRYPT_MAX_HASHSIZE, 
								&protocolInfo->nonceSize, BER_INTEGER );
		}
	if( cryptStatusOK( status ) && peekTag( stream ) == BER_BOOLEAN )
		status = readBoolean( stream, &protocolInfo->includeSigCerts );
	if( cryptStatusOK( status ) && peekTag( stream ) == MAKE_CTAG( 0 ) )
		{
		/* The TSP RFC specifies a truly braindamaged interpretation of
		   extension handling, added at the last minute with no debate or
		   discussion.  This says that extensions are handled just like RFC
		   2459 except when they're not.  In particular it requires that you
		   reject all extensions that you don't recognise, even if they
		   don't have the critical bit set (in violation of RFC 2459).
		   Since "recognise" is never defined and the spec doesn't specify
		   any particular extensions that must be handled (via MUST/SHALL/
		   SHOULD), any extension at all is regarded as unrecognised in the
		   context of the RFC.  For example if a request with a
		   subjectAltName is submitted then although the TSA knows perfectly
		   well what a subjectAltName, it has no idea what it's supposed to
		   do with it when it sees it in the request.  Since the semantics of
		   all extensions are unknown (in the context of the RFC), any
		   request with extensions has to be rejected.

		   Along with assorted other confusing and often contradictory terms
		   added in the last-minute rewrite, cryptlib ignores this
		   requirement and instead uses the common-sense interpretation of
		   allowing any extension that the RFC doesn't specifically provide
		   semantics for.  Since it doesn't provide semantics for any
		   extension, we allow anything */
		status = readUniversal( stream );
		}
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Invalid TSP request additional information fields" ) );
		}
	return( CRYPT_OK );
	}

/* Sign a timestamp token */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int signTSToken( OUT_BUFFER( tsaRespMaxLength, *tsaRespLength ) BYTE *tsaResp, 
						IN_LENGTH_SHORT_MIN( 64 ) const int tsaRespMaxLength, 
						OUT_LENGTH_SHORT_Z int *tsaRespLength,
						IN_ALGO_OPT const CRYPT_ALGO_TYPE tsaRespHashAlgo,
						IN_BUFFER( tstInfoLength ) const BYTE *tstInfo, 
						IN_LENGTH_SHORT const int tstInfoLength,
						IN_HANDLE const CRYPT_CONTEXT privateKey,
						const BOOLEAN includeCerts )
	{
	CRYPT_CERTIFICATE iCmsAttributes;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	DYNBUF essCertDB;
	static const int minBufferSize = MIN_BUFFER_SIZE;
	static const int contentType = CRYPT_CONTENT_TSTINFO;
	int status;

	assert( isWritePtr( tsaResp, tsaRespMaxLength ) );
	assert( isWritePtr( tsaRespLength, sizeof( int ) ) );
	assert( isReadPtr( tstInfo, tstInfoLength ) );

	REQUIRES( tsaRespMaxLength >= 64 && \
			  tsaRespMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( tsaRespHashAlgo == CRYPT_ALGO_NONE || \
			  ( tsaRespHashAlgo >= CRYPT_ALGO_FIRST_HASH && \
				tsaRespHashAlgo <= CRYPT_ALGO_LAST_HASH ) );
	REQUIRES( tstInfoLength > 0 && tstInfoLength < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( privateKey ) );

	/* Clear return values */
	memset( tsaResp, 0, min( 16, tsaRespMaxLength ) );
	*tsaRespLength = 0;

	/* Create the signing attributes.  We don't have to set the content-type
	   attribute since it'll be set automatically based on the envelope
	   content type */
	setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_CMS_ATTRIBUTES );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	iCmsAttributes = createInfo.cryptHandle;
	status = dynCreate( &essCertDB, privateKey, CRYPT_IATTRIBUTE_ESSCERTID );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, dynData( essCertDB ),
						dynLength( essCertDB ) );
		status = krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE_S,
						&msgData, CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID );
		dynDestroy( &essCertDB );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Create a cryptlib envelope to sign the data.  If we're not being
	   asked to include signer certificates we have to explicitly disable 
	   the inclusion of certificates in the signature since S/MIME includes 
	   them by default.  In addition the caller may have asked us to use a
	   non-default hash algorithm, which we specify for the envelope if it's
	   been set.  Unfortunately these special-case operations mean that we 
	   can't use envelopeSign() to process the data, but have to perform the 
	   whole process ourselves */
	setMessageCreateObjectInfo( &createInfo, CRYPT_FORMAT_CMS );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_ENVELOPE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &minBufferSize,
							  CRYPT_ATTRIBUTE_BUFFERSIZE );
	if( cryptStatusOK( status ) && tsaRespHashAlgo != CRYPT_ALGO_NONE )
		status = krnlSendMessage( createInfo.cryptHandle,
							IMESSAGE_SETATTRIBUTE, 
							( MESSAGE_CAST ) &tsaRespHashAlgo,
							CRYPT_OPTION_ENCR_HASH );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle,
							IMESSAGE_SETATTRIBUTE, ( MESSAGE_CAST ) &tstInfoLength,
							CRYPT_ENVINFO_DATASIZE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle,
							IMESSAGE_SETATTRIBUTE, ( MESSAGE_CAST ) &contentType,
							CRYPT_ENVINFO_CONTENTTYPE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle,
							IMESSAGE_SETATTRIBUTE, ( MESSAGE_CAST ) &privateKey,
							CRYPT_ENVINFO_SIGNATURE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle,
							IMESSAGE_SETATTRIBUTE, &iCmsAttributes,
							CRYPT_ENVINFO_SIGNATURE_EXTRADATA );
	if( cryptStatusOK( status ) && !includeCerts )
		status = krnlSendMessage( createInfo.cryptHandle,
							IMESSAGE_SETATTRIBUTE, MESSAGE_VALUE_FALSE,
							CRYPT_IATTRIBUTE_INCLUDESIGCERT );
	krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Push in the data and pop the signed result */
	setMessageData( &msgData, ( MESSAGE_CAST ) tstInfo, tstInfoLength );
	status = krnlSendMessage( createInfo.cryptHandle,
							  IMESSAGE_ENV_PUSHDATA, &msgData, 0 );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_ENV_PUSHDATA, &msgData, 0 );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, tsaResp, tsaRespMaxLength );
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_ENV_POPDATA, &msgData, 0 );
		*tsaRespLength = msgData.length;
		}
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );

	return( status );
	}

/****************************************************************************
*																			*
*							Client-side Functions							*
*																			*
****************************************************************************/

/* Send a request to a TSP server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sendClientRequest( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT TSP_PROTOCOL_INFO *protocolInfo )
	{
	TSP_INFO *tspInfo = sessionInfoPtr->sessionTSP;
	STREAM stream;
	void *msgImprintPtr;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( TSP_PROTOCOL_INFO ) ) );

	/* Create the encoded request:

		TSARequest ::= SEQUENCE {
			version				INTEGER (1),
			msgImprint			MessageDigest,
			includeSigCerts		BOOLEAN TRUE
			}
	
	   We have to ask for the inclusion of signing certificates in the
	   response (by default they're not included) because the caller may not 
	   have the TSA's certificate, or may have an out-of-date version 
	   depending on how frequently the TSA rolls over certificates.  This 
	   tends to bloat up the response somewhat, but it's only way to deal
	   with the certificate issue without requiring lots of manual 
	   certificate-processing from the caller.

	   When we write the message imprint as a hash value we save a copy of
	   the encoded data so that we can check it against the returned
	   timestamp, see the comment in readServerResponse() for details */
	protocolInfo->msgImprintSize = \
							sizeofMessageDigest( tspInfo->imprintAlgo,
												 tspInfo->imprintSize );
	ENSURES( protocolInfo->msgImprintSize > 0 && \
			 protocolInfo->msgImprintSize <= MAX_MSGIMPRINT_SIZE );
	sMemOpen( &stream, sessionInfoPtr->receiveBuffer, 1024 );
	writeSequence( &stream, sizeofShortInteger( TSP_VERSION ) + \
							protocolInfo->msgImprintSize + \
							sizeofBoolean() );
	writeShortInteger( &stream, TSP_VERSION, DEFAULT_TAG );
	status = sMemGetDataBlock( &stream, &msgImprintPtr, 
							   protocolInfo->msgImprintSize );
	ENSURES( cryptStatusOK( status ) );
	writeMessageDigest( &stream, tspInfo->imprintAlgo, 
						tspInfo->imprint, tspInfo->imprintSize );
	memcpy( protocolInfo->msgImprint, msgImprintPtr,
			protocolInfo->msgImprintSize );
	status = writeBoolean( &stream, TRUE, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		sessionInfoPtr->receiveBufEnd = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	DEBUG_DUMP_FILE( "tsa_req", sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufEnd );

	/* Send the request to the server */
	return( writePkiDatagram( sessionInfoPtr, TSP_CONTENT_TYPE_REQ,
							  TSP_CONTENT_TYPE_REQ_LEN ) );
	}

/* Read the response from the TSP server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readServerResponse( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT TSP_PROTOCOL_INFO *protocolInfo )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( TSP_PROTOCOL_INFO ) ) );

	/* Reset the buffer position indicators to clear any old data in the
	   buffer from previous transactions */
	sessionInfoPtr->receiveBufEnd = sessionInfoPtr->receiveBufPos = 0;

	/* Read the response data from the server */
	status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* Strip off the header and check the PKIStatus wrapper to make sure
	   that everything's OK:

		SEQUENCE {
			status	SEQUENCE {
				status	INTEGER,			-- 0 = OK
						... OPTIONAL
				}
			... */
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer,
				 sessionInfoPtr->receiveBufEnd );
	readSequence( &stream, NULL );
	status = readPkiStatusInfo( &stream, FALSE, 
								&sessionInfoPtr->errorInfo );
	if( cryptStatusError( status ) )
		{
		/* readPkiStatusInfo() has already set the extended error 
		   information */
		sMemDisconnect( &stream );
		return( status );
		}

	/* Remember where the encoded timestamp payload starts in the buffer so
	   that we can return it to the caller */
	sessionInfoPtr->receiveBufPos = stell( &stream );

	/* Make sure that we got back a timestamp of the value that we sent.  
	   This check means that it works with and without nonces (in theory 
	   someone could repeatedly contersign the same signature rather than 
	   countersigning the last timestamp as they're supposed to, but (a) 
	   that's rather unlikely and (b) cryptlib doesn't support it so they'd 
	   have to make some rather serious changes to the code to do it) */
	readSequence( &stream, NULL );		/* contentInfo */
	readUniversal( &stream );			/* contentType */
	readConstructed( &stream, NULL, 0 );/* content */
	readSequence( &stream, NULL );			/* signedData */
	readShortInteger( &stream, NULL );		/* version */
	readUniversal( &stream );				/* digestAlgos */
	readSequence( &stream, NULL );			/* encapContent */
	readUniversal( &stream );					/* contentType */
	readConstructed( &stream, NULL, 0 );		/* content */
	readOctetStringHole( &stream, NULL, 16, 
						 DEFAULT_TAG );			/* OCTET STRING hole */
	readSequence( &stream, NULL );					/* tstInfo */
	readShortInteger( &stream, NULL );				/* version */
	status = readUniversal( &stream );				/* policy */
	if( cryptStatusError( status ) )
		status = CRYPT_ERROR_BADDATA;
	else
		{
		void *msgImprintPtr;

		status = sMemGetDataBlock( &stream, &msgImprintPtr, 
								   protocolInfo->msgImprintSize );
		if( cryptStatusOK( status ) && \
			memcmp( protocolInfo->msgImprint, msgImprintPtr,
					protocolInfo->msgImprintSize ) )
			status = CRYPT_ERROR_INVALID;
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  ( status == CRYPT_ERROR_BADDATA || \
					status == CRYPT_ERROR_UNDERFLOW ) ? \
					"Invalid timestamp data" : \
					"Returned timestamp message imprint doesn't match "
					"original message imprint" ) );
		}
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Server-side Functions							*
*																			*
****************************************************************************/

/* Send an error response back to the client.  Since there are only a small
   number of these, we write back a fixed blob rather than encoding each
   one */

#define respSize( data )	( data[ 1 ] + 2 )

static const BYTE FAR_BSS respBadGeneric[] = {
	0x30, 0x05, 
		  0x30, 0x03, 
			    0x02, 0x01, 0x02		/* Rejection, unspecified reason */
	};
static const BYTE FAR_BSS respBadData[] = {
	0x30, 0x09, 
		  0x30, 0x07, 
			    0x02, 0x01, 0x02, 
				0x03, 0x02, 0x05, 0x20	/* Rejection, badDataFormat */
	};
static const BYTE FAR_BSS respBadExtension[] = {
	0x30, 0x0B, 
		  0x30, 0x09, 
				0x02, 0x01, 0x02, 
				0x03, 0x04, 0x07, 0x00, 0x00, 0x80	/* Rejection, unacceptedExtension */
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sendErrorResponse( INOUT SESSION_INFO *sessionInfoPtr,
							  const BYTE *errorResponse, 
							  IN_ERROR const int status )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( cryptStatusError( status ) );

	/* Since we're already in an error state there's not much that we can do
	   in terms of alerting the user if a further error occurs when writing 
	   the error response, so we ignore any potential write errors that occur
	   at this point */
	memcpy( sessionInfoPtr->receiveBuffer, errorResponse,
			respSize( errorResponse ) );
	sessionInfoPtr->receiveBufEnd = respSize( errorResponse );
	( void ) writePkiDatagram( sessionInfoPtr, TSP_CONTENT_TYPE_RESP,
							   TSP_CONTENT_TYPE_RESP_LEN );
	return( status );
	}

/* Read a request from a TSP client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readClientRequest( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT TSP_PROTOCOL_INFO *protocolInfo )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( TSP_PROTOCOL_INFO ) ) );

	/* Read the request data from the client */
	status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( sendErrorResponse( sessionInfoPtr, respBadGeneric, status ) );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer,
				 sessionInfoPtr->receiveBufEnd );
	status = readTSPRequest( &stream, protocolInfo, 
							 sessionInfoPtr->ownerHandle, SESSION_ERRINFO );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		return( sendErrorResponse( sessionInfoPtr, \
					( status == CRYPT_ERROR_BADDATA || \
					  status == CRYPT_ERROR_UNDERFLOW ) ? respBadData : \
					( status == CRYPT_ERROR_INVALID ) ? respBadExtension : \
					respBadGeneric, status ) );
		}
	return( CRYPT_OK );
	}

/* Send a response to the TSP client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sendServerResponse( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT TSP_PROTOCOL_INFO *protocolInfo )
	{
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE tstBuffer[ 1024 ];
	BYTE serialNo[ 16 + 8 ];
	BYTE *bufPtr = sessionInfoPtr->receiveBuffer;
	const time_t currentTime = getReliableTime( sessionInfoPtr->privateKey );
	int tstLength = DUMMY_INIT, responseLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( TSP_PROTOCOL_INFO ) ) );

	ENSURES( currentTime > MIN_TIME_VALUE );
			 /* Already checked in checkAttributeFunction() */

	/* Create a timestamp token */
	setMessageData( &msgData, serialNo, 16 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	if( cryptStatusError( status ) )
		return( status );
	sMemOpen( &stream, tstBuffer, 1024 );
	writeSequence( &stream, sizeofShortInteger( 1 ) + \
			sizeofOID( OID_TSP_POLICY ) + protocolInfo->msgImprintSize + \
			sizeofInteger( serialNo, 16 ) + sizeofGeneralizedTime() + \
			protocolInfo->nonceSize );
	writeShortInteger( &stream, 1, DEFAULT_TAG );
	writeOID( &stream, OID_TSP_POLICY );
	swrite( &stream, protocolInfo->msgImprint, protocolInfo->msgImprintSize );
	writeInteger( &stream, serialNo, 16, DEFAULT_TAG );
	status = writeGeneralizedTime( &stream, currentTime, DEFAULT_TAG );
	if( protocolInfo->nonceSize > 0 )
		status = swrite( &stream, protocolInfo->nonce,
						 protocolInfo->nonceSize );
	if( cryptStatusOK( status ) )
		tstLength = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( sendErrorResponse( sessionInfoPtr, respBadGeneric, status ) );
	
	/* Sign the token.   The reason for the min() part of the expression is 
	   that signTSToken() gets suspicious of very large buffer sizes, for 
	   example when the user has specified the use of a huge send buffer */
	status = signTSToken( sessionInfoPtr->receiveBuffer + 9,
						  min( sessionInfoPtr->receiveBufSize, \
							   MAX_INTLENGTH_SHORT - 1 ), &responseLength, 
						  protocolInfo->hashAlgo, tstBuffer, tstLength, 
						  sessionInfoPtr->privateKey, 
						  protocolInfo->includeSigCerts );
	if( cryptStatusError( status ) )
		return( sendErrorResponse( sessionInfoPtr, respBadGeneric, status ) );
	DEBUG_DUMP_FILE( "tsa_token",
					 sessionInfoPtr->receiveBuffer + 9,
					 responseLength );

	/* Add the TSA response wrapper and send it to the client.  This assumes
	   that the TSA response will be >= 256 bytes (for a 4-byte SEQUENCE
	   header encoding), which is always the case since it uses PKCS #7
	   signed data */
	REQUIRES( responseLength >= 256 && \
			  responseLength < MAX_INTLENGTH_SHORT );
	sMemOpen( &stream, bufPtr, 4 + 5 );		/* SEQ + resp.header */
	writeSequence( &stream, 5 + responseLength );
	swrite( &stream, "\x30\x03\x02\x01\x00", 5 );
	sMemDisconnect( &stream );
	sessionInfoPtr->receiveBufEnd = 9 + responseLength;
	return( writePkiDatagram( sessionInfoPtr, TSP_CONTENT_TYPE_RESP,
							  TSP_CONTENT_TYPE_RESP_LEN ) );
	}

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

/* Exchange data with a TSP client/server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int clientTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	TSP_PROTOCOL_INFO protocolInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that we have all of the needed information */
	if( sessionInfoPtr->sessionTSP->imprintSize == 0 )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_TSP_MSGIMPRINT,
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_NOTINITED );
		}

	/* Get a timestamp from the server */
	memset( &protocolInfo, 0, sizeof( TSP_PROTOCOL_INFO ) );
	status = sendClientRequest( sessionInfoPtr, &protocolInfo );
	if( cryptStatusOK( status ) )
		status = readServerResponse( sessionInfoPtr, &protocolInfo );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int serverTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	TSP_PROTOCOL_INFO protocolInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Send a timestamp to the client */
	memset( &protocolInfo, 0, sizeof( TSP_PROTOCOL_INFO ) );
	status = readClientRequest( sessionInfoPtr, &protocolInfo );
	if( cryptStatusOK( status ) )
		status = sendServerResponse( sessionInfoPtr, &protocolInfo );
	return( status );
	}

/****************************************************************************
*																			*
*					Control Information Management Functions				*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 OUT void *data, 
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	CRYPT_ENVELOPE *cryptEnvelopePtr = ( CRYPT_ENVELOPE * ) data;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	const int dataSize = sessionInfoPtr->receiveBufEnd - \
						 sessionInfoPtr->receiveBufPos;
	const int bufSize = max( dataSize + 128, MIN_BUFFER_SIZE );
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( data, sizeof( int ) ) );

	REQUIRES( type == CRYPT_SESSINFO_RESPONSE || \
			  type == CRYPT_IATTRIBUTE_ENC_TIMESTAMP );

	/* Make sure that there's actually a timestamp present (this can happen 
	   if we're using a persistent session and a subsequent transaction
	   fails, resulting in no timestamp being available) */
	if( sessionInfoPtr->receiveBufPos <= 0 )
		return( CRYPT_ERROR_NOTFOUND );

	/* If we're being asked for raw encoded timestamp data, return it
	   directly to the caller */
	if( type == CRYPT_IATTRIBUTE_ENC_TIMESTAMP )
		{
		REQUIRES( rangeCheck( sessionInfoPtr->receiveBufPos, dataSize,
							  sessionInfoPtr->receiveBufEnd ) );
		return( attributeCopy( ( MESSAGE_DATA * ) data,
					sessionInfoPtr->receiveBuffer + sessionInfoPtr->receiveBufPos,
					dataSize ) );
		}

	/* Delete any existing response if necessary */
	if( sessionInfoPtr->iCertResponse != CRYPT_ERROR )
		{
		krnlSendNotifier( sessionInfoPtr->iCertResponse,
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iCertResponse = CRYPT_ERROR;
		}

	/* We're being asked for interpreted data, create a cryptlib envelope to
	   contain it */
	setMessageCreateObjectInfo( &createInfo, CRYPT_FORMAT_AUTO );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_ENVELOPE );
	if( cryptStatusError( status ) )
		return( status );
	krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
					 ( MESSAGE_CAST ) &bufSize, 
					 CRYPT_ATTRIBUTE_BUFFERSIZE );

	/* Push in the timestamp data */
	setMessageData( &msgData, sessionInfoPtr->receiveBuffer + \
							  sessionInfoPtr->receiveBufPos, dataSize );
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_ENV_PUSHDATA,
							  &msgData, 0 );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_ENV_PUSHDATA, &msgData, 0 );
		}
	if( cryptStatusError( status ) )
		return( status );
	sessionInfoPtr->iCertResponse = createInfo.cryptHandle;

	/* Return the information to the caller */
	krnlSendNotifier( sessionInfoPtr->iCertResponse, IMESSAGE_INCREFCOUNT );
	*cryptEnvelopePtr = sessionInfoPtr->iCertResponse;
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int setAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 IN const void *data,
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	CRYPT_CONTEXT hashContext = *( ( CRYPT_CONTEXT * ) data );
	TSP_INFO *tspInfo = sessionInfoPtr->sessionTSP;
	int imprintAlgo, status;	/* int vs.enum */

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, sizeof( int ) ) );

	REQUIRES( type == CRYPT_SESSINFO_TSP_MSGIMPRINT );

	if( tspInfo->imprintSize != 0 )
		return( CRYPT_ERROR_INITED );

	/* Get the message imprint from the hash context */
	status = krnlSendMessage( hashContext, IMESSAGE_GETATTRIBUTE,
							  &imprintAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		{
		MESSAGE_DATA msgData;

		tspInfo->imprintAlgo = imprintAlgo;	/* int vs.enum */
		setMessageData( &msgData, tspInfo->imprint, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( hashContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_HASHVALUE );
		if( cryptStatusOK( status ) )
			tspInfo->imprintSize = msgData.length;
		}

	return( cryptStatusError( status ) ? CRYPT_ARGERROR_NUM1 : CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								   IN const void *data,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	const CRYPT_CONTEXT cryptContext = *( ( CRYPT_CONTEXT * ) data );
	int value, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, sizeof( int ) ) );

	REQUIRES( type > CRYPT_ATTRIBUTE_NONE && type < CRYPT_ATTRIBUTE_LAST );

	if( type != CRYPT_SESSINFO_PRIVATEKEY )
		return( CRYPT_OK );

	/* Make sure that the key is valid for timestamping */
	status = krnlSendMessage( cryptContext, IMESSAGE_CHECK, NULL,
							  MESSAGE_CHECK_PKC_SIGN );
	if( cryptStatusError( status ) )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_CERTINFO_KEYUSAGE,
					  CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ARGERROR_NUM1 );
		}
	status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE, &value,
							  CRYPT_CERTINFO_EXTKEY_TIMESTAMPING );
	if( cryptStatusError( status ) )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_CERTINFO_EXTKEY_TIMESTAMPING,
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ARGERROR_NUM1 );
		}

	/* Make sure that the time appears correct (if the time is screwed up 
	   then we can't really provide a signed indication of it to clients).  
	   The error information is somewhat misleading, but there's not much 
	   else that we can provide at this point */
	if( getReliableTime( cryptContext ) <= MIN_TIME_VALUE )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_CERTINFO_VALIDFROM,
					  CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ARGERROR_NUM1 );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodTSP( INOUT SESSION_INFO *sessionInfoPtr )
	{
	static const PROTOCOL_INFO protocolInfo = {
		/* General session information */
		TRUE,						/* Request-response protocol */
		SESSION_ISHTTPTRANSPORT,	/* Flags */
		80,							/* HTTP port */
		0,							/* Client flags */
		SESSION_NEEDS_PRIVATEKEY |	/* Server flags */
			SESSION_NEEDS_PRIVKEYSIGN | \
			SESSION_NEEDS_PRIVKEYCERT,
		1, 1, 1,					/* Version 1 */

		/* Protocol-specific information */
		BUFFER_SIZE_DEFAULT			/* Send/receive buffers */
		};

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Set the access method pointers */
	sessionInfoPtr->protocolInfo = &protocolInfo;
	if( isServer( sessionInfoPtr ) )
		sessionInfoPtr->transactFunction = serverTransact;
	else
		sessionInfoPtr->transactFunction = clientTransact;
	sessionInfoPtr->getAttributeFunction = getAttributeFunction;
	sessionInfoPtr->setAttributeFunction = setAttributeFunction;
	sessionInfoPtr->checkAttributeFunction = checkAttributeFunction;

	return( CRYPT_OK );
	}
#endif /* USE_TSP */
