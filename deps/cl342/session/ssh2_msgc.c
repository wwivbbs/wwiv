/****************************************************************************
*																			*
*				cryptlib SSHv2 Client-side Channel Message Management		*
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

/* The type of a channel-open request and the type of service that we're
   requesting */

typedef enum { OPENREQUEST_NONE, /* OPENREQUEST_STANDALONE, */
			   OPENREQUEST_CHANNELONLY, OPENREQUEST_SESSION,
			   OPENREQUEST_LAST } OPENREQUEST_TYPE;

#ifdef USE_SSH_EXTENDED
typedef enum { SERVICE_NONE, SERVICE_SHELL, SERVICE_PORTFORWARD, 
			   SERVICE_SUBSYSTEM, SERVICE_EXEC, SERVICE_LAST } SERVICE_TYPE;
#else
typedef enum { SERVICE_NONE, SERVICE_SHELL, SERVICE_LAST } SERVICE_TYPE;
#endif /* USE_SSH_EXTENDED */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#ifdef USE_SSH_EXTENDED

/* Determine which type of service the caller requested */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getServiceType( INOUT SESSION_INFO *sessionInfoPtr, 
						   OUT_ENUM_OPT( SERVICE ) SERVICE_TYPE *serviceType )
	{
	BYTE typeString[ CRYPT_MAX_TEXTSIZE + 8 ];
	int typeLen, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( serviceType, sizeof( SERVICE_TYPE ) ) );

	/* Clear return value */
	*serviceType = SERVICE_NONE;

	/* Get the information that's needed for the channel that we're about 
	   to create */
	status = getChannelAttributeS( sessionInfoPtr, 
								   CRYPT_SESSINFO_SSH_CHANNEL_TYPE,
								   typeString, CRYPT_MAX_TEXTSIZE,  
								   &typeLen );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Missing channel type for channel activation" ) );
		}
	if( !strCompare( typeString, "subsystem", 9 ) )
		{
		*serviceType = SERVICE_SUBSYSTEM;
		return( CRYPT_OK );
		}
	if( !strCompare( typeString, "direct-tcpip", 12 ) || \
		!strCompare( typeString, "forwarded-tcpip", 15 ) )
		{
		*serviceType = SERVICE_PORTFORWARD;
		return( CRYPT_OK );
		}
	if( !strCompare( typeString, "exec", 4 ) )
		{
		*serviceType = SERVICE_EXEC;
		return( CRYPT_OK );
		}

	/* The default is a just a straight pipe from A to B, a shell in SSH 
	   thinking */
	*serviceType = SERVICE_SHELL;
	return( CRYPT_OK );
	}
#else
  #define getServiceType( sessionInfoPtr, serviceType ) \
		  CRYPT_OK; \
		  *( serviceType ) = SERVICE_SHELL
#endif /* USE_SSH_EXTENDED */

/* Get information on why a channel-open failed */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getOpenFailInfo( INOUT SESSION_INFO *sessionInfoPtr,
							INOUT STREAM *stream )
	{
	BYTE stringBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	int stringLen, errorCode, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* The channel open failed, tell the caller why:

		byte	SSH_MSG_CHANNEL_OPEN_FAILURE
		uint32	recipient_channel
		uint32	reason_code
		string	additional_text */
	readUint32( stream );		/* Skip channel number */
	errorCode = readUint32( stream );
	status = readString32( stream, stringBuffer, CRYPT_MAX_TEXTSIZE, 
						   &stringLen );
	if( cryptStatusError( status ) || \
		stringLen <= 0 || stringLen > CRYPT_MAX_TEXTSIZE )
		{
		/* No error message, the best that we can do is give the reason code 
		   (or the stream status if we couldn't even get that) as part of 
		   the message */
		retExt( CRYPT_ERROR_OPEN,
				( CRYPT_ERROR_OPEN, SESSION_ERRINFO, 
				  "Channel open failed, reason code %d", 
				  errorCode ) );
		}
	retExt( CRYPT_ERROR_OPEN,
			( CRYPT_ERROR_OPEN, SESSION_ERRINFO, 
			  "Channel open failed, error message '%s'",
			  sanitiseString( stringBuffer, CRYPT_MAX_TEXTSIZE, 
							  stringLen ) ) );
	}

/****************************************************************************
*																			*
*							Client-side Channel Management					*
*																			*
****************************************************************************/

/* Create a request for the appropriate type of service, either encrypted-
   telnet, SFTP (or more generically a subsystem), or port forwarding.
   There are several different port-forwarding mechanisms that we can use.
   A global request of type "tcpip-forward" requests forwarding of a remote
   port to the local system, specifying the remote port to be forwarded but
   without actually opening a session/channel, it's merely a request for
   future forwarding.  When a connection arrives on the remote port for
   which forwarding has been requested the remote system opens a channel of
   type "forwarded-tcpip" to the local system.  To open a connection from a
   locally-forwarded port to a port on the remote system the local system
   opens a channel of type "direct-tcpip" to the remote system:

	Pkt		Name			Arg1			Arg2		Comment
	---		----			----			----		-------
	open	"session"									Followed by pty-req
														or subsys
	open	"fwded-tcpip"	remote_info (in)			Server -> client in
														response.to tcpip-fd
	open	"direct-tcpip"	remote_info		local_info	Client -> server, currently
														local_info = 127.0.0.1
	global	"tcpip-fwd"		remote_info (out)			Request for remote
														forwarding

   Once we've opened a standard session we need to follow it with either a
   pty-request + shell request or a subsystem request:

	Pkt		Name			Arg1			Arg2		Comment
	---		----			----			----		-------
	channel	"pty-req"
	channel "subsystem"		name

   In theory we could bundle the channel open + pty-request + shell request
   into a single packet group to save round-trips but the packets sent after
   the channel open require the use of the receive-channel number supplied by
   the remote system.  This is usually the same as the send channel that we
   specify but for some unknown reason Cisco use different send and receive
   channel numbers, requiring that we wait for the response to the channel-
   open before we send any subsequent packets, which adds yet another RTT to 
   the exchange */

#ifdef USE_SSH_EXTENDED 

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int createOpenRequest( INOUT SESSION_INFO *sessionInfoPtr, 
							  OUT STREAM *stream,
							  IN_ENUM( SERVICE ) const SERVICE_TYPE serviceType,
							  OUT_ENUM_OPT( OPENREQUEST ) \
									OPENREQUEST_TYPE *requestType )
	{
	const long channelNo = getCurrentChannelNo( sessionInfoPtr,
												CHANNEL_WRITE );
	const int maxPacketSize = sessionInfoPtr->sendBufSize - \
							  EXTRA_PACKET_SIZE;
	URL_INFO urlInfo = { DUMMY_INIT };
	BYTE arg1String[ CRYPT_MAX_TEXTSIZE + 8 ];
	int arg1Len = DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( requestType, sizeof( OPENREQUEST_TYPE ) ) );

	REQUIRES( serviceType > SERVICE_NONE && serviceType < SERVICE_LAST );

	/* Clear return value */
	*requestType = OPENREQUEST_NONE;

	/* If it's not a generic tunnel, get any additional parameters 
	   required */
	if( serviceType != SERVICE_SHELL )
		{
		status = getChannelAttributeS( sessionInfoPtr,
									   CRYPT_SESSINFO_SSH_CHANNEL_ARG1,
									   arg1String, CRYPT_MAX_TEXTSIZE, 
									   &arg1Len );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Missing channel argument (%s) for channel "
					  "activation", 
					  ( serviceType == SERVICE_PORTFORWARD ) ? \
						"host name/port" : \
					  ( serviceType == SERVICE_EXEC ) ? \
						"command" : \
						"subsystem name" ) );
			}
		}

	/* If we know that the argument is a URL (rather than a subsystem name 
	   or command), check its validity */
	if( serviceType == SERVICE_PORTFORWARD )
		{
		status = sNetParseURL( &urlInfo, arg1String, arg1Len, URL_TYPE_SSH );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Invalid channel argument (host name/port) for "
					  "channel activation" ) );
			}

		/* Tell the caller what to do after the channel open */
		*requestType = OPENREQUEST_CHANNELONLY;
		}
	else
		{
		/* Set the request type to tell the caller what to do after they've
		   sent the initial channel open */
		*requestType = OPENREQUEST_SESSION;
		}

#if 0	/* 17/9/04 This is a complex mechanism that requires the use of an
				   interactive or scriptable tool to use, until someone 
				   really needs this we don't implement it */
	/* Request forwarding of a port from the remote system to the local one.  
	   Once a connection arrives on the remote port it'll open a channel to 
	   the local system of type "forwarded-tcpip".  Since this isn't a 
	   normal channel open, we return a special status to let the caller 
	   know that there's nothing further to do */
	if( serviceType == SERVICE_PORTFORWARD_REQUEST )
		{
		URL_INFO urlInfo;

		*requestType = OPENREQUEST_STANDALONE;

		/*	byte	type = SSH_MSG_GLOBAL_REQUEST
			string	request_name = "tcpip-forward"
			boolean	want_reply = FALSE
			string	remote_address_to_bind (e.g. "0.0.0.0")
			uint32	remote_port_to_bind

		   Since this is a special-case request-only message we let the
		   caller know that they don't have to proceed further with the
		   channel-open */
		status = openPacketStreamSSH( stream, SSH_MSG_GLOBAL_REQUEST,
									  &packetOffset );
		if( cryptStatusError( status ) )
			return( status );
		writeString32( stream, "tcpip-forward", 13 );
		sputc( stream, 0 );
		writeString32( stream, urlInfo.host, urlInfo.hostLen );
		writeUint32( stream, urlInfo.port );
		return( wrapPacketSSH2( sessionInfoPtr, stream, packetOffset ) );
		}
#endif /* 0 */

	/* Send a channel open:

		byte	type = SSH_MSG_CHANNEL_OPEN
		string	channel_type
		uint32	sender_channel
		uint32	initial_window_size = MAX_WINDOW_SIZE
		uint32	max_packet_size = bufSize
		...

	   The use of security protocol-level flow control when there's already
	   a far better, heavily analysed and field-tested network protocol-
	   level flow control mechanism present is just stupid.  All it does is 
	   create a performance handbrake where throughput can be reduced by as 
	   much as an order of magnitude due to SSH's "flow-control" getting in 
	   the way (Putty even has an FAQ entry "Why is SFTP so much slower than 
	   scp?", for which the correct answer should be "It's the SSH-level 
	   flow-control braindamage").  For this reason cryptlib always 
	   advertises a maximum window size (effectively disabling the SSH-level 
	   flow control) and lets the network stack and network hardware take 
	   care of flow control, as they should.  Unfortunately some buggy 
	   implementations break when sent a window size over a certain limit 
	   in which case we have to limit the window size, thus reintroducing 
	   the performance handbrake when dealing with these buggy 
	   implementations, see the comments for the window handling in 
	   ssh2_msg.c for details */
	status = openPacketStreamSSH( stream, sessionInfoPtr, 
								  SSH_MSG_CHANNEL_OPEN );
	if( cryptStatusError( status ) )
		return( status );
	if( serviceType == SERVICE_SUBSYSTEM || serviceType == SERVICE_EXEC )
		{
		/* A subsystem is an additional layer on top of the standard
		   channel so we have to open the channel first and then add the
		   subsystem later via a channel request rather than opening it
		   directly.  An exec is a special case that works like the default
		   type of session operation, "shell", but doesn't go via a pty */
		writeString32( stream, "session", 7 );
		}
	else
		{
		if( serviceType == SERVICE_PORTFORWARD )
			writeString32( stream, "direct-tcpip", 12 );
		else
			{
			ENSURES( serviceType == SERVICE_SHELL );

			/* It's a generic secure-tunnel that'll be followed by a pty-
			   request and shell */
			writeString32( stream, "session", 7 );
			}
		}
	writeUint32( stream, channelNo );
	writeUint32( stream, getWindowSize( sessionInfoPtr ) );
	status = writeUint32( stream, maxPacketSize );
	if( serviceType == SERVICE_PORTFORWARD )
		{
		/* The caller has requested a port-forwarding channel open, continue
		   the basic channel-open packet with port-forwarding information:

			...
			string	remote_host_to_connect
			uint32	rempte_port_to_connect
			string	local_originator_IP_address
			uint32	local_originator_port */
		writeString32( stream, urlInfo.host, urlInfo.hostLen );
		writeUint32( stream, urlInfo.port );
		writeString32( stream, "127.0.0.1", 9 );
		status = writeUint32( stream, 22 );
		}
	if( cryptStatusOK( status ) )
		status = wrapPacketSSH2( sessionInfoPtr, stream, 0, FALSE, TRUE );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	return( CRYPT_OK );
	}

#else

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int createOpenRequest( INOUT SESSION_INFO *sessionInfoPtr, 
							  OUT STREAM *stream,
							  IN_ENUM( SERVICE ) const SERVICE_TYPE serviceType,
							  OUT_ENUM_OPT( OPENREQUEST ) \
									OPENREQUEST_TYPE *requestType )
	{
	const long channelNo = getCurrentChannelNo( sessionInfoPtr,
												CHANNEL_WRITE );
	const int maxPacketSize = sessionInfoPtr->sendBufSize - \
							  EXTRA_PACKET_SIZE;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( requestType, sizeof( OPENREQUEST_TYPE ) ) );

	REQUIRES( serviceType > SERVICE_NONE && serviceType < SERVICE_LAST );

	/* Set the request type to tell the caller what to do after they've sent 
	   the initial channel open */
	*requestType = OPENREQUEST_SESSION;

	/* Send a channel open:

		byte	type = SSH_MSG_CHANNEL_OPEN
		string	channel_type
		uint32	sender_channel
		uint32	initial_window_size = MAX_WINDOW_SIZE
		uint32	max_packet_size = bufSize
		...

	   The use of security protocol-level flow control when there's already
	   a far better, heavily analysed and field-tested network protocol-
	   level flow control mechanism present is just stupid.  All it does is 
	   create a performance handbrake where throughput can be reduced by as 
	   much as an order of magnitude due to SSH's "flow-control" getting in 
	   the way (Putty even has an FAQ entry "Why is SFTP so much slower than 
	   scp?", for which the correct answer should be "It's the SSH-level 
	   flow-control braindamage").  For this reason cryptlib always 
	   advertises a maximum window size (effectively disabling the SSH-level 
	   flow control) and lets the network stack and network hardware take 
	   care of flow control, as they should.  Unfortunately some buggy 
	   implementations break when sent a window size over a certain limit 
	   in which case we have to limit the window size, thus reintroducing 
	   the performance handbrake when dealing with these buggy 
	   implementations, see the comments for the window handling in 
	   ssh2_msg.c for details */
	status = openPacketStreamSSH( stream, sessionInfoPtr, 
								  SSH_MSG_CHANNEL_OPEN );
	if( cryptStatusError( status ) )
		return( status );
	writeString32( stream, "session", 7 );
	writeUint32( stream, channelNo );
	writeUint32( stream, getWindowSize( sessionInfoPtr ) );
	status = writeUint32( stream, maxPacketSize );
	if( cryptStatusOK( status ) )
		status = wrapPacketSSH2( sessionInfoPtr, stream, 0, FALSE, TRUE );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	return( CRYPT_OK );
	}
#endif /* USE_SSH_EXTENDED */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int createSessionOpenRequest( INOUT SESSION_INFO *sessionInfoPtr,
									 INOUT STREAM *stream,
									 IN_ENUM( SERVICE ) \
										const SERVICE_TYPE serviceType )
	{
	const long channelNo = getCurrentChannelNo( sessionInfoPtr,
												CHANNEL_WRITE );
	int packetOffset, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( serviceType > SERVICE_NONE && serviceType < SERVICE_LAST );

#ifdef USE_SSH_EXTENDED
	/* If the caller has requested the use of a custom subsystem (and at the
	   moment the only one that's likely to be used is SFTP), request this
	   from the server by modifying the channel that we've just opened to
	   run the subsystem */
	if( serviceType == SERVICE_SUBSYSTEM )
		{
		BYTE arg1String[ CRYPT_MAX_TEXTSIZE + 8 ];
		int arg1Len;

		/* Get the subsystem type */
		status = getChannelAttributeS( sessionInfoPtr, 
									   CRYPT_SESSINFO_SSH_CHANNEL_ARG1,
									   arg1String, CRYPT_MAX_TEXTSIZE, 
									   &arg1Len );
		if( cryptStatusError( status ) )
			return( status );

		/*	byte	type = SSH_MSG_CHANNEL_REQUEST
			uint32	recipient_channel
			string	request_name = "subsystem"
			boolean	want_reply = FALSE
			string	subsystem_name */
		status = openPacketStreamSSH( stream, sessionInfoPtr, 
									  SSH_MSG_CHANNEL_REQUEST );
		if( cryptStatusError( status ) )
			return( status );
		writeUint32( stream, channelNo );
		writeString32( stream, "subsystem", 9 );
		sputc( stream, 0 );
		status = writeString32( stream, arg1String, arg1Len );
		if( cryptStatusOK( status ) )
			status = wrapPacketSSH2( sessionInfoPtr, stream, 0, FALSE, TRUE );
		return( status );
		}

	/* If the caller has requested the use of remote command execution (i.e. 
	   an rexec rather than the usual SSH rsh), run the command directly
	   without going via a pty */
	if( serviceType == SERVICE_EXEC )
		{
		BYTE arg1String[ CRYPT_MAX_TEXTSIZE + 8 ];
		int arg1Len;

		/* Get the command to execute */
		status = getChannelAttributeS( sessionInfoPtr, 
									   CRYPT_SESSINFO_SSH_CHANNEL_ARG1,
									   arg1String, CRYPT_MAX_TEXTSIZE, 
									   &arg1Len );
		if( cryptStatusError( status ) )
			return( status );

		/*	byte	type = SSH_MSG_CHANNEL_REQUEST
			uint32	recipient_channel
			string	request_name = "exec"
			boolean	want_reply = FALSE
			string	command */
		status = openPacketStreamSSH( stream, sessionInfoPtr, 
									  SSH_MSG_CHANNEL_REQUEST );
		if( cryptStatusError( status ) )
			return( status );
		writeUint32( stream, channelNo );
		writeString32( stream, "exec", 4 );
		sputc( stream, 0 );
		status = writeString32( stream, arg1String, arg1Len );
		if( cryptStatusOK( status ) )
			status = wrapPacketSSH2( sessionInfoPtr, stream, 0, FALSE, TRUE );
		return( status );
		}
#endif /* USE_SSH_EXTENDED */

	REQUIRES( serviceType == SERVICE_SHELL );

	/* It's a standard channel open:

		byte	type = SSH_MSG_CHANNEL_REQUEST
		uint32	recipient_channel
		string	request_name = "pty-req"
		boolean	want_reply = FALSE
		string	TERM_environment_variable = "xterm"
		uint32	cols = 80
		uint32	rows = 48
		uint32	pixel_width = 0
		uint32	pixel_height = 0
		string	tty_mode_info = ""
		... */
	status = openPacketStreamSSH( stream, sessionInfoPtr, 
								  SSH_MSG_CHANNEL_REQUEST );
	if( cryptStatusError( status ) )
		return( status );
	writeUint32( stream, channelNo );
	writeString32( stream, "pty-req", 7 );
	sputc( stream, 0 );					/* No reply */
	writeString32( stream, "xterm", 5 );/* Generic */
	writeUint32( stream, 80 );
	writeUint32( stream, 48 );			/* 48 x 80 (24 x 80 is so 1970s) */
	writeUint32( stream, 0 );
	writeUint32( stream, 0 );			/* No graphics capabilities */
	status = writeUint32( stream, 0 );	/* No special TTY modes */
	if( cryptStatusOK( status ) )
		status = wrapPacketSSH2( sessionInfoPtr, stream, 0, FALSE, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/*	...
		byte	type = SSH_MSG_CHANNEL_REQUEST
		uint32	recipient_channel
		string	request_name = "shell"
		boolean	want_reply = FALSE

	   This final request, once sent, moves the server into interactive
	   session mode */
	status = continuePacketStreamSSH( stream, SSH_MSG_CHANNEL_REQUEST,
									  &packetOffset );
	if( cryptStatusError( status ) )
		return( status );
	writeUint32( stream, channelNo );
	writeString32( stream, "shell", 5 );
	status = sputc( stream, 0 );			/* No reply */
	if( cryptStatusOK( status ) )
		status = wrapPacketSSH2( sessionInfoPtr, stream, packetOffset, 
								 FALSE, TRUE );
	return( status );
	}

/* Send a channel open */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sendChannelOpen( INOUT SESSION_INFO *sessionInfoPtr )
	{
	STREAM stream;
	SERVICE_TYPE serviceType;
	OPENREQUEST_TYPE requestType;
	const long channelNo = getCurrentChannelNo( sessionInfoPtr,
												CHANNEL_READ );
	long currentChannelNo;
	int length, value, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Make sure that there's channel data available to activate and
	   that it doesn't correspond to an already-active channel */
	if( channelNo == UNUSED_CHANNEL_NO )
		{
		retExt( CRYPT_ERROR_NOTINITED,
				( CRYPT_ERROR_NOTINITED, SESSION_ERRINFO, 
				  "No current channel information available to activate "
				  "channel" ) );
		}
	status = getChannelAttribute( sessionInfoPtr,
								  CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE,
								  &value );
	if( cryptStatusError( status ) || value )
		{
		retExt( CRYPT_ERROR_INITED,
				( CRYPT_ERROR_INITED, SESSION_ERRINFO, 
				  "Current channel has already been activated" ) );
		}

	/* Determine the service type that we'll be using */
	status = getServiceType( sessionInfoPtr, &serviceType );
	if( cryptStatusError( status ) )
		return( status );

	/* Create a request for the appropriate type of service */
	status = createOpenRequest( sessionInfoPtr, &stream, serviceType, 
								&requestType );
	if( cryptStatusError( status ) )
		return( status );

#if 0	/* Never used, see comment in createOpenRequest() for 
		   "tcpip-forward" type */
	/* If it's a request-only message that doesn't open a channel, send it
	   and exit */
	if( requestType == OPENREQUEST_STANDALONE )
		{
		status = sendPacketSSH2( sessionInfoPtr, &stream, TRUE );
		sMemDisconnect( &stream );
		return( status );
		}
#endif /* 0 */

	/* Send the open request to the server.  The SSH spec doesn't really
	   explain the semantics of the server's response to the channel open
	   command, in particular whether the returned data size parameters are
	   merely a confirmation of the client's requested values or whether the
	   server is allowed to further modify them to suit its own requirements
	   (or perhaps one is for send and the other for receive?).  In the
	   absence of any further guidance we just ignore the returned values,
	   which seems to work for all deployed servers */
	status = sendPacketSSH2( sessionInfoPtr, &stream, TRUE );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Wait for the server's ack of the channel open request:

		byte	SSH_MSG_CHANNEL_OPEN_CONFIRMATION
		uint32	recipient_channel
		uint32	sender_channel
		uint32	initial_window_size
		uint32	maximum_packet_size
		... */
	status = length = \
		readHSPacketSSH2( sessionInfoPtr, SSH_MSG_SPECIAL_CHANNEL,
						  ID_SIZE + UINT32_SIZE + UINT32_SIZE + \
							UINT32_SIZE + UINT32_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	if( sessionInfoPtr->sessionSSH->packetType == SSH_MSG_CHANNEL_OPEN_FAILURE )
		{
		/* The open failed, report the details to the user */
		status = getOpenFailInfo( sessionInfoPtr, &stream );
		sMemDisconnect( &stream );
		return( status );
		}
	currentChannelNo = readUint32( &stream );
	if( currentChannelNo != channelNo )
		{
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid channel number %lX in channel open confirmation, "
				  "should be %lX", currentChannelNo, channelNo ) );
		}
	status = currentChannelNo = readUint32( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid channel data in channel open confirmation for "
				  "channel %lX", channelNo ) );
		}

	/* The channel has been successfully created, mark it as active and
	   select it for future exchanges */
	status = setChannelExtAttribute( sessionInfoPtr, SSH_ATTRIBUTE_ACTIVE, 
									 TRUE );
	if( cryptStatusOK( status ) && currentChannelNo != channelNo )
		{
		/* It's unclear why anyone would want to use different channel
		   numbers for different directions since it's the same channel that 
		   the data is moving across, but Cisco do it anyway */
		status = setChannelExtAttribute( sessionInfoPtr, 
										 SSH_ATTRIBUTE_ALTCHANNELNO,
										 currentChannelNo );
		}
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
	if( cryptStatusError( status ) )
		return( status );
	if( requestType == OPENREQUEST_CHANNELONLY )
		{
		/* If we're just opening a new channel in an existing session, we're 
		   done */
		return( CRYPT_OK );
		}
	REQUIRES( requestType == OPENREQUEST_SESSION );

	/* It's a session open request that requires additional messages to do
	   anything useful, create and send the extra packets */
	status = createSessionOpenRequest( sessionInfoPtr, &stream, 
									   serviceType );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream, TRUE );
	sMemDisconnect( &stream );
	return( status );
	}
#endif /* USE_SSH */
