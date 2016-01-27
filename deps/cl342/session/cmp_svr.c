/****************************************************************************
*																			*
*						 cryptlib CMP Server Management						*
*						Copyright Peter Gutmann 1999-2009					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "session.h"
  #include "cmp.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "session/session.h"
  #include "session/cmp.h"
#endif /* Compiler-specific includes */

#ifdef USE_CMP

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Map request to response types */

static const MAP_TABLE reqClibReqMapTbl[] = {
	{ CTAG_PB_IR, CRYPT_REQUESTTYPE_INITIALISATION },
	{ CTAG_PB_CR, CRYPT_REQUESTTYPE_CERTIFICATE },
	{ CTAG_PB_P10CR, CRYPT_REQUESTTYPE_CERTIFICATE },
	{ CTAG_PB_KUR, CRYPT_REQUESTTYPE_KEYUPDATE },
	{ CTAG_PB_RR, CRYPT_REQUESTTYPE_REVOCATION },
	{ CTAG_PB_GENM, CRYPT_REQUESTTYPE_PKIBOOT },
	{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
	};

CHECK_RETVAL_RANGE( CRYPT_REQUESTYPE_NONE, CRYPT_REQUESTYPE_LAST ) \
static int reqToClibReq( IN_ENUM_OPT( CTAG_PB ) const CMP_MESSAGE_TYPE reqType )
	{
	int value, status;

	REQUIRES( reqType >= CTAG_PB_IR && reqType < CTAG_PB_LAST );
			  /* CTAG_PB_IR == 0 so this is the same as _NONE */

	status = mapValue( reqType, &value, reqClibReqMapTbl, 
					   FAILSAFE_ARRAYSIZE( reqClibReqMapTbl, MAP_TABLE ) );
	return( cryptStatusError( status ) ? status : value );
	}

/* Set up user authentication information (either a MAC context or a public 
   key) based on a request submitted by the client.  This is done whenever 
   the client starts a new transaction with a new user ID or certificate 
   ID */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initServerAuthentMAC( INOUT SESSION_INFO *sessionInfoPtr, 
						  INOUT CMP_PROTOCOL_INFO *protocolInfo )
	{
	CMP_INFO *cmpInfo = sessionInfoPtr->sessionCMP;
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	MESSAGE_DATA msgData;
	char password[ CRYPT_MAX_TEXTSIZE + 8 ];
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Set up general authentication information and if there's user 
	   information still present from a previous transaction, clear it */
	status = setCMPprotocolInfo( protocolInfo, NULL, 0, 
								 CMP_INIT_FLAG_MACCTX, TRUE );
	if( cryptStatusError( status ) )
		return( status );
	if( cmpInfo->userInfo != CRYPT_ERROR )
		{
		krnlSendNotifier( cmpInfo->userInfo, IMESSAGE_DECREFCOUNT );
		cmpInfo->userInfo = CRYPT_ERROR;
		}
	if( protocolInfo->altMacKeySize > 0 )
		{
		zeroise( protocolInfo->altMacKey, CRYPT_MAX_HASHSIZE );
		protocolInfo->altMacKeySize = 0;
		}

	/* Get the user information for the user identified by the user ID from 
	   the certificate store.  If we get a not-found error we report it as 
	   "signer not trusted", which can also mean "signer unknown" */
	setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_KEYID,
						   protocolInfo->userID, protocolInfo->userIDsize, 
						   NULL, 0, KEYMGMT_FLAG_NONE );
	status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_KEY_GETKEY, &getkeyInfo, 
							  KEYMGMT_ITEM_PKIUSER );
	if( cryptStatusError( status ) )
		{
		const ATTRIBUTE_LIST *userNamePtr = \
						findSessionInfo( sessionInfoPtr->attributeList,
										 CRYPT_SESSINFO_USERNAME );
		char userID[ CRYPT_MAX_TEXTSIZE + 8 ];
		int userIDlen;

		REQUIRES( userNamePtr != NULL );
		if( userNamePtr->flags & ATTR_FLAG_ENCODEDVALUE )
			{
			userIDlen = min( userNamePtr->valueLength, CRYPT_MAX_TEXTSIZE );
			memcpy( userID, userNamePtr->value, userIDlen );
			}
		else
			{
			/* The ID isn't a cryptlib user ID but a certificate ID used for 
			   a cr rather than the user ID for an ir or rr (see the comment 
			   at the start of initServerAuthentSign() for details), we have 
			   to use a placeholder string since it's meaningless binary 
			   data.  We can't try and map this to a user ID since we've just 
			   failed to find the PKI user object that would contain it.
			   
			   Alternatively, in less likely cases it could also be an 
			   incorrect user ID that we couldn't recognise and record as a 
			   cryptlib user ID, which we can't do much with either */
			userIDlen = 18;
			memcpy( userID, "the requested user", userIDlen );
			}
		protocolInfo->pkiFailInfo = CMPFAILINFO_SIGNERNOTTRUSTED;
		retExtObj( status, 
				   ( status, SESSION_ERRINFO, sessionInfoPtr->cryptKeyset,
					 "Couldn't find PKI user information for %s",
					 sanitiseString( userID, CRYPT_MAX_TEXTSIZE, 
									 userIDlen ) ) );
		}
	cmpInfo->userInfo = getkeyInfo.cryptHandle;
	protocolInfo->userIDchanged = FALSE;

	/* Get the password from the PKI user object */
	setMessageData( &msgData, password, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( cmpInfo->userInfo, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD );
	if( cryptStatusOK( status ) )
		{
		status = updateSessionInfo( &sessionInfoPtr->attributeList,
									CRYPT_SESSINFO_PASSWORD, password, 
									msgData.length, CRYPT_MAX_TEXTSIZE,
									ATTR_FLAG_ENCODEDVALUE );
		}
	zeroise( password, CRYPT_MAX_TEXTSIZE );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Couldn't copy PKI user data from PKI user object to "
				  "session object" ) );
		}

	/* Things get a bit complicated for password-authenticated operations 
	   because there's a second password that may (optionally) be used for
	   revocations that doesn't fit into the standard pattern of username+
	   password.  In order to handle MAC'd revocation requests, we keep a 
	   copy of the revocation MAC key and use it in place of the standard
	   password if we get a revocation request rather than an issue
	   request */
	setMessageData( &msgData, password, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( cmpInfo->userInfo, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_CERTINFO_PKIUSER_REVPASSWORD );
	if( cryptStatusOK( status ) )
		{
		status = decodePKIUserValue( protocolInfo->altMacKey, CRYPT_MAX_HASHSIZE, 
									 &protocolInfo->altMacKeySize, password, 
									 msgData.length );
		ENSURES( cryptStatusOK( status ) );
		}
	zeroise( password, CRYPT_MAX_TEXTSIZE );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initServerAuthentSign( INOUT SESSION_INFO *sessionInfoPtr, 
						   INOUT CMP_PROTOCOL_INFO *protocolInfo )
	{
	CMP_INFO *cmpInfo = sessionInfoPtr->sessionCMP;
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	MESSAGE_DATA msgData;
	char userName[ CRYPT_MAX_TEXTSIZE + 8 ];
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Set up general authentication information and if there's client 
	   authentication information still present from a previous transaction 
	   that used MAC authentication, clear it */
	status = setCMPprotocolInfo( protocolInfo, NULL, 0, CMP_INIT_FLAG_NONE,
								 TRUE );
	if( cryptStatusError( status ) )
		return( status );
	if( cmpInfo->userInfo != CRYPT_ERROR )
		{
		krnlSendNotifier( cmpInfo->userInfo, IMESSAGE_DECREFCOUNT );
		cmpInfo->userInfo = CRYPT_ERROR;
		}

	/* Get the user information for the user that originally authorised the 
	   issue of the certificate that signed the request.  This serves two 
	   purposes, it obtains the original user ID if it wasn't supplied in 
	   the request (for example if the request uses only a certificate ID 
	   from a certificate five generations downstream from the original PKI
	   user after an ir -> cr -> cr -> cr -> cr and we've got the 
	   certificate ID from the final cr in the series), and it verifies that 
	   the authorising certificate belongs to a currently valid user */
	setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_CERTID,
						   protocolInfo->certID, protocolInfo->certIDsize, 
						   NULL, 0, KEYMGMT_FLAG_GETISSUER );
	status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_KEY_GETKEY, &getkeyInfo, 
							  KEYMGMT_ITEM_PKIUSER );
	if( cryptStatusError( status ) )
		{
		protocolInfo->pkiFailInfo = CMPFAILINFO_SIGNERNOTTRUSTED;
		retExtObj( status, 
				   ( status, SESSION_ERRINFO, sessionInfoPtr->cryptKeyset,
					 "Couldn't find PKI user information for owner of "
					 "requesting certificate" ) );
		}
	cmpInfo->userInfo = getkeyInfo.cryptHandle;

	/* Update the user ID from the PKI user object */
	setMessageData( &msgData, userName, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( getkeyInfo.cryptHandle, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_CERTINFO_PKIUSER_ID );
	if( cryptStatusOK( status ) )
		{
		status = updateSessionInfo( &sessionInfoPtr->attributeList,
									CRYPT_SESSINFO_USERNAME, userName, 
									msgData.length, CRYPT_MAX_TEXTSIZE,
									ATTR_FLAG_ENCODEDVALUE );
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Couldn't copy PKI user data from PKI user object to "
				  "session object" ) );
		}

	/* Get the public key identified by the certificate ID from the 
	   certificate store.  This verifies the assumption that the owner of an 
	   existing certificate/existing user is authorised to request further 
	   certificates using the existing one.  If we get a not found error we 
	   report it as "signer not trusted", which can also mean "signer 
	   unknown" */
	setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_CERTID,
						   protocolInfo->certID, protocolInfo->certIDsize, 
						   NULL, 0, KEYMGMT_FLAG_USAGE_SIGN );
	status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_KEY_GETKEY, &getkeyInfo, 
							  KEYMGMT_ITEM_PUBLICKEY );
	if( cryptStatusError( status ) )
		{
		protocolInfo->pkiFailInfo = CMPFAILINFO_SIGNERNOTTRUSTED;
		retExtObj( status, 
				   ( status, SESSION_ERRINFO, sessionInfoPtr->cryptKeyset,
					 "Couldn't find certificate for requested user" ) );
		}
	sessionInfoPtr->iAuthInContext = getkeyInfo.cryptHandle;
	protocolInfo->certIDchanged = FALSE;

	return( CRYPT_OK );
	}

/* Deliver an Einladung betreff Kehrseite to the client.  We don't bother
   checking the return value for the write since there's nothing that we can 
   do in the case of an error except close the connection, which we do 
   anyway since this is the last message */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void sendErrorResponse( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT CMP_PROTOCOL_INFO *protocolInfo,
							   IN_ERROR const int errorStatus )
	{
	BOOLEAN writeHttpResponseOnly = !protocolInfo->headerRead;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );
	
	REQUIRES_V( cryptStatusError( errorStatus ) );

	/* If we were going to protect the communication with the client with a
	   MAC and something failed we don't try and MAC the response since the 
	   failure could be a client MAC failure, failure to locate the MAC key, 
	   or something similar */
	protocolInfo->useMACsend = FALSE;
	protocolInfo->status = errorStatus;

	/* Write the error response if we can.  We only do this if at least the
	   header of the client's message was successfully read (indicated by 
	   the headerRead flag in the protocolInfo being set), otherwise we 
	   can't create our own header for the response */
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_LASTMESSAGE, TRUE );
	if( !writeHttpResponseOnly )
		{
		status = writePkiMessage( sessionInfoPtr, protocolInfo, CMPBODY_ERROR );
		if( cryptStatusError( status ) )
			writeHttpResponseOnly = TRUE;
		}
	if( writeHttpResponseOnly )
		{
		HTTP_DATA_INFO httpDataInfo;

		/* If we encounter an error processing the initial request then 
		   there won't be enough information available to create an error 
		   response.  At this point the best that we can do is send an error
		   at the HTTP level */
		initHttpDataInfo( &httpDataInfo, sessionInfoPtr->receiveBuffer,
						  sessionInfoPtr->receiveBufSize );
		httpDataInfo.reqStatus = errorStatus;
		swrite( &sessionInfoPtr->stream, &httpDataInfo, 
				sizeof( HTTP_DATA_INFO ) );
		}
	else
		{
		DEBUG_DUMP_CMP( CTAG_PB_ERROR, 1, sessionInfoPtr );
		( void ) writePkiDatagram( sessionInfoPtr, CMP_CONTENT_TYPE, 
								   CMP_CONTENT_TYPE_LEN );
		}
	}

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

/* Exchange data with a CMP client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int serverTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	CMP_INFO *cmpInfo = sessionInfoPtr->sessionCMP;
	MESSAGE_CERTMGMT_INFO certMgmtInfo;
	MESSAGE_KEYMGMT_INFO setkeyInfo;
	STREAM stream;
	CMP_PROTOCOL_INFO protocolInfo;
	const ATTRIBUTE_LIST *userNamePtr;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Initialise the server-side protocol state information.  Since the 
	   server doesn't have a user ID (it uses what the client sends it), we 
	   set the userID-sent flag to indicate that it's been implicitly 
	   exchanged */
	initCMPprotocolInfo( &protocolInfo,
						 ( sessionInfoPtr->flags & SESSION_ISCRYPTLIB ) ? \
						 TRUE : FALSE, TRUE );
	protocolInfo.authContext = sessionInfoPtr->privateKey;
	sessionInfoPtr->protocolFlags |= CMP_PFLAG_USERIDSENT;
	userNamePtr = findSessionInfo( sessionInfoPtr->attributeList,
								   CRYPT_SESSINFO_USERNAME );
	if( userNamePtr != NULL )
		{
		BYTE userID[ CRYPT_MAX_TEXTSIZE ];
		int userIDsize;

		/* There's already user information present from a previous 
		   transaction, try and re-use it (this can happen if we're
		   handling a series of transactions from the client, see the
		   dicussion in cmp.h for details).  This information can still
		   be overridden by the client sending us new user information */
		if( userNamePtr->flags & ATTR_FLAG_ENCODEDVALUE )
			{
			/* It's a cryptlib-style encoded user ID, decode it into its 
			   binary value */
			status = decodePKIUserValue( userID, CRYPT_MAX_TEXTSIZE,
										 &userIDsize,
										 userNamePtr->value,
										 userNamePtr->valueLength );
			ENSURES( cryptStatusOK( status ) );
			}
		else
			{
			/* It's a standard user ID, use it as is */
			userIDsize = min( userNamePtr->valueLength, CRYPT_MAX_TEXTSIZE );
			memcpy( userID, userNamePtr->value, userIDsize );
			}
		status = setCMPprotocolInfo( &protocolInfo, userID, userIDsize,
									 CMP_INIT_FLAG_USERID, FALSE );
		if( cryptStatusError( status ) )
			{
			destroyCMPprotocolInfo( &protocolInfo );
			return( status );
			}

		/* We're (potentially) re-using user information from a previous 
		   session, if there are cryptographic credentials associated with
		   the user information we have to move them over to the current
		   protocol state data */
		if( cmpInfo->savedMacContext != CRYPT_ERROR )
			{
			protocolInfo.iMacContext = cmpInfo->savedMacContext;
			cmpInfo->savedMacContext = CRYPT_ERROR;
			}
		}

	/* Read the initial message from the client.  We don't write an error
	   response at the initial read stage to prevent scanning/DOS attacks 
	   (vir sapit qui pauca loquitur) */
	status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusError( status ) )
		{
		destroyCMPprotocolInfo( &protocolInfo );
		return( status );
		}

	/* Basic lint filter to check for approximately-OK requests before we
	   try applying message-processing operations to the data:

		SEQUENCE {
			SEQUENCE {
				INTEGER 1	-- version
				... */
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer,
				 sessionInfoPtr->receiveBufEnd );
	readSequence( &stream, NULL );
	readSequence( &stream, NULL );
	status = readInteger( &stream, NULL, 2, NULL );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid CMP request header" ) );
		}

	/* Process the initial client message */
	status = readPkiMessage( sessionInfoPtr, &protocolInfo,
							 CTAG_PB_READ_ANY );
	if( cryptStatusOK( status ) )
		{
		status = cmpInfo->requestType = \
					reqToClibReq( protocolInfo.operation );
		}
	if( cryptStatusError( status ) )
		{
		sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
		destroyCMPprotocolInfo( &protocolInfo );
		return( status );
		}
	DEBUG_DUMP_CMP( protocolInfo.operation, 1, sessionInfoPtr );

	/* If it's a PKIBoot request, send the PKIBoot response */
	if( cmpInfo->requestType == CRYPT_REQUESTTYPE_PKIBOOT )
		{
		status = writePkiMessage( sessionInfoPtr, &protocolInfo, 
								  CMPBODY_GENMSG );
		if( cryptStatusOK( status ) )
			{
			DEBUG_DUMP_CMP( CTAG_PB_GENM, 2, sessionInfoPtr );
			status = writePkiDatagram( sessionInfoPtr, CMP_CONTENT_TYPE, 
									   CMP_CONTENT_TYPE_LEN );
			}
		if( cryptStatusError( status ) )
			{
			sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
			destroyCMPprotocolInfo( &protocolInfo );
			return( status );
			}

		/* Remember the authentication context in case we can reuse it for 
		   another transaction */
		cmpInfo->savedMacContext = protocolInfo.iMacContext;
		protocolInfo.iMacContext = CRYPT_ERROR;

		return( CRYPT_OK );
		}

	/* Make sure that the signature on the request data is OK (unless it's a 
	   non-signed revocation request or a request for an encryption-only 
	   key) */
	if( protocolInfo.operation != CTAG_PB_RR && !protocolInfo.cryptOnlyKey )
		{
		status = krnlSendMessage( sessionInfoPtr->iCertRequest,
								  IMESSAGE_CRT_SIGCHECK, NULL, 
								  CRYPT_UNUSED );
		if( cryptStatusError( status ) )
			{
			sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
			destroyCMPprotocolInfo( &protocolInfo );
			retExt( status, 
					( status, SESSION_ERRINFO, 
					  "Certificate request signature check failed" ) );
			}
		}

	/* Add the request to the certificate store */
	setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0, NULL, 0,
						   ( protocolInfo.operation == CTAG_PB_IR ) ? \
								KEYMGMT_FLAG_INITIALOP : \
						   ( protocolInfo.operation == CTAG_PB_KUR ) ? \
								KEYMGMT_FLAG_UPDATE : KEYMGMT_FLAG_NONE );
	setkeyInfo.cryptHandle = sessionInfoPtr->iCertRequest;
	status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_KEY_SETKEY, &setkeyInfo, 
							  ( protocolInfo.operation == CTAG_PB_RR ) ? \
								KEYMGMT_ITEM_REVREQUEST : \
								KEYMGMT_ITEM_REQUEST );
	if( cryptStatusError( status ) )
		{
		/* If the certificate store reports that there's a problem with the 
		   request, convert it to an invalid request error */
		if( cryptArgError( status ) )
			status = CRYPT_ERROR_INVALID;

		/* A special error condition at this point arises when the user 
		   tries to submit a second initialisation request for a PKI user 
		   that has already had a certificate issued for it, so we catch 
		   this condition and provide a more informative error response to 
		   the client than the generic failure response */
		if( protocolInfo.operation == CTAG_PB_IR && \
			status == CRYPT_ERROR_DUPLICATE )
			protocolInfo.pkiFailInfo = CMPFAILINFO_DUPLICATECERTREQ;

		/* Clean up and return the appropriate error information to the
		   caller */
		sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
		destroyCMPprotocolInfo( &protocolInfo );
		if( protocolInfo.operation == CTAG_PB_IR && \
			status == CRYPT_ERROR_DUPLICATE )
			{
			retExtObj( status, 
					   ( status, SESSION_ERRINFO, sessionInfoPtr->cryptKeyset,
						 "Initialisation request couldn't be added to the "
						 "certificate store because another initialisation "
						 "request has already been processed for this user" ) );
			}
		retExtObj( status, 
				   ( status, SESSION_ERRINFO, sessionInfoPtr->cryptKeyset,
					 "Request couldn't be added to the certificate store" ) );
		}

	/* Create or revoke a certificate from the request */
	if( protocolInfo.operation != CTAG_PB_RR )
		{
		setMessageCertMgmtInfo( &certMgmtInfo, sessionInfoPtr->privateKey,
								sessionInfoPtr->iCertRequest );
		status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
								  IMESSAGE_KEY_CERTMGMT, &certMgmtInfo,
								  CRYPT_CERTACTION_CERT_CREATION );
		if( cryptStatusOK( status ) )
			{
			int value;

			/* Remember the issued certificate and save various pieces
			   of information that we'll need later */
			sessionInfoPtr->iCertResponse = certMgmtInfo.cryptCert;
			status = krnlSendMessage( sessionInfoPtr->iCertResponse,
									  IMESSAGE_GETATTRIBUTE, &value,
									  CRYPT_IATTRIBUTE_CERTHASHALGO );
			if( cryptStatusOK( status ) )
				protocolInfo.confHashAlgo = value;
			}
		}
	else
		{
		setMessageCertMgmtInfo( &certMgmtInfo, CRYPT_UNUSED,
								sessionInfoPtr->iCertRequest );
		status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
								  IMESSAGE_KEY_CERTMGMT, &certMgmtInfo,
								  CRYPT_CERTACTION_REVOKE_CERT );
		}
	if( cryptStatusError( status ) )
		{
		/* If the certificate store reports that there's a problem with the 
		   request, convert it to an invalid request error */
		if( cryptArgError( status ) )
			status = CRYPT_ERROR_INVALID;
		sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
		destroyCMPprotocolInfo( &protocolInfo );
		retExtObj( status, 
				   ( status, SESSION_ERRINFO, sessionInfoPtr->cryptKeyset,
					 "Certificate %s was denied by certificate store",
					 ( protocolInfo.operation != CTAG_PB_RR ) ? \
						"issue" : "revocation" ) );
		}

	/* Send the response to the client */
	status = writePkiMessage( sessionInfoPtr, &protocolInfo, CMPBODY_NORMAL );
	if( cryptStatusOK( status ) )
		{
		DEBUG_DUMP_CMP( protocolInfo.operation, 2, sessionInfoPtr );
		status = writePkiDatagram( sessionInfoPtr, CMP_CONTENT_TYPE,
								   CMP_CONTENT_TYPE_LEN );
		}
	if( cryptStatusError( status ) )
		{
		sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
		if( protocolInfo.operation != CTAG_PB_RR )
			{
			/* If there was a problem, drop the partially-issued certificate.  
			   We don't have to go all the way and do a full reversal 
			   because it hasn't really been issued yet since we couldn't 
			   get it to the client (or even if the client somehow got it,
			   the certificate store hasn't marked it as complete yet so 
			   it's regarded as invalid if queried).  In addition we don't 
			   do anything with the return status since we want to return 
			   the status that caused the problem, not the result of the 
			   drop operation */
			setMessageCertMgmtInfo( &certMgmtInfo, CRYPT_UNUSED,
									sessionInfoPtr->iCertResponse );
			( void ) krnlSendMessage( sessionInfoPtr->cryptKeyset,
									  IMESSAGE_KEY_CERTMGMT, &certMgmtInfo,
									  CRYPT_CERTACTION_CERT_CREATION_DROP );
			}
		destroyCMPprotocolInfo( &protocolInfo );
		return( status );
		}

	/* If it's a transaction type that doesn't need a confirmation, we're 
	   done */
	if( protocolInfo.operation == CTAG_PB_RR )
		{
		/* Remember the authentication context in case we can reuse it for 
		   another transaction */
		cmpInfo->savedMacContext = protocolInfo.iMacContext;
		protocolInfo.iMacContext = CRYPT_ERROR;

		destroyCMPprotocolInfo( &protocolInfo );
		return( CRYPT_OK );
		}

	/* Read back the confirmation from the client */
	status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusOK( status ) )
		status = readPkiMessage( sessionInfoPtr, &protocolInfo,
								 CTAG_PB_CERTCONF );
	if( cryptStatusError( status ) )
		{
		sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
		destroyCMPprotocolInfo( &protocolInfo );

		/* Reverse the certificate issue operation by revoking the 
		   incompletely-issued certificate, returning the status that caused 
		   the failure earlier on */
		setMessageCertMgmtInfo( &certMgmtInfo, CRYPT_UNUSED,
								sessionInfoPtr->iCertResponse );
		( void ) krnlSendMessage( sessionInfoPtr->cryptKeyset,
								  IMESSAGE_KEY_CERTMGMT, &certMgmtInfo,
								  CRYPT_CERTACTION_CERT_CREATION_REVERSE );
		return( status );
		}
	DEBUG_DUMP_CMP( protocolInfo.operation, 3, sessionInfoPtr );
	if( protocolInfo.status == CRYPT_ERROR )
		{
		int localStatus;

		/* The client rejected the certificate, this isn't a protocol error 
		   so we send back a standard ack */
		status = writePkiMessage( sessionInfoPtr, &protocolInfo, 
								  CMPBODY_ACK );
		if( cryptStatusOK( status ) )
			{
			status = writePkiDatagram( sessionInfoPtr, CMP_CONTENT_TYPE, 
									   CMP_CONTENT_TYPE_LEN );
			}
		destroyCMPprotocolInfo( &protocolInfo );

		/* Reverse the certificate issue operation by revoking the 
		   incompletely-issued certificate */
		setMessageCertMgmtInfo( &certMgmtInfo, CRYPT_UNUSED,
								sessionInfoPtr->iCertResponse );
		localStatus = krnlSendMessage( sessionInfoPtr->cryptKeyset,
									   IMESSAGE_KEY_CERTMGMT, &certMgmtInfo,
									   CRYPT_CERTACTION_CERT_CREATION_REVERSE );
		return( cryptStatusOK( status ) ? localStatus : status );
		}

	/* The client has confirmed the certificate creation, finalise it */
	setMessageCertMgmtInfo( &certMgmtInfo, CRYPT_UNUSED,
							sessionInfoPtr->iCertResponse );
	status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_KEY_CERTMGMT, &certMgmtInfo,
							  CRYPT_CERTACTION_CERT_CREATION_COMPLETE );
	if( cryptStatusError( status ) )
		{
		sendErrorResponse( sessionInfoPtr, &protocolInfo, status );
		destroyCMPprotocolInfo( &protocolInfo );
		retExtObj( status, 
				   ( status, SESSION_ERRINFO, sessionInfoPtr->cryptKeyset,
					 "Certificate issue completion failed" ) );
		}

	/* Send back the final ack and clean up */
	status = writePkiMessage( sessionInfoPtr, &protocolInfo, CMPBODY_ACK );
	if( cryptStatusOK( status ) )
		{
		DEBUG_DUMP_CMP( protocolInfo.operation, 4, sessionInfoPtr );
		status = writePkiDatagram( sessionInfoPtr, CMP_CONTENT_TYPE,
								   CMP_CONTENT_TYPE_LEN );
		}

	/* Remember the authentication context in case we can reuse it for 
	   another transaction */
	cmpInfo->savedMacContext = protocolInfo.iMacContext;
	protocolInfo.iMacContext = CRYPT_ERROR;

	destroyCMPprotocolInfo( &protocolInfo );
	return( status );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initCMPserverProcessing( SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	sessionInfoPtr->transactFunction = serverTransact;
	}
#endif /* USE_CMP */
