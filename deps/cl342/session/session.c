/****************************************************************************
*																			*
*						cryptlib Session Support Routines					*
*						Copyright Peter Gutmann 1998-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "session.h"
#else
  #include "crypt.h"
  #include "session/session.h"
#endif /* Compiler-specific includes */

#ifdef USE_SESSIONS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Reset the state of a request/response session for reuse so that it can
   process another request or response */

STDC_NONNULL_ARG( ( 1 ) ) \
static void cleanupReqResp( INOUT SESSION_INFO *sessionInfoPtr,
							const BOOLEAN isPostTransaction )
	{
	const BOOLEAN isServer = isServer( sessionInfoPtr );

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Clean up server requests left over from a previous transaction or 
	   that have been created by the just-completed transaction */
	if( isServer && sessionInfoPtr->iCertRequest != CRYPT_ERROR )
		{
		krnlSendNotifier( sessionInfoPtr->iCertRequest,
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iCertRequest = CRYPT_ERROR;
		}

	/* Clean up client/server responses left over from a previous
	   transaction and server responses created by the just-completed
	   transaction */
	if( ( isServer || !isPostTransaction ) && \
		sessionInfoPtr->iCertResponse != CRYPT_ERROR )
		{
		krnlSendNotifier( sessionInfoPtr->iCertResponse,
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iCertResponse = CRYPT_ERROR;
		}
	}

/* Initialise network connection information based on the contents of the
   session object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initSessionNetConnectInfo( const SESSION_INFO *sessionInfoPtr,
							   OUT NET_CONNECT_INFO *connectInfo )
	{
	const ATTRIBUTE_LIST *clientNamePtr, *serverNamePtr, *portInfoPtr;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( connectInfo, sizeof( NET_CONNECT_INFO ) ) );

	initNetConnectInfo( connectInfo, sessionInfoPtr->ownerHandle,
				sessionInfoPtr->readTimeout, sessionInfoPtr->connectTimeout,
				( sessionInfoPtr->transportSession != CRYPT_ERROR ) ? \
					NET_OPTION_TRANSPORTSESSION : \
				( sessionInfoPtr->networkSocket != CRYPT_ERROR ) ? \
					NET_OPTION_NETWORKSOCKET : NET_OPTION_HOSTNAME );

	/* If the user has supplied the network transport information, there's
	   nothing further to do */
	if( sessionInfoPtr->transportSession != CRYPT_ERROR )
		{
		connectInfo->iCryptSession = sessionInfoPtr->transportSession;
		return( CRYPT_OK );
		}
	if( sessionInfoPtr->networkSocket != CRYPT_ERROR )
		{
		connectInfo->networkSocket = sessionInfoPtr->networkSocket;
		return( CRYPT_OK );
		}

	/* If there are explicit client and/or server names set, record them.  
	   For a client the server name is the remote system to connect to
	   and the client name is the optional local interface to bind to, for
	   the server the server name is the optional local interface to bind
	   to */
	clientNamePtr = findSessionInfo( sessionInfoPtr->attributeList,
									 CRYPT_SESSINFO_CLIENT_NAME );
	serverNamePtr = findSessionInfo( sessionInfoPtr->attributeList,
									 CRYPT_SESSINFO_SERVER_NAME );
	if( isServer( sessionInfoPtr ) )
		{
		if( serverNamePtr != NULL )
			{
			connectInfo->interface = serverNamePtr->value;
			connectInfo->interfaceLength = serverNamePtr->valueLength;
			}
		}
	else
		{
		REQUIRES( serverNamePtr != NULL );

		connectInfo->name = serverNamePtr->value;
		connectInfo->nameLength = serverNamePtr->valueLength;
		if( clientNamePtr != NULL )
			{
			connectInfo->interface = clientNamePtr->value;
			connectInfo->interfaceLength = clientNamePtr->valueLength;
			}
		}

	/* If there's an explicit port set, connect/bind to it, otherwise use the
	   default port for the protocol */
	if( ( portInfoPtr = \
			findSessionInfo( sessionInfoPtr->attributeList,
							 CRYPT_SESSINFO_SERVER_PORT ) ) != NULL )
		connectInfo->port = portInfoPtr->intValue;
	else
		connectInfo->port = sessionInfoPtr->protocolInfo->port;

	return( CRYPT_OK );
	}

/* Make sure that mutually exclusive session attributes haven't been set.
   The checks performed are:

	CRYPT_SESSINFO_REQUEST		-> !CRYPT_SESSINFO_REQUEST, 
								   !CRYPT_SESSINFO_PRIVATEKEY,
								   !CRYPT_SESSINFO_CMP_PRIVKEYSET

	CRYPT_SESSINFO_PRIVATEKEY	-> !CRYPT_SESSINFO_PRIVATEKEY,
								   !CRYPT_SESSINFO_CMP_PRIVKEYSET

	CRYPT_SESSINFO_CACERTIFICATE-> !CRYPT_SESSINFO_CACERTIFICATE,
								   !CRYPT_SESSINFO_SERVER_FINGERPRINT

	CRYPT_SESSINFO_SERVER_FINGERPRINT
								-> !CRYPT_SESSINFO_SERVER_FINGERPRINT,
								   !CRYPT_SESSINFO_CACERTIFICATE */

#define CHECK_ATTR_NONE			0x00
#define CHECK_ATTR_REQUEST		0x01
#define CHECK_ATTR_PRIVKEY		0x02
#define CHECK_ATTR_PRIVKEYSET	0x04
#define CHECK_ATTR_CACERT		0x08
#define CHECK_ATTR_FINGERPRINT	0x10

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkAttributesConsistent( INOUT SESSION_INFO *sessionInfoPtr,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	static const MAP_TABLE excludedAttrTbl[] = {
		{ CRYPT_SESSINFO_REQUEST, 
			CHECK_ATTR_REQUEST | CHECK_ATTR_PRIVKEY | CHECK_ATTR_PRIVKEYSET },
		{ CRYPT_SESSINFO_PRIVATEKEY,
			CHECK_ATTR_PRIVKEY | CHECK_ATTR_PRIVKEYSET },
		{ CRYPT_SESSINFO_CACERTIFICATE, 
			CHECK_ATTR_CACERT | CHECK_ATTR_FINGERPRINT },
		{ CRYPT_SESSINFO_SERVER_FINGERPRINT, 
			CHECK_ATTR_FINGERPRINT | CHECK_ATTR_CACERT },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 } 
		};
	int flags = 0, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	
	REQUIRES_B( attribute == CRYPT_SESSINFO_REQUEST || \
				attribute == CRYPT_SESSINFO_PRIVATEKEY || \
				attribute == CRYPT_SESSINFO_CACERTIFICATE || \
				attribute == CRYPT_SESSINFO_SERVER_FINGERPRINT );

	/* Find the excluded-attribute information for this attribute */
	status = mapValue( attribute, &flags, excludedAttrTbl,
					   FAILSAFE_ARRAYSIZE( excludedAttrTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status  ) );

	/* Make sure that none of the excluded attributes are present */
	if( ( flags & CHECK_ATTR_REQUEST ) && \
		sessionInfoPtr->iCertRequest != CRYPT_ERROR )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_REQUEST,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		return( FALSE );
		}
	if( ( flags & CHECK_ATTR_PRIVKEYSET ) && \
		sessionInfoPtr->privKeyset != CRYPT_ERROR )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_CMP_PRIVKEYSET,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		return( FALSE );
		}
	if( ( flags & CHECK_ATTR_CACERT ) && \
		sessionInfoPtr->iAuthInContext != CRYPT_ERROR )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_CACERTIFICATE,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		return( FALSE );
		}
	if( ( flags & CHECK_ATTR_FINGERPRINT ) && \
		findSessionInfo( sessionInfoPtr->attributeList,
						 CRYPT_SESSINFO_SERVER_FINGERPRINT ) != NULL )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_SERVER_FINGERPRINT,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		return( FALSE );
		}
	
	return( TRUE );
	}

/* Check that a server's certificate is currently valid.  This self-check 
   avoids ugly silent failures where everything appears to work just fine on 
   the server side but the client gets invalid data back */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int checkServerCertValid( const CRYPT_CERTIFICATE iServerCert,
						  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
						  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType )
	{
	static const int complianceLevelStandard = CRYPT_COMPLIANCELEVEL_STANDARD;
	int complianceLevel, value, status;

	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( isHandleRangeValid( iServerCert ) );

	status = krnlSendMessage( iServerCert, IMESSAGE_GETATTRIBUTE, 
							  &complianceLevel, 
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		{
		/* We can't do much more if we can't even get the initial compliance 
		   level */
		return( CRYPT_OK );
		}

	/* Check whether the certificate is valid at a standard level of 
	   compliance, which catches expired certificates and other obvious
	   problems */
	krnlSendMessage( iServerCert, IMESSAGE_SETATTRIBUTE, 
					 ( MESSAGE_CAST ) &complianceLevelStandard, 
					 CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	status = krnlSendMessage( iServerCert, IMESSAGE_CHECK, NULL, 
							  MESSAGE_CHECK_CERT );
	krnlSendMessage( iServerCert, IMESSAGE_SETATTRIBUTE, 
					 ( MESSAGE_CAST ) &complianceLevel, 
					 CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );

	/* The certificate isn't valid, copy the extended error information up 
	   from the certificate if possible.  This leads to rather odd extended 
	   error information for the session since the error information is 
	   pointing at certificate attributes for a session object, but it's 
	   unlikely that users will think of checking the certificate for error 
	   details and the presence of certificate-related error information 
	   should make it obvious what it pertains to */
	status = krnlSendMessage( iServerCert, IMESSAGE_GETATTRIBUTE, &value, 
							  CRYPT_ATTRIBUTE_ERRORLOCUS );
	if( cryptStatusOK( status ) )
		{
		*errorLocus = value;
		status = krnlSendMessage( iServerCert, IMESSAGE_GETATTRIBUTE, &value, 
								  CRYPT_ATTRIBUTE_ERRORTYPE );
		}
	if( cryptStatusOK( status ) )
		*errorType = value;

	return( CRYPT_ERROR_INVALID );
	}

/****************************************************************************
*																			*
*							Session Activation Functions					*
*																			*
****************************************************************************/

/* Check client/server-specific required values */

CHECK_RETVAL_ENUM( CRYPT_ATTRIBUTE_TYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static CRYPT_ATTRIBUTE_TYPE checkClientParameters( const SESSION_INFO *sessionInfoPtr )
	{
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that the network comms parameters are present */
	if( sessionInfoPtr->transportSession == CRYPT_ERROR && \
		sessionInfoPtr->networkSocket == CRYPT_ERROR && \
		findSessionInfo( sessionInfoPtr->attributeList, 
						 CRYPT_SESSINFO_SERVER_NAME ) == NULL )
		return( CRYPT_SESSINFO_SERVER_NAME );

	/* Make sure that the username + password and/or user private key are 
	   present if required */
	if( ( sessionInfoPtr->clientReqAttrFlags & SESSION_NEEDS_USERID ) && \
		findSessionInfo( sessionInfoPtr->attributeList, 
						 CRYPT_SESSINFO_USERNAME ) == NULL )
		return( CRYPT_SESSINFO_USERNAME );
	if( ( sessionInfoPtr->clientReqAttrFlags & SESSION_NEEDS_PASSWORD ) && \
		findSessionInfo( sessionInfoPtr->attributeList, 
						 CRYPT_SESSINFO_PASSWORD ) == NULL )
		{
		/* There's no password present, see if we can use a private key as 
		   an alternative */
		if( !( sessionInfoPtr->clientReqAttrFlags & \
			   SESSION_NEEDS_KEYORPASSWORD ) || \
			sessionInfoPtr->privateKey == CRYPT_ERROR )
			return( CRYPT_SESSINFO_PASSWORD );
			}
	if( ( sessionInfoPtr->clientReqAttrFlags & SESSION_NEEDS_PRIVATEKEY ) && \
		sessionInfoPtr->privateKey == CRYPT_ERROR )
		{
		/* There's no private key present, see if we can use a password as 
		   an alternative */
		if( !( sessionInfoPtr->clientReqAttrFlags & \
			   SESSION_NEEDS_KEYORPASSWORD ) || \
			findSessionInfo( sessionInfoPtr->attributeList, 
							 CRYPT_SESSINFO_PASSWORD ) == NULL )
			return( CRYPT_SESSINFO_PRIVATEKEY );
		}

	/* Make sure that request/response protocol data is present if required */
	if( ( sessionInfoPtr->clientReqAttrFlags & SESSION_NEEDS_REQUEST ) && \
		sessionInfoPtr->iCertRequest == CRYPT_ERROR )
		return( CRYPT_SESSINFO_REQUEST );

	return( CRYPT_ATTRIBUTE_NONE );
	}

CHECK_RETVAL_ENUM( CRYPT_ATTRIBUTE_TYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static CRYPT_ATTRIBUTE_TYPE checkServerParameters( const SESSION_INFO *sessionInfoPtr )
	{
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that server key and keyset information is present if 
	   required */
	if( ( sessionInfoPtr->serverReqAttrFlags & SESSION_NEEDS_PRIVATEKEY ) && \
		sessionInfoPtr->privateKey == CRYPT_ERROR )
		{
		/* There's no private key present, see if we can use a username +
		   password as an alternative */
		if( !( sessionInfoPtr->serverReqAttrFlags & \
			   SESSION_NEEDS_KEYORPASSWORD ) || \
			findSessionInfo( sessionInfoPtr->attributeList, 
							 CRYPT_SESSINFO_PASSWORD ) == NULL )
			return( CRYPT_SESSINFO_PRIVATEKEY );
		}
	if( ( sessionInfoPtr->serverReqAttrFlags & SESSION_NEEDS_KEYSET ) && \
		sessionInfoPtr->cryptKeyset == CRYPT_ERROR )
		return( CRYPT_SESSINFO_KEYSET );

	return( CRYPT_ATTRIBUTE_NONE );
	}

/* Activate the network connection for a session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int activateConnection( INOUT SESSION_INFO *sessionInfoPtr )
	{
	CRYPT_ATTRIBUTE_TYPE errorAttribute;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that everything is set up ready to go */
	errorAttribute = isServer( sessionInfoPtr ) ? \
					 checkServerParameters( sessionInfoPtr ) : \
					 checkClientParameters( sessionInfoPtr );
	if( errorAttribute != CRYPT_ATTRIBUTE_NONE )
		{
		setErrorInfo( sessionInfoPtr, errorAttribute, 
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_NOTINITED );
		}
	ENSURES( isServer( sessionInfoPtr ) || \
			 findSessionInfo( sessionInfoPtr->attributeList, 
							  CRYPT_SESSINFO_SERVER_NAME ) != NULL || \
			 sessionInfoPtr->networkSocket != CRYPT_ERROR );
	ENSURES( findSessionInfo( sessionInfoPtr->attributeList,
							  CRYPT_SESSINFO_SERVER_PORT ) != NULL || \
			 sessionInfoPtr->protocolInfo->port > 0 );

	/* Allocate the send and receive buffers if necessary.  The send buffer
	   isn't used for request-response session types that use the receive
	   buffer for both outgoing and incoming data so we only allocate it if
	   it's actually required */
	if( sessionInfoPtr->sendBuffer == NULL )
		{
		REQUIRES( sessionInfoPtr->receiveBufSize >= MIN_BUFFER_SIZE && \
				  sessionInfoPtr->receiveBufSize < MAX_INTLENGTH );
		REQUIRES( ( sessionInfoPtr->sendBufSize >= MIN_BUFFER_SIZE && \
					sessionInfoPtr->sendBufSize < MAX_INTLENGTH ) || \
				  sessionInfoPtr->sendBufSize == CRYPT_UNUSED );

		if( ( sessionInfoPtr->receiveBuffer = \
						clAlloc( "activateConnection", \
								 sessionInfoPtr->receiveBufSize + 8 ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		if( sessionInfoPtr->sendBufSize != CRYPT_UNUSED )
			{
			/* When allocating the send buffer we use the size given for the
			   receive buffer since the user may have overridden the default
			   buffer size */
			if( ( sessionInfoPtr->sendBuffer = \
						clAlloc( "activateConnection", \
								 sessionInfoPtr->receiveBufSize + 8 ) ) == NULL )
				{
				clFree( "activateConnection", sessionInfoPtr->receiveBuffer );
				sessionInfoPtr->receiveBuffer = NULL;
				return( CRYPT_ERROR_MEMORY );
				}
			sessionInfoPtr->sendBufSize = sessionInfoPtr->receiveBufSize;
			}
		}
	ENSURES( sessionInfoPtr->receiveBuffer != NULL && \
			 sessionInfoPtr->receiveBufSize >= MIN_BUFFER_SIZE && \
			 sessionInfoPtr->receiveBufSize < MAX_INTLENGTH );
	ENSURES( sessionInfoPtr->sendBufSize == CRYPT_UNUSED || \
			 sessionInfoPtr->sendBuffer != NULL );

	/* Set timeouts if they're not set yet.  If there's an error then we use
	   the default value rather than aborting the entire session because of 
	   a minor difference in timeout values, although we also warn the 
	   caller in debug mode */
	if( sessionInfoPtr->connectTimeout == CRYPT_ERROR )
		{
		int timeout;

		status = krnlSendMessage( sessionInfoPtr->ownerHandle,
								  IMESSAGE_GETATTRIBUTE, &timeout,
								  CRYPT_OPTION_NET_CONNECTTIMEOUT );
		if( cryptStatusOK( status ) )
			sessionInfoPtr->connectTimeout = timeout;
		else
			{
			DEBUG_DIAG(( "Couldn't get connect timeout config value" ));
			assert( DEBUG_WARN );
			sessionInfoPtr->connectTimeout = 30;
			}
		}
	if( sessionInfoPtr->readTimeout == CRYPT_ERROR )
		{
		int timeout;

		status = krnlSendMessage( sessionInfoPtr->ownerHandle,
								  IMESSAGE_GETATTRIBUTE, &timeout,
								  CRYPT_OPTION_NET_READTIMEOUT );
		if( cryptStatusOK( status ) )
			sessionInfoPtr->readTimeout = timeout;
		else
			{
			DEBUG_DIAG(( "Couldn't get read timeout config value" ));
			assert( DEBUG_WARN );
			sessionInfoPtr->readTimeout = 30;
			}
		}
	if( sessionInfoPtr->writeTimeout == CRYPT_ERROR )
		{
		int timeout;

		status = krnlSendMessage( sessionInfoPtr->ownerHandle,
								  IMESSAGE_GETATTRIBUTE, &timeout,
								  CRYPT_OPTION_NET_WRITETIMEOUT );
		if( cryptStatusOK( status ) )
			sessionInfoPtr->writeTimeout = timeout;
		else
			{
			DEBUG_DIAG(( "Couldn't get write timeout config value" ));
			assert( DEBUG_WARN );
			sessionInfoPtr->writeTimeout = 30;
			}
		}

	/* Wait for any async driver binding to complete.  We can delay this
	   until this very late stage because no networking functionality is
	   used until this point */
	if( !krnlWaitSemaphore( SEMAPHORE_DRIVERBIND ) )
		{
		/* The kernel is shutting down, bail out */
		return( CRYPT_ERROR_PERMISSION );
		}

	/* If this is the first time that we've got here, activate the session */
	if( !( sessionInfoPtr->flags & SESSION_PARTIALOPEN ) )
		{
		REQUIRES( !( sessionInfoPtr->flags & SESSION_ISOPEN ) )

		status = sessionInfoPtr->connectFunction( sessionInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If it's a secure data transport session, complete the session state
	   setup.  Note that some sessions dynamically change the protocol 
	   information during the handshake to accommodate parameters negotiated 
	   during the handshake so we can only access the protocol information 
	   after the handshake has completed */
	if( !sessionInfoPtr->protocolInfo->isReqResp )
		{
		/* Complete the session handshake to set up the secure state */
		status = sessionInfoPtr->transactFunction( sessionInfoPtr );
		if( cryptStatusError( status ) )
			{
			/* If we need feedback from the user before we can complete the 
			   handshake (for example checking a user name and password or 
			   certificate supplied by the other side) we remain in the 
			   handshake state so that the user can re-activate the session 
			   after confirming (or denying) the check */
			if( status == CRYPT_ENVELOPE_RESOURCE )
				sessionInfoPtr->flags |= SESSION_PARTIALOPEN;

			return( status );
			}

		/* Notify the kernel that the session key context is attached to the
		   session object.  Note that we increment its reference count even
		   though it's an internal object used only by the session because
		   otherwise it'll be automatically destroyed by the kernel as a
		   zero-reference dependent object when the session object is
		   destroyed (but before the session object itself since the context 
		   is just a dependent object).  This automatic cleanup could cause 
		   problems for lower-level session management code that tries to 
		   work with the (apparently still-valid) handle, for example 
		   protocols that need to encrypt a close-channel message on session 
		   shutdown */
		krnlSendMessage( sessionInfoPtr->objectHandle, IMESSAGE_SETDEPENDENT,
						 &sessionInfoPtr->iCryptInContext,
						 SETDEP_OPTION_INCREF );

		/* Set up the buffer management variables */
		sessionInfoPtr->receiveBufPos = sessionInfoPtr->receiveBufEnd = 0;
		sessionInfoPtr->sendBufPos = sessionInfoPtr->sendBufStartOfs;

		/* For data transport sessions, partial reads and writes (that is,
		   sending and receiving partial packets in the presence of 
		   timeouts) are permitted */
		sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_PARTIALREAD, TRUE );
		sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_PARTIALWRITE, TRUE );
		}

	/* The handshake has been completed, switch from the handshake timeout
	   to the data transfer timeout and remember that the session has been
	   successfully established */
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_HANDSHAKECOMPLETE, TRUE );
	sessionInfoPtr->flags &= ~SESSION_PARTIALOPEN;
	sessionInfoPtr->flags |= SESSION_ISOPEN;

	return( CRYPT_OK );
	}

/* Activate a session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int activateSession( INOUT SESSION_INFO *sessionInfoPtr )
	{
	int streamState, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Activate the connection if necessary */
	if( !( sessionInfoPtr->flags & SESSION_ISOPEN ) )
		{
		/* Try and activate the session */
		status = activateConnection( sessionInfoPtr );
		if( cryptStatusError( status ) )
			return( status );

		/* The session activation succeeded, make sure that we don't try
		   and replace the ephemeral attributes established during the 
		   session setup during any later operations.  This is used for
		   example when we're the server and the client provides us with 
		   authentication data but the validity of the data hasn't been 
		   confirmed yet by the user (see the comment about the 
		   CRYPT_ENVELOPE_RESOURCE status in activateConnection()), normally 
		   this would be deleted/overwritten when the session is recycled 
		   but once the caller has confirmed it as being valid we lock it to 
		   make sure that it won't be changed any more */
		if( sessionInfoPtr->attributeList != NULL )
			lockEphemeralAttributes( sessionInfoPtr->attributeList );
		}

	/* If it's a secure data transport session it's up to the caller to move 
	   data over it, and we're done */
	if( !sessionInfoPtr->protocolInfo->isReqResp )
		return( CRYPT_OK );

	/* Carry out a transaction on the request-response connection.  We
	   perform a cleanup of request/response data around the activation,
	   beforehand to catch data such as responses left over from a previous
	   transaction and afterwards to clean up ephemeral data such as
	   requests sent to a server */
	cleanupReqResp( sessionInfoPtr, FALSE );
	status = sessionInfoPtr->transactFunction( sessionInfoPtr );
	cleanupReqResp( sessionInfoPtr, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Check whether the other side has indicated that it's closing the 
	   stream and if it has, shut down our side as well and record the fact
	   that the session is now closed */
	status = sioctlGet( &sessionInfoPtr->stream, STREAM_IOCTL_CONNSTATE,
						&streamState, sizeof( int ) );
	if( cryptStatusError( status ) || !streamState )
		{
		sessionInfoPtr->flags &= ~SESSION_ISOPEN;
		sessionInfoPtr->shutdownFunction( sessionInfoPtr );
		}
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Shutdown Functions						*
*																			*
****************************************************************************/

/* Send a close notification.  This requires special-case handling because
   it's not certain how long we should wait around for the close to happen.
   If we're in the middle of a cryptlib shutdown then we don't want to wait 
   around forever since this would stall the overall shutdown, but if it's a 
   standard session shutdown then we should wait for at least a small amount 
   of time to ensure that all of the data is sent */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sendCloseNotification( INOUT SESSION_INFO *sessionInfoPtr,
						   IN_BUFFER_OPT( length ) const void *data, 
						   IN_LENGTH_SHORT_Z const int length )
	{
	BOOLEAN isShutdown = FALSE;
	int dummy, status = CRYPT_OK;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( ( data == NULL && length == 0 ) || \
			isReadPtr( data, length ) );

	REQUIRES( ( data == NULL && length == 0 ) || \
			  ( data != NULL && \
				length > 0 && length < MAX_INTLENGTH_SHORT ) );

	/* Determine whether we're being shut down as a part of a general 
	   cryptlib shutdown or just a session shutdown.  We do this by trying 
	   to read a configuration option from the owning user object, if the 
	   kernel is in the middle of a shutdown it disallows all frivolous 
	   messages so if we get a permission error then we're in the middle of 
	   the shutdown */
	if( krnlSendMessage( sessionInfoPtr->ownerHandle, 
						 IMESSAGE_GETATTRIBUTE, &dummy, 
						 CRYPT_OPTION_INFO_MAJORVERSION ) == CRYPT_ERROR_PERMISSION )
		isShutdown = TRUE;

	/* If necessary set a timeout sufficient to at least provide a chance of 
	   sending our close notification and receiving the other side's ack of 
	   the close, but without leading to excessive delays during the 
	   shutdown */
	if( isShutdown )
		{
		/* It's a cryptlib-wide shutdown, try and get out as quickly as
		   possible */
		sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_WRITETIMEOUT, 2 );
		}
	else
		{
		int timeout;

		/* It's a standard session shutdown, wait around for at least five
		   seconds, but not more than fifteen */
		status = sioctlGet( &sessionInfoPtr->stream, 
							STREAM_IOCTL_WRITETIMEOUT, &timeout, 
							sizeof( int ) );
		if( cryptStatusError( status ) || timeout < 5 )
			timeout = 5;
		else
			{
			if( timeout > 15 )
				timeout = 15;
			}
		sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_WRITETIMEOUT, 
				   timeout );
		}

	/* Send the close notification to the peer */
	if( data != NULL )
		status = swrite( &sessionInfoPtr->stream, data, length );

	/* Close the send side of the connection if it's a cryptlib-internal 
	   socket.  This is needed by some implementations that want to see a 
	   FIN before they react to a shutdown notification, as well as being
	   a hint to the network code to flush any remaining data enqueued for
	   sending before the arrival of the full close.  If it's a user-managed 
	   socket we can't perform the partial close since this would affect the 
	   state of the socket as seen by the user, since the need to see the 
	   FIN is fairly rare we choose this as the less problematic of the two 
	   options */
	if( sessionInfoPtr->networkSocket == CRYPT_ERROR )
		{
		sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_CLOSESENDCHANNEL, 
				   TRUE );
		}

	return( ( data == NULL || !cryptStatusError( status ) ) ? \
			CRYPT_OK : status );
	}

/****************************************************************************
*																			*
*							Default Action Handlers							*
*																			*
****************************************************************************/

/* Default init/shutdown functions used when no session-specific ones are
   provided */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int defaultClientStartupFunction( INOUT SESSION_INFO *sessionInfoPtr )
	{
	NET_CONNECT_INFO connectInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Connect to the server */
	status = initSessionNetConnectInfo( sessionInfoPtr, &connectInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionInfoPtr->flags & SESSION_ISHTTPTRANSPORT )
		status = sNetConnect( &sessionInfoPtr->stream, STREAM_PROTOCOL_HTTP,
							  &connectInfo, &sessionInfoPtr->errorInfo );
	else
		{
		status = sNetConnect( &sessionInfoPtr->stream,
							  STREAM_PROTOCOL_TCPIP,
							  &connectInfo, &sessionInfoPtr->errorInfo );
		}
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int defaultServerStartupFunction( INOUT SESSION_INFO *sessionInfoPtr )
	{
	NET_CONNECT_INFO connectInfo;
	int nameLen, port, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Wait for a client connection */
	status = initSessionNetConnectInfo( sessionInfoPtr, &connectInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionInfoPtr->flags & SESSION_ISHTTPTRANSPORT )
		status = sNetListen( &sessionInfoPtr->stream, STREAM_PROTOCOL_HTTP,
							 &connectInfo, &sessionInfoPtr->errorInfo );
	else
		{
		status = sNetListen( &sessionInfoPtr->stream,
							 STREAM_PROTOCOL_TCPIP,
							 &connectInfo, &sessionInfoPtr->errorInfo );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Save the client details for the caller, using the (always-present)
	   receive buffer as the intermediate store */
	status = sioctlGet( &sessionInfoPtr->stream, 
						STREAM_IOCTL_GETCLIENTNAMELEN, 
						&nameLen, sizeof( int ) );
	if( cryptStatusOK( status ) )
		{
		status = sioctlGet( &sessionInfoPtr->stream, 
							STREAM_IOCTL_GETCLIENTNAME,
							sessionInfoPtr->receiveBuffer, 
							CRYPT_MAX_TEXTSIZE );
		}
	if( cryptStatusError( status ) )
		{
		/* No client information available, exit */
		return( CRYPT_OK );
		}
	status = addSessionInfoS( &sessionInfoPtr->attributeList, 
							  CRYPT_SESSINFO_CLIENT_NAME, 
							  sessionInfoPtr->receiveBuffer, nameLen );
	if( cryptStatusError( status ) )
		return( status );
	status = sioctlGet( &sessionInfoPtr->stream, STREAM_IOCTL_GETCLIENTPORT, 
						&port, sizeof( int ) );
	if( cryptStatusError( status ) )
		{
		/* No port information available, exit */
		return( CRYPT_OK );
		}
	return( addSessionInfo( &sessionInfoPtr->attributeList, 
							CRYPT_SESSINFO_CLIENT_PORT, port ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void defaultShutdownFunction( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	sNetDisconnect( &sessionInfoPtr->stream );
	}

/* Default get-attribute function used when no session-specific one is
   provided */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int defaultGetAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
										OUT void *data,
										IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	CRYPT_CERTIFICATE *responsePtr = ( CRYPT_CERTIFICATE * ) data;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( data, sizeof( int ) ) );

	REQUIRES( type == CRYPT_SESSINFO_RESPONSE );

	/* If we didn't get a response there's nothing to return */
	if( sessionInfoPtr->iCertResponse == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTFOUND );

/************************************************************************/
/* SCEP gets a bit complicated because a single object has to fill 
   multiple roles so that for example the issued certificate has to do 
   double duty for both encryption and authentication.  For now we work 
   around this by juggling the values around */
if( sessionInfoPtr->type == CRYPT_SESSION_SCEP && \
	sessionInfoPtr->iAuthInContext != CRYPT_ERROR )
	{
	*responsePtr = sessionInfoPtr->iCertResponse;
	sessionInfoPtr->iCertResponse = sessionInfoPtr->iAuthInContext;
	sessionInfoPtr->iAuthInContext = CRYPT_ERROR;

	return( CRYPT_OK );
	}
/************************************************************************/

	/* Return the information to the caller */
	krnlSendNotifier( sessionInfoPtr->iCertResponse, IMESSAGE_INCREFCOUNT );
	*responsePtr = sessionInfoPtr->iCertResponse;
	return( CRYPT_OK );
	}

/* Set up the function pointers to the session I/O methods */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initSessionIO( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const PROTOCOL_INFO *protocolInfoPtr = sessionInfoPtr->protocolInfo;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Install default handler functions if required */
	if( sessionInfoPtr->shutdownFunction == NULL )
		sessionInfoPtr->shutdownFunction = defaultShutdownFunction;
	if( sessionInfoPtr->connectFunction == NULL )
		sessionInfoPtr->connectFunction = isServer( sessionInfoPtr ) ? 
			defaultServerStartupFunction : defaultClientStartupFunction;
	if( protocolInfoPtr->isReqResp && \
		sessionInfoPtr->getAttributeFunction == NULL )
		sessionInfoPtr->getAttributeFunction = defaultGetAttributeFunction;

	return( CRYPT_OK );
	}
#endif /* USE_SESSIONS */
