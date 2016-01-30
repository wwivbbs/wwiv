/****************************************************************************
*																			*
*						 cryptlib RTCS Session Management					*
*						Copyright Peter Gutmann 1999-2008					*
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

#ifdef USE_RTCS

/* RTCS HTTP content types */

#define RTCS_CONTENT_TYPE_REQ		"application/rtcs-request"
#define RTCS_CONTENT_TYPE_REQ_LEN	24
#define RTCS_CONTENT_TYPE_RESP		"application/rtcs-response"
#define RTCS_CONTENT_TYPE_RESP_LEN	25

/* The action to take to process an RTCS request/response */

typedef enum {
	ACTION_NONE,				/* No processing */
	ACTION_UNWRAP,				/* Unwrap raw data */
	ACTION_CRYPT,				/* Decrypt data */
	ACTION_SIGN,				/* Sig.check data */
	ACTION_LAST					/* Last valid action type */
	} ACTION_TYPE;

/* RTCS protocol state information.  This is passed around various
   subfunctions that handle individual parts of the protocol */

typedef struct {
	/* State variable information.  The nonce is copied from the request to
	   the response to prevent replay attacks */
	BYTE nonce[ CRYPT_MAX_HASHSIZE + 8 ];
	int nonceSize;
	} RTCS_PROTOCOL_INFO;

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Check for a valid-looking RTCS request/response header */

static const CMS_CONTENT_INFO FAR_BSS oidInfoSignedData = { 0, 3 };
static const CMS_CONTENT_INFO FAR_BSS oidInfoEnvelopedData = { 0, 3 };

static const OID_INFO FAR_BSS envelopeOIDinfo[] = {
	{ OID_CRYPTLIB_RTCSREQ, ACTION_UNWRAP },
	{ OID_CRYPTLIB_RTCSRESP, ACTION_UNWRAP },
	{ OID_CRYPTLIB_RTCSRESP_EXT, ACTION_UNWRAP },
	{ OID_CMS_SIGNEDDATA, ACTION_SIGN, &oidInfoSignedData },
	{ OID_CMS_ENVELOPEDDATA, ACTION_CRYPT, &oidInfoEnvelopedData },
	{ NULL, 0 }, { NULL, 0 }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkRtcsHeader( IN_BUFFER( rtcsDataLength ) const void *rtcsData, 
							IN_LENGTH_SHORT const int rtcsDataLength,
							OUT_ENUM_OPT( ACTION ) ACTION_TYPE *actionType )
	{
	STREAM stream;
	int status;

	assert( isReadPtr( rtcsData, rtcsDataLength ) );
	assert( isWritePtr( actionType, sizeof( ACTION_TYPE ) ) );

	REQUIRES( rtcsDataLength > 0 && rtcsDataLength < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*actionType = ACTION_NONE;

	/* We've got a valid response, check the CMS encapsulation */
	sMemConnect( &stream, rtcsData, rtcsDataLength );
	status = readCMSheader( &stream, envelopeOIDinfo, 
							FAILSAFE_ARRAYSIZE( envelopeOIDinfo, OID_INFO ), 
							NULL, READCMS_FLAG_NONE );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	*actionType = status;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Client-side Functions							*
*																			*
****************************************************************************/

/* Send a request to an RTCS server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendClientRequest( INOUT SESSION_INFO *sessionInfoPtr )
	{
	MESSAGE_DATA msgData;
	ERROR_INFO errorInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Get the encoded request data and wrap it up for sending */
	setMessageData( &msgData, sessionInfoPtr->receiveBuffer,
					sessionInfoPtr->receiveBufSize );
	status = krnlSendMessage( sessionInfoPtr->iCertRequest,
							  IMESSAGE_CRT_EXPORT, &msgData,
							  CRYPT_ICERTFORMAT_DATA );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't get RTCS request data from RTCS request "
				  "object" ) );
		}
	status = envelopeWrap( sessionInfoPtr->receiveBuffer, msgData.length,
						   sessionInfoPtr->receiveBuffer,
						   sessionInfoPtr->receiveBufSize,
						   &sessionInfoPtr->receiveBufEnd,
						   CRYPT_FORMAT_CMS, CRYPT_CONTENT_RTCSREQUEST,
						   CRYPT_UNUSED, &errorInfo );
	if( cryptStatusError( status ) )
		{
		retExtErr( status,
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Couldn't CMS-envelope RTCS request data: " ) );
		}
	DEBUG_DUMP_FILE( "rtcs_req", sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufEnd );

	/* Send the request to the responder */
	return( writePkiDatagram( sessionInfoPtr, RTCS_CONTENT_TYPE_REQ,
							  RTCS_CONTENT_TYPE_REQ_LEN ) );
	}

/* Read the response from the RTCS server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readServerResponse( INOUT SESSION_INFO *sessionInfoPtr )
	{
	CRYPT_CERTIFICATE iCmsAttributes;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	ERROR_INFO errorInfo;
	ACTION_TYPE actionType;
	BYTE nonceBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int dataLength, sigResult, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Read the response from the responder */
	status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	DEBUG_DUMP_FILE( "rtcs_resp", sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufEnd );
	status = checkRtcsHeader( sessionInfoPtr->receiveBuffer,
							  sessionInfoPtr->receiveBufEnd, &actionType );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid RTCS response header" ) );
		}
	if( actionType != ACTION_SIGN )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Unexpected RTCS encapsulation type %d", actionType ) );
		}

	/* Sig.check the data using the responder's key */
	status = envelopeSigCheck( sessionInfoPtr->receiveBuffer,
							   sessionInfoPtr->receiveBufEnd,
							   sessionInfoPtr->receiveBuffer, 
							   sessionInfoPtr->receiveBufSize, &dataLength,
							   CRYPT_UNUSED, &sigResult, NULL,
							   &iCmsAttributes, &errorInfo );
	if( cryptStatusError( status ) )
		{
		retExtErr( status,
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Invalid CMS-enveloped RTCS response data: " ) );
		}

	/* Make sure that the nonce in the response matches the one in the
	   request */
	setMessageData( &msgData, nonceBuffer, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCmsAttributes, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_CMS_NONCE );
	krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
	if( cryptStatusOK( status ) )
		{
		MESSAGE_DATA responseMsgData;
		BYTE responseNonceBuffer[ CRYPT_MAX_HASHSIZE + 8 ];

		setMessageData( &responseMsgData, responseNonceBuffer,
						CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( sessionInfoPtr->iCertRequest,
								  IMESSAGE_GETATTRIBUTE_S, &responseMsgData,
								  CRYPT_CERTINFO_CMS_NONCE );
		if( cryptStatusOK( status ) && \
			( msgData.length < 4 || \
			  msgData.length != responseMsgData.length || \
			  memcmp( msgData.data, responseMsgData.data, msgData.length ) ) )
			status = CRYPT_ERROR_SIGNATURE;
		}
	krnlSendNotifier( sessionInfoPtr->iCertRequest, IMESSAGE_DECREFCOUNT );
	sessionInfoPtr->iCertRequest = CRYPT_ERROR;
	if( cryptStatusError( status ) )
		{
		/* The response doesn't contain a nonce or it doesn't match what
		   we sent, we can't trust it.  The best error that we can return
		   here is a signature error to indicate that the integrity check
		   failed */
		retExt( status,
				( status, SESSION_ERRINFO, 
				  ( status != CRYPT_ERROR_SIGNATURE ) ? \
				  "RTCS response doesn't contain a nonce" : \
				  "RTCS response nonce doesn't match the one in the "
				  "request" ) );
		}

	/* Everything is OK, import the response */
	setMessageCreateObjectIndirectInfo( &createInfo,
							sessionInfoPtr->receiveBuffer, dataLength,
							CRYPT_CERTTYPE_RTCS_RESPONSE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid RTCS response contents" ) );
		}
	sessionInfoPtr->iCertResponse = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Server-side Functions							*
*																			*
****************************************************************************/

/* Read a request from an RTCS client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readClientRequest( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT RTCS_PROTOCOL_INFO *protocolInfo )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	ERROR_INFO errorInfo;
	ACTION_TYPE actionType;
	int dataLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( RTCS_PROTOCOL_INFO ) ) );

	/* Read the request data from the client.  We don't write an error
	   response at this initial stage to prevent scanning/DOS attacks
	   (vir sapit qui pauca loquitur) */
	status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	DEBUG_DUMP_FILE( "rtcs_sreq", sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufEnd );
	status = checkRtcsHeader( sessionInfoPtr->receiveBuffer,
							  sessionInfoPtr->receiveBufEnd, &actionType );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid RTCS request header" ) );
		}
	if( actionType != ACTION_UNWRAP )
		{
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO,
				  "Unexpected RTCS encapsulation type %d", actionType ) );
		}
	status = envelopeUnwrap( sessionInfoPtr->receiveBuffer,
							 sessionInfoPtr->receiveBufEnd,
							 sessionInfoPtr->receiveBuffer, 
							 sessionInfoPtr->receiveBufSize, &dataLength,
							 CRYPT_UNUSED, &errorInfo );
	if( cryptStatusError( status ) )
		{
		retExtErr( status,
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Invalid CMS-enveloped RTCS request data: " ) );
		}

	/* Create an RTCS response.  We always create this since an empty
	   response is sent to indicate an error condition */
	setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_RTCS_RESPONSE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	sessionInfoPtr->iCertResponse = createInfo.cryptHandle;

	/* Import the request as a cryptlib object and read the nonce from it */
	setMessageCreateObjectIndirectInfo( &createInfo,
							sessionInfoPtr->receiveBuffer, dataLength,
							CRYPT_CERTTYPE_RTCS_REQUEST );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, protocolInfo->nonce, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_CERTINFO_CMS_NONCE );
		if( cryptStatusOK( status ) )
			protocolInfo->nonceSize = msgData.length;
		else
			{
			/* We couldn't read the nonce, delete the request object prior 
			   to exiting */
			krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
			}
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid RTCS request contents" ) );
		}

	/* Create an RTCS response and add the request information to it */
	status = krnlSendMessage( sessionInfoPtr->iCertResponse,
							  IMESSAGE_SETATTRIBUTE,
							  &createInfo.cryptHandle,
							  CRYPT_IATTRIBUTE_RTCSREQUEST );
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create RTCS response from request" ) );
		}
	return( CRYPT_OK );
	}

/* Return a response to an RTCS client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sendServerResponse( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT RTCS_PROTOCOL_INFO *protocolInfo )
	{
	CRYPT_CERTIFICATE iCmsAttributes = CRYPT_UNUSED;
	MESSAGE_DATA msgData;
	ERROR_INFO errorInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( RTCS_PROTOCOL_INFO ) ) );

	/* Check the entries from the request against the certificate store and 
	   sign the resulting status information ("Love, ken").  Note that
	   CRYPT_ERROR_INVALID is a valid return status for the sigcheck call
	   since it indicates that one (or more) of the certificates are 
	   invalid */
	status = krnlSendMessage( sessionInfoPtr->iCertResponse,
							  IMESSAGE_CRT_SIGCHECK, NULL,
							  sessionInfoPtr->cryptKeyset );
	if( cryptStatusError( status ) && status != CRYPT_ERROR_INVALID )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't check RTCS request against certificate "
				  "store" ) );
		}

	/* If there's a nonce present, create CMS attributes to contain it */
	if( protocolInfo->nonceSize > 0 )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		setMessageCreateObjectInfo( &createInfo,
									CRYPT_CERTTYPE_CMS_ATTRIBUTES );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT,
								  &createInfo, OBJECT_TYPE_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( status );
		iCmsAttributes = createInfo.cryptHandle;
		setMessageData( &msgData, protocolInfo->nonce,
						protocolInfo->nonceSize );
		status = krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_CMS_NONCE );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}

	/* Extract the response data */
	setMessageData( &msgData, sessionInfoPtr->receiveBuffer,
					sessionInfoPtr->receiveBufSize );
	status = krnlSendMessage( sessionInfoPtr->iCertResponse,
							  IMESSAGE_CRT_EXPORT, &msgData,
							  CRYPT_ICERTFORMAT_DATA );
	if( cryptStatusError( status ) )
		{
		if( iCmsAttributes != CRYPT_UNUSED )
			krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't encode RTCS response" ) );
		}

	/* Sign the response data using the responder's key and send it to the
	   client */
	status = envelopeSign( sessionInfoPtr->receiveBuffer, msgData.length,
						   sessionInfoPtr->receiveBuffer,
						   sessionInfoPtr->receiveBufSize,
						   &sessionInfoPtr->receiveBufEnd,
						   CRYPT_CONTENT_RTCSRESPONSE,
						   sessionInfoPtr->privateKey, iCmsAttributes, 
						   &errorInfo );
	if( iCmsAttributes != CRYPT_UNUSED )
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		retExtErr( status,
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Couldn't CMS-envelope RTCS response: " ) );
		}
	DEBUG_DUMP_FILE( "rtcs_sresp", sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufEnd );
	return( writePkiDatagram( sessionInfoPtr, RTCS_CONTENT_TYPE_RESP,
							  RTCS_CONTENT_TYPE_RESP_LEN ) );
	}

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

/* Exchange data with an RTCS client/server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int clientTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Get certificate status information from the server */
	status = sendClientRequest( sessionInfoPtr );
	if( cryptStatusOK( status ) )
		status = readServerResponse( sessionInfoPtr );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int serverTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	RTCS_PROTOCOL_INFO protocolInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Send certificate status information to the client */
	memset( &protocolInfo, 0, sizeof( RTCS_PROTOCOL_INFO ) );
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
static int setAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 IN const void *data,
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	const CRYPT_CERTIFICATE rtcsRequest = *( ( CRYPT_CERTIFICATE * ) data );
	MESSAGE_DATA msgData = { NULL, 0 };
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, sizeof( int ) ) );

	REQUIRES( type == CRYPT_SESSINFO_REQUEST );

	/* Make sure that everything is set up ready to go.  Since RTCS requests
	   aren't signed like normal certificate objects we can't just check the 
	   immutable attribute but have to perform a dummy export for which the
	   certificate export code will return an error status if there's a 
	   problem with the request.  If not, it pseudo-signs the request (if it 
	   hasn't already done so) and prepares it for use */
	status = krnlSendMessage( rtcsRequest, IMESSAGE_CRT_EXPORT, &msgData,
							  CRYPT_ICERTFORMAT_DATA );
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_NUM1 );

	/* If we haven't already got a server name explicitly set, try and get
	   it from the request.  This is an opportunistic action so we ignore 
	   any potential error, the caller can still set the value explicitly */
	if( findSessionInfo( sessionInfoPtr->attributeList,
						 CRYPT_SESSINFO_SERVER_NAME ) == NULL )
		{
		char buffer[ MAX_URL_SIZE + 8 ];

		setMessageData( &msgData, buffer, MAX_URL_SIZE );
		status = krnlSendMessage( rtcsRequest, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_RESPONDERURL );
		if( cryptStatusOK( status ) )
			( void ) krnlSendMessage( sessionInfoPtr->objectHandle,
									  IMESSAGE_SETATTRIBUTE_S, &msgData,
									  CRYPT_SESSINFO_SERVER_NAME );
		}

	/* Add the request and increment its usage count */
	krnlSendNotifier( rtcsRequest, IMESSAGE_INCREFCOUNT );
	sessionInfoPtr->iCertRequest = rtcsRequest;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

/* Open/close an RTCS session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodRTCS( INOUT SESSION_INFO *sessionInfoPtr )
	{
	static const PROTOCOL_INFO protocolInfo = {
		/* General session information */
		TRUE,						/* Request-response protocol */
		SESSION_ISHTTPTRANSPORT,	/* Flags */
		80,							/* HTTP port */
		SESSION_NEEDS_REQUEST,		/* Client flags */
		SESSION_NEEDS_PRIVATEKEY |	/* Server flags */
			SESSION_NEEDS_PRIVKEYSIGN | \
			SESSION_NEEDS_PRIVKEYCERT | \
			SESSION_NEEDS_KEYSET,
		1, 1, 1						/* Version 1 */

		/* Protocol-specific information */
		};

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Set the access method pointers */
	sessionInfoPtr->protocolInfo = &protocolInfo;
	if( isServer( sessionInfoPtr ) )
		sessionInfoPtr->transactFunction = serverTransact;
	else
		sessionInfoPtr->transactFunction = clientTransact;
	sessionInfoPtr->setAttributeFunction = setAttributeFunction;

	return( CRYPT_OK );
	}
#endif /* USE_RTCS */
