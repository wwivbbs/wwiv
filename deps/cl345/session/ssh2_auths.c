/****************************************************************************
*																			*
*			cryptlib SSHv2 Server-side Authentication Management			*
*						Copyright Peter Gutmann 1998-2016					*
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

#ifdef USE_SSH

/* SSH user authentication gets quite complicated because of the way that 
   the multi-pass process defined in the spec affects our handling of user 
   name and password information.  The SSH spec allows the client to perform 
   authentication in bits and pieces and change the details of what it sends 
   at each step (and use authentication methods that it specifically knows 
   the server can't handle and all manner of other craziness, see the 
   SimpleSSH draft for a long discussion of these problems).  This is 
   particularly nasty because of the large amount of leeway that it provides 
   for malicious clients to subvert the authentication process, for example 
   the client can supply a privileged user name the first time around and 
   then authenticate the second time round as an unprivileged user.  If the 
   calling application just checks the first user name that it finds it'll 
   then treat the client as being an authenticated privileged user (which
   indeed some server applications have done in the past).
   
   To defend against this we record the user name the first time that it's 
   entered and from then on require that the client supply the same name on 
   subsequent authentication attempts.  This is the standard client 
   behaviour anyway, if the user name + password are rejected then the 
   assumption is that the password is wrong and the user gets to retry the 
   password.  In order to accommodate public-key authentication we also 
   verify that the authentication method remains constant over successive 
   iterations, i.e. that the client doesn't try part of an authentication 
   with a public key and then another part with a password.

   Handling the state machine required to process this gets a bit 
   complicated, the protocol flow that we enforce is:

	Step 0 (optional):

		Client sends SSH_MSG_USERAUTH_REQUEST with method "none" to query 
		available authentication method types.

		Server responds with SSH_MSG_USERAUTH_FAILURE listing available 
		methods.

	Step 0a (optional):

		Client sends SSH_MSG_USERAUTH_REQUEST with only a public key, no
		signature.

		Server responds with SSH_MSG_USERAUTH_PK_OK.

	Step 1:

		Client sends SSH_MSG_USERAUTH_REQUEST with method "password" or 
		"publickey" and password data or a digital signature as appropriate.

	Step 2, one of:

		a. Server responds with SSH_MSG_USERAUTH_SUCCESS and the 
		authentication exchange terminates.

		b. Server responds to method "password" with 
		SSH_MSG_USERAUTH_FAILURE, the client may retry step 1 if permitted 
		by the server as described in the SSH specification.

		c. Server responds to method "publickey" with 
		SSH_MSG_USERAUTH_FAILURE and the authentication exchange terminates.

   This is a simplified form of what's given in the SSH spec, which allows
   almost any jumble of messages including ones that don't make any sense 
   (again, see the SimpleSSH draft for more details).  The credential-type 
   matching that we perform in processUserAuth() is indicated by the caller 
   supplying one of the following values:

	NONE: No existing user name or password to match against, store the 
		client's user name and password for the caller to check.

	USERNAME: Existing user name present from a previous iteration of
		authentication, match the client's user name to the existing one and 
		store the client's password for the caller to check.

	USERNAME_PASSWORD: Caller-supplied credentials present, match the
		client's credentials against them */

typedef enum {
	CREDENTIAL_NONE,	/* No credential information type */
	CREDENTIAL_USERNAME,/* User name is present and must match */
	CREDENTIAL_USERNAME_PASSWORD,/* User/password present and must match */
	CREDENTIAL_USERNAME_PUBKEY,/* User/pubkey present and must match */
	CREDENTIAL_LAST		/* Last possible credential information type */
	} CREDENTIAL_TYPE;

/* The processUserAuth() function has multiple possible return values, 
   broken down into CRYPT_OK for a password match, OK_SPECIAL for something 
   that the caller has to handle, and the standard error status, with 
   subvalues given in the userAuthInfo variable.  The different types and
   subtypes are:

	CRYPT_OK + uAI = USERAUTH_SUCCESS: User credentials present and matched.  
		Note that the caller has to check both of these values, one a return 
		status and the other a by-reference parameter, to avoid a single 
		point of failure for the authentication.

	OK_SPECIAL + uAI = USERAUTH_CALLERCHECK: User credentials not present, 
		added to the session attributes for the caller to check.

	OK_SPECIAL + uAI = USERAUTH_NOOP: No-op read for a client query of 
		available authentication methods via the pseudo-method "none".

	OK_SPECIAL + uAI = USERAUTH_NOOP_2: Additional no-op read for a client 
		partial pubkey auth without a signature, requiring another round
		of messages to get a pubkey auth with a signature.

	Error + uAI = USERAUTH_ERROR: Standard error status, which includes non-
		matched credentials */

typedef enum {
	USERAUTH_NONE,		/* No authentication type */
	USERAUTH_SUCCESS,	/* User authenticated successfully */
	USERAUTH_CALLERCHECK,/* Caller must check whether auth.was successful */
	USERAUTH_NOOP,		/* No-op read */
	USERAUTH_NOOP_2,	/* Pubkey auth no-op */
	USERAUTH_ERROR,		/* User failed authentication */
	USERAUTH_LAST		/* Last possible authentication type */
	} USERAUTH_TYPE;

/* The match options with a caller-supplied list of credentials to match 
   against are (this doesn't apply to public-key auth because that's always
   implicitly present via the public-key database):

	Client sends | Cred.type	| Action					| Result
	-------------+--------------+---------------------------+--------------
	Name, pw	 | USERNAME_PW	| Match name, password		| SUCCESS/ERROR
	-------------+--------------+---------------------------+--------------
	Name, none	 | USERNAME_PW	| Match name				| NOOP
	Name, none	 | USERNAME_PW	| Match name				| ERROR (fatal)
	-------------+--------------+---------------------------+--------------
	Name, none	 | USERNAME_PW	| Match name				| NOOP
	Name, pw	 | USERNAME_PW	| Match name, password		| SUCCESS/ERROR
	-------------+--------------+---------------------------+--------------
	Name, none	 | USERNAME_PW	| Match name				| NOOP
	Name2, pw	 | USERNAME_PW	| Match name2 -> Fail		| ERROR (fatal)
	-------------+--------------+---------------------------+--------------
	Name, wrongPW| USERNAME_PW	| Match name, wrongPW		|
				 |				|	-> Fail					| ERROR
	Name, pw	 | USERNAME_PW	| Match name, password		| SUCCESS/ERROR

  If an error status is returned then any value other than 
  CRYPT_ERROR_WRONGKEY is treated as a fatal error.  CRYPT_ERROR_WRONGKEY
  is nonfatal as determined by the caller, for example until some predefined
  retry count has been exceeded.

  Handling of the third case (two different user names supplied in different
  messages) gets a bit tricky because if both names are present in the list
  of credentials then we'll get a successful match both times even though a 
  different user name was used.  To detect this we record the initial user 
  name in the SSH session information and check it against subsequently 
  supplied values.

  The match options with no caller-supplied list of credentials to match 
  against are:

	Client sends | Cred.type	| Action					| Result
	-------------+--------------+---------------------------+--------------
	Name, pw	 | NONE			| Add name, password		| CALLERCHECK
	-------------+--------------+---------------------------+--------------
	Name, none	 | USERNAME_PW	| Match name				| NOOP
	Name, none	 | USERNAME_PW	| Match name				| ERROR (fatal)
	-------------+--------------+---------------------------+--------------
	Name, none	 | NONE			| Add name					| NOOP
	Name, pw	 | USERNAME		| Match name, add password	| CALLERCHECK
	-------------+--------------+---------------------------+--------------
	Name, none	 | NONE			| Add name					| NOOP
	Name2, pw	 | USERNAME		| Match name2 -> Fail		| ERROR (fatal)
	-------------+--------------+---------------------------+--------------
	Name, wrongPW| NONE			| Add name, wrongPW			| CALLERCHECK
	Name, pw	 | USERNAME		| Match name, add password	| CALLERCHECK
	-------------+--------------+---------------------------+--------------
	Name, pkauth | NONE			| Add name					| SUCCESS/ERROR
				 |				| Check pkauth				|
	-------------+--------------+---------------------------+--------------
	Name, pubkey | NONE			| Add name					| NOOP_2
	Name, pkauth | USERNAME_PUBK| Match name				| SUCCESS/ERROR
				 |				| Check pkauth				|
	-------------+--------------+---------------------------+--------------
	Name, pubkey | NONE			| Add name					| NOOP_2
	Name, pubkey | USERNAME_PUBK| Match name				| ERROR (fatal)
	-------------+--------------+---------------------------+-------------- */

/* A lookup table for the authentication methods submitted by the client */

typedef enum {
	METHOD_NONE,		/* No authentication method type */
	METHOD_QUERY,		/* Query available authentication methods */
	METHOD_PASSWORD,	/* Password authentication */
	METHOD_PUBKEY,		/* Pubkey authentication */
	METHOD_LAST			/* Last possible authentication method type */
	} METHOD_TYPE;

typedef struct {
	BUFFER_FIXED( nameLength ) \
	const char *name;			/* Method name */
	const int nameLength;
	const METHOD_TYPE type;		/* Method type */
	} METHOD_INFO;

static const METHOD_INFO methodInfoTbl[] = {
	{ "none", 4, METHOD_QUERY },
	{ "password", 8, METHOD_PASSWORD },
	{ NULL, 0, METHOD_NONE }, { NULL, 0, METHOD_NONE }
	};
static const METHOD_INFO methodInfoTblPubkeyAuth[] = {
	{ "none", 4, METHOD_QUERY },
	{ "password", 8, METHOD_PASSWORD },
	{ "publickey", 9, METHOD_PUBKEY },
	{ NULL, 0, METHOD_NONE }, { NULL, 0, METHOD_NONE }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Send a succeeded/failed-authentication response to the client:

	byte	type = SSH_MSG_USERAUTH_SUCCESS

   or

	byte	type = SSH_MSG_USERAUTH_FAILURE
	string	allowed_authent = empty
	boolean	partial_success = FALSE 

   or

	byte	type = SSH_MSG_USERAUTH_FAILURE
	string	allowed_authent = "publickey" / "password"
	boolean	partial_success = FALSE

   The latter two variants are necessary because the failure response is
   overloaded to perform two functions, firstly to indicate that the 
   authentication failed and secondly to provide a list of methods that can
   be used to authenticate (see the comment at the start of this module for
   the calisthentics that are required to support this) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendResponseSuccess( INOUT SESSION_INFO *sessionInfoPtr )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	
	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_USERAUTH_SUCCESS );
	if( cryptStatusError( status ) )
		return( status );
	status = wrapSendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendResponseFailure( INOUT SESSION_INFO *sessionInfoPtr )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Straight failure response */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_USERAUTH_FAILURE );
	if( cryptStatusError( status ) )
		return( status );
	writeUint32( &stream, 0 );
	status = sputc( &stream, 0 );
	if( cryptStatusOK( status ) )
		status = wrapSendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendResponseFailureInfo( INOUT SESSION_INFO *sessionInfoPtr,
									const BOOLEAN allowPubkeyAuth )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( allowPubkeyAuth == TRUE || allowPubkeyAuth == FALSE );

	/* Failure response but really a means of telling the client how they 
	   can authenticate.  What to send as the allowed method is a bit 
	   tricky, if the caller tells us that publickey auth is allowed then 
	   we can always send that, but it's not clear whether we can say that
	   password auth is OK or not.  In theory we could check whether a 
	   password is present in the attribute list, but the caller could be 
	   performing on-demand checking without explicitly setting a password 
	   attribute, so there's no way to tell whether passwords should be 
	   allowed or not.  Because of this we always advertise password auth as 
	   being available */
	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_USERAUTH_FAILURE );
	if( cryptStatusError( status ) )
		return( status );
	if( allowPubkeyAuth )
		writeString32( &stream, "publickey,password", 18 );
	else
		writeString32( &stream, "password", 8 );
	status = sputc( &stream, 0 );
	if( cryptStatusOK( status ) )
		status = wrapSendPacketSSH2( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );

	return( status );
	}

/* Check that the query form and credentials submitted by the client are 
   consistent with any information submitted during earlier rounds of 
   authentication */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkQueryValidity( INOUT SSH_INFO *sshInfo,
							   IN_BUFFER( userNameLength ) const void *userName,
							   IN_LENGTH_TEXT const int userNameLength,
							   IN_ENUM( METHOD ) const METHOD_TYPE authMethod,
							   const BOOLEAN isInitialAuth )
	{
	HASH_FUNCTION_ATOMIC hashFunctionAtomic;
	BYTE userNameHash[ KEYID_SIZE + 8 ];

	assert( isWritePtr( sshInfo, sizeof( SSH_INFO ) ) );
	assert( isReadPtrDynamic( userName, userNameLength ) );

	REQUIRES( userNameLength >= 1 && userNameLength <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( isEnumRange( authMethod, METHOD ) );
	REQUIRES( isInitialAuth == TRUE || isInitialAuth == FALSE );

	/* Hash the user name so that we only need to store the fixed-length 
	   hash rather than the variable-length user name */
	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, NULL );
	hashFunctionAtomic( userNameHash, KEYID_SIZE, userName, userNameLength );

	/* If this is the first message remember the user name and 
	   authentication method in case we need to check it on a subsequent 
	   round of authentication */
	if( isInitialAuth )
		{
		memcpy( sshInfo->authUserNameHash, userNameHash, KEYID_SIZE );
		sshInfo->authType = authMethod;
		return( CRYPT_OK );
		}

	/* If the client is switching credentials across authentication messages 
	   then there's something fishy going on */
	if( !compareDataConstTime( sshInfo->authUserNameHash, userNameHash, 
							   KEYID_SIZE ) )
		return( CRYPT_ERROR_INVALID );

	/* Make sure that the authentication messages follow the state 
	   transitions:

		<empty> -> query -> auth_method
		<empty> ----------> auth_method

	   There are two error cases that can occur here, either the client 
	   sends multiple authentication requests with the pseudo-method "none"
	   (SSH's way of performing a method query) or they send a request with
	   a different method than a previous one, for example "password" in the
	   initial request and then "publickey" in a following one */
	if( sshInfo->authType == METHOD_QUERY )
		{
		/* If we've already processed a query message then any subsequent 
		   message has to be an actual authentication message */
		if( authMethod == METHOD_QUERY )
			return( CRYPT_ERROR_DUPLICATE );

		/* We've had a query followed by a standard authentication method, 
		   remember the authentication method for later */
		sshInfo->authType = authMethod;
		return( CRYPT_OK );
		}

	/* If we've already seen a standard authentication method then the new 
	   method must be the same */
	if( sshInfo->authType != authMethod )
		return( CRYPT_ERROR_INVALID );

	return( CRYPT_OK );
	}

/* Read the public key value that'll be used for pubkey authentication and 
   verify the authentication data created with it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPublicKey( INOUT SESSION_INFO *sessionInfoPtr,
						  INOUT STREAM *stream )
	{
	CRYPT_ALGO_TYPE pubkeyAlgo;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	void *keyPtr DUMMY_INIT_PTR;
	int keyLength, dummy, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Skip the first of the three copies of the algorithm name (see the 
	   comment in ssh2_cli.c for more on this).  We don't do anything with 
	   it because we're about to get two more copies of the same thing, and 
	   the key and signature information take precedence over anything that 
	   we find here */
	status = readUniversal32( stream );
	if( cryptStatusError( status ) )
		return( status );
	
	/* Read the client's public key */	
	streamBookmarkSet( stream, keyLength );
	status = checkReadPublicKey( stream, &pubkeyAlgo, &dummy, 
								 SESSION_ERRINFO );
	if( cryptStatusOK( status ) )
		{
		status = streamBookmarkComplete( stream, &keyPtr, &keyLength, 
										 keyLength );
		}
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid client pubkey data" ) );
		}

	/* Import the public-key data into a context */
	setMessageCreateObjectInfo( &createInfo, pubkeyAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, keyPtr, keyLength );
	status = krnlSendMessage( createInfo.cryptHandle, 
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_KEY_SSH );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, 
						  IMESSAGE_DECREFCOUNT );
		retExt( cryptArgError( status ) ? \
				CRYPT_ERROR_BADDATA : status,
				( cryptArgError( status ) ? \
				  CRYPT_ERROR_BADDATA : status, SESSION_ERRINFO, 
				  "Invalid client pubkey value" ) );
		}
	sessionInfoPtr->iKeyexAuthContext = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int checkPublicKeySig( INOUT SESSION_INFO *sessionInfoPtr,
							  const SSH_HANDSHAKE_INFO *handshakeInfo, 
							  INOUT STREAM *stream,
							  IN_BUFFER( userNameLength ) const void *userName, 
							  IN_LENGTH_TEXT const int userNameLength )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_KEYMGMT_INFO getkeyInfo DUMMY_INIT_STRUCT;
	MESSAGE_DATA msgData;
	BYTE keyID[ CRYPT_MAX_HASHSIZE + 8 ];
	BYTE holderName[ CRYPT_MAX_TEXTSIZE + 8 ];
	void *packetDataPtr DUMMY_INIT_PTR, *sigDataPtr;
	int packetDataLength, sigDataLength, holderNameLen, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( userName, userNameLength ) );

	REQUIRES( userNameLength > 0 && userNameLength <= CRYPT_MAX_TEXTSIZE );

	/* Make sure that this key is valid for authentication purposes, in 
	   other words that it's present in the authentication keyset.
	   
	   Note that this uses a not-entirely-reliable means of identifying the
	   key in the database in that CRYPT_IKEYID_KEYID is the 
	   subjectKeyIdentifier of the certificate if this is present, otherwise
	   a hash of the subjectPublicKey.  If the sKID is in a cryptlib-created 
	   certificate then the two are the same thing, but a certificate coming
	   from an external CA may contain arbitrary data for the sKID.  
	   However, since it's unlikely that someone is performing SSH client 
	   auth using a certificate from a commercial CA, it seems safe to 
	   assume that any certificate present will be a cryptlib-generated one 
	   used as a bit-bagging mechanism to get the key into a database, and
	   therefore that sKID == hash( subjectPublicKey ) */
	setMessageData( &msgData, keyID, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( sessionInfoPtr->iKeyexAuthContext, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData, 
							  CRYPT_IATTRIBUTE_KEYID );
	if( cryptStatusOK( status ) )
		{
		setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_KEYID, 
							   msgData.data, msgData.length, NULL, 0, 
							   KEYMGMT_FLAG_NONE );
		status = krnlSendMessage( sessionInfoPtr->cryptKeyset, 
								  IMESSAGE_KEY_GETKEY, &getkeyInfo, 
								  KEYMGMT_ITEM_PUBLICKEY );
		}
	if( cryptStatusError( status ) )
		{
#ifdef USE_ERRMSGS
		char keyIDText[ CRYPT_MAX_TEXTSIZE + 8 ];
#endif /* USE_ERRMSGS */

		formatFingerprint( keyIDText, CRYPT_MAX_TEXTSIZE, keyID, 
						   msgData.length );
		retExt( CRYPT_ERROR_PERMISSION,
				( CRYPT_ERROR_PERMISSION, SESSION_ERRINFO, 
				  "Client public key with ID '%s' is not trusted for "
				  "authentication purposes", keyIDText ) );
		}

	/* Check that the name in the certificate matches the supplied user 
	   name */
	setMessageData( &msgData, holderName, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( getkeyInfo.cryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_HOLDERNAME );
	krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DESTROY );
	if( cryptStatusOK( status ) )
		{
		holderNameLen = msgData.length;
		if( userNameLength != holderNameLen || \
			!compareDataConstTime( userName, holderName, userNameLength ) )
			status = CRYPT_ERROR_INVALID;
		}
	else
		{
		memcpy( holderName, "<<<Unknown>>>", 13 );
		holderNameLen = 13;
		}
	if( cryptStatusError( status ) )
		{
		BYTE userNameBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];

		REQUIRES( rangeCheck( userNameLength, 1, CRYPT_MAX_TEXTSIZE ) );
		memcpy( userNameBuffer, userName, userNameLength );
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
				  "Client public key name '%s' doesn't match supplied user "
				  "name '%s'", 
				  sanitiseString( holderName, CRYPT_MAX_TEXTSIZE, 
								  holderNameLen ),
				  sanitiseString( userNameBuffer, CRYPT_MAX_TEXTSIZE, 
								  userNameLength ) ) );
		}

	/* Get a pointer to the portion of the packet that gets signed */
	status = packetDataLength = stell( stream );
	if( !cryptStatusError( status ) )
		{
		status = sMemGetDataBlockAbs( stream, 0, &packetDataPtr, 
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
	status = hashAsString( createInfo.cryptHandle, 
						   handshakeInfo->sessionID,
						   handshakeInfo->sessionIDlength );
	if( cryptStatusOK( status ) )
		{
		static const BYTE packetID = SSH_MSG_USERAUTH_REQUEST;

		/* readAuthPacketSSH2() has stripped the ID byte from the start of 
		   the packet so we need to hash it in explicitly before we hash the 
		   rest of the packet */
		krnlSendMessage( createInfo.cryptHandle, IMESSAGE_CTX_HASH, 
						 ( MESSAGE_CAST ) &packetID, ID_SIZE );
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

	/* Check the signature.  The reason for the min() part of the expression 
	   is that iCryptCheckSignature() gets suspicious of very large buffer 
	   sizes, for example if the client were to specify the use of a huge 
	   signature packet */
	status = sMemGetDataBlockRemaining( stream, &sigDataPtr, 
										&sigDataLength );
	if( cryptStatusOK( status ) )
		{
		status = iCryptCheckSignature( sigDataPtr, sigDataLength, 
									   CRYPT_IFORMAT_SSH, 
									   sessionInfoPtr->iKeyexAuthContext, 
									   createInfo.cryptHandle, CRYPT_UNUSED, 
									   NULL );
		}
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Verification of client's pubkey auth failed" ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Verify the Client's Authentication					*
*																			*
****************************************************************************/

/* Handle client authentication */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processUserAuth( INOUT SESSION_INFO *sessionInfoPtr,
							const SSH_HANDSHAKE_INFO *handshakeInfo, 
							OUT_ENUM( USERAUTH ) USERAUTH_TYPE *userAuthInfo,
							IN_ENUM_OPT( CREDENTIAL ) \
								const CREDENTIAL_TYPE credentialType,
							const BOOLEAN initialAuth )
	{
	STREAM stream;
	const BOOLEAN allowPubkeyAuth = \
			( sessionInfoPtr->cryptKeyset != CRYPT_ERROR ) ? TRUE : FALSE;
	const METHOD_INFO *methodInfoTblPtr = allowPubkeyAuth ? \
			methodInfoTblPubkeyAuth : methodInfoTbl;
	const int methodInfoTblSize = allowPubkeyAuth ? \
			FAILSAFE_ARRAYSIZE( methodInfoTblPubkeyAuth, METHOD_INFO ) : \
			FAILSAFE_ARRAYSIZE( methodInfoTbl, METHOD_INFO );
	const ATTRIBUTE_LIST *attributeListPtr DUMMY_INIT_PTR;
	const METHOD_INFO *methodInfoPtr = NULL;
	BYTE userNameBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BYTE stringBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BOOLEAN kludgeFlag = FALSE;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int length, userNameLength, stringLength, i, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( userAuthInfo, sizeof( USERAUTH_TYPE * ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );
	REQUIRES( isEnumRangeOpt( credentialType, CREDENTIAL ) );
			  /* CREDENTIAL_NONE is a valid type since it indicates that we 
			     should record the user name and password for the caller to
				 check */
	REQUIRES( initialAuth == TRUE || initialAuth == FALSE );

	/* Clear the return value, or at least set it to the default failed-
	   authentication state */
	*userAuthInfo = USERAUTH_ERROR;

	/* Get the userAuth packet from the client:

		byte	type = SSH_MSG_USERAUTH_REQUEST
		string	user_name
		string	service_name = "ssh-connection"
		string	method_name = "none" | "password" | "publickey"
	  "password":
		boolean	FALSE/TRUE
		string	password
	  "publickey":
		boolean	FALSE/TRUE
		string		"ssh-rsa"	"ssh-dss"	"ecdsa-sha2-*"
		string		[ client key/certificate ]
			string	"ssh-rsa"	"ssh-dss"	"ecdsa-sha2-*"
			mpint	e			p			Q
			mpint	n			q
			mpint				g
			mpint				y
	  [	string		[ client signature ]			-- If boolean == TRUE
			string	"ssh-rsa"	"ssh-dss"	"ecdsa-sha2-*"
			string	signature ]

	    The client can optionally send a method-type of "none" as its first
		message to indicate that it'd like the server to return a list of 
		allowed authentication types, if we get a packet of this kind and 
		the initialAuth flag is set then we return our allowed types list */
	status = length = \
		readAuthPacketSSH2( sessionInfoPtr, SSH_MSG_USERAUTH_REQUEST,
							ID_SIZE + sizeofString32( 1 ) + \
								sizeofString32( 8 ) + sizeofString32( 4 ) );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	CFI_CHECK_UPDATE( "readAuthPacketSSH2" );

	/* Read the user name, service name, and authentication method type */
	status = readString32( &stream, userNameBuffer, CRYPT_MAX_TEXTSIZE, 
						   &userNameLength );
	if( cryptStatusError( status ) || \
		userNameLength <= 0 || userNameLength > CRYPT_MAX_TEXTSIZE )
		{
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid user-authentication user name" ) );
		}
	status = readString32( &stream, stringBuffer, CRYPT_MAX_TEXTSIZE, 
						   &stringLength );
	if( cryptStatusOK( status ) )
		{
		if( stringLength != 14 || \
			memcmp( stringBuffer, "ssh-connection", 14 ) )
			status = CRYPT_ERROR_BADDATA;
		}
	if( cryptStatusOK( status ) )
		{
		status = readString32( &stream, stringBuffer, CRYPT_MAX_TEXTSIZE,
							   &stringLength );
		}
	if( cryptStatusOK( status ) )
		{
		if( stringLength <= 0 || stringLength > CRYPT_MAX_TEXTSIZE )
			status = CRYPT_ERROR_BADDATA;
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid user-authentication service or method name" ) );
		}
	INJECT_FAULT( CORRUPT_ID, SESSION_CORRUPT_ID_SSH_1 );
	CFI_CHECK_UPDATE( "checkAuthPacketSSH2" );

	/* Check which authentication method the client has requested.  We 
	   perform this and the following checks for credential/query validity 
	   before any checking of the user name to make it harder for an 
	   attacker to scan for valid accounts (but see also the comment about 
	   this issue further on where the user-name check is performed) */
	LOOP_SMALL( i = 0, methodInfoTblPtr[ i ].type != METHOD_NONE && \
					   i < methodInfoTblSize, i++ )
		{
		const METHOD_INFO *methodInfo = &methodInfoTblPtr[ i ];

		if( methodInfo->nameLength == stringLength && \
			!memcmp( methodInfo->name, stringBuffer, stringLength ) )
			{
			methodInfoPtr = methodInfo;
			break;
			}
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( i < methodInfoTblSize );
	if( methodInfoPtr == NULL )
		{
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Unknown user-authentication method type '%s'",
				  sanitiseString( stringBuffer, CRYPT_MAX_TEXTSIZE,
								  stringLength ) ) );
		}

	/* Check that the query submitted by the client is valid, meaning that 
	   it doesn't appear to be an attempt to subvert the authentication 
	   process in some way, e.g. by changing data during subsequent rounds
	   of the authentication negotiation */
	status = checkQueryValidity( sessionInfoPtr->sessionSSH, userNameBuffer, 
								 userNameLength, methodInfoPtr->type, 
								 initialAuth );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );

		/* There are two slightly different error conditions that we can 
		   encounter here, we provide distinct error messages to give the
		   caller more information on what went wrong */
		if( status == CRYPT_ERROR_DUPLICATE )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Client sent duplicate authentication requests with "
					  "method 'none'" ) );
			}
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
				  "Client supplied different credentials than the ones "
				  "supplied during a previous authentication attempt" ) );
		}
	CFI_CHECK_UPDATE( "checkQueryValidity" );

	/* Read the kludge flag used to denote all sorts of additional 
	   functionality kludged onto the basic message.  In some cases the
	   kludgeFlag is set to TRUE (the password-auth message is actually a
	   change-password message), in others it's set to FALSE (the pubkey
	   auth message doesn't contain any pubkey auth) */
	if( methodInfoPtr->type != METHOD_QUERY )
		{
		int value;

		status = value = sgetc( &stream );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		kludgeFlag = value ? TRUE : FALSE;
		}

	/* Read the authentication information */
	switch( methodInfoPtr->type )
		{
		case METHOD_QUERY:
			/* No payload */
			break;

		case METHOD_PASSWORD:
			/* Read the password and check that it's approximately valid */
			status = readString32( &stream, stringBuffer, CRYPT_MAX_TEXTSIZE,
								   &stringLength );
			if( cryptStatusOK( status ) )
				{
				if( stringLength <= 0 || stringLength > CRYPT_MAX_TEXTSIZE )
					status = CRYPT_ERROR_BADDATA;
				}
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( &stream );
				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
						  "Invalid password payload" ) );
				}
			INJECT_FAULT( CORRUPT_AUTHENTICATOR, 
						  SESSION_CORRUPT_AUTHENTICATOR_SSH_1 );
			break;

		case METHOD_PUBKEY:
			/* Read the public key */
			status = readPublicKey( sessionInfoPtr, &stream );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( &stream );
				return( status );
				}
			break;

		default:
			retIntError();
		}
	CFI_CHECK_UPDATE( "readAuthInfo" );

	/* If the user credentials are pre-set make sure that the newly-
	   submitted user name matches an existing one.  Note that we've left 
	   this check as late as possible (so that it's right next to the 
	   password check) to avoid timing attacks that might help an attacker 
	   guess a valid user name.  On the other hand given the typical pattern 
	   of SSH password-guessing attacks which just run through a fixed set 
	   of user names this probably isn't worth the trouble since it'll have 
	   little to no effect on attackers, so what it's avoiding is purely a 
	   certificational weakness.

	   There's also a slight anomaly in error reporting here, if the client
	   is performing a dummy read and there are pre-set user credentials
	   present then instead of the expected list of available authentication
	   methods they'll get an error response.  This is consistent with the
	   specification (which is ambiguous on the topic), it says that the 
	   server must return a failure response and may also include a list of 
	   allowed methods, but it may not be what the client is expecting.  
	   This problem occurs because of the overloading of the authentication
	   mechanism as a method-query mechanism, it's not clear whether the
	   query response or the username-check response is supposed to take
	   precedence */
	if( credentialType != CREDENTIAL_NONE )
		{
		attributeListPtr = \
					findSessionInfoEx( sessionInfoPtr, CRYPT_SESSINFO_USERNAME,
									   userNameBuffer, userNameLength );
		if( attributeListPtr == NULL )
			{
			sMemDisconnect( &stream );
			( void ) sendResponseFailure( sessionInfoPtr );
			retExt( CRYPT_ERROR_WRONGKEY,
					( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
					  "Unknown/invalid user name '%s'", 
					  sanitiseString( userNameBuffer, CRYPT_MAX_TEXTSIZE, 
									  userNameLength ) ) );
			}

		/* We've matched an existing user name, select the attribute that
		   contains it */
		DATAPTR_SET( sessionInfoPtr->attributeListCurrent,
					 ( ATTRIBUTE_LIST * ) attributeListPtr );
		}
	else
		{
		/* It's a new user name, add it */
		status = addSessionInfoS( sessionInfoPtr,
								  CRYPT_SESSINFO_USERNAME,
								  userNameBuffer, userNameLength );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Error recording user name '%s'", 
					  sanitiseString( userNameBuffer, CRYPT_MAX_TEXTSIZE,
									  userNameLength ) ) );
			}
		}
	CFI_CHECK_UPDATE( "findSessionInfoEx" );

	/* If the client just wants a list of supported authentication 
	   mechanisms tell them what we allow (handled by sending a failed-
	   authentication response, which contains a list of mechanisms that can 
	   be used to continue) and await further input */
	if( methodInfoPtr->type == METHOD_QUERY )
		{
		sMemDisconnect( &stream );

		/* Tell the client which authentication methods can continue */
		status = sendResponseFailureInfo( sessionInfoPtr, allowPubkeyAuth );
		if( cryptStatusError( status ) )
			return( status );
		CFI_CHECK_UPDATE( "sendResponseFailureInfo" );

		/* Inform the caller that this was a no-op pass and the client can 
		   try again */
		*userAuthInfo = USERAUTH_NOOP;

		REQUIRES( CFI_CHECK_SEQUENCE_6( "readAuthPacketSSH2", 
										"checkAuthPacketSSH2", 
										"checkQueryValidity", "readAuthInfo", 
										"findSessionInfoEx",
										"sendResponseFailureInfo" ) );
		return( OK_SPECIAL );
		}
	CFI_CHECK_UPDATE( "METHOD_QUERY" );

	/* If it's public-key auth, try and verify the signature */
	if( methodInfoPtr->type == METHOD_PUBKEY )
		{
		/* In yet another variant of SSH's Lucy-and-Charlie-Brown 
		   authentication process, for pubkey auth the client can supply a 
		   public key but omit the signature, in which case the server has 
		   to go through yet another round of messages exchanged in the hope 
		   of eventually getting the actual authentication data */
		if( !kludgeFlag )
			{
			int pkcAlgo;

			/* Repeated partial pubkey auth messages are a sign that 
			   something is wrong */
			if( credentialType == CREDENTIAL_USERNAME_PUBKEY )
				{
				sMemDisconnect( &stream );
				( void ) sendResponseFailure( sessionInfoPtr );
				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
						  "Client sent duplicate partial pubkey "
						  "authentication request" ) );
				}

			/* If we're given a pubkey auth message with the signature 
			   missing, we have to send back a copy of the public key 
			   algorithm name and data, presumably to confirm that the three 
			   copies the client sent us arrived in good order:
			   
				byte	type = SSH_MSG_USERAUTH_PK_OK
				string	pubkey_algorithm_from_request
				string	pubkey_data_from_request
		
			   There's no obvious reason for this, and no implementation 
			   that bothers with this additional unnecessary step appears to 
			   check the returned data except for OpenSSH, and even that 
			   only checks it as an implementation artefact.  What OpenSSH 
			   does is throw every key it has available at the server until 
			   it finds one that sticks, however it doesn't remember which 
			   key was sent in which request but relies on the server's 
			   response to tell it which one it was.  It's likely that this 
			   message only exists as an OpenSSH quirk that got written into 
			   the spec, and as implemented in OpenSSH it allows a server to 
			   request auth from a key other than the one the client thinks 
			   it's using by sending back an ACK for key B when the client 
			   has proposed key A.
			   
			   Because only OpenSSH appears to bother checking this message, 
			   we send back an easter egg for any other implementation to 
			   catch anything that we're not currently aware of that does 
			   actually check */
			status = openPacketStreamSSH( &stream, sessionInfoPtr, 
										  SSH_MSG_USERAUTH_PK_OK );
			if( cryptStatusError( status ) )
				return( status );
			if( TEST_FLAG( sessionInfoPtr->protocolFlags, 
						   SSH_PFLAG_CHECKSPKOK ) )
				{
				status = krnlSendMessage( sessionInfoPtr->iKeyexAuthContext, 
										  IMESSAGE_GETATTRIBUTE, &pkcAlgo, 
										  CRYPT_CTXINFO_ALGO );
				if( cryptStatusOK( status ) )
					{
					status = writeAlgoStringEx( &stream, pkcAlgo, 
												handshakeInfo->hashAlgo, 0,
												SSH_ALGOSTRINGINFO_NONE );
					}
				if( cryptStatusOK( status ) )
					{
					status = exportAttributeToStream( &stream, 
										sessionInfoPtr->iKeyexAuthContext,
										CRYPT_IATTRIBUTE_KEY_SSH );
					}
				}
			else
				{
				writeString32( &stream, "Surprise! Does anything check this?", 
							   35 );
				status = writeString32( &stream, "Blah, blah, blah, stolen "
										"plans, blah, blah, blah, missing "
										"scientist, blah, blah, blah, atom "
										"bomb, blah, blah, blah", 114 );
				}
			if( cryptStatusOK( status ) )
				status = wrapSendPacketSSH2( sessionInfoPtr, &stream );
			sMemDisconnect( &stream );
			if( cryptStatusError( status ) )
				return( status );
			CFI_CHECK_UPDATE( "wrapSendPacketSSH2" );

			/* Inform the caller that this was yet another no-op pass and 
			   the client can try again */
			*userAuthInfo = USERAUTH_NOOP_2;

			REQUIRES( CFI_CHECK_SEQUENCE_7( "readAuthPacketSSH2", 
											"checkAuthPacketSSH2", 
											"checkQueryValidity", "readAuthInfo", 
											"findSessionInfoEx", "METHOD_QUERY",
											"wrapSendPacketSSH2" ) );
			return( OK_SPECIAL );
			}

		/* Check the signature on the authentication data */
		status = checkPublicKeySig( sessionInfoPtr, handshakeInfo, &stream,
									userNameBuffer, userNameLength );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			{
			( void ) sendResponseFailure( sessionInfoPtr );
			return( status );
			}
		CFI_CHECK_UPDATE( "checkPublicKeySig" );

		/* The user has successfully authenticated, let the client know and 
		   indicate this through a failsafe two-value return status (see the 
		   comment for processFixedAuth()/processServerAuth() for details) */
		status = sendResponseSuccess( sessionInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		*userAuthInfo = USERAUTH_SUCCESS;
		CFI_CHECK_UPDATE( "sendResponseSuccess" );

		REQUIRES( CFI_CHECK_SEQUENCE_8( "readAuthPacketSSH2", 
										"checkAuthPacketSSH2", 
										"checkQueryValidity", "readAuthInfo", 
										"findSessionInfoEx", "METHOD_QUERY",
										"checkPublicKeySig", 
										"sendResponseSuccess" ) );
		return( CRYPT_OK );
		}
	sMemDisconnect( &stream );
	CFI_CHECK_UPDATE( "METHOD_PUBKEY" );

	ENSURES( methodInfoPtr->type == METHOD_PASSWORD );

	/* If the client started with a partial pubkey auth then they can't 
	   continue with a password auth */
	if( credentialType == CREDENTIAL_USERNAME_PUBKEY )
		{
		sMemDisconnect( &stream );
		( void ) sendResponseFailure( sessionInfoPtr );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Client started pubkey auth but tried to complete with "
				  "password auth" ) );
		}

	/* If full user credentials are present then the user name has been 
	   matched to a caller-supplied list of allowed { user name, password } 
	   pairs and we move on to the corresponding password and verify it */
	if( credentialType == CREDENTIAL_USERNAME_PASSWORD )
		{
		/* Move on to the password associated with the user name */
		REQUIRES( DATAPTR_ISVALID( attributeListPtr->next ) );
		attributeListPtr = DATAPTR_GET( attributeListPtr->next );
		ENSURES( attributeListPtr != NULL && \
				 attributeListPtr->attributeID == CRYPT_SESSINFO_PASSWORD );
				 /* Ensured by checkMissingInfo() in sess_iattr.c */

		/* Make sure that the password matches.  Note that in the case of an
		   error we don't report the incorrect password that was entered 
		   since we don't want it appearing in logfiles */
		if( stringLength != attributeListPtr->valueLength || \
			!compareDataConstTime( stringBuffer, attributeListPtr->value, 
								   stringLength ) )
			{
			retExt( CRYPT_ERROR_WRONGKEY,
					( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
					  "Invalid password for user '%s'", 
					  sanitiseString( userNameBuffer, CRYPT_MAX_TEXTSIZE,
									  userNameLength ) ) );
			}

		/* The user has successfully authenticated, let the client know and 
		   indicate this through a failsafe two-value return status (see the 
		   comment for processFixedAuth()/processServerAuth() for details) */
		status = sendResponseSuccess( sessionInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		*userAuthInfo = USERAUTH_SUCCESS;
		CFI_CHECK_UPDATE( "sendResponseSuccess" );

		REQUIRES( CFI_CHECK_SEQUENCE_8( "readAuthPacketSSH2", 
										"checkAuthPacketSSH2", 
										"checkQueryValidity", "readAuthInfo", 
										"findSessionInfoEx", "METHOD_QUERY",
										"METHOD_PUBKEY", "sendResponseSuccess" ) );
		return( CRYPT_OK );
		}
	CFI_CHECK_UPDATE( "CREDENTIAL_USERNAME_PASSWORD" );

	ENSURES( credentialType == CREDENTIAL_NONE || \
			 credentialType == CREDENTIAL_USERNAME );

	/* There are no pre-set credentials present to match against, record the 
	   password for the caller to check, making it an ephemeral attribute 
	   since the client could try and re-enter it on a subsequent iteration 
	   if we tell them that it's incorrect.  This adds the password after 
	   the first user name that it finds but since there's only one user
	   name present, namely the one that was recorded or matched above, 
	   there's no problems with potentially ambiguous password entries in
	   the attribute list */
	status = updateSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_PASSWORD,
								stringBuffer, stringLength,
								CRYPT_MAX_TEXTSIZE, ATTR_FLAG_EPHEMERAL );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Error recording password for user '%s'",
				  sanitiseString( userNameBuffer, CRYPT_MAX_TEXTSIZE,
								  userNameLength ) ) );
		}
	CFI_CHECK_UPDATE( "updateSessionInfo" );

	*userAuthInfo = USERAUTH_CALLERCHECK;

	REQUIRES( CFI_CHECK_SEQUENCE_9( "readAuthPacketSSH2", 
									"checkAuthPacketSSH2", "checkQueryValidity", 
									"readAuthInfo", "findSessionInfoEx", 
									"METHOD_QUERY", "METHOD_PUBKEY", 
									"CREDENTIAL_USERNAME_PASSWORD", 
									"updateSessionInfo" ) );
	return( OK_SPECIAL );
	}

/****************************************************************************
*																			*
*						Perform Server-side Authentication					*
*																			*
****************************************************************************/

/* Server-side authentication is a critical authorisation step so we don't 
   want to make it vulnerable to a simple boolean control-variable overwrite
   that an attacker can use to bypass the authentication check.  To do this
   we require confirmation both via the function return status and the by-
   reference value, we require the two values to be different values (one a
   zero value, the other a small nonzero integer value), and we store them 
   separated by a canary that's also checked when the status values are
   checked.  In theory it's still possible to overwrite this if an exact
   pattern of 96 bits (on a 32-bit system) can be placed in memory, but this
   is now vastly harder than simply entering an over-long user name or 
   password that converts an access-granted boolean-flag zero value to a 
   nonzero value */

typedef struct {
	USERAUTH_TYPE userAuthInfo;
	int canary;
	int status;
	} FAILSAFE_AUTH_INFO;

static const FAILSAFE_AUTH_INFO failsafeAuthSuccessTemplate = \
	{ USERAUTH_SUCCESS, OK_SPECIAL, CRYPT_OK };

#define initFailsafeAuthInfo( authInfo ) \
	{ \
	memset( ( authInfo ), 0, sizeof( FAILSAFE_AUTH_INFO ) ); \
	( authInfo )->userAuthInfo = USERAUTH_ERROR; \
	( authInfo )->canary = OK_SPECIAL; \
	( authInfo )->status = CRYPT_ERROR_FAILED; \
	}

/* Process the client's authentication */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processFixedAuth( INOUT SESSION_INFO *sessionInfoPtr,
							 const SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	FAILSAFE_AUTH_INFO authInfo DUMMY_INIT_STRUCT;
	BOOLEAN isFatalError;
	int authAttempts, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );

	/* The caller has specified user credentials to match against so we can
	   perform a basic pass/fail check against the client-supplied 
	   information.  Since this is an all-or-nothing process at the end of 
	   which the client is either authenticated or not authenticated we 
	   allow the traditional three retries (or until the first fatal error) 
	   to get it right */
	LOOP_SMALL( ( isFatalError = FALSE, authAttempts = 0 ), 
				!isFatalError && authAttempts < 3,
				authAttempts++ )
		{
		/* Process the user authentication and, if it's a dummy read, try a 
		   second time.  This can only happen on the first iteration since 
		   the initialAuth flag for processUserAuth() is set to FALSE for 
		   subsequent attempts, which means that further dummy reads are 
		   disallowed */
		initFailsafeAuthInfo( &authInfo );
		authInfo.status = processUserAuth( sessionInfoPtr, handshakeInfo,
									&authInfo.userAuthInfo, 
									CREDENTIAL_USERNAME_PASSWORD, 
									( authAttempts <= 0 ) ? TRUE : FALSE );
		if( authInfo.status == OK_SPECIAL && \
			authInfo.userAuthInfo == USERAUTH_NOOP )
			{
			/* It was a dummy read, try again */
			authInfo.status = processUserAuth( sessionInfoPtr, handshakeInfo,
										&authInfo.userAuthInfo, 
										CREDENTIAL_USERNAME_PASSWORD, 
										FALSE );
			}
		if( cryptStatusOK( authInfo.status ) && \
			!memcmp( &authInfo, &failsafeAuthSuccessTemplate, \
					 sizeof( FAILSAFE_AUTH_INFO ) ) )
			{
			/* The user has authenticated successfully and this fact has 
			   been verified in a (reasonably) failsafe manner, we're 
			   done */
			return( sendResponseSuccess( sessionInfoPtr ) );
			}
		ENSURES( cryptStatusError( authInfo.status ) );

		/* If the authentication processing returns anything other than a 
		   wrong-key error due to a failed authentication attempt then the
		   error is fatal */
		if( authInfo.status != CRYPT_ERROR_WRONGKEY )
			isFatalError = TRUE;

		/* Tell the client that the authentication failed.  If it's the 
		   final attempt (either because it's a fatal error or because the
		   user has used up all of their retries) then we ignore any return 
		   code since it's secondary to the authentication-failed status 
		   that we're returning, and in any case since the caller is going 
		   to close the session in response to this it'll be signalled to 
		   the client one way or another */
		if( isFatalError || authAttempts >= 2 )
			( void ) sendResponseFailure( sessionInfoPtr );
		else
			{
			int status;

			status = sendResponseFailureInfo( sessionInfoPtr, FALSE );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( authInfo.status != OK_SPECIAL );

	/* The user still hasn't successfully authenticated after multiple 
	   attempts, we're done */
	return( authInfo.status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processServerAuth( INOUT SESSION_INFO *sessionInfoPtr,
					   const SSH_HANDSHAKE_INFO *handshakeInfo, 
					   const BOOLEAN userInfoPresent )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	USERAUTH_TYPE userAuthInfo;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int status DUMMY_INIT;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSH( sessionInfoPtr ) );
	REQUIRES( userInfoPresent == TRUE || userInfoPresent == FALSE );

	/* If the caller has specified user credentials to match against we 
	   perform a basic pass/fail check against the client-supplied 
	   information */
	if( userInfoPresent )
		return( processFixedAuth( sessionInfoPtr, handshakeInfo ) );

	/* The caller hasn't supplied any user credentials to match against,
	   indicating that they're going to perform an on-demand match.  If this 
	   is a second pass through then the caller will have supplied an
	   authentication response to be sent to the client, in which case we
	   send it before continuing */
	if( sshInfo->authRead )
		{
		/* If the caller accepted the authentication, we're done */
		if( sessionInfoPtr->authResponse == AUTHRESPONSE_SUCCESS )
			return( sendResponseSuccess( sessionInfoPtr ) );

		/* The caller denied the authentication, inform the client and go 
		   back to asking what to do at the next authentication attempt */
		status = sendResponseFailureInfo( sessionInfoPtr, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		sessionInfoPtr->authResponse = AUTHRESPONSE_NONE;
		}
	CFI_CHECK_UPDATE( "authRead" );

	/* Process the user authentication and, if it's a dummy read, try a 
	   second time and even a third time (see the comments at the start of
	   this module).  The first time that we're called (authRead == FALSE) 
	   we accept and record the user name supplied by the client, on any 
	   subsequent calls (authRead == TRUE) we require a match for the 
	   previously-supplied user name.

	   Note that we don't use the complex failsafe-check method used by
	   processFixedAuth() because we're not performing the authorisation
	   check here but simply passing the data back to the caller */
	if( !sshInfo->authRead )
		{
		status = processUserAuth( sessionInfoPtr, handshakeInfo, 
								  &userAuthInfo, CREDENTIAL_NONE, TRUE );
		if( status == OK_SPECIAL && userAuthInfo == USERAUTH_NOOP )
			{
			/* It was a dummy read, try again */
			status = processUserAuth( sessionInfoPtr, handshakeInfo,
									  &userAuthInfo, CREDENTIAL_USERNAME, 
									  FALSE );
			}
		if( status == OK_SPECIAL && userAuthInfo == USERAUTH_NOOP_2 )
			{
			/* It was yet another a dummy read, try again */
			status = processUserAuth( sessionInfoPtr, handshakeInfo,
									  &userAuthInfo, 
									  CREDENTIAL_USERNAME_PUBKEY, FALSE );
			}
		}
	else
		{
		status = processUserAuth( sessionInfoPtr, handshakeInfo, 
								  &userAuthInfo, CREDENTIAL_USERNAME, 
								  FALSE );
		ENSURES( !( status == OK_SPECIAL && \
					userAuthInfo == USERAUTH_NOOP ) );
		}
	sshInfo->authRead = TRUE;
	ENSURES( ( cryptStatusOK( status ) && \
			   userAuthInfo == USERAUTH_SUCCESS ) || \
			 ( ( cryptStatusError( status ) || status == OK_SPECIAL ) && \
			   userAuthInfo != USERAUTH_SUCCESS ) );
	if( cryptStatusError( status ) && status != OK_SPECIAL )
		return( status );
	CFI_CHECK_UPDATE( "processUserAuth" );

	REQUIRES( CFI_CHECK_SEQUENCE_2( "authRead", "processUserAuth" ) );
	return( ( status == OK_SPECIAL ) ? CRYPT_ENVELOPE_RESOURCE : CRYPT_OK );
	}
#endif /* USE_SSH */
