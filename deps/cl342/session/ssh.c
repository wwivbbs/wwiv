/****************************************************************************
*																			*
*						cryptlib SSH Session Management						*
*						Copyright Peter Gutmann 1998-2008					*
*																			*
****************************************************************************/

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

#if defined( USE_SSH ) || defined( USE_SSH1 )

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Initialise and destroy the handshake state information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initHandshakeInfo( OUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	/* Initialise the handshake state information values */
	memset( handshakeInfo, 0, sizeof( SSH_HANDSHAKE_INFO ) );
	handshakeInfo->iExchangeHashContext = \
		handshakeInfo->iExchangeHashAltContext = \
			handshakeInfo->iServerCryptContext = CRYPT_ERROR;
	handshakeInfo->exchangeHashAlgo = CRYPT_ALGO_SHA1;
	
	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void destroyHandshakeInfo( INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	/* Destroy any active contexts.  We need to do this here (even though
	   it's also done in the general session code) to provide a clean exit in
	   case the session activation fails, so that a second activation attempt
	   doesn't overwrite still-active contexts */
	if( handshakeInfo->iExchangeHashContext != CRYPT_ERROR )
		krnlSendNotifier( handshakeInfo->iExchangeHashContext,
						  IMESSAGE_DECREFCOUNT );
	if( handshakeInfo->iExchangeHashAltContext != CRYPT_ERROR )
		krnlSendNotifier( handshakeInfo->iExchangeHashAltContext,
						  IMESSAGE_DECREFCOUNT );
	if( handshakeInfo->iServerCryptContext != CRYPT_ERROR )
		krnlSendNotifier( handshakeInfo->iServerCryptContext,
						  IMESSAGE_DECREFCOUNT );

	/* Clear the handshake state information, then reset it to explicit non-
	   initialised values */
	zeroise( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) );
	( void ) initHandshakeInfo( handshakeInfo );
	}

/* Read the SSH version information string */

CHECK_RETVAL_RANGE( MAX_ERROR, 255 ) STDC_NONNULL_ARG( ( 1 ) ) \
static int readCharFunction( INOUT TYPECAST( STREAM * ) void *streamPtr )
	{
	STREAM *stream = streamPtr;
	BYTE ch;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	status = sread( stream, &ch, 1 );
	return( cryptStatusError( status ) ? status : byteToInt( ch ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readVersionString( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const BYTE *versionStringPtr;
	const char *peerType = isServer( sessionInfoPtr ) ? "Client" : "Server";
	int versionStringLength, length = DUMMY_INIT, linesRead;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Read the server version information, with the format for the ID 
	   string being "SSH-protocolversion-softwareversion comments", which 
	   (in the original ssh.com interpretation) was "SSH-x.y-x.y vendorname" 
	   (e.g. "SSH-2.0-3.0.0 SSH Secure Shell") but for almost everyone else 
	   is "SSH-x.y-vendorname*version" (e.g "SSH-2.0-OpenSSH_3.0").

	   This version information handling is rather ugly since it's a 
	   variable-length string terminated with a newline, so we have to use
	   readTextLine() as if we were talking HTTP.

	   Unfortunately the SSH RFC then further complicates this by allowing 
	   implementations to send non-version-related text lines before the
	   version line.  The theory is that this will allow applications like
	   TCP wrappers to display a (human-readable) error message before
	   disconnecting, however some installations use it to display general
	   banners before the ID string.  Since the RFC doesn't provide any 
	   means of distinguishing this banner information from arbitrary data 
	   we can't quickly reject attempts to connect to something that isn't 
	   an SSH server.  In other words we have to sit here waiting for 
	   further data in the hope that eventually an SSH ID turns up, until 
	   such time as the connect timeout expires */
	for( linesRead = 0; linesRead < 20; linesRead++ )
		{
		BOOLEAN isTextDataError;

		/* Get a line of input.  Since this is the first communication that
		   we have with the remote system we're a bit more loquacious about
		   diagnostics in the event of an error */
		status = readTextLine( readCharFunction, &sessionInfoPtr->stream, 
							   sessionInfoPtr->receiveBuffer, 
							   SSH_ID_MAX_SIZE, &length, &isTextDataError );
		if( cryptStatusError( status ) )
			{
			const char *lcPeerType = isServer( sessionInfoPtr ) ? \
									 "client" : "server";
			ERROR_INFO errorInfo;	/* Lowercase version of peerType */

			sNetGetErrorInfo( &sessionInfoPtr->stream, &errorInfo );
			retExtErr( status, 
					   ( status, SESSION_ERRINFO, &errorInfo, 
					     "Error reading %s's SSH identifier string: ", 
						 lcPeerType ) );
			}

		/* If it's the SSH ID/version string, we're done */
		if( length >= SSH_ID_SIZE && \
			!memcmp( sessionInfoPtr->receiveBuffer, SSH_ID, SSH_ID_SIZE ) )
			break;
		}

	/* The peer shouldn't be throwing infinite amounts of junk at us, if we 
	   don't get an SSH ID after reading 20 lines of input then there's a 
	   problem */
	if( linesRead >= 20 )
		{
		retExt( CRYPT_ERROR_OVERFLOW,
				( CRYPT_ERROR_OVERFLOW, SESSION_ERRINFO, 
				  "%s sent excessive amounts of text without sending an "
				  "SSH identifier string", peerType ) );
		}

	/* Make sure that we got enough data to work with.  We need at least 
	   "SSH-" (ID, size SSH_ID_SIZE) + "x.y-" (protocol version) + "xxxxx" 
	   (software version/ID, of which the shortest-known is "ConfD") */
	if( length < SSH_ID_SIZE + 9 || length > SSH_ID_MAX_SIZE )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "%s sent invalid-length identifier string '%s', total "
				  "length %d", peerType,
				  sanitiseString( sessionInfoPtr->receiveBuffer, 
								  CRYPT_MAX_TEXTSIZE, length ),
				  length ) );
		}

	/* Null-terminate the string so that we can hash it to create the SSHv2
	   exchange hash.  We actually set a block of memory following the 
	   string to zeroes (rather than just one null character) in case of
	   any slight range errors in the free-format text-string checks that
	   are required further on to identify bugs in SSH implementations */
	memset( sessionInfoPtr->receiveBuffer + length, 0, 16 );

	/* Determine which version we're talking to */
	switch( sessionInfoPtr->receiveBuffer[ SSH_ID_SIZE ] )
		{
		case '1':
			if( !memcmp( sessionInfoPtr->receiveBuffer + SSH_ID_SIZE, 
						 "1.99", 4 ) )
				{
				/* SSHv2 server in backwards-compatibility mode */
				sessionInfoPtr->version = 2;
				break;
				}

#ifdef USE_SSH1
			/* If the caller has specifically asked for SSHv2 but all that 
			   the server offers is SSHv1, we can't continue */
			if( sessionInfoPtr->version >= 2 )
				{
				retExt( CRYPT_ERROR_NOSECURE,
						( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
						  "Server can only do SSHv1 when SSHv2 was "
						  "requested" ) );
				}
			sessionInfoPtr->version = 1;
			break;
#else
			retExt( CRYPT_ERROR_NOSECURE,
					( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
					  "Server can only do SSHv1" ) );
#endif /* USE_SSH1 */

		case '2':
			sessionInfoPtr->version = 2;
			break;

		default:
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid SSH version '%s'",
					  sanitiseString( &sessionInfoPtr->receiveBuffer[ SSH_ID_SIZE ],
									  CRYPT_MAX_TEXTSIZE, 1 ) ) );
		}

	/* Find the end of the protocol version substring, i.e. locate whatever 
	   follows the "SSH-x.y" portion of the ID string by searching for the
	   second '-' delimiter */
	for( versionStringLength = length - SSH_ID_SIZE, \
			versionStringPtr = sessionInfoPtr->receiveBuffer + SSH_ID_SIZE;
		 versionStringLength > 0 && *versionStringPtr != '-';
		 versionStringLength--, versionStringPtr++ );
	if( versionStringLength < 4 )
		{
		/* We need at least "-x.y" after the initial ID string, we can't 
		   require any more than this because of CuteFTP (see note below) */
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "%s sent malformed identifier string '%s'", peerType,
				  sanitiseString( sessionInfoPtr->receiveBuffer, 
								  CRYPT_MAX_TEXTSIZE, length ) ) );
		}
	versionStringPtr++, versionStringLength--;	/* Skip '-' */
	ENSURES( versionStringLength >= 3 && \
			 versionStringLength < SSH_ID_MAX_SIZE );	/* From earlier checks */

	/* Check whether the peer is using cryptlib */
	if( versionStringLength >= 8 && \
		!memcmp( versionStringPtr, "cryptlib", 8 ) )
		sessionInfoPtr->flags |= SESSION_ISCRYPTLIB;

	/* Check for various servers that require special-case bug workarounds.  
	   The versions that we check for are:

		BitVise WinSSHD:
			This one is a bit hard to identify because it's built on top of 
			their SSH library which changes names from time to time, so for 
			WinSSHD 4.x it was identified via the vendor ID string
			"sshlib: WinSSHD 4.yy" while for WinSSHD 5.x it was identified 
			via the vendor ID string "FlowSsh: WinSSHD 5.xx".  In theory we 
			could handle this by skipping the library name and looking 
			further inside the string for the "WinSSHD" identifier, but then 
			there's another version that uses "SrSshServer" instead of 
			"WinSSHD", and there's also a "GlobalScape" ID used by CuteFTP 
			(which means that CuteFTP might have finally fixed their buggy 
			implementation of SSH by using someone else's).  As a result we 
			can see any of "sshlib: <vendor>" or "FlowSsh: <vendor>", which 
			we use as the identifier.
			
			Sends mismatched compression algorithm IDs, no compression 
			client -> server, zlib server -> client, but works fine if no 
			compression is selected, for versions 4.x and up.

		CuteFTP:
			Drops the connection after seeing the server hello with no
			(usable) error indication.  This implementation is somewhat
			tricky to detect since it identifies itself using the dubious
			vendor ID string "1.0" (see the ssh.com note below), this
			problem still hasn't been fixed several years after the vendor 
			was notified of it, indicating that it's unlikely to ever be 
			fixed.  This runs into problems with other implementations like 
			BitVise WinSSHD 5.x, which has an ID string beginning with "1.0" 
			(see the comment for WinSSHD above) so when trying to identify 
			CuteFTP we check for an exact match for "1.0" as the ID string.
			
			CuteFTP also uses the SSHv1 backwards-compatible version string 
			"1.99" even though it can't actually do SSHv1, which means that 
			it'll fail if it ever tries to connect to an SSHv1 peer.

		OpenSSH:
			Omits hashing the exchange hash length when creating the hash
			to be signed for client auth for version 2.0 (all subversions).

			Requires RSA signatures to be padded out with zeroes to the RSA 
			modulus size for all versions from 2.5 to 3.2.

			Can't handle "password" as a PAM sub-method (meaning an
			authentication method hint), it responds with an authentication-
			failed response as soon as we send the PAM authentication
			request, for versions 3.8 onwards (this doesn't look like it'll
			get fixed any time soon so we enable it for all newer versions
			until further notice).

		Putty:
			Sends zero-length SSH_MSG_IGNORE messages for version 0.59.

		ssh.com:
			This implementation puts the version number first so if we find
			something without a vendor name at the start we treat it as an
			ssh.com version.  However, Van Dyke's SSH server VShell also
			uses the ssh.com-style identification (fronti nulla fides) so
			when we check for the ssh.com implementation we habe to make 
			sure that it isn't really VShell.  In addition CuteFTP 
			advertises its implementation as "1.0" (without any vendor 
			name), which is going to cause problems in the future when they 
			move to 2.x.

			Omits the DH-derived shared secret when hashing the keying
			material for versions identified as "2.0.0" (all
			sub-versions) and "2.0.10".

			Uses an SSH2_FIXED_KEY_SIZE-sized key for HMAC instead of the de
			facto 160 bits for versions identified as "2.0.", "2.1 ", "2.1.",
			and "2.2." (i.e. all sub-versions of 2.0, 2.1, and 2.2), and
			specifically version "2.3.0".  This was fixed in 2.3.1.

			Omits the signature algorithm name for versions identified as
			"2.0" and "2.1" (all sub-versions), requiring a complex rewrite
			of the signature data in order to process it.

			Mishandles large window sizes in a variety of ways.  Typically
			for any size over about 8M the server gets slower and slower,
			eventually more or less grinding to halt at about 64MB 
			(presumably some O(n^2) algorithm, although how you manage to
			do this for a window-size notification is a mystery).  Some
			versions also reportedly require a window adjust for every 32K 
			or so sent no matter what the actual window size is, which seems
			to occur for versions identified as "2.0" and "2.1" (all 
			sub-versions).  This may be just a variant of the general mis-
			handling of large window sizes so we treat it as the same thing
			and advertise a smaller-than-optimal window which, as a side-
			effect, results in a constant flow of window adjusts.

			Omits hashing the exchange hash length when creating the hash
			to be signed for client auth for versions 2.1 and 2.2 (all
			subversions).

			Sends an empty SSH_SERVICE_ACCEPT response for version 2.0 (all
			subversions).

			Sends an empty userauth-failure response if no authentication is
			required instead of allowing the auth, for uncertain versions
			probably in the 2.x range.

			Dumps text diagnostics (that is, raw text strings rather than
			SSH error packets) onto the connection if something unexpected
			occurs, for uncertain versions probably in the 2.x range.

		Van Dyke:
			Omits hashing the exchange hash length when creating the hash to
			be signed for client auth for version 3.0 (SecureCRT = SSH) and
			1.7 (SecureFX = SFTP).

		WeOnlyDo:
			Has the same mismatched compression algorithm ID bug as BitVise 
			WinSSHD (see comment above) for unknown versions above about 2.x.

	   Further quirks and peculiarities abound, some are handled automatically 
	   by workarounds in the code and for the rest they're fortunately rare 
	   enough (mostly for long-obsolete SSHv1 versions) that we don't have to 
	   go out of our way to handle them */
	if( versionStringLength >= 8 + 3 && \
		!memcmp( versionStringPtr, "OpenSSH_", 8 ) )
		{
		const BYTE *subVersionStringPtr = versionStringPtr + 8;

		if( !memcmp( subVersionStringPtr, "2.0", 3 ) )
			sessionInfoPtr->protocolFlags |= SSH_PFLAG_NOHASHLENGTH;
		if( !memcmp( subVersionStringPtr, "3.8", 3 ) || \
			!memcmp( subVersionStringPtr, "3.9", 3 ) || \
			( versionStringLength >= 8 + 4 && \
			  !memcmp( subVersionStringPtr, "3.10", 4 ) ) || \
			*subVersionStringPtr >= '4' )
			sessionInfoPtr->protocolFlags |= SSH_PFLAG_PAMPW;
		if( ( !memcmp( subVersionStringPtr, "2.", 2 ) && \
			  subVersionStringPtr[ 2 ] >= '5' ) || \
			( !memcmp( subVersionStringPtr, "3.", 2 ) && \
			  subVersionStringPtr[ 2 ] <= '2' ) )
			sessionInfoPtr->protocolFlags |= SSH_PFLAG_RSASIGPAD;

		return( CRYPT_OK );
		}
	if( versionStringLength >= 14 + 4 && \
		!memcmp( versionStringPtr, "PuTTY_Release_", 14 ) )
		{
		const BYTE *subVersionStringPtr = versionStringPtr + 14;

		if( !memcmp( subVersionStringPtr, "0.59", 4 ) )
			sessionInfoPtr->protocolFlags |= SSH_PFLAG_ZEROLENIGNORE;

		return( CRYPT_OK );
		}
	if( versionStringLength >= 9 && \
		!memcmp( versionStringPtr, "WeOnlyDo ", 9 ) )
		{
		const BYTE *subVersionStringPtr = versionStringPtr + 9;

		if( subVersionStringPtr[ 0 ] >= '2' )
			sessionInfoPtr->protocolFlags |= SSH_PFLAG_ASYMMCOPR;
		return( CRYPT_OK );
		}
	if( isDigit( *versionStringPtr ) )
		{
		const BYTE *vendorIDString;
		const int versionDigit = byteToInt( *versionStringPtr );
		int vendorIDStringLength;

		/* Look for a vendor ID after the version information.  This breaks 
		   down the string "[SSH-x.y-]x.yy vendor-text" to 
		   'versionStringPtr = "x.yy"' and 'vendorIDString = "vendor-text"' */
		for( vendorIDStringLength = versionStringLength, \
				vendorIDString = versionStringPtr;
			 vendorIDStringLength > 0 && *vendorIDString != ' ';
			 vendorIDStringLength--, vendorIDString++ );
		if( vendorIDStringLength > 1 )
			{
			/* There's a vendor ID present, skip the ' ' separator */
			vendorIDString++, vendorIDStringLength--;
			}
		ENSURES( vendorIDStringLength >= 0 && \
				 vendorIDStringLength < SSH_ID_MAX_SIZE );
		switch( versionDigit )
			{
			case '1':
				if( versionStringLength >= 12 && \
					!memcmp( versionStringPtr, "1.7 SecureFX", 12 ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_NOHASHLENGTH;
				if( versionStringLength == 3 && \
					!memcmp( versionStringPtr, "1.0", 3 ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_CUTEFTP;
				if( ( vendorIDStringLength > 8 && \
					!memcmp( vendorIDString, "sshlib: ", 8 ) > 0 ) || \
					( vendorIDStringLength > 9 && \
					!memcmp( vendorIDString, "FlowSsh: ", 9 ) > 0 ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_ASYMMCOPR;
				break;

			case '2':
				if( vendorIDStringLength >= 6 && \
					!memcmp( vendorIDString, "VShell", 6 ) )
					break;	/* Make sure that it isn't VShell */

				/* ssh.com 2.x versions have quite a number of bugs so we
				   check for them as a group */
				if( ( versionStringLength >= 5 && \
					  !memcmp( versionStringPtr, "2.0.0", 5 ) ) || \
					( versionStringLength >= 6 && \
					  !memcmp( versionStringPtr, "2.0.10", 6 ) ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_NOHASHSECRET;
				if( !memcmp( versionStringPtr, "2.0", 3 ) || \
					!memcmp( versionStringPtr, "2.1", 3 ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_SIGFORMAT;
				if( !memcmp( versionStringPtr, "2.0", 3 ) || \
					!memcmp( versionStringPtr, "2.1", 3 ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_WINDOWSIZE;
				if( !memcmp( versionStringPtr, "2.1", 3 ) || \
					!memcmp( versionStringPtr, "2.2", 3 ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_NOHASHLENGTH;
				if( !memcmp( versionStringPtr, "2.0", 3 ) || \
					!memcmp( versionStringPtr, "2.1", 3 ) || \
					!memcmp( versionStringPtr, "2.2", 3 ) || \
					( versionStringLength >= 5 && \
					  !memcmp( versionStringPtr, "2.3.0", 5 ) ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_HMACKEYSIZE;
				if( !memcmp( versionStringPtr, "2.0", 3 ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_EMPTYSVCACCEPT;
				if( !memcmp( versionStringPtr, "2.", 2 ) )
					{
					/* Not sure of the exact versions where this occurs */
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_EMPTYUSERAUTH;
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_TEXTDIAGS;
					}
				break;

			case '3':
				if( versionStringLength >= 13 && \
					!memcmp( versionStringPtr, "3.0 SecureCRT", 13 ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_NOHASHLENGTH;
				break;

			case '5':
				if( versionStringLength >= 10 && \
					!memcmp( vendorIDString, "SSH Tectia", 10 ) )
					sessionInfoPtr->protocolFlags |= SSH_PFLAG_DUMMYUSERAUTH;
			}
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

/* Connect to an SSH server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initVersion( INOUT SESSION_INFO *sessionInfoPtr,
						INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	/* Set up handshake function pointers based on the protocol version */
	status = readVersionString( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
#ifdef USE_SSH1
	if( sessionInfoPtr->version == 1 )
		{
		initSSH1processing( sessionInfoPtr, handshakeInfo,
							isServer( sessionInfoPtr ) ? TRUE : FALSE );
		sessionInfoPtr->sendBufStartOfs = \
			sessionInfoPtr->receiveBufStartOfs = \
				sessionInfoPtr->protocolInfo->sendBufStartOfs;
		return( CRYPT_OK );
		}
#endif /* USE_SSH1 */
	initSSH2processing( sessionInfoPtr, handshakeInfo,
						isServer( sessionInfoPtr ) ? TRUE : FALSE );

	/* SSHv2 hashes parts of the handshake messages for integrity-protection
	   purposes so we create a context for the hash.  In addition since the 
	   handshake can retroactively switch to a different hash algorithm mid-
	   exchange we have to speculatively hash the messages with SHA2 as well
	   as SHA1 in case the other side decides to switch */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA1 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	handshakeInfo->iExchangeHashContext = createInfo.cryptHandle;
	if( algoAvailable( CRYPT_ALGO_SHA2 ) )
		{
		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA2 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
								  &createInfo, OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( handshakeInfo->iExchangeHashContext, 
							  IMESSAGE_DECREFCOUNT );
			handshakeInfo->iExchangeHashContext = CRYPT_ERROR;
			return( status );
			}
		handshakeInfo->iExchangeHashAltContext = createInfo.cryptHandle;
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int completeHandshake( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	status = handshakeInfo->completeHandshake( sessionInfoPtr,
											   handshakeInfo );
	destroyHandshakeInfo( handshakeInfo );
	if( cryptStatusError( status ) )
		{
		/* If we need confirmation from the user before continuing, let
		   them know */
		if( status == CRYPT_ENVELOPE_RESOURCE )
			return( status );

		/* At this point we could be in the secure state so we have to
		   keep the security information around until after we've called 
		   the shutdown function, which could require sending secured 
		   data */
		disableErrorReporting( sessionInfoPtr );
		sessionInfoPtr->shutdownFunction( sessionInfoPtr );
		destroySecurityContextsSSH( sessionInfoPtr );
		return( status );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int completeStartup( INOUT SESSION_INFO *sessionInfoPtr )
	{
	SSH_HANDSHAKE_INFO handshakeInfo;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Initialise the handshake information and begin the handshake.  Since 
	   we don't know what type of peer we're talking to and since the 
	   protocols aren't compatible in anything but name we have to peek at 
	   the peer's initial communication and, in initVersion(), redirect 
	   function pointers based on that */
	status = initHandshakeInfo( &handshakeInfo );
	if( cryptStatusOK( status ) )
		status = initVersion( sessionInfoPtr, &handshakeInfo );
	if( cryptStatusOK( status ) )
		status = handshakeInfo.beginHandshake( sessionInfoPtr,
											   &handshakeInfo );
	if( cryptStatusError( status ) )
		{
		/* If we run into an error at this point we need to disable error-
		   reporting during the shutdown phase since we've already got 
		   status information present from the already-encountered error */
		destroyHandshakeInfo( &handshakeInfo );
		disableErrorReporting( sessionInfoPtr );
		sessionInfoPtr->shutdownFunction( sessionInfoPtr );
		return( status );
		}

	/* Exchange a key with the server */
	status = handshakeInfo.exchangeKeys( sessionInfoPtr, &handshakeInfo );
	if( cryptStatusError( status ) )
		{
		destroySecurityContextsSSH( sessionInfoPtr );
		destroyHandshakeInfo( &handshakeInfo );
		disableErrorReporting( sessionInfoPtr );
		sessionInfoPtr->shutdownFunction( sessionInfoPtr );
		return( status );
		}

	/* Complete the handshake */
	return( completeHandshake( sessionInfoPtr, &handshakeInfo ) );
	}

/* Start an SSH server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int serverStartup( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const char *idString = ( sessionInfoPtr->version == 1 ) ? \
						   SSH1_ID_STRING "\n" : SSH2_ID_STRING "\r\n";
	const int idStringLen = SSH_ID_STRING_SIZE + \
							( ( sessionInfoPtr->version == 1 ) ? 1 : 2 );
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* If we're completing a handshake that was interrupted while we got
	   confirmation of the client auth, skip the initial handshake stages
	   and go straight to the handshake completion stage */
	if( sessionInfoPtr->flags & SESSION_PARTIALOPEN )
		{
		SSH_HANDSHAKE_INFO handshakeInfo;

		status = initHandshakeInfo( &handshakeInfo );
		if( cryptStatusError( status ) )
			return( status );
		initSSH2processing( sessionInfoPtr, &handshakeInfo, TRUE );
		return( completeHandshake( sessionInfoPtr, &handshakeInfo ) );
		}

	/* Send the ID string to the client before we continue with the
	   handshake.  We don't have to wait for any input from the client since
	   we know that if we got here there's a client listening.  Note that
	   standard cryptlib practice for sessions is to wait for input from the
	   client, make sure that it looks reasonable, and only then send back a
	   reply of any kind.  If anything that doesn't look right arrives, we
	   close the connection immediately without any response.  Unfortunately
	   this isn't possible with SSH, which requires that the server send data
	   before the client does */
	status = swrite( &sessionInfoPtr->stream, idString, idStringLen );
	if( cryptStatusError( status ) )
		return( status );

	/* Complete the handshake in the shared code */
	return( completeStartup( sessionInfoPtr ) );
	}

/****************************************************************************
*																			*
*						Control Information Management Functions			*
*																			*
****************************************************************************/

#ifdef USE_SSH_EXTENDED

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 OUT void *data, 
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( type == CRYPT_SESSINFO_SSH_CHANNEL ||\
			  type == CRYPT_SESSINFO_SSH_CHANNEL_TYPE || \
			  type == CRYPT_SESSINFO_SSH_CHANNEL_ARG1 || \
			  type == CRYPT_SESSINFO_SSH_CHANNEL_ARG2 || \
			  type == CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE );

	if( type == CRYPT_SESSINFO_SSH_CHANNEL || \
		type == CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE )
		{
		status = getChannelAttribute( sessionInfoPtr, type, data );
		}
	else
		{
		MESSAGE_DATA *msgData = data;

		status = getChannelAttributeS( sessionInfoPtr, type, msgData->data, 
									   msgData->length, &msgData->length );
		}
	return( ( status == CRYPT_ERROR ) ? CRYPT_ARGERROR_NUM1 : status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int setAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 IN const void *data,
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	int value = DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, sizeof( int ) ) );

	REQUIRES( type == CRYPT_SESSINFO_SSH_CHANNEL || \
			  type == CRYPT_SESSINFO_SSH_CHANNEL_TYPE || \
			  type == CRYPT_SESSINFO_SSH_CHANNEL_ARG1 || \
			  type == CRYPT_SESSINFO_SSH_CHANNEL_ARG2 || \
			  type == CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE );

	/* Get the data value if it's an integer parameter */
	if( type == CRYPT_SESSINFO_SSH_CHANNEL || \
		type == CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE )
		value = *( ( int * ) data );

	/* If we're selecting a channel and there's unwritten data from a
	   previous write still in the buffer, we can't change the write
	   channel */
	if( type == CRYPT_SESSINFO_SSH_CHANNEL && sessionInfoPtr->partialWrite )
		return( CRYPT_ERROR_INCOMPLETE );

	/* If we're creating a new channel by setting the value to CRYPT_UNUSED,
	   create the new channel */
	if( type == CRYPT_SESSINFO_SSH_CHANNEL && value == CRYPT_UNUSED )
		{
		/* If the session hasn't been activated yet, we can only create a
		   single channel during session activation, any subsequent ones
		   have to be handled later */
		if( !( sessionInfoPtr->flags & SESSION_ISOPEN ) && \
			getCurrentChannelNo( sessionInfoPtr, \
								 CHANNEL_READ ) != UNUSED_CHANNEL_NO )
			return( CRYPT_ERROR_INITED );

		return( createChannel( sessionInfoPtr ) );
		}

	/* If we 're setting the channel-active attribute, this implicitly
	   activates or deactivates the channel rather than setting any
	   attribute value */
	if( type == CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE )
		{
		if( value )
			return( sendChannelOpen( sessionInfoPtr ) );
		return( closeChannel( sessionInfoPtr, FALSE ) );
		}

	if( type == CRYPT_SESSINFO_SSH_CHANNEL )
		status = setChannelAttribute( sessionInfoPtr, type, value );
	else
		{
		const MESSAGE_DATA *msgData = data;

		status = setChannelAttributeS( sessionInfoPtr, type, msgData->data, 
									   msgData->length );
		}
	return( ( status == CRYPT_ERROR ) ? CRYPT_ARGERROR_NUM1 : status );
	}
#endif /* USE_SSH_EXTENDED */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkAttributeFunction( SESSION_INFO *sessionInfoPtr,
								   IN const void *data,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	const CRYPT_CONTEXT cryptContext = *( ( CRYPT_CONTEXT * ) data );
	MESSAGE_DATA msgData;
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	STREAM stream;
	BYTE buffer[ 128 + ( CRYPT_MAX_PKCSIZE * 4 ) + 8 ];
	BYTE fingerPrint[ CRYPT_MAX_HASHSIZE + 8 ];
	void *blobData = DUMMY_INIT_PTR;
	int blobDataLength = DUMMY_INIT, hashSize, pkcAlgo, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( isAttribute( type ) );

	if( type != CRYPT_SESSINFO_PRIVATEKEY )
		return( CRYPT_OK );

	/* If it's an ECC key then it has to be one of NIST { P256, P384, P521 }.
	   Unfortunately there's no easy way to determine whether the curve 
	   being used is an SSH-compatible one or not since the user could load
	   their own custom 256-bit curve, or conversely load a known NIST curve 
	   as a series of discrete key parameters, for now we just assume that a 
	   curve of the given size is the correct one */
	status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE, 
							  &pkcAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	if( isEccAlgo( pkcAlgo ) )
		{
		int keySize;

		status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE, 
								  &keySize, CRYPT_CTXINFO_KEYSIZE );
		if( cryptStatusError( status ) )
			return( status );
		if( keySize != bitsToBytes( 256 ) && \
			keySize != bitsToBytes( 384 ) && \
			keySize != bitsToBytes( 521 ) )
			return( CRYPT_ARGERROR_NUM1 );
		}

	/* Only the server key has a fingerprint */
	if( !isServer( sessionInfoPtr ) )
		return( CRYPT_OK );

	getHashAtomicParameters( CRYPT_ALGO_MD5, 0, &hashFunctionAtomic, 
							 &hashSize );

	/* The fingerprint is computed from the "key blob", which is different
	   from the server key.  The server key is the full key while the "key
	   blob" is only the raw key components (e, n for RSA, p, q, g, y for
	   DSA) so we have to skip the key header before we hash the key data:

		uint32		length
			string	algorithm
			byte[]	key_blob

	   Note that, as with the old PGP 2.x key hash mechanism, this allows
	   key spoofing (although it isn't quite as bad as the PGP 2.x key
	   fingerprint mechanism) since it doesn't hash an indication of the key
	   type or format */
	setMessageData( &msgData, buffer, 128 + ( CRYPT_MAX_PKCSIZE * 4 ) );
	status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_KEY_SSH );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, buffer, msgData.length );
	readUint32( &stream );					/* Length */
	status = readUniversal32( &stream );	/* Algorithm ID */
	if( cryptStatusOK( status ) )
		status = sMemGetDataBlockRemaining( &stream, &blobData, 
											&blobDataLength );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	hashFunctionAtomic( fingerPrint, CRYPT_MAX_HASHSIZE, blobData, 
						blobDataLength );

	/* Add the fingerprint */
	return( addSessionInfoS( &sessionInfoPtr->attributeList,
							 CRYPT_SESSINFO_SERVER_FINGERPRINT,
							 fingerPrint, hashSize ) );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodSSH( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Set the access method pointers */
#ifdef USE_SSH_EXTENDED
	sessionInfoPtr->getAttributeFunction = getAttributeFunction;
	sessionInfoPtr->setAttributeFunction = setAttributeFunction;
#endif /* USE_SSH_EXTENDED */
	sessionInfoPtr->checkAttributeFunction = checkAttributeFunction;
	if( isServer( sessionInfoPtr ) )
		{
		sessionInfoPtr->transactFunction = serverStartup;
		initSSH2processing( sessionInfoPtr, NULL, TRUE );
		}
	else
		{
		sessionInfoPtr->transactFunction = completeStartup;
		initSSH2processing( sessionInfoPtr, NULL, FALSE );
		}

	return( CRYPT_OK );
	}
#endif /* USE_SSH || USE_SSH1 */
