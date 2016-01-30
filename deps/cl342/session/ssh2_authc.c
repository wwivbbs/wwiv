/****************************************************************************
*																			*
*			cryptlib SSHv2 Client-side Authentication Management			*
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

/* The SSH authentication process is awkward and complex and even with the 
   significant simplifications applied here still ends up a lot more 
   convoluted than it should be, particularly when we have to add all sorts
   of special-case handling for valid but unusual interpretations of the
   spec.  The overall control flow (excluding handling of special-case
   peculiarities) is:

	reportAuthFailure:
		read allowed methods;
		if( method == PAM && !no_more_PAM )
			return( processPAM() );
		return error;

	processPAM:
		perform PAM authentication;
		if( fail )
			return( reportAuthFailure( no_more_PAM ) );
		return error/success;

	processClientAuth:
		send auth;
		if( success )
			return success;
		return( reportAuthFailure() ); */

#ifdef USE_SSH

/* Tables mapping SSH algorithm names to cryptlib algorithm IDs, in
   preferred algorithm order.  There are two of these, one that favours
   password-based authentication and one that favours PKC-based
   authentication, depending on whether the user has specified a password
   or a PKC as their authentication choice.  This is required in order to
   handle SSH's weird way of reporting authentication failures, see the
   comment in reportAuthFailure() for details */

static const ALGO_STRING_INFO FAR_BSS algoStringUserauthentPWTbl[] = {
	{ "password", 8, CRYPT_PSEUDOALGO_PASSWORD },
	{ "keyboard-interactive", 20, CRYPT_PSEUDOALGO_PAM },
	{ "publickey", 9, CRYPT_ALGO_RSA },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};
static const ALGO_STRING_INFO FAR_BSS algoStringUserauthentPKCTbl[] = {
	{ "publickey", 9, CRYPT_ALGO_RSA },
	{ "password", 8, CRYPT_PSEUDOALGO_PASSWORD },
	{ "keyboard-interactive", 20, CRYPT_PSEUDOALGO_PAM },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, CRYPT_ALGO_NONE }
	};

/* Forward declaration for authentication function */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processPamAuthentication( INOUT SESSION_INFO *sessionInfoPtr );

/****************************************************************************
*																			*
*							Utility Functions								*
*																			*
****************************************************************************/

/* Send a dummy authentication request, needed under various circumstances 
   for some buggy servers */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sendDummyAuth( INOUT SESSION_INFO *sessionInfoPtr,
						  IN_BUFFER( userNameLength ) const char *userName,
						  IN_LENGTH_TEXT const int userNameLength )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( userName, userNameLength ) );

	REQUIRES( userNameLength > 0 && userNameLength <= CRYPT_MAX_TEXTSIZE );

	/* Send the dummy authentication request to the server */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_USERAUTH_REQUEST );
	if( cryptStatusError( status ) )
		return( status );
	writeString32( &stream, userName, userNameLength );
	writeString32( &stream, "ssh-connection", 14 );
	status = writeString32( &stream, "none", 4 );
	if( cryptStatusOK( status ) )
		status = wrapPacketSSH2( sessionInfoPtr, &stream, 0, FALSE, TRUE );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream, TRUE );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Wait for the server's ack of the authentication.  In theory since 
	   this is just something used to de-confuse the buggy Tectia/ssh.com 
	   server we can ignore the content (as long as the packet's valid) as 
	   any authentication problems will be resolved by the real 
	   authentication process below, but in some rare cases we can encounter 
	   oddball devices that don't perform any authentication at the SSH 
	   level but instead ask for a password via the protocol being tunneled 
	   over SSH, which means that we'll get a USERAUTH_SUCCESS at this point 
	   even if we haven't actually authenticated ourselves */
	status = readHSPacketSSH2( sessionInfoPtr, SSH_MSG_SPECIAL_USERAUTH, 
							   ID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionInfoPtr->sessionSSH->packetType == SSH_MSG_USERAUTH_SUCCESS )
		{
		/* We're (non-)authenticated, we don't have to do anything 
		   further */
		return( OK_SPECIAL );
		}

	return( CRYPT_OK );
	}

/* Report specific details on an authentication failure to the caller */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int reportAuthFailure( INOUT SESSION_INFO *sessionInfoPtr,
							  IN_LENGTH_SHORT const int length, 
							  const BOOLEAN isPamAuth )
	{
	STREAM stream;
	CRYPT_ALGO_TYPE authentAlgo;
	const BOOLEAN hasPassword = \
			( findSessionInfo( sessionInfoPtr->attributeList,
							   CRYPT_SESSINFO_PASSWORD ) != NULL ) ? \
			TRUE : FALSE;
	int status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH_SHORT );

	/* The authentication failed, pick apart the response to see if we can
	   return more meaningful error information:

		byte	type = SSH_MSG_USERAUTH_FAILURE
		string	available_auth_types
		boolean	partial_success

	  We decode the response to favour password- or PKC-based
	  authentication depending on whether the user specified a password
	  or a PKC as their authentication choice.

	  God knows how the partial_success flag is really meant to be applied
	  (there are a whole pile of odd conditions surrounding changed
	  passwords and similar issues), according to the spec it means that the
	  authentication was successful, however the packet type indicates that
	  the authentication failed and something else is needed.  This whole
	  section of the protocol winds up in an extremely complex state machine
	  with all sorts of special-case conditions, several of which require
	  manual intervention by the user in order to continue.  It's easiest to 
	  not even try and handle this stuff */
	ENSURES( rangeCheckZ( 0, length, sessionInfoPtr->receiveBufSize ) );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	if( hasPassword )
		{
		status = readAlgoString( &stream, algoStringUserauthentPWTbl,
								 FAILSAFE_ARRAYSIZE( algoStringUserauthentPWTbl, \
													 ALGO_STRING_INFO ),
								 &authentAlgo, FALSE, SESSION_ERRINFO );
		}
	else
		{
		status = readAlgoString( &stream, algoStringUserauthentPKCTbl,
								 FAILSAFE_ARRAYSIZE( algoStringUserauthentPKCTbl, \
													 ALGO_STRING_INFO ),
								 &authentAlgo, FALSE, SESSION_ERRINFO );
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		/* If the problem is due to lack of a compatible algorithm, make the
		   error message a bit more specific to tell the user that we got
		   through most of the handshake but failed at the authentication
		   stage */
		if( status == CRYPT_ERROR_NOTAVAIL )
			{
			retExt( CRYPT_ERROR_NOTAVAIL,
					( CRYPT_ERROR_NOTAVAIL, SESSION_ERRINFO, 
					  "Remote system supports neither password nor "
					  "public-key authentication" ) );
			}

		/* Some buggy implementations return an authentication failure with
		   the available-authentication types string empty to indicate that 
		   no authentication is required rather than returning an 
		   authentication success status as required by the spec (although 
		   the problem is really in the spec and not in the interpretation
		   since there's no way to say that "no-auth" is a valid 
		   authentication type).  If we find one of these then we return an 
		   OK_SPECIAL status to let the caller know that they should retry 
		   the authentication.  Because this is a broken packet format we 
		   have to check the encoded form rather than being able to read it 
		   as normal */
		if( ( sessionInfoPtr->protocolFlags & SSH_PFLAG_EMPTYUSERAUTH ) && \
			length >= LENGTH_SIZE && \
			!memcmp( sessionInfoPtr->receiveBuffer,
					 "\x00\x00\x00\x00", LENGTH_SIZE ) )
			{
			/* It's a garbled attempt to tell us that no authentication is
			   required, tell the caller to try again without 
			   authentication */
			return( OK_SPECIAL );
			}

		/* There was some other problem with the returned information, we
		   still report it as a failed-authentication error but leave the
		   extended error information in place to let the caller see what 
		   the underlying cause was */
		return( CRYPT_ERROR_WRONGKEY );
		}

	/* If we tried straight password authentication and the server requested 
	   keyboard-interactive (== misnamed PAM) authentication try again using 
	   PAM authentication unless we've already been called as a result of 
	   failed PAM authentication */
	if( !isPamAuth && hasPassword && authentAlgo == CRYPT_PSEUDOALGO_PAM )
		return( processPamAuthentication( sessionInfoPtr ) );

	/* SSH reports authentication failures in a somewhat bizarre way,
	   instead of saying "authentication failed" it returns a list of
	   allowed authentication methods, one of which may be the one that we
	   just used.  To figure out whether a problem occurred because we used 
	   the wrong authentication method or because we submitted the wrong 
	   authentication value we have to perform a complex decode and match of 
	   the information in the returned packet with what we sent */
	if( !hasPassword )
		{
		/* If we used a PKC and the server wants a password, report the
		   error as a missing password */
		if( authentAlgo == CRYPT_PSEUDOALGO_PASSWORD || \
			authentAlgo == CRYPT_PSEUDOALGO_PAM )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_PASSWORD,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			retExt( CRYPT_ERROR_NOTINITED,
					( CRYPT_ERROR_NOTINITED, SESSION_ERRINFO, 
					  "Server requested password authentication but only a "
					  "public/private key was available" ) );
			}

		retExt( CRYPT_ERROR_WRONGKEY,
				( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
				  "Server reported: Invalid public-key authentication" ) );
		}

	/* If we used a password and the server wants a PKC, report the error
	   as a missing private key */
	if( isSigAlgo( authentAlgo ) )
		{
		setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_PRIVATEKEY,
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		retExt( CRYPT_ERROR_NOTINITED,
				( CRYPT_ERROR_NOTINITED, SESSION_ERRINFO, 
				  "Server requested public-key authentication but only a "
				  "password was available" ) );
		}

	retExt( CRYPT_ERROR_WRONGKEY,
			( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
			  "Server reported: Invalid password" ) );
	}

/* Create a public-key authentication packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int createPubkeyAuth( const SESSION_INFO *sessionInfoPtr,
							 const SSH_HANDSHAKE_INFO *handshakeInfo,
							 INOUT STREAM *stream,
							 const ATTRIBUTE_LIST *userNamePtr )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	void *sigDataPtr, *packetDataPtr;
	int sigDataLength, packetDataLength;
	int sigOffset, sigLength = DUMMY_INIT, pkcAlgo, status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( userNamePtr, sizeof( ATTRIBUTE_LIST ) ) );

	status = krnlSendMessage( sessionInfoPtr->privateKey, 
							  IMESSAGE_GETATTRIBUTE, &pkcAlgo, 
							  CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );

	/*	byte	type = SSH_MSG_USERAUTH_REQUEST
		string	user_name
		string	service_name = "ssh-connection"
		string	method-name = "publickey"
		boolean	TRUE
		string		"ssh-rsa"	"ssh-dss"
		string		[ client key/certificate ]
			string	"ssh-rsa"	"ssh-dss"
			mpint	e			p
			mpint	n			q
			mpint				g
			mpint				y
		string		[ client signature ]
			string	"ssh-rsa"	"ssh-dss"
			string	signature	signature.

	   Note the doubled-up algorithm name, the spec first requires that the 
	   public-key authentication packet send the algorithm name and then 
	   includes it a second time as part of the client key information (and
	   then just for good measure specifies it a third time in the 
	   signature) */
	streamBookmarkSetFullPacket( stream, packetDataLength );
	writeString32( stream, userNamePtr->value, userNamePtr->valueLength );
	writeString32( stream, "ssh-connection", 14 );
	writeString32( stream, "publickey", 9 );
	sputc( stream, 1 );
	status = writeAlgoString( stream, pkcAlgo );
	if( cryptStatusError( status ) )
		return( status );
	status = exportAttributeToStream( stream, sessionInfoPtr->privateKey,
									  CRYPT_IATTRIBUTE_KEY_SSH );
	if( cryptStatusError( status ) )
		return( status );
	status = streamBookmarkComplete( stream, &packetDataPtr, 
									 &packetDataLength, packetDataLength );
	if( cryptStatusError( status ) )
		return( status );

	/* Hash the authentication request data, composed of:

		string		exchange hash
		[ SSH_MSG_USERAUTH_REQUEST packet payload up to signature start ] */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA1 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionInfoPtr->protocolFlags & SSH_PFLAG_NOHASHLENGTH )
		{
		/* Some implementations erroneously omit the length when hashing the 
		   exchange hash */
		status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_CTX_HASH,
								  ( MESSAGE_CAST ) handshakeInfo->sessionID,
								  handshakeInfo->sessionIDlength );
		}
	else
		{
		status = hashAsString( createInfo.cryptHandle, 
							   handshakeInfo->sessionID,
							   handshakeInfo->sessionIDlength );
		}
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_CTX_HASH,
								  packetDataPtr, packetDataLength );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Sign the hash.  The reason for the min() part of the expression is 
	   that iCryptCreateSignature() gets suspicious of very large buffer 
	   sizes, for example when the user has specified the use of a huge send 
	   buffer */
	sigOffset = stell( stream );	/* Needed for later bug workaround */
	status = sMemGetDataBlockRemaining( stream, &sigDataPtr, &sigDataLength );
	if( cryptStatusOK( status ) )
		{
		status = iCryptCreateSignature( sigDataPtr, 
						min( sigDataLength, MAX_INTLENGTH_SHORT - 1 ), 
						&sigLength, CRYPT_IFORMAT_SSH, 
						sessionInfoPtr->privateKey, createInfo.cryptHandle, 
						NULL );
		}
	if( cryptStatusOK( status ) )
		status = sSkip( stream, sigLength );
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/* Some buggy implementations require that RSA signatures be padded with 
	   zeroes to the full modulus size, mysteriously failing the 
	   authentication in a small number of randomly-distributed cases when 
	   the signature format happens to be less than the modulus size.  To 
	   handle this we have to rewrite the signature to include the extra
	   padding bytes */
	if( ( sessionInfoPtr->protocolFlags & SSH_PFLAG_RSASIGPAD ) && \
		pkcAlgo == CRYPT_ALGO_RSA )
		{
		BYTE sigDataBuffer[ CRYPT_MAX_PKCSIZE + 8 ];
		int sigSize, keySize, delta, i;

		status = krnlSendMessage( sessionInfoPtr->privateKey, 
								  IMESSAGE_GETATTRIBUTE, &keySize, 
								  CRYPT_CTXINFO_KEYSIZE );
		if( cryptStatusError( status ) )
			return( status );

		/* Read the signature length and check whether it needs padding.  
		   Note that we read the signature data with readString32() rather 
		   than readInteger32() to ensure that we get the raw signature data 
		   exactly as written rather than the cleaned-up integer value */
		sseek( stream, sigOffset );
		readUint32( stream );
		readUniversal32( stream );
		status = readString32( stream, sigDataBuffer, CRYPT_MAX_PKCSIZE,
							   &sigSize );
		ENSURES( cryptStatusOK( status ) );
		if( sigSize >= keySize )
			{
			/* The signature size is the same as the modulus size, there's 
			   nothing to do.  The reads above have reset the stream-
			   position indicator to the end of the signature data so 
			   there's no need to perform an explicit seek before exiting */
			return( CRYPT_OK );
			}

		/* We've got a signature that's shorter than the RSA modulus, we 
		   need to rewrite it to pad it out to the modulus size:

		  sigOfs				  sigDataBuffer
			|							|
			v uint32	string32		v
			+-------+---------------+---+-------------------+
			| length|	algo-name	|pad|	sigData			|
			+-------+---------------+---+-------------------+
			|						|<+>|<---- sigSize ---->|
			|						delta					|
			|						|<------ keySize ------>|
			|<----------------- sigLength ----------------->| */
		delta = keySize - sigSize;
		if( sigLength + delta > sigDataLength )
			return( CRYPT_ERROR_OVERFLOW );
		sseek( stream, sigOffset );
		writeUint32( stream, sizeofString32( "", 7 ) + \
							 sizeofString32( "", keySize ) );
		writeString32( stream, "ssh-rsa", 7 );
		writeUint32( stream, keySize );
		for( i = 0; i < delta; i++ )
			sputc( stream, 0 );
		return( swrite( stream, sigDataBuffer, sigSize ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Perform PAM Authentication						*
*																			*
****************************************************************************/

/* Perform a single round of PAM authentication */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int pamAuthenticate( INOUT SESSION_INFO *sessionInfoPtr,
							IN_BUFFER( pamRequestDataLength ) \
								const void *pamRequestData, 
							IN_LENGTH_SHORT const int pamRequestDataLength )
	{
	const ATTRIBUTE_LIST *passwordPtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_PASSWORD );
	STREAM stream;
	BYTE nameBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BYTE promptBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	int nameLength, promptLength = -1, noPrompts = -1;
	int i, iterationCount, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( pamRequestData, pamRequestDataLength ) );

	REQUIRES( pamRequestDataLength > 0 && \
			  pamRequestDataLength < MAX_INTLENGTH_SHORT );

	/* Process the PAM user-auth request:

		byte	type = SSH_MSG_USERAUTH_INFO_REQUEST
		string	name
		string	instruction
		string	language = {}
		int		num_prompts
			string	prompt[ n ]
			boolean	echo[ n ]

	   Exactly who or what's name is supplied or what the instruction field 
	   is for is left unspecified by the spec (and they may indeed be left 
	   empty) so we just skip it.  Many implementations feel similarly about 
	   this and leave the fields empty.  In addition the spec allows 
	   num_prompts to be zero in one location (stating that the usually-
	   absent name and instruction field should be displayed to the user) 
	   but then prohibits the prompt from being empty in another location, 
	   so we require a nonzero number of prompts.

	   If the PAM authentication (from a previous iteration) fails or 
	   succeeds the server is supposed to send back a standard user-auth 
	   success or failure status but could also send another
	   SSH_MSG_USERAUTH_INFO_REQUEST even if it contains no payload (an 
	   OpenSSH bug) so we have to handle this as a special case */
	sMemConnect( &stream, pamRequestData, pamRequestDataLength );
	status = readString32( &stream, nameBuffer, CRYPT_MAX_TEXTSIZE,
						   &nameLength );		/* Name */
	if( cryptStatusOK( status ) )
		{
		readUniversal32( &stream );				/* Instruction */
		readUniversal32( &stream );				/* Language */
		status = noPrompts = readUint32( &stream );	/* No.prompts */
		if( !cryptStatusError( status ) )
			{
			status = CRYPT_OK;	/* readUint32() returns a count value */
			if( noPrompts <= 0 || noPrompts > 4 )
				{
				/* Requesting zero or more than a small number of prompts is 
				   suspicious */
				status = CRYPT_ERROR_BADDATA;
				}
			}
		}
	if( cryptStatusOK( status ) )
		status = readString32( &stream, promptBuffer, 
							   CRYPT_MAX_TEXTSIZE, &promptLength );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Invalid PAM authentication request packet" ) );
		}

	/* Make sure that we're being asked for some form of password 
	   authentication.  This assumes that the prompt string begins with the 
	   word "password" (which always seems to be the case), if it isn't then 
	   it may be necessary to do a substring search */
	if( promptLength < 8 || strCompare( promptBuffer, "Password", 8 ) )
		{
		/* The following may produce somewhat inconsistent results in terms
		   of what it reports because it's unclear what 'name' actually is, 
		   on the off chance that something fills this in it could produce
		   a less appropriate error message than the prompt, but we 
		   opportunistically try it in case it contains something useful */
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Server requested unknown PAM authentication type '%s'", 
				  ( nameLength > 0 ) ? \
				  sanitiseString( nameBuffer, CRYPT_MAX_TEXTSIZE, \
								  nameLength ) : \
				  sanitiseString( promptBuffer, CRYPT_MAX_TEXTSIZE, \
								  promptLength ) ) );
		}

	REQUIRES( passwordPtr != NULL && \
			  passwordPtr->valueLength > 0 && \
			  passwordPtr->valueLength <= CRYPT_MAX_TEXTSIZE );

	/* Send back the PAM user-auth response:

		byte	type = SSH_MSG_USERAUTH_INFO_RESPONSE
		int		num_responses = num_prompts
		string	response

	   What to do if there's more than one prompt is a bit tricky, usually 
	   PAM is used as a form of (awkward) password authentication and 
	   there's only a single prompt, if we ever encounter a situation where 
	   there's more than one prompt then it's probably a request to confirm 
	   the password so we just send it again for successive prompts */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_USERAUTH_INFO_RESPONSE );
	if( cryptStatusError( status ) )
		return( status );
	status = writeUint32( &stream, noPrompts );
	for( i = 0, iterationCount = 0;
		 cryptStatusOK( status ) && i < noPrompts && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 i++, iterationCount++ )
		{
		status = writeString32( &stream, passwordPtr->value,
								passwordPtr->valueLength );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream, FALSE );
	sMemDisconnect( &stream );

	return( status );
	}

/* Handle PAM authentication */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processPamAuthentication( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const ATTRIBUTE_LIST *userNamePtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_USERNAME );
	STREAM stream;
	int length, pamIteration, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( userNamePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( userNamePtr != NULL && \
			  userNamePtr->valueLength > 0 && \
			  userNamePtr->valueLength <= CRYPT_MAX_TEXTSIZE );

	/* Send a user-auth request asking for PAM authentication:

		byte	type = SSH_MSG_USERAUTH_REQUEST
		string	user_name
		string	service_name = "ssh-connection"
		string	method_name = "keyboard-interactive"
		string	language = ""
		string	sub_methods = "password"

	   The sub-methods are implementation-dependent and the spec suggests an
	   implementation strategy in which the server ignores them so
	   specifying anything here is mostly wishful thinking, but we ask for
	   password authentication anyway in case it helps */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_USERAUTH_REQUEST );
	if( cryptStatusError( status ) )
		return( status );
	writeString32( &stream, userNamePtr->value, userNamePtr->valueLength );
	writeString32( &stream, "ssh-connection", 14 );
	writeString32( &stream, "keyboard-interactive", 20 );
	writeUint32( &stream, 0 );		/* No language tag */
	if( sessionInfoPtr->protocolFlags & SSH_PFLAG_PAMPW )
		{
		/* Some servers choke if we supply a sub-method hint for the
		   authentication */
		status = writeUint32( &stream, 0 );
		}
	else
		status = writeString32( &stream, "password", 8 );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream, FALSE );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Handle the PAM negotiation.  This can (in theory) go on indefinitely,
	   to avoid potential DoS problems we limit it to five iterations.  In
	   general we'll only need two iterations (or three for OpenSSH's empty-
	   message bug) so we shouldn't ever get to five */
	for( pamIteration = 0; pamIteration < 5; pamIteration++ )
		{
		int type;

		/* Read back the response to our last message.  Although the spec
		   requires that the server not respond with a SSH_MSG_USERAUTH_-
		   FAILURE message if the request fails because of an invalid user
		   name (done for cargo-cult reasons to prevent an attacker from 
		   being able to determine valid user names by checking for error 
		   responses, although usability research on real-world users 
		   indicates that this actually reduces security while having little
		   to no tangible benefit) some servers can return a failure 
		   indication at this point so we have to allow for a failure 
		   response as well as the expected SSH_MSG_USERAUTH_INFO_REQUEST */
		status = length = \
			readHSPacketSSH2( sessionInfoPtr, SSH_MSG_SPECIAL_USERAUTH_PAM,
							  ID_SIZE );
		if( cryptStatusError( status ) )
			return( status );
		type = sessionInfoPtr->sessionSSH->packetType;

		/* If we got a success status, we're done */
		if( type == SSH_MSG_USERAUTH_SUCCESS )
			return( CRYPT_OK );

		/* If the authentication failed provide more specific details to the 
		   caller */
		if( type == SSH_MSG_USERAUTH_FAILURE )
			{
			/* If we failed on the first attempt (before we even tried to
			   send a password) it's probably because the user name is
			   invalid (or the server has the SSH_PFLAG_PAMPW bug).  Having
			   the server return a failure due to an invalid user name
			   shouldn't normally happen (see the comment above) but we 
			   handle it just in case */
			if( pamIteration <= 0 )
				{
				char userNameBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];

				memcpy( userNameBuffer, userNamePtr->value,
						userNamePtr->valueLength );
				retExt( CRYPT_ERROR_WRONGKEY,
						( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
						  "Server reported: Invalid user name '%s'",
						  sanitiseString( userNameBuffer,
										  CRYPT_MAX_TEXTSIZE,
									      userNamePtr->valueLength ) ) );
				}

			/* It's a failure after we've tried to authenticate ourselves,
			   report the details to the caller */
			return( reportAuthFailure( sessionInfoPtr, length, TRUE ) );
			}
		ENSURES( type == SSH_MSG_USERAUTH_INFO_REQUEST )
				 /* Guaranteed by the packet read with type = 
				    SSH_MSG_SPECIAL_USERAUTH_PAM */

		/* Perform the PAM authentication */
		status = pamAuthenticate( sessionInfoPtr, 
								  sessionInfoPtr->receiveBuffer, length );
		if( cryptStatusError( status ) )
			return( status );
		}

	retExt( CRYPT_ERROR_OVERFLOW,
			( CRYPT_ERROR_OVERFLOW, SESSION_ERRINFO, 
			  "Too many iterations of negotiation during PAM "
			  "authentication" ) );
	}

/****************************************************************************
*																			*
*						Perform Client-side Authentication					*
*																			*
****************************************************************************/

/* Authenticate the client to the server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processClientAuth( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	const ATTRIBUTE_LIST *userNamePtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_USERNAME );
	const ATTRIBUTE_LIST *passwordPtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_PASSWORD );
	STREAM stream;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( userNamePtr != NULL );

	/* The buggy Tectia/ssh.com server requires a dummy request for
	   authentication methods otherwise it will reject any method other 
	   than 'password' as invalid with the error "Client requested 
	   non-existing method 'publickey'".  To work around this we submit a 
	   dummy authentication request using the method 'none' */
	if( sessionInfoPtr->protocolFlags & SSH_PFLAG_DUMMYUSERAUTH )
		{
		status = sendDummyAuth( sessionInfoPtr, userNamePtr->value, 
								userNamePtr->valueLength );
		if( cryptStatusError( status ) )
			{
			/* Under some special circumstances involving other server bugs 
			   (see the comment in sendDummyAuth() for details) this dummy 
			   authenticate can get us in, in which case we're done */
			if( status == OK_SPECIAL )
				return( CRYPT_OK );

			return( status );
			}
		}

	/* The way in which we handle authentication here isn't totally
	   appropriate since we assume that the user knows the appropriate form
	   of authentication to use.  If they're ambiguous and supply both a
	   password and a private key and the server only accepts PKC-based
	   authentication we'll always preferentially choose password-based
	   authentication.  The way around this is to send an authentication 
	   request with a method-type of "none" to see what the server wants but 
	   the only thing that cryptlib can do in this case (since it's non-
	   interactive during the handshake phase) is disconnect, tell the user 
	   what went wrong, and try again.  The current mechanism does this 
	   anyway so we don't gain much except extra RTT delays by adding this 
	   question-and-answer facility */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_USERAUTH_REQUEST );
	if( cryptStatusError( status ) )
		return( status );
	if( passwordPtr != NULL )
		{
		/*	byte	type = SSH_MSG_USERAUTH_REQUEST
			string	user_name
			string	service_name = "ssh-connection"
			string	method-name = "password"
			boolean	FALSE
			string	password */
		writeString32( &stream, userNamePtr->value, 
					   userNamePtr->valueLength );
		writeString32( &stream, "ssh-connection", 14 );
		writeString32( &stream, "password", 8 );
		sputc( &stream, 0 );
		status = writeString32( &stream, passwordPtr->value,
								passwordPtr->valueLength );
		}
	else
		{
		status = createPubkeyAuth( sessionInfoPtr, handshakeInfo, &stream, 
								   userNamePtr );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	/* Send the authentication information to the server */
	status = wrapPacketSSH2( sessionInfoPtr, &stream, 0, TRUE, TRUE );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream, TRUE );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Wait for the server's ack of the authentication */
	status = length = \
		readHSPacketSSH2( sessionInfoPtr, SSH_MSG_SPECIAL_USERAUTH, 
						  ID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionInfoPtr->sessionSSH->packetType == SSH_MSG_USERAUTH_SUCCESS )
		{
		/* We've successfully authenticated ourselves and we're done */
		return( CRYPT_OK );
		}

	/* The authentication failed, provide more specific details for the 
	   caller, with an optional fallback to PAM authentication if the server 
	   requested it.  Since this fallback can result in a successful 
	   authentication (via the PAM fallback) we check for this as a return
	   status and convert the overall failure status that we'd be returning 
	   at this point into a success status */
	status = reportAuthFailure( sessionInfoPtr, length, FALSE );
	if( cryptStatusOK( status ) )
		{
		/* The fallback to PAM authentication succeeded, we're done */
		return( CRYPT_OK );
		}
	if( status != OK_SPECIAL )
		return( status );

	/* Some buggy implementations return an authentication failure as a 
	   garbled attempt to tell us that no authentication is required, if we 
	   encounter one of these (indicated by reportAuthFailure() returning an
	   OK_SPECIAL status) then we retry by sending a dummy authentication 
	   request, which should get us in.

	   The handling of return codes at this point is somewhat different from
	   normal because we're already in a failure state so even a successful
	   return (indicating that the dummy-auth was successfully sent) has to 
	   be converted into an overall failure status */
	status = sendDummyAuth( sessionInfoPtr, userNamePtr->value, 
							userNamePtr->valueLength );
	if( status == OK_SPECIAL )
		{
		/* If we got in with the dummy authenticate, we're done */
		return( CRYPT_OK );
		}
	return( cryptStatusOK( status ) ? CRYPT_ERROR_WRONGKEY : status );
	}
#endif /* USE_SSH */
