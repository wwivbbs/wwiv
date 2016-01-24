/****************************************************************************
*																			*
*						 cryptlib OCSP Session Management					*
*						Copyright Peter Gutmann 1999-2011					*
*																			*
****************************************************************************/

#include <stdio.h>
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

#ifdef USE_OCSP

/* OCSP HTTP content types */

#define OCSP_CONTENT_TYPE_REQ		"application/ocsp-request"
#define OCSP_CONTENT_TYPE_REQ_LEN	24
#define OCSP_CONTENT_TYPE_RESP		"application/ocsp-response"
#define OCSP_CONTENT_TYPE_RESP_LEN	25

/* OCSP query/response types */

typedef enum {
	OCSPRESPONSE_TYPE_NONE,				/* No response type */
	OCSPRESPONSE_TYPE_OCSP,				/* OCSP standard response */
	OCSPRESPONSE_TYPE_LAST				/* Last valid response type */
	} OCSPRESPONSE_TYPE;

/* OCSP response status values */

enum { OCSP_RESP_SUCCESSFUL, OCSP_RESP_MALFORMEDREQUEST,
	   OCSP_RESP_INTERNALERROR, OCSP_RESP_TRYLATER, OCSP_RESP_DUMMY,
	   OCSP_RESP_SIGREQUIRED, OCSP_RESP_UNAUTHORISED };

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Deliver an Einladung betreff Kehrseite to the client.  We don't bother
   checking the return value since there's nothing that we can do in the
   case of an error except close the connection, which we do anyway since
   this is the last message */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void sendErrorResponse( INOUT SESSION_INFO *sessionInfoPtr,
							   IN_BUFFER( responseDataLength ) \
									const void *responseData,
							   IN_LENGTH_SHORT const int responseDataLength )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( responseData, responseDataLength ) );

	REQUIRES_V( responseDataLength > 0 && \
				responseDataLength < MAX_INTLENGTH_SHORT );

	/* Since we're already in an error state there's not much that we can do
	   in terms of alerting the user if a further error occurs when writing 
	   the error response, so we ignore any potential write errors that occur
	   at this point */
	memcpy( sessionInfoPtr->receiveBuffer, responseData,
			responseDataLength );
	sessionInfoPtr->receiveBufEnd = responseDataLength;
	( void ) writePkiDatagram( sessionInfoPtr, OCSP_CONTENT_TYPE_RESP,
							   OCSP_CONTENT_TYPE_RESP_LEN );
	}

/* Compare the nonce in a request with the returned nonce in the response */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int checkNonce( IN_HANDLE const CRYPT_CERTIFICATE iCertResponse,
					   IN_BUFFER( requestNonceLength ) const void *requestNonce, 
					   IN_LENGTH_SHORT const int requestNonceLength )
	{
	MESSAGE_DATA responseMsgData;
	BYTE responseNonceBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int status;

	assert( isReadPtr( requestNonce, requestNonceLength ) );

	REQUIRES( isHandleRangeValid( iCertResponse ) );
	REQUIRES( requestNonceLength > 0 && \
			  requestNonceLength < MAX_INTLENGTH_SHORT );

	/* Make sure that the nonce has a plausible length */
	if( requestNonceLength < 4 || requestNonceLength > CRYPT_MAX_HASHSIZE )
		return( CRYPT_ERROR_BADDATA );

	/* Try and read the nonce from the response */
	setMessageData( &responseMsgData, responseNonceBuffer,
					CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCertResponse, IMESSAGE_GETATTRIBUTE_S,
							  &responseMsgData, CRYPT_CERTINFO_OCSP_NONCE );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_NOTFOUND );

	/* Make sure that the two nonces match.  The comparison is in theory 
	   somewhat complex because OCSP never specifies how the nonce is meant 
	   to be encoded so it's possible that some implementations will use 
	   things like TSP's bizarre INTEGER rather than the obvious and logical 
	   OCTET STRING.  In theory this means that we might need to check for 
	   the INTEGER-encoding alternatives that arise due to sign bits, but 
	   this doesn't seem to be required in practice since everyone use a de 
	   facto encoding of OCTET STRING */
	if( requestNonceLength != responseMsgData.length || \
		memcmp( requestNonce, responseMsgData.data, requestNonceLength ) )
		return( CRYPT_ERROR_SIGNATURE );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Client-side Functions							*
*																			*
****************************************************************************/

/* OID information used to read responses */

static const OID_INFO FAR_BSS ocspOIDinfo[] = {
	{ OID_OCSP_RESPONSE_OCSP, OCSPRESPONSE_TYPE_OCSP },
	{ NULL, 0 }, { NULL, 0 }
	};

/* Send a request to an OCSP server */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendClientRequest( INOUT SESSION_INFO *sessionInfoPtr )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Get the encoded request data.  We store this in the session buffer, 
	   which at its minimum size is roughly two orders of magnitude larger 
	   than the request */
	setMessageData( &msgData, sessionInfoPtr->receiveBuffer,
					sessionInfoPtr->receiveBufSize );
	status = krnlSendMessage( sessionInfoPtr->iCertRequest,
							  IMESSAGE_CRT_EXPORT, &msgData,
							  CRYPT_ICERTFORMAT_DATA );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't get OCSP request data from OCSP request "
				  "object" ) );
		}
	sessionInfoPtr->receiveBufEnd = msgData.length;
	DEBUG_DUMP_FILE( "ocsp_req", sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufEnd );

	/* Send the request to the responder */
	return( writePkiDatagram( sessionInfoPtr, OCSP_CONTENT_TYPE_REQ,
							  OCSP_CONTENT_TYPE_REQ_LEN ) );
	}

/* Read the response from the OCSP server */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static int readServerResponse( INOUT SESSION_INFO *sessionInfoPtr )
	{
	CRYPT_CERTIFICATE iCertResponse;
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE nonceBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	const char *errorString = NULL;
	int errorCode, responseType, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Read the response from the responder */
	status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	DEBUG_DUMP_FILE( "ocsp_resp", sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufEnd );

	/* Try and extract an OCSP status code from the returned object:

		SEQUENCE {
			respStatus			ENUMERATED,			-- 0 = OK
			respBytes		[0]	EXPLICIT SEQUENCE {
								... */
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer,
				 sessionInfoPtr->receiveBufEnd );
	readSequence( &stream, NULL );
	status = readEnumerated( &stream, &errorCode );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Invalid OCSP response status data" ) );
		}

	/* If it's an error status, try and translate it into something a bit 
	   more meaningful.  Some of the translations are a bit questionable, 
	   but it's better than the generic no va response (which should 
	   actually be "no marcha" in any case) */
	switch( errorCode )
		{
		case OCSP_RESP_SUCCESSFUL:
			status = CRYPT_OK;
			break;

		case OCSP_RESP_TRYLATER:
			errorString = "Try again later";
			status = CRYPT_ERROR_NOTAVAIL;
			break;

		case OCSP_RESP_SIGREQUIRED:
			errorString = "Signed OCSP request required";
			status = CRYPT_ERROR_SIGNATURE;
			break;

		case OCSP_RESP_UNAUTHORISED:
			errorString = "Client isn't authorised to perform query";
			status = CRYPT_ERROR_PERMISSION;
			break;

		default:
			errorString = "Unknown error";
			status = CRYPT_ERROR_INVALID;
			break;
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		retExt( status,
				( status, SESSION_ERRINFO, 
				   "OCSP server returned status %d: %s",
				   errorCode, errorString ) );
		}

	/* We've got a valid response, read the [0] EXPLICIT SEQUENCE { OID,
	   OCTET STRING { encapsulation and import the response into an OCSP
	   certificate object */
	readConstructed( &stream, NULL, 0 );		/* responseBytes */
	readSequence( &stream, NULL );
	readOID( &stream, ocspOIDinfo,				/* responseType */
			 FAILSAFE_ARRAYSIZE( ocspOIDinfo, OID_INFO ), &responseType );
	status = readGenericHole( &stream, &length, 16, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid OCSP response data header" ) );
		}
	status = importCertFromStream( &stream, &iCertResponse, 
								   DEFAULTUSER_OBJECT_HANDLE,
								   CRYPT_CERTTYPE_OCSP_RESPONSE, length );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid OCSP response data" ) );

	/* If the request went out with a nonce included (which it does by
	   default), make sure that it matches the nonce in the response */
	setMessageData( &msgData, nonceBuffer, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( sessionInfoPtr->iCertRequest,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_CERTINFO_OCSP_NONCE );
	if( cryptStatusOK( status ) )
		{
		/* There's a nonce in the request, check that it matches the one in
		   the response */
		status = checkNonce( iCertResponse, msgData.data, msgData.length );
		if( cryptStatusError( status ) )
			{
			/* The response doesn't contain a nonce or it doesn't match what 
			   we sent, we can't trust it.  The best error that we can return 
			   here is a signature error to indicate that the integrity 
			   check failed.
			   
			   Note that a later modification to OCSP, in an attempt to make 
			   it scale, removed the nonce, thus breaking the security of 
			   the protocol against replay attack.  Since the protocol is 
			   now broken against attack we treat a nonce-less response from 
			   one of these responders as a failure, since it's 
			   indistinguishable from an actual attack */
			krnlSendNotifier( iCertResponse, IMESSAGE_DECREFCOUNT );
			retExt( CRYPT_ERROR_SIGNATURE,
					( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
					  ( status == CRYPT_ERROR_NOTFOUND ) ? \
					  "OCSP response doesn't contain a nonce" : \
					  "OCSP response nonce doesn't match the one in the "
					  "request" ) );
			}
		}
	krnlSendNotifier( sessionInfoPtr->iCertRequest, IMESSAGE_DECREFCOUNT );
	sessionInfoPtr->iCertRequest = CRYPT_ERROR;
	sessionInfoPtr->iCertResponse = iCertResponse;

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

#define RESPONSE_SIZE		5

static const BYTE FAR_BSS respBadRequest[] = {
	0x30, 0x03, 0x0A, 0x01, 0x01	/* Rejection, malformed request */
	};
static const BYTE FAR_BSS respIntError[] = {
	0x30, 0x03, 0x0A, 0x01, 0x02	/* Rejection, internal error */
	};

/* Read a request from an OCSP client */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static int readClientRequest( INOUT SESSION_INFO *sessionInfoPtr )
	{
	CRYPT_CERTIFICATE iOcspRequest;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Read the request data from the client.  We don't write an error
	   response at this initial stage to prevent scanning/DOS attacks
	   (vir sapit qui pauca loquitur) */
	status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	DEBUG_DUMP_FILE( "ocsp_sreq", sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufEnd );

	/* Basic lint filter to check for approximately-OK requests before we
	   try creating a certificate object from the data:

		SEQUENCE {
			SEQUENCE {					-- tbsRequest
				version		[0]	...
				reqName		[1]	...
				SEQUENCE {				-- requestList
					SEQUENCE {			-- request
					... */
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer,
				 sessionInfoPtr->receiveBufEnd );
	readSequence( &stream, NULL );
	readSequence( &stream, NULL );
	if( peekTag( &stream ) == MAKE_CTAG( 0 ) )
		readUniversal( &stream );
	if( peekTag( &stream ) == MAKE_CTAG( 1 ) )
		readUniversal( &stream );
	readSequence( &stream, NULL );
	status = readSequence( &stream, NULL );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid OCSP request header" ) );
		}

	/* Import the request as a cryptlib object */
	setMessageCreateObjectIndirectInfo( &createInfo,
										sessionInfoPtr->receiveBuffer,
										sessionInfoPtr->receiveBufEnd,
										CRYPT_CERTTYPE_OCSP_REQUEST );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		sendErrorResponse( sessionInfoPtr, respBadRequest, RESPONSE_SIZE );
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid OCSP request data" ) );
		}
	iOcspRequest = createInfo.cryptHandle;

	/* Create an OCSP response and add the request information to it */
	setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_OCSP_RESPONSE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iOcspRequest, IMESSAGE_DECREFCOUNT );
		sendErrorResponse( sessionInfoPtr, respIntError, RESPONSE_SIZE );
		return( status );
		}
	status = krnlSendMessage( createInfo.cryptHandle,
							  IMESSAGE_SETATTRIBUTE, &iOcspRequest,
							  CRYPT_IATTRIBUTE_OCSPREQUEST );
	krnlSendNotifier( iOcspRequest, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		sendErrorResponse( sessionInfoPtr, respIntError, RESPONSE_SIZE );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create OCSP response from request" ) );
		}
	sessionInfoPtr->iCertResponse = createInfo.cryptHandle;
	return( CRYPT_OK );
	}

/* Return a response to an OCSP client */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendServerResponse( INOUT SESSION_INFO *sessionInfoPtr )
	{
	MESSAGE_DATA msgData;
	STREAM stream;
	int responseLength, responseDataLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Check the entries from the request against the certificate store and 
	   sign the resulting status information ("Love, ken").  Note that
	   CRYPT_ERROR_INVALID is a valid return status for the sigcheck call
	   since it indicates that one (or more) of the certificates was 
	   revoked */
	status = krnlSendMessage( sessionInfoPtr->iCertResponse,
							  IMESSAGE_CRT_SIGCHECK, NULL,
							  sessionInfoPtr->cryptKeyset );
	if( cryptStatusError( status ) && status != CRYPT_ERROR_INVALID )
		{
		sendErrorResponse( sessionInfoPtr, respIntError, RESPONSE_SIZE );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't check OCSP request against certificate "
				  "store" ) );
		}
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( sessionInfoPtr->iCertResponse,
							  IMESSAGE_CRT_SIGN, NULL,
							  sessionInfoPtr->privateKey );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( sessionInfoPtr->iCertResponse,
								  IMESSAGE_CRT_EXPORT, &msgData,
								  CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		sendErrorResponse( sessionInfoPtr, respIntError, RESPONSE_SIZE );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create signed OCSP response" ) );
		}
	responseDataLength = msgData.length;

	/* Write the wrapper for the response */
	sMemOpen( &stream, sessionInfoPtr->receiveBuffer,
			  sessionInfoPtr->receiveBufSize );
	responseLength = sizeofOID( OID_OCSP_RESPONSE_OCSP ) + \
					 sizeofObject( responseDataLength );
	writeSequence( &stream, sizeofEnumerated( 0 ) + \
				   sizeofObject( sizeofObject( responseLength ) ) );
	writeEnumerated( &stream, 0, DEFAULT_TAG );		/* respStatus */
	writeConstructed( &stream, sizeofObject( responseLength ), 0 );
	writeSequence( &stream, responseLength );		/* respBytes */
	writeOID( &stream, OID_OCSP_RESPONSE_OCSP );	/* respType */
	status = writeOctetStringHole( &stream, responseDataLength, 
								   DEFAULT_TAG );	/* response */
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	/* Get the encoded response data */
	status = exportCertToStream( &stream, sessionInfoPtr->iCertResponse,
								 CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusOK( status ) )
		sessionInfoPtr->receiveBufEnd = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		sendErrorResponse( sessionInfoPtr, respIntError, RESPONSE_SIZE );
		return( status );
		}
	DEBUG_DUMP_FILE( "ocsp_sresp", sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufEnd );

	/* Send the response to the client */
	return( writePkiDatagram( sessionInfoPtr, OCSP_CONTENT_TYPE_RESP,
							  OCSP_CONTENT_TYPE_RESP_LEN ) );
	}

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

/* Exchange data with an OCSP client/server */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static int clientTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Get certificate revocation information from the server */
	status = sendClientRequest( sessionInfoPtr );
	if( cryptStatusOK( status ) )
		status = readServerResponse( sessionInfoPtr );
	return( status );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static int serverTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Send certificate revocation information to the client */
	status = readClientRequest( sessionInfoPtr );
	if( cryptStatusOK( status ) )
		status = sendServerResponse( sessionInfoPtr );
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
	const CRYPT_CERTIFICATE ocspRequest = *( ( CRYPT_CERTIFICATE * ) data );
	MESSAGE_DATA msgData = { NULL, 0 };
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, sizeof( int ) ) );

	REQUIRES( type == CRYPT_SESSINFO_REQUEST );

	/* Make sure that everything is set up ready to go.  Since OCSP requests
	   aren't (usually) signed like normal certificate objects we can't just 
	   check the immutable attribute but have to perform a dummy export for 
	   which the certificate export code will return an error status if 
	   there's a problem with the request.  If not, it pseudo-signs the 
	   request (if it hasn't already done so) and prepares it for use */
	status = krnlSendMessage( ocspRequest, IMESSAGE_CRT_EXPORT, &msgData,
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
		status = krnlSendMessage( ocspRequest, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_RESPONDERURL );
		if( cryptStatusOK( status ) )
			( void ) krnlSendMessage( sessionInfoPtr->objectHandle,
									  IMESSAGE_SETATTRIBUTE_S, &msgData,
									  CRYPT_SESSINFO_SERVER_NAME );
		}

	/* Add the request and increment its usage count */
	krnlSendNotifier( ocspRequest, IMESSAGE_INCREFCOUNT );
	sessionInfoPtr->iCertRequest = ocspRequest;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodOCSP( INOUT SESSION_INFO *sessionInfoPtr )
	{
	static const PROTOCOL_INFO protocolInfo = {
		/* General session information */
		TRUE,						/* Request-response protocol */
		SESSION_ISHTTPTRANSPORT,	/* Flags */
		80,							/* HTTP port */
		SESSION_NEEDS_REQUEST,		/* Client attributes */
		SESSION_NEEDS_PRIVATEKEY |	/* Server attributes */
			SESSION_NEEDS_PRIVKEYSIGN | \
			SESSION_NEEDS_PRIVKEYCERT | \
			SESSION_NEEDS_KEYSET,
		1, 1, 2						/* Version 1 */

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
#endif /* USE_OCSP */
