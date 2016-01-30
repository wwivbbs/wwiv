/****************************************************************************
*																			*
*				cryptlib SSHv2 Server-side Channel Message Management		*
*						Copyright Peter Gutmann 1998-2008					*
*																			*
****************************************************************************/

#include <stdio.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "session.h"
  #include "ssh.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "session/session.h"
  #include "session/ssh.h"
#endif /* Compiler-specific includes */

#ifdef USE_SSH

/* SSH identifies lower-level operations using awkward string-based 
   identifiers, to make these easier to work with we use lookup tables to
   manage them and optionally map them to integer values */

#ifdef USE_SSH_EXTENDED
typedef enum { REQUEST_NONE, REQUEST_SUBSYSTEM, REQUEST_SHELL, REQUEST_EXEC,
			   REQUEST_PORTFORWARD, REQUEST_PORTFORWARD_CANCEL, REQUEST_PTY,
			   REQUEST_NOOP, REQUEST_DISALLOWED } REQUEST_TYPE;
#else
typedef enum { REQUEST_NONE, REQUEST_SHELL, REQUEST_PTY, REQUEST_NOOP, 
			   REQUEST_DISALLOWED } REQUEST_TYPE;
#endif /* USE_SSH_EXTENDED */

#define REQUEST_FLAG_NONE		0x00/* No request flag */
#define REQUEST_FLAG_TERMINAL	0x01/* Request ends negotiation */

typedef struct {
	BUFFER_FIXED( requestNameLength ) \
	const char FAR_BSS *requestName;/* String form of request type */
	const int requestNameLength;
	const REQUEST_TYPE requestType;	/* Integer form of request type */
	const int flags;				/* Request flags */
	} REQUEST_TYPE_INFO;

typedef struct {
	BUFFER_FIXED( channelNameLength ) \
	const char FAR_BSS *channelName;/* String form of channel type */
	const int channelNameLength;
	const BOOLEAN isPortForwarding;
	} CHANNEL_TYPE_INFO;

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#ifdef USE_SSH_EXTENDED

/* Read host name/address and port information and format it into string
   form for the caller */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
static int readAddressAndPort( INOUT SESSION_INFO *sessionInfoPtr, 
							   INOUT STREAM *stream,
							   OUT_BUFFER( hostInfoMaxLen, *hostInfoLen ) \
									char *hostInfo, 
							   IN_LENGTH_SHORT_MIN( 16 ) const int hostInfoMaxLen, 
							   OUT_LENGTH_SHORT_Z int *hostInfoLen )
	{
	BYTE stringBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	char portBuffer[ 16 + 8 ];
	int stringLength, port, portLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( hostInfo, hostInfoMaxLen ) );
	assert( isWritePtr( hostInfoLen, sizeof( int ) ) );

	REQUIRES( hostInfoMaxLen >= 16 && \
			  hostInfoMaxLen < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	memset( hostInfo, 0, hostInfoMaxLen );
	*hostInfoLen = 0;

	/* Get the host and port:

		string	host
		uint32	port */
	status = readString32( stream, stringBuffer, CRYPT_MAX_TEXTSIZE - 4,
						   &stringLength );
	if( cryptStatusError( status ) || \
		stringLength <= 0 || stringLength > CRYPT_MAX_TEXTSIZE - 4 )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid host name value" ) );
		}
	if( stringLength > hostInfoMaxLen )
		{
		/* Limit the returned size to the maximum requested by the caller */
		stringLength = hostInfoMaxLen;
		}
	status = port = readUint32( stream );
	if( cryptStatusError( status ) || port <= 0 || port >= 65535L )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid port number value" ) );
		}

	/* Convert the information into string form for the caller to process.  
	   Note that although we limit the maximum string length to 8 bytes the
	   buffer is actually declared as 16 bytes in size for those systems 
	   that don't have sprintf_s() */
	portLength = sprintf_s( portBuffer, 8, ":%d", port );
	memcpy( hostInfo, stringBuffer, stringLength );
	if( stringLength + portLength <= hostInfoMaxLen )
		{
		memcpy( hostInfo + stringLength, portBuffer, portLength );
		stringLength += portLength;
		}
	*hostInfoLen = stringLength;

	return( CRYPT_OK );
	}

/* Add or clear host name/address and port information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
static int getAddressAndPort( INOUT SESSION_INFO *sessionInfoPtr, 
							  INOUT STREAM *stream,
							  OUT_BUFFER( hostInfoMaxLen, *hostInfoLen ) \
									char *hostInfo, 
							  IN_LENGTH_SHORT_MIN( 16 ) const int hostInfoMaxLen,
							  OUT_LENGTH_SHORT_Z int *hostInfoLen )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( hostInfo, hostInfoMaxLen ) );
	assert( isWritePtr( hostInfoLen, sizeof( int ) ) );

	REQUIRES( hostInfoMaxLen >= 16 && \
			  hostInfoMaxLen < MAX_INTLENGTH_SHORT );

	/* Read the address and port information */
	status = readAddressAndPort( sessionInfoPtr, stream, hostInfo,
								 hostInfoMaxLen, hostInfoLen );
	if( cryptStatusError( status ) )
		return( status );
	if( getChannelStatusByAddr( sessionInfoPtr, hostInfo, \
								*hostInfoLen ) != CHANNEL_NONE )
		{
		/* We're adding new forwarding information, if it already exists 
		   this is an error */
		retExt( CRYPT_ERROR_DUPLICATE,
				( CRYPT_ERROR_DUPLICATE, SESSION_ERRINFO, 
				  "Received duplicate request for existing host/port %s",
				  sanitiseString( hostInfo, *hostInfoLen, *hostInfoLen ) ) );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int clearAddressAndPort( INOUT SESSION_INFO *sessionInfoPtr, 
								INOUT STREAM *stream )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

#if 0	/* This is a somewhat special-case function in that it does't apply
		   to an open channel but to a past request for forwarding that
		   exists outside of the normal attribute space.  Until this type of
		   functionality is explicitly requested by users we don't handle 
		   this special-case non-attribute data setting */
	SSH_CHANNEL_INFO *channelInfoPtr;
	char hostInfo[ CRYPT_MAX_TEXTSIZE + 8 ];
	int hostInfoLen, status;

	/* Read the address and port information */
	status = readAddressAndPort( sessionInfoPtr, stream, hostInfo, 
								 hostInfoMaxLen, &hostInfoLen );
	if( cryptStatusError( status ) )
		return( status );
	return( deleteChannelAddr( sessionInfoPtr, addrInfo, addrInfoLen ) );
#else
	return( CRYPT_OK );
#endif /* 0 */
	}
#endif /* USE_SSH_EXTENDED */

/* Send a response to a global/channel request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendChannelResponse( INOUT SESSION_INFO *sessionInfoPtr,
								IN const long channelNo,
								const BOOLEAN isSuccessful )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( ( channelNo == CRYPT_USE_DEFAULT ) || \
			  ( channelNo >= 0 && channelNo <= LONG_MAX ) );

	/* Indicate that the request succeeded/was denied:

		byte	type = SSH_MSG_CHANNEL/GLOBAL_SUCCESS/FAILURE
		uint32	channel_no */
	status = enqueueResponse( sessionInfoPtr,
				isSuccessful ? SSH_MSG_CHANNEL_SUCCESS : \
							   SSH_MSG_CHANNEL_FAILURE, 1,
				( channelNo == CRYPT_USE_DEFAULT ) ? \
					getCurrentChannelNo( sessionInfoPtr, CHANNEL_READ ) : \
					channelNo,
				CRYPT_UNUSED, CRYPT_UNUSED, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( status );
	return( sendEnqueuedResponse( sessionInfoPtr ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendGlobalResponse( INOUT SESSION_INFO *sessionInfoPtr,
							   const BOOLEAN isSuccessful )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Indicate that the request succeeded/was denied:

		byte	type = SSH_MSG_CHANNEL/GLOBAL_SUCCESS/FAILURE */
	status = enqueueResponse( sessionInfoPtr,
				isSuccessful ? SSH_MSG_GLOBAL_SUCCESS : \
							   SSH_MSG_GLOBAL_FAILURE, 0,
				CRYPT_UNUSED, CRYPT_UNUSED, CRYPT_UNUSED,
				CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( status );
	return( sendEnqueuedResponse( sessionInfoPtr ) );
	}

/****************************************************************************
*																			*
*							Channel Open Management							*
*																			*
****************************************************************************/

/* Process a channel open.  These are server-only messages so in theory we
   could immediately reject them in higher-level code if we're the client
   but unfortunately we have to process at least some of the message in 
   order to obtain a channel number to use in the response */

static const CHANNEL_TYPE_INFO FAR_BSS channelInfo[] = {
	{ "session", 7, FALSE },
#ifdef USE_SSH_EXTENDED
	{ "direct-tcpip", 12, TRUE },
#endif /* USE_SSH_EXTENDED */
	{ NULL, 0, FALSE }, { NULL, 0, FALSE }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendOpenResponseFailed( INOUT SESSION_INFO *sessionInfoPtr,
								   IN const long channelNo )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( channelNo >= 0 && channelNo <= LONG_MAX );

	/* Indicate that the request was denied:

		byte	SSH_MSG_CHANNEL_OPEN_FAILURE
		uint32	recipient_channel
		uint32	reason_code = SSH_OPEN_ADMINISTRATIVELY_PROHIBITED
		string	additional_text = ""
		string	language_tag = ""

	   We always send the same reason code to avoid giving away anything
	   to an attacker */
	status = enqueueResponse( sessionInfoPtr,
							  SSH_MSG_CHANNEL_OPEN_FAILURE, 4,
							  channelNo,
							  SSH_OPEN_ADMINISTRATIVELY_PROHIBITED,
							  0, 0 );
	if( cryptStatusOK( status ) )
		status = sendEnqueuedResponse( sessionInfoPtr );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processChannelOpen( INOUT SESSION_INFO *sessionInfoPtr, 
						INOUT STREAM *stream )
	{
	const CHANNEL_TYPE_INFO *channelInfoPtr = NULL;
	BYTE typeString[ CRYPT_MAX_TEXTSIZE + 8 ];
#ifdef USE_SSH_EXTENDED
	BYTE arg1String[ CRYPT_MAX_TEXTSIZE + 8 ];
#endif /* USE_SSH_EXTENDED */
	BYTE *arg1Ptr = NULL;
	long channelNo;
	int typeLen, arg1Len = 0, maxPacketSize, i, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Read the channel open request (the type has already been read by the
	   caller):

	  [	byte	type = SSH_MSG_CHANNEL_OPEN ]
		string	channel_type = "session" | "direct-tcpip"
		uint32	sender_channel
		uint32	initial_window_size
		uint32	max_packet_size
	  [ string	host_to_connect		- For port-forwarding
		uint32	port_to_connect
		string	originator_IP_address
		uint32	originator_port ]

	   As with global/channel requests in processChannelOpen() we can't
	   return an error indication if we encounter a problem too early in the
	   packet, see the comment for that function for further details */
	status = readString32( stream, typeString, CRYPT_MAX_TEXTSIZE, 
						   &typeLen );
	if( cryptStatusError( status ) || \
		typeLen <= 0 || typeLen > CRYPT_MAX_TEXTSIZE )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid channel type in channel open message" ) );
		}

	/* Try and identify the channel type */
	for( i = 0; channelInfo[ i ].channelName != NULL && \
				i < FAILSAFE_ARRAYSIZE( channelInfo, CHANNEL_TYPE_INFO ); 
		 i++ )
		{
		if( typeLen == channelInfo[ i ].channelNameLength && \
			!memcmp( typeString, channelInfo[ i ].channelName,
					 typeLen ) )
			{
			channelInfoPtr = &channelInfo[ i ];
			break;
			}
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( channelInfo, CHANNEL_TYPE_INFO ) );
	if( channelInfoPtr == NULL )
		{
		/* It's an unsupported channel open type, report it as an error */
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid channel-open channel type '%s'", 
				  sanitiseString( typeString, CRYPT_MAX_TEXTSIZE, 
								  typeLen ) ) );
		}
	channelNo = readUint32( stream );
	readUint32( stream );			/* Skip window size */
	status = maxPacketSize = readUint32( stream );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid '%s' channel parameters", 
				  channelInfoPtr->channelName ) );
		}
	if( maxPacketSize < 1024 || maxPacketSize > 0x100000L )
		{
		/* General sanity check to make sure that the packet size is in the
		   range 1K ... 1MB.  We've finally got valid packet data so we can
		   send error responses from now on */
		( void ) sendOpenResponseFailed( sessionInfoPtr, channelNo );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid '%s' channel maximum packet size parameter "
				  "value %d, should be 1K...1MB", 
				  channelInfoPtr->channelName, maxPacketSize ) );
		}
#ifdef USE_SSH_EXTENDED
	if( channelInfoPtr->isPortForwarding )
		{
		/* Get the source and destination host information */
		status = getAddressAndPort( sessionInfoPtr, stream,
									arg1String, CRYPT_MAX_TEXTSIZE, 
									&arg1Len );
		if( cryptStatusError( status ) )
			{
			( void ) sendOpenResponseFailed( sessionInfoPtr, channelNo );
			return( status );
			}
		arg1Ptr = arg1String;
		}
#endif /* USE_SSH_EXTENDED */
	maxPacketSize = min( maxPacketSize, \
						 sessionInfoPtr->receiveBufSize - EXTRA_PACKET_SIZE );

	/* If this is the client then opening a new channel by the server isn't
	   permitted */
	if( !isServer( sessionInfoPtr ) )
		{
		( void ) sendOpenResponseFailed( sessionInfoPtr, channelNo );
		retExt( CRYPT_ERROR_PERMISSION,
				( CRYPT_ERROR_PERMISSION, SESSION_ERRINFO, 
				  "Server attempted to a open a '%s' channel to the client",
				  channelInfoPtr->channelName ) );
		}

	ENSURES( isServer( sessionInfoPtr ) );

	/* Add the new channel */
	status = addChannel( sessionInfoPtr, channelNo, maxPacketSize,
						 typeString, typeLen, arg1Ptr, arg1Len );
	if( cryptStatusError( status ) )
		{
		( void ) sendOpenResponseFailed( sessionInfoPtr, channelNo );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't add new '%s' channel %lX", 
				  channelInfoPtr->channelName, channelNo ) );
		}

	/* Send back the open confirmation:

		byte	type = SSH_MSG_CHANNEL_OPEN_CONFIRMATION
		uint32	recipient_channel = prev. sender_channel
		uint32	sender_channel
		uint32	initial_window_size = MAX_WINDOW_SIZE
		uint32	max_packet_size = bufSize

	   The SSH spec doesn't really explain the semantics of the server's
	   response to the channel open command, in particular whether the
	   returned data size parameters are merely a confirmation of the
	   client's requested values or whether the server is allowed to further
	   modify them to suit its own requirements (or perhaps one is for send
	   and the other for receive?).  In the absence of any further guidance
	   we try and comply with a client's request for smaller data
	   quantities, but also return a smaller-than-requested data size value
	   if they ask for too much data.

	   See the comments in the client-side channel-open code for the reason
	   for the window size */
	status = enqueueResponse( sessionInfoPtr,
							  SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 4,
							  channelNo, channelNo,
							  getWindowSize( sessionInfoPtr ), 
							  maxPacketSize );
	if( cryptStatusOK( status ) )
		status = sendEnqueuedResponse( sessionInfoPtr );
	if( cryptStatusError( status ) )
		{
		/* Since we're already in an error state we can't do much more if 
		   the cleanup from the failed operation fails */
		( void ) deleteChannel( sessionInfoPtr, channelNo, CHANNEL_BOTH, 
								TRUE );
		return( status );
		}

	/* The channel has been successfully created, mark it as active and
	   select it for future exchanges */
	status = setChannelExtAttribute( sessionInfoPtr, SSH_ATTRIBUTE_ACTIVE,
									 TRUE );
	if( cryptStatusOK( status ) )
		{
		const int windowSize = getWindowSize( sessionInfoPtr );

		/* The initial window count is the same as the data window size */
		status = setChannelExtAttribute( sessionInfoPtr, 
										 SSH_ATTRIBUTE_WINDOWSIZE,
										 windowSize );
		if( cryptStatusOK( status ) )
			status = setChannelExtAttribute( sessionInfoPtr, 
											 SSH_ATTRIBUTE_WINDOWCOUNT,
											 windowSize );
		}
	if( cryptStatusOK( status ) )
		status = selectChannel( sessionInfoPtr, channelNo, CHANNEL_BOTH );
	return( status );
	}

/****************************************************************************
*																			*
*							Channel Request Management						*
*																			*
****************************************************************************/

/* Process a global or channel request */

static const REQUEST_TYPE_INFO FAR_BSS requestInfo[] = {
	/* Channel/session-creation requests, only permitted on the server-side */
	{ "pty-req", 7, REQUEST_PTY, REQUEST_FLAG_NONE },
	{ "shell", 5, REQUEST_SHELL, REQUEST_FLAG_TERMINAL },
#ifdef USE_SSH_EXTENDED
	{ "subsystem", 9, REQUEST_SUBSYSTEM, REQUEST_FLAG_TERMINAL },
	{ "tcpip-forward", 13, REQUEST_PORTFORWARD, REQUEST_FLAG_NONE },
	{ "cancel-tcpip-forward", 20, REQUEST_PORTFORWARD_CANCEL, REQUEST_FLAG_NONE },
	{ "exec", 4, REQUEST_EXEC, REQUEST_FLAG_TERMINAL },
#endif /* USE_SSH_EXTENDED */

	/* No-op requests */
	{ "env", 3, REQUEST_NOOP, REQUEST_FLAG_NONE },
	{ "exit-signal", 11, REQUEST_NOOP, REQUEST_FLAG_NONE },
	{ "exit-status", 11, REQUEST_NOOP, REQUEST_FLAG_NONE },
	{ "signal", 6, REQUEST_NOOP, REQUEST_FLAG_NONE },
	{ "xon-xoff", 8, REQUEST_NOOP, REQUEST_FLAG_NONE },
	{ "window-change", 13, REQUEST_NOOP, REQUEST_FLAG_NONE },

	/* Disallowed requests */
	{ "x11-req", 7, REQUEST_DISALLOWED, REQUEST_FLAG_NONE },
	{ NULL, 0, REQUEST_NONE, REQUEST_FLAG_NONE },
		{ NULL, 0, REQUEST_NONE, REQUEST_FLAG_NONE }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processChannelRequest( INOUT SESSION_INFO *sessionInfoPtr,
						   INOUT STREAM *stream, 
						   IN const long prevChannelNo )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	const REQUEST_TYPE_INFO *requestInfoPtr = NULL;
	const BOOLEAN isChannelRequest = \
			( sshInfo->packetType == SSH_MSG_CHANNEL_REQUEST ) ? TRUE : FALSE;
	BYTE stringBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BOOLEAN wantReply, requestOK = TRUE;
	int stringLength, i, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( prevChannelNo >= 0 && prevChannelNo <= LONG_MAX );

	/* Process the channel/global request (the type and channel number
	   have already been read by the caller):

	  [	byte	type = SSH_MSG_CHANNEL_REQUEST / SSH_MSG_GLOBAL_REQUEST ]
	  [	uint32	recipient_channel	- For channel reqs ]
		string	request_type
		boolean	want_reply
		[...]

	   If there's an error at this point we can't send back a response
	   because one or both of the channel number and the want_reply flag
	   aren't available yet.  The consensus among SSH implementors was that
	   not doing anything if the request packet is invalid is preferable to
	   sending back a response with a placeholder channel number or a
	   response when want_reply could have been false had it been able to
	   be decoded */
	readString32( stream, stringBuffer, CRYPT_MAX_TEXTSIZE, &stringLength );
	status = wantReply = sgetc( stream );
	if( cryptStatusError( status ) || \
		stringLength <= 0 || stringLength > CRYPT_MAX_TEXTSIZE  )
		{
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid request type in %s request packet",
				  isChannelRequest ? "channel" : "global" ) );
		}

	/* Try and identify the request type */
	for( i = 0; requestInfo[ i ].requestName != NULL && \
				i < FAILSAFE_ARRAYSIZE( requestInfo, REQUEST_TYPE_INFO ); 
		 i++ )
		{
		if( stringLength == requestInfo[ i ].requestNameLength && \
			!memcmp( stringBuffer, requestInfo[ i ].requestName,
					 stringLength ) )
			{
			requestInfoPtr = &requestInfo[ i ];
			break;
			}
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( requestInfo, REQUEST_TYPE_INFO ) );

	/* If it's an explicitly disallowed request type or if we're the client
	   and it's anything other than a no-op request (for example a request
	   to execute a command or perform port forwarding) then it isn't 
	   permitted */
	if( requestInfoPtr == NULL || \
		requestInfoPtr->requestType == REQUEST_DISALLOWED || \
		( !isServer( sessionInfoPtr ) && \
		  requestInfoPtr->requestType != REQUEST_NOOP ) )
		{
		/* If the other side doesn't want a response to their request, we're 
		   done */
		if( !wantReply )
			return( CRYPT_OK );

		/* Send a request-denied response to the other side's request */
		if( isChannelRequest )
			{
			int localStatus;

			/* The request failed, go back to the previous channel */
			status = sendChannelResponse( sessionInfoPtr, prevChannelNo,
										  FALSE );
			localStatus = selectChannel( sessionInfoPtr, prevChannelNo, 
										 CHANNEL_READ );
			if( cryptStatusOK( status ) )
				status = localStatus;
			}
		else
			status = sendGlobalResponse( sessionInfoPtr, FALSE );
		return( status );
		}

	ENSURES( requestInfoPtr != NULL && \
			 ( isServer( sessionInfoPtr ) || \
			   ( requestInfoPtr->requestType == REQUEST_NOOP ) ) );

	/* Process the request.  Since these are administrative messages that
	   aren't visible to the caller we don't bail out if we encounter a
	   problem but just deny the request */
	switch( requestInfoPtr->requestType )
		{
		case REQUEST_SHELL:
		case REQUEST_PTY:
		case REQUEST_NOOP:
			/* Generic requests containing extra information that we're not
			   interested in */
			break;

#ifdef USE_SSH_EXTENDED
		case REQUEST_EXEC:
			/* A further generic request that we're not interested in */
			break;

		case REQUEST_SUBSYSTEM:
			/* We're being asked for a subsystem, record the type:

				[...]
				string	subsystem_name */
			status = readString32( stream, stringBuffer, CRYPT_MAX_TEXTSIZE,
								   &stringLength );
			if( cryptStatusError( status ) || \
				stringLength <= 0 || stringLength > CRYPT_MAX_TEXTSIZE )
				requestOK = FALSE;
			else
				{
				/* The handling of subsystems is somewhat awkward, instead
				   of opening a subsystem channel SSH first opens a standard
				   session channel and then layers a subsystem on top of it.
				   Because of this we have to replace the standard channel
				   type with a new subsystem channel-type as well as recording
				   the subsystem type */
				status = setChannelAttributeS( sessionInfoPtr,
											   CRYPT_SESSINFO_SSH_CHANNEL_TYPE,
											   "subsystem", 9 );
				if( cryptStatusOK( status ) )
					{
					status = setChannelAttributeS( sessionInfoPtr,
												   CRYPT_SESSINFO_SSH_CHANNEL_ARG1,
												   stringBuffer, 
												   stringLength );
					}
				ENSURES( cryptStatusOK( status ) );
				}
			break;

		case REQUEST_PORTFORWARD:
			/* We're being asked for port forwarding, get the address and
			   port information:

				[...]
				string	local_address_to_bind (e.g. "0.0.0.0")
				uint32	local_port_to_bind */
			status = getAddressAndPort( sessionInfoPtr, stream, stringBuffer,
										CRYPT_MAX_TEXTSIZE, &stringLength );
			if( cryptStatusError( status ) )
				requestOK = FALSE;
			else
				{
#if 0			/* This is a global request that doesn't apply to any
				   channel which makes it rather hard to deal with since
				   we can't associate it with anything that the user can
				   work with.  For now we leave it until there's actual
				   user demand for it */
				setChannelAttribute( sessionInfoPtr,
									 CRYPT_SESSINFO_SSH_CHANNEL_ARG1,
									 stringBuffer, stringLength );
#endif /* 0 */
				}
			break;

		case REQUEST_PORTFORWARD_CANCEL:
			{
			const int offset = stell( stream );
			int iterationCount;

			/* Check that this is a request to close a port for which
			   forwarding was actually requested.  Since there could be
			   multiple channels open on the forwarded port we keep looking
			   for other channels open on this port until we've cleared them
			   all.  The spec is silent about what happens to open channels
			   when the forwarding is cancelled but from reading between the 
			   lines (new channel-open requests can be received until the 
			   forwarding is cancelled) it appears that the channels remain 
			   active until the channel itself is closed */
			for( requestOK = FALSE, iterationCount = 0;
				 iterationCount < FAILSAFE_ITERATIONS_MED;
				 iterationCount++ )
				{
				sseek( stream, offset );
				status = clearAddressAndPort( sessionInfoPtr, stream );
				if( cryptStatusError( status ) )
					break;
				requestOK = TRUE;
				}
			ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
			break;
			}
#endif /* USE_SSH_EXTENDED */

		case REQUEST_DISALLOWED:
		default:
			/* Anything else we don't allow */
			requestOK = FALSE;
			break;
		}

	/* Acknowledge the request if necessary */
	if( wantReply )
		{
		if( isChannelRequest )
			{
			status = sendChannelResponse( sessionInfoPtr, prevChannelNo, 
										  requestOK );
			if( cryptStatusError( status ) || !requestOK )
				{
				/* The request failed, go back to the previous channel */
				status = selectChannel( sessionInfoPtr, prevChannelNo,
										CHANNEL_READ );
				}
			}
		else
			status = sendGlobalResponse( sessionInfoPtr, requestOK );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If this request ends the negotiation, let the caller know */
	return( ( requestInfoPtr->flags & REQUEST_FLAG_TERMINAL ) ? \
			OK_SPECIAL : CRYPT_OK );
	}
#endif /* USE_SSH */
