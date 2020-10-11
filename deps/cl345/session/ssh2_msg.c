/****************************************************************************
*																			*
*					cryptlib SSHv2 Control Message Management				*
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

/* Forward declaration for channel-close function */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendChannelClose( INOUT SESSION_INFO *sessionInfoPtr,
							 IN const long channelNo,
							 IN_ENUM( CHANNEL ) const CHANNEL_TYPE channelType,
							 const BOOLEAN closeLastChannel );

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Implement the SSH performance handbrake unless we've managed to disable
   it during the channel-open */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int handleWindowAdjust( INOUT SESSION_INFO *sessionInfoPtr,
							   IN const long channelNo,
							   IN_LENGTH_Z const int length )
	{
	int windowCount, windowSize DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( channelNo >= 0 && channelNo <= CHANNEL_MAX );
	REQUIRES( isIntegerRange( length ) );

	/* Get the window parameters */
	status = getChannelExtAttribute( sessionInfoPtr, 
									 SSH_ATTRIBUTE_WINDOWCOUNT, 
									 &windowCount );
	if( cryptStatusOK( status ) )
		{
		status = getChannelExtAttribute( sessionInfoPtr,
										 SSH_ATTRIBUTE_WINDOWSIZE,
										 &windowSize );
		}
	ENSURES( cryptStatusOK( status ) );

	/* Adjust the data window and communicate changes to the other side if 
	   necessary.  This can get quite complicated because for those 
	   implementations where we can't disable the SSH handbrake we have to 
	   send a constant stream of 1977-vintage XMODEM XON/XOFF messages, but 
	   since there may be a half-assembled packet in the send buffer we
	   can't just dispatch it immediately but have to enqueue it pending
	   availability of the send machinery (it also leads to strange error
	   messages returned to the caller when they get a write failure in
	   response to a read).  Fortunately only a few old buggy 
	   implementations actually reject our disabling of the handbrake so 
	   it's usually disabled (at least for the receive side, which we 
	   control) and in the rare cases where it's present it's being used
	   for things like network router console interfaces for which the
	   traffic is highly interactive so there's a constant flow of packets
	   written to piggyback the XONs onto.

	   The exact strategy for window handling is a bit complex (mostly
	   because this isn't a good way to do things in the first place so it
	   would require horribly complex processing to really handle properly),
	   to keep things simple we just wait until the window size has fallen
	   to half its initial value and then reset it back to the initial
	   value again.  Since this is rarely used except where we can't disable
	   the handbrake it's not really worth introducing a huge amount of 
	   extra complexity to manage it */
	REQUIRES( windowCount > 0 && windowCount <= windowSize );
	windowCount -= length;
	if( windowCount < windowSize / 2 )
		{
		int adjustCount;

		/* Send the window adjust to the remote system:

			byte	SSH_MSG_CHANNEL_WINDOW_ADJUST
			uint32	channel
			uint32	bytes_to_add

		   Unfortunately the error status that we return from a failed 
		   window adjust is going to come as a complete surprise to the 
		   caller because we're supposed to be processing a read and not a 
		   write at this point, the write is only required by SSH's 
		   braindamaged flow-control handling */
		if( windowCount < 0 || windowCount >= windowSize )
			{
			/* We've consumed the remaining window and then some, reset it
			   to it's full size */
			adjustCount = windowSize;
			}
		else
			{
			/* Adjust the window back up to it's full size */
			adjustCount = windowSize - windowCount;
			}
		ENSURES( adjustCount > windowSize / 2 && \
				 adjustCount <= windowSize );
		status = enqueueChannelData( sessionInfoPtr,
									 SSH_MSG_CHANNEL_WINDOW_ADJUST,
									 channelNo, adjustCount );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Error sending SSH window adjust for data flow "
					  "control" ) );
			}

		/* We've reset the window, start again from zero */
		windowCount += adjustCount;
		if( windowCount < windowSize / 2 || windowCount > windowSize )
			{
			retExt( CRYPT_ERROR_INVALID,
					( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
					  "Invalid SSH flow control window count %d, should be "
					  "%d ... %d", windowCount, windowSize / 2, 
					  windowSize ) );
			}
		}
	status = setChannelExtAttribute( sessionInfoPtr,
									 SSH_ATTRIBUTE_WINDOWCOUNT,
									 windowCount );
	ENSURES( cryptStatusOK( status ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							General Channel Management						*
*																			*
****************************************************************************/

/* Get the data window size for a new channel.  Some buggy implementations 
   break when sent a window size over a certain limit in which case we have 
   to limit the window size.  Typically for these implementations for any 
   size over about 8M the server gets slower and slower, eventually more or 
   less grinding to halt at about 64MB (presumably some O(n^2) algorithm, 
   although how you manage to do this for a window-size notification is a 
   mystery).  Some variants of this buggy server reportedly require a window 
   adjust for every 32K or so sent no matter what the actual window size is, 
   but this may be just a variant of the general mis-handling of large 
   window sizes so we treat it as the same thing and advertise a smaller-
   than-optimal 16MB window which, as a side-effect, results in a constant 
   flow of window adjusts */

CHECK_RETVAL_RANGE_NOERROR( 10000, MAX_WINDOW_SIZE ) STDC_NONNULL_ARG( ( 1 ) ) \
int getWindowSize( const SESSION_INFO *sessionInfoPtr )
	{
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	return( TEST_FLAG( sessionInfoPtr->protocolFlags, 
					   SSH_PFLAG_WINDOWSIZE ) ? \
			0x1000000 : MAX_WINDOW_SIZE );
	}

/* Process a channel control message.  Returns OK_SPECIAL to tell the caller
   to try again with the next packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processChannelControlMessage( INOUT SESSION_INFO *sessionInfoPtr,
								  INOUT STREAM *stream )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	const long prevChannelNo = \
				getCurrentChannelNo( sessionInfoPtr, CHANNEL_READ );
	long channelNo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* See what we've got.  SSH has a whole pile of no-op equivalents that 
	   we have to handle as well as the obvious no-ops.  We can also get 
	   global and channel requests for assorted reasons and a constant 
	   stream of window adjusts to implement the SSH performance handbrake */
	switch( sshInfo->packetType )
		{
		case SSH_MSG_GLOBAL_REQUEST:
			status = processChannelRequest( sessionInfoPtr, stream,
											CRYPT_UNUSED );
			if( cryptStatusError( status ) && status != OK_SPECIAL )
				return( status );
			return( OK_SPECIAL );

		case SSH_MSG_CHANNEL_OPEN:
			/* Process the channel-open request.  In theory we could 
			   immediately reject any attempts by the server to open a
			   channel to the client at this point, but unfortunately we
			   have to process a considerable portion of the channel-open
			   request in order to use the information in it to send a
			   request-denied response back to the server */
			status = processChannelOpen( sessionInfoPtr, stream );
			if( cryptStatusError( status ) )
				return( status );

			/* Tell the caller that they have to process the new channel
			   information before they can continue */
			return( CRYPT_ENVELOPE_RESOURCE );

		case SSH_MSG_IGNORE:
		case SSH_MSG_DEBUG:
			/* Nothing to see here, move along, move along:

				byte	SSH_MSG_IGNORE
				string	data

				byte	SSH_MSG_DEBUG
				boolean	always_display
				string	message
				string	language_tag */
			DEBUG_PUTS(( "Processing ignore/debug message" ));
			return( OK_SPECIAL );

		case SSH_MSG_DISCONNECT:
			/* This only really seems to be used during the handshake phase,
			   once a channel is open it (and the session as a whole) is
			   disconnected with a channel EOF/close, but we handle it here
			   anyway just in case */
			return( getDisconnectInfo( sessionInfoPtr, stream ) );

		case SSH_MSG_KEXINIT:
			/* The SSH spec is extremely vague about the sequencing of
			   operations during a rehandshake.  Unlike SSL there's no real 
			   indication of what happens to the connection-layer transfers 
			   while a transport-layer rehandshake is in progress.  Also 
			   unlike SSL we can't refuse a rehandshake by ignoring the 
			   request, so once we've fallen we can't get up any more.  This 
			   is most obvious with ssh.com's server, which starting with 
			   version 2.3.0 would do a rehandshake every hour (for a basic 
			   encrypted telnet session, while a high-volume IPsec link can 
			   run for hours before it feels the need to do this).  To make 
			   things even messier, neither side can block for too long 
			   waiting for the rehandshake to complete before sending new 
			   data because the lack of WINDOW_ADJUSTs (in an implementation 
			   that sends these with almost every packet, as most do) will 
			   screw up flow control and lead to deadlock.  This problem got 
			   so bad that as of 2.4.0 the ssh.com implementation would 
			   detect OpenSSH (the other main implementation at the time) 
			   and disable the rehandshake when it was talking to it, but it 
			   may not do this for other implementations.

			   To avoid falling into this hole, or at least to fail
			   obviously when the two sides can't agree on how to handle the
			   layering mismatch problem, we report a rehandshake request as
			   an error.  Trying to handle it properly results in hard-to-
			   diagnose errors (it depends on what the layers are doing at
			   the time of the problem), typically some bad-packet error
			   when the other side tries to interpret a connection-layer
			   packet as part of the rehandshake, or when the two sides
			   disagree on when to switch keys and one of the two decrypts 
			   with the wrong keys and gets a garbled packet type */
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Unexpected KEXINIT request received" ) );

		case SSH_MSG_CHANNEL_DATA:
		case SSH_MSG_CHANNEL_EXTENDED_DATA:
		case SSH_MSG_CHANNEL_REQUEST:
		case SSH_MSG_CHANNEL_WINDOW_ADJUST:
		case SSH_MSG_CHANNEL_EOF:
		case SSH_MSG_CHANNEL_CLOSE:
			/* All channel-specific messages end up here */
			channelNo = readUint32( stream );
			if( cryptStatusError( channelNo ) )
				{
				/* We can't send an error response to a channel request at
				   this point both because we haven't got to the response-
				   required flag yet and because SSH doesn't provide a
				   mechanism for returning an error response without an
				   accompanying channel number.  The best that we can do is
				   to quietly ignore the packet */
				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
						  "Invalid channel number in channel-specific packet "
						  "%s (%d)", 
						  getSSHPacketName( sshInfo->packetType ), 
						  sshInfo->packetType ) );
				}
			if( channelNo != getCurrentChannelNo( sessionInfoPtr, \
												  CHANNEL_READ ) )
				{
				/* It's a request on something other than the current
				   channel, try and select the new channel */
				status = selectChannel( sessionInfoPtr, channelNo,
										CHANNEL_READ );
				if( cryptStatusError( status ) )
					{
					/* As before for error handling */
					retExt( CRYPT_ERROR_BADDATA,
							( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
							  "Invalid channel number %lX in "
							  "channel-specific packet %s (%d), current "
							  "channel is %lX", channelNo,
							  getSSHPacketName( sshInfo->packetType ), 
							  sshInfo->packetType, prevChannelNo ) );
					}
				}
			break;

		default:
			{
			char stringBuffer[ 256 + 8 ];
			int i, length, totalLength, LOOP_ITERATOR;

			/* If it's a channel message, try and read the channel number */
			channelNo = -1;
			if( sshInfo->packetType >= SSH_MSG_CHANNEL_OPEN && \
				sshInfo->packetType <= SSH_MSG_CHANNEL_FAILURE )
				{
				channelNo = readUint32( stream );
				if( cryptStatusError( channelNo ) )
					channelNo = -1;
				}

			/* We got something unexpected, throw an exception in the debug
			   version and let the caller know the details */
			DEBUG_DIAG(( "Unexpected control packet %s (%d)", 
						 getSSHPacketName( sshInfo->packetType ), 
						 sshInfo->packetType ));
			assert( DEBUG_WARN );
			totalLength = length = sprintf_s( stringBuffer, 256, 
							"Unexpected control packet %s (%d) received",
							getSSHPacketName( sshInfo->packetType ), 
							sshInfo->packetType );
			ENSURES( length > 0 && length <= 256 );
			if( channelNo != -1 )
				{
				length = sprintf_s( stringBuffer + totalLength, 
									256 - totalLength, ", channel = %lX", 
									channelNo );
				ENSURES( length > 0 && length <= 256 );
				totalLength += length;
				}
			LOOP_MED( i = 0, i < 16, i++ )
				{
				const int ch = sgetc( stream );

				if( cryptStatusError( ch ) )
					break;
				if( i == 0 )
					{
					length = sprintf_s( stringBuffer + totalLength, 
										256 - totalLength, ", data begins" );
					ENSURES( length > 0 && length <= 256 );
					totalLength += length;
					}
				length = sprintf_s( stringBuffer + totalLength, 
									256 - totalLength, " %02X", ch );
				ENSURES( length > 0 && length <= 256 );
				totalLength += length;
				}
			ENSURES( LOOP_BOUND_OK );
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, "%s", 
					  stringBuffer ) );	/* Redundant %s needed for gcc */
			}
		}

	/* From here on we're processing a channel-specific message that applies
	   to the currently selected channel */
	switch( sshInfo->packetType )
		{
		case SSH_MSG_CHANNEL_DATA:
		case SSH_MSG_CHANNEL_EXTENDED_DATA:
			{
			int length;

			/* Get the payload length and make sure that it's
			   (approximately) valid, more exact checking has already been
			   done by the caller so we don't need to return extended error
			   information as this is just a backup check */
			status = length = readUint32( stream );
			if( cryptStatusError( status ) || \
				length < 0 || length > sessionInfoPtr->receiveBufSize )
				return( CRYPT_ERROR_BADDATA );
			DEBUG_PRINT(( "Processing data message, length %d.\n", length ));

			/* These are messages that consume window space, adjust the data 
			   window and communicate changes to the other side if 
			   necessary */
			status = handleWindowAdjust( sessionInfoPtr, channelNo, length );
			if( cryptStatusError( status ) )
				return( status );

			/* If it's a standard data packet, we're done */
			if( sshInfo->packetType == SSH_MSG_CHANNEL_DATA )
				return( CRYPT_OK );

			/* The extended data message is used for out-of-band data sent
			   over a channel, specifically output sent to stderr from a
			   shell command.  What to do with this is somewhat uncertain,
			   the only possible action that we could take apart from just
			   ignoring it is to convert it back to in-band data.  However,
			   something running a shell command may not expect to get
			   anything returned in this manner (see the comment for the
			   port-forwarding channel open in the client-side channel-open
			   code for more on this) so for now we just ignore it and 
			   assume that the user will rely on results sent as in-band
			   data.  This should be fairly safe since this message type
			   seems to be rarely (if ever) used, so apps will function
			   without it */
			return( OK_SPECIAL );
			}

		case SSH_MSG_CHANNEL_REQUEST:
			status = processChannelRequest( sessionInfoPtr, stream,
											prevChannelNo );
			if( cryptStatusError( status ) && status != OK_SPECIAL )
				return( status );
			return( OK_SPECIAL );

		case SSH_MSG_CHANNEL_WINDOW_ADJUST:
			/* Another noop-equivalent (but a very performance-affecting
			   one) */
			DEBUG_PUTS(( "Processing window adjust message" ));
			return( OK_SPECIAL );

		case SSH_MSG_CHANNEL_EOF:
			/* According to the SSH docs the EOF packet is a courtesy 
			   notification of no clear purpose (and specifically "the 
			   channel remains open after this message"), however some 
			   implementations seem to use a channel EOF in place of a 
			   close before sending a disconnect message, so we record the
			   presence of an EOF in the log in case this turns into a 
			   problem later */
			DEBUG_PUTS(( "Processing EOF message" ));
			return( OK_SPECIAL );

		case SSH_MSG_CHANNEL_CLOSE:
			/* The peer has closed their side of the channel, if our side
			   isn't already closed (in other words if this message isn't
			   a response to a close that we sent), close our side as well */
			if( getChannelStatusByChannelNo( sessionInfoPtr, 
											 channelNo ) == CHANNEL_BOTH )
				{
				status = sendChannelClose( sessionInfoPtr, channelNo,
										   CHANNEL_BOTH, TRUE );
				}
			else
				{
				/* We've already closed our side of the channel, delete it */
				status = deleteChannel( sessionInfoPtr, channelNo,
										CHANNEL_BOTH, TRUE );
				}
			DEBUG_PRINT(( "Processing channel close message for "
						  "channel %d.\n", channelNo ));

			/* If this wasn't the last channel, we're done */
			if( status != OK_SPECIAL )
				return( OK_SPECIAL );

			/* We've closed the last channel, indicate that the overall
			   connection is now closed.  This behaviour isn't mentioned in
			   the spec but it seems to be the standard way of handling 
			   things, particularly for the most common case where
			   channel == session */
			SET_FLAG( sessionInfoPtr->flags, SESSION_FLAG_SENDCLOSED );
			retExt( CRYPT_ERROR_COMPLETE,
					( CRYPT_ERROR_COMPLETE, SESSION_ERRINFO, 
					  "Remote system closed last remaining SSH channel" ) );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*							Channel Close Handling							*
*																			*
****************************************************************************/

/* Send a channel close notification.  Returns OK_SPECIAL if the last
   channel is being closed */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendChannelClose( INOUT SESSION_INFO *sessionInfoPtr,
							 IN const long channelNo,
							 IN_ENUM( CHANNEL ) const CHANNEL_TYPE channelType,
							 const BOOLEAN closeLastChannel )
	{
	BOOLEAN lastChannel = FALSE;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );
	REQUIRES( channelNo >= 0 && channelNo <= CHANNEL_MAX );
	REQUIRES( isEnumRange( channelType, CHANNEL ) );
	REQUIRES( closeLastChannel == TRUE || closeLastChannel == FALSE );

	/* Delete the channel.  If we've deleted the last active channel
	   deleteChannel() will return OK_SPECIAL to let us know that there are
	   no more channels left to close */
	status = deleteChannel( sessionInfoPtr, channelNo, channelType,
							closeLastChannel  );
	if( cryptStatusError( status ) )
		{
		if( status != OK_SPECIAL )
			return( status );
		
		/* We're closing the last remaining channel */
		lastChannel = TRUE;
		}

	/* Prepare the channel-close notification:

		byte		SSH_MSG_CHANNEL_CLOSE
		uint32		channel_no */
	INJECT_FAULT( SESSION_SSH_CORRUPT_CHANNEL_CLOSE, 
				  SESSION_SSH_CORRUPT_CHANNEL_CLOSE_1 );
	status = enqueueResponse( sessionInfoPtr, SSH_MSG_CHANNEL_CLOSE, 1,
							  channelNo, CRYPT_UNUSED, CRYPT_UNUSED,
							  CRYPT_UNUSED );
	INJECT_FAULT( SESSION_SSH_CORRUPT_CHANNEL_CLOSE, 
				  SESSION_SSH_CORRUPT_CHANNEL_CLOSE_2 );
	if( cryptStatusError( status ) )
		return( status );

	/* We can't safely use anything that ends up at sendPacketSSH2() at this
	   point since we may be closing the connection in response to a link
	   error, in which case the error returned from the packet send would
	   overwrite the actual error information.  Because of this we send the
	   response with the no-report-error flag set to suppress reporting of
	   network errors during the send */
	disableErrorReporting( sessionInfoPtr );
	status = sendEnqueuedResponse( sessionInfoPtr );
	enableErrorReporting( sessionInfoPtr );

	/* If it's the last channel, let the caller know (this overrides any
	   possible error return status, since we're about to close the 
	   connection there's not much that we can do with an error anyway) */
	return( lastChannel ? OK_SPECIAL : status );
	}

/* Close a channel */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int closeChannel( INOUT SESSION_INFO *sessionInfoPtr,
				  const BOOLEAN closeAllChannels )
	{
	SES_READHEADER_FUNCTION readHeaderFunction;
	READSTATE_INFO readInfo;
	const int currWriteChannelNo = \
				getCurrentChannelNo( sessionInfoPtr, CHANNEL_WRITE );
	int noChannels = 1, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );
	REQUIRES( closeAllChannels == TRUE || closeAllChannels == FALSE );

	readHeaderFunction = ( SES_READHEADER_FUNCTION ) \
						 FNPTR_GET( sessionInfoPtr->readHeaderFunction );
	REQUIRES( readHeaderFunction != NULL );

	/* If we've already sent the final channel-close message in response to
	   getting a final close notification from the peer then we're done */
	if( TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_SENDCLOSED ) )
		return( CRYPT_OK );

	/* Normally we can keep closing open channels until we hit the last one
	   whereupon we close the overall session, however if we're closing a
	   single identified channel we can't automatically close the whole
	   session as a side-effect of closing the single channel */
	if( !closeAllChannels && currWriteChannelNo == UNUSED_CHANNEL_NO )
		{
		retExt( CRYPT_ERROR_NOTINITED,
				( CRYPT_ERROR_NOTINITED, SESSION_ERRINFO, 
				  "No channel information available to identify the "
				  "channel to close" ) );
		}

	/* If there's no channel open, close the session with a session
	   disconnect rather than a channel close:

		byte		SSH_MSG_DISCONNECT
		uint32		reason_code = SSH_DISCONNECT_CONNECTION_LOST
		string		description = ""
		string		language_tag = ""

	   The spec doesn't explain what the reason codes actually mean but
	   SSH_DISCONNECT_CONNECTION_LOST seems to be the least inappropriate
	   disconnect reason at this point */
	if( currWriteChannelNo == UNUSED_CHANNEL_NO )
		{
		/* Since we're closing the session there's not much that we can do
		   in the case of an error because the very next operation is to 
		   shut down the network session, so we don't do anything if the 
		   send fails */
		status = enqueueResponse( sessionInfoPtr, SSH_MSG_DISCONNECT, 3,
								  SSH_DISCONNECT_CONNECTION_LOST, 0, 0,
								  CRYPT_UNUSED );
		if( cryptStatusOK( status ) )
			( void ) sendEnqueuedResponse( sessionInfoPtr );
		SET_FLAG( sessionInfoPtr->flags, SESSION_FLAG_SENDCLOSED );

		return( CRYPT_OK );
		}

	/* Close one or all channels */
	if( closeAllChannels )
		{
		/* Get the first available channel (which must succeed, since we
		   just checked it above) and close each successive channel in
		   turn */
		status = selectChannel( sessionInfoPtr, CRYPT_USE_DEFAULT,
								CHANNEL_WRITE );
		LOOP_MED( noChannels = 0, 
				  cryptStatusOK( status ) && \
					cryptStatusOK( selectChannel( sessionInfoPtr, CRYPT_USE_DEFAULT,
												  CHANNEL_WRITE ) ),
				  noChannels++ )
			{
			status = sendChannelClose( sessionInfoPtr,
						getCurrentChannelNo( sessionInfoPtr, CHANNEL_WRITE ),
						CHANNEL_WRITE, TRUE );
			}
		ENSURES( LOOP_BOUND_OK );
		}
	else
		{
		/* We're just closing one channel, close the write side.  The
		   complete close will be done when the other side acknowledges our
		   close.  If this isn't the last open channel then the response to 
		   our close will be handled as part of normal packet processing and 
		   we're done */
		status = sendChannelClose( sessionInfoPtr, currWriteChannelNo,
								   CHANNEL_WRITE, FALSE );
		if( status != OK_SPECIAL )
			{
			/* If this is the last remaining channel, we similarly can't
			   close it */
			if( status == CRYPT_ERROR_PERMISSION )
				{
				retExt( CRYPT_ERROR_PERMISSION,
						( CRYPT_ERROR_PERMISSION, SESSION_ERRINFO, 
						  "Cannot close last remaining channel without "
						  "closing the overall session" ) );
				}

			return( CRYPT_OK );
			}
		}

	/* It's the last open channel, flush through the remaining data */
	status = sendCloseNotification( sessionInfoPtr, NULL, 0 );
	if( cryptStatusError( status ) || \
		TEST_FLAG( sessionInfoPtr->protocolFlags, SESSION_FLAG_SENDCLOSED ) )
		{
		/* There's a problem at the network level or the other side has
		   already closed the session, exit */
		return( CRYPT_OK );
		}

	/* If there's not enough room in the receive buffer to read at least 1K
	   of packet data then we can't try anything further */
	if( sessionInfoPtr->receiveBufSize - sessionInfoPtr->receiveBufEnd < \
		min( sessionInfoPtr->pendingPacketRemaining, 1024 ) )
		return( CRYPT_OK );

	/* If we're in the middle of reading other data then there's no hope of
	   reading back the other side's channel close without first clearing all
	   of the other data, which is unlikely to happen because the channel 
	   has been closed (this can happen when the caller requests a shutdown
	   of the session in the middle of a data transfer), in which case we
	   just exit */
	if( sessionInfoPtr->receiveBufPos != sessionInfoPtr->receiveBufEnd )
		return( CRYPT_OK );

	/* Read back the other side's channel close(s).  This is somewhat messy
	   since the other side could decide that it still wants to send us
	   arbitrary amounts of data (the spec is rather vague about how urgent
	   a channel close is, the general idea among implementors seems to be
	   that you should let output drain before you close your side but if 
	   you're in the middle of sending a 2GB file that's a lot of output to 
	   drain).  This can also be complicated by implementation-specific 
	   quirks, for example OpenSSH may hang more or less indefinitely if
	   there's output coming from a background process on the server.  This
	   is because of a rather obscure race condition that would occur if it
	   exited immediately in which the SSH server gets the SIGCHLD from the
	   (local) background process exiting before it's written all of its
	   data to the (local) pipe connecting it to the SSH server, so it
	   closes the (remote) SSH channel/connection before the last piece of
	   data comes over the (local) pipe.  Because the server won't close the
	   (remote) SSH connection until it's certain that the (local) process
	   has written all of its data, and it'll never get the EOF over the
	   pipe, it hangs forever.  This is a piece of Unix plumbing arcana that
	   doesn't really concern us so again just exiting after a short wait 
	   seems to be the best response.

	   Since we're about to shut down the session anyway we try to read a 
	   basic channel close ack from the other side, if there's anything more 
	   than that we drop it.  This is complicated somewhat by the fact that 
	   what we're doing here is something that's normally handled by the 
	   high-level read code in sess_rw.c.  What we implement here is the 
	   absolute minimum needed to clear the stream (sendCloseNotification() 
	   has already set the necessary (small) nonzero timeout for us).
	   
	   Finally, we don't bother trying to decode what's being sent, since 
	   we're in the process of closing the session it doesn't really matter 
	   what the other side is sending us, it's not as if we're going to
	   abort the shutdown because some channel flag is set wrong */
	LOOP_SMALL_CHECKINC( noChannels > 0, noChannels-- )
		{
		status = readHeaderFunction( sessionInfoPtr, &readInfo );
		if( cryptStatusError( status ) )
			break;

		/* Adjust the packet information for the packet header data that was 
		   just read */
		sessionInfoPtr->receiveBufEnd += status;
		sessionInfoPtr->pendingPacketRemaining -= status;
		if( sessionInfoPtr->pendingPacketRemaining <= 512 )
			{
			const int bytesLeft = sessionInfoPtr->receiveBufSize - \
								  sessionInfoPtr->receiveBufEnd;

			/* We got a packet and it's probably the channel close ack, read 
			   it */
			status = sread( &sessionInfoPtr->stream,
							sessionInfoPtr->receiveBuffer + \
								sessionInfoPtr->receiveBufEnd,
							min( sessionInfoPtr->pendingPacketRemaining, \
								 bytesLeft ) );
			if( cryptStatusError( status ) )
				break;
			}
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}
#endif /* USE_SSH */
