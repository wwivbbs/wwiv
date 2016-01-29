/****************************************************************************
*																			*
*							Write CMP Messages Types						*
*						Copyright Peter Gutmann 1999-2009					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "session.h"
  #include "cmp.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "session/session.h"
  #include "session/cmp.h"
#endif /* Compiler-specific includes */

#ifdef USE_CMP

/****************************************************************************
*																			*
*								PKI Body Functions							*
*																			*
****************************************************************************/

/* Write request body:

	body			[n]	EXPLICIT SEQUENCE {	-- n designates ir/cr/kur/rr
						...					-- CRMF request
					} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeRequestBody( INOUT STREAM *stream,
							 INOUT SESSION_INFO *sessionInfoPtr,
							 const CMP_PROTOCOL_INFO *protocolInfo )
	{
	const CRYPT_CERTFORMAT_TYPE certType = \
				( protocolInfo->operation == CTAG_PB_RR ) ? \
				CRYPT_ICERTFORMAT_DATA : CRYPT_CERTFORMAT_CERTIFICATE;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Find out how big the payload will be.  Since revocation requests are
	   unsigned entities we have to vary the attribute type that we're 
	   reading (via the certType specifier) based on whether we're 
	   submitting a signed or unsigned object in the request */
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( sessionInfoPtr->iCertRequest,
							  IMESSAGE_CRT_EXPORT, &msgData, certType );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the request body */
	writeConstructed( stream, objSize( msgData.length ),
					  protocolInfo->operation );
	writeSequence( stream, msgData.length );
	return( exportCertToStream( stream, sessionInfoPtr->iCertRequest, 
								certType ) );
	}

/* Write response body:

	body			[n] EXPLICIT SEQUENCE {	-- n designates ir/cr/kur
		response		SEQUENCE {
						SEQUENCE {
			certReqID	INTEGER (0),		-- Not present for rp's
			status		PKIStatusInfo,
			certKeyPair	SEQUENCE {
Either			cert[0]	EXPLICIT Certificate,	-- For sig-capable key
or				cmsEncCert					-- For encr-only key
					[2] EXPLICIT EncryptedCert
					}
				}
			}
		}

	body			[n] EXPLICIT SEQUENCE {	-- n designates rr
		response		SEQUENCE {
			status		PKIStatusInfo
			}
		}

   If we're returning an encryption-only certificate we send it as standard 
   CMS data under a new tag to avoid having to hand-assemble the garbled 
   mess that CMP uses for this */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeResponseBodyHeader( INOUT STREAM *stream, 
									IN_ENUM_OPT( CMP_MESSAGE ) \
										const CMP_MESSAGE_TYPE operationType,
									IN_LENGTH_SHORT_Z const int payloadSize )
	{
	const int respType = reqToResp( operationType );
	const int statusInfoLength = sizeofPkiStatusInfo( CRYPT_OK, 0 );
	int totalPayloadSize;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( operationType >= CTAG_PB_IR && operationType < CTAG_PB_LAST );
			  /* CTAG_PB_IR == 0 so this is the same as _NONE */
	REQUIRES( payloadSize >= 0 && payloadSize < MAX_INTLENGTH_SHORT );
	REQUIRES( respType >= CTAG_PB_IP && respType < CTAG_PB_LAST );
	
	/* If it's a revocation response then the only content is the 
	   OK-status */
	if( respType == CTAG_PB_RP )
		{
		writeConstructed( stream, objSize( objSize( statusInfoLength ) ),
						  CTAG_PB_RP );
		writeSequence( stream, objSize( statusInfoLength ) );
		writeSequence( stream, statusInfoLength );
		return( writePkiStatusInfo( stream, CRYPT_OK, 0 ) );
		}

	/* Calculate the overall size of the header and payload */
	totalPayloadSize = payloadSize + sizeofShortInteger( 0 ) + \
					   statusInfoLength;

	/* Write the response body wrapper */
	writeConstructed( stream, objSize( objSize( objSize( totalPayloadSize ) ) ),
					  respType );
	writeSequence( stream, objSize( objSize( totalPayloadSize ) ) );

	/* Write the response.  We always write an OK status here because an 
	   error will have been communicated by sending an explicit error 
	   response */
	writeSequence( stream, objSize( totalPayloadSize ) );
	writeSequence( stream, totalPayloadSize );
	writeShortInteger( stream, 0, DEFAULT_TAG );
	return( writePkiStatusInfo( stream, CRYPT_OK, 0 ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeEncryptedResponseBody( INOUT STREAM *stream,
									   INOUT SESSION_INFO *sessionInfoPtr,
									   const CMP_PROTOCOL_INFO *protocolInfo )
	{
	MESSAGE_DATA msgData;
	ERROR_INFO errorInfo;
	void *srcPtr, *destPtr;
	int dataLength, destLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Get a pointer into the stream buffer.  To avoid having to juggle two
	   buffers we use the stream buffer some distance ahead of the write
	   position as a temporary location to store the encoded certificate for
	   encryption:

		  buffer				 srcPtr
			|						|
			v						v
			+-------+---+-----------+-----------------------+
			|  Hdr	|-->|			|						|
			+-------+---+-----------+-----------------------+
						|<-- 100 -->|<---- dataLength ----->| */
	status = sMemGetDataBlockRemaining( stream, &srcPtr, &dataLength );
	if( cryptStatusError( status ) )
		return( status );
	srcPtr = ( BYTE * ) srcPtr + 100;
	dataLength -= 100;
	ENSURES( dataLength >= 1024 && dataLength < sMemDataLeft( stream ) );

	/* Extract the response data into the session buffer and wrap it using 
	   the client's certificate.  Since the client doesn't actually have the 
	   certificate yet (only we have it, since it's only just been issued) 
	   we have to use the S/MIME v3 format (keys identified by key ID rather 
	   than issuerAndSerialNumber) because the client won't know its iAndS 
	   until it decrypts the certificate */
	setMessageData( &msgData, srcPtr, dataLength );
	status = krnlSendMessage( sessionInfoPtr->iCertResponse,
							  IMESSAGE_CRT_EXPORT, &msgData,
							  CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	status = envelopeWrap( srcPtr, msgData.length, srcPtr, dataLength, 
						   &dataLength, CRYPT_FORMAT_CRYPTLIB, 
						   CRYPT_CONTENT_NONE, 
						   sessionInfoPtr->iCertResponse, &errorInfo );
	if( cryptStatusError( status ) )
		{
		retExtErr( status,
				   ( status, SESSION_ERRINFO, &errorInfo, 
					 "Couldn't wrap response data: " ) );
		}

	/* Write the response body header */
	status = writeResponseBodyHeader( stream, protocolInfo->operation, 
									  objSize( objSize( dataLength ) ) );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the encrypted certificate.  In theory we could use an swrite()
	   to move the data rather than a memmove() directly into the buffer but 
	   this is a bit risky because the read position is only about 30-40 
	   bytes ahead of the write position and it's not guaranteed that the 
	   two won't interfere:

		  buffer	 destPtr	 srcPtr
			|			|			|
			v			v			v
			+-----------+-----------+-----------------------+
			|	Hdr		|			|						|
			+-----------+-----------+-----------------------+
						|			|<---- dataLength ----->|
						|<---------- destLength ----------->| */
	writeSequence( stream, objSize( dataLength ) );
	writeConstructed( stream, dataLength, CTAG_CK_NEWENCRYPTEDCERT );
	status = sMemGetDataBlockRemaining( stream, &destPtr, &destLength );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( dataLength <= destLength );
			 /* Should never occur since it implies that we're overwritten
				the start of the wrapped certificate */
	memmove( destPtr, srcPtr, dataLength );
	return( sSkip( stream, dataLength, SSKIP_MAX ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeResponseBody( INOUT STREAM *stream,
							  INOUT SESSION_INFO *sessionInfoPtr,
							  const CMP_PROTOCOL_INFO *protocolInfo )
	{
	MESSAGE_DATA msgData;
	int dataLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Revocation request responses have no body */
	if( protocolInfo->operation == CTAG_PB_RR )
		return( writeResponseBodyHeader( stream, protocolInfo->operation, 
										 0 ) );

	/* If it's an encryption-only key we return the certificate in encrypted
	   form, the client performs POP by decrypting the returned 
	   certificate */
	if( protocolInfo->cryptOnlyKey )
		return( writeEncryptedResponseBody( stream, sessionInfoPtr, 
											protocolInfo ) );

	/* Write the response body header */
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( sessionInfoPtr->iCertResponse, 
							  IMESSAGE_CRT_EXPORT, &msgData,
							  CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	dataLength = msgData.length;
	status = writeResponseBodyHeader( stream, protocolInfo->operation, 
									  objSize( objSize( dataLength ) ) );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the certificate data */
	writeSequence( stream, objSize( dataLength ) );
	writeConstructed( stream, dataLength, CTAG_CK_CERT );
	return( exportCertToStream( stream, sessionInfoPtr->iCertResponse, 
								CRYPT_CERTFORMAT_CERTIFICATE ) );
	}

/* Write conf body:

	body		   [19]	EXPLICIT SEQUENCE {
						SEQUENCE {
		certHash		OCTET STRING
		certReqID		INTEGER (0),
			}
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeConfBody( INOUT STREAM *stream,
						  INOUT SESSION_INFO *sessionInfoPtr,
						  const CMP_PROTOCOL_INFO *protocolInfo )
	{
	static const MAP_TABLE fingerprintMapTable[] = {
		{ CRYPT_ALGO_SHA1, CRYPT_CERTINFO_FINGERPRINT_SHA1 },
		{ CRYPT_ALGO_SHA2, CRYPT_CERTINFO_FINGERPRINT_SHA2 },
		{ CRYPT_ALGO_SHAng, CRYPT_CERTINFO_FINGERPRINT_SHAng },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	MESSAGE_DATA msgData;
	BYTE hashBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int length, fingerprintType, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Get the certificate hash */
	status = mapValue( protocolInfo->confHashAlgo, &fingerprintType,
					   fingerprintMapTable, 
					   FAILSAFE_ARRAYSIZE( fingerprintMapTable, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	setMessageData( &msgData, hashBuffer, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( sessionInfoPtr->iCertResponse,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  fingerprintType );
	if( cryptStatusError( status ) )
		return( status );
	length = ( int ) objSize( msgData.length ) + sizeofShortInteger( 0 );

	/* Write the confirmation body */
	writeConstructed( stream, objSize( objSize( length ) ),
					  CTAG_PB_CERTCONF );
	writeSequence( stream, objSize( length ) );
	writeSequence( stream, length );
	writeOctetString( stream, hashBuffer, msgData.length, DEFAULT_TAG );
	return( writeShortInteger( stream, 0, DEFAULT_TAG ) );
	}

/* Write pkiConf body:

	body		   [24]	EXPLICIT NULL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writePKIConfBody( INOUT STREAM *stream,
							 STDC_UNUSED SESSION_INFO *sessionInfoPtr,
							 STDC_UNUSED const CMP_PROTOCOL_INFO *protocolInfo )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	writeConstructed( stream, sizeofNull(), CTAG_PB_PKICONF );
	return( writeNull( stream, DEFAULT_TAG ) );
	}

/* Write genMsg body:

	body		   [21]	EXPLICIT SEQUENCE OF {
						SEQUENCE {
		infoType		OBJECT IDENTIFIER,
		intoValue		ANY DEFINED BY infoType OPTIONAL
						}
					} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeGenMsgRequestBody( INOUT STREAM *stream,
							STDC_UNUSED SESSION_INFO *sessionInfoPtr,
							STDC_UNUSED const CMP_PROTOCOL_INFO *protocolInfo )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	writeConstructed( stream, objSize( objSize( sizeofOID( OID_PKIBOOT ) ) ),
					  CTAG_PB_GENM );
	writeSequence( stream, objSize( sizeofOID( OID_PKIBOOT ) ) );
	writeSequence( stream, sizeofOID( OID_PKIBOOT ) );
	return( writeOID( stream, OID_PKIBOOT ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeGenMsgResponseBody( INOUT STREAM *stream,
							INOUT SESSION_INFO *sessionInfoPtr,
							STDC_UNUSED const CMP_PROTOCOL_INFO *protocolInfo )
	{
	CRYPT_CERTIFICATE iCTL;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Get the CTL from the CA object.  We recreate this each time rather 
	   than cacheing it in the session to ensure that changes in the trusted
	   certificate set while the session is active get reflected back to the 
	   caller.
	   
	   In addition to the explicitly trusted certificates we also include 
	   the CA certificate(s) in the CTL as implicitly-trusted certificates.  
	   This is done both because users often forget to mark them as trusted 
	   on the server and then wonder where their CA certificates are on the 
	   client, and because these should inherently be trusted since the user 
	   is about to get their certificates issued by them */
	status = krnlSendMessage( sessionInfoPtr->ownerHandle,
							  IMESSAGE_GETATTRIBUTE, &iCTL,
							  CRYPT_IATTRIBUTE_CTL );
	if( cryptStatusError( status ) )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		if( status != CRYPT_ERROR_NOTFOUND )
			return( status );

		/* If there are no trusted certificates present then we won't be 
		   able to assemble a CTL, so we explicitly create an empty CTL to 
		   add the CA certificate to */
		setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_CERTCHAIN );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
								  OBJECT_TYPE_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( status );
		iCTL = createInfo.cryptHandle;
		}
	status = krnlSendMessage( iCTL, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &sessionInfoPtr->privateKey,
							  CRYPT_IATTRIBUTE_CERTCOLLECTION );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCTL, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( iCTL, IMESSAGE_CRT_EXPORT, &msgData, 
							  CRYPT_CERTFORMAT_CERTCHAIN );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCTL, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Write the response body wrapper.  As with the certificate ID, we can 
	   use the imprecision of the ASN.1 that CMP is specified in to 
	   interpret the InfoTypeAndValue:

		InfoTypeAndValue ::= SEQUENCE {
			infoType	OBJECT IDENTIFIER,
			infoValue	ANY DEFINED BY infoType OPTIONAL
			}

	   as:

		infoType ::= id-signedData
		infoValue ::= [0] EXPLICIT SignedData

	   which makes it standard CMS data that can be passed directly to the 
	   CMS code */
	writeConstructed( stream, objSize( msgData.length ), CTAG_PB_GENP );
	writeSequence( stream, msgData.length );
	status = exportCertToStream( stream, iCTL, CRYPT_CERTFORMAT_CERTCHAIN );
	krnlSendNotifier( iCTL, IMESSAGE_DECREFCOUNT );
	return( status );
	}

/* Write error body:

	body		   [23]	EXPLICIT SEQUENCE {
		status			PKIStatusInfo
					} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int writeErrorBody( INOUT STREAM *stream,
						   STDC_UNUSED SESSION_INFO *sessionInfoPtr,
						   const CMP_PROTOCOL_INFO *protocolInfo )
	{
	const int statusInfoLength = \
		sizeofPkiStatusInfo( protocolInfo->status, protocolInfo->pkiFailInfo );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	REQUIRES( statusInfoLength > 0 && \
			  statusInfoLength < MAX_INTLENGTH_SHORT );

	/* Write the error body */
	writeConstructed( stream, objSize( statusInfoLength ), CTAG_PB_ERROR );
	writeSequence( stream, statusInfoLength );
	return( writePkiStatusInfo( stream, protocolInfo->status,
								protocolInfo->pkiFailInfo ) );
	}

/****************************************************************************
*																			*
*						Write Function Access Information					*
*																			*
****************************************************************************/

typedef struct {
	const CMPBODY_TYPE type;
	const WRITEMESSAGE_FUNCTION function;
	} MESSAGEWRITE_INFO;
static const MESSAGEWRITE_INFO FAR_BSS messageWriteClientTable[] = {
	{ CMPBODY_NORMAL, writeRequestBody },
	{ CMPBODY_CONFIRMATION, writeConfBody },
	{ CMPBODY_GENMSG, writeGenMsgRequestBody },
	{ CMPBODY_ERROR, writeErrorBody },
	{ CMPBODY_NONE, NULL }, { CMPBODY_NONE, NULL }
	};
static const MESSAGEWRITE_INFO FAR_BSS messageWriteServerTable[] = {
	{ CMPBODY_NORMAL, writeResponseBody },
	{ CMPBODY_ACK, writePKIConfBody },
	{ CMPBODY_GENMSG, writeGenMsgResponseBody },
	{ CMPBODY_ERROR, writeErrorBody },
	{ CMPBODY_NONE, NULL }, { CMPBODY_NONE, NULL }
	};

CHECK_RETVAL_PTR \
WRITEMESSAGE_FUNCTION getMessageWriteFunction( IN_ENUM( CMPBODY ) \
									const CMPBODY_TYPE bodyType,
									const BOOLEAN isServer )
	{
	int i;

	REQUIRES_N( bodyType > CMPBODY_NONE && bodyType < CMPBODY_LAST );

	if( isServer )
		{
		for( i = 0; 
			 messageWriteServerTable[ i ].type != CMPBODY_NONE && \
				i < FAILSAFE_ARRAYSIZE( messageWriteServerTable, MESSAGEWRITE_INFO ); 
			 i++ )
			{
			if( messageWriteServerTable[ i ].type == bodyType )
				return( messageWriteServerTable[ i ].function );
			}
		ENSURES_N( i < FAILSAFE_ARRAYSIZE( messageWriteServerTable, MESSAGEWRITE_INFO ) );
		}
	else
		{
		for( i = 0; 
			 messageWriteClientTable[ i ].type != CMPBODY_NONE && \
				i < FAILSAFE_ARRAYSIZE( messageWriteClientTable, MESSAGEWRITE_INFO ); 
			 i++ )
			{
			if( messageWriteClientTable[ i ].type == bodyType )
				return( messageWriteClientTable[ i ].function );
			}
		ENSURES_N( i < FAILSAFE_ARRAYSIZE( messageWriteClientTable, MESSAGEWRITE_INFO ) );
		}

	return( NULL );
	}
#endif /* USE_CMP */
