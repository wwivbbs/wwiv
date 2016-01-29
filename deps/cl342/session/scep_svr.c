/****************************************************************************
*																			*
*						 cryptlib SCEP Server Management					*
*						Copyright Peter Gutmann 1999-2013					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "session.h"
  #include "certstore.h"
  #include "scep.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "session/session.h"
  #include "session/certstore.h"
  #include "session/scep.h"
#endif /* Compiler-specific includes */

#ifdef USE_SCEP

/* Table mapping a query submitted as an HTTP GET to a supplementary SCEP
   operation.  Note that the first letter must be lowercase for the
   case-insensitive quick match */

enum { SCEP_OPERATION_GETCACAPS, SCEP_OPERATION_GETCACERT, 
	   SCEP_OPERATION_GETCACERTCHAIN };

static const CERTSTORE_READ_INFO certstoreReadInfo[] = {
	{ "getCACaps", 9, SCEP_OPERATION_GETCACAPS, CERTSTORE_FLAG_NONE },
	{ "getCACert", 9, SCEP_OPERATION_GETCACERT, CERTSTORE_FLAG_NONE },
	{ "getCACertChain", 14, SCEP_OPERATION_GETCACERTCHAIN, CERTSTORE_FLAG_NONE },
	{ NULL, 0, CRYPT_ERROR, CERTSTORE_FLAG_NONE },
		{ NULL, 0, CRYPT_ERROR, CERTSTORE_FLAG_NONE }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Deliver an Einladung betreff Kehrseite to the client.  We don't bother
   checking the return value since there's nothing that we can do in the case 
   of an error except close the connection, which we do anyway since this is 
   the last message.  We don't return extended error information since this 
   would overwrite the information for the error that caused us to return an 
   error response */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void sendErrorResponse( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT SCEP_PROTOCOL_INFO *protocolInfo,
							   IN_ERROR const int scepStatus )
	{
	CRYPT_CERTIFICATE iCmsAttributes;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );
	
	REQUIRES_V( cryptStatusError( scepStatus ) );

	/* Sign the error response using the CA key and SCEP attributes */
	status = createScepAttributes( sessionInfoPtr, protocolInfo,  
								   &iCmsAttributes, MESSAGETYPE_CERTREP, 
								   scepStatus );
	if( cryptStatusOK( status ) )
		{
		ERROR_INFO errorInfo;

		/* Since this message is being sent in response to an existing 
		   error, we don't care about the possible error information 
		   returned from the function that sends the error response,
		   so the errorInfo result is ignored */
		status = envelopeSign( NULL, 0, sessionInfoPtr->receiveBuffer, 
							   sessionInfoPtr->receiveBufSize, 
							   &sessionInfoPtr->receiveBufEnd, 
							   CRYPT_CONTENT_NONE, sessionInfoPtr->privateKey, 
							   iCmsAttributes, &errorInfo );
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		}
	if( cryptStatusError( status ) )
		{
		HTTP_DATA_INFO httpDataInfo;

		/* If we encounter an error processing the initial request then 
		   there won't be enough information available to create an error 
		   response.  Similarly if we run into problems generating the
		   response then there won't be anything available to send.  At this 
		   point the best that we can do is send an error at the HTTP 
		   level */
		initHttpDataInfo( &httpDataInfo, sessionInfoPtr->receiveBuffer,
						  sessionInfoPtr->receiveBufSize );
		httpDataInfo.reqStatus = scepStatus;
		swrite( &sessionInfoPtr->stream, &httpDataInfo, 
				sizeof( HTTP_DATA_INFO ) );
		return;
		}
	DEBUG_DUMP_FILE( "scep_srespx", sessionInfoPtr->receiveBuffer, 
					 sessionInfoPtr->receiveBufEnd );

	/* Return the response to the client, discarding any error indication 
	   from the write.  Since we're already in an error state there's not 
	   much that we can do in terms of alerting the user if a further error 
	   occurs when writing the error response, so we ignore any potential 
	   write errors that occur at this point */
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_LASTMESSAGE, TRUE );
	( void ) writePkiDatagram( sessionInfoPtr, SCEP_CONTENT_TYPE, 
							   SCEP_CONTENT_TYPE_LEN );
	}

/* Check that the information supplied in a request matches what's stored for
   a PKI user */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkPkiUserInfo( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const ATTRIBUTE_LIST *userNamePtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_USERNAME );
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	MESSAGE_DATA msgData;
	BYTE keyID[ 64 + 8 ];
	BYTE requestPassword[ CRYPT_MAX_TEXTSIZE + 8 ];
	BYTE userPassword[ CRYPT_MAX_TEXTSIZE + 8 ];
	int requestPasswordSize, userPasswordSize DUMMY_INIT;
	int keyIDsize, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( userNamePtr != NULL );
	
	/* Get the password from the PKCS #10 request */
	setMessageData( &msgData, requestPassword, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( sessionInfoPtr->iCertRequest, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData, 
							  CRYPT_CERTINFO_CHALLENGEPASSWORD );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't get challenge password from PKCS #10 request" ) );
		}
	requestPasswordSize = msgData.length;

	/* Since it's a cryptlib encoded user ID we need to decode it before we 
	   can look up a PKI user with it */
	REQUIRES( userNamePtr->flags & ATTR_FLAG_ENCODEDVALUE );
	status = decodePKIUserValue( keyID, 64, &keyIDsize,
								 userNamePtr->value, 
								 userNamePtr->valueLength );
	ENSURES( cryptStatusOK( status ) );

	/* Get the user information for the request from the certificate 
	   store */
	setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_KEYID, keyID, 
						   keyIDsize, NULL, 0, KEYMGMT_FLAG_NONE );
	status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_KEY_GETKEY, &getkeyInfo, 
							  KEYMGMT_ITEM_PKIUSER );
	if( cryptStatusError( status ) )
		{
		char userID[ CRYPT_MAX_TEXTSIZE + 8 ];
		int userIDlen;

		zeroise( requestPassword, CRYPT_MAX_TEXTSIZE );
		userIDlen = min( userNamePtr->valueLength, CRYPT_MAX_TEXTSIZE );
		memcpy( userID, userNamePtr->value, userIDlen );
		retExtObj( status, 
				   ( status, SESSION_ERRINFO, sessionInfoPtr->cryptKeyset,
					 "Couldn't find PKI user information for %s",
					 sanitiseString( userID, CRYPT_MAX_TEXTSIZE, 
									 userIDlen ) ) );
		}

	/* Get the password from the PKI user object */
	setMessageData( &msgData, userPassword, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( getkeyInfo.cryptHandle, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD );
	if( cryptStatusOK( status ) )
		{
		userPasswordSize = msgData.length;
		status = updateSessionInfo( &sessionInfoPtr->attributeList, 
									CRYPT_SESSINFO_PASSWORD, 
									userPassword, userPasswordSize, 
									CRYPT_MAX_TEXTSIZE, 
									ATTR_FLAG_ENCODEDVALUE );
		}
	if( cryptStatusError( status ) )
		{
		zeroise( requestPassword, CRYPT_MAX_TEXTSIZE );
		zeroise( userPassword, CRYPT_MAX_TEXTSIZE );
		krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Couldn't copy read PKI user data from PKI user object "
				  "into session object" ) );
		}

	/* Make sure that the password matches the one in the request */
	if( userPasswordSize != requestPasswordSize || \
		!compareDataConstTime( userPassword, requestPassword, 
							   userPasswordSize ) )
		{
		zeroise( requestPassword, CRYPT_MAX_TEXTSIZE );
		zeroise( userPassword, CRYPT_MAX_TEXTSIZE );
		krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		retExt( CRYPT_ERROR_WRONGKEY, 
				( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO,
				  "Password in PKCS #10 request doesn't match PKI user "
				  "password" ) );
		}
	zeroise( userPassword, CRYPT_MAX_TEXTSIZE );

	/* If the subject only knows their CN, they may send a CN-only subject DN 
	   in the hope that we can fill it in for them.  In addition there may be 
	   other constraints that the CA wants to apply, these are handled by
	   applying the PKI user information to the request */
	status = krnlSendMessage( sessionInfoPtr->iCertRequest,
							  IMESSAGE_SETATTRIBUTE, &getkeyInfo.cryptHandle,
							  CRYPT_IATTRIBUTE_PKIUSERINFO );
	krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_INVALID, 
				( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
				  "User information in PKCS #10 request can't be "
				  "reconciled with stored information for the user" ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Additional Request Management Functions					*
*																			*
****************************************************************************/

/* Process one of the bolted-on additions to the basic SCEP protocol */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processAdditionalScepRequest( INOUT SESSION_INFO *sessionInfoPtr,
										 const HTTP_URI_INFO *httpReqInfo )
	{
	HTTP_URI_INFO rewrittenHttpReqInfo;
	MESSAGE_DATA msgData;
	int operationType, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( httpReqInfo, sizeof( HTTP_URI_INFO ) ) );

	/* If the client has fed us an HTTP GET request, find out what they  
	   want.  SCEP's handling of HTTP requests is a bit different from the 
	   "attribute '=' value" lookup that's normally used for HTTP data 
	   retrieval.  Instead, it uses the format 
	   "'operation =' value '&' extraData", with the search key buried in 
	   the 'extraData' value.  In addition the content of the 'extraData' 
	   value isn't defined outside of "any string which is understood by the 
	   CA".  However since 'value' defines what we want, we can determine 
	   what to return based on this and ignore the 'extraData' portion.

	   In order to fix up the query information into a format that works 
	   with standard HTTP queries, we rewrite the query data from the 
	   "'operation =' value '&' extraData" form into "attribute '=' value" 
	   before we process the query */
	memset( &rewrittenHttpReqInfo, 0, sizeof( HTTP_URI_INFO ) );
	memcpy( rewrittenHttpReqInfo.attribute, httpReqInfo->value, 
			httpReqInfo->valueLen );
	rewrittenHttpReqInfo.attributeLen = httpReqInfo->valueLen;
	if( httpReqInfo->extraDataLen > 0 )
		{
		memcpy( rewrittenHttpReqInfo.value, httpReqInfo->extraData, 
				httpReqInfo->extraDataLen );
		rewrittenHttpReqInfo.valueLen = httpReqInfo->extraDataLen;
		}
	status = processCertQuery( sessionInfoPtr, &rewrittenHttpReqInfo,
							   certstoreReadInfo, 
							   FAILSAFE_ARRAYSIZE( certstoreReadInfo, \
												   CERTSTORE_READ_INFO ),
							   &operationType, NULL, 0, NULL );
	if( cryptStatusError( status ) )
		{
		sendCertErrorResponse( sessionInfoPtr, status );
		return( status );
		}
	ENSURES( operationType == SCEP_OPERATION_GETCACAPS || \
			 operationType == SCEP_OPERATION_GETCACERT || \
			 operationType == SCEP_OPERATION_GETCACERTCHAIN );

	/* If it's a CA capabilities query, return the information as raw text
	   over HTTP */
	if( operationType == SCEP_OPERATION_GETCACAPS )
		{
		STREAM stream;

		sMemOpen( &stream, sessionInfoPtr->receiveBuffer, 1024 );
		swrite( &stream, "POSTPKIOperation\n", 17 );
#if 0	/* 14/6/14 Too risky to implement given its current state in the 
				   spec, see the discussion on the JSCEP mailing list for
				   details */
		status = swrite( &stream, "Renewal\n", 8 );
#endif /* 0 */
		if( algoAvailable( CRYPT_ALGO_SHA1 ) )
			status = swrite( &stream, "SHA-1\n", 6 );
		if( algoAvailable( CRYPT_ALGO_SHA2 ) )
			status = swrite( &stream, "SHA-256\n", 8 );
		if( algoAvailable( CRYPT_ALGO_SHAng ) )
			status = swrite( &stream, "SHAng\n", 6 );
		if( algoAvailable( CRYPT_ALGO_3DES ) )
			status = swrite( &stream, "DES3\n", 5 );
		if( algoAvailable( CRYPT_ALGO_AES ) )
			status = swrite( &stream, "AES\n", 4 );
		if( cryptStatusOK( status ) )
			sessionInfoPtr->receiveBufEnd = stell( &stream );
		sMemDisconnect( &stream );
		ENSURES( cryptStatusOK( status ) );
		return( writePkiDatagram( sessionInfoPtr, SCEP_CONTENT_TYPE, 
								  SCEP_CONTENT_TYPE_LEN ) );
		}

	/* Export the CA certificate, either as a standalone certificate or a 
	   full certificate chain depending on what was requested, and send it 
	   to the client */
	setMessageData( &msgData, sessionInfoPtr->receiveBuffer,
					sessionInfoPtr->receiveBufSize );
	status = krnlSendMessage( sessionInfoPtr->privateKey,
							  IMESSAGE_CRT_EXPORT, &msgData,
							  ( operationType == SCEP_OPERATION_GETCACERT ) ? \
								CRYPT_CERTFORMAT_CERTIFICATE : \
								CRYPT_CERTFORMAT_CERTCHAIN );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't export CA certificate%s for '%s' request", 
				  ( operationType == SCEP_OPERATION_GETCACERT ) ? \
					"" : " chain",
				  ( operationType == SCEP_OPERATION_GETCACERT ) ? \
					"GetCACert" : "GetCACertChain" ) );
		}
	sessionInfoPtr->receiveBufEnd = msgData.length;
	if( operationType == SCEP_OPERATION_GETCACERT )
		{
		return( writePkiDatagram( sessionInfoPtr, 
								  SCEP_CONTENT_TYPE_GETCACERT,
								  SCEP_CONTENT_TYPE_GETCACERT_LEN ) );
		}
	return( writePkiDatagram( sessionInfoPtr,  
							  SCEP_CONTENT_TYPE_GETCACERTCHAIN,
							  SCEP_CONTENT_TYPE_GETCACERTCHAIN_LEN ) );
	}

/****************************************************************************
*																			*
*						Request Management Functions						*
*																			*
****************************************************************************/

/* Check a SCEP request message */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkScepRequest( INOUT SESSION_INFO *sessionInfoPtr,
							 INOUT SCEP_PROTOCOL_INFO *protocolInfo, 
							 OUT_ALWAYS BOOLEAN *requestDataAvailable )
	{
	SCEP_INFO *scepInfo = sessionInfoPtr->sessionSCEP;
	CRYPT_CERTIFICATE iCmsAttributes;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	ERROR_INFO errorInfo;
	BYTE keyIDbuffer[ KEYID_SIZE + 8 ];
	int dataLength, sigResult, value, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );
	assert( isWritePtr( requestDataAvailable, sizeof( BOOLEAN ) ) );

	/* Clear return value */
	*requestDataAvailable = FALSE;

	/* Phase 1: Sig-check the self-signed data */
	DEBUG_DUMP_FILE( "scep_sreq2", sessionInfoPtr->receiveBuffer, 
					 sessionInfoPtr->receiveBufEnd );
	status = envelopeSigCheck( sessionInfoPtr->receiveBuffer, 
							   sessionInfoPtr->receiveBufEnd,
							   sessionInfoPtr->receiveBuffer, 
							   sessionInfoPtr->receiveBufSize, &dataLength, 
							   CRYPT_UNUSED, &sigResult, 
							   &protocolInfo->iScepCert, &iCmsAttributes,
							   &errorInfo );
	if( cryptStatusError( status ) )
		{
		retExtErr( status, 
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Invalid CMS signed data in client request: " ) );
		}
	DEBUG_DUMP_FILE( "scep_sreq1", sessionInfoPtr->receiveBuffer, 
					 dataLength );
	if( cryptStatusError( sigResult ) )
		{
		/* The signed data was valid but the signature on it wasn't, this is
		   a different style of error than the previous one */
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		retExt( sigResult, 
				( sigResult, SESSION_ERRINFO, 
				  "Bad signature on client request data" ) );
		}

	/* Make sure that the client certificate is valid for signing and 
	   decryption.  In effect the signing capability has already been 
	   checked by the fact that the certificate signed the request but we do 
	   an explicit check here just to be thorough */
	status = krnlSendMessage( protocolInfo->iScepCert, IMESSAGE_CHECK, 
							  NULL, MESSAGE_CHECK_PKC_SIGCHECK );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( protocolInfo->iScepCert, IMESSAGE_CHECK, 
								  NULL, MESSAGE_CHECK_PKC_ENCRYPT );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		retExt( CRYPT_ERROR_INVALID, 
				( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
				  "Ephemeral SCEP client certificate isn't valid for "
				  "signing/encryption" ) );
		}

	/* Get the nonce and transaction ID and save it for the reply */
	setMessageData( &msgData, protocolInfo->nonce, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCmsAttributes, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_SCEP_SENDERNONCE );
	if( cryptStatusOK( status ) )
		{
		protocolInfo->nonceSize = msgData.length;
		setMessageData( &msgData, protocolInfo->transID, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( iCmsAttributes, IMESSAGE_GETATTRIBUTE_S,
								  &msgData,
								  CRYPT_CERTINFO_SCEP_TRANSACTIONID );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Request is missing a nonce/transaction ID" ) );
		}
	protocolInfo->transIDsize = msgData.length;

	/* We've now got enough request data available to construct a SCEP-level 
	   response to an error instead of an HTTP-level one, let the caller 
	   know.  Note that this lets an attacker know that they've made it this
	   far in the checking, but it's not obvious that this is a security
	   problem, especially since they can get a good idea of how far they 
	   got from the different error conditions that will be returned */
	*requestDataAvailable = TRUE;

	/* Since the request is for a cryptlib server it'll have a transaction 
	   ID that's a cryptlib-encoded PKI user ID.  If we don't get this then 
	   we reject the request */
	if( protocolInfo->transIDsize != 17 || \
		!isPKIUserValue( protocolInfo->transID, protocolInfo->transIDsize ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO,
				  "Request has an invalid transaction ID '%s'",
				  sanitiseString( protocolInfo->transID, 
								  protocolInfo->transIDsize,
				  				  protocolInfo->transIDsize ) ) );
		}
	status = updateSessionInfo( &sessionInfoPtr->attributeList,
								CRYPT_SESSINFO_USERNAME, 
								protocolInfo->transID, 
								protocolInfo->transIDsize, 
								CRYPT_MAX_TEXTSIZE, ATTR_FLAG_ENCODEDVALUE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Check that we've been sent the correct type of message */
	status = getScepStatusValue( iCmsAttributes,
								 CRYPT_CERTINFO_SCEP_MESSAGETYPE, &value );
	if( cryptStatusOK( status ) && \
		( value != MESSAGETYPE_PKCSREQ_VALUE && \
		  value != MESSAGETYPE_RENEWAL_VALUE ) )
		status = CRYPT_ERROR_BADDATA;
	krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Incorrect SCEP message type %d", value ) );
		}
	scepInfo->requestType = ( value == MESSAGETYPE_PKCSREQ_VALUE ) ? \
							CRYPT_REQUESTTYPE_INITIALISATION : \
							CRYPT_REQUESTTYPE_CERTIFICATE;

	/* Phase 2: Decrypt the data using our CA key */
	status = envelopeUnwrap( sessionInfoPtr->receiveBuffer, dataLength,
							 sessionInfoPtr->receiveBuffer, 
							 sessionInfoPtr->receiveBufSize, &dataLength, 
							 sessionInfoPtr->privateKey, &errorInfo );
	if( cryptStatusError( status ) )
		{
		retExtErr( status, 
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Couldn't decrypt CMS enveloped data in client "
					 "request: " ) );
		}

	/* Import the request as a PKCS #10 request */
	setMessageCreateObjectIndirectInfo( &createInfo,
								sessionInfoPtr->receiveBuffer, dataLength,
								CRYPT_CERTTYPE_CERTREQUEST );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid PKCS #10 request in client request" ) );
		}

	/* Finally, check the certificate used to sign the SCEP message.  If 
	   it's an initialisation request then the the key in the PKCS #10 
	   request has to match the one in the signing certificate */
	if( scepInfo->requestType == CRYPT_REQUESTTYPE_INITIALISATION )
		{
		setMessageData( &msgData, keyIDbuffer, KEYID_SIZE );
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_KEYID );
		if( cryptStatusOK( status ) )
			{
			status = krnlSendMessage( protocolInfo->iScepCert, 
									  IMESSAGE_COMPARE, &msgData, 
									  MESSAGE_COMPARE_KEYID );
			}
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, 
							  IMESSAGE_DECREFCOUNT );
			retExt( CRYPT_ERROR_INVALID, 
					( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
					  "Key in client request signing certificate doesn't "
					  "match key in PKCS #10 request" ) );
			}
		}
	else
		{
		MESSAGE_KEYMGMT_INFO getkeyInfo;
		BYTE certIDbuffer[ CRYPT_MAX_HASHSIZE + 8 ];

		/* It's a renewal request, the signing certificate has to be one
		   that we've issued in the past */
		setMessageData( &msgData, certIDbuffer, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_CERTINFO_FINGERPRINT_SHA1 );
		if( cryptStatusOK( status ) )
			{
			setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_CERTID, 
								   msgData.data, msgData.length, NULL, 0, 
								   KEYMGMT_FLAG_CHECK_ONLY );
			status = krnlSendMessage( sessionInfoPtr->cryptKeyset, 
									  IMESSAGE_KEY_GETKEY, &getkeyInfo, 
									  KEYMGMT_ITEM_PUBLICKEY );
			}
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, 
							  IMESSAGE_DECREFCOUNT );
			retExt( CRYPT_ERROR_INVALID, 
					( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
					  "Unrecognised certificate used to sign client "
					  "renewal request" ) );
			}
		}

	sessionInfoPtr->iCertRequest = createInfo.cryptHandle;
	return( CRYPT_OK );
	}

/* Issue a certificate from a SCEP request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int issueCertFromRequest( INOUT SESSION_INFO *sessionInfoPtr )
	{
	MESSAGE_KEYMGMT_INFO setkeyInfo;
	MESSAGE_CERTMGMT_INFO certMgmtInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Check that the request is permitted and add it to the certificate
	   store */
	status = checkPkiUserInfo( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0, 
						   NULL, 0, KEYMGMT_FLAG_NONE );
	setkeyInfo.cryptHandle = sessionInfoPtr->iCertRequest;
	status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_KEY_SETKEY, &setkeyInfo, 
							  KEYMGMT_ITEM_REQUEST );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Request couldn't be added to certificate store" ) );
		}

	/* Convert the request into a certificate */
	setMessageCertMgmtInfo( &certMgmtInfo, sessionInfoPtr->privateKey,
							sessionInfoPtr->iCertRequest );
	status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_KEY_CERTMGMT, &certMgmtInfo,
							  CRYPT_CERTACTION_ISSUE_CERT );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't issue certificate for user" ) );
		}
	sessionInfoPtr->iCertResponse = certMgmtInfo.cryptCert;
	
	return( CRYPT_OK );
	}

/* Create an SCEP response message */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int createScepResponse( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT SCEP_PROTOCOL_INFO *protocolInfo )
	{
	CRYPT_CERTIFICATE iCmsAttributes;
	MESSAGE_DATA msgData;
	ERROR_INFO errorInfo;
	int dataLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );

	/* Extract the response data into the session buffer */
	setMessageData( &msgData, sessionInfoPtr->receiveBuffer,
					sessionInfoPtr->receiveBufSize );
	status = krnlSendMessage( sessionInfoPtr->iCertResponse,
							  IMESSAGE_CRT_EXPORT, &msgData,
							  CRYPT_CERTFORMAT_CERTCHAIN );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't get PKCS #7 certificate chain from SCEP "
				  "response object" ) );
		}
	DEBUG_DUMP_FILE( "scep_sresp0", sessionInfoPtr->receiveBuffer, 
					 msgData.length );

	/* Phase 1: Encrypt the data using the client's key */
	status = envelopeWrap( sessionInfoPtr->receiveBuffer, msgData.length,
						   sessionInfoPtr->receiveBuffer, 
						   sessionInfoPtr->receiveBufSize, &dataLength, 
						   CRYPT_FORMAT_CMS, CRYPT_CONTENT_NONE, 
						   protocolInfo->iScepCert, &errorInfo );
	if( cryptStatusError( status ) )
		{
		retExtErr( status,
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Couldn't encrypt response data with client key: " ) );
		}
	DEBUG_DUMP_FILE( "scep_sresp1", sessionInfoPtr->receiveBuffer, 
					 dataLength );

	/* Create the SCEP signing attributes */
	status = createScepAttributes( sessionInfoPtr, protocolInfo,  
								   &iCmsAttributes, MESSAGETYPE_CERTREP, 
								   CRYPT_OK );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create SCEP response signing attributes" ) );
		}

	/* Phase 2: Sign the data using the CA key and SCEP attributes */
	status = envelopeSign( sessionInfoPtr->receiveBuffer, dataLength,
						   sessionInfoPtr->receiveBuffer, 
						   sessionInfoPtr->receiveBufSize, 
						   &sessionInfoPtr->receiveBufEnd, 
						   CRYPT_CONTENT_NONE, sessionInfoPtr->privateKey, 
						   iCmsAttributes, &errorInfo );
	krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		retExtErr( status,
				   ( status, SESSION_ERRINFO, &errorInfo, 
					 "Couldn't sign response data with CA key: " ) );
		}
	DEBUG_DUMP_FILE( "scep_sresp2", sessionInfoPtr->receiveBuffer, 
					 sessionInfoPtr->receiveBufEnd );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							SCEP Server Functions							*
*																			*
****************************************************************************/

/* Exchange data with a SCEP client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int serverTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	SCEP_PROTOCOL_INFO protocolInfo;
	HTTP_DATA_INFO httpDataInfo;
	HTTP_URI_INFO httpReqInfo;
	STREAM stream;
	BOOLEAN requestDataOK, processedAdditionalRequest = TRUE;
	int requestCount, length DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* SCEP is a weird protocol that started out as a basic IPsec 
	   RSA certificate-provisioning mechanism for routers but then had a 
	   pile of additional functionality bolted onto it via HTTP mechanisms 
	   rather than having the protocol itself handle the extra 
	   functionality.  Because of this we have to handle not only the 
	   standard HTTP-as-a-substrate mechanism used by the other protocols 
	   but also HTTP GET requests for additional information that the 
	   original protocol didn't accommodate.  This means that we have to
	   set the allowed request type to STREAM_HTTPREQTYPE_ANY and provide
	   an additional HTTP_URI_INFO for the HTTP_DATA_INFO to contain the
	   GET request data if that's what the client is sending */
	sessionInfoPtr->receiveBufEnd = 0;
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_HTTPREQTYPES, 
			   STREAM_HTTPREQTYPE_ANY );
	for( requestCount = 0; requestCount < 5; requestCount++ )
		{
		memset( &httpReqInfo, 0, sizeof( HTTP_URI_INFO ) );
		initHttpDataInfoEx( &httpDataInfo, sessionInfoPtr->receiveBuffer,
							sessionInfoPtr->receiveBufSize, 
							&httpReqInfo );
		status = sread( &sessionInfoPtr->stream, &httpDataInfo,
						sizeof( HTTP_DATA_INFO ) );
		if( cryptStatusError( status ) )
			{
			/* If we've processed one of the bolted-on additions to SCEP 
			   then a read error at this point isn't necessarily a protocol
			   error */
			if( processedAdditionalRequest && status == CRYPT_ERROR_READ )
				{
				/* It's possible to send one or more of the bolted-in
				   protocol messages without ever running the actual SCEP
				   protocol, so if we get a read error after performing a
				   bolted-on exchange then we report a success status
				   since that may be all that the client was intending to 
				   do */
				return( CRYPT_OK );
				}
			sNetGetErrorInfo( &sessionInfoPtr->stream, 
							  &sessionInfoPtr->errorInfo );
			return( status );
			}

		/* If it's a proper SCEP protocol message, switch back to handling 
		   the main protocol */
		if( httpDataInfo.reqType != STREAM_HTTPREQTYPE_GET )
			{
			sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_HTTPREQTYPES, 
					   STREAM_HTTPREQTYPE_POST );
			length = httpDataInfo.bytesAvail;
			break;
			}

		/* It's one of the bolted-on additions to the basic SCEP protocol,
		   handle it specially */
		status = processAdditionalScepRequest( sessionInfoPtr, 
											   &httpReqInfo );
		if( cryptStatusError( status ) )
			return( status );
		processedAdditionalRequest = TRUE;
		}
	if( requestCount >= 5 )
		{
		/* The exact type of error response to send at this point is a bit
		   tricky, the least inappropriate one is probably 
		   CRYPT_ERROR_DUPLICATE to indicate that too many duplicate 
		   requests were sent, since to get here the client would have had 
		   to send repeated identical bolt-on requests */
		sendCertErrorResponse( sessionInfoPtr, CRYPT_ERROR_DUPLICATE );
		retExt( CRYPT_ERROR_OVERFLOW,
				( CRYPT_ERROR_OVERFLOW, SESSION_ERRINFO, 
				  "Excessive number (more than %d) of SCEP requests "
				  "encountered", requestCount ) );
		}

	/* Unfortunately we can't use readPkiDatagram() because of the weird 
	   dual-purpose HTTP transport used in SCEP where the main protocol uses 
	   POST + read response while the bolted-on portions use various GET
	   variations, so we have to duplicate portions of readPkiDatagram() 
	   here.  See the readPkiDatagram() function for code comments 
	   explaining the following operations */
	if( length < 4 || length >= MAX_BUFFER_SIZE )
		{
		sendCertErrorResponse( sessionInfoPtr, CRYPT_ERROR_BADDATA );
		retExt( CRYPT_ERROR_UNDERFLOW,
				( CRYPT_ERROR_UNDERFLOW, SESSION_ERRINFO, 
				  "Invalid PKI message length %d", length ) );
		}
	status = length = \
				checkObjectEncoding( sessionInfoPtr->receiveBuffer, length );
	if( cryptStatusError( status ) )
		{
		sendCertErrorResponse( sessionInfoPtr, CRYPT_ERROR_BADDATA );
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid PKI message encoding" ) );
		}
	sessionInfoPtr->receiveBufEnd = length;

	/* Basic lint filter to check for approximately-OK requests before we
	   try applying enveloping operations to the data:

		SEQUENCE {
			OID signedData		-- contentType
			[0] {				-- content
				SEQUENCE {
					INTEGER 1	-- version
					... */
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer,
				 sessionInfoPtr->receiveBufEnd );
	readSequence( &stream, NULL );
	readUniversal( &stream );
	readConstructed( &stream, NULL, 0 );
	readSequence( &stream, NULL );
	status = readInteger( &stream, NULL, 2, NULL );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid SCEP request header" ) );
		}

	/* Process the initial message from the client */
	initSCEPprotocolInfo( &protocolInfo );
	status = checkScepRequest( sessionInfoPtr, &protocolInfo, 
							   &requestDataOK );
	if( cryptStatusError( status ) )
		{
		/* If we got far enough into the request data to be able to send a 
		   SCEP-level response, send that, otherwise just send an HTTP-level
		   response */
		if( requestDataOK )
			sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
		else
			sendCertErrorResponse( sessionInfoPtr, status );
		return( status );
		}

	/* Issue a certificate from the request */
	status = issueCertFromRequest( sessionInfoPtr );
	if( cryptStatusError( status ) )
		{
		sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
		destroySCEPprotocolInfo( &protocolInfo );
		return( status );
		}

	/* Return the certificate to the client */
	status = createScepResponse( sessionInfoPtr, &protocolInfo );
	if( cryptStatusOK( status ) )
		status = writePkiDatagram( sessionInfoPtr, SCEP_CONTENT_TYPE, 
								   SCEP_CONTENT_TYPE_LEN );
	destroySCEPprotocolInfo( &protocolInfo );
	return( status );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initSCEPserverProcessing( SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	sessionInfoPtr->transactFunction = serverTransact;
	}
#endif /* USE_SCEP */
