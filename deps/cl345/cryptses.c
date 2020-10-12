/****************************************************************************
*																			*
*						cryptlib Secure Session Routines					*
*						Copyright Peter Gutmann 1998-2015					*
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

#ifdef USE_SESSIONS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check overall session data.  This is a general check for 
   reasonable values that's more targeted at catching inadvertent memory 
   corruption than a strict sanity check, protocol-specific sanity checks are
   performed by individual modules */

CHECK_RETVAL_BOOL \
static BOOLEAN checkObjectHandle( const CRYPT_HANDLE cryptHandle )
	{
	return( ( cryptHandle == CRYPT_ERROR || \
			  isHandleRangeValid( cryptHandle ) ) ? TRUE : FALSE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckSession( const SESSION_INFO *sessionInfoPtr )
	{
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Check general session data */
	if( !isEnumRange( sessionInfoPtr->type, CRYPT_SESSION ) || \
		sessionInfoPtr->version < 0 || sessionInfoPtr->version > 5 
#if defined( USE_WEBSOCKETS ) || defined( USE_EAP )
		|| !isEnumRangeOpt( sessionInfoPtr->subProtocol, CRYPT_SUBPROTOCOL ) 
#endif /* USE_WEBSOCKETS || USE_EAP */
		)
		{
		DEBUG_PUTS(( "sanityCheckSession: General info" ));
		return( FALSE );
		}
	if( !CHECK_FLAGS( sessionInfoPtr->flags, 
					  SESSION_FLAG_NONE, SESSION_FLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckSession: Flags" ));
		return( FALSE );
		}
	if( sessionInfoPtr->cryptAlgo != CRYPT_ALGO_NONE && \
		!isConvAlgo( sessionInfoPtr->cryptAlgo ) )
		{
		DEBUG_PUTS(( "sanityCheckSession: Crypt algorithm" ));
		return( FALSE );
		}
	if( sessionInfoPtr->integrityAlgo != CRYPT_ALGO_NONE && \
		!isHashAlgo( sessionInfoPtr->integrityAlgo ) && \
		!isMacAlgo( sessionInfoPtr->integrityAlgo ) )
		{
		DEBUG_PUTS(( "sanityCheckSession: Integrity algorithm" ));
		return( FALSE );
		}
	if( !isEnumRangeOpt( sessionInfoPtr->authResponse, AUTHRESPONSE ) )
		{
		DEBUG_PUTS(( "sanityCheckSession: Auth response" ));
		return( FALSE );
		}
	if( sessionInfoPtr->clientReqAttrFlags < SESSION_NEEDS_NONE || \
		sessionInfoPtr->clientReqAttrFlags > SESSION_NEEDS_MAX || \
		sessionInfoPtr->serverReqAttrFlags < SESSION_NEEDS_NONE || \
		sessionInfoPtr->serverReqAttrFlags > SESSION_NEEDS_MAX )
		{
		DEBUG_PUTS(( "sanityCheckSession: Client/server attribute flags" ));
		return( FALSE );
		}

	/* Check safe pointers.  We don't have to check the function pointers
	   because they're validated each time they're dereferenced */
	if( !DATAPTR_ISVALID( sessionInfoPtr->protocolInfo ) || \
		!DATAPTR_ISVALID( sessionInfoPtr->attributeList ) || \
		!DATAPTR_ISVALID( sessionInfoPtr->attributeListCurrent ) )
		{
		DEBUG_PUTS(( "sanityCheckSession: Attribute list" ));
		return( FALSE );
		}

	/* Check data buffer information */
	if( !sanityCheckSessionRead( sessionInfoPtr ) || \
		!sanityCheckSessionWrite( sessionInfoPtr ) )
		{
		DEBUG_PUTS(( "sanityCheckSession: Buffer information" ));
		return( FALSE );
		}

	/* Check crypto information */
	if( !checkObjectHandle( sessionInfoPtr->iKeyexCryptContext ) || \
		!checkObjectHandle( sessionInfoPtr->iKeyexAuthContext ) || \
		!checkObjectHandle( sessionInfoPtr->iCryptInContext ) || \
		!checkObjectHandle( sessionInfoPtr->iCryptOutContext ) || \
		!checkObjectHandle( sessionInfoPtr->iAuthInContext ) || \
		!checkObjectHandle( sessionInfoPtr->iAuthOutContext ) || \
		!checkObjectHandle( sessionInfoPtr->iCertRequest ) || \
		!checkObjectHandle( sessionInfoPtr->iCertResponse ) || \
		!checkObjectHandle( sessionInfoPtr->privateKey ) || \
		!checkObjectHandle( sessionInfoPtr->cryptKeyset ) || \
		!checkObjectHandle( sessionInfoPtr->privKeyset ) )
		{
		DEBUG_PUTS(( "sanityCheckSession: Crypto object handles" ));
		return( FALSE );
		}
	if( ( sessionInfoPtr->cryptBlocksize != 0 && \
		  sessionInfoPtr->cryptBlocksize != 1 &&		/* Stream cipher */ 
		  ( sessionInfoPtr->cryptBlocksize < MIN_IVSIZE || \
			sessionInfoPtr->cryptBlocksize > CRYPT_MAX_IVSIZE ) ) || \
		( sessionInfoPtr->authBlocksize != 0 && \
		  ( sessionInfoPtr->authBlocksize < MIN_HASHSIZE || \
			sessionInfoPtr->authBlocksize > CRYPT_MAX_HASHSIZE ) ) )
		{
		DEBUG_PUTS(( "sanityCheckSession: Crypt/auth block size" ));
		return( FALSE );
		}

	/* Check network information */
	if( TEST_FLAG( sessionInfoPtr->flags,
				   SESSION_FLAG_PARTIALOPEN | SESSION_FLAG_ISOPEN ) )
		{
		/* Before the session is activated, one or more of these can be set
		   to CRYPT_ERROR to indicate that they're not set yet */
		if( sessionInfoPtr->readTimeout < 0 || \
			sessionInfoPtr->readTimeout > MAX_NETWORK_TIMEOUT || \
			sessionInfoPtr->writeTimeout < 0 || \
			sessionInfoPtr->writeTimeout > MAX_NETWORK_TIMEOUT || \
			sessionInfoPtr->connectTimeout < 5 || \
			sessionInfoPtr->connectTimeout > MAX_NETWORK_TIMEOUT )
			{
			DEBUG_PUTS(( "sanityCheckSession: Timeouts" ));
			return( FALSE );
			}
		}

	/* Check associated handles */
	if( !isHandleRangeValid( sessionInfoPtr->objectHandle ) || \
		!( sessionInfoPtr->ownerHandle == DEFAULTUSER_OBJECT_HANDLE || \
		   isHandleRangeValid( sessionInfoPtr->ownerHandle ) ) )
		{
		DEBUG_PUTS(( "sanityCheckSession: Object handles" ));
		return( FALSE );
		}

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckProtocolInfo( const PROTOCOL_INFO *protocolInfo )
	{
	assert( isReadPtr( protocolInfo, sizeof( PROTOCOL_INFO ) ) );

	/* Check general information */
	if( !isFlagRangeZ( protocolInfo->flags, SESSION ) )
		{
		DEBUG_PUTS(( "sanityCheckProtocolInfo: Flags" ));
		return( FALSE );
		}
	if( protocolInfo->version < 0 || protocolInfo->version >= 5 )
		{
		DEBUG_PUTS(( "sanityCheckProtocolInfo: Protocol version" ));
		return( FALSE );
		}

	/* Check buffer/packet size information */
	if( protocolInfo->isReqResp != TRUE && protocolInfo->isReqResp != FALSE )
		{
		DEBUG_PUTS(( "sanityCheckProtocolInfo: Request/response indicator" ));
		return( FALSE );
		}
	if( protocolInfo->isReqResp )
		{
		if( protocolInfo->bufSize != 0 || \
			protocolInfo->sendBufStartOfs != 0 || \
			protocolInfo->maxPacketSize != 0 )
			{
			DEBUG_PUTS(( "sanityCheckProtocolInfo: Request/response protocol buffer info" ));
			return( FALSE );
			}
		}
	else
		{
		if( protocolInfo->bufSize < MIN_BUFFER_SIZE || \
			protocolInfo->bufSize >= MAX_BUFFER_SIZE || \
			protocolInfo->sendBufStartOfs < 5 || 
			protocolInfo->sendBufStartOfs >= protocolInfo->maxPacketSize || \
			protocolInfo->maxPacketSize > protocolInfo->bufSize )
			{
			DEBUG_PUTS(( "sanityCheckProtocolInfo: Transport protocol buffer info" ));
			return( FALSE );
			}
		}

	/* Check network information */
	if( protocolInfo->port <= 21 || protocolInfo->port > 65534L || \
		( ( protocolInfo->flags & SESSION_FLAG_ISHTTPTRANSPORT ) && \
			protocolInfo->port != 80 ) || \
		( !( protocolInfo->flags & SESSION_FLAG_ISHTTPTRANSPORT ) && \
			protocolInfo->port == 80 ) )
		{
		DEBUG_PUTS(( "sanityCheckProtocolInfo: Network port" ));
		return( FALSE );
		}

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkSessionFunctions( IN const SESSION_INFO *sessionInfoPtr )
	{
	const PROTOCOL_INFO *protocolInfo = \
							DATAPTR_GET( sessionInfoPtr->protocolInfo );

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_B( protocolInfo != NULL );
	REQUIRES_B( sanityCheckProtocolInfo( protocolInfo ) );

	/* We can't call sanityCheckSession() at this point because the storage
	   for the session data that we want to sanity-check hasn't been set
	   up yet.  The best that we can do is perform a subset of the standard
	   sanity checks */ 

	/* Check function pointers */
	if( !FNPTR_ISSET( sessionInfoPtr->shutdownFunction ) || \
		!FNPTR_ISSET( sessionInfoPtr->connectFunction ) || \
		!FNPTR_ISSET( sessionInfoPtr->transactFunction ) )
		{
		DEBUG_PUTS(( "checkSessionFunctions: General functions" ));
		return( FALSE );
		}
	if( protocolInfo->isReqResp )
		{
		if( !FNPTR_ISNULL( sessionInfoPtr->readHeaderFunction ) || \
			!FNPTR_ISNULL( sessionInfoPtr->processBodyFunction ) || \
			!FNPTR_ISNULL( sessionInfoPtr->preparePacketFunction ) )
			{
			DEBUG_PUTS(( "checkSessionFunctions: Spurious data transfer functions" ));
			return( FALSE );
			}
		}
	else
		{
		if( !FNPTR_ISSET( sessionInfoPtr->readHeaderFunction ) || \
			!FNPTR_ISSET( sessionInfoPtr->processBodyFunction ) || \
			!FNPTR_ISSET( sessionInfoPtr->preparePacketFunction ) )
			{
			DEBUG_PUTS(( "checkSessionFunctions: Data transfer functions" ));
			return( FALSE );
			}
		}

	return( TRUE );
	}

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

	REQUIRES( message == MESSAGE_DESTROY || \
			  sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( isEnumRange( message, MESSAGE ) );
	REQUIRES( isIntegerRange( messageValue ) );

	/* Process destroy object messages */
	if( message == MESSAGE_DESTROY )
		{
		/* Close the session.  Since we're about to destroy/delete the 
		   session object anyway, we ignore any error return value */
		( void ) closeSession( sessionInfoPtr );

		/* Clear session attributes if necessary */
		if( DATAPTR_ISSET( sessionInfoPtr->attributeList ) )
			deleteSessionInfoAll( sessionInfoPtr );

		/* Clear and free session state information if necessary.  Note that 
		   this has to follow the deletion of session attributes since the 
		   sanity check performed there detects the missingn session 
		   buffers if they're cleared first */
		if( sessionInfoPtr->sendBuffer != NULL )
			{
			zeroise( sessionInfoPtr->sendBuffer,
					 sessionInfoPtr->sendBufSize );
			safeBufferFree( sessionInfoPtr->sendBuffer );
			sessionInfoPtr->sendBuffer = NULL;
			}
		if( sessionInfoPtr->receiveBuffer != NULL )
			{
			zeroise( sessionInfoPtr->receiveBuffer,
					 sessionInfoPtr->receiveBufSize );
			safeBufferFree( sessionInfoPtr->receiveBuffer );
			sessionInfoPtr->receiveBuffer = NULL;
			}

		/* Clean up any session-related objects if necessary */
		if( sessionInfoPtr->iKeyexCryptContext != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->iKeyexCryptContext,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->iKeyexAuthContext != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->iKeyexAuthContext,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->iCryptInContext != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->iCryptInContext,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->iCryptOutContext != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->iCryptOutContext,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->iAuthInContext != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->iAuthInContext,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->iAuthOutContext != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->iAuthOutContext,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->iCertRequest != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->iCertRequest,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->iCertResponse != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->iCertResponse,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->privateKey != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->privateKey,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->cryptKeyset != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_DECREFCOUNT );
			}
		if( sessionInfoPtr->privKeyset != CRYPT_ERROR )
			{
			krnlSendNotifier( sessionInfoPtr->privKeyset,
							  IMESSAGE_DECREFCOUNT );
			}

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
				const SES_SETATTRIBUTE_FUNCTION setAttributeFunction = \
							( SES_SETATTRIBUTE_FUNCTION ) \
							FNPTR_GET( sessionInfoPtr->setAttributeFunction );

				REQUIRES( setAttributeFunction != NULL );

				/* Perform any protocol-specific additional checks if 
				   necessary */
				if( FNPTR_ISSET( sessionInfoPtr->checkAttributeFunction ) )
					{
					const SES_CHECKATTRIBUTE_FUNCTION checkAttributeFunction = \
							( SES_CHECKATTRIBUTE_FUNCTION ) \
							FNPTR_GET( sessionInfoPtr->checkAttributeFunction );

					REQUIRES( checkAttributeFunction != NULL );

					status = checkAttributeFunction( sessionInfoPtr, 
												messageDataPtr, messageValue );
					if( cryptStatusError( status ) )
						return( status );
					}

				status = setAttributeFunction( sessionInfoPtr, messageDataPtr, 
											   messageValue );
				if( status == CRYPT_ERROR_INITED )
					{
					setErrorInfo( sessionInfoPtr, messageValue, 
								  CRYPT_ERRTYPE_ATTR_PRESENT );
					return( CRYPT_ERROR_INITED );
					}
				}
			else
				{
				const SES_GETATTRIBUTE_FUNCTION getAttributeFunction = \
							( SES_GETATTRIBUTE_FUNCTION ) \
							FNPTR_GET( sessionInfoPtr->getAttributeFunction );

				REQUIRES( message == MESSAGE_GETATTRIBUTE || \
						  message == MESSAGE_GETATTRIBUTE_S );
				REQUIRES( getAttributeFunction != NULL );

				status = getAttributeFunction( sessionInfoPtr, messageDataPtr, 
											   messageValue );
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
			{
			return( getSessionAttribute( sessionInfoPtr, 
										 ( int * ) messageDataPtr,
										 messageValue ) );
			}
		if( message == MESSAGE_GETATTRIBUTE_S )
			{
			return( getSessionAttributeS( sessionInfoPtr, 
										  ( MESSAGE_DATA * ) messageDataPtr,
										  messageValue ) );
			}
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
		if( !TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_ISOPEN ) )
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
		ENSURES( TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_ISOPEN ) );
		ENSURES( sessionInfoPtr->sendBuffer != NULL );

		/* Make sure that everything is in order */
		if( TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_SENDCLOSED ) )
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
		if( !TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_ISOPEN ) )
			return( CRYPT_ERROR_NOTINITED );

		ENSURES( TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_ISOPEN ) );
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
						OUT_PTR_OPT SESSION_INFO **sessionInfoPtrPtr )
	{
	CRYPT_SESSION_TYPE sessionBaseType;
	SESSION_INFO *sessionInfoPtr;
	const PROTOCOL_INFO *protocolInfo;
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
	REQUIRES( isEnumRange( sessionType, CRYPT_SESSION ) );

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
	if( ( sessionType != sessionBaseType ) || \
		( sessionType == CRYPT_SESSION_CERTSTORE_SERVER ) )
		{
		/* If it's a server session, mark it as such.  An HTTP certstore 
		   session is a special case in that it's always a server session */
		INIT_FLAGS( sessionInfoPtr->flags, SESSION_FLAG_ISSERVER );
		}
	else
		INIT_FLAGS( sessionInfoPtr->flags, SESSION_FLAG_NONE );
	INIT_FLAGS( sessionInfoPtr->protocolFlags, 0 );
	DATAPTR_SET( sessionInfoPtr->attributeList, NULL );
	DATAPTR_SET( sessionInfoPtr->attributeListCurrent, NULL );
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
	sessionInfoPtr->networkSocket = CRYPT_ERROR;
	sessionInfoPtr->readTimeout = \
		sessionInfoPtr->writeTimeout = \
			sessionInfoPtr->connectTimeout = CRYPT_ERROR;

	/* Set up function pointers to contain null entries */
	FNPTR_SET( sessionInfoPtr->shutdownFunction, NULL );
	FNPTR_SET( sessionInfoPtr->connectFunction, NULL );
	FNPTR_SET( sessionInfoPtr->getAttributeFunction, NULL ); 
	FNPTR_SET( sessionInfoPtr->setAttributeFunction, NULL );
	FNPTR_SET( sessionInfoPtr->checkAttributeFunction, NULL );
	FNPTR_SET( sessionInfoPtr->transactFunction, NULL );
	FNPTR_SET( sessionInfoPtr->readHeaderFunction, NULL );
	FNPTR_SET( sessionInfoPtr->processBodyFunction, NULL );
	FNPTR_SET( sessionInfoPtr->preparePacketFunction, NULL );
#if defined( USE_WEBSOCKETS ) || defined( USE_EAP )
	FNPTR_SET( sessionInfoPtr->activateOuterSubprotocolFunction, NULL );
	FNPTR_SET( sessionInfoPtr->closeOuterSubprotocolFunction, NULL );
	FNPTR_SET( sessionInfoPtr->activateInnerSubprotocolFunction, NULL );
	FNPTR_SET( sessionInfoPtr->closeInnerSubprotocolFunction, NULL );
#endif /* USE_WEBSOCKETS || USE_EAP */

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
		{
		void *scoreboardInfo = getScoreboardInfoStorage();

		DATAPTR_SET( sessionInfoPtr->sessionSSL->scoreboardInfoPtr, 
					 scoreboardInfo );
		}

	/* Check that the protocol info is OK */
	protocolInfo = DATAPTR_GET( sessionInfoPtr->protocolInfo );
	ENSURES( protocolInfo != NULL );
	ENSURES( sanityCheckProtocolInfo( protocolInfo ) );

	/* Copy mutable protocol-specific information into the session info */
	SET_FLAGS( sessionInfoPtr->flags, protocolInfo->flags );
	sessionInfoPtr->clientReqAttrFlags = protocolInfo->clientReqAttrFlags;
	sessionInfoPtr->serverReqAttrFlags = protocolInfo->serverReqAttrFlags;
	sessionInfoPtr->version = protocolInfo->version;
	if( protocolInfo->isReqResp )
		{
		sessionInfoPtr->sendBufSize = CRYPT_UNUSED;
		sessionInfoPtr->receiveBufSize = MIN_BUFFER_SIZE;
		}
	else
		{
		sessionInfoPtr->sendBufSize = sessionInfoPtr->receiveBufSize = \
				protocolInfo->bufSize;
		sessionInfoPtr->sendBufStartOfs = sessionInfoPtr->receiveBufStartOfs = \
				sessionInfoPtr->sendBufPos = protocolInfo->sendBufStartOfs;
		sessionInfoPtr->maxPacketSize = protocolInfo->maxPacketSize;
		}

	/* Install default handlers if no session-specific ones are provided */
	status = initSessionIO( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* Check that the handlers are all OK */
	ENSURES( checkSessionFunctions( sessionInfoPtr ) );

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
	REQUIRES( isEnumRange( createInfo->arg1, CRYPT_SESSION ) );

	/* Pass the call on to the lower-level open function */
	initStatus = openSession( &iCryptSession, createInfo->cryptOwner,
							  createInfo->arg1, &sessionInfoPtr );
	if( cryptStatusError( initStatus ) )
		{
		/* If the create object failed, return immediately */
		if( sessionInfoPtr == NULL )
			return( initStatus );

		/* Since no object has been created there's nothing to get a 
		   detailed error string from, but we can at least send the 
		   information to the debug output */
		DEBUG_PRINT(( "Session create error: %s.\n",
					  ( sessionInfoPtr->errorInfo.errorStringLength > 0 ) ? \
					    sessionInfoPtr->errorInfo.errorString : \
						"<<<No information available>>>" ));

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
				status = initScoreboard( getScoreboardInfoStorage() );
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
				endScoreboard( getScoreboardInfoStorage() );
			if( initLevel > 0 )
				netEndTCP();
			initLevel = 0;

			return( CRYPT_OK );
		}

	retIntError();
	}
#endif /* USE_SESSIONS */
