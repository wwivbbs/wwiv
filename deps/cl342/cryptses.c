/****************************************************************************
*																			*
*						cryptlib Secure Session Routines					*
*						Copyright Peter Gutmann 1998-2005					*
*																			*
****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include "crypt.h"
#ifdef INC_ALL
  #include "asn1.h"
  #include "stream.h"
  #include "session.h"
  #include "scorebrd.h"
#else
  #include "enc_dec/asn1.h"
  #include "io/stream.h"
  #include "session/session.h"
  #include "session/scorebrd.h"
#endif /* Compiler-specific includes */

/* The number of entries in the SSL session cache.  Note that when increasing
   the SESSIONCACHE_SIZE value to more than about 256 you need to also change 
   MAX_ALLOC_SIZE in kernel/sec_mem.c to allow the allocation of such large 
   amounts of secure memory */

#if defined( CONFIG_CONSERVE_MEMORY )
  #define SESSIONCACHE_SIZE			8
#else
  #define SESSIONCACHE_SIZE			64
#endif /* CONFIG_CONSERVE_MEMORY */

static SCOREBOARD_STATE scoreboardInfo;

#ifdef USE_SESSIONS

/****************************************************************************
*																			*
*								Session Message Handler						*
*																			*
****************************************************************************/

/* Handle a message sent to a session object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sessionMessageFunction( INOUT TYPECAST( CONTEXT_INFO * ) \
									void *objectInfoPtr,
								   IN_MESSAGE const MESSAGE_TYPE message,
								   void *messageDataPtr,
								   IN_INT_Z const int messageValue )
	{
	SESSION_INFO *sessionInfoPtr = ( SESSION_INFO * ) objectInfoPtr;
	int status;

	assert( isWritePtr( objectInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( message > MESSAGE_NONE && message < MESSAGE_LAST );
	REQUIRES( messageValue >= 0 && messageValue < MAX_INTLENGTH );

	/* Process destroy object messages */
	if( message == MESSAGE_DESTROY )
		{
		/* Shut down the session if required.  Nemo nisi mors */
		if( sessionInfoPtr->flags & SESSION_ISOPEN )
			{
			sessionInfoPtr->flags |= SESSION_ISCLOSINGDOWN;
			sessionInfoPtr->shutdownFunction( sessionInfoPtr );
			}

		/* Clear and free session state information if necessary */
		if( sessionInfoPtr->sendBuffer != NULL )
			{
			zeroise( sessionInfoPtr->sendBuffer,
					 sessionInfoPtr->sendBufSize );
			clFree( "sessionMessageFunction", sessionInfoPtr->sendBuffer );
			}
		if( sessionInfoPtr->receiveBuffer != NULL )
			{
			zeroise( sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufSize );
			clFree( "sessionMessageFunction", sessionInfoPtr->receiveBuffer );
			}

		/* Clear session attributes if necessary */
		if( sessionInfoPtr->attributeList != NULL )
			deleteSessionInfoAll( &sessionInfoPtr->attributeList,
								  &sessionInfoPtr->attributeListCurrent );

		/* Clean up any session-related objects if necessary */
		if( sessionInfoPtr->iKeyexCryptContext != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->iKeyexCryptContext,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->iKeyexAuthContext != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->iKeyexAuthContext,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->iCryptInContext != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->iCryptInContext,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->iCryptOutContext != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->iCryptOutContext,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->iAuthInContext != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->iAuthInContext,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->iAuthOutContext != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->iAuthOutContext,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->iCertRequest != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->iCertRequest,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->iCertResponse != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->iCertResponse,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->privateKey != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->privateKey,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->cryptKeyset != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->privKeyset != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->privKeyset,
							  IMESSAGE_DECREFCOUNT );
		if( sessionInfoPtr->transportSession != CRYPT_ERROR )
			krnlSendNotifier( sessionInfoPtr->transportSession,
							  IMESSAGE_DECREFCOUNT );

		return( CRYPT_OK );
		}

	/* Process attribute get/set/delete messages */
	if( isAttributeMessage( message ) )
		{
		/* If it's a protocol-specific attribute, forward it directly to
		   the low-level code */
		if( message != MESSAGE_DELETEATTRIBUTE && \
			( ( messageValue >= CRYPT_SESSINFO_FIRST_SPECIFIC && \
				messageValue <= CRYPT_SESSINFO_LAST_SPECIFIC ) || \
			  messageValue == CRYPT_IATTRIBUTE_ENC_TIMESTAMP ) )
			{
			if( message == MESSAGE_SETATTRIBUTE || \
				message == MESSAGE_SETATTRIBUTE_S )
				{
				REQUIRES( sessionInfoPtr->setAttributeFunction != NULL );

				status = sessionInfoPtr->setAttributeFunction( sessionInfoPtr,
											messageDataPtr, messageValue );
				if( status == CRYPT_ERROR_INITED )
					{
					setErrorInfo( sessionInfoPtr, messageValue, 
								  CRYPT_ERRTYPE_ATTR_PRESENT );
					return( CRYPT_ERROR_INITED );
					}
				}
			else
				{
				REQUIRES( message == MESSAGE_GETATTRIBUTE || \
						  message == MESSAGE_GETATTRIBUTE_S );
				REQUIRES( sessionInfoPtr->getAttributeFunction != NULL );

				status = sessionInfoPtr->getAttributeFunction( sessionInfoPtr,
											messageDataPtr, messageValue );
				if( status == CRYPT_ERROR_NOTFOUND )
					{
					setErrorInfo( sessionInfoPtr, messageValue, 
								  CRYPT_ERRTYPE_ATTR_ABSENT );
					return( CRYPT_ERROR_NOTFOUND );
					}
				}
			return( status );
			}

		if( message == MESSAGE_GETATTRIBUTE )
			return( getSessionAttribute( sessionInfoPtr, 
										 ( int * ) messageDataPtr,
										 messageValue ) );
		if( message == MESSAGE_GETATTRIBUTE_S )
			return( getSessionAttributeS( sessionInfoPtr, 
										  ( MESSAGE_DATA * ) messageDataPtr,
										  messageValue ) );
		if( message == MESSAGE_SETATTRIBUTE )
			{
			/* CRYPT_IATTRIBUTE_INITIALISED is purely a notification message 
			   with no parameters so we don't pass it down to the attribute-
			   handling code */
			if( messageValue == CRYPT_IATTRIBUTE_INITIALISED )
				return( CRYPT_OK );

			return( setSessionAttribute( sessionInfoPtr, 
										 *( ( int * ) messageDataPtr ),
										 messageValue ) );
			}
		if( message == MESSAGE_SETATTRIBUTE_S )
			{
			const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

			return( setSessionAttributeS( sessionInfoPtr, msgData->data, 
										  msgData->length, messageValue ) );
			}
		if( message == MESSAGE_DELETEATTRIBUTE )
			return( deleteSessionAttribute( sessionInfoPtr, messageValue ) );

		retIntError();
		}

	/* Process object-specific messages */
	if( message == MESSAGE_ENV_PUSHDATA )
		{
		MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;
		const int length = msgData->length;
		int bytesCopied;

		/* Unless we're told otherwise, we've copied zero bytes */
		msgData->length = 0;

		/* If the session isn't open yet, perform an implicit open */
		if( !( sessionInfoPtr->flags & SESSION_ISOPEN ) )
			{
			status = krnlSendMessage( sessionInfoPtr->objectHandle, 
									  IMESSAGE_SETATTRIBUTE, MESSAGE_VALUE_TRUE,
									  CRYPT_SESSINFO_ACTIVE );
			if( cryptStatusError( status ) )
				return( status );

			/* The session is ready to process data, move it into the high
			   state */
			status = krnlSendMessage( sessionInfoPtr->objectHandle, 
									  IMESSAGE_SETATTRIBUTE, 
									  MESSAGE_VALUE_UNUSED, 
									  CRYPT_IATTRIBUTE_INITIALISED );
			if( cryptStatusError( status ) )
				return( status );
			}
		ENSURES( sessionInfoPtr->flags & SESSION_ISOPEN );
		ENSURES( sessionInfoPtr->sendBuffer != NULL );

		/* Make sure that everything is in order */
		if( sessionInfoPtr->flags & SESSION_SENDCLOSED )
			{
			/* If the other side has closed its receive channel (which is 
			   our send channel), we can't send any more data, although we 
			   can still get data on our receive channel if we haven't closed
			   it as well.  The closing of the other side's send channel is 
			   detected during a read and isn't a write error but a normal 
			   state change in the channel so we don't treat it as an error 
			   when it's seen at the read stage until the caller actually 
			   tries to write data to the closed channel */
			sessionInfoPtr->writeErrorState = CRYPT_ERROR_COMPLETE;
			}
		if( sessionInfoPtr->writeErrorState != CRYPT_OK )
			return( sessionInfoPtr->writeErrorState );

		/* Write the data */
		clearErrorInfo( sessionInfoPtr );
		status = putSessionData( sessionInfoPtr, msgData->data, length, 
								 &bytesCopied );
		if( cryptStatusOK( status ) )
			msgData->length = bytesCopied;
		ENSURES( ( cryptStatusError( status ) && bytesCopied == 0 ) || \
				 ( cryptStatusOK( status ) && bytesCopied >= 0 ) );
		return( status );
		}
	if( message == MESSAGE_ENV_POPDATA )
		{
		MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;
		const int length = msgData->length;
		int bytesCopied;

		/* Unless we're told otherwise, we've copied zero bytes */
		msgData->length = 0;

		/* If the session isn't open, there's nothing to pop */
		if( !( sessionInfoPtr->flags & SESSION_ISOPEN ) )
			return( CRYPT_ERROR_NOTINITED );

		ENSURES( sessionInfoPtr->flags & SESSION_ISOPEN );
		ENSURES( sessionInfoPtr->receiveBuffer != NULL );

		/* Make sure that everything is in order */
		if( sessionInfoPtr->readErrorState != CRYPT_OK )
			return( sessionInfoPtr->readErrorState );

		/* Read the data */
		clearErrorInfo( sessionInfoPtr );
		status = getSessionData( sessionInfoPtr, msgData->data, length,
								 &bytesCopied );
		if( cryptStatusOK( status ) )
			msgData->length = bytesCopied;
		ENSURES( ( cryptStatusError( status ) && bytesCopied == 0 ) || \
				 ( cryptStatusOK( status ) && bytesCopied >= 0 ) );
		return( status );
		}

	retIntError();
	}

/* Open a session.  This is a low-level function encapsulated by createSession()
   and used to manage error exits */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int openSession( OUT_HANDLE_OPT CRYPT_SESSION *iCryptSession,
						IN_HANDLE const CRYPT_USER iCryptOwner,
						IN_ENUM( CRYPT_SESSION ) \
							const CRYPT_SESSION_TYPE sessionType,
						OUT_OPT_PTR SESSION_INFO **sessionInfoPtrPtr )
	{
	CRYPT_SESSION_TYPE sessionBaseType;
	SESSION_INFO *sessionInfoPtr;
	const PROTOCOL_INFO *protocolInfoPtr;
	static const MAP_TABLE subtypeMapTbl[] = {
		{ CRYPT_SESSION_SSH, SUBTYPE_SESSION_SSH },
		{ CRYPT_SESSION_SSH_SERVER, SUBTYPE_SESSION_SSH_SVR },
		{ CRYPT_SESSION_SSL, SUBTYPE_SESSION_SSL },
		{ CRYPT_SESSION_SSL_SERVER, SUBTYPE_SESSION_SSL_SVR },
		{ CRYPT_SESSION_RTCS, SUBTYPE_SESSION_RTCS },
		{ CRYPT_SESSION_RTCS_SERVER, SUBTYPE_SESSION_RTCS_SVR },
		{ CRYPT_SESSION_OCSP, SUBTYPE_SESSION_OCSP },
		{ CRYPT_SESSION_OCSP_SERVER, SUBTYPE_SESSION_OCSP_SVR },
		{ CRYPT_SESSION_TSP, SUBTYPE_SESSION_TSP },
		{ CRYPT_SESSION_TSP_SERVER, SUBTYPE_SESSION_TSP_SVR },
		{ CRYPT_SESSION_CMP, SUBTYPE_SESSION_CMP },
		{ CRYPT_SESSION_CMP_SERVER, SUBTYPE_SESSION_CMP_SVR },
		{ CRYPT_SESSION_SCEP, SUBTYPE_SESSION_SCEP },
		{ CRYPT_SESSION_SCEP_SERVER, SUBTYPE_SESSION_SCEP_SVR },
		{ CRYPT_SESSION_CERTSTORE_SERVER, SUBTYPE_SESSION_CERT_SVR },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	static const MAP_TABLE basetypeMapTbl[] = {
		{ CRYPT_SESSION_SSH, CRYPT_SESSION_SSH },
		{ CRYPT_SESSION_SSH_SERVER, CRYPT_SESSION_SSH },
		{ CRYPT_SESSION_SSL, CRYPT_SESSION_SSL },
		{ CRYPT_SESSION_SSL_SERVER, CRYPT_SESSION_SSL },
		{ CRYPT_SESSION_RTCS, CRYPT_SESSION_RTCS },
		{ CRYPT_SESSION_RTCS_SERVER, CRYPT_SESSION_RTCS },
		{ CRYPT_SESSION_OCSP, CRYPT_SESSION_OCSP },
		{ CRYPT_SESSION_OCSP_SERVER, CRYPT_SESSION_OCSP },
		{ CRYPT_SESSION_TSP, CRYPT_SESSION_TSP },
		{ CRYPT_SESSION_TSP_SERVER, CRYPT_SESSION_TSP },
		{ CRYPT_SESSION_CMP, CRYPT_SESSION_CMP },
		{ CRYPT_SESSION_CMP_SERVER, CRYPT_SESSION_CMP },
		{ CRYPT_SESSION_SCEP, CRYPT_SESSION_SCEP },
		{ CRYPT_SESSION_SCEP_SERVER, CRYPT_SESSION_SCEP },
		{ CRYPT_SESSION_CERTSTORE_SERVER, CRYPT_SESSION_CERTSTORE_SERVER },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	OBJECT_SUBTYPE subType;
	int value, storageSize, status;

	assert( isWritePtr( iCryptSession, sizeof( CRYPT_SESSION * ) ) );
	assert( isWritePtr( sessionInfoPtrPtr, sizeof( SESSION_INFO * ) ) );

	REQUIRES( ( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE ) || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( sessionType > CRYPT_SESSION_NONE && \
			  sessionType < CRYPT_SESSION_LAST );

	/* Clear return values */
	*iCryptSession = CRYPT_ERROR;
	*sessionInfoPtrPtr = NULL;

	/* Map the external session type to a base type and internal object
	   subtype */
	status = mapValue( sessionType, &value, subtypeMapTbl, 
					   FAILSAFE_ARRAYSIZE( subtypeMapTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	subType = value;
	status = mapValue( sessionType, &value, basetypeMapTbl, 
					   FAILSAFE_ARRAYSIZE( basetypeMapTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	sessionBaseType = value;

	/* Set up subtype-specific information */
	switch( sessionBaseType )
		{
		case CRYPT_SESSION_SSH:
			storageSize = sizeof( SSH_INFO );
			break;

		case CRYPT_SESSION_SSL:
			storageSize = sizeof( SSL_INFO );
			break;

		case CRYPT_SESSION_TSP:
			storageSize = sizeof( TSP_INFO );
			break;

		case CRYPT_SESSION_CMP:
			storageSize = sizeof( CMP_INFO );
			break;

		case CRYPT_SESSION_SCEP:
			storageSize = sizeof( SCEP_INFO );
			break;
		
		case CRYPT_SESSION_RTCS:
		case CRYPT_SESSION_OCSP:
		case CRYPT_SESSION_CERTSTORE_SERVER:
			storageSize = 0;
			break;

		default:
			retIntError();
		}

	/* Create the session object */
	status = krnlCreateObject( iCryptSession, ( void ** ) &sessionInfoPtr, 
							   sizeof( SESSION_INFO ) + storageSize, 
							   OBJECT_TYPE_SESSION, subType, 
							   CREATEOBJECT_FLAG_NONE, iCryptOwner, 
							   ACTION_PERM_NONE_ALL, sessionMessageFunction );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( sessionInfoPtr != NULL );
	*sessionInfoPtrPtr = sessionInfoPtr;
	sessionInfoPtr->objectHandle = *iCryptSession;
	sessionInfoPtr->ownerHandle = iCryptOwner;
	sessionInfoPtr->type = sessionBaseType;
	if( storageSize > 0 )
		{
		switch( sessionBaseType )
			{
			case CRYPT_SESSION_SSH:
				sessionInfoPtr->sessionSSH = \
								( SSH_INFO * ) sessionInfoPtr->storage;
				break;

			case CRYPT_SESSION_SSL:
				sessionInfoPtr->sessionSSL = \
								( SSL_INFO * ) sessionInfoPtr->storage;
				break;

			case CRYPT_SESSION_TSP:
				sessionInfoPtr->sessionTSP = \
								( TSP_INFO * ) sessionInfoPtr->storage;
				break;

			case CRYPT_SESSION_CMP:
				sessionInfoPtr->sessionCMP = \
								( CMP_INFO * ) sessionInfoPtr->storage;
				break;

			case CRYPT_SESSION_SCEP:
				sessionInfoPtr->sessionSCEP = \
								( SCEP_INFO * ) sessionInfoPtr->storage;
				break;

			default:
				retIntError();
			}
		}
	sessionInfoPtr->storageSize = storageSize;

	/* If it's a server session, mark it as such.  An HTTP certstore session 
	   is a special case in that it's always a server session */
	if( ( sessionType != sessionBaseType ) || \
		( sessionType == CRYPT_SESSION_CERTSTORE_SERVER ) )
		sessionInfoPtr->flags = SESSION_ISSERVER;

	/* Set up any internal objects to contain invalid handles */
	sessionInfoPtr->iKeyexCryptContext = \
		sessionInfoPtr->iKeyexAuthContext = CRYPT_ERROR;
	sessionInfoPtr->iCryptInContext = \
		sessionInfoPtr->iCryptOutContext = CRYPT_ERROR;
	sessionInfoPtr->iAuthInContext = \
		sessionInfoPtr->iAuthOutContext = CRYPT_ERROR;
	sessionInfoPtr->iCertRequest = \
		sessionInfoPtr->iCertResponse = CRYPT_ERROR;
	sessionInfoPtr->privateKey = CRYPT_ERROR;
	sessionInfoPtr->cryptKeyset = CRYPT_ERROR;
	sessionInfoPtr->privKeyset =  CRYPT_ERROR;
	sessionInfoPtr->transportSession = CRYPT_ERROR;
	sessionInfoPtr->networkSocket = CRYPT_ERROR;
	sessionInfoPtr->readTimeout = \
		sessionInfoPtr->writeTimeout = \
			sessionInfoPtr->connectTimeout = CRYPT_ERROR;

	/* Set up any additinal values */
	sessionInfoPtr->authResponse = CRYPT_UNUSED;

	/* Set up the access information for the session and initialise it */
	switch( sessionBaseType )
		{
		case CRYPT_SESSION_CERTSTORE_SERVER:
			status = setAccessMethodCertstore( sessionInfoPtr );
			break;

		case CRYPT_SESSION_CMP:
			status = setAccessMethodCMP( sessionInfoPtr );
			break;

		case CRYPT_SESSION_RTCS:
			status = setAccessMethodRTCS( sessionInfoPtr );
			break;

		case CRYPT_SESSION_OCSP:
			status = setAccessMethodOCSP( sessionInfoPtr );
			break;

		case CRYPT_SESSION_SCEP:
			status = setAccessMethodSCEP( sessionInfoPtr );
			break;

		case CRYPT_SESSION_SSH:
			status = setAccessMethodSSH( sessionInfoPtr );
			break;

		case CRYPT_SESSION_SSL:
			status = setAccessMethodSSL( sessionInfoPtr );
			break;

		case CRYPT_SESSION_TSP:
			status = setAccessMethodTSP( sessionInfoPtr );
			break;

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a session type that uses the scoreboard, set up the 
	   scoreboard information for the session */
	if( sessionType == CRYPT_SESSION_SSL || \
		sessionType == CRYPT_SESSION_SSL_SERVER )
		sessionInfoPtr->sessionSSL->scoreboardInfoPtr = &scoreboardInfo;

	/* Check that the protocol info is OK */
	protocolInfoPtr = sessionInfoPtr->protocolInfo;
	ENSURES( ( protocolInfoPtr->isReqResp && \
			   protocolInfoPtr->bufSize == 0 && \
			   protocolInfoPtr->sendBufStartOfs == 0 && \
			   protocolInfoPtr->maxPacketSize == 0 ) || 
			 ( !protocolInfoPtr->isReqResp && \
			   protocolInfoPtr->bufSize >= MIN_BUFFER_SIZE && \
			   protocolInfoPtr->bufSize < MAX_INTLENGTH && \
			   protocolInfoPtr->sendBufStartOfs >= 5 && 
			   protocolInfoPtr->sendBufStartOfs < protocolInfoPtr->maxPacketSize && \
			   protocolInfoPtr->maxPacketSize <= protocolInfoPtr->bufSize ) );
	ENSURES( ( ( protocolInfoPtr->flags & SESSION_ISHTTPTRANSPORT ) && \
			   protocolInfoPtr->port == 80 ) || \
			 ( protocolInfoPtr->port != 80 ) );
	ENSURES( protocolInfoPtr->port > 21 && protocolInfoPtr->port <= 65534L );
	ENSURES( protocolInfoPtr->version >= 0 && protocolInfoPtr->version < 5 );

	/* Copy mutable protocol-specific information into the session info */
	sessionInfoPtr->flags |= protocolInfoPtr->flags;
	sessionInfoPtr->clientReqAttrFlags = protocolInfoPtr->clientReqAttrFlags;
	sessionInfoPtr->serverReqAttrFlags = protocolInfoPtr->serverReqAttrFlags;
	sessionInfoPtr->version = protocolInfoPtr->version;
	if( protocolInfoPtr->isReqResp )
		{
		sessionInfoPtr->sendBufSize = CRYPT_UNUSED;
		sessionInfoPtr->receiveBufSize = MIN_BUFFER_SIZE;
		}
	else
		{
		sessionInfoPtr->sendBufSize = sessionInfoPtr->receiveBufSize = \
				protocolInfoPtr->bufSize;
		sessionInfoPtr->sendBufStartOfs = sessionInfoPtr->receiveBufStartOfs = \
				protocolInfoPtr->sendBufStartOfs;
		sessionInfoPtr->maxPacketSize = protocolInfoPtr->maxPacketSize;
		}

	/* Install default handlers if no session-specific ones are provided */
	status = initSessionIO( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* Check that the handlers are all OK */
	ENSURES( sessionInfoPtr->connectFunction != NULL );
	ENSURES( sessionInfoPtr->transactFunction != NULL );
	ENSURES( ( protocolInfoPtr->isReqResp && \
			   sessionInfoPtr->readHeaderFunction == NULL && \
			   sessionInfoPtr->processBodyFunction == NULL && \
			   sessionInfoPtr->preparePacketFunction == NULL ) || \
			 ( !protocolInfoPtr->isReqResp && \
			   sessionInfoPtr->readHeaderFunction != NULL && \
			   sessionInfoPtr->processBodyFunction != NULL && \
			   sessionInfoPtr->preparePacketFunction != NULL ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createSession( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				   STDC_UNUSED const void *auxDataPtr, 
				   STDC_UNUSED const int auxValue )
	{
	CRYPT_SESSION iCryptSession;
	SESSION_INFO *sessionInfoPtr = NULL;
	int initStatus, status;

	assert( isWritePtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );

	REQUIRES( auxDataPtr == NULL && auxValue == 0 );
	REQUIRES( createInfo->arg1 > CRYPT_SESSION_NONE && \
			  createInfo->arg1 < CRYPT_SESSION_LAST );

	/* Pass the call on to the lower-level open function */
	initStatus = openSession( &iCryptSession, createInfo->cryptOwner,
							  createInfo->arg1, &sessionInfoPtr );
	if( cryptStatusError( initStatus ) )
		{
		/* If the create object failed, return immediately */
		if( sessionInfoPtr == NULL )
			return( initStatus );

		/* The init failed, make sure that the object gets destroyed when we 
		   notify the kernel that the setup process is complete */
		krnlSendNotifier( iCryptSession, IMESSAGE_DESTROY );
		}

	/* We've finished setting up the object-type-specific info, tell the
	   kernel that the object is ready for use */
	status = krnlSendMessage( iCryptSession, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusError( initStatus ) || cryptStatusError( status ) )
		return( cryptStatusError( initStatus ) ? initStatus : status );
	createInfo->cryptHandle = iCryptSession;

	return( CRYPT_OK );
	}

/* Generic management function for this class of object */

CHECK_RETVAL \
int sessionManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action )
	{
	static int initLevel = 0;
	int status;

	REQUIRES( action == MANAGEMENT_ACTION_INIT || \
			  action == MANAGEMENT_ACTION_PRE_SHUTDOWN || \
			  action == MANAGEMENT_ACTION_SHUTDOWN );

	switch( action )
		{
		case MANAGEMENT_ACTION_INIT:
			status = netInitTCP();
			if( cryptStatusOK( status ) )
				{
				initLevel++;
				if( krnlIsExiting() )
					{
					/* The kernel is shutting down, exit */
					return( CRYPT_ERROR_PERMISSION );
					}
				status = initScoreboard( &scoreboardInfo, 
										 SESSIONCACHE_SIZE );
				}
			if( cryptStatusOK( status ) )
				initLevel++;
			return( status );

		case MANAGEMENT_ACTION_PRE_SHUTDOWN:
			/* We have to wait for the driver binding to complete before we
			   can start the shutdown process */
			if( !krnlWaitSemaphore( SEMAPHORE_DRIVERBIND ) )
				{
				/* The kernel is shutting down, bail out */
				DEBUG_DIAG(( "Exiting due to kernel shutdown" ));
				assert( DEBUG_WARN );
				return( CRYPT_ERROR_PERMISSION );
				}
			if( initLevel > 0 )
				netSignalShutdown();
			return( CRYPT_OK );

		case MANAGEMENT_ACTION_SHUTDOWN:
			if( initLevel > 1 )
				endScoreboard( &scoreboardInfo );
			if( initLevel > 0 )
				netEndTCP();
			initLevel = 0;

			return( CRYPT_OK );
		}

	retIntError();
	}
#endif /* USE_SESSIONS */
