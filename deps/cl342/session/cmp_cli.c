/****************************************************************************
*																			*
*						 cryptlib CMP Client Management						*
*						Copyright Peter Gutmann 1999-2009					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "session.h"
  #include "cmp.h"
#else
  #include "crypt.h"
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

static const MAP_TABLE clibReqReqMapTbl[] = {
	{ CRYPT_REQUESTTYPE_INITIALISATION, CTAG_PB_IR },
	{ CRYPT_REQUESTTYPE_CERTIFICATE, CTAG_PB_CR },
	{ CRYPT_REQUESTTYPE_CERTIFICATE, CTAG_PB_P10CR },
	{ CRYPT_REQUESTTYPE_KEYUPDATE, CTAG_PB_KUR },
	{ CRYPT_REQUESTTYPE_REVOCATION, CTAG_PB_RR },
	{ CRYPT_REQUESTTYPE_PKIBOOT, CTAG_PB_GENM },
	{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
	};

CHECK_RETVAL_RANGE( 0, CTAG_PB_LAST ) \
static int clibReqToReq( IN_ENUM( CRYPT_REQUESTTYPE ) const int reqType )
	{
	int value, status;

	REQUIRES( reqType > CRYPT_REQUESTTYPE_NONE && \
			  reqType < CRYPT_REQUESTTYPE_LAST );

	status = mapValue( reqType, &value, clibReqReqMapTbl, 
					   FAILSAFE_ARRAYSIZE( clibReqReqMapTbl, MAP_TABLE ) );
	return( cryptStatusError( status ) ? status : value );
	}

/* Set up information needed to perform a client-side transaction */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initClientInfo( INOUT SESSION_INFO *sessionInfoPtr,
						   INOUT CMP_PROTOCOL_INFO *protocolInfo )
	{
	CMP_INFO *cmpInfo = sessionInfoPtr->sessionCMP;
	const ATTRIBUTE_LIST *userNamePtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_USERNAME );
	const ATTRIBUTE_LIST *passwordPtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_PASSWORD );
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	REQUIRES( !isServer( sessionInfoPtr ) );

	/* Determine what we need to do based on the request type */
	status = protocolInfo->operation = clibReqToReq( cmpInfo->requestType );
	if( cryptStatusError( status ) )
		return( status );
	status = CRYPT_OK;	/* clibReqToReq() returns request indicator */

	/* If we're using public key-based authentication, set up the key and 
	   user ID information */
	if( cmpInfo->requestType != CRYPT_REQUESTTYPE_PKIBOOT && \
		cmpInfo->requestType != CRYPT_REQUESTTYPE_INITIALISATION && \
		!( cmpInfo->requestType == CRYPT_REQUESTTYPE_REVOCATION && \
		   passwordPtr != NULL ) )
		{
		/* If it's an encryption-only key, remember this for later when we 
		   need to authenticate our request messages */
		status = krnlSendMessage( sessionInfoPtr->privateKey, IMESSAGE_CHECK, 
								  NULL, MESSAGE_CHECK_PKC_SIGN );
		if( cryptStatusError( status ) )
			{
			/* The private key can't be used for signature creation, use
			   the alternate authentication key instead */
			protocolInfo->authContext = sessionInfoPtr->iAuthOutContext;
			protocolInfo->cryptOnlyKey = TRUE;
			}
		else
			{
			/* The private key that we're using is capable of authenticating 
			   requests */
			protocolInfo->authContext = sessionInfoPtr->privateKey;
			}

		/* If we're not talking to a cryptlib server, get the user ID.  If 
		   it's a standard signed request the authenticating object will be 
		   the private key, however if the private key is an encryption-only 
		   key the message authentication key is a separate object.  To 
		   handle this we get the user ID from the signing key rather than 
		   automatically using the private key */
		if( !protocolInfo->isCryptlib )
			{
			MESSAGE_DATA msgData;
			BYTE userID[ CRYPT_MAX_HASHSIZE + 8 ];

			setMessageData( &msgData, userID, CRYPT_MAX_HASHSIZE );
			status = krnlSendMessage( protocolInfo->authContext, 
									  IMESSAGE_GETATTRIBUTE_S, &msgData, 
									  CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER );
			if( cryptStatusOK( status ) )
				{
				status = setCMPprotocolInfo( protocolInfo, userID, 
											 msgData.length, 
											 CMP_INIT_FLAG_USERID | \
											 CMP_INIT_FLAG_TRANSID, FALSE );
				}
			return( status );
			}

		/* It's a cryptlib peer, the certificate is identified by an 
		   unambiguous certificate ID and so we don't have to try and make
		   do with an arbitrary value derived from the associated public
		   key */
		return( setCMPprotocolInfo( protocolInfo, NULL, 0, 
									CMP_INIT_FLAG_TRANSID, TRUE ) );
		}

	/* If there's a MAC context present from a previous transaction, reuse 
	   it for the current one.  See the discussion in cmp.h for details */
	if( cmpInfo->savedMacContext != CRYPT_ERROR )
		{
		status = setCMPprotocolInfo( protocolInfo, NULL, 0, 
									 CMP_INIT_FLAG_TRANSID, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		protocolInfo->useMACsend = protocolInfo->useMACreceive = TRUE;
		protocolInfo->iMacContext = cmpInfo->savedMacContext;
		cmpInfo->savedMacContext = CRYPT_ERROR;
		return( CRYPT_OK );
		}

	/* We're using MAC authentication, initialise the protocol information */
	REQUIRES( userNamePtr != NULL );
	if( userNamePtr->flags & ATTR_FLAG_ENCODEDVALUE )
		{
		BYTE decodedValue[ 64 + 8 ];
		int decodedValueLength;

		/* It's a cryptlib-style encoded user ID, decode it into its binary 
		   value */
		status = decodePKIUserValue( decodedValue, 64, &decodedValueLength,
									 userNamePtr->value, 
									 userNamePtr->valueLength );
		ENSURES( cryptStatusOK( status ) );
		status = setCMPprotocolInfo( protocolInfo, decodedValue,
									 decodedValueLength, 
									 CMP_INIT_FLAG_ALL, TRUE );
		zeroise( decodedValue, CRYPT_MAX_TEXTSIZE );
		}
	else
		{
		/* It's an arbitrary non-cryptlib user ID, use it as is */
		status = setCMPprotocolInfo( protocolInfo, userNamePtr->value,
									 min( userNamePtr->valueLength, \
										  CRYPT_MAX_TEXTSIZE ), 
									 CMP_INIT_FLAG_ALL, FALSE );
		}
	if( cryptStatusError( status ) )
		return( status );

	REQUIRES( passwordPtr != NULL );

	/* Set up the MAC context used to authenticate messages */
	if( passwordPtr->flags & ATTR_FLAG_ENCODEDVALUE )
		{
		BYTE decodedValue[ 64 + 8 ];
		int decodedValueLength;

		/* It's a cryptlib-style encoded password, decode it into its binary 
		   value */
		status = decodePKIUserValue( decodedValue, 64, &decodedValueLength,
									 passwordPtr->value, 
									 passwordPtr->valueLength );
		ENSURES( cryptStatusOK( status ) );
		status = initMacInfo( protocolInfo->iMacContext, decodedValue, 
							  decodedValueLength, protocolInfo->salt, 
							  protocolInfo->saltSize, 
							  protocolInfo->iterations );
		zeroise( decodedValue, CRYPT_MAX_TEXTSIZE );
		}
	else
		{
		/* It's an arbitrary non-cryptlib password, use it as is */
		status = initMacInfo( protocolInfo->iMacContext,
							  passwordPtr->value, passwordPtr->valueLength,
							  protocolInfo->salt, protocolInfo->saltSize,
							  protocolInfo->iterations );
		}
	return( status );
	}

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

/* Prepare a CMP session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int clientStartup( INOUT SESSION_INFO *sessionInfoPtr )
	{
	CMP_INFO *cmpInfo = sessionInfoPtr->sessionCMP;
	NET_CONNECT_INFO connectInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that we have all the needed information.  Plug-and-play PKI 
	   uses PKIBoot to get the CA certificate and generates the requests 
	   internally so we only need to check for these values if we're doing 
	   standard CMP.  The check for user ID and authentication information 
	   has already been done at the general session level */
	if( !( cmpInfo->flags & CMP_PFLAG_PNPPKI ) )
		{
		if( cmpInfo->requestType == CRYPT_REQUESTTYPE_NONE )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_CMP_REQUESTTYPE,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}
		if( cmpInfo->requestType != CRYPT_REQUESTTYPE_PKIBOOT && \
			sessionInfoPtr->iAuthInContext == CRYPT_ERROR )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_CACERTIFICATE,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}
		if( cmpInfo->requestType != CRYPT_REQUESTTYPE_PKIBOOT && \
			sessionInfoPtr->iCertRequest == CRYPT_ERROR )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_REQUEST,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}
		}

	/* Connect to the remote server */
	status = initSessionNetConnectInfo( sessionInfoPtr, &connectInfo );
	if( cryptStatusError( status ) )
		return( status );
	return( sNetConnect( &sessionInfoPtr->stream, STREAM_PROTOCOL_HTTP, 
						 &connectInfo, &sessionInfoPtr->errorInfo ) );
	}

/* Exchange data with a CMP server.  Since the plug-and-play PKI client 
   performs multiple transactions, we wrap the basic clientTransact() in an 
   external function that either calls it indirectly when required from the 
   PnP code or just passes the call through to the transaction function */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int clientTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	CMP_INFO *cmpInfo = sessionInfoPtr->sessionCMP;
	CMP_PROTOCOL_INFO protocolInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Check that everything we need is present.  If it's a general CMP 
	   session this will already have been checked in clientStartup(), but
	   if it's coming from the PnPPKI wrapper it doesn't go through the
	   startup checks each time so we double-check here */
	REQUIRES( cmpInfo->requestType != CRYPT_REQUESTTYPE_NONE );
	REQUIRES( cmpInfo->requestType == CRYPT_REQUESTTYPE_PKIBOOT || \
			  sessionInfoPtr->iCertRequest != CRYPT_ERROR );
	REQUIRES( cmpInfo->requestType == CRYPT_REQUESTTYPE_PKIBOOT || \
			  sessionInfoPtr->iAuthInContext != CRYPT_ERROR );

	/* Initialise the client-side protocol state information */
	initCMPprotocolInfo( &protocolInfo, 
					     ( sessionInfoPtr->flags & SESSION_ISCRYPTLIB ) ? \
						 TRUE : FALSE, FALSE );
	status = initClientInfo( sessionInfoPtr, &protocolInfo );
	if( cryptStatusError( status ) )
		{
		destroyCMPprotocolInfo( &protocolInfo );
		return( status );
		}

	/* Write the message into the session buffer and send it to the server */
	status = writePkiMessage( sessionInfoPtr, &protocolInfo, 
							  ( cmpInfo->requestType == \
									CRYPT_REQUESTTYPE_PKIBOOT ) ? \
							  CMPBODY_GENMSG : CMPBODY_NORMAL );
	if( cryptStatusOK( status ) )
		{
		DEBUG_DUMP_CMP( protocolInfo.operation, 1, sessionInfoPtr );
		if( ( protocolInfo.operation == CTAG_PB_GENM || \
			  protocolInfo.operation == CTAG_PB_RR ) && \
			!( sessionInfoPtr->protocolFlags & CMP_PFLAG_RETAINCONNECTION ) )
			{
			/* There's no confirmation handshake for PKIBoot or a revocation 
			   request so we mark this as the last message if required */
			sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_LASTMESSAGE, 
					   TRUE );
			}
		status = writePkiDatagram( sessionInfoPtr, CMP_CONTENT_TYPE, 
								   CMP_CONTENT_TYPE_LEN );
		}
	if( cryptStatusError( status ) )
		{
		destroyCMPprotocolInfo( &protocolInfo );
		return( status );
		}

	/* Read the server response */
	status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusOK( status ) )
		{
		const int responseType = reqToResp( protocolInfo.operation );

		DEBUG_DUMP_CMP( protocolInfo.operation, 2, sessionInfoPtr );
		if( cryptStatusError( responseType ) )
			status = responseType;
		else
			{
			status = readPkiMessage( sessionInfoPtr, &protocolInfo, 
									 responseType );
			}
		}
	if( cryptStatusOK( status ) && protocolInfo.operation == CTAG_PB_GENM )
		{
		/* It's a PKIBoot, add the trusted certificates.  If the user wants 
		   the setting made permanent then they need to flush the 
		   configuration to disk after the session has completed */
		status = krnlSendMessage( sessionInfoPtr->ownerHandle,
								  IMESSAGE_SETATTRIBUTE, 
								  &sessionInfoPtr->iCertResponse,
								  CRYPT_IATTRIBUTE_CTL );
		if( status == CRYPT_ERROR_INITED )
			{
			/* If the certificates are already present, trying to add them 
			   again isn't an error */
			status = CRYPT_OK;
			}
		}
	if( cryptStatusError( status ) )
		{
		destroyCMPprotocolInfo( &protocolInfo );
		return( status );
		}

	/* If it's a transaction type that doesn't need a confirmation, we're 
	   done */
	if( protocolInfo.operation == CTAG_PB_GENM || \
		protocolInfo.operation == CTAG_PB_RR )
		{
		/* Remember the authentication context in case we can reuse it for 
		   another transaction */
		if( protocolInfo.iMacContext != CRYPT_ERROR )
			{
			cmpInfo->savedMacContext = protocolInfo.iMacContext;
			protocolInfo.iMacContext = CRYPT_ERROR;
			}

		destroyCMPprotocolInfo( &protocolInfo );
		return( CRYPT_OK );
		}

	/* Exchange confirmation data with the server */
	if( !( sessionInfoPtr->protocolFlags & CMP_PFLAG_RETAINCONNECTION ) )
		sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_LASTMESSAGE, TRUE );
	status = writePkiMessage( sessionInfoPtr, &protocolInfo,
							  CMPBODY_CONFIRMATION );
	if( cryptStatusOK( status ) )
		{
		DEBUG_DUMP_CMP( protocolInfo.operation, 3, sessionInfoPtr );
		status = writePkiDatagram( sessionInfoPtr, CMP_CONTENT_TYPE, 
								   CMP_CONTENT_TYPE_LEN );
		}
	if( cryptStatusOK( status ) )
		status = readPkiDatagram( sessionInfoPtr );
	if( cryptStatusOK( status ) )
		{
		DEBUG_DUMP_CMP( protocolInfo.operation, 4, sessionInfoPtr );
		status = readPkiMessage( sessionInfoPtr, &protocolInfo, CTAG_PB_PKICONF );
		}
	if( cryptStatusOK( status ) && protocolInfo.iMacContext != CRYPT_ERROR )
		{
		/* Remember the authentication context in case we can reuse it for 
		   another transaction */
		cmpInfo->savedMacContext = protocolInfo.iMacContext;
		protocolInfo.iMacContext = CRYPT_ERROR;
		}
	destroyCMPprotocolInfo( &protocolInfo );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int clientTransactWrapper( INOUT SESSION_INFO *sessionInfoPtr )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* If it's not a plug-and-play PKI session, just pass the call on down
	   to the client transaction function */
	if( !( sessionInfoPtr->sessionCMP->flags & CMP_PFLAG_PNPPKI ) )
		return( clientTransact( sessionInfoPtr ) );

	/* We're doing plug-and-play PKI, point the transaction function at the 
	   client-transact function while we execute the PnP steps, then reset 
	   it back to the PnP wrapper after we're done */
	sessionInfoPtr->transactFunction = clientTransact;
	status = pnpPkiSession( sessionInfoPtr );
	sessionInfoPtr->transactFunction = clientTransactWrapper;
	return( status );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initCMPclientProcessing( SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	sessionInfoPtr->connectFunction = clientStartup;
	sessionInfoPtr->transactFunction = clientTransactWrapper;
	}
#endif /* USE_CMP */
