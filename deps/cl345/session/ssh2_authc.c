/****************************************************************************
*																			*
*			cryptlib SSHv2 Client-side Authentication Management			*
*						Copyright Peter Gutmann 1998-2017					*
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

	processAuthFailure:
		readAuthFailureInfo();
		reportAuthFailure();

	processPAM:
		perform PAM authentication;
		if( fail )
			return( processAuthFailure() );
		return error/success;

	processClientAuth:
		if( hasPassword )
			send password auth;
		else
			send PKC auth;
		if( success )
			return success;
		required = readAuthFailureInfo();
		if( required == PAM )
			processPAM();
		if( required == further auth )
			send PKC auth if available;
		return( processAuthFailure() ); */

#ifdef USE_SSH

/* Tables mapping SSH algorithm names to cryptlib algorithm IDs, in
   preferred algorithm order.  There are two of these, one that favours
   password-based authentication and one that favours PKC-based
   authentication, depending on whether the user has specified a password
   or a PKC as their authentication choice.  This is required in order to
   handle SSH's weird way of reporting authentication failures, see the
   comment in reportAuthFailure() for details */

static const ALGO_STRING_INFO algoStringUserauthentPWTbl[] = {
	{ "password", 8, MK_ALGO( PSEUDOALGO_PASSWORD ) },
	{ "keyboard-interactive", 20, MK_ALGO( PSEUDOALGO_PAM ) },
	{ "publickey", 9, CRYPT_ALGO_RSA },	/* Representative PKC */
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};
static const ALGO_STRING_INFO algoStringUserauthentPKCTbl[] = {
	{ "publickey", 9, CRYPT_ALGO_RSA },	/* Representative PKC */
	{ "password", 8, MK_ALGO( PSEUDOALGO_PASSWORD ) },
	{ "keyboard-interactive", 20, MK_ALGO( PSEUDOALGO_PAM ) },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, CRYPT_ALGO_NONE }
	};

/****************************************************************************
*																			*
*							Utility Functions								*
*																			*
****************************************************************************/

/* Create a public-key authentication packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int rewriteRSASignature( const SESSION_INFO *sessionInfoPtr,
								INOUT STREAM *stream,
								IN_LENGTH_SHORT const int sigLength,
								IN_LENGTH_SHORT const int sigDataMaxLength )
	{
	BYTE sigDataBuffer[ CRYPT_MAX_PKCSIZE + 8 ];
	const int sigOffset = stell( stream );
	int sigSize, keySize, delta, i, status, LOOP_ITERATOR;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( isShortIntegerRangeNZ( sigLength ) );
	REQUIRES( isShortIntegerRangeNZ( sigDataMaxLength ) );

	status = krnlSendMessage( sessionInfoPtr->privateKey, 
							  IMESSAGE_GETATTRIBUTE, &keySize, 
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the signature length and check whether it needs padding.  Note 
	   that we read the signature data with readString32() rather than 
	   readInteger32() to ensure that we get the raw signature data exactly 
	   as written rather than the cleaned-up integer value */
	readUint32( stream );
	readUniversal32( stream );
	status = readString32( stream, sigDataBuffer, CRYPT_MAX_PKCSIZE,
						   &sigSize );
	ENSURES( cryptStatusOK( status ) );
	if( sigSize >= keySize )
		{
		/* The signature size is the same as the modulus size, there's 
		   nothing to do.  The reads above have reset the stream-position 
		   indicator to the end of the signature data so there's no need to 
		   perform an explicit seek before exiting */
		return( CRYPT_OK );
		}

	/* We've got a signature that's shorter than the RSA modulus, we need to 
	   rewrite it to pad it out to the modulus size:

		  sigOfs				  sigDataBuffer
			|							|
			v uint32	string32		v
			+-------+---------------+---+-------------------+
			| length|	algo-name	|pad|	sigData			|
			+-------+---------------+---+-------------------+
			|						|<+>|<---- sigSize ---->|
			|						delta					|
			|						|<------ keySize ------>|
			|<----------------- sigLength ----------------->|
			|<------------- sigDataMaxLength --------[...]-------->| */
	delta = keySize - sigSize;
	ENSURES( delta > 0 && delta < 16 );
	if( sigLength + delta > sigDataMaxLength )
		return( CRYPT_ERROR_OVERFLOW );
	sseek( stream, sigOffset );
	writeUint32( stream, sizeofString32( 7 ) + \
						 sizeofString32( keySize ) );
	writeString32( stream, "ssh-rsa", 7 );
	writeUint32( stream, keySize );
	LOOP_MED( i = 0, i < delta, i++ )
		sputc( stream, 0 );
	ENSURES( LOOP_BOUND_OK );
	return( swrite( stream, sigDataBuffer, sigSize ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int createPubkeyAuth( const SESSION_INFO *sessionInfoPtr,
							 const SSH_HANDSHAKE_INFO *handshakeInfo,
							 INOUT STREAM *stream,
							 const ATTRIBUTE_LIST *userNamePtr )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	void *sigDataPtr, *packetDataPtr DUMMY_INIT_PTR;
	int sigDataMaxLength, packetDataLength;
	int sigLength DUMMY_INIT, pkcAlgo, status;

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
		string		"ssh-rsa"		"rsa-sha2-256"		"ssh-dss"
		string		[ client key/certificate ]
			string	"ssh-rsa"		"ssh-rsa"			"ssh-dss"
			mpint	e				e					p
			mpint	n				n					q
			mpint										g
			mpint										y
		string		[ client signature ]
			string	"ssh-rsa"		"rsa-sha2-256"		"ssh-dss"
			string	signature		signature			signature

	   Note the multiple copies of the algorithm name, the spec first 
	   requires that the public-key authentication packet send the algorithm 
	   name, then includes it a second time as part of the client key 
	   information, and just for good measure specifies it a third time in 
	   the signature.  However, just to keep everyone on their toes the
	   name changes for RSA with SHA2 so that the key is given the old
	   name and the overall blob and signature are given the new name */
	streamBookmarkSetFullPacket( stream, packetDataLength );
	writeString32( stream, userNamePtr->value, userNamePtr->valueLength );
	writeString32( stream, "ssh-connection", 14 );
	writeString32( stream, "publickey", 9 );
	sputc( stream, 1 );
	status = writeAlgoStringEx( stream, pkcAlgo, handshakeInfo->hashAlgo, 0,
								SSH_ALGOSTRINGINFO_NONE );
	if( cryptStatusOK( status ) )
		{
		status = exportAttributeToStream( stream, sessionInfoPtr->privateKey,
										  CRYPT_IATTRIBUTE_KEY_SSH );
		}
	if( cryptStatusOK( status ) )
		{
		status = streamBookmarkComplete( stream, &packetDataPtr, 
										 &packetDataLength, 
										 packetDataLength );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Hash the authentication request data, composed of:

		string		exchange hash
		[ SSH_MSG_USERAUTH_REQUEST packet payload up to signature start ] */
	setMessageCreateObjectInfo( &createInfo, handshakeInfo->hashAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSH_PFLAG_NOHASHLENGTH ) )
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
		{
		status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_CTX_HASH,
								  packetDataPtr, packetDataLength );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_CTX_HASH, "", 0 );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Sign the hash.  The reason for the min() part of the expression is 
	   that iCryptCreateSignature() gets suspicious of very large buffer 
	   sizes, for example when the user has specified the use of a huge send 
	   buffer */
	status = sMemGetDataBlockRemaining( stream, &sigDataPtr, 
										&sigDataMaxLength );
	if( cryptStatusOK( status ) )
		{
		status = iCryptCreateSignature( sigDataPtr, 
						min( sigDataMaxLength, MAX_INTLENGTH_SHORT - 1 ), 
						&sigLength, CRYPT_IFORMAT_SSH, 
						sessionInfoPtr->privateKey, createInfo.cryptHandle, 
						NULL );
		}
	if( cryptStatusOK( status ) )
		{
		/* Some buggy implementations require that RSA signatures be padded 
		   with zeroes to the full modulus size, mysteriously failing the 
		   authentication in a small number of randomly-distributed cases 
		   when the signature format happens to be less than the modulus 
		   size.  To handle this we have to rewrite the signature to include 
		   the extra padding bytes */
		if( TEST_FLAG( sessionInfoPtr->protocolFlags, 
					   SSH_PFLAG_RSASIGPAD ) && pkcAlgo == CRYPT_ALGO_RSA )
			{
			status = rewriteRSASignature( sessionInfoPtr, stream, sigLength, 
										  min( sigDataMaxLength, \
											   MAX_INTLENGTH_SHORT - 1 ) );
			}
		else
			status = sSkip( stream, sigLength, MAX_INTLENGTH_SHORT );
		}
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );

	return( status );
	}

/****************************************************************************
*																			*
*						Process Authentication Failures						*
*																			*
****************************************************************************/

/* Read the server's authentication failure response */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int readAuthFailureInfo( INOUT SESSION_INFO *sessionInfoPtr,
								IN_LENGTH_SHORT const int length,
								OUT_ALGO_Z CRYPT_ALGO_TYPE *authAlgo,
								OUT_BOOL BOOLEAN *furtherAuthRequired,
								const BOOLEAN usedPasswordAuth )
	{
	CRYPT_ALGO_TYPE requiredAuthAlgo;
	STREAM stream;
	BOOLEAN partialSuccess = FALSE;
	int status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( authAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( isWritePtr( furtherAuthRequired, sizeof( BOOLEAN ) ) );

	REQUIRES( isShortIntegerRangeNZ( length ) );
	REQUIRES( usedPasswordAuth == TRUE || usedPasswordAuth == FALSE );

	/* Clear return values */
	*authAlgo = CRYPT_ALGO_NONE;
	*furtherAuthRequired = FALSE;

	/* Before we can try and interpret the response, we have to check for an
	   empty response */
	if( length >= LENGTH_SIZE && \
		!memcmp( sessionInfoPtr->receiveBuffer,
				 "\x00\x00\x00\x00", LENGTH_SIZE ) )
		{
		/* It's an empty response.  There are two interpretations for this, 
		   the first of which is that some buggy implementations return an 
		   authentication failure with the available-authentication types 
		   string empty to indicate that no authentication is required 
		   rather than returning an authentication success status as 
		   required by the spec (although the problem is really in the spec 
		   and not in the interpretation since there's no way to say that 
		   "no-auth" is a valid authentication type).  If we find one of 
		   these then we return an OK_SPECIAL status to let the caller know 
		   that they should retry the authentication.  Because this is a 
		   broken packet format we have to check the encoded form rather 
		   than being able to read it as normal */
		if( TEST_FLAG( sessionInfoPtr->protocolFlags, 
					   SSH_PFLAG_EMPTYUSERAUTH ) )
			{
			/* It's a garbled attempt to tell us that no authentication is
			   required, tell the caller to try again without 
			   authentication */
			return( OK_SPECIAL );
			}

		/* A more logical interpretation is that no further authentication
		   type is permitted */
		retExt( CRYPT_ERROR_PERMISSION,
				( CRYPT_ERROR_PERMISSION, SESSION_ERRINFO, 
				  "Remote system denied our authentication attempt" ) );
		}

	/* The authentication failed, pick apart the response to see if we can
	   return more meaningful error information:

	  [	byte	type = SSH_MSG_USERAUTH_FAILURE ]
		string	available_auth_types
		boolean	partial_success

	  The available-auth types field is handled in the typical SSH 
	  schizophrenic manner in which, instead of saying "authentication 
	  failed", it returns a list of allowed authentication methods, one of 
	  which may be the one that we just used, see the comment in 
	  reportAuthFailure() for details.  In order to deal with this we use
	  two different mapping tables to read the available-auth types field,
	  one that prefers passwords if we used password auth and one that 
	  prefers public keys if we used public-key auth.  In other words if we
	  used password auth then we assume that the failure was due to an
	  incorrect password and prefer a password-auth error report, and the
	  same for public keys.
	  
	  However this doesn't help in the case where the server asks for
	  something like { publickey, password } and reports partial success
	  with the required auth being { publickey, password }, because it
	  could be after two passwords or a public key and password or whatever.  
	  In order to deal with this, we have to re-read the available-auth 
	  types field using the opposite preference, and if we get a different
	  result then report an ambiguous error to the caller, in other words
	  that the server has indicated that something is needed to continue but
	  it won't specify what.

	  The partial_success flag is a kitchen-sink kludge used to deal with
	  situation-specific events like changed passwords and similar issues, 
	  in this case it means that the authentication was successful despite 
	  the overall packet type indicating that the authentication failed, and 
	  that something else is needed.  In general this whole section of the 
	  protocol winds up in an extremely complex state machine with all sorts 
	  of special-case conditions, several of which require manual 
	  intervention by the user in order to continue, however we can handle
	  the specific case of "authentication failed but it really succeeded"
	  by letting the user know that further authentication is required on 
	  top of the already-performed authentication */
	ENSURES( rangeCheck( length, 1, sessionInfoPtr->receiveBufSize ) );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	if( usedPasswordAuth )
		{
		status = readAlgoString( &stream, algoStringUserauthentPWTbl,
								 FAILSAFE_ARRAYSIZE( algoStringUserauthentPWTbl, \
													 ALGO_STRING_INFO ),
								 &requiredAuthAlgo, FALSE, SESSION_ERRINFO );
		}
	else
		{
		status = readAlgoString( &stream, algoStringUserauthentPKCTbl,
								 FAILSAFE_ARRAYSIZE( algoStringUserauthentPKCTbl, \
													 ALGO_STRING_INFO ),
								 &requiredAuthAlgo, FALSE, SESSION_ERRINFO );
		}
	if( cryptStatusOK( status ) )
		{
		int value;

		/* Read the partial-success value to see whether we need to change
		   how we're dealing with the problem */
		status = value = sgetc( &stream );
		if( !cryptStatusError( status ) && value )
			{
			partialSuccess = TRUE;
			status = CRYPT_OK;	/* sgetc() returns boolean flag */
			}
		}
	if( cryptStatusOK( status ) && partialSuccess )
		{
		CRYPT_ALGO_TYPE altRequiredAuthAlgo;

		/* We've been given possibly garbled available-auth type information, 
		   re-read the data with the preference reversed (see the comment at 
		   the start of this code block).  If we now get a different 
		   required algorithm then we can't really tell the user what to do 
		   in order to continue because the server's indication of what's 
		   required is ambiguous */
		sseek( &stream, 0 );
		if( usedPasswordAuth )
			{
			status = readAlgoString( &stream, algoStringUserauthentPKCTbl,
									 FAILSAFE_ARRAYSIZE( algoStringUserauthentPKCTbl, \
														 ALGO_STRING_INFO ),
									 &altRequiredAuthAlgo, FALSE, SESSION_ERRINFO );
			}
		else
			{
			status = readAlgoString( &stream, algoStringUserauthentPWTbl,
									 FAILSAFE_ARRAYSIZE( algoStringUserauthentPWTbl, \
														 ALGO_STRING_INFO ),
									 &altRequiredAuthAlgo, FALSE, SESSION_ERRINFO );
			}
		if( requiredAuthAlgo != altRequiredAuthAlgo )
			requiredAuthAlgo = CRYPT_PSEUDOALGO_AMBIGUOUS;
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

		/* There was some other problem with the returned information, we
		   still report it as a failed-authentication error but leave the
		   extended error information in place to let the caller see what 
		   the underlying cause was */
		return( CRYPT_ERROR_WRONGKEY );
		}

	*authAlgo = requiredAuthAlgo;
	*furtherAuthRequired = partialSuccess;
	return( CRYPT_OK );
	}

/* Process an authentication failure.  SSH reports authentication failures 
   in a somewhat bizarre way, instead of saying "authentication failed" it 
   returns a list of allowed authentication methods, one of which may 
   be the one that we just used, and one of which may be the one that we
   need to continue (it's not guaranteed, see the comments in 
   readAuthFailureInfo() and other locations).  To figure out whether a 
   problem occurred because we used the wrong authentication method or 
   because we submitted the wrong authentication value we have to perform a 
   complex decode and match of the information in the returned packet with 
   what we sent.  The breakdown is:

	  Tried	 Partial Required	Result
	  Auth.	 Success   Auth.
	--------+-------+-------+------------
			|	N	|  Sig	| Wrong sig.
			|	N	|  PW	| Need PW instead of sig.
	  Sig	+-------+-------+
			|	Y	|  Sig	| Sig OK, need more sig.
			|	Y	|  PW	| Sig OK, need PW.
	--------+-------+-------+
			|	N	|  Sig	| Need sig. instead of PW.
			|	N	|  PW	| Wrong PW.
	   PW	+-------+-------+
			|	Y	|  Sig	| PW OK, need sig.
			|	Y	|  PW	| PW OK, need more PW.
	--------+-------+-------+ */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int reportAuthFailure( INOUT SESSION_INFO *sessionInfoPtr,
							  IN_ALGO const CRYPT_ALGO_TYPE providedAuthAlgo,
							  IN_ALGO const CRYPT_ALGO_TYPE requiredAuthAlgo,
							  const BOOLEAN partialAuthOK )
	{
	const BOOLEAN needsPW = \
			( requiredAuthAlgo == CRYPT_PSEUDOALGO_PASSWORD || \
			  requiredAuthAlgo == CRYPT_PSEUDOALGO_PAM ) ? TRUE : FALSE;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( isEnumRange( providedAuthAlgo, CRYPT_ALGO ) );
	REQUIRES( isEnumRange( requiredAuthAlgo, CRYPT_ALGO ) );
	REQUIRES( partialAuthOK == TRUE || partialAuthOK == FALSE );

	/* Deal with SSH's braindamaged ability to report a totally ambiguous 
	   failure that could be any of "invalid password", "invalid public 
	   key", "valid password but public key required as well", "valid 
	   public key but password required as well", "valid password but 
	   further password required", and so on */
	if( requiredAuthAlgo == CRYPT_PSEUDOALGO_AMBIGUOUS )
		{
		if( isSigAlgo( providedAuthAlgo ) )
			{
			retExt( CRYPT_ERROR_NOTINITED,
					( CRYPT_ERROR_NOTINITED, SESSION_ERRINFO, 
					  "Authenticated with public/private key, server "
					  "reports that further public/private key and/or "
					  "password authentication is required" ) );
			}
		else
			{
			retExt( CRYPT_ERROR_NOTINITED,
					( CRYPT_ERROR_NOTINITED, SESSION_ERRINFO, 
					  "Authenticated with password, server reports that "
					  "further password and/or public/private key "
					  "authentication is required" ) );
			}
		}

	/* If the initial part of the authentication succeeded, report what
	   succeeded and what's needed to continue */
	if( partialAuthOK )
		{
		retExt( CRYPT_ERROR_NOTINITED,
				( CRYPT_ERROR_NOTINITED, SESSION_ERRINFO, 
				  "Authenticated with %s, server reports that further %s "
				  "authentication is required", 
				  isSigAlgo( providedAuthAlgo ) ? "public/private key" : \
												  "password",
				  needsPW ? "password" : "public/private key" ) );
			}

	/* Break down the authentication response as per the table above to try 
	   and return some sort of useful information to the caller */
	if( isSigAlgo( providedAuthAlgo ) )
		{
		if( needsPW )
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
	if( isSigAlgo( requiredAuthAlgo ) )
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processAuthFailure( INOUT SESSION_INFO *sessionInfoPtr,
							   IN_LENGTH_SHORT const int length,
							   IN_ALGO const CRYPT_ALGO_TYPE providedAuthAlgo,
							   const BOOLEAN partialAuthOK )
	{
	CRYPT_ALGO_TYPE requiredAuthAlgo;
	BOOLEAN needFurtherAuth = FALSE;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( isShortIntegerRangeNZ( length ) );
	REQUIRES( isEnumRange( providedAuthAlgo, CRYPT_ALGO ) );
	REQUIRES( partialAuthOK == TRUE || partialAuthOK == FALSE );

	status = readAuthFailureInfo( sessionInfoPtr, length, &requiredAuthAlgo, 
								  &needFurtherAuth, TRUE );
	if( cryptStatusError( status ) )
		return( ( status == OK_SPECIAL ) ? CRYPT_ERROR_WRONGKEY : status );
	return( reportAuthFailure( sessionInfoPtr, providedAuthAlgo, 
							   requiredAuthAlgo, partialAuthOK ) );
	}

/****************************************************************************
*																			*
*						Send/Receive Authentication Messages				*
*																			*
****************************************************************************/

/* Send an authentication request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int sendAuthRequest( INOUT SESSION_INFO *sessionInfoPtr,
							INOUT SSH_HANDSHAKE_INFO *handshakeInfo,
							const ATTRIBUTE_LIST *userNamePtr,
							const BOOLEAN preferPublickeyAuth )
	{
	const ATTRIBUTE_LIST *passwordPtr = \
			findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_PASSWORD );
	const BOOLEAN usePasswordAuth = \
			( passwordPtr != NULL && !preferPublickeyAuth ) ? TRUE : FALSE;
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );
	assert( isReadPtr( userNamePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( preferPublickeyAuth == TRUE || preferPublickeyAuth == FALSE );

	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_USERAUTH_REQUEST );
	if( cryptStatusError( status ) )
		return( status );
	if( usePasswordAuth )
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

	/* Send the authentication information to the server.  If we're sending 
	   a password, we enable the use of quantised padding, which also means 
	   that we have to use distinct wrap + send calls */
	if( usePasswordAuth )
		{
		status = wrapPacketSSH2( sessionInfoPtr, &stream, 0, TRUE );
		if( cryptStatusOK( status ) )
			status = sendPacketSSH2( sessionInfoPtr, &stream );
		}
	else
		status = wrapSendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );
	
	return( status );
	}

/* Read the authentication response from the server */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readAuthResponse( INOUT SESSION_INFO *sessionInfoPtr,
							 OUT_RANGE( 0, SSH_MSG_SPECIAL_LAST ) int *type,
							 OUT_LENGTH_Z int *packetLength,
							 const BOOLEAN isPAMAuth )
	{
	STREAM stream;
	const int authType = isPAMAuth ? SSH_MSG_SPECIAL_USERAUTH_PAM : \
									 SSH_MSG_SPECIAL_USERAUTH;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( type, sizeof( int ) ) );
	assert( isWritePtr( packetLength, sizeof( int ) ) );

	REQUIRES( isPAMAuth == TRUE || isPAMAuth == FALSE );

	/* Clear return values */
	*type = 0;
	*packetLength = 0;

	/* Read the server's response to the authentication request */
	status = length = readAuthPacketSSH2( sessionInfoPtr, authType, ID_SIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* If the server sent extension information, process it and then retry 
	   the server response read */
	if( sessionInfoPtr->sessionSSH->packetType == SSH_MSG_EXT_INFO )
		{
		sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
		status = readExtensionsSSH( sessionInfoPtr, &stream );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );

		/* Retry the authentication response read */
		status = length = readAuthPacketSSH2( sessionInfoPtr, authType, 
											  ID_SIZE );
		if( cryptStatusError( status ) )
			return( status );
		}
	*packetLength = length;
	*type = sessionInfoPtr->sessionSSH->packetType;
	if( sessionInfoPtr->sessionSSH->packetType == SSH_MSG_USERAUTH_SUCCESS )
		{
		/* We've successfully authenticated ourselves and we're done */
		return( CRYPT_OK );
		}

	/* The authentication failed, let the caller know that they need to 
	   process further auth-failure information */
	return( OK_SPECIAL );
	}

/* Send a dummy authentication request, needed under various circumstances 
   for some buggy servers */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sendDummyAuth( INOUT SESSION_INFO *sessionInfoPtr,
						  IN_BUFFER( userNameLength ) const char *userName,
						  IN_LENGTH_TEXT const int userNameLength )
	{
	STREAM stream;
	int type, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtrDynamic( userName, userNameLength ) );

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
		status = wrapSendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Wait for the server's ack of the authentication.  In theory since 
	   this is just something used to de-confuse the buggy servers we can 
	   ignore the content (as long as the packet is valid) as any 
	   authentication problems will be resolved by the real authentication 
	   process below, but in some rare cases we can encounter oddball 
	   devices that don't perform any authentication at the SSH level but 
	   instead ask for a password via the protocol being tunneled over SSH, 
	   which means that we'll get a USERAUTH_SUCCESS at this point even if 
	   we haven't actually authenticated ourselves */
	status = readAuthResponse( sessionInfoPtr, &type, &length, FALSE );
	if( cryptStatusOK( status ) )
		{
		/* We've successfully (non-)authenticated ourselves and we're done */
		return( OK_SPECIAL );
		}
	else
		{
		/* There was an error authenticating, exit */
		if( status != OK_SPECIAL )
			return( status );
		}

	/* The authentication failed, in this case since what we've sent is a 
	   dummy auth to de-confuse buggy servers we expect it to fail, in which
	   case we return CRYPT_OK to tell the caller that they can continue with
	   the actual authentication */
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
				findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_PASSWORD );
	STREAM stream;
	BYTE nameBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BYTE promptBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	int nameLength, promptLength = -1, noPrompts = -1;
	int i, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtrDynamic( pamRequestData, pamRequestDataLength ) );

	REQUIRES( isShortIntegerRangeNZ( pamRequestDataLength ) );

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
	   succeeds then the server is supposed to send back a standard user-
	   auth success or failure status but could also send another
	   SSH_MSG_USERAUTH_INFO_REQUEST even if it contains no payload (an 
	   OpenSSH bug) so we have to handle this as a special case */
	sMemConnect( &stream, pamRequestData, pamRequestDataLength );
	status = readString32Opt( &stream, nameBuffer, CRYPT_MAX_TEXTSIZE,
							  &nameLength );	/* Name */
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
		{
		status = readString32( &stream, promptBuffer, 
							   CRYPT_MAX_TEXTSIZE, &promptLength );
		if( cryptStatusOK( status ) && promptLength <= 0 )
			{
			/* We must have at least some sort of prompt given that we 
			   require num_prompts to be nonzero */
			status = CRYPT_ERROR_BADDATA;
			}
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Invalid PAM authentication request packet" ) );
		}
	REQUIRES( nameLength >= 0 && nameLength <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( promptLength >= 1 && promptLength <= CRYPT_MAX_TEXTSIZE );

	/* Make sure that we're being asked for some form of password 
	   authentication.  This assumes that the prompt string begins with the 
	   word "password" (which always seems to be the case), if it isn't then 
	   it may be necessary to do a substring search */
	if( promptLength < 8 || \
		!strIsPrintable( promptBuffer, promptLength ) || \
		strCompare( promptBuffer, "Password", 8 ) )
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
			string	response[ n ]

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
	LOOP_MED( i = 0, cryptStatusOK( status ) && i < noPrompts, i++ )
		{
		status = writeString32( &stream, passwordPtr->value,
								passwordPtr->valueLength );
		}
	ENSURES( LOOP_BOUND_OK );
	if( cryptStatusOK( status ) )
		status = wrapSendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );

	return( status );
	}

/* Handle PAM authentication */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processPamAuthentication( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const ATTRIBUTE_LIST *userNamePtr = \
				findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_USERNAME );
	STREAM stream;
	int length, pamIteration, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( userNamePtr, sizeof( ATTRIBUTE_LIST ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );
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
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSH_PFLAG_PAMPW ) )
		{
		/* Some servers choke if we supply a sub-method hint for the
		   authentication */
		status = writeUint32( &stream, 0 );
		}
	else
		status = writeString32( &stream, "password", 8 );
	if( cryptStatusOK( status ) )
		status = wrapSendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Handle the PAM negotiation.  This can (in theory) go on indefinitely,
	   to avoid potential DoS problems we limit it to five iterations.  In
	   general we'll only need two iterations (or three for servers with the 
	   empty-message bug) so we shouldn't ever get to five */
	LOOP_SMALL( pamIteration = 0, pamIteration < 5, pamIteration++ )
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
		status = readAuthResponse( sessionInfoPtr, &type, &length, TRUE );
		if( cryptStatusOK( status ) )
			{
			/* We've successfully authenticated ourselves and we're done */
			return( CRYPT_OK );
			}
		else
			{
			/* There was an error authenticating, exit */
			if( status != OK_SPECIAL )
				return( status );
			}

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

				REQUIRES( rangeCheck( userNamePtr->valueLength, 1, 
									  CRYPT_MAX_TEXTSIZE ) );
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
			return( processAuthFailure( sessionInfoPtr, length, 
										CRYPT_PSEUDOALGO_PASSWORD, FALSE ) );
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
	ENSURES( LOOP_BOUND_OK );

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
	CRYPT_ALGO_TYPE requiredAuthAlgo;
	const ATTRIBUTE_LIST *userNamePtr = \
				findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_USERNAME );
	const BOOLEAN hasPassword = \
				( findSessionInfo( sessionInfoPtr,
								   CRYPT_SESSINFO_PASSWORD ) != NULL ) ? \
				TRUE : FALSE;
	BOOLEAN needFurtherAuth = FALSE;
	int type, length DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );
	REQUIRES( userNamePtr != NULL );

	/* Some buggy servers require a dummy request for authentication methods 
	   otherwise they'll reject any method other than 'password' as invalid 
	   with the error "Client requested non-existing method 'publickey'".  
	   To work around this we submit a dummy authentication request using the 
	   method 'none' */
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, 
				   SSH_PFLAG_DUMMYUSERAUTH ) )
		{
		status = sendDummyAuth( sessionInfoPtr, userNamePtr->value, 
								userNamePtr->valueLength );
		if( cryptStatusError( status ) )
			{
			/* Under some special circumstances involving other server bugs 
			   (see the comment in sendDummyAuth() for details) this dummy 
			   authentication can get us in, in which case we're done */
			if( status == OK_SPECIAL )
				return( CRYPT_OK );

			return( status );
			}
		}

	/* The way in which we handle authentication here isn't totally
	   appropriate since we assume that the user knows the appropriate form
	   of authentication to use.  If they're ambiguous and supply both a
	   password and a private key and the server only accepts PKC-based
	   authentication then we'll always preferentially choose password-based
	   authentication.  The way around this is to send an authentication 
	   request with a method-type of "none" to see what the server wants but 
	   the only thing that cryptlib can do in this case (since it's non-
	   interactive during the handshake phase) is disconnect, tell the user 
	   what went wrong, and try again.  The current mechanism does this 
	   anyway so we don't gain much except extra RTT delays by adding this 
	   question-and-answer facility */
	status = sendAuthRequest( sessionInfoPtr, handshakeInfo, userNamePtr, 
							  FALSE );
	if( cryptStatusOK( status ) )
		status = readAuthResponse( sessionInfoPtr, &type, &length, FALSE );
	if( cryptStatusOK( status ) )
		{
		/* We've successfully authenticated ourselves and we're done */
		return( CRYPT_OK );
		}
	else
		{
		/* There was an error authenticating, exit */
		if( status != OK_SPECIAL )
			return( status );
		}

	/* Read the details of the authentication failure.  We decode the 
	   response to favour password- or PKC-based authentication depending 
	   on whether the user specified a password or a PKC as their 
	   authentication choice */
	status = readAuthFailureInfo( sessionInfoPtr, length, &requiredAuthAlgo, 
								  &needFurtherAuth, hasPassword );
	if( cryptStatusError( status ) )
		{
		if( status != OK_SPECIAL )
			return( status );

		/* Some buggy implementations return an authentication failure as a 
		   garbled attempt to tell us that no authentication is required, if 
		   we encounter one of these (indicated by readAuthFailureInfo() 
		   returning an OK_SPECIAL status) then we retry by sending a dummy 
		   authentication request, which should get us in.

		   The handling of return codes at this point is somewhat different 
		   from normal because we're already in a failure state so even a 
		   successful return (indicating that the dummy-auth was successfully 
		   sent) has to be converted into an overall failure status */
		status = sendDummyAuth( sessionInfoPtr, userNamePtr->value, 
								userNamePtr->valueLength );
		if( status == OK_SPECIAL )
			{
			/* If we got in with the dummy authenticate, we're done */
			return( CRYPT_OK );
			}
		return( cryptStatusOK( status ) ? CRYPT_ERROR_WRONGKEY : status );
		}

	/* If we tried straight password authentication and the server requested 
	   keyboard-interactive (== misnamed PAM) authentication try again using 
	   PAM authentication */
	if( hasPassword && requiredAuthAlgo == CRYPT_PSEUDOALGO_PAM )
		return( processPamAuthentication( sessionInfoPtr ) );

	/* If this is a final authentication failure, we're done */
	if( !needFurtherAuth )
		{
		return( reportAuthFailure( sessionInfoPtr, 
								   hasPassword ? CRYPT_PSEUDOALGO_PASSWORD : \
												 CRYPT_ALGO_RSA, 
								   requiredAuthAlgo, FALSE ) );
		}

	/* The authentication didn't actually fail, it succeeded but more 
	   authentication is required.  If we authenticated with a public key 
	   (meaning no password was available, since we preferentially 
	   authenticate with that), report the problem as public key used, other 
	   auth required */
	if( !hasPassword )
		{
		return( reportAuthFailure( sessionInfoPtr, CRYPT_ALGO_RSA, 
								   requiredAuthAlgo, TRUE ) );
		}

	/* We authenticated with a password, if there's no public key available 
	   or what's required isn't a public key, report the problem as password 
	   used, something else required */
	if( sessionInfoPtr->privateKey == CRYPT_ERROR || \
		!( isSigAlgo( requiredAuthAlgo ) || \
		   requiredAuthAlgo == CRYPT_PSEUDOALGO_AMBIGUOUS ) )
		{
		return( reportAuthFailure( sessionInfoPtr, CRYPT_PSEUDOALGO_PASSWORD,
								   requiredAuthAlgo, TRUE ) );
		}

	/* We authenticated with a password and additional authentication with a
	   public key is required, perform the additional authentication */
	status = sendAuthRequest( sessionInfoPtr, handshakeInfo, userNamePtr, 
							  TRUE );
	if( cryptStatusOK( status ) )
		status = readAuthResponse( sessionInfoPtr, &type, &length, FALSE );
	if( cryptStatusOK( status ) )
		{
		/* We've successfully authenticated ourselves and we're done */
		return( CRYPT_OK );
		}
	else
		{
		/* There was an error authenticating, exit */
		if( status != OK_SPECIAL )
			return( status );
		}

	/* The additional authentication failed, provide more specific details 
	   for the caller */
	return( processAuthFailure( sessionInfoPtr, length, CRYPT_ALGO_RSA, 
								TRUE ) );
	}
#endif /* USE_SSH */
