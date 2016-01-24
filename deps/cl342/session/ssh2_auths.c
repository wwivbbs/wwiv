/****************************************************************************
*																			*
*			cryptlib SSHv2 Server-side Authentication Management			*
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
   password.  In order to accommodate public-key authentication (currently 
   not handled, see the comment further on) we also verify that the 
   authentication method remains constant over successive iterations, i.e. 
   that the client doesn't try part of an authentication with a public key 
   and then another part with a password.

   Handling the state machine required to process this gets a bit 
   complicated, the protocol flow that we enforce is:

	Step 0 (optional):

		Client sends SSH_MSG_USERAUTH_REQUEST with method "none" to query 
		available authentication method types.

		Server responsd with SSH_MSG_USERAUTH_FAILURE listing available 
		methods.

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

	OK_SPECIAL + uAI = USERAUTH_NOOP:  No-op read for a client query of 
		available authentication methods via the pseudo-method "none".

	Error + uAI = USERAUTH_ERROR: Standard error status, which includes non-
		matched credentials */

typedef enum {
	USERAUTH_NONE,		/* No authentication type */
	USERAUTH_SUCCESS,	/* User authenticated successfully */
	USERAUTH_CALLERCHECK,/* Caller must check whether auth.was successful */
	USERAUTH_NOOP,		/* No-op read */
	USERAUTH_ERROR,		/* User failed authentication */
	USERAUTH_LAST		/* Last possible authentication type */
	} USERAUTH_TYPE;

/* The match options with a caller-supplied list of credentials to match 
   against are:

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

   Unlike SSHv1, SSHv2 properly identifies public keys, however because of
   its complexity (several more states added to the state machine because of
   SSH's propensity for carrying out any negotiation that it performs in lots
   of little bits and pieces) we don't support this form of authentication
   until someone specifically requests it */

/* A lookup table for the authentication methods submitted by the client */

typedef enum {
	METHOD_NONE,		/* No authentication method type */
	METHOD_QUERY,		/* Query available authentication methods */
	METHOD_PASSWORD,	/* Password authentication */
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

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Send a succeeded/failed-authentication response to the client:

	byte	type = SSH_MSG_USERAUTH_SUCCESS

   or

	byte	type = SSH_MSG_USERAUTH_FAILURE
	string	allowed_authent = "password" | empty
	boolean	partial_success = FALSE */

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
	status = sendPacketSSH2( sessionInfoPtr, &stream, FALSE );
	sMemDisconnect( &stream );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendResponseFailure( INOUT SESSION_INFO *sessionInfoPtr,
								const BOOLEAN allowFurtherAuth )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	status = openPacketStreamSSH( &stream, sessionInfoPtr, 
								  SSH_MSG_USERAUTH_FAILURE );
	if( cryptStatusError( status ) )
		return( status );
	if( allowFurtherAuth )
		writeString32( &stream, "password", 8 );
	else
		writeUint32( &stream, 0 );
	status = sputc( &stream, 0 );
	if( cryptStatusOK( status ) )
		status = sendPacketSSH2( sessionInfoPtr, &stream, FALSE );
	sMemDisconnect( &stream );

	return( status );
	}

/* Check that the credentials submitted by the client are consistent with 
   any information submitted during earlier rounds of authentication */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkCredentialsConsistent( INOUT SSH_INFO *sshInfo,
									   IN_BUFFER( userNameLength ) \
											const void *userName,
									   IN_LENGTH_TEXT const int userNameLength,
									   IN_ENUM( METHOD ) \
											const METHOD_TYPE authMethod,
									   const BOOLEAN isInitialAuth )
	{
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	BYTE userNameHash[ KEYID_SIZE + 8 ];

	assert( isWritePtr( sshInfo, sizeof( SSH_INFO ) ) );
	assert( isReadPtr( userName, userNameLength ) );

	REQUIRES( userNameLength >= 1 && userNameLength <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( authMethod > METHOD_NONE && authMethod < METHOD_LAST );

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
		/* We've already processed a query message, any subsequent message
		   has to be an actual authentication message */
		if( authMethod == METHOD_QUERY )
			return( CRYPT_ERROR_DUPLICATE );

		/* We've had a query followed by a standard authentication method, 
		   remember the authentication method for later */
		sshInfo->authType = authMethod;
		return( CRYPT_OK );
		}

	/* We've already seen a standard authentication method, the new method 
	   must be the same */
	if( sshInfo->authType != authMethod )
		return( CRYPT_ERROR_INVALID );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Verify the Client's Authentication					*
*																			*
****************************************************************************/

/* Handle client authentication */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processUserAuth( INOUT SESSION_INFO *sessionInfoPtr,
							OUT_ENUM( USERAUTH ) USERAUTH_TYPE *userAuthInfo,
							IN_ENUM_OPT( CREDENTIAL ) \
								const CREDENTIAL_TYPE credentialType,
							const BOOLEAN initialAuth )
	{
	STREAM stream;
	const ATTRIBUTE_LIST *attributeListPtr = DUMMY_INIT_PTR;
	const METHOD_INFO *methodInfoPtr = NULL;
	BYTE userNameBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	BYTE stringBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	int length, userNameLength, stringLength, i, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( credentialType >= CREDENTIAL_NONE && \
			  credentialType < CREDENTIAL_LAST );
			  /* CREDENTIAL_NONE is a valid type since it indicates that we 
			     should record the user name and password for the caller to
				 check */

	/* Clear the return value, or at least set it to the default failed-
	   authentication state */
	*userAuthInfo = USERAUTH_ERROR;

	/* Get the userAuth packet from the client:

		byte	type = SSH_MSG_USERAUTH_REQUEST
		string	user_name
		string	service_name = "ssh-connection"
		string	method_name = "none" | "password"
		[ boolean	FALSE			-- For method = "password"
		  string	password ]

	    The client can optionally send a method-type of "none" as its first
		message to indicate that it'd like the server to return a list of 
		allowed authentication types, if we get a packet of this kind and 
		the initialAuth flag is set then we return our allowed types list */
	status = length = \
		readHSPacketSSH2( sessionInfoPtr, SSH_MSG_USERAUTH_REQUEST,
						  ID_SIZE + sizeofString32( "", 1 ) + \
							sizeofString32( "", 8 ) + \
							sizeofString32( "", 4 ) );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );

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
				  "Invalid user-authentication service or method name" ) );
		}

	/* Check which authentication method the client has requested.  Note that 
	   we perform this and the following checks before any checking of the
	   user name to make it harder for an attacker to scan for valid accounts 
	   (but see also the comment further on this issue further on where the 
	   user-name check is performed) */
	for( i = 0; methodInfoTbl[ i ].type != METHOD_NONE && \
				i < FAILSAFE_ARRAYSIZE( methodInfoTbl, METHOD_INFO ); i++ )
		{
		const METHOD_INFO *methodInfo = &methodInfoTbl[ i ];

		if( methodInfo->nameLength == stringLength && \
			!memcmp( methodInfo->name, stringBuffer, stringLength ) )
			{
			methodInfoPtr = methodInfo;
			break;
			}
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( methodInfoTbl, METHOD_INFO ) );
	if( methodInfoPtr == NULL )
		{
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Unknown user-authentication method type '%s'",
				  sanitiseString( stringBuffer, CRYPT_MAX_TEXTSIZE,
								  stringLength ) ) );
		}
	( void ) sgetc( &stream );	/* Skip boolean flag */

	/* Check that the credentials submitted by the client are consistent 
	   with any information submitted during earlier rounds of 
	   authentication */
	status = checkCredentialsConsistent( sessionInfoPtr->sessionSSH, 
										 userNameBuffer, userNameLength, 
										 methodInfoPtr->type, 
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

	/* If it's a proper user authentication rather than a dummy read to get
	   a list of supported authentication types, read the user password */
	if( methodInfoPtr->type != METHOD_QUERY )
		{
		status = readString32( &stream, stringBuffer, CRYPT_MAX_TEXTSIZE,
							   &stringLength );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) || \
			stringLength <= 0 || stringLength > CRYPT_MAX_TEXTSIZE )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid user-authentication payload" ) );
			}
		}

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
					findSessionInfoEx( sessionInfoPtr->attributeList,
									   CRYPT_SESSINFO_USERNAME,
									   userNameBuffer, userNameLength );
		if( attributeListPtr == NULL )
			{
			( void ) sendResponseFailure( sessionInfoPtr, FALSE );
			retExt( CRYPT_ERROR_WRONGKEY,
					( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
					  "Unknown/invalid user name '%s'", 
					  sanitiseString( userNameBuffer, CRYPT_MAX_TEXTSIZE, 
									  userNameLength ) ) );
			}

		/* We've matched an existing user name, select the attribute that
		   contains it */
		sessionInfoPtr->attributeListCurrent = \
								( ATTRIBUTE_LIST * ) attributeListPtr;
		}
	else
		{
		/* It's a new user name, add it */
		status = addSessionInfoS( &sessionInfoPtr->attributeList,
								  CRYPT_SESSINFO_USERNAME,
								  userNameBuffer, userNameLength );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Error recording user name '%s'", 
					  sanitiseString( userNameBuffer, CRYPT_MAX_TEXTSIZE,
									  userNameLength ) ) );
			}
		}

	/* If the client just wants a list of supported authentication 
	   mechanisms tell them what we allow (handled by sending a failed-
	   authentication response, which contains a list of mechanisms that can 
	   be used to continue) and await further input */
	if( methodInfoPtr->type == METHOD_QUERY )
		{
		sMemDisconnect( &stream );

		/* Tell the client which authentication methods can continue */
		status = sendResponseFailure( sessionInfoPtr, TRUE );
		if( cryptStatusError( status ) )
			return( status );

		/* Inform the caller that this was a no-op pass and the client can 
		   try again */
		*userAuthInfo = USERAUTH_NOOP;
		return( OK_SPECIAL );
		}

	/* If full user credentials are present then the user name has been 
	   matched to a caller-supplied list of allowed { user name, password } 
	   pairs and we move on to the corresponding password and verify it */
	if( credentialType == CREDENTIAL_USERNAME_PASSWORD )
		{
		/* Move on to the password associated with the user name */
		attributeListPtr = attributeListPtr->next;
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

		/* The user has successfully authenticated, indicate this through a 
		   failsafe two-value return status (see the comment for 
		   processFixedAuth()/processServerAuth() for details) */
		*userAuthInfo = USERAUTH_SUCCESS;
		return( CRYPT_OK );
		}

	/* There are no pre-set credentials present to match against, record the 
	   password for the caller to check, making it an ephemeral attribute 
	   since the client could try and re-enter it on a subsequent iteration 
	   if we tell them that it's incorrect.  This adds the password after 
	   the first user name that it finds but since there's only one user
	   name present, namely the one that was recorded or matched above, 
	   there's no problems with potentially ambiguous password entries in
	   the attribute list */
	status = updateSessionInfo( &sessionInfoPtr->attributeList,
								CRYPT_SESSINFO_PASSWORD,
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

	*userAuthInfo = USERAUTH_CALLERCHECK;
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
   password that converts the following boolean-flag zero value to a nonzero
   value */

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
	( authInfo )->canary = OK_SPECIAL; \
	( authInfo )->status = CRYPT_ERROR_FAILED; \
	}

/* Process the client's authentication */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int processFixedAuth( INOUT SESSION_INFO *sessionInfoPtr )
	{
	FAILSAFE_AUTH_INFO authInfo = DUMMY_INIT_STRUCT;
	BOOLEAN isFatalError;
	int authAttempts;

	/* The caller has specified user credentials to match against so we can
	   perform a basic pass/fail check against the client-supplied 
	   information.  Since this is an all-or-nothing process at the end of 
	   which the client is either authenticated or not authenticated we 
	   allow the traditional three retries (or until the first fatal error) 
	   to get it right */
	for( isFatalError = FALSE, authAttempts = 0; 
		 !isFatalError && authAttempts < 3; authAttempts++ )
		{
		/* Process the user authentication and, if it's a dummy read, try a 
		   second time.  This can only happen on the first iteration since 
		   the initialAuth flag for processUserAuth() is set to FALSE for 
		   subsequent attempts, which means that further dummy reads are 
		   disallowed */
		initFailsafeAuthInfo( &authInfo );
		authInfo.status = processUserAuth( sessionInfoPtr, 
									&authInfo.userAuthInfo, 
									CREDENTIAL_USERNAME_PASSWORD, 
									( authAttempts <= 0 ) ? TRUE : FALSE );
		if( authInfo.status == OK_SPECIAL && \
			authInfo.userAuthInfo == USERAUTH_NOOP )
			{
			/* It was a dummy read, try again */
			authInfo.status = processUserAuth( sessionInfoPtr, 
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
		   the client one way or another.  In addition we set the 
		   allowFurtherAuth flag to FALSE to indicate to the client that 
		   they can't continue past this point */
		if( isFatalError || authAttempts >= 2 )
			( void ) sendResponseFailure( sessionInfoPtr, FALSE );
		else
			{
			int status;

			status = sendResponseFailure( sessionInfoPtr, TRUE );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	ENSURES( authInfo.status != OK_SPECIAL );

	/* The user still hasn't successfully authenticated after multiple 
	   attempts, we're done */
	return( authInfo.status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int processServerAuth( INOUT SESSION_INFO *sessionInfoPtr,
					   const BOOLEAN userInfoPresent )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	USERAUTH_TYPE userAuthInfo;
	int status = DUMMY_INIT;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* If the caller has specified user credentials to match against we 
	   perform a basic pass/fail check against the client-supplied 
	   information */
	if( userInfoPresent )
		return( processFixedAuth( sessionInfoPtr ) );

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
		status = sendResponseFailure( sessionInfoPtr, TRUE );
		if( cryptStatusError( status ) )
			return( status );
		sessionInfoPtr->authResponse = AUTHRESPONSE_NONE;
		}

	/* Process the user authentication and, if it's a dummy read, try a 
	   second time.  The first time that we're called (authRead == FALSE) we 
	   accept and record the user name supplied by the client, on any 
	   subsequent calls (authRead == TRUE) we require a match for the 
	   previously-supplied user name.

	   Note that we don't use the complex failsafe-check method used by
	   processFixedAuth() because we're not performing the authorisation
	   check here but simply passing the data back to the caller */
	if( !sshInfo->authRead )
		{
		status = processUserAuth( sessionInfoPtr, &userAuthInfo, 
								  CREDENTIAL_NONE, TRUE );
		if( status == OK_SPECIAL && userAuthInfo == USERAUTH_NOOP )
			{
			/* It was a dummy read, try again */
			status = processUserAuth( sessionInfoPtr, &userAuthInfo, 
									  CREDENTIAL_USERNAME, FALSE );
			}
		}
	else
		{
		status = processUserAuth( sessionInfoPtr, &userAuthInfo, 
								  CREDENTIAL_USERNAME, FALSE );
		ENSURES( !( status == OK_SPECIAL && \
					userAuthInfo == USERAUTH_NOOP ) );
		}
	sshInfo->authRead = TRUE;
	ENSURES( cryptStatusError( status ) || status == OK_SPECIAL );

	return( ( status == OK_SPECIAL ) ? CRYPT_ENVELOPE_RESOURCE : status );
	}
#endif /* USE_SSH */
