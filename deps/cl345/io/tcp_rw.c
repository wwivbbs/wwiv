/****************************************************************************
*																			*
*					   cryptlib TCP/IP Read/Write Routines					*
*						Copyright Peter Gutmann 1998-2016					*
*																			*
****************************************************************************/

#include <ctype.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "stream_int.h"
  #include "tcp.h"
  #include "tcp_int.h"
#else
  #include "crypt.h"
  #include "io/stream_int.h"
  #include "io/tcp.h"
  #include "io/tcp_int.h"
#endif /* Compiler-specific includes */

#ifdef USE_TCP

/****************************************************************************
*																			*
*							Network I/O Wait Management						*
*																			*
****************************************************************************/

/* Wait for I/O to become possible on a socket.  The particular use of 
   select that we employ here is reasonably optimal under load because we're 
   only asking select() to monitor a single descriptor.  There are a variety 
   of inefficiencies related to select that fall into either the category of 
   user <-> kernel copying or of descriptor list scanning.  For the first 
   category, when calling select() the system has to copy an entire list of 
   descriptors into kernel space and then back out again.  Large selects can 
   potentially contain hundreds or thousands of descriptors, which can in 
   turn involve allocating memory in the kernel and freeing it on return.  
   We're only using one so the amount of data to copy is minimal.

   The second category involves scanning the descriptor list, an O(n) 
   activity.  First the kernel has to scan the list to see whether there's 
   pending activity on a descriptor.  If there aren't any descriptors with 
   activity pending it has to update the descriptor's selinfo entry in the 
   event that the calling process calls tsleep() (used to handle event-based 
   process blocking in the kernel) while waiting for activity on the 
   descriptor.  After the select() returns or the process is woken up from a 
   tsleep() the user process in turn has to scan the list to see which 
   descriptors the kernel indicated as needing attention.  As a result, the 
   list has to be scanned three times.

   These problems arise because select() (and its cousin poll()) are 
   stateless by design so everything has to be recalculated on each call.  
   After various false starts the kqueue interface is now seen as the best 
   solution to this problem.  However cryptlib's use of only a single 
   descriptor per select() avoids the need to use system-specific and rather 
   non-portable interfaces like kqueue or earlier alternatives like Sun's 
   /dev/poll, FreeBSD's get_next_event(), and SGI's /dev/imon */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int ioWait( INOUT NET_STREAM_INFO *netStream, 
			IN_INT_Z const int timeout,
			const BOOLEAN previousDataRead, 
			IN_ENUM( IOWAIT ) const IOWAIT_TYPE type )
	{
	static const struct {
		const int status;
		const char *errorString;
		} errorInfo[] = {
		{ CRYPT_ERROR_OPEN, "unknown" },
		{ CRYPT_ERROR_READ, "read" },		/* IOWAIT_READ */
		{ CRYPT_ERROR_WRITE, "write" },		/* IOWAIT_WRITE */
		{ CRYPT_ERROR_OPEN, "connect" },	/* IOWAIT_CONNECT */
		{ CRYPT_ERROR_OPEN, "accept" },		/* IOWAIT_ACCEPT */
		{ CRYPT_ERROR_OPEN, "unknown" }, { CRYPT_ERROR_OPEN, "unknown" }
		};
	MONOTIMER_INFO timerInfo;
	struct timeval tv;
	fd_set readfds, writefds, exceptfds;
	fd_set *readFDPtr = ( type == IOWAIT_READ || \
						  type == IOWAIT_CONNECT || \
						  type == IOWAIT_ACCEPT ) ? &readfds : NULL;
	fd_set *writeFDPtr = ( type == IOWAIT_WRITE || \
						   type == IOWAIT_CONNECT ) ? &writefds : NULL;
	int selectIterations, status, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( isIntegerRange( timeout ) );
	REQUIRES( previousDataRead == TRUE || previousDataRead == FALSE );
	REQUIRES( isEnumRange( type, IOWAIT ) );

	/* Check for overflows in FD_SET().  This is an ugly implementation 
	   issue in which, for sufficiently badly-implemented FD_SET() macros
	   (and there are plenty of these around), the macro will just take the 
	   provided socket descriptor and use it to index the fd_set bitmask.
	   This occurs for the most common implementations under Unix (BSD) and 
	   BSD-derived embedded OSes, Windows gets it right and uses a bounds-
	   checked array.  
	   
	   The maximum socket descriptor is normally given by FD_SETSIZE, 
	   typically 64 under Windows (but we don't have to worry this since it 
	   does FD_SET() right) and 256 or sometimes 1024 under Unix, however 
	   this can be increased explicitly using setrlimit() or, from the 
	   shell, 'ulimit -n 512' to make it 512, which will cause an overflow.  
	   To deal with this, we reject any socket values less than zero (if 
	   it's a signed variable) or greater than FD_SETSIZE */
#ifndef __WINDOWS__ 
	REQUIRES( netStream->netSocket >= 0 && \
			  netStream->netSocket <= FD_SETSIZE );
#endif /* !Windows */

	/* Set up the information needed to handle timeouts and wait on the
	   socket.  If there's no timeout then we wait 5ms on the theory that it 
	   isn't noticeable to the caller but ensures that we at least get a 
	   chance to get anything that may be pending.  We also have to special-
	   case the first iteration of the loop because a timeout of zero will
	   always result in the timer being expired, so we force at least one
	   iteration through the loop.

	   The exact wait time depends on the system, but usually it's quantised
	   to the system timer quantum.  This means that on Unix systems with a
	   1ms timer resolution the wait time is quantised on a 1ms boundary.
	   Under everything newer than early Windows NT systems it's quantised 
	   on a 10ms boundary (some early NT systems had a granularity ranging 
	   from 7.5 - 15ms but all newer systems use 10ms) and for Win95/98/ME 
	   it was quantised on a 55ms boundary.  In other words when performing 
	   a select() on a Win95 box it would either return immediately or wait 
	   some multiple of 55ms even with the time set to 1ms, but we don't
	   have to worry about those Windows versions any more.

	   In theory we shouldn't have to reset either the fds or the timeval
	   each time through the loop since we're only waiting on one descriptor
	   so it's always set and the timeval is a const, however some versions
	   of Linux can update it if the select fails due to an EINTR (which is
	   the exact reason why we'd be going through the loop a second time in
	   the first place) and/or if a file descriptor changes status (e.g. due 
	   to data becoming available) so we have to reset it each time to be on 
	   the safe side.  It would actually be nice if the tv value were 
	   updated reliably to reflect how long the select() had to wait since 
	   it'd provide a nice source of entropy for the randomness pool (we 
	   could simulate this by readig a high-res timer before and after the
	   select() but that would add a pile of highly system-dependent code
	   and defeat the intent of using the "free" entropy that's provided as 
	   a side-effect of the select()).

	   The wait on connect is a slightly special case, the socket will
	   become writeable if the connect succeeds normally, but both readable
	   and writeable if there's an error on the socket or if there's data
	   already waiting on the connection (i.e. it arrives as part of the
	   connect).  It's up to the caller to check for these conditions */
	status = setMonoTimer( &timerInfo, timeout );
	if( cryptStatusError( status ) )
		return( status );
	LOOP_MED( ( selectIterations = 0, status = SOCKET_ERROR ), \
			  isSocketError( status ) && \
				( selectIterations <= 0 || \
				  !checkMonoTimerExpired( &timerInfo ) ) && \
				selectIterations < 20, 
			  selectIterations++ )
		{
		if( readFDPtr != NULL )
			{
			FD_ZERO( readFDPtr );
			FD_SET( netStream->netSocket, readFDPtr );
			}
		if( writeFDPtr != NULL )
			{
			FD_ZERO( writeFDPtr );
			FD_SET( netStream->netSocket, writeFDPtr );
			}
		FD_ZERO( &exceptfds );
		FD_SET( netStream->netSocket, &exceptfds );
		tv.tv_sec = timeout;
		tv.tv_usec = ( timeout <= 0 ) ? 5000 : 0;

		/* See if we can perform the I/O.  This gets a bit complex under 
		   Windows because a socket isn't an int like everywhere else so the
		   expression 'maxSocket + 1' that's required for the wacky nfds 
		   argument doesn't make much sense.  OTOH Windows ignores this 
		   argument entirely since it serves no purpose (beyond making the
		   socket-scan macro slightly easier to handle on a PDP-11) because 
		   the fd_sets already contain all the required information, so we 
		   just cast whatever the value ends up as to an int to fit the 
		   function prototype */
		clearErrorState();
		status = select( ( int ) netStream->netSocket + 1, readFDPtr, 
						 writeFDPtr, &exceptfds, &tv );

		/* If there's a problem and it's not something transient like an
		   interrupted system call, exit.  For a transient problem, we just
		   retry the select until the overall timeout expires */
		if( isSocketError( status ) && \
			!isRestartableError( netStream->netSocket ) )
			{
			int dummy;

			return( getSocketError( netStream, errorInfo[ type ].status, 
									&dummy ) );
			}
		}
	if( selectIterations >= 20 )
		{
		char errorMessage[ 128 + 8 ];
		int errorMessageLength;

		/* We've gone through the select loop a suspiciously large number
		   of times, there's something wrong.  In theory we could report 
		   this as a more serious error than a simple timeout since it means
		   that there's either a bug in our code or a bug in the select()
		   implementation, but without knowing in advance what caused this
		   can't-occur condition it's difficult to anticipate the correct
		   action to take, so all that we do is warn in the debug build */
		DEBUG_DIAG(( "select() went through %d iterations without "
					 "returning a result", selectIterations ));
		assert( DEBUG_WARN );
		errorMessageLength = sprintf_s( errorMessage, 128,
										"select() on %s went through %d "
										"iterations without returning a "
										"result",
										errorInfo[ type ].errorString, 
										selectIterations );
		return( setSocketError( netStream, errorMessage, errorMessageLength,
								CRYPT_ERROR_TIMEOUT, FALSE ) );
		}

	/* If the wait timed out, either explicitly in the select (status == 0)
	   or implicitly in the wait loop (isSocketError()), report it as a
	   select() timeout error */
	if( status == 0 || isSocketError( status ) )
		{
		char errorMessage[ 128 + 8 ];
		int errorMessageLength;

		/* If we've already received data from a previous I/O, tell the 
		   caller to use that as the transferred byte count even though we 
		   timed out this time round */
		if( previousDataRead )
			return( OK_SPECIAL );

		/* If it's a nonblocking wait (usually used as a poll to determine
		   whether I/O is possible) then a timeout isn't an error.  The
		   caller can distinguish this from the previous OK_SPECIAL return 
		   by whether previousDataRead was set or not */
		if( timeout <= 0 )
			return( OK_SPECIAL );

		/* The select() timed out, exit */
		errorMessageLength = sprintf_s( errorMessage, 128,
										"Timeout on %s (select()) after %d "
										"second%s",
										errorInfo[ type ].errorString, 
										timeout, ( timeout > 1 ) ? "s" : "" );
		ENSURES( errorMessageLength > 0 && \
				 errorMessageLength < 128 );
		return( setSocketError( netStream, errorMessage, errorMessageLength,
								CRYPT_ERROR_TIMEOUT, FALSE ) );
		}

	/* If there's an exception condition on a socket, exit.  This is
	   implementation-specific, traditionally under Unix this only indicates
	   the arrival of out-of-band data rather than any real error condition,
	   but in some cases it can be used to signal errors.  In these cases we
	   have to explicitly check for an exception condition because some
	   types of errors will result in select() timing out waiting for
	   readability rather than indicating an error and returning.  In 
	   addition for OOB data we could just ignore the notification (which 
	   happens automatically with the default setting of SO_OOBINLINE = 
	   false and an indicator to receive SIGURG's not set, the OOB data byte 
	   just languishes in a side-buffer), however we shouldn't be receiving 
	   OOB data so we treat that as an error too */
	if( FD_ISSET( netStream->netSocket, &exceptfds ) )
		{
		int socketErrorCode;

		status = getSocketError( netStream, errorInfo[ type ].status, 
								 &socketErrorCode );
		if( socketErrorCode != 0 )
			return( status );

		/* We got a no-error error code even though there's an exception 
		   condition present, this typically only happens under Windows.  
		   The most common case is when we're waiting on a nonblocking 
		   connect (type = IOWAIT_CONNECT), in which case a failure to 
		   connect due to e.g. an ECONNREFUSED can be reported as a select() 
		   error.  This is a bit tricky to report on because we can't be 
		   sure what the actual problem is without adding our own timer 
		   handling, in which case a fast reject would be due to an explicit 
		   notification like ECONNREFUSED while a slow reject might be an 
		   ENETUNREACH or something similar (a genuine timeout error should 
		   have been caught by the "wait timed out" code above).  Another
		   option is to retry the connect as a blocking one to get a genuine
		   error code, but that defeats the point of using a nonblocking 
		   connect to deal with problem conditions.

		   The conflict here is between an honest but rather useless 
		   CRYPT_ERROR_OPEN and a guessed and far more useful, but 
		   potentially misleading, ECONNREFUSED.  Given that this is an
		   oddball condition to begin with it's unclear how far we should
		   go down this rathole, for now we assume an ECONNREFUSED */
		if( type == IOWAIT_CONNECT )
			{
			( void ) mapNetworkError( netStream, NONBLOCKCONNECT_ERROR, 
									  FALSE, CRYPT_ERROR_OPEN );
			return( status );
			}

		/* This is probably a mis-handled select() timeout, which can happen 
		   with Winsock under certain circumstances and seems to be related 
		   to another socket-using application performing network I/O at the 
		   same time as we do the select() wait.  Non-Winsock cases can occur 
		   because some implementations don't treat a soft timeout as an 
		   error, and at least one (Tandem) returns EINPROGRESS rather than 
		   ETIMEDOUT, so we insert a timeout error code ourselves.  
			   
		   Since we're merely updating the extended internal error 
		   information (we already know what the actual error status is) we 
		   don't need to do anything with the mapError() return value */
		( void ) mapNetworkError( netStream, TIMEOUT_ERROR, FALSE, 
								  CRYPT_ERROR_TIMEOUT );
		return( status );
		}

	/* The socket is read for reading or writing */
	ENSURES( status > 0 );
	ENSURES( ( type == IOWAIT_READ && \
			   FD_ISSET( netStream->netSocket, &readfds ) ) || \
			 ( type == IOWAIT_WRITE && \
			   FD_ISSET( netStream->netSocket, &writefds ) ) || \
			 ( type == IOWAIT_CONNECT && \
			   ( FD_ISSET( netStream->netSocket, &readfds ) || \
				 FD_ISSET( netStream->netSocket, &writefds ) ) ) || \
			 ( type == IOWAIT_ACCEPT ) );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Read Network Data							*
*																			*
****************************************************************************/

/* Read and write data from and to a socket.  Because data can appear in 
   bits and pieces when reading we have to implement timeout handling at two 
   levels, once via ioWait() and a second time as an overall timeout.  If we 
   only used ioWait() this could potentially stretch the overall timeout to 
   (length * timeout) so we also perform a time check that leads to a worst-
   case timeout of (timeout-1 + timeout).  This is the same as the 
   implementation of SO_SND/RCVTIMEO in Berkeley-derived implementations, 
   where the timeout value is actually an interval timer rather than an
   absolute timer.

   In addition to the standard stream-based timeout behaviour we can also be 
   called with flags specifying explicit blocking behaviour (for a read 
   where we know that we're expecting a certain amount of data) or explicit 
   nonblocking behaviour (for speculative reads to fill a buffer).  These 
   flags are used by the buffered-read routines, which try and speculatively 
   read as much data as possible to avoid the many small reads required by 
   some protocols.  We don't do the blocking read using MSG_WAITALL since 
   this can (potentially) block forever if not all of the data arrives.

   Finally, if we're performing an explicit blocking read (which is usually 
   done when we're expecting a predetermined number of bytes) we dynamically 
   adjust the timeout so that if data is streaming in at a steady rate then 
   we don't abort the read just because there's more data to transfer than 
   we can manage in the originally specified timeout interval.  This is 
   especially useful when transferring large data amounts, for which a one-
   size-fits-all fixed timeout doesn't accurately reflect the amount of time 
   required to transfer the full data amount.

   Handling of return values is as follows:

	timeout		byteCount		return
	-------		---------		------
		0			0				0
		0		  > 0			byteCount
	  > 0			0			CRYPT_ERROR_TIMEOUT
	  > 0		  > 0			byteCount

   At the sread()/swrite() level if the partial-read/write flags aren't set
   for the stream, a byteCount < length is also converted to a
   CRYPTO_ERROR_TIMEOUT */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int readSocketFunction( INOUT NET_STREAM_INFO *netStream, 
							   OUT_BUFFER( maxLength, *length ) BYTE *buffer, 
							   IN_DATALENGTH const int maxLength, 
							   OUT_DATALENGTH_Z int *length, 
							   IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	MONOTIMER_INFO timerInfo;
	BYTE *bufPtr = buffer;
	const int timeout = ( flags & TRANSPORT_FLAG_NONBLOCKING ) ? 0 : \
						( flags & TRANSPORT_FLAG_BLOCKING ) ? \
						max( NET_TIMEOUT_READ, netStream->timeout ) : \
						netStream->timeout;
	const BOOLEAN isDgramSocket = \
			TEST_FLAG( netStream->nFlags, STREAM_NFLAG_DGRAM ) ? TRUE : FALSE;
	int bytesToRead, byteCount = 0, status, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isWritePtrDynamic( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES( ( ( flags & TRANSPORT_FLAG_NONBLOCKING ) && \
				timeout == 0 ) || \
			  ( !( flags & TRANSPORT_FLAG_NONBLOCKING ) && \
				isIntegerRange( timeout ) ) );
	REQUIRES( flags == TRANSPORT_FLAG_NONE || \
			  flags == TRANSPORT_FLAG_NONBLOCKING || \
			  flags == TRANSPORT_FLAG_BLOCKING );

	/* Clear return value */
	*length = 0;

	status = setMonoTimer( &timerInfo, timeout );
	if( cryptStatusError( status ) )
		return( status );
	LOOP_MAX_INITCHECK( bytesToRead = maxLength,
						bytesToRead > 0 && \
							( timeout <= 0 || \
							  !checkMonoTimerExpired( &timerInfo ) ) )
		{
		int bytesRead;

		/* Wait for data to become available */
		status = ioWait( netStream, timeout, 
						 ( byteCount > 0 ) ? TRUE : FALSE, IOWAIT_READ );
		if( status == OK_SPECIAL )
			{
			/* We got a timeout but either there's already data present from 
			   a previous read or it's a nonblocking wait, so this isn't an
			   error */
			if( byteCount > 0 )
				*length = byteCount;
			return( CRYPT_OK );
			}
		if( cryptStatusError( status ) )
			{
			/* Some buggy firewall software will block any data transfer 
			   attempts made after the initial connection setup.  More 
			   specifically they'll allow the initial SYN/SYN/ACK to 
			   establish the connection state but then block any further 
			   information from being transferred.  To handle this we
			   check whether we get a timeout on the first read and if we
			   do then we check for the presence of a software firewall,
			   reporting a problem due to the firewall rather than a
			   general networking problem if we find one */
			if( status == CRYPT_ERROR_TIMEOUT && \
				!TEST_FLAG( netStream->nFlags, STREAM_NFLAG_FIRSTREADOK ) )
				{
				return( checkFirewallError( netStream ) );
				}
			return( status );
			}

		/* We've got data waiting, read it */
		clearErrorState();
		bytesRead = recv( netStream->netSocket, bufPtr, bytesToRead, 0 );
		if( isSocketError( bytesRead ) )
			{
			int dummy;

			/* If it's a restartable read due to something like an
			   interrupted system call, retry the read */
			if( isRestartableError( netStream->netSocket ) )
				{
				DEBUG_DIAG(( "Restartable read, recv() indicated error" ));
				assert( DEBUG_WARN );
				continue;
				}

			/* There was a problem with the read */
			return( getSocketError( netStream, CRYPT_ERROR_READ, &dummy ) );
			}
		if( bytesRead <= 0 )
			{
			/* Under some odd circumstances (bugs in older versions of 
			   Winsock when using non-blocking sockets, or calling select() 
			   with a timeout of 0), recv() can return zero bytes without an 
			   EOF condition being present, even though it should return an 
			   error status if this happens (this could also happen under 
			   very old SysV implementations using O_NDELAY for nonblocking 
			   I/O).  
			   
			   One situation in which we can legitimately get this (although
			   the status is misleading) is when we don't get an ACK for a 
			   previous data send.  If we get here before the ACK timeout 
			   occurs then we won't get the ECONNABORTED but instead get 
			   recv() == 0 (exactly how we can get recv() == 0 due to the 
			   lack of ACK but not an ECONNABORTED at the same point remains 
			   a mystery).  
			   
			   An example of a situation in which we can get an ECONNABORTED 
			   is when the client sends an HTTP POST to the server, which 
			   looks at the HTTP request header, rejects it (e.g. due to 
			   excessive content length, which cryptlib will do in order to 
			   avoid being DoS'ed by the other side), and sends back an 
			   error response and closes the connection without trying to 
			   read the body of the request.  The connection is now half-
			   closed, with the client still writing the HTTP body to its 
			   side of the connection, which means that it gets buffered in 
			   the TCP stack but not sent.  At this point the client tries 
			   to read the HTTP response, but in the meantime the outgoing 
			   retransmission of the buffered data has failed and the TCP 
			   stack on the client shuts down the connection.  This means 
			   that the HTTP response that the server sent is never read, 
			   and the client gets an ECONNABORTED.

			   Dealing with this particular situation is quite difficult, 
			   see the comment in the code block for handling 
			   byteCount == 0 at the end of this function.
			   
			   To try and catch the more general situation we check for a 
			   restartable read due to something like an interrupted system 
			   call and retry the read if it is.  This doesn't catch the 
			   Winsock zero-delay bug but it may catch problems in other 
			   implementations.

			   Unfortunately this doesn't work under all circumstances
			   either.  If the connection is genuinely closed select() will
			   return a data-available status and recv() will return zero,
			   both without changing errno.  If the last status set in errno
			   matches the isRestartableError() check, the code will loop
			   forever.  Because of this we can't use the following check,
			   although since it doesn't catch the Winsock zero-delay bug
			   anyway it's probably no big deal.

			   The real culprit here is the design flaw in recv(), which
			   uses a valid bytes-received value to indicate an out-of-band
			   condition that should be reported via an error code ("There's
			   nowt wrong wi' owt what mitherin clutterbucks don't barley
			   grummit") */
#if 0	/* See above comment */
			if( isRestartableError() )
				{
				assert( !"Restartable read, recv() indicated no error" );
				continue;
				}
#endif /* 0 */

			/* Once we encounter this problem, we've fallen and can't get up 
			   any more.  WSAGetLastError() reports no error, select() 
			   reports data available for reading, and recv() reports zero 
			   bytes read.  If the following is used, the code will loop 
			   endlessly (or at least until the loop iteration watchdog 
			   triggers) waiting for data that can never be read */
#if 0	/* See above comment */
			getSocketError( netStream, CRYPT_ERROR_READ, &dummy );
			status = ioWait( netStream, 0, 0, IOWAIT_READ );
			if( cryptStatusOK( status ) )
				continue;
#endif /* 0 */

			/* "It said its piece, and then it sodded off" - Baldrick,
			   Blackadder's Christmas Carol */
			bytesToRead = 0;	/* Force exit from loop */
			continue;
			}
		bufPtr += bytesRead;
		bytesToRead -= bytesRead;
		byteCount += bytesRead;
		ENSURES( bytesToRead >= 0 && bytesToRead < maxLength && \
				 bytesToRead < MAX_BUFFER_SIZE );
		ENSURES( byteCount > 0 && byteCount <= maxLength && \
				 byteCount < MAX_BUFFER_SIZE );

		/* Remember that we've got some data, used for error diagnosis (see
		   the long comment above) */
		SET_FLAG( netStream->nFlags, STREAM_NFLAG_FIRSTREADOK );

		/* Datagram sockets are different from standard stream sockets in 
		   that data amounts are quantised, we get either nothing or a 
		   complete packet.  Since we've already got some data it means that 
		   we got a complete packet, so we're done */
		if( isDgramSocket )
			break;

		/* If this is a blocking read and we've been moving data at a 
		   reasonable rate (~1K/s) and we're about to time out, adjust the 
		   timeout to give us a bit more time.  This is an adaptive process 
		   that grants us more time for the read if data is flowing at 
		   a reasonable rate, but ensures that we don't hang around forever 
		   if data is trickling in at a few bytes a second */
		if( flags & TRANSPORT_FLAG_BLOCKING )
			{
			ENSURES( timeout > 0 );

			/* If the timer expiry is imminent but data is still flowing in, 
			   extend the timing duration to allow for further data to 
			   arrive.  Because of the minimum flow-rate limit that's 
			   imposed above this is unlikely to be subject to much of a DoS 
			   problem (at worst an attacker can limit us to reading data 
			   at 1K/s, which means 16s for SSL/TLS packets and 32s for SSH 
			   packets), but to make things a bit less predictable we dither 
			   the timeout a bit */
			if( ( byteCount / timeout ) >= 1000 && \
				checkMonoTimerExpiryImminent( &timerInfo, 5 ) )
				{
				extendMonoTimer( &timerInfo, 
								 ( getRandomInteger() % 5 ) + 2 );
				}
			}
		}
	ENSURES( LOOP_BOUND_OK );
	if( maxLength > 0 && byteCount <= 0 )
		{
		/* We didn't get anything because the other side closed the
		   connection.  We report this as a read-complete status rather than
		   a read error since it isn't necessarily a real error.
		   
		   One situation in which we can get this is when we don't get an 
		   ACK for a previous data send, however if we get here before the 
		   ACK timeout occurs then we won't get the ECONNABORTED/
		   WSAECONNABORTED but instead get recv() == 0.  Getting the 
		   ECONNABORTED is quite difficult, just adding a delay won't work
		   so we need to wait and then perform a second read.  Because this
		   is somewhat system-specific, we make it Windows-only for now */
#ifdef __WINDOWS__
		BYTE dummyBuffer[ 8 + 8 ];
  #ifdef USE_ERRMSGS
		int errorCode;
  #endif /* USE_ERRMSGS */

		Sleep( 500 );
		( void ) recv( netStream->netSocket, dummyBuffer, 8, 0 );
  #ifdef USE_ERRMSGS
		( void ) getSocketError( netStream, CRYPT_ERROR_READ, &errorCode );
		if( errorCode == WSAECONNABORTED )
			return( CRYPT_ERROR_COMPLETE );
  #endif /* USE_ERRMSGS */
#endif /* __WINDOWS__ */
		return( setSocketError( netStream, 
								"No data was read because the remote system "
								"closed the connection (recv() == 0)", 78,
								CRYPT_ERROR_COMPLETE, TRUE ) );
		}
	*length = byteCount;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Write Network Data							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int writeSocketFunction( INOUT NET_STREAM_INFO *netStream, 
								IN_BUFFER( maxLength ) const BYTE *buffer, 
								IN_DATALENGTH const int maxLength, 
								OUT_DATALENGTH_Z int *length,
								IN_FLAGS_Z( TRANSPORT ) const int flags )
	{
	MONOTIMER_INFO timerInfo;
	const BYTE *bufPtr = buffer;
	const int timeout = ( flags & TRANSPORT_FLAG_NONBLOCKING ) ? 0 : \
						( flags & TRANSPORT_FLAG_BLOCKING ) ? \
						max( NET_TIMEOUT_WRITE, netStream->timeout ) : \
						netStream->timeout;
	int bytesToWrite, byteCount = 0, status, LOOP_ITERATOR;

	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );
	assert( isReadPtrDynamic( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( sanityCheckNetStream( netStream ) );
	REQUIRES( maxLength > 0 && maxLength < MAX_BUFFER_SIZE );
	REQUIRES( ( ( flags & TRANSPORT_FLAG_NONBLOCKING ) && \
				timeout == 0 ) || \
			  ( !( flags & TRANSPORT_FLAG_NONBLOCKING ) && \
				isIntegerRange( timeout ) ) );
	REQUIRES( flags == TRANSPORT_FLAG_NONE || \
			  flags == TRANSPORT_FLAG_NONBLOCKING || \
			  flags == TRANSPORT_FLAG_BLOCKING || \
			  flags == TRANSPORT_FLAG_FLUSH );
			  /* TRANSPORT_FLAG_FLUSH is used by alternative write 
			     functions that buffer data, for a socket write there's
				 always an implicit flush */

	/* Clear return value */
	*length = 0;

	/* Send data to the remote system.  As with the receive-data code we 
	   have to work around a large number of quirks and socket 
	   implementation bugs, although most of the systems that exhibited 
	   these are now extinct or close to it.  Some very old Winsock stacks 
	   (Win3.x and early Win95 era) would almost always indicate that a 
	   socket was writeable even when it wasn't.  Even older (mid-1980s) 
	   Berkeley-derived implementations could return EWOULDBLOCK on a 
	   blocking socket if they couldn't get required mbufs so that even if 
	   select() indicated that the socket was writeable, an actual attempt 
	   to write would return an error since there were no mbufs available.  

	   Under Win95 select() could fail to block on a non-blocking socket, 
	   so that the send() would return EWOULDBLOCK.  One possible reason 
	   (related to the mbuf problem) for this was that another thread could 
	   have grabbed memory between the select() and the send() so that there 
	   was no buffer space available when the send() needed it, although 
	   this should really have returned WSAENOBUFS rather than 
	   WSAEWOULDBLOCK.  
	   
	   There was also a known bug in Win95 (and possibly Win98 as well, 
	   Q177346) under which a select() would indicate writeability but 
	   send() would return EWOULDBLOCK.  Another select() executed after the 
	   failed send() then caused select() to suddenly realise that the 
	   socket was non-writeable (accidit in puncto, quod non seperatur in 
	   anno).  
	   
	   Finally, in some cases send() can return an error but 
	   WSAGetLastError() indicates that there's no error, so we treat it as 
	   noise and try again */
	status = setMonoTimer( &timerInfo, timeout );
	if( cryptStatusError( status ) )
		return( status );
	LOOP_MAX_INITCHECK( bytesToWrite = maxLength,
						bytesToWrite > 0 && \
							( timeout <= 0 || \
							  !checkMonoTimerExpired( &timerInfo ) ) )
		{
		int bytesWritten;

		/* Wait for the socket to become available */
		status = ioWait( netStream, timeout, 
						 ( byteCount > 0 ) ? TRUE : FALSE, IOWAIT_WRITE );
		if( status == OK_SPECIAL )
			{
			/* We got a timeout but either there's already data present from 
			   a previous read or it's a nonblocking write, so this isn't an
			   error */
			if( byteCount > 0 )
				{
				*length = byteCount;

				return( CRYPT_OK );
				}
			return( CRYPT_OK );
			}
		if( cryptStatusError( status ) )
			return( status );

		/* Write the data */
		clearErrorState();
		bytesWritten = send( netStream->netSocket, bufPtr, bytesToWrite,
							 MSG_NOSIGNAL );
		if( isSocketError( bytesWritten ) )
			{
			int dummy;

			/* If it's a restartable write due to something like an
			   interrupted system call (or a sockets bug), retry the
			   write */
			if( isRestartableError( netStream->netSocket ) )
				{
				DEBUG_DIAG(( "Restartable write, send() indicated error" ));
				assert( DEBUG_WARN );
				continue;
				}

#ifdef __WINDOWS__
			/* If it's a Winsock bug, treat it as a restartable write */
			if( WSAGetLastError() < WSABASEERR )
				{
				DEBUG_DIAG(( "send() failed but WSAGetLastError() indicated "
							 "no error, ignoring" ));
				assert( DEBUG_WARN );
				continue;
				}
#endif /* __WINDOWS__ */

			/* There was a problem with the write */
			return( getSocketError( netStream, CRYPT_ERROR_WRITE, &dummy ) );
			}
		bufPtr += bytesWritten;
		bytesToWrite -= bytesWritten;
		byteCount += bytesWritten;
		ENSURES( bytesToWrite >= 0 && bytesToWrite < maxLength && \
				 bytesToWrite < MAX_BUFFER_SIZE );
		ENSURES( byteCount > 0 && byteCount <= maxLength && \
				 byteCount < MAX_BUFFER_SIZE );
		}
	ENSURES( LOOP_BOUND_OK );
	*length = byteCount;

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void setAccessMethodTCPReadWrite( INOUT NET_STREAM_INFO *netStream )
	{
	assert( isWritePtr( netStream, sizeof( NET_STREAM_INFO ) ) );

	/* Set the access method pointers */
	FNPTR_SET( netStream->transportReadFunction, readSocketFunction );
	FNPTR_SET( netStream->transportWriteFunction, writeSocketFunction );
	}
#endif /* USE_TCP */
